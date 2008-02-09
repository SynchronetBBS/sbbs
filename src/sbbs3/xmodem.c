/* xmodem.c */

/* Synchronet X/YMODEM Functions */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2006 Rob Swindell - http://www.synchro.net/copyright.html		*
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
	while(getcom(0)!=NOINP && is_connected(xm))
		;				/* wait for any trailing data */
	return putcom(ACK);
}

int xmodem_put_nak(xmodem_t* xm, unsigned block_num)
{
	while(getcom(0)!=NOINP && is_connected(xm))
		;				/* wait for any trailing data */

	if(block_num<=1) {
		if(*(xm->mode)&GMODE) {		/* G for Ymodem-G */
			lprintf(xm,LOG_INFO,"Requesting mode: Streaming, 16-bit CRC");
			return putcom('G');
		} else if(*(xm->mode)&CRC) {	/* C for CRC */
			lprintf(xm,LOG_INFO,"Requesting mode: 16-bit CRC");
			return putcom('C');
		} else {				/* NAK for checksum */
			lprintf(xm,LOG_INFO,"Requesting mode: 8-bit Checksum");
			return putcom(NAK);
		}
	}
	return putcom(NAK);
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

	return 0;
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

	for(errors=0;errors<=xm->max_errors && is_connected(xm);errors++) {

		i=getcom(expected_block_num<=1 ? 5 : 10);
		if(eot && i!=EOT && i!=NOINP)
			eot=0;
		if(can && i!=CAN)
			can=0;
		switch(i) {
			case SOH: /* 128 byte blocks */
				xm->block_size=128;
				break;
			case STX: /* 1024 byte blocks */
				xm->block_size=1024;
				break;
			case EOT:
				lprintf(xm,LOG_DEBUG,"EOT");
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
					lprintf(xm,LOG_WARNING,"Received CAN  Expected SOH, STX, or EOT");
					continue; 
				}
				lprintf(xm,LOG_WARNING,"Cancelled remotely");
				return(CAN);
			default:
				lprintf(xm,LOG_WARNING,"Received %s  Expected SOH, STX, or EOT",chr((uchar)i));
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
			lprintf(xm,LOG_WARNING,"Block number bit error (0x%02X vs 0x%02x)"
				,block_num,(uchar)~block_inv);
			break; 
		}

		if(block_num!=(uchar)(expected_block_num&0xff)) {
			lprintf(xm,LOG_WARNING,"Block number error (%u received, expected %u)"
				,block_num,expected_block_num&0xff);
			if(expected_block_num && block_num==(uchar)((expected_block_num-1)&0xff))
				continue;	/* silently discard repeated packets (ymodem.doc 7.3.2) */
			break; 
		}

		if((*xm->mode)&CRC) {
			if(crc!=calc_crc) {
				lprintf(xm,LOG_WARNING,"Block %u: CRC ERROR", expected_block_num); 
				break;
			}
		}
		else	/* CHKSUM */
		{	
			if(chksum!=calc_chksum) {
				lprintf(xm,LOG_WARNING,"Block %u: CHECKSUM ERROR", expected_block_num); 
				break;
			}
		}
		return(0);	/* Success */
	}

	return(-2);		/* Failure */
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

	if(block_size==128)
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
		return		putcom((uchar)(crc&0xff)); 
	}
	return putcom(chksum);
}

/************************************************************/
/* Gets an acknowledgement - usually after sending a block	*/
/* Returns 1 if ack received, 0 otherwise.					*/
/************************************************************/
BOOL xmodem_get_ack(xmodem_t* xm, unsigned tries, unsigned block_num)
{
	int i,can=0;
	unsigned errors;

	for(errors=0;errors<tries && is_connected(xm);errors++) {

		if((*xm->mode)&GMODE) {		/* Don't wait for ACK on Ymodem-G */
			SLEEP(xm->g_delay);
			if(getcom(0)==CAN) {
				lprintf(xm,LOG_WARNING,"Block %u: !Cancelled remotely", block_num);
				xmodem_cancel(xm);
				return(FALSE); 
			}
			return(TRUE); 
		}

		i=getcom(xm->ack_timeout);
		if(can && i!=CAN)
			can=0;
		if(i==ACK)
			return(TRUE);
		if(i==CAN) {
			if(can) {
				lprintf(xm,LOG_WARNING,"Block %u: !Cancelled remotely", block_num);
				xmodem_cancel(xm);
				return(FALSE); 
			}
			can=1; 
		}
		if(i!=NOINP) {
			lprintf(xm,LOG_WARNING,"Block %u: !Received %s  Expected ACK"
				,block_num, chr((uchar)i));
			if(i!=CAN)
				return(FALSE); 
		} 
	}

	return(FALSE);
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
				*(xm->mode)|=CRC;
				return(TRUE); 
			case 'G':
				lprintf(xm,LOG_INFO,"Receiver requested mode: Streaming, 16-bit CRC");
				*(xm->mode)|=(GMODE|CRC);
				return(TRUE); 
			case CAN:
				if(can) {
					lprintf(xm,LOG_WARNING,"Cancelled remotely");
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

BOOL xmodem_send_file(xmodem_t* xm, const char* fname, FILE* fp, time_t* start, ulong* sent)
{
	BOOL		success=FALSE;
	ulong		sent_bytes=0;
	char		block[1024];
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

	fstat(fileno(fp),&st);

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
			i=sprintf(block+strlen(block)+1,"%lu %lo 0 0 %d %u"
				,(ulong)st.st_size
				,st.st_mtime
				,xm->total_files-xm->sent_files
				,xm->total_bytes-xm->sent_bytes);
			
			lprintf(xm,LOG_INFO,"Sending Ymodem header block: '%s'",block+strlen(block)+1);
			
			block_len=strlen(block)+1+i;
			for(xm->errors=0;xm->errors<=xm->max_errors && !is_cancelled(xm) && is_connected(xm);xm->errors++) {
				xmodem_put_block(xm, block, block_len <=128 ? 128:1024, 0  /* block_num */);
				if(xmodem_get_ack(xm,1,0)) {
					sent_header=TRUE;
					break; 
				}
			}
			if(xm->errors>=xm->max_errors || is_cancelled(xm)) {
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
		while(sent_bytes < (ulong)st.st_size && xm->errors<=xm->max_errors && !is_cancelled(xm)
			&& is_connected(xm)) {
			fseek(fp,sent_bytes,SEEK_SET);
			memset(block,CPMEOF,xm->block_size);
			if(!sent_header) {
				if(xm->block_size>128) {
					if((long)(sent_bytes+xm->block_size) > st.st_size) {
						if((long)(sent_bytes+xm->block_size-128) >= st.st_size) {
							lprintf(xm,LOG_INFO,"Falling back to 128 byte blocks for end of file");
							xm->block_size=128;
						}
					}
				}
			}
			if((rd=fread(block,1,xm->block_size,fp))!=xm->block_size 
				&& (long)(sent_bytes + rd) != st.st_size) {
				lprintf(xm,LOG_ERR,"READ ERROR %d instead of %d at offset %lu"
					,rd,xm->block_size,sent_bytes);
				xm->errors++;
				continue;
			}
			if(xm->progress!=NULL)
				xm->progress(xm->cbdata,block_num,ftell(fp),st.st_size,startfile);
			xmodem_put_block(xm, block, xm->block_size, block_num);
			if(!xmodem_get_ack(xm,5,block_num)) {
				xm->errors++;
				lprintf(xm,LOG_WARNING,"Error #%d at offset %ld"
					,xm->errors,ftell(fp)-xm->block_size);
				if(xm->errors==3 && block_num==1 && xm->block_size>128) {
					lprintf(xm,LOG_NOTICE,"Falling back to 128 byte blocks");
					xm->block_size=128;
				}
			} else {
				block_num++; 
				sent_bytes+=rd;
			}
		}
		if(sent_bytes >= (ulong)st.st_size && !is_cancelled(xm) && is_connected(xm)) {

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
	sscanf("$Revision$", "%*s %s", buf);

	return(buf);
}

void xmodem_init(xmodem_t* xm, void* cbdata, long* mode
				,int	(*lputs)(void*, int level, const char* str)
				,void	(*progress)(void* unused, unsigned block_num, ulong offset, ulong fsize, time_t t)
				,int	(*send_byte)(void*, uchar ch, unsigned timeout)
				,int	(*recv_byte)(void*, unsigned timeout)
				,BOOL	(*is_connected)(void*)
				,BOOL	(*is_cancelled)(void*))
{
	memset(xm,0,sizeof(xmodem_t));

	/* Use sane default values */
	xm->send_timeout=10;		/* seconds */
	xm->recv_timeout=10;		/* seconds */
	xm->byte_timeout=3;			/* seconds */
	xm->ack_timeout=10;			/* seconds */

	xm->block_size=1024;
	xm->max_errors=9;
	xm->g_delay=1;

	xm->cbdata=cbdata;
	xm->mode=mode;
	xm->lputs=lputs;
	xm->progress=progress;
	xm->send_byte=send_byte;
	xm->recv_byte=recv_byte;
	xm->is_connected=is_connected;
	xm->is_cancelled=is_cancelled;
}
