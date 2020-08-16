/* Synchronet user data access routines (exported) */

/* $Id: userdat.h,v 1.77 2020/04/30 18:25:58 rswindell Exp $ */

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

#ifndef _USERDAT_H
#define _USERDAT_H

#include "scfgdefs.h"   /* scfg_t */
#include "dat_rec.h"	/* getrec/putrec prototypes */
#include "client.h"		/* client_t */
#include "link_list.h"

#ifdef DLLEXPORT
#undef DLLEXPORT
#endif

#ifdef _WIN32
	#ifdef __MINGW32__
		#define DLLEXPORT
	#else
		#ifdef SBBS_EXPORTS
			#define DLLEXPORT __declspec(dllexport)
		#else
			#define DLLEXPORT __declspec(dllimport)
		#endif
	#endif
#else
	#define DLLEXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern char* crlf;
extern char* nulstr;

DLLEXPORT int	openuserdat(scfg_t*, BOOL for_modify);
DLLEXPORT int	closeuserdat(int);
DLLEXPORT int	readuserdat(scfg_t*, unsigned user_number, char* userdat, int infile);
DLLEXPORT int	parseuserdat(scfg_t*, char* userdat, user_t*);
DLLEXPORT int	getuserdat(scfg_t*, user_t*); 	/* Fill userdat struct with user data   */
DLLEXPORT int	fgetuserdat(scfg_t*, user_t*, int file);
DLLEXPORT int	putuserdat(scfg_t*, user_t*);	/* Put userdat struct into user file	*/
DLLEXPORT int	newuserdat(scfg_t*, user_t*);	/* Create new userdat in user file */
DLLEXPORT uint	matchuser(scfg_t*, const char *str, BOOL sysop_alias); /* Checks for a username match */
DLLEXPORT char* alias(scfg_t*, const char* name, char* buf);
DLLEXPORT int	putusername(scfg_t*, int number, char * name);
DLLEXPORT uint	total_users(scfg_t*);
DLLEXPORT uint	lastuser(scfg_t*);
DLLEXPORT BOOL	del_lastuser(scfg_t*);
DLLEXPORT uint	getage(scfg_t*, char *birthdate);
DLLEXPORT char*	username(scfg_t*, int usernumber, char * str);
DLLEXPORT char* usermailaddr(scfg_t*, char* addr, const char* name);
DLLEXPORT int	opennodedat(scfg_t*);
DLLEXPORT int	opennodeext(scfg_t*);
DLLEXPORT int	getnodedat(scfg_t*, uint number, node_t *node, BOOL lockit, int* file);
DLLEXPORT int	putnodedat(scfg_t*, uint number, node_t *node, BOOL closeit, int file);
DLLEXPORT char* nodestatus(scfg_t*, node_t* node, char* buf, size_t buflen, int num);
DLLEXPORT void	printnodedat(scfg_t*, uint number, node_t* node);
DLLEXPORT int	is_user_online(scfg_t*, uint usernumber);
DLLEXPORT void	packchatpass(char *pass, node_t* node);
DLLEXPORT char* unpackchatpass(char *pass, node_t* node);
DLLEXPORT char* getsmsg(scfg_t*, int usernumber);
DLLEXPORT int	putsmsg(scfg_t*, int usernumber, char *strin);
DLLEXPORT char* getnmsg(scfg_t*, int node_num);
DLLEXPORT int	putnmsg(scfg_t*, int num, char *strin);

DLLEXPORT uint	userdatdupe(scfg_t*, uint usernumber, uint offset, uint datlen, char *dat
					,BOOL del, BOOL next, void (*progress)(void*, int, int), void* cbdata);

DLLEXPORT BOOL	chk_ar(scfg_t*, uchar* str, user_t*, client_t*); /* checks access requirements */

DLLEXPORT int	getuserrec(scfg_t*, int usernumber, int start, int length, char *str);
DLLEXPORT int	putuserrec(scfg_t*, int usernumber, int start, uint length, const char *str);
DLLEXPORT ulong	adjustuserrec(scfg_t*, int usernumber, int start, int length, long adj);
DLLEXPORT BOOL	logoutuserdat(scfg_t*, user_t*, time_t now, time_t logontime);
DLLEXPORT void	resetdailyuserdat(scfg_t*, user_t*, BOOL write);
DLLEXPORT void	subtract_cdt(scfg_t*, user_t*, long amt);
DLLEXPORT int	user_rec_len(int offset);
DLLEXPORT BOOL	can_user_access_sub(scfg_t*, uint subnum, user_t*, client_t* client);
DLLEXPORT BOOL	can_user_read_sub(scfg_t*, uint subnum, user_t*, client_t* client);
DLLEXPORT BOOL	can_user_post(scfg_t*, uint subnum, user_t*, client_t* client, uint* reason);
DLLEXPORT BOOL	can_user_send_mail(scfg_t*, enum smb_net_type, uint usernumber, user_t*, uint* reason);
DLLEXPORT BOOL	is_user_subop(scfg_t*, uint subnum, user_t*, client_t* client);
DLLEXPORT BOOL	is_download_free(scfg_t*, uint dirnum, user_t*, client_t* client);
DLLEXPORT BOOL	is_host_exempt(scfg_t*, const char* ip_addr, const char* host_name);
DLLEXPORT BOOL	filter_ip(scfg_t*, const char* prot, const char* reason, const char* host
								  ,const char* ip_addr, const char* username, const char* fname);

/* user .ini file access */
DLLEXPORT BOOL	user_get_property(scfg_t*, unsigned user_number, const char* section, const char* key, char* value, size_t maxlen);
DLLEXPORT BOOL	user_set_property(scfg_t*, unsigned user_number, const char* section, const char* key, const char* value);
DLLEXPORT BOOL	user_set_time_property(scfg_t*, unsigned user_number, const char* section, const char* key, time_t);

/* New-message-scan pointer functions: */
DLLEXPORT BOOL	getmsgptrs(scfg_t*, user_t*, subscan_t*, void (*progress)(void*, int, int), void* cbdata);
DLLEXPORT BOOL	putmsgptrs(scfg_t*, user_t*, subscan_t*);
DLLEXPORT BOOL	fixmsgptrs(scfg_t*, subscan_t*);
DLLEXPORT BOOL	initmsgptrs(scfg_t*, subscan_t*, unsigned days, void (*progress)(void*, int, int), void* cbdata);


/* New atomic numeric user field adjustment functions: */
DLLEXPORT BOOL	user_posted_msg(scfg_t*, user_t*, int count);
DLLEXPORT BOOL	user_sent_email(scfg_t*, user_t*, int count, BOOL feedback);
DLLEXPORT BOOL	user_downloaded(scfg_t*, user_t*, int files, long bytes);
DLLEXPORT BOOL	user_downloaded_file(scfg_t*, user_t*, client_t*, uint dirnum, const char* filename, ulong bytes);

DLLEXPORT BOOL	user_uploaded(scfg_t*, user_t*, int files, long bytes);
DLLEXPORT BOOL	user_adjust_credits(scfg_t*, user_t*, long amount);
DLLEXPORT BOOL	user_adjust_minutes(scfg_t*, user_t*, long amount);

DLLEXPORT time_t gettimeleft(scfg_t*, user_t*, time_t starttime);

DLLEXPORT BOOL	check_name(scfg_t*, const char* name);
DLLEXPORT BOOL	sysop_available(scfg_t*);
DLLEXPORT BOOL	set_sysop_availability(scfg_t*, BOOL available);

DLLEXPORT int	lookup_user(scfg_t*, link_list_t*, const char* name);

/* Login attempt/hack tracking */
typedef struct {
	union xp_sockaddr addr;	/* host with consecutive failed login attempts */
	ulong		count;	/* number of consecutive failed login attempts */
	ulong		dupes;	/* number of consecutive duplicate login attempts (same name and password) */
	time32_t	time;	/* time of last attempt */
	char		prot[32];	/* protocol used in last attempt */
	char		user[128];
	char		pass[128];
} login_attempt_t;

DLLEXPORT link_list_t*		loginAttemptListInit(link_list_t*);
DLLEXPORT BOOL				loginAttemptListFree(link_list_t*);
DLLEXPORT long				loginAttemptListCount(link_list_t*);
DLLEXPORT long				loginAttemptListClear(link_list_t*);
DLLEXPORT long				loginAttempts(link_list_t*, const union xp_sockaddr*);
DLLEXPORT void				loginSuccess(link_list_t*, const union xp_sockaddr*);
DLLEXPORT ulong				loginFailure(link_list_t*, const union xp_sockaddr*, const char* prot, const char* user, const char* pass);
DLLEXPORT ulong				loginBanned(scfg_t*, link_list_t*, SOCKET, const char* host_name, struct login_attempt_settings, login_attempt_t*);

#ifdef __cplusplus
}
#endif

#endif
