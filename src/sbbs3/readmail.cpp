/* readmail.cpp */

/* Synchronet private mail reading function */

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

/****************************************************************************/
/* Reads mail waiting for usernumber.                                       */
/****************************************************************************/
void sbbs_t::readmail(uint usernumber, int which)
{
	char	str[256],str2[256],str3[256],done=0,domsg=1
			,*p,*tp,*sp,ch;
	char 	tmp[512];
	int		i,j;
	int		error;
	int		mismatches=0,act;
	ulong 	msgs,curmsg;
    long    length,l,lm_mode;
	ulong	last;
	bool	replied;
	file_t	fd;
	mail_t	*mail;
	smbmsg_t msg;

	if(which==MAIL_SENT && useron.rest&FLAG('K')) {
		bputs(text[R_ReadSentMail]);
		return;
	}

	msg.total_hfields=0;			/* init to NULL, cause not allocated yet */

	fd.dir=cfg.total_dirs+1;			/* temp dir for file attachments */

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

	if(cfg.sys_misc&SM_SYSVDELM && (SYSOP || cfg.sys_misc&SM_USRVDELM))
		lm_mode=LM_INCDEL;
	else
		lm_mode=0;
	mail=loadmail(&smb,&msgs,usernumber,which,lm_mode);
	if(!msgs) {
		if(which==MAIL_SENT)
			bputs(text[NoMailSent]);
		else if(which==MAIL_ALL)
			bputs(text[NoMailOnSystem]);
		else
			bputs(text[NoMailWaiting]);
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		return; }

	last=smb.status.last_msg;

	if(which==MAIL_SENT)
		act=NODE_RSML;
	else if(which==MAIL_ALL)
		act=NODE_SYSP;
	else
		act=NODE_RMAL;
	action=act;
	if(msgs>1 && which!=MAIL_ALL) {
		if(which==MAIL_SENT)
			bputs(text[MailSentLstHdr]);
		else
			bputs(text[MailWaitingLstHdr]);

		for(curmsg=0;curmsg<msgs && !msgabort();curmsg++) {
			if(msg.total_hfields)
				smb_freemsgmem(&msg);
			msg.total_hfields=0;
			msg.idx.offset=mail[curmsg].offset;
			if(!loadmsg(&msg,mail[curmsg].number))
				continue;
			smb_unlockmsghdr(&smb,&msg);
			bprintf(text[MailWaitingLstFmt],curmsg+1
				,which==MAIL_SENT ? msg.to
				: (msg.hdr.attr&MSG_ANONYMOUS) && !SYSOP ? text[Anonymous]
				: msg.from
				,msg.hdr.attr&MSG_DELETE ? '-' : msg.hdr.attr&MSG_REPLIED ? 'R'
					: msg.hdr.attr&MSG_READ ? ' '
					: msg.from_net.type || msg.to_net.type ? 'N':'*'
				,msg.subj);
			smb_freemsgmem(&msg);
			msg.total_hfields=0; }

		ASYNC;
		if(!(sys_status&SS_ABORT)) {
			bprintf(text[StartWithN],1L);
			if((long)(curmsg=getnum(msgs))>0)
				curmsg--;
			else if((long)curmsg==-1) {
				FREE(mail);
				smb_close(&smb);
				smb_stack(&smb,SMB_STACK_POP);
				return; } }
		sys_status&=~SS_ABORT; }
	else {
		curmsg=0;
		if(which==MAIL_ALL)
			domsg=0; }
	if(which==MAIL_SENT) {
		sprintf(str,"%s read sent mail",useron.alias);
		logline("E",str);
	} else if(which==MAIL_ALL) {
		sprintf(str,"%s read all mail",useron.alias);
		logline("S+",str);
	} else {
		sprintf(str,"%s read mail",useron.alias);
		logline("E",str);
	}
	if(useron.misc&RIP) {
		strcpy(str,which==MAIL_YOUR ? "mailread" : which==MAIL_ALL ?
			"allmail" : "sentmail");
		menu(str); }
	while(online && !done) {
		action=act;

		if(msg.total_hfields)
			smb_freemsgmem(&msg);
		msg.total_hfields=0;

		msg.idx.offset=mail[curmsg].offset;
		msg.idx.number=mail[curmsg].number;
		msg.idx.to=mail[curmsg].to;
		msg.idx.from=mail[curmsg].from;
		msg.idx.subj=mail[curmsg].subj;

		if((i=smb_locksmbhdr(&smb))!=0) {
			errormsg(WHERE,ERR_LOCK,smb.file,i);
			break; }

		if((i=smb_getstatus(&smb))!=0) {
			smb_unlocksmbhdr(&smb);
			errormsg(WHERE,ERR_READ,smb.file,i);
			break; }
		smb_unlocksmbhdr(&smb);

		if(smb.status.last_msg!=last) { 	/* New messages */
			last=smb.status.last_msg;
			FREE(mail);
			mail=loadmail(&smb,&msgs,usernumber,which,lm_mode);   /* So re-load */
			if(!msgs)
				break;
			for(curmsg=0;curmsg<msgs;curmsg++)
				if(mail[curmsg].number==msg.idx.number)
					break;
			if(curmsg>=msgs)
				curmsg=(msgs-1);
			continue; }

		if(!loadmsg(&msg,mail[curmsg].number)) {	/* Message header gone */
			if(mismatches>5) {	/* We can't do this too many times in a row */
				errormsg(WHERE,ERR_CHK,"message number",mail[curmsg].number);
				break; }
			FREE(mail);
			mail=loadmail(&smb,&msgs,usernumber,which,lm_mode);
			if(!msgs)
				break;
			if(curmsg>(msgs-1))
				curmsg=(msgs-1);
			mismatches++;
			continue; }
		smb_unlockmsghdr(&smb,&msg);
		msg.idx.attr=msg.hdr.attr;

		mismatches=0;

		if(domsg && !(sys_status&SS_ABORT)) {

			show_msg(&msg
				,msg.from_ext && msg.idx.from==1 && !msg.from_net.type
					? 0:P_NOATCODES);

			if(msg.hdr.auxattr&MSG_FILEATTACH) {  /* Attached file */
				smb_getmsgidx(&smb,&msg);
				strcpy(str,msg.subj);					/* filenames in title */
				strupr(str);
				tp=str;
				while(online) {
					p=strchr(tp,SP);
					if(p) *p=0;
					sp=strrchr(tp,'/');              /* sp is slash pointer */
					if(!sp) sp=strrchr(tp,'\\');
					if(sp) tp=sp+1;
					padfname(tp,fd.name);
					sprintf(str2,"%sfile/%04u.in/%s"  /* str2 is path/fname */
						,cfg.data_dir,msg.idx.to,tp);
					length=flength(str2);
					if(length<1)
						bputs(text[FileNotFound]);
					else if(!(useron.exempt&FLAG('T')) && cur_cps && !SYSOP
						&& length/(long)cur_cps>(time_t)timeleft)
						bputs(text[NotEnoughTimeToDl]);
					else {
						sprintf(str3,text[DownloadAttachedFileQ]
							,tp,ultoac(length,tmp));
						if(length>0L && yesno(str3)) {
							if(online==ON_LOCAL) {
								bputs(text[EnterPath]);
								if(getstr(str3,60,K_LINE|K_UPPER)) {
									backslashcolon(str3);
									sprintf(tmp,"%s%s",str3,tp);
									if(!mv(str2,tmp,which!=MAIL_YOUR)) {
										logon_dlb+=length;
										logon_dls++;
										useron.dls=(ushort)adjustuserrec(&cfg,useron.number
											,U_DLS,5,1);
										useron.dlb=adjustuserrec(&cfg,useron.number
											,U_DLB,10,length);
										bprintf(text[FileNBytesSent]
											,fd.name,ultoac(length,tmp)); } } }

							else {	/* Remote User */
								menu("dlprot");
								mnemonics(text[ProtocolOrQuit]);
								strcpy(str3,"Q");
								for(i=0;i<cfg.total_prots;i++)
									if(cfg.prot[i]->dlcmd[0]
										&& chk_ar(cfg.prot[i]->ar,&useron)) {
										sprintf(tmp,"%c",cfg.prot[i]->mnemonic);
										strcat(str3,tmp); }
								ch=(char)getkeys(str3,0);
								for(i=0;i<cfg.total_prots;i++)
									if(cfg.prot[i]->dlcmd[0] && ch==cfg.prot[i]->mnemonic
										&& chk_ar(cfg.prot[i]->ar,&useron))
										break;
								if(i<cfg.total_prots) {
									error=protocol(cmdstr(cfg.prot[i]->dlcmd,str2,nulstr,NULL),false);
									if((cfg.prot[i]->misc&PROT_DSZLOG
										&& checkprotlog(&fd))
										|| (!(cfg.prot[i]->misc&PROT_DSZLOG) && !error)) {
											if(which==MAIL_YOUR)
												remove(str2);
											logon_dlb+=length;	/* Update stats */
											logon_dls++;
											useron.dls=(ushort)adjustuserrec(&cfg,useron.number
												,U_DLS,5,1);
											useron.dlb=adjustuserrec(&cfg,useron.number
												,U_DLB,10,length);
											bprintf(text[FileNBytesSent]
												,fd.name,ultoac(length,tmp));
											sprintf(str3
												,"%s downloaded attached file: %s"
												,useron.alias
												,fd.name);
											logline("D-",str3); }
									autohangup(); } } } }
						if(!p)
							break;
						tp=p+1;
						while(*tp==SP) tp++; }
				sprintf(str,"%sfile/%04u.in",cfg.data_dir,usernumber);
				rmdir(str); }
			if(which==MAIL_YOUR && !(msg.hdr.attr&MSG_READ)) {
				mail[curmsg].attr|=MSG_READ;
				if(thisnode.status==NODE_INUSE)
					telluser(&msg);
				if(msg.total_hfields)
					smb_freemsgmem(&msg);
				msg.total_hfields=0;
				msg.idx.offset=0;						/* Search by number */
				if(!smb_locksmbhdr(&smb)) { 			/* Lock the entire base */
					if(loadmsg(&msg,msg.idx.number)) {
						msg.hdr.attr|=MSG_READ;
						msg.idx.attr=msg.hdr.attr;
						if((i=smb_putmsg(&smb,&msg))!=0)
							errormsg(WHERE,ERR_WRITE,smb.file,i);
						smb_unlockmsghdr(&smb,&msg); }
					smb_unlocksmbhdr(&smb); }
				if(!msg.total_hfields) {				/* unsuccessful reload */
					domsg=0;
					continue; } }
			}
		else domsg=1;

		if(useron.misc&WIP) {
			strcpy(str,which==MAIL_YOUR ? "mailread" : which==MAIL_ALL ?
				"allmail" : "sentmail");
			menu(str); }

		ASYNC;
		if(which==MAIL_SENT)
			bprintf(text[ReadingSentMail],curmsg+1,msgs);
		else if(which==MAIL_ALL)
			bprintf(text[ReadingAllMail],curmsg+1,msgs);
		else
			bprintf(text[ReadingMail],curmsg+1,msgs);
		sprintf(str,"ADFLNQRT?<>[]{}-+");
		if(SYSOP)
			strcat(str,"CUSPH");
		if(which!=MAIL_YOUR)
			strcat(str,"E");
		l=getkeys(str,msgs);
		if(l&0x80000000L) {
			if(l==-1)	/* ctrl-c */
				break;
			curmsg=(l&~0x80000000L)-1;
			continue; }
		switch(l) {
			case 'A':   /* Auto-reply to last piece */
			case 'R':
				if(l==(cfg.sys_misc&SM_RA_EMU ? 'A' : 'R'))  /* re-read last message */
					break;

				if(which==MAIL_SENT)
					break;
				if((msg.hdr.attr&MSG_ANONYMOUS) && !SYSOP) {
					bputs(text[CantReplyToAnonMsg]);
					break; }

				quotemsg(&msg,1);

				if(msg.from_net.addr==NULL)
					strcpy(str,msg.from);
				else if(msg.from_net.type==NET_FIDO) 	/* FidoNet type */
					sprintf(str,"%s @%s",msg.from
						,faddrtoa((faddr_t *)msg.from_net.addr,tmp));
				else if(msg.from_net.type==NET_INTERNET)
					strcpy(str,(char *)msg.from_net.addr);
				else
					sprintf(str,"%s@%s",msg.from,(char*)msg.from_net.addr);

				strcpy(str2,str);

				bputs(text[Email]);
				if(!getstr(str,64,K_EDIT|K_AUTODEL))
					break;
				msg.hdr.number=msg.idx.number;
				smb_getmsgidx(&smb,&msg);

				if(!stricmp(str2,str))		/* Reply to sender */
					sprintf(str2,text[Regarding],msg.subj);
				else						/* Reply to other */
					sprintf(str2,text[RegardingByOn],msg.subj,msg.from
						,timestr((time_t *)&msg.hdr.when_written.time));

				p=strrchr(str,'@');
				if(p) { 							/* name @addr */
					replied=netmail(str,msg.subj,WM_QUOTE);
					sprintf(str2,text[DeleteMailQ],msg.from); }
				else {
					if(!msg.from_net.type && !stricmp(str,msg.from))
						replied=email(msg.idx.from,str2,msg.subj,WM_EMAIL|WM_QUOTE);
					else if(!stricmp(str,"SYSOP"))
						replied=email(1,str2,msg.subj,WM_EMAIL|WM_QUOTE);
					else if((i=finduser(str))!=0)
						replied=email(i,str2,msg.subj,WM_EMAIL|WM_QUOTE);
					else
						replied=false;
					sprintf(str2,text[DeleteMailQ],msg.from); }

				if(replied==true && !(msg.hdr.attr&MSG_REPLIED)) {
					if(msg.total_hfields)
						smb_freemsgmem(&msg);
					msg.total_hfields=0;
					msg.idx.offset=0;
					if(loadmsg(&msg,msg.idx.number)) {
						msg.hdr.attr|=MSG_REPLIED;
						msg.idx.attr=msg.hdr.attr;
						if((i=smb_putmsg(&smb,&msg))!=0)
							errormsg(WHERE,ERR_WRITE,smb.file,i);
						smb_unlockmsghdr(&smb,&msg); }
				}

				if(msg.hdr.attr&MSG_DELETE || !yesno(str2)) {
					if(curmsg<msgs-1) curmsg++;
					else done=1;
					break;	}
				/* Case 'D': must follow! */
			case 'D':   /* Delete last piece (toggle) */
				if(msg.hdr.attr&MSG_PERMANENT) {
					bputs("\r\nPermanent message.\r\n");
					domsg=0;
					break; }
				if(msg.total_hfields)
					smb_freemsgmem(&msg);
				msg.total_hfields=0;
				msg.idx.offset=0;
				if(loadmsg(&msg,msg.idx.number)) {
					msg.hdr.attr^=MSG_DELETE;
					msg.idx.attr=msg.hdr.attr;
	//				  mail[curmsg].attr=msg.hdr.attr;
					if((i=smb_putmsg(&smb,&msg))!=0)
						errormsg(WHERE,ERR_WRITE,smb.file,i);
					smb_unlockmsghdr(&smb,&msg); }
				if(curmsg<msgs-1) curmsg++;
				else done=1;
				break;
			case 'F':  /* Forward last piece */
				domsg=0;
				bputs(text[ForwardMailTo]);
				if(!getstr(str,LEN_ALIAS,K_UPRLWR))
					break;
				i=finduser(str);
				if(!i)
					break;
				domsg=1;
				if(curmsg<msgs-1) curmsg++;
				else done=1;
				smb_getmsgidx(&smb,&msg);
				forwardmail(&msg,i);
				if(msg.hdr.attr&MSG_PERMANENT)
					break;
				sprintf(str2,text[DeleteMailQ],msg.from);
				if(!yesno(str2))
					break;
				if(msg.total_hfields)
					smb_freemsgmem(&msg);
				msg.total_hfields=0;
				msg.idx.offset=0;
				if(loadmsg(&msg,msg.idx.number)) {
					msg.hdr.attr|=MSG_DELETE;
					msg.idx.attr=msg.hdr.attr;
	//				  mail[curmsg].attr=msg.hdr.attr;
					if((i=smb_putmsg(&smb,&msg))!=0)
						errormsg(WHERE,ERR_WRITE,smb.file,i);
					smb_unlockmsghdr(&smb,&msg); }

				break;
			case 'H':
				domsg=0;
				msghdr(&msg);
				break;
			case 'L':     /* List mail */
				domsg=0;
				bprintf(text[StartWithN],(long)curmsg+1);
				if((i=getnum(msgs))>0)
					i--;
				else if(i==-1)
					break;
				else
					i=curmsg;
				if(which==MAIL_SENT)
					bputs(text[MailSentLstHdr]);
				else if(which==MAIL_ALL)
					bputs(text[MailOnSystemLstHdr]);
				else
					bputs(text[MailWaitingLstHdr]);
				for(;(ulong)i<msgs && !msgabort();i++) {
					if(msg.total_hfields)
						smb_freemsgmem(&msg);
					msg.total_hfields=0;
					msg.idx.offset=mail[i].offset;
					if(!loadmsg(&msg,mail[i].number))
						continue;
					smb_unlockmsghdr(&smb,&msg);
					if(which==MAIL_ALL)
						bprintf(text[MailOnSystemLstFmt]
							,i+1,msg.from,msg.to
							,msg.hdr.attr&MSG_DELETE ? '-' : msg.hdr.attr&MSG_REPLIED ? 'R'
								: msg.hdr.attr&MSG_READ ? SP
								: msg.from_net.type || msg.to_net.type ? 'N':'*'
							,msg.subj);
					else
						bprintf(text[MailWaitingLstFmt],i+1
							,which==MAIL_SENT ? msg.to
							: (msg.hdr.attr&MSG_ANONYMOUS) && !SYSOP
							? text[Anonymous] : msg.from
							,msg.hdr.attr&MSG_DELETE ? '-' : msg.hdr.attr&MSG_REPLIED ? 'R'
								: msg.hdr.attr&MSG_READ ? SP
								: msg.from_net.type || msg.to_net.type ? 'N':'*'
							,msg.subj);
					smb_freemsgmem(&msg);
					msg.total_hfields=0; }
				break;
			case 'Q':
				done=1;
				break;
			case 'C':   /* Change attributes of last piece */
				i=chmsgattr(msg.hdr.attr);
				if(msg.hdr.attr==i)
					break;
				if(msg.total_hfields)
					smb_freemsgmem(&msg);
				msg.total_hfields=0;
				msg.idx.offset=0;
				if(loadmsg(&msg,msg.idx.number)) {
					msg.hdr.attr=msg.idx.attr=(ushort)i;
					if((i=smb_putmsg(&smb,&msg))!=0)
						errormsg(WHERE,ERR_WRITE,smb.file,i);
					smb_unlockmsghdr(&smb,&msg); }
				break;
			case '>':
				for(i=curmsg+1;(ulong)i<msgs;i++)
					if(mail[i].subj==msg.idx.subj)
						break;
				if((ulong)i<msgs)
					curmsg=i;
				else
					domsg=0;
				break;
			case '<':   /* Search Title backward */
				for(i=curmsg-1;i>-1;i--)
					if(mail[i].subj==msg.idx.subj)
						break;
				if(i>-1)
					curmsg=i;
				else
					domsg=0;
				break;
			case '}':   /* Search Author forward */
				strcpy(str,msg.from);
				for(i=curmsg+1;(ulong)i<msgs;i++)
					if(mail[i].from==msg.idx.from)
						break;
				if((ulong)i<msgs)
					curmsg=i;
				else
					domsg=0;
				break;
			case 'N':   /* Got to next un-read message */
				for(i=curmsg+1;(ulong)i<msgs;i++)
					if(!(mail[i].attr&MSG_READ))
						break;
				if((ulong)i<msgs)
					curmsg=i;
				else
					domsg=0;
				break;
			case '{':   /* Search Author backward */
				strcpy(str,msg.from);
				for(i=curmsg-1;i>-1;i--)
					if(mail[i].from==msg.idx.from)
						break;
				if(i>-1)
					curmsg=i;
				else
					domsg=0;
				break;
			case ']':   /* Search To User forward */
				strcpy(str,msg.to);
				for(i=curmsg+1;(ulong)i<msgs;i++)
					if(mail[i].to==msg.idx.to)
						break;
				if((ulong)i<msgs)
					curmsg=i;
				else
					domsg=0;
				break;
			case '[':   /* Search To User backward */
				strcpy(str,msg.to);
				for(i=curmsg-1;i>-1;i--)
					if(mail[i].to==msg.idx.to)
						break;
				if(i>-1)
					curmsg=i;
				else
					domsg=0;
				break;
			case 0:
			case '+':
				if(curmsg<msgs-1) curmsg++;
				else done=1;
				break;
			case '-':
				if(curmsg>0) curmsg--;
				break;
			case 'S':
				domsg=0;
	/*
				if(!yesno(text[SaveMsgToFile]))
					break;
	*/
				bputs(text[FileToWriteTo]);
				if(getstr(str,40,K_LINE|K_UPPER))
					msgtotxt(&msg,str,1,1);
				break;
			case 'E':
				editmsg(&msg,INVALID_SUB);
				break;
			case 'T':
				domsg=0;
				i=curmsg;
				if(i) i++;
				j=i+10;
				if((ulong)j>msgs)
					j=msgs;

				if(which==MAIL_SENT)
					bputs(text[MailSentLstHdr]);
				else if(which==MAIL_ALL)
					bputs(text[MailOnSystemLstHdr]);
				else
					bputs(text[MailWaitingLstHdr]);
				for(;i<j;i++) {
					if(msg.total_hfields)
						smb_freemsgmem(&msg);
					msg.total_hfields=0;
					msg.idx.offset=mail[i].offset;
					if(!loadmsg(&msg,mail[i].number))
						continue;
					smb_unlockmsghdr(&smb,&msg);
					if(which==MAIL_ALL)
						bprintf(text[MailOnSystemLstFmt]
							,i+1,msg.from,msg.to
							,msg.hdr.attr&MSG_DELETE ? '-' : msg.hdr.attr&MSG_REPLIED ? 'R'
								: msg.hdr.attr&MSG_READ ? SP 
								: msg.from_net.type || msg.to_net.type ? 'N':'*'
							,msg.subj);
					else
						bprintf(text[MailWaitingLstFmt],i+1
							,which==MAIL_SENT ? msg.to
							: (msg.hdr.attr&MSG_ANONYMOUS) && !SYSOP
							? text[Anonymous] : msg.from
							,msg.hdr.attr&MSG_DELETE ? '-' : msg.hdr.attr&MSG_REPLIED ? 'R'
								: msg.hdr.attr&MSG_READ ? SP
								: msg.from_net.type || msg.to_net.type ? 'N':'*'
							,msg.subj);
					smb_freemsgmem(&msg);
					msg.total_hfields=0; }
				curmsg=(i-1);
				break;
			case 'U':   /* user edit */
				msg.hdr.number=msg.idx.number;
				smb_getmsgidx(&smb,&msg);
				useredit(which==MAIL_SENT ? msg.idx.to : msg.idx.from);
				break;
			case 'P':   /* Purge author and all mail to/from */
				if(noyes(text[AreYouSureQ]))
					break;
				msg.hdr.number=msg.idx.number;
				smb_getmsgidx(&smb,&msg);
				purgeuser(msg.idx.from);
				if(curmsg<msgs-1) curmsg++;
				break;
			case '?':
				strcpy(str,which==MAIL_YOUR ? "mailread" : which==MAIL_ALL
						? "allmail" : "sentmail");
				menu(str);
				if(SYSOP && which==MAIL_SENT)
					menu("syssmail");
				else if(SYSOP && which==MAIL_YOUR)
					menu("sysmailr");   /* Sysop Mail Read */
				domsg=0;
				break;
				} }

	if(msg.total_hfields)
		smb_freemsgmem(&msg);

	if(msgs)
		FREE(mail);

	/***************************************/
	/* Delete messages marked for deletion */
	/***************************************/

	if(cfg.sys_misc&SM_DELEMAIL) {
		if((i=smb_locksmbhdr(&smb))!=0) 			/* Lock the base, so nobody */
			errormsg(WHERE,ERR_LOCK,smb.file,i);	/* messes with the index */
		else
			delmail(usernumber,which); }

	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
}

