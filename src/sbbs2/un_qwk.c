#line 1 "UN_QWK.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "qwk.h"

/****************************************************************************/
/* Unpacks .QWK packet, hubnum is the number of the QWK net hub 			*/
/****************************************************************************/
void unpack_qwk(char *packet,uint hubnum)
{
	uchar	str[256],fname[128],block[128],ch;
	int 	k,file;
	uint	i,j,n,msgs,lastsub=INVALID_SUB;
	long	l,size,misc;
	time_t	t;
    struct  ffblk ff;
	FILE	*qwk;

useron.number=1;
getuserdat(&useron);
console=CON_L_ECHO;
i=external(cmdstr(qhub[hubnum]->unpack,packet,"*.*",NULL),EX_OUTL);
if(i) {
	errormsg(WHERE,ERR_EXEC,cmdstr(qhub[hubnum]->unpack,packet,"*.*",NULL),i);
	return; }
sprintf(str,"%sMESSAGES.DAT",temp_dir);
if(!fexist(str)) {
	sprintf(str,"%s.QWK doesn't contain MESSAGES.DAT",qhub[hubnum]->id);
	errorlog(str);
	return; }
if((qwk=fnopen(&file,str,O_RDONLY))==NULL) {
	errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
	return; }
size=filelength(file);
/********************/
/* Process messages */
/********************/
bputs(text[QWKUnpacking]);

for(l=128;l<size;l+=i*128) {
	fseek(qwk,l,SEEK_SET);
	fread(block,128,1,qwk);
	sprintf(tmp,"%.6s",block+116);
	i=atoi(tmp);  /* i = number of 128 byte records */
	if(i<2) {
		i=1;
		continue; }
	/*********************************/
	/* public message on a sub-board */
	/*********************************/
	n=(uint)block[123]|(((uint)block[124])<<8);  /* conference number */

	if(!n) {		/* NETMAIL */
		sprintf(str,"%25.25s",block+21);
		truncsp(str);
		if(!stricmp(str,"NETMAIL")) {  /* QWK to FidoNet NetMail */
			qwktonetmail(qwk,block,NULL,hubnum+1);
            continue; }
		if(strchr(str,'@')) {
			qwktonetmail(qwk,block,str,hubnum+1);
            continue; }
		j=atoi(str);
		if(j && j>lastuser())
			j=0;
		if(!j && !stricmp(str,"SYSOP"))
			j=1;
		if(!j)
			j=matchuser(str);
		if(!j && !stricmp(str,sys_id))
			j=1;
		if(!j) {
			bputs(text[UnknownUser]);
            continue; }

		getuserrec(j,U_MISC,8,str);
		misc=ahtoul(str);
		if(misc&NETMAIL && sys_misc&SM_FWDTONET) {
			getuserrec(j,U_NETMAIL,LEN_NETMAIL,str);
			qwktonetmail(qwk,block,str,hubnum+1);
            continue; }

		smb_stack(&smb,SMB_STACK_PUSH);
		sprintf(smb.file,"%sMAIL",data_dir);
		smb.retry_time=smb_retry_time;
		if((k=smb_open(&smb))!=0) {
			errormsg(WHERE,ERR_OPEN,smb.file,k);
			smb_stack(&smb,SMB_STACK_POP);
			continue; }
		if(!filelength(fileno(smb.shd_fp))) {
			smb.status.max_crcs=mail_maxcrcs;
			smb.status.max_msgs=MAX_SYSMAIL;
			smb.status.max_age=mail_maxage;
			smb.status.attr=SMB_EMAIL;
			if((k=smb_create(&smb))!=0) {
				smb_close(&smb);
				errormsg(WHERE,ERR_CREATE,smb.file,k);
				smb_stack(&smb,SMB_STACK_POP);
				continue; } }
		if((k=smb_locksmbhdr(&smb))!=0) {
			smb_close(&smb);
			errormsg(WHERE,ERR_LOCK,smb.file,k);
			smb_stack(&smb,SMB_STACK_POP);
			continue; }
		if((k=smb_getstatus(&smb))!=0) {
			smb_close(&smb);
			errormsg(WHERE,ERR_READ,smb.file,k);
			smb_stack(&smb,SMB_STACK_POP);
            continue; }
		smb_unlocksmbhdr(&smb);
		qwktomsg(qwk,block,hubnum+1,INVALID_SUB,j);
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		sprintf(tmp,"%-25.25s",block+46);
		truncsp(tmp);
		sprintf(str,text[UserSentYouMail],tmp);
		putsmsg(j,str);
		continue;
		}

	for(j=0;j<qhub[hubnum]->subs;j++)
		if(qhub[hubnum]->conf[j]==n)
			break;
	if(j>=qhub[hubnum]->subs)	/* ignore messages for subs not in config */
		continue;

	j=qhub[hubnum]->sub[j];

	if(j!=lastsub) {
		if(lastsub!=INVALID_SUB)
			smb_close(&smb);
		lastsub=INVALID_SUB;
		sprintf(smb.file,"%s%s",sub[j]->data_dir,sub[j]->code);
		smb.retry_time=smb_retry_time;
		if((k=smb_open(&smb))!=0) {
			errormsg(WHERE,ERR_OPEN,smb.file,k);
			continue; }
		if(!filelength(fileno(smb.shd_fp))) {
			smb.status.max_crcs=sub[j]->maxcrcs;
			smb.status.max_msgs=sub[j]->maxmsgs;
			smb.status.max_age=sub[j]->maxage;
			smb.status.attr=sub[j]->misc&SUB_HYPER ? SMB_HYPERALLOC :0;
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
		lastsub=j; }

	if(!qwktomsg(qwk,block,hubnum+1,j,0))
		continue;

	if(sub[j]->misc&SUB_FIDO && sub[j]->echomail_sem[0]) /* update semaphore */
		if((file=nopen(sub[j]->echomail_sem,O_WRONLY|O_CREAT|O_TRUNC))!=-1)
			close(file);
	bprintf(text[Posted],grp[sub[j]->grp]->sname,sub[j]->lname); }

update_qwkroute(NULL);		/* Write ROUTE.DAT */

fclose(qwk);
if(lastsub!=INVALID_SUB)
	smb_close(&smb);

delfiles(temp_dir,"*.NDX");
sprintf(str,"%sMESSAGES.DAT",temp_dir);
remove(str);
sprintf(str,"%sDOOR.ID",temp_dir);
remove(str);
sprintf(str,"%sCONTROL.DAT",temp_dir);
remove(str);
sprintf(str,"%sNETFLAGS.DAT",temp_dir);
remove(str);

sprintf(str,"%s*.*",temp_dir);
i=findfirst(str,&ff,0);
if(!i) {
	sprintf(str,"%sQNET\\%s.IN",data_dir,qhub[hubnum]->id);
	mkdir(str); }
while(!i) {
	sprintf(str,"%s%s",temp_dir,ff.ff_name);
	sprintf(fname,"%sQNET\\%s.IN\\%s",data_dir,qhub[hubnum]->id,ff.ff_name);
	mv(str,fname,1);
	sprintf(str,text[ReceivedFileViaQWK],ff.ff_name,qhub[hubnum]->id);
	putsmsg(1,str);
	i=findnext(&ff); }

bputs(text[QWKUnpacked]);
CRLF;
remove(packet);

}
