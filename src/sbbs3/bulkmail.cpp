/* bulkmail.cpp */

/* Synchronet bulk e-mail functions */

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

bool sbbs_t::bulkmail(uchar *ar)
{
	char		str[256],title[LEN_TITLE+1];
	char		msgpath[MAX_PATH+1];
	char*		msgbuf;
	char 		tmp[512];
	int 		i,j,x;
	long		msgs=0;
	long		length;
	FILE*		fp;
	smb_t		smb;
	smbmsg_t	msg;
	user_t		user;

	memset(&msg,0,sizeof(msg));
	
	title[0]=0;
	action=NODE_SMAL;
	nodesync();

	if(cfg.sys_misc&SM_ANON_EM && useron.exempt&FLAG('A')
		&& !noyes(text[AnonymousQ]))
		msg.hdr.attr|=MSG_ANONYMOUS;

	sprintf(msgpath,"%sinput.msg",cfg.node_dir);
	if(!writemsg(msgpath,nulstr,title,WM_EMAIL,INVALID_SUB,"Bulk Mailing")) {
		bputs(text[Aborted]);
		return(false); 
	}

	if((fp=fopen(msgpath,"r"))==NULL) {
		errormsg(WHERE,ERR_OPEN,msgpath,O_RDONLY);
		return(false); 
	}

	if((length=filelength(fileno(fp)))<=0) {
		fclose(fp);
		return(false);
	}

	bputs(text[WritingIndx]);
	CRLF;

	if((msgbuf=(char*)malloc(length+1))==NULL) {
		errormsg(WHERE,ERR_ALLOC,msgpath,length+1);
		return(false);
	}
	length=fread(msgbuf,sizeof(char),length,fp);
	fclose(fp);
	if(length<0) {
		free(msgbuf);
		errormsg(WHERE,ERR_READ,msgpath,length);
		return(false);
	}
	msgbuf[length]=0;	/* ASCIIZ */

	smb_hfield_str(&msg,SENDER,useron.alias);

	sprintf(str,"%u",useron.number);
	smb_hfield_str(&msg,SENDEREXT,str);
	msg.idx.from=useron.number;

	smb_hfield_str(&msg,SUBJECT,title);
	msg.idx.subj=subject_crc(title);

	memset(&smb,0,sizeof(smb));
	smb.subnum=INVALID_SUB;	/* mail database */
	i=savemsg(&cfg, &smb, &msg, msgbuf);
	free(msgbuf);
	if(i!=0) {
		smb_close(&smb);
		smb_freemsgmem(&msg);
		return(false);
	}

	j=lastuser(&cfg);
	x=0;

	if(*ar)
		for(i=1;i<=j;i++) {
			user.number=i;
			if(getuserdat(&cfg, &user)!=0)
				continue;
			if(user.misc&(DELETED|INACTIVE))
				continue;
			if(chk_ar(ar,&user)) {
				if((x=bulkmailhdr(&smb, &msg, i))!=0) {
					errormsg(WHERE,ERR_WRITE,smb.file,x);
					break;
				}
				msgs++;
			} 
		}
	else
		while(online) {
			bputs(text[EnterAfterLastDestUser]);
			if(!getstr(str,LEN_ALIAS,K_UPRLWR))
				break;
			if((i=finduser(str))!=0) {
				if((x=bulkmailhdr(&smb, &msg, i))!=0) {
					errormsg(WHERE,ERR_WRITE,smb.file,x);
					break;
				}
				msgs++; 
			}
		}

	if((i=smb_open_da(&smb))==0) {
		if(!msgs)
			smb_freemsg_dfields(&smb,&msg,SMB_ALL_REFS);
		else if(msgs>1)
			smb_incmsg_dfields(&smb,&msg,(ushort)msgs-1);
		smb_close_da(&smb);
	}

	smb_close(&smb);
	smb_freemsgmem(&msg);

	if(i) {
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return(false);
	}

	putuserrec(&cfg,useron.number,U_EMAILS,5,ultoa(useron.emails,tmp,10));
	putuserrec(&cfg,useron.number,U_ETODAY,5,ultoa(useron.etoday,tmp,10));

	return(true);
}


int sbbs_t::bulkmailhdr(smb_t* smb, smbmsg_t* msg, uint usernum)
{
    char		str[256];
    int			i,j;
	ushort		nettype;
    node_t		node;
	user_t		user;
	smbmsg_t	newmsg;

	user.number=usernum;
	if(getuserdat(&cfg, &user)!=0)
		return(0);

	if((i=smb_copymsgmem(NULL,&newmsg,msg))!=0)
		return(i);

	SAFECOPY(str,user.alias);
	smb_hfield_str(&newmsg,RECIPIENT,str);

	if(cfg.sys_misc&SM_FWDTONET && user.misc&NETMAIL && user.netmail[0]) {
		bprintf(text[UserNetMail],user.netmail);
		nettype=NET_INTERNET;
		smb_hfield(&newmsg,RECIPIENTNETTYPE,sizeof(nettype),&nettype);
		smb_hfield_str(&newmsg,RECIPIENTNETADDR,user.netmail);
	} else {
		sprintf(str,"%u",usernum);
		smb_hfield_str(&newmsg,RECIPIENTEXT,str);
		newmsg.idx.to=usernum;
	}

	j=smb_addmsghdr(smb,&newmsg,SMB_SELFPACK);
	smb_freemsgmem(&newmsg);
	if(j)
		return(j);

	lncntr=0;
	bprintf("Bulk-mailed %s #%d\r\n",user.alias,usernum);
	sprintf(str,"%s bulk-mailed %s #%d"
		,useron.alias,user.alias,usernum);
	logline("E+",str);
	useron.emails++;
	logon_emails++;
	useron.etoday++;
	for(i=1;i<=cfg.sys_nodes;i++) { /* Tell user, if online */
		getnodedat(i,&node,0);
		if(node.useron==usernum && !(node.misc&NODE_POFF)
			&& (node.status==NODE_INUSE || node.status==NODE_QUIET)) {
			sprintf(str,text[EmailNodeMsg],cfg.node_num,useron.alias);
			putnmsg(&cfg,i,str);
			break; 
		} 
	}
	if(i>cfg.sys_nodes) {   /* User wasn't online, so leave short msg */
		sprintf(str,text[UserSentYouMail],useron.alias);
		putsmsg(&cfg,usernum,str); 
	}
	return(0);
}

