#line 1 "QWKTOMSG.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "qwk.h"

/****************************************************************************/
/* Converts a QWK message packet into a message. The 'qwkbuf' pointer       */
/* points to the message header and 'str' is the file to output to.         */
/* Returns the 1 on success or 0 if failure.								*/
/****************************************************************************/
char qwktomsg(FILE *qwk_fp, uchar *hdrblk, char fromhub, uint subnum
	, uint touser)
{
	uchar	str[256],*body,*tail,col=0,lastch=0,*p,*lzhbuf,qwkbuf[128];
	int 	file,i,j,k,lzh=0,storage,skip=0;
	ushort	xlat;
	long	l,bodylen,taillen,length;
	ulong	crc,block,blocks;
	smbmsg_t	msg;

memset(&msg,0,sizeof(smbmsg_t));		/* Initialize message header */
memcpy(msg.hdr.id,"SHD\x1a",4);
msg.hdr.version=smb_ver();

blocks=atol(hdrblk+116);
if(blocks<2)
	return(0);

if(subnum!=INVALID_SUB
	&& (hdrblk[0]=='*' || hdrblk[0]=='+' || sub[subnum]->misc&SUB_PONLY))
	msg.idx.attr|=MSG_PRIVATE;
if(subnum!=INVALID_SUB && sub[subnum]->misc&SUB_AONLY)
	msg.idx.attr|=MSG_ANONYMOUS;
if(subnum==INVALID_SUB && sys_misc&SM_DELREADM)
	msg.idx.attr|=MSG_KILLREAD;
if((fromhub || useron.rest&FLAG('Q')) &&
	(hdrblk[0]=='*' || hdrblk[0]=='-' || hdrblk[0]=='`'))
	msg.idx.attr|=MSG_READ;

if(subnum!=INVALID_SUB && !fromhub && sub[subnum]->mod_ar[0]
	&& chk_ar(sub[subnum]->mod_ar,useron))
	msg.idx.attr|=MSG_MODERATED;
if(subnum!=INVALID_SUB && !fromhub && sub[subnum]->misc&SUB_SYSPERM
	&& sub_op(subnum))
	msg.idx.attr|=MSG_PERMANENT;

msg.hdr.attr=msg.idx.attr;

date.da_mon=((hdrblk[8]&0xf)*10)+(hdrblk[9]&0xf);
date.da_day=((hdrblk[11]&0xf)*10)+(hdrblk[12]&0xf);
date.da_year=((hdrblk[14]&0xf)*10)+(hdrblk[15]&0xf);
if(date.da_year<Y2K_2DIGIT_WINDOW)
	date.da_year+=100;
date.da_year+=1900;
curtime.ti_hour=((hdrblk[16]&0xf)*10)+(hdrblk[17]&0xf);
curtime.ti_min=((hdrblk[19]&0xf)*10)+(hdrblk[20]&0xf);
curtime.ti_sec=0;
msg.hdr.when_written.time=dostounix(&date,&curtime);
if(!(useron.rest&FLAG('Q')) && !fromhub)
	msg.hdr.when_written.zone=sys_timezone;
msg.hdr.when_imported.time=time(NULL);
msg.hdr.when_imported.zone=sys_timezone;

hdrblk[116]=0; /* don't bleed number-of-blocks and re-msg-num fields together */
msg.hdr.thread_orig=atol(hdrblk+108);

if((uint)subnum==INVALID_SUB) { 		/* E-mail */
	msg.idx.to=touser;

/***					Unnecessary code in v2.10a
	if(!touser) {
		sprintf(str,"%25.25s",hdrblk+21);
		truncsp(str);
		for(i=0;i<total_qhubs;i++)
			if(!stricmp(qhub[i]->id,str))
				break;
		if(i==total_qhubs) {
			errormsg(WHERE,ERR_CHK,str,0);
			return(0); }
		smb_hfield(&msg,RECIPIENT,strlen(str),str); }
	else {
***/
		username(touser,str);
		smb_hfield(&msg,RECIPIENT,strlen(str),str);
		sprintf(str,"%u",touser);
		smb_hfield(&msg,RECIPIENTEXT,strlen(str),str); }

else {
	sprintf(str,"%25.25s",hdrblk+21);     /* To user */
	truncsp(str);
	smb_hfield(&msg,RECIPIENT,strlen(str),str);
	strlwr(str);
	msg.idx.to=crc16(str); }

fread(qwkbuf,1,128,qwk_fp);

if(useron.rest&FLAG('Q') || fromhub) {      /* QWK Net */
	if(!strnicmp(qwkbuf,"@VIA:",5)) {
		p=strchr(qwkbuf,'\xe3');
		if(p) {
			*p=0;
			skip=strlen(qwkbuf)+1; }
		truncsp(qwkbuf);
		p=qwkbuf+5; 					/* Skip "@VIA:" */
		while(*p && *p<=SP) p++;		/* Skip any spaces */
		if(route_circ(p,sys_id)) {
			smb_freemsgmem(&msg);
			bprintf("\r\nCircular message path: %s\r\n",p);
			sprintf(str,"Circular message path: %s from %s"
				,p,fromhub ? qhub[fromhub-1]->id:useron.alias);
			errorlog(str);
			return(0); }
		sprintf(str,"%s/%s"
			,fromhub ? qhub[fromhub-1]->id : useron.alias,p);
		strupr(str);
		update_qwkroute(str); }
	else {
		if(fromhub)
			strcpy(str,qhub[fromhub-1]->id);
		else
			strcpy(str,useron.alias); }
	strupr(str);
	j=NET_QWK;
	smb_hfield(&msg,SENDERNETTYPE,2,&j);
	smb_hfield(&msg,SENDERNETADDR,strlen(str),str);
	sprintf(str,"%25.25s",hdrblk+46);  /* From user */
	truncsp(str);
	if(!strnicmp(qwkbuf+skip,"@TZ:",4)) {
		p=strchr(qwkbuf+skip,'\xe3');
		i=skip;
		if(p) {
			*p=0;
			skip+=strlen(qwkbuf+i)+1; }
		p=qwkbuf+i+4;					/* Skip "@TZ:" */
		while(*p && *p<=SP) p++;		/* Skip any spaces */
		msg.hdr.when_written.zone=(short)ahtoul(p); }
	}
else {
	sprintf(str,"%u",useron.number);
	smb_hfield(&msg,SENDEREXT,strlen(str),str);
	if((uint)subnum!=INVALID_SUB && sub[subnum]->misc&SUB_NAME)
		strcpy(str,useron.name);
	else
		strcpy(str,useron.alias); }

smb_hfield(&msg,SENDER,strlen(str),str);
if((uint)subnum==INVALID_SUB) {
	if(useron.rest&FLAG('Q') || fromhub)
		msg.idx.from=0;
	else
		msg.idx.from=useron.number; }
else {
	strlwr(str);
	msg.idx.from=crc16(str); }

sprintf(str,"%25.25s",hdrblk+71);   /* Subject */
truncsp(str);
remove_re(str);
smb_hfield(&msg,SUBJECT,strlen(str),str);
strlwr(str);
msg.idx.subj=crc16(str);

/********************************/
/* Convert the QWK message text */
/********************************/

bodylen=0;
if((body=LMALLOC((blocks-1L)*128L*2L))==NULL) {
	smb_freemsgmem(&msg);
	errormsg(WHERE,ERR_ALLOC,"QWK msg",(blocks-1L)*128L*2L);
	return(0); }

taillen=0;
if((tail=LMALLOC((blocks-1L)*128L))==NULL) {
	LFREE(body);
	smb_freemsgmem(&msg);
	errormsg(WHERE,ERR_ALLOC,"QWK msg",(blocks-1L)*128L);
    return(0); }

for(block=1;block<blocks;block++) {
	if(block>1)
		if(!fread(qwkbuf,1,128,qwk_fp))
			break;
	for(k=skip;k<128;k++,skip=0) {
		if(qwkbuf[k]==0)
			continue;
		if(!taillen && qwkbuf[k]==SP && col==3 && bodylen>=3
			&& body[bodylen-3]=='-' && body[bodylen-2]=='-'
			&& body[bodylen-1]=='-') {
			bodylen-=3;
			strcpy(tail,"--- ");
			taillen=4;
			col++;
			continue; }
		if(qwkbuf[k]==0xE3) {		/* expand 0xe3 to crlf */
			if(!taillen && col==3 && bodylen>=3 && body[bodylen-3]=='-'
				&& body[bodylen-2]=='-' && body[bodylen-1]=='-') {
				bodylen-=3;
				strcpy(tail,"---");
				taillen=3; }
			col=0;
			if(taillen) {
				tail[taillen++]=CR;
				tail[taillen++]=LF; }
			else {
				body[bodylen++]=CR;
				body[bodylen++]=LF; }
			continue; }
		if(!fromhub && qwkbuf[k]==7 && useron.rest&FLAG('B'))   /* beep res */
			continue;
		if(!fromhub && (qwkbuf[k]==1 || qwkbuf[k]==ESC) /* ANSI restriction */
			&& useron.rest&FLAG('A'))
			continue;
		if(qwkbuf[k]!=1 && lastch!=1)
			col++;
		if(lastch==1 && !validattr(qwkbuf[k])) {
			if(taillen) taillen--;
			else		bodylen--;
			lastch=0;
			continue; }
		lastch=qwkbuf[k];
		if(taillen)
			tail[taillen++]=qwkbuf[k];
		else
			body[bodylen++]=qwkbuf[k]; } }

while(bodylen && body[bodylen-1]==SP) bodylen--; /* remove trailing spaces */
if(bodylen>=2 && body[bodylen-2]==CR && body[bodylen-1]==LF)
	bodylen-=2;

while(taillen && tail[taillen-1]<=SP) taillen--; /* remove trailing garbage */

/*****************/
/* Calculate CRC */
/*****************/

if(smb.status.max_crcs) {
	crc=0xffffffffUL;
	for(l=0;l<bodylen;l++)
		crc=ucrc32(body[l],crc);
	crc=~crc;

	/*******************/
	/* Check for dupes */
	/*******************/

	j=smb_addcrc(&smb,crc);
	if(j) {
		if(j==1) {
			bprintf("\r\nDuplicate message\r\n");
			logline(subnum==INVALID_SUB ? "E!":"P!","Duplicate message"); }
		else
			errormsg(WHERE,ERR_CHK,smb.file,j);

		smb_freemsgmem(&msg);
		LFREE(body);
		LFREE(tail);
		return(0); } }

bputs(text[WritingIndx]);

/*************************************/
/* Write SMB message header and text */
/*************************************/

if(subnum!=INVALID_SUB && sub[subnum]->misc&SUB_LZH && bodylen
	&& bodylen+2+taillen+2>=SDT_BLOCK_LEN
	&& (lzhbuf=(char *)LMALLOC(bodylen*2))!=NULL) {
	length=lzh_encode((uchar *)body,bodylen,(uchar *)lzhbuf);
	if(length>1L
		&& smb_datblocks(length+4L+taillen+2L)
			<smb_datblocks(bodylen+2L+taillen+2L)) {
		bodylen=length; 	/* Compressable */
		length+=4L;
		lzh=1;
		LFREE(body);
		body=lzhbuf; }
	else {					/* Non-compressable */
		length=bodylen+2L;
		LFREE(lzhbuf); } }
else
	length=bodylen+2L;					 /* +2 for translation string */

if(taillen)
	length+=taillen+2L;

if(length&0xfff00000UL) {
	errormsg(WHERE,ERR_LEN,"REP msg",length);
	smb_freemsgmem(&msg);
	LFREE(body);
	LFREE(tail);
	return(0); }

if((i=smb_locksmbhdr(&smb))!=0) {
	errormsg(WHERE,ERR_LOCK,smb.file,i);
	FREE(body);
	FREE(tail);
	return(0); }

if(smb.status.attr&SMB_HYPERALLOC) {
	msg.hdr.offset=smb_hallocdat(&smb);
	storage=SMB_HYPERALLOC; }
else {
	if((i=smb_open_da(&smb))!=0) {
		errormsg(WHERE,ERR_OPEN,smb.file,i);
		FREE(body);
		FREE(tail);
		return(0); }
	if((subnum==INVALID_SUB && sys_misc&SM_FASTMAIL)
		|| (subnum!=INVALID_SUB && sub[subnum]->misc&SUB_FAST)) {
		msg.hdr.offset=smb_fallocdat(&smb,length,1);
		storage=SMB_FASTALLOC; }
	else {
		msg.hdr.offset=smb_allocdat(&smb,length,1);
		storage=SMB_SELFPACK; }
	smb_close_da(&smb); }

if(msg.hdr.offset && msg.hdr.offset<1L) {
	smb_unlocksmbhdr(&smb);
	errormsg(WHERE,ERR_READ,smb.file,msg.hdr.offset);
	smb_freemsgmem(&msg);
	FREE(body);
	FREE(tail); }
fseek(smb.sdt_fp,msg.hdr.offset,SEEK_SET);
if(lzh) {
	xlat=XLAT_LZH;
	fwrite(&xlat,2,1,smb.sdt_fp); }
xlat=XLAT_NONE;
fwrite(&xlat,2,1,smb.sdt_fp);
fwrite(body,bodylen,1,smb.sdt_fp);
smb_dfield(&msg,TEXT_BODY,bodylen+2+(lzh ? 2:0));
if(taillen) {
	fwrite(&xlat,2,1,smb.sdt_fp);
	fwrite(tail,taillen,1,smb.sdt_fp);
	smb_dfield(&msg,TEXT_TAIL,taillen+2); }
fflush(smb.sdt_fp);
smb_unlocksmbhdr(&smb);

if((i=smb_addmsghdr(&smb,&msg,storage))!=0)
	errormsg(WHERE,ERR_WRITE,smb.file,i);

smb_freemsgmem(&msg);

LFREE(body);
LFREE(tail);

return(1);
}
