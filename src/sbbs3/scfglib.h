/* scfglib.h */

/* Synchronet configuration library routine prototypes */

/* $Id$ */

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

#ifndef _SCFGLIB_H
#define _SCFGLIB_H

#define SAVE_MEMORY

#define get_int(var,stream) { if(!fread(&var,1,sizeof(var),stream)) \
								memset(&var,0,sizeof(var)); \
							  offset+=sizeof(var); }
#define get_str(var,stream) { if(!fread(var,1,sizeof(var),stream)) \
								memset(var,0,sizeof(var)); \
                              offset+=sizeof(var); }

#define FREE_AND_NULL(x) if(x!=NULL) { FREE(x); x=NULL; }

typedef struct {
    char    *openerr,
            *reading,
            *readit,
            *allocerr,
            *error;
            } read_cfg_text_t;

#ifdef __cplusplus
extern "C" {
#endif

char *get_alloc(long *offset, char *outstr, int maxlen, FILE *instream);
BOOL allocerr(read_cfg_text_t* txt, long offset, char *fname, uint size);
char *readline(long *offset, char *str, int maxlen, FILE *stream);

BOOL read_node_cfg(scfg_t* cfg, read_cfg_text_t* txt);
BOOL read_main_cfg(scfg_t* cfg, read_cfg_text_t* txt);
BOOL read_xtrn_cfg(scfg_t* cfg, read_cfg_text_t* txt);
BOOL read_file_cfg(scfg_t* cfg, read_cfg_text_t* txt);
BOOL read_msgs_cfg(scfg_t* cfg, read_cfg_text_t* txt);
BOOL read_chat_cfg(scfg_t* cfg, read_cfg_text_t* txt);
BOOL read_attr_cfg(scfg_t* cfg, read_cfg_text_t* txt);
void prep_path(char* base, char* path);
void make_data_dirs(scfg_t* cfg);

void free_node_cfg(scfg_t* cfg);
void free_main_cfg(scfg_t* cfg);
void free_xtrn_cfg(scfg_t* cfg);
void free_file_cfg(scfg_t* cfg);
void free_msgs_cfg(scfg_t* cfg);
void free_chat_cfg(scfg_t* cfg);

long aftol(char *str);              /* Converts flag string to long */
char *ltoaf(long l, char *str);     /* Converts long to flag string */
char *faddrtoa(faddr_t addr);   /* FidoNet address to ASCII text conversion */
char chardupe(char *str);       /* Searches for duplicate chars in str */
uchar attrstr(char *str);		/* Convert ATTR string into attribute int */

#ifdef __cplusplus
}
#endif

#endif	/* Don't add anything after this line */
