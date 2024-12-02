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
	if(i <= 0)
		i = 0;
	else
		cursor_left(i);
	if(l < 0)
		l = 0;
	if(mode)
		bprintf(mode, "%-*.*s",l,l,strin);
	else
		column+=rprintf("%-*.*s",l,l,strin);
	cleartoeol();
	if(i<l) {
		auto_utf8(strin, mode);
		if(mode&P_UTF8)
			l = utf8_str_total_width(strin, unicode_zerowidth);
		cursor_left(l-i);
	}
}

int sbbs_t::uselect(bool add, uint n, const char *title, const char *item, const uchar *ar)
{
	char	str[128];
	int		i;
	uint	t,u;

	if(uselect_total>=sizeof(uselect_num)/sizeof(uselect_num[0]))	/* out of bounds */
		uselect_total=0;

	if(add) {
		if(ar && !chk_ar(ar,&useron,&client))
			return(0);
		if(!uselect_total)
			bprintf(text[SelectItemHdr],title);
		uselect_num[uselect_total++]=n;
		add_hotspot(uselect_total);
		bprintf(text[SelectItemFmt],uselect_total,item);
		return(0); 
	}

	if(!uselect_total)
		return(-1);

	for(u=0;u<uselect_total;u++)
		if(uselect_num[u]==n)
			break;
	if(u==uselect_total)
		u=0;
	snprintf(str, sizeof str, text[SelectItemWhich],u+1);
	mnemonics(str);
	i=getnum(uselect_total);
	t=uselect_total;
	uselect_total=0;
	clear_hotspots();
	if(i<0)
		return(-1);
	if(!i) {					/* User hit ENTER, use default */
		for(u=0;u<t;u++)
			if(uselect_num[u]==n)
				return(uselect_num[u]);
		if(n<t)
			return(uselect_num[n]);
		return(-1); 
	}
	return(uselect_num[i-1]);
}

unsigned count_set_bits(long val)
{
	unsigned count = 0;

	while(val) {
		val &= val-1;
		count++;
	}
	return count;
}

int sbbs_t::mselect(const char *hdr, str_list_t list, unsigned max_selections, const char* item_fmt
	,const char* selected_str, const char* unselected_str, const char* prompt_fmt)
{
	char	prompt[128];
	int		i;
	int		max_item_len = 0;
	int		selected = 0;

	for(i=0; list[i] != NULL; i++) {
		int len = strlen(list[i]);
		if(len > max_item_len)
			max_item_len = len;
	}
	unsigned items = i;
	if(max_selections > items)
		max_selections = items;
	while(online) {
		bputs(hdr);
		for(i=0; list[i] != NULL; i++)
			bprintf(item_fmt, i+1, max_item_len, max_item_len, list[i]
				, (selected & (1<<i)) ? selected_str : unselected_str);
		SAFEPRINTF(prompt, prompt_fmt, max_selections);
		mnemonics(prompt);
		i=getnum(items);
		if(i < 0)
			return 0;
		if(i == 0)
			break;
		if(i && i <= (int)items && (selected&(1<<(i-1)) || count_set_bits(selected) < max_selections))
			selected ^= 1<<(i-1);
	}
	return selected;
}


/****************************************************************************/
/* Prompts user for System Password. Returns 1 if user entered correct PW	*/
/****************************************************************************/
bool sbbs_t::chksyspass(const char* sys_pw)
{
	char	str[256],str2[256];

	if(online==ON_REMOTE && !(cfg.sys_misc&SM_R_SYSOP)) {
		logline(LOG_NOTICE,"S!","Remote sysop access disabled");
		return(false);
	}
	if(time(NULL) - last_sysop_auth < cfg.sys_pass_timeout * 60)
		return true;
	if(sys_pw != NULL)
		SAFECOPY(str, sys_pw);
	else {
		bputs(text[SystemPassword]);
		getstr(str, sizeof(cfg.sys_pass)-1, K_UPPER | K_NOECHO);
		CRLF;
		lncntr=0;
	}
	if(stricmp(cfg.sys_pass,str)) {
		if(cfg.sys_misc&SM_ECHO_PW) 
			SAFEPRINTF3(str2,"%s #%u System password attempt: '%s'"
				,useron.alias,useron.number,str);
		else
			SAFECOPY(str2,"System password verification failure");
		logline(LOG_NOTICE,"S!",str2);
		return(false); 
	}
	last_sysop_auth = time(NULL);
	return(true);
}
