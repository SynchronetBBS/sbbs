/* endian.h */

/* Macros to convert integer "endianness" */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
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

#ifndef _ENDIAN_H

/************************/
/* byte-swapping macros */
/************************/
#define BYTE_SWAP_SHORT(x)	((((short)(x)&0xff00)>>8) | (((short)(x)&0x00ff)<<8))
#define BYTE_SWAP_LONG(x)	((((long)(x)&0xff000000)>>24) | (((long)(x)&0x00ff0000)>>8) | (((long)(x)&0x0000ff00)<<8) | (((long)(x) & 0x000000ff)<<24))

/* auto-detect integer size */
#define BYTE_SWAP_INT(x)	(sizeof(x)==sizeof(short) ? BYTE_SWAP_SHORT(x) : BYTE_SWAP_LONG(x))

/********************************/
/* Architecture-specific macros */
/********************************/
#ifdef __BIG_ENDIAN__	/* e.g. Motorola */

	#define BE_SHORT(x)		(x)
	#define BE_LONG(x)		(x)
	#define BE_INT(x)		(x)
	#define LE_SHORT(x)		BYTE_SWAP_SHORT(x)
	#define LE_LONG(x)		BYTE_SWAP_LONG(x)
	#define LE_INT(x)		BYTE_SWAP_INT(x)

#else	/* Little Endian (e.g. Intel) */

	#define LE_SHORT(x)		(x)
	#define LE_LONG(x)		(x)
	#define LE_INT(x)		(x)
	#define BE_SHORT(x)		BYTE_SWAP_SHORT(x)
	#define BE_LONG(x)		BYTE_SWAP_LONG(x)
	#define BE_INT(x)		BYTE_SWAP_INT(x)

#endif

#endif	/* Don't add anything after this line */
