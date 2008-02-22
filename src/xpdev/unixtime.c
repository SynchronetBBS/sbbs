/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#ifndef __unix__
#include <dos.h>
#endif
#include <ctype.h>
#define USE_SNPRINTF	/* we don't need safe_snprintf for this project */
#include "genwrap.h"
#include "datewrap.h"

/****************************************************************************/
/* Converts a date string in format MM/DD/YY into unix time format			*/
/****************************************************************************/
time_t dstrtounix(char *str)
{
	char *p;
	struct tm t;

	memset(&t,0,sizeof(t));
	t.tm_year=((str[6]&0xf)*10)+(str[7]&0xf);
	if(t.tm_year<70)
		t.tm_year+=100;
	t.tm_mon=((str[0]&0xf)*10)+(str[1]&0xf);
	if(t.tm_mon) t.tm_mon--;
	t.tm_mday=((str[3]&0xf)*10)+(str[4]&0xf);
	p=strchr(str,' ');
	if(p) {
		t.tm_hour=atoi(++p);
		p=strchr(p,':');
		if(p) {
			t.tm_min=atoi(++p);
			p=strchr(p,':');
			if(p)
				t.tm_sec=atoi(++p); } }
	return(mktime(&t));
}

time_t checktime()
{
	struct tm tm;

	memset(&tm,0,sizeof(tm));
	tm.tm_year=94;
	tm.tm_mday=1;
	return(mktime(&tm)^0x2D24BD00L);
}

int main(int argc, char **argv)
{
	char		str[256];
	char		revision[16];
	time_t		t;
	struct tm*	tm;
	int			argn=1;

	printf("\n");
	DESCRIBE_COMPILER(str);
	sscanf("$Revision$", "%*s %s", revision);

	printf("Rev %s Built " __DATE__ " " __TIME__ " with %s\n\n", revision, str);

#if 0

	if((t=checktime())!=0L) {
		printf("Time problem (%08lX)\n",t);
		exit(1); }
#endif

	if(argc<2)
		printf("usage: unixtime [-z] <MM/DD/YY HH:MM:SS || time_t>\n\n");

	if(argc>1 && stricmp(argv[1],"-z")==0)	{ /* zulu/GMT/UTC timezone */
		printf("Setting timezone to Zulu/GMT/UTC\n\n");
		putenv("TZ=UTC0");
		argn++;
	}
	tzset();

	printf("Current timezone: %d\n", xpTimeZone_local());
	printf("\n");

	if(argc>argn && argv[argn][2]=='/') {
		sprintf(str,"%s %s",argv[argn],argc>argn+1 ? argv[argn+1] : "");
		if((t=dstrtounix(str))==-1) {
			printf("dstrtounix error\n");
			return -1;
		}
		printf("Using specified date and time: ");
	} else if(argc>argn) {
		printf("Using specified time_t value: ");
		t=strtoul(argv[argn],NULL,0);
	} else {
		printf("Using current time_t value: ");
		t=time(NULL);
	}
	printf("%ld (%08lX)\n", t, t);
	if((tm=localtime(&t))==NULL)
		printf("localtime() failure\n");
	else
		printf("%-8s %.24s\n","local", asctime(tm));
	if((tm=gmtime(&t))==NULL)
		printf("gmtime() failure\n");
	else
		printf("%-8s %.24s\n","GMT", asctime(tm));

	return(0);
}
