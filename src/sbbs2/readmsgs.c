#line 1 "READMSGS.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "post.h"

char movemsg(smbmsg_t msg, uint subnum);
void editmsg(smbmsg_t *msg, uint subnum);

int sub_op(uint subnum)
{
return(SYSOP || (sub[subnum]->op_ar[0] && chk_ar(sub[subnum]->op_ar,useron)));
}


void listmsgs(int subnum, post_t HUGE16 *post, long i, long posts)
{
	char ch;
	int j;
	smbmsg_t msg;


bputs(text[MailOnSystemLstHdr]);
msg.total_hfields=0;
while(i<posts && !msgabort()) {
	if(msg.total_hfields)
		smb_freemsgmem(&msg);
	msg.total_hfields=0;
	msg.idx.offset=post[i].offset;
	if(!loadmsg(&msg,post[i].number))
		break;
	smb_unlockmsghdr(&smb,&msg);
	if(msg.hdr.attr&MSG_DELETE)
		ch='-';
	else if((!stricmp(msg.to,useron.alias) || !stricmp(msg.to,useron.name))
		&& !(msg.hdr.attr&MSG_READ))
		ch='!';
	else if(msg.hdr.number>sub[subnum]->ptr)
		ch='*';
	else
		ch=' ';
	bprintf(text[SubMsgLstFmt],(long)i+1
		,msg.hdr.attr&MSG_ANONYMOUS && !sub_op(subnum)
		? text[Anonymous] : msg.from
		,msg.to
		,ch
		,msg.subj);
	smb_freemsgmem(&msg);
	msg.total_hfields=0;
	i++; }
}

char *binstr(uchar *buf, ushort length)
{
	static char str[128];
	char tmp[128];
	int i;

str[0]=0;
for(i=0;i<length;i++)
	if(buf[i] && (buf[i]<SP || buf[i]>=0x7f))
		break;
if(i==length)		/* not binary */
	return(buf);
for(i=0;i<length;i++) {
	sprintf(tmp,"%02X ",buf[i]);
	strcat(str,tmp); }
return(str);
}


void msghdr(smbmsg_t msg)
{
	int i;

for(i=0;i<msg.total_hfields;i++)
	bprintf("hfield[%u].type       %02Xh\r\n"
			"hfield[%u].length     %d\r\n"
			"hfield[%u]_dat        %s\r\n"
			,i,msg.hfield[i].type
			,i,msg.hfield[i].length
			,i,binstr(msg.hfield_dat[i],msg.hfield[i].length));
}

/****************************************************************************/
/****************************************************************************/
post_t HUGE16 *loadposts(ulong *posts, uint subnum, ulong ptr, uint mode)
{
	char name[128];
	ushort aliascrc,namecrc,sysop;
	int i,file,skip;
	ulong l=0,total,alloc_len;
	smbmsg_t msg;
	idxrec_t idx;
	post_t HUGE16 *post;

if(posts==NULL)
	return(NULL);

(*posts)=0;

if((i=smb_locksmbhdr(&smb))!=0) {				/* Be sure noone deletes or */
	errormsg(WHERE,ERR_LOCK,smb.file,i);		/* adds while we're reading */
	return(NULL); }

total=filelength(fileno(smb.sid_fp))/sizeof(idxrec_t); /* total msgs in sub */

if(!total) {			/* empty */
	smb_unlocksmbhdr(&smb);
	return(NULL); }

strcpy(name,useron.name);
strlwr(name);
namecrc=crc16(name);
strcpy(name,useron.alias);
strlwr(name);
aliascrc=crc16(name);
sysop=crc16("sysop");

rewind(smb.sid_fp);

alloc_len=sizeof(post_t)*total;
#ifdef __OS2__
	while(alloc_len%4096)
		alloc_len++;
#endif
if((post=(post_t HUGE16 *)LMALLOC(alloc_len))==NULL) {	/* alloc for max */
	smb_unlocksmbhdr(&smb);
	errormsg(WHERE,ERR_ALLOC,smb.file,sizeof(post_t *)*sub[subnum]->maxmsgs);
	return(NULL); }
while(!feof(smb.sid_fp)) {
	skip=0;
	if(!fread(&idx,sizeof(idxrec_t),1,smb.sid_fp))
        break;

	if(idx.number<=ptr)
		continue;

	if(idx.attr&MSG_READ && mode&LP_UNREAD) /* Skip read messages */
        continue;

	if(idx.attr&MSG_DELETE) {		/* Pre-flagged */
		if(mode&LP_REP) 			/* Don't include deleted msgs in REP pkt */
			continue;
		if(!(sys_misc&SM_SYSVDELM)) /* Noone can view deleted msgs */
			continue;
		if(!(sys_misc&SM_USRVDELM)	/* Users can't view deleted msgs */
			&& !sub_op(subnum)) 	/* not sub-op */
			continue;
		if(!sub_op(subnum)			/* not sub-op */
			&& idx.from!=namecrc && idx.from!=aliascrc) /* not for you */
			continue; }

	if(idx.attr&MSG_MODERATED && !(idx.attr&MSG_VALIDATED)
		&& (mode&LP_REP || !sub_op(subnum)))
		break;

	if(idx.attr&MSG_PRIVATE && !(mode&LP_PRIVATE)
		&& !sub_op(subnum) && !(useron.rest&FLAG('Q'))) {
		if(idx.to!=namecrc && idx.from!=namecrc
			&& idx.to!=aliascrc && idx.from!=aliascrc
			&& (useron.number!=1 || idx.to!=sysop))
			continue;
		if(!smb_lockmsghdr(&smb,&msg)) {
			if(!smb_getmsghdr(&smb,&msg)) {
				if(stricmp(msg.to,useron.alias)
					&& stricmp(msg.from,useron.alias)
					&& stricmp(msg.to,useron.name)
					&& stricmp(msg.from,useron.name)
					&& (useron.number!=1 || stricmp(msg.to,"sysop")
					|| msg.from_net.type))
					skip=1;
				smb_freemsgmem(&msg); }
			smb_unlockmsghdr(&smb,&msg); }
		if(skip)
            continue; }


	if(!(mode&LP_BYSELF) && (idx.from==namecrc || idx.from==aliascrc)) {
		msg.idx=idx;
		if(!smb_lockmsghdr(&smb,&msg)) {
			if(!smb_getmsghdr(&smb,&msg)) {
				if(!stricmp(msg.from,useron.alias)
					|| !stricmp(msg.from,useron.name))
					skip=1;
				smb_freemsgmem(&msg); }
			smb_unlockmsghdr(&smb,&msg); }
		if(skip)
			continue; }

	if(!(mode&LP_OTHERS)) {
		if(idx.to!=namecrc && idx.to!=aliascrc
			&& (useron.number!=1 || idx.to!=sysop))
			continue;
		msg.idx=idx;
		if(!smb_lockmsghdr(&smb,&msg)) {
			if(!smb_getmsghdr(&smb,&msg)) {
				if(stricmp(msg.to,useron.alias) && stricmp(msg.to,useron.name)
					&& (useron.number!=1 || stricmp(msg.to,"sysop")
					|| msg.from_net.type))
					skip=1;
				smb_freemsgmem(&msg); }
			smb_unlockmsghdr(&smb,&msg); }
		if(skip)
            continue; }

	post[l].offset=idx.offset;
	post[l].number=idx.number;
	post[l].to=idx.to;
	post[l].from=idx.from;
	post[l].subj=idx.subj;
	l++; }
smb_unlocksmbhdr(&smb);
if(!l) {
	LFREE(post);
	post=NULL; }
(*posts)=l;
return(post);
}


/****************************************************************************/
/* Reads posts on subboard sub. 'mode' determines new-posts only, browse,   */
/* or continuous read.                                                      */
/* Returns 0 if normal completion, 1 if aborted.                            */
/* Called from function main_sec                                            */
/****************************************************************************/
char scanposts(uint subnum, char mode, char *find)
{
	char	str[256],str2[256],str3[256],reread=0,mismatches=0
			,done=0,domsg=1,HUGE16 *buf,*p;
	int 	file,i,j,usub,ugrp,reads=0;
	uint	lp=0;
	long	curpost;
	ulong	msgs,last,posts,l;
	post_t	HUGE16 *post;
	smbmsg_t	msg;

cursubnum=subnum;	/* for ARS */
if(!chk_ar(sub[subnum]->read_ar,useron)) {
	bprintf("\1n\r\nYou can't read messages on %s %s\r\n"
			,grp[sub[subnum]->grp]->sname,sub[subnum]->sname);
	return(0); }
msg.total_hfields=0;				/* init to NULL, specify not-allocated */
if(!(mode&SCAN_CONST))
	lncntr=0;
if((msgs=getlastmsg(subnum,&last,0))==0) {
	if(mode&(SCAN_NEW|SCAN_TOYOU))
		bprintf(text[NScanStatusFmt]
			,grp[sub[subnum]->grp]->sname,sub[subnum]->lname,0L,0L);
	else
		bprintf(text[NoMsgsOnSub]
			,grp[sub[subnum]->grp]->sname,sub[subnum]->sname);
	return(0); }
if(mode&SCAN_NEW && sub[subnum]->ptr>=last && !(mode&SCAN_BACK)) {
	if(sub[subnum]->ptr>last)
		sub[subnum]->ptr=sub[subnum]->last=last;
	bprintf(text[NScanStatusFmt]
		,grp[sub[subnum]->grp]->sname,sub[subnum]->lname,0L,msgs);
	return(0); }

if((i=smb_stack(&smb,SMB_STACK_PUSH))!=0) {
	errormsg(WHERE,ERR_OPEN,sub[subnum]->code,i);
	return(0); }
sprintf(smb.file,"%s%s",sub[subnum]->data_dir,sub[subnum]->code);
smb.retry_time=smb_retry_time;
if((i=smb_open(&smb))!=0) {
	smb_stack(&smb,SMB_STACK_POP);
	errormsg(WHERE,ERR_OPEN,smb.file,i);
	return(0); }

if(!(mode&SCAN_TOYOU)
	&& (!mode || mode&SCAN_FIND || !(sub[subnum]->misc&SUB_YSCAN)))
	lp=LP_BYSELF|LP_OTHERS;
if(mode&SCAN_TOYOU)
	lp|=LP_UNREAD;
post=loadposts(&posts,subnum,0,lp);
if(mode&SCAN_NEW) { 		  /* Scanning for new messages */
	for(curpost=0;curpost<posts;curpost++)
		if(sub[subnum]->ptr<post[curpost].number)
			break;
	bprintf(text[NScanStatusFmt]
		,grp[sub[subnum]->grp]->sname,sub[subnum]->lname,posts-curpost,msgs);
	if(!posts) {		  /* no messages at all */
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		return(0); }
	if(curpost==posts) {  /* no new messages */
        if(!(mode&SCAN_BACK)) {
			if(post)
				LFREE(post);
			smb_close(&smb);
			smb_stack(&smb,SMB_STACK_POP);
			return(0); }
		curpost=posts-1; } }
else {
	if(mode&SCAN_TOYOU)
		bprintf(text[NScanStatusFmt]
		   ,grp[sub[subnum]->grp]->sname,sub[subnum]->lname,posts,msgs);
	if(!posts) {
		if(!(mode&SCAN_TOYOU))
			bprintf(text[NoMsgsOnSub]
				,grp[sub[subnum]->grp]->sname,sub[subnum]->sname);
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
        return(0); }
	if(mode&SCAN_FIND) {
		bprintf(text[SearchSubFmt]
			,grp[sub[subnum]->grp]->sname,sub[subnum]->lname,posts);
		domsg=1;
		curpost=0; }
	else if(mode&SCAN_TOYOU)
		curpost=0;
	else {
		for(curpost=0;curpost<posts;curpost++)
			if(post[curpost].number>=sub[subnum]->last)
				break;
		if(curpost==posts)
			curpost=posts-1;

		domsg=1; } }

if(useron.misc&RIP)
	menu("MSGSCAN");

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
smb_unlocksmbhdr(&smb);
last=smb.status.last_msg;

action=NODE_RMSG;
if(mode&SCAN_CONST) {   /* update action */
    getnodedat(node_num,&thisnode,1);
    thisnode.action=NODE_RMSG;
    putnodedat(node_num,thisnode); }
while(online && !done) {

	action=NODE_RMSG;
	if((i=heapcheck())!=_HEAPOK) {
		errormsg(WHERE,ERR_CHK,"heap",i);
		break; }

	if(mode&(SCAN_CONST|SCAN_FIND) && sys_status&SS_ABORT)
		break;

	if(post==NULL)	/* Been unloaded */
		post=loadposts(&posts,subnum,0,lp);   /* So re-load */

	if(!posts) {
        done=1;
        continue; }

	while(curpost>=posts) curpost--;

    for(ugrp=0;ugrp<usrgrps;ugrp++)
        if(usrgrp[ugrp]==sub[subnum]->grp)
            break;
    for(usub=0;usub<usrsubs[ugrp];usub++)
        if(usrsub[ugrp][usub]==subnum)
            break;
    usub++;
    ugrp++;

	msg.idx.offset=post[curpost].offset;
	msg.idx.number=post[curpost].number;
	msg.idx.to=post[curpost].to;
	msg.idx.from=post[curpost].from;
	msg.idx.subj=post[curpost].subj;

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
		if(post) {
			LFREE((void *)post); }
		post=loadposts(&posts,subnum,0,lp);   /* So re-load */
		if(!posts)
			break;
		for(curpost=0;curpost<posts;curpost++)
			if(post[curpost].number==msg.idx.number)
				break;
		if(curpost>(posts-1))
			curpost=(posts-1);
		continue; }

	if(msg.total_hfields)
		smb_freemsgmem(&msg);
	msg.total_hfields=0;

	if(!loadmsg(&msg,post[curpost].number)) {
		if(mismatches>5) {	/* We can't do this too many times in a row */
			errormsg(WHERE,ERR_CHK,smb.file,post[curpost].number);
			break; }
		if(post)
			LFREE(post);
		post=loadposts(&posts,subnum,0,lp);
		if(!posts)
			break;
		if(curpost>(posts-1))
			curpost=(posts-1);
		mismatches++;
		continue; }
	smb_unlockmsghdr(&smb,&msg);

	mismatches=0;

	if(domsg) {

		if(!reread && mode&SCAN_FIND) { 			/* Find text in messages */
			buf=smb_getmsgtxt(&smb,&msg,GETMSGTXT_TAILS);
			if(!buf) {
				if(curpost<posts-1) curpost++;
				else done=1;
                continue; }
			strupr((char *)buf);
			if(!strstr((char *)buf,find) && !strstr(msg.subj,find)) {
				FREE(buf);
				if(curpost<posts-1) curpost++;
				else done=1;
                continue; }
			FREE(buf); }

		if(mode&SCAN_CONST)
			bprintf(text[ZScanPostHdr],ugrp,usub,curpost+1,posts);

		if(!reads && mode)
            CRLF;

		show_msg(msg
			,msg.from_ext && !strcmp(msg.from_ext,"1") && !msg.from_net.type
				? 0:P_NOATCODES);

		reads++;	/* number of messages actually read during this sub-scan */

		/* Message is to this user and hasn't been read, so flag as read */
		if((!stricmp(msg.to,useron.name) || !stricmp(msg.to,useron.alias)
			|| (useron.number==1 && !stricmp(msg.to,"sysop")
			&& !msg.from_net.type))
			&& !(msg.hdr.attr&MSG_READ)) {
			if(msg.total_hfields)
				smb_freemsgmem(&msg);
			msg.total_hfields=0;
			msg.idx.offset=0;
			if(!smb_locksmbhdr(&smb)) { 			  /* Lock the entire base */
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

		sub[subnum]->last=post[curpost].number;

		if(sub[subnum]->ptr<post[curpost].number && !(mode&SCAN_TOYOU)) {
            posts_read++;
			sub[subnum]->ptr=post[curpost].number; } }
	else domsg=1;
    if(mode&SCAN_CONST) {
		if(curpost<posts-1) curpost++;
            else done=1;
        continue; }
	if(useron.misc&WIP)
		menu("MSGSCAN");
	ASYNC;
	bprintf(text[ReadingSub],ugrp,grp[sub[subnum]->grp]->sname
		,usub,sub[subnum]->sname,curpost+1,posts);
	sprintf(str,"ABCDFILMPQRTY?<>[]{}-+.,");
	if(sub_op(subnum))
		strcat(str,"O");
	reread=0;
	l=getkeys(str,posts);
	if(l&0x80000000L) {
		if((long)l==-1) { /* ctrl-c */
			if(msg.total_hfields)
				smb_freemsgmem(&msg);
			if(post)
				LFREE(post);
			smb_close(&smb);
			smb_stack(&smb,SMB_STACK_POP);
            return(1); }
		curpost=(l&~0x80000000L)-1;
		reread=1;
        continue; }
	switch(l) {
		case 'A':   /* Reply to last message */
			domsg=0;
			if(!chk_ar(sub[subnum]->post_ar,useron)) {
				bputs(text[CantPostOnSub]);
				break; }
			quotemsg(msg,0);
			if(post)
				LFREE(post);
			post=NULL;
			postmsg(subnum,&msg,WM_QUOTE);
//			  post=loadposts(&posts,subnum,0,lp);
			if(mode&SCAN_TOYOU)
				domsg=1;
			break;
		case 'B':   /* Skip sub-board */
			if(mode&SCAN_NEW && !noyes(text[RemoveFromNewScanQ]))
				sub[subnum]->misc&=~SUB_NSCAN;
			if(msg.total_hfields)
				smb_freemsgmem(&msg);
			if(post)
				LFREE(post);
			smb_close(&smb);
			smb_stack(&smb,SMB_STACK_POP);
			return(0);
		case 'C':   /* Continuous */
			mode|=SCAN_CONST;
			if(curpost<posts-1) curpost++;
			else done=1;
			break;
		case 'D':       /* Delete message on sub-board */
			domsg=0;
			if(!sub_op(subnum) && !(sub[subnum]->misc&SUB_DEL)) {
				bputs(text[CantDeletePosts]);
				break; }
			if(!sub_op(subnum)) {
				if(stricmp(sub[subnum]->misc&SUB_NAME
					? useron.name : useron.alias, msg.from)
				&& stricmp(sub[subnum]->misc&SUB_NAME
					? useron.name : useron.alias, msg.to)) {
					bprintf(text[YouDidntPostMsgN],curpost+1);
					break; } }
			if(msg.hdr.attr&MSG_PERMANENT) {
				bputs("\1n\r\nMessage is marked permanent.\r\n");
				domsg=0;
				break; }
			if(post)
				LFREE(post);
			post=NULL;

			if(msg.total_hfields)
				smb_freemsgmem(&msg);
			msg.total_hfields=0;
			msg.idx.offset=0;
			if(loadmsg(&msg,msg.idx.number)) {
				msg.idx.attr^=MSG_DELETE;
				msg.hdr.attr=msg.idx.attr;
				if((i=smb_putmsg(&smb,&msg))!=0)
					errormsg(WHERE,ERR_WRITE,smb.file,i);
				smb_unlockmsghdr(&smb,&msg);
				if(i==0 && msg.idx.attr&MSG_DELETE) {
					sprintf(str,"Removed post from %s %s"
						,grp[sub[subnum]->grp]->sname,sub[subnum]->lname);
					logline("P-",str);
					if(!stricmp(sub[subnum]->misc&SUB_NAME
						? useron.name : useron.alias, msg.from))
						useron.posts=adjustuserrec(useron.number
							,U_POSTS,5,-1); } }
			domsg=1;
			if((sys_misc&SM_SYSVDELM		// anyone can view delete msgs
				|| (sys_misc&SM_USRVDELM	// sys/subops can view deleted msgs
				&& sub_op(subnum)))
				&& curpost<posts-1)
				curpost++;
			if(curpost>=posts-1)
				done=1;
			break;

		case 'F':   /* find text in messages */
			domsg=0;
			bprintf(text[StartWithN],curpost+2);
			if((i=getnum(posts))<0)
				break;
			if(i)
				i--;
			else
				i=curpost+1;
			bputs(text[SearchStringPrompt]);
			if(!getstr(str,40,K_LINE|K_UPPER))
				break;
			searchposts(subnum,post,(long)i,posts,str);
			break;
		case 'I':   /* Sub-board information */
			domsg=0;
			subinfo(subnum);
			break;
		case 'L':   /* List messages */
			domsg=0;
			bprintf(text[StartWithN],curpost+1);
			if((i=getnum(posts))<0)
				break;
			if(i) i--;
			else i=curpost;
			listmsgs(subnum,post,i,posts);
			break;
		case 'M':   /* Reply to last post in mail */
			domsg=0;
			if(msg.hdr.attr&MSG_ANONYMOUS && !sub_op(subnum)) {
				bputs(text[CantReplyToAnonMsg]);
				break; }
			if(!sub_op(subnum) && msg.hdr.attr&MSG_PRIVATE
				&& stricmp(msg.to,useron.name)
				&& stricmp(msg.to,useron.alias))
				break;
			sprintf(str2,text[Regarding]
				,msg.subj
				,timestr((time_t *)&msg.hdr.when_written.time));
			if(msg.from_net.type==NET_FIDO)
				sprintf(str,"%s @%s",msg.from
					,faddrtoa(*(faddr_t *)msg.from_net.addr));
			else if(msg.from_net.type==NET_INTERNET)
				strcpy(str,msg.from_net.addr);
			else if(msg.from_net.type)
				sprintf(str,"%s@%s",msg.from,msg.from_net.addr);
			else
				strcpy(str,msg.from);
			bputs(text[Email]);
			if(!getstr(str,60,K_EDIT|K_AUTODEL))
				break;
			if(post)
				LFREE(post);
			post=NULL;
			quotemsg(msg,1);
			if(msg.from_net.type==NET_INTERNET
				&& (!strcmp(str,msg.from_net.addr) || strchr(str,'@')))
				inetmail(str,msg.subj,WM_QUOTE|WM_NETMAIL);
			else {
				p=strrchr(str,'@');
				if(p)								/* FidoNet or QWKnet */
					netmail(str,msg.subj,WM_QUOTE);
				else {
					i=atoi(str);
					if(!i) {
						if(sub[subnum]->misc&SUB_NAME)
							i=userdatdupe(0,U_NAME,LEN_NAME,str,0);
						else
							i=matchuser(str); }
					email(i,str2,msg.subj,WM_EMAIL|WM_QUOTE); } }
//			  post=loadposts(&posts,subnum,0,lp);
			break;
		case 'P':   /* Post message on sub-board */
			domsg=0;
			if(!chk_ar(sub[subnum]->post_ar,useron))
				bputs(text[CantPostOnSub]);
			else {
				if(post)
					LFREE(post);
				post=NULL;
				postmsg(subnum,0,0);
//				  post=loadposts(&posts,subnum,0,lp);
				}
			break;
		case 'Q':   /* Quit */
			if(msg.total_hfields)
				smb_freemsgmem(&msg);
			if(post)
				LFREE(post);
			smb_close(&smb);
			smb_stack(&smb,SMB_STACK_POP);
			return(1);
		case 'R':   /* re-read last message */
			reread=1;
			break;
		case 'T':   /* List titles of next ten messages */
			domsg=0;
			if(!posts)
				break;
			if(curpost>=posts-1) {
				 done=1;
				 break; }
			i=curpost+11;
			if(i>posts)
				i=posts;
			listmsgs(subnum,post,curpost+1,i);
			curpost=i-1;
			if(sub[subnum]->ptr<post[curpost].number)
				sub[subnum]->ptr=post[curpost].number;
			break;
		case 'Y':   /* Your messages */
			domsg=0;
			showposts_toyou(post,0,posts);
			break;
		case '-':
			if(curpost>0) curpost--;
			reread=1;
			break;
		case 'O':   /* Operator commands */
			while(online) {
				if(!(useron.misc&EXPERT))
					menu("SYSMSCAN");
				bprintf("\r\n\1y\1hOperator: \1w");
				strcpy(str,"?CEHMPQUV");
				if(SYSOP)
					strcat(str,"S");
				switch(getkeys(str,0)) {
					case '?':
						if(useron.misc&EXPERT)
							menu("SYSMSCAN");
						continue;
					case 'P':   /* Purge user */
						purgeuser(sub[subnum]->misc&SUB_NAME
							? userdatdupe(0,U_NAME,LEN_NAME,msg.from,0)
							: matchuser(msg.from));
						break;
					case 'C':   /* Change message attributes */
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
					case 'E':   /* edit last post */
						if(post)
							LFREE(post);
						post=NULL;
						editmsg(&msg,subnum);
//						  post=loadposts(&posts,subnum,0,lp);
						break;
					case 'H':   /* View message header */
						msghdr(msg);
						domsg=0;
						break;
					case 'M':   /* Move message */
						domsg=0;
						if(post)
							LFREE(post);
						post=NULL;
						if(msg.total_hfields)
							smb_freemsgmem(&msg);
						msg.total_hfields=0;
						msg.idx.offset=0;
						if(!loadmsg(&msg,msg.idx.number)) {
							errormsg(WHERE,ERR_READ,smb.file,msg.idx.number);
							break; }
						sprintf(str,text[DeletePostQ],msg.hdr.number,msg.subj);
						if(movemsg(msg,subnum) && yesno(str)) {
							msg.idx.attr|=MSG_DELETE;
							msg.hdr.attr=msg.idx.attr;
							if((i=smb_putmsg(&smb,&msg))!=0)
								errormsg(WHERE,ERR_WRITE,smb.file,i); }
						smb_unlockmsghdr(&smb,&msg);
//						  post=loadposts(&posts,subnum,0,lp);
						break;

					case 'Q':
						break;
					case 'S':   /* Save/Append message to another file */
/*	05/26/95
						if(!yesno(text[SaveMsgToFile]))
							break;
*/
						bputs(text[FileToWriteTo]);
						if(getstr(str,40,K_LINE|K_UPPER))
							msgtotxt(msg,str,1,1);
						break;
					case 'U':   /* User edit */
						useredit(sub[subnum]->misc&SUB_NAME
							? userdatdupe(0,U_NAME,LEN_NAME,msg.from,0)
							: matchuser(msg.from),0);
						break;
					case 'V':   /* Validate message */
						if(msg.total_hfields)
							smb_freemsgmem(&msg);
						msg.total_hfields=0;
						msg.idx.offset=0;
						if(loadmsg(&msg,msg.idx.number)) {
							msg.idx.attr|=MSG_VALIDATED;
							msg.hdr.attr=msg.idx.attr;
							if((i=smb_putmsg(&smb,&msg))!=0)
								errormsg(WHERE,ERR_WRITE,smb.file,i);
							smb_unlockmsghdr(&smb,&msg); }
						break;
					default:
						continue; }
				break; }
			break;
		case '.':   /* Thread forward */
			l=msg.hdr.thread_first;
			if(!l) l=msg.hdr.thread_next;
			if(!l) {
				domsg=0;
				break; }
			for(i=0;i<posts;i++)
				if(l==post[i].number)
					break;
			if(i<posts)
				curpost=i;
			break;
		case ',':   /* Thread backwards */
			if(!msg.hdr.thread_orig) {
				domsg=0;
				break; }
			for(i=0;i<posts;i++)
				if(msg.hdr.thread_orig==post[i].number)
					break;
			if(i<posts)
				curpost=i;
			break;
		case '>':   /* Search Title forward */
			for(i=curpost+1;i<posts;i++)
				if(post[i].subj==msg.idx.subj)
					break;
			if(i<posts)
				curpost=i;
			else
				domsg=0;
			break;
		case '<':   /* Search Title backward */
			for(i=curpost-1;i>-1;i--)
				if(post[i].subj==msg.idx.subj)
					break;
            if(i>-1)
				curpost=i;
			else
				domsg=0;
			break;
		case '}':   /* Search Author forward */
			strcpy(str,msg.from);
			for(i=curpost+1;i<posts;i++)
				if(post[i].from==msg.idx.from)
					break;
			if(i<posts)
				curpost=i;
			else
				domsg=0;
			break;
		case '{':   /* Search Author backward */
			strcpy(str,msg.from);
			for(i=curpost-1;i>-1;i--)
				if(post[i].from==msg.idx.from)
					break;
			if(i>-1)
				curpost=i;
			else
				domsg=0;
			break;
		case ']':   /* Search To User forward */
			strcpy(str,msg.to);
			for(i=curpost+1;i<posts;i++)
				if(post[i].to==msg.idx.to)
					break;
			if(i<posts)
				curpost=i;
			else
				domsg=0;
			break;
		case '[':   /* Search To User backward */
			strcpy(str,msg.to);
			for(i=curpost-1;i>-1;i--)
				if(post[i].to==msg.idx.to)
					break;
			if(i>-1)
				curpost=i;
			else
				domsg=0;
			break;
		case 0: /* Carriage return - Next Message */
		case '+':
			if(curpost<posts-1) curpost++;
			else done=1;
			break;
		case '?':
			menu("MSGSCAN");
			domsg=0;
			break;	} }
if(msg.total_hfields)
	smb_freemsgmem(&msg);
if(post)
	LFREE(post);
if(!(mode&(SCAN_CONST|SCAN_TOYOU|SCAN_FIND)) && !(sub[subnum]->misc&SUB_PONLY)
	&& reads && chk_ar(sub[subnum]->post_ar,useron)
	&& !(useron.rest&FLAG('P'))) {
    sprintf(str,text[Post],grp[sub[subnum]->grp]->sname
        ,sub[subnum]->lname);
    if(!noyes(str))
		postmsg(subnum,0,0); }
smb_close(&smb);
smb_stack(&smb,SMB_STACK_POP);
return(0);
}

/****************************************************************************/
/* This function will search the specified sub-board for messages that      */
/* contain the string 'search'.                                             */
/* Returns number of messages found.                                        */
/****************************************************************************/
int searchsub(uint subnum,char *search)
{
	int 	i,found;
	ulong	l,posts,total;
	post_t	HUGE16 *post;

if((i=smb_stack(&smb,SMB_STACK_PUSH))!=0) {
	errormsg(WHERE,ERR_OPEN,sub[subnum]->code,i);
	return(0); }
total=getposts(subnum);
sprintf(smb.file,"%s%s",sub[subnum]->data_dir,sub[subnum]->code);
smb.retry_time=smb_retry_time;
if((i=smb_open(&smb))!=0) {
	smb_stack(&smb,SMB_STACK_POP);
	errormsg(WHERE,ERR_OPEN,smb.file,i);
	return(0); }
post=loadposts(&posts,subnum,0,LP_BYSELF|LP_OTHERS);
bprintf(text[SearchSubFmt]
	,grp[sub[subnum]->grp]->sname,sub[subnum]->lname,posts,total);
found=searchposts(subnum,post,0,posts,search);
if(posts)
	LFREE(post);
smb_close(&smb);
smb_stack(&smb,SMB_STACK_POP);
return(found);
}

/****************************************************************************/
/* Will search the messages pointed to by 'msg' for the occurance of the    */
/* string 'search' and display any messages (number of message, author and  */
/* title). 'msgs' is the total number of valid messages.                    */
/* Returns number of messages found.                                        */
/****************************************************************************/
int searchposts(uint subnum, post_t HUGE16 *post, long start, long posts
	, char *search)
{
	char	HUGE16 *buf,ch;
	ushort	xlat;
	int 	i,j;
	long	l,length,found=0;
	smbmsg_t msg;

msg.total_hfields=0;
for(l=start;l<posts && !msgabort();l++) {
	msg.idx.offset=post[l].offset;
	if(!loadmsg(&msg,post[l].number))
		continue;
	smb_unlockmsghdr(&smb,&msg);
	buf=smb_getmsgtxt(&smb,&msg,1);
	if(!buf) {
		smb_freemsgmem(&msg);
		continue; }
	strupr((char *)buf);
	if(strstr((char *)buf,search) || strstr(msg.subj,search)) {
		if(!found)
			CRLF;
		if(msg.hdr.attr&MSG_DELETE)
			ch='-';
		else if((!stricmp(msg.to,useron.alias) || !stricmp(msg.to,useron.name))
			&& !(msg.hdr.attr&MSG_READ))
			ch='!';
		else if(msg.hdr.number>sub[subnum]->ptr)
			ch='*';
		else
			ch=' ';
		bprintf(text[SubMsgLstFmt],l+1
			,(msg.hdr.attr&MSG_ANONYMOUS) && !sub_op(subnum) ? text[Anonymous]
			: msg.from
			,msg.to
			,ch
			,msg.subj);
		found++; }
	FREE(buf);
	smb_freemsgmem(&msg); }

return(found);
}

/****************************************************************************/
/* Will search the messages pointed to by 'msg' for message to the user on  */
/* Returns number of messages found.                                        */
/****************************************************************************/
void showposts_toyou(post_t HUGE16 *post, ulong start, ulong posts)
{
	char	str[128];
	ushort	namecrc,aliascrc,sysop;
	int 	i;
	ulong	l,found;
    smbmsg_t msg;

strcpy(str,useron.alias);
strlwr(str);
aliascrc=crc16(str);
strcpy(str,useron.name);
strlwr(str);
namecrc=crc16(str);
sysop=crc16("sysop");
msg.total_hfields=0;
for(l=start,found=0;l<posts && !msgabort();l++) {

	if((useron.number!=1 || post[l].to!=sysop)
		&& post[l].to!=aliascrc && post[l].to!=namecrc)
		continue;

	if(msg.total_hfields)
		smb_freemsgmem(&msg);
	msg.total_hfields=0;
	msg.idx.offset=post[l].offset;
	if(!loadmsg(&msg,post[l].number))
		continue;
	smb_unlockmsghdr(&smb,&msg);
	if((useron.number==1 && !stricmp(msg.to,"sysop") && !msg.from_net.type)
		|| !stricmp(msg.to,useron.alias) || !stricmp(msg.to,useron.name)) {
		if(!found)
			CRLF;
		found++;
		bprintf(text[SubMsgLstFmt],l+1
			,(msg.hdr.attr&MSG_ANONYMOUS) && !SYSOP
			? text[Anonymous] : msg.from
			,msg.to
			,msg.hdr.attr&MSG_DELETE ? '-' : msg.hdr.attr&MSG_READ ? ' ' : '*'
			,msg.subj); } }

if(msg.total_hfields)
	smb_freemsgmem(&msg);
}

/****************************************************************************/
/* This function will search the specified sub-board for messages that		*/
/* are sent to the currrent user.											*/
/* returns number of messages found 										*/
/****************************************************************************/
int searchsub_toyou(uint subnum)
{
	int 	i;
	ulong	l,posts,total;
	post_t	HUGE16 *post;

if((i=smb_stack(&smb,SMB_STACK_PUSH))!=0) {
	errormsg(WHERE,ERR_OPEN,sub[subnum]->code,i);
	return(0); }
total=getposts(subnum);
sprintf(smb.file,"%s%s",sub[subnum]->data_dir,sub[subnum]->code);
smb.retry_time=smb_retry_time;
if((i=smb_open(&smb))!=0) {
	smb_stack(&smb,SMB_STACK_POP);
	errormsg(WHERE,ERR_OPEN,smb.file,i);
	return(0); }
post=loadposts(&posts,subnum,0,0);
bprintf(text[SearchSubFmt]
	,grp[sub[subnum]->grp]->sname,sub[subnum]->lname,total);
if(posts) {
	if(post)
		LFREE(post);
	post=loadposts(&posts,subnum,0,LP_BYSELF|LP_OTHERS);
	showposts_toyou(post,0,posts); }
if(post)
	LFREE(post);
smb_close(&smb);
smb_stack(&smb,SMB_STACK_POP);
return(posts);
}


