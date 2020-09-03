/* xmodem.c */

/* Synchronet X/YMODEM Functions */

/* $Id: xmodem.c,v 1.52 2019/08/31 22:39:24 rswindell Exp $ */

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

/* Standard headers */
#include <stdio.h>
#include <sys/stat.h>	/* struct stat */
#include <stdarg.h>		/* va_list */

/* smblib */
#include "crc16.h"

/* xpdev */
#include "genwrap.h"	/* YIELD */
#include "dirwrap.h"	/* getfname */
#include "filewrap.h"	/* fileoff_t */

/* sexyz */
#include "sexyz.h"

#define getcom(t)	xm->recv_byte(xm->cbdata,t)
#define putcom(ch)	xm->send_byte(xm->cbdata,ch,xm->send_timeout)

static int lprintf(xmodem_t* xm, int level, const char *fmt, ...)
{
	va_list argptr;
	char	sbuf[1024];

	if(xm->lputs==NULL)
		return(-1);
	if(xm->log_level != NULL)
		if(level > *xm->log_level)
			return 0;

    va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
    return(xm->lputs(xm->cbdata,level,sbuf));
}

static BOOL is_connected(xmodem_t* xm)
{
	if(xm->is_connected!=NULL)
		return(xm->is_connected(xm->cbdata));
	return(TRUE);
}

static BOOL is_cancelled(xmodem_t* xm)
{
	if(xm->is_cancelled!=NULL)
		return(xm->cancelled=xm->is_cancelled(xm->cbdata));
	return(xm->cancelled);
}

static void xmodem_flush(xmodem_t* xm)
{
	if(xm->flush!=NULL)
		xm->flush(xm);
}

static char *chr(uchar ch)
{
	static char str[25];

	switch(ch) {
		case SOH:	return("SOH");
		case STX:	return("STX");
		case ETX:	return("ETX");
		case EOT:	return("EOT");
		case ACK:	return("ACK");
		case NAK:	return("NAK");
		case CAN:	return("CAN");
	}
	if(ch>=' ' && ch<='~')
		sprintf(str,"'%c' (%02Xh)",ch,ch);
	else
		sprintf(str,"%u (%02Xh)",ch,ch);
	return(str); 
}


int xmodem_put_ack(xmodem_t* xm)
{
	int result;

	while(getcom(0)!=NOINP && is_connected(xm))
		;				/* wait for any trailing data */
	result = putcom(ACK);

	xmodem_flush(xm);

	return result;
}

int xmodem_put_nak(xmodem_t* xm, unsigned block_num)
{
	int i,dump_count=0;
	int	result;

	/* wait for any trailing data */
	while((i=getcom(0))!=NOINP && is_connected(xm)) {
		dump_count++;
		lprintf(xm,LOG_DEBUG,"Block %u: Dumping byte: %02Xh"
			,block_num, (BYTE)i);
		SLEEP(1);
	}
	if(dump_count)
		lprintf(xm,LOG_INFO,"Block %u: Dumped %u bytes"
			,block_num, dump_count);

	if(block_num<=1) {
		if(*(xm->mode)&GMODE) {		/* G for X/Ymodem-G */
			lprintf(xm,LOG_INFO,"Block %u: Requesting mode: Streaming, 16-bit CRC", block_num);
			result = putcom('G');
		} else if(*(xm->mode)&CRC) {	/* C for CRC */
			lprintf(xm,LOG_INFO,"Block %u: Requesting mode: 16-bit CRC", block_num);
			result = putcom('C');
		} else {				/* NAK for checksum */
			lprintf(xm,LOG_INFO,"Block %u: Requesting mode: 8-bit Checksum", block_num);
			result = putcom(NAK);
		}
	} else
		result = putcom(NAK);

	xmodem_flush(xm);

	return result;
}

int xmodem_cancel(xmodem_t* xm)
{
	int i;
	int result;

	if(!is_cancelled(xm) && is_connected(xm)) {
		for(i=0;i<8 && is_connected(xm);i++)
			if((result=putcom(CAN))!=0)
				return result;
		for(i=0;i<10 && is_connected(xm);i++)
			if((result=putcom('\b'))!=0)
				return result;
		xm->cancelled=TRUE;
	}

	xmodem_flush(xm);

	return SUCCESS;
}

/****************************************************************************/
/* Return 0 on success														*/
/****************************************************************************/
int xmodem_get_block(xmodem_t* xm, uchar* block, unsigned expected_block_num)
{
	uchar		block_num;				/* Block number received in header	*/
	uchar		block_inv;
	uchar		chksum,calc_chksum;
	int			i,eot=0,can=0;
	uint		b,errors;
	uint16_t	crc,calc_crc;

	lprintf(xm, LOG_DEBUG, "Requesting data block %u", expected_block_num);
	for(errors=0;errors<=xm->max_errors && is_connected(xm);errors++) {

		i=getcom(expected_block_num<=1 ? 3 : 10);
		if(eot && i!=EOT && i!=NOINP)
			eot=0;
		if(can && i!=CAN)
			can=0;
		switch(i) {
			case SOH: /* 128-byte blocks */
				xm->block_size=XMODEM_MIN_BLOCK_SIZE;
				break;
			case STX: /* 1024-byte blocks */
				if(xm->max_block_size < XMODEM_MAX_BLOCK_SIZE) {
					lprintf(xm,LOG_WARNING,"Block %u: 1024-byte blocks not supported"
						,expected_block_num);
					return FAILURE;
				}
				xm->block_size=XMODEM_MAX_BLOCK_SIZE;
				break;
			case EOT:
				lprintf(xm,LOG_DEBUG,"Block %u: EOT received", expected_block_num);
				if(/*((*xm->mode)&(YMODEM|GMODE))==YMODEM &&*/ !eot) {
					lprintf(xm,LOG_INFO,"NAKing first EOT");
					eot=1;	
					xmodem_put_nak(xm,expected_block_num); /* chuck's double EOT trick */
					continue; 
				}
				return(EOT);
			case CAN:
				if(!can) {			/* must get two CANs in a row */
					can=1;
					lprintf(xm,LOG_WARNING,"Block %u: Received CAN  Expected SOH, STX, or EOT"
						,expected_block_num);
					continue; 
				}
				lprintf(xm,LOG_WARNING,"Block %u: Canceled remotely", expected_block_num);
				return(CAN);
			default:
				lprintf(xm,LOG_WARNING,"Block %u: Received %s  Expected SOH, STX, or EOT"
					,expected_block_num, chr((uchar)i));
				/* Fall-through */
			case NOINP: 	/* Nothing came in */
				if(eot)
					return(EOT);
				return(NOINP);
		}
		if((i=getcom(xm->byte_timeout))==NOINP)
			break; 
		block_num=i;
		if((i=getcom(xm->byte_timeout))==NOINP)
			break; 
		block_inv=i;
		calc_crc=calc_chksum=0;
		for(b=0;b<xm->block_size && is_connected(xm);b++) {
			if((i=getcom(xm->byte_timeout))==NOINP)
				break;
			block[b]=i;
			if((*xm->mode)&CRC)
				calc_crc=ucrc16(block[b],calc_crc);
			else
				calc_chksum+=block[b]; 
		}

		if(b<xm->block_size)
			break; 

		if((*xm->mode)&CRC) {
			crc=getcom(xm->byte_timeout)<<8;
			crc|=getcom(xm->byte_timeout); 
		}
		else
			chksum=getcom(xm->byte_timeout);

		if(block_num!=(uchar)~block_inv) {
			lprintf(xm,LOG_WARNING,"Block %u: Block number bit error (0x%02X vs 0x%02x)"
				,expected_block_num, block_num,(uchar)~block_inv);
			break; 
		}

		if((*xm->mode)&CRC) {
			if(crc!=calc_crc) {
				lprintf(xm,LOG_WARNING,"Block %u: CRC ERROR", block_num); 
				break;
			}
		}
		else	/* CHKSUM */
		{
			if(chksum!=calc_chksum) {
				lprintf(xm,LOG_WARNING,"Block %u: CHECKSUM ERROR", block_num); 
				break;
			}
		}

		if(block_num!=(uchar)(expected_block_num&0xff)) {
			lprintf(xm,LOG_WARNING,"Block number error (%u received, expected %u)"
				,block_num,expected_block_num&0xff);
			if((*xm->mode)&XMODEM && expected_block_num==1 && block_num==0)
				return(NOT_XMODEM);
			if(expected_block_num==0 && block_num==1)
				return(NOT_YMODEM);
			if(expected_block_num && block_num==(uchar)((expected_block_num-1)&0xff))
				continue;	/* silently discard repeated packets (ymodem.doc 7.3.2) */
			break; 
		}

		return SUCCESS;	/* Success */
	}

	return FAILURE;		/* Failure */
}

/*****************/
/* Sends a block */
/*****************/
int xmodem_put_block(xmodem_t* xm, uchar* block, unsigned block_size, unsigned block_num)
{
	int			result;
	uchar		ch,chksum;
    uint		i;
	uint16_t	crc;

	if(block_size==XMODEM_MIN_BLOCK_SIZE)
		result=putcom(SOH);
	else			/* 1024 */
		result=putcom(STX);
	if(result!=0)
		return(result);
	ch=(uchar)(block_num&0xff);
	if((result=putcom(ch))!=0)
		return result;
	if((result=putcom((uchar)~ch))!=0)
		return result;
	chksum=0;
	crc=0;
	for(i=0;i<block_size && is_connected(xm);i++) {
		if((result=putcom(block[i]))!=0)
			return result;
		if((*xm->mode)&CRC)
			crc=ucrc16(block[i],crc);
		else
			chksum+=block[i]; 
	}

	if((*xm->mode)&CRC) {
		if((result=	putcom((uchar)(crc >> 8)))!=0)
			return result;
		result = putcom((uchar)(crc&0xff)); 
	} else
		result = putcom(chksum);

	xmodem_flush(xm);

	return result;
}

/************************************************************/
/* Gets an acknowledgement - usually after sending a block	*/
/* Returns ACK if ack received								*/
/************************************************************/
int xmodem_get_ack(xmodem_t* xm, unsigned tries, unsigned block_num)
{
	int i=NOINP,can=0;
	unsigned errors;

	for(errors=0;errors<tries && is_connected(xm);) {

		if((*xm->mode)&GMODE) {		/* Don't wait for ACK on X/Ymodem-G */
			SLEEP(xm->g_delay);
			if(getcom(0)==CAN) {
				lprintf(xm,LOG_WARNING,"Block %u: !Canceled remotely", block_num);
				xmodem_cancel(xm);
				return(CAN); 
			}
			return(ACK); 
		}

		i=getcom(xm->ack_timeout);
		if(can && i!=CAN)
			can=0;
		if(i==ACK)
			break;
		if(i==CAN) {
			if(can) {	/* 2 CANs in a row */
				lprintf(xm,LOG_WARNING,"Block %u: !Canceled remotely", block_num);
				xmodem_cancel(xm);
				return(CAN); 
			}
			can=1; 
		}
		if(i!=NOINP) {
			lprintf(xm,LOG_WARNING,"Block %u: !Received %s  Expected ACK"
				,block_num, chr((uchar)i));
			if(i!=CAN)
				return(i); 
		}
		if(i!=CAN)
			errors++;
	}

	return(i);
}

BOOL xmodem_get_mode(xmodem_t* xm)
{
	int			i;
	unsigned	errors;
	unsigned	can;

	lprintf(xm,LOG_INFO,"Waiting for transfer mode request...");

	*(xm->mode)&=~(GMODE|CRC);
	for(errors=can=0;errors<=xm->max_errors && is_connected(xm);errors++) {
		i=getcom(xm->recv_timeout);
		if(can && i!=CAN)
			can=0;
		switch(i) {
			case NAK: 		/* checksum */
				lprintf(xm,LOG_INFO,"Receiver requested mode: 8-bit Checksum");
				return(TRUE); 
			case 'C':
				lprintf(xm,LOG_INFO,"Receiver requested mode: 16-bit CRC");
				if(!xm->crc_mode_supported)
					continue;
				*(xm->mode)|=CRC;
				return(TRUE); 
			case 'G':
				lprintf(xm,LOG_INFO,"Receiver requested mode: Streaming, 16-bit CRC");
				if(!xm->crc_mode_supported || !xm->g_mode_supported)
					continue;
				*(xm->mode)|=(GMODE|CRC);
				return(TRUE); 
			case CAN:
				if(can) {
					lprintf(xm,LOG_WARNING,"Canceled remotely");
					return(FALSE); 
				}
				can=1; 
				break;
			case NOINP:
				break;
			default:
				lprintf(xm,LOG_WARNING,"Received %s  Expected NAK, C, or G"
					,chr((uchar)i));
				break;
		} 
	}

	lprintf(xm,LOG_ERR,"Failed to get transfer mode request from receiver");
	return(FALSE);
}

BOOL xmodem_put_eot(xmodem_t* xm)
{
	int ch;
	unsigned errors;
	unsigned cans=0;

	for(errors=0;errors<=xm->max_errors && is_connected(xm);errors++) {

		lprintf(xm,LOG_INFO,"Sending End-of-Text (EOT) indicator (%d)",errors+1);

		while((ch=getcom(0))!=NOINP && is_connected(xm))
			lprintf(xm,LOG_INFO,"Throwing out received: %s",chr((uchar)ch));

		putcom(EOT);
		xmodem_flush(xm);
		if((ch=getcom(xm->recv_timeout))==NOINP)
			continue;
		lprintf(xm,LOG_INFO,"Received %s",chr((uchar)ch)); 
		if(ch==ACK)
			return(TRUE);
		if(ch==CAN && ++cans>1)
			break;
		if(ch==NAK && errors==0 && (*(xm->mode)&(YMODEM|GMODE))==YMODEM) {
			continue;  /* chuck's double EOT trick so don't complain */
		}
		lprintf(xm,LOG_WARNING,"Expected ACK");
	}
	return(FALSE);
}

BOOL xmodem_send_file(xmodem_t* xm, const char* fname, FILE* fp, time_t* start, uint64_t* sent)
{
	BOOL		success=FALSE;
	int64_t		sent_bytes=0;
	char		block[XMODEM_MAX_BLOCK_SIZE];
	size_t		block_len;
	unsigned	block_num;
	size_t		i;
	size_t		rd;
	time_t		startfile;
	struct		stat st;
	BOOL		sent_header=FALSE;

	if(sent!=NULL)	
		*sent=0;

	if(start!=NULL)		
		*start=time(NULL);

	if(fstat(fileno(fp),&st) != 0) {
		lprintf(xm,LOG_ERR,"Failed to fstat file");
		return FALSE;
	}

	if(xm->total_files==0)
		xm->total_files=1;

	if(xm->total_bytes==0)
		xm->total_bytes=st.st_size;

	do {
	/* try */
		if(*(xm->mode)&YMODEM) {

			if(!xmodem_get_mode(xm))
				break;

			memset(block,0,sizeof(block));
			SAFECOPY(block,getfname(fname));
			i=sprintf(block+strlen(block)+1,"%"PRIu64" %"PRIoMAX" 0 0 %lu %"PRId64
				,(uint64_t)st.st_size
				,(uintmax_t)st.st_mtime
				,xm->total_files-xm->sent_files
				,xm->total_bytes-xm->sent_bytes);
			
			lprintf(xm,LOG_INFO,"Sending YMODEM header block: '%s'",block+strlen(block)+1);
			
			block_len=strlen(block)+1+i;
			for(xm->errors=0;xm->errors<=xm->max_errors && !is_cancelled(xm) && is_connected(xm);xm->errors++) {
				int ch;
				while((ch=getcom(0))!=NOINP && is_connected(xm))
					lprintf(xm,LOG_INFO,"Throwing out received: %s",chr((uchar)ch));
				xmodem_put_block(xm, (uchar*)block, block_len <=XMODEM_MIN_BLOCK_SIZE ? XMODEM_MIN_BLOCK_SIZE:XMODEM_MAX_BLOCK_SIZE, 0  /* block_num */);
				if((i=xmodem_get_ack(xm,/* tries: */1, /* block_num: */0)) == ACK) {
					sent_header=TRUE;
					break; 
				}
				if((i==NAK || i=='C' || i=='G')
					&& xm->fallback_to_xmodem && xm->errors+1 == xm->fallback_to_xmodem) {
					lprintf(xm,LOG_NOTICE,"Falling back to XMODEM mode after %u attempts"
						,xm->fallback_to_xmodem);
					*(xm->mode)&=~YMODEM;
					break;
				}
			}
			if(xm->errors>xm->max_errors || is_cancelled(xm)) {
				lprintf(xm,LOG_ERR,"Failed to send header block");
				break;
			}
		}

		if(!xmodem_get_mode(xm))
			break;

		startfile=time(NULL);	/* reset time, don't count header block */
		if(start!=NULL)
			*start=startfile;

		block_num=1;
		xm->errors=0;
		while(sent_bytes < st.st_size && xm->errors<=xm->max_errors && !is_cancelled(xm)
			&& is_connected(xm)) {
			fseeko(fp,(off_t)sent_bytes,SEEK_SET);
			memset(block,CPMEOF,xm->block_size);
			if(!sent_header) {
				if(xm->block_size>XMODEM_MIN_BLOCK_SIZE) {
					if((sent_bytes+xm->block_size) > st.st_size) {
						if((sent_bytes+xm->block_size-XMODEM_MIN_BLOCK_SIZE) >= st.st_size) {
							lprintf(xm,LOG_INFO,"Falling back to 128-byte blocks for end of file");
							xm->block_size=XMODEM_MIN_BLOCK_SIZE;
						}
					}
				}
			}
			if((rd=fread(block,1,xm->block_size,fp))!=xm->block_size 
				&& (sent_bytes + rd) != st.st_size) {
				lprintf(xm,LOG_ERR,"ERROR %d reading %u bytes at file offset %"PRId64
					,errno,xm->block_size,(int64_t)ftello(fp));
				xm->errors++;
				continue;
			}
			if(xm->progress!=NULL)
				xm->progress(xm->cbdata,block_num,ftello(fp),st.st_size,startfile);
			xmodem_put_block(xm, (uchar*)block, xm->block_size, block_num);
			if(xmodem_get_ack(xm, /* tries: */5,block_num) != ACK) {
				xm->errors++;
				lprintf(xm,LOG_WARNING,"Block %u: Error #%d at offset %"PRId64
					,block_num, xm->errors,(int64_t)(ftello(fp)-xm->block_size));
				if(xm->errors==3 && block_num==1 && xm->block_size>XMODEM_MIN_BLOCK_SIZE) {
					lprintf(xm,LOG_NOTICE,"Block %u: Falling back to 128-byte blocks", block_num);
					xm->block_size=XMODEM_MIN_BLOCK_SIZE;
				}
			} else {
				block_num++; 
				sent_bytes+=rd;
			}
		}
		if(sent_bytes >= st.st_size && !is_cancelled(xm) && is_connected(xm)) {

	#if 0 /* !SINGLE_THREADED */
			lprintf(LOG_DEBUG,"Waiting for output buffer to empty... ");
			if(WaitForEvent(outbuf_empty,5000)!=WAIT_OBJECT_0)
				lprintf(xm,LOG_WARNING,"FAILURE");
	#endif
			if(xmodem_put_eot(xm))	/* end-of-text, wait for ACK */
				success=TRUE;
		}
	} while(0);
	/* finally */

	if(!success)
		xmodem_cancel(xm);

	if(sent!=NULL)
		*sent=sent_bytes;

	return(success);
}


const char* xmodem_source(void)
{
	return(__FILE__);
}

char* xmodem_ver(char *buf)
{
	sscanf("$Revision: 1.52 $", "%*s %s", buf);

	return(buf);
}

void xmodem_init(xmodem_t* xm, void* cbdata, long* mode
				,int	(*lputs)(void*, int level, const char* str)
				,void	(*progress)(void* unused, unsigned block_num, int64_t offset, int64_t fsize, time_t t)
				,int	(*send_byte)(void*, uchar ch, unsigned timeout)
				,int	(*recv_byte)(void*, unsigned timeout)
				,BOOL	(*is_connected)(void*)
				,BOOL	(*is_cancelled)(void*)
				,void	(*flush)(void*))
{
	memset(xm,0,sizeof(xmodem_t));

	/* Use sane default values */
	xm->send_timeout=10;		/* seconds */
	xm->recv_timeout=10;		/* seconds */
	xm->byte_timeout=3;			/* seconds */
	xm->ack_timeout=10;			/* seconds */

	xm->block_size=XMODEM_MAX_BLOCK_SIZE;
	xm->max_block_size=XMODEM_MAX_BLOCK_SIZE;
	xm->max_errors=9;
	xm->g_delay=1;

	xm->cbdata=cbdata;
	xm->mode=mode;
	xm->g_mode_supported=TRUE;
	xm->crc_mode_supported=TRUE;
	xm->lputs=lputs;
	xm->progress=progress;
	xm->send_byte=send_byte;
	xm->recv_byte=recv_byte;
	xm->is_connected=is_connected;
	xm->is_cancelled=is_cancelled;
	xm->flush=flush;
}
