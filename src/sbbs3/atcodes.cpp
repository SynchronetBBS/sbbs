/* Synchronet "@code" functions */

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
#include "utf8.h"
#include "unicode.h"
#include "cp437defs.h"
#include "ver.h"
#include "petdefs.h"
#include "filedat.h"

#if defined(_WINSOCKAPI_)
extern WSADATA WSAData;
	#define SOCKLIB_DESC WSAData.szDescription
#else
	#define SOCKLIB_DESC NULL
#endif

struct atcode_format {
	int disp_len;
	enum {
		none,
		left,
		right,
		center
	} align = none;
	bool zero_padded = false;
	bool truncated = true;
	bool doubled = false;
	bool thousep = false;       // thousands-separated
	bool uppercase = false;
	bool width_specified = false;
	char* parse(char* sp) {
		char* p;

		disp_len = strlen(sp) + 2;
		if ((p = strchr(sp, '|')) != NULL) {
			if (strchr(p, 'T') != NULL)
				thousep = true;
			if (strchr(p, 'U') != NULL)
				uppercase = true;
			if (strchr(p, 'L') != NULL)
				align = left;
			else if (strchr(p, 'R') != NULL)
				align = right;
			else if (strchr(p, 'C') != NULL)
				align = center;
			else if (strchr(p, 'W') != NULL)
				doubled = true;
			else if (strchr(p, 'Z') != NULL)
				zero_padded = true;
			else if (strchr(p, '>') != NULL)
				truncated = false;
		}
		else if (strchr(sp, ':') != NULL)
			p = NULL;
		else if ((p = strstr(sp, "-L")) != NULL)
			align = left;
		else if ((p = strstr(sp, "-R")) != NULL)
			align = right;
		else if ((p = strstr(sp, "-C")) != NULL)
			align = center;
		else if ((p = strstr(sp, "-W")) != NULL)  /* wide */
			doubled = true;
		else if ((p = strstr(sp, "-Z")) != NULL)
			zero_padded = true;
		else if ((p = strstr(sp, "-T")) != NULL)
			thousep = true;
		else if ((p = strstr(sp, "-U")) != NULL)
			uppercase = true;
		else if ((p = strstr(sp, "->")) != NULL)  /* wrap */
			truncated = false;
		if (p != NULL) {
			char* lp = p;
			lp++;   // skip the '|' or '-'
			while (*lp == '>' || IS_ALPHA(*lp))
				lp++;
			if (*lp)
				width_specified = true;
			while (*lp && !IS_DIGIT(*lp))
				lp++;
			if (*lp && IS_DIGIT(*lp)) {
				disp_len = atoi(lp);
				width_specified = true;
			}
			*p = 0;
		}

		return p;
	}
};

/****************************************************************************/
/* Returns 0 if invalid @ code. Returns length of @ code if valid.          */
/****************************************************************************/
int sbbs_t::show_atcode(const char *instr, JSObject* obj)
{
	char          str[128], str2[128], *tp, *sp, *p;
	int           len;
	atcode_format fmt;
	int           pmode = 0;
	const char *  cp;

	if (*instr != '@')
		return 0;
	SAFECOPY(str, instr + 1);
	tp = strchr(str, '@');
	if (tp == nullptr)                 /* no terminating @ */
		return 0;
	len = (tp - str) + 2;
	*tp = '\0';
	sp = str;
	if (IS_DIGIT(*sp)) // @-codes cannot start with a number
		return 0;
	if (strcspn(sp, " \t\r\n") != strlen(sp))  // white-space before terminating @
		return 0;

	if (*sp == '~' && *(sp + 1)) {   // Mouse hot-spot (hungry)
		sp++;
		tp = strchr(sp + 1, '~');
		if (tp == NULL)
			tp = sp;
		else {
			*tp = 0;
			tp++;
		}
		c_unescape_str(tp);
		term->add_hotspot(tp, /* hungry: */ true, term->column, term->column + strlen(sp) - 1, term->row);
		bputs(sp);
		return len;
	}

	if (*sp == '`' && *(sp + 1)) {   // Mouse hot-spot (strict)
		sp++;
		tp = strchr(sp + 1, '`');
		if (tp == NULL)
			tp = sp;
		else {
			*tp = 0;
			tp++;
		}
		c_unescape_str(tp);
		term->add_hotspot(tp, /* hungry: */ false, term->column, term->column + strlen(sp) - 1, term->row);
		bputs(sp);
		return len;
	}

	// @!x@ for Ctrl-A x equivalent(s) */
	if (*sp == '!') {
		for (p = sp + 1; *p != '\0' && *p != '@'; p++)
			ctrl_a(*p);
		return len;
	}

	p = fmt.parse(sp);

	cp = atcode(sp, str2, sizeof(str2), &pmode, fmt.align == fmt.center, obj);
	if (cp == NULL)
		return 0;

	char separated[128];
	if (fmt.thousep)
		cp = separate_thousands(cp, separated, sizeof(separated), ',');

	char upper[128];
	if (fmt.uppercase) {
		SAFECOPY(upper, cp);
		strupr(upper);
		cp = upper;
	}

	if (p == NULL || fmt.truncated == false || (fmt.width_specified == false && fmt.align == fmt.none))
		fmt.disp_len = strlen(cp);

	if (fmt.uppercase && fmt.align == fmt.none)
		fmt.align = fmt.left;

	if (fmt.truncated && strchr(cp, '\n') == NULL) {
		if (term->column + fmt.disp_len > term->cols - 1) {
			if (term->column >= term->cols - 1)
				fmt.disp_len = 0;
			else
				fmt.disp_len = (term->cols - 1) - term->column;
		}
	}
	if (pmode & P_UTF8) {
		if (term->charset() == CHARSET_UTF8)
			fmt.disp_len += strlen(cp) - utf8_str_total_width(cp, unicode_zerowidth);
		else
			fmt.disp_len += strlen(cp) - utf8_str_count_width(cp, /* min: */ 1, /* max: */ 2, unicode_zerowidth);
	}
	if (fmt.align == fmt.left)
		bprintf(pmode, "%-*.*s", fmt.disp_len, fmt.disp_len, cp);
	else if (fmt.align == fmt.right)
		bprintf(pmode, "%*.*s", fmt.disp_len, fmt.disp_len, cp);
	else if (fmt.align == fmt.center) {
		int vlen = strlen(cp);
		if (vlen < fmt.disp_len) {
			int left = (fmt.disp_len - vlen) / 2;
			bprintf(pmode, "%*s%-*s", left, "", fmt.disp_len - left, cp);
		} else
			bprintf(pmode, "%.*s", fmt.disp_len, cp);
	} else if (fmt.doubled) {
		wide(cp);
	} else if (fmt.zero_padded) {
		int vlen = strlen(cp);
		if (vlen < fmt.disp_len)
			bprintf(pmode, "%-.*s%s", (int)(fmt.disp_len - strlen(cp)), "0000000000", cp);
		else
			bprintf(pmode, "%.*s", fmt.disp_len, cp);
	} else
		bprintf(pmode, "%.*s", fmt.disp_len, cp);

	return len;
}

static const char* getpath(scfg_t* cfg, const char* path)
{
	for (int i = 0; i < cfg->total_dirs; i++) {
		if (stricmp(cfg->dir[i]->code, path) == 0)
			return cfg->dir[i]->path;
	}
	return path;
}

const char* sbbs_t::formatted_atcode(const char* sp, char* str, size_t maxlen)
{
	char          tmp[128];
	char          buf[256];
	atcode_format fmt;

	SAFECOPY(tmp, sp);
	char*         p = fmt.parse(tmp);

	const char*   cp = atcode(tmp, buf, sizeof buf);
	if (cp == nullptr)
		return nullptr;

	char          separated[128];
	if (fmt.thousep)
		cp = separate_thousands(cp, separated, sizeof(separated), ',');

	char          upper[128];
	if (fmt.uppercase) {
		SAFECOPY(upper, cp);
		strupr(upper);
		cp = upper;
	}

	if (p == NULL || fmt.truncated == false || (fmt.width_specified == false && fmt.align == fmt.none))
		fmt.disp_len = strlen(cp);

	if (fmt.align == fmt.left)
		snprintf(str, maxlen, "%-*.*s", fmt.disp_len, fmt.disp_len, cp);
	else if (fmt.align == fmt.right)
		snprintf(str, maxlen, "%*.*s", fmt.disp_len, fmt.disp_len, cp);
	else if (fmt.align == fmt.center) {
		int vlen = strlen(cp);
		if (vlen < fmt.disp_len) {
			int left = (fmt.disp_len - vlen) / 2;
			snprintf(str, maxlen, "%*s%-*s", left, "", fmt.disp_len - left, cp);
		} else
			snprintf(str, maxlen, "%.*s", fmt.disp_len, cp);
//	} else if(fmt.doubled) { // Unsupported in this context
//		wide(cp);
	} else if (fmt.zero_padded) {
		int vlen = strlen(cp);
		if (vlen < fmt.disp_len)
			snprintf(str, maxlen, "%-.*s%s", (int)(fmt.disp_len - strlen(cp)), "0000000000", cp);
		else
			snprintf(str, maxlen, "%.*s", fmt.disp_len, cp);
	} else
		snprintf(str, maxlen, "%.*s", fmt.disp_len, cp);

	return str;
}

const char* sbbs_t::atcode(const char* sp, char* str, size_t maxlen, int* pmode, bool centered, JSObject* obj)
{
	char       tmp[128];
	char*      tp = NULL;
	int        i;
	uint       ugrp;
	uint       usub;
	long       l;
	stats_t    stats;
	node_t     node;
	struct  tm tm;

	str[0] = 0;

	if (strcmp(sp, "SHOW") == 0) {
		console &= ~CON_ECHO_OFF;
		return nulstr;
	}
	if (strncmp(sp, "SHOW:", 5) == 0) {
		uchar* ar = arstr(NULL, sp + 5, &cfg, NULL);
		if (ar != NULL) {
			if (!chk_ar(ar, &useron, &client))
				console |= CON_ECHO_OFF;
			else
				console &= ~CON_ECHO_OFF;
			free(ar);
		}
		return nulstr;
	}

	if (strcmp(sp, "HOT") == 0) { // Auto-mouse hot-spot attribute
		hot_attr = curatr;
		return nulstr;
	}
	if (strncmp(sp, "HOT:", 4) == 0) {   // Auto-mouse hot-spot attribute
		sp += 4;
		if (stricmp(sp, "hungry") == 0) {
			hungry_hotspots = true;
			hot_attr = curatr;
		}
		else if (stricmp(sp, "strict") == 0) {
			hungry_hotspots = false;
			hot_attr = curatr;
		}
		else if (stricmp(sp, "off") == 0)
			hot_attr = 0;
		else
			hot_attr = strtoattr(sp, /* endptr: */ NULL);
		return nulstr;
	}
	if (strcmp(sp, "CLEAR_HOT") == 0) {
		term->clear_hotspots();
		return nulstr;
	}

	if (strncmp(sp, "MNE:", 4) == 0) {   // Mnemonic attribute control
		sp += 4;
		mneattr_low = strtoattr(sp, &tp);
		mneattr_high = mneattr_low ^ HIGH;
		if (tp != NULL && *tp != '\0')
			mneattr_high = strtoattr(tp + 1, &tp);
		if (tp != NULL && *tp != '\0')
			mneattr_cmd = strtoattr(tp + 1, NULL);
		return nulstr;
	}

	if (strncmp(sp, "RAINBOW:", 8) == 0) {
		memset(rainbow, 0, sizeof rainbow);
		parse_attr_str_list(rainbow, LEN_RAINBOW, sp + 8);
		return nulstr;
	}

	if (strcmp(sp, "AT") == 0)
		return "@";

	if (strncmp(sp, "U+", 2) == 0) { // UNICODE
		enum unicode_codepoint codepoint = (enum unicode_codepoint)strtoul(sp + 2, &tp, 16);
		if (tp == NULL || *tp == 0)
			outcp(codepoint, unicode_to_cp437(codepoint));
		else if (*tp == ':')
			outcp(codepoint, tp + 1);
		else {
			char fallback = (char)strtoul(tp + 1, NULL, 16);
			if (*tp == ',')
				outcp(codepoint, fallback);
			else if (*tp == '!') {
				char ch = unicode_to_cp437(codepoint);
				if (ch != 0)
					fallback = ch;
				outcp(codepoint, fallback);
			}
			else
				return NULL;  // Invalid @-code
		}
		return nulstr;
	}

	if (strcmp(sp, "CHECKMARK") == 0) {
		outcp(UNICODE_CHECK_MARK, CP437_CHECK_MARK);
		return nulstr;
	}

	if (strcmp(sp, "ELLIPSIS") == 0) {
		outcp(UNICODE_HORIZONTAL_ELLIPSIS, "...");
		return nulstr;
	}
	if (strcmp(sp, "COPY") == 0) {
		outcp(UNICODE_COPYRIGHT_SIGN, "(C)");
		return nulstr;
	}
	if (strcmp(sp, "SOUNDCOPY") == 0) {
		outcp(UNICODE_SOUND_RECORDING_COPYRIGHT, "(P)");
		return nulstr;
	}
	if (strcmp(sp, "REGISTERED") == 0) {
		outcp(UNICODE_REGISTERED_SIGN, "(R)");
		return nulstr;
	}
	if (strcmp(sp, "TRADEMARK") == 0) {
		outcp(UNICODE_TRADE_MARK_SIGN, "(TM)");
		return nulstr;
	}
	if (strcmp(sp, "DEGREE_C") == 0) {
		outcp(UNICODE_DEGREE_CELSIUS, "\xF8""C");
		return nulstr;
	}
	if (strcmp(sp, "DEGREE_F") == 0) {
		outcp(UNICODE_DEGREE_FAHRENHEIT, "\xF8""F");
		return nulstr;
	}

	if (strncmp(sp, "WIDE:", 5) == 0) {
		wide(sp + 5);
		return nulstr;
	}

	if (!strcmp(sp, "VER"))
		return VERSION;

	if (!strcmp(sp, "REV")) {
		safe_snprintf(str, maxlen, "%c", REVISION);
		return str;
	}

	if (!strcmp(sp, "FULL_VER")) {
		safe_snprintf(str, maxlen, "%s%c%s", VERSION, REVISION, beta_version);
		truncsp(str);
#if defined(_DEBUG)
		strcat(str, " Debug");
#endif
		return str;
	}

	if (!strcmp(sp, "VER_NOTICE"))
		return VERSION_NOTICE;

	if (!strcmp(sp, "OS_VER"))
		return os_version(str, maxlen);

	if (strcmp(sp, "OS_CPU") == 0)
		return os_cpuarch(str, maxlen);

#ifdef JAVASCRIPT
	if (!strcmp(sp, "JS_VER"))
		return (char *)JS_GetImplementationVersion();
#endif

	if (!strcmp(sp, "PLATFORM"))
		return PLATFORM_DESC;

	if (!strcmp(sp, "COPYRIGHT"))
		return COPYRIGHT_NOTICE;

	if (!strcmp(sp, "COMPILER")) {
		char compiler[32];
		DESCRIBE_COMPILER(compiler);
		strncpy(str, compiler, maxlen);
		return str;
	}

	if (strcmp(sp, "GIT_HASH") == 0)
		return git_hash;

	if (strcmp(sp, "GIT_BRANCH") == 0)
		return git_branch;

	if (strcmp(sp, "GIT_DATE") == 0)
		return git_date;

	if (strcmp(sp, "BUILD_DATE") == 0)
		return __DATE__;

	if (strcmp(sp, "BUILD_TIME") == 0)
		return __TIME__;

	if (!strcmp(sp, "UPTIME")) {
		extern volatile time_t uptime;
		time_t                 up = 0;
		now = time(NULL);
		if (uptime != 0 && now >= uptime)
			up = now - uptime;
		char                   days[64] = "";
		if ((up / (24 * 60 * 60)) >= 2) {
			snprintf(days, sizeof days, "%u days ", (uint)(up / (24L * 60L * 60L)));
			up %= (24 * 60 * 60);
		}
		safe_snprintf(str, maxlen, "%s%u:%02u"
		              , days
		              , (uint)(up / (60L * 60L))
		              , (uint)((up / 60L) % 60L)
		              );
		return str;
	}

	if (!strcmp(sp, "SERVED")) {
		extern volatile uint served;
		safe_snprintf(str, maxlen, "%u", served);
		return str;
	}

	if (!strcmp(sp, "SOCKET_LIB"))
		return socklib_version(str, maxlen, SOCKLIB_DESC);

	if (!strcmp(sp, "MSG_LIB")) {
		safe_snprintf(str, maxlen, "SMBLIB %s", smb_lib_ver());
		return str;
	}

	if (!strcmp(sp, "BBS") || !strcmp(sp, "BOARDNAME"))
		return cfg.sys_name;

	if (!strcmp(sp, "BAUD") || !strcmp(sp, "BPS")) {
		safe_snprintf(str, maxlen, "%u", term->cur_output_rate ? term->cur_output_rate : cur_rate);
		return str;
	}

	if (!strcmp(sp, "CPS")) {
		safe_snprintf(str, maxlen, "%u", cur_cps);
		return str;
	}

	if (!strcmp(sp, "COLS")) {
		safe_snprintf(str, maxlen, "%u", term->cols);
		return str;
	}
	if (!strcmp(sp, "ROWS")) {
		safe_snprintf(str, maxlen, "%u", term->rows);
		return str;
	}
	if (strcmp(sp, "TERM") == 0)
		return term_type();

	if (strcmp(sp, "CHARSET") == 0)
		return term->charset_str();

	if (!strcmp(sp, "CONN"))
		return connection;

	if (!strcmp(sp, "SYSOP"))
		return cfg.sys_op;

	if (strcmp(sp, "SYSAVAIL") == 0)
		return text[sysop_available(&cfg) ? LiSysopAvailable : LiSysopNotAvailable];

	if (strcmp(sp, "SYSAVAILYN") == 0)
		return text[sysop_available(&cfg) ? Yes : No];

	if (!strcmp(sp, "LOCATION"))
		return cfg.sys_location;

	if (strcmp(sp, "NODE") == 0 || strcmp(sp, "NN") == 0) {
		safe_snprintf(str, maxlen, "%u", cfg.node_num);
		return str;
	}
	if (strcmp(sp, "TNODES") == 0 || strcmp(sp, "TNODE") == 0 || strcmp(sp, "TN") == 0) {
		safe_snprintf(str, maxlen, "%u", cfg.sys_nodes);
		return str;
	}
	if (strcmp(sp, "ANODES") == 0 || strcmp(sp, "ANODE") == 0 || strcmp(sp, "AN") == 0) {
		safe_snprintf(str, maxlen, "%u", count_nodes(/* self: */ true));
		return str;
	}
	if (strcmp(sp, "ONODES") == 0 || strcmp(sp, "ONODE") == 0 || strcmp(sp, "ON") == 0) {
		safe_snprintf(str, maxlen, "%u", count_nodes(/* self: */ false));
		return str;
	}

	if (strcmp(sp, "PWDAYS") == 0) {
		if (cfg.sys_pwdays) {
			safe_snprintf(str, maxlen, "%u", cfg.sys_pwdays);
			return str;
		}
		return text[Unlimited];
	}

	if (strcmp(sp, "AUTODEL") == 0) {
		if (cfg.sys_autodel) {
			safe_snprintf(str, maxlen, "%u", cfg.sys_autodel);
			return str;
		}
		return text[Unlimited];
	}

	if (strcmp(sp, "PAGER") == 0)
		return (thisnode.misc & NODE_POFF) ? text[Off] : text[On];

	if (strcmp(sp, "ALERTS") == 0)
		return (thisnode.misc & NODE_AOFF) ? text[Off] : text[On];

	if (strcmp(sp, "SPLITP") == 0)
		return (useron.chat & CHAT_SPLITP) ? text[On] : text[Off];

	if (!strcmp(sp, "INETADDR"))
		return cfg.sys_inetaddr;

	if (!strcmp(sp, "HOSTNAME"))
		return server_host_name();

	if (!strcmp(sp, "FIDOADDR")) {
		if (cfg.total_faddrs)
			return smb_faddrtoa(&cfg.faddr[0], str);
		return nulstr;
	}

	if (!strcmp(sp, "EMAILADDR"))
		return usermailaddr(&cfg, str
		                    , (cfg.inetmail_misc & NMAIL_ALIAS) || (useron.rest & FLAG('O')) ? useron.alias : useron.name);

	if (strcmp(sp, "NETMAIL") == 0)
		return useron.netmail;

	if (strcmp(sp, "TERMTYPE") == 0)
		return term_type(&useron, term->flags(), str, maxlen);

	if (strcmp(sp, "TERMROWS") == 0)
		return term_rows(&useron, str, maxlen);

	if (strcmp(sp, "TERMCOLS") == 0)
		return term_cols(&useron, str, maxlen);

	if (strcmp(sp, "AUTOTERM") == 0)
		return (useron.misc & AUTOTERM) ? text[On] : text[Off];

	if (strcmp(sp, "ANSI") == 0)
		return (useron.misc & ANSI) ? text[On] : text[Off];

	if (strcmp(sp, "ASCII") == 0)
		return (useron.misc & NO_EXASCII) ? text[On] : text[Off];

	if (strcmp(sp, "COLOR") == 0)
		return (useron.misc & COLOR) ? text[On] : text[Off];

	if (strcmp(sp, "ICE") == 0)
		return (useron.misc & ICE_COLOR) ? text[On] : text[Off];

	if (strcmp(sp, "RIP") == 0)
		return (useron.misc & RIP) ? text[On] : text[Off];

	if (strcmp(sp, "PETSCII") == 0)
		return (useron.misc & PETSCII) ? text[On] : text[Off];

	if (strcmp(sp, "PETGRFX") == 0) {
		if (term->charset() == CHARSET_PETSCII)
			term_out(PETSCII_UPPERGRFX);
		return nulstr;
	}

	if (strcmp(sp, "SWAPDEL") == 0)
		return (useron.misc & SWAP_DELETE) ? text[On] : text[Off];

	if (strcmp(sp, "UTF8") == 0)
		return (useron.misc & UTF8) ? text[On] : text[Off];

	if (strcmp(sp, "MOUSE") == 0)
		return (useron.misc & MOUSE) ? text[On] : text[Off];

	if (strcmp(sp, "UPAUSE") == 0)
		return (useron.misc & UPAUSE) ? text[On] : text[Off];

	if (strcmp(sp, "SPIN") == 0)
		return (useron.misc & SPIN) ? text[On] : text[Off];

	if (strcmp(sp, "PAUSESPIN") == 0)
		return (useron.misc & NOPAUSESPIN) ? text[Off] : text[On];

	if (strcmp(sp, "EXPERT") == 0)
		return (useron.misc & EXPERT) ? text[On] : text[Off];

	if (strcmp(sp, "HOTKEYS") == 0)
		return (useron.misc & COLDKEYS) ? text[Off] : text[On];

	if (strcmp(sp, "MSGCLS") == 0)
		return (useron.misc & CLRSCRN) ? text[On] : text[Off];

	if (strcmp(sp, "FWD") == 0)
		return (useron.misc & NETMAIL) ? text[On] : text[Off];

	if (strcmp(sp, "REMSUBS") == 0)
		return (useron.misc & CURSUB) ? text[On] : text[Off];

	if (strcmp(sp, "FILEDESC") == 0)
		return (useron.misc & EXTDESC) ? text[On] : text[Off];

	if (strcmp(sp, "FILEFLAG") == 0)
		return (useron.misc & BATCHFLAG) ? text[On] : text[Off];

	if (strcmp(sp, "AUTOHANG") == 0)
		return (useron.misc & AUTOHANG) ? text[On] : text[Off];

	if (strcmp(sp, "AUTOLOGON") == 0)
		return (useron.misc & AUTOLOGON) ? text[On] : text[Off];

	if (strcmp(sp, "QUIET") == 0)
		return (useron.misc & QUIET) ? text[On] : text[Off];

	if (strcmp(sp, "ASKNSCAN") == 0)
		return (useron.misc & ASK_NSCAN) ? text[On] : text[Off];

	if (strcmp(sp, "ASKSSCAN") == 0)
		return (useron.misc & ASK_SSCAN) ? text[On] : text[Off];

	if (strcmp(sp, "ANFSCAN") == 0)
		return (useron.misc & ANFSCAN) ? text[On] : text[Off];

	if (strcmp(sp, "EDITOR") == 0)
		return (useron.xedit < 1 || useron.xedit >= cfg.total_xedits) ? text[None] : cfg.xedit[useron.xedit - 1]->name;

	if (strcmp(sp, "SHELL") == 0)
		return cfg.shell[useron.shell]->name;

	if (strcmp(sp, "TMP") == 0)
		return useron.tmpext;

	if (strcmp(sp, "PROT") == 0) {
		safe_snprintf(str, maxlen, "%c", useron.prot);
		return str;
	}
	if (strcmp(sp, "PROTNAME") == 0)
		return protname(useron.prot);

	if (strcmp(sp, "SEX") == 0) {
		safe_snprintf(str, maxlen, "%c", useron.sex);
		return str;
	}
	if (strcmp(sp, "GENDERS") == 0) {
		return cfg.new_genders;
	}

	if (!strcmp(sp, "QWKID"))
		return cfg.sys_id;

	if (!strcmp(sp, "TIME") || !strcmp(sp, "SYSTIME") || !strcmp(sp, "TIME_UTC")) {
		now = time(NULL);
		memset(&tm, 0, sizeof(tm));
		if (strcmp(sp, "TIME_UTC") == 0)
			gmtime_r(&now, &tm);
		else
			localtime_r(&now, &tm);
		if (cfg.sys_misc & SM_MILITARY)
			safe_snprintf(str, maxlen, "%02d:%02d:%02d"
			              , tm.tm_hour, tm.tm_min, tm.tm_sec);
		else
			safe_snprintf(str, maxlen, "%02d:%02d %s"
			              , tm.tm_hour == 0 ? 12
			    : tm.tm_hour > 12 ? tm.tm_hour - 12
			    : tm.tm_hour, tm.tm_min, tm.tm_hour > 11 ? "pm":"am");
		return str;
	}

	if (!strcmp(sp, "TIMEZONE"))
		return smb_zonestr(sys_timezone(&cfg), str);

	if (!strcmp(sp, "DATE") || !strcmp(sp, "SYSDATE") || !strcmp(sp, "DATE_UTC")) {
		now = time(NULL);
		if (strcmp(sp, "DATE_UTC") == 0)
			now -= xpTimeZone_local() * 60;
		return datestr(now);
	}

	if (strncmp(sp, "DATE:", 5) == 0 || strncmp(sp, "TIME:", 5) == 0) {
		SAFECOPY(tmp, sp + 5);
		c_unescape_str(tmp);
		now = time(NULL);
		memset(&tm, 0, sizeof(tm));
		localtime_r(&now, &tm);
		strftime(str, maxlen, tmp, &tm);
		return str;
	}

	if (strncmp(sp, "UTC:", 4) == 0) {
		SAFECOPY(tmp, sp + 4);
		c_unescape_str(tmp);
		now = time(NULL);
		memset(&tm, 0, sizeof(tm));
		gmtime_r(&now, &tm);
		strftime(str, maxlen, tmp, &tm);
		return str;
	}

	if (!strcmp(sp, "DATETIME"))
		return timestr(time(NULL));

	if (!strcmp(sp, "DATETIME_UTC"))
		return timestr(time(NULL) - (xpTimeZone_local() * 60));

	if (!strcmp(sp, "DATETIMEZONE")) {
		char zone[32];
		safe_snprintf(str, maxlen, "%s %s", timestr(time(NULL)), smb_zonestr(sys_timezone(&cfg), zone));
		return str;
	}

	if (strcmp(sp, "DATEFMT") == 0) {
		return date_format(&cfg, str, maxlen);
	}

	if (strcmp(sp, "BDATEFMT") == 0 || strcmp(sp, "BIRTHFMT") == 0) {
		return birthdate_format(&cfg, str, maxlen);
	}

	if (strcmp(sp, "GENDERS") == 0)
		return cfg.new_genders;

	if (strcmp(sp, "MSGS") == 0) {
		uint msgs = usrgrps ? getposts(&cfg, usrsub[curgrp][cursub[curgrp]]) : 0;
		snprintf(str, maxlen, "%u",  msgs);
		return str;
	}

	if (strcmp(sp, "NEWMSGS") == 0) {
		uint msgs = usrgrps ? getnewposts(&cfg, usrsub[curgrp][cursub[curgrp]], subscan[usrsub[curgrp][cursub[curgrp]]].ptr) : 0;
		snprintf(str, maxlen, "%u",  msgs);
		return str;
	}

	if (!strcmp(sp, "TMSG")) {
		l = 0;
		for (i = 0; i < cfg.total_subs; i++)
			l += getposts(&cfg, i);     /* l=total posts */
		safe_snprintf(str, maxlen, "%lu", l);
		return str;
	}

	if (!strcmp(sp, "TUSER")) {
		safe_snprintf(str, maxlen, "%u", total_users(&cfg));
		return str;
	}

	if (!strcmp(sp, "TFILE")) {
		l = 0;
		for (i = 0; i < cfg.total_dirs; i++)
			l += getfiles(&cfg, i);
		safe_snprintf(str, maxlen, "%lu", l);
		return str;
	}

	if (strncmp(sp, "FILES:", 6) == 0) { // Number of files in specified directory
		const char* path = getpath(&cfg, sp + 6);
		safe_snprintf(str, maxlen, "%u", getfilecount(path));
		return str;
	}

	if (strcmp(sp, "FILES") == 0) {  // Number of files in current directory
		safe_snprintf(str, maxlen, "%u", usrlibs ? getfiles(&cfg, usrdir[curlib][curdir[curlib]]) : 0);
		return str;
	}

	if (strcmp(sp, "NEWFILES") == 0) {  // Number of new files in current directory
		safe_snprintf(str, maxlen, "%u", usrlibs ? getnewfiles(&cfg, usrdir[curlib][curdir[curlib]], ns_time) : 0);
		return str;
	}

	if (strncmp(sp, "FILESIZE:", 9) == 0) {
		const char* path = getpath(&cfg, sp + 9);
		byte_estimate_to_str(getfilesizetotal(path), str, maxlen, /* unit: */ 1, /* precision: */ 1);
		return str;
	}

	if (strcmp(sp, "FILESIZE") == 0) {
		byte_estimate_to_str(usrlibs ? getfilesizetotal(cfg.dir[usrdir[curlib][curdir[curlib]]]->path) : 0
		                     , str, maxlen, /* unit: */ 1, /* precision: */ 1);
		return str;
	}

	if (strncmp(sp, "FILEBYTES:", 10) == 0) {    // Number of bytes in current file directory
		const char* path = getpath(&cfg, sp + 10);
		safe_snprintf(str, maxlen, "%" PRIu64, getfilesizetotal(path));
		return str;
	}

	if (strcmp(sp, "FILEBYTES") == 0) {  // Number of bytes in current file directory
		safe_snprintf(str, maxlen, "%" PRIu64
		              , usrlibs ? getfilesizetotal(cfg.dir[usrdir[curlib][curdir[curlib]]]->path) : 0);
		return str;
	}

	if (strncmp(sp, "FILEKB:", 7) == 0) {    // Number of kibibytes in current file directory
		const char* path = getpath(&cfg, sp + 7);
		safe_snprintf(str, maxlen, "%1.1f", getfilesizetotal(path) / 1024.0);
		return str;
	}

	if (strcmp(sp, "FILEKB") == 0) { // Number of kibibytes in current file directory
		safe_snprintf(str, maxlen, "%1.1f"
		              , usrlibs ? getfilesizetotal(cfg.dir[usrdir[curlib][curdir[curlib]]]->path) / 1024.0 : 0);
		return str;
	}

	if (strncmp(sp, "FILEMB:", 7) == 0) {    // Number of mebibytes in current file directory
		const char* path = getpath(&cfg, sp + 7);
		safe_snprintf(str, maxlen, "%1.1f", getfilesizetotal(path) / (1024.0 * 1024.0));
		return str;
	}

	if (strcmp(sp, "FILEMB") == 0) { // Number of mebibytes in current file directory
		safe_snprintf(str, maxlen, "%1.1f"
		              , usrlibs ? getfilesizetotal(cfg.dir[usrdir[curlib][curdir[curlib]]]->path) / (1024.0 * 1024.0) : 0);
		return str;
	}

	if (strncmp(sp, "FILEGB:", 7) == 0) {    // Number of gibibytes in current file directory
		const char* path = getpath(&cfg, sp + 7);
		safe_snprintf(str, maxlen, "%1.1f", getfilesizetotal(path) / (1024.0 * 1024.0 * 1024.0));
		return str;
	}

	if (strcmp(sp, "FILEGB") == 0) { // Number of gibibytes in current file directory
		safe_snprintf(str, maxlen, "%1.1f"
		              , usrlibs ? getfilesizetotal(cfg.dir[usrdir[curlib][curdir[curlib]]]->path) / (1024.0 * 1024.0 * 1024.0) : 0);
		return str;
	}

	if (!strcmp(sp, "TCALLS") || !strcmp(sp, "NUMCALLS")) {
		getstats(&cfg, 0, &stats);
		safe_snprintf(str, maxlen, "%u", stats.logons);
		return str;
	}

	if (!strcmp(sp, "PREVON") || !strcmp(sp, "LASTCALLERNODE")
	    || !strcmp(sp, "LASTCALLERSYSTEM"))
		return lastuseron;

	if (!strcmp(sp, "CLS") || !strcmp(sp, "CLEAR")) {
		cls();
		return nulstr;
	}

	if (strcmp(sp, "GETDIM") == 0) {
		getdimensions();
		return nulstr;
	}

	if (strcmp(sp, "GETKEY") == 0) {
		getkey();
		return nulstr;
	}

	if (strcmp(sp, "CONTINUE") == 0) {
		char ch = getkey(K_UPPER);
		if (ch == no_key() || ch == quit_key())
			sys_status |= SS_ABORT;
		return nulstr;
	}

	if (strncmp(sp, "WAIT:", 5) == 0) {
		inkey(K_NONE, atoi(sp + 5) * 100);
		return nulstr;
	}

	if (!strcmp(sp, "PAUSE") || !strcmp(sp, "MORE")) {
		pause();
		return nulstr;
	}

	if (!strcmp(sp, "RESETPAUSE")) {
		term->lncntr = 0;
		return nulstr;
	}

	if (!strcmp(sp, "NOPAUSE") || !strcmp(sp, "POFF")) {
		sys_status ^= SS_PAUSEOFF;
		return nulstr;
	}

	if (!strcmp(sp, "PON") || !strcmp(sp, "AUTOMORE")) {
		sys_status ^= SS_PAUSEON;
		return nulstr;
	}

	if (strncmp(sp, "FILL:", 5) == 0) {
		SAFECOPY(tmp, sp + 5);
		int margin = centered ? term->column : 1;
		if (margin < 1)
			margin = 1;
		c_unescape_str(tmp);
		while (*tmp && online && term->column < term->cols - margin)
			bputs(tmp, P_TRUNCATE);
		return nulstr;
	}

	if (strncmp(sp, "POS:", 4) == 0) {   // PCBoard	(nn is 1 based)
		i = atoi(sp + 4);
		if (i >= 1)  // Convert to 0-based
			i--;
		for (l = i - term->column; l > 0; l--)
			outchar(' ');
		return nulstr;
	}

	if (strncmp(sp, "DELAY:", 6) == 0) { // PCBoard
		mswait(atoi(sp + 6) * 100);
		return nulstr;
	}

	if (strcmp(sp, "YESCHAR") == 0) {    // PCBoard
		safe_snprintf(str, maxlen, "%c", yes_key());
		return str;
	}

	if (strcmp(sp, "NOCHAR") == 0) {     // PCBoard
		safe_snprintf(str, maxlen, "%c", no_key());
		return str;
	}

	if (strcmp(sp, "QUITCHAR") == 0) {
		safe_snprintf(str, maxlen, "%c", quit_key());
		return str;
	}

	if (strncmp(sp, "BPS:", 4) == 0) {
		term->set_output_rate((enum output_rate)atoi(sp + 4));
		return nulstr;
	}

	if (strncmp(sp, "TEXT:", 5) == 0) {
		i = atoi(sp + 5);
		if (i >= 1 && i <= TOTAL_TEXT)
			return text[i - 1];
		else
			return get_text(sp + 5);
	}

	/* NOSTOP */

	/* STOP */

	if (!strcmp(sp, "BELL") || !strcmp(sp, "BEEP"))
		return "\a";

	if (!strcmp(sp, "EVENT")) {
		if (event_time == 0)
			return "<none>";
		return timestr(event_time);
	}

	/* LASTCALL */

	if (strcmp(sp, "NODE_USER") == 0)
		return thisnode.misc & NODE_ANON ? text[UNKNOWN_USER] : useron.alias;

	if (!strncmp(sp, "NODE", 4) && IS_DIGIT(sp[4])) {
		i = atoi(sp + 4);
		if (i && i <= cfg.sys_nodes) {
			getnodedat(i, &node);
			printnodedat(i, &node);
		}
		return nulstr;
	}

	if (!strcmp(sp, "WHO")) {
		whos_online(true);
		return nulstr;
	}

	/* User Codes */

	if (!strcmp(sp, "USER") || !strcmp(sp, "ALIAS") || !strcmp(sp, "NAME"))
		return useron.alias;

	if (!strcmp(sp, "FIRST")) {
		safe_snprintf(str, maxlen, "%s", useron.alias);
		tp = strchr(str, ' ');
		if (tp)
			*tp = 0;
		return str;
	}

	if (!strcmp(sp, "USERNUM")) {
		safe_snprintf(str, maxlen, "%u", useron.number);
		return str;
	}

	if (!strcmp(sp, "PHONE") || !strcmp(sp, "HOMEPHONE")
	    || !strcmp(sp, "DATAPHONE") || !strcmp(sp, "DATA"))
		return useron.phone;

	if (!strcmp(sp, "ADDR1"))
		return useron.address;

	if (!strcmp(sp, "FROM"))
		return useron.location;

	if (!strcmp(sp, "CITY")) {
		safe_snprintf(str, maxlen, "%s", useron.location);
		char* p = strchr(str, ',');
		if (p) {
			*p = 0;
			return str;
		}
		return nulstr;
	}

	if (!strcmp(sp, "STATE")) {
		char* p = strchr(useron.location, ',');
		if (p) {
			p++;
			if (*p == ' ')
				p++;
			return p;
		}
		return nulstr;
	}

	if (!strcmp(sp, "CPU"))
		return useron.comp;

	if (!strcmp(sp, "HOST"))
		return client_name;

	if (!strcmp(sp, "BDATE"))
		return getbirthdstr(&cfg, useron.birth, str, maxlen);

	if (strcmp(sp, "BIRTH") == 0)
		return format_birthdate(&cfg, useron.birth, str, maxlen);

	if (strncmp(sp, "BDATE:", 6) == 0 || strncmp(sp, "BIRTH:", 6) == 0) {
		SAFECOPY(tmp, sp + 6);
		c_unescape_str(tmp);
		memset(&tm, 0, sizeof(tm));
		tm.tm_year = getbirthyear(&cfg, useron.birth) - 1900;
		tm.tm_mon = getbirthmonth(&cfg, useron.birth) - 1;
		tm.tm_mday = getbirthday(&cfg, useron.birth);
		mktime(&tm);
		strftime(str, maxlen, tmp, &tm);
		return str;
	}

	if (!strcmp(sp, "AGE")) {
		safe_snprintf(str, maxlen, "%u", getage(&cfg, useron.birth));
		return str;
	}

	if (!strcmp(sp, "CALLS") || !strcmp(sp, "NUMTIMESON")) {
		safe_snprintf(str, maxlen, "%u", useron.logons);
		return str;
	}

	if (strcmp(sp, "PWAGE") == 0) {
		time_t age = time(NULL) - useron.pwmod;
		safe_snprintf(str, maxlen, "%d", (uint)(age / (24 * 60 * 60)));
		return str;
	}

	if (strcmp(sp, "PWDATE") == 0 || strcmp(sp, "MEMO") == 0)
		return datestr(useron.pwmod);

	if (strncmp(sp, "PWDATE:", 7) == 0) {
		SAFECOPY(tmp, sp + 7);
		c_unescape_str(tmp);
		memset(&tm, 0, sizeof(tm));
		time_t date = useron.pwmod;
		localtime_r(&date, &tm);
		strftime(str, maxlen, tmp, &tm);
		return str;
	}

	if (!strcmp(sp, "SEC") || !strcmp(sp, "SECURITY")) {
		safe_snprintf(str, maxlen, "%u", useron.level);
		return str;
	}

	if (!strcmp(sp, "SINCE"))
		return datestr(useron.firston);

	if (strncmp(sp, "SINCE:", 6) == 0) {
		SAFECOPY(tmp, sp + 6);
		c_unescape_str(tmp);
		memset(&tm, 0, sizeof(tm));
		time_t date = useron.firston;
		localtime_r(&date, &tm);
		strftime(str, maxlen, tmp, &tm);
		return str;
	}

	if (!strcmp(sp, "TIMEON") || !strcmp(sp, "TIMEUSED")) {
		now = time(NULL);
		safe_snprintf(str, maxlen, "%u", (uint)(now - logontime) / 60);
		return str;
	}

	if (!strcmp(sp, "TUSED")) {              /* Synchronet only */
		now = time(NULL);
		return sectostr((uint)(now - logontime), str) + 1;
	}

	if (!strcmp(sp, "TLEFT")) {              /* Synchronet only */
		gettimeleft();
		return sectostr(timeleft, str) + 1;
	}

	if (!strcmp(sp, "TPERD"))                /* Synchronet only */
		return sectostr(cfg.level_timeperday[useron.level], str) + 4;

	if (!strcmp(sp, "TPERC"))                /* Synchronet only */
		return sectostr(cfg.level_timepercall[useron.level], str) + 4;

	if (strcmp(sp, "MPERC") == 0 || strcmp(sp, "TIMELIMIT") == 0) {
		safe_snprintf(str, maxlen, "%u", cfg.level_timepercall[useron.level]);
		return str;
	}

	if (strcmp(sp, "MPERD") == 0) {
		safe_snprintf(str, maxlen, "%u", cfg.level_timeperday[useron.level]);
		return str;
	}

	if (strcmp(sp, "MAXCALLS") == 0) {
		safe_snprintf(str, maxlen, "%u", cfg.level_callsperday[useron.level]);
		return str;
	}

	if (strcmp(sp, "MAXPOSTS") == 0) {
		safe_snprintf(str, maxlen, "%u", cfg.level_postsperday[useron.level]);
		return str;
	}

	if (strcmp(sp, "MAXMAILS") == 0) {
		safe_snprintf(str, maxlen, "%u", cfg.level_emailperday[useron.level]);
		return str;
	}

	if (strcmp(sp, "MAXLINES") == 0) {
		safe_snprintf(str, maxlen, "%u", cfg.level_linespermsg[useron.level]);
		return str;
	}

	if (!strcmp(sp, "MINLEFT") || !strcmp(sp, "LEFT") || !strcmp(sp, "TIMELEFT")) {
		gettimeleft();
		safe_snprintf(str, maxlen, "%u", timeleft / 60);
		return str;
	}

	if (!strcmp(sp, "LASTON"))
		return timestr(useron.laston);

	if (!strcmp(sp, "LASTDATEON"))
		return datestr(useron.laston);

	if (strncmp(sp, "LASTON:", 7) == 0) {
		SAFECOPY(tmp, sp + 7);
		c_unescape_str(tmp);
		memset(&tm, 0, sizeof(tm));
		time_t date = useron.laston;
		localtime_r(&date, &tm);
		strftime(str, maxlen, tmp, &tm);
		return str;
	}

	if (!strcmp(sp, "LASTTIMEON")) {
		memset(&tm, 0, sizeof(tm));
		localtime32(&useron.laston, &tm);
		if (cfg.sys_misc & SM_MILITARY)
			safe_snprintf(str, maxlen, "%02d:%02d:%02d"
			              , tm.tm_hour, tm.tm_min, tm.tm_sec);
		else
			safe_snprintf(str, maxlen, "%02d:%02d %s"
			              , tm.tm_hour == 0 ? 12
			    : tm.tm_hour > 12 ? tm.tm_hour - 12
			    : tm.tm_hour, tm.tm_min, tm.tm_hour > 11 ? "pm":"am");
		return str;
	}

	if (!strcmp(sp, "FIRSTON"))
		return timestr(useron.firston);

	if (!strcmp(sp, "FIRSTDATEON"))
		return datestr(useron.firston);

	if (strncmp(sp, "FIRSTON:", 8) == 0) {
		SAFECOPY(tmp, sp + 8);
		c_unescape_str(tmp);
		memset(&tm, 0, sizeof(tm));
		time_t date = useron.firston;
		localtime_r(&date, &tm);
		strftime(str, maxlen, tmp, &tm);
		return str;
	}

	if (!strcmp(sp, "FIRSTTIMEON")) {
		memset(&tm, 0, sizeof(tm));
		localtime32(&useron.firston, &tm);
		if (cfg.sys_misc & SM_MILITARY)
			safe_snprintf(str, maxlen, "%02d:%02d:%02d"
			              , tm.tm_hour, tm.tm_min, tm.tm_sec);
		else
			safe_snprintf(str, maxlen, "%02d:%02d %s"
			              , tm.tm_hour == 0 ? 12
			    : tm.tm_hour > 12 ? tm.tm_hour - 12
			    : tm.tm_hour, tm.tm_min, tm.tm_hour > 11 ? "pm":"am");
		return str;
	}

	if (strcmp(sp, "EMAILS") == 0) {
		safe_snprintf(str, maxlen, "%u", useron.emails);
		return str;
	}

	if (strcmp(sp, "FBACKS") == 0) {
		safe_snprintf(str, maxlen, "%u", useron.fbacks);
		return str;
	}

	if (strcmp(sp, "ETODAY") == 0) {
		safe_snprintf(str, maxlen, "%u", useron.etoday);
		return str;
	}

	if (strcmp(sp, "PTODAY") == 0) {
		safe_snprintf(str, maxlen, "%u", useron.ptoday);
		return str;
	}

	if (strcmp(sp, "DTODAY") == 0) {
		safe_snprintf(str, maxlen, "%u", useron.dtoday);
		return str;
	}

	if (strcmp(sp, "BTODAY") == 0) {
		safe_snprintf(str, maxlen, "%" PRIu64, useron.btoday);
		return str;
	}

	if (strcmp(sp, "KTODAY") == 0) {
		safe_snprintf(str, maxlen, "%" PRIu64, useron.btoday / 1024);
		return str;
	}

	if (strcmp(sp, "LTODAY") == 0) {
		safe_snprintf(str, maxlen, "%u", useron.ltoday);
		return str;
	}

	if (strcmp(sp, "MTODAY") == 0) {
		safe_snprintf(str, maxlen, "%u", useron.ttoday);
		return str;
	}

	if (strcmp(sp, "MTOTAL") == 0) {
		safe_snprintf(str, maxlen, "%u", useron.timeon);
		return str;
	}

	if (strcmp(sp, "TTODAY") == 0)
		return sectostr(useron.ttoday, str) + 3;

	if (strcmp(sp, "TTOTAL") == 0)
		return sectostr(useron.timeon, str) + 3;

	if (strcmp(sp, "TLAST") == 0) {
		safe_snprintf(str, maxlen, "%u", useron.tlast);
		return str;
	}

	if (strcmp(sp, "MEXTRA") == 0) {
		safe_snprintf(str, maxlen, "%u", useron.textra);
		return str;
	}

	if (strcmp(sp, "TEXTRA") == 0)
		return sectostr(useron.textra, str) + 3;

	if (strcmp(sp, "MBANKED") == 0) {
		safe_snprintf(str, maxlen, "%" PRIu32, useron.min);
		return str;
	}

	if (strcmp(sp, "TBANKED") == 0)
		return sectostr(useron.min, str) + 3;

	if (!strcmp(sp, "MSGLEFT") || !strcmp(sp, "MSGSLEFT")) {
		safe_snprintf(str, maxlen, "%u", useron.posts);
		return str;
	}

	if (!strcmp(sp, "MSGREAD")) {
		safe_snprintf(str, maxlen, "%u", posts_read);
		return str;
	}

	if (!strcmp(sp, "FREESPACE")) {
		safe_snprintf(str, maxlen, "%" PRIu64, getfreediskspace(cfg.temp_dir, 0));
		return str;
	}

	if (!strcmp(sp, "FREESPACEK")) {
		safe_snprintf(str, maxlen, "%" PRIu64, getfreediskspace(cfg.temp_dir, 1024));
		return str;
	}

	if (strcmp(sp, "FREESPACEM") == 0) {
		safe_snprintf(str, maxlen, "%" PRIu64, getfreediskspace(cfg.temp_dir, 1024 * 1024));
		return str;
	}

	if (strcmp(sp, "FREESPACEG") == 0) {
		safe_snprintf(str, maxlen, "%" PRIu64, getfreediskspace(cfg.temp_dir, 1024 * 1024 * 1024));
		return str;
	}

	if (strcmp(sp, "FREESPACET") == 0) {
		safe_snprintf(str, maxlen, "%" PRIu64, getfreediskspace(cfg.temp_dir, 1024 * 1024 * 1024) / 1024);
		return str;
	}

	if (strcmp(sp, "MINSPACE") == 0) {
		byte_count_to_str(cfg.min_dspace, str, maxlen);
		return str;
	}

	if (!strcmp(sp, "UPBYTES")) {
		safe_snprintf(str, maxlen, "%" PRIu64, useron.ulb);
		return str;
	}

	if (!strcmp(sp, "UPK")) {
		safe_snprintf(str, maxlen, "%" PRIu64, useron.ulb / 1024L);
		return str;
	}

	if (!strcmp(sp, "UPS") || !strcmp(sp, "UPFILES")) {
		safe_snprintf(str, maxlen, "%u", useron.uls);
		return str;
	}

	if (!strcmp(sp, "DLBYTES")) {
		safe_snprintf(str, maxlen, "%" PRIu64, useron.dlb);
		return str;
	}

	if (!strcmp(sp, "DOWNK")) {
		safe_snprintf(str, maxlen, "%" PRIu64, useron.dlb / 1024L);
		return str;
	}

	if (!strcmp(sp, "DOWNS") || !strcmp(sp, "DLFILES")) {
		safe_snprintf(str, maxlen, "%u", useron.dls);
		return str;
	}

	if (strcmp(sp, "PCR") == 0) {
		float f = 0;
		if (useron.posts)
			f = (float)useron.logons / useron.posts;
		safe_snprintf(str, maxlen, "%u", f ? (uint)(100 / f) : 0);
		return str;
	}

	if (strcmp(sp, "UDR") == 0) {
		float f = 0;
		if (useron.ulb)
			f = (float)useron.dlb / useron.ulb;
		safe_snprintf(str, maxlen, "%u", f ? (uint)(100 / f) : 0);
		return str;
	}

	if (strcmp(sp, "UDFR") == 0) {
		float f = 0;
		if (useron.uls)
			f = (float)useron.dls / useron.uls;
		safe_snprintf(str, maxlen, "%u", f ? (uint)(100 / f) : 0);
		return str;
	}

	if (!strcmp(sp, "LASTNEW"))
		return datestr(ns_time);

	if (strncmp(sp, "LASTNEW:", 8) == 0) {
		SAFECOPY(tmp, sp + 8);
		c_unescape_str(tmp);
		memset(&tm, 0, sizeof(tm));
		time_t date = ns_time;
		localtime_r(&date, &tm);
		strftime(str, maxlen, tmp, &tm);
		return str;
	}

	if (!strcmp(sp, "NEWFILETIME"))
		return timestr(ns_time);

	if (strcmp(sp, "MAXDL") == 0) {
		uint max_files = user_downloads_per_day(&cfg, &useron);
		if (max_files == UINT_MAX)
			return text[Unlimited];
		safe_snprintf(str, maxlen, "%u", max_files);
		return str;
	}

	if (!strcmp(sp, "MAXDK") || !strcmp(sp, "DLKLIMIT") || !strcmp(sp, "KBLIMIT")) {
		safe_snprintf(str, maxlen, "%" PRIu64, cfg.level_freecdtperday[useron.level] / 1024L);
		return str;
	}

	if (!strcmp(sp, "DAYBYTES")) {    /* amt of free cdts used today */
		safe_snprintf(str, maxlen, "%" PRIu64, cfg.level_freecdtperday[useron.level] - useron.freecdt);
		return str;
	}

	if (!strcmp(sp, "BYTELIMIT")) {
		safe_snprintf(str, maxlen, "%" PRIu64, cfg.level_freecdtperday[useron.level]);
		return str;
	}

	if (!strcmp(sp, "KBLEFT")) {
		safe_snprintf(str, maxlen, "%" PRIu64, user_available_credits(&useron) / 1024UL);
		return str;
	}

	if (!strcmp(sp, "BYTESLEFT")) {
		safe_snprintf(str, maxlen, "%" PRIu64, user_available_credits(&useron));
		return str;
	}

	if (strcmp(sp, "CREDITS") == 0) {
		safe_snprintf(str, maxlen, "%" PRIu64, useron.cdt);
		return str;
	}

	if (strcmp(sp, "FREECDT") == 0) {
		safe_snprintf(str, maxlen, "%" PRIu64, useron.freecdt);
		return str;
	}

	if (!strcmp(sp, "CONF")) {
		safe_snprintf(str, maxlen, "%s %s"
		              , usrgrps ? cfg.grp[usrgrp[curgrp]]->sname :nulstr
		              , usrgrps ? cfg.sub[usrsub[curgrp][cursub[curgrp]]]->sname : nulstr);
		return str;
	}

	if (!strcmp(sp, "CONFNUM")) {
		safe_snprintf(str, maxlen, "%u %u", curgrp + 1, cursub[curgrp] + 1);
		return str;
	}

	if (!strcmp(sp, "NUMDIR")) {
		safe_snprintf(str, maxlen, "%u %u", usrlibs ? curlib + 1 : 0, usrlibs ? curdir[curlib] + 1 : 0);
		return str;
	}

	if (!strcmp(sp, "EXDATE") || !strcmp(sp, "EXPDATE"))
		return datestr(useron.expire);

	if (strncmp(sp, "EXPDATE:", 8) == 0) {
		if (!useron.expire)
			return nulstr;
		SAFECOPY(tmp, sp + 8);
		c_unescape_str(tmp);
		memset(&tm, 0, sizeof(tm));
		time_t date = useron.expire;
		localtime_r(&date, &tm);
		strftime(str, maxlen, tmp, &tm);
		return str;
	}

	if (!strcmp(sp, "EXPDAYS")) {
		now = time(NULL);
		l = (uint)(useron.expire - now);
		if (l < 0)
			l = 0;
		safe_snprintf(str, maxlen, "%lu", l / (1440L * 60L));
		return str;
	}

	if (strcmp(sp, "NOTE") == 0 || strcmp(sp, "MEMO1") == 0)
		return useron.note;

	if (strcmp(sp, "REALNAME") == 0 || !strcmp(sp, "MEMO2") || !strcmp(sp, "COMPANY"))
		return useron.name;

	if (!strcmp(sp, "ZIP"))
		return useron.zipcode;

	if (!strcmp(sp, "HANGUP")) {
		hangup();
		return nulstr;
	}

	/* Synchronet Specific */

	if (!strncmp(sp, "SETSTR:", 7)) {
		strcpy(main_csi.str, sp + 7);
		return nulstr;
	}

	if (strcmp(sp, "STR") == 0) {
		return main_csi.str;
	}

	if (strncmp(sp, "STRVAR:", 7) == 0) {
		uint32_t crc = crc32(sp + 7, 0);
		if (main_csi.str_var && main_csi.str_var_name) {
			for (i = 0; i < main_csi.str_vars; i++)
				if (main_csi.str_var_name[i] == crc)
					return main_csi.str_var[i];
		}
		return nulstr;
	}

	if (strncmp(sp, "JS:", 3) == 0) {
		jsval val;
		if (JS_GetProperty(js_cx, obj == NULL ? js_glob : obj, sp + 3, &val))
			JSVALUE_TO_STRBUF(js_cx, val, str, maxlen, NULL);
		return str;
	}

	if (!strncmp(sp, "EXEC:", 5)) {
		SAFECOPY(tmp, sp + 5);
		c_unescape_str(tmp);
		exec_bin(tmp, &main_csi);
		return nulstr;
	}

	if (!strncmp(sp, "EXEC_XTRN:", 10)) {
		for (i = 0; i < cfg.total_xtrns; i++)
			if (!stricmp(cfg.xtrn[i]->code, sp + 10))
				break;
		if (i < cfg.total_xtrns)
			exec_xtrn(i);
		return nulstr;
	}

	if (!strncmp(sp, "MENU:", 5)) {
		menu(sp + 5);
		return nulstr;
	}

	if (!strncmp(sp, "CONDMENU:", 9)) {
		menu(sp + 9, P_NOERROR);
		return nulstr;
	}

	if (!strncmp(sp, "TYPE:", 5)) {
		printfile(cmdstr(sp + 5, nulstr, nulstr, str), P_MODS);
		return nulstr;
	}

	if (!strncmp(sp, "INCLUDE:", 8)) {
		printfile(cmdstr(sp + 8, nulstr, nulstr, str), P_NOCRLF | P_SAVEATR | P_MODS);
		return nulstr;
	}

	if (!strcmp(sp, "QUESTION"))
		return question;

	if (!strcmp(sp, "HANDLE"))
		return useron.handle;

	if (strcmp(sp, "LASTIP") == 0)
		return useron.ipaddr;

	if (!strcmp(sp, "CID") || !strcmp(sp, "IP"))
		return cid;

	if (!strcmp(sp, "LOCAL-IP"))
		return local_addr;

	if (!strcmp(sp, "CRLF"))
		return "\r\n";

	if (!strcmp(sp, "PUSHXY")) {
		term->save_cursor_pos();
		return nulstr;
	}

	if (!strcmp(sp, "POPXY")) {
		term->restore_cursor_pos();
		return nulstr;
	}

	if (!strcmp(sp, "HOME")) {
		term->cursor_home();
		return nulstr;
	}

	if (!strcmp(sp, "CLRLINE")) {
		term->clearline();
		return nulstr;
	}

	if (!strcmp(sp, "CLR2EOL") || !strcmp(sp, "CLREOL")) {
		term->cleartoeol();
		return nulstr;
	}

	if (!strcmp(sp, "CLR2EOS")) {
		term->cleartoeos();
		return nulstr;
	}

	if (!strncmp(sp, "UP:", 3)) {
		term->cursor_up(atoi(sp + 3));
		return str;
	}

	if (!strncmp(sp, "DOWN:", 5)) {
		term->cursor_down(atoi(sp + 5));
		return str;
	}

	if (!strncmp(sp, "LEFT:", 5)) {
		term->cursor_left(atoi(sp + 5));
		return str;
	}

	if (!strncmp(sp, "RIGHT:", 6)) {
		term->cursor_right(atoi(sp + 6));
		return str;
	}

	if (!strncmp(sp, "GOTOXY:", 7)) {
		const char* cp = strchr(sp, ',');
		if (cp != NULL) {
			cp++;
			term->gotoxy(atoi(sp + 7), atoi(cp));
		}
		return nulstr;
	}

	if (!strcmp(sp, "GRP")) {
		if (SMB_IS_OPEN(&smb)) {
			if (smb.subnum == INVALID_SUB)
				return "Local";
			if (subnum_is_valid(smb.subnum))
				return cfg.grp[cfg.sub[smb.subnum]->grp]->sname;
		}
		return usrgrps ? cfg.grp[usrgrp[curgrp]]->sname : nulstr;
	}

	if (!strcmp(sp, "GRPL")) {
		if (SMB_IS_OPEN(&smb)) {
			if (smb.subnum == INVALID_SUB)
				return "Local";
			if (subnum_is_valid(smb.subnum))
				return cfg.grp[cfg.sub[smb.subnum]->grp]->lname;
		}
		return usrgrps ? cfg.grp[usrgrp[curgrp]]->lname : nulstr;
	}

	if (!strcmp(sp, "GN")) {
		if (SMB_IS_OPEN(&smb))
			ugrp = getusrgrp(smb.subnum);
		else
			ugrp = usrgrps ? curgrp + 1 : 0;
		safe_snprintf(str, maxlen, "%u", ugrp);
		return str;
	}

	if (!strcmp(sp, "GL")) {
		if (SMB_IS_OPEN(&smb))
			ugrp = getusrgrp(smb.subnum);
		else
			ugrp = usrgrps ? curgrp + 1 : 0;
		safe_snprintf(str, maxlen, "%-4u", ugrp);
		return str;
	}

	if (!strcmp(sp, "GR")) {
		if (SMB_IS_OPEN(&smb))
			ugrp = getusrgrp(smb.subnum);
		else
			ugrp = usrgrps ? curgrp + 1 : 0;
		safe_snprintf(str, maxlen, "%4u", ugrp);
		return str;
	}

	if (!strcmp(sp, "SUB")) {
		if (SMB_IS_OPEN(&smb)) {
			if (smb.subnum == INVALID_SUB)
				return "Mail";
			else if (subnum_is_valid(smb.subnum))
				return cfg.sub[smb.subnum]->sname;
		}
		return usrgrps ? cfg.sub[usrsub[curgrp][cursub[curgrp]]]->sname : nulstr;
	}

	if (!strcmp(sp, "SUBL")) {
		if (SMB_IS_OPEN(&smb)) {
			if (smb.subnum == INVALID_SUB)
				return "Mail";
			else if (subnum_is_valid(smb.subnum))
				return cfg.sub[smb.subnum]->lname;
		}
		return usrgrps  ? cfg.sub[usrsub[curgrp][cursub[curgrp]]]->lname : nulstr;
	}

	if (!strcmp(sp, "SN")) {
		if (SMB_IS_OPEN(&smb))
			usub = getusrsub(smb.subnum);
		else
			usub = usrgrps ? cursub[curgrp] + 1 : 0;
		safe_snprintf(str, maxlen, "%u", usub);
		return str;
	}

	if (!strcmp(sp, "SL")) {
		if (SMB_IS_OPEN(&smb))
			usub = getusrsub(smb.subnum);
		else
			usub = usrgrps ? cursub[curgrp] + 1 : 0;
		safe_snprintf(str, maxlen, "%-4u", usub);
		return str;
	}

	if (!strcmp(sp, "SR")) {
		if (SMB_IS_OPEN(&smb))
			usub = getusrsub(smb.subnum);
		else
			usub = usrgrps ? cursub[curgrp] + 1 : 0;
		safe_snprintf(str, maxlen, "%4u", usub);
		return str;
	}

	if (!strcmp(sp, "LIB"))
		return usrlibs ? cfg.lib[usrlib[curlib]]->sname : nulstr;

	if (!strcmp(sp, "LIBL"))
		return usrlibs ? cfg.lib[usrlib[curlib]]->lname : nulstr;

	if (!strcmp(sp, "LN")) {
		safe_snprintf(str, maxlen, "%u", usrlibs ? curlib + 1 : 0);
		return str;
	}

	if (!strcmp(sp, "LL")) {
		safe_snprintf(str, maxlen, "%-4u", usrlibs ? curlib + 1 : 0);
		return str;
	}

	if (!strcmp(sp, "LR")) {
		safe_snprintf(str, maxlen, "%4u", usrlibs  ? curlib + 1 : 0);
		return str;
	}

	if (!strcmp(sp, "DIR"))
		return usrlibs ? cfg.dir[usrdir[curlib][curdir[curlib]]]->sname :nulstr;

	if (!strcmp(sp, "DIRL"))
		return usrlibs ? cfg.dir[usrdir[curlib][curdir[curlib]]]->lname : nulstr;

	if (!strcmp(sp, "DN")) {
		safe_snprintf(str, maxlen, "%u", usrlibs ? curdir[curlib] + 1 : 0);
		return str;
	}

	if (!strcmp(sp, "DL")) {
		safe_snprintf(str, maxlen, "%-4u", usrlibs ? curdir[curlib] + 1 : 0);
		return str;
	}

	if (!strcmp(sp, "DR")) {
		safe_snprintf(str, maxlen, "%4u", usrlibs ? curdir[curlib] + 1 : 0);
		return str;
	}

	if (!strcmp(sp, "NOACCESS")) {
		if (noaccess_str == text[NoAccessTime])
			safe_snprintf(str, maxlen, noaccess_str, noaccess_val / 60, noaccess_val % 60);
		else if (noaccess_str == text[NoAccessDay])
			safe_snprintf(str, maxlen, noaccess_str, wday[noaccess_val]);
		else
			safe_snprintf(str, maxlen, noaccess_str, noaccess_val);
		return str;
	}

	if (!strcmp(sp, "LAST")) {
		tp = strrchr(useron.alias, ' ');
		if (tp)
			tp++;
		else
			tp = useron.alias;
		return tp;
	}

	if (!strcmp(sp, "REAL") || !strcmp(sp, "FIRSTREAL")) {
		safe_snprintf(str, maxlen, "%s", useron.name);
		tp = strchr(str, ' ');
		if (tp)
			*tp = 0;
		return str;
	}

	if (!strcmp(sp, "LASTREAL")) {
		tp = strrchr(useron.name, ' ');
		if (tp)
			tp++;
		else
			tp = useron.name;
		return tp;
	}

	if (!strcmp(sp, "MAILR")) {
		safe_snprintf(str, maxlen, "%u", getmail(&cfg, useron.number, /* Sent: */ FALSE, /* attr: */ MSG_READ));
		return str;
	}

	if (!strcmp(sp, "MAILU")) {
		safe_snprintf(str, maxlen, "%u", getmail(&cfg, useron.number, /* Sent: */ FALSE, /* attr: */ ~MSG_READ));
		return str;
	}

	if (!strcmp(sp, "MAILW")) {
		safe_snprintf(str, maxlen, "%u", getmail(&cfg, useron.number, /* Sent: */ FALSE, /* attr: */ 0));
		return str;
	}

	if (!strcmp(sp, "MAILP")) {
		safe_snprintf(str, maxlen, "%u", getmail(&cfg, useron.number, /* Sent: */ TRUE, /* attr: */ 0));
		return str;
	}

	if (!strcmp(sp, "SPAMW")) {
		safe_snprintf(str, maxlen, "%u", getmail(&cfg, useron.number, /* Sent: */ FALSE, /* attr: */ MSG_SPAM));
		return str;
	}

	if (!strncmp(sp, "MAILR:", 6) || !strncmp(sp, "MAILR#", 6)) {
		safe_snprintf(str, maxlen, "%u", getmail(&cfg, atoi(sp + 6), /* Sent: */ FALSE, /* attr: */ MSG_READ));
		return str;
	}

	if (!strncmp(sp, "MAILU:", 6) || !strncmp(sp, "MAILU#", 6)) {
		safe_snprintf(str, maxlen, "%u", getmail(&cfg, atoi(sp + 6), /* Sent: */ FALSE, /* attr: */ ~MSG_READ));
		return str;
	}

	if (!strncmp(sp, "MAILW:", 6) || !strncmp(sp, "MAILW#", 6)) {
		safe_snprintf(str, maxlen, "%u", getmail(&cfg, atoi(sp + 6), /* Sent: */ FALSE, /* attr: */ 0));
		return str;
	}

	if (!strncmp(sp, "MAILP:", 6) || !strncmp(sp, "MAILP#", 6)) {
		safe_snprintf(str, maxlen, "%u", getmail(&cfg, atoi(sp + 6), /* Sent: */ TRUE, /* attr: */ 0));
		return str;
	}

	if (!strncmp(sp, "SPAMW:", 6) || !strncmp(sp, "SPAMW#", 6)) {
		safe_snprintf(str, maxlen, "%u", getmail(&cfg, atoi(sp + 6), /* Sent: */ FALSE, /* attr: */ MSG_SPAM));
		return str;
	}

	if (!strcmp(sp, "MSGREPLY")) {
		safe_snprintf(str, maxlen, "%c", cfg.sys_misc & SM_RA_EMU ? 'R' : 'A');
		return str;
	}

	if (!strcmp(sp, "MSGREREAD")) {
		safe_snprintf(str, maxlen, "%c", cfg.sys_misc & SM_RA_EMU ? 'A' : 'R');
		return str;
	}

	if (!strncmp(sp, "STATS.", 6)) {
		getstats(&cfg, 0, &stats);
		sp += 6;
		if (!strcmp(sp, "LOGONS"))
			safe_snprintf(str, maxlen, "%u", stats.logons);
		else if (!strcmp(sp, "LTODAY"))
			safe_snprintf(str, maxlen, "%u", stats.ltoday);
		else if (!strcmp(sp, "TIMEON"))
			safe_snprintf(str, maxlen, "%u", stats.timeon);
		else if (!strcmp(sp, "TTODAY"))
			safe_snprintf(str, maxlen, "%u", stats.ttoday);
		else if (!strcmp(sp, "ULS"))
			safe_snprintf(str, maxlen, "%u", stats.uls);
		else if (!strcmp(sp, "ULB"))
			safe_snprintf(str, maxlen, "%" PRIu64, stats.ulb);
		else if (!strcmp(sp, "DLS"))
			safe_snprintf(str, maxlen, "%u", stats.dls);
		else if (!strcmp(sp, "DLB"))
			safe_snprintf(str, maxlen, "%" PRIu64, stats.dlb);
		else if (!strcmp(sp, "PTODAY"))
			safe_snprintf(str, maxlen, "%u", stats.ptoday);
		else if (!strcmp(sp, "ETODAY"))
			safe_snprintf(str, maxlen, "%u", stats.etoday);
		else if (!strcmp(sp, "FTODAY"))
			safe_snprintf(str, maxlen, "%u", stats.ftoday);
		else if (!strcmp(sp, "NUSERS"))
			safe_snprintf(str, maxlen, "%u", stats.nusers);
		return str;
	}

	/* Message header codes */
	if (!strcmp(sp, "MSG_TO") && current_msg != nullptr) {
		if (pmode != NULL)
			*pmode |= (current_msg->hdr.auxattr & MSG_HFIELDS_UTF8);
		if (current_msg->to_ext != NULL)
			safe_snprintf(str, maxlen, "%s #%s", current_msg_to == nullptr ? current_msg->to : current_msg_to, current_msg->to_ext);
		else if (current_msg->to_net.addr != NULL) {
			char tmp[128];
			safe_snprintf(str, maxlen, "%s (%s)", current_msg_to == nullptr ? current_msg->to : current_msg_to
			              , smb_netaddrstr(&current_msg->to_net, tmp));
		} else
			return current_msg_to == nullptr ? current_msg->to : current_msg_to;
		return str;
	}
	if (!strcmp(sp, "MSG_TO_NAME") && current_msg != nullptr) {
		if (pmode != NULL)
			*pmode |= (current_msg->hdr.auxattr & MSG_HFIELDS_UTF8);
		return current_msg_to == nullptr ? current_msg->to : current_msg_to;
	}
	if (!strcmp(sp, "MSG_TO_FIRST") && current_msg != nullptr) {
		if (pmode != NULL)
			*pmode |= (current_msg->hdr.auxattr & MSG_HFIELDS_UTF8);
		safe_snprintf(str, maxlen, "%s", current_msg_to == nullptr ? current_msg->to : current_msg_to);
		if ((tp = strchr(str, ' ')) != NULL)
			*tp = '\0';
		return str;
	}
	if (!strcmp(sp, "MSG_TO_EXT") && current_msg != NULL) {
		if (current_msg->to_ext == NULL)
			return nulstr;
		return current_msg->to_ext;
	}
	if (!strcmp(sp, "MSG_TO_NET") && current_msg != NULL) {
		if (current_msg->to_net.addr == NULL)
			return nulstr;
		return smb_netaddrstr(&current_msg->to_net, str);
	}
	if (!strcmp(sp, "MSG_TO_NETTYPE") && current_msg != NULL) {
		if (current_msg->to_net.type == NET_NONE)
			return nulstr;
		return smb_nettype((enum smb_net_type)current_msg->to_net.type);
	}
	if (!strcmp(sp, "MSG_CC") && current_msg != NULL)
		return current_msg->cc_list == NULL ? nulstr : current_msg->cc_list;
	if (!strcmp(sp, "MSG_FROM") && current_msg != nullptr) {
		if (current_msg->hdr.attr & MSG_ANONYMOUS && !useron_is_sysop())
			return text[Anonymous];
		if (pmode != NULL)
			*pmode |= (current_msg->hdr.auxattr & MSG_HFIELDS_UTF8);
		if (current_msg->from_ext != NULL)
			safe_snprintf(str, maxlen, "%s #%s", current_msg_from == nullptr ? current_msg->from : current_msg_from, current_msg->from_ext);
		else if (current_msg->from_net.addr != NULL) {
			safe_snprintf(str, maxlen, "%s (%s)", current_msg_from == nullptr ? current_msg->from : current_msg_from
			              , smb_netaddrstr(&current_msg->from_net, tmp));
		} else
			return current_msg_from == nullptr ? current_msg->from : current_msg_from;
		return str;
	}
	if (!strcmp(sp, "MSG_FROM_NAME") && current_msg != nullptr) {
		if (current_msg->hdr.attr & MSG_ANONYMOUS && !useron_is_sysop())
			return text[Anonymous];
		if (pmode != NULL)
			*pmode |= (current_msg->hdr.auxattr & MSG_HFIELDS_UTF8);
		return current_msg_from == nullptr ? current_msg->from : current_msg_from;
	}
	if (!strcmp(sp, "MSG_FROM_FIRST") && current_msg != nullptr) {
		if (current_msg->hdr.attr & MSG_ANONYMOUS && !useron_is_sysop())
			safe_snprintf(str, maxlen, "%s", text[Anonymous]);
		else
			safe_snprintf(str, maxlen, "%s", current_msg_from == nullptr ? current_msg->from : current_msg_from);
		if ((tp = strchr(str, ' ')) != NULL)
			*tp = '\0';
		return str;
	}
	if (!strcmp(sp, "MSG_FROM_EXT") && current_msg != NULL) {
		if (!(current_msg->hdr.attr & MSG_ANONYMOUS) || useron_is_sysop())
			if (current_msg->from_ext != NULL)
				return current_msg->from_ext;
		return nulstr;
	}
	if (!strcmp(sp, "MSG_FROM_NET") && current_msg != NULL) {
		if (current_msg->from_net.addr != NULL
		    && (!(current_msg->hdr.attr & MSG_ANONYMOUS) || useron_is_sysop()))
			return smb_netaddrstr(&current_msg->from_net, str);
		return nulstr;
	}
	if (!strcmp(sp, "MSG_FROM_NETTYPE") && current_msg != NULL) {
		if (current_msg->from_net.type == NET_NONE)
			return nulstr;
		return smb_nettype((enum smb_net_type)current_msg->from_net.type);
	}
	if (!strcmp(sp, "MSG_SUBJECT") && current_msg != nullptr) {
		if (pmode != NULL)
			*pmode |= (current_msg->hdr.auxattr & MSG_HFIELDS_UTF8);
		return current_msg_subj == nullptr ? current_msg->subj : current_msg_subj;
	}
	if (!strcmp(sp, "MSG_SUMMARY") && current_msg != NULL)
		return current_msg->summary == NULL ? nulstr : current_msg->summary;
	if (!strcmp(sp, "MSG_TAGS") && current_msg != NULL)
		return current_msg->tags == NULL ? nulstr : current_msg->tags;
	if (!strcmp(sp, "MSG_DATE") && current_msg != NULL)
		return timestr(smb_time(current_msg->hdr.when_written));
	if (!strcmp(sp, "MSG_DATE_UTC") && current_msg != NULL)
		return timestr(smb_time(current_msg->hdr.when_written) - (smb_tzutc(current_msg->hdr.when_written.zone) * 60));
	if (!strcmp(sp, "MSG_IMP_DATE") && current_msg != NULL)
		return timestr(current_msg->hdr.when_imported.time);
	if (!strcmp(sp, "MSG_AGE") && current_msg != NULL)
		return age_of_posted_item(str, maxlen
		                          , smb_time(current_msg->hdr.when_written) - (smb_tzutc(current_msg->hdr.when_written.zone) * 60));
	if (!strcmp(sp, "MSG_TIMEZONE") && current_msg != NULL)
		return smb_zonestr(current_msg->hdr.when_written.zone, NULL);
	if (!strcmp(sp, "MSG_IMP_TIMEZONE") && current_msg != NULL)
		return smb_zonestr(current_msg->hdr.when_imported.zone, NULL);
	if (!strcmp(sp, "MSG_ATTR") && current_msg != NULL) {
		uint16_t attr = current_msg->hdr.attr;
		uint16_t poll = attr & MSG_POLL_VOTE_MASK;
		uint32_t auxattr = current_msg->hdr.auxattr;
		/* Synchronized with show_msgattr(): */
		safe_snprintf(str, maxlen, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s"
		              , attr & MSG_PRIVATE                       ? "Private  "   :nulstr
		              , attr & MSG_SPAM                          ? "SPAM  "      :nulstr
		              , attr & MSG_READ                          ? "Read  "      :nulstr
		              , attr & MSG_DELETE                        ? "Deleted  "   :nulstr
		              , attr & MSG_KILLREAD                      ? "Kill  "      :nulstr
		              , attr & MSG_ANONYMOUS                     ? "Anonymous  " :nulstr
		              , attr & MSG_LOCKED                        ? "Locked  "    :nulstr
		              , attr & MSG_PERMANENT                     ? "Permanent  " :nulstr
		              , attr & MSG_MODERATED                     ? "Moderated  " :nulstr
		              , attr & MSG_VALIDATED                     ? "Validated  " :nulstr
		              , attr & MSG_REPLIED                       ? "Replied  "   :nulstr
		              , attr & MSG_NOREPLY                       ? "NoReply  "   :nulstr
		              , poll == MSG_POLL                       ? "Poll  "      :nulstr
		              , poll == MSG_POLL && auxattr & POLL_CLOSED    ? "(Closed)  "  :nulstr
		              );
		return str;
	}
	if (!strcmp(sp, "MSG_AUXATTR") && current_msg != NULL) {
		safe_snprintf(str, maxlen, "%s%s%s%s%s%s%s%s"
		              , current_msg->hdr.auxattr & MSG_FILEREQUEST   ? "FileRequest  "   :nulstr
		              , current_msg->hdr.auxattr & MSG_FILEATTACH    ? "FileAttach  "    :nulstr
		              , current_msg->hdr.auxattr & MSG_MIMEATTACH    ? "MimeAttach  "    :nulstr
		              , current_msg->hdr.auxattr & MSG_KILLFILE      ? "KillFile  "      :nulstr
		              , current_msg->hdr.auxattr & MSG_RECEIPTREQ    ? "ReceiptReq  "    :nulstr
		              , current_msg->hdr.auxattr & MSG_CONFIRMREQ    ? "ConfirmReq  "    :nulstr
		              , current_msg->hdr.auxattr & MSG_NODISP        ? "DontDisplay  "   :nulstr
		              , current_msg->hdr.auxattr & MSG_FIXED_FORMAT  ? "FixedFormat "    :nulstr
		              );
		return str;
	}
	if (!strcmp(sp, "MSG_NETATTR") && current_msg != NULL) {
		safe_snprintf(str, maxlen, "%s%s%s%s%s%s%s%s"
		              , current_msg->hdr.netattr & NETMSG_LOCAL          ? "Local  "         :nulstr
		              , current_msg->hdr.netattr & NETMSG_INTRANSIT      ? "InTransit  "     :nulstr
		              , current_msg->hdr.netattr & NETMSG_SENT           ? "Sent  "          :nulstr
		              , current_msg->hdr.netattr & NETMSG_KILLSENT       ? "KillSent  "      :nulstr
		              , current_msg->hdr.netattr & NETMSG_HOLD           ? "Hold  "          :nulstr
		              , current_msg->hdr.netattr & NETMSG_CRASH          ? "Crash  "         :nulstr
		              , current_msg->hdr.netattr & NETMSG_IMMEDIATE      ? "Immediate  "     :nulstr
		              , current_msg->hdr.netattr & NETMSG_DIRECT         ? "Direct  "        :nulstr
		              );
		return str;
	}
	if (!strcmp(sp, "MSG_ID") && current_msg != NULL)
		return current_msg->id == NULL ? nulstr : current_msg->id;
	if (!strcmp(sp, "MSG_REPLY_ID") && current_msg != NULL)
		return current_msg->reply_id == NULL ? nulstr : current_msg->reply_id;
	if (!strcmp(sp, "MSG_NUM") && current_msg != NULL) {
		safe_snprintf(str, maxlen, "%lu", (ulong)current_msg->hdr.number);
		return str;
	}
	if (!strcmp(sp, "MSG_SCORE") && current_msg != NULL) {
		safe_snprintf(str, maxlen, "%ld", (long)current_msg->upvotes - (long)current_msg->downvotes);
		return str;
	}
	if (!strcmp(sp, "MSG_UPVOTES") && current_msg != NULL) {
		safe_snprintf(str, maxlen, "%lu", (ulong)current_msg->upvotes);
		return str;
	}
	if (!strcmp(sp, "MSG_DOWNVOTES") && current_msg != NULL) {
		safe_snprintf(str, maxlen, "%lu", (ulong)current_msg->downvotes);
		return str;
	}
	if (!strcmp(sp, "MSG_TOTAL_VOTES") && current_msg != NULL) {
		safe_snprintf(str, maxlen, "%lu", (ulong)current_msg->total_votes);
		return str;
	}
	if (!strcmp(sp, "MSG_VOTED"))
		return (current_msg != NULL && current_msg->user_voted) ? text[PollAnswerChecked] : nulstr;
	if (!strcmp(sp, "MSG_UPVOTED"))
		return (current_msg != NULL && current_msg->user_voted == 1) ? text[PollAnswerChecked] : nulstr;
	if (!strcmp(sp, "MSG_DOWNVOTED"))
		return (current_msg != NULL && current_msg->user_voted == 2) ? text[PollAnswerChecked] : nulstr;
	if (strcmp(sp, "MSG_THREAD_ID") == 0 && current_msg != NULL) {
		safe_snprintf(str, maxlen, "%lu", (ulong)current_msg->hdr.thread_id);
		return str;
	}
	if (strcmp(sp, "MSG_THREAD_BACK") == 0 && current_msg != NULL) {
		safe_snprintf(str, maxlen, "%lu", (ulong)current_msg->hdr.thread_back);
		return str;
	}
	if (strcmp(sp, "MSG_THREAD_NEXT") == 0 && current_msg != NULL) {
		safe_snprintf(str, maxlen, "%lu", (ulong)current_msg->hdr.thread_next);
		return str;
	}
	if (strcmp(sp, "MSG_THREAD_FIRST") == 0 && current_msg != NULL) {
		safe_snprintf(str, maxlen, "%lu", (ulong)current_msg->hdr.thread_first);
		return str;
	}
	if (!strcmp(sp, "SMB_AREA")) {
		if (subnum_is_valid(smb.subnum))
			safe_snprintf(str, maxlen, "%s %s"
			              , cfg.grp[cfg.sub[smb.subnum]->grp]->sname
			              , cfg.sub[smb.subnum]->sname);
		else
			strncpy(str, "Email", maxlen);
		return str;
	}
	if (!strcmp(sp, "SMB_AREA_DESC")) {
		if (subnum_is_valid(smb.subnum))
			safe_snprintf(str, maxlen, "%s %s"
			              , cfg.grp[cfg.sub[smb.subnum]->grp]->lname
			              , cfg.sub[smb.subnum]->lname);
		else
			strncpy(str, "Personal Email", maxlen);
		return str;
	}
	if (!strcmp(sp, "SMB_GROUP")) {
		if (subnum_is_valid(smb.subnum))
			return cfg.grp[cfg.sub[smb.subnum]->grp]->sname;
		return nulstr;
	}
	if (!strcmp(sp, "SMB_GROUP_DESC")) {
		if (subnum_is_valid(smb.subnum))
			return cfg.grp[cfg.sub[smb.subnum]->grp]->lname;
		return nulstr;
	}
	if (!strcmp(sp, "SMB_GROUP_NUM")) {
		if (subnum_is_valid(smb.subnum))
			safe_snprintf(str, maxlen, "%u", getusrgrp(smb.subnum));
		return str;
	}
	if (!strcmp(sp, "SMB_SUB")) {
		if (smb.subnum == INVALID_SUB)
			return "Mail";
		else if (subnum_is_valid(smb.subnum))
			return cfg.sub[smb.subnum]->sname;
		return nulstr;
	}
	if (!strcmp(sp, "SMB_SUB_DESC")) {
		if (smb.subnum == INVALID_SUB)
			return "Mail";
		else if (subnum_is_valid(smb.subnum))
			return cfg.sub[smb.subnum]->lname;
		return nulstr;
	}
	if (!strcmp(sp, "SMB_SUB_CODE")) {
		if (smb.subnum == INVALID_SUB)
			return "MAIL";
		else if (subnum_is_valid(smb.subnum))
			return cfg.sub[smb.subnum]->code;
		return nulstr;
	}
	if (!strcmp(sp, "SMB_SUB_NUM")) {
		if (subnum_is_valid(smb.subnum))
			safe_snprintf(str, maxlen, "%u", getusrsub(smb.subnum));
		return str;
	}
	if (!strcmp(sp, "SMB_MSGS")) {
		safe_snprintf(str, maxlen, "%lu", (ulong)smb.msgs);
		return str;
	}
	if (!strcmp(sp, "SMB_CURMSG")) {
		safe_snprintf(str, maxlen, "%lu", (ulong)(smb.curmsg + 1));
		return str;
	}
	if (!strcmp(sp, "SMB_LAST_MSG")) {
		safe_snprintf(str, maxlen, "%lu", (ulong)smb.status.last_msg);
		return str;
	}
	if (!strcmp(sp, "SMB_MAX_MSGS")) {
		safe_snprintf(str, maxlen, "%lu", (ulong)smb.status.max_msgs);
		return str;
	}
	if (!strcmp(sp, "SMB_MAX_CRCS")) {
		safe_snprintf(str, maxlen, "%lu", (ulong)smb.status.max_crcs);
		return str;
	}
	if (!strcmp(sp, "SMB_MAX_AGE")) {
		safe_snprintf(str, maxlen, "%hu", smb.status.max_age);
		return str;
	}
	if (!strcmp(sp, "SMB_TOTAL_MSGS")) {
		safe_snprintf(str, maxlen, "%lu", (ulong)smb.status.total_msgs);
		return str;
	}

	/* Currently viewed file */
	if (current_file != NULL) {
		if (dirnum_is_valid(current_file->dir)) {
			if (strcmp(sp, "FILE_AREA") == 0) {
				safe_snprintf(str, maxlen, "%s %s"
				              , cfg.lib[cfg.dir[current_file->dir]->lib]->sname
				              , cfg.dir[current_file->dir]->sname);
				return str;
			}
			if (strcmp(sp, "FILE_AREA_DESC") == 0) {
				safe_snprintf(str, maxlen, "%s %s"
				              , cfg.lib[cfg.dir[current_file->dir]->lib]->lname
				              , cfg.dir[current_file->dir]->lname);
				return str;
			}
			if (strcmp(sp, "FILE_LIB") == 0)
				return cfg.lib[cfg.dir[current_file->dir]->lib]->sname;
			if (strcmp(sp, "FILE_LIB_DESC") == 0)
				return cfg.lib[cfg.dir[current_file->dir]->lib]->lname;
			if (strcmp(sp, "FILE_LIB_NUM") == 0) {
				safe_snprintf(str, maxlen, "%u", getusrlib(current_file->dir));
				return str;
			}
			if (strcmp(sp, "FILE_DIR") == 0)
				return cfg.dir[current_file->dir]->sname;
			if (strcmp(sp, "FILE_DIR_DESC") == 0)
				return cfg.dir[current_file->dir]->lname;
			if (strcmp(sp, "FILE_DIR_CODE") == 0)
				return cfg.dir[current_file->dir]->code;
			if (strcmp(sp, "FILE_DIR_NUM") == 0) {
				safe_snprintf(str, maxlen, "%u", getusrdir(current_file->dir));
				return str;
			}
			if (strcmp(sp, "FILE_FTP_PATH") == 0) {
				getfilevpath(&cfg, current_file, str, maxlen);
				return str;
			}
			if (strcmp(sp, "FILE_WEB_PATH") == 0) {
				const char* p = "";
				if (cfg.dir[current_file->dir]->vshortcut[0] == '\0') {
					p = startup->web_file_vpath_prefix;
					if (*p == '/')
						++p;
				}
				safe_snprintf(str, maxlen, "%s%s", p, getfilevpath(&cfg, current_file, tmp, sizeof tmp));
				return str;
			}
			if (strcmp(sp, "FILE_COST") == 0) {
				strlcpy(str, (cfg.dir[current_file->dir]->misc & DIR_FREE)
				        || (getfilesize(&cfg, current_file) > 0 && current_file->cost <= 0)
				        ? text[FREE] : u64toac(current_file->cost, tmp), maxlen);
				return str;
			}
		}
		if (strcmp(sp, "FILE_NAME") == 0)
			return current_file->name;
		if (strcmp(sp, "FILE_DESC") == 0)
			return current_file->desc == nullptr ? nulstr : current_file->desc;
		if (strcmp(sp, "FILE_TAGS") == 0)
			return current_file->tags == nullptr ? nulstr : current_file->tags;
		if (strcmp(sp, "FILE_AUTHOR") == 0)
			return current_file->author == nullptr ? nulstr : current_file->author;
		if (strcmp(sp, "FILE_GROUP") == 0)
			return current_file->author_org == nullptr ? nulstr : current_file->author_org;
		if (strcmp(sp, "FILE_UPLOADER") == 0)
			return (current_file->hdr.attr & MSG_ANONYMOUS) ? text[UNKNOWN_USER]
			    : (current_file->from == nullptr ? nulstr : current_file->from);
		if (strcmp(sp, "FILE_BYTES") == 0) {
			safe_snprintf(str, maxlen, "%" PRIi64, getfilesize(&cfg, current_file));
			return str;
		}
		if (strcmp(sp, "FILE_SIZE") == 0)
			return byte_estimate_to_str(getfilesize(&cfg, current_file), str, maxlen, /* units: */ 1024, /* precision: */ 1);
		if (strcmp(sp, "FILE_CREDITS") == 0) {
			safe_snprintf(str, maxlen, "%" PRIu64, current_file->cost);
			return str;
		}
		if (strcmp(sp, "FILE_CRC32") == 0) {
			if ((current_file->file_idx.hash.flags & SMB_HASH_CRC32)
			    && getfilesize(&cfg, current_file) > 0
			    && (uint64_t)getfilesize(&cfg, current_file) == smb_getfilesize(&current_file->idx)) {
				snprintf(str, maxlen, "%08x", current_file->file_idx.hash.data.crc32);
				return str;
			}
			return nulstr;
		}
		if (strcmp(sp, "FILE_MD5") == 0) {
			if ((current_file->file_idx.hash.flags & SMB_HASH_MD5)
			    && getfilesize(&cfg, current_file) > 0
			    && (uint64_t)getfilesize(&cfg, current_file) == smb_getfilesize(&current_file->idx)) {
				strlcpy(str, MD5_hex(tmp, current_file->file_idx.hash.data.md5), maxlen);
				return str;
			}
			return nulstr;
		}
		if (strcmp(sp, "FILE_SHA1") == 0) {
			if ((current_file->file_idx.hash.flags & SMB_HASH_SHA1)
			    && getfilesize(&cfg, current_file) > 0
			    && (uint64_t)getfilesize(&cfg, current_file) == smb_getfilesize(&current_file->idx)) {
				strlcpy(str, SHA1_hex(tmp, current_file->file_idx.hash.data.sha1), maxlen);
				return str;
			}
			return nulstr;
		}

		if (strcmp(sp, "FILE_TIME") == 0)
			return timestr(getfiletime(&cfg, current_file));
		if (strcmp(sp, "FILE_TIME_ULED") == 0)
			return timestr(current_file->hdr.when_imported.time);
		if (strcmp(sp, "FILE_TIME_DLED") == 0)
			return timestr(current_file->hdr.last_downloaded);
		if (strcmp(sp, "FILE_DATE") == 0)
			return datestr(getfiletime(&cfg, current_file));
		if (strcmp(sp, "FILE_DATE_ULED") == 0)
			return datestr(current_file->hdr.when_imported.time);
		if (strcmp(sp, "FILE_DATE_DLED") == 0)
			return datestr(current_file->hdr.last_downloaded);

		if (strcmp(sp, "FILE_TIMES_DLED") == 0) {
			safe_snprintf(str, maxlen, "%lu", (ulong)current_file->hdr.times_downloaded);
			return str;
		}
		if (strcmp(sp, "FILE_TIME_TO_DL") == 0) {
			safe_snprintf(str, maxlen, "%s", sectostr(gettimetodl(&cfg, current_file, cur_cps), tmp));
			return str;
		}
	}

	return get_text(sp);
}

char* sbbs_t::expand_atcodes(const char* src, char* buf, size_t size, const smbmsg_t* msg)
{
	char*           dst = buf;
	char*           end = dst + (size - 1);
	const smbmsg_t* saved_msg{current_msg};

	if (msg != NULL)
		current_msg = msg;
	while (*src != '\0' && dst < end) {
		if (*src == '@') {
			char        str[32];
			SAFECOPY(str, src + 1);
			char*       at = strchr(str, '@');
			const char* sp = strchr(str, ' ');
			if (at != NULL && (sp == NULL || sp > at)) {
				char        tmp[128];
				*at = '\0';
				src += strlen(str) + 2;
				const char* p = formatted_atcode(str, tmp, sizeof tmp);
				if (p != NULL)
					dst += strlcpy(dst, p, end - dst);
				continue;
			}
		}
		*(dst++) = *(src++);
	}
	if (dst > end)
		dst = end;
	*dst = '\0';
	current_msg = saved_msg;

	return buf;
}
