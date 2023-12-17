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
#include "dllexport.h"

#ifdef __cplusplus
extern "C" {
#endif

BOOL	allocerr(char* error, size_t maxerrlen, const char* fname, const char *item, size_t size);
char*	readline(long *offset, char *str, int maxlen, FILE *stream);

DLLEXPORT BOOL	read_node_cfg(scfg_t* cfg, char* error, size_t);
DLLEXPORT BOOL	read_main_cfg(scfg_t* cfg, char* error, size_t);
DLLEXPORT BOOL	read_xtrn_cfg(scfg_t* cfg, char* error, size_t);
DLLEXPORT BOOL	read_file_cfg(scfg_t* cfg, char* error, size_t);
DLLEXPORT BOOL	read_msgs_cfg(scfg_t* cfg, char* error, size_t);
DLLEXPORT BOOL	read_chat_cfg(scfg_t* cfg, char* error, size_t);
DLLEXPORT BOOL	read_attr_cfg(scfg_t* cfg, char* error, size_t);
DLLEXPORT char*	prep_path(char* path);
DLLEXPORT char* prep_dir(const char* base, char* path, size_t);
DLLEXPORT void	make_data_dirs(scfg_t* cfg);

DLLEXPORT void	free_node_cfg(scfg_t* cfg);
DLLEXPORT void	free_main_cfg(scfg_t* cfg);
DLLEXPORT void	free_xtrn_cfg(scfg_t* cfg);
DLLEXPORT void	free_file_cfg(scfg_t* cfg);
DLLEXPORT void	free_msgs_cfg(scfg_t* cfg);
DLLEXPORT void	free_chat_cfg(scfg_t* cfg);

DLLEXPORT uint32_t aftou32(const char *str);      /* Converts flag string to uint32_t */
DLLEXPORT char*	u32toaf(uint32_t t, char *str); /* Converts uint32_t to flag string */
uint	attrstr(const char *str);		/* Convert ATTR string into attribute int */

int		getdirnum(scfg_t*, const char* code);
int		getlibnum(scfg_t*, const char* code);
int		getlibnum_from_name(scfg_t*, const char* name);
int		getsubnum(scfg_t*, const char* code);
int		getgrpnum(scfg_t*, const char* code);
int		getgrpnum_from_name(scfg_t*, const char* name);
int		getxtrnsec(scfg_t*, const char* code);
int		getgurunum(scfg_t*, const char* code);
int		getchatactset(scfg_t*, const char* name);
int		getxeditnum(scfg_t*, const char* code);
int		getshellnum(scfg_t*, const char* code);

DLLEXPORT BOOL	is_valid_dirnum(scfg_t*, int);
DLLEXPORT BOOL	is_valid_libnum(scfg_t*, int);
DLLEXPORT BOOL	is_valid_subnum(scfg_t*, int);
DLLEXPORT BOOL	is_valid_grpnum(scfg_t*, int);
DLLEXPORT BOOL	is_valid_xtrnsec(scfg_t*, int);

DLLEXPORT char *	trashcan_fname(scfg_t* cfg, const char *name, char* fname, size_t);
DLLEXPORT char *	twitlist_fname(scfg_t* cfg, char* fname, size_t);

DLLEXPORT char *	sub_newsgroup_name(scfg_t*, sub_t*, char*, size_t);
DLLEXPORT char *	sub_area_tag(scfg_t*, sub_t*, char*, size_t);
DLLEXPORT char *	dir_area_tag(scfg_t*, dir_t*, char*, size_t);

uint nearest_sysfaddr_index(scfg_t*, faddr_t*);
faddr_t* nearest_sysfaddr(scfg_t*, faddr_t*);

#ifdef __cplusplus
}
#endif

#endif	/* Don't add anything after this line */
