/* ver.cpp */

/* Synchronet version display */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#define BETA		" alpha"     /* Space if non-beta, " beta" otherwise */

extern WSADATA WSAData;

void sbbs_t::ver()
{
	char str[128],compiler[32];

	CRLF;
	strcpy(str,VERSION_NOTICE);
#ifdef _DEBUG
	strcat(str,"Debug");
#endif
	center(str);
	CRLF;

#if defined(__BORLANDC__)
	sprintf(compiler,"BCC %X.%02X"
		,__BORLANDC__>>8
		,__BORLANDC__&0xff);
#elif defined(_MSC_VER)
	sprintf(compiler,"MSC %u", _MSC_VER);
#else
	strcpy(compiler,"UNKNOWN COMPILER");
#endif

	sprintf(str,"Revision %c%s %s %.5s  "
		"SMBLIB %s  %s"
		,REVISION,BETA,__DATE__,__TIME__
		,smb_lib_ver(),compiler);

	center(str);
	CRLF;

	sprintf(str,"%s - http://www.synchro.net", COPYRIGHT_NOTICE);
	center(str);
	CRLF;

#if defined(__OS2__)

	sprintf(str,"OS/2 %u.%u (%u.%u)",_osmajor/10,_osminor/10,_osmajor,_osminor);

#elif defined(_WIN32)

	center(WSAData.szDescription);
	CRLF;

	/* Windows Version */
	char*			winflavor=nulstr;
	OSVERSIONINFO	winver;

	winver.dwOSVersionInfoSize=sizeof(winver);
	GetVersionEx(&winver);

	switch(winver.dwPlatformId) {
		case VER_PLATFORM_WIN32_NT:
			winflavor="NT ";
			break;
		case VER_PLATFORM_WIN32s:
			winflavor="Win32s ";
			break;
		case VER_PLATFORM_WIN32_WINDOWS:
			winver.dwBuildNumber&=0xffff;
			break;
	}

	sprintf(str,"Windows %sVersion %u.%02u (Build %u) %s"
			,winflavor
			,winver.dwMajorVersion, winver.dwMinorVersion
			,winver.dwBuildNumber,winver.szCSDVersion);

#else	/* DOS */

	sprintf(str,"DOS %u.%02u",_osmajor,_osminor);

#endif

	center(str);
}

