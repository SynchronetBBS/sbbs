/* 32-bit CRC table and calculation macro */

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
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _CRC32_H_
#define _CRC32_H_

#include <stdio.h>  /* FILE */
#include "gen_defs.h"   /* uint32_t */

#ifdef __cplusplus
extern "C" {
#endif

extern int32_t crc32tbl[];
extern const uint32_t crc32tbl_slicing[3][256];

uint32_t crc32_update(uint32_t crc, const void* buf, size_t);
uint32_t crc32i(uint32_t crc, const char* buf, size_t);
uint32_t fcrc32(FILE* fp, size_t);

#ifdef __cplusplus
}
#endif

#define ucrc32(ch, crc) (crc32tbl[(crc ^ (ch)) & 0xff] ^ (crc >> 8))
static inline uint32_t ucrc32_4(const unsigned char* p, uint32_t crc)
{
	crc ^= (uint32_t)p[0]
	    | ((uint32_t)p[1] << 8)
	    | ((uint32_t)p[2] << 16)
	    | ((uint32_t)p[3] << 24);
	return crc32tbl_slicing[2][crc & 0xff]
	    ^ crc32tbl_slicing[1][(crc >> 8) & 0xff]
	    ^ crc32tbl_slicing[0][(crc >> 16) & 0xff]
	    ^ (uint32_t)crc32tbl[crc >> 24];
}
#define crc32(x, y) crc32i(0xffffffff, x, y)

#endif  /* Don't add anything after this line */
