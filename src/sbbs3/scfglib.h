/* Synchronet configuration library routine prototypes */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _SCFGLIB_H
#define _SCFGLIB_H

#include "scfgdefs.h"	/* scfg_t */

#define get_int(var,stream) { if(!fread(&var,1,sizeof(var),stream)) \
								memset(&var,0,sizeof(var)); \
							  offset+=sizeof(var); }
#define get_str(var,stream) { if(!fread(var,1,sizeof(var),stream)) \
								memset(var,0,sizeof(var)); \
 							  else var[sizeof(var)-1] = 0; \
                              offset+=sizeof(var); }

#ifdef __cplusplus
extern "C" {
#endif

char*	get_alloc(long *offset, char *outstr, int maxlen, FILE *instream);
BOOL	allocerr(FILE*, char* error, size_t maxerrlen, long offset, const char *fname, size_t size);
char*	readline(long *offset, char *str, int maxlen, FILE *stream);

BOOL	read_node_cfg(scfg_t* cfg, char* error, size_t);
BOOL	read_main_cfg(scfg_t* cfg, char* error, size_t);
BOOL	read_xtrn_cfg(scfg_t* cfg, char* error, size_t);
BOOL	read_file_cfg(scfg_t* cfg, char* error, size_t);
BOOL	read_msgs_cfg(scfg_t* cfg, char* error, size_t);
BOOL	read_chat_cfg(scfg_t* cfg, char* error, size_t);
BOOL	read_attr_cfg(scfg_t* cfg, char* error, size_t);
char*	prep_path(char* path); 
void	make_data_dirs(scfg_t* cfg);

void	free_node_cfg(scfg_t* cfg);
void	free_main_cfg(scfg_t* cfg);
void	free_xtrn_cfg(scfg_t* cfg);
void	free_file_cfg(scfg_t* cfg);
void	free_msgs_cfg(scfg_t* cfg);
void	free_chat_cfg(scfg_t* cfg);

long	aftol(char *str);              /* Converts flag string to long */
char*	ltoaf(long l, char *str);     /* Converts long to flag string */
uint	attrstr(char *str);		/* Convert ATTR string into attribute int */

int		getdirnum(scfg_t*, const char* code);
int		getlibnum(scfg_t*, const char* code);
int		getsubnum(scfg_t*, const char* code);
int		getgrpnum(scfg_t*, const char* code);

faddr_t* nearest_sysfaddr(scfg_t*, faddr_t*);

#ifdef __cplusplus
}
#endif

#endif	/* Don't add anything after this line */
