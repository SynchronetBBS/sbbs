/* scfgdefs.h */

/* Synchronet configuration structure (scfg_t) definition */

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

#include "sbbsdefs.h"

#ifndef _SCFGDEFS_H_
#define _SCFGDEFS_H_

#include "startup.h"

typedef struct 
{

	grp_t		**grp;				/* Each message group */
	ushort		total_grps; 		/* Total number of groups */
	sub_t		**sub;				/* Each message sub */
	ushort		total_subs; 		/* Total number of subs */
	lib_t		**lib;				/* Each library */
	ushort		total_libs; 		/* Total number of libraries */
	dir_t		**dir;				/* Each message directory */
	ushort		total_dirs; 		/* Total number of directories */
	txtsec_t 	**txtsec;			/* Each text section */
	ushort		total_txtsecs;		/* Total number of text sections */
	xtrnsec_t	**xtrnsec;			/* Each external section */
	ushort		total_xtrnsecs; 	/* Total number of external sections */
	xtrn_t		**xtrn; 			/* Each external program */
	ushort		total_xtrns;		/* Total number of externals */
	mdm_result_t *mdm_result;		/* Each Modem Result Code */
	ushort		mdm_results;		/* Total number of Modem Results */
	prot_t		**prot; 			/* Each Transfer Protocol */
	ushort		total_prots;		/* Total Transfer Protocols */
	fextr_t		**fextr;			/* Each extractable file type */
	ushort		total_fextrs;		/* Total extractable file types */
	fcomp_t		**fcomp;			/* Each compressable file type */
	ushort		total_fcomps;		/* Total */
	fview_t		**fview;			/* Each veiwable file type */
	ushort		total_fviews;		/* Total viewable file types */
	ftest_t		**ftest;			/* Each testable file type */
	ushort		total_ftests;		/* Total testable file types */
	xedit_t		**xedit;			/* Each external editor */
	ushort		total_xedits;		/* Total external editors */
	qhub_t		**qhub; 			/* QWK network hubs */
	ushort		total_qhubs;		/* Total qwk network hubs */
	phub_t		**phub; 			/* PostLink/PCRelay network hubs */
	ushort		total_phubs;		/* Total PostLink/PCRelay hubs */
	chan_t		**chan; 			/* Each chat channel */
	ushort		total_chans;		/* Total number of chat channels */
	chatact_t	**chatact;			/* Chat action commands */
	ushort       total_chatacts;     /* Total number of action commands */
	actset_t 	**actset;			/* Name of action set */
	ushort		total_actsets;		/* Total number of action sets */
	page_t		**page; 			/* External chat page */
	ushort		total_pages;		/* Total number of external pages */
	event_t		**event;			/* Timed events */
	ushort		total_events;		/* Total number of timed events */
	dlevent_t	**dlevent;			/* Download events */
	ushort		total_dlevents; 	/* Total download events */
	faddr_t		*faddr; 			/* FidoNet addresses */
	ushort		total_faddrs;		/* Total number of fido addresses */
	swap_t		**swap; 			/* Swapping externals */
	ushort		total_swaps;		/* Total number of non-swap xtrns */
	natvpgm_t 	**natvpgm;			/* Native (32-bit) Programs */
	ushort		total_natvpgms; 	/* Total number of native pgms */
	guru_t		**guru; 			/* Gurus */
	ushort		total_gurus;		/* Total number of guru files */
	shell_t		**shell;			/* Command shells */
	ushort		total_shells;		/* Total number of command shells */

									/* COM port registers: */
	ushort		com_base,			/* COM base address */
				com_irq;			/* irq line number	   */
	ulong		com_rate;			/* DTE rate in bps	   */
	char  		com_port;			/* Number of COM port  */

									/* Modem command strings */
	char 	mdm_init[64],			/* Initialization */
			mdm_spec[64],		/* Special Initialization */
			mdm_term[64],		/* Terminal Initialization String */
			mdm_dial[64],		/* Dial */
			mdm_offh[64],		/* Off hook */
			mdm_answ[64],		/* Answer */
			mdm_hang[64];		/* Hang-up */
	ulong	mdm_misc;			/* Misc bits used for flags */
	ushort	mdm_reinit; 		/* Modem reinitialization minute count */
	ushort	mdm_ansdelay;		/* Modem seconds to delay after answer */
	uchar	mdm_rings;			/* Rings to wait till answer */

	long 	sys_misc;			/* System Misc Settings */
	char 	sys_pass[41];		/* System Pass Word */
	char 	sys_name[41];		/* System Name */
	char 	sys_id[9];			/* System ID for QWK Packets */
	char 	sys_psname[13]; 	/* PostLink and PCRelay Site Name */
	ulong	sys_psnum;			/* PostLink and PCRelay Site Number */
	char 	sys_inetaddr[128];	/* System's internet address */
	char 	sys_location[41];	/* System Location */
	short	sys_timezone;		/* Time Zone of BBS */
	char 	sys_daily[LEN_CMD+1];	   /* Daily event */
	char 	sys_logon[LEN_CMD+1];	   /* Logon event */
	char 	sys_logout[LEN_CMD+1];	   /* Logoff event */
	ushort	sys_pwdays; 		/* Max days between password change */
	ushort	sys_deldays;		/* Days to keep deleted users */
	ushort	sys_autodel;		/* Autodeletion after x days inactive */
	ushort	sys_nodes;			/* Number of nodes on system */
	char    sys_op[41];         /* Name of system operator */
	char    sys_guru[41];       /* Name of system guru */
	uchar	sys_exp_warn;		/* Days left till expire to notify */
	char 	sys_def_stat;		/* Default status line */
	char 	sys_phonefmt[LEN_PHONE+1];	/* format of phone numbers */
	ushort	sys_lastnode;		/* Last displayable node number */
	ushort	sys_autonode;		/* First node number for autonode */
	#ifdef SCFG
	char	sys_chat_ar[LEN_ARSTR+1];	/* chat override */
	#else
	uchar	*sys_chat_ar;
	#endif

	char	node_comspec[LEN_CMD+1];	/* DOS COMMAND.COM to use */
	char	node_editor[LEN_CMD+1]; /* Local text editor command line to use */
	char	node_viewer[LEN_CMD+1]; /* Local text viewer command line */
	char	node_daily[LEN_CMD+1];	/* Name of node's daily event */
	uchar	node_scrnlen;		/* Length of screen (rows) */
	uchar	node_scrnblank; 	/* Min of inactivity for blank screen */
	ulong	node_misc;			/* Misc bits for node setup */
	ushort	node_valuser;		/* User validation mail goes to */
	ushort	node_ivt;			/* Time-slice APIs */
	uchar	node_swap;			/* Swap types allowed */
	char	node_swapdir[LEN_DIR+1];	/* Swap directory */
	ushort	node_minbps;		/* Minimum connect rate of this node */
	ushort	node_num;			/* Local node number of this node */
	char	node_phone[13], 	/* Phone number of this node */
					node_name[41];     	/* Name of this node */
	#ifdef SCFG
	char	node_ar[LEN_ARSTR+1]; /* Node minimum requirements */
	#else
	uchar	*node_ar;
	#endif
	ulong	node_cost;			/* Node cost to call - in credits */
	uchar	node_dollars_per_call;	/* Billing Node Dollars Per Call */
	ushort	node_sem_check; 	/* Seconds between semaphore checks */
	ushort	node_stat_check;	/* Seconds between statistic checks */

	char 	new_pass[41];		/* New User Password */
	char 	new_magic[21];		/* New User Magic Word */
	char 	new_sif[9]; 		/* New User SIF Questionaire */
	char 	new_sof[9]; 		/* New User SIF Questionaire output SIF */
	char 	new_level;			/* New User Main Level */
	ulong	new_flags1; 		/* New User Main Flags from set #1*/
	ulong	new_flags2; 		/* New User Main Flags from set #2*/
	ulong	new_flags3; 		/* New User Main Flags from set #3*/
	ulong	new_flags4; 		/* New User Main Flags from set #4*/
	ulong	new_exempt;			/* New User Exemptions */
	ulong	new_rest;			/* New User Restrictions */
	ulong	new_cdt;			/* New User Credits */
	ulong	new_min;			/* New User Minutes */
	char	new_xedit[9];		/* New User Default Editor */
	ushort	new_shell;			/* New User Default Command Set */
	ulong	new_misc;			/* New User Miscellaneous Defaults */
	ushort	new_expire; 		/* Expiration days for new user */
	uchar	new_prot;			/* New User Default Download Protocol */
	char 	val_level[10];		/* Validation User Main Level */
	ulong	val_flags1[10]; 	/* Validation User Flags from set #1*/
	ulong	val_flags2[10]; 	/* Validation User Flags from set #2*/
	ulong	val_flags3[10]; 	/* Validation User Flags from set #3*/
	ulong	val_flags4[10]; 	/* Validation User Flags from set #4*/
	ulong	val_exempt[10]; 	/* Validation User Exemption Flags */
	ulong	val_rest[10];		/* Validation User Restriction Flags */
	ulong	val_cdt[10];		/* Validation User Additional Credits */
	ushort	val_expire[10]; 	/* Validation User Extend Expire #days */
	uchar	level_expireto[100];
	ushort	level_timepercall[100], /* Security level settings */
			level_timeperday[100],
			level_callsperday[100],
			level_linespermsg[100],
			level_postsperday[100],
			level_emailperday[100];
	long 	level_freecdtperday[100];
	long 	level_misc[100];
	char 	expired_level;	/* Expired user's ML */
	ulong	expired_flags1; /* Flags from set #1 to remove when expired */
	ulong	expired_flags2; /* Flags from set #2 to remove when expired */
	ulong	expired_flags3; /* Flags from set #3 to remove when expired */
	ulong	expired_flags4; /* Flags from set #4 to remove when expired */
	ulong	expired_exempt; /* Exemptions to remove when expired */
	ulong	expired_rest;	/* Restrictions to add when expired */
	ushort	min_dspace; 	/* Minimum amount of free space for uploads */
	ushort	max_batup;		/* Max number of files in upload queue */
	ushort	max_batdn;		/* Max number of files in download queue */
	ushort	max_userxfer;	/* Max dest. users of user to user xfer */
	ulong	max_minutes;	/* Maximum minutes a user can have */
	ulong	max_qwkmsgs;	/* Maximum messages per QWK packet */
	#ifdef SCFG
	char	preqwk_ar[LEN_ARSTR+1]; /* pre pack QWK */
	#else
	uchar	*preqwk_ar;
	#endif
	ushort	cdt_min_value;	/* Minutes per 100k credits */
	ulong	cdt_per_dollar; /* Credits per dollar */
	ushort	cdt_up_pct; 	/* Pct of credits credited on uploads */
	ushort	cdt_dn_pct; 	/* Pct of credits credited per download */
	char 	node_dir[LEN_DIR+1];
	char 	ctrl_dir[LEN_DIR+1];
	char 	data_dir[LEN_DIR+1];
	char 	text_dir[LEN_DIR+1];
	char 	exec_dir[LEN_DIR+1];
	char 	temp_dir[LEN_DIR+1];
	char 	**node_path;		/* paths to all node dirs */
	ushort	sysop_dir;			/* Destination for uploads to sysop */
	ushort	user_dir;			/* Directory for user to user xfers */
	ushort	upload_dir; 		/* Directory where all uploads go */
	char 	**altpath;			/* Alternate paths for files */
	ushort	altpaths;			/* Total number of alternate paths */
	ushort	leech_pct;			/* Leech detection percentage */
	ushort	leech_sec;			/* Minimum seconds before possible leech */
	ulong	netmail_cost;		/* Cost in credits to send netmail */
	char 	netmail_dir[LEN_DIR+1];    /* Directory to store netmail */
	ushort	netmail_misc;		/* Miscellaneous bits regarding netmail */
	ulong	inetmail_misc;		/* Miscellaneous bits regarding inetmail */
	ulong	inetmail_cost;		/* Cost in credits to send Internet mail */
	char	inetmail_sem[LEN_DIR+1];	/* Internet Mail semaphore file */
	char 	echomail_dir[LEN_DIR+1];   /* Directory to store echomail in */
	char 	fidofile_dir[LEN_DIR+1];   /* Directory where inbound files go */
	char 	netmail_sem[LEN_DIR+1];    /* FidoNet NetMail semaphore */
	char 	echomail_sem[LEN_DIR+1];   /* FidoNet EchoMail semaphore  */
	char 	origline[51];		/* Default EchoMail origin line */
	char 	qnet_tagline[128];	/* Default QWK Network tagline */
	long 	uq; 					/* User Questions */
	ulong	mail_maxcrcs;			/* Dupe checking in e-mail */
	ushort	mail_maxage;			/* Maximum age of e-mail */
	faddr_t	dflt_faddr; 			/* Default FidoNet address */
	char	logon_mod[9];			/* Logon module */
	char	logoff_mod[9];			/* Logoff module */
	char	newuser_mod[9]; 		/* New User Module */
	char	login_mod[9];			/* Login module */
	char	logout_mod[9];			/* Logout module */
	char	sync_mod[9];			/* Synchronization module */
	char	expire_mod[9];			/* User expiration module */
	char	scfg_cmd[LEN_CMD+1];	/* SCFG command line */
	uchar	smb_retry_time; 		/* Seconds to retry on SMBs */
	ushort	sec_warn;				/* Seconds before inactivity warning */
	ushort	sec_hangup; 			/* Seconds before inactivity hang-up */

	char 	color[TOTAL_COLORS];	/* Different colors for the BBS */

	#ifndef SCFG

	char	data_dir_subs[128]; 	/* DATA\SUBS directory */
	char    data_dir_dirs[128];     /* DATA\DIRS directory */

	#endif

	#ifdef SCFG

	char 	wfc_cmd[10][LEN_CMD+1];    /* 0-9 WFC DOS commands */
	char 	wfc_scmd[12][LEN_CMD+1];   /* F1-F12 WFC shrinking DOS commands */

	#else

	char 	*wfc_cmd[10];		/* 0-9 WFC DOS commands */
	char     *wfc_scmd[12];      /* F1-F12 WFC shrinking DOS commands */

	#endif

	bbs_startup_t*	startup;

} scfg_t;

#endif /* Don't add anything after this line */