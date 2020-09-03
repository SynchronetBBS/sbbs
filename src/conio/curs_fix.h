/* $Id: curs_fix.h,v 1.6 2020/04/25 03:12:53 deuce Exp $ */

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

#ifdef __DARWIN__
 #define _XOPEN_SOURCE_EXTENDED 1
 #include <ncurses.h>
#else
 #define NCURSES_WIDECHAR 1
 #ifdef XCURSES
  #include <xcurses.h>
 #else
  #ifdef N_CURSES_LIB
   #include <ncurses.h>
  #else
   #ifdef DEBIAN_HATES_YOU
    #include <ncursesw/ncurses.h>
   #else
    #include <curses.h>
   #endif
  #endif
 #endif
#endif

#ifndef	ACS_SBSD
#define ACS_SBSD	ACS_SBSS
#endif

#ifndef ACS_DBDS
#define ACS_DBDS	ACS_SBSS
#endif

#ifndef ACS_BBDS
#define	ACS_BBDS	ACS_BBSS
#endif

#ifndef ACS_BBSD
#define ACS_BBSD	ACS_BBSS
#endif

#ifndef ACS_DBDD
#define	ACS_DBDD	ACS_SBSS
#endif

#ifndef	ACS_DBDB
#define	ACS_DBDB	ACS_SBSB
#endif

#ifndef ACS_BBDD
#define ACS_BBDD	ACS_BBSS
#endif

#ifndef ACS_DBBD
#define ACS_DBBD	ACS_SBBS
#endif

#ifndef ACS_DBBS
#define ACS_DBBS	ACS_SBBS
#endif

#ifndef ACS_SBBD
#define ACS_SBBD	ACS_SBBS
#endif

#ifndef ACS_SDSB
#define ACS_SDSB	ACS_SSSB
#endif

#ifndef	ACS_DSDB
#define ACS_DSDB	ACS_SSSB
#endif

#ifndef ACS_DDBB
#define	ACS_DDBB	ACS_SSBB
#endif

#ifndef ACS_BDDB
#define ACS_BDDB	ACS_BSSB
#endif

#ifndef ACS_DDBD
#define ACS_DDBD	ACS_SSBS
#endif

#ifndef ACS_BDDD
#define	ACS_BDDD	ACS_BSSS
#endif

#ifndef ACS_DDDB
#define ACS_DDDB	ACS_SSSB
#endif

#ifndef ACS_BDBD
#define ACS_BDBD	ACS_BSBS
#endif

#ifndef ACS_DDDD
#define	ACS_DDDD	ACS_SSSS
#endif

#ifndef ACS_SDBD
#define ACS_SDBD	ACS_SSBS
#endif

#ifndef ACS_DSBS
#define ACS_DSBS	ACS_SSBS
#endif

#ifndef ACS_BDSD
#define ACS_BDSD	ACS_BSSS
#endif

#ifndef	ACS_BSDS
#define ACS_BSDS	ACS_BSSS
#endif

#ifndef	ACS_DSBB
#define ACS_DSBB	ACS_SSBB
#endif
      
#ifndef ACS_SDBB
#define ACS_SDBB	ACS_SSBB
#endif

#ifndef ACS_BDSB
#define ACS_BDSB	ACS_BSSB
#endif

#ifndef ACS_BSDB
#define ACS_BSDB	ACS_BSSB
#endif

#ifndef ACS_DSDS
#define	ACS_DSDS	ACS_SSSS
#endif

#ifndef ACS_SDSD
#define	ACS_SDSD	ACS_SSSS
#endif

// Now wide...
#ifndef	WACS_SBSD
#define WACS_SBSD	WACS_SBSS
#endif

#ifndef WACS_DBDS
#define WACS_DBDS	WACS_SBSS
#endif

#ifndef WACS_BBDS
#define	WACS_BBDS	WACS_BBSS
#endif

#ifndef WACS_BBSD
#define WACS_BBSD	WACS_BBSS
#endif

#ifndef WACS_DBDD
#define	WACS_DBDD	WACS_SBSS
#endif

#ifndef	WACS_DBDB
#define	WACS_DBDB	WACS_SBSB
#endif

#ifndef WACS_BBDD
#define WACS_BBDD	WACS_BBSS
#endif

#ifndef WACS_DBBD
#define WACS_DBBD	WACS_SBBS
#endif

#ifndef WACS_DBBS
#define WACS_DBBS	WACS_SBBS
#endif

#ifndef WACS_SBBD
#define WACS_SBBD	WACS_SBBS
#endif

#ifndef WACS_SDSB
#define WACS_SDSB	WACS_SSSB
#endif

#ifndef	WACS_DSDB
#define WACS_DSDB	WACS_SSSB
#endif

#ifndef WACS_DDBB
#define	WACS_DDBB	WACS_SSBB
#endif

#ifndef WACS_BDDB
#define WACS_BDDB	WACS_BSSB
#endif

#ifndef WACS_DDBD
#define WACS_DDBD	WACS_SSBS
#endif

#ifndef WACS_BDDD
#define	WACS_BDDD	WACS_BSSS
#endif

#ifndef WACS_DDDB
#define WACS_DDDB	WACS_SSSB
#endif

#ifndef WACS_BDBD
#define WACS_BDBD	WACS_BSBS
#endif

#ifndef WACS_DDDD
#define	WACS_DDDD	WACS_SSSS
#endif

#ifndef WACS_SDBD
#define WACS_SDBD	WACS_SSBS
#endif

#ifndef WACS_DSBS
#define WACS_DSBS	WACS_SSBS
#endif

#ifndef WACS_BDSD
#define WACS_BDSD	WACS_BSSS
#endif

#ifndef	WACS_BSDS
#define WACS_BSDS	WACS_BSSS
#endif

#ifndef	WACS_DSBB
#define WACS_DSBB	WACS_SSBB
#endif

#ifndef WACS_SDBB
#define WACS_SDBB	WACS_SSBB
#endif

#ifndef WACS_BDSB
#define WACS_BDSB	WACS_BSSB
#endif

#ifndef WACS_BSDB
#define WACS_BSDB	WACS_BSSB
#endif

#ifndef WACS_DSDS
#define	WACS_DSDS	WACS_SSSS
#endif

#ifndef WACS_SDSD
#define	WACS_SDSD	WACS_SSSS
#endif
