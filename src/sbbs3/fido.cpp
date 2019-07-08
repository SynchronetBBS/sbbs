/* Synchronet FidoNet-related routines */

/* $Id$ */

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

/* TODO: qwktonetmail() -> fidonet: write to SMB, not *.msg		*/

#include "sbbs.h"
#include "qwk.h"

faddr_t atofaddr(scfg_t* cfg, char *str);

void pt_zone_kludge(fmsghdr_t hdr,int fido)
{
	char str[256];

	sprintf(str,"\1INTL %hu:%hu/%hu %hu:%hu/%hu\r"
		,hdr.destzone,hdr.destnet,hdr.destnode
		,hdr.origzone,hdr.orignet,hdr.orignode);
	write(fido,str,strlen(str));

	if(hdr.destpoint) {
		sprintf(str,"\1TOPT %hu\r"
			,hdr.destpoint);
		write(fido,str,strlen(str)); 
	}

	if(hdr.origpoint) {
		sprintf(str,"\1FMPT %hu\r"
			,hdr.origpoint);
		write(fido,str,strlen(str)); 
	}
}

bool sbbs_t::lookup_netuser(char *into)
{
	char to[128],name[26],str[256],q[128];
	int i;
	FILE *stream;

	if(strchr(into,'@'))
		return(false);
	SAFECOPY(to,into);
	strupr(to);
	sprintf(str,"%sqnet/users.dat", cfg.data_dir);
	if((stream=fnopen(&i,str,O_RDONLY))==NULL)
		return(false);
	while(!feof(stream)) {
		if(!fgets(str,sizeof(str),stream))
			break;
		str[25]=0;
		truncsp(str);
		SAFECOPY(name,str);
		strupr(name);
		str[35]=0;
		truncsp(str+27);
		sprintf(q,"Do you mean %s @%s",str,str+27);
		if(strstr(name,to) && yesno(q)) {
			fclose(stream);
			sprintf(into,"%s@%s",str,str+27);
			return(true); 
		}
		if(sys_status&SS_ABORT)
			break; 
	}
	fclose(stream);
	return(false);
}

/****************************************************************************/
/* Send FidoNet/QWK/Internet NetMail from BBS								*/
/****************************************************************************/
bool sbbs_t::netmail(const char *into, const char *title, long mode, smb_t* resmb, smbmsg_t* remsg)
{
	char	str[256],fname[128],*buf,*p,ch;
	char	to[256] = "";
	char	from[FIDO_NAME_LEN]= "";
	char	subj[FIDO_SUBJ_LEN]= "";
	char	msgpath[MAX_PATH+1];
	char 	tmp[512];
	char*	editor=NULL;
	char*	charset=NULL;
	int		file,x;
	uint	i;
	long	length,l;
	faddr_t src_addr;
	faddr_t dest_addr;
	uint16_t net_type;
	smbmsg_t msg;
	memset(&msg, 0, sizeof(msg));

	if(useron.etoday>=cfg.level_emailperday[useron.level] && !SYSOP && !(useron.exempt&FLAG('M'))) {
		bputs(text[TooManyEmailsToday]);
		return false; 
	}

	if(useron.rest&FLAG('M')) {
		bputs(text[NoNetMailAllowed]);
		return false;
	}
	
	if(title != NULL)
		SAFECOPY(subj, title);
	if(into != NULL)
		SAFECOPY(to, into);
	if(remsg != NULL) {
		if(subj[0] == 0 && remsg->subj != NULL)
			SAFECOPY(subj, remsg->subj);
		if(to[0] == 0) {
			if((p = smb_netaddrstr(&remsg->from_net, tmp)) != NULL) {
				if(strchr(p, '@')) {
					SAFECOPY(to, p);
				} else {
					SAFEPRINTF2(to, "%s@%s", remsg->from, p);
				}
			} else {
				SAFECOPY(to, remsg->from);
			}
		}
	}

	lookup_netuser(to);

	net_type = smb_netaddr_type(to);
	lprintf(LOG_DEBUG, "parsed net type of '%s' is %s", to, smb_nettype((enum smb_net_type)net_type));
	if(net_type == NET_QWK) {
		if(mode&WM_FILE) {
			bputs(text[EmailFilesNotAllowed]);
			mode&=~WM_FILE;
		}
		return qnetmail(to, title, mode, resmb, remsg);
	}
	if(net_type == NET_INTERNET) {
		if(!(cfg.inetmail_misc&NMAIL_ALLOW)) {
			bputs(text[NoNetMailAllowed]);
			return false;
		}
		if(mode&WM_FILE && !SYSOP && !(cfg.inetmail_misc&NMAIL_FILE)) {
			bputs(text[EmailFilesNotAllowed]);
			mode&=~WM_FILE;
		}
		return inetmail(to, title, mode, resmb, remsg);
	}
	p=strrchr(to,'@');      /* Find '@' in name@addr */
	if(p==NULL || net_type != NET_FIDO) {
		bputs(text[InvalidNetMailAddr]);
		return false; 
	}
	if(!cfg.total_faddrs || (!SYSOP && !(cfg.netmail_misc&NMAIL_ALLOW))) {
		bputs(text[NoNetMailAllowed]);
		return false; 
	}
	*p=0;					/* Chop off address */
	p++;
	SKIP_WHITESPACE(p);
	dest_addr=atofaddr(&cfg,p); 	/* Get fido address */

	if((mode&WM_FILE) && !SYSOP && !(cfg.netmail_misc&NMAIL_FILE)) {
		bputs(text[EmailFilesNotAllowed]);
		mode&=~WM_FILE;
	}

	truncsp(to);				/* Truncate off space */

	SAFECOPY(from, cfg.netmail_misc&NMAIL_ALIAS ? useron.alias : useron.name);

	/* Look-up in nodelist? */

	if(cfg.netmail_cost && !(useron.exempt&FLAG('S'))) {
		if(useron.cdt+useron.freecdt<cfg.netmail_cost) {
			bputs(text[NotEnoughCredits]);
			return(false); 
		}
		sprintf(str,text[NetMailCostContinueQ],cfg.netmail_cost);
		if(noyes(str))
			return(false); 
	}

	for(i=0;i<cfg.total_faddrs;i++)
		if(dest_addr.zone==cfg.faddr[i].zone && dest_addr.net==cfg.faddr[i].net)
			break;
	if(i==cfg.total_faddrs) {
		for(i=0;i<cfg.total_faddrs;i++)
			if(dest_addr.zone==cfg.faddr[i].zone)
				break; 
	}
	if(i >= cfg.total_faddrs)
		i=0;

	if((cfg.netmail_misc&NMAIL_CHSRCADDR) && cfg.total_faddrs > 1) {
		for(uint j=0; j < cfg.total_faddrs; j++)
			uselect(/* add: */TRUE, j, text[OriginFidoAddr], smb_faddrtoa(&cfg.faddr[j], tmp), /* ar: */NULL);
		int choice = uselect(/* add: */FALSE, /* default: */i, NULL, NULL, NULL);
		if(choice < 0)
			return false;
		i = choice;
	}
	src_addr = cfg.faddr[i];

	smb_faddrtoa(&cfg.faddr[i], str);
	bprintf(text[NetMailing], to, smb_faddrtoa(&dest_addr,tmp), from, str);

	if(cfg.netmail_misc&NMAIL_CRASH) msg.hdr.netattr |= MSG_CRASH;
	if(cfg.netmail_misc&NMAIL_HOLD)  msg.hdr.netattr |= MSG_HOLD;
	if(cfg.netmail_misc&NMAIL_KILL)  msg.hdr.netattr |= MSG_KILLSENT;
	if(mode&WM_FILE) msg.hdr.auxattr |= MSG_FILEATTACH; 

	if(remsg != NULL && resmb != NULL && !(mode&WM_QUOTE)) {
		if(quotemsg(resmb, remsg, /* include tails: */true))
			mode |= WM_QUOTE;
	}

	msg_tmp_fname(useron.xedit, msgpath, sizeof(msgpath));
	if(!writemsg(msgpath,nulstr,subj,WM_NETMAIL|mode,INVALID_SUB, to, from, &editor, &charset)) {
		bputs(text[Aborted]);
		return(false); 
	}

	if(mode&WM_FILE) {
		SAFECOPY(fname, subj);
		sprintf(str,"%sfile/%04u.out", cfg.data_dir, useron.number);
		MKDIR(str);
		SAFECOPY(tmp, cfg.data_dir);
		if(tmp[0]=='.')    /* Relative path */
			sprintf(tmp,"%s%s", cfg.node_dir, cfg.data_dir);
		sprintf(str,"%sfile/%04u.out/%s",tmp,useron.number,fname);
		SAFECOPY(subj, str);
		if(fexistcase(str)) {
			bputs(text[FileAlreadyThere]);
			return(false); 
		}
		{ /* Remote */
			xfer_prot_menu(XFER_UPLOAD);
			mnemonics(text[ProtocolOrQuit]);
			sprintf(str,"%c",text[YNQP][2]);
			for(x=0;x<cfg.total_prots;x++)
				if(cfg.prot[x]->ulcmd[0] && chk_ar(cfg.prot[x]->ar,&useron,&client)) {
					sprintf(tmp,"%c",cfg.prot[x]->mnemonic);
					strcat(str,tmp); 
				}
			ch=(char)getkeys(str,0);
			if(ch==text[YNQP][2] || sys_status&SS_ABORT) {
				bputs(text[Aborted]);
				return(false); 
			}
			for(x=0;x<cfg.total_prots;x++)
				if(cfg.prot[x]->ulcmd[0] && cfg.prot[x]->mnemonic==ch
					&& chk_ar(cfg.prot[x]->ar,&useron,&client))
					break;
			if(x<cfg.total_prots)	/* This should be always */
				protocol(cfg.prot[x],XFER_UPLOAD,subj,nulstr,true); 
		}
		sprintf(tmp,"%s%s",cfg.temp_dir,title);
		if(!fexistcase(subj) && fexistcase(tmp))
			mv(tmp,subj,0);
		l=(long)flength(subj);
		if(l>0)
			bprintf(text[FileNBytesReceived],fname,ultoac(l,tmp));
		else {
			bprintf(text[FileNotReceived],fname);
			return(false); 
		} 
	}

	lprintf(LOG_DEBUG, "NetMail subject: %s", subj);
	p=subj;
	if((SYSOP || useron.exempt&FLAG('F'))
		&& !strnicmp(p,"CR:",3)) {     /* Crash over-ride by sysop */
		p+=3;				/* skip CR: */
		SKIP_WHITESPACE(p);
		msg.hdr.netattr |= MSG_CRASH; 
	}

	if((SYSOP || useron.exempt&FLAG('F'))
		&& !strnicmp(p,"FR:",3)) {     /* File request */
		p+=3;				/* skip FR: */
		SKIP_WHITESPACE(p);
		msg.hdr.auxattr |= MSG_FILEREQUEST; 
	}

	if((SYSOP || useron.exempt&FLAG('F'))
		&& !strnicmp(p,"RR:",3)) {     /* Return receipt request */
		p+=3;				/* skip RR: */
		SKIP_WHITESPACE(p);
		msg.hdr.auxattr |= MSG_RECEIPTREQ; 
	}

	if((SYSOP || useron.exempt&FLAG('F'))
		&& !strnicmp(p,"FA:",3)) {     /* File Attachment */
		p+=3;				/* skip FA: */
		SKIP_WHITESPACE(p);
//		hdr.attr|=FIDO_FILE;	TODO
	}

	if(p != subj)
		SAFECOPY(subj, p);

	if((file=nopen(msgpath,O_RDONLY))==-1) {
		errormsg(WHERE,ERR_OPEN,msgpath,O_RDONLY);
		return(false); 
	}
	length=(long)filelength(file);
	if((buf=(char *)calloc(1, length+1))==NULL) {
		close(file);
		errormsg(WHERE,ERR_ALLOC,str,length);
		return(false); 
	}
	read(file,buf,length);
	close(file);

	smb_net_type_t nettype = NET_FIDO;
	smb_hfield_str(&msg,SENDER, from);
	sprintf(str,"%u",useron.number);
	smb_hfield_str(&msg,SENDEREXT,str);
	smb_hfield(&msg,SENDERNETTYPE, sizeof(nettype), &nettype); 
	smb_hfield(&msg,SENDERNETADDR, sizeof(dest_addr), &src_addr); 

	smb_hfield_str(&msg,RECIPIENT, to);
	smb_hfield(&msg,RECIPIENTNETTYPE, sizeof(nettype), &nettype); 
	smb_hfield(&msg,RECIPIENTNETADDR, sizeof(dest_addr), &dest_addr); 

	smb_hfield_str(&msg,SUBJECT, subj);

	editor_info_to_msg(&msg, editor, charset);

	if(cfg.netmail_misc&NMAIL_DIRECT)
		msg.hdr.netattr |= MSG_DIRECT;

	smb_t smb;
	memset(&smb, 0, sizeof(smb));
	smb.subnum = INVALID_SUB;
	int result = savemsg(&cfg, &smb, &msg, &client, startup->host_name, buf, remsg);
	free(buf);
	smb_close(&smb);
	smb_freemsgmem(&msg);
	if(result != SMB_SUCCESS) {
		errormsg(WHERE, ERR_WRITE, smb.file, result, smb.last_error);
		return false;
	}

	useron.emails = (ushort)adjustuserrec(&cfg, useron.number, U_EMAILS, 0, 1);
	logon_emails++;
	useron.etoday = (ushort)adjustuserrec(&cfg, useron.number, U_ETODAY, 0, 1);
	if(!(useron.exempt&FLAG('S')))
		subtract_cdt(&cfg,&useron,cfg.netmail_cost);
	if(mode&WM_FILE)
		SAFEPRINTF2(str,"sent NetMail file attachment to %s (%s)"
			,to, smb_faddrtoa(&dest_addr,tmp));
	else
		SAFEPRINTF2(str,"sent NetMail to %s (%s)"
			,to, smb_faddrtoa(&dest_addr,tmp));
	logline("EN",str);

	return true;
}

/****************************************************************************/
/* Send NetMail from QWK REP Packet 										*/
/****************************************************************************/
void sbbs_t::qwktonetmail(FILE *rep, char *block, char *into, uchar fromhub)
{
	char	*qwkbuf,to[129],name[129],sender[129],senderaddr[129]
			   ,str[256],*p,*cp,*addr,fulladdr[129],ch;
	char*	sender_id = fromhub ? cfg.qhub[fromhub-1]->id : useron.alias;
	char*	subject = NULL;
	char 	tmp[512];
	int 	i,fido,inet=0,qnet=0;
	uint16_t net;
	uint16_t xlat;
	long	l,offset,length,m,n;
	faddr_t fidoaddr;
    fmsghdr_t hdr;
	smbmsg_t msg;
	struct	tm tm;

	if(useron.rest&FLAG('M')) { 
		bputs(text[NoNetMailAllowed]);
		return; 
	}

	to[0]=0;
	name[0]=0;
	sender[0]=0;
	senderaddr[0]=0;
	fulladdr[0]=0;

	sprintf(str,"%.6s",block+116);
	n=atol(str);	  /* number of 128 byte records */

	if(n<2L || n>999999L) {
		errormsg(WHERE,ERR_CHK,"QWK blocks",n);
		return; 
	}
	// Allocate/zero an extra block of NULs for strchr() usage and other ASCIIZ goodness
	if((qwkbuf=(char *)calloc(n + 1, QWK_BLOCK_LEN))==NULL) {
		errormsg(WHERE,ERR_ALLOC,nulstr,n*QWK_BLOCK_LEN);
		return; 
	}
	memcpy((char *)qwkbuf,block,QWK_BLOCK_LEN);
	fread(qwkbuf+QWK_BLOCK_LEN,n-1,QWK_BLOCK_LEN,rep);

	size_t kludge_hdrlen = 0;
	char* beg = qwkbuf + QWK_BLOCK_LEN;
	char* end = qwkbuf + (n * QWK_BLOCK_LEN);
	p = beg;
	if(into==NULL) {
		SAFECOPY(to, p);  /* To user on first line */
		char* tp = strchr(to, QWK_NEWLINE);		/* chop off at first CR */
		if(tp != NULL)
			*tp = 0;
		p += strlen(to) + 1;
	}
	else
		SAFECOPY(to, into);

	// Parse QWKE Kludge Lines here:
	while(p < end && *p != QWK_NEWLINE) {
		if(strncmp(p, "To:", 3) == 0) {
			p += 3;
			SKIP_WHITESPACE(p);
			char* tp = strchr(p, QWK_NEWLINE);		/* chop off at first CR */
			if(tp != NULL)
				*tp = 0;
			SAFECOPY(to, p);
			p += strlen(p) + 1;
			continue;
		}
		if(strncmp(p, "Subject:", 8) == 0) {
			p += 8;
			SKIP_WHITESPACE(p);
			char* tp = strchr(p, QWK_NEWLINE);		/* chop off at first CR */
			if(tp != NULL)
				*tp = 0;
			subject = p;
			p += strlen(p) + 1;
			continue;
		}
		break;
	}
	kludge_hdrlen += (p - beg) + 1;

	SAFECOPY(name,to);
	p=strchr(name,'@');
	if(p) *p=0;
	truncsp(name);


	p=strrchr(to,'@');       /* Find '@' in name@addr */
	if(p && !isdigit(*(p+1)) && !strchr(p,'.') && !strchr(p,':')) { /* QWKnet */
		qnet=1;
		*p=0; 
	}
	else if(p==NULL || !isdigit(*(p+1)) || !cfg.total_faddrs) {
		if(cfg.inetmail_misc&NMAIL_ALLOW) {	/* Internet */
			inet=1;
		} else {
			bputs(text[InvalidNetMailAddr]);
			free(qwkbuf);
			return; 
		} 
	}
	else {
		fidoaddr=atofaddr(&cfg,p+1); 	/* Get fido address */
		*p=0;					/* Chop off address */
	}


	if(!inet && !qnet &&		/* FidoNet */
		((!SYSOP && !(cfg.netmail_misc&NMAIL_ALLOW)) || !cfg.total_faddrs)) {
		bputs(text[NoNetMailAllowed]);
		free(qwkbuf);
		return; 
	}

	truncsp(to);			/* Truncate off space */

	if(!stricmp(to,"SBBS") && !SYSOP && qnet) {
		free(qwkbuf);
		return; 
	}

	l = QWK_BLOCK_LEN + kludge_hdrlen;		/* Start of message text */

	if(qnet || inet) {

		memset(&msg,0,sizeof(smbmsg_t));
		msg.hdr.version=smb_ver();
		msg.hdr.when_imported.time=time32(NULL);
		msg.hdr.when_imported.zone=sys_timezone(&cfg);

		if(fromhub || useron.rest&FLAG('Q')) {
			net=NET_QWK;
			smb_hfield(&msg,SENDERNETTYPE,sizeof(net),&net);
			if(!strncmp(qwkbuf+l,"@VIA:",5)) {
				sprintf(str,"%.128s",qwkbuf+l+5);
				cp=strchr(str,QWK_NEWLINE);
				if(cp) *cp=0;
				l+=strlen(str)+1;
				cp=str;
				while(*cp && *cp<=' ') cp++;
				sprintf(senderaddr,"%s/%s",sender_id,cp);
				strupr(senderaddr);
				smb_hfield(&msg,SENDERNETADDR,strlen(senderaddr),senderaddr); 
			}
			else {
				SAFECOPY(senderaddr, sender_id);
				strupr(senderaddr);
				smb_hfield(&msg,SENDERNETADDR,strlen(senderaddr),senderaddr); 
			}
			sprintf(sender,"%.25s",block+46);     /* From name */
		}
		else {	/* Not Networked */
			msg.hdr.when_written.zone=sys_timezone(&cfg);
			sprintf(str,"%u",useron.number);
			smb_hfield(&msg,SENDEREXT,strlen(str),str);
			SAFECOPY(sender,(qnet || cfg.inetmail_misc&NMAIL_ALIAS)
				? useron.alias : useron.name);
			}
		truncsp(sender);
		smb_hfield(&msg,SENDER,strlen(sender),sender);
		if(fromhub)
			msg.idx.from=0;
		else
			msg.idx.from=useron.number;
		if(!strncmp(qwkbuf+l,"@TZ:",4)) {
			sprintf(str,"%.128s",qwkbuf+l);
			cp=strchr(str,QWK_NEWLINE);
			if(cp) *cp=0;
			l+=strlen(str)+1;
			cp=str+4;
			while(*cp && *cp<=' ') cp++;
			msg.hdr.when_written.zone=(short)ahtoul(cp); 
		}
		else
			msg.hdr.when_written.zone=sys_timezone(&cfg);
		memset(&tm,0,sizeof(tm));
		tm.tm_mon=((qwkbuf[8]&0xf)*10)+(qwkbuf[9]&0xf);
		if(tm.tm_mon) tm.tm_mon--;	/* 0 based */
		tm.tm_mday=((qwkbuf[11]&0xf)*10)+(qwkbuf[12]&0xf);
		tm.tm_year=((qwkbuf[14]&0xf)*10)+(qwkbuf[15]&0xf);
		if(tm.tm_year<Y2K_2DIGIT_WINDOW)
			tm.tm_year+=100;
		tm.tm_hour=((qwkbuf[16]&0xf)*10)+(qwkbuf[17]&0xf);
		tm.tm_min=((qwkbuf[19]&0xf)*10)+(qwkbuf[20]&0xf);  /* From QWK time */
		tm.tm_sec=0;

		tm.tm_isdst=-1;	/* Do not adjust for DST */
		msg.hdr.when_written.time=mktime32(&tm);

		if(subject == NULL) {
			sprintf(str, "%.25s", block+71);
			subject = str;
		}
		smb_hfield_str(&msg, SUBJECT, subject);
	}

	if(qnet) {

		p++;
		addr=p;
		msg.idx.to=qwk_route(&cfg,addr,fulladdr, sizeof(fulladdr)-1);
		if(!fulladdr[0]) {		/* Invalid address, so BOUNCE it */
		/**
			errormsg(WHERE,ERR_CHK,addr,0);
			free(qwkbuf);
			smb_freemsgmem(msg);
			return;
		**/
			smb_hfield(&msg,SENDER,strlen(cfg.sys_id),cfg.sys_id);
			msg.idx.from=0;
			msg.idx.to=useron.number;
			SAFECOPY(to,sender);
			SAFECOPY(fulladdr,senderaddr);
			SAFEPRINTF(str,"BADADDR: %s",addr);
			smb_hfield(&msg,SUBJECT,strlen(str),str);
			net=NET_NONE;
			smb_hfield(&msg,SENDERNETTYPE,sizeof(net),&net);
		}
		/* This is required for fixsmb to be able to rebuild the index */
		SAFEPRINTF(str,"%u",msg.idx.to);
		smb_hfield_str(&msg,RECIPIENTEXT,str);

		smb_hfield(&msg,RECIPIENT,strlen(name),name);
		net=NET_QWK;
		smb_hfield(&msg,RECIPIENTNETTYPE,sizeof(net),&net);

		truncsp(fulladdr);
		if(fulladdr[0])
			smb_hfield(&msg,RECIPIENTNETADDR,strlen(fulladdr),fulladdr);

		bprintf(text[NetMailing],to,fulladdr,sender,cfg.sys_id); 
	}

	if(inet) {				/* Internet E-mail */

		if(cfg.inetmail_cost && !(useron.exempt&FLAG('S'))) {
			if(useron.cdt+useron.freecdt<cfg.inetmail_cost) {
				bputs(text[NotEnoughCredits]);
				free(qwkbuf);
				smb_freemsgmem(&msg);
				return; 
			}
			sprintf(str,text[NetMailCostContinueQ],cfg.inetmail_cost);
			if(noyes(str)) {
				free(qwkbuf);
				smb_freemsgmem(&msg);
				return; 
			} 
		}

		net=NET_INTERNET;
		smb_hfield(&msg,RECIPIENT,strlen(name),name);
		msg.idx.to=0;   /* Out-bound NetMail set to 0 */
		smb_hfield(&msg,RECIPIENTNETTYPE,sizeof(net),&net);
		smb_hfield(&msg,RECIPIENTNETADDR,strlen(to),to);

		bprintf(text[NetMailing],name,to
			,cfg.inetmail_misc&NMAIL_ALIAS ? useron.alias : useron.name
			,cfg.sys_inetaddr); 
	}

	if(qnet || inet) {

		bputs(text[WritingIndx]);

		if((i=smb_stack(&smb,SMB_STACK_PUSH))!=0) {
			errormsg(WHERE,ERR_OPEN,"MAIL",i);
			free(qwkbuf);
			smb_freemsgmem(&msg);
			return; 
		}
		sprintf(smb.file,"%smail", cfg.data_dir);
		smb.retry_time=cfg.smb_retry_time;
		smb.subnum=INVALID_SUB;
		if((i=smb_open(&smb))!=0) {
			smb_stack(&smb,SMB_STACK_POP);
			errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
			free(qwkbuf);
			smb_freemsgmem(&msg);
			return; 
		}
		add_msg_ids(&cfg, &smb, &msg, /* remsg: */NULL);

		if(smb_fgetlength(smb.shd_fp)<1L) {   /* Create it if it doesn't exist */
			smb.status.max_crcs=cfg.mail_maxcrcs;
			smb.status.max_msgs=0;
			smb.status.max_age=cfg.mail_maxage;
			smb.status.attr=SMB_EMAIL;
			if((i=smb_create(&smb))!=0) {
				smb_close(&smb);
				smb_stack(&smb,SMB_STACK_POP);
				errormsg(WHERE,ERR_CREATE,smb.file,i,smb.last_error);
				free(qwkbuf);
				smb_freemsgmem(&msg);
				return; 
			} 
		}

		length=n*256L;	// Extra big for CRLF xlat, was (n-1L)*256L (03/16/96)


		if(length&0xfff00000UL || !length) {
			smb_close(&smb);
			smb_stack(&smb,SMB_STACK_POP);
			sprintf(str,"REP msg (%ld)",n);
			errormsg(WHERE,ERR_LEN,str,length);
			free(qwkbuf);
			smb_freemsgmem(&msg);
			return; 
		}

		if((i=smb_open_da(&smb))!=0) {
			smb_close(&smb);
			smb_stack(&smb,SMB_STACK_POP);
			errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
			free(qwkbuf);
			smb_freemsgmem(&msg);
			return; 
		}
		if(cfg.sys_misc&SM_FASTMAIL)
			offset=smb_fallocdat(&smb,length,1);
		else
			offset=smb_allocdat(&smb,length,1);
		smb_close_da(&smb);

		smb_fseek(smb.sdt_fp,offset,SEEK_SET);
		xlat=XLAT_NONE;
		smb_fwrite(&smb,&xlat,2,smb.sdt_fp);
		m=2;
		for(;l<n*QWK_BLOCK_LEN && m<length;l++) {
			if(qwkbuf[l]==0 || qwkbuf[l]==LF)
				continue;
			if(qwkbuf[l]==QWK_NEWLINE) {
				if(m <= 2)	/* Ignore blank lines at top of message */
					continue;
				smb_fwrite(&smb,crlf,2,smb.sdt_fp);
				m+=2;
				continue; 
			}
			smb_fputc(qwkbuf[l],smb.sdt_fp);
			m++; 
		}

		for(ch=0;m<length;m++)			/* Pad out with NULLs */
			smb_fputc(ch,smb.sdt_fp);
		smb_fflush(smb.sdt_fp);

		msg.hdr.offset=offset;

		smb_dfield(&msg,TEXT_BODY,length);

		i=smb_addmsghdr(&smb,&msg,smb_storage_mode(&cfg, &smb));
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);

		smb_freemsgmem(&msg);
		if(i!=SMB_SUCCESS) {
			errormsg(WHERE,ERR_WRITE,smb.file,i,smb.last_error); 
			smb_freemsgdat(&smb,offset,length,1);
		}
		else {		/* Successful */
			if(inet) {
				if(cfg.inetmail_sem[0]) 	 /* update semaphore file */
					ftouch(cmdstr(cfg.inetmail_sem,nulstr,nulstr,NULL));
				if(!(useron.exempt&FLAG('S')))
					subtract_cdt(&cfg,&useron,cfg.inetmail_cost); 
			}

			useron.emails++;
			logon_emails++;
			putuserrec(&cfg,useron.number,U_EMAILS,5,ultoa(useron.emails,tmp,10)); 
			useron.etoday++;
			putuserrec(&cfg,useron.number,U_ETODAY,5,ultoa(useron.etoday,tmp,10));

			safe_snprintf(str,sizeof(str), "%s (%s) sent %s NetMail to %s (%s) via QWK"
				,sender, sender_id
				,qnet ? "QWK":"Internet",name,qnet ? fulladdr : to);
			logline("EN",str); 
		}

		free((char *)qwkbuf);
		return; 
	}


	/****************************** FidoNet **********************************/

	if(!fidoaddr.zone || !cfg.netmail_dir[0]) {  // No fido netmail allowed
		bputs(text[InvalidNetMailAddr]);
		free(qwkbuf);
		return; 
	}

	memset(&hdr,0,sizeof(hdr));   /* Initialize header to null */

	if(fromhub || useron.rest&FLAG('Q')) {
		sprintf(str,"%.25s",block+46);              /* From */
		truncsp(str);
		sprintf(tmp,"@%s",sender_id);
		strupr(tmp);
		strcat(str,tmp); 
	}
	else
		SAFECOPY(str,cfg.netmail_misc&NMAIL_ALIAS ? useron.alias : useron.name);
	SAFECOPY(hdr.from,str);

	SAFECOPY(hdr.to,to);

	/* Look-up in nodelist? */

	if(cfg.netmail_cost && !(useron.exempt&FLAG('S'))) {
		if(useron.cdt+useron.freecdt<cfg.netmail_cost) {
			bputs(text[NotEnoughCredits]);
			free(qwkbuf);
			return; 
		}
		sprintf(str,text[NetMailCostContinueQ],cfg.netmail_cost);
		if(noyes(str)) {
			free(qwkbuf);
			return; 
		} 
	}

	hdr.destzone	=fidoaddr.zone;
	hdr.destnet 	=fidoaddr.net;
	hdr.destnode	=fidoaddr.node;
	hdr.destpoint	=fidoaddr.point;

	for(i=0;i<cfg.total_faddrs;i++)
		if(fidoaddr.zone==cfg.faddr[i].zone && fidoaddr.net==cfg.faddr[i].net)
			break;
	if(i==cfg.total_faddrs) {
		for(i=0;i<cfg.total_faddrs;i++)
			if(fidoaddr.zone==cfg.faddr[i].zone)
				break; 
	}
	if(i==cfg.total_faddrs)
		i=0;
	hdr.origzone	=cfg.faddr[i].zone;
	hdr.orignet 	=cfg.faddr[i].net;
	hdr.orignode	=cfg.faddr[i].node;
	hdr.origpoint   =cfg.faddr[i].point;

	smb_faddrtoa(&cfg.faddr[i],str);
	bprintf(text[NetMailing],hdr.to,smb_faddrtoa(&fidoaddr,tmp),hdr.from,str);
	tm.tm_mon=((qwkbuf[8]&0xf)*10)+(qwkbuf[9]&0xf);
	if (tm.tm_mon) tm.tm_mon--;
	tm.tm_mday=((qwkbuf[11]&0xf)*10)+(qwkbuf[12]&0xf);
	tm.tm_year=((qwkbuf[14]&0xf)*10)+(qwkbuf[15]&0xf)+1900;
	tm.tm_hour=((qwkbuf[16]&0xf)*10)+(qwkbuf[17]&0xf);
	tm.tm_min=((qwkbuf[19]&0xf)*10)+(qwkbuf[20]&0xf);		/* From QWK time */
	tm.tm_sec=0;
	sprintf(hdr.time,"%02u %3.3s %02u  %02u:%02u:%02u"          /* To FidoNet */
		,tm.tm_mday,mon[tm.tm_mon],TM_YEAR(tm.tm_year)
		,tm.tm_hour,tm.tm_min,tm.tm_sec);
	hdr.attr=(FIDO_LOCAL|FIDO_PRIVATE);

	if(cfg.netmail_misc&NMAIL_CRASH) hdr.attr|=FIDO_CRASH;
	if(cfg.netmail_misc&NMAIL_HOLD)  hdr.attr|=FIDO_HOLD;
	if(cfg.netmail_misc&NMAIL_KILL)  hdr.attr|=FIDO_KILLSENT;

	sprintf(str,"%.25s",block+71);      /* Title */
	truncsp(str);
	p=str;
	if((SYSOP || useron.exempt&FLAG('F'))
		&& !strnicmp(p,"CR:",3)) {     /* Crash over-ride by sysop */
		p+=3;               /* skip CR: */
		if(*p==' ') p++;     /* skip extra space if it exists */
		hdr.attr|=FIDO_CRASH; 
	}

	if((SYSOP || useron.exempt&FLAG('F'))
		&& !strnicmp(p,"FR:",3)) {     /* File request */
		p+=3;               /* skip FR: */
		if(*p==' ') p++;
		hdr.attr|=FIDO_FREQ; 
	}

	if((SYSOP || useron.exempt&FLAG('F'))
		&& !strnicmp(p,"RR:",3)) {     /* Return receipt request */
		p+=3;               /* skip RR: */
		if(*p==' ') p++;
		hdr.attr|=FIDO_RRREQ; 
	}

	if((SYSOP || useron.exempt&FLAG('F'))
		&& !strnicmp(p,"FA:",3)) {     /* File attachment */
		p+=3;				/* skip FA: */
		if(*p==' ') p++;
		hdr.attr|=FIDO_FILE; 
	}

	if(subject != NULL)
		SAFECOPY(hdr.subj, subject);
	else
		SAFECOPY(hdr.subj, p);

	md(cfg.netmail_dir);
	for(i=1;i;i++) {
		sprintf(str,"%s%u.msg", cfg.netmail_dir,i);
		if(!fexistcase(str))
			break; 
	}
	if(!i) {
		bputs(text[TooManyEmailsToday]);
		return; 
	}
	if((fido=nopen(str,O_WRONLY|O_CREAT|O_EXCL))==-1) {
		free(qwkbuf);
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_EXCL);
		return; 
	}
	write(fido,&hdr,sizeof(hdr));

	pt_zone_kludge(hdr,fido);

	if(cfg.netmail_misc&NMAIL_DIRECT) {
		sprintf(str,"\1FLAGS DIR\r\n");
		write(fido,str,strlen(str)); 
	}

	l = QWK_BLOCK_LEN + kludge_hdrlen;

	length=n*QWK_BLOCK_LEN;
	while(l<length) {
		if(qwkbuf[l]==CTRL_A) {   /* Ctrl-A, so skip it and the next char */
			l++;
			if(l>=length)
				break;
			if((ch=ctrl_a_to_ascii_char(qwkbuf[l])) != 0)
				write(fido,&ch,1);
		}
		else if(qwkbuf[l]!=LF) {
			if(qwkbuf[l]==QWK_NEWLINE) /* QWK cr/lf char converted to hard CR */
				qwkbuf[l]=CR;
			write(fido,(char *)qwkbuf+l,1); 
		}
		l++;
	}
	l=0;
	write(fido,&l,1);	/* Null terminator */
	close(fido);
	free((char *)qwkbuf);
	if(cfg.netmail_sem[0])		/* update semaphore file */
		ftouch(cmdstr(cfg.netmail_sem,nulstr,nulstr,NULL));
	if(!(useron.exempt&FLAG('S')))
		subtract_cdt(&cfg,&useron,cfg.netmail_cost);

	useron.emails++;
	logon_emails++;
	putuserrec(&cfg,useron.number,U_EMAILS,5,ultoa(useron.emails,tmp,10)); 
	useron.etoday++;
	putuserrec(&cfg,useron.number,U_ETODAY,5,ultoa(useron.etoday,tmp,10));

	sprintf(str,"%s sent NetMail to %s @%s via QWK"
		,sender_id
		,hdr.to,smb_faddrtoa(&fidoaddr,tmp));
	logline("EN",str);
}

/****************************************************************************/
/* Returns the FidoNet address kept in str as ASCII.                        */
/****************************************************************************/
faddr_t atofaddr(scfg_t* cfg, char *str)
{
	char *p;
	faddr_t addr;

	addr.zone=addr.net=addr.node=addr.point=0;
	if((p=strchr(str,':'))!=NULL) {
		addr.zone=atoi(str);
		addr.net=atoi(p+1); 
	}
	else {
		if(cfg->total_faddrs)
			addr.zone=cfg->faddr[0].zone;
		else
			addr.zone=1;
		addr.net=atoi(str); 
	}
	if(!addr.zone)              /* no such thing as zone 0 */
		addr.zone=1;
	if((p=strchr(str,'/'))!=NULL)
		addr.node=atoi(p+1);
	else {
		if(cfg->total_faddrs)
			addr.net=cfg->faddr[0].net;
		else
			addr.net=1;
		addr.node=atoi(str); 
	}
	if((p=strchr(str,'.'))!=NULL)
		addr.point=atoi(p+1);
	return(addr);
}
