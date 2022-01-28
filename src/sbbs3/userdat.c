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
#include "cmdshell.h"
#include "userdat.h"
#include "filedat.h"
#include "ars_defs.h"
#include "text.h"
#include "nopen.h"
#include "datewrap.h"
#include "date_str.h"
#include "smblib.h"
#include "getstats.h"
#include "msgdate.h"
#include "scfglib.h"

#ifndef USHRT_MAX
	#define USHRT_MAX ((unsigned short)~0)
#endif

/* convenient space-saving global variables */
static const char* crlf="\r\n";
static const char* nulstr="";
static const char* strIpFilterExemptConfigFile = "ipfilter_exempt.cfg";

#define VALID_CFG(cfg)	(cfg!=NULL && cfg->size==sizeof(scfg_t))

/****************************************************************************/
/* Looks for a perfect match among all usernames (not deleted users)		*/
/* Makes dots and underscores synonymous with spaces for comparisons		*/
/* Returns the number of the perfect matched username or 0 if no match		*/
/****************************************************************************/
uint matchuser(scfg_t* cfg, const char *name, BOOL sysop_alias)
{
	int		file,c;
	char	dat[LEN_ALIAS+2];
	char	str[256];
	ulong	l,length;
	FILE*	stream;

	if(!VALID_CFG(cfg) || name==NULL)
		return(0);

	if(sysop_alias &&
		(!stricmp(name,"SYSOP") || !stricmp(name,"POSTMASTER") || !stricmp(name,cfg->sys_id)))
		return(1);

	SAFEPRINTF(str,"%suser/name.dat",cfg->data_dir);
	if((stream=fnopen(&file,str,O_RDONLY))==NULL)
		return(0);
	length=(long)filelength(file);
	for(l=0;l<length;l+=LEN_ALIAS+2) {
		fread(dat,sizeof(dat),1,stream);
		for(c=0;c<LEN_ALIAS;c++)
			if(dat[c]==ETX) break;
		dat[c]=0;
		if(matchusername(cfg, dat, name))
			break;
	}
	fclose(stream);
	if(l<length)
		return((l/(LEN_ALIAS+2))+1);
	return(0);
}

/****************************************************************************/
/* Return TRUE if the user 'name' (or alias) matches with 'comp'			*/
/* ... ignoring non-alpha-numeric chars in either string					*/
/* and terminating the comparison string at an '@'							*/
/****************************************************************************/
BOOL matchusername(scfg_t* cfg, const char* name, const char* comp)
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
uint total_users(scfg_t* cfg)
{
    char	str[MAX_PATH+1];
    uint	total_users=0;
	int		file;
    long	l,length;

	if(!VALID_CFG(cfg))
		return(0);

	if((file=openuserdat(cfg, /* for_modify: */FALSE)) < 0)
		return(0);
	length=(long)filelength(file);
	for(l=0;l<length;l+=U_LEN) {
		lseek(file,l+U_MISC,SEEK_SET);
		if(read(file,str,8)!=8)
			continue;
		getrec(str,0,8,str);
		if(ahtoul(str)&(DELETED|INACTIVE))
			continue;
		total_users++;
	}
	close(file);
	return(total_users);
}


/****************************************************************************/
/* Returns the number of the last user in user.dat (deleted ones too)		*/
/****************************************************************************/
uint lastuser(scfg_t* cfg)
{
	char str[256];
	long length;

	if(!VALID_CFG(cfg))
		return(0);

	SAFEPRINTF(str,"%suser/user.dat", cfg->data_dir);
	if((length=(long)flength(str))>0)
		return((uint)(length/U_LEN));
	return(0);
}

/****************************************************************************/
/* Deletes (completely removes) last user record in user.dat				*/
/****************************************************************************/
BOOL del_lastuser(scfg_t* cfg)
{
	int		file;
	long	length;

	if(!VALID_CFG(cfg))
		return(FALSE);

	if((file=openuserdat(cfg, /* for_modify: */TRUE)) < 0)
		return(FALSE);
	length=(long)filelength(file);
	if(length<U_LEN) {
		close(file);
		return(FALSE);
	}
	chsize(file,length-U_LEN);
	close(file);
	return(TRUE);
}

/****************************************************************************/
/* Opens the user database returning the file descriptor or -1 on error		*/
/****************************************************************************/
int openuserdat(scfg_t* cfg, BOOL for_modify)
{
	char path[MAX_PATH+1];

	if(!VALID_CFG(cfg))
		return(-1);

	SAFEPRINTF(path,"%suser/user.dat",cfg->data_dir);
	return nopen(path, for_modify ? (O_RDWR|O_CREAT|O_DENYNONE) : (O_RDONLY|O_DENYNONE));
}

int closeuserdat(int file)
{
	if(file < 1)
		return -1;
	return close(file);
}

/****************************************************************************/
/* Locks and reads a single user record from an open user.dat file into a	*/
/* buffer of U_LEN+1 in size.												*/
/* Returns 0 on success.													*/
/****************************************************************************/
int readuserdat(scfg_t* cfg, unsigned user_number, char* userdat, int infile)
{
	int i,file;

	if(!VALID_CFG(cfg) || user_number<1)
		return(-1);

	if(infile >= 0)
		file = infile;
	else {
		if((file = openuserdat(cfg, /* for_modify: */FALSE)) < 0)
			return file;
	}

	if(user_number > (unsigned)(filelength(file)/U_LEN)) {
		if(file != infile)
			close(file);
		return(-1);	/* no such user record */
	}

	lseek(file,(long)((long)(user_number-1)*U_LEN),SEEK_SET);
	i=0;
	while(i<LOOP_NODEDAB
		&& lock(file,(long)((long)(user_number-1)*U_LEN),U_LEN)==-1) {
		if(i)
			mswait(100);
		i++;
	}
	if(i>=LOOP_NODEDAB) {
		if(file != infile)
			close(file);
		return(-2);
	}

	if(read(file,userdat,U_LEN)!=U_LEN) {
		unlock(file,(long)((long)(user_number-1)*U_LEN),U_LEN);
		if(file != infile)
			close(file);
		return(-3);
	}
	unlock(file,(long)((long)(user_number-1)*U_LEN),U_LEN);
	if(file != infile)
		close(file);
	return 0;
}

/****************************************************************************/
/* Fills the structure 'user' with info for user.number	from userdat		*/
/* (a buffer representing a single user 'record' from the user.dat file		*/
/****************************************************************************/
int parseuserdat(scfg_t* cfg, char *userdat, user_t *user)
{
	char str[U_LEN+1];
	int i;
	unsigned user_number;

	if(user==NULL)
		return(-1);

	user_number=user->number;
	memset(user,0,sizeof(user_t));

	if(!VALID_CFG(cfg) || user_number < 1)
		return(-1);

	/* The user number needs to be set here
	   before calling chk_ar() below for user-number comparisons in AR strings to function correctly */
	user->number=user_number;	/* Signal of success */

	/* order of these function calls is irrelevant */
	getrec(userdat,U_ALIAS,LEN_ALIAS,user->alias);
	getrec(userdat,U_NAME,LEN_NAME,user->name);
	getrec(userdat,U_HANDLE,LEN_HANDLE,user->handle);
	getrec(userdat,U_NOTE,LEN_NOTE,user->note);
	getrec(userdat,U_COMP,LEN_COMP,user->comp);
	getrec(userdat,U_COMMENT,LEN_COMMENT,user->comment);
	getrec(userdat,U_NETMAIL,LEN_NETMAIL,user->netmail);
	getrec(userdat,U_ADDRESS,LEN_ADDRESS,user->address);
	getrec(userdat,U_LOCATION,LEN_LOCATION,user->location);
	getrec(userdat,U_ZIPCODE,LEN_ZIPCODE,user->zipcode);
	getrec(userdat,U_PASS,LEN_PASS,user->pass);
	if(user->pass[0] == 0)	// Backwards compatibility hack
		getrec(userdat, U_OLDPASS, LEN_OLDPASS, user->pass);
	getrec(userdat,U_PHONE,LEN_PHONE,user->phone);
	getrec(userdat,U_BIRTH,LEN_BIRTH,user->birth);
	getrec(userdat,U_MODEM,LEN_MODEM,user->modem);
	getrec(userdat,U_IPADDR,LEN_IPADDR,user->ipaddr);
	getrec(userdat,U_LASTON,8,str); user->laston=ahtoul(str);
	getrec(userdat,U_FIRSTON,8,str); user->firston=ahtoul(str);
	getrec(userdat,U_EXPIRE,8,str); user->expire=ahtoul(str);
	getrec(userdat,U_PWMOD,8,str); user->pwmod=ahtoul(str);
	getrec(userdat,U_NS_TIME,8,str);
	user->ns_time=ahtoul(str);
	if(user->ns_time<0x20000000L)
		user->ns_time=user->laston;  /* Fix for v2.00->v2.10 */
	getrec(userdat,U_LOGONTIME,8,str); user->logontime=ahtoul(str);

	getrec(userdat,U_LOGONS,5,str); user->logons=atoi(str);
	getrec(userdat,U_LTODAY,5,str); user->ltoday=atoi(str);
	getrec(userdat,U_TIMEON,5,str); user->timeon=atoi(str);
	getrec(userdat,U_TEXTRA,5,str); user->textra=atoi(str);
	getrec(userdat,U_TTODAY,5,str); user->ttoday=atoi(str);
	getrec(userdat,U_TLAST,5,str); user->tlast=atoi(str);
	getrec(userdat,U_POSTS,5,str); user->posts=atoi(str);
	getrec(userdat,U_EMAILS,5,str); user->emails=atoi(str);
	getrec(userdat,U_FBACKS,5,str); user->fbacks=atoi(str);
	getrec(userdat,U_ETODAY,5,str); user->etoday=atoi(str);
	getrec(userdat,U_PTODAY,5,str); user->ptoday=atoi(str);
	getrec(userdat,U_ULB,10,str); user->ulb=strtoul(str, NULL, 10);
	getrec(userdat,U_ULS,5,str); user->uls=atoi(str);
	getrec(userdat,U_DLB,10,str); user->dlb=strtoul(str, NULL, 10);
	getrec(userdat,U_DLS,5,str); user->dls=atoi(str);
	getrec(userdat,U_CDT,10,str); user->cdt=strtoul(str, NULL, 10);
	getrec(userdat,U_MIN,10,str); user->min=strtoul(str, NULL, 10);
	getrec(userdat,U_LEVEL,2,str); user->level=atoi(str);
	getrec(userdat,U_FLAGS1,8,str); user->flags1=ahtoul(str);
	getrec(userdat,U_FLAGS2,8,str); user->flags2=ahtoul(str);
	getrec(userdat,U_FLAGS3,8,str); user->flags3=ahtoul(str);
	getrec(userdat,U_FLAGS4,8,str); user->flags4=ahtoul(str);
	getrec(userdat,U_EXEMPT,8,str); user->exempt=ahtoul(str);
	getrec(userdat,U_REST,8,str); user->rest=ahtoul(str);
	if(getrec(userdat,U_OLDROWS,2,str) <= 0) // Moved to new location
		getrec(userdat, U_ROWS, LEN_ROWS, str);
	user->rows = atoi(str);
	if(user->rows && user->rows < TERM_ROWS_MIN)
		user->rows = TERM_ROWS_MIN;
	getrec(userdat, U_COLS, LEN_COLS, str);
	user->cols = atoi(str);
	if(user->cols && user->cols < TERM_COLS_MIN)
		user->cols = TERM_COLS_MIN;
	user->sex=userdat[U_SEX];
	if(!user->sex)
		user->sex=' ';	 /* fix for v1b04 that could save as 0 */
	user->prot=userdat[U_PROT];
	if(user->prot<' ')
		user->prot=' ';
	getrec(userdat,U_MISC,8,str); user->misc=ahtoul(str);
	if(user->rest&FLAG('Q'))
		user->misc&=~SPIN;

	getrec(userdat,U_LEECH,2,str);
	user->leech=(uchar)ahtoul(str);
	getrec(userdat,U_CURSUB,sizeof(user->cursub)-1,user->cursub);
	getrec(userdat,U_CURDIR,sizeof(user->curdir)-1,user->curdir);
	getrec(userdat,U_CURXTRN,8,user->curxtrn);

	getrec(userdat,U_FREECDT,10,str);
	user->freecdt=strtoul(str, NULL, 10);

	getrec(userdat,U_XEDIT,8,str);
	for(i=0;i<cfg->total_xedits;i++)
		if(!stricmp(str,cfg->xedit[i]->code))
			break;
	user->xedit=i+1;
	if(user->xedit>cfg->total_xedits)
		user->xedit=0;

	getrec(userdat,U_SHELL,8,str);
	for(i=0;i<cfg->total_shells;i++)
		if(!stricmp(str,cfg->shell[i]->code))
			break;
	if(i==cfg->total_shells)
		i=0;
	user->shell=i;

	getrec(userdat,U_QWK,8,str);
	if(str[0]<' ') { 			   /* v1c, so set defaults */
		if(user->rest&FLAG('Q'))
			user->qwk=QWK_DEFAULT|QWK_RETCTLA;
		else
			user->qwk=QWK_DEFAULT;
	}
	else
		user->qwk=ahtoul(str);

	getrec(userdat,U_TMPEXT,3,user->tmpext);
	if((!user->tmpext[0] || !strcmp(user->tmpext,"0")) && cfg->total_fcomps)
		strcpy(user->tmpext,cfg->fcomp[0]->ext);  /* For v1x to v2x conversion */

	getrec(userdat,U_CHAT,8,str);
	user->chat=ahtoul(str);
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
				resetdailyuserdat(cfg,user,/* write: */FALSE);
		}
	}
	return(0);
}

/****************************************************************************/
/* Fills the structure 'user' with info for user.number	from user.dat file	*/
/****************************************************************************/
int getuserdat(scfg_t* cfg, user_t *user)
{
	int		retval;
	int		file;
	char	userdat[U_LEN+1];

	if(!VALID_CFG(cfg) || user==NULL || user->number < 1)
		return(-1);

	if((file = openuserdat(cfg, /* for_modify: */FALSE)) < 0) {
		user->number = 0;
		return file;
	}

	memset(userdat, 0, sizeof(userdat));
	if((retval = readuserdat(cfg, user->number, userdat, file)) != 0) {
		close(file);
		user->number = 0;
		return retval;
	}
	retval = parseuserdat(cfg, userdat, user);
	close(file);
	return retval;
}

/* Fast getuserdat() (leaves user.dat file open) */
int fgetuserdat(scfg_t* cfg, user_t *user, int file)
{
	int		retval;
	char	userdat[U_LEN+1];

	if(!VALID_CFG(cfg) || user==NULL || user->number < 1)
		return(-1);

	memset(userdat, 0, sizeof(userdat));
	if((retval = readuserdat(cfg, user->number, userdat, file)) != 0) {
		user->number = 0;
		return retval;
	}
	return parseuserdat(cfg, userdat, user);
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
		if(getnodedat(cfg, i,&node, /* lockit: */FALSE, &file) != 0)
			continue;
		if(node.useron==usernumber && (node.status==NODE_INUSE
			|| node.status==NODE_QUIET)) {
			if(getnodedat(cfg, i,&node, /* lockit: */TRUE, &file) == 0) {
				node.misc|=NODE_UDAT;
				putnodedat(cfg, i,&node, /* closeit: */FALSE, file);
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
		getnodedat(cfg, i, &node, /* lockit: */FALSE, &file);
		if((node.status==NODE_INUSE || node.status==NODE_QUIET
			|| node.status==NODE_LOGON) && node.useron==usernumber)
			return i;
	}
	CLOSE_OPEN_FILE(file);
	return 0;
}

/****************************************************************************/
/* Writes into user.number's slot in user.dat data in structure 'user'      */
/* Called from functions newuser, useredit and main                         */
/****************************************************************************/
int putuserdat(scfg_t* cfg, user_t* user)
{
    int		i,file;
    char	userdat[U_LEN],str[MAX_PATH+1];

	if(user==NULL)
		return(-1);

	if(!VALID_CFG(cfg) || user->number<1)
		return(-1);

	memset(userdat,ETX,U_LEN);
	putrec(userdat,U_ALIAS,LEN_ALIAS+5,user->alias);
	putrec(userdat,U_NAME,LEN_NAME,user->name);
	putrec(userdat,U_HANDLE,LEN_HANDLE,user->handle);
	putrec(userdat,U_HANDLE+LEN_HANDLE,2,crlf);

	putrec(userdat,U_NOTE,LEN_NOTE,user->note);
	putrec(userdat,U_COMP,LEN_COMP,user->comp);
	putrec(userdat,U_COMP+LEN_COMP,2,crlf);

	putrec(userdat,U_COMMENT,LEN_COMMENT,user->comment);
	putrec(userdat,U_COMMENT+LEN_COMMENT,2,crlf);

	putrec(userdat,U_NETMAIL,LEN_NETMAIL,user->netmail);
	putrec(userdat,U_NETMAIL+LEN_NETMAIL,2,crlf);

	putrec(userdat,U_ADDRESS,LEN_ADDRESS,user->address);
	putrec(userdat,U_LOCATION,LEN_LOCATION,user->location);
	putrec(userdat,U_ZIPCODE,LEN_ZIPCODE,user->zipcode);
	putrec(userdat,U_ZIPCODE+LEN_ZIPCODE,2,crlf);

	putrec(userdat,U_PASS,LEN_PASS,user->pass);
	putrec(userdat,U_OLDPASS,LEN_OLDPASS,user->pass);	// So a sysop can downgrade to a previous build/version
	putrec(userdat,U_PHONE,LEN_PHONE,user->phone);
	putrec(userdat,U_BIRTH,LEN_BIRTH,user->birth);
	putrec(userdat,U_MODEM,LEN_MODEM,user->modem);
	putrec(userdat,U_IPADDR,LEN_IPADDR,user->ipaddr);
	putrec(userdat,U_LASTON,8,ultoa((ulong)user->laston,str,16));
	putrec(userdat,U_FIRSTON,8,ultoa((ulong)user->firston,str,16));
	putrec(userdat,U_EXPIRE,8,ultoa((ulong)user->expire,str,16));
	putrec(userdat,U_PWMOD,8,ultoa((ulong)user->pwmod,str,16));
	putrec(userdat,U_PWMOD+8,2,crlf);

	putrec(userdat,U_LOGONS,5,ultoa(user->logons,str,10));
	putrec(userdat,U_LTODAY,5,ultoa(user->ltoday,str,10));
	putrec(userdat,U_TIMEON,5,ultoa(user->timeon,str,10));
	putrec(userdat,U_TEXTRA,5,ultoa(user->textra,str,10));
	putrec(userdat,U_TTODAY,5,ultoa(user->ttoday,str,10));
	putrec(userdat,U_TLAST,5,ultoa(user->tlast,str,10));
	putrec(userdat,U_POSTS,5,ultoa(user->posts,str,10));
	putrec(userdat,U_EMAILS,5,ultoa(user->emails,str,10));
	putrec(userdat,U_FBACKS,5,ultoa(user->fbacks,str,10));
	putrec(userdat,U_ETODAY,5,ultoa(user->etoday,str,10));
	putrec(userdat,U_PTODAY,5,ultoa(user->ptoday,str,10));
	putrec(userdat,U_PTODAY+5,2,crlf);

	putrec(userdat,U_ULB,10,ultoa(user->ulb,str,10));
	putrec(userdat,U_ULS,5,ultoa(user->uls,str,10));
	putrec(userdat,U_DLB,10,ultoa(user->dlb,str,10));
	putrec(userdat,U_DLS,5,ultoa(user->dls,str,10));
	putrec(userdat,U_CDT,10,ultoa(user->cdt,str,10));
	putrec(userdat,U_MIN,10,ultoa(user->min,str,10));
	putrec(userdat,U_MIN+10,2,crlf);

	putrec(userdat,U_LEVEL,2,ultoa(user->level,str,10));
	putrec(userdat,U_FLAGS1,8,ultoa(user->flags1,str,16));
	putrec(userdat,U_TL,2,nulstr);	/* unused */
	putrec(userdat,U_FLAGS2,8,ultoa(user->flags2,str,16));
	putrec(userdat,U_EXEMPT,8,ultoa(user->exempt,str,16));
	putrec(userdat,U_REST,8,ultoa(user->rest,str,16));
	putrec(userdat,U_REST+8,2,crlf);

	putrec(userdat, U_ROWS, LEN_ROWS, ultoa(user->rows,str,10));
	putrec(userdat, U_COLS, LEN_COLS, ultoa(user->cols,str,10));
	userdat[U_SEX]=user->sex;
	userdat[U_PROT]=user->prot;
	putrec(userdat,U_MISC,8,ultoa(user->misc,str,16));
	putrec(userdat,U_LEECH,2,ultoa(user->leech,str,16));

	putrec(userdat,U_CURSUB,sizeof(user->cursub)-1,user->cursub);
	putrec(userdat,U_CURDIR,sizeof(user->curdir)-1,user->curdir);
	putrec(userdat,U_CURXTRN,8,user->curxtrn);
	putrec(userdat,U_CURXTRN+8,2,crlf);

	putrec(userdat,U_PASS+LEN_PASS, 2, crlf);

	putrec(userdat,U_IPADDR+LEN_IPADDR,2,crlf);

	putrec(userdat,U_FREECDT,10,ultoa(user->freecdt,str,10));

	putrec(userdat,U_FLAGS3,8,ultoa(user->flags3,str,16));
	putrec(userdat,U_FLAGS4,8,ultoa(user->flags4,str,16));

	if(user->xedit)
		putrec(userdat,U_XEDIT,8,cfg->xedit[user->xedit-1]->code);
	else
		putrec(userdat,U_XEDIT,8,nulstr);

	putrec(userdat,U_SHELL,8,cfg->shell[user->shell]->code);

	putrec(userdat,U_QWK,8,ultoa(user->qwk,str,16));
	putrec(userdat,U_TMPEXT,3,user->tmpext);
	putrec(userdat,U_CHAT,8,ultoa(user->chat,str,16));
	putrec(userdat,U_NS_TIME,8,ultoa((ulong)user->ns_time,str,16));
	putrec(userdat,U_LOGONTIME,8,ultoa((ulong)user->logontime,str,16));

	putrec(userdat,U_UNUSED,U_LEN-(U_UNUSED)-2,crlf);
	putrec(userdat,U_UNUSED+(U_LEN-(U_UNUSED)-2),2,crlf);

	if((file=openuserdat(cfg, /* for_modify: */TRUE)) < 0)
		return(errno);

	if(filelength(file)<((long)user->number-1)*U_LEN) {
		close(file);
		return(-4);
	}

	lseek(file,(long)((long)((long)user->number-1)*U_LEN),SEEK_SET);

	i=0;
	while(i<LOOP_NODEDAB
		&& lock(file,(long)((long)(user->number-1)*U_LEN),U_LEN)==-1) {
		if(i)
			mswait(100);
		i++;
	}

	if(i>=LOOP_NODEDAB) {
		close(file);
		return(-2);
	}

	if(write(file,userdat,U_LEN)!=U_LEN) {
		unlock(file,(long)((long)(user->number-1)*U_LEN),U_LEN);
		close(file);
		return(-3);
	}
	unlock(file,(long)((long)(user->number-1)*U_LEN),U_LEN);
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

	if(!VALID_CFG(cfg) || usernumber<1) {
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
	lseek(file,(long)((long)(usernumber-1)*(LEN_ALIAS+2)),SEEK_SET);
	read(file,name,LEN_ALIAS);
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
	long length;
	uint total_users;

	if(!VALID_CFG(cfg) || name==NULL || number<1)
		return(-1);

	SAFEPRINTF(str,"%suser/name.dat", cfg->data_dir);
	if((file=nopen(str,O_RDWR|O_CREAT))==-1)
		return(errno);
	length=(long)filelength(file);

	/* Truncate corrupted name.dat */
	total_users=lastuser(cfg);
	if((uint)(length/(LEN_ALIAS+2))>total_users)
		chsize(file,total_users*(LEN_ALIAS+2));

	if(length && length%(LEN_ALIAS+2)) {
		close(file);
		return(-3);
	}
	if(length<(((long)number-1)*(LEN_ALIAS+2))) {
		SAFEPRINTF2(str,"%*s\r\n",LEN_ALIAS,nulstr);
		memset(str,ETX,LEN_ALIAS);
		lseek(file,0L,SEEK_END);
		while(filelength(file)<((long)number*(LEN_ALIAS+2)))
			write(file,str,(LEN_ALIAS+2));
	}
	lseek(file,(long)(((long)number-1)*(LEN_ALIAS+2)),SEEK_SET);
	putrec(str,0,LEN_ALIAS,name);
	putrec(str,LEN_ALIAS,2,crlf);
	wr=write(file,str,LEN_ALIAS+2);
	close(file);

	if(wr!=LEN_ALIAS+2)
		return(errno);
	return(0);
}

#define DECVAL(ch, mul)	(DEC_CHAR_TO_INT(ch) * (mul))

int getbirthyear(const char* birth)
{
	if(IS_DIGIT(birth[2]))				// CCYYMMYY format
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
	if(IS_DIGIT(birth[5]))				// CCYYMMYY format
		return DECVAL(birth[4], 10)	+ DECVAL(birth[5], 1);
	if(cfg->sys_misc & SM_EURODATE) {	// DD/MM/YY format
		return DECVAL(birth[3], 10) + DECVAL(birth[4], 1);
	} else {							// MM/DD/YY format
		return DECVAL(birth[0], 10) + DECVAL(birth[1], 1);
	}
}

int getbirthday(scfg_t* cfg, const char* birth)
{
	if(IS_DIGIT(birth[5]))				// CCYYMMYY format
		return DECVAL(birth[6], 10)	+ DECVAL(birth[7], 1);
	if(cfg->sys_misc & SM_EURODATE) {	// DD/MM/YY format
		return DECVAL(birth[0], 10) + DECVAL(birth[1], 1);
	} else {							// MM/DD/YY format
		return DECVAL(birth[3], 10) + DECVAL(birth[4], 1);
	}
}

// Always returns string in MM/DD/YY format
char* getbirthmmddyy(scfg_t* cfg, const char* birth, char* buf, size_t max)
{
	safe_snprintf(buf, max, "%02u/%02u/%02u"
		, getbirthmonth(cfg, birth)
		, getbirthday(cfg, birth)
		, getbirthyear(birth) % 100);
	return buf;
}

// Always returns string in DD/MM/YY format
char* getbirthddmmyy(scfg_t* cfg, const char* birth, char* buf, size_t max)
{
	safe_snprintf(buf, max, "%02u/%02u/%02u"
		, getbirthday(cfg, birth)
		, getbirthmonth(cfg, birth)
		, getbirthyear(birth) % 100);
	return buf;
}

char* getbirthdstr(scfg_t* cfg, const char* birth, char* buf, size_t max)
{
	if(cfg->sys_misc & SM_EURODATE)
		getbirthddmmyy(cfg, birth, buf, max);
	else
		getbirthmmddyy(cfg, birth, buf, max);
	return buf;
}

/****************************************************************************/
/* Returns the age derived from the string 'birth' in the format CCYYMMDD	*/
/* or legacy: MM/DD/YY or DD/MM/YY											*/
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
	if(mon > tm.tm_mon || (mon == tm.tm_mon && getbirthday(cfg, birth) > tm.tm_mday))
		age--;
	return age;
}

/****************************************************************************/
/* Converts from either MM/DD/YYYYY or DD/MM/YYYY to YYYYMMDD				*/
/****************************************************************************/
char* parse_birthdate(scfg_t* cfg, const char* birthdate, char* out, size_t maxlen)
{
	if(cfg->sys_misc & SM_EURODATE)
		safe_snprintf(out, maxlen, "%.4s%.2s%.2s", birthdate + 6, birthdate + 3, birthdate);
	else
		safe_snprintf(out, maxlen, "%.4s%.2s%.2s", birthdate + 6, birthdate, birthdate + 3);
	return out;
}

/****************************************************************************/
/* Converts from user birth date to either MM/DD/YYYYY or DD/MM/YYYY		*/
/****************************************************************************/
char* format_birthdate(scfg_t* cfg, const char* birthdate, char* out, size_t maxlen)
{
	if(maxlen < 1)
		return NULL;
	*out = '\0';
	if(*birthdate) {
		if(cfg->sys_misc & SM_EURODATE)
			safe_snprintf(out, maxlen, "%02u/%02u/%04u"
				,getbirthday(cfg, birthdate), getbirthmonth(cfg, birthdate), getbirthyear(birthdate));
		else
			safe_snprintf(out, maxlen, "%02u/%02u/%04u"
				,getbirthmonth(cfg, birthdate), getbirthday(cfg, birthdate), getbirthyear(birthdate));
	}
	return out;
}

const char* birthdate_format(scfg_t* cfg)
{
	return cfg->sys_misc&SM_EURODATE ? "DD/MM/YYYY" : "MM/DD/YYYY";
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
/* Reads the data for node number 'number' into the structure 'node'        */
/* from node.dab															*/
/****************************************************************************/
int getnodedat(scfg_t* cfg, uint number, node_t *node, BOOL lockit, int* fdp)
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
			lseek(file,(long)number*sizeof(node_t),SEEK_SET);
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
/****************************************************************************/
int putnodedat(scfg_t* cfg, uint number, node_t* node, BOOL closeit, int file)
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
		lseek(file,(long)number*sizeof(node_t),SEEK_SET);
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
	lseek(f, (num-1) * 128, SEEK_SET);
	read(f, buf, 128);
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
                    if(!node->aux)
                        strcat(str,"at external program menu");
                    else if(node->aux<=cfg->total_xtrns)
                        sprintf(str+strlen(str),"running %s"
 	                       ,cfg->xtrn[node->aux-1]->name);
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
        if(node->misc&(NODE_MSGW|NODE_NMSG))
            strcat(str,"M");
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
uint userdatdupe(scfg_t* cfg, uint usernumber, uint offset, uint datlen
						 ,char *dat, BOOL del, BOOL next, void (*progress)(void*, int, int), void* cbdata)
{
    char	str[MAX_PATH+1];
    uint	i;
	int		file;
    long	l,length;

	if(!VALID_CFG(cfg) || dat==NULL)
		return(0);

	truncsp(dat);
	SAFEPRINTF(str,"%suser/user.dat", cfg->data_dir);
	if((file=nopen(str,O_RDONLY|O_DENYNONE))==-1)
		return(0);
	length=(long)filelength(file);
	if(usernumber && next)
		l=((long)usernumber) * U_LEN;
	else
		l=0;
	if(progress != NULL)
		progress(cbdata, l, length);
	for(;l<length;l+=U_LEN) {
		if(progress != NULL)
			progress(cbdata, l, length);
		if(usernumber && l/U_LEN==(long)usernumber-1)
			continue;
		lseek(file,l+offset,SEEK_SET);
		i=0;
		while(i<LOOP_NODEDAB && lock(file,l,U_LEN)==-1) {
			if(i)
				mswait(100);
			i++;
		}

		if(i>=LOOP_NODEDAB) {
			close(file);
			return(0);
		}

		read(file,str,datlen);
		for(i=0;i<datlen;i++)
			if(str[i]==ETX) break;
		str[i]=0;
		truncsp(str);
		if(!stricmp(str,dat)) {
			if(!del) {      /* Don't include deleted users in search */
				lseek(file,l+U_MISC,SEEK_SET);
				read(file,str,8);
				getrec(str,0,8,str);
				if(ahtoul(str)&(DELETED|INACTIVE)) {
					unlock(file,l,U_LEN);
					continue;
				}
			}
			unlock(file,l,U_LEN);
			close(file);
			return((l/U_LEN)+1);
		} else
			unlock(file,l,U_LEN);
	}
	close(file);
	if(progress != NULL)
		progress(cbdata, l, length);
	return(0);
}

/****************************************************************************/
/* Creates a short message for 'usernumber' that contains 'strin'           */
/****************************************************************************/
int putsmsg(scfg_t* cfg, int usernumber, char *strin)
{
    char str[256];
    int file,i;
    node_t node;

	if(!VALID_CFG(cfg) || usernumber<1 || strin==NULL)
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
		getnodedat(cfg,i,&node,/* lockit: */FALSE, &file);
		if(node.useron==usernumber
			&& (node.status==NODE_INUSE || node.status==NODE_QUIET)
			&& !(node.misc&NODE_MSGW)) {
			if(getnodedat(cfg,i,&node, /* lockit: */TRUE, &file)==0) {
				node.misc|=NODE_MSGW;
				putnodedat(cfg,i,&node, /* closeit: */FALSE, file);
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
	char	str[MAX_PATH+1], *buf;
	int		i;
    int		file = -1;
    long	length;
	node_t	node;

	if(!VALID_CFG(cfg) || usernumber<1)
		return(NULL);

	for(i=1;i<=cfg->sys_nodes;i++) {	/* clear msg waiting flag */
		getnodedat(cfg,i,&node, /* lockit: */FALSE, &file);
		if(node.useron==usernumber
			&& (node.status==NODE_INUSE || node.status==NODE_QUIET)
			&& node.misc&NODE_MSGW) {
			if(getnodedat(cfg,i,&node, /* lockit: */TRUE, &file) == 0) {
				node.misc&=~NODE_MSGW;
				putnodedat(cfg,i,&node, /* closeit: */FALSE, file);
			}
		}
	}
	CLOSE_OPEN_FILE(file);

	SAFEPRINTF2(str,"%smsgs/%4.4u.msg",cfg->data_dir,usernumber);
	if(flength(str)<1L)
		return(NULL);
	if((file=nopen(str,O_RDWR))==-1)
		return(NULL);
	length=(long)filelength(file);
	if((buf=(char *)malloc(length+1))==NULL) {
		close(file);
		return(NULL);
	}
	if(read(file,buf,length)!=length) {
		close(file);
		free(buf);
		return(NULL);
	}
	chsize(file,0L);
	close(file);
	buf[length]=0;
	strip_invalid_attr(buf);

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

	if(getnodedat(cfg,node_num,&node, /* lockit: */TRUE, &file) == 0) {
		node.misc&=~NODE_NMSG;          /* clear the NMSG flag */
		putnodedat(cfg,node_num,&node, /* closeit: */TRUE, file);
	}

	SAFEPRINTF2(str,"%smsgs/n%3.3u.msg",cfg->data_dir,node_num);
	if(flength(str)<1L)
		return(NULL);
	if((file=nopen(str,O_RDWR))==-1)
		return(NULL);
	length=(long)filelength(file);
	if(!length) {
		close(file);
		return(NULL);
	}
	if((buf=(char *)malloc(length+1))==NULL) {
		close(file);
		return(NULL);
	}
	if(read(file,buf,length)!=length) {
		close(file);
		free(buf);
		return(NULL);
	}
	chsize(file,0L);
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
	lseek(file,0L,SEEK_END);	/* Instead of opening with O_APPEND */
	i=strlen(strin);
	if(write(file,strin,i)!=i) {
		close(file);
		return(errno);
	}
	CLOSE_OPEN_FILE(file);
	getnodedat(cfg,num,&node, /* lockit: */FALSE, &file);
	if((node.status==NODE_INUSE || node.status==NODE_QUIET)
		&& !(node.misc&NODE_NMSG)) {
		if(getnodedat(cfg,num,&node, /* lockit: */TRUE, &file) == 0) {
			node.misc|=NODE_NMSG;
			putnodedat(cfg,num,&node, /* closeit: */FALSE, file);
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
		free((char*)client->user);
	}
	memset(client, 0, sizeof(*client));
	client->size = sizeof(client);
	SAFEPRINTF(path, "%sclient.ini", cfg->node_path[number - 1]);
	fp = iniOpenFile(path, /* create: */FALSE);
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
	if((p = iniReadString(fp, ROOT_SECTION, "name", NULL, value)) != NULL)
		client->user = strdup(p);
	*done = iniReadInteger(fp, ROOT_SECTION, "done", client->time);
	fclose(fp);
	return sock;
}

static BOOL ar_exp(scfg_t* cfg, uchar **ptrptr, user_t* user, client_t* client)
{
	BOOL	result,not,or,equal;
	uint	i,n,artype=AR_LEVEL,age;
	ulong	l;
	time_t	now;
	struct tm tm;
	const char*	p;

	result = TRUE;

	for(;(**ptrptr);(*ptrptr)++) {

		if((**ptrptr)==AR_ENDNEST)
			break;

		not=or=equal = FALSE;

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
				if(user==NULL || !(user->misc&WIP))
					result=not;
				else result=!not;
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
				if(user==NULL || user->level<SYSOP_LEVEL)
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
				l=(ulong)i*1024UL;
				if(user==NULL
					|| (equal && user->cdt+user->freecdt!=l)
					|| (!equal && user->cdt+user->freecdt<l))
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

BOOL chk_ar(scfg_t* cfg, uchar *ar, user_t* user, client_t* client)
{
	uchar *p;

	if(ar==NULL)
		return(TRUE);
	if(!VALID_CFG(cfg))
		return(FALSE);
	p=ar;
	return(ar_exp(cfg,&p,user,client));
}

/****************************************************************************/
/* Fills 'str' with record for usernumber starting at start for length bytes*/
/* Called from function ???													*/
/****************************************************************************/
int getuserrec(scfg_t* cfg, int usernumber,int start, int length, char *str)
{
	char	path[256];
	int		i,c,file;

	if(!VALID_CFG(cfg) || usernumber<1 || str==NULL)
		return(-1);
	SAFEPRINTF(path,"%suser/user.dat",cfg->data_dir);
	if((file=nopen(path,O_RDONLY|O_DENYNONE))==-1)
		return(errno);
	if(usernumber<1
		|| filelength(file)<(long)((long)(usernumber-1L)*U_LEN)+(long)start) {
		close(file);
		return(-2);
	}
	lseek(file,(long)((long)(usernumber-1)*U_LEN)+start,SEEK_SET);

	if(length < 1) { /* auto-length */
		length=user_rec_len(start);
		if(length < 1) {
			close(file);
			return -5;
		}
	}

	i=0;
	while(i<LOOP_NODEDAB
		&& lock(file,(long)((long)(usernumber-1)*U_LEN)+start,length)==-1) {
		if(i)
			mswait(100);
		i++;
	}

	if(i>=LOOP_NODEDAB) {
		close(file);
		return(-3);
	}

	if(read(file,str,length)!=length) {
		unlock(file,(long)((long)(usernumber-1)*U_LEN)+start,length);
		close(file);
		return(-4);
	}

	unlock(file,(long)((long)(usernumber-1)*U_LEN)+start,length);
	close(file);
	for(c=0;c<length;c++)
		if(str[c]==ETX || str[c]==CR) break;
	str[c]=0;

	if(c == 0 && start == LEN_PASS) // Backwards compatibility hack
		return getuserrec(cfg, usernumber, U_OLDPASS, LEN_OLDPASS, str);

	return(0);
}

/****************************************************************************/
/* Write a string (str) into user.dat at the offset for usernumber + start	*/
/* 'length' may be auto-determined (from 'start') by passing 0 for length	*/
/* If 'str' is longer than 'length', only 'length' characters are written	*/
/****************************************************************************/
int putuserrec(scfg_t* cfg, int usernumber, int start, int length, const char *str)
{
	const char*	p;
	char	buf[256];
	char	path[MAX_PATH + 1];
	int		file;
	uint	i;

	if(!VALID_CFG(cfg) || usernumber < 1 || str == NULL)
		return -1;

	if(length == 0) {	/* auto-length */
		length = user_rec_len(start);
		if(length < 1)
			return -2;
	}
	size_t slen = strlen(str);
	if(slen >= (size_t)length)
		p = str;
	else {
		if(length > sizeof(buf))
			return -10;
		memset(buf, ETX, length);
		memcpy(buf, str, slen);
		p = buf;
	}

	SAFEPRINTF(path, "%suser/user.dat", cfg->data_dir);
	if((file = nopen(path, O_RDWR|O_DENYNONE))==-1)
		return errno;

	off_t offset = (usernumber - 1) * U_LEN;
	if(filelength(file) < offset) {
		close(file);
		return -4;
	}

	offset += start;
	if(lseek(file, offset, SEEK_SET) != offset) {
		close(file);
		return -5;
	}

	i=0;
	while(i < LOOP_NODEDAB
		&& lock(file, offset, length) == -1) {
		if(i)
			mswait(100);
		i++;
	}

	if(i >= LOOP_NODEDAB) {
		close(file);
		return -3;
	}

	int wr = write(file, p, length);
	unlock(file, offset, length);
	close(file);
	if(wr != length)
		return -6;
	dirtyuserdat(cfg, usernumber);
	return 0;
}

/****************************************************************************/
/* Updates user 'usernumber's record (numeric string) by adding 'adj' to it */
/* returns the new value.													*/
/****************************************************************************/
ulong adjustuserrec(scfg_t* cfg, int usernumber, int start, int length, long adj)
{
	char str[256],path[256];
	char tmp[32];
	int i,c,file;
	ulong val;

	if(!VALID_CFG(cfg) || usernumber<1)
		return(0);

	SAFEPRINTF(path,"%suser/user.dat",cfg->data_dir);
	if((file=nopen(path,O_RDWR|O_DENYNONE))==-1)
		return(0);

	if(filelength(file)<((long)usernumber-1)*U_LEN) {
		close(file);
		return(0);
	}

	lseek(file,(long)((long)(usernumber-1)*U_LEN)+start,SEEK_SET);

	if(length < 1) { /* auto-length */
		length=user_rec_len(start);
		if(length < 1) {
			close(file);
			return 0;
		}
	}

	i=0;
	while(i<LOOP_NODEDAB
		&& lock(file,(long)((long)(usernumber-1)*U_LEN)+start,length)==-1) {
		if(i)
			mswait(100);
		i++;
	}

	if(i>=LOOP_NODEDAB) {
		close(file);
		return(0);
	}

	if(read(file,str,length)!=length) {
		unlock(file,(long)((long)(usernumber-1)*U_LEN)+start,length);
		close(file);
		return(0);
	}
	for(c=0;c<length;c++)
		if(str[c]==ETX || str[c]==CR) break;
	str[c]=0;
	if(length > 5) {
		val = strtoul(str, NULL, 10);
		if(adj<0L && val<(ulong)-adj)		/* don't go negative */
			val=0;
		else if(adj > 0 && val + adj < val)
			val = ULONG_MAX;
		else
			val += (ulong)adj;
	} else {
		ushort sval = (ushort)strtoul(str, NULL, 10);
		if(adj < 0L && sval < (ushort)-adj)		/* don't go negative */
			sval = 0;
		else if(adj > 0 && sval + adj < sval)
			sval = USHRT_MAX;
		else
			sval += (ushort)adj;
		val = sval;
	}
	lseek(file,(long)((long)(usernumber-1)*U_LEN)+start,SEEK_SET);
	putrec(str,0,length,ultoa(val,tmp,10));
	if(write(file,str,length)!=length) {
		unlock(file,(long)((long)(usernumber-1)*U_LEN)+start,length);
		close(file);
		return(val);
	}
	unlock(file,(long)((long)(usernumber-1)*U_LEN)+start,length);
	close(file);
	dirtyuserdat(cfg,usernumber);
	return(val);
}

/****************************************************************************/
/* Subtract credits from the current user online, accounting for the new    */
/* "free credits" field.                                                    */
/****************************************************************************/
void subtract_cdt(scfg_t* cfg, user_t* user, long amt)
{
	char tmp[64];
    long mod;

	if(!amt || user==NULL)
		return;
	if(user->freecdt) {
		if((ulong)amt>user->freecdt) {      /* subtract both credits and */
			mod=amt-user->freecdt;   /* free credits */
			putuserrec(cfg, user->number,U_FREECDT,10,"0");
			user->freecdt=0;
			user->cdt=adjustuserrec(cfg, user->number,U_CDT,10,-mod);
		} else {                          /* subtract just free credits */
			user->freecdt-=amt;
			putuserrec(cfg, user->number,U_FREECDT,10
				,ultoa(user->freecdt,tmp,10));
		}
	}
	else    /* no free credits */
		user->cdt=adjustuserrec(cfg, user->number,U_CDT,10,-amt);
}

BOOL user_posted_msg(scfg_t* cfg, user_t* user, int count)
{
	if(user==NULL)
		return(FALSE);

	user->posts	=(ushort)adjustuserrec(cfg, user->number, U_POSTS, 5, count);
	user->ptoday=(ushort)adjustuserrec(cfg, user->number, U_PTODAY, 5, count);

	return(TRUE);
}

BOOL user_sent_email(scfg_t* cfg, user_t* user, int count, BOOL feedback)
{
	if(user==NULL)
		return(FALSE);

	if(feedback)
		user->fbacks=(ushort)adjustuserrec(cfg, user->number, U_FBACKS, 5, count);
	else
		user->emails=(ushort)adjustuserrec(cfg, user->number, U_EMAILS, 5, count);
	user->etoday=(ushort)adjustuserrec(cfg, user->number, U_ETODAY, 5, count);

	return(TRUE);
}

BOOL user_downloaded(scfg_t* cfg, user_t* user, int files, off_t bytes)
{
	if(user==NULL)
		return(FALSE);

	user->dls=(ushort)adjustuserrec(cfg, user->number, U_DLS, 5, files);
	user->dlb=adjustuserrec(cfg, user->number, U_DLB, 10, (long)bytes);

	return(TRUE);
}

#ifdef SBBS
BOOL user_downloaded_file(scfg_t* cfg, user_t* user, client_t* client,
	uint dirnum, const char* filename, off_t bytes)
{
	file_t f;
	bool removed = false;

	if(!loadfile(cfg, dirnum, filename, &f, file_detail_normal))
		return FALSE;

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
		return FALSE;
	}

	/**************************/
	/* Update Uploader's Info */
	/**************************/
	user_t uploader = {0};
	uploader.number=matchuser(cfg, f.from, TRUE /*sysop_alias*/);
	if(uploader.number
		&& uploader.number != user->number 
		&& getuserdat(cfg, &uploader) == 0
		&& (uint32_t)uploader.firston < f.hdr.when_imported.time) {
		ulong l = (ulong)f.cost;
		if(!(cfg->dir[dirnum]->misc&DIR_CDTDL))	/* Don't give credits on d/l */
			l=0;
		ulong mod=(ulong)(l*(cfg->dir[dirnum]->dn_pct/100.0));
		adjustuserrec(cfg, uploader.number, U_CDT, 10, mod);
		if(cfg->text != NULL && !(cfg->dir[dirnum]->misc&DIR_QUIET)) {
			char str[256];
			char tmp[128];
			char prefix[128]="";
			ultoac(mod,tmp);
			const char* alias = user->alias[0] ? user->alias : cfg->text[UNKNOWN_USER];
			char username[64];
			if(client != NULL && uploader.level >= SYSOP_LEVEL) {
				if(client->host[0] != '\0' && strcmp(client->host, STR_NO_HOSTNAME) != 0)
					SAFEPRINTF2(username,"%s [%s]", alias, client->host);
				else
					SAFEPRINTF2(username,"%s [%s]", alias, client->addr);
			} else
				SAFECOPY(username, alias);
			if(strcmp(cfg->dir[dirnum]->code, "TEMP") == 0 || bytes < (ulong)f.size)
				SAFECOPY(prefix, cfg->text[Partially]);
			if(client != NULL) {
				SAFECAT(prefix, client->protocol);
				SAFECAT(prefix, "-");
			}
			/* Inform uploader of downloaded file */
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
		subtract_cdt(cfg, user, (long)f.cost);

	if(!(cfg->dir[dirnum]->misc&DIR_NOSTAT))
		inc_sys_download_stats(cfg, /* files: */1, (ulong)bytes);

	smb_freefilemem(&f);
	return TRUE;
}
#endif

BOOL user_uploaded(scfg_t* cfg, user_t* user, int files, off_t bytes)
{
	if(user==NULL)
		return(FALSE);

	user->uls=(ushort)adjustuserrec(cfg, user->number, U_ULS, 5, files);
	user->ulb=adjustuserrec(cfg, user->number, U_ULB, 10, (long)bytes);

	return(TRUE);
}

BOOL user_adjust_credits(scfg_t* cfg, user_t* user, long amount)
{
	if(user==NULL)
		return(FALSE);

	if(amount<0)	/* subtract */
		subtract_cdt(cfg, user, -amount);
	else			/* add */
		user->cdt=adjustuserrec(cfg, user->number, U_CDT, 10, amount);

	return(TRUE);
}

BOOL user_adjust_minutes(scfg_t* cfg, user_t* user, long amount)
{
	if(user==NULL)
		return(FALSE);

	user->min=adjustuserrec(cfg, user->number, U_MIN, 10, amount);

	return(TRUE);
}

/****************************************************************************/
/****************************************************************************/
BOOL logoutuserdat(scfg_t* cfg, user_t* user, time_t now, time_t logontime)
{
	char str[128];
	time_t tused;
	struct tm tm, tm_now;

	if(user==NULL)
		return(FALSE);

	if(now==0)
		now=time(NULL);

	tused=(now-logontime)/60;
	user->tlast=(ushort)(tused > USHRT_MAX ? USHRT_MAX : tused);

	putuserrec(cfg,user->number,U_LASTON,8,ultoa((ulong)now,str,16));
	putuserrec(cfg,user->number,U_TLAST,5,ultoa(user->tlast,str,10));
	adjustuserrec(cfg,user->number,U_TIMEON,5,user->tlast);
	adjustuserrec(cfg,user->number,U_TTODAY,5,user->tlast);

	/* Convert time_t to struct tm */
	if(localtime_r(&now,&tm_now)==NULL)
		return(FALSE);

	if(localtime_r(&logontime,&tm)==NULL)
		return(FALSE);

	/* Reset daily stats if new day */
	if(tm.tm_mday!=tm_now.tm_mday)
		resetdailyuserdat(cfg, user, /* write: */TRUE);

	return(TRUE);
}

/****************************************************************************/
/****************************************************************************/
void resetdailyuserdat(scfg_t* cfg, user_t* user, BOOL write)
{
	char str[128];

	if(!VALID_CFG(cfg) || user==NULL)
		return;

	/* logons today */
	user->ltoday=0;
	if(write) putuserrec(cfg,user->number,U_LTODAY,5,"0");
	/* e-mails today */
	user->etoday=0;
	if(write) putuserrec(cfg,user->number,U_ETODAY,5,"0");
	/* posts today */
	user->ptoday=0;
	if(write) putuserrec(cfg,user->number,U_PTODAY,5,"0");
	/* free credits per day */
	user->freecdt=cfg->level_freecdtperday[user->level];
	if(write) putuserrec(cfg,user->number,U_FREECDT,10
		,ultoa(user->freecdt,str,10));
	/* time used today */
	user->ttoday=0;
	if(write) putuserrec(cfg,user->number,U_TTODAY,5,"0");
	/* extra time today */
	user->textra=0;
	if(write) putuserrec(cfg,user->number,U_TEXTRA,5,"0");
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
	if((fp=fopen(fname,"r"))==NULL)
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
	long	misc;
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
			fread(str,LEN_ALIAS+2,1,stream);
			for(c=0;c<LEN_ALIAS;c++)
				if(str[c]==ETX) break;
			str[c]=0;
			if(!c) {	 /* should be a deleted user */
				getuserrec(cfg,unum,U_MISC,8,str);
				misc=ahtoul(str);
				if(misc&DELETED) {	 /* deleted bit set too */
					getuserrec(cfg,unum,U_LASTON,8,str);
					if((time(NULL)-ahtoul(str))/86400>=cfg->sys_deldays)
						break; /* deleted long enough ? */
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
		getuserrec(cfg,unum,U_MISC,8,str);
		misc=ahtoul(str);
		if(!(misc&DELETED)) /* Not deleted? Set usernumber to end+1 */
			unum=last+1;
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
	SAFEPRINTF2(str,"%suser/%04u",cfg->data_dir,user->number);
	delfiles(str,ALLFILES, /* keep: */0);
	rmdir(str);

	SAFEPRINTF2(str,"%suser/ptrs/%04u.ixb",cfg->data_dir,user->number); /* msg ptrs */
	remove(str);
	SAFEPRINTF2(str,"%smsgs/%04u.msg",cfg->data_dir,user->number); /* delete short msg */
	remove(str);

	/* Update daily statistics database (for system and node) */

	for(i=0;i<2;i++) {
		SAFEPRINTF(str,"%sdsts.dab",i ? cfg->ctrl_dir : cfg->node_dir);
		if((file=nopen(str,O_RDWR))==-1)
			continue;
		memset(&stats,0,sizeof(stats));
		lseek(file,4L,SEEK_SET);   /* Skip timestamp */
		read(file,&stats,sizeof(stats));
		stats.nusers++;
		lseek(file,4L,SEEK_SET);
		write(file,&stats,sizeof(stats));
		close(file);
	}

	return(0);
}

/* Returns length of specified user record 'field', or -1 if invalid */
int user_rec_len(int offset)
{
	switch(offset) {

		/* Strings (of different lengths) */
		case U_ALIAS:		return(LEN_ALIAS);
		case U_NAME:		return(LEN_NAME);
		case U_HANDLE:		return(LEN_HANDLE);
		case U_NOTE:		return(LEN_NOTE);
		case U_COMP:		return(LEN_COMP);
		case U_COMMENT:		return(LEN_COMMENT);
		case U_NETMAIL:		return(LEN_NETMAIL);
		case U_ADDRESS:		return(LEN_ADDRESS);
		case U_LOCATION:	return(LEN_LOCATION);
		case U_ZIPCODE:		return(LEN_ZIPCODE);
		case U_PASS:		return(LEN_PASS);
		case U_PHONE:		return(LEN_PHONE);
		case U_BIRTH:		return(LEN_BIRTH);
		case U_MODEM:		return(LEN_MODEM);
		case U_IPADDR:		return(LEN_IPADDR);

		/* Internal codes (16 chars) */
		case U_CURSUB:
		case U_CURDIR:
			return (16);

		/* Dates in time_t format (8 hex digits) */
		case U_LASTON:
		case U_FIRSTON:
		case U_EXPIRE:
		case U_PWMOD:
		case U_NS_TIME:
		case U_LOGONTIME:

		/* 32-bit integers (8 hex digits) */
		case U_FLAGS1:
		case U_FLAGS2:
		case U_FLAGS3:
		case U_FLAGS4:
		case U_EXEMPT:
		case U_REST:
		case U_MISC:
		case U_QWK:
		case U_CHAT:

		/* Internal codes (8 chars) */
		case U_CURXTRN:
		case U_XEDIT:
		case U_SHELL:
			return(8);

		/* 16-bit integers (5 decimal digits) */
		case U_LOGONS:
		case U_LTODAY:
		case U_TIMEON:
		case U_TEXTRA:
		case U_TTODAY:
		case U_TLAST:
		case U_POSTS:
		case U_EMAILS:
		case U_FBACKS:
		case U_ETODAY:
		case U_PTODAY:
		case U_ULS:
		case U_DLS:
			return(5);

		/* 32-bit integers (10 decimal digits) */
		case U_ULB:
		case U_DLB:
		case U_CDT:
		case U_MIN:
		case U_FREECDT:
			return(10);

		/* 3 char strings */
		case U_TMPEXT:
			return(3);
		case U_ROWS:
			return LEN_ROWS;
		case U_COLS:
			return LEN_COLS;

		/* 2 digit integers (0-99 or 00-FF) */
		case U_LEVEL:
		case U_TL:
		case U_LEECH:
			return(2);

		/* Single digits chars */
		case U_SEX:
		case U_PROT:
			return(1);
	}

	return(-1);
}

/****************************************************************************/
/* Determine if the specified user can or cannot access the specified sub	*/
/****************************************************************************/
BOOL can_user_access_sub(scfg_t* cfg, uint subnum, user_t* user, client_t* client)
{
	if(!VALID_CFG(cfg))
		return FALSE;
	if(subnum>=cfg->total_subs)
		return FALSE;
	if(!chk_ar(cfg,cfg->grp[cfg->sub[subnum]->grp]->ar,user,client))
		return FALSE;
	if(!chk_ar(cfg,cfg->sub[subnum]->ar,user,client))
		return FALSE;

	return TRUE;
}

/****************************************************************************/
/* Determine if the specified user can or cannot read the specified sub		*/
/****************************************************************************/
BOOL can_user_read_sub(scfg_t* cfg, uint subnum, user_t* user, client_t* client)
{
	if(!can_user_access_sub(cfg, subnum, user, client))
		return FALSE;
	return chk_ar(cfg,cfg->sub[subnum]->read_ar,user,client);
}

/****************************************************************************/
/* Determine if the specified user can or cannot post on the specified sub	*/
/* 'reason' is an (optional) pointer to a text.dat item number, indicating	*/
/* the reason the user cannot post, when returning FALSE.					*/
/****************************************************************************/
BOOL can_user_post(scfg_t* cfg, uint subnum, user_t* user, client_t* client, uint* reason)
{
	if(reason!=NULL)
		*reason=NoAccessSub;
	if(!can_user_access_sub(cfg, subnum, user, client))
		return FALSE;
	if(reason!=NULL)
		*reason=CantPostOnSub;
	if(!chk_ar(cfg,cfg->sub[subnum]->post_ar,user,client))
		return FALSE;
	if(cfg->sub[subnum]->misc&(SUB_QNET|SUB_FIDO|SUB_PNET|SUB_INET)
		&& user->rest&FLAG('N'))		/* network restriction? */
		return FALSE;
	if((cfg->sub[subnum]->misc & SUB_NAME)
		&& (user->rest & (FLAG('Q') | FLAG('O'))) == FLAG('O'))
		return FALSE;
	if(reason!=NULL)
		*reason=R_Post;
	if(user->rest&FLAG('P'))			/* post restriction? */
		return FALSE;
	if(reason!=NULL)
		*reason=TooManyPostsToday;
	if(user->ptoday>=cfg->level_postsperday[user->level])
		return FALSE;

	return TRUE;
}

/****************************************************************************/
/* Determine if the specified user can or cannot access the specified dir	*/
/****************************************************************************/
BOOL can_user_access_dir(scfg_t* cfg, uint dirnum, user_t* user, client_t* client)
{
	if(!VALID_CFG(cfg))
		return FALSE;
	if(dirnum>=cfg->total_dirs)
		return FALSE;
	if(!chk_ar(cfg,cfg->lib[cfg->dir[dirnum]->lib]->ar,user,client))
		return FALSE;
	if(!chk_ar(cfg,cfg->dir[dirnum]->ar,user,client))
		return FALSE;

	return TRUE;
}

/****************************************************************************/
/* Determine if the specified user can or cannot upload files to the dirnum	*/
/* 'reason' is an (optional) pointer to a text.dat item number, indicating	*/
/* the reason the user cannot post, when returning FALSE.					*/
/****************************************************************************/
BOOL can_user_upload(scfg_t* cfg, uint dirnum, user_t* user, client_t* client, uint* reason)
{
	if(reason!=NULL)
		*reason=NoAccessDir;
	if(!can_user_access_dir(cfg, dirnum, user, client))
		return FALSE;
	if(reason!=NULL)
		*reason=R_Upload;
	if(user->rest&FLAG('U'))			/* upload restriction? */
		return FALSE;
	if(user->rest&FLAG('T'))			/* transfer restriction? */
		return FALSE;
	if(!(user->exempt&FLAG('U'))		/* upload exemption */
		&& !is_user_dirop(cfg, dirnum, user, client)) {
		if(reason!=NULL)
			*reason=CantUploadHere;
		if(!chk_ar(cfg, cfg->dir[dirnum]->ul_ar, user, client))
			return FALSE;
	}
	return TRUE;
}

/****************************************************************************/
/* Determine if the specified user can or cannot download files from dirnum	*/
/* 'reason' is an (optional) pointer to a text.dat item number, indicating	*/
/* the reason the user cannot post, when returning FALSE.					*/
/****************************************************************************/
BOOL can_user_download(scfg_t* cfg, uint dirnum, user_t* user, client_t* client, uint* reason)
{
	if(reason!=NULL)
		*reason=NoAccessDir;
	if(!can_user_access_dir(cfg, dirnum, user, client))
		return FALSE;
	if(reason!=NULL)
		*reason=CantDownloadFromDir;
	if(!chk_ar(cfg,cfg->dir[dirnum]->dl_ar,user,client))
		return FALSE;
	if(reason!=NULL)
		*reason=R_Download;
	if(user->rest&FLAG('D'))			/* download restriction? */
		return FALSE;
	if(user->rest&FLAG('T'))			/* transfer restriction? */
		return FALSE;

	return TRUE;
}

/****************************************************************************/
/* Determine if the specified user can or cannot send email					*/
/* 'reason' is an (optional) pointer to a text.dat item number				*/
/* usernumber==0 for netmail												*/
/****************************************************************************/
BOOL can_user_send_mail(scfg_t* cfg, enum smb_net_type net_type, uint usernumber, user_t* user, uint* reason)
{
	if(reason!=NULL)
		*reason=R_Email;
	if(user==NULL || user->number==0)
		return FALSE;
	if(net_type==NET_NONE && usernumber>1 && user->rest&FLAG('E'))			/* local mail restriction? */
		return FALSE;
	if(reason!=NULL)
		*reason=NoNetMailAllowed;
	if(net_type!=NET_NONE && user->rest&FLAG('M'))							/* netmail restriction */
		return FALSE;
	if(net_type==NET_FIDO && !(cfg->netmail_misc&NMAIL_ALLOW))				/* Fido netmail globally disallowed */
		return FALSE;
	if(net_type==NET_INTERNET && !(cfg->inetmail_misc&NMAIL_ALLOW))			/* Internet mail globally disallowed */
		return FALSE;
	if(reason!=NULL)
		*reason=R_Feedback;
	if(net_type==NET_NONE && usernumber==1 && user->rest&FLAG('S'))			/* feedback restriction? */
		return FALSE;
	if(reason!=NULL)
		*reason=TooManyEmailsToday;
	if(user->etoday>=cfg->level_emailperday[user->level] && !(user->exempt&FLAG('M')))
		return FALSE;

	return TRUE;
}

/****************************************************************************/
/* Determine if the specified user is a sub-board operator					*/
/****************************************************************************/
BOOL is_user_subop(scfg_t* cfg, uint subnum, user_t* user, client_t* client)
{
	if(user==NULL)
		return FALSE;
	if(!can_user_access_sub(cfg, subnum, user, client))
		return FALSE;
	if(user->level>=SYSOP_LEVEL)
		return TRUE;

	return cfg->sub[subnum]->op_ar[0]!=0 && chk_ar(cfg,cfg->sub[subnum]->op_ar,user,client);
}

/****************************************************************************/
/* Determine if the specified user is a directory operator					*/
/****************************************************************************/
BOOL is_user_dirop(scfg_t* cfg, uint dirnum, user_t* user, client_t* client)
{
	if(user==NULL)
		return FALSE;
	if(!can_user_access_dir(cfg, dirnum, user, client))
		return FALSE;
	if(user->level >= SYSOP_LEVEL)
		return TRUE;

	return cfg->dir[dirnum]->op_ar[0]!=0 && chk_ar(cfg,cfg->dir[dirnum]->op_ar,user,client);
}

/****************************************************************************/
/* Determine if downloads from the specified directory are free for the		*/
/* specified user															*/
/****************************************************************************/
BOOL is_download_free(scfg_t* cfg, uint dirnum, user_t* user, client_t* client)
{
	if(!VALID_CFG(cfg))
		return(FALSE);

	if(dirnum>=cfg->total_dirs)
		return(FALSE);

	if(cfg->dir[dirnum]->misc&DIR_FREE)
		return(TRUE);

	if(user==NULL)
		return(FALSE);

	if(user->exempt&FLAG('D'))
		return(TRUE);

	if(cfg->dir[dirnum]->ex_ar[0]==0)
		return(FALSE);

	return(chk_ar(cfg,cfg->dir[dirnum]->ex_ar,user,client));
}

BOOL is_host_exempt(scfg_t* cfg, const char* ip_addr, const char* host_name)
{
	char	exempt[MAX_PATH+1];

	SAFEPRINTF2(exempt, "%s%s", cfg->ctrl_dir, strIpFilterExemptConfigFile);
	return findstr(ip_addr, exempt) || findstr(host_name, exempt);
}

/****************************************************************************/
/* Add an IP address (with comment) to the IP filter/trashcan file			*/
/* ToDo: Move somewhere more appropriate (filter.c?)						*/
/****************************************************************************/
BOOL filter_ip(scfg_t* cfg, const char* prot, const char* reason, const char* host
					   ,const char* ip_addr, const char* username, const char* fname)
{
	char	ip_can[MAX_PATH+1];
	char	exempt[MAX_PATH+1];
	char	tstr[64];
    FILE*	fp;
    time_t	now = time(NULL);

	if(ip_addr==NULL)
		return(FALSE);

	SAFEPRINTF2(exempt, "%s%s", cfg->ctrl_dir, strIpFilterExemptConfigFile);
	if(findstr(ip_addr, exempt))
		return(FALSE);
	if(findstr(host, exempt))
		return(FALSE);

	SAFEPRINTF(ip_can,"%sip.can",cfg->text_dir);
	if(fname==NULL)
		fname=ip_can;

	if(findstr(ip_addr, fname))	/* Already filtered? */
		return(TRUE);

    if((fp=fopen(fname,"a"))==NULL)
    	return(FALSE);

    fprintf(fp, "\n; %s %s ", prot, reason);
	if(username != NULL)
		fprintf(fp, "by %s ", username);
    fprintf(fp,"on %.24s\n", ctime_r(&now, tstr));

	if(host!=NULL)
		fprintf(fp,"; Hostname: %s\n",host);

	fprintf(fp,"%s\n",ip_addr);

    fclose(fp);
	return(TRUE);
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
		tleft-=(long)(now-starttime);
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
BOOL check_name(scfg_t* cfg, const char* name)
{
	char	tmp[512];
	size_t	len;

	len=strlen(name);
	if(len<1)
		return FALSE;
	if (   name[0] <= ' '			/* begins with white-space? */
		|| name[len-1] <= ' '		/* ends with white-space */
		|| !IS_ALPHA(name[0])
		|| !stricmp(name,cfg->sys_id)
		|| strchr(name,0xff)
		|| matchuser(cfg,name,TRUE /* sysop_alias */)
		|| trashcan(cfg,name,"name")
		|| alias(cfg,name,tmp)!=name
 	   )
 		return FALSE;
 	return TRUE;
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
BOOL loginAttemptListFree(link_list_t* list)
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
		listRemoveNode(list, node, /* freeData: */TRUE);
	listUnlock(list);
}

/****************************************************************************/
/* Returns number of *unique* login attempts (excludes consecutive dupes)	*/
/****************************************************************************/
ulong loginFailure(link_list_t* list, const union xp_sockaddr* addr, const char* prot, const char* user, const char* pass)
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
	if(node==NULL)
		listPushNodeData(list, attempt, sizeof(login_attempt_t));
	listUnlock(list);

	return count;
}

#if !defined(NO_SOCKET_SUPPORT)
ulong loginBanned(scfg_t* cfg, link_list_t* list, SOCKET sock, const char* host_name
	,struct login_attempt_settings settings, login_attempt_t* details)
{
	char				ip_addr[128];
	list_node_t*		node;
	login_attempt_t*	attempt;
	BOOL				result = FALSE;
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
		&& findstr(ip_addr, exempt))
		return 0;
	if(host_name != NULL
		&& findstr(host_name, exempt))
		return 0;

	if(!listLock(list))
		return 0;
	node = login_attempted(list, &client_addr);
	listUnlock(list);
	if(node == NULL)
		return 0;
	attempt = node->data;
	if(((settings.tempban_threshold && (attempt->count - attempt->dupes) >= settings.tempban_threshold)
		|| trashcan(cfg, attempt->user, "name")) && now < (time32_t)(attempt->time + settings.tempban_duration)) {
		if(details != NULL)
			*details = *attempt;
		return settings.tempban_duration - (now - attempt->time);
	}
	return 0;
}

/****************************************************************************/
/* Message-new-scan pointer/configuration functions							*/
/****************************************************************************/
BOOL getmsgptrs(scfg_t* cfg, user_t* user, subscan_t* subscan, void (*progress)(void*, int, int), void* cbdata)
{
	char		path[MAX_PATH+1];
	uint		i;
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
			subscan[i].ptr	= iniGetLongInt(keys, NULL, "ptr"	, subscan[i].ptr);
			subscan[i].last	= iniGetLongInt(keys, NULL, "last"	, subscan[i].last);
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
		return TRUE;
	}

	/* Old way: */
	SAFEPRINTF2(path,"%suser/ptrs/%4.4u.ixb", cfg->data_dir, user->number);
	if((stream=fnopen(&file,path,O_RDONLY))==NULL) {
		if(fexist(path))
			return(FALSE);	/* file exists, but couldn't be opened? */
		return initmsgptrs(cfg, subscan, cfg->new_msgscan_init, progress, cbdata);
	}

	length=(long)filelength(file);
	for(i=0;i<cfg->total_subs;i++) {
		if(progress != NULL)
			progress(cbdata, i, cfg->total_subs);
		if(length>=(cfg->sub[i]->ptridx+1)*10L) {
			fseek(stream,(long)cfg->sub[i]->ptridx*10L,SEEK_SET);
			fread(&subscan[i].ptr,sizeof(subscan[i].ptr),1,stream);
			fread(&subscan[i].last,sizeof(subscan[i].last),1,stream);
			fread(&subscan[i].cfg,sizeof(subscan[i].cfg),1,stream);
		}
		subscan[i].sav_ptr=subscan[i].ptr;
		subscan[i].sav_last=subscan[i].last;
		subscan[i].sav_cfg=subscan[i].cfg;
	}
	if(progress != NULL)
		progress(cbdata, i, cfg->total_subs);
	fclose(stream);
	return(TRUE);
}

/****************************************************************************/
/* Writes to data/user/####.subs the msgptr array for the current user		*/
/* Pass usernumber value of 0 to indicate "Guest" login						*/
/****************************************************************************/
BOOL putmsgptrs(scfg_t* cfg, user_t* user, subscan_t* subscan)
{
	char		path[MAX_PATH+1];
	uint		i;
	time_t		now = time(NULL);
	BOOL		result = TRUE;

	if(user->number==0 || (user->rest&FLAG('G')))	/* Guest */
		return(TRUE);

	fixmsgptrs(cfg, subscan);
	SAFEPRINTF2(path,"%suser/%4.4u.subs", cfg->data_dir, user->number);
	FILE* fp = fnopen(NULL, path, O_RDWR|O_CREAT|O_TEXT);
	if (fp == NULL)
		return FALSE;
	str_list_t new = strListInit();
	str_list_t ini = iniReadFile(fp);
	ini_style_t ini_style = { .key_prefix = "\t", .section_separator = "" };
	BOOL modified = FALSE;
	for(i=0; i < cfg->total_subs; i++) {
		str_list_t keys = iniGetSection(ini, cfg->sub[i]->code);
		if(subscan[i].sav_ptr==subscan[i].ptr
			&& subscan[i].sav_last==subscan[i].last
			&& subscan[i].sav_cfg==subscan[i].cfg
			&& keys != NULL && *keys != NULL)
			iniAppendSectionWithKeys(&new, cfg->sub[i]->code, keys, &ini_style);
		else {
			iniSetLongInt(&new, cfg->sub[i]->code, "ptr", subscan[i].ptr, &ini_style);
			iniSetLongInt(&new, cfg->sub[i]->code, "last", subscan[i].last, &ini_style);
			iniSetHexInt(&new, cfg->sub[i]->code, "cfg", subscan[i].cfg, &ini_style);
			iniSetDateTime(&new, cfg->sub[i]->code, "updated", /* include_time: */TRUE, now, &ini_style);
			modified = TRUE;
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

BOOL newmsgs(smb_t* smb, time_t t)
{
	char index_fname[MAX_PATH + 1];

	SAFEPRINTF(index_fname, "%s.sid", smb->file);
	return fdate(index_fname) >= t;
}

/****************************************************************************/
/* Initialize new-msg-scan pointers (e.g. for new users)					*/
/* If 'days' is specified as 0, just set pointer to last message (faster)	*/
/****************************************************************************/
BOOL initmsgptrs(scfg_t* cfg, subscan_t* subscan, unsigned days, void (*progress)(void*, int, int), void* cbdata)
{
	uint		i;
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
	return TRUE;
}

/****************************************************************************/
/* Insure message new-scan pointers are within the range of the msgs in		*/
/* the sub-board.															*/
/****************************************************************************/
BOOL fixmsgptrs(scfg_t* cfg, subscan_t* subscan)
{
	uint		i;
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
	return TRUE;
}

static char* sysop_available_semfile(scfg_t* scfg)
{
	static char semfile[MAX_PATH+1];
	SAFEPRINTF(semfile, "%ssysavail.chat", scfg->ctrl_dir);
	return semfile;
}

BOOL sysop_available(scfg_t* scfg)
{
	return fexist(sysop_available_semfile(scfg));
}

BOOL set_sysop_availability(scfg_t* scfg, BOOL available)
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

BOOL sound_muted(scfg_t* scfg)
{
	return fexist(sound_muted_semfile(scfg));
}

BOOL set_sound_muted(scfg_t* scfg, BOOL muted)
{
	if(muted)
		return ftouch(sound_muted_semfile(scfg));
	return remove(sound_muted_semfile(scfg)) == 0;
}

/************************************/
/* user .ini file get/set functions */
/************************************/

static FILE* user_ini_open(scfg_t* scfg, unsigned user_number, BOOL create)
{
	char path[MAX_PATH+1];

	SAFEPRINTF2(path, "%suser/%04u.ini", scfg->data_dir, user_number);
	return iniOpenFile(path, create);
}

BOOL user_get_property(scfg_t* scfg, unsigned user_number, const char* section, const char* key, char* value, size_t maxlen)
{
	FILE* fp;
	char buf[INI_MAX_VALUE_LEN];

	fp = user_ini_open(scfg, user_number, /* create: */FALSE);
	if(fp == NULL)
		return FALSE;
	char* result = iniReadValue(fp, section, key, NULL, buf);
	if(result != NULL)
		safe_snprintf(value, maxlen, "%s", result);
	iniCloseFile(fp);
	return result != NULL;
}

BOOL user_set_property(scfg_t* scfg, unsigned user_number, const char* section, const char* key, const char* value)
{
	FILE* fp;
	str_list_t ini;

	fp = user_ini_open(scfg, user_number, /* create: */TRUE);
	if(fp == NULL)
		return FALSE;
	ini = iniReadFile(fp);
	ini_style_t ini_style = { .key_prefix = "\t", .section_separator = "", .value_separator = " = " };
	char* result = iniSetValue(&ini, section, key, value, &ini_style);
	iniWriteFile(fp, ini);
	iniFreeStringList(ini);
	iniCloseFile(fp);
	return result != NULL;
}

BOOL user_set_time_property(scfg_t* scfg, unsigned user_number, const char* section, const char* key, time_t value)
{
	FILE* fp;
	str_list_t ini;

	fp = user_ini_open(scfg, user_number, /* create: */TRUE);
	if(fp == NULL)
		return FALSE;
	ini = iniReadFile(fp);
	ini_style_t ini_style = { .key_prefix = "\t", .section_separator = "", .value_separator = " = " };
	char* result = iniSetDateTime(&ini, section, key, /* include_time */TRUE, value, &ini_style);
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
		int userdat = openuserdat(cfg, /* modify */FALSE);
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

/* Returns the directory index of a virtual lib/dir path (e.g. main/games/filename) */
int getdir_from_vpath(scfg_t* cfg, const char* p, user_t* user, client_t* client, BOOL include_upload_only)
{
	char*	tp;
	char	path[MAX_PATH+1];
	uint	dir;
	uint	lib;

	SAFECOPY(path,p);
	p=path;

	if(*p=='/') 
		p++;
	if(!strncmp(p,"./",2))
		p+=2;

	tp=strchr(p,'/');
	if(tp) *tp=0;
	for(lib=0;lib<cfg->total_libs;lib++) {
		if(!chk_ar(cfg,cfg->lib[lib]->ar,user,client))
			continue;
		if(!stricmp(cfg->lib[lib]->vdir,p))
			break;
	}
	if(lib>=cfg->total_libs) 
		return(-1);

	if(tp!=NULL)
		p=tp+1;

	tp=strchr(p,'/');
	if(tp) *tp=0;
	for(dir=0;dir<cfg->total_dirs;dir++) {
		if(cfg->dir[dir]->lib!=lib)
			continue;
		if((!include_upload_only || (dir!=cfg->sysop_dir && dir!=cfg->upload_dir))
			&& !chk_ar(cfg,cfg->dir[dir]->ar,user,client))
			continue;
		if(!stricmp(cfg->dir[dir]->vdir,p))
			break;
	}
	if(dir>=cfg->total_dirs) 
		return(-1);

	return(dir);
}
