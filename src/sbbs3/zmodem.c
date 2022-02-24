/* zmodem.c */

/* Synchronet ZMODEM Functions */

/******************************************************************************/
/* Project : Unite!       File : zmodem general        Version : 1.02         */
/*                                                                            */
/* (C) Mattheij Computer Service 1994                                         */
/*
 *	Date: Thu, 19 Nov 2015 10:10:02 +0100
 *	From: Jacques Mattheij
 *	Subject: Re: zmodem license
 *	To: Stephen Hurd, Fernando Toledo
 *	CC: Rob Swindell
 *
 *	Hello there to all of you,
 *
 *	So, this email will then signify as the transfer of any and all rights I
 *	held up to this point with relation to the copyright of the zmodem
 *	package as released by me many years ago and all associated files to
 *	Stephen Hurd. Fernando Toledo and Rob Swindell are named as
 *	witnesses to this transfer.
 *
 *	...
 *
 *	best regards,
 *
 *	Jacques Mattheij
 ******************************************************************************/

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

#define ENDOFFRAME	2
#define FRAMEOK		1
#define TIMEOUT			-1	/* rx routine did not receive a character within timeout */
#define INVHDR			-2	/* invalid header received; but within timeout */
#define ABORTED			-3	/* Aborted *or* disconnected */
#define SUBPKTOVERFLOW  -4	/* Subpacket received more than block length */
#define CRCFAILED		-5	/* Failed CRC comparison */
#define INVALIDSUBPKT	-6	/* Invalid Subpacket Type */
#define ZDLEESC 0x8000	/* one of ZCRCE; ZCRCG; ZCRCQ or ZCRCW was received; ZDLE escaped */

#define BADSUBPKT	0x80

#define SEND_SUCCESS	0

#define HDRLEN     5	/* size of a zmodem header */

#define STRIPPED_PARITY(c)	((c) & 0x7f)

static int lprintf(zmodem_t* zm, int level, const char *fmt, ...)
{
	va_list argptr;
	char	sbuf[1024];

	if(zm->lputs==NULL)
		return(-1);
	if(zm->log_level != NULL)
		if(level > *zm->log_level)
			return 0;

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

static BOOL is_data_waiting(zmodem_t* zm, unsigned timeout)
{
	if(zm->data_waiting)
		return(zm->data_waiting(zm->cbdata, timeout));
	return(FALSE);
}

static char *chr(int ch)
{
	static char str[25];

	switch(ch) {
		case TIMEOUT:			return("TIMEOUT");
		case ABORTED:			return("ABORTED");
		case SUBPKTOVERFLOW:	return "Subpacket Overflow";
		case CRCFAILED:			return "CRC ERROR";
		case INVALIDSUBPKT:		return "Invalid Subpacket";
		case ZRQINIT:			return("ZRQINIT");
		case ZRINIT:			return("ZRINIT");
		case ZSINIT:			return("ZSINIT");
		case ZACK:				return("ZACK");
		case ZFILE:				return("ZFILE");
		case ZSKIP:				return("ZSKIP");
		case ZCRC:				return("ZCRC");
		case ZNAK:				return("ZNAK");
		case ZABORT:			return("ZABORT");
		case ZFIN:				return("ZFIN");
		case ZRPOS:				return("ZRPOS");
		case ZDATA:				return("ZDATA");
		case ZEOF:				return("ZEOF");
		case ZFERR:				return("ZFERR");
		case ZPAD:				return("ZPAD");
		case ZCAN:				return("ZCAN");
		case ZDLE:				return("ZDLE");
		case ZDLEE:				return("ZDLEE");
		case ZBIN:				return("ZBIN");
		case ZHEX:				return("ZHEX");
		case ZBIN32:			return("ZBIN32");
		case ZRESC:				return("ZRESC");
		case ZCRCE:				return("ZCRCE");
		case ZCRCG:				return("ZCRCG");
		case ZCRCQ:				return("ZCRCQ");
		case ZCRCW:				return("ZCRCW");
		case XON:				return "XON";
		case XOFF:				return "XOFF";
	}
	if(ch<0)
		sprintf(str,"%d",ch);
	else if(ch>=' ' && ch<='~')
		sprintf(str,"'%c' (%02Xh)",(uchar)ch,(uchar)ch);
	else
		sprintf(str,"%u (%02Xh)",(uchar)ch,(uchar)ch);
	return(str);
}

static char* frame_desc(int frame)
{
	static char str[25];

	if(frame==TIMEOUT)
		return "TIMEOUT";

	if(frame==INVHDR)
		return "Invalid Header";

	if(frame==ABORTED)
		return "Aborted";

	if(frame >= 0 && (frame&BADSUBPKT)) {
		strcpy(str,"BAD ");
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
	} else
		return chr(frame);
	return(str);
}

ulong frame_pos(zmodem_t* zm, int type)
{
	switch(type) {
		case ZRPOS:
		case ZACK:
		case ZEOF:
		case ZDATA:
			return(zm->rxd_header_pos);
	}

	return 0;
}

/*
 * read bytes as long as rdchk indicates that
 * more data is available.
 */

void zmodem_recv_purge(zmodem_t* zm, unsigned timeout)
{
	int c;

	while((c = zm->recv_byte(zm->cbdata, timeout)) >= 0) {
		lprintf(zm, LOG_DEBUG, "Purging data in receive buffer: %s", chr(c));
	}
}

/*
 * Flush the output buffer
 */
void zmodem_flush(zmodem_t* zm)
{
	if(zm->flush!=NULL)
		zm->flush(zm);
}

/*
 * transmit a character.
 * this is the raw modem interface
 */
/* Returns 0 on success */
int zmodem_send_raw(zmodem_t* zm, unsigned char ch)
{
	int	result;

	if((result = zm->send_byte(zm->cbdata, ch, zm->send_timeout)) != SEND_SUCCESS)
		lprintf(zm, LOG_ERR, "%s ERROR: %d", __FUNCTION__, result);
	else
		zm->last_sent = ch;

	return result;
}

/*
 * transmit a character ZDLE escaped
 */

int zmodem_send_esc(zmodem_t* zm, unsigned char c)
{
	int	result;

	if((result = zmodem_send_raw(zm, ZDLE)) != SEND_SUCCESS) {
		lprintf(zm, LOG_ERR, "%s ERROR: %d", __FUNCTION__, result);
		return result;
	}
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
				if((result=zmodem_send_raw(zm, ZDLE)) != SEND_SUCCESS)
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

//	lprintf(zm, LOG_DEBUG, __FUNCTION__ " %02X",val);

	if((result=zmodem_send_raw(zm, xdigit[val>>4])) != SEND_SUCCESS)
		return result;
	return zmodem_send_raw(zm, xdigit[val&0xf]);
}

int zmodem_send_padded_zdle(zmodem_t* zm)
{
	int result;

	if((result=zmodem_send_raw(zm, ZPAD)) != SEND_SUCCESS)
		return result;
	if((result=zmodem_send_raw(zm, ZPAD)) != SEND_SUCCESS)
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

//	lprintf(zm, LOG_DEBUG, __FUNCTION__ " %s", chr(type));

	if((result=zmodem_send_padded_zdle(zm)) != SEND_SUCCESS)
		return result;

	if((result=zmodem_send_raw(zm, ZHEX)) != SEND_SUCCESS)
		return result;

	/*
 	 * initialise the crc
	 */

	crc = 0;

	/*
 	 * transmit the header
	 */

	for(i=0;i<HDRLEN;i++) {
		if((result=zmodem_send_hex(zm, *p)) != SEND_SUCCESS)
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

	if((result=zmodem_send_hex(zm, (uchar)(crc>>8))) != SEND_SUCCESS)
		return result;
	if((result=zmodem_send_hex(zm, (uchar)(crc&0xff))) != SEND_SUCCESS)
		return result;

	/*
	 * end of line sequence
	 */

	if((result=zmodem_send_raw(zm, '\r')) != SEND_SUCCESS)
		return result;
	if((result=zmodem_send_raw(zm, '\n')) != SEND_SUCCESS)	/* FDSZ sends 0x8a instead of 0x0a */
		return result;

	if(type!=ZACK && type!=ZFIN)
		result=zmodem_send_raw(zm, XON);

	zmodem_flush(zm);

	return(result);
}

/*
 * Send ZMODEM binary header hdr
 */

int zmodem_send_bin32_header(zmodem_t* zm, unsigned char * p)
{
	int i;
	int result;
	uint32_t crc;

//	lprintf(zm, LOG_DEBUG, __FUNCTION__ " %s", chr(*p));

	if((result=zmodem_send_padded_zdle(zm)) != SEND_SUCCESS)
		return result;

	if((result=zmodem_send_raw(zm, ZBIN32)) != SEND_SUCCESS)
		return result;

	crc = 0xffffffffL;

	for(i=0;i<HDRLEN;i++) {
		crc = ucrc32(*p,crc);
		if((result=zmodem_tx(zm, *p++)) != SEND_SUCCESS)
			return result;
	}

	crc = ~crc;

	if((result=	zmodem_tx(zm, (uchar)((crc      ) & 0xff))) != SEND_SUCCESS)
		return result;
	if((result=	zmodem_tx(zm, (uchar)((crc >>  8) & 0xff))) != SEND_SUCCESS)
		return result;
	if((result=	zmodem_tx(zm, (uchar)((crc >> 16) & 0xff))) != SEND_SUCCESS)
		return result;
	return		zmodem_tx(zm, (uchar)((crc >> 24) & 0xff));
}

int zmodem_send_bin16_header(zmodem_t* zm, unsigned char * p)
{
	int i;
	int result;
	unsigned int crc;

//	lprintf(zm, LOG_DEBUG, __FUNCTION__ " %s", chr(*p));

	if((result=zmodem_send_padded_zdle(zm)) != SEND_SUCCESS)
		return result;

	if((result=zmodem_send_raw(zm, ZBIN)) != SEND_SUCCESS)
		return result;

	crc = 0;

	for(i=0;i<HDRLEN;i++) {
		crc = ucrc16(*p,crc);
		if((result=zmodem_tx(zm, *p++)) != SEND_SUCCESS)
			return result;
	}

	if((result=	zmodem_tx(zm, (uchar)(crc >> 8))) != SEND_SUCCESS)
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
	uint32_t crc;

//	lprintf(zm, LOG_DEBUG, __FUNCTION__ " %s (%u bytes)", chr(subpkt_type), l);

	crc = 0xffffffffl;

	while(l > 0) {
		crc = ucrc32(*p,crc);
		if((result=zmodem_tx(zm, *p++)) != SEND_SUCCESS)
			return result;
		l--;
	}

	crc = ucrc32(subpkt_type, crc);

	if((result=zmodem_send_raw(zm, ZDLE)) != SEND_SUCCESS)
		return result;
	if((result=zmodem_send_raw(zm, subpkt_type)) != SEND_SUCCESS)
		return result;

	crc = ~crc;

	if((result=	zmodem_tx(zm, (uchar) ((crc      ) & 0xff))) != SEND_SUCCESS)
		return result;
	if((result=	zmodem_tx(zm, (uchar) ((crc >> 8 ) & 0xff))) != SEND_SUCCESS)
		return result;
	if((result=	zmodem_tx(zm, (uchar) ((crc >> 16) & 0xff))) != SEND_SUCCESS)
		return result;
	return		zmodem_tx(zm, (uchar) ((crc >> 24) & 0xff));
}

int zmodem_send_data16(zmodem_t* zm, uchar subpkt_type,unsigned char * p, size_t l)
{
	int	result;
	unsigned short crc;

//	lprintf(zm, LOG_DEBUG, __FUNCTION__ " %s (%u bytes)", chr(subpkt_type), l);

	crc = 0;

	while(l > 0) {
		crc = ucrc16(*p,crc);
		if((result=zmodem_tx(zm, *p++)) != SEND_SUCCESS)
			return result;
		l--;
	}

	crc = ucrc16(subpkt_type,crc);

	if((result=zmodem_send_raw(zm, ZDLE)) != SEND_SUCCESS)
		return result;
	if((result=zmodem_send_raw(zm, subpkt_type)) != SEND_SUCCESS)
		return result;

	if((result=	zmodem_tx(zm, (uchar)(crc >> 8))) != SEND_SUCCESS)
		return result;
	return		zmodem_tx(zm, (uchar)(crc&0xff));
}

BOOL zmodem_end_of_frame(int subpkt_type)
{
	return subpkt_type == ZCRCW || subpkt_type == ZCRCE;
}

/*
 * send a data subpacket using crc 16 or crc 32 as desired by the receiver
 */

int zmodem_send_data_subpkt(zmodem_t* zm, uchar subpkt_type, unsigned char* data, size_t len)
{
	int result;

	if(zmodem_end_of_frame(subpkt_type))
		zm->frame_in_transit=FALSE;
	else	/* other subpacket (mid-frame) */
		zm->frame_in_transit=TRUE;

	if(!zm->want_fcs_16 && zm->can_fcs_32) {
		if((result=zmodem_send_data32(zm, subpkt_type, data, len)) != SEND_SUCCESS)
			return result;
	}
	else {
		if((result=zmodem_send_data16(zm, subpkt_type, data, len)) != SEND_SUCCESS)
			return result;
	}

	if(subpkt_type == ZCRCW)
		result=zmodem_send_raw(zm, XON);

	zmodem_flush(zm);

	return result;
}

int zmodem_send_data(zmodem_t* zm, uchar subpkt_type, unsigned char* data, size_t len)
{
	if(!zm->frame_in_transit)	{ /* Start of frame, include ZDATA header */
		lprintf(zm, LOG_DEBUG, "%lu %s Start of frame: %s"
			,(ulong)zm->current_file_pos, __FUNCTION__, chr(subpkt_type));
		zmodem_send_pos_header(zm, ZDATA, (uint32_t)zm->current_file_pos, /* Hex? */ FALSE);
	}

	return zmodem_send_data_subpkt(zm, subpkt_type, data, len);
}

int zmodem_send_pos_header(zmodem_t* zm, int type, int32_t pos, BOOL hex)
{
	uchar header[5];

	lprintf(zm, LOG_DEBUG, "%lu %s %s", (ulong)pos, __FUNCTION__, chr(type));
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

int zmodem_send_ack(zmodem_t* zm, int32_t pos)
{
	return zmodem_send_pos_header(zm, ZACK, pos, /* Hex? */ TRUE);
}

int zmodem_send_zfin(zmodem_t* zm)
{
	unsigned char zfin_header[] = { ZFIN, 0, 0, 0, 0 };

	lprintf(zm, LOG_NOTICE, "%lu Finishing Session (Sending ZFIN)", (ulong)zm->current_file_pos);
	return zmodem_send_hex_header(zm,zfin_header);
}

int zmodem_send_zabort(zmodem_t* zm)
{
	lprintf(zm, LOG_WARNING, "%lu Aborting Transfer (Sending ZABORT)", (ulong)zm->current_file_pos);
	return zmodem_send_pos_header(zm, ZABORT, 0, /* Hex? */ TRUE);
}

int zmodem_send_znak(zmodem_t* zm)
{
	lprintf(zm, LOG_INFO, "%lu Sending ZNAK", (ulong)zm->current_file_pos);
	return zmodem_send_pos_header(zm, ZNAK, 0, /* Hex? */ TRUE);
}

int zmodem_send_zskip(zmodem_t* zm)
{
	lprintf(zm, LOG_INFO, "%lu Sending ZSKIP", (ulong)zm->current_file_pos);
	return zmodem_send_pos_header(zm, ZSKIP, 0L, /* Hex? */ TRUE);
}

int zmodem_send_zeof(zmodem_t* zm)
{
	lprintf(zm, LOG_INFO, "%lu Sending End-of-File (ZEOF) frame", (ulong)zm->current_file_pos);
	return zmodem_send_pos_header(zm, ZEOF, (int32_t)zm->current_file_pos, /* Hex? */ TRUE);
}


/*
 * rx_raw ; receive a single byte from the line.
 * reads as many are available and then processes them one at a time
 * check the data stream for 5 consecutive CAN characters;
 * and if you see them abort. this saves a lot of clutter in
 * the rest of the code; even though it is a very strange place
 * for an exit. (but that was what session abort was all about.)
 */

int zmodem_recv_raw(zmodem_t* zm)
{
	int c;
	unsigned attempt;

	for(attempt=0; attempt < zm->recv_timeout; attempt++) {
		if((c=zm->recv_byte(zm->cbdata,1 /* second timeout */)) >= 0)
			break;
		if(is_cancelled(zm))
			return(ZCAN);
		if(!is_connected(zm))
			return(ABORTED);
		lprintf(zm, LOG_DEBUG, "%s Received NO INPUT (attempt %u of %u)"
			,__FUNCTION__, attempt + 1, zm->recv_timeout);
	}
	if(attempt >= zm->recv_timeout) {
		lprintf(zm, LOG_WARNING, "%lu %s TIMEOUT (%u seconds)"
			,(ulong)zm->current_file_pos, __FUNCTION__, attempt);
		return TIMEOUT;
	}

	if(c == CAN) {
		zm->n_cans++;
		if(zm->n_cans == 5) {
			zm->cancelled=TRUE;
			lprintf(zm, LOG_WARNING, "%lu Canceled remotely", (ulong)zm->current_file_pos);
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

	while(is_connected(zm) && !is_cancelled(zm)) {

		do {
			switch(c = zmodem_recv_raw(zm)) {
				case ZDLE:
					break;
				case XON:
				case XON|0x80:
				case XOFF:
				case XOFF|0x80:
					lprintf(zm,LOG_WARNING, "%lu Dropping flow ctrl char: %s"
						,(ulong)zm->current_file_pos, chr(c));
					continue;
				default:
					if(c < 0)
						return c;
					/*
	 				 * if all control characters should be escaped and
					 * this one wasn't then its spurious and should be dropped.
					 */
					if(zm->escape_ctrl_chars && (c >= 0) && (c & 0x60) == 0) {
						lprintf(zm,LOG_WARNING, "%lu Dropping unescaped ctrl char: %s"
							,(ulong)zm->current_file_pos, chr(c));
						continue;
					}
					/*
					 * normal character; return it.
					 */
					return c;
			}
			break;
		} while(!is_cancelled(zm));

		/*
	 	 * ZDLE encoded sequence or session abort.
		 * (or something illegal; then back to the top)
		 */

		while(!is_cancelled(zm)) {

			switch(c=zmodem_recv_raw(zm)) {
				case XON:
				case XON|0x80:
				case XOFF:
				case XOFF|0x80:
				case ZDLE:
					lprintf(zm,LOG_WARNING, "%lu Dropping escaped flow ctrl char: %s"
						,(ulong)zm->current_file_pos, chr(c));
					continue;
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
//					lprintf(zm,LOG_DEBUG, "%lu Encoding data subpacket type: %s", (ulong)zm->current_file_pos, chr(c));
					return (c | ZDLEESC);
				case ZRUB0:
					return 0x7f;
				case ZRUB1:
					return 0xff;
				default:
					if(c < 0)
						return c;

					if(zm->escape_ctrl_chars && (c & 0x60) == 0) {
						/*
						 * a not escaped control character; probably
						 * something from a network. just drop it.
						 */
						lprintf(zm,LOG_WARNING, "%lu Dropping unescaped ctrl char: %s"
							,(ulong)zm->current_file_pos, chr(c));
						continue;
					}
					/*
					 * legitimate escape sequence.
					 * rebuild the original and return it.
					 */
					if((c & 0x60) == 0x40) {
						return c ^ 0x40;
					}
					lprintf(zm,LOG_WARNING, "%lu Illegal sequence: ZDLE %s"
						,(ulong)zm->current_file_pos, chr(c));
					break;
			}
			break;
		}
	}

	/*
	 * not reached (unless canceled).
	 */

	return ABORTED;
}

/*
 * receive a data subpacket as dictated by the last received header.
 * return 2 with correct packet and end of frame
 * return 1 with correct packet frame continues
 * return 0 with incorrect frame.
 * return TIMEOUT with a timeout
 * if an acknowledgment is requested it is generated automatically
 * here.
 */

/*
 * data subpacket reception
 */

int zmodem_recv_data32(zmodem_t* zm, unsigned char * p, unsigned maxlen, unsigned* len, int* type)
{
	int c;
	uint32_t rxd_crc;
	uint32_t crc;
	int subpkt_type;

//	lprintf(zm,LOG_DEBUG, __FUNCTION___);

	*type = INVALIDSUBPKT;
	crc = 0xffffffffl;

	do {
		c = zmodem_rx(zm);

		if(c < 0)
			return c;

		if(c > 0xff)
			break;

		if(*len >= maxlen) {
			lprintf(zm, LOG_ERR, "%lu Subpacket OVERFLOW (%u > %u)",(ulong)zm->ack_file_pos, *len, maxlen);
			return SUBPKTOVERFLOW;
		}
		crc = ucrc32(c,crc);
		*p++ = c;
		(*len)++;
	} while(1);

	subpkt_type = c & 0xff;
	*type = subpkt_type;

	crc = ucrc32(subpkt_type, crc);

	crc = ~crc;

	rxd_crc  = zmodem_rx(zm);
	rxd_crc |= zmodem_rx(zm) << 8;
	rxd_crc |= zmodem_rx(zm) << 16;
	rxd_crc |= zmodem_rx(zm) << 24;

	if(rxd_crc != crc) {
		lprintf(zm, LOG_DEBUG, "%lu %s CRC ERROR (%08X, expected: %08X) Bytes=%u, subpacket type=%s"
			,(ulong)zm->ack_file_pos, __FUNCTION__, rxd_crc, crc, *len, chr(subpkt_type));
		return CRCFAILED;
	}
//	lprintf(zm,LOG_DEBUG, "%lu %s GOOD CRC: %08lX (Bytes=%u, subpacket type=%s)"
//		,(ulong)zm->ack_file_pos, __FUNCTION__, crc, *len, chr(subpkt_type));

	zm->ack_file_pos += *len;

	return subpkt_type;
}

int zmodem_recv_data16(zmodem_t* zm, register unsigned char* p, unsigned maxlen, unsigned* len, int* type)
{
	int c;
	int subpkt_type;
 	unsigned short crc;
	unsigned short rxd_crc;

//	lprintf(zm, LOG_DEBUG, __FUNCTION__);

	crc = 0;
	*type = INVALIDSUBPKT;

	do {
		c = zmodem_rx(zm);

		if(c < 0)
			return c;

		if(c > 0xff)
			break;

		if(*len >= maxlen)
			return SUBPKTOVERFLOW;
		crc = ucrc16(c,crc);
		*p++ = c;
		(*len)++;
	} while(1);

	subpkt_type = c & 0xff;
	*type = subpkt_type;

	crc = ucrc16(subpkt_type,crc);

	rxd_crc  = zmodem_rx(zm) << 8;
	rxd_crc |= zmodem_rx(zm);

	if(rxd_crc != crc) {
		lprintf(zm, LOG_DEBUG, "%lu %s CRC ERROR (%04hX, expected: %04hX) Bytes=%u, subpacket type=%s"
			,(ulong)zm->ack_file_pos, __FUNCTION__, rxd_crc, crc, *len, chr(subpkt_type));
		return CRCFAILED;
	}
//	lprintf(zm,LOG_DEBUG, "%lu %s GOOD CRC: %04hX (Bytes=%d, subpacket type=%s)"
//		,(ulong)zm->ack_file_pos, __FUNCTION__, crc, *len, chr(subpkt_type));

	zm->ack_file_pos += *len;

	return subpkt_type;
}

int zmodem_recv_data(zmodem_t* zm, unsigned char* buf, size_t maxlen, unsigned* len, BOOL ack, int* type)
{
	int subpkt_type;
	unsigned n=0;

	if(len == NULL)
		len = &n;

//	lprintf(zm,LOG_DEBUG, __FUNCTION__ " (%u-bit)", zm->receive_32bit_data ? 32:16);

	/*
	 * receive the right type of frame
	 */

	*len = 0;

	if(zm->receive_32bit_data) {
		subpkt_type = zmodem_recv_data32(zm, buf, maxlen, len, type);
	}
	else {
		subpkt_type = zmodem_recv_data16(zm, buf, maxlen, len, type);
	}

	if(subpkt_type <= 0) {	/* e.g. TIMEOUT, SUBPKTOVERFLOW, CRCFAILED */
		lprintf(zm, LOG_WARNING, "%s data subpacket (%u bytes) %s"
			,chr(*type), *len, chr(subpkt_type));
		return(subpkt_type);
	}

	lprintf(zm, LOG_DEBUG, "%lu Successful receipt of subpacket type: %s (%u bytes)"
		,(ulong)zm->ack_file_pos, chr(subpkt_type), *len);

	switch(subpkt_type)  {
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

	lprintf(zm,LOG_WARNING, "%lu INVALID subpacket type: %s (%u bytes)"
		,(ulong)zm->ack_file_pos, chr(subpkt_type), *len);

	return INVALIDSUBPKT;
}

BOOL zmodem_recv_subpacket(zmodem_t* zm, BOOL ack, int* type)
{
	int	result;
	unsigned len = 0;

	result = zmodem_recv_data(zm,zm->rx_data_subpacket, sizeof(zm->rx_data_subpacket), &len, ack, type);
	if(result != FRAMEOK && result != ENDOFFRAME) {
		lprintf(zm, LOG_DEBUG, "%lu %s ERROR: %s (subpacket type: %s, %u bytes)"
			,(ulong)zm->ack_file_pos, __FUNCTION__, chr(result), chr(*type), len);
		zmodem_send_znak(zm);
		return(FALSE);
	}

	return(TRUE);
}

int zmodem_recv_nibble(zmodem_t* zm)
{
	int c;

	c = zmodem_rx(zm);

	if(c < 0)
		return c;

	c = STRIPPED_PARITY(c);
	if(c > '9') {
		if(c < 'a' || c > 'f') {
			/*
			 * illegal hex; different than expected.
			 * we might as well time out.
			 */
			return -1;
		}

		c -= 'a' - 10;
	}
	else {
		if(c < '0') {
			/*
			 * illegal hex; different than expected.
			 * we might as well time out.
			 */
			return -1;
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

	if(n1 < 0)
		return n1;

	n0 = zmodem_recv_nibble(zm);

	if(n0 < 0)
		return n0;

	ret = (n1 << 4) | n0;

//	lprintf(zm,LOG_DEBUG, __FUNCTION__ " returning: 0x%02X", ret);

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

//	lprintf(zm ,LOG_DEBUG, __FUNCTION__);

	crc = 0;

	for(n=0;n<HDRLEN;n++) {
		c = zmodem_rx(zm);
		if(c < 0) {
			lprintf(zm, LOG_WARNING, "%lu %s ERROR: %s"
				,(ulong)zm->current_file_pos, __FUNCTION__, chr(c));
			return(FALSE);
		}
		crc = ucrc16(c,crc);
		zm->rxd_header[n] = c;
	}

	rxd_crc  = zmodem_rx(zm) << 8;
	rxd_crc |= zmodem_rx(zm);

	if(rxd_crc != crc) {
		lprintf(zm,LOG_WARNING, "%lu %s CRC ERROR: 0x%hX, expected: 0x%hX"
			,(ulong)zm->ack_file_pos, __FUNCTION__, rxd_crc, crc);
		return(FALSE);
	}
//	lprintf(zm,LOG_DEBUG, "%lu %s GOOD CRC: %04hX", __FUNCTION__
//		,(ulong)zm->ack_file_pos, __FUNCTION__, crc);

	zm->rxd_header_len = n;

	return(TRUE);
}

BOOL zmodem_recv_hex_header(zmodem_t* zm)
{
	int c;
	int i;
	unsigned short int crc = 0;
	unsigned short int rxd_crc;

//	lprintf(zm, LOG_DEBUG, __FUNCTION__);

	for(i=0;i<HDRLEN;i++) {
		c = zmodem_recv_hex(zm);
		if(c < 0 )
			return FALSE;
		crc = ucrc16(c,crc);

		zm->rxd_header[i] = c;
	}

	/*
	 * receive the crc
	 */

	c = zmodem_recv_hex(zm);

	if(c < 0)
		return FALSE;

	rxd_crc = c << 8;

	c = zmodem_recv_hex(zm);

	if(c < 0 )
		return FALSE;

	rxd_crc |= c;

	if(rxd_crc == crc) {
//		lprintf(zm,LOG_DEBUG, "%s GOOD CRC: %04hX", __FUNCTION__, crc);
		zm->rxd_header_len = i;
	}
	else {
		lprintf(zm,LOG_WARNING, "%s CRC ERROR: 0x%hX, expected: 0x%hX"
			,__FUNCTION__, rxd_crc, crc);
		return FALSE;
	}

	/*
	 * drop the end of line sequence after a hex header
	 */
	c = zmodem_rx(zm);
	if(c == '\r') {
		/*
		 * both are expected with CR
		 */
		c = zmodem_rx(zm);	/* drop LF */
	}
	if(c != '\n' && c != 0x8A) {
		lprintf(zm, LOG_ERR, "%s HEX header not terminated with LF: %s"
			,__FUNCTION__, chr(c));
		return FALSE;
	}

	return TRUE;
}

BOOL zmodem_recv_bin32_header(zmodem_t* zm)
{
	int c;
	int n;
	uint32_t crc;
	uint32_t rxd_crc;

//	lprintf(zm,LOG_DEBUG,"recv_bin32_header");

	crc = 0xffffffffL;

	for(n=0;n<HDRLEN;n++) {
		c = zmodem_rx(zm);
		if(c < 0)
			return(TRUE);
		crc = ucrc32(c,crc);
		zm->rxd_header[n] = c;
	}

	crc = ~crc;

	rxd_crc  = zmodem_rx(zm);
	rxd_crc |= zmodem_rx(zm) << 8;
	rxd_crc |= zmodem_rx(zm) << 16;
	rxd_crc |= zmodem_rx(zm) << 24;

	if(rxd_crc != crc) {
		lprintf(zm,LOG_WARNING, "%lu %s CRC ERROR (%08X, expected: %08X)"
			,(ulong)zm->ack_file_pos, __FUNCTION__, rxd_crc, crc);
		return(FALSE);
	}
//	lprintf(zm,LOG_DEBUG, "%lu %s GOOD CRC: %08lX", (ulong)zm->ack_file_pos, __FUNCTION__, crc);

	zm->rxd_header_len = n;
	return(TRUE);
}

/*
 * receive any style header
 * a flag (receive_32bit_data) will be set to indicate whether data
 * packets following this header will have 16 or 32 bit data attached.
 * variable headers are not implemented.
 */

int zmodem_recv_header_raw(zmodem_t* zm)
{
	int c;
	int type = INVALIDSUBPKT;
	int	frame_type;

//	lprintf(zm,LOG_DEBUG, __FUNCTION__);

	zm->rxd_header_len = 0;

	do {
		do {
			if((c = zmodem_recv_raw(zm)) < 0)
				return(c);
			if(is_cancelled(zm))
				return(ZCAN);
		} while(STRIPPED_PARITY(c) != ZPAD);

		if((c = zmodem_recv_raw(zm)) < 0)
			return(c);

		if(STRIPPED_PARITY(c) == ZPAD) {
			if((c = zmodem_recv_raw(zm)) < 0)
				return(c);
		}

		/*
		 * spurious ZPAD check
		 */

		if(STRIPPED_PARITY(c) != ZDLE) {
			lprintf(zm,LOG_DEBUG, "%s Expected ZDLE, received: %s", __FUNCTION__, chr(c));
			continue;
		}

		/*
		 * now read the header style
		 */

		c = zmodem_rx(zm);

		switch (STRIPPED_PARITY(c)) {
			case ZBIN:
				if(!zmodem_recv_bin16_header(zm))
					return INVHDR;
				zm->receive_32bit_data = FALSE;
				break;
			case ZHEX:
				if(!zmodem_recv_hex_header(zm))
					return INVHDR;
				zm->receive_32bit_data = FALSE;
				break;
			case ZBIN32:
				if(!zmodem_recv_bin32_header(zm))
					return INVHDR;
				zm->receive_32bit_data = TRUE;
				break;
			default:
				if(c < 0) {
					lprintf(zm,LOG_WARNING, "%lu %s ERROR: %s"
						,(ulong)zm->current_file_pos, __FUNCTION__, chr(c));
					return c;
				}
				/*
				 * unrecognized header style
				 */
				lprintf(zm,LOG_ERR, "%lu %s UNRECOGNIZED header: %s"
					,(ulong)zm->current_file_pos, __FUNCTION__, chr(c));
				continue;
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
			if(!zmodem_recv_subpacket(zm,/* ack? */FALSE, &type)) {
				lprintf(zm, LOG_WARNING, "%s bad %s subpacket: %s", __FUNCTION__, chr(frame_type), chr(type));
				frame_type |= BADSUBPKT;
			}
			break;
		case ZSINIT:
		case ZCOMMAND:
			if(!zmodem_recv_subpacket(zm,/* ack? */TRUE, &type)) {
				lprintf(zm, LOG_WARNING, "%s bad %s subpacket: %s", __FUNCTION__, chr(frame_type), chr(type));
				frame_type |= BADSUBPKT;
			}
			break;
		case ZFREECNT:
			zmodem_send_pos_header(zm, ZACK, getfreediskspace(".",1), /* Hex? */ TRUE);
			break;
	}

#if 0 /* def _DEBUG */
	lprintf(zm,LOG_DEBUG, __FUNCTION__ " received header type: %s"
		,frame_desc(frame_type));
#endif

	return frame_type;
}

int zmodem_recv_header(zmodem_t* zm)
{
	int ret;

	switch(ret = zmodem_recv_header_raw(zm)) {
		case TIMEOUT:
			lprintf(zm,LOG_WARNING, "%s TIMEOUT", __FUNCTION__);
			break;
		case INVHDR:
			lprintf(zm,LOG_WARNING, "%s detected an INVALID HEADER", __FUNCTION__);
			break;
		default:
			lprintf(zm,LOG_DEBUG, "%lu %s frame: %s"
				,(ulong)frame_pos(zm, ret), __FUNCTION__, frame_desc(ret));
			if(ret==ZCAN)
				zm->cancelled=TRUE;
			else if(ret==ZRINIT)
				zmodem_parse_zrinit(zm);
			break;
	}

	return ret;
}

BOOL zmodem_request_crc(zmodem_t* zm, int32_t length)
{
	zmodem_recv_purge(zm, /* timeout: */0);
	zmodem_send_pos_header(zm, ZCRC, length, /* HEX: */TRUE);
	return TRUE;
}

BOOL zmodem_recv_crc(zmodem_t* zm, uint32_t* crc)
{
	int type;

	if(!is_data_waiting(zm,zm->crc_timeout)) {
		lprintf(zm,LOG_ERR, "%lu %s Timeout waiting for response (%u seconds)"
			,(ulong)zm->current_file_pos, __FUNCTION__, zm->crc_timeout);
		return(FALSE);
	}
	if((type=zmodem_recv_header(zm))!=ZCRC) {
		lprintf(zm,LOG_ERR, "%lu %s Received %s instead of ZCRC"
			,(ulong)zm->current_file_pos, __FUNCTION__, frame_desc(type));
		return(FALSE);
	}
	if(crc!=NULL)
		*crc = zm->crc_request;
	return TRUE;
}

BOOL zmodem_get_crc(zmodem_t* zm, int32_t length, uint32_t* crc)
{
	if(zmodem_request_crc(zm, length))
		return zmodem_recv_crc(zm, crc);
	return FALSE;
}

void zmodem_parse_zrinit(zmodem_t* zm)
{
	zm->can_full_duplex					= INT_TO_BOOL(zm->rxd_header[ZF0] & ZF0_CANFDX);
	zm->can_overlap_io					= INT_TO_BOOL(zm->rxd_header[ZF0] & ZF0_CANOVIO);
	zm->can_break						= INT_TO_BOOL(zm->rxd_header[ZF0] & ZF0_CANBRK);
	zm->can_fcs_32						= INT_TO_BOOL(zm->rxd_header[ZF0] & ZF0_CANFC32);
	zm->escape_ctrl_chars				= INT_TO_BOOL(zm->rxd_header[ZF0] & ZF0_ESCCTL);
	zm->escape_8th_bit					= INT_TO_BOOL(zm->rxd_header[ZF0] & ZF0_ESC8);

	lprintf(zm, LOG_INFO, "Receiver requested mode (0x%02X):\r\n"
		"%s-duplex, %s overlap I/O, CRC-%u, Escape: %s"
		,zm->rxd_header[ZF0]
		,zm->can_full_duplex ? "Full" : "Half"
		,zm->can_overlap_io ? "Can" : "Cannot"
		,zm->can_fcs_32 ? 32 : 16
		,zm->escape_ctrl_chars ? "ALL" : "Normal"
		);

	if((zm->recv_bufsize = (zm->rxd_header[ZP0] | zm->rxd_header[ZP1]<<8)) != 0)
		lprintf(zm, LOG_INFO, "Receiver specified buffer size of: %u", zm->recv_bufsize);
}

int zmodem_get_zrinit(zmodem_t* zm)
{
	unsigned char zrqinit_header[] = { ZRQINIT, /* ZF3: */0, 0, 0, /* ZF0: */0 };
	/* Note: sz/dsz/fdsz sends 0x80 in ZF3 because it supports var-length headers. */
	/* We do not, so we send 0x00, resulting in a CRC-16 value of 0x0000 as well. */

	lprintf(zm, LOG_DEBUG, __FUNCTION__);
	zmodem_send_raw(zm,'r');
	zmodem_send_raw(zm,'z');
	zmodem_send_raw(zm,'\r');
	zmodem_send_hex_header(zm,zrqinit_header);

	if(!is_data_waiting(zm,zm->init_timeout))
		return(TIMEOUT);
	return zmodem_recv_header(zm);
}

int zmodem_send_zrinit(zmodem_t* zm)
{
	unsigned char zrinit_header[] = { ZRINIT, 0, 0, 0, 0 };

	if(zm->can_full_duplex)
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
		zm->recv_bufsize = zm->max_block_size;

	zrinit_header[ZP0] = zm->recv_bufsize & 0xff;
	zrinit_header[ZP1] = zm->recv_bufsize >> 8;

	return zmodem_send_hex_header(zm, zrinit_header);
}

/* Returns ZFIN on success */
int zmodem_get_zfin(zmodem_t* zm)
{
	int result;
	int type=ZCAN;
	unsigned attempts;

	lprintf(zm, LOG_DEBUG, __FUNCTION__);

	for(attempts=0; attempts<zm->max_errors && is_connected(zm) && !is_cancelled(zm); attempts++) {
		if(attempts&1)	/* Alternate between ZABORT and ZFIN */
			result = zmodem_send_zabort(zm);
		else
			result = zmodem_send_zfin(zm);
		if(result != SEND_SUCCESS)
			return result;
		if((type = zmodem_recv_header(zm)) == ZFIN)
			break;
	}

	/*
	 * these Os are formally required; but they don't do a thing
	 * unfortunately many programs require them to exit
	 * (both programs already sent a ZFIN so why bother ?)
	 */

	if(type == ZFIN) {
		zmodem_send_raw(zm,'O');
		zmodem_send_raw(zm,'O');
	}

	return type;
}

BOOL zmodem_handle_zrpos(zmodem_t* zm, uint64_t* pos)
{
	if(zm->rxd_header_pos < zm->current_file_size) {
		if(*pos != zm->rxd_header_pos) {
			*pos = zm->rxd_header_pos;
			zm->ack_file_pos = (int32_t)*pos;
			lprintf(zm, LOG_INFO, "%lu Resuming transfer from offset: %"PRIu64
				,(ulong)zm->current_file_pos, *pos);
		}
		zmodem_recv_purge(zm, /* timeout: */1);
		return TRUE;
	}
	lprintf(zm, LOG_WARNING, "%lu Received INVALID ZRPOS offset: %u"
		,(ulong)zm->current_file_pos, zm->rxd_header_pos);
	return FALSE;
}

BOOL zmodem_handle_zack(zmodem_t* zm, uint32_t min, uint32_t max)
{
	if(zm->rxd_header_pos >= min && zm->rxd_header_pos <= max) {
		lprintf(zm, LOG_DEBUG, "%lu Received valid ZACK", (ulong)zm->rxd_header_pos);
		zm->ack_file_pos = zm->rxd_header_pos;
		return TRUE;
	}
	lprintf(zm, LOG_WARNING, "%lu Received INVALID ZACK, offset: %u"
		, (ulong)zm->current_file_pos, zm->rxd_header_pos);
	return FALSE;
}

static unsigned new_window_size(zmodem_t* zm, time_t start, unsigned pos)
{
	time_t elapsed = time(NULL) - start;
	if(elapsed < 1)
		elapsed = 1;
	unsigned cps = (unsigned)(pos / elapsed);
	if(cps < 1)
		cps = 1;
	return cps * zm->target_window_size;
}

/*
 * send from the current position in the file
 * all the way to end of file or until something goes wrong.
 * (ZNAK or ZRPOS received)
 * returns ZRINIT on success.
 */

int zmodem_send_from(zmodem_t* zm, FILE* fp, uint64_t pos, uint64_t* sent)
{
	size_t len;
	uchar tx_type;
	unsigned buf_sent = 0;
	unsigned subpkts_sent = 0;
	unsigned backchannel_wait = 0;
	time_t start = time(NULL);

	lprintf(zm, LOG_DEBUG, "%lu %s", (ulong)pos, __FUNCTION__); 
	if(sent!=NULL)
		*sent=0;

	if(fseeko(fp,(off_t)pos,SEEK_SET) != 0) {
		lprintf(zm, LOG_ERR, "%s ERROR %d seeking to file offset %"PRIu64
			,__FUNCTION__, errno, pos);
		zmodem_send_pos_header(zm, ZFERR, (uint32_t)pos, /* Hex? */ TRUE);
		return ZFERR;
	}
	zm->current_file_pos = pos;

	/*
	 * send the data in the file
	 */

	while(is_connected(zm)) {

		/*
		 * characters from the other side
		 * check out that header
		 */
		if(zm->consecutive_errors && backchannel_wait == 0)
			backchannel_wait = 1;
		while(is_data_waiting(zm, backchannel_wait) && !is_cancelled(zm) && is_connected(zm)) {
			int rx_type;
			int c;
			lprintf(zm, LOG_DEBUG, "Back-channel traffic detected");
			if((c = zmodem_recv_raw(zm)) < 0) {
				lprintf(zm, LOG_ERR, "Back-channel receive ERROR: %s", chr(c));
				return(c);
			}
			if(c == ZPAD) {
				rx_type = zmodem_recv_header(zm);
				lprintf(zm,LOG_DEBUG, "Received back-channel data: %s", chr(rx_type));
				if(rx_type == ZACK && zmodem_handle_zack(zm, zm->ack_file_pos, (uint32_t)zm->current_file_pos)) {
					zm->current_window_size = zm->current_file_pos - zm->ack_file_pos;
					lprintf(zm, LOG_DEBUG, "%lu Asynchronous acknowledgment (ZACK) of %lu bytes, new window: %lu"
						,(ulong)zm->current_file_pos, (ulong)zm->ack_file_pos, (ulong)zm->current_window_size);
					if(zm->max_window_size && zm->target_window_size) {
						zm->max_window_size = new_window_size(zm, start, zm->rxd_header_pos);
						lprintf(zm, LOG_DEBUG, "%lu New window size: %lu (%u seconds of data)"
							,(ulong)zm->current_file_pos, (ulong)zm->max_window_size, zm->target_window_size);
					}
					continue;
				}
				else if(rx_type >= 0) {
					zmodem_send_data(zm, ZCRCE, /* data: */NULL, /* len: */0);
					return rx_type;
				}
			} else
				lprintf(zm,LOG_INFO, "Unexpected back-channel data received: %s", chr(c));
		}
		if(is_cancelled(zm))
			return(ZCAN);

		/*
		 * read a block from the file
		 */
		pos = zm->current_file_pos;
		if(zm->max_window_size && zm->current_window_size >= zm->max_window_size) {
			lprintf(zm, LOG_WARNING, "%lu Transmit-Window management: %lu >= %lu"
				,(ulong)zm->current_file_pos, (ulong)zm->current_window_size, (ulong)zm->max_window_size);
			backchannel_wait = 1;
			continue;
		}

		len = fread(zm->tx_data_subpacket,sizeof(BYTE),zm->block_size,fp);

		if(zm->progress!=NULL)
			zm->progress(zm->cbdata, ftello(fp));

		tx_type = ZCRCW;

		/** ZMODEM.DOC:
			ZCRCW data subpackets expect a response before the next frame is sent.
			If the receiver does not indicate overlapped I/O capability with the
			CANOVIO bit, or sets a buffer size, the sender uses the ZCRCW to allow
			the receiver to write its buffer before sending more data.
		***/
		/* Note: we always use ZCRCW for the first frame */
		if(subpkts_sent || len < zm->block_size) {
			/*  ZMODEM.DOC:
				In the absence of fatal error, the sender eventually encounters end of
				file.  If the end of file is encountered within a frame, the frame is
				closed with a ZCRCE data subpacket which does not elicit a response
				except in case of error.
			*/
			if(len < zm->block_size || zm->consecutive_errors)
				tx_type = ZCRCE;
			else {
				if(zm->can_overlap_io && !zm->no_streaming && (zm->recv_bufsize==0 || buf_sent + len < zm->recv_bufsize)) {
					if(zm->can_full_duplex && zm->max_window_size)
						tx_type = (subpkts_sent % (zm->max_window_size / zm->block_size / 4)) == 0 ? ZCRCQ : ZCRCG;
					else
						tx_type = ZCRCG;
				}
				else	/* Send a ZCRCW frame */
					buf_sent = 0;
			}
		}

		lprintf(zm, LOG_DEBUG, "%lu Sending %s data subpacket (%lu bytes) window: %lu / %lu"
			,(ulong)pos, chr(tx_type), len, (ulong)zm->current_window_size, (ulong)zm->max_window_size);
		if(zmodem_send_data(zm, tx_type, zm->tx_data_subpacket, len) != SEND_SUCCESS) {
			zm->consecutive_errors++;
			continue;
		}

		zm->current_window_size += len;
		zm->current_file_pos += len;
		if(zm->current_file_pos > zm->current_file_size)
			zm->current_file_size = zm->current_file_pos;
		subpkts_sent++;

		if(zmodem_end_of_frame(tx_type)) {
			lprintf(zm, LOG_DEBUG, "%lu Sent end-of-frame (%s subpacket)", (ulong)pos, chr(tx_type));
			if(tx_type == ZCRCW) {	/* ZACK expected */
				lprintf(zm, LOG_DEBUG, "%lu Waiting for ZACK", (ulong)pos);
				while(is_connected(zm)) {
					int ack;
					if((ack = zmodem_recv_header(zm)) != ZACK)
						return(ack);

					if(is_cancelled(zm))
						return(ZCAN);

					if(zmodem_handle_zack(zm, (uint32_t)zm->current_file_pos, (uint32_t)zm->current_file_pos)) {
						zm->current_window_size = 0;
						break;
					}
				}
			}
		}

		if(sent!=NULL)
			*sent += len;

		buf_sent += len;

		if(len < zm->block_size) {
			lprintf(zm, LOG_DEBUG, "%lu End of file (or read error) reached", (ulong)zm->current_file_pos);
			zmodem_send_zeof(zm);
			return zmodem_recv_header(zm);	/* If this is ZRINIT, Success */
		}
		backchannel_wait = 0;

		zm->consecutive_errors = 0;

		if(zm->block_size < zm->max_block_size) {
			zm->block_size*=2;
			if(zm->block_size > zm->max_block_size)
				zm->block_size = zm->max_block_size;
		}
	}

	lprintf(zm, LOG_WARNING, "%s Returning unexpectedly!", __FUNCTION__);

	/*
	 * end of file reached.
	 * should receive something... so fake ZACK
	 */

	return ZACK;
}

/*
 * send a file; returns true when session is successful. (or file is skipped)
 */

BOOL zmodem_send_file(zmodem_t* zm, char* fname, FILE* fp, BOOL request_init, time_t* start, uint64_t* sent)
{
	uint64_t	pos=0;
	uint64_t	sent_bytes;
	struct stat		s;
	unsigned char * p;
	uchar		zfile_frame[] = { ZFILE, 0, 0, 0, 0 };
	int			type;
	int			i;
	unsigned	attempts;

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
			if(zm->errors)
				lprintf(zm,LOG_NOTICE,"Sending ZRQINIT (%u of %u)"
					,zm->errors+1,zm->max_errors+1);
			else
				lprintf(zm,LOG_INFO,"Sending ZRQINIT");
			i = zmodem_get_zrinit(zm);
			if(i == ZRINIT)
				break;
			lprintf(zm,LOG_WARNING, "%s UNEXPECTED %s received instead of ZRINIT"
				,__FUNCTION__, frame_desc(i));
		}
		if(zm->errors>=zm->max_errors || is_cancelled(zm) || !is_connected(zm))
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
		lprintf(zm,LOG_DEBUG, "protecting destination");
	}
	else if(zm->management_clobber) {
		zfile_frame[ZF1] = ZF1_ZMCLOB;
		lprintf(zm,LOG_DEBUG, "overwriting destination");
	}
	else if(zm->management_newer) {
		zfile_frame[ZF1] = ZF1_ZMNEW;
		lprintf(zm,LOG_DEBUG, "overwriting destination if newer");
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

	strncpy((char *)zm->tx_data_subpacket,getfname(fname),sizeof(zm->tx_data_subpacket)-1);
	zm->tx_data_subpacket[sizeof(zm->tx_data_subpacket)-1]=0;

	p += strlen((char*)p) + 1;

	sprintf((char*)p,"%"PRId64" %"PRIoMAX" 0 0 %u %"PRId64" 0"
		,zm->current_file_size	/* use for estimating only, could be zero! */
		,(uintmax_t)s.st_mtime
		,zm->files_remaining
		,zm->bytes_remaining
		);

	p += strlen((char*)p) + 1;

	for(type = INVALIDSUBPKT, attempts=0; type != ZRPOS; attempts++) {

		if(attempts >= zm->max_errors)
			return(FALSE);

		/*
	 	 * send the header and the data
	 	 */

		lprintf(zm,LOG_DEBUG,"Sending ZFILE frame: '%s'"
			,zm->tx_data_subpacket+strlen((char*)zm->tx_data_subpacket)+1);

		if((i=zmodem_send_bin_header(zm,zfile_frame)) != SEND_SUCCESS) {
			lprintf(zm,LOG_DEBUG,"zmodem_send_bin_header returned %d",i);
			continue;
		}
		if((i=zmodem_send_data_subpkt(zm,ZCRCW,zm->tx_data_subpacket,p - zm->tx_data_subpacket)) != SEND_SUCCESS) {
			lprintf(zm,LOG_DEBUG,"zmodem_send_data_subpkt returned %d",i);
			continue;
		}
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

		if(type == ZCRC) {
			if(zm->crc_request==0)
				lprintf(zm,LOG_NOTICE,"Receiver requested CRC of entire file");
			else
				lprintf(zm,LOG_NOTICE,"Receiver requested CRC of first %u bytes of file"
					,zm->crc_request);
			zmodem_send_pos_header(zm, ZCRC, fcrc32(fp,zm->crc_request), /* Hex: */TRUE);
			type = zmodem_recv_header(zm);
		}

		if(type == ZSKIP) {
			zm->file_skipped=TRUE;
			lprintf(zm,LOG_WARNING,"File skipped by receiver");
			return(TRUE);
		}
	}

	if(!zmodem_handle_zrpos(zm, &pos))
		return(FALSE);

	zm->transfer_start_pos = pos;
	zm->transfer_start_time = time(NULL);

	if(start!=NULL)
		*start=zm->transfer_start_time;

	rewind(fp);
	zm->errors = 0;
	zm->consecutive_errors = 0;

	lprintf(zm,LOG_DEBUG,"%lu Sending %s", (ulong)pos, fname);
	do {
		/*
		 * and start sending
		 */

		type = zmodem_send_from(zm, fp, pos, &sent_bytes);

		if(!is_connected(zm))
			return(FALSE);

		if(type == ZFERR || type == ZABORT || is_cancelled(zm)) {
			lprintf(zm, LOG_WARNING, "Aborting receive");
			break;
		}

		if(type == ZSKIP) {
			zm->file_skipped=TRUE;
			lprintf(zm,LOG_WARNING,"File skipped by receiver at offset: %"PRIu64, pos + sent_bytes);
			/* ZOC sends a ZRINIT after mid-file ZSKIP, so consume the ZRINIT here */
			zmodem_recv_header(zm);
			return(TRUE);
		}

		if(sent != NULL)
			*sent += sent_bytes;

		if(type == ZRINIT)
			return(TRUE);	/* Success */

		pos = zm->ack_file_pos;

		if(type == ZACK && zmodem_handle_zack(zm, (uint32_t)pos, (uint32_t)zm->current_file_pos)) {
			continue;
		}

		/* Error of some kind */

		zm->errors++;
		lprintf(zm, LOG_ERR, "%lu ERROR #%d: %s", (ulong)zm->current_file_pos, zm->errors, chr(type));

		if(zm->block_size == zm->max_block_size && zm->max_block_size > ZBLOCKLEN)
			zm->max_block_size /= 2;

		if(zm->block_size > 128)
			zm->block_size /= 2;

		if(++zm->consecutive_errors > zm->max_errors) {
			lprintf(zm, LOG_WARNING, "Too many consecutive errors: %u (%u total)"
				,zm->consecutive_errors, zm->errors);
			break;	/* failure */
		}

		if(type==ZRPOS) {
			if(!zmodem_handle_zrpos(zm, &pos))
				break;
		}
	} while(TRUE);

	lprintf(zm,LOG_WARNING,"Transfer failed on receipt of: %s", chr(type));

	return(FALSE);
}

/* Returns number of successfully-received files */
int zmodem_recv_files(zmodem_t* zm, const char* download_dir, uint64_t* bytes_received)
{
	char		fpath[MAX_PATH*2+2];
	FILE*		fp;
	int64_t		l;
	BOOL		skip;
	BOOL		loop;
	uint64_t	b;
	uint32_t	crc;
	uint32_t	rcrc;
	int64_t		bytes;
	int64_t		kbytes;
	int64_t		start_bytes;
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
		lprintf(zm,LOG_INFO,"Downloading %s (%"PRId64" KBytes) via ZMODEM", zm->current_file_name, kbytes);

		do {	/* try */
			skip=TRUE;
			loop=FALSE;

			SAFEPRINTF2(fpath,"%s/%s",download_dir,zm->current_file_name);
			lprintf(zm,LOG_DEBUG,"fpath=%s",fpath);
			if(fexist(fpath)) {
				l=flength(fpath);
				lprintf(zm,LOG_WARNING,"%s already exists (%"PRId64" bytes)",fpath,l);
				if(l>=(int32_t)bytes) {
					lprintf(zm,LOG_WARNING,"Local file size >= remote file size (%"PRId64")"
						,bytes);
					if(zm->duplicate_filename==NULL)
						break;
					else {
						if(l > (int32_t)bytes) {
							if(zm->duplicate_filename(zm->cbdata, zm)) {
								loop=TRUE;
								continue;
							}
							break;
						}
					}
				}
				if((fp=fopen(fpath,"rb"))==NULL) {
					lprintf(zm,LOG_ERR,"Error %d opening %s", errno, fpath);
					break;
				}
				setvbuf(fp,NULL,_IOFBF,0x10000);

				lprintf(zm,LOG_NOTICE,"Requesting CRC of remote file: %s", zm->current_file_name);
				if(!zmodem_request_crc(zm, (uint32_t)l)) {
					fclose(fp);
					lprintf(zm,LOG_ERR,"Failed to request CRC of remote file");
					break;
				}
				lprintf(zm,LOG_NOTICE,"Calculating CRC of: %s", fpath);
				crc=fcrc32(fp,(uint32_t)l);	/* Warning: 4GB limit! */
				fclose(fp);
				lprintf(zm,LOG_INFO,"CRC of %s (%lu bytes): %08X"
					,getfname(fpath), (ulong)l, crc);
				lprintf(zm,LOG_NOTICE,"Waiting for CRC of remote file: %s", zm->current_file_name);
				if(!zmodem_recv_crc(zm,&rcrc)) {
					lprintf(zm,LOG_ERR,"Failed to get CRC of remote file");
					break;
				}
				if(crc!=rcrc) {
					lprintf(zm,LOG_WARNING,"Remote file has different CRC value: %08X", rcrc);
					if(zm->duplicate_filename) {
						if(zm->duplicate_filename(zm->cbdata, zm)) {
							loop=TRUE;
							continue;
						}
					}
					break;
				}
				if(l == (int32_t)bytes) {
					lprintf(zm,LOG_INFO,"CRC, length, and filename match.");
					break;
				}
				lprintf(zm,LOG_INFO,"Resuming download of %s",fpath);
			}

			if((fp=fopen(fpath,"ab"))==NULL) {
				lprintf(zm,LOG_ERR,"Error %d opening/creating/appending %s",errno,fpath);
				break;
			}
			start_bytes=filelength(fileno(fp));
			if(start_bytes < 0) {
				fclose(fp);
				lprintf(zm,LOG_ERR,"Invalid file length %"PRId64": %s", start_bytes, fpath);
				break;
			}

			skip=FALSE;
			errors=zmodem_recv_file_data(zm,fp,start_bytes);
			if(errors)
				lprintf(zm, LOG_WARNING, "%u errors occurred while receiving file: %s", errors, fpath);
			fclose(fp);
			l=flength(fpath);
			if(errors && l==0)	{	/* aborted/failed download */
				if(remove(fpath))	/* don't save 0-byte file */
					lprintf(zm,LOG_ERR,"Error %d removing %s",errno,fpath);
				else
					lprintf(zm,LOG_INFO,"Deleted 0-byte file %s",fpath);
				return files_received;
			}
			else {
				if(l!=bytes) {
					lprintf(zm,LOG_WARNING,"Incomplete download (%"PRId64" bytes received, expected %"PRId64")"
						,l,bytes);
					return files_received;
				} else {
					if((t=time(NULL)-zm->transfer_start_time)<=0)
						t=1;
					b=l-start_bytes;
					if((cps=(unsigned)(b/t))==0)
						cps=1;
					lprintf(zm,LOG_INFO,"Received %"PRIu64" bytes successfully (%u CPS)",b,cps);
					files_received++;
					if(bytes_received!=NULL)
						*bytes_received+=b;
				}
				if(zm->current_file_time)
					setfdate(fpath,zm->current_file_time);
			}

		} while(loop);
		/* finally */

		if(skip) {
			lprintf(zm,LOG_WARNING,"Skipping file");
			zmodem_send_zskip(zm);
		}
		zm->current_file_num++;
	}
	if(zm->local_abort)
		zmodem_send_zabort(zm);

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

	lprintf(zm,LOG_DEBUG, __FUNCTION__);

#if 0
	while(is_connected(zm) && !is_cancelled(zm) && (ch=zm->recv_byte(zm,0))!=NOINP)
		lprintf(zm,LOG_DEBUG, __FUNCTION__ " Throwing out received: %s",chr((uchar)ch));
#endif

	for(errors=0; errors<=zm->max_errors && !is_cancelled(zm) && is_connected(zm); errors++) {
		if(errors)
			lprintf(zm,LOG_NOTICE,"Sending ZRINIT (%u of %u)"
				,errors+1, zm->max_errors+1);
		else
			lprintf(zm,LOG_INFO,"Sending ZRINIT");
		zmodem_send_zrinit(zm);

		type = zmodem_recv_header(zm);

		if(zm->local_abort)
			break;

		if(type==TIMEOUT)
			continue;

		lprintf(zm,LOG_DEBUG, "%s received %s", __FUNCTION__, chr(type));

		if(type==ZFILE) {
			zmodem_parse_zfile_subpacket(zm);
			return(type);
		}

		if(type==ZFIN) {
			zmodem_send_zfin(zm);	/* ACK */
			return(type);
		}

		lprintf(zm,LOG_WARNING, "%s received %s instead of ZFILE or ZFIN"
			,__FUNCTION__, frame_desc(type));
		lprintf(zm,LOG_DEBUG,"ZF0=%02X ZF1=%02X ZF2=%02X ZF3=%02X"
			,zm->rxd_header[ZF0],zm->rxd_header[ZF1],zm->rxd_header[ZF2],zm->rxd_header[ZF3]);
	}

	return(type);
}

void zmodem_parse_zfile_subpacket(zmodem_t* zm)
{
	int			i;
	int			mode=0;
	long		serial=-1L;
	ulong		tmptime;

	SAFECOPY(zm->current_file_name,getfname((char*)zm->rx_data_subpacket));

	zm->current_file_size = 0;
	zm->current_file_time = 0;
	zm->files_remaining = 0;
	zm->bytes_remaining = 0;

	i=sscanf((char*)zm->rx_data_subpacket+strlen((char*)zm->rx_data_subpacket)+1,"%"SCNd64" %lo %o %lo %u %"SCNd64
		,&zm->current_file_size	/* file size (decimal) */
		,&tmptime				/* file time (octal unix format) */
		,&mode					/* file mode */
		,&serial				/* program serial number */
		,&zm->files_remaining	/* remaining files to be sent */
		,&zm->bytes_remaining	/* remaining bytes to be sent */
		);
	zm->current_file_time=tmptime;

	lprintf(zm,LOG_DEBUG,"ZMODEM file (ZFILE) data (%u fields): %s"
		,i, zm->rx_data_subpacket+strlen((char*)zm->rx_data_subpacket)+1);

	if(!zm->files_remaining)
		zm->files_remaining = 1;
	if(!zm->bytes_remaining)
		zm->bytes_remaining = zm->current_file_size;

	if(!zm->total_files)
		zm->total_files = zm->files_remaining;
	if(!zm->total_bytes)
		zm->total_bytes = zm->bytes_remaining;
}

BOOL zmodem_waits_for_ack(int type)
{
	return type == ZCRCW;
}

/*
 * receive file data until the end of the file or until something goes wrong.
 * returns error count (non-zero does not mean failure).
 */

unsigned zmodem_recv_file_data(zmodem_t* zm, FILE* fp, int64_t offset)
{
	int			type = INVALIDSUBPKT;	// data subpacket type
	unsigned	errors=0;
	off_t		pos = (off_t)offset;

	zm->transfer_start_pos=offset;
	zm->transfer_start_time=time(NULL);

	if(fseeko(fp, pos, SEEK_SET) != 0) {
		lprintf(zm,LOG_ERR, "%s ERROR %d seeking to file offset %"PRId64
			,__FUNCTION__, errno, offset);
		zmodem_send_pos_header(zm, ZFERR, (uint32_t)pos, /* Hex? */ TRUE);
		return 1; /* errors */
	}
	zmodem_send_pos_header(zm, ZRPOS, (uint32_t)pos, /* Hex? */ TRUE);

	/*  zmodem.doc:

		The zmodem receiver uses the file length [from ZFILE data] as an estimate only.
		It may be used to display an estimate of the transmission time,
		and may be compared with the amount of free disk space.  The
		actual length of the received file is determined by the data
		transfer. A file may grow after transmission commences, and
		all the data will be sent.
	*/
	while(is_connected(zm) && !is_cancelled(zm)) {

		if(pos > zm->current_file_size)
			zm->current_file_size = pos;

		if(zm->max_file_size != 0 && pos >= zm->max_file_size) {
			lprintf(zm, LOG_ERR, "%lu Specified maximum file size (%"PRId64" bytes) reached"
				,(ulong)pos, zm->max_file_size);
			zmodem_send_pos_header(zm, ZFERR, (uint32_t)pos, /* Hex? */ TRUE);
			++errors;
			break;
		}

		int result = zmodem_recv_file_frame(zm, fp, &type);
		pos = ftello(fp);
		if(result == ENDOFFRAME) {
			lprintf(zm,LOG_DEBUG, "%lu Complete data frame received (type: %s)", (ulong)pos, chr(type));
			continue;
		}
		if(result == ZEOF || result == ZFIN) {
			lprintf(zm, LOG_INFO, "%lu Received: %s", (ulong)pos, chr(result));
			break;
		}
		errors++;
		lprintf(zm, LOG_WARNING, "%lu ERROR #%d: %s (type: %s)", (ulong)pos, errors, chr(result), chr(type));
		if(errors > zm->max_errors) {
			lprintf(zm, LOG_ERR, "%lu Maximum errors (%u) exceeded", (ulong)pos, zm->max_errors);
			break;
		}
		if(zmodem_waits_for_ack(type))
			zmodem_recv_purge(zm, /* timeout: */1);
		lprintf(zm, LOG_NOTICE, "%lu Repositioning sender (sending ZRPOS)", (ulong)pos);
		zmodem_send_pos_header(zm, ZRPOS, (uint32_t)pos, /* Hex? */ TRUE);
	}

	return errors;
}


int zmodem_recv_file_frame(zmodem_t* zm, FILE* fp, int* type)
{
	int			result;
	unsigned	attempt;

	/*
	 * wait for a ZDATA header with the right file offset
	 * or a timeout or a ZFIN
	 */

	for(attempt=0;;attempt++) {
		if(attempt>=zm->max_errors)
			return TIMEOUT;
		result = zmodem_recv_header(zm);
		switch(result) {
			case ZEOF:
				/* ZMODEM.DOC:
				   If the receiver has not received all the bytes of the file,
				   the receiver ignores the ZEOF because a new ZDATA is coming.
				*/
				if(zm->rxd_header_pos==(uint32_t)ftello(fp))
					return result;
				lprintf(zm,LOG_WARNING,"Ignoring ZEOF as all bytes (%u) have not been received"
					,zm->rxd_header_pos);
				continue;
			case ZFIN:
			case TIMEOUT:
				return result;
		}
		if(is_cancelled(zm) || !is_connected(zm))
			return ZCAN;

		if(result == ZDATA)
			break;

		lprintf(zm, LOG_WARNING, "%lu Received %s instead of ZDATA frame", (ulong)ftello(fp), frame_desc(result));
	}

	if(zm->rxd_header_pos != (uint32_t)ftello(fp)) {
		lprintf(zm,LOG_WARNING, "%lu Received ZDATA frame with invalid position: %lu"
			,(ulong)ftello(fp), (ulong)zm->rxd_header_pos);
		return FALSE;
	}

	do {
		unsigned len = 0;
		result = zmodem_recv_data(zm,zm->rx_data_subpacket,sizeof(zm->rx_data_subpacket), &len, /* ack */TRUE, type);

/*		fprintf(stderr,"packet len %d type %d\n",n,type);
*/
		if (result == ENDOFFRAME || result == FRAMEOK) {
			if(fwrite(zm->rx_data_subpacket, sizeof(uint8_t), len, fp) != len) {
				lprintf(zm,LOG_ERR,"ERROR %d writing %u bytes at file offset %"PRIu64
						,errno, len, (uint64_t)ftello(fp));
				zmodem_send_pos_header(zm, ZFERR, (uint32_t)ftello(fp), /* Hex? */ TRUE);
				return FALSE;
			}
		}

		if(result == FRAMEOK)
			zm->block_size = len;

		if(zm->progress!=NULL)
			zm->progress(zm->cbdata, ftello(fp));

		if(is_cancelled(zm))
			return ZCAN;

	} while(result == FRAMEOK);

	return result;
}

const char* zmodem_source(void)
{
	return(__FILE__);
}

char* zmodem_ver(char *buf)
{
	return strcpy(buf, "2.0");
}

void zmodem_init(zmodem_t* zm, void* cbdata
				,int	(*lputs)(void*, int level, const char* str)
				,void	(*progress)(void* unused, int64_t)
				,int	(*send_byte)(void*, uchar ch, unsigned timeout /* seconds */)
				,int	(*recv_byte)(void*, unsigned timeout /* seconds */)
				,BOOL	(*is_connected)(void*)
				,BOOL	(*is_cancelled)(void*)
				,BOOL	(*data_waiting)(void*, unsigned timeout /* seconds */)
				,void   (*flush)(void*))
{
	memset(zm,0,sizeof(zmodem_t));

	/* Use sane default values */
	zm->init_timeout=10;		/* seconds */
	zm->send_timeout=10;		/* seconds (reduced from 15) */
	zm->recv_timeout=10;		/* seconds (reduced from 20) */
	zm->crc_timeout=120;		/* seconds */
	zm->block_size=ZBLOCKLEN;
	zm->max_block_size=ZBLOCKLEN;
	zm->max_errors=9;
	zm->can_full_duplex=TRUE;

	zm->cbdata=cbdata;
	zm->lputs=lputs;
	zm->progress=progress;
	zm->send_byte=send_byte;
	zm->recv_byte=recv_byte;
	zm->is_connected=is_connected;
	zm->is_cancelled=is_cancelled;
	zm->data_waiting=data_waiting;
	zm->flush=flush;
}
