/* userdat.h */

/* Synchronet user data access routines (exported) */

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

#ifndef _USERDAT_H
#define _USERDAT_H

#include "scfgdefs.h"   /* scfg_t */
#include "dat_rec.h"	/* getrec/putrec prototypes */

#ifdef DLLEXPORT
#undef DLLEXPORT
#endif
#ifdef DLLCALL
#undef DLLCALL
#endif

#ifdef _WIN32
	#ifdef SBBS_EXPORTS
		#define DLLEXPORT __declspec(dllexport)
	#else
		#define DLLEXPORT __declspec(dllimport)
	#endif
	#ifdef __BORLANDC__
		#define DLLCALL __stdcall
	#else
		#define DLLCALL
	#endif
#else
	#define DLLEXPORT
	#define DLLCALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern char* crlf;
extern char* nulstr;

DLLEXPORT int	DLLCALL getuserdat(scfg_t* cfg, user_t* user); 	/* Fill userdat struct with user data   */
DLLEXPORT int	DLLCALL putuserdat(scfg_t* cfg, user_t* user);	/* Put userdat struct into user file	*/
DLLEXPORT int	DLLCALL newuserdat(scfg_t* cfg, user_t* user);	/* Create new userdat in user file */
DLLEXPORT uint	DLLCALL matchuser(scfg_t* cfg, char *str, BOOL sysop_alias); /* Checks for a username match */
DLLEXPORT char* DLLCALL alias(scfg_t* cfg, char* name, char* buf);
DLLEXPORT int	DLLCALL putusername(scfg_t* cfg, int number, char * name);
DLLEXPORT uint	DLLCALL lastuser(scfg_t* cfg);
DLLEXPORT BOOL	DLLCALL del_lastuser(scfg_t* cfg);
DLLEXPORT char	DLLCALL getage(scfg_t* cfg, char *birthdate);
DLLEXPORT char*	DLLCALL username(scfg_t* cfg, int usernumber, char * str);
DLLEXPORT char* DLLCALL usermailaddr(scfg_t* cfg, char* addr, char* name);
DLLEXPORT int	DLLCALL getnodedat(scfg_t* cfg, uint number, node_t *node, int* file);
DLLEXPORT int	DLLCALL putnodedat(scfg_t* cfg, uint number, node_t *node, int file);
DLLEXPORT void	DLLCALL printnodedat(scfg_t* cfg, uint number, node_t* node);
DLLEXPORT void	DLLCALL packchatpass(char *pass, node_t* node);
DLLEXPORT char* DLLCALL unpackchatpass(char *pass, node_t* node);

DLLEXPORT int	DLLCALL putsmsg(scfg_t* cfg, int usernumber, char *strin);
DLLEXPORT int	DLLCALL putnmsg(scfg_t* cfg, int num, char *strin);

DLLEXPORT uint	DLLCALL userdatdupe(scfg_t* cfg, uint usernumber, uint offset, uint datlen, char *dat
							,BOOL del);

DLLEXPORT BOOL	DLLCALL chk_ar(scfg_t* cfg, uchar* str, user_t* user); /* checks access requirements */

DLLEXPORT int	DLLCALL getuserrec(scfg_t*, int usernumber, int start, int length, char *str);
DLLEXPORT int	DLLCALL putuserrec(scfg_t*, int usernumber, int start, uint length, char *str);
DLLEXPORT ulong	DLLCALL adjustuserrec(scfg_t*, int usernumber, int start, int length, long adj);
DLLEXPORT BOOL	DLLCALL logoutuserdat(scfg_t*, user_t*, time_t now, time_t logontime);
DLLEXPORT void	DLLCALL resetdailyuserdat(scfg_t*, user_t*);
DLLEXPORT void	DLLCALL subtract_cdt(scfg_t*, user_t*, long amt);
DLLEXPORT int	DLLCALL user_rec_len(int offset);

#ifdef __cplusplus
}
#endif

#endif
