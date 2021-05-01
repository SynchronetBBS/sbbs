/* Copyright (C), 2007 by Stephen Hurd */

/* $Id: bbslist.h,v 1.57 2020/06/27 08:27:39 deuce Exp $ */

#ifndef _BBSLIST_H_
#define _BBSLIST_H_

#include <stdio.h>  /* FILE * */
#include "gen_defs.h"
#include "dirwrap.h"    /* MAX_PATH */
#include "ini_file.h"
#include <time.h>

#if defined(_WIN32)
    #include <malloc.h> /* alloca() on Win32 */
#endif

#include <cterm.h>

#define LIST_NAME_MAX   30
#define LIST_ADDR_MAX   64
#define MAX_USER_LEN    30
#define MAX_PASSWD_LEN  128
#define MAX_SYSPASS_LEN 128

enum {
     USER_BBSLIST
    ,SYSTEM_BBSLIST
};

enum {
     BBSLIST_SELECT
    ,BBSLIST_EDIT
};

enum {
     SCREEN_MODE_CURRENT
    ,SCREEN_MODE_80X25
    ,SCREEN_MODE_80X28
    ,SCREEN_MODE_80X30
    ,SCREEN_MODE_80X43
    ,SCREEN_MODE_80X50
    ,SCREEN_MODE_80X60
    ,SCREEN_MODE_132X37
    ,SCREEN_MODE_132x52
    ,SCREEN_MODE_132X25
    ,SCREEN_MODE_132X28
    ,SCREEN_MODE_132X30
    ,SCREEN_MODE_132X34
    ,SCREEN_MODE_132X43
    ,SCREEN_MODE_132X50
    ,SCREEN_MODE_132X60
    ,SCREEN_MODE_C64
    ,SCREEN_MODE_C128_40
    ,SCREEN_MODE_C128_80
    ,SCREEN_MODE_ATARI
    ,SCREEN_MODE_ATARI_XEP80
    ,SCREEN_MODE_CUSTOM
    ,SCREEN_MODE_EGA_80X25
    ,SCREEN_MODE_TERMINATOR
};

enum {
     ADDRESS_FAMILY_UNSPEC
    ,ADDRESS_FAMILY_INET
    ,ADDRESS_FAMILY_INET6
};

enum {
	 RIP_VERSION_NONE
	,RIP_VERSION_1
	,RIP_VERSION_3
};

/* NOTE: changing this may require updating sort_order in bbslist.c */
struct bbslist {
    char            name[LIST_NAME_MAX+1];
    char            addr[LIST_ADDR_MAX+1];
    short unsigned int port;
    time_t          added;
    time_t          connected;
    unsigned int    calls;
    char            user[MAX_USER_LEN+1];
    char            password[MAX_PASSWD_LEN+1];
    char            syspass[MAX_SYSPASS_LEN+1];
    int             type;
    int             conn_type;
    int             id;
    int             screen_mode;
    int             nostatus;
    char            dldir[MAX_PATH+1];
    char            uldir[MAX_PATH+1];
    char            logfile[MAX_PATH+1];
    BOOL            append_logfile;
    int             xfer_loglevel;
    int             telnet_loglevel;
    int             bpsrate;
    int             music;
    int             address_family;
    char            font[80];
    int             hidepopups;
    char            ghost_program[9]; /* GHost program can only be 8 chars max. */
    int             rip;
    int             flow_control;
    char            comment[1024];
};

extern char *music_names[];
extern char music_helpbuf[];

struct bbslist *show_bbslist(char *current, int connected);
extern char *log_levels[];
extern char *rate_names[];
extern int rates[];
extern int sortorder[];
extern ini_style_t ini_style;
extern char *screen_modes[];
void read_item(str_list_t listfile, struct bbslist *entry, char *bbsname, int id, int type);
void read_list(char *listpath, struct bbslist **list, struct bbslist *defaults, int *i, int type);
void free_list(struct bbslist **list, int listcount);
void add_bbs(char *listpath, struct bbslist *bbs);
int edit_list(struct bbslist **list, struct bbslist *item,char *listpath,int isdefault);
int  get_rate_num(int rate);
cterm_emulation_t get_emulation(struct bbslist *bbs);
const char *get_emulation_str(cterm_emulation_t emu);
void get_term_size(struct bbslist *bbs, int *cols, int *rows);

#endif
