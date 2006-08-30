/* zmodem.c */

/* Synchronet ZMODEM Functions */

/* $Id$ */

/******************************************************************************/
/* Project : Unite!       File : zmodem general        Version : 1.02         */
/*                                                                            */
/* (C) Mattheij Computer Service 1994                                         */
/*                                                                            */
/* contact us through (in order of preference)                                */
/*                                                                            */
/*   email:          jacquesm@hacktic.nl                                      */
/*   mail:           MCS                                                      */
/*                   Prinses Beatrixlaan 535                                  */
/*                   2284 AT  RIJSWIJK                                        */
/*                   The Netherlands                                          */
/*   voice phone:    31+070-3936926                                           */
/******************************************************************************/

/*
 * zmodem primitives and other code common to zmtx and zmrx
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>	/* va_list */
#include <sys/stat.h>	/* struct stat */

#include "genwrap.h"
#include "dirwrap.h"	/* getfname() */
#include "filewrap.h"	/* filelength() */

#include "zmodem.h"
#include "crc16.h"
#include "crc32.h"

#include "sexyz.h"
#include "telnet.h"

#define ENDOFFRAME 2
#define FRAMEOK    1
#define TIMEOUT   -1	/* rx routine did not receive a character within timeout */
#define INVHDR    -2	/* invalid header received; but within timeout */
#define ZDLEESC 0x8000	/* one of ZCRCE; ZCRCG; ZCRCQ or ZCRCW was received; ZDLE escaped */

#define BADSUBPKT	0x80

#define HDRLEN     5	/* size of a zmodem header */

static int lprintf(zmodem_t* zm, int level, const char *fmt, ...)
{
	va_list argptr;
	char	sbuf[1024];

	if(zm->lputs==NULL)
		return(-1);

    va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
    return(zm->lputs(zm->cbdata,level,sbuf));
}

static BOOL is_connected(zmodem_t* zm)
{
	if(zm->is_connected!=NULL)
		return(zm->is_connected(zm->cbdata));
	return(TRUE);
}

static BOOL is_cancelled(zmodem_t* zm)
{
	if(zm->is_cancelled!=NULL)
		return(zm->cancelled=zm->is_cancelled(zm->cbdata));
	return(zm->cancelled);
}
int zmodem_data_waiting(zmodem_t* zm, unsigned timeout)
{
	if(zm->data_waiting)
		return(zm->data_waiting(zm->cbdata, timeout));
	return(FALSE);
}

static char *chr(int ch)
{
	static char str[25];

	switch(ch) {
		case TIMEOUT:	return("TIMEOUT");
		case ZRQINIT:	return("ZRQINIT");
		case ZRINIT:	return("ZRINIT");
		case ZSINIT:	return("ZSINIT");
		case ZACK:		return("ZACK");
		case ZFILE:		return("ZFILE");
		case ZSKIP:		return("ZSKIP");
		case ZCRC:		return("ZCRC");
		case ZNAK:		return("ZNAK");
		case ZABORT:	return("ZABORT");
		case ZFIN:		return("ZFIN");
		case ZRPOS:		return("ZRPOS");
		case ZDATA:		return("ZDATA");
		case ZEOF:		return("ZEOF");
		case ZPAD:		return("ZPAD");
		case ZCAN:		return("ZCAN");
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
	if(ch>=' ' && ch<='~')
		sprintf(str,"'%c' (%02Xh)",(uchar)ch,(uchar)ch);
	else
		sprintf(str,"%u (%02Xh)",(uchar)ch,(uchar)ch);
	return(str); 
}

static char* frame_desc(int frame)
{
	static char str[25];

	if(frame==TIMEOUT)
		return("TIMEOUT");

	if(frame==INVHDR)
		return("Invalid Header");

	if(frame&BADSUBPKT)
		strcpy(str,"BAD ");
	else
		str[0]=0;

	switch(frame&~BADSUBPKT) {
		case ZRQINIT:		strcat(str,"ZRQINIT");		break;
		case ZRINIT:		strcat(str,"ZRINIT");		break;
		case ZSINIT:		strcat(str,"ZSINIT");		break;
		case ZACK:			strcat(str,"ZACK");			break;
		case ZFILE:			strcat(str,"ZFILE");		break;
		case ZSKIP:			strcat(str,"ZSKIP");		break;
		case ZNAK:			strcat(str,"ZNAK");			break;
		case ZABORT:		strcat(str,"ZABORT");		break;
		case ZFIN:			strcat(str,"ZFIN");			break;
		case ZRPOS:			strcat(str,"ZRPOS");		break;
		case ZDATA:			strcat(str,"ZDATA");		break;
		case ZEOF:			strcat(str,"ZEOF");			break;
		case ZFERR:			strcat(str,"ZFERR");		break;
		case ZCRC:			strcat(str,"ZCRC");			break;
		case ZCHALLENGE:	strcat(str,"ZCHALLENGE");	break;
		case ZCOMPL:		strcat(str,"ZCOMPL");		break;
		case ZCAN:			strcat(str,"ZCAN");			break;
		case ZFREECNT:		strcat(str,"ZFREECNT");		break;
		case ZCOMMAND:		strcat(str,"ZCOMMAND");		break;	
		case ZSTDERR:		strcat(str,"ZSTDERR");		break;		
		default: 
			sprintf(str,"Unknown (%08X)", frame);
			break;
	}
	return(str); 
}


/*
 * read bytes as long as rdchk indicates that
 * more data is available.
 */

void zmodem_recv_purge(zmodem_t* zm)
{
	while(zm->recv_byte(zm->cbdata,0)>=0);
}

/* 
 * transmit a character. 
 * this is the raw modem interface
 */

int zmodem_send_raw(zmodem_t* zm, unsigned char ch)
{
	int	result;

	if((result=zm->send_byte(zm->cbdata,ch,zm->send_timeout))!=0)
		lprintf(zm,LOG_ERR,"send_raw SEND ERROR: %d",result);

	zm->last_sent = ch;

	return result;
}

/*
 * transmit a character ZDLE escaped
 */

int zmodem_send_esc(zmodem_t* zm, unsigned char c)
{
	int	result;

	if((result=zmodem_send_raw(zm, ZDLE))!=0)
		return(result);
	/*
	 * exclusive or; not an or so ZDLE becomes ZDLEE
	 */
	return zmodem_send_raw(zm, (uchar)(c ^ 0x40));
}

/*
 * transmit a character; ZDLE escaping if appropriate
 */

int zmodem_tx(zmodem_t* zm, unsigned char c)
{
	int result;

	switch (c) {
		case DLE:
		case DLE|0x80:          /* even if high-bit set */
		case XON:
		case XON|0x80:
		case XOFF:
		case XOFF|0x80:
		case ZDLE:
			return zmodem_send_esc(zm, c);
		case CR:
		case CR|0x80:
			if(zm->escape_ctrl_chars && (zm->last_sent&0x7f) == '@')
				return zmodem_send_esc(zm, c);
			break;
		case TELNET_IAC:
			if(zm->escape_telnet_iac) {
				if((result=zmodem_send_raw(zm, ZDLE))!=0)
					return(result);
				return zmodem_send_raw(zm, ZRUB1);
			}
			break;
		default:
			if(zm->escape_ctrl_chars && (c&0x60)==0)
				return zmodem_send_esc(zm, c);
			break;
	}
	/*
	 * anything that ends here is so normal we might as well transmit it.
	 */
	return zmodem_send_raw(zm, c);
}

/**********************************************/
/* Output single byte as two hex ASCII digits */
/**********************************************/
int zmodem_send_hex(zmodem_t* zm, uchar val)
{
	char* xdigit="0123456789abcdef";
	int	result;

	lprintf(zm,LOG_DEBUG,"send_hex: %02X ",val);

	if((result=zmodem_send_raw(zm, xdigit[val>>4]))!=0)
		return result;
	return zmodem_send_raw(zm, xdigit[val&0xf]);
}

int zmodem_send_padded_zdle(zmodem_t* zm)
{
	int result;

	if((result=zmodem_send_raw(zm, ZPAD))!=0)
		return result;
	if((result=zmodem_send_raw(zm, ZPAD))!=0)
		return result;
	return zmodem_send_raw(zm, ZDLE);
}

/* 
 * transmit a hex header.
 * these routines use tx_raw because we're sure that all the
 * characters are not to be escaped.
 */
int zmodem_send_hex_header(zmodem_t* zm, unsigned char * p)
{
	int i;
	int result;
	uchar type=*p;
	unsigned short int crc;

	lprintf(zm,LOG_DEBUG,"send_hex_header: %s", chr(type));

	if((result=zmodem_send_padded_zdle(zm))!=0)
		return result;

	if((result=zmodem_send_raw(zm, ZHEX))!=0)
		return result;

	/*
 	 * initialise the crc
	 */

	crc = 0;

	/*
 	 * transmit the header
	 */

	for(i=0;i<HDRLEN;i++) {
		if((result=zmodem_send_hex(zm, *p))!=0)
			return result;
		crc = ucrc16(*p, crc);
		p++;
	}

	/*
 	 * update the crc as though it were zero
	 */

	/* 
	 * transmit the crc
	 */

	if((result=zmodem_send_hex(zm, (uchar)(crc>>8)))!=0)
		return result;
	if((result=zmodem_send_hex(zm, (uchar)(crc&0xff)))!=0)
		return result;

	/*
	 * end of line sequence
	 */

	if((result=zmodem_send_raw(zm, '\r'))!=0)
		return result;
	if((result=zmodem_send_raw(zm, '\n'))!=0)	/* FDSZ sends 0x8a instead of 0x0a */
		return result;

	if(type!=ZACK && type!=ZFIN)
		result=zmodem_send_raw(zm, XON);

	return(result);
}

/*
 * Send ZMODEM binary header hdr
 */

int zmodem_send_bin32_header(zmodem_t* zm, unsigned char * p)
{
	int i;
	int result;
	unsigned long crc;

	lprintf(zm,LOG_DEBUG,"send_bin32_header: %s", chr(*p));

	if((result=zmodem_send_padded_zdle(zm))!=0)
		return result;

	if((result=zmodem_send_raw(zm, ZBIN32))!=0)
		return result;

	crc = 0xffffffffL;

	for(i=0;i<HDRLEN;i++) {
		crc = ucrc32(*p,crc);
		if((result=zmodem_tx(zm, *p++))!=0)
			return result;
	}

	crc = ~crc;

	if((result=	zmodem_tx(zm, (uchar)((crc      ) & 0xff)))!=0)
		return result;
	if((result=	zmodem_tx(zm, (uchar)((crc >>  8) & 0xff)))!=0)
		return result;
	if((result=	zmodem_tx(zm, (uchar)((crc >> 16) & 0xff)))!=0)
		return result;
	return		zmodem_tx(zm, (uchar)((crc >> 24) & 0xff));
}

int zmodem_send_bin16_header(zmodem_t* zm, unsigned char * p)
{
	int i;
	int result;
	unsigned int crc;

	lprintf(zm,LOG_DEBUG,"send_bin16_header: %s", chr(*p));

	if((result=zmodem_send_padded_zdle(zm))!=0)
		return result;

	if((result=zmodem_send_raw(zm, ZBIN))!=0)
		return result;

	crc = 0;

	for(i=0;i<HDRLEN;i++) {
		crc = ucrc16(*p,crc);
		if((result=zmodem_tx(zm, *p++))!=0)
			return result;
	}

	if((result=	zmodem_tx(zm, (uchar)(crc >> 8)))!=0)
		return result;
	return		zmodem_tx(zm, (uchar)(crc&0xff));
}


/* 
 * transmit a header using either hex 16 bit crc or binary 32 bit crc
 * depending on the receivers capabilities
 * we dont bother with variable length headers. I dont really see their
 * advantage and they would clutter the code unneccesarily
 */

int zmodem_send_bin_header(zmodem_t* zm, unsigned char * p)
{
	if(zm->can_fcs_32 && !zm->want_fcs_16)
		return zmodem_send_bin32_header(zm, p);
	return zmodem_send_bin16_header(zm, p);
}

/*
 * data subpacket transmission
 */

int zmodem_send_data32(zmodem_t* zm, uchar subpkt_type, unsigned char * p, size_t l)
{
	int	result;
	unsigned long crc;

	lprintf(zm,LOG_DEBUG,"send_data32: %s (%u bytes)", chr(subpkt_type), l);

	crc = 0xffffffffl;

	while(l > 0) {
		crc = ucrc32(*p,crc);
		if((result=zmodem_tx(zm, *p++))!=0)
			return result;
		l--;
	}

	crc = ucrc32(subpkt_type, crc);

	if((result=zmodem_send_raw(zm, ZDLE))!=0)
		return result;
	if((result=zmodem_send_raw(zm, subpkt_type))!=0)
		return result;

	crc = ~crc;

	if((result=	zmodem_tx(zm, (uchar) ((crc      ) & 0xff)))!=0)
		return result;
	if((result=	zmodem_tx(zm, (uchar) ((crc >> 8 ) & 0xff)))!=0)
		return result;
	if((result=	zmodem_tx(zm, (uchar) ((crc >> 16) & 0xff)))!=0)
		return result;
	return		zmodem_tx(zm, (uchar) ((crc >> 24) & 0xff));
}

int zmodem_send_data16(zmodem_t* zm, uchar subpkt_type,unsigned char * p, size_t l)
{
	int	result;
	unsigned short crc;

	lprintf(zm,LOG_DEBUG,"send_data16: %s (%u bytes)", chr(subpkt_type), l);

	crc = 0;

	while(l > 0) {
		crc = ucrc16(*p,crc);
		if((result=zmodem_tx(zm, *p++))!=0)
			return result;
		l--;
	}

	crc = ucrc16(subpkt_type,crc);

	if((result=zmodem_send_raw(zm, ZDLE))!=0)
		return result;
	if((result=zmodem_send_raw(zm, subpkt_type))!=0)
		return result;
	
	if((result=	zmodem_tx(zm, (uchar)(crc >> 8)))!=0)
		return result;
	return		zmodem_tx(zm, (uchar)(crc&0xff));
}

/*
 * send a data subpacket using crc 16 or crc 32 as desired by the receiver
 */

int zmodem_send_data(zmodem_t* zm, uchar subpkt_type, unsigned char * p, size_t l)
{
	int result;

	if(!zm->want_fcs_16 && zm->can_fcs_32) {
		if((result=zmodem_send_data32(zm, subpkt_type,p,l))!=0)
			return result;
	}
	else {	
		if((result=zmodem_send_data16(zm, subpkt_type,p,l))!=0)
			return result;
	}

	if(subpkt_type == ZCRCW)
		result=zmodem_send_raw(zm, XON);

	return result;
}

int zmodem_send_pos_header(zmodem_t* zm, int type, long pos, BOOL hex) 
{
	uchar header[5];

	header[0]   = type;
	header[ZP0] = (uchar) (pos        & 0xff);
	header[ZP1] = (uchar)((pos >>  8) & 0xff);
	header[ZP2] = (uchar)((pos >> 16) & 0xff);
	header[ZP3] = (uchar)((pos >> 24) & 0xff);

	if(hex)
		return zmodem_send_hex_header(zm, header);
	else
		return zmodem_send_bin_header(zm, header);
}

int zmodem_send_ack(zmodem_t* zm, long pos)
{
	return zmodem_send_pos_header(zm, ZACK, pos, /* Hex? */ TRUE);
}

int zmodem_send_nak(zmodem_t* zm)
{
	return zmodem_send_pos_header(zm, ZNAK, 0, /* Hex? */ TRUE);
}

int zmodem_send_zfin(zmodem_t* zm)
{
	unsigned char zfin_header[] = { ZFIN, 0, 0, 0, 0 };

	return zmodem_send_hex_header(zm,zfin_header);
}

int zmodem_abort_receive(zmodem_t* zm)
{
	return zmodem_send_pos_header(zm, ZABORT, 0, /* Hex? */ TRUE);
}

int zmodem_send_znak(zmodem_t* zm)
{
	return zmodem_send_pos_header(zm, ZNAK, zm->ack_file_pos, /* Hex? */ TRUE);
}

int zmodem_send_zskip(zmodem_t* zm)
{
	return zmodem_send_pos_header(zm, ZSKIP, 0L, /* Hex? */ TRUE);
}

int zmodem_send_zeof(zmodem_t* zm)
{
	return zmodem_send_pos_header(zm, ZEOF, zm->current_file_size, /* Hex? */ TRUE);
}


/*
 * rx_raw ; receive a single byte from the line.
 * reads as many are available and then processes them one at a time
 * check the data stream for 5 consecutive CAN characters;
 * and if you see them abort. this saves a lot of clutter in
 * the rest of the code; even though it is a very strange place
 * for an exit. (but that was wat session abort was all about.)
 */

int zmodem_recv_raw(zmodem_t* zm)
{
	int c;

	if((c=zm->recv_byte(zm->cbdata,zm->recv_timeout)) < 0)
		return(TIMEOUT);

	if(c == CAN) {
		zm->n_cans++;
		if(zm->n_cans == 5) {
			zm->cancelled=TRUE;
			lprintf(zm,LOG_WARNING,"recv_raw: Cancelled remotely");
/*			return(TIMEOUT);	removed June-12-2005 */
		}
	}
	else {
		zm->n_cans = 0;
	}

	return c;
}

/*
 * rx; receive a single byte undoing any escaping at the
 * sending site. this bit looks like a mess. sorry for that
 * but there seems to be no other way without incurring a lot
 * of overhead. at least like this the path for a normal character
 * is relatively short.
 */

int zmodem_rx(zmodem_t* zm)
{
	int c;

	/*
	 * outer loop for ever so for sure something valid
	 * will come in; a timeout will occur or a session abort
	 * will be received.
	 */

	while(is_connected(zm)) {

		while(TRUE) {
			if((c = zmodem_recv_raw(zm)) < 0)
				return(c);
	
			switch (c) {
				case ZDLE:
					break;
				case XON:
				case XON|0x80:
				case XOFF:
				case XOFF|0x80:
					continue;			
				default:
					/*
	 				 * if all control characters should be escaped and 
					 * this one wasnt then its spurious and should be dropped.
					 */
					if(zm->escape_ctrl_chars && (c & 0x60) == 0) {
						continue;
					}
					/*
					 * normal character; return it.
					 */
					return c;
			}
			break;
		}
	
		/*
	 	 * ZDLE encoded sequence or session abort.
		 * (or something illegal; then back to the top)
		 */

		while(TRUE) {
			if((c = zmodem_recv_raw(zm)) < 0)
				return(c);

			if(c == XON || c == (XON|0x80) || c == XOFF || c == (XOFF|0x80) || c == ZDLE) {
				/*
				 * these can be dropped.
				 */
				continue;
			}

			switch (c) {
				/*
				 * these four are really nasty.
				 * for convenience we just change them into 
				 * special characters by setting a bit outside the
				 * first 8. that way they can be recognized and still
				 * be processed as characters by the rest of the code.
				 */
				case ZCRCE:
				case ZCRCG:
				case ZCRCQ:
				case ZCRCW:
					lprintf(zm,LOG_DEBUG,"rx: encoding data subpacket type: %s"
						,chr(c));
					return (c | ZDLEESC);
				case ZRUB0:
					return 0x7f;
				case ZRUB1:
					return 0xff;
				default:
					if(zm->escape_ctrl_chars && (c & 0x60) == 0) {
						/*
						 * a not escaped control character; probably
						 * something from a network. just drop it.
						 */
						continue;
					}
					/*
					 * legitimate escape sequence.
					 * rebuild the orignal and return it.
					 */
					if((c & 0x60) == 0x40) {
						return c ^ 0x40;
					}
					lprintf(zm,LOG_WARNING,"rx: illegal sequence: ZDLE %s"
						,chr(c));
					break;
			}
			break;
		} 
	}

	/*
	 * not reached.
	 */

	return 0;
}

/*
 * receive a data subpacket as dictated by the last received header.
 * return 2 with correct packet and end of frame
 * return 1 with correct packet frame continues
 * return 0 with incorrect frame.
 * return TIMEOUT with a timeout
 * if an acknowledgement is requested it is generated automatically
 * here. 
 */

/*
 * data subpacket reception
 */

int zmodem_recv_data32(zmodem_t* zm, unsigned char * p, unsigned maxlen, unsigned* l)
{
	int c;
	unsigned long rxd_crc;
	unsigned long crc;
	int subpkt_type;

	lprintf(zm,LOG_DEBUG,"recv_data32");

	crc = 0xffffffffl;

	do {
		c = zmodem_rx(zm);

		if(c == TIMEOUT) {
			return TIMEOUT;
		}
		if(c < 0x100 && *l < maxlen) {
			crc = ucrc32(c,crc);
			*p++ = c;
			(*l)++;
			continue;
		}
	} while(c < 0x100);

	subpkt_type = c & 0xff;

	crc = ucrc32(subpkt_type, crc);

	crc = ~crc;

	rxd_crc  = zmodem_rx(zm);
	rxd_crc |= zmodem_rx(zm) << 8;
	rxd_crc |= zmodem_rx(zm) << 16;
	rxd_crc |= zmodem_rx(zm) << 24;

	if(rxd_crc != crc) {
		lprintf(zm,LOG_WARNING,"CRC32 ERROR (%08lX, expected: %08lX) Bytes=%u, subpacket-type=%s"
			,rxd_crc, crc, *l, chr(subpkt_type));
		return FALSE;
	}
	lprintf(zm,LOG_DEBUG,"GOOD CRC32: %08lX (Bytes=%u, subpacket-type=%s)"
		,crc, *l, chr(subpkt_type));

	zm->ack_file_pos += *l;

	return subpkt_type;
}

int zmodem_recv_data16(zmodem_t* zm, register unsigned char* p, unsigned maxlen, unsigned* l)
{
	int c;
	int subpkt_type;
 	unsigned short crc;
	unsigned short rxd_crc;

	lprintf(zm,LOG_DEBUG,"recv_data16");

	crc = 0;

	do {
		c = zmodem_rx(zm);

		if(c == TIMEOUT) {
			return TIMEOUT;
		}
		if(c < 0x100 && *l < maxlen) {
			crc = ucrc16(c,crc);
			*p++ = c;
			(*l)++;
		}
	} while(c < 0x100);

	subpkt_type = c & 0xff;

	crc = ucrc16(subpkt_type,crc);

	rxd_crc  = zmodem_rx(zm) << 8;
	rxd_crc |= zmodem_rx(zm);

	if(rxd_crc != crc) {
		lprintf(zm,LOG_WARNING,"CRC16 ERROR (%04hX, expected: %04hX) Bytes=%d"
			,rxd_crc, crc, *l);
		return FALSE;
	}
	lprintf(zm,LOG_DEBUG,"GOOD CRC16: %04hX (Bytes=%d)", crc, *l);

	zm->ack_file_pos += *l;

	return subpkt_type;
}

int zmodem_recv_data(zmodem_t* zm, unsigned char* p, size_t maxlen, unsigned* l, BOOL ack)
{
	int subpkt_type;
	unsigned n=0;

	if(l==NULL)
		l=&n;

	lprintf(zm,LOG_DEBUG,"recv_data (%u-bit)", zm->receive_32bit_data ? 32:16);

	/*
	 * receive the right type of frame
	 */

	*l = 0;

	if(zm->receive_32bit_data) {
		subpkt_type = zmodem_recv_data32(zm, p, maxlen, l);
	}
	else {	
		subpkt_type = zmodem_recv_data16(zm, p, maxlen, l);
	}

	if(subpkt_type==FALSE)
		return(FALSE);
	
	if(subpkt_type==TIMEOUT)
		return(TIMEOUT);
	
	lprintf(zm,LOG_DEBUG,"recv_data received subpacket-type: %s"
		,chr(subpkt_type));

	switch (subpkt_type)  {
		/*
		 * frame continues non-stop
		 */
		case ZCRCG:
			return FRAMEOK;
		/*
		 * frame ends
		 */
		case ZCRCE:
			return ENDOFFRAME;
		/*
 		 * frame continues; ZACK expected
		 */
		case ZCRCQ:		
			if(ack)
				zmodem_send_ack(zm, zm->ack_file_pos);
			return FRAMEOK;
		/*
		 * frame ends; ZACK expected
		 */
		case ZCRCW:
			if(ack)
				zmodem_send_ack(zm, zm->ack_file_pos);
			return ENDOFFRAME;
	}

	lprintf(zm,LOG_WARNING,"Invalid subpacket-type: %s",chr(subpkt_type));

	return FALSE;
}

BOOL zmodem_recv_subpacket(zmodem_t* zm, BOOL ack)
{
	int type;

	type=zmodem_recv_data(zm,zm->rx_data_subpacket,sizeof(zm->rx_data_subpacket),NULL,ack);
	if(type!=FRAMEOK && type!=ENDOFFRAME) {
		zmodem_send_nak(zm);
		return(FALSE);
	}

	return(TRUE);
}

int zmodem_recv_nibble(zmodem_t* zm) 
{
	int c;

	c = zmodem_rx(zm);

	if(c == TIMEOUT) {
		return c;
	}

	if(c > '9') {
		if(c < 'a' || c > 'f') {
			/*
			 * illegal hex; different than expected.
			 * we might as well time out.
			 */
			return TIMEOUT;
		}

		c -= 'a' - 10;
	}
	else {
		if(c < '0') {
			/*
			 * illegal hex; different than expected.
			 * we might as well time out.
			 */
			return TIMEOUT;
		}
		c -= '0';
	}

	return c;
}

int zmodem_recv_hex(zmodem_t* zm)
{
	int n1;
	int n0;
	int ret;

	n1 = zmodem_recv_nibble(zm);

	if(n1 == TIMEOUT) {
		return n1;
	}

	n0 = zmodem_recv_nibble(zm);

	if(n0 == TIMEOUT) {
		return n0;
	}

	ret = (n1 << 4) | n0;

	lprintf(zm,LOG_DEBUG,"recv_hex returning: 0x%02X", ret);

	return ret;
}

/*
 * receive routines for each of the six different styles of header.
 * each of these leaves zm->rxd_header_len set to 0 if the end result is
 * not a valid header.
 */

BOOL zmodem_recv_bin16_header(zmodem_t* zm)
{
	int c;
	int n;
	unsigned short int crc;
	unsigned short int rxd_crc;

	lprintf(zm,LOG_DEBUG,"recv_bin16_header");

	crc = 0;

	for(n=0;n<HDRLEN;n++) {
		c = zmodem_rx(zm);
		if(c == TIMEOUT) {
			lprintf(zm,LOG_WARNING,"recv_bin16_header: timeout");
			return(FALSE);
		}
		crc = ucrc16(c,crc);
		zm->rxd_header[n] = c;
	}

	rxd_crc  = zmodem_rx(zm) << 8;
	rxd_crc |= zmodem_rx(zm);

	if(rxd_crc != crc) {
		lprintf(zm,LOG_WARNING,"CRC16 ERROR: 0x%hX, expected: 0x%hX", rxd_crc, crc);
		return(FALSE);
	}
	lprintf(zm,LOG_DEBUG,"GOOD CRC16: %04hX", crc);

	zm->rxd_header_len = 5;

	return(TRUE);
}

void zmodem_recv_hex_header(zmodem_t* zm)
{
	int c;
	int i;
	unsigned short int crc = 0;
	unsigned short int rxd_crc;

	lprintf(zm,LOG_DEBUG,"recv_hex_header");

	for(i=0;i<HDRLEN;i++) {
		c = zmodem_recv_hex(zm);
		if(c == TIMEOUT) {
			return;
		}
		crc = ucrc16(c,crc);

		zm->rxd_header[i] = c;
	}

	/*
	 * receive the crc
	 */

	c = zmodem_recv_hex(zm);

	if(c == TIMEOUT) {
		return;
	}

	rxd_crc = c << 8;

	c = zmodem_recv_hex(zm);

	if(c == TIMEOUT) {
		return;
	}

	rxd_crc |= c;

	if(rxd_crc == crc) {
		lprintf(zm,LOG_DEBUG,"GOOD CRC16: %04hX", crc);
		zm->rxd_header_len = 5;
	}
	else {
		lprintf(zm,LOG_WARNING,"CRC16 ERROR: 0x%hX, expected: 0x%hX", rxd_crc, crc);
	}

	/*
	 * drop the end of line sequence after a hex header
	 */
	c = zmodem_rx(zm);
	if(c == '\r') {
		/*
		 * both are expected with CR
		 */
		zmodem_rx(zm);	/* drop LF */
	}
}

BOOL zmodem_recv_bin32_header(zmodem_t* zm)
{
	int c;
	int n;
	unsigned long crc;
	unsigned long rxd_crc;

	lprintf(zm,LOG_DEBUG,"recv_bin32_header");

	crc = 0xffffffffL;

	for(n=0;n<HDRLEN;n++) {
		c = zmodem_rx(zm);
		if(c == TIMEOUT) {
			return(TRUE);
		}
		crc = ucrc32(c,crc);
		zm->rxd_header[n] = c;
	}

	crc = ~crc;

	rxd_crc  = zmodem_rx(zm);
	rxd_crc |= zmodem_rx(zm) << 8;
	rxd_crc |= zmodem_rx(zm) << 16;
	rxd_crc |= zmodem_rx(zm) << 24;

	if(rxd_crc != crc) {
		lprintf(zm,LOG_WARNING,"CRC32 ERROR (%08lX, expected: %08lX)"
			,rxd_crc, crc);
		return(FALSE);
	}
	lprintf(zm,LOG_DEBUG,"GOOD CRC32: %08lX", crc);

	zm->rxd_header_len = 5;
	return(TRUE);
}

/*
 * receive any style header
 * if the errors flag is set than whenever an invalid header packet is
 * received INVHDR will be returned. otherwise we wait for a good header
 * also; a flag (receive_32bit_data) will be set to indicate whether data
 * packets following this header will have 16 or 32 bit data attached.
 * variable headers are not implemented.
 */

int zmodem_recv_header_raw(zmodem_t* zm, int errors)
{
	int c;
	int	frame_type;

	lprintf(zm,LOG_DEBUG,"recv_header_raw");

	zm->rxd_header_len = 0;

	do {
		do {
			if((c = zmodem_recv_raw(zm)) < 0)
				return(c);
			if(is_cancelled(zm))
				return(ZCAN);
		} while(c != ZPAD);

		if((c = zmodem_recv_raw(zm)) < 0)
			return(c);

		if(c == ZPAD) {
			if((c = zmodem_recv_raw(zm)) < 0)
				return(c);
		}

		/*
		 * spurious ZPAD check
		 */

		if(c != ZDLE) {
			lprintf(zm,LOG_WARNING,"recv_header_raw: Expected ZDLE, received: %s"
				,chr(c));
			continue;
		}

		/*
		 * now read the header style
		 */

		c = zmodem_rx(zm);

		if(c == TIMEOUT) {
			lprintf(zm,LOG_WARNING,"recv_header_raw: TIMEOUT");
			return c;
		}

		switch (c) {
			case ZBIN:
				zmodem_recv_bin16_header(zm);
				zm->receive_32bit_data = FALSE;
				break;
			case ZHEX:
				zmodem_recv_hex_header(zm);
				zm->receive_32bit_data = FALSE;
				break;
			case ZBIN32:
				zmodem_recv_bin32_header(zm);
				zm->receive_32bit_data = TRUE;
				break;
			default:
				/*
				 * unrecognized header style
				 */
				lprintf(zm,LOG_ERR,"recv_header_raw: UNRECOGNIZED header style: %s"
					,chr(c));
				if(errors) {
					return INVHDR;
				}

				continue;
		}
		if(errors && zm->rxd_header_len == 0) {
			return INVHDR;
		}

	} while(zm->rxd_header_len == 0 && !is_cancelled(zm));

	if(is_cancelled(zm))
		return(ZCAN);

	/*
 	 * this appears to have been a valid header.
	 * return its type.
	 */

	frame_type = zm->rxd_header[0];

	zm->rxd_header_pos = zm->rxd_header[ZP0] | (zm->rxd_header[ZP1] << 8) |
				(zm->rxd_header[ZP2] << 16) | (zm->rxd_header[ZP3] << 24);

	switch(frame_type) {
		case ZCRC:
			zm->crc_request = zm->rxd_header_pos;
			break;
		case ZDATA:
			zm->ack_file_pos = zm->rxd_header_pos;
			break;
		case ZFILE:
			zm->ack_file_pos = 0l;
			if(!zmodem_recv_subpacket(zm,/* ack? */FALSE))
				frame_type |= BADSUBPKT;
			break;
		case ZSINIT:
		case ZCOMMAND:
			if(!zmodem_recv_subpacket(zm,/* ack? */TRUE))
				frame_type |= BADSUBPKT;
			break;
		case ZFREECNT:
			zmodem_send_pos_header(zm, ZACK, getfreediskspace(".",1), /* Hex? */ TRUE);
			break;
	}

#if 0 /*def _DEBUG */
	lprintf(zm,LOG_DEBUG,"recv_header_raw received header type: %s"
		,frame_desc(frame_type));
#endif

	return frame_type;
}

int zmodem_recv_header(zmodem_t* zm)
{
	int ret;
	
	ret = zmodem_recv_header_raw(zm, FALSE);

	if(ret == TIMEOUT)
		lprintf(zm,LOG_WARNING,"recv_header TIMEOUT");
	else if(ret == INVHDR)
		lprintf(zm,LOG_WARNING,"recv_header detected an invalid header");
	else
		lprintf(zm,LOG_DEBUG,"recv_header returning: %s (pos=%ld)"
			,frame_desc(ret), zm->rxd_header_pos);

	if(ret==ZCAN)
		zm->cancelled=TRUE;

	return ret;
}

int zmodem_recv_header_and_check(zmodem_t* zm)
{
	int type;

	while(is_connected(zm)) {
		type = zmodem_recv_header_raw(zm,TRUE);		

		if(type != INVHDR && (type&BADSUBPKT) == 0) {
			break;
		}

		zmodem_send_znak(zm);
	}

	return type;
}

BOOL zmodem_get_crc(zmodem_t* zm, long length, ulong* crc)
{
	zmodem_send_pos_header(zm,ZCRC,length,TRUE);
	if(!zmodem_data_waiting(zm,zm->crc_timeout*1000))
		return(FALSE);
	if(zmodem_recv_header(zm)!=ZCRC)
		return(FALSE);
	if(crc!=NULL)
		*crc = zm->crc_request;
	return(TRUE);
}

void zmodem_parse_zrinit(zmodem_t* zm)
{
	zm->can_full_duplex					= INT_TO_BOOL(zm->rxd_header[ZF0] & ZF0_CANFDX);
	zm->can_overlap_io					= INT_TO_BOOL(zm->rxd_header[ZF0] & ZF0_CANOVIO);
	zm->can_break						= INT_TO_BOOL(zm->rxd_header[ZF0] & ZF0_CANBRK);
	zm->can_fcs_32						= INT_TO_BOOL(zm->rxd_header[ZF0] & ZF0_CANFC32);
	zm->escape_ctrl_chars				= INT_TO_BOOL(zm->rxd_header[ZF0] & ZF0_ESCCTL);
	zm->escape_8th_bit					= INT_TO_BOOL(zm->rxd_header[ZF0] & ZF0_ESC8);

	lprintf(zm,LOG_INFO,"Receiver requested mode (0x%02X):\r\n"
		"%s-duplex, %s overlap I/O, CRC-%u, Escape: %s"
		,zm->rxd_header[ZF0]
		,zm->can_full_duplex ? "Full" : "Half"
		,zm->can_overlap_io ? "Can" : "Cannot"
		,zm->can_fcs_32 ? 32 : 16
		,zm->escape_ctrl_chars ? "ALL" : "Normal"
		);

	if((zm->recv_bufsize = (zm->rxd_header[ZP0] | zm->rxd_header[ZP1]<<8)) != 0)
		lprintf(zm,LOG_INFO,"Receiver specified buffer size of: %u", zm->recv_bufsize);
}

int zmodem_get_zrinit(zmodem_t* zm)
{
	unsigned char zrqinit_header[] = { ZRQINIT, /* ZF3: */0, 0, 0, /* ZF0: */0 };
	/* Note: sz/dsz/fdsz sends 0x80 in ZF3 because it supports var-length headers. */
	/* We do not, so we send 0x00, resulting in a CRC-16 value of 0x0000 as well. */

	zmodem_send_raw(zm,'r');
	zmodem_send_raw(zm,'z');
	zmodem_send_raw(zm,'\r');
	zmodem_send_hex_header(zm,zrqinit_header);
	
	if(!zmodem_data_waiting(zm,zm->init_timeout*1000))
		return(TIMEOUT);
	return zmodem_recv_header(zm);
}

int zmodem_send_zrinit(zmodem_t* zm)
{
	unsigned char zrinit_header[] = { ZRINIT, 0, 0, 0, 0 };
	
	zrinit_header[ZF0] = ZF0_CANFDX;

	if(!zm->no_streaming)
		zrinit_header[ZF0] |= ZF0_CANOVIO;

	if(zm->can_break)
		zrinit_header[ZF0] |= ZF0_CANBRK;

	if(!zm->want_fcs_16)
		zrinit_header[ZF0] |= ZF0_CANFC32;

	if(zm->escape_ctrl_chars)
		zrinit_header[ZF0] |= ZF0_ESCCTL;

	if(zm->escape_8th_bit)
		zrinit_header[ZF0] |= ZF0_ESC8;

	if(zm->no_streaming && zm->recv_bufsize==0)
		zm->recv_bufsize = sizeof(zm->rx_data_subpacket);

	zrinit_header[ZP0] = zm->recv_bufsize & 0xff;
	zrinit_header[ZP1] = zm->recv_bufsize >> 8;

	return zmodem_send_hex_header(zm, zrinit_header);
}

int zmodem_get_zfin(zmodem_t* zm)
{
	int type;

	zmodem_send_zfin(zm);
	do {
		type = zmodem_recv_header(zm);
	} while(type != ZFIN && type != TIMEOUT && is_connected(zm));
	
	/*
	 * these Os are formally required; but they don't do a thing
	 * unfortunately many programs require them to exit 
	 * (both programs already sent a ZFIN so why bother ?)
	 */

	if(type != TIMEOUT) {
		zmodem_send_raw(zm,'O');
		zmodem_send_raw(zm,'O');
	}

	return 0;
}


/*
 * send from the current position in the file
 * all the way to end of file or until something goes wrong.
 * (ZNAK or ZRPOS received)
 * the name is only used to show progress
 */

int zmodem_send_from(zmodem_t* zm, FILE* fp, ulong pos, ulong* sent)
{
	size_t n;
	uchar type;
	unsigned buf_sent=0;

	if(sent!=NULL)
		*sent=0;

	fseek(fp,pos,SEEK_SET);

	zmodem_send_pos_header(zm, ZDATA, pos, /* Hex? */ FALSE);

	/*
	 * send the data in the file
	 */

	while(is_connected(zm)) {

		/*
		 * read a block from the file
		 */

		n = fread(zm->tx_data_subpacket,sizeof(BYTE),zm->block_size,fp);

		if(zm->progress!=NULL)
			zm->progress(zm->cbdata, ftell(fp));

		type = ZCRCG;

		/** ZMODEM.DOC:
			ZCRCW data subpackets expect a response before the next frame is sent.
			If the receiver does not indicate overlapped I/O capability with the
			CANOVIO bit, or sets a buffer size, the sender uses the ZCRCW to allow
			the receiver to write its buffer before sending more data.
		***/
		if(!zm->can_overlap_io || zm->no_streaming
			|| (zm->recv_bufsize && buf_sent+n>=zm->recv_bufsize)) {
			type=ZCRCW;
			buf_sent=0;
		}

		if((ulong)ftell(fp) >= zm->current_file_size || n==0)	/* can't use feof() here! */
			type = ZCRCE;

		if(zmodem_send_data(zm, type, zm->tx_data_subpacket, n)!=0)
			return(TIMEOUT);

		if(type == ZCRCW || type == ZCRCE) {	
			int ack;
			lprintf(zm,LOG_DEBUG,"Sent end-of-frame (%s sub-packet)", chr(type));

			if(type==ZCRCW) {	/* ZACK expected */

				lprintf(zm,LOG_DEBUG,"Waiting for ZACK");
				while(is_connected(zm)) {
					if((ack = zmodem_recv_header(zm)) != ZACK)
						return(ack);

					if(is_cancelled(zm))
						return(ZCAN);

					if(zm->rxd_header_pos == (ulong)ftell(fp))
						break;
					lprintf(zm,LOG_WARNING,"ZACK for incorrect offset (%lu vs %lu)"
						,zm->rxd_header_pos, ftell(fp));
				} 

			}
		}

		if(sent!=NULL)
			*sent+=n;

		buf_sent+=n;

		if((ulong)ftell(fp) >= zm->current_file_size) {
			lprintf(zm,LOG_DEBUG,"send_from: end of file (%ld)", zm->current_file_size );
			return ZACK;
		}
		if(n==0) {
			lprintf(zm,LOG_ERR,"send_from: read error %d at offset %lu"
				,ferror(fp), ftell(fp) );
			return ZACK;
		}

		/* 
		 * characters from the other side
		 * check out that header
		 */

		while(zmodem_data_waiting(zm, zm->consecutive_errors ? 1000:0) 
			&& !is_cancelled(zm) && is_connected(zm)) {
			int type;
			int c;
			lprintf(zm,LOG_DEBUG,"Back-channel traffic detected:");
			if((c = zmodem_recv_raw(zm)) < 0)
				return(c);
			if(c == ZPAD) {
				type = zmodem_recv_header(zm);
				if(type != TIMEOUT && type != ZACK) {
					return type;
				}
			} else
				lprintf(zm,LOG_DEBUG,"Received: %s",chr(c));
		}
		if(is_cancelled(zm))
			return(ZCAN);

		zm->consecutive_errors = 0;

		if(zm->block_size < zm->max_block_size) {
			zm->block_size*=2;
			if(zm->block_size > zm->max_block_size)
				zm->block_size = zm->max_block_size;
		}

		if(type == ZCRCW || type == ZCRCE)	/* end-of-frame */
			zmodem_send_pos_header(zm, ZDATA, ftell(fp), /* Hex? */ FALSE);
	}

	lprintf(zm,LOG_DEBUG,"send_from: returning unexpectedly!");

	/*
	 * end of file reached.
	 * should receive something... so fake ZACK
	 */

	return ZACK;
}

/*
 * send a file; returns true when session is aborted.
 * (using ZABORT frame)
 */

BOOL zmodem_send_file(zmodem_t* zm, char* fname, FILE* fp, BOOL request_init, time_t* start, ulong* sent)
{
	BOOL	success=FALSE;
	ulong	pos=0;
	ulong	sent_bytes;
	struct	stat s;
	unsigned char * p;
	uchar	zfile_frame[] = { ZFILE, 0, 0, 0, 0 };
	int		type;
	int		i;
	unsigned attempts;

	if(zm->block_size == 0)
		zm->block_size = ZBLOCKLEN;	

	if(zm->block_size < 128)
		zm->block_size = 128;	

	if(zm->block_size > sizeof(zm->tx_data_subpacket))
		zm->block_size = sizeof(zm->tx_data_subpacket);

	if(zm->max_block_size < zm->block_size)
		zm->max_block_size = zm->block_size;

	if(zm->max_block_size > sizeof(zm->rx_data_subpacket))
		zm->max_block_size = sizeof(zm->rx_data_subpacket);

	if(sent!=NULL)	
		*sent=0;

	if(start!=NULL)		
		*start=time(NULL);

	zm->file_skipped=FALSE;

	if(zm->no_streaming)
		lprintf(zm,LOG_WARNING,"Streaming disabled");

	if(request_init) {
		for(zm->errors=0; zm->errors<=zm->max_errors && !is_cancelled(zm) && is_connected(zm); zm->errors++) {
			lprintf(zm,LOG_INFO,"Sending ZRQINIT (%u of %u)"
				,zm->errors+1,zm->max_errors+1);
			i = zmodem_get_zrinit(zm);
			if(i == ZRINIT) {
				zmodem_parse_zrinit(zm);
				break;
			}
			lprintf(zm,LOG_WARNING,"send_file: received header type %s"
				,frame_desc(i));
		}
		if(zm->errors>=zm->max_errors || is_cancelled(zm))
			return(FALSE);
	}

	fstat(fileno(fp),&s);
	zm->current_file_size = s.st_size;
	SAFECOPY(zm->current_file_name, getfname(fname));

	/*
	 * the file exists. now build the ZFILE frame
	 */

	/*
	 * set conversion option
	 * (not used; always binary)
	 */

	zfile_frame[ZF0] = ZF0_ZCBIN;

	/*
	 * management option
	 */

	if(zm->management_protect) {
		zfile_frame[ZF1] = ZF1_ZMPROT;		
		lprintf(zm,LOG_DEBUG,"send_file: protecting destination");
	}
	else if(zm->management_clobber) {
		zfile_frame[ZF1] = ZF1_ZMCLOB;
		lprintf(zm,LOG_DEBUG,"send_file: overwriting destination");
	}
	else if(zm->management_newer) {
		zfile_frame[ZF1] = ZF1_ZMNEW;
		lprintf(zm,LOG_DEBUG,"send_file: overwriting destination if newer");
	}
	else
		zfile_frame[ZF1] = ZF1_ZMCRC;

	/*
	 * transport options
	 * (just plain normal transfer)
	 */

	zfile_frame[ZF2] = ZF2_ZTNOR;

	/*
	 * extended options
	 */

	zfile_frame[ZF3] = 0;

	/*
 	 * now build the data subpacket with the file name and lots of other
	 * useful information.
	 */

	/*
	 * first enter the name and a 0
	 */

	p = zm->tx_data_subpacket;

	strcpy(p,getfname(fname));

	p += strlen(p) + 1;

	sprintf(p,"%lu %lo %lo %d %u %lu %d"
		,zm->current_file_size
		,s.st_mtime
		,0						/* file mode */
		,0						/* serial number */
		,zm->files_remaining
		,zm->bytes_remaining
		,0						/* file type */
		);

	p += strlen(p) + 1;

	do {
		/*
	 	 * send the header and the data
	 	 */

		lprintf(zm,LOG_DEBUG,"Sending ZFILE header block: '%s'"
			,zm->tx_data_subpacket+strlen(zm->tx_data_subpacket)+1);

		zmodem_send_bin_header(zm,zfile_frame);
		zmodem_send_data(zm,ZCRCW,zm->tx_data_subpacket,p - zm->tx_data_subpacket);
	
		/*
		 * wait for anything but an ZACK packet
		 */

		do {
			type = zmodem_recv_header(zm);
			if(is_cancelled(zm))
				return(FALSE);
		} while(type == ZACK && is_connected(zm));

		if(!is_connected(zm))
			return(FALSE);

#if 0
		lprintf(zm,LOG_INFO,"type : %d",type);
#endif

		if(type == ZSKIP) {
			zm->file_skipped=TRUE;
			lprintf(zm,LOG_WARNING,"File skipped by receiver");
			return(FALSE);
		}

		if(type == ZCRC) {
			if(zm->crc_request==0)
				lprintf(zm,LOG_NOTICE,"Receiver requested CRC of entire file");
			else
				lprintf(zm,LOG_NOTICE,"Receiver requested CRC of first %lu bytes"
					,zm->crc_request);
			zmodem_send_pos_header(zm,ZCRC,fcrc32(fp,zm->crc_request),TRUE);
			type = zmodem_recv_header(zm);
		}

	} while(type != ZRPOS);

	zm->transfer_start_time = time(NULL);
	zm->transfer_start_pos = 0;
	if(zm->rxd_header_pos && zm->rxd_header_pos <= zm->current_file_size) {
		pos = zm->transfer_start_pos = zm->rxd_header_pos;
		lprintf(zm,LOG_INFO,"Starting transfer at offset: %lu (resume)", pos);
	}

	if(start!=NULL)		
		*start=zm->transfer_start_time;

	rewind(fp);
	zm->errors = 0;
	zm->consecutive_errors = 0;
	do {
		/*
		 * and start sending
		 */

		type = zmodem_send_from(zm, fp, pos, &sent_bytes);

		if(!is_connected(zm))
			return(FALSE);

		if(type == ZFERR || type == ZABORT || is_cancelled(zm))
			return(FALSE);

		if(sent != NULL)
			*sent += sent_bytes;

		pos += sent_bytes;

		if(type == ZACK)	/* success */
			break;

		lprintf(zm,LOG_ERR,"%s at offset: %lu", chr(type), pos);

		if(zm->block_size == zm->max_block_size && zm->max_block_size > ZBLOCKLEN)
			zm->max_block_size /= 2;

		if(zm->block_size > 128)
			zm->block_size /= 2; 

		zm->errors++;
		if(++zm->consecutive_errors > zm->max_errors)
			return(FALSE);

		/*
		 * fetch pos from the ZRPOS header
		 */

		if(type == ZRPOS) {
			if(zm->rxd_header_pos <= zm->current_file_size) {
				if(pos != zm->rxd_header_pos) {
					pos = zm->rxd_header_pos;
					lprintf(zm,LOG_INFO,"Resuming transfer from offset: %lu", pos);
				}
			} else
				lprintf(zm,LOG_WARNING,"Invalid ZRPOS offset: %lu", zm->rxd_header_pos);
		}

	} while(type == ZRPOS || type == ZNAK || type==TIMEOUT);


	lprintf(zm,LOG_INFO,"Finishing transfer on receipt of header: %s", chr(type));
	if(sent!=NULL)
		lprintf(zm,LOG_DEBUG,"Sent %lu total bytes", *sent);

	if(type==ZACK) {
		/*
		 * file sent. send end of file frame
		 * and wait for zrinit. if it doesnt come then try again
		 */

		for(attempts=0;attempts<=zm->max_errors && !is_cancelled(zm) && is_connected(zm);attempts++) {
			lprintf(zm,LOG_INFO,"Sending End-of-File (ZEOF) frame (%u of %u)"
				,attempts+1, zm->max_errors+1);
			zmodem_send_zeof(zm);
			if(zmodem_recv_header(zm)==ZRINIT) {
				success=TRUE;
				break;
			}
		}
	}
	return(success);
}

int zmodem_recv_files(zmodem_t* zm, const char* download_dir, ulong* bytes_received)
{
	char		fpath[MAX_PATH+1];
	FILE*		fp;
	long		l;
	BOOL		skip;
	ulong		b;
	ulong		crc;
	ulong		rcrc;
	ulong		bytes;
	ulong		kbytes;
	ulong		start_bytes;
	unsigned	files_received=0;
	time_t		t;
	unsigned	cps;
	unsigned	timeout;
	unsigned	errors;

	if(bytes_received!=NULL)
		*bytes_received=0;
	zm->current_file_num=1;
	while(zmodem_recv_init(zm)==ZFILE) {
		bytes=zm->current_file_size;
		kbytes=bytes/1024;
		if(kbytes<1) kbytes=0;
		lprintf(zm,LOG_INFO,"Downloading %s (%lu KBytes) via Zmodem", zm->current_file_name, kbytes);

		do {	/* try */
			skip=TRUE;

			sprintf(fpath,"%s/%s",download_dir,zm->current_file_name);
			lprintf(zm,LOG_DEBUG,"fpath=%s",fpath);
			if(fexist(fpath)) {
				l=flength(fpath);
				lprintf(zm,LOG_WARNING,"%s already exists (%lu bytes)",fpath,l);
				if(l>=(long)bytes) {
					lprintf(zm,LOG_WARNING,"Local file size >= remote file size (%ld)"
						,bytes);
					break;
				}
				if((fp=fopen(fpath,"rb"))==NULL) {
					lprintf(zm,LOG_ERR,"Error %d opening %s", errno, fpath);
					break;
				}
				setvbuf(fp,NULL,_IOFBF,0x10000);

				lprintf(zm,LOG_INFO,"Calculating CRC of: %s", fpath);
				crc=fcrc32(fp,l);
				fclose(fp);
				lprintf(zm,LOG_INFO,"CRC of %s (%lu bytes): %08lX"
					,getfname(fpath), l, crc);
				lprintf(zm,LOG_INFO,"Requesting CRC of remote file: %s", zm->current_file_name);
				if(!zmodem_get_crc(zm,l,&rcrc)) {
					lprintf(zm,LOG_ERR,"Failed to get CRC of remote file");
					break;
				}
				if(crc!=rcrc) {
					lprintf(zm,LOG_WARNING,"Remote file has different CRC value: %08lX", rcrc);
					break;
				}
				lprintf(zm,LOG_INFO,"Resuming download of %s",fpath);
			}

			if((fp=fopen(fpath,"ab"))==NULL) {
				lprintf(zm,LOG_ERR,"Error %d opening/creating/appending %s",errno,fpath);
				break;
			}
			start_bytes=filelength(fileno(fp));

			skip=FALSE;
			errors=zmodem_recv_file_data(zm,fp,flength(fpath));

			for(;errors<=zm->max_errors && !is_cancelled(zm); errors++) {
				if(zmodem_recv_header_and_check(zm))
					break;
			}
			fclose(fp);
			l=flength(fpath);
			if(errors && l==0)	{	/* aborted/failed download */
				if(remove(fpath))	/* don't save 0-byte file */
					lprintf(zm,LOG_ERR,"Error %d removing %s",errno,fpath);
				else
					lprintf(zm,LOG_INFO,"Deleted 0-byte file %s",fpath);
			}
			else {
				if(l!=(long)bytes) {
					lprintf(zm,LOG_WARNING,"Incomplete download (%ld bytes received, expected %lu)"
						,l,bytes);
				} else {
					if((t=time(NULL)-zm->transfer_start_time)<=0)
						t=1;
					b=l-start_bytes;
					if((cps=b/t)==0)
						cps=1;
					lprintf(zm,LOG_INFO,"Received %lu bytes successfully (%u CPS)",b,cps);
					files_received++;
					if(bytes_received!=NULL)
						*bytes_received+=b;
				}
				if(zm->current_file_time)
					setfdate(fpath,zm->current_file_time);
			}

		} while(0);
		/* finally */

		if(skip) {
			lprintf(zm,LOG_WARNING,"Skipping file");
			zmodem_send_zskip(zm);
		}
		zm->current_file_num++;
	}
	if(zm->local_abort)
		zmodem_abort_receive(zm);

	/* wait for "over-and-out" */
	timeout=zm->recv_timeout;
	zm->recv_timeout=2;
	if(zmodem_rx(zm)=='O')
		zmodem_rx(zm);
	zm->recv_timeout=timeout;

	return(files_received);
}

int zmodem_recv_init(zmodem_t* zm)
{
	int			type=CAN;
	unsigned	errors;

	lprintf(zm,LOG_DEBUG,"recv_init");

#if 0
	while(is_connected(zm) && !is_cancelled(zm) && (ch=zm->recv_byte(zm,0))!=NOINP)
		lprintf(zm,LOG_DEBUG,"Throwing out received: %s",chr((uchar)ch));
#endif

	for(errors=0; errors<=zm->max_errors && !is_cancelled(zm) && is_connected(zm); errors++) {
		lprintf(zm,LOG_DEBUG,"Sending ZRINIT (%u of %u)"
			,errors+1, zm->max_errors+1);
		zmodem_send_zrinit(zm);

		type = zmodem_recv_header(zm);

		if(type==TIMEOUT)
			continue;

		lprintf(zm,LOG_DEBUG,"Received header: %s",chr(type));

		if(type==ZFILE) {
			zmodem_parse_zfile_subpacket(zm);
			return(type);
		}

		if(type==ZFIN) {
			zmodem_send_zfin(zm);	/* ACK */
			return(type);
		}

		lprintf(zm,LOG_WARNING,"Received frame: %s, expected ZFILE or ZFIN"
			,frame_desc(type));
		lprintf(zm,LOG_DEBUG,"ZF0=%02X ZF1=%02X ZF2=%02X ZF3=%02X"
			,zm->rxd_header[ZF0],zm->rxd_header[ZF1],zm->rxd_header[ZF2],zm->rxd_header[ZF3]);
	}

	return(type);
}

void zmodem_parse_zfile_subpacket(zmodem_t* zm)
{
	int			i;
	long		mode=0;
	long		serial=-1;

	SAFECOPY(zm->current_file_name,getfname(zm->rx_data_subpacket));

	zm->current_file_size = 0;
	zm->current_file_time = 0;
	zm->files_remaining = 0;
	zm->bytes_remaining = 0;

	i=sscanf(zm->rx_data_subpacket+strlen(zm->rx_data_subpacket)+1,"%lu %lo %lo %lo %lu %lu"
		,&zm->current_file_size	/* file size (decimal) */
		,&zm->current_file_time /* file time (octal unix format) */
		,&mode					/* file mode */
		,&serial				/* program serial number */
		,&zm->files_remaining	/* remaining files to be sent */
		,&zm->bytes_remaining	/* remaining bytes to be sent */
		);

	lprintf(zm,LOG_DEBUG,"Zmodem header (%u fields): %s"
		,i, zm->rx_data_subpacket+strlen(zm->rx_data_subpacket)+1);

	if(!zm->files_remaining)
		zm->files_remaining = 1;
	if(!zm->bytes_remaining)
		zm->bytes_remaining = zm->current_file_size;

	if(!zm->total_files)
		zm->total_files = zm->files_remaining;
	if(!zm->total_bytes)
		zm->total_bytes = zm->bytes_remaining;
}

/*
 * receive file data until the end of the file or until something goes wrong.
 * the name is only used to show progress
 */

unsigned zmodem_recv_file_data(zmodem_t* zm, FILE* fp, ulong offset)
{
	int			i=0;
	unsigned	errors=0;

	zm->transfer_start_pos=offset;
	zm->transfer_start_time=time(NULL);

	fseek(fp,offset,SEEK_SET);
	offset=ftell(fp);

	while(errors<=zm->max_errors && is_connected(zm)
		&& (ulong)ftell(fp) < zm->current_file_size && !is_cancelled(zm)) {

		if(i!=ENDOFFRAME)
			zmodem_send_pos_header(zm, ZRPOS, ftell(fp), /* Hex? */ TRUE);

		if((i = zmodem_recv_file_frame(zm,fp)) == ZEOF)
			break;
		if(i!=ENDOFFRAME) {
			if(i>0)
				lprintf(zm,LOG_ERR,"%s at offset: %lu", chr(i), ftell(fp));
			errors++;
		}
	}
	return(errors);
}


int zmodem_recv_file_frame(zmodem_t* zm, FILE* fp)
{
	unsigned n;
	int type;

	/*
	 * wait for a ZDATA header with the right file offset
	 * or a timeout or a ZFIN
	 */

	do {
		do {
			type = zmodem_recv_header(zm);
			if (type == TIMEOUT) {
				return TIMEOUT;
			}
			if(is_cancelled(zm))
				return(ZCAN);

		} while(type != ZDATA);

		if(zm->rxd_header_pos==(ulong)ftell(fp))
			break;
		lprintf(zm,LOG_WARNING,"Wrong ZDATA block (%lu vs %lu)", zm->rxd_header_pos, ftell(fp));

	} while(!is_cancelled(zm) && is_connected(zm));
		
	do {
		type = zmodem_recv_data(zm,zm->rx_data_subpacket,sizeof(zm->rx_data_subpacket),&n,TRUE);

/*		fprintf(stderr,"packet len %d type %d\n",n,type);
*/
		if (type == ENDOFFRAME || type == FRAMEOK) {
			fwrite(zm->rx_data_subpacket,1,n,fp);
		}

		if(type==FRAMEOK)
			zm->block_size = n;

		if(zm->progress!=NULL)
			zm->progress(zm->cbdata,ftell(fp));

		if(is_cancelled(zm))
			return(ZCAN);

	} while(type == FRAMEOK);

	return type;
}

const char* zmodem_source(void)
{
	return(__FILE__);
}

char* zmodem_ver(char *buf)
{
	sscanf("$Revision$", "%*s %s", buf);

	return(buf);
}

void zmodem_init(zmodem_t* zm, void* cbdata
				,int	(*lputs)(void*, int level, const char* str)
				,void	(*progress)(void* unused, ulong)
				,int	(*send_byte)(void*, uchar ch, unsigned timeout)
				,int	(*recv_byte)(void*, unsigned timeout)
				,BOOL	(*is_connected)(void*)
				,BOOL	(*is_cancelled)(void*)
				,BOOL	(*data_waiting)(void*, unsigned timeout))
{
	memset(zm,0,sizeof(zmodem_t));

	/* Use sane default values */
	zm->init_timeout=10;		/* seconds */
	zm->send_timeout=15;		/* seconds */
	zm->recv_timeout=20;		/* seconds */
	zm->crc_timeout=60;			/* seconds */
#if 0
	zm->byte_timeout=3;			/* seconds */
	zm->ack_timeout=10;			/* seconds */
#endif
	zm->block_size=ZBLOCKLEN;
	zm->max_block_size=ZBLOCKLEN;
	zm->max_errors=9;

	zm->cbdata=cbdata;
	zm->lputs=lputs;
	zm->progress=progress;
	zm->send_byte=send_byte;
	zm->recv_byte=recv_byte;
	zm->is_connected=is_connected;
	zm->is_cancelled=is_cancelled;
	zm->data_waiting=data_waiting;
}
