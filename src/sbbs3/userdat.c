/* userdat.c */

/* Synchronet user data-related routines (exported) */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#include "cmdshell.h"

/* convenient space-saving global variables */
char* crlf="\r\n";
char* nulstr="";

#define REPLACE_CHARS(str,ch1,ch2)	for(c=0;str[c];c++)	if(str[c]==ch1) str[c]=ch2;

#define VALID_CFG(cfg)	(cfg!=NULL && cfg->size==sizeof(scfg_t))

/****************************************************************************/
/* Looks for a perfect match amoung all usernames (not deleted users)		*/
/* Makes dots and underscores synomynous with spaces for comparisions		*/
/* Returns the number of the perfect matched username or 0 if no match		*/
/****************************************************************************/
uint DLLCALL matchuser(scfg_t* cfg, char *name, BOOL sysop_alias)
{
	int file,c;
	char dat[LEN_ALIAS+2];
	char str[256];
	ulong l,length;
	FILE *stream;

	if(!VALID_CFG(cfg) || name==NULL)
		return(0);

	if(sysop_alias &&
		(!stricmp(name,"SYSOP") || !stricmp(name,"POSTMASTER") || !stricmp(name,cfg->sys_id)))
		return(1);

	sprintf(str,"%suser/name.dat",cfg->data_dir);
	if((file=nopen(str,O_RDONLY))==-1)
		return(0);
	length=filelength(file);
	if((stream=fdopen(file,"rb"))==NULL)
		return(0);
	for(l=0;l<length;l+=LEN_ALIAS+2) {
		fread(dat,sizeof(dat),1,stream);
		for(c=0;c<LEN_ALIAS;c++)
			if(dat[c]==ETX) break;
		dat[c]=0;
		if(!stricmp(dat,name)) 
			break;
		/* convert dots to spaces */
		strcpy(str,dat);
		REPLACE_CHARS(str,'.',' ');
		if(!stricmp(str,name)) 
			break;
		/* convert spaces to dots */
		strcpy(str,dat);
		REPLACE_CHARS(str,' ','.');
		if(!stricmp(str,name)) 
			break;
		/* convert dots to underscores */
		strcpy(str,dat);
		REPLACE_CHARS(str,'.','_');
		if(!stricmp(str,name)) 
			break;
		/* convert underscores to dots */
		strcpy(str,dat);
		REPLACE_CHARS(str,'_','.');
		if(!stricmp(str,name)) 
			break;
		/* convert spaces to underscores */
		strcpy(str,dat);
		REPLACE_CHARS(str,' ','_');
		if(!stricmp(str,name)) 
			break;
		/* convert underscores to spaces */
		strcpy(str,dat);
		REPLACE_CHARS(str,'_',' ');
		if(!stricmp(str,name)) 
			break;
	}
	fclose(stream);
	if(l<length)
		return((l/(LEN_ALIAS+2))+1); 
	return(0);
}

/****************************************************************************/
uint DLLCALL total_users(scfg_t* cfg)
{
    char	str[MAX_PATH+1];
    uint	total_users=0;
	int		file;
    long	l,length;

	if(!VALID_CFG(cfg))
		return(0);

	sprintf(str,"%suser/user.dat", cfg->data_dir);
	if((file=nopen(str,O_RDONLY|O_DENYNONE))==-1)
		return(0);
	length=filelength(file);
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
uint DLLCALL lastuser(scfg_t* cfg)
{
	char str[256];
	long length;

	if(!VALID_CFG(cfg))
		return(0);

	sprintf(str,"%suser/user.dat", cfg->data_dir);
	if((length=flength(str))>0)
		return((uint)(length/U_LEN));
	return(0);
}

/****************************************************************************/
/* Deletes last user record in user.dat										*/
/****************************************************************************/
BOOL DLLCALL del_lastuser(scfg_t* cfg)
{
	char	str[256];
	int		file;
	long	length;

	if(!VALID_CFG(cfg))
		return(FALSE);

	sprintf(str,"%suser/user.dat", cfg->data_dir);
	if((file=nopen(str,O_RDWR|O_DENYNONE))==-1)
		return(FALSE);
	length=filelength(file);
	if(length<U_LEN) {
		close(file);
		return(FALSE);
	}
	chsize(file,length-U_LEN);
	close(file);
	return(TRUE);
}


/****************************************************************************/
/* Fills the structure 'user' with info for user.number	from user.dat		*/
/* Called from functions useredit, waitforcall and main_sec					*/
/****************************************************************************/
int DLLCALL getuserdat(scfg_t* cfg, user_t *user)
{
	char userdat[U_LEN+1],str[U_LEN+1],tmp[64];
	int i,file;

	if(user==NULL)
		return(-1);

	if(!VALID_CFG(cfg) || user->number<1) {
		memset(user,0,sizeof(user_t));
		return(-1); 
	}
	sprintf(userdat,"%suser/user.dat",cfg->data_dir);
	if((file=nopen(userdat,O_RDONLY|O_DENYNONE))==-1) {
		memset(user,0,sizeof(user_t));
		return(errno); 
	}
	if(user->number > (filelength(file)/U_LEN)) {
		close(file);
		return(-1);	/* no such user record */
	}
	lseek(file,(long)((long)(user->number-1)*U_LEN),SEEK_SET);
	i=0;
	while(i<LOOP_NODEDAB
		&& lock(file,(long)((long)(user->number-1)*U_LEN),U_LEN)==-1) {
		if(i)
			mswait(100);
		i++; 
	}

	if(i>=LOOP_NODEDAB) {
		close(file);
		memset(user,0L,sizeof(user_t));
		return(-2); 
	}

	if(read(file,userdat,U_LEN)!=U_LEN) {
		unlock(file,(long)((long)(user->number-1)*U_LEN),U_LEN);
		close(file);
		memset(user,0L,sizeof(user_t));
		return(-3); 
	}

	unlock(file,(long)((long)(user->number-1)*U_LEN),U_LEN);
	close(file);
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
	getrec(userdat,U_PHONE,LEN_PHONE,user->phone);
	getrec(userdat,U_BIRTH,LEN_BIRTH,user->birth);
	getrec(userdat,U_MODEM,LEN_MODEM,user->modem);
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
	getrec(userdat,U_ULB,10,str); user->ulb=atol(str);
	getrec(userdat,U_ULS,5,str); user->uls=atoi(str);
	getrec(userdat,U_DLB,10,str); user->dlb=atol(str);
	getrec(userdat,U_DLS,5,str); user->dls=atoi(str);
	getrec(userdat,U_CDT,10,str); user->cdt=atol(str);
	getrec(userdat,U_MIN,10,str); user->min=atol(str);
	getrec(userdat,U_LEVEL,2,str); user->level=atoi(str);
	getrec(userdat,U_FLAGS1,8,str); user->flags1=ahtoul(str);
	getrec(userdat,U_FLAGS2,8,str); user->flags2=ahtoul(str);
	getrec(userdat,U_FLAGS3,8,str); user->flags3=ahtoul(str);
	getrec(userdat,U_FLAGS4,8,str); user->flags4=ahtoul(str);
	getrec(userdat,U_EXEMPT,8,str); user->exempt=ahtoul(str);
	getrec(userdat,U_REST,8,str); user->rest=ahtoul(str);
	getrec(userdat,U_ROWS,2,str); user->rows=atoi(str);
	if(user->rows && user->rows<10)
		user->rows=10;
	user->sex=userdat[U_SEX];
	if(!user->sex)
		user->sex=SP;	 /* fix for v1b04 that could save as 0 */
	user->prot=userdat[U_PROT];
	if(user->prot<SP)
		user->prot=SP;
	getrec(userdat,U_MISC,8,str); user->misc=ahtoul(str);
	if(user->rest&FLAG('Q'))
		user->misc&=~SPIN;

	getrec(userdat,U_LEECH,2,str);
	user->leech=(uchar)ahtoul(str);
	getrec(userdat,U_CURSUB,sizeof(user->cursub)-1,user->cursub);
	getrec(userdat,U_CURDIR,sizeof(user->curdir)-1,user->curdir);
	getrec(userdat,U_CURXTRN,8,user->curxtrn);

	getrec(userdat,U_FREECDT,10,str);
	user->freecdt=atol(str);

	getrec(userdat,U_XEDIT,8,str);
	for(i=0;i<cfg->total_xedits;i++)
		if(!stricmp(str,cfg->xedit[i]->code) && chk_ar(cfg,cfg->xedit[i]->ar,user))
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
	if(str[0]<SP) { 			   /* v1c, so set defaults */
		if(user->rest&FLAG('Q'))
			user->qwk=(QWK_RETCTLA);
		else
			user->qwk=(QWK_FILES|QWK_ATTACH|QWK_EMAIL|QWK_DELMAIL); 
	}
	else
		user->qwk=ahtoul(str);

	getrec(userdat,U_TMPEXT,3,user->tmpext);
	if((!user->tmpext[0] || !strcmp(user->tmpext,"0")) && cfg->total_fcomps)
		strcpy(user->tmpext,cfg->fcomp[0]->ext);  /* For v1x to v2x conversion */

	getrec(userdat,U_CHAT,8,str);
	user->chat=ahtoul(str);

	/* Reset daily stats if not logged on today */
	unixtodstr(cfg, time(NULL),str);
	unixtodstr(cfg, user->laston,tmp);
	if(strcmp(str,tmp) && user->ltoday) 
		resetdailyuserdat(cfg,user);

#if 0 // removed 01/19/00
	if(useron.number==user->number) {
		if(user!=&useron)
			useron=*user;

		if(online) {

	#if 0	/* legacy? */
			getusrdirs();
			getusrsubs();
	#endif
			if(user->misc&AUTOTERM) {			// was useron.misc (01/19/00)
				user->misc&=~(ANSI|RIP|WIP|HTML);
				user->misc|=autoterm; 
			}
		} 
	}
#endif
	return(0);
}

/****************************************************************************/
/* Writes into user.number's slot in user.dat data in structure 'user'      */
/* Called from functions newuser, useredit and main                         */
/****************************************************************************/
int DLLCALL putuserdat(scfg_t* cfg, user_t* user)
{
    int		i,file;
    char	userdat[U_LEN],str[MAX_PATH+1];
    node_t	node;

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
	putrec(userdat,U_PHONE,LEN_PHONE,user->phone);
	putrec(userdat,U_BIRTH,LEN_BIRTH,user->birth);
	putrec(userdat,U_MODEM,LEN_MODEM,user->modem);
	putrec(userdat,U_LASTON,8,ultoa(user->laston,str,16));
	putrec(userdat,U_FIRSTON,8,ultoa(user->firston,str,16));
	putrec(userdat,U_EXPIRE,8,ultoa(user->expire,str,16));
	putrec(userdat,U_PWMOD,8,ultoa(user->pwmod,str,16));
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

	putrec(userdat,U_ROWS,2,ultoa(user->rows,str,10));
	userdat[U_SEX]=user->sex;
	userdat[U_PROT]=user->prot;
	putrec(userdat,U_MISC,8,ultoa(user->misc,str,16));
	putrec(userdat,U_LEECH,2,ultoa(user->leech,str,16));

	putrec(userdat,U_CURSUB,sizeof(user->cursub)-1,user->cursub);
	putrec(userdat,U_CURDIR,sizeof(user->curdir)-1,user->curdir);
	putrec(userdat,U_CURXTRN,8,user->curxtrn);
	putrec(userdat,U_CURXTRN+8,2,crlf);

	putrec(userdat,U_XFER_CMD+LEN_XFER_CMD,2,crlf);

	putrec(userdat,U_MAIL_CMD+LEN_MAIL_CMD,2,crlf);

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
	putrec(userdat,U_NS_TIME,8,ultoa(user->ns_time,str,16));
	putrec(userdat,U_LOGONTIME,8,ultoa(user->logontime,str,16));

	putrec(userdat,U_UNUSED,U_LEN-(U_UNUSED)-2,crlf);
	putrec(userdat,U_UNUSED+(U_LEN-(U_UNUSED)-2),2,crlf);

	sprintf(str,"%suser/user.dat", cfg->data_dir);
	if((file=nopen(str,O_RDWR|O_CREAT|O_DENYNONE))==-1) {
		return(errno);
	}

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
	for(i=1;i<=cfg->sys_nodes;i++) { /* instant user data update */
		if(i==cfg->node_num)
			continue;
		getnodedat(cfg, i,&node,NULL);
		if(node.useron==user->number && (node.status==NODE_INUSE
			|| node.status==NODE_QUIET)) {
			getnodedat(cfg, i,&node,&file);
			node.misc|=NODE_UDAT;
			putnodedat(cfg, i,&node,file);
			break; 
		} 
	}
	return(0);
}


/****************************************************************************/
/* Returns the username in 'str' that corresponds to the 'usernumber'       */
/* Called from functions everywhere                                         */
/****************************************************************************/
char* DLLCALL username(scfg_t* cfg, int usernumber, char *name)
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
	sprintf(str,"%suser/name.dat",cfg->data_dir);
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
int DLLCALL putusername(scfg_t* cfg, int number, char *name)
{
	char str[256];
	int file;
	int wr;
	long length;
	uint total_users;

	if(!VALID_CFG(cfg) || name==NULL || number<1) 
		return(-1);

	sprintf(str,"%suser/name.dat", cfg->data_dir);
	if((file=nopen(str,O_RDWR|O_CREAT))==-1) 
		return(errno); 
	length=filelength(file);

	/* Truncate corrupted name.dat */
	total_users=lastuser(cfg);
	if((uint)(length/(LEN_ALIAS+2))>total_users)
		chsize(file,total_users*(LEN_ALIAS+2));

	if(length && length%(LEN_ALIAS+2)) {
		close(file);
		return(-3); 
	}
	if(length<(((long)number-1)*(LEN_ALIAS+2))) {
		sprintf(str,"%*s",LEN_ALIAS,nulstr);
		memset(str,ETX,LEN_ALIAS);
		strcat(str,crlf);
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

/****************************************************************************/
/* Returns the age derived from the string 'birth' in the format MM/DD/YY	*/
/****************************************************************************/
char DLLCALL getage(scfg_t* cfg, char *birth)
{
	char	age;
	struct	tm tm;
	time_t	now;

	if(!VALID_CFG(cfg) || birth==NULL)
		return(0);

	if(!atoi(birth) || !atoi(birth+3))	/* Invalid */
		return(0);

	now=time(NULL);
	if(localtime_r(&now,&tm)==NULL)
		return(0);
	age=(tm.tm_year)-(((birth[6]&0xf)*10)+(birth[7]&0xf));
	if(age>105)
		age-=105;
	tm.tm_mon++;	/* convert to 1 based */
	if(cfg->sys_misc&SM_EURODATE) {		/* DD/MM/YY format */
		if(atoi(birth)>31 || atoi(birth+3)>12)
			return(0);
		if(((birth[3]&0xf)*10)+(birth[4]&0xf)>tm.tm_mon ||
			(((birth[3]&0xf)*10)+(birth[4]&0xf)==tm.tm_mon &&
			((birth[0]&0xf)*10)+(birth[1]&0xf)>tm.tm_mday))
			age--; 
	} else {							/* MM/DD/YY format */
		if(atoi(birth)>12 || atoi(birth+3)>31)
			return(0);
		if(((birth[0]&0xf)*10)+(birth[1]&0xf)>tm.tm_mon ||
			(((birth[0]&0xf)*10)+(birth[1]&0xf)==tm.tm_mon &&
			((birth[3]&0xf)*10)+(birth[4]&0xf)>tm.tm_mday))
			age--; 
	}
	if(age<0)
		return(0);
	return(age);
}


/****************************************************************************/
/* Reads the data for node number 'number' into the structure 'node'        */
/* from node.dab															*/
/****************************************************************************/
int DLLCALL getnodedat(scfg_t* cfg, uint number, node_t *node, int* fdp)
{
	char	str[MAX_PATH+1];
	int		rd;
	int		count=0;
	int		file;

	if(fdp!=NULL)
		*fdp=-1;

	if(!VALID_CFG(cfg) 
		|| node==NULL || number<1 || number>cfg->sys_nodes)
		return(-1);

	memset(node,0,sizeof(node_t));
	sprintf(str,"%snode.dab",cfg->ctrl_dir);
	if((file=nopen(str,O_RDWR|O_DENYNONE))==-1)
		return(errno); 

	if(filelength(file)>=(long)(number*sizeof(node_t))) {
		number--;	/* make zero based */
		for(count=0;count<LOOP_NODEDAB;count++) {
			if(count)
				mswait(100);
			lseek(file,(long)number*sizeof(node_t),SEEK_SET);
			if(fdp!=NULL 
				&& lock(file,(long)number*sizeof(node_t),sizeof(node_t))!=0) 
				continue; 
			rd=read(file,node,sizeof(node_t));
			if(fdp==NULL || rd!=sizeof(node_t))
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
int DLLCALL putnodedat(scfg_t* cfg, uint number, node_t* node, int file)
{
	size_t	wr=0;
	int		wrerr=0;
	int		attempts;

	if(!VALID_CFG(cfg) 
		|| node==NULL || number<1 || number>cfg->sys_nodes || file<0) {
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
	close(file);

	if(wr!=sizeof(node_t))
		return(wrerr);
	return(0);
}

/****************************************************************************/
/* Packs the password 'pass' into 5bit ASCII inside node_t. 32bits in 		*/
/* node.extaux, and the other 8bits in the upper byte of node.aux			*/
/****************************************************************************/
void DLLCALL packchatpass(char *pass, node_t *node)
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
char* DLLCALL unpackchatpass(char *pass, node_t* node)
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

char* DLLCALL nodestatus(scfg_t* cfg, node_t* node, char* buf, size_t buflen)
{
	char	str[256];
	char*	mer;
	int		hour;

	if(node==NULL) {
		strncpy(buf,"(null)",buflen);
		return(buf);
	}

    switch(node->status) {
        case NODE_WFC:
            strcpy(str,"Waiting for call");
            break;
        case NODE_OFFLINE:
            strcpy(str,"Offline");
            break;
        case NODE_NETTING:
            strcpy(str,"Networking");
            break;
        case NODE_LOGON:
            strcpy(str,"At logon prompt");
            break;
        case NODE_EVENT_WAITING:
            strcpy(str,"Waiting for all nodes to become inactive");
            break;
        case NODE_EVENT_LIMBO:
            sprintf(str,"Waiting for node %d to finish external event"
                ,node->aux);
            break;
        case NODE_EVENT_RUNNING:
            strcpy(str,"Running external event");
            break;
        case NODE_NEWUSER:
            strcpy(str,"New user applying for access ");
            if(!node->connection)
                strcat(str,"Locally");
            else if(node->connection==0xffff) {
                strcat(str,"via telnet");
            } else
                sprintf(str+strlen(str),"at %ubps",node->connection);
            break;
        case NODE_QUIET:
        case NODE_INUSE:
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
                    if(!node->aux)
                        strcat(str,"in local chat with sysop");
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
            if(!node->connection)
                strcat(str," locally");
            if(node->connection==0xffff) {
                strcat(str," via telnet");
            } else
                sprintf(str+strlen(str)," at %ubps",node->connection);
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
void DLLCALL printnodedat(scfg_t* cfg, uint number, node_t* node)
{
	char	status[128];

	printf("Node %2d: %s\n",number,nodestatus(cfg,node,status,sizeof(status)));
}

/****************************************************************************/
uint DLLCALL userdatdupe(scfg_t* cfg, uint usernumber, uint offset, uint datlen
						 ,char *dat, BOOL del)
{
    char	str[MAX_PATH+1];
    uint	i;
	int		file;
    long	l,length;

	if(!VALID_CFG(cfg) || dat==NULL)
		return(0);

	truncsp(dat);
	sprintf(str,"%suser/user.dat", cfg->data_dir);
	if((file=nopen(str,O_RDONLY|O_DENYNONE))==-1)
		return(0);
	length=filelength(file);
	for(l=0;l<length;l+=U_LEN) {
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
	return(0);
}

/****************************************************************************/
/* Creates a short message for 'usernumber' that contains 'strin'           */
/****************************************************************************/
int DLLCALL putsmsg(scfg_t* cfg, int usernumber, char *strin)
{
    char str[256];
    int file,i;
    node_t node;

	if(!VALID_CFG(cfg) || usernumber<1 || strin==NULL)
		return(-1);

	if(*strin==0)
		return(0);

	sprintf(str,"%smsgs/%4.4u.msg",cfg->data_dir,usernumber);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
		return(errno); 
	}
	i=strlen(strin);
	if(write(file,strin,i)!=i) {
		close(file);
		return(errno); 
	}
	close(file);
	for(i=1;i<=cfg->sys_nodes;i++) {     /* flag node if user on that msg waiting */
		getnodedat(cfg,i,&node,NULL);
		if(node.useron==usernumber
			&& (node.status==NODE_INUSE || node.status==NODE_QUIET)
			&& !(node.misc&NODE_MSGW)) {
			getnodedat(cfg,i,&node,&file);
			node.misc|=NODE_MSGW;
			putnodedat(cfg,i,&node,file); 
		} 
	}
	return(0);
}

/****************************************************************************/
/* Returns any short messages waiting for user number, buffer must be freed */
/****************************************************************************/
char* DLLCALL getsmsg(scfg_t* cfg, int usernumber)
{
	char	str[MAX_PATH+1], HUGE16 *buf;
	int		i;
    int		file;
    long	length;
	node_t	node;

	if(!VALID_CFG(cfg) || usernumber<1)
		return(NULL);

	sprintf(str,"%smsgs/%4.4u.msg",cfg->data_dir,usernumber);
	if(flength(str)<1L)
		return(NULL);
	if((file=nopen(str,O_RDWR))==-1)
		return(NULL);
	length=filelength(file);
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

	for(i=1;i<=cfg->sys_nodes;i++) {	/* clear msg waiting flag */
		getnodedat(cfg,i,&node,NULL);
		if(node.useron==usernumber
			&& (node.status==NODE_INUSE || node.status==NODE_QUIET)
			&& node.misc&NODE_MSGW) {
			getnodedat(cfg,i,&node,&file);
			node.misc&=~NODE_MSGW;
			putnodedat(cfg,i,&node,file); 
		} 
	}

	return(buf);	/* caller must free */
}

char* DLLCALL getnmsg(scfg_t* cfg, int node_num)
{
	char	str[MAX_PATH+1];
	char*	buf;
	int		file;
	long	length;
	node_t	node;

	if(!VALID_CFG(cfg) || node_num<1)
		return(NULL);

	getnodedat(cfg,node_num,&node,&file);
	node.misc&=~NODE_NMSG;          /* clear the NMSG flag */
	putnodedat(cfg,node_num,&node,file);

	sprintf(str,"%smsgs/n%3.3u.msg",cfg->data_dir,node_num);
	if(flength(str)<1L)
		return(NULL);
	if((file=nopen(str,O_RDWR))==-1)
		return(NULL); 
	length=filelength(file);
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
int DLLCALL putnmsg(scfg_t* cfg, int num, char *strin)
{
    char str[256];
    int file,i;
    node_t node;

	if(!VALID_CFG(cfg) || num<1 || strin==NULL)
		return(-1);

	if(*strin==0)
		return(0);

	sprintf(str,"%smsgs/n%3.3u.msg",cfg->data_dir,num);
	if((file=nopen(str,O_WRONLY|O_CREAT))==-1)
		return(errno); 
	lseek(file,0L,SEEK_END);	// Instead of opening with O_APPEND
	i=strlen(strin);
	if(write(file,strin,i)!=i) {
		close(file);
		return(errno); 
	}
	close(file);
	getnodedat(cfg,num,&node,NULL);
	if((node.status==NODE_INUSE || node.status==NODE_QUIET)
		&& !(node.misc&NODE_NMSG)) {
		getnodedat(cfg,num,&node,&file);
		node.misc|=NODE_NMSG;
		putnodedat(cfg,num,&node,file); 
	}

	return(0);
}

static BOOL ar_exp(scfg_t* cfg, uchar **ptrptr, user_t* user)
{
	BOOL	result,not,or,equal;
	uint	i,n,artype=AR_LEVEL,age;
	ulong	l;
	time_t	now;
	struct tm tm;

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
			if(ar_exp(cfg,ptrptr,user))
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
			case AR_RIP:
			case AR_WIP:
			case AR_LOCAL:
			case AR_EXPERT:
			case AR_SYSOP:
			case AR_QUIET:
			case AR_OS2:
			case AR_DOS:
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
				#ifdef __FLAT__
					result=not;
				#else
					result=!not;
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
				result=not;
				(*ptrptr)++;
				break;
			case AR_SUB:
				result=not;
				(*ptrptr)++;
				break;
			case AR_SUBCODE:
				result=not;
				while(*(*ptrptr))
					(*ptrptr)++;
				break;
			case AR_LIB:
				result=not;
				(*ptrptr)++;
				break;
			case AR_DIR:
				result=not;
				(*ptrptr)++;
				break;
			case AR_DIRCODE:
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
				n=sbbs_random(i+1);
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
					&& (!user->posts || 100/(user->logons/user->posts)<(long)n))
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
						&& (!user->ulb || 100/(l/user->ulb)<n))
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
						&& (!user->uls || 100/(i/user->uls)<n))
						result=not;
					else
						result=!not;
				}
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
					|| stricmp(cfg->shell[user->shell]->code,(char*)*ptrptr))
					result=not;
				else
					result=!not;
				while(*(*ptrptr))
					(*ptrptr)++;
				break;
		} 
	}
	return(result);
}

BOOL DLLCALL chk_ar(scfg_t* cfg, uchar *ar, user_t* user)
{
	uchar *p;

	if(ar==NULL)
		return(TRUE);
	if(!VALID_CFG(cfg) || user==NULL)
		return(FALSE);
	p=ar;
	return(ar_exp(cfg,&p,user));
}

/****************************************************************************/
/* Fills 'str' with record for usernumber starting at start for length bytes*/
/* Called from function ???													*/
/****************************************************************************/
int DLLCALL getuserrec(scfg_t* cfg, int usernumber,int start, int length, char *str)
{
	char	path[256];
	int		i,c,file;

	if(!VALID_CFG(cfg) || usernumber<1 || str==NULL)
		return(-1);
	sprintf(path,"%suser/user.dat",cfg->data_dir);
	if((file=nopen(path,O_RDONLY|O_DENYNONE))==-1) 
		return(errno);
	if(usernumber<1
		|| filelength(file)<(long)((long)(usernumber-1L)*U_LEN)+(long)start) {
		close(file);
		return(-2); 
	}
	lseek(file,(long)((long)(usernumber-1)*U_LEN)+start,SEEK_SET);

	if(length==0)	/* auto-length */
		length=user_rec_len(start);

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

	return(0);
}

/****************************************************************************/
/* Places into user.dat at the offset for usernumber+start for length bytes */
/* Called from various locations											*/
/****************************************************************************/
int DLLCALL putuserrec(scfg_t* cfg, int usernumber,int start, uint length, char *str)
{
	char	str2[256];
	int		file;
	uint	c,i;
	node_t	node;

	if(!VALID_CFG(cfg) || usernumber<1 || str==NULL)
		return(-1);

	sprintf(str2,"%suser/user.dat",cfg->data_dir);
	if((file=nopen(str2,O_RDWR|O_DENYNONE))==-1)
		return(errno);

	if(filelength(file)<((long)usernumber-1)*U_LEN) {
		close(file);
		return(-4);
	}

	if(length==0)	/* auto-length */
		length=user_rec_len(start);

	strcpy(str2,str);
	if(strlen(str2)<length) {
		for(c=strlen(str2);c<length;c++)
			str2[c]=ETX;
		str2[c]=0; 
	}
	lseek(file,(long)((long)((long)((long)usernumber-1)*U_LEN)+start),SEEK_SET);

	i=0;
	while(i<LOOP_NODEDAB
		&& lock(file,(long)((long)(usernumber-1)*U_LEN)+start,length)==-1) {
		if(i)
			mswait(100);
		i++; 
	}

	if(i>=LOOP_NODEDAB) 
		return(-3);

	write(file,str2,length);
	unlock(file,(long)((long)(usernumber-1)*U_LEN)+start,length);
	close(file);
	for(i=1;i<=cfg->sys_nodes;i++) {	/* instant user data update */
		if(i==cfg->node_num)
			continue;
		getnodedat(cfg, i,&node,NULL);
		if(node.useron==usernumber && (node.status==NODE_INUSE
			|| node.status==NODE_QUIET)) {
			getnodedat(cfg, i,&node,&file);
			node.misc|=NODE_UDAT;
			putnodedat(cfg, i,&node,file);
			break; 
		} 
	}

	return(0);
}

/****************************************************************************/
/* Updates user 'usernumber's record (numeric string) by adding 'adj' to it */
/* returns the new value.													*/
/****************************************************************************/
ulong DLLCALL adjustuserrec(scfg_t* cfg, int usernumber, int start, int length, long adj)
{
	char str[256],path[256];
	char tmp[32];
	int i,c,file;
	long val;
	node_t node;

	if(!VALID_CFG(cfg) || usernumber<1) 
		return(0); 

	sprintf(path,"%suser/user.dat",cfg->data_dir);
	if((file=nopen(path,O_RDWR|O_DENYNONE))==-1)
		return(0); 

	if(filelength(file)<((long)usernumber-1)*U_LEN) {
		close(file);
		return(0);
	}

	lseek(file,(long)((long)(usernumber-1)*U_LEN)+start,SEEK_SET);

	if(length==0)	/* auto-length */
		length=user_rec_len(start);

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
	val=atol(str);
	if(adj<0L && val<-adj)		/* don't go negative */
		val=0;
	else val+=adj;
	lseek(file,(long)((long)(usernumber-1)*U_LEN)+start,SEEK_SET);
	putrec(str,0,length,ultoa(val,tmp,10));
	if(write(file,str,length)!=length) {
		unlock(file,(long)((long)(usernumber-1)*U_LEN)+start,length);
		close(file);
		return(val); 
	}
	unlock(file,(long)((long)(usernumber-1)*U_LEN)+start,length);
	close(file);
	for(i=1;i<=cfg->sys_nodes;i++) { /* instant user data update */
		if(i==cfg->node_num)
			continue;
		getnodedat(cfg, i,&node,NULL);
		if(node.useron==usernumber && (node.status==NODE_INUSE
			|| node.status==NODE_QUIET)) {
			getnodedat(cfg, i,&node,&file);
			node.misc|=NODE_UDAT;
			putnodedat(cfg, i,&node,file);
			break; 
		} 
	}
	return(val);
}

/****************************************************************************/
/* Subtract credits from the current user online, accounting for the new    */
/* "free credits" field.                                                    */
/****************************************************************************/
void DLLCALL subtract_cdt(scfg_t* cfg, user_t* user, long amt)
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

/****************************************************************************/
/****************************************************************************/
BOOL DLLCALL logoutuserdat(scfg_t* cfg, user_t* user, time_t now, time_t logontime)
{
	char str[128];
	time_t tused;
	struct tm tm, tm_now;

	if(user==NULL)
		return(FALSE);

	tused=(now-logontime)/60;
	user->tlast=(ushort)(tused > USHRT_MAX ? USHRT_MAX : tused);

	putuserrec(cfg,user->number,U_LASTON,8,ultoa(now,str,16));
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
		resetdailyuserdat(cfg, user);

	return(TRUE);
}

/****************************************************************************/
/****************************************************************************/
void DLLCALL resetdailyuserdat(scfg_t* cfg, user_t* user)
{
	char str[128];

	if(!VALID_CFG(cfg) || user==NULL)
		return;

	/* logons today */
	user->ltoday=0;	
	putuserrec(cfg,user->number,U_LTODAY,5,"0");
	/* e-mails today */
	user->etoday=0;	
	putuserrec(cfg,user->number,U_ETODAY,5,"0");	
	/* posts today */
	user->ptoday=0;	
	putuserrec(cfg,user->number,U_PTODAY,5,"0");
	/* free credits per day */				
	user->freecdt=cfg->level_freecdtperday[user->level];
	putuserrec(cfg,user->number,U_FREECDT,10		
		,ultoa(user->freecdt,str,10));
	/* time used today */
	user->ttoday=0;
	putuserrec(cfg,user->number,U_TTODAY,5,"0");
	/* extra time today */
	user->textra=0;
	putuserrec(cfg,user->number,U_TEXTRA,5,"0");	
}

/****************************************************************************/
/****************************************************************************/
char* DLLCALL usermailaddr(scfg_t* cfg, char* addr, char* name)
{
	int i;

	if(!VALID_CFG(cfg) || addr==NULL || name==NULL)
		return(NULL);

	if(strchr(name,'@')!=NULL) { /* Avoid double-@ */
		strcpy(addr,name);
		return(addr);
	}
	if(strchr(name,'.') && strchr(name,' '))
		sprintf(addr,"\"%s\"@",name);
	else {
		sprintf(addr,"%s@",name);
		/* convert "first last@" to "first.last@" */
		for(i=0;addr[i];i++)
			if(addr[i]==' ' || addr[i]&0x80)
				addr[i]='.';
		strlwr(addr);
	}
	strcat(addr,cfg->sys_inetaddr);
	return(addr);
}

char* DLLCALL alias(scfg_t* cfg, char* name, char* buf)
{
	int		file;
	char	line[128];
	char*	p;
	char*	np;
	char*	tp;
	char	fname[MAX_PATH+1];
	size_t	namelen;
	size_t	cmplen;
	FILE*	fp;

	if(!VALID_CFG(cfg) || name==NULL || buf==NULL)
		return(NULL);

	p=name;

	sprintf(fname,"%salias.cfg",cfg->ctrl_dir);
	if((file=sopen(fname,O_RDONLY|O_BINARY,SH_DENYNO))==-1)
		return(name);

	if((fp=fdopen(file,"rb"))==NULL) {
		close(file);
		return(name);
	}

	while(!feof(fp)) {
		if(!fgets(line,sizeof(line),fp))
			break;
		np=line;
		while(*np && *np<=' ') np++;
		if(*np==';')
			continue;
		tp=np;
		while(*tp && *tp>' ') tp++;
		if(*tp) *tp=0;
		if(*np=='*') {
			np++;
			cmplen=strlen(np);
			namelen=strlen(name);
			if(namelen<cmplen)
				continue;
			if(strnicmp(np,name+(namelen-cmplen),cmplen))
				continue;
			np=tp+1;
			while(*np && *np<=' ') np++;
			truncsp(np);
			if(*np=='*') 
				sprintf(buf,"%.*s%s",(int)(namelen-cmplen),name,np+1);
			else
				strcpy(buf,np);
			p=buf;
			break;
		}
		if(!stricmp(np,name)) {
			np=tp+1;
			while(*np && *np<=' ') np++;
			truncsp(np);
			strcpy(buf,np);
			p=buf;
			break;
		}
	}
	fclose(fp);
	return(p);
}

int DLLCALL newuserdat(scfg_t* cfg, user_t* user)
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

	sprintf(str,"%suser/name.dat",cfg->data_dir);
	if(fexist(str)) {
		if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
			return(errno); 
		}
		last=filelength(file)/(LEN_ALIAS+2);	   /* total users */
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

	sprintf(str,"%sfile/%04u.in",cfg->data_dir,user->number);  /* delete any files */
	delfiles(str,ALLFILES);                                    /* waiting for user */
	rmdir(str);
	sprintf(tmp,"%04u.*",user->number);
	sprintf(str,"%sfile",cfg->data_dir);
	delfiles(str,tmp);

	sprintf(str,"%suser/ptrs/%04u.ixb",cfg->data_dir,user->number); /* msg ptrs */
	remove(str);
	sprintf(str,"%smsgs/%04u.msg",cfg->data_dir,user->number); /* delete short msg */
	remove(str);
	sprintf(str,"%suser/%04u.msg",cfg->data_dir,user->number); /* delete ex-comment */
	remove(str);
	sprintf(str,"%suser/%04u.sig",cfg->data_dir,user->number); /* delete signature */
	remove(str);

	/* Update daily statistics database (for system and node) */

	for(i=0;i<2;i++) {
		sprintf(str,"%sdsts.dab",i ? cfg->ctrl_dir : cfg->node_dir);
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
int DLLCALL user_rec_len(int offset)
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

		/* 2 digits integers (0-99) */
		case U_LEVEL:
		case U_TL:
		case U_ROWS:
		case U_LEECH:	/* actually, 2 hex digits */
			return(2);

		/* Single digits chars */
		case U_SEX:
		case U_PROT:
			return(1);
	}

	return(-1);
}

BOOL DLLCALL is_download_free(scfg_t* cfg, uint dirnum, user_t* user)
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

	return(chk_ar(cfg,cfg->dir[dirnum]->ex_ar,user));
}

/****************************************************************************/
/* Add an IP address (with comment) to the IP filter/trashcan file			*/
/* ToDo: Move somewhere more appropriate (filter.c?)						*/
/****************************************************************************/
BOOL DLLCALL filter_ip(scfg_t* cfg, char* prot, char* reason, char* ip_addr, char* username)
{
	char	filename[MAX_PATH+1];
	char	tstr[64];
    FILE*	fp;
    time_t	now=time(NULL);

	sprintf(filename,"%sip.can",cfg->text_dir);

    if((fp=fopen(filename,"a"))==NULL)
    	return(FALSE);

    fprintf(fp,"\n;%s %s by %s on %s\n%s\n"
    	,prot,reason,username,timestr(cfg,&now,tstr),ip_addr);

    fclose(fp);
	return(TRUE);
}
