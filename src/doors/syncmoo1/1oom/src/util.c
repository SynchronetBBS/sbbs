#include "config.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "lib.h"
#include "os.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

char *util_concat(const char *s, ...)
{
#define UTIL_CONCAT_MAX_ARGS 128
    const char *arg;
    char *newp, *ptr;
    int num_args;
    size_t arg_len[UTIL_CONCAT_MAX_ARGS], tot_len;
    int i;
    va_list ap;

    arg_len[0] = tot_len = strlen(s);

    va_start(ap, s);
    for (i = 1; i < UTIL_CONCAT_MAX_ARGS && (arg = va_arg(ap, const char *)) != NULL; ++i) {
        arg_len[i] = strlen(arg);
        tot_len += arg_len[i];
    }
    num_args = i;
    va_end(ap);

    newp = lib_malloc(tot_len + 1);

    if (arg_len[0] > 0) {
        memcpy(newp, s, arg_len[0]);
    }
    ptr = newp + arg_len[0];

    va_start(ap, s);
    for (i = 1; i < num_args; ++i) {
        memcpy(ptr, va_arg(ap, const char *), arg_len[i]);
        ptr += arg_len[i];
    }
    *ptr = '\0';
    va_end(ap);

    return newp;
}

int util_concat_buf(char *buf, int buflen, ...)
{
    const char *arg;
    char *ptr;
    int num_args;
    size_t arg_len[UTIL_CONCAT_MAX_ARGS], tot_len = 0, len = 0;
    int i;
    va_list ap;

    va_start(ap, buflen);
    for (i = 0; i < UTIL_CONCAT_MAX_ARGS && (arg = va_arg(ap, const char *)) != NULL; ++i) {
        arg_len[i] = strlen(arg);
        tot_len += arg_len[i];
    }
    num_args = i;
    va_end(ap);

    ptr = buf;
    --buflen;   /* reserve space for terminating '\0' */
    va_start(ap, buflen);
    for (i = 0; (i < num_args) && (buflen > 0); ++i) {
        int l;
        l = arg_len[i];
        if (l > buflen) {
            l = buflen;
        }
        memcpy(ptr, va_arg(ap, const char *), l);
        ptr += l;
        len += l;
        buflen -= l;
    }
    va_end(ap);
    *ptr = '\0';
    --buflen;

    return (len == tot_len) ? len : (len - tot_len);
}

/* Input one line from the file descriptor `f'.  FIXME: we need something
   better, like GNU `getline()'.  */
int util_get_line(char *buf, int bufsize, FILE *f)
{
    char *r;
    size_t len;

    r = fgets(buf, bufsize, f);

    if (r == NULL) {
        return -1;
    }

    len = strlen(buf);

    if (len > 0) {
        char *p;

        /* Remove trailing newline characters.  */
        /* Remove both 0x0a and 0x0d characters, this solution makes it */
        /* work on all target platforms: Unixes, Win32, DOS, and even for MAC */
        while ((len > 0) && ((*(buf + len - 1) == 0x0d) || (*(buf + len - 1) == 0x0a))) {
            len--;
        }

        /* Remove useless spaces.  */
        while ((len > 0) && (*(buf + len - 1) == ' ')) {
            len--;
        }
        for (p = buf; *p == ' '; p++, len--);
        memmove(buf, p, len + 1);
        *(buf + len) = '\0';
    }

    return (int)len;
}

/* Split `path' into a file name and a directory component.  Unlike
   the MS-DOS `fnsplit', the directory does not have a trailing '/'.  */
void util_fname_split(const char *path, char **dir_out, char **name_out)
{
    const char *p;

    if (path == NULL) {
        *dir_out = *name_out = NULL;
        return;
    }

    p = strrchr(path, FSDEV_DIR_SEP_CHR);

#ifdef FSDEV_DIR_SEP_ALT
    {
        const char *p1;
        p1 = strrchr(path, FSDEV_DIR_SEP_ALT);
        if ((p == NULL) || (p < p1)) {
            p = p1;
        }
    }
#endif

    if (p == NULL) {
        if (dir_out != NULL) {
            *dir_out = NULL;
        }
        if (name_out != NULL) {
            *name_out = lib_stralloc(path);
        }
        return;
    }

    if (dir_out != NULL) {
        *dir_out = lib_malloc((size_t)(p - path + 1));
        memcpy(*dir_out, path, p - path);
        (*dir_out)[p - path] = '\0';
    }

    if (name_out != NULL) {
        *name_out = lib_stralloc(p + 1);
    }
}

/* Write the first `size' bytes of `src' into a newly created file `name'.
   If `name' already exists, it is replaced by the new one.  Returns 0 on
   success, -1 on failure.  */
int util_file_save(const char *name, const uint8_t *src, int size)
{
    FILE *fd;
    size_t r;
    fd = fopen(name, "wb");
    if (fd == NULL) {
        return -1;
    }
    r = fwrite((char *)src, size, 1, fd);
    fclose(fd);
    return (r < 1) ? -1 : 0;
}

uint8_t *util_file_load(const char *filename, uint32_t *len_out)
{
    FILE *fd = NULL;
    uint8_t *data = NULL;
    uint32_t len = 0;
    if ((fd = fopen(filename, "rb")) == NULL) {
        perror(filename);
        goto fail;
    }
    if (fseek(fd, 0, SEEK_END) != 0) {
        perror(filename);
        goto fail;
    }
    len = ftell(fd);
    rewind(fd);
    data = lib_malloc(len + 1);
    if (fread(data, len, 1, fd) < 1) {
        goto fail;
    }
    data[len] = '\0';
    *len_out = len;
    fclose(fd);
    fd = NULL;
    return data;

fail:
    *len_out = 0;
    if (fd) {
        fclose(fd);
        fd = NULL;
    }
    if (data) {
        lib_free(data);
        data = NULL;
    }
    return NULL;
}

void util_trim_whitespace(char *buf, size_t bufsize)
{
    int i;
    for (i = 0; ((buf[i] == ' ') || (buf[i] == '\t')); ++i);
    if (i > 0) {
        lib_strcpy(buf, &buf[i], bufsize);
    }
    for (i = 0; i < bufsize && buf[i] != '\0'; ++i);
    --i;
    for (; (i >= 0) && (((buf[i] == ' ') || (buf[i] == '\t'))); --i) {
        buf[i] = '\0';
    }
}

void util_str_tolower(char *buf, size_t bufsize)
{
    char c;
    char *buf_end = buf + bufsize;
    while (buf != buf_end && (c = *buf) != '\0') {
        /* tolower leaves characters that aren't upper-case letters unchanged. */
        *buf++ = tolower(c);
    }
}

bool util_parse_number(const char *str, uint32_t *val_ptr)
{
    char *strend = NULL;
    uint32_t v = strtoul(str, &strend, 0);
    *val_ptr = v;
    return (*strend == '\0');
}

bool util_parse_signed_number(const char *str, int *val_ptr)
{
    char *strend = NULL;
    int v = strtol(str, &strend, 10);
    *val_ptr = v;
    return (*strend == '\0');
}

int32_t *util_parse_numbers(const char *str, char sep, int *numptr)
{
    char *strend = NULL;
    int32_t *nums = NULL;
    int numnum = 0;
    char c;
    do {
        int32_t v;
        v = strtol(str, &strend, 0);
        do {
            c = *strend++;
        } while ((c == ' ') || (c == '\t'));
        if ((c == sep) || (c == '\n') || (c == '\r') || (c == '\0')) {
            nums = lib_realloc(nums, (numnum + 1) * sizeof(int32_t));
            nums[numnum++] = v;
            if (c == sep) {
                str = strend;
            } else {
                c = '\0';
            }
        } else {
            lib_free(nums);
            nums = NULL;
            numnum = 0;
            c = '\0';
        }
    } while (c != '\0');
    if (numptr) {
        *numptr = numnum;
    }
    return nums;
}

void util_table_remove_item_keep_order(int itemi, void *tbl, int itemsz, int itemnum)
{
    if ((itemi < 0) || (itemi >= (itemnum - 1))) {
        return;
    }
    memmove(((uint8_t *)tbl) + itemi * itemsz, ((uint8_t *)tbl) + (itemi + 1) * itemsz, (itemnum - 1 - itemi) * itemsz);
}

void util_table_remove_item_keep_order_zero(int itemi, void *tbl, int itemsz, int itemnum)
{
    if ((itemi < 0) || (itemi >= itemnum)) {
        return;
    }
    if (itemi < (itemnum - 1)) {
        memmove(((uint8_t *)tbl) + itemi * itemsz, ((uint8_t *)tbl) + (itemi + 1) * itemsz, (itemnum - 1 - itemi) * itemsz);
    }
    memset(((uint8_t *)tbl) + (itemnum - 1) * itemsz, 0, itemsz);
}

void util_table_remove_item_any_order(int itemi, void *tbl, int itemsz, int itemnum)
{
    if ((itemi < 0) || (itemi >= (itemnum - 1))) {
        return;
    }
    memcpy(((uint8_t *)tbl) + itemi * itemsz, ((uint8_t *)tbl) + (itemnum - 1) * itemsz, itemsz);
}

/* -------------------------------------------------------------------------- */

/* The following `strcasecmp()' implementation is taken from:
   GLIB - Library of useful routines for C programming
   Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh
   MacDonald.
   The source is available from http://www.gtk.org/.  */

#if !defined HAVE_STRCASECMP

int strcasecmp(const char *s1, const char *s2)
{
    int c1, c2;

    if (s1 == NULL || s2 == NULL) {
        return 0;
    }

    while (*s1 && *s2) {
        /* According to A. Cox, some platforms have islower's that don't work
           right on non-uppercase.  */
        c1 = isupper((unsigned int)*s1) ? tolower((unsigned int)*s1) : *s1;
        c2 = isupper((unsigned int)*s2) ? tolower((unsigned int)*s2) : *s2;
        if (c1 != c2) {
            return (c1 - c2);
        }
        s1++; s2++;
    }

    return (((int)(unsigned char)*s1) - ((int)(unsigned char)*s2));
}

#endif
