/* Synchronet bulk e-mail functions */

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

bool sbbs_t::bulkmail(uchar *ar)
{
	char		str[256],title[LEN_TITLE+1];
	char		msgpath[MAX_PATH+1];
	char*		msgbuf;
	const char*	editor=NULL;
	const char*	charset=NULL;
	int 		i,j,x;
	long		msgs=0;
	long		length;
	long		wm_mode=WM_EMAIL;
	FILE*		fp;
	smb_t		smb;
	smbmsg_t	msg;
	user_t		user;

	memset(&msg,0,sizeof(msg));
	
	title[0]=0;
	action=NODE_SMAL;
	nodesync();

	if(cfg.sys_misc&SM_ANON_EM && useron.exempt&FLAG('A')
		&& !noyes(text[AnonymousQ])) {
		msg.hdr.attr|=MSG_ANONYMOUS;
		wm_mode|=WM_ANON;
	}

	msg_tmp_fname(useron.xedit, msgpath, sizeof(msgpath));
	if(!writemsg(msgpath,nulstr,title,wm_mode,INVALID_SUB,"Bulk Mailing"
		,/* From: */useron.alias
		,&editor
		,&charset)) {
		bputs(text[Aborted]);
		return(false); 
	}

	if((fp=fopen(msgpath,"r"))==NULL) {
		errormsg(WHERE,ERR_OPEN,msgpath,O_RDONLY);
		return(false); 
	}

	if((length=(long)filelength(fileno(fp)))<=0) {
		fclose(fp);
		return(false);
	}

	bputs(text[WritingIndx]);
	CRLF;

	if((msgbuf=(char*)malloc(length+1))==NULL) {
		fclose(fp);
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

	SAFEPRINTF(str,"%u",useron.number);
	smb_hfield_str(&msg,SENDEREXT,str);

	smb_hfield_str(&msg,SUBJECT,title);

	msg.hdr.when_written.time=time32(NULL);
	msg.hdr.when_written.zone=sys_timezone(&cfg);

	editor_info_to_msg(&msg, editor, charset);

	memset(&smb,0,sizeof(smb));
	smb.subnum=INVALID_SUB;	/* mail database */
	i=savemsg(&cfg, &smb, &msg, &client, server_host_name(), msgbuf, /* remsg: */NULL);
	free(msgbuf);
	if(i!=0) {
		smb_close(&smb);
		smb_freemsgmem(&msg);
		return(false);
	}

	j=lastuser(&cfg);

	if(ar && *ar)
		for(i=1;i<=j;i++) {
			user.number=i;
			if(getuserdat(&cfg, &user)!=0)
				continue;
			if(user.misc&(DELETED|INACTIVE))
				continue;
			if(chk_ar(ar,&user,/* client: */NULL)) {
				if((x=bulkmailhdr(&smb, &msg, i))!=SMB_SUCCESS) {
					errormsg(WHERE,ERR_WRITE,smb.file,x);
					break;
				}
				msgs++;
			} 
		}
	else
		while(online) {
			bputs(text[EnterAfterLastDestUser]);
			if(!getstr(str,LEN_ALIAS,cfg.uq&UQ_NOUPRLWR ? K_NONE:K_UPRLWR))
				break;
			if((i=finduser(str))!=0) {
				if((x=bulkmailhdr(&smb, &msg, i))!=SMB_SUCCESS) {
					errormsg(WHERE,ERR_WRITE,smb.file,x);
					break;
				}
				msgs++; 
			}
		}

	if((i=smb_open_da(&smb))==SMB_SUCCESS) {
		if(!msgs)
			smb_freemsg_dfields(&smb,&msg,SMB_ALL_REFS);
		else if(msgs>1)
			smb_incmsg_dfields(&smb,&msg,(ushort)msgs-1);
		smb_close_da(&smb);
	}

	smb_close(&smb);
	smb_freemsgmem(&msg);

	if(i!=SMB_SUCCESS) {
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return(false);
	}

	putuserdec32(useron.number, USER_EMAILS, useron.emails);
	putuserdec32(useron.number, USER_ETODAY, useron.etoday);

	return(true);
}


int sbbs_t::bulkmailhdr(smb_t* smb, smbmsg_t* msg, uint usernum)
{
    char		str[256];
    int			i,j;
	ushort		nettype=NET_UNKNOWN;
    node_t		node;
	user_t		user;
	smbmsg_t	newmsg;

	user.number=usernum;
	if(getuserdat(&cfg, &user)!=0)
		return(0);

	if((i=smb_copymsgmem(NULL,&newmsg,msg))!=SMB_SUCCESS)
		return(i);

	SAFECOPY(str,user.alias);
	smb_hfield_str(&newmsg,RECIPIENT,str);

	if(cfg.sys_misc&SM_FWDTONET && user.misc&NETMAIL && user.netmail[0]) {
		bprintf(text[UserNetMail],user.netmail);
		smb_hfield_netaddr(&newmsg,RECIPIENTNETADDR,user.netmail,&nettype);
		smb_hfield_bin(&newmsg,RECIPIENTNETTYPE,nettype);
	} else {
		SAFEPRINTF(str,"%u",usernum);
		smb_hfield_str(&newmsg,RECIPIENTEXT,str);
	}

	j=smb_addmsghdr(smb,&newmsg,smb_storage_mode(&cfg, smb));
	smb_freemsgmem(&newmsg);
	if(j!=SMB_SUCCESS)
		return(j);

	lncntr=0;
	bprintf(text[Emailing],user.alias,usernum);
	SAFEPRINTF2(str,"bulk-mailed %s #%d"
		,user.alias,usernum);
	logline("E+",str);
	useron.emails++;
	logon_emails++;
	useron.etoday++;
	for(i=1;i<=cfg.sys_nodes;i++) { /* Tell user, if online */
		getnodedat(i, &node);
		if(node.useron==usernum && !(node.misc&NODE_POFF)
			&& (node.status==NODE_INUSE || node.status==NODE_QUIET)) {
			SAFEPRINTF2(str,text[EmailNodeMsg],cfg.node_num,useron.alias);
			putnmsg(i,str);
			break; 
		} 
	}
	if(i>cfg.sys_nodes) {   /* User wasn't online, so leave short msg */
		SAFEPRINTF(str,text[UserSentYouMail],useron.alias);
		putsmsg(usernum,str);
	}
	return(0);
}

