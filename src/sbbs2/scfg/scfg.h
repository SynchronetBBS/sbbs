/* SCFG.H */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "uifc.h"
#include <dos.h>
#include <dir.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(__DPMI32__) || defined(__OS2__)
    #define far
	#define huge
#endif

#include "sbbs.h"
#include "scfglib.h"

/**********/
/* Macros */
/**********/

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
extern long freedosmem;
extern char item;
extern char **opt;
extern char tmp[256];
extern char *nulstr;
extern char **mdm_type;
extern char **mdm_file;
extern int	mdm_types;
extern char *invalid_code,*num_flags;
extern int	backup_level;

extern read_cfg_text_t txt;

/***********************/
/* Function Prototypes */
/***********************/

int  save_changes(int mode);
void node_menu();
void node_cfg();
void results(int i);
void sys_cfg();
void net_cfg();
void msgs_cfg();
void sub_cfg(uint grpnum);
void xfer_cfg();
void libs_cfg();
void dir_cfg(uint libnum);
void xprogs_cfg();
void fevents_cfg();
void tevents_cfg();
void xtrn_cfg();
void swap_cfg();
void xtrnsec_cfg();
void page_cfg();
void xedit_cfg();
void txt_cfg();
void shell_cfg();
void read_node_cfg(read_cfg_text_t txt);
void write_node_cfg();
void write_main_cfg();
void write_msgs_cfg();
void write_file_cfg();
void write_xtrn_cfg();
void write_chat_cfg();
void init_mdms();
void mdm_cfg(int mdmnum);
int export_mdm(char *fname);
char code_ok(char *str);
int  bits(long l);
char oneflag(long l);
void getar(char *desc, char *ar);
char *ultoac(ulong l,char *str);
FILE *fnopen(int *file, char *str, int access);
