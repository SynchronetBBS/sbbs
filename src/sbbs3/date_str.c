/* date_str.c */

/* Synchronet date/time string conversion routines */

/* $Id: date_str.c,v 1.29 2016/05/27 07:44:46 rswindell Exp $ */

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
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "sbbs.h"

const char *wday[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
const char *mon[]={"Jan","Feb","Mar","Apr","May","Jun"
            ,"Jul","Aug","Sep","Oct","Nov","Dec"};

/****************************************************************************/
/* Converts a date string in format MM/DD/YY into unix time format			*/
/****************************************************************************/
time32_t DLLCALL dstrtounix(scfg_t* cfg, const char *instr)
{
	const char*	p;
	const char*	day;
	char		str[16];
	struct tm	tm;

	if(!instr[0] || !strncmp(instr,"00/00/00",8))
		return(0);

	if(isdigit(instr[0]) && isdigit(instr[1])
		&& isdigit(instr[3]) && isdigit(instr[4])
		&& isdigit(instr[6]) && isdigit(instr[7]))
		p=instr;	/* correctly formatted */
	else {
		p=instr;	/* incorrectly formatted */
		while(*p && isdigit(*p)) p++;
		if(*p==0)
			return(0);
		p++;
		day=p;
		while(*p && isdigit(*p)) p++;
		if(*p==0)
			return(0);
		p++;
		sprintf(str,"%02u/%02u/%02u"
			,atoi(instr)%100,atoi(day)%100,atoi(p)%100);
		p=str;
	}

	memset(&tm,0,sizeof(tm));
	tm.tm_year=((p[6]&0xf)*10)+(p[7]&0xf);
	if (tm.tm_year<Y2K_2DIGIT_WINDOW)
		tm.tm_year+=100;
	if(cfg->sys_misc&SM_EURODATE) {
		tm.tm_mon=((p[3]&0xf)*10)+(p[4]&0xf);
		tm.tm_mday=((p[0]&0xf)*10)+(p[1]&0xf); 
	}
	else {
		tm.tm_mon=((p[0]&0xf)*10)+(p[1]&0xf);
		tm.tm_mday=((p[3]&0xf)*10)+(p[4]&0xf); 
	}
	if (tm.tm_mon)
		tm.tm_mon--;	/* zero-based month field */
	tm.tm_isdst=-1;		/* Do not adjust for DST */
	return(mktime32(&tm));
}

/****************************************************************************/
/* Converts unix time format (long - time_t) into a char str MM/DD/YY		*/
/****************************************************************************/
char* DLLCALL unixtodstr(scfg_t* cfg, time32_t t, char *str)
{
	struct tm tm;
	time_t unix_time=t;

	if(!unix_time)
		strcpy(str,"00/00/00");
	else {
		if(localtime_r(&unix_time,&tm)==NULL) {
			strcpy(str,"00/00/00");
			return(str);
		}
		if(tm.tm_mon>11) {	  /* DOS leap year bug */
			tm.tm_mon=0;
			tm.tm_year++; 
		}
		if(tm.tm_mday>31)
			tm.tm_mday=1;
		if(cfg->sys_misc&SM_EURODATE)
			sprintf(str,"%02u/%02u/%02u",tm.tm_mday,tm.tm_mon+1
				,TM_YEAR(tm.tm_year));
		else
			sprintf(str,"%02u/%02u/%02u",tm.tm_mon+1,tm.tm_mday
				,TM_YEAR(tm.tm_year)); 
	}
	return(str);
}

/****************************************************************************/
/* Takes the value 'sec' and makes a string the format HH:MM:SS             */
/****************************************************************************/
char* DLLCALL sectostr(uint sec,char *str)
{
    uchar hour,min,sec2;

	hour=(sec/60)/60;
	min=(sec/60)-(hour*60);
	sec2=sec-((min+(hour*60))*60);
	sprintf(str,"%2.2d:%2.2d:%2.2d",hour,min,sec2);
	return(str);
}

/* Returns a shortened version of "HH:MM:SS" formatted seconds value */
char* DLLCALL seconds_to_str(uint seconds, char* str)
{
	char* p = sectostr(seconds, str);
	while(*p=='0' || *p==':')
		p++;
	return p;
}

/****************************************************************************/
/****************************************************************************/
char* DLLCALL hhmmtostr(scfg_t* cfg, struct tm* tm, char* str)
{
	if(cfg->sys_misc&SM_MILITARY)
		sprintf(str,"%02d:%02d "
	        ,tm->tm_hour,tm->tm_min);
	else
		sprintf(str,"%02d:%02d%c"
	        ,tm->tm_hour>12 ? tm->tm_hour-12 : tm->tm_hour==0 ? 12 : tm->tm_hour
			,tm->tm_min,tm->tm_hour>=12 ? 'p' : 'a');
	return(str);
}

/****************************************************************************/
/* Generates a 24 character ASCII string that represents the time_t pointer */
/* Used as a replacement for ctime()                                        */
/****************************************************************************/
char* DLLCALL timestr(scfg_t* cfg, time32_t t, char* str)
{
    char*		mer;
	uchar		hour;
    struct tm	tm;
	time_t		intime=t;

	if(localtime_r(&intime,&tm)==NULL) {
		strcpy(str,"Invalid Time");
		return(str); 
	}
	if(cfg->sys_misc&SM_MILITARY) {
		sprintf(str,"%s %s %02u %4u %02u:%02u:%02u"
			,wday[tm.tm_wday],mon[tm.tm_mon],tm.tm_mday,1900+tm.tm_year
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
	sprintf(str,"%s %s %02u %4u %02u:%02u %s"
		,wday[tm.tm_wday],mon[tm.tm_mon],tm.tm_mday,1900+tm.tm_year
		,hour,tm.tm_min,mer);
	return(str);
}

