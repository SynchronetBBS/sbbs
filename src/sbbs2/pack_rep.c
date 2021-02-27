#line 1 "PACK_REP.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "post.h"
#include "qwk.h"

/****************************************************************************/
/* Creates an REP packet for upload to QWK hub 'hubnum'.                    */
/* Returns 1 if successful, 0 if not.										*/
/****************************************************************************/
char pack_rep(uint hubnum)
{
	uchar	str[256],tmp2[256],ch,*p;
	uchar	HUGE16 *qwkbuf;
	int 	file,mode;
	uint	i,j,k,n;
	long	l,size,msgcnt,submsgs,mailmsgs,packedmail,netfiles=0,deleted;
	ulong	last,ptr,posts,msgs;
	post_t	HUGE16 *post;
	mail_t	*mail;
	struct	ffblk ff;
	FILE	*rep;
	smbmsg_t msg;

msgcnt=0L;
delfiles(temp_dir,"*.*");
sprintf(str,"%s%s.REP",data_dir,qhub[hubnum]->id);
if(fexist(str))
	external(cmdstr(qhub[hubnum]->unpack,str,"*.*",NULL),EX_OUTL);
/*************************************************/
/* Create SYSID.MSG, write header and leave open */
/*************************************************/
sprintf(str,"%s%s.MSG",temp_dir,qhub[hubnum]->id);
if((rep=fnopen(&file,str,O_CREAT|O_WRONLY))==NULL) {
	errormsg(WHERE,ERR_OPEN,str,O_CREAT|O_WRONLY);
	return(0); }
if(!filelength(file)) { 						/* New REP packet */
	sprintf(str,"%-128s",qhub[hubnum]->id);     /* So write header */
	fwrite(str,128,1,rep); }
fseek(rep,0L,SEEK_END);
/*********************/
/* Pack new messages */
/*********************/
console|=CON_L_ECHO;

sprintf(smb.file,"%sMAIL",data_dir);
smb.retry_time=smb_retry_time;
if((i=smb_open(&smb))!=0) {
	fclose(rep);
	errormsg(WHERE,ERR_OPEN,smb.file,i);
	return(0); }

/***********************/
/* Pack E-mail, if any */
/***********************/
qwkmail_time=time(NULL);
mailmsgs=loadmail(&mail,0,MAIL_YOUR,LM_QWK);
packedmail=0;
if(mailmsgs) {
	bputs(text[QWKPackingEmail]);
	for(l=0;l<mailmsgs;l++) {
		bprintf("\b\b\b\b\b%-5lu",l+1);

		msg.idx.offset=mail[l].offset;
		if(!loadmsg(&msg,mail[l].number))
			continue;

		sprintf(str,"%s/",qhub[hubnum]->id);
		if(msg.to_net.type!=NET_QWK
			|| (strcmp(msg.to_net.addr,qhub[hubnum]->id)
			&& strncmp(msg.to_net.addr,str,strlen(str)))) {
			smb_unlockmsghdr(&smb,&msg);
			smb_freemsgmem(&msg);
			continue; }

		msgtoqwk(msg,rep,TO_QNET|REP|A_LEAVE,INVALID_SUB,0);
		packedmail++;
		smb_unlockmsghdr(&smb,&msg);
		smb_freemsgmem(&msg); }
	bprintf(text[QWKPackedEmail],packedmail); }
smb_close(&smb);					/* Close the e-mail */
if(mailmsgs)
	FREE(mail);

useron.number=1;
getuserdat(&useron);
for(i=0;i<qhub[hubnum]->subs;i++) {
	j=qhub[hubnum]->sub[i]; 			/* j now equals the real sub num */
	msgs=getlastmsg(j,&last,0);
	lncntr=0;						/* defeat pause */
	if(!msgs || last<=sub[j]->ptr) {
		if(sub[j]->ptr>last) {
			sub[j]->ptr=last;
			sub[j]->last=last; }
		bprintf(text[NScanStatusFmt]
			,grp[sub[j]->grp]->sname
			,sub[j]->lname,0L,msgs);
		continue; }

	sprintf(smb.file,"%s%s"
		,sub[j]->data_dir,sub[j]->code);
	smb.retry_time=smb_retry_time;
	if((k=smb_open(&smb))!=0) {
		errormsg(WHERE,ERR_OPEN,smb.file,k);
		continue; }

	post=loadposts(&posts,j,sub[j]->ptr,LP_BYSELF|LP_OTHERS|LP_PRIVATE|LP_REP);
	bprintf(text[NScanStatusFmt]
		,grp[sub[j]->grp]->sname
		,sub[j]->lname,posts,msgs);
	if(!posts)	{ /* no new messages */
		smb_close(&smb);
		continue; }

    sub[j]->ptr=last;                   /* set pointer */
	bputs(text[QWKPackingSubboard]);	/* ptr to last msg	*/
	submsgs=0;
	for(l=0;l<posts;l++) {
		bprintf("\b\b\b\b\b%-5lu",l+1);

		msg.idx.offset=post[l].offset;
		if(!loadmsg(&msg,post[l].number))
			continue;

		if(msg.from_net.type && msg.from_net.type!=NET_QWK &&
			!(sub[j]->misc&SUB_GATE)) {
			smb_freemsgmem(&msg);
			smb_unlockmsghdr(&smb,&msg);
			continue; }

		if(!strncmpi(msg.subj,"NE:",3) || (msg.from_net.type==NET_QWK &&
			route_circ(msg.from_net.addr,qhub[hubnum]->id))) {
			smb_freemsgmem(&msg);
			smb_unlockmsghdr(&smb,&msg);
			continue; }

		mode=qhub[hubnum]->mode[i]|TO_QNET|REP;
		if(mode&A_LEAVE) mode|=(VIA|TZ);
		if(msg.from_net.type!=NET_QWK)
			mode|=TAGLINE;

		msgtoqwk(msg,rep,mode,j,qhub[hubnum]->conf[i]);

		smb_freemsgmem(&msg);
		smb_unlockmsghdr(&smb,&msg);
		msgcnt++;
		submsgs++; }
	bprintf(text[QWKPackedSubboard],submsgs,msgcnt);
	LFREE(post);
	smb_close(&smb); }

fclose(rep);			/* close MESSAGE.DAT */
CRLF;
						/* Look for extra files to send out */
sprintf(str,"%sQNET\\%s.OUT\\*.*",data_dir,qhub[hubnum]->id);
i=findfirst(str,&ff,0);
while(!i) {
    sprintf(str,"%sQNET\\%s.OUT\\%s",data_dir,qhub[hubnum]->id,ff.ff_name);
    sprintf(tmp2,"%s%s",temp_dir,ff.ff_name);
	bprintf(text[RetrievingFile],str);
	if(!mv(str,tmp2,1))
        netfiles++;
    i=findnext(&ff); }
if(netfiles)
	CRLF;

if(!msgcnt && !netfiles && !packedmail) {
	bputs(text[QWKNoNewMessages]);
	return(0); }

/*******************/
/* Compress Packet */
/*******************/
sprintf(str,"%s%s.REP",data_dir,qhub[hubnum]->id);
sprintf(tmp2,"%s*.*",temp_dir);
i=external(cmdstr(qhub[hubnum]->pack,str,tmp2,NULL),EX_OUTL);
if(!fexist(str)) {
	bputs(text[QWKCompressionFailed]);
	if(i)
		errormsg(WHERE,ERR_EXEC,cmdstr(qhub[hubnum]->pack,str,tmp2,NULL),i);
	else
		errorlog("Couldn't compress REP packet");
	return(0); }
sprintf(str,"%sQNET\\%s.OUT\\",data_dir,qhub[hubnum]->id);
delfiles(str,"*.*");

if(packedmail) {						/* Delete NetMail */
	sprintf(smb.file,"%sMAIL",data_dir);
	smb.retry_time=smb_retry_time;
	if((i=smb_open(&smb))!=0) {
		errormsg(WHERE,ERR_OPEN,smb.file,i);
		return(1); }

	mailmsgs=loadmail(&mail,0,MAIL_YOUR,LM_QWK);

	if((i=smb_locksmbhdr(&smb))!=0) {			  /* Lock the base, so nobody */
		if(mailmsgs)
			FREE(mail);
		smb_close(&smb);
		errormsg(WHERE,ERR_LOCK,smb.file,i);	/* messes with the index */
		return(1); }

	if((i=smb_getstatus(&smb))!=0) {
		if(mailmsgs)
			FREE(mail);
		smb_close(&smb);
		errormsg(WHERE,ERR_READ,smb.file,i);
		return(1); }

	deleted=0;
	/* Mark as READ and DELETE */
	for(l=0;l<mailmsgs;l++) {
		if(mail[l].time>qwkmail_time)
			continue;
		msg.idx.offset=0;
		if(!loadmsg(&msg,mail[l].number))
			continue;

		sprintf(str,"%s/",qhub[hubnum]->id);
		if(msg.to_net.type!=NET_QWK
			|| (strcmp(msg.to_net.addr,qhub[hubnum]->id)
			&& strncmp(msg.to_net.addr,str,strlen(str)))) {
			smb_unlockmsghdr(&smb,&msg);
			smb_freemsgmem(&msg);
            continue; }

		msg.hdr.attr|=MSG_DELETE;
		msg.idx.attr=msg.hdr.attr;
		if((i=smb_putmsg(&smb,&msg))!=0)
			errormsg(WHERE,ERR_WRITE,smb.file,i);
		else
			deleted++;
		smb_unlockmsghdr(&smb,&msg);
		smb_freemsgmem(&msg); }

	if(deleted && sys_misc&SM_DELEMAIL)
		delmail(0,MAIL_YOUR);
	smb_close(&smb);
	if(mailmsgs)
		FREE(mail); }

return(1);
}
