#line 1 "UN_REP.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "qwk.h"

/****************************************************************************/
/* Unpacks .REP packet, 'repname' is the path and filename of the packet    */
/****************************************************************************/
void unpack_rep()
{
	uchar	ch,str[256],fname[128],block[128]
			,*AttemptedToUploadREPpacket="Attempted to upload REP packet";
	int 	file;
	uint	h,i,j,k,msgs,lastsub=INVALID_SUB;
	ulong	n;
	long	l,size,misc,crc;
	node_t	node;
	time_t	t;
	file_t	f;
    struct  ffblk ff;
	FILE	*rep;

sprintf(str,"%s%s.REP",temp_dir,sys_id);
if(!fexist(str)) {
	bputs(text[QWKReplyNotReceived]);
	logline("U!",AttemptedToUploadREPpacket);
	logline(nulstr,"REP file not received");
	return; }
for(k=0;k<total_fextrs;k++)
	if(!stricmp(fextr[k]->ext,useron.tmpext) && chk_ar(fextr[k]->ar,useron))
		break;
if(k>=total_fextrs)
	k=0;
i=external(cmdstr(fextr[k]->cmd,str,"*.*",NULL),EX_OUTL|EX_OUTR);
if(i) {
	bputs(text[QWKExtractionFailed]);
	logline("U!",AttemptedToUploadREPpacket);
	logline(nulstr,"Extraction failed");
	return; }
sprintf(str,"%s%s.MSG",temp_dir,sys_id);
if(!fexist(str)) {
	bputs(text[QWKReplyNotReceived]);
	logline("U!",AttemptedToUploadREPpacket);
	logline(nulstr,"MSG file not received");
	return; }
if((rep=fnopen(&file,str,O_RDONLY))==NULL) {
	errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
	return; }
size=filelength(file);
fread(block,128,1,rep);
if(strncmpi(block,sys_id,strlen(sys_id))) {
	fclose(rep);
	bputs(text[QWKReplyNotReceived]);
	logline("U!",AttemptedToUploadREPpacket);
	logline(nulstr,"Incorrect BBSID");
	return; }
logline("U+","Uploaded REP packet");
/********************/
/* Process messages */
/********************/
bputs(text[QWKUnpacking]);

for(l=128;l<size;l+=i*128) {
	lncntr=0;					/* defeat pause */
	fseek(rep,l,SEEK_SET);
	fread(block,128,1,rep);
	sprintf(tmp,"%.6s",block+116);
	i=atoi(tmp);  /* i = number of 128 byte records */
	if(i<2) {
		sprintf(tmp,"%s blocks",str);
		errormsg(WHERE,ERR_CHK,tmp,i);
		i=1;
		continue; }
	if(atoi(block+1)==0) {					/**********/
		if(useron.rest&FLAG('E')) {         /* E-mail */
			bputs(text[R_Email]);			/**********/
			continue; }

        sprintf(str,"%25.25s",block+21);
		truncsp(str);
		if(!stricmp(str,"NETMAIL")) {  /* QWK to FidoNet NetMail */
			qwktonetmail(rep,block,NULL,0);
			continue; }
		if(strchr(str,'@')) {
			qwktonetmail(rep,block,str,0);
			continue; }
		if(!stricmp(str,"SBBS")) {    /* to SBBS, config stuff */
			qwkcfgline(block+71,INVALID_SUB);
            continue; }

		if(useron.etoday>=level_emailperday[useron.level]
			&& !(useron.rest&FLAG('Q'))) {
			bputs(text[TooManyEmailsToday]);
			continue; }
		j=atoi(str);
		if(j && j>lastuser())
			j=0;
		if(!j &&
			(!stricmp(str,"SYSOP")
			|| !stricmp(str,sys_id)
			|| !stricmp(str,sys_op)))
			j=1;
		if(!j)
			j=matchuser(str);
		if(!j) {
			bputs(text[UnknownUser]);
			continue; }
		if(j==1 && useron.rest&FLAG('S')) {
			bprintf(text[R_Feedback],sys_op);
			continue; }

		getuserrec(j,U_MISC,8,str);
		misc=ahtoul(str);
		if(misc&NETMAIL && sys_misc&SM_FWDTONET) {
			getuserrec(j,U_NETMAIL,LEN_NETMAIL,str);
			qwktonetmail(rep,block,str,0);
			continue; }

		sprintf(smb.file,"%sMAIL",data_dir);
		smb.retry_time=smb_retry_time;

		if(lastsub!=INVALID_SUB) {
			smb_close(&smb);
			lastsub=INVALID_SUB; }

		if((k=smb_open(&smb))!=0) {
			errormsg(WHERE,ERR_OPEN,smb.file,k);
			continue; }

		if(!filelength(fileno(smb.shd_fp))) {
			smb.status.max_crcs=mail_maxcrcs;
			smb.status.max_msgs=MAX_SYSMAIL;
			smb.status.max_age=mail_maxage;
			smb.status.attr=SMB_EMAIL;
			if((k=smb_create(&smb))!=0) {
				smb_close(&smb);
				errormsg(WHERE,ERR_CREATE,smb.file,k);
				continue; } }

		if((k=smb_locksmbhdr(&smb))!=0) {
			smb_close(&smb);
			errormsg(WHERE,ERR_LOCK,smb.file,k);
			continue; }

		if((k=smb_getstatus(&smb))!=0) {
			smb_close(&smb);
			errormsg(WHERE,ERR_READ,smb.file,k);
			continue; }

		smb_unlocksmbhdr(&smb);

		if(!qwktomsg(rep,block,0,INVALID_SUB,j)) {
			smb_close(&smb);
			continue; }
		smb_close(&smb);

		if(j==1) {
			useron.fbacks++;
			logon_fbacks++;
			putuserrec(useron.number,U_FBACKS,5
				,itoa(useron.fbacks,tmp,10)); }
		else {
			useron.emails++;
			logon_emails++;
			putuserrec(useron.number,U_EMAILS,5
				,itoa(useron.emails,tmp,10)); }
		useron.etoday++;
		putuserrec(useron.number,U_ETODAY,5
			,itoa(useron.etoday,tmp,10));
		bprintf(text[Emailed],username(j,tmp),j);
		sprintf(str,"E-mailed %s #%d",username(j,tmp),j);
		logline("E+",str);
		if(useron.rest&FLAG('Q')) {
			sprintf(tmp,"%-25.25s",block+46);
			truncsp(tmp); }
		else
			strcpy(tmp,useron.alias);
		for(k=1;k<=sys_nodes;k++) { /* Tell user, if online */
			getnodedat(k,&node,0);
			if(node.useron==j && !(node.misc&NODE_POFF)
				&& (node.status==NODE_INUSE
				|| node.status==NODE_QUIET)) {
				sprintf(str,text[EmailNodeMsg]
					,node_num,tmp);
				putnmsg(k,str);
				break; } }
		if(k>sys_nodes) {
			sprintf(str,text[UserSentYouMail],tmp);
			putsmsg(j,str); } }    /* end of email */

			/**************************/
	else {	/* message on a sub-board */
			/**************************/
		n=atol((char *)block+1); /* conference number */
		for(j=0;j<usrgrps;j++) {
			for(k=0;k<usrsubs[j];k++)
				if(sub[usrsub[j][k]]->qwkconf==n)
					break;
			if(k<usrsubs[j])
				break; }

		if(j>=usrgrps) {
			if(n<1000) {			 /* version 1 method, start at 101 */
				j=n/100;
				k=n-(j*100); }
			else {					 /* version 2 method, start at 1001 */
				j=n/1000;
				k=n-(j*1000); }
			j--;	/* j is group */
			k--;	/* k is sub */
			if(j>=usrgrps || k>=usrsubs[j]) {
				bprintf(text[QWKInvalidConferenceN],n);
				sprintf(str,"Invalid conference number %d",n);
				logline("P!",str);
				continue; } }

		n=usrsub[j][k];

		/* if posting, add to new-scan config for QWKnet nodes automatically */
        if(useron.rest&FLAG('Q'))
			sub[n]->misc|=SUB_NSCAN;

		sprintf(str,"%-25.25s","SBBS");
		if(!strnicmp((char *)block+21,str,25)) {	/* to SBBS, config stuff */
			qwkcfgline((char *)block+71,n);
			continue; }

		if(!SYSOP && sub[n]->misc&SUB_QNET) {	/* QWK Netted */
			sprintf(str,"%-25.25s","DROP");         /* Drop from new-scan? */
			if(!strnicmp((char *)block+71,str,25))	/* don't allow post */
				continue;
			sprintf(str,"%-25.25s","ADD");          /* Add to new-scan? */
			if(!strnicmp((char *)block+71,str,25))	/* don't allow post */
				continue; }

		if(useron.rest&FLAG('Q') && !(sub[n]->misc&SUB_QNET)) {
			bputs(text[CantPostOnSub]);
			logline("P!","Attempted to post on non-QWKnet sub");
			continue; }

        if(useron.rest&FLAG('P')) {
            bputs(text[R_Post]);
			logline("P!","Post attempted");
            continue; }

		if(useron.ptoday>=level_postsperday[useron.level]
			&& !(useron.rest&FLAG('Q'))) {
			bputs(text[TooManyPostsToday]);
			continue; }

		if(useron.rest&FLAG('N')
			&& sub[n]->misc&(SUB_FIDO|SUB_PNET|SUB_QNET|SUB_INET)) {
			bputs(text[CantPostOnSub]);
			logline("P!","Networked post attempted");
			continue; }

		if(!chk_ar(sub[n]->post_ar,useron)) {
			bputs(text[CantPostOnSub]);
			logline("P!","Post attempted");
			continue; }

		if((block[0]=='*' || block[0]=='+')
			&& !(sub[n]->misc&SUB_PRIV)) {
			bputs(text[PrivatePostsNotAllowed]);
			logline("P!","Private post attempt");
			continue; }

		if(block[0]=='*' || block[0]=='+'           /* Private post */
			|| sub[n]->misc&SUB_PONLY) {
			sprintf(str,"%-25.25s",nulstr);
			sprintf(tmp,"%-25.25s","ALL");
			if(!strnicmp((char *)block+21,str,25)
				|| !strnicmp((char *)block+21,tmp,25)) {	/* to blank */
				bputs(text[NoToUser]);						/* or all */
				continue; } }

		if(!SYSOP && !(useron.rest&FLAG('Q'))) {
			sprintf(str,"%-25.25s","SYSOP");
			if(!strnicmp((char *)block+21,str,25)) {
				sprintf(str,"%-25.25s",username(1,tmp));
				memcpy((char *)block+21,str,25); } }	/* change from sysop */
														/* to user #1 */

#if 0	/* TWIT FILTER */
		sprintf(str,"%25.25s",block+46);  /* From user */
		truncsp(str);

		if(!stricmp(str,"Lee Matherne")
			|| !stricmp(str,"Big Joe")
			) {
			bprintf(text[Posted],grp[sub[n]->grp]->sname
				,sub[n]->lname);
			continue; }
#endif

		if(n!=lastsub) {
			if(lastsub!=INVALID_SUB)
				smb_close(&smb);
			lastsub=INVALID_SUB;
			sprintf(smb.file,"%s%s",sub[n]->data_dir,sub[n]->code);
			smb.retry_time=smb_retry_time;
			if((j=smb_open(&smb))!=0) {
				errormsg(WHERE,ERR_OPEN,smb.file,j);
				continue; }

			if(!filelength(fileno(smb.shd_fp))) {
				smb.status.max_crcs=sub[n]->maxcrcs;
				smb.status.max_msgs=sub[n]->maxmsgs;
				smb.status.max_age=sub[n]->maxage;
				smb.status.attr=sub[n]->misc&SUB_HYPER ? SMB_HYPERALLOC:0;
				if((j=smb_create(&smb))!=0) {
					smb_close(&smb);
					lastsub=INVALID_SUB;
					errormsg(WHERE,ERR_CREATE,smb.file,j);
					continue; } }

			if((j=smb_locksmbhdr(&smb))!=0) {
				smb_close(&smb);
				lastsub=INVALID_SUB;
				errormsg(WHERE,ERR_LOCK,smb.file,j);
				continue; }
			if((j=smb_getstatus(&smb))!=0) {
				smb_close(&smb);
				lastsub=INVALID_SUB;
				errormsg(WHERE,ERR_READ,smb.file,j);
				continue; }
			smb_unlocksmbhdr(&smb);
			lastsub=n; }

		if(!qwktomsg(rep,block,0,n,0))
			continue;

		useron.ptoday++;
		useron.posts++;
//		  if(!(useron.rest&FLAG('Q')))
		logon_posts++;
		putuserrec(useron.number,U_POSTS,5,itoa(useron.posts,str,10));
		putuserrec(useron.number,U_PTODAY,5,itoa(useron.ptoday,str,10));
		bprintf(text[Posted],grp[sub[n]->grp]->sname
			,sub[n]->lname);
		sprintf(str,"Posted on %s %s",grp[sub[n]->grp]->sname
			,sub[n]->lname);
		if(sub[n]->misc&SUB_FIDO && sub[n]->echomail_sem[0])  /* semaphore */
			if((file=nopen(sub[n]->echomail_sem,O_WRONLY|O_CREAT|O_TRUNC))!=-1)
				close(file);
        logline("P+",str); } }         /* end of public message */

update_qwkroute(NULL);			/* Write ROUTE.DAT */

if(lastsub!=INVALID_SUB)
	smb_close(&smb);
fclose(rep);

if(useron.rest&FLAG('Q')) {             /* QWK Net Node */
	sprintf(str,"%s%s.MSG",temp_dir,sys_id);
	remove(str);
	sprintf(str,"%s%s.REP",temp_dir,sys_id);
    remove(str);

	sprintf(str,"%s*.*",temp_dir);
	i=findfirst(str,&ff,0);
	if(!i) {
		sprintf(str,"%sQNET\\%s.IN",data_dir,useron.alias);
		mkdir(str); }
	while(!i) { 						/* Extra files */
		sprintf(str,"%s%s",temp_dir,ff.ff_name);
		sprintf(fname,"%sQNET\\%s.IN\\%s",data_dir,useron.alias,ff.ff_name);
		mv(str,fname,1);
		sprintf(str,text[ReceivedFileViaQWK],ff.ff_name,useron.alias);
		putsmsg(1,str);
		i=findnext(&ff); } }

bputs(text[QWKUnpacked]);
CRLF;
/**********************************************/
/* Hang-up now if that's what the user wanted */
/**********************************************/
autohangup();

}
