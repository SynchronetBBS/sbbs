#line 1 "NETMAIL.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "qwk.h"

/****************************************************************************/
/****************************************************************************/
void inetmail(char *into, char *subj, char mode)
{
	char	str[256],str2[256],msgpath[256],title[256],name[256],ch
			,buf[SDT_BLOCK_LEN],*p,addr[256];
	ushort	xlat=XLAT_NONE,net=NET_INTERNET;
	int 	i,j,x,file;
	long	l;
	ulong	length,offset;
	FILE	*instream;
	node_t	node;
	smbmsg_t msg;

strcpy(name,into);
strcpy(addr,into);
strcpy(title,subj);

if((!SYSOP && !(inetmail_misc&NMAIL_ALLOW)) || useron.rest&FLAG('M')) {
    bputs(text[NoNetMailAllowed]);
    return; }

if(inetmail_cost && !(useron.exempt&FLAG('S'))) {
	if(useron.cdt+useron.freecdt<inetmail_cost) {
		bputs(text[NotEnoughCredits]);
		return; }
	sprintf(str,text[NetMailCostContinueQ],inetmail_cost);
	if(noyes(str))
		return; }

p=strrchr(name,'@');
if(!p)
	p=strrchr(name,'!');
if(p) {
	*p=0;
	truncsp(name); }
bprintf(text[NetMailing],name,addr
	,inetmail_misc&NMAIL_ALIAS ? useron.alias : useron.name
	,sys_inetaddr);
action=NODE_SMAL;
nodesync();

sprintf(msgpath,"%sNETMAIL.MSG",node_dir);
if(!writemsg(msgpath,nulstr,title,mode,0,into)) {
    bputs(text[Aborted]);
    return; }

if(mode&WM_FILE) {
	sprintf(str2,"%sFILE\\%04u.OUT",data_dir,useron.number);
	mkdir(str2);
	sprintf(str2,"%sFILE\\%04u.OUT\\%s",data_dir,useron.number,title);
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
	fwrite(buf,j,1,smb.sdt_fp);
	x=SDT_BLOCK_LEN; }
fflush(smb.sdt_fp);
fclose(instream);

memset(&msg,0,sizeof(smbmsg_t));
memcpy(msg.hdr.id,"SHD\x1a",4);
msg.hdr.version=smb_ver();
if(mode&WM_FILE)
	msg.hdr.auxattr|=MSG_FILEATTACH;
msg.hdr.when_written.time=msg.hdr.when_imported.time=time(NULL);
msg.hdr.when_written.zone=msg.hdr.when_imported.zone=sys_timezone;

msg.hdr.offset=offset;

net=NET_INTERNET;
smb_hfield(&msg,RECIPIENT,strlen(name),name);
msg.idx.to=0;	/* Out-bound NetMail set to 0 */
smb_hfield(&msg,RECIPIENTNETTYPE,sizeof(net),&net);
smb_hfield(&msg,RECIPIENTNETADDR,strlen(addr),addr);

strcpy(str,inetmail_misc&NMAIL_ALIAS ? useron.alias : useron.name);
smb_hfield(&msg,SENDER,strlen(str),str);

sprintf(str,"%u",useron.number);
smb_hfield(&msg,SENDEREXT,strlen(str),str);
msg.idx.from=useron.number;

/*
smb_hfield(&msg,SENDERNETTYPE,sizeof(net),&net);
smb_hfield(&msg,SENDERNETADDR,strlen(sys_inetaddr),sys_inetaddr);
*/

smb_hfield(&msg,SUBJECT,strlen(title),title);
strcpy(str,title);
strlwr(str);
msg.idx.subj=crc16(str);

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

if(mode&WM_FILE && online==ON_REMOTE)
	autohangup();

if(inetmail_sem[0]) 	 /* update semaphore file */
	if((file=nopen(inetmail_sem,O_WRONLY|O_CREAT|O_TRUNC))!=-1)
		close(file);
if(!(useron.exempt&FLAG('S')))
	subtract_cdt(inetmail_cost);
sprintf(str,"Sent Internet Mail to %s (%s)",name,addr);
logline("EN",str);
}

void qnetmail(char *into, char *subj, char mode)
{
	char	str[256],str2[128],msgpath[128],title[128],to[128],fulladdr[128],ch
			,buf[SDT_BLOCK_LEN],*addr,*p;
	ushort	xlat=XLAT_NONE,net=NET_QWK,touser;
	int 	i,j,x,file;
	long	l;
	ulong	length,offset;
	FILE	*instream;
	node_t	node;
	smbmsg_t msg;

strcpy(to,into);
strcpy(title,subj);

if(useron.rest&FLAG('M')) {
    bputs(text[NoNetMailAllowed]);
    return; }

addr=strrchr(to,'@');
if(!addr) {
	bputs("Invalid netmail address\r\n");
	return; }
*addr=0;
addr++;
strupr(addr);
truncsp(addr);
touser=qwk_route(addr,fulladdr);
if(!fulladdr[0]) {
	bputs("Invalid netmail address\r\n");
	return; }

truncsp(to);
if(!stricmp(to,"SBBS") && !SYSOP) {
	bputs("Invalid netmail address\r\n");
    return; }
bprintf(text[NetMailing],to,fulladdr
	,useron.alias,sys_id);
action=NODE_SMAL;
nodesync();

sprintf(msgpath,"%sNETMAIL.MSG",node_dir);
if(!writemsg(msgpath,nulstr,title,mode|WM_QWKNET,0,to)) {
    bputs(text[Aborted]);
    return; }

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
	fwrite(buf,j,1,smb.sdt_fp);
	x=SDT_BLOCK_LEN; }
fflush(smb.sdt_fp);
fclose(instream);

memset(&msg,0,sizeof(smbmsg_t));
memcpy(msg.hdr.id,"SHD\x1a",4);
msg.hdr.version=smb_ver();
if(mode&WM_FILE)
	msg.hdr.auxattr|=MSG_FILEATTACH;
msg.hdr.when_written.time=msg.hdr.when_imported.time=time(NULL);
msg.hdr.when_written.zone=msg.hdr.when_imported.zone=sys_timezone;

msg.hdr.offset=offset;

net=NET_QWK;
smb_hfield(&msg,RECIPIENT,strlen(to),to);
msg.idx.to=touser;
smb_hfield(&msg,RECIPIENTNETTYPE,sizeof(net),&net);
smb_hfield(&msg,RECIPIENTNETADDR,strlen(fulladdr),fulladdr);

smb_hfield(&msg,SENDER,strlen(useron.alias),useron.alias);

sprintf(str,"%u",useron.number);
smb_hfield(&msg,SENDEREXT,strlen(str),str);
msg.idx.from=useron.number;

smb_hfield(&msg,SUBJECT,strlen(title),title);
strcpy(str,title);
strlwr(str);
msg.idx.subj=crc16(str);

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

sprintf(str,"Sent QWK NetMail to %s (%s)",to,fulladdr);
logline("EN",str);
}
