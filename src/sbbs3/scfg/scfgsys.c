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

#include "scfg.h"
#include "ssl.h"
#include "ciolib.h" // CIO_KEY_*

extern char* strDisabled;

static int wiz_help(int page, int total, const char* buf)
{
	wizard_msg(page, total, buf);
	uifc.helpbuf = NULL;
	return WIN_SAV | WIN_L2R | WIN_NOBRDR;
}

int edit_sys_name(int page, int total)
{
	int mode = WIN_SAV | WIN_MID;
	uifc.helpbuf =
		"`BBS Name:`\n"
		"\n"
		"This is the name of your Bulletin Board System.  Try to be original.\n"
		"If you want to see BBS names already in use, reference Internet indexes\n"
		"such as `http://synchro.net/sbbslist.html` and `http://telnetbbsguide.com`"
	;
	if (page)
		mode = wiz_help(page, total, uifc.helpbuf);
	return uifc.input(mode, 0, 10, "BBS Name", cfg.sys_name, sizeof(cfg.sys_name) - 1, K_EDIT);
}

int edit_sys_location(int page, int total)
{
	int mode = WIN_SAV | WIN_MID;
	uifc.helpbuf =
		"`System Location:`\n"
		"\n"
		"This is the location of the BBS.  The format is flexible, but it is\n"
		"suggested you use the `City, State` format for U.S. locations.\n"
	;
	if (page)
		mode = wiz_help(page, total, uifc.helpbuf);
	return uifc.input(mode, 0, 10, "System Location", cfg.sys_location, sizeof(cfg.sys_location) - 1, K_EDIT);
}

int edit_sys_operator(int page, int total)
{
	int mode = WIN_SAV | WIN_MID;
	uifc.helpbuf =
		"`System Operator:`\n"
		"\n"
		"This is the name or alias of the system operator (you).  This does not\n"
		"have to be the same name or alias as user #1.  This value is used for\n"
		"informational/display purposes only.\n"
	;
	if (page)
		mode = wiz_help(page, total, uifc.helpbuf);
	return uifc.input(mode, 0, 10, "System Operator Name", cfg.sys_op, sizeof(cfg.sys_op) - 1, K_EDIT);
}

int edit_sys_password(int page, int total)
{
	int mode = WIN_SAV | WIN_MID;
	uifc.helpbuf =
		"`System Password:`\n"
		"\n"
		"This is an extra security password may be required for sysop login and\n"
		"sysop functions.  This password should be something not easily guessed\n"
		"and should be kept absolutely confidential.  This password must be\n"
		"entered at the Terminal Server `SY:` prompt.\n"
		"\n"
		"This system password can also be used to enable sysop access to the\n"
		"FTP Server by authenticating with a password that combines a sysop's\n"
		"password with the system password, separated by a colon\n"
		"(i.e. '`user-pass:system-pass`').\n"
	;
	if (page)
		mode = wiz_help(page, total, uifc.helpbuf);
	return uifc.input(mode, 0, 16, "System Password", cfg.sys_pass, sizeof(cfg.sys_pass) - 1, K_EDIT | K_UPPER);
}

int edit_sys_inetaddr(int page, int total)
{
	int mode = WIN_SAV | WIN_MID;
	uifc.helpbuf =
		"`System Internet Address:`\n"
		"\n"
		"Enter your system's Internet address (hostname or IP address) here\n"
		"(e.g. `joesbbs.com`).\n"
	;
	if (page)
		mode = wiz_help(page, total, uifc.helpbuf);
	return uifc.input(mode, 0, 10, "System Internet Address"
	                  , cfg.sys_inetaddr, 32, K_EDIT | K_NOSPACE);
}

int edit_sys_id(int page, int total)
{
	int  mode = WIN_SAV | WIN_MID;
	char str[LEN_QWKID + 1];

	do {
		SAFECOPY(str, cfg.sys_id);
		uifc.helpbuf =
			"`BBS ID for QWK Packets:`\n"
			"\n"
			"Enter an ID for your BBS that will be used for QWK message packets.\n"
			"This ID should be an abbreviation of your BBS name or other related\n"
			"words.\n"
			"\n"
			"The maximum length of the ID is eight characters and cannot contain\n"
			"spaces or other invalid DOS filename characters.  This ID should not\n"
			"begin with a number.\n"
			"\n"
			"In a QWK packet network, each system must have a unique BBS ID.\n"
		;
		if (page)
			mode = wiz_help(page, total, uifc.helpbuf);
		if (uifc.input(mode, 0, 16, "BBS ID for QWK Packets"
		               , str, LEN_QWKID, K_EDIT | K_UPPER) < 1)
			break;
		if (code_ok(str)) {
			SAFECOPY(cfg.sys_id, str);
			return 1;
		}
	} while (uifc.msg("Invalid QWK-ID (view help for constraints)") >= 0);

	return -1;
}

static int configure_dst(int page, int total)
{
	int mode = WIN_SAV | WIN_MID;
	strcpy(opt[0], "Yes");
	strcpy(opt[1], "No");
	strcpy(opt[2], "Automatic");
	opt[3][0] = 0;
	int i = 2;
	if (!(cfg.sys_misc & SM_AUTO_DST))
		i = !(cfg.sys_timezone & DAYLIGHT);
	uifc.helpbuf =
		"`Daylight Saving Time (DST):`\n"
		"\n"
		"If your system is using a U.S. standard time zone, and you would like\n"
		"to have the daylight saving time `flag` automatically toggled for you,\n"
		"set this option to ~Automatic~ (recommended).\n"
		"\n"
		"The ~DST~ `flag` is used for display purposes only (e.g. to display \"PDT\"\n"
		"instead of \"PST\" and calculate the correct offset from UTC), it does not\n"
		"actually change the time on your computer system(s) for you.\n"
	;
	if (page)
		mode = wiz_help(page, total, uifc.helpbuf);
	i = uifc.list(mode, 0, 14, 0, &i, 0
	              , "Daylight Saving Time (DST)", opt);
	if (i == -1)
		return -1;
	cfg.sys_misc &= ~SM_AUTO_DST;
	switch (i) {
		case 0:
			cfg.sys_timezone |= DAYLIGHT;
			break;
		case 1:
			cfg.sys_timezone &= ~DAYLIGHT;
			break;
		case 2:
			cfg.sys_misc |= SM_AUTO_DST;
			sys_timezone(&cfg);
			break;
	}
	return i;
}

#define UTC_OFFSET_HELP \
		"The UTC offset alone can be ambiguous as some time zones share the same\n" \
		"offset from UTC as other time zones for some or all months of the year.\n"

int edit_sys_timezone(int page, int total)
{
	int  mode = WIN_SAV | WIN_MID;
	char str[128];
	int  i;
	int  bar;

	i = cfg.sys_timezone != SYS_TIMEZONE_AUTO;
	uifc.helpbuf =
		"`Automatically Determine Time Zone:`\n"
		"\n"
		"Query the operating system for the current local time zone as an offset\n"
		"(i.e. minutes east or west) from UTC.\n"
		"\n"
		UTC_OFFSET_HELP
		"\n"
		"If enabled, this query will occur automatically each time the local time\n"
		"zone is needed, so any affect of daylight saving time will be applied\n"
		"automatically as well.\n"
	;
	if (page)
		mode = wiz_help(page, total, uifc.helpbuf);
	i = uifc.list(mode, 0, 15, 0, &i, 0
	              , "Automatically Determine Time Zone (as offset from UTC)"
	              , uifcYesNoOpts);
	if (i == -1)
		return -1;
	if (i == 0) {
		cfg.sys_timezone = SYS_TIMEZONE_AUTO;
		return 0;
	}
	if (cfg.sys_timezone == SYS_TIMEZONE_AUTO)
		cfg.sys_timezone = 0;
	i = OTHER_ZONE(cfg.sys_timezone) || !(cfg.sys_timezone & US_ZONE);
	uifc.helpbuf =
		"`United States Time Zone:`\n"
		"\n"
		"If your local time zone is the United States, select `Yes`.\n"
	;
	if (page)
		mode = wiz_help(page, total, uifc.helpbuf);
	i = uifc.list(mode, 0, 10, 0, &i, 0
	              , "United States Time Zone", uifcYesNoOpts);
	if (i == -1)
		return -1;
	if (i == 0) {
		strcpy(opt[i++], "Atlantic");
		strcpy(opt[i++], "Eastern");
		strcpy(opt[i++], "Central");
		strcpy(opt[i++], "Mountain");
		strcpy(opt[i++], "Pacific");
		strcpy(opt[i++], "Yukon");
		strcpy(opt[i++], "Hawaii/Alaska");
		strcpy(opt[i++], "Bering");
		opt[i][0] = 0;
		switch (((uint16_t)cfg.sys_timezone) & ~DAYLIGHT) {
			default:
			case AST: i = 0; break;
			case EST: i = 1; break;
			case CST: i = 2; break;
			case MST: i = 3; break;
			case PST: i = 4; break;
			case YST: i = 5; break;
			case HST: i = 6; break;
			case BST: i = 7; break;
		}
		uifc.helpbuf =
			"`U.S. Time Zone:`\n"
			"\n"
			"Choose the region which most closely reflects your local U.S. time zone.\n"
		;
		if (page)
			mode = wiz_help(page, total, uifc.helpbuf);
		i = uifc.list(mode, 0, 9, 0, &i, 0
		              , "U.S. Time Zone", opt);
		if (i == -1)
			return -1;
		switch (i) {
			case 0:
				cfg.sys_timezone = AST;
				break;
			case 1:
				cfg.sys_timezone = EST;
				break;
			case 2:
				cfg.sys_timezone = CST;
				break;
			case 3:
				cfg.sys_timezone = MST;
				break;
			case 4:
				cfg.sys_timezone = PST;
				break;
			case 5:
				cfg.sys_timezone = YST;
				break;
			case 6:
				cfg.sys_timezone = HST;
				break;
			case 7:
				cfg.sys_timezone = BST;
				break;
		}
		return configure_dst(page, total);
	}
	i = 0;
	strcpy(opt[i++], "Midway");
	strcpy(opt[i++], "Vancouver");
	strcpy(opt[i++], "Edmonton");
	strcpy(opt[i++], "Winnipeg");
	strcpy(opt[i++], "Bogota");
	strcpy(opt[i++], "Caracas");
	strcpy(opt[i++], "Rio de Janeiro");
	strcpy(opt[i++], "Fernando de Noronha");
	strcpy(opt[i++], "Azores");
	strcpy(opt[i++], "Western Europe (WET)");
	strcpy(opt[i++], "Central Europe (CET)");
	strcpy(opt[i++], "Eastern Europe (EET)");
	strcpy(opt[i++], "Moscow");
	strcpy(opt[i++], "Dubai");
	strcpy(opt[i++], "Kabul");
	strcpy(opt[i++], "Karachi");
	strcpy(opt[i++], "Bombay");
	strcpy(opt[i++], "Kathmandu");
	strcpy(opt[i++], "Dhaka");
	strcpy(opt[i++], "Bangkok");
	strcpy(opt[i++], "Hong Kong");
	strcpy(opt[i++], "Tokyo");
	strcpy(opt[i++], "Australian Central");
	strcpy(opt[i++], "Australian Eastern");
	strcpy(opt[i++], "Noumea");
	strcpy(opt[i++], "New Zealand");
	strcpy(opt[i++], "Other...");
	opt[i][0] = 0;
	switch (cfg.sys_timezone) {
		case MID: i = 0; break;
		case VAN: i = 1; break;
		case EDM: i = 2; break;
		case WIN: i = 3; break;
		case BOG: i = 4; break;
		case CAR: i = 5; break;
		case RIO: i = 6; break;
		case FER: i = 7; break;
		case AZO: i = 8; break;
		case WET: i = 9; break;
		case CET: i = 10; break;
		case EET: i = 11; break;
		case MOS: i = 12; break;
		case DUB: i = 13; break;
		case KAB: i = 14; break;
		case KAR: i = 15; break;
		case BOM: i = 16; break;
		case KAT: i = 17; break;
		case DHA: i = 18; break;
		case BAN: i = 19; break;
		case HON: i = 20; break;
		case TOK: i = 21; break;
		case ACST: i = 22; break;
		case AEST: i = 23; break;
		case NOU: i = 24; break;
		case NZST: i = 25; break;
		default: i = 26; break;
	}
	uifc.helpbuf =
		"`Non-U.S. Time Zone:`\n"
		"\n"
		"Choose the region which most closely reflects your local time zone.\n"
		"\n"
		"Choose `Other...` if a region representing your local time zone is not\n"
		"listed (you will be able to set the exact UTC offset manually)."
	;
	if (page) {
		mode = wiz_help(page, total, uifc.helpbuf);
		mode |= WIN_FIXEDHEIGHT;
	}
	bar = i;
	uifc.list_height = 8;
	i = uifc.list(mode, 0, 10, 0, &i, &bar
	              , "Non-U.S. Time Zone", opt);
	if (i == -1)
		return -1;
	switch (i) {
		case 0:
			cfg.sys_timezone = MID;
			break;
		case 1:
			cfg.sys_timezone = VAN;
			break;
		case 2:
			cfg.sys_timezone = EDM;
			break;
		case 3:
			cfg.sys_timezone = WIN;
			break;
		case 4:
			cfg.sys_timezone = BOG;
			break;
		case 5:
			cfg.sys_timezone = CAR;
			break;
		case 6:
			cfg.sys_timezone = RIO;
			break;
		case 7:
			cfg.sys_timezone = FER;
			break;
		case 8:
			cfg.sys_timezone = AZO;
			break;
		case 9:
			cfg.sys_timezone = WET;
			break;
		case 10:
			cfg.sys_timezone = CET;
			break;
		case 11:
			cfg.sys_timezone = EET;
			break;
		case 12:
			cfg.sys_timezone = MOS;
			break;
		case 13:
			cfg.sys_timezone = DUB;
			break;
		case 14:
			cfg.sys_timezone = KAB;
			break;
		case 15:
			cfg.sys_timezone = KAR;
			break;
		case 16:
			cfg.sys_timezone = BOM;
			break;
		case 17:
			cfg.sys_timezone = KAT;
			break;
		case 18:
			cfg.sys_timezone = DHA;
			break;
		case 19:
			cfg.sys_timezone = BAN;
			break;
		case 20:
			cfg.sys_timezone = HON;
			break;
		case 21:
			cfg.sys_timezone = TOK;
			break;
		case 22:
			cfg.sys_timezone = ACST;
			break;
		case 23:
			cfg.sys_timezone = AEST;
			break;
		case 24:
			cfg.sys_timezone = NOU;
			break;
		case 25:
			cfg.sys_timezone = NZST;
			break;
		default:
			if (cfg.sys_timezone > 720 || cfg.sys_timezone < -720)
				cfg.sys_timezone = 0;
			snprintf(str, sizeof(str), "%02d:%02d"
			         , cfg.sys_timezone / 60, cfg.sys_timezone < 0
			    ? (-cfg.sys_timezone) % 60 : cfg.sys_timezone % 60);
			uifc.helpbuf =
				"`Time Zone Offset:`\n"
				"\n"
				"Enter your local time zone offset from Universal Time (UTC/GMT) in\n"
				"`HH:MM` format.\n"
				"\n"
				UTC_OFFSET_HELP
			;
			if (page)
				mode = wiz_help(page, total, uifc.helpbuf);
			if (uifc.input(mode, 0, 10
			               , "Time (HH:MM) East (+) or West (-) of Universal Time"
			               , str, 6, K_EDIT | K_UPPER) < 1)
				return -1;
			cfg.sys_timezone = atoi(str) * 60;
			char *p = strchr(str, ':');
			if (p) {
				if (cfg.sys_timezone < 0)
					cfg.sys_timezone -= atoi(p + 1);
				else
					cfg.sys_timezone += atoi(p + 1);
			}
			return 0;
	}
	if (SMB_TZ_HAS_DST(cfg.sys_timezone))
		return configure_dst(page, total);
	return 1;
}

int edit_sys_newuser_policy(int page, int total)
{
	int mode = WIN_SAV | WIN_MID;
	int i = cfg.sys_misc & SM_CLOSED ? 1:0;
	uifc.helpbuf =
		"`Open to New Users:`\n"
		"\n"
		"If you want callers to be able to register as a new user of your system\n"
		"(e.g. create a new user account by logging-in as `New`), set this option\n"
		"to `Yes`.\n"
	;
	if (page)
		mode = wiz_help(page, total, uifc.helpbuf);
	i = uifc.list(mode, 0, 11, 0, &i, 0
	              , "Open to New Users", uifcYesNoOpts);
	if (i == 0) {
		cfg.sys_misc &= ~SM_CLOSED;
		uifc.helpbuf =
			"`New User Password:`\n"
			"\n"
			"If you want callers to be able to register ~ only ~ if they know a secret\n"
			"password, enter that password here.  If you prefer that `any` caller be\n"
			"able to register a new user account, leave this option blank.\n"
		;
		if (page)
			mode = wiz_help(page, total, uifc.helpbuf);
		if (uifc.input(mode, 0, 10, "New User Password (optional)", cfg.new_pass, sizeof(cfg.new_pass) - 1
		               , K_EDIT | K_UPPER) < 0)
			return -1;
	}
	else if (i == 1) {
		cfg.sys_misc |= SM_CLOSED;
	}
	return i;
}

int edit_sys_delmsg_policy(int page, int total)
{
	int   mode = WIN_SAV | WIN_MID;
	char* opt[] = {"Yes", "No", "Sysops Only", NULL };
	int   i;
	switch (cfg.sys_misc & (SM_USRVDELM | SM_SYSVDELM)) {
		default:
		case SM_USRVDELM | SM_SYSVDELM:
			i = 0;
			break;
		case 0:
			i = 1;
			break;
		case SM_SYSVDELM:
			i = 2;
			break;
	}
	uifc.helpbuf =
		"`Users Can View Deleted Messages:`\n"
		"\n"
		"If this option is set to `Yes`, then users will be able to view messages\n"
		"they've sent and deleted, or messages sent to them and they've deleted\n"
		"with the option of un-deleting the message before the message is\n"
		"physically purged from the e-mail database.\n"
		"\n"
		"If this option is set to `No`, then when a message is deleted, it is no\n"
		"longer viewable (with SBBS) by anyone.\n"
		"\n"
		"If this option is set to `Sysops Only`, then only sysops and sub-ops (when\n"
		"appropriate) can view deleted messages.\n"
	;
	if (page)
		mode = wiz_help(page, total, uifc.helpbuf);
	i = uifc.list(mode, 0, 15, 0, &i, 0
	              , "Users Can View Deleted Messages", opt);
	if (!i && (cfg.sys_misc & (SM_USRVDELM | SM_SYSVDELM))
	    != (SM_USRVDELM | SM_SYSVDELM)) {
		cfg.sys_misc |= (SM_USRVDELM | SM_SYSVDELM);
		uifc.changes = TRUE;
	}
	else if (i == 1 && cfg.sys_misc & (SM_USRVDELM | SM_SYSVDELM)) {
		cfg.sys_misc &= ~(SM_USRVDELM | SM_SYSVDELM);
		uifc.changes = TRUE;
	}
	else if (i == 2 && (cfg.sys_misc & (SM_USRVDELM | SM_SYSVDELM))
	         != SM_SYSVDELM) {
		cfg.sys_misc |= SM_SYSVDELM;
		cfg.sys_misc &= ~SM_USRVDELM;
		uifc.changes = TRUE;
	}
	return i;
}

void security_cfg(void)
{
	char       str[128];
	static int dflt;
	static int quick_dflt;
	static int expired_dflt;
	static int seclevel_dflt, seclevel_bar;
	int        i, j, k;
	BOOL       done;

	while (1) {
		i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%s", "System Password", "*******");
		if (cfg.sys_misc & SM_R_SYSOP) {
			*str = '\0';
			if (cfg.sys_misc & SM_SYSPASSLOGIN)
				SAFECOPY(str, "At Login and ");
			sprintf(str + strlen(str), "After %u minutes", cfg.sys_pass_timeout);
		} else
			SAFECOPY(str, "N/A");
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%s", "Prompt for System Password", str);
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%s", "Allow Sysop Access"
		         , (cfg.sys_misc & SM_R_SYSOP) ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%s", "Allow Login by Real Name"
		         , (!(cfg.uq & UQ_ALIASES) || cfg.sys_login & LOGIN_REALNAME) ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%s", "Allow Login by User Number"
		         , (cfg.sys_login & LOGIN_USERNUM) ? "Yes" : "No");
		SAFEPRINTF(str, "%s Password"
		           , cfg.sys_misc & SM_PWEDIT && cfg.sys_pwdays ? "Users Must Change"
		    : cfg.sys_pwdays ? "Users Get New Random" : "Users Can Choose");
		if (cfg.sys_pwdays)
			SAFEPRINTF(tmp, "Every %u Days", cfg.sys_pwdays);
		else if (cfg.sys_misc & SM_PWEDIT)
			SAFECOPY(tmp, "Yes");
		else
			SAFECOPY(tmp, "No");
		if (cfg.sys_misc & SM_PWEDIT)
			sprintf(tmp + strlen(tmp), ", %u chars minimum", cfg.min_pwlen);
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%s", str, tmp);
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%s", "Demand High Quality Password"
				, cfg.hq_password ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%s", "Always Prompt for Password"
		         , cfg.sys_login & LOGIN_PWPROMPT ? "Yes":"No");
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%s", "Display/Log Passwords Locally"
		         , cfg.sys_misc & SM_ECHO_PW ? "Yes" : "No");

		snprintf(opt[i++], MAX_OPLN, "%-33.33s%u", "Days to Preserve Deleted Users"
		         , cfg.sys_deldays);
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%s", "Maximum Days of User Inactivity"
		         , cfg.sys_autodel ? ultoa(cfg.sys_autodel, tmp, 10) : "Unlimited");
		if (cfg.sys_misc & SM_CLOSED)
			SAFECOPY(str, "No");
		else {
			if (*cfg.new_pass)
				SAFEPRINTF(str, "Yes, PW: %s", cfg.new_pass);
			else
				SAFECOPY(str, "Yes");
		}
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%s", "Open to New Users", str);
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%s", "User Expires When Out-of-time"
		         , cfg.sys_misc & SM_TIME_EXP ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%s", "Create Self-signed Certificate"
		         , cfg.create_self_signed_cert ? "Yes" : "No");
		strcpy(opt[i++], "Security Level Values...");
		strcpy(opt[i++], "Expired Account Values...");
		strcpy(opt[i++], "Quick-Validation Values...");

		opt[i][0] = 0;
		uifc.helpbuf =
			"`System Security Options:`\n"
			"\n"
			"This menu contains options and sub-menus of options that affect the\n"
			"security related behavior of the entire BBS.\n"
		;
		switch (uifc.list(WIN_ACT | WIN_BOT | WIN_RHT, 0, 0, 72, &dflt, 0
		                  , "Security Options", opt)) {
			case -1:
				return;
			case __COUNTER__:
				edit_sys_password(false, false);
				break;
			case __COUNTER__:
				if (!(cfg.sys_misc & SM_R_SYSOP))
					break;
				i = cfg.sys_misc & SM_SYSPASSLOGIN ? 0:1;
				uifc.helpbuf =
					"`Require System Password for Sysop Login:`\n"
					"\n"
					"If you want to require the correct system password to be provided during\n"
					"system operator logins (in addition to the sysop's personal user account\n"
					"password), set this option to `Yes`.\n"
					"\n"
					"For elevated security, set this option to `Yes`.\n"
				;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
				              , "Require System Password for Sysop Login", uifcYesNoOpts);
				if (i == 1)
					cfg.sys_misc &= ~SM_SYSPASSLOGIN;
				else if (i == 0)
					cfg.sys_misc |= SM_SYSPASSLOGIN;
				else
					break;
				sprintf(str, "%u", cfg.sys_pass_timeout);
				uifc.helpbuf =
					"`System Password Timeout:`\n"
					"\n"
					"Set this value to the number of minutes after which the system password\n"
					"will again have to be successfully entered to engage in system operator\n"
					"activities.\n"
					"\n"
					"For elevated security, set this option to `0`.\n"
				;
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "System Password Timeout (minutes)", str, 5, K_EDIT | K_NUMBER) > 0)
					cfg.sys_pass_timeout = atoi(str);
				break;
			case __COUNTER__:
				i = cfg.sys_misc & SM_R_SYSOP ? 0:1;
				uifc.helpbuf =
					"`Allow Sysop Access:`\n"
					"\n"
					"Setting this option to `No` will prevent users with sysop security level\n"
					"from invoking functions that require system-password authentication.\n"
				;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
				              , "Allow Sysop Access", uifcYesNoOpts);
				if (i == 0)
					cfg.sys_misc |= SM_R_SYSOP;
				else if (i == 1)
					cfg.sys_misc &= ~SM_R_SYSOP;
				break;
			case __COUNTER__:
				if (!(cfg.uq & UQ_ALIASES))
					break;
				i = (cfg.sys_login & LOGIN_REALNAME) ? 0:1;
				uifc.helpbuf =
					"`Allow Login by Real Name:`\n"
					"\n"
					"If you want users to be able login using their real name as well as\n"
					"their alias, set this option to `Yes`.\n"
					"\n"
					"For elevated security, set this option to `No`.\n"
				;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 10, 0, &i, 0
				              , "Allow Login by Real Name", uifcYesNoOpts);
				if ((i == 0 && !(cfg.sys_login & LOGIN_REALNAME))
				    || (i == 1 && (cfg.sys_login & LOGIN_REALNAME))) {
					cfg.sys_login ^= LOGIN_REALNAME;
				}
				break;
			case __COUNTER__:
				i = (cfg.sys_login & LOGIN_USERNUM) ? 0:1;
				uifc.helpbuf =
					"`Allow Login by User Number:`\n"
					"\n"
					"If you want users to be able login using their user number at the\n"
					"login prompt, set this option to `Yes`.\n"
					"\n"
					"For elevated security, set this option to `No`.\n"
				;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 10, 0, &i, 0
				              , "Allow Login by User Number", uifcYesNoOpts);
				if ((i == 0 && !(cfg.sys_login & LOGIN_USERNUM))
				    || (i == 1 && (cfg.sys_login & LOGIN_USERNUM))) {
					cfg.sys_login ^= LOGIN_USERNUM;
				}
				break;
			case __COUNTER__:
				i = (cfg.sys_misc & SM_PWEDIT) ? 0 : 1;
				uifc.helpbuf =
					"`Allow Users to Choose Their Password:`\n"
					"\n"
					"If you want the users of your system to have the option of changing\n"
					"their randomly-generated password to a string of their choice, set this\n"
					"option to `Yes`.\n"
					"\n"
					"For elevated security, set this option to `No`.\n"
				;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
				              , "Allow Users to Choose Their Password", uifcYesNoOpts);
				if (!i && !(cfg.sys_misc & SM_PWEDIT)) {
					cfg.sys_misc |= SM_PWEDIT;
				}
				else if (i == 1 && cfg.sys_misc & SM_PWEDIT) {
					cfg.sys_misc &= ~SM_PWEDIT;
				} else if (i == -1)
					break;

				if (cfg.sys_misc & SM_PWEDIT) {
					uifc.helpbuf =
						"`Minimum Password Length`\n"
						"\n"
						"Set the minimum required user password length, in characters.\n"
						"\n"
						"For elevated of security, set this to at least `8`.\n"
					;
					SAFEPRINTF(tmp, "%u", cfg.min_pwlen);
					SAFEPRINTF2(str, "Minimum Password Length (between %u and %u)", MIN_PASS_LEN, LEN_PASS);
					if (uifc.input(WIN_MID | WIN_SAV, 0, 0, str
					               , tmp, 2, K_NUMBER | K_EDIT) < 1)
						break;
					cfg.min_pwlen = atoi(tmp);
					if (cfg.min_pwlen < MIN_PASS_LEN)
						cfg.min_pwlen = MIN_PASS_LEN;
					if (cfg.min_pwlen > LEN_PASS)
						cfg.min_pwlen = LEN_PASS;
				}
				i = cfg.sys_pwdays ? 0 : 1;
				uifc.helpbuf =
					"`Force Periodic New Password:`\n"
					"\n"
					"\n"
					"If you want your users to be forced to change their password\n"
					"periodically, select `Yes`.\n"
					"\n"
					"For elevated security, set this option to `Yes`.\n"
				;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
				              , "Force Periodic New Password", uifcYesNoOpts);
				if (!i) {
					ultoa(cfg.sys_pwdays, str, 10);
					uifc.helpbuf =
						"`Maximum Days Between New Passwords:`\n"
						"\n"
						"Enter the maximum number of days allowed between password changes.  If a\n"
						"user has not voluntarily changed his or her password in this many days,\n"
						"he or she will be forced to change their password upon logon.\n"
						"\n"
						"Setting this value to `0` disables the forced periodic password change.\n"
						"\n"
						"For elevated security, set this to `90` or less.\n"
					;
					uifc.input(WIN_MID, 0, 0, "Maximum Days Between New Password"
					           , str, 5, K_NUMBER | K_EDIT);
					cfg.sys_pwdays = atoi(str);
				}
				else if (i == 1 && cfg.sys_pwdays) {
					cfg.sys_pwdays = 0;
				}
				break;
			case __COUNTER__:
				i = (cfg.hq_password) ? 0:1;
				uifc.helpbuf =
					"`Demand High Quality Password:`\n"
					"\n"
					"If you want users to be required to have a \"high quality\" password\n"
					"based on calculated entropy, set this option to `Yes`.\n"
					"\n"
					"For elevated security, set this option to `Yes`.\n"
				;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 10, 0, &i, 0
				              , "Require Users to use High Quality/Entropy Passwords", uifcYesNoOpts);
				if ((i == 0 && !cfg.hq_password) || (i == 1 && cfg.hq_password))
					cfg.hq_password = !cfg.hq_password;
				break;
			case __COUNTER__:
				i = cfg.sys_login & LOGIN_PWPROMPT ? 0:1;
				uifc.helpbuf =
					"`Always Prompt for Password:`\n"
					"\n"
					"If you want to have attempted logins using an unknown login ID still\n"
					"prompt for a password, set this option to `Yes`.\n"
					"\n"
					"For elevated security, set this option to `Yes`.\n"
				;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 10, 0, &i, 0
				              , "Always Prompt for Password", uifcYesNoOpts);
				if ((i == 0 && !(cfg.sys_login & LOGIN_PWPROMPT))
				    || (i == 1 && (cfg.sys_login & LOGIN_PWPROMPT))) {
					cfg.sys_login ^= LOGIN_PWPROMPT;
				}
				break;
			case __COUNTER__:
				i = cfg.sys_misc & SM_ECHO_PW ? 0:1;
				uifc.helpbuf =
					"`Display/Log Passwords Locally:`\n"
					"\n"
					"If you want attempted passwords to be displayed locally and/or logged to\n"
					"disk (e.g. when there's a failed login attempt), set this option to `Yes`.\n"
					"\n"
					"For elevated security, set this option to `No`.\n"
				;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
				              , "Display/Log Passwords Locally", uifcYesNoOpts);
				if (!i && !(cfg.sys_misc & SM_ECHO_PW)) {
					cfg.sys_misc |= SM_ECHO_PW;
				}
				else if (i == 1 && cfg.sys_misc & SM_ECHO_PW) {
					cfg.sys_misc &= ~SM_ECHO_PW;
				}
				break;
			case __COUNTER__:
				sprintf(str, "%u", cfg.sys_deldays);
				uifc.helpbuf =
					"`Days to Preserve Deleted Users:`\n"
					"\n"
					"Deleted user records can be `undeleted` until the slot is over-written\n"
					"by a new user.  If you want deleted user records to be preserved for a\n"
					"period of time after deletion, set this value to the number of days to\n"
					"keep new users from taking over a deleted user's slot.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Days to Preserve Deleted Users"
				           , str, 5, K_EDIT | K_NUMBER);
				cfg.sys_deldays = atoi(str);
				break;
			case __COUNTER__:
				sprintf(str, "%u", cfg.sys_autodel);
				uifc.helpbuf =
					"`Maximum Days of User Inactivity Before Auto-Deletion:`\n"
					"\n"
					"If you want users that have not logged-on in a certain period of time to\n"
					"be automatically deleted, set this value to the maximum number of days\n"
					"of inactivity before the user is deleted. Setting this value to `0`\n"
					"disables this feature.\n"
					"\n"
					"Users with the `P` exemption will not be deleted due to inactivity.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Days of User Inactivity Before Auto-Deletion"
				           , str, 5, K_EDIT | K_NUMBER);
				cfg.sys_autodel = atoi(str);
				break;
			case __COUNTER__:
				edit_sys_newuser_policy(false, false);
				break;
			case __COUNTER__:
				i = cfg.sys_misc & SM_TIME_EXP ? 0:1;
				uifc.helpbuf =
					"`User Expires When Out-of-time:`\n"
					"\n"
					"If you want users to be set to `Expired Account Values` if they run out of\n"
					"time online, then set this option to `Yes`.\n"
				;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
				              , "User Expires When Out-of-time", uifcYesNoOpts);
				if (!i && !(cfg.sys_misc & SM_TIME_EXP)) {
					cfg.sys_misc |= SM_TIME_EXP;
				}
				else if (i == 1 && cfg.sys_misc & SM_TIME_EXP) {
					cfg.sys_misc &= ~SM_TIME_EXP;
				}
				break;
			case __COUNTER__:
				i = cfg.create_self_signed_cert ? 0 : 1;
				uifc.helpbuf =
					"`Create Self-signed TLS Certificate:`\n"
					"\n"
					"If you want Synchronet to automatically create a self-signed certificate\n"
					"(for TLS connections) when the certificate file (`ctrl/ssl.cert`) cannot\n"
					"be found or read, set this option to `Yes`.\n"
				;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
				              , "Create Self-signed Certificate", uifcYesNoOpts);
				if (!i && !cfg.create_self_signed_cert) {
					cfg.create_self_signed_cert = true;
				}
				else if (i == 1 && cfg.create_self_signed_cert) {
					cfg.create_self_signed_cert = false;
				}
				break;
			case __COUNTER__: /* Security Levels */
				k = 0;
				while (1) {
					for (i = 0; i < 100; i++) {
						if (cfg.level_downloadsperday[i] < 1)
							SAFECOPY(str, "*");
						else
							snprintf(str, sizeof str, "%u", cfg.level_downloadsperday[i]);
						byte_count_to_str(cfg.level_freecdtperday[i], tmp, sizeof(tmp));
						snprintf(opt[i], MAX_OPLN, "%-2d    %5d %5d "
						         "%5d %5d %5d %5d %5s %6s %7s %2u", i
						         , cfg.level_timeperday[i], cfg.level_timepercall[i]
						         , cfg.level_callsperday[i], cfg.level_emailperday[i]
						         , cfg.level_postsperday[i], cfg.level_linespermsg[i]
						         , str
						         , tmp
						         , cfg.level_misc[i] & LEVEL_EXPTOVAL ? "Val Set" : "Level"
						         , cfg.level_misc[i] & (LEVEL_EXPTOVAL | LEVEL_EXPTOLVL) ?
						         cfg.level_expireto[i] : cfg.expired_level);
					}
					opt[i][0] = 0;
					i = 0;
					uifc.helpbuf =
						"`Security Level Values:`\n"
						"\n"
						"This menu allows you to change the security options for every possible\n"
						"security level from 0 to 99. The available options for each level are:\n"
						"\n"
						"    Time Per Day           Maximum online time per day\n"
						"    Time Per Call          Maximum online time per call (session)\n"
						"    Calls Per Day          Maximum number of calls (logins) per day\n"
						"    Email Per Day          Maximum number of email sent per day\n"
						"    Posts Per Day          Maximum number of posted messages per day\n"
						"    Lines Per Message      Maximum number of lines per message\n"
						"    Downloads Per Day      Maximum number of files downloaded per day\n"
						"    Free Credits Per Day   Number of free credits awarded per day\n"
						"    Expire To              Level or validation set to Expire to\n"
					;
					i = uifc.list(WIN_RHT | WIN_ACT | WIN_SAV, 0, 3, 0, &seclevel_dflt, &seclevel_bar
					              , "Level   T/D   T/C   C/D   E/D   P/D   L/M   D/D    F/D  "
					              "Expire To", opt);
					if (i == -1)
						break;
					while (1) {
						sprintf(str, "Security Level %d Values", i);
						j = 0;
						snprintf(opt[j++], MAX_OPLN, "%-22.22s%-5u", "Time Per Day"
						         , cfg.level_timeperday[i]);
						snprintf(opt[j++], MAX_OPLN, "%-22.22s%-5u", "Time Per Call"
						         , cfg.level_timepercall[i]);
						snprintf(opt[j++], MAX_OPLN, "%-22.22s%-5u", "Calls Per Day"
						         , cfg.level_callsperday[i]);
						snprintf(opt[j++], MAX_OPLN, "%-22.22s%-5u", "Email Per Day"
						         , cfg.level_emailperday[i]);
						snprintf(opt[j++], MAX_OPLN, "%-22.22s%-5u", "Posts Per Day"
						         , cfg.level_postsperday[i]);
						snprintf(opt[j++], MAX_OPLN, "%-22.22s%-5u", "Lines Per Message"
						         , cfg.level_linespermsg[i]);
						if (cfg.level_downloadsperday[i] < 1)
							SAFECOPY(tmp, "Unlimited");
						else
							snprintf(tmp, sizeof tmp, "%u", cfg.level_downloadsperday[i]);
						snprintf(opt[j++], MAX_OPLN, "%-22.22s%s", "Downloads Per Day", tmp);
						byte_count_to_str(cfg.level_freecdtperday[i], tmp, sizeof(tmp));
						snprintf(opt[j++], MAX_OPLN, "%-22.22s%-6s", "Free Credits Per Day"
						         , tmp);
						snprintf(opt[j++], MAX_OPLN, "%-22.22s%s %u", "Expire To"
						         , cfg.level_misc[i] & LEVEL_EXPTOVAL ? "Validation Set"
						        : "Level"
						         , cfg.level_misc[i] & (LEVEL_EXPTOVAL | LEVEL_EXPTOLVL) ?
						         cfg.level_expireto[i] : cfg.expired_level);
						opt[j][0] = 0;
						uifc_winmode_t wmode = WIN_RHT | WIN_SAV | WIN_ACT | WIN_EXTKEYS;
						if (i > 0)
							wmode |= WIN_LEFTKEY;
						if (i + 1 < 100)
							wmode |= WIN_RIGHTKEY;
						j = uifc.list(wmode, 2, 1, 0, &k, 0
						              , str, opt);
						if (j == -1)
							break;
						switch (j) {
							case -CIO_KEY_LEFT - 2:
								if (i > 0)
									i--;
								break;
							case -CIO_KEY_RIGHT - 2:
								if (i + 1 < 100)
									i++;
								break;
							case 0:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Total Time Allowed Per Day (in minutes)"
								           , ultoa(cfg.level_timeperday[i], tmp, 10), 4
								           , K_NUMBER | K_EDIT);
								cfg.level_timeperday[i] = atoi(tmp);
								if (cfg.level_timeperday[i] > 1440)
									cfg.level_timeperday[i] = 1440;
								break;
							case 1:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Time Allowed Per Call (in minutes)"
								           , ultoa(cfg.level_timepercall[i], tmp, 10), 4
								           , K_NUMBER | K_EDIT);
								cfg.level_timepercall[i] = atoi(tmp);
								if (cfg.level_timepercall[i] > 1440)
									cfg.level_timepercall[i] = 1440;
								break;
							case 2:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Calls (Logons) Allowed Per Day"
								           , ultoa(cfg.level_callsperday[i], tmp, 10), 4
								           , K_NUMBER | K_EDIT);
								cfg.level_callsperday[i] = atoi(tmp);
								break;
							case 3:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Email (Sent) Allowed Per Day"
								           , ultoa(cfg.level_emailperday[i], tmp, 10), 4
								           , K_NUMBER | K_EDIT);
								cfg.level_emailperday[i] = atoi(tmp);
								break;
							case 4:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Posted Messages Allowed Per Day"
								           , ultoa(cfg.level_postsperday[i], tmp, 10), 4
								           , K_NUMBER | K_EDIT);
								cfg.level_postsperday[i] = atoi(tmp);
								break;
							case 5:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Lines Allowed Per Message (Post/E-mail)"
								           , ultoa(cfg.level_linespermsg[i], tmp, 10), 5
								           , K_NUMBER | K_EDIT);
								cfg.level_linespermsg[i] = atoi(tmp);
								break;
							case 6:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Downloaded Files Allowed Per Day (0=Unlimited)"
								           , ultoa(cfg.level_downloadsperday[i], tmp, 10), 5
								           , K_NUMBER | K_EDIT);
								cfg.level_downloadsperday[i] = strtoul(tmp, NULL, 10);
								break;
							case 7:
								byte_count_to_str(cfg.level_freecdtperday[i], tmp, sizeof(tmp));
								if (uifc.input(WIN_MID | WIN_SAV, 0, 0
								               , "Free Credits Awarded Per Day"
								               , tmp, 19
								               , K_EDIT | K_UPPER) > 0)
									cfg.level_freecdtperday[i] = parse_byte_count(tmp, 1);
								break;
							case 8:
								j = 0;
								snprintf(opt[j++], MAX_OPLN, "Default Expired Level "
								         "(Currently %u)", cfg.expired_level);
								snprintf(opt[j++], MAX_OPLN, "Specific Level");
								snprintf(opt[j++], MAX_OPLN, "Quick-Validation Set");
								opt[j][0] = 0;
								j = 0;
								sprintf(str, "Level %u Expires To", i);
								j = uifc.list(WIN_SAV, 2, 1, 0, &j, 0
								              , str, opt);
								if (j == -1)
									break;
								if (j == 0) {
									cfg.level_misc[i] &=
										~(LEVEL_EXPTOLVL | LEVEL_EXPTOVAL);
									break;
								}
								if (j == 1) {
									cfg.level_misc[i] &= ~LEVEL_EXPTOVAL;
									cfg.level_misc[i] |= LEVEL_EXPTOLVL;
									uifc.input(WIN_MID | WIN_SAV, 0, 0
									           , "Expired Level"
									           , ultoa(cfg.level_expireto[i], tmp, 10), 2
									           , K_EDIT | K_NUMBER);
									cfg.level_expireto[i] = atoi(tmp);
									break;
								}
								cfg.level_misc[i] &= ~LEVEL_EXPTOLVL;
								cfg.level_misc[i] |= LEVEL_EXPTOVAL;
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Quick-Validation Set to Expire To"
								           , ultoa(cfg.level_expireto[i], tmp, 10), 1
								           , K_EDIT | K_NUMBER);
								cfg.level_expireto[i] = atoi(tmp);
								break;
						}
					}
				}
				break;
			case __COUNTER__:   /* Expired Account Values */
				done = 0;
				while (!done) {
					i = 0;
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%u", "Level", cfg.expired_level);
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Flag Set #1 to Remove"
					         , u32toaf(cfg.expired_flags1, str));
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Flag Set #2 to Remove"
					         , u32toaf(cfg.expired_flags2, str));
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Flag Set #3 to Remove"
					         , u32toaf(cfg.expired_flags3, str));
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Flag Set #4 to Remove"
					         , u32toaf(cfg.expired_flags4, str));
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Exemptions to Remove"
					         , u32toaf(cfg.expired_exempt, str));
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Restrictions to Add"
					         , u32toaf(cfg.expired_rest, str));
					opt[i][0] = 0;
					uifc.helpbuf =
						"`Expired Account Values:`\n"
						"\n"
						"If a user's account expires, the security levels for that account will\n"
						"be modified according to the settings of this menu.  The account's\n"
						"security level will be set to the value listed on this menu.  The `Flags`\n"
						"and `Exemptions` listed on this menu will be removed from the account\n"
						"if they are set.  The `Restrictions` listed will be added to the account.\n"
					;
					switch (uifc.list(WIN_ACT | WIN_MID | WIN_SAV, 0, 0, 60, &expired_dflt, 0
					                  , "Expired Account Values", opt)) {
						case -1:
							done = 1;
							break;
						case 0:
							ultoa(cfg.expired_level, str, 10);
							uifc.helpbuf =
								"`Expired Account Security Level:`\n"
								"\n"
								"This is the security level automatically given to expired user accounts.\n"
							;
							uifc.input(WIN_SAV | WIN_MID, 0, 0, "Security Level"
							           , str, 2, K_EDIT | K_NUMBER);
							cfg.expired_level = atoi(str);
							break;
						case 1:
							truncsp(u32toaf(cfg.expired_flags1, str));
							uifc.helpbuf =
								"`Expired Security Flags to Remove:`\n"
								"\n"
								"These are the security flags automatically removed when a user account\n"
								"has expired.\n"
							;
							uifc.input(WIN_SAV | WIN_MID, 0, 0, "Flags Set #1"
							           , str, 26, K_EDIT | K_UPPER | K_ALPHA);
							cfg.expired_flags1 = aftou32(str);
							break;
						case 2:
							truncsp(u32toaf(cfg.expired_flags2, str));
							uifc.helpbuf =
								"`Expired Security Flags to Remove:`\n"
								"\n"
								"These are the security flags automatically removed when a user account\n"
								"has expired.\n"
							;
							uifc.input(WIN_SAV | WIN_MID, 0, 0, "Flags Set #2"
							           , str, 26, K_EDIT | K_UPPER | K_ALPHA);
							cfg.expired_flags2 = aftou32(str);
							break;
						case 3:
							truncsp(u32toaf(cfg.expired_flags3, str));
							uifc.helpbuf =
								"`Expired Security Flags to Remove:`\n"
								"\n"
								"These are the security flags automatically removed when a user account\n"
								"has expired.\n"
							;
							uifc.input(WIN_SAV | WIN_MID, 0, 0, "Flags Set #3"
							           , str, 26, K_EDIT | K_UPPER | K_ALPHA);
							cfg.expired_flags3 = aftou32(str);
							break;
						case 4:
							truncsp(u32toaf(cfg.expired_flags4, str));
							uifc.helpbuf =
								"`Expired Security Flags to Remove:`\n"
								"\n"
								"These are the security flags automatically removed when a user account\n"
								"has expired.\n"
							;
							uifc.input(WIN_SAV | WIN_MID, 0, 0, "Flags Set #4"
							           , str, 26, K_EDIT | K_UPPER | K_ALPHA);
							cfg.expired_flags4 = aftou32(str);
							break;
						case 5:
							truncsp(u32toaf(cfg.expired_exempt, str));
							uifc.helpbuf =
								"`Expired Exemption Flags to Remove:`\n"
								"\n"
								"These are the exemptions that are automatically removed from a user\n"
								"account if it expires.\n"
							;
							uifc.input(WIN_SAV | WIN_MID, 0, 0, "Exemption Flags", str, 26
							           , K_EDIT | K_UPPER | K_ALPHA);
							cfg.expired_exempt = aftou32(str);
							break;
						case 6:
							truncsp(u32toaf(cfg.expired_rest, str));
							uifc.helpbuf =
								"`Expired Restriction Flags to Add:`\n"
								"\n"
								"These are the restrictions that are automatically added to a user\n"
								"account if it expires.\n"
							;
							uifc.input(WIN_SAV | WIN_MID, 0, 0, "Restriction Flags", str, 26
							           , K_EDIT | K_UPPER | K_ALPHA);
							cfg.expired_rest = aftou32(str);
							break;
					}
				}
				break;
			case __COUNTER__:   /* Quick-Validation Values */
				k = 0;
				while (1) {
					for (i = 0; i < 10; i++)
						snprintf(opt[i], MAX_OPLN, "%d  SL: %-2d  F1: %s"
						         , i, cfg.val_level[i], u32toaf(cfg.val_flags1[i], str));
					opt[i][0] = 0;
					i = 0;
					uifc.helpbuf =
						"`Quick-Validation Values:`\n"
						"\n"
						"This is a list of the ten quick-validation sets.  These sets are used to\n"
						"quickly set a user's security values (Level, Flags, Exemptions,\n"
						"Restrictions, Expiration Date, and Credits) with one key stroke.  The\n"
						"user's expiration date may be extended and additional credits may also\n"
						"be added using quick-validation sets.\n"
						"\n"
						"From within the `User Edit` function, a sysop can use the `V`alidate\n"
						"User command and select from this quick-validation list to change a\n"
						"user's security values with very few key-strokes.\n"
					;
					i = uifc.list(WIN_L2R | WIN_BOT | WIN_ACT | WIN_SAV, 0, 0, 0, &quick_dflt, 0
					              , "Quick-Validation Values", opt);
					if (i == -1)
						break;
					while (1) {
						j = 0;
						snprintf(opt[j++], MAX_OPLN, "%-22.22s%u", "Level", cfg.val_level[i]);
						snprintf(opt[j++], MAX_OPLN, "%-22.22s%s", "Flag Set #1"
						         , u32toaf(cfg.val_flags1[i], tmp));
						snprintf(opt[j++], MAX_OPLN, "%-22.22s%s", "Flag Set #2"
						         , u32toaf(cfg.val_flags2[i], tmp));
						snprintf(opt[j++], MAX_OPLN, "%-22.22s%s", "Flag Set #3"
						         , u32toaf(cfg.val_flags3[i], tmp));
						snprintf(opt[j++], MAX_OPLN, "%-22.22s%s", "Flag Set #4"
						         , u32toaf(cfg.val_flags4[i], tmp));
						snprintf(opt[j++], MAX_OPLN, "%-22.22s%s", "Exemptions"
						         , u32toaf(cfg.val_exempt[i], tmp));
						snprintf(opt[j++], MAX_OPLN, "%-22.22s%s", "Restrictions"
						         , u32toaf(cfg.val_rest[i], tmp));
						snprintf(opt[j++], MAX_OPLN, "%-22.22s%u days", "Extend Expiration"
						         , cfg.val_expire[i]);
						snprintf(opt[j++], MAX_OPLN, "%-22.22s%u", "Additional Credits"
						         , cfg.val_cdt[i]);
						opt[j][0] = 0;

						uifc_winmode_t wmode = WIN_RHT | WIN_SAV | WIN_ACT | WIN_EXTKEYS;
						if (i > 0)
							wmode |= WIN_LEFTKEY;
						if (i + 1 < 10)
							wmode |= WIN_RIGHTKEY;
						SAFEPRINTF(str, "Quick-Validation Set %d", i);
						j = uifc.list(wmode, 2, 1, 0, &k, 0, str, opt);
						if (j == -1)
							break;
						switch (j) {
							case -CIO_KEY_LEFT - 2:
								if (i > 0)
									i--;
								break;
							case -CIO_KEY_RIGHT - 2:
								if (i + 1 < 10)
									i++;
								break;
							case 0:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Level"
								           , ultoa(cfg.val_level[i], tmp, 10), 2
								           , K_NUMBER | K_EDIT);
								cfg.val_level[i] = atoi(tmp);
								break;
							case 1:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Flag Set #1"
								           , truncsp(u32toaf(cfg.val_flags1[i], tmp)), 26
								           , K_UPPER | K_ALPHA | K_EDIT);
								cfg.val_flags1[i] = aftou32(tmp);
								break;
							case 2:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Flag Set #2"
								           , truncsp(u32toaf(cfg.val_flags2[i], tmp)), 26
								           , K_UPPER | K_ALPHA | K_EDIT);
								cfg.val_flags2[i] = aftou32(tmp);
								break;
							case 3:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Flag Set #3"
								           , truncsp(u32toaf(cfg.val_flags3[i], tmp)), 26
								           , K_UPPER | K_ALPHA | K_EDIT);
								cfg.val_flags3[i] = aftou32(tmp);
								break;
							case 4:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Flag Set #4"
								           , truncsp(u32toaf(cfg.val_flags4[i], tmp)), 26
								           , K_UPPER | K_ALPHA | K_EDIT);
								cfg.val_flags4[i] = aftou32(tmp);
								break;
							case 5:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Exemption Flags"
								           , truncsp(u32toaf(cfg.val_exempt[i], tmp)), 26
								           , K_UPPER | K_ALPHA | K_EDIT);
								cfg.val_exempt[i] = aftou32(tmp);
								break;
							case 6:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Restriction Flags"
								           , truncsp(u32toaf(cfg.val_rest[i], tmp)), 26
								           , K_UPPER | K_ALPHA | K_EDIT);
								cfg.val_rest[i] = aftou32(tmp);
								break;
							case 7:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Days to Extend Expiration"
								           , ultoa(cfg.val_expire[i], tmp, 10), 4
								           , K_NUMBER | K_EDIT);
								cfg.val_expire[i] = atoi(tmp);
								break;
							case 8:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Additional Credits"
								           , ultoa(cfg.val_cdt[i], tmp, 10), 10
								           , K_NUMBER | K_EDIT);
								cfg.val_cdt[i] = atol(tmp);
								break;
						}
					}
				}
				break;
		}
	}
}

int edit_sys_timefmt(int page, int total)
{
	int   mode = WIN_SAV | WIN_MID;
	int   i = (cfg.sys_misc & SM_MILITARY) ? 1:0;
	char* opts[3] = { "12 hour (AM/PM)", "24 hour", NULL };
	uifc.helpbuf =
		"`Time Display Format:`\n"
		"\n"
		"If you would like the time-of-day to be displayed and entered in 24 hour\n"
		"format always, set this option to `24 hour`.\n"
	;
	if (page)
		mode = wiz_help(page, total, uifc.helpbuf);
	i = uifc.list(mode, 0, 10, 0, &i, 0
	              , "Time Display Format", opts);
	if (i == 0)
		cfg.sys_misc &= ~SM_MILITARY;
	else if (i == 1)
		cfg.sys_misc |= SM_MILITARY;
	return i;
}

int edit_sys_datefmt(int page, int total)
{
	int mode = WIN_SAV | WIN_MID;
	int i = cfg.sys_date_fmt;
	char* opts[] = {
		"Month First", "Day First", "Year First",
		NULL
	};
	uifc.helpbuf =
		"`Short Date Format:`\n"
		"\n"
		"If you would like dates to be entered and displayed in the traditional\n"
		"U.S. date format of month first (e.g. 'MM/DD/YY'), choose `Month First`.\n"
		"If you prefer the European traditional date format of day first, choose\n"
		"`Day First`.  If you and your users would prefer year first format, choose\n"
		"`Year First`.\n"
	;
	if (page) {
		mode = wiz_help(page, total, uifc.helpbuf);
		mode |= WIN_FIXEDHEIGHT;
		uifc.list_height = 3;
	}
	i = uifc.list(mode, 0, 11, 0, &i, 0
	              , "Short Date Format", opts);
	if (i < 0)
		return i;
	cfg.sys_date_fmt = i;
	return i;
}

int edit_sys_date_sep(int page, int total)
{
	char str[2] = { cfg.sys_date_sep };
	int mode = WIN_SAV | WIN_MID;
	uifc.helpbuf =
		"`Numeric Date Separator:`\n"
		"\n"
		"Choose a preferred short numeric date field separating character.\n"
		"\n"
		"Default: `/`\n"
	;
	if (page)
		mode = wiz_help(page, total, uifc.helpbuf);
	int i = uifc.input(mode, 0, 16, "Numeric Date Field Separator"
	                   , str, 1, K_EDIT | K_SPACE);
	if (i >= 0 && *str >= ' ')
		cfg.sys_date_sep = *str;
	return i;
}

int edit_sys_vdate_sep(int page, int total)
{
	char str[3] = { cfg.sys_vdate_sep };
	int mode = WIN_SAV | WIN_MID;
	uifc.helpbuf =
		"`Verbal Date Separator:`\n"
		"\n"
		"Choose a preferred verbal numeric date field separating character.\n"
		"\n"
		"Default: `'`\n"
	;
	if (page)
		mode = wiz_help(page, total, uifc.helpbuf);
	int i = uifc.input(mode, 0, 16, "Verbal Date Field Separator"
	                   , str, 2, K_EDIT | K_SPACE);
	if (i >= 0 && *str >= ' ')
		cfg.sys_vdate_sep = *str;
	return i;
}

int edit_sys_date_verbal(int page, int total)
{
	int    mode = WIN_SAV | WIN_MID;
	int    i = cfg.sys_date_verbal;
	time_t t = time(NULL);
	snprintf(opt[0], MAX_OPLN, "Numeric (e.g. %s)", unixtodstr(&cfg, (time32_t)t, tmp));
	snprintf(opt[1], MAX_OPLN, "Verbal  (e.g. %s)", verbal_datestr(&cfg, (time32_t)t, tmp));
	opt[2][0] = '\0';
	uifc.helpbuf =
		"`Short Date Display Format:`\n"
		"\n"
		"If you would like short (8 character) dates to be displayed using verbal\n"
		"(non-numeric, less ambiguous) month name abbreviations, choose `Verbal`.";
	;
	if (page) {
		mode = wiz_help(page, total, uifc.helpbuf);
		mode |= WIN_FIXEDHEIGHT;
		uifc.list_height = 2;
	}
	i = uifc.list(mode, 0, 11, 0, &i, 0
	              , "Short Date Display Format", opt);
	if (i < 0)
		return i;
	cfg.sys_date_verbal = i;
	return i;
}

int edit_sys_alias_policy(int page, int total)
{
	int mode = WIN_SAV | WIN_MID;
	int i = (cfg.uq & UQ_ALIASES) ? 0:1;
	uifc.helpbuf =
		"`Allow Users to Use Aliases:`\n"
		"\n"
		"If you want the users of your system to be allowed to be known by a\n"
		"false name, handle, or alias, set this option to `Yes`.  If you want all\n"
		"users on your system to be known only by their real names, select `No`.\n"
		"\n"
		"Note: real names may be required to consist of more than one word.\n"
	;
	if (page)
		mode = wiz_help(page, total, uifc.helpbuf);
	i = uifc.list(mode, 0, 10, 0, &i, 0
	              , "Allow Users to Use Aliases", uifcYesNoOpts);
	if (!i && !(cfg.uq & UQ_ALIASES)) {
		cfg.uq |= UQ_ALIASES;
	}
	else if (i == 1 && cfg.uq & UQ_ALIASES) {
		cfg.uq &= ~UQ_ALIASES;
	}
	return i;
}

int edit_sys_newuser_fback_policy(int page, int total)
{
	int  mode = WIN_SAV | WIN_MID;
	char str[128];
	ultoa(cfg.valuser, str, 10);
	uifc.helpbuf =
		"`Require New User Feedback:`\n"
		"\n"
		"When a caller registers as a new user, they can be required to send a\n"
		"validation request via feedback message to the sysop.\n"
		"\n"
		"If you want new users of this system to be forced to send validation\n"
		"feedback, set this value to the number of the user to whom the feedback\n"
		"is sent, e.g. `1` for user number one.\n"
		"\n"
		"This feature can be disabled by setting this value to `0`, allowing new\n"
		"users to register and logon without sending validation feedback.\n"
	;
	if (page)
		mode = wiz_help(page, total, uifc.helpbuf);
	int i = uifc.input(mode, 0, 16, "Require New User Feedback to (0=Nobody)"
	                   , str, 5, K_NUMBER | K_EDIT);
	if (i >= 0)
		cfg.valuser = atoi(str);
	return i;
}

void reencrypt_keys(const char *old_pass, const char* new_pass)
{
	if (fexist("ssl.cert") || fexist("cryptlib.key") || fexist("letsyncrypt.key")) {
		CRYPT_KEYSET  ssl_keyset;
		CRYPT_CONTEXT ssl_context = -1;
		int           status;
		int           ignoreme;

		if (cryptStatusOK(status = cryptKeysetOpen(&ssl_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, "ssl.cert", CRYPT_KEYOPT_NONE)))
			if (cryptStatusOK(status = cryptGetPrivateKey(ssl_keyset, &ssl_context, CRYPT_KEYID_NAME, "ssl_cert", old_pass)))
				if (cryptStatusOK(status = cryptDeleteKey(ssl_keyset, CRYPT_KEYID_NAME, "ssl_cert"))) {
					ignoreme = cryptAddPrivateKey(ssl_keyset, ssl_context, new_pass);
					cryptKeysetClose(ssl_keyset);
				}

		if (cryptStatusOK(status = cryptKeysetOpen(&ssl_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, "cryptlib.key", CRYPT_KEYOPT_NONE)))
			if (cryptStatusOK(status = cryptGetPrivateKey(ssl_keyset, &ssl_context, CRYPT_KEYID_NAME, "ssh_server", old_pass)))
				if (cryptStatusOK(status = cryptDeleteKey(ssl_keyset, CRYPT_KEYID_NAME, "ssh_server"))) {
					ignoreme = cryptAddPrivateKey(ssl_keyset, ssl_context, new_pass);
					cryptKeysetClose(ssl_keyset);
				}

		if (cryptStatusOK(status = cryptKeysetOpen(&ssl_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, "letsyncrypt.key", CRYPT_KEYOPT_NONE))) {
			char  value[INI_MAX_VALUE_LEN];
			char* host = "acme-v02.api.letsencrypt.org";
			FILE* fp = fopen("letsyncrypt.ini", "r");
			if (fp != NULL) {
				host = iniReadString(fp, "state", "host", host, value);
				fclose(fp);
			}
			if (cryptStatusOK(status = cryptGetPrivateKey(ssl_keyset, &ssl_context, CRYPT_KEYID_NAME, host, old_pass)))
				if (cryptStatusOK(status = cryptDeleteKey(ssl_keyset, CRYPT_KEYID_NAME, host))) {
					ignoreme = cryptAddPrivateKey(ssl_keyset, ssl_context, new_pass);
					cryptKeysetClose(ssl_keyset);
				}
		}
		(void)ignoreme;
	}
}

/* These correlate with the LOG_* definitions in syslog.h/gen_defs.h */
static char* errLevelStringList[]
    = {"Emergency", "Alert", "Critical", "Error", NULL};

void cfg_notify(void)
{
	char       str[128];
	static int dflt;

	while (1) {
		int i = 0;
		if (cfg.valuser)
			SAFEPRINTF(str, "User #%u", cfg.valuser);
		else
			SAFECOPY(str, "Nobody");
		snprintf(opt[i++], MAX_OPLN, "%-23.23s%s", "New User Feedback", str);
		if (cfg.erruser)
			SAFEPRINTF(str, "User #%u", cfg.erruser);
		else
			SAFECOPY(str, "Nobody");
		snprintf(opt[i++], MAX_OPLN, "%-23.23s%s", "Error Notifications", str);
		if (cfg.erruser)
			snprintf(opt[i++], MAX_OPLN, "%-23.23s%s", "Error Level", errLevelStringList[cfg.errlevel]);
		opt[i][0] = '\0';
		uifc.helpbuf =
			"`System Operator Notifications:`\n"
			"\n"
			"Configure settings related to notifications sent to the operators of\n"
			"the system via emails and instant messages.\n"
		;
		switch (uifc.list(WIN_BOT | WIN_RHT | WIN_ACT | WIN_SAV, 0, 0, 0, &dflt, 0
		                  , "System Operator Notifications", opt)) {
			case -1:
				return;
				break;
			case 0:
				edit_sys_newuser_fback_policy(false, false);
				break;
			case 1:
				ultoa(cfg.erruser, str, 10);
				uifc.helpbuf =
					"`Error Notifications:`\n"
					"\n"
					"When an error has occurred, a notification message can be sent to a\n"
					"configured user number (i.e. a sysop).  This feature can be disabled by\n"
					"setting this value to `0`.  The normal value of this option is `1` for\n"
					"user number one.\n"
					"\n"
					"Note: error messages are always logged as well (e.g. to `data/error.log`)."
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 13, "Send Error Notifications to (0=Nobody)"
				           , str, 5, K_NUMBER | K_EDIT);
				cfg.erruser = atoi(str);
				break;
			case 2:
				uifc.helpbuf =
					"`Notification Error Level`\n"
					"\n"
					"Select the minimum severity of error messages that should be forwarded\n"
					"to the Error Notification User.  `Error` is the lowest severity level\n"
					"while `Emergency` is the highest.\n"
					"\n"
					"The default minimum level for error notifications is `Critical`.";
				int i = cfg.errlevel;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0, "Notification Error Level", errLevelStringList);
				if (i >= 0 && i <= LOG_ERR)
					cfg.errlevel = i;
				break;
		}
	}
}

void newuser_qwk_opts(void)
{
	static int cur, bar;

	uifc.helpbuf =
		"`New User QWK Message Packet Settings:`\n"
		"\n"
		"This menu contains the default state of new user QWK message packet\n"
		"related settings.\n"
		"\n"
		"Note that while most of the options may be toggled between 2 states\n"
		"(`Yes` and `No`), some options offer 3 states:\n"
		"  `Include Ctrl-A Codes:` Yes, Expand (to ANSI), and No\n"
		"  `Include E-mail Messages`: All, Unread (only), and No\n"
	;
	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-27.27s %s"
		         , "Include QWKE Extensions"
		         , (cfg.new_qwk & QWK_EXT) ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-27.27s %s"
		         , "Include Ctrl-A Codes"
		         , (cfg.new_qwk & QWK_RETCTLA) ? "Yes" : (cfg.new_qwk & QWK_EXPCTLA) ? "Expand" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-27.27s %s"
		         , "Include UTF-8 Characters"
		         , (cfg.new_qwk & QWK_UTF8) ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-27.27s %s"
		         , "Include New Files List"
		         , (cfg.new_qwk & QWK_FILES) ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-27.27s %s"
		         , "Include File Attachments"
		         , (cfg.new_qwk & QWK_ATTACH) ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-27.27s %s"
		         , "Include Messages From Self"
		         , (cfg.new_qwk & QWK_BYSELF) ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-27.27s %s"
		         , "Include E-mail Messages"
		         , (cfg.new_qwk & QWK_ALLMAIL) ? "All" : (cfg.new_qwk & QWK_EMAIL) ? "Unread" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-27.27s %s"
		         , "Delete Downloaded E-mail"
		         , (cfg.new_qwk & QWK_DELMAIL) ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-27.27s %s"
		         , "Include Index Files"
		         , (cfg.new_qwk & QWK_NOINDEX) ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-27.27s %s"
		         , "Include Control Files"
		         , (cfg.new_qwk & QWK_NOCTRL) ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-27.27s %s"
		         , "Include Time Zone Kludges"
		         , (cfg.new_qwk & QWK_TZ) ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-27.27s %s"
		         , "Include Via/Routing Kludges"
		         , (cfg.new_qwk & QWK_VIA) ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-27.27s %s"
		         , "Include Message-ID Kludges"
		         , (cfg.new_qwk & QWK_MSGID) ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-27.27s %s"
		         , "Include HEADERS.DAT File"
		         , (cfg.new_qwk & QWK_HEADERS) ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-27.27s %s"
		         , "Include VOTING.DAT File"
		         , (cfg.new_qwk & QWK_VOTING) ? "Yes" : "No");
		opt[i][0] = '\0';
		switch (uifc.list(WIN_BOT | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "QWK Message Packet Settings", opt)) {
			case -1:
				return;
			case 0:
				cfg.new_qwk ^= QWK_EXT;
				break;
			case 1:
				switch (cfg.new_qwk & (QWK_RETCTLA | QWK_EXPCTLA)) {
					case QWK_RETCTLA:
						cfg.new_qwk ^= QWK_RETCTLA;
						cfg.new_qwk |= QWK_EXPCTLA;
						break;
					case QWK_EXPCTLA:
						cfg.new_qwk ^= QWK_EXPCTLA;
						break;
					default:
						cfg.new_qwk |= QWK_RETCTLA;
						break;
				}
				break;
			case 2:
				cfg.new_qwk ^= QWK_UTF8;
				break;
			case 3:
				cfg.new_qwk ^= QWK_FILES;
				break;
			case 4:
				cfg.new_qwk ^= QWK_ATTACH;
				break;
			case 5:
				cfg.new_qwk ^= QWK_BYSELF;
				break;
			case 6:
				switch (cfg.new_qwk & (QWK_ALLMAIL | QWK_EMAIL)) {
					case QWK_ALLMAIL:
						cfg.new_qwk ^= QWK_ALLMAIL;
						cfg.new_qwk |= QWK_EMAIL;
						break;
					case QWK_EMAIL:
						cfg.new_qwk ^= QWK_EMAIL;
						break;
					default:
						cfg.new_qwk |= QWK_ALLMAIL;
						break;
				}
				break;
			case 7:
				cfg.new_qwk ^= QWK_DELMAIL;
				break;
			case 8:
				cfg.new_qwk ^= QWK_NOINDEX;
				break;
			case 9:
				cfg.new_qwk ^= QWK_NOCTRL;
				break;
			case 10:
				cfg.new_qwk ^= QWK_TZ;
				break;
			case 11:
				cfg.new_qwk ^= QWK_VIA;
				break;
			case 12:
				cfg.new_qwk ^= QWK_MSGID;
				break;
			case 13:
				cfg.new_qwk ^= QWK_HEADERS;
				break;
			case 14:
				cfg.new_qwk ^= QWK_VOTING;
				break;
		}
	}
}

bool edit_loadable_module(const char* name, char* cmd, char* ars)
{
	char title[128];
	int  i;
	int  cur = 0, bar = 0;
	char org_cmd[LEN_CMD + 1];
	char org_ars[LEN_ARSTR + 1];

	SAFECOPY(org_cmd, cmd);
	SAFECOPY(org_ars, ars);
	snprintf(title, sizeof title, "%s Module", name);
	while (1) {
		i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-32.32s %-32.32s", "Name / Command-line", cmd);
		snprintf(opt[i++], MAX_OPLN, "%-32.32s %-32.32s", "Access Requirements", ars);
		opt[i][0] = 0;
		switch(uifc.list(WIN_BOT | WIN_SAV | WIN_ACT | WIN_CHE, 0, 0, 0, &cur, &bar, title, opt)) {
			case 0:
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Module Name/Command-line", cmd, LEN_CMD, K_EDIT);
				break;
			case 1:
				getar(title, ars);
				break;
			case 2:
				return true;
			default:
				if (strcmp(org_cmd, cmd) != 0 || strcmp(org_ars, ars) != 0) {
					switch(uifc.list(WIN_SAV | WIN_MID, 0, 0, 0, 0, 0, "Save changes", uifcYesNoOpts)) {
						case 0:
							return true;
						case 1:
							return false;
					}
					break;
				}
				return false;
		}
	}
}

bool cfg_loadable_modules(const char* name, struct loadable_module* mod, int top, int minimum_count)
{
	char title[128];
	int i;
	int cur = 0, bar = 0;
	bool changed = false;

	if (++top > (int)uifc.scrn_len - 10)
		top = uifc.scrn_len - 10;

	snprintf(title, sizeof title, "%s Modules", name);
	while (1) {
		for (i = 0; mod->cmd != NULL && mod->cmd[i] != NULL; ++i)
			snprintf(opt[i], MAX_OPLN, "%-32.32s %-32.32s", mod->cmd[i], mod->ars[i]);
		opt[i][0] = 0;
		uifc_winmode_t wmode = WIN_RHT | WIN_SAV | WIN_ACT | WIN_INS | WIN_INSACT | WIN_XTR;
		if (i > minimum_count)
			wmode |= WIN_DEL;
		i = uifc.list(wmode, 2, top, 0, &cur, &bar, title, opt);
		if (i == -1)
			return changed;
		char cmd[LEN_CMD + 1];
		char ars[LEN_ARSTR + 1];
		if ((i & MSK_ON) == MSK_INS) {
			i &= MSK_OFF;
			SAFECOPY(cmd, "modname");
			*ars = '\0';
			if (edit_loadable_module(name, cmd, ars)) {
				strListInsert(&mod->cmd, cmd, i);
				strListInsert(&mod->ars, ars, i);
				changed = true;
			}
			continue;
		}
		if ((i & MSK_ON) == MSK_DEL) {
			i &= MSK_OFF;
			strListFastDelete(mod->cmd, i, 1);
			strListFastDelete(mod->ars, i, 1);
			changed = true;
			continue;
		}
		SAFECOPY(cmd, mod->cmd[i]);
		SAFECOPY(ars, mod->ars[i]);
		if (edit_loadable_module(name, cmd, ars)) {
			if (strcmp(cmd, mod->cmd[i]) != 0) {
				free(mod->cmd[i]);
				mod->cmd[i] = strdup(cmd);
				changed = true;
			}
			if (strcmp(ars, mod->ars[i]) != 0) {
				free(mod->ars[i]);
				mod->ars[i] = strdup(ars);
				changed = true;
			}
		}
	}
}

void sys_cfg(void)
{
	static int sys_dflt, adv_dflt, tog_dflt, new_dflt;
	static int tog_bar;
	static int adv_bar;
	static int mod_dflt, mod_bar;
	static int uq_cur, uq_bar;
	static int newtog_cur, newtog_bar;
	char       str[81], done = 0;
	char       dstr[9];
	int        i, j;
	scfg_t     saved_cfg = cfg;
	bool       mods_changed = false;
	char       sys_pass[sizeof(cfg.sys_pass)];
	SAFECOPY(sys_pass, cfg.sys_pass);
	while (1) {
		i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "BBS Name", cfg.sys_name);
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Location", cfg.sys_location);
		snprintf(opt[i++], MAX_OPLN, "%-20s%s%s %s", "Local Time Zone"
		         , cfg.sys_timezone == SYS_TIMEZONE_AUTO ? "Auto: " : ""
		         , smb_zonestr(sys_timezone(&cfg), NULL)
		         , SMB_TZ_HAS_DST(cfg.sys_timezone) && cfg.sys_misc & SM_AUTO_DST ? "(Auto-DST)" : "");
		snprintf(opt[i++], MAX_OPLN, "%-20s%s, display: %s", "Short Date Format"
		         , date_format(&cfg, str, sizeof str, false)
		         , datestr(&cfg, time(NULL), dstr));
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Operator", cfg.sys_op);

		strcpy(opt[i++], "Notifications...");
		strcpy(opt[i++], "Toggle Options...");
		strcpy(opt[i++], "New User Values...");
		strcpy(opt[i++], "New User Prompts...");
		strcpy(opt[i++], "Security Options...");
		strcpy(opt[i++], "Advanced Options...");
		strcpy(opt[i++], "Loadable Modules...");
		strcpy(opt[i++], "Extra Attribute Codes...");
		opt[i][0] = 0;
		uifc.helpbuf =
			"`System Configuration:`\n"
			"\n"
			"This menu contains options and sub-menus of options that affect the\n"
			"entire BBS and the Synchronet Terminal Server in particular.\n"
		;
		uifc.changes = mods_changed || memcmp(&saved_cfg, &cfg, sizeof(saved_cfg)) != 0;
		switch (uifc.list(WIN_ORG | WIN_ACT | WIN_CHE, 0, 0, 0, &sys_dflt, 0
		                  , "System Configuration", opt)) {
			case -1:
				i = save_changes(WIN_MID);
				if (i == -1)
					break;
				if (!i) {
					cfg.new_install = new_install;
					if (strcmp(sys_pass, cfg.sys_pass) != 0) {
						reencrypt_keys(sys_pass, cfg.sys_pass);
					}
					save_main_cfg(&cfg);
					refresh_cfg(&cfg);
				}
				return;
			case 0:
				edit_sys_name(false, false);
				break;
			case 1:
				edit_sys_location(false, false);
				break;
			case 2:
				edit_sys_timezone(false, false);
				break;
			case 3:
				if (edit_sys_datefmt(false, false) < 0)
					break;
				if (edit_sys_date_sep(false, 0) < 0)
					break;
				if (edit_sys_date_verbal(false, false) < 0)
					break;
				if (cfg.sys_date_verbal)
					edit_sys_vdate_sep(false, 0);
				break;
			case 4:
				edit_sys_operator(false, false);
				break;
			case 5:
				cfg_notify();
				break;
			case 6:    /* Toggle Options */
				done = 0;
				while (!done) {
					i = 0;
					snprintf(opt[i++], MAX_OPLN, "%-35s%s", "Allow User Aliases"
					         , cfg.uq & UQ_ALIASES ? "Yes" : "No");
					snprintf(opt[i++], MAX_OPLN, "%-35s%s", "Allow Time Banking"
					         , cfg.sys_misc & SM_TIMEBANK ? "Yes" : "No");
					snprintf(opt[i++], MAX_OPLN, "%-35s%s", "Allow Credit Conversions"
					         , cfg.sys_misc & SM_NOCDTCVT ? "No" : "Yes");
					snprintf(opt[i++], MAX_OPLN, "%-35s%s", "Short Sysop Page"
					         , cfg.sys_misc & SM_SHRTPAGE ? "Yes" : "No");
					snprintf(opt[i++], MAX_OPLN, "%-35s%s", "Include Sysop in Statistics"
					         , cfg.sys_misc & SM_SYSSTAT ? "Yes" : "No");
					snprintf(opt[i++], MAX_OPLN, "%-35s%s", "Use Location in User Lists"
					         , cfg.sys_misc & SM_LISTLOC ? "Yes" : "No");
					snprintf(opt[i++], MAX_OPLN, "%-35s%s", "Military (24 hour) Time Format"
					         , cfg.sys_misc & SM_MILITARY ? "Yes" : "No");
					snprintf(opt[i++], MAX_OPLN, "%-35s%s", "Display Sys Info During Logon"
					         , cfg.sys_misc & SM_NOSYSINFO ? "No" : "Yes");
					snprintf(opt[i++], MAX_OPLN, "%-35s%s", "Display Node List During Logon"
					         , cfg.sys_misc & SM_NONODELIST ? "No" : "Yes");
					snprintf(opt[i++], MAX_OPLN, "%-35s%s", "Mouse Hot-spots in Menus/Prompts"
					         , cfg.sys_misc & SM_MOUSE_HOT ? "Yes" : "No");
					snprintf(opt[i++], MAX_OPLN, "%-35s%s", "Spinning Cursor at Pause Prompts"
					         , cfg.spinning_pause_prompt ? "Yes" : "No");
					opt[i][0] = 0;
					uifc.helpbuf =
						"`System Toggle Options:`\n"
						"\n"
						"This is a menu of system related options that can be toggled between\n"
						"two or more states, such as `Yes` and `No`.\n"
					;
					switch (uifc.list(WIN_ACT | WIN_BOT | WIN_RHT, 0, 0, 43, &tog_dflt, &tog_bar
					                  , "Toggle Options", opt)) {
						case -1:
							done = 1;
							break;
						case 0:
							edit_sys_alias_policy(false, false);
							break;
						case 1:
							i = cfg.sys_misc & SM_TIMEBANK ? 0:1;
							uifc.helpbuf =
								"`Allow Time Banking:`\n"
								"\n"
								"If you want the users of your system to be allowed to deposit\n"
								"any extra time they may have left during a call into their minute bank,\n"
								"set this option to `Yes`.  If this option is set to `No`, then the only\n"
								"way a user may get minutes in their minute bank is to purchase them\n"
								"with credits.\n"
							;
							i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
							              , "Allow Users to Deposit Time in Minute Bank", uifcYesNoOpts);
							if (!i && !(cfg.sys_misc & SM_TIMEBANK)) {
								cfg.sys_misc |= SM_TIMEBANK;
							}
							else if (i == 1 && cfg.sys_misc & SM_TIMEBANK) {
								cfg.sys_misc &= ~SM_TIMEBANK;
							}
							break;
						case 2:
							i = cfg.sys_misc & SM_NOCDTCVT ? 1:0;
							uifc.helpbuf =
								"`Allow Credits to be Converted into Minutes:`\n"
								"\n"
								"If you want the users of your system to be allowed to convert any\n"
								"credits they may have into minutes for their minute bank, set this\n"
								"option to `Yes`.\n"
							;
							i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
							              , "Allow Users to Convert Credits into Minutes"
							              , uifcYesNoOpts);
							if (!i && cfg.sys_misc & SM_NOCDTCVT) {
								cfg.sys_misc &= ~SM_NOCDTCVT;
							}
							else if (i == 1 && !(cfg.sys_misc & SM_NOCDTCVT)) {
								cfg.sys_misc |= SM_NOCDTCVT;
							}
							break;
						case 3:
							i = cfg.sys_misc & SM_SHRTPAGE ? 0:1;
							uifc.helpbuf =
								"`Short Sysop Page:`\n"
								"\n"
								"If you would like the sysop page to be a short series of beeps rather\n"
								"than continuous random tones, set this option to `Yes`.\n"
							;
							i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0, "Short Sysop Page"
							              , uifcYesNoOpts);
							if (i == 1 && cfg.sys_misc & SM_SHRTPAGE) {
								cfg.sys_misc &= ~SM_SHRTPAGE;
							}
							else if (!i && !(cfg.sys_misc & SM_SHRTPAGE)) {
								cfg.sys_misc |= SM_SHRTPAGE;
							}
							break;
						case 4:
							i = cfg.sys_misc & SM_SYSSTAT ? 0:1;
							uifc.helpbuf =
								"`Include Sysop Activity in System Statistics:`\n"
								"\n"
								"If you want sysops to be included in the statistical data of the BBS,\n"
								"set this option to `Yes`.  The suggested setting for this option is\n"
								"`No` so that statistical data will only reflect user usage and not\n"
								"include sysop maintenance activity.\n"
							;
							i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
							              , "Include Sysop Activity in System Statistics"
							              , uifcYesNoOpts);
							if (!i && !(cfg.sys_misc & SM_SYSSTAT)) {
								cfg.sys_misc |= SM_SYSSTAT;
							}
							else if (i == 1 && cfg.sys_misc & SM_SYSSTAT) {
								cfg.sys_misc &= ~SM_SYSSTAT;
							}
							break;
						case 5:
							i = cfg.sys_misc & SM_LISTLOC ? 0:1;
							uifc.helpbuf =
								"`User Location in User Lists:`\n"
								"\n"
								"If you want user locations (city, state) displayed in the user lists,\n"
								"set this option to `Yes`.  If this option is set to `No`, the user notes\n"
								"(if they exist) are displayed in the user lists.\n"
							;
							i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
							              , "User Location (Instead of Note) in User Lists"
							              , uifcYesNoOpts);
							if (!i && !(cfg.sys_misc & SM_LISTLOC)) {
								cfg.sys_misc |= SM_LISTLOC;
							}
							else if (i == 1 && cfg.sys_misc & SM_LISTLOC) {
								cfg.sys_misc &= ~SM_LISTLOC;
							}
							break;
						case 6:
							edit_sys_timefmt(false, false);
							break;
						case 7:
							i = cfg.sys_misc & SM_NOSYSINFO ? 1:0;
							uifc.helpbuf =
								"`Display System Information During Logon:`\n"
								"\n"
								"If you want system information displayed during logon, set this option\n"
								"to `Yes`.\n"
							;
							i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
							              , "Display System Information During Logon", uifcYesNoOpts);
							if (!i && cfg.sys_misc & SM_NOSYSINFO) {
								cfg.sys_misc &= ~SM_NOSYSINFO;
							}
							else if (i == 1 && !(cfg.sys_misc & SM_NOSYSINFO)) {
								cfg.sys_misc |= SM_NOSYSINFO;
							}
							break;
						case 8:
							i = cfg.sys_misc & SM_NONODELIST ? 1:0;
							uifc.helpbuf =
								"`Display Active Node List During Logon:`\n"
								"\n"
								"If you want the active nodes displayed during logon, set this option\n"
								"to `Yes`.\n"
							;
							i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
							              , "Display Active Node List During Logon", uifcYesNoOpts);
							if (!i && cfg.sys_misc & SM_NONODELIST) {
								cfg.sys_misc &= ~SM_NONODELIST;
							}
							else if (i == 1 && !(cfg.sys_misc & SM_NONODELIST)) {
								cfg.sys_misc |= SM_NONODELIST;
							}
							break;
						case 9:
							i = cfg.sys_misc & SM_MOUSE_HOT ? 0:1;
							uifc.helpbuf =
								"`Mouse Hot-spots in Menus and Prompts:`\n"
								"\n"
								"If you want to disable all mouse `Hot-spots` in the BBS menus and prompts,\n"
								"set this option to `No`.\n"
							;
							i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
							              , "Mouse Hot-spots in Menus and Prompts", uifcYesNoOpts);
							if (i == 1 && cfg.sys_misc & SM_MOUSE_HOT) {
								cfg.sys_misc &= ~SM_MOUSE_HOT;
							}
							else if (i == 0 && !(cfg.sys_misc & SM_MOUSE_HOT)) {
								cfg.sys_misc |= SM_MOUSE_HOT;
							}
							break;
						case 10:
							i = cfg.spinning_pause_prompt ? 0:1;
							uifc.helpbuf =
								"`Spinning Cursor at Pause Prompts:`\n"
								"\n"
								"If you want to display a spinning cursor at the [Hit a key] prompt,\n"
								"set this option to `Yes`.\n"
							;
							i = uifc.list(WIN_MID | WIN_SAV, 0, 10, 0, &i, 0
							              , "Spinning Cursor at Pause Prompts", uifcYesNoOpts);
							if (i >= 0)
								cfg.spinning_pause_prompt = !cfg.spinning_pause_prompt;
							break;

					}
				}
				break;
			case 7:    /* New User Values */
				done = 0;
				while (!done) {
					i = 0;
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%u", "Level", cfg.new_level);
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Flag Set #1"
					         , u32toaf(cfg.new_flags1, str));
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Flag Set #2"
					         , u32toaf(cfg.new_flags2, str));
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Flag Set #3"
					         , u32toaf(cfg.new_flags3, str));
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Flag Set #4"
					         , u32toaf(cfg.new_flags4, str));
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Exemptions"
					         , u32toaf(cfg.new_exempt, str));
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Restrictions"
					         , u32toaf(cfg.new_rest, str));
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Expiration Days"
					         , ultoa(cfg.new_expire, str, 10));

					u32toac(cfg.new_cdt, str, ',');
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Credits", str);
					u32toac(cfg.new_min, str, ',');
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Minutes", str);
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Editor"
					         , cfg.new_xedit);
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Command Shell"
					         , cfg.new_shell >= cfg.total_shells ? "<invalid>" : cfg.shell[cfg.new_shell]->code);
					if (cfg.new_prot != ' ')
						sprintf(str, "%c", cfg.new_prot);
					else
						strcpy(str, "None");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Download Protocol", str);
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%hu", "Days of New Messages", cfg.new_msgscan_init);
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Gender Options", cfg.new_genders);
					strcpy(opt[i++], "Default Toggles...");
					strcpy(opt[i++], "QWK Packet Settings...");
					opt[i][0] = 0;
					uifc.helpbuf =
						"`New User Values:`\n"
						"\n"
						"This menu allows you to determine the default values (settings,\n"
						"security) assigned to newly-created user accounts.\n"
						"\n"
						"Some of thes settings are also automatically applied to 'Guest'\n"
						"(G-restricted) account logons.\n"
					;
					switch (uifc.list(WIN_ACT | WIN_BOT | WIN_RHT | WIN_SAV, 0, 0, 0, &new_dflt, 0
					                  , "New User Values", opt)) {
						case -1:
							done = 1;
							break;
						case 0:
							ultoa(cfg.new_level, str, 10);
							uifc.helpbuf =
								"`New User Security Level:`\n"
								"\n"
								"This is the security level automatically given to new users.\n"
								"\n"
								"Recommended setting: `50` or less.\n"
							;
							uifc.input(WIN_SAV | WIN_MID, 0, 0, "Security Level"
							           , str, 2, K_EDIT | K_NUMBER);
							cfg.new_level = atoi(str);
							break;
						case 1:
							truncsp(u32toaf(cfg.new_flags1, str));
							uifc.helpbuf =
								"`New User Security Flags:`\n"
								"\n"
								"These are the security flags automatically given to new users.\n"
							;
							uifc.input(WIN_SAV | WIN_MID, 0, 0, "Flag Set #1"
							           , str, 26, K_EDIT | K_UPPER | K_ALPHA);
							cfg.new_flags1 = aftou32(str);
							break;
						case 2:
							truncsp(u32toaf(cfg.new_flags2, str));
							uifc.helpbuf =
								"`New User Security Flags:`\n"
								"\n"
								"These are the security flags automatically given to new users.\n"
							;
							uifc.input(WIN_SAV | WIN_MID, 0, 0, "Flag Set #2"
							           , str, 26, K_EDIT | K_UPPER | K_ALPHA);
							cfg.new_flags2 = aftou32(str);
							break;
						case 3:
							truncsp(u32toaf(cfg.new_flags3, str));
							uifc.helpbuf =
								"`New User Security Flags:`\n"
								"\n"
								"These are the security flags automatically given to new users.\n"
							;
							uifc.input(WIN_SAV | WIN_MID, 0, 0, "Flag Set #3"
							           , str, 26, K_EDIT | K_UPPER | K_ALPHA);
							cfg.new_flags3 = aftou32(str);
							break;
						case 4:
							truncsp(u32toaf(cfg.new_flags4, str));
							uifc.helpbuf =
								"`New User Security Flags:`\n"
								"\n"
								"These are the security flags automatically given to new users.\n"
							;
							uifc.input(WIN_SAV | WIN_MID, 0, 0, "Flag Set #4"
							           , str, 26, K_EDIT | K_UPPER | K_ALPHA);
							cfg.new_flags4 = aftou32(str);
							break;
						case 5:
							truncsp(u32toaf(cfg.new_exempt, str));
							uifc.helpbuf =
								"`New User Exemption Flags:`\n"
								"\n"
								"These are the exemptions that are automatically given to new users.\n"
								"\n"
								"See `http://wiki.synchro.net/access:exemptions` for details.\n"
							;
							uifc.input(WIN_SAV | WIN_MID, 0, 0, "Exemption Flags", str, 26
							           , K_EDIT | K_UPPER | K_ALPHA);
							cfg.new_exempt = aftou32(str);
							break;
						case 6:
							truncsp(u32toaf(cfg.new_rest, str));
							uifc.helpbuf =
								"`New User Restriction Flags:`\n"
								"\n"
								"These are the restrictions that are automatically given to new users.\n"
								"\n"
								"See `http://wiki.synchro.net/access:restrictions` for details.\n"
							;
							uifc.input(WIN_SAV | WIN_MID, 0, 0, "Restriction Flags", str, 26
							           , K_EDIT | K_UPPER | K_ALPHA);
							cfg.new_rest = aftou32(str);
							break;
						case 7:
							ultoa(cfg.new_expire, str, 10);
							uifc.helpbuf =
								"`New User Expiration Days:`\n"
								"\n"
								"If you wish new users to have an automatic expiration date, set this\n"
								"value to the number of days before the user will expire. To disable\n"
								"New User expiration, set this value to `0`.\n"
							;
							uifc.input(WIN_SAV | WIN_MID, 0, 0, "Expiration Days", str, 4
							           , K_EDIT | K_NUMBER);
							cfg.new_expire = atoi(str);
							break;
						case 8:
							ultoa(cfg.new_cdt, str, 10);
							uifc.helpbuf =
								"`New User Credits:`\n"
								"\n"
								"This is the amount of credits that are automatically given to new users.\n"
							;
							uifc.input(WIN_SAV | WIN_MID, 0, 0, "Credits", str, 10
							           , K_EDIT | K_NUMBER);
							cfg.new_cdt = atol(str);
							break;
						case 9:
							ultoa(cfg.new_min, str, 10);
							uifc.helpbuf =
								"`New User Minutes:`\n"
								"\n"
								"This is the number of extra minutes automatically given to new users.\n"
							;
							uifc.input(WIN_SAV | WIN_MID, 0, 0, "Minutes (Time Credit)"
							           , str, 10, K_EDIT | K_NUMBER);
							cfg.new_min = atol(str);
							break;
						case 10:
							if (!cfg.total_xedits) {
								uifc.msg("No External Editors Configured");
								break;
							}
							strcpy(opt[0], "Internal");
							for (i = 1; i <= cfg.total_xedits; i++)
								strcpy(opt[i], cfg.xedit[i - 1]->code);
							opt[i][0] = 0;
							i = 0;
							uifc.helpbuf =
								"`New User Editor:`\n"
								"\n"
								"You can use this option to select the default editor for new users\n"
								"and 'Guest' (G-restriced) account logons.\n"
							;
							i = uifc.list(WIN_SAV | WIN_RHT, 2, 1, 13, &i, 0, "Editors", opt);
							if (i == -1)
								break;
							if (i && i <= cfg.total_xedits)
								SAFECOPY(cfg.new_xedit, cfg.xedit[i - 1]->code);
							else
								cfg.new_xedit[0] = 0;
							break;
						case 11:
							for (i = 0; i < cfg.total_shells && i < MAX_OPTS; i++)
								snprintf(opt[i], MAX_OPLN, "%-*s %s", LEN_CODE, cfg.shell[i]->code, cfg.shell[i]->name);
							opt[i][0] = 0;
							i = cfg.new_shell;
							uifc.helpbuf =
								"`New User Command Shell:`\n"
								"\n"
								"You can use this option to select the default command shell for new\n"
								"users or 'Guest' (G-restricted) account logons.\n"
							;
							i = uifc.list(WIN_SAV | WIN_RHT, 2, 1, 13, &i, 0
							              , "Command Shells", opt);
							if (i == -1)
								break;
							cfg.new_shell = i;
							break;
						case 12:
							uifc.helpbuf =
								"`New User Default Download Protocol:`\n"
								"\n"
								"This option allows you to set the default download protocol of new users\n"
								"or 'Guest' (G-restricted) account logons.\n"
								"\n"
								"Set this to the desired protocol command key or `BLANK` for no default.\n"
							;
							sprintf(str, "%c", cfg.new_prot);
							uifc.input(WIN_SAV | WIN_MID, 0, 0
							           , "Default Download Protocol (SPACE=Disabled)"
							           , str, 1, K_EDIT | K_UPPER);
							cfg.new_prot = str[0];
							if (cfg.new_prot < ' ')
								cfg.new_prot = ' ';
							break;
						case 13:
							uifc.helpbuf =
								"`New User Days of New Messages:`\n"
								"\n"
								"This option allows you to set the number of days worth of messages\n"
								"which will be included in the new user's first `new message scan`.\n"
								"\n"
								"The value `0` means there will be `no` new messages for the new user.\n"
							;
							sprintf(str, "%hu", cfg.new_msgscan_init);
							uifc.input(WIN_SAV | WIN_MID, 0, 0
							           , "Days of New Messages for New User's first new message scan"
							           , str, 4, K_EDIT | K_NUMBER);
							cfg.new_msgscan_init = atoi(str);
							break;
						case 14:
							uifc.helpbuf =
								"`New User Gender Options:`\n"
								"\n"
								"This is a list of single-character gender options for new users to\n"
								"choose from.\n"
								"\n"
								"Default: `MFX`\n"
							;
							uifc.input(WIN_SAV | WIN_MID, 0, 0, "Gender Options"
							           , cfg.new_genders, sizeof(cfg.new_genders) - 1, K_EDIT | K_UPPER);
							break;
						case 15:
							uifc.helpbuf =
								"`New User Default Toggle Options:`\n"
								"\n"
								"This menu contains the default state of new user toggle options.  All\n"
								"new users on your system will have their defaults set according to the\n"
								"settings on this menu.  The user can then change them to their liking.\n"
								"\n"
								"See the Synchronet User Manual (`http://synchro.net/docs/user.html`)\n"
								"for more information on the individual options available.\n"
							;
							j = 0;
							while (1) {
								i = 0;
								snprintf(opt[i++], MAX_OPLN, "%-32.32s %-3.3s"
								         , "Expert Menu Mode"
								         , cfg.new_misc & EXPERT ? "Yes":"No");
								snprintf(opt[i++], MAX_OPLN, "%-32.32s %-3.3s"
								         , "Screen Pause"
								         , cfg.new_misc & UPAUSE ? "Yes":"No");
								snprintf(opt[i++], MAX_OPLN, "%-32.32s %-3.3s"
								         , "Spinning Cursor"
								         , cfg.new_misc & SPIN ? "Yes":"No");
								snprintf(opt[i++], MAX_OPLN, "%-32.32s %-3.3s"
								         , "Clear Screen Between Messages"
								         , cfg.new_misc & CLRSCRN ? "Yes":"No");
								snprintf(opt[i++], MAX_OPLN, "%-32.32s %-3.3s"
								         , "Ask For New Scan"
								         , cfg.new_misc & ASK_NSCAN ? "Yes":"No");
								snprintf(opt[i++], MAX_OPLN, "%-32.32s %-3.3s"
								         , "Ask For Your Msg Scan"
								         , cfg.new_misc & ASK_SSCAN ? "Yes":"No");
								snprintf(opt[i++], MAX_OPLN, "%-32.32s %-3.3s"
								         , "Automatic New File Scan"
								         , cfg.new_misc & ANFSCAN ? "Yes":"No");
								snprintf(opt[i++], MAX_OPLN, "%-32.32s %-3.3s"
								         , "Remember Current Sub-board"
								         , cfg.new_misc & CURSUB ? "Yes":"No");
								snprintf(opt[i++], MAX_OPLN, "%-32.32s %-3.3s"
								         , "Batch Download File Flagging"
								         , cfg.new_misc & BATCHFLAG ? "Yes":"No");
								snprintf(opt[i++], MAX_OPLN, "%-32.32s %-3.3s"
								         , "Extended File Descriptions"
								         , cfg.new_misc & EXTDESC ? "Yes":"No");
								snprintf(opt[i++], MAX_OPLN, "%-32.32s %-3.3s"
								         , "Mouse-enabled Terminal"
								         , cfg.new_misc & MOUSE ? "Yes":"No");
								snprintf(opt[i++], MAX_OPLN, "%-32.32s %-3.3s"
								         , "Auto Hang-up After Xfer"
								         , cfg.new_misc & AUTOHANG ? "Yes":"No");
								snprintf(opt[i++], MAX_OPLN, "%-32.32s %-3.3s"
								         , "Multinode Chat Echo"
								         , cfg.new_chat & CHAT_ECHO ? "Yes":"No");
								snprintf(opt[i++], MAX_OPLN, "%-32.32s %-3.3s"
								         , "Multinode Chat Actions"
								         , cfg.new_chat & CHAT_ACTION ? "Yes":"No");
								snprintf(opt[i++], MAX_OPLN, "%-32.32s %-3.3s"
								         , "Pageable for Chat"
								         , cfg.new_chat & CHAT_NOPAGE ? "No":"Yes");
								snprintf(opt[i++], MAX_OPLN, "%-32.32s %-3.3s"
								         , "Node Activity Messages"
								         , cfg.new_chat & CHAT_NOACT ? "No":"Yes");
								snprintf(opt[i++], MAX_OPLN, "%-32.32s %-3.3s"
								         , "Split-Screen Private Chat"
								         , cfg.new_chat & CHAT_SPLITP ? "Yes":"No");
								opt[i][0] = 0;
								j = uifc.list(WIN_BOT | WIN_SAV, 0, 0, 0, &newtog_cur, &newtog_bar
								              , "Default Toggle Options", opt);
								if (j == -1)
									break;
								switch (j) {
									case 0:
										cfg.new_misc ^= EXPERT;
										break;
									case 1:
										cfg.new_misc ^= UPAUSE;
										break;
									case 2:
										cfg.new_misc ^= SPIN;
										break;
									case 3:
										cfg.new_misc ^= CLRSCRN;
										break;
									case 4:
										cfg.new_misc ^= ASK_NSCAN;
										break;
									case 5:
										cfg.new_misc ^= ASK_SSCAN;
										break;
									case 6:
										cfg.new_misc ^= ANFSCAN;
										break;
									case 7:
										cfg.new_misc ^= CURSUB;
										break;
									case 8:
										cfg.new_misc ^= BATCHFLAG;
										break;
									case 9:
										cfg.new_misc ^= EXTDESC;
										break;
									case 10:
										cfg.new_misc ^= MOUSE;
										break;
									case 11:
										cfg.new_misc ^= AUTOHANG;
										break;
									case 12:
										cfg.new_chat ^= CHAT_ECHO;
										break;
									case 13:
										cfg.new_chat ^= CHAT_ACTION;
										break;
									case 14:
										cfg.new_chat ^= CHAT_NOPAGE;
										break;
									case 15:
										cfg.new_chat ^= CHAT_NOACT;
										break;
									case 16:
										cfg.new_chat ^= CHAT_SPLITP;
										break;
								}
							}
							break;
						case 16:
							newuser_qwk_opts();
							break;
					}
				}
				break;
			case 8:
				uifc.helpbuf =
					"`New User Questions/Prompts:`\n"
					"\n"
					"This menu allows you to decide which questions will be asked of a new\n"
					"user.\n"
				;
				j = 0;
				while (1) {
					i = 0;
					snprintf(opt[i++], MAX_OPLN, "%-27.27s %-3.3s"
					         , "Real Name"
					         , cfg.uq & UQ_REALNAME ? "Yes":"No");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s %-3.3s"
					         , "Force Multi-word Real Name"
					         , cfg.uq & UQ_NOSPACEREQ ? "No":"Yes");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s %-3.3s"
					         , "Force Unique Real Name"
					         , cfg.uq & UQ_DUPREAL ? "Yes":"No");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s %-3.3s"
					         , "Force Upper/Lower Case"
					         , cfg.uq & UQ_NOUPRLWR ? "No":"Yes");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s %-3.3s"
					         , "Company Name"
					         , cfg.uq & UQ_COMPANY ? "Yes":"No");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s %-3.3s"
					         , "Chat Handle / Call Sign"
					         , cfg.uq & UQ_HANDLE ? "Yes":"No");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s %-3.3s"
					         , "Force Unique Handle / Call Sign"
					         , cfg.uq & UQ_DUPHAND ? "Yes":"No");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s %-3.3s"
					         , "E-mail/NetMail Address"
					         , cfg.uq & UQ_NONETMAIL ? "No":"Yes");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s %-3.3s"
					         , "Force Unique E-mail/NetMail Address"
					         , cfg.uq & UQ_DUPNETMAIL ? "Yes":"No");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s %-3.3s"
					         , "Gender"
					         , cfg.uq & UQ_SEX ? "Yes":"No");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s %-3.3s"
					         , "Birth Date"
					         , cfg.uq & UQ_BIRTH ? "Yes":"No");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s %-3.3s"
					         , "Address and Zip Code"
					         , cfg.uq & UQ_ADDRESS ? "Yes":"No");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s %-3.3s"
					         , "Location"
					         , cfg.uq & UQ_LOCATION ? "Yes":"No");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s %-3.3s"
					         , "Require Comma in Location"
					         , cfg.uq & UQ_NOCOMMAS ? "No":"Yes");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s %-3.3s"
					         , "Phone Number"
					         , cfg.uq & UQ_PHONE ? "Yes":"No");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s %-3.3s"
					         , "Allow EX-ASCII in Answers"
					         , cfg.uq & UQ_NOEXASC ? "No":"Yes");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s %-3.3s"
					         , "External Editor"
					         , cfg.uq & UQ_XEDIT ? "Yes":"No");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s %-3.3s"
					         , "Command Shell"
					         , cfg.uq & UQ_CMDSHELL ? "Yes":"No");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s %-3.3s"
					         , "Default Settings"
					         , cfg.uq & UQ_NODEF ? "No":"Yes");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s %-3.3s"
					         , "Color Terminal"
					         , cfg.uq & UQ_COLORTERM ? "Yes":"No");
					opt[i][0] = 0;
					j = uifc.list(WIN_BOT | WIN_RHT | WIN_SAV, 0, 0, 0, &uq_cur, &uq_bar
					              , "New User Questions/Prompts", opt);
					if (j == -1)
						break;
					switch (j) {
						case 0:
							cfg.uq ^= UQ_REALNAME;
							break;
						case 1:
							cfg.uq ^= UQ_NOSPACEREQ;
							break;
						case 2:
							cfg.uq ^= UQ_DUPREAL;
							break;
						case 3:
							cfg.uq ^= UQ_NOUPRLWR;
							break;
						case 4:
							cfg.uq ^= UQ_COMPANY;
							break;
						case 5:
							cfg.uq ^= UQ_HANDLE;
							break;
						case 6:
							cfg.uq ^= UQ_DUPHAND;
							break;
						case 7:
							cfg.uq ^= UQ_NONETMAIL;
							break;
						case 8:
							cfg.uq ^= UQ_DUPNETMAIL;
							break;
						case 9:
							cfg.uq ^= UQ_SEX;
							break;
						case 10:
							cfg.uq ^= UQ_BIRTH;
							break;
						case 11:
							cfg.uq ^= UQ_ADDRESS;
							break;
						case 12:
							cfg.uq ^= UQ_LOCATION;
							break;
						case 13:
							cfg.uq ^= UQ_NOCOMMAS;
							break;
						case 14:
							cfg.uq ^= UQ_PHONE;
							break;
						case 15:
							cfg.uq ^= UQ_NOEXASC;
							break;
						case 16:
							cfg.uq ^= UQ_XEDIT;
							break;
						case 17:
							cfg.uq ^= UQ_CMDSHELL;
							break;
						case 18:
							cfg.uq ^= UQ_NODEF;
							break;
						case 19:
							cfg.uq ^= UQ_COLORTERM;
							break;
					}
				}
				break;
			case 9:
				security_cfg();
				break;
			case 10:    /* Advanced Options */
				done = 0;
				while (!done) {
					i = 0;
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "New User Magic Word", cfg.new_magic);
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Data Directory"
					         , cfg.data_dir);
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Logs Directory"
					         , cfg.logs_dir);
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Exec Directory"
					         , cfg.exec_dir);
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Mods Directory"
					         , cfg.mods_dir);
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Input SIF Questionnaire"
					         , cfg.new_sif);
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Output SIF Questionnaire"
					         , cfg.new_sof);
					u32toac(cfg.cdt_per_dollar, str, ',');
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Credits Per Dollar", str);
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%u", "Minutes Per 100K Credits"
					         , cfg.cdt_min_value);
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Maximum Number of Minutes"
					         , cfg.max_minutes ? ultoa(cfg.max_minutes, tmp, 10) : "Unlimited");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%u", "Warning Days Till Expire"
					         , cfg.sys_exp_warn);
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%u", "Last Displayable Node"
					         , cfg.sys_lastnode);
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Phone Number Format"
					         , cfg.sys_phonefmt);
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Sysop Chat Override"
					         , cfg.sys_chat_arstr);
					if (cfg.user_backup_level)
						sprintf(str, "%u", cfg.user_backup_level);
					else
						strcpy(str, "None");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "User Database Backups", str);
					if (cfg.mail_backup_level)
						sprintf(str, "%u", cfg.mail_backup_level);
					else
						strcpy(str, "None");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Mail Database Backups", str);
					if (cfg.config_backup_level)
						sprintf(str, "%u", cfg.config_backup_level);
					else
						strcpy(str, "None");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Configuration Backups", str);
					if (cfg.max_log_size && cfg.max_logs_kept) {
						SAFEPRINTF2(str, "%s bytes, keep %u"
						            , byte_count_to_str(cfg.max_log_size, tmp, sizeof(tmp))
						            , cfg.max_logs_kept);
					} else {
						SAFECOPY(str, "Unlimited");
					}
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Maximum Log File Size", str);
					duration_to_vstr(cfg.max_getkey_inactivity, str, sizeof(str));
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Maximum User Inactivity", str);
					if (cfg.inactivity_warn)
						SAFEPRINTF(str, "%u percent", cfg.inactivity_warn);
					else
						SAFECOPY(str, "Disabled");
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "User Inactivity Warning", str);
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%" PRIX32, "Control Key Pass-through"
					         , cfg.ctrlkey_passthru);
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Statistics Interval"
						, duration_to_vstr(cfg.stats_interval, str, sizeof str));
					snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Cache Filter Files"
						, cfg.cache_filter_files > 0 ? duration_to_vstr(cfg.cache_filter_files, str, sizeof str) : strDisabled);
					opt[i][0] = 0;
					uifc.helpbuf =
						"`System Advanced Options:`\n"
						"\n"
						"Care should be taken when modifying any of the options listed here.\n"
					;
					switch (uifc.list(WIN_ACT | WIN_BOT | WIN_RHT | WIN_SAV, 0, 0, 0, &adv_dflt, &adv_bar
					                  , "Advanced Options", opt)) {
						case -1:
							done = 1;
							break;

						case 0:
							uifc.helpbuf =
								"`New User Magic Word:`\n"
								"\n"
								"If this field has a value, it is assumed the sysop has placed some\n"
								"reference to this `magic word` in `text/newuser.msg` and new users\n"
								"will be prompted for the magic word after they enter their password.\n"
								"If they do not enter it correctly, it is assumed they didn't read the\n"
								"new user information displayed to them and they are disconnected.\n"
								"\n"
								"Think of it as a password to guarantee that new users read the text\n"
								"displayed to them.\n"
							;
							uifc.input(WIN_MID | WIN_SAV, 0, 0, "New User Magic Word", cfg.new_magic, sizeof(cfg.new_magic) - 1
							           , K_EDIT | K_UPPER);
							break;
						case 1:
							uifc.helpbuf =
								"`Data File Directory:`\n"
								"\n"
								"The Synchronet data directory contains almost all the data for your BBS.\n"
								"This directory must be located where `ALL` nodes can access it and\n"
								"`MUST NOT` be placed on a RAM disk or other volatile media.\n"
								"\n"
								"See `http://wiki.synchro.net/dir:data` for details.\n"
								"\n"
								"This option allows you to change the location of your data directory.\n"
							;
							strcpy(str, cfg.data_dir);
							if (uifc.input(WIN_MID | WIN_SAV, 0, 9, "Data Directory"
							               , str, sizeof(cfg.data_dir) - 1, K_EDIT) > 0) {
								backslash(str);
								SAFECOPY(cfg.data_dir, str);
							}
							break;
						case 2:
							uifc.helpbuf =
								"`Log File Directory:`\n"
								"\n"
								"Log files will be stored in this directory.\n"
								"\n"
								"By default, this is set to the same as your Data File directory.\n"
							;
							strcpy(str, cfg.logs_dir);
							if (uifc.input(WIN_MID | WIN_SAV, 0, 9, "Logs Directory"
							               , str, sizeof(cfg.logs_dir) - 1, K_EDIT) > 0) {
								backslash(str);
								SAFECOPY(cfg.logs_dir, str);
							}
							break;
						case 3:
							uifc.helpbuf =
								"`Executable/Module File Directory:`\n"
								"\n"
								"The Synchronet exec directory contains program and script files that the\n"
								"BBS executes. This directory does `not` need to be in your OS search path.\n"
								"\n"
								"If you place programs in this directory for the BBS to execute, you\n"
								"should place the `%!` specifier for the `exec` directory at the\n"
								"beginning of the configured command-lines.\n"
								"\n"
								"See `http://wiki.synchro.net/dir:exec` for details.\n"
								"\n"
								"This option allows you to change the location of your exec directory.\n"
							;
							strcpy(str, cfg.exec_dir);
							if (uifc.input(WIN_MID | WIN_SAV, 0, 9, "Exec Directory"
							               , str, sizeof(cfg.exec_dir) - 1, K_EDIT) > 0) {
								backslash(str);
								SAFECOPY(cfg.exec_dir, str);
							}
							break;
						case 4:
							uifc.helpbuf =
								"`Modified Modules Directory:`\n"
								"\n"
								"This optional directory can be used to specify a location where modified\n"
								"module files are stored. These modified modules will take precedence\n"
								"over modules with the same filename (in the `exec` directory) and will\n"
								"`not be overwritten` by future updates/upgrades.\n"
								"\n"
								"Sub-directory searches of this directory also take precedence\n"
								"(e.g. `mods/load/*` overrides `exec/load/*`).\n"
								"\n"
								"If this directory is `blank`, then this feature is not used and all\n"
								"modules are assumed to be located in the `exec` directory.\n"
								"\n"
								"See `http://wiki.synchro.net/dir:mods` for details.\n"
							;
							strcpy(str, cfg.mods_dir);
							if (uifc.input(WIN_MID | WIN_SAV, 0, 9, "Mods Directory"
							               , str, sizeof(cfg.mods_dir) - 1, K_EDIT) > 0) {
								backslash(str);
								SAFECOPY(cfg.mods_dir, str);
							}
							break;
						case 5:
							strcpy(str, cfg.new_sif);
							uifc.helpbuf =
								"`SIF Questionnaire for User Input:`\n"
								"\n"
								"This is the name of a SIF questionnaire file that resides your text\n"
								"directory that all users will be prompted to answer.\n"
							;
							uifc.input(WIN_MID | WIN_SAV, 0, 0
							           , "SIF Questionnaire for User Input"
							           , str, LEN_CODE, K_UPPER | K_EDIT | K_NOSPACE);
							if (!str[0] || code_ok(str))
								strcpy(cfg.new_sif, str);
							else
								uifc.msg("Invalid SIF Name");
							break;
						case 6:
							strcpy(str, cfg.new_sof);
							uifc.helpbuf =
								"`SIF Questionnaire for Reviewing User Input:`\n"
								"\n"
								"This is the SIF file used to review the input of users from the user\n"
								"edit function.\n"
							;
							uifc.input(WIN_MID | WIN_SAV, 0, 0
							           , "SIF Questionnaire for Reviewing User Input"
							           , str, LEN_CODE, K_UPPER | K_EDIT | K_NOSPACE);
							if (!str[0] || code_ok(str))
								strcpy(cfg.new_sof, str);
							else
								uifc.msg("Invalid SIF Name");
							break;
						case 7:
							uifc.helpbuf =
								"`Credits Per Dollar:`\n"
								"\n"
								"This is the monetary value of a credit (How many credits per dollar).\n"
								"This value should be a power of 2 (1, 2, 4, 8, 16, 32, 64, 128, etc.)\n"
								"since credits are usually converted in 100 kibibyte (102400) blocks.\n"
								"To make a dollar worth two mebibytes of credits, set this value to\n"
								"2,097,152 (a mebibyte is 1024*1024 or 1048576).\n"
							;
							ultoa(cfg.cdt_per_dollar, str, 10);
							uifc.input(WIN_MID | WIN_SAV, 0, 0
							           , "Credits Per Dollar", str, 10, K_NUMBER | K_EDIT);
							cfg.cdt_per_dollar = atol(str);
							break;
						case 8:
							uifc.helpbuf =
								"`Minutes Per 100K Credits:`\n"
								"\n"
								"This is the value of a minute of time online. This field is the number\n"
								"of minutes to give the user in exchange for each 100K credit block.\n"
							;
							sprintf(str, "%u", cfg.cdt_min_value);
							uifc.input(WIN_MID | WIN_SAV, 0, 0
							           , "Minutes Per 100K Credits", str, 5, K_NUMBER | K_EDIT);
							cfg.cdt_min_value = atoi(str);
							break;
						case 9:
							uifc.helpbuf =
								"`Maximum Number of Minutes User Can Have:`\n"
								"\n"
								"This value is the maximum total number of minutes a user can have. If a\n"
								"user has this number of minutes or more, they will not be allowed to\n"
								"convert credits into minutes. A sysop can add minutes to a user's\n"
								"account regardless of this maximum. If this value is set to `0`, users\n"
								"will have no limit on the total number of minutes they can have.\n"
							;
							sprintf(str, "%" PRIu32, cfg.max_minutes);
							uifc.input(WIN_MID | WIN_SAV, 0, 0
							           , "Maximum Number of Minutes a User Can Have "
							           "(0=No Limit)"
							           , str, 10, K_NUMBER | K_EDIT);
							cfg.max_minutes = atol(str);
							break;
						case 10:
							uifc.helpbuf =
								"`Warning Days Till Expire:`\n"
								"\n"
								"If a user's account will expire in this many days or less, the user will\n"
								"be notified at logon. Setting this value to `0` disables the warning\n"
								"completely.\n"
							;
							sprintf(str, "%u", cfg.sys_exp_warn);
							uifc.input(WIN_MID | WIN_SAV, 0, 0
							           , "Warning Days Till Expire", str, 5, K_NUMBER | K_EDIT);
							cfg.sys_exp_warn = atoi(str);
							break;
						case 11:
							uifc.helpbuf =
								"`Last Displayed Node:`\n"
								"\n"
								"This is the number of the last node to display to users in node lists.\n"
								"This allows the sysop to define the higher numbered nodes as `invisible`\n"
								"to users.\n"
							;
							sprintf(str, "%u", cfg.sys_lastnode);
							uifc.input(WIN_MID | WIN_SAV, 0, 0
							           , "Last Displayed Node", str, 5, K_NUMBER | K_EDIT);
							cfg.sys_lastnode = atoi(str);
							break;
						case 12:
							uifc.helpbuf =
								"`Phone Number Format:`\n"
								"\n"
								"This is the format used for phone numbers in your local calling\n"
								"area. Use `N` for number positions, `A` for alphabetic, or `!` for any\n"
								"character. All other characters will be static in the phone number\n"
								"format. An example for North American phone numbers is `NNN-NNN-NNNN`.\n"
							;
							uifc.input(WIN_MID | WIN_SAV, 0, 0
							           , "Phone Number Format", cfg.sys_phonefmt
							           , LEN_PHONE, K_UPPER | K_EDIT);
							break;
						case 13:
							getar("Sysop Chat Override", cfg.sys_chat_arstr);
							break;
						case 14:
							uifc.helpbuf =
								"`User Database Backups:`\n"
								"\n"
								"Setting this option to anything but 0 will enable automatic daily\n"
								"backups of the user database.  This number determines how many backups\n"
								"to keep on disk.\n"
							;
							sprintf(str, "%u", cfg.user_backup_level);
							uifc.input(WIN_MID | WIN_SAV, 0, 0
							           , "Number of Daily User Database Backups to Keep"
							           , str, 4, K_NUMBER | K_EDIT);
							cfg.user_backup_level = atoi(str);
							break;
						case 15:
							uifc.helpbuf =
								"`Mail Database Backups:`\n"
								"\n"
								"Setting this option to anything but 0 will enable automatic daily\n"
								"backups of the mail database.  This number determines how many backups\n"
								"to keep on disk.\n"
							;
							sprintf(str, "%u", cfg.mail_backup_level);
							uifc.input(WIN_MID | WIN_SAV, 0, 0
							           , "Number of Daily Mail Database Backups to Keep"
							           , str, 4, K_NUMBER | K_EDIT);
							cfg.mail_backup_level = atoi(str);
							break;
						case 16:
							uifc.helpbuf =
								"`Configuration Backups:`\n"
								"\n"
								"Setting this option to anything but 0 will enable automatic backups of\n"
								"your configuration files when saving changes.\n"
							;
							sprintf(str, "%u", cfg.config_backup_level);
							uifc.input(WIN_MID | WIN_SAV, 0, 0
							           , "Number of Configuration File Backups to Keep"
							           , str, 4, K_NUMBER | K_EDIT);
							cfg.config_backup_level = atoi(str);
							break;
						case 17:
							uifc.helpbuf =
								"`Maximum Log File Size:`\n"
								"\n"
								"This option allows you to limit the size of the following log files\n"
								"created and appended-to by Synchronet, in the `Logs Directory`:\n"
								"\n"
								"  `hungup.log`\n"
								"  `error.log`\n"
								"  `crash.log`\n"
								"  `hack.log`\n"
								"  `spam.log`\n"
								"  `guru.log`\n"
								"\n"
								"The largest supported log file size limit is 4294967295 (3.99G) bytes.\n"
								"Log files that have reached or exceeded the configured size limit are\n"
								"retained by renaming the `*.log` file to `*.#.log` beginning with `*.0.log`.\n"
								"\n"
								"You must also specify the number of older/max-size log files to retain.\n"
								"The largest number of supported retained rotated log files is 9999.\n"
								"Oldest rotated log files are automatically deleted to save disk space.\n"
							;
							byte_count_to_str(cfg.max_log_size, str, sizeof(str));
							if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Log File Size (in bytes, 0=Unlimited)", str, 10, K_EDIT | K_UPPER) > 0) {
								cfg.max_log_size = (uint32_t)parse_byte_count(str, 1);
								if (cfg.max_log_size) {
									SAFEPRINTF(str, "%u", cfg.max_logs_kept);
									if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Number of old Log Files to Keep", str, 4, K_EDIT | K_NUMBER) > 0)
										cfg.max_logs_kept = atoi(str);
								}
							}
							break;
						case 18:
							duration_to_str(cfg.max_getkey_inactivity, str, sizeof(str));
							uifc.helpbuf =
								"`Duration Before Inactive-User Disconnection:`\n"
								"\n"
								"This is the duration of time the user must be inactive while the BBS\n"
								"is waiting for keyboard input (via `getkey()`) before the user will be\n"
								"automatically disconnected.  Default is `5 minutes`.\n"
								"\n"
								"For lower-level inactive socket detection/disconnection, see the\n"
								"`Max*Inactivity` settings in `Servers->Terminal Server`. \n"
								"\n"
								"`H`-exempt users will not be disconnected due to inactivity."
							;
							uifc.input(WIN_MID | WIN_SAV, 0, 14
							           , "Duration Before Inactive-User Disconnection"
							           , str, 10, K_EDIT);
							cfg.max_getkey_inactivity = (uint)parse_duration(str);
							break;
						case 19:
							ultoa(cfg.inactivity_warn, str, 10);
							uifc.helpbuf =
								"`Percentage of Maximum Inactivity Before Warning:`\n"
								"\n"
								"This is the percentage of the maximum inactivity duration that must\n"
								"elapse before a warning of detected user inactivity will be given.\n"
								"A setting of `0` will disable all inactivity warnings (including socket\n"
								"inactivity warnings).  Default is `75` percent.\n"
							;
							uifc.input(WIN_MID | WIN_SAV, 0, 14
							           , "Percentage of Maximum Inactivity Before Warning"
							           , str, 4, K_NUMBER | K_EDIT);
							cfg.inactivity_warn = atoi(str);
							break;
						case 20:
							uifc.helpbuf =
								"`Control Key Pass-through:`\n"
								"\n"
								"This value is a 32-bit hexadecimal number. Each set bit represents a\n"
								"control key combination that will `not` be handled internally by\n"
								"Synchronet or by a Global Hot Key Event.\n"
								"\n"
								"To disable internal handling of the `Ctrl-C` key combination (for example)\n"
								"set this value to `8`. The value is determined by taking 2 to the power of\n"
								"the ASCII value of the control character (Ctrl-A is 1, Ctrl-B is 2, \n"
								"etc.). In the case of Ctrl-C, taking 2 to the power of 3 equals 8.\n"
								"\n"
								"To pass-through multiple control key combinations, multiple bits must be\n"
								"set (or'd together) to create the necessary value, which may require the\n"
								"use of a hexadecimal calculator.\n"
								"\n"
								"If unsure, leave this value set to `0`, the default.\n"
							;
							sprintf(str, "%" PRIX32, cfg.ctrlkey_passthru);
							uifc.input(WIN_MID | WIN_SAV, 0, 0
							           , "Control Key Pass-through"
							           , str, 8, K_UPPER | K_EDIT);
							cfg.ctrlkey_passthru = strtoul(str, NULL, 16);
							break;
						case 21:
							uifc.helpbuf =
								"`Statistics Interval:`\n"
								"\n"
								"This value is the interval, in seconds, between reads of statistics\n"
								"files for the purposes of displaying system and node statistics to\n"
								"users and caching the results for performance.\n"
								"\n"
								"A lower value means more current statistics, but more disk I/O.\n"
								"A higher value means less current statistics, but less disk I/O.\n"
								"\n"
								"If unsure, leave this value set to `5s`, the default.\n"
							;
							duration_to_str(cfg.stats_interval, str, sizeof str);
							uifc.input(WIN_MID | WIN_SAV, 0, 0
							           , "Statistics Interval (cache duration)"
							           , str, 10, K_UPPER | K_EDIT);
							cfg.stats_interval = (uint)parse_duration(str);
							break;
						case 22:
							uifc.helpbuf =
								"`Cache Filter Files:`\n"
								"\n"
								"Some filter files (e.g. `text/*.can`) are frequently checked (e.g. upon\n"
								"every incoming TCP connection) and these files can become quite large.\n"
								"Caching these file contents in memory may dramatically speed up these\n"
								"checks and reduce redundant disk I/O.\n"
								"\n"
								"This value determines the interval, in seconds, between checks for\n"
								"changes to filter files.  If a change is detected, the file's cache will\n"
								"be refreshed.  Setting this value to a lower value means changes to\n"
								"filter files will be detected and applied more quickly, but with more\n"
								"disk I/O.  Setting this value to a higher value means changes to filter\n"
								"files will be detected and applied less quickly, but with less disk I/O.\n"
								"\n"
								"Setting this value to `0` disables caching of filter files entirely,\n"
								"using less memory, but increasing disk I/O operations.\n"
								"\n"
								"If unsure, leave this value set to `5s`, the default.\n"
							;
							duration_to_str(cfg.cache_filter_files, str, sizeof str);
							uifc.input(WIN_MID | WIN_SAV, 0, 0
							           , "Filter File Cache Duration"
							           , str, 10, K_UPPER | K_EDIT);
							cfg.cache_filter_files = (uint)parse_duration(str);
							break;
					}
				}
				break;
			case 11: /* Loadable Modules */
				done = 0;
				while (!done) {
					i = 0;
					const char* list_sep = ", ";
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Login", strListCombine(cfg.login_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Logon", strListCombine(cfg.logon_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Sync", strListCombine(cfg.sync_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Logoff", strListCombine(cfg.logoff_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Logout", strListCombine(cfg.logout_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "New User Prompts", strListCombine(cfg.newuser_prompts_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "New User Info", strListCombine(cfg.newuser_info_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "New User Created", strListCombine(cfg.newuser_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "User Config", strListCombine(cfg.usercfg_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Expired User", strListCombine(cfg.expire_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Auto Message", strListCombine(cfg.automsg_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Send Feedback", strListCombine(cfg.feedback_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Chat Section", strListCombine(cfg.chatsec_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Text Section", strListCombine(cfg.textsec_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Xtrn Section", strListCombine(cfg.xtrnsec_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Pre Xtrn", strListCombine(cfg.prextrn_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Post Xtrn", strListCombine(cfg.postxtrn_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Read Mail", strListCombine(cfg.readmail_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Scan Msgs", strListCombine(cfg.scanposts_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Scan Subs", strListCombine(cfg.scansubs_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "List Msgs", strListCombine(cfg.listmsgs_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "List Logons", strListCombine(cfg.logonlist_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "List Users", strListCombine(cfg.userlist_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "List Nodes", strListCombine(cfg.nodelist_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Who's Online", strListCombine(cfg.whosonline_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Private Msg", strListCombine(cfg.privatemsg_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Scan Dirs", strListCombine(cfg.scandirs_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "List Files", strListCombine(cfg.listfiles_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "View File Info", strListCombine(cfg.fileinfo_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Batch Transfer", strListCombine(cfg.batxfer_mod.cmd, str, sizeof str, list_sep));
					snprintf(opt[i++], MAX_OPLN, "%-18.18s%s", "Temp Transfer", strListCombine(cfg.tempxfer_mod.cmd, str, sizeof str, list_sep));
					opt[i][0] = 0;
					uifc.helpbuf =
						"`Loadable Modules:`\n"
						"\n"
						"Baja modules (`.bin` files) or JavaScript modules (`.js` files) can be\n"
						"automatically loaded and executed during certain Terminal Server\n"
						"operations.  Command-line arguments may be included for all.\n"
						"\n"
						"`Login`            Required module for interactive terminal logins (auth)\n"
						"`Logon`            Terminal logon procedure (post login/authentication)\n"
						"`Sync`             Node is periodically synchronized (comm/disk I/O flush)\n"
						"`Logoff`           Terminal logoff procedure, potentially user-interactive\n"
						"`Logout`           Terminal logout procedure, off-line, post-logoff\n"
						"`New User Prompts` New user registration prompts\n"
						"`New user Info`    New user information/help\n"
						"`New User Created` End of new user registration/creation process\n"
						"`User Config`      User (e.g. terminal) settings configuration menu\n"
						"`Expired User`     User account expires (off-line)\n"
						"`Auto Message`     User chooses to re-read or edit the auto-message\n"
						"`Send Feedback`    User sending email to a sysop (return error to cancel)\n"
						"`Chat Section`     User enters chat section/menu\n"
						"`Text Section`     General text file (add/remove/viewing) section\n"
						"`Xtrn Section`     External programs (doors) section\n"
						"`Pre Xtrn`         Executed before external programs (doors) run\n"
						"`Post Xtrn`        Executed after external programs (doors) run\n"
						"`Read Mail`        User reads email/netmail\n"
						"`Scan Msgs`        User reads or scans a message sub-board\n"
						"`Scan Subs`        User scans one or more sub-boards for msgs\n"
						"`List Msgs`        User lists msgs from the msg read prompt\n"
						"`List Logons`      User lists logons ('-y' for yesterday)\n"
						"`List Users`       User lists the users of the system\n"
						"`List Nodes`       User lists all nodes\n"
						"`Who's Online`     User lists the nodes in-use (e.g. `^U` key-press)\n"
						"`Private Msg`      User sends a private node msg (e.g. `^P` key-press)\n"
						"`Scan Dirs`        User scans one or more directories for files\n"
						"`List Files`       User lists files within a file directory\n"
						"`View File Info`   User views detailed information on files in a directory\n"
						"`Batch Transfer`   Batch file transfer menu\n"
						"`Temp Transfer`    Temporary/archive file transfer menu\n"
						"\n"
						"`Note:` JavaScript modules take precedence over Baja modules if both exist\n"
						"      in your `exec` or `mods` directories.\n"
					;
					switch (uifc.list(WIN_ACT | WIN_BOT | WIN_RHT | WIN_SAV, 0, 0, 0, &mod_dflt, &mod_bar
					                  , "Loadable Modules", opt)) {

						case -1:
							done = 1;
							break;

						case 0:
							mods_changed |= cfg_loadable_modules("Login", &cfg.login_mod, mod_bar, 1);
							break;
						case 1:
							mods_changed |= cfg_loadable_modules("Logon", &cfg.logon_mod, mod_bar, 0);
							break;
						case 2:
							mods_changed |= cfg_loadable_modules("Synchronize", &cfg.sync_mod, mod_bar, 0);
							break;
						case 3:
							mods_changed |= cfg_loadable_modules("Logoff", &cfg.logoff_mod, mod_bar, 0);
							break;
						case 4:
							mods_changed |= cfg_loadable_modules("Logout", &cfg.logout_mod, mod_bar, 0);
							break;
						case 5:
							mods_changed |= cfg_loadable_modules("New User Prompts", &cfg.newuser_prompts_mod, mod_bar, 1);
							break;
						case 6:
							mods_changed |= cfg_loadable_modules("New User Information", &cfg.newuser_info_mod, mod_bar, 0);
							break;
						case 7:
							mods_changed |= cfg_loadable_modules("New User Created", &cfg.newuser_mod, mod_bar, 0);
							break;
						case 8:
							mods_changed |= cfg_loadable_modules("User Configuration", &cfg.usercfg_mod, mod_bar, 1);
							break;
						case 9:
							mods_changed |= cfg_loadable_modules("Expired User", &cfg.expire_mod, mod_bar, 0);
							break;
						case 10:
							mods_changed |= cfg_loadable_modules("Auto Message", &cfg.automsg_mod, mod_bar, 0);
							break;
						case 11:
							mods_changed |= cfg_loadable_modules("Send Feedback", &cfg.feedback_mod, mod_bar, 0);
							break;
						case 12:
							mods_changed |= cfg_loadable_modules("Chat Section", &cfg.chatsec_mod, mod_bar, 1);
							break;
						case 13:
							mods_changed |= cfg_loadable_modules("Text File Section", &cfg.textsec_mod, mod_bar, 1);
							break;
						case 14:
							mods_changed |= cfg_loadable_modules("External Program Section", &cfg.xtrnsec_mod, mod_bar, 1);
							break;
						case 15:
							mods_changed |= cfg_loadable_modules("Pre External Program", &cfg.prextrn_mod, mod_bar, 0);
							break;
						case 16:
							mods_changed |= cfg_loadable_modules("Post External Program", &cfg.postxtrn_mod, mod_bar, 0);
							break;
						case 17:
							mods_changed |= cfg_loadable_modules("Read Mail", &cfg.readmail_mod, mod_bar, 0);
							break;
						case 18:
							mods_changed |= cfg_loadable_modules("Scan Msgs", &cfg.scanposts_mod, mod_bar, 0);
							break;
						case 19:
							mods_changed |= cfg_loadable_modules("Scan Subs", &cfg.scansubs_mod, mod_bar, 0);
							break;
						case 20:
							mods_changed |= cfg_loadable_modules("List Msgs", &cfg.listmsgs_mod, mod_bar, 0);
							break;
						case 21:
							mods_changed |= cfg_loadable_modules("List Logons", &cfg.logonlist_mod, mod_bar, 0);
							break;
						case 22:
							mods_changed |= cfg_loadable_modules("List Users", &cfg.userlist_mod, mod_bar, 0);
							break;
						case 23:
							mods_changed |= cfg_loadable_modules("List Nodes", &cfg.nodelist_mod, mod_bar, 0);
							break;
						case 24:
							mods_changed |= cfg_loadable_modules("Who's Online", &cfg.whosonline_mod, mod_bar, 0);
							break;
						case 25:
							mods_changed |= cfg_loadable_modules("Private Message", &cfg.privatemsg_mod, mod_bar, 0);
							break;
						case 26:
							mods_changed |= cfg_loadable_modules("Scan Dirs", &cfg.scandirs_mod, mod_bar, 0);
							break;
						case 27:
							mods_changed |= cfg_loadable_modules("List Files", &cfg.listfiles_mod, mod_bar, 0);
							break;
						case 28:
							mods_changed |= cfg_loadable_modules("View File Information", &cfg.fileinfo_mod, mod_bar, 0);
							break;
						case 29:
							mods_changed |= cfg_loadable_modules("Batch File Transfer", &cfg.batxfer_mod, mod_bar, 0);
							break;
						case 30:
							mods_changed |= cfg_loadable_modules("Temporary File Transfer", &cfg.tempxfer_mod, mod_bar, 0);
							break;
					}
				}
				break;
			case 12:
				toggle_xattr_support("Extra Attribute Codes in Display Files", &cfg.sys_misc, /* shift: */0);
				break;
		}
	}
}
