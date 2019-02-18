/* comio_nix.c */

/* Synchronet Serial Communications I/O Library Functions for *nix */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
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

#include <sys/ioctl.h>
#include <sys/file.h>
#include <fcntl.h>		// O_NONBLOCK
#include "comio.h"
#include "genwrap.h"

#ifdef SPEED_MACROS_ONLY

#define SUPPORTED_SPEED(x) \
	if (speed <= (x)) \
		return B##x

speed_t rate_to_macro(unsigned long speed)
{
	// Standard values
	SUPPORTED_SPEED(0);
	SUPPORTED_SPEED(50);
	SUPPORTED_SPEED(75);
	SUPPORTED_SPEED(110);
	SUPPORTED_SPEED(134);
	SUPPORTED_SPEED(150);
	SUPPORTED_SPEED(200);
	SUPPORTED_SPEED(300);
	SUPPORTED_SPEED(600);
	SUPPORTED_SPEED(1200);
	SUPPORTED_SPEED(1800);
	SUPPORTED_SPEED(2400);
	SUPPORTED_SPEED(4800);
	SUPPORTED_SPEED(9600);
	SUPPORTED_SPEED(19200);
	SUPPORTED_SPEED(38400);

	// Non-POSIX
#ifdef B57600
	SUPPORTED_SPEED(57600);
#endif
#ifdef B115200
	SUPPORTED_SPEED(115200);
#endif
#ifdef B230400
	SUPPORTED_SPEED(230400);
#endif
#ifdef B460800
	SUPPORTED_SPEED(460800);
#endif
#ifdef B500000
	SUPPORTED_SPEED(500000);
#endif
#ifdef B576000
	SUPPORTED_SPEED(576000);
#endif
#ifdef B921600
	SUPPORTED_SPEED(921600);
#endif
#ifdef B1000000
	SUPPORTED_SPEED(1000000);
#endif
#ifdef B1152000
	SUPPORTED_SPEED(1152000);
#endif
#ifdef B1500000
	SUPPORTED_SPEED(1500000);
#endif
#ifdef B2000000
	SUPPORTED_SPEED(2000000);
#endif
#ifdef B2500000
	SUPPORTED_SPEED(2500000);
#endif
#ifdef B3000000
	SUPPORTED_SPEED(3000000);
#endif
#ifdef B3500000
	SUPPORTED_SPEED(3500000);
#endif
#ifdef B4000000
	SUPPORTED_SPEED(4000000);
#endif
	return B0;
}
#undef SUPPORTED_SPEED
#define SUPPORTED_SPEED(x) \
	if (speed == B##x) \
		return x;

unsigned long macro_to_rate(speed_t speed)
{
	// Standard values
	SUPPORTED_SPEED(0);
	SUPPORTED_SPEED(50);
	SUPPORTED_SPEED(75);
	SUPPORTED_SPEED(110);
	SUPPORTED_SPEED(134);
	SUPPORTED_SPEED(150);
	SUPPORTED_SPEED(200);
	SUPPORTED_SPEED(300);
	SUPPORTED_SPEED(600);
	SUPPORTED_SPEED(1200);
	SUPPORTED_SPEED(1800);
	SUPPORTED_SPEED(2400);
	SUPPORTED_SPEED(4800);
	SUPPORTED_SPEED(9600);
	SUPPORTED_SPEED(19200);
	SUPPORTED_SPEED(38400);

	// Non-POSIX
#ifdef B57600
	SUPPORTED_SPEED(57600);
#endif
#ifdef B115200
	SUPPORTED_SPEED(115200);
#endif
#ifdef B230400
	SUPPORTED_SPEED(230400);
#endif
#ifdef B460800
	SUPPORTED_SPEED(460800);
#endif
#ifdef B500000
	SUPPORTED_SPEED(500000);
#endif
#ifdef B576000
	SUPPORTED_SPEED(576000);
#endif
#ifdef B921600
	SUPPORTED_SPEED(921600);
#endif
#ifdef B1000000
	SUPPORTED_SPEED(1000000);
#endif
#ifdef B1152000
	SUPPORTED_SPEED(1152000);
#endif
#ifdef B1500000
	SUPPORTED_SPEED(1500000);
#endif
#ifdef B2000000
	SUPPORTED_SPEED(2000000);
#endif
#ifdef B2500000
	SUPPORTED_SPEED(2500000);
#endif
#ifdef B3000000
	SUPPORTED_SPEED(3000000);
#endif
#ifdef B3500000
	SUPPORTED_SPEED(3500000);
#endif
#ifdef B4000000
	SUPPORTED_SPEED(4000000);
#endif
	return 0;
}
#undef SUPPORTED_SPEED

#else
#define rate_to_macro(x)	(x)
#define macro_to_rate(x)	(x)
#endif

char* COMIOCALL comVersion(char* str, size_t len)
{
	char revision[16];

	sscanf("$Revision$", "%*s %s", revision);

	safe_snprintf(str,len,"Synchronet Communications I/O Library for "PLATFORM_DESC" v%s", revision);
	return str;
}

COM_HANDLE COMIOCALL comOpen(const char* device)
{
	COM_HANDLE handle;
	struct termios t;

	if((handle=open(device, O_NONBLOCK|O_RDWR))==COM_HANDLE_INVALID)
		return COM_HANDLE_INVALID;

	if(tcgetattr(handle, &t)==-1) {
		close(handle);
		return COM_HANDLE_INVALID;
	}

	t.c_iflag = (
				  IGNBRK   /* ignore BREAK condition */
				| IGNPAR   /* ignore (discard) parity errors */
				);
	t.c_oflag = 0;	/* No output processing */
	t.c_cflag = (
				  CS8         /* 8 bits */
				| CREAD       /* enable receiver */
/*
Fun snippet from the FreeBSD manpage:

     If CREAD is set, the receiver is enabled.  Otherwise, no character is
     received.  Not all hardware supports this bit.  In fact, this flag is
     pretty silly and if it were not part of the termios specification it
     would be omitted.
*/
				| HUPCL       /* hang up on last close */
				| CLOCAL      /* ignore modem status lines */
/* The next two are pretty much never used */
#ifdef CCTX_OFLOW
				| CCTS_OFLOW  /* CTS flow control of output */
#endif
#ifdef CRTSCTS
				| CRTSCTS     /* same as CCTS_OFLOW */
#endif
#ifdef CRTS_IFLOW
				| CRTS_IFLOW  /* RTS flow control of input */
#endif
				);
	t.c_lflag = 0;	/* No local modes */
	if(tcsetattr(handle, TCSANOW, &t)==-1) {
		close(handle);
		return COM_HANDLE_INVALID;
	}
	
	return handle;
}

BOOL COMIOCALL comClose(COM_HANDLE handle)
{
	return (!close(handle));
}

long COMIOCALL comGetBaudRate(COM_HANDLE handle)
{
	struct termios t;
	speed_t	in;
	speed_t	out;

	if(tcgetattr(handle, &t))
		return COM_ERROR;

	/* 
	 * We actually have TWO speeds available...
	 * return the biggest one
	 */
	in = macro_to_rate(cfgetispeed(&t));
	out = macro_to_rate(cfgetospeed(&t));
	return ((long)(in>out?in:out));
}

BOOL COMIOCALL comSetBaudRate(COM_HANDLE handle, unsigned long rate)
{
	struct termios t;

	if(tcgetattr(handle, &t))
		return FALSE;

	cfsetispeed(&t, rate_to_macro(rate));
	cfsetospeed(&t, rate_to_macro(rate));
	if(tcsetattr(handle, TCSANOW, &t)==-1)
		return FALSE;

	return TRUE;
}

int COMIOCALL comGetModemStatus(COM_HANDLE handle)
{
	int status;

	if(ioctl(handle, TIOCMGET, &status)==-1)
		return COM_ERROR;

	return status;
}

BOOL COMIOCALL comRaiseDTR(COM_HANDLE handle)
{
	int flags = TIOCM_DTR;
	return(ioctl(handle, TIOCMBIS, &flags)==0);
}

BOOL COMIOCALL comLowerDTR(COM_HANDLE handle)
{
	int flags = TIOCM_DTR;
	return(ioctl(handle, TIOCMBIC, &flags)==0);
}

BOOL COMIOCALL comWriteByte(COM_HANDLE handle, BYTE ch)
{
	return(write(handle, &ch, 1)==1);
}

int COMIOCALL comWriteBuf(COM_HANDLE handle, const BYTE* buf, size_t buflen)
{
	return write(handle, buf, buflen);
}

/*
 * TODO: This seem kinda dangerous for short writes...
 */
int COMIOCALL comWriteString(COM_HANDLE handle, const char* str)
{
	return comWriteBuf(handle, (BYTE*)str, strlen(str));
}

BOOL COMIOCALL comReadByte(COM_HANDLE handle, BYTE* ch)
{
	return(read(handle, ch, 1)==1);
}

BOOL COMIOCALL comPurgeInput(COM_HANDLE handle)
{
	return(tcflush(handle, TCIFLUSH)==0);
}

BOOL COMIOCALL comPurgeOutput(COM_HANDLE handle)
{
	return(tcflush(handle, TCOFLUSH)==0);
}

BOOL COMIOCALL comDrainOutput(COM_HANDLE handle)
{
	return(tcdrain(handle)==0);
}
