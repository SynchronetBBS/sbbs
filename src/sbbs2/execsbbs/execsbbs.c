/* execbbs.c */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include <dos.h>
#include <dir.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>

extern unsigned _heaplen=2048;

/* usage: execsbbs start_dir "program parms" sbbspath [r] */

int main(int argc, char *argv[])
{
	char path[129],*comspec,*p,*arg[30],c;
	int disk;

if(argc<4) {
	printf("This program is for the internal use of Synchronet.\r\n");
/***	Debug stuff
	printf("argc=%d\r\n",argc);
	for(c=0;c<argc;c++)
		printf("argv[%d]='%s'\n",c,argv[c]);
***/
	return(0); }
disk=getdisk();
getcwd(path,128);
comspec=getenv("COMSPEC");

/***					/* Removed */
strcpy(str,comspec);	/* save comspec */
strcpy(comspec,"");     /* destroy comspec */
***/

if(argv[1][1]==':')                /* drive letter specified */
	setdisk(toupper(argv[1][0])-'A');
chdir(argv[1]);

p=strchr(argv[2],' ');
if(p)
	p++;
if(spawnlpe(P_WAIT,comspec,comspec,"/c",argv[2],NULL,environ))
	printf("EXECSBBS: Error %d spawning %s\r\n",errno,argv[2]);
/***
strcpy(comspec,str);	/* restore comspec */
***/
setdisk(disk);
if(chdir(path))
	printf("\7\r\nEXECBBS: Error changing directory to %s\r\n",path);
for(c=0;c<30;c++)
	arg[c]=argv[c+3];
if(execve(arg[0],arg,environ))
	printf("EXECSBBS: Error %d executing %s\r\n",errno,arg[0]);
return(1);
}
