/* RIOLIB.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Remote communications Input/Output Library for OS/2 and Win32 */

#define RIOLIB_VER	100

#ifdef __OS2__
	#define INCL_DOSDEVICES
	#define INCL_DOSDEVIOCTL
	#define INCL_DOS
	#include <os2.h>

#else	// Win32

	#include <windows.h>
#endif

#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <process.h>
#include "riolib.h"
#include "riodefs.h"

int rio_handle=-1;

int rio_abort=0,rio_aborted=0;
int inbufbot,inbuftop;
int outbufbot,outbuftop;
char inbuf[RIO_INCOM_BUFLEN];
char outbuf[RIO_OUTCOM_BUFLEN];

/****************************************************************************/
/* OS/2 Specific															*/
/****************************************************************************/

#ifdef __OS2__

HEV rio_sem;

typedef struct {	/* Packet for Get/Set Device Control Block Parms */

	ushort	wr_timeout;
	ushort	rd_timeout;
	uchar	flags1;
	uchar	flags2;
	uchar	flags3;
	uchar	err_char;
	uchar	brk_char;
	uchar	xon_char;
	uchar	xoff_char;

	} async_dcb_t;

typedef struct {	/* Parameter Packet for Extended Set Bit Rate */

	ulong rate;
	uchar frac;

	} setrate_t;

typedef struct {	/* Data Packet for Extended Query Bit Rate */

	ulong cur_rate;
	uchar cur_rate_frac;
	ulong min_rate;
	uchar min_rate_frac;
	ulong max_rate;
	uchar max_rate_frac;

	} getrate_t;

typedef struct {	/* Data Packet for Query Number of Chars Queue */

	ushort	cur,
			max;

	} getqueue_t;

typedef struct {	/* Parameter Packet for Set Modem Control Signals */

	uchar	on;
	uchar	off;

	} setmdmctrl_t;

typedef struct {	/* Parameter Packet for Set Line Characteristics */

	uchar	data;
	uchar	parity;
	uchar	stop;

	} setline_t;

#else

/****************************************************************************/
/* Win32 Specific															*/
/****************************************************************************/

HANDLE	rio_event;

#endif

/************************************************************************/
/* Opens COM port, ignoring IRQ argument								*/
/* Returns 0 on no error												*/
/************************************************************************/
int rioini(int com, int unused)
{
	char		str[64];
#ifdef __OS2__
	async_dcb_t async_dcb;
#else // Win32
	DCB 		dcb;
	COMMTIMEOUTS timeouts;
#endif

#ifdef __WATOMC__
	char		*stack;
#endif

inbufbot=inbuftop=outbufbot=outbuftop=0;	/* clear our i/o buffers */

if(!com) { /* Close port */
	if(rio_handle>=0) {
#ifdef __OS2__
		close(rio_handle);
		rio_handle=-1;	 /* signal outcom_thread() to end */
		DosPostEventSem(rio_sem);
#else // Win32
		CloseHandle((HANDLE)rio_handle);
		rio_handle=-1;
		SetEvent(rio_event);
#endif
		return(0); }
	return(-2); }

sprintf(str,"COM%u",com);
#ifdef __OS2__
if(rio_handle==-1) {	/* Not already opened */
	if((rio_handle=open(str,O_RDWR|O_BINARY,0))==-1)
		return(-1); }
#else // Win32
if(rio_handle==-1) {	/* Not already opened */
	if((rio_handle=(int)CreateFile(str
		,GENERIC_READ|GENERIC_WRITE 	/* Access */
		,0								/* Share mode */
		,NULL							/* Security attributes */
		,OPEN_EXISTING					/* Create access */
		,FILE_ATTRIBUTE_NORMAL			/* File attributes */
		,NULL							/* Template */
		))==(int)INVALID_HANDLE_VALUE)
		return(-1); }
#endif

#ifdef __OS2__

if(DosDevIOCtl(rio_handle
	,IOCTL_ASYNC, 0x73	/* Query Device Control Block (DCB) Parms */
	,NULL
	,0
	,NULL
	,&async_dcb
	,sizeof(async_dcb)
	,NULL))
	return(-6);

async_dcb.flags2&=~(1<<1);	/* Disable Automatic Recv XON/XOFF flow ctrl */
/* Extended Hardware Buffering (16550 FIFO) */
if(async_dcb.flags3&((1<<4)|(1<<3))) {			/* Supported */
	async_dcb.flags3&=~((1<<3)|(1<<6)|(1<<5));	/* Set to 1 char trgr lvl */
	async_dcb.flags3|=((1<<4)|(1<<7)); }		/* Set to 16 char tx buf */
async_dcb.flags3|=((1<<2)|(1<<1));				/* No-Wait read timeout */
async_dcb.flags3&=~(1<<0);						/* Normal write timeout */
async_dcb.wr_timeout=499;						/* 5 seconds? */
DosDevIOCtl(rio_handle
	,IOCTL_ASYNC, 0x53	/* Set Device Control Block (DCB) Parms */
	,(void *)&async_dcb
	,sizeof(async_dcb)
	,NULL
	,NULL
	,0
	,NULL);

if(DosCreateEventSem(NULL,&rio_sem,0,0))
	return(-5);

#else // Win32

if(GetCommState((HANDLE)rio_handle,&dcb)!=TRUE)
	return(-6);

dcb.fBinary=1;			// No EOF check
dcb.fDtrControl=DTR_CONTROL_ENABLE;
dcb.fDsrSensitivity=FALSE;
dcb.fOutX=0;			// No Xon/Xoff out
dcb.fInX=0; 			// No Xon/Xoff in
dcb.fErrorChar=FALSE;	// No character replacement
dcb.fNull=0;			// No null stripping
dcb.fAbortOnError=0;	// Continue to communicate even if error detected

SetCommState((HANDLE)rio_handle,&dcb);

if(GetCommTimeouts((HANDLE)rio_handle,&timeouts)!=TRUE)
	return(-7);

timeouts.ReadIntervalTimeout=MAXDWORD;
timeouts.ReadTotalTimeoutMultiplier=0;
timeouts.ReadTotalTimeoutConstant=0;		// No-wait read timeout
timeouts.WriteTotalTimeoutMultiplier=0;
timeouts.WriteTotalTimeoutConstant=5000;	// 5 seconds
SetCommTimeouts((HANDLE)rio_handle,&timeouts);

SetupComm((HANDLE)rio_handle,4096,4096);	/* Init Rx and Tx buffer sizes */

if((rio_event=CreateEvent(NULL			/* Security attributes */
	,TRUE								/* Manual reset */
	,FALSE								/* Non-signaled by default */
	,NULL								/* Event name */
	))==NULL)
	return(-5);

#endif

#ifdef __WATCOMC__
stack=malloc(RIO_OUTCOM_STACK);
if(stack==NULL)
	return(-4);
if(_beginthread(outcom_thread,stack,RIO_OUTCOM_STKLEN,NULL)==-1)
	return(-3);
#else
if(_beginthread(outcom_thread,RIO_OUTCOM_STKLEN,NULL)==(ulong)-1)
	return(-3);
#endif


return(0);
}

/************************************************************************/
/* Raise or lower DTR													*/
/* If onoff is 0, drop and exit, 1 raise and exit, >1 drop and wait 	*/
/* up to x seconds for DCD to drop, return 0 on success, 1 on failure	*/
/************************************************************************/
int dtr(char onoff)
{
#ifdef __OS2__
	setmdmctrl_t setmdmctrl;
	ulong start;
	ushort w;
	uchar c;

if(onoff==1) {
	setmdmctrl.on=1;
	setmdmctrl.off=0xff; }
else {
	setmdmctrl.on=0;
	setmdmctrl.off=0xfe; }

DosDevIOCtl(rio_handle, IOCTL_ASYNC, 0x46	/* Set Modem Ctrl Signals */
	,&setmdmctrl
	,sizeof(setmdmctrl)
	,NULL
	,&w
	,sizeof(short)
	,NULL);

if(onoff<=1)
	return(0);

start=time(NULL);
while(time(NULL)-start<=onoff) {
	DosDevIOCtl(rio_handle
		,IOCTL_ASYNC, 0x67					/* Get Modem Input Signals */
		,NULL
		,0
		,NULL
		,&c
		,1
		,NULL);
	if(!(c&DCD))	/* DCD is low */
		return(0);
	DosSleep(1); }

#else // Win32

	ulong l,start;

if(onoff==1)
	EscapeCommFunction((HANDLE)rio_handle,SETDTR);
else
	EscapeCommFunction((HANDLE)rio_handle,CLRDTR);

if(onoff<=1)
	return(0);

start=time(NULL);
while(time(NULL)-start<=onoff) {
	GetCommModemStatus((HANDLE)rio_handle,&l);
	if(!(l&MS_RLSD_ON)) // DCD is low
		return(0);
	Sleep(1000); }

#endif

return(1); /* Dropping DTR failed to lower DCD */
}



/************************************************************************/
/* Returns the current DTE rate of the currently open COM port			*/
/************************************************************************/
long rio_getbaud(void)
{
#ifdef __OS2__
	getrate_t getrate;

if(DosDevIOCtl(rio_handle, IOCTL_ASYNC, 0x63 /* Extended Query Bit Rate */
	,NULL
	,0
	,NULL
	,&getrate
	,sizeof(getrate)
	,NULL)==0)
	return(getrate.cur_rate);

#else // Win32

	DCB dcb;

if(GetCommState((HANDLE)rio_handle,&dcb)==TRUE) {
	switch(dcb.BaudRate) {
		case CBR_110:
			return(110);
		case CBR_300:
			return(300);
		case CBR_600:
			return(600);
		case CBR_1200:
			return(1200);
		case CBR_2400:
			return(2400);
		case CBR_4800:
			return(4800);
		case CBR_9600:
			return(9600);
		case CBR_14400:
			return(14400);
		case CBR_19200:
			return(19200);
		case CBR_38400:
			return(38400);
		case CBR_56000:
			return(56000);
		case CBR_57600:
			return(57600);
		case CBR_115200:
			return(115200);
		case CBR_128000:
			return(128000);
		case CBR_256000:
			return(256000);
		default:
			return(dcb.BaudRate); } }

#endif

return(-1); // Error
}


/************************************************************************/
/* Sets the current DTE rate											*/
/* Returns 0 on success 												*/
/************************************************************************/
int setbaud(int rate)
{
#ifdef __OS2__
	setrate_t setrate;
	APIRET	ret;

setrate.rate=rate;
setrate.frac=0;

ret=DosDevIOCtl(rio_handle, IOCTL_ASYNC, 0x43	/* Extended Set Bit Rate */
	,&setrate
	,sizeof(setrate)
	,NULL
	,NULL
	,0
	,NULL);

if(ret)
	return(ret);

if(rio_getbaud()!=rate) 	/* Make sure it actually changed rates */
	return(-1);

#else // Win32

	DCB dcb;

if(GetCommState((HANDLE)rio_handle,&dcb)!=TRUE)
	return(-1);
dcb.BaudRate=rate;
if(SetCommState((HANDLE)rio_handle,&dcb)!=TRUE)
	return(-1);

#endif

return(0);
}

/************************************************************************/
/* Return next incoming character from COM port, NOINP if none avail.	*/
/* Uses a linear buffer.												*/
/************************************************************************/
int incom(void)
{
	char c;

if(inbufbot!=inbuftop) {
	c=inbuf[inbufbot];
	inbufbot++;
	if(inbufbot==inbuftop)
		inbufbot=inbuftop=0;
	if(rio_abort && c==3) { 		/* Ctrl-C */
		rio_aborted=1;
		rioctl(IOFO);
		return(NOINP); }
	return(c); }

inbufbot=0;
#ifdef __OS2__
inbuftop=read(rio_handle,inbuf,RIO_INCOM_BUFLEN);
#else // Win32
ReadFile((HANDLE)rio_handle,inbuf,RIO_INCOM_BUFLEN,(DWORD *)&inbuftop,NULL);
#endif
if(inbuftop<=0 || inbuftop>RIO_INCOM_BUFLEN) {
	inbuftop=0;
	return(NOINP); }
else
	return(inbuf[inbufbot++]);
}

/************************************************************************/
/* Return number of chars in our input buffer							*/
/************************************************************************/
int inbufcnt(void)
{
return(inbuftop-inbufbot);
}

/************************************************************************/
/* Place a character into outbound buffer, return TXBOF on buffer		*/
/* overflow, 0 on success.												*/
/************************************************************************/
int outcom(int ch)
{
	int i=outbuftop+1;

if(i==RIO_OUTCOM_BUFLEN)
	i=0;
if(i==outbufbot)
	return(TXBOF);
outbuf[outbuftop++]=ch;
if(outbuftop==RIO_OUTCOM_BUFLEN)
	outbuftop=0;
#ifdef __OS2__
DosPostEventSem(rio_sem);	 // Enable output
#else // Win32
SetEvent(rio_event);
#endif
return(0);
}

/************************************************************************/
/************************************************************************/
void outcom_thread(void *unused)
{
	int i,top;
	ulong l;

while(rio_handle>=0) {
	if(outbufbot==outbuftop) {
#ifdef __OS2__
		DosResetEventSem(rio_sem,&l);
		DosWaitEventSem(rio_sem,10000);  /* every 10 seconds */
#else // Win32
		ResetEvent(rio_event);
		WaitForSingleObject(rio_event,10000);
#endif
		continue; }
	top=outbuftop;
	if(top<outbufbot)
		i=RIO_OUTCOM_BUFLEN-outbufbot;
	else
		i=top-outbufbot;
#ifdef __OS2__
	i=write(rio_handle,outbuf+outbufbot,i);
#else // Win32
	WriteFile((HANDLE)rio_handle,outbuf+outbufbot,i,(DWORD *)&i,NULL);
#endif
	if(i>0 && i<=RIO_OUTCOM_BUFLEN-outbufbot)
		outbufbot+=i;
	if(outbufbot==RIO_OUTCOM_BUFLEN)
		outbufbot=0; }
_endthread();
}

/************************************************************************/
/* Return number of chars in our output buffer							*/
/************************************************************************/
int outbufcnt(void)
{
if(outbuftop>=outbufbot)
    return(outbuftop-outbufbot);
return(outbuftop+(RIO_OUTCOM_BUFLEN-outbufbot));
}

/************************************************************************/
/************************************************************************/
int rio_getstate(void)
{
#ifdef __OS2__
	uchar c;
	ushort state=0,w;
	getqueue_t getqueue;

if(rio_abort && !rio_aborted) {
	DosDevIOCtl(rio_handle
		,IOCTL_ASYNC, 0x68 /* Query # of chars in rx queue */
		,NULL
		,0
		,NULL
		,&getqueue
		,sizeof(getqueue)
		,NULL);
	if(getqueue.cur && read(rio_handle,&c,1)==1) {
		if(c==3) { /* ctrl-c */
			rio_aborted=1;
			rioctl(IOFO); }
		else
			if(inbuftop<RIO_INCOM_BUFLEN)  /* don't overflow input buffer */
				inbuf[inbuftop++]=c; } }

if(rio_aborted)
    state|=ABORT;

#if 0	// just to see if things speed up
DosDevIOCtl(rio_handle
	,IOCTL_ASYNC, 0x6D	/* Query COM Error */
	,NULL
	,0
	,NULL
	,&w
	,sizeof(short)
	,NULL);
state|=w&0xf;	/* FERR|PERR|OVRR|RXLOST */
#endif

DosDevIOCtl(rio_handle
	,IOCTL_ASYNC, 0x67	/* Query Modem Input Signals */
	,NULL
	,0
	,NULL
	,&c
	,sizeof(char)
	,NULL);
state|=c&0xf0;	/* DCD|RI|DSR|CTS */

#else // Win32

	uchar c;
	ushort state=0;
	int i;
	ulong l;
	COMSTAT comstat;

if(rio_abort && !rio_aborted) {
	ClearCommError((HANDLE)rio_handle,&l,&comstat);
	if(comstat.cbInQue) {
		ReadFile((HANDLE)rio_handle,&c,1,(DWORD *)&i,NULL);
		if(i==1) {
			if(c==3) { /* Ctrl-C */
				rio_aborted=1;
				rioctl(IOFO); }
			else
				if(inbuftop<RIO_INCOM_BUFLEN)  /* don't overflow input buffer */
					inbuf[inbuftop++]=c; } } }

GetCommModemStatus((HANDLE)rio_handle,(DWORD *)&state);  /* DCD|RI|DSR|CTS */
if(rio_aborted)
    state|=ABORT;

#endif

return(state);
}


/************************************************************************/
/************************************************************************/
int rioctl(ushort action)
{
#ifdef __OS2__
	async_dcb_t 	async_dcb;
	getqueue_t		getqueue;
	setline_t		setline;
#else // Win32
	COMMPROP		commprop;
	COMSTAT 		comstat;
	DCB 			dcb;
	ulong			l;
#endif
	uchar c;
	ushort mode,w;
	clock_t start;

switch(action) {
	case GVERS: 	/* Get version */
		return(RIOLIB_VER);
	case GUART: 	/* Get UART I/O address, not available */
		return(0xffff);
	case GIRQN: 	/* Get IRQ number, not available */
		return((int)rio_handle);
	case GBAUD: 	/* Get current bit rate */
		return(rio_getbaud());
	case RXBC:		/* Get receive buffer count */
#ifndef __OS2__ // Win32
		ClearCommError((HANDLE)rio_handle,&l,&comstat);
		return(comstat.cbInQue+inbufcnt());
#endif
		/* Fall-through if OS/2 */
	case RXBS:		/* Get receive buffer size */
#ifdef __OS2__
		DosDevIOCtl(rio_handle
			,IOCTL_ASYNC, 0x68 /* Query # of chars in rx queue */
			,NULL
			,0
			,NULL
			,&getqueue
			,sizeof(getqueue)
			,NULL);
		if(action==RXBC)
			return(getqueue.cur+inbufcnt());
		/* RXBS */
		return(getqueue.max+RIO_INCOM_BUFLEN);
#else
		GetCommProperties((HANDLE)rio_handle,&commprop);
		return(commprop.dwCurrentRxQueue+RIO_INCOM_BUFLEN);
#endif
	case TXBC:		/* Get transmit buffer count */
#ifndef __OS2__ // Win32
		ClearCommError((HANDLE)rio_handle,&l,&comstat);
		return(comstat.cbOutQue+outbufcnt());
#endif
		/* Fall-through if OS/2 */
	case TXBS:		/* Get transmit buffer size */
	case TXBF:		/* Get transmit buffer free space */
#ifdef __OS2__
		DosDevIOCtl(rio_handle
			,IOCTL_ASYNC, 0x69	/* Query # of chars in tx queue */
			,NULL
			,0
			,NULL
			,&getqueue
			,sizeof(getqueue)
			,NULL);
		if(action==TXBC)
			return(getqueue.cur+outbufcnt());
		else if(action==TXBS)
			return(getqueue.max+RIO_OUTCOM_BUFLEN);
		/* TXBF */
		return((getqueue.max-getqueue.cur)+(RIO_OUTCOM_BUFLEN-outbufcnt()));
#else
		GetCommProperties((HANDLE)rio_handle,&commprop);
		if(action==TXBS)
			return(commprop.dwCurrentTxQueue+RIO_OUTCOM_BUFLEN);
		/* TXBF */
		ClearCommError((HANDLE)rio_handle,&l,&comstat);
		return((commprop.dwCurrentTxQueue-comstat.cbOutQue)
			+(RIO_OUTCOM_BUFLEN-outbufcnt()));
#endif
	case IOSTATE:
		return(rio_getstate());
	case IOFI:		/* Flush input buffer */
	case IOFO:		/* Flush output buffer */
	case IOFB:		/* Flush both buffers */
#ifdef __OS2__
		c=0;
		if((action&IOFI)==IOFI) {
			DosDevIOCtl(rio_handle
				,IOCTL_GENERAL, DEV_FLUSHINPUT
				,&c
				,sizeof(char)
				,NULL
				,&w
				,sizeof(short)
				,NULL);
			inbufbot=inbuftop=0;
			}	   /* Clear our input buffer too */
		c=0;
		if((action&IOFO)==IOFO) {
			DosDevIOCtl(rio_handle
				,IOCTL_GENERAL, DEV_FLUSHOUTPUT
				,&c
				,sizeof(char)
				,NULL
				,&w
				,sizeof(short)
                ,NULL);
			outbufbot=outbuftop=0;
			}	 /* Clear our output buffer too */
#else // Win32
		l=0;
		if((action&IOFI)==IOFI)
			l|=(PURGE_RXABORT|PURGE_RXCLEAR);
		if((action&IOFO)==IOFO)
			l|=(PURGE_TXABORT|PURGE_TXCLEAR);
		PurgeComm((HANDLE)rio_handle,l);
#endif
		return(rio_getstate());
	case LFN81:
	case LFE71:
#ifdef __OS2__
		setline.stop=0; /* 1 stop bit */
		if(action==LFN81) {
			setline.data=8;
			setline.parity=0; } /* No parity */
		else {
			setline.data=7;
			setline.parity=2; } /* Even parity */
		DosDevIOCtl(rio_handle
			,IOCTL_ASYNC, 0x42	/* Set Line Characteristics */
			,&setline
			,sizeof(setline)
			,NULL
			,NULL
			,0
			,NULL);
#else // Win32
		GetCommState((HANDLE)rio_handle,&dcb);
		if(action==LFN81) {
			dcb.Parity=NOPARITY;
			dcb.ByteSize=8;
			dcb.StopBits=ONESTOPBIT; }
		else {
			dcb.Parity=EVENPARITY;
			dcb.ByteSize=7;
			dcb.StopBits=ONESTOPBIT; }
		SetCommState((HANDLE)rio_handle,&dcb);
#endif
		return(0);
	case FIFOCTL:
#ifdef __OS2__
		DosDevIOCtl(rio_handle
			,IOCTL_ASYNC, 0x73	/* Query Device Control Block (DCB) Parms */
			,NULL
			,0
			,NULL
			,&async_dcb
			,sizeof(async_dcb)
			,NULL);

		/* Extended Hardware Buffering (16550 FIFO) */
		if(async_dcb.flags3&(1<<4)) {					/* Supported */
			c=0xc0;
			if(async_dcb.flags3&(1<<7))
				c|=0x0c;
			return(c); }
#else // Win32
		// Not supported under Win32?
#endif
		return(0);
	}

if((action&0xff)<=IOCM) {	/* Get/Set/Clear mode */
	mode=0;

#ifdef __OS2__
	DosDevIOCtl(rio_handle
		,IOCTL_ASYNC, 0x73	/* Query Device Control Block (DCB) Parms */
		,NULL
		,0
		,NULL
		,&async_dcb
		,sizeof(async_dcb)
		,NULL);
	if(async_dcb.flags1&(1<<3)) /* Output CTS handshaking */
		mode|=CTSCK;
	if(async_dcb.flags2&(1<<0)) /* Automatic Xmit Control Flow Xon/Xoff */
		mode|=PAUSE;
	if(async_dcb.flags2&(1<<7)	/* RTS Input handshaking */
		&& !(async_dcb.flags2&(1<<6)))
		mode|=RTSCK;
	if(rio_abort)
		mode|=ABORT;

	if(action==IOMODE)
		return(mode);

	if((action&0xff)==IOCM) 	/* Clear mode */
		mode&=~(action&0xff00);
	else						/* Set mode */
		mode|=(action&0xff00);

	if(mode&CTSCK)
		async_dcb.flags1|=(1<<3);
	else
		async_dcb.flags1&=~(1<<3);
	if(mode&PAUSE) {
		async_dcb.flags2|=(1<<0);	/* xmit flow control */
		async_dcb.xon_char=17;		/* Ctrl-Q */
		async_dcb.xoff_char=19; }	/* Ctrl-S */
	else
		async_dcb.flags2&=~(1<<0);
	if(mode&RTSCK) {
		async_dcb.flags2|=(1<<7);
		async_dcb.flags2&=~(1<<6); }
	else
		async_dcb.flags2&=~((1<<7)|(1<<6));
	if(mode&ABORT)
		rio_abort=1;
	else
		rio_abort=0;

	DosDevIOCtl(rio_handle
		,IOCTL_ASYNC, 0x53	/* Set Device Control Block (DCB) Parms */
		,&async_dcb
		,sizeof(async_dcb)
		,NULL
		,NULL
		,0
		,NULL);

#else // Win32

	GetCommState((HANDLE)rio_handle,&dcb);
	if(dcb.fOutxCtsFlow)
		mode|=CTSCK;
	if(dcb.fOutX)
		mode|=PAUSE;
	if(dcb.fRtsControl==RTS_CONTROL_HANDSHAKE)
		mode|=RTSCK;

	if(rio_abort)
		mode|=ABORT;

	if(action==IOMODE)
        return(mode);

	if((action&0xff)==IOCM) 	/* Clear mode */
		mode&=~(action&0xff00);
	else						/* Set mode */
        mode|=(action&0xff00);

	if(mode&CTSCK)
		dcb.fOutxCtsFlow=1;
	else
		dcb.fOutxCtsFlow=0;
	if(mode&PAUSE) {
		dcb.fOutX=1;
		dcb.XonChar=17; 	/* Ctrl-Q */
		dcb.XoffChar=19; }	/* Ctrl-S */
	else
		dcb.fOutX=0;
	if(mode&RTSCK)
		dcb.fRtsControl=RTS_CONTROL_HANDSHAKE;
	else
		dcb.fRtsControl=RTS_CONTROL_ENABLE;
	if(mode&ABORT)
		rio_abort=1;
	else
        rio_abort=0;

	SetCommState((HANDLE)rio_handle,&dcb);

#endif

	return(mode); }

if((action&0xff)==IOSS) {	/* Set state */

	if(action&ABORT)
		rio_aborted=1;

#ifdef __OS2__
	if(action&PAUSE)
		DosDevIOCtl(rio_handle
			,IOCTL_ASYNC, 0x47	/* Behave as if XOFF Received */
			,NULL
			,0
			,NULL
			,&w
			,sizeof(short)
            ,NULL);
#else // Win32
	if(action&PAUSE)
		EscapeCommFunction((HANDLE)rio_handle,SETXOFF);
#endif

	return(rio_getstate()); }

if((action&0xff)==IOCS) {	/* Clear state */

	if(action&ABORT)
		rio_aborted=0;

#ifdef __OS2__
	if(action&PAUSE)
		DosDevIOCtl(rio_handle
			,IOCTL_ASYNC, 0x48	/* Behave as if XON Received */
			,NULL
			,0
			,NULL
			,&w
			,sizeof(short)
            ,NULL);
#else // Win32
	if(action&PAUSE)
		EscapeCommFunction((HANDLE)rio_handle,SETXON);
#endif
    return(rio_getstate()); }

if((action&0xff)==TXSYNC) { /* Synchronize transmition */
    c=action>>8;            /* Number of seconds */
	w=110+(c*1000); 		/* Number of milliseconds */
	start=clock();
	while(clock()-start<w) {
		if(!outbufcnt()) {
#ifdef __OS2__
			DosDevIOCtl(rio_handle
				,IOCTL_ASYNC, 0x69	/* Query # of chars in tx queue */
				,NULL
				,0
				,NULL
				,&getqueue
				,sizeof(getqueue)
				,NULL);
			if(getqueue.cur==0) 	/* Empty outbound queue */
				return(0);
#else // Win32
			ClearCommError((HANDLE)rio_handle,&l,&comstat);
			if(comstat.cbOutQue==0) /* Empty outbound queue */
				return(0);
#endif
			}
#ifdef __OS2__
		DosSleep(1);
#else // Win32
		Sleep(1);
#endif
		}
	return(1); }

return(0);
}

/* End of file */
