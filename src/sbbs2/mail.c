#line 1 "MAIL.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

/****************************************************************************/
/* Returns the number of pieces of mail waiting for usernumber              */
/* If sent is non-zero, it returns the number of mail sent by usernumber    */
/* If usernumber is 0, it returns all mail on the system                    */
/****************************************************************************/
int getmail(int usernumber, char sent)
{
    char    str[128];
    int     i=0;
    long    l;
    idxrec_t idx;

smb_stack(&smb,SMB_STACK_PUSH);
sprintf(smb.file,"%sMAIL",data_dir);
smb.retry_time=smb_retry_time;
sprintf(str,"%s.SID",smb.file);
l=flength(str);
if(l<sizeof(idxrec_t))
	return(0);
if(!usernumber) {
	smb_stack(&smb,SMB_STACK_POP);
	return(l/sizeof(idxrec_t)); }	/* Total system e-mail */
if(smb_open(&smb)) {
	smb_stack(&smb,SMB_STACK_POP);
	return(0); }
while(!feof(smb.sid_fp)) {
	if(!fread(&idx,sizeof(idxrec_t),1,smb.sid_fp))
        break;
	if(idx.attr&MSG_DELETE)
		continue;
    if((!sent && idx.to==usernumber)
     || (sent && idx.from==usernumber))
        i++; }
smb_close(&smb);
smb_stack(&smb,SMB_STACK_POP);
return(i);
}

/***************************/
/* Delete file attachments */
/***************************/
void delfattach(uint to, char *title)
{
    char str[128],str2[128],*tp,*sp,*p;
    uint i;

strcpy(str,title);
tp=str;
while(1) {
    p=strchr(tp,SP);
    if(p) *p=0;
    sp=strrchr(tp,'/');              /* sp is slash pointer */
    if(!sp) sp=strrchr(tp,'\\');
    if(sp) tp=sp+1;
    sprintf(str2,"%sFILE\\%04u.IN\\%s"  /* str2 is path/fname */
        ,data_dir,to,tp);
    remove(str2);
    if(!p)
        break;
    tp=p+1; }
sprintf(str,"%sFILE\\%04u.IN",data_dir,to);
rmdir(str);                     /* remove the dir if it's empty */
}


/****************************************************************************/
/* Deletes all mail messages for usernumber that have been marked 'deleted' */
/* smb_locksmbhdr() should be called prior to this function 				*/
/****************************************************************************/
int delmail(uint usernumber, int which)
{
	ulong	 i,l,now;
	idxrec_t HUGE16 *idxbuf;
	smbmsg_t msg;

now=time(NULL);
if((i=smb_getstatus(&smb))!=0) {
	errormsg(WHERE,ERR_READ,smb.file,i);
	return(2); }
if(!smb.status.total_msgs)
	return(0);
if((idxbuf=(idxrec_t *)LMALLOC(smb.status.total_msgs*sizeof(idxrec_t)))==NULL) {
	errormsg(WHERE,ERR_ALLOC,smb.file,smb.status.total_msgs*sizeof(idxrec_t));
	return(-1); }
if((i=smb_open_da(&smb))!=0) {
    errormsg(WHERE,ERR_OPEN,smb.file,i);
	LFREE(idxbuf);
    return(i); }
if((i=smb_open_ha(&smb))!=0) {
    smb_close_da(&smb);
    errormsg(WHERE,ERR_OPEN,smb.file,i);
	LFREE(idxbuf);
    return(i); }
rewind(smb.sid_fp);
for(l=0;l<smb.status.total_msgs;) {
	if(!fread(&msg.idx,sizeof(idxrec_t),1,smb.sid_fp))
        break;
	if(which==MAIL_ALL && !(msg.idx.attr&MSG_PERMANENT)
		&& smb.status.max_age && now>msg.idx.time
		&& (now-msg.idx.time)/(24L*60L*60L)>smb.status.max_age)
		msg.idx.attr|=MSG_DELETE;
	if(msg.idx.attr&MSG_DELETE && !(msg.idx.attr&MSG_PERMANENT)
		&& ((which==MAIL_SENT && usernumber==msg.idx.from)
		|| (which==MAIL_YOUR && usernumber==msg.idx.to)
		|| (which==MAIL_ANY
			&& (usernumber==msg.idx.to || usernumber==msg.idx.from))
		|| which==MAIL_ALL)) {
		/* Don't need to lock message because base is locked */
		if(which==MAIL_ALL && !online)
			lprintf(" #%lu",msg.idx.number);
		if((i=smb_getmsghdr(&smb,&msg))!=0)
			errormsg(WHERE,ERR_READ,smb.file,i);
		else {
			if(msg.hdr.attr!=msg.idx.attr) {
				msg.hdr.attr=msg.idx.attr;
				if((i=smb_putmsghdr(&smb,&msg))!=0)
					errormsg(WHERE,ERR_WRITE,smb.file,i); }
			if((i=smb_freemsg(&smb,&msg))!=0)
				errormsg(WHERE,ERR_REMOVE,smb.file,i);
			if(msg.hdr.auxattr&MSG_FILEATTACH)
				delfattach(msg.idx.to,msg.subj);
			smb_freemsgmem(&msg); }
		continue; }
	idxbuf[l]=msg.idx;
	l++; }
rewind(smb.sid_fp);
chsize(fileno(smb.sid_fp),0);
for(i=0;i<l;i++)
	fwrite(&idxbuf[i],sizeof(idxrec_t),1,smb.sid_fp);
LFREE(idxbuf);
smb.status.total_msgs=l;
smb_putstatus(&smb);
fflush(smb.sid_fp);
smb_close_ha(&smb);
smb_close_da(&smb);
return(0);
}


/***********************************************/
/* Tell the user that so-and-so read your mail */
/***********************************************/
void telluser(smbmsg_t msg)
{
	char str[256],*p;
	uint usernumber,n;
	node_t node;

if(msg.from_net.type)
	return;
if(msg.from_ext)
	usernumber=atoi(msg.from_ext);
else {
	usernumber=matchuser(msg.from);
	if(!usernumber)
		return; }
for(n=1;n<=sys_nodes;n++) { /* Tell user */
	getnodedat(n,&node,0);
	if(node.useron==usernumber
	&& (node.status==NODE_INUSE
	|| node.status==NODE_QUIET)) {
		sprintf(str
			,text[UserReadYourMailNodeMsg]
			,node_num,useron.alias);
		putnmsg(n,str);
		break; } }
if(n>sys_nodes) {
	now=time(NULL);
	sprintf(str,text[UserReadYourMail]
		,useron.alias,timestr(&now));
	putsmsg(usernumber,str); }
}

/****************************************************************************/
/* Loads mail waiting for user number 'usernumber' into the mail array of   */
/* of pointers to mail_t (message numbers and attributes)                   */
/* smb_open(&smb) must be called prior											*/
/****************************************************************************/
ulong loadmail(mail_t **mail, uint usernumber, int which, int mode)
{
	int 		i;
	ulong		l=0;
    idxrec_t    idx;

if((i=smb_locksmbhdr(&smb))!=0) {				/* Be sure noone deletes or */
	errormsg(WHERE,ERR_LOCK,smb.file,i);		/* adds while we're reading */
    return(0); }
(*mail)=NULL;
rewind(smb.sid_fp);
while(!feof(smb.sid_fp)) {
	if(!fread(&idx,sizeof(idxrec_t),1,smb.sid_fp))
        break;
	if((which==MAIL_SENT && idx.from!=usernumber)
		|| (which==MAIL_YOUR && idx.to!=usernumber)
		|| (which==MAIL_ANY && idx.from!=usernumber && idx.to!=usernumber))
        continue;
	if(idx.attr&MSG_DELETE) {
		if(mode&LM_QWK) 				/* Don't included deleted msgs */
			continue;					/* in QWK packet */
		if(!(sys_misc&SM_SYSVDELM)) 	/* Noone can view deleted msgs */
			continue;					
		if(!SYSOP						/* not sysop */
			&& !(sys_misc&SM_USRVDELM)) /* users can't view deleted msgs */
			continue; }
	if(mode&LM_UNREAD && idx.attr&MSG_READ)
		continue;
	if(((*mail)=(mail_t *)REALLOC((*mail),sizeof(mail_t)*(l+1)))
        ==NULL) {
		smb_unlocksmbhdr(&smb);
		errormsg(WHERE,ERR_ALLOC,smb.file,sizeof(mail_t)*(l+1));
        return(0); }
	(*mail)[l].offset=idx.offset;
	(*mail)[l].number=idx.number;
	(*mail)[l].to=idx.to;
	(*mail)[l].from=idx.from;
	(*mail)[l].subj=idx.subj;
	(*mail)[l].time=idx.time;
	(*mail)[l].attr=idx.attr;
	l++; }
smb_unlocksmbhdr(&smb);
return(l);
}

/************************************************************************/
/* Deletes all mail waiting for user number 'usernumber'                */
/************************************************************************/
void delallmail(uint usernumber)
{
	int 	i;
	ulong	l,msgs,deleted=0;
	mail_t	*mail;
	smbmsg_t msg;

if((i=smb_stack(&smb,SMB_STACK_PUSH))!=0) {
	errormsg(WHERE,ERR_OPEN,"MAIL",i);
	return; }
sprintf(smb.file,"%sMAIL",data_dir);
smb.retry_time=smb_retry_time;
if((i=smb_open(&smb))!=0) {
	errormsg(WHERE,ERR_OPEN,smb.file,i);
	smb_stack(&smb,SMB_STACK_POP);
	return; }

msgs=loadmail(&mail,usernumber,MAIL_ANY,0);
if(!msgs) {
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
	return; }
if((i=smb_locksmbhdr(&smb))!=0) {			/* Lock the base, so nobody */
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
	FREE(mail);
	errormsg(WHERE,ERR_LOCK,smb.file,i);	/* messes with the index */
	return; }
for(l=0;l<msgs;l++) {
	msg.idx.offset=0;						/* search by number */
	if(loadmsg(&msg,mail[l].number)) {	   /* message still there */
		msg.hdr.attr|=MSG_DELETE;
		msg.hdr.attr&=~MSG_PERMANENT;
		msg.idx.attr=msg.hdr.attr;
		if((i=smb_putmsg(&smb,&msg))!=0)
			errormsg(WHERE,ERR_WRITE,smb.file,i);
		else
			deleted++;
		smb_freemsgmem(&msg);
		smb_unlockmsghdr(&smb,&msg); } }

if(msgs)
	FREE(mail);
if(deleted && sys_misc&SM_DELEMAIL)
	delmail(usernumber,MAIL_ANY);
smb_unlocksmbhdr(&smb);
smb_close(&smb);
smb_stack(&smb,SMB_STACK_POP);
}

/****************************************************************************/
/* Reads mail waiting for usernumber.                                       */
/****************************************************************************/
void readmail(uint usernumber, int which)
{
	char	str[256],str2[256],str3[256],done=0,domsg=1
			,*buf,*p,*tp,*sp,ch;
	int 	file,msgs,curmsg,i,j,k,n,m,mismatches=0,act;
    long    length,l;
	ulong	last;
	file_t	fd;
	mail_t	*mail;
	smbmsg_t msg;

msg.total_hfields=0;			/* init to NULL, cause not allocated yet */

fd.dir=total_dirs+1;			/* temp dir for file attachments */

if((i=smb_stack(&smb,SMB_STACK_PUSH))!=0) {
	errormsg(WHERE,ERR_OPEN,"MAIL",i);
	return; }
sprintf(smb.file,"%sMAIL",data_dir);
smb.retry_time=smb_retry_time;
if((i=smb_open(&smb))!=0) {
	smb_stack(&smb,SMB_STACK_POP);
	errormsg(WHERE,ERR_OPEN,smb.file,i);
	return; }

msgs=loadmail(&mail,usernumber,which,0);
if(!msgs) {
	if(which==MAIL_SENT)
		bputs(text[NoMailSent]);
	else if(which==MAIL_ALL)
		bputs(text[NoMailOnSystem]);
	else
		bputs(text[NoMailWaiting]);
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
    return; }

last=smb.status.last_msg;

if(which==MAIL_SENT)
	act=NODE_RSML;
else if(which==MAIL_ALL)
	act=NODE_SYSP;
else
	act=NODE_RMAL;
action=act;
if(msgs>1 && which!=MAIL_ALL) {
	if(which==MAIL_SENT)
		bputs(text[MailSentLstHdr]);
	else
		bputs(text[MailWaitingLstHdr]);

	for(curmsg=0;curmsg<msgs && !msgabort();curmsg++) {
		if(msg.total_hfields)
			smb_freemsgmem(&msg);
		msg.total_hfields=0;
		msg.idx.offset=mail[curmsg].offset;
		if(!loadmsg(&msg,mail[curmsg].number))
			continue;
		smb_unlockmsghdr(&smb,&msg);
		bprintf(text[MailWaitingLstFmt],curmsg+1
			,which==MAIL_SENT ? msg.to
			: (msg.hdr.attr&MSG_ANONYMOUS) && !SYSOP ? text[Anonymous]
			: msg.from
			,msg.hdr.attr&MSG_DELETE ? '-' : msg.hdr.attr&MSG_READ ? ' '
				: msg.from_net.type || msg.to_net.type ? 'N':'*'
			,msg.subj);
		smb_freemsgmem(&msg);
		msg.total_hfields=0; }

	ASYNC;
	if(!(sys_status&SS_ABORT)) {
		bprintf(text[StartWithN],1L);
		if((curmsg=getnum(msgs))>0)
			curmsg--;
		else if(curmsg==-1) {
			FREE(mail);
			smb_close(&smb);
			smb_stack(&smb,SMB_STACK_POP);
			return; } }
	sys_status&=~SS_ABORT; }
else {
	curmsg=0;
	if(which==MAIL_ALL)
		domsg=0; }
if(which==MAIL_SENT)
	logline("E","Read sent mail");
else if(which==MAIL_ALL)
	logline("S+","Read all mail");
else
	logline("E","Read mail");
if(useron.misc&RIP) {
	strcpy(str,which==MAIL_YOUR ? "MAILREAD" : which==MAIL_ALL ?
		"ALLMAIL" : "SENTMAIL");
	menu(str); }
while(online && !done) {
	action=act;

	if(msg.total_hfields)
		smb_freemsgmem(&msg);
	msg.total_hfields=0;

	msg.idx.offset=mail[curmsg].offset;
	msg.idx.number=mail[curmsg].number;
	msg.idx.to=mail[curmsg].to;
	msg.idx.from=mail[curmsg].from;
	msg.idx.subj=mail[curmsg].subj;

	if((i=smb_locksmbhdr(&smb))!=0) {
		errormsg(WHERE,ERR_LOCK,smb.file,i);
		break; }

	if((i=smb_getstatus(&smb))!=0) {
		smb_unlocksmbhdr(&smb);
		errormsg(WHERE,ERR_READ,smb.file,i);
		break; }
	smb_unlocksmbhdr(&smb);

	if(smb.status.last_msg!=last) { 	/* New messages */
		last=smb.status.last_msg;
		FREE(mail);
		msgs=loadmail(&mail,usernumber,which,0);   /* So re-load */
		if(!msgs)
			break;
		for(curmsg=0;curmsg<msgs;curmsg++)
			if(mail[curmsg].number==msg.idx.number)
				break;
		if(curmsg>=msgs)
			curmsg=(msgs-1);
		continue; }

	if(!loadmsg(&msg,mail[curmsg].number)) {	/* Message header gone */
		if(mismatches>5) {	/* We can't do this too many times in a row */
			errormsg(WHERE,ERR_CHK,"message number",mail[curmsg].number);
			break; }
		FREE(mail);
		msgs=loadmail(&mail,usernumber,which,0);
		if(!msgs)
			break;
		if(curmsg>(msgs-1))
			curmsg=(msgs-1);
		mismatches++;
		continue; }
	smb_unlockmsghdr(&smb,&msg);
	msg.idx.attr=msg.hdr.attr;

	mismatches=0;

	if(domsg && !(sys_status&SS_ABORT)) {

		show_msg(msg
			,msg.from_ext && msg.idx.from==1 && !msg.from_net.type
				? 0:P_NOATCODES);

		if(msg.hdr.auxattr&MSG_FILEATTACH) {  /* Attached file */
			smb_getmsgidx(&smb,&msg);
			strcpy(str,msg.subj);					/* filenames in title */
			strupr(str);
			tp=str;
			while(online) {
				p=strchr(tp,SP);
				if(p) *p=0;
				sp=strrchr(tp,'/');              /* sp is slash pointer */
				if(!sp) sp=strrchr(tp,'\\');
                if(sp) tp=sp+1;
				padfname(tp,fd.name);
				sprintf(str2,"%sFILE\\%04u.IN\\%s"  /* str2 is path/fname */
					,data_dir,msg.idx.to,tp);
				length=flength(str2);
				if(length<1)
					bputs(text[FileNotFound]);
				else if(!(useron.exempt&FLAG('T')) && cur_cps && !SYSOP
					&& length/(ulong)cur_cps>timeleft)
					bputs(text[NotEnoughTimeToDl]);
				else {
					sprintf(str3,text[DownloadAttachedFileQ]
						,tp,ultoac(length,tmp));
					if(length>0L && yesno(str3)) {
						if(online==ON_LOCAL) {
							bputs(text[EnterPath]);
							if(getstr(str3,60,K_LINE|K_UPPER)) {
								backslashcolon(str3);
								sprintf(tmp,"%s%s",str3,tp);
								if(!mv(str2,tmp,which!=MAIL_YOUR)) {
									logon_dlb+=length;
									logon_dls++;
									useron.dls=adjustuserrec(useron.number
										,U_DLS,5,1);
									useron.dlb=adjustuserrec(useron.number
										,U_DLB,10,length);
									bprintf(text[FileNBytesSent]
										,fd.name,ultoac(length,tmp)); } } }

						else {	/* Remote User */
							menu("DLPROT");
							mnemonics(text[ProtocolOrQuit]);
							strcpy(str3,"Q");
							for(i=0;i<total_prots;i++)
								if(prot[i]->dlcmd[0]
									&& chk_ar(prot[i]->ar,useron)) {
									sprintf(tmp,"%c",prot[i]->mnemonic);
									strcat(str3,tmp); }
							ch=getkeys(str3,0);
							for(i=0;i<total_prots;i++)
								if(prot[i]->dlcmd[0] && ch==prot[i]->mnemonic
									&& chk_ar(prot[i]->ar,useron))
									break;
							if(i<total_prots) {
								j=protocol(cmdstr(prot[i]->dlcmd,str2,nulstr
									,NULL),0);
								if((prot[i]->misc&PROT_DSZLOG
									&& checkprotlog(fd))
									|| (!(prot[i]->misc&PROT_DSZLOG) && !j)) {
										if(which==MAIL_YOUR)
											remove(str2);
										logon_dlb+=length;	/* Update stats */
										logon_dls++;
										useron.dls=adjustuserrec(useron.number
											,U_DLS,5,1);
										useron.dlb=adjustuserrec(useron.number
											,U_DLB,10,length);
										bprintf(text[FileNBytesSent]
											,fd.name,ultoac(length,tmp));
										sprintf(str3
											,"Downloaded attached file: %s"
											,fd.name);
										logline("D-",str3); }
								autohangup(); } } } }
					if(!p)
						break;
					tp=p+1;
					while(*tp==SP) tp++; }
			sprintf(str,"%sFILE\\%04u.IN",data_dir,usernumber);
			rmdir(str); }
		if(which==MAIL_YOUR && !(msg.hdr.attr&MSG_READ)) {
			mail[curmsg].attr|=MSG_READ;
			if(thisnode.status==NODE_INUSE)
				telluser(msg);
			if(msg.total_hfields)
				smb_freemsgmem(&msg);
			msg.total_hfields=0;
			msg.idx.offset=0;						/* Search by number */
			if(!smb_locksmbhdr(&smb)) { 			/* Lock the entire base */
				if(loadmsg(&msg,msg.idx.number)) {
					msg.hdr.attr|=MSG_READ;
					msg.idx.attr=msg.hdr.attr;
					if((i=smb_putmsg(&smb,&msg))!=0)
						errormsg(WHERE,ERR_WRITE,smb.file,i);
					smb_unlockmsghdr(&smb,&msg); }
				smb_unlocksmbhdr(&smb); }
			if(!msg.total_hfields) {				/* unsuccessful reload */
				domsg=0;
				continue; } }
		}
    else domsg=1;

	if(useron.misc&WIP) {
		strcpy(str,which==MAIL_YOUR ? "MAILREAD" : which==MAIL_ALL ?
			"ALLMAIL" : "SENTMAIL");
		menu(str); }

	ASYNC;
	if(which==MAIL_SENT)
		bprintf(text[ReadingSentMail],curmsg+1,msgs);
	else if(which==MAIL_ALL)
		bprintf(text[ReadingAllMail],curmsg+1,msgs);
	else
		bprintf(text[ReadingMail],curmsg+1,msgs);
	sprintf(str,"ADFLNQRT?<>[]{}-+");
	if(SYSOP)
		strcat(str,"CUSP");
	if(which!=MAIL_YOUR)
		strcat(str,"E");
	l=getkeys(str,msgs);
	if(l&0x80000000L) {
		if(l==-1)	/* ctrl-c */
            break;
		curmsg=(l&~0x80000000L)-1;
        continue; }
	switch(l) {
		case 'A':   /* Auto-reply to last piece */
			if(which==MAIL_SENT)
				break;
			if((msg.hdr.attr&MSG_ANONYMOUS) && !SYSOP) {
				bputs(text[CantReplyToAnonMsg]);
				break; }

			quotemsg(msg,1);

			if(msg.from_net.type==NET_FIDO) 		/* FidoNet type */
				sprintf(str,"%s @%s",msg.from
				,faddrtoa(*(faddr_t *)msg.from_net.addr));
			else if(msg.from_net.type==NET_INTERNET)
				strcpy(str,msg.from_net.addr);
			else if(msg.from_net.type)
				sprintf(str,"%s@%s",msg.from,msg.from_net.addr);
			else									/* No net */
				strcpy(str,msg.from);

			strcpy(str2,str);

			bputs(text[Email]);
			if(!getstr(str,64,K_EDIT|K_AUTODEL))
				break;
			msg.hdr.number=msg.idx.number;
			smb_getmsgidx(&smb,&msg);

			if(!stricmp(str2,str))		/* Reply to sender */
				sprintf(str2,text[Regarding],msg.subj);
			else						/* Reply to other */
				sprintf(str2,text[RegardingBy],msg.subj,msg.from
					,timestr((time_t *)&msg.hdr.when_written.time));

			p=strrchr(str,'@');
			if(p) { 							/* name @addr */
				netmail(str,msg.subj,WM_QUOTE);
				sprintf(str2,text[DeleteMailQ],msg.from); }
			else {
				if(!msg.from_net.type && !stricmp(str,msg.from))
					email(msg.idx.from,str2,msg.subj,WM_EMAIL|WM_QUOTE);
				else if(!stricmp(str,"SYSOP"))
					email(1,str2,msg.subj,WM_EMAIL|WM_QUOTE);
				else if((i=finduser(str))!=0)
					email(i,str2,msg.subj,WM_EMAIL|WM_QUOTE);
				sprintf(str2,text[DeleteMailQ],msg.from); }
			if(!yesno(str2)) {
				if(curmsg<msgs-1) curmsg++;
				else done=1;
				break;	}
			/* Case 'D': must follow! */
		case 'D':   /* Delete last piece (toggle) */
			if(msg.hdr.attr&MSG_PERMANENT) {
				bputs("\r\nPermanent message.\r\n");
				domsg=0;
				break; }
			if(msg.total_hfields)
				smb_freemsgmem(&msg);
			msg.total_hfields=0;
			msg.idx.offset=0;
			if(loadmsg(&msg,msg.idx.number)) {
				msg.hdr.attr^=MSG_DELETE;
				msg.idx.attr=msg.hdr.attr;
//				  mail[curmsg].attr=msg.hdr.attr;
				if((i=smb_putmsg(&smb,&msg))!=0)
					errormsg(WHERE,ERR_WRITE,smb.file,i);
				smb_unlockmsghdr(&smb,&msg); }
			if(curmsg<msgs-1) curmsg++;
			else done=1;
			break;
		case 'F':  /* Forward last piece */
			domsg=0;
			bputs(text[ForwardMailTo]);
			if(!getstr(str,LEN_ALIAS,K_UPRLWR))
				break;
			i=finduser(str);
			if(!i)
				break;
			domsg=1;
			if(curmsg<msgs-1) curmsg++;
			else done=1;
			smb_getmsgidx(&smb,&msg);
			forwardmail(&msg,i);
			if(msg.hdr.attr&MSG_PERMANENT)
				break;
			sprintf(str2,text[DeleteMailQ],msg.from);
			if(!yesno(str2))
				break;
			if(msg.total_hfields)
				smb_freemsgmem(&msg);
			msg.total_hfields=0;
			msg.idx.offset=0;
			if(loadmsg(&msg,msg.idx.number)) {
				msg.hdr.attr|=MSG_DELETE;
				msg.idx.attr=msg.hdr.attr;
//				  mail[curmsg].attr=msg.hdr.attr;
				if((i=smb_putmsg(&smb,&msg))!=0)
					errormsg(WHERE,ERR_WRITE,smb.file,i);
				smb_unlockmsghdr(&smb,&msg); }

			break;
		case 'L':     /* List mail */
			domsg=0;
			bprintf(text[StartWithN],(long)curmsg+1);
			if((i=getnum(msgs))>0)
				i--;
			else if(i==-1)
				break;
			else
				i=curmsg;
			if(which==MAIL_SENT)
				bputs(text[MailSentLstHdr]);
			else if(which==MAIL_ALL)
				bputs(text[MailOnSystemLstHdr]);
			else
				bputs(text[MailWaitingLstHdr]);
			for(;i<msgs && !msgabort();i++) {
				if(msg.total_hfields)
					smb_freemsgmem(&msg);
				msg.total_hfields=0;
				msg.idx.offset=mail[i].offset;
				if(!loadmsg(&msg,mail[i].number))
					continue;
				smb_unlockmsghdr(&smb,&msg);
				if(which==MAIL_ALL)
					bprintf(text[MailOnSystemLstFmt]
						,i+1,msg.from,msg.to
						,msg.hdr.attr&MSG_DELETE ? '-'
							: msg.hdr.attr&MSG_READ ? SP
							: msg.from_net.type || msg.to_net.type ? 'N':'*'
						,msg.subj);
				else
					bprintf(text[MailWaitingLstFmt],i+1
						,which==MAIL_SENT ? msg.to
						: (msg.hdr.attr&MSG_ANONYMOUS) && !SYSOP
						? text[Anonymous] : msg.from
						,msg.hdr.attr&MSG_DELETE ? '-'
							: msg.hdr.attr&MSG_READ ? SP
							: msg.from_net.type || msg.to_net.type ? 'N':'*'
						,msg.subj);
				smb_freemsgmem(&msg);
				msg.total_hfields=0; }
			break;
		case 'Q':
			done=1;
			break;
		case 'C':   /* Change attributes of last piece */
			i=chmsgattr(msg.hdr.attr);
			if(msg.hdr.attr==i)
				break;
			if(msg.total_hfields)
				smb_freemsgmem(&msg);
			msg.total_hfields=0;
			msg.idx.offset=0;
			if(loadmsg(&msg,msg.idx.number)) {
				msg.hdr.attr=msg.idx.attr=i;
				if((i=smb_putmsg(&smb,&msg))!=0)
					errormsg(WHERE,ERR_WRITE,smb.file,i);
				smb_unlockmsghdr(&smb,&msg); }
			break;
		case '>':
			for(i=curmsg+1;i<msgs;i++)
				if(mail[i].subj==msg.idx.subj)
					break;
			if(i<msgs)
				curmsg=i;
			else
				domsg=0;
			break;
		case '<':   /* Search Title backward */
			for(i=curmsg-1;i>-1;i--)
				if(mail[i].subj==msg.idx.subj)
					break;
            if(i>-1)
				curmsg=i;
			else
				domsg=0;
			break;
		case '}':   /* Search Author forward */
			strcpy(str,msg.from);
			for(i=curmsg+1;i<msgs;i++)
				if(mail[i].from==msg.idx.from)
					break;
			if(i<msgs)
				curmsg=i;
			else
				domsg=0;
			break;
		case 'N':   /* Got to next un-read message */
			for(i=curmsg+1;i<msgs;i++)
				if(!(mail[i].attr&MSG_READ))
					break;
			if(i<msgs)
				curmsg=i;
			else
				domsg=0;
            break;
		case '{':   /* Search Author backward */
			strcpy(str,msg.from);
			for(i=curmsg-1;i>-1;i--)
				if(mail[i].from==msg.idx.from)
					break;
			if(i>-1)
				curmsg=i;
			else
				domsg=0;
			break;
		case ']':   /* Search To User forward */
			strcpy(str,msg.to);
			for(i=curmsg+1;i<msgs;i++)
				if(mail[i].to==msg.idx.to)
					break;
			if(i<msgs)
				curmsg=i;
			else
				domsg=0;
			break;
		case '[':   /* Search To User backward */
			strcpy(str,msg.to);
			for(i=curmsg-1;i>-1;i--)
				if(mail[i].to==msg.idx.to)
					break;
			if(i>-1)
				curmsg=i;
			else
				domsg=0;
            break;
		case 0:
		case '+':
			if(curmsg<msgs-1) curmsg++;
			else done=1;
			break;
		case '-':
			if(curmsg>0) curmsg--;
			break;
		case 'S':
			domsg=0;
/*
			if(!yesno(text[SaveMsgToFile]))
				break;
*/
			bputs(text[FileToWriteTo]);
			if(getstr(str,40,K_LINE|K_UPPER))
				msgtotxt(msg,str,1,1);
			break;
		case 'E':
			editmsg(&msg,INVALID_SUB);
			break;
		case 'T':
			domsg=0;
			i=curmsg;
			if(i) i++;
			j=i+10;
			if(j>msgs)
				j=msgs;

			if(which==MAIL_SENT)
				bputs(text[MailSentLstHdr]);
			else if(which==MAIL_ALL)
				bputs(text[MailOnSystemLstHdr]);
			else
                bputs(text[MailWaitingLstHdr]);
			for(;i<j;i++) {
				if(msg.total_hfields)
					smb_freemsgmem(&msg);
				msg.total_hfields=0;
				msg.idx.offset=mail[i].offset;
				if(!loadmsg(&msg,mail[i].number))
					continue;
				smb_unlockmsghdr(&smb,&msg);
				if(which==MAIL_ALL)
					bprintf(text[MailOnSystemLstFmt]
						,i+1,msg.from,msg.to
						,msg.hdr.attr&MSG_DELETE ? '-'
							: msg.hdr.attr&MSG_READ ? SP
							: msg.from_net.type || msg.to_net.type ? 'N':'*'
						,msg.subj);
				else
					bprintf(text[MailWaitingLstFmt],i+1
						,which==MAIL_SENT ? msg.to
						: (msg.hdr.attr&MSG_ANONYMOUS) && !SYSOP
						? text[Anonymous] : msg.from
						,msg.hdr.attr&MSG_DELETE ? '-'
							: msg.hdr.attr&MSG_READ ? SP
							: msg.from_net.type || msg.to_net.type ? 'N':'*'
						,msg.subj);
				smb_freemsgmem(&msg);
				msg.total_hfields=0; }
			curmsg=(i-1);
			break;
		case 'U':   /* user edit */
			msg.hdr.number=msg.idx.number;
			smb_getmsgidx(&smb,&msg);
			useredit(which==MAIL_SENT ? msg.idx.to : msg.idx.from,0);
			break;
		case 'P':   /* Purge author and all mail to/from */
			if(noyes(text[AreYouSureQ]))
				break;
            msg.hdr.number=msg.idx.number;
            smb_getmsgidx(&smb,&msg);
            purgeuser(msg.idx.from);
			if(curmsg<msgs-1) curmsg++;
            break;
		case '?':
			strcpy(str,which==MAIL_YOUR ? "MAILREAD" : which==MAIL_ALL
					? "ALLMAIL" : "SENTMAIL");
			menu(str);
			if(SYSOP && which==MAIL_SENT)
				menu("SYSSMAIL");
			else if(SYSOP && which==MAIL_YOUR)
				menu("SYSMAILR");   /* Sysop Mail Read */
			domsg=0;
			break;
			} }

if(msg.total_hfields)
	smb_freemsgmem(&msg);

if(msgs)
    FREE(mail);

/***************************************/
/* Delete messages marked for deletion */
/***************************************/

if(sys_misc&SM_DELEMAIL) {
	if((i=smb_locksmbhdr(&smb))!=0) 			/* Lock the base, so nobody */
		errormsg(WHERE,ERR_LOCK,smb.file,i);	/* messes with the index */
	else
		delmail(usernumber,which); }

smb_close(&smb);
smb_stack(&smb,SMB_STACK_POP);
}

