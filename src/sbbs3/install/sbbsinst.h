/* sbbsinst.h */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#include <stdio.h>
#include <stdlib.h>

#include "conwrap.h"
#include "uifc.h"
#include "sbbs.h"

/**********/
/* Macros */
/**********/

#define SETHELP(where)  uifc.sethelp(where)

/*************/
/* Constants */
/*************/

#define SUB_HDRMOD	(1L<<31)		/* Modified sub-board header info */

/************/
/* Typedefs */
/************/
typedef struct {
	char install_path[256];
	BOOL usebcc;
	char cflags[256];
	BOOL release;
	BOOL symlink;
	BOOL cvs;
	char cvstag[256];
} params_t;

/********************/
/* Global Variables */
/********************/
extern uifcapi_t uifc;
extern char item;
extern char **opt;
extern char tmp[256];
extern char error[256];
extern char *nulstr;
extern char *invalid_code,*num_flags;
extern int	backup_level;

/***********************/
/* Function Prototypes */
/***********************/

void bail(int code);
void write_makefile(void);
void install_sbbs(void);
