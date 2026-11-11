---
name: gba-butano
description: Build, change and test the Game Boy Advance demo in consoles/gba (Butano 21.10 on devkitARM, built in Docker, tested in mGBA with a Lua script). Use when editing consoles/gba, when a GBA build fails, when the ROM shows a Butano error screen, or when adding a screen, text or drawing to the GBA demo.
---

# GBA demo with Butano

Everything here was learned building `consoles/gba`. Check the Butano headers before
trusting memory: the API changes between versions. Inside the Docker image they are in
`/opt/butano/butano/include`, and the examples in `/opt/butano/examples`.

## Build and test loop

```sh
cd consoles/gba
bash build.sh                                        # Docker build, writes clerk-anywhere-gba.gba
node --experimental-strip-types test/e2e.mjs         # drives mGBA, prints PASS or FAIL
```

After a test run, read the PNG files in `test/out`. A Butano assert shows as a red
"ERROR in bn_....cpp" screen in the screenshot, with the reason in plain text.

`build.sh` writes `include/ca_config.h` from `CA_SECRET`, `CA_BASE_URL` and
`CA_DISPLAY_URL`. That file holds the secret and is ignored by git.

## Makefile facts

- Butano resolves `SOURCES`, `INCLUDES` and `GRAPHICS` relative to the project. An
  absolute path fails silently (header not found). The project is mounted at `/work`,
  so Butano is reached as `../opt/butano`.
- `DEFAULTLIBS` is empty in the Butano template, which leaves out libc. Any `strlen`,
  `strchr` or third-party C code then fails at link time with "undefined reference".
  Set `DEFAULTLIBS := true`.
- The fonts come from Butano's `common` folder (`common/src`, `common/include`,
  `common/graphics`).

## Drawing

- The demo uses one `bn::sp_direct_bitmap_bg_ptr` (mode 3, 240x160, 15-bit colour) and
  draws with `bn::sp_direct_bitmap_bg_painter`: `fill`, `plot`, `rectangle(x1, y1, x2, y2, color)`.
  Painter coordinates start at the top-left corner.
- Text is sprites, made with `bn::sprite_text_generator`. Sprite coordinates start at
  the centre of the screen, so subtract 120 and 80. The `app::text` helpers do this.
- `bn::color` components go from 0 to 31.

## Sprite VRAM runs out fast

In bitmap modes there is half the usual sprite tile memory. Two rules follow:

1. Do not regenerate all the text when one thing changes. Draw static text once, keep
   changing text in its own `bn::vector` (`field_sprites`), and move highlights by
   repainting rectangles on the bitmap.
2. Sprite tiles are released on the next `bn::core::update()`, not when the
   `sprite_ptr` is destroyed. Clear the old text, call `bn::core::update()`, then
   generate the new text. `app::clear` does this.

Breaking either rule gives "Sprite tiles allocate failed. There's no more available VRAM".

## Save data

`bn::sram::read(value)` and `bn::sram::write(value)` copy a trivially copyable struct
at offset 0. The test reads SRAM from Lua at `0x0E000000 + offset`, so changing the
layout of `save_data` means updating the offsets at the top of `test/e2e.lua`.

## Testing with mGBA

- `--script file.lua` exists in mGBA 0.11 development builds, not in 0.10.x.
- The script cannot receive arguments. `e2e.mjs` fills placeholders in `e2e.lua` and
  talks to it through files in `test/out`.
- Inside a script: `callbacks:add("frame", fn)`, `emu:addKey(C.GBA_KEY.A)`,
  `emu:clearKey(...)`, `emu:read8(address)`, `emu:screenshot(path)`. Lua `io` works.
- Hold a key for a few frames and release it for a few more, or the ROM misses presses.
- `test/e2e.lua` mirrors the keyboard cursor rules of `run_keyboard()`. Change both together.

## Protocol

The code check must match `server/src/lib/offline-code.ts` byte for byte. The format is
in `protocol/offline-code.md`. The end-to-end test computes the code with the server's
implementation, so a mismatch fails the test.
