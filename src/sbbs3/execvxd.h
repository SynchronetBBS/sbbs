/* execvxd.h */

/* Synchronet FOSSIL driver (VxD) for Windows 9x API */

/* $Id: execvxd.h,v 1.1.1.1 2000/10/10 11:26:23 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#define SBBSEXEC_VXD	"sbbsexec.vxd"

#define SBBSEXEC_IOCTL_START		0x8001
#define SBBSEXEC_IOCTL_COMPLETE		0x8002
#define SBBSEXEC_IOCTL_READ			0x8003
#define SBBSEXEC_IOCTL_WRITE		0x8004
#define SBBSEXEC_IOCTL_DISCONNECT	0x8005
#define SBBSEXEC_IOCTL_STOP			0x8006

#define SBBSEXEC_MODE_FOSSIL		(0)
#define SBBSEXEC_MODE_DOS_IN		(1<<0)
#define SBBSEXEC_MODE_DOS_OUT		(1<<1)

enum {
	 SBBSEXEC_ERROR_INUSE=1
	,SBBSEXEC_ERROR_INBUF
	,SBBSEXEC_ERROR_INDATA
	,SBBSEXEC_ERROR_IOCTL
	,SBBSEXEC_ERROR_OUTBUF
};

typedef struct {
	DWORD	mode;
	HANDLE	event;
} sbbsexec_start_t;

extern HANDLE exec_mutex;
