/* Synchronet client/content-filtering (trashcan/twit) functions */

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

#ifndef _TRASH_H

#include <time.h>
#include "scfgdefs.h"
#include "str_list.h"
#include "dllexport.h"

#define strIpFilterExemptConfigFile "ipfilter_exempt.cfg"
#define STR_FAILED_LOGIN_ATTEMPTS "FAILED LOGIN ATTEMPTS"

struct trash {
	time_t added;
	time_t expires;
	char prot[32];
	char user[64];
	char reason[128];
};

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT bool		trashcan(scfg_t* cfg, const char *insearch, const char *name);
DLLEXPORT bool		trashcan2(scfg_t* cfg, const char* str1, const char* str2, const char *name, struct trash*);
DLLEXPORT bool		trash_in_list(const char* str1, const char* str2, str_list_t list, struct trash*);
DLLEXPORT char *	trash_details(const struct trash*, char* str, size_t);
DLLEXPORT str_list_t trashcan_list(scfg_t* cfg, const char* name);
DLLEXPORT bool		is_host_exempt(scfg_t*, const char* ip_addr, const char* host_name);
DLLEXPORT bool		filter_ip(scfg_t*, const char* prot, const char* reason, const char* host
								  ,const char* ip_addr, const char* username, const char* fname, uint duration);
DLLEXPORT bool		is_twit(scfg_t*, const char* name);
DLLEXPORT bool		list_twit(scfg_t*, const char* name, const char* comment);
DLLEXPORT str_list_t list_of_twits(scfg_t*);

#ifdef __cplusplus
}
#endif

#endif	/* Don't add anything after this line */
