/* STP.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Synchronet Transfer Protocols */

#include <io.h>
#include <dos.h>
#include <dir.h>
#include <time.h>
#include <alloc.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>

#define STP 1

#include "zmodem.h"

#define MAXERRORS	10

#define ERROR -1
							/* Various Character/Ctrl-Code Definitions	*/
#define SP			32		/* Space bar								*/
#define ESC			27		/* ESC Char 								*/
#define	CR			13		/* Carriage Return							*/
#define FF          12      /* Form Feed								*/
#define	LF			10		/* Line Feed								*/
#define TAB			9		/* Horizontal Tabulation					*/
#define BS			8		/* Back Space								*/
#define SOH 		1		/* Start of header							*/
#define STX         2		/* Start of text                            */
#define ETX         3       /* End of text                              */
#define EOT 		4		/* End of transmission						*/
#define ACK 		6		/* Acknowledge								*/
#define DLE 		16		/* Data link escape 						*/
#define XON 		17		/* Ctrl-Q - resume transmission 			*/
#define XOFF		19		/* Ctrl-S - pause transmission				*/
#define NAK 		21		/* Negative Acknowledge 					*/
#define CAN 		24		/* Cancel									*/
#define CPMEOF		26		/* CP/M End of file (^Z)					*/
#define LOC_ABORT	0x2e03	/* Local abort key	(^C)					*/

#define uchar	unsigned char
#define uint	unsigned int
#define ulong   unsigned long

#define SEND		(1<<0)	/* Sending file(s)							*/
#define RECV		(1<<1)	/* Receiving file(s)						*/
#define XMODEM		(1<<2)	/* Use Xmodem								*/
#define YMODEM		(1<<3)	/* Use Ymodem								*/
#define ZMODEM		(1<<4)	/* Use Zmodem								*/
#define CRC 		(1<<5)	/* Use CRC error correction 				*/
#define GMODE		(1<<6)	/* For Qmodem-G and Ymodem-G				*/
#define DIR 		(1<<7)	/* Directory specified to download to		*/
#define DEBUG		(1<<8)	/* Debug output 							*/
#define OVERWRITE	(1<<9)	/* Overwrite receiving files				*/
#define IGNORE_DCD	(1<<10) /* Ignore DCD								*/
#define ALARM		(1<<11) /* Alarm when starting and stopping xfer	*/
#define NO_LOCAL	(1<<12) /* Don't check local keyboard               */

							/* Zmodem mode bits 						*/
#define CTRL_ESC	(1<<0)	/* Escape all control chars 				*/
#define VAR_HDRS	(1<<1)	/* Use variable headers 					*/

#define LOOP_NOPEN	50

#define MAX_FNAMES	100 	/* Up to 100 filenames						*/

/************************/
/* Remote I/O Constants */
/************************/

							/* i/o mode and state flags */
#define CTSCK 0x1000     	/* check cts (mode only) */
#define RTSCK 0x2000		/* check rts (mode only) */
#define TXBOF 0x0800		/* transmit buffer overflow (outcom only) */
#define ABORT 0x0400     	/* check for ^C (mode), aborting (state) */
#define PAUSE 0x0200     	/* check for ^S (mode), pausing (state) */
#define NOINP 0x0100     	/* input buffer empty (incom only) */

							/* status flags */
#define DCD 	0x80       	/* DCD on */
#define RI		0x40		/* Ring indicate */
#define DSR 	0x20		/* Dataset ready */
#define CTS 	0x10       	/* CTS on */
#define FERR 	0x08		/* Frameing error */
#define PERR 	0x04		/* Parity error */
#define OVRR 	0x02		/* Overrun */
#define RXLOST 	0x01       	/* Receive buffer overflow */

/* rioctl() arguments */
/* returns mode or state flags in high 8 bits, status flags in low 8 bits */

							/* the following return mode in high 8 bits */
#define IOMODE 0        	/* no operation */
#define IOSM 1          	/* i/o set mode flags */
#define IOCM 2          	/* i/o clear mode flags */

#define GVERS 0x007 		/* get version */
#define GUART 0x107 		/* get uart */
#define GIRQN 0x207 		/* get IRQ number */
#define GBAUD 0x307 		/* get baud */

							/* the following return state in high 8 bits */
#define IOSTATE 4       	/* no operation */
#define IOSS 5          	/* i/o set state flags */
#define IOCS 6          	/* i/o clear state flags */
#define IOFB 0x308      	/* i/o buffer flush */
#define IOFI 0x208      	/* input buffer flush */
#define IOFO 0x108      	/* output buffer flush */
#define IOCE 9          	/* i/o clear error flags */

#define TS_INT28	 1
#define TS_WINOS2	 2
#define TS_NODV 	 4

							/* return count (16bit)	*/
#define RXBC	0x000a		/* get receive buffer count */
#define RXBS	0x010a		/* get receive buffer size */
#define TXBC	0x000b		/* get transmit buffer count */
#define TXBS	0x010b		/* get transmit buffer size */
#define TXBF	0x020b		/* get transmit buffer free space */
#define TXSYNC	0x000c		/* sync transmition (seconds<<8|0x0c) */
#define IDLE	0x000d		/* suspend communication routines */
#define RESUME	0x010d		/* return from suspended state */
#define RLERC	0x000e		/* read line error count and clear */
#define CPTON	0x0110		/* set input translation flag for ctrl-p on */
#define CPTOFF	0x0010		/* set input translation flag for ctrl-p off */
#define GETCPT	0x8010		/* return the status of ctrl-p translation */
#define MSR 	0x0011		/* read modem status register */
#define FIFOCTL 0x0012		/* FIFO UART control */
#define TSTYPE	0x0013		/* Time-slice API type */
#define GETTST	0x8013		/* Get Time-slice API type */

							/* ivhctl() arguments */
#define INT29R 1         	/* copy int 29h output to remote */
#define INT29L 2			/* Use _putlc for int 29h */
#define INT16  0x10      	/* return remote chars to int 16h calls */
#define INTCLR 0            /* release int 16h, int 29h */

#define DCDHIGH (rioctl(IOSTATE)&DCD || mode&IGNORE_DCD)

/**************/
/* Prototypes */
/**************/

/* LCLOLL.ASM */
int	 lclini(int);
void lclxy(int,int);
int  lclwx(void);
int  lclwy(void);
int  lclatr(int);
int  lclaes(void);
void lputc(int);
long lputs(char far *);
uint lkbrd(int);

/* RCIOLL.ASM */

int  rioini(int com,int irq);          /* initialize com,irq */
int  setbaud(int rate);                /* set baud rate */
int  rioctl(int action);               /* remote i/o control */
int  dtr(char onoff);                  /* set/reset dtr */
int  incom(void);                      /* receive character */
int  ivhctl(int intcode);              /* local i/o redirection */

void riosync();
void cancel();

extern	mswtyp;
extern uint riobp;

uint	asmrev;

/***************/
/* Global Vars */
/***************/
long	mode=0L;						/* Program mode 					*/
long	zmode=0L;						/* Zmodem mode						*/
uchar	hdr_block_num;					/* Block number received in header	*/
uchar	block[1024];					/* Block buffer 					*/
uint	block_size; 					/* Block size (128 or 1024) 		*/
ulong	block_num;						/* Block number 					*/
ulong	last_block_num; 				/* Last block number sent			*/
uint	flows=0;						/* Number of flow controls			*/

/**************************************/
/* Zmodem specific, from Chuck's RZSZ */
/**************************************/
char	Txhdr[ZMAXHLEN];				/* Zmodem transmit header			*/
char	Rxhdr[ZMAXHLEN];				/* Zmodem receive header			*/
char	attn[ZATTNLEN]; 				/* Attention string for sender		*/
char	zconv;
char	zmanag;
char	ztrans;
int 	tryzhdrtype;
int 	Rxtimeout = 100;		/* Tenths of seconds to wait for something */
int 	Rxframeind; 			/* ZBIN ZBIN32, or ZHEX type of frame */
int 	Rxtype; 				/* Type of header received */
int 	Rxhlen; 				/* Length of header received */
int 	Rxcount;				/* Count of data bytes received */

/********/
/* Code */
/********/

int cbreakh()
{
return(1);
}

/****************************************************************************/
/* Performs printf() through local assembly routines                        */
/****************************************************************************/
int lprintf(char *fmat, ...)
{
	va_list argptr;
	char sbuf[2048];
	int chcount;

va_start(argptr,fmat);
chcount=vsprintf(sbuf,fmat,argptr);
va_end(argptr);
lputs(sbuf);
return(chcount);
}

void newline()
{
if(lclwx()>1)
	lputs("\r\n");
}

/****************************************************************************/
/* Network open function. Opens all files DENYALL and retries LOOP_NOPEN    */
/* number of times if the attempted file is already open or denying access  */
/* for some other reason.	All files are opened in BINARY mode.			*/
/****************************************************************************/
int nopen(char *str, int access)
{
	char count=0,logstr[256];
	int file,share;

if(access==O_RDONLY) share=O_DENYWRITE;
	else share=O_DENYALL;
while(((file=open(str,O_BINARY|share|access,S_IWRITE))==-1)
	&& errno==EACCES && count++<LOOP_NOPEN);
if(file==-1 && errno==EACCES)
	lprintf("\7\r\nNOPEN: ACCESS DENIED\r\n\7");
return(file);
}


/****************************************************************************/
/* Converts an ASCII Hex string into an ulong								*/
/****************************************************************************/
ulong ahtoul(char *str)
{
	ulong l,val=0;

while((l=(*str++)|0x20)!=0x20)
	val=(l&0xf)+(l>>6&1)*9+val*16;
return(val);
}

/****************************************************************************/
/* Checks the disk drive for the existence of a file. Returns 1 if it       */
/* exists, 0 if it doesn't.                                                 */
/****************************************************************************/
char fexist(char *filespec)
{
    struct ffblk f;

if(findfirst(filespec,&f,0)==NULL)
    return(1);
return(0);
}

/****************************************************************************/
/* Truncates white-space chars off end of 'str' and terminates at first tab */
/****************************************************************************/
void truncsp(char *str)
{
	char c;

str[strcspn(str,"\t")]=0;
c=strlen(str);
while(c && str[c-1]<=SP) c--;
str[c]=0;
}

/**************/
/* Exit Point */
/**************/
void bail(int code)
{

if(mode&ALARM) {
	sound(2000);
	mswait(500);
	sound(1000);
	mswait(500);
	nosound(); }
riosync();
rioini(0,0);	/* uninstall com routines */
newline();
lprintf("Exiting - Error level: %d  Flow restraint count: %u\r\n",code,flows);
fcloseall();
exit(code);
}

/************************************************************/
/* Get a character from com port, time out after 10 seconds */
/************************************************************/
int getcom(char timeout)
{
	uint	i,ch;
	time_t	start;

if((ch=incom())!=NOINP)
	return(ch);
for(i=0;i<10000;i++)			   /* ten consecutive re-tries */
	if((ch=incom())!=NOINP)
		return(ch);
flows++;
start=time(NULL);
while(time(NULL)-start<(long)timeout) {   /* wait up to ten seconds */
	if((ch=incom())!=NOINP)
		return(ch);
	if(!DCDHIGH) {
		newline();
		lprintf("No carrier\r\n");
		bail(1); }
	mswait(0);
	if(!(mode&NO_LOCAL) && lkbrd(0)==LOC_ABORT) {
		newline();
		lprintf("Local abort\r\n");
		cancel();
		bail(1); } }
newline();
lprintf("Input timeout\r\n");
return(NOINP);
}

/**********************************/
/* Output a character to COM port */
/**********************************/
void putcom(uchar ch)
{
	int i=0;

while(outcom(ch)&TXBOF && i<180) { /* 10 sec delay */
	if(!i) lputc('F');
	if(!DCDHIGH) {
		newline();
		lprintf("No carrier\r\n");
		bail(1); }
	i++;
	mswait(1);
	if(!(mode&NO_LOCAL) && lkbrd(0)==LOC_ABORT) {
		newline();
		lprintf("Local abort\r\n");
		bail(1); } }
if(i) {
	lprintf("\b \b");
	flows++;
	if(i==180) {
		newline();
		lprintf("Output timeout\r\n");
		bail(1); } }
}

void put_nak()
{
while(getcom(1)!=NOINP && (mode&NO_LOCAL || lkbrd(0)!=LOC_ABORT))
	;				/* wait for any trailing data */
putcom(NAK);
}

void cancel()
{
	int i;

for(i=0;i<10;i++)
	putcom(CAN);
for(i=0;i<10;i++)
	putcom(BS);
}

/********************************/
/* Update the CRC bytes 		*/
/********************************/
/****
void update_crc(uchar c, uchar *crc1, uchar *crc2)
{
	int i, temp;
	uchar carry, c_crc1, c_crc2;

for (i=0; i < 8; i++) {
	temp = c * 2;
	c = temp;			/* rotate left */
	carry = ((temp > 255) ? 1 : 0);
	temp = (*crc2) * 2;
	(*crc2) = temp;
	(*crc2) |= carry;		   /* rotate with carry */
	c_crc2 = ((temp > 255) ? 1 : 0);
	temp = (*crc1) * 2;
	(*crc1) = temp;
	(*crc1) |= c_crc2;
	c_crc1 = ((temp > 255) ? 1 : 0);
	if (c_crc1) {
		(*crc2) ^= 0x21;
		(*crc1) ^= 0x10; } }
}
****/

void ucrc16(uchar ch, uint *rcrc) {
    uint i, cy;
    uchar nch=ch;
 
for (i=0; i<8; i++) {
    cy=*rcrc & 0x8000;
    *rcrc<<=1;
    if (nch & 0x80) *rcrc |= 1;
    nch<<=1;
    if (cy) *rcrc ^= 0x1021; }
}


char *chr(uchar ch)
{
	static char str[25];

switch(ch) {
	case SOH:
		return("SOH");
	case STX:
		return("STX");
	case ETX:
		return("ETX");
	case EOT:
		return("EOT");
	case ACK:
		return("ACK");
	case NAK:
		return("NAK");
	case CAN:
		return("CAN");
	default:
		sprintf(str,"%02Xh",ch);
		return(str); }
}

/****************************************************************/
/* Gets the filename from the filename and optional path in buf */
/****************************************************************/
char *justfname(char *buf, char *out)
{
	char *p1,*p2;

if(mode&DEBUG)
	lprintf("justfname:  in: '%s'\r\n",buf);
p1=buf;
while(*p1) {
	if((p2=strchr(p1,':'))!=NULL) {      /* Remove path */
		p1=p2+1;
		continue; }
	if((p2=strchr(p1,'/'))!=NULL) {
		p1=p2+1;
		continue; }
	if((p2=strchr(p1,'\\'))!=NULL) {
		p1=p2+1;
		continue; }
	break; }
if(!*p1) {
	lprintf("Invalid filename\r\n");
	strcpy(out,""); }
else
	strcpy(out,p1); 					   /* Use just the filename */
if(mode&DEBUG)
	lprintf("justfname: out: '%s'\r\n",out);
return(out);
}

/****************************************************************************/
/* Receive a X/Y/Zmodem block (sector) from COM port						*/
/* hdrblock is 1 if attempting to get Ymodem header block, 0 if data block	*/
/* Returns 0 if all went well, -1 on error or CAN, and EOT if EOT			*/
/****************************************************************************/
int get_block(int hdrblock)
{
	uchar chksum,calc_chksum;
	int i,b,errors,eot=0,can=0;
	uint crc,calc_crc;

for(errors=0;errors<MAXERRORS;errors++) {
	i=getcom(10);
	if(eot && i!=EOT)
		eot=0;
	if(can && i!=CAN)
		can=0;
	switch(i) {
		case SOH: /* 128 byte blocks */
			block_size=128;
			break;
		case STX: /* 1024 byte blocks */
			block_size=1024;
			break;
		case EOT:
			if((mode&(YMODEM|GMODE))==YMODEM && !eot) {
				eot=1;
				put_nak();		/* chuck's double EOT trick */
				continue; }
			return(EOT);
		case CAN:
			newline();
			if(!can) {			/* must get two CANs in a row */
				can=1;
				lprintf("Received CAN  Expected SOH, STX, or EOT\r\n");
				continue; }
			lprintf("Cancelled remotely\r\n");
			return(-1);
		case NOINP: 	/* Nothing came in */
			continue;
		default:
			newline();
			lprintf("Received %s  Expected SOH, STX, or EOT\r\n",chr(i));
			if(hdrblock)  /* Trying to get Ymodem header block */
				return(-1);
			put_nak();
			continue; }
	i=getcom(1);
	if(i==NOINP) {
		put_nak();
		continue; }
	hdr_block_num=i;
	i=getcom(1);
	if(i==NOINP) {
		put_nak();
		continue; }
	if(hdr_block_num!=(uchar)~i) {
		newline();
		lprintf("Block number error\r\n");
		put_nak();
		continue; }
	calc_crc=calc_chksum=0;
	for(b=0;b<block_size;b++) {
		i=getcom(1);
		if(i==NOINP)
			break;
		block[b]=i;
		if(mode&CRC)
			ucrc16(block[b],&calc_crc);
		else
			calc_chksum+=block[b]; }

	if(b<block_size) {
		put_nak();
		continue; }

	if(mode&CRC) {
		crc=getcom(1)<<8;
		crc|=getcom(1); }
	else
		chksum=getcom(1);

	if(mode&CRC) {
		ucrc16(0,&calc_crc);
		ucrc16(0,&calc_crc);
		if(crc==calc_crc)
			break;
		newline();
		lprintf("CRC error\r\n"); }

	else {	/* CHKSUM */
		if(chksum==calc_chksum)
			break;
		newline();
		lprintf("Checksum error\r\n"); }

	if(mode&GMODE) {	/* Don't bother sending a NAK. He's not listening */
		cancel();
		bail(1); }
	put_nak(); }

if(errors>=MAXERRORS) {
	newline();
	lprintf("Too many errors\r\n");
	return(-1); }
return(0);
}

/*****************/
/* Sends a block */
/*****************/
void put_block()
{
	uchar ch,chksum;
    int i;
	uint crc;

if(block_size==128)
	putcom(SOH);
else			/* 1024 */
	putcom(STX);
ch=(block_num&0xff);
putcom(ch);
putcom(~ch);
chksum=crc=0;
for(i=0;i<block_size;i++) {
	putcom(block[i]);
	if(mode&CRC)
		ucrc16(block[i],&crc);
	else
		chksum+=block[i]; }

if(mode&CRC) {
	ucrc16(0,&crc);
	ucrc16(0,&crc);
	putcom(crc>>8);
	putcom(crc&0xff); }
else
	putcom(chksum);
}

/************************************************************/
/* Gets an acknowledgement - usually after sending a block	*/
/* Returns 1 if ack received, 0 otherwise.					*/
/************************************************************/
int get_ack()
{
	int i,errors,can=0;

for(errors=0;errors<MAXERRORS;errors++) {

	if(mode&GMODE) {		/* Don't wait for ACK on Ymodem-G */
		if(incom()==CAN) {
			newline();
			lprintf("Cancelled remotely\r\n");
			cancel();
			bail(1); }
		return(1); }

	i=getcom(10);
	if(can && i!=CAN)
		can=0;
	if(i==ACK)
		return(1);
	if(i==CAN) {
		if(can) {
			newline();
			lprintf("Cancelled remotely\r\n");
			cancel();
			bail(1); }
		can=1; }
	if(i!=NOINP) {
		newline();
		lprintf("Received %s  Expected ACK\r\n",chr(i));
		if(i!=CAN)
			return(0); } }

return(0);
}

/****************************************************************************/
/* Syncronizes the remote and local machines								*/
/****************************************************************************/
void riosync()
{
	int 	i=0;

while(rioctl(TXBC) && i<180) { /* 10 sec */
	if(!(mode&NO_LOCAL) && lkbrd(0)==LOC_ABORT) {
		newline();
		lprintf("Local abort\r\n");
		cancel();
		break; }
	if(!DCDHIGH)
		break;
	mswait(1);
	i++; }
}

/*********************************************************/
/* Returns the number of blocks required to send n bytes */
/*********************************************************/
long blocks(long n)
{
	long l;

l=n/(long)block_size;
if(l*(long)block_size<n)
	l++;
return(l);
}

/****************************************/
/* Zmodem specific functions start here */
/****************************************/

/**********************************************/
/* Output single byte as two hex ASCII digits */
/**********************************************/
void putzhex(uchar val)
{
	char *digits="0123456789abcdef";

putcom(digits[(val&0xF0)>>4]);
putcom(digits[val&0xF]);
}

/***********************/
/* Output a hex header */
/***********************/
void putzhhdr(char type)
{
	uint i,crc=0;

putcom(ZPAD);
putcom(ZPAD);
putcom(ZDLE);
if(zmode&VAR_HDRS) {
	putcom(ZVHEX);
	putzhex(4); }
else
	putcom(ZHEX);
putzhex(type);
ucrc16(type,&crc);
for(i=0;i<4;i++) {
	putzhex(Txhdr[i]);
	ucrc16(Txhdr[i],&crc); }
ucrc16(0,&crc);
ucrc16(0,&crc);
putzhex(crc>>8);
putzhex(crc&0xff);
putcom(CR);
putcom(LF); 	/* Chuck's RZ.C sends LF|0x80 for some unknown reason */
if(type!=ZFIN && type!=ZACK)
	putcom(XON);
}

/****************************************************************************/
/* Stores a long in the Zmodem transmit header (usually position offset)	*/
/****************************************************************************/
void ltohdr(long l)
{

Txhdr[ZP0] = l;
Txhdr[ZP1] = l>>8;
Txhdr[ZP2] = l>>16;
Txhdr[ZP3] = l>>24;
}

/****************************************************************************/
/* Outputs single Zmodem character, escaping with ZDLE when appropriate     */
/****************************************************************************/
void putzcom(uchar ch)
{
    static lastsent;

if(ch&0x60) /* not a control char */
    putcom(lastsent=ch);
else
    switch(ch) {
        case DLE:
        case DLE|0x80:          /* even if high-bit set */
        case XON:
        case XON|0x80:
        case XOFF:
        case XOFF|0x80:
        case ZDLE:
            putcom(ZDLE);
            ch^=0x40;
            putcom(lastsent=ch);
            break;
        case CR:
        case CR|0x80:
            if(!(zmode&CTRL_ESC) && (lastsent&0x7f)!='@')
                putcom(lastsent=ch);
            else {
                putcom(ZDLE);
                ch^=0x40;
                putcom(lastsent=ch); }
            break;
        default:
            if(zmode&CTRL_ESC && !(ch&0x60)) {  /* it's a ctrl char */
                putcom(ZDLE);
                ch^=0x40; }
            putcom(lastsent=ch);
            break; }
}

/*
 * Read a byte, checking for ZMODEM escape encoding
 *  including CAN*5 which represents a quick abort
 */
int getzcom()
{
	int i;

while(1) {
	/* Quick check for non control characters */
	if((i=getcom(Rxtimeout))&0x60)
		return(i);
	if(i==ZDLE)
		break;
	if((i&0x7f)==XOFF || (i&0x7f)==XON)
		continue;
	if(zmode&CTRL_ESC && !(i&0x60))
		continue;
	return(i); }

while(1) {	/* Escaped characters */
	if((i=getcom(Rxtimeout))<0)
		return(i);
	if(i==CAN && (i=getcom(Rxtimeout))<0)
		return(i);
	if(i==CAN && (i=getcom(Rxtimeout))<0)
        return(i);
	if(i==CAN && (i=getcom(Rxtimeout))<0)
        return(i);
	switch (i) {
		case CAN:
			return(GOTCAN);
		case ZCRCE:
		case ZCRCG:
		case ZCRCQ:
		case ZCRCW:
			return(i|GOTOR);
		case ZRUB0:
			return(0x7f);
		case ZRUB1:
			return(0xff);
		case XON:
		case XON|0x80:
		case XOFF:
		case XOFF|0x80:
			continue;
		default:
			if (zmode&CTRL_ESC && !(i&0x60))
				continue;
			if ((i&0x60)==0x40)
				return(i^0x40);
			break; }
	break; }
return(ERROR);
}



/*
 * Read a character from the modem line with timeout.
 *  Eat parity, XON and XOFF characters.
 */
int getcom7()
{
	int i;

while(1) {
	i=getcom(10);
	switch(i) {
		case XON:
		case XOFF:
			continue;
		case CR:
		case LF:
		case NOINP:
		case ZDLE:
			return(i);
		default:
			if(!(i&0x60) && zmode&CTRL_ESC)
				continue;
			return(i); } }
}


#if 0

int getzhdr()
{
	int done=0;

while(!done) {
	i=getcom(10);
	switch(i) {
		case NOINP:
			done=1;
			continue;
		case XON:
		case XON|0x80:
			continue;
		case ZPAD|0x80:
		case ZPAD:
			break;
		case CAN:
			cancount++;
			if(cancount>=5) {
				i=ZCAN;
				done=1;
				break; }
			i=getcom(10);
			switch(i) {
				case NOINP:
					continue;
				case ZCRCW:
					switch(getcom(10)) {
						case NOINP:
							i=ERROR
							done=1;
							break;
						case RCDO:
							done=1;
							break;
						default:
							continue; }
					break;
				case RCDO:
					done=1;
					break;
				case CAN:
					cancount++;
					if(cancount>=5) {
						i=ZCAN;
						done=1;
					continue;
				default:
					break; }
			continue;
		default:
			continue; }

	i=Rxframeind=getcom7();
	switch (i) {
		case ZVBIN32:
			if((Rxhlen=c=getzcom())<0)
				goto fifi;
			if(c>ZMAXHLEN)
				goto agn2;
			Crc32r=1;
			c=zrbhd32(hdr);
			break;
		case ZBIN32:
			if(zmode&VAR_HDRS)
				goto agn2;
			Crc32r=1;
			c=zrbhd32(hdr);
			break;
		case ZVBINR32:
			if((Rxhlen=c=getzcom())<0)
				goto fifi;
			if(c>ZMAXHLEN)
				goto agn2;
			Crc32r=2;
			c=zrbhd32(hdr);
			break;
		case ZBINR32:
			if(zmode&VAR_HDRS)
				goto agn2;
			Crc32r=2;
			c=zrbhd32(hdr);
			break;
		case RCDO:
		case TIMEOUT:
			goto fifi;
		case ZVBIN:
			if((Rxhlen=c=getzcom())<0)
				goto fifi;
			if(c>ZMAXHLEN)
				goto agn2;
			Crc32r=0;
			c=zrbhdr(hdr);
			break;
		case ZBIN:
			if(zmode&VAR_HDRS)
				goto agn2;
			Crc32r=0;
			c=zrbhdr(hdr);
			break;
		case ZVHEX:
			if((Rxhlen=c=zgethex()) < 0)
				goto fifi;
			if(c>ZMAXHLEN)
				goto agn2;
			Crc32r=0;
			c=zrhhdr(hdr);
			break;
		case ZHEX:
			if(zmode&VAR_HDRS)
				goto agn2;
			Crc32r=0;
			c=zrhhdr(hdr);
			break;
		case CAN:
			goto gotcan;
		default:
			goto agn2;
		}



/****************************************************************************/
/* Get the receiver's init parameters                                       */
/****************************************************************************/
int getzrxinit()
{
	int i;
	struct stat f;

for(i=0;i<10;i++) {
	switch(zgethdr(Rxhdr,1)) {
		case ZCHALLENGE:	/* Echo receiver's challenge numbr */
			ltohdr(Rxpos);
			putzhhdr(ZACK);
			continue;
		case ZCOMMAND:		/* They didn't see out ZRQINIT */
			ltohdr(0L);
			putzhhdr(ZRQINIT);
			continue;
		case ZRINIT:
			Rxflags=Rxhdr[ZF0]&0x7f;
			if(Rxhdr[ZF1]&CANVHDR)
                zmode|=VAR_HDRS;
			Txfcs32 = (Wantfcs32 && (Rxflags & CANFC32));
			Zctlesc |= Rxflags & TESCCTL;
			Rxbuflen = (0377 & Rxhdr[ZP0])+((0377 & Rxhdr[ZP1])<<8);
			if ( !(Rxflags & CANFDX))
				Txwindow = 0;

			/* Override to force shorter frame length */
			if (Rxbuflen && (Rxbuflen>Tframlen) && (Tframlen>=32))
				Rxbuflen = Tframlen;
			if ( !Rxbuflen && (Tframlen>=32) && (Tframlen<=1024))
				Rxbuflen = Tframlen;
			vfile("Rxbuflen=%d", Rxbuflen);

			/*
			 * If input is not a regular file, force ACK's to
			 *	prevent running beyond the buffer limits
			 */
			if ( !Command) {
				fstat(fileno(in), &f);
				if ((f.st_mode & S_IFMT) != S_IFREG) {
					Canseek = -1;
	ef TXBSIZE
					Txwindow = TXBSIZE - 1024;
					Txwspac = TXBSIZE/4;
	e
					return ERROR;
	if
				}
			}

			/* Set initial subpacket length */
			if (blklen < 1024) {	/* Command line override? */
				if (Effbaud > 300)
					blklen = 256;
				if (Effbaud > 1200)
					blklen = 512;
				if (Effbaud > 2400)
					blklen = 1024;
			}
			if (Rxbuflen && blklen>Rxbuflen)
				blklen = Rxbuflen;
			if (blkopt && blklen > blkopt)
				blklen = blkopt;
			vfile("Rxbuflen=%d blklen=%d", Rxbuflen, blklen);
			vfile("Txwindow = %u Txwspac = %d", Txwindow, Txwspac);


			if (Lztrans == ZTRLE && (Rxflags & CANRLE))
				Txfcs32 = 2;
			else
				Lztrans = 0;

			return (sendzsinit());
		case ZCAN:
		case TIMEOUT:
			return ERROR;
		case ZRQINIT:
			if (Rxhdr[ZF0] == ZCOMMAND)
				continue;
		default:
			putzhhdr(ZNAK);
			continue; } }
return(ERROR);
}


getzdata(char *buf, int length)
{
	int c,d;
	uint crc;
	char *end;

	switch (Crc32r) {
	case 1:
		return zrdat32(buf, length);
	case 2:
		return zrdatr32(buf, length);
	}

	crc = Rxcount = 0;  end = buf + length;
	while (buf <= end) {
		if ((c = getzcom()) & ~0377) {
crcfoo:
			switch (c) {
			case GOTCRCE:
			case GOTCRCG:
			case GOTCRCQ:
			case GOTCRCW:
				crc = updcrc((d=c)&0377, crc);
				if ((c = getzcom()) & ~0377)
					goto crcfoo;
				crc = updcrc(c, crc);
				if ((c = getzcom()) & ~0377)
					goto crcfoo;
				crc = updcrc(c, crc);
				if (crc & 0xFFFF) {
					zperr(badcrc);
					return ERROR;
				}
				Rxcount = length - (end - buf);
#ifndef DSZ
				vfile("zrdata: %d  %s", Rxcount,
				 Zendnames[d-GOTCRCE&3]);
#endif
				return d;
			case GOTCAN:
				zperr("Sender Canceled");
				return ZCAN;
			case TIMEOUT:
				zperr("TIMEOUT");
				return c;
			default:
				garbitch(); return c;
			}
		}
		*buf++ = c;
		crc = updcrc(c, crc);
	}
#ifdef DSZ
	garbitch(); 
#else
	zperr("Data subpacket too long");
#endif
	return ERROR;
}

#endif

/************************************************/
/* Dump the current blockm contents - for debug */
/************************************************/
void dump_block()
{
	long l;

for(l=0;l<block_size;l++)
	lprintf("%02X  ",block[l]);
lputs("\r\n");
}

char	*usage=
"usage: STP <port> [chan] [opts] <cmd> [file | path | +list]\r\n\r\n"
"where:\r\n\r\n"
"port   = COM port (1-4), D for DigiBoard, or UART I/O address (hex)\r\n"
"chan   = IRQ channel (2-15) or DigiBoard channel (4+)\r\n"
"opts   = b# to set DTE rate to #bps\r\n"
"         t# to set time-slice API type (default=1)\r\n"
"         o  to overwrite files when receiving\r\n"
"         d  to disable dropped carrier detection\r\n"
"         a  to sound alarm at start and stop of transfer\r\n"
"         l  to disable local keyboard (Ctrl-C) checking\r\n"
"cmd    = sx to send Xmodem     rx to recv Xmodem\r\n"
"         sX to send Xmodem-1k  rc to recv Xmodem-CRC\r\n"
"         sy to send Ymodem     ry to recv Ymodem\r\n"
"         sY to send Ymodem-1k  rg to recv Ymodem-G\r\n"
"         sz to send Zmodem     rz to recv Zmodem\r\n"
"file   = filename to send or receive\r\n"
"path   = path to receive files into\r\n"
"list   = name of text file with list of filenames to send or receive\r\n";

/***************/
/* Entry Point */
/***************/
int main(int argc, char **argv)
{
	char	str[256],tmp[256],tmp2[256],irq,errors,*p,*p2,first_block
			,*fname[MAX_FNAMES],fnames=0,fnum,success;
	int 	ch,i,j,k,file,last,total_files=0,sent_files=0,can;
	uint	base=0,baud=0,cps;
	long	b,l,m,file_bytes_left,serial_num;
	ulong	file_bytes=0,total_bytes=0,sent_bytes=0;
	time_t	t,startall,startfile,ftime;
	struct	ffblk ff;
	struct	ftime ft;
	struct	date dosdate;
	struct	time dostime;
	FILE	*stream,*log=NULL;

mswtyp=TS_INT28;   /* default to int 28 only */

ctrlbrk(cbreakh);

if((asmrev=*(&riobp-1))!=19) {
	printf("Wrong rciol.obj\n");
    exit(1); }

lclini(0xd<<8);

lprintf("\r\nSynchronet Transfer Protocols v1.00"
	"  Developed 1993-1997 Rob Swindell\r\n\r\n");

lputs("Command line: ");
for(i=1;i<argc;i++)
	lprintf("%s ",argv[i]);
lputs("\r\n");

if(argc<3) {
	lprintf(usage);
	exit(1); }

i=1;

base=ahtoul(argv[i]);
if(base>4 && base<0x100)	/* 'D' and 'FFFF' are the same */
	base=0xffff;
if(base!=0xffff && base<5)
	switch(atoi(argv[i])) {
		case 1:
			base=0x3f8;
			irq=4;
			break;
		case 2:
			base=0x2f8;
			irq=3;
			break;
		case 3:
			base=0x3e8;
			irq=4;
			break;
		case 4:
			base=0x2e8;
			irq=3;
			break;
		default:
			lprintf("Invalid COM Port (%s)\r\n",argv[1]);
			lprintf(usage);
			exit(1); }

if(base>4 || isdigit(argv[i+1][0]))
	irq=atoi(argv[++i]);

for(i++;i<argc;i++) {

	if(!(mode&(SEND|RECV))) {
		if(toupper(argv[i][0])=='S' || toupper(argv[i][0])=='R') { /* cmd */
			if(toupper(argv[i][0])=='R')
				mode|=RECV;
			else
				mode|=SEND;

			block_size=1024;

			switch(argv[i][1]) {
				case 'c':
				case 'C':
					mode|=CRC;
				case 'x':
					block_size=128;
				case 'X':
					mode|=XMODEM;
					break;
				case 'y':
					block_size=128;
				case 'Y':
					mode|=(YMODEM|CRC);
					break;
				case 'g':
				case 'G':
					mode|=(YMODEM|CRC|GMODE);
					break;
				case 'z':
				case 'Z':
					mode|=(ZMODEM|CRC);
					break;
				default:
					lprintf("Unrecognized command '%s'\r\n\r\n",argv[i]);
					lprintf(usage);
					exit(1); } }

		else if(toupper(argv[i][0])=='B')
			baud=atoi(argv[i]+1);

		else if(toupper(argv[i][0])=='T')
			mswtyp=atoi(argv[i]+1);

		else if(toupper(argv[i][0])=='O')
			mode|=OVERWRITE;

		else if(toupper(argv[i][0])=='D')
			mode|=IGNORE_DCD;

		else if(toupper(argv[i][0])=='A')
			mode|=ALARM;

		else if(toupper(argv[i][0])=='L')
			mode|=NO_LOCAL;

		else if(argv[i][0]=='*')
			mode|=DEBUG; }

	else if(argv[i][0]=='+') {
		if(mode&DIR) {
			lprintf("Cannot specify both directory and filename\r\n");
			exit(1); }
		sprintf(str,"%s",argv[i]+1);
		if((file=nopen(str,O_RDONLY))==-1
			|| (stream=fdopen(file,"rb"))==NULL) {
			lprintf("Error opening filelist %s\r\n",str);
			exit(1); }
		while(!feof(stream) && !ferror(stream) && fnames<MAX_FNAMES) {
			if(!fgets(str,128,stream))
				break;
			truncsp(str);
			strupr(str);
			if((fname[fnames]=(char *)malloc(strlen(str)+1))==NULL) {
				lprintf("Error allocating memory for filename\r\n");
				exit(1); }
			strcpy(fname[fnames++],str); }
		fclose(stream); }

	else if(mode&(SEND|RECV)){
		if((fname[fnames]=(char *)malloc(128))==NULL) {
			lprintf("Error allocating memory for filename\r\n");
			exit(1); }
		strcpy(fname[fnames],argv[i]);
		strupr(fname[fnames]);
		j=strlen(fname[fnames]);
		if(fname[fnames][j-1]=='\\'                 /* Ends in \ */
			|| !strcmp(fname[fnames]+1,":")) {      /* Drive letter only */
/*			  || !findfirst(fname[fnames],&ff,FA_DIREC)) {	  is a directory */
			if(mode&DIR) {
				lprintf("Only one directory can be specified\r\n");
				exit(1); }
			if(fnames) {
				lprintf("Cannot specify both directory and filename\r\n");
				exit(1); }
			if(mode&SEND) {
				lprintf("Cannot send directory '%s'\r\n",fname[fnames]);
				exit(1);}
			mode|=DIR; }
		fnames++; } }

if(!(mode&(SEND|RECV))) {
	lprintf("No command specified\r\n");
	lprintf(usage);
	exit(1); }

if(mode&(SEND|XMODEM) && !fnames) { /* Sending with any or recv w/Xmodem */
	lprintf("Must specify filename or filelist\r\n");
	lprintf(usage);
	exit(1); }

i=strlen(fname[0]); 	/* Make sure the directory ends in \ or : */
if(mode&DIR && fname[0][i-1]!='\\' && fname[0][i-1]!=':')
	strcat(fname[0],"\\");

i=rioini(base,irq);
if(i) {
	lprintf("STP: Error (%d) initializing COM port base %x irq %d\r\n"
		,i,base,irq);
	exit(1); }

if(mode&ALARM) {
	sound(1000);
	mswait(500);
	sound(2000);
	mswait(500);
	nosound(); }

if(baud) {
	i=setbaud(baud);
	if(i) {
		lprintf("STP: Error setting baud rate to %u\r\n",baud);
		bail(1); } }
else
	baud=rioctl(GBAUD);

rioctl(IOSM|CTSCK|RTSCK);

rioctl(TSTYPE|mswtyp);			/* set time-slice API type */

if(!DCDHIGH) {
	newline();
	lprintf("No carrier\r\n");
	bail(1); }

p=getenv("DSZLOG");
if(p) {
	if((file=open(p,O_WRONLY|O_BINARY|O_CREAT,S_IWRITE|S_IREAD))==-1
		|| (log=fdopen(file,"wb"))==NULL) {
		lprintf("Error opening DSZLOG file '%s'\r\n",p);
		bail(1); }
	setvbuf(log,NULL,_IOFBF,16*1024); }

startall=time(NULL);

if(mode&RECV) {
	if(fnames>1)
		lprintf("Receiving %u files\r\n",fnames);
	fnum=0;
	while(1) {
		if(mode&XMODEM) {
			strcpy(str,fname[0]);
			file_bytes=file_bytes_left=0x7fffffff;
			serial_num=-1; }

		else if(mode&YMODEM) {
			lprintf("Fetching Ymodem header block\r\n");
			for(errors=0;errors<MAXERRORS;errors++) {
				if(errors>3 && mode&CRC && !(mode&GMODE))
					mode&=~CRC;
				if(mode&GMODE)		/* G for Ymodem-G */
					putcom('G');
				else if(mode&CRC)	/* C for CRC */
					putcom('C');
				else				/* NAK for checksum */
					putcom(NAK);
				for(i=60;i;i--) {
					if(rioctl(RXBC))	/* no chars in-bound */
						break;
					mswait(100); }		/* so wait */

				if(!i) {				/* none after 6 secs */
					if(errors)
						lprintf("Ymodem header timeout (%d)\r\n",errors);
					continue; }
				if(!get_block(1)) { 	 /* block received successfully */
					putcom(ACK);
					break; } }
			if(errors==MAXERRORS) {
				lprintf("Error fetching Ymodem header block\r\n");
				cancel();
				bail(1); }
			if(!block[0]) {
				lputs("Received Ymodem termination block\r\n");
				bail(0); }
			p=block+strlen(block)+1;
			sscanf(p,"%ld %lo %lo %lo %d %ld"
				,&file_bytes			/* file size (decimal) */
				,&ftime 				/* file time (octal unix format) */
				,&m 					/* file mode (not used) */
				,&serial_num			/* program serial number */
				,&total_files			/* remaining files to be sent */
				,&total_bytes			/* remaining bytes to be sent */
				);
			if(!file_bytes)
				file_bytes=0x7fffffff;
			file_bytes_left=file_bytes;
			if(!total_files)
				total_files=fnames-fnum;
			if(!total_files)
				total_files=1;
			if(total_bytes<file_bytes)
				total_bytes=file_bytes;
			if(!serial_num)
				serial_num=-1;
			strupr(block);
			lprintf("Incoming filename: %.64s ",block);
			if(mode&DIR)
				sprintf(str,"%s%s",fname[0],justfname(block,tmp));
			else {
				justfname(block,str);
				for(i=0;i<fnames;i++) {
					if(!fname[i][0])	/* name blank or already used */
						continue;
					justfname(fname[i],tmp);
					if(!stricmp(tmp,str)) {
						strcpy(str,fname[i]);
						fname[i][0]=0;
						break; } }
				if(i==fnames) { 				/* Not found in list */
					if(fnames)
						lputs(" - Not in receive list!");
					if(!fnames || fnum>=fnames || !fname[fnum][0])
						justfname(block,str);	/* worst case */
					else {
						strcpy(str,fname[fnum]);
						fname[fnum][0]=0; } } }
			lputs("\r\n"); }

		else {	/* Zmodem */
#if 0
			tryzhdrtype=ZRINIT;
			while(1) {
				Txhdr[ZF0]=(CANFC32|CANFDX|CANOVIO|CANRLE);
				/* add CANBRK if we can send break signal */
				if(zmode&CTRL_ESC)
					Txhdr[ZF0]|=TESCCTL;
				Txhdr[ZF1]=CANVHDR;
				Txhdr[ZP0]=0;
				Txhdr[ZP1]=0;
				putzhhdr(tryzhdrtype);
				done=0;
				while(!done) {
					done=1;
					switch(getzhdr()) {
						case ZRQINIT:
							if(Rxhdr[ZF3]&0x80)
								zmode|=VAR_HDRS;   /* we can var header */
							break;
						case ZFILE:
							zconv=Rxhdr[ZF0];
							zmanag=Rxhdr[ZF1];
							ztrans=Rxhdr[ZF2];
							if(Rxhdr[ZF3]&ZCANVHDR)
								zmode|=VAR_HDRS;
							tryzhdrtype=ZRINIT;
							if(getzdata(block, 1024)==GOTCRCW) {
								/* something */
								done=1; }
							putzhhdr(ZNAK);
							done=0;
							break;
						case ZSINIT:
							if(Rxhdr[ZF0]&TESCCTL)
								zmode|=CTRL_ESC;
							if (getzdata(attn,ZATTNLEN)==GOTCRCW) {
								ltohdr(1L);
								putzhhdr(ZACK); }
							else
								putzhhdr(ZNAK);
							done=0;
							break;
						case ZFREECNT:
							ltohdr(0);			/* should be free disk space */
							putzhhdr(ZACK);
							done=0;
							break;
						case ZCOMMAND:
/***
							cmdzack1flg = Rxhdr[ZF0];
							if(getzdata(block,1024)==GOTCRCW) {
								if (cmdzack1flg & ZCACK1)
									ltohdr(0L);
								else
									ltohdr((long)sys2(block));
								purgeline();	/* dump impatient questions */
								do {
									zshhdr(4,ZCOMPL, Txhdr);
								}
								while (++errors<20 && zgethdr(Rxhdr,1)!=ZFIN);
								ackbibi();
								if (cmdzack1flg & ZCACK1)
									exec2(block);
								return ZCOMPL;
							}
***/
							putzhhdr(ZNAK);
							done=0;
							break;
						case ZCOMPL:
							done=0;
							break;
						case ZFIN:
							ackbibi();
							return ZCOMPL;
						case ZCAN:
							return ERROR; } }
#endif
			}

		fnum++;

		if(!(mode&DIR) && fnames && fnum>fnames) {
			newline();
			lprintf("Attempt to send more files than specified\r\n");
			cancel();
			break; }

		k=strlen(str);	/* strip out control characters and high ASCII */
		for(i=j=0;i<k;i++)
			if(str[i]>SP && (uchar)str[i]<0x7f)
				str[j++]=str[i];
		str[j]=0;
		strupr(str);
		if(fexist(str) && !(mode&OVERWRITE)) {
			lprintf("%s already exists\r\n",str);
			cancel();
			bail(1); }
		if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1
			|| (stream=fdopen(file,"wb"))==NULL) {
			lprintf("Error creating %s\r\n",str);
			cancel();
			bail(1); }
		setvbuf(stream,NULL,_IOFBF,8*1024);
		startfile=time(NULL);
		lprintf("Receiving %s (%lu bytes) via %s %s\r\n"
			,str
			,mode&XMODEM ? 0 : file_bytes
			,mode&XMODEM ? "Xmodem" : mode&YMODEM ? mode&GMODE ? "Ymodem-G"
			: "Ymodem" :"Zmodem"
			,mode&CRC ? "CRC-16":"Checksum");

		errors=0;
		block_num=0;
		if(mode&GMODE)		/* G for Ymodem-G */
			putcom('G');
		else if(mode&CRC)	/* C for CRC */
			putcom('C');
		else				/* NAK for checksum */
			putcom(NAK);
		while(errors<MAXERRORS) {
			if(block_num && !(mode&GMODE))
				putcom(ACK);
			i=get_block(0); 	/* returns 0 if block okay */
			if(i==EOT)			/* end of transfer */
				break;
			if(i) { 			/* other error */
				cancel();
				bail(1); }
			if(file_bytes_left<=0L)  { /* No more bytes to send */
				newline();
				lputs("Attempt to send more than header specified\r\n");
				break; }
			if(hdr_block_num==((block_num+1)&0xff)) {	/* correct block */
				block_num++;
				if(file_bytes_left<block_size) {
					if(fwrite(block,1,file_bytes_left,stream)
						!=file_bytes_left) {
						newline();
						lprintf("Error writing to file\r\n");
						cancel();
						bail(1); } }
				else {
					if(fwrite(block,1,block_size,stream)
						!=block_size) {
						newline();
						lprintf("Error writing to file\r\n");
						cancel();
						bail(1); } }
				file_bytes_left-=block_size; }
			else {
				newline();
				lprintf("Block number %u instead of %u\r\n"
					,hdr_block_num,(block_num+1)&0xff);
				// dump_block();
				errors++; }
			t=time(NULL)-startfile;
			if(!t) t=1;
			cps=(uint)((block_num*(long)block_size)/t); 	/* cps so far */
			if(!cps) cps=1;
			l=file_bytes/cps;  /* total transfer est time */
			l-=t;				/* now, it's est time left */
            if(l<0) l=0;
			b=blocks(file_bytes);
			if(mode&YMODEM)
				lprintf("\rBlock (%lu%s): %lu/%lu  Byte: %lu  Time: %lu:%02lu  "
					"Left: %lu:%02lu  CPS: %u  %lu%% "
					,block_size%1024L ? block_size: block_size/1024L
					,block_size%1024L ? "" : "k"
					,block_num
					,b
					,block_num*(long)block_size
					,t/60L
					,t%60L
					,l/60L
					,l%60L
					,cps
					,(long)(((float)block_num/(float)b)*100.0)
					);
			else	/* Xmodem */
				lprintf("\rBlock (%lu%s): %lu  Byte: %lu  Time: %lu:%02lu  "
					"CPS: %u "
					,block_size%1024L ? block_size: block_size/1024L
					,block_size%1024L ? "" : "k"
					,block_num
					,block_num*(long)block_size
					,t/60L
					,t%60L
					,cps
                    );
		}

		putcom(ACK);
		if(!(mode&XMODEM) && ftime) {
			unixtodos(ftime,&dosdate,&dostime);
			if(dosdate.da_year>=1980) {
				ft.ft_min=dostime.ti_min;
				ft.ft_hour=dostime.ti_hour;
				ft.ft_tsec=dostime.ti_sec/2;
				ft.ft_year=dosdate.da_year-1980;
				ft.ft_day=dosdate.da_day;
				ft.ft_month=dosdate.da_mon;
				setftime(file,&ft); } }
		fclose(stream);
		t=time(NULL)-startfile;
		if(!t) t=1;
		l=(block_num-1)*(long)block_size;
		if(l>file_bytes)
			l=file_bytes;
		newline();
		lprintf("Successsful - Time: %lu:%02lu  CPS: %lu\r\n"
			,t/60,t%60,l/t);
		if(log) {
			fprintf(log,"%c %6lu %5u bps %4lu cps %3u errors %5u %4u "
				"%s %d\r\n"
				,mode&ZMODEM ? 'Z' : 'R'
				,l
				,baud
				,l/t
				,errors
				,flows
				,block_size
				,str
				,serial_num); }
		if(mode&XMODEM)
			break;
		total_files--;
		total_bytes-=file_bytes;
		if(total_files>1 && total_bytes)
			lprintf("Remaining - Time: %lu:%02lu  Files: %u  Bytes: %lu\r\n"
				,(total_bytes/cps)/60
				,(total_bytes/cps)%60
				,total_files
				,total_bytes
                );
		}
	bail(0); }

/********/
/* SEND */
/********/

/****************************************************/
/* Search through all to find total files and bytes */
/****************************************************/
for(fnum=0;fnum<fnames;fnum++) {
	last=findfirst(fname[fnum],&ff,0);	/* incase wildcards are used */
	if(last)
		lprintf("%s not found\r\n",fname[fnum]);
	while(!last) {
		total_files++;
		total_bytes+=ff.ff_fsize;
		last=findnext(&ff); } }

if(fnames>1)
	lprintf("Sending %u files (%lu bytes total)\r\n"
		,total_files,total_bytes);

rioctl(IOFB);

/***********************************************/
/* Send every file matching names or filespecs */
/***********************************************/
for(fnum=0;fnum<fnames;fnum++) {
	last=findfirst(fname[fnum],&ff,0);
	while(!last) {
		if(mode&ZMODEM) {
#if 0
			putcom('r'); putcom('z'); putcom(CR);       /* send rz\r */
			ltohdr(0L); 	/* Zero the header */
			putzhhdr(ZRQINIT);
			getzrxinit();
#endif
			} /* Zmodem */
		else {
			mode&=~GMODE;
			flows=0;
			for(errors=can=0;errors<MAXERRORS;errors++) {
				i=getcom(10);
				if(can && i!=CAN)
					can=0;
				if(i==NAK) {		/* csum */
					mode&=~CRC;
					break; }
				if(i=='C') {
					mode|=CRC;
					break; }
				if(i=='G') {
					mode|=(GMODE|CRC);
					break; }
				if(i==CAN) {
					if(can) {
						newline();
						lprintf("Cancelled remotely\r\n");
						bail(1); }
					can=1; }
				rioctl(IOFB);	/* flush buffers cause we have crap-o-la */
				if(i!=NOINP) {
					newline();
					lprintf("Received %s  Expected NAK, C, or G\r\n"
						,chr(i)); } }
			if(errors==MAXERRORS) {
				lprintf("Timeout starting transfer\r\n");
				cancel();
				bail(1); } } /* X/Ymodem */

		strcpy(str,fname[fnum]);
		if(strchr(str,'*') || strchr(str,'?')) {    /* wildcards used */
			p=strrchr(str,'\\');
			if(!p)
				p=strchr(str,':');
			if(p)
				*(p+1)=NULL;
			else
				str[0]=0;
			strcat(str,ff.ff_name); }

		lprintf("Sending %s (%lu bytes) via %s %s\r\n"
			,str,ff.ff_fsize
			,mode&XMODEM ? "Xmodem" : mode&YMODEM ? mode&GMODE ? "Ymodem-G"
			: "Ymodem" :"Zmodem"
            ,mode&CRC ? "CRC-16":"Checksum");

		if((file=nopen(str,O_RDONLY))==-1
			|| (stream=fdopen(file,"rb"))==NULL) {
			lprintf("Error opening %s for read\r\n",str);
			cancel();
			bail(1); }
		setvbuf(stream,NULL,_IOFBF,8*1024);
		rioctl(IOFB);	/* flush buffers cause extra 'G', 'C', or NAKs */
		if(!(mode&XMODEM)) {
			getftime(file,&ft);
			dostime.ti_min=ft.ft_min;
			dostime.ti_hour=ft.ft_hour;
			dostime.ti_hund=0;
			dostime.ti_sec=ft.ft_tsec*2;
			dosdate.da_year=1980+ft.ft_year;
			dosdate.da_day=ft.ft_day;
			dosdate.da_mon=ft.ft_month;
			t=dostounix(&dosdate,&dostime);
			memset(block,NULL,128);
			strcpy(block,ff.ff_name);
			strlwr(block);
			sprintf(block+strlen(block)+1,"%lu %lo 0 0 %d %ld"
				,ff.ff_fsize,t,total_files-sent_files,total_bytes-sent_bytes);
			/*
			lprintf("Sending Ymodem block '%s'\r\n",block+strlen(block)+1);
			*/
			block_num=0;
			i=block_size;
			block_size=128; 	/* Always use 128 for first block */
			for(errors=0;errors<MAXERRORS;errors++) {
				put_block();
				if(get_ack())
					break; }
			if(errors==MAXERRORS) {
				newline();
				lprintf("Failed to send header block\r\n");
				cancel();
				bail(1); }
			block_size=i;		/* Restore block size */
			mode&=~GMODE;
			for(errors=can=0;errors<MAXERRORS;errors++) {
				i=getcom(10);
				if(can && i!=CAN)
					can=0;
				if(i==NAK) {		/* csum */
					mode&=~CRC;
					break; }
				if(i=='C') {
					mode|=CRC;
					break; }
				if(i=='G') {
					mode|=(GMODE|CRC);
					break; }
				if(i==CAN) {
					if(can) {
						newline();
						lprintf("Cancelled remotely\r\n");
						bail(1); }
					can=1; }
				rioctl(IOFB);
				if(i!=NOINP) {
					newline();
					lprintf("Received %s  Expected NAK, C, or G\r\n"
						,chr(i)); } }
			if(errors==MAXERRORS) {
				newline();
				lprintf("Too many errors waiting for receiver\r\n");
				cancel();
				bail(1); } }
		last_block_num=block_num=1;
		startfile=time(NULL);
		errors=0;
		while((block_num-1)*(long)block_size<ff.ff_fsize && errors<MAXERRORS) {
			if(last_block_num==block_num) {  /* block_num didn't increment */
				fseek(stream,(block_num-1)*(long)block_size,SEEK_SET);
				i=fread(block,1,block_size,stream);
				while(i<block_size)
					block[i++]=CPMEOF; }
			last_block_num=block_num;
			put_block();
			i=fread(block,1,block_size,stream); /* read next block from disk */
			while(i<block_size)
				block[i++]=CPMEOF;
			t=time(NULL)-startfile;
			if(!t) t=1; 		/* t is time so far */
			cps=(uint)((block_num*(long)block_size)/t); 	/* cps so far */
			if(!cps) cps=1;
			l=ff.ff_fsize/cps;	/* total transfer est time */
			l-=t;				/* now, it's est time left */
			if(l<0) l=0;
			b=blocks(ff.ff_fsize);
			lprintf("\rBlock (%lu%s): %lu/%lu  Byte: %lu  "
				"Time: %lu:%02lu  Left: %lu:%02lu  CPS: %u  %lu%% "
				,block_size%1024L ? block_size: block_size/1024L
				,block_size%1024L ? "" : "k"
				,block_num
				,b
				,block_num*(long)block_size
				,t/60L
				,t%60L
				,l/60L
				,l%60L
				,cps
				,(long)(((float)block_num/(float)b)*100.0)
				);
			if(!get_ack())
				errors++;
			else
				block_num++; }
		fclose(stream);
		success=0;
		if((block_num-1)*(long)block_size>=ff.ff_fsize) {
			sent_files++;
			sent_bytes+=ff.ff_fsize;
			riosync();
			lprintf("\n");

			for(i=0;i<10;i++) {
				lprintf("\rSending EOT (%d)",i+1);
				rioctl(IOFI);
				putcom(EOT);
				ch=getcom(10);
				if(ch==ACK)
					break;
				if(ch==NAK && i==0 && (mode&(YMODEM|GMODE))==YMODEM)
					continue;  /* chuck's double EOT trick so don't complain */
				if(ch!=NOINP) {
					newline();
					lprintf("Received %s  Expected ACK\r\n"
						,chr(ch)); } }
			if(i==3)
				lprintf("\rNo ACK on EOT   \n");
			t=time(NULL)-startfile;
			if(!t) t=1;
			lprintf("\rSuccesssful - Time: %lu:%02lu  CPS: %lu\r\n"
				,t/60,t%60,ff.ff_fsize/t);
			success=1; }
		else {
			newline();
			lputs("Unsuccessful!\r\n");
			t=time(NULL)-startfile;
			if(!t) t=1; }

		if(total_files>1 && total_files-sent_files>1)
			lprintf("Remaining - Time: %lu:%02lu  Files: %u  Bytes: %lu\r\n"
				,((total_bytes-sent_bytes)/cps)/60
				,((total_bytes-sent_bytes)/cps)%60
				,total_files-sent_files
				,total_bytes-sent_bytes
				);
		if(log) {
			l=(block_num-1)*(long)block_size;
			if(l>ff.ff_fsize)
				l=ff.ff_fsize;
			fprintf(log,"%c %6lu %5u bps %4lu cps %3u errors %5u %4u "
				"%s -1\r\n"
				,success ? (mode&ZMODEM ? 'z':'S') : 'E'
				,l
				,baud
				,l/t
				,errors
				,flows
				,block_size
				,fname[fnum]); }

		last=findnext(&ff); } }
if(mode&XMODEM)
	bail(0);
mode&=~GMODE;
i=getcom(10);
if(i==NAK)
	mode&=~CRC;
else if(i=='C')
	mode|=CRC;
else if(i=='G')
	mode|=(GMODE|CRC);
if(i!=NOINP && i!=NAK && i!='C' && i!='G') {
	newline();
	lprintf("Received %s  Expected NAK, C, or G\r\n",chr(i)); }
else if(i!=NOINP) {
	block_num=0;
	block[0]=0;
	block_size=128;
	put_block();
	if(!get_ack()) {
		newline();
		lprintf("Failed to receive ACK after terminating block\r\n"); } }
if(total_files>1) {
	t=time(NULL)-startall;
	if(!t) t=1;
	newline();
	lprintf("Overall - Time %02lu:%02lu  Bytes: %lu  CPS: %lu\r\n"
		,t/60,t%60,sent_bytes,sent_bytes/t); }
bail(0);
return(0);
}

