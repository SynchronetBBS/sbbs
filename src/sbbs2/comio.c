#line 1 "COMIO.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/***********************************/
/* COM port and modem i/o routines */
/***********************************/

#include "sbbs.h"

#ifndef __FLAT__
extern mswtyp;
#endif

extern char term_ret;
extern uint addrio;

/****************************************************************************/
/* Outputs a NULL terminated string to the COM port verbatim				*/
/* Called from functions putmsg, getstr and color							*/
/****************************************************************************/
void putcom(char *str)
{
	ulong	l=0;

while(str[l])
	putcomch(str[l++]);
}

/****************************************************************************/
/* Outputs a single character to the COM port								*/
/****************************************************************************/
void putcomch(char ch)
{
	char 	lch;
	int 	i=0;

if(!com_port)
	return;
while(outcom(ch)&TXBOF && i<1440) {	/* 3 minute pause delay */
    if(lkbrd(1)) {
		lch=lkbrd(0);	/* ctrl-c */
		if(lch==3) {
			lputs("local abort (putcomch)\r\n");
			i=1440;
			break; }
		ungetkey(lch); }
	if(!DCDHIGH)
		break;
	i++;
	if(sys_status&SS_SYSPAGE)
		beep(i,80);
	else
		mswait(80); }
if(i==1440) {						/* timeout - beep flush outbuf */
	i=rioctl(TXBC);
	lprintf("timeout(putcomch) %04X %04X\r\n",i,rioctl(IOFO));
	outcom(7);
	lputc(7);
	rioctl(IOCS|PAUSE); }
}

/****************************************************************************/
/* Sends string of characters to COM port. Interprets ^M and ~ (pause)		*/
/* Called from functions waitforcall and offhook							*/
/****************************************************************************/
void mdmcmd(char *str)
{
	int 	i=0;
	uint	lch;

if(mdm_misc&MDM_DUMB)
	return;
if(sys_status&SS_MDMDEBUG)
	lputs("\r\nModem command  : ");
while(str[i]) {
	if(str[i]=='~')
		mswait(DELAY_MDMTLD);
	else {
		if(i && str[i-1]=='^' && str[i]!='^')  /* control character */
			putcomch(toupper(str[i])-64);
		else if(str[i]!='^' || (i && str[i-1]=='^'))
			putcomch(str[i]);
		if(sys_status&SS_MDMDEBUG)
			lputc(str[i]); }
	i++; }
putcomch(CR);
i=0;
while(rioctl(TXSYNC|(1<<8)) && i<10) { /* wait for modem to receive all chars */
	if(lkbrd(1)) {
		lch=lkbrd(0);	/* ctrl-c */
		if(lch==0x2e03) {
			lputs("local abort (mdmcmd)\r\n");
			break; }
		if(lch==0xff00)
			bail(1);
		ungetkey(lch); }
	i++; }
if(i==10) {
	i=rioctl(TXBC);
	lprintf("\r\ntimeout(mdmcmd) %04X %04X\r\n",i,rioctl(IOFO)); }
if(sys_status&SS_MDMDEBUG)
	lputs(crlf);
}

/****************************************************************************/
/* Returns CR terminated string from the COM port. 							*/
/* Called from function waitforcall 										*/
/****************************************************************************/
char getmdmstr(char *str, int sec)
{
	uchar	j=0,ch;
	uint	lch;
	time_t	start;

if(sys_status&SS_MDMDEBUG)
	lputs("Modem response : ");
start=time(NULL);
while(time(NULL)-start<sec && j<81) {
	if(lkbrd(1)) {
		lch=lkbrd(0);	/* ctrl-c */
		if(lch==0x2e03 || lch==3) {
			lputs("local abort (getmdmstr)\r\n");
			break; }
		if(lch==0xff00)
			bail(1);
		ungetkey(lch); }
	if((ch=incom())==CR && j) {
		if(mdm_misc&MDM_VERBAL)
			incom();	/* LF */
		break; }
	if(ch && (ch<0x20 || ch>0x7f)) { /* ignore ctrl characters and ex-ascii */
		if(sys_status&SS_MDMDEBUG)
			lprintf("[%02X]",ch);
		continue; }
	if(ch) {
		str[j++]=ch;
		if(sys_status&SS_MDMDEBUG)
			lputc(ch); }
	else mswait(0); }
mswait(500);
str[j]=0;
if(sys_status&SS_MDMDEBUG)
	lputs(crlf);
return(j);
}

/****************************************************************************/
/* Drops DTR (if COM port used), clears KEY and COM Buffers and other vars	*/
/* Called from functions checkline, getkey, inkey, main, waitforcall,		*/
/* main_sec, xfer_sec, gettimeleft and newuser								*/
/****************************************************************************/
void hangup()
{
	char	str1[256],str2[256],c;
	uint	i;

rioctl(MSR);
if(!term_ret && com_port && (rioctl(IOSTATE)&DCD)) {  /* if carrier detect */
	riosync(0);
	mswait(DELAY_HANGUP);		 /* wait for modem buffer to clear */
	if(mdm_misc&MDM_NODTR)
		mdmcmd(mdm_hang);
	else {
		lputs("\r\nDropping DTR...");
		if(dtr(15))  {			/* drop dtr, wait 15 seconds for dcd to drop */
			lputs("\rDropping DTR Failed");
			logline("@!","Dropping DTR Failed");
			mdmcmd(mdm_hang); } }
	mswait(110);
	rioctl(MSR);
	if(rioctl(IOSTATE)&DCD) {
		mswait(5000);
		rioctl(MSR);
		if(rioctl(IOSTATE)&DCD) {
			lputs("\r\nDCD high after hang up");
			logline("@!","DCD high after hang up");
			mswait(5000); } } }
if(sys_status&SS_CAP) {	/* Close capture if open */
	fclose(capfile);
#ifdef __MSDOS__
	freedosmem=farcoreleft(); /* fnopen allocates memory and fclose frees */
#endif
	sys_status^=SS_CAP; }
if(sys_status&SS_FINPUT) { /* Close file input if open */
	close(inputfile);
	sys_status^=SS_FINPUT; }

keybufbot=keybuftop=online=console=0;
sys_status&=~(SS_TMPSYSOP|SS_LCHAT|SS_SYSPAGE|SS_ABORT);
nosound();
if(com_port)
	dtr(1);
}

/****************************************************************************/
/* Sends 'off-hook' string to modem to pick up phone (if COM port used)		*/
/* Called from function waitforcall											*/
/****************************************************************************/
void offhook()
{
if(com_port && !(mdm_misc&MDM_DUMB)) {
	mdmcmd(mdm_offh);
	rioctl(TXSYNC|(2<<8));	/* wait up 2 seconds for modem to receive */
	}
}

/****************************************************************************/
/* Checks to see if remote user has hung up.								*/
/* Called from function getkey												*/
/****************************************************************************/
void checkline()
{
if(online!=ON_REMOTE) return;
if(!DCDHIGH) {
	lprintf("\r\nHung up");
	logline(nulstr,"Hung up");
	hangup(); }
}

/****************************************************************************/
/* Syncronizes the remote and local machines before command prompts         */
/****************************************************************************/
void riosync(char abortable)
{
	int 	i=0;

if(useron.misc&(RIP|WIP))	/* don't allow abort with RIP or WIP */
	abortable=0;		/* mainly because of ANSI cursor posistion response */
if(sys_status&SS_ABORT)	/* no need to sync if already aborting */
	return;
if(online==ON_REMOTE && console&CON_R_ECHO) {
	while(rioctl(TXSYNC|(1<<8)) && i<180) { /* three minutes till tx buf empty */
		if(sys_status&SS_SYSPAGE)
			beep(i,10);
		if(lkbrd(1))
			break;
		if(abortable && rioctl(RXBC)) { 	/* incoming characer */
			rioctl(IOFO);					/* flush output */
			sys_status|=SS_ABORT;			/* set abort flag so no pause */
			break; }						/* abort sync */
		if(!DCDHIGH)
			break;
		mswait(0);
		i++; }
	if(i==180) {
		i=rioctl(TXBC);
		lprintf("timeout(sync) %04X %04X\r\n",i,rioctl(IOFO));
		outcom(7);
		lputc(7);
		rioctl(IOCS|PAUSE); }
	if(rioctl(IOSTATE)&ABORT)
		sys_status|=SS_ABORT;
	rioctl(IOCS|ABORT); }
}

/*****************************************************************************/
/* Initializes com i/o routines and sets baud rate                           */
/*****************************************************************************/
void comini()
{
	int 	i;
#ifndef __FLAT__
	uint	base=0xffff;
#endif

if(sys_status&SS_COMISR)	/* Already installed */
	return;
lprintf("\r\nInitializing COM port %u: ",com_port);
#ifndef __FLAT__
switch(com_base) {
	case 0xb:
		lputs("PC BIOS");
		break;
	case 0xd:
		lputs("DigiBoard");
		break;
	case 0xe:
		lputs("PS/2 BIOS");
		break;
	case 0xf:
		lputs("FOSSIL");
		break;
	case 0:
		base=com_port;
		lputs("UART I/O (BIOS), ");
		if(com_irq)
            lprintf("IRQ %d",com_irq);
        else lputs("default IRQ");
		break;
	default:
		base=com_base;
		lprintf("UART I/O %Xh, ",com_base);
		if(com_irq)
            lprintf("IRQ %d",com_irq);
        else lputs("default IRQ");
        break; }

if(base==0xffff)
	lprintf(" channel %u",com_irq);
i=rioini(base,com_irq);
#else
#ifdef __OS2__
if(rio_handle!=-1)	// Already opened, prolly passed to SBBS4OS2 via cmd line
	lprintf("handle %d",rio_handle);
#endif
i=rioini(com_port,0);
#endif
if(i) {
	lprintf(" - Failed! (%d)\r\n",i);
	bail(1); }
i=0;
if(mdm_misc&MDM_CTS)
	i=CTSCK;
if(mdm_misc&MDM_RTS)
	i|=RTSCK;
if(i)
	rioctl(IOSM|i); /* set cts checking if hardware flow control */

#ifndef __FLAT__
rioctl(TSTYPE|mswtyp);	 /* set time-slice API type */
#endif

rioctl(CPTON);			/* ctrl-p translation */

if(addrio) {
	lprintf("\r\nAdditional rioctl: %04Xh",addrio);
	rioctl(addrio); }

sys_status|=SS_COMISR;
}

/****************************************************************************/
/* Sets the current baud rate to either the current connect rate or the 	*/
/* DTE rate 																*/
/****************************************************************************/
void setrate()
{
	int i;

if(online==ON_REMOTE && !(mdm_misc&MDM_STAYHIGH))
	dte_rate=cur_rate;
else
	dte_rate=com_rate;
lprintf("\r\nSetting DTE rate: %lu baud",dte_rate);
#ifdef __FLAT__
if((i=setbaud(dte_rate))!=0) {
#else
if((i=setbaud((uint)(dte_rate&0xffffL)))!=0) {
#endif
	lprintf(" - Failed! (%d)\r\n",i);
	bail(1); }
lputs(crlf);
}
