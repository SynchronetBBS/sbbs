/* date_str.c */

/* Synchronet date/time string conversion routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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
time_t DLLCALL dstrtounix(scfg_t* cfg, char *instr)
{
	char*	p;
	char*	day;
	char	str[16];
	struct tm tm;

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
		tm.tm_mday=((p[0]&0xf)*10)+(p[1]&0xf); }
	else {
		tm.tm_mon=((p[0]&0xf)*10)+(p[1]&0xf);
		tm.tm_mday=((p[3]&0xf)*10)+(p[4]&0xf); }
	if (tm.tm_mon)
		tm.tm_mon--;	/* zero-based month field */
	return(mktime(&tm));
}

/****************************************************************************/
/* Converts unix time format (long - time_t) into a char str MM/DD/YY		*/
/****************************************************************************/
char* DLLCALL unixtodstr(scfg_t* cfg, time_t unix_time, char *str)
{
	struct tm tm;

	if(!unix_time)
		strcpy(str,"00/00/00");
	else {
		if(localtime_r(&unix_time,&tm)==NULL) {
			strcpy(str,"00/00/00");
			return(str);
		}
		if(tm.tm_mon>11) {	  /* DOS leap year bug */
			tm.tm_mon=0;
			tm.tm_year++; }
		if(tm.tm_mday>31)
			tm.tm_mday=1;
		if(cfg->sys_misc&SM_EURODATE)
			sprintf(str,"%02u/%02u/%02u",tm.tm_mday,tm.tm_mon+1
				,TM_YEAR(tm.tm_year));
		else
			sprintf(str,"%02u/%02u/%02u",tm.tm_mon+1,tm.tm_mday
				,TM_YEAR(tm.tm_year)); }
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
char* DLLCALL timestr(scfg_t* cfg, time_t *intime, char* str)
{
    char*		mer;
	uchar		hour;
    struct tm	tm;

	if(localtime_r(intime,&tm)==NULL) {
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
	while(*p && !isdigit(*p)) p++;
	/* DAY */
	tm.tm_mday=atoi(p);
	while(*p && isdigit(*p)) p++;
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

	while(*p && isdigit(*p)) p++;
	/* HOUR */
	while(*p && *p<=' ') p++;
	tm.tm_hour=atoi(p);
	while(*p && isdigit(*p)) p++;
	/* MINUTE */
	if(*p) p++;
	tm.tm_min=atoi(p);
	while(*p && isdigit(*p)) p++;
	/* SECONDS */
	if(*p) p++;
	tm.tm_sec=atoi(p);
	while(*p && isdigit(*p)) p++;
	/* TIME ZONE */
	while(*p && *p<=' ') p++;
	if(*p && (isdigit(*p) || *p=='-')) { /* HHMM or -HHMM format */
		sprintf(str,"%.*s",*p=='-'? 3:2,p);
		when.zone=atoi(str)*60;
		p+=(*p=='-') ? 3:2;
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

	when.time=mktime(&tm);

	return(when);
}

/****************************************************************************/
/* Converts when_t.zone into ASCII format                                   */
/****************************************************************************/
char* DLLCALL zonestr(short zone)
{
	char*		plus;
    static char str[32];

	switch((ushort)zone) {
		case 0:     return("UTC");
		case AST:   return("AST");
		case EST:   return("EST");
		case CST:   return("CST");
		case MST:   return("MST");
		case PST:   return("PST");
		case YST:   return("YST");
		case HST:   return("HST");
		case BST:   return("BST");
		case ADT:   return("ADT");
		case EDT:   return("EDT");
		case CDT:   return("CDT");
		case MDT:   return("MDT");
		case PDT:   return("PDT");
		case YDT:   return("YDT");
		case HDT:   return("HDT");
		case BDT:   return("BDT");
		case MID:   return("MID");
		case VAN:   return("VAN");
		case EDM:   return("EDM");
		case WIN:   return("WIN");
		case BOG:   return("BOG");
		case CAR:   return("CAR");
		case RIO:   return("RIO");
		case FER:   return("FER");
		case AZO:   return("AZO");
		case LON:   return("LON");
		case BER:   return("BER");
		case ATH:   return("ATH");
		case MOS:   return("MOS");
		case DUB:   return("DUB");
		case KAB:   return("KAB");
		case KAR:   return("KAR");
		case BOM:   return("BOM");
		case KAT:   return("KAT");
		case DHA:   return("DHA");
		case BAN:   return("BAN");
		case HON:   return("HON");
		case TOK:   return("TOK");
		case SYD:   return("SYD");
		case NOU:   return("NOU");
		case WEL:   return("WEL");
		}

	if(!OTHER_ZONE(zone)) {
		if(zone&(WESTERN_ZONE|US_ZONE))	/* West of UTC? */
			zone=-(zone&0xfff);
		else
			zone&=0xfff;
	}

	if(zone>0)
		plus="+";
	else
		plus="";
	sprintf(str,"UTC%s%d:%02u", plus, zone/60, zone<0 ? (-zone)%60 : zone%60);
	return(str);
}

/****************************************************************************/
/* Convert when_t structure to RFC822 date header field (string)			*/
/****************************************************************************/
char* DLLCALL msgdate(when_t when, char* buf)
{
	struct tm tm;
	
	if(localtime_r((const time_t*)&when.time,&tm)==NULL)
		memset(&tm,0,sizeof(tm));
	sprintf(buf,"%s, %d %s %d %02d:%02d:%02d %s"
		,wday[tm.tm_wday]
		,tm.tm_mday
		,mon[tm.tm_mon]
		,1900+tm.tm_year
		,tm.tm_hour
		,tm.tm_min
		,tm.tm_sec
		,zonestr(when.zone)
		);
	return(buf);
}
