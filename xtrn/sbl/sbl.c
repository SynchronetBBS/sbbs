/* sbl.c */

/* Synchronet BBS List Door */

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

/***********
 * History *
 ***********

******************
* RELEASE: v1.00 *
******************

07/03/93 03:16am
Fixed bug with "Auto-deleting" already deleted entries. This would cause a
long list of "Auto-deletion" messages once a day.

07/03/93 03:30am
The name of the user who last updated the entry is now stored and displayed.

07/03/93 03:45am
Adding/Updating entries is now much easier and user friendly.

07/03/93 04:00am
Added support for user "verification" of BBS entries.

07/03/93 04:10am
Users may now update or remove entries using partial system names.

07/03/93 04:30am
Sysops may now un-delete purged entries with the '*' key.

******************
* RELEASE: v1.10 *
******************

10/18/93 06:04pm
Fixed bug that would cause entries to be purged almost immediately.

10/18/93 07:01pm
(F)ind text now searches user names who last updated and verified.

10/19/93 01:34am
Added option for users to change the format of BBS listings.

******************
* RELEASE: v1.20 *
******************

10/20/93 04:44pm
Fixed cosmetic problem with opening menu (for users, not sysop).

******************
* RELEASE: v1.21 *
******************

11/29/93 09:40pm
More cosmetic changes.	Added "Saving..." message.

******************
* RELEASE: v1.22 *
******************

02/02/94
Added warning for pending auto-deletion of BBS entries.

02/02/94
Added option for turning screen pause off/on.

02/03/94
Added option in SBL.CFG for sysop/co-sysop notification of changes made to
BBS list by users.

02/03/94
Converted all file operations from handles to streams for buffered i/o (speed).

02/09/94
Added options for generating a sort index and displaying sorted list based on
various criteria.

02/09/94
Added nodesync() calls to display any messages waiting for this user/node.

02/10/94
Added search for duplicate names when adding new BBS entries.

02/10/94
Notification notice of actual auto-deletion sent to author of BBS entry upon
auto-deletion.

******************
* RELEASE: v1.30 *
******************

03/14/94
Added /M switch to force daily maintenance.

03/22/94
Fixed occasional double pause after listings.

03/22/94
Added total entries found to find text listings.

03/22/94
If a user verifies an entry, the user who created the entry is notified.

03/29/94
Sysop can define/change the "owner" of an entry when adding or updating.

04/18/94
Fixed bug in the sort-by-string functions that caused lock-ups when sorting
more than 312 entries.

04/18/94
Lowered memory requirements for all sort functions.

******************
* RELEASE: v1.31 *
******************

08/23/94
BBS entries now know whether they were created by a user or by SMB2SBL (via
message base).

08/23/94
Fixed problem with hitting Ctrl-C locally during regular (not extended)
listing. Returning to main menu would not clear screen or have correct colors.
'aborted' variable is now reset in main() loop.

******************
* RELEASE: v1.32 *
******************

08/30/94
Fixed stack overflow that would cause periodic lock-ups on some systems.

******************
* RELEASE: v1.33 *
******************

09/08/94
When deleting an entry, the name of the BBS deleted wasn't being printed.

02/01/95
Import utility made mistake of ignoring READ messages to SBL. This has been
fixed.

12/16/99
Fixed Y2K window bug in dstrtounix().

12/16/99
Fixed bug that caused SMB imported entries to never be autodeleted.

06/01/00
Added support for XSDK 3.0

06/01/00
Added support for MSVC 5+

06/01/00
Changed cosmetic appearance of main menu a bit

06/14/99
Added support for C++Builder 5
*/

#include <stddef.h>		/* offsetof() macro */
#include <xsdk.h>
#include "sbldefs.h"

unsigned _stklen=16000; 		  /* Set stack size in code, not header */

#define Y2K_2DIGIT_WINDOW	70
#define VERIFICATION_MOD	FALSE

typedef struct {

	char	str[32];
	short	offset;

	} sortstr_t;

typedef struct {

	long	i;
	short	offset;

	} sortint_t;

char *nulstr="";
char *wday[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
char *mon[]={"Jan","Feb","Mar","Apr","May","Jun"
            ,"Jul","Aug","Sep","Oct","Nov","Dec"};
char tmp[256];
char list_fmt[128];
uint del_days,add_ml,update_ml,remove_ml,verify_ml,sbl_pause=1,notify_user;
time_t now;

/****************************************************************************/
/* Generates a 24 character ASCII string that represents the time_t pointer */
/* Used as a replacement for ctime()										*/
/****************************************************************************/
char *timestr(time_t *intime)
{
	static char str[256];
    char mer[3],hour;
    struct tm *gm;

gm=localtime(intime);
if(gm->tm_hour>=12) {
    if(gm->tm_hour==12)
        hour=12;
    else
        hour=gm->tm_hour-12;
    strcpy(mer,"pm"); }
else {
    if(gm->tm_hour==0)
        hour=12;
    else
        hour=gm->tm_hour;
    strcpy(mer,"am"); }
sprintf(str,"%s %s %02d %4d %02d:%02d %s"
    ,wday[gm->tm_wday],mon[gm->tm_mon],gm->tm_mday,1900+gm->tm_year
    ,hour,gm->tm_min,mer);
return(str);
}


/****************************************************************************/
/* Converts a date string in format MM/DD/YY into unix time format			*/
/****************************************************************************/
time_t dstrtounix(char *str)
{
	struct tm tm;

	if(!strncmp(str,"00/00/00",8))
		return(0);
	memset(&tm,0,sizeof(tm));
	tm.tm_year=((str[6]&0xf)*10)+(str[7]&0xf);
	if (tm.tm_year<Y2K_2DIGIT_WINDOW)
		tm.tm_year+=100;
	tm.tm_mon=((str[0]&0xf)*10)+(str[1]&0xf);
	tm.tm_mday=((str[3]&0xf)*10)+(str[4]&0xf); 
	if (tm.tm_mon)
		tm.tm_mon--;	/* zero-based month field */
	return(mktime(&tm));
}

/****************************************************************************/
/* Converts unix time format (long - time_t) into a char str MM/DD/YY		*/
/****************************************************************************/
char * unixtodstr(time_t t, char *str)
{
	struct tm* tm;

	if(t==0)
		strcpy(str,"00/00/00");
	else {
		tm=gmtime(&t);
		if(tm==NULL) {
			strcpy(str,"00/00/00");
			return(str);
		}
		if(tm->tm_mon>11) {	  /* DOS leap year bug */
			tm->tm_mon=0;
			tm->tm_year++; }
		if(tm->tm_mday>31)
			tm->tm_mday=1;
		sprintf(str,"%02u/%02u/%02u",tm->tm_mon+1,tm->tm_mday
			,tm->tm_year%100); }
	return(str);
}

void dots(int show)
{
	static int i;

if(!show) {  /* reset */
	i=0;
	return; }
if(++i>5) {
	bputs("\b\b\b\b\b     \b\b\b\b\b");
	i=0;
	return; }
outchar('.');
}


/* Displays short information about BBS. Returns 0 if aborted. */

char short_bbs_info(bbs_t bbs)
{
	char	str[128];
	int		i,j;

    for(i=0;i<bbs.total_numbers && i<MAX_NUMBERS;i++) {
        for(j=0;list_fmt[j];j++) {
            if(j) bputs(" ");
            switch(toupper(list_fmt[j])) {
                case 'N':
                    bprintf("\1h\1m%-25.25s",i ? nulstr : bbs.name);
                    break;
                case 'S':
                    bprintf("\1h\1c%-15.15s",i ? nulstr : bbs.software);
                    break;
                case 'P':
                    if(bbs.number[i].modem.min_rate==0xffff) {
                        if(bbs.number[i].telnet.port!=0 
                            && bbs.number[i].telnet.port!=IPPORT_TELNET)
                            sprintf(str,"%s:%hu"
                                ,bbs.number[i].telnet.addr
                                ,bbs.number[i].telnet.port);
                        else
                            strcpy(str,bbs.number[i].telnet.addr);
                        bprintf("\1h\1b%-28.28s",str);
                    } else
                        bprintf("\1n\1g%12.12s",bbs.number[i].modem.number);
                    break;
                case 'B':
                    if(bbs.number[i].modem.min_rate!=0xffff)
                        bprintf("\1h\1g%5u",bbs.number[i].modem.max_rate);
                    break;
                case 'M':
                    if(bbs.number[i].modem.min_rate!=0xffff)
                        bprintf("\1h\1b%-15.15s",bbs.number[i].modem.desc);
                    break;
                case 'Y':
                    bprintf("\1h\1y%-25.25s",i ? nulstr : bbs.sysop[0]);
                    break;
                case 'T':
                    if(i) bputs("   ");
                    else  bprintf("\1y\1h%3u",bbs.nodes);
                    break;
                case 'U':
                    if(i) bputs("     ");
                    else  bprintf("\1n\1r%5u",bbs.users);
                    break;
                case 'H':
                    if(i) bprintf("%10.10s",nulstr);
                    else  bprintf("\1h\1r%10u",bbs.megs);
                    break;
                case 'L':
                    bprintf("\1n\1c%-20.20s",bbs.number[i].modem.location);
                    break;
                case 'F':
                    bprintf("\1n\1b%s",i ? nulstr : unixtodstr(bbs.birth,tmp));
                    break;
                case 'V':
                    bprintf("\1n\1m%s",i ? nulstr : unixtodstr(bbs.verified,tmp));
                    break;
                case 'D':
                    bprintf("\1n%s",i ? nulstr : unixtodstr(bbs.updated,tmp));
                    break;
                case 'C':
                    bprintf("\1n\1y%s",i ? nulstr : unixtodstr(bbs.created,tmp));
                    break;
                default:
                    bprintf("%c",list_fmt[j]);
                    break;
                    } }
        bputs("\r\n"); }
#if 0
    if(kbhit())
        return(0);
#endif
    return(1);
}

char long_bbs_info(bbs_t bbs)
{
	int i;

cls();
bprintf("\1n\1gBBS Name: \1h%s \1n\1gsince \1h%s\r\n"
	,bbs.name,unixtodstr(bbs.birth,tmp));
bprintf("\1n\1gOperator: ");
for(i=0;i<bbs.total_sysops && i<MAX_SYSOPS;i++) {
	if(i) {
		if(bbs.total_sysops>2)
			bputs(", ");
		else
			outchar(SP);
		if(!(i%4))
            bputs("\r\n          ");
		if(i+1==bbs.total_sysops)
			bputs("and "); }
	bprintf("\1h%s\1n\1g",bbs.sysop[i]); 
	if(!i && bbs.sysop_email[0])
		bprintf(" <\1h%s\1n\1g>", bbs.sysop_email);
}
CRLF;
bprintf("\1n\1gSoftware: \1h%-15.15s \1n\1gNodes: \1h%-5u \1n\1g"
	"Users: \1h%-5u \1n\1gDoors: \1h%u\r\n"
	,bbs.software,bbs.nodes,bbs.users,bbs.xtrns);
bprintf("\1n\1gDownload: \1h%lu \1n\1gfiles in \1h%u \1n\1gdirectories of \1h"
	"%luMB \1n\1gtotal space\r\n"
	,bbs.files,bbs.dirs,bbs.megs);
bprintf("Messages: \1h%lu \1n\1gmessages in \1h%u \1n\1gsub-boards\r\n"
	,bbs.msgs,bbs.subs);
bprintf("Networks: ");
for(i=0;i<bbs.total_networks && i<MAX_NETS;i++) {
	if(i) {
		if(bbs.total_networks>2)
			bputs(", ");
		else
			outchar(SP);
		if(!(i%3))
            bputs("\r\n          ");
		if(i+1==bbs.total_networks)
            bputs("and "); }
	bprintf("\1h%s [%s]\1n\1g",bbs.network[i],bbs.address[i]); }
CRLF;
bprintf("Terminal: ");
for(i=0;i<bbs.total_terminals && i<MAX_TERMS;i++) {
	if(i) {
		if(bbs.total_terminals>2)
			bputs(", ");
		else
			outchar(SP);
		if(i+1==bbs.total_terminals)
            bputs("and "); }
	bprintf("\1h%s\1n\1g",bbs.terminal[i]); }
CRLF;
if(bbs.web_url[0])
	bprintf("Web-site: http://\1h%s\r\n",bbs.web_url);
CRLF;
for(i=0;i<bbs.total_numbers && i<MAX_NUMBERS;i++) {
	bprintf("\1h\1b%-30.30s "
		,i && !strcmp(bbs.number[i].modem.location,bbs.number[i-1].modem.location)
				? nulstr : bbs.number[i].modem.location);
	if(bbs.number[i].modem.min_rate==0xffff) {
		if(bbs.number[i].telnet.port!=0
			&& bbs.number[i].telnet.port!=IPPORT_TELNET)
			bprintf("\1n\1mtelnet://\1h%s:%hu\r\n"
				,bbs.number[i].telnet.addr
				,bbs.number[i].telnet.port);
		else
			bprintf("\1n\1mtelnet://\1h%s\r\n"
				,bbs.number[i].telnet.addr);
	} else
		bprintf("\1n\1g%12.12s \1h%5u \1b%-15.15s \1n\1cMinimum: \1h%u\r\n"
			,bbs.number[i].modem.number
			,bbs.number[i].modem.max_rate
			,bbs.number[i].modem.desc
			,bbs.number[i].modem.min_rate);
}
bputs("\r\n\1w\1h");
for(i=0;i<DESC_LINES;i++) {
	if(!bbs.desc[i][0])
		break;
	bprintf("%15s%s\r\n",nulstr,bbs.desc[i]); }

CRLF;
if(bbs.misc&FROM_SMB)
	bputs("\1r\1hImported from message base.\r\n");
bprintf("\1n\1cEntry created on \1h%s\1n\1c by \1h%s\r\n"
	,timestr(&bbs.created),bbs.user);
if(bbs.updated && bbs.userupdated[0])
	bprintf("\1n\1c Last updated on \1h%s\1n\1c by \1h%s\r\n"
		,timestr(&bbs.updated),bbs.userupdated);
if(bbs.verified && bbs.userverified[0])
	bprintf("\1n\1cLast verified on \1h%s\1n\1c by \1h%s"
#if VERIFICATION_MOD
	" \1y(%d/%d)"
#endif
	"\r\n"
		,timestr(&bbs.verified),bbs.userverified
		,bbs.verification_count,bbs.verification_attempts);
CRLF;
if(aborted) {
	aborted=0;
	return(0); }
if(!sbl_pause) {
#if 0
	if(kbhit())
		return(0);
#endif
	return(1); }
nodesync();
#if VERIFICATION_MOD
	bputs("More? Y/N/V ");
switch(getkeys("YNV\r",0)) {
	case '\r':
	case 'Y':
		return(1);
	case 'V':
		return(2);
	default:
		return(0);
}
#else
	return(yesno("More"));
#endif
}

/* Gets/updates BBS info from user. Returns 0 if aborted. */

char get_bbs_info(bbs_t *bbs)
{
	char str[128];
	int i;

aborted=0;
if(!(bbs->user[0]))
	strcpy(bbs->user,user_name);
if(SYSOP) {
	bputs("\1y\1hUser Name (Creator/Owner of Entry): ");
	if(!getstr(bbs->user,25,K_EDIT|K_LINE|K_AUTODEL))
		return(0); }
bputs("\1y\1hSystem Name: ");
if(getstr(bbs->name,25,K_EDIT|K_LINE|K_AUTODEL)<2)
	return(0);
if(!bbs->software[0])
	strcpy(bbs->software,"Synchronet");
bprintf("\1y\1hSoftware: \1w",bbs->software);
if(!getstr(bbs->software,15,K_AUTODEL|K_EDIT))
	return(0);

for(i=0;i<MAX_SYSOPS && !aborted;i++) {
	bprintf("\1y\1hName of System Operator #%d [\1wNone\1y]: ",i+1);
	if(!getstr(bbs->sysop[i],25,K_EDIT|K_LINE|K_AUTODEL))
		break; 
	if(!i) {
		bprintf("\1y\1hInternet E-mail Address [\1wNone\1y]:\1w ");
		getstr(bbs->sysop_email,40,K_EDIT|K_AUTODEL);
	}
}
bbs->total_sysops=i;
if(aborted)
	return(0);

unixtodstr(bbs->birth,str);
bprintf("\1y\1hFirst Day Online (MM/DD/YY): \1w");
if(getstr(str,8,K_UPPER|K_EDIT|K_AUTODEL))
	bbs->birth=dstrtounix(str);
if(aborted) return(0);

for(i=0;i<MAX_NETS && !aborted;i++) {
	bprintf("\1y\1hName of Message Network #%d [\1wNone\1y]: ",i+1);
	if(!getstr(bbs->network[i],15,K_EDIT|K_AUTODEL|K_LINE))
		break;
	bprintf("\1y\1hNetwork Message \1wAddress\1y #%d [\1wNone\1y]: \1w",i+1);
	getstr(bbs->address[i],25,K_EDIT|K_AUTODEL); }
bbs->total_networks=i;
if(aborted) return(0);

for(i=0;i<MAX_TERMS && !aborted;i++) {
	bprintf("\1y\1hSupported Terminal Type #%d (i.e. TTY, ANSI, RIP) "
		"[\1wNone\1y]: ",i+1);
	if(!getstr(bbs->terminal[i],15,K_EDIT|K_AUTODEL|K_LINE))
		break; }
bbs->total_terminals=i;
if(aborted) return(0);

bprintf("\1y\1hWeb-site [\1wNone\1y] http://\1w");
getstr(bbs->web_url,60,K_EDIT|K_AUTODEL);

if(!bbs->nodes)
	bbs->nodes=1;
bprintf("\1y\1hNodes (maximum number of simultaneous REMOTE users): \1w");
sprintf(str,"%u",bbs->nodes);
if(getstr(str,5,K_NUMBER|K_EDIT|K_AUTODEL))
	bbs->nodes=atoi(str);
if(!bbs->nodes)
	bbs->nodes=1;
if(aborted) return(0);

for(i=0;i<MAX_NUMBERS;i++) {
#if 0
	if(!i && !bbs->number[i].modem.number[0])
		sprintf(bbs->number[i].modem.number,"%.8s",user_phone);
#endif
	bprintf("\1y\1hPhone Number or \1mTelnet address\1y #%d [\1wNone\1y]: ",i+1);
	if(!getstr(bbs->number[i].telnet.addr,28,K_EDIT|K_LINE))
		break;
	if(!bbs->number[i].modem.location[0]) {
		if(!i)
			strcpy(bbs->number[i].modem.location,user_location);
		else
			strcpy(bbs->number[i].modem.location,bbs->number[i-1].modem.location); }
	if(aborted) return(0);
	if(isalpha(bbs->number[i].telnet.addr[0]) 
		|| yesno("Is the above a Telnet address")) {
		bbs->number[i].modem.min_rate=0xffff;
		if(bbs->number[i].telnet.port==0)
			bbs->number[i].telnet.port=IPPORT_TELNET;
		bprintf("\1y\1hTCP Port: \1w");
		sprintf(str,"%u",bbs->number[i].telnet.port);
		if(getstr(str,5,K_NUMBER|K_EDIT|K_AUTODEL))
			bbs->number[i].telnet.port=atoi(str);
		if(aborted) return(0);

	} else {
		if(bbs->number[i].modem.min_rate==0xffff)
			bbs->number[i].modem.min_rate=300;
		bbs->number[i].modem.number[13]=0;
		if(!bbs->number[i].modem.min_rate) {
			if(i)
				bbs->number[i].modem.min_rate=bbs->number[i-1].modem.min_rate;
			else
				bbs->number[i].modem.min_rate=300; }
		bprintf("\1y\1hMinimum Connect Rate: \1w");
		sprintf(str,"%u",bbs->number[i].modem.min_rate);
		if(getstr(str,5,K_NUMBER|K_EDIT|K_AUTODEL))
			bbs->number[i].modem.min_rate=atoi(str);
		if(aborted) return(0);

		if(!bbs->number[i].modem.max_rate) {
			if(i)
				bbs->number[i].modem.max_rate=bbs->number[i-1].modem.max_rate;
			else
				bbs->number[i].modem.max_rate=2400; }
		if(bbs->number[i].modem.max_rate<bbs->number[i].modem.min_rate)
			bbs->number[i].modem.max_rate=bbs->number[i].modem.min_rate;
		bprintf("\1y\1hMaximum Connect Rate (i.e. 2400, 9600, 14400, etc): \1w");
		sprintf(str,"%u",bbs->number[i].modem.max_rate);
		if(getstr(str,5,K_NUMBER|K_EDIT|K_AUTODEL))
			bbs->number[i].modem.max_rate=atoi(str);
		if(aborted) return(0);
		bprintf("\1y\1hModem Description (i.e. Hayes, HST, V.32, etc): \1w");
		getstr(bbs->number[i].modem.desc,15,K_EDIT|K_AUTODEL);  
	}

	bprintf("\1y\1hLocation (City, State): \1w");
	if(!getstr(bbs->number[i].modem.location,30,K_EDIT|K_AUTODEL))
		break;

}
if(!i)
	return(0);
bbs->total_numbers=i;
if(aborted)
	return(0);

if(!bbs->users)
	bbs->users=100;
bprintf("\1y\1hTotal Number of Users: \1w");
sprintf(str,"%u",bbs->users);
if(getstr(str,5,K_NUMBER|K_EDIT|K_AUTODEL))
	bbs->users=atoi(str);
if(aborted) return(0);

if(!bbs->subs)
	bbs->subs=10;
bprintf("\1y\1hTotal Number of Sub-boards (Message Areas): \1w");
sprintf(str,"%u",bbs->subs);
if(getstr(str,5,K_NUMBER|K_EDIT|K_AUTODEL))
	bbs->subs=atoi(str);
if(aborted) return(0);

if(!bbs->msgs)
	bbs->msgs=500;
bprintf("\1y\1hTotal Number of Public Messages: \1w");
sprintf(str,"%lu",bbs->msgs);
if(getstr(str,10,K_NUMBER|K_EDIT|K_AUTODEL))
	bbs->msgs=atol(str);
if(aborted) return(0);

if(!bbs->dirs)
	bbs->dirs=5;
bprintf("\1y\1hTotal Number of Directories (File Areas): \1w");
sprintf(str,"%u",bbs->dirs);
if(getstr(str,5,K_NUMBER|K_EDIT|K_AUTODEL))
	bbs->dirs=atoi(str);
if(aborted) return(0);

if(!bbs->files)
	bbs->files=250;
bprintf("\1y\1hTotal Number of Downloadable Files: \1w");
sprintf(str,"%lu",bbs->files);
if(getstr(str,10,K_NUMBER|K_EDIT|K_AUTODEL))
	bbs->files=atol(str);
if(aborted) return(0);

if(!bbs->xtrns)
    bbs->xtrns=5;
bprintf("\1y\1hTotal Number of External Programs (Doors): \1w");
sprintf(str,"%u",bbs->xtrns);
if(getstr(str,5,K_NUMBER|K_EDIT|K_AUTODEL))
    bbs->xtrns=atoi(str);
if(aborted) return(0);

if(!bbs->megs)
	bbs->megs=40;
bprintf("\1y\1hTotal Storage Space (in Megabytes): \1w");
sprintf(str,"%lu",bbs->megs);
if(getstr(str,10,K_NUMBER|K_EDIT|K_AUTODEL))
	bbs->megs=atol(str);
if(aborted) return(0);

for(i=0;i<DESC_LINES;i++) {
	bprintf("\1y\1hBBS Description (%d of %d): ",i+1,DESC_LINES);
	if(!getstr(bbs->desc[i],50,K_EDIT|K_AUTODEL|K_LINE))
		break; }

return(1);
}

char partname(char *inname, char *inpart)
{
	char name[128],part[128],str[256];

strcpy(name,inname);
strupr(name);
strcpy(part,inpart);
strupr(part);
if(inname[0] && (strstr(name,part) || strstr(part,name))) {
	sprintf(str,"\r\nDo you mean %s",inname);
	if(yesno(str))
		return(1); }
return(0);
}

int sortint_cmp(sortint_t *int1, sortint_t *int2)
{

if(int1->i>int2->i)
	return(-1);
if(int1->i<int2->i)
	return(1);
return(0);
}

int sortstr_cmp(sortstr_t *str1, sortstr_t *str2)
{
return(stricmp(str1->str,str2->str));
}

int main(int argc, char **argv)
{
	char	str[512],name[128],*p,ch;
	char*	database="sbl.dab";
	short	i,j,file,done,sort_by_str;
	int 	maint=0;
	long	l,found;
	bbs_t	bbs,tmpbbs;
	FILE	*stream;
	sortstr_t *sortstr;
	sortint_t *sortint;

	for(i=1;i<argc;i++)
		if(
#ifndef __unix__
			argv[i][0]=='/' || 
#endif
			argv[i][0]=='-') {
			switch(toupper(argv[i][1])) {
				case 'M':
					maint=1;
					xsdk_mode&=~XSDK_MODE_NOCONSOLE;
					break; 
				case 'W':
					xsdk_mode&=~XSDK_MODE_NOCONSOLE;
					break; 
			}
		} else
			database=argv[i];

	p=getenv("SBBSNODE");
	if(p)
		strcpy(node_dir,p);
	else {
		printf("\nSBBSNODE environment variable must be set\n");
		exit(0); }

	if(node_dir[strlen(node_dir)-1]!='\\' && node_dir[strlen(node_dir)-1]!='/')
		strcat(node_dir,"/");

	strcpy(str,"sbl.cfg");
	if((file=sopen(str,O_RDONLY,SH_DENYNO))==-1) {
		printf("error opening %s\r\n",str);
		exit(1); }
	if((stream=fdopen(file,"rb"))==NULL) {
		printf("fdopen error with %s\r\n",str);
		exit(1); }
	fgets(str,81,stream);
	del_days=atoi(str);
	fgets(str,81,stream);
	add_ml=atoi(str);
	fgets(str,81,stream);
	update_ml=atoi(str);
	fgets(str,81,stream);
	remove_ml=atoi(str);
	fgets(str,81,stream);
	verify_ml=atoi(str);
	fgets(str,81,stream);
	notify_user=atoi(str);
	fclose(stream);

	initdata();
	if(maint) 
		client_socket=INVALID_SOCKET;

	mnehigh=HIGH|LIGHTGRAY;
	mnelow=HIGH|YELLOW;
	if(maint)
		user_misc=(ANSI|COLOR);

	if((file=sopen(database,O_RDWR|O_BINARY|O_CREAT,SH_DENYNO))==-1) {
		bprintf("\r\n\7Error opening/creating %s\r\n",database);
		exit(1); }
	if((stream=fdopen(file,"w+b"))==NULL) {
		bprintf("\r\n\7Error converting %s file handle to stream\r\n",database);
		exit(1); }
	setvbuf(stream,0L,_IOFBF,2048);

	if(del_days) {
		now=time(NULL);
		strcpy(str,"sblpurge.dab");
		if((file=nopen(str,O_RDWR|O_CREAT))==-1) {
			printf("Error creating %s\r\n",str);
			exit(1); }
		l=0;
		read(file,&l,4);
		if(now-l>(24L*60L*60L) || maint) {	 /* more than a day since update */
			bputs("\r\n\1n\1hRunning daily maintenance for Synchronet BBS List...");
			lseek(file,0L,SEEK_SET);
			write(file,&now,4);
			close(file);
			fseek(stream,0L,SEEK_SET);
			while(!feof(stream)) {
				if(!fread(&bbs,sizeof(bbs_t),1,stream))
					break;
				if(bbs.name[0]) {
					if((now-bbs.updated)/(24L*60L*60L)>(time_t)del_days
						&& (now-bbs.created)/(24L*60L*60L)>(time_t)del_days
						&& (now-bbs.verified)/(24L*60L*60L)>(time_t)del_days) {
						lncntr=0;
						bprintf("\r\n\1n\1hAuto-deleting \1m%s\r\n",bbs.name);
						if(!(bbs.misc&FROM_SMB)) {
							sprintf(str,"\1n\1hSBL: \1mYour BBS entry for \1y%s\1m\r\n"
								"     was auto-deleted from the \1cSynchronet BBS "
								"List\r\n",bbs.name);
							i=usernumber(bbs.user);
							if(i) putsmsg(i,str);
						}
						bbs.name[0]=0;
						fseek(stream,-(long)(sizeof(bbs_t)),SEEK_CUR);
						fwrite(&bbs,sizeof(bbs_t),1,stream); 
						fflush(stream); 
					} else if (!(bbs.misc&FROM_SMB)) { /* Warn user */
						l=bbs.created;
						if(l<bbs.updated)
							l=bbs.updated;
						if(l<bbs.verified)
							l=bbs.verified;
						if((now-l)/(24L*60L*60L)>=(time_t)(del_days/2)) {
							bprintf("\r\n\1n\1hWarning \1y%s\r\n",bbs.user);
							lncntr=0;
							sprintf(str,"\1n\1hSBL: \1mPlease verify your BBS "
								"entry for \1y%s\1m\r\n     "
								"in the \1cSynchronet BBS List "
								"\1mor it will be deleted in \1i\1r%u "
								"\1n\1h\1mdays.\r\n"
								,bbs.name
								,del_days-((now-l)/(24L*60L*60L)));
							i=usernumber(bbs.user);
							if(i) putsmsg(i,str); } } } } }
		else
			close(file); }

	if(maint)
		return(0);

	strcpy(list_fmt,DEF_LIST_FMT);
	while(1) {
		aborted=0;
		attr(LIGHTGRAY);
		cls();
		COMPILER_DESC(str);
		bprintf("\1n\1m\1hSynchronet \1wBBS List \1mv3.11 for %s\r\n (XSDK v%s %s %s) "
			"\r\n\r\n"
			,PLATFORM_DESC
			,xsdk_ver
			,str
			,__DATE__
			);

	#define SBL_INDENT "    "
		sprintf(str,SBL_INDENT"~List all systems (condensed)\r\n"
					SBL_INDENT"~Change list format\r\n"
					SBL_INDENT"~Extended information on all systems\r\n"
					SBL_INDENT"~Turn screen pause %s\r\n"
					SBL_INDENT"~Find text in BBS entries\r\n"
					SBL_INDENT"~Generate sorted list\r\n"
					SBL_INDENT"~Display sorted list\r\n"
					SBL_INDENT"~New entry scan\r\n"
					SBL_INDENT"~Add a BBS entry\r\n"
					SBL_INDENT"~Update a BBS entry\r\n"
					SBL_INDENT"~Verify a BBS entry\r\n"
					SBL_INDENT"~Remove a BBS entry\r\n"
					SBL_INDENT"~Quit back to BBS\r\n"
					,sbl_pause ? "OFF" : "ON");
		mnemonics(str);
		if(SYSOP)
			mnemonics(SBL_INDENT"~* Undelete entries\r\n");

		bputs("\r\n");

		l=filelength(fileno(stream));
		if(l>0)
			bprintf(SBL_INDENT"\1n\1cThere are \1h%lu\1n\1c entries in the online BBS list "
				"database.\r\n",l/(long)sizeof(bbs_t));

		if(del_days) {
			bprintf(SBL_INDENT"\1n\1cEntries are auto-deleted \1h%u\1n\1c days after "
				"last update or verification.\r\n",del_days);
			bputs(SBL_INDENT"Users are encouraged to \1hV\1n\1cerify (vouch for) any listed "
				"systems they call.\r\n"); }

		nodesync(); 			/* Display any waiting messages */

		bputs("\r\n"SBL_INDENT"\1y\1hWhich: \1w");
		switch(getkeys("CLGDEFSNAURTQV!*",0)) {
			case '!':
				bprintf("\r\nsizeof(bbs_t)=%u\r\n",sizeof(bbs_t));
				bprintf("\r\noffsetof(verification_count=%x)\r\n",offsetof(bbs_t,verification_count));
				pause();
				break;
			case '*':
				cls();
				if(!SYSOP)
					break;
				fseek(stream,0L,SEEK_SET);
				while(!feof(stream) && !aborted) {
					if(!fread(&bbs,sizeof(bbs_t),1,stream))
						break;
					if(!bbs.name[0] && bbs.total_numbers>0) {
						bbs.name[0]='?';
						bbs.verified=time(NULL);
						sprintf(bbs.userverified,"%.25s",user_name);
						if(yesno(bbs.name)) {
							bprintf("\1n\1gFirst char: \1h");
							bbs.name[0]=getkey(0);
							bprintf("%s\r\n",bbs.name);
							fseek(stream,-(long)sizeof(bbs_t),SEEK_CUR);
							fwrite(&bbs,sizeof(bbs_t),1,stream); 
							fflush(stream); 
						} 
					} 
				}
				break;
			case 'L':
				cls();
				fseek(stream,0L,SEEK_SET);
				i=0;
				while(!feof(stream) && !aborted) {
					if(!fread(&bbs,sizeof(bbs_t),1,stream))
						break;
					if(!bbs.name[0] || bbs.total_numbers<1)
						continue;
					i++;
					if(!short_bbs_info(bbs))
						break;
					if(!sbl_pause)
						lncntr=0; }
				bprintf("\r\n\1n\1h%u systems listed.\r\n",i);
#if 0
				if(kbhit())
					getch();
#endif                    
				if(lncntr)
					pause();
				break;
			case 'C':
				cls();
				bputs("\1n\1c\1hList Format Specifier Definitions:\1n\r\n\r\n");
				bputs("\1h\1w(\1mN\1w) \1n\1mName of System\r\n");
				bputs("\1h\1w(\1mS\1w) \1n\1mSoftware Used\r\n");
				bputs("\1h\1w(\1mP\1w) \1n\1mPhone Number\r\n");
				bputs("\1h\1w(\1mB\1w) \1n\1mMaximum Connect Rate (in bps)\r\n");
				bputs("\1h\1w(\1mM\1w) \1n\1mModem Type\r\n");
				bputs("\1h\1w(\1mY\1w) \1n\1mSysop's Name\r\n");
				bputs("\1h\1w(\1mT\1w) \1n\1mTotal Number of Nodes\r\n");
				bputs("\1h\1w(\1mU\1w) \1n\1mTotal Number of Users\r\n");
				bputs("\1h\1w(\1mH\1w) \1n\1mTotal Storage Capacity (in megabytes)\r\n");
				bputs("\1h\1w(\1mL\1w) \1n\1mLocation (City, State)\r\n");
				bputs("\1h\1w(\1mF\1w) \1n\1mDate System was First Online\r\n");
				bputs("\1h\1w(\1mC\1w) \1n\1mDate Entry was Created\r\n");
				bputs("\1h\1w(\1mV\1w) \1n\1mDate Entry was Last Verified\r\n");
				bputs("\1h\1w(\1mD\1w) \1n\1mDate Entry was Last Updated\r\n");
				bprintf("\r\n\1n\1gDefault Format: \1h%s",DEF_LIST_FMT);
				bprintf("\r\n\1n\1gCurrent Format: \1h%s\r\n",list_fmt);
				bprintf("\r\n\1y\1hNew Format: ");
				if(getstr(tmp,10,K_UPPER|K_LINE)) {
					if(!strchr(tmp,'P') || !strchr(tmp,'N')) {
						bputs("\r\n\1h\1mP\1n\1mhone and \1hN\1n\1mame specifiers "
							"must be present in format.\r\n\r\n");
						pause(); }
					else
						strcpy(list_fmt,tmp); }
				break;
			case 'E':
				fseek(stream,0L,SEEK_SET);
				while(!feof(stream) && !aborted) {
					if(!fread(&bbs,sizeof(bbs_t),1,stream))
						break;
					if(!bbs.name[0] || bbs.total_numbers<1)
						continue;
					i=long_bbs_info(bbs);
					if(bbs.name[0] && !i)
						break;
#if VERIFICATION_MOD
					if(i==2) {
						bbs.verification_count++;
						fseek(stream,-(int)sizeof(bbs_t),SEEK_CUR);
						fwrite(&bbs,sizeof(bbs_t),1,stream);
						fflush(stream);
						fseek(stream,-(int)sizeof(bbs_t),SEEK_CUR);
					}
#endif
					if(!sbl_pause)
						lncntr=0; }
				break;
			case 'F':   /* Find text */
				cls();
				bputs("\1y\1hText to search for: ");
				if(!getstr(name,25,K_UPPER|K_LINE))
					break;
				ch=yesno("\r\nDisplay extended information");

				found=0;
				bputs("\1n\1h\r\nSearching...\r\n\r\n");
				fseek(stream,0L,SEEK_SET);
				while(!feof(stream) && !aborted) {
					if(!sbl_pause)
						lncntr=0;
					if(!fread(&bbs,sizeof(bbs_t),1,stream))
						break;
					if(!bbs.name[0] || bbs.total_numbers<1)
						continue;
					tmpbbs=bbs;
					strupr(tmpbbs.name);
					strupr(tmpbbs.user);
					strupr(tmpbbs.software);
					strupr(tmpbbs.userverified);
					strupr(tmpbbs.userupdated);
					strupr(tmpbbs.web_url);
					for(i=0;i<DESC_LINES;i++) {
						strupr(tmpbbs.desc[i]);
						if(strstr(tmpbbs.desc[i],name))
							break;
					}
					if(i<DESC_LINES
						|| strstr(tmpbbs.name,name)
						|| strstr(tmpbbs.user,name)
						|| strstr(tmpbbs.software,name)
						|| strstr(tmpbbs.userverified,name)
						|| strstr(tmpbbs.userupdated,name)
						|| strstr(tmpbbs.web_url,name)
						) {
						found++;
						if(ch && !long_bbs_info(bbs))
							break;
						if(!ch && !short_bbs_info(bbs))
							break;
						continue; }

					for(i=0;i<tmpbbs.total_sysops;i++) {
						strupr(tmpbbs.sysop[i]);
						if(strstr(tmpbbs.sysop[i],name))
							break; }
					if(i<tmpbbs.total_sysops) {
						found++;
						if(ch && !long_bbs_info(bbs))
							break;
						if(!ch && !short_bbs_info(bbs))
							break;
						continue; }

					for(i=0;i<tmpbbs.total_networks;i++) {
						strupr(tmpbbs.network[i]);
						strupr(tmpbbs.address[i]);
						if(strstr(tmpbbs.network[i],name)
							|| strstr(tmpbbs.address[i],name))
							break; }
					if(i<tmpbbs.total_networks) {
						found++;
						if(ch && !long_bbs_info(bbs))
							break;
						if(!ch && !short_bbs_info(bbs))
							break;
						continue; }

					for(i=0;i<tmpbbs.total_terminals;i++) {
						strupr(tmpbbs.terminal[i]);
						if(strstr(tmpbbs.terminal[i],name))
							break; }
					if(i<tmpbbs.total_terminals) {
						found++;
						if(ch && !long_bbs_info(bbs))
							break;
						if(!ch && !short_bbs_info(bbs))
							break;
						continue; }

					for(i=0;i<tmpbbs.total_numbers;i++) {
						strupr(tmpbbs.number[i].modem.number);
						strupr(tmpbbs.number[i].modem.desc);
						strupr(tmpbbs.number[i].modem.location);
						if(strstr(tmpbbs.number[i].modem.number,name)
							|| strstr(tmpbbs.number[i].modem.desc,name)
							|| strstr(tmpbbs.number[i].modem.location,name))
							break; }
					if(i<tmpbbs.total_numbers) {
						found++;
						if(ch && !long_bbs_info(bbs))
							break;
						if(!ch && !short_bbs_info(bbs))
							break;
						continue; } }
				if(!ch || !found) {
					CRLF;
#if 0
					if(kbhit())
						getch();
#endif                        
					if(found)
						bprintf("\1n\1h%u systems listed.\r\n",found);
					pause(); }
				break;
			case 'G':   /* Generated sorted list */
				cls();
				if(!filelength(fileno(stream))) {
					bprintf("No BBS list exists.\r\n");
					pause();
					break; }
				bputs("\1n\1c\1hSort Options:\1n\r\n\r\n");
				bputs("\1h\1w(\1mN\1w) \1n\1mName of System\r\n");
				bputs("\1h\1w(\1mS\1w) \1n\1mSoftware Used\r\n");
				bputs("\1h\1w(\1mP\1w) \1n\1mPhone Number\r\n");
				bputs("\1h\1w(\1mB\1w) \1n\1mMaximum Connect Rate (in bps)\r\n");
				bputs("\1h\1w(\1mM\1w) \1n\1mModem Type\r\n");
				bputs("\1h\1w(\1mY\1w) \1n\1mSysop's Name\r\n");
				bputs("\1h\1w(\1mT\1w) \1n\1mTotal Number of Nodes\r\n");
				bputs("\1h\1w(\1mU\1w) \1n\1mTotal Number of Users\r\n");
				bputs("\1h\1w(\1mH\1w) \1n\1mTotal Storage Capacity (in megabytes)\r\n");
				bputs("\1h\1w(\1mL\1w) \1n\1mLocation (City, State)\r\n");
				bputs("\1h\1w(\1mF\1w) \1n\1mDate System was First Online\r\n");
				bputs("\1h\1w(\1mC\1w) \1n\1mDate Entry was Created\r\n");
				bputs("\1h\1w(\1mV\1w) \1n\1mDate Entry was Last Verified\r\n");
				bputs("\1h\1w(\1mD\1w) \1n\1mDate Entry was Last Updated\r\n");
				bprintf("\r\n\1y\1hSort by (\1wQ\1y=Quit): \1w");
				ch=getkeys("NSPBMYTUHLFCVDQ",0);
				if(!ch || ch=='Q')
					break;
				cls();
				bputs("\1n\1hSorting...     \1m");
				fseek(stream,0L,SEEK_SET);
				i=j=done=0;
				sort_by_str=0;
				sortstr=NULL;
				sortint=NULL;
				while(!feof(stream) && !done) {
					if(!fread(&bbs,sizeof(bbs_t),1,stream))
						break;
					j++;
					bprintf("\b\b\b\b%4u",j);
					if(!bbs.name[0])	/* don't sort deleted entries */
						continue;
					if(bbs.total_numbers<1)	/* don't sort corrupted entries */
						continue;
					i++;
					switch(ch) {
						case 'N':
							sprintf(str,"%.30s",bbs.name);
							sort_by_str=1;
							break;
						case 'S':
							sprintf(str,"%.30s",bbs.software);
							sort_by_str=1;
							break;
						case 'P':
							sprintf(str,"%.30s",bbs.number[0].modem.number);
							sort_by_str=1;
							break;
						case 'M':
							sprintf(str,"%.30s",bbs.number[0].modem.desc);
							sort_by_str=1;
							break;
						case 'Y':
							sprintf(str,"%.30s",bbs.sysop[0]);
							sort_by_str=1;
							break;
						case 'L':
							sprintf(str,"%.30s",bbs.number[0].modem.location);
							sort_by_str=1;
							break;
						case 'B':
							l=bbs.number[0].modem.max_rate;
							break;
						case 'T':
							l=bbs.nodes;
							break;
						case 'U':
							l=bbs.users;
							break;
						case 'H':
							l=bbs.megs;
							break;
						case 'F':
							l=bbs.birth;
							break;
						case 'C':
							l=bbs.created;
							break;
						case 'V':
							l=bbs.verified;
							break;
						case 'D':
							l=bbs.updated;
							break; }
					if(sort_by_str) {
						if((sortstr=(sortstr_t *)REALLOC(sortstr
							,sizeof(sortstr_t)*i))==NULL) {
							bprintf("\r\n\7Memory allocation error\r\n");
							LFREE(sortstr);
							done=1;
							continue; }
						strcpy(sortstr[i-1].str,str);
						sortstr[i-1].offset=j-1; }
					else {
						if((sortint=(sortint_t *)REALLOC(sortint
							,sizeof(sortint_t)*i))==NULL) {
							bprintf("\r\n\7Memory allocation error\r\n");
							LFREE(sortint);
							done=1;
							continue; }
						sortint[i-1].i=l;
						sortint[i-1].offset=j-1; } }

				if(done) {
					pause();
					break; }

				if(sort_by_str)
					qsort((void *)sortstr,i,sizeof(sortstr[0])
						,(int(*)(const void *, const void *))sortstr_cmp);
				else
					qsort((void *)sortint,i,sizeof(sortint[0])
						,(int(*)(const void *, const void *))sortint_cmp);

				bprintf("\r\n\r\n\1h\1gCreating index...");
				sprintf(str,"sort_%03d.ndx",node_num);
				if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
					bprintf("\r\n\7Error creating %s\r\n",str);
					if(sort_by_str)
						LFREE(sortstr);
					else
						LFREE(sortint);
					pause();
					break; }
				for(j=0;j<i;j++)
					if(sort_by_str)
						write(file,&sortstr[j].offset,2);
					else
						write(file,&sortint[j].offset,2);
				close(file);
				if(sort_by_str)
					LFREE(sortstr);
				else
					LFREE(sortint);
				bputs("\r\n\r\n\1n\1hDone.\r\n");
				pause();
				break;
			case 'D':
				cls();
				sprintf(str,"sort_%03d.ndx",node_num);
				if((file=nopen(str,O_RDONLY))==-1) {
					bputs("\1n\1r\1hSorted list not generated.\r\n");
					pause(); }
				ch=yesno("Display extended information");
				cls();
				while(/* !eof(file) && not on unix */ !aborted) {
					if(read(file,&i,2)!=2)
						break;
					fseek(stream,(long)i*sizeof(bbs_t),SEEK_SET);
					if(!sbl_pause)
						lncntr=0;
					if(!fread(&bbs,sizeof(bbs_t),1,stream))
						break;
					if(!bbs.name[0] || bbs.total_numbers<1)
						continue;
					if(ch && !long_bbs_info(bbs))
						break;
					if(!ch && !short_bbs_info(bbs))
						break; }
				close(file);
#if 0
				if(kbhit())
					getch();
#endif                    
				if(lncntr)
					pause();
				break;
			case 'N':   /* New (updated) entry scan */
				cls();
				bputs("\1y\1hLast update (MM/DD/YY): ");
				if(!getstr(str,8,K_UPPER|K_LINE))
					break;
				l=dstrtounix(str);
				ch=yesno("\r\nDisplay extended information");
				found=0;
				bputs("\1n\1h\r\nSearching...\r\n\r\n");
				fseek(stream,0L,SEEK_SET);
				while(!feof(stream) && !aborted) {
					if(!sbl_pause)
						lncntr=0;
					if(!fread(&bbs,sizeof(bbs_t),1,stream))
						break;
					if(!bbs.name[0] || bbs.total_numbers<1)
						continue;
					if(bbs.updated>=l || bbs.created>=l) {
						if(ch && !long_bbs_info(bbs))
							break;
						if(!ch && !short_bbs_info(bbs))
							break;
						found++;
						continue; } }
				if(!ch || !found) {
					CRLF;
					pause(); }
				break;
			case 'A':
				cls();
				if((uint)user_level<add_ml) {
					bprintf("\1h\1rYou have insufficient access.\r\n\r\n");
					pause();
					break; }
				bputs("\1g\1hAdding a BBS entry:\1n\r\n\r\n");
				bputs("\1n\1gHit ENTER for unknown data items.\r\n\r\n");
				memset(&bbs,0,sizeof(bbs_t));
				if(!get_bbs_info(&bbs))
					break;
				bputs("\1n\1h\r\nSearching for duplicates...");
				fseek(stream,0L,SEEK_SET);
				i=0;
				dots(0);
				while(!feof(stream) && !i) {
					dots(1);
					if(!fread(&tmpbbs,sizeof(bbs_t),1,stream))
						break;
					if(!stricmp(tmpbbs.name,bbs.name)) i=1; }
				if(i) {
					bprintf("\7\1n\1h\1r\1i\r\n\r\n%s \1n\1h\1ralready exists!"
						"\r\n\r\n"
						,bbs.name);
					pause();
					break; }

				bputs("\1n\1h\r\nSaving...");
				fseek(stream,0L,SEEK_SET);
				dots(0);
				while(!feof(stream)) {
					dots(1);
					if(!fread(&ch,1,1,stream))
						break;
					if(!ch) {			/* first byte is null */
						fseek(stream,-1L,SEEK_CUR);
						break; }
					fseek(stream,(long)sizeof(bbs_t)-1L,SEEK_CUR); }
				bbs.created=time(NULL);
				fwrite(&bbs,sizeof(bbs_t),1,stream);
				fflush(stream);
				if(notify_user && notify_user!=user_number) {
					sprintf(str,"\1n\1hSBL: \1y%s \1madded \1c%s\1m "
						"to the BBS List\r\n",user_name,bbs.name);
					putsmsg(notify_user,str); }
				break;
			case 'R':       /* Remove an entry */
				cls();
				if((uint)user_level<remove_ml) {
					bprintf("\1h\1rYou have insufficient access.\r\n\r\n");
					pause();
					break; }
				bprintf("\1y\1hRemove which system: ");
				if(!getstr(name,25,K_LINE|K_UPPER))
					break;
				bputs("\1n\1h\r\nSearching...");
				fseek(stream,0L,SEEK_SET);
				found=0;
				dots(0);
				while(!feof(stream) && !aborted) {
					dots(1);
					if(!fread(&bbs,sizeof(bbs_t),1,stream))
						break;
					if(!stricmp(bbs.name,name) || partname(bbs.name,name)) {
						found=1;
						for(i=0;i<bbs.total_sysops && i<MAX_SYSOPS;i++)
							if(!stricmp(bbs.sysop[i],user_name))
								break;
						if(SYSOP || !stricmp(bbs.user,user_name)
							|| i<bbs.total_sysops) {
							fseek(stream,-(long)(sizeof(bbs_t)),SEEK_CUR);
							strcpy(tmp,bbs.name);
							bbs.name[0]=0;
							bbs.updated=time(NULL);
							fwrite(&bbs,sizeof(bbs_t),1,stream);
							fflush(stream);
							bprintf("\r\n\r\n\1m%s\1c deleted."
								,tmp);
							if(notify_user && notify_user!=user_number) {
								sprintf(str,"\1n\1hSBL: \1y%s \1mremoved \1c%s\1m "
									"from the BBS List\r\n",user_name,tmp);
								putsmsg(notify_user,str); } }
						else
							bprintf("\r\n\r\n\1rYou did not create \1m%s\1n."
								,bbs.name);
						break; } }
				if(!found)
					bprintf("\r\n\r\n\1m%s\1c not found.",name);
				CRLF;
				CRLF;
				pause();
				break;
			case 'T':
				sbl_pause=!sbl_pause;
				break;
			case 'V':       /* Verify an entry */
				cls();
				if((uint)user_level<verify_ml) {
					bprintf("\1h\1rYou have insufficient access.\r\n\r\n");
					pause();
					break; }
				bprintf("\1y\1hVerify which system: ");
				if(!getstr(name,25,K_LINE|K_UPPER))
					break;
				bputs("\1n\1h\r\nSearching...");
				fseek(stream,0L,SEEK_SET);
				found=0;
				dots(0);
				while(!feof(stream) && !aborted) {
					dots(1);
					if(!fread(&bbs,sizeof(bbs_t),1,stream))
						break;
					if(!stricmp(bbs.name,name) || partname(bbs.name,name)) {
						found=1;
						bbs.verified=time(NULL);
						sprintf(bbs.userverified,"%.25s",user_name);
						fseek(stream,-(long)(sizeof(bbs_t)),SEEK_CUR);
						fwrite(&bbs,sizeof(bbs_t),1,stream);
						fflush(stream);
						bprintf("\r\n\r\n\1m%s\1c verified. \1r\1h\1iThank you!"
							,bbs.name);
						sprintf(str,"\1n\1hSBL: \1y%s \1mverified \1c%s\1m "
							"in the BBS List\r\n",user_name,bbs.name);
						if(notify_user && notify_user!=user_number)
							putsmsg(notify_user,str);
						if(stricmp(bbs.user,user_name)) {
							i=usernumber(bbs.user);
							if(i && i!=(int)user_number) putsmsg(i,str); }
						break; } }
				if(!found)
					bprintf("\r\n\r\n\1m%s\1c not found.",name);
				CRLF;
				CRLF;
				pause();
				break;
			case 'U':       /* Update an entry */
				cls();
				if((uint)user_level<update_ml) {
					bprintf("\1h\1rYou have insufficient access.\r\n\r\n");
					pause();
					break; }
				bprintf("\1y\1hUpdate which system: ");
				if(!getstr(name,25,K_LINE|K_UPPER))
					break;
				bputs("\1n\1h\r\nSearching...");
				fseek(stream,0L,SEEK_SET);
				found=0;
				dots(0);
				while(!feof(stream) && !aborted) {
					dots(1);
					l=ftell(stream);
					if(!fread(&bbs,sizeof(bbs_t),1,stream))
						break;
					if(!stricmp(bbs.name,name) || partname(bbs.name,name)) {
						found=1;
						break; } }
				if(found) {
					for(i=0;i<bbs.total_sysops && i<MAX_SYSOPS;i++)
						if(!bbs.sysop[i][0] || !stricmp(bbs.sysop[i],user_name))
							break;
					if(SYSOP || !stricmp(bbs.user,user_name)
						|| i<bbs.total_sysops) {
						CRLF;
						CRLF;
						if(get_bbs_info(&bbs)) {
							bbs.misc&=~FROM_SMB;
							bbs.updated=time(NULL);
							sprintf(bbs.userupdated,"%.25s",user_name);
							fseek(stream,l,SEEK_SET);
							fwrite(&bbs,sizeof(bbs_t),1,stream);
							fflush(stream);
							bprintf("\r\n\1h\1m%s\1c updated.",bbs.name);
							if(notify_user && notify_user!=user_number) {
								sprintf(str,"\1n\1hSBL: \1y%s \1mupdated \1c%s\1m "
									"in the BBS List\r\n",user_name,bbs.name);
								putsmsg(notify_user,str); } } }
					else
						bprintf("\r\n\r\n\1h\1rYou did not create \1m%s\1n."
						   ,bbs.name); }
				else
					bprintf("\r\n\r\n\1h\1m%s\1c not found.",name);
				CRLF;
				CRLF;
				pause();
				break;
			case 'Q':
				return(0); } }
	return(-1);
}

/* End of SBL.C */
