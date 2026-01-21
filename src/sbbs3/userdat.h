/* Synchronet user data access routines (exported) */

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

#ifndef _USERDAT_H
#define _USERDAT_H

#include <stdio.h>      // FILE

#include "scfgdefs.h"   /* scfg_t */
#include "client.h"     /* client_t */
#include "link_list.h"
#include "startup.h"    /* struct login_attempt_settings */
#include "dllexport.h"
#include "userfields.h"

#define USER_DATA_FILENAME      "user.tab"
#define USER_RECORD_LINE_LEN    1000                    // includes LF terminator
#define USER_RECORD_LEN         (USER_RECORD_LINE_LEN - 1)  // does not include LF
#define USER_INDEX_FILENAME     "name.dat"
#define USER_INDEX_RECORD_LEN   (LEN_ALIAS + 2)

// API function return values
#define USER_SUCCESS            0
#define USER_INVALID_ARG        -100
#define USER_INVALID_NUM        -101
#define USER_OPEN_ERROR         -102
#define USER_SEEK_ERROR         -103
#define USER_LOCK_ERROR         -104
#define USER_READ_ERROR         -105
#define USER_FORMAT_ERROR       -106
#define USER_WRITE_ERROR        -107
#define USER_TRUNC_ERROR        -108
#define USER_SIZE_ERROR         -109

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT char* userdat_filename(scfg_t*, char*, size_t);
DLLEXPORT char* msgptrs_filename(scfg_t*, unsigned user_number, char*, size_t);
DLLEXPORT int   openuserdat(scfg_t*, bool for_modify);
DLLEXPORT bool  seekuserdat(int file, unsigned user_number);
DLLEXPORT int   closeuserdat(int file);
DLLEXPORT int   readuserdat(scfg_t*, unsigned user_number, char* userdat, size_t, int file, bool leave_locked);
DLLEXPORT int   parseuserdat(scfg_t*, char* userdat, user_t*, char* fields[]);
DLLEXPORT int   getuserdat(scfg_t*, user_t*);   // Fill user_t with user data
DLLEXPORT int   fgetuserdat(scfg_t*, user_t*, int file);
DLLEXPORT int   fputuserdat(scfg_t*, user_t*, int file);
DLLEXPORT bool  format_userdat(scfg_t*, user_t*, char userdat[]);
DLLEXPORT bool  lockuserdat(int file, unsigned user_number);
DLLEXPORT bool  unlockuserdat(int file, unsigned user_number);
DLLEXPORT int   putuserdat(scfg_t*, user_t*);   // Put user_t into user file
DLLEXPORT int   newuserdat(scfg_t*, user_t*);   // Create new user in user file
DLLEXPORT void  newuserdefaults(scfg_t*, user_t*);
DLLEXPORT void  newsysop(scfg_t*, user_t*);
DLLEXPORT uint  matchuser(scfg_t*, const char *str, bool sysop_alias); // Checks for a username match
DLLEXPORT bool  matchusername(scfg_t*, const char* name, const char* compare);
DLLEXPORT char* alias(scfg_t*, const char* name, char* buf);
DLLEXPORT int   putusername(scfg_t*, int number, const char* name);
DLLEXPORT int   total_users(scfg_t*);
DLLEXPORT int   lastuser(scfg_t*);
DLLEXPORT bool  del_lastuser(scfg_t*);
DLLEXPORT int	del_user(scfg_t*, user_t*);
DLLEXPORT int	undel_user(scfg_t*, user_t*);
DLLEXPORT int   getage(scfg_t*, const char* birthdate);
DLLEXPORT int   getbirthmonth(scfg_t*, const char* birthdate);
DLLEXPORT int   getbirthday(scfg_t*, const char* birthdate);
DLLEXPORT int   getbirthyear(scfg_t*, const char* birthdate);
DLLEXPORT char* getbirthdstr(scfg_t*, const char* birthdate, char* buf, size_t);
DLLEXPORT char* getbirthmmddyy(scfg_t*, char sep, const char* birthdate, char* buf, size_t);
DLLEXPORT char* getbirthddmmyy(scfg_t*, char sep, const char* birthdate, char* buf, size_t);
DLLEXPORT char* getbirthyymmdd(scfg_t*, char sep, const char* birthdate, char* buf, size_t);
DLLEXPORT char* parse_birthdate(scfg_t*, const char* birthdate, char* out, size_t);
DLLEXPORT char* format_birthdate(scfg_t*, const char* birthdate, char* out, size_t);
DLLEXPORT char* birthdate_format(scfg_t*, char* buf, size_t);
DLLEXPORT char* birthdate_template(scfg_t*, char* buf, size_t);
DLLEXPORT bool  birthdate_is_valid(scfg_t*, const char* birthdate);
DLLEXPORT char* username(scfg_t*, int usernumber, char * str);
DLLEXPORT char* usermailaddr(scfg_t*, char* addr, const char* name);
DLLEXPORT void  smtp_netmailaddr(scfg_t*, smbmsg_t*, char* name, size_t namelen, char* addr, size_t addrlen);
DLLEXPORT int   opennodedat(scfg_t*);
DLLEXPORT off_t nodedatoffset(unsigned node_number);
DLLEXPORT bool  seeknodedat(int file, unsigned node_number);
DLLEXPORT int   opennodeext(scfg_t*);
DLLEXPORT int   getnodedat(scfg_t*, uint number, node_t *node, bool lockit, int* file);
DLLEXPORT int   putnodedat(scfg_t*, uint number, node_t *node, bool closeit, int file);
DLLEXPORT char* node_activity(scfg_t*, node_t* node, char* str, size_t size, int num);
DLLEXPORT char* node_vstatus(scfg_t*, node_t* node, char* str, size_t size);
DLLEXPORT char* nodestatus(scfg_t*, node_t* node, char* buf, size_t buflen, int num);
DLLEXPORT void  printnodedat(scfg_t*, uint number, node_t* node);
DLLEXPORT int   user_is_online(scfg_t*, uint usernumber);
DLLEXPORT void  packchatpass(char *pass, node_t* node);
DLLEXPORT char* unpackchatpass(char *pass, node_t* node);
DLLEXPORT char* readsmsg(scfg_t*, int usernumber);
DLLEXPORT char* getsmsg(scfg_t*, int usernumber);
DLLEXPORT int   putsmsg(scfg_t*, int usernumber, char *strin);
DLLEXPORT char* getnmsg(scfg_t*, int node_num);
DLLEXPORT int   putnmsg(scfg_t*, int num, char *strin);
DLLEXPORT int   getnodeclient(scfg_t*, uint number, client_t*, time_t*);
DLLEXPORT bool  set_node_lock(scfg_t*, int node_num, bool);
DLLEXPORT bool  set_node_interrupt(scfg_t*, int node_num, bool);
DLLEXPORT bool  set_node_down(scfg_t*, int node_num, bool);
DLLEXPORT bool  set_node_rerun(scfg_t*, int node_num, bool);
DLLEXPORT bool  set_node_status(scfg_t*, int node_num, enum node_status);
DLLEXPORT bool  set_node_misc(scfg_t*, int node_num, uint);
DLLEXPORT bool  set_node_errors(scfg_t*, int node_num, uint);
DLLEXPORT bool  xtrn_is_running(scfg_t*, int xtrn_num);

DLLEXPORT uint  finduserstr(scfg_t*, uint usernumber, enum user_field, const char *str
                            , bool del, bool next, void (*progress)(void*, int, int), void* cbdata);

DLLEXPORT uint  find_login_id(scfg_t*, const char* user_id);

DLLEXPORT bool  chk_ar(scfg_t*, uchar* str, user_t*, client_t*); /* checks access requirements */
DLLEXPORT bool  chk_ars(scfg_t*, char* str, user_t*, client_t*);

DLLEXPORT uint32_t getusermisc(scfg_t*, int usernumber);
DLLEXPORT uint32_t getuserchat(scfg_t*, int usernumber);
DLLEXPORT uint32_t getusermail(scfg_t*, int usernumber);
DLLEXPORT uint32_t getuserqwk(scfg_t*, int usernumber);
DLLEXPORT uint32_t getuserflags(scfg_t*, int usernumber, enum user_field);
DLLEXPORT uint32_t getuserhex32(scfg_t*, int usernumber, enum user_field);
DLLEXPORT uint32_t getuserdec32(scfg_t*, int usernumber, enum user_field);
DLLEXPORT uint64_t getuserdec64(scfg_t*, int usernumber, enum user_field);
DLLEXPORT time32_t getuserdatetime(scfg_t*, int usernumber, enum user_field);
DLLEXPORT char* getuserstr(scfg_t*, int usernumber, enum user_field, char *str, size_t);
DLLEXPORT int   putuserstr(scfg_t*, int usernumber, enum user_field, const char *str);
DLLEXPORT int   putuserdatetime(scfg_t*, int usernumber, enum user_field, time32_t t);
DLLEXPORT int   putuserflags(scfg_t*, int usernumber, enum user_field, uint32_t flags);
DLLEXPORT int   putuserhex32(scfg_t*, int usernumber, enum user_field, uint32_t value);
DLLEXPORT int   putuserdec32(scfg_t*, int usernumber, enum user_field, uint32_t value);
DLLEXPORT int   putuserdec64(scfg_t*, int usernumber, enum user_field, uint64_t value);
DLLEXPORT int   putusermisc(scfg_t*, int usernumber, uint32_t value);
DLLEXPORT int   putuserchat(scfg_t*, int usernumber, uint32_t value);
DLLEXPORT int   putusermail(scfg_t*, int usernumber, uint32_t value);
DLLEXPORT int   putuserqwk(scfg_t*, int usernumber, uint32_t value);
DLLEXPORT uint64_t adjustuserval(scfg_t*, user_t*, enum user_field, int64_t value);
DLLEXPORT bool  writeuserfields(scfg_t*, char* field[], int file);
DLLEXPORT int   loginuserdat(scfg_t*, user_t*, client_t* client, bool use_prot, char* save_ars);
DLLEXPORT int   logoutuserdat(scfg_t*, user_t*, time_t logontime);
DLLEXPORT void  resetdailyuserdat(scfg_t*, user_t*, bool write);
DLLEXPORT void  subtract_cdt(scfg_t*, user_t*, uint64_t amt);
DLLEXPORT uint64_t user_available_credits(user_t*);
DLLEXPORT size_t user_field_len(enum user_field);
DLLEXPORT bool  user_can_access_all_libs(scfg_t*, user_t*, client_t*);
DLLEXPORT bool  user_can_access_all_dirs(scfg_t*, int libnum, user_t*, client_t*);
DLLEXPORT bool  user_can_access_lib(scfg_t*, int libnum, user_t*, client_t*);
DLLEXPORT bool  user_can_access_dir(scfg_t*, int dirnum, user_t*, client_t* client);
DLLEXPORT bool  user_can_access_grp(scfg_t*, int subnum, user_t*, client_t* client);
DLLEXPORT bool  user_can_access_sub(scfg_t*, int subnum, user_t*, client_t* client);
DLLEXPORT bool  user_can_read_sub(scfg_t*, int subnum, user_t*, client_t* client);
DLLEXPORT bool  user_can_post(scfg_t*, int subnum, user_t*, client_t* client, uint* reason);
DLLEXPORT bool  user_can_upload(scfg_t*, int dirnum, user_t*, client_t* client, uint* reason);
DLLEXPORT bool  user_can_download(scfg_t*, int dirnum, user_t*, client_t* client, uint* reason);
DLLEXPORT bool  user_can_send_mail(scfg_t*, enum smb_net_type, uint usernumber, user_t*, uint* reason);
DLLEXPORT bool  user_is_nobody(user_t*);
DLLEXPORT bool  user_is_guest(user_t*);
DLLEXPORT bool  user_is_sysop(user_t*);
DLLEXPORT bool  user_is_subop(scfg_t*, int subnum, user_t*, client_t* client);
DLLEXPORT bool  user_is_dirop(scfg_t*, int dirnum, user_t*, client_t* client);
DLLEXPORT uint  user_downloads_per_day(scfg_t*, user_t*);
DLLEXPORT bool  download_is_free(scfg_t*, int dirnum, user_t*, client_t* client);

enum parsed_vpath {
	PARSED_VPATH_NONE,
	PARSED_VPATH_ROOT,
	PARSED_VPATH_LIB,
	PARSED_VPATH_DIR,
	PARSED_VPATH_FULL
};
DLLEXPORT enum parsed_vpath parse_vpath(scfg_t*, const char* vpath, int* libnum, int* dirnum, char** filename);

/* user .ini file access */
DLLEXPORT bool  user_get_property(scfg_t*, unsigned user_number, const char* section, const char* key, char* value, size_t maxlen);
DLLEXPORT bool  user_set_property(scfg_t*, unsigned user_number, const char* section, const char* key, const char* value);
DLLEXPORT bool  user_set_time_property(scfg_t*, unsigned user_number, const char* section, const char* key, time_t);
DLLEXPORT bool  user_get_bool_property(scfg_t*, unsigned user_number, const char* section, const char* key, bool dflt);
DLLEXPORT bool  user_set_bool_property(scfg_t*, unsigned user_number, const char* section, const char* key, bool value);

/* New-message-scan pointer functions: */
DLLEXPORT bool  newmsgs(smb_t*, time_t);
DLLEXPORT bool  getmsgptrs(scfg_t*, user_t*, subscan_t*, void (*progress)(void*, int, int), void* cbdata);
DLLEXPORT bool  putmsgptrs(scfg_t*, user_t*, subscan_t*);
DLLEXPORT bool  putmsgptrs_fp(scfg_t*, user_t*, subscan_t*, FILE*);
DLLEXPORT bool  fixmsgptrs(scfg_t*, subscan_t*);
DLLEXPORT bool  initmsgptrs(scfg_t*, subscan_t*, unsigned days, void (*progress)(void*, int, int), void* cbdata);

/* New atomic numeric user field adjustment functions: */
DLLEXPORT bool  user_posted_msg(scfg_t*, user_t*, int count);
DLLEXPORT bool  user_sent_email(scfg_t*, user_t*, int count, bool feedback);
DLLEXPORT bool  user_downloaded(scfg_t*, user_t*, int files, off_t bytes);
DLLEXPORT bool  user_downloaded_file(scfg_t*, user_t*, client_t*, int dirnum, const char* filename, off_t bytes);

DLLEXPORT bool  user_uploaded(scfg_t*, user_t*, int files, off_t bytes);
DLLEXPORT bool  user_adjust_credits(scfg_t*, user_t*, int64_t amount);
DLLEXPORT bool  user_adjust_minutes(scfg_t*, user_t*, long amount);

DLLEXPORT time_t gettimeleft(scfg_t*, user_t*, time_t starttime);

DLLEXPORT bool  check_pass(scfg_t*, const char *passwd, user_t* user, bool unique, int* reason);
DLLEXPORT bool  check_name(scfg_t*, const char* name, bool unique);
DLLEXPORT bool  check_realname(scfg_t*, const char* name);
DLLEXPORT bool  sysop_available(scfg_t*);
DLLEXPORT bool  set_sysop_availability(scfg_t*, bool available);
DLLEXPORT bool  sound_muted(scfg_t*);
DLLEXPORT bool  set_sound_muted(scfg_t*, bool muted);

DLLEXPORT int   lookup_user(scfg_t*, link_list_t*, const char* name);

/* Login attempt/hack tracking */
typedef struct {
	union xp_sockaddr addr; /* host with consecutive failed login attempts */
	ulong count;        /* number of consecutive failed login attempts */
	ulong dupes;        /* number of consecutive duplicate login attempts (same name and password) */
	time32_t first;     /* time of first attempt */
	time32_t time;      /* time of last attempt */
	char prot[32];          /* protocol used in last attempt */
	char user[128];
	char pass[128];
} login_attempt_t;

DLLEXPORT link_list_t*      loginAttemptListInit(link_list_t*);
DLLEXPORT bool              loginAttemptListFree(link_list_t*);
DLLEXPORT long              loginAttemptListCount(link_list_t*);
DLLEXPORT long              loginAttemptListClear(link_list_t*);
DLLEXPORT long              loginAttempts(link_list_t*, const union xp_sockaddr*);
DLLEXPORT void              loginSuccess(link_list_t*, const union xp_sockaddr*);
DLLEXPORT ulong             loginFailure(link_list_t*, const union xp_sockaddr*, const char* prot, const char* user, const char* pass, login_attempt_t*);
DLLEXPORT ulong loginBanned(scfg_t*, link_list_t*, SOCKET, const char* host_name, struct login_attempt_settings, login_attempt_t*);

#ifdef __cplusplus
}
#endif

#endif
