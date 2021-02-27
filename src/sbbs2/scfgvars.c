/* SCFGVARS.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/**********************************************************************/
/* External (Global/Public) Variables for use with both SBBS and SCFG */
/**********************************************************************/

#ifndef GLOBAL
#define GLOBAL
	   char *scfgnulstr="";
#else
extern char *scfgnulstr;
#endif

#include "sbbsdefs.h"

GLOBAL grp_t		**grp;				/* Each message group */
GLOBAL ushort		total_grps; 		/* Total number of groups */
GLOBAL sub_t		**sub;				/* Each message sub */
GLOBAL ushort		total_subs; 		/* Total number of subs */
GLOBAL lib_t		**lib;				/* Each library */
GLOBAL ushort		total_libs; 		/* Total number of libraries */
GLOBAL dir_t		**dir;				/* Each message directory */
GLOBAL ushort		total_dirs; 		/* Total number of directories */
GLOBAL txtsec_t 	**txtsec;			/* Each text section */
GLOBAL ushort		total_txtsecs;		/* Total number of text sections */
GLOBAL xtrnsec_t	**xtrnsec;			/* Each external section */
GLOBAL ushort		total_xtrnsecs; 	/* Total number of external sections */
GLOBAL xtrn_t		**xtrn; 			/* Each external program */
GLOBAL ushort		total_xtrns;		/* Total number of externals */
GLOBAL mdm_result_t *mdm_result;		/* Each Modem Result Code */
GLOBAL ushort		mdm_results;		/* Total number of Modem Results */
GLOBAL prot_t		**prot; 			/* Each Transfer Protocol */
GLOBAL ushort		total_prots;		/* Total Transfer Protocols */
GLOBAL fextr_t		**fextr;			/* Each extractable file type */
GLOBAL ushort		total_fextrs;		/* Total extractable file types */
GLOBAL fcomp_t		**fcomp;			/* Each compressable file type */
GLOBAL ushort		total_fcomps;		/* Total */
GLOBAL fview_t		**fview;			/* Each veiwable file type */
GLOBAL ushort		total_fviews;		/* Total viewable file types */
GLOBAL ftest_t		**ftest;			/* Each testable file type */
GLOBAL ushort		total_ftests;		/* Total testable file types */
GLOBAL xedit_t		**xedit;			/* Each external editor */
GLOBAL ushort		total_xedits;		/* Total external editors */
GLOBAL qhub_t		**qhub; 			/* QWK network hubs */
GLOBAL ushort		total_qhubs;		/* Total qwk network hubs */
GLOBAL phub_t		**phub; 			/* PostLink/PCRelay network hubs */
GLOBAL ushort		total_phubs;		/* Total PostLink/PCRelay hubs */
GLOBAL chan_t		**chan; 			/* Each chat channel */
GLOBAL ushort		total_chans;		/* Total number of chat channels */
GLOBAL chatact_t	**chatact;			/* Chat action commands */
GLOBAL ushort       total_chatacts;     /* Total number of action commands */
GLOBAL actset_t 	**actset;			/* Name of action set */
GLOBAL ushort		total_actsets;		/* Total number of action sets */
GLOBAL page_t		**page; 			/* External chat page */
GLOBAL ushort		total_pages;		/* Total number of external pages */
GLOBAL event_t		**event;			/* Timed events */
GLOBAL ushort		total_events;		/* Total number of timed events */
GLOBAL dlevent_t	**dlevent;			/* Download events */
GLOBAL ushort		total_dlevents; 	/* Total download events */
GLOBAL faddr_t		*faddr; 			/* FidoNet addresses */
GLOBAL ushort		total_faddrs;		/* Total number of fido addresses */
GLOBAL swap_t		**swap; 			/* Swapping externals */
GLOBAL ushort		total_swaps;		/* Total number of non-swap xtrns */
GLOBAL os2pgm_t 	**os2pgm;			/* DOS Programs */
GLOBAL ushort		total_os2pgms;		/* Total number of DOS pgms */
GLOBAL guru_t		**guru; 			/* Gurus */
GLOBAL ushort		total_gurus;		/* Total number of guru files */
GLOBAL shell_t		**shell;			/* Command shells */
GLOBAL ushort		total_shells;		/* Total number of command shells */

									/* COM port registers: */
GLOBAL ushort	com_base,			/* COM base address */
				com_irq;			/* irq line number	   */
GLOBAL ulong	com_rate;			/* DTE rate in bps	   */
GLOBAL char  	com_port;			/* Number of COM port  */

									/* Modem command strings */
GLOBAL char 	mdm_init[64],		/* Initialization */
				mdm_spec[64],		/* Special Initialization */
				mdm_term[64],		/* Terminal Initialization String */
				mdm_dial[64],		/* Dial */
				mdm_offh[64],		/* Off hook */
				mdm_answ[64],		/* Answer */
				mdm_hang[64];		/* Hang-up */
GLOBAL ulong	mdm_misc;			/* Misc bits used for flags */
GLOBAL ushort	mdm_reinit; 		/* Modem reinitialization minute count */
GLOBAL ushort	mdm_ansdelay;		/* Modem seconds to delay after answer */
GLOBAL uchar	mdm_rings;			/* Rings to wait till answer */

GLOBAL long 	sys_misc;			/* System Misc Settings */
GLOBAL char 	sys_pass[41];		/* System Pass Word */
GLOBAL char 	sys_name[41];		/* System Name */
GLOBAL char 	sys_id[9];			/* System ID for QWK Packets */
GLOBAL char 	sys_psname[13]; 	/* PostLink and PCRelay Site Name */
GLOBAL ulong	sys_psnum;			/* PostLink and PCRelay Site Number */
GLOBAL char 	sys_inetaddr[128];	/* System's internet address */
GLOBAL char 	sys_location[41];	/* System Location */
GLOBAL short	sys_timezone;		/* Time Zone of BBS */
GLOBAL char 	sys_daily[LEN_CMD+1];	   /* Daily event */
GLOBAL char 	sys_logon[LEN_CMD+1];	   /* Logon event */
GLOBAL char 	sys_logout[LEN_CMD+1];	   /* Logoff event */
GLOBAL ushort	sys_pwdays; 		/* Max days between password change */
GLOBAL ushort	sys_deldays;		/* Days to keep deleted users */
GLOBAL ushort	sys_autodel;		/* Autodeletion after x days inactive */
GLOBAL ushort	sys_nodes;			/* Number of local nodes on system	*/
GLOBAL char     sys_op[41];         /* Name of system operator */
GLOBAL char     sys_guru[41];       /* Name of system guru */
GLOBAL uchar	sys_exp_warn;		/* Days left till expire to notify */
GLOBAL char 	sys_def_stat;		/* Default status line */
GLOBAL char 	sys_phonefmt[LEN_PHONE+1];	/* format of phone numbers */
GLOBAL ushort	sys_lastnode;		/* Last displayable node number */
GLOBAL ushort	sys_autonode;		/* First node number for autonode */
#ifdef SCFG
GLOBAL uchar	sys_chat_ar[LEN_ARSTR+1];	/* chat override */
#else
GLOBAL uchar	*sys_chat_ar;
#endif

GLOBAL uchar	node_comspec[LEN_CMD+1];	/* DOS COMMAND.COM to use */
GLOBAL uchar	node_editor[LEN_CMD+1]; /* Local text editor command line to use */
GLOBAL uchar	node_viewer[LEN_CMD+1]; /* Local text viewer command line */
GLOBAL uchar	node_daily[LEN_CMD+1];	/* Name of node's daily event */
GLOBAL uchar	node_scrnlen;		/* Length of screen (rows) */
GLOBAL uchar	node_scrnblank; 	/* Min of inactivity for blank screen */
GLOBAL ulong	node_misc;			/* Misc bits for node setup */
GLOBAL ushort	node_valuser;		/* User validation mail goes to */
GLOBAL ushort	node_ivt;			/* Time-slice APIs */
GLOBAL uchar	node_swap;			/* Swap types allowed */
GLOBAL uchar	node_swapdir[LEN_DIR+1];	/* Swap directory */
GLOBAL ushort	node_minbps;		/* Minimum connect rate of this node */
GLOBAL ushort	node_num;			/* Local node number of this node */
GLOBAL uchar	node_phone[13], 	/* Phone number of this node */
				node_name[41];     	/* Name of this node */
#ifdef SCFG
GLOBAL uchar	node_ar[LEN_ARSTR+1]; /* Node minimum requirements */
#else
GLOBAL uchar	*node_ar;
#endif
GLOBAL ulong	node_cost;			/* Node cost to call - in credits */
GLOBAL uchar	node_dollars_per_call;	/* Billing Node Dollars Per Call */
GLOBAL ushort	node_sem_check; 	/* Seconds between semaphore checks */
GLOBAL ushort	node_stat_check;	/* Seconds between statistic checks */

GLOBAL char 	new_pass[41];		/* New User Password */
GLOBAL char 	new_magic[21];		/* New User Magic Word */
GLOBAL char 	new_sif[9]; 		/* New User SIF Questionaire */
GLOBAL char 	new_sof[9]; 		/* New User SIF Questionaire output SIF */
GLOBAL char 	new_level;			/* New User Main Level */
GLOBAL ulong	new_flags1; 		/* New User Main Flags from set #1*/
GLOBAL ulong	new_flags2; 		/* New User Main Flags from set #2*/
GLOBAL ulong	new_flags3; 		/* New User Main Flags from set #3*/
GLOBAL ulong	new_flags4; 		/* New User Main Flags from set #4*/
GLOBAL ulong	new_exempt;			/* New User Exemptions */
GLOBAL ulong	new_rest;			/* New User Restrictions */
GLOBAL ulong	new_cdt;			/* New User Credits */
GLOBAL ulong	new_min;			/* New User Minutes */
GLOBAL uchar	new_xedit[9];		/* New User Default Editor */
GLOBAL ushort	new_shell;			/* New User Default Command Set */
GLOBAL ulong	new_misc;			/* New User Miscellaneous Defaults */
GLOBAL ushort	new_expire; 		/* Expiration days for new user */
GLOBAL uchar	new_prot;			/* New User Default Download Protocol */
GLOBAL char 	val_level[10];		/* Validation User Main Level */
GLOBAL ulong	val_flags1[10]; 	/* Validation User Flags from set #1*/
GLOBAL ulong	val_flags2[10]; 	/* Validation User Flags from set #2*/
GLOBAL ulong	val_flags3[10]; 	/* Validation User Flags from set #3*/
GLOBAL ulong	val_flags4[10]; 	/* Validation User Flags from set #4*/
GLOBAL ulong	val_exempt[10]; 	/* Validation User Exemption Flags */
GLOBAL ulong	val_rest[10];		/* Validation User Restriction Flags */
GLOBAL ulong	val_cdt[10];		/* Validation User Additional Credits */
GLOBAL ushort	val_expire[10]; 	/* Validation User Extend Expire #days */
GLOBAL uchar	level_expireto[100];
GLOBAL ushort	level_timepercall[100], /* Security level settings */
				level_timeperday[100],
				level_callsperday[100],
				level_linespermsg[100],
				level_postsperday[100],
				level_emailperday[100];
GLOBAL long 	level_freecdtperday[100];
GLOBAL long 	level_misc[100];
GLOBAL char 	expired_level;	/* Expired user's ML */
GLOBAL ulong	expired_flags1; /* Flags from set #1 to remove when expired */
GLOBAL ulong	expired_flags2; /* Flags from set #2 to remove when expired */
GLOBAL ulong	expired_flags3; /* Flags from set #3 to remove when expired */
GLOBAL ulong	expired_flags4; /* Flags from set #4 to remove when expired */
GLOBAL ulong	expired_exempt; /* Exemptions to remove when expired */
GLOBAL ulong	expired_rest;	/* Restrictions to add when expired */
GLOBAL ushort	min_dspace; 	/* Minimum amount of free space for uploads */
GLOBAL ushort	max_batup;		/* Max number of files in upload queue */
GLOBAL ushort	max_batdn;		/* Max number of files in download queue */
GLOBAL ushort	max_userxfer;	/* Max dest. users of user to user xfer */
GLOBAL ulong	max_minutes;	/* Maximum minutes a user can have */
GLOBAL ulong	max_qwkmsgs;	/* Maximum messages per QWK packet */
#ifdef SCFG
GLOBAL uchar	preqwk_ar[LEN_ARSTR+1]; /* pre pack QWK */
#else
GLOBAL uchar	*preqwk_ar;
#endif
GLOBAL ushort	cdt_min_value;	/* Minutes per 100k credits */
GLOBAL ulong	cdt_per_dollar; /* Credits per dollar */
GLOBAL ushort	cdt_up_pct; 	/* Pct of credits credited on uploads */
GLOBAL ushort	cdt_dn_pct; 	/* Pct of credits credited per download */
GLOBAL char 	node_dir[LEN_DIR+1];
GLOBAL char 	ctrl_dir[LEN_DIR+1];
GLOBAL char 	data_dir[LEN_DIR+1];
GLOBAL char 	text_dir[LEN_DIR+1];
GLOBAL char 	exec_dir[LEN_DIR+1];
GLOBAL char 	temp_dir[LEN_DIR+1];
GLOBAL char 	**node_path;		/* paths to all node dirs */
GLOBAL ushort	sysop_dir;			/* Destination for uploads to sysop */
GLOBAL ushort	user_dir;			/* Directory for user to user xfers */
GLOBAL ushort	upload_dir; 		/* Directory where all uploads go */
GLOBAL char 	**altpath;			/* Alternate paths for files */
GLOBAL ushort	altpaths;			/* Total number of alternate paths */
GLOBAL ushort	leech_pct;			/* Leech detection percentage */
GLOBAL ushort	leech_sec;			/* Minimum seconds before possible leech */
GLOBAL ulong	netmail_cost;		/* Cost in credits to send netmail */
GLOBAL char 	netmail_dir[LEN_DIR+1];    /* Directory to store netmail */
GLOBAL ushort	netmail_misc;		/* Miscellaneous bits regarding netmail */
GLOBAL ulong	inetmail_misc;		/* Miscellaneous bits regarding inetmail */
GLOBAL ulong	inetmail_cost;		/* Cost in credits to send Internet mail */
GLOBAL uchar	inetmail_sem[LEN_DIR+1];	/* Internet Mail semaphore file */
GLOBAL char 	echomail_dir[LEN_DIR+1];   /* Directory to store echomail in */
GLOBAL char 	fidofile_dir[LEN_DIR+1];   /* Directory where inbound files go */
GLOBAL char 	netmail_sem[LEN_DIR+1];    /* FidoNet NetMail semaphore */
GLOBAL char 	echomail_sem[LEN_DIR+1];   /* FidoNet EchoMail semaphore  */
GLOBAL char 	origline[51];		/* Default EchoMail origin line */
GLOBAL char 	qnet_tagline[128];	/* Default QWK Network tagline */
GLOBAL long 	uq; 					/* User Questions */
GLOBAL ulong	mail_maxcrcs;			/* Dupe checking in e-mail */
GLOBAL ushort	mail_maxage;			/* Maximum age of e-mail */
GLOBAL faddr_t	dflt_faddr; 			/* Default FidoNet address */
GLOBAL uchar	logon_mod[9];			/* Logon module */
GLOBAL uchar	logoff_mod[9];			/* Logoff module */
GLOBAL uchar	newuser_mod[9]; 		/* New User Module */
GLOBAL uchar	login_mod[9];			/* Login module */
GLOBAL uchar	logout_mod[9];			/* Logout module */
GLOBAL uchar	sync_mod[9];			/* Synchronization module */
GLOBAL uchar	expire_mod[9];			/* User expiration module */
GLOBAL uchar	scfg_cmd[LEN_CMD+1];	/* SCFG command line */
GLOBAL uchar	smb_retry_time; 		/* Seconds to retry on SMBs */
GLOBAL ushort	sec_warn;				/* Seconds before inactivity warning */
GLOBAL ushort	sec_hangup; 			/* Seconds before inactivity hang-up */

#ifndef SCFG

GLOBAL uchar	data_dir_subs[128]; 	/* DATA\SUBS directory */
GLOBAL uchar    data_dir_dirs[128];     /* DATA\DIRS directory */

#endif

#ifdef SCFG

GLOBAL char 	wfc_cmd[10][LEN_CMD+1];    /* 0-9 WFC DOS commands */
GLOBAL char 	wfc_scmd[12][LEN_CMD+1];   /* F1-F12 WFC shrinking DOS commands */

#else

GLOBAL char 	*wfc_cmd[10];		/* 0-9 WFC DOS commands */
GLOBAL char     *wfc_scmd[12];      /* F1-F12 WFC shrinking DOS commands */

#endif

