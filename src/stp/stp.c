/* STP.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Synchronet Transfer Protocols */

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "conwrap.h"
#include "genwrap.h"
#include "dirwrap.h"
#include "filewrap.h"
#include "sockwrap.h"

#include "crc16.h"

#define STP 1

#include "zmodem.h"

#define MAXERRORS	10

#if 0

#define ERROR -1
							/* Various Character/Ctrl-Code Definitions	*/
#define SP			32		/* Space bar								*/
#define ESC			27		/* ESC Char 								*/
#define	CR			13		/* Carriage Return							*/
#define FF          12      /* Form Feed								*/
#define	LF			10		/* Line Feed								*/
#define TAB			9		/* Horizontal Tabulation					*/
#define BS			8		/* Back Space								*/
#define STX         2		/* Start of text                            */
#define ETX         3       /* End of text                              */

#endif

#define DLE 		CTRL_P	/* Data link escape 						*/
#define XON 		CTRL_Q	/* Resume transmission 						*/
#define XOFF		CTRL_S	/* Pause transmission						*/

#define SOH 		CTRL_A	/* Start of header							*/
#define EOT 		CTRL_D	/* End of transmission						*/

#define ACK 		CTRL_F	/* Acknowledge								*/
#define NAK 		CTRL_U	/* Negative Acknowledge 					*/
#define CAN 		CTRL_X	/* Cancel									*/

#define CPMEOF		CTRL_Z	/* CP/M End of file (^Z)					*/
#define LOC_ABORT	0x2e03	/* Local abort key	(^C)					*/

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

void cancel(void);

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

FILE*	errfp;
FILE*	statfp;

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

SOCKET	sock;
#define DCDHIGH		socket_check(sock, NULL, NULL, 0)

#ifdef _WINSOCKAPI_

WSADATA WSAData;
static BOOL WSAInitialized=FALSE;

static BOOL winsock_startup(void)
{
	int		status;             /* Status Code */

    if((status = WSAStartup(MAKEWORD(1,1), &WSAData))==0) {
		fprintf(statfp,"%s %s\n",WSAData.szDescription, WSAData.szSystemStatus);
		WSAInitialized=TRUE;
		return(TRUE);
	}

    fprintf(errfp,"!WinSock startup ERROR %d\n", status);
	return(FALSE);
}

#else /* No WINSOCK */

#define winsock_startup()	(TRUE)	

#endif

/********/
/* Code */
/********/

void newline(void)
{
#if 0
	if(lclwx()>1)
		fprintf(statfp,"\n");
#endif
}

int incom(void);
int outcom(char);


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
		BEEP(2000,500);
		BEEP(1000,500);
	}
	newline();
	fprintf(statfp,"Exiting - Error level: %d  Flow restraint count: %u\n",code,flows);
	fcloseall();

#ifdef _WINSOCKAPI_
	if(WSAInitialized && WSACleanup()!=0) 
		fprintf(statfp,"!WSACleanup ERROR %d\n",ERROR_VALUE);
#endif

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
			fprintf(statfp,"No carrier\n");
			bail(1); 
		}
		YIELD();
		if(!(mode&NO_LOCAL) && kbhit()==LOC_ABORT) {
			newline();
			fprintf(statfp,"Local abort\n");
			cancel();
			bail(1); 
		} 
	}
	newline();
	fprintf(statfp,"Input timeout\n");
	return(NOINP);
}

/**********************************/
/* Output a character to COM port */
/**********************************/
void putcom(uchar ch)
{
	int i=0;

	while(outcom(ch)&TXBOF && i<180) { /* 10 sec delay */
		if(!i) fprintf(statfp,"F");
		if(!DCDHIGH) {
			newline();
			fprintf(statfp,"No carrier\n");
			bail(1); 
		}
		i++;
		YIELD();
		if(!(mode&NO_LOCAL) && kbhit()==LOC_ABORT) {
			newline();
			fprintf(statfp,"Local abort\n");
			bail(1); 
		} 
	}
	if(i) {
		fprintf(statfp,"\b \b");
		flows++;
		if(i==180) {
			newline();
			fprintf(statfp,"Output timeout\n");
			bail(1); 
		} 
	}
}

void put_nak(void)
{
	while(getcom(1)!=NOINP && (mode&NO_LOCAL || kbhit()!=LOC_ABORT))
		;				/* wait for any trailing data */
	putcom(NAK);
}

void cancel(void)
{
	int i;

	for(i=0;i<10;i++)
		putcom(CAN);
	for(i=0;i<10;i++)
		putcom(BS);
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
			return(str); 
	}
}

/****************************************************************************/
/* Receive a X/Y/Zmodem block (sector) from COM port						*/
/* hdrblock is 1 if attempting to get Ymodem header block, 0 if data block	*/
/* Returns 0 if all went well, -1 on error or CAN, and EOT if EOT			*/
/****************************************************************************/
int get_block(int hdrblock)
{
	uchar chksum,calc_chksum;
	int i,errors,eot=0,can=0;
	uint b;
	ushort crc,calc_crc;

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
					continue; 
				}
				return(EOT);
			case CAN:
				newline();
				if(!can) {			/* must get two CANs in a row */
					can=1;
					fprintf(statfp,"Received CAN  Expected SOH, STX, or EOT\n");
					continue; 
				}
				fprintf(statfp,"Cancelled remotely\n");
				return(-1);
			case NOINP: 	/* Nothing came in */
				continue;
			default:
				newline();
				fprintf(statfp,"Received %s  Expected SOH, STX, or EOT\n",chr(i));
				if(hdrblock)  /* Trying to get Ymodem header block */
					return(-1);
				put_nak();
				continue; 
		}
		i=getcom(1);
		if(i==NOINP) {
			put_nak();
			continue; 
		}
		hdr_block_num=i;
		i=getcom(1);
		if(i==NOINP) {
			put_nak();
			continue; 
		}
		if(hdr_block_num!=(uchar)~i) {
			newline();
			fprintf(statfp,"Block number error\n");
			put_nak();
			continue; 
		}
		calc_crc=calc_chksum=0;
		for(b=0;b<block_size;b++) {
			i=getcom(1);
			if(i==NOINP)
				break;
			block[b]=i;
			if(mode&CRC)
				calc_crc=ucrc16(block[b],calc_crc);
			else
				calc_chksum+=block[b]; 
		}

		if(b<block_size) {
			put_nak();
			continue; 
		}

		if(mode&CRC) {
			crc=getcom(1)<<8;
			crc|=getcom(1); 
		}
		else
			chksum=getcom(1);

		if(mode&CRC) {
			calc_crc=ucrc16(0,calc_crc);
			calc_crc=ucrc16(0,calc_crc);
			if(crc==calc_crc)
				break;
			newline();
			fprintf(statfp,"CRC error\n"); 
		}

		else {	/* CHKSUM */
			if(chksum==calc_chksum)
				break;
			newline();
			fprintf(statfp,"Checksum error\n"); 
		}

		if(mode&GMODE) {	/* Don't bother sending a NAK. He's not listening */
			cancel();
			bail(1); 
		}
		put_nak(); 
	}

	if(errors>=MAXERRORS) {
		newline();
		fprintf(statfp,"Too many errors\n");
		return(-1); 
	}
	return(0);
}

/*****************/
/* Sends a block */
/*****************/
void put_block(void)
{
	uchar	ch,chksum;
    uint	i;
	ushort	crc;

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
			crc=ucrc16(block[i],crc);
		else
			chksum+=block[i]; 
	}

	if(mode&CRC) {
		crc=ucrc16(0,crc);
		crc=ucrc16(0,crc);
		putcom(crc>>8);
		putcom(crc&0xff); 
	}
	else
		putcom(chksum);
}

/************************************************************/
/* Gets an acknowledgement - usually after sending a block	*/
/* Returns 1 if ack received, 0 otherwise.					*/
/************************************************************/
int get_ack(void)
{
	int i,errors,can=0;

	for(errors=0;errors<MAXERRORS;errors++) {

		if(mode&GMODE) {		/* Don't wait for ACK on Ymodem-G */
			if(incom()==CAN) {
				newline();
				fprintf(statfp,"Cancelled remotely\n");
				cancel();
				bail(1); 
			}
			return(1); 
		}

		i=getcom(10);
		if(can && i!=CAN)
			can=0;
		if(i==ACK)
			return(1);
		if(i==CAN) {
			if(can) {
				newline();
				fprintf(statfp,"Cancelled remotely\n");
				cancel();
				bail(1); 
			}
			can=1; 
		}
		if(i!=NOINP) {
			newline();
			fprintf(statfp,"Received %s  Expected ACK\n",chr(i));
			if(i!=CAN)
				return(0); 
		} 
	}

	return(0);
}

/****************************************************************************/
/* Syncronizes the remote and local machines								*/
/****************************************************************************/
void riosync(void)
{
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
	uint i;
	ushort crc=0;

	putcom(ZPAD);
	putcom(ZPAD);
	putcom(ZDLE);
	if(zmode&VAR_HDRS) {
		putcom(ZVHEX);
		putzhex(4); 
	}
	else
		putcom(ZHEX);
	putzhex(type);
	crc=ucrc16(type,crc);
	for(i=0;i<4;i++) {
		putzhex(Txhdr[i]);
		crc=ucrc16(Txhdr[i],crc); 
	}
	crc=ucrc16(0,crc);
	crc=ucrc16(0,crc);
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
					putcom(lastsent=ch); 
				}
				break;
			default:
				if(zmode&CTRL_ESC && !(ch&0x60)) {  /* it's a ctrl char */
					putcom(ZDLE);
					ch^=0x40; 
				}
				putcom(lastsent=ch);
				break; 
		}
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
		return(i); 
	}

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
				break; 
		}
		break; 
	}
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
				return(i); 
		} 
	}
}

/************************************************/
/* Dump the current blockm contents - for debug */
/************************************************/
void dump_block()
{
	uint l;

	for(l=0;l<block_size;l++)
		fprintf(statfp,"%02X  ",block[l]);
	fprintf(statfp,"\n");
}

static const char* usage=
	"usage: STP <socket> [opts] <cmd> [file | path | +list]\n\n"
	"where:\n\n"
	"socket = TCP socket descriptor\n"
	"opts   = o  to overwrite files when receiving\n"
	"         d  to disable dropped carrier detection\n"
	"         a  to sound alarm at start and stop of transfer\n"
	"         l  to disable local keyboard (Ctrl-C) checking\n"
	"cmd    = sx to send Xmodem     rx to recv Xmodem\n"
	"         sX to send Xmodem-1k  rc to recv Xmodem-CRC\n"
	"         sy to send Ymodem     ry to recv Ymodem\n"
	"         sY to send Ymodem-1k  rg to recv Ymodem-G\n"
	"         sz to send Zmodem     rz to recv Zmodem\n"
	"file   = filename to send or receive\n"
	"path   = path to receive files into\n"
	"list   = name of text file with list of filenames to send or receive\n";

/***************/
/* Entry Point */
/***************/
int main(int argc, char **argv)
{
	char	str[256],tmp[256],tmp2[256],irq,errors,*p,*p2,first_block
			,*fname[MAX_FNAMES],fnames=0,fnum,success;
	int 	ch,i,j,k,last,total_files=0,sent_files=0,can;
	uint	file_bytes_left;
	uint	cps;
	long	b,l,m;
	long	serial_num;
	long	fsize;
	ulong	file_bytes=0,total_bytes=0,sent_bytes=0;
	ulong	val;
	time_t	t,startall,startfile,ftime;
	glob_t	g;
	int		gi;
	FILE*	fp;
	FILE*	log=NULL;

	errfp=stderr;
	statfp=stdout;

	fprintf(statfp,"\nSynchronet Transfer Protocols v1.00"
		"  Copyright 2003 Rob Swindell\n\n");

	fprintf(statfp,"Command line: ");
	for(i=1;i<argc;i++)
		fprintf(statfp,"%s ",argv[i]);
	fprintf(statfp,"\n",statfp);

	if(argc<3) {
		fprintf(errfp,usage);
		exit(1); 
	}

	sock=atoi(argv[1]);

	for(i=2;i<argc;i++) {

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
						fprintf(statfp,"Unrecognized command '%s'\n\n",argv[i]);
						fprintf(statfp,usage);
						exit(1); 
				} 
			}

			else if(toupper(argv[i][0])=='O')
				mode|=OVERWRITE;

			else if(toupper(argv[i][0])=='D')
				mode|=IGNORE_DCD;

			else if(toupper(argv[i][0])=='A')
				mode|=ALARM;

			else if(toupper(argv[i][0])=='L')
				mode|=NO_LOCAL;

			else if(argv[i][0]=='*')
				mode|=DEBUG; 
		}

		else if(argv[i][0]=='+') {
			if(mode&DIR) {
				fprintf(statfp,"Cannot specify both directory and filename\n");
				exit(1); 
			}
			sprintf(str,"%s",argv[i]+1);
			if((fp=fopen(str,"rb"))==NULL) {
				fprintf(statfp,"Error opening filelist %s\n",str);
				exit(1); 
			}
			while(!feof(fp) && !ferror(fp) && fnames<MAX_FNAMES) {
				if(!fgets(str,128,fp))
					break;
				truncsp(str);
				if((fname[fnames]=(char *)malloc(strlen(str)+1))==NULL) {
					fprintf(statfp,"Error allocating memory for filename\n");
					exit(1); 
				}
				strcpy(fname[fnames++],str); 
			}
			fclose(fp); 
		}

		else if(mode&(SEND|RECV)){
			if((fname[fnames]=(char *)malloc(128))==NULL) {
				fprintf(statfp,"Error allocating memory for filename\n");
				exit(1); 
			}
			strcpy(fname[fnames],argv[i]);
			if(isdir(fname[fnames])) { /* is a directory */
				if(mode&DIR) {
					fprintf(statfp,"Only one directory can be specified\n");
					exit(1); 
				}
				if(fnames) {
					fprintf(statfp,"Cannot specify both directory and filename\n");
					exit(1); 
				}
				if(mode&SEND) {
					fprintf(statfp,"Cannot send directory '%s'\n",fname[fnames]);
					exit(1);
				}
				mode|=DIR; 
			}
			fnames++; 
		} 
	}

	if(!(mode&(SEND|RECV))) {
		fprintf(statfp,"No command specified\n");
		fprintf(statfp,usage);
		exit(1); 
	}

	if(mode&(SEND|XMODEM) && !fnames) { /* Sending with any or recv w/Xmodem */
		fprintf(statfp,"Must specify filename or filelist\n");
		fprintf(statfp,usage);
		exit(1); 
	}


	if(mode&DIR)
		backslash(fname[0]);

	if(mode&ALARM) {
		BEEP(1000,500);
		BEEP(2000,500);
	}

	if(!winsock_startup())
		bail(2);

	/* Non-blocking socket I/O */
	val=1;
	ioctlsocket(sock,FIONBIO,&val);	

	if(!DCDHIGH) {
		newline();
		fprintf(statfp,"No carrier\n");
		bail(1); 
	}

	p=getenv("DSZLOG");
	if(p) {
		if((log=fopen(p,"wb"))==NULL) {
			fprintf(statfp,"Error opening DSZLOG file '%s'\n",p);
			bail(1); 
		}
		setvbuf(log,NULL,_IOFBF,16*1024); 
	}

	startall=time(NULL);

	if(mode&RECV) {
		if(fnames>1)
			fprintf(statfp,"Receiving %u files\n",fnames);
		fnum=0;
		while(1) {
			if(mode&XMODEM) {
				strcpy(str,fname[0]);
				file_bytes=file_bytes_left=0x7fffffff;
				serial_num=-1; 
			}

			else if(mode&YMODEM) {
				fprintf(statfp,"Fetching Ymodem header block\n");
				for(errors=0;errors<MAXERRORS;errors++) {
					if(errors>3 && mode&CRC && !(mode&GMODE))
						mode&=~CRC;
					if(mode&GMODE)		/* G for Ymodem-G */
						putcom('G');
					else if(mode&CRC)	/* C for CRC */
						putcom('C');
					else				/* NAK for checksum */
						putcom(NAK);
#if 0
					for(i=60;i;i--) {
						if(rioctl(RXBC))	/* no chars in-bound */
							break;
						SLEEP(100); 		/* so wait */
					}
					if(!i) {				/* none after 6 secs */
						if(errors)
							fprintf(statfp,"Ymodem header timeout (%d)\n",errors);
						continue; 
					}
#endif
					if(!get_block(1)) { 	 /* block received successfully */
						putcom(ACK);
						break; 
					} 
				}
				if(errors==MAXERRORS) {
					fprintf(statfp,"Error fetching Ymodem header block\n");
					cancel();
					bail(1); 
				}
				if(!block[0]) {
					fprintf(statfp,"Received Ymodem termination block\n");
					bail(0); 
				}
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
				fprintf(statfp,"Incoming filename: %.64s ",block);
				if(mode&DIR)
					sprintf(str,"%s%s",fname[0],getfname(block));
				else {
					getfname(block);
					for(i=0;i<fnames;i++) {
						if(!fname[i][0])	/* name blank or already used */
							continue;
						getfname(fname[i]);
						if(!stricmp(tmp,str)) {
							strcpy(str,fname[i]);
							fname[i][0]=0;
							break; 
						} 
					}
					if(i==fnames) { 				/* Not found in list */
						if(fnames)
							fprintf(statfp," - Not in receive list!");
						if(!fnames || fnum>=fnames || !fname[fnum][0])
							getfname(block);	/* worst case */
						else {
							strcpy(str,fname[fnum]);
							fname[fnum][0]=0; 
						} 
					} 
				}
				fprintf(statfp,"\n"); 
			}

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
									done=1; 
								}
								putzhhdr(ZNAK);
								done=0;
								break;
							case ZSINIT:
								if(Rxhdr[ZF0]&TESCCTL)
									zmode|=CTRL_ESC;
								if (getzdata(attn,ZATTNLEN)==GOTCRCW) {
									ltohdr(1L);
									putzhhdr(ZACK); 
								}
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
								return ERROR; 
					} 
				}
	#endif
			}

			fnum++;

			if(!(mode&DIR) && fnames && fnum>fnames) {
				newline();
				fprintf(statfp,"Attempt to send more files than specified\n");
				cancel();
				break; 
			}

			k=strlen(str);	/* strip out control characters and high ASCII */
			for(i=j=0;i<k;i++)
				if(str[i]>SP && (uchar)str[i]<0x7f)
					str[j++]=str[i];
			str[j]=0;
			if(fexist(str) && !(mode&OVERWRITE)) {
				fprintf(statfp,"%s already exists\n",str);
				cancel();
				bail(1); 
			}
			if((fp=fopen(str,"wb"))==NULL) {
				fprintf(statfp,"Error creating %s\n",str);
				cancel();
				bail(1); 
			}
			setvbuf(fp,NULL,_IOFBF,8*1024);
			startfile=time(NULL);
			fprintf(statfp,"Receiving %s (%lu bytes) via %s %s\n"
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
					bail(1); 
				}
				if(file_bytes_left<=0L)  { /* No more bytes to send */
					newline();
					fprintf(statfp,"Attempt to send more than header specified\n");
					break; 
				}
				if(hdr_block_num==(uchar)((block_num+1)&0xff)) {	/* correct block */
					block_num++;
					if(file_bytes_left<block_size) {
						if(fwrite(block,1,file_bytes_left,fp)
							!=file_bytes_left) {
							newline();
							fprintf(statfp,"Error writing to file\n");
							cancel();
							bail(1); 
						} 
					}
					else {
						if(fwrite(block,1,block_size,fp)
							!=block_size) {
							newline();
							fprintf(statfp,"Error writing to file\n");
							cancel();
							bail(1); 
						} 
					}
					file_bytes_left-=block_size; 
				}
				else {
					newline();
					fprintf(statfp,"Block number %u instead of %u\n"
						,hdr_block_num,(block_num+1)&0xff);
					// dump_block();
					errors++; 
				}
				t=time(NULL)-startfile;
				if(!t) t=1;
				cps=(uint)((block_num*(long)block_size)/t); 	/* cps so far */
				if(!cps) cps=1;
				l=file_bytes/cps;  /* total transfer est time */
				l-=t;				/* now, it's est time left */
				if(l<0) l=0;
				b=blocks(file_bytes);
				if(mode&YMODEM)
					fprintf(statfp,"\rBlock (%lu%s): %lu/%lu  Byte: %lu  Time: %lu:%02lu  "
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
					fprintf(statfp,"\rBlock (%lu%s): %lu  Byte: %lu  Time: %lu:%02lu  "
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
			if(!(mode&XMODEM) && ftime)
				setfdate(str,ftime); 
			fclose(fp);
			t=time(NULL)-startfile;
			if(!t) t=1;
			l=(block_num-1)*(long)block_size;
			if(l>file_bytes)
				l=file_bytes;
			newline();
			fprintf(statfp,"Successsful - Time: %lu:%02lu  CPS: %lu\n"
				,t/60,t%60,l/t);
			if(log) {
				fprintf(log,"%c %6lu %5u bps %4lu cps %3u errors %5u %4u "
					"%s %d\n"
					,mode&ZMODEM ? 'Z' : 'R'
					,l
					,30000 /* baud */
					,l/t
					,errors
					,flows
					,block_size
					,str
					,serial_num); 
			}
			if(mode&XMODEM)
				break;
			total_files--;
			total_bytes-=file_bytes;
			if(total_files>1 && total_bytes)
				fprintf(statfp,"Remaining - Time: %lu:%02lu  Files: %u  Bytes: %lu\n"
					,(total_bytes/cps)/60
					,(total_bytes/cps)%60
					,total_files
					,total_bytes
					);
			}
		bail(0); 
	}

	/********/
	/* SEND */
	/********/

	/****************************************************/
	/* Search through all to find total files and bytes */
	/****************************************************/
	for(fnum=0;fnum<fnames;fnum++) {
		if(glob(fname[fnum],0,NULL,&g)) {
			fprintf(statfp,"%s not found\n",fname[fnum]);
			continue;
		}
		for(i=0;i<(int)g.gl_pathc;i++) {
			if(isdir(g.gl_pathv[i]))
				continue;
			total_files++;
			total_bytes+=flength(g.gl_pathv[i]);
		} 
		globfree(&g);
	}

	if(fnames>1)
		fprintf(statfp,"Sending %u files (%lu bytes total)\n"
			,total_files,total_bytes);

#if 0
	rioctl(IOFB);
#endif
	/***********************************************/
	/* Send every file matching names or filespecs */
	/***********************************************/
	for(fnum=0;fnum<fnames;fnum++) {
		if(glob(fname[fnum],0,NULL,&g)) {
			fprintf(statfp,"%s not found\n",fname[fnum]);
			continue;
		}
		for(gi=0;gi<(int)g.gl_pathc;gi++) {
			if(isdir(g.gl_pathv[gi]))
				continue;
			fsize=flength(g.gl_pathv[gi]);
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
						break; 
					}
					if(i=='C') {
						mode|=CRC;
						break; 
					}
					if(i=='G') {
						mode|=(GMODE|CRC);
						break; 
					}
					if(i==CAN) {
						if(can) {
							newline();
							fprintf(statfp,"Cancelled remotely\n");
							bail(1); 
						}
						can=1; 
					}
#if 0
					rioctl(IOFB);	/* flush buffers cause we have crap-o-la */
#endif
					if(i!=NOINP) {
						newline();
						fprintf(statfp,"Received %s  Expected NAK, C, or G\n"
							,chr(i)); 
					} 
				}
				if(errors==MAXERRORS) {
					fprintf(statfp,"Timeout starting transfer\n");
					cancel();
					bail(1); 
				} 
			} /* X/Ymodem */

			strcpy(str,fname[fnum]);
			if(strchr(str,'*') || strchr(str,'?')) {    /* wildcards used */
				p=strrchr(str,'\\');
				if(!p)
					p=strchr(str,':');
				if(p)
					*(p+1)=NULL;
				else
					str[0]=0;
				strcat(str,g.gl_pathv[gi]); 
			}

			fprintf(statfp,"Sending %s (%lu bytes) via %s %s\n"
				,str,fsize
				,mode&XMODEM ? "Xmodem" : mode&YMODEM ? mode&GMODE ? "Ymodem-G"
				: "Ymodem" :"Zmodem"
				,mode&CRC ? "CRC-16":"Checksum");

			if((fp=fopen(str,"rb"))==NULL) {
				fprintf(statfp,"Error opening %s for read\n",str);
				cancel();
				bail(1); 
			}
			setvbuf(fp,NULL,_IOFBF,8*1024);
#if 0
			rioctl(IOFB);	/* flush buffers cause extra 'G', 'C', or NAKs */
#endif
			if(!(mode&XMODEM)) {
				t=fdate(str);
				memset(block,NULL,128);
				strcpy(block,g.gl_pathv[gi]);
				sprintf(block+strlen(block)+1,"%lu %lo 0 0 %d %ld"
					,fsize,t,total_files-sent_files,total_bytes-sent_bytes);
				/*
				fprintf(statfp,"Sending Ymodem block '%s'\n",block+strlen(block)+1);
				*/
				block_num=0;
				i=block_size;
				block_size=128; 	/* Always use 128 for first block */
				for(errors=0;errors<MAXERRORS;errors++) {
					put_block();
					if(get_ack())
						break; 
				}
				if(errors==MAXERRORS) {
					newline();
					fprintf(statfp,"Failed to send header block\n");
					cancel();
					bail(1); 
				}
				block_size=i;		/* Restore block size */
				mode&=~GMODE;
				for(errors=can=0;errors<MAXERRORS;errors++) {
					i=getcom(10);
					if(can && i!=CAN)
						can=0;
					if(i==NAK) {		/* csum */
						mode&=~CRC;
						break; 
					}
					if(i=='C') {
						mode|=CRC;
						break; 
					}
					if(i=='G') {
						mode|=(GMODE|CRC);
						break; 
					}
					if(i==CAN) {
						if(can) {
							newline();
							fprintf(statfp,"Cancelled remotely\n");
							bail(1); 
						}
						can=1; 
					}
#if 0
					rioctl(IOFB);
#endif
					if(i!=NOINP) {
						newline();
						fprintf(statfp,"Received %s  Expected NAK, C, or G\n"
							,chr(i)); 
					} 
				}
				if(errors==MAXERRORS) {
					newline();
					fprintf(statfp,"Too many errors waiting for receiver\n");
					cancel();
					bail(1); 
				} 
			}
			last_block_num=block_num=1;
			startfile=time(NULL);
			errors=0;
			while((block_num-1)*(long)block_size<fsize && errors<MAXERRORS) {
				if(last_block_num==block_num) {  /* block_num didn't increment */
					fseek(fp,(block_num-1)*(long)block_size,SEEK_SET);
					i=fread(block,1,block_size,fp);
					while(i<block_size)
						block[i++]=CPMEOF; 
				}
				last_block_num=block_num;
				put_block();
				i=fread(block,1,block_size,fp); /* read next block from disk */
				while(i<block_size)
					block[i++]=CPMEOF;
				t=time(NULL)-startfile;
				if(!t) t=1; 		/* t is time so far */
				cps=(uint)((block_num*(long)block_size)/t); 	/* cps so far */
				if(!cps) cps=1;
				l=fsize/cps;	/* total transfer est time */
				l-=t;				/* now, it's est time left */
				if(l<0) l=0;
				b=blocks(fsize);
				fprintf(statfp,"\rBlock (%lu%s): %lu/%lu  Byte: %lu  "
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
					block_num++; 
			}
			fclose(fp);
			success=0;
			if((long)(block_num-1)*(long)block_size>=fsize) {
				sent_files++;
				sent_bytes+=fsize;
				riosync();
				fprintf(statfp,"\n");

				for(i=0;i<10;i++) {
					fprintf(statfp,"\rSending EOT (%d)",i+1);
#if 0
					rioctl(IOFI);
#endif
					putcom(EOT);
					ch=getcom(10);
					if(ch==ACK)
						break;
					if(ch==NAK && i==0 && (mode&(YMODEM|GMODE))==YMODEM)
						continue;  /* chuck's double EOT trick so don't complain */
					if(ch!=NOINP) {
						newline();
						fprintf(statfp,"Received %s  Expected ACK\n"
							,chr(ch)); 
					} 
				}
				if(i==3)
					fprintf(statfp,"\rNo ACK on EOT   \n");
				t=time(NULL)-startfile;
				if(!t) t=1;
				fprintf(statfp,"\rSuccesssful - Time: %lu:%02lu  CPS: %lu\n"
					,t/60,t%60,fsize/t);
				success=1; 
			}
			else {
				newline();
				fprintf(statfp,"Unsuccessful!\n");
				t=time(NULL)-startfile;
				if(!t) t=1; 
			}

			if(total_files>1 && total_files-sent_files>1)
				fprintf(statfp,"Remaining - Time: %lu:%02lu  Files: %u  Bytes: %lu\n"
					,((total_bytes-sent_bytes)/cps)/60
					,((total_bytes-sent_bytes)/cps)%60
					,total_files-sent_files
					,total_bytes-sent_bytes
					);
			if(log) {
				l=(block_num-1)*(long)block_size;
				if(l>fsize)
					l=fsize;
				fprintf(log,"%c %6lu %5u bps %4lu cps %3u errors %5u %4u "
					"%s -1\n"
					,success ? (mode&ZMODEM ? 'z':'S') : 'E'
					,l
					,30000 /* baud */
					,l/t
					,errors
					,flows
					,block_size
					,fname[fnum]); 
			}
		} 
	}
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
		fprintf(statfp,"Received %s  Expected NAK, C, or G\n",chr(i)); 
	}
	else if(i!=NOINP) {
		block_num=0;
		block[0]=0;
		block_size=128;
		put_block();
		if(!get_ack()) {
			newline();
			fprintf(statfp,"Failed to receive ACK after terminating block\n"); 
		} 
	}
	if(total_files>1) {
		t=time(NULL)-startall;
		if(!t) t=1;
		newline();
		fprintf(statfp,"Overall - Time %02lu:%02lu  Bytes: %lu  CPS: %lu\n"
			,t/60,t%60,sent_bytes,sent_bytes/t); 
	}
	bail(0);
	return(0);
}

