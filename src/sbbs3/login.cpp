/* Synchronet user login routine */

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

const char* sbbs_t::parse_login(const char* str)
{
	sys_status &= ~(SS_QWKLOGON | SS_FASTLOGON);

	if (*str == '*') {
		sys_status |= SS_QWKLOGON;
		return str + 1;
	}

	if (*str == '!') {
		sys_status |= SS_FASTLOGON;
		return str + 1;
	}
	return str;
}

// -----------------------------------------------------------
// Interactive user login, called from Baja or JS login module
// -----------------------------------------------------------
int sbbs_t::login(const char *username, const char *pw_prompt, const char* user_pw, const char* sys_pw)
{
	char str[128];
	long useron_misc = useron.misc;

	username = parse_login(username);

	useron.number = find_login_id(&cfg, username);
	if (useron.number) {
		getuserdat(&cfg, &useron);
		if (useron.number && useron.misc & (DELETED | INACTIVE))
			useron.number = 0;
	}

	if (!useron.number) {
		if ((cfg.sys_login & LOGIN_PWPROMPT) && pw_prompt != NULL) {
			SAFECOPY(useron.alias, username);
			bputs(pw_prompt);
			console |= CON_R_ECHOX;
			getstr(str, LEN_PASS * 2, K_UPPER | K_LOWPRIO | K_TAB);
			// We don't care about the password in this case, we already know it's a bad user-ID
			console &= ~(CON_R_ECHOX | CON_L_ECHOX);
			badlogin(useron.alias, str);
			bputs(text[InvalidLogon]);
			if (cfg.sys_misc & SM_ECHO_PW)
				llprintf(LOG_NOTICE, "+!", "(%04u)  %-25s  FAILED Login-ID attempt, Password: '%s'"
				         , 0, useron.alias, str);
			else
				llprintf(LOG_NOTICE, "+!", "(%04u)  %-25s  FAILED Login-ID attempt"
				         , 0, useron.alias);
		} else {
			badlogin(username, NULL);
			bputs(text[UnknownUser]);
			llprintf(LOG_NOTICE, "+!", "Unknown User '%s'", username);
		}
		useron.misc = useron_misc;
		return LOGIC_FALSE;
	}

	if (!online) {
		useron.number = 0;
		return LOGIC_FALSE;
	}

	if (useron.pass[0] || user_is_sysop(&useron)) {
		if (user_pw != NULL)
			SAFECOPY(str, user_pw);
		else {
			if (pw_prompt != NULL)
				bputs(pw_prompt);
			console |= CON_R_ECHOX;
			getstr(str, LEN_PASS * 2, K_UPPER | K_LOWPRIO | K_TAB);
			console &= ~(CON_R_ECHOX | CON_L_ECHOX);
		}
		if (!online) {
			useron.number = 0;
			return LOGIC_FALSE;
		}
		if (stricmp(useron.pass, str)) {
			badlogin(useron.alias, str);
			bputs(text[InvalidLogon]);
			if (cfg.sys_misc & SM_ECHO_PW)
				llprintf(LOG_NOTICE, "+!", "(%04u)  %-25s  FAILED Password attempt: '%s' expected: '%s'"
				         , useron.number, useron.alias, str, useron.pass);
			else
				llprintf(LOG_NOTICE, "+!", "(%04u)  %-25s  FAILED Password attempt"
				         , useron.number, useron.alias);
			useron.number = 0;
			useron.misc = useron_misc;
			return LOGIC_FALSE;
		}
		if (user_is_sysop(&useron) && (cfg.sys_misc & SM_SYSPASSLOGIN) && (cfg.sys_misc & SM_R_SYSOP) && !chksyspass(sys_pw)) {
			bputs(text[InvalidLogon]);
			useron.number = 0;
			useron.misc = useron_misc;
			return LOGIC_FALSE;
		}
	}

	register_login();

	return LOGIC_TRUE;
}

void sbbs_t::badlogin(const char* user, const char* passwd, const char* protocol, xp_sockaddr* addr, bool delay)
{
	char            tmp[128];
	char            reason[128];
	char            host_name[128];
	ulong           count;
	login_attempt_t attempt;

	if (protocol == NULL)
		protocol = connection;
	if (addr == NULL)
		addr = &client_addr;

	SAFECOPY(host_name, STR_NO_HOSTNAME);
	socklen_t addr_len = sizeof(*addr);
	SAFEPRINTF(reason, "%s LOGIN", protocol);
	count = loginFailure(startup->login_attempt_list, addr, protocol, user, passwd, &attempt);
	if (count > 1)
		lprintf(LOG_NOTICE, "!%lu " STR_FAILED_LOGIN_ATTEMPTS " in %s"
		        , count, duration_estimate_to_vstr(attempt.time - attempt.first, tmp, sizeof tmp, 1, 1));
	mqtt_user_login_fail(mqtt, &client, user);
	if (user != NULL && startup->login_attempt.hack_threshold && count >= startup->login_attempt.hack_threshold) {
		getnameinfo(&addr->addr, addr_len, host_name, sizeof(host_name), NULL, 0, NI_NAMEREQD);
		::hacklog(&cfg, mqtt, reason, user, passwd, host_name, addr);
#ifdef _WIN32
		if (startup->sound.hack[0] && !sound_muted(&cfg))
			PlaySound(startup->sound.hack, NULL, SND_ASYNC | SND_FILENAME);
#endif
	}
	if (startup->login_attempt.filter_threshold && count >= startup->login_attempt.filter_threshold) {
		char ipaddr[INET6_ADDRSTRLEN];
		inet_addrtop(addr, ipaddr, sizeof(ipaddr));
		getnameinfo(&addr->addr, addr_len, host_name, sizeof(host_name), NULL, 0, NI_NAMEREQD);
		snprintf(reason, sizeof reason, "%lu " STR_FAILED_LOGIN_ATTEMPTS " in %s"
		         , count, duration_estimate_to_str(attempt.time - attempt.first, tmp, sizeof tmp, 1, 1));
		filter_ip(&cfg, protocol, reason, host_name, ipaddr, user, /* fname: */ NULL, startup->login_attempt.filter_duration);
	}

	if (delay)
		mswait(startup->login_attempt.delay);
}
