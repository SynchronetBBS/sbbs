/* Upgrade Synchronet v3.1x user.dat to Synchronet v3.20 user.tab */

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

#include "sbbs.h"
#include "str_util.h"
#include "findstr.h"
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

scfg_t scfg;

#define VALID_CFG(cfg)	(cfg!=NULL && cfg->size==sizeof(scfg_t))

int lprintf(int level, const char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

    va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
    return(puts(sbuf));
}

/****************************************************************************/
/* Returns the number of the last user in user.dat (deleted ones too)		*/
/****************************************************************************/
static uint v31x_lastuser(scfg_t* cfg)
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
/* Opens the user database returning the file descriptor or -1 on error		*/
/****************************************************************************/
static int v31x_openuserdat(scfg_t* cfg, BOOL for_modify)
{
	char path[MAX_PATH+1];

	if(!VALID_CFG(cfg))
		return(-1);

	SAFEPRINTF(path,"%suser/user.dat",cfg->data_dir);
	return nopen(path, for_modify ? (O_RDWR|O_CREAT|O_DENYNONE) : (O_RDONLY|O_DENYNONE));
}

static int v31x_closeuserdat(int file)
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
static int v31x_readuserdat(scfg_t* cfg, unsigned user_number, char* userdat, int infile)
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

	(void)lseek(file,(long)((long)(user_number-1)*U_LEN),SEEK_SET);
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
static int v31x_parseuserdat(scfg_t* cfg, char *userdat, user_t *user)
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
	getrec(userdat,U_ULB,10,str); user->ulb=parse_byte_count(str, 1);
	getrec(userdat,U_ULS,5,str); user->uls=atoi(str);
	getrec(userdat,U_DLB,10,str); user->dlb=parse_byte_count(str, 1);
	getrec(userdat,U_DLS,5,str); user->dls=atoi(str);
	getrec(userdat,U_CDT,LEN_CDT,str);
	if(str[0] < '0' || str[0] > '9')
		getrec(userdat,U_OLDCDT,10,str);
	user->cdt=strtoull(str, NULL, 10);
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

	getrec(userdat,U_FREECDT,LEN_CDT,str);
	if(str[0] < '0' || str[0] > '9')
		getrec(userdat,U_OLDFREECDT,10,str);
	user->freecdt=strtoull(str, NULL, 10);

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

/* Fast getuserdat() (leaves user.dat file open) */
static int v31x_fgetuserdat(scfg_t* cfg, user_t *user, int file)
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
/* Fills 'str' with record for usernumber starting at start for length bytes*/
/* Called from function ???													*/
/****************************************************************************/
static int v31x_getuserrec(scfg_t* cfg, int usernumber,int start, int length, char *str)
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
	(void)lseek(file,(long)((long)(usernumber-1)*U_LEN)+start,SEEK_SET);

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

/* Returns length of specified user record 'field', or -1 if invalid */
static int v31x_user_rec_len(int offset)
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
		case U_MIN:
			return(10);

		/* 64-bit integers (20 decimal digits) */
		case U_CDT:
		case U_FREECDT:
			return 20;

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

bool upgrade_users()
{
	int result = true;
	time_t start = time(NULL);
	uint total_users = 0;
	uint last = v31x_lastuser(&scfg);
	uint largest = 0;
	size_t maxlen = 0;
	char userdat[USER_REC_LINE_LEN];
	char path[MAX_PATH + 1];
	SAFEPRINTF(path, "%suser/user.tab", scfg.data_dir);

	printf("Converting from user.dat to %s\n", path);
	FILE* out = fopen(path, "wb");
	if(out == NULL) {
		perror(path);
		return false;
	}

	int file = v31x_openuserdat(&scfg, /* for_modify */FALSE);
	if(file == -1) {
		perror("user.dat");
		return false;
	}
	for(uint i = 1; i <= last; i++) {
		user_t user;
		ZERO_VAR(user);
		user.number = i;
		if(v31x_fgetuserdat(&scfg, &user, file) != 0) {
			printf("Error reading user %d\n", user.number);
			result = false;
			break;
		}
		format_userdat(&scfg, &user, userdat);
		fwrite(userdat, sizeof(char), sizeof(userdat), out);
		userdat[USER_REC_LEN] = '\0';
		truncsp(userdat);
		size_t len = strlen(userdat);
		if(len > maxlen) {
			maxlen = len;
			largest = user.number;
		}
		total_users++;
	}
	v31x_closeuserdat(file);
	fclose(out);

	time_t diff = time(NULL) - start;
	printf("%u users converted (%lu users/second), largest (#%u) = %u bytes\n"
		,total_users, (ulong)(diff ? total_users / diff : total_users), largest, (unsigned)maxlen);

	return result;
}

bool verify_users()
{
	int result = true;
	time_t start = time(NULL);
	uint total_users = 0;
	uint last = v31x_lastuser(&scfg);
	uint largest = 0;
	size_t maxlen = 0;
	char userdat[USER_REC_LINE_LEN];
	char path[MAX_PATH + 1];

	printf("Comparing user.dat to user.tab\n");
	int tab = openuserdat(&scfg, /* for_modify */FALSE);
	if(tab == -1) {
		perror("user.tab");
		return false;
	}

	int file = v31x_openuserdat(&scfg, /* for_modify */FALSE);
	if(file == -1) {
		perror("user.dat");
		return false;
	}
	for(uint i = 1; i <= last; i++) {
		user_t user;
		ZERO_VAR(user);
		user.number = i;
		if(v31x_fgetuserdat(&scfg, &user, file) != 0) {
			printf("Error reading user %d\n", user.number);
			result = false;
			break;
		}
		user_t new;
		new.number = i;
		if(fgetuserdat(&scfg, &new, tab) != 0) {
			printf("Error reading user %d from user.tab\n", i);
			result = false;
			break;
		}
		if(memcmp(&user, &new, sizeof(user)) != 0) {
			printf("Error comparing user #%u afer upgrade\n", i);
			result = false;
			break;
		}
		total_users++;
	}
	v31x_closeuserdat(file);
	closeuserdat(tab);

	time_t diff = time(NULL) - start;
	printf("%u users verified (%lu users/second)\n"
		,total_users, (ulong)(diff ? total_users / diff : total_users));

	return result;
}

char *usage="\nusage: upgrade [ctrl_dir]\n";

int main(int argc, char** argv)
{
	char	error[512];

	fprintf(stderr,"\nupgrade - Upgrade Synchronet BBS to %s\n"
		,VERSION
		);

	memset(&scfg, 0, sizeof(scfg));
	scfg.size = sizeof(scfg);
	SAFECOPY(scfg.ctrl_dir, get_ctrl_dir(/* warn: */true));

	if(chdir(scfg.ctrl_dir) != 0)
		fprintf(stderr,"!ERROR changing directory to: %s", scfg.ctrl_dir);

	printf("\nLoading configuration files from %s\n",scfg.ctrl_dir);
	if(!load_cfg(&scfg, NULL, TRUE, /* node: **/FALSE, error, sizeof(error))) {
		fprintf(stderr,"!ERROR loading configuration files: %s\n",error);
		return EXIT_FAILURE + __COUNTER__;
	}

	if(!upgrade_users())
		return EXIT_FAILURE + __COUNTER__;

	if(!verify_users())
		return EXIT_FAILURE + __COUNTER__;

	printf("Upgrade successful.\n");
    return EXIT_SUCCESS;
}
