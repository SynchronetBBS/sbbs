/* dat_rec.h */

/* Synchronet text data access routines (exported) */

/* $Id: dat_rec.h,v 1.5 2019/03/22 21:28:27 rswindell Exp $ */

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

#ifndef _DAT_REC_H
#define _DAT_REC_H

#ifdef DLLEXPORT
#undef DLLEXPORT
#endif
#ifdef DLLCALL
#undef DLLCALL
#endif

#ifdef _WIN32
	#ifdef __MINGW32__
		#define DLLEXPORT
		#define DLLCALL
	#else
		#ifdef SBBS_EXPORTS
			#define DLLEXPORT __declspec(dllexport)
		#else
			#define DLLEXPORT __declspec(dllimport)
		#endif
		#ifdef __BORLANDC__
			#define DLLCALL
		#else
			#define DLLCALL
		#endif
	#endif
#else
	#define DLLEXPORT
	#define DLLCALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT void	DLLCALL getrec(const char *instr,int start,int length,char *outstr); /* Retrieve a record from a string */
DLLEXPORT void	DLLCALL putrec(char *outstr,int start,int length,char *instr); /* Place a record into a string */

#ifdef __cplusplus
}
#endif

#endif
