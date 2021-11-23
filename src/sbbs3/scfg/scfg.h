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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>	/* USHRT_MAX */

#include "scfgsave.h"
#include "scfglib.h"
#include "date_str.h"
#include "str_util.h"
#include "gen_defs.h"
#include "smblib.h"
#include "load_cfg.h"
#include "ini_file.h"
#include "uifc.h"
#include "nopen.h"

/**********/
/* Macros */
/**********/

#define SETHELP(where)  uifc.sethelp(where)

#define SCFG_CMDLINE_PREFIX_HELP "\n"																				\
								"Command lines may begin with a special `prefix` character to indicate:\n"			\
								"\n"																				\
								"  `*`   Program is either a JavaScript (`.js`) or Baja (`.bin`) module\n"			\
								"  `?`   Program is a JavaScript (`.js`) module\n"

#define SCFG_CMDLINE_SPEC_HELP 	"\n"																				\
								"The following is a list of commonly-used command line specifiers:\n"				\
								"\n"																				\
								"  `%f`  The path/filename of the file to act upon or door/game `drop file`\n"		\
								"  `%s`  File specification (e.g. `*.txt`) or the current `Startup Directory`\n"	\
								"  `%.`  Executable file extension (`.exe`, or blank for Unix systems)\n"			\
								"  `%!`  The Synchronet `exec directory` (use `%@` for non-Unix only)\n"			\
								"  `%g`  The Synchronet `temp directory`\n"											\
								"  `%j`  The Synchronet `data directory`\n"											\
								"  `%k`  The Synchronet `ctrl directory`\n"											\
								"  `%z`  The Synchronet `text directory`\n"											\
								"  `%n`  The current `node directory`\n"											\
								"  `%#`  The current `node number`\n"												\
								"  `%a`  The current `user's alias`\n"												\
								"  `%1`  The current `user's number` (use `%2`, `%3`, etc. for 0-padded values)\n"	\
								"  `%h`  The current TCP/IP `socket` descriptor (handle) value\n"					\
								"  `%p`  The current connection type (protocol, e.g. `telnet`, `rlogin`, etc.)\n"	\
								"  `%r`  The current user's terminal height (`rows`)\n"								\
								"  `%w`  The current user's terminal width (`columns`)\n"							\
								"\n"																				\
								"For a complete list of the supported command-line specifiers, see:\n"				\
								"`http://wiki.synchro.net/config:cmdline`\n"

/*************/
/* Constants */
/*************/

#define SUB_HDRMOD	(1L<<31)		/* Modified sub-board header info */

#define MAX_UNIQUE_CODE_ATTEMPTS (36*36*36)

enum import_list_type {
	IMPORT_LIST_TYPE_SUBS_TXT,
	IMPORT_LIST_TYPE_QWK_CONTROL_DAT,
	IMPORT_LIST_TYPE_GENERIC_AREAS_BBS,
	IMPORT_LIST_TYPE_SBBSECHO_AREAS_BBS,
	IMPORT_LIST_TYPE_BACKBONE_NA,
	IMPORT_LIST_TYPE_BAD_AREAS,
	IMPORT_LIST_TYPE_ECHOSTATS,
	IMPORT_LIST_TYPE_NEWSGROUPS
};

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
extern const char *nulstr;
extern char *invalid_code,*num_flags;
extern int	backup_level;
extern BOOL new_install;
extern char* area_sort_desc[AREA_SORT_TYPES+1];

/***********************/
/* Function Prototypes */
/***********************/

void bail(int code);
void errormsg(int line, const char* function, const char *source, const char* action, const char *object, ulong access);
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
bool new_sub(unsigned new_subnum, unsigned group_num, sub_t* pasted_sub, long misc);
bool new_qhub_sub(qhub_t*, unsigned qsubnum, sub_t*, unsigned confnum);
void remove_sub(scfg_t*, unsigned subnum, bool cut);
void sort_subs(int grpnum);
void sort_dirs(int libnum);
unsigned subs_in_group(unsigned grpnum);
char random_code_char(void);
const char* io_method(uint32_t mode);
void choose_io_method(uint32_t* misc);
BOOL load_main_cfg(scfg_t*, char*, size_t);
BOOL load_node_cfg(scfg_t*, char*, size_t);
BOOL load_msgs_cfg(scfg_t*, char*, size_t);
BOOL load_file_cfg(scfg_t*, char*, size_t);
BOOL load_chat_cfg(scfg_t*, char*, size_t);
BOOL load_xtrn_cfg(scfg_t*, char*, size_t);
BOOL save_main_cfg(scfg_t*, int);
BOOL save_node_cfg(scfg_t*, int);
BOOL save_msgs_cfg(scfg_t*, int);
BOOL save_file_cfg(scfg_t*, int);
BOOL save_chat_cfg(scfg_t*, int);	
BOOL save_xtrn_cfg(scfg_t*, int);

long import_msg_areas(enum import_list_type, FILE*, unsigned grpnum, int min_confnum, int max_confnum
	, qhub_t*, const char* pkt_orig, faddr_t* faddr, uint32_t misc, long* added);

/* Prepare a string to be used as an internal code; Note: use the return value, Luke */
char* prep_code(char *str, const char* prefix);

/* scfgnet.h */
faddr_t atofaddr(char *str);
