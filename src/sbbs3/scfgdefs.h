/* Synchronet configuration structure (scfg_t) definition */

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

#include "sbbsdefs.h"

#ifndef _SCFGDEFS_H_
#define _SCFGDEFS_H_

typedef struct {							/* Message sub board info */
	char		code[LEN_EXTCODE+1];		/* Internal code (with optional lib prefix) */
	char		code_suffix[LEN_CODE+1];	/* Eight character code suffix */
	char		lname[LEN_SLNAME+1],		/* Long name - used for listing */
				sname[LEN_SSNAME+1],		/* Short name - used for prompts */
				arstr[LEN_ARSTR+1],			/* Access requirements */
				read_arstr[LEN_ARSTR+1],	/* Read requirements */
				post_arstr[LEN_ARSTR+1],	/* Post requirements */
				op_arstr[LEN_ARSTR+1], 		/* Operator requirements */
				mod_arstr[LEN_ARSTR+1],		/* Moderated user requirements */
				qwkname[11],				/* QWK name - only 10 chars */
				data_dir[LEN_DIR+1],		/* Data file directory */
				origline[51],				/* Optional EchoMail origin line */
				post_sem[LEN_DIR+1],		/* Semaphore file for this sub */
				tagline[81],				/* Optional QWK net tag line */
				newsgroup[LEN_DIR+1];		/* Newsgroup name */
	char		area_tag[41];
	uchar		ar[LEN_ARSTR+1],
				read_ar[LEN_ARSTR+1],
				post_ar[LEN_ARSTR+1],
				op_ar[LEN_ARSTR+1],
				mod_ar[LEN_ARSTR+1];
	uint16_t	grp,						/* Which group this sub belongs to */
				ptridx, 					/* Index into pointer file */
				qwkconf,					/* QWK conference number */
				maxage, 					/* Max age of messages (in days) */
				subnum;						/* ephemeral index of this sub in cfg.sub[] */
	uint32_t	misc,						/* Miscellaneous flags */
				maxmsgs,					/* Max number of messages allowed */
				maxcrcs;					/* Max number of CRCs to keep */
	int32_t		pmode;						/* printfile()/putmsg() mode flags */
	int32_t		n_pmode;					/* set of negated pmode flags */
	faddr_t		faddr;						/* FidoNet address */

} sub_t;

typedef struct {							/* Message group info */
	char		lname[LEN_GLNAME+1],		/* Long name */
				sname[LEN_GSNAME+1],		/* Short name */
				arstr[LEN_ARSTR+1],			/* Access requirements */
				code_prefix[LEN_CODE+1];	/* Prefix for internal code */
	uchar		ar[LEN_ARSTR+1];
	enum area_sort sort;

} grp_t;

typedef struct {							/* Transfer Directory Info */
	char		code[LEN_EXTCODE+1];		/* Internal code (with optional lib prefix) */
	char		code_suffix[LEN_CODE+1];	/* Eight character code suffix */
	char		area_tag[41];
	char		lname[LEN_SLNAME+1],		/* Long name - used for listing */
				sname[LEN_SSNAME+1],		/* Short name - used for prompts */
				arstr[LEN_ARSTR+1],			/* Access Requirements */
				ul_arstr[LEN_ARSTR+1], 		/* Upload Requirements */
				dl_arstr[LEN_ARSTR+1], 		/* Download Requirements */
				ex_arstr[LEN_ARSTR+1], 		/* Exemption Requirements (credits) */
				op_arstr[LEN_ARSTR+1], 		/* Operator Requirements */
				path[LEN_DIR+1],			/* Path to directory for files */
				exts[41],   		        /* Extensions allowed */
				upload_sem[LEN_DIR+1],		/* Upload semaphore file */
				data_dir[LEN_DIR+1];		/* Directory where data is stored */
	uchar		ar[LEN_ARSTR+1],
				ul_ar[LEN_ARSTR+1],
				dl_ar[LEN_ARSTR+1],
				ex_ar[LEN_ARSTR+1],
				op_ar[LEN_ARSTR+1],
				seqdev, 					/* Sequential access device number */
				sort;						/* Sort type */
	uint16_t	maxfiles,					/* Max number of files allowed */
				maxage, 					/* Max age of files (in days) */
				up_pct, 					/* Percentage of credits on uloads */
				dn_pct, 					/* Percentage of credits on dloads */
				lib,						/* Which library this dir is in */
				dirnum;						/* ephemeral index of this dir in cfg.dir[] */
	uint32_t	misc;						/* Miscellaneous bits */

} dir_t;

typedef struct {							/* Transfer Library Information */
	char		lname[LEN_GLNAME+1],		/* Long Name - used for listings */
				sname[LEN_GSNAME+1],		/* Short Name - used for prompts */
				arstr[LEN_ARSTR+1],			/* Access Requirements */
				code_prefix[LEN_CODE+1],	/* Prefix for internal code */
				parent_path[48];			/* Parent for dir paths */
	uchar		ar[LEN_ARSTR+1];
	uint32_t	offline_dir;				/* Offline file directory */
	uint32_t	misc;						/* Miscellaneous bits */
	enum area_sort sort;

} lib_t;

typedef struct {							/* Gfile Section Information */
	char		code[LEN_CODE+1];			/* Eight character code */
	char		name[41],					/* Name of section */
				arstr[LEN_ARSTR+1];			/* Access requirements */
	uchar		ar[LEN_ARSTR+1];

} txtsec_t;

typedef struct {							/* External Section Information */
	char		code[LEN_CODE+1];			/* Eight character code	*/
	char		name[41],					/* Name of section */
				arstr[LEN_ARSTR+1];			/* Access requirements */
	uchar		ar[LEN_ARSTR+1];

} xtrnsec_t;

typedef struct {							/* Swappable executable */
	char		cmd[LEN_CMD+1]; 			/* Program name */

} swap_t;

typedef struct {							/* OS/2 executable */
	char		name[13];					/* Program name */
	uint32_t	misc;						/* See OS2PGM_* */

} natvpgm_t;

typedef struct {							/* External Program Information */
	char		code[LEN_CODE+1];			/* Internal code for program */
	char		name[41],					/* Name of External */
				arstr[LEN_ARSTR+1],			/* Access Requirements */
				run_arstr[LEN_ARSTR+1],		/* Run Requirements */
				cmd[LEN_CMD+1], 			/* Command line */
				clean[LEN_CMD+1],			/* Clean-up command line */
				path[LEN_DIR+1];			/* Start-up path */
	uchar		ar[LEN_ARSTR+1],
				run_ar[LEN_ARSTR+1];
	uchar		type,						/* What type of external program */
				event,                      /* Execute upon what event */
				textra, 					/* Extra time while in this program */
				maxtime;					/* Maximum time allowed in this door */
	uint16_t	sec;						/* Section this program belongs to */
	uint32_t	cost,						/* Cost to run in credits */
				misc;						/* Misc. bits - ANSI, DOS I/O etc. */

} xtrn_t;

typedef struct {							/* External Page program info */
	char		cmd[LEN_CMD+1], 			/* Command line */
				arstr[LEN_ARSTR+1];			/* ARS for this chat page */
	uchar		ar[LEN_ARSTR+1];
	uint32_t	misc;						/* Intercept I/O */

} page_t;


typedef struct {							/* Chat action set */
	char		name[26];					/* Name of set */

} actset_t;

typedef struct {							/* Chat action info */
	char		cmd[LEN_CHATACTCMD+1],		/* Command word */
				out[LEN_CHATACTOUT+1];		/* Output */
	uint16_t	actset; 					/* Set this action belongs to */

} chatact_t;

typedef struct {							/* Gurus */
	char		code[LEN_CODE+1];
	char		name[26],
				arstr[LEN_ARSTR+1];
	uchar		ar[LEN_ARSTR+1];

} guru_t;

typedef struct {							/* Chat Channel Information */
	char		code[LEN_CODE+1];
	char		name[26];					/* Channel description */
	char		arstr[LEN_ARSTR+1];			/* Access requirements */
	uchar		ar[LEN_ARSTR+1];
	uint16_t	actset, 					/* Set of actions used in this chan */
				guru;						/* Guru file number */
	uint32_t	cost,						/* Cost to join */
				misc;						/* Misc. bits CHAN_* definitions */

} chan_t;

typedef struct {							/* Modem Result codes info */
	uint16_t	code,						/* Numeric Result Code */
				cps,    		            /* Average Transfer CPS */
				rate;   		            /* DCE Rate (Modem to Modem) */
	char		str[LEN_MODEM+1];   		/* String to use for description */

} mdm_result_t;

typedef struct {							/* Transfer Protocol information */
	char		mnemonic;					/* Letter to select this protocol */
	char		name[26],					/* Name of protocol */
				arstr[LEN_ARSTR+1],			/* ARS */
				ulcmd[LEN_CMD+1],			/* Upload command line */
				dlcmd[LEN_CMD+1],			/* Download command line */
				batulcmd[LEN_CMD+1],		/* Batch upload command line */
				batdlcmd[LEN_CMD+1],		/* Batch download command line */
				blindcmd[LEN_CMD+1],		/* Blind upload command line */
				bicmd[LEN_CMD+1];			/* Bidirectional command line */
	uchar		ar[LEN_ARSTR+1];
	uint32_t	misc;						/* Miscellaneous bits */

} prot_t;

typedef struct {	                        /* Extractable file types */
	char		ext[4]; 					/* Extension */
	char		arstr[LEN_ARSTR+1],			/* Access Requirements */
				cmd[LEN_CMD+1]; 			/* Command line */
	uchar		ar[LEN_ARSTR+1];

} fextr_t;

typedef struct {							/* Compressable file types */
	char		ext[4]; 					/* Extension */
	char		arstr[LEN_ARSTR+1],			/* Access Requirements */
				cmd[LEN_CMD+1]; 			/* Command line */
	uchar		ar[LEN_ARSTR+1];

} fcomp_t;

typedef struct {							/* Viewable file types */
	char		ext[4]; 					/* Extension */
	char		arstr[LEN_ARSTR+1],			/* Access Requirements */
				cmd[LEN_CMD+1]; 			/* Command line */
	uchar		ar[LEN_ARSTR+1];

} fview_t;

typedef struct {							/* Testable file types */
	char		ext[4]; 					/* Extension */
	char		arstr[LEN_ARSTR+1],			/* Access requirement */
				cmd[LEN_CMD+1], 			/* Command line */
				workstr[41];				/* String to display while working */
	uchar		ar[LEN_ARSTR+1];

} ftest_t;

typedef struct {							/* Download events */
	char		ext[4];
	char		arstr[LEN_ARSTR+1],			/* Access requirement */
				cmd[LEN_CMD+1], 			/* Command line */
				workstr[41];				/* String to display while working */
	uchar		ar[LEN_ARSTR+1];

} dlevent_t;

enum xedit_soft_cr {						// What to do with so-called "Soft CRs"
	XEDIT_SOFT_CR_UNDEFINED,
	XEDIT_SOFT_CR_EXPAND,
	XEDIT_SOFT_CR_STRIP,
	XEDIT_SOFT_CR_RETAIN
};								

typedef struct {							/* External Editors */
	char		code[LEN_CODE+1],
				name[41],					/* Name (description) */
				arstr[LEN_ARSTR+1],			/* Access Requirement */
				lcmd[LEN_CMD+1],			/* Local command line */
				rcmd[LEN_CMD+1];			/* Remote command line */
	uchar		ar[LEN_ARSTR+1];
	uint32_t	misc;						/* Misc. bits */
	uchar		type;						/* Drop file type */
	uint16_t	quotewrap_cols;				/* When word-wrapping quoted text, use this width (if non-zero */
	enum xedit_soft_cr soft_cr;				// What to do with so-called "Soft CRs"

} xedit_t;


typedef struct {							/* Generic Timed Event */
	char			code[LEN_CODE+1],		/* Internal code */
					dir[LEN_DIR+1], 		/* Start-up directory */
					cmd[LEN_CMD+1]; 		/* Command line */
	uint8_t			days;					/* Week days to run event */
	uint16_t		node,					/* Node to execute event */
					time,					/* Time to run event */
					freq;					/* Frequency to run event */
	uint32_t		misc,					/* Misc bits */
					mdays;					/* Days of month (if non-zero) to run event */
	uint16_t		months;					/* Months (if non-zero) to run event */
	time32_t		last;					/* Last time event ran */
	uchar			errlevel;				/* Log level to use upon execution error */

} event_t;

typedef struct {							/* QWK Network Hub */
	char		id[LEN_QWKID+1],			/* System ID of Hub */
				*mode,						/* Mode for Ctrl-A codes for ea. sub */
				days,						/* Days to call-out on */
				call[LEN_CMD+1],			/* Call-out command line to execute */
				pack[LEN_CMD+1],			/* Packing command line */
				unpack[LEN_CMD+1];			/* Unpacking command line */
	char		fmt[4]; 					/* Archive format */
	uint16_t	time,						/* Time to call-out */
				node,						/* Node to do the call-out */
				freq,						/* Frequency of call-outs */
				subs,						/* Number Sub-boards carried */
				*conf;						/* Conference number of ea. */
	sub_t**		sub;
	time32_t	last;						/* Last network attempt */
	uint32_t	misc;						/* QHUB_* flags */

} qhub_t;

typedef struct {							/* PCRelay/PostLink Hub */
	char		days,						/* Days to call-out on */
				name[11],					/* Site Name of Hub */
				call[LEN_CMD+1];			/* Call-out command line to execute */
	uint16_t	time,						/* Time to call-out */
				node,						/* Node to do the call-out */
				freq;						/* Frequency of call-outs */
	time32_t	last;						/* Last network attempt */

} phub_t;


typedef struct {							/* Command Shells */
	char		code[LEN_CODE+1];
	char		name[41],					/* Name (description) */
				arstr[LEN_ARSTR+1];			/* Access Requirement */
	uchar		ar[LEN_ARSTR+1];
	uint32_t	misc;

} shell_t;

typedef struct {
	uchar		key;
	char		cmd[LEN_CMD+1];
} hotkey_t;

typedef struct 
{
	DWORD			size;				/* sizeof(scfg_t) */
	BOOL			prepped;			/* TRUE if prep_cfg() has been used */

	grp_t			**grp;				/* Each message group */
	uint16_t		total_grps; 		/* Total number of groups */
	sub_t			**sub;				/* Each message sub */
	uint16_t		total_subs; 		/* Total number of subs */
	lib_t			**lib;				/* Each library */
	uint16_t		total_libs; 		/* Total number of libraries */
	dir_t			**dir;				/* Each message directory */
	uint16_t		total_dirs; 		/* Total number of directories */
	txtsec_t 		**txtsec;			/* Each text section */
	uint16_t		total_txtsecs;		/* Total number of text sections */
	xtrnsec_t		**xtrnsec;			/* Each external section */
	uint16_t		total_xtrnsecs; 	/* Total number of external sections */
	xtrn_t			**xtrn; 			/* Each external program */
	uint16_t		total_xtrns;		/* Total number of externals */
	mdm_result_t	*mdm_result;		/* Each Modem Result Code */
	uint16_t		mdm_results;		/* Total number of Modem Results */
	prot_t			**prot; 			/* Each Transfer Protocol */
	uint16_t		total_prots;		/* Total Transfer Protocols */
	fextr_t			**fextr;			/* Each extractable file type */
	uint16_t		total_fextrs;		/* Total extractable file types */
	fcomp_t			**fcomp;			/* Each compressable file type */
	uint16_t		total_fcomps;		/* Total */
	fview_t			**fview;			/* Each veiwable file type */
	uint16_t		total_fviews;		/* Total viewable file types */
	ftest_t			**ftest;			/* Each testable file type */
	uint16_t		total_ftests;		/* Total testable file types */
	xedit_t			**xedit;			/* Each external editor */
	uint16_t		total_xedits;		/* Total external editors */
	qhub_t			**qhub; 			/* QWK network hubs */
	uint16_t		total_qhubs;		/* Total qwk network hubs */
	phub_t			**phub; 			/* PostLink/PCRelay network hubs */
	uint16_t		total_phubs;		/* Total PostLink/PCRelay hubs */
	chan_t			**chan; 			/* Each chat channel */
	uint16_t		total_chans;		/* Total number of chat channels */
	chatact_t		**chatact;			/* Chat action commands */
	uint16_t		total_chatacts;     /* Total number of action commands */
	actset_t 		**actset;			/* Name of action set */
	uint16_t		total_actsets;		/* Total number of action sets */
	page_t			**page; 			/* External chat page */
	uint16_t		total_pages;		/* Total number of external pages */
	event_t			**event;			/* Timed events */
	uint16_t		total_events;		/* Total number of timed events */
	dlevent_t		**dlevent;			/* Download events */
	uint16_t		total_dlevents; 	/* Total download events */
	faddr_t			*faddr; 			/* FidoNet addresses */
	uint16_t		total_faddrs;		/* Total number of fido addresses */
	swap_t			**swap; 			/* Swapping externals */
	uint16_t		total_swaps;		/* Total number of non-swap xtrns */
	natvpgm_t 		**natvpgm;			/* Native (not MS-DOS) Programs */
	uint16_t		total_natvpgms; 	/* Total number of native pgms */
	guru_t			**guru; 			/* Gurus */
	uint16_t		total_gurus;		/* Total number of guru files */
	shell_t			**shell;			/* Command shells */
	uint16_t		total_shells;		/* Total number of command shells */
	hotkey_t		**hotkey;			/* Global hot keys */
	uint16_t		total_hotkeys;		/* Total number of global hot keys */

										/* COM port registers: */
	uint16_t		com_base,			/* COM base address */
					com_irq;			/* irq line number	   */
	uint32_t		com_rate;			/* DTE rate in bps	   */
	char  			com_port;			/* Number of COM port  */

										/* Modem command strings */
	char 			mdm_init[64],		/* Initialization */
					mdm_spec[64],		/* Special Initialization */
					mdm_term[64],		/* Terminal Initialization String */
					mdm_dial[64],		/* Dial */
					mdm_offh[64],		/* Off hook */
					mdm_answ[64],		/* Answer */
					mdm_hang[64];		/* Hang-up */
	uint32_t		mdm_misc;			/* Misc bits used for flags */
	uint16_t		mdm_reinit; 		/* Modem reinitialization minute count */
	uint16_t		mdm_ansdelay;		/* Modem seconds to delay after answer */
	uchar			mdm_rings;			/* Rings to wait till answer */

	int32_t 		sys_misc;			/* System Misc Settings */
	char 			sys_pass[41];		/* System Pass Word */
	char 			sys_name[41];		/* System Name */
	char 			sys_id[LEN_QWKID+1];/* System ID for QWK Packets */
	char 			sys_psname[13]; 	/* PostLink and PCRelay Site Name */
	uint32_t		sys_psnum;			/* PostLink and PCRelay Site Number */
	char 			sys_inetaddr[128];	/* System's internet address */
	char 			sys_location[41];	/* System Location */
	int16_t			sys_timezone;		/* Time Zone of BBS */
	char 			sys_daily[LEN_CMD+1];	   /* Daily event */
	char 			sys_logon[LEN_CMD+1];	   /* Logon event */
	char 			sys_logout[LEN_CMD+1];	   /* Logoff event */
	uint8_t			min_pwlen;
	uint16_t		sys_pwdays; 		/* Max days between password change */
	uint16_t		sys_deldays;		/* Days to keep deleted users */
	uint16_t		sys_autodel;		/* Autodeletion after x days inactive */
	uint16_t		sys_nodes;			/* Number of nodes on system */
	char			sys_op[41];         /* Name of system operator */
	char			sys_guru[41];       /* Name of system guru */
	uchar			sys_exp_warn;		/* Days left till expire to notify */
	char 			sys_def_stat;		/* Default status line */
	char 			sys_phonefmt[LEN_PHONE+1];	/* format of phone numbers */
	uint16_t		sys_lastnode;		/* Last displayable node number */
	uint16_t		sys_autonode;		/* First node number for autonode */
	char			sys_chat_arstr[LEN_ARSTR+1];	/* chat override */
	uchar			sys_chat_ar[LEN_ARSTR+1];

	int32_t			msg_misc;			/* Global Message-Related Settings (upper 16-bits default to on) */
	int32_t 		file_misc;			/* File Misc Settings */
	int32_t			xtrn_misc;			/* External Programs Misc Settings */

	char			node_comspec[LEN_CMD+1];	/* DOS COMMAND.COM to use */
	char			node_editor[LEN_CMD+1]; /* Local text editor command line to use */
	char			node_viewer[LEN_CMD+1]; /* Local text viewer command line */
	char			node_daily[LEN_CMD+1];	/* Name of node's daily event */
	uchar			node_scrnlen;		/* Length of screen (rows) */
	uchar			node_scrnblank; 	/* Min of inactivity for blank screen */
	uint32_t		node_misc;			/* Misc bits for node setup */
	uint16_t		node_valuser;		/* User validation mail goes to */
	uint16_t		node_erruser;		/* User error messages goes to */
	uchar			node_errlevel;		/* Log level threshold to notify user (node_erruser) */
	uint16_t		node_ivt;			/* Time-slice APIs */
	uchar			node_swap;			/* Swap types allowed */
	char			node_swapdir[LEN_DIR+1];	/* Swap directory */
	uint16_t		node_minbps;		/* Minimum connect rate of this node */
	uint16_t		node_num;			/* Local node number of this node */
	char			node_phone[13], 	/* Phone number of this node */
					node_name[41];     	/* Name of this node */
	char			node_arstr[LEN_ARSTR+1]; /* Node minimum requirements */
	uchar			node_ar[LEN_ARSTR+1];
	uint32_t		node_cost;			/* Node cost to call - in credits */
	uchar			node_dollars_per_call;	/* Billing Node Dollars Per Call */
	uint16_t		node_sem_check; 	/* Seconds between semaphore checks */
	uint16_t		node_stat_check;	/* Seconds between statistic checks */

	char			new_install;		/* This is a brand new installation */
	char			new_pass[41];		/* New User Password */
	char			new_magic[21];		/* New User Magic Word */
	char			new_sif[LEN_SIFNAME+1]; 		/* New User SIF Questionaire */
	char			new_sof[LEN_SIFNAME+1]; 		/* New User SIF Questionaire output SIF */
	char			new_genders[41];	/* New User Gender options (default: "MF") */
	char			new_level;			/* New User Main Level */
	uint32_t		new_flags1; 		/* New User Main Flags from set #1*/
	uint32_t		new_flags2; 		/* New User Main Flags from set #2*/
	uint32_t		new_flags3; 		/* New User Main Flags from set #3*/
	uint32_t		new_flags4; 		/* New User Main Flags from set #4*/
	uint32_t		new_exempt;			/* New User Exemptions */
	uint32_t		new_rest;			/* New User Restrictions */
	uint32_t		new_cdt;			/* New User Credits */
	uint32_t		new_min;			/* New User Minutes */
	char			new_xedit[LEN_CODE+1];		/* New User Default Editor */
	uint16_t		new_shell;			/* New User Default Command Set */
	uint32_t		new_misc;			/* New User Miscellaneous Defaults */
	uint16_t		new_expire; 		/* Expiration days for new user */
	uchar			new_prot;			/* New User Default Download Protocol */
	uint16_t		new_msgscan_init;	/* Uew User new-scan pointers initialized to msgs this old (in days) */
	uint16_t		guest_msgscan_init;	/* Guest new-scan pointers initialized to msgs this old (in days) */
	char 			val_level[10];		/* Validation User Main Level */
	uint32_t		val_flags1[10]; 	/* Validation User Flags from set #1*/
	uint32_t		val_flags2[10]; 	/* Validation User Flags from set #2*/
	uint32_t		val_flags3[10]; 	/* Validation User Flags from set #3*/
	uint32_t		val_flags4[10]; 	/* Validation User Flags from set #4*/
	uint32_t		val_exempt[10]; 	/* Validation User Exemption Flags */
	uint32_t		val_rest[10];		/* Validation User Restriction Flags */
	uint32_t		val_cdt[10];		/* Validation User Additional Credits */
	uint16_t		val_expire[10]; 	/* Validation User Extend Expire #days */
	uchar			level_expireto[100];
	uint16_t		level_timepercall[100], /* Security level settings */
					level_timeperday[100],
					level_callsperday[100],
					level_linespermsg[100],
					level_postsperday[100],
					level_emailperday[100];
	int32_t			level_freecdtperday[100];
	int32_t			level_misc[100];
	char 			expired_level;	/* Expired user's ML */
	uint32_t		expired_flags1; /* Flags from set #1 to remove when expired */
	uint32_t		expired_flags2; /* Flags from set #2 to remove when expired */
	uint32_t		expired_flags3; /* Flags from set #3 to remove when expired */
	uint32_t		expired_flags4; /* Flags from set #4 to remove when expired */
	uint32_t		expired_exempt; /* Exemptions to remove when expired */
	uint32_t		expired_rest;	/* Restrictions to add when expired */
	uint16_t		min_dspace; 	/* Minimum amount of free space for uploads (in kilobytes) */
	uint16_t		max_batup;		/* Max number of files in upload queue */
	uint16_t		max_batdn;		/* Max number of files in download queue */
	uint16_t		max_userxfer;	/* Max dest. users of user to user xfer */
	uint32_t		max_minutes;	/* Maximum minutes a user can have */
	uint32_t		max_qwkmsgs;	/* Maximum messages per QWK packet */
	uint16_t		max_qwkmsgage;	/* Maximum age (in days) of QWK messages to be imported */
	uint16_t		max_spamage;	/* Maximum age (in days) of SPAM-tagged messages */
	char			preqwk_arstr[LEN_ARSTR+1]; /* pre pack QWK */
	uchar			preqwk_ar[LEN_ARSTR+1];
	uint16_t		cdt_min_value;	/* Minutes per 100k credits */
	uint32_t		cdt_per_dollar; /* Credits per dollar */
	uint16_t		cdt_up_pct; 	/* Pct of credits credited on uploads */
	uint16_t		cdt_dn_pct; 	/* Pct of credits credited per download */
	char		 	node_dir[LEN_DIR+1];
	char		 	ctrl_dir[LEN_DIR+1];
	char			data_dir[LEN_DIR+1];
	char			text_dir[LEN_DIR+1];
	char			exec_dir[LEN_DIR+1];
	char			temp_dir[LEN_DIR+1];
	char			mods_dir[LEN_DIR+1];
	char			logs_dir[LEN_DIR+1];
	char			node_path[MAX_NODES][LEN_DIR+1]; /* paths to all node dirs */
	uint			sysop_dir;			/* Destination for uploads to sysop */
	uint			user_dir;			/* Directory for user to user xfers */
	uint			upload_dir; 		/* Directory where all uploads go */
	char **			altpath;			/* Alternate paths for files */
	uint16_t		altpaths;			/* Total number of alternate paths */
	uint16_t		leech_pct;			/* Leech detection percentage */
	uint16_t		leech_sec;			/* Minimum seconds before possible leech */
	uint32_t		netmail_cost;		/* Cost in credits to send netmail */
	char 			netmail_dir[LEN_DIR+1];    /* Directory to store netmail */
	uint16_t		netmail_misc;		/* Miscellaneous bits regarding netmail */
	uint32_t		inetmail_misc;		/* Miscellaneous bits regarding inetmail */
	uint32_t		inetmail_cost;		/* Cost in credits to send Internet mail */
	char			smtpmail_sem[LEN_DIR+1];	/* Inbound Internet Mail semaphore file */
	char			inetmail_sem[LEN_DIR+1];	/* Outbound Internet Mail semaphore file */
	char			echomail_dir[LEN_DIR+1];	/* Directory to store echomail in */
	char 			fidofile_dir[LEN_DIR+1];	/* Directory where inbound files go (deprecated and not used) */
	char			netmail_sem[LEN_DIR+1];		/* FidoNet NetMail semaphore */
	char 			echomail_sem[LEN_DIR+1];	/* FidoNet EchoMail semaphore  */
	char		 	origline[51];		/* Default EchoMail origin line */
	char			qnet_tagline[128];	/* Default QWK Network tagline */
	int32_t 		uq; 					/* User Questions */
	uint32_t		mail_maxcrcs;			/* Dupe checking in e-mail */
	uint16_t		mail_maxage;			/* Maximum age of e-mail */
	faddr_t			dflt_faddr; 			/* Default FidoNet address */
	char			logon_mod[LEN_MODNAME+1];			/* Logon module */
	char			logoff_mod[LEN_MODNAME+1];			/* Logoff module */
	char			newuser_mod[LEN_MODNAME+1]; 		/* New User Module */
	char			login_mod[LEN_MODNAME+1];			/* Login module */
	char			logout_mod[LEN_MODNAME+1];			/* Logout module */
	char			sync_mod[LEN_MODNAME+1];			/* Synchronization module */
	char			expire_mod[LEN_MODNAME+1];			/* User expiration module */
	char			textsec_mod[LEN_MODNAME+1];			/* Text section module */
	char			xtrnsec_mod[LEN_MODNAME+1];			/* External Program section module */
	char			automsg_mod[LEN_MODNAME+1];			/* Auto-message module */
	char			readmail_mod[LEN_CMD+1];	/* Reading mail module */
	char			scanposts_mod[LEN_CMD+1];	/* Scanning posts (in a single sub) module */
	char			scansubs_mod[LEN_CMD+1];	/* Scanning sub-boards module */
	char			listmsgs_mod[LEN_CMD+1];	/* Listing messages module */
	char			nodelist_mod[LEN_CMD+1];
	char			whosonline_mod[LEN_CMD+1];
	char			privatemsg_mod[LEN_CMD+1];
	char			logonlist_mod[LEN_CMD+1];
    char			prextrn_mod[LEN_MODNAME+1];			/* External Program pre-execution module */
    char			postxtrn_mod[LEN_MODNAME+1];		/* External Program post-execution module */
	char			tempxfer_mod[LEN_MODNAME+1];
	char			scfg_cmd[LEN_CMD+1];	/* SCFG command line - unused! */
	uchar			smb_retry_time; 		/* Seconds to retry on SMBs */
	uint16_t		sec_warn;				/* Seconds before inactivity warning */
	uint16_t		sec_hangup; 			/* Seconds before inactivity hang-up */

	uint* 			color;					/* Different colors for the BBS */
	uint32_t		total_colors;
	uint32_t		ctrlkey_passthru;		/* Bits represent control keys NOT handled by inkey() */

	char			wfc_cmd[10][LEN_CMD+1];    /* 0-9 WFC DOS commands */
	char			wfc_scmd[12][LEN_CMD+1];   /* F1-F12 WFC shrinking DOS commands */

	uint16_t		user_backup_level;
	uint16_t		mail_backup_level;
	char**			text;

	// Run-time state information (not configuration)
	int				tls_certificate;

} scfg_t;

#endif /* Don't add anything after this line */
