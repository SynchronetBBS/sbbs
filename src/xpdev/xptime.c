/* $Id: xptime.c,v 1.4 2018/02/14 20:44:00 deuce Exp $ */

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
#include "xpdatetime.h"

int main(int argc, char **argv)
{
	char			str[256];
	char			revision[16];
	time_t			t;
	struct tm*		tp;
	struct tm		tm;
	int				argn=1;
	xpDateTime_t	xpDateTime;

	printf("\n");
	DESCRIBE_COMPILER(str);
	sscanf("$Revision: 1.4 $", "%*s %s", revision);

	printf("Rev %s Built " __DATE__ " " __TIME__ " with %s\n\n", revision, str);

	if(argc<2)
		printf("usage: xptime [-z] <date_str || time_t>\n\n");

	if(argc>1 && stricmp(argv[1],"-z")==0)	{ /* zulu/GMT/UTC timezone */
		printf("Setting timezone to Zulu/GMT/UTC\n\n");
		putenv("TZ=UTC0");
		argn++;
	}
	tzset();

	if((t=checktime())!=0L)
		printf("!time() result diverges from standard by: %ld seconds\n\n",t);

	printf("Current timezone: %d\n", xpTimeZone_local());
	printf("\n");

	if(argc>argn && strlen(argv[argn]) > 10) {
		xpDateTime=isoDateTimeStr_parse(argv[argn]);
		t=xpDateTime_to_time(xpDateTime);
		printf("Using specified date and time:\n");
	} else if(argc>argn) {
		printf("Using specified time_t value:\n");
		t=strtoul(argv[argn],NULL,0);
		xpDateTime=time_to_xpDateTime(t,xpTimeZone_LOCAL);
	} else {
		printf("Using current time:\n");
		xpDateTime=xpDateTime_now();
		t=time(NULL);
	}
	printf("%-8s %-10ld  (0x%08lX)    ISO %s\n"
		,"time_t"
		,(long)t, (long)t
		,xpDateTime_to_isoDateTimeStr(xpDateTime
			,NULL, " ", NULL
			,/* precision: */3
			, str, sizeof(str)));
	{
		const char* fmt="%-8s %.24s    ISO %s\n";

		if((tp=localtime_r(&t, &tm))==NULL)
			printf("localtime() failure\n");
		else
			printf(fmt
				,"local"
				,asctime(tp)
				,xpDateTime_to_isoDateTimeStr(
					time_to_xpDateTime(t, xpTimeZone_LOCAL)
					,NULL, " ", NULL
					,/*precision: */0
					,str, sizeof(str))
				);
		if((tp=gmtime_r(&t, &tm))==NULL)
			printf("gmtime() failure\n");
		else
			printf(fmt
				,"GMT"
				,asctime(tp)
				,xpDateTime_to_isoDateTimeStr(
					gmtime_to_xpDateTime(t)
					,NULL, " ", NULL
					,/*precision: */0
					,str, sizeof(str))
				);
	}

	return(0);
}
