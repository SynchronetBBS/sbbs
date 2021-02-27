#line 1 "ATCODES.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "cmdshell.h"

extern char *wday[];    /* 3 char days of week */
char question[128];

int syncatcodes(char *sp, int len)
{
	char str2[128],*tp;

/* Synchronet Specific */

if(!strncmp(sp,"SETSTR:",7))
    strcpy(main_csi.str,sp+7);

else if(!strncmp(sp,"EXEC:",5))
	exec_bin(sp+5,&main_csi);

else if(!strncmp(sp,"MENU:",5))
    menu(sp+5);

else if(!strncmp(sp,"TYPE:",5))
    printfile(cmdstr(sp+5,nulstr,nulstr,str2),0);

else if(!strcmp(sp,"QUESTION"))
    bputs(question);

else if(!strncmp(sp,"NAME-L",6))
    bprintf("%-*.*s",len,len,useron.alias);

else if(!strncmp(sp,"NAME-R",6))
    bprintf("%*.*s",len,len,useron.alias);

else if(!strcmp(sp,"HANDLE"))
    bputs(useron.handle);

else if(!strcmp(sp,"CID") || !strcmp(sp,"IP"))
    bputs(cid);

else if(!strcmp(sp,"GRP"))
    bputs(usrgrps ? grp[usrgrp[curgrp]]->sname : nulstr);

else if(!strncmp(sp,"GRP-L",5))
    bprintf("%-*.*s",len,len,usrgrps ? grp[usrgrp[curgrp]]->sname : nulstr);

else if(!strncmp(sp,"GRP-R",5))
    bprintf("%*.*s",len,len,usrgrps ? grp[usrgrp[curgrp]]->sname : nulstr);

else if(!strcmp(sp,"GRPL"))
    bputs(usrgrps ? grp[usrgrp[curgrp]]->lname : nulstr);

else if(!strncmp(sp,"GRPL-L",6))
    bprintf("%-*.*s",len,len,usrgrps ? grp[usrgrp[curgrp]]->lname : nulstr);

else if(!strncmp(sp,"GRPL-R",6))
    bprintf("%*.*s",len,len,usrgrps ? grp[usrgrp[curgrp]]->lname : nulstr);

else if(!strcmp(sp,"GN"))
    bprintf("%u",usrgrps ? curgrp+1 : 0);

else if(!strcmp(sp,"GL"))
    bprintf("%-4u",usrgrps ? curgrp+1 : 0);

else if(!strcmp(sp,"GR"))
    bprintf("%4u",usrgrps ? curgrp+1 : 0);

else if(!strcmp(sp,"SUB"))
    bputs(usrgrps ? sub[usrsub[curgrp][cursub[curgrp]]]->sname : nulstr);

else if(!strncmp(sp,"SUB-L",5))
    bprintf("%-*.*s",len,len,usrgrps
        ? sub[usrsub[curgrp][cursub[curgrp]]]->sname : nulstr);

else if(!strncmp(sp,"SUB-R",5))
    bprintf("%*.*s",len,len,usrgrps
        ? sub[usrsub[curgrp][cursub[curgrp]]]->sname : nulstr);

else if(!strcmp(sp,"SUBL"))
    bputs(usrgrps  ? sub[usrsub[curgrp][cursub[curgrp]]]->lname : nulstr);

else if(!strncmp(sp,"SUBL-L",6))
    bprintf("%-*.*s",len,len,usrgrps
        ? sub[usrsub[curgrp][cursub[curgrp]]]->lname : nulstr);

else if(!strncmp(sp,"SUBL-R",6))
    bprintf("%*.*s",len,len,usrgrps
        ? sub[usrsub[curgrp][cursub[curgrp]]]->lname : nulstr);

else if(!strcmp(sp,"SN"))
    bprintf("%u",usrgrps ? cursub[curgrp]+1 : 0);

else if(!strcmp(sp,"SL"))
    bprintf("%-4u",usrgrps ? cursub[curgrp]+1 : 0);

else if(!strcmp(sp,"SR"))
    bprintf("%4u",usrgrps ? cursub[curgrp]+1 : 0);

else if(!strcmp(sp,"LIB"))
    bputs(usrlibs ? lib[usrlib[curlib]]->sname : nulstr);

else if(!strncmp(sp,"LIB-L",5))
    bprintf("%-*.*s",len,len,usrlibs ? lib[usrlib[curlib]]->sname : nulstr);

else if(!strncmp(sp,"LIB-R",5))
    bprintf("%*.*s",len,len,usrlibs ? lib[usrlib[curlib]]->sname : nulstr);

else if(!strcmp(sp,"LIBL"))
    bputs(usrlibs ? lib[usrlib[curlib]]->lname : nulstr);

else if(!strncmp(sp,"LIBL-L",6))
    bprintf("%-*.*s",len,len,usrlibs ? lib[usrlib[curlib]]->lname : nulstr);

else if(!strncmp(sp,"LIBL-R",6))
    bprintf("%*.*s",len,len,usrlibs ? lib[usrlib[curlib]]->lname : nulstr);

else if(!strcmp(sp,"LN"))
    bprintf("%u",usrlibs ? curlib+1 : 0);

else if(!strcmp(sp,"LL"))
    bprintf("%-4u",usrlibs ? curlib+1 : 0);

else if(!strcmp(sp,"LR"))
    bprintf("%4u",usrlibs  ? curlib+1 : 0);

else if(!strcmp(sp,"DIR"))
    bputs(usrlibs ? dir[usrdir[curlib][curdir[curlib]]]->sname :nulstr);

else if(!strncmp(sp,"DIR-L",5))
    bprintf("%-*.*s",len,len,usrlibs
        ? dir[usrdir[curlib][curdir[curlib]]]->sname : nulstr);

else if(!strncmp(sp,"DIR-R",5))
    bprintf("%*.*s",len,len,usrlibs
        ? dir[usrdir[curlib][curdir[curlib]]]->sname : nulstr);

else if(!strcmp(sp,"DIRL"))
    bputs(usrlibs ? dir[usrdir[curlib][curdir[curlib]]]->lname : nulstr);

else if(!strncmp(sp,"DIRL-L",6))
    bprintf("%-*.*s",len,len,usrlibs
        ? dir[usrdir[curlib][curdir[curlib]]]->lname : nulstr);

else if(!strncmp(sp,"DIRL-R",6))
    bprintf("%*.*s",len,len,usrlibs
        ? dir[usrdir[curlib][curdir[curlib]]]->lname : nulstr);

else if(!strcmp(sp,"DN"))
    bprintf("%u",usrlibs ? curdir[curlib]+1 : 0);

else if(!strcmp(sp,"DL"))
    bprintf("%-4u",usrlibs ? curdir[curlib]+1 : 0);

else if(!strcmp(sp,"DR"))
    bprintf("%4u",usrlibs ? curdir[curlib]+1 : 0);

else if(!strcmp(sp,"NOACCESS")) {
    if(noaccess_str==text[NoAccessTime])
        bprintf(noaccess_str,noaccess_val/60,noaccess_val%60);
    else if(noaccess_str==text[NoAccessDay])
        bprintf(noaccess_str,wday[noaccess_val]);
    else
        bprintf(noaccess_str,noaccess_val); }

else if(!strcmp(sp,"LAST")) {
    tp=strrchr(useron.alias,SP);
    if(tp) tp++;
    else tp=useron.alias;
    bputs(tp); }

else if(!strcmp(sp,"REAL")) {
    strcpy(str2,useron.name);
    tp=strchr(str2,SP);
    if(tp) *tp=0;
    bputs(str2); }

else if(!strcmp(sp,"FIRSTREAL")) {
    strcpy(str2,useron.name);
    tp=strchr(str2,SP);
    if(tp) *tp=0;
    bputs(str2); }

else if(!strcmp(sp,"LASTREAL")) {
    tp=strrchr(useron.name,SP);
    if(tp) tp++;
    else tp=useron.name;
    bputs(tp); }

else if(!strcmp(sp,"MAILW"))
	bprintf("%u",getmail(useron.number,0));

else if(!strcmp(sp,"MAILP"))
	bprintf("%u",getmail(useron.number,1));

else if(!strncmp(sp,"MAILW:",6))
	bprintf("%u",getmail(atoi(sp+6),0));

else if(!strncmp(sp,"MAILP:",6))
	bprintf("%u",getmail(atoi(sp+6),1));

else return(0);

return(len);
}

/****************************************************************************/
/* Returns 0 if invalid @ code. Returns length of @ code if valid.          */
/****************************************************************************/
int atcodes(char *instr)
{
	char	str[64],str2[64],*p,*tp,*sp;
    int     i,len;
	long	l;
    stats_t stats;
    node_t  node;
    struct  dfree d;

sprintf(str,"%.40s",instr);
tp=strchr(str+1,'@');
if(!tp)                 /* no terminating @ */
    return(0);
sp=strchr(str+1,SP);
if(sp && sp<tp)         /* space before terminating @ */
    return(0);
len=(tp-str)+1;
(*tp)=0;
sp=(str+1);

if(!strcmp(sp,"VER"))
    bputs(VERSION);

else if(!strcmp(sp,"REV"))
	bprintf("%c",REVISION);

else if(!strcmp(sp,"BBS") || !strcmp(sp,"BOARDNAME"))
    bputs(sys_name);

else if(!strcmp(sp,"BAUD") || !strcmp(sp,"BPS"))
	bprintf("%lu",cur_rate);

else if(!strcmp(sp,"CONN"))
	bputs(connection);

else if(!strcmp(sp,"SYSOP"))
    bputs(sys_op);

else if(!strcmp(sp,"LOCATION"))
	bputs(sys_location);

else if(!strcmp(sp,"NODE"))
    bprintf("%u",node_num);

else if(!strcmp(sp,"TNODE"))
    bprintf("%u",sys_nodes);

else if(!strcmp(sp,"TIME") || !strcmp(sp,"SYSTIME")) {
    now=time(NULL);
    unixtodos(now,&date,&curtime);
    bprintf("%02d:%02d %s"
        ,curtime.ti_hour==0 ? 12
        : curtime.ti_hour>12 ? curtime.ti_hour-12
        : curtime.ti_hour, curtime.ti_min, curtime.ti_hour>11 ? "pm":"am"); }

else if(!strcmp(sp,"DATE") || !strcmp(sp,"SYSDATE")) {
    now=time(NULL);
    bputs(unixtodstr(now,str2)); }

else if(!strcmp(sp,"TMSG")) {
	l=0;
    for(i=0;i<total_subs;i++)
		l+=getposts(i); 		/* l=total posts */
    bprintf("%lu",l); }

else if(!strcmp(sp,"TUSER"))
    bprintf("%u",lastuser());

else if(!strcmp(sp,"TFILE")) {
	l=0;
    for(i=0;i<total_dirs;i++)
        l+=getfiles(i);
    bprintf("%lu",l); }

else if(!strcmp(sp,"TCALLS") || !strcmp(sp,"NUMCALLS")) {
    getstats(0,&stats);
    bprintf("%lu",stats.logons); }

else if(!strcmp(sp,"PREVON") || !strcmp(sp,"LASTCALLERNODE")
    || !strcmp(sp,"LASTCALLERSYSTEM"))
    bputs(lastuseron);

else if(!strcmp(sp,"CLS"))
    CLS;

else if(!strcmp(sp,"PAUSE") || !strcmp(sp,"MORE"))
    pause();

else if(!strcmp(sp,"NOPAUSE") || !strcmp(sp,"POFF"))
    sys_status^=SS_PAUSEOFF;

else if(!strcmp(sp,"PON") || !strcmp(sp,"AUTOMORE"))
    sys_status^=SS_PAUSEON;

/* NOSTOP */

/* STOP */

else if(!strcmp(sp,"BELL") || !strcmp(sp,"BEEP"))
    outchar(7);

// else if(!strcmp(sp,"EVENT"))
//	  bputs(sectostr(sys_eventtime,str2));

/* LASTCALL */

else if(!strncmp(sp,"NODE",4)) {
    i=atoi(sp+4);
    if(i && i<=sys_nodes) {
        getnodedat(i,&node,0);
        printnodedat(i,node); } }

else if(!strcmp(sp,"WHO"))
    whos_online(1);

/* User Codes */

else if(!strcmp(sp,"USER") || !strcmp(sp,"ALIAS") || !strcmp(sp,"NAME"))
    bputs(useron.alias);

else if(!strcmp(sp,"FIRST")) {
    strcpy(str2,useron.alias);
    tp=strchr(str2,SP);
    if(tp) *tp=0;
    bputs(str2); }

else if(!strcmp(sp,"PHONE") || !strcmp(sp,"HOMEPHONE")
    || !strcmp(sp,"DATAPHONE") || !strcmp(sp,"DATA"))
    bputs(useron.phone);

else if(!strcmp(sp,"ADDR1"))
    bputs(useron.address);

else if(!strcmp(sp,"FROM"))
    bputs(useron.location);

else if(!strcmp(sp,"CITY")) {
    strcpy(str2,useron.location);
    p=strchr(str2,',');
    if(p) {
        *p=0;
        bputs(str2); } }

else if(!strcmp(sp,"STATE")) {
    p=strchr(useron.location,',');
    if(p) {
        p++;
        if(*p==SP)
            p++;
        bputs(p); } }

else if(!strcmp(sp,"CPU") || !strcmp(sp,"HOST"))
    bputs(useron.comp);

else if(!strcmp(sp,"BDATE"))
    bputs(useron.birth);

else if(!strcmp(sp,"CALLS") || !strcmp(sp,"NUMTIMESON"))
    bprintf("%u",useron.logons);

else if(!strcmp(sp,"MEMO"))
    bputs(unixtodstr(useron.pwmod,str2));

else if(!strcmp(sp,"SEC") || !strcmp(sp,"SECURITY"))
    bprintf("%u",useron.level);

else if(!strcmp(sp,"SINCE"))
    bputs(unixtodstr(useron.firston,str2));

else if(!strcmp(sp,"TIMEON") || !strcmp(sp,"TIMEUSED")) {
	now=time(NULL);
	bprintf("%u",(now-logontime)/60); }

else if(!strcmp(sp,"TUSED")) {              /* Synchronet only */
	now=time(NULL);
	bputs(sectostr(now-logontime,str2)+1); }

else if(!strcmp(sp,"TLEFT")) {              /* Synchronet only */
	gettimeleft();
	bputs(sectostr(timeleft,str2)+1); }

else if(!strcmp(sp,"TPERD"))                /* Synchronet only */
    bputs(sectostr(level_timeperday[useron.level],str)+1);

else if(!strcmp(sp,"TPERC"))                /* Synchronet only */
    bputs(sectostr(level_timepercall[useron.level],str)+1);

else if(!strcmp(sp,"TIMELIMIT"))
    bprintf("%u",level_timepercall[useron.level]);

else if(!strcmp(sp,"MINLEFT") || !strcmp(sp,"LEFT") || !strcmp(sp,"TIMELEFT")) {
	gettimeleft();
	bprintf("%u",timeleft/60); }

else if(!strcmp(sp,"LASTON"))
    bputs(timestr(&useron.laston));

else if(!strcmp(sp,"LASTDATEON"))
    bputs(unixtodstr(useron.laston,str2));

else if(!strcmp(sp,"LASTTIMEON")) {
    unixtodos(useron.laston,&date,&curtime);
    bprintf("%02d:%02d %s"
        ,curtime.ti_hour==0 ? 12
        : curtime.ti_hour>12 ? curtime.ti_hour-12
        : curtime.ti_hour, curtime.ti_min, curtime.ti_hour>11 ? "pm":"am"); }

else if(!strcmp(sp,"MSGLEFT") || !strcmp(sp,"MSGSLEFT"))
    bprintf("%u",useron.posts);

else if(!strcmp(sp,"MSGREAD"))
    bprintf("%u",posts_read);

else if(!strcmp(sp,"FREESPACE")) {
    if(temp_dir[1]==':')
        i=temp_dir[0]-'A'+1;
    else i=0;
    getdfree(i,&d);
    if(d.df_sclus!=0xffff)
        bprintf("%lu",(ulong)d.df_bsec*(ulong)d.df_sclus*(ulong)d.df_avail); }

else if(!strcmp(sp,"UPBYTES"))
    bprintf("%lu",useron.ulb);

else if(!strcmp(sp,"UPK"))
    bprintf("%lu",useron.ulb/1024L);

else if(!strcmp(sp,"UPS") || !strcmp(sp,"UPFILES"))
    bprintf("%u",useron.uls);

else if(!strcmp(sp,"DLBYTES"))
    bprintf("%lu",useron.dlb);

else if(!strcmp(sp,"DOWNK"))
    bprintf("%lu",useron.dlb/1024L);

else if(!strcmp(sp,"DOWNS") || !strcmp(sp,"DLFILES"))
    bprintf("%u",useron.dls);

else if(!strcmp(sp,"LASTNEW"))
    bputs(unixtodstr(ns_time,str2));

else if(!strcmp(sp,"NEWFILETIME"))
	bputs(timestr(&ns_time));

/* MAXDL */

else if(!strcmp(sp,"MAXDK") || !strcmp(sp,"DLKLIMIT") || !strcmp(sp,"KBLIMIT"))
    bprintf("%lu",level_freecdtperday[useron.level]/1024L);

else if(!strcmp(sp,"DAYBYTES"))     /* amt of free cdts used today */
    bprintf("%lu",level_freecdtperday[useron.level]-useron.freecdt);

else if(!strcmp(sp,"BYTELIMIT"))
    bprintf("%lu",level_freecdtperday[useron.level]);

else if(!strcmp(sp,"KBLEFT"))
    bprintf("%lu",(useron.cdt+useron.freecdt)/1024L);

else if(!strcmp(sp,"BYTESLEFT"))
    bprintf("%lu",useron.cdt+useron.freecdt);

else if(!strcmp(sp,"CONF"))
    bprintf("%s %s"
		,usrgrps ? grp[usrgrp[curgrp]]->sname :nulstr
		,usrgrps ? sub[usrsub[curgrp][cursub[curgrp]]]->sname : nulstr);

else if(!strcmp(sp,"CONFNUM"))
    bprintf("%u %u",curgrp+1,cursub[curgrp]+1);

else if(!strcmp(sp,"NUMDIR"))
	bprintf("%u %u",usrlibs ? curlib+1 : 0,usrlibs ? curdir[curlib]+1 : 0);

else if(!strcmp(sp,"EXDATE") || !strcmp(sp,"EXPDATE"))
    bputs(unixtodstr(useron.expire,str2));

else if(!strcmp(sp,"EXPDAYS")) {
    now=time(NULL);
	l=useron.expire-now;
	if(l<0)
		l=0;
	bprintf("%u",l/(1440L*60L)); }

else if(!strcmp(sp,"MEMO1"))
    bputs(useron.note);

else if(!strcmp(sp,"MEMO2") || !strcmp(sp,"COMPANY"))
    bputs(useron.name);

else if(!strcmp(sp,"ZIP"))
	bputs(useron.zipcode);

else if(!strcmp(sp,"HANGUP"))
    hangup();

else
	return(syncatcodes(sp,len));

return(len);
}



