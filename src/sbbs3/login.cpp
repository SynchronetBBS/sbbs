/* Synchronet user login routine */

/* $Id: login.cpp,v 1.31 2020/04/26 06:32:05 rswindell Exp $ */

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
#include "cmdshell.h"

char* sbbs_t::parse_login(char* str)
{
	sys_status &= ~(SS_QWKLOGON|SS_FASTLOGON);

	if(*str == '*') {
		sys_status |= SS_QWKLOGON;
		return str + 1;
	}

	if(*str == '!') {
		sys_status |= SS_FASTLOGON;
		return str + 1;
	}
	return str;
}

int sbbs_t::login(char *username, char *pw_prompt, const char* user_pw, const char* sys_pw)
{
	char	str[128];
	char 	tmp[512];
	long	useron_misc=useron.misc;

	useron.number=0;
	username = parse_login(username);

	if(!(cfg.node_misc&NM_NO_NUM) && isdigit((uchar)username[0])) {
		useron.number=atoi(username);
		getuserdat(&cfg,&useron);
		if(useron.number && useron.misc&(DELETED|INACTIVE))
			useron.number=0; 
	}

	if(!useron.number) {
		useron.number=matchuser(&cfg,username,FALSE);
		if(!useron.number && (uchar)username[0]<0x7f && username[1]
			&& isalpha(username[0]) && strchr(username,' ') && cfg.node_misc&NM_LOGON_R)
			useron.number=userdatdupe(0,U_NAME,LEN_NAME,username);
		if(useron.number) {
			getuserdat(&cfg,&useron);
			if(useron.number && useron.misc&(DELETED|INACTIVE))
				useron.number=0; } 
	}

	if(!useron.number) {
		if((cfg.node_misc&NM_LOGON_P) && pw_prompt != NULL) {
			SAFECOPY(useron.alias,username);
			bputs(pw_prompt);
			console|=CON_R_ECHOX;
			getstr(str,LEN_PASS*2,K_UPPER|K_LOWPRIO|K_TAB);
			console&=~(CON_R_ECHOX|CON_L_ECHOX);
			badlogin(useron.alias, str);
			bputs(text[InvalidLogon]);	/* why does this always fail? */
			if(cfg.sys_misc&SM_ECHO_PW) 
				sprintf(tmp,"(%04u)  %-25s  FAILED Password attempt: '%s'"
					,0,useron.alias,str);
			else
				sprintf(tmp,"(%04u)  %-25s  FAILED Password attempt"
					,0,useron.alias);
			logline(LOG_NOTICE,"+!",tmp); 
		} else {
			badlogin(username, NULL);
			bputs(text[UnknownUser]);
			sprintf(tmp,"Unknown User '%s'",username);
			logline(LOG_NOTICE,"+!",tmp); 
		}
		useron.misc=useron_misc;
		return(LOGIC_FALSE); 
	}

	if(!online) {
		useron.number=0;
		return(LOGIC_FALSE); 
	}

	if(useron.pass[0] || REALSYSOP) {
		if(user_pw != NULL)
			SAFECOPY(str, user_pw);
		else {
			if(pw_prompt != NULL)
				bputs(pw_prompt);
			console |= CON_R_ECHOX;
			getstr(str, LEN_PASS * 2, K_UPPER | K_LOWPRIO | K_TAB);
			console &= ~(CON_R_ECHOX | CON_L_ECHOX);
		}
		if(!online) {
			useron.number=0;
			return(LOGIC_FALSE); 
		}
		if(stricmp(useron.pass,str)) {
			badlogin(useron.alias, str);
			bputs(text[InvalidLogon]);
			if(cfg.sys_misc&SM_ECHO_PW) 
				sprintf(tmp,"(%04u)  %-25s  FAILED Password: '%s' Attempt: '%s'"
					,useron.number,useron.alias,useron.pass,str);
			else
				sprintf(tmp,"(%04u)  %-25s  FAILED Password attempt"
					,useron.number,useron.alias);
			logline(LOG_NOTICE,"+!",tmp);
			useron.number=0;
			useron.misc=useron_misc;
			return(LOGIC_FALSE); 
		}
		if(REALSYSOP && (cfg.sys_misc&SM_SYSPASSLOGIN) && !chksyspass(sys_pw)) {
			bputs(text[InvalidLogon]);
			useron.number=0;
			useron.misc=useron_misc;
			return(LOGIC_FALSE); 
		} 
	}

	return(LOGIC_TRUE);
}

void sbbs_t::badlogin(char* user, char* passwd, const char* protocol, xp_sockaddr* addr, bool delay)
{
	char reason[128];
	char host_name[128];
	ulong count;

	if(protocol == NULL)
		protocol = connection;
	if(addr == NULL)
		addr = &client_addr;

	SAFECOPY(host_name, STR_NO_HOSTNAME);
	socklen_t addr_len = sizeof(*addr);
	SAFEPRINTF(reason,"%s LOGIN", protocol);
	count=loginFailure(startup->login_attempt_list, addr, protocol, user, passwd);
	if(user!=NULL && startup->login_attempt.hack_threshold && count>=startup->login_attempt.hack_threshold) {
		getnameinfo(&addr->addr, addr_len, host_name, sizeof(host_name), NULL, 0, NI_NAMEREQD);
		::hacklog(&cfg, reason, user, passwd, host_name, addr);
	}
	if(startup->login_attempt.filter_threshold && count>=startup->login_attempt.filter_threshold) {
		char ipaddr[INET6_ADDRSTRLEN];
		inet_addrtop(addr, ipaddr, sizeof(ipaddr));
		getnameinfo(&addr->addr, addr_len, host_name, sizeof(host_name), NULL, 0, NI_NAMEREQD);
		SAFEPRINTF(reason, "- TOO MANY CONSECUTIVE FAILED LOGIN ATTEMPTS (%lu)", count);
		filter_ip(&cfg, protocol, reason, host_name, ipaddr, user, /* fname: */NULL);
	}

	if(delay)
		mswait(startup->login_attempt.delay);
}
