/* Synchronet log file routines (C-exported from logfile.cpp) */

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
 
#ifndef LOGFILE_H_
#define LOGFILE_H_

#include "dllexport.h"
#include "mqtt.h"
#include "scfgdefs.h"
#include "sockwrap.h"

#include <stdio.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT int		errorlog(scfg_t*, struct mqtt*, int level, const char* host, const char* text);
DLLEXPORT bool		hacklog(scfg_t*, struct mqtt*, const char* prot, const char* user, const char* text
									,const char* host, union xp_sockaddr* addr);
DLLEXPORT bool		spamlog(scfg_t*, struct mqtt*, char* prot, char* action, char* reason
									,char* host, char* ip_addr, char* to, char* from);
DLLEXPORT FILE*		fopenlog(scfg_t*, const char* path, bool shareable);
DLLEXPORT size_t	fprintlog(scfg_t*, FILE**, const char* str);
DLLEXPORT void		fcloselog(FILE*);

#ifdef __cplusplus
}
#endif

#endif	/* Don't add anything after this line */
