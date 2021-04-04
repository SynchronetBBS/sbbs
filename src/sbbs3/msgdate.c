/* Synchronet RFC822 message date/time string conversion routines */

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

#include "msgdate.h"
#include "smblib.h"
#include "datewrap.h"
#include "date_str.h"

/****************************************************************************/
/* Convert when_t structure to RFC822 date header field (string)			*/
/****************************************************************************/
char* DLLCALL msgdate(when_t when, char* buf)
{
	struct tm	tm;
	char		plus='+';
	short		tz;
	time_t		tt;
	
	tz=smb_tzutc(when.zone);
	if(tz<0) {
		plus='-';
		tz=-tz;
	}

	tt=when.time;
	if(localtime_r(&tt,&tm)==NULL)
		memset(&tm,0,sizeof(tm));
	sprintf(buf,"%s, %d %s %d %02d:%02d:%02d %c%02u%02u"
		,wday[tm.tm_wday]
		,tm.tm_mday
		,mon[tm.tm_mon]
		,1900+tm.tm_year
		,tm.tm_hour
		,tm.tm_min
		,tm.tm_sec
		/* RFC1123: implementations SHOULD use numeric timezones instead of timezone names */
		,plus, tz/60, tz%60	
		);
	return(buf);
}

/****************************************************************************/
/* Convert RFC822 date header field to when_t structure						*/
/* dd mon yyyy hh:mm:ss [zone]												*/
/****************************************************************************/
when_t DLLCALL rfc822date(char* date)
{
	char*	p=date;
	char	str[32];
	char	month[32];
	when_t	when;
	struct tm tm;

	memset(&tm,0,sizeof(tm));
	memset(&when,0,sizeof(when));

	while(*p && *p<=' ') p++;
	while(*p && !IS_DIGIT(*p)) p++;
	/* DAY */
	tm.tm_mday=atoi(p);
	while(*p && IS_DIGIT(*p)) p++;
	/* MONTH */
	while(*p && *p<=' ') p++;
	sprintf(month,"%3.3s",p);
	if(!stricmp(month,"jan"))
		tm.tm_mon=0;
	else if(!stricmp(month,"feb"))
		tm.tm_mon=1;
	else if(!stricmp(month,"mar"))
		tm.tm_mon=2;
	else if(!stricmp(month,"apr"))
		tm.tm_mon=3;
	else if(!stricmp(month,"may"))
		tm.tm_mon=4;
	else if(!stricmp(month,"jun"))
		tm.tm_mon=5;
	else if(!stricmp(month,"jul"))
		tm.tm_mon=6;
	else if(!stricmp(month,"aug"))
		tm.tm_mon=7;
	else if(!stricmp(month,"sep"))
		tm.tm_mon=8;
	else if(!stricmp(month,"oct"))
		tm.tm_mon=9;
	else if(!stricmp(month,"nov"))
		tm.tm_mon=10;
	else
		tm.tm_mon=11;
	p+=4;
	/* YEAR */
	tm.tm_year=atoi(p);
	if(tm.tm_year<Y2K_2DIGIT_WINDOW)
		tm.tm_year+=100;
	else if(tm.tm_year>1900)
		tm.tm_year-=1900;

	while(*p && IS_DIGIT(*p)) p++;
	/* HOUR */
	while(*p && *p<=' ') p++;
	tm.tm_hour=atoi(p);
	while(*p && IS_DIGIT(*p)) p++;
	/* MINUTE */
	if(*p) p++;
	tm.tm_min=atoi(p);
	while(*p && IS_DIGIT(*p)) p++;
	/* SECONDS */
	if(*p) p++;
	tm.tm_sec=atoi(p);
	while(*p && IS_DIGIT(*p)) p++;
	/* TIME ZONE */
	while(*p && *p<=' ') p++;
	if(*p) {
		if(IS_DIGIT(*p) || *p=='-' || *p=='+') { /* [+|-]HHMM format */
			if(*p=='+') p++;
			sprintf(str,"%.*s",*p=='-'? 3:2,p);
			when.zone=atoi(str)*60;
			p+=(*p=='-') ? 3:2;
			if(when.zone<0)
				when.zone-=atoi(p);
			else
				when.zone+=atoi(p);
		}
		else if(!strnicmp(p,"PDT",3))
			when.zone=(short)PDT;
		else if(!strnicmp(p,"MDT",3))
			when.zone=(short)MDT;
		else if(!strnicmp(p,"CDT",3))
			when.zone=(short)CDT;
		else if(!strnicmp(p,"EDT",3))
			when.zone=(short)EDT;
		else if(!strnicmp(p,"PST",3))
			when.zone=(short)PST;
		else if(!strnicmp(p,"MST",3))
			when.zone=(short)MST;
		else if(!strnicmp(p,"CST",3))
			when.zone=(short)CST;
		else if(!strnicmp(p,"EST",3))
			when.zone=(short)EST;
	}

	tm.tm_isdst=-1;	/* Don't adjust for daylight-savings-time */
	when.time=(uint32_t)mktime(&tm);

	return(when);
}
