#line 1 "MAIN_WFC.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "etext.h"
#include "cmdshell.h"
#include "qwk.h"

extern char onquiet,nmi,llo,qwklogon;
extern char term_ret;
extern ulong connect_rate;	 /* already connected at xbps */
extern char *wday[],*mon[];
extern char *hungupstr;
extern char cidarg[];

void reset_logon_vars(void)
{
	int i;

sys_status&=~(SS_USERON|SS_TMPSYSOP|SS_LCHAT|SS_ABORT
    |SS_PAUSEON|SS_PAUSEOFF|SS_EVENT|SS_NEWUSER|SS_NEWDAY);
keybufbot=keybuftop=lbuflen=slcnt=altul=timeleft_warn=0;
logon_uls=logon_ulb=logon_dls=logon_dlb=0;
logon_posts=logon_emails=logon_fbacks=0;
posts_read=0;
batdn_total=batup_total=0;
usrgrps=usrlibs=0;
curgrp=curlib=0;
for(i=0;i<total_libs;i++)
	curdir[i]=0;
for(i=0;i<total_grps;i++)
	cursub[i]=0;
}

void mail_maint(void)
{
	int i;

lprintf("\r\n\r\nPurging deleted/expired e-mail...");
sprintf(smb.file,"%sMAIL",data_dir);
smb.retry_time=smb_retry_time;
if((i=smb_open(&smb))!=0)
	errormsg(WHERE,ERR_OPEN,smb.file,i);
else {
	if((i=smb_locksmbhdr(&smb))!=0)
		errormsg(WHERE,ERR_LOCK,smb.file,i);
	else
		delmail(0,MAIL_ALL);
	smb_close(&smb); }
}

/*************************************************************/
/* Returns 0 to reinitialize modem and start WFC cycle again */
/* Returns 1 to continue checking things peacefully 		 */
/*************************************************************/
char wfc_events(time_t lastnodechk)
{
	char	str[256],str2[256],*buf;
	int 	i,j,k,file,ret=1,chunk;
	ulong	l,m;
	user_t	user;
	node_t	node;
	struct	ffblk ff;
	struct	tm *gm;
	struct	date lastdate;

for(i=0;i<total_qhubs;i++) {
	gm=localtime(&now); 	  /* Qnet call out based on time */
	unixtodos(qhub[i]->last,&date,&curtime);
	if(node_num==qhub[i]->node				/* or frequency */
		&& (qhub[i]->last==-1L
		|| ((qhub[i]->freq
			&& (now-qhub[i]->last)/60>qhub[i]->freq)
		|| (qhub[i]->time
			&& (gm->tm_hour*60)+gm->tm_min>=qhub[i]->time
		&& (gm->tm_mday!=date.da_day || gm->tm_mon!=date.da_mon-1)))
		&& qhub[i]->days&(1<<gm->tm_wday))) {
		offhook();
		lputc(FF);
		sprintf(str,"%sQNET\\%s.NOW"
			,data_dir,qhub[i]->id);
		remove(str);					/* Remove semaphore file */
		sprintf(str,"%sQNET\\%s.PTR"
			,data_dir,qhub[i]->id);
		file=nopen(str,O_RDONLY);
		for(j=0;j<qhub[i]->subs;j++) {
			sub[qhub[i]->sub[j]]->ptr=0;
			lseek(file,sub[qhub[i]->sub[j]]->ptridx*4L,SEEK_SET);
			read(file,&sub[qhub[i]->sub[j]]->ptr,4); }
		if(file!=-1)
			close(file);
		if(pack_rep(i)) {
			if((file=nopen(str,O_WRONLY|O_CREAT))==-1)
				errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT);
			else {
				for(j=l=0;j<qhub[i]->subs;j++) {
					while(filelength(file)<sub[qhub[i]->sub[j]]->ptridx*4L)
						write(file,&l,4);		/* initialize ptrs to null */
					lseek(file,sub[qhub[i]->sub[j]]->ptridx*4L,SEEK_SET);
					write(file,&sub[qhub[i]->sub[j]]->ptr,4); }
				close(file); } }
		delfiles(temp_dir,"*.*");

		qhub[i]->last=time(NULL);
		sprintf(str,"%sQNET.DAB",ctrl_dir);
		if((file=nopen(str,O_WRONLY))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY);
			bail(1); }
		lseek(file,sizeof(time_t)*i,SEEK_SET);
		write(file,&qhub[i]->last,sizeof(time_t));
		close(file);

		if(qhub[i]->call[0]) {
			getnodedat(node_num,&thisnode,1);
			thisnode.status=NODE_NETTING;
			putnodedat(node_num,thisnode);
			lputc(FF);
			external(cmdstr(qhub[i]->call,nulstr,nulstr,NULL),EX_SWAP);
			ret=0; }
		for(j=0;j<10;j++) {
			sprintf(str,"%s%s.QW%c",data_dir,qhub[i]->id,j ? (j-1)+'0' : 'K');
			if(fexist(str)) {
				lclini(node_scrnlen-1);
				delfiles(temp_dir,"*.*");
				unpack_qwk(str,i); } }
		lputc(FF); } }

for(i=0;i<total_phubs;i++) {
	gm=localtime(&now); 	  /* PostLink call out based on time */
	unixtodos(phub[i]->last,&date,&curtime);
	if(node_num==phub[i]->node				/* or frequency */
		&& ((phub[i]->freq
			&& (now-phub[i]->last)/60>phub[i]->freq)
		|| (phub[i]->time
			&& (gm->tm_hour*60)+gm->tm_min>=phub[i]->time
		&& (gm->tm_mday!=date.da_day || gm->tm_mon!=date.da_mon-1)))
		&& phub[i]->days&(1<<gm->tm_wday)) {
		offhook();
		lputc(FF);

		phub[i]->last=time(NULL);
		sprintf(str,"%sPNET.DAB",ctrl_dir);
		if((file=nopen(str,O_WRONLY))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY);
			bail(1); }
		lseek(file,sizeof(time_t)*i,SEEK_SET);
		write(file,&phub[i]->last,sizeof(time_t));
		close(file);

		if(phub[i]->call[0]) {
			getnodedat(node_num,&thisnode,1);
			thisnode.status=NODE_NETTING;
			putnodedat(node_num,thisnode);
			lputc(FF);
			external(cmdstr(phub[i]->call,nulstr,nulstr,NULL),EX_SWAP);
			ret=0; } } }

for(i=0;i<total_events;i++) {
	if(!event[i]->node || event[i]->node>sys_nodes)
        continue;
	gm=localtime(&now);
	unixtodos(event[i]->last,&date,&curtime);
	if(event[i]->last==-1
		|| ((gm->tm_hour*60)+gm->tm_min>=event[i]->time
		&& (gm->tm_mday!=date.da_day || gm->tm_mon!=date.da_mon-1)
		&& event[i]->days&(1<<gm->tm_wday))) {

		if(event[i]->misc&EVENT_EXCL) { /* exclusive event */
			offhook();
			lputc(FF);
			if(event[i]->node!=node_num) {
				lprintf("Waiting for node %d to run timed event.\r\n\r\n"
					,event[i]->node);
				lputs("Hit any key to abort wait...");
				getnodedat(node_num,&thisnode,1);
				thisnode.status=NODE_EVENT_LIMBO;
				thisnode.aux=event[i]->node;
				putnodedat(node_num,thisnode);
				lastnodechk=0;	 /* really last event time check */
				while(!lkbrd(0)) {
					mswait(1);
					now=time(NULL);
					if(now-lastnodechk<10)
						continue;
					getnodedat(node_num,&thisnode,0);
					if(thisnode.misc&NODE_DOWN)
						return(0);
					lastnodechk=now;
					sprintf(str,"%sTIME.DAB",ctrl_dir);
					if((file=nopen(str,O_RDONLY))==-1) {
						errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
						event[i]->last=now;
						return(0); }
					lseek(file,(long)i*4L,SEEK_SET);
					read(file,&event[i]->last,sizeof(time_t));
					close(file);
					if(now-event[i]->last<(60*60))	/* event is done */
						break; }
				sprintf(str,"%s%s.NOW",data_dir,event[i]->code);
				remove(str);
				event[i]->last=now;
				ret=0; }
			else {
				lputs("Waiting for all nodes to become inactive before "
					"running timed event.\r\n\r\n");
				lputs("Hit any key to abort wait and run event now...\r\n\r\n");
				getnodedat(node_num,&thisnode,1);
				thisnode.status=NODE_EVENT_WAITING;
				putnodedat(node_num,thisnode);
				lastnodechk=0;
				while(!lkbrd(0)) {
					mswait(1);
					now=time(NULL);
					if(now-lastnodechk<10)
						continue;
					lastnodechk=now;
					getnodedat(node_num,&thisnode,0);
					if(thisnode.misc&NODE_DOWN)
                        return(0);
					for(j=1;j<=sys_nodes;j++) {
						if(j==node_num)
							continue;
						getnodedat(j,&node,0);
						if(node.status!=NODE_OFFLINE
							&& node.status!=NODE_EVENT_LIMBO)
							break; }
					if(j>sys_nodes) /* all nodes either offline or in limbo */
						break;
					lprintf("\rWaiting for node %d (status=%d)"
						,j,node.status);
					lputc(CLREOL); } } }
		if(event[i]->node!=node_num)
			event[i]->last=now;
		else {
			sprintf(str,"%s%s.NOW",data_dir,event[i]->code);
			remove(str);
			offhook();
			lputc(FF);
			getnodedat(node_num,&thisnode,1);
			thisnode.status=NODE_EVENT_RUNNING;
			putnodedat(node_num,thisnode);
			if(event[i]->dir[0]) {
				if(event[i]->dir[1]==':')           /* drive letter specified */
					setdisk(toupper(event[i]->dir[0])-'A');
				if(chdir(event[i]->dir))
					errormsg(WHERE,ERR_CHDIR,event[i]->dir,0); }

			external(cmdstr(event[i]->cmd,nulstr,nulstr,NULL),0);  /* EX_CC */
			event[i]->last=time(NULL);
			sprintf(str,"%sTIME.DAB",ctrl_dir);
			if((file=nopen(str,O_WRONLY))==-1) {
				errormsg(WHERE,ERR_OPEN,str,O_WRONLY);
				return(ret); }
			lseek(file,(long)i*4L,SEEK_SET);
			write(file,&event[i]->last,sizeof(time_t));
			close(file);
			ret=0; } } }


if(sys_status&SS_DAILY || thisnode.misc&NODE_EVENT) {  /* daily events */

	offhook();
    lputc(FF);

	if(sys_status&SS_DAILY) {

		getnodedat(node_num,&thisnode,1);
		now=time(NULL);
		j=lastuser();
		thisnode.status=NODE_EVENT_RUNNING;
		putnodedat(node_num,thisnode);

		lprintf("Running system daily maintenance...\r\n\r\n");
		logentry("!:","Ran system daily maintenance");
		for(i=1;i<=j;i++) {

			lprintf("\rChecking user %5u of %-5u",i,j);
			user.number=i;
			getuserdat(&user);

			/***********************************************/
			/* Fix name (NAME.DAT and USER.DAT) mismatches */
			/***********************************************/
			if(user.misc&DELETED) {
				if(strcmp(username(i,str2),"DELETED USER"))
					putusername(i,nulstr);
				continue; }

			if(strcmp(user.alias,username(i,str2)))
				putusername(i,user.alias);

			if(!(user.misc&(DELETED|INACTIVE))
				&& user.expire && (ulong)user.expire<=(ulong)now) {
				putsmsg(i,text[AccountHasExpired]);
				sprintf(str,"%s #%u Expired",user.alias,user.number);
				logentry("!%",str);
				if(level_misc[user.level]&LEVEL_EXPTOVAL
					&& level_expireto[user.level]<10) {
					user.flags1=val_flags1[level_expireto[user.level]];
					user.flags2=val_flags2[level_expireto[user.level]];
					user.flags3=val_flags3[level_expireto[user.level]];
					user.flags4=val_flags4[level_expireto[user.level]];
					user.exempt=val_exempt[level_expireto[user.level]];
					user.rest=val_rest[level_expireto[user.level]];
					if(val_expire[level_expireto[user.level]])
						user.expire=now
							+(val_expire[level_expireto[user.level]]*24*60*60);
					else
						user.expire=0;
					user.level=val_level[level_expireto[user.level]]; }
				else {
					if(level_misc[user.level]&LEVEL_EXPTOLVL)
						user.level=level_expireto[user.level];
					else
						user.level=expired_level;
					user.flags1&=~expired_flags1; /* expired status */
					user.flags2&=~expired_flags2; /* expired status */
					user.flags3&=~expired_flags3; /* expired status */
					user.flags4&=~expired_flags4; /* expired status */
					user.exempt&=~expired_exempt;
					user.rest|=expired_rest;
					user.expire=0; }
				putuserrec(i,U_LEVEL,2,itoa(user.level,str,10));
				putuserrec(i,U_FLAGS1,8,ultoa(user.flags1,str,16));
				putuserrec(i,U_FLAGS2,8,ultoa(user.flags2,str,16));
				putuserrec(i,U_FLAGS3,8,ultoa(user.flags3,str,16));
				putuserrec(i,U_FLAGS4,8,ultoa(user.flags4,str,16));
				putuserrec(i,U_EXPIRE,8,ultoa(user.expire,str,16));
				putuserrec(i,U_EXEMPT,8,ultoa(user.exempt,str,16));
				putuserrec(i,U_REST,8,ultoa(user.rest,str,16));
				if(expire_mod[0]) {
					useron=user;
					online=ON_LOCAL;
					exec_bin(expire_mod,&main_csi);
					online=0; }
				}

			/***********************************************************/
			/* Auto deletion based on expiration date or days inactive */
			/***********************************************************/
			if(!(user.exempt&FLAG('P'))     /* Not a permanent account */
				&& !(user.misc&(DELETED|INACTIVE))	 /* alive */
				&& (sys_autodel && (now-user.laston)/(long)(24L*60L*60L)
				> sys_autodel)) {			/* Inactive too long */
				sprintf(str,"Auto-Deleted %s #%u",user.alias,user.number);
				logentry("!*",str);
				delallmail(i);
				putusername(i,nulstr);
				putuserrec(i,U_MISC,8,ultoa(user.misc|DELETED,str,16)); }

			if(!(user.misc&(DELETED|INACTIVE))
				&& preqwk_ar[0] && chk_ar(preqwk_ar,user)) {  /* Pre-QWK */
				for(k=1;k<=sys_nodes;k++) {
					getnodedat(k,&node,0);
					if((node.status==NODE_INUSE || node.status==NODE_QUIET
						|| node.status==NODE_LOGON) && node.useron==i)
						break; }
				if(k<=sys_nodes)	/* Don't pre-pack with user online */
					continue;
				lclini(node_scrnlen-1);
				lclatr(LIGHTGRAY);
                lputc(FF);
				console|=CON_L_ECHO;
				lprintf("Pre-packing QWK for %s...\r\n"
					,user.alias);
				useron=user;
				online=ON_LOCAL;
				statline=sys_def_stat;
				statusline();
				useron.qwk&=~QWK_FILES; /* turn off for pre-packing */
				useron.misc|=(ANSI|COLOR);
				delfiles(temp_dir,"*.*");
				getmsgptrs();
				getusrsubs();
				batdn_total=0;
				sprintf(str,"%sFILE\\%04u.QWK"
					,data_dir,useron.number);
				if(pack_qwk(str,&l,1)) {
					qwk_success(l,0,1);
					putmsgptrs(); }
				delfiles(temp_dir,"*.*");
				lclatr(LIGHTGRAY);
				lclini(node_scrnlen);
				online=0;
				lputc(FF); } }

		mail_maint();

		lprintf("\r\n\r\nRunning system daily event...\r\n");
		logentry("!:","Ran system daily event");
		sys_status&=~SS_DAILY;
		if(sys_daily[0])
			external(cmdstr(sys_daily,nulstr,nulstr,NULL),0); }    /* EX_CC */

	if(thisnode.misc&NODE_EVENT) {
		getnodedat(node_num,&thisnode,1);
        thisnode.status=NODE_EVENT_RUNNING;
        putnodedat(node_num,thisnode);
		if(node_daily[0])
			external(cmdstr(node_daily,nulstr,nulstr,NULL),0); /* EX_CC */
		getnodedat(node_num,&thisnode,1);
		thisnode.misc&=~NODE_EVENT;
		putnodedat(node_num,thisnode); }
	ret=0; }
return(ret);
}

/****************************************************************************/
/* This function waits for either a caller or a local logon. It returns 0   */
/* if the user failed the logon procedure, a 1 if the user succeeded        */
/* Called from function main                                                */
/****************************************************************************/
char waitforcall()
{
	static uint calls;
	uchar str[256],str2[256],cname[LEN_CID+1],c,gotcaller=0,x,y,dcd,*p
		,ans=0,hbeat=0,menuon=0,blank=0
		,*ok=mdm_misc&MDM_VERBAL ? "OK":"0"

	/* IP logging vars added by enigma */
		, ipstr[256], *ips;

	uint i,j,k,nodes,lastnodes=0;
	long l,m;
	int file,result_code=0;
	time_t start,lastnodechk=0,laststatchk=0,laststatfdate=0;
    node_t node;
    struct tm *gm;
	struct dfree d;
	stats_t stats,node_stats;

reset_logon_vars();
ipstr[0]=cid[0]=cname[0]=0;
online=console=0;
start=time(NULL);

getnodedat(node_num,&thisnode,1);
thisnode.status=NODE_WFC;
thisnode.misc&=~(NODE_INTR|NODE_MSGW|NODE_NMSG|NODE_UDAT|NODE_POFF|NODE_AOFF);
putnodedat(node_num,thisnode);
catsyslog(0);
if(term_ret)
    return(terminal());
/***
if(com_port)
    rioctl(0x10f);  /* for blanking debug line */
***/

if(qoc && calls) {
    if(qoc==1)
        offhook();
    lclini(node_scrnlen);
    lputc(FF);
    bail(0); }
useron.misc=0;
rows=24;
lputc(FF);
if(node_misc&NM_RESETVID) {
	textmode(C40);
	textmode(C80);
	_setcursortype(_NORMALCURSOR); }

lclatr(curatr=LIGHTGRAY);
lclini(node_scrnlen);


lputc(FF);
if(com_port && !connect_rate) {  /* Initialize the modem */
	lprintf("\rSetting DTE rate: %lu baud",com_rate);
	lputc(CLREOL);
#ifdef __OS2__
	if((i=setbaud(com_rate))!=0) {
#else
	if((i=setbaud((uint)(com_rate&0xffffL)))!=0) {
#endif
		lprintf(" - Failed! (%d)\r\n",i);
		bail(1); }
    c=0;
	if(!nmi && !(mdm_misc&MDM_DUMB))	/* if not dumb */
    while(c<4) {
        rioctl(IOFB);
        rioctl(IOCM|PAUSE|ABORT);
        rioctl(IOCS|PAUSE|ABORT);

		if(mdm_misc&MDM_CTS)
            if(!(rioctl(IOSTATE)&CTS)) {
				lputs("\rWaiting up to 30 seconds for CTS to raise (  )\b\b\b");
				for(i=0;i<30 && !lkbrd(0);i++) {   /* wait upto 15 seconds */
					lprintf("%2d\b\b",i+1);
					mswait(1000);
					if(rioctl(IOSTATE)&CTS)
						break; }
				if(i==30) {
					lputs("\r\n\r\nModem configured for hardware flow "
						"control and CTS is stuck low.\r\n");
					logline("@!","CTS stuck low. Can't initialize.");
					bail(1); } }

		lputs("\rInitializing modem...");
		lputc(CLREOL);

		dtr(5); 		/* in case we're still connected to somebody */
        dtr(1);
		mswait(750);	/* DTR is sometimes slow */
        rioctl(IOFB);   /* crap comes in when dtr is raised ? */
		if(rioctl(IOSTATE)&DCD && mdm_hang[0]) {
			mdmcmd(mdm_hang);
			if(strcmp(str,"0") && strcmp(str,"OK"))
				getmdmstr(str,SEC_OK); }

		/******************************************/
		/* Take phone off-hook while initializing */
		/******************************************/
		offhook();		
		getmdmstr(str,SEC_OK);
        if(strcmp(str,"0") && strcmp(str,"OK"))
			getmdmstr(str,SEC_OK);

		/*********************************************************/
		/* Send User Configured (broken?) initialization strings */
		/*********************************************************/
        mdmcmd(mdm_init);
        if(mdm_spec[0]) {
			getmdmstr(str,SEC_OK);
			if(strcmp(str,"0") && strcmp(str,"OK"))
				getmdmstr(str,SEC_OK);
            mdmcmd(mdm_spec); }
		getmdmstr(str,SEC_OK);
		if(strcmp(str,"0") && strcmp(str,"OK"))
			getmdmstr(str,SEC_OK);

		/*****************************/
		/* Send AT&C1 if DCD is high */
		/*****************************/
        if(rioctl(IOSTATE)&DCD) {
			lputs("\r\nDCD is high. Sending AT&C1 to modem...");
			mdmcmd("AT&C1");
			getmdmstr(str,SEC_OK);
			if(strcmp(str,ok))
				getmdmstr(str,SEC_OK); }

		/**********************************************/
		/* DCD still high? Pause and try dropping DTR */
		/**********************************************/
        if(rioctl(IOSTATE)&DCD) {
			lputs("\r\nDCD is high. Pausing...");
			dtr(0);
			mswait(2000);
			rioctl(MSR);
			if(rioctl(IOSTATE)&DCD) {
				lputs("\rDCD was high after modem init.\r\n");
				logentry("@!","DCD was high after modem init");
				if(mdm_misc&MDM_NODTR)
					mdmcmd(mdm_hang);
				else
					dtr(15);
				if(rioctl(IOSTATE)&DCD) {
					lputs("Hanging up failed to lower DCD.\r\n");
					logentry("@!","Hanging up failed to lower DCD");
					if(++c==4)
						break;
					continue; } }
			dtr(1); }

		/************************************************************/
		/* Let's repair any damage the user configured init strings */
		/* may have caused											*/
		/************************************************************/
		for(i=0;i<4;i++) {
			rioctl(IOFB);
			mdmcmd("AT");
			if(!getmdmstr(str,SEC_OK) || !strcmp(str,ok))
				break;
			if(!strcmp(str,"AT")) {     /* Echoing commands? */
				getmdmstr(str,SEC_OK);	/* Get OK */
				lputs("\r\nCommand echoed. Sending ATE0 to modem...");
				mdmcmd("ATE0"); }
			else if(mdm_misc&MDM_VERBAL && !strcmp(str,"0")) { /* numeric? */
				lputs("\r\nNumeric response. Sending ATV1 to modem...");
				mdmcmd("ATV1"); }
			else if(!(mdm_misc&MDM_VERBAL) && !strcmp(str,"OK")) {
				lputs("\r\nVerbal response. Sending ATV0 to modem...");
				mdmcmd("ATV0"); }
			else {
				lprintf("\r\nUnrecognized response '%s'. Sending AT to modem..."
					,str);
				mdmcmd("AT"); }
			if(getmdmstr(str,SEC_OK) && !strcmp(str,ok))
				break; }

		/************************************************/
		/* Put phone back on-hook and check result code */
		/************************************************/
		if(!mdm_hang[0])
			mdmcmd("ATH");
		else
			mdmcmd(mdm_hang); 
		if(!getmdmstr(str,SEC_OK)) {
			lputs("\r\nNo response.\r\n");
			logentry("@!","No response"); }
        else {
			if(!strcmp(str,ok))
                break;
			lprintf("\r\nInvalid result code: '%s' "
				"instead of '%s'.\r\n",str,ok);
			sprintf(tmp,"Invalid result code: '%s' instead of '%s'.",str,ok);
            logentry("@!",tmp); }
        if(++c==4)
            break;
        rioini(0,0);                /* remove com routines */
        sys_status&=~SS_COMISR;
		comini();
		setrate(); }
    if(c==4) {
		lputs("\r\nModem failed initialization.\r\n");
		errorlog("Modem failed initialization.");
		offhook();
        bail(1); }
	lputc(FF); }

else if(com_port && connect_rate) {
    gotcaller=1;
	console=CON_R_ECHO|CON_L_ECHO|CON_R_INPUT|CON_L_INPUT;
	online=ON_REMOTE;
	sprintf(connection,"%lu",connect_rate);
	cur_rate=connect_rate;
	cur_cps=connect_rate/10;
	connect_rate=0;
	if(cidarg[0]) { 					/* Caller-id passed as argument */
		mdm_misc|=MDM_CALLERID;
		p=strstr(cidarg,"NUMBER:");
		if(p) p+=8;
		else {
			p=strstr(cidarg,"NMBR");
			if(p) p+=7; }
		if(p) { 						   /* 3 message format */
			sprintf(cid,"%.*s",LEN_CID,p);
			for(i=strlen(cid);!isdigit(cid[i]) && i;i--)
				;
			cid[i+1]=0; 					/* chop off non-numbers */
			p=strstr(cidarg,"NAME:");
			if(p) p+=6;
			else {
				p=strstr(cidarg,"NAME");
				if(p) p+=7; }
			if(p)
				sprintf(cname,"%.*s",LEN_CID,p); }
		else if(strlen(cidarg)>12)		   /* Single message format */
			sprintf(cid,"%.*s",LEN_CID,cidarg+12);
		else
			sprintf(cid,"ERROR: '%s'",cidarg); } }


if(!online) {
	/*******************************************************/
    /* To make sure that BBS print routines work correctly */
    /*******************************************************/
    useron.number=1;
    getuserdat(&useron);
    if(!useron.number)
        useron.misc=ANSI|COLOR; }

useron.misc&=~UPAUSE;	/* make sure pause is off */

if(llo) {
	quicklogonstuff();
	sys_status&=~SS_USERON;
	gotcaller=1; }

if(!(useron.misc&EXPERT) && !gotcaller) {	/* Novice sysop */
	console=CON_L_ECHO;
	online=ON_LOCAL;
	lputc(FF);
	tos=1;
	lncntr=0;
	menu("WFC");
	menuon=1;
	online=0; }

while(!gotcaller) {
	mswait(0);		   /* give up rest of time slice */

	console=CON_L_ECHO;
    now=time(NULL);

	if(node_scrnblank && (now-start)/60>node_scrnblank
		&& !blank) { /* blank screen */
		lputc(FF);
		menuon=0;
		blank=1; }
	if(mdm_reinit && (now-start)/60>mdm_reinit) /* reinitialize modem */
		return(0);
	if(now-lastnodechk>node_sem_check) {  /* check node.dab every x seconds */

#if 1
		if((i=heapcheck())!=_HEAPOK) {
			lputc(FF);
			offhook();
			errormsg(WHERE,ERR_CHK,"heap",i);
			bail(1); }
#endif

		for(i=0;i<total_qhubs;i++)
			if(node_num==qhub[i]->node) {
				sprintf(str,"%sQNET\\%s.NOW",data_dir,qhub[i]->id);
				if(fexist(str))
					qhub[i]->last=-1; }

		for(i=0;i<total_events;i++)
			if(node_num==event[i]->node || event[i]->misc&EVENT_EXCL) {
				sprintf(str,"%s%s.NOW",data_dir,event[i]->code);
				if(fexist(str))
					event[i]->last=-1; }

        lastnodechk=now;
        getnodedat(node_num,&thisnode,0);
        if(thisnode.misc&NODE_DOWN) {
            lputc(FF);
            offhook();
            bail(0); }
        if(thisnode.status!=NODE_WFC) {
            getnodedat(node_num,&thisnode,1);
            thisnode.status=NODE_WFC;
            putnodedat(node_num,thisnode); }
        if(thisnode.misc&NODE_RRUN) {   /* Re-run the node */
            offhook();
            lputc(FF);
            getnodedat(node_num,&thisnode,1);
            thisnode.status=NODE_OFFLINE;
            thisnode.misc=0;
            putnodedat(node_num,thisnode);
            close(nodefile);
			close(node_ext);
			sys_status&=~SS_NODEDAB;
            if(sys_status&SS_LOGOPEN)
                close(logfile);
            if(sys_status&SS_COMISR)
                rioini(0,0);
            sys_status&=~(SS_COMISR|SS_INITIAL|SS_LOGOPEN);
            p=strchr(orgcmd,' ');
            if(p)
                p++;
			close(5);
			if(execle(orgcmd,orgcmd,p,NULL,envp)) {
				lprintf("\r\nCouldn't execute %s!\r\n",orgcmd);
                bail(1); } } }

	if(!wfc_events(lastnodechk))
		return(0);

	if(!menuon) {
		/******************/
		/* Roaming cursor */
		/******************/
		x=lclwx();
		y=lclwy();
		switch(random(5)) {
			case 0:
				if(x>1)
					x--;
				break;
			case 1:
				if(x<79)
					x++;
				break;
			case 2:
				if(y>1)
					y--;
				break;
			case 4:
				if(y<node_scrnlen-2)
					y++;
				break; } }
	else {						/* across the bottom */
		y=25;
		x=lclwx();
		if(x%2) x+=2;
		else x-=2;
		if(x<1)
			x=1;
		else if(x>80)
			x=80;
/*
		lclxy(x,y);
		gettext(x,y,x,y,&i);
		lclatr(random(0xf)+1);
		lputc(i);
		lclatr(LIGHTGRAY);
*/
/**
		mswait(100);
		puttext(x,y,x,y,&i);
**/ 	   }

#ifndef __FLAT__		// Heart beat always for 32-bit OSs
	if(inDV) {
#endif
		lclxy(80,1);
		if(hbeat) {
			lclatr(RED|HIGH);
			lputc(3); }
		else
			lputc(0);
		hbeat=!hbeat;
		lclatr(LIGHTGRAY);
#ifndef __FLAT__
		}
#endif

	lclxy(x,y);
	mswait(100);

								/* Wait for call screen */
								/* check every 10 seconds */

	if(node_misc&NM_WFCSCRN && now-laststatchk>node_stat_check
		&& !menuon && !blank) {
		laststatchk=now;
		sprintf(str,"%sDSTS.DAB",ctrl_dir);
		if(fdate(str)!=laststatfdate) {   /* system statistics */
			lclxy(1,1);
			lclatr(LIGHTGRAY);
			lputs("Retrieving Statistics.");
			lputc(CLREOL);
			if(!laststatfdate)	/* First time this WFC */
				getstats(node_num,&node_stats);
			lputc('.');
			laststatfdate=fdate(str);
			getstats(0,&stats);
			lputc('.');
			l=m=0;
			if(node_misc&NM_WFCMSGS) {
				for(i=0;i<total_subs;i++)
					l+=getposts(i); 			/* l=total posts */
				lputc('.');
				for(i=0;i<total_dirs;i++)
					m+=getfiles(i); 			/* m=total files */
				lputc('.'); }
			i=getmail(0,0); 				/* i=total email */
			lputc('.');
			j=getmail(1,0); 				/* j=total fback */
			lclxy(1,1);
			lclatr(GREEN);
			lprintf("Node #: ");
			lclatr(GREEN|HIGH);
			gm=localtime(&now);
			lprintf("%-3u ",node_num);
			lclatr(GREEN);
			lprintf("%-3.3s %-2u  Space: ",mon[gm->tm_mon],gm->tm_mday);
			if(temp_dir[1]==':')
				k=temp_dir[0]-'A'+1;
			else k=0;
			getdfree(k,&d);
			if((ulong)d.df_bsec*(ulong)d.df_sclus
				*(ulong)d.df_avail<((ulong)min_dspace*1024L)*2L)
				lclatr(RED|HIGH|BLINK);
			else
				lclatr(GREEN|HIGH);
			sprintf(str,"%sk",ultoac(((ulong)d.df_bsec
				*(ulong)d.df_sclus*(ulong)d.df_avail)/1024UL,tmp));
			lprintf("%-12.12s",str);
			if(lastuseron[0]) {
				lclatr(GREEN);
				lprintf("Laston: ");
				lclatr(GREEN|HIGH);
				lprintf(lastuseron);
				lclatr(GREEN);
				if(thisnode.connection)
					lprintf(" %u",thisnode.connection);
				else
					lprintf(" Local"); }
			lprintf("\r\n");
			lclatr(GREEN);
			lprintf("Logons: ");
			lclatr(GREEN|HIGH);
			sprintf(tmp,"%lu/%lu",node_stats.ltoday,stats.ltoday);
			lprintf("%-12.12s",tmp);
			lclatr(GREEN);
			lprintf("Total: ");
			lclatr(GREEN|HIGH);
			lprintf("%-12.12s",ultoac(stats.logons,tmp));
			lclatr(GREEN);
			lprintf("Timeon: ");
			lclatr(GREEN|HIGH);
			sprintf(tmp,"%lu/%lu",node_stats.ttoday,stats.ttoday);
			lprintf("%-12.12s",tmp);
			lclatr(GREEN);
			lprintf("Total: ");
			lclatr(GREEN|HIGH);
			lprintf("%-12.12s\r\n",ultoac(stats.timeon,tmp));
			lclatr(GREEN);
			lprintf("Emails: ");
			lclatr(GREEN|HIGH);
			sprintf(str,"%u/%u",(uint)stats.etoday,i);
			lprintf("%-12.12s",str);
			lclatr(GREEN);
			lprintf("Posts: ");
			lclatr(GREEN|HIGH);
			sprintf(str,"%u",(uint)stats.ptoday);
			if(node_misc&NM_WFCMSGS) {
				strcat(str,"/");
				strcat(str,ultoa(l,tmp,10)); }
			lprintf("%-12.12s",str);
			lclatr(GREEN);
			lprintf("Fbacks: ");
			lclatr(GREEN|HIGH);
			sprintf(str,"%u/%u",(uint)stats.ftoday,j);
			lprintf("%-12.12s",str);
			lclatr(GREEN);
			lprintf("Users: ");
			lclatr(GREEN|HIGH);
			sprintf(str,"%u/%u",stats.nusers,lastuser());
			lprintf("%-12.12s\r\n",str);
			lclatr(GREEN);
			lprintf("Uloads: ");
			lclatr(GREEN|HIGH);
			ultoac(stats.ulb/1024UL,tmp);
			strcat(tmp,"k");
			lprintf("%-12.12s",tmp);
			lclatr(GREEN);
			lprintf("Files: ");
			lclatr(GREEN|HIGH);
			sprintf(str,"%u",(uint)stats.uls);
			if(node_misc&NM_WFCMSGS) {
				strcat(str,"/");
				strcat(str,ultoa(m,tmp,10)); }
			lprintf("%-12.12s",str);
			lclatr(GREEN);
			lprintf("Dloads: ");
			lclatr(GREEN|HIGH);
			ultoac(stats.dlb/1024UL,tmp);
			strcat(tmp,"k");
			lprintf("%-12.12s",tmp);
			lclatr(GREEN);
			lprintf("Files: ");
			lclatr(GREEN|HIGH);
			lprintf("%-12.12s\r\n",ultoac(stats.dls,tmp)); }
		lclatr(curatr=LIGHTGRAY);
		lclxy(1,6);
		nodes=0;
		for(i=1;i<=sys_nodes;i++) {
			getnodedat(i,&node,0);
			if((node.status!=NODE_WFC && node.status!=NODE_OFFLINE)
				|| node.errors) {
				lputc(CLREOL);
				printnodedat(i,node);
				nodes++; } }
		if(nodes<lastnodes) {
			i=j=lclwy();
			j+=(lastnodes-nodes)+1;
			for(;i<=node_scrnlen && i<j;i++) {
				lclxy(1,i);
				lputc(CLREOL); } }
		lastnodes=nodes;
		lclatr(curatr=LIGHTGRAY); }


	if(!(node_misc&NM_NO_LKBRD) && lkbrd(1)) {
		start=time(NULL);
		/* menuon=0; */
        lputc(FF);
        tos=1;
		lncntr=0;
		i=lkbrd(0);
		if(!(i&0xff) && (i>=0x3b00 && i<=0x4400)
			|| i==0x8500 || i==0x8600) {		/* F1-F12 */
			if(i>=0x3b00 && i<=0x4400)
				i=((i-0x3b00)>>8);
			else if(i==0x8500)
				i=10;
			else
				i=11;
			offhook();
			external(cmdstr(wfc_scmd[i],nulstr,nulstr,NULL),EX_SWAP);
            return(0); }

		if(isdigit(i&0xff)) {
			offhook();
			external(cmdstr(wfc_cmd[i&0xf],nulstr,nulstr,NULL),0);
			return(0); }

		switch(toupper(i)) {
            case 'A':   /* forced answer */
                ans=1;
                break;
			case 'C':   /* configure */
				if(node_misc&NM_SYSPW) {
					quicklogonstuff();
					if(!chksyspass(1))
                        return(0); }
                offhook();
                 if(sys_status&SS_LOGOPEN) {
                    close(logfile);
                    sys_status&=~SS_LOGOPEN; }
                if(sys_status&SS_COMISR) {
                    rioini(0,0);    /* replace COM i/o vectors */
                    sys_status&=~SS_COMISR; }
                close(nodefile);
				close(node_ext);
				sys_status&=~SS_NODEDAB;
				sprintf(str,"%sEXECSBBS.%s",exec_dir,
#ifdef __OS2__
					"EXE"
#else
					"COM"
#endif
					);
				sprintf(tmp,"\"%s \"",cmdstr(scfg_cmd,nulstr,nulstr,NULL));
				if(execl(str,str,".",tmp,orgcmd,NULL,envp)) {
					errormsg(WHERE,ERR_EXEC,str,0);
                    bail(1); }
			case 'D':   /* Dos shell */
                offhook();
				external(comspec,0);
				return(0);
			case 'F':   /* Force Network Call-out */
                quicklogonstuff();
				lclatr(YELLOW);
				lputs("QWK Hub ID: ");
				lclatr(WHITE);
				if(!getstr(str,8,K_UPPER))
					return(0);
				for(i=0;i<total_qhubs;i++)
					if(!stricmp(str,qhub[i]->id)) {
						if(qhub[i]->node==node_num)
							qhub[i]->last=-1;
						else {
							sprintf(tmp,"%sQNET\\%s.NOW",data_dir,str);
							if((file=nopen(tmp,O_CREAT|O_TRUNC|O_WRONLY))
								!=-1)
								close(file); }
						break; }
                return(0);
            case 'L':   /* Log */
				if(node_misc&NM_SYSPW) {
					quicklogonstuff();
					if(!chksyspass(1))
                        return(0); }
                offhook();
                now=time(NULL);
                unixtodos(now,&date,&curtime);
                sprintf(str,"%sLOGS\\%2.2d%2.2d%2.2d.LOG"
                    ,data_dir,date.da_mon,date.da_day,TM_YEAR(date.da_year-1900));
				external(cmdstr(node_viewer,str,nulstr,NULL),0);
                return(0);
            case 'M':   /* Read all mail */
                quicklogonstuff();
				if(node_misc&NM_SYSPW && !chksyspass(1))
                    return(0);
				readmail(1,MAIL_ALL);
                online=console=0;
                return(0);
            case 'N':   /* Node statistics */
                quicklogonstuff();
				if(node_misc&NM_SYSPW && !chksyspass(1))
                    return(0);
				useron.misc|=UPAUSE;
                printstatslog(node_num);
                pause();
                return(0);
            case 'Q':   /* Quit */
                offhook();
                bail(0);
			case 'R':   /* Read feedback */
                quicklogonstuff();
				if(node_misc&NM_SYSPW && !chksyspass(1))
                    return(0);
				readmail(1,MAIL_YOUR);
                return(0);
			case 'K':   /* Sent mail */
                quicklogonstuff();
				if(node_misc&NM_SYSPW && !chksyspass(1))
                    return(0);
				readmail(1,MAIL_SENT);
				return(0);
            case 'S':   /* System Statistics */
                quicklogonstuff();
				if(node_misc&NM_SYSPW && !chksyspass(1))
                    return(0);
				useron.misc|=UPAUSE;
                printstatslog(0);
                pause();
                return(0);
			case 'T':   /* Terminal mode */
				if(!com_port) {
					lprintf("No COM port configured.");
					getch();
                    lputc(FF);
					return(0); }
				if(node_misc&NM_SYSPW) {
                    quicklogonstuff();
					if(!chksyspass(1))
                        return(0); }
                return(terminal());
			case 'U':   /* User edit */
                quicklogonstuff();
				useredit(0,1);
                return(0);
			case 'E':
			case 'W':   /* Write e-mail */
                quicklogonstuff();
				if(node_misc&NM_SYSPW && !chksyspass(1))
					return(0);
                bputs(text[Email]);
				if(!getstr(str,50,0))
                    return(0);
				if(!strchr(str,'@') && (i=finduser(str))!=0)
					email(i,nulstr,nulstr,WM_EMAIL);
				else
					netmail(str,nulstr,0);
                return(0);
			case 'X':   /* Exit with phone on-hook */
                bail(0);
			case 'Y':   /* Yesterday's log */
				if(node_misc&NM_SYSPW) {
					quicklogonstuff();
					if(!chksyspass(1))
						return(0); }
                offhook();
                now=time(NULL);
                now-=(ulong)60L*24L*60L;
                unixtodos(now,&date,&curtime);
                sprintf(str,"%sLOGS\\%2.2d%2.2d%2.2d.LOG"
                    ,data_dir,date.da_mon,date.da_day,TM_YEAR(date.da_year-1900));
				external(cmdstr(node_viewer,str,nulstr,NULL),0);
                return(0);
			case 'Z':
				mail_maint();
				return(0);
			case '?':   /* Menu */
				if(menuon) {
					menuon=0;
					lputc(FF);
					laststatchk=0;
					laststatfdate=0; }
				else {
					online=ON_LOCAL;
					menu("WFC");
					menuon=1;
					blank=0;
					online=0; }
                continue;
			case SP:   /* Log on */
				lclatr(WHITE|HIGH); lputc('Y'); lclatr(GREEN);
				lputs(" Yes\r\n");
				lclatr(WHITE|HIGH); lputc('Z'); lclatr(GREEN);
				lputs(" Yes, Quiet\r\n");
				lclatr(WHITE|HIGH); lputc('F'); lclatr(GREEN);
				lputs(" Fast User #1\r\n");
				lclatr(WHITE|HIGH); lputc('Q'); lclatr(GREEN);
				lputs(" Fast User #1, Quiet\r\n");
				lclatr(BROWN|HIGH);
				lputs("\r\nLog on? [No]: ");
				lclatr(LIGHTGRAY);
				i=getch();
				switch(toupper(i)) {
                    case 'Z':
                        onquiet=1;
					case 'Y':       /* Yes, logon */
                        lputc(FF);
                        quicklogonstuff();
						sys_status&=~SS_USERON;
						useron.misc=(UPAUSE|ANSI|COLOR);
                        gotcaller=1;
                        continue;
                    case 'Q':
                        onquiet=1;
                    case 'F':
                        lputc(FF);
                        quicklogonstuff();
						if(sys_misc&SM_REQ_PW || node_misc&NM_SYSPW) {
							useron.misc=(UPAUSE|ANSI|COLOR);
							gotcaller=1;
							continue; }
                        if(!useron.number) {
                            lputc(7);
                            lputs("A Sysop account hasn't been created");
							getch();
                            return(0); }
						return(logon());
                    default:
						return(0); }
            default:
                lputs("Hit '?' for a menu.");
                continue; } }
	if(com_port) {
		if(mdm_misc&MDM_VERBAL) {
			if(rioctl(RXBC)) {
				lputc(FF);
				getmdmstr(str,SEC_RING);
				if(!strcmp(str,"RING"))
					ans=1; } }
		else {	/* numeric */
			if(incom()=='2')
				ans=1; }
		if(mdm_misc&MDM_DUMB && DCDHIGH)
			ans=1; }
	if(ans && com_port) {
        lputc(FF);
		if(mdm_misc&MDM_CALLERID) {
			lputs("Obtaining Caller-ID...");
			getmdmstr(str,SEC_CID);
			lprintf("\r\nCID: %s",str);
			if(!strncmpi(str,"TIME:",5)) {  /* 3 message format */
				getmdmstr(str,SEC_CID);
				lprintf("\r\nCID: %s",str);
				p=strchr(str,':');
				if(p)
					strcpy(cid,p+2);		/* Phone Number */
				getmdmstr(str,SEC_CID);
				lprintf("\r\nCID: %s",str);
				p=strchr(str,':');
				if(p)
					sprintf(cname,"%.*s",LEN_CID,p+2); }    /* Caller Name */
			else if(!strncmpi(str,"DATE =",6)) { /* Supra format */
				getmdmstr(str,SEC_CID);
				lprintf("\r\nCID: %s",str);     /* TIME = line */
				getmdmstr(str,SEC_CID);
				lprintf("\r\nCID: %s",str);     /* NMBR = line */
				p=strchr(str,'=');
				if(p)
					sprintf(strstr(str,"NAME") ? cname:cid,"%.*s",LEN_CID,p+2);
				getmdmstr(str,SEC_CID); 		/* NAME = line */
				lprintf("\r\nCID: %s",str);
				p=strchr(str,'=');
				if(p)
					sprintf(strstr(str,"NAME") ? cname:cid,"%.*s",LEN_CID,p+2);
				}
			else if(strlen(str)>12) 		/* Single message format */
				sprintf(cid,"%.*s",LEN_CID,str+12);
			else
				sprintf(cid,"ERROR: '%s'",str);
			lputs(crlf); }
		cur_rate=cur_cps=0;
		connection[0]=0;
		result_code=0;
		if(!(mdm_misc&MDM_DUMB)) {
			if(mdm_rings>1) {
				lputs("Ring\r\n");
				for(i=1;i<mdm_rings;i++)
					if(!getmdmstr(str,10))
						break;
					else
						lputs("Ring\r\n");
				if(i<mdm_rings)
					return(0);
				lputs(crlf); }
			lputs("Answering...");
			mdmcmd(mdm_answ);
			rioctl(IOFI);						// flush extra ring results
			while(!cur_rate || rioctl(RXBC)) {	// no connect or more rsults
				if(!getmdmstr(str,SEC_ANSWER)) {
					if(cur_rate)
						break;
					lprintf("\r\nNO CARRIER\r\n");
					if(!(mdm_misc&MDM_NODTR)) {
						dtr(15);
						dtr(1);
						mswait(500); }
					mdmcmd(nulstr);
					hangup();
					return(0); }
				i=0;
				while(i<2
					&& (!strcmp(str,"2")
					|| !strcmp(str,"RING"))) { /* Ring? Get next result */
					lputs("Ring\r\n");
					if(i) {
						mdmcmd(mdm_answ);	  /* Give a second ATA */
						rioctl(IOFI); }
					if(!getmdmstr(str,SEC_ANSWER)) {
						if(!(mdm_misc&MDM_NODTR)) {
							dtr(15);
							dtr(1);
							mswait(500); }
						mdmcmd(nulstr);
						hangup();		  /* Get next result code from modem */
						return(0); }
					i++; }
				if(!strcmp(str,"3") || !strcmp(str,"NO CARRIER")) {
					lprintf("\r\nNO CARRIER\r\n");
					hangup();
					return(0); }

				lprintf("\r\nResult Code: %s\r\n",str);

				/* IP (vmodem) logging additions by enigma */
				ips = strstr(str, "TEL FROM ");
				if(ips) {
				  strcpy(ipstr, ips+9);
				}

				if(strstr(str,"FAX") || strstr(str,"+FCO")) {
                                    /* Just for the ZyXEL's */
                    mswait(1500);   /* 1 and a half second wait for ZyXEL */
                    sys_misc&=~SM_ERRALARM;
                    bail(100); }

				if(mdm_misc&MDM_VERBAL) {
					if(!cur_rate
						&& (!strnicmp(str,"CONNECT ",8)
						|| !strnicmp(str,"CARRIER ",8))) {
						cur_rate=atol(str+8);
						// build description
						for(i=8,j=0;str[i] && j<LEN_MODEM;i++) {
							if(j==2 && isdigit(str[i])
								&& str[i+1]=='0' && str[i+2]=='0'
								&& !isdigit(str[i+3]) && j+1<LEN_MODEM) {
								connection[j++]='.';
								connection[j++]=str[i];
								i+=2; }
							else
								connection[j++]=str[i]; }
						connection[j]=0; }
					if(!cur_rate && !strcmp(str,"CONNECT"))
						cur_rate=com_rate;
					if(cur_rate
						&& (strstr(str,"ARQ") || strstr(str,"MNP")
						|| strstr(str,"V42") || strstr(str,"LAPM")
						|| strstr(str,"REL")))
						cur_cps=cur_rate/9; }
				else {	/* numeric */
					j=atoi(str);
					for(i=0;i<mdm_results;i++)
						if(mdm_result[i].code==j) break;
					if(!cur_rate) {
						if(i==mdm_results) {
							lprintf("Unknown result code: '%s'\r\n",str);
							sprintf(tmp,"Unknown modem result code: '%s'",str);
							errorlog(tmp);
							if(mdm_misc&MDM_KNOWNRES || !mdm_results) {
								hangup();
								return(0); }	/* Don't allow invalid codes */
							i--; }	/* Use last configured result code */
						else
							result_code=mdm_result[i].code; }
					if(!strncmpi(mdm_result[i].str,"EXIT ",5))
						bail(atoi(mdm_result[i].str+5));
					if(!strcmpi(mdm_result[i].str,"FAX")) {
						lputs("FAX Connection\r\n");
						bail(100); }
					if(!strcmpi(mdm_result[i].str,"IGNORE") || cur_rate)
                        lputs("Ignored\r\n");
					else {
						cur_rate=mdm_result[i].rate;
						cur_cps=mdm_result[i].cps;
						strcpy(connection,mdm_result[i].str); } } } }
		else   /* dumb modem */
			cur_rate=com_rate;

		if(!connection[0])
			sprintf(connection,"%lu",cur_rate);
		lprintf("Logging Caller in at %s\r\n",connection);
		if(!cur_cps)
			cur_cps=cur_rate/10L;

        rioctl(IOCE|((RXLOST|FERR|PERR|OVRR)<<8));  /* clear error flags */
        console=CON_R_ECHO|CON_L_ECHO|CON_R_INPUT|CON_L_INPUT;
        online=ON_REMOTE;
        gotcaller=1;
		putcom(crlf);
#if defined(__OS2__)
		putcom(decrypt(VersionNoticeOS2,0));
#elif defined(__WIN32__)
		putcom(decrypt(VersionNoticeW32,0));
#else
		putcom(decrypt(VersionNoticeDOS,0));
#endif
		putcom(crlf);
		lprintf("Pausing %u Seconds...",mdm_ansdelay);
		if(node_misc&NM_ANSALARM) {
			now=time(NULL);
			while(time(NULL)-now<mdm_ansdelay)
                for(x=0;x<10;x++)
					for(y=0;y<10;y++)
						beep((x*100)+(y*5),10); }
		else
			secwait(mdm_ansdelay);
        nosound();
		rioctl(IOFI); } }	/* flush input buffer */

reset_logon_vars();
calls++;
useron.misc=0;
answertime=logontime=starttime=time(NULL);
if(online==ON_REMOTE) {
	setrate();

    now=time(NULL);
    gm=localtime(&now);
	sprintf(str,"%02d:%02d%c  %s %s %02d %u            Node %3u  %5lubps (%s)"
        ,gm->tm_hour>12 ? gm->tm_hour-12 : gm->tm_hour==0 ? 12 : gm->tm_hour
		,gm->tm_min,gm->tm_hour>=12 ? 'p' : 'a',wday[gm->tm_wday]
        ,mon[gm->tm_mon],gm->tm_mday,gm->tm_year+1900,node_num,cur_rate
        ,connection);
	if(result_code) {
		sprintf(tmp," [%d]",result_code);
		strcat(str,tmp); }
	logline("@",str);

	/* IP trashcan modification by enigma */
	if(trashcan(ipstr,"IP")) {
		sprintf(tmp, "IP Trashcan: %s, hanging up!\r\n", ipstr);
		logline("@!",tmp);
		hangup();
		return(0); }

	if(mdm_misc&MDM_CALLERID) {
		outcom(0xC);
#if defined(__OS2__)
		putcom(decrypt(VersionNoticeOS2,0));
#elif defined(__WIN32__)
		putcom(decrypt(VersionNoticeW32,0));
#else
		putcom(decrypt(VersionNoticeDOS,0));
#endif
		putcom(crlf);
		sprintf(str,"CID: %-*s %s",LEN_CID,cid,cname);
		logline("@*",str);
		if(trashcan(cid,"CID")) {
			hangup();
			return(0); } } }
lclini(node_scrnlen-1);
if(online==ON_REMOTE) {
    if(node_dollars_per_call) { /* Billing node stuff */
        now=time(NULL);
        bprintf(text[BillingNodeMsg],sys_name,node_dollars_per_call);
		while(online && now-answertime<30) {
			if(incom()!=NOINP) {
				if(!(mdm_misc&MDM_NODTR))
					dtr(15);
				else
					mdmcmd(mdm_hang); }
            checkline();
            now=time(NULL); }
        if(!online)
            return(0); }
	rioctl(IOFI);
	putcom("\x1b[99B\x1b[6n\x1b[!_\x1b[0t_\x1b[0m_\xC");
    lputc(FF);
	i=l=lncntr=autoterm=0;
	tos=1;
#if defined(__OS2__)
	strcpy(str,decrypt(VersionNoticeOS2,0));
#elif defined(__WIN32__)
	strcpy(str,decrypt(VersionNoticeW32,0));
#else
	strcpy(str,decrypt(VersionNoticeDOS,0));
#endif
	strcat(str,decrypt(CopyrightNotice,0));
	center(str);
	mswait(500);
	while(i++<30 && l<40) { 		/* wait up to 3 seconds for response */
		if((c=(incom()&0x7f))=='R') {   /* break immediately if response */
			str[l++]=c;
			mswait(110);
			break; }
        if(c) {
			str[l++]=c;
			if(l==1 && c!=ESC)
				break; }
        else
			mswait(100); }

	if(rioctl(RXBC))	/* wait a bit for extra RIP reply chars */
		mswait(550);

	while((i=(incom()&0x7f))!=NOINP && l<40)
		str[l++]=i;
	str[l]=0;

#if 0
	for(i=0;str[i];i++)
		lprintf("%02X ",str[i]);
	lputs(crlf);
#endif

    if(l) {
        if(str[0]==ESC && str[1]=='[') {
			useron.misc|=(ANSI|COLOR);
			autoterm|=ANSI;
            rows=((str[2]&0xf)*10)+(str[3]&0xf);
			if(rows<10 || rows>99) rows=24; }
		truncsp(str);
		if(strstr(str,"RIPSCRIP")) {
			logline("@R",strstr(str,"RIPSCRIP"));
			useron.misc|=(RIP|COLOR|ANSI);
			autoterm|=(RIP|COLOR|ANSI); }
		else if(strstr(str,"DC-TERM")
			&& toupper(*(strstr(str,"DC-TERM")+12))=='W') {
			logline("@W",strstr(str,"DC-TERM"));
			useron.misc|=(WIP|COLOR|ANSI);
			autoterm|=(WIP|COLOR|ANSI); } }
    rioctl(IOFI); /* flush left-over or late response chars */
    sprintf(str,"%sANSWER",text_dir);
	sprintf(tmp,"%s.%s",str,autoterm&WIP ? "WIP":"RIP");
	sprintf(str2,"%s.ANS",str);
	if(autoterm&(RIP|WIP) && fexist(tmp))
		strcat(str,autoterm&WIP ? ".WIP":".RIP");
	else if(autoterm&ANSI && fexist(str2))
        strcat(str,".ANS");
    else
		strcat(str,".ASC");
    rioctl(IOSM|PAUSE);
	sys_status|=SS_PAUSEON;
	tos=1;
    printfile(str,P_NOABORT);
	sys_status&=~SS_PAUSEON;
    CRLF; }
else
	useron.misc=(UPAUSE|ANSI|COLOR);

useron.number=0;

exec_bin(login_mod,&main_csi);

if(!useron.number)
    hangup();
if(!online) {
	logout();
	return(0); }

if(online==ON_REMOTE && mdm_misc&MDM_CALLERID) {
	strcpy(useron.note,cid);
	putuserrec(useron.number,U_NOTE,LEN_NOTE,useron.note); }

/* log the IP by enigma */

if(online==ON_REMOTE && ipstr[0]!='\0') {
	sprintf(useron.note, "Telnet: %s", ipstr);
	putuserrec(useron.number,U_NOTE,LEN_NOTE,useron.note);
	logline("@*", useron.note); }

if(!(sys_status&SS_USERON)) {
    errormsg(WHERE,ERR_CHK,"User not logged on",0);
	hangup();
	logout();
	return(0); }

return(1);
}

