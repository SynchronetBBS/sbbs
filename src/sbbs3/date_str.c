/* Synchronet date/time string conversion routines */

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

#include "date_str.h"
#include "datewrap.h"
#include "xpdatetime.h"
#include "text.h"

const char *wday[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
const char *mon[]={"Jan","Feb","Mar","Apr","May","Jun"
            ,"Jul","Aug","Sep","Oct","Nov","Dec"};

/****************************************************************************/
/****************************************************************************/
char* date_format(scfg_t* cfg, char* buf, size_t size)
{
	switch (cfg->sys_date_fmt) {
		case DDMMYY: snprintf(buf, size, "DD%cMM%cYY", cfg->sys_date_sep, cfg->sys_date_sep); return buf;
		case MMDDYY: snprintf(buf, size, "MM%cDD%cYY", cfg->sys_date_sep, cfg->sys_date_sep); return buf;
		case YYMMDD: snprintf(buf, size, "YY%cMM%cDD", cfg->sys_date_sep, cfg->sys_date_sep); return buf;
	}
	return "????????";
}

/****************************************************************************/
/* Assumes buf is at least 9 bytes in size									*/
/****************************************************************************/
char* date_template(scfg_t* cfg, char* buf, size_t size)
{
	snprintf(buf, size, "nn%cnn%cnn", cfg->sys_date_sep, cfg->sys_date_sep);
	return buf;
}

#define DECVAL(ch, mul)	(DEC_CHAR_TO_INT(ch) * (mul))

/****************************************************************************/
/* Converts a date string in numeric format (e.g. MM/DD/YY) to time_t		*/
/****************************************************************************/
time32_t dstrtounix(enum date_fmt fmt, const char *instr)
{
	const char*	p;
	const char*	day;
	char		str[16];
	struct tm	tm;

	if(instr == NULL || !instr[0] || !strncmp(instr,"00/00/00",8))
		return(0);

	if(IS_DIGIT(instr[0]) && IS_DIGIT(instr[1])
		&& IS_DIGIT(instr[3]) && IS_DIGIT(instr[4])
		&& IS_DIGIT(instr[6]) && IS_DIGIT(instr[7]))
		p=instr;	/* correctly formatted */
	else {
		p=instr;	/* incorrectly formatted */
		while(*p && IS_DIGIT(*p)) p++;
		if(*p==0)
			return(0);
		p++;
		day=p;
		while(*p && IS_DIGIT(*p)) p++;
		if(*p==0)
			return(0);
		p++;
		sprintf(str,"%02u/%02u/%02u"
			,atoi(instr)%100,atoi(day)%100,atoi(p)%100);
		p=str;
	}

	memset(&tm,0,sizeof(tm));
	if (fmt == YYMMDD) {
		tm.tm_year = DECVAL(p[0], 10) + DECVAL(p[1], 1);
		tm.tm_mon = DECVAL(p[3], 10) + DECVAL(p[4], 1);
		tm.tm_mday = DECVAL(p[6], 10) + DECVAL(p[7], 1);
	} else {
		tm.tm_year=((p[6]&0xf)*10)+(p[7]&0xf);
		if(fmt == DDMMYY) {
			tm.tm_mon=((p[3]&0xf)*10)+(p[4]&0xf);
			tm.tm_mday=((p[0]&0xf)*10)+(p[1]&0xf); 
		}
		else {
			tm.tm_mon=((p[0]&0xf)*10)+(p[1]&0xf);
			tm.tm_mday=((p[3]&0xf)*10)+(p[4]&0xf); 
		}
	}
	if (tm.tm_year<Y2K_2DIGIT_WINDOW)
		tm.tm_year+=100;
	if (tm.tm_mon)
		tm.tm_mon--;	/* zero-based month field */
	tm.tm_isdst=-1;		/* Do not adjust for DST */
	return(mktime32(&tm));
}

/****************************************************************************/
/* Converts unix time format (long - time_t) into a char str MM/DD/YY		*/
/****************************************************************************/
char* unixtodstr(scfg_t* cfg, time32_t t, char *str)
{
	struct tm tm = {0};
	time_t unix_time=t;

	if (unix_time != 0) {
		if (localtime_r(&unix_time, &tm) != NULL) {
			if(tm.tm_mon>11) {	  /* DOS leap year bug */
				tm.tm_mon=0;
				tm.tm_year++;
			}
			if(tm.tm_mday>31)
				tm.tm_mday=1;
			++tm.tm_mon;
		}
	}
	if (cfg->sys_date_fmt == YYMMDD)
		sprintf(str,"%02u%c%02u%c%02u"
			,TM_YEAR(tm.tm_year)
			,cfg->sys_date_sep
			,tm.tm_mon
			,cfg->sys_date_sep
			,tm.tm_mday);
	else if(cfg->sys_date_fmt == DDMMYY)
		sprintf(str,"%02u%c%02u%c%02u"
			,tm.tm_mday
			,cfg->sys_date_sep
			,tm.tm_mon
			,cfg->sys_date_sep
			,TM_YEAR(tm.tm_year));
	else
		sprintf(str,"%02u%c%02u%c%02u"
			,tm.tm_mon
			,cfg->sys_date_sep
			,tm.tm_mday
			,cfg->sys_date_sep
			,TM_YEAR(tm.tm_year));
	return str;
}

/****************************************************************************/
/* Return 8-char numeric or verbal date	or "never" when passed 0			*/
/****************************************************************************/
char* datestr(scfg_t* cfg, time_t t, char* str)
{
	if(t == 0)
		return cfg->text == NULL ? "--------" : cfg->text[Never];
	if(!cfg->sys_date_verbal)
		return unixtodstr(cfg, (time32_t)t, str);
	return verbal_datestr(cfg, t, str);
}

/****************************************************************************/
/* return 8-char numeric date												*/
/****************************************************************************/
char* verbal_datestr(scfg_t* cfg, time_t t, char* str)
{
	struct tm tm = {0};
	if(localtime_r(&t, &tm) == NULL)
		return "!!!!!!!!";
	char fmt[32] = "";
	switch(cfg->sys_date_fmt) {
		case MMDDYY:
			snprintf(fmt, sizeof fmt, "%%b%%d%c%%y", cfg->sys_date_sep);
			break;
		case DDMMYY:
			snprintf(fmt, sizeof fmt, "%%d%c%%b%%y", cfg->sys_date_sep);
			break;
		case YYMMDD:
			snprintf(fmt, sizeof fmt, "%%y%c%%b%%d", cfg->sys_date_sep);
			break;
	}
	strftime(str, 9, fmt, &tm);
	return str;
}

/****************************************************************************/
/* Takes the value 'sec' and makes a string the format HH:MM:SS             */
/****************************************************************************/
char* sectostr(uint sec,char *str)
{
    uchar hour,min,sec2;

	hour=(sec/60)/60;
	min=(sec/60)-(hour*60);
	sec2=sec-((min+(hour*60))*60);
	sprintf(str,"%2.2d:%2.2d:%2.2d",hour,min,sec2);
	return(str);
}

/* Returns a shortened version of "HH:MM:SS" formatted seconds value */
char* seconds_to_str(uint seconds, char* str)
{
	char* p = sectostr(seconds, str);
	while(*p=='0' || *p==':')
		p++;
	return p;
}

/* Returns a duration in minutes into a string */
char* minutes_to_str(uint min, char* str, size_t size)
{
	safe_snprintf(str, size, "%ud %uh %um"
		,min / (24 *60)
		,(min % (24 * 60)) / 60
		,min % 60);
	return str;
}

/****************************************************************************/
/* Returns 5 or 6 character string, depending on configuration				*/
/****************************************************************************/
char* tm_as_hhmm(scfg_t* cfg, struct tm* tm, char* str)
{
	if(cfg != NULL && (cfg->sys_misc & SM_MILITARY))
		sprintf(str,"%02d:%02d"
	        ,tm->tm_hour,tm->tm_min);
	else
		sprintf(str,"%02d:%02d%c"
	        ,tm->tm_hour>12 ? tm->tm_hour-12 : tm->tm_hour==0 ? 12 : tm->tm_hour
			,tm->tm_min,tm->tm_hour>=12 ? 'p' : 'a');
	return(str);
}

/****************************************************************************/
/* Returns 5 or 6 character string, depending on configuration				*/
/****************************************************************************/
char* time_as_hhmm(scfg_t* cfg, time_t t, char* str)
{
	struct tm tm;
	if(t == INVALID_TIME || localtime_r(&t, &tm) == NULL) {
		strcpy(str,"??:??");
		return str;
	}
	return tm_as_hhmm(cfg, &tm, str);
}

/****************************************************************************/
/* Generates a 24 character ASCII string that represents the time_t pointer */
/* Used as a replacement for ctime()                                        */
/****************************************************************************/
char* timestr(scfg_t* cfg, time32_t t, char* str)
{
    char*		mer;
	uchar		hour;
    struct tm	tm;
	time_t		intime=t;
	char** w = (cfg->text == NULL) ? (char**)wday : &cfg->text[Sun];
	char** m = (cfg->text == NULL) ? (char**)mon : &cfg->text[Jan];

	if(localtime_r(&intime,&tm)==NULL) {
		strcpy(str,"Invalid Time");
		return(str);
	}
	if(cfg->sys_misc&SM_MILITARY) {
		snprintf(str, LEN_DATETIME + 1, "%s %s %02u %4u %02u:%02u:%02u"
			,w[tm.tm_wday],m[tm.tm_mon],tm.tm_mday,1900+tm.tm_year
			,tm.tm_hour,tm.tm_min,tm.tm_sec);
		return(str);
	}
	if(tm.tm_hour>=12) {
		if(tm.tm_hour==12)
			hour=12;
		else
			hour=tm.tm_hour-12;
		mer="pm";
	} else {
		if(tm.tm_hour==0)
			hour=12;
		else
			hour=tm.tm_hour;
		mer="am";
	}
	snprintf(str, LEN_DATETIME + 1, "%s %s %02u %4u %02u:%02u %s"
		,w[tm.tm_wday],m[tm.tm_mon],tm.tm_mday,1900+tm.tm_year
		,hour,tm.tm_min,mer);
	return(str);
}

