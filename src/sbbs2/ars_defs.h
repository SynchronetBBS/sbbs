/* ARS_DEFS.H */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

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

char *arstr(ushort *count, char *str);

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
    ,AR_USER
	,AR_TIME
    ,AR_PCR
	,AR_FLAG1
	,AR_FLAG2
	,AR_FLAG3
	,AR_FLAG4
	,AR_EXEMPT
	,AR_REST
    ,AR_SEX
	,AR_UDR
	,AR_UDFR
	,AR_EXPIRE
	,AR_CREDIT
	,AR_DAY
	,AR_ANSI
	,AR_RIP
	,AR_LOCAL
	,AR_GROUP
	,AR_SUB
	,AR_LIB
	,AR_DIR
	,AR_EXPERT
	,AR_SYSOP
	,AR_QUIET
	,AR_MAIN_CMDS
	,AR_FILE_CMDS
	,AR_RANDOM
	,AR_LASTON
	,AR_LOGONS
	,AR_WIP
	,AR_SUBCODE
	,AR_DIRCODE
	,AR_OS2
	,AR_DOS
    };

#endif		/* Don't add anything after this line */
