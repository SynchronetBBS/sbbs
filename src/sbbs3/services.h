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

#ifndef _SERVICES_H_
#define _SERVICES_H_

#include "startup.h"

typedef struct {

	STARTUP_COMMON_ELEMENTS
	struct in_addr outgoing4;
	struct in6_addr outgoing6;
	str_list_t interfaces;
	char services_ini[128];     // services.ini filename
	char login_info_save[INI_MAX_VALUE_LEN];
	uint max_connects_per_period;
	uint connect_rate_limit_period;    /* in seconds */

	/* JavaScript operating parameters */
	js_startup_t js;

} services_startup_t;

#if 0
/* startup options that requires re-initialization/recycle when changed */
static struct init_field services_init_fields[] = {
	OFFSET_AND_SIZE(services_startup_t, outgoing4)
	OFFSET_AND_SIZE(services_startup_t, outgoing6)
	, OFFSET_AND_SIZE(services_startup_t, ctrl_dir)
	, { 0, 0 }    /* terminator */
};
#endif

/* Option bit definitions	*/
#define SERVICE_OPT_UDP         (1 << 0)  /* UDP Socket */
#define SERVICE_OPT_STATIC      (1 << 1)  /* Static service (accepts client connectsions) */
#define SERVICE_OPT_STATIC_LOOP (1 << 2)  /* Loop static service until terminated */
#define SERVICE_OPT_NATIVE      (1 << 3)  /* non-JavaScript service */
#define SERVICE_OPT_FULL_ACCEPT (1 << 4)  /* Accept/close connections when server is full */
#define SERVICE_OPT_TLS         (1 << 5)  /* Use TLS */
#define SERVICE_OPT_NO_USER_PROT (1 << 6) /* Don't change the user's protcool field */

/* services_startup_t.options bits that require re-init/recycle when changed */
#define SERVICE_INIT_OPTS   (0)

#if defined(STARTUP_INI_BITDESC_TABLES) || defined(SERVICES_INI_BITDESC_TABLE)
static ini_bitdesc_t service_options[] = {

	{ BBS_OPT_NO_HOST_LOOKUP, "NO_HOST_LOOKUP"   },
	{ BBS_OPT_GET_IDENT, "GET_IDENT"             },
	{ BBS_OPT_NO_RECYCLE, "NO_RECYCLE"           },
	{ BBS_OPT_MUTE, "MUTE"                       },
	{ SERVICE_OPT_UDP, "UDP"                     },
	{ SERVICE_OPT_STATIC, "STATIC"               },
	{ SERVICE_OPT_STATIC_LOOP, "LOOP"            },
	{ SERVICE_OPT_NATIVE, "NATIVE"               },
	{ SERVICE_OPT_FULL_ACCEPT, "FULL_ACCEPT"     },
	{ SERVICE_OPT_TLS, "TLS"                     },
	{ SERVICE_OPT_NO_USER_PROT, "NO_USER_PROT"   },
	/* terminator */
	{ 0, NULL }
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DLLEXPORT
#undef DLLEXPORT
#endif
#ifdef DLLCALL
#undef DLLCALL
#endif

#ifdef _WIN32
	#ifdef SERVICES_EXPORTS
		#define DLLEXPORT __declspec(dllexport)
	#else
		#define DLLEXPORT __declspec(dllimport)
	#endif
	#ifdef __BORLANDC__
		#define DLLCALL
	#else
		#define DLLCALL
	#endif
#else
	#define DLLEXPORT
	#define DLLCALL
#endif

/* arg is pointer to static bbs_startup_t* */
DLLEXPORT void DLLCALL services_thread(void* arg);
DLLEXPORT void DLLCALL services_terminate(void);
DLLEXPORT const char*   DLLCALL services_ver(void);

#ifdef __cplusplus
}
#endif

#endif /* Don't add anything after this line */
