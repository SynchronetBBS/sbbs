#line 1 "EMAIL.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "cmdshell.h"

/****************************************************************************/
/* Mails a message to usernumber. 'top' is a buffer to place at beginning   */
/* of message.                                                              */
/* Called from functions main_sec, newuser, readmail and scanposts			*/
/****************************************************************************/
void email(int usernumber, char *top, char *subj, char mode)
{
	char	str[256],str2[256],msgpath[256],title[LEN_TITLE+1],ch
			,buf[SDT_BLOCK_LEN];
	ushort	xlat=XLAT_NONE,msgattr=0;
	int 	i,j,x,file;
	long	l;
	ulong	length,offset,crc=0xffffffffUL;
	FILE	*instream;
	node_t	node;
	smbmsg_t msg;

sprintf(title,"%.*s",LEN_TITLE,subj);

if(useron.etoday>=level_emailperday[useron.level] && !SYSOP) {
	bputs(text[TooManyEmailsToday]);
	return; }

if(usernumber==1 && useron.rest&FLAG('S')
	&& (node_valuser!=1 || useron.fbacks || useron.emails)) { /* ! val fback */
    bprintf(text[R_Feedback],sys_op);
    return; }
if(usernumber!=1 && useron.rest&FLAG('E')
	&& (node_valuser!=usernumber || useron.fbacks || useron.emails)) {
    bputs(text[R_Email]);
    return; }
if(!usernumber) {
    bputs(text[UnknownUser]);
    return; }
getuserrec(usernumber,U_MISC,8,str);
l=ahtoul(str);
if(l&(DELETED|INACTIVE)) {              /* Deleted or Inactive User */
    bputs(text[UnknownUser]);
    return; }
if(l&NETMAIL && sys_misc&SM_FWDTONET
	&& yesno(text[ForwardMailQ])) { /* Forward to netmail address */
    getuserrec(usernumber,U_NETMAIL,LEN_NETMAIL,str);
	netmail(str,subj,mode);
    return; }
bprintf(text[Emailing],username(usernumber,tmp),usernumber);
action=NODE_SMAL;
nodesync();

sprintf(str,"%sFEEDBACK.BIN",exec_dir);
if(usernumber==1 && useron.fbacks && fexist(str)) {
	exec_bin("FEEDBACK",&main_csi);
	if(main_csi.logic!=LOGIC_TRUE)
		return; }

if(sys_misc&SM_ANON_EM && (SYSOP || useron.exempt&FLAG('A'))
    && !noyes(text[AnonymousQ]))
	msgattr|=MSG_ANONYMOUS;

if(sys_misc&SM_DELREADM)
	msgattr|=MSG_KILLREAD;

sprintf(msgpath,"%sINPUT.MSG",node_dir);
sprintf(str2,"%s #%u",username(usernumber,tmp),usernumber);
if(!writemsg(msgpath,top,title,mode,0,str2)) {
    bputs(text[Aborted]);
    return; }

if(mode&WM_FILE) {
	sprintf(str2,"%sFILE\\%04u.IN",data_dir,usernumber);
	mkdir(str2);
	sprintf(str2,"%sFILE\\%04u.IN\\%s",data_dir,usernumber,title);
	if(fexist(str2)) {
		bputs(text[FileAlreadyThere]);
		remove(msgpath);
		return; }
	if(online==ON_LOCAL) {		/* Local upload */
		bputs(text[EnterPath]);
		if(!getstr(str,60,K_LINE|K_UPPER)) {
			bputs(text[Aborted]);
			remove(msgpath);
			return; }
		backslash(str);
		strcat(str,title);
		mv(str,str2,1); }
	else { /* Remote */
		menu("ULPROT");
		mnemonics(text[ProtocolOrQuit]);
		strcpy(str,"Q");
		for(x=0;x<total_prots;x++)
			if(prot[x]->ulcmd[0] && chk_ar(prot[x]->ar,useron)) {
				sprintf(tmp,"%c",prot[x]->mnemonic);
				strcat(str,tmp); }
		ch=getkeys(str,0);
		if(ch=='Q' || sys_status&SS_ABORT) {
			bputs(text[Aborted]);
			remove(msgpath);
			return; }
		for(x=0;x<total_prots;x++)
			if(prot[x]->ulcmd[0] && prot[x]->mnemonic==ch
				&& chk_ar(prot[x]->ar,useron))
				break;
		if(x<total_prots)	/* This should be always */
			protocol(cmdstr(prot[x]->ulcmd,str2,nulstr,NULL),0); }
	l=flength(str2);
	if(l>0)
		bprintf(text[FileNBytesReceived],title,ultoac(l,tmp));
	else {
		bprintf(text[FileNotReceived],title);
		remove(msgpath);
		return; } }

bputs(text[WritingIndx]);

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
	smb.status.max_age=mail_maxage;
	smb.status.max_msgs=MAX_SYSMAIL;
	smb.status.attr=SMB_EMAIL;
	if((i=smb_create(&smb))!=0) {
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_CREATE,smb.file,i);
		return; } }

if((i=smb_locksmbhdr(&smb))!=0) {
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
	errormsg(WHERE,ERR_LOCK,smb.file,i);
	return; }

length=flength(msgpath)+2;	 /* +2 for translation string */

if(length&0xfff00000UL) {
	smb_unlocksmbhdr(&smb);
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
	errormsg(WHERE,ERR_LEN,msgpath,length);
    return; }

if((i=smb_open_da(&smb))!=0) {
	smb_unlocksmbhdr(&smb);
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
	smb_unlocksmbhdr(&smb);
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
	if(mail_maxcrcs) {
		for(i=0;i<j;i++)
			crc=ucrc32(buf[i],crc); }
	fwrite(buf,j,1,smb.sdt_fp);
	x=SDT_BLOCK_LEN; }
fflush(smb.sdt_fp);
fclose(instream);
crc=~crc;

memset(&msg,0,sizeof(smbmsg_t));
memcpy(msg.hdr.id,"SHD\x1a",4);
msg.hdr.version=smb_ver();
msg.hdr.attr=msg.idx.attr=msgattr;
if(mode&WM_FILE)
	msg.hdr.auxattr|=MSG_FILEATTACH;
msg.hdr.when_written.time=msg.hdr.when_imported.time=time(NULL);
msg.hdr.when_written.zone=msg.hdr.when_imported.zone=sys_timezone;

if(mail_maxcrcs) {
	i=smb_addcrc(&smb,crc);
	if(i) {
		smb_freemsgdat(&smb,offset,length,1);
		smb_unlocksmbhdr(&smb);
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		bputs("\1r\1h\1iDuplicate message!\r\n");
		return; } }

msg.hdr.offset=offset;

username(usernumber,str);
smb_hfield(&msg,RECIPIENT,strlen(str),str);

sprintf(str,"%u",usernumber);
smb_hfield(&msg,RECIPIENTEXT,strlen(str),str);
msg.idx.to=usernumber;

strcpy(str,useron.alias);
smb_hfield(&msg,SENDER,strlen(str),str);

sprintf(str,"%u",useron.number);
smb_hfield(&msg,SENDEREXT,strlen(str),str);
msg.idx.from=useron.number;

smb_hfield(&msg,SUBJECT,strlen(title),title);
strlwr(title);
msg.idx.subj=crc16(title);

smb_dfield(&msg,TEXT_BODY,length);

smb_unlocksmbhdr(&smb);
i=smb_addmsghdr(&smb,&msg,SMB_SELFPACK);
smb_close(&smb);
smb_stack(&smb,SMB_STACK_POP);

smb_freemsgmem(&msg);
if(i) {
	smb_freemsgdat(&smb,offset,length,1);
	errormsg(WHERE,ERR_WRITE,smb.file,i);
	return; }

if(usernumber==1) {
    useron.fbacks++;
    logon_fbacks++;
    putuserrec(useron.number,U_FBACKS,5,itoa(useron.fbacks,tmp,10)); }
else {
    useron.emails++;
    logon_emails++;
    putuserrec(useron.number,U_EMAILS,5,itoa(useron.emails,tmp,10)); }
useron.etoday++;
putuserrec(useron.number,U_ETODAY,5,itoa(useron.etoday,tmp,10));
bprintf(text[Emailed],username(usernumber,tmp),usernumber);
sprintf(str,"E-mailed %s #%d",username(usernumber,tmp),usernumber);
logline("E+",str);
if(mode&WM_FILE && online==ON_REMOTE)
	autohangup();
if(msgattr&MSG_ANONYMOUS)				/* Don't tell user if anonymous */
    return;
for(i=1;i<=sys_nodes;i++) { /* Tell user, if online */
    getnodedat(i,&node,0);
    if(node.useron==usernumber && !(node.misc&NODE_POFF)
        && (node.status==NODE_INUSE || node.status==NODE_QUIET)) {
        sprintf(str,text[EmailNodeMsg],node_num,useron.alias);
		putnmsg(i,str);
        break; } }
if(i>sys_nodes) {	/* User wasn't online, so leave short msg */
	sprintf(str,text[UserSentYouMail],useron.alias);
	putsmsg(usernumber,str); }
}
