/* Synchronet user logon routines */

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
#include "cmdshell.h"
#include "filedat.h"

extern "C" void client_on(SOCKET sock, client_t* client, BOOL update);

/****************************************************************************/
/* Called once upon each user logging on the board							*/
/* Returns 1 if user passed logon, 0 if user failed.						*/
/****************************************************************************/
bool sbbs_t::logon()
{
	char	str[256],c;
	char 	tmp[512];
	char	path[MAX_PATH+1];
	int		i,j;
	uint	mailw,mailr;
	int		kmode;
	uint	totallogons;
	node_t	node;
	struct	tm	tm;

	now=time(NULL);
	if(localtime_r(&now,&tm)==NULL)
		return(false);

	if(!useron.number)
		return(false);

	SAFECOPY(client.user, useron.alias);
	client.usernum = useron.number;
	client_on(client_socket,&client,TRUE /* update */);

#ifdef JAVASCRIPT
	js_create_user_objects(js_cx, js_glob);
#endif

	if(useron.dlcps)
		cur_cps = useron.dlcps;

	if(useron.rest&FLAG('Q'))
		sys_status ^= SS_QWKLOGON;

	if(useron.rest&FLAG('G')) {     /* Guest account */
		useron.misc=(cfg.new_misc&(~ASK_NSCAN));
		useron.rows = TERM_ROWS_AUTO;
		useron.cols = TERM_COLS_AUTO;
		useron.misc &= ~TERM_FLAGS;
		useron.misc|=autoterm;
		if(!(useron.misc&(ANSI|PETSCII)) && text[AnsiTerminalQ][0] && yesno(text[AnsiTerminalQ]))
			useron.misc|=ANSI;
		if((useron.misc&ANSI) && text[MouseTerminalQ][0] && yesno(text[MouseTerminalQ]))
			useron.misc|=MOUSE;
		if((useron.misc&RIP) || !(cfg.uq&UQ_COLORTERM)
			|| (useron.misc&(ANSI|PETSCII) && yesno(text[ColorTerminalQ])))
			useron.misc|=COLOR;
		if(!(useron.misc&(NO_EXASCII|PETSCII)) && !yesno(text[ExAsciiTerminalQ]))
			useron.misc|=NO_EXASCII;
		for(i=0;i<cfg.total_xedits;i++)
			if(!stricmp(cfg.xedit[i]->code,cfg.new_xedit)
				&& chk_ar(cfg.xedit[i]->ar,&useron,&client))
				break;
		if(i<cfg.total_xedits)
			useron.xedit=i+1;
		else
			useron.xedit=0;
		useron.prot=cfg.new_prot;
		useron.shell=cfg.new_shell;
		*useron.lang = '\0';
	}
	load_user_text();

	if(!chk_ars(startup->login_ars, &useron, &client)) {
		bputs(text[NoNodeAccess]);
		safe_snprintf(str, sizeof str, "(%04u)  %-25s  Insufficient server access: %s"
			,useron.number, useron.alias, startup->login_ars);
		logline(LOG_NOTICE, "+!", str);
		hangup();
		return false; 
	}

	if(!chk_ar(cfg.node_ar,&useron,&client)) {
		bputs(text[NoNodeAccess]);
		safe_snprintf(str, sizeof(str), "(%04u)  %-25s  Insufficient node access: %s"
			,useron.number, useron.alias, cfg.node_arstr);
		logline(LOG_NOTICE,"+!",str);
		hangup();
		return(false); 
	}

	getnodedat(cfg.node_num,&thisnode,1);
	if(thisnode.misc&NODE_LOCK) {
		putnodedat(cfg.node_num,&thisnode);	/* must unlock! */
		if(!SYSOP && !(useron.exempt&FLAG('N'))) {
			bputs(text[NodeLocked]);
			safe_snprintf(str, sizeof(str), "(%04u)  %-25s  Locked node logon attempt"
				,useron.number,useron.alias);
			logline(LOG_NOTICE,"+!",str);
			hangup();
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

	if(useron.exempt&FLAG('H'))
		console|=CON_NO_INACT;

	if((useron.exempt&FLAG('Q') && useron.misc&QUIET))
		thisnode.status=NODE_QUIET;
	else
		thisnode.status=NODE_INUSE;
	action=thisnode.action=NODE_LOGN;
	thisnode.connection=node_connection;
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

	if((useron.misc & AUTOTERM)
		// User manually-enabled PETSCII, but they're logging in with an ANSI (auto-detected) terminal
		|| ((useron.misc & PETSCII) && (autoterm & ANSI))) {
		useron.misc &= ~(ANSI|RIP|CHARSET_FLAGS);
		useron.misc |= (AUTOTERM | autoterm);
	}

	if(!chk_ar(cfg.shell[useron.shell]->ar,&useron,&client)) {
		useron.shell=cfg.new_shell;
		if(!chk_ar(cfg.shell[useron.shell]->ar,&useron,&client)) {
			for(i=0;i<cfg.total_shells;i++)
				if(chk_ar(cfg.shell[i]->ar,&useron,&client))
					break;
			if(i==cfg.total_shells)
				useron.shell=0; 
		} 
	}

	logon_ml=useron.level;
	logontime=time(NULL);
	starttime=logontime;
	useron.logontime=(time32_t)logontime;
	last_ns_time=ns_time=useron.ns_time;
	// ns_time-=(useron.tlast*60); /* file newscan time == last logon time */

	if(!SYSOP && online==ON_REMOTE && !(sys_status&SS_QWKLOGON)) {
		rioctl(IOCM|ABORT);	/* users can't abort anything */
		rioctl(IOCS|ABORT); 
	}

	bputs(text[LoggingOn]);
	if(useron.rows != TERM_ROWS_AUTO)
		rows = useron.rows;
	if(useron.cols != TERM_COLS_AUTO)
		cols = useron.cols;
	update_nodeterm();
	if(tm.tm_mon + 1 == getbirthmonth(&cfg, useron.birth) && tm.tm_mday == getbirthday(&cfg, useron.birth)
		&& !(useron.rest&FLAG('Q'))) {
		if(text[HappyBirthday][0]) {
			bputs(text[HappyBirthday]);
			pause();
			CLS;
		}
		user_event(EVENT_BIRTHDAY); 
	}
	useron.ltoday++;

	gettimeleft();

	// TODO: GUEST accounts
	/* Inform the user of what's in their batch upload queue */
	{
		str_list_t ini = batch_list_read(&cfg, useron.number, XFER_BATCH_UPLOAD);
		str_list_t filenames = iniGetSectionList(ini, NULL);
		for(size_t i = 0; filenames[i] != NULL; i++) {
			const char* filename = filenames[i];
			file_t f = {{}};
			if(batch_file_get(&cfg, ini, filename, &f)) {
				bprintf(text[FileAddedToUlQueue], f.name, i + 1, cfg.max_batup);
				smb_freefilemem(&f);
			} else
				batch_file_remove(&cfg, useron.number, XFER_BATCH_UPLOAD, filename);
		}
		iniFreeStringList(ini);
		iniFreeStringList(filenames);
	}

	/* Remove defunct files from user's batch download queue and inform them of what's remaining */
	{
		str_list_t ini = batch_list_read(&cfg, useron.number, XFER_BATCH_DOWNLOAD);
		str_list_t filenames = iniGetSectionList(ini, NULL);
		for(size_t i = 0; filenames[i] != NULL; i++) {
			const char* filename = filenames[i];
			file_t f = {{}};
			if(!batch_file_load(&cfg, ini, filename, &f)
				|| !can_user_download(&cfg, f.dir, &useron, &client, /* reason: */NULL)) {
				lprintf(LOG_NOTICE, "Removing defunct file from user's batch download queue: %s", filename);
				batch_file_remove(&cfg, useron.number, XFER_BATCH_DOWNLOAD, filename);
			}
			else {
				char tmp2[256];
				getfilesize(&cfg, &f);
				bprintf(text[FileAddedToBatDlQueue]
					,f.name, i + 1, cfg.max_batdn
					,byte_estimate_to_str(f.cost, tmp, sizeof(tmp), 1, 1)
					,byte_estimate_to_str(f.size, tmp2, sizeof(tmp2), 1, 1)
					,sectostr((uint)(f.size / (uint64_t)cur_cps),str));
			}
			smb_freefilemem(&f);
		}
		iniFreeStringList(ini);
		iniFreeStringList(filenames);
	}
	if(!(sys_status&SS_QWKLOGON)) { 	 /* QWK Nodes don't go through this */

		if(cfg.sys_pwdays && useron.pass[0]
			&& (ulong)logontime>(useron.pwmod+((ulong)cfg.sys_pwdays*24UL*60UL*60UL))) {
			bprintf(text[TimeToChangePw],cfg.sys_pwdays);

			c=0;
			while(c < MAX(RAND_PASS_LEN, cfg.min_pwlen)) { 				/* Create random password */
				str[c]=sbbs_random(43)+'0';
				if(IS_ALPHANUMERIC(str[c]))
					c++;
			}
			str[c]=0;
			bprintf(text[YourPasswordIs],str);

			if(cfg.sys_misc&SM_PWEDIT && yesno(text[NewPasswordQ]))
				while(online) {
					bprintf(text[NewPasswordPromptFmt], cfg.min_pwlen, LEN_PASS);
					getstr(str,LEN_PASS,K_UPPER|K_LINE|K_TRIM);
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
				else {
					if(!text[NewUserPasswordVerify][0])
						break;
					bputs(text[NewUserPasswordVerify]);
				}
				console|=CON_R_ECHOX;
				getstr(tmp,LEN_PASS*2,K_UPPER);
				console&=~(CON_R_ECHOX|CON_L_ECHOX);
				if(strcmp(str,tmp)) {
					bputs(text[Wrong]); // Should be WrongPassword instead?
					continue; 
				}
				break; 
			}
			SAFECOPY(useron.pass,str);
			useron.pwmod=time32(NULL);
			putuserdatetime(useron.number, USER_PWMOD, useron.pwmod);
			bputs(text[PasswordChanged]);
			pause(); 
		}
		if(useron.ltoday>cfg.level_callsperday[useron.level]
			&& !(useron.exempt&FLAG('L'))) {
			bputs(text[NoMoreLogons]);
			safe_snprintf(str, sizeof(str), "(%04u)  %-25s  Out of logons"
				,useron.number,useron.alias);
			logline(LOG_NOTICE,"+!",str);
			hangup();
			return(false); 
		}
		if(useron.rest&FLAG('L') && useron.ltoday>1) {
			bputs(text[R_Logons]);
			safe_snprintf(str, sizeof(str), "(%04u)  %-25s  Out of logons"
				,useron.number,useron.alias);
			logline(LOG_NOTICE,"+!",str);
			hangup();
			return(false); 
		}
		kmode=(cfg.uq&UQ_NOEXASC)|K_TRIM;
		if(!(cfg.uq&UQ_NOUPRLWR))
			kmode|=K_UPRLWR;

		if(!(useron.rest&FLAG('G'))) {
			if(!useron.name[0] && ((cfg.uq&UQ_ALIASES && cfg.uq&UQ_REALNAME)
				|| cfg.uq&UQ_COMPANY))
				while(online) {
					if(cfg.uq&UQ_ALIASES && cfg.uq&UQ_REALNAME)
						bputs(text[EnterYourRealName]);
					else
						bputs(text[EnterYourCompany]);
					getstr(useron.name,LEN_NAME,kmode);
					if(cfg.uq&UQ_ALIASES && cfg.uq&UQ_REALNAME) {
						if(trashcan(useron.name,"name") || !useron.name[0]
							|| !strchr(useron.name,' ')
							|| strchr(useron.name,0xff)
							|| (cfg.uq&UQ_DUPREAL
								&& finduserstr(useron.number, USER_NAME
								,useron.name,0,0)))
							bputs(text[YouCantUseThatName]);
						else
							break; 
					}
					else
						break; 
				}
			if(cfg.uq&UQ_HANDLE && !useron.handle[0]) {
				SAFECOPY(useron.handle, useron.alias);
				while(online) {
					bputs(text[EnterYourHandle]);
					if(!getstr(useron.handle,LEN_HANDLE
						,K_LINE|K_EDIT|K_AUTODEL|kmode)
						|| strchr(useron.handle,0xff)
						|| (cfg.uq&UQ_DUPHAND
							&& finduserstr(useron.number, USER_HANDLE
							,useron.handle,0,0))
						|| trashcan(useron.handle,"name"))
						bputs(text[YouCantUseThatName]);
					else
						break; 
				} 
			}
			if(cfg.uq&UQ_LOCATION && !useron.location[0])
				while(online) {
					bputs(text[EnterYourCityState]);
					if(getstr(useron.location,LEN_LOCATION,kmode))
						break; 
				}
			if(cfg.uq&UQ_ADDRESS && !useron.address[0])
				while(online) {
					bputs(text[EnterYourAddress]);
					if(getstr(useron.address,LEN_ADDRESS,kmode))
						break; 
				}
			if(cfg.uq&UQ_ADDRESS && !useron.zipcode[0])
				while(online) {
					bputs(text[EnterYourZipCode]);
					if(getstr(useron.zipcode,LEN_ZIPCODE,K_UPPER|kmode))
						break; 
				}
			if(cfg.uq&UQ_PHONE && !useron.phone[0]) {
				if(text[CallingFromNorthAmericaQ][0])
					i=yesno(text[CallingFromNorthAmericaQ]);
				else
					i=0;
				while(online) {
					bputs(text[EnterYourPhoneNumber]);
					if(i) {
						if(gettmplt(useron.phone,cfg.sys_phonefmt
							,K_LINE|(cfg.uq&UQ_NOEXASC))<strlen(cfg.sys_phonefmt))
							 continue; 
					} else {
						if(getstr(useron.phone,LEN_PHONE
							,K_UPPER|(cfg.uq&UQ_NOEXASC)|K_TRIM)<5)
							continue; 
					}
					if(!trashcan(useron.phone,"phone"))
						break; 
				} 
			}
			if(!(cfg.uq&UQ_NONETMAIL) && !useron.netmail[0]) {
				while(online) {
					bputs(text[EnterNetMailAddress]);
					if(getstr(useron.netmail,LEN_NETMAIL,K_EDIT|K_AUTODEL|K_LINE|K_TRIM)
						&& !trashcan(useron.netmail,"email"))
						break;
				}
				if(useron.netmail[0] && cfg.sys_misc&SM_FWDTONET && !noyes(text[ForwardMailQ]))
					useron.misc|=NETMAIL;
				else 
					useron.misc&=~NETMAIL;
			}
			if(cfg.new_sif[0]) {
				safe_snprintf(str, sizeof(str), "%suser/%4.4u.dat",cfg.data_dir,useron.number);
				if(flength(str)<1L)
					create_sif_dat(cfg.new_sif,str); 
			} 
		}
	}	
	if(!online) {
		safe_snprintf(str, sizeof(str), "(%04u)  %-25s  Unsuccessful logon"
			,useron.number,useron.alias);
		logline(LOG_NOTICE,"+!",str);
		return(false); 
	}
	SAFECOPY(useron.modem,connection);
	SAFECOPY(useron.ipaddr, client_ipaddr);
	SAFECOPY(useron.comp, client_name);
	useron.logons++;
	putuserdat(&cfg,&useron);
	getmsgptrs();
	sys_status|=SS_USERON;          /* moved from further down */

	mqtt_user_login(mqtt, &client);

	if(useron.rest&FLAG('Q')) {
		safe_snprintf(str, sizeof(str), "(%04u)  %-25s  QWK Network Connection"
			,useron.number,useron.alias);
		logline("++",str);
		return(true); 
	}

	/********************/
	/* SUCCESSFUL LOGON */
	/********************/
	totallogons=logonstats();
	safe_snprintf(str, sizeof(str), "(%04u)  %-25s  %sLogon %u - %u"
		,useron.number,useron.alias, (sys_status&SS_FASTLOGON) ? "Fast-":"", totallogons,useron.ltoday);
	logline("++",str);

	if(!(sys_status&SS_QWKLOGON) && cfg.logon_mod[0])
		exec_bin(cfg.logon_mod,&main_csi);

	if(thisnode.status!=NODE_QUIET && (!REALSYSOP || cfg.sys_misc&SM_SYSSTAT)) {
		int file;
		safe_snprintf(path, sizeof(path), "%slogon.lst",cfg.data_dir);
		if((file=nopen(path,O_WRONLY|O_CREAT|O_APPEND))==-1) {
			errormsg(WHERE,ERR_OPEN,path,O_RDWR|O_CREAT|O_APPEND);
			return(false);
		}
		getuserstr(&cfg, useron.number, USER_NOTE, useron.note, sizeof(useron.note));
		getuserstr(&cfg, useron.number, USER_LOCATION, useron.location, sizeof(useron.location));
		safe_snprintf(str, sizeof(str), text[LastFewCallersFmt],cfg.node_num
			,totallogons,useron.alias
			,cfg.sys_misc&SM_LISTLOC ? useron.location : useron.note
			,tm.tm_hour,tm.tm_min
			,connection,useron.ltoday > 999 ? 999 : useron.ltoday);
		int wr = write(file,str,strlen(str));
		close(file);
		if(wr < 0)
			errormsg(WHERE, ERR_WRITE, path, strlen(str));
	}

	if(cfg.sys_logon[0]) {				/* execute system logon event */
		lprintf(LOG_DEBUG, "executing logon event: %s", cfg.sys_logon);
		external(cmdstr(cfg.sys_logon,nulstr,nulstr,NULL),EX_STDOUT); /* EX_SH */
	}

	if(sys_status&SS_QWKLOGON)
		return(true);

	sys_status|=SS_PAUSEON;	/* always force pause on during this section */
	mailw=getmail(&cfg,useron.number,/* Sent: */FALSE, /* attr: */0);
	mailr=getmail(&cfg,useron.number,/* Sent: */FALSE, /* attr: */MSG_READ);

	if(!(cfg.sys_misc&SM_NOSYSINFO)) {
		bprintf(text[SiSysName],cfg.sys_name);
		//bprintf(text[SiNodeNumberName],cfg.node_num,cfg.node_name);
		bprintf(text[LiUserNumberName],useron.number,useron.alias);
		bprintf(text[LiLogonsToday],useron.ltoday
			,cfg.level_callsperday[useron.level]);
		bprintf(text[LiTimeonToday],useron.ttoday
			,cfg.level_timeperday[useron.level]+useron.min);
		bprintf(text[LiMailWaiting],mailw, mailw-mailr);
		bprintf(text[LiSysopIs]
			, text[sysop_available(&cfg) ? LiSysopAvailable : LiSysopNotAvailable]);
		newline();
	}

	if(sys_status&SS_EVENT)
		bprintf(text[ReducedTime],timestr(event_time));
	getnodedat(cfg.node_num,&thisnode,1);
	thisnode.misc&=~(NODE_AOFF|NODE_POFF);
	if(useron.chat&CHAT_NOACT)
		thisnode.misc|=NODE_AOFF;
	if(useron.chat&CHAT_NOPAGE)
		thisnode.misc|=NODE_POFF;
	putnodedat(cfg.node_num,&thisnode);

	getsmsg(useron.number); 		/* Moved from further down */
	sync();
	c=0;
	for(i=1;i<=cfg.sys_nodes;i++) {
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
				SAFEPRINTF2(str,"(%04u)  %-25s  On more than one node at the same time"
					,useron.number,useron.alias);
				logline(LOG_NOTICE,"+!",str);
				bputs(text[UserOnTwoNodes]);
				hangup();
				return(false); 
			}
		}
	}
	for(i=1;i<=cfg.sys_nodes;i++) {
		if(i!=cfg.node_num) {
			getnodedat(i,&node,0);
			if(thisnode.status!=NODE_QUIET
				&& (node.status==NODE_INUSE || node.status==NODE_QUIET)
				&& !(node.misc&NODE_AOFF) && node.useron!=useron.number) {
				putnmsg(i, format_text(NodeLoggedOnAtNbps
						,cfg.node_num
						,thisnode.misc&NODE_ANON ? text[UNKNOWN_USER] : useron.alias
						,connection));
			} 
		}
	}

	if(cfg.sys_exp_warn && useron.expire && useron.expire>now /* Warn user of coming */
		&& (useron.expire-now)/(1440L*60L)<=cfg.sys_exp_warn) /* expiration */
		bprintf(text[AccountWillExpireInNDays],(useron.expire-now)/(1440L*60L));

	if(criterrs && SYSOP)
		bprintf(text[CriticalErrors],criterrs);
	if((i=getuserxfers(&cfg, /* from: */NULL, useron.number)) != 0)
		bprintf(text[UserXferForYou],i,i>1 ? "s" : nulstr); 
	if((i=getuserxfers(&cfg, useron.alias, /* to: */0)) != 0)
		bprintf(text[UnreceivedUserXfer],i,i>1 ? "s" : nulstr);
	sync();
	sys_status&=~SS_PAUSEON;	/* Turn off the pause override flag */
	if(online==ON_REMOTE)
		rioctl(IOSM|ABORT);		/* Turn abort ability on */
	if(text[ReadYourMailNowQ][0] && mailw) {
		if((mailw == mailr && !noyes(text[ReadYourMailNowQ]))
			|| (mailw != mailr && yesno(text[ReadYourMailNowQ]))) {
				uint32_t user_mail = useron.mail & ~MAIL_LM_MODE;
				int result = readmail(useron.number, MAIL_YOUR, useron.mail & MAIL_LM_MODE);
				user_mail |= result & MAIL_LM_MODE;
				if(user_mail != useron.mail)
					putusermail(&cfg, useron.number, useron.mail = user_mail);
			}
	}
	if(usrgrps && useron.misc&ASK_NSCAN && text[NScanAllGrpsQ][0] && yesno(text[NScanAllGrpsQ]))
		scanallsubs(SCAN_NEW);
	if(usrgrps && useron.misc&ASK_SSCAN && text[SScanAllGrpsQ][0] && yesno(text[SScanAllGrpsQ]))
		scanallsubs(SCAN_TOYOU|SCAN_UNREAD);
	return(true);
}

/****************************************************************************/
/* Checks the system dsts.ini to see if it is a new day, if it is, all the  */
/* nodes' and the system's csts.tab are added to, and the dsts.ini's daily  */
/* stats are cleared. Also increments the logon values in dsts.ini if       */
/* applicable.                                                              */
/****************************************************************************/
uint sbbs_t::logonstats()
{
	char msg[256];
    char path[MAX_PATH+1];
    FILE* csts;
	FILE* dsts;
    uint i;
    stats_t stats;
	node_t	node;
	struct tm tm, update_tm;

	sys_status&=~SS_DAILY;
	if(!getstats(&cfg, 0, &stats)) {
		errormsg(WHERE, ERR_READ, "system stats");
		return 0;
	}

	now=time(NULL);
	if(stats.date > now+(24L*60L*60L)) /* More than a day in the future? */
		errormsg(WHERE,ERR_CHK,"Daily stats date/time stamp", (ulong)stats.date);
	if(localtime_r(&stats.date, &update_tm)==NULL)
		return(0);
	if(localtime_r(&now,&tm)==NULL)
		return(0);
	if((tm.tm_mday>update_tm.tm_mday && tm.tm_mon==update_tm.tm_mon)
		|| tm.tm_mon>update_tm.tm_mon || tm.tm_year>update_tm.tm_year) {

		safe_snprintf(msg, sizeof(msg), "New Day - Prev: %s ",timestr(stats.date));
		logline(LOG_NOTICE, "!=", msg);

		sys_status|=SS_DAILY;       /* New Day !!! */
		safe_snprintf(path, sizeof(path), "%slogon.lst",cfg.data_dir);    /* Truncate logon list (LEGACY) */
		int file;
		if((file=nopen(path,O_TRUNC|O_CREAT|O_WRONLY))==-1) {
			errormsg(WHERE,ERR_OPEN,path,O_TRUNC|O_CREAT|O_WRONLY);
			return(0L); 
		}
		close(file);
		for(i=0;i<=cfg.sys_nodes;i++) {
			if(i) {     /* updating a node */
				getnodedat(i,&node,1);
				node.misc|=NODE_EVENT;
				putnodedat(i,&node); 
			}
			if((dsts = fopen_dstats(&cfg, i, /* for_write: */TRUE)) == NULL) /* doesn't have stats yet */
				continue;

			if((csts = fopen_cstats(&cfg, i, /* for_write: */TRUE)) == NULL) {
				fclose_dstats(dsts);
				errormsg(WHERE, ERR_OPEN, "csts.tab", i);
				continue;
			}

			if(!fread_dstats(dsts, &stats)) {
				errormsg(WHERE, ERR_READ, "dsts.ini", i);
			} else {
				stats.date = time(NULL);
				fwrite_cstats(csts, &stats);
				fclose_cstats(csts);
				rolloverstats(&stats);
				fwrite_dstats(dsts, &stats, __FUNCTION__);
			}
			fclose_dstats(dsts);
		} 
	}

	if(cfg.node_num==0)	/* called from event_thread() */
		return(0);

	if(thisnode.status==NODE_QUIET)       /* Quiet users aren't counted */
		return(0);

	if(REALSYSOP && !(cfg.sys_misc&SM_SYSSTAT))
		return(0);

	for(i=0;i<2;i++) {
		FILE* fp = fopen_dstats(&cfg, i ? 0 : cfg.node_num, /* for_write: */TRUE);
		if(fp == NULL) {
			errormsg(WHERE, ERR_OPEN, "dsts.ini", i);
			return(0L); 
		}
		if(!fread_dstats(fp, &stats)) {
			errormsg(WHERE, ERR_READ, "dsts.ini", i);
		} else {
			stats.today.logons++;
			stats.total.logons++;
			fwrite_dstats(fp, &stats, __FUNCTION__);
		}
		fclose_dstats(fp);
	}

	return(stats.logons);
}
