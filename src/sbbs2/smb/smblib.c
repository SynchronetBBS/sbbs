/* SMBLIB.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "smblib.h"

/* Use smb_ver() and smb_lib_ver() to obtain these values */
#define SMBLIB_VERSION		"2.10"      /* SMB library version */
#define SMB_VERSION 		0x0121		/* SMB format version */
										/* High byte major, low byte minor */

#ifdef _MSC_VER	  /* Microsoft C */
#define sopen(f,o,s,p)	   _sopen(f,o,s,p)
#define close(f)		   _close(f)
#define SH_DENYNO		   _SH_DENYNO
#define SH_DENYRW		   _SH_DENYRW

#include <sys/locking.h>

int lock(int file, long offset, int size) 
{
	int	i;
	long	pos;
   
	pos=tell(file);
	if(offset!=pos)
		lseek(file, offset, SEEK_SET);
	i=locking(file,LK_NBLCK,size);
	if(offset!=pos)
		lseek(file, pos, SEEK_SET);
	return(i);
}

int unlock(int file, long offset, int size)
{
	int	i;
	long	pos;
   
	pos=tell(file);
	if(offset!=pos)
		lseek(file, offset, SEEK_SET);
	i=locking(file,LK_UNLCK,size);
	if(offset!=pos)
		lseek(file, pos, SEEK_SET);
	return(i);
}

#endif /* _MSC_VER */


int SMBCALL smb_ver(void)
{
	return(SMB_VERSION);
}

char * SMBCALL smb_lib_ver(void)
{
	return(SMBLIB_VERSION);
}

/****************************************************************************/
/* Open a message base of name 'smb->file'                                  */
/* Opens files for READing messages or updating message indices only        */
/****************************************************************************/
int SMBCALL smb_open(smb_t *smb)
{
    int file;
    char str[128];
	smbhdr_t hdr;

if(!smb->retry_time)
	smb->retry_time=10;
smb->shd_fp=smb->sdt_fp=smb->sid_fp=NULL;
sprintf(str,"%s.SHD",smb->file);
if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYNO,S_IWRITE|S_IREAD))==-1
	|| (smb->shd_fp=fdopen(file,"r+b"))==NULL) {
	if(file!=-1)
		close(file);
    return(2); }

if(filelength(file)>=sizeof(smbhdr_t)) {
	setvbuf(smb->shd_fp,smb->shd_buf,_IONBF,SHD_BLOCK_LEN);
	if(smb_locksmbhdr(smb)) {
		smb_close(smb);
        return(-1); }
	memset(&hdr,0,sizeof(smbhdr_t));
	fread(&hdr,sizeof(smbhdr_t),1,smb->shd_fp);
    if(memcmp(hdr.id,"SMB\x1a",4)) {
		smb_close(smb);
        return(-2); }
    if(hdr.version<0x110) {         /* Compatibility check */
		smb_close(smb);
        return(-3); }
	if(fread(&(smb->status),1,sizeof(smbstatus_t),smb->shd_fp)
		!=sizeof(smbstatus_t)) {
		smb_close(smb);
		return(-4); }
	smb_unlocksmbhdr(smb);
	rewind(smb->shd_fp); }

setvbuf(smb->shd_fp,smb->shd_buf,_IOFBF,SHD_BLOCK_LEN);

sprintf(str,"%s.SDT",smb->file);
if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYNO,S_IWRITE|S_IREAD))==-1
	|| (smb->sdt_fp=fdopen(file,"r+b"))==NULL) {
	if(file!=-1)
		close(file);
	smb_close(smb);
	return(1); }
setvbuf(smb->sdt_fp,NULL,_IOFBF,2*1024);

sprintf(str,"%s.SID",smb->file);
if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYNO,S_IWRITE|S_IREAD))==-1
	|| (smb->sid_fp=fdopen(file,"r+b"))==NULL) {
	if(file!=-1)
		close(file);
	smb_close(smb);
	return(3); }
setvbuf(smb->sid_fp,NULL,_IOFBF,2*1024);

return(0);
}

/****************************************************************************/
/* Closes the currently open message base									*/
/****************************************************************************/
void SMBCALL smb_close(smb_t *smb)
{
if(smb->shd_fp!=NULL) {
	smb_unlocksmbhdr(smb);		   /* In case it's been locked */
	fclose(smb->shd_fp); }
if(smb->sid_fp!=NULL)
	fclose(smb->sid_fp);
if(smb->sdt_fp!=NULL)
	fclose(smb->sdt_fp);
smb->sid_fp=smb->shd_fp=smb->sdt_fp=NULL;
}

/****************************************************************************/
/* Opens the data block allocation table message base 'smb->file'           */
/* Retrys for retry_time number of seconds									*/
/* Return 0 on success, non-zero otherwise									*/
/****************************************************************************/
int SMBCALL smb_open_da(smb_t *smb)
{
	int 	file;
	char	str[128];
	ulong	start=0;

sprintf(str,"%s.SDA",smb->file);
while(1) {
	if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYRW,S_IWRITE|S_IREAD))!=-1)
		break;
	if(errno!=EACCES)
		return(-1);
	if(!start)
		start=time(NULL);
	else
		if(time(NULL)-start>=smb->retry_time)
			return(-2); }
if((smb->sda_fp=fdopen(file,"r+b"))==NULL) {
	close(file);
	return(-3); }
setvbuf(smb->sda_fp,NULL,_IOFBF,2*1024);
return(0);
}

void SMBCALL smb_close_da(smb_t *smb)
{
if(smb->sda_fp!=NULL)
	fclose(smb->sda_fp);
smb->sda_fp=NULL;
}

/****************************************************************************/
/* Opens the header block allocation table for message base 'smb.file'      */
/* Retrys for smb.retry_time number of seconds								*/
/* Return 0 on success, non-zero otherwise									*/
/****************************************************************************/
int SMBCALL smb_open_ha(smb_t *smb)
{
	int 	file;
	char	str[128];
	ulong	start=0;

sprintf(str,"%s.SHA",smb->file);
while(1) {
	if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYRW,S_IWRITE|S_IREAD))!=-1)
		break;
	if(errno!=EACCES)
		return(-1);
	if(!start)
		start=time(NULL);
	else
		if(time(NULL)-start>=smb->retry_time)
			return(-2); }
if((smb->sha_fp=fdopen(file,"r+b"))==NULL) {
	close(file);
	return(-3); }
setvbuf(smb->sha_fp,NULL,_IOFBF,2*1024);
return(0);
}

void SMBCALL smb_close_ha(smb_t *smb)
{
if(smb->sha_fp!=NULL)
	fclose(smb->sha_fp);
smb->sha_fp=NULL;
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
int SMBCALL smb_stack(smb_t *smb, int op)
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
	if(smb->shd_fp==NULL || smb->sdt_fp==NULL || smb->sid_fp==NULL)
		return(0);	  /* Msg base not open */
	memcpy(stack_file[stack_idx],smb->file,128);
	stack_sdt[stack_idx]=smb->sdt_fp;
	stack_shd[stack_idx]=smb->shd_fp;
	stack_sid[stack_idx]=smb->sid_fp;
	stack_sda[stack_idx]=smb->sda_fp;
	stack_sha[stack_idx]=smb->sha_fp;
	stack_idx++;
	return(0); }
/* pop or xchng */
if(!stack_idx)	/* Nothing on the stack, so do nothing */
	return(0);
if(op==SMB_STACK_XCHNG) {
	if(!smb->shd_fp)
		return(0);
	memcpy(tmp_file,smb->file,128);
	tmp_sdt=smb->sdt_fp;
	tmp_shd=smb->shd_fp;
	tmp_sid=smb->sid_fp;
	tmp_sda=smb->sda_fp;
	tmp_sha=smb->sha_fp; }

stack_idx--;
memcpy(smb->file,stack_file[stack_idx],128);
smb->sdt_fp=stack_sdt[stack_idx];
smb->shd_fp=stack_shd[stack_idx];
smb->sid_fp=stack_sid[stack_idx];
smb->sda_fp=stack_sda[stack_idx];
smb->sha_fp=stack_sha[stack_idx];
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
/* Retrys for smb.retry_time number of seconds								*/
/* Return 0 on success, non-zero otherwise									*/
/****************************************************************************/
int SMBCALL smb_trunchdr(smb_t *smb)
{
	ulong	start=0;

rewind(smb->shd_fp);
while(1) {
	if(!chsize(fileno(smb->shd_fp),0L))
		break;
	if(errno!=EACCES)
		return(-1);
	if(!start)
		start=time(NULL);
	else
		if(time(NULL)-start>=smb->retry_time)		 /* Time-out */
			return(-2); }
return(0);
}

/*********************************/
/* Message Base Header Functions */
/*********************************/

/****************************************************************************/
/* Attempts for smb.retry_time number of seconds to lock the msg base hdr	*/
/****************************************************************************/
int SMBCALL smb_locksmbhdr(smb_t *smb)
{
	ulong	start=0;

while(1) {
	if(!lock(fileno(smb->shd_fp),0L,sizeof(smbhdr_t)+sizeof(smbstatus_t)))
		return(0);
	if(!start)
		start=time(NULL);
	else
		if(time(NULL)-start>=smb->retry_time)
			break;						/* Incase we've already locked it */
	unlock(fileno(smb->shd_fp),0L,sizeof(smbhdr_t)+sizeof(smbstatus_t)); }
return(-1);
}

/****************************************************************************/
/* Read the SMB header from the header file and place into smb.status		*/
/****************************************************************************/
int SMBCALL smb_getstatus(smb_t *smb)
{
	int 	i;

setvbuf(smb->shd_fp,smb->shd_buf,_IONBF,SHD_BLOCK_LEN);
clearerr(smb->shd_fp);
fseek(smb->shd_fp,sizeof(smbhdr_t),SEEK_SET);
i=fread(&(smb->status),1,sizeof(smbstatus_t),smb->shd_fp);
setvbuf(smb->shd_fp,smb->shd_buf,_IOFBF,SHD_BLOCK_LEN);
if(i==sizeof(smbstatus_t))
	return(0);
return(1);
}

/****************************************************************************/
/* Writes message base header												*/
/****************************************************************************/
int SMBCALL smb_putstatus(smb_t *smb)
{
	int i;

clearerr(smb->shd_fp);
fseek(smb->shd_fp,sizeof(smbhdr_t),SEEK_SET);
i=fwrite(&(smb->status),1,sizeof(smbstatus_t),smb->shd_fp);
fflush(smb->shd_fp);
if(i==sizeof(smbstatus_t))
	return(0);
return(1);
}

/****************************************************************************/
/* Unlocks previously locks message base header 							*/
/****************************************************************************/
int SMBCALL smb_unlocksmbhdr(smb_t *smb)
{
return(unlock(fileno(smb->shd_fp),0L,sizeof(smbhdr_t)+sizeof(smbstatus_t)));
}

/********************************/
/* Individual Message Functions */
/********************************/

/****************************************************************************/
/* Attempts for smb.retry_time number of seconds to lock the hdr for 'msg'  */
/****************************************************************************/
int SMBCALL smb_lockmsghdr(smb_t *smb, smbmsg_t *msg)
{
	ulong	start=0;

while(1) {
	if(!lock(fileno(smb->shd_fp),msg->idx.offset,sizeof(msghdr_t)))
        return(0);
	if(!start)
		start=time(NULL);
	else
		if(time(NULL)-start>=smb->retry_time)
			break;
	unlock(fileno(smb->shd_fp),msg->idx.offset,sizeof(msghdr_t)); }
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
int SMBCALL smb_getmsgidx(smb_t *smb, smbmsg_t *msg)
{
	idxrec_t idx;
	ulong	 l,length,total,bot,top;

clearerr(smb->sid_fp);
if(!msg->hdr.number) {
	fseek(smb->sid_fp,msg->offset*sizeof(idxrec_t),SEEK_SET);
	if(!fread(&msg->idx,sizeof(idxrec_t),1,smb->sid_fp))
		return(1);
	return(0); }

length=filelength(fileno(smb->sid_fp));
if(!length)
	return(1);
total=length/sizeof(idxrec_t);
if(!total)
	return(1);

bot=0;
top=total;
l=total/2; /* Start at middle index */
while(1) {
	fseek(smb->sid_fp,l*sizeof(idxrec_t),SEEK_SET);
	if(!fread(&idx,sizeof(idxrec_t),1,smb->sid_fp))
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
int SMBCALL smb_getlastidx(smb_t *smb, idxrec_t *idx)
{
	long length;

clearerr(smb->sid_fp);
length=filelength(fileno(smb->sid_fp));
if(length<sizeof(idxrec_t))
	return(-1);
fseek(smb->sid_fp,length-sizeof(idxrec_t),SEEK_SET);
if(!fread(idx,sizeof(idxrec_t),1,smb->sid_fp))
	return(-2);
return(0);
}

/****************************************************************************/
/* Figures out the total length of the header record for 'msg'              */
/* Returns length															*/
/****************************************************************************/
uint SMBCALL smb_getmsghdrlen(smbmsg_t *msg)
{
	int i;

/* fixed portion */
msg->hdr.length=sizeof(msghdr_t);
/* data fields */
msg->hdr.length+=msg->hdr.total_dfields*sizeof(dfield_t);
/* header fields */
for(i=0;i<msg->total_hfields;i++) {
	msg->hdr.length+=sizeof(hfield_t);
	msg->hdr.length+=msg->hfield[i].length; }
return(msg->hdr.length);
}

/****************************************************************************/
/* Figures out the total length of the data buffer for 'msg'                */
/* Returns length															*/
/****************************************************************************/
ulong SMBCALL smb_getmsgdatlen(smbmsg_t *msg)
{
	int i;
	ulong length=0L;

for(i=0;i<msg->hdr.total_dfields;i++)
	length+=msg->dfield[i].length;
return(length);
}

/****************************************************************************/
/* Read header information into 'msg' structure                             */
/* msg->idx.offset must be set before calling this function 				*/
/* Must call smb_freemsgmem() to free memory allocated for var len strs 	*/
/* Returns 0 on success, non-zero if error									*/
/****************************************************************************/
int SMBCALL smb_getmsghdr(smb_t *smb, smbmsg_t *msg)
{
	void	*vp,**vpp;
	ushort	i;
	ulong	l,offset;
	idxrec_t idx;

rewind(smb->shd_fp);
fseek(smb->shd_fp,msg->idx.offset,SEEK_SET);
idx=msg->idx;
offset=msg->offset;
memset(msg,0,sizeof(smbmsg_t));
msg->idx=idx;
msg->offset=offset;
if(!fread(&msg->hdr,sizeof(msghdr_t),1,smb->shd_fp))
	return(-1);
if(memcmp(msg->hdr.id,"SHD\x1a",4))
	return(-2);
if(msg->hdr.version<0x110)
	return(-9);
l=sizeof(msghdr_t);
if(msg->hdr.total_dfields && (msg->dfield
	=(dfield_t *)MALLOC(sizeof(dfield_t)*msg->hdr.total_dfields))==NULL) {
	smb_freemsgmem(msg);
	return(-3); }
i=0;
while(i<msg->hdr.total_dfields && l<msg->hdr.length) {
	if(!fread(&msg->dfield[i],sizeof(dfield_t),1,smb->shd_fp)) {
		smb_freemsgmem(msg);
		return(-4); }
	i++;
	l+=sizeof(dfield_t); }
if(i<msg->hdr.total_dfields) {
	smb_freemsgmem(msg);
	return(-8); }

while(l<msg->hdr.length) {
	i=msg->total_hfields;
	if((vpp=(void **)REALLOC(msg->hfield_dat,sizeof(void *)*(i+1)))==NULL) {
		smb_freemsgmem(msg);
		return(-3); }
	msg->hfield_dat=vpp;
	if((vp=(hfield_t *)REALLOC(msg->hfield,sizeof(hfield_t)*(i+1)))==NULL) {
		smb_freemsgmem(msg);
		return(-3); }
	msg->hfield=vp;
	msg->total_hfields++;
	if(!fread(&msg->hfield[i],sizeof(hfield_t),1,smb->shd_fp)) {
		smb_freemsgmem(msg);
		return(-5); }
	l+=sizeof(hfield_t);
	if((msg->hfield_dat[i]=(char *)MALLOC(msg->hfield[i].length+1))
		==NULL) {			/* Allocate 1 extra for NULL terminator */
		smb_freemsgmem(msg);  /* or 0 length field */
		return(-3); }
	memset(msg->hfield_dat[i],0,msg->hfield[i].length+1);  /* init to NULL */
	if(msg->hfield[i].length
		&& !fread(msg->hfield_dat[i],msg->hfield[i].length,1,smb->shd_fp)) {
		smb_freemsgmem(msg);
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
	smb_freemsgmem(msg);
	return(-7); }
return(0);
}

/****************************************************************************/
/* Frees memory allocated for 'msg'                                         */
/****************************************************************************/
void SMBCALL smb_freemsgmem(smbmsg_t *msg)
{
	ushort	i;

	if(msg->dfield) {
		FREE(msg->dfield);
		msg->dfield=NULL;
	}
	for(i=0;i<msg->total_hfields;i++)
		if(msg->hfield_dat[i]) {
			FREE(msg->hfield_dat[i]);
			msg->hfield_dat[i]=NULL;
		}
	msg->total_hfields=0;
	if(msg->hfield) {
		FREE(msg->hfield);
		msg->hfield=NULL;
	}
	if(msg->hfield_dat) {
		FREE(msg->hfield_dat);
		msg->hfield_dat=NULL;
	}
}

/****************************************************************************/
/* Unlocks header for 'msg'                                                 */
/****************************************************************************/
int SMBCALL smb_unlockmsghdr(smb_t *smb, smbmsg_t *msg)
{
return(unlock(fileno(smb->shd_fp),msg->idx.offset,sizeof(msghdr_t)));
}


/****************************************************************************/
/* Adds a header field to the 'msg' structure (in memory only)              */
/****************************************************************************/
int SMBCALL smb_hfield(smbmsg_t *msg, ushort type, ushort length, void *data)
{
	void *vp,**vpp;
	int i;

i=msg->total_hfields;
if((vp=(hfield_t *)REALLOC(msg->hfield,sizeof(hfield_t)*(i+1)))==NULL)
	return(1);
msg->hfield=vp;
if((vpp=(void **)REALLOC(msg->hfield_dat,sizeof(void *)*(i+1)))==NULL)
	return(2);
msg->hfield_dat=vpp;
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
int SMBCALL smb_dfield(smbmsg_t *msg, ushort type, ulong length)
{
	void *vp;
	int i,j;

i=msg->hdr.total_dfields;
if((vp=(dfield_t *)REALLOC(msg->dfield,sizeof(dfield_t)*(i+1)))==NULL)
	return(1);
msg->dfield=vp;
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
int SMBCALL smb_addcrc(smb_t *smb, ulong crc)
{
	char	str[128];
	int 	file;
	long	length;
	ulong	l,*buf;
	ulong	start=0;

if(!smb->status.max_crcs)
	return(0);

sprintf(str,"%s.SCH",smb->file);
while(1) {
	if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYRW,S_IWRITE|S_IREAD))!=-1)
		break;
	if(errno!=EACCES)
		return(-1);
	if(!start)
		start=time(NULL);
	else
		if(time(NULL)-start>=smb->retry_time)
			return(-2); }

length=filelength(file);
if(length<0L) {
	close(file);
	return(-4); }
if((buf=(ulong *)MALLOC(smb->status.max_crcs*4))==NULL) {
	close(file);
	return(-3); }
if(length>=smb->status.max_crcs*4) { /* Reached or exceeds max crcs */
	read(file,buf,smb->status.max_crcs*4);
	for(l=0;l<smb->status.max_crcs;l++)
		if(crc==buf[l])
			break;
	if(l<smb->status.max_crcs) {				/* Dupe CRC found */
		close(file);
		FREE(buf);
		return(1); }
	chsize(file,0L);				/* truncate it */
	lseek(file,0L,SEEK_SET);
	write(file,buf+4,(smb->status.max_crcs-1)*4); }

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
int SMBCALL smb_addmsghdr(smb_t *smb, smbmsg_t *msg, int storage)
{
	int i;
	long l;

if(smb_locksmbhdr(smb))
    return(1);
if(smb_getstatus(smb))
    return(2);

if(storage!=SMB_HYPERALLOC && (i=smb_open_ha(smb))!=0)
    return(i);

msg->hdr.length=smb_getmsghdrlen(msg);
if(storage==SMB_HYPERALLOC)
	l=smb_hallochdr(smb);
else if(storage==SMB_FASTALLOC)
	l=smb_fallochdr(smb,msg->hdr.length);
else
	l=smb_allochdr(smb,msg->hdr.length);
if(l==-1L) {
	smb_unlocksmbhdr(smb);
	smb_close_ha(smb);
	return(-1); }

smb->status.last_msg++;
msg->idx.number=msg->hdr.number=smb->status.last_msg;
msg->idx.offset=smb->status.header_offset+l;
msg->idx.time=msg->hdr.when_imported.time;
msg->idx.attr=msg->hdr.attr;
msg->offset=smb->status.total_msgs;
smb->status.total_msgs++;
smb_putstatus(smb);

if(storage!=SMB_HYPERALLOC)
	smb_close_ha(smb);
i=smb_putmsg(smb,msg);
smb_unlocksmbhdr(smb);
return(i);
}

/****************************************************************************/
/* Writes both header and index information for msg 'msg'                   */
/****************************************************************************/
int SMBCALL smb_putmsg(smb_t *smb, smbmsg_t *msg)
{
	int i;

i=smb_putmsghdr(smb,msg);
if(i)
	return(i);
return(smb_putmsgidx(smb,msg));
}

/****************************************************************************/
/* Writes index information for 'msg'                                       */
/* msg->idx 																 */
/* and msg->offset must be set prior to calling to this function			 */
/* Returns 0 if everything ok                                               */
/****************************************************************************/
int SMBCALL smb_putmsgidx(smb_t *smb, smbmsg_t *msg)
{

clearerr(smb->sid_fp);
fseek(smb->sid_fp,msg->offset*sizeof(idxrec_t),SEEK_SET);
if(!fwrite(&msg->idx,sizeof(idxrec_t),1,smb->sid_fp))
	return(1);
fflush(smb->sid_fp);
return(0);
}

/****************************************************************************/
/* Writes header information for 'msg'                                      */
/* msg->hdr.length															 */
/* msg->idx.offset															 */
/* and msg->offset must be set prior to calling to this function			 */
/* Returns 0 if everything ok                                               */
/****************************************************************************/
int SMBCALL smb_putmsghdr(smb_t *smb, smbmsg_t *msg)
{
	ushort	i;
	ulong	l;

clearerr(smb->shd_fp);
if(fseek(smb->shd_fp,msg->idx.offset,SEEK_SET))
	return(-1);

/************************************************/
/* Write the fixed portion of the header record */
/************************************************/
if(!fwrite(&msg->hdr,sizeof(msghdr_t),1,smb->shd_fp))
	return(-2);

/************************************************/
/* Write the data fields (each is fixed length) */
/************************************************/
for(i=0;i<msg->hdr.total_dfields;i++)
	if(!fwrite(&msg->dfield[i],sizeof(dfield_t),1,smb->shd_fp))
		return(-3);

/*******************************************/
/* Write the variable length header fields */
/*******************************************/
for(i=0;i<msg->total_hfields;i++) {
	if(!fwrite(&msg->hfield[i],sizeof(hfield_t),1,smb->shd_fp))
		return(-4);
	if(msg->hfield[i].length					 /* more then 0 bytes long */
		&& !fwrite(msg->hfield_dat[i],msg->hfield[i].length,1,smb->shd_fp))
		return(-5); }

l=smb_getmsghdrlen(msg);
while(l%SHD_BLOCK_LEN) {
	if(fputc(0,smb->shd_fp)==EOF)
		return(-6); 			   /* pad block with NULL */
	l++; }
fflush(smb->shd_fp);
return(0);
}

/****************************************************************************/
/* Creates a sub-board's initial header file                                */
/* Truncates and deletes other associated SMB files 						*/
/****************************************************************************/
int SMBCALL smb_create(smb_t *smb)
{
    char        str[128];
	smbhdr_t	hdr;

if(filelength(fileno(smb->shd_fp))>=sizeof(smbhdr_t)+sizeof(smbstatus_t)
	&& smb_locksmbhdr(smb))  /* header exists, so lock it */
	return(1);
memset(&hdr,0,sizeof(smbhdr_t));
memcpy(hdr.id,"SMB\x1a",4);     /* <S> <M> <B> <^Z> */
hdr.version=SMB_VERSION;
hdr.length=sizeof(smbhdr_t)+sizeof(smbstatus_t);
smb->status.last_msg=smb->status.total_msgs=0;
smb->status.header_offset=sizeof(smbhdr_t)+sizeof(smbstatus_t);
rewind(smb->shd_fp);
fwrite(&hdr,1,sizeof(smbhdr_t),smb->shd_fp);
fwrite(&(smb->status),1,sizeof(smbstatus_t),smb->shd_fp);
rewind(smb->shd_fp);
chsize(fileno(smb->shd_fp),sizeof(smbhdr_t)+sizeof(smbstatus_t));
fflush(smb->shd_fp);

rewind(smb->sdt_fp);
chsize(fileno(smb->sdt_fp),0L);
rewind(smb->sid_fp);
chsize(fileno(smb->sid_fp),0L);

sprintf(str,"%s.SDA",smb->file);
remove(str);						/* if it exists, delete it */
sprintf(str,"%s.SHA",smb->file);
remove(str);                        /* if it exists, delete it */
sprintf(str,"%s.SCH",smb->file);
remove(str);
smb_unlocksmbhdr(smb);
return(0);
}

/****************************************************************************/
/* Returns number of data blocks required to store "length" amount of data  */
/****************************************************************************/
ulong SMBCALL smb_datblocks(ulong length)
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
ulong SMBCALL smb_hdrblocks(ulong length)
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
/* smb_close_da() should be called after									*/
/* Returns negative on error												*/
/****************************************************************************/
long SMBCALL smb_allocdat(smb_t *smb, ulong length, ushort headers)
{
    ushort  i,j;
	ulong	l,blocks,offset=0L;

blocks=smb_datblocks(length);
j=0;	/* j is consecutive unused block counter */
fflush(smb->sda_fp);
rewind(smb->sda_fp);
while(!feof(smb->sda_fp)) {
	if(!fread(&i,2,1,smb->sda_fp))
        break;
	offset+=SDT_BLOCK_LEN;
    if(!i) j++;
    else   j=0;
	if(j==blocks) {
		offset-=(blocks*SDT_BLOCK_LEN);
        break; } }
clearerr(smb->sda_fp);
fseek(smb->sda_fp,(offset/SDT_BLOCK_LEN)*2L,SEEK_SET);
for(l=0;l<blocks;l++)
	if(!fwrite(&headers,2,1,smb->sda_fp))
		return(-1);
fflush(smb->sda_fp);
return(offset);
}

/****************************************************************************/
/* Allocates space for data, but doesn't search for unused blocks           */
/* Returns negative on error												*/
/****************************************************************************/
long SMBCALL smb_fallocdat(smb_t *smb, ulong length, ushort headers)
{
	ulong	l,blocks,offset;

fflush(smb->sda_fp);
clearerr(smb->sda_fp);
blocks=smb_datblocks(length);
fseek(smb->sda_fp,0L,SEEK_END);
offset=(ftell(smb->sda_fp)/2L)*SDT_BLOCK_LEN;
for(l=0;l<blocks;l++)
	if(!fwrite(&headers,2,1,smb->sda_fp))
        break;
fflush(smb->sda_fp);
if(l<blocks)
	return(-1L);
return(offset);
}

/****************************************************************************/
/* De-allocates space for data												*/
/* Returns non-zero on error												*/
/****************************************************************************/
int SMBCALL smb_freemsgdat(smb_t *smb, ulong offset, ulong length
			, ushort headers)
{
	int		da_opened=0;
	ushort	i;
	ulong	l,blocks;

blocks=smb_datblocks(length);

if(smb->sda_fp==NULL) {
	if((i=smb_open_da(smb))!=0)
		return(i);
	da_opened=1;
}

clearerr(smb->sda_fp);
for(l=0;l<blocks;l++) {
	if(fseek(smb->sda_fp,((offset/SDT_BLOCK_LEN)+l)*2L,SEEK_SET))
		return(1);
	if(!fread(&i,2,1,smb->sda_fp))
		return(2);
	if(headers>i)
		i=0;			/* don't want to go negative */
	else
		i-=headers;
	if(fseek(smb->sda_fp,-2L,SEEK_CUR))
		return(3);
	if(!fwrite(&i,2,1,smb->sda_fp))
		return(4); }
fflush(smb->sda_fp);
if(da_opened)
	smb_close_da(smb);
return(0);
}

/****************************************************************************/
/* Adds to data allocation records for blocks starting at 'offset'          */
/* Returns non-zero on error												*/
/****************************************************************************/
int SMBCALL smb_incdat(smb_t *smb, ulong offset, ulong length, ushort headers)
{
	ushort	i;
	ulong	l,blocks;

clearerr(smb->sda_fp);
blocks=smb_datblocks(length);
for(l=0;l<blocks;l++) {
	fseek(smb->sda_fp,((offset/SDT_BLOCK_LEN)+l)*2L,SEEK_SET);
	if(!fread(&i,2,1,smb->sda_fp))
		return(1);
	i+=headers;
	fseek(smb->sda_fp,-2L,SEEK_CUR);
	if(!fwrite(&i,2,1,smb->sda_fp))
		return(2); }
fflush(smb->sda_fp);
return(0);
}

/****************************************************************************/
/* De-allocates blocks for header record									*/
/* Returns non-zero on error												*/
/****************************************************************************/
int SMBCALL smb_freemsghdr(smb_t *smb, ulong offset, ulong length)
{
	uchar	c=0;
	ulong	l,blocks;

clearerr(smb->sha_fp);
blocks=smb_hdrblocks(length);
fseek(smb->sha_fp,offset/SHD_BLOCK_LEN,SEEK_SET);
for(l=0;l<blocks;l++)
	if(!fwrite(&c,1,1,smb->sha_fp))
		return(1);
fflush(smb->sha_fp);
return(0);
}

/****************************************************************************/
/* Frees all allocated header and data blocks for 'msg'                     */
/****************************************************************************/
int SMBCALL smb_freemsg(smb_t *smb, smbmsg_t *msg)
{
	int 	i;
	ushort	x;

if(smb->status.attr&SMB_HYPERALLOC)  /* Nothing to do */
	return(0);

for(x=0;x<msg->hdr.total_dfields;x++) {
	if((i=smb_freemsgdat(smb,msg->hdr.offset+msg->dfield[x].offset
		,msg->dfield[x].length,1))!=0)
		return(i); }
return(smb_freemsghdr(smb,msg->idx.offset-smb->status.header_offset
	,msg->hdr.length));
}

/****************************************************************************/
/* Finds unused space in header file based on block allocation table and	*/
/* marks space as used in allocation table.                                 */
/* File must be opened read/write DENY ALL									*/
/* Returns offset to beginning of header (in bytes, not blocks) 			*/
/* Assumes smb_open_ha() has been called									*/
/* smb_close_ha() should be called after									*/
/* Returns -1L on error 													*/
/****************************************************************************/
long SMBCALL smb_allochdr(smb_t *smb, ulong length)
{
	uchar	c;
	ushort	i;
	ulong	l,blocks,offset=0;

blocks=smb_hdrblocks(length);
i=0;	/* i is consecutive unused block counter */
fflush(smb->sha_fp);
rewind(smb->sha_fp);
while(!feof(smb->sha_fp)) {
	if(!fread(&c,1,1,smb->sha_fp))
        break;
	offset+=SHD_BLOCK_LEN;
	if(!c) i++;
	else   i=0;
	if(i==blocks) {
		offset-=(blocks*SHD_BLOCK_LEN);
        break; } }
clearerr(smb->sha_fp);
fseek(smb->sha_fp,offset/SHD_BLOCK_LEN,SEEK_SET);
c=1;
for(l=0;l<blocks;l++)
	if(!fwrite(&c,1,1,smb->sha_fp))
		return(-1L);
fflush(smb->sha_fp);
return(offset);
}

/****************************************************************************/
/* Allocates space for index, but doesn't search for unused blocks          */
/* Returns -1L on error 													*/
/****************************************************************************/
long SMBCALL smb_fallochdr(smb_t *smb, ulong length)
{
	uchar	c=1;
	ulong	l,blocks,offset;

blocks=smb_hdrblocks(length);
fflush(smb->sha_fp);
clearerr(smb->sha_fp);
fseek(smb->sha_fp,0L,SEEK_END);
offset=ftell(smb->sha_fp)*SHD_BLOCK_LEN;
for(l=0;l<blocks;l++)
	if(!fwrite(&c,1,1,smb->sha_fp))
		return(-1L);
fflush(smb->sha_fp);
return(offset);
}

/************************************************************************/
/* Allocate header blocks using Hyper Allocation						*/
/* this function should be most likely not be called from anywhere but	*/
/* smb_addmsghdr()														*/
/************************************************************************/
long SMBCALL smb_hallochdr(smb_t *smb)
{
	ulong l;

fflush(smb->shd_fp);
fseek(smb->shd_fp,0L,SEEK_END);
l=ftell(smb->shd_fp);
if(l<smb->status.header_offset) 			 /* Header file truncated?!? */
	return(smb->status.header_offset);
while((l-smb->status.header_offset)%SHD_BLOCK_LEN)	/* Even block boundry */
	l++;
return(l-smb->status.header_offset);
}

/************************************************************************/
/* Allocate data blocks using Hyper Allocation							*/
/* smb_locksmbhdr() should be called before this function and not		*/
/* unlocked until all data fields for this message have been written	*/
/* to the SDT file														*/
/************************************************************************/
long SMBCALL smb_hallocdat(smb_t *smb)
{
	long l;

fflush(smb->sdt_fp);
fseek(smb->sdt_fp,0L,SEEK_END);
l=ftell(smb->sdt_fp);
if(l<=0)
	return(l);
while(l%SDT_BLOCK_LEN)					/* Make sure even block boundry */
	l++;
return(l);
}


int SMBCALL smb_feof(FILE *fp)
{
return(feof(fp));
}

int SMBCALL smb_ferror(FILE *fp)
{
return(ferror(fp));
}

int SMBCALL smb_fflush(FILE *fp)
{
return(fflush(fp));
}

int SMBCALL smb_fgetc(FILE *fp)
{
return(fgetc(fp));
}

int SMBCALL smb_fputc(int ch, FILE *fp)
{
return(fputc(ch,fp));
}

int SMBCALL smb_fseek(FILE *fp, long offset, int whence)
{
return(fseek(fp,offset,whence));
}

long SMBCALL smb_ftell(FILE *fp)
{
return(ftell(fp));
}

long SMBCALL smb_fgetlength(FILE *fp)
{
return(filelength(fileno(fp)));
}

int SMBCALL smb_fsetlength(FILE *fp, long length)
{
return(chsize(fileno(fp),length));
}

void SMBCALL smb_rewind(FILE *fp)
{
rewind(fp);
}

void SMBCALL smb_clearerr(FILE *fp)
{
clearerr(fp);
}

long SMBCALL smb_fread(void HUGE16 *buf, long bytes, FILE *fp)
{
#ifdef __FLAT__
	return(fread(buf,1,bytes,fp));
#else
	long count;

for(count=bytes;count>0x7fff;count-=0x7fff,(char*)buf+=0x7fff)
	if(fread((char *)buf,1,0x7fff,fp)!=0x7fff)
		return(bytes-count);
if(fread((char *)buf,1,(size_t)count,fp)!=(size_t)count)
	return(bytes-count);
return(bytes);
#endif
}

long SMBCALL smb_fwrite(void HUGE16 *buf, long bytes, FILE *fp)
{
#ifdef __FLAT__
	return(fwrite(buf,1,bytes,fp));
#else
	long count;

for(count=bytes;count>0x7fff;count-=0x7fff,(char*)buf+=0x7fff)
	if(fwrite((char *)buf,1,0x7fff,fp)!=0x7fff)
		return(bytes-count);
if(fwrite((char *)buf,1,(size_t)count,fp)!=(size_t)count)
	return(bytes-count);
return(bytes);
#endif
}

#ifdef SMB_GETMSGTXT

char HUGE16 * SMBCALL smb_getmsgtxt(smb_t *smb, smbmsg_t *msg, ulong mode)
{
	char	HUGE16 *buf=NULL,HUGE16 *lzhbuf,HUGE16 *p;
	ushort	xlat;
	int 	i,lzh;
	long	l=0,lzhlen,length;

for(i=0;i<msg->hdr.total_dfields;i++) {
	if(!(msg->dfield[i].type==TEXT_BODY
		|| (mode&GETMSGTXT_TAILS && msg->dfield[i].type==TEXT_TAIL))
		|| msg->dfield[i].length<=2L)
		continue;
	fseek(smb->sdt_fp,msg->hdr.offset+msg->dfield[i].offset
		,SEEK_SET);
	fread(&xlat,2,1,smb->sdt_fp);
	lzh=0;
	if(xlat==XLAT_LZH) {
		lzh=1;
		fread(&xlat,2,1,smb->sdt_fp); }
	if(xlat!=XLAT_NONE) 	/* no other translations currently supported */
		continue;

	length=msg->dfield[i].length-2L;
	if(lzh) {
		length-=2;
		if(length<1)
			continue;
		if((lzhbuf=LMALLOC(length))==NULL)
			return(buf);
		smb_fread(lzhbuf,length,smb->sdt_fp);
		lzhlen=*(long *)lzhbuf;
		if((p=REALLOC(buf,l+lzhlen+3L))==NULL) {
			FREE(lzhbuf);
			return(buf); }
		buf=p;
		lzh_decode((char *)lzhbuf,length,(char *)buf+l);
		FREE(lzhbuf);
		l+=lzhlen; }
	else {
		if((p=REALLOC(buf,l+length+3L))==NULL)
			return(buf);
		buf=p;
		p=buf+l;
		l+=fread(p,1,length,smb->sdt_fp);
		}
	if(!l)
		continue;
	l--;
	while(l && buf[l]==0) l--;
	l++;
	*(buf+l)=CR;
	l++;
	*(buf+l)=LF;
	l++;
	*(buf+l)=0; }
return(buf);
}

void SMBCALL smb_freemsgtxt(char HUGE16 *buf)
{
if(buf!=NULL)
	FREE(buf);
}

#endif

/* End of SMBLIB.C */
