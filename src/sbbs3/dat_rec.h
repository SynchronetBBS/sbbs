/* Synchronet text data access routines (exported) */

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

#ifndef _DAT_REC_H
#define _DAT_REC_H

#include "dllexport.h"

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT int	getrec(const char *instr, int start, int length, char *outstr); /* Retrieve a record from a string */
DLLEXPORT void	putrec(char *outstr, int start, int length, const char *instr); /* Place a record into a string */

#ifdef __cplusplus
}
#endif

#endif
