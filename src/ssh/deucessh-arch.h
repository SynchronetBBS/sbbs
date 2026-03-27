// RFC-4251

#ifndef DSSH_ARCH_H
#define DSSH_ARCH_H

#ifndef DSSH_H
 #error Only include deucessh.h, do not directly include this file.
#endif

#include <inttypes.h>
#include <stdbool.h>

#include "deucessh-portable.h"

// SSH Data Types (internal representations)
typedef uint8_t dssh_byte;
typedef bool dssh_boolean;
typedef uint32_t dssh_uint32_t;
typedef uint64_t dssh_uint64_t;
typedef struct dssh_string_s {
	const dssh_byte *value;
	dssh_uint32_t    length;
} *dssh_string;
typedef struct dssh_string_s *dssh_mpint;
typedef struct dssh_namelist_s {
	const dssh_byte *value;
	dssh_uint32_t    length;
} *dssh_namelist;

DSSH_PUBLIC int64_t dssh_parse_uint32(const uint8_t *buf, size_t bufsz, dssh_uint32_t *val);
DSSH_PUBLIC int dssh_serialize_uint32(dssh_uint32_t val, uint8_t *buf, size_t bufsz, size_t *pos);

#endif // ifndef DSSH_ARCH_H
