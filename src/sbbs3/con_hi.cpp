/* Synchronet hi-level console routines */

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
#include "utf8.h"

/****************************************************************************/
/* Redraws str using i as current cursor position and l as length           */
/* Currently only used by getstr() - so should be moved to getstr.cpp?		*/
/****************************************************************************/
void sbbs_t::redrwstr(char *strin, int i, int l, int mode)
{
	if (i <= 0)
		i = 0;
	else
		term->cursor_left(i);
	if (l < 0)
		l = 0;
	if (mode)
		bprintf(mode, "%-*.*s", l, l, strin);
	else
		rprintf("%-*.*s", l, l, strin);
	term->cleartoeol();
	if (i < l) {
		auto_utf8(strin, mode);
		if (mode & P_UTF8)
			l = utf8_str_total_width(strin, unicode_zerowidth);
		term->cursor_left(l - i);
	}
}

int sbbs_t::uselect(bool add, uint num, const char *title, const char *name, const uchar *ar)
{
	if (title != nullptr)
		uselect_title = title;

	if (add) {
		if (name == nullptr)
			return -1;
		if (ar != nullptr && !chk_ar(ar, &useron, &client))
			return 0;
		uselect_item item = { name, num };
		uselect_items.emplace_back(item);
		return 0;
	}

	if (uselect_items.size() < 1)
		return -1;

	bool invoked;
	int retval = exec_mod("select item", cfg.uselect_mod, &invoked, "%u", num);
	if (!invoked) {
		bprintf(text[SelectItemHdr], uselect_title.c_str());
		int dflt = 0;
		for (auto u = 0; u < static_cast<int>(uselect_items.size()); ++u) {
			bprintf(text[SelectItemFmt], u + 1, uselect_items[u].name.c_str());
			term->add_hotspot(u + 1);
			if (uselect_items[u].num == num)
				dflt = u;
		}

		char str[128];
		snprintf(str, sizeof str, text[SelectItemWhich], dflt + 1);
		mnemonics(str);
		int i = getnum(uselect_items.size());
		term->clear_hotspots();
		retval = -1;
		if (i == 0)                    // User hit ENTER, use default
			retval = num;
		else if (i > 0 && i <= static_cast<int>(uselect_items.size()))
			retval = uselect_items[i - 1].num;
	}
	uselect_items.clear();
	return retval;
}

unsigned count_set_bits(long val)
{
	unsigned count = 0;

	while (val) {
		val &= val - 1;
		count++;
	}
	return count;
}

int sbbs_t::mselect(const char *hdr, str_list_t list, unsigned max_selections, const char* item_fmt
                    , const char* selected_str, const char* unselected_str, const char* prompt_fmt)
{
	char prompt[128];
	int  i;
	int  max_item_len = 0;
	int  selected = 0;

	for (i = 0; list[i] != NULL; i++) {
		int len = strlen(list[i]);
		if (len > max_item_len)
			max_item_len = len;
	}
	unsigned items = i;
	if (max_selections > items)
		max_selections = items;
	while (online) {
		bputs(hdr);
		for (i = 0; list[i] != NULL; i++)
			bprintf(item_fmt, i + 1, max_item_len, max_item_len, list[i]
			        , (selected & (1 << i)) ? selected_str : unselected_str);
		SAFEPRINTF(prompt, prompt_fmt, max_selections);
		mnemonics(prompt);
		i = getnum(items);
		if (i < 0)
			return 0;
		if (i == 0)
			break;
		if (i && i <= (int)items && (selected & (1 << (i - 1)) || count_set_bits(selected) < max_selections))
			selected ^= 1 << (i - 1);
	}
	return selected;
}


/****************************************************************************/
/* Prompts user for System Password. Returns 1 if user entered correct PW	*/
/****************************************************************************/
bool sbbs_t::chksyspass(const char* sys_pw)
{
	char str[256];

	if (online == ON_REMOTE && !(cfg.sys_misc & SM_R_SYSOP)) {
		logline(LOG_NOTICE, "S!", "Remote sysop access disabled");
		return false;
	}
	if (time(NULL) - last_sysop_auth < cfg.sys_pass_timeout * 60)
		return true;
	if (sys_pw != NULL)
		SAFECOPY(str, sys_pw);
	else {
		bputs(text[SystemPassword]);
		getstr(str, sizeof(cfg.sys_pass) - 1, K_UPPER | K_NOECHO);
		term->newline();
		term->lncntr = 0;
	}
	if (stricmp(cfg.sys_pass, str)) {
		if (cfg.sys_misc & SM_ECHO_PW)
			llprintf(LOG_NOTICE, "S!", "%s #%u System password attempt: '%s'"
			            , useron.alias, useron.number, str);
		else
			llprintf(LOG_NOTICE, "S!", "System password verification failure");
		return false;
	}
	last_sysop_auth = time(NULL);
	return true;
}
