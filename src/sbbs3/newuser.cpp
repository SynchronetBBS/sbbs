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
	long kmode;
	bool usa;

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

	/* Sets defaults per sysop config */
	SAFECOPY(useron.comp, client_name);  /* hostname or CID name */
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
		useron.level = 99;
		useron.exempt = useron.flags1 = useron.flags2 = 0xffffffffUL;
		useron.flags3 = useron.flags4 = 0xffffffffUL;
		useron.rest = 0L;
	}

	kmode = (cfg.uq & UQ_NOEXASC) | K_EDIT | K_AUTODEL | K_TRIM;
	if (!(cfg.uq & UQ_NOUPRLWR))
		kmode |= K_UPRLWR;

	while (online) {

		if (autoterm || (text[AutoTerminalQ][0] && yesno(text[AutoTerminalQ]))) {
			useron.misc |= AUTOTERM;
			useron.misc |= autoterm;
		} else
			useron.misc &= ~AUTOTERM;

		while (text[HitYourBackspaceKey][0] && !(useron.misc & (PETSCII | SWAP_DELETE)) && online) {
			bputs(text[HitYourBackspaceKey]);
			uchar key = getkey(K_CTRLKEYS);
			bprintf(text[CharacterReceivedFmt], key, key);
			if (key == '\b')
				break;
			if (key == DEL) {
				if (text[SwapDeleteKeyQ][0] == 0 || yesno(text[SwapDeleteKeyQ]))
					useron.misc |= SWAP_DELETE;
			}
			else if (key == PETSCII_DELETE)
				useron.misc |= (PETSCII | COLOR);
			else {
				llprintf(LOG_NOTICE, "N!", "Unsupported backspace key received: %02Xh", key);
				bprintf(text[InvalidBackspaceKeyFmt], key, key);
				if (text[ContinueQ][0] && !yesno(text[ContinueQ]))
					return false;
			}
		}

		if (!(useron.misc & (AUTOTERM | PETSCII))) {
			if (text[AnsiTerminalQ][0] && yesno(text[AnsiTerminalQ]))
				useron.misc |= ANSI;
			else
				useron.misc &= ~ANSI;
		}

		if (useron.misc & ANSI) {
			useron.rows = TERM_ROWS_AUTO;
			useron.cols = TERM_COLS_AUTO;
			if (!(cfg.uq & UQ_COLORTERM) || useron.misc & (RIP) || yesno(text[ColorTerminalQ]))
				useron.misc |= COLOR;
			else
				useron.misc &= ~COLOR;
			if (text[MouseTerminalQ][0] && yesno(text[MouseTerminalQ]))
				useron.misc |= MOUSE;
			else
				useron.misc &= ~MOUSE;
		}
		else
			useron.rows = startup->default_term_height;

		if (useron.misc & PETSCII) {
			autoterm |= PETSCII;
			update_terminal(this);
			term_out(PETSCII_UPPERLOWER);
			bputs(text[PetTerminalDetected]);
		} else if (!(useron.misc & UTF8)) {
			if (!yesno(text[ExAsciiTerminalQ]))
				useron.misc |= NO_EXASCII;
			else
				useron.misc &= ~NO_EXASCII;
		}
		sys_status |= SS_NEWUSER;
		update_nodeterm();

		if (rlogin_name[0])
			SAFECOPY(useron.alias, rlogin_name);

		char* prompt = text[EnterYourRealName];
		if (cfg.uq & UQ_ALIASES)
			prompt = text[EnterYourAlias];
		while (*prompt && online) {
			bputs(prompt);
			getstr(useron.alias, LEN_ALIAS, kmode);
			truncsp(useron.alias);
			if (!check_name(&cfg, useron.alias)
			    || finduserstr(useron.number, USER_NAME, useron.alias)
			    || (!(cfg.uq & UQ_ALIASES) && !check_realname(&cfg, useron.alias))) {
				llprintf(LOG_NOTICE, "N!", "Invalid or duplicate user real name or alias: '%s'", useron.alias);
				bputs(text[YouCantUseThatName]);
				if (text[ContinueQ][0] && !yesno(text[ContinueQ]))
					return false;
				continue;
			}
			break;
		}
		if (!online)
			return false;
		if ((cfg.uq & UQ_ALIASES) && (cfg.uq & UQ_REALNAME)) {
			while (online && text[EnterYourRealName][0]) {
				bputs(text[EnterYourRealName]);
				getstr(useron.name, LEN_NAME, kmode);
				if (!check_name(&cfg, useron.name)
				    || !check_realname(&cfg, useron.name)
				    || ((cfg.uq & UQ_DUPREAL)
				        && finduserstr(useron.number, USER_NAME, useron.name))) {
					llprintf(LOG_NOTICE, "N!", "Invalid or duplicate user real name: '%s'", useron.name);
					bputs(text[YouCantUseThatName]);
				} else
					break;
				if (text[ContinueQ][0] && !yesno(text[ContinueQ]))
					return false;
			}
		}
		else if (cfg.uq & UQ_COMPANY && text[EnterYourCompany][0]) {
			bputs(text[EnterYourCompany]);
			getstr(useron.name, LEN_NAME, kmode);
		}
		if (!useron.alias[0]) {
			errormsg(WHERE, ERR_CHK, "alias", 0);
			return false;
		}
		if (!useron.name[0])
			SAFECOPY(useron.name, useron.alias);
		else if (!(cfg.uq & UQ_DUPREAL) && finduserstr(useron.number, USER_NAME, useron.name) > 0)
			useron.rest |= FLAG('O'); // Can't post or send netmail using real name (it's a duplicate)
		if (!online)
			return false;
		if (!useron.handle[0])
			SAFECOPY(useron.handle, useron.alias);
		while ((cfg.uq & UQ_HANDLE) && online && text[EnterYourHandle][0]) {
			bputs(text[EnterYourHandle]);
			if (!getstr(useron.handle, LEN_HANDLE
			            , K_LINE | K_EDIT | K_AUTODEL | K_TRIM | (cfg.uq & UQ_NOEXASC))
			    || strchr(useron.handle, 0xff)
			    || ((cfg.uq & UQ_DUPHAND)
			        && finduserstr(0, USER_HANDLE, useron.handle))
			    || trashcan(useron.handle, "name")) {
				llprintf(LOG_NOTICE, "N!", "Invalid or duplicate user handle: '%s'", useron.name);
				bputs(text[YouCantUseThatName]);
			} else
				break;
			if (text[ContinueQ][0] && !yesno(text[ContinueQ]))
				return false;
		}
		if (!online)
			return false;
		if (cfg.uq & UQ_ADDRESS)
			while (online && text[EnterYourAddress][0]) {       /* Get address and zip code */
				bputs(text[EnterYourAddress]);
				if (getstr(useron.address, LEN_ADDRESS, kmode))
					break;
			}
		if (!online)
			return false;
		while ((cfg.uq & UQ_LOCATION) && online && text[EnterYourCityState][0]) {
			bputs(text[EnterYourCityState]);
			if (getstr(useron.location, LEN_LOCATION, kmode) < 1)
				continue;
			if (!(cfg.uq & UQ_NOCOMMAS) && strchr(useron.location, ',') == NULL) {
				bputs(text[CommaInLocationRequired]);
				useron.location[0] = 0;
			} else
				break;
		}
		if (cfg.uq & UQ_ADDRESS)
			while (online && text[EnterYourZipCode][0]) {
				bputs(text[EnterYourZipCode]);
				if (getstr(useron.zipcode, LEN_ZIPCODE
				           , K_UPPER | (cfg.uq & UQ_NOEXASC) | K_EDIT | K_AUTODEL | K_TRIM))
					break;
			}
		if (!online)
			return false;
		if ((cfg.uq & UQ_PHONE) && text[EnterYourPhoneNumber][0]) {
			if (text[CallingFromNorthAmericaQ][0])
				usa = yesno(text[CallingFromNorthAmericaQ]);
			else
				usa = false;
			while (online && text[EnterYourPhoneNumber][0]) {
				bputs(text[EnterYourPhoneNumber]);
				if (!usa) {
					if (getstr(useron.phone, LEN_PHONE
					           , K_UPPER | K_LINE | (cfg.uq & UQ_NOEXASC) | K_EDIT | K_AUTODEL | K_TRIM) < 5)
						continue;
				}
				else {
					if (gettmplt(useron.phone, cfg.sys_phonefmt
					             , K_LINE | (cfg.uq & UQ_NOEXASC) | K_EDIT) < strlen(cfg.sys_phonefmt))
						continue;
				}
				if (!trashcan(useron.phone, "phone"))
					break;
			}
		}
		if (!online)
			return false;
		while ((cfg.uq & UQ_SEX) && text[EnterYourGender][0] && cfg.new_genders[0] != '\0' && online) {
			bputs(text[EnterYourGender]);
			long gender = getkeys(cfg.new_genders, 0);
			if (gender > 0) {
				useron.sex = (char)gender;
				break;
			}
		}
		while ((cfg.uq & UQ_BIRTH) && online && text[EnterYourBirthday][0]) {
			bprintf(text[EnterYourBirthday], birthdate_format(&cfg, tmp, sizeof tmp));
			format_birthdate(&cfg, useron.birth, str, sizeof(str));
			if (gettmplt(str, birthdate_template(&cfg, tmp, sizeof tmp), K_EDIT) < 10)
				continue;
			int age = getage(&cfg, parse_birthdate(&cfg, str, tmp, sizeof(tmp)));
			if (age >= 1 && age <= 200) { // TODO: Configurable min/max user age
				SAFECOPY(useron.birth, tmp);
				break;
			}
		}
		if (!online)
			return false;
		while (!(cfg.uq & UQ_NONETMAIL) && online && text[EnterNetMailAddress][0]) {
			bputs(text[EnterNetMailAddress]);
			if (getstr(useron.netmail, LEN_NETMAIL, K_EDIT | K_AUTODEL | K_LINE | K_TRIM) < 1
			    || trashcan(useron.netmail, "email")
			    || ((cfg.uq & UQ_DUPNETMAIL) && finduserstr(useron.number, USER_NETMAIL, useron.netmail)))
				bputs(text[YouCantUseThatNetmail]);
			else
				break;
		}
		useron.misc &= ~NETMAIL;
		if ((cfg.sys_misc & SM_FWDTONET) && is_supported_netmail_addr(&cfg, useron.netmail) && text[ForwardMailQ][0] && yesno(text[ForwardMailQ]))
			useron.misc |= NETMAIL;

		if (text[UserInfoCorrectQ][0] == 0 || yesno(text[UserInfoCorrectQ]))
			break;
	}
	if (!online)
		return false;
	llprintf("N", "New user: %s", useron.alias);
	if (!online)
		return false;
	menu("../sbbs", P_NOABORT | P_NOERROR);
	menu("../system", P_NOABORT | P_NOERROR);
	menu("../newuser", P_NOABORT | P_NOERROR);
	answertime = time(NULL);      /* could take 10 minutes to get this far */

	/* Default editor (moved here, after terminal type setup Jan-2003) */
	for (i = 0; i < cfg.total_xedits; i++)
		if (!stricmp(cfg.xedit[i]->code, cfg.new_xedit) && chk_ar(cfg.xedit[i]->ar, &useron, &client))
			break;
	if (i < cfg.total_xedits)
		useron.xedit = i + 1;

	if (cfg.total_xedits && (cfg.uq & UQ_XEDIT) && text[UseExternalEditorQ][0]) {
		if (yesno(text[UseExternalEditorQ])) {
			for (i = 0; i < cfg.total_xedits; i++)
				uselect(1, i, text[ExternalEditorHeading], cfg.xedit[i]->name, cfg.xedit[i]->ar);
			if ((int)(i = uselect(0, useron.xedit ? useron.xedit - 1 : 0, 0, 0, 0)) >= 0)
				useron.xedit = i + 1;
		} else
			useron.xedit = 0;
	}

	if (cfg.total_shells > 1 && (cfg.uq & UQ_CMDSHELL) && text[CommandShellHeading][0]) {
		for (i = 0; i < cfg.total_shells; i++)
			uselect(1, i, text[CommandShellHeading], cfg.shell[i]->name, cfg.shell[i]->ar);
		if ((int)(i = uselect(0, useron.shell, 0, 0, 0)) >= 0)
			useron.shell = i;
	}

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
			console |= CON_R_ECHOX;
			str[0] = 0;
			getstr(str, LEN_PASS * 2, K_UPPER);
			console &= ~(CON_R_ECHOX | CON_L_ECHOX);
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
		              , nulstr, getage(&cfg, useron.birth), useron.sex, useron.birth
		              , useron.name, useron.phone, useron.comp, useron.connection);
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

	if (cfg.newuser_mod[0])
		exec_bin(cfg.newuser_mod, &main_csi);
	user_event(EVENT_NEWUSER);
	getuseron(WHERE);   // In case event(s) modified user data
	logline("N+", "Successful new user registration");

	return true;
}
