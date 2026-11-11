// Clerk Anywhere - Game Boy Advance demo.
//
// The console has no network, so sign-in is a challenge-response:
//   1. You type your Clerk username.
//   2. The console shows a random challenge as a QR code.
//   3. The web page behind the QR code signs you in with Clerk and shows a code.
//   4. You type the code; the console checks it with HMAC-SHA256.
// See /protocol/offline-code.md for the exact format.

#include "bn_core.h"
#include "bn_sram.h"
#include "bn_color.h"
#include "bn_keypad.h"
#include "bn_string.h"
#include "bn_vector.h"
#include "bn_sprite_ptr.h"
#include "bn_string_view.h"
#include "bn_sprite_text_generator.h"
#include "bn_sp_direct_bitmap_bg_ptr.h"
#include "bn_sp_direct_bitmap_bg_painter.h"

#include "common_fixed_8x16_sprite_font.h"
#include "common_variable_8x16_sprite_font.h"

#include <string.h>

extern "C"
{
    #include "qrcodegen.h"
}

#include "ca_config.h"
#include "ca_crypto.h"

namespace
{
    constexpr int screen_width = 240;
    constexpr int screen_height = 160;
    constexpr int max_attempts = 5;

    constexpr bn::color color_bg(2, 2, 3);
    constexpr bn::color color_card(4, 4, 6);
    constexpr bn::color color_border(9, 9, 12);
    constexpr bn::color color_accent(13, 9, 31);    // Clerk purple, #6C47FF
    constexpr bn::color color_white(31, 31, 31);
    constexpr bn::color color_black(0, 0, 0);
    constexpr bn::color color_error(28, 8, 8);

    enum class session_state : uint8_t
    {
        signed_out = 0,
        pending = 1,    // A challenge was issued and is waiting for its code.
        signed_in = 2
    };

    // Stored in cartridge SRAM. Keep it trivially copyable.
    struct save_data
    {
        char tag[8];
        uint32_t boot_count;
        session_state state;
        uint8_t attempts;
        char username[ca::username_max_length + 1];
        char challenge[ca::challenge_length + 1];
    };

    constexpr char save_tag[8] = { 'C', 'L', 'R', 'K', 'A', 'N', 'Y', '1' };

    struct app
    {
        bn::sp_direct_bitmap_bg_ptr bg = bn::sp_direct_bitmap_bg_ptr::create();
        bn::sp_direct_bitmap_bg_painter painter{ bg };
        bn::sprite_text_generator variable_text{ common::variable_8x16_sprite_font };
        bn::sprite_text_generator fixed_text{ common::fixed_8x16_sprite_font };
        bn::vector<bn::sprite_ptr, 96> sprites;
        bn::vector<bn::sprite_ptr, 8> field_sprites;    // Text that changes while a screen is shown.
        save_data save;
        uint32_t frames = 0;
        uint32_t entropy = 0x9e3779b9;

        // Text positions are given in screen pixels from the top-left corner;
        // y is the top of the 16 pixel tall line.
        void text(int x, int y, const bn::string_view& value)
        {
            variable_text.set_left_alignment();
            variable_text.generate(x - (screen_width / 2), y + 8 - (screen_height / 2), value, sprites);
        }

        void text_centered(int center_x, int y, const bn::string_view& value)
        {
            variable_text.set_center_alignment();
            variable_text.generate(center_x - (screen_width / 2), y + 8 - (screen_height / 2), value, sprites);
        }

        void mono(int x, int y, const bn::string_view& value)
        {
            mono(x, y, value, sprites);
        }

        void mono(int x, int y, const bn::string_view& value, bn::ivector<bn::sprite_ptr>& output)
        {
            fixed_text.set_left_alignment();
            fixed_text.generate(x - (screen_width / 2), y + 8 - (screen_height / 2), value, output);
        }

        void rect(int x, int y, int width, int height, bn::color color)
        {
            painter.rectangle(x, y, x + width - 1, y + height - 1, color);
        }

        void frame(int x, int y, int width, int height, bn::color border, bn::color fill)
        {
            rect(x, y, width, height, border);
            rect(x + 1, y + 1, width - 2, height - 2, fill);
        }

        void clear()
        {
            // Sprite tiles are only released on the next frame. In bitmap modes there is
            // half the usual sprite VRAM, so let the old text go before drawing new text.
            sprites.clear();
            field_sprites.clear();
            bn::core::update();
            painter.fill(color_bg);
        }

        // Small Clerk-like mark: a purple disc with a dark core.
        void mark(int x, int y)
        {
            rect(x + 2, y, 6, 10, color_accent);
            rect(x, y + 2, 10, 6, color_accent);
            rect(x + 1, y + 1, 8, 8, color_accent);
            rect(x + 3, y + 3, 4, 4, color_card);
        }

        void header(const bn::string_view& title)
        {
            rect(0, 0, screen_width, 22, color_card);
            rect(0, 22, screen_width, 1, color_border);
            mark(8, 6);
            text(24, 3, title);
        }

        void update()
        {
            ++frames;

            // Button timing is the only entropy source the console has.
            if(bn::keypad::any_pressed())
            {
                entropy = (entropy ^ frames) * 0x01000193;
            }

            bn::core::update();
        }

        void write_save()
        {
            bn::sram::write(save);
        }

        void load_save()
        {
            bn::sram::read(save);

            if(memcmp(save.tag, save_tag, sizeof(save_tag)) != 0)
            {
                memset(&save, 0, sizeof(save));
                memcpy(save.tag, save_tag, sizeof(save_tag));
            }

            // Never trust unterminated strings coming from the cartridge.
            save.username[ca::username_max_length] = 0;
            save.challenge[ca::challenge_length] = 0;
            ++save.boot_count;
            write_save();
        }

        void new_challenge()
        {
            struct
            {
                uint32_t boot_count;
                uint32_t frames;
                uint32_t entropy;
                char username[ca::username_max_length + 1];
                char previous[ca::challenge_length + 1];
            } pool;

            memset(&pool, 0, sizeof(pool));
            pool.boot_count = save.boot_count;
            pool.frames = frames;
            pool.entropy = entropy;
            memcpy(pool.username, save.username, sizeof(pool.username));
            memcpy(pool.previous, save.challenge, sizeof(pool.previous));

            uint8_t digest[ca::sha256_size];
            ca::sha256(reinterpret_cast<const uint8_t*>(&pool), int(sizeof(pool)), digest);

            for(int index = 0; index < ca::challenge_length; ++index)
            {
                save.challenge[index] = ca::alphabet[digest[index] & 31];
            }

            save.challenge[ca::challenge_length] = 0;
            save.attempts = 0;
            save.state = session_state::pending;
            entropy ^= uint32_t(digest[8]) | (uint32_t(digest[9]) << 8) | (uint32_t(digest[10]) << 16);
            write_save();
        }
    };

    // ---------------------------------------------------------------------------------------------
    // On-screen keyboard
    // ---------------------------------------------------------------------------------------------

    struct keyboard_layout
    {
        const char* rows[4];
        int rows_count;
    };

    constexpr keyboard_layout username_layout = { { "1234567890", "qwertyuiop", "asdfghjkl-", "zxcvbnm_" }, 4 };
    constexpr keyboard_layout code_layout = { { "0123456789", "ABCDEFGHJK", "MNPQRSTVWX", "YZ" }, 4 };

    constexpr int key_width = 20;
    constexpr int key_height = 18;
    constexpr int keys_x = 20;
    constexpr int keys_y = 62;

    enum class keyboard_result
    {
        accepted,
        cancelled
    };

    struct keyboard_options
    {
        const keyboard_layout& layout;
        bn::string_view title;
        bn::string_view hint;
        bn::string_view message;    // Shown in red under the field, may be empty.
        int max_length;
        int min_length;
        int group_size;             // Draws a gap every group_size characters, 0 for none.
        bool submit_when_full;
    };

    void draw_key_cursor(app& a, int row, int column, bn::color color)
    {
        a.rect(keys_x + (column * key_width), keys_y + (row * key_height), key_width - 2, key_height - 2, color);
    }

    void draw_field(app& a, const keyboard_options& options, const char* value, int length)
    {
        constexpr int field_x = 20;
        constexpr int field_y = 28;
        constexpr int field_width = 200;

        a.frame(field_x, field_y, field_width, 20, color_accent, color_card);

        bn::string<40> shown;

        for(int index = 0; index < length; ++index)
        {
            if(options.group_size && index && index % options.group_size == 0)
            {
                shown.push_back(' ');
            }

            shown.push_back(value[index]);
        }

        if(length < options.max_length)
        {
            shown.push_back('_');
        }

        a.field_sprites.clear();
        a.mono(field_x + 8, field_y + 2, shown, a.field_sprites);
    }

    keyboard_result run_keyboard(app& a, const keyboard_options& options, char* value)
    {
        const keyboard_layout& layout = options.layout;
        int length = int(strlen(value));
        int row = 1;
        int column = 0;

        // Everything but the field and the cursor is drawn once: regenerating
        // all the text on every key press would exhaust the sprite VRAM.
        a.clear();
        a.header(options.title);

        if(! options.message.empty())
        {
            a.rect(20, 49, 200, 12, color_error);
            a.text_centered(screen_width / 2, 47, options.message);
        }

        for(int r = 0; r < layout.rows_count; ++r)
        {
            const char* keys = layout.rows[r];

            for(int c = 0; keys[c]; ++c)
            {
                char key[2] = { keys[c], 0 };
                a.mono(keys_x + (c * key_width) + 5, keys_y + (r * key_height), key);
            }
        }

        a.text_centered(screen_width / 2, 142, options.hint);
        draw_field(a, options, value, length);
        draw_key_cursor(a, row, column, color_accent);

        while(true)
        {
            a.update();

            int old_row = row;
            int old_column = column;
            int row_length = int(strlen(layout.rows[row]));

            if(bn::keypad::left_pressed())
            {
                column = (column + row_length - 1) % row_length;
            }
            else if(bn::keypad::right_pressed())
            {
                column = (column + 1) % row_length;
            }
            else if(bn::keypad::up_pressed() || bn::keypad::down_pressed())
            {
                int step = bn::keypad::up_pressed() ? layout.rows_count - 1 : 1;
                row = (row + step) % layout.rows_count;
                int new_length = int(strlen(layout.rows[row]));

                if(column >= new_length)
                {
                    column = new_length - 1;
                }
            }
            else if(bn::keypad::a_pressed())
            {
                if(length < options.max_length)
                {
                    value[length++] = layout.rows[row][column];
                    value[length] = 0;
                    draw_field(a, options, value, length);

                    if(options.submit_when_full && length == options.max_length)
                    {
                        return keyboard_result::accepted;
                    }
                }
            }
            else if(bn::keypad::b_pressed())
            {
                if(length)
                {
                    value[--length] = 0;
                    draw_field(a, options, value, length);
                }
                else
                {
                    return keyboard_result::cancelled;
                }
            }
            else if(bn::keypad::start_pressed())
            {
                if(length >= options.min_length)
                {
                    return keyboard_result::accepted;
                }
            }

            if(row != old_row || column != old_column)
            {
                draw_key_cursor(a, old_row, old_column, color_bg);
                draw_key_cursor(a, row, column, color_accent);
            }
        }
    }

    // ---------------------------------------------------------------------------------------------
    // Screens
    // ---------------------------------------------------------------------------------------------

    void sign_in_screen(app& a)
    {
        keyboard_options options = {
            username_layout, "Sign in to Clerk Anywhere", "A: type   B: delete   START: continue", "",
            ca::username_max_length, ca::username_min_length, 0, false
        };

        while(run_keyboard(a, options, a.save.username) != keyboard_result::accepted)
        {
        }

        a.new_challenge();
    }

    void draw_qr(app& a, const char* url, int x, int y, int size)
    {
        uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(6)];
        uint8_t temp[qrcodegen_BUFFER_LEN_FOR_VERSION(6)];

        a.rect(x, y, size, size, color_white);

        if(! qrcodegen_encodeText(url, temp, qr, qrcodegen_Ecc_LOW, 1, 6, qrcodegen_Mask_AUTO, true))
        {
            return;
        }

        int modules = qrcodegen_getSize(qr);
        int scale = size / (modules + 4);

        if(scale < 1)
        {
            return;
        }

        int offset = (size - (modules * scale)) / 2;

        for(int my = 0; my < modules; ++my)
        {
            for(int mx = 0; mx < modules; ++mx)
            {
                if(qrcodegen_getModule(qr, mx, my))
                {
                    a.rect(x + offset + (mx * scale), y + offset + (my * scale), scale, scale, color_black);
                }
            }
        }
    }

    // Returns false when the user goes back to change the username.
    bool challenge_screen(app& a)
    {
        bn::string<64> url(CA_BASE_URL);
        url.append(a.save.challenge);

        bn::string<32> title("Sign in as ");
        title.append(a.save.username);

        bn::string<16> challenge;
        challenge.append(bn::string_view(a.save.challenge, 4));
        challenge.push_back(' ');
        challenge.append(bn::string_view(a.save.challenge + 4, 4));

        bn::string<48> manual(CA_DISPLAY_URL);
        manual.push_back('/');
        manual.append(a.save.challenge);

        a.clear();
        a.header(title);
        draw_qr(a, url.c_str(), 4, 27, 116);

        constexpr int column_x = 128;
        a.text(column_x, 26, "Scan with your");
        a.text(column_x, 40, "phone and sign in");
        a.text(column_x, 62, "Challenge");
        a.mono(column_x, 78, challenge);
        a.frame(column_x, 104, 104, 20, color_accent, color_accent);
        a.text_centered(column_x + 52, 106, "A: enter code");
        a.text(column_x, 126, "B: back");
        a.text_centered(screen_width / 2, 144, manual);

        while(true)
        {
            a.update();

            if(bn::keypad::a_pressed() || bn::keypad::start_pressed())
            {
                return true;
            }

            if(bn::keypad::b_pressed())
            {
                a.save.state = session_state::signed_out;
                a.write_save();
                return false;
            }
        }
    }

    // Returns true once the session is verified. Returns false to show the
    // challenge again, or with the session reset after too many failures.
    bool code_screen(app& a)
    {
        char code[ca::code_length + 1] = {};
        bn::string<40> message;

        while(true)
        {
            keyboard_options options = {
                code_layout, "Enter the code", "A: type   B: delete / back", message,
                ca::code_length, ca::code_length, 5, true
            };

            if(run_keyboard(a, options, code) == keyboard_result::cancelled)
            {
                return false;
            }

            char expected[ca::code_length + 1];
            ca::response_code(CA_SECRET, "gba", a.save.challenge, a.save.username, expected);

            // Compare every character so timing does not depend on the match position.
            int difference = 0;

            for(int index = 0; index < ca::code_length; ++index)
            {
                difference |= code[index] ^ expected[index];
            }

            if(difference == 0)
            {
                a.save.state = session_state::signed_in;
                a.save.attempts = 0;
                a.write_save();
                return true;
            }

            ++a.save.attempts;

            if(a.save.attempts >= max_attempts)
            {
                // The challenge is burned: a fresh one is required.
                a.save.state = session_state::signed_out;
                a.save.challenge[0] = 0;
                a.write_save();
                return false;
            }

            a.write_save();
            memset(code, 0, sizeof(code));
            message = "Wrong code. Tries left: ";
            message.push_back(char('0' + (max_attempts - a.save.attempts)));
        }
    }

    void draw_identicon(app& a, const char* username, int x, int y, int cell)
    {
        constexpr bn::color colors[] = {
            bn::color(13, 9, 31), bn::color(31, 12, 16), bn::color(6, 24, 20),
            bn::color(31, 21, 6), bn::color(8, 18, 31), bn::color(24, 10, 28)
        };

        uint8_t digest[ca::sha256_size];
        ca::sha256(reinterpret_cast<const uint8_t*>(username), int(strlen(username)), digest);

        bn::color color = colors[digest[31] % 6];
        a.rect(x, y, cell * 5, cell * 5, bn::color(27, 27, 29));

        // 5x5 grid, mirrored horizontally: 15 bits decide the pattern.
        for(int row = 0; row < 5; ++row)
        {
            for(int column = 0; column < 3; ++column)
            {
                int bit = (row * 3) + column;

                if((digest[bit >> 3] >> (bit & 7)) & 1)
                {
                    a.rect(x + (column * cell), y + (row * cell), cell, cell, color);
                    a.rect(x + ((4 - column) * cell), y + (row * cell), cell, cell, color);
                }
            }
        }
    }

    void profile_screen(app& a)
    {
        a.clear();
        a.header("Clerk Anywhere");

        a.frame(12, 34, 216, 84, color_border, color_card);
        draw_identicon(a, a.save.username, 22, 44, 12);

        constexpr int column_x = 94;
        a.text(column_x, 44, a.save.username);
        a.text(column_x, 64, "Signed in with Clerk");
        a.text(column_x, 82, "Verified offline");

        a.frame(70, 128, 100, 22, color_accent, color_accent);
        a.text_centered(screen_width / 2, 131, "A: Sign out");

        while(true)
        {
            a.update();

            if(bn::keypad::a_pressed())
            {
                // Signing out forgets the challenge too, so an old code is useless.
                a.save.state = session_state::signed_out;
                a.save.challenge[0] = 0;
                a.save.username[0] = 0;
                a.save.attempts = 0;
                a.write_save();
                return;
            }
        }
    }
}

int main()
{
    bn::core::init();

    app a;
    a.load_save();

    while(true)
    {
        if(a.save.state == session_state::signed_in)
        {
            profile_screen(a);
        }
        else if(a.save.state == session_state::pending && a.save.challenge[0] && a.save.username[0])
        {
            if(challenge_screen(a) && code_screen(a))
            {
                continue;
            }
        }
        else
        {
            sign_in_screen(a);
        }
    }
}
