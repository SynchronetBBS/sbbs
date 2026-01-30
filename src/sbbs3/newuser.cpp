/* Synchronet new user routine */

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
#include "petdefs.h"
#include "cmdshell.h"
#include "filedat.h"

/****************************************************************************/
/* This function is invoked when a user enters "NEW" at the NN: prompt		*/
/* Prompts user for personal information and then sends feedback to sysop.  */
/* Called from function waitforcall											*/
/****************************************************************************/
bool sbbs_t::newuser()
{
	char c, str[512];
	char tmp[512];
	int  i;

	max_socket_inactivity = startup->max_newuser_inactivity;
	bputs(text[StartingNewUserRegistration]);
	user_login_state = user_registering;
	logline("N", "New user registration begun");
	getnodedat(cfg.node_num, &thisnode);
	if (thisnode.misc & NODE_LOCK) {
		bputs(text[NodeLocked]);
		logline(LOG_WARNING, "N!", "New user locked node logon attempt");
		hangup();
		return false;
	}

	if (cfg.sys_misc & SM_CLOSED) {
		bputs(text[NoNewUsers]);
		logline(LOG_WARNING, "N!", "System is closed to new users");
		hangup();
		return false;
	}
	if (getnodedat(cfg.node_num, &thisnode, true)) {
		thisnode.status = NODE_NEWUSER;
		thisnode.connection = node_connection;
		putnodedat(cfg.node_num, &thisnode);
	}
	memset(&useron, 0, sizeof(user_t));     /* Initialize user info to null */
	newuserdefaults(&cfg, &useron);
	if (cfg.new_pass[0] && online == ON_REMOTE) {
		c = 0;
		while (++c < 4) {
			bputs(text[NewUserPasswordPrompt]);
			getstr(str, 40, K_UPPER | K_TRIM);
			if (!strcmp(str, cfg.new_pass))
				break;
			llprintf(LOG_NOTICE, "N!", "NUP Attempted: '%s'", str);
		}
		if (c == 4) {
			logline(LOG_NOTICE, "N!", "Failed all New User Password attempts");
			menu("../nupguess", P_NOABORT | P_NOERROR);
			hangup();
			return false;
		}
	}
	SAFECOPY(useron.alias, rlogin_name);
	truncsp(useron.alias);

	/* Sets defaults per sysop config */
	SAFECOPY(useron.host, client_name);  /* hostname or CID name */
	SAFECOPY(useron.ipaddr, cid);            /* IP address or CID number */
	if ((i = finduserstr(0, USER_IPADDR, cid, /* del */ true)) != 0) {    /* Duplicate IP address */
		SAFEPRINTF2(useron.comment, "Warning: same IP address as user #%d %s"
		            , i, username(&cfg, i, str));
		logline(LOG_NOTICE, "N!", useron.comment);
	}

	SAFECOPY(useron.connection, connection);
	if (!lastuser(&cfg)) {   /* Automatic sysop access for first user */
		bprintf("Creating sysop account... System password required.\r\n");
		if (!chksyspass())
			return false;
		newsysop(&cfg, &useron);
	}

	sys_status |= SS_NEWUSER;

	if (exec_mod("new user prompts", cfg.newuser_prompts_mod) != 0)
		return false;
	if (!online)
		return false;
	truncsp(useron.alias);
	if (useron.alias[0] == '\0') {
		errormsg(WHERE, ERR_CHK, "New user alias (blank)", 0);
		return false;
	}
	if (!check_name(&cfg, useron.alias, /* unique: */true)) {
		errormsg(WHERE, ERR_CHK, "New user alias (invalid or duplicate)", 0, useron.alias);
		return false;
	}
	truncsp(useron.name);
	if (useron.name[0] == '\0') {
		lprintf(LOG_NOTICE, "New user name blank, using alias");
		SAFECOPY(useron.name, useron.alias);
	}
	bool dupe_name = finduserstr(0, USER_NAME, useron.name) > 0;
	if (!check_name(&cfg, useron.name, /* unique: */true)
		|| !check_realname(&cfg, useron.name)
		|| ((cfg.uq & UQ_ALIASES) && (cfg.uq & UQ_REALNAME) && (cfg.uq & UQ_DUPREAL) && dupe_name)) {
		errormsg(WHERE, ERR_CHK, "New user name (invalid or duplicate)", 0, useron.name);
		return false;
	}
	if (dupe_name) {
		lprintf(LOG_NOTICE, "New user real name '%s' is a duplicate, setting O Restriction", useron.name);
		useron.rest |= FLAG('O'); // Can't post or send netmail using real name (it's a duplicate)
	}
	truncsp(useron.handle);
	if (useron.handle[0] == '\0') {
		lprintf(LOG_NOTICE, "New user handle blank, using alias");
		SAFECOPY(useron.handle, useron.alias);
		truncsp(useron.handle);
	}
	if (((cfg.uq & UQ_HANDLE) && (cfg.uq & UQ_DUPHAND) && finduserstr(0, USER_HANDLE, useron.handle))
	    || trashcan(useron.handle, "name")) {
		errormsg(WHERE, ERR_CHK, "New user handle (invalid or duplicate)", 0, useron.handle);
		return false;
	}
	truncsp(useron.address);
	truncsp(useron.location);
	truncsp(useron.zipcode);
	truncsp(useron.phone);
	truncsp(useron.birth);
	truncsp(useron.netmail);
	llprintf("N", "New user: %s (real name: %s, handle: %s)", useron.alias, useron.name, useron.handle);

	/* Default editor (moved here, after terminal type setup Jan-2003) */
	if (!set_editor(cfg.new_xedit))
		set_editor("");
	if (!set_shell(cfg.new_shell))
		set_shell(0);

	bool invoked;
	if (exec_mod("new user info", cfg.newuser_info_mod, &invoked) != 0 && invoked)
		return false;
	if (!online)
		return false;
	answertime = time(NULL);      /* could take 10 minutes to get this far */

	if (rlogin_pass[0] && chkpass(rlogin_pass, &useron)) {
		term->newline();
		SAFECOPY(useron.pass, rlogin_pass);
		strupr(useron.pass);    /* passwords are case insensitive, but assumed (in some places) to be uppercase in the user database */
	}
	else {
		if (rlogin_pass[0]) {
			if (cfg.sys_misc & SM_ECHO_PW)
				llprintf(LOG_NOTICE, "N!", "Rejected RLogin password for new user: '%s'", rlogin_pass);
			else
				llprintf(LOG_NOTICE, "N!", "Rejected RLogin password for new user");
		}
		lprintf(LOG_INFO, "Generating a random password for new user");
		do {
			c = 0;
			while (c < MAX(RAND_PASS_LEN, cfg.min_pwlen)) {              /* Create random password */
				useron.pass[c] = sbbs_random(43) + '0';
				if (IS_ALPHANUMERIC(useron.pass[c]))
					c++;
			}
			useron.pass[c] = 0;
		} while (!check_pass(&cfg, useron.pass, &useron, /* unique: */false, /* reason: */NULL));

		bprintf(text[YourPasswordIs], useron.pass);

		if (cfg.sys_misc & SM_PWEDIT && text[NewPasswordQ][0] && yesno(text[NewPasswordQ]))
			while (online) {
				bprintf(text[NewPasswordPromptFmt], cfg.min_pwlen, LEN_PASS);
				getstr(str, LEN_PASS, K_UPPER | K_LINE | K_TRIM);
				truncsp(str);
				if (chkpass(str, &useron)) {
					SAFECOPY(useron.pass, str);
					term->newline();
					bprintf(text[YourPasswordIs], useron.pass);
					break;
				}
				term->newline();
			}

		c = 0;
		while (online && text[NewUserPasswordVerify][0]) {
			bputs(text[NewUserPasswordVerify]);
			console |= CON_PASSWORD;
			str[0] = 0;
			getstr(str, LEN_PASS * 2, K_UPPER);
			console &= ~CON_PASSWORD;
			if (!strcmp(str, useron.pass))
				break;
			if (cfg.sys_misc & SM_ECHO_PW)
				llprintf(LOG_NOTICE, "N!", "FAILED Password verification: '%s' instead of '%s'"
				            , str
				            , useron.pass);
			else
				logline(LOG_NOTICE, "N!", "FAILED Password verification");
			if (++c == 4) {
				logline(LOG_NOTICE, "N!", "Couldn't figure out password.");
				hangup();
			}
			bputs(text[IncorrectPassword]);
			bprintf(text[YourPasswordIs], useron.pass);
		}
	}

	if (!online)
		return false;
	if (cfg.new_magic[0] && text[MagicWordPrompt][0]) {
		bputs(text[MagicWordPrompt]);
		str[0] = 0;
		getstr(str, 50, K_UPPER | K_TRIM);
		if (strcmp(str, cfg.new_magic)) {
			bputs(text[FailedMagicWord]);
			llprintf("N!", "failed magic word: '%s'", str);
			hangup();
		}
		if (!online)
			return false;
	}

	bputs(text[CheckingSlots]);

	if ((i = newuserdat(&cfg, &useron)) != 0) {
		SAFEPRINTF(str, "user record #%u", useron.number);
		errormsg(WHERE, ERR_CREATE, str, i);
		hangup();
		return false;
	}
	llprintf(nulstr, "Created user record #%u: %s", useron.number, useron.alias);

	snprintf(str, sizeof(str), "%u\t%s"
	         , useron.number, useron.alias);
	char topic[128];
	snprintf(topic, sizeof(topic), "newuser/%s", client.protocol);
	strlwr(topic);
	mqtt_pub_timestamped_msg(mqtt, TOPIC_BBS_ACTION, topic, answertime, str);

	if (cfg.new_sif[0]) {
		SAFEPRINTF2(str, "%suser/%4.4u.dat", cfg.data_dir, useron.number);
		create_sif_dat(cfg.new_sif, str);
	}

	if (!(cfg.uq & UQ_NODEF))
		user_config(&useron);

	delallmail(useron.number, MAIL_ANY);

	if (useron.number != 1 && cfg.valuser) {
		menu("../feedback", P_NOABORT | P_NOERROR);
		safe_snprintf(str, sizeof(str), text[NewUserFeedbackHdr]
		              , nulstr, getage(&cfg, useron.birth), useron.gender, useron.birth
		              , useron.name, useron.phone, useron.host, useron.connection);
		email(cfg.valuser, str, text[NewUserValEmailSubj], WM_SUBJ_RO | WM_FORCEFWD);
		if (!useron.fbacks && !useron.emails) {
			if (online) {                        /* didn't hang up */
				bprintf(text[NoFeedbackWarning], username(&cfg, cfg.valuser, tmp));
				email(cfg.valuser, str, text[NewUserValEmailSubj], WM_SUBJ_RO | WM_FORCEFWD);
			}     /* give 'em a 2nd try */
			if (!useron.fbacks && !useron.emails) {
				bprintf(text[NoFeedbackWarning], username(&cfg, cfg.valuser, tmp));
				logline(LOG_NOTICE, "N!", "Aborted feedback");
				hangup();
				putuserstr(useron.number, USER_COMMENT, "Didn't leave feedback");
				del_user(&cfg, &useron);
				return false;
			}
		}
	}

	answertime = starttime = time(NULL);      /* set answertime to now */

#ifdef JAVASCRIPT
	js_create_user_objects(js_cx, js_glob);
#endif

	exec_mod("new user", cfg.newuser_mod);
	user_event(EVENT_NEWUSER);
	getuseron(WHERE);   // In case event(s) modified user data
	logline("N+", "New user registration completed");

	return true;
}
