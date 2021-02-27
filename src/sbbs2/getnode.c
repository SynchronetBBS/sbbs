#line 1 "GETNODE.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "cmdshell.h"

/****************************************************************************/
/* Reads the data for node number 'number' into the structure 'node'        */
/* from NODE.DAB															*/
/* if lockit is non-zero, locks this node's record. putnodedat() unlocks it */
/****************************************************************************/
void getnodedat(uint number, node_t *node, char lockit)
{
	char str[256];
	int count=0;

if(!(sys_status&SS_NODEDAB))
	return;
if(!number || number>sys_nodes) {
	errormsg(WHERE,ERR_CHK,"node number",number);
	return; }
number--;	/* make zero based */
while(count<LOOP_NODEDAB) {
	if(count>10)
		mswait(55);
	lseek(nodefile,(long)number*sizeof(node_t),SEEK_SET);
	if(lockit
		&& lock(nodefile,(long)number*sizeof(node_t),sizeof(node_t))==-1) {
		count++;
		continue; }
	if(read(nodefile,node,sizeof(node_t))==sizeof(node_t))
		break;
	count++; }
if(count>(LOOP_NODEDAB/2) && count!=LOOP_NODEDAB) {
	sprintf(str,"NODE.DAB COLLISION - Count: %d",count);
	logline("!!",str); }
if(count==LOOP_NODEDAB) {
	errormsg(WHERE,ERR_READ,"NODE.DAB",number+1);
	return; }
}

/****************************************************************************/
/* Synchronizes all the nodes knowledge of the other nodes' actions, mode,  */
/* status and other flags.                                                  */
/* Assumes that getnodedat(node_num,&thisnode,0) was called right before it */
/* is called.  																*/
/****************************************************************************/
void nodesync()
{
	static user_t user;
	static int inside;
	char str[256],today[32];
	int i,j,atr=curatr; /* was lclatr(-1) 01/29/96 */
	node_t node;

if(inside) return;
inside=1;

if(thisnode.action!=action) {
	getnodedat(node_num,&thisnode,1);
	thisnode.action=action;
	putnodedat(node_num,thisnode); }

criterrs=thisnode.errors;

if(sys_status&SS_USERON) {
	if(!(sys_status&SS_NEWDAY)) {
		now=time(NULL);
		unixtodstr(logontime,str);
		unixtodstr(now,today);
		if(strcmp(str,today)) { /* New day, clear "today" user vars */
			sys_status|=SS_NEWDAY;	// So we don't keep doing this over&over
			putuserrec(useron.number,U_ETODAY,5,"0");
			putuserrec(useron.number,U_PTODAY,5,"0");
			putuserrec(useron.number,U_TTODAY,5,"0");
			putuserrec(useron.number,U_LTODAY,5,"0");
			putuserrec(useron.number,U_TEXTRA,5,"0");
			putuserrec(useron.number,U_FREECDT,10
				,ultoa(level_freecdtperday[useron.level],str,10));
			getuserdat(&useron); } }
	if(thisnode.misc&NODE_UDAT && !(useron.rest&FLAG('G'))) {   /* not guest */
		getuserdat(&useron);
		getnodedat(node_num,&thisnode,1);
		thisnode.misc&=~NODE_UDAT;
		putnodedat(node_num,thisnode); }
	if(thisnode.misc&NODE_MSGW)
		getsmsg(useron.number); 	/* getsmsg clears MSGW flag */
	if(thisnode.misc&NODE_NMSG)
		getnmsg(); }				/* getnmsg clears NMSG flag */

if(sync_mod[0])
	exec_bin(sync_mod,&main_csi);

if(thisnode.misc&NODE_INTR) {
    bputs(text[NodeLocked]);
	logline(nulstr,"Interrupted");
    hangup();
	inside=0;
    return; }

if(sys_status&SS_USERON && memcmp(&user,&useron,sizeof(user_t))) {
//	  lputc(7);
    getusrdirs();
    getusrsubs();
    user=useron; }

if(sys_status&SS_USERON && online && (timeleft/60)<(5-timeleft_warn)
	&& !SYSOP) {
	timeleft_warn=5-(timeleft/60);
	attr(LIGHTGRAY);
	bprintf(text[OnlyXminutesLeft]
		,((ushort)timeleft/60)+1,(timeleft/60) ? "s" : nulstr); }

attr(atr);	/* replace original attributes */
inside=0;
}

/****************************************************************************/
/* Prints short messages waiting for this node, if any...                   */
/****************************************************************************/
void getnmsg()
{
	char str[256], HUGE16 *buf;
	int file;
	long length;

getnodedat(node_num,&thisnode,1);
thisnode.misc&=~NODE_NMSG;          /* clear the NMSG flag */
putnodedat(node_num,thisnode);

#if 0
/* Only for v1b rev 2-5 and XSDK v2.1x compatibility */
sprintf(str,"%sMSGS\\N%3.3u.IXB",data_dir,node_num);
if((ixb=nopen(str,O_RDONLY))!=-1) {
    read(ixb,&offset,4);
    chsize(ixb,0L);
    close(ixb);
    remove(str); }
#endif

sprintf(str,"%sMSGS\\N%3.3u.MSG",data_dir,node_num);
if(flength(str)<1L)
	return;
if((file=nopen(str,O_RDWR))==-1) {
	/**
		errormsg(WHERE,ERR_OPEN,str,O_RDWR);
	**/
    return; }
length=filelength(file);
if(!length) {
	close(file);
	return; }
if((buf=LMALLOC(length+1))==NULL) {
    close(file);
    errormsg(WHERE,ERR_ALLOC,str,length+1);
    return; }
if(lread(file,buf,length)!=length) {
    close(file);
    FREE(buf);
    errormsg(WHERE,ERR_READ,str,length);
    return; }
chsize(file,0L);
close(file);
buf[length]=0;

if(thisnode.action==NODE_MAIN || thisnode.action==NODE_XFER
    || sys_status&SS_IN_CTRLP) {
    CRLF; }
putmsg(buf,P_NOATCODES);
LFREE(buf);
}

/****************************************************************************/
/* 'ext' must be at least 128 bytes!                                        */
/****************************************************************************/
void getnodeext(uint number, char *ext)
{
    char str[256];
    int count=0;

if(!(sys_status&SS_NODEDAB))
    return;
if(!number || number>sys_nodes) {
    errormsg(WHERE,ERR_CHK,"node number",number);
    return; }
number--;   /* make zero based */
while(count<LOOP_NODEDAB) {
    if(count>10)
        mswait(55);
    if(lock(node_ext,(long)number*128L,128)==-1) {
        count++;
        continue; }
    lseek(node_ext,(long)number*128L,SEEK_SET);
    if(read(node_ext,ext,128)==128)
        break;
    count++; }
unlock(node_ext,(long)number*128L,128);
if(count>(LOOP_NODEDAB/2) && count!=LOOP_NODEDAB) {
    sprintf(str,"NODE.EXB COLLISION - Count: %d",count);
    logline("!!",str); }
if(count==LOOP_NODEDAB) {
    errormsg(WHERE,ERR_READ,"NODE.EXB",number+1);
    return; }
}


/****************************************************************************/
/* Prints short messages waiting for 'usernumber', if any...                */
/* then deletes them.                                                       */
/****************************************************************************/
void getsmsg(int usernumber)
{
	char str[256], HUGE16 *buf;
    int file;
    long length;

sprintf(str,"%sMSGS\\%4.4u.MSG",data_dir,usernumber);
if(flength(str)<1L)
    return;
if((file=nopen(str,O_RDWR))==-1) {
    errormsg(WHERE,ERR_OPEN,str,O_RDWR);
    return; }
length=filelength(file);
if((buf=LMALLOC(length+1))==NULL) {
    close(file);
    errormsg(WHERE,ERR_ALLOC,str,length+1);
    return; }
if(lread(file,buf,length)!=length) {
    close(file);
    FREE(buf);
    errormsg(WHERE,ERR_READ,str,length);
    return; }
chsize(file,0L);
close(file);
buf[length]=0;
getnodedat(node_num,&thisnode,0);
if(thisnode.action==NODE_MAIN || thisnode.action==NODE_XFER
    || sys_status&SS_IN_CTRLP) {
    CRLF; }
if(thisnode.misc&NODE_MSGW) {
    getnodedat(node_num,&thisnode,1);
    thisnode.misc&=~NODE_MSGW;
    putnodedat(node_num,thisnode); }
putmsg(buf,P_NOATCODES);
LFREE(buf);
}

