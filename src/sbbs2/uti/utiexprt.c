/* UTIEXPRT.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "uti.h"
#include "post.h"

char *nulstr="";
smb_t smb;

ulong loadmsgs(post_t huge **post, ulong ptr)
{
	int i;
	long l=0;
	idxrec_t idx;


if((i=smb_locksmbhdr(&smb))!=0) {				  /* Be sure noone deletes or */
	errormsg(WHERE,ERR_LOCK,smb.file,i);		/* adds while we're reading */
	return(0L); }

fseek(smb.sid_fp,0L,SEEK_SET);
while(!feof(smb.sid_fp)) {
	if(!fread(&idx,sizeof(idxrec_t),1,smb.sid_fp))
        break;

	if(idx.number<ptr || idx.attr&MSG_DELETE)
		continue;

	if(idx.attr&MSG_MODERATED && !(idx.attr&MSG_VALIDATED))
		break;

	if(((*post)=(post_t huge *)REALLOC((*post),sizeof(post_t)*(l+1)))
        ==NULL) {
		smb_unlocksmbhdr(&smb);
		errormsg(WHERE,ERR_ALLOC,smb.file,sizeof(post_t)*(l+1));
		return(l); }
	(*post)[l].offset=idx.offset;
	(*post)[l].number=idx.number;
	l++; }
smb_unlocksmbhdr(&smb);
return(l);
}


int main(int argc, char **argv)
{
	char	str[512],*buf,ch,*outbuf;
	ushort	xlat;
	int 	i,file,tear,cr,net=0,lzh;
	uint	subnum;
	long	l,m,length,buflen;
	ulong	msgnum,posts,exported=0;
	FILE	*stream;
	post_t	huge *post;
	smbmsg_t msg;
	struct	date date;
	struct	time curtime;

PREPSCREEN;

printf("Synchronet UTIEXPRT v%s\n",VER);

if(argc<3)
	exit(1);

if(argc>4 && !stricmp(argv[4],"/NETWORK"))
	net=1;

uti_init("UTIEXPRT",argc,argv);

if((file=nopen(argv[3],O_CREAT|O_TRUNC|O_WRONLY))==-1)
	bail(2);
if((stream=fdopen(file,"wb"))==NULL)
	bail(2);
setvbuf(stream,0,_IOFBF,4096);

subnum=getsubnum(argv[1]);
if((int)subnum==-1)
	bail(7);
msgnum=atol(argv[2]);

sprintf(smb.file,"%s%s",sub[subnum]->data_dir,sub[subnum]->code);
smb.retry_time=30;
if((i=smb_open(&smb))!=0) {
	errormsg(WHERE,ERR_OPEN,smb.file,i);
	bail(5); }

post=NULL;
posts=loadmsgs(&post,msgnum);

printf("\nExporting\n\n");
for(l=0;l<posts;l++) {
	printf("\rScanning: %lu of %lu  Exported: %lu",l+1,posts,exported);
	msg.idx.offset=post[l].offset;
	if((i=smb_lockmsghdr(&smb,&msg))!=0) {
		errormsg(WHERE,ERR_LOCK,smb.file,i);
		continue; }
	i=smb_getmsghdr(&smb,&msg);
	if(i || msg.hdr.number!=post[l].number) {
		smb_unlockmsghdr(&smb,&msg);
		smb_freemsgmem(&msg);

		msg.hdr.number=post[l].number;
		if((i=smb_getmsgidx(&smb,&msg))!=0) {
			errormsg(WHERE,ERR_READ,smb.file,i);
			continue; }
		if((i=smb_lockmsghdr(&smb,&msg))!=0) {
			errormsg(WHERE,ERR_LOCK,smb.file,i);
			continue; }
		if((i=smb_getmsghdr(&smb,&msg))!=0) {
			smb_unlockmsghdr(&smb,&msg);
			errormsg(WHERE,ERR_READ,smb.file,i);
			continue; } }

	if(net										/* Network */
		&& (!strncmpi(msg.subj,"NE:",3)         /* No Echo */
		|| msg.from_net.type==NET_POSTLINK)) {	/* from PostLink */
		smb_unlockmsghdr(&smb,&msg);
		smb_freemsgmem(&msg);
		continue; } 					/* From a Fido node, ignore it */

	if(net && !(sub[subnum]->misc&SUB_GATE) && msg.from_net.type) {
		smb_unlockmsghdr(&smb,&msg);
		smb_freemsgmem(&msg);
		continue; }

	exported++;

	fprintf(stream,"%s\r\n%s\r\n%s\r\n"
		,msg.to,msg.from,msg.subj);
	unixtodos(msg.hdr.when_written.time,&date,&curtime);
	fprintf(stream,"%lu\r\n%lu\r\n%02u/%02u/%02u\r\n%02u:%02u\r\n"
		"%s\r\n%c\r\n%c\r\nTEXT:\r\n"
		,msg.hdr.number
		,msg.hdr.thread_orig
		,date.da_mon,date.da_day,TM_YEAR(date.da_year-1900)
		,curtime.ti_hour,curtime.ti_min
		,msg.hdr.attr&MSG_PRIVATE ? "PRIVATE" : "PUBLIC"
		,msg.hdr.attr&MSG_READ ? 'Y':'N'
		,strncmpi(msg.subj,"NE:",3) ? 'Y':'N');

	for(i=0;i<msg.hdr.total_dfields;i++) {

		if(msg.dfield[i].type!=TEXT_BODY
			&& msg.dfield[i].type!=TEXT_TAIL)
			continue;					/* skip non-text data fields */

		if(msg.dfield[i].length<3)		/* need at least 3 bytes */
			continue;

		fseek(smb.sdt_fp,msg.hdr.offset+msg.dfield[i].offset,SEEK_SET);

		lzh=0;
		fread(&xlat,2,1,smb.sdt_fp);
		if(xlat==XLAT_LZH) {
			lzh=1;
			fread(&xlat,2,1,smb.sdt_fp); }
		if(xlat!=XLAT_NONE) 			/* no translations supported */
			continue;

		length=msg.dfield[i].length-2;
		if(lzh)
			length-=2;

		if((buf=MALLOC(length))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,length);
			continue; }

		fread(buf,length,1,smb.sdt_fp);

		if(lzh) {
			buflen=*(long *)buf;
			if((outbuf=MALLOC(buflen))==NULL) {
				errormsg(WHERE,ERR_ALLOC,"lzh",buflen);
				FREE(buf);
				continue; }
			length=lzh_decode(buf,length,outbuf);
			FREE(buf);
			buf=outbuf; }

		tear=0;
		for(m=0,cr=1;m<length;m++) {
			if(buf[m]==1) { /* Ctrl-A, so skip it and the next char */
				m++;
				continue; }
			if(buf[m]==0)  /* Ignore line feeds */
				continue;

			if(m+3<length && cr && buf[m]=='-' && buf[m+1]=='-'
				&& buf[m+2]=='-' && (buf[m+3]==CR || buf[m+3]==SP))
				tear=1;

			if(buf[m]==LF)
				cr=1;
			else
				cr=0;

			if(sub[subnum]->misc&SUB_ASCII) {
				if(buf[m]<SP && buf[m]!=CR) /* Ctrl ascii */
					buf[m]='.';             /* converted to '.' */
				if((uchar)buf[m]>0x7f)		/* extended ASCII */
					buf[m]='*'; }           /* converted to '*' */
			fputc(buf[m],stream); }
		fprintf(stream,"\r\n");
		FREE(buf); }

	if(!(sub[subnum]->misc&SUB_NOTAG)) {
		if(!tear)	/* No previous tear line */
			fprintf(stream,"---\r\n");            /* so add one */
		if(sub[subnum]->misc&SUB_ASCII) ch='*';
		else ch='þ';
		fprintf(stream," %c Synchronet UTI v%s\r\n",ch,VER); }

	fprintf(stream,"\xff\r\n");
	smb_unlockmsghdr(&smb,&msg);
	smb_freemsgmem(&msg); }

sprintf(str,"%20s Scanned %lu, Exported %lu\r\n"
	,"",posts,exported);
write(logfile,str,strlen(str));
printf("\nDone.\n");
smb_close(&smb);
FREE(post);
bail(0);
return(0);
}
