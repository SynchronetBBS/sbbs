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

#if 0

/****************************************/
/* Zmodem specific functions start here */
/****************************************/

/**********************************************/
/* Output single byte as two hex ASCII digits */
/**********************************************/
void putzhex(uchar val)
{
	char *digits="0123456789abcdef";

	putcom(digits[(val&0xF0)>>4]);
	putcom(digits[val&0xF]);
}

/***********************/
/* Output a hex header */
/***********************/
void putzhhdr(char type)
{
	uint i;
	ushort crc=0;

	putcom(ZPAD);
	putcom(ZPAD);
	putcom(ZDLE);
	if(zmode&VAR_HDRS) {
		putcom(ZVHEX);
		putzhex(4); 
	}
	else
		putcom(ZHEX);
	putzhex(type);
//	crc=ucrc16(type,crc);
	for(i=0;i<4;i++) {
		putzhex(Txhdr[i]);
		crc=ucrc16(Txhdr[i],crc); 
	}
//	crc=ucrc16(0,crc);
//	crc=ucrc16(0,crc);
	putzhex(crc>>8);
	putzhex(crc&0xff);
	putcom(CR);
	putcom(LF); 	/* Chuck's RZ.C sends LF|0x80 for some unknown reason */
	if(type!=ZFIN && type!=ZACK)
		putcom(XON);
}

/****************************************************************************/
/* Stores a long in the Zmodem transmit header (usually position offset)	*/
/****************************************************************************/
void ltohdr(long l)
{

	Txhdr[ZP0] = l;
	Txhdr[ZP1] = l>>8;
	Txhdr[ZP2] = l>>16;
	Txhdr[ZP3] = l>>24;
}

/****************************************************************************/
/* Outputs single Zmodem character, escaping with ZDLE when appropriate     */
/****************************************************************************/
void putzcom(uchar ch)
{
    static lastsent;

	if(ch&0x60) /* not a control char */
		putcom(lastsent=ch);
	else
		switch(ch) {
			case DLE:
			case DLE|0x80:          /* even if high-bit set */
			case XON:
			case XON|0x80:
			case XOFF:
			case XOFF|0x80:
			case ZDLE:
				putcom(ZDLE);
				ch^=0x40;
				putcom(lastsent=ch);
				break;
			case CR:
			case CR|0x80:
				if(!(zmode&CTRL_ESC) && (lastsent&0x7f)!='@')
					putcom(lastsent=ch);
				else {
					putcom(ZDLE);
					ch^=0x40;
					putcom(lastsent=ch); 
				}
				break;
			default:
				if(zmode&CTRL_ESC && !(ch&0x60)) {  /* it's a ctrl char */
					putcom(ZDLE);
					ch^=0x40; 
				}
				putcom(lastsent=ch);
				break; 
		}
}

/*
 * Read a byte, checking for ZMODEM escape encoding
 *  including CAN*5 which represents a quick abort
 */
int getzcom()
{
	int i;

	while(1) {
		/* Quick check for non control characters */
		if((i=getcom(Rxtimeout))&0x60)
			return(i);
		if(i==ZDLE)
			break;
		if((i&0x7f)==XOFF || (i&0x7f)==XON)
			continue;
		if(zmode&CTRL_ESC && !(i&0x60))
			continue;
		return(i); 
	}

	while(1) {	/* Escaped characters */
		if((i=getcom(Rxtimeout))<0)
			return(i);
		if(i==CAN && (i=getcom(Rxtimeout))<0)
			return(i);
		if(i==CAN && (i=getcom(Rxtimeout))<0)
			return(i);
		if(i==CAN && (i=getcom(Rxtimeout))<0)
			return(i);
		switch (i) {
			case CAN:
				return(GOTCAN);
			case ZCRCE:
			case ZCRCG:
			case ZCRCQ:
			case ZCRCW:
				return(i|GOTOR);
			case ZRUB0:
				return(0x7f);
			case ZRUB1:
				return(0xff);
			case XON:
			case XON|0x80:
			case XOFF:
			case XOFF|0x80:
				continue;
			default:
				if (zmode&CTRL_ESC && !(i&0x60))
					continue;
				if ((i&0x60)==0x40)
					return(i^0x40);
				break; 
		}
		break; 
	}
	return(ERROR);
}



/*
 * Read a character from the modem line with timeout.
 *  Eat parity, XON and XOFF characters.
 */
int getcom7()
{
	int i;

	while(1) {
		i=getcom(10);
		switch(i) {
			case XON:
			case XOFF:
				continue;
			case CR:
			case LF:
			case NOINP:
			case ZDLE:
				return(i);
			default:
				if(!(i&0x60) && zmode&CTRL_ESC)
					continue;
				return(i); 
		} 
	}
}

#endif

/*
 * zmodem primitives and other code common to zmtx and zmrx
 */

#include <stdio.h>
#include <sys/stat.h>	/* struct stat */

#include "sexyz.h"
#include "genwrap.h"
#include "sockwrap.h"

#include "zmodem.h"
#include "crc16.h"
#include "crc32.h"

#define UPDCRC16 ucrc16
#define UPDCRC32 ucrc32

#define ENDOFFRAME 2
#define FRAMEOK    1
#define TIMEOUT   -1	/* rx routine did not receive a character within timeout */
#define INVHDR    -2	/* invalid header received; but within timeout */
#define INVDATA   -3	/* invalid data subpacket received */
#define ZDLEESC 0x8000	/* one of ZCRCE; ZCRCG; ZCRCQ or ZCRCW was received; ZDLE escaped */

#define HDRLEN     5	/* size of a zmodem header */


int opt_v = TRUE;		/* show progress output */
int opt_d = TRUE;		/* show debug output */

/*
 * read bytes as long as rdchk indicates that
 * more data is available.
 */

void
zmodem_rx_purge(zmodem_t* zm)

{
	while(recv_byte(zm->sock,0,zm->mode)<=0xff);
}

/* 
 * transmit a character. 
 * this is the raw modem interface
 */

void
zmodem_tx_raw(zmodem_t* zm, unsigned char ch)
{

#ifdef _DEBUG
	if (zm->raw_trace) {
		fprintf(stderr,"%02x ",ch);
	}
#endif

	if(send_byte(zm->sock,ch,10,zm->mode))
		fprintf(stderr,"!Send error: %u\n",ERROR_VALUE);

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
		case ZDLE:
			zmodem_tx_esc(zm, c);
			return;
		case 0x8d:
		case 0x0d:
			if (zm->escape_all_control_characters && (zm->last_sent&0x7f) == '@') {
				zmodem_tx_esc(zm, c);
				return;
			}
			break;
		case 0x10:
		case 0x90:
		case 0x11:
		case 0x91:
		case 0x13:
		case 0x93:
			zmodem_tx_esc(zm, c);
			return;
		default:
			if (zm->escape_all_control_characters && (c & 0x60) == 0) {
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

/* 
 * transmit a hex header.
 * these routines use tx_raw because we're sure that all the
 * characters are not to be escaped.
 */

void
zmodem_tx_nibble(zmodem_t* zm, int n)

{
	n &= 0x0f;
	if (n < 10) {
		n += '0';
	}
	else {
		n += 'a' - 10;
	}

	zmodem_tx_raw(zm, (uchar)n);
}

void
zmodem_tx_hex(zmodem_t* zm, int h)

{
	zmodem_tx_nibble(zm, h >> 4);
	zmodem_tx_nibble(zm, h);
}

void
zmodem_tx_hex_header(zmodem_t* zm, unsigned char * p)

{
	int i;
	unsigned short int crc;

#ifdef _DEBUG
	fprintf(stderr,"tx_hheader : ");
#endif

	zmodem_tx_raw(zm, ZPAD);
	zmodem_tx_raw(zm, ZPAD);
	zmodem_tx_raw(zm, ZDLE);

	if (zm->use_variable_headers) {
		zmodem_tx_raw(zm, ZVHEX);
		zmodem_tx_hex(zm, HDRLEN);
	}
	else {
		zmodem_tx_raw(zm, ZHEX);
	}

	/*
 	 * initialise the crc
	 */

	crc = 0;

	/*
 	 * transmit the header
	 */

	for (i=0;i<HDRLEN;i++) {
		zmodem_tx_hex(zm, *p);
		crc = UPDCRC16(*p, crc);
		p++;
	}

	/*
 	 * update the crc as though it were zero
	 */

//	crc = UPDCRC16(0,crc);
//	crc = UPDCRC16(0,crc);

	/* 
	 * transmit the crc
	 */

	zmodem_tx_hex(zm, crc >> 8);
	zmodem_tx_hex(zm, crc&0xff);

	/*
	 * end of line sequence
	 */

	zmodem_tx_raw(zm, '\r');
	zmodem_tx_raw(zm, '\n');

	zmodem_tx_raw(zm, XON);


#ifdef _DEBUG
	fprintf(stderr,"\n");
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

#ifdef _DEBUG
	fprintf(stderr,"tx binary header 32 bits crc\n");
//	zm->raw_trace = 1;
#endif

	zmodem_tx_raw(zm, ZPAD);
	zmodem_tx_raw(zm, ZPAD);
	zmodem_tx_raw(zm, ZDLE);

	if (zm->use_variable_headers) {
		zmodem_tx_raw(zm, ZVBIN32);
		zmodem_tx(zm, HDRLEN);
	}
	else {
		zmodem_tx_raw(zm, ZBIN32);
	}

	crc = 0xffffffffL;

	for (i=0;i<HDRLEN;i++) {
		crc = UPDCRC32(*p,crc);
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

#ifdef _DEBUG
	fprintf(stderr,"tx binary header 16 bits crc\n");
#endif

	zmodem_tx_raw(zm, ZPAD);
	zmodem_tx_raw(zm, ZPAD);
	zmodem_tx_raw(zm, ZDLE);

	if (zm->use_variable_headers) {
		zmodem_tx_raw(zm, ZVBIN);
		zmodem_tx(zm, HDRLEN);
	}
	else {
		zmodem_tx_raw(zm, ZBIN);
	}

	crc = 0;

	for (i=0;i<HDRLEN;i++) {
		crc = UPDCRC16(*p,crc);
		zmodem_tx(zm, *p++);
	}

//	crc = UPDCRC16(0,crc);
//	crc = UPDCRC16(0,crc);

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
	if (zm->can_fcs_32) {
		if (!zm->want_fcs_16) {
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

#ifdef _DEBUG
	fprintf(stderr,"tx_32_data\n");
#endif

	crc = 0xffffffffl;

	while (l > 0) {
		crc = UPDCRC32(*p,crc);
		zmodem_tx(zm, *p++);
		l--;
	}

	crc = UPDCRC32(sub_frame_type, crc);

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

#ifdef _DEBUG
	fprintf(stderr,"tx_16_data\n");
#endif

	crc = 0;

	while (l > 0) {
		crc = UPDCRC16(*p,crc);
		zmodem_tx(zm, *p++);
		l--;
	}

	crc = UPDCRC16(sub_frame_type,crc);

	zmodem_tx_raw(zm, ZDLE); 
	zmodem_tx_raw(zm, sub_frame_type);
	
//	crc = UPDCRC16(0,crc);
//	crc = UPDCRC16(0,crc);

	zmodem_tx(zm, (uchar)(crc >> 8));
	zmodem_tx(zm, (uchar)(crc&0xff));
}

/*
 * send a data subpacket using crc 16 or crc 32 as desired by the receiver
 */

void
zmodem_tx_data(zmodem_t* zm, uchar sub_frame_type,unsigned char * p, int l)

{
	if (!zm->want_fcs_16 && zm->can_fcs_32) {
		zmodem_tx_32_data(zm, sub_frame_type,p,l);
	}
	else {	
		zmodem_tx_16_data(zm, sub_frame_type,p,l);
	}

	if (sub_frame_type == ZCRCW) {
		zmodem_tx_raw(zm, XON);
	}

}

void
zmodem_tx_pos_header(zmodem_t* zm, int type,long pos) 

{
	char header[5];

	header[0]   = type;
	header[ZP0] =  pos        & 0xff;
	header[ZP1] = (pos >>  8) & 0xff;
	header[ZP2] = (pos >> 16) & 0xff;
	header[ZP3] = (pos >> 24) & 0xff;

	zmodem_tx_hex_header(zm, header);
}

void
zmodem_tx_znak(zmodem_t* zm)

{
	fprintf(stderr,"tx_znak\n");

	zmodem_tx_pos_header(zm, ZNAK, zm->ack_file_pos);
}

void
zmodem_tx_zskip(zmodem_t* zm)

{
	zmodem_tx_pos_header(zm, ZSKIP, 0L);
}

/*
 * receive any style header within timeout milliseconds
 */

int
zmodem_rx_poll(zmodem_t* zm)
{
	int rd=0;

	socket_check(zm->sock,&rd,NULL,0);

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

	if((c=recv_byte(zm->sock,to,zm->mode)) > 0xff)
		return TIMEOUT;

//	fprintf(stdout,"%02X  ",c);

	if (c == CAN) {
		zm->n_cans++;
		if (zm->n_cans == 5) {
			fprintf(stderr,"\nCancelled Remotely\n");
			bail(CAN);
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
			c = zmodem_rx_raw(zm, to);
			if (c == TIMEOUT) {
				return c;
			}
	
			switch (c) {
				case ZDLE:
					break;
				case 0x11:
				case 0x91:
				case 0x13:
				case 0x93:
					continue;			
				default:
					/*
	 				 * if all control characters should be escaped and 
					 * this one wasnt then its spurious and should be dropped.
					 */
					if (zm->escape_all_control_characters && (c & 0x60) == 0) {
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
			c = zmodem_rx_raw(zm, to);

			if (c == 0x11 || c == 0x13 || c == 0x91 || c == 0x93 || c == ZDLE) {
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
					return (c | ZDLEESC);
				case ZRUB0:
					return 0x7f;
				case ZRUB1:
					return 0xff;
				default:
					if (zm->escape_all_control_characters && (c & 0x60) == 0) {
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
					if ((c & 0x60) == 0x40) {
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
zmodem_rx_32_data(zmodem_t* zm, unsigned char * p,int * l)

{
	int c;
	unsigned long rxd_crc;
	unsigned long crc;
	int sub_frame_type;

#ifdef _DEBUG
	fprintf(stderr,"rx_32_data\n");
#endif

	crc = 0xffffffffl;

	do {
		c = zmodem_rx(zm, 1);

		if (c == TIMEOUT) {
			return TIMEOUT;
		}
		if (c < 0x100) {
			crc = UPDCRC32(c,crc);
			*p++ = c;
			(*l)++;
			continue;
		}
	} while (c < 0x100);

	sub_frame_type = c & 0xff;

	crc = UPDCRC32(sub_frame_type, crc);

	crc = ~crc;

	rxd_crc  = zmodem_rx(zm, 1);
	rxd_crc |= zmodem_rx(zm, 1) << 8;
	rxd_crc |= zmodem_rx(zm, 1) << 16;
	rxd_crc |= zmodem_rx(zm, 1) << 24;

	if (rxd_crc != crc) {
		return FALSE;
	}

	zm->ack_file_pos += *l;

	return sub_frame_type;
}

int
zmodem_rx_16_data(zmodem_t* zm, register unsigned char * p,int * l)

{
	register int c;
	int sub_frame_type;
 	register unsigned short crc;
	unsigned short rxd_crc;

#ifdef _DEBUG
	fprintf(stderr,"rx_16_data\n");
#endif

	crc = 0;

	do {
		c = zmodem_rx(zm, 5);

		if (c == TIMEOUT) {
			return TIMEOUT;
		}
		if (c < 0x100) {
			crc = UPDCRC16(c,crc);
			*p++ = c;
			(*l)++;
		}
	} while (c < 0x100);

	sub_frame_type = c & 0xff;

	crc = UPDCRC16(sub_frame_type,crc);

//	crc = UPDCRC16(0,crc);
//	crc = UPDCRC16(0,crc);

	rxd_crc  = zmodem_rx(zm, 1) << 8;
	rxd_crc |= zmodem_rx(zm, 1);

	if (rxd_crc != crc) {
		return FALSE;
	}

	zm->ack_file_pos += *l;

	return sub_frame_type;
}

int
zmodem_rx_data(zmodem_t* zm, unsigned char * p, int * l)

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

	if (zm->receive_32_bit_data) {
		sub_frame_type = zmodem_rx_32_data(zm, p,l);
	}
	else {	
		sub_frame_type = zmodem_rx_16_data(zm, p,l);
	}

	switch (sub_frame_type)  {
		case TIMEOUT:
			return TIMEOUT;
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
			zmodem_tx_pos_header(zm, ZACK, pos);
			return FRAMEOK;
		/*
		 * frame ends; ZACK expected
		 */
		case ZCRCW:
			zmodem_tx_pos_header(zm, ZACK, pos);
			return ENDOFFRAME;
	}

	return FALSE;
}

int
zmodem_rx_nibble(zmodem_t* zm, int to) 

{
	int c;

	c = zmodem_rx(zm, to);

	if (c == TIMEOUT) {
		return c;
	}

	if (c > '9') {
		if (c < 'a' || c > 'f') {
			/*
			 * illegal hex; different than expected.
			 * we might as well time out.
			 */
			return TIMEOUT;
		}

		c -= 'a' - 10;
	}
	else {
		if (c < '0') {
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

	n1 = zmodem_rx_nibble(zm, to);

	if (n1 == TIMEOUT) {
		return n1;
	}

	n0 = zmodem_rx_nibble(zm, to);

	if (n0 == TIMEOUT) {
		return n0;
	}

	return (n1 << 4) | n0;
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

#ifdef _DEBUG
	fprintf(stderr,"rx binary header 16 bits crc\n");
#endif

	crc = 0;

	for (n=0;n<5;n++) {
		c = zmodem_rx(zm, to);
		if (c == TIMEOUT) {
#ifdef _DEBUG
			fprintf(stderr,"timeout\n");
#endif
			return;
		}
		crc = UPDCRC16(c,crc);
		zm->rxd_header[n] = c;
	}

//	crc = UPDCRC16(0,crc);
//	crc = UPDCRC16(0,crc);

	rxd_crc  = zmodem_rx(zm, 1) << 8;
	rxd_crc |= zmodem_rx(zm, 1);

	if (rxd_crc != crc) {
#ifdef _DEBUG
		fprintf(stderr,"bad crc %4.4x %4.4x\n",rxd_crc,crc);
#endif
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

#ifdef _DEBUG
	fprintf(stderr,"rx_hex_header : ");
#endif
	for (i=0;i<5;i++) {
		c = zmodem_rx_hex(zm, to);
		if (c == TIMEOUT) {
			return;
		}
		crc = UPDCRC16(c,crc);

		zm->rxd_header[i] = c;
	}

//	crc = UPDCRC16(0,crc);
//	crc = UPDCRC16(0,crc);

	/*
	 * receive the crc
	 */

	c = zmodem_rx_hex(zm, to);

	if (c == TIMEOUT) {
		return;
	}

	rxd_crc = c << 8;

	c = zmodem_rx_hex(zm, to);

	if (c == TIMEOUT) {
		return;
	}

	rxd_crc |= c;

	if (rxd_crc == crc) {
		zm->rxd_header_len = 5;
	}
#ifdef _DEBUG
	else {
		fprintf(stderr,"\n!BAD CRC-16: 0x%hX, expected: 0x%hX\n", rxd_crc, crc);
	}
#endif

	/*
	 * drop the end of line sequence after a hex header
	 */
	c = zmodem_rx(zm, to);
	if (c == CR) {
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

#ifdef _DEBUG
	fprintf(stderr,"rx binary header 32 bits crc\n");
#endif

	crc = 0xffffffffL;

	for (n=0;n<to;n++) {
		c = zmodem_rx(zm, 1);
		if (c == TIMEOUT) {
			return;
		}
		crc = UPDCRC32(c,crc);
		zm->rxd_header[n] = c;
	}

	crc = ~crc;

	rxd_crc  = zmodem_rx(zm, 1);
	rxd_crc |= zmodem_rx(zm, 1) << 8;
	rxd_crc |= zmodem_rx(zm, 1) << 16;
	rxd_crc |= zmodem_rx(zm, 1) << 24;

	if (rxd_crc != crc) {
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
zmodem_rx_header_raw(zmodem_t* zm, int to,int errors)

{
	int c;

#ifdef _DEBUG
	fprintf(stderr,"rx header : ");
#endif
	zm->rxd_header_len = 0;

	do {
		do {
			c = zmodem_rx_raw(zm, to);
			if (c == TIMEOUT) {
				fprintf(stderr,"\n%s %d\n",__FILE__,__LINE__);
				return c;
			}
		} while (c != ZPAD);

		c = zmodem_rx_raw(zm, to);
		if (c == TIMEOUT) {
			fprintf(stderr,"\n%s %d\n",__FILE__,__LINE__);
			return c;
		}

		if (c == ZPAD) {
			c = zmodem_rx_raw(zm, to);
			if (c == TIMEOUT) {
				fprintf(stderr,"\n%s %d\n",__FILE__,__LINE__);
				return c;
			}
		}

		/*
		 * spurious ZPAD check
		 */

		if (c != ZDLE) {
#ifdef _DEBUG
			fprintf(stderr,"expected ZDLE; got %c\n",c);
#endif
			continue;
		}

		/*
		 * now read the header style
		 */

		c = zmodem_rx(zm, to);

		if (c == TIMEOUT) {
			fprintf(stderr,"\n%s %d\n",__FILE__,__LINE__);
			return c;
		}

#ifdef _DEBUG
		fprintf(stderr,"\n");
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
#ifdef _DEBUG
				fprintf(stderr,"unrecognized header style %c\n",c);
#endif
				if (errors) {
					return INVHDR;
				}

				continue;
		}
		if (errors && zm->rxd_header_len == 0) {
			return INVHDR;
		}

	} while (zm->rxd_header_len == 0);

	/*
 	 * this appears to have been a valid header.
	 * return its type.
	 */

	if (zm->rxd_header[0] == ZDATA) {
		zm->ack_file_pos = zm->rxd_header[ZP0] | (zm->rxd_header[ZP1] << 8) |
			(zm->rxd_header[ZP2] << 16) | (zm->rxd_header[ZP3] << 24);
	}

	if (zm->rxd_header[0] == ZFILE) {
		zm->ack_file_pos = 0l;
	}

#ifdef _DEBUG
	fprintf(stderr,"type %d\n",zm->rxd_header[0]);
#endif

	return zm->rxd_header[0];
}

int
zmodem_rx_header(zmodem_t* zm, int timeout)

{
	return zmodem_rx_header_raw(zm, timeout, FALSE);
}

int
zmodem_rx_header_and_check(zmodem_t* zm, int timeout)

{
	int type;
	while (TRUE) {
		type = zmodem_rx_header_raw(zm, timeout,TRUE);		

		if (type != INVHDR) {
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
}

int zmodem_get_zrinit(zmodem_t* zm)
{
	unsigned char zrqinit_header[] = { ZRQINIT, 0, 0, 0, 0 };

	zmodem_tx_raw(zm,'r');
	zmodem_tx_raw(zm,'z');
	zmodem_tx_raw(zm,'\r');
	zmodem_tx_hex_header(zm,zrqinit_header);
	
	return zmodem_rx_header(zm,7);
}

int zmodem_send_zfin(zmodem_t* zm)
{
	int type;
	unsigned char zfin_header[] = { ZFIN, 0, 0, 0, 0 };

	zmodem_tx_hex_header(zm,zfin_header);
	do {
		type = zmodem_rx_header(zm,10);
	} while (type != ZFIN && type != TIMEOUT);
	
	/*
	 * these Os are formally required; but they don't do a thing
	 * unfortunately many programs require them to exit 
	 * (both programs already sent a ZFIN so why bother ?)
	 */

	if (type != TIMEOUT) {
		zmodem_tx_raw(zm,'O');
		zmodem_tx_raw(zm,'O');
	}

	return 0;
}

/* 
 * show the progress of the transfer like this:
 * zmtx: sending file "garbage" 4096 bytes ( 20%)
 */

void
show_progress(zmodem_t* zm, FILE * fp)

{
	time_t duration;
	int cps;
	int percentage;

	if (zm->current_file_size > 0) {
		percentage = (ftell(fp) * 100) / zm->current_file_size;
	}
	else {
		percentage = 100;
	}

	duration = time(NULL) - zm->transfer_start;

	if (duration == 0l) {
		duration = 1l;
	}

	cps = ftell(fp) / duration;

	fprintf(stderr,"zmtx: %8ld bytes (%3d %%/%5d cps)           \r",
		ftell(fp),percentage,cps);
}

/*
 * send from the current position in the file
 * all the way to end of file or until something goes wrong.
 * (ZNAK or ZRPOS received)
 * the name is only used to show progress
 */

int
send_from(zmodem_t* zm, FILE * fp)

{
	int n;
	long pos;
	uchar type = ZCRCG;
	char zdata_frame[] = { ZDATA, 0, 0, 0, 0 };

	/*
 	 * put the file position in the ZDATA frame
	 */

	pos = ftell(fp);
	zdata_frame[ZP0] =  pos        & 0xff;
	zdata_frame[ZP1] = (pos >> 8)  & 0xff;
	zdata_frame[ZP2] = (pos >> 16) & 0xff;
	zdata_frame[ZP3] = (pos >> 24) & 0xff;

	zmodem_tx_header(zm, zdata_frame);
	/*
	 * send the data in the file
	 */

	while (!feof(fp)) {
		if (opt_v) {
			show_progress(zm, fp);
		}

		/*
		 * read a block from the file
		 */
		n = fread(zm->tx_data_subpacket,1,sizeof(zm->tx_data_subpacket),fp);

		if (n == 0) {
			/*
			 * nothing to send ?
			 */
			break;
		}

		/*
		 * at end of file wait for an ACK
		 */
		if (ftell(fp) == zm->current_file_size) {
			type = ZCRCW;
		}

		zmodem_tx_data(zm, type, zm->tx_data_subpacket, n);

		if (type == ZCRCW) {
			int type;
			do {
				type = zmodem_rx_header(zm, 10);
				if (type == ZNAK || type == ZRPOS) {
					return type;
				}
			} while (type != ZACK);

			if (ftell(fp) == zm->current_file_size) {
				if (opt_d) {
					fprintf(stderr,"end of file\n");
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
			c = zmodem_rx_raw(zm, 1);
			if (c == ZPAD) {
				type = zmodem_rx_header(zm, 1);
				if (type != TIMEOUT && type != ACK) {
					return type;
				}
			}
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

int
zmodem_send_file(zmodem_t* zm, char* name, FILE* fp)

{
	long pos;
	struct stat s;
	unsigned char * p;
	char zfile_frame[] = { ZFILE, 0, 0, 0, 0 };
	char zeof_frame[] = { ZEOF, 0, 0, 0, 0 };
	int type;

	fstat(fileno(fp),&s);
	zm->current_file_size = s.st_size;

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

	if (zm->management_protect) {
		zfile_frame[ZF1] = ZF1_ZMPROT;		
		if (opt_d) {
			fprintf(stderr,"zmtx: protecting destination\n");
		}
	}

	if (zm->management_clobber) {
		zfile_frame[ZF1] = ZF1_ZMCLOB;
		if (opt_d) {
			fprintf(stderr,"zmtx: overwriting destination\n");
		}
	}

	if (zm->management_newer) {
		zfile_frame[ZF1] = ZF1_ZMNEW;
		if (opt_d) {
			fprintf(stderr,"zmtx: overwriting destination if newer\n");
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

	strcpy(p,name);

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
			type = zmodem_rx_header(zm,10);
		} while (type == ZACK);

		if (opt_d) {
			fprintf(stderr,"type : %d\n",type);
		}

		if (type == ZSKIP) {
			fclose(fp);
			if (opt_v) {
				fprintf(stderr,"zmtx: skipped file \"%s\"                       \n",name);
			}
			return -1;
		}

	} while (type != ZRPOS);

	zm->transfer_start = time(NULL);

	do {
		/*
		 * fetch pos from the ZRPOS header
		 */

		if (type == ZRPOS) {
			pos = zm->rxd_header[ZP0] | (zm->rxd_header[ZP1] << 8) | (zm->rxd_header[ZP2] << 16) | (zm->rxd_header[ZP3] << 24);
		}

		/*
 		 * seek to the right place in the file
		 */
		fseek(fp,pos,0);

		/*
		 * and start sending
		 */

		type = send_from(zm,fp);

		if (type == ZFERR || type == ZABORT) {
 			fclose(fp);
			return -1;
		}

	} while (type == ZRPOS || type == ZNAK);

	/*
	 * file sent. send end of file frame
	 * and wait for zrinit. if it doesnt come then try again
	 */

	zeof_frame[ZP0] =  s.st_size        & 0xff;
	zeof_frame[ZP1] = (s.st_size >> 8)  & 0xff;
	zeof_frame[ZP2] = (s.st_size >> 16) & 0xff;
	zeof_frame[ZP3] = (s.st_size >> 24) & 0xff;

	do {
		zmodem_tx_hex_header(zm,zeof_frame);
		type = zmodem_rx_header(zm,10);
	} while (type != ZRINIT);

	/*
	 * and close the input file
	 */

	if (opt_v) {
		fprintf(stderr,"zmtx: sent file \"%s\"                                    \n",name);
	}

	fclose(fp);

	return 0;
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

	if (opt_v) {
		fprintf(stderr,"zmtx: establishing contact with receiver\n");
	}

	i = 0;
	do {
		unsigned char zrqinit_header[] = { ZRQINIT, 0, 0, 0, 0 };
		i++;
		if (i > 10) {
			fprintf(stderr,"zmtx: can't establish contact with receiver\n");
			bail(3);
		}

		zmodem_tx_raw('r');
		zmodem_tx_raw('z');
		zmodem_tx_raw('\r');
		zmodem_tx_hex_header(zrqinit_header);
	} while (zmodem_rx_header(7) != ZRINIT);

	if (opt_v) {
		fprintf(stderr,"zmtx: contact established\n");
		fprintf(stderr,"zmtx: starting file transfer\n");
	}

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

	zm->use_variable_headers			= (zm->rxd_header[ZF1] & ZF1_CANVHDR) != 0;

	if (opt_d) {
		fprintf(stderr,"receiver %s full duplex\n"          ,zmodem_can_full_duplex               ? "can"      : "can't");
		fprintf(stderr,"receiver %s overlap io\n"           ,zmodem_can_overlap_io                ? "can"      : "can't");
		fprintf(stderr,"receiver %s break\n"                ,zmodem_can_break                     ? "can"      : "can't");
		fprintf(stderr,"receiver %s fcs 32\n"               ,zmodem_can_fcs_32                    ? "can"      : "can't");
		fprintf(stderr,"receiver %s escaped control chars\n",zmodem_escape_all_control_characters ? "requests" : "doesn't request");
		fprintf(stderr,"receiver %s escaped 8th bit\n"      ,zmodem_escape_8th_bit                ? "requests" : "doesn't request");
		fprintf(stderr,"receiver %s use variable headers\n" ,zm->use_variable_headers          ? "can"      : "can't");
	}

	/* 
	 * and send each file in turn
	 */

	n_files_remaining = total_files;

	for(fnum=0;fnum<total_files;fnum++) {
		if(send_file(fname[fnum])) {
			if (opt_v) {
				fprintf(stderr,"zmtx: remote aborted.\n");
			}
			break;
		}
		n_files_remaining--;
	}

	/*
	 * close the session
	 */

	if (opt_v) {
		fprintf(stderr,"zmtx: closing the session\n");
	}

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

		if (type != TIMEOUT) {
			zmodem_tx_raw('O');
			zmodem_tx_raw('O');
		}
	}

	/*
	 * c'est fini
	 */

	if (opt_d) {
		fprintf(stderr,"zmtx: cleanup and exit\n");
	}

	return 0;
}






#endif

char* zmodem_ver(char *buf)
{
	sscanf("$Revision$", "%*s %s", buf);

	return(buf);
}

const char* zmodem_source(void)
{
	return(__FILE__);
}