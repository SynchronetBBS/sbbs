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

#if defined(_WINSOCKAPI_)
	extern WSADATA WSAData;
	#define SOCKLIB_DESC WSAData.szDescription
#else
	#define	SOCKLIB_DESC NULL
#endif

static char* separate_thousands(const char* src, char *dest, size_t maxlen, char sep)
{
	if(strlen(src) * 1.3 > maxlen)
		return (char*)src;
	const char* tail = src;
	while(*tail && IS_DIGIT(*tail))
		tail++;
	if(tail == src)
		return (char*)src;
	size_t digits = tail - src;
	char* d = dest;
	for(size_t i = 0; i < digits; d++, i++) {
		*d = src[i];
		if(i && i + 3 < digits && (digits - (i + 1)) % 3 == 0)
			*(++d) = sep;
	}
	*d = 0;
	strcpy(d, tail);
	return dest;
}

/****************************************************************************/
/* Returns 0 if invalid @ code. Returns length of @ code if valid.          */
/****************************************************************************/
int sbbs_t::show_atcode(const char *instr, JSObject* obj)
{
	char	str[128],str2[128],*tp,*sp,*p;
    int     len;
	int		disp_len;
	enum {
		none,
		left,
		right,
		center
	} align = none;
	bool	zero_padded=false;
	bool	truncated = true;
	bool	doubled = false;
	bool	thousep = false;	// thousands-separated
	bool	uppercase = false;
	bool	width_specified = false;
	long	pmode = 0;
	const char *cp;

	if(*instr != '@')
		return 0;
	SAFECOPY(str,instr);
	tp=strchr(str+1,'@');
	if(!tp)                 /* no terminating @ */
		return(0);
	sp=strchr(str+1,' ');
	if(sp && sp<tp)         /* space before terminating @ */
		return(0);
	len=(tp-str)+1;
	(*tp)=0;
	sp=(str+1);

	if(*sp == '~' && *(sp + 1)) {	// Mouse hot-spot (hungry)
		sp++;
		tp = strchr(sp + 1, '~');
		if(tp == NULL)
			tp = sp;
		else {
			*tp = 0;
			tp++;
		}
		c_unescape_str(tp);
		add_hotspot(tp, /* hungry: */true, column, column + strlen(sp) - 1, row);
		bputs(sp);
		return len;
	}

	if(*sp == '`' && *(sp + 1)) {	// Mouse hot-spot (strict)
		sp++;
		tp = strchr(sp + 1, '`');
		if(tp == NULL)
			tp = sp;
		else {
			*tp = 0;
			tp++;
		}
		c_unescape_str(tp);
		add_hotspot(tp, /* hungry: */false, column, column + strlen(sp) - 1, row);
		bputs(sp);
		return len;
	}

	disp_len=len;
	if((p = strchr(sp, '|')) != NULL) {
		if(strchr(p, 'T') != NULL)
			thousep = true;
		if(strchr(p, 'U') != NULL)
			uppercase = true;
		if(strchr(p, 'L') != NULL)
			align = left;
		else if(strchr(p, 'R') != NULL)
			align = right;
		else if(strchr(p, 'C') != NULL)
			align = center;
		else if(strchr(p, 'W') != NULL)
			doubled = true;
		else if(strchr(p, 'Z') != NULL)
			zero_padded = true;
		else if(strchr(p, '>') != NULL)
			truncated = false;
	}
	else if(strchr(sp, ':') != NULL)
		p = NULL;
	else if((p=strstr(sp,"-L"))!=NULL)
		align = left;
	else if((p=strstr(sp,"-R"))!=NULL)
		align = right;
	else if((p=strstr(sp,"-C"))!=NULL)
		align = center;
	else if((p=strstr(sp,"-W"))!=NULL)	/* wide */
		doubled=true;
	else if((p=strstr(sp,"-Z"))!=NULL)
		zero_padded=true;
	else if((p=strstr(sp,"-T"))!=NULL)
		thousep=true;
	else if((p=strstr(sp,"-U"))!=NULL)
		uppercase=true;
	else if((p=strstr(sp,"->"))!=NULL)	/* wrap */
		truncated = false;
	if(p!=NULL) {
		char* lp = p;
		lp++;	// skip the '|' or '-'
		while(*lp == '>'|| IS_ALPHA(*lp))
			lp++;
		if(*lp)
			width_specified = true;
		while(*lp && !IS_DIGIT(*lp))
			lp++;
		if(*lp && IS_DIGIT(*lp)) {
			disp_len=atoi(lp);
			width_specified = true;
		}
		*p=0;
	}

	cp = atcode(sp, str2, sizeof(str2), &pmode, align == center, obj);
	if(cp==NULL)
		return(0);

	char separated[128];
	if(thousep)
		cp = separate_thousands(cp, separated, sizeof(separated), ',');

	char upper[128];
	if(uppercase) {
		SAFECOPY(upper, cp);
		strupr(upper);
		cp = upper;
	}

	if(p==NULL || truncated == false || (width_specified == false && align == none))
		disp_len = strlen(cp);

	if(uppercase && align == none)
		align = left;

	if(truncated && strchr(cp, '\n') == NULL) {
		if(column + disp_len > cols - 1) {
			if(column >= cols - 1)
				disp_len = 0;
			else
				disp_len = (cols - 1) - column;
		}
	}
	if(pmode & P_UTF8) {
		if(term_supports(UTF8))
			disp_len += strlen(cp) - utf8_str_total_width(cp);
		else
			disp_len += strlen(cp) - utf8_str_count_width(cp, /* min: */1, /* max: */2);
	}
	if(align == left)
		bprintf(pmode, "%-*.*s",disp_len,disp_len,cp);
	else if(align == right)
		bprintf(pmode, "%*.*s",disp_len,disp_len,cp);
	else if(align == center) {
		int vlen = strlen(cp);
		if(vlen < disp_len) {
			int left = (disp_len - vlen) / 2;
			bprintf(pmode, "%*s%-*s", left, "", disp_len - left, cp);
		} else
			bprintf(pmode, "%.*s", disp_len, cp);
	} else if(doubled) {
		wide(cp);
	} else if(zero_padded) {
		int vlen = strlen(cp);
		if(vlen < disp_len)
			bprintf(pmode, "%-.*s%s", (int)(disp_len - strlen(cp)), "0000000000", cp);
		else
			bprintf(pmode, "%.*s", disp_len, cp);
	} else
		bprintf(pmode, "%.*s", disp_len, cp);

	return(len);
}

static const char* getpath(scfg_t* cfg, const char* path)
{
	for(int i = 0; i < cfg->total_dirs; i++) {
		if(stricmp(cfg->dir[i]->code, path) == 0)
			return cfg->dir[i]->path;
	}
	return path;
}

const char* sbbs_t::atcode(char* sp, char* str, size_t maxlen, long* pmode, bool centered, JSObject* obj)
{
	char*	tp = NULL;
	uint	i;
	uint	ugrp;
	uint	usub;
	long	l;
    stats_t stats;
    node_t  node;
	struct	tm tm;

	str[0]=0;

	if(strcmp(sp, "SHOW") == 0) {
		console &= ~CON_ECHO_OFF;
		return nulstr;
	}
	if(strncmp(sp, "SHOW:", 5) == 0) {
		uchar* ar = arstr(NULL, sp + 5, &cfg, NULL);
		if(ar != NULL) {
			if(!chk_ar(ar, &useron, &client))
				console |= CON_ECHO_OFF;
			else
				console &= ~CON_ECHO_OFF;
			free(ar);
		}
		return nulstr;
	}

	if(strcmp(sp, "HOT") == 0) { // Auto-mouse hot-spot attribute
		hot_attr = curatr;
		return nulstr;
	}
	if(strncmp(sp, "HOT:", 4) == 0) {	// Auto-mouse hot-spot attribute
		sp += 4;
		if(stricmp(sp, "hungry") == 0) {
			hungry_hotspots = true;
			hot_attr = curatr;
		}
		else if(stricmp(sp, "strict") == 0) {
			hungry_hotspots = false;
			hot_attr = curatr;
		}
		else if(stricmp(sp, "off") == 0)
			hot_attr = 0;
		else
			hot_attr = attrstr(sp);
		return nulstr;
	}
	if(strcmp(sp, "CLEAR_HOT") == 0) {
		clear_hotspots();
		return nulstr;
	}

	if(strncmp(sp, "U+", 2) == 0) {	// UNICODE
		enum unicode_codepoint codepoint = (enum unicode_codepoint)strtoul(sp + 2, &tp, 16);
		if(tp == NULL || *tp == 0)
			outchar(codepoint, unicode_to_cp437(codepoint));
		else if(*tp == ':')
			outchar(codepoint, tp + 1);
		else {
			char fallback = (char)strtoul(tp + 1, NULL, 16);
			if(*tp == ',')
				outchar(codepoint, fallback);
			else if(*tp == '!') {
				char ch = unicode_to_cp437(codepoint);
				if(ch != 0)
					fallback = ch;
				outchar(codepoint, fallback);
			}
			else return NULL; // Invalid @-code
		}
		return nulstr;
	}

	if(strcmp(sp, "CHECKMARK") == 0) {
		outchar(UNICODE_CHECK_MARK, CP437_CHECK_MARK);
		return nulstr;
	}

	if(strcmp(sp, "ELLIPSIS") == 0) {
		outchar(UNICODE_HORIZONTAL_ELLIPSIS, "...");
		return nulstr;
	}
	if(strcmp(sp, "COPY") == 0) {
		outchar(UNICODE_COPYRIGHT_SIGN, "(C)");
		return nulstr;
	}
	if(strcmp(sp, "SOUNDCOPY") == 0) {
		outchar(UNICODE_SOUND_RECORDING_COPYRIGHT, "(P)");
		return nulstr;
	}
	if(strcmp(sp, "REGISTERED") == 0) {
		outchar(UNICODE_REGISTERED_SIGN, "(R)");
		return nulstr;
	}
	if(strcmp(sp, "TRADEMARK") == 0) {
		outchar(UNICODE_TRADE_MARK_SIGN, "(TM)");
		return nulstr;
	}
	if(strcmp(sp, "DEGREE_C") == 0) {
		outchar(UNICODE_DEGREE_CELSIUS, "\xF8""C");
		return nulstr;
	}
	if(strcmp(sp, "DEGREE_F") == 0) {
		outchar(UNICODE_DEGREE_FAHRENHEIT, "\xF8""F");
		return nulstr;
	}

	if(strncmp(sp, "WIDE:", 5) == 0) {
		wide(sp + 5);
		return(nulstr);
	}

	if(!strcmp(sp,"VER"))
		return(VERSION);

	if(!strcmp(sp,"REV")) {
		safe_snprintf(str,maxlen,"%c",REVISION);
		return(str);
	}

	if(!strcmp(sp,"FULL_VER")) {
		safe_snprintf(str,maxlen,"%s%c%s",VERSION,REVISION,beta_version);
		truncsp(str);
#if defined(_DEBUG)
		strcat(str," Debug");
#endif
		return(str);
	}

	if(!strcmp(sp,"VER_NOTICE"))
		return(VERSION_NOTICE);

	if(!strcmp(sp,"OS_VER"))
		return(os_version(str));

#ifdef JAVASCRIPT
	if(!strcmp(sp,"JS_VER"))
		return((char *)JS_GetImplementationVersion());
#endif

	if(!strcmp(sp,"PLATFORM"))
		return(PLATFORM_DESC);

	if(!strcmp(sp,"COPYRIGHT"))
		return(COPYRIGHT_NOTICE);

	if(!strcmp(sp,"COMPILER")) {
		char compiler[32];
		DESCRIBE_COMPILER(compiler);
		strncpy(str, compiler, maxlen);
		return(str);
	}

	if(strcmp(sp, "GIT_HASH") == 0)
		return git_hash;

	if(strcmp(sp, "GIT_BRANCH") == 0)
		return git_branch;

	if(!strcmp(sp,"UPTIME")) {
		extern volatile time_t uptime;
		time_t up=0;
		now = time(NULL);
		if (uptime != 0 && now >= uptime)
			up = now-uptime;
		char   days[64]="";
		if((up/(24*60*60))>=2) {
	        sprintf(days,"%lu days ",(ulong)(up/(24L*60L*60L)));
			up%=(24*60*60);
		}
		safe_snprintf(str,maxlen,"%s%lu:%02lu"
	        ,days
			,(ulong)(up/(60L*60L))
			,(ulong)((up/60L)%60L)
			);
		return(str);
	}

	if(!strcmp(sp,"SERVED")) {
		extern volatile ulong served;
		safe_snprintf(str,maxlen,"%lu",served);
		return(str);
	}

	if(!strcmp(sp,"SOCKET_LIB"))
		return(socklib_version(str,SOCKLIB_DESC));

	if(!strcmp(sp,"MSG_LIB")) {
		safe_snprintf(str,maxlen,"SMBLIB %s",smb_lib_ver());
		return(str);
	}

	if(!strcmp(sp,"BBS") || !strcmp(sp,"BOARDNAME"))
		return(cfg.sys_name);

	if(!strcmp(sp,"BAUD") || !strcmp(sp,"BPS")) {
		safe_snprintf(str,maxlen,"%lu",cur_output_rate ? cur_output_rate : cur_rate);
		return(str);
	}

	if(!strcmp(sp,"COLS")) {
		safe_snprintf(str,maxlen,"%lu",cols);
		return(str);
	}
	if(!strcmp(sp,"ROWS")) {
		safe_snprintf(str,maxlen,"%lu",rows);
		return(str);
	}
	if(strcmp(sp,"TERM") == 0)
		return term_type();

	if(strcmp(sp,"CHARSET") == 0)
		return term_charset();

	if(!strcmp(sp,"CONN"))
		return(connection);

	if(!strcmp(sp,"SYSOP"))
		return(cfg.sys_op);

	if(strcmp(sp, "SYSAVAIL") == 0)
		return text[sysop_available(&cfg) ? LiSysopAvailable : LiSysopNotAvailable];

	if(strcmp(sp, "SYSAVAILYN") == 0)
		return text[sysop_available(&cfg) ? Yes : No];

	if(!strcmp(sp,"LOCATION"))
		return(cfg.sys_location);

	if(strcmp(sp,"NODE") == 0 || strcmp(sp,"NN") == 0) {
		safe_snprintf(str,maxlen,"%u",cfg.node_num);
		return(str);
	}
	if(strcmp(sp, "TNODES") == 0 || strcmp(sp, "TNODE") == 0 || strcmp(sp, "TN") == 0) {
		safe_snprintf(str,maxlen,"%u",cfg.sys_nodes);
		return(str);
	}
	if(strcmp(sp, "ANODES") == 0 || strcmp(sp, "ANODE") == 0 || strcmp(sp, "AN") == 0) {
		safe_snprintf(str, maxlen, "%u", count_nodes(/* self: */true));
		return str;
	}
	if(strcmp(sp, "ONODES") == 0 || strcmp(sp, "ONODE") == 0 || strcmp(sp, "ON") == 0) {
		safe_snprintf(str, maxlen, "%u", count_nodes(/* self: */false));
		return str;
	}

	if(strcmp(sp, "PWDAYS") == 0) {
		if(cfg.sys_pwdays) {
			safe_snprintf(str, maxlen, "%u", cfg.sys_pwdays);
			return str;
		}
		return text[Unlimited];
	}

	if(strcmp(sp, "AUTODEL") == 0) {
		if(cfg.sys_autodel) {
			safe_snprintf(str, maxlen, "%u", cfg.sys_autodel);
			return str;
		}
		return text[Unlimited];
	}

	if(strcmp(sp, "PAGER") == 0)
		return (thisnode.misc&NODE_POFF) ? text[Off] : text[On];

	if(strcmp(sp, "ALERTS") == 0)
		return (thisnode.misc&NODE_AOFF) ? text[Off] : text[On];

	if(strcmp(sp, "SPLITP") == 0)
		return (useron.chat&CHAT_SPLITP) ? text[On] : text[Off];

	if(!strcmp(sp,"INETADDR"))
		return(cfg.sys_inetaddr);

	if(!strcmp(sp,"HOSTNAME"))
		return server_host_name();

	if(!strcmp(sp,"FIDOADDR")) {
		if(cfg.total_faddrs)
			return(smb_faddrtoa(&cfg.faddr[0],str));
		return(nulstr);
	}

	if(!strcmp(sp,"EMAILADDR"))
		return(usermailaddr(&cfg, str
			,cfg.inetmail_misc&NMAIL_ALIAS ? useron.alias : useron.name));

	if(strcmp(sp, "NETMAIL") == 0)
		return useron.netmail;

	if(strcmp(sp, "FWD") == 0)
		return (useron.misc & NETMAIL) ? text[On] : text[Off];

	if(strcmp(sp, "TMP") == 0)
		return useron.tmpext;

	if(strcmp(sp, "SEX") == 0) {
		safe_snprintf(str, maxlen, "%c", useron.sex);
		return str;
	}
	if(strcmp(sp, "GENDERS") == 0) {
		return cfg.new_genders;
	}

	if(!strcmp(sp,"QWKID"))
		return(cfg.sys_id);

	if(!strcmp(sp,"TIME") || !strcmp(sp,"SYSTIME")) {
		now=time(NULL);
		memset(&tm,0,sizeof(tm));
		localtime_r(&now,&tm);
		if(cfg.sys_misc&SM_MILITARY)
			safe_snprintf(str,maxlen,"%02d:%02d:%02d"
		        	,tm.tm_hour,tm.tm_min,tm.tm_sec);
		else
			safe_snprintf(str,maxlen,"%02d:%02d %s"
				,tm.tm_hour==0 ? 12
				: tm.tm_hour>12 ? tm.tm_hour-12
				: tm.tm_hour, tm.tm_min, tm.tm_hour>11 ? "pm":"am");
		return(str);
	}

	if(!strcmp(sp,"TIMEZONE"))
		return(smb_zonestr(sys_timezone(&cfg),str));

	if(!strcmp(sp,"DATE") || !strcmp(sp,"SYSDATE")) {
		return(unixtodstr(&cfg,time32(NULL),str));
	}

	if(strncmp(sp, "DATE:", 5) == 0 || strncmp(sp, "TIME:", 5) == 0) {
		sp += 5;
		c_unescape_str(sp);
		now = time(NULL);
		memset(&tm, 0, sizeof(tm));
		localtime_r(&now, &tm);
		strftime(str, maxlen, sp, &tm);
		return str;
	}

	if(!strcmp(sp,"DATETIME"))
		return(timestr(time(NULL)));

	if(!strcmp(sp,"DATETIMEZONE")) {
		char zone[32];
		safe_snprintf(str, maxlen, "%s %s", timestr(time(NULL)), smb_zonestr(sys_timezone(&cfg),zone));
		return str;
	}
	
	if(strcmp(sp, "DATEFMT") == 0) {
		return cfg.sys_misc&SM_EURODATE ? "DD/MM/YY" : "MM/DD/YY";
	}

	if(strcmp(sp, "BDATEFMT") == 0 || strcmp(sp, "BIRTHFMT") == 0) {
		return birthdate_format(&cfg);
	}

	if(strcmp(sp, "GENDERS") == 0)
		return cfg.new_genders;

	if(!strcmp(sp,"TMSG")) {
		l=0;
		for(i=0;i<cfg.total_subs;i++)
			l+=getposts(&cfg,i); 		/* l=total posts */
		safe_snprintf(str,maxlen,"%lu",l);
		return(str);
	}

	if(!strcmp(sp,"TUSER")) {
		safe_snprintf(str,maxlen,"%u",total_users(&cfg));
		return(str);
	}

	if(!strcmp(sp,"TFILE")) {
		l=0;
		for(i=0;i<cfg.total_dirs;i++)
			l+=getfiles(&cfg,i);
		safe_snprintf(str,maxlen,"%lu",l);
		return(str);
	}

	if(strncmp(sp, "FILES:", 6) == 0) {	// Number of files in specified directory
		const char* path = getpath(&cfg, sp + 6);
		safe_snprintf(str, maxlen, "%lu", getfilecount(path));
		return str;
	}

	if(strcmp(sp, "FILES") == 0) {	// Number of files in current directory
		safe_snprintf(str, maxlen, "%lu", (ulong)getfiles(&cfg, usrdir[curlib][curdir[curlib]]));
		return str;
	}

	if(strncmp(sp, "FILESIZE:", 9) == 0) {
		const char* path = getpath(&cfg, sp + 9);
		byte_estimate_to_str(getfilesizetotal(path), str, maxlen, /* unit: */1, /* precision: */1);
		return str;
	}

	if(strcmp(sp, "FILESIZE") == 0) {
		byte_estimate_to_str(getfilesizetotal(cfg.dir[usrdir[curlib][curdir[curlib]]]->path)
			,str, maxlen, /* unit: */1, /* precision: */1);
		return str;
	}

	if(strncmp(sp, "FILEBYTES:", 10) == 0) {	// Number of bytes in current file directory
		const char* path = getpath(&cfg, sp + 10);
		safe_snprintf(str, maxlen, "%" PRIu64, getfilesizetotal(path));
		return str;
	}

	if(strcmp(sp, "FILEBYTES") == 0) {	// Number of bytes in current file directory
		safe_snprintf(str, maxlen, "%" PRIu64
			,getfilesizetotal(cfg.dir[usrdir[curlib][curdir[curlib]]]->path));
		return str;
	}

	if(strncmp(sp, "FILEKB:", 7) == 0) {	// Number of kibibytes in current file directory
		const char* path = getpath(&cfg, sp + 7);
		safe_snprintf(str, maxlen, "%1.1f", getfilesizetotal(path) / 1024.0);
		return str;
	}

	if(strcmp(sp, "FILEKB") == 0) {	// Number of kibibytes in current file directory
		safe_snprintf(str, maxlen, "%1.1f"
			,getfilesizetotal(cfg.dir[usrdir[curlib][curdir[curlib]]]->path) / 1024.0);
		return str;
	}

	if(strncmp(sp, "FILEMB:", 7) == 0) {	// Number of mebibytes in current file directory
		const char* path = getpath(&cfg, sp + 7);
		safe_snprintf(str, maxlen, "%1.1f", getfilesizetotal(path) / (1024.0 * 1024.0));
		return str;
	}

	if(strcmp(sp, "FILEMB") == 0) {	// Number of mebibytes in current file directory
		safe_snprintf(str, maxlen, "%1.1f"
			,getfilesizetotal(cfg.dir[usrdir[curlib][curdir[curlib]]]->path) / (1024.0 * 1024.0));
		return str;
	}

	if(strncmp(sp, "FILEGB:", 7) == 0) {	// Number of gibibytes in current file directory
		const char* path = getpath(&cfg, sp + 7);
		safe_snprintf(str, maxlen, "%1.1f", getfilesizetotal(path) / (1024.0 * 1024.0 * 1024.0));
		return str;
	}

	if(strcmp(sp, "FILEGB") == 0) {	// Number of gibibytes in current file directory
		safe_snprintf(str, maxlen, "%1.1f"
			,getfilesizetotal(cfg.dir[usrdir[curlib][curdir[curlib]]]->path) / (1024.0 * 1024.0 * 1024.0));
		return str;
	}

	if(!strcmp(sp,"TCALLS") || !strcmp(sp,"NUMCALLS")) {
		getstats(&cfg,0,&stats);
		safe_snprintf(str,maxlen,"%lu", (ulong)stats.logons);
		return(str);
	}

	if(!strcmp(sp,"PREVON") || !strcmp(sp,"LASTCALLERNODE")
		|| !strcmp(sp,"LASTCALLERSYSTEM"))
		return(lastuseron);

	if(!strcmp(sp,"CLS") || !strcmp(sp,"CLEAR")) {
		CLS;
		return(nulstr);
	}

	if(strcmp(sp, "GETKEY") == 0) {
		getkey();
		return(nulstr);
	}

	if(strcmp(sp, "CONTINUE") == 0) {
		char ch = getkey(K_UPPER);
		if(ch == text[YNQP][1] || ch == text[YNQP][2])
			sys_status|=SS_ABORT;
		return(nulstr);
	}

	if(strncmp(sp, "WAIT:", 5) == 0) {
		inkey(K_NONE, atoi(sp + 5) * 100);
		return(nulstr);
	}

	if(!strcmp(sp,"PAUSE") || !strcmp(sp,"MORE")) {
		pause();
		return(nulstr);
	}

	if(!strcmp(sp,"RESETPAUSE")) {
		lncntr=0;
		return(nulstr);
	}

	if(!strcmp(sp,"NOPAUSE") || !strcmp(sp,"POFF")) {
		sys_status^=SS_PAUSEOFF;
		return(nulstr);
	}

	if(!strcmp(sp,"PON") || !strcmp(sp,"AUTOMORE")) {
		sys_status^=SS_PAUSEON;
		return(nulstr);
	}

	if(strncmp(sp, "FILL:", 5) == 0) {
		sp += 5;
		long margin = centered ? column : 1;
		if(margin < 1) margin = 1;
		c_unescape_str(sp);
		while(*sp && online && column < cols - margin)
			bputs(sp, P_TRUNCATE);
		return nulstr;
	}

	if(strncmp(sp, "POS:", 4) == 0) {	// PCBoard	(nn is 1 based)
		i = atoi(sp + 4);
		if(i >= 1)	// Convert to 0-based
			i--;
		for(l = i - column; l > 0; l--)
			outchar(' ');
		return nulstr;
	}

	if(strncmp(sp, "DELAY:", 6) == 0) {	// PCBoard
		mswait(atoi(sp + 6) * 100);
		return nulstr;
	}

	if(strcmp(sp, "YESCHAR") == 0) {	// PCBoard
		safe_snprintf(str, maxlen, "%c", text[YNQP][0]);
		return str;
	}

	if(strcmp(sp, "NOCHAR") == 0) {		// PCBoard
		safe_snprintf(str, maxlen, "%c", text[YNQP][1]);
		return str;
	}

	if(strcmp(sp, "QUITCHAR") == 0) {
		safe_snprintf(str, maxlen, "%c", text[YNQP][2]);
		return str;
	}

	if(strncmp(sp, "BPS:", 4) == 0) {
		set_output_rate((enum output_rate)atoi(sp + 4));
		return nulstr;
	}

	if(strncmp(sp, "TEXT:", 5) == 0) {
		i = atoi(sp + 5);
		if(i >= 1 && i <= TOTAL_TEXT)
			return text[i - 1];
		return nulstr;
	}

	/* NOSTOP */

	/* STOP */

	if(!strcmp(sp,"BELL") || !strcmp(sp,"BEEP"))
		return("\a");

	if(!strcmp(sp,"EVENT")) {
		if(event_time==0)
			return("<none>");
		return(timestr(event_time));
	}

	/* LASTCALL */

	if(!strncmp(sp,"NODE",4)) {
		i=atoi(sp+4);
		if(i && i<=cfg.sys_nodes) {
			getnodedat(i,&node,0);
			printnodedat(i,&node);
		}
		return(nulstr);
	}

	if(!strcmp(sp,"WHO")) {
		whos_online(true);
		return(nulstr);
	}

	/* User Codes */

	if(!strcmp(sp,"USER") || !strcmp(sp,"ALIAS") || !strcmp(sp,"NAME"))
		return(useron.alias);

	if(!strcmp(sp,"FIRST")) {
		safe_snprintf(str,maxlen,"%s",useron.alias);
		tp=strchr(str,' ');
		if(tp) *tp=0;
		return(str);
	}

	if(!strcmp(sp,"USERNUM")) {
		safe_snprintf(str,maxlen,"%u",useron.number);
		return(str);
	}

	if(!strcmp(sp,"PHONE") || !strcmp(sp,"HOMEPHONE")
		|| !strcmp(sp,"DATAPHONE") || !strcmp(sp,"DATA"))
		return(useron.phone);

	if(!strcmp(sp,"ADDR1"))
		return(useron.address);

	if(!strcmp(sp,"FROM"))
		return(useron.location);

	if(!strcmp(sp,"CITY")) {
		safe_snprintf(str,maxlen,"%s",useron.location);
		char* p=strchr(str,',');
		if(p) {
			*p=0;
			return(str);
		}
		return(nulstr);
	}

	if(!strcmp(sp,"STATE")) {
		char* p=strchr(useron.location,',');
		if(p) {
			p++;
			if(*p==' ')
				p++;
			return(p);
		}
		return(nulstr);
	}

	if(!strcmp(sp,"CPU"))
		return(useron.comp);

	if(!strcmp(sp,"HOST"))
		return(client_name);

	if(!strcmp(sp,"BDATE"))
		return getbirthdstr(&cfg, useron.birth, str, maxlen);

	if(strcmp(sp, "BIRTH") == 0)
		return format_birthdate(&cfg, useron.birth, str, maxlen);

	if(strncmp(sp, "BDATE:", 6) == 0 || strncmp(sp, "BIRTH:", 6) == 0) {
		sp += 6;
		c_unescape_str(sp);
		memset(&tm,0,sizeof(tm));
		tm.tm_year = getbirthyear(useron.birth) - 1900;
		tm.tm_mon = getbirthmonth(&cfg, useron.birth) - 1;
		tm.tm_mday = getbirthday(&cfg, useron.birth);
		mktime(&tm);
		strftime(str, maxlen, sp, &tm);
		return str;
	}

	if(!strcmp(sp,"AGE")) {
		safe_snprintf(str,maxlen,"%u",getage(&cfg,useron.birth));
		return(str);
	}

	if(!strcmp(sp,"CALLS") || !strcmp(sp,"NUMTIMESON")) {
		safe_snprintf(str,maxlen,"%u",useron.logons);
		return(str);
	}

	if(strcmp(sp, "PWAGE") == 0) {
		time_t age = time(NULL) - useron.pwmod;
		safe_snprintf(str, maxlen, "%ld", (long)(age/(24*60*60)));
		return str;
	}

	if(strcmp(sp, "PWDATE") == 0 || strcmp(sp, "MEMO") == 0)
		return(unixtodstr(&cfg,useron.pwmod,str));

	if(strncmp(sp, "PWDATE:", 7) == 0) {
		sp += 7;
		c_unescape_str(sp);
		memset(&tm, 0, sizeof(tm));
		time_t date = useron.pwmod;
		localtime_r(&date, &tm);
		strftime(str, maxlen, sp, &tm);
		return str;
	}

	if(!strcmp(sp,"SEC") || !strcmp(sp,"SECURITY")) {
		safe_snprintf(str,maxlen,"%u",useron.level);
		return(str);
	}

	if(!strcmp(sp,"SINCE"))
		return(unixtodstr(&cfg,useron.firston,str));

	if(strncmp(sp, "SINCE:", 6) == 0) {
		sp += 6;
		c_unescape_str(sp);
		memset(&tm, 0, sizeof(tm));
		time_t date = useron.firston;
		localtime_r(&date, &tm);
		strftime(str, maxlen, sp, &tm);
		return str;
	}

	if(!strcmp(sp,"TIMEON") || !strcmp(sp,"TIMEUSED")) {
		now=time(NULL);
		safe_snprintf(str,maxlen,"%lu",(ulong)(now-logontime)/60L);
		return(str);
	}

	if(!strcmp(sp,"TUSED")) {              /* Synchronet only */
		now=time(NULL);
		return(sectostr((uint)(now-logontime),str)+1);
	}

	if(!strcmp(sp,"TLEFT")) {              /* Synchronet only */
		gettimeleft();
		return(sectostr(timeleft,str)+1);
	}

	if(!strcmp(sp,"TPERD"))                /* Synchronet only */
		return(sectostr(cfg.level_timeperday[useron.level],str)+4);

	if(!strcmp(sp,"TPERC"))                /* Synchronet only */
		return(sectostr(cfg.level_timepercall[useron.level],str)+4);

	if(strcmp(sp, "MPERC") == 0 || strcmp(sp, "TIMELIMIT") == 0) {
		safe_snprintf(str,maxlen,"%u",cfg.level_timepercall[useron.level]);
		return(str);
	}

	if(strcmp(sp, "MPERD") == 0) {
		safe_snprintf(str,maxlen,"%u",cfg.level_timeperday[useron.level]);
		return str;
	}

	if(strcmp(sp, "MAXCALLS") == 0) {
		safe_snprintf(str,maxlen,"%u",cfg.level_callsperday[useron.level]);
		return str;
	}

	if(strcmp(sp, "MAXPOSTS") == 0) {
		safe_snprintf(str,maxlen,"%u",cfg.level_postsperday[useron.level]);
		return str;
	}

	if(strcmp(sp, "MAXMAILS") == 0) {
		safe_snprintf(str,maxlen,"%u",cfg.level_emailperday[useron.level]);
		return str;
	}

	if(strcmp(sp, "MAXLINES") == 0) {
		safe_snprintf(str,maxlen,"%u",cfg.level_linespermsg[useron.level]);
		return str;
	}

	if(!strcmp(sp,"MINLEFT") || !strcmp(sp,"LEFT") || !strcmp(sp,"TIMELEFT")) {
		gettimeleft();
		safe_snprintf(str,maxlen,"%lu",timeleft/60);
		return(str);
	}

	if(!strcmp(sp,"LASTON"))
		return(timestr(useron.laston));

	if(!strcmp(sp,"LASTDATEON"))
		return(unixtodstr(&cfg,useron.laston,str));

	if(strncmp(sp, "LASTON:", 7) == 0) {
		sp += 7;
		c_unescape_str(sp);
		memset(&tm, 0, sizeof(tm));
		time_t date = useron.laston;
		localtime_r(&date, &tm);
		strftime(str, maxlen, sp, &tm);
		return str;
	}

	if(!strcmp(sp,"LASTTIMEON")) {
		memset(&tm,0,sizeof(tm));
		localtime32(&useron.laston,&tm);
		if(cfg.sys_misc&SM_MILITARY)
			safe_snprintf(str,maxlen,"%02d:%02d:%02d"
				,tm.tm_hour, tm.tm_min, tm.tm_sec);
		else
			safe_snprintf(str,maxlen,"%02d:%02d %s"
				,tm.tm_hour==0 ? 12
				: tm.tm_hour>12 ? tm.tm_hour-12
				: tm.tm_hour, tm.tm_min, tm.tm_hour>11 ? "pm":"am");
		return(str);
	}

	if(!strcmp(sp,"FIRSTON"))
		return(timestr(useron.firston));

	if(!strcmp(sp,"FIRSTDATEON"))
		return(unixtodstr(&cfg,useron.firston,str));

	if(strncmp(sp, "FIRSTON:", 8) == 0) {
		sp += 8;
		c_unescape_str(sp);
		memset(&tm, 0, sizeof(tm));
		time_t date = useron.firston;
		localtime_r(&date, &tm);
		strftime(str, maxlen, sp, &tm);
		return str;
	}

	if(!strcmp(sp,"FIRSTTIMEON")) {
		memset(&tm,0,sizeof(tm));
		localtime32(&useron.firston,&tm);
		if(cfg.sys_misc&SM_MILITARY)
			safe_snprintf(str,maxlen,"%02d:%02d:%02d"
				,tm.tm_hour, tm.tm_min, tm.tm_sec);
		else
			safe_snprintf(str,maxlen,"%02d:%02d %s"
				,tm.tm_hour==0 ? 12
				: tm.tm_hour>12 ? tm.tm_hour-12
				: tm.tm_hour, tm.tm_min, tm.tm_hour>11 ? "pm":"am");
		return(str);
	}

	if(strcmp(sp, "EMAILS") == 0) {
		safe_snprintf(str, maxlen, "%u", useron.emails);
		return str;
	}

	if(strcmp(sp, "FBACKS") == 0) {
		safe_snprintf(str, maxlen, "%u", useron.fbacks);
		return str;
	}

	if(strcmp(sp, "ETODAY") == 0) {
		safe_snprintf(str, maxlen, "%u", useron.etoday);
		return str;
	}

	if(strcmp(sp, "PTODAY") == 0) {
		safe_snprintf(str, maxlen, "%u", useron.ptoday);
		return str;
	}

	if(strcmp(sp, "LTODAY") == 0) {
		safe_snprintf(str, maxlen, "%u", useron.ltoday);
		return str;
	}

	if(strcmp(sp, "MTODAY") == 0) {
		safe_snprintf(str, maxlen, "%u", useron.ttoday);
		return str;
	}

	if(strcmp(sp, "MTOTAL") == 0) {
		safe_snprintf(str, maxlen, "%u", useron.timeon);
		return str;
	}

	if(strcmp(sp, "TTODAY") == 0)
		return sectostr(useron.ttoday, str) + 3;

	if(strcmp(sp, "TTOTAL") == 0)
		return sectostr(useron.timeon, str) + 3;

	if(strcmp(sp, "TLAST") == 0) {
		safe_snprintf(str, maxlen, "%u", useron.tlast);
		return str;
	}

	if(strcmp(sp, "MEXTRA") == 0) {
		safe_snprintf(str, maxlen, "%u", useron.textra);
		return str;
	}

	if(strcmp(sp, "TEXTRA") == 0)
		return sectostr(useron.textra, str) + 3;

	if(strcmp(sp, "MBANKED") == 0) {
		safe_snprintf(str, maxlen, "%lu", useron.min);
		return str;
	}

	if(strcmp(sp, "TBANKED") == 0)
		return sectostr(useron.min, str) + 3;

	if(!strcmp(sp,"MSGLEFT") || !strcmp(sp,"MSGSLEFT")) {
		safe_snprintf(str,maxlen,"%u",useron.posts);
		return(str);
	}

	if(!strcmp(sp,"MSGREAD")) {
		safe_snprintf(str,maxlen,"%lu",posts_read);
		return(str);
	}

	if(!strcmp(sp,"FREESPACE")) {
		safe_snprintf(str,maxlen,"%lu",getfreediskspace(cfg.temp_dir,0));
		return(str);
	}

	if(!strcmp(sp,"FREESPACEK")) {
		safe_snprintf(str,maxlen,"%lu",getfreediskspace(cfg.temp_dir,1024));
		return(str);
	}

	if(strcmp(sp,"FREESPACEM") == 0) {
		safe_snprintf(str,maxlen,"%lu",getfreediskspace(cfg.temp_dir, 1024 * 1024));
		return(str);
	}

	if(strcmp(sp,"FREESPACEG") == 0) {
		safe_snprintf(str,maxlen,"%lu",getfreediskspace(cfg.temp_dir, 1024 * 1024 * 1024));
		return(str);
	}

	if(strcmp(sp,"FREESPACET") == 0) {
		safe_snprintf(str,maxlen,"%lu",getfreediskspace(cfg.temp_dir, 1024 * 1024 * 1024) / 1024);
		return(str);
	}

	if(!strcmp(sp,"UPBYTES")) {
		safe_snprintf(str,maxlen,"%lu",useron.ulb);
		return(str);
	}

	if(!strcmp(sp,"UPK")) {
		safe_snprintf(str,maxlen,"%lu",useron.ulb/1024L);
		return(str);
	}

	if(!strcmp(sp,"UPS") || !strcmp(sp,"UPFILES")) {
		safe_snprintf(str,maxlen,"%u",useron.uls);
		return(str);
	}

	if(!strcmp(sp,"DLBYTES")) {
		safe_snprintf(str,maxlen,"%lu",useron.dlb);
		return(str);
	}

	if(!strcmp(sp,"DOWNK")) {
		safe_snprintf(str,maxlen,"%lu",useron.dlb/1024L);
		return(str);
	}

	if(!strcmp(sp,"DOWNS") || !strcmp(sp,"DLFILES")) {
		safe_snprintf(str,maxlen,"%u",useron.dls);
		return(str);
	}

	if(strcmp(sp, "PCR") == 0) {
		float f = 0;
		if(useron.posts)
			f = (float)useron.logons / useron.posts;
		safe_snprintf(str, maxlen, "%u", f ? (uint)(100 / f) : 0);
		return str;
	}

	if(strcmp(sp, "UDR") == 0) {
		float f = 0;
		if(useron.ulb)
			f = (float)useron.dlb / useron.ulb;
		safe_snprintf(str, maxlen, "%u", f ? (uint)(100 / f) : 0);
		return str;
	}

	if(strcmp(sp, "UDFR") == 0) {
		float f = 0;
		if(useron.uls)
			f = (float)useron.dls / useron.uls;
		safe_snprintf(str, maxlen, "%u", f ? (uint)(100 / f) : 0);
		return str;
	}

	if(!strcmp(sp,"LASTNEW"))
		return(unixtodstr(&cfg,(time32_t)ns_time,str));

	if(strncmp(sp, "LASTNEW:", 8) == 0) {
		sp += 8;
		c_unescape_str(sp);
		memset(&tm, 0, sizeof(tm));
		time_t date = ns_time;
		localtime_r(&date, &tm);
		strftime(str, maxlen, sp, &tm);
		return str;
	}

	if(!strcmp(sp,"NEWFILETIME"))
		return(timestr(ns_time));

	/* MAXDL */

	if(!strcmp(sp,"MAXDK") || !strcmp(sp,"DLKLIMIT") || !strcmp(sp,"KBLIMIT")) {
		safe_snprintf(str,maxlen,"%lu",cfg.level_freecdtperday[useron.level]/1024L);
		return(str);
	}

	if(!strcmp(sp,"DAYBYTES")) {    /* amt of free cdts used today */
		safe_snprintf(str,maxlen,"%lu",cfg.level_freecdtperday[useron.level]-useron.freecdt);
		return(str);
	}

	if(!strcmp(sp,"BYTELIMIT")) {
		safe_snprintf(str,maxlen,"%ld", (long)cfg.level_freecdtperday[useron.level]);
		return(str);
	}

	if(!strcmp(sp,"KBLEFT")) {
		safe_snprintf(str,maxlen,"%lu",(useron.cdt+useron.freecdt)/1024L);
		return(str);
	}

	if(!strcmp(sp,"BYTESLEFT")) {
		safe_snprintf(str,maxlen,"%lu",useron.cdt+useron.freecdt);
		return(str);
	}

	if(strcmp(sp, "CREDITS") == 0) {
		safe_snprintf(str, maxlen, "%lu", useron.cdt);
		return str;
	}

	if(strcmp(sp, "FREECDT") == 0) {
		safe_snprintf(str, maxlen, "%lu", useron.freecdt);
		return str;
	}

	if(!strcmp(sp,"CONF")) {
		safe_snprintf(str,maxlen,"%s %s"
			,usrgrps ? cfg.grp[usrgrp[curgrp]]->sname :nulstr
			,usrgrps ? cfg.sub[usrsub[curgrp][cursub[curgrp]]]->sname : nulstr);
		return(str);
	}

	if(!strcmp(sp,"CONFNUM")) {
		safe_snprintf(str,maxlen,"%u %u",curgrp+1,cursub[curgrp]+1);
		return(str);
	}

	if(!strcmp(sp,"NUMDIR")) {
		safe_snprintf(str,maxlen,"%u %u",usrlibs ? curlib+1 : 0,usrlibs ? curdir[curlib]+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"EXDATE") || !strcmp(sp,"EXPDATE"))
		return(unixtodstr(&cfg,useron.expire,str));

	if(strncmp(sp, "EXPDATE:", 8) == 0) {
		if(!useron.expire)
			return nulstr;
		sp += 8;
		c_unescape_str(sp);
		memset(&tm, 0, sizeof(tm));
		time_t date = useron.expire;
		localtime_r(&date, &tm);
		strftime(str, maxlen, sp, &tm);
		return str;
	}

	if(!strcmp(sp,"EXPDAYS")) {
		now=time(NULL);
		l=(long)(useron.expire-now);
		if(l<0)
			l=0;
		safe_snprintf(str,maxlen,"%lu",l/(1440L*60L));
		return(str);
	}

	if(strcmp(sp, "NOTE") == 0 || strcmp(sp, "MEMO1") == 0)
		return useron.note;

	if(strcmp(sp,"REALNAME") == 0 || !strcmp(sp,"MEMO2") || !strcmp(sp,"COMPANY"))
		return(useron.name);

	if(!strcmp(sp,"ZIP"))
		return(useron.zipcode);

	if(!strcmp(sp,"HANGUP")) {
		hangup();
		return(nulstr);
	}

	/* Synchronet Specific */

	if(!strncmp(sp,"SETSTR:",7)) {
		strcpy(main_csi.str,sp+7);
		return(nulstr);
	}

	if(strcmp(sp,"STR") == 0) {
		return main_csi.str;
	}

	if(strncmp(sp, "STRVAR:", 7) == 0) {
		uint32_t crc = crc32(sp + 7, 0);
		if(main_csi.str_var && main_csi.str_var_name) {
			for(i=0;i<main_csi.str_vars;i++)
				if(main_csi.str_var_name[i] == crc)
					return main_csi.str_var[i];
		}
		return nulstr;
	}

	if(strncmp(sp, "JS:", 3) == 0) {
		jsval val;
		if(JS_GetProperty(js_cx, obj == NULL ? js_glob : obj, sp + 3, &val))
			JSVALUE_TO_STRBUF(js_cx, val, str, maxlen, NULL);
		return str;
	}

	if(!strncmp(sp,"EXEC:",5)) {
		exec_bin(sp+5,&main_csi);
		return(nulstr);
	}

	if(!strncmp(sp,"EXEC_XTRN:",10)) {
		for(i=0;i<cfg.total_xtrns;i++)
			if(!stricmp(cfg.xtrn[i]->code,sp+10))
				break;
		if(i<cfg.total_xtrns)
			exec_xtrn(i);
		return(nulstr);
	}

	if(!strncmp(sp,"MENU:",5)) {
		menu(sp+5);
		return(nulstr);
	}

	if(!strncmp(sp,"CONDMENU:",9)) {
		menu(sp+9, P_NOERROR);
		return(nulstr);
	}

	if(!strncmp(sp,"TYPE:",5)) {
		printfile(cmdstr(sp+5,nulstr,nulstr,str),0);
		return(nulstr);
	}

	if(!strncmp(sp,"INCLUDE:",8)) {
		printfile(cmdstr(sp+8,nulstr,nulstr,str),P_NOCRLF|P_SAVEATR);
		return(nulstr);
	}

	if(!strcmp(sp,"QUESTION"))
		return(question);

	if(!strcmp(sp,"HANDLE"))
		return(useron.handle);

	if(strcmp(sp, "LASTIP") == 0)
		return useron.ipaddr;

	if(!strcmp(sp,"CID") || !strcmp(sp,"IP"))
		return(cid);

	if(!strcmp(sp,"LOCAL-IP"))
		return(local_addr);

	if(!strcmp(sp,"CRLF"))
		return("\r\n");

	if(!strcmp(sp,"PUSHXY")) {
		ansi_save();
		return(nulstr);
	}

	if(!strcmp(sp,"POPXY")) {
		ansi_restore();
		return(nulstr);
	}

	if(!strcmp(sp,"HOME")) {
		cursor_home();
		return(nulstr);
	}

	if(!strcmp(sp,"CLRLINE")) {
		clearline();
		return(nulstr);
	}

	if(!strcmp(sp,"CLR2EOL") || !strcmp(sp,"CLREOL")) {
		cleartoeol();
		return(nulstr);
	}

	if(!strcmp(sp,"CLR2EOS")) {
		cleartoeos();
		return(nulstr);
	}

	if(!strncmp(sp,"UP:",3)) {
		cursor_up(atoi(sp+3));
		return(str);
	}

	if(!strncmp(sp,"DOWN:",5)) {
		cursor_down(atoi(sp+5));
		return(str);
	}

	if(!strncmp(sp,"LEFT:",5)) {
		cursor_left(atoi(sp+5));
		return(str);
	}

	if(!strncmp(sp,"RIGHT:",6)) {
		cursor_right(atoi(sp+6));
		return(str);
	}

	if(!strncmp(sp,"GOTOXY:",7)) {
		tp=strchr(sp,',');
		if(tp!=NULL) {
			tp++;
			cursor_xy(atoi(sp+7),atoi(tp));
		}
		return(nulstr);
	}

	if(!strcmp(sp,"GRP")) {
		if(SMB_IS_OPEN(&smb)) {
			if(smb.subnum==INVALID_SUB)
				return("Local");
			if(smb.subnum<cfg.total_subs)
				return(cfg.grp[cfg.sub[smb.subnum]->grp]->sname);
		}
		return(usrgrps ? cfg.grp[usrgrp[curgrp]]->sname : nulstr);
	}

	if(!strcmp(sp,"GRPL")) {
		if(SMB_IS_OPEN(&smb)) {
			if(smb.subnum==INVALID_SUB)
				return("Local");
			if(smb.subnum<cfg.total_subs)
				return(cfg.grp[cfg.sub[smb.subnum]->grp]->lname);
		}
		return(usrgrps ? cfg.grp[usrgrp[curgrp]]->lname : nulstr);
	}

	if(!strcmp(sp,"GN")) {
		if(SMB_IS_OPEN(&smb))
			ugrp=getusrgrp(smb.subnum);
		else
			ugrp=usrgrps ? curgrp+1 : 0;
		safe_snprintf(str,maxlen,"%u",ugrp);
		return(str);
	}

	if(!strcmp(sp,"GL")) {
		if(SMB_IS_OPEN(&smb))
			ugrp=getusrgrp(smb.subnum);
		else
			ugrp=usrgrps ? curgrp+1 : 0;
		safe_snprintf(str,maxlen,"%-4u",ugrp);
		return(str);
	}

	if(!strcmp(sp,"GR")) {
		if(SMB_IS_OPEN(&smb))
			ugrp=getusrgrp(smb.subnum);
		else
			ugrp=usrgrps ? curgrp+1 : 0;
		safe_snprintf(str,maxlen,"%4u",ugrp);
		return(str);
	}

	if(!strcmp(sp,"SUB")) {
		if(SMB_IS_OPEN(&smb)) {
			if(smb.subnum==INVALID_SUB)
				return("Mail");
			else if(smb.subnum<cfg.total_subs)
				return(cfg.sub[smb.subnum]->sname);
		}
		return(usrgrps ? cfg.sub[usrsub[curgrp][cursub[curgrp]]]->sname : nulstr);
	}

	if(!strcmp(sp,"SUBL")) {
		if(SMB_IS_OPEN(&smb)) {
			if(smb.subnum==INVALID_SUB)
				return("Mail");
			else if(smb.subnum<cfg.total_subs)
				return(cfg.sub[smb.subnum]->lname);
		}
		return(usrgrps  ? cfg.sub[usrsub[curgrp][cursub[curgrp]]]->lname : nulstr);
	}

	if(!strcmp(sp,"SN")) {
		if(SMB_IS_OPEN(&smb))
			usub=getusrsub(smb.subnum);
		else
			usub=usrgrps ? cursub[curgrp]+1 : 0;
		safe_snprintf(str,maxlen,"%u",usub);
		return(str);
	}

	if(!strcmp(sp,"SL")) {
		if(SMB_IS_OPEN(&smb))
			usub=getusrsub(smb.subnum);
		else
			usub=usrgrps ? cursub[curgrp]+1 : 0;
		safe_snprintf(str,maxlen,"%-4u",usub);
		return(str);
	}

	if(!strcmp(sp,"SR")) {
		if(SMB_IS_OPEN(&smb))
			usub=getusrsub(smb.subnum);
		else
			usub=usrgrps ? cursub[curgrp]+1 : 0;
		safe_snprintf(str,maxlen,"%4u",usub);
		return(str);
	}

	if(!strcmp(sp,"LIB"))
		return(usrlibs ? cfg.lib[usrlib[curlib]]->sname : nulstr);

	if(!strcmp(sp,"LIBL"))
		return(usrlibs ? cfg.lib[usrlib[curlib]]->lname : nulstr);

	if(!strcmp(sp,"LN")) {
		safe_snprintf(str,maxlen,"%u",usrlibs ? curlib+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"LL")) {
		safe_snprintf(str,maxlen,"%-4u",usrlibs ? curlib+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"LR")) {
		safe_snprintf(str,maxlen,"%4u",usrlibs  ? curlib+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"DIR"))
		return(usrlibs ? cfg.dir[usrdir[curlib][curdir[curlib]]]->sname :nulstr);

	if(!strcmp(sp,"DIRL"))
		return(usrlibs ? cfg.dir[usrdir[curlib][curdir[curlib]]]->lname : nulstr);

	if(!strcmp(sp,"DN")) {
		safe_snprintf(str,maxlen,"%u",usrlibs ? curdir[curlib]+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"DL")) {
		safe_snprintf(str,maxlen,"%-4u",usrlibs ? curdir[curlib]+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"DR")) {
		safe_snprintf(str,maxlen,"%4u",usrlibs ? curdir[curlib]+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"NOACCESS")) {
		if(noaccess_str==text[NoAccessTime])
			safe_snprintf(str,maxlen,noaccess_str,noaccess_val/60,noaccess_val%60);
		else if(noaccess_str==text[NoAccessDay])
			safe_snprintf(str,maxlen,noaccess_str,wday[noaccess_val]);
		else
			safe_snprintf(str,maxlen,noaccess_str,noaccess_val);
		return(str);
	}

	if(!strcmp(sp,"LAST")) {
		tp=strrchr(useron.alias,' ');
		if(tp) tp++;
		else tp=useron.alias;
		return(tp);
	}

	if(!strcmp(sp,"REAL") || !strcmp(sp,"FIRSTREAL")) {
		safe_snprintf(str,maxlen,"%s",useron.name);
		tp=strchr(str,' ');
		if(tp) *tp=0;
		return(str);
	}

	if(!strcmp(sp,"LASTREAL")) {
		tp=strrchr(useron.name,' ');
		if(tp) tp++;
		else tp=useron.name;
		return(tp);
	}

	if(!strcmp(sp,"MAILR")) {
		safe_snprintf(str,maxlen,"%u",getmail(&cfg,useron.number, /* Sent: */FALSE, /* attr: */MSG_READ));
		return(str);
	}

	if(!strcmp(sp,"MAILU")) {
		safe_snprintf(str,maxlen,"%u",getmail(&cfg,useron.number, /* Sent: */FALSE, /* attr: */~MSG_READ));
		return(str);
	}

	if(!strcmp(sp,"MAILW")) {
		safe_snprintf(str,maxlen,"%u",getmail(&cfg,useron.number, /* Sent: */FALSE, /* attr: */0));
		return(str);
	}

	if(!strcmp(sp,"MAILP")) {
		safe_snprintf(str,maxlen,"%u",getmail(&cfg,useron.number,/* Sent: */TRUE, /* attr: */0));
		return(str);
	}

	if(!strcmp(sp,"SPAMW")) {
		safe_snprintf(str,maxlen,"%u",getmail(&cfg,useron.number, /* Sent: */FALSE, /* attr: */MSG_SPAM));
		return(str);
	}

	if(!strncmp(sp,"MAILR:",6) || !strncmp(sp,"MAILR#",6)) {
		safe_snprintf(str,maxlen,"%u",getmail(&cfg,atoi(sp+6), /* Sent: */FALSE, /* attr: */MSG_READ));
		return(str);
	}

	if(!strncmp(sp,"MAILU:",6) || !strncmp(sp,"MAILU#",6)) {
		safe_snprintf(str,maxlen,"%u",getmail(&cfg,atoi(sp+6), /* Sent: */FALSE, /* attr: */~MSG_READ));
		return(str);
	}

	if(!strncmp(sp,"MAILW:",6) || !strncmp(sp,"MAILW#",6)) {
		safe_snprintf(str,maxlen,"%u",getmail(&cfg,atoi(sp+6), /* Sent: */FALSE, /* attr: */0));
		return(str);
	}

	if(!strncmp(sp,"MAILP:",6) || !strncmp(sp,"MAILP#",6)) {
		safe_snprintf(str,maxlen,"%u",getmail(&cfg,atoi(sp+6), /* Sent: */TRUE, /* attr: */0));
		return(str);
	}

	if(!strncmp(sp,"SPAMW:",6) || !strncmp(sp,"SPAMW#",6)) {
		safe_snprintf(str,maxlen,"%u",getmail(&cfg,atoi(sp+6), /* Sent: */FALSE, /* attr: */MSG_SPAM));
		return(str);
	}

	if(!strcmp(sp,"MSGREPLY")) {
		safe_snprintf(str,maxlen,"%c",cfg.sys_misc&SM_RA_EMU ? 'R' : 'A');
		return(str);
	}

	if(!strcmp(sp,"MSGREREAD")) {
		safe_snprintf(str,maxlen,"%c",cfg.sys_misc&SM_RA_EMU ? 'A' : 'R');
		return(str);
	}

	if(!strncmp(sp,"STATS.",6)) {
		getstats(&cfg,0,&stats);
		sp+=6;
		if(!strcmp(sp,"LOGONS"))
			safe_snprintf(str,maxlen,"%lu", (ulong)stats.logons);
		else if(!strcmp(sp,"LTODAY"))
			safe_snprintf(str,maxlen,"%lu", (ulong)stats.ltoday);
		else if(!strcmp(sp,"TIMEON"))
			safe_snprintf(str,maxlen,"%lu", (ulong)stats.timeon);
		else if(!strcmp(sp,"TTODAY"))
			safe_snprintf(str,maxlen,"%lu", (ulong)stats.ttoday);
		else if(!strcmp(sp,"ULS"))
			safe_snprintf(str,maxlen,"%lu", (ulong)stats.uls);
		else if(!strcmp(sp,"ULB"))
			safe_snprintf(str,maxlen,"%lu", (ulong)stats.ulb);
		else if(!strcmp(sp,"DLS"))
			safe_snprintf(str,maxlen,"%lu", (ulong)stats.dls);
		else if(!strcmp(sp,"DLB"))
			safe_snprintf(str,maxlen,"%lu", (ulong)stats.dlb);
		else if(!strcmp(sp,"PTODAY"))
			safe_snprintf(str,maxlen,"%lu", (ulong)stats.ptoday);
		else if(!strcmp(sp,"ETODAY"))
			safe_snprintf(str,maxlen,"%lu", (ulong)stats.etoday);
		else if(!strcmp(sp,"FTODAY"))
			safe_snprintf(str,maxlen,"%lu", (ulong)stats.ftoday);
		else if(!strcmp(sp,"NUSERS"))
			safe_snprintf(str,maxlen,"%u",stats.nusers);
		return(str);
	}

	/* Message header codes */
	if(!strcmp(sp,"MSG_TO") && current_msg!=NULL && current_msg_to!=NULL) {
		if(pmode != NULL)
			*pmode |= (current_msg->hdr.auxattr & MSG_HFIELDS_UTF8);
		if(current_msg->to_ext!=NULL)
			safe_snprintf(str,maxlen,"%s #%s",current_msg_to,current_msg->to_ext);
		else if(current_msg->to_net.addr != NULL) {
			char tmp[128];
			safe_snprintf(str,maxlen,"%s (%s)",current_msg_to
				,smb_netaddrstr(&current_msg->to_net,tmp));
		} else
			return(current_msg_to);
		return(str);
	}
	if(!strcmp(sp,"MSG_TO_NAME") && current_msg_to!=NULL) {
		if(pmode != NULL && current_msg != NULL)
			*pmode |= (current_msg->hdr.auxattr & MSG_HFIELDS_UTF8);
		return(current_msg_to);
	}
	if(!strcmp(sp,"MSG_TO_EXT") && current_msg!=NULL) {
		if(current_msg->to_ext==NULL)
			return(nulstr);
		return(current_msg->to_ext);
	}
	if(!strcmp(sp,"MSG_TO_NET") && current_msg!=NULL) {
		if(current_msg->to_net.addr == NULL)
			return nulstr;
		return(smb_netaddrstr(&current_msg->to_net,str));
	}
	if(!strcmp(sp,"MSG_TO_NETTYPE") && current_msg!=NULL) {
		if(current_msg->to_net.type==NET_NONE)
			return nulstr;
		return(smb_nettype((enum smb_net_type)current_msg->to_net.type));
	}
	if(!strcmp(sp,"MSG_CC") && current_msg!=NULL)
		return(current_msg->cc_list == NULL ? nulstr : current_msg->cc_list);
	if(!strcmp(sp,"MSG_FROM") && current_msg != NULL && current_msg_from != NULL) {
		if(current_msg->hdr.attr&MSG_ANONYMOUS && !SYSOP)
			return(text[Anonymous]);
		if(pmode != NULL)
			*pmode |= (current_msg->hdr.auxattr & MSG_HFIELDS_UTF8);
		if(current_msg->from_ext!=NULL)
			safe_snprintf(str,maxlen,"%s #%s",current_msg_from,current_msg->from_ext);
		else if(current_msg->from_net.addr != NULL) {
			char tmp[128];
			safe_snprintf(str,maxlen,"%s (%s)",current_msg_from
				,smb_netaddrstr(&current_msg->from_net,tmp));
		} else
			return(current_msg_from);
		return(str);
	}
	if(!strcmp(sp,"MSG_FROM_NAME") && current_msg != NULL && current_msg_from != NULL) {
		if(current_msg->hdr.attr&MSG_ANONYMOUS && !SYSOP)
			return(text[Anonymous]);
		if(pmode != NULL)
			*pmode |= (current_msg->hdr.auxattr & MSG_HFIELDS_UTF8);
		return(current_msg_from);
	}
	if(!strcmp(sp,"MSG_FROM_EXT") && current_msg!=NULL) {
		if(!(current_msg->hdr.attr&MSG_ANONYMOUS) || SYSOP)
			if(current_msg->from_ext!=NULL)
				return(current_msg->from_ext);
		return(nulstr);
	}
	if(!strcmp(sp,"MSG_FROM_NET") && current_msg!=NULL) {
		if(current_msg->from_net.addr!=NULL
			&& (!(current_msg->hdr.attr&MSG_ANONYMOUS) || SYSOP))
			return(smb_netaddrstr(&current_msg->from_net,str));
		return(nulstr);
	}
	if(!strcmp(sp,"MSG_FROM_NETTYPE") && current_msg!=NULL) {
		if(current_msg->from_net.type==NET_NONE)
			return nulstr;
		return(smb_nettype((enum smb_net_type)current_msg->from_net.type));
	}
	if(!strcmp(sp,"MSG_SUBJECT") && current_msg_subj != NULL) {
		if(pmode != NULL && current_msg != NULL)
			*pmode |= (current_msg->hdr.auxattr & MSG_HFIELDS_UTF8);
		return(current_msg_subj);
	}
	if(!strcmp(sp,"MSG_SUMMARY") && current_msg!=NULL)
		return(current_msg->summary==NULL ? nulstr : current_msg->summary);
	if(!strcmp(sp,"MSG_TAGS") && current_msg!=NULL)
		return(current_msg->tags==NULL ? nulstr : current_msg->tags);
	if(!strcmp(sp,"MSG_DATE") && current_msg!=NULL)
		return(timestr(current_msg->hdr.when_written.time));
	if(!strcmp(sp,"MSG_IMP_DATE") && current_msg!=NULL)
		return(timestr(current_msg->hdr.when_imported.time));
	if(!strcmp(sp,"MSG_AGE") && current_msg!=NULL)
		return age_of_posted_item(str, maxlen
			, current_msg->hdr.when_written.time - (smb_tzutc(current_msg->hdr.when_written.zone) * 60));
	if(!strcmp(sp,"MSG_TIMEZONE") && current_msg!=NULL)
		return(smb_zonestr(current_msg->hdr.when_written.zone,NULL));
	if(!strcmp(sp,"MSG_IMP_TIMEZONE") && current_msg!=NULL)
		return(smb_zonestr(current_msg->hdr.when_imported.zone,NULL));
	if(!strcmp(sp,"MSG_ATTR") && current_msg!=NULL) {
		uint16_t attr = current_msg->hdr.attr;
		uint16_t poll = attr&MSG_POLL_VOTE_MASK;
		uint32_t auxattr = current_msg->hdr.auxattr;
		/* Synchronized with show_msgattr(): */
		safe_snprintf(str,maxlen,"%s%s%s%s%s%s%s%s%s%s%s%s%s%s"
			,attr&MSG_PRIVATE						? "Private  "   :nulstr
			,attr&MSG_SPAM							? "SPAM  "      :nulstr
			,attr&MSG_READ							? "Read  "      :nulstr
			,attr&MSG_DELETE						? "Deleted  "   :nulstr
			,attr&MSG_KILLREAD						? "Kill  "      :nulstr
			,attr&MSG_ANONYMOUS						? "Anonymous  " :nulstr
			,attr&MSG_LOCKED						? "Locked  "    :nulstr
			,attr&MSG_PERMANENT						? "Permanent  " :nulstr
			,attr&MSG_MODERATED						? "Moderated  " :nulstr
			,attr&MSG_VALIDATED						? "Validated  " :nulstr
			,attr&MSG_REPLIED						? "Replied  "	:nulstr
			,attr&MSG_NOREPLY						? "NoReply  "	:nulstr
			,poll == MSG_POLL						? "Poll  "		:nulstr
			,poll == MSG_POLL && auxattr&POLL_CLOSED	? "(Closed)  "	:nulstr
			);
		return(str);
	}
	if(!strcmp(sp,"MSG_AUXATTR") && current_msg!=NULL) {
		safe_snprintf(str,maxlen,"%s%s%s%s%s%s%s"
			,current_msg->hdr.auxattr&MSG_FILEREQUEST	? "FileRequest  "   :nulstr
			,current_msg->hdr.auxattr&MSG_FILEATTACH	? "FileAttach  "    :nulstr
			,current_msg->hdr.auxattr&MSG_MIMEATTACH	? "MimeAttach  "	:nulstr
			,current_msg->hdr.auxattr&MSG_KILLFILE		? "KillFile  "      :nulstr
			,current_msg->hdr.auxattr&MSG_RECEIPTREQ	? "ReceiptReq  "	:nulstr
			,current_msg->hdr.auxattr&MSG_CONFIRMREQ	? "ConfirmReq  "    :nulstr
			,current_msg->hdr.auxattr&MSG_NODISP		? "DontDisplay  "	:nulstr
			);
		return(str);
	}
	if(!strcmp(sp,"MSG_NETATTR") && current_msg!=NULL) {
		safe_snprintf(str,maxlen,"%s%s%s%s%s%s%s%s"
			,current_msg->hdr.netattr&MSG_LOCAL			? "Local  "			:nulstr
			,current_msg->hdr.netattr&MSG_INTRANSIT		? "InTransit  "     :nulstr
			,current_msg->hdr.netattr&MSG_SENT			? "Sent  "			:nulstr
			,current_msg->hdr.netattr&MSG_KILLSENT		? "KillSent  "      :nulstr
			,current_msg->hdr.netattr&MSG_HOLD			? "Hold  "			:nulstr
			,current_msg->hdr.netattr&MSG_CRASH			? "Crash  "			:nulstr
			,current_msg->hdr.netattr&MSG_IMMEDIATE		? "Immediate  "		:nulstr
			,current_msg->hdr.netattr&MSG_DIRECT		? "Direct  "		:nulstr
			);
		return(str);
	}
	if(!strcmp(sp,"MSG_ID") && current_msg!=NULL)
		return(current_msg->id==NULL ? nulstr : current_msg->id);
	if(!strcmp(sp,"MSG_REPLY_ID") && current_msg!=NULL)
		return(current_msg->reply_id==NULL ? nulstr : current_msg->reply_id);
	if(!strcmp(sp,"MSG_NUM") && current_msg!=NULL) {
		safe_snprintf(str,maxlen,"%lu", (ulong)current_msg->hdr.number);
		return(str);
	}
	if(!strcmp(sp,"MSG_SCORE") && current_msg!=NULL) {
		safe_snprintf(str, maxlen, "%ld", (long)(current_msg->upvotes - current_msg->downvotes));
		return(str);
	}
	if(!strcmp(sp,"MSG_UPVOTES") && current_msg!=NULL) {
		safe_snprintf(str, maxlen, "%lu", (ulong)current_msg->upvotes);
		return(str);
	}
	if(!strcmp(sp,"MSG_DOWNVOTES") && current_msg!=NULL) {
		safe_snprintf(str, maxlen, "%lu", (ulong)current_msg->downvotes);
		return(str);
	}
	if(!strcmp(sp,"MSG_TOTAL_VOTES") && current_msg!=NULL) {
		safe_snprintf(str, maxlen, "%lu", (ulong)current_msg->total_votes);
		return(str);
	}
	if(!strcmp(sp,"MSG_VOTED"))
		return (current_msg != NULL && current_msg->user_voted) ? text[PollAnswerChecked] : nulstr;
	if(!strcmp(sp,"MSG_UPVOTED"))
		return (current_msg != NULL && current_msg->user_voted == 1) ? text[PollAnswerChecked] : nulstr;
	if(!strcmp(sp,"MSG_DOWNVOTED"))
		return (current_msg != NULL && current_msg->user_voted == 2) ? text[PollAnswerChecked] : nulstr;
	if(strcmp(sp, "MSG_THREAD_ID") == 0 && current_msg != NULL) {
		safe_snprintf(str, maxlen, "%lu", (ulong)current_msg->hdr.thread_id);
		return str;
	}
	if(strcmp(sp, "MSG_THREAD_BACK") == 0 && current_msg != NULL) {
		safe_snprintf(str, maxlen, "%lu", (ulong)current_msg->hdr.thread_back);
		return str;
	}
	if(strcmp(sp, "MSG_THREAD_NEXT") == 0 && current_msg != NULL) {
		safe_snprintf(str, maxlen, "%lu", (ulong)current_msg->hdr.thread_next);
		return str;
	}
	if(strcmp(sp, "MSG_THREAD_FIRST") == 0 && current_msg != NULL) {
		safe_snprintf(str, maxlen, "%lu", (ulong)current_msg->hdr.thread_first);
		return str;
	}
	if(!strcmp(sp,"SMB_AREA")) {
		if(smb.subnum!=INVALID_SUB && smb.subnum<cfg.total_subs)
			safe_snprintf(str,maxlen,"%s %s"
				,cfg.grp[cfg.sub[smb.subnum]->grp]->sname
				,cfg.sub[smb.subnum]->sname);
		else
			strncpy(str, "Email", maxlen);
		return(str);
	}
	if(!strcmp(sp,"SMB_AREA_DESC")) {
		if(smb.subnum!=INVALID_SUB && smb.subnum<cfg.total_subs)
			safe_snprintf(str,maxlen,"%s %s"
				,cfg.grp[cfg.sub[smb.subnum]->grp]->lname
				,cfg.sub[smb.subnum]->lname);
		else
			strncpy(str, "Personal Email", maxlen);
		return(str);
	}
	if(!strcmp(sp,"SMB_GROUP")) {
		if(smb.subnum!=INVALID_SUB && smb.subnum<cfg.total_subs)
			return(cfg.grp[cfg.sub[smb.subnum]->grp]->sname);
		return(nulstr);
	}
	if(!strcmp(sp,"SMB_GROUP_DESC")) {
		if(smb.subnum!=INVALID_SUB && smb.subnum<cfg.total_subs)
			return(cfg.grp[cfg.sub[smb.subnum]->grp]->lname);
		return(nulstr);
	}
	if(!strcmp(sp,"SMB_GROUP_NUM")) {
		if(smb.subnum!=INVALID_SUB && smb.subnum<cfg.total_subs)
			safe_snprintf(str,maxlen,"%u",getusrgrp(smb.subnum));
		return(str);
	}
	if(!strcmp(sp,"SMB_SUB")) {
		if(smb.subnum==INVALID_SUB)
			return("Mail");
		else if(smb.subnum<cfg.total_subs)
			return(cfg.sub[smb.subnum]->sname);
		return(nulstr);
	}
	if(!strcmp(sp,"SMB_SUB_DESC")) {
		if(smb.subnum==INVALID_SUB)
			return("Mail");
		else if(smb.subnum<cfg.total_subs)
			return(cfg.sub[smb.subnum]->lname);
		return(nulstr);
	}
	if(!strcmp(sp,"SMB_SUB_CODE")) {
		if(smb.subnum==INVALID_SUB)
			return("MAIL");
		else if(smb.subnum<cfg.total_subs)
			return(cfg.sub[smb.subnum]->code);
		return(nulstr);
	}
	if(!strcmp(sp,"SMB_SUB_NUM")) {
		if(smb.subnum!=INVALID_SUB && smb.subnum<cfg.total_subs)
			safe_snprintf(str,maxlen,"%u",getusrsub(smb.subnum));
		return(str);
	}
	if(!strcmp(sp,"SMB_MSGS")) {
		safe_snprintf(str,maxlen,"%lu", (ulong)smb.msgs);
		return(str);
	}
	if(!strcmp(sp,"SMB_CURMSG")) {
		safe_snprintf(str,maxlen,"%lu", (ulong)(smb.curmsg+1));
		return(str);
	}
	if(!strcmp(sp,"SMB_LAST_MSG")) {
		safe_snprintf(str,maxlen,"%lu", (ulong)smb.status.last_msg);
		return(str);
	}
	if(!strcmp(sp,"SMB_MAX_MSGS")) {
		safe_snprintf(str,maxlen,"%lu", (ulong)smb.status.max_msgs);
		return(str);
	}
	if(!strcmp(sp,"SMB_MAX_CRCS")) {
		safe_snprintf(str,maxlen,"%lu", (ulong)smb.status.max_crcs);
		return(str);
	}
	if(!strcmp(sp,"SMB_MAX_AGE")) {
		safe_snprintf(str,maxlen,"%hu",smb.status.max_age);
		return(str);
	}
	if(!strcmp(sp,"SMB_TOTAL_MSGS")) {
		safe_snprintf(str,maxlen,"%lu", (ulong)smb.status.total_msgs);
		return(str);
	}

	/* Currently viewed file */
	if(current_file != NULL) {
		if(current_file->dir < cfg.total_dirs) {
			if(strcmp(sp, "FILE_AREA") == 0) {
				safe_snprintf(str, maxlen, "%s %s"
					,cfg.lib[cfg.dir[current_file->dir]->lib]->sname
					,cfg.dir[current_file->dir]->sname);
				return str;
			}
			if(strcmp(sp, "FILE_AREA_DESC") == 0) {
				safe_snprintf(str, maxlen, "%s %s"
					,cfg.lib[cfg.dir[current_file->dir]->lib]->lname
					,cfg.dir[current_file->dir]->lname);
				return str;
			}
			if(strcmp(sp, "FILE_LIB") == 0)
				return cfg.lib[cfg.dir[current_file->dir]->lib]->sname;
			if(strcmp(sp, "FILE_LIB_DESC") == 0)
				return cfg.lib[cfg.dir[current_file->dir]->lib]->lname;
			if(strcmp(sp, "FILE_LIB_NUM") == 0) {
				safe_snprintf(str, maxlen, "%u", getusrlib(current_file->dir));
				return str;
			}
			if(strcmp(sp, "FILE_DIR") == 0)
				return cfg.dir[current_file->dir]->sname;
			if(strcmp(sp, "FILE_DIR_DESC") == 0)
				return cfg.dir[current_file->dir]->lname;
			if(strcmp(sp, "FILE_DIR_CODE") == 0)
				return cfg.dir[current_file->dir]->code;
			if(strcmp(sp, "FILE_DIR_NUM") == 0) {
				safe_snprintf(str, maxlen, "%u", getusrdir(current_file->dir));
				return str;
			}
		}
		if(strcmp(sp, "FILE_NAME") == 0)
			return current_file->name;
		if(strcmp(sp, "FILE_DESC") == 0)
			return current_file->desc;
		if(strcmp(sp, "FILE_TAGS") == 0)
			return current_file->tags;
		if(strcmp(sp, "FILE_UPLOADER") == 0)
			return current_file->from;
		if(strcmp(sp, "FILE_SIZE") == 0) {
			safe_snprintf(str, maxlen, "%ld", (long)current_file->size);
			return str;
		}
		if(strcmp(sp, "FILE_CREDITS") == 0) {
			safe_snprintf(str, maxlen, "%lu", (ulong)current_file->cost);
			return str;
		}
		if(strcmp(sp, "FILE_TIME") == 0)
			return timestr(current_file->time);
		if(strcmp(sp, "FILE_TIME_ULED") == 0)
			return timestr(current_file->hdr.when_imported.time);
		if(strcmp(sp, "FILE_TIME_DLED") == 0)
			return timestr(current_file->hdr.last_downloaded);
		if(strcmp(sp, "FILE_DATE") == 0)
			return datestr(current_file->time);
		if(strcmp(sp, "FILE_DATE_ULED") == 0)
			return datestr(current_file->hdr.when_imported.time);
		if(strcmp(sp, "FILE_DATE_DLED") == 0)
			return datestr(current_file->hdr.last_downloaded);

		if(strcmp(sp, "FILE_TIMES_DLED") == 0) {
			safe_snprintf(str, maxlen, "%lu", (ulong)current_file->hdr.times_downloaded);
			return str;
		}
	}

	return NULL;
}

