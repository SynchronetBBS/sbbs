/*
 * ssh-arch.h -- Internal arch-layer functions.
 * Not a public header.  Included via ssh-internal.h.
 */
#ifndef DSSH_ARCH_INTERNAL_H
#define DSSH_ARCH_INTERNAL_H

#include "deucessh.h"

DSSH_PRIVATE int64_t dssh_parse_byte(const uint8_t *buf, size_t bufsz, dssh_byte *val);
DSSH_PRIVATE int dssh_serialize_byte(dssh_byte val, uint8_t *buf, size_t bufsz, size_t *pos);

DSSH_PRIVATE int64_t dssh_parse_boolean(const uint8_t *buf, size_t bufsz, dssh_boolean *val);
DSSH_PRIVATE int dssh_serialize_boolean(dssh_boolean val, uint8_t *buf, size_t bufsz, size_t *pos);

DSSH_PRIVATE int64_t dssh_parse_uint64(const uint8_t *buf, size_t bufsz, dssh_uint64_t *val);
DSSH_PRIVATE int dssh_serialize_uint64(dssh_uint64_t val, uint8_t *buf, size_t bufsz, size_t *pos);

DSSH_PRIVATE int64_t dssh_parse_string(const uint8_t *buf, size_t bufsz, dssh_string val);
DSSH_PRIVATE int dssh_serialize_string(dssh_string val, uint8_t *buf, size_t bufsz, size_t *pos);

DSSH_PRIVATE int64_t dssh_parse_mpint(const uint8_t *buf, size_t bufsz, dssh_mpint val);
DSSH_PRIVATE int dssh_serialize_mpint(dssh_mpint val, uint8_t *buf, size_t bufsz, size_t *pos);

DSSH_PRIVATE int64_t dssh_parse_namelist(const uint8_t *buf, size_t bufsz, dssh_namelist val);
DSSH_PRIVATE int dssh_serialize_namelist(dssh_namelist val, uint8_t *buf, size_t bufsz, size_t *pos);

#endif // ifndef DSSH_ARCH_INTERNAL_H
