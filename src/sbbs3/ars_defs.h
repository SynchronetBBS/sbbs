/* Synchronet Access Requirement Strings (ARS) constants */

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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _ARS_DEFS_H
#define _ARS_DEFS_H

/************************************************************************/
/* Synchronet Access Requirement Strings fucntion prototypes and type	*/
/* definitions															*/
/************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "gen_defs.h"
#include "scfgdefs.h"

#ifdef __cplusplus
extern "C"
#endif
uchar *arstr(ushort* count, const char* str, scfg_t* cfg, uchar* ar_buf);

#define AR_INVALID	-1				/* Unspecified keyword */

enum {                              /* Access requirement binaries */
     AR_NULL
    ,AR_OR
    ,AR_NOT
    ,AR_EQUAL
    ,AR_BEGNEST
    ,AR_ENDNEST
    ,AR_LEVEL
    ,AR_AGE
    ,AR_BPS
    ,AR_NODE
    ,AR_TLEFT
    ,AR_TUSED
    ,AR_USER		/* 12 */
	,AR_TIME
    ,AR_PCR
	,AR_FLAG1
	,AR_FLAG2
	,AR_FLAG3
	,AR_FLAG4
	,AR_EXEMPT
	,AR_REST		/* 20 */
    ,AR_SEX
	,AR_UDR
	,AR_UDFR
	,AR_EXPIRE
	,AR_CREDIT
	,AR_DAY
	,AR_ANSI
	,AR_RIP
	,AR_LOCAL
	,AR_GROUP		/* 30 */
	,AR_SUB
	,AR_LIB
	,AR_DIR
	,AR_EXPERT
	,AR_SYSOP
	,AR_QUIET
	,AR_MAIN_CMDS
	,AR_FILE_CMDS
	,AR_RANDOM
	,AR_LASTON		/* 40 */
	,AR_LOGONS		
	,AR_WIP
	,AR_SUBCODE
	,AR_DIRCODE
	,AR_OS2
	,AR_DOS
	,AR_WIN32
	,AR_UNIX
	,AR_LINUX
	,AR_SHELL		/* 50 */
	,AR_PROT
	,AR_GUEST
	,AR_QNODE
	,AR_ACTIVE
	,AR_INACTIVE
	,AR_DELETED
	,AR_ULS
	,AR_ULK
	,AR_ULM
	,AR_DLS			/* 60 */
	,AR_DLK
	,AR_DLM
	,AR_HOST	/* Remote/client hostname (wildcards allowed) */
	,AR_IP		/* Remote/client IP address (wildcards allowed) */
	,AR_TERM
	,AR_COLS
	,AR_ROWS
	,AR_PETSCII
	,AR_ASCII
	,AR_UTF8
	,AR_CP437
	,AR_USERNAME
    };

enum ar_type {
	 AR_BOOL
	,AR_NUM
	,AR_STRING
};

static inline enum ar_type ar_type(int artype)
{
	switch(artype) {
		case AR_RIP:
		case AR_WIP:
		case AR_ANSI:
		case AR_PETSCII:
		case AR_ASCII:
		case AR_UTF8:
		case AR_CP437:
		case AR_DOS:
		case AR_OS2:
		case AR_UNIX:
		case AR_LINUX:
		case AR_WIN32:
		case AR_LOCAL:
		case AR_ACTIVE:
		case AR_INACTIVE:
		case AR_DELETED:
		case AR_EXPERT:
		case AR_SYSOP:
		case AR_GUEST:
		case AR_QNODE:
		case AR_QUIET:
			return AR_BOOL;
		case AR_SUBCODE:
		case AR_DIRCODE:
		case AR_SHELL:
		case AR_PROT:
		case AR_HOST:
		case AR_IP:
		case AR_USERNAME:
			return AR_STRING;
	}
	return AR_NUM;
}

#endif		/* Don't add anything after this line */
