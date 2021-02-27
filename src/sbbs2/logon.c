#line 1 "LOGON.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/****************************************************************************/
/* Functions pertaining to the logging on and off of a user					*/
/****************************************************************************/

#include "sbbs.h"
#include "cmdshell.h"

extern char qwklogon,onquiet;

/****************************************************************************/
/* Called once upon each user logging on the board							*/
/* Returns 1 if user passed logon, 0 if user failed.						*/
/****************************************************************************/
char logon()
{
	char	str[256],c;
	int 	file;
	uint	i,j,k,mailw;
	ulong	totallogons;
	node_t	node;
	FILE	*stream;
	struct tm *gm;

now=time(NULL);
gm=localtime(&now);
if(!useron.number)
	return(0);

if(useron.rest&FLAG('Q'))
    qwklogon=1;
if(SYSOP && ((online==ON_REMOTE && !(sys_misc&SM_R_SYSOP))
    || (online==ON_LOCAL && !(sys_misc&SM_L_SYSOP))))
	return(0);
if(cur_rate<node_minbps && !(useron.exempt&FLAG('M'))) {
    bprintf(text[MinimumModemSpeed],node_minbps);
    sprintf(str,"%sTOOSLOW.MSG",text_dir);
    if(fexist(str))
        printfile(str,0);
    sprintf(str,"(%04u)  %-25s  Modem speed: %u<%u"
        ,useron.number,useron.alias,cur_rate,node_minbps);
    logline("+!",str);
	return(0); }

if(useron.rest&FLAG('G')) {     /* Guest account */
    useron.misc=(new_misc&(~ASK_NSCAN));
	useron.rows=0;
	useron.misc&=~(ANSI|RIP|WIP|NO_EXASCII|COLOR);
    useron.misc|=autoterm;
    if(!(useron.misc&ANSI) && yesno(text[AnsiTerminalQ]))
        useron.misc|=ANSI;
	if(useron.misc&(RIP|WIP)
		|| (useron.misc&ANSI && yesno(text[ColorTerminalQ])))
        useron.misc|=COLOR;
	if(!yesno(text[ExAsciiTerminalQ]))
		useron.misc|=NO_EXASCII;
    for(i=0;i<total_xedits;i++)
        if(!stricmp(xedit[i]->code,new_xedit)
            && chk_ar(xedit[i]->ar,useron))
            break;
    if(i<total_xedits)
        useron.xedit=i+1;
    else
        useron.xedit=0;
	useron.prot=new_prot;
    useron.shell=new_shell; }

if(node_dollars_per_call) {
    adjustuserrec(useron.number,U_CDT,10
        ,cdt_per_dollar*node_dollars_per_call);
    bprintf(text[CreditedAccount]
        ,cdt_per_dollar*node_dollars_per_call);
    sprintf(str,"%s #%u was billed $%d T: %u seconds"
        ,useron.alias,useron.number
        ,node_dollars_per_call,(uint)now-answertime);
    logline("$+",str);
    hangup();
	return(0); }

//lclini(node_scrnlen-1);

if(!chk_ar(node_ar,useron)) {
    bputs(text[NoNodeAccess]);
    sprintf(str,"(%04u)  %-25s  Insufficient node access"
        ,useron.number,useron.alias);
    logline("+!",str);
	return(0); }

getnodedat(node_num,&thisnode,1);
if(thisnode.misc&NODE_LOCK) {
	putnodedat(node_num,thisnode);	/* must unlock! */
    if(!SYSOP && !(useron.exempt&FLAG('N'))) {
        bputs(text[NodeLocked]);
        sprintf(str,"(%04u)  %-25s  Locked node logon attempt"
            ,useron.number,useron.alias);
        logline("+!",str);
		return(0); }
    if(yesno(text[RemoveNodeLockQ])) {
		getnodedat(node_num,&thisnode,1);
        logline("S-","Removed Node Lock");
		thisnode.misc&=~NODE_LOCK; }
	else
		getnodedat(node_num,&thisnode,1); }

if(onquiet || (useron.exempt&FLAG('Q') && useron.misc&QUIET))
    thisnode.status=NODE_QUIET;
else
    thisnode.status=NODE_INUSE;
onquiet=0;
action=thisnode.action=NODE_LOGN;
if(online==ON_LOCAL)
    thisnode.connection=0;
else
    thisnode.connection=cur_rate;
thisnode.misc&=~(NODE_ANON|NODE_INTR|NODE_MSGW|NODE_POFF|NODE_AOFF);
if(useron.chat&CHAT_NOACT)
    thisnode.misc|=NODE_AOFF;
if(useron.chat&CHAT_NOPAGE)
    thisnode.misc|=NODE_POFF;
thisnode.useron=useron.number;
putnodedat(node_num,thisnode);

getusrsubs();
getusrdirs();

if(useron.misc&CURSUB && !(useron.rest&FLAG('G'))) {
    for(i=0;i<usrgrps;i++) {
		for(j=0;j<usrsubs[i];j++) {
			if(!strcmp(sub[usrsub[i][j]]->code,useron.cursub))
				break; }
        if(j<usrsubs[i]) {
            curgrp=i;
            cursub[i]=j;
            break; } }
	for(i=0;i<usrlibs;i++) {
        for(j=0;j<usrdirs[i];j++)
			if(!strcmp(dir[usrdir[i][j]]->code,useron.curdir))
                break;
        if(j<usrdirs[i]) {
            curlib=i;
            curdir[i]=j;
            break; } } }


if(useron.misc&AUTOTERM) {
    useron.misc&=~(ANSI|RIP|WIP);
    useron.misc|=autoterm; }

if(!chk_ar(shell[useron.shell]->ar,useron)) {
    useron.shell=new_shell;
    if(!chk_ar(shell[useron.shell]->ar,useron)) {
        for(i=0;i<total_shells;i++)
            if(chk_ar(shell[i]->ar,useron))
                break;
        if(i==total_shells)
            useron.shell=0; } }

statline=sys_def_stat;
statusline();
logon_ml=useron.level;
logontime=time(NULL);
starttime=logontime;
last_ns_time=ns_time=useron.ns_time;
// ns_time-=(useron.tlast*60); /* file newscan time == last logon time */
delfiles(temp_dir,"*.*");
sprintf(str,"%sMSGS\\N%3.3u.MSG",data_dir,node_num);
remove(str);            /* remove any pending node messages */
sprintf(str,"%sMSGS\\N%3.3u.IXB",data_dir,node_num);
remove(str);			/* remove any pending node message indices */

if(!SYSOP && online==ON_REMOTE) {
	rioctl(IOCM|ABORT);	/* users can't abort anything */
	rioctl(IOCS|ABORT); }

CLS;
if(useron.rows)
	rows=useron.rows;
else if(online==ON_LOCAL)
	rows=node_scrnlen-1;
unixtodstr(logontime,str);
if(!strncmp(str,useron.birth,5) && !(useron.rest&FLAG('Q'))) {
	bputs(text[HappyBirthday]);
	pause();
	CLS;
	user_event(EVENT_BIRTHDAY); }
unixtodstr(useron.laston,tmp);
if(strcmp(str,tmp)) {			/* str still equals logon time */
	useron.ltoday=1;
	useron.ttoday=useron.etoday=useron.ptoday=useron.textra=0;
	useron.freecdt=level_freecdtperday[useron.level]; }
else
	useron.ltoday++;

gettimeleft();
sprintf(str,"%sFILE\\%04u.DWN",data_dir,useron.number);
batch_add_list(str);
if(!qwklogon) { 	 /* QWK Nodes don't go through this */

	if(sys_pwdays
		&& logontime>(useron.pwmod+((ulong)sys_pwdays*24UL*60UL*60UL))) {
		bprintf(text[TimeToChangePw],sys_pwdays);

		c=0;
		while(c<LEN_PASS) { 				/* Create random password */
			str[c]=random(43)+48;
			if(isalnum(str[c]))
				c++; }
		str[c]=0;
		bprintf(text[YourPasswordIs],str);

		if(sys_misc&SM_PWEDIT && yesno(text[NewPasswordQ]))
			while(online) {
				bputs(text[NewPassword]);
				getstr(str,LEN_PASS,K_UPPER|K_LINE);
				truncsp(str);
				if(chkpass(str,useron))
					break;
				CRLF; }

		while(online) {
			if(sys_misc&SM_PWEDIT) {
				CRLF;
				bputs(text[VerifyPassword]); }
			else
				bputs(text[NewUserPasswordVerify]);
			console|=CON_R_ECHOX;
			if(!(sys_misc&SM_ECHO_PW))
				console|=CON_L_ECHOX;
			getstr(tmp,LEN_PASS,K_UPPER);
			console&=~(CON_R_ECHOX|CON_L_ECHOX);
			if(strcmp(str,tmp)) {
				bputs(text[Wrong]);
				continue; }
			break; }
		strcpy(useron.pass,str);
		useron.pwmod=time(NULL);
		putuserrec(useron.number,U_PWMOD,8,ultoa(useron.pwmod,str,16));
		bputs(text[PasswordChanged]);
		pause(); }
	if(useron.ltoday>level_callsperday[useron.level]
		&& !(useron.exempt&FLAG('L'))) {
		bputs(text[NoMoreLogons]);
		sprintf(str,"(%04u)  %-25s  Out of logons"
			,useron.number,useron.alias);
		logline("+!",str);
		hangup();
		return(0); }
	if(useron.rest&FLAG('L') && useron.ltoday>1) {
		bputs(text[R_Logons]);
		sprintf(str,"(%04u)  %-25s  Out of logons"
			,useron.number,useron.alias);
		logline("+!",str);
		hangup();
		return(0); }
	if(!useron.name[0] && ((uq&UQ_ALIASES && uq&UQ_REALNAME)
		|| uq&UQ_COMPANY))
		while(online) {
			if(uq&UQ_ALIASES && uq&UQ_REALNAME)
				bputs(text[EnterYourRealName]);
			else
				bputs(text[EnterYourCompany]);
			getstr(useron.name,LEN_NAME,K_UPRLWR|(uq&UQ_NOEXASC));
			if(uq&UQ_ALIASES && uq&UQ_REALNAME) {
				if(trashcan(useron.name,"NAME") || !useron.name[0]
					|| !strchr(useron.name,SP)
					|| strchr(useron.name,0xff)
					|| (uq&UQ_DUPREAL
						&& userdatdupe(useron.number,U_NAME,LEN_NAME
						,useron.name,0)))
					bputs(text[YouCantUseThatName]);
				else
					break; }
			else
				break; }
	if(uq&UQ_HANDLE && !useron.handle[0]) {
		sprintf(useron.handle,"%.*s",LEN_HANDLE,useron.alias);
		while(online) {
			bputs(text[EnterYourHandle]);
			if(!getstr(useron.handle,LEN_HANDLE
				,K_LINE|K_EDIT|K_AUTODEL|(uq&UQ_NOEXASC))
				|| strchr(useron.handle,0xff)
				|| (uq&UQ_DUPHAND
					&& userdatdupe(useron.number,U_HANDLE,LEN_HANDLE
					,useron.handle,0))
				|| trashcan(useron.handle,"NAME"))
				bputs(text[YouCantUseThatName]);
			else
				break; } }
	if(uq&UQ_LOCATION && !useron.location[0])
		while(online) {
			bputs(text[EnterYourCityState]);
			if(getstr(useron.location,LEN_LOCATION,K_UPRLWR|(uq&UQ_NOEXASC)))
				break; }
	if(uq&UQ_ADDRESS && !useron.address[0])
		while(online) {
			bputs(text[EnterYourAddress]);
			if(getstr(useron.address,LEN_ADDRESS,K_UPRLWR|(uq&UQ_NOEXASC)))
				break; }
	if(uq&UQ_ADDRESS && !useron.zipcode[0])
		while(online) {
			bputs(text[EnterYourZipCode]);
			if(getstr(useron.zipcode,LEN_ZIPCODE,K_UPPER|(uq&UQ_NOEXASC)))
				break; }
	if(uq&UQ_PHONE && !useron.phone[0]) {
		i=yesno(text[CallingFromNorthAmericaQ]);
		while(online) {
			bputs(text[EnterYourPhoneNumber]);
			if(i) {
				if(gettmplt(useron.phone,sys_phonefmt
					,K_LINE|(uq&UQ_NOEXASC))<strlen(sys_phonefmt))
                     continue; }
			else {
				if(getstr(useron.phone,LEN_PHONE
					,K_UPPER|(uq&UQ_NOEXASC))<5)
					continue; }
			if(!trashcan(useron.phone,"PHONE"))
				break; } }
	if(uq&UQ_COMP && !useron.comp[0])
		getcomputer(useron.comp);
	if(new_sif[0]) {
		sprintf(str,"%sUSER\\%4.4u.DAT",data_dir,useron.number);
		if(flength(str)<1L)
			create_sif_dat(new_sif,str); } }
if(!online) {
	sprintf(str,"(%04u)  %-25s  Unsuccessful logon"
		,useron.number,useron.alias);
    logline("+!",str);
	return(0); }
strcpy(useron.modem,connection);
useron.logons++;
putuserdat(useron);
getmsgptrs();
sys_status|=SS_USERON;          /* moved from further down */

if(useron.rest&FLAG('Q')) {
	sprintf(str,"(%04u)  %-25s  QWK Network Connection"
		,useron.number,useron.alias);
	logline("++",str);
	return(1); }

/********************/
/* SUCCESSFUL LOGON */
/********************/
totallogons=logonstats();
sprintf(str,"(%04u)  %-25s  Logon %lu - %u"
	,useron.number,useron.alias,totallogons,useron.ltoday);
logline("++",str);

if(!qwklogon && logon_mod[0])
	exec_bin(logon_mod,&main_csi);

if(thisnode.status!=NODE_QUIET) {
    sprintf(str,"%sLOGON.LST",data_dir);
    if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
        errormsg(WHERE,ERR_OPEN,str,O_RDWR|O_CREAT|O_APPEND);
        return(0); }
    sprintf(str,text[LastFewCallersFmt],node_num
        ,totallogons,useron.alias
        ,sys_misc&SM_LISTLOC ? useron.location : useron.note
        ,gm->tm_hour,gm->tm_min
        ,connection,useron.ltoday);
    write(file,str,strlen(str));
    close(file); }

if(sys_logon[0])				/* execute system logon event */
	external(cmdstr(sys_logon,nulstr,nulstr,NULL),EX_OUTR|EX_OUTL); /* EX_CC */

if(qwklogon)
    return(1);

sys_status|=SS_PAUSEON;	/* always force pause on during this section */
main_cmds=xfer_cmds=posts_read=0;
mailw=getmail(useron.number,0);
bprintf(text[SiSysName],sys_name);
bprintf(text[SiNodeNumberName],node_num,node_name);
bprintf(text[LiUserNumberName],useron.number,useron.alias);
bprintf(text[LiLogonsToday],useron.ltoday
	,level_callsperday[useron.level]);
bprintf(text[LiTimeonToday],useron.ttoday
	,level_timeperday[useron.level]+useron.min);
bprintf(text[LiMailWaiting],mailw);
strcpy(str,text[LiSysopIs]);
i=kbd_state();		 /* Check scroll lock */
if(i&16 || (sys_chat_ar[0] && chk_ar(sys_chat_ar,useron)))
	strcat(str,text[LiSysopAvailable]);
else
	strcat(str,text[LiSysopNotAvailable]);
bprintf("%s\r\n\r\n",str);
if(sys_status&SS_EVENT)
	bputs(text[ReducedTime]);
getnodedat(node_num,&thisnode,1);
thisnode.misc&=~(NODE_AOFF|NODE_POFF);
if(useron.chat&CHAT_NOACT)
    thisnode.misc|=NODE_AOFF;
if(useron.chat&CHAT_NOPAGE)
    thisnode.misc|=NODE_POFF;
putnodedat(node_num,thisnode);
getsmsg(useron.number); 		/* Moved from further down */
SYNC;
c=0;
for(i=1;i<=sys_nodes;i++)
	if(i!=node_num) {
		getnodedat(i,&node,0);
		if(node.status==NODE_INUSE
			|| ((node.status==NODE_QUIET || node.errors) && SYSOP)) {
			if(!c)
				bputs(text[NodeLstHdr]);
			printnodedat(i,node);
			c=1; }
		if(node.status==NODE_INUSE && i!=node_num && node.useron==useron.number
			&& !SYSOP && !(useron.exempt&FLAG('G'))) {
			strcpy(tmp,"On two nodes at the same time");
			sprintf(str,"(%04u)  %-25s  %s"
				,useron.number,useron.alias,tmp);
			logline("+!",str);
			errorlog(tmp);
			bputs(text[UserOnTwoNodes]);
			hangup();
			return(0); }
		if(thisnode.status!=NODE_QUIET
			&& (node.status==NODE_INUSE || node.status==NODE_QUIET)
			&& !(node.misc&NODE_AOFF) && node.useron!=useron.number) {
			sprintf(str,text[NodeLoggedOnAtNbps]
				,node_num
				,thisnode.misc&NODE_ANON ? text[UNKNOWN_USER] : useron.alias
				,connection);
			putnmsg(i,str); } }

if(sys_exp_warn && useron.expire && useron.expire>now /* Warn user of coming */
	&& (useron.expire-now)/(1440L*60L)<=sys_exp_warn) /* expiration */
	bprintf(text[AccountWillExpireInNDays],(useron.expire-now)/(1440L*60L));

if(criterrs && SYSOP)
	bprintf(text[CriticalErrors],criterrs);
if((i=getuserxfers(0,useron.number,0))!=0) {
	bprintf(text[UserXferForYou],i,i>1 ? "s" : nulstr); }
if((i=getuserxfers(useron.number,0,0))!=0) {
	bprintf(text[UnreceivedUserXfer],i,i>1 ? "s" : nulstr); }
SYNC;
sys_status&=~SS_PAUSEON;	/* Turn off the pause override flag */
if(online==ON_REMOTE)
	rioctl(IOSM|ABORT);		/* Turn abort ability on */
if(mailw) {
	if(yesno(text[ReadYourMailNowQ]))
		readmail(useron.number,MAIL_YOUR); }
lastnodemsg=0;
if(useron.misc&ASK_NSCAN && yesno(text[NScanAllGrpsQ]))
    scanallsubs(SCAN_NEW);
if(useron.misc&ASK_SSCAN && yesno(text[SScanAllGrpsQ]))
    scanallsubs(SCAN_TOYOU);
return(1);
}

/****************************************************************************/
/* Checks the system DSTS.DAB to see if it is a new day, if it is, all the  */
/* nodes' and the system's CSTS.DAB are added to, and the DSTS.DAB's daily  */
/* stats are cleared. Also increments the logon values in DSTS.DAB if       */
/* applicable.                                                              */
/****************************************************************************/
ulong logonstats()
{
    char str[256];
    int dsts,csts;
    uint i;
    struct date update;
    time_t update_t;
    stats_t stats;
    node_t node;

sprintf(str,"%sDSTS.DAB",ctrl_dir);
if((dsts=nopen(str,O_RDWR))==-1) {
    errormsg(WHERE,ERR_OPEN,str,O_RDWR);
    return(0L); }
read(dsts,&update_t,4);         /* Last updated         */
read(dsts,&stats.logons,4);     /* Total number of logons on system */
close(dsts);
if(update_t>now+(24L*60L*60L)) /* More than a day in the future? */
    errormsg(WHERE,ERR_CHK,"Daily stats time stamp",update_t);
unixtodos(update_t,&update,&curtime);
unixtodos(now,&date,&curtime);
if((date.da_day>update.da_day && date.da_mon==update.da_mon)
    || date.da_mon>update.da_mon || date.da_year>update.da_year) {

    sprintf(str,"New Day - Prev: %s ",timestr(&update_t));
    logentry("!=",str);

    sys_status|=SS_DAILY;       /* New Day !!! */
    sprintf(str,"%sLOGON.LST",data_dir);    /* Truncate logon list */
    if((dsts=nopen(str,O_TRUNC|O_CREAT|O_WRONLY))==-1) {
        errormsg(WHERE,ERR_OPEN,str,O_TRUNC|O_CREAT|O_WRONLY);
        return(0L); }
    close(dsts);
    for(i=0;i<=sys_nodes;i++) {
        if(i) {     /* updating a node */
            getnodedat(i,&node,1);
            node.misc|=NODE_EVENT;
            putnodedat(i,node); }
        sprintf(str,"%sDSTS.DAB",i ? node_path[i-1] : ctrl_dir);
        if((dsts=nopen(str,O_RDWR))==-1) /* node doesn't have stats yet */
            continue;
        sprintf(str,"%sCSTS.DAB",i ? node_path[i-1] : ctrl_dir);
        if((csts=nopen(str,O_WRONLY|O_APPEND|O_CREAT))==-1) {
            close(dsts);
            errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_APPEND|O_CREAT);
            continue; }
        lseek(dsts,8L,SEEK_SET);        /* Skip time and logons */
        write(csts,&now,4);
        read(dsts,&stats.ltoday,4);
        write(csts,&stats.ltoday,4);
        lseek(dsts,4L,SEEK_CUR);        /* Skip total time on */
        read(dsts,&stats.ttoday,4);
        write(csts,&stats.ttoday,4);
        read(dsts,&stats.uls,4);
        write(csts,&stats.uls,4);
        read(dsts,&stats.ulb,4);
        write(csts,&stats.ulb,4);
        read(dsts,&stats.dls,4);
        write(csts,&stats.dls,4);
        read(dsts,&stats.dlb,4);
        write(csts,&stats.dlb,4);
        read(dsts,&stats.ptoday,4);
        write(csts,&stats.ptoday,4);
        read(dsts,&stats.etoday,4);
        write(csts,&stats.etoday,4);
        read(dsts,&stats.ftoday,4);
        write(csts,&stats.ftoday,4);
        close(csts);
        lseek(dsts,0L,SEEK_SET);        /* Go back to beginning */
        write(dsts,&now,4);             /* Update time stamp  */
        lseek(dsts,4L,SEEK_CUR);        /* Skip total logons */
        stats.ltoday=0;
        write(dsts,&stats.ltoday,4);  /* Logons today to 0 */
        lseek(dsts,4L,SEEK_CUR);     /* Skip total time on */
        stats.ttoday=0;              /* Set all other today variables to 0 */
        write(dsts,&stats.ttoday,4);        /* Time on today to 0 */
        write(dsts,&stats.ttoday,4);        /* Uploads today to 0 */
        write(dsts,&stats.ttoday,4);        /* U/L Bytes today    */
        write(dsts,&stats.ttoday,4);        /* Download today     */
        write(dsts,&stats.ttoday,4);        /* Download bytes     */
        write(dsts,&stats.ttoday,4);        /* Posts today        */
        write(dsts,&stats.ttoday,4);        /* Emails today       */
        write(dsts,&stats.ttoday,4);        /* Feedback today     */
        write(dsts,&stats.ttoday,2);        /* New users Today    */
        close(dsts); } }

if(thisnode.status==NODE_QUIET)       /* Quiet users aren't counted */
    return(0L);

for(i=0;i<2;i++) {
    sprintf(str,"%sDSTS.DAB",i ? ctrl_dir : node_dir);
    if((dsts=nopen(str,O_RDWR))==-1) {
        errormsg(WHERE,ERR_OPEN,str,O_RDWR);
        return(0L); }
    lseek(dsts,4L,SEEK_SET);        /* Skip time stamp */
    read(dsts,&stats.logons,4);
    read(dsts,&stats.ltoday,4);
    stats.logons++;
    stats.ltoday++;
    lseek(dsts,4L,SEEK_SET);        /* Rewind back and overwrite */
    write(dsts,&stats.logons,4);
    write(dsts,&stats.ltoday,4);
    close(dsts); }
return(stats.logons);
}


