/* Synchronet external program/door section and drop file routines */

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
#include "pcbdefs.hpp"
#include "qbbsdefs.hpp"

/****************************************************************************/
/* This is the external programs (doors) section of the bbs                 */
/* Return 1 if no externals available, 0 otherwise. 						*/
/****************************************************************************/
int sbbs_t::xtrn_sec(const char* section)
{
	char str[MAX_PATH + 1];

	if (cfg.xtrnsec_mod[0] == '\0') {
		errormsg(WHERE, ERR_CHK, "xtrnsec_mod", 0);
		return 1;
	}
	SAFEPRINTF2(str, "%s %s", cfg.xtrnsec_mod, section);
	return exec_bin(str, &main_csi);
}

const char *hungupstr = "\1n\1h%s\1n hung up on \1h%s\1n %s\r\n";

/****************************************************************************/
/* Convert from unix time (seconds since 1/70) to julian (days since 1900)	*/
/****************************************************************************/
int unixtojulian(time_t unix_time)
{
	int       days[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
	int       j;
	struct tm tm;

	if (localtime_r(&unix_time, &tm) == NULL)
		return 0;
	j = 36525L * (1900 + tm.tm_year);
	if (!(j % 100) && TM_MONTH(tm.tm_mon) < 3)
		j--;
	j = (j - (1900 * 36525)) / 100;
	j += tm.tm_mday + days[tm.tm_mon];
	return j;
}

/****************************************************************************/
/* Convert julian date into unix format 									*/
/****************************************************************************/
#ifdef __BORLANDC__
#pragma argsused
#endif
time_t juliantounix(uint j)
{
#if 0 /* julian time */
	int       days[2][12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334,
		                     0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};
	long      temp;
	int       leap, counter;
	struct tm tm;

	if (!j)
		return 0L;

	tm.tm_year = ((100L * j) / 36525L) - 1900;
	temp = (long)date.da_year * 36525L;
	date.da_year += 1900;
	j -= temp / 100L;

	if (!(temp % 100)) {
		j++;
		leap = 1;
	}
	else
		leap = 0;

	for (date.da_mon = counter = 0; counter < 12; counter++)
		if (days[leap][counter] < j)
			date.da_mon = counter;

	date.da_day = j - days[leap][date.da_mon];
	date.da_mon++;  /* go from 0 to 1 based */

	curtime.ti_hour = curtime.ti_min = curtime.ti_sec = 0;
	return dostounix(&date, &curtime);
#else
	return (time_t)-1;
#endif
}

#ifdef __unix__
static void lfexpand(char *str, uint misc)
{
	char * p;
	char   newstr[1024];
	size_t len = 0;

	if (misc & XTRN_NATIVE)
		return;

	for (p = str; *p && len < sizeof(newstr) - 2; p++) {
		if (*p == '\n')
			newstr[len++] = '\r';
		newstr[len++] = *p;
	}

	newstr[len] = 0;
	strcpy(str, newstr);
}
#else
	#define lfexpand(str, misc)
#endif

/****************************************************************************/
/* Creates various types of xtrn (Doors, Chains, etc.) data (drop) files.	*/
/****************************************************************************/
void sbbs_t::xtrndat(const char *name, const char *dropdir, uchar type, uint tleft
                     , uint misc)
{
	char      str[1024], tmp2[128], *p;
	char      tmp[512];
	/* TODO: 16-bit i */
	int16_t   i;
	FILE*     fp;
	int32_t   l;
	struct tm tm;
	struct tm tl;
	stats_t   stats;
	uint max_files = user_downloads_per_day(&cfg, &useron);

	char      node_dir[MAX_PATH + 1];
	char      ctrl_dir[MAX_PATH + 1];
	char      data_dir[MAX_PATH + 1];
	char      exec_dir[MAX_PATH + 1];
	char      text_dir[MAX_PATH + 1];
	char      temp_dir[MAX_PATH + 1];

	SAFECOPY(node_dir, cfg.node_dir);
	SAFECOPY(ctrl_dir, cfg.ctrl_dir);
	SAFECOPY(data_dir, cfg.data_dir);
	SAFECOPY(exec_dir, cfg.exec_dir);
	SAFECOPY(text_dir, cfg.text_dir);
	SAFECOPY(temp_dir, cfg.temp_dir);

	if (!(misc & XTRN_NATIVE)) {
#ifdef _WIN32
		/* Put Micros~1 shortened paths in drop files when running 16-bit DOS programs */
		GetShortPathName(cfg.node_dir, node_dir, sizeof(node_dir));
		GetShortPathName(cfg.ctrl_dir, ctrl_dir, sizeof(ctrl_dir));
		GetShortPathName(cfg.data_dir, data_dir, sizeof(data_dir));
		GetShortPathName(cfg.exec_dir, exec_dir, sizeof(exec_dir));
		GetShortPathName(cfg.text_dir, text_dir, sizeof(text_dir));
		GetShortPathName(cfg.temp_dir, temp_dir, sizeof(temp_dir));
#elif defined(__linux__)
		/* These drive mappings must match the Linux/DOSEMU patch in xtrn.cpp: */
		SAFECOPY(node_dir, DOSEMU_NODE_DIR);
		SAFECOPY(ctrl_dir, DOSEMU_CTRL_DIR);
		SAFECOPY(data_dir, DOSEMU_DATA_DIR);
		SAFECOPY(exec_dir, DOSEMU_EXEC_DIR);
		SAFECOPY(text_dir, DOSEMU_TEXT_DIR);
		SAFECOPY(temp_dir, DOSEMU_TEMP_DIR);
#endif
	}


	if (type == XTRN_SBBS) {   /* SBBS XTRN.DAT file */
		SAFECOPY(tmp, "XTRN.DAT");
		if (misc & XTRN_LWRCASE)
			strlwr(tmp);
		SAFEPRINTF2(str, "%s%s", dropdir, tmp);
		(void)removecase(str);
		if ((fp = fnopen(NULL, str, O_WRONLY | O_CREAT | O_TRUNC | O_TEXT)) == NULL) {
			errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_CREAT | O_TRUNC | O_TEXT);
			return;
		}

		safe_snprintf(str, sizeof(str), "%s\n%s\n%s\n%s\n"
		              , name                    /* User name */
		              , cfg.sys_name            /* System name */
		              , cfg.sys_op              /* Sysop name */
		              , cfg.sys_guru);          /* Guru name */
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		safe_snprintf(str, sizeof(str), "%s\n%s\n%u\n%u\n%u\n%s\n%u\n%" PRIu64 "\n"
		              , ctrl_dir                /* Ctrl dir */
		              , data_dir                /* Data dir */
		              , cfg.sys_nodes           /* Total system nodes */
		              , cfg.node_num            /* Current node */
		              , tleft                   /* User Timeleft in seconds */
		              , (term->supports(ANSI))           /* User ANSI ? (Yes/Mono/No) */
		        ? (term->supports(COLOR))
		        ? "Yes":"Mono":"No"
		              , term->rows                    /* User Screen lines */
		              , user_available_credits(&useron)); /* User Credits */
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		safe_snprintf(str, sizeof(str), "%u\n%u\n%s\n%c\n%u\n%s\n"
		              , useron.level            /* User main level */
		              , useron.level            /* User transfer level */
		              , getbirthmmddyy(&cfg, '/', useron.birth, tmp, sizeof(tmp)) /* User birthday (MM/DD/YY) */
		              , useron.sex ? useron.sex : '?' /* User sex (M/F) */
		              , useron.number           /* User number */
		              , useron.phone);          /* User phone number */
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		safe_snprintf(str, sizeof(str), "%u\n%u\n%x\n%u\n%s\n%s\n"
		              "%s\n%s\n%s\n%s\n%s\n%s\n%lu\n"
		              , misc & (XTRN_STDIO | XTRN_CONIO) ? 0:cfg.com_port /* Com port or 0 if !FOSSIL */
		              , cfg.com_irq             /* Com IRQ */
		              , cfg.com_base            /* Com base in hex */
		              , dte_rate                /* Com rate */
		              , "Yes"                   /* Hardware flow ctrl Y/N */
		              , "Yes"                   /* Locked DTE rate Y/N */
		              , ""                      /* Modem initialization string */
		              , ""                      /* Modem special init string */
		              , ""                      /* Modem terminal mode init str */
		              , ""                      /* Modem dial string */
		              , ""                      /* Modem off-hook string */
		              , ""                      /* Modem answer string */
		              , 0L
		              );
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		SAFEPRINTF(str, "%u\n", cfg.total_xtrns);
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);           /* Total external programs */

		for (i = 0; i < cfg.total_xtrns; i++) {        /* Each program's name */
			if (useron_is_sysop() || chk_ar(cfg.xtrn[i]->ar, &useron, &client)) {
				SAFECOPY(str, cfg.xtrn[i]->name);
			} else
				str[0] = 0;                     /* Blank if no access */
			SAFECAT(str, "\n");
			lfexpand(str, misc);
			fwrite(str, strlen(str), 1, fp);
		}

		SAFEPRINTF2(str, "%s\n%s\n"
		            , u32toaf(useron.flags1, tmp) /* Main flags */
		            , u32toaf(useron.flags2, tmp2)  /* Transfer flags */
		            );
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		safe_snprintf(str, sizeof(str), "%s\n%s\n%x\n%s\n%s\n%s\n"
		              , u32toaf(useron.exempt, tmp) /* Exemptions */
		              , u32toaf(useron.rest, tmp2) /* Restrictions */
		              , useron.expire           /* Expiration date in unix form */
		              , useron.address          /* Address */
		              , useron.location         /* City/State */
		              , useron.zipcode          /* Zip/Postal code */
		              );
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		safe_snprintf(str, sizeof(str), "%s\n%s\n%d\n%s\n%u\n%s\n%s\n%s\n%s\n"
		              "%" PRIx32 "\n%d\n"
		              , u32toaf(useron.flags3, tmp) /* Flag set #3 */
		              , u32toaf(useron.flags4, tmp2) /* Flag set #4 */
		              , 0                       /* Time-slice type */
		              , useron.name             /* Real name/company */
		              , cur_rate                /* DCE rate */
		              , exec_dir
		              , text_dir
		              , temp_dir
		              , cfg.sys_id
		              , cfg.node_misc
		              , misc & (XTRN_STDIO | XTRN_CONIO) ? INVALID_SOCKET : client_socket_dup
		              );
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		fclose(fp);
	}

	else if (type == XTRN_WWIV) {  /*	WWIV CHAIN.TXT File */
		SAFECOPY(tmp, "CHAIN.TXT");
		if (misc & XTRN_LWRCASE)
			strlwr(tmp);
		SAFEPRINTF2(str, "%s%s", dropdir, tmp);
		(void)removecase(str);
		if ((fp = fnopen(NULL, str, O_WRONLY | O_CREAT | O_TRUNC | O_TEXT)) == NULL) {
			errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_CREAT | O_TRUNC | O_TEXT);
			return;
		}

		safe_snprintf(str, sizeof(str), "%u\n%s\n%s\n%s\n%u\n%c\n"
		              , useron.number           /* User number */
		              , name                    /* User name */
		              , useron.name             /* User real name */
		              , nulstr                  /* User call sign */
		              , getage(&cfg, useron.birth) /* User age */
		              , useron.sex ? useron.sex : '?'); /* User sex (M/F) */
		strupr(str);
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		time_t t = useron.laston;
		localtime_r(&t, &tm);
		safe_snprintf(str, sizeof(str), "%" PRIu64 "\n%02u/%02u/%02u\n%u\n%d\n%u\n%u\n%u\n%d\n%u\n"
		              , user_available_credits(&useron) /* Gold */
		              , TM_MONTH(tm.tm_mon)     /* User last on date (MM/DD/YY) */
		              , tm.tm_mday, TM_YEAR(tm.tm_year)
		              , term->cols                    /* User screen width */
		              , term->rows                    /* User screen length */
		              , useron.level            /* User SL */
		              , 0                       /* Cosysop? */
		              , useron_is_sysop()       /* Sysop? (1/0) */
		              , term->supports(ANSI) /* ANSI ? (1/0) */
		              , online == ON_REMOTE);   /* Remote (1/0) */
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		safe_snprintf(str, sizeof(str), "%u\n%s\n%s\n%s\n%u\n%d\n%s\n%s\n"
		              "%u\n%u\n%" PRIu64 "\n%u\n%" PRIu64 "\n%u\n%s\n"
		              , MIN(tleft, INT16_MAX)   /* Time left in seconds */
		              , node_dir                /* Gfiles dir (log dir) */
		              , data_dir                /* Data dir */
		              , "node.log"              /* Name of log file */
		              , dte_rate                /* DTE rate */
		              , cfg.com_port            /* COM port number */
		              , cfg.sys_name            /* System name */
		              , cfg.sys_op              /* Sysop name */
		              , 0                       /* Logon time (sec past 12am) */
		              , 0                       /* Current time (sec past 12am) */
		              , useron.ulb / 1024UL     /* User uploaded kbytes */
		              , useron.uls              /* User uploaded files */
		              , useron.dlb / 1024UL     /* User downloaded kbytes */
		              , useron.dls              /* User downloaded files */
		              , "8N1");                 /* Data, parity, stop bits */
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		fclose(fp);
	}

	else if (type == XTRN_GAP || type == XTRN_DOOR_SYS) {    /* Gap DOOR.SYS File */
		SAFECOPY(tmp, "DOOR.SYS");
		if (misc & XTRN_LWRCASE)
			strlwr(tmp);
		SAFEPRINTF2(str, "%s%s", dropdir, tmp);
		(void)removecase(str);
		if ((fp = fnopen(NULL, str, O_WRONLY | O_CREAT | O_TRUNC | O_TEXT)) == NULL) {
			errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_CREAT | O_TRUNC | O_TEXT);
			return;
		}

		SAFEPRINTF(str, "COM%d:\n"
		           , online == ON_REMOTE ? cfg.com_port:0); /* 01: COM port - 0 if Local */

		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);
		/* Note about door.sys, line 2 (April-24-2005):
		   It *should* be the DCE rate (any number, including the popular modem
		   DCE rates of 14400, 28800, and 33600).  However, according to Deuce,
		   doors created with the DMlib door kit choke with "Error -25: Illegal
		   baud rate" if this isn't a valid FOSSIL baud (DTE) rate, so we're
		   changing this value to the DTE rate until/unless some other doors
		   have an issue with that. <sigh>
		*/
		safe_snprintf(str, sizeof(str), "%u\n%u\n%u\n%u\n%c\n%c\n%c\n%c\n"
		              , dte_rate /* was cur_rate */		/* 02: DCE rate, see note above */
		              , 8                       /* 03: Data bits */
		              , cfg.node_num            /* 04: Node number */
		              , dte_rate                /* 05: DTE rate */
		              , (misc & XTRN_NODISPLAY) ? 'N': 'Y' /* 06: Screen display */
		              , 'Y'                     /* 07: Printer toggle */
		              , 'Y'                     /* 08: Page bell */
		              , 'Y');                   /* 09: Caller alarm */
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		safe_snprintf(str, sizeof(str), "%s\n%s\n%s\n%s\n%s\n"
		              , name                    /* 10: User name */
		              , useron.location         /* 11: User location */
		              , useron.phone            /* 12: User home phone */
		              , useron.phone            /* 13: User work/data phone */
		              , useron.pass);           /* 14: User password */
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		time_t t = useron.laston;
		localtime_r(&t, &tm);
		safe_snprintf(str, sizeof(str), "%u\n%u\n%02u/%02u/%02u\n%u\n%u\n%s\n"
		              , useron.level            /* 15: User security level */
		              , MIN(useron.logons, INT16_MAX) /* 16: User total logons */
		              , TM_MONTH(tm.tm_mon)     /* 17: User last on date */
		              , tm.tm_mday, TM_YEAR(tm.tm_year)
		              , MIN(tleft, INT16_MAX)   /* 18: User time left in sec */
		              , MIN((tleft / 60), INT16_MAX) /* 19: User time left in min */
		              , term->charset() == CHARSET_ASCII     /* 20: GR if COLOR ANSI */
		        ? "7E" : (term->supports(ANSI | COLOR) ? "GR" : "NG"));
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		t = useron.expire;
		localtime_r(&t, &tm);
		safe_snprintf(str, sizeof(str), "%u\n%c\n%s\n%u\n%02u/%02u/%02u\n%u\n%c\n%u\n%u\n"
		              , term->rows                    /* 21: User screen length */
		              , (useron.misc & EXPERT) ? 'Y':'N' /* 22: Expert? (Y/N) */
		              , u32toaf(useron.flags1, tmp2) /* 23: Registered conferences */
		              , 0                       /* 24: Conference came from */
		              , TM_MONTH(tm.tm_mon)     /* 25: User expiration date (MM/DD/YY) */
		              , tm.tm_mday, TM_YEAR(tm.tm_year)
		              , useron.number           /* 26: User number */
		              , useron.prot             /* 27: Default protocol */
		              , useron.uls              /* 28: User total uploads */
		              , useron.dls);            /* 29: User total downloads */
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		t = getnextevent(&cfg, NULL);
		localtime_r(&t, &tm);
		safe_snprintf(str, sizeof(str), "%" PRIu64 "\n%" PRIu64 "\n"
		              , useron.btoday / 1024                     /* 30: Kbytes downloaded today */
		              , user_available_credits(&useron) / 1024UL /* 31: Max Kbytes to download today */
		              );
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		if (type == XTRN_DOOR_SYS) { // 52-line variant
			safe_snprintf(str, sizeof(str), "%s\n%s\n%s\n%s"
			              "\n%s\n%02d:%02d\n%c\n"
			              , getbirthmmddyy(&cfg, '/', useron.birth, tmp, sizeof(tmp)) /* 32: User birthday (MM/DD/YY) */
			              , node_dir                /* 33: Path to MAIN directory */
			              , data_dir                /* 34: Path to GEN directory */
			              , cfg.sys_op              /* 35: Sysop name */
			              , useron.handle           /* 36: Alias name */
			              , tm.tm_hour              /* 37: Event time HH:MM */
			              , tm.tm_min
			              , 'Y');                   /* 38: Error correcting connection */
			lfexpand(str, misc);
			fwrite(str, strlen(str), 1, fp);

			localtime_r(&ns_time, &tm);
			safe_snprintf(str, sizeof(str), "%c\n%c\n%u\n%" PRIu32 "\n%02d/%02d/%02d\n"
			              , (term->flags() & (NO_EXASCII | ANSI | COLOR)) == ANSI
			        ? 'Y':'N'                       /* 39: ANSI supported but NG mode */
			              , 'Y'                     /* 40: Use record locking */
			              , cfg.color[clr_external] /* 41: BBS default color */
			              , MIN(useron.min, INT16_MAX) /* 42: Time credits in minutes */
			              , TM_MONTH(tm.tm_mon)     /* 43: File new-scan date */
			              , tm.tm_mday
			              , TM_YEAR(tm.tm_year));
			lfexpand(str, misc);
			fwrite(str, strlen(str), 1, fp);

			localtime_r(&logontime, &tm);
			localtime32(&useron.laston, &tl);
			safe_snprintf(str, sizeof(str), "%02d:%02d\n%02d:%02d\n%u\n%u\n%" PRIu64 "\n"
			              "%"  PRIu64 "\n%s\n%u\n%u\n"
			              , tm.tm_hour              /* 44: Time of this call */
			              , tm.tm_min
			              , tl.tm_hour              /* 45: Time of last call */
			              , tl.tm_min
			              , MIN(max_files, INT16_MAX) /* 46: Max daily files available */
			              , useron.ptoday           /* 47: Files downloaded so far today */
			              , useron.ulb / 1024UL     /* 48: Total Kbytes uploaded */
			              , useron.dlb / 1024UL     /* 49: Total Kbytes downloaded */
			              , useron.comment          /* 50: User comment */
			              , 0                       /* 51: Total doors opened */
			              , MIN(useron.posts, INT16_MAX)); /* 52: User message left */
			lfexpand(str, misc);
			fwrite(str, strlen(str), 1, fp);
		} else { // 31-line variant, followed by 21 blank lines
			for (i = 0; i < 21; ++i) {
				if (!(misc & XTRN_NATIVE))
					fputc('\r', fp);
				fputc('\n', fp);
			}
		}
		fclose(fp);
	}

	else if (type == XTRN_RBBS || type == XTRN_RBBS1) {
		if (type == XTRN_RBBS) {
			SAFEPRINTF(tmp, "DORINFO%X.DEF", cfg.node_num);   /* support 1-F */
		} else {
			SAFECOPY(tmp, "DORINFO1.DEF");
		}
		if (misc & XTRN_LWRCASE)
			strlwr(tmp);
		SAFEPRINTF2(str, "%s%s", dropdir, tmp);
		(void)removecase(str);
		if ((fp = fnopen(NULL, str, O_WRONLY | O_CREAT | O_TRUNC | O_TEXT)) == NULL) {
			errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_CREAT | O_TRUNC | O_TEXT);
			return;
		}

		SAFECOPY(tmp, cfg.sys_op);
		p = strchr(tmp, ' ');
		if (p)
			*(p++) = 0;
		else
			p = (char*)nulstr;

		safe_snprintf(str, sizeof(str), "%s\n%s\n%s\nCOM%d\n%u BAUD,N,8,1\n%u\n"
		              , cfg.sys_name            /* Name of BBS */
		              , tmp                     /* Sysop's firstname */
		              , p                       /* Sysop's lastname */
		              , online == ON_REMOTE ? cfg.com_port:0 /* COM port number, 0 if local */
		              , dte_rate                /* DTE rate */
		              , 0);                     /* Network type */
		strupr(str);
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		SAFECOPY(tmp, name);
		p = strchr(tmp, ' ');
		if (p)
			*(p++) = 0;
		else
			p = (char*)nulstr;
		safe_snprintf(str, sizeof(str), "%s\n%s\n%s\n%d\n%u\n%u\n"
		              , tmp                     /* User's firstname */
		              , p                       /* User's lastname */
		              , useron.location         /* User's city */
		              , term->supports(ANSI)    /* 1=ANSI 0=ASCII */
		              , useron.level            /* Security level */
		              , MIN((tleft / 60), INT16_MAX)); /* Time left in minutes */
		strupr(str);
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		fclose(fp);

		SAFECOPY(tmp, "EXITINFO.BBS");
		if (misc & XTRN_LWRCASE)
			strlwr(tmp);
		SAFEPRINTF2(str, "%s%s", dropdir, tmp);
		(void)removecase(str);
		if ((fp = fnopen(NULL, str, O_WRONLY | O_CREAT | O_TRUNC)) == NULL) {
			errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_CREAT | O_TRUNC);
			return;
		}
		getstats(&cfg, 0, &stats);
		QBBS::exitinfo exitinfo{};
		exitinfo.BaudRate = (uint16_t)dte_rate;
		exitinfo.SysInfo.CallCount = stats.logons;
		exitinfo.UserInfo.Name = name;
		exitinfo.UserInfo.Location = useron.location;
		exitinfo.UserInfo.DataPhone = useron.phone;
		exitinfo.UserInfo.HomePhone = useron.phone;
		localtime32(&useron.laston, &tm);
		SAFEPRINTF2(tmp, "%02d:%02d", tm.tm_hour, tm.tm_min);
		exitinfo.UserInfo.LastTime = tmp;
		SAFEPRINTF3(tmp, "%02u/%02u/%02u", TM_MONTH(tm.tm_mon), tm.tm_mday, TM_YEAR(tm.tm_year));
		exitinfo.UserInfo.LastDate = tmp;
		if (useron.misc & DELETED)
			exitinfo.UserInfo.Attrib |= QBBS::USER_ATTRIB_DELETED;
		if (useron.misc & CLRSCRN)
			exitinfo.UserInfo.Attrib |= QBBS::USER_ATTRIB_CLRSCRN;
		if (useron.misc & UPAUSE)
			exitinfo.UserInfo.Attrib |= QBBS::USER_ATTRIB_MORE;
		if (term->supports(ANSI))
			exitinfo.UserInfo.Attrib |= QBBS::USER_ATTRIB_ANSI;
		if (useron.sex == 'F')
			exitinfo.UserInfo.Attrib |= QBBS::USER_ATTRIB_FEMALE;
		exitinfo.UserInfo.Flags = useron.flags1;
		exitinfo.UserInfo.TimesPosted = useron.posts;
		exitinfo.UserInfo.SecLvl = useron.level;
		exitinfo.UserInfo.Ups = useron.uls;
		exitinfo.UserInfo.Downs = useron.dls;
		exitinfo.UserInfo.UpK = (uint16_t)(useron.ulb / 1024UL);
		exitinfo.UserInfo.DownK = (uint16_t)(useron.dlb / 1024UL);
		exitinfo.UserInfo.TodayK = (uint16_t)(logon_dlb / 1024UL);
		exitinfo.UserInfo.ScreenLength = (int16_t)term->rows;
		localtime_r(&logontime, &tm);
		SAFEPRINTF2(tmp, "%02d:%02d", tm.tm_hour, tm.tm_min);
		exitinfo.LoginTime = tmp;
		SAFEPRINTF3(tmp, "%02u/%02u/%02u", TM_MONTH(tm.tm_mon), tm.tm_mday, TM_YEAR(tm.tm_year));
		exitinfo.LoginDate = tmp;
		exitinfo.TimeLimit = cfg.level_timepercall[useron.level];
		exitinfo.Credit = (uint32_t)MIN(useron.cdt, UINT32_MAX);
		exitinfo.UserRecNum = useron.number;
		exitinfo.WantChat = (sys_status & SS_SYSPAGE);
		exitinfo.ScreenClear = (useron.misc & CLRSCRN);
		exitinfo.MorePrompts = (useron.misc & UPAUSE);
		exitinfo.GraphicsMode = !(term->charset() == CHARSET_ASCII);
		exitinfo.ExternEdit = (useron.xedit);
		exitinfo.ScreenLength = (int16_t)term->rows;
		exitinfo.MNP_Connect = true;
		exitinfo.ANSI_Capable = term->supports(ANSI);
		exitinfo.RIP_Active = term->supports(RIP);

		fwrite(&exitinfo, sizeof(exitinfo), 1, fp);
		fclose(fp);
	}

	else if (type == XTRN_WILDCAT) { /* WildCat CALLINFO.BBS File */
		SAFECOPY(tmp, "CALLINFO.BBS");
		if (misc & XTRN_LWRCASE)
			strlwr(tmp);
		SAFEPRINTF2(str, "%s%s", dropdir, tmp);
		(void)removecase(str);
		if ((fp = fnopen(NULL, str, O_WRONLY | O_CREAT | O_TRUNC | O_TEXT)) == NULL) {
			errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_CREAT | O_TRUNC | O_TEXT);
			return;
		}

		if (online == ON_LOCAL)
			i = 5;
		else
			switch (dte_rate) {
				case 300:
					i = 1;
					break;
				case 1200:
					i = 2;
					break;
				case 2400:
					i = 0;
					break;
				case 9600:
					i = 3;
					break;
				case 19200:
					i = 4;
					break;
				case 38400:
					i = 6;
					break;
				default:
					i = 7;
					break;
			}
		safe_snprintf(str, sizeof(str), "%s\n%u\n%s\n%u\n%u\n%s\n%s\n%u\n"
		              , name                    /* User name */
		              , i                       /* DTE rate */
		              , useron.location         /* User location */
		              , useron.level            /* Security level */
		              , MIN((tleft / 60), INT16_MAX) /* Time left in min */
		              , term->supports(ANSI) ? "COLOR":"MONO" /* ANSI ??? */
		              , useron.pass             /* Password */
		              , useron.number);         /* User number */
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		localtime_r(&now, &tm);
		safe_snprintf(str, sizeof(str), "%u\n%02d:%02d\n%02d:%02d %02d/%02d/%02d\n%s\n"
		              , MIN(tleft, INT16_MAX) /* Time left in seconds */
		              , tm.tm_hour, tm.tm_min /* Current time HH:MM */
		              , tm.tm_hour, tm.tm_min /* Current time and date HH:MM */
		              , TM_MONTH(tm.tm_mon), tm.tm_mday /* MM/DD/YY */
		              , TM_YEAR(tm.tm_year)
		              , nulstr);                /* Conferences with access */
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		localtime32(&useron.laston, &tm);
		safe_snprintf(str, sizeof(str), "%u\n%u\n%" PRIu64 "\n%" PRIu64 "\n%s\n%02u/%02u/%02u %02u:%02u\n"
		              , useron.dtoday           /* Daily download total */
		              , max_files               /* Max download files */
		              , useron.btoday / 1024    /* Daily download k total */
		              , user_available_credits(&useron) / 1024 /* Max download k total */
		              , useron.phone            /* User phone number */
		              , TM_MONTH(tm.tm_mon)     /* Last on date and time ("MM/DD/YY  HH:MM") */
		              , tm.tm_mday, TM_YEAR(tm.tm_year)
		              , tm.tm_hour
		              , tm.tm_min);
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		localtime_r(&ns_time, &tm);
		safe_snprintf(str, sizeof(str), "%s\n%s\n%02d/%02d/%02d\n%u\n%u\n%u"
		              "\n%u\n%u\n"
		              , useron.misc & EXPERT    /* Expert or Novice mode */
		        ? "EXPERT":"NOVICE"
		              , "All"                   /* Transfer Protocol */
		              , TM_MONTH(tm.tm_mon), tm.tm_mday /* File new-scan date */
		              , TM_YEAR(tm.tm_year)     /* in MM/DD/YY */
		              , useron.logons           /* Total logons */
		              , term->rows                    /* Screen length */
		              , 0                       /* Highest message read */
		              , useron.uls              /* Total files uploaded */
		              , useron.dls);            /* Total files downloaded */
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		safe_snprintf(str, sizeof(str), "%u\n%s\nCOM%u\n%s\n%u\n%s\n%s\n"
		              , 8                       /* Data bits */
		              , online == ON_LOCAL?"LOCAL":"REMOTE" /* Online local or remote */
		              , cfg.com_port            /* COMx port */
		              , getbirthmmddyy(&cfg, '/', useron.birth, tmp, sizeof(tmp)) /* User birthday (MM/DD/YY) */
		              , dte_rate                /* DTE rate */
		              , "FALSE"                 /* Already connected? */
		              , "Normal Connection");   /* Normal or ARQ connect */
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		localtime_r(&now, &tm);
		safe_snprintf(str, sizeof(str), "%02d/%02d/%02d %02d:%02d\n%u\n%u\n"
		              , TM_MONTH(tm.tm_mon), tm.tm_mday /* Current date MM/DD/YY */
		              , TM_YEAR(tm.tm_year)
		              , tm.tm_hour, tm.tm_min   /* Current time HH:MM */
		              , cfg.node_num            /* Node number */
		              , 0);                     /* Door number */
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		fclose(fp);
	}

	else if (type == XTRN_PCBOARD) { /* PCBoard Files */
		SAFECOPY(tmp, "PCBOARD.SYS");
		if (misc & XTRN_LWRCASE)
			strlwr(tmp);
		SAFEPRINTF2(str, "%s%s", dropdir, tmp);
		(void)removecase(str);
		if ((fp = fnopen(NULL, str, O_WRONLY | O_CREAT | O_TRUNC)) == NULL) {
			errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_CREAT | O_TRUNC);
			return;
		}
		PCBoard::sys sys{};
		sys.Screen = !(misc & XTRN_NODISPLAY);
		sys.PageBell = sys_status & SS_SYSPAGE;
		sys.Alarm = startup->sound.answer[0] && !sound_muted(&cfg);
		sys.ErrorCorrected = true;
		sys.GraphicsMode = term->charset() == CHARSET_ASCII ? 'N' : 'Y';
		sys.UserNetStatus = (thisnode.misc & NODE_POFF) ? 'U' : 'A'; /* Node chat status ([A]vailable or [U]navailable) */
		SAFEPRINTF(tmp, "%u", dte_rate);
		sys.ModemSpeed = tmp;
		sys.CarrierSpeed = connection;
		sys.UserRecNo = useron.number;
		SAFECOPY(tmp, name);
		p = strchr(tmp, ' ');
		if (p)
			*p = 0;
		sys.FirstName = tmp;
		sys.Password = useron.pass;
		if (localtime_r(&logontime, &tm) != NULL)
			sys.LogonMinute = (tm.tm_hour * 60) + tm.tm_min;
		SAFEPRINTF2(tmp, "%02d:%02d", tm.tm_hour, tm.tm_min);
		sys.LogonTime = tmp;
		now = time(NULL);
		sys.TimeUsed = -(int16_t)(((now - starttime) / 60) + (time_t)useron.ttoday);/* Negative minutes used */
		sys.PwrdTimeAllowed = cfg.level_timepercall[useron.level];
		sys.Name = name;
		sys.MinutesLeft = (int16_t)(tleft / 60);
		sys.NodeNum = (uint8_t)cfg.node_num;
		sys.EventTime = "00:00";
		sys.UseAnsi = term->supports(ANSI);
		sys.YesChar = yes_key();
		sys.NoChar = no_key();
		sys.Conference2 = cursubnum;

		fwrite(&sys, sizeof(sys), 1, fp);
		fclose(fp);

		SAFECOPY(tmp, "USERS.SYS");
		if (misc & XTRN_LWRCASE)
			strlwr(tmp);
		SAFEPRINTF2(str, "%s%s", dropdir, tmp);
		(void)removecase(str);
		if ((fp = fnopen(NULL, str, O_WRONLY | O_CREAT | O_TRUNC)) == NULL) {
			errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_CREAT | O_TRUNC);
			return;
		}
		PCBoard::usersys user{};
		user.hdr.Version = PCBoard::Version;
		user.hdr.SizeOfRec = sizeof(user.fixed);
		SAFECOPY(user.fixed.Name, name);
		SAFECOPY(user.fixed.City, useron.location);
		SAFECOPY(user.fixed.Password, useron.pass);
		SAFECOPY(user.fixed.BusDataPhone, useron.phone);
		SAFECOPY(user.fixed.HomeVoicePhone, useron.phone);
		user.fixed.ExpertMode = INT_TO_BOOL(useron.misc & EXPERT);
		user.fixed.Protocol = useron.prot;
		user.fixed.SecurityLevel = useron.level;
		user.fixed.NumTimesOn = useron.logons;
		user.fixed.PageLen = (uint8_t)term->rows;
		user.fixed.NumUploads = useron.uls;
		user.fixed.NumDownloads = useron.dls;
		user.fixed.DailyDnldBytes = (uint32_t)logon_dlb;
		SAFECOPY(user.fixed.UserComment, useron.note);
		SAFECOPY(user.fixed.SysopComment, useron.comment);
		user.fixed.ElapsedTimeOn = (int16_t)(now - starttime) / 60;
		user.fixed.RegExpDate = unixtojulian(useron.expire);
		user.fixed.ExpSecurityLevel = cfg.expired_level;
		user.fixed.LastConference = cursubnum;
		user.fixed.ulTotDnldBytes = (uint32_t)MIN(useron.dlb, UINT32_MAX);
		user.fixed.ulTotUpldBytes = (uint32_t)MIN(useron.ulb, UINT32_MAX);
		user.fixed.DeleteFlag = INT_TO_BOOL(useron.misc & DELETED);
		user.fixed.RecNum = useron.number;
		user.fixed.MsgsLeft = useron.posts + useron.emails + useron.fbacks;
		if (useron.misc & CLRSCRN)
			user.fixed.PackedFlags |= PCBoard::USER_FLAG_MSGCLEAR;

		fwrite(&user, sizeof(user), 1, fp);
		fclose(fp);
	}

	else if (type == XTRN_SPITFIRE) {   /* SpitFire SFDOORS.DAT File */
		SAFECOPY(tmp, "SFDOORS.DAT");
		if (misc & XTRN_LWRCASE)
			strlwr(tmp);
		SAFEPRINTF2(str, "%s%s", dropdir, tmp);
		(void)removecase(str);
		if ((fp = fnopen(NULL, str, O_WRONLY | O_CREAT | O_TRUNC | O_TEXT)) == NULL) {
			errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_CREAT | O_TRUNC | O_TEXT);
			return;
		}

		now = time(NULL);
		if (localtime_r(&now, &tm) == NULL)
			l = 0;
		else
			l = (((tm.tm_hour * 60) + tm.tm_min) * 60) + tm.tm_sec;

		SAFECOPY(tmp, name);
		if ((p = strchr(tmp, ' ')) != NULL)
			*p = 0;

		safe_snprintf(str, sizeof(str), "%u\n%s\n%s\n%s\n%u\n%u\n%u\n%" PRId32 "\n"
		              , useron.number           /* User number */
		              , name                    /* User name */
		              , useron.pass             /* Password */
		              , tmp                     /* User's first name */
		              , dte_rate                /* DTE Rate */
		              , cfg.com_port            /* COM Port */
		              , MIN((tleft / 60), INT16_MAX) /* Time left in minutes */
		              , l                       /* Seconds since midnight (now) */
		              );
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		if (localtime_r(&logontime, &tm) == NULL)
			l = 0;
		else
			l = (((tm.tm_hour * 60) + tm.tm_min) * 60) + tm.tm_sec;

		safe_snprintf(str, sizeof(str), "%s\n%s\n%u\n%u\n%u\n%u\n%" PRId32 "\n%" PRIu64 "\n%s\n"
		              "%s\n%s\n%u\n%s\n%u\n%u\n%u\n%u\n%u\n%" PRIu64 "\n%" PRIu64 "\n"
		              "%" PRIu64 "\n%" PRIu64 "\n%s\n%s\n"
		              , dropdir
		              , term->supports(ANSI) ? "TRUE":"FALSE" /* ANSI ? True or False */
		              , useron.level            /* Security level */
		              , useron.uls              /* Total uploads */
		              , useron.dls              /* Total downloads */
		              , cfg.level_timepercall[useron.level]/* Minutes allowed this call */
		              , l                       /* Secs since midnight (logon) */
		              , (uint64_t)(starttime - logontime) /* Extra time in seconds */
		              , "FALSE"                 /* Sysop next? */
		              , "FALSE"                 /* From Front-end? */
		              , "FALSE"                 /* Software Flow Control? */
		              , dte_rate                /* DTE Rate */
		              , "FALSE"                 /* Error correcting connection? */
		              , 0                       /* Current conference */
		              , 0                       /* Current file dir */
		              , cfg.node_num            /* Node number */
		              , max_files               /* Downloads allowed per day */
		              , useron.dtoday           /* Downloads already this day */
		              , user_available_credits(&useron) /* Download bytes allowed/day */
		              , useron.btoday           /* Downloaded bytes already today */
		              , useron.ulb / 1024L      /* Kbytes uploaded */
		              , useron.dlb / 1024L      /* Kbytes downloaded */
		              , useron.phone            /* Phone Number */
		              , useron.location         /* City, State */
		              );
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		fclose(fp);
	}

	else if (type == XTRN_SR) { /* Solar Realms DOORFILE.SR */
		SAFECOPY(tmp, "DOORFILE.SR");
		if (misc & XTRN_LWRCASE)
			strlwr(tmp);
		SAFEPRINTF2(str, "%s%s", dropdir, tmp);
		(void)removecase(str);
		if ((fp = fnopen(NULL, str, O_WRONLY | O_CREAT | O_TRUNC | O_TEXT)) == NULL) {
			errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_CREAT | O_TRUNC | O_TEXT);
			return;
		}

		/* from SRE0994B's SRDOOR.DOC:
		 * The main change from 3.1 was the addition of line 8,
		 * the "Real Name" line.  This line will be used in the
		 * future for BRE's "dupe-checking" feature.
		 */
		safe_snprintf(str, sizeof(str), "%s\n%d \n%d\n%u\n%u\n%u\n%u\n%s\n"
		              , name                    // Complete name or handle of user
		              , term->supports(ANSI) // ANSI status:  1 = yes, 0 = no, -1 = don't know
		              , !(term->charset() == CHARSET_ASCII) // IBM Graphic characters:  1 = yes, 0 = no, -1 = unknown
		              , term->rows                    // Page length of screen, in lines.  Assume 25 if unknown
		              , dte_rate                // Baud Rate:  300, 1200, 2400, 9600, 19200, etc.
		              , online == ON_LOCAL ? 0:cfg.com_port // Com Port:  1, 2, 3, or 4.
		              , MIN((tleft / 60), INT16_MAX) // Time Limit:  (in minutes); -1 if unknown.
		              , useron.name             // Real name (the same as line 1 if not known)
		              );
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);
		fclose(fp);
	}

	else if (type == XTRN_TRIBBS) { /* TRIBBS.SYS */
		SAFECOPY(tmp, "TRIBBS.SYS");
		if (misc & XTRN_LWRCASE)
			strlwr(tmp);
		SAFEPRINTF2(str, "%s%s", dropdir, tmp);
		(void)removecase(str);
		if ((fp = fnopen(NULL, str, O_WRONLY | O_CREAT | O_TRUNC | O_TEXT)) == NULL) {
			errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_CREAT | O_TRUNC | O_TEXT);
			return;
		}

		safe_snprintf(str, sizeof(str), "%u\n%s\n%s\n%u\n%c\n%c\n%u\n%s\n%s\n%s\n"
		              , useron.number           /* User's record number */
		              , name                    /* User's name */
		              , useron.pass             /* User's password */
		              , useron.level            /* User's level */
		              , useron.misc & EXPERT ? 'Y':'N' /* Expert? */
		              , term->supports(ANSI) ? 'Y':'N' /* ANSI? */
		              , MIN((tleft / 60), INT16_MAX) /* Minutes left */
		              , useron.phone            /* User's phone number */
		              , useron.location         /* User's city and state */
		              , getbirthmmddyy(&cfg, '/', useron.birth, tmp, sizeof(tmp)) /* User's birth date (MM/DD/YY) */
		              );
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);

		safe_snprintf(str, sizeof(str), "%u\n%u\n%u\n%u\n%c\n%c\n%s\n%s\n%s\n"
		              , cfg.node_num            /* Node number */
		              , cfg.com_port            /* Serial port */
		              , online == ON_LOCAL ? 0:cur_rate /* Baud rate */
		              , dte_rate                /* Locked rate */
		              , 'Y'
		              , 'Y'                     /* Error correcting connection */
		              , cfg.sys_name            /* Board's name */
		              , cfg.sys_op              /* Sysop's name */
		              , useron.handle           /* User's alias */
		              );
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);
		fclose(fp);
	}

	else if (type == XTRN_DOOR32) { /* DOOR32.SYS */
		SAFECOPY(tmp, "DOOR32.SYS");
		if (misc & XTRN_LWRCASE)
			strlwr(tmp);
		SAFEPRINTF2(str, "%s%s", dropdir, tmp);
		(void)removecase(str);
		if ((fp = fnopen(NULL, str, O_WRONLY | O_CREAT | O_TRUNC | O_TEXT)) == NULL) {
			errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_CREAT | O_TRUNC | O_TEXT);
			return;
		}

		safe_snprintf(str, sizeof(str), "%d\n%d\n%u\n%s%c\n%d\n%s\n%s\n%d\n%d\n"
		              "%d\n%d\n"
		              , misc & (XTRN_STDIO | XTRN_CONIO) ? 0 /* Local */ : 2 /* Telnet */
		, misc & (XTRN_STDIO | XTRN_CONIO) ? INVALID_SOCKET : client_socket_dup
		, dte_rate
		, VERSION_NOTICE, REVISION
		, useron.number
		, useron.name
		, name
		, useron.level
		, tleft / 60
		, term->supports(ANSI)
		, cfg.node_num);
		lfexpand(str, misc);
		fwrite(str, strlen(str), 1, fp);
		fclose(fp);
	}

	else if (type)
		errormsg(WHERE, ERR_CHK, "Drop file type", type);

}

/****************************************************************************/
/* Reads in MODUSER.DAT, EXITINFO.BBS, or DOOR.SYS and modify the current	*/
/* user's values.                                                           */
/****************************************************************************/
void sbbs_t::moduserdat(uint xtrnnum)
{
	char     str[256], path[256], c, startup[MAX_PATH + 1];
	char     tmp[512];
	/* TODO: I don't really like a 16-bit i */
	uint16_t i;
	int      mod;
	int      file;
	FILE *   stream;

	SAFEPRINTF(startup, "%s/", cfg.xtrn[xtrnnum]->path);
	if (cfg.xtrn[xtrnnum]->type == XTRN_RBBS || cfg.xtrn[xtrnnum]->type == XTRN_RBBS1) {
		SAFEPRINTF(path, "%sEXITINFO.BBS", xtrn_dropdir(cfg.xtrn[xtrnnum], startup, sizeof(startup)));
		fexistcase(path);
		if ((file = nopen(path, O_RDONLY)) != -1) {
			lseek(file, 361, SEEK_SET);
			if (read(file, &useron.flags1, 4) != 4) /* Flags */
				errormsg(WHERE, ERR_READ, path, 4);
			putuserflags(useron.number, USER_FLAGS1, useron.flags1);
			lseek(file, 373, SEEK_SET);
			if (read(file, &i, 2) != 2)        /* SecLvl */
				errormsg(WHERE, ERR_READ, path, 2);
			if (i < SYSOP_LEVEL) {
				useron.level = (uint8_t)i;
				putuserdec32(useron.number, USER_LEVEL, useron.level);
			}
			close(file);
			remove(path);
		}
		return;
	}
	else if (cfg.xtrn[xtrnnum]->type == XTRN_GAP || cfg.xtrn[xtrnnum]->type == XTRN_DOOR_SYS) {
		SAFEPRINTF(path, "%sDOOR.SYS", xtrn_dropdir(cfg.xtrn[xtrnnum], startup, sizeof(startup)));
		fexistcase(path);
		if ((stream = fopen(path, "rb")) != NULL) {
			for (i = 0; i < 15; i++)           /* skip first 14 lines */
				if (!fgets(str, 128, stream))
					break;
			if (i == 15 && IS_DIGIT(str[0])) {
				mod = atoi(str);
				if (mod < SYSOP_LEVEL) {
					useron.level = (char)mod;
					putuserdec32(useron.number, USER_LEVEL, useron.level);
				}
			}

			for (; i < 23; i++)
				if (!fgets(str, 128, stream))
					break;
			if (i == 23) {                     /* set main flags */
				useron.flags1 = aftou32(str);
				putuserflags(useron.number, USER_FLAGS1, useron.flags1);
			}

			for (; i < 25; i++)
				if (!fgets(str, 128, stream))
					break;
			if (i == 25 && IS_DIGIT(str[0]) && IS_DIGIT(str[1])
			    && (str[2] == '/' || str[2] == '-') /* xx/xx/xx or xx-xx-xx */
			    && IS_DIGIT(str[3]) && IS_DIGIT(str[4])
			    && (str[5] == '/' || str[5] == '-')
			    && IS_DIGIT(str[6]) && IS_DIGIT(str[7])) { /* valid expire date */
				useron.expire = dstrtounix(MMDDYY, str);
				putuserdatetime(useron.number, USER_EXPIRE, useron.expire);
			}

			for (; i < 29; i++)                  /* line 29, total downloaded files */
				if (!fgets(str, 128, stream))
					break;
			if (i == 29) {
				truncsp(str);
				useron.dls = atoi(str);
				putuserdec32(useron.number, USER_DLS, useron.dls);
			}

			if (fgets(str, 128, stream)) {     /* line 30, Kbytes downloaded today */
				i++;
				truncsp(str);
				mod = atol(str) * 1024L;
				if (mod) {
					useron.dlb = adjustuserval(&cfg, useron.number, USER_DLB, mod);
					subtract_cdt(&cfg, &useron, mod);
				}
			}

			if (cfg.xtrn[xtrnnum]->type == XTRN_DOOR_SYS) {
				for (; i < 42; i++)
					if (!fgets(str, 128, stream))
						break;
				if (i == 42 && IS_DIGIT(str[0])) { /* Time Credits in Minutes */
					useron.min = atol(str);
					putuserdec32(useron.number, USER_MIN, useron.min);
				}
			}

			fclose(stream);
		}
		return;
	}

	else if (cfg.xtrn[xtrnnum]->type == XTRN_PCBOARD) {
		SAFEPRINTF(path, "%sUSERS.SYS", xtrn_dropdir(cfg.xtrn[xtrnnum], startup, sizeof(startup)));
		fexistcase(path);
		if ((file = nopen(path, O_RDONLY)) != -1) {
			lseek(file, 39, SEEK_SET);
			if (read(file, &c, 1) != 1)
				c = 0;
			if (c == 1) {   /* file has been updated */
				lseek(file, 105, SEEK_CUR);   /* read security level */
				if (read(file, &i, 2) != 2) {
					close(file);
					errormsg(WHERE, ERR_READ, path, 2);
					return;
				}
				i = LE_INT(i);
				if (i < SYSOP_LEVEL) {
					useron.level = (uint8_t)i;
					putuserdec32(useron.number, USER_LEVEL, useron.level);
				}
				lseek(file, 75, SEEK_CUR);    /* read in expiration date */
				if (read(file, &i, 2) != 2) {  /* convert from julian to unix */
					close(file);
					errormsg(WHERE, ERR_READ, path, 2);
					return;
				}
				i = LE_INT(i);
				useron.expire = (time32_t)juliantounix(i);
				putuserdatetime(useron.number, USER_EXPIRE, useron.expire);
			}
			close(file);
		}
		return;
	}

	SAFEPRINTF(path, "%sMODUSER.DAT", xtrn_dropdir(cfg.xtrn[xtrnnum], startup, sizeof(startup)));
	fexistcase(path);
	if ((stream = fopen(path, "rb")) != NULL) {           /* File exists */
		if (fgets(str, 81, stream) && (mod = atol(str)) != 0) {
			ultoac(mod > 0L ? mod : -mod, tmp);        /* put commas in the # */
			SAFECOPY(str, "Credit Adjustment: ");
			if (mod < 0L)
				SAFECAT(str, "-");                   /* negative, put '-' */
			SAFECAT(str, tmp);
			if (mod > 0L) {
				SAFECOPY(tmp, "$+");
			} else {
				SAFECOPY(tmp, "$-");
			}
			logline(tmp, str);
			if (mod > 0L)          /* always add to real cdt */
				useron.cdt = adjustuserval(&cfg, useron.number, USER_CDT, mod);
			else
				subtract_cdt(&cfg, &useron, -mod); /* subtract from free cdt first */
		}
		if (fgets(str, 81, stream)) {      /* main level */
			mod = atoi(str);
			if (IS_DIGIT(str[0]) && mod < SYSOP_LEVEL) {
				useron.level = (uchar)mod;
				putuserdec32(useron.number, USER_LEVEL, useron.level);
			}
		}
		if (fgets(str, 81, stream) == NULL) /* was transfer level, now ignored */
			*str = '\0';
		if (fgets(str, 81, stream)) {      /* flags #1 */
			if (strchr(str, '-'))         /* remove flags */
				useron.flags1 &= ~aftou32(str);
			else                        /* add flags */
				useron.flags1 |= aftou32(str);
			putuserflags(useron.number, USER_FLAGS1, useron.flags1);
		}

		if (fgets(str, 81, stream)) {      /* flags #2 */
			if (strchr(str, '-'))         /* remove flags */
				useron.flags2 &= ~aftou32(str);
			else                        /* add flags */
				useron.flags2 |= aftou32(str);
			putuserflags(useron.number, USER_FLAGS2, useron.flags2);
		}

		if (fgets(str, 81, stream)) {      /* exemptions */
			if (strchr(str, '-'))
				useron.exempt &= ~aftou32(str);
			else
				useron.exempt |= aftou32(str);
			putuserflags(useron.number, USER_EXEMPT, useron.exempt);
		}
		if (fgets(str, 81, stream)) {      /* restrictions */
			if (strchr(str, '-'))
				useron.rest &= ~aftou32(str);
			else
				useron.rest |= aftou32(str);
			putuserflags(useron.number, USER_REST, useron.rest);
		}
		if (fgets(str, 81, stream)) {      /* Expiration date */
			if (isxdigit(str[0]))
				putuserdatetime(useron.number, USER_EXPIRE, ahtoul(str));
		}
		if (fgets(str, 81, stream)) {      /* additional minutes */
			mod = atol(str);
			if (mod) {
				SAFEPRINTF(str, "Minute Adjustment: %s", ultoac(mod, tmp));
				logline("*+", str);
				useron.min = (uint32_t)adjustuserval(&cfg, useron.number, USER_MIN, mod);
			}
		}
		if (fgets(str, 81, stream)) {      /* flags #3 */
			if (strchr(str, '-'))         /* remove flags */
				useron.flags3 &= ~aftou32(str);
			else                        /* add flags */
				useron.flags3 |= aftou32(str);
			putuserflags(useron.number, USER_FLAGS3, useron.flags3);
		}

		if (fgets(str, 81, stream)) {      /* flags #4 */
			if (strchr(str, '-'))         /* remove flags */
				useron.flags4 &= ~aftou32(str);
			else                        /* add flags */
				useron.flags4 |= aftou32(str);
			putuserflags(useron.number, USER_FLAGS4, useron.flags4);
		}

		if (fgets(str, 81, stream)) {      /* flags #1 to REMOVE only */
			useron.flags1 &= ~aftou32(str);
			putuserflags(useron.number, USER_FLAGS1, useron.flags1);
		}
		if (fgets(str, 81, stream)) {      /* flags #2 to REMOVE only */
			useron.flags2 &= ~aftou32(str);
			putuserflags(useron.number, USER_FLAGS2, useron.flags2);
		}
		if (fgets(str, 81, stream)) {      /* flags #3 to REMOVE only */
			useron.flags3 &= ~aftou32(str);
			putuserflags(useron.number, USER_FLAGS3, useron.flags3);
		}
		if (fgets(str, 81, stream)) {      /* flags #4 to REMOVE only */
			useron.flags4 &= ~aftou32(str);
			putuserflags(useron.number, USER_FLAGS4, useron.flags4);
		}
		if (fgets(str, 81, stream)) {      /* exemptions to remove */
			useron.exempt &= ~aftou32(str);
			putuserflags(useron.number, USER_EXEMPT, useron.exempt);
		}
		if (fgets(str, 81, stream)) {      /* restrictions to remove */
			useron.rest &= ~aftou32(str);
			putuserflags(useron.number, USER_REST, useron.rest);
		}

		fclose(stream);
		remove(path);
	}
}

const char* sbbs_t::xtrn_dropdir(const xtrn_t* xtrn, char* buf, size_t maxlen)
{
	const char* p = cfg.node_dir;
	if (xtrn->misc & STARTUPDIR)
		p = xtrn->path;
	else if (xtrn->misc & XTRN_TEMP_DIR)
		p = cfg.temp_dir;
	char path[MAX_PATH + 1];
	SAFECOPY(path, p);
	backslash(path);
	strncpy(buf, path, maxlen);
	buf[maxlen - 1] = 0;
	return buf;
}

/****************************************************************************/
/* This function handles configured external program execution. 			*/
/****************************************************************************/
bool sbbs_t::exec_xtrn(uint xtrnnum, bool user_event)
{
	char   str[256], path[MAX_PATH + 1], dropdir[MAX_PATH + 1], name[32], c;
	uint   i;
	int    tleft, mode;
	node_t node;
	time_t start, end;

	if (!chk_ar(cfg.xtrn[xtrnnum]->run_ar, &useron, &client)
	    || !chk_ar(cfg.xtrnsec[cfg.xtrn[xtrnnum]->sec]->ar, &useron, &client)) {
		bputs(text[CantRunThatProgram]);
		return false;
	}

	if (cfg.xtrn[xtrnnum]->cost && !(useron.exempt & FLAG('X'))) {    /* costs */
		if (cfg.xtrn[xtrnnum]->cost > user_available_credits(&useron)) {
			bputs(text[NotEnoughCredits]);
			pause();
			return false;
		}
		subtract_cdt(&cfg, &useron, cfg.xtrn[xtrnnum]->cost);
	}

	if (cfg.prextrn_mod[0] != '\0') {
		SAFEPRINTF2(str, "%s %s", cfg.prextrn_mod, cfg.xtrn[xtrnnum]->code);
		if (exec_bin(str, &main_csi) != 0) {
			return false;
		}
	}

	if (!(cfg.xtrn[xtrnnum]->misc & MULTIUSER)) {
		for (i = 1; i <= cfg.sys_nodes; i++) {
			getnodedat(i, &node);
			c = i;
			if ((node.status == NODE_INUSE || node.status == NODE_QUIET)
			    && node.action == NODE_XTRN && node.aux == (xtrnnum + 1)) {
				if (node.status == NODE_QUIET) {
					SAFECOPY(str, cfg.sys_guru);
					c = cfg.sys_nodes + 1;
				}
				else if (node.misc & NODE_ANON) {
					SAFECOPY(str, text[UNKNOWN_USER]);
				} else
					username(&cfg, node.useron, str);
				bprintf(text[UserRunningXtrn], str
				        , cfg.xtrn[xtrnnum]->name, c);
				pause();
				break;
			}
		}
		if (i <= cfg.sys_nodes)
			return false;
	}

	if (cfg.xtrn[xtrnnum]->misc & XTRN_TEMP_DIR)
		delfiles(cfg.temp_dir, ALLFILES);
	xtrn_dropdir(cfg.xtrn[xtrnnum], dropdir, sizeof(dropdir));
	SAFECOPY(path, dropdir);

	switch (cfg.xtrn[xtrnnum]->type) {
			case XTRN_WWIV:
				SAFECOPY(name, "CHAIN.TXT");
				break;
			case XTRN_DOOR_SYS:
				SAFECOPY(name, "DOOR.SYS");
				break;
			case XTRN_RBBS:
				SAFEPRINTF(name, "DORINFO%X.DEF", cfg.node_num);
				break;
			case XTRN_RBBS1:
				SAFECOPY(name, "DORINFO1.DEF");
				break;
			case XTRN_WILDCAT:
				SAFECOPY(name, "CALLINFO.BBS");
				break;
			case XTRN_PCBOARD:
				SAFECOPY(name, "PCBOARD.SYS");
				break;
			case XTRN_SR:
				SAFECOPY(name, "DOORFILE.SR");
				break;
			case XTRN_TRIBBS:
				SAFECOPY(name, "TRIBBS.SYS");
				break;
			case XTRN_DOOR32:
				SAFECOPY(name, "DOOR32.SYS");
				break;
			default:
				SAFECOPY(name, "XTRN.DAT");
				break;
	}
	if (cfg.xtrn[xtrnnum]->misc & XTRN_LWRCASE)
		strlwr(name);
	SAFECAT(path, name);
	if (action != NODE_PCHT) {
		if (getnodedat(cfg.node_num, &thisnode, true)) {
			thisnode.action = NODE_XTRN;
			thisnode.aux = xtrnnum + 1;
			putnodedat(cfg.node_num, &thisnode);
		}
	}
	putuserstr(useron.number, USER_CURXTRN, cfg.xtrn[xtrnnum]->code);

	if (cfg.xtrn[xtrnnum]->misc & REALNAME) {
		SAFECOPY(name, useron.name);
	} else {
		SAFECOPY(name, useron.alias);
	}
	gettimeleft(cfg.xtrn[xtrnnum]->misc & XTRN_CHKTIME ? true:false);
	tleft = timeleft + (cfg.xtrn[xtrnnum]->textra * 60);
	if (cfg.xtrn[xtrnnum]->maxtime && tleft > (cfg.xtrn[xtrnnum]->maxtime * 60))
		tleft = (cfg.xtrn[xtrnnum]->maxtime * 60);
	xtrndat(name, dropdir, cfg.xtrn[xtrnnum]->type, tleft, cfg.xtrn[xtrnnum]->misc);
	if (!online)
		return false;
	snprintf(str, sizeof(str), "running external %s: %s"
	         , user_event ? "user event" : "program"
	         , cfg.xtrn[xtrnnum]->name);
	logline("X-", str);
	if (cfg.xtrn[xtrnnum]->cmd[0] != '?' &&  cfg.xtrn[xtrnnum]->cmd[0] != '*' && logfile_fp != NULL) {
		fclose(logfile_fp);
		logfile_fp = NULL;
	}

	SAFEPRINTF(str, "%shangup.now", cfg.node_dir);
	(void)removecase(str);

	mode = 0;
	if (cfg.xtrn[xtrnnum]->misc & XTRN_SH)
		mode |= EX_SH;
	if (cfg.xtrn[xtrnnum]->misc & XTRN_STDIO)
		mode |= EX_STDIO;
	else if (cfg.xtrn[xtrnnum]->misc & XTRN_CONIO)
		mode |= EX_CONIO;
	else if (cfg.xtrn[xtrnnum]->misc & XTRN_UART)
		mode |= EX_UART;
	else if (cfg.xtrn[xtrnnum]->misc & XTRN_FOSSIL)
		mode |= EX_FOSSIL;
	mode |= (cfg.xtrn[xtrnnum]->misc & (XTRN_CHKTIME | XTRN_NATIVE | XTRN_NOECHO | XTRN_NODISPLAY | WWIVCOLOR));
	if (cfg.xtrn[xtrnnum]->misc & MODUSERDAT) {        /* Delete MODUSER.DAT */
		SAFEPRINTF(str, "%sMODUSER.DAT", dropdir);    /* if for some weird  */
		(void)removecase(str);                      /* reason it's there  */
	}

	char drop_file[MAX_PATH + 1];
	char startup_dir[MAX_PATH + 1];
#if defined(__linux__)
	if (cfg.xtrn[xtrnnum]->cmd[0] != '?' && cfg.xtrn[xtrnnum]->cmd[0] != '*' && !(cfg.xtrn[xtrnnum]->misc & XTRN_NATIVE)) {
		SAFEPRINTF2(startup_dir, "%s\\%s", DOSEMU_XTRN_DRIVE, getdirname(cfg.xtrn[xtrnnum]->path));
		backslash(startup_dir);
		if (cfg.xtrn[xtrnnum]->misc & STARTUPDIR)
			SAFEPRINTF2(drop_file, "%s%s", startup_dir, getfname(path));
		else
			SAFEPRINTF2(drop_file, "%s\\%s", DOSEMU_NODE_DRIVE, getfname(path));
	}
	else
#endif
	{
		cmdstr(cfg.xtrn[xtrnnum]->path, "", "", startup_dir);
		SAFECOPY(drop_file, path);
	}

	start = time(NULL);
	if (cfg.xtrn[xtrnnum]->max_inactivity > 0)
		max_socket_inactivity = cfg.xtrn[xtrnnum]->max_inactivity;

	char topic[128];
	snprintf(topic, sizeof(topic), "exec/%s", cfg.xtrn[xtrnnum]->code);
	snprintf(str, sizeof(str), "%u\t%s", useron.number, useron.alias);
	mqtt_pub_timestamped_msg(mqtt, TOPIC_BBS_ACTION, topic, start, str);

	external(cmdstr(cfg.xtrn[xtrnnum]->cmd, drop_file, startup_dir, NULL, mode)
	         , mode
	         , cfg.xtrn[xtrnnum]->path);
	end = time(NULL);

	if (cfg.xtrn[xtrnnum]->misc & FREETIME)
		starttime += end - start;
	if (cfg.xtrn[xtrnnum]->clean[0]) {
		external(cmdstr(cfg.xtrn[xtrnnum]->clean, drop_file, startup_dir, NULL, mode)
		         , mode & ~(EX_STDIN | EX_CONIO), cfg.xtrn[xtrnnum]->path);
	}
	max_socket_inactivity = startup->max_session_inactivity;
	/* Re-open the logfile */
	if (logfile_fp == NULL) {
		SAFEPRINTF(str, "%snode.log", cfg.node_dir);
		if ((logfile_fp = fopen(str, "a+b")) == NULL)
			errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_CREAT | O_APPEND);
	}

	SAFEPRINTF(str, "%shangup.now", cfg.node_dir);
	if (fexistcase(str)) {
		lprintf(LOG_NOTICE, "Node %d External program requested hangup (%s signaled)"
		        , cfg.node_num, str);
		(void)removecase(str);
		hangup();
	}
	else if (!online) {
		SAFEPRINTF(str, "%shungup.log", cfg.logs_dir);
		FILE* fp = fopenlog(&cfg, str, /* shareable: */false);
		if (fp == NULL)
			errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_CREAT | O_APPEND);
		else {
			now = time(NULL);
			fprintf(fp, hungupstr, useron.alias, cfg.xtrn[xtrnnum]->name, timestr(now));
			fcloselog(fp);
		}
	}
	if (cfg.xtrn[xtrnnum]->misc & MODUSERDAT)  /* Modify user data */
		moduserdat(xtrnnum);

	if (getnodedat(cfg.node_num, &thisnode, true)) {
		thisnode.aux = 0;
		putnodedat(cfg.node_num, &thisnode);
	}

	if (cfg.xtrn[xtrnnum]->misc & XTRN_PAUSE)
		pause();

	if (cfg.postxtrn_mod[0] != '\0') {
		SAFEPRINTF2(str, "%s %s", cfg.postxtrn_mod, cfg.xtrn[xtrnnum]->code);
		exec_bin(str, &main_csi);
	}

	return true;
}

/****************************************************************************/
/* This function will execute an external program if it is configured to    */
/* run during the event specified.                                          */
/****************************************************************************/
bool sbbs_t::user_event(user_event_t event)
{
	int  i;
	bool success = false;

	for (i = 0; i < cfg.total_xtrns; i++) {
		if (cfg.xtrn[i]->event != event)
			continue;
		if (!chk_ar(cfg.xtrn[i]->ar, &useron, &client)
		    || !chk_ar(cfg.xtrn[i]->run_ar, &useron, &client)
		    || !chk_ar(cfg.xtrnsec[cfg.xtrn[i]->sec]->ar, &useron, &client))
			continue;
		success = exec_xtrn(i, /* user_event: */ true);
	}

	return success;
}


