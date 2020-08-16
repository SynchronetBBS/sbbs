/* sexyz.c */

/* Synchronet External X/Y/ZMODEM Transfer Protocols */

/* $Id: sexyz.c,v 2.10 2020/03/31 07:14:58 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

/* 
 * ZMODEM code based on zmtx/zmrx v1.02 (C) Mattheij Computer Service 1994
 * by Jacques Mattheij
 *
 *	Date: Thu, 19 Nov 2015 10:10:02 +0100
 *	From: Jacques Mattheij
 *	Subject: Re: zmodem license
 *	To: Stephen Hurd, Fernando Toledo
 *	CC: Rob Swindell
 *
 *	Hello there to all of you,
 *
 *	So, this email will then signify as the transfer of any and all rights I
 *	held up to this point with relation to the copyright of the zmodem
 *	package as released by me many years ago and all associated files to
 *	Stephen Hurd. Fernando Toledo and Rob Swindell are named as
 *	witnesses to this transfer.
 *
 *	...
 *
 *	best regards,
 *
 *	Jacques Mattheij
 */

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>		/* isdigit */
#include <sys/stat.h>

#ifdef __unix__
	#include <termios.h>
	#include <signal.h>
#endif

/* xpdev */
#include "conwrap.h"
#include "genwrap.h"
#include "semwrap.h"
#include "dirwrap.h"
#include "filewrap.h"
#include "sockwrap.h"
#include "str_list.h"
#include "ini_file.h"
#include "eventwrap.h"
#include "threadwrap.h"

/* sbbs */
#include "ringbuf.h"
#include "telnet.h"
#include "nopen.h"

/* sexyz */
#include "sexyz.h"

#define SINGLE_THREADED		FALSE
#define MIN_OUTBUF_SIZE		1024
#define MAX_OUTBUF_SIZE		(64*1024)
#define INBUF_SIZE			(64*1024)
#define	MAX_FILE_SIZE		0			/* Default value for max recv file size, 0 = unlimited*/

/***************/
/* Global Vars */
/***************/
long	mode=0;							/* Program mode 					*/
long	zmode=0L;						/* Zmodem mode						*/
uchar	block[XMODEM_MAX_BLOCK_SIZE];	/* Block buffer 					*/
ulong	block_num;						/* Block number 					*/
char*	dszlog;
BOOL	dszlog_path=TRUE;				/* Log complete path to filename	*/
BOOL	dszlog_short=FALSE;				/* Log Micros~1 short filename		*/
BOOL	dszlog_quotes=FALSE;			/* Quote filenames in DSZLOG		*/
int		log_level=LOG_INFO;
BOOL	use_syslog=FALSE;
BOOL	lc_filenames=FALSE;
int64_t	max_file_size=MAX_FILE_SIZE;

xmodem_t xm;
zmodem_t zm;

FILE*	errfp;
FILE*	statfp;
FILE*	logfp=NULL;

char	revision[16];

SOCKET	sock=INVALID_SOCKET;

BOOL	telnet=TRUE;
#ifdef __unix__
BOOL	stdio=FALSE;
#endif
BOOL	terminate=FALSE;
BOOL	debug_tx=FALSE;
BOOL	debug_rx=FALSE;
BOOL	debug_telnet=FALSE;
BOOL	pause_on_exit=FALSE;
BOOL	pause_on_abend=FALSE;
BOOL	newline=TRUE;
BOOL	connected=TRUE;

time_t		progress_interval;

RingBuf		outbuf;
#if defined(RINGBUF_EVENT)
	#define		outbuf_empty outbuf.empty_event
#else
	xpevent_t	outbuf_empty;
#endif
unsigned	outbuf_drain_timeout;
long		outbuf_size;

uchar		inbuf[INBUF_SIZE];
unsigned	inbuf_pos=0;
unsigned	inbuf_len=0;

unsigned	flows=0;
unsigned	select_errors=0;

#ifdef _WINSOCKAPI_

/* Note: Don't call WSACleanup() or TCP session will close! */
WSADATA WSAData;	

static BOOL winsock_startup(void)
{
	int		status;             /* Status Code */

    if((status = WSAStartup(MAKEWORD(1,1), &WSAData))==0) {
		fprintf(statfp,"%s %s\n",WSAData.szDescription, WSAData.szSystemStatus);
		return(TRUE);
	}

    fprintf(errfp,"!WinSock startup ERROR %d\n", status);
	return(FALSE);
}

#else /* No WINSOCK */

#define winsock_startup()	(TRUE)	

#endif

static int lputs(void* unused, int level, const char* str)
{
	FILE*	fp=statfp;
	int		ret;

#if defined(_WIN32) && defined(_DEBUG)
	if(log_level==LOG_DEBUG) {
		char dbgstr[1024];
		SAFEPRINTF(dbgstr, "SEXYZ: %s", str);
		OutputDebugString(dbgstr);
	}
#endif

	if(level>log_level)
		return 0;

    if(level<LOG_NOTICE)
		fp=errfp;

	if(!newline) {
		fprintf(fp,"\n");
		newline=TRUE;
	}
	if(level<LOG_NOTICE)
		ret=fprintf(fp,"!%s\n",str);
	else
		ret=fprintf(fp,"%s\n",str);

#if defined(__unix__)
	if(use_syslog) {
		char*	msg;
		char*	p;
		if((msg=strdup(str))!=NULL) {
			REPLACE_CHARS(msg,'\r',' ',p);
			REPLACE_CHARS(msg,'\n',' ',p);
			syslog(level,"%s",msg);
			free(msg);
		}
	}
#endif

	return ret;
}

static int lprintf(int level, const char *fmt, ...)
{
	char sbuf[1024];
	va_list argptr;

	if(level>log_level)
		return 0;

    va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
    return(lputs(NULL,level,sbuf));
}

void break_handler(int type)
{
	lprintf(LOG_NOTICE,"-> Aborted Locally (signal: %d)",type);

	/* Flag to indicate local (as opposed to remote) abort */
	zm.local_abort=TRUE;

	/* Stop any transfers in progress immediately */
	xm.cancelled=TRUE;	
	zm.cancelled=TRUE;
}

#if defined(_WIN32)
BOOL WINAPI ControlHandler(unsigned long CtrlType)
{
	break_handler((int)CtrlType);
	return TRUE;
}
#endif

char* dszlog_filename(char* str)
{
	char*		p=str;
	static char	path[MAX_PATH+1];

#ifdef _WIN32
	char sfpath[MAX_PATH+1];
	if(dszlog_short) {
		SAFECOPY(sfpath,str);
		GetShortPathName(str,sfpath,sizeof(sfpath));
		p=sfpath;
	}
#endif

	if(!dszlog_path)
		p=getfname(p);

	if(!dszlog_quotes)
		return(p);

	SAFEPRINTF(path,"\"%s\"",p);
	return(path);
}

static char *chr(uchar ch)
{
	static char str[25];

	if(mode&ZMODEM) {
		switch(ch) {
			case ZRQINIT:	return("ZRQINIT");
			case ZRINIT:	return("ZRINIT");
			case ZSINIT:	return("ZSINIT");
			case ZACK:		return("ZACK");
			case ZFILE:		return("ZFILE");
			case ZSKIP:		return("ZSKIP");
			case ZNAK:		return("ZNAK");
			case ZABORT:	return("ZABORT");
			case ZFIN:		return("ZFIN");
			case ZRPOS:		return("ZRPOS");
			case ZDATA:		return("ZDATA");
			case ZEOF:		return("ZEOF");
			case ZPAD:		return("ZPAD");
			case ZDLE:		return("ZDLE");
			case ZDLEE:		return("ZDLEE");
			case ZBIN:		return("ZBIN");
			case ZHEX:		return("ZHEX");
			case ZBIN32:	return("ZBIN32");
			case ZRESC:		return("ZRESC");
			case ZCRCE:		return("ZCRCE");
			case ZCRCG:		return("ZCRCG");
			case ZCRCQ:		return("ZCRCQ");
			case ZCRCW:		return("ZCRCW");
		}
	} else {
		switch(ch) {
			case SOH:	return("SOH");
			case STX:	return("STX");
			case ETX:	return("ETX");
			case EOT:	return("EOT");
			case ACK:	return("ACK");
			case NAK:	return("NAK");
			case CAN:	return("CAN");
		}
	}
	if(ch>=' ' && ch<='~')
		sprintf(str,"'%c' (%02Xh)",ch,ch);
	else
		sprintf(str,"%u (%02Xh)",ch,ch);
	return(str); 
}

void dump(BYTE* buf, int len)
{
	char str[128];
	int i,j;
	size_t slen=0;

	slen=sprintf(str,"TX: ");
	for(i=0;i<len;i+=j) {
		for(j=0;i+j<len && j<32;j++)
			slen+=sprintf(str+slen,"%02X ",buf[i+j]);
		lprintf(LOG_DEBUG,"%s",str);
		slen=sprintf(str,"TX: ");
	}
}

int sendbuf(SOCKET s, void *buf, size_t buflen)
{
	size_t		sent=0;
	int			ret;
	fd_set		socket_set;

	for(;;) {
#ifdef __unix__
		if(stdio)
			ret=write(STDOUT_FILENO, (char *)buf+sent,buflen-sent);
		else
#endif
			ret=sendsocket(s,(char *)buf+sent,buflen-sent);
		if(ret==SOCKET_ERROR) {
			switch(ERROR_VALUE) {
				case EAGAIN:
				case ENOBUFS:
#if (EAGAIN != EWOULDBLOCK)
				case EWOULDBLOCK:
#endif
					/* Block until we can send */
					FD_ZERO(&socket_set);
#ifdef __unix__
					if(stdio)
						FD_SET(STDIN_FILENO,&socket_set);
					else
#endif
						FD_SET(sock,&socket_set);

					if((ret=select(sock+1,NULL,&socket_set,NULL,NULL))<1) {
						if(ret==SOCKET_ERROR && ERROR_VALUE != EINTR) {
							lprintf(LOG_ERR,"ERROR %d selecting socket", ERROR_VALUE);
							goto disconnect;
						}
					}
					break;
				default:
					goto disconnect;
			}
		}
		else {
			sent += ret;
			if(sent >= buflen)
				return(sent);
		}
	}

disconnect:
	lprintf(LOG_DEBUG,"DISCONNECTED line %u", __LINE__);
	connected=FALSE;
	if(sent)
		return sent;
	return SOCKET_ERROR;
}

void send_telnet_cmd(SOCKET sock, uchar cmd, uchar opt)
{
	uchar	buf[3];
	
	buf[0]=TELNET_IAC;
	buf[1]=cmd;
	buf[2]=opt;

	if(debug_telnet)
		lprintf(LOG_DEBUG,"Sending telnet command: %s %s"
			,telnet_cmd_desc(buf[1]),telnet_opt_desc(buf[2]));

	if(sendbuf(sock,buf,sizeof(buf))!=sizeof(buf) && debug_telnet)
		lprintf(LOG_ERR,"FAILED");
}

#define DEBUG_TELNET FALSE

/*
 * Returns -1 on disconnect or the number of bytes read.
 * Does not muck around with ERROR_VALUE (hopefully)
 */
static int recv_buffer(int timeout /* seconds */)
{
	int i;
	fd_set		socket_set;
	struct timeval	tv;
	int			magic_errno;

	for(;;) {
		if(inbuf_len > inbuf_pos)
			return(inbuf_len-inbuf_pos);
#ifdef __unix__
		if(stdio) {
			i=read(STDIN_FILENO,inbuf,sizeof(inbuf));
			/* Look like a socket using MAGIC! */
			switch(i) {
				case 0:
					i=-1;
					magic_errno=EAGAIN;
					break;
				case -1:
					magic_errno=errno;
					break;
			}
		}
		else
#endif
		{
			i=recv(sock,inbuf,sizeof(inbuf),0);
			if(i==SOCKET_ERROR)
				magic_errno=ERROR_VALUE;
		}
		if(i==SOCKET_ERROR) {
			switch(magic_errno) {
				case EAGAIN:
				case EINTR:
#if (EAGAIN != EWOULDBLOCK)
				case EWOULDBLOCK:
#endif
					// Call select()
					if(timeout) {
						FD_ZERO(&socket_set);
#ifdef __unix__
						if(stdio)
							FD_SET(STDIN_FILENO,&socket_set);
						else
#endif
							FD_SET(sock,&socket_set);
						tv.tv_sec=timeout;
						tv.tv_usec=0;
						if((i=select(sock+1,&socket_set,NULL,NULL,&tv))<1) {
							if(i==SOCKET_ERROR) {
								lprintf(LOG_ERR,"ERROR %d selecting socket", magic_errno);
								connected=FALSE;
							}
							else
								lprintf(LOG_WARNING,"Receive timeout (%u seconds)", timeout);
						}
						else {
							timeout=0;
							continue;
						}
					}
					return 0;
				default:
					lprintf(LOG_DEBUG,"DISCONNECTED line %u, error=%u", __LINE__,magic_errno);
					connected=FALSE;
					return -1;
			}
			return -1; // Impossible.
		}
		else if(i==0) {
			lprintf(LOG_DEBUG,"DISCONNECTED line %u", __LINE__);
			connected=FALSE;
			return -1;
		}
		else {
			inbuf_len=i;
			return i;
		}
	};
}

/****************************************************************************/
/* Receive a byte from remote (single-threaded version)						*/
/****************************************************************************/
int recv_byte(void* unused, unsigned timeout /* seconds */)
{
	int			i;
	uchar		ch;
	static uchar	telnet_cmd;
	static int		telnet_cmdlen;

	while((inbuf_len || connected) && !terminate) {
		if(inbuf_len) {
			ch=inbuf[inbuf_pos++];
			i=1;
			if(inbuf_pos >= inbuf_len)
				inbuf_pos=inbuf_len=0;
		}
		else {
			i=recv_buffer(timeout);
			switch(i) {
				case -1:
					i=0;
					break;
				case 0:
					return NOINP;
				default:
					i=1;
					continue;
			}
		}

		if(i!=sizeof(ch)) {
			if(i==0) {
				lprintf(LOG_WARNING,"Socket Disconnected");
			} else
				lprintf(LOG_ERR,"recv error %d (%d)",i,ERROR_VALUE);
			return(NOINP); 
		}

		if(telnet) {
			if(ch==TELNET_IAC) {
#if DEBUG_TELNET
				lprintf(LOG_DEBUG,"T<%s> ",telnet_cmd_desc(ch));
#endif
				if(telnet_cmdlen==0) {
					telnet_cmdlen=1;
					continue;
				}
				if(telnet_cmdlen==1) {
					telnet_cmdlen=0;
					if(debug_rx)
						lprintf(LOG_DEBUG,"RX: %s",chr(TELNET_IAC));
					return(TELNET_IAC);
				}
			}
			if(telnet_cmdlen) {
				telnet_cmdlen++;
#if DEBUG_TELNET
				if(telnet_cmdlen==2)
					lprintf(LOG_DEBUG,"T<%s> ",telnet_cmd_desc(ch));
				else
					lprintf(LOG_DEBUG,"T<%s> ",telnet_opt_desc(ch));
#endif
				if(debug_telnet && telnet_cmdlen==3)
					lprintf(LOG_DEBUG,"Received telnet command: %s %s"
						,telnet_cmd_desc(telnet_cmd),telnet_opt_desc(ch));
				if(telnet_cmdlen==3 && telnet_cmd==TELNET_DO)
					send_telnet_cmd(sock, TELNET_WILL,ch);
	/*
				else if(telnet_cmdlen==3 && telnet_cmd==TELNET_WILL)
					send_telnet_cmd(sock, TELNET_DO,ch);
	*/
				telnet_cmd=ch;
				if((telnet_cmdlen==2 && ch<TELNET_WILL) || telnet_cmdlen>2) {
					telnet_cmdlen=0;
					/* Code disabled.  Why?  ToDo */
					/* break; */
				}
				continue;
			}
		}
		if(debug_rx)
			lprintf(LOG_DEBUG,"RX: %s",chr(ch));
		return(ch);
	}

	return(NOINP);
}

#if !SINGLE_THREADED
/*************************/
/* Send a byte to remote */
/*************************/
int send_byte(void* unused, uchar ch, unsigned timeout)
{
	uchar		buf[2] = { TELNET_IAC, TELNET_IAC };
	unsigned	len=1;
	DWORD		result;

	if(telnet && ch==TELNET_IAC)	/* escape IAC char */
		len=2;
	else
		buf[0]=ch;

	if(RingBufFree(&outbuf)<len) {
#if !defined(RINGBUF_EVENT)
		ResetEvent(outbuf_empty);
#endif
		fprintf(statfp,"FLOW");
		flows++;
		result=WaitForEvent(outbuf_empty,timeout*1000);
		fprintf(statfp,"\b\b\b\b    \b\b\b\b");
		if(result!=WAIT_OBJECT_0) {
			lprintf(LOG_WARNING
				,"!TIMEOUT (%d) waiting for output buffer to flush (%u seconds, %u bytes)\n"
				,result, timeout, RingBufFull(&outbuf));
			fprintf(statfp
				,"\n!TIMEOUT (%d) waiting for output buffer to flush (%u seconds, %u bytes)\n"
				,result, timeout, RingBufFull(&outbuf));
			newline=TRUE;
			if((result=RingBufFree(&outbuf))<len) {
				lprintf(LOG_ERR,"Still not enough space in ring buffer (need %d, avail=%d)",len,result);
				return(-1);
			}
		}
		if((result=RingBufFree(&outbuf))<len) {
			lprintf(LOG_ERR,"Not enough space in ring buffer (need %d, avail=%d) although empty event is set!",len,result);
			return(-1);
		}
	}

	if((result=RingBufWrite(&outbuf,buf,len))!=len) {
		lprintf(LOG_ERR,"RingBufWrite() returned %d, expected %d",result,len);
	}

#if 0
	if(debug_tx)
		lprintf(LOG_DEBUG,"TX: %s",chr(ch));
#endif
	return(0);
}

#else

/*************************/
/* Send a byte to remote */
/*************************/
int send_byte(void* unused, uchar ch, unsigned timeout)
{
	uchar		buf[2] = { TELNET_IAC, TELNET_IAC };
	int			len=1;
	int			i;

	if(telnet && ch==TELNET_IAC)	/* escape IAC char */
		len=2;
	else
		buf[0]=ch;

	i=sendbuf(sock,buf,len);
	
	if(i==len) {
		if(debug_tx)
			lprintf(LOG_DEBUG,"TX: %s",chr(ch));
		return(0);
	}

	return(-1);
}
#endif

static void output_thread(void* arg)
{
	char		stats[128];
    BYTE		buf[MAX_OUTBUF_SIZE];
	int			i;
    ulong		avail;
	uint64_t	total_sent=0;
	uint64_t	total_pkts=0;
	ulong		short_sends=0;
    ulong		bufbot=0;
    ulong		buftop=0;

#if 0 /* def _DEBUG */
	fprintf(statfp,"output thread started\n");
#endif

	while(sock!=INVALID_SOCKET && !terminate) {

		if(bufbot==buftop)
	    	avail=RingBufFull(&outbuf);
		else
        	avail=buftop-bufbot;

		if(!avail) {
#if !defined(RINGBUF_EVENT)
			SetEvent(outbuf_empty);
#endif
			sem_wait(&outbuf.sem);
			if(outbuf.highwater_mark)
				sem_trywait_block(&outbuf.highwater_sem,outbuf_drain_timeout);
			continue; 
		}

        if(bufbot==buftop) { /* linear buf empty, read from ring buf */
            if(avail>sizeof(buf)) {
                lprintf(LOG_ERR,"Insufficient linear output buffer (%lu > %lu)"
					,avail, sizeof(buf));
                avail=sizeof(buf);
            }
            buftop=RingBufRead(&outbuf, buf, avail);
            bufbot=0;
        }
		i=sendbuf(sock, (char*)buf+bufbot, buftop-bufbot);
		if(i==SOCKET_ERROR) {
        	if(ERROR_VALUE == ENOTSOCK)
                lprintf(LOG_ERR,"client socket closed on send");
            else if(ERROR_VALUE==ECONNRESET) 
				lprintf(LOG_ERR,"connection reset by peer on send");
            else if(ERROR_VALUE==ECONNABORTED) 
				lprintf(LOG_ERR,"connection aborted by peer on send");
			else
				lprintf(LOG_ERR,"ERROR %d sending on socket %d"
                	,ERROR_VALUE, sock);
			break;
		}

		if(debug_tx)
			dump(buf+bufbot,i);

		if(i!=(int)(buftop-bufbot)) {
			lprintf(LOG_ERR,"Short socket send (%u instead of %u)"
				,i ,buftop-bufbot);
			short_sends++;
		}
		bufbot+=i;
		total_sent+=i;
		total_pkts++;
    }

	if(total_sent)
		sprintf(stats,"(sent %"PRIu64" bytes in %"PRIu64" blocks, %"PRIu64" average, %lu short, %u errors)"
			,total_sent, total_pkts, total_sent/total_pkts, short_sends, select_errors);
	else
		stats[0]=0;

	lprintf(LOG_DEBUG,"output thread terminated\n%s", stats);
}

/* Flush output buffer */
void flush(void* unused)
{
#ifdef __unix__
	if(stdio)
		fflush(stdout);
#endif
}

BOOL is_connected(void* unused)
{
	if(inbuf_len > inbuf_pos)
		return TRUE;
	return connected;
}

BOOL data_waiting(void* unused, unsigned timeout /* seconds */)
{
	if(recv_buffer(timeout) > 0)
		return TRUE;
	return FALSE;
}

/****************************************************************************/
/* Returns the total number of blocks required to send the file				*/
/****************************************************************************/
uint64_t num_blocks(unsigned block_num, uint64_t offset, uint64_t len, unsigned block_size)
{
	uint64_t blocks;
	uint64_t remain = len - offset;

	blocks=block_num + (remain/block_size);
	if(remain%block_size)
		blocks++;
	return(blocks);
}

/************************************************/
/* Dump the current block contents - for debug  */
/************************************************/
void dump_block(long block_size)
{
	long l;

	for(l=0;l<block_size;l++)
		fprintf(statfp,"%02X  ",block[l]);
	fprintf(statfp,"\n");
}

void xmodem_progress(void* unused, unsigned block_num, int64_t offset, int64_t fsize, time_t start)
{
	unsigned	cps;
	uint64_t	total_blocks;
	long		l;
	long		t;
	time_t		now;
	static time_t last_progress;

	now=time(NULL);
	if(now-last_progress>=progress_interval || offset >= fsize || newline) {
		t=(long)(now-start);
		if(t<=0)
			t=1;
		if((cps=(unsigned)(offset/t))==0)
			cps=1;				/* cps so far */
		l=(long)(fsize/cps);	/* total transfer est time */
		l-=t;					/* now, it's est time left */
		if(l<0) l=0;
		if(mode&SEND) {
			total_blocks=num_blocks(block_num,offset,fsize,xm.block_size);
			fprintf(statfp,"\rBlock (%lu%s): %u/%"PRId64"  Byte: %"PRId64"  "
				"Time: %lu:%02lu/%lu:%02lu  %u cps  %lu%% "
				,xm.block_size%1024L ? xm.block_size: xm.block_size/1024L
				,xm.block_size%1024L ? "" : "K"
				,block_num
				,total_blocks
				,offset
				,t/60L
				,t%60L
				,l/60L
				,l%60L
				,cps
				,fsize?(long)(((float)offset/(float)fsize)*100.0):100
				);
		} else if(mode&YMODEM) {
			fprintf(statfp,"\rBlock (%lu%s): %u  Byte: %"PRId64"  "
				"Time: %lu:%02lu/%lu:%02lu  %u cps  %lu%% "
				,xm.block_size%1024L ? xm.block_size: xm.block_size/1024L
				,xm.block_size%1024L ? "" : "K"
				,block_num
				,offset
				,t/60L
				,t%60L
				,l/60L
				,l%60L
				,cps
				,fsize?(long)(((float)offset/(float)fsize)*100.0):100
				);
		} else { /* XModem receive */
			fprintf(statfp,"\rBlock (%lu%s): %u  Byte: %"PRId64"  "
				"Time: %lu:%02lu  %u cps "
				,xm.block_size%1024L ? xm.block_size: xm.block_size/1024L
				,xm.block_size%1024L ? "" : "K"
				,block_num
				,offset
				,t/60L
				,t%60L
				,cps
				);
		}
		newline=FALSE;
		last_progress=now;
	}
}

/* 
 * show the progress of the transfer like this:
 * zmtx: sending file "garbage" 4096 bytes ( 20%)
 */
void zmodem_progress(void* cbdata, int64_t current_pos)
{
	unsigned	cps;
	long		l;
	long		t;
	time_t		now;
	static time_t last_progress;

	now=time(NULL);
	if(now-last_progress>=progress_interval || current_pos >= zm.current_file_size || newline) {
		t=(long)(now-zm.transfer_start_time);
		if(t<=0)
			t=1;
		if(zm.transfer_start_pos>current_pos)
			zm.transfer_start_pos=0;
		if((cps=(unsigned)((current_pos-zm.transfer_start_pos)/t))==0)
			cps=1;							/* cps so far */
		l=(long)(zm.current_file_size/cps);	/* total transfer est time */
		l-=t;								/* now, it's est time left */
		if(l<0) l=0;
		fprintf(statfp,"\rKByte: %"PRId64"/%"PRId64"  %u/CRC-%u  "
			"Time: %lu:%02lu/%lu:%02lu  %u cps  %lu%% "
			,current_pos/1024
			,zm.current_file_size/1024
			,zm.block_size
			,mode&RECV ? (zm.receive_32bit_data ? 32:16) : 
				(zm.can_fcs_32 && !zm.want_fcs_16) ? 32:16
			,t/60L
			,t%60L
			,l/60L
			,l%60L
			,cps
			,zm.current_file_size?(long)(((float)current_pos/(float)zm.current_file_size)*100.0):100
			);
		newline=FALSE;
		last_progress=now;
	}
}

static int send_files(char** fname, uint fnames)
{
	char		path[MAX_PATH+1];
	int			i;
	uint		fnum;
	uint		cps;
	glob_t		g;
	int			gi;
	BOOL		success=FALSE;
	uint64_t	fsize;
	uint64_t	sent_bytes;
	uint64_t	total_bytes=0;
	time_t		t,startfile;
	time_t		startall;
	FILE*		fp;

	startall=time(NULL);

	/****************************************************/
	/* Search through all to find total files and bytes */
	/****************************************************/
	for(fnum=0;fnum<fnames;fnum++) {
		if(glob(fname[fnum],0,NULL,&g)) {
			lprintf(LOG_WARNING,"%s not found",fname[fnum]);
			continue;
		}
		for(i=0;i<(int)g.gl_pathc;i++) {
			if(isdir(g.gl_pathv[i]))
				continue;
			xm.total_files++;
			xm.total_bytes+=flength(g.gl_pathv[i]);
		} 
		globfree(&g);
	}

	if(xm.total_files<1) {
		lprintf(LOG_ERR,"No files to send");
		return(-1);
	}
	if(xm.total_files>1)
		lprintf(LOG_INFO,"Sending %u files (%"PRId64" KB total)"
			,xm.total_files,xm.total_bytes/1024);

	zm.files_remaining = xm.total_files;
	zm.bytes_remaining = xm.total_bytes;

	/***********************************************/
	/* Send every file matching names or filespecs */
	/***********************************************/
	for(fnum=0;fnum<fnames;fnum++) {
		if(glob(fname[fnum],0,NULL,&g)) {
			lprintf(LOG_WARNING,"%s not found",fname[fnum]);
			continue;
		}
		for(gi=0;gi<(int)g.gl_pathc;gi++) {
			SAFECOPY(path,g.gl_pathv[gi]);
			if(isdir(path))
				continue;

			if((fp=fnopen(NULL,path,O_RDONLY|O_BINARY))==NULL
				&& (fp=fopen(path,"rb"))==NULL) {
				lprintf(LOG_ERR,"Error %d opening %s for read",errno,path);
				continue;
			}
			setvbuf(fp,NULL,_IOFBF,0x10000);

			fsize=filelength(fileno(fp));

			startfile=time(NULL);

			lprintf(LOG_INFO,"Sending %s (%"PRId64" KB) via %cMODEM"
				,path,fsize/1024
				,mode&XMODEM ? 'X' : mode&YMODEM ? 'Y' : 'Z');

			if(mode&ZMODEM)
				success=zmodem_send_file(&zm, path, fp, /* ZRQINIT? */fnum==0, &startfile, &sent_bytes);
			else	/* X/Ymodem */
				success=xmodem_send_file(&xm, path, fp, &startfile, &sent_bytes);

			fclose(fp);

			if((t=time(NULL)-startfile)<=0) 
				t=1;
			if((cps=(unsigned)(sent_bytes/t))==0)
				cps=1;
			if(success) {
				xm.sent_files++;
				xm.sent_bytes+=fsize;
				if(zm.file_skipped)
					lprintf(LOG_WARNING,"File Skipped");
				else
					lprintf(LOG_INFO,"Successful - Time: %lu:%02lu  CPS: %lu"
						,t/60,t%60,cps);

				if(xm.total_files-xm.sent_files)
					lprintf(LOG_INFO,"Remaining - Time: %lu:%02lu  Files: %u  KBytes: %"PRId64
						,((xm.total_bytes-xm.sent_bytes)/cps)/60
						,((xm.total_bytes-xm.sent_bytes)/cps)%60
						,xm.total_files-xm.sent_files
						,(xm.total_bytes-xm.sent_bytes)/1024
						);
			} else
				lprintf(LOG_WARNING,"File Transfer %s", zm.local_abort ? "Aborted" : "Failure");

			/* DSZLOG entry */
			if(logfp) {
				lprintf(LOG_DEBUG,"Updating DSZLOG: %s", dszlog);
				fprintf(logfp,"%c %7"PRId64" %5u bps %6u cps %3u errors %5u %4u "
					"%s -1\n"
					,(mode&ZMODEM && zm.file_skipped) ? 's' 
						: success ? (mode&ZMODEM ? 'z':'S') 
						: 'E'
					,sent_bytes
					,115200 /* baud */
					,cps
					,mode&ZMODEM ? zm.errors : xm.errors
					,flows
					,mode&ZMODEM ? zm.block_size : xm.block_size
					,dszlog_filename(path)); 
				fflush(logfp);
			}
			total_bytes += sent_bytes;

			if(zm.local_abort) {
				xm.cancelled=FALSE;
				xmodem_cancel(&xm);
				break;
			}

			if(xm.cancelled || zm.cancelled || !success)
				break;

		} /* while(gi<(int)g.gl_pathc) */

		if(gi<(int)g.gl_pathc)/* error occurred */
			break;
	}

	if(mode&ZMODEM && !zm.cancelled && is_connected(NULL) && (success || total_bytes))
		zmodem_get_zfin(&zm);

	if(fnum<fnames) /* error occurred */
		return(-1);

	if(!success)
		return(-1);

	if(mode&XMODEM)
		return(0);
	if(mode&YMODEM) {

		if(xmodem_get_mode(&xm)) {

			lprintf(LOG_INFO,"Sending YMODEM termination block");

			memset(block,0,XMODEM_MIN_BLOCK_SIZE);	/* send short block for terminator */
			xmodem_put_block(&xm, block, XMODEM_MIN_BLOCK_SIZE /* block_size */, 0 /* block_num */);
			if(xmodem_get_ack(&xm, /* tries: */6, /* block_num: */0) != ACK) {
				lprintf(LOG_WARNING,"Failed to receive ACK after terminating block"); 
			} 
		}
	}
	if(xm.total_files>1) {
		t=time(NULL)-startall;
		if(!t) t=1;
		lprintf(LOG_INFO,"Overall - Time %02lu:%02lu  KBytes: %"PRId64"  CPS: %lu"
			,t/60,t%60,total_bytes/1024,total_bytes/t); 
	}
	return(0);	/* success */
}

static int receive_files(char** fname_list, int fnames)
{
	char	str[MAX_PATH+1];
	char	fname[MAX_PATH+1];
	int		i;
	int		fnum=0;
	uint	errors;
	uint	total_files=0;
	uint	cps;
	uint	wr;
	BOOL	success=FALSE;
	long	fmode;
	long	serial_num=-1;
	int64_t	file_bytes=0,file_bytes_left=0;
	int64_t	total_bytes=0;
	FILE*	fp;
	time_t	t,startfile;
	uintmax_t	ftime;

	if(fnames>1)
		lprintf(LOG_INFO,"Receiving %u files",fnames);

	outbuf.highwater_mark=0;	/* don't delay ACK/NAK transmits */

	/* Purge input buffer */
	while(is_connected(NULL) && (i=recv_byte(NULL,0))!=NOINP)
		lprintf(LOG_WARNING,"Throwing out received: %s",chr((uchar)i));

	while(is_connected(NULL)) {
		file_bytes=0x7fffffff;
		if(mode&XMODEM) {
			SAFECOPY(str,fname_list[0]);	/* we'll have at least one fname */
			file_bytes_left=file_bytes;
		}
		else {
			if(mode&YMODEM) {
				lprintf(LOG_INFO,"Fetching YMODEM header block");
				for(errors=0;errors<=xm.max_errors && !xm.cancelled;errors++) {
					xmodem_put_nak(&xm, /* expected_block: */ 0);
					if(xmodem_get_block(&xm, block, /* expected_block: */ 0) == SUCCESS) {
						if(!(mode&GMODE))
							send_byte(NULL,ACK,10);
						break; 
					} 
					if(errors+1>xm.max_errors/3 && mode&CRC && !(mode&GMODE)) {
						lprintf(LOG_NOTICE,"Falling back to 8-bit Checksum mode");
						mode&=~CRC;
					}
				}
				if(errors>xm.max_errors || xm.cancelled) {
					lprintf(LOG_ERR,"Error fetching YMODEM header block");
					xmodem_cancel(&xm);
					return(1); 
				}
				if(!block[0]) {
					lprintf(LOG_INFO,"Received YMODEM termination block");
					return(0); 
				}
				ftime=total_files=0;
				total_bytes=0;
				i=sscanf((char*)block+strlen((char*)block)+1,"%"SCNd64" %"SCNoMAX" %lo %lo %u %"SCNd64
					,&file_bytes			/* file size (decimal) */
					,&ftime 				/* file time (octal unix format) */
					,&fmode 				/* file mode (not used) */
					,&serial_num			/* program serial number */
					,&total_files			/* remaining files to be sent */
					,&total_bytes			/* remaining bytes to be sent */
					);
				lprintf(LOG_DEBUG,"YMODEM header (%u fields): %s", i, block+strlen((char*)block)+1);
				SAFECOPY(fname,(char*)block);

			} else {	/* Zmodem */
				lprintf(LOG_INFO,"Waiting for ZMODEM sender...");

				i=zmodem_recv_init(&zm);

				if(zm.cancelled)
					return(1);
				if(i<0)
					return(-1);
				switch(i) {
					case ZFILE:
						SAFECOPY(fname,zm.current_file_name);
						file_bytes = zm.current_file_size;
						ftime = zm.current_file_time;
						total_files = zm.files_remaining;
						total_bytes = zm.bytes_remaining;
						break;
					case ZFIN:
					case ZCOMPL:
						return(!success);
					default:
						return(-1);
				}
			}
			file_bytes_left=file_bytes;
			if(!total_files)
				total_files=fnames-fnum;
			if(!total_files)
				total_files=1;
			if(total_bytes<file_bytes)
				total_bytes=file_bytes;

			lprintf(LOG_DEBUG,"Incoming filename: %.64s ",fname);

			if(mode&RECVDIR)
				SAFEPRINTF2(str,"%s%s",fname_list[0],getfname(fname));
			else {
				SAFECOPY(str,getfname(fname));
				for(i=0;i<fnames;i++) {
					if(!fname_list[i][0])	/* name blank or already used */
						continue;
					if(!stricmp(getfname(fname_list[i]),str)) {
						SAFECOPY(str,fname_list[i]);
						fname_list[i][0]=0;
						break; 
					} 
				}
				if(i==fnames) { 				/* Not found in list */
					if(fnames)
						fprintf(statfp," - Not in receive list!");
					if(!fnames || fnum>=fnames || !fname_list[fnum][0])
						SAFECOPY(str,getfname(fname));	/* worst case */
					else {
						SAFECOPY(str,fname_list[fnum]);
						fname_list[fnum][0]=0; 
					} 
				} 
			}
			fprintf(statfp,"File size: %"PRId64" bytes\n", file_bytes);
			if(total_files>1)
				fprintf(statfp,"Remaining: %"PRId64" bytes in %u files\n", total_bytes, total_files);
		}

		lprintf(LOG_DEBUG,"Receiving: %.64s ",str);

		fnum++;

		if(!(mode&RECVDIR) && fnames && fnum>fnames) {
			lprintf(LOG_WARNING,"Attempt to send more files than specified");
			xmodem_cancel(&xm);
			break; 
		}

		if(fexistcase(str) && !(mode&OVERWRITE)) {
			lprintf(LOG_WARNING,"%s already exists",str);
			if(mode&ZMODEM) {
				zmodem_send_zskip(&zm);
				continue;
			}
			xmodem_cancel(&xm);
			return(1); 
		}

		if(!(mode&XMODEM) && max_file_size!=0 && file_bytes > max_file_size) {
			lprintf(LOG_WARNING,"%s file size (%"PRId64") exceeds specified maximum: %"PRId64" bytes", str, file_bytes, max_file_size);
			if(mode&ZMODEM) {
				zmodem_send_zskip(&zm);
				continue;
			}
			xmodem_cancel(&xm);
			return(1); 
		}

		if(lc_filenames) {
			strlwr(str);
		}

		if((fp=fnopen(NULL,str,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY))==NULL
			&& (fp=fopen(str,"wb"))==NULL) {
			lprintf(LOG_ERR,"Error %d creating %s",errno,str);
			if(mode&ZMODEM) {
				zmodem_send_zskip(&zm);
				continue;
			}
			xmodem_cancel(&xm);
			return(1); 
		}

		if(mode&XMODEM)
			lprintf(LOG_INFO,"Receiving %s via XMODEM%s %s"
				,str
				,mode&GMODE ? "-G" : ""
				,mode&CRC ? "CRC-16":"Checksum");
		else
			lprintf(LOG_INFO,"Receiving %s (%"PRId64" KB) via %s %s"
				,str
				,file_bytes/1024
				,mode&YMODEM ? mode&GMODE ? "YMODEM-G" : "YMODEM" :"ZMODEM"
				,mode&ZMODEM ? "" : (mode&CRC ? "CRC-16" : "Checksum"));

		startfile=time(NULL);
		success=FALSE;
		if(mode&ZMODEM) {

			errors=zmodem_recv_file_data(&zm,fp,0);

			if(errors<=zm.max_errors && !zm.cancelled)
				success=TRUE;

		} else {
			errors=0;
			block_num=1;
			xmodem_put_nak(&xm, block_num);
			while(is_connected(NULL)) {
				off_t pos=ftello(fp);
				if(max_file_size!=0 && pos>=max_file_size) {
					lprintf(LOG_WARNING,"Specified maximum file size (%"PRId64" bytes) reached at offset %"PRId64
						,max_file_size, pos);
					break;
				}
				xmodem_progress(NULL,block_num,pos,file_bytes,startfile);
				i=xmodem_get_block(&xm, block, block_num); 	

				if(i!=SUCCESS) {
					if(i==EOT)	{		/* end of transfer */
						success=TRUE;
						xmodem_put_ack(&xm);
						break;
					}
					if(i==CAN) {		/* Cancel */
						xm.cancelled=TRUE;
						break;
					}

					if(++errors>xm.max_errors || (mode&GMODE)) {
						lprintf(LOG_ERR,"Too many errors (%u)",errors);
						xmodem_cancel(&xm);
						break;
					}
					if(i!=NOT_XMODEM 
						&& block_num==1 && errors>xm.max_errors/3 && mode&CRC && !(mode&GMODE)) {
						lprintf(LOG_NOTICE,"Falling back to 8-bit Checksum mode (error=%d)", i);
						mode&=~CRC;
					}
					xmodem_put_nak(&xm, block_num);
					continue;
				}
				if(!(mode&GMODE))
					send_byte(NULL,ACK,10);
				if(file_bytes_left<=0L)  { /* No more bytes to receive */
					lprintf(LOG_WARNING,"Sender attempted to send more bytes than were specified in header");
					break; 
				}
				wr=xm.block_size;
				if(wr>(uint)file_bytes_left)
					wr=(uint)file_bytes_left;
				if(fwrite(block,1,wr,fp)!=wr) {
					lprintf(LOG_ERR,"ERROR %d writing %u bytes at file offset %"PRIu64
						,errno, wr, (uint64_t)ftello(fp));
					fclose(fp);
					xmodem_cancel(&xm);
					return(1); 
				}
				file_bytes_left-=wr; 
				block_num++;
			}
		}

		/* Use correct file size */
		if(mode&ZMODEM)
			file_bytes = zm.current_file_size;	/* file can grow in transit */
		else {
			fflush(fp);
			if(file_bytes < filelength(fileno(fp))) {
				lprintf(LOG_INFO,"Truncating file to %"PRIu64" bytes", file_bytes);
				chsize(fileno(fp),(ulong)file_bytes);	/* <--- 4GB limit */
			} else
				file_bytes = filelength(fileno(fp));
		}
		fclose(fp);

		t=time(NULL)-startfile;
		if(!t) t=1;
		if(zm.file_skipped)
			lprintf(LOG_WARNING,"File Skipped");
		else if(success)
			lprintf(LOG_INFO,"Successful - Time: %lu:%02lu  CPS: %lu"
				,t/60,t%60,(ulong)(file_bytes/t));
		else
			lprintf(LOG_ERR,"File Transfer %s"
				,zm.local_abort ? "Aborted": zm.cancelled ? "Cancelled":"Failure");

		if(!(mode&XMODEM) && ftime)
			setfdate(str,ftime); 

		if(logfp) {
			lprintf(LOG_DEBUG,"Updating DSZLOG: %s", dszlog);
			fprintf(logfp,"%c %6"PRId64" %5u bps %4"PRId64" cps %3u errors %5u %4u "
				"%s %ld\n"
				,success ? (mode&ZMODEM ? 'Z' : 'R') : 'E'
				,file_bytes
				,115200	/* baud */
				,file_bytes/t
				,errors
				,flows
				,mode&ZMODEM ? zm.block_size : xm.block_size
				,dszlog_filename(str)
				,serial_num); 
			fflush(logfp);
		}

		if(zm.local_abort) {
			lprintf(LOG_DEBUG,"Locally aborted, sending cancel to remote");
			if(mode&ZMODEM)
				zmodem_send_zabort(&zm);
			xm.cancelled=FALSE;
			xmodem_cancel(&xm);
			break;
		}

		if(mode&XMODEM)	/* maximum of one file */
			break;
		if((cps=(unsigned)(file_bytes/t))==0)
			cps=1;
		total_files--;
		total_bytes-=file_bytes;
		if(total_files>1 && total_bytes)
			lprintf(LOG_INFO,"Remaining - Time: %lu:%02lu  Files: %u  KBytes: %"PRIu64
				,(total_bytes/cps)/60
				,(total_bytes/cps)%60
				,total_files
				,total_bytes/1024
				);
	}
	return(!success);	/* 0=success */
}

void bail(int code)
{
	lprintf(LOG_DEBUG, "Exiting with error level %d", code);
	if(pause_on_exit || (pause_on_abend && code!=0)) {
		printf("Hit enter to continue...");
		getchar();
	}
	exit(code);
}

static const char* usage=
	"usage: sexyz <socket> [-opts] <cmd> [file | path | @list]\n"
	"\n"
#ifdef __unix__
	"socket = TCP socket descriptor (leave blank for stdio mode)\n"
#else
	"socket = TCP socket descriptor\n"
#endif
	"\n"
	"opts   = -y  allow overwriting of existing files when receiving\n"
	"         -o  disable Zmodem CRC-32 mode (use CRC-16)\n"
	"         -s  disable Zmodem streaming (Slow Zmodem)\n"
	"         -k  enable X/Ymodem-1K send mode\n"
    "         -c  enable Xmodem-CRC receive mode\n"
	"         -g  enable X/Ymodem-G receive mode (no error recovery)\n"
	"         -2  set maximum Zmodem block size to 2K\n"
	"         -4  set maximum Zmodem block size to 4K\n"
	"         -8  set maximum Zmodem block size to 8K (ZedZap)\n"
	"         -m# set maximum receive file size to # bytes (0=unlimited, default=%u)\n"
	"         -!  to pause after abnormal exit (error)\n"
	"         -l  lowercase received filenames\n"
#ifdef __unix__
	"         -telnet to enable Telnet mode (the default except in stdio mode)\n"
#else
	"         -telnet to enable Telnet mode (the default)\n"
#endif
	"         -rlogin or -ssh or -raw to disable Telnet mode\n"
	"\n"
	"cmd    = v  to display detailed version information\n"
	"         sx to send Xmodem     rx to receive Xmodem\n"
	"         sX to send Xmodem-1K  rc to receive Xmodem-CRC\n"
	"         sy to send Ymodem     ry to receive Ymodem\n"
	"         sY to send Ymodem-1K  rg to receive Ymodem-G\n"
	"         sz to send Zmodem     rz to receive Zmodem\n"
	"\n"
	"file   = filename to send or receive\n"
	"path   = directory to receive files into\n"
	"list   = name of text file with list of filenames to send or receive\n";

#ifdef __unix__

struct termios tio_default;				/* Initial term settings */

#ifdef NEEDS_CFMAKERAW
static void
cfmakeraw(struct termios *t)
{
	t->c_iflag &= ~(IMAXBEL|IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	t->c_oflag &= ~OPOST;
	t->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	t->c_cflag &= ~(CSIZE|PARENB);
	t->c_cflag |= CS8;
}
#endif

static void fixterm(void)
{
	tcsetattr(STDIN_FILENO,TCSANOW,&tio_default);
}

static void init_stdio(void)
{
	struct termios tio_raw;

	if(isatty(STDERR_FILENO))
		fclose(stderr);

	if (isatty(STDIN_FILENO))  {
		tcgetattr(STDIN_FILENO,&tio_default);
		tio_raw = tio_default;
		/* cfmakeraw(&tio_raw); */
		tio_raw.c_iflag &= ~(IMAXBEL|IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
		tio_raw.c_oflag &= ~OPOST;
		tio_raw.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
		tio_raw.c_cflag &= ~(CSIZE|PARENB);
		tio_raw.c_cflag |= CS8;
		tcsetattr(STDIN_FILENO,TCSANOW,&tio_raw);
		setvbuf(stdout, NULL, _IOFBF, 0);
		atexit(fixterm);
	}
}

BOOL	RingBufIsEmpty(void *buf)
{
	return(RingBufFull(buf)==0);
}

#endif

/***************/
/* Entry Point */
/***************/
int main(int argc, char **argv)
{
	char	str[MAX_PATH+1];
	char	fname[MAX_PATH+1];
	char	ini_fname[MAX_PATH+1];
	char*	p;
	char*	arg;
	int 	i;
	int		retval;
	uint	fnames=0;
	FILE*	fp;
	BOOL	tcp_nodelay;
	char	compiler[32];
	BOOL	telnet_requested=FALSE;
	str_list_t fname_list;

	fname_list=strListInit();

	DESCRIBE_COMPILER(compiler);

	errfp=stderr;
#ifdef __unix__
	statfp=stderr;
#else
	statfp=stdout;
#endif

	sscanf("$Revision: 2.10 $", "%*s %s", revision);

	fprintf(statfp,"\nSynchronet External X/Y/ZMODEM  v%s-%s"
		"  Copyright %s Rob Swindell\n\n"
		,revision
		,PLATFORM_DESC
		,&__DATE__[7]
		);

	xmodem_init(&xm,NULL,&mode,lputs,xmodem_progress,send_byte,recv_byte,is_connected,NULL,flush);
	zmodem_init(&zm,NULL,lputs,zmodem_progress,send_byte,recv_byte,is_connected,NULL,data_waiting,flush);
	xm.log_level=&log_level;
	zm.log_level=&log_level;

	/* Generate path/sexyz[.host].ini from path/sexyz[.exe] */
	SAFECOPY(str,argv[0]);
	p=getfname(str);
	SAFECOPY(fname,p);
	*p=0;
	if((p=getfext(fname))!=NULL) 
		*p=0;
	strcat(fname,".ini");
	
	iniFileName(ini_fname,sizeof(ini_fname),str,fname);
	if((fp=fopen(ini_fname,"r"))!=NULL)
		fprintf(statfp,"Reading %s\n",ini_fname);

	tcp_nodelay				=iniReadBool(fp,ROOT_SECTION,"TCP_NODELAY",TRUE);

	telnet					=iniReadBool(fp,ROOT_SECTION,"Telnet",TRUE);
	debug_tx				=iniReadBool(fp,ROOT_SECTION,"DebugTx",FALSE);
	debug_rx				=iniReadBool(fp,ROOT_SECTION,"DebugRx",FALSE);
	debug_telnet			=iniReadBool(fp,ROOT_SECTION,"DebugTelnet",FALSE);

	pause_on_exit			=iniReadBool(fp,ROOT_SECTION,"PauseOnExit",FALSE);
	pause_on_abend			=iniReadBool(fp,ROOT_SECTION,"PauseOnAbend",FALSE);

	log_level				=iniReadLogLevel(fp,ROOT_SECTION,"LogLevel",log_level);
	use_syslog				=iniReadBool(fp,ROOT_SECTION,"SysLog",use_syslog);

	outbuf.highwater_mark	=(ulong)iniReadBytes(fp,ROOT_SECTION,"OutbufHighwaterMark",1,1100);
	outbuf_drain_timeout	=iniReadInteger(fp,ROOT_SECTION,"OutbufDrainTimeout",10);
	outbuf_size				=(ulong)iniReadBytes(fp,ROOT_SECTION,"OutbufSize",1,16*1024);

	progress_interval		=iniReadInteger(fp,ROOT_SECTION,"ProgressInterval",1);
	max_file_size 			=iniReadBytes(fp,ROOT_SECTION,"MaxFileSize",/* unit: */1,MAX_FILE_SIZE);

	if(iniReadBool(fp,ROOT_SECTION,"Debug",FALSE))
		log_level=LOG_DEBUG;

	xm.send_timeout			=iniReadInteger(fp,"Xmodem","SendTimeout",xm.send_timeout);	/* seconds */
	xm.recv_timeout			=iniReadInteger(fp,"Xmodem","RecvTimeout",xm.recv_timeout);	/* seconds */
	xm.byte_timeout			=iniReadInteger(fp,"Xmodem","ByteTimeout",xm.byte_timeout);	/* seconds */
	xm.ack_timeout			=iniReadInteger(fp,"Xmodem","AckTimeout",xm.ack_timeout);	/* seconds */
	xm.block_size			=(ulong)iniReadBytes(fp,"Xmodem","BlockSize",1,xm.block_size);			/* 128 or 1024 */
	xm.max_block_size		=(ulong)iniReadBytes(fp,"Xmodem","MaxBlockSize",1,xm.max_block_size);	/* 128 or 1024 */
	xm.max_errors			=iniReadInteger(fp,"Xmodem","MaxErrors",xm.max_errors);
	xm.g_delay				=iniReadInteger(fp,"Xmodem","G_Delay",xm.g_delay);
	xm.crc_mode_supported	=iniReadBool(fp,"Xmodem","SendCRC",xm.crc_mode_supported);
	xm.g_mode_supported		=iniReadBool(fp,"Xmodem","SendG",xm.g_mode_supported);

	xm.fallback_to_xmodem	=iniReadInteger(fp,"Ymodem","FallbackToXmodem", xm.fallback_to_xmodem);

	zm.init_timeout			=iniReadInteger(fp,"Zmodem","InitTimeout",zm.init_timeout);	/* seconds */
	zm.send_timeout			=iniReadInteger(fp,"Zmodem","SendTimeout",zm.send_timeout);	/* seconds */
	zm.recv_timeout			=iniReadInteger(fp,"Zmodem","RecvTimeout",zm.recv_timeout);	/* seconds */
	zm.crc_timeout			=iniReadInteger(fp,"Zmodem","CrcTimeout",zm.crc_timeout);	/* seconds */
	zm.block_size			=(ulong)iniReadBytes(fp,"Zmodem","BlockSize",1,zm.block_size);	/* 1024  */
	zm.max_block_size		=(ulong)iniReadBytes(fp,"Zmodem","MaxBlockSize",1,zm.max_block_size); /* 1024 or 8192 */
	zm.max_errors			=iniReadInteger(fp,"Zmodem","MaxErrors",zm.max_errors);
	zm.recv_bufsize			=(ulong)iniReadBytes(fp,"Zmodem","RecvBufSize",1,0);
	zm.no_streaming			=!iniReadBool(fp,"Zmodem","Streaming",TRUE);
	zm.want_fcs_16			=!iniReadBool(fp,"Zmodem","CRC32",TRUE);
	zm.escape_telnet_iac	=iniReadBool(fp,"Zmodem","EscapeTelnetIAC",TRUE);
	zm.escape_8th_bit		=iniReadBool(fp,"Zmodem","Escape8thBit",FALSE);
	zm.escape_ctrl_chars	=iniReadBool(fp,"Zmodem","EscapeCtrlChars",FALSE);

	dszlog_path				=iniReadBool(fp,"DSZLOG","Path",TRUE);
	dszlog_short			=iniReadBool(fp,"DSZLOG","Short",FALSE);
	dszlog_quotes			=iniReadBool(fp,"DSZLOG","Quotes",FALSE);

	if(fp!=NULL)
		fclose(fp);

	if(zm.recv_bufsize > 0xffff)
		zm.recv_bufsize = 0xffff;

	if(outbuf_size < MIN_OUTBUF_SIZE)
		outbuf_size = MIN_OUTBUF_SIZE;
	else if(outbuf_size > MAX_OUTBUF_SIZE)
		outbuf_size = MAX_OUTBUF_SIZE;
	
	lprintf(LOG_DEBUG, "Output buffer size: %lu", outbuf_size);
	RingBufInit(&outbuf, outbuf_size);

#if !defined(RINGBUF_EVENT)
	outbuf_empty=CreateEvent(NULL,/* ManualReset */TRUE, /*InitialState */TRUE,NULL);
#ifdef __unix__
	outbuf_empty->cbdata=&outbuf;
	outbuf_empty->verify=RingBufIsEmpty;
#endif
#endif

#if 0
	if(argc>1) {
		fprintf(statfp,"Command line: ");
		for(i=1;i<argc;i++)
			fprintf(statfp,"%s ",argv[i]);
		fprintf(statfp,"\n",statfp);
	}
#endif


	for(i=1;i<argc;i++) {

		if(sock==INVALID_SOCKET && isdigit(argv[i][0])) {
			sock=atoi(argv[i]);
			continue;
		}

		if(!(mode&(SEND|RECV))) {
			if(toupper(argv[i][0])=='S' || toupper(argv[i][0])=='R') { /* cmd */
				if(toupper(argv[i][0])=='R')
					mode|=RECV;
				else
					mode|=SEND;

				switch(argv[i][1]) {
					case 'c':
					case 'C':
						mode|=XMODEM|CRC;
						break;
					case 'x':
						xm.block_size=XMODEM_MIN_BLOCK_SIZE;
					case 'X':
						mode|=XMODEM;
						break;
					case 'b':	/* sz/rz compatible */
					case 'B':
					case 'y':
						xm.block_size=XMODEM_MIN_BLOCK_SIZE;
					case 'Y':
						mode|=(YMODEM|CRC);
						break;
					case 'k':	/* Ymodem-Checksum for debug/test purposes only */
						mode|=YMODEM;
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
						fprintf(statfp,usage,MAX_FILE_SIZE);
						bail(1); 
						return -1;
				} 
				continue;
			}

			if(toupper(argv[i][0])=='V') {

				fprintf(statfp,"%-8s %s\n",getfname(__FILE__)		,revision);
				fprintf(statfp,"%-8s %s\n",getfname(xmodem_source()),xmodem_ver(str));
				fprintf(statfp,"%-8s %s\n",getfname(zmodem_source()),zmodem_ver(str));
#ifdef _DEBUG
				fprintf(statfp,"Debug\n");
#endif
				fprintf(statfp,"Compiled %s %.5s with %s\n",__DATE__,__TIME__,compiler);
				fprintf(statfp,"%s\n",os_version(str));
				bail(0);
				return 0;
			}

			arg=argv[i];
			if(*arg=='-') {
				while(*arg=='-')
					arg++;
				if(stricmp(arg,"telnet")==0) {
					telnet_requested=TRUE;
					telnet=TRUE;
					continue;
				}
				if(stricmp(arg,"rlogin")==0 || stricmp(arg,"ssh")==0 || stricmp(arg,"raw")==0) {
					telnet_requested=FALSE;
					telnet=FALSE;
					continue;
				}
				if(stricmp(arg,"debug")==0) {
					log_level=LOG_DEBUG;
					continue;
				}
				if(stricmp(arg,"syslog")==0) {
					use_syslog=TRUE;
					continue;
				}
				if(stricmp(arg,"quotes")==0) {
					dszlog_quotes=TRUE;
					continue;
				}
				switch(toupper(*arg)) {
					case 'K':	/* sz/rz compatible */
						xm.block_size=XMODEM_MAX_BLOCK_SIZE;
						break;
					case 'C':	/* sz/rz compatible */
						mode|=CRC;
						break;
					case '2':
						zm.max_block_size=2048;
						break;
					case '4':
						zm.max_block_size=4096;
						break;
					case '8':	/* ZedZap */
						zm.max_block_size=8192;
						break;
					case 'O':	/* disable Zmodem CRC-32 */
						zm.want_fcs_16=TRUE;
						break;
					case 'S':	/* disable Zmodem streaming */
						zm.no_streaming=TRUE;
						break;
					case 'G':	/* Ymodem-G or Xmodem-G (a.k.a. Qmodem-G) */
						mode|=(GMODE|CRC);
						break;
					case 'Y':
						mode|=OVERWRITE;
						break;
					case '!':
						pause_on_abend=TRUE;
						break;
					case 'M':	/* MaxFileSize */
						max_file_size=strtoul(arg++,NULL,0);	/* TODO: use strtoull() ? */
						break;
					case 'L':	/* Lowercase received filenames */
						lc_filenames=TRUE;
						break;
				}
			}
		}

		else if((argv[i][0]=='+' || argv[i][0]=='@') && fexist(argv[i]+1)) {
			if(mode&RECVDIR) {
				fprintf(statfp,"!Cannot specify both directory and filename\n");
				bail(1); 
				return -1;
			}
			SAFEPRINTF(str,"%s",argv[i]+1);
			if((fp=fopen(str,"r"))==NULL) {
				fprintf(statfp,"!Error %d opening filelist: %s\n",errno,str);
				bail(1); 
				return -1;
			}
			while(!feof(fp) && !ferror(fp)) {
				if(!fgets(str,sizeof(str),fp))
					break;
				truncsp(str);
				strListAppend(&fname_list,strdup(str),fnames++);
			}
			fclose(fp); 
		}

		else if(mode&(SEND|RECV)){
			if(isdir(argv[i])) { /* is a directory */
				if(mode&RECVDIR) {
					fprintf(statfp,"!Only one directory can be specified\n");
					bail(1); 
					return -1;
				}
				if(fnames) {
					fprintf(statfp,"!Cannot specify both directory and filename\n");
					bail(1); 
					return -1;
				}
				if(mode&SEND) {
					fprintf(statfp,"!Cannot send directory '%s'\n",argv[i]);
					bail(1);
					return -1;
				}
				mode|=RECVDIR; 
			}
			strListAppend(&fname_list,argv[i],fnames++);
		} 
	}

	if(max_file_size)
		fprintf(statfp,"Maximum receive file size: %"PRIi64"\n", max_file_size);

	if(!(mode&(SEND|RECV))) {
		fprintf(statfp,"!No command specified\n\n");
		fprintf(statfp,usage,MAX_FILE_SIZE);
		bail(1); 
		return -1;
	}

	if(mode&(SEND|XMODEM) && !fnames) { /* Sending with any or recv w/Xmodem */
		fprintf(statfp,"!Must specify filename or filelist\n\n");
		fprintf(statfp,usage,MAX_FILE_SIZE);
		bail(1); 
		return -1;
	}

	if(sock==INVALID_SOCKET || sock<1) {
#ifdef __unix__
		if(STDOUT_FILENO > STDIN_FILENO)
			sock=STDOUT_FILENO;
		else
			sock=STDIN_FILENO;
		stdio=TRUE;

		fprintf(statfp,"No socket descriptor specified, using STDIO\n");
		if(!telnet_requested)
			telnet=FALSE;
		init_stdio();
#else
		fprintf(statfp,"!No socket descriptor specified\n\n");
		fprintf(errfp,usage,MAX_FILE_SIZE);
		bail(1);
		return -1;
#endif
	}
#ifdef __unix__
	else
		statfp=stdout;
#endif

	if(!telnet)
		zm.escape_telnet_iac = FALSE;

	zm.max_file_size = max_file_size;

	/* Code disabled.  Why?  ToDo */
/*	if(mode&RECVDIR)
		backslash(fname[0]); */

	if(!winsock_startup())
		bail(-1);

	/* Enable the Nagle Algorithm */
#ifdef __unix__
	if(!stdio) {
#endif
		lprintf(LOG_DEBUG,"Setting TCP_NODELAY to %d",tcp_nodelay);
		(void)setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,(char*)&tcp_nodelay,sizeof(tcp_nodelay));
#ifdef __unix__
	}
#endif

	/* Set non-blocking mode */
#ifdef __unix__
	if(stdio) {
		(void)fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
	}
	else
#endif
	{
		i=1;
		ioctlsocket(sock, FIONBIO, &i);
	}

	if(!socket_check(sock, NULL, NULL, 0)) {
		lprintf(LOG_WARNING,"No socket connection");
		bail(-1); 
		return -1;
	}

	if((dszlog=getenv("DSZLOG"))!=NULL) {
		lprintf(LOG_DEBUG, "Logging to %s", dszlog);
		if((logfp=fopen(dszlog,"w"))==NULL) {
			lprintf(LOG_WARNING,"Error %d opening DSZLOG file: %s",errno,dszlog);
			bail(-1); 
			return -1;
		}
	}

	/* Install Ctrl-C/Break signal handler here */
#if defined(_WIN32)
	SetConsoleCtrlHandler(ControlHandler, TRUE /* Add */);
#elif defined(__unix__)
	signal(SIGQUIT,break_handler);
	signal(SIGINT,break_handler);
	signal(SIGTERM,break_handler);

	signal(SIGHUP,SIG_IGN);

	/* Don't die on SIGPIPE  */
	signal(SIGPIPE,SIG_IGN);
#endif

#if !SINGLE_THREADED
	_beginthread(output_thread,0,NULL);
#endif

	if(mode&RECV)
		retval=receive_files(fname_list, fnames);
	else
		retval=send_files(fname_list, fnames);

#if !SINGLE_THREADED
	lprintf(LOG_DEBUG,"Waiting for output buffer to empty... ");
	if(RingBufFull(&outbuf)) {
#if !defined(RINGBUF_EVENT)
		ResetEvent(outbuf_empty);
#endif
		if(WaitForEvent(outbuf_empty,5000)!=WAIT_OBJECT_0)
			lprintf(LOG_DEBUG,"FAILURE");
	}
#endif

	terminate=TRUE;	/* stop output thread */
	/* Code disabled.  Why?  ToDo */
/*	sem_post(outbuf.sem);
	sem_post(outbuf.highwater_sem); */

	lprintf(LOG_INFO, "Exiting - Error level: %d, flows: %u, select_errors=%u"
		,retval, flows, select_errors);

	bail(retval);
	return retval;
}

