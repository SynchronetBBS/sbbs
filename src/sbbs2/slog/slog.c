/* SLOG.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys\stat.h>
#include <io.h>
#include <dos.h>
#include <alloc.h>
#include <time.h>
#include <errno.h>

#include "sbbsdefs.h"

/****************************************************************************/
/* Network open function. Opens all files DENYALL and retries LOOP_NOPEN    */
/* number of times if the attempted file is already open or denying access  */
/* for some other reason.	All files are opened in BINARY mode.			*/
/****************************************************************************/
int nopen(char *str, int access)
{
	char	logstr[256];
	int 	file,share,count=0;

if(access==O_RDONLY) share=O_DENYWRITE;
	else share=O_DENYALL;
while(((file=open(str,O_BINARY|share|access,S_IWRITE))==-1)
	&& errno==EACCES && count++<LOOP_NOPEN);
if(count)
	printf("NOPEN COLLISION - File: %s Count: %d"
		,str,count);
if(file==-1 && errno==EACCES)
	printf("\7\r\nNOPEN: ACCESS DENIED\r\n\7");
return(file);
}


/****************************************************************************/
/* Lists system statistics for everyday the bbs has been running.           */
/* Either for the current node (node=1) or the system (node=0)              */
/****************************************************************************/
int main(int argc, char **argv)
{
	char str[256],dir[256]={""},*p;
    uchar *buf;
	int i,file,pause=0,lncntr=0;
    time_t timestamp;
    long l;
    ulong   length,
            logons,
            timeon,
            posts,
            emails,
            fbacks,
            ulb,
            uls,
            dlb,
            dls;
	 struct date date;
	 struct time curtime;


printf("\nSynchronet System/Node Statistics Log Viewer v1.02\n\n");

for(i=1;i<argc;i++)
	if(!stricmp(argv[i],"/P"))
		pause=1;
	else
		strcpy(dir,argv[1]);
if(!dir[0]) {
	p=getenv("SBBSCTRL");
	if(p!=NULL)
		strcpy(dir,p); }

if(dir[0] && dir[strlen(dir)-1]!='\\')
	strcat(dir,"\\");

sprintf(str,"%sCSTS.DAB",dir);
if((file=nopen(str,O_RDONLY))==-1) {
	printf("Error opening %s\r\n",str);
	return(1); }
length=filelength(file);
if(length<40) {
    close(file);
	return(1); }
if((buf=(char *)MALLOC(length))==0) {
    close(file);
	printf("error allocating %lu bytes\r\n",length);
	return(1); }
read(file,buf,length);
close(file);
l=length-4;
while(l>-1L) {
    fbacks=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
        |((long)buf[l+3]<<24);
    l-=4;
    emails=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
        |((long)buf[l+3]<<24);
    l-=4;
    posts=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
        |((long)buf[l+3]<<24);
    l-=4;
    dlb=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
        |((long)buf[l+3]<<24);
    l-=4;
    dls=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
        |((long)buf[l+3]<<24);
    l-=4;
    ulb=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
        |((long)buf[l+3]<<24);
    l-=4;
    uls=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
        |((long)buf[l+3]<<24);
    l-=4;
    timeon=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
        |((long)buf[l+3]<<24);
    l-=4;
    logons=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
        |((long)buf[l+3]<<24);
    l-=4;
    timestamp=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
        |((long)buf[l+3]<<24);
    l-=4;
    unixtodos(timestamp-(24*60*60),&date,&curtime); /* 1 day less than stamp */
	printf("%2.2d/%2.2d/%2.2d T:%5lu   L:%3lu   P:%3lu   "
		"E:%3lu   F:%3lu   U:%6luk %3lu  D:%6luk %3lu\n"
		,date.da_mon,date.da_day,date.da_year%100,timeon,logons,posts,emails
		,fbacks,ulb/1024,uls,dlb/1024,dls);
	lncntr++;
	if(pause && lncntr>=20) {
		printf("[Hit a key]");
		if(getch()==3)
			break;
		printf("\r");
		lncntr=0; } }
FREE(buf);
return(0);
}
