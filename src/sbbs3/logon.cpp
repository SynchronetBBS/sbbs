/* logon.cpp */

/* Synchronet user logon routines */

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

extern "C" void client_on(SOCKET sock, client_t* client, BOOL update);

/****************************************************************************/
/* Called once upon each user logging on the board							*/
/* Returns 1 if user passed logon, 0 if user failed.						*/
/****************************************************************************/
bool sbbs_t::logon()
{
	char	str[256],c;
	char 	tmp[512];
	int 	file;
	uint	i,j,mailw;
	ulong	totallogons;
	node_t	node;
	struct	tm* tm_p;
	struct	tm	tm;

	now=time(NULL);
	tm_p=localtime(&now);
	if(tm_p==NULL)
		return(false);
	tm=*tm_p;

	if(!useron.number)
		return(false);

	client.user=useron.alias;
	client_on(client_socket,&client,TRUE /* update */);

#ifdef JAVASCRIPT
	js_create_user_objects();
#endif

	if(useron.rest&FLAG('Q'))
		qwklogon=1;
	if(SYSOP && ((online==ON_REMOTE && !(cfg.sys_misc&SM_R_SYSOP))
		|| (online==ON_LOCAL && !(cfg.sys_misc&SM_L_SYSOP))))
		return(false);
	if(cur_rate<cfg.node_minbps && !(useron.exempt&FLAG('M'))) {
		bprintf(text[MinimumModemSpeed],cfg.node_minbps);
		sprintf(str,"%stooslow.msg",cfg.text_dir);
		if(fexist(str))
			printfile(str,0);
		sprintf(str,"(%04u)  %-25s  Modem speed: %lu<%u"
			,useron.number,useron.alias,cur_rate,cfg.node_minbps);
		logline("+!",str);
		return(false); 
	}

	if(useron.rest&FLAG('G')) {     /* Guest account */
		useron.misc=(cfg.new_misc&(~ASK_NSCAN));
		useron.rows=0;
		useron.misc&=~(ANSI|RIP|WIP|NO_EXASCII|COLOR);
		useron.misc|=autoterm;
		if(!(useron.misc&ANSI) && yesno(text[AnsiTerminalQ]))
			useron.misc|=ANSI;
		if(useron.misc&(RIP|WIP)
			|| (useron.misc&ANSI && yesno(text[ColorTerminalQ])))
			useron.misc|=COLOR;
		if(!yesno(text[ExAsciiTerminalQ]))
			useron.misc|=NO_EXASCII;
		for(i=0;i<cfg.total_xedits;i++)
			if(!stricmp(cfg.xedit[i]->code,cfg.new_xedit)
				&& chk_ar(cfg.xedit[i]->ar,&useron))
				break;
		if(i<cfg.total_xedits)
			useron.xedit=i+1;
		else
			useron.xedit=0;
		useron.prot=cfg.new_prot;
		useron.shell=cfg.new_shell; 
	}

	if(cfg.node_dollars_per_call) {
		adjustuserrec(&cfg,useron.number,U_CDT,10
			,cfg.cdt_per_dollar*cfg.node_dollars_per_call);
		bprintf(text[CreditedAccount]
			,cfg.cdt_per_dollar*cfg.node_dollars_per_call);
		sprintf(str,"%s #%u was billed $%d T: %lu seconds"
			,useron.alias,useron.number
			,cfg.node_dollars_per_call,now-answertime);
		logline("$+",str);
		hangup();
		return(false); 
	}

	if(!chk_ar(cfg.node_ar,&useron)) {
		bputs(text[NoNodeAccess]);
		sprintf(str,"(%04u)  %-25s  Insufficient node access"
			,useron.number,useron.alias);
		logline("+!",str);
		return(false); 
	}

	getnodedat(cfg.node_num,&thisnode,1);
	if(thisnode.misc&NODE_LOCK) {
		putnodedat(cfg.node_num,&thisnode);	/* must unlock! */
		if(!SYSOP && !(useron.exempt&FLAG('N'))) {
			bputs(text[NodeLocked]);
			sprintf(str,"(%04u)  %-25s  Locked node logon attempt"
				,useron.number,useron.alias);
			logline("+!",str);
			return(false); 
		}
		if(yesno(text[RemoveNodeLockQ])) {
			getnodedat(cfg.node_num,&thisnode,1);
			logline("S-","Removed Node Lock");
			thisnode.misc&=~NODE_LOCK; 
		}
		else
			getnodedat(cfg.node_num,&thisnode,1); 
	}

	if((useron.exempt&FLAG('Q') && useron.misc&QUIET))
		thisnode.status=NODE_QUIET;
	else
		thisnode.status=NODE_INUSE;
	action=thisnode.action=NODE_LOGN;
	thisnode.connection=0xffff;
	thisnode.misc&=~(NODE_ANON|NODE_INTR|NODE_MSGW|NODE_POFF|NODE_AOFF);
	if(useron.chat&CHAT_NOACT)
		thisnode.misc|=NODE_AOFF;
	if(useron.chat&CHAT_NOPAGE)
		thisnode.misc|=NODE_POFF;
	thisnode.useron=useron.number;
	putnodedat(cfg.node_num,&thisnode);

	getusrsubs();
	getusrdirs();

	if(useron.misc&CURSUB && !(useron.rest&FLAG('G'))) {
		for(i=0;i<usrgrps;i++) {
			for(j=0;j<usrsubs[i];j++) {
				if(!strcmp(cfg.sub[usrsub[i][j]]->code,useron.cursub))
					break; 
			}
			if(j<usrsubs[i]) {
				curgrp=i;
				cursub[i]=j;
				break; 
			} 
		}
		for(i=0;i<usrlibs;i++) {
			for(j=0;j<usrdirs[i];j++)
				if(!strcmp(cfg.dir[usrdir[i][j]]->code,useron.curdir))
					break;
			if(j<usrdirs[i]) {
				curlib=i;
				curdir[i]=j;
				break; 
			} 
		} 
	}


	if(useron.misc&AUTOTERM) {
		useron.misc&=~(ANSI|RIP|WIP);
		useron.misc|=autoterm; 
	}

	if(!chk_ar(cfg.shell[useron.shell]->ar,&useron)) {
		useron.shell=cfg.new_shell;
		if(!chk_ar(cfg.shell[useron.shell]->ar,&useron)) {
			for(i=0;i<cfg.total_shells;i++)
				if(chk_ar(cfg.shell[i]->ar,&useron))
					break;
			if(i==cfg.total_shells)
				useron.shell=0; 
		} 
	}

	logon_ml=useron.level;
	logontime=time(NULL);
	starttime=logontime;
	useron.logontime=logontime;
	last_ns_time=ns_time=useron.ns_time;
	// ns_time-=(useron.tlast*60); /* file newscan time == last logon time */
	delfiles(cfg.temp_dir,ALLFILES);
	sprintf(str,"%smsgs/n%3.3u.msg",cfg.data_dir,cfg.node_num);
	remove(str);            /* remove any pending node messages */
	sprintf(str,"%smsgs/n%3.3u.ixb",cfg.data_dir,cfg.node_num);
	remove(str);			/* remove any pending node message indices */

	if(!SYSOP && online==ON_REMOTE) {
		rioctl(IOCM|ABORT);	/* users can't abort anything */
		rioctl(IOCS|ABORT); 
	}

	CLS;
	if(useron.rows)
		rows=useron.rows;
	else if(online==ON_LOCAL)
		rows=cfg.node_scrnlen-1;
	unixtodstr(&cfg,logontime,str);
	if(!strncmp(str,useron.birth,5) && !(useron.rest&FLAG('Q'))) {
		bputs(text[HappyBirthday]);
		pause();
		CLS;
		user_event(EVENT_BIRTHDAY); 
	}
	unixtodstr(&cfg,useron.laston,tmp);
	if(strcmp(str,tmp)) {			/* str still equals logon time */
		useron.ltoday=1;
		useron.ttoday=useron.etoday=useron.ptoday=useron.textra=0;
		useron.freecdt=cfg.level_freecdtperday[useron.level]; 
	}
	else
		useron.ltoday++;

	gettimeleft();
	sprintf(str,"%sfile/%04u.dwn",cfg.data_dir,useron.number);
	batch_add_list(str);
	if(!qwklogon) { 	 /* QWK Nodes don't go through this */

		if(cfg.sys_pwdays
			&& (ulong)logontime>(useron.pwmod+((ulong)cfg.sys_pwdays*24UL*60UL*60UL))) {
			bprintf(text[TimeToChangePw],cfg.sys_pwdays);

			c=0;
			while(c<LEN_PASS) { 				/* Create random password */
				str[c]=sbbs_random(43)+'0';
				if(isalnum(str[c]))
					c++; 
			}
			str[c]=0;
			bprintf(text[YourPasswordIs],str);

			if(cfg.sys_misc&SM_PWEDIT && yesno(text[NewPasswordQ]))
				while(online) {
					bputs(text[NewPassword]);
					getstr(str,LEN_PASS,K_UPPER|K_LINE);
					truncsp(str);
					if(chkpass(str,&useron,true))
						break;
					CRLF; 
				}

			while(online) {
				if(cfg.sys_misc&SM_PWEDIT) {
					CRLF;
					bputs(text[VerifyPassword]); 
				}
				else
					bputs(text[NewUserPasswordVerify]);
				console|=CON_R_ECHOX;
				if(!(cfg.sys_misc&SM_ECHO_PW))
					console|=CON_L_ECHOX;
				getstr(tmp,LEN_PASS,K_UPPER);
				console&=~(CON_R_ECHOX|CON_L_ECHOX);
				if(strcmp(str,tmp)) {
					bputs(text[Wrong]);
					continue; 
				}
				break; 
			}
			strcpy(useron.pass,str);
			useron.pwmod=time(NULL);
			putuserrec(&cfg,useron.number,U_PWMOD,8,ultoa(useron.pwmod,str,16));
			bputs(text[PasswordChanged]);
			pause(); 
		}
		if(useron.ltoday>cfg.level_callsperday[useron.level]
			&& !(useron.exempt&FLAG('L'))) {
			bputs(text[NoMoreLogons]);
			sprintf(str,"(%04u)  %-25s  Out of logons"
				,useron.number,useron.alias);
			logline("+!",str);
			hangup();
			return(false); 
		}
		if(useron.rest&FLAG('L') && useron.ltoday>1) {
			bputs(text[R_Logons]);
			sprintf(str,"(%04u)  %-25s  Out of logons"
				,useron.number,useron.alias);
			logline("+!",str);
			hangup();
			return(false); 
		}
		if(!(useron.rest&FLAG('G'))) {
			if(!useron.name[0] && ((cfg.uq&UQ_ALIASES && cfg.uq&UQ_REALNAME)
				|| cfg.uq&UQ_COMPANY))
				while(online) {
					if(cfg.uq&UQ_ALIASES && cfg.uq&UQ_REALNAME)
						bputs(text[EnterYourRealName]);
					else
						bputs(text[EnterYourCompany]);
					getstr(useron.name,LEN_NAME,K_UPRLWR|(cfg.uq&UQ_NOEXASC));
					if(cfg.uq&UQ_ALIASES && cfg.uq&UQ_REALNAME) {
						if(trashcan(useron.name,"name") || !useron.name[0]
							|| !strchr(useron.name,SP)
							|| strchr(useron.name,0xff)
							|| (cfg.uq&UQ_DUPREAL
								&& userdatdupe(useron.number,U_NAME,LEN_NAME
								,useron.name,0)))
							bputs(text[YouCantUseThatName]);
						else
							break; 
					}
					else
						break; 
				}
			if(cfg.uq&UQ_HANDLE && !useron.handle[0]) {
				sprintf(useron.handle,"%.*s",LEN_HANDLE,useron.alias);
				while(online) {
					bputs(text[EnterYourHandle]);
					if(!getstr(useron.handle,LEN_HANDLE
						,K_LINE|K_EDIT|K_AUTODEL|(cfg.uq&UQ_NOEXASC))
						|| strchr(useron.handle,0xff)
						|| (cfg.uq&UQ_DUPHAND
							&& userdatdupe(useron.number,U_HANDLE,LEN_HANDLE
							,useron.handle,0))
						|| trashcan(useron.handle,"name"))
						bputs(text[YouCantUseThatName]);
					else
						break; 
				} 
			}
			if(cfg.uq&UQ_LOCATION && !useron.location[0])
				while(online) {
					bputs(text[EnterYourCityState]);
					if(getstr(useron.location,LEN_LOCATION,K_UPRLWR|(cfg.uq&UQ_NOEXASC)))
						break; 
				}
			if(cfg.uq&UQ_ADDRESS && !useron.address[0])
				while(online) {
					bputs(text[EnterYourAddress]);
					if(getstr(useron.address,LEN_ADDRESS,K_UPRLWR|(cfg.uq&UQ_NOEXASC)))
						break; 
				}
			if(cfg.uq&UQ_ADDRESS && !useron.zipcode[0])
				while(online) {
					bputs(text[EnterYourZipCode]);
					if(getstr(useron.zipcode,LEN_ZIPCODE,K_UPPER|(cfg.uq&UQ_NOEXASC)))
						break; 
				}
			if(cfg.uq&UQ_PHONE && !useron.phone[0]) {
				i=yesno(text[CallingFromNorthAmericaQ]);
				while(online) {
					bputs(text[EnterYourPhoneNumber]);
					if(i) {
						if(gettmplt(useron.phone,cfg.sys_phonefmt
							,K_LINE|(cfg.uq&UQ_NOEXASC))<strlen(cfg.sys_phonefmt))
							 continue; 
					} else {
						if(getstr(useron.phone,LEN_PHONE
							,K_UPPER|(cfg.uq&UQ_NOEXASC))<5)
							continue; 
					}
					if(!trashcan(useron.phone,"phone"))
						break; 
				} 
			}
			if(!(sys_status&SS_RLOGIN) 
				&& !(cfg.uq&UQ_NONETMAIL) && !useron.netmail[0]) {
				while(online) {
					bputs(text[EnterNetMailAddress]);
					if(getstr(useron.netmail,LEN_NETMAIL,K_EDIT|K_AUTODEL|K_LINE)
						&& !trashcan(useron.netmail,"email"))
						break;
				}
				if(useron.netmail[0] && cfg.sys_misc&SM_FWDTONET && !noyes(text[ForwardMailQ]))
					useron.misc|=NETMAIL;
				else 
					useron.misc&=~NETMAIL;
			}
			if(cfg.new_sif[0]) {
				sprintf(str,"%suser/%4.4u.dat",cfg.data_dir,useron.number);
				if(flength(str)<1L)
					create_sif_dat(cfg.new_sif,str); 
			} 
		}
	}	
	if(!online) {
		sprintf(str,"(%04u)  %-25s  Unsuccessful logon"
			,useron.number,useron.alias);
		logline("+!",str);
		return(false); 
	}
	strcpy(useron.modem,connection);
	useron.logons++;
	putuserdat(&cfg,&useron);
	getmsgptrs();
	sys_status|=SS_USERON;          /* moved from further down */

	if(useron.rest&FLAG('Q')) {
		sprintf(str,"(%04u)  %-25s  QWK Network Connection"
			,useron.number,useron.alias);
		logline("++",str);
		return(true); 
	}

	/********************/
	/* SUCCESSFUL LOGON */
	/********************/
	totallogons=logonstats();
	sprintf(str,"(%04u)  %-25s  Logon %lu - %u"
		,useron.number,useron.alias,totallogons,useron.ltoday);
	logline("++",str);

	if(!qwklogon && cfg.logon_mod[0])
		exec_bin(cfg.logon_mod,&main_csi);

	if(thisnode.status!=NODE_QUIET && (!REALSYSOP || cfg.sys_misc&SM_SYSSTAT)) {
		sprintf(str,"%slogon.lst",cfg.data_dir);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_RDWR|O_CREAT|O_APPEND);
			return(false); 
		}
		sprintf(str,text[LastFewCallersFmt],cfg.node_num
			,totallogons,useron.alias
			,cfg.sys_misc&SM_LISTLOC ? useron.location : useron.note
			,tm.tm_hour,tm.tm_min
			,connection,useron.ltoday);
		write(file,str,strlen(str));
		close(file); 
	}

	if(cfg.sys_logon[0])				/* execute system logon event */
		external(cmdstr(cfg.sys_logon,nulstr,nulstr,NULL),EX_OUTR|EX_OUTL); /* EX_SH */

	if(qwklogon)
		return(true);

	sys_status|=SS_PAUSEON;	/* always force pause on during this section */
	mailw=getmail(&cfg,useron.number,0);

	if(!(cfg.sys_misc&SM_NOSYSINFO)) {
		bprintf(text[SiSysName],cfg.sys_name);
		//bprintf(text[SiNodeNumberName],cfg.node_num,cfg.node_name);
		bprintf(text[LiUserNumberName],useron.number,useron.alias);
		bprintf(text[LiLogonsToday],useron.ltoday
			,cfg.level_callsperday[useron.level]);
		bprintf(text[LiTimeonToday],useron.ttoday
			,cfg.level_timeperday[useron.level]+useron.min);
		bprintf(text[LiMailWaiting],mailw);
		strcpy(str,text[LiSysopIs]);
		if(startup->options&BBS_OPT_SYSOP_AVAILABLE 
			|| (cfg.sys_chat_ar[0] && chk_ar(cfg.sys_chat_ar,&useron)))
			strcat(str,text[LiSysopAvailable]);
		else
			strcat(str,text[LiSysopNotAvailable]);
		bprintf("%s\r\n\r\n",str);
	}

	if(sys_status&SS_EVENT)
		bputs(text[ReducedTime]);
	getnodedat(cfg.node_num,&thisnode,1);
	thisnode.misc&=~(NODE_AOFF|NODE_POFF);
	if(useron.chat&CHAT_NOACT)
		thisnode.misc|=NODE_AOFF;
	if(useron.chat&CHAT_NOPAGE)
		thisnode.misc|=NODE_POFF;
	putnodedat(cfg.node_num,&thisnode);

	getsmsg(useron.number); 		/* Moved from further down */
	SYNC;
	c=0;
	for(i=1;i<=cfg.sys_nodes;i++)
		if(i!=cfg.node_num) {
			getnodedat(i,&node,0);
			if(!(cfg.sys_misc&SM_NONODELIST)
				&& (node.status==NODE_INUSE
					|| ((node.status==NODE_QUIET || node.errors) && SYSOP))) {
				if(!c)
					bputs(text[NodeLstHdr]);
				printnodedat(i,&node);
				c=1; 
			}
			if(node.status==NODE_INUSE && i!=cfg.node_num && node.useron==useron.number
				&& !SYSOP && !(useron.exempt&FLAG('G'))) {
				strcpy(tmp,"On two nodes at the same time");
				sprintf(str,"(%04u)  %-25s  %s"
					,useron.number,useron.alias,tmp);
				logline("+!",str);
				errorlog(tmp);
				bputs(text[UserOnTwoNodes]);
				hangup();
				return(false); 
			}
			if(thisnode.status!=NODE_QUIET
				&& (node.status==NODE_INUSE || node.status==NODE_QUIET)
				&& !(node.misc&NODE_AOFF) && node.useron!=useron.number) {
				sprintf(str,text[NodeLoggedOnAtNbps]
					,cfg.node_num
					,thisnode.misc&NODE_ANON ? text[UNKNOWN_USER] : useron.alias
					,connection);
				putnmsg(&cfg,i,str); 
			} 
		}

	if(cfg.sys_exp_warn && useron.expire && useron.expire>now /* Warn user of coming */
		&& (useron.expire-now)/(1440L*60L)<=cfg.sys_exp_warn) /* expiration */
		bprintf(text[AccountWillExpireInNDays],(useron.expire-now)/(1440L*60L));

	if(criterrs && SYSOP)
		bprintf(text[CriticalErrors],criterrs);
	if((i=getuserxfers(0,useron.number,0))!=0)
		bprintf(text[UserXferForYou],i,i>1 ? "s" : nulstr); 
	if((i=getuserxfers(useron.number,0,0))!=0)
		bprintf(text[UnreceivedUserXfer],i,i>1 ? "s" : nulstr);
	SYNC;
	sys_status&=~SS_PAUSEON;	/* Turn off the pause override flag */
	if(online==ON_REMOTE)
		rioctl(IOSM|ABORT);		/* Turn abort ability on */
	if(mailw) {
		if(yesno(text[ReadYourMailNowQ]))
			readmail(useron.number,MAIL_YOUR); 
	}
	if(usrgrps && useron.misc&ASK_NSCAN && yesno(text[NScanAllGrpsQ]))
		scanallsubs(SCAN_NEW);
	if(usrgrps && useron.misc&ASK_SSCAN && yesno(text[SScanAllGrpsQ]))
		scanallsubs(SCAN_TOYOU);
	return(true);
}

/****************************************************************************/
/* Checks the system dsts.dab to see if it is a new day, if it is, all the  */
/* nodes' and the system's csts.dab are added to, and the dsts.dab's daily  */
/* stats are cleared. Also increments the logon values in dsts.dab if       */
/* applicable.                                                              */
/****************************************************************************/
ulong sbbs_t::logonstats()
{
    char str[256];
    int dsts,csts;
    uint i;
    time_t update_t=0;
    stats_t stats;
    node_t node;
	struct tm * tm, update_tm;

	memset(&stats,0,sizeof(stats));
	sprintf(str,"%sdsts.dab",cfg.ctrl_dir);
	if((dsts=nopen(str,O_RDWR))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_RDWR);
		return(0L); 
	}
	read(dsts,&update_t,4);         /* Last updated         */
	read(dsts,&stats.logons,4);     /* Total number of logons on system */
	close(dsts);
	if(update_t>now+(24L*60L*60L)) /* More than a day in the future? */
		errormsg(WHERE,ERR_CHK,"Daily stats time stamp",update_t);
	tm = localtime(&update_t);
	if(tm==NULL)
		return(0);
	update_tm=*tm;
	tm = localtime(&now);
	if(tm==NULL)
		return(0);
	if((tm->tm_mday>update_tm.tm_mday && tm->tm_mon==update_tm.tm_mon)
		|| tm->tm_mon>update_tm.tm_mon || tm->tm_year>update_tm.tm_year) {

		sprintf(str,"New Day - Prev: %s ",timestr(&update_t));
		logentry("!=",str);

		sys_status|=SS_DAILY;       /* New Day !!! */
		sprintf(str,"%slogon.lst",cfg.data_dir);    /* Truncate logon list */
		if((dsts=nopen(str,O_TRUNC|O_CREAT|O_WRONLY))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_TRUNC|O_CREAT|O_WRONLY);
			return(0L); 
		}
		close(dsts);
		for(i=0;i<=cfg.sys_nodes;i++) {
			if(i) {     /* updating a node */
				getnodedat(i,&node,1);
				node.misc|=NODE_EVENT;
				putnodedat(i,&node); 
			}
			sprintf(str,"%sdsts.dab",i ? cfg.node_path[i-1] : cfg.ctrl_dir);
			if((dsts=nopen(str,O_RDWR))==-1) /* node doesn't have stats yet */
				continue;
			sprintf(str,"%scsts.dab",i ? cfg.node_path[i-1] : cfg.ctrl_dir);
			if((csts=nopen(str,O_WRONLY|O_APPEND|O_CREAT))==-1) {
				close(dsts);
				errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_APPEND|O_CREAT);
				continue; 
			}
			lseek(dsts,8L,SEEK_SET);        /* Skip time and logons */
			write(csts,&now,4);
			read(dsts,&stats.ltoday,4);
			write(csts,&stats.ltoday,4);
			lseek(dsts,4L,SEEK_CUR);        /* Skip total time on */
			read(dsts,&stats.ttoday,4);
			write(csts,&stats.ttoday,4);
			read(dsts,&stats.uls,4);
			write(csts,&stats.uls,4);
			read(dsts,&stats.ulb,4);
			write(csts,&stats.ulb,4);
			read(dsts,&stats.dls,4);
			write(csts,&stats.dls,4);
			read(dsts,&stats.dlb,4);
			write(csts,&stats.dlb,4);
			read(dsts,&stats.ptoday,4);
			write(csts,&stats.ptoday,4);
			read(dsts,&stats.etoday,4);
			write(csts,&stats.etoday,4);
			read(dsts,&stats.ftoday,4);
			write(csts,&stats.ftoday,4);
			close(csts);
			lseek(dsts,0L,SEEK_SET);        /* Go back to beginning */
			write(dsts,&now,4);             /* Update time stamp  */
			lseek(dsts,4L,SEEK_CUR);        /* Skip total logons */
			stats.ltoday=0;
			write(dsts,&stats.ltoday,4);  /* Logons today to 0 */
			lseek(dsts,4L,SEEK_CUR);     /* Skip total time on */
			stats.ttoday=0;              /* Set all other today variables to 0 */
			write(dsts,&stats.ttoday,4);        /* Time on today to 0 */
			write(dsts,&stats.ttoday,4);        /* Uploads today to 0 */
			write(dsts,&stats.ttoday,4);        /* U/L Bytes today    */
			write(dsts,&stats.ttoday,4);        /* Download today     */
			write(dsts,&stats.ttoday,4);        /* Download bytes     */
			write(dsts,&stats.ttoday,4);        /* Posts today        */
			write(dsts,&stats.ttoday,4);        /* Emails today       */
			write(dsts,&stats.ttoday,4);        /* Feedback today     */
			write(dsts,&stats.ttoday,2);        /* New users Today    */
			close(dsts); 
		} 
	}

	if(thisnode.status==NODE_QUIET)       /* Quiet users aren't counted */
		return(0);

	if(REALSYSOP && !(cfg.sys_misc&SM_SYSSTAT))
		return(0);

	for(i=0;i<2;i++) {
		sprintf(str,"%sdsts.dab",i ? cfg.ctrl_dir : cfg.node_dir);
		if((dsts=nopen(str,O_RDWR))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_RDWR);
			return(0L); 
		}
		lseek(dsts,4L,SEEK_SET);        /* Skip time stamp */
		read(dsts,&stats.logons,4);
		read(dsts,&stats.ltoday,4);
		stats.logons++;
		stats.ltoday++;
		lseek(dsts,4L,SEEK_SET);        /* Rewind back and overwrite */
		write(dsts,&stats.logons,4);
		write(dsts,&stats.ltoday,4);
		close(dsts); 
	}
	return(stats.logons);
}


