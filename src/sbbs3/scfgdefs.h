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
	char		area_tag[FIDO_AREATAG_LEN+1];
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
	uint32_t	pmode;						/* printfile()/putmsg() mode flags */
	uint32_t	n_pmode;					/* set of negated pmode flags */
	faddr_t		faddr;						/* FidoNet address */
	bool		cfg_modified;               /* Update SMB header for this sub */

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
	char		vdir[LEN_DIR+1];			/* Virtual Directory name (dynamically generated) */
	char		vdir_name[LEN_DIR+1];		/* Sysop-defined Virtual Directory name */
	char		vshortcut[LEN_DIR+1];		/* Sysop-defined Virtual Directory shortcut (from root) */
	char		area_tag[FIDO_AREATAG_LEN+1];
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
				op_ar[LEN_ARSTR+1];
	uint		seqdev, 					/* Sequential access device number */
				sort;						/* Sort type */
	uint		maxfiles;					/* Max number of files allowed */
	uint16_t	maxage, 					/* Max age of files (in days) */
				up_pct, 					/* Percentage of credits on uloads */
				dn_pct, 					/* Percentage of credits on dloads */
				lib,						/* Which library this dir is in */
				dirnum;						/* ephemeral index of this dir in cfg.dir[] */
	uint32_t	misc;						/* Miscellaneous bits */
	bool		cfg_modified;               /* Update SMB header for this dir */

} dir_t;

typedef struct {							/* Transfer Library Information */
	char		lname[LEN_GLNAME+1],		/* Long Name - used for listings */
				sname[LEN_GSNAME+1],		/* Short Name - used for prompts */
				vdir[LEN_GSNAME+1],			/* Virtual Directory name */
				arstr[LEN_ARSTR+1],			/* Access Requirements */
				ul_arstr[LEN_ARSTR+1], 		/* Upload Requirements */
				dl_arstr[LEN_ARSTR+1], 		/* Download Requirements */
				ex_arstr[LEN_ARSTR+1], 		/* Exemption Requirements (credits) */
				op_arstr[LEN_ARSTR+1], 		/* Operator Requirements */
				code_prefix[LEN_CODE+1],	/* Prefix for internal code */
				parent_path[LEN_DIR+1];		/* Parent for dir paths */
	uchar		ar[LEN_ARSTR+1],
				ul_ar[LEN_ARSTR+1],
				dl_ar[LEN_ARSTR+1],
				ex_ar[LEN_ARSTR+1],
				op_ar[LEN_ARSTR+1];
	int			offline_dir;				/* Offline file directory */
	uint32_t	misc;						/* Miscellaneous bits */
	enum area_sort sort;
	enum vdir_name vdir_name;
	dir_t dir_defaults;

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
	uint		max_inactivity;				/* Maximum socket inactivity (in seconds), when non-zero */
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
	char		ext[MAX_FILEEXT_LEN+1]; 	/* Extension */
	char		arstr[LEN_ARSTR+1],			/* Access Requirements */
				cmd[LEN_CMD+1]; 			/* Command line */
	uchar		ar[LEN_ARSTR+1];
	uint32_t	ex_mode;					// external() mode flags

} fextr_t;

typedef struct {							/* Compressable file types */
	char		ext[MAX_FILEEXT_LEN+1]; 	/* Extension */
	char		arstr[LEN_ARSTR+1],			/* Access Requirements */
				cmd[LEN_CMD+1]; 			/* Command line */
	uchar		ar[LEN_ARSTR+1];
	uint32_t	ex_mode;					// external() mode flags

} fcomp_t;

typedef struct {							/* Viewable file types */
	char		ext[MAX_FILEEXT_LEN+1]; 	/* Extension */
	char		arstr[LEN_ARSTR+1],			/* Access Requirements */
				cmd[LEN_CMD+1]; 			/* Command line */
	uchar		ar[LEN_ARSTR+1];
	uint32_t	ex_mode;					// external() mode flags

} fview_t;

typedef struct {							/* Testable file types */
	char		ext[MAX_FILEEXT_LEN+1]; 	/* Extension */
	char		arstr[LEN_ARSTR+1],			/* Access requirement */
				cmd[LEN_CMD+1], 			/* Command line */
				workstr[41];				/* String to display while working */
	uchar		ar[LEN_ARSTR+1];
	uint32_t	ex_mode;					// external() mode flags

} ftest_t;

typedef struct {							/* Download events */
	char		ext[MAX_FILEEXT_LEN+1]; 	/* Extension */
	char		arstr[LEN_ARSTR+1],			/* Access requirement */
				cmd[LEN_CMD+1], 			/* Command line */
				workstr[41];				/* String to display while working */
	uchar		ar[LEN_ARSTR+1];
	uint32_t	ex_mode;					// external() mode flags

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
	char			xtrn[LEN_CODE+1];		/* Associated external program (optional) */
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

typedef struct {							// Fixed event
	char			cmd[LEN_CMD + 1];
	uint32_t		misc;					// Settings flags
} fevent_t;

typedef struct {							/* QWK Network Hub */
	char		id[LEN_QWKID+1],			/* System ID of Hub */
				call[LEN_CMD+1],			/* Call-out command line to execute */
				pack[LEN_CMD+1],			/* Packing command line */
				unpack[LEN_CMD+1];			/* Unpacking command line */
	uint8_t		*mode,						/* Mode for Ctrl-A codes for ea. sub */
				days;						/* Days to call-out on */
	char		fmt[MAX_FILEEXT_LEN+1]; 	/* Archive format */
	uint16_t	time,						/* Time to call-out */
				node,						/* Node to do the call-out */
				freq;						/* Frequency of call-outs */
	int			subs;						/* Number Sub-boards carried */
	uint16_t	*conf;						/* Conference number of ea. */
	sub_t**		sub;
	time32_t	last;						/* Last network attempt */
	uint32_t	misc;						/* QHUB_* flags */
	bool		enabled;

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

enum mqtt_tls_mode {
	MQTT_TLS_DISABLED,
	MQTT_TLS_CERT,
	MQTT_TLS_PSK,
	MQTT_TLS_SBBS
};

struct mqtt_cfg {
	bool		enabled;
	bool		verbose;
	char		broker_addr[128];
	uint16_t	broker_port;
	char		username[256];
	char		password[256];
	int			keepalive;
	int			publish_qos;
	int			subscribe_qos;
	int			protocol_version;
	int			log_level;
	struct {
		enum mqtt_tls_mode mode;
		char	cafile[256];
		char	certfile[256];
		char	keyfile[256];
		char	keypass[256];
		char	psk[256];
		char	identity[256];
	} tls;
};

struct loadable_module {
	str_list_t cmd;
	str_list_t ars;
};

enum date_fmt { MMDDYY, DDMMYY, YYMMDD };

typedef struct
{
	DWORD			size;				/* sizeof(scfg_t) */
	bool			prepped;			/* TRUE if prep_cfg() has been used */
	char			filename[MAX_PATH + 1]; // last-loaded cfg file path/name

	grp_t			**grp;				/* Each message group */
	int				total_grps; 		/* Total number of groups */
	sub_t			**sub;				/* Each message sub */
	int				total_subs; 		/* Total number of subs */
	lib_t			**lib;				/* Each library */
	int				total_libs; 		/* Total number of libraries */
	dir_t			**dir;				/* Each message directory */
	int				total_dirs; 		/* Total number of directories */
	txtsec_t 		**txtsec;			/* Each text section */
	int				total_txtsecs;		/* Total number of text sections */
	xtrnsec_t		**xtrnsec;			/* Each external section */
	int				total_xtrnsecs; 	/* Total number of external sections */
	xtrn_t			**xtrn; 			/* Each external program */
	int				total_xtrns;		/* Total number of externals */
	prot_t			**prot; 			/* Each Transfer Protocol */
	int				total_prots;		/* Total Transfer Protocols */
	fextr_t			**fextr;			/* Each extractable file type */
	int				total_fextrs;		/* Total extractable file types */
	fcomp_t			**fcomp;			/* Each compressable file type */
	int				total_fcomps;		/* Total */
	fview_t			**fview;			/* Each veiwable file type */
	int				total_fviews;		/* Total viewable file types */
	ftest_t			**ftest;			/* Each testable file type */
	int				total_ftests;		/* Total testable file types */
	xedit_t			**xedit;			/* Each external editor */
	int				total_xedits;		/* Total external editors */
	qhub_t			**qhub; 			/* QWK network hubs */
	int				total_qhubs;		/* Total qwk network hubs */
	phub_t			**phub; 			/* PostLink/PCRelay network hubs */
	int				total_phubs;		/* Total PostLink/PCRelay hubs */
	chan_t			**chan; 			/* Each chat channel */
	int				total_chans;		/* Total number of chat channels */
	chatact_t		**chatact;			/* Chat action commands */
	int				total_chatacts;     /* Total number of action commands */
	actset_t 		**actset;			/* Name of action set */
	int				total_actsets;		/* Total number of action sets */
	page_t			**page; 			/* External chat page */
	int				total_pages;		/* Total number of external pages */
	event_t			**event;			/* Timed events */
	int				total_events;		/* Total number of timed events */
	dlevent_t		**dlevent;			/* Download events */
	int				total_dlevents; 	/* Total download events */
	faddr_t			*faddr; 			/* FidoNet addresses */
	int				total_faddrs;		/* Total number of fido addresses */
	swap_t			**swap; 			/* Swapping externals */
	int				total_swaps;		/* Total number of non-swap xtrns */
	natvpgm_t 		**natvpgm;			/* Native (not MS-DOS) Programs */
	int				total_natvpgms; 	/* Total number of native pgms */
	guru_t			**guru; 			/* Gurus */
	int				total_gurus;		/* Total number of guru files */
	shell_t			**shell;			/* Command shells */
	int				total_shells;		/* Total number of command shells */
	hotkey_t		**hotkey;			/* Global hot keys */
	int				total_hotkeys;		/* Total number of global hot keys */

										/* COM port registers: */
	uint16_t		com_base,			/* COM base address */
					com_irq;			/* irq line number	   */
	uint32_t		com_rate;			/* DTE rate in bps	   */
	char  			com_port;			/* Number of COM port  */

	uint32_t		sys_login;			// Login Settings (Bit-flags)
	uint32_t 		sys_misc;			/* System Misc Settings */
	uint32_t		sys_pass_timeout;	// Minutes between required SYSPASS entry
	char 			sys_pass[41];		/* System Pass Word */
	char 			sys_name[41];		/* System Name */
	char 			sys_id[LEN_QWKID+1];/* System ID for QWK Packets */
	char 			sys_inetaddr[128];	/* System's internet address */
	char 			sys_location[41];	/* System Location */
#define SYS_TIMEZONE_AUTO -1
	int16_t			sys_timezone;		/* Time Zone of BBS */
	enum date_fmt	sys_date_fmt;
	char			sys_date_sep;
	char			sys_vdate_sep;
	bool			sys_date_verbal;
	fevent_t		sys_monthly;		/* Monthly event */
	fevent_t		sys_weekly;			/* Weekly event */
	fevent_t 		sys_daily;			/* Daily event */
	fevent_t 		sys_logon;			/* Logon event */
	fevent_t 		sys_logout;			/* Logout event */
	bool			hq_password;		/* Require high quality/entropy user passwords */
	uint8_t			min_pwlen;
	uint16_t		sys_pwdays; 		/* Max days between password change */
	uint16_t		sys_deldays;		/* Days to keep deleted users */
	uint16_t		sys_autodel;		/* Autodeletion after x days inactive */
	uint16_t		sys_nodes;			/* Number of nodes on system */
	char			sys_op[41];         /* Name of system operator */
	char			sys_guru[41];       /* Name of system guru */
	uchar			sys_exp_warn;		/* Days left till expire to notify */
	char 			sys_phonefmt[LEN_PHONE+1];	/* format of phone numbers */
	uint16_t		sys_lastnode;		/* Last displayable node number */
	uint16_t		sys_autonode;		/* First node number for autonode */
	char			sys_chat_arstr[LEN_ARSTR+1];	/* chat override */
	uchar			sys_chat_ar[LEN_ARSTR+1];

	int32_t			msg_misc;			/* Global Message-Related Settings (upper 16-bits default to on) */
	int32_t 		file_misc;			/* File Misc Settings */
	int32_t			xtrn_misc;			/* External Programs Misc Settings */
	uint16_t		filename_maxlen;	/* Maximum filename length */
	str_list_t      supported_archive_formats;    /* Full support in libachive */

	bool			create_self_signed_cert;

	fevent_t		node_daily;			/* Node's daily event */
	uint32_t		node_misc;			/* Misc bits for node setup */
	bool			spinning_pause_prompt;
	uint16_t		valuser;			/* User validation mail goes to */
	uint16_t		erruser;			/* User error messages goes to */
	uchar			errlevel;			/* Log level threshold to notify user (erruser) */
	uint16_t		node_num;			/* Local node number of this node */
	char			node_phone[13]; 	/* Phone number of this node */
	char			node_arstr[LEN_ARSTR+1]; /* Node minimum requirements */
	uchar			node_ar[LEN_ARSTR+1];

	char			new_install;		/* This is a brand new installation */
	char			new_pass[41];		/* New User Password */
	char			new_magic[21];		/* New User Magic Word */
	char			new_sif[LEN_SIFNAME+1]; 		/* New User SIF Questionnaire */
	char			new_sof[LEN_SIFNAME+1]; 		/* New User SIF Questionnaire output SIF */
	char			new_genders[41];	/* New User Gender options (default: "MF") */
	int				new_level;			/* New User Main Level */
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
	uint32_t		new_chat;			/* New User Chat settings */
	uint32_t		new_qwk;			/* New User QWK settings */
	uint16_t		new_expire; 		/* Expiration days for new user */
	uchar			new_prot;			/* New User Default Download Protocol */
	uint16_t		new_msgscan_init;	/* Uew User new-scan pointers initialized to msgs this old (in days) */
	uint16_t		guest_msgscan_init;	/* Guest new-scan pointers initialized to msgs this old (in days) */
	int 			val_level[10];		/* Validation User Main Level */
	uint32_t		val_flags1[10]; 	/* Validation User Flags from set #1*/
	uint32_t		val_flags2[10]; 	/* Validation User Flags from set #2*/
	uint32_t		val_flags3[10]; 	/* Validation User Flags from set #3*/
	uint32_t		val_flags4[10]; 	/* Validation User Flags from set #4*/
	uint32_t		val_exempt[10]; 	/* Validation User Exemption Flags */
	uint32_t		val_rest[10];		/* Validation User Restriction Flags */
	uint32_t		val_cdt[10];		/* Validation User Additional Credits */
	uint16_t		val_expire[10]; 	/* Validation User Extend Expire #days */
	int				level_expireto[100];
	uint16_t		level_timepercall[100], /* Security level settings */
					level_timeperday[100],
					level_callsperday[100],
					level_linespermsg[100],
					level_postsperday[100],
					level_emailperday[100];
	uint64_t		level_freecdtperday[100];
	uint32_t		level_downloadsperday[100];
	int32_t			level_misc[100];
	int 			expired_level;	/* Expired user's ML */
	uint32_t		expired_flags1; /* Flags from set #1 to remove when expired */
	uint32_t		expired_flags2; /* Flags from set #2 to remove when expired */
	uint32_t		expired_flags3; /* Flags from set #3 to remove when expired */
	uint32_t		expired_flags4; /* Flags from set #4 to remove when expired */
	uint32_t		expired_exempt; /* Exemptions to remove when expired */
	uint32_t		expired_rest;	/* Restrictions to add when expired */
	int64_t			min_dspace; 	/* Minimum amount of free space for uploads */
	uint16_t		max_batup;		/* Max number of files in upload queue */
	uint16_t		max_batdn;		/* Max number of files in download queue */
	uint16_t		max_userxfer;	/* Max dest. users of user to user xfer */
	uint32_t		max_minutes;	/* Maximum minutes a user can have */
	uint32_t		max_qwkmsgs;	/* Maximum messages per QWK packet */
	uint16_t		max_qwkmsgage;	/* Maximum age (in days) of QWK messages to be imported */
	uint16_t		max_spamage;	/* Maximum age (in days) of SPAM-tagged messages */
	uint32_t		max_log_size;
	uint16_t		max_logs_kept;
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
	int				sysop_dir;			/* Destination for uploads to sysop */
	int				user_dir;			/* Directory for user to user xfers */
	int				upload_dir; 		/* Directory where all uploads go */
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
	char			netmail_sem[LEN_DIR+1];		/* FidoNet NetMail semaphore */
	char 			echomail_sem[LEN_DIR+1];	/* FidoNet EchoMail semaphore  */
	char		 	origline[51];		/* Default EchoMail origin line */
	char			qnet_tagline[128];	/* Default QWK Network tagline */
	int32_t 		uq; 					/* User Questions */
	uint32_t		mail_maxcrcs;			/* Dupe checking in e-mail */
	uint16_t		mail_maxage;			/* Maximum age of e-mail */
	struct loadable_module logon_mod;			/* Logon module */
	struct loadable_module logoff_mod;			/* Logoff module */
	struct loadable_module newuser_prompts_mod;	/* New User Prompts Module */
	struct loadable_module newuser_info_mod;	/* New User Info Module */
	struct loadable_module newuser_mod; 		/* New User Module */
	struct loadable_module login_mod;			/* Login module */
	struct loadable_module logout_mod;			/* Logout module */
	struct loadable_module sync_mod;			/* Synchronization module */
	struct loadable_module expire_mod;			/* User expiration module */
	struct loadable_module emailsec_mod;
	struct loadable_module textsec_mod;			/* Text section module */
	struct loadable_module xtrnsec_mod;			/* External Program section module */
	struct loadable_module chatsec_mod;			/* Chat section module */
	struct loadable_module automsg_mod;			/* Auto-message module */
	struct loadable_module feedback_mod;		/* Send feedback to sysop module */
	struct loadable_module readmail_mod;		/* Reading mail module */
	struct loadable_module scanposts_mod;		/* Scanning posts (in a single sub) module */
	struct loadable_module scansubs_mod;		/* Scanning sub-boards module */
	struct loadable_module listmsgs_mod;		/* Listing messages module */
	struct loadable_module scandirs_mod;
	struct loadable_module listfiles_mod;
	struct loadable_module fileinfo_mod;
	struct loadable_module nodelist_mod;
	struct loadable_module whosonline_mod;
	struct loadable_module privatemsg_mod;
	struct loadable_module logonlist_mod;
	struct loadable_module userlist_mod;
	struct loadable_module usercfg_mod;
    struct loadable_module prextrn_mod;			/* External Program pre-execution module */
    struct loadable_module postxtrn_mod;		/* External Program post-execution module */
	struct loadable_module tempxfer_mod;
	struct loadable_module batxfer_mod;
	uchar			smb_retry_time; 		/* Seconds to retry on SMBs */
	uchar			inactivity_warn;		// percentage
	uint			max_getkey_inactivity;	// Seconds before user inactivity hang-up

	uint 			color[NUM_COLORS];		/* Different colors for the BBS */
	uint			rainbow[LEN_RAINBOW + 1];
	uint32_t		ctrlkey_passthru;		/* Bits represent control keys NOT handled by inkey() */

	uint			user_backup_level;
	uint			mail_backup_level;
	uint			config_backup_level;
	char**			text;

	uint			stats_interval;		// Statistics read interval in seconds (cache duration)
	uint			cache_filter_files;

	// Run-time state information (not configuration)

	struct mqtt_cfg mqtt;
} scfg_t;

#endif /* Don't add anything after this line */
