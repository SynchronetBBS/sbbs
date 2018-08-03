/* Synchronet QWK packet-related functions */

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

#include "sbbs.h"
#include "qwk.h"

/****************************************************************************/
/* Converts a long to an msbin real number. required for QWK NDX file		*/
/****************************************************************************/
float ltomsbin(long val)
{
	union {
		uchar	uc[10];
		ushort	ui[5];
		ulong	ul[2];
		float	f[2];
		double	d[1];
	} t;
	int   sign, exp;	/* sign and exponent */

	t.f[0]=(float)val;
	sign=t.uc[3]/0x80;
	exp=((t.ui[1]>>7)-0x7f+0x81)&0xff;
	t.ui[1]=(t.ui[1]&0x7f)|(sign<<7)|(exp<<8);
	return(t.f[0]);
}

bool route_circ(char *via, char *id)
{
	char str[256],*p,*sp;

	if(via==NULL || id==NULL)
		return(false);

	SAFECOPY(str,via);
	p=str;
	SKIP_WHITESPACE(p);
	while(*p) {
		sp=strchr(p,'/');
		if(sp) *sp=0;
		if(!stricmp(p,id))
			return(true);
		if(!sp)
			break;
		p=sp+1;
	}
	return(false);
}

extern "C" int DLLCALL qwk_route(scfg_t* cfg, const char *inaddr, char *fulladdr, size_t maxlen)
{
	char node[64],str[256],path[MAX_PATH+1],*p;
	int file,i;
	FILE *stream;

	fulladdr[0]=0;
	SAFECOPY(str,inaddr);
	p=strrchr(str,'/');
	if(p) p++;
	else p=str;
	SAFECOPY(node,p);                 /* node = destination node */
	truncsp(node);

	for(i=0;i<cfg->total_qhubs;i++)			/* Check if destination is our hub */
		if(!stricmp(cfg->qhub[i]->id,node))
			break;
	if(i<cfg->total_qhubs) {
		strncpy(fulladdr,node,maxlen);
		return(0);
	}

	i=matchuser(cfg,node,FALSE);			/* Check if destination is a node */
	if(i) {
		getuserrec(cfg,i,U_REST,8,str);
		if(ahtoul(str)&FLAG('Q')) {
			strncpy(fulladdr,node,maxlen);
			return(i);
		}

	}

	SAFECOPY(node,inaddr);            /* node = next hop */
	p=strchr(node,'/');
	if(p) *p=0;
	truncsp(node);

	if(strchr(inaddr,'/')) {                /* Multiple hops */

		for(i=0;i<cfg->total_qhubs;i++)			/* Check if next hop is our hub */
			if(!stricmp(cfg->qhub[i]->id,node))
				break;
		if(i<cfg->total_qhubs) {
			strncpy(fulladdr,inaddr,maxlen);
			return(0);
		}

		i=matchuser(cfg,node,FALSE);			/* Check if next hop is a node */
		if(i) {
			getuserrec(cfg,i,U_REST,8,str);
			if(ahtoul(str)&FLAG('Q')) {
				strncpy(fulladdr,inaddr,maxlen);
				return(i);
			}
		}
	}

	p=strchr(node,' ');
	if(p) *p=0;

	if(strlen(node) > LEN_QWKID)
		return 0;

	SAFEPRINTF(path,"%sqnet/route.dat",cfg->data_dir);
	if((stream=fnopen(&file,path,O_RDONLY))==NULL)
		return(0);

	strcat(node,":");
	fulladdr[0]=0;
	while(!feof(stream)) {
		if(!fgets(str,sizeof(str),stream))
			break;
		if(!strnicmp(str+9,node,strlen(node))) {
			truncsp(str);
			safe_snprintf(fulladdr,maxlen,"%s/%s",str+9+strlen(node),inaddr);
			break;
		}

	}

	fclose(stream);
	if(!fulladdr[0])			/* First hop not found in ROUTE.DAT */
		return(0);

	SAFECOPY(node, fulladdr);
	p=strchr(node,'/');
	if(p) *p=0;
	truncsp(node);

	for(i=0;i<cfg->total_qhubs;i++)				/* Check if first hop is our hub */
		if(!stricmp(cfg->qhub[i]->id,node))
			break;
	if(i<cfg->total_qhubs)
		return(0);

	i=matchuser(cfg,node,FALSE);				/* Check if first hop is a node */
	if(i) {
		getuserrec(cfg,i,U_REST,8,str);
		if(ahtoul(str)&FLAG('Q'))
			return(i);
	}
	fulladdr[0]=0;
	return(0);
}

/* Via is in format: NODE/NODE/... */
void sbbs_t::update_qwkroute(char *via)
{
	char str[256],*p,*tp,node[9];
	uint	i;
	int		file;
	time_t	t;
	FILE *	stream;

	if(via==NULL) {
		if(!total_qwknodes)
			return;
		sprintf(str,"%sqnet/route.dat",cfg.data_dir);
		if((stream=fnopen(&file,str,O_WRONLY|O_CREAT|O_TRUNC))!=NULL) {
			t=time(NULL);
			t-=(90L*24L*60L*60L);
			for(i=0;i<total_qwknodes;i++)
				if(qwknode[i].time>t)
					fprintf(stream,"%s %s:%s\r\n"
						,unixtodstr(&cfg,(time32_t)qwknode[i].time,str),qwknode[i].id,qwknode[i].path);
			fclose(stream);
		}
		else
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
		FREE_AND_NULL(qwknode);
		total_qwknodes=0;
		return;
	}

	if(!total_qwknodes) {
		sprintf(str,"%sqnet/route.dat",cfg.data_dir);
		if((stream=fnopen(&file,str,O_RDONLY))!=NULL) {
			while(!feof(stream)) {
				if(!fgets(str,sizeof(str),stream))
					break;
				truncsp(str);
				t=dstrtounix(&cfg,str);
				p=strchr(str,':');
				if(!p) continue;
				*p=0;
				sprintf(node,"%.8s",str+9);
				tp=strchr(node,' '); 		/* change "node bbs:" to "node:" */
				if(tp) *tp=0;
				for(i=0;i<total_qwknodes;i++)
					if(!stricmp(qwknode[i].id,node))
						break;
				if(i<total_qwknodes && qwknode[i].time>t)
					continue;
				if(i==total_qwknodes) {
					if((qwknode=(struct qwknode*)realloc(qwknode,sizeof(struct qwknode)*(i+1)))==NULL) {
						errormsg(WHERE,ERR_ALLOC,str,sizeof(struct qwknode)*(i+1));
						break;
					}
					total_qwknodes++;
				}
				strcpy(qwknode[i].id,node);
				p++;
				while(*p && *p<=' ') p++;
				sprintf(qwknode[i].path,"%.127s",p);
				qwknode[i].time=t;
			}
			fclose(stream);
		}

	}

	strupr(via);
	p=strchr(via,'/');   /* Skip uplink */

	while(p && *p) {
		p++;
		sprintf(node,"%.8s",p);
		tp=strchr(node,'/');
		if(tp) *tp=0;
		tp=strchr(node,' '); 		/* no spaces allowed */
		if(tp) *tp=0;
		truncsp(node);
		for(i=0;i<total_qwknodes;i++)
			if(!stricmp(qwknode[i].id,node))
				break;
		if(i==total_qwknodes) {		/* Not in list */
			if((qwknode=(struct qwknode*)realloc(qwknode,sizeof(struct qwknode)*(total_qwknodes+1)))==NULL) {
				errormsg(WHERE,ERR_ALLOC,str,sizeof(struct qwknode)*(total_qwknodes+1));
				break;
			}
			total_qwknodes++;
		}
		sprintf(qwknode[i].id,"%.8s",node);
		sprintf(qwknode[i].path,"%.*s",(int)((p-1)-via),via);
		qwknode[i].time=time(NULL);
		p=strchr(p,'/');
	}
}

/****************************************************************************/
/* Successful download of QWK packet										*/
/****************************************************************************/
void sbbs_t::qwk_success(ulong msgcnt, char bi, char prepack)
{
	char	str[MAX_PATH+1];
	int 	i;
	long	deleted=0;
	uint32_t	u,msgs;
	mail_t	*mail;
	smbmsg_t msg;

	if(useron.rest&FLAG('Q')) {	// Was if(!prepack) only
		char id[LEN_QWKID+1];
		SAFECOPY(id,useron.alias);
		strlwr(id);
		sprintf(str,"%sqnet/%s.out/",cfg.data_dir,id);
		long result = delfiles(str,ALLFILES);
		if(result < 0)
			errormsg(WHERE, ERR_REMOVE, str, result);
	}

	if(!prepack) {
		SAFECOPY(str, "downloaded QWK packet");
		logline("D-",str);
		posts_read+=msgcnt;

		sprintf(str,"%sfile/%04u.qwk",cfg.data_dir,useron.number);
		if(fexistcase(str))
			remove(str);

		if(!bi) {
			batch_download(-1);
			delfiles(cfg.temp_dir,ALLFILES);
		}

	}

	if(useron.rest&FLAG('Q'))
		useron.qwk|=(QWK_EMAIL|QWK_ALLMAIL|QWK_DELMAIL);
	if(useron.qwk&(QWK_EMAIL|QWK_ALLMAIL)) {
		sprintf(smb.file,"%smail",cfg.data_dir);
		smb.retry_time=cfg.smb_retry_time;
		smb.subnum=INVALID_SUB;
		if((i=smb_open(&smb))!=0) {
			errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
			return;
		}

		mail=loadmail(&smb,&msgs,useron.number,0
			,useron.qwk&QWK_ALLMAIL ? 0 : LM_UNREAD);

		if((i=smb_locksmbhdr(&smb))!=0) {			  /* Lock the base, so nobody */
			if(msgs)
				free(mail);
			smb_close(&smb);
			errormsg(WHERE,ERR_LOCK,smb.file,i,smb.last_error);	/* messes with the index */
			return;
		}

		if((i=smb_getstatus(&smb))!=0) {
			if(msgs)
				free(mail);
			smb_close(&smb);
			errormsg(WHERE,ERR_READ,smb.file,i,smb.last_error);
			return;
		}

		/* Mark as READ and DELETE */
		for(u=0;u<msgs;u++) {
			if(mail[u].number>qwkmail_last)
				continue;
			memset(&msg,0,sizeof(msg));
			/* !IMPORTANT: search by number (do not initialize msg.idx.offset) */
			if(loadmsg(&msg,mail[u].number) < 1)
				continue;
			if(!(msg.hdr.attr&MSG_READ)) {
				if(thisnode.status==NODE_INUSE)
					telluser(&msg);
				msg.hdr.attr|=MSG_READ;
				msg.idx.attr=msg.hdr.attr;
				smb_putmsg(&smb,&msg);
			}
			if(!(msg.hdr.attr&MSG_PERMANENT)
				&& (((msg.hdr.attr&MSG_KILLREAD) && (msg.hdr.attr&MSG_READ))
				|| (useron.qwk&QWK_DELMAIL))) {
				msg.hdr.attr|=MSG_DELETE;
				msg.idx.attr=msg.hdr.attr;
				if((i=smb_putmsg(&smb,&msg))!=0)
					errormsg(WHERE,ERR_WRITE,smb.file,i,smb.last_error);
				else
					deleted++;
			}
			smb_freemsgmem(&msg);
			smb_unlockmsghdr(&smb,&msg);
		}

		if(deleted && cfg.sys_misc&SM_DELEMAIL)
			delmail(useron.number,MAIL_YOUR);
		smb_close(&smb);
		if(msgs)
			free(mail);
	}
}

/****************************************************************************/
/* QWK mail packet section													*/
/****************************************************************************/
void sbbs_t::qwk_sec()
{
	char	str[256],tmp2[256],ch,bi=0;
	char 	tmp[512];
	int		error;
	int 	s;
	uint	i,k;
	ulong	l;
	ulong	msgcnt;
	ulong	*sav_ptr;
	file_t	fd;

	memset(&fd,0,sizeof(fd));
	getusrdirs();
	fd.dir=cfg.total_dirs;
	if((sav_ptr=(ulong *)malloc(sizeof(ulong)*cfg.total_subs))==NULL) {
		errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(ulong)*cfg.total_subs);
		return;
	}
	for(i=0;i<cfg.total_subs;i++)
		sav_ptr[i]=subscan[i].ptr;
	for(i=0;i<cfg.total_prots;i++)
		if(cfg.prot[i]->bicmd[0] && chk_ar(cfg.prot[i]->ar,&useron,&client))
			bi++;				/* number of bidirectional protocols configured */
	if(useron.rest&FLAG('Q'))
		getusrsubs();
	delfiles(cfg.temp_dir,ALLFILES);
	while(online) {
		if((useron.misc&(WIP|RIP|HTML) || !(useron.misc&EXPERT))
			&& (useron.logons<2 || !(useron.rest&FLAG('Q'))))
			menu("qwk");
		action=NODE_TQWK;
		ASYNC;
		bputs(text[QWKPrompt]);
		sprintf(str,"?UDCSP\r%c",text[YNQP][2]);
		if(bi)
			strcat(str,"B");
		ch=(char)getkeys(str,0);
		if(ch>' ')
			logch(ch,0);
		if(sys_status&SS_ABORT || ch==text[YNQP][2] || ch==CR || !online)
			break;
		if(ch=='?') {
			if((useron.misc&(WIP|RIP|HTML) || !(useron.misc&EXPERT))
				&& !(useron.rest&FLAG('Q')))
				continue;
			menu("qwk");
			continue;
		}
		if(ch=='S') {
			new_scan_cfg(SUB_CFG_NSCAN);
			delfiles(cfg.temp_dir,ALLFILES);
			continue;
		}
		if(ch=='P') {
			new_scan_ptr_cfg();
			for(i=0;i<cfg.total_subs;i++)
				sav_ptr[i]=subscan[i].ptr;
			delfiles(cfg.temp_dir,ALLFILES);
			continue;
		}
		if(ch=='C') {
			while(online) {
				CLS;
				bprintf(text[QWKSettingsHdr],useron.alias,useron.number);
				bprintf(text[QWKSettingsCtrlA]
					,useron.qwk&QWK_EXPCTLA
					? "Expand to ANSI" : useron.qwk&QWK_RETCTLA ? "Leave in"
					: "Strip");
				bprintf(text[QWKSettingsArchive],useron.tmpext);
				bprintf(text[QWKSettingsEmail]
					,useron.qwk&QWK_EMAIL ? "Un-read Only"
					: useron.qwk&QWK_ALLMAIL ? text[Yes] : text[No]);
				if(useron.qwk&(QWK_ALLMAIL|QWK_EMAIL)) {
					bprintf(text[QWKSettingsAttach]
						,useron.qwk&QWK_ATTACH ? text[Yes] : text[No]);
					bprintf(text[QWKSettingsDeleteEmail]
						,useron.qwk&QWK_DELMAIL ? text[Yes]:text[No]);
				}
				bprintf(text[QWKSettingsNewFilesList]
					,useron.qwk&QWK_FILES ? text[Yes]:text[No]);
				bprintf(text[QWKSettingsIndex]
					,useron.qwk&QWK_NOINDEX ? text[No]:text[Yes]);
				bprintf(text[QWKSettingsControl]
					,useron.qwk&QWK_NOCTRL ? text[No]:text[Yes]);
				bprintf(text[QWKSettingsVoting]
					,useron.qwk&QWK_VOTING ? text[Yes]:text[No]);
				bprintf(text[QWKSettingsHeaders]
					,useron.qwk&QWK_HEADERS ? text[Yes]:text[No]);
				bprintf(text[QWKSettingsBySelf]
					,useron.qwk&QWK_BYSELF ? text[Yes]:text[No]);
				bprintf(text[QWKSettingsTimeZone]
					,useron.qwk&QWK_TZ ? text[Yes]:text[No]);
				bprintf(text[QWKSettingsVIA]
					,useron.qwk&QWK_VIA ? text[Yes]:text[No]);
				bprintf(text[QWKSettingsMsgID]
					,useron.qwk&QWK_MSGID ? text[Yes]:text[No]);
				bprintf(text[QWKSettingsExtended]
					,useron.qwk&QWK_EXT ? text[Yes]:text[No]);
				bputs(text[QWKSettingsWhich]);
				ch=(char)getkeys("AEDFHIOPQTYMNCXZV",0);
				if(sys_status&SS_ABORT || !ch || ch=='Q' || !online)
					break;
				switch(ch) {
					case 'A':
						if(!(useron.qwk&(QWK_EXPCTLA|QWK_RETCTLA)))
							useron.qwk|=QWK_EXPCTLA;
						else if(useron.qwk&QWK_EXPCTLA) {
							useron.qwk&=~QWK_EXPCTLA;
							useron.qwk|=QWK_RETCTLA;
						}
						else
							useron.qwk&=~(QWK_EXPCTLA|QWK_RETCTLA);
						break;
					case 'T':
						for(i=0;i<cfg.total_fcomps;i++)
							uselect(1,i,text[ArchiveTypeHeading],cfg.fcomp[i]->ext,cfg.fcomp[i]->ar);
						s=uselect(0,0,0,0,0);
						if(s>=0) {
							strcpy(useron.tmpext,cfg.fcomp[s]->ext);
							putuserrec(&cfg,useron.number,U_TMPEXT,3,useron.tmpext);
						}
						break;
					case 'E':
						if(!(useron.qwk&(QWK_EMAIL|QWK_ALLMAIL)))
							useron.qwk|=QWK_EMAIL;
						else if(useron.qwk&QWK_EMAIL) {
							useron.qwk&=~QWK_EMAIL;
							useron.qwk|=QWK_ALLMAIL;
						}
						else
							useron.qwk&=~(QWK_EMAIL|QWK_ALLMAIL);
						break;
					case 'H':
						useron.qwk^=QWK_HEADERS;
						break;
					case 'I':
						useron.qwk^=QWK_ATTACH;
						break;
					case 'D':
						useron.qwk^=QWK_DELMAIL;
						break;
					case 'F':
						useron.qwk^=QWK_FILES;
						break;
					case 'N':   /* NO IDX files */
						useron.qwk^=QWK_NOINDEX;
						break;
					case 'C':
						useron.qwk^=QWK_NOCTRL;
						break;
					case 'Z':
						useron.qwk^=QWK_TZ;
						break;
					case 'V':
						useron.qwk^=QWK_VOTING;
						break;
					case 'P':
						useron.qwk^=QWK_VIA;
						break;
					case 'M':
						useron.qwk^=QWK_MSGID;
						break;
					case 'Y':   /* Yourself */
						useron.qwk^=QWK_BYSELF;
						break;
					case 'X':	/* QWKE */
						useron.qwk^=QWK_EXT;
						break;
				}
				putuserrec(&cfg,useron.number,U_QWK,8,ultoa(useron.qwk,str,16));
			}
			delfiles(cfg.temp_dir,ALLFILES);
			continue;
		}


		if(ch=='B') {   /* Bidirectional QWK and REP packet transfer */
			sprintf(str,"%s%s.qwk",cfg.temp_dir,cfg.sys_id);
			if(!fexistcase(str) && !pack_qwk(str,&msgcnt,0)) {
				for(i=0;i<cfg.total_subs;i++)
					subscan[i].ptr=sav_ptr[i];
				remove(str);
				last_ns_time=ns_time;
				continue;
			}
			bprintf(text[UploadingREP],cfg.sys_id);
			xfer_prot_menu(XFER_BIDIR);
			mnemonics(text[ProtocolOrQuit]);
			sprintf(tmp2,"%c",text[YNQP][2]);
			for(i=0;i<cfg.total_prots;i++)
				if(cfg.prot[i]->bicmd[0] && chk_ar(cfg.prot[i]->ar,&useron,&client)) {
					sprintf(tmp,"%c",cfg.prot[i]->mnemonic);
					strcat(tmp2,tmp);
				}
			ch=(char)getkeys(tmp2,0);
			if(ch==text[YNQP][2] || sys_status&SS_ABORT || !online) {
				for(i=0;i<cfg.total_subs;i++)
					subscan[i].ptr=sav_ptr[i];	/* re-load saved pointers */
				last_ns_time=ns_time;
				continue;
			}
			for(i=0;i<cfg.total_prots;i++)
				if(cfg.prot[i]->bicmd[0] && cfg.prot[i]->mnemonic==ch
					&& chk_ar(cfg.prot[i]->ar,&useron,&client))
					break;
			if(i<cfg.total_prots) {
				batup_total=1;
				batup_dir[0]=cfg.total_dirs;
				sprintf(batup_name[0],"%s.rep",cfg.sys_id);
				batdn_total=1;
				batdn_dir[0]=cfg.total_dirs;
				sprintf(batdn_name[0],"%s.qwk",cfg.sys_id);
				if(!create_batchdn_lst((cfg.prot[i]->misc&PROT_NATIVE) ? true:false)
					|| !create_batchup_lst()
					|| !create_bimodem_pth()) {
					batup_total=batdn_total=0;
					continue;
				}
				sprintf(str,"%s%s.qwk",cfg.temp_dir,cfg.sys_id);
				sprintf(tmp2,"%s.qwk",cfg.sys_id);
				padfname(tmp2,fd.name);
				sprintf(str,"%sBATCHDN.LST",cfg.node_dir);
				sprintf(tmp2,"%sBATCHUP.LST",cfg.node_dir);
				error=protocol(cfg.prot[i],XFER_BIDIR,str,tmp2,true);
				batdn_total=batup_total=0;
				if(!checkprotresult(cfg.prot[i],error,&fd)) {
					last_ns_time=ns_time;
					for(i=0;i<cfg.total_subs;i++)
						subscan[i].ptr=sav_ptr[i]; /* re-load saved pointers */
				}
				else {
					qwk_success(msgcnt,1,0);
					for(i=0;i<cfg.total_subs;i++)
						sav_ptr[i]=subscan[i].ptr;
				}
				sprintf(str,"%s%s.qwk",cfg.temp_dir,cfg.sys_id);
				if(fexistcase(str))
					remove(str);
				unpack_rep();
				delfiles(cfg.temp_dir,ALLFILES);
				//autohangup();
				}
			else {
				last_ns_time=ns_time;
				for(i=0;i<cfg.total_subs;i++)
					subscan[i].ptr=sav_ptr[i];
			}

		}

		else if(ch=='D') {   /* Download QWK Packet of new messages */
			sprintf(str,"%s%s.qwk",cfg.temp_dir,cfg.sys_id);
			if(!fexistcase(str) && !pack_qwk(str,&msgcnt,0)) {
				for(i=0;i<cfg.total_subs;i++)
					subscan[i].ptr=sav_ptr[i];
				last_ns_time=ns_time;
				remove(str);
				continue;
			}

			l=(long)flength(str);
			bprintf(text[FiFilename],getfname(str));
			bprintf(text[FiFileSize],ultoac(l,tmp)
				, byte_estimate_to_str(l, tmp2, sizeof(tmp), /* units: */1024, /* precision: */1));
			if(l>0L && cur_cps)
				i=l/(ulong)cur_cps;
			else
				i=0;
			bprintf(text[FiTransferTime],sectostr(i,tmp));
			CRLF;
			if(!(useron.exempt&FLAG('T')) && i>timeleft) {
				bputs(text[NotEnoughTimeToDl]);
				break;
			}
			/***************/
			/* Send Packet */
			/***************/
			xfer_prot_menu(XFER_DOWNLOAD);
			mnemonics(text[ProtocolOrQuit]);
			sprintf(tmp2,"%c",text[YNQP][2]);
			for(i=0;i<cfg.total_prots;i++)
				if(cfg.prot[i]->dlcmd[0] && chk_ar(cfg.prot[i]->ar,&useron,&client)) {
					sprintf(tmp,"%c",cfg.prot[i]->mnemonic);
					strcat(tmp2,tmp);
				}
			ungetkey(useron.prot);
			ch=(char)getkeys(tmp2,0);
			if(ch==text[YNQP][2] || sys_status&SS_ABORT || !online) {
				for(i=0;i<cfg.total_subs;i++)
					subscan[i].ptr=sav_ptr[i];   /* re-load saved pointers */
				last_ns_time=ns_time;
				continue;
			}
			for(i=0;i<cfg.total_prots;i++)
				if(cfg.prot[i]->dlcmd[0] && cfg.prot[i]->mnemonic==ch
					&& chk_ar(cfg.prot[i]->ar,&useron,&client))
					break;
			if(i<cfg.total_prots) {
				sprintf(str,"%s%s.qwk",cfg.temp_dir,cfg.sys_id);
				sprintf(tmp2,"%s.qwk",cfg.sys_id);
				padfname(tmp2,fd.name);
				error=protocol(cfg.prot[i],XFER_DOWNLOAD,str,nulstr,false);
				if(!checkprotresult(cfg.prot[i],error,&fd)) {
					last_ns_time=ns_time;
					for(i=0;i<cfg.total_subs;i++)
						subscan[i].ptr=sav_ptr[i]; /* re-load saved pointers */
				} else {
					qwk_success(msgcnt,0,0);
					for(i=0;i<cfg.total_subs;i++)
						sav_ptr[i]=subscan[i].ptr;
				}
				autohangup();
			}
			else {	 /* if not valid protocol (hungup?) */
				for(i=0;i<cfg.total_subs;i++)
					subscan[i].ptr=sav_ptr[i];
				last_ns_time=ns_time;
			}
		}

		else if(ch=='U') { /* Upload REP Packet */
	/*
			if(useron.rest&FLAG('Q') && useron.rest&FLAG('P')) {
				bputs(text[R_Post]);
				continue;
				}
	*/

			delfiles(cfg.temp_dir,ALLFILES);
			bprintf(text[UploadingREP],cfg.sys_id);
			for(k=0;k<cfg.total_fextrs;k++)
				if(!stricmp(cfg.fextr[k]->ext,useron.tmpext)
					&& chk_ar(cfg.fextr[k]->ar,&useron,&client))
					break;
			if(k>=cfg.total_fextrs) {
				bputs(text[QWKExtractionFailed]);
				lprintf(LOG_ERR, "Couldn't extract REP packet - configuration error");
				continue;
			}

			/******************/
			/* Receive Packet */
			/******************/
			xfer_prot_menu(XFER_UPLOAD);
			mnemonics(text[ProtocolOrQuit]);
			sprintf(tmp2,"%c",text[YNQP][2]);
			for(i=0;i<cfg.total_prots;i++)
				if(cfg.prot[i]->ulcmd[0] && chk_ar(cfg.prot[i]->ar,&useron,&client)) {
					sprintf(tmp,"%c",cfg.prot[i]->mnemonic);
					strcat(tmp2,tmp);
				}
			ch=(char)getkeys(tmp2,0);
			if(ch==text[YNQP][2] || sys_status&SS_ABORT || !online)
				continue;
			for(i=0;i<cfg.total_prots;i++)
				if(cfg.prot[i]->ulcmd[0] && cfg.prot[i]->mnemonic==ch
					&& chk_ar(cfg.prot[i]->ar,&useron,&client))
					break;
			if(i>=cfg.total_prots)	/* This shouldn't happen */
				continue;
			sprintf(str,"%s%s.rep",cfg.temp_dir,cfg.sys_id);
			protocol(cfg.prot[i],XFER_UPLOAD,str,nulstr,true);
			unpack_rep();
			delfiles(cfg.temp_dir,ALLFILES);
			//autohangup();
		}
	}
	delfiles(cfg.temp_dir,ALLFILES);
	free(sav_ptr);
}

void sbbs_t::qwksetptr(uint subnum, char *buf, int reset)
{
	long		l;
	uint32_t	last;

	if(buf[2]=='/' && buf[5]=='/') {    /* date specified */
		time_t t=dstrtounix(&cfg,buf);
		subscan[subnum].ptr=getmsgnum(subnum,t);
		return;
	}
	l=atol(buf);
	if(l>=0)							  /* ptr specified */
		subscan[subnum].ptr=l;
	else if(l) {						  /* relative ptr specified */
		getlastmsg(subnum,&last,/* time_t* */NULL);
		if(-l>(long)last)
			subscan[subnum].ptr=0;
		else
			subscan[subnum].ptr=last+l;
	}
	else if(reset)
		getlastmsg(subnum,&(subscan[subnum].ptr),/* time_t* */NULL);
}


/****************************************************************************/
/* Process a QWK Config line												*/
/****************************************************************************/
void sbbs_t::qwkcfgline(char *buf,uint subnum)
{
	char	str[128];
	char 	tmp[512];
	uint 	x,y;
	long	l;
	ulong	qwk=useron.qwk;
	file_t	f;

	sprintf(str,"%-25.25s",buf);	/* Note: must be space-padded, left justified */
	strupr(str);
	bprintf("\1n\r\n\1b\1hQWK Control [\1c%s\1b]: \1g%s\r\n"
		,subnum==INVALID_SUB ? "Mail":cfg.sub[subnum]->qwkname,str);

	if(subnum!=INVALID_SUB) {					/* Only valid in sub-boards */

		if(!strncmp(str,"DROP ",5)) {              /* Drop from new-scan */
			l=atol(str+5);
			if(!l)
				subscan[subnum].cfg&=~SUB_CFG_NSCAN;
			else {
				x=l/1000;
				y=l-(x*1000);
				if(x>=usrgrps || y>=usrsubs[x]) {
					bprintf(text[QWKInvalidConferenceN],l);
					sprintf(str,"Invalid conference number %lu",l);
					logline(LOG_NOTICE,"Q!",str);
				}
				else
					subscan[usrsub[x][y]].cfg&=~SUB_CFG_NSCAN;
			}
			return;
		}

		if(!strncmp(str,"ADD YOURS ",10)) {               /* Add to new-scan */
			subscan[subnum].cfg|=(SUB_CFG_NSCAN|SUB_CFG_YSCAN);
			qwksetptr(subnum,str+10,0);
			return;
		}

		else if(!strncmp(str,"YOURS ",6)) {
			subscan[subnum].cfg|=(SUB_CFG_NSCAN|SUB_CFG_YSCAN);
			qwksetptr(subnum,str+6,0);
			return;
		}

		else if(!strncmp(str,"ADD ",4)) {               /* Add to new-scan */
			subscan[subnum].cfg|=SUB_CFG_NSCAN;
			subscan[subnum].cfg&=~SUB_CFG_YSCAN;
			qwksetptr(subnum,str+4,0);
			return;
		}

		if(!strncmp(str,"RESET ",6)) {             /* set msgptr */
			qwksetptr(subnum,str+6,1);
			return;
		}

		if(!strncmp(str,"SUBPTR ",7)) {
			qwksetptr(subnum,str+7,1);
			return;
		}
		}

	if(!strncmp(str,"RESETALL ",9)) {              /* set all ptrs */
		for(x=y=0;x<usrgrps;x++)
			for(y=0;y<usrsubs[x];y++)
				if(subscan[usrsub[x][y]].cfg&SUB_CFG_NSCAN)
					qwksetptr(usrsub[x][y],str+9,1);
	}

	else if(!strncmp(str,"ALLPTR ",7)) {              /* set all ptrs */
		for(x=y=0;x<usrgrps;x++)
			for(y=0;y<usrsubs[x];y++)
				if(subscan[usrsub[x][y]].cfg&SUB_CFG_NSCAN)
					qwksetptr(usrsub[x][y],str+7,1);
	}

	else if(!strncmp(str,"FILES ",6)) {                 /* files list */
		if(!strncmp(str+6,"ON ",3))
			useron.qwk|=QWK_FILES;
		else if(str[8]=='/' && str[11]=='/') {      /* set scan date */
			useron.qwk|=QWK_FILES;
			ns_time=dstrtounix(&cfg,str+6);
		}
		else
			useron.qwk&=~QWK_FILES;
	}

	else if(!strncmp(str,"OWN ",4)) {                   /* message from you */
		if(!strncmp(str+4,"ON ",3))
			useron.qwk|=QWK_BYSELF;
		else
			useron.qwk&=~QWK_BYSELF;
		return;
	}

	else if(!strncmp(str,"NDX ",4)) {                   /* include indexes */
		if(!strncmp(str+4,"OFF ",4))
			useron.qwk|=QWK_NOINDEX;
		else
			useron.qwk&=~QWK_NOINDEX;
	}

	else if(!strncmp(str,"CONTROL ",8)) {               /* exclude ctrl files */
		if(!strncmp(str+8,"OFF ",4))
			useron.qwk|=QWK_NOCTRL;
		else
			useron.qwk&=~QWK_NOCTRL;
	}

	else if(!strncmp(str,"VIA ",4)) {                   /* include @VIA: */
		if(!strncmp(str+4,"ON ",3))
			useron.qwk|=QWK_VIA;
		else
			useron.qwk&=~QWK_VIA;
	}

	else if(!strncmp(str,"MSGID ",6)) {                 /* include @MSGID: */
		if(!strncmp(str+6,"ON ",3))
			useron.qwk|=QWK_MSGID;
		else
			useron.qwk&=~QWK_MSGID;
	}

	else if(!strncmp(str,"TZ ",3)) {                    /* include @TZ: */
		if(!strncmp(str+3,"ON ",3))
			useron.qwk|=QWK_TZ;
		else
			useron.qwk&=~QWK_TZ;
	}

	else if(!strncmp(str,"ATTACH ",7)) {                /* file attachments */
		if(!strncmp(str+7,"ON ",3))
			useron.qwk|=QWK_ATTACH;
		else
			useron.qwk&=~QWK_ATTACH;
	}

	else if(!strncmp(str,"DELMAIL ",8)) {               /* delete mail */
		if(!strncmp(str+8,"ON ",3))
			useron.qwk|=QWK_DELMAIL;
		else
			useron.qwk&=~QWK_DELMAIL;
	}

	else if(!strncmp(str,"CTRL-A ",7)) {                /* Ctrl-a codes  */
		if(!strncmp(str+7,"KEEP ",5)) {
			useron.qwk|=QWK_RETCTLA;
			useron.qwk&=~QWK_EXPCTLA;
		}
		else if(!strncmp(str+7,"EXPAND ",7)) {
			useron.qwk|=QWK_EXPCTLA;
			useron.qwk&=~QWK_RETCTLA;
		}
		else
			useron.qwk&=~(QWK_EXPCTLA|QWK_RETCTLA);
	}

	else if(!strncmp(str,"MAIL ",5)) {                  /* include e-mail */
		if(!strncmp(str+5,"ALL ",4)) {
			useron.qwk|=QWK_ALLMAIL;
			useron.qwk&=~QWK_EMAIL;
		}
		else if(!strncmp(str+5,"ON ",3)) {
			useron.qwk|=QWK_EMAIL;
			useron.qwk&=~QWK_ALLMAIL;
		}
		else
			useron.qwk&=~(QWK_ALLMAIL|QWK_EMAIL);
	}

	else if(!strncmp(str,"FREQ ",5)) {                  /* file request */
		padfname(str+5,f.name);
		for(x=y=0;x<usrlibs;x++) {
			for(y=0;y<usrdirs[x];y++)
				if(findfile(&cfg,usrdir[x][y],f.name))
					break;
			if(y<usrdirs[x])
				break;
		}
		if(x>=usrlibs) {
			bprintf("\r\n%s",f.name);
			bputs(text[FileNotFound]);
		}
		else {
			f.dir=usrdir[x][y];
			getfileixb(&cfg,&f);
			f.size=0;
			getfiledat(&cfg,&f);
			if(f.size==-1L)
				bprintf(text[FileIsNotOnline],f.name);
			else
				addtobatdl(&f);
		}

	}

	else {
		attr(cfg.color[clr_err]);
		bputs("Unrecognized Control Command!\1n\r\n");
	}

	if(qwk!=useron.qwk)
		putuserrec(&cfg,useron.number,U_QWK,8,ultoa(useron.qwk,tmp,16));
}


int sbbs_t::set_qwk_flag(ulong flag)
{
	int i;
	char str[32];

	if(useron.qwk&flag)
		return 0;
	if((i=getuserrec(&cfg,useron.number,U_QWK,8,str))!=0)
		return(i);
	useron.qwk=ahtoul(str);
	useron.qwk|=flag;
	return putuserrec(&cfg,useron.number,U_QWK,8,ultoa(useron.qwk,str,16));
}

/****************************************************************************/
/* Convert a QWK conference number into a sub-board offset					*/
/* Return INVALID_SUB upon failure to convert								*/
/****************************************************************************/
uint sbbs_t::resolve_qwkconf(uint n, int hubnum)
{
	uint	j,k;

	if(hubnum >= 0 && hubnum < cfg.total_qhubs) {
		for(j=0;j<cfg.qhub[hubnum]->subs;j++)
			if(cfg.qhub[hubnum]->conf[j] == n)
				return cfg.qhub[hubnum]->sub[j]->subnum;
		return INVALID_SUB;
	}

	for(j=0;j<usrgrps;j++)
		for(k=0;k<usrsubs[j];k++)
			if(cfg.sub[usrsub[j][k]]->qwkconf == n)
				return usrsub[j][k];

	if(n<1000) {			 /* version 1 method, start at 101 */
		j=n/100;
		k=n%100;
	}
	else {					 /* version 2 method, start at 1001 */
		j=n/1000;
		k=n%1000;
	}
	if(j == 0 || k == 0)
		return INVALID_SUB;
	j--;	/* j is group */
	k--;	/* k is sub */
	if(j>=usrgrps || k>=usrsubs[j] || cfg.sub[usrsub[j][k]]->qwkconf != 0)
		return INVALID_SUB;

	return usrsub[j][k];
}

bool sbbs_t::qwk_voting(str_list_t* ini, long offset, smb_net_type_t net_type, const char* qnet_id, int hubnum)
{
	char* section;
	char location[128];
	bool result;
	int found;
	str_list_t section_list = iniGetSectionList(*ini, /* prefix: */NULL);

	sprintf(location, "%lx", offset);
	if((found = strListFind(section_list, location, /* case_sensitive: */FALSE)) < 0) {
		strListFree(&section_list);
		return false;
	}
	/* The section immediately following the (empty) [offset] section is the section of interest */
	if((section = section_list[found+1]) == NULL) {
		strListFree(&section_list);
		return false;
	}
	result = qwk_vote(*ini, section, net_type, qnet_id, hubnum);
	iniRemoveSection(ini, section);
	iniRemoveSection(ini, location);
	strListFree(&section_list);
	return result;
}

void sbbs_t::qwk_handle_remaining_votes(str_list_t* ini, smb_net_type_t net_type, const char* qnet_id, int hubnum)
{
	str_list_t section_list = iniGetSectionList(*ini, /* prefix: */NULL);

	for(int i=0; section_list != NULL && section_list[i] != NULL; i++)
		qwk_vote(*ini, section_list[i], net_type, qnet_id, hubnum);
	strListFree(&section_list);
}

bool sbbs_t::qwk_vote(str_list_t ini, const char* section, smb_net_type_t net_type, const char* qnet_id, int hubnum)
{
	char* p;
	int result;
	smb_t smb;
	ZERO_VAR(smb);

	smb.subnum = resolve_qwkconf(iniGetInteger(ini, section, "Conference", 0), hubnum);
	if(smb.subnum == INVALID_SUB)
		return false;
	if(cfg.sub[smb.subnum]->misc&SUB_NOVOTING)
		return false;
	if((result = smb_open_sub(&cfg, &smb, smb.subnum)) != SMB_SUCCESS) {
		errormsg(WHERE, ERR_OPEN, smb.file, 0, smb.last_error);
		return false;
	}

	smbmsg_t msg;
	ZERO_VAR(msg);

	if((p=iniGetString(ini, section, "WhenWritten", NULL, NULL)) != NULL) {
		char	zone[32];
		xpDateTime_t dt=isoDateTimeStr_parse(p);
		msg.hdr.when_written.time=(uint32_t)xpDateTime_to_localtime(dt);
		msg.hdr.when_written.zone=dt.zone;
		sscanf(p,"%*s %s",zone);
		if(zone[0])
			msg.hdr.when_written.zone=(ushort)strtoul(zone,NULL,16);
	}

	if((p=iniGetString(ini, section, smb_hfieldtype(SENDER), NULL, NULL)) != NULL)
		smb_hfield_str(&msg, SENDER, p);

	if((p=iniGetString(ini, section, smb_hfieldtype(SUBJECT), NULL, NULL)) != NULL)
		smb_hfield_str(&msg, SUBJECT, p);

	/* ToDo: Check circular routes here? */
	if(net_type == NET_QWK) {
		char fulladdr[256];
		const char* netaddr = iniGetString(ini, section, smb_hfieldtype(SENDERNETADDR), NULL, NULL);
		if(netaddr == NULL)
			netaddr = qnet_id;
		else {
			SAFEPRINTF2(fulladdr, "%s/%s", qnet_id, netaddr);
			netaddr = fulladdr;
		}
		if(netaddr != NULL) {
			smb_hfield_netaddr(&msg, SENDERNETADDR, netaddr, &net_type);
			smb_hfield(&msg, SENDERNETTYPE, sizeof(net_type), &net_type);
		}
	}

	if((p=iniGetString(ini, section, smb_hfieldtype(RFC822REPLYID), NULL, NULL)) != NULL)
		smb_hfield_str(&msg, RFC822REPLYID, p);

	/* Trace header fields */
	while((p=iniGetString(ini, section, smb_hfieldtype(SENDERIPADDR), NULL, NULL)) != NULL)
		smb_hfield_str(&msg, SENDERIPADDR, p);
	while((p=iniGetString(ini, section, smb_hfieldtype(SENDERHOSTNAME), NULL, NULL)) != NULL)
		smb_hfield_str(&msg, SENDERHOSTNAME, p);
	while((p=iniGetString(ini, section, smb_hfieldtype(SENDERPROTOCOL), NULL, NULL)) != NULL)
		smb_hfield_str(&msg, SENDERPROTOCOL, p);
	while((p=iniGetString(ini,section, smb_hfieldtype(SENDERORG), NULL, NULL)) != NULL)
		smb_hfield_str(&msg, SENDERORG, p);

	if(strnicmp(section, "poll:", 5) == 0) {

		smb_hfield_str(&msg, RFC822MSGID, section + 5);
		msg.hdr.votes = iniGetShortInt(ini, section, "MaxVotes", 0);
		ulong results = iniGetLongInt(ini, section, "Results", 0);
		msg.hdr.auxattr = (results << POLL_RESULTS_SHIFT) & POLL_RESULTS_MASK;
		for(int i=0;;i++) {
			char str[128];
			SAFEPRINTF2(str, "%s%u", smb_hfieldtype(SMB_COMMENT), i);
			const char* comment = iniGetString(ini, section, str, NULL, NULL);
			if(comment == NULL)
				break;
			smb_hfield_str(&msg, SMB_COMMENT, comment);
		}
		for(int i=0;;i++) {
			char str[128];
			SAFEPRINTF2(str, "%s%u", smb_hfieldtype(SMB_POLL_ANSWER), i);
			const char* answer = iniGetString(ini, section, str, NULL, NULL);
			if(answer == NULL)
				break;
			smb_hfield_str(&msg, SMB_POLL_ANSWER, answer);
		}
		if((result=postpoll(&cfg, &smb, &msg)) != SMB_SUCCESS)
			errormsg(WHERE, ERR_WRITE, smb.file, result, smb.last_error);
	}
	else if(strnicmp(section, "vote:", 5) == 0) {
		const char* notice = NULL;

		smb_hfield_str(&msg, RFC822MSGID, section + 5);
		if(iniGetBool(ini, section, "upvote", FALSE)) {
			msg.hdr.attr = MSG_UPVOTE;
			notice = text[MsgUpVoteNotice];
		}
		else if(iniGetBool(ini, section, "downvote", FALSE)) {
			msg.hdr.attr = MSG_DOWNVOTE;
			notice = text[MsgDownVoteNotice];
		}
		else {
			msg.hdr.attr = MSG_VOTE;
			msg.hdr.votes = iniGetShortInt(ini, section, "votes", 0);
			notice = text[PollVoteNotice];
		}
		result = votemsg(&cfg, &smb, &msg, notice);
		if(result != SMB_SUCCESS && result != SMB_DUPE_MSG)
			errormsg(WHERE, ERR_WRITE, smb.file, result, smb.last_error);
	}
	else if(strnicmp(section, "close:", 6) == 0) {
		smb_hfield_str(&msg, RFC822MSGID, section + 6);
		if((result = smb_addpollclosure(&smb, &msg, smb_storage_mode(&cfg, &smb))) != SMB_SUCCESS)
			errormsg(WHERE,ERR_WRITE,smb.file,result,smb.last_error);
	}
	else result = SMB_SUCCESS;

	smb_close(&smb);
	smb_freemsgmem(&msg);
	return result == SMB_SUCCESS;
}
