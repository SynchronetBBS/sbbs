/* xmodem.c */

/* Synchronet X/YMODEM Functions */

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

#include "sexyz.h"
#include "genwrap.h"	/* YIELD */
#include "conwrap.h"	/* kbhit */

#define getcom(t)	recv_byte(xm->sock,t,xm->mode)
#define putcom(ch)	send_byte(xm->sock,ch,10,xm->mode)
#define newline()	fprintf(xm->statfp,"\n");

void xmodem_put_nak(xmodem_t* xm)
{
	while(getcom(1)!=NOINP && (xm->mode&NO_LOCAL || kbhit()!=LOC_ABORT))
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
int xmodem_get_block(xmodem_t* xm, uchar* block, uint block_size, BOOL hdrblock)
{
	uchar	block_num;					/* Block number received in header	*/
	uchar	chksum,calc_chksum;
	int		i,errors,eot=0,can=0;
	uint	b;
	ushort	crc,calc_crc;

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
				if((xm->mode&(YMODEM|GMODE))==YMODEM && !eot) {
					eot=1;
					xmodem_put_nak(xm);		/* chuck's double EOT trick */
					continue; 
				}
				return(-EOT);
			case CAN:
				newline();
				if(!can) {			/* must get two CANs in a row */
					can=1;
					fprintf(xm->statfp,"Received CAN  Expected SOH, STX, or EOT\n");
					continue; 
				}
				fprintf(xm->statfp,"Cancelled remotely\n");
				bail(-1);
				break;
			case NOINP: 	/* Nothing came in */
				continue;
			default:
				newline();
				fprintf(xm->statfp,"Received %s  Expected SOH, STX, or EOT\n",chr((uchar)i));
				if(hdrblock)  /* Trying to get Ymodem header block */
					return(-1);
				xmodem_put_nak(xm);
				continue; 
		}
		i=getcom(1);
		if(i==NOINP) {
			xmodem_put_nak(xm);
			continue; 
		}
		block_num=i;
		i=getcom(1);
		if(i==NOINP) {
			xmodem_put_nak(xm);
			continue; 
		}
		if(block_num!=(uchar)~i) {
			newline();
			fprintf(xm->statfp,"Block number error\n");
			xmodem_put_nak(xm);
			continue; 
		}
		calc_crc=calc_chksum=0;
		for(b=0;b<block_size;b++) {
			i=getcom(1);
			if(i==NOINP)
				break;
			block[b]=i;
			if(xm->mode&CRC)
				calc_crc=ucrc16(block[b],calc_crc);
			else
				calc_chksum+=block[b]; 
		}

		if(b<block_size) {
			xmodem_put_nak(xm);
			continue; 
		}

		if(xm->mode&CRC) {
			crc=getcom(1)<<8;
			crc|=getcom(1); 
		}
		else
			chksum=getcom(1);

		if(xm->mode&CRC) {
			if(crc==calc_crc)
				break;
			newline();
			fprintf(xm->statfp,"CRC error\n"); 
		}
		else	/* CHKSUM */
		{	
			if(chksum==calc_chksum)
				break;
			newline();
			fprintf(xm->statfp,"Checksum error\n"); 
		}

		if(xm->mode&GMODE) {	/* Don't bother sending a NAK. He's not listening */
			xmodem_cancel(xm);
			bail(1); 
		}
		xmodem_put_nak(xm); 
	}

	if(errors>=MAXERRORS) {
		newline();
		fprintf(xm->statfp,"Too many errors\n");
		return(-1); 
	}
	return(block_num);
}

/*****************/
/* Sends a block */
/*****************/
void xmodem_put_block(xmodem_t* xm, uchar* block, uint block_size, ulong block_num)
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
		if(xm->mode&CRC)
			crc=ucrc16(block[i],crc);
		else
			chksum+=block[i]; 
	}

	if(xm->mode&CRC) {
		putcom((uchar)(crc >> 8));
		putcom((uchar)(crc&0xff)); 
	}
	else
		putcom(chksum);
	YIELD();
}

/************************************************************/
/* Gets an acknowledgement - usually after sending a block	*/
/* Returns 1 if ack received, 0 otherwise.					*/
/************************************************************/
int xmodem_get_ack(xmodem_t* xm, int tries)
{
	int i,errors,can=0;

	for(errors=0;errors<tries;errors++) {

		if(xm->mode&GMODE) {		/* Don't wait for ACK on Ymodem-G */
			if(getcom(0)==CAN) {
				newline();
				fprintf(xm->statfp,"Cancelled remotely\n");
				xmodem_cancel(xm);
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
				fprintf(xm->statfp,"Cancelled remotely\n");
				xmodem_cancel(xm);
				bail(1); 
			}
			can=1; 
		}
		if(i!=NOINP) {
			newline();
			fprintf(xm->statfp,"Received %s  Expected ACK\n",chr((uchar)i));
			if(i!=CAN)
				return(0); 
		} 
	}

	return(0);
}

char* xmodem_ver(char *buf)
{
	sscanf("$Revision$", "%*s %s", buf);

	return(buf);
}

const char* xmodem_source(void)
{
	return(__FILE__);
}