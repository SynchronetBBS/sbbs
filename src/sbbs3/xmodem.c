/* xmodem.c */

/* Synchronet X/YMODEM Functions */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2005 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#include <stdarg.h>		/* va_list */
#include "sexyz.h"
#include "crc16.h"
#include "genwrap.h"	/* YIELD */
#include "conwrap.h"	/* kbhit */

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

void xmodem_put_nak(xmodem_t* xm)
{
	while(getcom(0)!=NOINP)
		;				/* wait for any trailing data */
	putcom(NAK);
}

void xmodem_cancel(xmodem_t* xm)
{
	int i;

	for(i=0;i<8;i++)
		putcom(CAN);
	for(i=0;i<10;i++)
		putcom('\b');
}

/****************************************************************************/
/* Receive a X/Ymodem block (sector) from COM port							*/
/* hdrblock is 1 if attempting to get Ymodem header block, 0 if data block	*/
/* Returns blocknum if all went well, -1 on error or CAN, and -EOT if EOT	*/
/****************************************************************************/
int xmodem_get_block(xmodem_t* xm, uchar* block, BOOL hdrblock)
{
	uchar	block_num;					/* Block number received in header	*/
	uchar	chksum,calc_chksum;
	int		i,eot=0,can=0;
	uint	b,errors;
	ushort	crc,calc_crc;

	for(errors=0;errors<xm->max_errors;errors++) {
		i=getcom(hdrblock ? 5 : 10);
		if(eot && i!=EOT)
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
				if(((*xm->mode)&(YMODEM|GMODE))==YMODEM && !eot) {
					eot=1;
					xmodem_put_nak(xm);		/* chuck's double EOT trick */
					continue; 
				}
				return(-EOT);
			case CAN:
				if(!can) {			/* must get two CANs in a row */
					can=1;
					lprintf(xm,LOG_WARNING,"\nReceived CAN  Expected SOH, STX, or EOT");
					continue; 
				}
				lprintf(xm,LOG_WARNING,"\nCancelled remotely");
				return(-1);
			default:
				lprintf(xm,LOG_WARNING,"\nReceived %s  Expected SOH, STX, or EOT",chr((uchar)i));
			case NOINP: 	/* Nothing came in */
				if(hdrblock || (*xm->mode)&GMODE)
					return(-1);
				xmodem_put_nak(xm);
				continue; 
		}
		i=getcom(xm->byte_timeout);
		if(i==NOINP) {
			if(hdrblock || (*xm->mode)&GMODE)
				return(-1);
			xmodem_put_nak(xm);
			continue; 
		}
		block_num=i;
		i=getcom(xm->byte_timeout);
		if(i==NOINP) {
			if(hdrblock || (*xm->mode)&GMODE)
				return(-1);
			xmodem_put_nak(xm);
			continue; 
		}
		if(block_num!=(uchar)~i) {
			lprintf(xm,LOG_ERR,"\nBlock number error");
			if(hdrblock || (*xm->mode)&GMODE)
				return(-1);
			xmodem_put_nak(xm);
			continue; 
		}
		calc_crc=calc_chksum=0;
		for(b=0;b<xm->block_size;b++) {
			i=getcom(xm->byte_timeout);
			if(i==NOINP)
				break;
			block[b]=i;
			if((*xm->mode)&CRC)
				calc_crc=ucrc16(block[b],calc_crc);
			else
				calc_chksum+=block[b]; 
		}

		if(b<xm->block_size) {
			if(hdrblock || (*xm->mode)&GMODE)
				return(-1);
			xmodem_put_nak(xm);
			continue; 
		}

		if((*xm->mode)&CRC) {
			crc=getcom(xm->byte_timeout)<<8;
			crc|=getcom(xm->byte_timeout); 
		}
		else
			chksum=getcom(xm->byte_timeout);

		if((*xm->mode)&CRC) {
			if(crc==calc_crc)
				break;
			lprintf(xm,LOG_ERR,"\nCRC error"); 
		}
		else	/* CHKSUM */
		{	
			if(chksum==calc_chksum)
				break;
			lprintf(xm,LOG_ERR,"\nChecksum error"); 
		}

		if((*xm->mode)&GMODE) 	/* Don't bother sending a NAK. He's not listening */
			return(-1);

		xmodem_put_nak(xm); 
	}

	if(errors>=xm->max_errors) {
		lprintf(xm,LOG_ERR,"\nToo many errors");
		return(-1); 
	}
	return(block_num);
}

/*****************/
/* Sends a block */
/*****************/
void xmodem_put_block(xmodem_t* xm, uchar* block, unsigned block_size, unsigned block_num)
{
	uchar	ch,chksum;
    uint	i;
	ushort	crc;

	if(block_size==128)
		putcom(SOH);
	else			/* 1024 */
		putcom(STX);
	ch=(uchar)(block_num&0xff);
	putcom(ch);
	putcom((uchar)~ch);
	chksum=0;
	crc=0;
	for(i=0;i<block_size;i++) {
		putcom(block[i]);
		if((*xm->mode)&CRC)
			crc=ucrc16(block[i],crc);
		else
			chksum+=block[i]; 
	}

	if((*xm->mode)&CRC) {
		putcom((uchar)(crc >> 8));
		putcom((uchar)(crc&0xff)); 
	}
	else
		putcom(chksum);
//	YIELD();
}

/************************************************************/
/* Gets an acknowledgement - usually after sending a block	*/
/* Returns 1 if ack received, 0 otherwise.					*/
/************************************************************/
BOOL xmodem_get_ack(xmodem_t* xm, unsigned tries, unsigned block_num)
{
	int i,can=0;
	unsigned errors;

	for(errors=0;errors<tries;errors++) {

		if((*xm->mode)&GMODE) {		/* Don't wait for ACK on Ymodem-G */
			SLEEP(xm->g_delay);
			if(getcom(0)==CAN) {
				lprintf(xm,LOG_WARNING,"\nBlock %u: !Cancelled remotely", block_num);
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
				lprintf(xm,LOG_WARNING,"\nBlock %u: !Cancelled remotely", block_num);
				xmodem_cancel(xm);
				return(FALSE); 
			}
			can=1; 
		}
		if(i!=NOINP) {
			lprintf(xm,LOG_WARNING,"\nBlock %u: !Received %s  Expected ACK"
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
	for(errors=can=0;errors<xm->max_errors;errors++) {
		i=getcom(xm->recv_timeout);
		if(can && i!=CAN)
			can=0;
		switch(i) {
			case NAK: 		/* checksum */
				lprintf(xm,LOG_INFO,"Receiver requested 8-bit Checksum mode");
				return(TRUE); 
			case 'C':
				lprintf(xm,LOG_INFO,"Receiver requested 16-bit CRC mode");
				*(xm->mode)|=CRC;
				return(TRUE); 
			case 'G':
				lprintf(xm,LOG_INFO,"Receiver requested Streaming (16-bit CRC) mode");
				*(xm->mode)|=(GMODE|CRC);
				return(TRUE); 
			case CAN:
				if(can) {
					lprintf(xm,LOG_WARNING,"!Receiver cancelled");
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

	lprintf(xm,LOG_ERR,"!FAILED to get transfer mode request from receiver");
	return(FALSE);
}

BOOL xmodem_put_eot(xmodem_t* xm)
{
	int ch;
	unsigned errors;

	for(errors=0;errors<xm->max_errors;errors++) {

		lprintf(xm,LOG_INFO,"Sending end-of-Text indicator (%d)",errors+1);

		while((ch=getcom(0))!=NOINP)
			lprintf(xm,LOG_INFO,"Throwing out received: %s",chr((uchar)ch));

		putcom(EOT);
		if((ch=getcom(xm->recv_timeout))==NOINP)
			continue;
		lprintf(xm,LOG_INFO,"Received %s ",chr((uchar)ch)); 
		if(ch==ACK)
			return(TRUE);
		if(ch==NAK && errors==0 && (*(xm->mode)&(YMODEM|GMODE))==YMODEM) {
			continue;  /* chuck's double EOT trick so don't complain */
		}
		lprintf(xm,LOG_WARNING,"Expected ACK");
	}
	return(FALSE);
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
				,void	(*progress)(void* unused, unsigned block_num, ulong fsize, time_t t)
				,int	(*send_byte)(void*, uchar ch, unsigned timeout)
				,int	(*recv_byte)(void*, unsigned timeout))
{
	memset(xm,0,sizeof(xmodem_t));

	/* Use sane default values */
	xm->send_timeout=10;		/* seconds */
	xm->recv_timeout=10;		/* seconds */
	xm->byte_timeout=3;			/* seconds */
	xm->ack_timeout=10;			/* seconds */
	xm->progress_interval=1;	/* seconds */

	xm->block_size=1024;
	xm->max_errors=10;
	xm->g_delay=1;

	xm->cbdata=cbdata;
	xm->mode=mode;
	xm->lputs=lputs;
	xm->progress=progress;
	xm->send_byte=send_byte;
	xm->recv_byte=recv_byte;
}
