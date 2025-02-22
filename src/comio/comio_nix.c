/* Synchronet Serial Communications I/O Library Functions for *nix */

/****************************************************************************
 * @format.tab-size 4       (Plain Text/Source Code File Header)            *
 * @format.use-tabs true    (see http://www.synchro.net/ptsc_hdr.html)      *
 *                                                                          *
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html           *
 *                                                                          *
 * This library is free software; you can redistribute it and/or            *
 * modify it under the terms of the GNU Lesser General Public License       *
 * as published by the Free Software Foundation; either version 2           *
 * of the License, or (at your option) any later version.                   *
 * See the GNU Lesser General Public License for more details: lgpl.txt or  *
 * http://www.fsf.org/copyleft/lesser.html                                  *
 *                                                                          *
 * For Synchronet coding style and modification guidelines, see             *
 * http://www.synchro.net/source.html                                       *
 *                                                                          *
 * Note: If this box doesn't appear square, then you need to fix your tabs. *
 ****************************************************************************/

#include <sys/ioctl.h>
#include <sys/file.h>
#include <fcntl.h>      // O_NONBLOCK
#include "comio.h"
#include "genwrap.h"

#if defined(CCTS_OFLOW) && defined(CRTS_IFLOW)
 #define CTSRTS_FLOW_CFLAGS (CCTS_OFLOW | CRTS_IFLOW)
#elif defined(CRTSCTS)
 #define CTSRTS_FLOW_CFLAGS (CRTSCTS)
#else
 #error No way to control CTS/RTS flow control
#endif

#if defined(IXON) && defined (IXOFF)
 #define XONXOFF_FLOW_IFLAGS (IXON | IXOFF)
#else
 #error No way to control XON/XOFF flow control
#endif

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
#define rate_to_macro(x)    (x)
#define macro_to_rate(x)    (x)
#endif

char* comVersion(char* str, size_t len)
{
    char revision[16];

    sscanf("$Revision: 1.19 $", "%*s %s", revision);

    safe_snprintf(str,len,"Synchronet Communications I/O Library for "PLATFORM_DESC" v%s", revision);
    return str;
}

COM_HANDLE comOpen(const char* device)
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
    t.c_oflag = 0;  /* No output processing */
#ifdef CBAUD
    t.c_cflag &= CBAUD;
#else
    t.c_cflag = 0;
#endif
    t.c_cflag |= (
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
                | CTSRTS_FLOW_CFLAGS
                );
    t.c_lflag = 0;  /* No local modes */
    if(tcsetattr(handle, TCSANOW, &t)==-1) {
        close(handle);
        return COM_HANDLE_INVALID;
    }

    return handle;
}

bool comClose(COM_HANDLE handle)
{
    if (handle == COM_HANDLE_INVALID)
        return false;
    return (!close(handle));
}

long comGetBaudRate(COM_HANDLE handle)
{
    struct termios t;
    speed_t in;
    speed_t out;

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

bool comSetBaudRate(COM_HANDLE handle, unsigned long rate)
{
    struct termios t;

    if(tcgetattr(handle, &t))
        return false;

    cfsetispeed(&t, rate_to_macro(rate));
    cfsetospeed(&t, rate_to_macro(rate));
    if(tcsetattr(handle, TCSANOW, &t)==-1)
        return false;

    return true;
}

int comGetFlowControl(COM_HANDLE handle)
{
    int ret = 0;
    struct termios t;

    if(tcgetattr(handle, &t)==-1)
        return false;

    if ((t.c_cflag & CTSRTS_FLOW_CFLAGS) == CTSRTS_FLOW_CFLAGS)
        ret |= COM_FLOW_CONTROL_RTS_CTS;
    if ((t.c_iflag & XONXOFF_FLOW_IFLAGS) == XONXOFF_FLOW_IFLAGS)
        ret |= COM_FLOW_CONTROL_XON_OFF;

    return ret;
}

bool comSetFlowControl(COM_HANDLE handle, int modes)
{
    struct termios t;

    if(tcgetattr(handle, &t)==-1)
        return false;

    if (modes & COM_FLOW_CONTROL_RTS_CTS)
        t.c_cflag |= CTSRTS_FLOW_CFLAGS;
    else
        t.c_cflag &= ~CTSRTS_FLOW_CFLAGS;

    if (modes & COM_FLOW_CONTROL_XON_OFF)
        t.c_iflag |= XONXOFF_FLOW_IFLAGS;
    else
        t.c_iflag &= ~XONXOFF_FLOW_IFLAGS;

    if(tcsetattr(handle, TCSANOW, &t)==-1)
        return false;

    return true;
}

bool comSetParity(COM_HANDLE handle, bool enable, bool odd)
{
    struct termios t;

    if(tcgetattr(handle, &t)==-1)
        return false;

    if (enable) {
        t.c_cflag |= PARENB;
		if (odd)
			t.c_cflag |= PARODD;
		else
			t.c_cflag &= ~PARODD;
	} else
        t.c_cflag &= ~(PARENB | PARODD);

    if(tcsetattr(handle, TCSANOW, &t)==-1)
        return false;

    return true;
}

bool comSetBits(COM_HANDLE handle, size_t byteSize, size_t stopBits)
{
    struct termios t;

    if(tcgetattr(handle, &t)==-1)
		return false;

	t.c_cflag &= ~CSIZE;
	switch(byteSize) {
		case 5:
			t.c_cflag |= CS5;
#ifdef ISTRIP
			t.c_iflag |= ISTRIP;
#endif
			break;
		case 6:
			t.c_cflag |= CS6;
#ifdef ISTRIP
			t.c_iflag |= ISTRIP;
#endif
			break;
		case 7:
			t.c_cflag |= CS7;
#ifdef ISTRIP
			t.c_iflag |= ISTRIP;
#endif
			break;
		default:
			t.c_cflag |= CS8;
#ifdef ISTRIP
			t.c_iflag &= ~(ISTRIP);
#endif
			break;
	}
	if(stopBits == 2)
		t.c_cflag |= CSTOPB;
	else
		t.c_cflag &= ~CSTOPB;

    if(tcsetattr(handle, TCSANOW, &t)==-1)
        return false;

    return true;
}

int comGetModemStatus(COM_HANDLE handle)
{
    int status;

    if(ioctl(handle, TIOCMGET, &status)==-1)
        return COM_ERROR;

    return status;
}

static bool comSetFlags(COM_HANDLE handle, int flags, bool set)
{
    unsigned long cmd = set ? TIOCMBIS : TIOCMBIC;

    return (ioctl(handle, cmd, &flags) == 0);
}

bool comRaiseDTR(COM_HANDLE handle)
{
    return comSetFlags(handle, TIOCM_DTR, true);
}

bool comLowerDTR(COM_HANDLE handle)
{
    return comSetFlags(handle, TIOCM_DTR, false);
}

bool comRaiseRTS(COM_HANDLE handle)
{
    return comSetFlags(handle, TIOCM_RTS, true);
}

bool comLowerRTS(COM_HANDLE handle)
{
    return comSetFlags(handle, TIOCM_RTS, false);
}
bool comWriteByte(COM_HANDLE handle, BYTE ch)
{
    return(write(handle, &ch, 1)==1);
}

int comWriteBuf(COM_HANDLE handle, const BYTE* buf, size_t buflen)
{
    return write(handle, buf, buflen);
}

/*
 * TODO: This seem kinda dangerous for short writes...
 */
int comWriteString(COM_HANDLE handle, const char* str)
{
    return comWriteBuf(handle, (BYTE*)str, strlen(str));
}

bool comReadByte(COM_HANDLE handle, BYTE* ch)
{
    return(read(handle, ch, 1)==1);
}

bool comPurgeInput(COM_HANDLE handle)
{
    return(tcflush(handle, TCIFLUSH)==0);
}

bool comPurgeOutput(COM_HANDLE handle)
{
    return(tcflush(handle, TCOFLUSH)==0);
}

bool comDrainOutput(COM_HANDLE handle)
{
    return(tcdrain(handle)==0);
}
