/* crc32.h */

/* 32-bit CRC table and calculation macro */

/* $Id: crc32.h,v 1.18 2019/03/22 21:29:12 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _CRC32_H_
#define _CRC32_H_

#include <stdio.h>	/* FILE */
#include "gen_defs.h"	/* uint32_t */

#if defined(_WIN32) && (defined(CRC_IMPORTS) || defined(CRC_EXPORTS))
	#if defined(CRC_IMPORTS)
		#define CRCEXPORT	__declspec(dllimport)
	#else
		#define CRCEXPORT	__declspec(dllexport)
	#endif
	#if defined(__BORLANDC__)
		#define CRCCALL
	#else
		#define CRCCALL
	#endif
#else	/* !_WIN32 */
	#define CRCEXPORT
	#define CRCCALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

CRCEXPORT extern int32_t crc32tbl[];

CRCEXPORT uint32_t CRCCALL crc32i(uint32_t crc, const char* buf, unsigned long len);
CRCEXPORT uint32_t CRCCALL fcrc32(FILE* fp, unsigned long len);

#ifdef __cplusplus
}
#endif

#define ucrc32(ch,crc) (crc32tbl[(crc^(ch))&0xff]^(crc>>8))
#define crc32(x,y) crc32i(0xffffffff,x,y)

#endif	/* Don't add anything after this line */
