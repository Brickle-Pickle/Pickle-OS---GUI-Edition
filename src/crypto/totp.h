#pragma once
#include <Arduino.h>
#include <time.h>
#include "mbedtls/md.h"

// RFC 4648 Base32 decoder and RFC 4226/6238 HOTP/TOTP generator.
// Header-only so any screen can include it without extra build wiring.
namespace totp {

// Decode a Base32 string into bytes. Accepts upper/lower case, ignores spaces,
// dashes and padding. Returns the number of bytes written, or 0 on invalid input.
inline size_t base32Decode(const char* input, uint8_t* out, size_t outMax) {
    if (!input || !out) return 0;
    uint32_t buffer = 0;
    int bits = 0;
    size_t produced = 0;
    for (const char* p = input; *p; ++p) {
        char c = *p;
        if (c == ' ' || c == '-' || c == '\t' || c == '=' || c == '\n' || c == '\r') continue;
        int v;
        if (c >= 'A' && c <= 'Z')      v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a';
        else if (c >= '2' && c <= '7') v = 26 + (c - '2');
        else return 0;
        buffer = (buffer << 5) | (uint32_t)v;
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            if (produced >= outMax) return 0;
            out[produced++] = (uint8_t)((buffer >> bits) & 0xFF);
        }
    }
    return produced;
}

// HOTP per RFC 4226. Returns code in [0, 10^digits), or -1 on failure.
inline int32_t hotp(const uint8_t* key, size_t keyLen, uint64_t counter, int digits) {
    uint8_t msg[8];
    for (int i = 7; i >= 0; --i) {
        msg[i] = (uint8_t)(counter & 0xFFULL);
        counter >>= 8;
    }
    uint8_t digest[20];
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    if (!info) return -1;
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    if (mbedtls_md_setup(&ctx, info, 1) != 0) {
        mbedtls_md_free(&ctx);
        return -1;
    }
    mbedtls_md_hmac_starts(&ctx, key, keyLen);
    mbedtls_md_hmac_update(&ctx, msg, sizeof(msg));
    mbedtls_md_hmac_finish(&ctx, digest);
    mbedtls_md_free(&ctx);

    int offset = digest[19] & 0x0F;
    uint32_t bin = ((uint32_t)(digest[offset] & 0x7F) << 24)
                 | ((uint32_t)digest[offset + 1] << 16)
                 | ((uint32_t)digest[offset + 2] << 8)
                 | (uint32_t)digest[offset + 3];
    int32_t mod = 1;
    for (int i = 0; i < digits; ++i) mod *= 10;
    return (int32_t)(bin % (uint32_t)mod);
}

// TOTP per RFC 6238 with a 30-second step and 6 digits.
inline int32_t totpAt(const uint8_t* key, size_t keyLen, time_t unixTime) {
    if (unixTime < 0) return -1;
    uint64_t counter = (uint64_t)unixTime / 30ULL;
    return hotp(key, keyLen, counter, 6);
}

}
