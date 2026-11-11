#ifndef CA_CRYPTO_H
#define CA_CRYPTO_H

#include <stdint.h>

namespace ca
{
    constexpr int sha256_size = 32;
    constexpr int challenge_length = 8;
    constexpr int code_length = 10;
    constexpr int username_max_length = 16;
    constexpr int username_min_length = 4;

    // Crockford base32, same alphabet as the server.
    constexpr const char* alphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

    void sha256(const uint8_t* data, int size, uint8_t out[sha256_size]);

    void hmac_sha256(const uint8_t* key, int key_size, const uint8_t* data, int data_size,
                     uint8_t out[sha256_size]);

    // Writes code_length characters plus a terminator.
    // Must match responseCode() in server/src/lib/offline-code.ts.
    void response_code(const char* secret, const char* console_id, const char* challenge,
                       const char* username, char out[code_length + 1]);
}

#endif
