/* sexyz.c */

/* Synchronet External X/Y/ZMODEM Transfer Protocols */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2008 Rob Swindell - http://www.synchro.net/copyright.html		*
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
 * by Jacques Mattheij <jacquesm@hacktic.nl> 
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

/***************/
/* Global Vars */
/***************/
long	mode=0;							/* Program mode 					*/
long	zmode=0L;						/* Zmodem mode						*/
uchar	block[XMODEM_MAX_BLOCK_SIZE];					/* Block buffer 					*/
ulong	block_num;						/* Block number 					*/
char*	dszlog;
BOOL	dszlog_path=TRUE;				/* Log complete path to filename	*/
BOOL	dszlog_short=FALSE;				/* Log Micros~1 short filename		*/
BOOL	dszlog_quotes=FALSE;			/* Quote filenames in DSZLOG		*/
int		log_level=LOG_INFO;

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
struct termios origterm;
#endif
BOOL	terminate=FALSE;
BOOL	debug_tx=FALSE;
BOOL	debug_rx=FALSE;
BOOL	debug_telnet=FALSE;
BOOL	pause_on_exit=FALSE;
BOOL	pause_on_abend=FALSE;
BOOL	newline=TRUE;

time_t		progress_interval;

RingBuf		outbuf;
#if defined(RINGBUF_EVENT)
	#define		outbuf_empty outbuf.empty_event
#else
	xpevent_t	outbuf_empty;
#endif
unsigned	outbuf_drain_timeout;
long		outbuf_size;

unsigned	flows=0;
unsigned	select_errors=0;

#ifdef __unix__
void resetterm(void)
{
	tcsetattr(STDOUT_FILENO, TCSADRAIN, &origterm);
}
#endif

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

#if defined(_WIN32) && defined(_DEBUG)
	if(log_level==LOG_DEBUG)
		OutputDebugString(str);
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
		return fprintf(fp,"!%s\n",str);
	else
		return fprintf(fp,"%s\n",str);
}

static int lprintf(int level, const char *fmt, ...)
{
	char sbuf[1024];
	va_list argptr;

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
BOOL WINAPI ControlHandler(DWORD CtrlType)
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

void send_telnet_cmd(SOCKET sock, uchar cmd, uchar opt)
{
	uchar	buf[3];
	
	buf[0]=TELNET_IAC;
	buf[1]=cmd;
	buf[2]=opt;

	if(debug_telnet)
		lprintf(LOG_DEBUG,"Sending telnet command: %s %s"
			,telnet_cmd_desc(buf[1]),telnet_opt_desc(buf[2]));

	if(sendsocket(sock,buf,sizeof(buf))!=sizeof(buf) && debug_telnet)
		lprintf(LOG_ERR,"FAILED");
}

#define DEBUG_TELNET FALSE

/****************************************************************************/
/* Receive a byte from remote (single-threaded version)						*/
/****************************************************************************/
int recv_byte(void* unused, unsigned timeout)
{
	int			i;
	long		t;
	uchar		ch;
	fd_set		socket_set;
	time_t		end;
	struct timeval	tv;
	static uchar	telnet_cmd;
	static int		telnet_cmdlen;

	end=msclock()+(timeout*MSCLOCKS_PER_SEC);
	while(!terminate) {

		FD_ZERO(&socket_set);
#ifdef __unix__
		if(stdio)
			FD_SET(STDIN_FILENO,&socket_set);
		else
#endif
			FD_SET(sock,&socket_set);
		if((t=end-msclock())<0) t=0;
		tv.tv_sec=t/MSCLOCKS_PER_SEC;
		tv.tv_usec=0;

		if((i=select(sock+1,&socket_set,NULL,NULL,&tv))<1) {
			if(i==SOCKET_ERROR) {
				lprintf(LOG_ERR,"ERROR %d selecting socket", ERROR_VALUE);
			}
			if(timeout)
				lprintf(LOG_WARNING,"Receive timeout (%u seconds)", timeout);
			return(NOINP);
		}
		
#ifdef __unix__
		if(stdio)
			i=read(STDIN_FILENO,&ch,sizeof(ch));
		else
#endif
			i=recv(sock,&ch,sizeof(ch),0);

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
		fprintf(statfp,"FLOW");
		flows++;
		result=WaitForEvent(outbuf_empty,timeout*1000);
		fprintf(statfp,"\b\b\b\b    \b\b\b\b");
		if(result!=WAIT_OBJECT_0) {
			fprintf(statfp
				,"\n!TIMEOUT (%d) waiting for output buffer to flush (%u seconds, %u bytes)\n"
				,result, timeout, RingBufFull(&outbuf));
			newline=TRUE;
			if(RingBufFree(&outbuf)<len)
				return(-1);
		}
	}

	RingBufWrite(&outbuf,buf,len);
#if !defined(RINGBUF_EVENT)
	ResetEvent(outbuf_empty);
#endif

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
	fd_set		socket_set;
	struct timeval	tv;

	FD_ZERO(&socket_set);
#ifdef __unix__
	if(stdio)
		FD_SET(STDOUT_FILENO,&socket_set);
	else
#endif
		FD_SET(sock,&socket_set);
	tv.tv_sec=timeout;
	tv.tv_usec=0;

	if(select(sock+1,NULL,&socket_set,NULL,&tv)<1)
		return(ERROR_VALUE);

	if(telnet && ch==TELNET_IAC)	/* escape IAC char */
		len=2;
	else
		buf[0]=ch;

#ifdef __unix__
	if(stdio)
		i=write(STDOUT_FILENO,buf,len);
	else
#endif
		i=sendsocket(sock,buf,len);
	
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
	ulong		total_sent=0;
	ulong		total_pkts=0;
	ulong		short_sends=0;
    ulong		bufbot=0;
    ulong		buftop=0;
	fd_set		socket_set;
	struct timeval tv;

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

		/* Check socket for writability (using select) */
		tv.tv_sec=0;
		tv.tv_usec=1000;

		FD_ZERO(&socket_set);
#ifdef __unix__
		if(stdio)
			FD_SET(STDOUT_FILENO,&socket_set);
		else
#endif
			FD_SET(sock,&socket_set);

		i=select(sock+1,NULL,&socket_set,NULL,&tv);
		if(i==SOCKET_ERROR) {
			lprintf(LOG_ERR,"ERROR %d selecting socket %u for send"
				,ERROR_VALUE,sock);
			break;
		}
		if(i<1) {
			select_errors++;
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
#ifdef __unix__
		if(stdio)
			i=write(STDOUT_FILENO, (char*)buf+bufbot, buftop-bufbot);
		else
#endif
			i=sendsocket(sock, (char*)buf+bufbot, buftop-bufbot);
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
		sprintf(stats,"(sent %lu bytes in %lu blocks, %lu average, %lu short, %lu errors)"
			,total_sent, total_pkts, total_sent/total_pkts, short_sends, select_errors);
	else
		stats[0]=0;

	lprintf(LOG_DEBUG,"output thread terminated\n%s", stats);
}

BOOL is_connected(void* unused)
{
	return socket_check(sock,NULL,NULL,0);
}

BOOL data_waiting(void* unused, unsigned timeout)
{
	BOOL rd;

	if(!socket_check(sock,&rd,NULL,timeout))
		return(FALSE);
	return(rd);
}

/****************************************************************************/
/* Returns the total number of blocks required to send the file				*/
/****************************************************************************/
unsigned num_blocks(unsigned block_num, ulong offset, ulong len, unsigned block_size)
{
	ulong blocks;
	ulong remain = len - offset;

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

void xmodem_progress(void* unused, unsigned block_num, ulong offset, ulong fsize, time_t start)
{
	unsigned	cps;
	unsigned	total_blocks;
	long		l;
	long		t;
	time_t		now;
	static time_t last_progress;

	now=time(NULL);
	if(now-last_progress>=progress_interval || offset >= fsize || newline) {
		t=now-start;
		if(t<=0)
			t=1;
		if((cps=offset/t)==0)
			cps=1;			/* cps so far */
		l=fsize/cps;		/* total transfer est time */
		l-=t;				/* now, it's est time left */
		if(l<0) l=0;
		if(mode&SEND) {
			total_blocks=num_blocks(block_num,offset,fsize,xm.block_size);
			fprintf(statfp,"\rBlock (%lu%s): %lu/%lu  Byte: %lu  "
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
				,(long)(((float)offset/(float)fsize)*100.0)
				);
		} else if(mode&YMODEM) {
			fprintf(statfp,"\rBlock (%lu%s): %lu  Byte: %lu  "
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
				,(long)(((float)offset/(float)fsize)*100.0)
				);
		} else { /* XModem receive */
			fprintf(statfp,"\rBlock (%lu%s): %lu  Byte: %lu  "
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
void zmodem_progress(void* cbdata, ulong current_pos)
{
	unsigned	cps;
	long		l;
	long		t;
	time_t		now;
	static time_t last_progress;

	now=time(NULL);
	if(now-last_progress>=progress_interval || current_pos >= zm.current_file_size || newline) {
		t=now-zm.transfer_start_time;
		if(t<=0)
			t=1;
		if(zm.transfer_start_pos>current_pos)
			zm.transfer_start_pos=0;
		if((cps=(current_pos-zm.transfer_start_pos)/t)==0)
			cps=1;		/* cps so far */
		l=zm.current_file_size/cps;	/* total transfer est time */
		l-=t;			/* now, it's est time left */
		if(l<0) l=0;
		fprintf(statfp,"\rKByte: %lu/%lu  %u/CRC-%u  "
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
			,(long)(((float)current_pos/(float)zm.current_file_size)*100.0)
			);
		newline=FALSE;
		last_progress=now;
	}
}

static int send_files(char** fname, uint fnames)
{
	char	path[MAX_PATH+1];
	int		i;
	uint	errors;
	uint	fnum;
	uint	cps;
	glob_t	g;
	int		gi;
	BOOL	success=TRUE;
	long	fsize;
	ulong	sent_bytes;
	ulong	total_bytes=0;
	time_t	t,startfile;
	time_t	startall;
	FILE*	fp;

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
		lprintf(LOG_INFO,"Sending %u files (%lu KB total)"
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

			errors=0;
			success=FALSE;
			startfile=time(NULL);

			lprintf(LOG_INFO,"Sending %s (%lu KB) via %s"
				,path,fsize/1024
				,mode&XMODEM ? "Xmodem" : mode&YMODEM ? "Ymodem" : "Zmodem");

			if(mode&ZMODEM)
					success=zmodem_send_file(&zm, path, fp, /* ZRQINIT? */fnum==0, &startfile, &sent_bytes);
			else	/* X/Ymodem */
					success=xmodem_send_file(&xm, path, fp, &startfile, &sent_bytes);

			fclose(fp);

			if((t=time(NULL)-startfile)<=0) 
				t=1;
			if((cps=sent_bytes/t)==0)
				cps=1;
			if(success) {
				xm.sent_files++;
				xm.sent_bytes+=fsize;
				lprintf(LOG_INFO,"Successful - Time: %lu:%02lu  CPS: %lu"
						,t/60,t%60,cps);

				if(xm.total_files-xm.sent_files)
					lprintf(LOG_INFO,"Remaining - Time: %lu:%02lu  Files: %u  KBytes: %lu"
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
				fprintf(logfp,"%c %7lu %5u bps %6lu cps %3u errors %5u %4u "
					"%s -1\n"
					,success ? (mode&ZMODEM ? 'z':'S') 
						: (mode&ZMODEM && zm.file_skipped) ? 's' 
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

			if(xm.cancelled || zm.cancelled)
				break;

		} /* while(gi<(int)g.gl_pathc) */

		if(gi<(int)g.gl_pathc)/* error occurred */
			break;
	}

	if(mode&ZMODEM && !zm.cancelled && is_connected(NULL))
		zmodem_get_zfin(&zm);

	if(fnum<fnames) /* error occurred */
		return(-1);

	if(!success)
		return(-1);

	if(mode&XMODEM)
		return(0);
	if(mode&YMODEM) {

		if(xmodem_get_mode(&xm)) {

			lprintf(LOG_INFO,"Sending Ymodem termination block");

			memset(block,0,XMODEM_MIN_BLOCK_SIZE);	/* send short block for terminator */
			xmodem_put_block(&xm, block, XMODEM_MIN_BLOCK_SIZE /* block_size */, 0 /* block_num */);
			if(!xmodem_get_ack(&xm,6,0)) {
				lprintf(LOG_WARNING,"Failed to receive ACK after terminating block"); 
			} 
		}
	}
	if(xm.total_files>1) {
		t=time(NULL)-startall;
		if(!t) t=1;
		lprintf(LOG_INFO,"Overall - Time %02lu:%02lu  KBytes: %lu  CPS: %lu"
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
	ulong	file_bytes=0,file_bytes_left=0;
	ulong	total_bytes=0;
	FILE*	fp;
	time_t	t,startfile,ftime;

	if(fnames>1)
		lprintf(LOG_INFO,"Receiving %u files",fnames);

	outbuf.highwater_mark=0;	/* don't delay ACK/NAK transmits */

	/* Purge input buffer */
	while(is_connected(NULL) && (i=recv_byte(NULL,0))!=NOINP)
		lprintf(LOG_WARNING,"Throwing out received: %s",chr((uchar)i));

	while(is_connected(NULL)) {
		if(mode&XMODEM) {
			SAFECOPY(str,fname_list[0]);	/* we'll have at least one fname */
			file_bytes=file_bytes_left=0x7fffffff;
		}

		else {
			if(mode&YMODEM) {
				lprintf(LOG_INFO,"Fetching Ymodem header block");
				for(errors=0;errors<=xm.max_errors && !xm.cancelled;errors++) {
					if(errors>(xm.max_errors/2) && mode&CRC && !(mode&GMODE))
						mode&=~CRC;
					xmodem_put_nak(&xm, /* expected_block: */ 0);
					if(xmodem_get_block(&xm, block, /* expected_block: */ 0) == 0) {
						send_byte(NULL,ACK,10);
						break; 
					} 
				}
				if(errors>=xm.max_errors || xm.cancelled) {
					lprintf(LOG_ERR,"Error fetching Ymodem header block");
					xmodem_cancel(&xm);
					return(1); 
				}
				if(!block[0]) {
					lprintf(LOG_INFO,"Received Ymodem termination block");
					return(0); 
				}
				file_bytes=ftime=total_files=total_bytes=0;
				i=sscanf(block+strlen(block)+1,"%ld %lo %lo %lo %d %ld"
					,&file_bytes			/* file size (decimal) */
					,&ftime 				/* file time (octal unix format) */
					,&fmode 				/* file mode (not used) */
					,&serial_num			/* program serial number */
					,&total_files			/* remaining files to be sent */
					,&total_bytes			/* remaining bytes to be sent */
					);
				lprintf(LOG_DEBUG,"Ymodem header (%u fields): %s", i, block+strlen(block)+1);
				SAFECOPY(fname,block);

			} else {	/* Zmodem */
				lprintf(LOG_INFO,"Waiting for Zmodem sender...");

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

			if(!file_bytes)
				file_bytes=0x7fffffff;
			file_bytes_left=file_bytes;
			if(!total_files)
				total_files=fnames-fnum;
			if(!total_files)
				total_files=1;
			if(total_bytes<file_bytes)
				total_bytes=file_bytes;

			lprintf(LOG_DEBUG,"Incoming filename: %.64s ",fname);

			if(mode&RECVDIR)
				sprintf(str,"%s%s",fname_list[0],getfname(fname));
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
			fprintf(statfp,"File size: %lu bytes\n", file_bytes);
			if(total_files>1)
				fprintf(statfp,"Remaining: %lu bytes in %u files\n", total_bytes, total_files);
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
			lprintf(LOG_INFO,"Receiving %s via Xmodem %s"
				,str
				,mode&CRC ? "CRC-16":"Checksum");
		else
			lprintf(LOG_INFO,"Receiving %s (%lu KB) via %s %s"
				,str
				,file_bytes/1024
				,mode&YMODEM ? mode&GMODE ? "Ymodem-G" : "Ymodem" :"Zmodem"
				,mode&ZMODEM ? "" : (mode&CRC ? "CRC-16" : "Checksum"));

		startfile=time(NULL);
		success=FALSE;
		if(mode&ZMODEM) {

			errors=zmodem_recv_file_data(&zm,fp,0);

			/*
 			 * wait for the eof header
			 */

			for(;errors<=zm.max_errors && !success && !zm.cancelled; errors++) {
				if(zmodem_recv_header_and_check(&zm))
					success=TRUE;
			} 

		} else {
			errors=0;
			block_num=1;
			xmodem_put_nak(&xm, block_num);
			while(is_connected(NULL)) {
				xmodem_progress(NULL,block_num,ftell(fp),file_bytes,startfile);
				i=xmodem_get_block(&xm, block, block_num); 	

				if(i!=0) {
					if(i==EOT)	{		/* end of transfer */
						success=TRUE;
						xmodem_put_ack(&xm);
						break;
					}
					if(i==CAN) {		/* Cancel */
						xm.cancelled=TRUE;
						break;
					}

					if(mode&GMODE)
						return(-1);

					if(++errors>=xm.max_errors) {
						lprintf(LOG_ERR,"Too many errors (%u)",errors);
						xmodem_cancel(&xm);
						break;
					}
					if(block_num==1 && errors>(xm.max_errors/2) && mode&CRC && !(mode&GMODE))
						mode&=~CRC;
					xmodem_put_nak(&xm, block_num);
					continue;
				}
				if(!(mode&GMODE))
					send_byte(NULL,ACK,10);
				if(file_bytes_left<=0L)  { /* No more bytes to send */
					lprintf(LOG_WARNING,"Attempt to send more byte specified in header");
					break; 
				}
				wr=xm.block_size;
				if(wr>file_bytes_left)
					wr=file_bytes_left;
				if(fwrite(block,1,wr,fp)!=wr) {
					lprintf(LOG_ERR,"Error writing %u bytes to file at offset %lu"
						,wr,ftell(fp));
					xmodem_cancel(&xm);
					return(1); 
				}
				file_bytes_left-=wr; 
				block_num++;
			}
		}

		/* Use correct file size */
		fflush(fp);
		if(file_bytes < (ulong)filelength(fileno(fp))) {
			lprintf(LOG_INFO,"Truncating file to %lu bytes", file_bytes);
			chsize(fileno(fp),file_bytes);
		} else
			file_bytes = filelength(fileno(fp));
		fclose(fp);
		
		t=time(NULL)-startfile;
		if(!t) t=1;
		if(success)
			lprintf(LOG_INFO,"Successful - Time: %lu:%02lu  CPS: %lu"
				,t/60,t%60,file_bytes/t);	
		else
			lprintf(LOG_ERR,"File Transfer %s", zm.local_abort ? "Aborted":"Failure");

		if(!(mode&XMODEM) && ftime)
			setfdate(str,ftime); 

		if(logfp) {
			lprintf(LOG_DEBUG,"Updating DSZLOG: %s", dszlog);
			fprintf(logfp,"%c %6lu %5u bps %4lu cps %3u errors %5u %4u "
				"%s %d\n"
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
				zmodem_abort_receive(&zm);
			xm.cancelled=FALSE;
			xmodem_cancel(&xm);
			break;
		}

		if(mode&XMODEM)	/* maximum of one file */
			break;
		if((cps=file_bytes/t)==0)
			cps=1;
		total_files--;
		total_bytes-=file_bytes;
		if(total_files>1 && total_bytes)
			lprintf(LOG_INFO,"Remaining - Time: %lu:%02lu  Files: %u  KBytes: %lu"
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
	if(logfp!=NULL)
		fclose(logfp);
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
	"opts   = -y  to overwrite files when receiving\n"
	"         -o  disable Zmodem CRC-32 mode (use CRC-16)\n"
	"         -s  disable Zmodem streaming (Slow Zmodem)\n"
	"         -2  set maximum Zmodem block size to 2K\n"
	"         -4  set maximum Zmodem block size to 4K\n"
	"         -8  set maximum Zmodem block size to 8K (ZedZap)\n"
	"         -!  to pause after abnormal exit (error)\n"
	"         -telnet to enable Telnet mode (the default)\n"
	"         -rlogin or -ssh or -raw to disable Telnet mode\n"
	"\n"
	"cmd    = v  to display detailed version information\n"
	"         sx to send Xmodem     rx to recv Xmodem\n"
	"         sX to send Xmodem-1K  rc to recv Xmodem-CRC\n"
	"         sy to send Ymodem     ry to recv Ymodem\n"
	"         sY to send Ymodem-1K  rg to recv Ymodem-G\n"
	"         sz to send Zmodem     rz to recv Zmodem\n"
	"\n"
	"file   = filename to send or receive\n"
	"path   = directory to receive files into\n"
	"list   = name of text file with list of filenames to send or receive\n";

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
	str_list_t fname_list;

	fname_list=strListInit();

	DESCRIBE_COMPILER(compiler);

	errfp=stderr;
#ifdef __unix__
	statfp=stderr;
#else
	statfp=stdout;
#endif

	sscanf("$Revision$", "%*s %s", revision);

	fprintf(statfp,"\nSynchronet External X/Y/Zmodem  v%s-%s"
		"  Copyright %s Rob Swindell\n\n"
		,revision
		,PLATFORM_DESC
		,__DATE__+7
		);

	xmodem_init(&xm,NULL,&mode,lputs,xmodem_progress,send_byte,recv_byte,is_connected,NULL);
	zmodem_init(&zm,NULL,lputs,zmodem_progress,send_byte,recv_byte,is_connected,NULL,data_waiting);

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

	outbuf.highwater_mark	=iniReadInteger(fp,ROOT_SECTION,"OutbufHighwaterMark",1100);
	outbuf_drain_timeout	=iniReadInteger(fp,ROOT_SECTION,"OutbufDrainTimeout",10);
	outbuf_size				=iniReadInteger(fp,ROOT_SECTION,"OutbufSize",16*1024);

	progress_interval		=iniReadInteger(fp,ROOT_SECTION,"ProgressInterval",1);

	if(iniReadBool(fp,ROOT_SECTION,"Debug",FALSE))
		log_level=LOG_DEBUG;

	xm.send_timeout			=iniReadInteger(fp,"Xmodem","SendTimeout",xm.send_timeout);	/* seconds */
	xm.recv_timeout			=iniReadInteger(fp,"Xmodem","RecvTimeout",xm.recv_timeout);	/* seconds */
	xm.byte_timeout			=iniReadInteger(fp,"Xmodem","ByteTimeout",xm.byte_timeout);	/* seconds */
	xm.ack_timeout			=iniReadInteger(fp,"Xmodem","AckTimeout",xm.ack_timeout);	/* seconds */
	xm.block_size			=iniReadInteger(fp,"Xmodem","BlockSize",xm.block_size);		/* 128 or 1024 */
	xm.max_errors			=iniReadInteger(fp,"Xmodem","MaxErrors",xm.max_errors);
	xm.g_delay				=iniReadInteger(fp,"Xmodem","G_Delay",xm.g_delay);

	zm.init_timeout			=iniReadInteger(fp,"Zmodem","InitTimeout",zm.init_timeout);	/* seconds */
	zm.send_timeout			=iniReadInteger(fp,"Zmodem","SendTimeout",zm.send_timeout);	/* seconds */
	zm.recv_timeout			=iniReadInteger(fp,"Zmodem","RecvTimeout",zm.recv_timeout);	/* seconds */
	zm.crc_timeout			=iniReadInteger(fp,"Zmodem","CrcTimeout",zm.crc_timeout);	/* seconds */
	zm.block_size			=iniReadInteger(fp,"Zmodem","BlockSize",zm.block_size);			/* 1024  */
	zm.max_block_size		=iniReadInteger(fp,"Zmodem","MaxBlockSize",zm.max_block_size);	/* 1024 or 8192 */
	zm.max_errors			=iniReadInteger(fp,"Zmodem","MaxErrors",zm.max_errors);
	zm.recv_bufsize			=iniReadInteger(fp,"Zmodem","RecvBufSize",0);
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
	
	fprintf(statfp,"Output buffer size: %u\n", outbuf_size);
	RingBufInit(&outbuf, outbuf_size);

#if !defined(RINGBUF_EVENT)
	outbuf_empty=CreateEvent(NULL,/* ManualReset */TRUE, /*InitialState */TRUE,NULL);
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
						bail(1); 
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
			}

			arg=argv[i];
			if(*arg=='-') {
				while(*arg=='-')
					arg++;
				if(stricmp(arg,"telnet")==0) {
					telnet=TRUE;
					continue;
				}
				if(stricmp(arg,"rlogin")==0 || stricmp(arg,"ssh")==0 || stricmp(arg,"raw")==0) {
					telnet=FALSE;
					continue;
				}
				if(stricmp(arg,"debug")==0) {
					log_level=LOG_DEBUG;
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
					case 'G':	/* Ymodem-G */
						mode|=GMODE;
						break;
					case 'Y':
						mode|=OVERWRITE;
						break;
					case '!':
						pause_on_abend=TRUE;
						break;
				}
			}
		}

		else if((argv[i][0]=='+' || argv[i][0]=='@') && fexist(argv[i]+1)) {
			if(mode&RECVDIR) {
				fprintf(statfp,"!Cannot specify both directory and filename\n");
				bail(1); 
			}
			sprintf(str,"%s",argv[i]+1);
			if((fp=fopen(str,"r"))==NULL) {
				fprintf(statfp,"!Error %d opening filelist: %s\n",errno,str);
				bail(1); 
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
				}
				if(fnames) {
					fprintf(statfp,"!Cannot specify both directory and filename\n");
					bail(1); 
				}
				if(mode&SEND) {
					fprintf(statfp,"!Cannot send directory '%s'\n",argv[i]);
					bail(1);
				}
				mode|=RECVDIR; 
			}
			strListAppend(&fname_list,argv[i],fnames++);
		} 
	}

	if(!telnet)
		zm.escape_telnet_iac = FALSE;

	if(sock==INVALID_SOCKET || sock<1) {
#ifdef __unix__
		if(STDOUT_FILENO > STDIN_FILENO)
			sock=STDOUT_FILENO;
		else
			sock=STDIN_FILENO;
		stdio=TRUE;
		
		fprintf(statfp,"No socket descriptor specified, using STDIO\n");
		telnet=FALSE;
#else
		fprintf(statfp,"!No socket descriptor specified\n\n");
		fprintf(errfp,usage);
		bail(1);
#endif
	}
#ifdef __unix__
	else
		statfp=stdout;
#endif

	if(!(mode&(SEND|RECV))) {
		fprintf(statfp,"!No command specified\n\n");
		fprintf(statfp,usage);
		bail(1); 
	}

	if(mode&(SEND|XMODEM) && !fnames) { /* Sending with any or recv w/Xmodem */
		fprintf(statfp,"!Must specify filename or filelist\n\n");
		fprintf(statfp,usage);
		bail(1); 
	}

#ifdef __unix__
	if(stdio) {
		struct termios term;
		memset(&term,0,sizeof(term));
		cfsetispeed(&term,B19200);
		cfsetospeed(&term,B19200);
		term.c_iflag &= ~(IMAXBEL|IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
		term.c_oflag &= ~OPOST;
		term.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
		term.c_cflag &= ~(CSIZE|PARENB);
		term.c_cflag |= CS8;
		atexit(resetterm);
		tcgetattr(STDOUT_FILENO, &origterm);
		tcsetattr(STDOUT_FILENO, TCSADRAIN, &term);
	}
#endif

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
		setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,(char*)&tcp_nodelay,sizeof(tcp_nodelay));
#ifdef __unix__
	}
#endif

	if(!socket_check(sock, NULL, NULL, 0)) {
		lprintf(LOG_WARNING,"No socket connection");
		bail(-1); 
	}

	if((dszlog=getenv("DSZLOG"))!=NULL) {
		if((logfp=fopen(dszlog,"w"))==NULL) {
			lprintf(LOG_WARNING,"Error %d opening DSZLOG file: %s",errno,dszlog);
			bail(-1); 
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
	if(WaitForEvent(outbuf_empty,5000)!=WAIT_OBJECT_0)
		lprintf(LOG_DEBUG,"FAILURE");
#endif

	terminate=TRUE;	/* stop output thread */
	/* Code disabled.  Why?  ToDo */
/*	sem_post(outbuf.sem);
	sem_post(outbuf.highwater_sem); */

	fprintf(statfp,"Exiting - Error level: %d, flows: %u, select_errors=%u"
		,retval, flows, select_errors);
	fprintf(statfp,"\n");

	bail(retval);
}

