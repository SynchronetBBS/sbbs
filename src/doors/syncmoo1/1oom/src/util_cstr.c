#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util_cstr.h"
#include "lib.h"
#include "log.h"

/* -------------------------------------------------------------------------- */

static inline int parse_hex_char(char c)
{
    int val;
    if ((c >= '0') && (c <= '9')) {
        val = c - '0';
    } else if ((c >= 'A') && (c <= 'F')) {
        val = c - 'A' + 10;
    } else if ((c >= 'a') && (c <= 'f')) {
        val = c - 'a' + 10;
    } else {
        val = -1;
    }
    return val;
}

static inline int parse_hex_2char(const char *p)
{
    uint8_t val;
    int t;
    t = parse_hex_char(p[0]);
    if (t < 0) {
        return t;
    }
    val = t << 4;
    t = parse_hex_char(p[1]);
    if (t < 0) {
        return t;
    }
    val |= t;
    return val;
}

/* -------------------------------------------------------------------------- */

int util_cstr_parse(const char *str, char *dst, uint32_t *len_out)
{
    uint32_t len = 0;
    const char *p = str;
    char c;
    ++p;
    while ((c = *p++) != '"') {
        if (c == '\\') {
            c = *p++;
            switch (c) {
                case '"':
                case '\\':
                    break;
                case 'n':
                    c = '\n';
                    break;
                case 'r':
                    c = '\r';
                    break;
                case 'x':
                    {
                        int val;
                        val = parse_hex_2char(p);
                        if (val < 0) {
                            log_error("invalid hex escape\n");
                            return false;
                        }
                        c = val;
                        p += 2;
                    }
                    break;
                default:
                    log_error("unhandled escape char 0x%02x\n", c);
                    return -1;
            }
        } else if ((c < 0x20) || (c > 0x7e)) {
            log_error("invalid char 0x%02x\n", c);
            return -1;
        }
        *dst++ = c;
        ++len;
    }
    *dst = '\0';
    if (len_out) {
        *len_out = len;
    }
    return p - str;
}

int util_cstr_out(FILE *fd, const char *str)
{
    char c;
    while ((c = *str++) != 0) {
        if ((c == '\\') || (c == '"')) {
            if (fputc('\\', fd) == EOF) {
                return -1;
            }
        }
        if (fputc(c, fd) == EOF) {
            return -1;
        }
    }
    return 0;
}

int util_cstr_parse_in_place(char *str)
{
    char *p = str;
    char c;
    while ((c = *str++) != 0) {
        if (c == '"') {
            break;
        }
        if (c == '\\') {
            c = *str++;
            switch (c) {
                case '\\':
                case '"':
                    break;
                case 'n':
                    c = '\n';
                    break;
                case 't':
                    c = '\t';
                    break;
                default:
                    log_error("%s: unhandled escape 0x%02x\n", __func__, c);
                    return -1;
            }
        }
        *p++ = c;
    }
    *p = 0;
    return 0;
}
