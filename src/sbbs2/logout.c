#line 1 "LOGOUT.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "cmdshell.h"

/****************************************************************************/
/* Function that is called after a user hangs up or logs off				*/
/****************************************************************************/
void logout()
{
	char	str[256];
	int 	done,i,j;
	ushort	ttoday;
	file_t	f;
	node_t	node;
	struct	date logondate;
	struct	time lt;
	FILE	*stream;

now=time(NULL);
unixtodos(now,&date,&curtime);

if(!useron.number) {				 /* Not logged in, so do nothing */
	if(!online) {
		sprintf(str,"%02d:%02d%c  T:%3u sec\r\n"
			,curtime.ti_hour>12 ? curtime.ti_hour-12
			: curtime.ti_hour==0 ? 12 : curtime.ti_hour, curtime.ti_min
			, curtime.ti_hour>=12 ? 'p' : 'a'
			,(uint)(now-answertime));
		logline("@-",str); }
	return; }

strcpy(lastuseron,useron.alias);	/* for use with WFC status display */

if(useron.rest&FLAG('G')) {        /* Reset guest's msg scan cfg */
	putuserrec(useron.number,U_NAME,LEN_NAME,nulstr);
	for(i=0;i<total_subs;i++) {
		if(sub[i]->misc&SUB_NSDEF)
			sub[i]->misc|=SUB_NSCAN;
		else
			sub[i]->misc&=~SUB_NSCAN;
		if(sub[i]->misc&SUB_SSDEF)
			sub[i]->misc|=SUB_SSCAN;
		else
			sub[i]->misc&=~SUB_SSCAN; }
	batdn_total=0; }

if(batdn_total) {
	sprintf(str,"%sFILE\\%04u.DWN",data_dir,useron.number);
	if((stream=fnopen(&i,str,O_WRONLY|O_TRUNC|O_CREAT))!=NULL) {
		for(i=0;i<batdn_total;i++)
			fprintf(stream,"%s\r\n",batdn_name[i]);
		fclose(stream); } }
	
if(sys_status&SS_USERON && thisnode.status!=NODE_QUIET 
	&& !(useron.rest&FLAG('Q')))
	for(i=1;i<=sys_nodes;i++)
		if(i!=node_num) {
			getnodedat(i,&node,0);
			if((node.status==NODE_INUSE || node.status==NODE_QUIET)
				&& !(node.misc&NODE_AOFF) && node.useron!=useron.number) {
				sprintf(str,text[NodeLoggedOff],node_num
					,thisnode.misc&NODE_ANON
					? text[UNKNOWN_USER] : useron.alias);
				putnmsg(i,str); } }

if(!online) {		/* NOT re-login */

	getnodedat(node_num,&thisnode,1);
	thisnode.status=NODE_WFC;
	thisnode.misc&=~(NODE_INTR|NODE_MSGW|NODE_NMSG
		|NODE_UDAT|NODE_POFF|NODE_AOFF|NODE_EXT);
	putnodedat(node_num,thisnode);

	if(sys_status&SS_SYSALERT) {
		mswait(500);
		if(com_port)
			dtr(1);
		mswait(500);
		offhook();
		CLS;
		lputs("\r\n\r\nAlerting Sysop...");
		while(!lkbrd(1)) {
			beep(1000,200);
			nosound();
			mswait(200); }
		lkbrd(0); }

	sys_status&=~SS_SYSALERT;
	if(sys_logout[0])		/* execute system logoff event */
		external(cmdstr(sys_logout,nulstr,nulstr,NULL),EX_OUTL);	/* EX_CC */
	}

if(logout_mod[0])
	exec_bin(logout_mod,&main_csi);
backout();
sprintf(str,"%sMSGS\\%4.4u.MSG",data_dir,useron.number);
if(!flength(str))		/* remove any 0 byte message files */
	remove(str);

delfiles(temp_dir,"*.*");
putmsgptrs();
if(!REALSYSOP)
	logofflist();
useron.laston=now;

ttoday=useron.ttoday-useron.textra; 		/* billable time used prev calls */
if(ttoday>=level_timeperday[useron.level])
	i=0;
else
	i=level_timeperday[useron.level]-ttoday;
if(i>level_timepercall[useron.level])      /* i=amount of time without min */
	i=level_timepercall[useron.level];
j=(now-starttime)/60;			/* j=billable time online in min */
if(i<0) i=0;
if(j<0) j=0;

if(useron.min && j>i) {
    j-=i;                               /* j=time to deduct from min */
	sprintf(str,"Minute Adjustment: %d",-j);
	logline(">>",str);
    if(useron.min>j)
        useron.min-=j;
    else
        useron.min=0L;
    putuserrec(useron.number,U_MIN,10,ultoa(useron.min,str,10)); }

useron.tlast=(now-logontime)/60;
useron.timeon+=useron.tlast;
useron.ttoday+=useron.tlast;

if(timeleft>0 && starttime-logontime>0) 			/* extra time */
	useron.textra+=(starttime-logontime)/60;

unixtodos(logontime,&logondate,&lt);
if(logondate.da_day!=date.da_day) { /* date has changed while online */
    putuserrec(useron.number,U_LTODAY,5,"0");
	useron.ttoday=0;			/* so zero logons today and time on today */
	useron.textra=0; }			/* and extra time */



putuserrec(useron.number,U_NS_TIME,8,ultoa(last_ns_time,str,16));
putuserrec(useron.number,U_LASTON,8,ultoa(useron.laston,str,16));
putuserrec(useron.number,U_TIMEON,5,itoa(useron.timeon,str,10));
putuserrec(useron.number,U_TTODAY,5,itoa(useron.ttoday,str,10));
putuserrec(useron.number,U_TEXTRA,5,itoa(useron.textra,str,10));
putuserrec(useron.number,U_TLAST,5,itoa(useron.tlast,str,10));

getusrsubs();
getusrdirs();
putuserrec(useron.number,U_CURSUB,8,sub[usrsub[curgrp][cursub[curgrp]]]->code);
putuserrec(useron.number,U_CURDIR,8,dir[usrdir[curlib][curdir[curlib]]]->code);

sprintf(str,"%02d:%02d%c  ",curtime.ti_hour>12 ? curtime.ti_hour-12
	: curtime.ti_hour==0 ? 12 : curtime.ti_hour, curtime.ti_min
	, curtime.ti_hour>=12 ? 'p' : 'a');
if(sys_status&SS_USERON)
	sprintf(tmp,"T:%3u   R:%3lu   P:%3lu   E:%3lu   F:%3lu   "
		"U:%3luk %lu   D:%3luk %lu"
		,(uint)(now-logontime)/60,posts_read,logon_posts
		,logon_emails,logon_fbacks,logon_ulb/1024UL,logon_uls
		,logon_dlb/1024UL,logon_dls);
else
	sprintf(tmp,"T:%3u sec",(uint)(now-answertime));
strcat(str,tmp);
strcat(str,"\r\n");
logline("@-",str);
sys_status&=~SS_USERON;
answertime=now; // Incase we're relogging on
}

/****************************************************************************/
/* Backout of transactions and statuses for this node 						*/
/****************************************************************************/
void backout()
{
	char str[256],code[128],*buf;
	int i,file;
	long length,l;
	file_t f;

sprintf(str,"%sBACKOUT.DAB",node_dir);
if(flength(str)<1L) {
	remove(str);
	return; }
if((file=nopen(str,O_RDONLY))==-1) {
	errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
	return; }
length=filelength(file);
if((buf=MALLOC(length))==NULL) {
	close(file);
	errormsg(WHERE,ERR_ALLOC,str,length);
	return; }
if(read(file,buf,length)!=length) {
	close(file);
	FREE(buf);
	errormsg(WHERE,ERR_READ,str,length);
	return; }
close(file);
for(l=0;l<length;l+=BO_LEN) {
	switch(buf[l]) {
		case BO_OPENFILE:	/* file left open */
			memcpy(code,buf+l+1,8);
			code[8]=0;
			for(i=0;i<total_dirs;i++)			/* search by code */
				if(!stricmp(dir[i]->code,code))
					break;
			if(i<total_dirs) {		/* found internal code */
				f.dir=i;
				memcpy(&f.datoffset,buf+l+9,4);
				closefile(f); }
			break;
		default:
			errormsg(WHERE,ERR_CHK,str,buf[l]); } }
FREE(buf);
remove(str);	/* always remove the backout file */
}

/****************************************************************************/
/* Detailed usage stats for each logon                                      */
/****************************************************************************/
void logofflist()
{
    char str[256];
    int file;
    struct time ontime;

unixtodos(logontime,&date,&ontime);
sprintf(str,"%sLOGS\\%2.2d%2.2d%2.2d.LOL",data_dir,date.da_mon,date.da_day
    ,TM_YEAR(date.da_year-1900));
if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
    errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_APPEND);
    return; }
unixtodos(now,&date,&curtime);
sprintf(str,"%-*.*s %-2d %-8.8s %2.2d:%2.2d %2.2d:%2.2d %3d%3ld%3ld%3ld%3ld"
	"%3ld%3ld\r\n",LEN_ALIAS,LEN_ALIAS,useron.alias,node_num,connection
    ,ontime.ti_hour,ontime.ti_min,curtime.ti_hour,curtime.ti_min
    ,(int)(now-logontime)/60,posts_read,logon_posts,logon_emails
    ,logon_fbacks,logon_uls,logon_dls);
write(file,str,strlen(str));
close(file);
}
