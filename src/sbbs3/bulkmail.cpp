/* bulkmail.cpp */

/* Synchronet bulk e-mail functions */

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

void sbbs_t::bulkmail(uchar *ar)
{
	char	str[256],str2[256],msgpath[256],title[LEN_TITLE+1]
			,buf[SDT_BLOCK_LEN],found=0;
	char 	tmp[512];
	ushort	xlat=XLAT_NONE,msgattr=0;
	int 	i,j,x,file;
	long	msgs=0;
	ulong	length,offset;
	FILE	*instream;
	user_t	user;
	smbmsg_t msg;

	memset(&msg,0,sizeof(smbmsg_t));

	title[0]=0;
	action=NODE_SMAL;
	nodesync();

	if(cfg.sys_misc&SM_ANON_EM && (SYSOP || useron.exempt&FLAG('A'))
		&& !noyes(text[AnonymousQ]))
		msgattr|=MSG_ANONYMOUS;

	sprintf(msgpath,"%sINPUT.MSG",cfg.node_dir);
	sprintf(str2,"Bulk Mailing");
	if(!writemsg(msgpath,nulstr,title,WM_EMAIL,0,str2)) {
		bputs(text[Aborted]);
		return; }

	bputs(text[WritingIndx]);
	CRLF;

	if((i=smb_stack(&smb,SMB_STACK_PUSH))!=0) {
		errormsg(WHERE,ERR_OPEN,"MAIL",i);
		return; }
	sprintf(smb.file,"%smail",cfg.data_dir);
	smb.retry_time=cfg.smb_retry_time;
	smb.subnum=INVALID_SUB;
	if((i=smb_open(&smb))!=0) {
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return; }

	if(smb_fgetlength(smb.shd_fp)<1) {	 /* Create it if it doesn't exist */
		smb.status.max_crcs=cfg.mail_maxcrcs;
		smb.status.max_msgs=MAX_SYSMAIL;
		smb.status.max_age=cfg.mail_maxage;
		smb.status.attr=SMB_EMAIL;
		if((i=smb_create(&smb))!=0) {
			smb_close(&smb);
			smb_stack(&smb,SMB_STACK_POP);
			errormsg(WHERE,ERR_CREATE,smb.file,i,smb.last_error);
			return; } }

	length=flength(msgpath)+2;	 /* +2 for translation string */

	if(length&0xfff00000UL) {
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_LEN,smb.file,length);
		return; }

	if((i=smb_open_da(&smb))!=0) {
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return; }
	if(cfg.sys_misc&SM_FASTMAIL)
		offset=smb_fallocdat(&smb,length,1);
	else
		offset=smb_allocdat(&smb,length,1);
	smb_close_da(&smb);

	if((file=open(msgpath,O_RDONLY|O_BINARY))==-1
		|| (instream=fdopen(file,"rb"))==NULL) {
		smb_freemsgdat(&smb,offset,length,1);
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_OPEN,msgpath,O_RDONLY|O_BINARY);
		return; }

	setvbuf(instream,NULL,_IOFBF,2*1024);
	smb_fseek(smb.sdt_fp,offset,SEEK_SET);
	xlat=XLAT_NONE;
	smb_fwrite(&xlat,2,smb.sdt_fp);
	x=SDT_BLOCK_LEN-2;				/* Don't read/write more than 255 */
	while(!feof(instream)) {
		memset(buf,0,x);
		j=fread(buf,1,x,instream);
		if(j<1)
			break;
		if(j>1 && (j!=x || feof(instream)) && buf[j-1]==LF && buf[j-2]==CR)
			buf[j-1]=buf[j-2]=0;
		smb_fwrite(buf,j,smb.sdt_fp);
		x=SDT_BLOCK_LEN; }
	smb_fflush(smb.sdt_fp);
	fclose(instream);

	j=lastuser(&cfg);
	x=0;

	if(*ar)
		for(i=1;i<=j;i++) {
			user.number=i;
			getuserdat(&cfg, &user);
			if(user.misc&(DELETED|INACTIVE))
				continue;
			if(chk_ar(ar,&user)) {
				if(found)
					smb_freemsgmem(&msg);
				x=bulkmailhdr(i,&msg,msgattr,offset,length,title);
				if(x)
					break;
				msgs++;
				found=1; } }
	else
		while(1) {
			bputs(text[EnterAfterLastDestUser]);
			if(!getstr(str,LEN_ALIAS,K_UPRLWR))
				break;
			if((i=finduser(str))!=0) {
				if(found)
					smb_freemsgmem(&msg);
				x=bulkmailhdr(i,&msg,msgattr,offset,length,title);
				if(x)
					break;
				msgs++; }
			found=1; }

	if((i=smb_open_da(&smb))!=0) {
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return; }
	if(!msgs)
		smb_freemsgdat(&smb,offset,length,1);
	else if(msgs>1)
		smb_incdat(&smb,offset,length,msgs-1);
	smb_close_da(&smb);

	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);

	smb_freemsgmem(&msg);
	if(x) {
		smb_freemsgdat(&smb,offset,length,1);
		errormsg(WHERE,ERR_WRITE,smb.file,x);
		return; }

	putuserrec(&cfg,useron.number,U_EMAILS,5,ultoa(useron.emails,tmp,10));
	putuserrec(&cfg,useron.number,U_ETODAY,5,ultoa(useron.etoday,tmp,10));
}


int sbbs_t::bulkmailhdr(uint usernum, smbmsg_t *msg, ushort msgattr, ulong offset
    , ulong length, char *title)
{
    char	str[256];
	char 	tmp[512];
    int		i,j;
    node_t	node;

	memset(msg,0,sizeof(smbmsg_t));
	msg->hdr.version=smb_ver();
	msg->hdr.attr=msg->idx.attr=msgattr;
	msg->hdr.when_written.time=msg->hdr.when_imported.time=time(NULL);
	msg->hdr.when_written.zone=msg->hdr.when_imported.zone=cfg.sys_timezone;
	msg->hdr.offset=msg->idx.offset=offset;

	username(&cfg, usernum,str);
	smb_hfield(msg,RECIPIENT,strlen(str),str);
	strlwr(str);

	sprintf(str,"%u",usernum);
	smb_hfield(msg,RECIPIENTEXT,strlen(str),str);
	msg->idx.to=usernum;

	strcpy(str,useron.alias);
	smb_hfield(msg,SENDER,strlen(str),str);
	strlwr(str);

	sprintf(str,"%u",useron.number);
	smb_hfield(msg,SENDEREXT,strlen(str),str);
	msg->idx.from=useron.number;

	strcpy(str,title);
	smb_hfield(msg,SUBJECT,strlen(str),str);
	msg->idx.subj=subject_crc(str);

	smb_dfield(msg,TEXT_BODY,length);

	j=smb_addmsghdr(&smb,msg,SMB_SELFPACK);
	if(j)
		return(j);

	// smb_incdat(&smb,offset,length,1); Remove 04/15/96
	lncntr=0;
	bprintf("Bulk-mailed %s #%d\r\n",username(&cfg, usernum,tmp),usernum);
	sprintf(str,"%s bulk-mailed %s #%d"
		,useron.alias,username(&cfg, usernum,tmp),usernum);
	logline("E+",str);
	useron.emails++;
	logon_emails++;
	useron.etoday++;
	for(i=1;i<=cfg.sys_nodes;i++) { /* Tell user, if online */
		getnodedat(i,&node,0);
		if(node.useron==usernum && !(node.misc&NODE_POFF)
			&& (node.status==NODE_INUSE || node.status==NODE_QUIET)) {
			sprintf(str,text[EmailNodeMsg],cfg.node_num,useron.alias);
			putnmsg(i,str);
			break; } }
	if(i>cfg.sys_nodes) {   /* User wasn't online, so leave short msg */
		sprintf(str,text[UserSentYouMail],useron.alias);
		putsmsg(&cfg,usernum,str); }
	return(0);
}

