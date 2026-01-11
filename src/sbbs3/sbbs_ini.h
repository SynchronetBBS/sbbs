/* Synchronet initialization (.ini) file routines */

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

#ifndef _SBBS_INI_H
#define _SBBS_INI_H

#include "ini_file.h"
#include "startup.h"    /* bbs_startup_t */
#include "ftpsrvr.h"    /* ftp_startup_t */
#include "mailsrvr.h"   /* mail_startup_t */
#include "services.h"   /* services_startup_t */
#include "websrvr.h"    /* services_startup_t */
#include "scfgdefs.h"   /* scfg_t */

#if defined(__cplusplus)
extern "C" {
#endif

void sbbs_get_ini_fname(
	  char* ini_file
	, const char* ctrl_dir
	);

bool sbbs_read_ini(
	  FILE*                  fp
	, const char*            ini_fname
	, global_startup_t*      global
	, bool*                  run_bbs
	, bbs_startup_t*         bbs_startup
	, bool*                  run_ftp
	, ftp_startup_t*         ftp_startup
	, bool*                  run_web
	, web_startup_t*         web_startup
	, bool*                  run_mail
	, mail_startup_t*        mail_startup
	, bool*                  run_services
	, services_startup_t*    services_startup
	);

void sbbs_free_ini(
	  global_startup_t*      global
	, bbs_startup_t*         bbs_startup
	, ftp_startup_t*         ftp_startup
	, web_startup_t*         web_startup
	, mail_startup_t*        mail_startup
	, services_startup_t*    services_startup
	);

void sbbs_get_js_settings(
	  str_list_t list
	, const char* section
	, js_startup_t* js
	, js_startup_t* defaults
	);

bool sbbs_set_js_settings(
	  str_list_t* list
	, const char* section
	, js_startup_t* js
	, js_startup_t* defaults
	, ini_style_t*
	);

bool sbbs_write_ini(
	  FILE*                  fp
	, scfg_t*                cfg
	, global_startup_t*      global
	, bool run_bbs
	, bbs_startup_t*         bbs
	, bool run_ftp
	, ftp_startup_t*         ftp
	, bool run_web
	, web_startup_t*         web
	, bool run_mail
	, mail_startup_t*        mail
	, bool run_services
	, services_startup_t*    services
	);


#if defined(__cplusplus)
}
#endif

#endif  /* Don't add anything after this line */
