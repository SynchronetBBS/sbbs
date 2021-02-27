#line 1 "BULKMAIL.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

int bulkmailhdr(int usernum, smbmsg_t *msg, ushort msgattr, ulong offset
    , ulong length, char *title)
{
    char str[256];
    int i,j;
    node_t node;

memset(msg,0,sizeof(smbmsg_t));
memcpy(msg->hdr.id,"SHD\x1a",4);
msg->hdr.version=smb_ver();
msg->hdr.attr=msg->idx.attr=msgattr;
msg->hdr.when_written.time=msg->hdr.when_imported.time=time(NULL);
msg->hdr.when_written.zone=msg->hdr.when_imported.zone=sys_timezone;
msg->hdr.offset=msg->idx.offset=offset;

username(usernum,str);
smb_hfield(msg,RECIPIENT,strlen(str),str);
strlwr(str);

sprintf(str,"%u",usernum);
smb_hfield(msg,RECIPIENTEXT,strlen(str),str);
msg->idx.to=usernum;

strcpy(str,useron.alias);
smb_hfield(msg,SENDER,strlen(str),str);
strlwr(str);

sprintf(str,"%u",useron.number);
smb_hfield(msg,SENDEREXT,strlen(str),str);
msg->idx.from=useron.number;

strcpy(str,title);
smb_hfield(msg,SUBJECT,strlen(str),str);
strlwr(str);
msg->idx.subj=crc16(str);

smb_dfield(msg,TEXT_BODY,length);

j=smb_addmsghdr(&smb,msg,SMB_SELFPACK);
if(j)
    return(j);

// smb_incdat(&smb,offset,length,1); Remove 04/15/96
lncntr=0;
bprintf("Bulk Mailed %s #%d\r\n",username(usernum,tmp),usernum);
sprintf(str,"Bulk Mailed %s #%d",username(usernum,tmp),usernum);
logline("E+",str);
useron.emails++;
logon_emails++;
useron.etoday++;
for(i=1;i<=sys_nodes;i++) { /* Tell user, if online */
    getnodedat(i,&node,0);
    if(node.useron==usernum && !(node.misc&NODE_POFF)
        && (node.status==NODE_INUSE || node.status==NODE_QUIET)) {
        sprintf(str,text[EmailNodeMsg],node_num,useron.alias);
        putnmsg(i,str);
        break; } }
if(i>sys_nodes) {   /* User wasn't online, so leave short msg */
    sprintf(str,text[UserSentYouMail],useron.alias);
    putsmsg(usernum,str); }
return(0);
}

void bulkmail(uchar *ar)
{
	char	str[256],str2[256],msgpath[256],title[LEN_TITLE+1],ch
			,buf[SDT_BLOCK_LEN],found=0;
	ushort	xlat=XLAT_NONE,msgattr=0;
	int 	i,j,k,x,file;
	long	l,msgs=0;
	ulong	length,offset;
	FILE	*instream;
	user_t	user;
	smbmsg_t msg;

memset(&msg,0,sizeof(smbmsg_t));

title[0]=0;
action=NODE_SMAL;
nodesync();

if(sys_misc&SM_ANON_EM && (SYSOP || useron.exempt&FLAG('A'))
    && !noyes(text[AnonymousQ]))
	msgattr|=MSG_ANONYMOUS;

sprintf(msgpath,"%sINPUT.MSG",node_dir);
sprintf(str2,"Bulk Mailing");
if(!writemsg(msgpath,nulstr,title,WM_EMAIL,0,str2)) {
    bputs(text[Aborted]);
    return; }

bputs(text[WritingIndx]);
CRLF;

if((i=smb_stack(&smb,SMB_STACK_PUSH))!=0) {
	errormsg(WHERE,ERR_OPEN,"MAIL",i);
	return; }
sprintf(smb.file,"%sMAIL",data_dir);
smb.retry_time=smb_retry_time;
if((i=smb_open(&smb))!=0) {
	smb_stack(&smb,SMB_STACK_POP);
	errormsg(WHERE,ERR_OPEN,smb.file,i);
	return; }

if(filelength(fileno(smb.shd_fp))<1) {	 /* Create it if it doesn't exist */
	smb.status.max_crcs=mail_maxcrcs;
	smb.status.max_msgs=MAX_SYSMAIL;
	smb.status.max_age=mail_maxage;
	smb.status.attr=SMB_EMAIL;
	if((i=smb_create(&smb))!=0) {
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_CREATE,smb.file,i);
		return; } }

length=flength(msgpath)+2;	 /* +2 for translation string */

if(length&0xfff00000UL) {
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
	errormsg(WHERE,ERR_LEN,smb.file,length);
    return; }

if((i=smb_open_da(&smb))!=0) {
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
	errormsg(WHERE,ERR_OPEN,smb.file,i);
	return; }
if(sys_misc&SM_FASTMAIL)
	offset=smb_fallocdat(&smb,length,1);
else
	offset=smb_allocdat(&smb,length,1);
smb_close_da(&smb);

if((file=open(msgpath,O_RDONLY|O_BINARY))==-1
	|| (instream=fdopen(file,"rb"))==NULL) {
	smb_freemsgdat(&smb,offset,length,1);
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
	errormsg(WHERE,ERR_OPEN,msgpath,O_RDONLY|O_BINARY);
	return; }

setvbuf(instream,NULL,_IOFBF,2*1024);
fseek(smb.sdt_fp,offset,SEEK_SET);
xlat=XLAT_NONE;
fwrite(&xlat,2,1,smb.sdt_fp);
x=SDT_BLOCK_LEN-2;				/* Don't read/write more than 255 */
while(!feof(instream)) {
	memset(buf,0,x);
	j=fread(buf,1,x,instream);
	if((j!=x || feof(instream)) && buf[j-1]==LF && buf[j-2]==CR)
		buf[j-1]=buf[j-2]=0;
	fwrite(buf,j,1,smb.sdt_fp);
	x=SDT_BLOCK_LEN; }
fflush(smb.sdt_fp);
fclose(instream);

j=lastuser();
x=0;

if(*ar)
	for(i=1;i<=j;i++) {
		user.number=i;
		getuserdat(&user);
		if(user.misc&(DELETED|INACTIVE))
			continue;
		if(chk_ar(ar,user)) {
			if(found)
				smb_freemsgmem(&msg);
			x=bulkmailhdr(i,&msg,msgattr,offset,length,title);
			if(x)
				break;
			msgs++;
			found=1; } }
else
	while(1) {
		bputs(text[EnterAfterLastDestUser]);
		if(!getstr(str,LEN_ALIAS,K_UPRLWR))
			break;
		if((i=finduser(str))!=0) {
			if(found)
				smb_freemsgmem(&msg);
			x=bulkmailhdr(i,&msg,msgattr,offset,length,title);
			if(x)
				break;
			msgs++; }
		found=1; }

if((i=smb_open_da(&smb))!=0) {
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
	errormsg(WHERE,ERR_OPEN,smb.file,i);
	return; }
if(!msgs)
	smb_freemsgdat(&smb,offset,length,1);
else if(msgs>1)
	smb_incdat(&smb,offset,length,msgs-1);
smb_close_da(&smb);

smb_close(&smb);
smb_stack(&smb,SMB_STACK_POP);

smb_freemsgmem(&msg);
if(x) {
	smb_freemsgdat(&smb,offset,length,1);
	errormsg(WHERE,ERR_WRITE,smb.file,x);
	return; }

putuserrec(useron.number,U_EMAILS,5,itoa(useron.emails,tmp,10));
putuserrec(useron.number,U_ETODAY,5,itoa(useron.etoday,tmp,10));
}


