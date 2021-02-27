/* DSTSEDIT.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include <dos.h>
#include <dir.h>
#include <stdio.h>
#include <io.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#include "..\sbbsdefs.h"

/****************************************************************************/
/* Network open function. Opens all files DENYALL and retries LOOP_NOPEN    */
/* number of times if the attempted file is already open or denying access  */
/* for some other reason.	All files are opened in BINARY mode.			*/
/****************************************************************************/
int nopen(char *str, int access)
{
	char logstr[256];
	int file,share,count=0;

if(access==O_RDONLY) share=O_DENYWRITE;
	else share=O_DENYALL;
while(((file=open(str,O_BINARY|share|access,S_IWRITE))==-1)
	&& errno==EACCES && count++<LOOP_NOPEN)
#ifndef __OS2__
    if(count>10)
        delay(50);
#else
	;
#endif
if(file==-1 && errno==EACCES)
	puts("\7\r\nNOPEN: ACCESS DENIED\r\n\7");
return(file);
}

/****************************************************************************/
/* Converts a date string in format MM/DD/YY into unix time format			*/
/****************************************************************************/
time_t dstrtounix(char *str)
{
	struct time curtime;
	struct date date;

if(!strcmp(str,"00/00/00"))
	return(0);
curtime.ti_hour=curtime.ti_min=curtime.ti_sec=0;
if(str[6]<'7')
	date.da_year=2000+((str[6]&0xf)*10)+(str[7]&0xf);
else
	date.da_year=1900+((str[6]&0xf)*10)+(str[7]&0xf);
date.da_mon=((str[0]&0xf)*10)+(str[1]&0xf);
date.da_day=((str[3]&0xf)*10)+(str[4]&0xf);
return(dostounix(&date,&curtime));
}

/****************************************************************************/
/* Converts unix time format (long - time_t) into a char str MM/DD/YY		*/
/****************************************************************************/
char *unixtodstr(time_t unix, char *str)
{
	struct time curtime;
	struct date date;

if(!unix)
	strcpy(str,"00/00/00");
else {
	unixtodos(unix,&date,&curtime);
	if((unsigned)date.da_mon>12) {	  /* DOS leap year bug */
		date.da_mon=1;
		date.da_year++; }
	if((unsigned)date.da_day>31)
		date.da_day=1;
	sprintf(str,"%02u/%02u/%02u",date.da_mon,date.da_day
		,date.da_year>=2000 ? date.da_year-2000 : date.da_year-1900); }
return(str);
}


int main(int argc, char **argv)
{
	char ch, str[512], path[256]
		,*lst="%c) %-25s: %13lu\n"
		,*nv="\nNew value: ";
	int file;
	stats_t stats;
	time_t t;

if(argc>1)
	strcpy(path,argv[1]);
else
	getcwd(path,MAXDIR);
if(path[strlen(path)-1]!='\\')
	strcat(path,"\\");

sprintf(str,"%sDSTS.DAB",path);
if((file=nopen(str,O_RDONLY))==-1) {
	printf("Can't open %s\r\n",str);
	exit(1); }
read(file,&t,4L);
if(read(file,&stats,sizeof(stats_t))!=sizeof(stats_t)) {
	close(file);
	printf("Error reading %u bytes from %s\r\n",sizeof(stats_t),str);
	exit(1); }
close(file);
while(1) {
	clrscr();
	printf("Synchronet Daily Statistics Editor v1.01\r\n\r\n");
	printf("S) %-25s: %13s\n","Date Stamp",unixtodstr(t,str));
	printf(lst,'L',"Total Logons",stats.logons);
	printf(lst,'O',"Logons Today",stats.ltoday);
	printf(lst,'T',"Total Time on",stats.timeon);
	printf(lst,'I',"Time on Today",stats.ttoday);
	printf(lst,'U',"Uploaded Files Today",stats.uls);
	printf(lst,'B',"Uploaded Bytes Today",stats.ulb);
	printf(lst,'D',"Downloaded Files Today",stats.dls);
	printf(lst,'W',"Downloaded Bytes Today",stats.dlb);
	printf(lst,'P',"Posts Today",stats.ptoday);
	printf(lst,'E',"E-Mails Today",stats.etoday);
	printf(lst,'F',"Feedback Today",stats.ftoday);
	printf("%c) %-25s: %13u\r\n",'N',"New Users Today",stats.nusers);

	printf("Q) Quit and save changes\r\n");
	printf("X) Quit and don't save changes\r\n");

	printf("\r\nWhich: ");

	ch=toupper(getch());
	printf("%c\r\n",ch);

	switch(ch) {
		case 'S':
			printf("Date stamp (MM/DD/YY): ");
			gets(str);
			if(str[0])
				t=dstrtounix(str);
			break;
		case 'L':
			printf(nv);
			gets(str);
			if(str[0])
				stats.logons=atol(str);
			break;
		case 'O':
			printf(nv);
			gets(str);
			if(str[0])
				stats.ltoday=atol(str);
			break;
		case 'T':
			printf(nv);
			gets(str);
			if(str[0])
				stats.timeon=atol(str);
			break;
		case 'I':
			printf(nv);
			gets(str);
			if(str[0])
				stats.ttoday=atol(str);
			break;
		case 'U':
			printf(nv);
			gets(str);
			if(str[0])
				stats.uls=atol(str);
			break;
		case 'B':
			printf(nv);
			gets(str);
			if(str[0])
				stats.ulb=atol(str);
			break;
		case 'D':
			printf(nv);
			gets(str);
			if(str[0])
				stats.dls=atol(str);
            break;
		case 'W':
			printf(nv);
			gets(str);
			if(str[0])
				stats.dlb=atol(str);
            break;
		case 'P':
			printf(nv);
			gets(str);
			if(str[0])
				stats.ptoday=atol(str);
            break;
		case 'E':
			printf(nv);
			gets(str);
			if(str[0])
				stats.etoday=atol(str);
            break;
		case 'F':
			printf(nv);
			gets(str);
			if(str[0])
				stats.ftoday=atol(str);
            break;
		case 'N':
			printf(nv);
			gets(str);
			if(str[0])
				stats.nusers=atoi(str);
			break;
		case 'Q':
			sprintf(str,"%sDSTS.DAB",path);
			if((file=nopen(str,O_WRONLY))==-1) {
				printf("Error opening %s\r\n",str);
				exit(1); }
			write(file,&t,4L);
			write(file,&stats,sizeof(stats_t));
			close(file);
		case 'X':
			exit(0);
		default:
			putchar(7);
			break; } }
}
