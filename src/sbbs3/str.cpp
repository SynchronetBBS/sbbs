/* str.cpp */

/* Synchronet high-level string i/o routines */

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

/****************************************************************************/
/* Lists all users who have access to the current sub.                      */
/****************************************************************************/
void sbbs_t::userlist(long mode)
{
	char	name[256],sort=0;
	char 	tmp[512];
	int		i,j,k,users=0;
	char *	line[2500];
	user_t	user;

	if(lastuser(&cfg)<=(sizeof(line)/sizeof(line[0])))
		sort=yesno(text[SortAlphaQ]);
	if(sort) {
		bputs(text[CheckingSlots]); }
	else {
		CRLF; }
	j=0;
	k=lastuser(&cfg);
	for(i=1;i<=k && !msgabort();i++) {
		if(sort && (online==ON_LOCAL || !rioctl(TXBC)))
			bprintf("%-4d\b\b\b\b",i);
		user.number=i;
		getuserdat(&cfg,&user);
		if(user.misc&(DELETED|INACTIVE))
			continue;
		users++;
		if(mode==UL_SUB) {
			if(!usrgrps)
				continue;
			if(!chk_ar(cfg.grp[usrgrp[curgrp]]->ar,&user))
				continue;
			if(!chk_ar(cfg.sub[usrsub[curgrp][cursub[curgrp]]]->ar,&user)
				|| (cfg.sub[usrsub[curgrp][cursub[curgrp]]]->read_ar[0]
				&& !chk_ar(cfg.sub[usrsub[curgrp][cursub[curgrp]]]->read_ar,&user)))
				continue; }
		else if(mode==UL_DIR) {
			if(user.rest&FLAG('T'))
				continue;
			if(!usrlibs)
				continue;
			if(!chk_ar(cfg.lib[usrlib[curlib]]->ar,&user))
				continue;
			if(!chk_ar(cfg.dir[usrdir[curlib][curdir[curlib]]]->ar,&user))
				continue; }
		if(sort) {
			if((line[j]=(char *)MALLOC(128))==0) {
				errormsg(WHERE,ERR_ALLOC,nulstr,83);
				for(i=0;i<j;i++)
					FREE(line[i]);
				return; }
			sprintf(name,"%s #%d",user.alias,i);
			sprintf(line[j],text[UserListFmt],name
				,cfg.sys_misc&SM_LISTLOC ? user.location : user.note
				,unixtodstr(&cfg,user.laston,tmp)
				,user.modem); }
		else {
			sprintf(name,"%s #%u",user.alias,i);
			bprintf(text[UserListFmt],name
				,cfg.sys_misc&SM_LISTLOC ? user.location : user.note
				,unixtodstr(&cfg,user.laston,tmp)
				,user.modem); }
		j++; }
	if(i<=k) {	/* aborted */
		if(sort)
			for(i=0;i<j;i++)
				FREE(line[i]);
		return; }
	if(!sort) {
		CRLF; }
	bprintf(text[NTotalUsers],users);
	if(mode==UL_SUB)
		bprintf(text[NUsersOnCurSub],j);
	else if(mode==UL_DIR)
		bprintf(text[NUsersOnCurDir],j);
	if(!sort)
		return;
	CRLF;
	qsort((void *)line,j,sizeof(line[0])
		,(int(*)(const void*, const void*))pstrcmp);
	for(i=0;i<j && !msgabort();i++)
		bputs(line[i]);
	for(i=0;i<j;i++)
		FREE(line[i]);
}

/****************************************************************************/
/* SIF input function. See SIF.DOC for more info        					*/
/****************************************************************************/
void sbbs_t::sif(char *fname, char *answers, long len)
{
	char	str[256],tmplt[256],HUGE16 *buf;
	uint	t,max,min,mode,cr;
	int		file;
	long	length,l=0,m,top,a=0;

	sprintf(str,"%s%s.sif",cfg.text_dir,fname);
	if((file=nopen(str,O_RDONLY))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
		answers[0]=0;
		return; }
	length=filelength(file);
	if((buf=(char *)MALLOC(length))==0) {
		close(file);
		errormsg(WHERE,ERR_ALLOC,str,length);
		answers[0]=0;
		return; }
	if(lread(file,buf,length)!=length) {
		close(file);
		errormsg(WHERE,ERR_READ,str,length);
		answers[0]=0;
		return; }
	close(file);
	while(l<length && online) {
		mode=min=max=t=cr=0;
		top=l;
		while(l<length && buf[l++]!=STX);
		for(m=l;m<length;m++)
			if(buf[m]==ETX || !buf[m]) {
				buf[m]=0;
				break; }
		if(l>=length) break;
		if(online==ON_REMOTE) {
			rioctl(IOCM|ABORT);
			rioctl(IOCS|ABORT); }
		putmsg(buf+l,P_SAVEATR);
		m++;
		if(toupper(buf[m])!='C' && toupper(buf[m])!='S')
			continue;
		SYNC;
		if(online==ON_REMOTE)
			rioctl(IOSM|ABORT);
		if(a>=len) {
			errormsg(WHERE,ERR_LEN,fname,len);
			break; }
		if((buf[m]&0xdf)=='C') {
    		if((buf[m+1]&0xdf)=='U') {		/* Uppercase only */
				mode|=K_UPPER;
				m++; }
			else if((buf[m+1]&0xdf)=='N') {	/* Numbers only */
				mode|=K_NUMBER;
				m++; }
			if((buf[m+1]&0xdf)=='L') {		/* Draw line */
        		if(useron.misc&COLOR)
					attr(cfg.color[clr_inputline]);
				else
					attr(BLACK|BG_LIGHTGRAY);
				bputs(" \b");
				m++; }
			if((buf[m+1]&0xdf)=='R') {		/* Add CRLF */
				cr=1;
				m++; }
			if(buf[m+1]=='"') {
				m+=2;
				for(l=m;l<length;l++)
					if(buf[l]=='"') {
						buf[l]=0;
						break; }
				answers[a++]=(char)getkeys((char *)buf+m,0); }
			else {
				answers[a]=getkey(mode);
				outchar(answers[a++]);
				attr(LIGHTGRAY);
				CRLF; }
			if(cr) {
				answers[a++]=CR;
				answers[a++]=LF; } }
		else if((buf[m]&0xdf)=='S') {		/* String */
			if((buf[m+1]&0xdf)=='U') {		/* Uppercase only */
				mode|=K_UPPER;
				m++; }
			else if((buf[m+1]&0xdf)=='F') { /* Force Upper/Lowr case */
				mode|=K_UPRLWR;
				m++; }
			else if((buf[m+1]&0xdf)=='N') {	/* Numbers only */
				mode|=K_NUMBER;
				m++; }
			if((buf[m+1]&0xdf)=='L') {		/* Draw line */
				mode|=K_LINE;
				m++; }
			if((buf[m+1]&0xdf)=='R') {		/* Add CRLF */
				cr=1;
				m++; }
			if(isdigit(buf[m+1])) {
				max=buf[++m]&0xf;
				if(isdigit(buf[m+1]))
					max=max*10+(buf[++m]&0xf); }
			if(buf[m+1]=='.' && isdigit(buf[m+2])) {
				m++;
				min=buf[++m]&0xf;
				if(isdigit(buf[m+1]))
					min=min*10+(buf[++m]&0xf); }
			if(buf[m+1]=='"') {
				m++;
				mode&=~K_NUMBER;
				while(buf[++m]!='"' && t<80)
					tmplt[t++]=buf[m];
				tmplt[t]=0;
				max=strlen(tmplt); }
			if(t) {
				if(gettmplt(str,tmplt,mode)<min) {
					l=top;
					continue; } }
			else {
				if(!max)
					continue;
				if(getstr(str,max,mode)<min) {
					l=top;
					continue; } }
			if(!cr) {
				for(cr=0;str[cr];cr++)
					answers[a+cr]=str[cr];
				while(cr<max)
					answers[a+cr++]=ETX;
				a+=max; }
			else {
				putrec(answers,a,max,str);
				putrec(answers,a+max,2,crlf);
				a+=max+2; } } }
	answers[a]=0;
	FREE((char *)buf);
}

/****************************************************************************/
/* SIF output function. See SIF.DOC for more info        					*/
/****************************************************************************/
void sbbs_t::sof(char *fname, char *answers, long len)
{
	char str[256],HUGE16 *buf,max,min,cr;
	int file;
	long length,l=0,m,a=0;

	sprintf(str,"%s%s.sif",cfg.text_dir,fname);
	if((file=nopen(str,O_RDONLY))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
		answers[0]=0;
		return; }
	length=filelength(file);
	if((buf=(char *)MALLOC(length))==0) {
		close(file);
		errormsg(WHERE,ERR_ALLOC,str,length);
		answers[0]=0;
		return; }
	if(lread(file,buf,length)!=length) {
		close(file);
		errormsg(WHERE,ERR_READ,str,length);
		answers[0]=0;
		return; }
	close(file);
	while(l<length && online) {
		min=max=cr=0;
		while(l<length && buf[l++]!=STX);
		for(m=l;m<length;m++)
			if(buf[m]==ETX || !buf[m]) {
				buf[m]=0;
				break; }
		if(l>=length) break;
		if(online==ON_REMOTE) {
			rioctl(IOCM|ABORT);
			rioctl(IOCS|ABORT); }
		putmsg(buf+l,P_SAVEATR);
		m++;
		if(toupper(buf[m])!='C' && toupper(buf[m])!='S')
			continue;
		SYNC;
		if(online==ON_REMOTE)
			rioctl(IOSM|ABORT);
		if(a>=len) {
			bprintf("\r\nSOF: %s defined more data than buffer size "
				"(%lu bytes)\r\n",fname,len);
			break; }
		if((buf[m]&0xdf)=='C') {
			if((buf[m+1]&0xdf)=='U')  		/* Uppercase only */
				m++;
			else if((buf[m+1]&0xdf)=='N')  	/* Numbers only */
				m++;
			if((buf[m+1]&0xdf)=='L') {		/* Draw line */
        		if(useron.misc&COLOR)
					attr(cfg.color[clr_inputline]);
				else
					attr(BLACK|BG_LIGHTGRAY);
				bputs(" \b");
				m++; }
			if((buf[m+1]&0xdf)=='R') {		/* Add CRLF */
				cr=1;
				m++; }
			outchar(answers[a++]);
			attr(LIGHTGRAY);
			CRLF;
			if(cr)
				a+=2; }
		else if((buf[m]&0xdf)=='S') {		/* String */
			if((buf[m+1]&0xdf)=='U')
				m++;
			else if((buf[m+1]&0xdf)=='F')
				m++;
			else if((buf[m+1]&0xdf)=='N')   /* Numbers only */
				m++;
			if((buf[m+1]&0xdf)=='L') {
        		if(useron.misc&COLOR)
					attr(cfg.color[clr_inputline]);
				else
					attr(BLACK|BG_LIGHTGRAY);
				m++; }
			if((buf[m+1]&0xdf)=='R') {
				cr=1;
				m++; }
			if(isdigit(buf[m+1])) {
				max=buf[++m]&0xf;
				if(isdigit(buf[m+1]))
					max=max*10+(buf[++m]&0xf); }
			if(buf[m+1]=='.' && isdigit(buf[m+2])) {
				m++;
				min=buf[++m]&0xf;
				if(isdigit(buf[m+1]))
					min=min*10+(buf[++m]&0xf); }
			if(buf[m+1]=='"') {
				max=0;
				m++;
				while(buf[++m]!='"' && max<80)
					max++; }
			if(!max)
				continue;
			getrec(answers,a,max,str);
			bputs(str);
			attr(LIGHTGRAY);
			CRLF;
			if(!cr)
				a+=max;
			else
				a+=max+2; } }
	FREE((char *)buf);
}

/****************************************************************************/
/* Creates data file 'datfile' from input via sif file 'siffile'            */
/****************************************************************************/
void sbbs_t::create_sif_dat(char *siffile, char *datfile)
{
	char *buf;
	int file;

	if((buf=(char *)MALLOC(SIF_MAXBUF))==NULL) {
		errormsg(WHERE,ERR_ALLOC,siffile,SIF_MAXBUF);
		return; }
	memset(buf,SIF_MAXBUF,0);	 /* initialize to null */
	sif(siffile,buf,SIF_MAXBUF);
	if((file=nopen(datfile,O_WRONLY|O_TRUNC|O_CREAT))==-1) {
		FREE(buf);
		errormsg(WHERE,ERR_OPEN,datfile,O_WRONLY|O_TRUNC|O_CREAT);
		return; }
	write(file,buf,strlen(buf));
	close(file);
	FREE(buf);
}

/****************************************************************************/
/* Reads data file 'datfile' and displays output via sif file 'siffile'     */
/****************************************************************************/
void sbbs_t::read_sif_dat(char *siffile, char *datfile)
{
	char *buf;
	int file;
	long length;

	if((file=nopen(datfile,O_RDONLY))==-1) {
		errormsg(WHERE,ERR_OPEN,datfile,O_RDONLY);
		return; }
	length=filelength(file);
	if(!length) {
		close(file);
		return; }
	if((buf=(char *)MALLOC(length))==NULL) {
		close(file);
		errormsg(WHERE,ERR_ALLOC,datfile,length);
		return; }
	read(file,buf,length);
	close(file);
	sof(siffile,buf,length);
	FREE(buf);
}

/****************************************************************************/
/* Get string by template. A=Alpha, N=Number, !=Anything                    */
/* First character MUST be an A,N or !.                                     */
/* Modes - K_LINE and K_UPPER are supported.                                */
/****************************************************************************/
uint sbbs_t::gettmplt(char *strout,char *templt, long mode)
{
	char	ch,str[256];
	char	tmplt[128];
	uint	t=strlen(templt),c=0;

	sys_status&=~SS_ABORT;
	SAFECOPY(tmplt, templt);
	strupr(tmplt);
	if(useron.misc&ANSI) {
		if(mode&K_LINE) {
			if(useron.misc&COLOR)
				attr(cfg.color[clr_inputline]);
			else
				attr(BLACK|BG_LIGHTGRAY); }
		while(c<t) {
			if(tmplt[c]=='N' || tmplt[c]=='A' || tmplt[c]=='!')
				outchar(SP);
			else
				outchar(tmplt[c]);
			c++; }
		bprintf("\x1b[%dD",t); }
	c=0;
	if(mode&K_EDIT) {
		strcpy(str,strout);
		bputs(str);
		c=strlen(str); }
	while((ch=getkey(mode))!=CR && online && !(sys_status&SS_ABORT)) {
		if(ch==BS || ch==DEL) {
			if(!c)
				continue;
			for(ch=1,c--;c;c--,ch++)
				if(tmplt[c]=='N' || tmplt[c]=='A' || tmplt[c]=='!')
					break;
			if(useron.misc&ANSI)
				bprintf("\x1b[%dD",ch);
			else while(ch--)
				outchar(BS);
			bputs(" \b");
			continue; }
		if(ch==CTRL_X) {
			for(;c;c--) {
				outchar(BS);
				if(tmplt[c-1]=='N' || tmplt[c-1]=='A' || tmplt[c-1]=='!')
					bputs(" \b"); 
			}
		}
		else if(c<t) {
			if(tmplt[c]=='N' && !isdigit(ch))
				continue;
			if(tmplt[c]=='A' && !isalpha(ch))
				continue;
			outchar(ch);
			str[c++]=ch;
			while(c<t && tmplt[c]!='N' && tmplt[c]!='A' && tmplt[c]!='!'){
				str[c]=tmplt[c];
				outchar(tmplt[c++]); } } }
	str[c]=0;
	attr(LIGHTGRAY);
	CRLF;
	if(!(sys_status&SS_ABORT))
		strcpy(strout,str);
	return(c);
}

/*****************************************************************************/
/* Accepts a user's input to change a new-scan time pointer                  */
/* Returns 0 if input was aborted or invalid, 1 if complete					 */
/*****************************************************************************/
bool sbbs_t::inputnstime(time_t *dt)
{
	int hour;
	struct tm tm;
	bool pm=false;
	char str[256];

	bputs(text[NScanDate]);
	bputs(timestr(dt));
	CRLF;
	if(localtime_r(dt,&tm)==NULL) {
		errormsg(WHERE,ERR_CHK,"time ptr",0);
		return(FALSE);
	}

	bputs(text[NScanYear]);
	ultoa(tm.tm_year+1900,str,10);
	if(!getstr(str,4,K_EDIT|K_AUTODEL|K_NUMBER|K_NOCRLF) || sys_status&SS_ABORT) {
		CRLF;
		return(false); }
	tm.tm_year=atoi(str);
	if(tm.tm_year<1970) {		/* unix time is seconds since 1/1/1970 */
		CRLF;
		return(false); }
	tm.tm_year-=1900;	/* tm_year is years since 1900 */

	bputs(text[NScanMonth]);
	ultoa(tm.tm_mon+1,str,10);
	if(!getstr(str,2,K_EDIT|K_AUTODEL|K_NUMBER|K_NOCRLF) || sys_status&SS_ABORT) {
		CRLF;
		return(false); }
	tm.tm_mon=atoi(str);
	if(tm.tm_mon<1 || tm.tm_mon>12) {
		CRLF;
		return(false); }
	tm.tm_mon--;		/* tm_mon is zero-based */

	bputs(text[NScanDay]);
	ultoa(tm.tm_mday,str,10);
	if(!getstr(str,2,K_EDIT|K_AUTODEL|K_NUMBER|K_NOCRLF) || sys_status&SS_ABORT) {
		CRLF;
		return(false); }
	tm.tm_mday=atoi(str);
	if(tm.tm_mday<1 || tm.tm_mday>31) {
		CRLF;
		return(false); }
	bputs(text[NScanHour]);
	if(cfg.sys_misc&SM_MILITARY)
		hour=tm.tm_hour;
	else {
		if(tm.tm_hour==0) {	/* 12 midnite */
			pm=false;
			hour=12; }
		else if(tm.tm_hour>12) {
			hour=tm.tm_hour-12;
			pm=true; }
		else {
			hour=tm.tm_hour;
			pm=false; } }
	ultoa(hour,str,10);
	if(!getstr(str,2,K_EDIT|K_AUTODEL|K_NUMBER|K_NOCRLF) || sys_status&SS_ABORT) {
		CRLF;
		return(false); }
	tm.tm_hour=atoi(str);
	if(tm.tm_hour>24) {
		CRLF;
		return(false); }

	bputs(text[NScanMinute]);
	ultoa(tm.tm_min,str,10);
	if(!getstr(str,2,K_EDIT|K_AUTODEL|K_NUMBER|K_NOCRLF) || sys_status&SS_ABORT) {
		CRLF;
		return(false); }

	tm.tm_min=atoi(str);
	if(tm.tm_min>59) {
		CRLF;
		return(false); }
	tm.tm_sec=0;
	if(!(cfg.sys_misc&SM_MILITARY) && tm.tm_hour && tm.tm_hour<13) {
		if(pm && yesno(text[NScanPmQ])) {
				if(tm.tm_hour<12)
					tm.tm_hour+=12; }
		else if(!pm && !yesno(text[NScanAmQ])) {
				if(tm.tm_hour<12)
					tm.tm_hour+=12; }
		else if(tm.tm_hour==12)
			tm.tm_hour=0; }
	else {
		CRLF; }
	*dt=mktime(&tm);
	return(true);
}

/*****************************************************************************/
/* Checks a password for uniqueness and validity                              */
/*****************************************************************************/
bool sbbs_t::chkpass(char *pass, user_t* user, bool unique)
{
	char c,d,first[128],last[128],sysop[41],sysname[41],*p;
	char alias[LEN_ALIAS+1], name[LEN_NAME+1], handle[LEN_HANDLE+1];

	if(strlen(pass)<4) {
		bputs(text[PasswordTooShort]);
		return(0); }
	if(!strcmp(pass,user->pass)) {
		bputs(text[PasswordNotChanged]);
		return(0); }
	d=strlen(pass);
	for(c=1;c<d;c++)
		if(pass[c]!=pass[c-1])
			break;
	if(c==d) {
		bputs(text[PasswordInvalid]);
		return(0); }
	for(c=0;c<3;c++)	/* check for 1234 and ABCD */
		if(pass[c]!=pass[c+1]+1)
			break;
	if(c==3) {
		bputs(text[PasswordObvious]);
		return(0); }
	for(c=0;c<3;c++)	/* check for 4321 and ZYXW */
		if(pass[c]!=pass[c+1]-1)
			break;
	if(c==3) {
		bputs(text[PasswordObvious]);
		return(0); }
	strcpy(name,user->name);
	strupr(name);
	strcpy(alias,user->alias);
	strupr(alias);
	strcpy(first,alias);
	p=strchr(first,SP);
	if(p) {
		*p=0;
		strcpy(last,p+1); }
	else
		last[0]=0;
	strcpy(handle,user->handle);
	strupr(handle);
	strcpy(sysop,cfg.sys_op);
	strupr(sysop);
	strcpy(sysname,cfg.sys_name);
	strupr(sysname);
	if((unique && user->pass[0]
			&& (strstr(pass,user->pass) || strstr(user->pass,pass)))
		|| (name[0]
			&& (strstr(pass,name) || strstr(name,pass)))
		|| strstr(pass,alias) || strstr(alias,pass)
		|| strstr(pass,first) || strstr(first,pass)
		|| (last[0]
			&& (strstr(pass,last) || strstr(last,pass)))
		|| strstr(pass,handle) || strstr(handle,pass)
		|| (user->zipcode[0]
			&& (strstr(pass,user->zipcode) || strstr(user->zipcode,pass)))
		|| (sysname[0]
			&& (strstr(pass,sysname) || strstr(sysname,pass)))
		|| (sysop[0]
			&& (strstr(pass,sysop) || strstr(sysop,pass)))
		|| (cfg.sys_id[0]
			&& (strstr(pass,cfg.sys_id) || strstr(cfg.sys_id,pass)))
		|| (cfg.node_phone[0] && strstr(pass,cfg.node_phone))
		|| (user->phone[0] && strstr(user->phone,pass))
		|| !strncmp(pass,"QWER",3)
		|| !strncmp(pass,"ASDF",3)
		|| !strncmp(pass,"!@#$",3)
		)
		{
		bputs(text[PasswordObvious]);
		return(0); }
	return(1);
}

/****************************************************************************/
/* Displays information about sub-board subnum								*/
/****************************************************************************/
void sbbs_t::subinfo(uint subnum)
{
	char str[256];

	bputs(text[SubInfoHdr]);
	bprintf(text[SubInfoLongName],cfg.sub[subnum]->lname);
	bprintf(text[SubInfoShortName],cfg.sub[subnum]->sname);
	bprintf(text[SubInfoQWKName],cfg.sub[subnum]->qwkname);
	bprintf(text[SubInfoMaxMsgs],cfg.sub[subnum]->maxmsgs);
	if(cfg.sub[subnum]->misc&SUB_QNET)
		bprintf(text[SubInfoTagLine],cfg.sub[subnum]->tagline);
	if(cfg.sub[subnum]->misc&SUB_FIDO)
		bprintf(text[SubInfoFidoNet]
			,cfg.sub[subnum]->origline
			,faddrtoa(&cfg.sub[subnum]->faddr,str));
	sprintf(str,"%s%s.msg",cfg.sub[subnum]->data_dir,cfg.sub[subnum]->code);
	if(fexist(str) && yesno(text[SubInfoViewFileQ]))
		printfile(str,0);
}

/****************************************************************************/
/* Displays information about transfer directory dirnum 					*/
/****************************************************************************/
void sbbs_t::dirinfo(uint dirnum)
{
	char str[256];

	bputs(text[DirInfoHdr]);
	bprintf(text[DirInfoLongName],cfg.dir[dirnum]->lname);
	bprintf(text[DirInfoShortName],cfg.dir[dirnum]->sname);
	if(cfg.dir[dirnum]->exts[0])
		bprintf(text[DirInfoAllowedExts],cfg.dir[dirnum]->exts);
	bprintf(text[DirInfoMaxFiles],cfg.dir[dirnum]->maxfiles);
	sprintf(str,"%s%s.msg",cfg.dir[dirnum]->data_dir,cfg.dir[dirnum]->code);
	if(fexist(str) && yesno(text[DirInfoViewFileQ]))
		printfile(str,0);
}

/****************************************************************************/
/* Searches the file <name>.can in the TEXT directory for matches			*/
/* Returns TRUE if found in list, FALSE if not.								*/
/* Displays bad<name>.can in text directory if found.						*/
/****************************************************************************/
bool sbbs_t::trashcan(char *insearchof, char *name)
{
	char str[MAX_PATH+1];
	bool result;

	result=::trashcan(&cfg, insearchof, name)
		? true:false; // This is a dumb bool conversion to make BC++ happy
	if(result) {
		sprintf(str,"%sbad%s.msg",cfg.text_dir,name);
		if(fexistcase(str)) {
			printfile(str,0);
			mswait(500); // give time for tx buffer to clear before disconnect
		}
	}
	return(result);
}

char* sbbs_t::timestr(time_t *intime)
{
	return(::timestr(&cfg,intime,timestr_output));
}

void sbbs_t::sys_info()
{
	char	tmp[128];
	uint	i;
	stats_t stats;

	bputs(text[SiHdr]);
	getstats(&cfg,0,&stats);
	bprintf(text[SiSysName],cfg.sys_name);
	bprintf(text[SiSysID],cfg.sys_id);	/* QWK ID */
	for(i=0;i<cfg.total_faddrs;i++)
		bprintf(text[SiSysFaddr],faddrtoa(&cfg.faddr[i],tmp));
	if(cfg.sys_psname[0])				/* PostLink/PCRelay */
		bprintf(text[SiSysPsite],cfg.sys_psname,cfg.sys_psnum);
	bprintf(text[SiSysLocation],cfg.sys_location);
	bprintf(text[SiSysop],cfg.sys_op);
	bprintf(text[SiSysNodes],cfg.sys_nodes);
//	bprintf(text[SiNodeNumberName],cfg.node_num,cfg.node_name);
	bprintf(text[SiNodePhone],cfg.node_phone);
	bprintf(text[SiTotalLogons],ultoac(stats.logons,tmp));
	bprintf(text[SiLogonsToday],ultoac(stats.ltoday,tmp));
	bprintf(text[SiTotalTime],ultoac(stats.timeon,tmp));
	bprintf(text[SiTimeToday],ultoac(stats.ttoday,tmp));
	ver();
	if(yesno(text[ViewSysInfoFileQ])) {
		CLS;
		sprintf(tmp,"%ssystem.msg", cfg.text_dir);
		printfile(tmp,0); 
	}
	if(yesno(text[ViewLogonMsgQ])) {
		CLS;
		menu("logon"); 
	}
}

void sbbs_t::user_info()
{
	uint	i;
	char	str[128];
	char	tmp[128];
	char	tmp2[128];
	struct	tm tm;

	bprintf(text[UserStats],useron.alias,useron.number);

	if(localtime_r(&useron.laston,&tm)!=NULL)
		bprintf(text[UserDates]
			,unixtodstr(&cfg,useron.firston,str)
			,unixtodstr(&cfg,useron.expire,tmp)
			,unixtodstr(&cfg,useron.laston,tmp2)
			,tm.tm_hour,tm.tm_min);

	bprintf(text[UserTimes]
		,useron.timeon,useron.ttoday
		,cfg.level_timeperday[useron.level]
		,useron.tlast
		,cfg.level_timepercall[useron.level]
		,useron.textra);
	if(useron.posts)
		i=useron.logons/useron.posts;
	else
		i=0;
	bprintf(text[UserLogons]
		,useron.logons,useron.ltoday
		,cfg.level_callsperday[useron.level],useron.posts
		,i ? 100/i : useron.posts>useron.logons ? 100 : 0
		,useron.ptoday);
	bprintf(text[UserEmails]
		,useron.emails,useron.fbacks
		,getmail(&cfg,useron.number,0),useron.etoday);
	CRLF;
	bprintf(text[UserUploads]
		,ultoac(useron.ulb,tmp),useron.uls);
	bprintf(text[UserDownloads]
		,ultoac(useron.dlb,tmp),useron.dls,nulstr);
	bprintf(text[UserCredits],ultoac(useron.cdt,tmp)
		,ultoac(useron.freecdt,tmp2)
		,ultoac(cfg.level_freecdtperday[useron.level],str));
	bprintf(text[UserMinutes],ultoac(useron.min,tmp));
}

void sbbs_t::xfer_policy()
{
	char	str[MAX_PATH+1];

	if(!usrlibs) return;
	sprintf(str,"%smenu/tpolicy.*", cfg.text_dir);
	if(fexist(str))
		menu("tpolicy");
	else {
		bprintf(text[TransferPolicyHdr],cfg.sys_name);
		bprintf(text[TpUpload]
			,cfg.dir[usrdir[curlib][curdir[curlib]]]->up_pct);
		bprintf(text[TpDownload]
			,cfg.dir[usrdir[curlib][curdir[curlib]]]->dn_pct);
		}
}

void sbbs_t::node_stats(uint node_num)
{
	char	tmp[128];
	stats_t	stats;

	if(node_num>cfg.sys_nodes) {
		bputs(text[InvalidNode]);
		return; 
	}
	if(!node_num) node_num=cfg.node_num;
	bprintf(text[NodeStatsHdr],node_num);
	getstats(&cfg,node_num,&stats);
	bprintf(text[StatsTotalLogons],ultoac(stats.logons,tmp));
	bprintf(text[StatsLogonsToday],ultoac(stats.ltoday,tmp));
	bprintf(text[StatsTotalTime],ultoac(stats.timeon,tmp));
	bprintf(text[StatsTimeToday],ultoac(stats.ttoday,tmp));
	bprintf(text[StatsUploadsToday],ultoac(stats.ulb,tmp)
		,stats.uls);
	bprintf(text[StatsDownloadsToday],ultoac(stats.dlb,tmp)
		,stats.dls);
	bprintf(text[StatsPostsToday],ultoac(stats.ptoday,tmp));
	bprintf(text[StatsEmailsToday],ultoac(stats.etoday,tmp));
	bprintf(text[StatsFeedbacksToday],ultoac(stats.ftoday,tmp));
}

void sbbs_t::sys_stats(void)
{
	char	tmp[128];
	stats_t stats;

	bputs(text[SystemStatsHdr]);
	getstats(&cfg,0,&stats);
	bprintf(text[StatsTotalLogons],ultoac(stats.logons,tmp));
	bprintf(text[StatsLogonsToday],ultoac(stats.ltoday,tmp));
	bprintf(text[StatsTotalTime],ultoac(stats.timeon,tmp));
	bprintf(text[StatsTimeToday],ultoac(stats.ttoday,tmp));
	bprintf(text[StatsUploadsToday],ultoac(stats.ulb,tmp)
		,stats.uls);
	bprintf(text[StatsDownloadsToday],ultoac(stats.dlb,tmp)
		,stats.dls);
	bprintf(text[StatsPostsToday],ultoac(stats.ptoday,tmp));
	bprintf(text[StatsEmailsToday],ultoac(stats.etoday,tmp));
	bprintf(text[StatsFeedbacksToday],ultoac(stats.ftoday,tmp));
}

void sbbs_t::logonlist(void)
{
	char	str[MAX_PATH+1];

	sprintf(str,"%slogon.lst", cfg.data_dir);
	if(flength(str)<1) {
		bputs("\r\n\r\n");
		bputs(text[NoOneHasLoggedOnToday]); 
	} else {
		bputs(text[CallersToday]);
		printfile(str,P_NOATCODES|P_OPENCLOSE);
		CRLF; 
	}
}

extern SOCKET	spy_socket[];
extern RingBuf* node_inbuf[];

bool sbbs_t::spy(uint i /* node_num */)
{
	char	ch;
	char	ansi_seq[32];
	int		ansi_len;
	int		in;

	if(!i || i>MAX_NODES) {
		bprintf("Invalid node number: %d\r\n",i);
		return(false);
	}
	if(i==cfg.node_num) {
		bprintf("Can't spy on yourself.\r\n");
		return(false);
	}
	if(spy_socket[i-1]!=INVALID_SOCKET) {
		bprintf("Node %d already being spied (%lx)\r\n",i,spy_socket[i-1]);
		return(false);
	}
	bprintf("*** Synchronet Remote Spy on Node %d: Ctrl-C to Abort ***"
		"\r\n\r\n",i);
	spy_socket[i-1]=client_socket;
	ansi_len=0;
	while(online 
		&& client_socket!=INVALID_SOCKET 
		&& spy_socket[i-1]!=INVALID_SOCKET 
		&& !msgabort()) {
		in=incom();
		if(in==NOINP) {
			gettimeleft();
			YIELD();
			continue;
		}
		ch=in;
		if(ch==ESC) {
			if(!ansi_len) {
				ansi_seq[ansi_len++]=ch;
				continue;
			}
			ansi_len=0;
		}
		if(ansi_len && ansi_len<(int)sizeof(ansi_seq)-2) {
			if(ansi_len==1) {
				if(ch=='[') {
					ansi_seq[ansi_len++]=ch;
					continue;
				}
				ansi_len=0;
			}
			if(ch=='R') { /* through-away cursor position report */
				ansi_len=0;
				continue;
			}
			ansi_seq[ansi_len++]=ch;
			if(isalpha(ch)) {
				RingBufWrite(node_inbuf[i-1],(uchar*)ansi_seq,ansi_len);
				ansi_len=0;
			}
			continue;
		}
		if(ch==CTRL_P) {		/* Private node-node comm */
			lncntr=0;						/* defeat pause */
			spy_socket[i-1]=INVALID_SOCKET;	/* disable spy output */
			nodesync(); 					/* read waiting messages */
			nodemsg();						/* send a message */
			spy_socket[i-1]=client_socket;	/* enable spy output */
			continue; 
		}
		if(ch==CTRL_U) {		/* Users online */
			lncntr=0;			/* defeat pause */
			whos_online(true); 	/* list users */
			continue;
		}
		if(node_inbuf[i-1]!=NULL) 
			RingBufWrite(node_inbuf[i-1],(uchar*)&ch,1);
	}
	spy_socket[i-1]=INVALID_SOCKET;
	return(true);
}

void sbbs_t::time_bank(void)
{
	char	str[128];
	char	tmp[128];
	char	tmp2[128];
	int		s;

	if(cfg.sys_misc&SM_TIMEBANK) {	/* Allow users to deposit free time */
		s=(cfg.level_timeperday[useron.level]-useron.ttoday)+useron.textra;
		if(s<0) s=0;
		if(s>cfg.level_timepercall[useron.level])
			s=cfg.level_timepercall[useron.level];
		s-=(now-starttime)/60;
		if(s<0) s=0;
		bprintf(text[FreeMinLeft],s);
		bprintf(text[UserMinutes],ultoac(useron.min,tmp));
		if(cfg.max_minutes && useron.min>=cfg.max_minutes) {
			bputs(text[YouHaveTooManyMinutes]);
			return; 
		}
		if(cfg.max_minutes)
			while(s>0 && s+useron.min>cfg.max_minutes) s--;
		bprintf(text[FreeMinToDeposit],s);
		s=getnum(s);
		if(s>0) {
			logline("  ","Minute Bank Deposit");
			useron.min=adjustuserrec(&cfg,useron.number,U_MIN,10,s);
			useron.ttoday=(ushort)adjustuserrec(&cfg,useron.number,U_TTODAY,10,s);
			sprintf(str,"Minute Adjustment: %u",s*cfg.cdt_min_value);
			logline("*+",str); 
		} 
	}

	if(!(cfg.sys_misc&SM_NOCDTCVT)) {
		bprintf(text[ConversionRate],cfg.cdt_min_value);
		bprintf(text[UserCredits]
			,ultoac(useron.cdt,tmp)
			,ultoac(useron.freecdt,tmp2)
			,ultoac(cfg.level_freecdtperday[useron.level],str));
		bprintf(text[UserMinutes],ultoac(useron.min,tmp));
		if(useron.cdt/102400L<1L) {
			bprintf(text[YouOnlyHaveNCredits],ultoac(useron.cdt,tmp));
			return; 
		}
		if(cfg.max_minutes && useron.min>=cfg.max_minutes) {
			bputs(text[YouHaveTooManyMinutes]);
			return; 
		}
		s=useron.cdt/102400L;
		if(cfg.max_minutes)
			while(s>0 && (s*cfg.cdt_min_value)+useron.min>cfg.max_minutes) s--;
		bprintf(text[CreditsToMin],s);
		s=getnum(s);
		if(s>0) {
			logline("  ","Credit to Minute Conversion");
			useron.cdt=adjustuserrec(&cfg,useron.number,U_CDT,10,-(s*102400L));
			useron.min=adjustuserrec(&cfg,useron.number,U_MIN,10,s*cfg.cdt_min_value);
			sprintf(str,"Credit Adjustment: %ld",-(s*102400L));
			logline("$-",str);
			sprintf(str,"Minute Adjustment: %u",s*cfg.cdt_min_value);
			logline("*+",str); 
		} 
	}
}

void sbbs_t::change_user(void)
{
	uint	i;
	char	str[256];
	char	tmp[128];

	if(!chksyspass())
		return;
	bputs(text[ChUserPrompt]);
	if(!getstr(str,LEN_ALIAS,K_UPPER))
		return;
	if((i=finduser(str))==0)
		return;
	if(online==ON_REMOTE) {
		getuserrec(&cfg,i,U_LEVEL,2,str);
		if(atoi(str)>logon_ml) {
			getuserrec(&cfg,i,U_PASS,8,tmp);
			bputs(text[ChUserPwPrompt]);
			console|=CON_R_ECHOX;
			if(!(cfg.sys_misc&SM_ECHO_PW))
				console|=CON_L_ECHOX;
			getstr(str,8,K_UPPER);
			console&=~(CON_R_ECHOX|CON_L_ECHOX);
			if(strcmp(str,tmp))
				return;
		}
	}
	putmsgptrs();
	putuserrec(&cfg,useron.number,U_CURSUB,0
		,cfg.sub[usrsub[curgrp][cursub[curgrp]]]->code);
	putuserrec(&cfg,useron.number,U_CURDIR,0
		,cfg.dir[usrdir[curlib][curdir[curlib]]]->code);
	useron.number=i;
	getuserdat(&cfg,&useron);
	getnodedat(cfg.node_num,&thisnode,1);
	thisnode.useron=useron.number;
	putnodedat(cfg.node_num,&thisnode);
	getmsgptrs();
	if(REALSYSOP) sys_status&=~SS_TMPSYSOP;
	else sys_status|=SS_TMPSYSOP;
	sprintf(str,"Changed into %s #%u",useron.alias,useron.number);
	logline("S+",str);
}
