/* scfg.h */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2002 Rob Swindell - http://www.synchro.net/copyright.html		*
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

/********************/
/* Global Variables */
/********************/
extern scfg_t cfg;
extern uifcapi_t uifc;
extern char item;
extern char **opt;
extern char tmp[256];
extern char error[256];
extern char *nulstr;
extern char *invalid_code,*num_flags;
extern int	backup_level;
extern BOOL new_install;

/***********************/
/* Function Prototypes */
/***********************/

void bail(int code);
void errormsg(int line, char *source,  char action, char *object, ulong access);
void clearptrs(int subnum);
int  save_changes(int mode);
void node_menu(void);
void node_cfg(void);
void results(int i);
void sys_cfg(void);
void net_cfg(void);
void msgs_cfg(void);
void msg_opts(void);
void sub_cfg(uint grpnum);
void xfer_cfg(void);
void xfer_opts(void);
void libs_cfg(void);
void dir_cfg(uint libnum);
void xprogs_cfg(void);
void fevents_cfg(void);
void tevents_cfg(void);
void xtrn_cfg(uint section);
void swap_cfg(void);
void xtrnsec_cfg(void);
int  natvpgm_cfg(void);
void page_cfg(void);
void xedit_cfg(void);
void txt_cfg(void);
void shell_cfg(void);
void init_mdms(void);
void guru_cfg(void);
void actsets_cfg(void);
void chan_cfg(void);
void mdm_cfg(int mdmnum);
int export_mdm(char *fname);
int code_ok(char *str);
int  bits(long l);
void getar(char *desc, char *ar);

/* Prepare a string to be used as an internal code */
char* prep_code(char *str);

/* scfgnet.h */
faddr_t atofaddr(char *str);
