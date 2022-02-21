/* Synchronet answer "caller" function */

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
#include "telnet.h"
#include "ssl.h"

extern "C" void client_on(SOCKET sock, client_t* client, BOOL update);

bool sbbs_t::answer()
{
	char	str[MAX_PATH+1],str2[MAX_PATH+1],c;
	char 	tmp[MAX_PATH];
	char 	*ctmp;
	char 	path[MAX_PATH+1];
	int		i,l,in;
	struct tm tm;

	useron.number=0;
	answertime=logontime=starttime=now=time(NULL);
	/* Caller ID string is client IP address, by default (may be overridden later) */
	SAFECOPY(cid,client_ipaddr);

	memset(&tm,0,sizeof(tm));
    localtime_r(&now,&tm); 

	safe_snprintf(str,sizeof(str),"%s  %s %s %02d %u            Node %3u"
		,hhmmtostr(&cfg,&tm,str2)
		,wday[tm.tm_wday]
        ,mon[tm.tm_mon],tm.tm_mday,tm.tm_year+1900,cfg.node_num);
	logline("@ ",str);

	safe_snprintf(str,sizeof(str),"%s  %s [%s]", connection, client_name, client_ipaddr);
	logline("@+:",str);

	if(client_ident[0]) {
		safe_snprintf(str,sizeof(str),"Identity: %s",client_ident);
		logline("@*",str);
	}

	if(sys_status&SS_RLOGIN) {
		if(incom(1000)==0) {
			for(i=0;i<(int)sizeof(str)-1;i++) {
				in=incom(1000);
				if(in==0 || in==NOINP)
					break;
				str[i]=in;
			}
			str[i]=0;
			for(i=0;i<(int)sizeof(str2)-1;i++) {
				in=incom(1000);
				if(in==0 || in==NOINP)
					break;
				str2[i]=in;
			}
			str2[i]=0;
			for(i=0;i<(int)sizeof(terminal)-1;i++) {
				in=incom(1000);
				if(in==0 || in==NOINP)
					break;
				terminal[i]=in;
			}
			terminal[i]=0;
			lprintf(LOG_DEBUG,"RLogin: '%.*s' / '%.*s' / '%s'"
				,LEN_ALIAS*2,str
				,LEN_ALIAS*2,str2
				,terminal);
			SAFECOPY(rlogin_term, terminal);
			SAFECOPY(rlogin_name, parse_login(str2));
			SAFECOPY(rlogin_pass, str);
			/* Truncate terminal speed (e.g. "/57600") from terminal-type string 
			   (but keep full terminal type/speed string in rlogin_term): */
			truncstr(terminal,"/");
			useron.number = 0;
			if(rlogin_name[0])
				useron.number=matchuser(&cfg, rlogin_name, /* sysop_alias: */FALSE);
			if(useron.number) {
				getuserdat(&cfg,&useron);
				SAFEPRINTF(path,"%srlogin.cfg",cfg.ctrl_dir);
				if(!findstr(client.addr,path)) {
					SAFECOPY(tmp, rlogin_pass);
					for(i=0;i<3 && online;i++) {
						if(stricmp(tmp,useron.pass)) {
							if(cfg.sys_misc&SM_ECHO_PW)
								safe_snprintf(str,sizeof(str),"(%04u)  %-25s  FAILED Password attempt: '%s'"
									,useron.number,useron.alias,tmp);
							else
								safe_snprintf(str,sizeof(str),"(%04u)  %-25s  FAILED Password attempt"
									,useron.number,useron.alias);
							logline(LOG_NOTICE,"+!",str);
							badlogin(useron.alias, tmp);
							rioctl(IOFI);       /* flush input buffer */
							bputs(text[InvalidLogon]);
							bputs(text[PasswordPrompt]);
							console|=CON_R_ECHOX;
							getstr(tmp,LEN_PASS*2,K_UPPER|K_LOWPRIO|K_TAB);
							console&=~(CON_R_ECHOX|CON_L_ECHOX);
						}
						else {
							if(REALSYSOP && (cfg.sys_misc&SM_SYSPASSLOGIN) && (cfg.sys_misc&SM_R_SYSOP)) {
								rioctl(IOFI);       /* flush input buffer */
								if(!chksyspass())
									bputs(text[InvalidLogon]);
								else {
									i=0;
									break;
								}
							}
							else {
								i = 0;
								break;
							}
						}
					}
					if(i) {
						if(stricmp(tmp,useron.pass)) {
							if(cfg.sys_misc&SM_ECHO_PW)
								safe_snprintf(str,sizeof(str),"(%04u)  %-25s  FAILED Password attempt: '%s'"
									,useron.number,useron.alias,tmp);
							else
								safe_snprintf(str,sizeof(str),"(%04u)  %-25s  FAILED Password attempt"
									,useron.number,useron.alias);
							logline(LOG_NOTICE,"+!",str);
							badlogin(useron.alias, tmp);
							bputs(text[InvalidLogon]);
						}
						lprintf(LOG_DEBUG,"!CLIENT IP (%s) NOT LISTED in %s", client.addr, path);
						useron.number=0;
						hangup();
					}
				}
			}
			else {
				if(cfg.sys_misc&SM_ECHO_PW)
					lprintf(LOG_NOTICE, "RLogin !UNKNOWN USER: '%s' (password: %s)", rlogin_name, rlogin_pass);
				else
					lprintf(LOG_NOTICE, "RLogin !UNKNOWN USER: '%s'",rlogin_name);
				badlogin(rlogin_name, rlogin_pass);
			}
		}
		if(rlogin_name[0]==0) {
			lprintf(LOG_NOTICE,"!RLogin: No user name received");
			sys_status&=~SS_RLOGIN;
		}
	}

	if(!(telnet_mode&TELNET_MODE_OFF)) {
		/* Disable Telnet Terminal Echo */
		request_telnet_opt(TELNET_WILL,TELNET_ECHO);
		/* Will suppress Go Ahead */
		request_telnet_opt(TELNET_WILL,TELNET_SUP_GA);
		/* Retrieve terminal type and speed from telnet client --RS */
		request_telnet_opt(TELNET_DO,TELNET_TERM_TYPE);
		request_telnet_opt(TELNET_DO,TELNET_TERM_SPEED);
		request_telnet_opt(TELNET_DO,TELNET_SEND_LOCATION);
		request_telnet_opt(TELNET_DO,TELNET_NEGOTIATE_WINDOW_SIZE);
		request_telnet_opt(TELNET_DO,TELNET_NEW_ENVIRON);
	}
#ifdef USE_CRYPTLIB
	if(sys_status&SS_SSH) {
		tmp[0]=0;
		pthread_mutex_lock(&ssh_mutex);
		ctmp = get_crypt_attribute(ssh_session, CRYPT_SESSINFO_USERNAME);
		if (ctmp) {
			SAFECOPY(rlogin_name, parse_login(ctmp));
			free_crypt_attrstr(ctmp);
			ctmp = get_crypt_attribute(ssh_session, CRYPT_SESSINFO_PASSWORD);
			if (ctmp) {
				SAFECOPY(tmp, ctmp);
				free_crypt_attrstr(ctmp);
			}
			pthread_mutex_unlock(&ssh_mutex);
			lprintf(LOG_DEBUG,"SSH login: '%s'", rlogin_name);
		}
		else {
			rlogin_name[0] = 0;
			pthread_mutex_unlock(&ssh_mutex);
		}
		useron.number=matchuser(&cfg, rlogin_name, /* sysop_alias: */FALSE);
		if(useron.number) {
			getuserdat(&cfg,&useron);
			for(i=0;i<3 && online;i++) {
				if(stricmp(tmp,useron.pass)) {
					if(cfg.sys_misc&SM_ECHO_PW)
						safe_snprintf(str,sizeof(str),"(%04u)  %-25s  FAILED Password attempt: '%s'"
							,useron.number,useron.alias,tmp);
					else
						safe_snprintf(str,sizeof(str),"(%04u)  %-25s  FAILED Password attempt"
							,useron.number,useron.alias);
					logline(LOG_NOTICE,"+!",str);
					badlogin(useron.alias, tmp);
					rioctl(IOFI);       /* flush input buffer */
					bputs(text[InvalidLogon]);
					bputs(text[PasswordPrompt]);
					console|=CON_R_ECHOX;
					getstr(tmp,LEN_PASS*2,K_UPPER|K_LOWPRIO|K_TAB);
					console&=~(CON_R_ECHOX|CON_L_ECHOX);
				}
				else {
					SAFECOPY(rlogin_pass, tmp);
					if(REALSYSOP && (cfg.sys_misc&SM_SYSPASSLOGIN) && (cfg.sys_misc&SM_R_SYSOP)) {
						rioctl(IOFI);       /* flush input buffer */
						if(!chksyspass())
							bputs(text[InvalidLogon]);
						else {
							i=0;
							break;
						}
					}
					else {
						i = 0;
						break;
					}
				}
			}
			if(i) {
				if(stricmp(tmp,useron.pass)) {
					if(cfg.sys_misc&SM_ECHO_PW)
						safe_snprintf(str,sizeof(str),"(%04u)  %-25s  FAILED Password attempt: '%s'"
							,useron.number,useron.alias,tmp);
					else
						safe_snprintf(str,sizeof(str),"(%04u)  %-25s  FAILED Password attempt"
							,useron.number,useron.alias);
					logline(LOG_NOTICE,"+!",str);
					badlogin(useron.alias, tmp);
					bputs(text[InvalidLogon]);
				}
				useron.number=0;
				hangup();
			}
		}
		else {
			if(cfg.sys_misc&SM_ECHO_PW)
				lprintf(LOG_NOTICE, "SSH !UNKNOWN USER: '%s' (password: %s)", rlogin_name, truncsp(tmp));
			else
				lprintf(LOG_NOTICE, "SSH !UNKNOWN USER: '%s'", rlogin_name);
			badlogin(rlogin_name, tmp);
		}
	}
#endif

	/* Detect terminal type */
	mswait(200);	// Allow some time for Telnet negotiation
	rioctl(IOFI);		/* flush input buffer */
	safe_snprintf(str, sizeof(str), "%s  %s", VERSION_NOTICE, COPYRIGHT_NOTICE);
	if(autoterm&PETSCII) {
		SAFECOPY(terminal, "PETSCII");
		outchar(FF);
		center(str);
	} else {	/* ANSI+ terminal detection */
		putcom( "\r\n"		/* locate cursor at column 1 */
				"\x1b[s"	/* save cursor position (necessary for HyperTerm auto-ANSI) */
				"\x1b[0c"	/* Request CTerm version */
    			"\x1b[255B"	/* locate cursor as far down as possible */
				"\x1b[255C"	/* locate cursor as far right as possible */
				"\b_"		/* need a printable char at this location to actually move cursor */
				"\x1b[6n"	/* Get cursor position */
				"\x1b[u"	/* restore cursor position */
				"\x1b[!_"	/* RIP? */
	#ifdef SUPPORT_ZUULTERM
				"\x1b[30;40m\xc2\x9f""Zuul.connection.write('\\x1b""Are you the gatekeeper?')\xc2\x9c"	/* ZuulTerm? */
	#endif
				"\r"		/* Move cursor left */
				"\xef\xbb\xbf"	// UTF-8 Zero-width non-breaking space
				"\x1b[6n"	/* Get cursor position (again) */
				"\x1b[0m_"	/* "Normal" colors */
				"\x1b[2J"	/* clear screen */
				"\x1b[H"	/* home cursor */
				"\xC"		/* clear screen (in case not ANSI) */
				"\r"		/* Move cursor left (in case previous char printed) */
				);
		i=l=0;
		row=0;
		lncntr=0;
		center(str);

		while(i++<50 && l<(int)sizeof(str)-1) { 	/* wait up to 5 seconds for response */
			c=incom(100)&0x7f;
			if(c==0)
				continue;
			i=0;
			if(l==0 && c!=ESC)	// response must begin with escape char
				continue;
			str[l++]=c;
			if(c=='R') {   /* break immediately if ANSI response */
				mswait(500);
				break; 
			}
		}

		while((c=(incom(100)&0x7f))!=0 && l<(int)sizeof(str)-1)
			str[l++]=c;
		str[l]=0;

		if(l) {
			truncsp(str);
			c_escape_str(str,tmp,sizeof(tmp)-1,TRUE);
			lprintf(LOG_DEBUG,"received terminal auto-detection response: '%s'", tmp);

			if(strstr(str,"RIPSCRIP")) {
				if(terminal[0]==0)
					SAFECOPY(terminal,"RIP");
				logline("@R",strstr(str,"RIPSCRIP"));
				autoterm|=(RIP|COLOR|ANSI); 
			}
	#ifdef SUPPORT_ZUULTERM
			else if(strstr(str,"Are you the gatekeeper?"))  {
				if(terminal[0]==0)
					SAFECOPY(terminal,"HTML");
				logline("@H",strstr(str,"Are you the gatekeeper?"));
				autoterm|=HTML;
			} 
	#endif

			char* tokenizer = NULL;
			char* p = strtok_r(str, "\x1b", &tokenizer);
			unsigned cursor_pos_report = 0;
			while(p != NULL) {
				int	x,y;

				if(terminal[0]==0)
					SAFECOPY(terminal,"ANSI");
				autoterm|=(ANSI|COLOR);
				if(sscanf(p, "[%u;%uR", &y, &x) == 2) {
					cursor_pos_report++;
					lprintf(LOG_DEBUG,"received ANSI cursor position report [%u]: %ux%u"
						,cursor_pos_report, x, y);
					if(cursor_pos_report == 1) {
						/* Sanity check the coordinates in the response: */
						if(x >= TERM_COLS_MIN && x <= TERM_COLS_MAX) cols=x; 
						if(y >= TERM_ROWS_MIN && y <= TERM_ROWS_MAX) rows=y;
					} else {	// second report
						if(x < 3)	// ZWNBSP didn't move cursor (more than one column)
							autoterm |= UTF8;
					}
				} else if(sscanf(p, "[=67;84;101;114;109;%u;%u", &x, &y) == 2 && *lastchar(p) == 'c') {
					lprintf(LOG_INFO,"received CTerm version report: %u.%u", x, y);
					cterm_version = (x*1000) + y;
				}
				p = strtok_r(NULL, "\x1b", &tokenizer);
			}
		}

		rioctl(IOFI); /* flush left-over or late response chars */

		if(!autoterm) {
			autoterm |= NO_EXASCII;
			if(str[0]) {
				c_escape_str(str,tmp,sizeof(tmp)-1,TRUE);
				lprintf(LOG_NOTICE,"terminal auto-detection failed, response: '%s'", tmp);
			}
		}
		if(terminal[0])
			lprintf(LOG_DEBUG, "auto-detected terminal type: %lux%lu %s", cols, rows, terminal);
		else
			SAFECOPY(terminal,"DUMB");
	}
	update_nodeterm();

	/* AutoLogon via IP or Caller ID here */
	if(!useron.number && !(sys_status&SS_RLOGIN)
		&& (startup->options&BBS_OPT_AUTO_LOGON) && client_ipaddr[0]) {
		useron.number=userdatdupe(0, U_IPADDR, LEN_IPADDR, client_ipaddr);
		if(useron.number) {
			getuserdat(&cfg, &useron);
			if(!(useron.misc&AUTOLOGON) || !(useron.exempt&FLAG('V')))
				useron.number=0;
		}
	}

	if(!online) {
		useron.number=0;
		return(false); 
	}

	if(!(telnet_mode&TELNET_MODE_OFF)) {
		/* Stop the input thread from writing to the telnet_* vars */
		pthread_mutex_lock(&input_thread_mutex);

		if(telnet_cmds_received) {
			if(stricmp(telnet_terminal,"sexpots")==0) {	/* dial-up connection (via SexPOTS) */
				SAFEPRINTF2(str,"%s connection detected at %lu bps", terminal, cur_rate);
				logline("@S",str);
				node_connection = (ushort)cur_rate;
				SAFEPRINTF(connection,"%lu",cur_rate);
				SAFECOPY(cid,"Unknown");
				SAFECOPY(client_name,"Unknown");
				if(telnet_location[0]) {			/* Caller-ID info provided */
					SAFEPRINTF(str, "CID: %s", telnet_location);
					logline("@*",str);
					SAFECOPY(cid,telnet_location);
					truncstr(cid," ");				/* Only include phone number in CID */
					char* p=telnet_location;
					FIND_WHITESPACE(p);
					SKIP_WHITESPACE(p);
					if(*p) {
						SAFECOPY(client_name,p);	/* CID name, if provided (maybe 'P' or 'O' if private or out-of-area) */
					}
				}
				SAFECOPY(client.addr,cid);
				SAFECOPY(client.host,client_name);
				client_on(client_socket,&client,TRUE /* update */);
			} else {
				if(telnet_location[0]) {			/* Telnet Location info provided */
					lprintf(LOG_INFO, "Telnet Location: %s", telnet_location);
					if(trashcan(telnet_location, "ip-silent")) {
						hangup();
						return false;
					}
					if(trashcan(telnet_location, "ip")) {
						lprintf(LOG_NOTICE, "%04d %s !TELNET LOCATION BLOCKED in ip.can: %s"
							,client_socket, client.protocol, telnet_location);
						hangup();
						return false;
					}
					SAFECOPY(cid, telnet_location);
				}
			}
			if(telnet_speed) {
				lprintf(LOG_INFO, "Telnet Speed: %lu bps", telnet_speed);
				cur_rate = telnet_speed;
				cur_cps = telnet_speed/10;
			}
			if(telnet_terminal[0])
				SAFECOPY(terminal, telnet_terminal);
			if(telnet_cols >= TERM_COLS_MIN && telnet_cols <= TERM_COLS_MAX)
				cols = telnet_cols;
			if(telnet_rows >= TERM_ROWS_MIN && telnet_rows <= TERM_ROWS_MAX)
				rows = telnet_rows;
		} else {
			lprintf(LOG_NOTICE, "no Telnet commands received, reverting to Raw TCP mode");
			telnet_mode |= TELNET_MODE_OFF;
			client.protocol = "Raw";
			client_on(client_socket, &client,/* update: */true);
			SAFECOPY(connection, client.protocol);
			node_connection = NODE_CONNECTION_RAW;
		}
		pthread_mutex_unlock(&input_thread_mutex);
	}
	lprintf(LOG_INFO, "terminal type: %lux%lu %s", cols, rows, terminal);
	SAFECOPY(client_ipaddr, cid);	/* Over-ride IP address with Caller-ID info */
	SAFECOPY(useron.comp,client_name);

	if(!useron.number 
		&& rlogin_name[0]!=0 
		&& !(cfg.sys_misc&SM_CLOSED) 
		&& !matchuser(&cfg, rlogin_name, /* Sysop alias: */FALSE)
		&& !::trashcan(&cfg, rlogin_name, "name")) {
		lprintf(LOG_INFO, "%s !UNKNOWN specified username: '%s', starting new user signup", client.protocol,rlogin_name);
		bprintf("%s: %s\r\n", text[UNKNOWN_USER], rlogin_name);
		newuser();
	}

	if(!useron.number) {	/* manual/regular logon */

		/* Display ANSWER screen */
		rioctl(IOSM|PAUSE);
		sys_status|=SS_PAUSEON;
		menu("../answer");	// Should use P_NOABORT ?
		sys_status&=~SS_PAUSEON;
		exec_bin(cfg.login_mod,&main_csi);
	} else	/* auto logon here */
		if(logon()==false)
			return(false);

	if(!useron.number)
		hangup();

	if(!online) 
		return(false); 

	if(!(sys_status&SS_USERON)) {
		errormsg(WHERE,ERR_CHK,"User not logged on",sys_status);
		hangup();
		return(false); 
	}

	if(useron.pass[0])
		loginSuccess(startup->login_attempt_list, &client_addr);

	return(true);
}
