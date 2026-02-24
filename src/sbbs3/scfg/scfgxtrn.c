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
#include "ciolib.h" // CIO_KEY_*

char *daystr(char days);
static void hotkey_cfg(void);

char* use_shell_opt = "Use Shell or New Context";
char* use_shell_help =
	"`Use System Shell or New JavaScript Context to Execute:`\n"
	"\n"
	"If this command line requires the system command shell to execute\n"
	"(e.g. uses pipes/redirection or invokes a Unix shell script or\n"
	"DOS/Windows batch/command file), then set this option to ~Yes~.\n"
	"\n"
	"If this command line is invoking a Synchronet JavaScript module\n"
	"(e.g. it begins with a '`?`' character), then setting this option to ~Yes~\n"
	"will enable the creation and initialization of a new JavaScript run-time\n"
	"context for it to execute within, for every invocation."
;
char* use_shell_prompt = "Use System Shell or New JavaScript Context to Execute";
char*        native_help =
	"`Native Executable:`\n"
	"\n"
	"If this program is `16-bit MS-DOS` executable, set this option to `No`,\n"
	"otherwise (it is a native program `or script`) set this option to `Yes`.\n"
;
char* native_opt = "Native Executable";

#define CUT_XTRNSEC_NUM USHRT_MAX

static bool new_timed_event(unsigned new_event_num)
{
	event_t* new_event;
	if ((new_event = (event_t *)malloc(sizeof(*new_event))) == NULL) {
		errormsg(WHERE, ERR_ALLOC, "timed event", sizeof(*new_event));
		return false;
	}
	memset(new_event, 0, sizeof(*new_event));
	new_event->node = NODE_ANY;
	new_event->days = (uchar)0xff;
	new_event->errlevel = LOG_ERR;

	event_t** new_event_list = realloc(cfg.event, sizeof(event_t *) * (cfg.total_events + 1));
	if (new_event_list == NULL) {
		free(new_event);
		errormsg(WHERE, ERR_ALLOC, "timed event list", cfg.total_events + 1);
		return false;
	}
	cfg.event = new_event_list;

	for (unsigned u = cfg.total_events; u > new_event_num; u--)
		cfg.event[u] = cfg.event[u - 1];

	cfg.event[new_event_num] = new_event;
	cfg.total_events++;
	return true;
}

static bool new_external_program_section(int new_section_num)
{
	xtrnsec_t* new_xtrnsec = malloc(sizeof(*new_xtrnsec));
	if (new_xtrnsec == NULL) {
		errormsg(WHERE, ERR_ALLOC, "external program section", sizeof(*new_xtrnsec));
		return false;
	}
	memset(new_xtrnsec, 0, sizeof(*new_xtrnsec));

	xtrnsec_t** new_xtrnsec_list = realloc(cfg.xtrnsec, sizeof(xtrnsec_t *) * (cfg.total_xtrnsecs + 1));
	if (new_xtrnsec_list == NULL) {
		free(new_xtrnsec);
		errormsg(WHERE, ERR_ALLOC, "external program section list", cfg.total_xtrnsecs + 1);
		return false;
	}
	cfg.xtrnsec = new_xtrnsec_list;
	for (int u = cfg.total_xtrnsecs; u > new_section_num; u--)
		cfg.xtrnsec[u] = cfg.xtrnsec[u - 1];
	for (int j = 0; j < cfg.total_xtrns; j++) {
		if (cfg.xtrn[j]->sec >= new_section_num && cfg.xtrn[j]->sec != CUT_XTRNSEC_NUM)
			cfg.xtrn[j]->sec++;
	}

	cfg.xtrnsec[new_section_num] = new_xtrnsec;
	cfg.total_xtrnsecs++;
	return true;
}

static bool new_external_program(unsigned new_xtrn_num, unsigned section)
{
	xtrn_t* new_xtrn = malloc(sizeof(*new_xtrn));
	if (new_xtrn == NULL) {
		errormsg(WHERE, ERR_ALLOC, "external program", sizeof(*new_xtrn));
		return false;
	}
	memset(new_xtrn, 0, sizeof(*new_xtrn));
	new_xtrn->sec = section;
	new_xtrn->misc = MULTIUSER;

	xtrn_t ** new_xtrn_list = realloc(cfg.xtrn, sizeof(xtrn_t *) * (cfg.total_xtrns + 1));
	if (new_xtrn_list == NULL) {
		free(new_xtrn);
		errormsg(WHERE, ERR_ALLOC, "external program list", cfg.total_xtrns + 1);
		return false;
	}
	cfg.xtrn = new_xtrn_list;
	for (unsigned n = cfg.total_xtrns; n > new_xtrn_num; n--)
		cfg.xtrn[n] = cfg.xtrn[n - 1];

	cfg.xtrn[new_xtrn_num] = new_xtrn;
	cfg.total_xtrns++;
	return true;
}

static int next_program(const int num)
{
	for (int i = num + 1; i < cfg.total_xtrns; i++) {
		if (cfg.xtrn[i]->sec == cfg.xtrn[num]->sec)
			return i;
	}
	return num;
}

static int prev_program(const int num)
{
	for (int i = num - 1; i >= 0; i--) {
		if (cfg.xtrn[i]->sec == cfg.xtrn[num]->sec)
			return i;
	}
	return num;
}

static bool new_external_editor(unsigned new_xedit_num)
{
	xedit_t* new_xedit = malloc(sizeof(*new_xedit));
	if (new_xedit == NULL) {
		errormsg(WHERE, ERR_ALLOC, "external editor", sizeof(*new_xedit));
		return false;
	}
	memset(new_xedit, 0, sizeof(*new_xedit));
	new_xedit->misc |= QUOTEWRAP;

	xedit_t** new_xedit_list = realloc(cfg.xedit, sizeof(xedit_t *) * (cfg.total_xedits + 1));
	if (new_xedit_list == NULL) {
		free(new_xedit);
		errormsg(WHERE, ERR_ALLOC, "external editor list", cfg.total_xedits + 1);
		return false;
	}
	cfg.xedit = new_xedit_list;
	for (unsigned u = cfg.total_xedits; u > new_xedit_num; u--)
		cfg.xedit[u] = cfg.xedit[u - 1];

	cfg.xedit[new_xedit_num] = new_xedit;
	cfg.total_xedits++;
	return true;
}

static char* monthstr(uint16_t months)
{
	int         i;
	static char str[256];

	if (months == 0)
		return "Any";

	str[0] = 0;
	for (i = 0; i < 12; i++) {
		if ((months & (1 << i)) == 0)
			continue;
		if (str[0])
			strcat(str, " ");
		SAFECAT(str, mon[i]);
	}

	return str;
}

static char* mdaystr(long mdays)
{
	int         i;
	char        tmp[16];
	static char str[256];

	if (mdays == 0 || mdays == 1)
		return "Any";

	str[0] = 0;
	for (i = 1; i < 32; i++) {
		if ((mdays & (1 << i)) == 0)
			continue;
		if (str[0])
			strcat(str, " ");
		sprintf(tmp, "%u", i);
		strcat(str, tmp);
	}

	return str;
}

static char* dropfile(int type, ulong misc)
{
	static char str[128];

	switch (type) {
		case XTRN_SBBS:
			strcpy(str, "XTRN.DAT");
			break;
		case XTRN_WWIV:
			strcpy(str, "CHAIN.TXT");
			break;
		case XTRN_GAP:
			strcpy(str, "DOOR.SYS (31 lines)");
			break;
		case XTRN_DOOR_SYS:
			strcpy(str, "DOOR.SYS (52 lines)");
			break;
		case XTRN_RBBS:
			strcpy(str, "DORINFO#.DEF");
			break;
		case XTRN_RBBS1:
			strcpy(str, "DORINFO1.DEF");
			break;
		case XTRN_WILDCAT:
			strcpy(str, "CALLINFO.BBS");
			break;
		case XTRN_PCBOARD:
			strcpy(str, "PCBOARD.SYS");
			break;
		case XTRN_SPITFIRE:
			strcpy(str, "SFDOORS.DAT");
			break;
		case XTRN_SR:
			strcpy(str, "DOORFILE.SR");
			break;
		case XTRN_TRIBBS:
			strcpy(str, "TRIBBS.SYS");
			break;
		case XTRN_DOOR32:
			strcpy(str, "DOOR32.SYS");
			break;
		default:
			strcpy(str, "None");
			return str;
	}

	if (misc & XTRN_LWRCASE)
		strlwr(str);
	return str;
}

void xprogs_cfg()
{
	static int xprogs_dflt;
	int        i;

	while (1) {
		i = 0;
		strcpy(opt[i++], "Fixed Events");
		strcpy(opt[i++], "Timed Events");
		strcpy(opt[i++], "Native Program List");
		strcpy(opt[i++], "Message Editors");
		strcpy(opt[i++], "Global Hot Key Events");
		strcpy(opt[i++], "Online Programs (Doors)");
		opt[i][0] = 0;
		uifc.helpbuf =
			"`Online External Programs:`\n"
			"\n"
			"From this menu, you can configure external events, external message\n"
			"editors, or online external programs (e.g. `door games`).\n"
		;
		switch (uifc.list(WIN_ORG | WIN_CHE | WIN_ACT, 0, 0, 0, &xprogs_dflt, 0
		                  , "External Programs", opt)) {
			case -1:
				i = save_changes(WIN_MID);
				if (i == -1)
					break;
				if (!i) {
					cfg.new_install = new_install;
					save_xtrn_cfg(&cfg);
					save_main_cfg(&cfg);
					refresh_cfg(&cfg);
				}
				return;
			case 0:
				fevents_cfg();
				break;
			case 1:
				tevents_cfg();
				break;
			case 2:
				natvpgm_cfg();
				break;
			case 3:
				xedit_cfg();
				break;
			case 4:
				hotkey_cfg();
				break;
			case 5:
				xtrnsec_cfg();
				break;
		}
	}
}

bool edit_fixed_event(const char* name, char* cmd, uint32_t* misc, const char* help)
{
	char title[128];
	static int dflt;
	int        i;

	snprintf(title, sizeof title, "%s Event", name);
	while (1) {
		i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-27s%s", "Enabled", (*misc & EVENT_DISABLED) ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-27s%s", native_opt, (*misc & EX_NATIVE) ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-27s%s", use_shell_opt, (*misc & EX_SH) ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-27s%s", "Command Line", cmd);
		opt[i][0] = 0;
		uifc.helpbuf = (char*)help;
		switch (uifc.list(WIN_ACT | WIN_SAV | WIN_MID, 0, 0, 0, &dflt, 0, title, opt)) {
			case -1:
				return *cmd != '\0';
			case 0:
				*misc ^= EVENT_DISABLED;
				uifc.changes = TRUE;
				break;
			case 1:
				toggle_flag(native_opt, misc, EX_NATIVE, false, native_help);
				break;
			case 2:
				toggle_flag(use_shell_prompt, misc, EX_SH, false, use_shell_help);
				break;
			case 3:
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Command"
				           , cmd, LEN_CMD, K_EDIT);
				break;
		}
	}
	return false;
}

void cfg_fixed_events(const char* name, fevent_t* event, const char* help)
{
	char title[128];
	int i;
	int cur = 0, bar = 0;
	static char save_cmd[LEN_CMD + 1] = "";
	static uint32_t save_misc = 0;

	snprintf(title, sizeof title, "%s Events", name);
	while (1) {
		for (i = 0; event->cmd != NULL && event->cmd[i] != NULL; ++i)
			snprintf(opt[i], MAX_OPLN, "%-32.32s", event->cmd[i]);
		opt[i][0] = 0;
		uifc_winmode_t wmode = WIN_RHT | WIN_SAV | WIN_ACT | WIN_INS | WIN_INSACT | WIN_XTR;
		if (save_cmd[0] != '\0')
			wmode |= WIN_PASTE | WIN_PASTEXTR;
		if (i > 0)
			wmode |= WIN_DEL | WIN_CUT | WIN_COPY;
		i = uifc.list(wmode, 2, 0, 0, &cur, &bar, title, opt);
		if (i == -1)
			return;
		char cmd[LEN_CMD + 1];
		uint32_t misc = 0;
		int msk = i & MSK_ON;
		i &= MSK_OFF;
		if (msk == MSK_INS) {
			*cmd = '\0';
			misc = EX_NATIVE;
			if (edit_fixed_event(name, cmd, &misc, help)) {
				int count = strListCount(event->cmd);
				strListInsert(&event->cmd, cmd, i);
				event->misc = realloc_or_free(event->misc, sizeof(*event->misc) * (count + 1));
				if (event->misc == NULL) {
					errormsg(WHERE, ERR_ALLOC, "fixed event misc", sizeof(*event->misc) * (count + 1));
					strListFastDelete(event->cmd, i, 1);
					continue;
				}
				memmove(&event->misc[i + 1], &event->misc[i], sizeof(*event->misc) * (count - i));
				event->misc[i] = misc;
				uifc.changes = TRUE;
			}
			continue;
		}
		if (msk == MSK_DEL || msk == MSK_CUT) {
			if (msk == MSK_CUT) {
				SAFECOPY(save_cmd, event->cmd[i]);
				save_misc = event->misc[i];
			}
			if (event->misc != NULL) {
				int count = strListCount(event->cmd);
				memmove(&event->misc[i], &event->misc[i + 1], sizeof(*event->misc) * (count - i));
			}
			strListFastDelete(event->cmd, i, 1);
			uifc.changes = TRUE;
			continue;
		}
		if (msk == MSK_COPY) {
			SAFECOPY(save_cmd, event->cmd[i]);
			save_misc = event->misc[i];
			continue;
		}
		if (msk == MSK_PASTE) {
			int count = strListCount(event->cmd);
			strListInsert(&event->cmd, save_cmd, i);
			event->misc = realloc_or_free(event->misc, sizeof(*event->misc) * (count + 1));
			if (event->misc == NULL) {
				errormsg(WHERE, ERR_ALLOC, "fixed event misc", sizeof(*event->misc) * (count + 1));
				strListFastDelete(event->cmd, i, 1);
				continue;
			}
			memmove(&event->misc[i + 1], &event->misc[i], sizeof(*event->misc) * (count - i));
			event->misc[i] = misc;
			uifc.changes = TRUE;
			continue;
		}
		if (msk != 0)
			continue;
		if (event->cmd == NULL || event->cmd[i] == NULL)
			continue;
		edit_fixed_event(name, event->cmd[i], &event->misc[i], help);
	}
}


const char* first_fevent(fevent_t event)
{
	const char* result = "";
	for (int i = 0; event.cmd != NULL && event.cmd[i] != NULL; ++i) {
		if (event.misc[i] & EVENT_DISABLED)
			result = "<DISABLED>";
		else if (event.cmd[i][0] != '\0')
			result = event.cmd[i];
	}
	return result;
}

void fevents_cfg()
{
	static int event_dflt;
	int        i;

	while (1) {
		i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-12s%s", "Logon", first_fevent(cfg.sys_logon));
		snprintf(opt[i++], MAX_OPLN, "%-12s%s", "Logout", first_fevent(cfg.sys_logout));
		snprintf(opt[i++], MAX_OPLN, "%-12s%s", "Daily", first_fevent(cfg.sys_daily));
		snprintf(opt[i++], MAX_OPLN, "%-12s%s", "Weekly", first_fevent(cfg.sys_weekly));
		snprintf(opt[i++], MAX_OPLN, "%-12s%s", "Monthly", first_fevent(cfg.sys_monthly));
		opt[i][0] = 0;
		uifc.helpbuf =
			"`Fixed Events:`\n"
			"\n"
			"From this menu, you can configure non-interactive logon and logout\n"
			"events of the Terminal Server and the system's daily, weekly and monthly\n"
			"off-line events, executed at those intervals by the Terminal Server\n"
			"Event Thread.\n"
			"\n"
			"If you wish to add interactive logon or logoff events, you probably want\n"
			"to use online external programs configured to run as logon or\n"
			"logon events, respectively.\n"
			"\n"
			"The commands configured for these events may invoke native executable,\n"
			"shell scripts, Baja or JavaScript modules, or any combination thereof.\n"
			"If a command line requires the system command shell to execute (e.g.\n"
			"uses pipes/redirection or invokes a Unix shell script or DOS/Windows\n"
			"batch/command file), then set the `Use System Shell or New JavaScript\n"
			"Context to Execute` option for that event to ~Yes~.\n"
		;
		switch (uifc.list(WIN_ACT | WIN_SAV | WIN_CHE | WIN_BOT | WIN_RHT, 0, 0, 0, &event_dflt, 0
		                  , "Fixed Events", opt)) {
			case -1:
				return;
			case 0:
				cfg_fixed_events("Logon", &cfg.sys_logon,
				           "`Logon Event:`\n"
				           "\n"
				           "This is the command line for a program that will execute during the\n"
				           "logon sequence of every user.  The program cannot have user interaction.\n"
				           "The program will be executed after the LOGON message is displayed and\n"
				           "before the logon user list is displayed.  If you wish to place a program\n"
				           "in the logon sequence of users that includes interaction or requires\n"
				           "account information, you probably want to use an online external\n"
				           "program configured to run as a logon event.\n"
				           SCFG_CMDLINE_PREFIX_HELP
				           SCFG_CMDLINE_SPEC_HELP
				           );
				break;
			case 1:
				cfg_fixed_events("Logout", &cfg.sys_logout,
				           "`Logout Event:`\n"
				           "\n"
				           "This is the command line for a program that will execute during the\n"
				           "logout sequence of every user.  This program cannot have user\n"
				           "interaction because it is executed after carrier is dropped.  If you\n"
				           "wish to have a program execute before carrier is dropped, you probably\n"
				           "want to use an `Online External Program` configured to run as a logoff\n"
				           "event.\n"
				           SCFG_CMDLINE_PREFIX_HELP
				           SCFG_CMDLINE_SPEC_HELP
				           );
				break;
			case 2:
				cfg_fixed_events("Daily", &cfg.sys_daily,
				           "`Daily Event:`\n"
				           "\n"
				           "This is the command line for a program that will run after the first\n"
				           "user that logs on after midnight, logs off (regardless of what node).\n"
				           SCFG_CMDLINE_PREFIX_HELP
				           SCFG_CMDLINE_SPEC_HELP
				           );
				break;
			case 3:
				cfg_fixed_events("Weekly", &cfg.sys_weekly,
				           "`Weekly Event:`\n"
				           "\n"
				           "Enter a command line for a program that will run once each new week.\n"
				           "\n"
				           "The event will normally run on Sunday.\n"
				           SCFG_CMDLINE_PREFIX_HELP
				           SCFG_CMDLINE_SPEC_HELP
				           );
				break;
			case 4:
				cfg_fixed_events("Monthly", &cfg.sys_monthly,
				           "`Monthly Event:`\n"
				           "\n"
				           "Enter a command line for a program that will run once each new month.\n"
				           "\n"
				           "The event will normally run on the first day of the month.\n"
				           SCFG_CMDLINE_PREFIX_HELP
				           SCFG_CMDLINE_SPEC_HELP
				           );
				break;
		}
	}
}

void tevents_cfg()
{
	static int     dflt, dfltopt, bar;
	char           str[81], done = 0, *p;
	int            j, k;
	int            i;
	static event_t savevent;

	while (1) {
		for (i = 0; i < cfg.total_events && i < MAX_OPTS; i++)
			snprintf(opt[i], MAX_OPLN, "%-*s  %s"
			         , LEN_CODE
			         , cfg.event[i]->code
			         , (cfg.event[i]->misc & EVENT_DISABLED) ? "<DISABLED>" : cfg.event[i]->cmd);
		opt[i][0] = 0;
		j = WIN_SAV | WIN_ACT | WIN_CHE | WIN_RHT;
		if (cfg.total_events)
			j |= WIN_DEL | WIN_COPY | WIN_CUT;
		if (cfg.total_events < MAX_OPTS)
			j |= WIN_INS | WIN_INSACT | WIN_XTR;
		if (savevent.code[0])
			j |= WIN_PASTE | WIN_PASTEXTR;
		uifc.helpbuf =
			"`Timed Events:`\n"
			"\n"
			"This is a list of the configured timed external events.\n"
			"\n"
			"To add an event hit ~ INS ~.\n"
			"\n"
			"To delete an event, select it and hit ~ DEL ~.\n"
			"\n"
			"To configure an event, select it and hit ~ ENTER ~.\n"
		;
		snprintf(str, sizeof str, "Timed Events (%u)", cfg.total_events);
		i = uifc.list(j, 0, 0, 0, &dflt, &bar, str, opt);
		if ((signed)i == -1)
			return;
		int msk = i & MSK_ON;
		i &= MSK_OFF;
		if (msk == MSK_INS) {
			uifc.helpbuf =
				"`Timed Event Internal Code:`\n"
				"\n"
				"This is the internal code for the timed event.\n"
			;
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Event Internal Code", str, LEN_CODE
			               , K_UPPER | K_NOSPACE) < 1)
				continue;
			if (!new_timed_event(i))
				continue;
			SAFECOPY(cfg.event[i]->code, str);
			uifc.changes = TRUE;
			continue;
		}
		if (msk == MSK_DEL || msk == MSK_CUT) {
			if (msk == MSK_CUT)
				savevent = *cfg.event[i];
			free(cfg.event[i]);
			cfg.total_events--;
			for (j = i; j < cfg.total_events; j++)
				cfg.event[j] = cfg.event[j + 1];
			uifc.changes = TRUE;
			continue;
		}
		if (msk == MSK_COPY) {
			savevent = *cfg.event[i];
			continue;
		}
		if (msk == MSK_PASTE) {
			if (!new_timed_event(i))
				continue;
			*cfg.event[i] = savevent;
			uifc.changes = TRUE;
			continue;
		}
		if (msk != 0)
			continue;
		done = 0;
		while (!done) {
			event_t* event = cfg.event[i];
			k = 0;
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Internal Code", cfg.event[i]->code);
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Start-up Directory", cfg.event[i]->dir);
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Command Line", cfg.event[i]->cmd);
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Enabled"
			         , cfg.event[i]->misc & EVENT_DISABLED ? "No":"Yes");
			if (cfg.event[i]->node == NODE_ANY)
				SAFECOPY(str, "Any");
			else
				SAFEPRINTF(str, "%u", cfg.event[i]->node);
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Execution Node", str);
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Execution Months"
			         , monthstr(cfg.event[i]->months));
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Execution Days of Month"
			         , mdaystr(cfg.event[i]->mdays));
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Execution Days of Week", daystr(cfg.event[i]->days));
			if (cfg.event[i]->freq) {
				sprintf(str, "%u times a day", 1440 / cfg.event[i]->freq);
				snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Execution Frequency", str);
			} else {
				sprintf(str, "%2.2u:%2.2u"
				        , cfg.event[i]->time / 60, cfg.event[i]->time % 60);
				snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Execution Time", str);
			}
			if (event->xtrn[0] != '\0')
				snprintf(str, sizeof str, "XTRN: %s", event->xtrn);
			else
				SAFECOPY(str, (event->misc & EVENT_EXCL) ? "Yes" : "No");
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Requires Exclusive Exec", str);
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Force Users Off-line"
			         , cfg.event[i]->misc & EVENT_FORCE ? "Yes":"No");
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", native_opt
			         , cfg.event[i]->misc & EX_NATIVE ? "Yes" : "No");
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", use_shell_opt
			         , cfg.event[i]->misc & XTRN_SH ? "Yes" : "No");
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Background Execution"
			         , cfg.event[i]->misc & EX_BG ? "Yes" : "No");
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Always Run After (re)Init"
			         , cfg.event[i]->misc & EVENT_INIT ? "Yes":"No");
			if (!(cfg.event[i]->misc & EX_BG))
				snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Error Log Level", iniLogLevelStringList()[cfg.event[i]->errlevel]);
			opt[k][0] = 0;
			uifc.helpbuf =
				"`Timed Event:`\n"
				"\n"
				"This is the configuration menu for a timed event.  An event is an\n"
				"external program that performs some type of automated function on the\n"
				"system.  Use this menu to configure how and when this event will be\n"
				"executed.\n"
				"\n"
				"The left and right arrow keys may be used to cycle through events.\n"
			;
			sprintf(str, "%s Timed Event", cfg.event[i]->code);
			uifc_winmode_t wmode = WIN_SAV | WIN_ACT | WIN_L2R | WIN_BOT | WIN_EXTKEYS;
			if (i > 0)
				wmode |= WIN_LEFTKEY;
			if (i + 1 < cfg.total_events)
				wmode |= WIN_RIGHTKEY;
			switch (uifc.list(wmode, 0, 0, 70, &dfltopt, 0
			                  , str, opt)) {
				case -1:
					done = 1;
					break;
				case -CIO_KEY_LEFT - 2:
					if (i > 0)
						i--;
					break;
				case -CIO_KEY_RIGHT - 2:
					if (i + 1 < cfg.total_events)
						i++;
					break;
				case 0:
					SAFECOPY(str, cfg.event[i]->code);
					uifc.helpbuf =
						"`Timed Event Internal Code:`\n"
						"\n"
						"Every timed event must have its own unique internal code for Synchronet\n"
						"to reference it by.  It is helpful if this code is an abbreviation of\n"
						"the command line or program name.\n"
					;
					uifc.input(WIN_MID | WIN_SAV, 0, 17, "Internal Code (unique)"
					           , str, LEN_CODE, K_EDIT | K_UPPER | K_NOSPACE);
					if (code_ok(str))
						SAFECOPY(cfg.event[i]->code, str);
					else {
						uifc.helpbuf = invalid_code;
						uifc.msg(strInvalidCode);
						uifc.helpbuf = NULL;
					}
					break;
				case 1:
					uifc.helpbuf =
						"`Timed Event Start-up Directory:`\n"
						"\n"
						"If a path is specified here, it will be made the current directory\n"
						"before the event's command line is executed.  This eliminates the need\n"
						"for batch files that just change the current drive and directory before\n"
						"executing the event.\n"
						"\n"
						"Command-line specifiers may be included in the start-up directory.\n"
					;
					uifc.input(WIN_MID | WIN_SAV, 0, 10, "Directory"
					           , cfg.event[i]->dir, sizeof(cfg.event[i]->dir) - 1, K_EDIT);
					break;
				case 2:
					uifc.helpbuf =
						"`Timed Event Command Line:`\n"
						"\n"
						"This is the command line to execute upon this timed event.\n"
						SCFG_CMDLINE_PREFIX_HELP
						SCFG_CMDLINE_SPEC_HELP
					;
					uifc.input(WIN_MID | WIN_SAV, 0, 10, "Command"
					           , cfg.event[i]->cmd, sizeof(cfg.event[i]->cmd) - 1, K_EDIT);
					break;

				case 3:
					toggle_flag("Event Enabled", &cfg.event[i]->misc, EVENT_DISABLED, true,
					            "`Timed Event Enabled:`\n"
					            "\n"
					            "If you want disable this event from executing, set this option to ~No~.\n"
					            );
					break;

				case 4:
					uifc.helpbuf =
						"`Timed Event Execution Node:`\n"
						"\n"
						"The Execution Node Number specifies the instance of Synchronet that\n"
						"will run this timed event (or `Any`).\n"
					;
					if (cfg.event[i]->node == NODE_ANY)
						SAFECOPY(str, "Any");
					else
						SAFEPRINTF(str, "%u", cfg.event[i]->node);
					if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Execution Node Number (or Any)"
					               , str, 3, K_EDIT) > 0) {
						if (IS_DIGIT(*str))
							cfg.event[i]->node = atoi(str);
						else
							cfg.event[i]->node = NODE_ANY;
					}
					break;
				case 5:
					uifc.helpbuf =
						"`Months to Execute Event:`\n"
						"\n"
						"Specifies the months (`Jan`-`Dec`, separated by spaces) on which to\n"
						"execute this event, or `Any` to execute event in any/all months.\n"
					;
					SAFECOPY(str, monthstr(cfg.event[i]->months));
					uifc.input(WIN_MID | WIN_SAV, 0, 0, "Months to Execute Event (or Any)"
					           , str, 50, K_EDIT);
					cfg.event[i]->months = 0;
					for (p = str; *p; p++) {
						int num = atoi(p);
						if (num > 0) {
							cfg.event[i]->months |= (1 << (num - 1));
							while (*p && IS_DIGIT(*p))
								p++;
						} else {
							for (j = 0; j < 12; j++)
								if (strnicmp(mon[j], p, 3) == 0) {
									cfg.event[i]->months |= (1 << j);
									p += 2;
									break;
								}
							p++;
						}
					}
					break;
				case 6:
					uifc.helpbuf =
						"`Days of Month to Execute Event:`\n"
						"\n"
						"Specifies the days of the month (`1-31`, separated by spaces) on which \n"
						"to execute this event, or `Any` to execute event on any and all days of\n"
						"the month.\n"
					;
					SAFECOPY(str, mdaystr(cfg.event[i]->mdays));
					uifc.input(WIN_MID | WIN_SAV, 0, 0, "Days of Month to Execute Event (or Any)"
					           , str, 16, K_EDIT);
					cfg.event[i]->mdays = 0;
					for (p = str; *p; p++) {
						if (!isdigit(*p))
							continue;
						cfg.event[i]->mdays |= (1 << atoi(p));
						while (*p && IS_DIGIT(*p))
							p++;
					}
					break;
				case 7:
					j = 0;
					while (1) {
						for (k = 0; k < 7; k++)
							snprintf(opt[k], MAX_OPLN, "%-30s %3s"
							         , wday[k], (cfg.event[i]->days & (1 << k)) ? "Yes":"No");
						strcpy(opt[k++], "All");
						strcpy(opt[k++], "None");
						opt[k][0] = 0;
						uifc.helpbuf =
							"`Days of Week to Execute Event:`\n"
							"\n"
							"These are the days of the week that this event will be executed.\n"
						;
						k = uifc.list(WIN_MID | WIN_SAV | WIN_ACT, 0, 0, 0, &j, 0
						              , "Days of Week to Execute Event", opt);
						if (k == -1)
							break;
						if (k == 7)
							cfg.event[i]->days = 0x7f;
						else if (k == 8)
							cfg.event[i]->days = 0;
						else
							cfg.event[i]->days ^= (1 << k);
						uifc.changes = TRUE;
					}
					break;
				case 8:
					if (cfg.event[i]->freq == 0)
						k = 0;
					else
						k = 1;
					uifc.helpbuf =
						"`Execute Event at a Specific Time:`\n"
						"\n"
						"If you want the system execute this event at a specific time, set\n"
						"this option to `Yes`.  If you want the system to execute this event more\n"
						"than once a day at predetermined intervals, set this option to `No`.\n"
					;
					k = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &k, 0
					              , "Execute Event at a Specific Time", uifcYesNoOpts);
					if (k == 0) {
						sprintf(str, "%2.2u:%2.2u", cfg.event[i]->time / 60
						        , cfg.event[i]->time % 60);
						uifc.helpbuf =
							"`Time to Execute Event:`\n"
							"\n"
							"This is the time (in 24 hour HH:MM format) to execute the event.\n"
						;
						if (uifc.input(WIN_MID | WIN_SAV, 0, 0
						               , "Time to Execute Event (HH:MM)"
						               , str, 5, K_UPPER | K_EDIT) > 0) {
							cfg.event[i]->freq = 0;
							if ((p = strchr(str, ':')) == NULL) {
								uifc.msg("Incorrect time format");
								break;
							}
							cfg.event[i]->time = atoi(str) * 60;
							cfg.event[i]->time += atoi(p + 1);
						}
					}
					else if (k == 1) {
						sprintf(str, "%u"
						        , cfg.event[i]->freq && cfg.event[i]->freq <= 1440
						        ? 1440 / cfg.event[i]->freq : 0);
						uifc.helpbuf =
							"`Number of Executions Per Day:`\n"
							"\n"
							"This is the maximum number of times the system will execute this event\n"
							"per day.\n"
						;
						if (uifc.input(WIN_MID | WIN_SAV, 0, 0
						               , "Number of Executions Per Day"
						               , str, 4, K_NUMBER | K_EDIT) > 0) {
							cfg.event[i]->time = 0;
							k = atoi(str);
							if (k && k <= 1440)
								cfg.event[i]->freq = 1440 / k;
							else
								cfg.event[i]->freq = 0;
						}
					}
					break;
				case 9:
					k = 0;
					strcpy(opt[k++], "Yes, All Nodes Must Be Inactive");
					strcpy(opt[k++], "Yes, An External Program Must Not Be Running");
					strcpy(opt[k++], "No");
					opt[k][0] = '\0';
					uifc.helpbuf =
				        "`Exclusive Event Execution:`\n"
				        "\n"
						"If this event `should not` execute unless all nodes are inactive, set this\n"
						"option to `Yes, All Nodes Must Be Inactive` and when all nodes become\n"
						"become inactive, the event will execute at the scheduled time or when\n"
						"triggered via semaphore.\n"
						"\n"
						"Alternatively, if this event `should not` execute when a specific Online\n"
						"Program (e.g. Door Game) is being run by a user of a node, set this\n"
						"option to `Yes, An External Program Must Not Be Running` and you will\n"
						"next be prompted to specify which external program requires exclusive\n"
						"execution with this event.\n"
						"\n"
						"If this event can execute regardless of node activity, set this option\n"
						"to `No`.\n"
					;
					k = (event->misc & EVENT_EXCL) ? 0 : (event->xtrn[0] ? 1 : 2);
					k = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &k, 0
					              , "Requires Exclusive Execution", opt);
					if (k == 0) {
						event->misc |= EVENT_EXCL;
						event->xtrn[0] = '\0';
						uifc.changes = true;
					}
					else if (k == 1) {
						str_list_t list = strListInit();
						for (int j = 0; j < cfg.total_xtrns; ++j)
							strListPush(&list, cfg.xtrn[j]->code);
						k = getxtrnnum(&cfg, cfg.event[i]->xtrn);
						static int xtrn_bar;
						k = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &k, &xtrn_bar
								, "Exclusive with Which External Program", list);
						if (k < 0)
							break;
						cfg.event[i]->misc &= ~EVENT_EXCL;
						SAFECOPY(cfg.event[i]->xtrn, list[k]);
						uifc.changes = true;
					}
					else if (k == 2) {
						event->misc &= ~EVENT_EXCL;
						event->xtrn[0] = '\0';
						uifc.changes = true;
					}
					break;
				case 10:
					toggle_flag("Force Users Off-line for Event"
					            , &cfg.event[i]->misc, EVENT_FORCE, false,
					            "`Force Users Off-line for Event:`\n"
					            "\n"
					            "If you want to have your users' on-line time reduced so the event can\n"
					            "execute precisely on time, set this option to `Yes`.\n"
					            );
					break;

				case 11:
					toggle_flag(native_opt, &cfg.event[i]->misc, XTRN_NATIVE, false, native_help);
					break;

				case 12:
					toggle_flag(use_shell_prompt, &cfg.event[i]->misc, XTRN_SH, false, use_shell_help);
					break;

				case 13:
					toggle_flag("Background (Asynchronous) Execution"
					            , &cfg.event[i]->misc, EX_BG, false,
					            "`Execute Event in Background (Asynchronously):`\n"
					            "\n"
					            "If you would like this event to run simultaneously with other events,\n"
					            "set this option to `Yes`. Exclusive events will not run in the background.\n"
					            );
					break;

				case 14:
					toggle_flag("Always Run After (re-)Initialization"
					            , &cfg.event[i]->misc, EVENT_INIT, false,
					            "`Always Run After (re-)Initialization:`\n"
					            "\n"
					            "If you want this event to always run after the BBS is initialized or\n"
					            "re-initialized, set this option to ~Yes~.\n"
					            );
					break;

				case 15:
					uifc.helpbuf =
						"`Error Log Level:`\n"
						"\n"
						"Specify the log level used when reporting an error executing this event.\n"
					;
					k = cfg.event[i]->errlevel;
					k = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &k, 0
					              , "Error Log Level", iniLogLevelStringList());
					if (k > 0 && cfg.event[i]->errlevel != k) {
						cfg.event[i]->errlevel = k;
						uifc.changes = true;
					}
					break;
			}
		}
	}
}

const char* io_method(uint32_t mode)
{
	static char str[128];

	sprintf(str, "%s%s%s"
	        , mode & XTRN_UART ? "UART" : (mode & XTRN_FOSSIL) ? "FOSSIL"
	            : (mode & XTRN_STDIO ? "Standard"
	                : mode & XTRN_CONIO ? "Console": (mode & XTRN_NATIVE ? "Socket" : "FOSSIL or UART"))
	        , (mode & (XTRN_STDIO | WWIVCOLOR)) == (XTRN_STDIO | WWIVCOLOR) ? ", WWIV Color" : ""
	        , (mode & (XTRN_STDIO | XTRN_NOECHO)) == (XTRN_STDIO | XTRN_NOECHO) ? ", No Echo" : "");
	return str;
}

void choose_io_method(uint32_t* misc)
{
	int k;

	switch ((*misc) & (XTRN_STDIO | XTRN_UART | XTRN_FOSSIL)) {
		case XTRN_STDIO:
			k = 0;
			break;
		case XTRN_UART:
			k = 2;
			break;
		case XTRN_FOSSIL:
			k = 3;
			break;
		default:
			k = 1;
			break;
	}
	strcpy(opt[0], "Standard");
	if ((*misc) & XTRN_NATIVE) {
		uifc.helpbuf =
			"`I/O Method:`\n"
			"\n"
			"Select the type of input and output to/from this program that you would\n"
			"like to have intercepted and directed to the remote user's terminal.\n"
			"\n"
			"`Standard`\n"
			"   So-called 'Standard I/O' of console mode (typically UNIX) programs.\n"
			"\n"
			"`Socket`\n"
			"   Stream (TCP) socket interface to a native (e.g. Win32 or *nix)\n"
			"   program.  The socket descriptor/handle (number) is passed to the\n"
			"   program either via drop file (e.g. DOOR32.SYS) or command-line\n"
			"   parameter.\n"
			"\n"
			"~ Note ~\n"
			"   This setting is not applied when invoking Baja or JavaScript modules.\n"
		;
		strcpy(opt[1], "Socket");
		opt[2][0] = '\0';
	} else {
		uifc.helpbuf =
			"`I/O Method:`\n"
			"\n"
			"Select the type of input and output to/from this program that you would\n"
			"like to have intercepted and directed to the remote user's terminal.\n"
			"\n"
			"`Standard`\n"
			"   So-called 'Standard I/O' of console mode (typically UNIX) programs.\n"
			"   Int29h is intercepted for output and int16h for keyboard input.\n"
			"   Will not intercept direct screen writes or PC-BIOS int10h calls.\n"
			"\n"
			"`FOSSIL`\n"
			"   Int14h (PC-BIOS) serial communication interface to 16-bit MS-DOS\n"
			"   programs.  Most traditional BBS door games will support FOSSIL.\n"
			"   The port number (contained in the DX register of int14h calls) is\n"
			"   ignored by the Synchronet FOSSIL driver for Windows.\n"
			"\n"
			"`UART`\n"
			"   Communication port I/O of 16-bit MS-DOS programs via emulation of an\n"
			"   NS16550 Universal Asynchronous Receiver/Transmitter (UART), by\n"
			"   default as an IBM-PC `COM1` port (I/O port 3F8h, IRQ 4).\n"
			"\n"
			"~ Note ~\n"
			"   This setting is not applied when invoking Baja or JavaScript modules.\n"
		;
		strcpy(opt[1], "FOSSIL or UART");
		strcpy(opt[2], "UART Only");
		strcpy(opt[3], "FOSSIL Only");
		opt[4][0] = '\0';
	}
	switch (uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &k, 0, "I/O Method"
	                  , opt)) {
		case 0: /* Standard I/O */
			if (((*misc) & (XTRN_STDIO | XTRN_UART | XTRN_FOSSIL)) != XTRN_STDIO) {
				(*misc) |= XTRN_STDIO;
				(*misc) &= ~XTRN_UART | XTRN_FOSSIL;
				uifc.changes = TRUE;
			}
			k = ((*misc) & WWIVCOLOR) ? 0:1;
			uifc.helpbuf =
				"`Program Uses WWIV Color Codes:`\n"
				"\n"
				"If this program was written for use exclusively under ~WWIV~ BBS\n"
				"software, set this option to ~Yes~.\n"
			;
			k = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &k, 0
			              , "Program Uses WWIV Color Codes"
			              , uifcYesNoOpts);
			if (!k && !((*misc) & WWIVCOLOR)) {
				(*misc) |= WWIVCOLOR;
				uifc.changes = TRUE;
			}
			else if (k == 1 && ((*misc) & WWIVCOLOR)) {
				(*misc) &= ~WWIVCOLOR;
				uifc.changes = TRUE;
			}
			k = ((*misc) & XTRN_NOECHO) ? 1:0;
			uifc.helpbuf =
				"`Echo Input:`\n"
				"\n"
				"If you want the BBS to copy (\"echo\") all keyboard input to the screen\n"
				"output, set this option to ~Yes~ (for native Win32 programs only).\n"
			;
			k = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &k, 0
			              , "Echo Keyboard Input"
			              , uifcYesNoOpts);
			if (!k && ((*misc) & XTRN_NOECHO)) {
				(*misc) &= ~XTRN_NOECHO;
				uifc.changes = TRUE;
			} else if (k == 1 && !((*misc) & XTRN_NOECHO)) {
				(*misc) |= XTRN_NOECHO;
				uifc.changes = TRUE;
			}
			break;
		case 1: /* FOSSIL or UART or Socket */
			if (((*misc) & (XTRN_STDIO | XTRN_UART | XTRN_FOSSIL)) != 0) {
				(*misc) &= ~(XTRN_UART | XTRN_FOSSIL | XTRN_STDIO | WWIVCOLOR | XTRN_NOECHO);
				uifc.changes = TRUE;
			}
			break;
		case 2: /* UART */
			if (((*misc) & (XTRN_STDIO | XTRN_UART | XTRN_FOSSIL)) != XTRN_UART) {
				(*misc) |= XTRN_UART;
				(*misc) &= ~(XTRN_FOSSIL | XTRN_STDIO | WWIVCOLOR | XTRN_NOECHO);
				uifc.changes = TRUE;
			}
			break;
		case 3: /* FOSSIL */
			if (((*misc) & (XTRN_STDIO | XTRN_UART | XTRN_FOSSIL)) != XTRN_FOSSIL) {
				(*misc) |= XTRN_FOSSIL;
				(*misc) &= ~(XTRN_UART | XTRN_STDIO | WWIVCOLOR | XTRN_NOECHO);
				uifc.changes = TRUE;
			}
			break;
	}
}

void xtrn_cfg(int section)
{
	static int    ext_dflt, ext_bar, sub_bar, opt_dflt, time_dflt;
	char          str[128], code[128], done = 0;
	int           j, k;
	int           i, xtrnnum[MAX_OPTS + 1];
	static xtrn_t savxtrn;

	while (1) {
		for (i = 0, j = 0; i < cfg.total_xtrns && j < MAX_OPTS; i++)
			if (cfg.xtrn[i]->sec == section) {
				snprintf(opt[j], MAX_OPLN, "%-25s", cfg.xtrn[i]->name);
				xtrnnum[j++] = i;
			}
		xtrnnum[j] = cfg.total_xtrns;
		opt[j][0] = 0;
		i = WIN_ACT | WIN_CHE | WIN_SAV | WIN_RHT;
		if (j)
			i |= WIN_DEL | WIN_COPY | WIN_CUT;
		if (cfg.total_xtrns < MAX_OPTS)
			i |= WIN_INS | WIN_INSACT | WIN_XTR;
		if (savxtrn.name[0])
			i |= WIN_PASTE | WIN_PASTEXTR;
		uifc.helpbuf =
			"`Online External Programs:`\n"
			"\n"
			"This is a list of the configured online external programs (doors).\n"
			"\n"
			"To add a program, select the desired location with the arrow keys and\n"
			"hit ~ INS ~.\n"
			"\n"
			"To delete a program, select it with the arrow keys and hit ~ DEL ~.\n"
			"\n"
			"To configure a program, select it with the arrow keys and hit ~ ENTER ~.\n"
		;
		snprintf(str, sizeof str, "%s Programs (%d)", cfg.xtrnsec[section]->name, j);
		i = uifc.list(i, 0, 0, 45, &ext_dflt, &ext_bar, str, opt);
		if ((signed)i == -1)
			return;
		int msk = i & MSK_ON;
		i &= MSK_OFF;
		if (msk == MSK_INS) {
			uifc.helpbuf =
				"`Program Name:`\n"
				"\n"
				"This is the name or description of the online program (door).\n"
			;
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Program Name", str, 40, K_NONE) < 1)
				continue;
			SAFECOPY(code, str);
			prep_code(code, /* prefix: */ NULL);
			uifc.helpbuf =
				"`Program Internal Code:`\n"
				"\n"
				"Every online program must have its own unique code for Synchronet to\n"
				"refer to it internally.  This code is usually an abbreviation of the\n"
				"online program name.\n"
			;
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Internal Code"
			               , code, LEN_CODE, K_EDIT | K_UPPER | K_NOSPACE) < 1)
				continue;
			if (xtrnnum_is_valid(&cfg, getxtrnnum(&cfg, code))) {
				uifc.msg(strDuplicateCode);
				continue;
			}
			if (!code_ok(code)) {
				uifc.helpbuf = invalid_code;
				uifc.msg(strInvalidCode);
				uifc.helpbuf = NULL;
				continue;
			}
			if (!new_external_program(xtrnnum[i], section))
				continue;
			SAFECOPY(cfg.xtrn[xtrnnum[i]]->name, str);
			SAFECOPY(cfg.xtrn[xtrnnum[i]]->code, code);
			uifc.changes = TRUE;
			continue;
		}
		if (msk == MSK_DEL || msk == MSK_CUT) {
			if (msk == MSK_CUT)
				savxtrn = *cfg.xtrn[xtrnnum[i]];
			free(cfg.xtrn[xtrnnum[i]]);
			cfg.total_xtrns--;
			for (j = xtrnnum[i]; j < cfg.total_xtrns; j++)
				cfg.xtrn[j] = cfg.xtrn[j + 1];
			uifc.changes = TRUE;
			continue;
		}
		if (msk == MSK_COPY) {
			savxtrn = *cfg.xtrn[xtrnnum[i]];
			continue;
		}
		if (msk == MSK_PASTE) {
			if (!new_external_program(xtrnnum[i], section))
				continue;
			*cfg.xtrn[xtrnnum[i]] = savxtrn;
			cfg.xtrn[xtrnnum[i]]->sec = section;
			uifc.changes = TRUE;
			continue;
		}
		if (msk != 0)
			continue;
		done = 0;
		i = xtrnnum[i];
		while (!done) {
			k = 0;
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Name", cfg.xtrn[i]->name);
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Internal Code", cfg.xtrn[i]->code);
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Start-up Directory", cfg.xtrn[i]->path);
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Command Line", cfg.xtrn[i]->cmd);
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Clean-up Command Line", cfg.xtrn[i]->clean);
			if (cfg.xtrn[i]->cost)
				sprintf(str, "%" PRIu32 " credits", cfg.xtrn[i]->cost);
			else
				strcpy(str, "None");
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Execution Cost", str);
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Access Requirements", cfg.xtrn[i]->arstr);
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Execution Requirements"
			         , cfg.xtrn[i]->run_arstr);
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Multiple Concurrent Users"
			         , cfg.xtrn[i]->misc & MULTIUSER ? "Yes" : "No");
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", native_opt
			         , cfg.xtrn[i]->misc & XTRN_NATIVE ? "Yes" : "No");
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "I/O Method", io_method(cfg.xtrn[i]->misc));
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", use_shell_opt
			         , cfg.xtrn[i]->misc & XTRN_SH ? "Yes" : "No");
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Modify User Data"
			         , cfg.xtrn[i]->misc & MODUSERDAT ? "Yes" : "No");
			switch (cfg.xtrn[i]->event) {
				case EVENT_LOGON:
					strcpy(str, "Logon");
					break;
				case EVENT_LOGOFF:
					strcpy(str, "Logoff");
					break;
				case EVENT_NEWUSER:
					strcpy(str, "New User");
					break;
				case EVENT_BIRTHDAY:
					strcpy(str, "Birthday");
					break;
				case EVENT_POST:
					strcpy(str, "Message Posted");
					break;
				case EVENT_UPLOAD:
					strcpy(str, "File Uploaded");
					break;
				case EVENT_DOWNLOAD:
					strcpy(str, "File Downloaded");
					break;
				case EVENT_LOCAL_CHAT:
					strcpy(str, "Local/Sysop Chat");
					break;
				default:
					strcpy(str, "No");
					break;
			}
			if ((cfg.xtrn[i]->misc & EVENTONLY) && cfg.xtrn[i]->event)
				strcat(str, ", Only");
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Execute on Event", str);
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Pause After Execution"
			         , cfg.xtrn[i]->misc & XTRN_PAUSE ? "Yes" : "No");
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Disable Local Display"
			         , cfg.xtrn[i]->misc & XTRN_NODISPLAY ? "Yes" : "No");
			snprintf(opt[k++], MAX_OPLN, "%-23.23s%-4s%s", "BBS Drop File Type"
			         , (cfg.xtrn[i]->type != XTRN_NONE && (cfg.xtrn[i]->misc & REALNAME)) ? "(R)":nulstr
			         , dropfile(cfg.xtrn[i]->type, cfg.xtrn[i]->misc));
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Place Drop File In"
			         , cfg.xtrn[i]->misc & STARTUPDIR ? "Start-Up Directory":cfg.xtrn[i]->misc & XTRN_TEMP_DIR ? "Temp Directory" : "Node Directory");
			snprintf(opt[k++], MAX_OPLN, "Time Options...");
			opt[k][0] = 0;
			uifc.helpbuf =
				"`Online Program Configuration:`\n"
				"\n"
				"This menu is for configuring the selected online program.\n"
				"\n"
				"For detailed instructions for configuring BBS doors, see\n"
				"`http://wiki.synchro.net/howto:door:index`\n"
				"\n"
				"The left and right arrow keys may be used to cycle through programs.\n"
			;
			uifc_winmode_t wmode = WIN_SAV | WIN_ACT | WIN_MID | WIN_EXTKEYS;
			int            prev = prev_program(i);
			int            next = next_program(i);
			if (prev != i)
				wmode |= WIN_LEFTKEY;
			if (next != i)
				wmode |= WIN_RIGHTKEY;
			switch (uifc.list(wmode, 0, 0, 60, &opt_dflt, &sub_bar, cfg.xtrn[i]->name
			                  , opt)) {
				case -1:
					done = 1;
					break;
				case -CIO_KEY_LEFT - 2:
					i = prev;
					break;
				case -CIO_KEY_RIGHT - 2:
					i = next;
					break;
				case __COUNTER__:
					uifc.helpbuf =
						"`Online Program Name:`\n"
						"\n"
						"This is the name or description of the online program (door).\n"
					;
					SAFECOPY(str, cfg.xtrn[i]->name);
					if (!uifc.input(WIN_MID | WIN_SAV, 0, 10, "Online Program Name"
					                , cfg.xtrn[i]->name, sizeof(cfg.xtrn[i]->name) - 1, K_EDIT))
						SAFECOPY(cfg.xtrn[i]->name, str);
					break;
				case __COUNTER__:
					uifc.helpbuf =
						"`Online Program Internal Code:`\n"
						"\n"
						"Every online program must have its own unique code for Synchronet to\n"
						"refer to it internally.  This code is usually an abbreviation of the\n"
						"online program name.\n"
					;
					SAFECOPY(str, cfg.xtrn[i]->code);
					if (uifc.input(WIN_MID | WIN_SAV, 0, 10, "Internal Code"
					           , str, LEN_CODE, K_UPPER | K_EDIT | K_NOSPACE | K_CHANGED | K_FIND) < 1)
						break;
					if (xtrnnum_is_valid(&cfg, getxtrnnum(&cfg, str))) {
						uifc.msg(strDuplicateCode);
						break;
					}
					if (code_ok(str)) {
						SAFECOPY(cfg.xtrn[i]->code, str);
						uifc.changes = TRUE;
					}
					else {
						uifc.helpbuf = invalid_code;
						uifc.msg(strInvalidCode);
						uifc.helpbuf = NULL;
					}
					break;
				case __COUNTER__:
					uifc.helpbuf =
						"`Online Program Start-up Directory:`\n"
						"\n"
						"If a path is specified here, it will be made the current directory\n"
						"before the program's command line is executed.  This eliminates the need\n"
						"for batch files that just change the current drive and directory before\n"
						"executing the program.\n"
						"\n"
						"For BBS doors, this is often the path where the program was installed.\n"
						"\n"
						"Command-line specifiers may be included in the start-up directory.\n"
					;
					uifc.input(WIN_MID | WIN_SAV, 0, 10, ""
					           , cfg.xtrn[i]->path, sizeof(cfg.xtrn[i]->path) - 1, K_EDIT);
					break;
				case __COUNTER__:
					uifc.helpbuf =
						"`Online Program Command Line:`\n"
						"\n"
						"This is the command line to execute to run the online program.\n"
						SCFG_CMDLINE_PREFIX_HELP
						SCFG_CMDLINE_SPEC_HELP
					;
					uifc.input(WIN_MID | WIN_SAV, 0, 10, "Command"
					           , cfg.xtrn[i]->cmd, sizeof(cfg.xtrn[i]->cmd) - 1, K_EDIT);
					break;
				case __COUNTER__:
					uifc.helpbuf =
						"`Online Program Clean-up Command:`\n"
						"\n"
						"This is the command line to execute after the main command line.  This\n"
						"option is usually only used for multiuser online programs.\n"
						SCFG_CMDLINE_PREFIX_HELP
						SCFG_CMDLINE_SPEC_HELP
					;
					uifc.input(WIN_MID | WIN_SAV, 0, 10, "Clean-up"
					           , cfg.xtrn[i]->clean, sizeof(cfg.xtrn[i]->clean) - 1, K_EDIT);
					break;
				case __COUNTER__:
					ultoa(cfg.xtrn[i]->cost, str, 10);
					uifc.helpbuf =
						"`Online Program Cost to Run:`\n"
						"\n"
						"If you want users to be charged credits to run this online program,\n"
						"set this value to the number of credits to charge.  If you want this\n"
						"online program to be free, set this value to `0`.\n"
					;
					uifc.input(WIN_MID | WIN_SAV, 0, 0, "Cost to Run (in Credits)"
					           , str, 10, K_EDIT | K_NUMBER);
					cfg.xtrn[i]->cost = atol(str);
					break;
				case __COUNTER__:
					sprintf(str, "%s Access", cfg.xtrn[i]->name);
					getar(str, cfg.xtrn[i]->arstr);
					break;
				case __COUNTER__:
					sprintf(str, "%s Execution", cfg.xtrn[i]->name);
					getar(str, cfg.xtrn[i]->run_arstr);
					break;
				case __COUNTER__:
					toggle_flag("Supports Multiple Users", &cfg.xtrn[i]->misc, MULTIUSER, false,
					            "`Supports Multiple Users:`\n"
					            "\n"
					            "If this online program supports multiple simultaneous users (nodes),\n"
					            "set this option to `Yes`.\n"
					            );
					break;
				case __COUNTER__:
					toggle_flag(native_opt, &cfg.xtrn[i]->misc, XTRN_NATIVE, false, native_help);
					break;
				case __COUNTER__:
					choose_io_method(&cfg.xtrn[i]->misc);
					break;
				case __COUNTER__:
					toggle_flag(use_shell_prompt, &cfg.xtrn[i]->misc, XTRN_SH, false, use_shell_help);
					break;
				case __COUNTER__:
					toggle_flag("Program Can Modify User Data"
					            , &cfg.xtrn[i]->misc, MODUSERDAT, false,
					            "`Program Can Modify User Data:`\n"
					            "\n"
					            "If this online programs recognizes the Synchronet MODUSER.DAT format\n"
					            "or the RBBS/QuickBBS EXITINFO.BBS format and you want it to be able to\n"
					            "modify the data of users who run the program, set this option to `Yes`.\n"
					            );
					break;
				case __COUNTER__:
					k = 0;
					strcpy(opt[k++], "No");
					strcpy(opt[k++], "Logon");
					strcpy(opt[k++], "Logoff");
					strcpy(opt[k++], "New User");
					strcpy(opt[k++], "Birthday");
					strcpy(opt[k++], "Message Posted");
					strcpy(opt[k++], "File Uploaded");
					strcpy(opt[k++], "File Downloaded");
					strcpy(opt[k++], "Local/Sysop Chat");
					opt[k][0] = 0;
					k = cfg.xtrn[i]->event;
					uifc.helpbuf =
						"`Execute Online Program on Event:`\n"
						"\n"
						"If you would like this online program to automatically execute on a\n"
						"specific user event, select the event. Otherwise, select `No`.\n"
					;
					k = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &k, 0
					              , "Execute on Event", opt);
					if (k == -1)
						break;
					if (cfg.xtrn[i]->event != k) {
						cfg.xtrn[i]->event = k;
						uifc.changes = TRUE;
					}
					if (!cfg.xtrn[i]->event) {
						if (cfg.xtrn[i]->misc & EVENTONLY) {
							cfg.xtrn[i]->misc &= ~EVENTONLY;
							uifc.changes = TRUE;
						}
						break;
					}
					toggle_flag("Execute as Event Only"
					            , &cfg.xtrn[i]->misc, EVENTONLY, false,
					            "`Execute Online Program as Event Only:`\n"
					            "\n"
					            "If you would like this online program to execute as an event only\n"
					            "(not available to users on the online program menu), set this option\n"
					            "to `Yes`.\n"
					            );
					break;
				case __COUNTER__:
					toggle_flag("Pause After Execution", &cfg.xtrn[i]->misc, XTRN_PAUSE, false,
					            "`Pause Screen After Execution:`\n"
					            "\n"
					            "Set this option to ~Yes~ if you would like an automatic screen pause\n"
					            "(`[Hit a key]` prompt) to appear after the program executes.\n"
					            "\n"
					            "This can be useful if the program displays information just before\n"
					            "exiting or you want to debug a program with a program not running\n"
					            "correctly.\n"
					            );
					break;
				case __COUNTER__:
					toggle_flag("Disable Local Screen Display"
					            , &cfg.xtrn[i]->misc, XTRN_NODISPLAY, false,
					            "`Disable Local Screen Display:`\n"
					            "\n"
					            "Set this option to `Yes` if you wish to prevent this program from\n"
					            "displaying on the local screen.\n"
					            "\n"
					            "This will disable 'Screen' output in the `DOOR.SYS` and `PCBOARD.SYS` drop\n"
					            "files and on Windows, stop the creation of a new console window when\n"
					            "executing this program."
					            );
					break;
				case __COUNTER__:
					k = 0;
					strcpy(opt[k++], "None");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "Synchronet", "XTRN.DAT");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "WWIV", "CHAIN.TXT");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "GAP (extended)", "DOOR.SYS (52 lines)");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "RBBS/QuickBBS", "DORINFO#.DEF");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "Wildcat", "CALLINFO.BBS");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "PCBoard", "PCBOARD.SYS");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "SpitFire", "SFDOORS.DAT");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "GAP (original)", "DOOR.SYS (31 lines)");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "Solar Realms", "DOORFILE.SR");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "RBBS/QuickBBS", "DORINFO1.DEF");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "TriBBS", "TRIBBS.SYS");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "Mystic", "DOOR32.SYS");
					opt[k][0] = 0;
					k = cfg.xtrn[i]->type;
					uifc.helpbuf =
						"`Online Program BBS Drop File Type:`\n"
						"\n"
						"If this online program requires a specific BBS data (drop) file\n"
						"format, select the file format from the list.\n"
						"\n"
						"Supported drop file formats:\n"
						"\n"
						" ~ Software        Write File(s)                 Read File(s) ~\n"
						"  Mystic          DOOR32.SYS\n"
						"  GAP             DOOR.SYS                      DOOR.SYS\n"
						"  WWIV            CHAIN.TXT\n"
						"  PCBoard 15.3    PCBOARD.SYS & USERS.SYS       USERS.SYS\n"
						"  RBBS/QuickBBS   DORINFO#.DEF & EXITINFO.BBS   EXITINFO.BBS\n"
						"  Wildcat! 2.x    CALLINFO.BBS\n"
						"  SpitFire        SFDOORS.DAT\n"
						"  TriBBS          TRIBBS.SYS\n"
						"  Solar Realms    DOORFILE.SR\n"
						"  Synchronet      XTRN.DAT                      MODUSER.DAT\n"
						"\n"
						"`DOOR.SYS`\n"
						"The drop file format compatible with the largest number of online\n"
						"programs (e.g. door games) written for MS-DOS based BBSes is the\n"
						"`DOOR.SYS` file format.  Synchronet supports both the original (GAP)\n"
						"31-line DOOR.SYS file format and the extended (later) 52-line format.\n"
						"The 31-line DOOR.SYS file created by Synchronet includes 21 extra blank\n"
						"lines to accommodate software that expects more than 31 lines, but has\n"
						"an issue with the contents of the additional lines as populated by SBBS.\n"
						"If you encounter door compatibility issues with the original 31-line\n"
						"format, try using the 52-line format instead (or vice versa).\n"
					;
					k = uifc.list(WIN_MID | WIN_ACT, 0, 0, 0, &k, 0
					              , "BBS Drop File Type", opt);
					if (k == -1)
						break;
					if (cfg.xtrn[i]->type != k) {
						cfg.xtrn[i]->type = k;
						if (cfg.xtrn[i]->type == XTRN_DOOR32)
							cfg.xtrn[i]->misc |= XTRN_NATIVE;
						uifc.changes = TRUE;
					}
					if (cfg.xtrn[i]->type && cfg.uq & UQ_ALIASES) {
						k = (cfg.xtrn[i]->misc & REALNAME) ? 0:1;
						k = uifc.list(WIN_MID, 0, 0, 0, &k, 0, "Use Real Names", uifcYesNoOpts);
						if (k == -1)
							break;
						if (k == 0 && !(cfg.xtrn[i]->misc & REALNAME)) {
							cfg.xtrn[i]->misc |= REALNAME;
							uifc.changes = TRUE;
						}
						else if (k == 1 && (cfg.xtrn[i]->misc & REALNAME)) {
							cfg.xtrn[i]->misc &= ~REALNAME;
							uifc.changes = TRUE;
						}
					}
					if (cfg.xtrn[i]->type) {
						k = (cfg.xtrn[i]->misc & XTRN_LWRCASE) ? 0:1;
						k = uifc.list(WIN_MID, 0, 0, 0, &k, 0, "Lowercase Filename", uifcYesNoOpts);
						if (k == 0 && !(cfg.xtrn[i]->misc & XTRN_LWRCASE)) {
							cfg.xtrn[i]->misc |= XTRN_LWRCASE;
							uifc.changes = TRUE;
						}
						else if (k == 1 && (cfg.xtrn[i]->misc & XTRN_LWRCASE)) {
							cfg.xtrn[i]->misc &= ~XTRN_LWRCASE;
							uifc.changes = TRUE;
						}
					}
					break;
				case __COUNTER__:
					k = (cfg.xtrn[i]->misc & STARTUPDIR) ? 1 : (cfg.xtrn[i]->misc & XTRN_TEMP_DIR) ? 2 : 0;
					strcpy(opt[0], "Node Directory");
					strcpy(opt[1], "Start-up Directory");
					strcpy(opt[2], "Temporary Directory");
					opt[3][0] = 0;
					uifc.helpbuf =
						"`Directory for Drop File:`\n"
						"\n"
						"You can have the data (drop) file created in the current `Node Directory`,\n"
						"current `Temporary Directory`, or the `Start-up Directory` (if specified).\n"
						"\n"
						"For multi-user doors, you usually will `not` want the drop files placed\n"
						"in the start-up directory due to potential conflict with other nodes.\n"
						"\n"
						"The safest option is the `Temp Directory`, as this directory is unique per\n"
						"terminal server node and is automatically cleared-out for each logon.\n"
						"\n"
						"`Note:`\n"
						"Many classic Synchronet (XSDK) doors assume the drop file will be\n"
						"located in the `Node Directory`.\n"
					;
					k = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &k, 0, "Create Drop File In"
					              , opt);
					if (!k && (cfg.xtrn[i]->misc & (STARTUPDIR | XTRN_TEMP_DIR)) != 0) {
						cfg.xtrn[i]->misc &= ~(STARTUPDIR | XTRN_TEMP_DIR);
						uifc.changes = TRUE;
					}
					else if (k == 1 && (cfg.xtrn[i]->misc & (STARTUPDIR | XTRN_TEMP_DIR)) != STARTUPDIR) {
						cfg.xtrn[i]->misc &= ~(STARTUPDIR | XTRN_TEMP_DIR);
						cfg.xtrn[i]->misc |= STARTUPDIR;
						uifc.changes = TRUE;
					}
					else if (k == 2 && (cfg.xtrn[i]->misc & (STARTUPDIR | XTRN_TEMP_DIR)) != XTRN_TEMP_DIR) {
						cfg.xtrn[i]->misc &= ~(STARTUPDIR | XTRN_TEMP_DIR);
						cfg.xtrn[i]->misc |= XTRN_TEMP_DIR;
						uifc.changes = TRUE;
					}
					break;
				case __COUNTER__:
					while (1) {
						k = 0;
						if (cfg.xtrn[i]->textra)
							sprintf(str, "%u minutes", cfg.xtrn[i]->textra);
						else
							strcpy(str, "None");
						snprintf(opt[k++], MAX_OPLN, "%-25.25s%s", "Extra Time", str);
						if (cfg.xtrn[i]->maxtime)
							sprintf(str, "%u minutes", cfg.xtrn[i]->maxtime);
						else
							strcpy(str, "None");
						snprintf(opt[k++], MAX_OPLN, "%-25.25s%s", "Maximum Time", str);
						snprintf(opt[k++], MAX_OPLN, "%-25.25s%s", "Suspended (Free) Time"
						         , cfg.xtrn[i]->misc & FREETIME ? "Yes" : "No");
						snprintf(opt[k++], MAX_OPLN, "%-25.25s%s", "Monitor Time Left"
						         , cfg.xtrn[i]->misc & XTRN_CHKTIME ? "Yes" : "No");
						if (cfg.xtrn[i]->max_inactivity)
							duration_to_vstr(cfg.xtrn[i]->max_inactivity, str, sizeof(str));
						else
							SAFECOPY(str, "None");
						snprintf(opt[k++], MAX_OPLN, "%-25.25s%s", "Maximum Inactivity", str);
						opt[k][0] = 0;
						uifc.helpbuf =
							"`Online Program Time Options:`\n"
							"\n"
							"This sub-menu allows you to define specific preferences regarding the\n"
							"time users spend running this program.\n"
						;
						k = uifc.list(WIN_SAV | WIN_ACT | WIN_RHT | WIN_BOT, 0, 0, 40
						              , &time_dflt, 0
						              , "Online Program Time Options", opt);
						if (k == -1)
							break;
						switch (k) {
							case 0:
								ultoa(cfg.xtrn[i]->textra, str, 10);
								uifc.helpbuf =
									"`Extra Time to Give User in Program:`\n"
									"\n"
									"If you want to give users extra time while in this online program,\n"
									"set this value to the number of minutes to add to their current time\n"
									"left online.\n"
								;
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Extra Time to Give User (in minutes)"
								           , str, 2, K_EDIT | K_NUMBER);
								cfg.xtrn[i]->textra = atoi(str);
								break;
							case 1:
								ultoa(cfg.xtrn[i]->maxtime, str, 10);
								uifc.helpbuf =
									"`Maximum Time Allowed in Program:`\n"
									"\n"
									"If this program supports a drop file that contains the number of minutes\n"
									"left online for the current user, this option allows the sysop to set\n"
									"the maximum number of minutes that will be allowed in the drop file.\n"
									"\n"
									"Setting this option to `0`, disables this feature.\n"
								;
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Maximum Time (in minutes, 0=disabled)"
								           , str, 2, K_EDIT | K_NUMBER);
								cfg.xtrn[i]->maxtime = atoi(str);
								break;
							case 2:
								toggle_flag("Suspended (Free) Time"
								            , &cfg.xtrn[i]->misc, FREETIME, false,
								            "`Suspended (Free) Time:`\n"
								            "\n"
								            "If you want the user's time online to be suspended while running this\n"
								            "online program (e.g. Free Time), set this option to `Yes`.\n"
								            );
								break;
							case 3:
								toggle_flag("Monitor Time Left"
								            , &cfg.xtrn[i]->misc, XTRN_CHKTIME, false,
								            "`Monitor Time Left:`\n"
								            "\n"
								            "If you want Synchronet to monitor the user's time left online while this\n"
								            "program runs (and disconnect the user if their time runs out), set this\n"
								            "option to `Yes`.\n"
								            );
								break;
							case 4:
								if (cfg.xtrn[i]->max_inactivity < 1)
									SAFECOPY(str, "None");
								else
									duration_to_str(cfg.xtrn[i]->max_inactivity, str, sizeof(str));
								uifc.helpbuf =
									"`Duration Before Inactive-Socket Disconnection:`\n"
									"\n"
									"This is the duration of time the client socket must be inactive before\n"
									"the user will be automatically disconnected and control returned to the\n"
									"BBS.\n"
									"\n"
									"`H`-exempt users will not be disconnected due to inactivity.\n"
									"\n"
									"A setting of `0` (the default) will disable this inactivity detection\n"
									"feature and rely on the external program itself to detect inactivity\n"
									"and return control to the BBS.\n"
								;
								uifc.input(WIN_MID | WIN_SAV, 0, 14
								           , "Duration Before Inactive-Socket Disconnection"
								           , str, 5, K_EDIT);
								cfg.xtrn[i]->max_inactivity = (uint)parse_duration(str);
								break;
						}
					}
					break;
			}
		}
	}
}

void xedit_cfg()
{
	static int     dflt, dfltopt, bar;
	char           str[81], code[81], done = 0;
	int            j, k;
	int            i;
	static xedit_t savxedit;

	while (1) {
		for (i = 0; i < cfg.total_xedits && i < MAX_OPTS; i++)
			snprintf(opt[i], MAX_OPLN, "%-*s  %s", LEN_CODE, cfg.xedit[i]->code, cfg.xedit[i]->rcmd);
		opt[i][0] = 0;
		j = WIN_SAV | WIN_ACT | WIN_CHE | WIN_RHT;
		if (cfg.total_xedits)
			j |= WIN_DEL | WIN_COPY | WIN_CUT;
		if (cfg.total_xedits < MAX_OPTS)
			j |= WIN_INS | WIN_INSACT | WIN_XTR;
		if (savxedit.name[0])
			j |= WIN_PASTE | WIN_PASTEXTR;
		uifc.helpbuf =
			"`Message Editors:`\n"
			"\n"
			"This is a list of the configured external message editors.\n"
			"\n"
			"To add an editor, select the desired location and hit ~ INS ~.\n"
			"\n"
			"To delete an editor, select it and hit ~ DEL ~.\n"
			"\n"
			"To configure an editor, select it and hit ~ ENTER ~.\n"
		;
		i = uifc.list(j, 0, 0, 45, &dflt, &bar, "Message Editors", opt);
		if ((signed)i == -1)
			return;
		int msk = i & MSK_ON;
		i &= MSK_OFF;
		if (msk == MSK_INS) {
			uifc.helpbuf =
				"`Message Editor Name:`\n"
				"\n"
				"This is the name or description of the message editor.\n"
			;
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Message Editor Name", str, 40, K_NONE) < 1)
				continue;
			SAFECOPY(code, str);
			prep_code(code, /* prefix: */ NULL);
			uifc.helpbuf =
				"`Message Editor Internal Code:`\n"
				"\n"
				"This is the internal code for the external message editor.\n"
			;
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Message Editor Internal Code", code, 8
			               , K_UPPER | K_EDIT | K_NOSPACE) < 1)
				continue;
			if (!code_ok(code)) {
				uifc.helpbuf = invalid_code;
				uifc.msg(strInvalidCode);
				uifc.helpbuf = NULL;
				continue;
			}
			if (!new_external_editor(i))
				continue;
			SAFECOPY(cfg.xedit[i]->name, str);
			SAFECOPY(cfg.xedit[i]->code, code);
			uifc.changes = TRUE;
			continue;
		}
		if (msk == MSK_DEL || msk == MSK_CUT) {
			if (msk == MSK_CUT)
				savxedit = *cfg.xedit[i];
			free(cfg.xedit[i]);
			cfg.total_xedits--;
			for (j = i; j < cfg.total_xedits; j++)
				cfg.xedit[j] = cfg.xedit[j + 1];
			uifc.changes = TRUE;
			continue;
		}
		if (msk == MSK_COPY) {
			savxedit = *cfg.xedit[i];
			continue;
		}
		if (msk == MSK_PASTE) {
			if (!new_external_editor(i))
				continue;
			*cfg.xedit[i] = savxedit;
			uifc.changes = TRUE;
			continue;
		}
		if (msk != 0)
			continue;
		done = 0;
		while (!done) {
			k = 0;
			snprintf(opt[k++], MAX_OPLN, "%-32s%s", "Name", cfg.xedit[i]->name);
			snprintf(opt[k++], MAX_OPLN, "%-32s%s", "Internal Code", cfg.xedit[i]->code);
			snprintf(opt[k++], MAX_OPLN, "%-32s%s", "Command Line", cfg.xedit[i]->rcmd);
			snprintf(opt[k++], MAX_OPLN, "%-32s%s", "Access Requirements", cfg.xedit[i]->arstr);
			snprintf(opt[k++], MAX_OPLN, "%-32s%s", native_opt
			         , cfg.xedit[i]->misc & XTRN_NATIVE ? "Yes" : "No");
			snprintf(opt[k++], MAX_OPLN, "%-32s%s", "I/O Method", io_method(cfg.xedit[i]->misc));
			snprintf(opt[k++], MAX_OPLN, "%-32s%s", use_shell_opt
			         , cfg.xedit[i]->misc & XTRN_SH ? "Yes" : "No");
			snprintf(opt[k++], MAX_OPLN, "%-32s%s", "Record Terminal Width"
			         , cfg.xedit[i]->misc & SAVECOLUMNS ? "Yes" : "No");
			str[0] = 0;
			if (cfg.xedit[i]->misc & QUOTEWRAP) {
				if (cfg.xedit[i]->quotewrap_cols == 0)
					SAFECOPY(str, ", for terminal width");
				else
					SAFEPRINTF(str, ", for %u columns", (uint)cfg.xedit[i]->quotewrap_cols);
			}
			snprintf(opt[k++], MAX_OPLN, "%-32s%s%s", "Word-wrap Quoted Text"
			         , cfg.xedit[i]->misc & QUOTEWRAP ? "Yes":"No", str);
			snprintf(opt[k++], MAX_OPLN, "%-32s%s", "Retain Ctrl-A Codes in Quotes"
			         , cfg.xedit[i]->misc & KEEP_CTRL_A ? "Yes":"No");
			snprintf(opt[k++], MAX_OPLN, "%-32s%s", "Automatically Quoted Text"
			         , cfg.xedit[i]->misc & QUOTEALL ? "All":cfg.xedit[i]->misc & QUOTENONE
			        ? "None" : "Prompt User");
			SAFECOPY(str, cfg.xedit[i]->misc & QUICKBBS ? "MSGINF/MSGTMP ": "EDITOR.INF/RESULT.ED");
			if (cfg.xedit[i]->misc & XTRN_LWRCASE)
				strlwr(str);
			snprintf(opt[k++], MAX_OPLN, "%-32s%s %s", "Editor Information Files"
			         , cfg.xedit[i]->misc & QUICKBBS ? "QuickBBS":"WWIV", str);
			const char* p;
			if (cfg.xedit[i]->misc & XTRN_UTF8) {
				p = "N/A";
			} else {
				switch (cfg.xedit[i]->soft_cr) {
					case XEDIT_SOFT_CR_EXPAND:
						p = "Convert to CRLF";
						break;
					case XEDIT_SOFT_CR_STRIP:
						p = "Strip";
						break;
					case XEDIT_SOFT_CR_RETAIN:
						p = "Retain";
						break;
					default:
					case XEDIT_SOFT_CR_UNDEFINED:
						p = "Unspecified";
						break;
				}
			}
			snprintf(opt[k++], MAX_OPLN, "%-32s%s", "Handle Soft CRs", p);
			snprintf(opt[k++], MAX_OPLN, "%-32s%s", "Strip FidoNet Kludges"
			         , cfg.xedit[i]->misc & STRIPKLUDGE ? "Yes":"No");
			snprintf(opt[k++], MAX_OPLN, "%-32s%s", "Support UTF-8 Encoding"
			         , cfg.xedit[i]->misc & XTRN_UTF8 ? "Yes":"No");
			snprintf(opt[k++], MAX_OPLN, "%-32s%s", "BBS Drop File Type"
			         , dropfile(cfg.xedit[i]->type, cfg.xedit[i]->misc));
			opt[k][0] = 0;
			uifc.helpbuf =
				"`Message Editor Configuration:`\n"
				"\n"
				"This menu allows you to change the settings for the selected external\n"
				"message editor. External message editors are very common on BBSs. Some\n"
				"popular editors include `fseditor.js`, `SyncEdit`, `SlyEdit`, `WWIVedit`, `FEdit`,\n"
				"`GEdit`, `IceEdit`, and many others.\n"
			;

			SAFEPRINTF(str, "%s Editor", cfg.xedit[i]->name);
			uifc_winmode_t wmode = WIN_SAV | WIN_ACT | WIN_L2R | WIN_BOT | WIN_EXTKEYS;
			if (i > 0)
				wmode |= WIN_LEFTKEY;
			if (i + 1 < cfg.total_xedits)
				wmode |= WIN_RIGHTKEY;
			switch (uifc.list(wmode, 0, 0, 70, &dfltopt, 0
			                  , str, opt)) {
				case -1:
					done = 1;
					break;
				case -CIO_KEY_LEFT - 2:
					if (i > 0)
						i--;
					break;
				case -CIO_KEY_RIGHT - 2:
					if (i + 1 < cfg.total_xedits)
						i++;
					break;
				case 0:
					uifc.helpbuf =
						"`Message Editor Name:`\n"
						"\n"
						"This is the name or description of the message editor.\n"
					;
					SAFECOPY(str, cfg.xedit[i]->name);
					if (!uifc.input(WIN_MID | WIN_SAV, 0, 10, "Message Editor Name"
					                , cfg.xedit[i]->name, sizeof(cfg.xedit[i]->name) - 1, K_EDIT))
						SAFECOPY(cfg.xedit[i]->name, str);
					break;
				case 1:
					uifc.helpbuf =
						"`WARNING:`\n"
						"\n"
						"If you change an external editor's internal code, any users that have\n"
						"chosen that editor as their default, will be reverted to the internal\n"
						"message editor as their default preference (that they can then change).";
					if (uifc.deny("Changing an editor's internal code is not recommended. Edit anyway?"))
						break;
					SAFECOPY(str, cfg.xedit[i]->code);
					uifc.helpbuf =
						"`Message Editor Internal Code:`\n"
						"\n"
						"Every message editor must have its own unique internal code for\n"
						"Synchronet to reference it by.  It is helpful if this code is an\n"
						"abbreviation of the name.\n"
					;
					uifc.input(WIN_MID | WIN_SAV, 0, 17, "Internal Code (unique)"
					           , str, LEN_CODE, K_EDIT | K_UPPER | K_NOSPACE);
					if (code_ok(str))
						SAFECOPY(cfg.xedit[i]->code, str);
					else {
						uifc.helpbuf = invalid_code;
						uifc.msg(strInvalidCode);
						uifc.helpbuf = NULL;
					}
					break;
				case 2:
					uifc.helpbuf =
						"`Message Editor Command Line:`\n"
						"\n"
						"This is the command line to execute when using this editor.\n"
						SCFG_CMDLINE_PREFIX_HELP
						SCFG_CMDLINE_SPEC_HELP
					;
					uifc.input(WIN_MID | WIN_SAV, 0, 10, "Command"
					           , cfg.xedit[i]->rcmd, sizeof(cfg.xedit[i]->rcmd) - 1, K_EDIT);
					break;
				case 3:
					sprintf(str, "%s Message Editor", cfg.xedit[i]->name);
					getar(str, cfg.xedit[i]->arstr);
					break;
				case 4:
					toggle_flag(native_opt, &cfg.xedit[i]->misc, XTRN_NATIVE, false, native_help);
					break;
				case 5:
					choose_io_method(&cfg.xedit[i]->misc);
					break;
				case 6:
					toggle_flag(use_shell_prompt, &cfg.xedit[i]->misc, XTRN_SH, false, use_shell_help);
					break;
				case 7:
					toggle_flag("Record Terminal Width"
					            , &cfg.xedit[i]->misc, SAVECOLUMNS, false,
					            "`Record Terminal Width:`\n"
					            "\n"
					            "When set to `Yes`, Synchronet will store the current terminal width\n"
					            "(in columns) in the header of messages created with this editor and use\n"
					            "the saved value to nicely re-word-wrap the message text when displaying\n"
					            "or quoting the message for other users with different terminal sizes.\n"
					            "\n"
					            "If this editor correctly detects and supports terminal widths `other`\n"
					            "`than 80 columns`, set this option to `Yes`."
					            );
					break;
				case 8:
					k = (cfg.xedit[i]->misc & QUOTEWRAP) ? 0:1;
					uifc.helpbuf =
						"`Word-wrap Quoted Text:`\n"
						"\n"
						"Set to `Yes` to have Synchronet word-wrap quoted message text when\n"
						"creating the quote file (e.g. QUOTES.TXT) or initial message text file\n"
						"(e.g. MSGTMP) used by some external message editors.\n"
						"\n"
						"When set to `No`, the original unmodified message text is written to the\n"
						"quote / message text file."
					;
					switch (uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &k, 0
					                  , "Word-wrap Quoted Text", uifcYesNoOpts)) {
						case 0:
							if (!(cfg.xedit[i]->misc & QUOTEWRAP)) {
								cfg.xedit[i]->misc |= QUOTEWRAP;
								uifc.changes = TRUE;
							}
							SAFEPRINTF(str, "%u", (uint)cfg.xedit[i]->quotewrap_cols);
							uifc.helpbuf =
								"`Screen width to wrap to:`\n"
								"\n"
								"Set to `0` to wrap the quoted text suiting the user's terminal width.\n"
								"Set to `79` to wrap the quoted text suiting an 80 column terminal.\n"
								"Set to `9999` to unwrap quoted text to long-line paragraphs.\n"
							;
							if (uifc.input(WIN_MID | WIN_SAV, 0, 0
							               , "Screen width to wrap to (0 = current terminal width)"
							               , str, 4, K_NUMBER | K_EDIT) > 0) {
								cfg.xedit[i]->quotewrap_cols = atoi(str);
								uifc.changes = TRUE;
							}
							break;
						case 1:
							if (cfg.xedit[i]->misc & QUOTEWRAP) {
								cfg.xedit[i]->misc &= ~QUOTEWRAP;
								uifc.changes = TRUE;
							}
							break;
					}
					break;
				case 9:
					toggle_flag("Retain Ctrl-A Codes in Quoted Text", &cfg.xedit[i]->misc, KEEP_CTRL_A, false
						, "`Retain Ctrl-A Codes in Quoted Text:`\n"
						"\n"
						"If this editor supports Ctrl-A codes in the generated `QUOTES.TXT` file,\n"
						"set this option to `Yes`.\n");
					break;
				case 10:
					switch (cfg.xedit[i]->misc & (QUOTEALL | QUOTENONE)) {
						case 0:     /* prompt user */
							k = 2;
							break;
						case QUOTENONE:
							k = 1;
							break;
						default:    /* all */
							k = 0;
							break;
					}
					strcpy(opt[0], "All");
					strcpy(opt[1], "None");
					strcpy(opt[2], "Prompt User");
					opt[3][0] = 0;
					uifc.helpbuf =
						"`Automatically Quoted Text:`\n"
						"\n"
						"If you want all the message text to be automatically entered into the\n"
						"message input file (e.g. `INPUT.MSG` or `MSGTMP`), select `All`.\n"
						"\n"
						"If you want the user to be prompted for which lines to quote before\n"
						"running the editor, select `Prompt User`.\n"
						"\n"
						"If you want none of the lines to be automatically quoted, select `None`.\n"
						"This option is mainly for use with editors that support the `QUOTES.TXT`\n"
						"drop file (like `SyncEdit v2.x`).\n"
					;
					k = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &k, 0, "Automatically Quoted Text"
					              , opt);
					if (!k && !(cfg.xedit[i]->misc & QUOTEALL)) {
						cfg.xedit[i]->misc |= QUOTEALL;
						cfg.xedit[i]->misc &= ~QUOTENONE;
						uifc.changes = TRUE;
					}
					else if (k == 1 && !(cfg.xedit[i]->misc & QUOTENONE)) {
						cfg.xedit[i]->misc |= QUOTENONE;
						cfg.xedit[i]->misc &= ~QUOTEALL;
						uifc.changes = TRUE;
					}
					else if (k == 2 && cfg.xedit[i]->misc & (QUOTENONE | QUOTEALL)) {
						cfg.xedit[i]->misc &= ~(QUOTENONE | QUOTEALL);
						uifc.changes = TRUE;
					}
					break;
				case 11:
					k = cfg.xedit[i]->misc & QUICKBBS ? 0:1;
					strcpy(opt[0], "QuickBBS MSGINF/MSGTMP");
					strcpy(opt[1], "WWIV EDITOR.INF/RESULT.ED");
					opt[2][0] = 0;
					uifc.helpbuf =
						"`Editor Information File:`\n"
						"\n"
						"If this message editor uses the QuickBBS style MSGTMP interface, set\n"
						"this option to ~QuickBBS MSGINF/MSGTMP~, otherwise set to ~WWIV EDITOR.INF/RESULT.ED~.\n"
					;
					k = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &k, 0, "Editor Information Files"
					              , opt);
					if (k == -1)
						break;
					if (!k && !(cfg.xedit[i]->misc & QUICKBBS)) {
						cfg.xedit[i]->misc |= QUICKBBS;
						uifc.changes = TRUE;
					}
					else if (k == 1 && (cfg.xedit[i]->misc & QUICKBBS)) {
						cfg.xedit[i]->misc &= ~QUICKBBS;
						uifc.changes = TRUE;
					}
					goto lowercase_filename;
					break;
				case 12:
					if (cfg.xedit[i]->misc & XTRN_UTF8)
						break; // N/A
					k = cfg.xedit[i]->soft_cr;
					strcpy(opt[0], "Unspecified");
					strcpy(opt[1], "Convert to CRLF");
					strcpy(opt[2], "Strip (Remove)");
					strcpy(opt[3], "Retain (Leave in)");
					opt[4][0] = 0;
					uifc.helpbuf =
						"`Handle Soft Carriage Returns:`\n"
						"\n"
						"This setting determines what is to be done with so-called \"Soft\" CR\n"
						"(Carriage Return) characters that are added to the message text by\n"
						"this message editor.\n"
						"\n"
						"Soft-CRs are defined in FidoNet specifications as 8Dh or ASCII 141 and\n"
						"were used historically to indicate an automatic line-wrap performed by\n"
						"the message editor.\n"
						"\n"
						"The supported settings for this option are:\n"
						"\n"
						"    `Convert` - to change Soft-CRs to the more universal CRLF (Hard-CR)\n"
						"    `Strip`   - to store long line paragraphs in the message bases\n"
						"    `Retain`  - to treat 8Dh characters like any other printable char\n"
					;
					k = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &k, 0, "Handle Soft Carriage Returns", opt);
					if (k >= 0 &&  k != cfg.xedit[i]->soft_cr) {
						cfg.xedit[i]->soft_cr = k;
						uifc.changes = TRUE;
					}
					break;
				case 13:
					toggle_flag("Strip FidoNet Kludge Lines"
					            , &cfg.xedit[i]->misc, STRIPKLUDGE, false,
					            "`Strip FidoNet Kludge Lines From Messages:`\n"
					            "\n"
					            "If this message editor adds FidoNet Kludge lines to the message text,\n"
					            "set this option to `Yes` to strip those lines from the message.\n"
					            );
					break;
				case 14:
					toggle_flag("Support UTF-8 Encoding"
					            , &cfg.xedit[i]->misc, XTRN_UTF8, false,
					            "`Support UTF-8 Encoding:`\n"
					            "\n"
					            "If this editor can handle UTF-8 encoded message text and header fields\n"
					            "and can detect and support UTF-8 terminal input and output, set this\n"
					            "option to `Yes`."
					            );
					break;
				case 15:
					k = 0;
					strcpy(opt[k++], "None");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "Synchronet", "XTRN.DAT");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "WWIV", "CHAIN.TXT");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "GAP (extended)", "DOOR.SYS (52 lines)");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "RBBS/QuickBBS", "DORINFO#.DEF");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "Wildcat", "CALLINFO.BBS");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "PCBoard", "PCBOARD.SYS");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "SpitFire", "SFDOORS.DAT");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "GAP (original)", "DOOR.SYS (31 lines)");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "Solar Realms", "DOORFILE.SR");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "RBBS/QuickBBS", "DORINFO1.DEF");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "TriBBS", "TRIBBS.SYS");
					snprintf(opt[k++], MAX_OPLN, "%-15s %s", "Mystic", "DOOR32.SYS");
					opt[k][0] = 0;
					k = cfg.xedit[i]->type;
					uifc.helpbuf =
						"`External Program BBS Drop File Type:`\n"
						"\n"
						"If this message editor requires a specific BBS data (drop) file\n"
						"format, select the file format from the list.\n"
					;
					k = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &k, 0
					              , "BBS Drop File Type", opt);
					if (k == -1)
						break;
					if (cfg.xedit[i]->type != k) {
						cfg.xedit[i]->type = k;
						if (cfg.xedit[i]->type == XTRN_DOOR32)
							cfg.xedit[i]->misc |= XTRN_NATIVE;
						uifc.changes = TRUE;
					}
					if (cfg.xedit[i]->type) {
lowercase_filename:
						k = (cfg.xedit[i]->misc & XTRN_LWRCASE) ? 0:1;
						k = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &k, 0, "Lowercase Filename", uifcYesNoOpts);
						if (k == 0 && !(cfg.xedit[i]->misc & XTRN_LWRCASE)) {
							cfg.xedit[i]->misc |= XTRN_LWRCASE;
							uifc.changes = TRUE;
						}
						else if (k == 1 && (cfg.xedit[i]->misc & XTRN_LWRCASE)) {
							cfg.xedit[i]->misc &= ~XTRN_LWRCASE;
							uifc.changes = TRUE;
						}
					}
					break;
			}
		}
	}
}

int natvpgm_cfg()
{
	static int dflt, bar;
	char       str[81];
	int        j;
	int        i, u;
#define NATIVE_PROGRAM_NAME_NOTE    "\n`Note:` If a file extension (e.g. `.exe`) is included in the filename,\n" \
		"      all command-lines must also include the file extension."
	char*      native_program_name_help =
		"`Native Program Name:`\n"
		"\n"
		"This is the executable filename of the Native external program.\n"
		NATIVE_PROGRAM_NAME_NOTE
	;

	while (1) {
		for (i = 0; i < MAX_OPTS && i < cfg.total_natvpgms; i++)
			snprintf(opt[i], MAX_OPLN, "%-12s", cfg.natvpgm[i]->name);
		opt[i][0] = 0;
		j = WIN_ACT | WIN_CHE | WIN_L2R | WIN_SAV;
		if (cfg.total_natvpgms)
			j |= WIN_DEL;
		if (cfg.total_natvpgms < MAX_OPTS)
			j |= WIN_INS | WIN_INSACT | WIN_XTR;
		uifc.helpbuf =
			"`Native Program List:`\n"
			"\n"
			"This is a list of all native (non-DOS) external program names that\n"
			"may be executed in the Terminal Server.  Any programs `not` listed\n"
			"here will be assumed to be DOS programs (unless otherwise flagged as\n"
			"'`Native`') and executed accordingly, or not, depending on the system.\n"
			"\n"
			"Use ~ INS ~ and ~ DELETE ~ to add and remove native program names.\n"
			"\n"
			"To change the filename of a program, hit ~ ENTER ~.\n"
			NATIVE_PROGRAM_NAME_NOTE
		;
		i = uifc.list(j, 0, 0, 30, &dflt, &bar, "Native Program List", opt);
		if ((signed)i == -1)
			break;
		int msk = i & MSK_ON;
		i &= MSK_OFF;
		if (msk == MSK_INS) {
			uifc.helpbuf = native_program_name_help;
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Native Program Name", str, 12, K_NONE) < 1)
				continue;
			if ((cfg.natvpgm = (natvpgm_t **)realloc_or_free(cfg.natvpgm
			        , sizeof(natvpgm_t *) * (cfg.total_natvpgms + 1))) == NULL) {
				errormsg(WHERE, ERR_ALLOC, nulstr, cfg.total_natvpgms + 1);
				cfg.total_natvpgms = 0;
				bail(1);
				continue;
			}
			if (cfg.total_natvpgms)
				for (u = cfg.total_natvpgms; u > i; u--)
					cfg.natvpgm[u] = cfg.natvpgm[u - 1];
			if ((cfg.natvpgm[i] = (natvpgm_t *)malloc(sizeof(natvpgm_t))) == NULL) {
				errormsg(WHERE, ERR_ALLOC, nulstr, sizeof(natvpgm_t));
				continue;
			}
			memset((natvpgm_t *)cfg.natvpgm[i], 0, sizeof(natvpgm_t));
			SAFECOPY(cfg.natvpgm[i]->name, str);
			cfg.total_natvpgms++;
			uifc.changes = TRUE;
			continue;
		}
		if (msk == MSK_DEL) {
			free(cfg.natvpgm[i]);
			cfg.total_natvpgms--;
			for (j = i; j < cfg.total_natvpgms; j++)
				cfg.natvpgm[j] = cfg.natvpgm[j + 1];
			uifc.changes = TRUE;
			continue;
		}
		if (msk != 0)
			continue;
		uifc.helpbuf = native_program_name_help;
		SAFECOPY(str, cfg.natvpgm[i]->name);
		if (uifc.input(WIN_MID | WIN_SAV, 0, 5, "Native Program Name", str, 12
		               , K_EDIT) > 0)
			SAFECOPY(cfg.natvpgm[i]->name, str);
	}
	return 0;
}

static int progs_in_sec(int sec)
{
	int total = 0;

	for (int i = 0; i < cfg.total_xtrns; ++i)
		if (cfg.xtrn[i]->sec == sec)
			++total;
	return total;
}

void xtrnsec_cfg()
{
	static int       xtrnsec_dflt, xtrnsec_bar, xtrnsec_opt;
	char             str[128], code[128], done = 0;
	int              j, k;
	int              i;
	static xtrnsec_t savxtrnsec;

	while (1) {
		for (i = 0; i < cfg.total_xtrnsecs && i < MAX_OPTS; i++)
			snprintf(opt[i], MAX_OPLN, "%-*s %5u", (int)sizeof(cfg.xtrn[i]->name) - 1, cfg.xtrnsec[i]->name, progs_in_sec(i));
		opt[i][0] = 0;
		j = WIN_SAV | WIN_ACT | WIN_CHE | WIN_BOT;
		if (cfg.total_xtrnsecs)
			j |= WIN_DEL | WIN_COPY | WIN_CUT;
		if (cfg.total_xtrnsecs < MAX_OPTS)
			j |= WIN_INS | WIN_INSACT | WIN_XTR;
		if (savxtrnsec.name[0])
			j |= WIN_PASTE | WIN_PASTEXTR;
		uifc.helpbuf =
			"`Online Program Sections:`\n"
			"\n"
			"This is a list of `Online Program Sections` configured for your system.\n"
			"\n"
			"To add an online program section, select the desired location with the\n"
			"arrow keys and hit ~ INS ~.\n"
			"\n"
			"To delete an online program section, select it and hit ~ DEL ~.\n"
			"\n"
			"To configure an online program section, select it and hit ~ ENTER ~.\n"
		;
		i = uifc.list(j, 0, 0, 0, &xtrnsec_dflt, &xtrnsec_bar, "Online Program Sections               Programs", opt);
		if ((signed)i == -1)
			return;
		int msk = i & MSK_ON;
		i &= MSK_OFF;
		int xtrnsec_num = i;
		if (msk == MSK_INS) {
			uifc.helpbuf =
				"`Online Program Section Name:`\n"
				"\n"
				"This is the name of this section.\n"
			;
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Online Program Section Name", str, 40, K_NONE) < 1)
				continue;
			SAFECOPY(code, str);
			prep_code(code, /* prefix: */ NULL);
			uifc.helpbuf =
				"`Online Program Section Internal Code:`\n"
				"\n"
				"Every online program section must have its own unique internal code\n"
				"for Synchronet to reference it by.  It is helpful if this code is an\n"
				"abbreviation of the name.\n"
			;
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Online Program Section Internal Code"
			               , code, LEN_CODE, K_EDIT | K_UPPER | K_NOSPACE) < 1)
				continue;
			if (xtrnsec_is_valid(&cfg, getxtrnsec(&cfg, code))) {
				uifc.msg(strDuplicateCode);
				continue;
			}
			if (!code_ok(code)) {
				uifc.helpbuf = invalid_code;
				uifc.msg(strInvalidCode);
				uifc.helpbuf = NULL;
				continue;
			}
			if (!new_external_program_section(xtrnsec_num))
				continue;
			SAFECOPY(cfg.xtrnsec[xtrnsec_num]->name, str);
			SAFECOPY(cfg.xtrnsec[xtrnsec_num]->code, code);
			uifc.changes = TRUE;
			continue;
		}
		if (msk == MSK_DEL || msk == MSK_CUT) {
			if (msk == MSK_CUT)
				savxtrnsec = *cfg.xtrnsec[xtrnsec_num];
			free(cfg.xtrnsec[xtrnsec_num]);
			if (msk == MSK_DEL) {
				for (j = 0; j < cfg.total_xtrns;) {
					if (cfg.xtrn[j]->sec == xtrnsec_num) {   /* delete xtrns of this group */
						free(cfg.xtrn[j]);
						cfg.total_xtrns--;
						k = j;
						while (k < cfg.total_xtrns) {    /* move all xtrns down */
							cfg.xtrn[k] = cfg.xtrn[k + 1];
							k++;
						}
					}
					else
						j++;
				}
				for (j = 0; j < cfg.total_xtrns; j++)    /* move xtrn group numbers down */
					if (cfg.xtrn[j]->sec > xtrnsec_num)
						cfg.xtrn[j]->sec--;
			}
			else { /* CUT */
				for (j = 0; j < cfg.total_xtrns; j++) {
					if (cfg.xtrn[j]->sec > xtrnsec_num)
						cfg.xtrn[j]->sec--;
					else if (cfg.xtrn[j]->sec == xtrnsec_num)
						cfg.xtrn[j]->sec = CUT_XTRNSEC_NUM;
				}
			}
			cfg.total_xtrnsecs--;
			for (i = xtrnsec_num; i < cfg.total_xtrnsecs; i++)
				cfg.xtrnsec[i] = cfg.xtrnsec[i + 1];
			uifc.changes = TRUE;
			continue;
		}
		if (msk == MSK_COPY) {
			savxtrnsec = *cfg.xtrnsec[xtrnsec_num];
			continue;
		}
		if (msk == MSK_PASTE) {
			if (!new_external_program_section(xtrnsec_num))
				continue;
			/* Restore previously cut xtrns to newly-pasted xtrn_sec */
			for (int u = 0; u < cfg.total_xtrns; u++)
				if (cfg.xtrn[u]->sec == CUT_XTRNSEC_NUM)
					cfg.xtrn[u]->sec = xtrnsec_num;
			*cfg.xtrnsec[xtrnsec_num] = savxtrnsec;
			uifc.changes = TRUE;
			continue;
		}
		if (msk != 0)
			continue;
		done = 0;
		while (!done) {
			k = 0;
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Name", cfg.xtrnsec[i]->name);
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Internal Code", cfg.xtrnsec[i]->code);
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Access Requirements"
			         , cfg.xtrnsec[i]->arstr);
			snprintf(opt[k++], MAX_OPLN, "%s", "Online Programs...");
			opt[k][0] = 0;
			sprintf(str, "%s Program Section", cfg.xtrnsec[i]->name);
			uifc.helpbuf =
				"`Program Section Configuration:`\n"
				"\n"
				"This menu allows you to configure the security requirements for access\n"
				"to this external program section.  You can also add, delete, and\n"
				"configure the programs (e.g. door games) of this section by selecting\n"
				"the `Online Programs...` option.\n"
				"\n"
				"The left and right arrow keys may be used to cycle through program\n"
				"sections.\n"
			;
			uifc_winmode_t wmode = WIN_SAV | WIN_ACT | WIN_MID | WIN_EXTKEYS;
			if (i > 0)
				wmode |= WIN_LEFTKEY;
			if (i + 1 < cfg.total_xtrnsecs)
				wmode |= WIN_RIGHTKEY;
			switch (uifc.list(wmode, 0, 0, 60, &xtrnsec_opt, 0, str
			                  , opt)) {
				case -1:
					done = 1;
					break;
				case -CIO_KEY_LEFT - 2:
					if (i > 0)
						i--;
					break;
				case -CIO_KEY_RIGHT - 2:
					if (i + 1 < cfg.total_xtrnsecs)
						i++;
					break;
				case 0:
					uifc.helpbuf =
						"`Online Program Section Name:`\n"
						"\n"
						"This is the name of this section.\n"
					;
					SAFECOPY(str, cfg.xtrnsec[i]->name);  /* save */
					if (!uifc.input(WIN_MID | WIN_SAV, 0, 10
					                , "Program Section Name"
					                , cfg.xtrnsec[i]->name, sizeof(cfg.xtrnsec[i]->name) - 1, K_EDIT))
						SAFECOPY(cfg.xtrnsec[i]->name, str);
					break;
				case 1:
					SAFECOPY(str, cfg.xtrnsec[i]->code);
					uifc.helpbuf =
						"`Online Program Section Internal Code:`\n"
						"\n"
						"Every online program section must have its own unique internal code\n"
						"for Synchronet to reference it by.  It is helpful if this code is an\n"
						"abbreviation of the name.\n"
					;
					if (uifc.input(WIN_MID | WIN_SAV, 0, 17, "Internal Code (unique)"
					           , str, LEN_CODE, K_EDIT | K_UPPER | K_NOSPACE | K_CHANGED | K_FIND) < 1)
							break;
					if (xtrnsec_is_valid(&cfg, getxtrnsec(&cfg, str))) {
						uifc.msg(strDuplicateCode);
						break;
					}
					if (code_ok(str)) {
						SAFECOPY(cfg.xtrnsec[i]->code, str);
						uifc.changes = TRUE;
					}
					else {
						uifc.helpbuf = invalid_code;
						uifc.msg(strInvalidCode);
						uifc.helpbuf = NULL;
					}
					break;
				case 2:
					getar(cfg.xtrnsec[i]->name, cfg.xtrnsec[i]->arstr);
					break;
				case 3:
					xtrn_cfg(i);
					break;
			}
		}
	}
}

void hotkey_cfg(void)
{
	static int      dflt, dfltopt, bar;
	char            str[81], done = 0;
	int             j, k;
	int             i, u;
	static hotkey_t savhotkey;

	while (1) {
		for (i = 0; i < cfg.total_hotkeys && i < MAX_OPTS; i++)
			snprintf(opt[i], MAX_OPLN, "Ctrl-%c    %s"
			         , cfg.hotkey[i]->key + '@'
			         , cfg.hotkey[i]->cmd);
		opt[i][0] = 0;
		j = WIN_SAV | WIN_ACT | WIN_CHE | WIN_RHT;
		if (cfg.total_hotkeys)
			j |= WIN_DEL | WIN_COPY | WIN_CUT;
		if (cfg.total_hotkeys < MAX_OPTS)
			j |= WIN_INS | WIN_INSACT | WIN_XTR;
		if (savhotkey.cmd[0])
			j |= WIN_PASTE;
		uifc.helpbuf =
			"`Global Hot Key Events:`\n"
			"\n"
			"This is a list of programs or loadable modules that can be executed by\n"
			"anyone on the BBS at any time (while the BBS has control of user input).\n"
			"\n"
			"To add a hot key event, select the desired location and hit ~ INS ~.\n"
			"\n"
			"To delete a hot key event, select it and hit ~ DEL ~.\n"
			"\n"
			"To configure a hot key event, select it and hit ~ ENTER ~.\n"
		;
		i = uifc.list(j, 0, 0, 45, &dflt, &bar, "Global Hot Key Events", opt);
		if ((signed)i == -1)
			return;
		int msk = i & MSK_ON;
		i &= MSK_OFF;
		if (msk == MSK_INS) {
			uifc.helpbuf =
				"`Global Hot Key:`\n"
				"\n"
				"This is the control key used to trigger the hot key event. Example, A\n"
				"indicates a Ctrl-A hot key event.\n"
			;
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Control Key", str, 1, K_UPPER | K_NOSPACE) < 1)
				continue;

			if ((cfg.hotkey = (hotkey_t **)realloc_or_free(cfg.hotkey
				    , sizeof(hotkey_t *) * (cfg.total_hotkeys + 1))) == NULL) {
				errormsg(WHERE, ERR_ALLOC, nulstr, cfg.total_hotkeys + 1);
				cfg.total_hotkeys = 0;
				bail(1);
				continue;
			}
			if (cfg.total_hotkeys)
				for (u = cfg.total_hotkeys; u > i; u--)
					cfg.hotkey[u] = cfg.hotkey[u - 1];
			if ((cfg.hotkey[i] = (hotkey_t *)malloc(sizeof(hotkey_t))) == NULL) {
				errormsg(WHERE, ERR_ALLOC, nulstr, sizeof(hotkey_t));
				continue;
			}
			memset((hotkey_t *)cfg.hotkey[i], 0, sizeof(hotkey_t));
			cfg.hotkey[i]->key = str[0] - '@';
			cfg.total_hotkeys++;
			uifc.changes = TRUE;
			continue;
		}
		if (msk == MSK_DEL || msk == MSK_CUT) {
			if (msk == MSK_CUT)
				savhotkey = *cfg.hotkey[i];
			free(cfg.hotkey[i]);
			cfg.total_hotkeys--;
			for (j = i; j < cfg.total_hotkeys; j++)
				cfg.hotkey[j] = cfg.hotkey[j + 1];
			uifc.changes = TRUE;
			continue;
		}
		if (msk == MSK_COPY) {
			savhotkey = *cfg.hotkey[i];
			continue;
		}
		if (msk == MSK_PASTE) {
			*cfg.hotkey[i] = savhotkey;
			uifc.changes = TRUE;
			continue;
		}
		if (msk != 0)
			continue;
		done = 0;
		while (!done) {
			k = 0;
			snprintf(opt[k++], MAX_OPLN, "%-27.27sCtrl-%c", "Global Hot Key"
			         , cfg.hotkey[i]->key + '@');
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Command Line", cfg.hotkey[i]->cmd);
			opt[k][0] = 0;
			uifc.helpbuf =
				"`Global Hot Key Event:`\n"
				"\n"
				"This menu allows you to change the settings for the selected global\n"
				"hot key event.  Hot key events are control characters that are used to\n"
				"execute an external program or module anywhere in the BBS.\n"
			;
			sprintf(str, "Ctrl-%c Hot Key Event", cfg.hotkey[i]->key + '@');
			uifc_winmode_t wmode = WIN_SAV | WIN_ACT | WIN_L2R | WIN_BOT | WIN_EXTKEYS;
			if (i > 0)
				wmode |= WIN_LEFTKEY;
			if (i + 1 < cfg.total_hotkeys)
				wmode |= WIN_RIGHTKEY;
			switch (uifc.list(wmode, 0, 0, 60, &dfltopt, 0
			                  , str, opt)) {
				case -1:
					done = 1;
					break;
				case -CIO_KEY_LEFT - 2:
					if (i > 0)
						i--;
					break;
				case -CIO_KEY_RIGHT - 2:
					if (i + 1 < cfg.total_hotkeys)
						i++;
					break;
				case 0:
					uifc.helpbuf =
						"`Global Hot-Ctrl Key:`\n"
						"\n"
						"This is the global control key used to execute this event.\n"
					;
					sprintf(str, "%c", cfg.hotkey[i]->key + '@');
					if (uifc.input(WIN_MID | WIN_SAV, 0, 10, "Global Hot Ctrl-Key"
					               , str, 1, K_EDIT | K_UPPER | K_NOSPACE) > 0)
						cfg.hotkey[i]->key = str[0] - '@';
					break;
				case 1:
					uifc.helpbuf =
						"`Hot Key Event Command Line:`\n"
						"\n"
						"This is the command line to execute when this hot key is pressed.\n"
						SCFG_CMDLINE_PREFIX_HELP
						SCFG_CMDLINE_SPEC_HELP
					;
					uifc.input(WIN_MID | WIN_SAV, 0, 10, "Command"
					           , cfg.hotkey[i]->cmd, sizeof(cfg.hotkey[i]->cmd) - 1, K_EDIT);
					break;
			}
		}
	}
}
