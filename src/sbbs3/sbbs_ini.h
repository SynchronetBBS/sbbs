/* sbbs_ini.h */

#ifndef _SBBS_INI_H
#define _SBBS_INI_H

#include "startup.h"	/* bbs_startup_t */
#include "ftpsrvr.h"	/* ftp_startup_t */
#include "mailsrvr.h"	/* mail_startup_t */
#include "services.h"	/* services_startup_t */
#include "websrvr.h"	/* services_startup_t */
#include "ini_file.h"

#if defined(__cplusplus)
extern "C" {
#endif

void sbbs_read_ini(
	 FILE*					fp
	,BOOL*					run_bbs
	,bbs_startup_t*			bbs_startup
	,BOOL*					run_ftp
	,ftp_startup_t*			ftp_startup
	,BOOL*					run_web
	,web_startup_t*			web_startup
	,BOOL*					run_mail		
	,mail_startup_t*		mail_startup
	,BOOL*					run_services
	,services_startup_t*	services_startup
	);

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
