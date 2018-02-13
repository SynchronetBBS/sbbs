#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "xpdoor.h"
#include <ansi_cio.h>
#include <comio.h>
#include <genwrap.h>
#include <cterm.h>
#include <dirwrap.h>

struct xpd_info		xpd_info;
static BOOL raw_write=FALSE;
static struct cterminal *cterm;

static int xpd_ansi_readbyte_cb(void)
{
	unsigned char	ch;

	switch(xpd_info.io_type) {
		case XPD_IO_STDIO:
			return(ansi_readbyte_cb());
		case XPD_IO_COM:
			if(!comReadByte(xpd_info.io.com, &ch)) {
				if(comGetModemStatus(xpd_info.io.com)&COM_DCD==0)
					return(-2);
				return(-1);
			}
			return(ch);
		case XPD_IO_SOCKET:
			{
				BOOL	can_read;

				if(!socket_check(xpd_info.io.sock, &can_read, NULL, 1000))
					return(-2);
				if(can_read) {
					if(recv(xpd_info.io.sock, &ch, 1, 0)==1)
						return(ch);
					return(-2);
				}
				return(-1);
			}
		case XPD_IO_TELNET:
			{
				BOOL	can_read;

				// TODO: Proper telnet stuff...
				if(!socket_check(xpd_info.io.telnet.sock, &can_read, NULL, 1000))
					return(-2);
				if(can_read) {
					if(recv(xpd_info.io.telnet.sock, &ch, 1, 0)==1)
						return(ch);
					return(-2);
				}
				return(-1);
			}
	}
	return(-2);
}

static int dummy_writebyte_cb(const unsigned char ch)
{
	return(ch);
}

static int xpd_ansi_writebyte_cb(const unsigned char ch)
{
	switch(xpd_info.io_type) {
		case XPD_IO_STDIO:
			return(ansi_writebyte_cb(ch));
		case XPD_IO_COM:
			if(comWriteByte(xpd_info.io.com, ch))
				return(1);
			return(-1);
		case XPD_IO_SOCKET:
			{
				BOOL	can_write;

				if(!socket_check(xpd_info.io.sock, NULL, &can_write, 580000))
					return(-2);
				if(can_write) {
					if(send(xpd_info.io.sock, &ch, 1, 0)==1)
						return(ch);
					return(-2);
				}
				return(-1);
			}
		case XPD_IO_TELNET:
			{
				BOOL	can_write;

				// TODO: Proper telnet stuff...
				if(!socket_check(xpd_info.io.sock, NULL, &can_write, 580000))
					return(-2);
				if(can_write) {
					if(send(xpd_info.io.sock, &ch, 1, 0)==1)
						return(ch);
					return(-2);
				}
				return(-1);
			}
	}
	return(-2);
}

static int xpd_ansi_initio_cb(void)
{
	switch(xpd_info.io_type) {
		case XPD_IO_STDIO:
			return(ansi_initio_cb());
		case XPD_IO_COM:
		case XPD_IO_SOCKET:
		case XPD_IO_TELNET:
			// TODO: Init stuff... (enable Nagle, Disable non-blocking, etc)
			break;
	}
	return(0);
}

static int xpd_ansi_writestr_cb(const unsigned char *str, size_t len)
{
	int i;

	// TODO: Quit being silly.
	switch(xpd_info.io_type) {
		case XPD_IO_STDIO:
			return(ansi_writestr_cb(str,len));
		case XPD_IO_COM:
		case XPD_IO_SOCKET:
		case XPD_IO_TELNET:
			for(i=0; i<len; i++) {
				if(xpd_ansi_writebyte_cb(str[i])<0)
					return(-2);
			}
	}
	return(-2);
}

void xpd_parse_cmdline(int argc, char **argv)
{
	int	i;

	for(i=0; i<argc; i++) {
		if(strncmp(argv[i],"--io-type=",10)==0) {
			if(stricmp(argv[i]+10, "stdio")==0)
				xpd_info.io_type=XPD_IO_STDIO;
			else if(stricmp(argv[i]+10, "com")==0)
				xpd_info.io_type=XPD_IO_COM;
			else if(stricmp(argv[i]+10, "socket")==0)
				xpd_info.io_type=XPD_IO_SOCKET;
			else if(stricmp(argv[i]+10, "telnet")==0)
				xpd_info.io_type=XPD_IO_TELNET;
			else if(stricmp(argv[i]+10, "local")==0)
				xpd_info.io_type=XPD_IO_LOCAL;
		}
		else if(strncmp(argv[i],"--io-com=",9)==0) {
			xpd_info.io_type=XPD_IO_COM;
			xpd_info.io.com=atoi(argv[i]+9);
		}
		else if(strncmp(argv[i],"--io-socket=",12)==0) {
			xpd_info.io_type=XPD_IO_SOCKET;
			xpd_info.io.sock=atoi(argv[i]+12);
		}
		else if(strncmp(argv[i],"--drop-path=",12)==0) {
			xpd_info.drop.path=strdup(argv[i]+12);
		}
	}
}

void xpd_exit()
{
	if(xpd_info.doorway_mode)
		ansi_ciolib_setdoorway(0);
}

int xpd_init()
{
	struct text_info	ti;

	if(xpd_info.io_type == XPD_IO_LOCAL) {
		if(initciolib(CIOLIB_MODE_AUTO))
			return(-1);
	}
	else {
		ciolib_ansi_readbyte_cb=xpd_ansi_readbyte_cb;
		ciolib_ansi_writebyte_cb=xpd_ansi_writebyte_cb;
		ciolib_ansi_initio_cb=xpd_ansi_initio_cb;
		ciolib_ansi_writestr_cb=xpd_ansi_writestr_cb;
		if(initciolib(CIOLIB_MODE_ANSI))
			return(-1);
	}
	gettextinfo(&ti);
	cterm = cterm_init(ti.screenheight, ti.screenwidth, 1, 1, 0, NULL, CTERM_EMULATION_ANSI_BBS);
	return 0;
}

void xpd_doorway(int enable)
{
	xpd_info.doorway_mode=enable;
	ansi_ciolib_setdoorway(enable);
}

int xpd_rwrite(const char *data, int data_len)
{
	struct text_info	ti;

	if(data_len == -1)
		data_len=strlen(data);

	/* Set up cterm to match conio */
	gettextinfo(&ti);
	cterm->xpos=ti.winleft+ti.curx-1;
	cterm->ypos=ti.wintop+ti.cury-1;
	cterm->attr=ti.attribute;
	cterm->quiet=TRUE;
	cterm->doorway_mode=xpd_info.doorway_mode;

	/* Disable ciolib output for ANSI */
	ciolib_ansi_writebyte_cb=dummy_writebyte_cb;

	/* Send data to cterm */
	cterm_write(cterm, (char *)data, data_len, NULL, 0, NULL);

	/* Send data to remote */
	xpd_ansi_writestr_cb((char *)data,data_len);

	/* Set conio to match cterm */
	gotoxy(cterm->xpos-ti.winleft+1, cterm->ypos-ti.winright+1);
	textattr(cterm->attr);

	/* Re-enable ciolib output */
	ciolib_ansi_writebyte_cb=xpd_ansi_writebyte_cb;
	return 0;
}
