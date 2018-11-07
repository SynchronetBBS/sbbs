/* Synchronet user data access routines (exported) */

/* $Id$ */

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
			#define DLLCALL __stdcall
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

extern char* crlf;
extern char* nulstr;

DLLEXPORT int	DLLCALL openuserdat(scfg_t*, BOOL for_modify);
DLLEXPORT int	DLLCALL closeuserdat(int);
DLLEXPORT int	DLLCALL readuserdat(scfg_t*, unsigned user_number, char* userdat, int infile);
DLLEXPORT int	DLLCALL parseuserdat(scfg_t*, char* userdat, user_t*);
DLLEXPORT int	DLLCALL getuserdat(scfg_t*, user_t*); 	/* Fill userdat struct with user data   */
DLLEXPORT int	DLLCALL fgetuserdat(scfg_t*, user_t*, int file);
DLLEXPORT int	DLLCALL putuserdat(scfg_t*, user_t*);	/* Put userdat struct into user file	*/
DLLEXPORT int	DLLCALL newuserdat(scfg_t*, user_t*);	/* Create new userdat in user file */
DLLEXPORT uint	DLLCALL matchuser(scfg_t*, const char *str, BOOL sysop_alias); /* Checks for a username match */
DLLEXPORT char* DLLCALL alias(scfg_t*, const char* name, char* buf);
DLLEXPORT int	DLLCALL putusername(scfg_t*, int number, char * name);
DLLEXPORT uint	DLLCALL total_users(scfg_t*);
DLLEXPORT uint	DLLCALL lastuser(scfg_t*);
DLLEXPORT BOOL	DLLCALL del_lastuser(scfg_t*);
DLLEXPORT uint	DLLCALL getage(scfg_t*, char *birthdate);
DLLEXPORT char*	DLLCALL username(scfg_t*, int usernumber, char * str);
DLLEXPORT char* DLLCALL usermailaddr(scfg_t*, char* addr, const char* name);
DLLEXPORT int	DLLCALL getnodedat(scfg_t*, uint number, node_t *node, int* file);
DLLEXPORT int	DLLCALL putnodedat(scfg_t*, uint number, node_t *node, int file);
DLLEXPORT char* DLLCALL nodestatus(scfg_t*, node_t* node, char* buf, size_t buflen);
DLLEXPORT void	DLLCALL printnodedat(scfg_t*, uint number, node_t* node);
DLLEXPORT int	DLLCALL is_user_online(scfg_t*, uint usernumber);
DLLEXPORT void	DLLCALL packchatpass(char *pass, node_t* node);
DLLEXPORT char* DLLCALL unpackchatpass(char *pass, node_t* node);
DLLEXPORT char* DLLCALL getsmsg(scfg_t*, int usernumber);
DLLEXPORT int	DLLCALL putsmsg(scfg_t*, int usernumber, char *strin);
DLLEXPORT char* DLLCALL getnmsg(scfg_t*, int node_num);
DLLEXPORT int	DLLCALL putnmsg(scfg_t*, int num, char *strin);

DLLEXPORT uint	DLLCALL userdatdupe(scfg_t*, uint usernumber, uint offset, uint datlen, char *dat
							,BOOL del, BOOL next, void (*progress)(void*, int, int), void* cbdata);

DLLEXPORT BOOL	DLLCALL chk_ar(scfg_t*, uchar* str, user_t*, client_t*); /* checks access requirements */

DLLEXPORT int	DLLCALL getuserrec(scfg_t*, int usernumber, int start, int length, char *str);
DLLEXPORT int	DLLCALL putuserrec(scfg_t*, int usernumber, int start, uint length, const char *str);
DLLEXPORT ulong	DLLCALL adjustuserrec(scfg_t*, int usernumber, int start, int length, long adj);
DLLEXPORT BOOL	DLLCALL logoutuserdat(scfg_t*, user_t*, time_t now, time_t logontime);
DLLEXPORT void	DLLCALL resetdailyuserdat(scfg_t*, user_t*, BOOL write);
DLLEXPORT void	DLLCALL subtract_cdt(scfg_t*, user_t*, long amt);
DLLEXPORT int	DLLCALL user_rec_len(int offset);
DLLEXPORT BOOL	DLLCALL can_user_access_sub(scfg_t*, uint subnum, user_t*, client_t* client);
DLLEXPORT BOOL	DLLCALL can_user_read_sub(scfg_t*, uint subnum, user_t*, client_t* client);
DLLEXPORT BOOL	DLLCALL can_user_post(scfg_t*, uint subnum, user_t*, client_t* client, uint* reason);
DLLEXPORT BOOL	DLLCALL can_user_send_mail(scfg_t*, enum smb_net_type, uint usernumber, user_t*, uint* reason);
DLLEXPORT BOOL	DLLCALL is_user_subop(scfg_t*, uint subnum, user_t*, client_t* client);
DLLEXPORT BOOL	DLLCALL is_download_free(scfg_t*, uint dirnum, user_t*, client_t* client);
DLLEXPORT BOOL	DLLCALL is_host_exempt(scfg_t*, const char* ip_addr, const char* host_name);
DLLEXPORT BOOL	DLLCALL filter_ip(scfg_t*, const char* prot, const char* reason, const char* host
								  ,const char* ip_addr, const char* username, const char* fname);

/* user .ini file access */
DLLEXPORT BOOL DLLCALL user_get_property(scfg_t*, unsigned user_number, const char* section, const char* key, char* value, size_t maxlen);
DLLEXPORT BOOL DLLCALL user_set_property(scfg_t*, unsigned user_number, const char* section, const char* key, const char* value);
DLLEXPORT BOOL DLLCALL user_set_time_property(scfg_t*, unsigned user_number, const char* section, const char* key, time_t);

/* New-message-scan pointer functions: */
DLLEXPORT BOOL	DLLCALL getmsgptrs(scfg_t*, user_t*, subscan_t*, void (*progress)(void*, int, int), void* cbdata);
DLLEXPORT BOOL	DLLCALL putmsgptrs(scfg_t*, user_t*, subscan_t*);
DLLEXPORT BOOL	DLLCALL fixmsgptrs(scfg_t*, subscan_t*);
DLLEXPORT BOOL	DLLCALL initmsgptrs(scfg_t*, subscan_t*, unsigned days, void (*progress)(void*, int, int), void* cbdata);


/* New atomic numeric user field adjustment functions: */
DLLEXPORT BOOL	DLLCALL user_posted_msg(scfg_t*, user_t*, int count);
DLLEXPORT BOOL	DLLCALL user_sent_email(scfg_t*, user_t*, int count, BOOL feedback);
DLLEXPORT BOOL	DLLCALL user_downloaded(scfg_t*, user_t*, int files, long bytes);
DLLEXPORT BOOL	DLLCALL user_uploaded(scfg_t*, user_t*, int files, long bytes);
DLLEXPORT BOOL	DLLCALL user_adjust_credits(scfg_t*, user_t*, long amount);
DLLEXPORT BOOL	DLLCALL user_adjust_minutes(scfg_t*, user_t*, long amount);

DLLEXPORT time_t DLLCALL gettimeleft(scfg_t*, user_t*, time_t starttime);

DLLEXPORT BOOL	DLLCALL check_name(scfg_t*, const char* name);
DLLEXPORT BOOL	DLLCALL sysop_available(scfg_t*);
DLLEXPORT BOOL	DLLCALL set_sysop_availability(scfg_t*, BOOL available);

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

DLLEXPORT link_list_t*		DLLCALL	loginAttemptListInit(link_list_t*);
DLLEXPORT BOOL				DLLCALL	loginAttemptListFree(link_list_t*);
DLLEXPORT long				DLLCALL	loginAttemptListClear(link_list_t*);
DLLEXPORT long				DLLCALL loginAttempts(link_list_t*, const union xp_sockaddr*);
DLLEXPORT void				DLLCALL	loginSuccess(link_list_t*, const union xp_sockaddr*);
DLLEXPORT ulong				DLLCALL loginFailure(link_list_t*, const union xp_sockaddr*, const char* prot, const char* user, const char* pass);
DLLEXPORT ulong				DLLCALL loginBanned(scfg_t*, link_list_t*, SOCKET, const char* host_name, struct login_attempt_settings, login_attempt_t*);

#ifdef __cplusplus
}
#endif

#endif
