/* UTIHIGH.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "uti.h"

smb_t smb;

/****************************************************************************/
/* Returns the total number of msgs in the sub-board and sets 'ptr' to the  */
/* number of the last message in the sub (0) if no messages.                */
/****************************************************************************/
ulong getlastmsg(uint subnum, ulong *ptr, time_t *t)
{
    char        str[256];
    int         i;
    ulong       total;
    idxrec_t    idx;

if(ptr)
    (*ptr)=0;
if(t)
    (*t)=0;

sprintf(smb.file,"%s%s",sub[subnum]->data_dir,sub[subnum]->code);
smb.retry_time=30;
if((i=smb_open(&smb))!=0) {
	errormsg(WHERE,ERR_OPEN,smb.file,i);
    return(0); }

if(!filelength(fileno(smb.sid_fp))) {			/* Empty base */
	smb_close(&smb);
    return(0); }
if((i=smb_locksmbhdr(&smb))!=0) {
	smb_close(&smb);
	errormsg(WHERE,ERR_LOCK,smb.file,i);
    return(0); }
if((i=smb_getlastidx(&smb,&idx))!=0) {
	smb_close(&smb);
	errormsg(WHERE,ERR_READ,smb.file,i);
    return(0); }
total=filelength(fileno(smb.sid_fp))/sizeof(idxrec_t);
smb_unlocksmbhdr(&smb);
smb_close(&smb);
if(ptr)
    (*ptr)=idx.number;
if(t)
    (*t)=idx.time;
return(total);
}


int main(int argc, char **argv)
{
	char str[256];
	int file,subnum,i;
	ulong ptr;

PREPSCREEN;

printf("Synchronet UTIHIGH v%s\n",VER);


if(argc<3)
	exit(1);

uti_init("UTIHIGH",argc,argv);

subnum=getsubnum(argv[1]);
if((int)subnum==-1)
	bail(7);
getlastmsg(subnum,&ptr,0);

if((file=nopen(argv[2],O_CREAT|O_TRUNC|O_WRONLY))==-1)
	bail(2);

sprintf(str,"%lu",ptr);
write(file,str,strlen(str));
close(file);
sprintf(str,"%20s Last message #%lu\r\n","",ptr);
write(logfile,str,strlen(str));
printf("\nDone.\n");
bail(0);
return(0);
}
