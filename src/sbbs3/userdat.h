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

#ifdef EXPORT32
#undef EXPORT32
#endif

#ifdef _WIN32
#ifdef SBBS_EXPORTS
#define EXPORT32 __declspec(dllexport)
#else
#define EXPORT32 __declspec(dllimport)
#endif
#else
#define EXPORT32
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern char* crlf;
extern char* nulstr;

EXPORT32 int	getuserdat(scfg_t* cfg, user_t* user); 	/* Fill userdat struct with user data   */
EXPORT32 int	putuserdat(scfg_t* cfg, user_t* user);	/* Put userdat struct into user file	*/
EXPORT32 void	getrec(char *instr,int start,int length,char *outstr); /* Retrieve a record from a string */
EXPORT32 void	putrec(char *outstr,int start,int length,char *instr); /* Place a record into a string */
EXPORT32 uint	matchuser(scfg_t* cfg, char *str); /* Checks for a username match */
EXPORT32 uint	lastuser(scfg_t* cfg);
EXPORT32 char	getage(scfg_t* cfg, char *birthdate);
EXPORT32 char *	username(scfg_t* cfg, int usernumber, char * str);
EXPORT32 int	getnodedat(scfg_t* cfg, uint number, node_t *node, char lockit);
EXPORT32 int	putnodedat(scfg_t* cfg, uint number, node_t *node);
EXPORT32 uint	userdatdupe(scfg_t* cfg, uint usernumber, uint offset, uint datlen, char *dat
					,BOOL del);

EXPORT32 BOOL	chk_ar(scfg_t* cfg, uchar* str, user_t* user); /* checks access requirements */

EXPORT32 int	getuserrec(scfg_t*, int usernumber, int start, int length, char *str);
EXPORT32 int	putuserrec(scfg_t*, int usernumber, int start, uint length, char *str);
EXPORT32 ulong	adjustuserrec(scfg_t*, int usernumber, int start, int length, long adj);
EXPORT32 void	subtract_cdt(scfg_t*, user_t*, long amt);

#ifdef __cplusplus
}
#endif

#endif