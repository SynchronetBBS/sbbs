#line 1 "GETUSER.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "cmdshell.h"

/****************************************************************************/
/* Fills the structure 'user' with info for user.number	from USER.DAT		*/
/* Called from functions useredit, waitforcall and main_sec					*/
/****************************************************************************/
void getuserdat(user_t *user)
{
	uchar userdat[U_LEN+1],str[U_LEN+1];
	int i,file;

if(!user->number || user->number>lastuser()) {
	memset(user,0L,sizeof(user_t));
	user->number=0;
	return; }
sprintf(userdat,"%sUSER\\USER.DAT",data_dir);
if((file=nopen(userdat,O_RDONLY|O_DENYNONE))==-1) {
	close(file);
	memset(user,0L,sizeof(user_t));
	user->number=0;
	return; }
lseek(file,(long)((long)(user->number-1)*U_LEN),SEEK_SET);
i=0;
while(i<LOOP_NODEDAB
	&& lock(file,(long)((long)(user->number-1)*U_LEN),U_LEN)==-1) {
	if(i>10)
		mswait(55);
	i++; }

if(i>=LOOP_NODEDAB) {
	close(file);
	errormsg(WHERE,ERR_LOCK,"USER.DAT",user->number);
	memset(user,0L,sizeof(user_t));
	user->number=0;
    return; }

if(read(file,userdat,U_LEN)!=U_LEN) {
	unlock(file,(long)((long)(user->number-1)*U_LEN),U_LEN);
	close(file);
	errormsg(WHERE,ERR_READ,"USER.DAT",U_LEN);
	memset(user,0L,sizeof(user_t));
	user->number=0;
	return; }

unlock(file,(long)((long)(user->number-1)*U_LEN),U_LEN);
close(file);
getrec(userdat,U_ALIAS,LEN_ALIAS,user->alias);
/* order of these function	*/
getrec(userdat,U_NAME,LEN_NAME,user->name);
/* calls is irrelevant */
getrec(userdat,U_HANDLE,LEN_HANDLE,user->handle);
getrec(userdat,U_NOTE,LEN_NOTE,user->note);
getrec(userdat,U_COMP,LEN_COMP,user->comp);
getrec(userdat,U_COMMENT,LEN_COMMENT,user->comment);
getrec(userdat,U_NETMAIL,LEN_NETMAIL,user->netmail);
getrec(userdat,U_ADDRESS,LEN_ADDRESS,user->address);
getrec(userdat,U_LOCATION,LEN_LOCATION,user->location);
getrec(userdat,U_ZIPCODE,LEN_ZIPCODE,user->zipcode);
getrec(userdat,U_PASS,LEN_PASS,user->pass);
getrec(userdat,U_PHONE,LEN_PHONE,user->phone);
getrec(userdat,U_BIRTH,LEN_BIRTH,user->birth);
getrec(userdat,U_MODEM,LEN_MODEM,user->modem);
getrec(userdat,U_LASTON,8,str); user->laston=ahtoul(str);
getrec(userdat,U_FIRSTON,8,str); user->firston=ahtoul(str);
getrec(userdat,U_EXPIRE,8,str); user->expire=ahtoul(str);
getrec(userdat,U_PWMOD,8,str); user->pwmod=ahtoul(str);
getrec(userdat,U_NS_TIME,8,str);
user->ns_time=ahtoul(str);
if(user->ns_time<0x20000000L)
	user->ns_time=user->laston;  /* Fix for v2.00->v2.10 */

getrec(userdat,U_LOGONS,5,str); user->logons=atoi(str);
getrec(userdat,U_LTODAY,5,str); user->ltoday=atoi(str);
getrec(userdat,U_TIMEON,5,str); user->timeon=atoi(str);
getrec(userdat,U_TEXTRA,5,str); user->textra=atoi(str);
getrec(userdat,U_TTODAY,5,str); user->ttoday=atoi(str);
getrec(userdat,U_TLAST,5,str); user->tlast=atoi(str);
getrec(userdat,U_POSTS,5,str); user->posts=atoi(str);
getrec(userdat,U_EMAILS,5,str); user->emails=atoi(str);
getrec(userdat,U_FBACKS,5,str); user->fbacks=atoi(str);
getrec(userdat,U_ETODAY,5,str); user->etoday=atoi(str);
getrec(userdat,U_PTODAY,5,str); user->ptoday=atoi(str);
getrec(userdat,U_ULB,10,str); user->ulb=atol(str);
getrec(userdat,U_ULS,5,str); user->uls=atoi(str);
getrec(userdat,U_DLB,10,str); user->dlb=atol(str);
getrec(userdat,U_DLS,5,str); user->dls=atoi(str);
getrec(userdat,U_CDT,10,str); user->cdt=atol(str);
getrec(userdat,U_MIN,10,str); user->min=atol(str);
getrec(userdat,U_LEVEL,2,str); user->level=atoi(str);
getrec(userdat,U_FLAGS1,8,str); user->flags1=ahtoul(str); /***
getrec(userdat,U_TL,2,str); user->tl=atoi(str); ***/
getrec(userdat,U_FLAGS2,8,str); user->flags2=ahtoul(str);
getrec(userdat,U_FLAGS3,8,str); user->flags3=ahtoul(str);
getrec(userdat,U_FLAGS4,8,str); user->flags4=ahtoul(str);
getrec(userdat,U_EXEMPT,8,str); user->exempt=ahtoul(str);
getrec(userdat,U_REST,8,str); user->rest=ahtoul(str);
getrec(userdat,U_ROWS,2,str); user->rows=atoi(str);
if(user->rows && user->rows<10)
	user->rows=10;
user->sex=userdat[U_SEX];
if(!user->sex)
	user->sex=SP;	 /* fix for v1b04 that could save as 0 */
user->prot=userdat[U_PROT];
if(user->prot<SP)
	user->prot=SP;
getrec(userdat,U_MISC,8,str); user->misc=ahtoul(str);
if(user->rest&FLAG('Q'))
	user->misc&=~SPIN;

getrec(userdat,U_LEECH,2,str);
user->leech=(uchar)ahtoul(str);
getrec(userdat,U_CURSUB,8,useron.cursub);
getrec(userdat,U_CURDIR,8,useron.curdir);

getrec(userdat,U_FREECDT,10,str);
user->freecdt=atol(str);

getrec(userdat,U_XEDIT,8,str);
for(i=0;i<total_xedits;i++)
	if(!stricmp(str,xedit[i]->code) && chk_ar(xedit[i]->ar,*user))
		break;
user->xedit=i+1;
if(user->xedit>total_xedits)
    user->xedit=0;

getrec(userdat,U_SHELL,8,str);
for(i=0;i<total_shells;i++)
	if(!stricmp(str,shell[i]->code))
		break;
if(i==total_shells)
	i=0;
user->shell=i;

getrec(userdat,U_QWK,8,str);
if(str[0]<SP) { 			   /* v1c, so set defaults */
	if(user->rest&FLAG('Q'))
		user->qwk=(QWK_RETCTLA);
	else
		user->qwk=(QWK_FILES|QWK_ATTACH|QWK_EMAIL|QWK_DELMAIL); }
else
	user->qwk=ahtoul(str);

getrec(userdat,U_TMPEXT,3,user->tmpext);
if((!user->tmpext[0] || !strcmp(user->tmpext,"0")) && total_fcomps)
	strcpy(user->tmpext,fcomp[0]->ext);  /* For v1x to v2x conversion */

getrec(userdat,U_CHAT,8,str);
user->chat=ahtoul(str);

if(useron.number==user->number) {
	useron=*user;

	if(online) {

#if 0
		getusrdirs();
		getusrsubs();
#endif
		if(useron.misc&AUTOTERM) {
			useron.misc&=~(ANSI|RIP|WIP);
			useron.misc|=autoterm; }
		statusline(); } }
}


/****************************************************************************/
/* Returns the username in 'str' that corresponds to the 'usernumber'       */
/* Called from functions everywhere                                         */
/****************************************************************************/
char *username(int usernumber,char *strin)
{
    char str[256];
    char c;
    int file;

if(usernumber<1) {
    strin[0]=0;
    return(strin); }
sprintf(str,"%sUSER\\NAME.DAT",data_dir);
if(flength(str)<1L) {
    strin[0]=0;
    return(strin); }
if((file=nopen(str,O_RDONLY))==-1) {
    errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
    strin[0]=0;
    return(strin); }
if(filelength(file)<(long)((long)usernumber*(LEN_ALIAS+2))) {
    close(file);
    strin[0]=0;
    return(strin); }
lseek(file,(long)((long)(usernumber-1)*(LEN_ALIAS+2)),SEEK_SET);
read(file,strin,LEN_ALIAS);
close(file);
for(c=0;c<LEN_ALIAS;c++)
    if(strin[c]==ETX) break;
strin[c]=0;
if(!c)
    strcpy(strin,"DELETED USER");
return(strin);
}

/****************************************************************************/
/* Fills the timeleft variable with the correct value. Hangs up on the      */
/* user if their time is up.                                                */
/* Called from functions main_sec and xfer_sec                              */
/****************************************************************************/
void gettimeleft(void)
{
    static  inside;
    char    str[128];
    int     i;
    time_t  eventtime=0,thisevent;
    long    tleft;
    struct  date lastdate;
    struct  tm *gm;

now=time(NULL);

gm=localtime(&now);
if(useron.exempt&FLAG('T')) {   /* Time online exemption */
    timeleft=level_timepercall[useron.level]*60;
    if(timeleft<10)             /* never get below 10 for exempt users */
        timeleft=10; }
else {
    tleft=(((long)level_timeperday[useron.level]-useron.ttoday)
        +useron.textra)*60L;
    if(tleft<0) tleft=0;
    if(tleft>level_timepercall[useron.level]*60)
        tleft=level_timepercall[useron.level]*60;
    tleft+=useron.min*60L;
    tleft-=now-starttime;
    if(tleft>0x7fffL)
        timeleft=0x7fff;
    else
        timeleft=tleft; }

/* Timed event time reduction handler */

for(i=0;i<total_events;i++) {
    if(!event[i]->node || event[i]->node>sys_nodes)
        continue;
    if(!(event[i]->misc&EVENT_FORCE)
        || (!(event[i]->misc&EVENT_EXCL) && event[i]->node!=node_num)
        || !(event[i]->days&(1<<gm->tm_wday)))
        continue;
    unixtodos(event[i]->last,&lastdate,&curtime);
    unixtodos(now,&date,&curtime);
    curtime.ti_hour=event[i]->time/60;   /* hasn't run yet today */
    curtime.ti_min=event[i]->time-(curtime.ti_hour*60);
    curtime.ti_sec=0;
    thisevent=dostounix(&date,&curtime);
    if(date.da_day==lastdate.da_day && date.da_mon==lastdate.da_mon)
        thisevent+=24L*60L*60L;     /* already ran today, so add 24hrs */
    if(!eventtime || thisevent<eventtime)
        eventtime=thisevent; }
if(eventtime && now+timeleft>eventtime) {    /* less time, set flag */
    sys_status|=SS_EVENT;
    timeleft=eventtime-now; }

/* Event time passed by front-end */
if(next_event && (next_event<now || next_event-now<timeleft)) {
    timeleft=next_event-now;
    sys_status|=SS_EVENT; }

if(timeleft<0)  /* timeleft can't go negative */
    timeleft=0;
if(thisnode.status==NODE_NEWUSER) {
    timeleft=level_timepercall[new_level];
    if(timeleft<10*60L)
        timeleft=10*60L; }

if(inside)			/* The following code is not recursive */
    return;
inside=1;

if(!timeleft && !SYSOP && !(sys_status&SS_LCHAT)) {
    logline(nulstr,"Ran out of time");
    SAVELINE;
    bputs(text[TimesUp]);
    if(!(sys_status&(SS_EVENT|SS_USERON)) && useron.cdt>=100L*1024L
		&& !(sys_misc&SM_NOCDTCVT)) {
        sprintf(tmp,text[Convert100ktoNminQ],cdt_min_value);
        if(yesno(tmp)) {
			logline("  ","Credit to Minute Conversion");
            useron.min=adjustuserrec(useron.number,U_MIN,10,cdt_min_value);
			useron.cdt=adjustuserrec(useron.number,U_CDT,10,-(102400L));
			sprintf(str,"Credit Adjustment: %ld",-(102400L));
			logline("$-",str);
			sprintf(str,"Minute Adjustment: %u",cdt_min_value);
			logline("*+",str);
            RESTORELINE;
            gettimeleft();
            inside=0;
            return; } }
	if(sys_misc&SM_TIME_EXP && !(sys_status&SS_EVENT)
		&& !(useron.exempt&FLAG('E'))) {
                                        /* set to expired values */
		bputs(text[AccountHasExpired]);
		sprintf(str,"%s Expired",useron.alias);
		logentry("!%",str);
		if(level_misc[useron.level]&LEVEL_EXPTOVAL
			&& level_expireto[useron.level]<10) {
			useron.flags1=val_flags1[level_expireto[useron.level]];
			useron.flags2=val_flags2[level_expireto[useron.level]];
			useron.flags3=val_flags3[level_expireto[useron.level]];
			useron.flags4=val_flags4[level_expireto[useron.level]];
			useron.exempt=val_exempt[level_expireto[useron.level]];
			useron.rest=val_rest[level_expireto[useron.level]];
			if(val_expire[level_expireto[useron.level]])
				useron.expire=now
					+(val_expire[level_expireto[useron.level]]*24*60*60);
			else
				useron.expire=0;
			useron.level=val_level[level_expireto[useron.level]]; }
		else {
			if(level_misc[useron.level]&LEVEL_EXPTOLVL)
				useron.level=level_expireto[useron.level];
			else
				useron.level=expired_level;
			useron.flags1&=~expired_flags1; /* expired status */
			useron.flags2&=~expired_flags2; /* expired status */
			useron.flags3&=~expired_flags3; /* expired status */
			useron.flags4&=~expired_flags4; /* expired status */
			useron.exempt&=~expired_exempt;
			useron.rest|=expired_rest;
			useron.expire=0; }
        putuserrec(useron.number,U_LEVEL,2,itoa(useron.level,str,10));
        putuserrec(useron.number,U_FLAGS1,8,ultoa(useron.flags1,str,16));
        putuserrec(useron.number,U_FLAGS2,8,ultoa(useron.flags2,str,16));
        putuserrec(useron.number,U_FLAGS3,8,ultoa(useron.flags3,str,16));
        putuserrec(useron.number,U_FLAGS4,8,ultoa(useron.flags4,str,16));
        putuserrec(useron.number,U_EXPIRE,8,ultoa(useron.expire,str,16));
        putuserrec(useron.number,U_EXEMPT,8,ultoa(useron.exempt,str,16));
        putuserrec(useron.number,U_REST,8,ultoa(useron.rest,str,16));
		if(expire_mod[0])
			exec_bin(expire_mod,&main_csi);
        RESTORELINE;
        gettimeleft();
        inside=0;
        return; }
    SYNC;
    hangup(); }
inside=0;
}

/****************************************************************************/
/* Places into 'strout' CR or ETX terminated string starting at             */
/* 'start' and ending at 'start'+'length' or terminator from 'strin'        */
/****************************************************************************/
void getrec(char *strin,int start,int length,char *strout)
{
    int i=0,stop;

stop=start+length;
while(start<stop) {
	if(strin[start]==ETX || strin[start]==CR || strin[start]==LF)
        break;
    strout[i++]=strin[start++]; }
strout[i]=0;
}

/****************************************************************************/
/* Places into 'strout', 'strin' starting at 'start' and ending at          */
/* 'start'+'length'                                                         */
/****************************************************************************/
void putrec(char *strout,int start,int length,char *strin)
{
    int i=0,j;

j=strlen(strin);
while(i<j && i<length)
    strout[start++]=strin[i++];
while(i++<length)
    strout[start++]=ETX;
}


/****************************************************************************/
/* Returns the age derived from the string 'birth' in the format MM/DD/YY	*/
/* Called from functions statusline, main_sec, xfer_sec, useredit and 		*/
/* text files																*/
/****************************************************************************/
char getage(char *birth)
{
	char age;

if(!atoi(birth) || !atoi(birth+3))	/* Invalid */
	return(0);
getdate(&date);
age=(date.da_year-1900)-(((birth[6]&0xf)*10)+(birth[7]&0xf));
if(age>90)
	age-=90;
if(sys_misc&SM_EURODATE) {		/* DD/MM/YY format */
	if(atoi(birth)>31 || atoi(birth+3)>12)
		return(0);
	if(((birth[3]&0xf)*10)+(birth[4]&0xf)>date.da_mon ||
		(((birth[3]&0xf)*10)+(birth[4]&0xf)==date.da_mon &&
		((birth[0]&0xf)*10)+(birth[1]&0xf)>date.da_day))
		age--; }
else {							/* MM/DD/YY format */
	if(atoi(birth)>12 || atoi(birth+3)>31)
		return(0);
	if(((birth[0]&0xf)*10)+(birth[1]&0xf)>date.da_mon ||
		(((birth[0]&0xf)*10)+(birth[1]&0xf)==date.da_mon &&
		((birth[3]&0xf)*10)+(birth[4]&0xf)>date.da_day))
		age--; }
if(age<0)
	return(0);
return(age);
}


/****************************************************************************/
/* Converts an ASCII Hex string into an ulong                               */
/****************************************************************************/
ulong ahtoul(char *str)
{
    ulong l,val=0;

while((l=(*str++)|0x20)!=0x20)
    val=(l&0xf)+(l>>6&1)*9+val*16;
return(val);
}

