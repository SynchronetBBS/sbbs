#line 1 "CON_MID.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

extern char *wday[];	/* 3 char days of week */

/****************************************************************************/
/* Waits for remote or local user to hit a key. Inactivity timer is checked */
/* and hangs up if inactive for 4 minutes. Returns key hit, or uppercase of */
/* key hit if mode&K_UPPER or key out of KEY BUFFER. Does not print key.    */
/* Called from functions all over the place.                                */
/****************************************************************************/
char getkey(long mode)
{
	char ch,coldkey,c=0,spin=random(5);

if(!online)
    return(0);
sys_status&=~SS_ABORT;
if((sys_status&SS_USERON || action==NODE_DFLT) && !(mode&K_GETSTR))
    mode|=(useron.misc&SPIN);
lncntr=0;
timeout=time(NULL);
if(mode&K_SPIN)
    outchar(' ');
do {
    checkline();    /* check to make sure remote user is still online */
	if(online==ON_REMOTE && console&CON_R_INPUT && rioctl(IOSTATE)&ABORT) {
        rioctl(IOCS|ABORT);
        sys_status|=SS_ABORT;
        if(mode&K_SPIN) /* back space once if on spinning cursor */
            bputs("\b \b");
        return(0); }
	if(sys_status&SS_SYSPAGE)
		beep(random(800),100);
    if(mode&K_SPIN)
        switch(spin) {
            case 0:
                switch(c++) {
                    case 0:
                        outchar(BS);
                        outchar('³');
                        break;
                    case 10:
                        outchar(BS);
                        outchar('/');
                        break;
                    case 20:
                        outchar(BS);
                        outchar('Ä');
                        break;
                    case 30:
                        outchar(BS);
                        outchar('\\');
                        break;
                    case 40:
                        c=0;
                        break;
                    default:
                        if(!inDV && !(node_misc&NM_WINOS2))
							mswait(DELAY_SPIN);
                        break;  }
                break;
            case 1:
                switch(c++) {
                    case 0:
                        outchar(BS);
                        outchar('°');
                        break;
                    case 10:
                        outchar(BS);
                        outchar('±');
                        break;
                    case 20:
                        outchar(BS);
                        outchar('²');
                        break;
                    case 30:
                        outchar(BS);
                        outchar('Û');
                        break;
                    case 40:
                        outchar(BS);
                        outchar('²');
                        break;
                    case 50:
                        outchar(BS);
                        outchar('±');
                        break;
                    case 60:
                        c=0;
                        break;
                    default:
                        if(!inDV && !(node_misc&NM_WINOS2))
							mswait(DELAY_SPIN);
                        break;  }
                break;
            case 2:
                switch(c++) {
                    case 0:
                        outchar(BS);
                        outchar('-');
                        break;
                    case 10:
                        outchar(BS);
                        outchar('=');
                        break;
                    case 20:
                        outchar(BS);
                        outchar('ð');
                        break;
                    case 30:
                        outchar(BS);
                        outchar('=');
                        break;
                    case 40:
                        c=0;
                        break;
                    default:
                        if(!inDV && !(node_misc&NM_WINOS2))
							mswait(DELAY_SPIN);
                        break;  }
                break;
            case 3:
                switch(c++) {
                    case 0:
                        outchar(BS);
                        outchar('Ú');
                        break;
                    case 10:
                        outchar(BS);
                        outchar('À');
                        break;
                    case 20:
                        outchar(BS);
                        outchar('Ù');
                        break;
                    case 30:
                        outchar(BS);
                        outchar('¿');
                        break;
                    case 40:
                        c=0;
                        break;
                    default:
                        if(!inDV && !(node_misc&NM_WINOS2))
							mswait(DELAY_SPIN);
                        break;  }
                break;
            case 4:
                switch(c++) {
                    case 0:
                        outchar(BS);
                        outchar('Ü');
                        break;
                    case 10:
                        outchar(BS);
                        outchar('Þ');
                        break;
                    case 20:
                        outchar(BS);
                        outchar('ß');
                        break;
                    case 30:
                        outchar(BS);
                        outchar('Ý');
                        break;
                    case 40:
                        c=0;
                        break;
                    default:
                        if(!inDV && !(node_misc&NM_WINOS2))
							mswait(DELAY_SPIN);
                        break;  }
                break; }
    if(keybuftop!=keybufbot) {
        ch=keybuf[keybufbot++];
        if(keybufbot==KEY_BUFSIZE)
            keybufbot=0; }
    else
        ch=inkey(mode);
    if(sys_status&SS_ABORT)
        return(0);
    now=time(NULL);
    if(ch) {
        if(mode&K_NUMBER && isprint(ch) && !isdigit(ch))
            continue;
        if(mode&K_ALPHA && isprint(ch) && !isalpha(ch))
            continue;
        if(mode&K_NOEXASC && ch&0x80)
            continue;
        if(mode&K_SPIN)
            bputs("\b \b");
		if(mode&K_COLD && ch>SP && useron.misc&COLDKEYS) {
			if(mode&K_UPPER)
				outchar(toupper(ch));
			else
				outchar(ch);
			while((coldkey=inkey(mode))==0 && online && !(sys_status&SS_ABORT))
				checkline();
			bputs("\b \b");
			if(coldkey==BS)
				continue;
			if(coldkey>SP)
				ungetkey(coldkey); }
        if(mode&K_UPPER)
            return(toupper(ch));
        return(ch); }
    if(sys_status&SS_USERON && !(sys_status&SS_LCHAT)) gettimeleft();
    else if(online &&
        ((node_dollars_per_call && now-answertime>SEC_BILLING)
        || (now-answertime>SEC_LOGON && !(sys_status&SS_LCHAT)))) {
        console&=~(CON_R_ECHOX|CON_L_ECHOX);
        console|=(CON_R_ECHO|CON_L_ECHO);
        bputs(text[TakenTooLongToLogon]);
        hangup(); }
	if(sys_status&SS_USERON && online && (timeleft/60)<(5-timeleft_warn)
		&& !SYSOP && !(sys_status&SS_LCHAT)) {
        timeleft_warn=5-(timeleft/60);
        SAVELINE;
		attr(LIGHTGRAY);
        bprintf(text[OnlyXminutesLeft]
			,((ushort)timeleft/60)+1,(timeleft/60) ? "s" : nulstr);
        RESTORELINE; }

	if(online==ON_LOCAL && node_misc&NM_NO_INACT)
		timeout=now;
	if(now-timeout>=sec_warn) { 					/* warning */
        if(sys_status&SS_USERON) {
            SAVELINE;
            bputs(text[AreYouThere]); }
        else
            bputs("\7\7");
		while(!inkey(0) && online && now-timeout>=sec_warn) {
            now=time(NULL);
			if(now-timeout>=sec_hangup) {
                if(online==ON_REMOTE) {
                    console|=CON_R_ECHO;
                    console&=~CON_R_ECHOX; }
                bputs(text[CallBackWhenYoureThere]);
                logline(nulstr,"Inactive");
                hangup();
                return(0); }
            mswait(100); }
        if(sys_status&SS_USERON) {
            bputs("\r\1n\1>");
            RESTORELINE; }
        timeout=now; }

    } while(online);
return(0);
}

/****************************************************************************/
/* This function lists users that are online.                               */
/* If listself is true, it will list the current node.                      */
/* Returns number of active nodes (not including current node).             */
/****************************************************************************/
int whos_online(char listself)
{
    int i,j;
    node_t node;

CRLF;
bputs(text[NodeLstHdr]);
for(j=0,i=1;i<=sys_nodes && i<=sys_lastnode;i++) {
    getnodedat(i,&node,0);
    if(i==node_num) {
        if(listself)
            printnodedat(i,node);
        continue; }
    if(node.status==NODE_INUSE || (SYSOP && node.status==NODE_QUIET)) {
        printnodedat(i,node);
        if(!lastnodemsg)
            lastnodemsg=i;
        j++; } }
if(!j)
    bputs(text[NoOtherActiveNodes]);
return(j);
}

/****************************************************************************/
/* Displays the information for node number 'number' contained in 'node'    */
/****************************************************************************/
void printnodedat(uint number, node_t node)
{
    uint i;
    char hour,mer[3];

attr(color[clr_nodenum]);
bprintf("%3d  ",number);
attr(color[clr_nodestatus]);
switch(node.status) {
    case NODE_WFC:
        bputs("Waiting for call");
        break;
    case NODE_OFFLINE:
        bputs("Offline");
        break;
    case NODE_NETTING:
        bputs("Networking");
        break;
    case NODE_LOGON:
        bputs("At logon prompt");
        break;
    case NODE_EVENT_WAITING:
        bputs("Waiting for all nodes to become inactive");
        break;
    case NODE_EVENT_LIMBO:
        bprintf("Waiting for node %d to finish external event",node.aux);
        break;
    case NODE_EVENT_RUNNING:
        bputs("Running external event");
        break;
    case NODE_NEWUSER:
        attr(color[clr_nodeuser]);
        bputs("New user");
        attr(color[clr_nodestatus]);
        bputs(" applying for access ");
        if(!node.connection)
            bputs("Locally");
        else
            bprintf("at %ubps",node.connection);
        break;
    case NODE_QUIET:
        if(!SYSOP) {
            bputs("Waiting for call");
            break; }
    case NODE_INUSE:
		if(node.misc&NODE_EXT) {
			getnodeext(number,tmp);
			bputs(tmp);
			break; }
        attr(color[clr_nodeuser]);
        if(node.misc&NODE_ANON && !SYSOP)
            bputs("UNKNOWN USER");
        else
            bputs(username(node.useron,tmp));
        attr(color[clr_nodestatus]);
        bputs(" ");
        switch(node.action) {
            case NODE_MAIN:
                bputs("at main menu");
                break;
            case NODE_RMSG:
                bputs("reading messages");
                break;
            case NODE_RMAL:
                bputs("reading mail");
                break;
            case NODE_RSML:
                bputs("reading sent mail");
                break;
            case NODE_RTXT:
                bputs("reading text files");
                break;
            case NODE_PMSG:
                bputs("posting message");
                break;
            case NODE_SMAL:
                bputs("sending mail");
                break;
            case NODE_AMSG:
                bputs("posting auto-message");
                break;
            case NODE_XTRN:
                if(!node.aux)
                    bputs("at external program menu");
                else {
                    bputs("running ");
                    i=node.aux-1;
					if(SYSOP || chk_ar(xtrn[i]->ar,useron))
                        bputs(xtrn[node.aux-1]->name);
                    else
                        bputs("external program"); }
                break;
            case NODE_DFLT:
                bputs("changing defaults");
                break;
            case NODE_XFER:
                bputs("at transfer menu");
                break;
            case NODE_RFSD:
                bprintf("retrieving from device #%d",node.aux);
                break;
            case NODE_DLNG:
                bprintf("downloading");
                break;
            case NODE_ULNG:
                bputs("uploading");
                break;
            case NODE_BXFR:
                bputs("transferring bidirectional");
                break;
            case NODE_LFIL:
                bputs("listing files");
                break;
            case NODE_LOGN:
                bputs("logging on");
                break;
            case NODE_LCHT:
                bprintf("in local chat with %s",sys_op);
                break;
            case NODE_MCHT:
                if(node.aux) {
                    bprintf("in multinode chat channel %d",node.aux&0xff);
                    if(node.aux&0x1f00) { /* password */
                        outchar('*');
                        if(SYSOP)
                            bprintf(" %s",unpackchatpass(tmp,node)); } }
                else
                    bputs("in multinode global chat channel");
                break;
            case NODE_PAGE:
                bprintf("paging node %u for private chat",node.aux);
                break;
            case NODE_PCHT:
                bprintf("in private chat with node %u",node.aux);
                break;
            case NODE_GCHT:
				i=node.aux;
				if(i>=total_gurus)
					i=0;
				bprintf("chatting with %s",guru[i]->name);
                break;
            case NODE_CHAT:
                bputs("in chat section");
                break;
            case NODE_TQWK:
                bputs("transferring QWK packet");
                break;
            case NODE_SYSP:
                bputs("performing sysop activities");
                break;
            default:
                bputs(itoa(node.action,tmp,10));
                break;  }
        if(!node.connection)
            bputs(" locally");
        else
            bprintf(" at %ubps",node.connection);
        if(node.action==NODE_DLNG) {
            if(sys_misc&SM_MILITARY) {
                hour=node.aux/60;
                mer[0]=0; }
            else if((node.aux/60)>=12) {
                if(node.aux/60==12)
                    hour=12;
                else
                    hour=(node.aux/60)-12;
                strcpy(mer,"pm"); }
            else {
                if((node.aux/60)==0)    /* 12 midnite */
                    hour=12;
                else hour=node.aux/60;
                strcpy(mer,"am"); }
            bprintf(" ETA %02d:%02d %s"
                ,hour,node.aux%60,mer); }
        break; }
i=NODE_LOCK;
if(node.status==NODE_INUSE || SYSOP)
	i|=NODE_POFF|NODE_AOFF|NODE_MSGW|NODE_NMSG;
if(node.misc&i) {
    bputs(" (");
	if(node.misc&(i&NODE_AOFF))
        outchar('A');
    if(node.misc&NODE_LOCK)
        outchar('L');
	if(node.misc&(i&(NODE_MSGW|NODE_NMSG)))
        outchar('M');
	if(node.misc&(i&NODE_POFF))
        outchar('P');
    outchar(')'); }
if(SYSOP && ((node.misc
    &(NODE_ANON|NODE_UDAT|NODE_INTR|NODE_RRUN|NODE_EVENT|NODE_DOWN))
    || node.status==NODE_QUIET)) {
    bputs(" [");
    if(node.misc&NODE_ANON)
        outchar('A');
    if(node.misc&NODE_INTR)
        outchar('I');
    if(node.misc&NODE_RRUN)
        outchar('R');
    if(node.misc&NODE_UDAT)
        outchar('U');
    if(node.status==NODE_QUIET)
        outchar('Q');
    if(node.misc&NODE_EVENT)
        outchar('E');
    if(node.misc&NODE_DOWN)
        outchar('D');
    outchar(']'); }
if(node.errors && SYSOP) {
    attr(color[clr_err]);
	bprintf(" %d error%c",node.errors, node.errors>1 ? 's' : '\0' ); }
attr(LIGHTGRAY);
CRLF;
}

/****************************************************************************/
/* Prints/updates the local status line (line #25) with the user/system     */
/* information. Which info depends on value of statline.                    */
/* Called from several functions                                            */
/****************************************************************************/
void statusline()
{
    int row,col,atr;
    char tmp1[128],tmp2[256],tmp3[256],age;

#ifndef __OS2__
if(lclaes())
    return;
#endif
col=lclwx();
row=lclwy();
STATUSLINE;
lclxy(1,node_scrnlen);
age=getage(useron.birth);
if(sys_status&(SS_CAP|SS_TMPSYSOP)) {
    atr=lclatr((LIGHTGRAY<<4)|BLINK);
    sys_status&SS_CAP ? lputc('C') : lputc(SP);
    sys_status&SS_TMPSYSOP ? lputc('*') : lputc(SP);
    lclatr(LIGHTGRAY<<4); }
else {
    atr=lclatr(LIGHTGRAY<<4);
    lputs("  "); }
switch(statline) {
    case -1:
        lputs("Terminal:  Alt-X Exit  Alt-D DOS  Alt-H Hangup  "
            "Alt-L Logon  Alt-U User Edit");
        break;
    case 0:     /* Alias ML Password Modem Birthday Age Sex Phone */
        lprintf("%-24.24s %02d %-8.8s %-8.8s %-8.8s %02d %c %s"
            ,useron.alias,useron.level,sys_misc&SM_ECHO_PW ? useron.pass:"XXXX"
            ,useron.modem,useron.birth,age,useron.sex,useron.phone);
        break;
    case 1:     /* Alias ML RealName Alt-Z for Help */
        lprintf("%-24.24s %02d %-25.25s  "
            ,useron.alias,useron.level,useron.name);
        lputs("Alt-Z for Help");
        break;
    case 2:     /* Alias ML RealName Age Sex Phone */
        lprintf("%-24.24s %02d %-25.25s  %02d %c %s"
            ,useron.alias,useron.level,useron.name,age,useron.sex,useron.phone);
        break;
    case 3:     /* Alias ML Location Phone */
        lprintf("%-24.24s %02d %-30.30s  %s"
            ,useron.alias,useron.level,useron.location,useron.phone);
        break;
    case 4:     /* Alias ML Note Phone */
        lprintf("%-24.24s %02d %-30.30s  %s"
            ,useron.alias,useron.level,useron.note,useron.phone);
        break;
    case 5:     /* Alias ML MF Age Sex Phone */
        lprintf("%-24.24s %02d %-26.26s %02d %c %s"
            ,useron.alias,useron.level,ltoaf(useron.flags1,tmp1)
            ,age,useron.sex,useron.phone);
        break;
    case 6:     /* Alias ML MF Expiration */
        lprintf("%-24.24s %02d %-26.26s Exp: %s"
            ,useron.alias,useron.level,ltoaf(useron.flags1,tmp1)
            ,unixtodstr(useron.expire,tmp2));
        break;
    case 7:     /* Alias ML Firston Laston Expire */
        lprintf("%-24.24s %02d First: %s Last: %s Exp: %s"
            ,useron.alias,useron.level,unixtodstr(useron.firston,tmp1)
            ,unixtodstr(useron.laston,tmp2),unixtodstr(useron.expire,tmp3));
        break;
    case 8:     /* Alias Credits Minutes Expire */
        lprintf("%-24.24s Cdt: %-13.13s Min: %-10luExp: %s"
            ,useron.alias,ultoac(useron.cdt,tmp1),useron.min
            ,unixtodstr(useron.expire,tmp2));
        break;
    case 9:    /* Exemptions Restrictions */
        lprintf("  Exempt:%-26.26s   Restrict:%s"
            ,ltoaf(useron.exempt,tmp1),ltoaf(useron.rest,tmp2));
        break;
    case 10:    /* Computer Modem Handle*/
        lprintf("Comp: %-30.30s  Modem: %-8.8s  Handle: %s"
            ,useron.comp,connection,useron.handle);
        break;
    case 11:    /* StreetAddress Location Zip */
        lprintf("%-30.30s %-30.30s %s"
            ,useron.address,useron.location,useron.zipcode);
        break;
    case 12:    /* UploadBytes Uploads DownloadBytes Downloads Leeches */
        lprintf("Uloads: %-13.13s / %-5u Dloads: %-13.13s / %-5u Leech: %u"
            ,ultoac(useron.ulb,tmp1),useron.uls
            ,ultoac(useron.dlb,tmp2),useron.dls,useron.leech);
        break;
    case 13:    /* Posts Emails Fbacks Waiting Logons Timeon */
        lprintf("P: %-5u E: %-5u F: %-5u W: %-5u L: %-5u T: %-5u"
            ,useron.posts,useron.emails,useron.fbacks,getmail(useron.number,0)
            ,useron.logons,useron.timeon);
        break;
    case 14:    /* NetMail forwarding address */
        lprintf("NetMail: %s",useron.netmail);
        break;
    case 15:    /* Comment */
        lprintf("Comment: %s",useron.comment);
        break;
        }
lputc(CLREOL);
lclxy(75,lclwy());
getnodedat(node_num,&thisnode,0);
if(sys_status&SS_SYSALERT
    || thisnode.misc&(NODE_RRUN|NODE_DOWN|NODE_LOCK|NODE_EVENT)) {
    lclatr((LIGHTGRAY<<4)|BLINK);
    sys_status&SS_SYSALERT ? lputc('A'):lputc(SP);
    thisnode.misc&NODE_RRUN ? lputc('R'):lputc(SP);
    thisnode.misc&NODE_DOWN ? lputc('D'):lputc(SP);
    thisnode.misc&NODE_LOCK ? lputc('L'):lputc(SP);
    thisnode.misc&NODE_EVENT ? lputc('E'):lputc(SP); }
TEXTWINDOW;
lclxy(col,row);
lclatr(atr);
}

/****************************************************************************/
/* Prints PAUSE message and waits for a key stoke                           */
/****************************************************************************/
void pause()
{
	char	ch;
	uchar	tempattrs=curatr; /* was lclatr(-1) */
	int 	i,j;
	long	l=K_UPPER;

RIOSYNC(0);
if(sys_status&SS_ABORT)
    return;
lncntr=0;
if(online==ON_REMOTE)
    rioctl(IOFI);
bputs(text[Pause]);
j=bstrlen(text[Pause]);
if(sys_status&SS_USERON && !(useron.misc&NO_EXASCII) && !(useron.misc&WIP))
	l|=K_SPIN;

ch=getkey(l);
if(ch==text[YN][1] || ch=='Q')
	sys_status|=SS_ABORT;
else if(ch==LF)
	lncntr=rows-2;	/* down arrow == display one more line */
if(text[Pause][0]!='@')
	for(i=0;i<j;i++)
		bputs("\b \b");
getnodedat(node_num,&thisnode,0);
nodesync();
attr(tempattrs);
}

void getlines()
{
if(useron.misc&ANSI && !useron.rows         /* Auto-detect rows */
    && online==ON_REMOTE) {                 /* Remote */
    SYNC;   
    putcom("\x1b[s\x1b[99B\x1b[6n\x1b[u");
    while(online && !rioctl(RXBC) && !lkbrd(1))
        checkline();
    inkey(0); }

}


/****************************************************************************/
/* Prints a file remotely and locally, interpreting ^A sequences, checks    */
/* for pauses, aborts and ANSI. 'str' is the path of the file to print      */
/* Called from functions menu and text_sec                                  */
/****************************************************************************/
void printfile(char *str, int mode)
{
	char HUGE16 *buf;
	int file,wip=0,rip=0;
	long length,savcon=console;
	FILE *stream;

if(strstr(str,".WIP"))
	wip=1;
if(strstr(str,".RIP"))
	rip=1;

if(mode&P_NOABORT || wip || rip) {
	if(online==ON_REMOTE && console&CON_R_ECHO) {
        rioctl(IOCM|ABORT);
        rioctl(IOCS|ABORT); }
    sys_status&=~SS_ABORT; }

if(!tos && !wip && !rip)
	CRLF;

if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
    bputs(text[FileNotFound]);
    if(SYSOP) bputs(str);
    CRLF;
	return; }

if(wip || rip || !(console&CON_L_ECHO)) {
	if(online!=ON_REMOTE || !(console&CON_R_ECHO)) {
		fclose(stream);
		return; }
	console&=~CON_L_ECHO;
	lprintf("PRINTFILE (Remote Only): %s\r\n",str);  }
if(mode&P_OPENCLOSE) {
	length=filelength(file);
	if((buf=MALLOC(length+1L))==NULL) {
		close(file);
		console=savcon;
		errormsg(WHERE,ERR_ALLOC,str,length+1L);
		return; }
	buf[lread(file,buf,length)]=0;
	fclose(stream);
	putmsg(buf,mode);
	FREE((char *)buf); }
else {
	putmsg_fp(stream,filelength(file),mode);
	fclose(stream); }
if((mode&P_NOABORT || wip || rip) && online==ON_REMOTE) {
    SYNC;
    rioctl(IOSM|ABORT); }
if(rip)
	getlines();
console=savcon;
}

void printtail(char *str, int lines, int mode)
{
	char HUGE16 *buf,HUGE16 *p;
	int file,cur=0;
	ulong length,l;

if(mode&P_NOABORT) {
    if(online==ON_REMOTE) {
        rioctl(IOCM|ABORT);
        rioctl(IOCS|ABORT); }
    sys_status&=~SS_ABORT; }
strupr(str);
if(!tos) {
    CRLF; }
if((file=nopen(str,O_RDONLY))==-1) {
    bputs(text[FileNotFound]);
    if(SYSOP) bputs(str);
    CRLF;
	return; }
length=filelength(file);
if((buf=MALLOC(length+1L))==NULL) {
    close(file);
	errormsg(WHERE,ERR_ALLOC,str,length+1L);
	return; }
l=lread(file,buf,length);
buf[l]=0;
close(file);
p=(buf+l)-1;
if(*p==LF) p--;
while(*p && p>buf) {
	if(*p==LF)
		cur++;
	if(cur>=lines) {
		p++;
		break; }
	p--; }
putmsg(p,mode);
if(mode&P_NOABORT && online==ON_REMOTE) {
    SYNC;
    rioctl(IOSM|ABORT); }
FREE((char *)buf);
}

/****************************************************************************/
/* Prints the menu number 'menunum' from the text directory. Checks for ^A  */
/* ,ANSI sequences, pauses and aborts. Usually accessed by user inputing '?'*/
/* Called from every function that has an available menu.                   */
/* The code definitions are as follows:                                     */
/****************************************************************************/
void menu(char *code)
{
    char str[256],path[256];
    int c,i,l;

sys_status&=~SS_ABORT;
if(menu_file[0])
	strcpy(path,menu_file);
else {
	sprintf(str,"%sMENU\\",text_dir);
	if(menu_dir[0]) {
		strcat(str,menu_dir);
		strcat(str,"\\"); }
	strcat(str,code);
	strcat(str,".");
	sprintf(path,"%s%s",str,useron.misc&WIP ? "WIP":"RIP");
	if(!(useron.misc&(RIP|WIP)) || !fexist(path)) {
		sprintf(path,"%sMON",str);
		if((useron.misc&(COLOR|ANSI))!=ANSI || !fexist(path)) {
			sprintf(path,"%sANS",str);
			if(!(useron.misc&ANSI) || !fexist(path))
				sprintf(path,"%sASC",str); } } }

printfile(path,P_OPENCLOSE);
}

/****************************************************************************/
/* Puts a character into the input buffer                                   */
/****************************************************************************/
void ungetkey(char ch)
{

keybuf[keybuftop++]=ch;
if(keybuftop==KEY_BUFSIZE)
    keybuftop=0;
}

