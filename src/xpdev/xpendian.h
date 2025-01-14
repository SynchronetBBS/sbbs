/* Macros to convert integer "endianness" */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * https://www.gnu.org/licenses/old-licenses/lgpl-2.0.html					*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _XPENDIAN_H
#define _XPENDIAN_H

#include "gen_defs.h"

/************************/
/* byte-swapping macros */
/************************/
#define BYTE_SWAP_16(x) ((((WORD)(x) & 0xff00) >> 8) | (((WORD)(x) & 0x00ff) << 8))
#define BYTE_SWAP_32(x) ((((DWORD)(x) & 0xff000000) >> 24) | (((DWORD)(x) & 0x00ff0000) >> 8) | (((DWORD)(x) & 0x0000ff00) << 8) | (((DWORD)(x) & 0x000000ff) << 24))
#define BYTE_SWAP_64(x) ((((uint64_t)(x) & 0xff00000000000000ULL) >> 56) | \
						 (((uint64_t)(x) & 0x00ff000000000000ULL) >> 40) | \
						 (((uint64_t)(x) & 0x0000ff0000000000ULL) >> 24) | \
						 (((uint64_t)(x) & 0x000000ff00000000ULL) >> 8)  | \
						 (((uint64_t)(x) & 0x00000000ff000000ULL) << 8)  | \
						 (((uint64_t)(x) & 0x0000000000ff0000ULL) << 24) | \
						 (((uint64_t)(x) & 0x000000000000ff00ULL) << 40) | \
						 (((uint64_t)(x) & 0x00000000000000ffULL) << 56))

/* auto-detect integer size */
#define BYTE_SWAP_INT(x)    (sizeof(x) == 2 ? BYTE_SWAP_16(x) : sizeof(x) == 4 ? BYTE_SWAP_32(x) : sizeof(x) == 8 ? BYTE_SWAP_64(x) : (x))

/********************************/
/* Architecture-specific macros */
/********************************/
#ifdef __BIG_ENDIAN__   /* e.g. Motorola */

	#define BE_SHORT(x)     (x)
	#define BE_LONG(x)      (x)
	#define BE_INT16(x)     (x)
	#define BE_INT32(x)     (x)
	#define BE_INT64(x)     (x)
	#define BE_INT(x)       (x)
	#define LE_SHORT(x)     BYTE_SWAP_16(x)
	#define LE_LONG(x)      BYTE_SWAP_32(x)
	#define LE_INT16(x)     BYTE_SWAP_16(x)
	#define LE_INT32(x)     BYTE_SWAP_32(x)
	#define LE_INT64(x)     BYTE_SWAP_64(x)
	#define LE_INT(x)       BYTE_SWAP_INT(x)

#else   /* Little Endian (e.g. Intel) */

	#define LE_SHORT(x)     (x)
	#define LE_LONG(x)      (x)
	#define LE_INT16(x)     (x)
	#define LE_INT32(x)     (x)
	#define LE_INT64(x)     (x)
	#define LE_INT(x)       (x)
	#define BE_SHORT(x)     BYTE_SWAP_16(x)
	#define BE_LONG(x)      BYTE_SWAP_32(x)
	#define BE_INT16(x)     BYTE_SWAP_16(x)
	#define BE_INT32(x)     BYTE_SWAP_32(x)
	#define BE_INT64(x)     BYTE_SWAP_64(x)
	#define BE_INT(x)       BYTE_SWAP_INT(x)

#endif

#endif  /* Don't add anything after this line */
