/* SMBLIB.C */

#include "smblib.h"

/****************************************************************************/
/* Open a message base of name 'smb_file'                                   */
/* If retry_time is 0, fast open method (no compatibility/validity check)	*/
/* Opens files for READing messages or updating message indices only        */
/****************************************************************************/
int smb_open(int retry_time)
{
    int file;
    char str[128];
	smbhdr_t hdr;

shd_fp=sdt_fp=sid_fp=NULL;
sprintf(str,"%s.SHD",smb_file);
if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYNO,S_IWRITE|S_IREAD))==-1
	|| (shd_fp=fdopen(file,"r+b"))==NULL) {
	if(file!=-1)
		close(file);
    return(2); }

if(retry_time && filelength(file)>=sizeof(smbhdr_t)) {
	setvbuf(shd_fp,shd_buf,_IONBF,SHD_BLOCK_LEN);
    if(smb_locksmbhdr(retry_time)) {
		smb_close();
        return(-1); }
	memset(&hdr,0,sizeof(smbhdr_t));
    fread(&hdr,sizeof(smbhdr_t),1,shd_fp);
    if(memcmp(hdr.id,"SMB\x1a",4)) {
		smb_close();
        return(-2); }
    if(hdr.version<0x110) {         /* Compatibility check */
		smb_close();
        return(-3); }
    smb_unlocksmbhdr();
	rewind(shd_fp); }

setvbuf(shd_fp,shd_buf,_IOFBF,SHD_BLOCK_LEN);

sprintf(str,"%s.SDT",smb_file);
if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYNO,S_IWRITE|S_IREAD))==-1
	|| (sdt_fp=fdopen(file,"r+b"))==NULL) {
	if(file!=-1)
		close(file);
	smb_close();
	return(1); }
setvbuf(sdt_fp,NULL,_IOFBF,2*1024);

sprintf(str,"%s.SID",smb_file);
if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYNO,S_IWRITE|S_IREAD))==-1
	|| (sid_fp=fdopen(file,"r+b"))==NULL) {
	if(file!=-1)
		close(file);
	smb_close();
	return(3); }
setvbuf(sid_fp,NULL,_IOFBF,2*1024);

return(0);
}

/****************************************************************************/
/* Closes the currently open message base									*/
/****************************************************************************/
void smb_close(void)
{
if(shd_fp!=NULL) {
	smb_unlocksmbhdr(); 			/* In case it's been locked */
	fclose(shd_fp); }
if(sid_fp!=NULL)
	fclose(sid_fp);
if(sdt_fp!=NULL)
	fclose(sdt_fp);
sid_fp=shd_fp=sdt_fp=NULL;
}

/****************************************************************************/
/* Opens the data block allocation table message base 'smb_file'            */
/* Retrys for retry_time number of seconds									*/
/* Return 0 on success, non-zero otherwise									*/
/****************************************************************************/
int smb_open_da(int retry_time)
{
    int file;
    char str[128];
	long start;

start=time(NULL);
sprintf(str,"%s.SDA",smb_file);
while(1) {
	if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYRW,S_IWRITE|S_IREAD))!=-1)
		break;
	if(errno!=EACCES)
		return(-1);
	if(time(NULL)-start>=retry_time)
		return(-2); }
if((sda_fp=fdopen(file,"r+b"))==NULL)
	return(-3);
setvbuf(sda_fp,NULL,_IOFBF,2*1024);
return(0);
}

/****************************************************************************/
/* Opens the header block allocation table for message base 'smb_file'      */
/* Retrys for retry_time number of seconds									*/
/* Return 0 on success, non-zero otherwise									*/
/****************************************************************************/
int smb_open_ha(int retry_time)
{
    int file;
    char str[128];
	long start;

start=time(NULL);
sprintf(str,"%s.SHA",smb_file);
while(1) {
	if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYRW,S_IWRITE|S_IREAD))!=-1)
		break;
	if(errno!=EACCES)
		return(-1);
	if(time(NULL)-start>=retry_time)
		return(-2); }
if((sha_fp=fdopen(file,"r+b"))==NULL)
	return(-3);
setvbuf(sha_fp,NULL,_IOFBF,2*1024);
return(0);
}

/****************************************************************************/
/* If the parameter 'push' is non-zero, this function stores the currently  */
/* open message base to the "virtual" smb stack. Up to SMB_STACK_LEN        */
/* message bases may be stored (defined in SMBDEFS.H).						*/
/* The parameter 'op' is the operation to perform on the stack. Either      */
/* SMB_STACK_PUSH, SMB_STACK_POP, or SMB_STACK_XCHNG						*/
/* If the operation is SMB_STACK_POP, this function restores a message base */
/* previously saved with a SMB_STACK_PUSH call to this same function.		*/
/* If the operation is SMB_STACK_XCHNG, then the current message base is	*/
/* exchanged with the message base on the top of the stack (most recently	*/
/* pushed.																	*/
/* If the current message base is not open, the SMB_STACK_PUSH and			*/
/* SMB_STACK_XCHNG operations do nothing									*/
/* Returns 0 on success, non-zero if stack full.                            */
/* If operation is SMB_STACK_POP or SMB_STACK_XCHNG, it always returns 0.	*/
/****************************************************************************/
int smb_stack(int op)
{
	static char stack_file[SMB_STACK_LEN][128];
	static FILE *stack_sdt[SMB_STACK_LEN],
				*stack_shd[SMB_STACK_LEN],
				*stack_sid[SMB_STACK_LEN],
				*stack_sda[SMB_STACK_LEN],
				*stack_sha[SMB_STACK_LEN];
	static int	stack_idx;
	char		tmp_file[128];
	FILE		*tmp_sdt,
				*tmp_shd,
				*tmp_sid,
				*tmp_sda,
				*tmp_sha;

if(op==SMB_STACK_PUSH) {
	if(stack_idx>=SMB_STACK_LEN)
		return(1);
	if(shd_fp==NULL || sdt_fp==NULL || sid_fp==NULL) /* Msg base not open */
		return(0);
	memcpy(stack_file[stack_idx],smb_file,128);
	stack_sdt[stack_idx]=sdt_fp;
	stack_shd[stack_idx]=shd_fp;
	stack_sid[stack_idx]=sid_fp;
	stack_sda[stack_idx]=sda_fp;
	stack_sha[stack_idx]=sha_fp;
	stack_idx++;
	return(0); }
/* pop or xchng */
if(!stack_idx)	/* Nothing on the stack, so do nothing */
	return(0);
if(op==SMB_STACK_XCHNG) {
	if(!shd_fp)
		return(0);
	memcpy(tmp_file,smb_file,128);
	tmp_sdt=sdt_fp;
	tmp_shd=shd_fp;
	tmp_sid=sid_fp;
	tmp_sda=sda_fp;
	tmp_sha=sha_fp; }

stack_idx--;
memcpy(smb_file,stack_file[stack_idx],128);
sdt_fp=stack_sdt[stack_idx];
shd_fp=stack_shd[stack_idx];
sid_fp=stack_sid[stack_idx];
sda_fp=stack_sda[stack_idx];
sha_fp=stack_sha[stack_idx];
if(op==SMB_STACK_XCHNG) {
	stack_idx++;
	memcpy(stack_file[stack_idx-1],tmp_file,128);
	stack_sdt[stack_idx-1]=tmp_sdt;
	stack_shd[stack_idx-1]=tmp_shd;
	stack_sid[stack_idx-1]=tmp_sid;
	stack_sda[stack_idx-1]=tmp_sda;
	stack_sha[stack_idx-1]=tmp_sha; }
return(0);
}

/****************************************************************************/
/* Truncates header file													*/
/* Retrys for retry_time number of seconds									*/
/* Return 0 on success, non-zero otherwise									*/
/****************************************************************************/
int smb_trunchdr(int retry_time)
{
	long start;

start=time(NULL);
rewind(shd_fp);
while(1) {
	if(!chsize(fileno(shd_fp),0L))
		break;
	if(errno!=EACCES)
		return(-1);
	if(time(NULL)-start>=retry_time)		/* Time-out */
		return(-2); }
return(0);
}

/*********************************/
/* Message Base Header Functions */
/*********************************/

/****************************************************************************/
/* Attempts for retry_time number of seconds to lock the message base hdr	*/
/****************************************************************************/
int smb_locksmbhdr(int retry_time)
{
	ulong start;

start=time(NULL);
while(1) {
	if(!lock(fileno(shd_fp),0L,sizeof(smbhdr_t)+sizeof(smbstatus_t)))
		return(0);
	if(time(NULL)-start>=retry_time)
		break;							/* Incase we've already locked it */
	unlock(fileno(shd_fp),0L,sizeof(smbhdr_t)+sizeof(smbstatus_t)); }
return(-1);
}

/****************************************************************************/
/* Read the SMB header from the header file and place into "status"         */
/****************************************************************************/
int smb_getstatus(smbstatus_t *status)
{
    char    str[128];
	int 	i;

setvbuf(shd_fp,shd_buf,_IONBF,SHD_BLOCK_LEN);
clearerr(shd_fp);
fseek(shd_fp,sizeof(smbhdr_t),SEEK_SET);
i=fread(status,1,sizeof(smbstatus_t),shd_fp);
setvbuf(shd_fp,shd_buf,_IOFBF,SHD_BLOCK_LEN);
if(i==sizeof(smbstatus_t))
	return(0);
return(1);
}

/****************************************************************************/
/* Writes message base header												*/
/****************************************************************************/
int smb_putstatus(smbstatus_t status)
{
	int i;

clearerr(shd_fp);
fseek(shd_fp,sizeof(smbhdr_t),SEEK_SET);
i=fwrite(&status,1,sizeof(smbstatus_t),shd_fp);
fflush(shd_fp);
if(i==sizeof(smbstatus_t))
	return(0);
return(1);
}

/****************************************************************************/
/* Unlocks previously locks message base header 							*/
/****************************************************************************/
int smb_unlocksmbhdr()
{
return(unlock(fileno(shd_fp),0L,sizeof(smbhdr_t)+sizeof(smbstatus_t)));
}

/********************************/
/* Individual Message Functions */
/********************************/

/****************************************************************************/
/* Attempts for retry_time number of seconds to lock the header for 'msg'   */
/****************************************************************************/
int smb_lockmsghdr(smbmsg_t msg, int retry_time)
{
    ulong start;

start=time(NULL);
while(1) {
	if(!lock(fileno(shd_fp),msg.idx.offset,sizeof(msghdr_t)))
        return(0);
    if(time(NULL)-start>=retry_time)
		break;
	unlock(fileno(shd_fp),msg.idx.offset,sizeof(msghdr_t)); }
return(-1);
}

/****************************************************************************/
/* Fills msg->idx with message index based on msg->hdr.number				*/
/* OR if msg->hdr.number is 0, based on msg->offset (record offset).		*/
/* if msg.hdr.number does not equal 0, then msg->offset is filled too.		*/
/* Either msg->hdr.number or msg->offset must be initialized before 		*/
/* calling this function													*/
/* Returns 1 if message number wasn't found, 0 if it was                    */
/****************************************************************************/
int smb_getmsgidx(smbmsg_t *msg)
{
	idxrec_t idx;
	ulong	 l,length,total,bot,top;

clearerr(sid_fp);
if(!msg->hdr.number) {
	fseek(sid_fp,msg->offset*sizeof(idxrec_t),SEEK_SET);
	if(!fread(&msg->idx,sizeof(idxrec_t),1,sid_fp))
		return(1);
	return(0); }

length=filelength(fileno(sid_fp));
if(!length)
	return(1);
total=length/sizeof(idxrec_t);
if(!total)
	return(1);

bot=0;
top=total;
l=total/2; /* Start at middle index */
while(1) {
	fseek(sid_fp,l*sizeof(idxrec_t),SEEK_SET);
	if(!fread(&idx,sizeof(idxrec_t),1,sid_fp))
		return(1);
	if(bot==top-1 && idx.number!=msg->hdr.number)
        return(1);
	if(idx.number>msg->hdr.number) {
		top=l;
		l=bot+((top-bot)/2);
		continue; }
	if(idx.number<msg->hdr.number) {
		bot=l;
		l=top-((top-bot)/2);
		continue; }
	break; }
msg->idx=idx;
msg->offset=l;
return(0);
}

/****************************************************************************/
/* Reads the last index record in the open message base 					*/
/****************************************************************************/
int smb_getlastidx(idxrec_t *idx)
{
	long length;

clearerr(sid_fp);
length=filelength(fileno(sid_fp));
if(length<sizeof(idxrec_t))
	return(-1);
fseek(sid_fp,length-sizeof(idxrec_t),SEEK_SET);
if(!fread(idx,sizeof(idxrec_t),1,sid_fp))
	return(-2);
return(0);
}

/****************************************************************************/
/* Figures out the total length of the header record for 'msg'              */
/* Returns length															*/
/****************************************************************************/
uint smb_getmsghdrlen(smbmsg_t msg)
{
	int i;

/* fixed portion */
msg.hdr.length=sizeof(msghdr_t);
/* data fields */
msg.hdr.length+=msg.hdr.total_dfields*sizeof(dfield_t);
/* header fields */
for(i=0;i<msg.total_hfields;i++) {
	msg.hdr.length+=sizeof(hfield_t);
	msg.hdr.length+=msg.hfield[i].length; }
return(msg.hdr.length);
}

/****************************************************************************/
/* Figures out the total length of the data buffer for 'msg'                */
/* Returns length															*/
/****************************************************************************/
ulong smb_getmsgdatlen(smbmsg_t msg)
{
	int i;
	ulong length=0L;

for(i=0;i<msg.hdr.total_dfields;i++)
	length+=msg.dfield[i].length;
return(length);
}

/****************************************************************************/
/* Read header information into 'msg' structure                             */
/* msg->idx.offset must be set before calling this function 				*/
/* Must call smb_freemsgmem() to free memory allocated for var len strs 	*/
/* Returns 0 on success, non-zero if error									*/
/****************************************************************************/
int smb_getmsghdr(smbmsg_t *msg)
{
	ushort	i;
	ulong	l,offset;
	idxrec_t idx;

rewind(shd_fp);
fseek(shd_fp,msg->idx.offset,SEEK_SET);
idx=msg->idx;
offset=msg->offset;
memset(msg,0,sizeof(smbmsg_t));
msg->idx=idx;
msg->offset=offset;
if(!fread(&msg->hdr,sizeof(msghdr_t),1,shd_fp))
	return(-1);
if(memcmp(msg->hdr.id,"SHD\x1a",4))
	return(-2);
if(msg->hdr.version<0x110)
	return(-9);
l=sizeof(msghdr_t);
if(msg->hdr.total_dfields && (msg->dfield
	=(dfield_t *)MALLOC(sizeof(dfield_t)*msg->hdr.total_dfields))==NULL) {
	smb_freemsgmem(*msg);
	return(-3); }
i=0;
while(i<msg->hdr.total_dfields && l<msg->hdr.length) {
	if(!fread(&msg->dfield[i],sizeof(dfield_t),1,shd_fp)) {
		smb_freemsgmem(*msg);
		return(-4); }
	i++;
	l+=sizeof(dfield_t); }
if(i<msg->hdr.total_dfields) {
	smb_freemsgmem(*msg);
	return(-8); }

while(l<msg->hdr.length) {
	i=msg->total_hfields;
	if((msg->hfield_dat=(void **)REALLOC(msg->hfield_dat,sizeof(void *)*(i+1)))
		==NULL) {
		smb_freemsgmem(*msg);
		return(-3); }
	if((msg->hfield=(hfield_t *)REALLOC(msg->hfield
		,sizeof(hfield_t)*(i+1)))==NULL) {
		smb_freemsgmem(*msg);
		return(-3); }
	msg->total_hfields++;
	if(!fread(&msg->hfield[i],sizeof(hfield_t),1,shd_fp)) {
		smb_freemsgmem(*msg);
		return(-5); }
	l+=sizeof(hfield_t);
	if((msg->hfield_dat[i]=(char *)MALLOC(msg->hfield[i].length+1))
		==NULL) {			/* Allocate 1 extra for NULL terminator */
		smb_freemsgmem(*msg);  /* or 0 length field */
		return(-3); }
	memset(msg->hfield_dat[i],0,msg->hfield[i].length+1);  /* init to NULL */
	if(msg->hfield[i].length
		&& !fread(msg->hfield_dat[i],msg->hfield[i].length,1,shd_fp)) {
		smb_freemsgmem(*msg);
		return(-6); }

	switch(msg->hfield[i].type) {	/* convenience variables */
		case SENDER:
			if(!msg->from) {
				msg->from=msg->hfield_dat[i];
				break; }
		case FORWARDED: 	/* fall through */
			msg->forwarded=1;
			break;
		case SENDERAGENT:
			if(!msg->forwarded)
				msg->from_agent=*(ushort *)msg->hfield_dat[i];
            break;
		case SENDEREXT:
			if(!msg->forwarded)
				msg->from_ext=msg->hfield_dat[i];
			break;
		case SENDERNETTYPE:
			if(!msg->forwarded)
				msg->from_net.type=*(ushort *)msg->hfield_dat[i];
            break;
		case SENDERNETADDR:
			if(!msg->forwarded)
				msg->from_net.addr=msg->hfield_dat[i];
            break;
		case REPLYTO:
			msg->replyto=msg->hfield_dat[i];
            break;
		case REPLYTOEXT:
			msg->replyto_ext=msg->hfield_dat[i];
			break;
		case REPLYTOAGENT:
			msg->replyto_agent=*(ushort *)msg->hfield_dat[i];
            break;
		case REPLYTONETTYPE:
			msg->replyto_net.type=*(ushort *)msg->hfield_dat[i];
            break;
		case REPLYTONETADDR:
			msg->replyto_net.addr=msg->hfield_dat[i];
            break;
		case RECIPIENT:
			msg->to=msg->hfield_dat[i];
            break;
		case RECIPIENTEXT:
			msg->to_ext=msg->hfield_dat[i];
			break;
		case RECIPIENTAGENT:
			msg->to_agent=*(ushort *)msg->hfield_dat[i];
            break;
		case RECIPIENTNETTYPE:
			msg->to_net.type=*(ushort *)msg->hfield_dat[i];
            break;
		case RECIPIENTNETADDR:
			msg->to_net.addr=msg->hfield_dat[i];
            break;
		case SUBJECT:
			msg->subj=msg->hfield_dat[i];
			break; }
	l+=msg->hfield[i].length; }

if(!msg->from || !msg->to || !msg->subj) {
	smb_freemsgmem(*msg);
	return(-7); }
return(0);
}

/****************************************************************************/
/* Frees memory allocated for 'msg'                                         */
/****************************************************************************/
void smb_freemsgmem(smbmsg_t msg)
{
	ushort	i;

if(msg.dfield)
	FREE(msg.dfield);
for(i=0;i<msg.total_hfields;i++)
	if(msg.hfield_dat[i])
		FREE(msg.hfield_dat[i]);
if(msg.hfield)
	FREE(msg.hfield);
if(msg.hfield_dat)
	FREE(msg.hfield_dat);
}

/****************************************************************************/
/* Unlocks header for 'msg'                                                 */
/****************************************************************************/
int smb_unlockmsghdr(smbmsg_t msg)
{
return(unlock(fileno(shd_fp),msg.idx.offset,sizeof(msghdr_t)));
}


/****************************************************************************/
/* Adds a header field to the 'msg' structure (in memory only)              */
/****************************************************************************/
int smb_hfield(smbmsg_t *msg, ushort type, ushort length, void *data)
{
	int i;

i=msg->total_hfields;
if((msg->hfield=(hfield_t *)REALLOC(msg->hfield,sizeof(hfield_t)*(i+1)))
	==NULL)
	return(1);
if((msg->hfield_dat=(void **)REALLOC(msg->hfield_dat,sizeof(void *)*(i+1)))
	==NULL)
	return(2);
msg->total_hfields++;
msg->hfield[i].type=type;
msg->hfield[i].length=length;
if(length) {
	if((msg->hfield_dat[i]=(void *)MALLOC(length))==NULL)
		return(4);
	memcpy(msg->hfield_dat[i],data,length); }
else
	msg->hfield_dat[i]=NULL;
return(0);
}

/****************************************************************************/
/* Adds a data field to the 'msg' structure (in memory only)                */
/* Automatically figures out the offset into the data buffer from existing	*/
/* dfield lengths															*/
/****************************************************************************/
int smb_dfield(smbmsg_t *msg, ushort type, ulong length)
{
	int i,j;

i=msg->hdr.total_dfields;
if((msg->dfield=(dfield_t *)REALLOC(msg->dfield,sizeof(dfield_t)*(i+1)))
	==NULL)
	return(1);
msg->hdr.total_dfields++;
msg->dfield[i].type=type;
msg->dfield[i].length=length;
for(j=msg->dfield[i].offset=0;j<i;j++)
	msg->dfield[i].offset+=msg->dfield[j].length;
return(0);
}

/****************************************************************************/
/* Checks CRC history file for duplicate crc. If found, returns 1.			*/
/* If no dupe, adds to CRC history and returns 0, or negative if error. 	*/
/****************************************************************************/
int smb_addcrc(ulong max_crcs, ulong crc, int retry_time)
{
	char	str[128];
	int 	file;
	long	length;
	ulong	l,*buf;
	time_t	start;

if(!max_crcs)
	return(0);
start=time(NULL);
sprintf(str,"%s.SCH",smb_file);
while(1) {
	if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYRW,S_IWRITE|S_IREAD))!=-1)
		break;
	if(errno!=EACCES)
		return(-1);
	if(time(NULL)-start>=retry_time)
        return(-2); }
length=filelength(file);
if(length<0L) {
	close(file);
	return(-4); }
if((buf=(ulong *)MALLOC(max_crcs*4))==NULL) {
	close(file);
	return(-3); }
if(length>=max_crcs*4) {			/* Reached or exceeds max crcs */
	read(file,buf,max_crcs*4);
	for(l=0;l<max_crcs;l++)
		if(crc==buf[l])
			break;
	if(l<max_crcs) {				/* Dupe CRC found */
		close(file);
		FREE(buf);
		return(1); }
	chsize(file,0L);				/* truncate it */
	lseek(file,0L,SEEK_SET);
	write(file,buf+4,(max_crcs-1)*4); }

else if(length/4) { 						/* Less than max crcs */
	read(file,buf,length);
	for(l=0;l<length/4;l++)
		if(crc==buf[l])
			break;
	if(l<length/4) {					/* Dupe CRC found */
		close(file);
		FREE(buf);
		return(1); } }

lseek(file,0L,SEEK_END);
write(file,&crc,4); 			   /* Write to the end */
FREE(buf);
close(file);
return(0);
}


/****************************************************************************/
/* Creates a new message header record in the header file.					*/
/* If storage is SMB_SELFPACK, self-packing conservative allocation is used */
/* If storage is SMB_FASTALLOC, fast allocation is used 					*/
/* If storage is SMB_HYPERALLOC, no allocation tables are used (fastest)	*/
/****************************************************************************/
int smb_addmsghdr(smbmsg_t *msg, smbstatus_t *status, int storage
	,int retry_time)
{
	int i;
	long l;

if(smb_locksmbhdr(retry_time))
    return(1);
if(smb_getstatus(status))
    return(2);

if(storage!=SMB_HYPERALLOC && (i=smb_open_ha(retry_time))!=0)
    return(i);

msg->hdr.length=smb_getmsghdrlen(*msg);
if(storage==SMB_HYPERALLOC)
	l=smb_hallochdr(status->header_offset);
else if(storage==SMB_FASTALLOC)
    l=smb_fallochdr(msg->hdr.length);
else
    l=smb_allochdr(msg->hdr.length);
if(l==-1L) {
	smb_unlocksmbhdr();
	fclose(sha_fp);
	return(-1); }

status->last_msg++;
msg->idx.number=msg->hdr.number=status->last_msg;
msg->idx.offset=status->header_offset+l;
msg->idx.time=msg->hdr.when_imported.time;
msg->idx.attr=msg->hdr.attr;
msg->offset=status->total_msgs;
status->total_msgs++;
smb_putstatus(*status);

if(storage!=SMB_HYPERALLOC)
	fclose(sha_fp);
i=smb_putmsg(*msg);
smb_unlocksmbhdr();
return(i);
}

/****************************************************************************/
/* Writes both header and index information for msg 'msg'                   */
/****************************************************************************/
int smb_putmsg(smbmsg_t msg)
{
	int i;

i=smb_putmsghdr(msg);
if(i)
	return(i);
return(smb_putmsgidx(msg));
}

/****************************************************************************/
/* Writes index information for 'msg'                                       */
/* msg.idx																	*/
/* and msg.offset must be set prior to calling to this function             */
/* Returns 0 if everything ok                                               */
/****************************************************************************/
int smb_putmsgidx(smbmsg_t msg)
{

clearerr(sid_fp);
fseek(sid_fp,msg.offset*sizeof(idxrec_t),SEEK_SET);
if(!fwrite(&msg.idx,sizeof(idxrec_t),1,sid_fp))
	return(1);
fflush(sid_fp);
return(0);
}

/****************************************************************************/
/* Writes header information for 'msg'                                      */
/* msg.hdr.length                                                           */
/* msg.idx.offset                                                           */
/* and msg.offset must be set prior to calling to this function             */
/* Returns 0 if everything ok                                               */
/****************************************************************************/
int smb_putmsghdr(smbmsg_t msg)
{
	ushort	i;
	ulong	l;

clearerr(shd_fp);
if(fseek(shd_fp,msg.idx.offset,SEEK_SET))
	return(-1);

/************************************************/
/* Write the fixed portion of the header record */
/************************************************/
if(!fwrite(&msg.hdr,sizeof(msghdr_t),1,shd_fp))
	return(-2);

/************************************************/
/* Write the data fields (each is fixed length) */
/************************************************/
for(i=0;i<msg.hdr.total_dfields;i++)
	if(!fwrite(&msg.dfield[i],sizeof(dfield_t),1,shd_fp))
		return(-3);

/*******************************************/
/* Write the variable length header fields */
/*******************************************/
for(i=0;i<msg.total_hfields;i++) {
	if(!fwrite(&msg.hfield[i],sizeof(hfield_t),1,shd_fp))
		return(-4);
	if(msg.hfield[i].length 					/* more then 0 bytes long */
		&& !fwrite(msg.hfield_dat[i],msg.hfield[i].length,1,shd_fp))
		return(-5); }

l=smb_getmsghdrlen(msg);
while(l%SHD_BLOCK_LEN) {
	if(fputc(0,shd_fp)==EOF)
		return(-6); 			   /* pad block with NULL */
	l++; }
fflush(shd_fp);
return(0);
}

/****************************************************************************/
/* Creates a sub-board's initial header file                                */
/* Truncates and deletes other associated SMB files 						*/
/****************************************************************************/
int smb_create(ulong max_crcs, ulong max_msgs, ushort max_age, ushort attr
	,int retry_time)
{
    char        str[128];
	smbhdr_t	hdr;
	smbstatus_t status;

if(filelength(fileno(shd_fp))>=sizeof(smbhdr_t)+sizeof(smbstatus_t)
	&& smb_locksmbhdr(retry_time))	/* header exists, so lock it */
	return(1);
memset(&hdr,0,sizeof(smbhdr_t));
memset(&status,0,sizeof(smbstatus_t));
memcpy(hdr.id,"SMB\x1a",4);     /* <S> <M> <B> <^Z> */
hdr.version=SMB_VERSION;
hdr.length=sizeof(smbhdr_t)+sizeof(smbstatus_t);
status.last_msg=status.total_msgs=0;
status.header_offset=sizeof(smbhdr_t)+sizeof(smbstatus_t);
status.max_crcs=max_crcs;
status.max_msgs=max_msgs;
status.max_age=max_age;
status.attr=attr;
rewind(shd_fp);
fwrite(&hdr,1,sizeof(smbhdr_t),shd_fp);
fwrite(&status,1,sizeof(smbstatus_t),shd_fp);
rewind(shd_fp);
chsize(fileno(shd_fp),sizeof(smbhdr_t)+sizeof(smbstatus_t));
fflush(shd_fp);

rewind(sdt_fp);
chsize(fileno(sdt_fp),0L);
rewind(sid_fp);
chsize(fileno(sid_fp),0L);

sprintf(str,"%s.SDA",smb_file);
remove(str);						/* if it exists, delete it */
sprintf(str,"%s.SHA",smb_file);
remove(str);                        /* if it exists, delete it */
sprintf(str,"%s.SCH",smb_file);
remove(str);
smb_unlocksmbhdr();
return(0);
}

/****************************************************************************/
/* Returns number of data blocks required to store "length" amount of data  */
/****************************************************************************/
ulong smb_datblocks(ulong length)
{
	ulong blocks;

blocks=length/SDT_BLOCK_LEN;
if(length%SDT_BLOCK_LEN)
	blocks++;
return(blocks);
}

/****************************************************************************/
/* Returns number of header blocks required to store "length" size header   */
/****************************************************************************/
ulong smb_hdrblocks(ulong length)
{
	ulong blocks;

blocks=length/SHD_BLOCK_LEN;
if(length%SHD_BLOCK_LEN)
	blocks++;
return(blocks);
}

/****************************************************************************/
/* Finds unused space in data file based on block allocation table and		*/
/* marks space as used in allocation table.                                 */
/* File must be opened read/write DENY ALL									*/
/* Returns offset to beginning of data (in bytes, not blocks)				*/
/* Assumes smb_open_da() has been called									*/
/* fclose(sda_fp) should be called after									*/
/* Returns negative on error												*/
/****************************************************************************/
long smb_allocdat(ulong length, ushort headers)
{
    ushort  i,j;
	ulong	l,blocks,offset=0L;

blocks=smb_datblocks(length);
j=0;	/* j is consecutive unused block counter */
fflush(sda_fp);
rewind(sda_fp);
while(!feof(sda_fp)) {
	if(!fread(&i,2,1,sda_fp))
        break;
	offset+=SDT_BLOCK_LEN;
    if(!i) j++;
    else   j=0;
	if(j==blocks) {
		offset-=(blocks*SDT_BLOCK_LEN);
        break; } }
clearerr(sda_fp);
fseek(sda_fp,(offset/SDT_BLOCK_LEN)*2L,SEEK_SET);
for(l=0;l<blocks;l++)
	if(!fwrite(&headers,2,1,sda_fp))
		return(-1);
fflush(sda_fp);
return(offset);
}

/****************************************************************************/
/* Allocates space for data, but doesn't search for unused blocks           */
/* Returns negative on error												*/
/****************************************************************************/
long smb_fallocdat(ulong length, ushort headers)
{
	ulong	l,blocks,offset;

fflush(sda_fp);
clearerr(sda_fp);
blocks=smb_datblocks(length);
fseek(sda_fp,0L,SEEK_END);
offset=(ftell(sda_fp)/2L)*SDT_BLOCK_LEN;
for(l=0;l<blocks;l++)
	if(!fwrite(&headers,2,1,sda_fp))
        break;
fflush(sda_fp);
if(l<blocks)
	return(-1L);
return(offset);
}

/****************************************************************************/
/* De-allocates space for data												*/
/* Returns non-zero on error												*/
/****************************************************************************/
int smb_freemsgdat(ulong offset, ulong length, ushort headers)
{
	ushort	i;
	ulong	l,blocks;

blocks=smb_datblocks(length);

clearerr(sda_fp);
for(l=0;l<blocks;l++) {
	if(fseek(sda_fp,((offset/SDT_BLOCK_LEN)+l)*2L,SEEK_SET))
		return(1);
	if(!fread(&i,2,1,sda_fp))
		return(2);
	if(headers>i)
		i=0;			/* don't want to go negative */
	else
		i-=headers;
	if(fseek(sda_fp,-2L,SEEK_CUR))
		return(3);
	if(!fwrite(&i,2,1,sda_fp))
		return(4); }
fflush(sda_fp);
return(0);
}

/****************************************************************************/
/* Adds to data allocation records for blocks starting at 'offset'          */
/* Returns non-zero on error												*/
/****************************************************************************/
int smb_incdat(ulong offset, ulong length, ushort headers)
{
	ushort	i;
	ulong	l,blocks;

clearerr(sda_fp);
blocks=smb_datblocks(length);
for(l=0;l<blocks;l++) {
	fseek(sda_fp,((offset/SDT_BLOCK_LEN)+l)*2L,SEEK_SET);
	if(!fread(&i,2,1,sda_fp))
		return(1);
	i+=headers;
	fseek(sda_fp,-2L,SEEK_CUR);
	if(!fwrite(&i,2,1,sda_fp))
		return(2); }
fflush(sda_fp);
return(0);
}

/****************************************************************************/
/* De-allocates blocks for header record									*/
/* Returns non-zero on error												*/
/****************************************************************************/
int smb_freemsghdr(ulong offset, ulong length)
{
	uchar	c=0;
	ulong	l,blocks;

clearerr(sha_fp);
blocks=smb_hdrblocks(length);
fseek(sha_fp,offset/SHD_BLOCK_LEN,SEEK_SET);
for(l=0;l<blocks;l++)
	if(!fwrite(&c,1,1,sha_fp))
		return(1);
fflush(sha_fp);
return(0);
}

/****************************************************************************/
/* Frees all allocated header and data blocks for 'msg'                     */
/****************************************************************************/
int smb_freemsg(smbmsg_t msg, smbstatus_t status)
{
	int 	i;
	ushort	x;

if(status.attr&SMB_HYPERALLOC)	/* Nothing to do */
	return(0);

for(x=0;x<msg.hdr.total_dfields;x++) {
	if((i=smb_freemsgdat(msg.hdr.offset+msg.dfield[x].offset
		,msg.dfield[x].length,1))!=0)
		return(i); }
return(smb_freemsghdr(msg.idx.offset-status.header_offset,msg.hdr.length));
}

/****************************************************************************/
/* Finds unused space in header file based on block allocation table and	*/
/* marks space as used in allocation table.                                 */
/* File must be opened read/write DENY ALL									*/
/* Returns offset to beginning of header (in bytes, not blocks) 			*/
/* Assumes smb_open_ha() has been called									*/
/* fclose(sha_fp) should be called after									*/
/* Returns -1L on error 													*/
/****************************************************************************/
long smb_allochdr(ulong length)
{
	uchar	c;
	ushort	i;
	ulong	l,blocks,offset=0;

blocks=smb_hdrblocks(length);
i=0;	/* i is consecutive unused block counter */
fflush(sha_fp);
rewind(sha_fp);
while(!feof(sha_fp)) {
	if(!fread(&c,1,1,sha_fp))
        break;
	offset+=SHD_BLOCK_LEN;
	if(!c) i++;
	else   i=0;
	if(i==blocks) {
		offset-=(blocks*SHD_BLOCK_LEN);
        break; } }
clearerr(sha_fp);
fseek(sha_fp,offset/SHD_BLOCK_LEN,SEEK_SET);
c=1;
for(l=0;l<blocks;l++)
	if(!fwrite(&c,1,1,sha_fp))
		return(-1L);
fflush(sha_fp);
return(offset);
}

/****************************************************************************/
/* Allocates space for index, but doesn't search for unused blocks          */
/* Returns -1L on error 													*/
/****************************************************************************/
long smb_fallochdr(ulong length)
{
	uchar	c=1;
	ulong	l,blocks,offset;

blocks=smb_hdrblocks(length);
fflush(sha_fp);
clearerr(sha_fp);
fseek(sha_fp,0L,SEEK_END);
offset=ftell(sha_fp)*SHD_BLOCK_LEN;
for(l=0;l<blocks;l++)
	if(!fwrite(&c,1,1,sha_fp))
		return(-1L);
fflush(sha_fp);
return(offset);
}

/************************************************************************/
/* Allocate header blocks using Hyper Allocation						*/
/* this function should be most likely not be called from anywhere but	*/
/* smb_addmsghdr()														*/
/************************************************************************/
long smb_hallochdr(ulong header_offset)
{
	long l;

fflush(shd_fp);
fseek(shd_fp,0L,SEEK_END);
l=ftell(shd_fp);
if(l<header_offset) 					/* Header file truncated?!? */
	return(header_offset);
while((l-header_offset)%SHD_BLOCK_LEN)	/* Make sure even block boundry */
	l++;
return(l-header_offset);
}

/************************************************************************/
/* Allocate data blocks using Hyper Allocation							*/
/* smb_locksmbhdr() should be called before this function and not		*/
/* unlocked until all data fields for this message have been written	*/
/* to the SDT file														*/
/************************************************************************/
long smb_hallocdat()
{
	long l;

fflush(sdt_fp);
fseek(sdt_fp,0L,SEEK_END);
l=ftell(sdt_fp);
if(l<=0)
	return(l);
while(l%SDT_BLOCK_LEN)					/* Make sure even block boundry */
	l++;
return(l);
}

/* End of SMBLIB.C */
