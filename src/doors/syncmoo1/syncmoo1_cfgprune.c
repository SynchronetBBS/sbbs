/* syncmoo1_cfgprune.c -- see syncmoo1_cfgprune.h. */

#include "syncmoo1_cfgprune.h"

#include <stdlib.h>
#include <string.h>

/* Span of `s` with leading and trailing spaces/tabs/CR removed, written to
 * `out` (truncated to outlen-1). Returns out. */
static char *sm_trim(const char *s, size_t len, char *out, size_t outlen)
{
    size_t b = 0, e = len, n;

    while (b < e && (s[b] == ' ' || s[b] == '\t'))
        ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r'))
        --e;
    n = e - b;
    if (n >= outlen)
        n = outlen - 1;
    memcpy(out, s + b, n);
    out[n] = '\0';
    return out;
}

/* Split a line at its first '='. Returns 0 when there is none, or when the
 * line is blank or a comment. */
static int sm_split(const char *line, size_t len, char *key, size_t keylen,
                    char *val, size_t vallen)
{
    const char *eq;
    char        first[2];

    sm_trim(line, len, first, sizeof first);
    if (first[0] == '\0' || first[0] == '#' || first[0] == ';')
        return 0;
    eq = memchr(line, '=', len);
    if (eq == NULL)
        return 0;
    sm_trim(line, (size_t)(eq - line), key, keylen);
    sm_trim(eq + 1, len - (size_t)(eq - line) - 1, val, vallen);
    return key[0] != '\0';
}

int sm_cfg_key(const char *line, char *out, size_t outlen)
{
    char val[256];

    return sm_split(line, strlen(line), out, outlen, val, sizeof val);
}

/* Value of `key` in `base`, copied to `out`. Returns 1 when found. */
static int sm_base_value(const char *base, const char *key,
                         char *out, size_t outlen)
{
    const char *p = base;

    while (*p != '\0') {
        const char *nl = strchr(p, '\n');
        size_t      len = (nl != NULL) ? (size_t)(nl - p) : strlen(p);
        char        k[128], v[256];

        if (sm_split(p, len, k, sizeof k, v, sizeof v)
            && strcmp(k, key) == 0) {
            size_t n = strlen(v);
            if (n >= outlen)
                n = outlen - 1;
            memcpy(out, v, n);
            out[n] = '\0';
            return 1;
        }
        if (nl == NULL)
            break;
        p = nl + 1;
    }
    return 0;
}

int sm_cfg_prune(const char *base, const char *user, char **out, size_t *outlen)
{
    size_t cap = strlen(user) + 1;
    char  *buf = (char *)malloc(cap);
    size_t used = 0;
    const char *p = user;

    if (out != NULL)
        *out = NULL;
    if (buf == NULL)
        return -1;

    while (*p != '\0') {
        const char *nl  = strchr(p, '\n');
        size_t      len = (nl != NULL) ? (size_t)(nl - p) : strlen(p);
        char        k[128], v[256], bv[256];
        int         drop = 0;

        if (sm_split(p, len, k, sizeof k, v, sizeof v)
            && sm_base_value(base, k, bv, sizeof bv)
            && strcmp(v, bv) == 0)
            drop = 1;   /* the user never diverged from the sysop's value */

        if (!drop) {
            memcpy(buf + used, p, len);
            used += len;
            buf[used++] = '\n';
        }
        if (nl == NULL)
            break;
        p = nl + 1;
    }
    buf[used] = '\0';
    if (out != NULL)
        *out = buf;
    else
        free(buf);
    if (outlen != NULL)
        *outlen = used;
    return 0;
}
