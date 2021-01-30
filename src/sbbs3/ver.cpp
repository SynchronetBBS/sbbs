/* Synchronet version info */

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

#include "sbbs.h"
#ifdef SBBS
#include "ssl.h"
#endif
#include "git_hash.h"
#include "git_branch.h"
#include "ver.h"

#ifdef __cplusplus
extern "C" {
#endif

const char* git_hash = GIT_HASH;
const char* git_branch = GIT_BRANCH;
const char* beta_version = " "; /* Space if non-beta, " beta" otherwise */

#ifdef __cplusplus
}
#endif

#if defined(_WINSOCKAPI_)
	extern WSADATA WSAData;
	#define SOCKLIB_DESC WSAData.szDescription
#else
	#define	SOCKLIB_DESC NULL
#endif

#if defined(__unix__)
	#include <sys/utsname.h>	/* uname() */
#endif

char* socklib_version(char* str, char* winsock_ver)
{
#if defined(_WINSOCKAPI_)

	strcpy(str,winsock_ver);

#elif defined(__GLIBC__)

	sprintf(str,"GLIBC %u.%u",__GLIBC__,__GLIBC_MINOR__);

#else
	
	strcpy(str,"");

#endif

	return(str);
}

#if defined(SBBS) && !defined(JSDOOR)
void sbbs_t::ver()
{
	char str[128],compiler[32];

	CRLF;
	strcpy(str,VERSION_NOTICE);
#if defined(_DEBUG)
	strcat(str,"  Debug");
#endif
	center(str);
	CRLF;

	DESCRIBE_COMPILER(compiler);

	sprintf(str,"Revision %c%s %s %.5s  "
		"SMBLIB %s  %s"
		,toupper(REVISION)
		,beta_version
		,__DATE__,__TIME__
		,smb_lib_ver(),compiler);

	center(str);
	CRLF;

	center("https://gitlab.synchro.net - " GIT_BRANCH "/" GIT_HASH); 
	CRLF;

	sprintf(str,"%s - http://synchro.net", COPYRIGHT_NOTICE);
	center(str);
	CRLF;

#ifdef JAVASCRIPT
	if(!(startup->options&BBS_OPT_NO_JAVASCRIPT)) {
		center((char *)JS_GetImplementationVersion());
		CRLF;
	}
#endif

#ifdef USE_CRYPTLIB
	if(is_crypt_initialized()) {
		int cl_major=0, cl_minor=0, cl_step=0;
		int result;
		result = cryptGetAttribute(CRYPT_UNUSED, CRYPT_OPTION_INFO_MAJORVERSION, &cl_major);
		result = cryptGetAttribute(CRYPT_UNUSED, CRYPT_OPTION_INFO_MINORVERSION, &cl_minor);
		result = cryptGetAttribute(CRYPT_UNUSED, CRYPT_OPTION_INFO_STEPPING, &cl_step);
		(void)result;
		safe_snprintf(str, sizeof(str), "cryptlib %u.%u.%u (%u)", cl_major, cl_minor, cl_step, CRYPTLIB_VERSION);
		center(str);
		CRLF;
	}
#endif

	center(os_version(str));
}
#endif
