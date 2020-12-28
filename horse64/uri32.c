// Copyright (c) 2020, ellie/@ell1e & Horse64 Team (see AUTHORS.md),
// also see LICENSE.md file.
// SPDX-License-Identifier: BSD-2-Clause

#include "compileconfig.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <winuser.h>
#endif

#include "filesys.h"
#include "filesys32.h"
#include "nonlocale.h"
#include "uri32.h"
#include "widechar.h"


static int uri32_ExtractSingleURIEscape(
        const h64wchar *s, int64_t slen
        ) {
    if (slen < 3)
        return -1;
    if (s[0] == (int)'%' &&
            ((s[1] >= (int)'0' &&
              s[1] <= (int)'9') ||
             (s[1] >= (int)'a' &&
              s[1] <= (int)'a') ||
             (s[1] >= (int)'A' &&
              s[1] <= (int)'Z')) &&
            ((s[2] >= (int)'0' &&
              s[2] <= (int)'9') ||
             (s[2] >= (int)'a' &&
              s[2] <= (int)'z') ||
             (s[2] >= (int)'A' &&
              s[2] <= (int)'Z'))
            ) {
        char hexvalue[3];
        hexvalue[0] = s[1];
        hexvalue[1] = s[2];
        hexvalue[2] = '\0';
        int number = (int)h64strtoll(hexvalue, NULL, 16);
        if (number > 255 || number == 0)
            number = '?';
        return number;
    }
    return -1;
}

static int64_t uri32_ExtractUtf8URIEscape(
        const h64wchar *s, int64_t slen, int *consumedcodepoints
        ) {
    int64_t slen_orig = slen;

    // See if this starts out with a possibly UTF-8 related escape:
    int value = uri32_ExtractSingleURIEscape(s, slen);
    if (value < 0) {
        if (consumedcodepoints) *consumedcodepoints = 0;
        return -1;
    }
    uint8_t c = value;
    int utf8len = utf8_char_len(&c);
    if (utf8len < 2) {
        if (consumedcodepoints) *consumedcodepoints = 0;
        return -1;
    }

    // Extract remaining Utf-8 bytes escaped after this one:
    int utf8bytescount = 1;
    uint8_t utf8bytes[10];
    utf8bytes[0] = value;
    assert(utf8len < (int)sizeof(utf8bytes));
    s += 3;
    slen -= 3;
    utf8len--;
    while (slen >= 3 && utf8len > 0) {
        int value = uri32_ExtractSingleURIEscape(s, slen);
        if (value < 0) {
            if (consumedcodepoints) *consumedcodepoints = 0;
            return -1;
        }
        utf8bytes[utf8bytescount] = value;
        utf8bytescount++;
        s += 3;
        slen -= 3;
        utf8len--;
    }
    utf8bytes[utf8bytescount] = '\0';

    int parsedlen = 0;
    h64wchar result = 0;
    if (!get_utf8_codepoint(
            utf8bytes, utf8bytescount,
            &result, &parsedlen
            )) {
        if (consumedcodepoints) *consumedcodepoints = 0;
        return -1;
    }
    if (consumedcodepoints) *consumedcodepoints = slen_orig - slen;
    return (int64_t)result;
}

static h64wchar *uri32_ParsePath(
        const h64wchar *escaped_path, int64_t escaped_path_len,
        int enforceleadingslash, int64_t *out_len
        ) {
    h64wchar *unescaped_path = malloc(
        escaped_path_len * sizeof(*escaped_path)
    );
    if (!unescaped_path) {
        if (out_len) *out_len = 0;
        return NULL;
    }
    memcpy(
        unescaped_path, escaped_path,
        escaped_path_len * sizeof(*escaped_path)
    );
    int unescaped_path_len = escaped_path_len;
    int i = 0;
    while (i < unescaped_path_len) {
        int escaped_value = -1;
        if (unescaped_path[i] == (int)'%' &&
                (escaped_value = uri32_ExtractSingleURIEscape(
                    unescaped_path + i, unescaped_path_len - i
                )) >= 0) {
            int skipcharslen = 3;
            if (escaped_value <= 127) {
                unescaped_path[i] = (unsigned int)escaped_value;
            } else {
                // Non-ASCII value.
                // See if it reads as UTF-8:
                int codepointlen = 0;
                int64_t utf8codepoint = uri32_ExtractUtf8URIEscape(
                    unescaped_path + i, unescaped_path_len - i,
                    &codepointlen
                );
                if (utf8codepoint >= 0) {
                    assert(codepointlen >= 6);
                    skipcharslen = codepointlen;
                    unescaped_path[i] = (uint64_t)utf8codepoint;
                } else {
                    // NOT Utf-8. Surrogate escape this:
                    unescaped_path[i] = (uint64_t)(
                        0xDC80ULL + escaped_value
                    );
                }
            }
            if (i + skipcharslen < unescaped_path_len)
                memmove(
                    &unescaped_path[i + 1],
                    &unescaped_path[i + skipcharslen],
                    (unescaped_path_len - (i + skipcharslen)) *
                        sizeof(*unescaped_path)
                );
            unescaped_path_len -= (skipcharslen - 1);
        }
        i++;
    }
    if ((unescaped_path_len == 0 || (
            unescaped_path[0] != '/' &&
            unescaped_path[0] != '\\')) &&
            enforceleadingslash) {
        h64wchar *unescaped_path_2 = malloc(
            (unescaped_path_len + 1) * sizeof(*unescaped_path)
        );
        if (!unescaped_path_2) {
            free(unescaped_path);
            if (out_len) *out_len = 0;
            return NULL;
        }
        unescaped_path_2[0] = (int)'/';
        memcpy(
            unescaped_path_2 + 1, unescaped_path,
            unescaped_path_len * sizeof(*unescaped_path)
        );
        unescaped_path_len++;
        free(unescaped_path);
        unescaped_path = unescaped_path_2;
    }
    if (out_len) *out_len = unescaped_path_len;
    return unescaped_path;
}

static int u32u8compare_simple(
        const h64wchar *s1, int64_t s1len,
        const char *s2, int casesensitive,
        int *wasoom
        ) {
    if (s1len == 0) {
        if (wasoom) *wasoom = 0;
        return (strlen(s2) == 0);
    }
    char short_out_buf[128];
    int innerwasinvalid = 0;
    int innerwasoom = 0;
    int64_t s2u32len = 0;
    h64wchar *s2u32 = utf8_to_utf32_ex(
        s2, strlen(s2), short_out_buf, sizeof(short_out_buf),
        NULL, NULL, &s2u32len, 1, 0,
        &innerwasinvalid, &innerwasoom
    );
    if (innerwasinvalid || innerwasoom) {
        if (wasoom) *wasoom = (innerwasoom != 0);
        return 0;
    }
    if (s2u32len != s1len) {
        if ((char *)s2u32 != short_out_buf) free(s2u32);
        if (wasoom) *wasoom = 0;
        return 0;
    }
    h64wchar *s1buf = NULL;
    if (!casesensitive) {
        s1buf = malloc(
            sizeof(*s1) * s1len
        );
        if (!s1buf) {
            if ((char *)s2u32 != short_out_buf) free(s2u32);
            if (wasoom) *wasoom = 0;
            return 0;
        }
        memcpy(s1buf, s1, sizeof(*s1) * s1len);
        utf32_tolower(s1buf, s1len);
        utf32_tolower(s2u32, s2u32len);
        s1 = s1buf;
    }
    if (memcmp(s2u32, s1, s1len * sizeof(*s1)) != 0) {
        if ((char *)s2u32 != short_out_buf) free(s2u32);
        if (wasoom) *wasoom = 0;
        if (s1buf) free(s1buf);
        return 0;
    }
    if ((char *)s2u32 != short_out_buf) free(s2u32);
    if (wasoom) *wasoom = 0;
    if (s1buf) free(s1buf);
    return 1;
}

int uri32_Compare(
        const h64wchar *uri1str, int64_t uri1len,
        const h64wchar *uri2str, int64_t uri2len,
        int converttoabsolutefilepaths,
        int assumecasesensitivefilepaths, int *result
        ) {
    h64wchar *uri1normalized = NULL;
    int64_t uri1normalizedlen = 0;
    h64wchar *uri2normalized = NULL;
    int64_t uri2normalizedlen = 0;
    uri32info *uri1 = NULL;
    uri32info *uri2 = NULL;
    uri1normalized = uri32_Normalize(
        uri1str, uri1len, converttoabsolutefilepaths,
        &uri1normalizedlen
    );
    if (!uri1normalized)
        goto oom;
    uri2normalized = uri32_Normalize(
        uri2str, uri2len, converttoabsolutefilepaths,
        &uri2normalizedlen
    );
    if (!uri2normalized)
        goto oom;
    if (uri1normalizedlen == uri2normalizedlen &&
            memcmp(
                uri1normalized, uri2normalized, uri1normalizedlen *
                sizeof(*uri1normalized)
            ) == 0) {
        match:
        uri32_Free(uri1);
        uri32_Free(uri2);
        free(uri1normalized);
        free(uri2normalized);
        *result = 1;
        return 1;
    }
    uri1 = uri32_Parse(uri1normalized, uri1normalizedlen);
    uri2 = uri32_Parse(uri2normalized, uri2normalizedlen);
    if (!uri1 || !uri2) {
        oom:
        uri32_Free(uri1);
        uri32_Free(uri2);
        free(uri1normalized);
        free(uri2normalized);
        return 0;
    }
    int uri1isfile = 1;
    {
        int compareoom = 0;
        if (!u32u8compare_simple(
                uri1->protocol, uri1->protocollen,
                "file", 0,
                &compareoom
                )) {
            uri1isfile = 0;
            if (compareoom)
                goto oom;
        }
    }
    if (uri1->protocollen != uri2->protocollen ||
            memcpy(uri1->protocol, uri2->protocol,
                sizeof(*uri1->protocol) * uri1->protocollen) != 0 ||
            uri1->pathlen != uri2->pathlen ||
            ((!assumecasesensitivefilepaths ||
              !uri1isfile ||
              memcmp(
                  uri1->path, uri2->path,
                  sizeof(*uri1->path) * uri1->pathlen
              ) != 0))) {
        nomatch:
        uri32_Free(uri1);
        uri32_Free(uri2);
        free(uri1normalized);
        free(uri2normalized);
        *result = 0;
        return 1;
    }
    if (assumecasesensitivefilepaths &&
            uri1isfile) {
        #if defined(_WIN32) || defined(_WIN64)
        // Actually use winapi case folding to compare:
        int wasoom = 0;
        if (!filesys32_WinApiInsensitiveCompare(
                uri1->path, uri1->pathlen,
                uri2->path, uri2->pathlen, &wasoom
                )) {
            if (wasoom) {
                goto oom;
            }
            goto nomatch;
        }
        #else
        assert(0);  // FIXME implement this, if ever required
        goto nomatch;
        #endif
    }
    if (uri1->hostlen != uri2->hostlen ||
            memcmp(
                uri1->host, uri2->host,
                uri1->hostlen * sizeof(*uri1->host)
            ) != 0 ||
            uri1->port != uri2->port) {
        goto nomatch;
    }
    goto match;
}

uri32info *uri32_ParseEx(
        const h64wchar *uri, int64_t urilen,
        const char *default_remote_protocol
        ) {
    if (!uri)
        return NULL;

    uri32info *result = malloc(sizeof(*result));
    if (!result)
        return NULL;
    memset(result, 0, sizeof(*result));
    result->port = -1;

    int isknownfileuri = 0;

    int lastdotindex = -1;
    const h64wchar *part_start = uri;
    int64_t part_start_len = urilen;
    const h64wchar *next_part = uri;
    int64_t next_part_len = urilen;
    while (next_part_len > 0 &&
            *next_part != ' ' && *next_part != ';' && *next_part != ':' &&
            *next_part != '/' && *next_part != '\\' &&
            *next_part != '\n' && *next_part != '\r' && *next_part != '\t' &&
            *next_part != '@' && *next_part != '*' && *next_part != '&' &&
            *next_part != '%' && *next_part != '#' && *next_part != '$' &&
            *next_part != '!' && *next_part != '"' && *next_part != '\'' &&
            *next_part != '(' && *next_part != ')' && *next_part != '|') {
        if (*next_part == '.')
            lastdotindex = (next_part - part_start);
        next_part++;
        next_part_len--;
    }
    int haswinprotocolslashes = 0;
    int recognizedfirstblock = 0;
    int has_protocol_doubleslash = 0;
    {  // Check whether :// follows the first part:
        int compare1oom = 0;
        int compare2oom = 0;
        int _cmplen = next_part_len;
        if (_cmplen > 3)
            _cmplen = 3;
        if (u32u8compare_simple(
                    next_part, _cmplen,
                    "://", 0, &compare1oom
                    )) {
            has_protocol_doubleslash = 1;
        } else if (u32u8compare_simple(
                    next_part, _cmplen,
                    ":\\\\", 0, &compare2oom
                    )) {
            has_protocol_doubleslash = 1;
            haswinprotocolslashes = 1;
        } else {
            if (compare1oom || compare2oom) {
                uri32_Free(result);
                return NULL;
            }
        }
    }
    if (has_protocol_doubleslash) {
        // Extract protocol part at the front.
        result->protocol = malloc(
            (next_part - part_start) * sizeof(*next_part)
        );
        if (!result->protocol) {
            uri32_Free(result);
            return NULL;
        }
        memcpy(
            result->protocol, part_start,
            (next_part - part_start) * sizeof(*next_part)
        );
        result->protocollen = (next_part - part_start);
        next_part += 3;
        next_part_len -= 3;
        lastdotindex = -1;
        part_start = next_part;
        part_start_len = next_part_len;
        int compareoom = 0;
        if (u32u8compare_simple(
                result->protocol, result->protocollen,
                "file", 1, &compareoom)) {
            isknownfileuri = 1;
            int maybewindowspath = haswinprotocolslashes;
            if (!maybewindowspath) {
                // Check if it has a plain unescaped '\' in it:
                int i = 0;
                while (i < part_start_len) {
                    if (part_start[i] == '\\') {
                        maybewindowspath = 1;
                        break;
                    }
                    i++;
                }
            }
            result->path = uri32_ParsePath(
                part_start, part_start_len, 0, &result->pathlen
            );
            if (!result->path) {
                uri32_Free(result);
                return NULL;
            }
            int64_t path_cleaned_len = 0;
            h64wchar *path_cleaned = filesys32_NormalizeEx(
                result->path, result->pathlen, maybewindowspath,
                &path_cleaned_len
            );
            if (!path_cleaned) {
                uri32_Free(result);
                return NULL;
            }
            free(result->path);
            result->path = path_cleaned;
            result->pathlen = path_cleaned_len;
            return result;
        } else {
            isknownfileuri = 0;
            if (compareoom) {
                uri32_Free(result);
                return NULL;
            }
        }
        recognizedfirstblock = 1;
    } else {
        isknownfileuri = 0;
        int is_win_abspath = (
            (next_part_len > 0 &&
             (*next_part) == ':' && (next_part - uri) == 1 &&
            ((uri[0] >= 'a' && uri[0] <= 'z') ||
            (uri[0] >= 'A' && uri[0] <= 'Z')) &&
            (next_part_len > 1 && (*(next_part + 1) == '/' ||
             *(next_part + 1) == '\\')))
        );
        int is_linux_abspath = (
            (*next_part == '/' && (next_part - uri) == 0)
        );
        if (is_win_abspath || is_linux_abspath) {
            // Looks like an absolute path:
            result->protocol = malloc(
                strlen("file") * sizeof(*result->protocol)
            );
            if (!result->protocol) {
                uri32_Free(result);
                return NULL;
            }
            result->protocol[0] = 'f';
            result->protocol[1] = 'i';
            result->protocol[2] = 'l';
            result->protocol[3] = 'e';
            result->protocollen = strlen("file");
            isknownfileuri = 1;
            result->path = filesys32_NormalizeEx(
                uri, urilen, is_win_abspath, &result->pathlen
            );
            if (!result->path) {
                uri32_Free(result);
                return NULL;
            }
            return result;
        } else {
            recognizedfirstblock = 0;
        }
    }

    if (recognizedfirstblock) {
        // We successfully parsed a protocol header, so find
        // the end of the host or whatever comes first now:
        while (next_part_len > 0 && *next_part != ' ' &&
                *next_part != ';' &&
                *next_part != '/' && *next_part != '\\' &&
                *next_part != '\n' && *next_part != '\r' &&
                *next_part != '\t' &&
                *next_part != '@' && *next_part != '*' &&
                *next_part != '&' &&
                *next_part != '%' && *next_part != '#' &&
                *next_part != '$' &&
                *next_part != '!' && *next_part != '"' &&
                *next_part != '\'' &&
                *next_part != '(' && *next_part != ')' &&
                *next_part != '|') {
            if (*next_part == '.')
                lastdotindex = (next_part - part_start);
            next_part++;
            next_part_len--;
        }
    }

    if (next_part_len >= 2 &&
            *next_part == ':' &&
            (*(next_part + 1) >= '0' && *(next_part + 1) <= '9') &&
            lastdotindex > 0 && (
            !default_remote_protocol ||
            h64casecmp(default_remote_protocol, "file") != 0)) {
        // Looks like we've had the host followed by port:
        if (!result->protocol) {
            if (default_remote_protocol) {
                int wasinvalid = 0;
                int wasoom = 0;
                result->protocol = utf8_to_utf32_ex(
                    default_remote_protocol,
                    strlen(default_remote_protocol),
                    NULL, 0, NULL, NULL,
                    &result->protocollen, 0, 1,
                    &wasinvalid, &wasoom
                );
                if (!result->protocol) {
                    uri32_Free(result);
                    return NULL;
                }
                isknownfileuri = 0;
                if (h64casecmp(default_remote_protocol, "file") == 0)
                    isknownfileuri = 1;
            } else {
                result->protocol = NULL;
            }
        }
        result->host = malloc(
            sizeof(*next_part) * (next_part - part_start)
        );
        if (!result->host) {
            uri32_Free(result);
            return NULL;
        }
        memcpy(
            result->host, part_start,
            sizeof(*next_part) * (next_part - part_start)
        );
        result->hostlen = (next_part - part_start);
        next_part++;  // Skip past ':'
        next_part_len--;
        part_start = next_part;
        part_start_len = next_part_len;
        lastdotindex = -1;
        while (next_part_len > 0 &&
                (*next_part >= '0' && *next_part <= '9')) {
            next_part++;
            next_part_len--;
        }
        int64_t portbuflen = 0;
        char *portbuf = malloc((next_part - part_start) * 5 + 1);
        int convertresult = 0;
        if (portbuf) {
            convertresult = utf32_to_utf8(
                part_start, next_part - part_start,
                portbuf, (next_part - part_start) * 5 + 1,
                &portbuflen, 1, 1
            );
        }
        if (!portbuf || !convertresult ||
                portbuflen >= (next_part - part_start) * 5 + 1) {
            uri32_Free(result);
            free(portbuf);
            return NULL;
        }
        portbuf[portbuflen] = '\0';
        result->port = atoi(portbuf);
        free(portbuf);
        part_start = next_part;
        part_start_len = next_part_len;
        lastdotindex = -1;
    } else if ((next_part_len == 0 || *next_part == '/') &&
            result->protocol != NULL &&
            (!default_remote_protocol ||
             h64casecmp(default_remote_protocol, "file") != 0)) {
        // Ok, we got directly a path of sorts following the host,
        // or nothing following what looks like a host.
        #ifndef NDEBUG
        assert(!isknownfileuri);
        // ^ should have been handled earlier already when extracting
        //   the protocol.
        #endif
        result->host = malloc(
            sizeof(*next_part) * (next_part - part_start)
        );
        if (!result->host) {
            uri32_Free(result);
            return NULL;
        }
        memcpy(
            result->host, part_start,
            sizeof(*next_part) * (next_part - part_start)
        );
        result->hostlen = (next_part - part_start);
        part_start = next_part;
        part_start_len = next_part_len;
        lastdotindex = -1;
    }

    int dont_unescape_path = 0;
    if (!result->protocol && !result->host && result->port < 0) {
        result->protocol = malloc(
            strlen("file") * sizeof(*result->protocol)
        );
        if (!result->protocol) {
            uri32_Free(result);
            return NULL;
        }
        result->protocol[0] = 'f';
        result->protocol[1] = 'i';
        result->protocol[2] = 'l';
        result->protocol[3] = 'e';
        result->protocollen = strlen("file");
        isknownfileuri = 1;
        dont_unescape_path = 1;  // since no URI header of any kind
    }

    if (!dont_unescape_path) {
        result->path = uri32_ParsePath(
            part_start, part_start_len,
            !isknownfileuri, &result->pathlen
        );
        if (!result->path) {
            uri32_Free(result);
            return NULL;
        }
    } else {
        result->path = malloc(
            sizeof(*result->path) * part_start_len
        );
        if (!result->path) {
            uri32_Free(result);
            return NULL;
        }
        memcpy(
            result->path, part_start,
            sizeof(*result->path) * part_start_len
        );
        result->pathlen = part_start_len;
    }

    int comparewasoom = 0;
    if (isknownfileuri) {  // normalize path if file url:
        int64_t path_cleaned_len = 0;
        h64wchar *path_cleaned = filesys32_Normalize(
            result->path, result->pathlen, &path_cleaned_len
        );
        if (!path_cleaned) {
            uri32_Free(result);
            return NULL;
        }
        free(result->path);
        result->path = path_cleaned;
        result->pathlen = path_cleaned_len;
    } else if (comparewasoom) {
        uri32_Free(result);
        return NULL;
    }
    return result;
}

uri32info *uri32_Parse(
        const h64wchar *uri, int64_t urilen
        ) {
    return uri32_ParseEx(uri, urilen, "https");
}

h64wchar *uriencode(
        const h64wchar *path, int64_t pathlen, int64_t *out_len
        ) {
    h64wchar *buf = malloc(
        sizeof(*buf) * (pathlen * 3)  // worst case len
    );
    if (!buf)
        return NULL;
    uint64_t buffill = 0;
    unsigned int i = 0;
    while (i < pathlen) {
        if (path[i] == '%' ||
                #if !defined(_WIN32) && !defined(_WIN64)
                path[i] == '\\' ||
                #endif
                path[i] <= 32 ||
                path[i] == ' ' || path[i] == '\t' ||
                path[i] == '[' || path[i] == ']' ||
                path[i] == ':' || path[i] == '?' ||
                path[i] == '&' || path[i] == '=' ||
                path[i] == '\'' || path[i] == '"' ||
                path[i] == '@' || path[i] == '#') {
            char hexval[4];
            snprintf(hexval, sizeof(hexval) - 1,
                "%x", (int)(path[i]));
            buf[buffill] = '%'; buffill++;
            unsigned int z = strlen(hexval);
            while (z < 2) {
                buf[buffill] = '0'; buffill++;
                z++;
            }
            z = 0;
            while (z < strlen(hexval)) {
                buf[buffill] = hexval[z]; buffill++;
                z++;
            }
        } else {
            #if defined(_WIN32) || defined(_WIN64)
            if (path[i] == '\\') {
                buf[buffill] = '/'; buffill++;
                i++;
                continue;
            }
            #endif
            assert((int64_t)buffill < pathlen * 3);
            buf[buffill] = path[i]; buffill++;
        }
        i++;
    }
    if (out_len) *out_len = buffill;
    return buf;
}

h64wchar *uri32_DumpEx(
    const uri32info *uinfo, int absolutefilepaths,
    int64_t *out_len
);

h64wchar *uri32_Normalize(
        const h64wchar *uri, int64_t urilen,
        int absolutefilepaths, int64_t *out_len
        ) {
    uri32info *uinfo = uri32_Parse(uri, urilen);
    if (!uinfo) {
        return NULL;
    }
    int64_t resultlen = 0;
    h64wchar *result = uri32_DumpEx(
        uinfo, absolutefilepaths, &resultlen
    );
    uri32_Free(uinfo);
    if (out_len) *out_len = resultlen;
    return result;
}

h64wchar *uri32_Dump(const uri32info *uinfo, int64_t *out_len) {
    return uri32_DumpEx(uinfo, 0, out_len);
}

h64wchar *uri32_DumpEx(
        const uri32info *uinfo, int absolutefilepaths,
        int64_t *out_len
        ) {
    int64_t portlen = 0;
    h64wchar *port = NULL;
    {
        char portu8buf[128] = "";
        if (uinfo->port > 0) {
            h64snprintf(
                portu8buf, sizeof(portu8buf) - 1,
                ":%d", uinfo->port
            );
            portu8buf[sizeof(portu8buf) - 1] = '\0';
        }
        int wasinvalid, wasoom;
        port = utf8_to_utf32_ex(
            portu8buf, strlen(portu8buf),
            NULL, 0, NULL, NULL, &portlen,
            0, 0, &wasinvalid, &wasoom
        );
        if (!port)
            return NULL;
    }
    int64_t pathlen = 0;
    h64wchar *path = malloc(
        (uinfo->path ? uinfo->pathlen : 1) * sizeof(*path)
    );
    if (!path) {
        free(port);
        return NULL;
    }
    if (uinfo->path && uinfo->pathlen > 0) {
        memcpy(
            path, uinfo->path,
            sizeof(*path) * uinfo->pathlen
        );
        pathlen = uinfo->pathlen;
    }
    int isfileuri = 0;
    {
        int compareoom = 0;
        if (u32u8compare_simple(
                uinfo->protocol, uinfo->protocollen,
                "file", 0, &compareoom
                )) {
            isfileuri = 1;
        } else if (compareoom) {
            free(path);
            free(port);
        }
    }
    if (isfileuri &&
            (!filesys32_IsAbsolutePath(path, pathlen) &&
             (pathlen < 1 || path[0] != '/')) &&
            absolutefilepaths
            ) {
        int64_t newpathlen = 0;
        h64wchar *newpath = filesys32_ToAbsolutePath(
            path, pathlen, &newpathlen
        );
        if (!newpath) {
            free(path);
            free(port);
            return NULL;
        }
        free(path);
        path = newpath;
        pathlen = newpathlen;
    }
    {
        int64_t encodedpathlen = 0;
        h64wchar *encodedpath = uriencode(
            path, pathlen, &encodedpathlen
        );
        if (!encodedpath) {
            free(path);
            free(port);
            return NULL;
        }
        free(path);
        path = encodedpath;
        pathlen = encodedpathlen;
    }

    int upperboundlen = (
        (uinfo->protocol ? uinfo->protocollen : 0) +
        strlen("://") +
        (uinfo->host ? uinfo->hostlen : 0) +
        portlen + pathlen
    ) + 10;
    h64wchar *buf = malloc(
        sizeof(h64wchar) * upperboundlen
    );
    if (!buf) {
        free(port);
        free(path);
        return NULL;
    }
    int64_t plen = 0;
    h64wchar *p = buf;
    if (uinfo->protocol) {
        memcpy(p, uinfo->protocol, sizeof(*p) * uinfo->protocollen);
        p += uinfo->protocollen;
        plen += uinfo->protocollen;
        *p = ':'; p++; plen++;
        *p = '/'; p++; plen++;
        *p = '/'; p++; plen++;
    }
    if (uinfo->host) {
        memcpy(p, uinfo->host, sizeof(*p) * uinfo->hostlen);
        p += uinfo->hostlen;
        plen += uinfo->hostlen;
    }
    if (path && pathlen > 0) {
        memcpy(p, path, sizeof(*p) * pathlen);
        p += pathlen;
        plen += pathlen;
    }
    free(path);
    path = NULL;
    free(port);
    port = NULL;
    h64wchar *shrunkbuf = malloc(
        sizeof(*buf) * (plen > 0 ? plen : 1LL)
    );
    if (!shrunkbuf) {
        free(buf);
        return NULL;
    }
    memcpy(
        shrunkbuf, buf, sizeof(*buf) * plen
    );
    free(buf);
    if (out_len) *out_len = plen;
    return shrunkbuf;
}

void uri32_Free(uri32info *uri) {
    if (!uri)
        return;
    if (uri->protocol)
        free(uri->protocol);
    if (uri->host)
        free(uri->host);
    if (uri->path)
        free(uri->path);
    free(uri);
}
