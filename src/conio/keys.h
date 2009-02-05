/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2004 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#define CIO_KEY_HOME      (0x47 << 8)
#define CIO_KEY_UP        (72   << 8)
#define CIO_KEY_END       (0x4f << 8)
#define CIO_KEY_DOWN      (80   << 8)
#define CIO_KEY_F(x)      ((x<11)?((0x3a+x) << 8):((0x7a+x) << 8))
#define CIO_KEY_IC        (0x52 << 8)
#define CIO_KEY_DC        (0x53 << 8)
#define CIO_KEY_LEFT      (0x4b << 8)
#define CIO_KEY_RIGHT     (0x4d << 8)
#define CIO_KEY_PPAGE     (0x49 << 8)
#define CIO_KEY_NPAGE     (0x51 << 8)
#define CIO_KEY_ALT_F(x)      ((x<11)?((0x67+x) << 8):((0x80+x) << 8))

#define CIO_KEY_MOUSE     0x7d00	// This is the right mouse on Schneider/Amstrad PC1512 PC keyboards
#define CIO_KEY_ABORTED   0x0100	// ESC key by scancode
