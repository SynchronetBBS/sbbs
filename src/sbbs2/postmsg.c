#line 1 "POSTMSG.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

/****************************************************************************/
/* Posts a message on subboard number sub, with 'top' as top of message.    */
/* Returns 1 if posted, 0 if not.                                           */
/****************************************************************************/
char postmsg(uint subnum, smbmsg_t *remsg, int wm_mode)
{
	char	str[256],touser[256],title[LEN_TITLE+1],buf[SDT_BLOCK_LEN]
			,top[256],c;
	ushort	xlat,msgattr;
	int 	i,j,n,x,file,storage;
	ulong	l,length,offset,crc=0xffffffff;
	FILE	*instream;
	smbmsg_t msg,tmpmsg;

if(remsg) {
	sprintf(title,"%.*s",LEN_TITLE,remsg->subj);
	if(sub[subnum]->misc&SUB_INET)	// All Internet posts to "All" 05/20/97
		touser[0]=0;
	else if(remsg->hdr.attr&MSG_ANONYMOUS)
		strcpy(touser,text[Anonymous]);
	else
		strcpy(touser,remsg->from);
	msgattr=(remsg->hdr.attr&MSG_PRIVATE);
	sprintf(top,text[RegardingByTo],title,touser,remsg->to
		,timestr((time_t *)&remsg->hdr.when_written.time)
		,zonestr(remsg->hdr.when_written.zone)); }
else {
	title[0]=0;
	touser[0]=0;
	top[0]=0;
	msgattr=0; }

if(useron.rest&FLAG('P')) {
    bputs(text[R_Post]);
    return(0); }

if((sub[subnum]->misc&(SUB_QNET|SUB_FIDO|SUB_PNET|SUB_INET))
	&& (useron.rest&FLAG('N'))) {
    bputs(text[CantPostOnSub]);
    return(0); }

if(useron.ptoday>=level_postsperday[useron.level]) {
	bputs(text[TooManyPostsToday]);
	return(0); }

bprintf(text[Posting],grp[sub[subnum]->grp]->sname,sub[subnum]->lname);
action=NODE_PMSG;
nodesync();

if(!(msgattr&MSG_PRIVATE) && (sub[subnum]->misc&SUB_PONLY
	|| (sub[subnum]->misc&SUB_PRIV && !noyes(text[PrivatePostQ]))))
	msgattr|=MSG_PRIVATE;

if(sys_status&SS_ABORT)
	return(0);

if(!(sub[subnum]->misc&SUB_INET)	// Prompt for TO: user
	&& (sub[subnum]->misc&SUB_TOUSER || msgattr&MSG_PRIVATE || touser[0])) {
	if(!touser[0] && !(msgattr&MSG_PRIVATE))
		strcpy(touser,"All");
    bputs(text[PostTo]);
	i=LEN_ALIAS;
	if(sub[subnum]->misc&(SUB_PNET|SUB_INET))
		i=60;
	if(sub[subnum]->misc&SUB_FIDO)
		i=35;
	if(sub[subnum]->misc&SUB_QNET)
		i=25;
	getstr(touser,i,K_UPRLWR|K_LINE|K_EDIT|K_AUTODEL);
	if(stricmp(touser,"ALL")
	&& !(sub[subnum]->misc&(SUB_PNET|SUB_FIDO|SUB_QNET|SUB_INET|SUB_ANON))) {
		if(sub[subnum]->misc&SUB_NAME) {
			if(!userdatdupe(useron.number,U_NAME,LEN_NAME,touser,0)) {
				bputs(text[UnknownUser]);
				return(0); } }
		else {
			if((i=finduser(touser))==0)
				return(0);
			username(i,touser); } }
	if(sys_status&SS_ABORT)
		return(0); }

if(!touser[0])
	strcpy(touser,"All");       // Default to ALL

if(!stricmp(touser,"SYSOP") && !SYSOP)  // Change SYSOP to user #1
	username(1,touser);

if(msgattr&MSG_PRIVATE && !stricmp(touser,"ALL")) {
    bputs(text[NoToUser]);
    return(0); }
if(msgattr&MSG_PRIVATE)
	wm_mode|=WM_PRIVATE;

if(sub[subnum]->misc&SUB_AONLY
	|| (sub[subnum]->misc&SUB_ANON && useron.exempt&FLAG('A')
		&& !noyes(text[AnonymousQ])))
	msgattr|=MSG_ANONYMOUS;

if(sub[subnum]->mod_ar[0] && chk_ar(sub[subnum]->mod_ar,useron))
	msgattr|=MSG_MODERATED;

if(sub[subnum]->misc&SUB_SYSPERM && sub_op(subnum))
	msgattr|=MSG_PERMANENT;

if(msgattr&MSG_PRIVATE)
	bputs(text[PostingPrivately]);

if(msgattr&MSG_ANONYMOUS)
	bputs(text[PostingAnonymously]);

if(sub[subnum]->misc&SUB_NAME)
	bputs(text[UsingRealName]);

sprintf(str,"%sINPUT.MSG",node_dir);
if(!writemsg(str,top,title,wm_mode,subnum,touser)) {
    bputs(text[Aborted]);
    return(0); }

bputs(text[WritingIndx]);

if((i=smb_stack(&smb,SMB_STACK_PUSH))!=0) {
	errormsg(WHERE,ERR_OPEN,sub[subnum]->code,i);
	return(0); }

sprintf(smb.file,"%s%s",sub[subnum]->data_dir,sub[subnum]->code);
smb.retry_time=smb_retry_time;
if((i=smb_open(&smb))!=0) {
	smb_stack(&smb,SMB_STACK_POP);
	errormsg(WHERE,ERR_OPEN,smb.file,i);
	return(0); }

if(filelength(fileno(smb.shd_fp))<1) {	 /* Create it if it doesn't exist */
	smb.status.max_crcs=sub[subnum]->maxcrcs;
	smb.status.max_msgs=sub[subnum]->maxmsgs;
	smb.status.max_age=sub[subnum]->maxage;
	smb.status.attr=sub[subnum]->misc&SUB_HYPER ? SMB_HYPERALLOC : 0;
	if((i=smb_create(&smb))!=0) {
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_CREATE,smb.file,i);
		return(0); } }

if((i=smb_locksmbhdr(&smb))!=0) {
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
	errormsg(WHERE,ERR_LOCK,smb.file,i);
    return(0); }

if((i=smb_getstatus(&smb))!=0) {
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
	errormsg(WHERE,ERR_READ,smb.file,i);
    return(0); }

length=flength(str)+2;	 /* +2 for translation string */

if(length&0xfff00000UL) {
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
	errormsg(WHERE,ERR_LEN,str,length);
    return(0); }

if(smb.status.attr&SMB_HYPERALLOC) {
	offset=smb_hallocdat(&smb);
	storage=SMB_HYPERALLOC; }
else {
	if((i=smb_open_da(&smb))!=0) {
		smb_unlocksmbhdr(&smb);
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_OPEN,smb.file,i);
		return(0); }
	if(sub[subnum]->misc&SUB_FAST) {
		offset=smb_fallocdat(&smb,length,1);
		storage=SMB_FASTALLOC; }
	else {
		offset=smb_allocdat(&smb,length,1);
		storage=SMB_SELFPACK; }
	smb_close_da(&smb); }

if((file=open(str,O_RDONLY|O_BINARY))==-1
	|| (instream=fdopen(file,"rb"))==NULL) {
	smb_freemsgdat(&smb,offset,length,1);
	smb_unlocksmbhdr(&smb);
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
	errormsg(WHERE,ERR_OPEN,str,O_RDONLY|O_BINARY);
	return(0); }

setvbuf(instream,NULL,_IOFBF,2*1024);
fseek(smb.sdt_fp,offset,SEEK_SET);
xlat=XLAT_NONE;
fwrite(&xlat,2,1,smb.sdt_fp);
x=SDT_BLOCK_LEN-2;				/* Don't read/write more than 255 */
while(!feof(instream)) {
	memset(buf,0,x);
	j=fread(buf,1,x,instream);
	if((j!=x || feof(instream)) && buf[j-1]==LF && buf[j-2]==CR)
		buf[j-1]=buf[j-2]=0;	/* Convert to NULL */
	if(sub[subnum]->maxcrcs) {
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
msg.hdr.when_written.time=msg.hdr.when_imported.time=time(NULL);
msg.hdr.when_written.zone=msg.hdr.when_imported.zone=sys_timezone;
if(remsg) {
	msg.hdr.thread_orig=remsg->hdr.number;
	if(!remsg->hdr.thread_first) {
		remsg->hdr.thread_first=smb.status.last_msg+1;
		if((i=smb_lockmsghdr(&smb,remsg))!=0)
			errormsg(WHERE,ERR_LOCK,smb.file,i);
		else {
			i=smb_putmsghdr(&smb,remsg);
			smb_unlockmsghdr(&smb,remsg);
			if(i)
				errormsg(WHERE,ERR_WRITE,smb.file,i); } }
	else {
		l=remsg->hdr.thread_first;
		while(1) {
			tmpmsg.idx.offset=0;
			if(!loadmsg(&tmpmsg,l))
				break;
			if(tmpmsg.hdr.thread_next && tmpmsg.hdr.thread_next!=l) {
				l=tmpmsg.hdr.thread_next;
				smb_unlockmsghdr(&smb,&tmpmsg);
				smb_freemsgmem(&tmpmsg);
				continue; }
			tmpmsg.hdr.thread_next=smb.status.last_msg+1;
			if((i=smb_putmsghdr(&smb,&tmpmsg))!=0)
				errormsg(WHERE,ERR_WRITE,smb.file,i);
			smb_unlockmsghdr(&smb,&tmpmsg);
			smb_freemsgmem(&tmpmsg);
			break; } } }


if(sub[subnum]->maxcrcs) {
	i=smb_addcrc(&smb,crc);
	if(i) {
		smb_freemsgdat(&smb,offset,length,1);
		smb_unlocksmbhdr(&smb);
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		bputs("\1r\1h\1iDuplicate message!\r\n");
		return(0); } }

msg.hdr.offset=offset;

smb_hfield(&msg,RECIPIENT,strlen(touser),touser);
strlwr(touser);
msg.idx.to=crc16(touser);

strcpy(str,sub[subnum]->misc&SUB_NAME ? useron.name : useron.alias);
smb_hfield(&msg,SENDER,strlen(str),str);
strlwr(str);
msg.idx.from=crc16(str);

sprintf(str,"%u",useron.number);
smb_hfield(&msg,SENDEREXT,strlen(str),str);

smb_hfield(&msg,SUBJECT,strlen(title),title);
strcpy(str,title);
strlwr(str);
remove_re(str);
msg.idx.subj=crc16(str);

smb_dfield(&msg,TEXT_BODY,length);

smb_unlocksmbhdr(&smb);
i=smb_addmsghdr(&smb,&msg,storage);
smb_close(&smb);
smb_stack(&smb,SMB_STACK_POP);

smb_freemsgmem(&msg);
if(i) {
	smb_freemsgdat(&smb,offset,length,1);
	errormsg(WHERE,ERR_WRITE,smb.file,i);
	return(0); }

useron.ptoday++;
useron.posts++;
logon_posts++;
putuserrec(useron.number,U_POSTS,5,itoa(useron.posts,str,10));
putuserrec(useron.number,U_PTODAY,5,itoa(useron.ptoday,str,10));
bprintf(text[Posted],grp[sub[subnum]->grp]->sname
    ,sub[subnum]->lname);
sprintf(str,"Posted on %s %s",grp[sub[subnum]->grp]->sname,sub[subnum]->lname);
logline("P+",str);
if(sub[subnum]->misc&SUB_FIDO && sub[subnum]->echomail_sem[0]) /* semaphore */
	if((file=nopen(sub[subnum]->echomail_sem,O_WRONLY|O_CREAT|O_TRUNC))!=-1)
        close(file);
return(1);
}
