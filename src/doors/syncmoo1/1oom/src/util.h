#ifndef INC_1OOM_UTIL_H
#define INC_1OOM_UTIL_H

#include "config.h"

#include <stdio.h>
#include <strings.h>

#include "types.h"

extern char *util_concat(const char *s, ...);
extern int util_concat_buf(char *buf, int buflen, ...);
extern int util_get_line(char *buf, int bufsize, FILE *f);
extern void util_fname_split(const char *path, char **directory_out, char **name_out);
extern int util_file_save(const char *name, const uint8_t *src, int size);
extern uint8_t *util_file_load(const char *filename, uint32_t *len_out);
extern void util_trim_whitespace(char *buf, size_t bufsize);
extern void util_str_tolower(char *buf, size_t bufsize);
extern bool util_parse_number(const char *str, uint32_t *val_ptr);
extern bool util_parse_signed_number(const char *str, int *val_ptr);
extern int32_t *util_parse_numbers(const char *str, char sep, int *numptr);
extern void util_table_remove_item_keep_order(int itemi, void *tbl, int itemsz, int itemnum);
extern void util_table_remove_item_keep_order_zero(int itemi, void *tbl, int itemsz, int itemnum);
extern void util_table_remove_item_any_order(int itemi, void *tbl, int itemsz, int itemnum);

#if !defined HAVE_STRCASECMP
extern int strcasecmp(const char *s1, const char *s2);
#endif

#endif
