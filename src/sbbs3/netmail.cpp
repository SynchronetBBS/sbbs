/* netmail.cpp */

/* Synchronet network mail-related functions */

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
#include "qwk.h"

/****************************************************************************/
/****************************************************************************/
bool sbbs_t::inetmail(char *into, char *subj, long mode)
{
	char	str[256],str2[256],msgpath[256],title[256],name[256],ch
			,buf[SDT_BLOCK_LEN],*p,addr[256];
	char 	tmp[512];
	char	your_addr[128];
	ushort	xlat=XLAT_NONE,net=NET_INTERNET;
	int 	i,j,x,file;
	long	l;
	ulong	length,offset;
	FILE	*instream;
	smbmsg_t msg;

	if(useron.etoday>=cfg.level_emailperday[useron.level] && !SYSOP) {
		bputs(text[TooManyEmailsToday]);
		return(false); 
	}

	strcpy(name,into);
	strcpy(addr,into);
	strcpy(title,subj);

	if((!SYSOP && !(cfg.inetmail_misc&NMAIL_ALLOW)) || useron.rest&FLAG('M')) {
		bputs(text[NoNetMailAllowed]);
		return(false); }

	if(cfg.inetmail_cost && !(useron.exempt&FLAG('S'))) {
		if(useron.cdt+useron.freecdt<cfg.inetmail_cost) {
			bputs(text[NotEnoughCredits]);
			return(false); }
		sprintf(str,text[NetMailCostContinueQ],cfg.inetmail_cost);
		if(noyes(str))
			return(false); }

	/* Get destination user name */
	p=strrchr(name,'@');
	if(!p)
		p=strrchr(name,'!');
	if(p) {
		*p=0;
		truncsp(name); }

	/* Get this user's internet mailing address */
	usermailaddr(&cfg,your_addr
		,cfg.inetmail_misc&NMAIL_ALIAS ? useron.alias : useron.name);

	bprintf(text[InternetMailing],addr,your_addr);
	action=NODE_SMAL;
	nodesync();

	sprintf(msgpath,"%snetmail.msg",cfg.node_dir);
	if(!writemsg(msgpath,nulstr,title,mode,INVALID_SUB,into)) {
		bputs(text[Aborted]);
		return(false); }

	if(mode&WM_FILE) {
		sprintf(str2,"%sfile/%04u.out",cfg.data_dir,useron.number);
		MKDIR(str2);
		sprintf(str2,"%sfile/%04u.out/%s",cfg.data_dir,useron.number,title);
		if(fexistcase(str2)) {
			bputs(text[FileAlreadyThere]);
			remove(msgpath);
			return(false); }
		if(online==ON_LOCAL) {		/* Local upload */
			bputs(text[EnterPath]);
			if(!getstr(str,60,K_LINE|K_UPPER)) {
				bputs(text[Aborted]);
				remove(msgpath);
				return(false); }
			backslash(str);
			strcat(str,title);
			mv(str,str2,1); }
		else { /* Remote */
			menu("ulprot");
			mnemonics(text[ProtocolOrQuit]);
			strcpy(str,"Q");
			for(x=0;x<cfg.total_prots;x++)
				if(cfg.prot[x]->ulcmd[0] && chk_ar(cfg.prot[x]->ar,&useron)) {
					sprintf(tmp,"%c",cfg.prot[x]->mnemonic);
					strcat(str,tmp); }
			ch=(char)getkeys(str,0);
			if(ch=='Q' || sys_status&SS_ABORT) {
				bputs(text[Aborted]);
				remove(msgpath);
				return(false); }
			for(x=0;x<cfg.total_prots;x++)
				if(cfg.prot[x]->ulcmd[0] && cfg.prot[x]->mnemonic==ch
					&& chk_ar(cfg.prot[x]->ar,&useron))
					break;
			if(x<cfg.total_prots)	/* This should be always */
				protocol(cmdstr(cfg.prot[x]->ulcmd,str2,nulstr,NULL),true); 
		}
		sprintf(tmp,"%s%s",cfg.temp_dir,title);
		if(!fexistcase(str2) && fexistcase(tmp))
			mv(tmp,str2,0);
		l=flength(str2);
		if(l>0)
			bprintf(text[FileNBytesReceived],title,ultoac(l,tmp));
		else {
			bprintf(text[FileNotReceived],title);
			remove(msgpath);
			return(false); } }

	if((i=smb_stack(&smb,SMB_STACK_PUSH))!=0) {
		errormsg(WHERE,ERR_OPEN,"MAIL",i);
		return(false); }
	sprintf(smb.file,"%smail",cfg.data_dir);
	smb.retry_time=cfg.smb_retry_time;
	smb.subnum=INVALID_SUB;
	if((i=smb_open(&smb))!=0) {
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return(false); }

	if(filelength(fileno(smb.shd_fp))<1) {	 /* Create it if it doesn't exist */
		smb.status.max_crcs=cfg.mail_maxcrcs;
		smb.status.max_age=cfg.mail_maxage;
		smb.status.max_msgs=MAX_SYSMAIL;
		smb.status.attr=SMB_EMAIL;
		if((i=smb_create(&smb))!=0) {
			smb_close(&smb);
			smb_stack(&smb,SMB_STACK_POP);
			errormsg(WHERE,ERR_CREATE,smb.file,i);
			return(false); } }

	if((i=smb_locksmbhdr(&smb))!=0) {
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_LOCK,smb.file,i);
		return(false); }

	length=flength(msgpath)+2;	 /* +2 for translation string */

	if(length&0xfff00000UL) {
		smb_unlocksmbhdr(&smb);
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_LEN,msgpath,length);
		return(false); }

	if((i=smb_open_da(&smb))!=0) {
		smb_unlocksmbhdr(&smb);
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return(false); }
	if(cfg.sys_misc&SM_FASTMAIL)
		offset=smb_fallocdat(&smb,length,1);
	else
		offset=smb_allocdat(&smb,length,1);
	smb_close_da(&smb);

	if((file=open(msgpath,O_RDONLY|O_BINARY))==-1
		|| (instream=fdopen(file,"rb"))==NULL) {
		smb_freemsgdat(&smb,offset,length,1);
		smb_unlocksmbhdr(&smb);
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_OPEN,msgpath,O_RDONLY|O_BINARY);
		return(false); }

	setvbuf(instream,NULL,_IOFBF,2*1024);
	fseek(smb.sdt_fp,offset,SEEK_SET);
	xlat=XLAT_NONE;
	fwrite(&xlat,2,1,smb.sdt_fp);
	x=SDT_BLOCK_LEN-2;				/* Don't read/write more than 255 */
	while(!feof(instream)) {
		memset(buf,0,x);
		j=fread(buf,1,x,instream);
		if(j<1)
			break;
		if(j>1 && (j!=x || feof(instream)) && buf[j-1]==LF && buf[j-2]==CR)
			buf[j-1]=buf[j-2]=0;
		fwrite(buf,j,1,smb.sdt_fp);
		x=SDT_BLOCK_LEN; }
	fflush(smb.sdt_fp);
	fclose(instream);

	memset(&msg,0,sizeof(smbmsg_t));
	msg.hdr.version=smb_ver();
	if(mode&WM_FILE)
		msg.hdr.auxattr|=MSG_FILEATTACH;
	msg.hdr.when_written.time=msg.hdr.when_imported.time=time(NULL);
	msg.hdr.when_written.zone=msg.hdr.when_imported.zone=cfg.sys_timezone;

	msg.hdr.offset=offset;

	net=NET_INTERNET;
	smb_hfield(&msg,RECIPIENT,strlen(name),name);
	msg.idx.to=0;	/* Out-bound NetMail set to 0 */
	smb_hfield(&msg,RECIPIENTNETTYPE,sizeof(net),&net);
	smb_hfield(&msg,RECIPIENTNETADDR,strlen(addr),addr);

	strcpy(str,cfg.inetmail_misc&NMAIL_ALIAS ? useron.alias : useron.name);
	smb_hfield(&msg,SENDER,strlen(str),str);

	sprintf(str,"%u",useron.number);
	smb_hfield(&msg,SENDEREXT,strlen(str),str);
	msg.idx.from=useron.number;

	/*
	smb_hfield(&msg,SENDERNETTYPE,sizeof(net),&net);
	smb_hfield(&msg,SENDERNETADDR,strlen(sys_inetaddr),sys_inetaddr);
	*/

	smb_hfield(&msg,SUBJECT,strlen(title),title);
	strcpy(str,title);
	msg.idx.subj=subject_crc(str);

	smb_dfield(&msg,TEXT_BODY,length);

	i=smb_addmsghdr(&smb,&msg,SMB_SELFPACK);	// calls smb_unlocksmbhdr() 
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);

	smb_freemsgmem(&msg);
	if(i) {
		smb_freemsgdat(&smb,offset,length,1);
		errormsg(WHERE,ERR_WRITE,smb.file,i,smb.last_error);
		return(false); }

	if(mode&WM_FILE && online==ON_REMOTE)
		autohangup();

	if(cfg.inetmail_sem[0]) 	 /* update semaphore file */
		if((file=nopen(cmdstr(cfg.inetmail_sem,nulstr,nulstr,NULL)
			,O_WRONLY|O_CREAT|O_TRUNC))!=-1)
			close(file);
	if(!(useron.exempt&FLAG('S')))
		subtract_cdt(&cfg,&useron,cfg.inetmail_cost);

	useron.emails++;
	logon_emails++;
	putuserrec(&cfg,useron.number,U_EMAILS,5,ultoa(useron.emails,tmp,10)); 
	useron.etoday++;
	putuserrec(&cfg,useron.number,U_ETODAY,5,ultoa(useron.etoday,tmp,10));

	sprintf(str,"%s sent Internet Mail to %s (%s)"
		,useron.alias
		,name,addr);
	logline("EN",str);
	return(true);
}

bool sbbs_t::qnetmail(char *into, char *subj, long mode)
{
	char	str[256],msgpath[128],title[128],to[128],fulladdr[128]
			,buf[SDT_BLOCK_LEN],*addr;
	char 	tmp[512];
	ushort	xlat=XLAT_NONE,net=NET_QWK,touser;
	int 	i,j,x,file;
	ulong	length,offset;
	FILE	*instream;
	smbmsg_t msg;

	if(useron.etoday>=cfg.level_emailperday[useron.level] && !SYSOP) {
		bputs(text[TooManyEmailsToday]);
		return(false); 
	}

	strcpy(to,into);
	strcpy(title,subj);

	if(useron.rest&FLAG('M')) {
		bputs(text[NoNetMailAllowed]);
		return(false); }

	addr=strrchr(to,'@');
	if(!addr) {
		bputs(text[InvalidNetMailAddr]);
		return(false); 
	}
	*addr=0;
	addr++;
	strupr(addr);
	truncsp(addr);
	touser=qwk_route(addr,fulladdr);
	if(!fulladdr[0]) {
		bputs(text[InvalidNetMailAddr]);
		return(false); 
	}

	truncsp(to);
	if(!stricmp(to,"SBBS") && !SYSOP) {
		bputs(text[InvalidNetMailAddr]);
		return(false); 
	}
	bprintf(text[NetMailing],to,fulladdr
		,useron.alias,cfg.sys_id);
	action=NODE_SMAL;
	nodesync();

	sprintf(msgpath,"%snetmail.msg",cfg.node_dir);
	if(!writemsg(msgpath,nulstr,title,mode|WM_QWKNET,INVALID_SUB,to)) {
		bputs(text[Aborted]);
		return(false); }

	if((i=smb_stack(&smb,SMB_STACK_PUSH))!=0) {
		errormsg(WHERE,ERR_OPEN,"MAIL",i);
		return(false); }
	sprintf(smb.file,"%smail",cfg.data_dir);
	smb.retry_time=cfg.smb_retry_time;
	smb.subnum=INVALID_SUB;
	if((i=smb_open(&smb))!=0) {
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return(false); }

	if(filelength(fileno(smb.shd_fp))<1) {	 /* Create it if it doesn't exist */
		smb.status.max_crcs=cfg.mail_maxcrcs;
		smb.status.max_msgs=MAX_SYSMAIL;
		smb.status.max_age=cfg.mail_maxage;
		smb.status.attr=SMB_EMAIL;
		if((i=smb_create(&smb))!=0) {
			smb_close(&smb);
			smb_stack(&smb,SMB_STACK_POP);
			errormsg(WHERE,ERR_CREATE,smb.file,i,smb.last_error);
			return(false); } }

	if((i=smb_locksmbhdr(&smb))!=0) {
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_LOCK,smb.file,i,smb.last_error);
		return(false); }

	length=flength(msgpath)+2;	 /* +2 for translation string */

	if(length&0xfff00000UL) {
		smb_unlocksmbhdr(&smb);
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_LEN,msgpath,length);
		return(false); }

	if((i=smb_open_da(&smb))!=0) {
		smb_unlocksmbhdr(&smb);
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return(false); }
	if(cfg.sys_misc&SM_FASTMAIL)
		offset=smb_fallocdat(&smb,length,1);
	else
		offset=smb_allocdat(&smb,length,1);
	smb_close_da(&smb);

	if((file=open(msgpath,O_RDONLY|O_BINARY))==-1
		|| (instream=fdopen(file,"rb"))==NULL) {
		smb_freemsgdat(&smb,offset,length,1);
		smb_unlocksmbhdr(&smb);
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_OPEN,msgpath,O_RDONLY|O_BINARY);
		return(false); }

	setvbuf(instream,NULL,_IOFBF,2*1024);
	fseek(smb.sdt_fp,offset,SEEK_SET);
	xlat=XLAT_NONE;
	fwrite(&xlat,2,1,smb.sdt_fp);
	x=SDT_BLOCK_LEN-2;				/* Don't read/write more than 255 */
	while(!feof(instream)) {
		memset(buf,0,x);
		j=fread(buf,1,x,instream);
		if(j<1)
			break;
		if(j>1 && (j!=x || feof(instream)) && buf[j-1]==LF && buf[j-2]==CR)
			buf[j-1]=buf[j-2]=0;
		fwrite(buf,j,1,smb.sdt_fp);
		x=SDT_BLOCK_LEN; }
	fflush(smb.sdt_fp);
	fclose(instream);

	memset(&msg,0,sizeof(smbmsg_t));
	msg.hdr.version=smb_ver();
	if(mode&WM_FILE)
		msg.hdr.auxattr|=MSG_FILEATTACH;
	msg.hdr.when_written.time=msg.hdr.when_imported.time=time(NULL);
	msg.hdr.when_written.zone=msg.hdr.when_imported.zone=cfg.sys_timezone;

	msg.hdr.offset=offset;

	net=NET_QWK;
	smb_hfield(&msg,RECIPIENT,strlen(to),to);
	msg.idx.to=touser;
	smb_hfield(&msg,RECIPIENTNETTYPE,sizeof(net),&net);
	smb_hfield(&msg,RECIPIENTNETADDR,strlen(fulladdr),fulladdr);

	smb_hfield(&msg,SENDER,strlen(useron.alias),useron.alias);

	sprintf(str,"%u",useron.number);
	smb_hfield(&msg,SENDEREXT,strlen(str),str);
	msg.idx.from=useron.number;

	smb_hfield(&msg,SUBJECT,strlen(title),title);
	msg.idx.subj=subject_crc(title);

	smb_dfield(&msg,TEXT_BODY,length);

	i=smb_addmsghdr(&smb,&msg,SMB_SELFPACK); // calls smb_unlocksmbhdr() 
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);

	smb_freemsgmem(&msg);
	if(i) {
		smb_freemsgdat(&smb,offset,length,1);
		errormsg(WHERE,ERR_WRITE,smb.file,i,smb.last_error);
		return(false); }

	useron.emails++;
	logon_emails++;
	putuserrec(&cfg,useron.number,U_EMAILS,5,ultoa(useron.emails,tmp,10)); 
	useron.etoday++;
	putuserrec(&cfg,useron.number,U_ETODAY,5,ultoa(useron.etoday,tmp,10));

	sprintf(str,"%s sent QWK NetMail to %s (%s)"
		,useron.alias
		,to,fulladdr);
	logline("EN",str);
	return(true);
}
