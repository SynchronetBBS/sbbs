/* ver.cpp */
// vi: tabstop=4

/* Synchronet version display */

/* $Id: ver.cpp,v 1.31 2019/10/08 02:07:26 rswindell Exp $ */

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

#include "sbbs.h"
#include "ssl.h"

const char* beta_version = " "; /* Space if non-beta, " beta" otherwise */

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

#ifndef JSDOOR
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

	sprintf(str,"%s - http://www.synchro.net", COPYRIGHT_NOTICE);
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
		sprintf(str, "  cryptlib %u.%u.%u (%u)", cl_major, cl_minor, cl_step, CRYPTLIB_VERSION);
		center(str);
		CRLF;
	}
#endif

	center(os_version(str));
}
#endif
