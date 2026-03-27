/*
 * ssh-arch.h -- Internal arch-layer functions.
 * Not a public header.  Included via ssh-internal.h.
 */
#ifndef DSSH_ARCH_INTERNAL_H
#define DSSH_ARCH_INTERNAL_H

#include "deucessh.h"

/* Only dssh_parse_uint32() and dssh_serialize_uint32() remain;
 * they are declared DSSH_PUBLIC in deucessh-arch.h.
 * All other parse/serialize functions were removed (dead code,
 * no library callers). */

#endif // ifndef DSSH_ARCH_INTERNAL_H
