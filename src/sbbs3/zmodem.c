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
#include "sockwrap.h"

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

/* temporary */
extern	SOCKET sock;

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
		case ZDLE:		return("ZDLE");
		case ZDLEE:		return("ZDLEE");
		case ZBIN:		return("ZBIN");
		case ZHEX:		return("ZHEX");
		case ZBIN32:	return("ZBIN32");
		case ZBINR32:	return("ZBINR32");
		case ZVBIN:		return("ZVBIN");
		case ZVHEX:		return("ZVHEX");
		case ZVBIN32:	return("ZVBIN32");
		case ZVBINR32:	return("ZVBINR32");
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

void
zmodem_rx_purge(zmodem_t* zm)
{
	while(zm->recv_byte(zm->cbdata,0)>=0);
}

/* 
 * transmit a character. 
 * this is the raw modem interface
 */

void
zmodem_tx_raw(zmodem_t* zm, unsigned char ch)
{
	if(zm->raw_trace)
		lprintf(zm,LOG_INFO,"RX: %s ",chr(ch));

	if(zm->send_byte(zm->cbdata,ch,zm->send_timeout))
		lprintf(zm,LOG_ERR,"Send error: %u",ERROR_VALUE);

	zm->last_sent = ch;
}

/*
 * transmit a character ZDLE escaped
 */

void
zmodem_tx_esc(zmodem_t* zm, unsigned char c)
{
	zmodem_tx_raw(zm, ZDLE);
	/*
	 * exclusive or; not an or so ZDLE becomes ZDLEE
	 */
	zmodem_tx_raw(zm, (uchar)(c ^ 0x40));
}

/*
 * transmit a character; ZDLE escaping if appropriate
 */

void
zmodem_tx(zmodem_t* zm, unsigned char c)
{
	switch (c) {
		case DLE:
		case DLE|0x80:          /* even if high-bit set */
		case XON:
		case XON|0x80:
		case XOFF:
		case XOFF|0x80:
		case ZDLE:
			zmodem_tx_esc(zm, c);
			return;
		case CR:
		case CR|0x80:
			if(zm->escape_all_control_characters && (zm->last_sent&0x7f) == '@') {
				zmodem_tx_esc(zm, c);
				return;
			}
			break;
		default:
			if(zm->escape_all_control_characters && (c&0x60)==0) {
				zmodem_tx_esc(zm, c);
				return;
			}
			break;
	}
	/*
	 * anything that ends here is so normal we might as well transmit it.
	 */
	zmodem_tx_raw(zm, c);
}

/**********************************************/
/* Output single byte as two hex ASCII digits */
/**********************************************/
void
zmodem_tx_hex(zmodem_t* zm, uchar val)
{
	char* xdigit="0123456789abcdef";

//	lprintf(zm,LOG_INFO,"TX: %02X ",val);

	zmodem_tx_raw(zm, xdigit[val>>4]);
	zmodem_tx_raw(zm, xdigit[val&0xf]);
}

/* 
 * transmit a hex header.
 * these routines use tx_raw because we're sure that all the
 * characters are not to be escaped.
 */
void
zmodem_tx_hex_header(zmodem_t* zm, unsigned char * p)
{
	int i;
	uchar type=*p;
	unsigned short int crc;

#if 0 /* def _DEBUG */
	lprintf(zm,LOG_INFO,"tx_hheader : ");
	for (i=0;i<HDRLEN;i++)
		lprintf(zm,LOG_INFO,"%02X ",*(p+i));
	lprintf(zm,LOG_INFO,"");
#endif

	lprintf(zm,LOG_DEBUG,"tx_hex_header: %s", chr(type));

	zmodem_tx_raw(zm, ZPAD);
	zmodem_tx_raw(zm, ZPAD);
	zmodem_tx_raw(zm, ZDLE);

	zmodem_tx_raw(zm, ZHEX);

	/*
 	 * initialise the crc
	 */

	crc = 0;

	/*
 	 * transmit the header
	 */

	for (i=0;i<HDRLEN;i++) {
		zmodem_tx_hex(zm, *p);
		crc = ucrc16(*p, crc);
		p++;
	}

	/*
 	 * update the crc as though it were zero
	 */

//	crc = ucrc16(0,crc);
//	crc = ucrc16(0,crc);

	/* 
	 * transmit the crc
	 */

	zmodem_tx_hex(zm, (uchar)(crc>>8));
	zmodem_tx_hex(zm, (uchar)(crc&0xff));

	/*
	 * end of line sequence
	 */

	zmodem_tx_raw(zm, '\r');
	zmodem_tx_raw(zm, '\n');

	if(type!=ZACK && type!=ZFIN)
		zmodem_tx_raw(zm, XON);

#if 0 /* def _DEBUG */
	lprintf(zm,LOG_INFO,"");
#endif
}

/*
 * Send ZMODEM binary header hdr
 */

void
zmodem_tx_bin32_header(zmodem_t* zm, unsigned char * p)
{
	int i;
	unsigned long crc;

#if 0 /* def _DEBUG */
	lprintf(zm,LOG_INFO,"tx binary header 32 bits crc");
//	zm->raw_trace = 1;
#endif

	zmodem_tx_raw(zm, ZPAD);
	zmodem_tx_raw(zm, ZPAD);
	zmodem_tx_raw(zm, ZDLE);

	zmodem_tx_raw(zm, ZBIN32);

	crc = 0xffffffffL;

	for (i=0;i<HDRLEN;i++) {
		crc = ucrc32(*p,crc);
		zmodem_tx(zm, *p++);
	}

	crc = ~crc;

	zmodem_tx(zm, (uchar)((crc      ) & 0xff));
	zmodem_tx(zm, (uchar)((crc >>  8) & 0xff));
	zmodem_tx(zm, (uchar)((crc >> 16) & 0xff));
	zmodem_tx(zm, (uchar)((crc >> 24) & 0xff));
}

void
zmodem_tx_bin16_header(zmodem_t* zm, unsigned char * p)
{
	int i;
	unsigned int crc;

#if 0 /* def _DEBUG */
	lprintf(zm,LOG_INFO,"tx binary header 16 bits crc");
#endif

	zmodem_tx_raw(zm, ZPAD);
	zmodem_tx_raw(zm, ZPAD);
	zmodem_tx_raw(zm, ZDLE);

	zmodem_tx_raw(zm, ZBIN);

	crc = 0;

	for (i=0;i<HDRLEN;i++) {
		crc = ucrc16(*p,crc);
		zmodem_tx(zm, *p++);
	}

//	crc = ucrc16(0,crc);
//	crc = ucrc16(0,crc);

	zmodem_tx(zm, (uchar)(crc >> 8));
	zmodem_tx(zm, (uchar)(crc&0xff));
}


/* 
 * transmit a header using either hex 16 bit crc or binary 32 bit crc
 * depending on the receivers capabilities
 * we dont bother with variable length headers. I dont really see their
 * advantage and they would clutter the code unneccesarily
 */

void
zmodem_tx_header(zmodem_t* zm, unsigned char * p)
{
	if(zm->can_fcs_32) {
		if(!zm->want_fcs_16) {
			zmodem_tx_bin32_header(zm, p);
		}
		else {
			zmodem_tx_bin16_header(zm, p);
		}
	}
	else {
		zmodem_tx_hex_header(zm, p);
	}
}

/*
 * data subpacket transmission
 */

void
zmodem_tx_32_data(zmodem_t* zm, uchar sub_frame_type, unsigned char * p, int l)
{
	unsigned long crc;

#if 0 /* def _DEBUG */
	lprintf(zm,LOG_INFO,"tx_32_data");
#endif

	crc = 0xffffffffl;

	while (l > 0) {
		crc = ucrc32(*p,crc);
		zmodem_tx(zm, *p++);
		l--;
	}

	crc = ucrc32(sub_frame_type, crc);

	zmodem_tx_raw(zm, ZDLE);
	zmodem_tx_raw(zm, sub_frame_type);

	crc = ~crc;

	zmodem_tx(zm, (uchar) ((crc      ) & 0xff));
	zmodem_tx(zm, (uchar) ((crc >> 8 ) & 0xff));
	zmodem_tx(zm, (uchar) ((crc >> 16) & 0xff));
	zmodem_tx(zm, (uchar) ((crc >> 24) & 0xff));
}

void
zmodem_tx_16_data(zmodem_t* zm, uchar sub_frame_type,unsigned char * p,int l)
{
	unsigned short crc;

#if 0 /* def _DEBUG */
	lprintf(zm,LOG_INFO,"tx_16_data");
#endif

	crc = 0;

	while (l > 0) {
		crc = ucrc16(*p,crc);
		zmodem_tx(zm, *p++);
		l--;
	}

	crc = ucrc16(sub_frame_type,crc);

	zmodem_tx_raw(zm, ZDLE); 
	zmodem_tx_raw(zm, sub_frame_type);
	
//	crc = ucrc16(0,crc);
//	crc = ucrc16(0,crc);

	zmodem_tx(zm, (uchar)(crc >> 8));
	zmodem_tx(zm, (uchar)(crc&0xff));
}

/*
 * send a data subpacket using crc 16 or crc 32 as desired by the receiver
 */

void
zmodem_tx_data(zmodem_t* zm, uchar sub_frame_type,unsigned char * p, int l)
{
	if(!zm->want_fcs_16 && zm->can_fcs_32) {
		zmodem_tx_32_data(zm, sub_frame_type,p,l);
	}
	else {	
		zmodem_tx_16_data(zm, sub_frame_type,p,l);
	}

	if(sub_frame_type == ZCRCW) {
		zmodem_tx_raw(zm, XON);
	}
//	YIELD();
}

void zmodem_send_pos_header(zmodem_t* zm, int type, long pos, BOOL hex) 
{
	uchar header[5];

	header[0]   = type;
	header[ZP0] =  pos        & 0xff;
	header[ZP1] = (pos >>  8) & 0xff;
	header[ZP2] = (pos >> 16) & 0xff;
	header[ZP3] = (pos >> 24) & 0xff;

	if(hex)
		zmodem_tx_hex_header(zm, header);
	else
		zmodem_tx_header(zm, header);
}

void
zmodem_tx_znak(zmodem_t* zm)
{
//	lprintf(zm,LOG_INFO,"tx_znak");

	zmodem_send_pos_header(zm, ZNAK, zm->ack_file_pos, /* Hex? */ TRUE);
}

void zmodem_send_zskip(zmodem_t* zm)
{
	zmodem_send_pos_header(zm, ZSKIP, 0L, /* Hex? */ TRUE);
}

/*
 * receive any style header within timeout milliseconds
 */

int
zmodem_rx_poll(zmodem_t* zm)
{
	int rd=0;

	socket_check(sock,&rd,NULL,0);

	return(rd);
}

/*
 * rx_raw ; receive a single byte from the line.
 * reads as many are available and then processes them one at a time
 * check the data stream for 5 consecutive CAN characters;
 * and if you see them abort. this saves a lot of clutter in
 * the rest of the code; even though it is a very strange place
 * for an exit. (but that was wat session abort was all about.)
 */

int
zmodem_rx_raw(zmodem_t* zm, int to)
{
	int c;

	if((c=zm->recv_byte(zm->cbdata,to)) < 0)
		return(TIMEOUT);

//	lprintf(zm,LOG_INFO,"%02X  ",c);

	if(c == CAN) {
		zm->n_cans++;
		if(zm->n_cans == 5) {
			zm->cancelled=TRUE;
			lprintf(zm,LOG_WARNING,"Cancelled remotely");
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

int
zmodem_rx(zmodem_t* zm, int to)
{
	int c;

	/*
	 * outer loop for ever so for sure something valid
	 * will come in; a timeout will occur or a session abort
	 * will be received.
	 */

	while (TRUE) {

		/*
	 	 * fake do loop so we may continue
		 * in case a character should be dropped.
		 */

		do {
			if((c = zmodem_rx_raw(zm, to)) < 0)
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
		} while (FALSE);
	
		/*
	 	 * ZDLE encoded sequence or session abort.
		 * (or something illegal; then back to the top)
		 */

		do {
			if((c = zmodem_rx_raw(zm, to)) < 0)
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
					lprintf(zm,LOG_DEBUG,"escaping data subpacket type: %s", chr((uchar)c));
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
		} while (FALSE);
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

int
zmodem_rx_32_data(zmodem_t* zm, unsigned char * p, unsigned maxlen, unsigned* l)
{
	int c;
	unsigned long rxd_crc;
	unsigned long crc;
	int sub_frame_type;

#if 0 /* def _DEBUG */
	lprintf(zm,LOG_INFO,"rx_32_data");
#endif

	crc = 0xffffffffl;

	do {
		c = zmodem_rx(zm, 1);

		if(c == TIMEOUT) {
			return TIMEOUT;
		}
		if(c < 0x100 && *l < maxlen) {
			crc = ucrc32(c,crc);
			*p++ = c;
			(*l)++;
			continue;
		}
	} while (c < 0x100);

	sub_frame_type = c & 0xff;

	crc = ucrc32(sub_frame_type, crc);

	crc = ~crc;

	rxd_crc  = zmodem_rx(zm, 1);
	rxd_crc |= zmodem_rx(zm, 1) << 8;
	rxd_crc |= zmodem_rx(zm, 1) << 16;
	rxd_crc |= zmodem_rx(zm, 1) << 24;

	if(rxd_crc != crc) {
		lprintf(zm,LOG_WARNING,"CRC-32 ERROR");
		return FALSE;
	}

	zm->ack_file_pos += *l;

	return sub_frame_type;
}

int
zmodem_rx_16_data(zmodem_t* zm, register unsigned char* p, unsigned maxlen, unsigned* l)
{
	int c;
	int sub_frame_type;
 	unsigned short crc;
	unsigned short rxd_crc;

#if 0 /* def _DEBUG */
	lprintf(zm,LOG_INFO,"rx_16_data");
#endif

	crc = 0;

	do {
		c = zmodem_rx(zm, zm->recv_timeout);

		if(c == TIMEOUT) {
			return TIMEOUT;
		}
		if(c < 0x100 && *l < maxlen) {
			crc = ucrc16(c,crc);
			*p++ = c;
			(*l)++;
		}
	} while (c < 0x100);

	sub_frame_type = c & 0xff;

	crc = ucrc16(sub_frame_type,crc);

//	crc = ucrc16(0,crc);
//	crc = ucrc16(0,crc);

	rxd_crc  = zmodem_rx(zm, zm->recv_timeout) << 8;
	rxd_crc |= zmodem_rx(zm, zm->recv_timeout);

	if(rxd_crc != crc) {
		lprintf(zm,LOG_WARNING,"CRC-16 ERROR (%04hX vs %04hX) Bytes=%d"
			,rxd_crc, crc, *l);
		return FALSE;
	}
	lprintf(zm,LOG_DEBUG,"GOOD CRC-16: %04hX (Bytes=%d)", crc, *l);

	zm->ack_file_pos += *l;

	return sub_frame_type;
}

int
zmodem_rx_data(zmodem_t* zm, unsigned char* p, size_t maxlen, unsigned* l)
{
	unsigned char zack_header[] = { ZACK, 0, 0, 0, 0 };
	int sub_frame_type;
	long pos;

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

	if(zm->receive_32_bit_data) {
		sub_frame_type = zmodem_rx_32_data(zm, p, maxlen, l);
	}
	else {	
		sub_frame_type = zmodem_rx_16_data(zm, p, maxlen, l);
	}

	switch (sub_frame_type)  {
		case TIMEOUT:
			return TIMEOUT;
		/*
		 * frame continues non-stop
		 */
		case ZCRCG:
			lprintf(zm,LOG_DEBUG,"ZCRCG");
			return FRAMEOK;
		/*
		 * frame ends
		 */
		case ZCRCE:
			lprintf(zm,LOG_DEBUG,"ZCRCE");
			return ENDOFFRAME;
		/*
 		 * frame continues; ZACK expected
		 */
		case ZCRCQ:		
			lprintf(zm,LOG_DEBUG,"ZCRCQ");
			zmodem_send_pos_header(zm, ZACK, pos, /* Hex? */ TRUE);
			return FRAMEOK;
		/*
		 * frame ends; ZACK expected
		 */
		case ZCRCW:
			lprintf(zm,LOG_DEBUG,"ZCRCW");
			zmodem_send_pos_header(zm, ZACK, pos, /* Hex? */ TRUE);
			return ENDOFFRAME;
	}

	lprintf(zm,LOG_WARNING,"Invalid sub-frame type: %s",chr((uchar)sub_frame_type));

	return FALSE;
}

int
zmodem_rx_nibble(zmodem_t* zm, int to) 
{
	int c;

	c = zmodem_rx(zm, to);

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

int
zmodem_rx_hex(zmodem_t* zm, int to)
{
	int n1;
	int n0;
	int ret;

	n1 = zmodem_rx_nibble(zm, to);

	if(n1 == TIMEOUT) {
		return n1;
	}

	n0 = zmodem_rx_nibble(zm, to);

	if(n0 == TIMEOUT) {
		return n0;
	}

	ret = (n1 << 4) | n0;

	if(*(zm->mode)&DEBUG)
		lprintf(zm,LOG_INFO,"zmodem_rx_hex returning 0x%02X", ret);

	return ret;
}

/*
 * receive routines for each of the six different styles of header.
 * each of these leaves zm->rxd_header_len set to 0 if the end result is
 * not a valid header.
 */

void
zmodem_rx_bin16_header(zmodem_t* zm, int to)
{
	int c;
	int n;
	unsigned short int crc;
	unsigned short int rxd_crc;

#if 0 /* def _DEBUG */
	lprintf(zm,LOG_INFO,"rx binary header 16 bits crc");
#endif

	crc = 0;

	for (n=0;n<5;n++) {
		c = zmodem_rx(zm, to);
		if(c == TIMEOUT) {
			lprintf(zm,LOG_ERR,"timeout");
			return;
		}
		crc = ucrc16(c,crc);
		zm->rxd_header[n] = c;
	}

//	crc = ucrc16(0,crc);
//	crc = ucrc16(0,crc);

	rxd_crc  = zmodem_rx(zm, 1) << 8;
	rxd_crc |= zmodem_rx(zm, 1);

	if(rxd_crc != crc) {
		lprintf(zm,LOG_ERR,"bad crc %4.4x %4.4x",rxd_crc,crc);
		return;
	}

	zm->rxd_header_len = 5;
}

void
zmodem_rx_hex_header(zmodem_t* zm, int to)
{
	int c;
	int i;
	unsigned short int crc = 0;
	unsigned short int rxd_crc;

#if 0 /* def _DEBUG */
	lprintf(zm,LOG_INFO,"rx_hex_header : ");
#endif
	for (i=0;i<5;i++) {
		c = zmodem_rx_hex(zm, to);
		if(c == TIMEOUT) {
			return;
		}
		crc = ucrc16(c,crc);

		zm->rxd_header[i] = c;
	}

//	crc = ucrc16(0,crc);
//	crc = ucrc16(0,crc);

	/*
	 * receive the crc
	 */

	c = zmodem_rx_hex(zm, to);

	if(c == TIMEOUT) {
		return;
	}

	rxd_crc = c << 8;

	c = zmodem_rx_hex(zm, to);

	if(c == TIMEOUT) {
		return;
	}

	rxd_crc |= c;

	if(rxd_crc == crc) {
		zm->rxd_header_len = 5;
	}
	else {
		lprintf(zm,LOG_ERR,"BAD CRC-16: 0x%hX, expected: 0x%hX", rxd_crc, crc);
	}

	/*
	 * drop the end of line sequence after a hex header
	 */
	c = zmodem_rx(zm, to);
	if(c == '\r') {
		/*
		 * both are expected with CR
		 */
		zmodem_rx(zm, to);	/* drop LF */
	}
}

void
zmodem_rx_bin32_header(zmodem_t* zm, int to)
{
	int c;
	int n;
	unsigned long crc;
	unsigned long rxd_crc;

#if 0 /* def _DEBUG */
	lprintf(zm,LOG_INFO,"rx binary header 32 bits crc");
#endif

	crc = 0xffffffffL;

	for (n=0;n<to;n++) {
		c = zmodem_rx(zm, 1);
		if(c == TIMEOUT) {
			return;
		}
		crc = ucrc32(c,crc);
		zm->rxd_header[n] = c;
	}

	crc = ~crc;

	rxd_crc  = zmodem_rx(zm, 1);
	rxd_crc |= zmodem_rx(zm, 1) << 8;
	rxd_crc |= zmodem_rx(zm, 1) << 16;
	rxd_crc |= zmodem_rx(zm, 1) << 24;

	if(rxd_crc != crc) {
		return;
	}

	zm->rxd_header_len = 5;
}

/*
 * receive any style header
 * if the errors flag is set than whenever an invalid header packet is
 * received INVHDR will be returned. otherwise we wait for a good header
 * also; a flag (receive_32_bit_data) will be set to indicate whether data
 * packets following this header will have 16 or 32 bit data attached.
 * variable headers are not implemented.
 */

int
zmodem_rx_header_raw(zmodem_t* zm, int to, int errors)
{
	int c;

#if 0 /* def _DEBUG */
	lprintf(zm,LOG_INFO,"rx header : ");
#endif
	zm->rxd_header_len = 0;

	do {
		do {
			if((c = zmodem_rx_raw(zm, to)) < 0)
				return(c);
		} while (c != ZPAD);

		if((c = zmodem_rx_raw(zm, to)) < 0)
			return(c);

		if(c == ZPAD) {
			if((c = zmodem_rx_raw(zm, to)) < 0)
				return(c);
		}

		/*
		 * spurious ZPAD check
		 */

		if(c != ZDLE) {
			lprintf(zm,LOG_WARNING,"Expected ZDLE, received: %s", chr((uchar)c));
			continue;
		}

		/*
		 * now read the header style
		 */

		c = zmodem_rx(zm, to);

		if(c == TIMEOUT) {
			lprintf(zm,LOG_ERR,"TIMEOUT %s %d",__FILE__,__LINE__);
			return c;
		}

#if 0 /* def _DEBUG */
		lprintf(zm,LOG_INFO,"");
#endif
		switch (c) {
			case ZBIN:
				zmodem_rx_bin16_header(zm, to);
				zm->receive_32_bit_data = FALSE;
				break;
			case ZHEX:
				zmodem_rx_hex_header(zm, to);
				zm->receive_32_bit_data = FALSE;
				break;
			case ZBIN32:
				zmodem_rx_bin32_header(zm, to);
				zm->receive_32_bit_data = TRUE;
				break;
			default:
				/*
				 * unrecognized header style
				 */
				lprintf(zm,LOG_ERR,"UNRECOGNIZED header style: %s",chr((uchar)c));
				if(errors) {
					return INVHDR;
				}

				continue;
		}
		if(errors && zm->rxd_header_len == 0) {
			return INVHDR;
		}

	} while (zm->rxd_header_len == 0);

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

#if 0 /* def _DEBUG */
	lprintf(zm,LOG_INFO,"type %d",zm->rxd_header[0]);
#endif

	return zm->rxd_header[0];
}

int
zmodem_rx_header(zmodem_t* zm, int timeout)
{
	int ret = zmodem_rx_header_raw(zm, timeout, FALSE);
	if(ret != TIMEOUT)
		lprintf(zm,LOG_INFO,"zmodem_rx_header returning %s", chr((uchar)ret));

	if(ret==ZCAN)
		zm->cancelled=TRUE;

	return ret;
}

int
zmodem_rx_header_and_check(zmodem_t* zm, int timeout)
{
	int type;
	while (TRUE) {
		type = zmodem_rx_header_raw(zm, timeout,TRUE);		

		if(type != INVHDR) {
			break;
		}

		zmodem_tx_znak(zm);
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
	zm->use_variable_headers			= (zm->rxd_header[ZF1] & ZF1_CANVHDR) != 0;

	lprintf(zm,LOG_INFO,"Receiver requested mode: CRC-%u, Escape: %s, %s Headers"
		,zm->can_fcs_32 ? 32 : 16
		,zm->escape_all_control_characters ? "ALL" : "Normal"
		,zm->use_variable_headers ? "Variable" : "Fixed");
}

int zmodem_get_zrinit(zmodem_t* zm)
{
	unsigned char zrqinit_header[] = { ZRQINIT, 0, 0, 0, 0 };

	zmodem_tx_raw(zm,'r');
	zmodem_tx_raw(zm,'z');
	zmodem_tx_raw(zm,'\r');
	zmodem_tx_hex_header(zm,zrqinit_header);
	
	return zmodem_rx_header(zm,zm->recv_timeout);
}

void zmodem_send_zrinit(zmodem_t* zm)
{
	unsigned char zrinit_header[] = { ZRINIT, 0, 0, 0, ZF0_CANFDX | ZF0_CANOVIO | ZF0_CANFC32 };

#if 0
	zrinit_header[ZP0] = sizeof(zm->rx_data_subpacket) >> 8;
	zrinit_header[ZP1] = sizeof(zm->rx_data_subpacket) & 0xff;
#endif

	zmodem_tx_hex_header(zm, zrinit_header);
}

void zmodem_send_nak(zmodem_t* zm)
{
	zmodem_send_pos_header(zm, ZNAK, 0, /* Hex? */ TRUE);
}

int zmodem_send_zfin(zmodem_t* zm)
{
	int type;
	unsigned char zfin_header[] = { ZFIN, 0, 0, 0, 0 };

	zmodem_tx_hex_header(zm,zfin_header);
	do {
		type = zmodem_rx_header(zm,zm->recv_timeout);
	} while (type != ZFIN && type != TIMEOUT);
	
	/*
	 * these Os are formally required; but they don't do a thing
	 * unfortunately many programs require them to exit 
	 * (both programs already sent a ZFIN so why bother ?)
	 */

	if(type != TIMEOUT) {
		zmodem_tx_raw(zm,'O');
		zmodem_tx_raw(zm,'O');
	}

	return 0;
}

/*
 * send from the current position in the file
 * all the way to end of file or until something goes wrong.
 * (ZNAK or ZRPOS received)
 * the name is only used to show progress
 */

int
zmodem_send_from(zmodem_t* zm, FILE* fp, ulong pos, ulong fsize, ulong* sent)
{
	int n;
	uchar type = ZCRCG;

	if(sent!=NULL)
		*sent=0;

	fseek(fp,pos,SEEK_SET);

	zmodem_send_pos_header(zm, ZDATA, pos, /* Hex? */ FALSE);

	/*
	 * send the data in the file
	 */

	while (!feof(fp)) {

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

		/*
		 * at end of file wait for an ACK
		 */
		if((ulong)ftell(fp) == fsize) {
			type = ZCRCW;
		}

		zmodem_tx_data(zm, type, zm->tx_data_subpacket, n);

		if(sent!=NULL)
			*sent+=n;

		if(type == ZCRCW) {
			int type;
			do {
				type = zmodem_rx_header(zm, zm->recv_timeout);
				if(type == ZNAK || type == ZRPOS || type == TIMEOUT) {
					return type;
				}
			} while (type != ZACK);

			if((ulong)ftell(fp) >= fsize) {
				if(*(zm->mode)&DEBUG) {
					lprintf(zm,LOG_INFO,"end of file (%ld)", fsize );
				}
				return ZACK;
			}
		}

		/* 
		 * characters from the other side
		 * check out that header
		 */

		while (zmodem_rx_poll(zm)) {
			int type;
			int c;
			if((c = zmodem_rx_raw(zm, 1)) < 0)
				return(c);
			if(c == ZPAD) {
				type = zmodem_rx_header(zm, 1);
				if(type != TIMEOUT && type != ACK) {
					return type;
				}
			}
			if(zm->cancelled)
				return(-1);
		}

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
		for(errors=0; errors<zm->max_errors && !zm->cancelled; errors++) {
			lprintf(zm,LOG_INFO,"Sending ZRQINIT (%u of %u)",errors+1,zm->max_errors);
			i = zmodem_get_zrinit(zm);
			if(i == ZRINIT) {
				zmodem_parse_zrinit(zm);
				break;
			}
			lprintf(zm,LOG_WARNING,"RX header type: %d 0x%02X", i, i);
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
		if(*(zm->mode)&DEBUG) {
			lprintf(zm,LOG_INFO,"zmtx: protecting destination");
		}
	}

	if(zm->management_clobber) {
		zfile_frame[ZF1] = ZF1_ZMCLOB;
		if(*(zm->mode)&DEBUG) {
			lprintf(zm,LOG_INFO,"zmtx: overwriting destination");
		}
	}

	if(zm->management_newer) {
		zfile_frame[ZF1] = ZF1_ZMNEW;
		if(*(zm->mode)&DEBUG) {
			lprintf(zm,LOG_INFO,"zmtx: overwriting destination if newer");
		}
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

		zmodem_tx_header(zm,zfile_frame);
		zmodem_tx_data(zm,ZCRCW,zm->tx_data_subpacket,p - zm->tx_data_subpacket);
	
		/*
		 * wait for anything but an ZACK packet
		 */

		do {
			type = zmodem_rx_header(zm,zm->recv_timeout);
			if(zm->cancelled)
				return(FALSE);
		} while (type == ZACK);

#if 0
		lprintf(zm,LOG_INFO,"type : %d",type);
#endif

		if(type == ZSKIP) {
			zm->file_skipped=TRUE;
			lprintf(zm,LOG_WARNING,"File skipped by receiver");
			return(FALSE);
		}

	} while (type != ZRPOS);

	zm->transfer_start = time(NULL);

	if(start!=NULL)		
		*start=zm->transfer_start;

	do {
		/*
		 * fetch pos from the ZRPOS header
		 */

		if(type == ZRPOS) {
			pos = zm->rxd_header[ZP0] | (zm->rxd_header[ZP1] << 8) | (zm->rxd_header[ZP2] << 16) | (zm->rxd_header[ZP3] << 24);
		}

		/*
		 * and start sending
		 */

		type = zmodem_send_from(zm, fp, pos, s.st_size, &sent_bytes);

		if(sent!=NULL)
			*sent+=sent_bytes;

		if(type == ZFERR || type == ZABORT || zm->cancelled)
			return(FALSE);

	} while (type == ZRPOS || type == ZNAK);

	lprintf(zm,LOG_INFO,"Finishing transfer on rx of header type: %s", chr((uchar)type));

	/*
	 * file sent. send end of file frame
	 * and wait for zrinit. if it doesnt come then try again
	 */

	zeof_frame[ZP0] =  s.st_size        & 0xff;
	zeof_frame[ZP1] = (s.st_size >> 8)  & 0xff;
	zeof_frame[ZP2] = (s.st_size >> 16) & 0xff;
	zeof_frame[ZP3] = (s.st_size >> 24) & 0xff;

	zm->raw_trace = FALSE;
	for(errors=0;errors<zm->max_errors && !zm->cancelled;errors++) {
		lprintf(zm,LOG_INFO,"Sending End-of-File (ZEOF) frame (%u of %u)", errors+1, zm->max_errors);
		zmodem_tx_hex_header(zm,zeof_frame);
		if(zmodem_rx_header(zm,zm->recv_timeout)==ZRINIT) {
			success=TRUE;
			break;
		}
	}

	return(success);
}

#if 0

zmodem_send_files(char** fname, int total_files)
{
	int i;
	int fnum;
	char * s;

	/*
	 * clear the input queue from any possible garbage
	 * this also clears a possible ZRINIT from an already started
	 * zmodem receiver. this doesn't harm because we reinvite to
	 * receive again below and it may be that the receiver whose
	 * ZRINIT we are about to wipe has already died.
	 */

	zmodem_rx_purge();

	/*
	 * establish contact with the receiver
	 */

	lprintf(zm,LOG_INFO,"zmtx: establishing contact with receiver");

	i = 0;
	do {
		unsigned char zrqinit_header[] = { ZRQINIT, 0, 0, 0, 0 };
		i++;
		if(i > 10) {
			lprintf(zm,LOG_INFO,"zmtx: can't establish contact with receiver");
			bail(3);
		}

		zmodem_tx_raw('r');
		zmodem_tx_raw('z');
		zmodem_tx_raw('\r');
		zmodem_tx_hex_header(zrqinit_header);
	} while (zmodem_rx_header(7) != ZRINIT);

	lprintf(zm,LOG_INFO,"zmtx: contact established");
	lprintf(zm,LOG_INFO,"zmtx: starting file transfer");

	/*
	 * decode receiver capability flags
	 * forget about encryption and compression.
	 */

	zmodem_can_full_duplex					= (zm->rxd_header[ZF0] & ZF0_CANFDX)  != 0;
	zmodem_can_overlap_io					= (zm->rxd_header[ZF0] & ZF0_CANOVIO) != 0;
	zmodem_can_break						= (zm->rxd_header[ZF0] & ZF0_CANBRK)  != 0;
	zmodem_can_fcs_32						= (zm->rxd_header[ZF0] & ZF0_CANFC32) != 0;
	zmodem_escape_all_control_characters	= (zm->rxd_header[ZF0] & ZF0_ESCCTL)  != 0;
	zmodem_escape_8th_bit					= (zm->rxd_header[ZF0] & ZF0_ESC8)    != 0;

	zm->use_variable_headers				= (zm->rxd_header[ZF1] & ZF1_CANVHDR) != 0;

	if(*(zm->mode)&DEBUG) {
		lprintf(zm,LOG_INFO,"receiver %s full duplex"          ,zmodem_can_full_duplex               ? "can"      : "can't");
		lprintf(zm,LOG_INFO,"receiver %s overlap io"           ,zmodem_can_overlap_io                ? "can"      : "can't");
		lprintf(zm,LOG_INFO,"receiver %s break"                ,zmodem_can_break                     ? "can"      : "can't");
		lprintf(zm,LOG_INFO,"receiver %s fcs 32"               ,zmodem_can_fcs_32                    ? "can"      : "can't");
		lprintf(zm,LOG_INFO,"receiver %s escaped control chars",zmodem_escape_all_control_characters ? "requests" : "doesn't request");
		lprintf(zm,LOG_INFO,"receiver %s escaped 8th bit"      ,zmodem_escape_8th_bit                ? "requests" : "doesn't request");
		lprintf(zm,LOG_INFO,"receiver %s use variable headers" ,zm->use_variable_headers          ? "can"      : "can't");
	}

	/* 
	 * and send each file in turn
	 */

	n_files_remaining = total_files;

	for(fnum=0;fnum<total_files;fnum++) {
		if(send_file(fname[fnum])) {
			lprintf(zm,LOG_WARNING,"zmtx: remote aborted.");
			break;
		}
		n_files_remaining--;
	}

	/*
	 * close the session
	 */

	lprintf(zm,LOG_INFO,"zmtx: closing the session");

	{
		int type;
		unsigned char zfin_header[] = { ZFIN, 0, 0, 0, 0 };

		zmodem_tx_hex_header(zfin_header);
		do {
			type = zmodem_rx_header(10);
		} while (type != ZFIN && type != TIMEOUT);
		
		/*
		 * these Os are formally required; but they don't do a thing
		 * unfortunately many programs require them to exit 
		 * (both programs already sent a ZFIN so why bother ?)
		 */

		if(type != TIMEOUT) {
			zmodem_tx_raw('O');
			zmodem_tx_raw('O');
		}
	}

	/*
	 * c'est fini
	 */

	if(*(zm->mode)&DEBUG) {
		lprintf(zm,LOG_INFO,"zmtx: cleanup and exit");
	}

	return 0;
}

#endif

int zmodem_recv_init(zmodem_t* zm)
{
	int			ch;
	int			type=CAN;
	unsigned	errors;

	while(!zm->cancelled && (ch=zm->recv_byte(zm,0))!=NOINP)
		lprintf(zm,LOG_WARNING,"Throwing out received: %s",chr((uchar)ch));

	for(errors=0; errors<zm->max_errors && !zm->cancelled; errors++) {
		lprintf(zm,LOG_DEBUG,"Sending ZRINIT (%u of %u)", errors+1, zm->max_errors);
		zmodem_send_zrinit(zm);
		type = zmodem_rx_header(zm, zm->recv_timeout);
		if(type!=TIMEOUT && type!=ZRQINIT)
			break;
		if(type!=TIMEOUT)
			lprintf(zm,LOG_WARNING,"Received header type: %s, expected ZFILE", chr((uchar)type));
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

	i=zmodem_rx_data(zm, block, sizeof(block), &l);

	if(i!=FRAMEOK && i!=ENDOFFRAME)
		return(FALSE);

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
	int			i;
	unsigned	errors=0;

	while(errors<zm->max_errors 
		&& (ulong)ftell(fp) < fsize && !zm->cancelled) {
		if((i = zmodem_recv_file_frame(zm,fp,offset,fsize,start)) == ZEOF)
			break;
		if(i!=ENDOFFRAME) {
			if(i>=0)
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
 	 * create a ZRPOS frame and send it to the other side
	 */
	zmodem_send_pos_header(zm, ZRPOS, ftell(fp), /* Hex? */ TRUE);

	/*
	 * wait for a ZDATA header with the right file offset
	 * or a timeout or a ZFIN
	 */

	do {
		do {
			type = zmodem_rx_header(zm,zm->recv_timeout);
			if (type == TIMEOUT) {
				return TIMEOUT;
			}
		} while (type != ZDATA && !zm->cancelled);

		pos = zm->rxd_header[ZP0] | (zm->rxd_header[ZP1] << 8) |
			(zm->rxd_header[ZP2] << 16) | (zm->rxd_header[ZP3] << 24);
	} while (pos != ftell(fp) && !zm->cancelled);
		
	do {
		type = zmodem_rx_data(zm,zm->rx_data_subpacket,sizeof(zm->rx_data_subpacket),&n);

/*		fprintf(stderr,"packet len %d type %d\n",n,type);
*/
		if (type == ENDOFFRAME || type == FRAMEOK) {
			fwrite(zm->rx_data_subpacket,1,n,fp);
		}

		if(zm->progress!=NULL)
			zm->progress(zm->cbdata,offset,ftell(fp),fsize,start);

	} while (type == FRAMEOK && !zm->cancelled);

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

void zmodem_init(zmodem_t* zm, void* cbdata, long* mode
				,int	(*lputs)(void*, int level, const char* str)
				,void	(*progress)(void* unused, ulong, ulong, ulong, time_t)
				,int	(*send_byte)(void*, uchar ch, unsigned timeout)
				,int	(*recv_byte)(void*, unsigned timeout))
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
	zm->max_errors=10;

	zm->cbdata=cbdata;
	zm->mode=mode;
	zm->lputs=lputs;
	zm->progress=progress;
	zm->send_byte=send_byte;
	zm->recv_byte=recv_byte;
}
