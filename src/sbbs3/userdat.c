/* userdat.c */

/* Synchronet user data-related routines (exported) */

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

#include "sbbs.h"
#include "cmdshell.h"

/* convenient space-saving global variables */
char* crlf="\r\n";
char* nulstr="";

#define REPLACE_CHARS(str,ch1,ch2)	for(c=0;str[c];c++)	if(str[c]==ch1) str[c]=ch2;


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
/* Returns the number of the last user in user.dat (deleted ones too)		*/
/****************************************************************************/
uint DLLCALL lastuser(scfg_t* cfg)
{
	char str[256];
	long length;

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

	if(!user->number) {
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
	getrec(userdat,U_CURSUB,8,user->cursub);
	getrec(userdat,U_CURDIR,8,user->curdir);
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
			user->qwk=(QWK_FILES|QWK_ATTACH|QWK_EMAIL|QWK_DELMAIL); }
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
				user->misc&=~(ANSI|RIP|WIP);
				user->misc|=autoterm; }
		} }
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

	if(!user->number) 
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

	putrec(userdat,U_CURSUB,8,user->cursub);
	putrec(userdat,U_CURDIR,8,user->curdir);
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
char* DLLCALL username(scfg_t* cfg, int usernumber,char *strin)
{
    char	str[256];
    int		c;
    int		file;

	if(usernumber<1) {
		strin[0]=0;
		return(strin); }
	sprintf(str,"%suser/name.dat",cfg->data_dir);
	if(flength(str)<1L) {
		strin[0]=0;
		return(strin); }
	if((file=nopen(str,O_RDONLY))==-1) {
		strin[0]=0;
		return(strin); }
	if(filelength(file)<(long)((long)usernumber*(LEN_ALIAS+2))) {
		close(file);
		strin[0]=0;
		return(strin); }
	lseek(file,(long)((long)(usernumber-1)*(LEN_ALIAS+2)),SEEK_SET);
	read(file,strin,LEN_ALIAS);
	close(file);
	for(c=0;c<LEN_ALIAS;c++)
		if(strin[c]==ETX) break;
	strin[c]=0;
	if(!c)
		strcpy(strin,"DELETED USER");
	return(strin);
}

/****************************************************************************/
/* Puts 'name' into slot 'number' in user/name.dat							*/
/****************************************************************************/
int DLLCALL putusername(scfg_t* cfg, int number, char *name)
{
	char str[256];
	int file;
	long length;
	uint total_users;

	if (number<1) 
		return(-1);

	sprintf(str,"%suser/name.dat", cfg->data_dir);
	if((file=nopen(str,O_RDWR|O_CREAT))==-1) 
		return(-2); 
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
	write(file,str,LEN_ALIAS+2);
	close(file);

	return(0);
}

/****************************************************************************/
/* Places into 'strout' CR or ETX terminated string starting at             */
/* 'start' and ending at 'start'+'length' or terminator from 'strin'        */
/****************************************************************************/
void DLLCALL getrec(char *strin,int start,int length,char *strout)
{
    int i=0,stop;

	stop=start+length;
	while(start<stop) {
		if(strin[start]==ETX || strin[start]==CR || strin[start]==LF)
			break;
		strout[i++]=strin[start++]; 
	}
	strout[i]=0;
}

/****************************************************************************/
/* Places into 'strout', 'strin' starting at 'start' and ending at          */
/* 'start'+'length'                                                         */
/****************************************************************************/
void DLLCALL putrec(char *strout,int start,int length,char *strin)
{
    int i=0,j;

	j=strlen(strin);
	while(i<j && i<length)
		strout[start++]=strin[i++];
	while(i++<length)
		strout[start++]=ETX;
}


/****************************************************************************/
/* Returns the age derived from the string 'birth' in the format MM/DD/YY	*/
/* Called from functions statusline, main_sec, xfer_sec, useredit and 		*/
/* text files																*/
/****************************************************************************/
char DLLCALL getage(scfg_t* cfg, char *birth)
{
	char	age;
	struct	tm * tm;
	time_t	now;

	if(!atoi(birth) || !atoi(birth+3))	/* Invalid */
		return(0);

	now=time(NULL);
	tm=localtime(&now);
	if(tm==NULL)
		return(0);
	age=(tm->tm_year)-(((birth[6]&0xf)*10)+(birth[7]&0xf));
	if(age>105)
		age-=105;
	tm->tm_mon++;	/* convert to 1 based */
	if(cfg->sys_misc&SM_EURODATE) {		/* DD/MM/YY format */
		if(atoi(birth)>31 || atoi(birth+3)>12)
			return(0);
		if(((birth[3]&0xf)*10)+(birth[4]&0xf)>tm->tm_mon ||
			(((birth[3]&0xf)*10)+(birth[4]&0xf)==tm->tm_mon &&
			((birth[0]&0xf)*10)+(birth[1]&0xf)>tm->tm_mday))
			age--; }
	else {							/* MM/DD/YY format */
		if(atoi(birth)>12 || atoi(birth+3)>31)
			return(0);
		if(((birth[0]&0xf)*10)+(birth[1]&0xf)>tm->tm_mon ||
			(((birth[0]&0xf)*10)+(birth[1]&0xf)==tm->tm_mon &&
			((birth[3]&0xf)*10)+(birth[4]&0xf)>tm->tm_mday))
			age--; }
	if(age<0)
		return(0);
	return(age);
}


/****************************************************************************/
/* Reads the data for node number 'number' into the structure 'node'        */
/* from node.dab															*/
/* if lockit is non-zero, locks this node's record. putnodedat() unlocks it */
/****************************************************************************/
int DLLCALL getnodedat(scfg_t* cfg, uint number, node_t *node, int* fp)
{
	char	str[MAX_PATH+1];
	int		count;
	int		file;

	if(!number || number>cfg->sys_nodes)
		return(-1);

	sprintf(str,"%snode.dab",cfg->ctrl_dir);
	if((file=nopen(str,O_RDWR|O_DENYNONE))==-1) {
		memset(node,0,sizeof(node_t));
		if(fp!=NULL)
			*fp=file;
		return(errno); 
	}

	number--;	/* make zero based */
	for(count=0;count<LOOP_NODEDAB;count++) {
		if(count)
			mswait(100);
		lseek(file,(long)number*sizeof(node_t),SEEK_SET);
		if(fp!=NULL
			&& lock(file,(long)number*sizeof(node_t),sizeof(node_t))==-1) 
			continue; 
		if(read(file,node,sizeof(node_t))==sizeof(node_t))
			break;
	}

	if(fp==NULL)
		close(file);
	else
		*fp=file;

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

	if(!number || number>cfg->sys_nodes || file<0) {
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

/****************************************************************************/
/* Displays the information for node number 'number' contained in 'node'    */
/****************************************************************************/
void DLLCALL printnodedat(scfg_t* cfg, uint number, node_t* node)
{
	char	mer[3];
	char	tmp[128];
    int		hour;

	printf("Node %2d: ",number);
	switch(node->status) {
		case NODE_WFC:
			printf("Waiting for call");
			break;
		case NODE_OFFLINE:
			printf("Offline");
			break;
		case NODE_NETTING:
			printf("Networking");
			break;
		case NODE_LOGON:
			printf("At logon prompt");
			break;
		case NODE_EVENT_WAITING:
			printf("Waiting for all nodes to become inactive");
			break;
		case NODE_EVENT_LIMBO:
			printf("Waiting for node %d to finish external event",node->aux);
			break;
		case NODE_EVENT_RUNNING:
			printf("Running external event");
			break;
		case NODE_NEWUSER:
			printf("New user");
			printf(" applying for access ");
			if(!node->connection)
				printf("locally");
			else if(node->connection==0xffff)
				printf("via telnet");
			else
				printf("at %ubps",node->connection);
			break;
		case NODE_QUIET:
		case NODE_INUSE:
			printf("%s ",username(cfg, node->useron, tmp));
			switch(node->action) {
				case NODE_MAIN:
					printf("at main menu");
					break;
				case NODE_RMSG:
					printf("reading messages");
					break;
				case NODE_RMAL:
					printf("reading mail");
					break;
				case NODE_RSML:
					printf("reading sent mail");
					break;
				case NODE_RTXT:
					printf("reading text files");
					break;
				case NODE_PMSG:
					printf("posting message");
					break;
				case NODE_SMAL:
					printf("sending mail");
					break;
				case NODE_AMSG:
					printf("posting auto-message");
					break;
				case NODE_XTRN:
					if(!node->aux)
						printf("at external program menu");
					else
						printf("running external program #%d",node->aux);
					break;
				case NODE_DFLT:
					printf("changing defaults");
					break;
				case NODE_XFER:
					printf("at transfer menu");
					break;
				case NODE_RFSD:
					printf("retrieving from device #%d",node->aux);
					break;
				case NODE_DLNG:
					printf("downloading");
					break;
				case NODE_ULNG:
					printf("uploading");
					break;
				case NODE_BXFR:
					printf("transferring bidirectional");
					break;
				case NODE_LFIL:
					printf("listing files");
					break;
				case NODE_LOGN:
					printf("logging on");
					break;
				case NODE_LCHT:
					printf("in local chat with sysop");
					break;
				case NODE_MCHT:
					if(node->aux) {
						printf("in multinode chat channel %d",node->aux&0xff);
						if(node->aux&0x1f00) { /* password */
							putchar('*');
							printf(" %s",unpackchatpass(tmp,node)); } }
					else
						printf("in multinode global chat channel");
					break;
				case NODE_PAGE:
					printf("paging node %u for private chat",node->aux);
					break;
				case NODE_PCHT:
					printf("in private chat with node %u",node->aux);
					break;
				case NODE_GCHT:
					printf("chatting with The Guru");
					break;
				case NODE_CHAT:
					printf("in chat section");
					break;
				case NODE_TQWK:
					printf("transferring QWK packet");
					break;
				case NODE_SYSP:
					printf("performing sysop activities");
					break;
				default:
					printf(ultoa(node->action,tmp,10));
					break;  }
			if(!node->connection)
				printf(" locally");
			else if(node->connection==0xffff)
				printf(" via telnet");
			else
				printf(" at %ubps",node->connection);
			if(node->action==NODE_DLNG) {
				if((node->aux/60)>=12) {
					if(node->aux/60==12)
						hour=12;
					else
						hour=(node->aux/60)-12;
					strcpy(mer,"pm"); }
				else {
					if((node->aux/60)==0)    /* 12 midnite */
						hour=12;
					else hour=node->aux/60;
					strcpy(mer,"am"); }
				printf(" ETA %02d:%02d %s"
					,hour,node->aux-((node->aux/60)*60),mer); }
			break; }
	if(node->misc&(NODE_LOCK|NODE_POFF|NODE_AOFF|NODE_MSGW|NODE_NMSG)) {
		printf(" (");
		if(node->misc&NODE_AOFF)
			putchar('A');
		if(node->misc&NODE_LOCK)
			putchar('L');
		if(node->misc&(NODE_MSGW|NODE_NMSG))
			putchar('M');
		if(node->misc&NODE_POFF)
			putchar('P');
		putchar(')'); }
	if(((node->misc
		&(NODE_ANON|NODE_UDAT|NODE_INTR|NODE_RRUN|NODE_EVENT|NODE_DOWN))
		|| node->status==NODE_QUIET)) {
		printf(" [");
		if(node->misc&NODE_ANON)
			putchar('A');
		if(node->misc&NODE_INTR)
			putchar('I');
		if(node->misc&NODE_RRUN)
			putchar('R');
		if(node->misc&NODE_UDAT)
			putchar('U');
		if(node->status==NODE_QUIET)
			putchar('Q');
		if(node->misc&NODE_EVENT)
			putchar('E');
		if(node->misc&NODE_DOWN)
			putchar('D');
		if(node->misc&NODE_LCHAT)
			putchar('C');
		putchar(']'); }
	if(node->errors)
		printf(" %d error%c",node->errors, node->errors>1 ? 's' : '\0' );
	printf("\n");
}

/****************************************************************************/
uint DLLCALL userdatdupe(scfg_t* cfg, uint usernumber, uint offset, uint datlen, char *dat
    ,BOOL del)
{
    char	str[MAX_PATH+1];
    uint	i;
	int		file;
    long	l,length;

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
					continue; } }
			unlock(file,l,U_LEN);
			close(file);
			return((l/U_LEN)+1); }
		else
			unlock(file,l,U_LEN); }
	close(file);
	return(0);
}

/****************************************************************************/
/* Creates a short message for 'usernumber' than contains 'strin'           */
/****************************************************************************/
int DLLCALL putsmsg(scfg_t* cfg, int usernumber, char *strin)
{
    char str[256];
    int file,i;
    node_t node;

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

static BOOL ar_exp(scfg_t* cfg, uchar **ptrptr, user_t* user)
{
	BOOL	result,not,or,equal;
	uint	i,n,artype=AR_LEVEL,age;
	ulong	l;
	time_t	now;
	struct tm * tm;

	result = TRUE;

	for(;(**ptrptr);(*ptrptr)++) {

		if((**ptrptr)==AR_ENDNEST)
			break;

		not=or=equal = FALSE;

		if((**ptrptr)==AR_OR) {
			or=1;
			(*ptrptr)++; }
		
		if((**ptrptr)==AR_NOT) {
			not=1;
			(*ptrptr)++; }

		if((**ptrptr)==AR_EQUAL) {
			equal=1;
			(*ptrptr)++; }

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
			continue; }

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
				break; }

		n=(**ptrptr);
		i=(*(short *)*ptrptr);
		switch(artype) {
			case AR_LEVEL:
				if((equal && user->level!=n) || (!equal && user->level<n))
					result=not;
				else
					result=!not;
				break;
			case AR_AGE:
				age=getage(cfg,user->birth);
				if((equal && age!=n) || (!equal && age<n))
					result=not;
				else
					result=!not;
				break;
			case AR_BPS:
				result=!not;
				(*ptrptr)++;
				break;
			case AR_ANSI:
				if(!(user->misc&ANSI))
					result=not;
				else result=!not;
				break;
			case AR_RIP:
				if(!(user->misc&RIP))
					result=not;
				else result=!not;
				break;
			case AR_WIP:
				if(!(user->misc&WIP))
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
				if(!(user->misc&EXPERT))
					result=not;
				else result=!not;
				break;
			case AR_SYSOP:
				if(user->level<SYSOP_LEVEL)
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
				tm=localtime(&now);
				if(tm==NULL || (equal && tm->tm_wday!=(int)n) 
					|| (!equal && tm->tm_wday<(int)n))
					result=not;
				else
					result=!not;
				break;
			case AR_CREDIT:
				l=(ulong)i*1024UL;
				if((equal && user->cdt+user->freecdt!=l)
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
				if((equal && user->number!=i) || (!equal && user->number<i))
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
				if(!user->expire || now+((long)i*24L*60L*60L)>user->expire)
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
				if((now-user->laston)/(24L*60L*60L)<(long)i)
					result=not;
				else
					result=!not;
				(*ptrptr)++;
				break;
			case AR_LOGONS:
				if((equal && user->logons!=i) || (!equal && user->logons<i))
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
				tm=localtime(&now);
				if(tm==NULL || (tm->tm_hour*60)+tm->tm_min<(int)i)
					result=not;
				else
					result=!not;
				(*ptrptr)++;
				break;
			case AR_PCR:
				if(user->logons>user->posts
					&& (!user->posts || 100/(user->logons/user->posts)<(long)n))
					result=not;
				else
					result=!not;
				break;
			case AR_UDR:	/* up/download byte ratio */
				l=user->dlb;
				if(!l) l=1;
				if(user->dlb>user->ulb
					&& (!user->ulb || 100/(l/user->ulb)<n))
					result=not;
				else
					result=!not;
				break;
			case AR_UDFR:	/* up/download file ratio */
				i=user->dls;
				if(!i) i=1;
				if(user->dls>user->uls
					&& (!user->uls || 100/(i/user->uls)<n))
					result=not;
				else
					result=!not;
				break;
			case AR_FLAG1:
				if((!equal && !(user->flags1&FLAG(n)))
					|| (equal && user->flags1!=FLAG(n)))
					result=not;
				else
					result=!not;
				break;
			case AR_FLAG2:
				if((!equal && !(user->flags2&FLAG(n)))
					|| (equal && user->flags2!=FLAG(n)))
					result=not;
				else
					result=!not;
				break;
			case AR_FLAG3:
				if((!equal && !(user->flags3&FLAG(n)))
					|| (equal && user->flags3!=FLAG(n)))
					result=not;
				else
					result=!not;
				break;
			case AR_FLAG4:
				if((!equal && !(user->flags4&FLAG(n)))
					|| (equal && user->flags4!=FLAG(n)))
					result=not;
				else
					result=!not;
				break;
			case AR_REST:
				if((!equal && !(user->rest&FLAG(n)))
					|| (equal && user->rest!=FLAG(n)))
					result=not;
				else
					result=!not;
				break;
			case AR_EXEMPT:
				if((!equal && !(user->exempt&FLAG(n)))
					|| (equal && user->exempt!=FLAG(n)))
					result=not;
				else
					result=!not;
				break;
			case AR_SEX:
				if(user->sex!=n)
					result=not;
				else
					result=!not;
				break; 
			case AR_SHELL:
				if(user->shell>=cfg->total_shells
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

	if(!usernumber)
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

	if(usernumber<1)
		return(-1);

	sprintf(str2,"%suser/user.dat",cfg->data_dir);
	if((file=nopen(str2,O_RDWR|O_DENYNONE))==-1)
		return(errno);

	if(filelength(file)<((long)usernumber-1)*U_LEN) {
		close(file);
		return(-4);
	}

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

	if(usernumber<1) 
		return(0UL); 

	sprintf(path,"%suser/user.dat",cfg->data_dir);
	if((file=nopen(path,O_RDWR|O_DENYNONE))==-1)
		return(0UL); 

	if(filelength(file)<((long)usernumber-1)*U_LEN) {
		close(file);
		return(0);
	}

	lseek(file,(long)((long)(usernumber-1)*U_LEN)+start,SEEK_SET);

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
		return(0UL); 
	}
	for(c=0;c<length;c++)
		if(str[c]==ETX || str[c]==CR) break;
	str[c]=0;
	val=atol(str);
	if(adj<0L && val<-adj)		/* don't go negative */
		val=0UL;
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

	if(!amt)
		return;
	if(user->freecdt) {
		if((ulong)amt>user->freecdt) {      /* subtract both credits and */
			mod=amt-user->freecdt;   /* free credits */
			putuserrec(cfg, user->number,U_FREECDT,10,"0");
			user->freecdt=0;
			user->cdt=adjustuserrec(cfg, user->number,U_CDT,10,-mod); }
		else {                          /* subtract just free credits */
			user->freecdt-=amt;
			putuserrec(cfg, user->number,U_FREECDT,10
				,ultoa(user->freecdt,tmp,10)); } }
	else    /* no free credits */
		user->cdt=adjustuserrec(cfg, user->number,U_CDT,10,-amt);
}

/****************************************************************************/
/****************************************************************************/
BOOL DLLCALL logoutuserdat(scfg_t* cfg, user_t* user, time_t now, time_t logontime)
{
	char str[128];
	struct tm* tm, tm_now;

	user->tlast=(now-logontime)/60;

	putuserrec(cfg,user->number,U_LASTON,8,ultoa(now,str,16));
	putuserrec(cfg,user->number,U_TLAST,5,ultoa(user->tlast,str,10));
	adjustuserrec(cfg,user->number,U_TIMEON,5,user->tlast);
	adjustuserrec(cfg,user->number,U_TTODAY,5,user->tlast);

	/* Convert time_t to struct tm */
	tm=localtime(&now);
	if(tm==NULL)
		return(FALSE);
	tm_now=*tm;

	tm=localtime(&logontime);
	if(tm==NULL)
		return(FALSE);

	/* Reset daily stats if new day */
	if(tm->tm_mday!=tm_now.tm_mday) 
		resetdailyuserdat(cfg, user);

	return(TRUE);
}

/****************************************************************************/
/****************************************************************************/
void DLLCALL resetdailyuserdat(scfg_t* cfg, user_t* user)
{
	char str[128];

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
				sprintf(buf,"%.*s%s",namelen-cmplen,name,np+1);
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
