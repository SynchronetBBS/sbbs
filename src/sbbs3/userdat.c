/* Synchronet user data-related routines (exported) */

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

#include "str_util.h"
#include "findstr.h"
#include "cmdshell.h"
#include "userdat.h"
#include "filedat.h"
#include "ars_defs.h"
#include "text.h"
#include "trash.h"
#include "nopen.h"
#include "datewrap.h"
#include "date_str.h"
#include "smblib.h"
#include "getstats.h"
#include "msgdate.h"
#include "scfglib.h"
#include "xpdatetime.h"
#include "dat_rec.h"

#ifndef USHRT_MAX
	#define USHRT_MAX ((unsigned short)~0)
#endif

#define VALID_CFG(cfg)	(cfg!=NULL && cfg->size==sizeof(scfg_t))
#define VALID_USER_NUMBER(n) ((n) >= 1)
#define VALID_USER_FIELD(n)	((n) >= 0 && (n) < USER_FIELD_COUNT)

#define USER_FIELD_SEPARATOR '\t'
static const char user_field_separator[2] = { USER_FIELD_SEPARATOR, '\0' };

#define LOOP_USERDAT 50

char* userdat_filename(scfg_t* cfg, char* path, size_t size)
{
	safe_snprintf(path, size, "%suser/" USER_DATA_FILENAME, cfg->data_dir);
	return path;
}

/****************************************************************************/
/****************************************************************************/
void split_userdat(char *userdat, char* field[])
{
	char* p = userdat;
	for(size_t i = 0; i < USER_FIELD_COUNT; i++) {
		field[i] = p;
		FIND_CHAR(p, USER_FIELD_SEPARATOR);
		if(*p != '\0')
			*(p++) = '\0';
	}
}

/****************************************************************************/
/* Looks for a perfect match among all usernames (not deleted users)		*/
/* Makes dots and underscores synonymous with spaces for comparisons		*/
/* Returns the number of the perfect matched username or 0 if no match		*/
/****************************************************************************/
uint matchuser(scfg_t* cfg, const char *name, bool sysop_alias)
{
	int		file,c;
	char	dat[LEN_ALIAS+2];
	char	str[256];
	off_t	l,length;
	FILE*	stream;

	if(!VALID_CFG(cfg) || name==NULL || *name == '\0')
		return(0);

	if(sysop_alias &&
		(!stricmp(name,"SYSOP") || !stricmp(name,"POSTMASTER") || !stricmp(name,cfg->sys_id)))
		return(1);

	SAFEPRINTF(str,"%suser/name.dat",cfg->data_dir);
	if((stream=fnopen(&file,str,O_RDONLY))==NULL)
		return(0);
	length = filelength(file);
	if(length < sizeof(dat)) {
		fclose(stream);
		return 0;
	}
	for(l = 0; l < length; l += sizeof(dat)) {
		if(fread(dat,sizeof(dat),1,stream) != 1)
			break;
		for(c=0;c<LEN_ALIAS;c++)
			if(dat[c]==ETX) break;
		dat[c]=0;
		if(c < 1) // Deleted user
			continue;
		if(matchusername(cfg, dat, name))
			break;
	}
	fclose(stream);
	if(l<length)
		return (uint)((l/(LEN_ALIAS+2))+1);
	return(0);
}

/****************************************************************************/
/* Return true if the user 'name' (or alias) matches with 'comp'			*/
/* ... ignoring non-alpha-numeric chars in either string					*/
/* and terminating the comparison string at an '@'							*/
/****************************************************************************/
bool matchusername(scfg_t* cfg, const char* name, const char* comp)
{
	(void)cfg; // in case we want this matching logic to be configurable in the future

	const char* np = name;
	const char* cp = comp;
	while(*np != '\0' && *cp != '\0' && *cp != '@') {
		if(!IS_ALPHANUMERIC(*np)) {
			np++;
			continue;
		}
		if(!IS_ALPHANUMERIC(*cp)) {
			cp++;
			continue;
		}
		if(toupper(*np) != toupper(*cp))
			break;
		np++, cp++;
	}
	while(*np != '\0' && !IS_ALPHANUMERIC(*np))
		np++;
	while(*cp != '\0' && !IS_ALPHANUMERIC(*cp) && *cp != '@' )
		cp++;
	return *np == '\0' && (*cp == '\0' || *cp == '@');
}

/****************************************************************************/
uint find_login_id(scfg_t* cfg, const char* user_id)
{
	uint usernum;

	if(cfg == NULL || user_id == NULL)
		return 0;

	if((cfg->sys_login & LOGIN_USERNUM) && IS_DIGIT(user_id[0])) {
		char* end = NULL;
		usernum = (uint)strtoul(user_id, &end, 10);
		if(end == NULL || *end != '\0' || usernum > (uint)lastuser(cfg))
			usernum = 0;
		return usernum;
	}

	usernum = matchuser(cfg, user_id, /* sysop_alias: */false);
	if(usernum < 1 && check_realname(cfg, user_id) && (cfg->sys_login & LOGIN_REALNAME))
		usernum = finduserstr(cfg, 0, USER_NAME, user_id
			,/* del: */false, /* next: */false
			,/* Progress_cb: */NULL, /* cbdata: */NULL);
	return usernum;
}

/****************************************************************************/
int total_users(scfg_t* cfg)
{
    int		total_users=0;
	int		file;
	bool	success = true;

	if(!VALID_CFG(cfg))
		return(0);

	if((file=openuserdat(cfg, /* for_modify: */false)) < 0)
		return 0;

	for(int usernumber = 1; success; usernumber++) {
		char userdat[USER_RECORD_LEN + 1];
		if(readuserdat(cfg, usernumber, userdat, sizeof(userdat), file, /* leave_locked: */false) == 0) {
			char* field[USER_FIELD_COUNT];
			split_userdat(userdat, field);
			if(!(ahtou32(field[USER_MISC]) & (DELETED|INACTIVE)))
				total_users++;
		} else
			success = false;
	}

	close(file);
	return total_users;
}

/****************************************************************************/
/* Returns the number of the last user in user.tab (deleted ones too)		*/
/****************************************************************************/
int lastuser(scfg_t* cfg)
{
	char path[MAX_PATH + 1];
	off_t length;

	if(!VALID_CFG(cfg))
		return(0);

	if((length = flength(userdat_filename(cfg, path, sizeof(path)))) > 0)
		return (int)(length / USER_RECORD_LINE_LEN);
	return 0;
}

/****************************************************************************/
/* Deletes (completely removes) last user record in userbase				*/
/****************************************************************************/
bool del_lastuser(scfg_t* cfg)
{
	int		file;
	off_t	length;

	if(!VALID_CFG(cfg))
		return(false);

	if((file=openuserdat(cfg, /* for_modify: */true)) < 0)
		return(false);
	length = filelength(file);
	if(length < USER_RECORD_LINE_LEN) {
		close(file);
		return(false);
	}
	int result = chsize(file, (long)length - USER_RECORD_LINE_LEN);
	close(file);
	return result == 0;
}

/****************************************************************************/
/* Opens the user database returning the file descriptor or -1 on error		*/
/****************************************************************************/
int openuserdat(scfg_t* cfg, bool for_modify)
{
	char path[MAX_PATH+1];

	if(!VALID_CFG(cfg))
		return(-1);

	return nopen(userdat_filename(cfg, path, sizeof(path)), for_modify ? (O_RDWR|O_CREAT|O_DENYNONE) : (O_RDONLY|O_DENYNONE));
}

int closeuserdat(int file)
{
	if(file < 1)
		return -1;
	return close(file);
}

off_t userdatoffset(unsigned user_number)
{
	return (user_number - 1) * USER_RECORD_LINE_LEN;
}

bool seekuserdat(int file, unsigned user_number)
{
	return lseek(file, userdatoffset(user_number), SEEK_SET) == userdatoffset(user_number);
}

bool lockuserdat(int file, unsigned user_number)
{
	if(!VALID_USER_NUMBER(user_number))
		return false;

	off_t offset = userdatoffset(user_number);

	if(lseek(file, offset, SEEK_SET) != offset)
		return false;
	unsigned attempt=0;
	while(attempt < LOOP_USERDAT && lock(file, offset, USER_RECORD_LINE_LEN) == -1) {
		if(attempt)
			mswait(100);
		attempt++;
	}
	return attempt < LOOP_USERDAT;
}

bool unlockuserdat(int file, unsigned user_number)
{
	if(!VALID_USER_NUMBER(user_number))
		return false;

	return unlock(file, userdatoffset(user_number), USER_RECORD_LINE_LEN) == 0;
}

/****************************************************************************/
/* Locks and reads a single user record from an open userbase file into a	*/
/* buffer of USER_RECORD_LINE_LEN in size.									*/
/* Returns 0 on success.													*/
/****************************************************************************/
int readuserdat(scfg_t* cfg, unsigned user_number, char* userdat, size_t size, int infile, bool leave_locked)
{
	int file;

	if(!VALID_CFG(cfg) || !VALID_USER_NUMBER(user_number))
		return -1;

	memset(userdat, 0, size);
	if(infile >= 0)
		file = infile;
	else {
		if((file = openuserdat(cfg, /* for_modify: */false)) < 0)
			return file;
	}

	if(user_number > (unsigned)(filelength(file) / USER_RECORD_LINE_LEN)) {
		if(file != infile)
			close(file);
		return -2;	/* no such user record */
	}

	if(!seekuserdat(file, user_number)) {
		if(file != infile)
			close(file);
		return -3;
	}

	if(!lockuserdat(file, user_number)) {
		if(file != infile)
			close(file);
		return -4;
	}

	if(read(file, userdat, size - 1) != size - 1) {
		unlockuserdat(file, user_number);
		if(file != infile)
			close(file);
		return -5;
	}
	if(!leave_locked)
		unlockuserdat(file, user_number);
	if(file != infile)
		close(file);
	return 0;
}

// Assumes file already positioned at beginning of user record
bool writeuserfields(scfg_t* cfg, char* field[], int file)
{
	char	userdat[USER_RECORD_LINE_LEN + 1] = "";

	for(size_t i = 0; i < USER_FIELD_COUNT; i++) {
		SAFECAT(userdat, field[i]);
		SAFECAT(userdat, user_field_separator);
	}
	size_t len = strlen(userdat);
	memset(userdat + len, USER_FIELD_SEPARATOR, USER_RECORD_LEN - len);
	userdat[USER_RECORD_LINE_LEN - 1] = '\n';
	if(write(file, userdat, USER_RECORD_LINE_LEN) != USER_RECORD_LINE_LEN)
		return false;
	return true;
}

static time32_t parse_usertime(const char* str)
{
	time_t result = xpDateTime_to_time(isoDateTimeStr_parse(str));
	if(result == INVALID_TIME)
		result = 0;
	return (time32_t)result;
}

/****************************************************************************/
/* Fills the structure 'user' with info for user.number	from userdat		*/
/* (a buffer representing a single user 'record' from the userbase file		*/
/****************************************************************************/
int parseuserdat(scfg_t* cfg, char *userdat, user_t *user, char* field[])
{
	unsigned user_number;

	if(user==NULL)
		return(-1);

	user_number=user->number;
	memset(user,0,sizeof(user_t));

	if(!VALID_CFG(cfg) || !VALID_USER_NUMBER(user_number))
		return(-1);

	/* The user number needs to be set here
	   before calling chk_ar() below for user-number comparisons in AR strings to function correctly */
	user->number=user_number;	/* Signal of success */

	char* fbuf[USER_FIELD_COUNT];
	if(field == NULL)
		field = fbuf;
	split_userdat(userdat, field);
	SAFECOPY(user->alias, field[USER_ALIAS]);
	SAFECOPY(user->name, field[USER_NAME]);
	SAFECOPY(user->handle, field[USER_HANDLE]);
	SAFECOPY(user->lang, field[USER_LANG]);
	SAFECOPY(user->note, field[USER_NOTE]);
	SAFECOPY(user->ipaddr, field[USER_IPADDR]);
	SAFECOPY(user->comp, field[USER_HOST]);
	SAFECOPY(user->netmail, field[USER_NETMAIL]);
	SAFECOPY(user->address, field[USER_ADDRESS]);
	SAFECOPY(user->location, field[USER_LOCATION]);
	SAFECOPY(user->zipcode, field[USER_ZIPCODE]);
	SAFECOPY(user->phone, field[USER_PHONE]);
	SAFECOPY(user->birth, field[USER_BIRTH]);
	user->sex = *field[USER_GENDER];
	SAFECOPY(user->comment, field[USER_COMMENT]);
	SAFECOPY(user->modem, field[USER_CONNECTION]);

	user->misc = (uint32_t)strtoul(field[USER_MISC], NULL, 16);
	user->qwk = (uint32_t)strtoul(field[USER_QWK], NULL, 16);
	user->chat = (uint32_t)strtoul(field[USER_CHAT], NULL, 16);
	user->mail = (uint32_t)strtoul(field[USER_MAIL], NULL, 16);

	user->rows = strtoul(field[USER_ROWS], NULL, 0);
	user->cols = strtoul(field[USER_COLS], NULL, 0);

	user->xedit = getxeditnum(cfg, field[USER_XEDIT]);
	user->shell = getshellnum(cfg, field[USER_SHELL]);

	SAFECOPY(user->tmpext, field[USER_TMPEXT]);
	user->prot = *field[USER_PROT];
	SAFECOPY(user->cursub, field[USER_CURSUB]);
	SAFECOPY(user->curdir, field[USER_CURDIR]);
	SAFECOPY(user->curxtrn, field[USER_CURXTRN]);
	user->logontime = parse_usertime(field[USER_LOGONTIME]);
	user->ns_time = parse_usertime(field[USER_NS_TIME]);
	user->laston = parse_usertime(field[USER_LASTON]);
	user->firston = parse_usertime(field[USER_FIRSTON]);

	user->logons = (ushort)strtoul(field[USER_LOGONS], NULL, 0);
	user->ltoday = (ushort)strtoul(field[USER_LTODAY], NULL, 0);
	user->timeon = (ushort)strtoul(field[USER_TIMEON], NULL, 0);
	user->ttoday = (ushort)strtoul(field[USER_TTODAY], NULL, 0);
	user->tlast = (ushort)strtoul(field[USER_TLAST], NULL, 0);
	user->posts = (ushort)strtoul(field[USER_POSTS], NULL, 0);
	user->emails = (ushort)strtoul(field[USER_EMAILS], NULL, 0);
	user->fbacks = (ushort)strtoul(field[USER_FBACKS], NULL, 0);
	user->etoday = (ushort)strtoul(field[USER_ETODAY], NULL, 0);
	user->ptoday = (ushort)strtoul(field[USER_PTODAY], NULL, 0);

	user->ulb = strtoull(field[USER_ULB], NULL, 0);
	user->uls = (ushort)strtoul(field[USER_ULS], NULL, 0);
	user->dlb = strtoull(field[USER_DLB], NULL, 0);
	user->dls = (ushort)strtoul(field[USER_DLS], NULL, 0);
	user->dlcps = (uint32_t)strtoul(field[USER_DLCPS], NULL, 0);
	user->leech = (uchar)strtoul(field[USER_LEECH], NULL, 0);

	SAFECOPY(user->pass, field[USER_PASS]);
	user->pwmod = parse_usertime(field[USER_PWMOD]);
	user->level = atoi(field[USER_LEVEL]);
	user->flags1 = aftou32(field[USER_FLAGS1]);
	user->flags2 = aftou32(field[USER_FLAGS2]);
	user->flags3 = aftou32(field[USER_FLAGS3]);
	user->flags4 = aftou32(field[USER_FLAGS4]);
	user->exempt = aftou32(field[USER_EXEMPT]);
	user->rest = aftou32(field[USER_REST]);
	user->cdt = strtoull(field[USER_CDT], NULL, 0);
	user->freecdt = strtoull(field[USER_FREECDT], NULL, 0);
	user->min = strtoul(field[USER_MIN], NULL, 0);
	user->textra = (ushort)strtoul(field[USER_TEXTRA], NULL, 0);
	user->expire = parse_usertime(field[USER_EXPIRE]);

	/* Reset daily stats if not already logged on today */
	if(user->ltoday || user->etoday || user->ptoday || user->ttoday) {
		time_t		now;
		struct tm	now_tm;
		struct tm	logon_tm;

		now=time(NULL);
		if(localtime_r(&now, &now_tm)!=NULL
			&& localtime32(&user->logontime, &logon_tm)!=NULL) {
			if(now_tm.tm_year!=logon_tm.tm_year
				|| now_tm.tm_mon!=logon_tm.tm_mon
				|| now_tm.tm_mday!=logon_tm.tm_mday)
				resetdailyuserdat(cfg,user,/* write: */false);
		}
	}
	return(0);
}

/****************************************************************************/
/* Fills the structure 'user' with info for user.number	from userbase file	*/
/****************************************************************************/
int getuserdat(scfg_t* cfg, user_t *user)
{
	int		retval;
	int		file;
	char	userdat[USER_RECORD_LINE_LEN + 1];

	if(!VALID_CFG(cfg) || user==NULL || !VALID_USER_NUMBER(user->number))
		return(-1);

	if((file = openuserdat(cfg, /* for_modify: */false)) < 0) {
		user->number = 0;
		return file;
	}

	if((retval = readuserdat(cfg, user->number, userdat, sizeof(userdat), file, /* leave_locked: */false)) != 0) {
		close(file);
		user->number = 0;
		return retval;
	}
	retval = parseuserdat(cfg, userdat, user, NULL);
	close(file);
	return retval;
}

/* Fast getuserdat() (leaves userbase file open) */
int fgetuserdat(scfg_t* cfg, user_t *user, int file)
{
	int		retval;
	char	userdat[USER_RECORD_LEN + 1];

	if(!VALID_CFG(cfg) || user==NULL || !VALID_USER_NUMBER(user->number))
		return(-1);

	if((retval = readuserdat(cfg, user->number, userdat, sizeof(userdat), file, /* leave_locked: */false)) != 0) {
		user->number = 0;
		return retval;
	}
	return parseuserdat(cfg, userdat, user, NULL);
}

/****************************************************************************/
/****************************************************************************/
static void dirtyuserdat(scfg_t* cfg, uint usernumber)
{
	int	i,file = -1;
    node_t	node;

	for(i=1;i<=cfg->sys_nodes;i++) { /* instant user data update */
//		if(i==cfg->node_num)
//			continue;
		if(getnodedat(cfg, i,&node, /* lockit: */false, &file) != 0)
			continue;
		if(node.useron==usernumber && (node.status==NODE_INUSE
			|| node.status==NODE_QUIET)) {
			if(getnodedat(cfg, i,&node, /* lockit: */true, &file) == 0) {
				node.misc|=NODE_UDAT;
				putnodedat(cfg, i,&node, /* closeit: */false, file);
			}
			break;
		}
	}
	CLOSE_OPEN_FILE(file);
}

/****************************************************************************/
/****************************************************************************/
int is_user_online(scfg_t* cfg, uint usernumber)
{
	int i;
	int file = -1;
	node_t	node;

	for(i=1; i<=cfg->sys_nodes; i++) {
		getnodedat(cfg, i, &node, /* lockit: */false, &file);
		if((node.status==NODE_INUSE || node.status==NODE_QUIET
			|| node.status==NODE_LOGON) && node.useron==usernumber)
			return i;
	}
	CLOSE_OPEN_FILE(file);
	return 0;
}

// Returns empty string if 't' is zero (Unix epoch)
static char* format_datetime(time_t t, char* str, size_t size)
{
	if(t == 0)
		*str = '\0';
	else
		gmtime_to_isoDateTimeStr(t, str, size);
	return str;
}

/****************************************************************************/
/****************************************************************************/
bool format_userdat(scfg_t* cfg, user_t* user, char userdat[])
{
	if(user == NULL)
		return false;

	if(!VALID_CFG(cfg) || !VALID_USER_NUMBER(user->number))
		return false;

	char flags1[LEN_FLAGSTR + 1];
	char flags2[LEN_FLAGSTR + 1];
	char flags3[LEN_FLAGSTR + 1];
	char flags4[LEN_FLAGSTR + 1];
	char exemptions[LEN_FLAGSTR + 1];
	char restrictions[LEN_FLAGSTR + 1];
	char logontime[64];
	char ns_time[64];
	char firston[64];
	char laston[64];
	char pwmod[64];
	char expire[64];
	u32toaf(user->flags1, flags1);
	u32toaf(user->flags2, flags2);
	u32toaf(user->flags3, flags3);
	u32toaf(user->flags4, flags4);
	u32toaf(user->exempt, exemptions);
	u32toaf(user->rest, restrictions);
	format_datetime(user->logontime, logontime, sizeof(logontime));
	format_datetime(user->ns_time, ns_time, sizeof(ns_time));
	format_datetime(user->firston, firston, sizeof(firston));
	format_datetime(user->laston, laston, sizeof(laston));
	format_datetime(user->pwmod, pwmod, sizeof(pwmod));
	format_datetime(user->expire, expire, sizeof(expire));

	// NOTE: order must match enum user_field definition (in userfields.h)
	int len = snprintf(userdat, USER_RECORD_LEN,
		"%u\t"	// USER_ID
		"%s\t"	// USER_ALIAS
		"%s\t"	// USER_NAME
		"%s\t"	// USER_HANDLE
		"%s\t"	// USER_NOTE
		"%s\t"	// USER_IPADDR
		"%s\t"	// USER_HOST
		"%s\t"	// USER_NETMAIL
		"%s\t"	// USER_ADDRESS
		"%s\t"	// USER_LOCATION
		"%s\t"	// USER_ZIPCODE
		"%s\t"	// USER_PHONE
		"%s\t"	// USER_BIRTH
		"%c\t"	// USER_GENDER
		"%s\t"	// USER_COMMENT
		"%s\t"	// USER_CONNECTION
		"%x\t"	// USER_MISC
		"%x\t"	// USER_QWK
		"%x\t"	// USER_CHAT
		"%u\t"	// USER_ROWS
		"%u\t"	// USER_COLS
		"%s\t"	// USER_XEDIT
		"%s\t"	// USER_SHELL
		"%s\t"	// USER_TMPEXT
		"%c\t"	// USER_PROT
		"%s\t"	// USER_CURSUB
		"%s\t"	// USER_CURDIR
		"%s\t"	// USER_CURXTRN
		"%s\t"	// USER_LOGONTIME
		"%s\t"	// USER_NS_TIME
		"%s\t"	// USER_LASTON
		"%s\t"	// USER_FIRSTON
		"%u\t"	// USER_LOGONS
		"%u\t"	// USER_LTODAY
		"%u\t"	// USER_TIMEON
		"%u\t"	// USER_TTODAY
		"%u\t"	// USER_TLAST
		"%u\t"	// USER_POSTS
		"%u\t"	// USER_EMAILS
		"%u\t"	// USER_FBACKS
		"%u\t"	// USER_ETODAY
		"%u\t"	// USER_PTODAY
		"%" PRIu64 "\t"	// USER_ULB
		"%u\t"			// USER_ULS
		"%" PRIu64 "\t" // USER_DLB
		"%u\t"			// USER_DLS
		"%" PRIu32 "\t" // USER_DLCPS
		"%s\t"	// USER_PASS
		"%s\t"	// USER_PWMOD
		"%u\t"	// USER_LEVEL
		"%s\t"	// USER_FLAGS1
		"%s\t"	// USER_FLAGS2
		"%s\t"	// USER_FLAGS3
		"%s\t"	// USER_FLAGS4
		"%s\t"	// USER_EXEMPT
		"%s\t"	// USER_REST
		"%" PRIu64 "\t"	// USER_CDT
		"%" PRIu64 "\t"	// USER_FREECDT
		"%" PRIu32 "\t" // USER_MIN
		"%u\t"	// USER_TEXTRA
		"%s\t"	// USER_EXPIRE
		"%u\t"	// USER_LEECH
		"%x\t"	// USER_MAIL
		"%s\t"	// USER_LANG
		,user->number
		,user->alias
		,user->name
		,user->handle
		,user->note
		,user->ipaddr
		,user->comp
		,user->netmail
		,user->address
		,user->location
		,user->zipcode
		,user->phone
		,user->birth
		,user->sex ? user->sex : '?'
		,user->comment
		,user->modem
		,user->misc
		,user->qwk
		,user->chat
		,user->rows
		,user->cols
		,user->xedit && user->xedit <= cfg->total_xedits ? cfg->xedit[user->xedit - 1]->code : ""
		,user->shell < cfg->total_shells ? cfg->shell[user->shell]->code : ""
		,user->tmpext
		,user->prot ? user->prot : ' '
		,user->cursub
		,user->curdir
		,user->curxtrn
		,logontime
		,ns_time
		,laston
		,firston
		,user->logons
		,(uint)user->ltoday
		,(uint)user->timeon
		,(uint)user->ttoday
		,(uint)user->tlast
		,(uint)user->posts
		,(uint)user->emails
		,(uint)user->fbacks
		,(uint)user->etoday
		,(uint)user->ptoday
		,user->ulb
		,(uint)user->uls
		,user->dlb
		,(uint)user->dls
		,user->dlcps
		,user->pass
		,pwmod
		,(uint)user->level
		,flags1
		,flags2
		,flags3
		,flags4
		,exemptions
		,restrictions
		,user->cdt
		,user->freecdt
		,user->min
		,user->textra
		,expire
		,(uint)user->leech
		,user->mail
		,user->lang
	);
	if(len > USER_RECORD_LEN || len < 0) // truncated?
		return false;

	memset(userdat + len, USER_FIELD_SEPARATOR, USER_RECORD_LEN - len);
	userdat[USER_RECORD_LINE_LEN - 1] = '\n';

	return true;
}

/****************************************************************************/
/* Writes into user.number's slot in userbase data in structure 'user'      */
/* Called from functions newuser, useredit and main                         */
/****************************************************************************/
int putuserdat(scfg_t* cfg, user_t* user)
{
    int		file;
    char	userdat[USER_RECORD_LINE_LEN];

	if(user==NULL)
		return(-1);

	if(!VALID_CFG(cfg) || !VALID_USER_NUMBER(user->number))
		return(-1);

	if(!format_userdat(cfg, user, userdat))
		return -10;

	if((file=openuserdat(cfg, /* for_modify: */true)) < 0)
		return(errno);

	if(filelength(file)<((off_t)user->number - 1) * USER_RECORD_LINE_LEN) {
		close(file);
		return(-4);
	}

	seekuserdat(file, user->number);
	if(!lockuserdat(file, user->number)) {
		close(file);
		return(-2);
	}

	if(write(file,userdat,sizeof(userdat)) != sizeof(userdat)) {
		unlockuserdat(file, user->number);
		close(file);
		return(-3);
	}
	unlockuserdat(file, user->number);
	close(file);
	dirtyuserdat(cfg,user->number);

	return(0);
}

/****************************************************************************/
/* Returns the username in 'str' that corresponds to the 'usernumber'       */
/* Called from functions everywhere                                         */
/****************************************************************************/
char* username(scfg_t* cfg, int usernumber, char *name)
{
    char	str[256];
    int		c;
    int		file;

	if(name==NULL)
		return(NULL);

	if(!VALID_CFG(cfg) || !VALID_USER_NUMBER(usernumber)) {
		name[0]=0;
		return(name);
	}
	SAFEPRINTF(str,"%suser/name.dat",cfg->data_dir);
	if(flength(str)<1L) {
		name[0]=0;
		return(name);
	}
	if((file=nopen(str,O_RDONLY))==-1) {
		name[0]=0;
		return(name);
	}
	if(filelength(file)<(long)((long)usernumber*(LEN_ALIAS+2))) {
		close(file);
		name[0]=0;
		return(name);
	}
	(void)lseek(file,(long)((long)(usernumber-1)*(LEN_ALIAS+2)),SEEK_SET);
	if(read(file,name,LEN_ALIAS) != LEN_ALIAS)
		memset(name, ETX, LEN_ALIAS);
	close(file);
	for(c=0;c<LEN_ALIAS;c++)
		if(name[c]==ETX) break;
	name[c]=0;
	if(!c)
		strcpy(name,"DELETED USER");
	return(name);
}

/****************************************************************************/
/* Puts 'name' into slot 'number' in user/name.dat							*/
/****************************************************************************/
int putusername(scfg_t* cfg, int number, const char *name)
{
	char str[256];
	int file;
	int wr;
	off_t length;
	off_t total_users;

	if(!VALID_CFG(cfg) || name==NULL || !VALID_USER_NUMBER(number))
		return(-1);

	SAFEPRINTF(str,"%suser/name.dat", cfg->data_dir);
	if((file=nopen(str,O_RDWR|O_CREAT))==-1)
		return(errno);
	length = filelength(file);

	/* Truncate corrupted name.dat */
	total_users=lastuser(cfg);
	if(length/(LEN_ALIAS+2) > total_users) {
		if(chsize(file,(long)(total_users*(LEN_ALIAS+2))) != 0) {
			close(file);
			return -4;
		}
	}

	if(length && length%(LEN_ALIAS+2)) {
		close(file);
		return(-3);
	}
	if(length<(((long)number-1)*(LEN_ALIAS+2))) {
		SAFEPRINTF2(str,"%*s\r\n",LEN_ALIAS,"");
		memset(str,ETX,LEN_ALIAS);
		(void)lseek(file,0L,SEEK_END);
		while((length = filelength(file)) >= 0 && length < ((long)number*(LEN_ALIAS+2)))	// Shouldn't this be (number-1)?
			if(write(file,str,(LEN_ALIAS+2)) != LEN_ALIAS+2)
				break;
	}
	(void)lseek(file,(long)(((long)number-1)*(LEN_ALIAS+2)),SEEK_SET);
	putrec(str,0,LEN_ALIAS,name);
	putrec(str,LEN_ALIAS,2,"\r\n");
	wr=write(file,str,LEN_ALIAS+2);
	close(file);

	if(wr!=LEN_ALIAS+2)
		return(errno);
	return(0);
}

#define DECVAL(ch, mul)	(DEC_CHAR_TO_INT(ch) * (mul))

int getbirthyear(const char* birth)
{
	if(IS_DIGIT(birth[2]))				// CCYYMMDD format
		return DECVAL(birth[0], 1000)
				+ DECVAL(birth[1], 100)
				+ DECVAL(birth[2], 10)
				+ DECVAL(birth[3], 1);
	// DD/MM/YY or MM/DD/YY format
	time_t now = time(NULL);
	struct	tm tm;
	if(localtime_r(&now, &tm) == NULL)
		return 0;
	tm.tm_year += 1900;
	int year = 1900 + DECVAL(birth[6], 10) + DECVAL(birth[7], 1);
	if(tm.tm_year - year > 105)
		year += 100;
	return year;
}

int getbirthmonth(scfg_t* cfg, const char* birth)
{
	if(IS_DIGIT(birth[5]))				// CCYYMMDD format
		return DECVAL(birth[4], 10)	+ DECVAL(birth[5], 1);
	if(cfg->sys_misc & SM_EURODATE) {	// DD/MM/YY format
		return DECVAL(birth[3], 10) + DECVAL(birth[4], 1);
	} else {							// MM/DD/YY format
		return DECVAL(birth[0], 10) + DECVAL(birth[1], 1);
	}
}

int getbirthday(scfg_t* cfg, const char* birth)
{
	if(IS_DIGIT(birth[5]))				// CCYYMMDD format
		return DECVAL(birth[6], 10)	+ DECVAL(birth[7], 1);
	if(cfg->sys_misc & SM_EURODATE) {	// DD/MM/YY format
		return DECVAL(birth[0], 10) + DECVAL(birth[1], 1);
	} else {							// MM/DD/YY format
		return DECVAL(birth[3], 10) + DECVAL(birth[4], 1);
	}
}

// Always returns string in MM/DD/YY format
char* getbirthmmddyy(scfg_t* cfg, char sep, const char* birth, char* buf, size_t max)
{
	safe_snprintf(buf, max, "%02u%c%02u%c%02u"
		, getbirthmonth(cfg, birth)
		, sep
		, getbirthday(cfg, birth)
		, sep
		, getbirthyear(birth) % 100);
	return buf;
}

// Always returns string in DD/MM/YY format
char* getbirthddmmyy(scfg_t* cfg, char sep, const char* birth, char* buf, size_t max)
{
	safe_snprintf(buf, max, "%02u%c%02u%c%02u"
		, getbirthday(cfg, birth)
		, sep
		, getbirthmonth(cfg, birth)
		, sep
		, getbirthyear(birth) % 100);
	return buf;
}

// Always returns string in YY/MM/DD format
char* getbirthyymmdd(scfg_t* cfg, char sep, const char* birth, char* buf, size_t max)
{
	safe_snprintf(buf, max, "%02u%c%02u%c%02u"
		, getbirthyear(birth) % 100
		, sep
		, getbirthmonth(cfg, birth)
		, sep
		, getbirthday(cfg, birth));
	return buf;
}

char* getbirthdstr(scfg_t* cfg, const char* birth, char* buf, size_t max)
{
	if(cfg->sys_date_fmt == YYMMDD)
		getbirthyymmdd(cfg, cfg->sys_date_sep, birth, buf, max);
	else if(cfg->sys_date_fmt == DDMMYY)
		getbirthddmmyy(cfg, cfg->sys_date_sep, birth, buf, max);
	else
		getbirthmmddyy(cfg, cfg->sys_date_sep, birth, buf, max);
	return buf;
}

/****************************************************************************/
/* Returns the age derived from the string 'birth' in the format CCYYMMDD	*/
/* or legacy: MM/DD/YY or DD/MM/YY											*/
/* Returns 0 on invalid 'birth' date										*/
/****************************************************************************/
int getage(scfg_t* cfg, const char *birth)
{
	struct	tm tm;
	time_t	now;

	if(!VALID_CFG(cfg) || birth==NULL)
		return(0);

	if(!atoi(birth) || !atoi(birth+3))	/* Invalid */
		return(0);

	now=time(NULL);
	if(localtime_r(&now,&tm)==NULL)
		return(0);

	tm.tm_mon++;	/* convert to 1 based */
	int year = getbirthyear(birth);
	int age = (1900 + tm.tm_year) - year;
	int mon = getbirthmonth(cfg, birth);
	int day = getbirthday(cfg, birth);
	if(mon < 1 || mon > 12 || day < 1 || day > 31)
		return 0;
	if(mon > tm.tm_mon || (mon == tm.tm_mon && day > tm.tm_mday))
		age--;
	return age;
}

/****************************************************************************/
/* Converts from MM/DD/YYYYY, DD/MM/YYYY, or YYYY/MM/DD to YYYYMMDD			*/
/****************************************************************************/
char* parse_birthdate(scfg_t* cfg, const char* birthdate, char* out, size_t maxlen)
{
	if (cfg->sys_date_fmt == YYMMDD)
		safe_snprintf(out, maxlen, "%.4s%.2s%.2s", birthdate, birthdate + 5, birthdate + 8);
	else if (cfg->sys_date_fmt == DDMMYY)
		safe_snprintf(out, maxlen, "%.4s%.2s%.2s", birthdate + 6, birthdate + 3, birthdate);
	else
		safe_snprintf(out, maxlen, "%.4s%.2s%.2s", birthdate + 6, birthdate, birthdate + 3);
	return out;
}

/****************************************************************************/
/* Converts from user birth date to MM/DD/YYYYY, DD/MM/YYYY, or YYYY/MM/DD	*/
/****************************************************************************/
char* format_birthdate(scfg_t* cfg, const char* birthdate, char* out, size_t maxlen)
{
	if(maxlen < 1)
		return NULL;
	*out = '\0';
	if(*birthdate) {
		if (cfg->sys_date_fmt == YYMMDD)
			safe_snprintf(out, maxlen, "%04u%c%02u%c%02u"
				, getbirthyear(birthdate)
				, cfg->sys_date_sep
				, getbirthmonth(cfg, birthdate)
				, cfg->sys_date_sep
				, getbirthday(cfg, birthdate));
		else if (cfg->sys_date_fmt == DDMMYY)
			safe_snprintf(out, maxlen, "%02u%c%02u%c%04u"
				, getbirthday(cfg, birthdate)
				, cfg->sys_date_sep
				, getbirthmonth(cfg, birthdate)
				, cfg->sys_date_sep
				, getbirthyear(birthdate));
		else
			safe_snprintf(out, maxlen, "%02u%c%02u%c%04u"
				, getbirthmonth(cfg, birthdate)
				, cfg->sys_date_sep
				, getbirthday(cfg, birthdate)
				, cfg->sys_date_sep
				, getbirthyear(birthdate));
	}
	return out;
}

/****************************************************************************/
/****************************************************************************/
char* birthdate_format(scfg_t* cfg, char* buf, size_t size)
{
	switch (cfg->sys_date_fmt) {
		case MMDDYY: snprintf(buf, size, "MM%cDD%cYYYY", cfg->sys_date_sep, cfg->sys_date_sep); return buf;
		case DDMMYY: snprintf(buf, size, "DD%cMM%cYYYY", cfg->sys_date_sep, cfg->sys_date_sep); return buf;
		case YYMMDD: snprintf(buf, size, "YYYY%cMM%cDD", cfg->sys_date_sep, cfg->sys_date_sep); return buf;
	}
	return "??????????";
}

/****************************************************************************/
/****************************************************************************/
char* birthdate_template(scfg_t* cfg, char* buf, size_t size)
{
	if (cfg->sys_date_fmt == YYMMDD)
		snprintf(buf, size, "nnnn%cnn%cnn", cfg->sys_date_sep, cfg->sys_date_sep);
	else
		snprintf(buf, size, "nn%cnn%cnnnn", cfg->sys_date_sep, cfg->sys_date_sep);
	return buf;
}

/****************************************************************************/
/****************************************************************************/
int opennodedat(scfg_t* cfg)
{
	char	fname[MAX_PATH+1];

	if(!VALID_CFG(cfg))
		return -1;

	SAFEPRINTF(fname, "%snode.dab", cfg->ctrl_dir);
	return nopen(fname, O_RDWR|O_DENYNONE);
}

/****************************************************************************/
/****************************************************************************/
int opennodeext(scfg_t* cfg)
{
	char	fname[MAX_PATH+1];

	if(!VALID_CFG(cfg))
		return -1;

	SAFEPRINTF(fname, "%snode.exb", cfg->ctrl_dir);
	return nopen(fname, O_RDWR|O_DENYNONE);
}

/****************************************************************************/
/* Reads the data for 1-based node number 'number' into the node structure	*/
/* from node.dab															*/
/****************************************************************************/
int getnodedat(scfg_t* cfg, uint number, node_t *node, bool lockit, int* fdp)
{
	int		rd;
	int		count=0;
	int		file;

	if(!VALID_CFG(cfg)
		|| node==NULL || number<1 || number>cfg->sys_nodes)
		return(-1);

	memset(node,0,sizeof(node_t));
	if(fdp != NULL && *fdp > 0)
		file = *fdp;
	else {
		if((file = opennodedat(cfg)) == -1)
			return errno;
	}

	if(filelength(file)>=(long)(number*sizeof(node_t))) {
		number--;	/* make zero based */
		for(count=0;count<LOOP_NODEDAB;count++) {
			if(count)
				mswait(100);
			(void)lseek(file,(long)number*sizeof(node_t),SEEK_SET);
			if(lockit
				&& lock(file,(long)number*sizeof(node_t),sizeof(node_t))!=0)
				continue;
			rd=read(file,node,sizeof(node_t));
			if(rd!=sizeof(node_t))
				unlock(file,(long)number*sizeof(node_t),sizeof(node_t));
			if(rd==sizeof(node_t))
				break;
		}
	}

	if(fdp==NULL || count==LOOP_NODEDAB)
		close(file);
	else
		*fdp=file;

	if(count==LOOP_NODEDAB)
		return(-2);

	return(0);
}

/****************************************************************************/
/* Write the data from the structure 'node' into node.dab  					*/
/* number is the 1-based node number										*/
/****************************************************************************/
int putnodedat(scfg_t* cfg, uint number, node_t* node, bool closeit, int file)
{
	size_t	wr=0;
	int		wrerr=0;
	int		attempts;

	if(file<0)
		return -1;
	if(!VALID_CFG(cfg)
		|| node==NULL || number<1 || number>cfg->sys_nodes) {
		if(closeit)
			close(file);
		return(-1);
	}

	number--;	/* make zero based */
	for(attempts=0;attempts<10;attempts++) {
		(void)lseek(file,(long)number*sizeof(node_t),SEEK_SET);
		if((wr=write(file,node,sizeof(node_t)))==sizeof(node_t))
			break;
		wrerr=errno;	/* save write error */
		mswait(100);
	}
	unlock(file,(long)number*sizeof(node_t),sizeof(node_t));
	if(closeit)
		close(file);

	if(wr!=sizeof(node_t))
		return(wrerr);
	return(0);
}

/****************************************************************************/
/****************************************************************************/
bool set_node_status(scfg_t* cfg, int nodenum, enum node_status status)
{
	node_t node;
	int file;

	if(getnodedat(cfg, nodenum, &node, /* lockit: */true, &file) != 0)
		return false;
	node.status = status;
	return putnodedat(cfg, nodenum, &node, /* closeit: */true, file) == 0;
}

/****************************************************************************/
/****************************************************************************/
bool set_node_misc(scfg_t* cfg, int nodenum, uint misc)
{
	node_t node;
	int file;

	if(getnodedat(cfg, nodenum, &node, /* lockit: */true, &file) != 0)
		return false;
	node.misc = misc;
	return putnodedat(cfg, nodenum, &node, /* closeit: */true, file) == 0;
}

/****************************************************************************/
/****************************************************************************/
bool set_node_lock(scfg_t* cfg, int nodenum, bool set)
{
	node_t node;
	int file;

	if(getnodedat(cfg, nodenum, &node, /* lockit: */true, &file) != 0)
		return false;
	if(set)
		node.misc |= NODE_LOCK;
	else
		node.misc &= ~NODE_LOCK;
	return putnodedat(cfg, nodenum, &node, /* closeit: */true, file) == 0;
}

/****************************************************************************/
/****************************************************************************/
bool set_node_interrupt(scfg_t* cfg, int nodenum, bool set)
{
	node_t node;
	int file;

	if(getnodedat(cfg, nodenum, &node, /* lockit: */true, &file) != 0)
		return false;
	if(set)
		node.misc |= NODE_INTR;
	else
		node.misc &= ~NODE_INTR;
	return putnodedat(cfg, nodenum, &node, /* closeit: */true, file) == 0;
}

/****************************************************************************/
/****************************************************************************/
bool set_node_down(scfg_t* cfg, int nodenum, bool set)
{
	node_t node;
	int file;

	if(getnodedat(cfg, nodenum, &node, /* lockit: */true, &file) != 0)
		return false;
	if(set)
		node.misc |= NODE_DOWN;
	else
		node.misc &= ~NODE_DOWN;
	return putnodedat(cfg, nodenum, &node, /* closeit: */true, file) == 0;
}

/****************************************************************************/
/****************************************************************************/
bool set_node_rerun(scfg_t* cfg, int nodenum, bool set)
{
	node_t node;
	int file;

	if(getnodedat(cfg, nodenum, &node, /* lockit: */true, &file) != 0)
		return false;
	if(set)
		node.misc |= NODE_RRUN;
	else
		node.misc &= ~NODE_RRUN;
	return putnodedat(cfg, nodenum, &node, /* closeit: */true, file) == 0;
}

/****************************************************************************/
/****************************************************************************/
bool set_node_errors(scfg_t* cfg, int nodenum, uint errors)
{
	node_t node;
	int file;

	if(getnodedat(cfg, nodenum, &node, /* lockit: */true, &file) != 0)
		return false;
	node.errors = errors;
	return putnodedat(cfg, nodenum, &node, /* closeit: */true, file) == 0;
}

/****************************************************************************/
/* Packs the password 'pass' into 5bit ASCII inside node_t. 32bits in 		*/
/* node.extaux, and the other 8bits in the upper byte of node.aux			*/
/****************************************************************************/
void packchatpass(char *pass, node_t *node)
{
	char	bits;
	int		i,j;

	if(pass==NULL || node==NULL)
		return;

	node->aux&=~0xff00;		/* clear the password */
	node->extaux=0L;
	if((j=strlen(pass))==0) /* there isn't a password */
		return;
	node->aux|=(int)((pass[0]-64)<<8);  /* 1st char goes in low 5bits of aux */
	if(j==1)	/* password is only one char, we're done */
		return;
	node->aux|=(int)((pass[1]-64)<<13); /* low 3bits of 2nd char go in aux */
	node->extaux|=(long)((pass[1]-64)>>3); /* high 2bits of 2nd char go extaux */
	bits=2;
	for(i=2;i<j;i++) {	/* now process the 3rd char through the last */
		node->extaux|=(long)((long)(pass[i]-64)<<bits);
		bits+=5;
	}
}

/****************************************************************************/
/* Unpacks the password 'pass' from the 5bit ASCII inside node_t. 32bits in */
/* node.extaux, and the other 8bits in the upper byte of node.aux			*/
/****************************************************************************/
char* unpackchatpass(char *pass, node_t* node)
{
	char 	bits;
	int 	i;

	if(pass==NULL || node==NULL)
		return(NULL);

	pass[0]=(node->aux&0x1f00)>>8;
	pass[1]=(char)(((node->aux&0xe000)>>13)|((node->extaux&0x3)<<3));
	bits=2;
	for(i=2;i<8;i++) {
		pass[i]=(char)((node->extaux>>bits)&0x1f);
		bits+=5;
	}
	pass[8]=0;
	for(i=0;i<8;i++)
		if(pass[i])
			pass[i]+=64;
	return(pass);
}

static char* node_connection_desc(ushort conn, char* str)
{
	switch(conn) {
		case NODE_CONNECTION_LOCAL:
			strcpy(str,"Locally");
			break;
		case NODE_CONNECTION_TELNET:
			strcpy(str,"via telnet");
			break;
		case NODE_CONNECTION_RLOGIN:
			strcpy(str,"via rlogin");
			break;
		case NODE_CONNECTION_SSH:
			strcpy(str,"via ssh");
			break;
		case NODE_CONNECTION_SFTP:
			strcpy(str,"via sftp");
			break;
		case NODE_CONNECTION_RAW:
			strcpy(str,"via raw");
			break;
		default:
			sprintf(str,"at %ubps",conn);
			break;
	}

	return str;
}

char* getnodeext(scfg_t* cfg, int num, char* buf)
{
	int f;

	if(!VALID_CFG(cfg) || num < 1)
		return "";
	if((f = opennodeext(cfg)) < 1)
		return "";
	(void)lseek(f, (num-1) * 128, SEEK_SET);
	if(read(f, buf, 128) != 128)
		memset(buf, 0, 128);
	close(f);
	buf[127] = 0;
	return buf;
}

char* nodestatus(scfg_t* cfg, node_t* node, char* buf, size_t buflen, int num)
{
	char	str[256];
	char	tmp[128];
	char*	mer;
	int		hour;
	int		xtrnnum;
	user_t	user = {0};

	if(node==NULL) {
		strncpy(buf,"(null)",buflen);
		return(buf);
	}

	str[0]=0;
    switch(node->status) {
        case NODE_WFC:
            SAFECOPY(str,"Waiting for connection");
            break;
        case NODE_OFFLINE:
            strcpy(str,"Offline");
            break;
        case NODE_NETTING:	/* Obsolete */
            SAFECOPY(str,"Networking");
            break;
        case NODE_LOGON:
            SAFEPRINTF(str,"At login prompt %s"
				,node_connection_desc(node->connection, tmp));
            break;
		case NODE_LOGOUT:
			SAFEPRINTF(str,"Logging out %s", username(cfg,node->useron,tmp));
			break;
        case NODE_EVENT_WAITING:
            SAFECOPY(str,"Waiting for all nodes to become inactive");
            break;
        case NODE_EVENT_LIMBO:
            SAFEPRINTF(str,"Waiting for node %d to finish external event"
                ,node->aux);
            break;
        case NODE_EVENT_RUNNING:
            SAFECOPY(str,"Running external event");
            break;
        case NODE_NEWUSER:
            SAFEPRINTF(str,"New user applying for access %s"
				,node_connection_desc(node->connection, tmp));
            break;
        case NODE_QUIET:
        case NODE_INUSE:
			if(node->misc & NODE_EXT) {
				getnodeext(cfg, num, str);
				break;
			}
            username(cfg,node->useron,str);
            strcat(str," ");
            switch(node->action) {
                case NODE_MAIN:
                    strcat(str,"at main menu");
                    break;
                case NODE_RMSG:
                    strcat(str,"reading messages");
                    break;
                case NODE_RMAL:
                    strcat(str,"reading mail");
                    break;
                case NODE_RSML:
                    strcat(str,"reading sent mail");
                    break;
                case NODE_RTXT:
                    strcat(str,"reading text files");
                    break;
                case NODE_PMSG:
                    strcat(str,"posting message");
                    break;
                case NODE_SMAL:
                    strcat(str,"sending mail");
                    break;
                case NODE_AMSG:
                    strcat(str,"posting auto-message");
                    break;
                case NODE_XTRN:
                    if(node->aux == 0) {
                        strcat(str,"at external program menu");
						break;
					}
					user.number = node->useron;
					getuserdat(cfg, &user);
					xtrnnum = getxtrnnum(cfg, user.curxtrn);
					if(is_valid_xtrnnum(cfg, xtrnnum))
						sprintf(str+strlen(str),"running %s"
							,cfg->xtrn[xtrnnum]->name);
					else if(*user.curxtrn != '\0')
						sprintf(str+strlen(str),"running external program %s"
							,user.curxtrn);
					else
						sprintf(str+strlen(str),"running external program #%d"
							,node->aux);
                    break;
                case NODE_DFLT:
                    strcat(str,"changing defaults");
                    break;
                case NODE_XFER:
                    strcat(str,"at transfer menu");
                    break;
                case NODE_RFSD:
                    sprintf(str+strlen(str),"retrieving from device #%d",node->aux);
                    break;
                case NODE_DLNG:
                    strcat(str,"downloading");
                    break;
                case NODE_ULNG:
                    strcat(str,"uploading");
                    break;
                case NODE_BXFR:
                    strcat(str,"transferring bidirectional");
                    break;
                case NODE_LFIL:
                    strcat(str,"listing files");
                    break;
                case NODE_LOGN:
                    strcat(str,"logging on");
                    break;
                case NODE_LCHT:
                    strcat(str,"in local chat with sysop");
                    break;
                case NODE_MCHT:
                    if(node->aux) {
                        sprintf(str+strlen(str),"in multinode chat channel %d"
                            ,node->aux&0xff);
                        if(node->aux&0x1f00) { /* password */
                            strcat(str,"* ");
                            unpackchatpass(str+strlen(str),node);
                        }
                    }
                    else
                        strcat(str,"in multinode global chat channel");
                    break;
                case NODE_PAGE:
                    sprintf(str+strlen(str)
						,"paging node %u for private chat",node->aux);
                    break;
                case NODE_PCHT:
                    if(node->aux==0)
                        sprintf(str+strlen(str)
							,"in local chat with %s"
							,cfg->sys_op);
                    else
                        sprintf(str+strlen(str)
							,"in private chat with node %u"
                            ,node->aux);
                    break;
                case NODE_GCHT:
                    strcat(str,"chatting with The Guru");
                    break;
                case NODE_CHAT:
                    strcat(str,"in chat section");
                    break;
                case NODE_TQWK:
                    strcat(str,"transferring QWK packet");
                    break;
                case NODE_SYSP:
                    strcat(str,"performing sysop activities");
                    break;
                default:
                    sprintf(str+strlen(str),"%d",node->action);
                    break;
			}
			sprintf(str+strlen(str)," %s",node_connection_desc(node->connection, tmp));
            if(node->action==NODE_DLNG) {
                if((node->aux/60)>=12) {
                    if(node->aux/60==12)
                        hour=12;
                    else
                        hour=(node->aux/60)-12;
                    mer="pm";
                } else {
                    if((node->aux/60)==0)    /* 12 midnite */
                        hour=12;
                    else hour=node->aux/60;
                    mer="am";
                }
                sprintf(str+strlen(str), " ETA %02d:%02d %s"
                    ,hour,node->aux-((node->aux/60)*60),mer);
            }
            break;
	}
    if(node->misc&(NODE_LOCK|NODE_POFF|NODE_AOFF|NODE_MSGW|NODE_NMSG)) {
        strcat(str," (");
        if(node->misc&NODE_AOFF)
            strcat(str,"A");
        if(node->misc&NODE_LOCK)
            strcat(str,"L");
        if(node->misc&NODE_MSGW)
            strcat(str,"M");
        if(node->misc&NODE_NMSG)
            strcat(str,"N");
        if(node->misc&NODE_POFF)
            strcat(str,"P");
        strcat(str,")");
	}
    if(((node->misc
        &(NODE_ANON|NODE_UDAT|NODE_INTR|NODE_RRUN|NODE_EVENT|NODE_DOWN))
        || node->status==NODE_QUIET)) {
        strcat(str," [");
        if(node->misc&NODE_ANON)
            strcat(str,"A");
        if(node->misc&NODE_INTR)
            strcat(str,"I");
        if(node->misc&NODE_RRUN)
            strcat(str,"R");
        if(node->misc&NODE_UDAT)
            strcat(str,"U");
        if(node->status==NODE_QUIET)
            strcat(str,"Q");
        if(node->misc&NODE_EVENT)
            strcat(str,"E");
        if(node->misc&NODE_DOWN)
            strcat(str,"D");
        if(node->misc&NODE_LCHAT)
            strcat(str,"C");
        if(node->misc&NODE_FCHAT)
            strcat(str,"F");
        strcat(str,"]");
	}
	if(node->errors)
		sprintf(str+strlen(str)
			," %d error%c",node->errors, node->errors>1 ? 's' : '\0' );

	strncpy(buf,str,buflen);

	return(buf);
}

/****************************************************************************/
/* Displays the information for node number 'number' contained in 'node'    */
/****************************************************************************/
void printnodedat(scfg_t* cfg, uint number, node_t* node)
{
	char	status[128];

	printf("Node %2d: %s\n",number,nodestatus(cfg,node,status,sizeof(status),number));
}

/****************************************************************************/
uint finduserstr(scfg_t* cfg, uint usernumber, enum user_field fnum
						 ,const char* str, bool del, bool next, void (*progress)(void*, int, int), void* cbdata)
{
	int		file;
    int		unum;
	uint	found = 0;

	if(!VALID_CFG(cfg) || str == NULL)
		return(0);

	if((file=openuserdat(cfg, /* for_modify: */false)) == -1)
		return(0);
	int last = (int)filelength(file) / USER_RECORD_LINE_LEN;
	if(usernumber && next)
		unum = usernumber;
	else
		unum = 1;
	if(progress != NULL)
		progress(cbdata, unum, last);
	for(;unum <= last && found == 0; unum++) {
		if(progress != NULL)
			progress(cbdata, unum, last);
		if(usernumber && unum == usernumber)
			continue;
		char userdat[USER_RECORD_LEN + 1];
		if(readuserdat(cfg, unum, userdat, sizeof(userdat), file, /* leave_locked: */false) == 0) {
			char* field[USER_FIELD_COUNT];
			split_userdat(userdat, field);
			if(stricmp(field[fnum], str) == 0) {
				if(del || !(ahtou32(field[USER_MISC]) & (DELETED|INACTIVE)))
					found = unum;
			}
		}
	}
	close(file);
	if(progress != NULL)
		progress(cbdata, unum, last);
	return found;
}

/****************************************************************************/
/* Creates a short message for 'usernumber' that contains 'strin'           */
/****************************************************************************/
int putsmsg(scfg_t* cfg, int usernumber, char *strin)
{
    char str[256];
    int file,i;
    node_t node;

	if(!VALID_CFG(cfg) || !VALID_USER_NUMBER(usernumber) || strin==NULL)
		return(-1);

	if(*strin==0)
		return(0);

	SAFEPRINTF2(str,"%smsgs/%4.4u.msg",cfg->data_dir,usernumber);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
		return(errno);
	}
	i=strlen(strin);
	if(write(file,strin,i)!=i) {
		close(file);
		return(errno);
	}
	close(file);
	file = -1;
	for(i=1;i<=cfg->sys_nodes;i++) {     /* flag node if user on that msg waiting */
		getnodedat(cfg,i,&node,/* lockit: */false, &file);
		if(node.useron==usernumber
			&& (node.status==NODE_INUSE || node.status==NODE_QUIET)
			&& !(node.misc&NODE_MSGW)) {
			if(getnodedat(cfg,i,&node, /* lockit: */true, &file)==0) {
				node.misc|=NODE_MSGW;
				putnodedat(cfg,i,&node, /* closeit: */false, file);
			}
		}
	}
	CLOSE_OPEN_FILE(file);
	return(0);
}

/****************************************************************************/
/* Returns any short messages waiting for user number, buffer must be freed */
/****************************************************************************/
char* getsmsg(scfg_t* cfg, int usernumber)
{
	int		i;
    int		file = -1;
	node_t	node;

	if(!VALID_CFG(cfg) || !VALID_USER_NUMBER(usernumber))
		return(NULL);

	for(i=1;i<=cfg->sys_nodes;i++) {	/* clear msg waiting flag */
		getnodedat(cfg,i,&node, /* lockit: */false, &file);
		if(node.useron==usernumber
			&& (node.status==NODE_INUSE || node.status==NODE_QUIET)
			&& node.misc&NODE_MSGW) {
			if(getnodedat(cfg,i,&node, /* lockit: */true, &file) == 0) {
				node.misc&=~NODE_MSGW;
				putnodedat(cfg,i,&node, /* closeit: */false, file);
			}
		}
	}
	CLOSE_OPEN_FILE(file);

	return readsmsg(cfg, usernumber);
}

/****************************************************************************/
/* Returns any short messages waiting for user number, buffer must be freed */
/****************************************************************************/
char* readsmsg(scfg_t* cfg, int usernumber)
{
	char	str[MAX_PATH+1], *buf;
    int		file;
    long	length;

	if(!VALID_CFG(cfg) || !VALID_USER_NUMBER(usernumber))
		return(NULL);

	SAFEPRINTF2(str,"%smsgs/%4.4u.msg",cfg->data_dir,usernumber);
	if(flength(str)<1L)
		return(NULL);
	if((file=nopen(str,O_RDWR))==-1)
		return(NULL);
	length=(long)filelength(file);
	if(length < 0 || (buf=(char *)malloc(length+1))==NULL) {
		close(file);
		return(NULL);
	}
	if(read(file,buf,length)!=length || chsize(file, 0) != 0) {
		close(file);
		free(buf);
		return(NULL);
	}
	close(file);
	buf[length]=0;
	strip_invalid_attr(buf);

	SAFEPRINTF2(str, "%smsgs/%4.4u.last.msg", cfg->data_dir, usernumber);
	backup(str, 19, /* rename: */true);
	if((file = nopen(str, O_WRONLY|O_CREAT|O_APPEND)) != -1) {
		int wr = write(file, buf, length);
		close(file);
		if(wr != length) {
			free(buf);
			return NULL;
		}
	}

	return(buf);	/* caller must free */
}

char* getnmsg(scfg_t* cfg, int node_num)
{
	char	str[MAX_PATH+1];
	char*	buf;
	int		file = -1;
	long	length;
	node_t	node;

	if(!VALID_CFG(cfg) || node_num<1)
		return(NULL);

	if(getnodedat(cfg,node_num,&node, /* lockit: */true, &file) == 0) {
		node.misc&=~NODE_NMSG;          /* clear the NMSG flag */
		putnodedat(cfg,node_num,&node, /* closeit: */true, file);
	}

	SAFEPRINTF2(str,"%smsgs/n%3.3u.msg",cfg->data_dir,node_num);
	if(flength(str)<1L)
		return(NULL);
	if((file=nopen(str,O_RDWR))==-1)
		return(NULL);
	length=(long)filelength(file);
	if(length < 1) {
		close(file);
		return(NULL);
	}
	if((buf=(char *)malloc(length+1))==NULL) {
		close(file);
		return(NULL);
	}
	if(read(file,buf,length)!=length || chsize(file, 0) != 0) {
		close(file);
		free(buf);
		return(NULL);
	}
	close(file);
	buf[length]=0;

	return(buf);	/* caller must free */
}

/****************************************************************************/
/* Creates a short message for node 'num' that contains 'strin'             */
/****************************************************************************/
int putnmsg(scfg_t* cfg, int num, char *strin)
{
    char str[256];
    int file,i;
    node_t node;

	if(!VALID_CFG(cfg) || num<1 || strin==NULL)
		return(-1);

	if(*strin==0)
		return(0);

	SAFEPRINTF2(str,"%smsgs/n%3.3u.msg",cfg->data_dir,num);
	if((file=nopen(str,O_WRONLY|O_CREAT))==-1)
		return(errno);
	(void)lseek(file,0L,SEEK_END);	/* Instead of opening with O_APPEND */
	i=strlen(strin);
	if(write(file,strin,i)!=i) {
		close(file);
		return(errno);
	}
	CLOSE_OPEN_FILE(file);
	getnodedat(cfg,num,&node, /* lockit: */false, &file);
	if((node.status==NODE_INUSE || node.status==NODE_QUIET)
		&& !(node.misc&NODE_NMSG)) {
		if(getnodedat(cfg,num,&node, /* lockit: */true, &file) == 0) {
			node.misc|=NODE_NMSG;
			putnodedat(cfg,num,&node, /* closeit: */false, file);
		}
	}
	CLOSE_OPEN_FILE(file);

	return(0);
}

/* Return node's client's socket descriptor or negative on error */
int getnodeclient(scfg_t* cfg, uint number, client_t* client, time_t* done)
{
	SOCKET sock = INVALID_SOCKET;
	char path[MAX_PATH + 1];
	char value[INI_MAX_VALUE_LEN];
	char* p;
	FILE* fp;

	if(!VALID_CFG(cfg)
		|| client == NULL || number < 1 || number > cfg->sys_nodes)
		return -1;

	if(client->size == sizeof(client)) {
		free((char*)client->protocol);
	}
	memset(client, 0, sizeof(*client));
	client->size = sizeof(client);
	SAFEPRINTF(path, "%sclient.ini", cfg->node_path[number - 1]);
	fp = iniOpenFile(path, /* for_modify: */false);
	if(fp == NULL)
		return -2;
	sock = iniReadShortInt(fp, ROOT_SECTION, "sock", 0);
	client->port = iniReadShortInt(fp, ROOT_SECTION, "port", 0);
	client->time = iniReadInteger(fp, ROOT_SECTION, "time", 0);
	client->usernum = iniReadInteger(fp, ROOT_SECTION, "user", 0);
	SAFECOPY(client->addr, iniReadString(fp, ROOT_SECTION, "addr", "<none>", value));
	SAFECOPY(client->host, iniReadString(fp, ROOT_SECTION, "host", "<none>", value));
	if((p = iniReadString(fp, ROOT_SECTION, "prot", NULL, value)) != NULL)
		client->protocol = strdup(p);
	SAFECOPY(client->user, iniReadString(fp, ROOT_SECTION, "name", "<unknown>", value));
	*done = iniReadInteger(fp, ROOT_SECTION, "done", client->time);
	fclose(fp);
	return sock;
}

static bool ar_exp(scfg_t* cfg, uchar **ptrptr, user_t* user, client_t* client)
{
	bool	result,not,or,equal;
	uint	i,n,artype=AR_LEVEL,age;
	uint64_t l;
	time_t	now;
	struct tm tm;
	const char*	p;

	result = true;

	for(;(**ptrptr);(*ptrptr)++) {

		if((**ptrptr)==AR_ENDNEST)
			break;

		not=or=equal = false;

		if((**ptrptr)==AR_OR) {
			or=1;
			(*ptrptr)++;
		}

		if((**ptrptr)==AR_NOT) {
			not=1;
			(*ptrptr)++;
		}

		if((**ptrptr)==AR_EQUAL) {
			equal=1;
			(*ptrptr)++;
		}

		if((result && or) || (!result && !or))
			break;

		if((**ptrptr)==AR_BEGNEST) {
			(*ptrptr)++;
			if(ar_exp(cfg,ptrptr,user,client))
				result=!not;
			else
				result=not;
			while((**ptrptr)!=AR_ENDNEST && (**ptrptr)) /* in case of early exit */
				(*ptrptr)++;
			if(!(**ptrptr))
				break;
			continue;
		}

		artype=(**ptrptr);
		switch(artype) {
			case AR_ANSI:				/* No arguments */
			case AR_PETSCII:
			case AR_ASCII:
			case AR_UTF8:
			case AR_CP437:
			case AR_RIP:
			case AR_WIP:
			case AR_LOCAL:
			case AR_EXPERT:
			case AR_SYSOP:
			case AR_GUEST:
			case AR_QNODE:
			case AR_QUIET:
			case AR_OS2:
			case AR_DOS:
			case AR_WIN32:
			case AR_UNIX:
			case AR_LINUX:
			case AR_ACTIVE:
			case AR_INACTIVE:
			case AR_DELETED:
				break;
			default:
				(*ptrptr)++;
				break;
		}

		n=(**ptrptr);
		i=(*(short *)*ptrptr);
		switch(artype) {
			case AR_LEVEL:
				if(user==NULL
					|| (equal && user->level!=n)
					|| (!equal && user->level<n))
					result=not;
				else
					result=!not;
				break;
			case AR_AGE:
				if(user==NULL)
					result=not;
				else {
					age=getage(cfg,user->birth);
					if((equal && age!=n) || (!equal && age<n))
						result=not;
					else
						result=!not;
				}
				break;
			case AR_BPS:
				result=!not;
				(*ptrptr)++;
				break;
			case AR_ANSI:
				if(user==NULL || !(user->misc&ANSI))
					result=not;
				else result=!not;
				break;
			case AR_PETSCII:
				if(user==NULL || (user->misc&CHARSET_FLAGS) != CHARSET_PETSCII)
					result=not;
				else result=!not;
				break;
			case AR_ASCII:
				if(user==NULL || (user->misc&CHARSET_FLAGS) != CHARSET_ASCII)
					result=not;
				else result=!not;
				break;
			case AR_UTF8:
				if(user==NULL || (user->misc&CHARSET_FLAGS) != CHARSET_UTF8)
					result=not;
				else result=!not;
				break;
			case AR_CP437:
				if(user==NULL || (user->misc&CHARSET_FLAGS) != CHARSET_CP437)
					result=not;
				else result=!not;
				break;
			case AR_RIP:
				if(user==NULL || !(user->misc&RIP))
					result=not;
				else result=!not;
				break;
			case AR_WIP:
				result=not;
				break;
			case AR_OS2:
				#ifndef __OS2__
					result=not;
				#else
					result=!not;
				#endif
				break;
			case AR_DOS:
				#if defined(_WIN32) || defined(__linux__) || defined(__FreeBSD__)
					result=!not;
				#else
					result=not;
				#endif
				break;
			case AR_WIN32:
				#ifndef _WIN32
					result=not;
				#else
					result=!not;
				#endif
				break;
			case AR_UNIX:
				#ifndef __unix__
					result=not;
				#else
					result=!not;
				#endif
				break;
			case AR_LINUX:
				#ifndef __linux__
					result=not;
				#else
					result=!not;
				#endif
				break;
			case AR_ACTIVE:
				if(user==NULL || user->misc&(DELETED|INACTIVE))
					result=not;
				else result=!not;
				break;
			case AR_INACTIVE:
				if(user==NULL || !(user->misc&INACTIVE))
					result=not;
				else result=!not;
				break;
			case AR_DELETED:
				if(user==NULL || !(user->misc&DELETED))
					result=not;
				else result=!not;
				break;
			case AR_EXPERT:
				if(user==NULL || !(user->misc&EXPERT))
					result=not;
				else result=!not;
				break;
			case AR_SYSOP:
				if(!is_user_sysop(user))
					result=not;
				else result=!not;
				break;
			case AR_GUEST:
				if(user==NULL || !(user->rest&FLAG('G')))
					result=not;
				else result=!not;
				break;
			case AR_QNODE:
				if(user==NULL || !(user->rest&FLAG('Q')))
					result=not;
				else result=!not;
				break;
			case AR_QUIET:
				result=not;
				break;
			case AR_LOCAL:
				result=not;
				break;
			case AR_DAY:
				now=time(NULL);
				localtime_r(&now,&tm);
				if((equal && tm.tm_wday!=(int)n)
					|| (!equal && tm.tm_wday<(int)n))
					result=not;
				else
					result=!not;
				break;
			case AR_CREDIT:
				l = i * 1024UL;
				if(user==NULL
					|| (equal && user_available_credits(user)!=l)
					|| (!equal && user_available_credits(user)<l))
					result=not;
				else
					result=!not;
				(*ptrptr)++;
				break;
			case AR_NODE:
				if((equal && cfg->node_num!=n) || (!equal && cfg->node_num<n))
					result=not;
				else
					result=!not;
				break;
			case AR_USER:
				if(user==NULL
					|| (equal && user->number!=i)
					|| (!equal && user->number<i))
					result=not;
				else
					result=!not;
				(*ptrptr)++;
				break;
			case AR_GROUP:
				if(user==NULL)
					result=not;
				else {
					l=getgrpnum(cfg,user->cursub);
					if((equal && l!=i) || (!equal && l<i))
						result=not;
					else
						result=!not;
				}
				(*ptrptr)++;
				break;
			case AR_SUB:
				if(user==NULL)
					result=not;
				else {
					l=getsubnum(cfg,user->cursub);
					if((equal && l!=i) || (!equal && l<i))
						result=not;
					else
						result=!not;
				}
				(*ptrptr)++;
				break;
			case AR_SUBCODE:
				if(user!=NULL && findstr_in_string(user->cursub,(char *)*ptrptr)==0)
					result=!not;
				else
					result=not;
				while(*(*ptrptr))
					(*ptrptr)++;
				break;
			case AR_LIB:
				if(user==NULL)
					result=not;
				else {
					l=getlibnum(cfg,user->curdir);
					if((equal && l!=i) || (!equal && l<i))
						result=not;
					else
						result=!not;
				}
				(*ptrptr)++;
				break;
			case AR_DIR:
				if(user==NULL)
					result=not;
				else {
					l=getdirnum(cfg,user->curdir);
					if((equal && l!=i) || (!equal && l<i))
						result=not;
					else
						result=!not;
				}
				(*ptrptr)++;
				break;
			case AR_DIRCODE:
				if(user!=NULL && findstr_in_string(user->curdir,(char *)*ptrptr)==0)
					result=!not;
				else
					result=not;
				while(*(*ptrptr))
					(*ptrptr)++;
				break;
			case AR_EXPIRE:
				now=time(NULL);
				if(user==NULL
					|| user->expire==0
					|| now+((long)i*24L*60L*60L)>user->expire)
					result=not;
				else
					result=!not;
				(*ptrptr)++;
				break;
			case AR_RANDOM:
				n=xp_random(i+1);
				if((equal && n!=i) || (!equal && n<i))
					result=not;
				else
					result=!not;
				(*ptrptr)++;
				break;
			case AR_LASTON:
				now=time(NULL);
				if(user==NULL || (now-user->laston)/(24L*60L*60L)<(long)i)
					result=not;
				else
					result=!not;
				(*ptrptr)++;
				break;
			case AR_LOGONS:
				if(user==NULL
					|| (equal && user->logons!=i)
					|| (!equal && user->logons<i))
					result=not;
				else
					result=!not;
				(*ptrptr)++;
				break;
			case AR_MAIN_CMDS:
				result=not;
				(*ptrptr)++;
				break;
			case AR_FILE_CMDS:
				result=not;
				(*ptrptr)++;
				break;
			case AR_TLEFT:
				result=not;
				break;
			case AR_TUSED:
				result=not;
				break;
			case AR_TIME:
				now=time(NULL);
				localtime_r(&now,&tm);
				if((tm.tm_hour*60)+tm.tm_min<(int)i)
					result=not;
				else
					result=!not;
				(*ptrptr)++;
				break;
			case AR_PCR:
				if(user==NULL)
					result=not;
				else if(user->logons>user->posts
					&& (!user->posts || 100/((float)user->logons/user->posts)<(long)n))
					result=not;
				else
					result=!not;
				break;
			case AR_UDR:	/* up/download byte ratio */
				if(user==NULL)
					result=not;
				else {
					l=user->dlb;
					if(!l) l=1;
					if(user->dlb>user->ulb
						&& (!user->ulb || 100/((float)l/user->ulb)<n))
						result=not;
					else
						result=!not;
				}
				break;
			case AR_UDFR:	/* up/download file ratio */
				if(user==NULL)
					result=not;
				else {
					i=user->dls;
					if(!i) i=1;
					if(user->dls>user->uls
						&& (!user->uls || 100/((float)i/user->uls)<n))
						result=not;
					else
						result=!not;
				}
				break;
			case AR_ULS:
				if(user==NULL || (equal && user->uls!=i) || (!equal && user->uls<i))
					result=not;
				else
					result=!not;
				(*ptrptr)++;
				break;
			case AR_ULK:
				if(user==NULL || (equal && user->ulb/1024!=i) || (!equal && user->ulb/1024<i))
					result=not;
				else
					result=!not;
				(*ptrptr)++;
				break;
			case AR_ULM:
				if(user==NULL || (equal && user->ulb/(1024*1024)!=i) || (!equal && user->ulb/(1024*1024)<i))
					result=not;
				else
					result=!not;
				(*ptrptr)++;
				break;
			case AR_DLS:
				if(user==NULL || (equal && user->dls!=i) || (!equal && user->dls<i))
					result=not;
				else
					result=!not;
				(*ptrptr)++;
				break;
			case AR_DLK:
				if(user==NULL || (equal && user->dlb/1024!=i) || (!equal && user->dlb/1024<i))
					result=not;
				else
					result=!not;
				(*ptrptr)++;
				break;
			case AR_DLM:
				if(user==NULL || (equal && user->dlb/(1024*1024)!=i) || (!equal && user->dlb/(1024*1024)<i))
					result=not;
				else
					result=!not;
				(*ptrptr)++;
				break;
			case AR_FLAG1:
				if(user==NULL
					|| (!equal && !(user->flags1&FLAG(n)))
					|| (equal && user->flags1!=FLAG(n)))
					result=not;
				else
					result=!not;
				break;
			case AR_FLAG2:
				if(user==NULL
					|| (!equal && !(user->flags2&FLAG(n)))
					|| (equal && user->flags2!=FLAG(n)))
					result=not;
				else
					result=!not;
				break;
			case AR_FLAG3:
				if(user==NULL
					|| (!equal && !(user->flags3&FLAG(n)))
					|| (equal && user->flags3!=FLAG(n)))
					result=not;
				else
					result=!not;
				break;
			case AR_FLAG4:
				if(user==NULL
					|| (!equal && !(user->flags4&FLAG(n)))
					|| (equal && user->flags4!=FLAG(n)))
					result=not;
				else
					result=!not;
				break;
			case AR_REST:
				if(user==NULL
					|| (!equal && !(user->rest&FLAG(n)))
					|| (equal && user->rest!=FLAG(n)))
					result=not;
				else
					result=!not;
				break;
			case AR_EXEMPT:
				if(user==NULL
					|| (!equal && !(user->exempt&FLAG(n)))
					|| (equal && user->exempt!=FLAG(n)))
					result=not;
				else
					result=!not;
				break;
			case AR_SEX:
				if(user==NULL || user->sex!=n)
					result=not;
				else
					result=!not;
				break;
			case AR_SHELL:
				if(user==NULL
					|| user->shell>=cfg->total_shells
					|| !findstr_in_string(cfg->shell[user->shell]->code,(char*)*ptrptr))
					result=not;
				else
					result=!not;
				while(*(*ptrptr))
					(*ptrptr)++;
				break;
			case AR_PROT:
				if(client!=NULL)
					p=client->protocol;
				else if(user!=NULL)
					p=user->modem;
				else
					p=NULL;
				if(!findstr_in_string(p,(char*)*ptrptr))
					result=not;
				else
					result=!not;
				while(*(*ptrptr))
					(*ptrptr)++;
				break;
			case AR_HOST:
				if(client!=NULL)
					p=client->host;
				else if(user!=NULL)
					p=user->comp;
				else
					p=NULL;
				if(!findstr_in_string(p,(char*)*ptrptr))
					result=not;
				else
					result=!not;
				while(*(*ptrptr))
					(*ptrptr)++;
				break;
			case AR_IP:
				if(client!=NULL)
					p=client->addr;
				else if(user!=NULL)
					p=user->ipaddr;
				else
					p=NULL;
				if(!findstr_in_string(p,(char*)*ptrptr))
					result=not;
				else
					result=!not;
				while(*(*ptrptr))
					(*ptrptr)++;
				break;
			case AR_TERM:
				result=!not;
				while(*(*ptrptr))
					(*ptrptr)++;
				break;
			case AR_ROWS:
			case AR_COLS:
				result=!not;
				break;
		}
	}
	return(result);
}

bool chk_ar(scfg_t* cfg, uchar *ar, user_t* user, client_t* client)
{
	uchar *p;

	if(ar==NULL)
		return(true);
	if(!VALID_CFG(cfg))
		return(false);
	p=ar;
	return(ar_exp(cfg,&p,user,client));
}

/****************************************************************************/
/* Does the Access Requirement String (ARS) parse and check					*/
/****************************************************************************/
bool chk_ars(scfg_t* cfg, char *ars, user_t* user, client_t* client)
{
	uchar ar_buf[LEN_ARSTR + 1];

	return chk_ar(cfg, arstr(NULL, ars, cfg, ar_buf), user, client);
}

/****************************************************************************/
/* Returns 0 on success, non-zero on failure.								*/
/****************************************************************************/
char* getuserstr(scfg_t* cfg, int usernumber, enum user_field fnum, char *str, size_t size)
{
	char*	field[USER_FIELD_COUNT];
	char	userdat[USER_RECORD_LEN + 1];
	int		file;

	if(!VALID_CFG(cfg) || !VALID_USER_NUMBER(usernumber) || !VALID_USER_FIELD(fnum) || str == NULL)
		return NULL;

	memset(str, 0, size);
	if((file = openuserdat(cfg, /* for_modify: */false)) == -1)
		return str;

	if(readuserdat(cfg, usernumber, userdat, sizeof(userdat), file, /* leave_locked: */false) == 0) {
		split_userdat(userdat, field);
		safe_snprintf(str, size, "%s", field[fnum]);
	}
	close(file);
	dirtyuserdat(cfg, usernumber);
	return str;
}

uint32_t getuserhex32(scfg_t* cfg, int usernumber, enum user_field fnum)
{
	char str[32];

	if(getuserstr(cfg, usernumber, fnum, str, sizeof(str)) == NULL)
		return 0;

	return ahtou32(str);
}

uint32_t getuserdec32(scfg_t* cfg, int usernumber, enum user_field fnum)
{
	char str[32];

	if(getuserstr(cfg, usernumber, fnum, str, sizeof(str)) == NULL)
		return 0;

	return strtoul(str, NULL, 10);
}

uint64_t getuserdec64(scfg_t* cfg, int usernumber, enum user_field fnum)
{
	char str[32];

	if(getuserstr(cfg, usernumber, fnum, str, sizeof(str)) == NULL)
		return 0;

	return strtoull(str, NULL, 10);
}

uint32_t getusermisc(scfg_t* cfg, int usernumber)
{
	return getuserhex32(cfg, usernumber, USER_MISC);
}

uint32_t getuserchat(scfg_t* cfg, int usernumber)
{
	return getuserhex32(cfg, usernumber, USER_CHAT);
}

uint32_t getusermail(scfg_t* cfg, int usernumber)
{
	return getuserhex32(cfg, usernumber, USER_MAIL);
}

uint32_t getuserqwk(scfg_t* cfg, int usernumber)
{
	return getuserhex32(cfg, usernumber, USER_QWK);
}

uint32_t getuserflags(scfg_t* cfg, int usernumber, enum user_field fnum)
{
	char str[LEN_FLAGSTR + 1];

	if(getuserstr(cfg, usernumber, fnum, str, sizeof(str)) == NULL)
		return 0;

	return aftou32(str);
}

/****************************************************************************/
/* Returns 0 on success, non-zero on failure.								*/
/****************************************************************************/
int putuserstr(scfg_t* cfg, int usernumber, enum user_field fnum, const char *str)
{
	char*	field[USER_FIELD_COUNT];
	char	userdat[USER_RECORD_LEN + 1];
	int		file;
	int		retval;

	if(!VALID_CFG(cfg) || !VALID_USER_NUMBER(usernumber) || !VALID_USER_FIELD(fnum) || str == NULL)
		return -1;

	if(strchr(str, USER_FIELD_SEPARATOR) != NULL)
		return -2;

	if((file = openuserdat(cfg, /* for_modify: */true)) == -1)
		return errno;

	retval = readuserdat(cfg, usernumber, userdat, sizeof(userdat), file, /* leave_locked: */true);
	if(retval == 0) {
		split_userdat(userdat, field);
		field[fnum] = (char*)str;
		if(!seekuserdat(file, usernumber))
			retval = -4;
		else
			writeuserfields(cfg, field, file);
		unlockuserdat(file, usernumber);
	}
	close(file);
	dirtyuserdat(cfg, usernumber);
	return retval;
}

int putuserdatetime(scfg_t* cfg, int usernumber, enum user_field fnum, time32_t t)
{
	char str[128];

	return putuserstr(cfg, usernumber, fnum, format_datetime(t, str, sizeof(str)));
}

int putuserflags(scfg_t* cfg, int usernumber, enum user_field fnum, uint32_t flags)
{
	char str[128];

	return putuserstr(cfg, usernumber, fnum, u32toaf(flags, str));
}

int putuserhex32(scfg_t* cfg, int usernumber, enum user_field fnum, uint32_t value)
{
	char str[128];

	return putuserstr(cfg, usernumber, fnum, ultoa(value, str, 16));
}

int putuserdec32(scfg_t* cfg, int usernumber, enum user_field fnum, uint32_t value)
{
	char str[128];

	return putuserstr(cfg, usernumber, fnum, ultoa(value, str, 10));
}

int putuserdec64(scfg_t* cfg, int usernumber, enum user_field fnum, uint64_t value)
{
	char str[128];

	SAFEPRINTF(str, "%" PRIu64, value);

	return putuserstr(cfg, usernumber, fnum, str);
}

int putusermisc(scfg_t* cfg, int usernumber, uint32_t value)
{
	return putuserhex32(cfg, usernumber, USER_MISC, value);
}

int putuserchat(scfg_t* cfg, int usernumber, uint32_t value)
{
	return putuserhex32(cfg, usernumber, USER_CHAT, value);
}

int putusermail(scfg_t* cfg, int usernumber, uint32_t value)
{
	return putuserhex32(cfg, usernumber, USER_MAIL, value);
}

int putuserqwk(scfg_t* cfg, int usernumber, uint32_t value)
{
	return putuserhex32(cfg, usernumber, USER_QWK, value);
}

/****************************************************************************/
/* Updates user 'usernumber's numeric field value by adding 'adj' to it		*/
/* returns the new value.													*/
/****************************************************************************/
uint64_t adjustuserval(scfg_t* cfg, int usernumber, enum user_field fnum, int64_t adj)
{
	char value[256];
	char userdat[USER_RECORD_LEN + 1];
	int file;
	bool valid = true;
	uint64_t val = 0;

	if(!VALID_CFG(cfg) || !VALID_USER_NUMBER(usernumber) || !VALID_USER_FIELD(fnum))
		return 0;

	if((file = openuserdat(cfg, /* for_modify: */true)) == -1)
		return 0;

	if(readuserdat(cfg, usernumber, userdat, sizeof(userdat), file, /* leave_locked: */true) == 0) {
		char* field[USER_FIELD_COUNT];
		split_userdat(userdat, field);
		val = strtoull(field[fnum], NULL, 10);
		switch(user_field_len(fnum)) {
			case sizeof(uint64_t):
				if(adj < 0 && val < (uint64_t)-adj)		// don't go negative
					val = 0;
				else if(adj > 0 && val + adj < val)
					val = UINT64_MAX;
				else
					val += (uint64_t)adj;
				SAFEPRINTF(value, "%" PRIu64, val);
				break;
			case sizeof(uint32_t):
				if(adj < 0 && val < (uint32_t)-adj)		// don't go negative
					val = 0;
				else if(adj > 0 && val + adj < val)
					val = UINT32_MAX;
				else
					val += (uint32_t)adj;
				SAFEPRINTF(value, "%" PRIu32, (uint)val);
				break;
			case sizeof(uint16_t):
				if(adj < 0 && val < (uint16_t)-adj)		// don't go negative
					val = 0;
				else if(adj > 0 && val + adj < val)
					val = UINT16_MAX;
				else
					val += (uint16_t)adj;
				SAFEPRINTF(value, "%u", (uint)val);
				break;
			case sizeof(uint8_t):
				if(adj < 0 && val < (uint8_t)-adj)		// don't go negative
					val = 0;
				else if(adj > 0 && val + adj < val)
					val = UINT8_MAX;
				else
					val += (uint8_t)adj;
				SAFEPRINTF(value, "%u", (uint)val);
				break;
			default:
				valid = false;
				break;
		}
		if(valid) {
			field[fnum] = value;
			if(seekuserdat(file, usernumber))
				writeuserfields(cfg, field, file);
		}
	}
	unlockuserdat(file, usernumber);
	close(file);
	dirtyuserdat(cfg, usernumber);
	return val;
}

/****************************************************************************/
/* Returns user's available credits, handling integer overflow				*/
/****************************************************************************/
uint64_t user_available_credits(user_t* user)
{
	uint64_t result;
	if(user == NULL)
		return 0;
	result = user->cdt + user->freecdt;
	if(result < user->cdt)
		return UINT64_MAX;
	return result;
}

/****************************************************************************/
/* Subtract credits from the current user online, accounting for the new    */
/* "free credits" field.                                                    */
/****************************************************************************/
void subtract_cdt(scfg_t* cfg, user_t* user, uint64_t amt)
{
	char tmp[64];
    int64_t mod;

	if(!amt || user==NULL)
		return;
	if(user->freecdt) {
		if(amt > user->freecdt) {      /* subtract both credits and */
			mod=amt-user->freecdt;   /* free credits */
			putuserstr(cfg, user->number, USER_FREECDT, "0");
			user->freecdt=0;
			user->cdt = adjustuserval(cfg, user->number, USER_FREECDT, -mod);
		} else {                          /* subtract just free credits */
			user->freecdt-=amt;
			putuserstr(cfg, user->number, USER_FREECDT, _ui64toa(user->freecdt, tmp, 10));
		}
	}
	else {  /* no free credits */
		if(amt > INT64_MAX)
			amt = INT64_MAX;
		user->cdt = adjustuserval(cfg, user->number, USER_CDT, -(int64_t)amt);
	}
}

bool user_posted_msg(scfg_t* cfg, user_t* user, int count)
{
	if(user==NULL)
		return(false);

	user->posts	=(ushort)adjustuserval(cfg, user->number, USER_POSTS, count);
	user->ptoday=(ushort)adjustuserval(cfg, user->number, USER_PTODAY, count);

	if(user->rest & FLAG('Q'))
		return true;

	return inc_post_stats(cfg, count);
}

bool user_sent_email(scfg_t* cfg, user_t* user, int count, bool feedback)
{
	if(user==NULL)
		return(false);

	if(feedback)
		user->fbacks=(ushort)adjustuserval(cfg, user->number, USER_FBACKS, count);
	else
		user->emails=(ushort)adjustuserval(cfg, user->number, USER_EMAILS, count);
	user->etoday=(ushort)adjustuserval(cfg, user->number, USER_ETODAY, count);

	return inc_email_stats(cfg, count, feedback);
}

bool user_downloaded(scfg_t* cfg, user_t* user, int files, off_t bytes)
{
	if(user==NULL)
		return(false);

	user->dls=(ushort)adjustuserval(cfg, user->number, USER_DLS, files);
	user->dlb=adjustuserval(cfg, user->number, USER_DLB, bytes);

	return(true);
}

#ifdef SBBS
bool user_downloaded_file(scfg_t* cfg, user_t* user, client_t* client,
	int dirnum, const char* filename, off_t bytes)
{
	file_t f;
	bool removed = false;

	if(!loadfile(cfg, dirnum, filename, &f, file_detail_normal))
		return false;

	if(!bytes)
		bytes = getfilesize(cfg, &f);

	if(dirnum == cfg->user_dir) {

		str_list_t dest_user_list = strListSplitCopy(NULL, f.to_list, ",");
		char usernum[16];
		SAFEPRINTF(usernum, "%u", user->number);
		int i = strListFind(dest_user_list, usernum, /* case-sensitive: */true);
		if(i >= 0) {
			strListFastDelete(dest_user_list, i);
			char tmp[512];
			smb_hfield_replace_str(&f, RECIPIENTLIST, strListCombine(dest_user_list, tmp, sizeof(tmp), ","));
		}
		if(strListCount(dest_user_list) < 1) {
			char path[MAX_PATH + 1];
			if(remove(getfilepath(cfg, &f, path)) == 0)
				removed = removefile(cfg, dirnum, f.name);
		}
		strListFree(&dest_user_list);
	}

	f.hdr.times_downloaded++;
	f.hdr.last_downloaded = time32(NULL);
	if(!removed && !updatefile(cfg, &f)) {
		smb_freefilemem(&f);
		return false;
	}

	/**************************/
	/* Update Uploader's Info */
	/**************************/
	user_t uploader = {0};
	uploader.number=matchuser(cfg, f.from, true /*sysop_alias*/);
	if(uploader.number
		&& uploader.number != user->number 
		&& getuserdat(cfg, &uploader) == 0
		&& (uint32_t)uploader.firston < f.hdr.when_imported.time) {
		uint64_t l = f.cost;
		if(!(cfg->dir[dirnum]->misc&DIR_CDTDL))	/* Don't give credits on d/l */
			l=0;
		uint64_t mod=(uint64_t)(l*(cfg->dir[dirnum]->dn_pct/100.0));
		adjustuserval(cfg, uploader.number, USER_CDT, mod);
		if(cfg->text != NULL && !(cfg->dir[dirnum]->misc&DIR_QUIET)) {
			char str[256];
			char tmp[128];
			char prefix[128]="";
			u64toac(mod,tmp,',');
			const char* alias = user->alias[0] ? user->alias : cfg->text[UNKNOWN_USER];
			char username[64];
			if(client != NULL && is_user_sysop(&uploader)) {
				if(client->host[0] != '\0' && strcmp(client->host, STR_NO_HOSTNAME) != 0)
					SAFEPRINTF2(username,"%s [%s]", alias, client->host);
				else
					SAFEPRINTF2(username,"%s [%s]", alias, client->addr);
			} else
				SAFECOPY(username, alias);
			if(strcmp(cfg->dir[dirnum]->code, "TEMP") == 0 || bytes < getfilesize(cfg, &f))
				SAFECOPY(prefix, cfg->text[Partially]);
			if(client != NULL) {
				SAFECAT(prefix, client->protocol);
				SAFECAT(prefix, "-");
			}
			/* Inform uploader of downloaded file */
			if(mod == 0)
				SAFEPRINTF3(str, cfg->text[FreeDownloadUserMsg]
					,getfname(filename)
					,prefix
					,username);
			else
				SAFEPRINTF4(str, cfg->text[DownloadUserMsg]
					,getfname(filename)
					,prefix
					,username, tmp);
			putsmsg(cfg, uploader.number, str);
		}
	}
	/****************************/
	/* Update Downloader's Info */
	/****************************/
	user_downloaded(cfg, user, /* files: */1, bytes);
	if(!is_download_free(cfg, dirnum, user, client))
		subtract_cdt(cfg, user, f.cost);

	if(!(cfg->dir[dirnum]->misc&DIR_NOSTAT))
		inc_download_stats(cfg, /* files: */1, bytes);

	smb_freefilemem(&f);
	return true;
}
#endif

bool user_uploaded(scfg_t* cfg, user_t* user, int files, off_t bytes)
{
	if(user==NULL)
		return(false);

	user->uls=(ushort)adjustuserval(cfg, user->number, USER_ULS, files);
	user->ulb=adjustuserval(cfg, user->number, USER_ULB, bytes);

	return(true);
}

bool user_adjust_credits(scfg_t* cfg, user_t* user, int64_t amount)
{
	if(user==NULL)
		return(false);

	if(amount<0)	/* subtract */
		subtract_cdt(cfg, user, -amount);
	else			/* add */
		user->cdt=adjustuserval(cfg, user->number, USER_CDT, amount);

	return(true);
}

bool user_adjust_minutes(scfg_t* cfg, user_t* user, long amount)
{
	if(user==NULL)
		return(false);

	user->min=(uint32_t)adjustuserval(cfg, user->number, USER_MIN, amount);

	return(true);
}

/****************************************************************************/
/****************************************************************************/
bool logoutuserdat(scfg_t* cfg, user_t* user, time_t now, time_t logontime)
{
	char str[128];
	time_t tused;
	struct tm tm, tm_now;

	if(user==NULL)
		return(false);

	if(now==0)
		now=time(NULL);

	tused=(now-logontime)/60;
	user->tlast=(ushort)(tused > USHRT_MAX ? USHRT_MAX : tused);

	putuserdatetime(cfg,user->number, USER_LASTON, (time32_t)now);
	putuserstr(cfg,user->number, USER_TLAST, ultoa(user->tlast,str,10));
	adjustuserval(cfg,user->number, USER_TIMEON, user->tlast);
	adjustuserval(cfg,user->number, USER_TTODAY, user->tlast);

	/* Convert time_t to struct tm */
	if(localtime_r(&now,&tm_now)==NULL)
		return(false);

	if(localtime_r(&logontime,&tm)==NULL)
		return(false);

	/* Reset daily stats if new day */
	if(tm.tm_mday!=tm_now.tm_mday)
		resetdailyuserdat(cfg, user, /* write: */true);

	return(true);
}

/****************************************************************************/
/****************************************************************************/
void resetdailyuserdat(scfg_t* cfg, user_t* user, bool write)
{
	if(!VALID_CFG(cfg) || user==NULL)
		return;

	/* logons today */
	user->ltoday=0;
	if(write) putuserstr(cfg,user->number, USER_LTODAY, "0");
	/* e-mails today */
	user->etoday=0;
	if(write) putuserstr(cfg,user->number, USER_ETODAY, "0");
	/* posts today */
	user->ptoday=0;
	if(write) putuserstr(cfg,user->number, USER_PTODAY, "0");
	/* free credits per day */
	user->freecdt=cfg->level_freecdtperday[user->level];
	if(write) putuserdec64(cfg,user->number, USER_FREECDT, user->freecdt);
	/* time used today */
	user->ttoday=0;
	if(write) putuserstr(cfg,user->number, USER_TTODAY, "0");
	/* extra time today */
	user->textra=0;
	if(write) putuserstr(cfg,user->number, USER_TEXTRA, "0");
}

/****************************************************************************/
/* Get dotted-equivalent email address for user 'name'.						*/
/* 'addr' is the target buffer for the full address.						*/
/* Pass cfg=NULL to NOT have "@address" portion appended.					*/
/****************************************************************************/
char* usermailaddr(scfg_t* cfg, char* addr, const char* name)
{
	int i;

	if(addr==NULL || name==NULL)
		return(NULL);

	if(strchr(name,'@')!=NULL) { /* Avoid double-@ */
		strcpy(addr,name);
		return(addr);
	}
	if(strchr(name,'.') && strchr(name,' ')) {
		/* convert "Dr. Seuss" to "Dr.Seuss" */
		strip_space(name,addr);
	} else if(strchr(name,'!')) {
		sprintf(addr,"\"%s\"",name);
	} else {
		strcpy(addr,name);
		/* convert "first last" to "first.last" */
		for(i=0;addr[i];i++)
			if(addr[i]==' ' || addr[i]&0x80)
				addr[i]='.';
	}
	if(VALID_CFG(cfg)) {
		strcat(addr,"@");
		strcat(addr,cfg->sys_inetaddr);
	}
	return(addr);
}

/****************************************************************************/
/* Convert a sender's name/address from a message header into Synchronet	*/
/* SMTP-server-routable form												*/
/****************************************************************************/
void smtp_netmailaddr(scfg_t* cfg, smbmsg_t* msg, char* name, size_t namelen, char* addr, size_t addrlen)
{
	char addrbuf[256];

	if(name != NULL)
		snprintf(name, namelen, "\"%s\"", msg->from);
	if(msg->from_net.type==NET_QWK && msg->from_net.addr!=NULL)
		snprintf(addr, addrlen, "%s!%s"
			,(char*)msg->from_net.addr
			,usermailaddr(cfg, addrbuf, msg->from));
	else if(msg->from_net.type==NET_FIDO && msg->from_net.addr!=NULL) {
		faddr_t* faddr = (faddr_t *)msg->from_net.addr;
		char faddrstr[128];
		if(name != NULL)
			snprintf(name, namelen, "\"%s\" (%s)", msg->from, smb_faddrtoa(faddr, faddrstr));
		if(faddr->point)
			SAFEPRINTF4(faddrstr,"p%hu.f%hu.n%hu.z%hu"FIDO_TLD
				,faddr->point, faddr->node, faddr->net, faddr->zone);
		else
			SAFEPRINTF3(faddrstr,"f%hu.n%hu.z%hu"FIDO_TLD
				,faddr->node, faddr->net, faddr->zone);
		snprintf(addr, addrlen, "%s@%s", usermailaddr(NULL, addrbuf, msg->from), faddrstr);
	} else if(msg->from_net.type!=NET_NONE && msg->from_net.addr!=NULL)
		snprintf(addr, addrlen, "%s", (char*)msg->from_net.addr);
	else 
		usermailaddr(cfg, addr, msg->from);
}

char* alias(scfg_t* cfg, const char* name, char* buf)
{
	char	line[128];
	char*	p;
	char*	np;
	char*	tp;
	char*	vp;
	char	fname[MAX_PATH+1];
	size_t	namelen;
	size_t	cmplen;
	FILE*	fp;

	if(!VALID_CFG(cfg) || name==NULL || buf==NULL)
		return(NULL);

	p=(char*)name;

	SAFEPRINTF(fname,"%salias.cfg",cfg->ctrl_dir);
	if((fp = fnopen(NULL, fname, O_RDONLY)) == NULL)
		return((char*)name);

	while(!feof(fp)) {
		if(!fgets(line,sizeof(line),fp))
			break;
		np=line;
		SKIP_WHITESPACE(np);
		if(*np==';' || *np==0)	/* no name value, or comment */
			continue;
		tp=np;
		FIND_WHITESPACE(tp);

		if(*tp==0)	/* no alias value */
			continue;
		*tp=0;

		vp=tp+1;
		SKIP_WHITESPACE(vp);
		truncsp(vp);
		if(*vp==0)	/* no value */
			continue;

		if(*np=='*') {
			np++;
			cmplen=strlen(np);
			namelen=strlen(name);
			if(namelen<cmplen)
				continue;
			if(strnicmp(np,name+(namelen-cmplen),cmplen))
				continue;
			if(*vp=='*')
				sprintf(buf,"%.*s%s",(int)(namelen-cmplen),name,vp+1);
			else
				strcpy(buf,vp);
			p=buf;
			break;
		}
		if(!stricmp(np,name)) {
			strcpy(buf,vp);
			p=buf;
			break;
		}
	}
	fclose(fp);
	return(p);
}

int newuserdefaults(scfg_t* cfg, user_t* user)
{
	int i;

	user->sex = ' ';

	/* statistics */
	user->firston=user->laston=user->pwmod=time32(NULL);

	/* security */
	user->level=cfg->new_level;
	user->flags1=cfg->new_flags1;
	user->flags2=cfg->new_flags2;
	user->flags3=cfg->new_flags3;
	user->flags4=cfg->new_flags4;
	user->rest=cfg->new_rest;
	user->exempt=cfg->new_exempt;

	user->cdt=cfg->new_cdt;
	user->min=cfg->new_min;
	user->freecdt=cfg->level_freecdtperday[user->level];
	if(cfg->new_expire)
		user->expire=user->firston+((long)cfg->new_expire*24L*60L*60L);
	else
		user->expire=0;

	/* settings */
	if(cfg->total_fcomps)
		SAFECOPY(user->tmpext,cfg->fcomp[0]->ext);
	else
		SAFECOPY(user->tmpext,"zip");

	user->shell = cfg->new_shell;
	user->misc = cfg->new_misc|(AUTOTERM|COLOR);
	user->misc &= ~(DELETED|INACTIVE|QUIET|NETMAIL);
	user->prot = cfg->new_prot;
	user->qwk = cfg->new_qwk;
	user->chat = cfg->new_chat;

	for(i=0;i<cfg->total_xedits;i++)
		if(!stricmp(cfg->xedit[i]->code,cfg->new_xedit) && chk_ar(cfg,cfg->xedit[i]->ar, user, /* client: */NULL))
			break;
	if(i<cfg->total_xedits)
		user->xedit=i+1;

	return 0;
}

int newuserdat(scfg_t* cfg, user_t* user)
{
	char	str[MAX_PATH+1];
	char	tmp[128];
	int		c;
	int		i;
	int		err;
	int		file;
	int		unum=1;
	int		last;
	FILE*	stream;
	stats_t	stats;

	if(!VALID_CFG(cfg) || user==NULL)
		return(-1);

	SAFEPRINTF(str,"%suser/name.dat",cfg->data_dir);
	if(fexist(str)) {
		if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
			return(errno);
		}
		last=(long)filelength(file)/(LEN_ALIAS+2);	   /* total users */
		while(unum<=last) {
			if(fread(str,LEN_ALIAS+2,1,stream) != 1)
				memset(str, ETX, LEN_ALIAS);
			for(c=0;c<LEN_ALIAS;c++)
				if(str[c]==ETX) break;
			str[c]=0;
			if(!c) {	 /* should be a deleted user */
				user_t deluser;
				deluser.number = unum;
				if(getuserdat(cfg, &deluser) == 0) {
					if(deluser.misc&DELETED) {	 /* deleted bit set too */
						if((time(NULL)-deluser.laston)/86400>=cfg->sys_deldays)
							break; /* deleted long enough ? */
					}
				}
			}
			unum++;
		}
		fclose(stream);
	}

	last=lastuser(cfg);		/* Check against data file */

	if(unum>last+1) 		/* Corrupted name.dat? */
		unum=last+1;
	else if(unum<=last) {	/* Overwriting existing user */
		user_t deluser;
		deluser.number = unum;
		if(getuserdat(cfg, &deluser) == 0) {
			if(!(deluser.misc&DELETED)) /* Not deleted? Set usernumber to end+1 */
				unum=last+1;
		}
	}

	user->number=unum;		/* store the new user number */

	if((err=putusername(cfg,user->number,user->alias))!=0)
		return(err);

	if((err=putuserdat(cfg,user))!=0)
		return(err);

	SAFEPRINTF2(str,"%sfile/%04u.in",cfg->data_dir,user->number);  /* delete any files */
	delfiles(str, ALLFILES, /* keep: */0);                         /* waiting for user */
	rmdir(str);
	SAFEPRINTF(tmp,"%04u.*",user->number);
	SAFEPRINTF(str,"%sfile",cfg->data_dir);
	delfiles(str,tmp, /* keep: */0);
	SAFEPRINTF(str,"%suser",cfg->data_dir);
	delfiles(str,tmp, /* keep: */0);
	SAFEPRINTF(str,"%smsgs",cfg->data_dir);
	delfiles(str,tmp, /* keep: */0);
	SAFEPRINTF2(str,"%suser/%04u",cfg->data_dir,user->number);
	delfiles(str,ALLFILES, /* keep: */0);
	rmdir(str);

	SAFEPRINTF2(str,"%suser/ptrs/%04u.ixb",cfg->data_dir,user->number); /* legacy msg ptrs */
	remove(str);

	/* Update daily statistics database (for system and node) */

	for(i=0;i<2;i++) {
		FILE* fp = fopen_dstats(cfg, i ? cfg->node_num : 0, /* for_write: */true);
		if(fp == NULL)
			continue;
		if(fread_dstats(fp, &stats)) {
			stats.today.nusers++;
			stats.total.nusers++;
			fwrite_dstats(fp, &stats);
		}
		fclose_dstats(fp);
	}

	return(0);
}

size_t user_field_len(enum user_field fnum)
{
	user_t user;

	switch(fnum) {
		case USER_ALIAS:	return sizeof(user.alias) - 1;
		case USER_NAME:		return sizeof(user.name) - 1;
		case USER_HANDLE:	return sizeof(user.handle) -1;
		case USER_NOTE:		return sizeof(user.note) - 1;
		case USER_IPADDR:	return sizeof(user.ipaddr) - 1;
		case USER_HOST:		return sizeof(user.comp) - 1;
		case USER_NETMAIL:	return sizeof(user.netmail) - 1;
		case USER_ADDRESS:	return sizeof(user.address) - 1;
		case USER_LOCATION:	return sizeof(user.location) - 1;
		case USER_ZIPCODE:	return sizeof(user.zipcode) - 1;
		case USER_PHONE:	return sizeof(user.phone) - 1;
		case USER_BIRTH:	return sizeof(user.birth) - 1;
		case USER_GENDER:	return sizeof(user.sex);
		case USER_COMMENT:	return sizeof(user.comment) - 1;
		case USER_CONNECTION: return sizeof(user.modem) -1;

		// Bit-fields:
		case USER_MISC:		return sizeof(user.misc);
		case USER_QWK:		return sizeof(user.qwk);
		case USER_CHAT:		return sizeof(user.chat);

		// Settings:
		case USER_ROWS:		return sizeof(user.rows);
		case USER_COLS:		return sizeof(user.cols);
		case USER_XEDIT:	return sizeof(user.xedit);
		case USER_SHELL:	return sizeof(user.shell);
		case USER_TMPEXT:	return sizeof(user.tmpext) - 1;
		case USER_PROT:		return sizeof(user.prot);
		case USER_CURSUB:	return sizeof(user.cursub) - 1;
		case USER_CURDIR:	return sizeof(user.curdir) - 1;
		case USER_CURXTRN:	return sizeof(user.curxtrn) - 1;

		// Date/times:
		case USER_LOGONTIME:	return sizeof(user.logontime);
		case USER_NS_TIME:		return sizeof(user.ns_time);
		case USER_LASTON:		return sizeof(user.laston);
		case USER_FIRSTON:		return sizeof(user.firston);

		// Counting stats:
		case USER_LOGONS:		return sizeof(user.logons);
		case USER_LTODAY:		return sizeof(user.ltoday);
		case USER_TIMEON:		return sizeof(user.timeon);
		case USER_TTODAY:		return sizeof(user.ttoday);
		case USER_TLAST:		return sizeof(user.tlast);
		case USER_POSTS:		return sizeof(user.posts);
		case USER_EMAILS:		return sizeof(user.emails);
		case USER_FBACKS:		return sizeof(user.fbacks);
		case USER_ETODAY:		return sizeof(user.etoday);
		case USER_PTODAY:		return sizeof(user.ptoday);

		// File xfer stats:
		case USER_ULB:			return sizeof(user.ulb);
		case USER_ULS:			return sizeof(user.uls);
		case USER_DLB:			return sizeof(user.dlb);
		case USER_DLS:			return sizeof(user.dls);
		case USER_DLCPS:		return sizeof(user.dlcps);
		case USER_LEECH:		return sizeof(user.leech);

		// Security:
		case USER_PASS:			return sizeof(user.pass) - 1;
		case USER_PWMOD:		return sizeof(user.pwmod);
		case USER_LEVEL:		return sizeof(user.level);
		case USER_FLAGS1:		return sizeof(user.flags1);
		case USER_FLAGS2:		return sizeof(user.flags2);
		case USER_FLAGS3:		return sizeof(user.flags3);
		case USER_FLAGS4:		return sizeof(user.flags4);
		case USER_EXEMPT:		return sizeof(user.exempt);
		case USER_REST:			return sizeof(user.rest);
		case USER_CDT:			return sizeof(user.cdt);
		case USER_FREECDT:		return sizeof(user.freecdt);
		case USER_MIN:			return sizeof(user.min);
		case USER_TEXTRA:		return sizeof(user.textra);
		case USER_EXPIRE:		return sizeof(user.expire);

		default:			return 0;
	}
}

/****************************************************************************/
/* Determine if the specified user can or cannot access the specified sub	*/
/****************************************************************************/
bool can_user_access_sub(scfg_t* cfg, int subnum, user_t* user, client_t* client)
{
	if(!VALID_CFG(cfg))
		return false;
	if(!is_valid_subnum(cfg, subnum))
		return false;
	if(!chk_ar(cfg,cfg->grp[cfg->sub[subnum]->grp]->ar,user,client))
		return false;
	if(!chk_ar(cfg,cfg->sub[subnum]->ar,user,client))
		return false;

	return true;
}

/****************************************************************************/
/* Determine if the specified user can or cannot read the specified sub		*/
/****************************************************************************/
bool can_user_read_sub(scfg_t* cfg, int subnum, user_t* user, client_t* client)
{
	if(!can_user_access_sub(cfg, subnum, user, client))
		return false;
	return chk_ar(cfg,cfg->sub[subnum]->read_ar,user,client);
}

/****************************************************************************/
/* Determine if the specified user can or cannot post on the specified sub	*/
/* 'reason' is an (optional) pointer to a text.dat item number, indicating	*/
/* the reason the user cannot post, when returning false.					*/
/****************************************************************************/
bool can_user_post(scfg_t* cfg, int subnum, user_t* user, client_t* client, uint* reason)
{
	if(reason!=NULL)
		*reason=CantPostOnSub;
	if(!can_user_access_sub(cfg, subnum, user, client))
		return false;
	if(!chk_ar(cfg,cfg->sub[subnum]->post_ar,user,client))
		return false;
	if(cfg->sub[subnum]->misc&(SUB_QNET|SUB_FIDO|SUB_PNET|SUB_INET)
		&& user->rest&FLAG('N'))		/* network restriction? */
		return false;
	if((cfg->sub[subnum]->misc & SUB_NAME)
		&& (user->rest & (FLAG('Q') | FLAG('O'))) == FLAG('O'))
		return false;
	if(reason!=NULL)
		*reason=R_Post;
	if(user->rest&FLAG('P'))			/* post restriction? */
		return false;
	if(reason!=NULL)
		*reason=TooManyPostsToday;
	if(user->ptoday>=cfg->level_postsperday[user->level])
		return false;

	return true;
}

/****************************************************************************/
// Determine if the specified user can access one or more directories of lib
/****************************************************************************/
bool can_user_access_lib(scfg_t* cfg, int libnum, user_t* user, client_t* client)
{
	uint count = 0;

	for(int dirnum = 0; dirnum < cfg->total_dirs; dirnum++) {
		if(cfg->dir[dirnum]->lib != libnum)
			continue;
		if(can_user_access_dir(cfg, dirnum, user, client)) // checks lib's AR already
			count++;
	}
	return count >= 1; // User has access to one or more directories of library
}

/****************************************************************************/
// Determine if the specified user can access ALL file libraries
/****************************************************************************/
bool can_user_access_all_libs(scfg_t* cfg, user_t* user, client_t* client)
{
	for(int libnum = 0; libnum < cfg->total_libs; libnum++) {
		if(!can_user_access_lib(cfg, libnum, user, client))
			return false;
	}
	return true;
}

/****************************************************************************/
// Determine if the specified user can all dirs of a lib
/****************************************************************************/
bool can_user_access_all_dirs(scfg_t* cfg, int libnum, user_t* user, client_t* client)
{
	uint count = 0;

	for(int dirnum = 0; dirnum < cfg->total_dirs; dirnum++) {
		if(cfg->dir[dirnum]->lib != libnum)
			continue;
		if(can_user_access_dir(cfg, dirnum, user, client)) // checks lib's AR already
			count++;
		else
			return false;
	}
	return count >= 1; // User has access to one or more directories of library
}

/****************************************************************************/
/* Determine if the specified user can or cannot access the specified dir	*/
/****************************************************************************/
bool can_user_access_dir(scfg_t* cfg, int dirnum, user_t* user, client_t* client)
{
	if(!VALID_CFG(cfg))
		return false;
	if(!is_valid_dirnum(cfg, dirnum))
		return false;
	if(!chk_ar(cfg,cfg->lib[cfg->dir[dirnum]->lib]->ar,user,client))
		return false;
	if(!chk_ar(cfg,cfg->dir[dirnum]->ar,user,client))
		return false;

	return true;
}

/****************************************************************************/
/* Determine if the specified user can or cannot upload files to the dirnum	*/
/* 'reason' is an (optional) pointer to a text.dat item number, indicating	*/
/* the reason the user cannot post, when returning false.					*/
/****************************************************************************/
bool can_user_upload(scfg_t* cfg, int dirnum, user_t* user, client_t* client, uint* reason)
{
	if(reason!=NULL)
		*reason=CantUploadHere;
	if(!can_user_access_dir(cfg, dirnum, user, client))
		return false;
	if(reason!=NULL)
		*reason=R_Upload;
	if(user->rest&FLAG('U'))			/* upload restriction? */
		return false;
	if(user->rest&FLAG('T'))			/* transfer restriction? */
		return false;
	if(!(user->exempt&FLAG('U'))		/* upload exemption */
		&& !is_user_dirop(cfg, dirnum, user, client)) {
		if(reason!=NULL)
			*reason=CantUploadHere;
		if(!chk_ar(cfg, cfg->lib[cfg->dir[dirnum]->lib]->ul_ar, user, client))
			return false;
		if(!chk_ar(cfg, cfg->dir[dirnum]->ul_ar, user, client))
			return false;
	}
	return true;
}

/****************************************************************************/
/* Determine if the specified user can or cannot download files from dirnum	*/
/* 'reason' is an (optional) pointer to a text.dat item number, indicating	*/
/* the reason the user cannot post, when returning false.					*/
/****************************************************************************/
bool can_user_download(scfg_t* cfg, int dirnum, user_t* user, client_t* client, uint* reason)
{
	if(reason!=NULL)
		*reason=CantDownloadFromDir;
	if(dirnum != cfg->user_dir && !can_user_access_dir(cfg, dirnum, user, client))
		return false;
	if(!chk_ar(cfg,cfg->lib[cfg->dir[dirnum]->lib]->dl_ar,user,client))
		return false;
	if(!chk_ar(cfg,cfg->dir[dirnum]->dl_ar,user,client))
		return false;
	if(reason!=NULL)
		*reason=R_Download;
	if(user->rest&FLAG('D'))			/* download restriction? */
		return false;
	if(user->rest&FLAG('T'))			/* transfer restriction? */
		return false;

	return true;
}

/****************************************************************************/
/* Determine if the specified user can or cannot send email					*/
/* 'reason' is an (optional) pointer to a text.dat item number				*/
/* usernumber==0 for netmail												*/
/****************************************************************************/
bool can_user_send_mail(scfg_t* cfg, enum smb_net_type net_type, uint usernumber, user_t* user, uint* reason)
{
	if(reason!=NULL)
		*reason=R_Email;
	if(user==NULL || user->number==0)
		return false;
	if(net_type==NET_NONE && usernumber>1 && user->rest&FLAG('E'))			/* local mail restriction? */
		return false;
	if(reason!=NULL)
		*reason=NoNetMailAllowed;
	if(net_type!=NET_NONE && user->rest&FLAG('M'))							/* netmail restriction */
		return false;
	if(net_type==NET_FIDO && !(cfg->netmail_misc&NMAIL_ALLOW))				/* Fido netmail globally disallowed */
		return false;
	if(net_type==NET_INTERNET && !(cfg->inetmail_misc&NMAIL_ALLOW))			/* Internet mail globally disallowed */
		return false;
	if(reason!=NULL)
		*reason=R_Feedback;
	if(net_type==NET_NONE && usernumber==1 && user->rest&FLAG('S'))			/* feedback restriction? */
		return false;
	if(reason!=NULL)
		*reason=TooManyEmailsToday;
	if(user->etoday>=cfg->level_emailperday[user->level] && !(user->exempt&FLAG('M')))
		return false;

	return true;
}

/****************************************************************************/
/* Determine if the specified user is a system operator						*/
/****************************************************************************/
bool is_user_sysop(user_t* user)
{
	if(user == NULL)
		return false;
	return user->level >= SYSOP_LEVEL;
}

/****************************************************************************/
/* Determine if the specified user is a sub-board operator					*/
/****************************************************************************/
bool is_user_subop(scfg_t* cfg, int subnum, user_t* user, client_t* client)
{
	if(user==NULL)
		return false;
	if(!can_user_access_sub(cfg, subnum, user, client))
		return false;
	if(is_user_sysop(user))
		return true;

	return cfg->sub[subnum]->op_ar[0]!=0 && chk_ar(cfg,cfg->sub[subnum]->op_ar,user,client);
}

/****************************************************************************/
/* Determine if the specified user is a directory operator					*/
/****************************************************************************/
bool is_user_dirop(scfg_t* cfg, int dirnum, user_t* user, client_t* client)
{
	if(user==NULL)
		return false;
	if(!can_user_access_dir(cfg, dirnum, user, client))
		return false;
	if(is_user_sysop(user))
		return true;

	return (cfg->dir[dirnum]->op_ar[0]!=0 && chk_ar(cfg,cfg->dir[dirnum]->op_ar,user,client))
			|| (cfg->lib[cfg->dir[dirnum]->lib]->op_ar[0]!=0 && chk_ar(cfg,cfg->lib[cfg->dir[dirnum]->lib]->op_ar,user,client));
}

/****************************************************************************/
/* Determine if downloads from the specified directory are free for the		*/
/* specified user															*/
/****************************************************************************/
bool is_download_free(scfg_t* cfg, int dirnum, user_t* user, client_t* client)
{
	if(!VALID_CFG(cfg))
		return(false);

	if(!is_valid_dirnum(cfg, dirnum))
		return(false);

	if(cfg->dir[dirnum]->misc&DIR_FREE)
		return(true);

	if(user==NULL)
		return(false);

	if(user->exempt&FLAG('D'))
		return(true);

	if(cfg->lib[cfg->dir[dirnum]->lib]->ex_ar[0] != 0
		&& chk_ar(cfg,cfg->lib[cfg->dir[dirnum]->lib]->ex_ar,user,client))
		return true;

	if(cfg->dir[dirnum]->ex_ar[0]==0)
		return(false);

	return(chk_ar(cfg,cfg->dir[dirnum]->ex_ar,user,client));
}

/****************************************************************************/
/* Note: This function does not account for timed events!					*/
/****************************************************************************/
time_t gettimeleft(scfg_t* cfg, user_t* user, time_t starttime)
{
	time_t	now;
    long    tleft;
	time_t	timeleft;

	now=time(NULL);

	if(user->exempt&FLAG('T')) {	/* Time online exemption */
		timeleft=cfg->level_timepercall[user->level];
		if(timeleft<10)             /* never get below 10 minutes for exempt users */
			timeleft=10;
		timeleft*=60;				/* convert to seconds */
	}
	else {
		tleft=(((long)cfg->level_timeperday[user->level]-user->ttoday)
			+user->textra)*60L;
		if(tleft<0) tleft=0;
		if(tleft>cfg->level_timepercall[user->level]*60)
			tleft=cfg->level_timepercall[user->level]*60;
		tleft+=user->min*60L;
		long tused = (long)MAX(now - starttime, 0);
		tleft -= tused;
		if(tleft < 0)
			tleft = 0;
		if(tleft>0x7fffL)
			timeleft=0x7fff;
		else
			timeleft=tleft;
	}

	return(timeleft);
}

/*************************************************************************/
/* Check a supplied name/alias and see if it's valid by our standards.   */
/*************************************************************************/
bool check_name(scfg_t* cfg, const char* name)
{
	char	tmp[512];
	size_t	len;

	if(name == NULL)
		return false;

	len=strlen(name);
	if(len<1)
		return false;
	if (   name[0] <= ' '			/* begins with white-space? */
		|| name[len-1] <= ' '		/* ends with white-space */
		|| !IS_ALPHA(name[0])
		|| !stricmp(name,cfg->sys_id)
		|| strchr(name,0xff)
		|| matchuser(cfg,name,true /* sysop_alias */)
		|| trashcan(cfg,name,"name")
		|| alias(cfg,name,tmp)!=name
 	   )
 		return false;
 	return true;
}

/*************************************************************************/
/* Check a supplied real name and see if it's valid by our standards.   */
/*************************************************************************/
bool check_realname(scfg_t* cfg, const char* name)
{
	if(name == NULL)
		return false;

	return (uchar)name[0]<0x7f && name[1] && IS_ALPHA(name[0]) && strchr(name,' ');
}

/****************************************************************************/
/* Login attempt/hack tracking												*/
/****************************************************************************/

/****************************************************************************/
link_list_t* loginAttemptListInit(link_list_t* list)
{
	return listInit(list, LINK_LIST_MUTEX);
}

/****************************************************************************/
bool loginAttemptListFree(link_list_t* list)
{
	return listFree(list);
}

/****************************************************************************/
/* Returns negative value on failure										*/
/****************************************************************************/
long loginAttemptListCount(link_list_t* list)
{
	long count;

	if(!listLock(list))
		return -1;
	count = listCountNodes(list);
	listUnlock(list);
	return count;
}

/****************************************************************************/
/* Returns number of items (attempts) removed from the list					*/
/* Returns negative value on failure										*/
/****************************************************************************/
long loginAttemptListClear(link_list_t* list)
{
	long count;

	if(!listLock(list))
		return -1;
	count=listCountNodes(list);
	count-=listFreeNodes(list);
	listUnlock(list);
	return count;
}

/****************************************************************************/
static list_node_t* login_attempted(link_list_t* list, const union xp_sockaddr* addr)
{
	list_node_t*		node;
	login_attempt_t*	attempt;

	if(list==NULL)
		return NULL;
	for(node=list->first; node!=NULL; node=node->next) {
		attempt=node->data;
		if(attempt->addr.addr.sa_family != addr->addr.sa_family)
			continue;
		switch(addr->addr.sa_family) {
			case AF_INET:
				if(memcmp(&attempt->addr.in.sin_addr, &addr->in.sin_addr, sizeof(addr->in.sin_addr)) == 0)
					return node;
				break;
			case AF_INET6:
				if(memcmp(&attempt->addr.in6.sin6_addr, &addr->in6.sin6_addr, sizeof(addr->in6.sin6_addr)) == 0)
					return node;
				break;
		}
	}
	return NULL;
}

/****************************************************************************/
/* Returns negative value on failure										*/
/****************************************************************************/
long loginAttempts(link_list_t* list, const union xp_sockaddr* addr)
{
	long				count=0;
	list_node_t*		node;

	if(addr->addr.sa_family != AF_INET && addr->addr.sa_family != AF_INET6)
		return 0;
	if(!listLock(list))
		return -1;
	if((node=login_attempted(list, addr))!=NULL)
		count = ((login_attempt_t*)node->data)->count - ((login_attempt_t*)node->data)->dupes;
	listUnlock(list);

	return count;
}

/****************************************************************************/
void loginSuccess(link_list_t* list, const union xp_sockaddr* addr)
{
	list_node_t*		node;

	if(addr->addr.sa_family != AF_INET && addr->addr.sa_family != AF_INET6)
		return;
	listLock(list);
	if((node=login_attempted(list, addr)) != NULL)
		listRemoveNode(list, node, /* freeData: */true);
	listUnlock(list);
}

/****************************************************************************/
/* Returns number of *unique* login attempts (excludes consecutive dupes)	*/
/****************************************************************************/
ulong loginFailure(link_list_t* list, const union xp_sockaddr* addr, const char* prot, const char* user, const char* pass, login_attempt_t* details)
{
	list_node_t*		node;
	login_attempt_t		first;
	login_attempt_t*	attempt=&first;
	ulong				count=0;

	if(addr->addr.sa_family != AF_INET && addr->addr.sa_family != AF_INET6)
		return 0;
	if(list==NULL)
		return 0;
	memset(&first, 0, sizeof(first));
	if(!listLock(list))
		return 0;
	if((node=login_attempted(list, addr)) != NULL) {
		attempt=node->data;
		/* Don't count consecutive duplicate attempts (same name and password): */
		if((user!=NULL && strcmp(attempt->user,user)==0) && (pass!=NULL && strcmp(attempt->pass,pass)==0))
			attempt->dupes++;
	}
	SAFECOPY(attempt->prot,prot);
	attempt->time=time32(NULL);
	memcpy(&attempt->addr, addr, sizeof(*addr));
	if(user != NULL)
		SAFECOPY(attempt->user, user);
	memset(attempt->pass, 0, sizeof(attempt->pass));
	if(pass != NULL)
		SAFECOPY(attempt->pass, pass);
	attempt->count++;
	count = attempt->count - attempt->dupes;
	if(node==NULL) {
		attempt->first = attempt->time;
		listPushNodeData(list, attempt, sizeof(login_attempt_t));
	}
	listUnlock(list);

	if (details != NULL)
		*details = *attempt;

	return count;
}

#if !defined(NO_SOCKET_SUPPORT)
ulong loginBanned(scfg_t* cfg, link_list_t* list, SOCKET sock, const char* host_name
	,struct login_attempt_settings settings, login_attempt_t* details)
{
	char				ip_addr[128];
	char				name[(LEN_ALIAS * 2) + 1];
	list_node_t*		node;
	login_attempt_t*	attempt;
	bool				result = false;
	time32_t			now = time32(NULL);
	union xp_sockaddr	client_addr;
	union xp_sockaddr	server_addr;
	socklen_t			addr_len;
	char				exempt[MAX_PATH+1];

	SAFEPRINTF2(exempt, "%s%s", cfg->ctrl_dir, strIpFilterExemptConfigFile);

	if(list==NULL)
		return 0;

	addr_len=sizeof(server_addr);
	if((result=getsockname(sock, &server_addr.addr, &addr_len)) != 0)
		return 0;

	addr_len=sizeof(client_addr);
	if((result=getpeername(sock, &client_addr.addr, &addr_len)) != 0)
		return 0;

	/* Don't ban connections from the server back to itself */
	if(inet_addrmatch(&server_addr, &client_addr))
		return 0;

	if(inet_addrtop(&client_addr, ip_addr, sizeof(ip_addr)) != NULL
		&& find2strs(ip_addr, host_name, exempt, NULL))
		return 0;

	if(!listLock(list))
		return 0;
	node = login_attempted(list, &client_addr);
	listUnlock(list);
	if(node == NULL)
		return 0;
	attempt = node->data;
	SAFECOPY(name, attempt->user);
	truncstr(name, "@");
	if(((settings.tempban_threshold && (attempt->count - attempt->dupes) >= settings.tempban_threshold)
		|| trashcan(cfg, name, "name")) && now < (time32_t)(attempt->time + settings.tempban_duration)) {
		if(details != NULL)
			*details = *attempt;
		return settings.tempban_duration - (now - attempt->time);
	}
	return 0;
}

/****************************************************************************/
/* Message-new-scan pointer/configuration functions							*/
/****************************************************************************/
bool getmsgptrs(scfg_t* cfg, user_t* user, subscan_t* subscan, void (*progress)(void*, int, int), void* cbdata)
{
	char		path[MAX_PATH+1];
	int			i;
	int 		file;
	long		length;
	FILE*		stream;

	/* Initialize to configured defaults */
	for(i=0;i<cfg->total_subs;i++) {
		subscan[i].ptr=subscan[i].sav_ptr=0;
		subscan[i].last=subscan[i].sav_last=0;
		subscan[i].cfg=0xff;
		if(!(cfg->sub[i]->misc&SUB_NSDEF))
			subscan[i].cfg&=~SUB_CFG_NSCAN;
		if(!(cfg->sub[i]->misc&SUB_SSDEF))
			subscan[i].cfg&=~SUB_CFG_SSCAN;
		subscan[i].sav_cfg=subscan[i].cfg;
	}

	if(user->number == 0)
		return 0;

	if(user->rest&FLAG('G'))
		return initmsgptrs(cfg, subscan, cfg->guest_msgscan_init, progress, cbdata);

	/* New way: */
	SAFEPRINTF2(path,"%suser/%4.4u.subs", cfg->data_dir, user->number);
	FILE* fp = fnopen(NULL, path, O_RDONLY|O_TEXT);
	if (fp != NULL) {
		str_list_t ini = iniReadFile(fp);
		for(i = 0; i < cfg->total_subs; i++) {
			if(progress != NULL)
				progress(cbdata, i, cfg->total_subs);
			str_list_t keys = iniGetSection(ini, cfg->sub[i]->code);
			if(keys == NULL)
				continue;
			subscan[i].ptr	= iniGetUInt32(keys, NULL, "ptr"	, subscan[i].ptr);
			subscan[i].last	= iniGetUInt32(keys, NULL, "last"	, subscan[i].last);
			subscan[i].cfg	= iniGetShortInt(keys, NULL, "cfg"	, subscan[i].cfg);
			subscan[i].cfg &= (SUB_CFG_NSCAN|SUB_CFG_SSCAN|SUB_CFG_YSCAN);	// Sanitize the 'cfg' value
			subscan[i].sav_ptr	= subscan[i].ptr;
			subscan[i].sav_last	= subscan[i].last;
			subscan[i].sav_cfg	= subscan[i].cfg;
			iniFreeStringList(keys);
			iniRemoveSection(&ini, cfg->sub[i]->code);
		}
		iniFreeStringList(ini);
		fclose(fp);
		if(progress != NULL)
			progress(cbdata, i, cfg->total_subs);
		return true;
	}

	/* Old way: */
	SAFEPRINTF2(path,"%suser/ptrs/%4.4u.ixb", cfg->data_dir, user->number);
	if((stream=fnopen(&file,path,O_RDONLY))==NULL) {
		if(fexist(path))
			return(false);	/* file exists, but couldn't be opened? */
		return initmsgptrs(cfg, subscan, cfg->new_msgscan_init, progress, cbdata);
	}

	length=(long)filelength(file);
	for(i=0;i<cfg->total_subs;i++) {
		if(progress != NULL)
			progress(cbdata, i, cfg->total_subs);
		if(length>=(cfg->sub[i]->ptridx+1)*10L) {
			fseek(stream,(long)cfg->sub[i]->ptridx*10L,SEEK_SET);
			if(fread(&subscan[i].ptr,sizeof(subscan[i].ptr),1,stream) != 1)
				break;
			if(fread(&subscan[i].last,sizeof(subscan[i].last),1,stream) != 1)
				break;
			if(fread(&subscan[i].cfg,sizeof(subscan[i].cfg),1,stream) != 1)
				break;
		}
		subscan[i].sav_ptr=subscan[i].ptr;
		subscan[i].sav_last=subscan[i].last;
		subscan[i].sav_cfg=subscan[i].cfg;
	}
	if(progress != NULL)
		progress(cbdata, i, cfg->total_subs);
	fclose(stream);
	return(true);
}

/****************************************************************************/
/* Writes to data/user/####.subs the msgptr array for the current user		*/
/* Pass usernumber value of 0 to indicate "Guest" login						*/
/****************************************************************************/
bool putmsgptrs(scfg_t* cfg, user_t* user, subscan_t* subscan)
{
	char		path[MAX_PATH+1];
	int			i;
	time_t		now = time(NULL);
	bool		result = true;

	if(user->number==0 || (user->rest&FLAG('G')))	/* Guest */
		return(true);

	fixmsgptrs(cfg, subscan);
	SAFEPRINTF2(path,"%suser/%4.4u.subs", cfg->data_dir, user->number);
	FILE* fp = fnopen(NULL, path, O_RDWR|O_CREAT|O_TEXT);
	if (fp == NULL)
		return false;
	str_list_t new = strListInit();
	str_list_t ini = iniReadFile(fp);
	ini_style_t ini_style = { .key_prefix = "\t", .section_separator = "" };
	bool modified = false;
	for(i=0; i < cfg->total_subs; i++) {
		str_list_t keys = iniGetSection(ini, cfg->sub[i]->code);
		if(subscan[i].sav_ptr==subscan[i].ptr
			&& subscan[i].sav_last==subscan[i].last
			&& subscan[i].sav_cfg==subscan[i].cfg
			&& keys != NULL && *keys != NULL)
			iniAppendSectionWithKeys(&new, cfg->sub[i]->code, keys, &ini_style);
		else {
			iniSetUInt32(&new, cfg->sub[i]->code, "ptr", subscan[i].ptr, &ini_style);
			iniSetUInt32(&new, cfg->sub[i]->code, "last", subscan[i].last, &ini_style);
			iniSetHexInt(&new, cfg->sub[i]->code, "cfg", subscan[i].cfg, &ini_style);
			iniSetDateTime(&new, cfg->sub[i]->code, "updated", /* include_time: */true, now, &ini_style);
			modified = true;
		}
		if(keys != NULL) {
			iniRemoveSection(&ini, cfg->sub[i]->code);
			iniFreeStringList(keys);
		}
	}
	if(modified || strListCount(ini))
		result = iniWriteFile(fp, new);
	strListFree(&new);
	iniFreeStringList(ini);
	fclose(fp);

	return result;
}

bool newmsgs(smb_t* smb, time_t t)
{
	char index_fname[MAX_PATH + 1];

	SAFEPRINTF(index_fname, "%s.sid", smb->file);
	return fdate(index_fname) >= t;
}

/****************************************************************************/
/* Initialize new-msg-scan pointers (e.g. for new users)					*/
/* If 'days' is specified as 0, just set pointer to last message (faster)	*/
/****************************************************************************/
bool initmsgptrs(scfg_t* cfg, subscan_t* subscan, unsigned days, void (*progress)(void*, int, int), void* cbdata)
{
	int			i;
	smb_t		smb;
	idxrec_t	idx;
	time_t		t = time(NULL) - (days * 24 * 60 * 60);

	for(i=0;i<cfg->total_subs;i++) {
		if(progress != NULL)
			progress(cbdata, i, cfg->total_subs);
		/* This value will be "fixed" (changed to the last msg) when saving */
		subscan[i].ptr = ~0;
		if(days == 0)
			continue;
		ZERO_VAR(smb);
		SAFEPRINTF2(smb.file,"%s%s",cfg->sub[i]->data_dir,cfg->sub[i]->code);

		if(!newmsgs(&smb, t))
			continue;

		smb.retry_time=cfg->smb_retry_time;
		smb.subnum=i;
		if(smb_open_index(&smb) != SMB_SUCCESS)
			continue;
		memset(&idx, 0, sizeof(idx));
		smb_getlastidx(&smb, &idx);
		subscan[i].ptr = idx.number;
		if(idx.time >= t && smb_getmsgidx_by_time(&smb, &idx, t) >= SMB_SUCCESS)
			subscan[i].ptr = idx.number - 1;
		smb_close(&smb);
	}
	if(progress != NULL)
		progress(cbdata, i, cfg->total_subs);
	return true;
}

/****************************************************************************/
/* Insure message new-scan pointers are within the range of the msgs in		*/
/* the sub-board.															*/
/****************************************************************************/
bool fixmsgptrs(scfg_t* cfg, subscan_t* subscan)
{
	int			i;
	smb_t		smb;

	for(i=0;i<cfg->total_subs;i++) {
		if(subscan[i].ptr == 0)
			continue;
		if(subscan[i].ptr < ~0 && subscan[i].sav_ptr == subscan[i].ptr)
			continue;
		ZERO_VAR(smb);
		SAFEPRINTF2(smb.file,"%s%s",cfg->sub[i]->data_dir,cfg->sub[i]->code);
		smb.retry_time=cfg->smb_retry_time;
		smb.subnum=i;
		if(smb_open_index(&smb) != SMB_SUCCESS) {
			subscan[i].ptr = 0;
			continue;
		}
		idxrec_t idx;
		memset(&idx, 0, sizeof(idx));
		smb_getlastidx(&smb, &idx);
		if(subscan[i].ptr > idx.number)
			subscan[i].ptr = idx.number;
		if(subscan[i].last > idx.number)
			subscan[i].last = idx.number;
		smb_close(&smb);
	}
	return true;
}

static char* sysop_available_semfile(scfg_t* scfg)
{
	static char semfile[MAX_PATH+1];
	SAFEPRINTF(semfile, "%ssysavail.chat", scfg->ctrl_dir);
	return semfile;
}

bool sysop_available(scfg_t* scfg)
{
	return fexist(sysop_available_semfile(scfg));
}

bool set_sysop_availability(scfg_t* scfg, bool available)
{
	if(available)
		return ftouch(sysop_available_semfile(scfg));
	return remove(sysop_available_semfile(scfg)) == 0;
}

static char* sound_muted_semfile(scfg_t* scfg)
{
	static char semfile[MAX_PATH+1];
	SAFEPRINTF(semfile, "%ssound.mute", scfg->ctrl_dir);
	return semfile;
}

bool sound_muted(scfg_t* scfg)
{
	return fexist(sound_muted_semfile(scfg));
}

bool set_sound_muted(scfg_t* scfg, bool muted)
{
	if(muted)
		return ftouch(sound_muted_semfile(scfg));
	return remove(sound_muted_semfile(scfg)) == 0;
}

/************************************/
/* user .ini file get/set functions */
/************************************/

static FILE* user_ini_open(scfg_t* scfg, unsigned user_number, bool for_modify)
{
	char path[MAX_PATH+1];

	SAFEPRINTF2(path, "%suser/%04u.ini", scfg->data_dir, user_number);
	return iniOpenFile(path, for_modify);
}

bool user_get_property(scfg_t* scfg, unsigned user_number, const char* section, const char* key, char* value, size_t maxlen)
{
	FILE* fp;
	char buf[INI_MAX_VALUE_LEN];

	fp = user_ini_open(scfg, user_number, /* for_modify: */false);
	if(fp == NULL)
		return false;
	char* result = iniReadValue(fp, section, key, NULL, buf);
	if(result != NULL)
		safe_snprintf(value, maxlen, "%s", result);
	iniCloseFile(fp);
	return result != NULL;
}

bool user_set_property(scfg_t* scfg, unsigned user_number, const char* section, const char* key, const char* value)
{
	FILE* fp;
	str_list_t ini;

	fp = user_ini_open(scfg, user_number, /* for_modify: */true);
	if(fp == NULL)
		return false;
	ini = iniReadFile(fp);
	ini_style_t ini_style = { .key_prefix = "\t", .section_separator = "", .value_separator = " = " };
	char* result = iniSetValue(&ini, section, key, value, &ini_style);
	iniWriteFile(fp, ini);
	iniFreeStringList(ini);
	iniCloseFile(fp);
	return result != NULL;
}

bool user_set_time_property(scfg_t* scfg, unsigned user_number, const char* section, const char* key, time_t value)
{
	FILE* fp;
	str_list_t ini;

	fp = user_ini_open(scfg, user_number, /* for_modify: */true);
	if(fp == NULL)
		return false;
	ini = iniReadFile(fp);
	ini_style_t ini_style = { .key_prefix = "\t", .section_separator = "", .value_separator = " = " };
	char* result = iniSetDateTime(&ini, section, key, /* include_time */true, value, &ini_style);
	iniWriteFile(fp, ini);
	iniFreeStringList(ini);
	iniCloseFile(fp);
	return result != NULL;
}

#endif /* !NO_SOCKET_SUPPORT */

/****************************************************************************/
/* Returns user number or 0 on failure or "user not found".					*/
/****************************************************************************/
int lookup_user(scfg_t* cfg, link_list_t* list, const char *inname)
{
	if(inname == NULL || *inname == 0)
		return 0;

	if(list->first == NULL) {
		user_t user;
		int userdat = openuserdat(cfg, /* modify */false);
		if(userdat < 0)
			return 0;

		for(user.number = 1; ;user.number++) {
			if(fgetuserdat(cfg, &user, userdat) != 0)
				break;
			if(user.misc&DELETED)
				continue;
			listPushNodeData(list, &user, sizeof(user));
		}
		close(userdat);
	}
	for(list_node_t* node = listFirstNode(list); node != NULL; node = node->next) {
		if(matchusername(cfg, ((user_t*)node->data)->alias, inname))
			return ((user_t*)node->data)->number;
	}
	for(list_node_t* node = listFirstNode(list); node != NULL; node = node->next) {
		if(matchusername(cfg, ((user_t*)node->data)->name, inname))
			return ((user_t*)node->data)->number;
	}
	return 0;
}

/* Parse a virtual filebase path of the form "[/]lib[/dir][/filename]" (e.g. main/games/filename.ext) */
enum parsed_vpath parse_vpath(scfg_t* cfg, const char* vpath, user_t* user, client_t* client, bool include_upload_only
	,int* lib, int* dir, char** filename)
{
	char*	p;
	char*	tp;
	char	path[MAX_PATH+1];

	*lib = -1;
	*dir = -1;

	SAFECOPY(path, vpath);
	p=path;

	if(*p=='/') 
		p++;
	if(!strncmp(p,"./",2))
		p+=2;

	if(*p == '\0')
		return PARSED_VPATH_ROOT;

	tp=strchr(p,'/');
	if(tp) *tp=0;
	for(*lib = 0; *lib < cfg->total_libs; (*lib)++) {
		if(!chk_ar(cfg,cfg->lib[*lib]->ar,user,client))
			continue;
		if(!stricmp(cfg->lib[*lib]->vdir,p))
			break;
	}
	if(*lib >= cfg->total_libs)
		return PARSED_VPATH_NONE;
	if(tp == NULL || *(tp + 1) == '\0') 
		return PARSED_VPATH_LIB;

	p=tp+1;
	tp=strchr(p,'/');
	if(tp) {
		*tp=0;
		if(*(tp + 1) != '\0')
			*filename = getfname(vpath);
	}
	for(*dir = 0; *dir < cfg->total_dirs; (*dir)++) {
		if(cfg->dir[*dir]->lib != *lib)
			continue;
		if((!include_upload_only || (*dir != cfg->sysop_dir && *dir != cfg->upload_dir))
			&& !chk_ar(cfg,cfg->dir[*dir]->ar,user,client))
			continue;
		if(!stricmp(cfg->dir[*dir]->vdir,p))
			break;
	}
	if(*dir >= cfg->total_dirs) 
		return PARSED_VPATH_NONE;

	return *filename == NULL ? PARSED_VPATH_DIR : PARSED_VPATH_FULL;
}
