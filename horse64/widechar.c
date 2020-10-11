// Copyright (c) 2020, ellie/@ell1e & Horse64 Team (see AUTHORS.md),
// also see LICENSE.md file.
// SPDX-License-Identifier: BSD-2-Clause

#include <assert.h>
#if defined(_WIN32) || defined(_WIN64)
#include <malloc.h>
#else
#include <alloca.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "widechar.h"

static int is_utf8_start(uint8_t c) {
    if ((int)(c & 0xE0) == (int)0xC0) {  // 110xxxxx
        return 1;
    } else if ((int)(c & 0xF0) == (int)0xE0) {  // 1110xxxx
        return 1;
    } else if ((int)(c & 0xF8) == (int)0xF0) {  // 11110xxx
        return 1;
    }
    return 0;
}

int utf8_char_len(const unsigned char *p) {
    if ((int)((*p) & 0xE0) == (int)0xC0)
        return 2;
    if ((int)((*p) & 0xF0) == (int)0xE0)
        return 3;
    if ((int)((*p) & 0xF8) == (int)0xF0)
        return 4;
    return 1;
}

int write_codepoint_as_utf8(
        uint64_t codepoint, int surrogateunescape,
        char *out, int outbuflen, int *outlen
        ) {
    if (surrogateunescape &&
            codepoint >= 0xDC80ULL + 0 &&
            codepoint <= 0xDC80ULL + 255) {
        if (outbuflen < 1) return 0;
        out[0] = (int)(codepoint - 0xDC80ULL);
        if (outbuflen >= 2)
            out[1] = '\0';
        if (outlen) *outlen = 1;
        return 1;
    }
    if (codepoint < 0x80ULL) {
        if (outbuflen < 1) return 0;
        out[0] = (int)codepoint;
        if (outbuflen >= 2)
            out[1] = '\0';
        if (outlen) *outlen = 1;
        return 1;
    } else if (codepoint < 0x800ULL) {
        uint64_t byte2val = (codepoint & 0x3FULL);
        uint64_t byte1val = (codepoint & 0x7C0ULL) >> 6;
        if (outbuflen < 2) return 0;
        out[0] = (int)(byte1val | 0xC0);
        out[1] = (int)(byte2val | 0x80);
        if (outbuflen >= 3)
            out[2] = '\0';
        if (outlen) *outlen = 2;
        return 1;
    } else if (codepoint < 0x10000ULL) {
        uint64_t byte3val = (codepoint & 0x3FULL);
        uint64_t byte2val = (codepoint & 0xFC0ULL) >> 6;
        uint64_t byte1val = (codepoint & 0xF000ULL) >> 12;
        if (outbuflen < 3) return 0;
        out[0] = (int)(byte1val | 0xC0);
        out[1] = (int)(byte2val | 0x80);
        out[2] = (int)(byte3val | 0x80);
        if (outbuflen >= 4)
            out[3] = '\0';
        if (outlen) *outlen = 3;
        return 1;
    } else if (codepoint < 0x200000ULL) {
        uint64_t byte4val = (codepoint & 0x3FULL);
        uint64_t byte3val = (codepoint & 0xFC0ULL) >> 6;
        uint64_t byte2val = (codepoint & 0x3F000ULL) >> 12;
        uint64_t byte1val = (codepoint & 0x1C0000ULL) >> 18;
        if (outbuflen < 4) return 0;
        out[0] = (int)(byte1val | 0xC0);
        out[1] = (int)(byte2val | 0x80);
        out[2] = (int)(byte3val | 0x80);
        out[3] = (int)(byte4val | 0x80);
        if (outbuflen >= 5)
            out[4] = '\0';
        if (outlen) *outlen = 4;
        return 1;
    } else {
        return 0;
    }
}

int get_utf8_codepoint(
        const unsigned char *p, int size,
        h64wchar *out, int *outlen
        ) {
    if (size < 1)
        return 0;
    if (!is_utf8_start(*p)) {
        if (*p > 127)
            return 0;
        if (out) *out = (h64wchar)(*p);
        if (outlen) *outlen = 1;
        return 1;
    }
    uint8_t c = (*(uint8_t*)p);
    if ((int)(c & 0xE0) == (int)0xC0 && size >= 2) {  // p[0] == 110xxxxx
        if ((int)(*(p + 1) & 0xC0) != (int)0x80) { // p[1] != 10xxxxxx
            return 0;
        }
        if (size >= 3 &&
                (int)(*(p + 2) & 0xC0) == (int)0x80) { // p[2] == 10xxxxxx
            return 0;
        }
        h64wchar c = (   // 00011111 of first byte
            (h64wchar)(*p) & (h64wchar)0x1FULL
        ) << (h64wchar)6ULL;
        c += (  // 00111111 of second byte
            (h64wchar)(*(p + 1)) & (h64wchar)0x3FULL
        );
        if (c <= 127ULL)
            return 0;  // not valid to be encoded with two bytes.
        if (out) *out = c;
        if (outlen) *outlen = 2;
        return 1;
    }
    if ((int)(c & 0xF0) == (int)0xE0 && size >= 3) {  // p[0] == 1110xxxx
        if ((int)(*(p + 1) & 0xC0) != (int)0x80) { // p[1] != 10xxxxxx
            return 0;
        }
        if ((int)(*(p + 2) & 0xC0) != (int)0x80) { // p[2] != 10xxxxxx
            return 0;
        }
        if (size >= 4 &&
                (int)(*(p + 3) & 0xC0) == (int)0x80) { // p[3] == 10xxxxxx
            return 0;
        }
        h64wchar c = (   // 00011111 of first byte
            (h64wchar)(*p) & (h64wchar)0x1FULL
        ) << (h64wchar)12ULL;
        c += (  // 00111111 of second byte
            (h64wchar)(*(p + 1)) & (h64wchar)0x3FULL
        ) << (h64wchar)6ULL;
        c += (  // 00111111 of third byte
            (h64wchar)(*(p + 2)) & (h64wchar)0x3FULL
        );
        if (c <= 0x7FFULL)
            return 0;  // not valid to be encoded with three bytes.
        if (c >= 0xD800ULL && c <= 0xDFFFULL) {
            // UTF-16 surrogate code points may not be used in UTF-8
            // (in part because we re-use them to store invalid bytes)
            return 0;
        }
        if (out) *out = c;
        if (outlen) *outlen = 3;
        return 1;
    }
    if ((int)(c & 0xF8) == (int)0xF0 && size >= 4) {  // p[0] == 11110xxx
        if ((int)(*(p + 1) & 0xC0) != (int)0x80) { // p[1] != 10xxxxxx
            return 0;
        }
        if ((int)(*(p + 2) & 0xC0) != (int)0x80) { // p[2] != 10xxxxxx
            return 0;
        }
        if ((int)(*(p + 3) & 0xC0) != (int)0x80) { // p[3] != 10xxxxxx
            return 0;
        }
        if (size >= 5 &&
                (int)(*(p + 4) & 0xC0) == (int)0x80) { // p[4] == 10xxxxxx
            return 0;
        }
        h64wchar c = (   // 00011111 of first byte
            (h64wchar)(*p) & (h64wchar)0x1FULL
        ) << (h64wchar)18ULL;
        c += (  // 00111111 of second byte
            (h64wchar)(*(p + 1)) & (h64wchar)0x3FULL
        ) << (h64wchar)12ULL;
        c += (  // 00111111 of third byte
            (h64wchar)(*(p + 2)) & (h64wchar)0x3FULL
        ) << (h64wchar)6ULL;
        c += (  // 00111111 of fourth byte
            (h64wchar)(*(p + 3)) & (h64wchar)0x3FULL
        );
        if (c <= 0xFFFFULL)
            return 0;  // not valid to be encoded with four bytes.
        if (out) *out = c;
        if (outlen) *outlen = 4;
        return 1;
    }
    return 0;
}

int is_valid_utf8_char(
        const unsigned char *p, int size
        ) {
    if (!get_utf8_codepoint(p, size, NULL, NULL))
        return 0;
    return 1;
}

h64wchar *utf8_to_utf32(
        const char *input,
        int64_t input_len,
        void *(*out_alloc)(uint64_t len, void *userdata),
        void *out_alloc_ud,
        int64_t *out_len
        ) {
    return utf8_to_utf32_ex(
        input, input_len, NULL, 0, out_alloc, out_alloc_ud,
        out_len, 1, 0, NULL, NULL
    );
}

h64wchar *utf8_to_utf32_ex(
        const char *input,
        int64_t input_len,
        char *short_out_buf, int short_out_buf_bytes,
        void *(*out_alloc)(uint64_t len, void *userdata),
        void *out_alloc_ud,
        int64_t *out_len,
        int surrogatereplaceinvalid,
        int questionmarkinvalid,
        int *was_aborted_invalid,
        int *was_aborted_outofmemory
        ) {
    int free_temp_buf = 0;
    char *temp_buf = NULL;
    int64_t temp_buf_len = (input_len + 1) * sizeof(h64wchar);
    if (temp_buf_len < 1024 * 2) {
        temp_buf = alloca(temp_buf_len);
    } else {
        free_temp_buf = 1;
        temp_buf = malloc(temp_buf_len);
        if (!temp_buf) {
            if (was_aborted_invalid)
                *was_aborted_invalid = 0;
            if (was_aborted_outofmemory)
                *was_aborted_outofmemory = 1;
            return NULL;
        }
    }
    int k = 0;
    int i = 0;
    while (i < input_len) {
        h64wchar c;
        int cbytes = 0;
        if (!get_utf8_codepoint(
                (const unsigned char*)(input + i),
                input_len - i, &c, &cbytes)) {
            if (!surrogatereplaceinvalid && !questionmarkinvalid) {
                if (free_temp_buf)
                    free(temp_buf);
                if (was_aborted_invalid)
                    *was_aborted_invalid = 1;
                if (was_aborted_outofmemory)
                    *was_aborted_outofmemory = 0;
                return NULL;
            }
            h64wchar invalidbyte = 0xFFFDULL;
            if (!questionmarkinvalid) {
                invalidbyte = 0xDC80ULL + (
                    (h64wchar)(*(const unsigned char*)(input + i))
                );
            }
            memcpy((char*)temp_buf + k * sizeof(invalidbyte),
                   &invalidbyte, sizeof(invalidbyte));
            k++;
            i++;
            continue;
        }
        i += cbytes;
        memcpy((char*)temp_buf + k * sizeof(c), &c, sizeof(c));
        k++;
    }
    temp_buf[k * sizeof(h64wchar)] = '\0';
    char *result = NULL;
    if (short_out_buf && (unsigned int)short_out_buf_bytes >=
            (k + 1) * sizeof(h64wchar)) {
        result = short_out_buf;
    } else {
        if (out_alloc) {
            result = out_alloc(
                (k + 1) * sizeof(h64wchar), out_alloc_ud
            );
        } else {
            result = malloc((k + 1) * sizeof(h64wchar));
        }
    }
    assert((k + 1) * sizeof(h64wchar) <=
           (input_len + 1) * sizeof(h64wchar));
    if (result) {
        memcpy(result, temp_buf, (k + 1) * sizeof(h64wchar));
    }
    if (free_temp_buf)
        free(temp_buf);
    if (!result) {
        if (was_aborted_invalid) *was_aborted_invalid = 0;
        if (was_aborted_outofmemory) *was_aborted_outofmemory = 1;
        return NULL;
    }
    if (was_aborted_invalid) *was_aborted_invalid = 0;
    if (was_aborted_outofmemory) *was_aborted_outofmemory = 0;
    if (out_len) *out_len = k;
    return (h64wchar*)result;
}

int utf32_to_utf8(
        const h64wchar *input, int64_t input_len,
        char *outbuf, int64_t outbuflen,
        int64_t *out_len, int surrogateunescape
        ) {
    const h64wchar *p = input;
    uint64_t totallen = 0;
    int64_t i = 0;
    while (i < input_len) {
        if (outbuflen < 6)
            return 0;
        int inneroutlen = 0;
        if (!write_codepoint_as_utf8(
                (uint64_t)*p, surrogateunescape,
                outbuf, outbuflen, &inneroutlen
                )) {
            return 0;
        }
        assert(inneroutlen > 0);
        p++;
        outbuflen -= inneroutlen;
        outbuf += inneroutlen;
        totallen += inneroutlen;
        i++;
    }
    if (out_len) *out_len = (int64_t)totallen;
    return 1;
}
