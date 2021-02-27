/* VARS.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/*************************************************************/
/* External (Global/Public) Variables for use only with SBBS */
/*************************************************************/

#include <stdio.h>
#include <dos.h>

#ifndef GLOBAL
#define GLOBAL
unsigned _stklen=22000; 			/* Set stack size in code, not header */
									/* 20k, NOT enough */
#endif

#include "sbbsdefs.h"
#include "smbdefs.h"
#include "text.h"

GLOBAL char 	*envp[128]; 	/* Original environment */

GLOBAL char 	menu_dir[128];	/* Over-ride default menu dir */
GLOBAL char 	menu_file[128]; /* Over-ride menu file */

GLOBAL user_t	useron; 		/* User currently online */
GLOBAL node_t	thisnode;		/* Node information */
GLOBAL smb_t	smb;			/* Currently open message base */

								/* Batch download queue */
GLOBAL char 	**batdn_name;	/* Filenames */
GLOBAL ushort	*batdn_alt; 	/* Alternate path */
GLOBAL uint 	*batdn_dir, 	/* Directory for each file */
				batdn_total;	/* Total files */
GLOBAL long 	*batdn_offset;	/* Offset for data */
GLOBAL ulong	*batdn_size;	/* Size of file in bytes */
GLOBAL ulong	*batdn_cdt; 	/* Credit value of file */

								/* Batch upload queue */
GLOBAL char 	**batup_desc,	/* Description for each file */
				**batup_name,	/* Filenames */
				*batup_misc;	/* Miscellaneous bits */
GLOBAL ushort	*batup_alt; 	/* Alternate path */
GLOBAL uint 	*batup_dir, 	/* Directory for each file */
				batup_total;	/* Total files */

/*********************************/
/* Color Configuration Variables */
/*********************************/
GLOBAL char 	*text[TOTAL_TEXT];			/* Text from CTRL\TEXT.DAT */
GLOBAL char 	*text_sav[TOTAL_TEXT];		/* Text from CTRL\TEXT.DAT */
GLOBAL int		directvideo;	/* Turbo C's video flag - direct or bios */
GLOBAL char 	qoc;			/* Quit after one caller */
GLOBAL long 	freedosmem; 	/* Amount of free DOS memory */
GLOBAL char 	orgcmd[129];	/* Original command to execute bbs */
GLOBAL char 	dszlog[127];	/* DSZLOG enviornment variable */
GLOBAL char 	debug;			/* Flag to allow debug writes */
GLOBAL int		keybuftop,keybufbot;	/* Keyboard input buffer pointers */
GLOBAL char 	keybuf[KEY_BUFSIZE];	/* Keyboard input buffer */
GLOBAL char 	connection[LEN_MODEM+1];/* Connection Description */
GLOBAL ulong	cur_rate;		/* Current Connection (DCE) Rate */
GLOBAL ulong	cur_cps;		/* Current Average Transfer CPS */
GLOBAL ulong	dte_rate;		/* Current COM Port (DTE) Rate */
GLOBAL time_t 	timeout;		/* User inactivity timeout reference */
GLOBAL char 	timeleft_warn;	/* low timeleft warning flag */
GLOBAL char 	curatr; 		/* Current Text Attributes Always */
GLOBAL long 	lncntr; 		/* Line Counter - for PAUSE */
GLOBAL long 	tos;			/* Top of Screen */
GLOBAL long 	rows;			/* Current Rows for User */
GLOBAL long 	autoterm;		/* Autodetected terminal type */
GLOBAL char 	slbuf[SAVE_LINES][LINE_BUFSIZE+1]; /* Saved for redisplay */
GLOBAL char 	slatr[SAVE_LINES];	/* Starting attribute of each line */
GLOBAL char 	slcnt;			/* Number of lines currently saved */
GLOBAL char 	lbuf[LINE_BUFSIZE+1];/* Temp storage for each line output */
GLOBAL int		lbuflen;		/* Number of characters in line buffer */
GLOBAL char 	latr;			/* Starting attribute of line buffer */
GLOBAL ulong	console;		/* Defines current Console settings */
GLOBAL char 	tmp[256];		/* Used all over as temp string */
GLOBAL char 	*nulstr;		/* Null string pointer */
GLOBAL char 	*crlf;			/* CRLF string pointer */
GLOBAL char 	wordwrap[81];	/* Word wrap buffer */
GLOBAL time_t	now,			/* Used to store current time in Unix format */
				answertime, 	/* Time call was answered */
				logontime,		/* Time user logged on */
				starttime,		/* Time stamp to use for time left calcs */
				ns_time,		/* File new-scan time */
				last_ns_time;	/* Most recent new-file-scan this call */
GLOBAL uchar 	action;			/* Current action of user */
GLOBAL char 	statline;		/* Current Status Line number */
GLOBAL long 	online; 		/* Remote/Local or not online */
GLOBAL long 	sys_status; 	/* System Status */
GLOBAL char 	*sub_misc;		/* Save misc and ptrs for subs */
GLOBAL ulong	*sub_ptr;		/* for fast pointer update */
GLOBAL ulong	*sub_last;		/* last read message pointer */

GLOBAL ulong	logon_ulb,		/* Upload Bytes This Call */
				logon_dlb,		/* Download Bytes This Call */
				logon_uls,		/* Uploads This Call */
				logon_dls,		/* Downloads This Call */
				logon_posts,	/* Posts This Call */
				logon_emails,	/* Emails This Call */
				logon_fbacks;	/* Feedbacks This Call */
GLOBAL uchar	logon_ml;		/* ML of the user apon logon */

GLOBAL int 		node_disk;		/* Number of Node's disk */
GLOBAL uint 	main_cmds;		/* Number of Main Commands this call */
GLOBAL uint 	xfer_cmds;		/* Number of Xfer Commands this call */
GLOBAL ulong	posts_read; 	/* Number of Posts read this call */
GLOBAL char 	temp_uler[31];  /* User who uploaded the files to temp dir */
GLOBAL char 	temp_file[41];	/* Origin of extracted temp files */
GLOBAL long 	temp_cdt;		/* Credit value of file that was extracted */
GLOBAL char 	autohang;		/* Used for auto-hangup after transfer */
GLOBAL char 	cap_fname[41];	/* Capture filename - default is CAPTURE.TXT */
GLOBAL FILE 	*capfile;		/* File string to use for capture file */
GLOBAL int 		inputfile;		/* File handle to use for input */
GLOBAL int 		logfile; 		/* File handle for node.log */
GLOBAL int 		nodefile;		/* File handle for node.dab */
GLOBAL int		node_ext;		/* File handle for node.exb */
GLOBAL char 	logcol; 		/* Current column of log file */
GLOBAL uint 	criterrs; 		/* Critical error counter */
GLOBAL struct date date;		/* Used for DOS compatible date pointer */
GLOBAL struct time curtime; 	/* Used for DOS compatible time pointer */

GLOBAL uint 	curgrp, 		/* Current group */
				*cursub,		/* Current sub-board for each group */
				curlib, 		/* Current library */
				*curdir;		/* Current directory for each library */
GLOBAL uint 	*usrgrp,		/* Real group numbers */
				usrgrps;		/* Number groups this user has access to */
GLOBAL uint 	*usrlib,		/* Real library numbers */
				usrlibs;		/* Number of libs this user can access */
GLOBAL uint 	**usrsub,		/* Real sub numbers */
				*usrsubs;		/* Num of subs with access for each grp */
GLOBAL uint 	**usrdir,		/* Real dir numbers */
				*usrdirs;		/* Num of dirs with access for each lib */
GLOBAL int		cursubnum;		/* For ARS */
GLOBAL int		curdirnum;		/* For ARS */
GLOBAL long 	timeleft;		/* Number of seconds user has left online */
GLOBAL uchar	sbbsnode[81];	/* Environment var to contain node dir path */
GLOBAL uchar	sbbsnnum[81];	/* Environment var to contain node num */
GLOBAL char 	*comspec;		/* Pointer to environment variable COMSPEC */
GLOBAL ushort	altul;			/* Upload to alternate path flag */
GLOBAL uint 	inDV;			/* DESQview version, or 0 if not under DV */
GLOBAL uchar	lastnodemsg;	/* Number of node last message was sent to */
GLOBAL char 	color[TOTAL_COLORS];	/* Different colors for the BBS */
GLOBAL time_t	next_event; 	/* Next event time - from front-end */
GLOBAL char 	lastuseron[LEN_ALIAS+1];  /* Name of user last online */
GLOBAL char 	cid[LEN_CID+1]; /* Caller ID of current caller */
GLOBAL uint 	emshandle;		/* EMS handle for overlay swap */
GLOBAL char 	emsver; 		/* Version of EMS installed */
GLOBAL char 	*noaccess_str;	/* Why access was denied via ARS */
GLOBAL long 	noaccess_val;	/* Value of parameter not met in ARS */
GLOBAL int		errorlevel; 	/* Error level of external program */
