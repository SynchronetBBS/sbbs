#line 1 "MAIN_OVL.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "etext.h"
#include <signal.h>

#ifndef __FLAT__
#include "spawno.h"
#endif

uint asmrev;		// RCIOL.OBJ revision
char llo=0			// Local logon only
	,nmi=0; 		// No modem init
char cidarg[65];	// Caller ID arguments
uint addrio;		// Additional RIOCTL call on init

FILE *nodefile_fp,*node_ext_fp,*logfile_fp;

char prompt[128];

extern char *compile_time,*compile_date;

#ifdef __FLAT__
int mswtyp=0;
#else
extern mswtyp;
extern uint riobp;
#endif

extern char onquiet;
extern char term_ret;
extern ulong connect_rate;	 /* already connected at xbps */
extern char *wday[],*mon[];

#ifdef __OS2__
extern long _timezone=0L;
extern int _daylight=0L;
#else
extern long timezone=0L;
extern int	daylight=0L;
#endif

#ifdef __OS2__
	extern HEV con_out_sem;
	void con_out_thread(void *);
#endif


#ifdef __OS2__
void cbreakh(int sig);
#else
int cbreakh();
#endif

void reset_logon_vars(void);

time_t checktime(void)
{
	struct tm tm;

memset(&tm,0,sizeof(tm));
tm.tm_year=94;
tm.tm_mday=1;
return(mktime(&tm)^0x2D24BD00L);
}

void console_test(void)
{
	int i;
	time_t start=time(NULL);;

lclxy(1,1);
online=ON_LOCAL;
console=CON_L_ECHO;
useron.misc=ANSI|COLOR;
for(i=0;i<500;i++)
	bprintf("\1n%05d\t\1hWhite\1rRed\1bBlue\1gGreen\001\x82"
		"\1mMagenta\t\b\b\1cCyan\1nNormal\1gGreen"
		"\1yBrown\1hYellow\1rR\1gG\1bB\1cC\1mM\1yY\1kK"
		"\1>\r\n"
		,i);
bprintf("\r\n%lu secs\r\n",time(NULL)-start);
getch();
lputc(FF);
}




void  startup(int argc, char *argv[])
{
	char str[256],HUGE16 *buf,c;
    void *v;
	int i,j,events_only=0;
    int file;
    ulong l;
	node_t node;
    struct ffblk ff;
    struct tm *gm;
#ifdef __FLAT__
	uchar *cptab;	/* unused */
#else
	uchar cptab[1024];
    union REGS reg;
#endif

#ifdef __MSDOS__
setcbrk(0);
#endif

#ifndef __FLAT__
if((asmrev=*(&riobp-1))!=23) {
    printf("Wrong rciol.obj\n");
    exit(1); }
#endif

#ifdef __OS2__
signal(SIGINT,cbreakh);
signal(SIGBREAK,cbreakh);
fixkbdmode();
#elif defined(__MSDOS__)
ctrlbrk(cbreakh);
#endif

#ifndef __FLAT__				/* no DESQview under OS/2 */
inDV = 0;
_CX = 0x4445; /* 'DE' */        /* set CX to 4445H; DX to 5351H     */
_DX = 0x5351; /* 'SQ' */        /* (an invalid date)                */
_AX = 0x2B01;					/* DOS' set data function           */
geninterrupt( 0x21 );			/* call DOS 						*/
if ( _AL != 0xFF )				/* if DOS didn't see as invalid     */
	_AX = _BX;					/* AH=major ver; AL=minor ver		*/
else							/* DOS saw as invalid				*/
	_AX = 0;					/* no desqview						*/
inDV = _AX; 					/* Save version in inDV flag		*/
#endif

#ifdef __FLAT__
if(putenv("TZ=UCT0"))
	printf("putenv() failed!\n");
tzset();
#endif

if((l=checktime())!=0) {   /* Check binary time */
	printf("Time problem (%08lx)\n",l);
	exit(1); }

directvideo=1;  /* default to direct video */

_fmode=O_BINARY;
nulstr="";
crlf="\r\n";
strcpy(cap_fname,"CAPTURE.TXT");
sys_status=lncntr=tos=criterrs=keybufbot=keybuftop=lbuflen=slcnt=0L;
debug=1;
curatr=LIGHTGRAY;
errorlevel=0;
logcol=1;
next_event=0;
lastuseron[0]=0;
emsver=0;
emshandle=0xffff;
randomize();
srand(clock());

for(i=0;i<127 && environ[i];i++)  /* save original environment for execl() */
	envp[i]=environ[i];
envp[i]=0;

strcpy(str,getenv("PROMPT"));
if(!strstr(str,"[SBBS]")) {
	sprintf(prompt,"PROMPT=[SBBS] %s",str);
	putenv(prompt); }

comspec=getenv("COMSPEC");

lputc(FF);

#ifndef __FLAT__
node_scrnlen=lclini(0xd<<8);      /* Tab expansion, no CRLF expansion */
#else
node_scrnlen=25;
#endif
lclini(node_scrnlen);
lclatr(LIGHTGRAY);

#ifdef __OS2__	// Test console speed

if(DosCreateEventSem(NULL,&con_out_sem,0,0)) {
	printf("Can't create console output semaphore.\n");
	exit(2); }

if(_beginthread(con_out_thread,1024,NULL)==-1) {
	printf("Can't start console output thread.\n");
	exit(3); }
//console_test();
#endif

#if defined(__OS2__)
lputs(decrypt(VersionNoticeOS2,0));
#elif defined(__WIN32__)
lputs(decrypt(VersionNoticeW32,0));
#else
lputs(decrypt(VersionNoticeDOS,0));
#endif
lputs(decrypt(CopyrightNotice,0));	 /* display copyright notice */
lputc(CR);
lputc(LF);
cidarg[0]=0;
addrio=0;
strcpy(orgcmd,argv[0]);     /* build up original command line */
strcat(orgcmd," ");
for(i=1;i<argc;i++) {
    for(c=0;c<strlen(argv[i]);c++)
        switch(toupper(argv[i][c])) {
            case '/':   /* ignore these characters */
            case '-':
                break;
            case 'B':   /* desqview or other that needs bios video only */
                directvideo=0;
                strcat(orgcmd,"B");
                break;
            case 'C':   /* connected at xbps */
				connect_rate=atol(argv[i]+c+1);
                c=strlen(argv[i]);
                break;
			case 'D':   /* assume DCD is always high */
				sys_status|=SS_DCDHIGH;
				strcat(orgcmd,"D");
				break;
			case 'E':
				next_event=time(NULL)+(atol(argv[i]+c+1)*60L);
				c=strlen(argv[i]);
				break;
			case 'F':
				sys_status|=SS_DAILY;
				break;
#ifdef __OS2__
			case 'H':
				rio_handle=atoi(argv[i]+c+1);
				c=strlen(argv[i]);
				break;
#endif
			case 'I':   /* no modem init */
				strcat(orgcmd,"I");
				nmi=1;
				break;
			case 'L':   /* local logon only */
				strcat(orgcmd,"L");
				llo=1;
				break;
			case 'M':   /* modem debug info */
				strcat(orgcmd,"M");
				sys_status|=SS_MDMDEBUG;
				break;
			case 'O':   /* execute events only */
				events_only=1;
				break;
            case 'Q':   /* quit after one caller - phone off hook */
                strcat(orgcmd,"Q");
                qoc=1;
                break;
			case 'R':   /* additional rioctl call */
				addrio=ahtoul(argv[i]+c+1);
				c=strlen(argv[i]);
				break;
            case 'X':   /* quit after one caller - phone on hook */
                strcat(orgcmd,"X");
                qoc=2;
                break;
			case 'Z':
				sprintf(cidarg,"%-.64s",argv[i]+c+1);
                c=strlen(argv[i]);
				break;
			case 'V':
				lputs(crlf);
				lprintf("Revision %c%s %s %.5s  "
				#ifdef __FLAT__
					"RIOLIB %u.%02u"
				#else
					"RCIOL %u"
				#endif
					"  SMBLIB %s  BCC %X.%02X"
					,REVISION,BETA,compile_date,compile_time
				#ifdef __FLAT__
					,rioctl(GVERS)/100,rioctl(GVERS)%100
				#else
					,rioctl(GVERS)
				#endif
					,smb_lib_ver()
					,__BORLANDC__>>8
					,__BORLANDC__&0xff);
				lputs(crlf);
				bail(0);
            default:
				lputs("\r\nusage: sbbs [bdmoqxfliv] [c#] [e#] [zs] [r#] [h#]"
					"\r\n\r\n"
                    "b  = use bios for video\r\n"
					"d  = assume DCD is always high\r\n"
					"m  = modem debug output\r\n"
                    "q  = quit after one call (phone off hook)\r\n"
                    "x  = quit after one call\r\n"
					"o  = execute events only and then quit\r\n"
					"c# = connected at # bps (ex: c2400)\r\n"
					"e# = external event in # minutes (ex: e30)\r\n"
					"f  = force daily event\r\n"
					"l  = local logon only\r\n"
					"i  = no modem initialization\r\n"
					"zs = use s for caller-id information\r\n"
					"r# = additional rioctl call during port init\r\n"
					"h# = open port handle (SBBS4OS2 only)\r\n"
					"v  = version information\r\n"
					);
                bail(0); } }


node_disk=getdisk();
getcwd(node_dir,MAXDIR);        /* Get current Directory    */
if(strlen(node_dir)>40) {
    lputs("\r\n\7Current Directory is too long for bbs to run reliably.\r\n");
    bail(1); }
strcat(node_dir,"\\");
initdata();                     /* auto-sense scrnlen can be overridden */

mswtyp=0;
if(node_misc&NM_INT28)
	mswtyp|=TS_INT28;
if(node_misc&NM_WINOS2)
	mswtyp|=TS_WINOS2;
if(node_misc&NM_NODV)
	mswtyp|=TS_NODV;

#ifndef __FLAT__

__spawn_ext = (node_swap & SWAP_EXT) != 0 ;
__spawn_ems = (node_swap & SWAP_EMS) != 0 ;
__spawn_xms = (node_swap & SWAP_XMS) != 0 ;

#endif

#ifndef __FLAT__				 /* no EMS under OS/2 */
if(node_misc&NM_EMSOVL) {
	lputs("\r\nEMS: ");
	if((i=open("EMMXXXX0",O_RDONLY))==-1)
		lputs("not installed.");
	else {
		close(i);
		reg.h.ah=0x46;			/* Get EMS version */
		int86(0x67,&reg,&reg);
		if(reg.h.ah)
			lputs("\7error getting version.");
		else {
			lprintf("Version %u.%u ",(reg.h.al&0xf0)>>4,reg.h.al&0xf);
			emsver=reg.h.al;
			reg.h.ah=0x4b;		/* get handle count */
			int86(0x67,&reg,&reg);
			if(reg.h.ah)
				lputs("\7error getting handle count.");
			else {
				if(_OvrInitEms(0,0,23))    /* use up to 360K */
					lprintf("allocation failed.");
				else
					emshandle=reg.x.bx; } } } }
#endif

sprintf(str,"%s%s",ctrl_dir,"NODE.DAB");
if((nodefile_fp=fnopen(&nodefile,str
	,O_DENYNONE|O_RDWR|O_CREAT))==NULL) {
    lprintf("\r\n\7Error opening/creating %s\r\n",str);
    exit(1); }
sprintf(str,"%s%s",ctrl_dir,"NODE.EXB");
if((node_ext_fp=fnopen(&node_ext,str,O_DENYNONE|O_RDWR|O_CREAT))==NULL) {
    lprintf("\r\n\7Error opening/creating %s\r\n",str);
    exit(1); }
memset(&node,0,sizeof(node_t));  /* write NULL to node struct */
node.status=NODE_OFFLINE;
while(filelength(nodefile)<sys_nodes*sizeof(node_t)) {
    lseek(nodefile,0L,SEEK_END);
    write(nodefile,&node,sizeof(node_t)); }
if(lock(nodefile,(node_num-1)*sizeof(node_t),sizeof(node_t))
	|| unlock(nodefile,(node_num-1)*sizeof(node_t),sizeof(node_t))) {
    lprintf("\r\n\7File locking failed.\r\n");

#ifndef __FLAT__			 /* no SHARE under Win32 and OS/2 */
    reg.x.ax=0x1000;
    int86(0x2f,&reg,&reg);
	if(!reg.h.al) {
        lputs("SHARE is not installed. Must run SHARE before SBBS.\r\n");
		lputs("SHARE.EXE is included with DOS v3.0 and higher.\r\n"); }
    else if(reg.h.al==1)
        lputs("SHARE is not installed and NOT OKAY to install.\r\n");
    else if(reg.h.al==0xff)
        lputs("SHARE is installed.\r\n");
    else
        lprintf("INT 2F returned %xh in AL.\r\n",reg.h.al);
#endif

    bail(1); }
sys_status|=SS_NODEDAB; /* says that node.dab is okay to use */
getnodedat(node_num,&thisnode,1);

/* if not returning, clear node.dab record */
if(!connect_rate) {
	thisnode.status=thisnode.action=thisnode.useron=0;
    thisnode.aux=0; /* use to always clear */
    thisnode.misc&=NODE_EVENT; }    /* turn off all misc bits but event */
criterrs=thisnode.errors;
putnodedat(node_num,thisnode);

if(com_base==0xb)
	rioctl(I14PC);
else if(com_base==0xd)
	rioctl(I14DB);
else if(com_base==0xe)
	rioctl(I14PS);
else if(com_base==0xf)
	rioctl(I14FO);
if(com_port) {
    comini();
	setrate(); }

for(i=0;i<total_qhubs;i++) {
	if(qhub[i]->node!=node_num)
		continue;
	for(j=0;j<10;j++) {
		sprintf(str,"%s%s.QW%c",data_dir,qhub[i]->id,j ? (j-1)+'0' : 'K');
		if(fexist(str)) {
			lclini(node_scrnlen-1);
			delfiles(temp_dir,"*.*");
			unpack_qwk(str,i); } } }

sprintf(str,"%sTIME.DAB",ctrl_dir);
if((file=nopen(str,O_RDWR|O_CREAT))==-1) {
    lprintf("Error opening/creating %s\r\n",str);
    bail(1); }
for(i=0;i<total_events;i++) {
	event[i]->last=0;
	if(filelength(file)<sizeof(time_t)*(i+1))
		write(file,&event[i]->last,sizeof(time_t));
	else
		read(file,&event[i]->last,sizeof(time_t)); }

close(file);

sprintf(str,"%sQNET.DAB",ctrl_dir);
if((file=nopen(str,O_RDWR|O_CREAT))==-1) {
    lprintf("Error opening/creating %s\r\n",str);
    bail(1); }

for(i=0;i<total_qhubs;i++) {
	qhub[i]->last=0;
	if(filelength(file)<sizeof(time_t)*(i+1))
		write(file,&qhub[i]->last,sizeof(time_t));
    else
		read(file,&qhub[i]->last,sizeof(time_t)); }

close(file);

sprintf(str,"%sPNET.DAB",ctrl_dir);
if((file=nopen(str,O_RDWR|O_CREAT))==-1) {
    lprintf("Error opening/creating %s\r\n",str);
    bail(1); }

for(i=0;i<total_phubs;i++) {
	phub[i]->last=0;
	if(filelength(file)<sizeof(time_t)*(i+1))
        write(file,&phub[i]->last,sizeof(time_t));
    else
        read(file,&phub[i]->last,sizeof(time_t)); }

close(file);

sprintf(str,"%sNODE.LOG",node_dir);
lprintf("\r\nOpening %s...",str);
if((logfile_fp=fnopen(&logfile,str,O_WRONLY|O_CREAT|O_APPEND|O_DENYALL))==NULL) {
	lprintf("\r\n\7Error opening %s\r\n\r\n"
		"Perhaps this node is already running.\r\n",str);
    bail(1); }
lprintf("\r%s opened.",str);
sys_status|=SS_LOGOPEN;
if(filelength(logfile)) {
	log(crlf);
	now=time(NULL);
    gm=localtime(&now);
	sprintf(str,"%02d:%02d%c  %s %s %02d %u  "
		"End of preexisting log entry (possible crash)"
		,gm->tm_hour>12 ? gm->tm_hour-12 : gm->tm_hour==0 ? 12 : gm->tm_hour
		,gm->tm_min,gm->tm_hour>=12 ? 'p' : 'a',wday[gm->tm_wday]
		,mon[gm->tm_mon],gm->tm_mday,gm->tm_year+1900);
	logline("L!",str);
	log(crlf);
	catsyslog(1); }
lputc(CLREOL);
sprintf(dszlog,"DSZLOG=%sPROTOCOL.LOG",node_dir);
putenv(dszlog); 		/* Makes the DSZ LOG active */
sprintf(sbbsnode,"SBBSNODE=%s",node_dir);
putenv(sbbsnode);		/* create enviornment var to contain node num */
sprintf(sbbsnnum,"SBBSNNUM=%d",node_num);
putenv(sbbsnnum);	   /* create enviornment var to contain node num */
backout();
if(events_only) {
	reset_logon_vars();
	while(!wfc_events(time(NULL)))
		;
	bail(0); }
}

/****************************************************************************/
/* Reads data from DSTS.DAB into stats structure                            */
/* If node is zero, reads from ctrl\dsts.dab, otherwise from each node		*/
/****************************************************************************/
void getstats(char node,stats_t *stats)
{
    char str[256];
    int file;

sprintf(str,"%sDSTS.DAB",node ? node_path[node-1] : ctrl_dir);
if((file=nopen(str,O_RDONLY))==-1) {
	errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
    return; }
lseek(file,4L,SEEK_SET);    /* Skip update time/date */
read(file,stats,sizeof(stats_t));
close(file);
}


/****************************************************************************/
/* Updates dstst.dab file upon user logoff. 								*/
/* First node, then system stats.                                           */
/****************************************************************************/
void logoffstats()
{
    char str[256];
    int i,file;
    stats_t stats;

for(i=0;i<2;i++) {
    sprintf(str,"%sDSTS.DAB",i ? ctrl_dir : node_dir);
    if((file=nopen(str,O_RDWR))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_RDWR);
        return; }
    lseek(file,12L,SEEK_SET);   /* Skip timestamp, logons and logons today */
    read(file,&stats.timeon,4);   /* Total time on system  */
	stats.timeon+=(now-logontime)/60;
    read(file,&stats.ttoday,4); /* Time today on system  */
	stats.ttoday+=(now-logontime)/60;
    read(file,&stats.uls,4);        /* Uploads today         */
    stats.uls+=logon_uls;
    read(file,&stats.ulb,4);        /* Upload bytes today    */
    stats.ulb+=logon_ulb;
    read(file,&stats.dls,4);        /* Downloads today       */
    stats.dls+=logon_dls;
    read(file,&stats.dlb,4);        /* Download bytes today  */
    stats.dlb+=logon_dlb;
	read(file,&stats.ptoday,4); 	/* Posts today			 */
	if(!(useron.rest&FLAG('Q')))
		stats.ptoday+=logon_posts;
    read(file,&stats.etoday,4); /* Emails today          */
    stats.etoday+=logon_emails;
    read(file,&stats.ftoday,4); /* Feedback sent today  */
    stats.ftoday+=logon_fbacks;
	read(file,&stats.nusers,2); /* New users today		*/
	if(sys_status&SS_NEWUSER)
		stats.nusers++;
    lseek(file,12L,SEEK_SET);
	write(file,&stats.timeon,4);	/* Total time on system  */
    write(file,&stats.ttoday,4);    /* Time today on system  */
    write(file,&stats.uls,4);       /* Uploads today         */
    write(file,&stats.ulb,4);       /* Upload bytes today    */
    write(file,&stats.dls,4);       /* Downloads today       */
    write(file,&stats.dlb,4);       /* Download bytes today  */
    write(file,&stats.ptoday,4);    /* Posts today           */
    write(file,&stats.etoday,4);    /* Emails today          */
	write(file,&stats.ftoday,4);	/* Feedback sent today	 */
	write(file,&stats.nusers,2);	/* New users today		 */
    close(file); }

}

/****************************************************************************/
/* Lists system statistics for everyday the bbs has been running.           */
/* Either for the current node (node=1) or the system (node=0)              */
/****************************************************************************/
void printstatslog(uint node)
{
    char str[256];
    uchar *buf;
    int file;
    time_t timestamp;
    long l;
    ulong   length,
            logons,
            timeon,
            posts,
            emails,
            fbacks,
            ulb,
            uls,
            dlb,
            dls;

if(node)
    bprintf(text[NodeStatsLogHdr],node);
else
    bputs(text[SysStatsLogHdr]);
sprintf(str,"%sCSTS.DAB",node ? node_path[node-1] : ctrl_dir);
if((file=nopen(str,O_RDONLY))==-1)
    return;
length=filelength(file);
if(length<40) {
    close(file);
    return; }
if((buf=(char *)MALLOC(length))==0) {
    close(file);
	errormsg(WHERE,ERR_ALLOC,str,length);
    return; }
read(file,buf,length);
close(file);
l=length-4;
while(l>-1L && !msgabort()) {
    fbacks=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
        |((long)buf[l+3]<<24);
    l-=4;
    emails=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
        |((long)buf[l+3]<<24);
    l-=4;
    posts=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
        |((long)buf[l+3]<<24);
    l-=4;
    dlb=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
        |((long)buf[l+3]<<24);
    l-=4;
    dls=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
        |((long)buf[l+3]<<24);
    l-=4;
    ulb=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
        |((long)buf[l+3]<<24);
    l-=4;
    uls=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
        |((long)buf[l+3]<<24);
    l-=4;
    timeon=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
        |((long)buf[l+3]<<24);
    l-=4;
    logons=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
        |((long)buf[l+3]<<24);
    l-=4;
    timestamp=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
        |((long)buf[l+3]<<24);
    l-=4;
    unixtodos(timestamp-(24*60*60),&date,&curtime); /* 1 day less than stamp */
    bprintf(text[SlogFmt]
        ,date.da_mon,date.da_day,TM_YEAR(date.da_year-1900),timeon,logons,posts,emails
        ,fbacks,ulb/1024,uls,dlb/1024,dls); }
FREE(buf);
}

#ifdef __OS2__
#define INPUT_LOOP 1000
#else
#define INPUT_LOOP 1000
#endif

/****************************************************************************/
/* Terminal mode                                                            */
/* Returns 1 if it is to logon locally, 0 if it is done.                    */
/****************************************************************************/
char terminal()
{
    char str[256],c,x,y,*p;
	int file;
	uint i,l;

lclini(node_scrnlen-1);
statline=-1;
statusline();
if(!term_ret) {
#ifdef __FLAT__
	setbaud(com_rate);
#else
	setbaud((uint)(com_rate&0xffffL));
#endif
	mdmcmd(mdm_term); }
else
    lputs("\r\nReturned.\r\n");

l=0;    /* l is the ansi response code counter */

while(1) {
    if(lkbrd(1)) {
        i=lkbrd(0);
        if((i&0xff)==0) {
            i>>=8;
			if(i==45) { 			/* Alt-X */
                rioctl(0x11);
				if(DCDHIGH) {
                    lputs("\r\nHang up (Yes/No/Abort) ? ");
					c=toupper(getch());
					if(c!='Y' && c!='N') {  /* Hang-up Y/N/Abort */
                        lputs("Abort\r\n");
                        continue; }
					if(c=='Y') {    /* Yes, hang-up */
						if(mdm_misc&MDM_NODTR)
							mdmcmd(mdm_hang);
						else
							if(dtr(15))
								lputs("Dropping DTR Failed"); } }
                break; }
			else if(i==35) {		/* Alt-H */
				if(mdm_misc&MDM_NODTR)
					mdmcmd(mdm_hang);
				else
					if(dtr(15))
						lputs("Dropping DTR Failed\r\n");
                dtr(1); }
			else if((i>=0x3b &&i<=0x44) || (i>=0x54 &&i<=0x71)
				|| (i>=0x85 &&i<=0x8c)) {
                sprintf(str,"%s%sF%d.MAC",text_dir
                    ,i<0x45 || (i>=0x85 && i<=0x86) ? nulstr
                    :i<0x5e || (i>=0x87 && i<=0x88) ? "SHFT-"
                    :i<0x68 || (i>=0x89 && i<=0x8a) ? "CTRL-" : "ALT-"
                    ,i<0x45 ? i-0x3a : i<0x5e ? i-0x53 : i<0x68 ? i-0x5d
                    :i<0x72 ? i-0x67 : i<0x87 ? i-0x7a : i<0x89 ? i-0x7c
                    :i<0x8b ? i-0x7e : i-0x80);
                if((file=nopen(str,O_RDONLY))==-1)
                    continue;
                i=filelength(file);
                while(i--) {
                    read(file,&c,1);
                    if(c!=LF)
                        putcomch(c); }
                close(file); }
			else if(i==0x48)	/* up arrow */
				putcom("\x1b[A");
			else if(i==0x50)	/* dn arrow */
				putcom("\x1b[B");
			else if(i==0x4b)	/* left */
				putcom("\x1b[D");
			else if(i==0x4d)	/* right */
				putcom("\x1b[C");
            else if(i==0x16) {  /* Alt-U User Edit */
				if((p=MALLOC((node_scrnlen*80)*2))==NULL) {
                    lputs("Allocation error.\r\n");
                    continue; }
                gettext(1,1,80,node_scrnlen,p);
                x=lclwx();
                y=lclwy();
                quicklogonstuff();
				useredit(0,1);
                puttext(1,1,80,node_scrnlen,p);
				FREE(p);
                lclxy(x,y); }
            else if(i==0x26) { /* Alt-L logon locally */
                lputc(FF);
                quicklogonstuff();
                if(!useron.number) {
                    lputc(7);
                    lputs("A Sysop account hasn't been created");
                    continue; }
                term_ret=1;
                return(1); }
            else if(i==0x20) {  /* Alt-D Shell to DOS */
				if((p=MALLOC((node_scrnlen*80)*2))==NULL) {
                    lputs("Allocation error.\r\n");
                    continue; }
                gettext(1,1,80,node_scrnlen,p);
                x=lclwx();
                y=lclwy();
                lclini(node_scrnlen);
                lputc(FF);
				external(comspec,0);
                puttext(1,1,80,node_scrnlen,p);
				FREE(p);
                lclxy(x,y); } }
		else {
			putcomch(i);
			continue; } }
	for(i=0;i<INPUT_LOOP;i++) {
        if((c=incom())!=0) {
			if(c==ESC)
                l=!l;
            else if(l) {
                if(c=='[') {
                    if(l!=1) l=0; else l++; }
                else if(c=='6') {
                    if(l!=2) l=0; else l++; }
                else if(c=='n') {
                    if(l==3) {
						sprintf(str,"\x1b[%u;%uR"
                            ,lclwy(),lclwx());
                        putcom(str); }
                    l=0; }
                else l=0; }
			outcon(c); }
		else
			break; }
	if(!c)
		mswait(0); }
term_ret=0;
return(0);
}


/****************************************************************************/
/* Exits bbs to DOS with appropriate error code after setting COM port isr  */
/* back to normal.                                                          */
/****************************************************************************/
void bail(int code)
{

if(sys_status&SS_COMISR)
    rioini(0,0);

lclatr(LIGHTGRAY);
if(code) {
	getnodedat(node_num,&thisnode,1);
	criterrs=++thisnode.errors;
	putnodedat(node_num,thisnode);
    now=time(NULL);
    lprintf("\r\nExiting with errorlevel (%d) on %s\r\n",code,timestr(&now));
    if(sys_misc&SM_ERRALARM) {
		beep(500,220); beep(250,220);
		beep(500,220); beep(250,220);
		beep(500,220); beep(250,220);
        nosound(); } }

if(sys_status&SS_INITIAL) {
    getnodedat(node_num,&thisnode,1);
    thisnode.status=NODE_OFFLINE;
    putnodedat(node_num,thisnode);
    close(nodefile);
    close(node_ext); }

exit(code);
}


