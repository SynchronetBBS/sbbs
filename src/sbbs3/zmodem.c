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
#include <stdarg.h>	/* va_list */
#include <sys/stat.h>	/* struct stat */

#include "genwrap.h"
#include "dirwrap.h"

#include "zmodem.h"
#include "crc16.h"
#include "crc32.h"

#include "sexyz.h"

#define ENDOFFRAME 2
#define FRAMEOK    1
#define TIMEOUT   -1	/* rx routine did not receive a character within timeout */
#define INVHDR    -2	/* invalid header received; but within timeout */
#define INVDATA   -3	/* invalid data subpacket received */
#define ZDLEESC 0x8000	/* one of ZCRCE; ZCRCG; ZCRCQ or ZCRCW was received; ZDLE escaped */

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

int zmodem_data_waiting(zmodem_t* zm)
{
	if(zm->data_waiting)
		return(zm->data_waiting(zm->cbdata));
	return(FALSE);
}

static char *chr(uchar ch)
{
	static char str[25];

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
		sprintf(str,"'%c' (%02Xh)",ch,ch);
	else
		sprintf(str,"%u (%02Xh)",ch,ch);
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
		lprintf(zm,LOG_ERR,"zmodem_send_raw SEND ERROR: %d",result);

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
			if(zm->escape_all_control_characters && (zm->last_sent&0x7f) == '@')
				return zmodem_send_esc(zm, c);
			break;
		default:
			if(zm->escape_all_control_characters && (c&0x60)==0)
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

	lprintf(zm,LOG_DEBUG,"zmodem_send_hex: %02X ",val);

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

	lprintf(zm,LOG_DEBUG,"zmodem_send_hex_header: %s", chr(type));

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

int zmodem_send_data32(zmodem_t* zm, uchar sub_frame_type, unsigned char * p, int l)
{
	int	result;
	unsigned long crc;

	lprintf(zm,LOG_DEBUG,"zmodem_send_data32");

	crc = 0xffffffffl;

	while(l > 0) {
		crc = ucrc32(*p,crc);
		if((result=zmodem_tx(zm, *p++))!=0)
			return result;
		l--;
	}

	crc = ucrc32(sub_frame_type, crc);

	if((result=zmodem_send_raw(zm, ZDLE))!=0)
		return result;
	if((result=zmodem_send_raw(zm, sub_frame_type))!=0)
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

int zmodem_send_data16(zmodem_t* zm, uchar sub_frame_type,unsigned char * p,int l)
{
	int	result;
	unsigned short crc;

	lprintf(zm,LOG_DEBUG,"zmodem_send_data16");

	crc = 0;

	while(l > 0) {
		crc = ucrc16(*p,crc);
		if((result=zmodem_tx(zm, *p++))!=0)
			return result;
		l--;
	}

	crc = ucrc16(sub_frame_type,crc);

	if((result=zmodem_send_raw(zm, ZDLE))!=0)
		return result;
	if((result=zmodem_send_raw(zm, sub_frame_type))!=0)
		return result;
	
	if((result=	zmodem_tx(zm, (uchar)(crc >> 8)))!=0)
		return result;
	return		zmodem_tx(zm, (uchar)(crc&0xff));
}

/*
 * send a data subpacket using crc 16 or crc 32 as desired by the receiver
 */

int zmodem_send_data(zmodem_t* zm, uchar sub_frame_type, unsigned char * p, int l)
{
	int result;

	if(!zm->want_fcs_16 && zm->can_fcs_32) {
		if((result=zmodem_send_data32(zm, sub_frame_type,p,l))!=0)
			return result;
	}
	else {	
		if((result=zmodem_send_data16(zm, sub_frame_type,p,l))!=0)
			return result;
	}

	if(sub_frame_type == ZCRCW)
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
			lprintf(zm,LOG_WARNING,"zmodem_recv_raw: Cancelled remotely");
			return(TIMEOUT);
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

		/*
	 	 * fake do loop so we may continue
		 * in case a character should be dropped.
		 */

		do {
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
					if(zm->escape_all_control_characters && (c & 0x60) == 0) {
						continue;
					}
					/*
					 * normal character; return it.
					 */
					return c;
			}
		} while(FALSE);
	
		/*
	 	 * ZDLE encoded sequence or session abort.
		 * (or something illegal; then back to the top)
		 */

		do {
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
					lprintf(zm,LOG_DEBUG,"zmodem_rx: encoding data subpacket type: %s"
						,chr((uchar)c));
					return (c | ZDLEESC);
				case ZRUB0:
					return 0x7f;
				case ZRUB1:
					return 0xff;
				default:
					if(zm->escape_all_control_characters && (c & 0x60) == 0) {
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
					break;
			}
		} while(FALSE);
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
	int sub_frame_type;

	lprintf(zm,LOG_DEBUG,"zmodem_recv_data32");

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

	sub_frame_type = c & 0xff;

	crc = ucrc32(sub_frame_type, crc);

	crc = ~crc;

	rxd_crc  = zmodem_rx(zm);
	rxd_crc |= zmodem_rx(zm) << 8;
	rxd_crc |= zmodem_rx(zm) << 16;
	rxd_crc |= zmodem_rx(zm) << 24;

	if(rxd_crc != crc) {
		lprintf(zm,LOG_WARNING,"CRC32 ERROR (%08lX, expected: %08lX) Bytes=%u, sub-frame-type=%s"
			,rxd_crc, crc, *l, chr((char)sub_frame_type));
		return FALSE;
	}
	lprintf(zm,LOG_DEBUG,"GOOD CRC32: %08lX (Bytes=%u, sub-frame-type=%s)"
		,crc, *l, chr((char)sub_frame_type));

	zm->ack_file_pos += *l;

	return sub_frame_type;
}

int zmodem_recv_data16(zmodem_t* zm, register unsigned char* p, unsigned maxlen, unsigned* l)
{
	int c;
	int sub_frame_type;
 	unsigned short crc;
	unsigned short rxd_crc;

	lprintf(zm,LOG_DEBUG,"zmodem_recv_data16");

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

	sub_frame_type = c & 0xff;

	crc = ucrc16(sub_frame_type,crc);

	rxd_crc  = zmodem_rx(zm) << 8;
	rxd_crc |= zmodem_rx(zm);

	if(rxd_crc != crc) {
		lprintf(zm,LOG_WARNING,"CRC16 ERROR (%04hX, expected: %04hX) Bytes=%d"
			,rxd_crc, crc, *l);
		return FALSE;
	}
	lprintf(zm,LOG_DEBUG,"GOOD CRC16: %04hX (Bytes=%d)", crc, *l);

	zm->ack_file_pos += *l;

	return sub_frame_type;
}

int zmodem_recv_data(zmodem_t* zm, unsigned char* p, size_t maxlen, unsigned* l)
{
	int sub_frame_type;
	long pos;
	unsigned n=0;

	if(l==NULL)
		l=&n;

	lprintf(zm,LOG_DEBUG,"zmodem_recv_data (%u-bit)", zm->receive_32bit_data ? 32:16);

	/*
	 * fill in the file pointer in case acknowledgement is requested.	
	 * the ack file pointer will be updated in the subpacket read routine;
	 * so we need to get it now
	 */

	pos = zm->ack_file_pos;

	/*
	 * receive the right type of frame
	 */

	*l = 0;

	if(zm->receive_32bit_data) {
		sub_frame_type = zmodem_recv_data32(zm, p, maxlen, l);
	}
	else {	
		sub_frame_type = zmodem_recv_data16(zm, p, maxlen, l);
	}

	if(sub_frame_type==FALSE)
		return(FALSE);
	
	if(sub_frame_type==TIMEOUT)
		return(TIMEOUT);
	
	lprintf(zm,LOG_DEBUG,"zmodem_recv_data received sub-frame-type: %s"
		,chr((uchar)sub_frame_type));

	switch (sub_frame_type)  {
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
			zmodem_send_ack(zm, pos);
			return FRAMEOK;
		/*
		 * frame ends; ZACK expected
		 */
		case ZCRCW:
			zmodem_send_ack(zm, pos);
			return ENDOFFRAME;
	}

	lprintf(zm,LOG_WARNING,"Invalid sub-frame-type: %s",chr((uchar)sub_frame_type));

	return FALSE;
}

int zmodem_recv_subpacket(zmodem_t* zm)
{
	int type;

	type=zmodem_recv_data(zm,zm->rx_data_subpacket,sizeof(zm->rx_data_subpacket),NULL);
	if(type!=FRAMEOK && type!=ENDOFFRAME)
		zmodem_send_nak(zm);

	return(type);
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

	lprintf(zm,LOG_DEBUG,"zmodem_recv_hex returning: 0x%02X", ret);

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

	lprintf(zm,LOG_DEBUG,"zmodem_recv_bin16_header");

	crc = 0;

	for(n=0;n<HDRLEN;n++) {
		c = zmodem_rx(zm);
		if(c == TIMEOUT) {
			lprintf(zm,LOG_WARNING,"zmodem_recv_bin16_header: timeout");
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

	lprintf(zm,LOG_DEBUG,"zmodem_recv_hex_header");

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

	lprintf(zm,LOG_DEBUG,"zmodem_recv_bin32_header");

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

	lprintf(zm,LOG_DEBUG,"zmodem_recv_header_raw");

	zm->rxd_header_len = 0;

	do {
		do {
			if((c = zmodem_recv_raw(zm)) < 0)
				return(c);
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
			lprintf(zm,LOG_WARNING,"zmodem_recv_header_raw: Expected ZDLE, received: %s"
				,chr((uchar)c));
			continue;
		}

		/*
		 * now read the header style
		 */

		c = zmodem_rx(zm);

		if(c == TIMEOUT) {
			lprintf(zm,LOG_WARNING,"zmodem_recv_header_raw: TIMEOUT");
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
				lprintf(zm,LOG_ERR,"zmodem_recv_header_raw: UNRECOGNIZED header style: %s"
					,chr((uchar)c));
				if(errors) {
					return INVHDR;
				}

				continue;
		}
		if(errors && zm->rxd_header_len == 0) {
			return INVHDR;
		}

	} while(zm->rxd_header_len == 0);

	/*
 	 * this appears to have been a valid header.
	 * return its type.
	 */

	if(zm->rxd_header[0] == ZDATA) {
		zm->ack_file_pos = zm->rxd_header[ZP0] | (zm->rxd_header[ZP1] << 8) |
			(zm->rxd_header[ZP2] << 16) | (zm->rxd_header[ZP3] << 24);
	}

	if(zm->rxd_header[0] == ZFILE) {
		zm->ack_file_pos = 0l;
	}

#if 0 //def _DEBUG
	lprintf(zm,LOG_DEBUG,"zmodem_recv_header_raw received header type: %s",chr(zm->rxd_header[0]));
#endif

	return zm->rxd_header[0];
}

int zmodem_recv_header(zmodem_t* zm)
{
	int ret = zmodem_recv_header_raw(zm, FALSE);

	if(ret == TIMEOUT)
		lprintf(zm,LOG_WARNING,"zmodem_recv_header TIMEOUT");
	else
		lprintf(zm,LOG_DEBUG,"zmodem_recv_header returning: %s", chr((uchar)ret));

	if(ret==ZCAN)
		zm->cancelled=TRUE;

	return ret;
}

int zmodem_recv_header_and_check(zmodem_t* zm)
{
	int type;
	while(is_connected(zm)) {
		type = zmodem_recv_header_raw(zm,TRUE);		

		if(type != INVHDR) {
			break;
		}

		zmodem_send_znak(zm);
	}

	return type;
}

void zmodem_parse_zrinit(zmodem_t* zm)
{
	zm->can_full_duplex					= (zm->rxd_header[ZF0] & ZF0_CANFDX)  != 0;
	zm->can_overlap_io					= (zm->rxd_header[ZF0] & ZF0_CANOVIO) != 0;
	zm->can_break						= (zm->rxd_header[ZF0] & ZF0_CANBRK)  != 0;
	zm->can_fcs_32						= (zm->rxd_header[ZF0] & ZF0_CANFC32) != 0;
	zm->escape_all_control_characters	= (zm->rxd_header[ZF0] & ZF0_ESCCTL)  != 0;
	zm->escape_8th_bit					= (zm->rxd_header[ZF0] & ZF0_ESC8)    != 0;

	lprintf(zm,LOG_INFO,"Receiver requested mode (0x%02X):\r\n"
		"%s-duplex, %s overlap I/O, CRC-%u, Escape: %s"
		,zm->rxd_header[ZF0]
		,zm->can_full_duplex ? "Full" : "Half"
		,zm->can_overlap_io ? "Can" : "Cannot"
		,zm->can_fcs_32 ? 32 : 16
		,zm->escape_all_control_characters ? "ALL" : "Normal"
		);

	if((zm->recv_bufsize = (zm->rxd_header[ZP0]<<8 | zm->rxd_header[ZP1])) != 0)
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
	
	return zmodem_recv_header(zm);
}

int zmodem_send_zrinit(zmodem_t* zm)
{
	unsigned char zrinit_header[] = { ZRINIT, 0, 0, 0, 0 };
	
	zrinit_header[ZF0] = ZF0_CANBRK | ZF0_CANFDX | ZF0_CANOVIO | ZF0_CANFC32;

	if(zm->no_streaming) {
		zrinit_header[ZP0] = sizeof(zm->rx_data_subpacket) >> 8;
		zrinit_header[ZP1] = sizeof(zm->rx_data_subpacket) & 0xff;
	}

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

int zmodem_send_from(zmodem_t* zm, FILE* fp, ulong pos, ulong fsize, ulong* sent)
{
	int n;
	uchar type;
	unsigned buf_sent=0;

	if(sent!=NULL)
		*sent=0;

	fseek(fp,pos,SEEK_SET);

	zmodem_send_pos_header(zm, ZDATA, pos, /* Hex? */ FALSE);

	/*
	 * send the data in the file
	 */

	while(!feof(fp) && is_connected(zm)) {

		/*
		 * read a block from the file
		 */
		n = fread(zm->tx_data_subpacket,1,sizeof(zm->tx_data_subpacket),fp);

		if(n == 0) {
			/*
			 * nothing to send ?
			 */
			break;
		}
	
		if(zm->progress!=NULL)
			zm->progress(zm->cbdata, pos, ftell(fp), fsize, zm->transfer_start);

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

		/*
		 * at end of file wait for an ACK
		 */
		if(feof(fp))
			type = ZCRCW;

		zmodem_send_data(zm, type, zm->tx_data_subpacket, n);

		if(sent!=NULL)
			*sent+=n;
		
		buf_sent+=n;

		if(type == ZCRCW) {
			int type;
			do {
				type = zmodem_recv_header(zm);
				if(type == ZNAK || type == ZRPOS || type == TIMEOUT) {
					return type;
				}
			} while(type != ZACK);

			if((ulong)ftell(fp) >= fsize) {
				lprintf(zm,LOG_DEBUG,"end of file (%ld)", fsize );
				return ZACK;
			}
		}

		/* 
		 * characters from the other side
		 * check out that header
		 */

		while(zmodem_data_waiting(zm) && !zm->cancelled) {
			int type;
			int c;
			if((c = zmodem_recv_raw(zm)) < 0)
				return(c);
			if(c == ZPAD) {
				type = zmodem_recv_header(zm);
				if(type != TIMEOUT && type != ACK) {
					return type;
				}
			}
		}
		if(zm->cancelled)
			return(-1);
	}

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
	long	pos=0;
	ulong	sent_bytes;
	struct	stat s;
	unsigned char * p;
	uchar	zfile_frame[] = { ZFILE, 0, 0, 0, 0 };
	uchar	zeof_frame[] = { ZEOF, 0, 0, 0, 0 };
	int		type;
	int		i;
	unsigned errors;

	if(sent!=NULL)	
		*sent=0;

	if(start!=NULL)		
		*start=time(NULL);

	zm->file_skipped=FALSE;

	if(request_init) {
		for(errors=0; errors<=zm->max_errors && !zm->cancelled && is_connected(zm); errors++) {
			lprintf(zm,LOG_INFO,"Sending ZRQINIT (%u of %u)"
				,errors+1,zm->max_errors+1);
			i = zmodem_get_zrinit(zm);
			if(i == ZRINIT) {
				zmodem_parse_zrinit(zm);
				break;
			}
			lprintf(zm,LOG_WARNING,"zmodem_send_file: received header type: %d 0x%02X"
				,i, i);
		}
		if(errors>=zm->max_errors || zm->cancelled)
			return(FALSE);
	}

	fstat(fileno(fp),&s);

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
		lprintf(zm,LOG_DEBUG,"zmodem_send_file: protecting destination");
	}

	if(zm->management_clobber) {
		zfile_frame[ZF1] = ZF1_ZMCLOB;
		lprintf(zm,LOG_DEBUG,"zmodem_send_file: overwriting destination");
	}

	if(zm->management_newer) {
		zfile_frame[ZF1] = ZF1_ZMNEW;
		lprintf(zm,LOG_DEBUG,"zmodem_send_file: overwriting destination if newer");
	}

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
		,s.st_size
		,s.st_mtime
		,0						/* file mode */
		,0						/* serial number */
		,zm->n_files_remaining
		,zm->n_bytes_remaining
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
			if(zm->cancelled)
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

	} while(type != ZRPOS);

	zm->transfer_start = time(NULL);

	if(start!=NULL)		
		*start=zm->transfer_start;

	do {
		/*
		 * fetch pos from the ZRPOS header
		 */

		if(type == ZRPOS) {
			pos = zm->rxd_header[ZP0] | (zm->rxd_header[ZP1] << 8) | (zm->rxd_header[ZP2] << 16) | (zm->rxd_header[ZP3] << 24);
			if(pos)
				lprintf(zm,LOG_INFO,"Resuming transfer from offset: %lu", pos);
		}

		/*
		 * and start sending
		 */

		type = zmodem_send_from(zm, fp, pos, s.st_size, &sent_bytes);

		if(!is_connected(zm))
			return(FALSE);

		if(sent!=NULL)
			*sent+=sent_bytes;

		if(type == ZFERR || type == ZABORT || zm->cancelled)
			return(FALSE);

	} while(type == ZRPOS || type == ZNAK);

	lprintf(zm,LOG_INFO,"Finishing transfer on rx of header type: %s", chr((uchar)type));

	if(type==ZACK) {
		/*
		 * file sent. send end of file frame
		 * and wait for zrinit. if it doesnt come then try again
		 */

		zeof_frame[ZP0] = (uchar) (s.st_size        & 0xff);
		zeof_frame[ZP1] = (uchar)((s.st_size >> 8)  & 0xff);
		zeof_frame[ZP2] = (uchar)((s.st_size >> 16) & 0xff);
		zeof_frame[ZP3] = (uchar)((s.st_size >> 24) & 0xff);

		for(errors=0;errors<=zm->max_errors && !zm->cancelled && is_connected(zm);errors++) {
			lprintf(zm,LOG_INFO,"Sending End-of-File (ZEOF) frame (%u of %u)"
				,errors+1, zm->max_errors+1);
			zmodem_send_hex_header(zm,zeof_frame);
			if(zmodem_recv_header(zm)==ZRINIT) {
				success=TRUE;
				break;
			}
		}
	}
	return(success);
}

int zmodem_recv_init(zmodem_t* zm
						   ,char* fname, size_t maxlen
						   ,ulong* p_size
						   ,time_t* p_time
						   ,long* p_mode
						   ,long* p_serial
						   ,ulong* p_total_files
						   ,ulong* p_total_bytes)
{
	int			ch;
	int			type=CAN;
	unsigned	errors;

	lprintf(zm,LOG_DEBUG,"zmodem_recv_init");

	while(!zm->cancelled && (ch=zm->recv_byte(zm,0))!=NOINP)
		lprintf(zm,LOG_DEBUG,"Throwing out received: %s",chr((uchar)ch));

	for(errors=0; errors<=zm->max_errors && !zm->cancelled && is_connected(zm); errors++) {
		lprintf(zm,LOG_DEBUG,"Sending ZRINIT (%u of %u)"
			,errors+1, zm->max_errors+1);
		zmodem_send_zrinit(zm);

		type = zmodem_recv_header(zm);

		if(type==TIMEOUT)
			continue;

		lprintf(zm,LOG_DEBUG,"Received header: %s",chr((uchar)type));

		if(type==ZFILE) {
			if(!zmodem_recv_file_info(zm
				,fname,maxlen
				,p_size
				,p_time
				,p_mode
				,p_serial
				,p_total_files
				,p_total_bytes))
				continue;
			return(type);
		}

		if(type==ZFIN) {
			zmodem_send_zfin(zm);	/* ACK */
			return(type);
		}

		lprintf(zm,LOG_WARNING,"Received header type: %s, expected ZFILE or ZFIN"
			,chr((uchar)type));
		lprintf(zm,LOG_DEBUG,"ZF0=%02X ZF1=%02X ZF2=%02X ZF3=%02X"
			,zm->rxd_header[ZF0],zm->rxd_header[ZF1],zm->rxd_header[ZF2],zm->rxd_header[ZF3]);

		switch(type) {
			case ZFREECNT:
				zmodem_send_pos_header(zm, ZACK, getfreediskspace(".",1), /* Hex? */ TRUE);
				break;
			case ZSINIT:
			case ZCOMMAND:
				/* unsupported headers, receive and ignore data subpacket to follow */
				zmodem_recv_subpacket(zm);
				break;
		}
	}

	return(type);
}

BOOL zmodem_recv_file_info(zmodem_t* zm
						   ,char* fname, size_t maxlen
						   ,ulong* p_size
						   ,time_t* p_time
						   ,long* p_mode
						   ,long* p_serial
						   ,ulong* p_total_files
						   ,ulong* p_total_bytes)
{
	uchar		block[8192];
	int			i;
	ulong		size=0;
	ulong		time=0;
	ulong		total_files=0;
	ulong		total_bytes=0;
	long		mode=0;
	long		serial=-1;
	unsigned	l;

	memset(block,0,sizeof(block));
	i=zmodem_recv_data(zm, block, sizeof(block), &l);

	if(i!=FRAMEOK && i!=ENDOFFRAME) {
		zmodem_send_nak(zm);
		return(FALSE);
	}

	if(fname!=NULL)
		safe_snprintf(fname,maxlen,"%s",block);

	i=sscanf(block+strlen(block)+1,"%lu %lo %lo %lo %lu %lu"
		,&size					/* file size (decimal) */
		,&time 					/* file time (octal unix format) */
		,&mode 					/* file mode */
		,&serial				/* program serial number */
		,&total_files			/* remaining files to be sent */
		,&total_bytes			/* remaining bytes to be sent */
		);

	lprintf(zm,LOG_DEBUG,"Zmodem header (%u fields): %s"
		,i, block+strlen(block)+1);

	if(!total_files)
		total_files=1;
	if(!total_bytes)
		total_bytes=size;

	if(p_size)			*p_size=size;
	if(p_time)			*p_time=time;
	if(p_mode)			*p_mode=mode;
	if(p_serial)		*p_serial=serial;
	if(p_total_files)	*p_total_files=total_files;
	if(p_total_bytes)	*p_total_bytes=total_bytes;

	return(TRUE);
}

/*
 * receive file data until the end of the file or until something goes wrong.
 * the name is only used to show progress
 */

unsigned zmodem_recv_file_data(zmodem_t* zm, FILE* fp, ulong offset, ulong fsize, time_t start)
{
	int			i=0;
	unsigned	errors=0;

	if(start==0)
		start=time(NULL);

	fseek(fp,offset,SEEK_SET);
	offset=ftell(fp);

	while(errors<=zm->max_errors && is_connected(zm)
		&& (ulong)ftell(fp) < fsize && !zm->cancelled) {

		if(i!=ENDOFFRAME)
			zmodem_send_pos_header(zm, ZRPOS, ftell(fp), /* Hex? */ TRUE);

		if((i = zmodem_recv_file_frame(zm,fp,offset,fsize,start)) == ZEOF)
			break;
		if(i!=ENDOFFRAME) {
			if(i>0)
				lprintf(zm,LOG_WARNING,"Error at byte %lu: %s", ftell(fp), chr((uchar)i));
			errors++;
		}
	}
	return(errors);
}


int zmodem_recv_file_frame(zmodem_t* zm, FILE* fp, ulong offset, ulong fsize, time_t start)
{
	long pos;
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
			if(zm->cancelled)
				return(ZCAN);

			if(type == ZFILE)	/* rrs: what? Ignore following data block! */
				zmodem_recv_subpacket(zm);

		} while(type != ZDATA);

		pos = zm->rxd_header[ZP0] | (zm->rxd_header[ZP1] << 8) |
			(zm->rxd_header[ZP2] << 16) | (zm->rxd_header[ZP3] << 24);
		if(pos==ftell(fp))
			break;
		lprintf(zm,LOG_WARNING,"Wrong ZDATA block (%lu vs %lu)", pos, ftell(fp));

	} while(!zm->cancelled && is_connected(zm));
		
	do {
		type = zmodem_recv_data(zm,zm->rx_data_subpacket,sizeof(zm->rx_data_subpacket),&n);

/*		fprintf(stderr,"packet len %d type %d\n",n,type);
*/
		if (type == ENDOFFRAME || type == FRAMEOK) {
			fwrite(zm->rx_data_subpacket,1,n,fp);
		}

		if(zm->progress!=NULL)
			zm->progress(zm->cbdata,offset,ftell(fp),fsize,start);

		if(zm->cancelled)
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
				,void	(*progress)(void* unused, ulong, ulong, ulong, time_t)
				,int	(*send_byte)(void*, uchar ch, unsigned timeout)
				,int	(*recv_byte)(void*, unsigned timeout)
				,BOOL	(*is_connected)(void*)
				,BOOL	(*data_waiting)(void*))
{
	memset(zm,0,sizeof(zmodem_t));

	/* Use sane default values */
	zm->send_timeout=10;		/* seconds */
	zm->recv_timeout=10;		/* seconds */
#if 0
	zm->byte_timeout=3;			/* seconds */
	zm->ack_timeout=10;			/* seconds */
	zm->block_size=1024;
#endif
	zm->max_errors=9;

	zm->cbdata=cbdata;
	zm->lputs=lputs;
	zm->progress=progress;
	zm->send_byte=send_byte;
	zm->recv_byte=recv_byte;
	zm->is_connected=is_connected;
	zm->data_waiting=data_waiting;
}
