#line 1 "MSG1.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/***********************************************************************/
/* Functions that do i/o with messages (posts/mail/auto) or their data */
/***********************************************************************/

#include "sbbs.h"

#define LZH 1

void lfputs(char HUGE16 *buf, FILE *fp)
{
while(*buf) {
    fputc(*buf,fp);
    buf++; }
}

/****************************************************************************/
/* Loads an SMB message from the open msg base the fastest way possible 	*/
/* first by offset, and if that's the wrong message, then by number.        */
/* Returns 1 if the message was loaded and left locked, otherwise			*/
/****************************************************************************/
int loadmsg(smbmsg_t *msg, ulong number)
{
	char str[128];
	int i;

if(msg->idx.offset) {				/* Load by offset if specified */

	if((i=smb_lockmsghdr(&smb,msg))!=0) {
		errormsg(WHERE,ERR_LOCK,smb.file,i);
		return(0); }

	i=smb_getmsghdr(&smb,msg);
	if(!i && msg->hdr.number==number)
		return(1);

	/* Wrong offset  */

	if(!i) {
		smb_freemsgmem(msg);
		msg->total_hfields=0; }

	smb_unlockmsghdr(&smb,msg); }

msg->hdr.number=number;
if((i=smb_getmsgidx(&smb,msg))!=0)				 /* Message is deleted */
	return(0);
if((i=smb_lockmsghdr(&smb,msg))!=0) {
	errormsg(WHERE,ERR_LOCK,smb.file,i);
	return(0); }
if((i=smb_getmsghdr(&smb,msg))!=0) {
	sprintf(str,"(%06lX) #%lu/%lu %s",msg->idx.offset,msg->idx.number
		,number,smb.file);
	smb_unlockmsghdr(&smb,msg);
	errormsg(WHERE,ERR_READ,str,i);
	return(0); }
return(msg->total_hfields);
}

/****************************************************************************/
/* Displays a text file to the screen, reading/display a block at time		*/
/****************************************************************************/
void putmsg_fp(FILE *fp, long length, int mode)
{
	uchar *buf,tmpatr;
	int i,j,b=8192,orgcon=console;
	long l;

tmpatr=curatr;	/* was lclatr(-1) */
if((buf=MALLOC(b+1))==NULL) {
	errormsg(WHERE,ERR_ALLOC,nulstr,b+1L);
    return; }
for(l=0;l<length;l+=b) {
	if(l+b>length)
		b=length-l;
	i=j=fread(buf,1,b,fp);
	if(!j) break;						/* No bytes read */
	if(l+i<length)						/* Not last block */
		while(i && buf[i-1]!=LF) i--;	/* Search for last LF */
	if(!i) i=j; 						/* None found */
	buf[i]=0;
	if(i<j)
		fseek(fp,(long)-(j-i),SEEK_CUR);
	b=i;
	if(putmsg(buf,mode|P_SAVEATR))
		break; }
if(!(mode&P_SAVEATR)) {
	console=orgcon;
	attr(tmpatr); }
FREE(buf);
}

void show_msgattr(ushort attr)
{

bprintf(text[MsgAttr]
	,attr&MSG_PRIVATE	? "Private  "   :nulstr
	,attr&MSG_READ		? "Read  "      :nulstr
	,attr&MSG_DELETE	? "Deleted  "   :nulstr
	,attr&MSG_KILLREAD	? "Kill  "      :nulstr
	,attr&MSG_ANONYMOUS ? "Anonymous  " :nulstr
	,attr&MSG_LOCKED	? "Locked  "    :nulstr
	,attr&MSG_PERMANENT ? "Permanent  " :nulstr
	,attr&MSG_MODERATED ? "Moderated  " :nulstr
	,attr&MSG_VALIDATED ? "Validated  " :nulstr
	,attr&MSG_REPLIED	? "Replied  "	:nulstr
	,nulstr
	,nulstr
	,nulstr
	,nulstr
	,nulstr
	,nulstr
	);
}

/****************************************************************************/
/* Displays a message header to the screen                                  */
/****************************************************************************/
void show_msghdr(smbmsg_t msg)
{
	char	*sender=NULL;
	int 	i;

attr(LIGHTGRAY);
if(useron.misc&CLRSCRN)
    outchar(FF);
else
    CRLF;
bprintf(text[MsgSubj],msg.subj);
if(msg.hdr.attr)
	show_msgattr(msg.hdr.attr);

bprintf(text[MsgTo],msg.to);
if(msg.to_ext)
	bprintf(text[MsgToExt],msg.to_ext);
if(msg.to_net.addr)
	bprintf(text[MsgToNet],msg.to_net.type==NET_FIDO
        ? faddrtoa(*(faddr_t *)msg.to_net.addr) : msg.to_net.addr);
if(!(msg.hdr.attr&MSG_ANONYMOUS) || SYSOP) {
	bprintf(text[MsgFrom],msg.from);
	if(msg.from_ext)
		bprintf(text[MsgFromExt],msg.from_ext);
	if(msg.from_net.addr)
		bprintf(text[MsgFromNet],msg.from_net.type==NET_FIDO
			? faddrtoa(*(faddr_t *)msg.from_net.addr)
				: msg.from_net.addr); }
bprintf(text[MsgDate]
	,timestr((time_t *)&msg.hdr.when_written.time)
    ,zonestr(msg.hdr.when_written.zone));

CRLF;

for(i=0;i<msg.total_hfields;i++) {
	if(msg.hfield[i].type==SENDER)
		sender=msg.hfield_dat[i];
	if(msg.hfield[i].type==FORWARDED && sender)
		bprintf(text[ForwardedFrom],sender
			,timestr((time_t *)msg.hfield_dat[i])); }

/* Debug stuff
if(SYSOP) {
	bprintf("\1n\1c\r\nAux  : \1h%08lX",msg.hdr.auxattr);
	bprintf("\1n\1c\r\nNum  : \1h%lu",msg.hdr.number); }
*/

CRLF;
}

#if !LZH

/****************************************************************************/
/* Displays message header and text (if not deleted)                        */
/****************************************************************************/
void show_msg(smbmsg_t msg, int mode)
{
	ushort xlat;
    int i;

show_msghdr(msg);
for(i=0;i<msg.hdr.total_dfields;i++)
	switch(msg.dfield[i].type) {
        case TEXT_BODY:
        case TEXT_TAIL:
			fseek(smb.sdt_fp,msg.hdr.offset+msg.dfield[i].offset
                ,SEEK_SET);
			fread(&xlat,2,1,smb.sdt_fp);
			if(xlat!=XLAT_NONE) 		/* no translations supported */
				continue;
			putmsg_fp(smb.sdt_fp,msg.dfield[i].length-2,mode);
            CRLF;
            break; }
}

#else

/****************************************************************************/
/* Displays message header and text (if not deleted)                        */
/****************************************************************************/
void show_msg(smbmsg_t msg, int mode)
{
	char *inbuf,*lzhbuf;
	ushort xlat;
	int i,lzh;
	long lzhlen,length;

show_msghdr(msg);
for(i=0;i<msg.hdr.total_dfields;i++)
	switch(msg.dfield[i].type) {
        case TEXT_BODY:
        case TEXT_TAIL:
			fseek(smb.sdt_fp,msg.hdr.offset+msg.dfield[i].offset
                ,SEEK_SET);
			fread(&xlat,2,1,smb.sdt_fp);
			lzh=0;
			if(xlat==XLAT_LZH) {
				lzh=1;
				fread(&xlat,2,1,smb.sdt_fp); }
			if(xlat!=XLAT_NONE) 		/* no translations supported */
				continue;
			if(lzh) {
				length=msg.dfield[i].length-4;
				if((inbuf=MALLOC(length))==NULL) {
					errormsg(WHERE,ERR_ALLOC,nulstr,length);
					continue; }
				fread(inbuf,length,1,smb.sdt_fp);
				lzhlen=*(long *)inbuf;
/**
				if(SYSOP)
				bprintf("Decoding %lu bytes of LZH into %lu bytes of text "
					"(%d%% compression)"
					,length,lzhlen
					,(int)(((float)(lzhlen-length)/lzhlen)*100.0));
**/
				if((lzhbuf=MALLOC(lzhlen+2L))==NULL) {
					FREE(inbuf);
					errormsg(WHERE,ERR_ALLOC,nulstr,lzhlen+2L);
					continue; }
				lzh_decode(inbuf,length,lzhbuf);
				lzhbuf[lzhlen]=0;
//				  CRLF;
				putmsg(lzhbuf,P_NOATCODES);
				FREE(lzhbuf);
				FREE(inbuf); }
			else
				putmsg_fp(smb.sdt_fp,msg.dfield[i].length-2,mode);
            CRLF;
            break; }
}

#endif

void quotemsg(smbmsg_t msg, int tails)
{
	char	str[256];

sprintf(str,"%sQUOTES.TXT",node_dir);
remove(str);
msgtotxt(msg,str,0,tails);
}


/****************************************************************************/
/* Writes message header and text data to a text file						*/
/****************************************************************************/
void msgtotxt(smbmsg_t msg, char *str, int header, int tails)
{
	uchar	HUGE16 *buf;
	ushort	xlat;
	int 	i,j,x;
	ulong	l;
	FILE	*out;

if((out=fnopen(&i,str,O_WRONLY|O_CREAT|O_APPEND))==NULL) {
	errormsg(WHERE,ERR_OPEN,str,0);
	return; }
if(header) {
	fprintf(out,"\r\n");
	fprintf(out,"Subj : %s\r\n",msg.subj);
	fprintf(out,"To   : %s",msg.to);
	if(msg.to_ext)
		fprintf(out," #%s",msg.to_ext);
	if(msg.to_net.addr)
		fprintf(out," (%s)",msg.to_net.type==NET_FIDO
			? faddrtoa(*(faddr_t *)msg.to_net.addr) : msg.to_net.addr);
	fprintf(out,"\r\nFrom : %s",msg.from);
	if(msg.from_ext && !(msg.hdr.attr&MSG_ANONYMOUS))
		fprintf(out," #%s",msg.from_ext);
	if(msg.from_net.addr)
		fprintf(out," (%s)",msg.from_net.type==NET_FIDO
			? faddrtoa(*(faddr_t *)msg.from_net.addr)
				: msg.from_net.addr);
	fprintf(out,"\r\nDate : %.24s %s"
		,timestr((time_t *)&msg.hdr.when_written.time)
		,zonestr(msg.hdr.when_written.zone));
	fprintf(out,"\r\n\r\n"); }

buf=smb_getmsgtxt(&smb,&msg,tails);
if(buf) {
	lfputs(buf,out);
	LFREE(buf); }
else if(smb_getmsgdatlen(&msg)>2)
	errormsg(WHERE,ERR_ALLOC,smb.file,smb_getmsgdatlen(&msg));
fclose(out);
}


/****************************************************************************/
/* Returns total number of posts in a sub-board 							*/
/****************************************************************************/
ulong getposts(uint subnum)
{
	char str[128];
	ulong l;

sprintf(str,"%s%s.SID",sub[subnum]->data_dir,sub[subnum]->code);
l=flength(str);
if((long)l==-1)
	return(0);
return(l/sizeof(idxrec_t));
}

/****************************************************************************/
/* Returns message number posted at or after time							*/
/****************************************************************************/
ulong getmsgnum(uint subnum, time_t t)
{
    int     i;
	ulong	l,total,bot,top;
	smbmsg_t msg;

if(!t)
	return(0);

sprintf(smb.file,"%s%s",sub[subnum]->data_dir,sub[subnum]->code);
smb.retry_time=smb_retry_time;
if((i=smb_open(&smb))!=0) {
	errormsg(WHERE,ERR_OPEN,smb.file,i);
	return(0); }

total=filelength(fileno(smb.sid_fp))/sizeof(idxrec_t);

if(!total) {		   /* Empty base */
	smb_close(&smb);
	return(0); }

if((i=smb_locksmbhdr(&smb))!=0) {
	smb_close(&smb);
	errormsg(WHERE,ERR_LOCK,smb.file,i);
    return(0); }

if((i=smb_getlastidx(&smb,&msg.idx))!=0) {
	smb_close(&smb);
	errormsg(WHERE,ERR_READ,smb.file,i);
    return(0); }

if(msg.idx.time<=t) {
	smb_close(&smb);
	return(msg.idx.number); }

bot=0;
top=total;
l=total/2; /* Start at middle index */
clearerr(smb.sid_fp);
while(1) {
	fseek(smb.sid_fp,l*sizeof(idxrec_t),SEEK_SET);
	if(!fread(&msg.idx,sizeof(idxrec_t),1,smb.sid_fp))
		break;
	if(bot==top-1)
		break;
	if(msg.idx.time>t) {
		top=l;
		l=bot+((top-bot)/2);
		continue; }
	if(msg.idx.time<t) {
		bot=l;
		l=top-((top-bot)/2);
		continue; }
	break; }
smb_close(&smb);
return(msg.idx.number);
}

/****************************************************************************/
/* Returns the time of the message number pointed to by 'ptr'               */
/****************************************************************************/
time_t getmsgtime(uint subnum, ulong ptr)
{
	char	str[128];
	int 	i;
	long	l;
	smbmsg_t msg;
	idxrec_t lastidx;

sprintf(smb.file,"%s%s",sub[subnum]->data_dir,sub[subnum]->code);
smb.retry_time=smb_retry_time;
if((i=smb_open(&smb))!=0) {
	errormsg(WHERE,ERR_OPEN,smb.file,i);
	return(0); }
if(!filelength(fileno(smb.sid_fp))) {			/* Empty base */
	smb_close(&smb);
	return(0); }
msg.offset=0;
msg.hdr.number=0;
if(smb_getmsgidx(&smb,&msg)) {				/* Get first message index */
	smb_close(&smb);
	return(0); }
if(!ptr || msg.idx.number>=ptr) {			/* ptr is before first message */
	smb_close(&smb);
	return(msg.idx.time); } 				/* so return time of first msg */

if(smb_getlastidx(&smb,&lastidx)) { 			 /* Get last message index */
	smb_close(&smb);
	return(0); }
if(lastidx.number<ptr) {					/* ptr is after last message */
	smb_close(&smb);
	return(lastidx.time); } 				/* so return time of last msg */

msg.idx.time=0;
msg.hdr.number=ptr;
if(!smb_getmsgidx(&smb,&msg)) {
	smb_close(&smb);
	return(msg.idx.time); }

if(ptr-msg.idx.number < lastidx.number-ptr) {
	msg.offset=0;
	msg.idx.number=0;
	while(msg.idx.number<ptr) {
		msg.hdr.number=0;
		if(smb_getmsgidx(&smb,&msg) || msg.idx.number>=ptr)
			break;
		msg.offset++; }
	smb_close(&smb);
	return(msg.idx.time); }

ptr--;
while(ptr) {
	msg.hdr.number=ptr;
	if(!smb_getmsgidx(&smb,&msg))
		break;
	ptr--; }
smb_close(&smb);
return(msg.idx.time);
}


/****************************************************************************/
/* Returns the total number of msgs in the sub-board and sets 'ptr' to the  */
/* number of the last message in the sub (0) if no messages.				*/
/****************************************************************************/
ulong getlastmsg(uint subnum, ulong *ptr, time_t *t)
{
	char		str[256];
	int 		i;
	ulong		total;
	idxrec_t	idx;

if(ptr)
	(*ptr)=0;
if(t)
	(*t)=0;
if(subnum>=total_subs)
	return(0);

sprintf(smb.file,"%s%s",sub[subnum]->data_dir,sub[subnum]->code);
smb.retry_time=smb_retry_time;
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

