/* sexyz.c */

/* Synchronet External X/Y/ZMODEM Transfer Protocols */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#include <sys/stat.h>

#include "conwrap.h"
#include "genwrap.h"
#include "dirwrap.h"
#include "filewrap.h"
#include "sockwrap.h"

#include "telnet.h"

#include "sexyz.h"

#define LOOP_NOPEN	50

#define MAX_FNAMES	100 	/* Up to 100 filenames						*/

/************************/
/* Remote I/O Constants */
/************************/

							/* i/o mode and state flags */

void cancel(void);

/***************/
/* Global Vars */
/***************/
long	mode=PAUSE_ABEND|TELNET;		/* Program mode 					*/
long	zmode=0L;						/* Zmodem mode						*/
uchar	block[1024];					/* Block buffer 					*/
int	block_size; 						/* Block size (128 or 1024) 		*/
ulong	block_num;						/* Block number 					*/
ulong	last_block_num; 				/* Last block number sent			*/
uint	flows=0;						/* Number of flow controls			*/
time_t	startall;

FILE*	errfp;
FILE*	statfp;

char	revision[16];

SOCKET	sock=INVALID_SOCKET;
#define DCDHIGH		socket_check(sock, NULL, NULL, 0)

#define getcom(t)	recv_byte(sock,t,mode)
#define putcom(ch)	send_byte(sock,ch,10,mode)


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
	fprintf(statfp,"\n");
}

/****************************************************************************/
/* Truncates white-space chars off end of 'str' and terminates at first tab */
/****************************************************************************/
void truncsp(char *str)
{
	char c;

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
	fprintf(statfp,"Exiting - Error level: %d",code);
	if(flows)
		fprintf(statfp,"  Flow restraint count: %u",flows);
	fprintf(statfp,"\n");

	if(/* code && */ mode&PAUSE_ABEND) {
		printf("Hit enter to continue...");
		getchar();
	}

	exit(code);
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
			if(ch>=' ' && ch<='~')
				sprintf(str,"'%c' (%02Xh)",ch,ch);
			else
				sprintf(str,"%u (%02Xh)",ch,ch);
			return(str); 
	}
}

void send_telnet_cmd(SOCKET sock, uchar cmd, uchar opt)
{
	uchar buf[3];
	
	buf[0]=TELNET_IAC;
	buf[1]=cmd;
	buf[2]=opt;

	fprintf(statfp,"\nSending telnet command: %s %s"
		,telnet_cmd_desc(buf[1]),telnet_opt_desc(buf[2]));
	if(send(sock,buf,sizeof(buf),0)==sizeof(buf))
		fprintf(statfp,"\n");
	else
		fprintf(statfp," FAILED!\n");
}

#define DEBUG_TELNET FALSE

/****************************************************************************/
/* Receive a byte from remote												*/
/****************************************************************************/
uint recv_byte(SOCKET sock, int timeout, long mode)
{
	int			i;
	uchar		ch;
	fd_set		socket_set;
	struct timeval	tv;
	static uchar	telnet_cmd;
	static int		telnet_cmdlen;

	while(1) {

		FD_ZERO(&socket_set);
		FD_SET(sock,&socket_set);
		tv.tv_sec=timeout;
		tv.tv_usec=0;

		if(select(sock+1,&socket_set,NULL,NULL,&tv)<1) {
			if(timeout) {
				newline();
				fprintf(statfp,"Input timeout\n");
			}
			return(NOINP);
		}
		
		i=recv(sock,&ch,sizeof(ch),0);

		if(i!=sizeof(ch)) {
			newline();
			if(i==0)
				fprintf(statfp,"No carrier\n");
			else
				fprintf(statfp,"!recv error %d (%d)\n",i,ERROR_VALUE);
			bail(1); 
		}

		if(mode&TELNET) {
			if(ch==TELNET_IAC) {
#if DEBUG_TELNET
				fprintf(statfp,"T<%s> ",telnet_cmd_desc(ch));
#endif
				if(telnet_cmdlen==0) {
					telnet_cmdlen=1;
					continue;
				}
				if(telnet_cmdlen==1) {
					telnet_cmdlen=0;
					return(TELNET_IAC);
				}
			}
			if(telnet_cmdlen) {
				telnet_cmdlen++;
#if DEBUG_TELNET
				if(telnet_cmdlen==2)
					fprintf(statfp,"T<%s> ",telnet_cmd_desc(ch));
				else
					fprintf(statfp,"T<%s> ",telnet_opt_desc(ch));
#endif
				if(telnet_cmdlen==3 && telnet_cmd==TELNET_DO)
					send_telnet_cmd(sock, TELNET_WILL,ch);
	/*
				else if(telnet_cmdlen==3 && telnet_cmd==TELNET_WILL)
					send_telnet_cmd(sock, TELNET_DO,ch);
	*/
				telnet_cmd=ch;
				if((telnet_cmdlen==2 && ch<TELNET_WILL) || telnet_cmdlen>2)
					telnet_cmdlen=0;
				continue;
			}
		}
		return(ch);
	}

	return(NOINP);
}
	
/*************************/
/* Send a byte to remote */
/*************************/
int send_byte(SOCKET sock, uchar ch, int timeout, long mode)
{
	uchar		buf[2] = { TELNET_IAC, TELNET_IAC };
	int			len=1;
	int			i;
	fd_set		socket_set;
	struct timeval	tv;

	FD_ZERO(&socket_set);
	FD_SET(sock,&socket_set);
	tv.tv_sec=timeout;
	tv.tv_usec=0;

	if(select(sock+1,NULL,&socket_set,NULL,&tv)<1)
		return(ERROR_VALUE);

	if(mode&TELNET && ch==TELNET_IAC)	/* escape IAC char */
		len=2;
	else
		buf[0]=ch;

	i=send(sock,buf,len,0);
	
	if(i==len)
		return(0);

	return(-1);
}

int send_str(SOCKET sock, char* str, int timeout, long mode)
{
	char*	p;
	int		i;

	for(p=str;*p;p++) {
		if((i=send_byte(sock,*p,timeout,mode))!=0)
			return(i);
	}
	return(0);
}


/****************************************************************************/
/* Returns the number of blocks required to send len bytes					*/
/****************************************************************************/
long num_blocks(long len, long block_size)
{
	long blocks;

	blocks=len/block_size;
	if(len%block_size)
		blocks++;
	return(blocks);
}

/************************************************/
/* Dump the current block contents - for debug  */
/************************************************/
void dump_block()
{
	int l;

	for(l=0;l<block_size;l++)
		fprintf(statfp,"%02X  ",block[l]);
	fprintf(statfp,"\n");
}

void send_files(char** fname, uint fnames, FILE* log)
{
	char	path[MAX_PATH+1];
	int		ch;
	int		i;
	uint	errors;
	uint	fnum;
	uint	cps;
	glob_t	g;
	int		gi;
	BOOL	can;
	BOOL	success;
	long	b,l;
	long	fsize;
	uint	total_files=0,sent_files=0;
	ulong	total_bytes=0,sent_bytes=0;
	time_t	t,startfile;
	FILE*	fp;
	xmodem_t xm;
	zmodem_t zm;

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

	memset(&xm,0,sizeof(xm));
	xm.sock=sock;
	xm.mode=mode;

	memset(&zm,0,sizeof(zm));
	zm.sock=sock;
	zm.mode=mode;
	zm.n_files_remaining = total_files;
	zm.n_bytes_remaining = total_bytes;

	/***********************************************/
	/* Send every file matching names or filespecs */
	/***********************************************/
	for(fnum=0;fnum<fnames;fnum++) {
		if(glob(fname[fnum],0,NULL,&g)) {
			fprintf(statfp,"%s not found\n",fname[fnum]);
			continue;
		}
		for(gi=0;gi<(int)g.gl_pathc;gi++) {
			SAFECOPY(path,g.gl_pathv[gi]);
			if(isdir(path))
				continue;

			if((fp=fopen(path,"rb"))==NULL) {
				fprintf(statfp,"!Error %d opening %s for read\n",errno,path);
				continue;
			}

			if(mode&ZMODEM) {

				for(errors=0;errors<MAXERRORS;errors++) {
					fprintf(statfp,"\nSending ZRQINIT\n");
					i = zmodem_get_zrinit(&zm);
					if(i == ZRINIT) {
						zmodem_parse_zrinit(&zm);
						break;
					}
					fprintf(statfp,"\n!RX header: %d 0x%02X\n", i, i);
				}

			} else {	/* X/Ymodem */

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
							,chr((uchar)i)); 
					} 
				}
			}

			if(errors==MAXERRORS) {
				fprintf(statfp,"\n!Timeout waiting for receiver to start/accept file transfer\n");
				xmodem_cancel(&xm);
				bail(1); 
			} 

			fsize=filelength(fileno(fp));

			fprintf(statfp,"\nSending %s (%lu bytes) via %s %s\n"
				,path,fsize
				,mode&XMODEM ? "Xmodem" : mode&YMODEM ? mode&GMODE ? "Ymodem-G"
					: "Ymodem" : "Zmodem"
				,mode&ZMODEM ? (zm.can_fcs_32 ? "CRC-32" : "CRC-16")
					: mode&CRC ? "CRC-16":"Checksum");

			errors=0;
			success=0;
			startfile=time(NULL);

			if(mode&ZMODEM) {
				if(zmodem_send_file(&zm,getfname(path),fp)==0) {
					sent_files++;
					sent_bytes+=fsize;

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

			} else {	/* X/Ymodem */

				if(!(mode&XMODEM)) {
					t=fdate(path);
					memset(block,0,sizeof(block));
					SAFECOPY(block,getfname(path));
					sprintf(block+strlen(block)+1,"%lu %lo 0 0 %d %ld"
						,fsize,t,total_files-sent_files,total_bytes-sent_bytes);
					/*
					fprintf(statfp,"Sending Ymodem block '%s'\n",block+strlen(block)+1);
					*/
					for(errors=0;errors<MAXERRORS;errors++) {
						xmodem_put_block(&xm, block, 128 /* block_size */, 0 /* block_num */);
						if(xmodem_get_ack(&xm,1))
							break; 
					}
					if(errors==MAXERRORS) {
						newline();
						fprintf(statfp,"Failed to send header block\n");
						xmodem_cancel(&xm);
						bail(1); 
					}
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
								,chr((uchar)i)); 
						} 
					}
					if(errors==MAXERRORS) {
						newline();
						fprintf(statfp,"Too many errors waiting for receiver\n");
						xmodem_cancel(&xm);
						bail(1); 
					} 
				}
				last_block_num=block_num=1;
				errors=0;
				while((block_num-1)*block_size<(ulong)fsize && errors<MAXERRORS) {
					if(last_block_num==block_num) {  /* block_num didn't increment */
						fseek(fp,(block_num-1)*(long)block_size,SEEK_SET);
						i=fread(block,1,block_size,fp);
						while(i<block_size)
							block[i++]=CPMEOF; 
					}
					last_block_num=block_num;
					xmodem_put_block(&xm, block, block_size, block_num);
					i=fread(block,1,block_size,fp); /* read next block from disk */
					while(i<(int)block_size)
						block[i++]=CPMEOF;
					t=time(NULL)-startfile;
					if(!t) t=1; 		/* t is time so far */
					cps=(uint)((block_num*(long)block_size)/t); 	/* cps so far */
					if(!cps) cps=1;
					l=fsize/cps;	/* total transfer est time */
					l-=t;				/* now, it's est time left */
					if(l<0) l=0;
					b=num_blocks(fsize,block_size);
					fprintf(statfp,"\rBlock (%lu%s): %lu/%lu  Byte: %lu  "
						"Time: %lu:%02lu/%lu:%02lu  CPS: %u  %lu%% "
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
					if(!xmodem_get_ack(&xm,5))
						errors++;
					else
						block_num++; 
				}
				fclose(fp);
				if((long)(block_num-1)*(long)block_size>=fsize) {
					sent_files++;
					sent_bytes+=fsize;
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
								,chr((uchar)ch)); 
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
			}

			if(total_files>1 && total_files-sent_files>1)
				fprintf(statfp,"Remaining - Time: %lu:%02lu  Files: %u  Bytes: %lu\n"
					,((total_bytes-sent_bytes)/cps)/60
					,((total_bytes-sent_bytes)/cps)%60
					,total_files-sent_files
					,total_bytes-sent_bytes
					);

			/* DSZLOG entry */
			if(log) {
				if(mode&ZMODEM)
					l=fsize;
				else {
					l=(block_num-1)*(long)block_size;
					if(l>fsize)
						l=fsize;
				}
				fprintf(log,"%c %6lu %5u bps %4lu cps %3u errors %5u %4u "
					"%s -1\n"
					,success ? (mode&ZMODEM ? 'z':'S') : 'E'
					,l
					,30000 /* baud */
					,l/t
					,errors
					,flows
					,block_size
					,path); 
			}
		} 
	}
	if(mode&XMODEM)
		bail(0);
	if(mode&ZMODEM)
		zmodem_send_zfin(&zm);
	else {	/* YMODEM */
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
			fprintf(statfp,"Received %s  Expected NAK, C, or G\n",chr((uchar)i)); 
		}
		else if(i!=NOINP) {
			block[0]=0;
			xmodem_put_block(&xm, block, 128 /* block_size */, 0 /* block_num */);
			if(!xmodem_get_ack(&xm,6)) {
				newline();
				fprintf(statfp,"Failed to receive ACK after terminating block\n"); 
			} 
		}
	}
	if(total_files>1) {
		t=time(NULL)-startall;
		if(!t) t=1;
		newline();
		fprintf(statfp,"Overall - Time %02lu:%02lu  Bytes: %lu  CPS: %lu\n"
			,t/60,t%60,sent_bytes,sent_bytes/t); 
	}
}

void receive_files(char** fname, int fnames, FILE* log)
{
	char	str[MAX_PATH+1];
	int		i;
	int		fnum=0;
	uint	errors;
	uint	total_files;
	uint	cps;
	uint	hdr_block_num;
	long	b,l,m;
	long	serial_num;
	ulong	file_bytes=0,file_bytes_left=0;
	ulong	total_bytes=0;
	FILE*	fp;
	time_t	t,startfile,ftime;
	xmodem_t xm;
	zmodem_t zm;

	if(fnames>1)
		fprintf(statfp,"Receiving %u files\n",fnames);

	memset(&xm,0,sizeof(xm));
	xm.sock=sock;
	xm.mode=mode;

	memset(&zm,0,sizeof(zm));
	zm.sock=sock;
	zm.mode=mode;

	while(1) {
		if(mode&XMODEM) {
			SAFECOPY(str,fname[0]);
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
				if(xmodem_get_block(&xm, block,block_size,TRUE)==0) { 	 /* block received successfully */
					putcom(ACK);
					break; 
				} 
			}
			if(errors==MAXERRORS) {
				fprintf(statfp,"Error fetching Ymodem header block\n");
				xmodem_cancel(&xm);
				bail(1); 
			}
			if(!block[0]) {
				fprintf(statfp,"Received Ymodem termination block\n");
				bail(0); 
			}
			sscanf(block+strlen(block)+1,"%ld %lo %lo %lo %d %ld"
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
				SAFECOPY(str,getfname(block));
				for(i=0;i<fnames;i++) {
					if(!fname[i][0])	/* name blank or already used */
						continue;
					if(!stricmp(getfname(fname[i]),str)) {
						SAFECOPY(str,fname[i]);
						fname[i][0]=0;
						break; 
					} 
				}
				if(i==fnames) { 				/* Not found in list */
					if(fnames)
						fprintf(statfp," - Not in receive list!");
					if(!fnames || fnum>=fnames || !fname[fnum][0])
						SAFECOPY(str,getfname(block));	/* worst case */
					else {
						SAFECOPY(str,fname[fnum]);
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
			xmodem_cancel(&xm);
			break; 
		}

		if(fexist(str) && !(mode&OVERWRITE)) {
			fprintf(statfp,"%s already exists\n",str);
			xmodem_cancel(&xm);
			bail(1); 
		}
		if((fp=fopen(str,"wb"))==NULL) {
			fprintf(statfp,"Error creating %s\n",str);
			xmodem_cancel(&xm);
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
			i=xmodem_get_block(&xm, block,block_size,FALSE); 	/* returns block num */
			if(i<0) {
				if(i==-EOT)			/* end of transfer */
					break;
				/* other error */
				xmodem_cancel(&xm);
				bail(1); 
			}
			hdr_block_num=i;
			if(file_bytes_left<=0L)  { /* No more bytes to send */
				newline();
				fprintf(statfp,"Attempt to send more than header specified\n");
				break; 
			}
			if(hdr_block_num==(uchar)((block_num+1)&0xff)) {	/* correct block */
				block_num++;
				if(file_bytes_left<(ulong)block_size) {
					if(fwrite(block,1,file_bytes_left,fp)
						!=file_bytes_left) {
						newline();
						fprintf(statfp,"Error writing to file\n");
						xmodem_cancel(&xm);
						bail(1); 
					} 
				}
				else {
					if(fwrite(block,1,block_size,fp)
						!=(uint)block_size) {
						newline();
						fprintf(statfp,"Error writing to file\n");
						xmodem_cancel(&xm);
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
			b=num_blocks(file_bytes, block_size);
			if(mode&YMODEM)
				fprintf(statfp,"\rBlock (%lu%s): %lu/%lu  Byte: %lu  Time: %lu:%02lu/"
					"%lu:%02lu  CPS: %u  %lu%% "
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
		/* Use correct file size */
		fflush(fp);
		if(file_bytes < (ulong)filelength(fileno(fp)));
			chsize(fileno(fp),file_bytes);
		fclose(fp);
		t=time(NULL)-startfile;
		if(!t) t=1;
		l=(block_num-1)*block_size;
		if(l>(long)file_bytes)
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
}

static const char* usage=
	"usage: sexyz <socket> [opts] <cmd> [file | path | +list]\n\n"
	"socket = TCP socket descriptor\n"
	"opts   = o  to overwrite files when receiving\n"
	"         d  to disable dropped carrier detection\n"
	"         a  to sound alarm at start and stop of transfer\n"
	"         p  to pause after abnormal exit (error)\n"
	"         l  to disable local keyboard (Ctrl-C) checking\n"
	"cmd    = v  to display detailed version information\n"
	"         sx to send Xmodem     rx to recv Xmodem\n"
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
	char	str[256],*p
			,*fname[MAX_FNAMES];
	int 	i;
	uint	fnames=0;
	FILE*	fp;
	FILE*	log=NULL;
	char	compiler[32];

	DESCRIBE_COMPILER(compiler);

	errfp=stderr;
	statfp=stdout;

	sscanf("$Revision$", "%*s %s", revision);

	fprintf(statfp,"\nSynchronet External X/Y/Zmodem  v%s-%s"
		"  Copyright 2003 Rob Swindell\n\n"
		,revision
		,PLATFORM_DESC
		);

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

			else if(toupper(argv[i][0])=='V') {

				fprintf(statfp,"%-8s %s\n",getfname(__FILE__),revision);
				fprintf(statfp,"%-8s %s\n",getfname(xmodem_source()),xmodem_ver(str));
				fprintf(statfp,"%-8s %s\n",getfname(zmodem_source()),zmodem_ver(str));
#ifdef _DEBUG
				fprintf(statfp,"Debug\n");
#endif
				fprintf(statfp,"Compiled %s %.5s with %s\n",__DATE__,__TIME__,compiler);
				fprintf(statfp,"%s\n",os_version(str));
				exit(1);
			}


			else if(toupper(argv[i][0])=='O')
				mode|=OVERWRITE;

			else if(toupper(argv[i][0])=='D')
				mode|=IGNORE_DCD;

			else if(toupper(argv[i][0])=='A')
				mode|=ALARM;

			else if(toupper(argv[i][0])=='L')
				mode|=NO_LOCAL;

			else if(toupper(argv[i][0])=='P')
				mode|=PAUSE_ABEND;

			else if(argv[i][0]=='*')
				mode|=DEBUG; 
		}

		else if(argv[i][0]=='+') {
			if(mode&DIR) {
				fprintf(statfp,"Cannot specify both directory and filename\n");
				exit(1); 
			}
			sprintf(str,"%s",argv[i]+1);
			if((fp=fopen(str,"r"))==NULL) {
				fprintf(statfp,"Error %d opening filelist: %s\n",errno,str);
				exit(1); 
			}
			while(!feof(fp) && !ferror(fp) && fnames<MAX_FNAMES) {
				if(!fgets(str,sizeof(str),fp))
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
			if((fname[fnames]=(char *)malloc(strlen(argv[i])+1))==NULL) {
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

	if(sock==INVALID_SOCKET || sock<1) {
		fprintf(statfp,"!No socket descriptor specified\n\n");
		fprintf(errfp,usage);
		exit(1);
	}

	if(!(mode&(SEND|RECV))) {
		fprintf(statfp,"!No command specified\n\n");
		fprintf(statfp,usage);
		exit(1); 
	}

	if(mode&(SEND|XMODEM) && !fnames) { /* Sending with any or recv w/Xmodem */
		fprintf(statfp,"!Must specify filename or filelist\n\n");
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
#if 0
	/* Non-blocking socket I/O */
	val=1;
	ioctlsocket(sock,FIONBIO,&val);	
#endif
	if(!DCDHIGH) {
		newline();
		fprintf(statfp,"No carrier\n");
		bail(1); 
	}

	p=getenv("DSZLOG");
	if(p) {
		if((log=fopen(p,"w"))==NULL) {
			fprintf(statfp,"Error opening DSZLOG file: %s\n",p);
			bail(1); 
		}
	}

	startall=time(NULL);

	if(mode&RECV)
		receive_files(fname, fnames, log);
	else
		send_files(fname, fnames, log);

	bail(0);
	return(0);
}

