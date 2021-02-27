/* 	CNF file reader/writer - mcmlxxix - 2013
	$Id: cnfdefs.js,v 1.10 2020/03/01 19:12:28 rswindell Exp $
*/

/* miscellaneous constants required for cnf parsing
   (if adding definitions to this file, make sure the required
   constants are defined... if not, add them) 
 */

/* Note: rev 1.3 (and earlier) of this file used field names derived from the
 * C/C++ source code (scfgdefs.h), while rev 1.4 and later now use field
 * names which correlate with the Synchronet JS object model (jsobjs.html).
 */

var LEN_DIR = 		63;
var LEN_CMD = 		63;
var LEN_ARSTR =		40;
var LEN_CODE =		8;		/* Maximum length of internal codes			*/
var LEN_QWKID =		8;		/* Maximum length of QWK-ID					*/
var LEN_MODNAME = 	8;		/* Maximum length of readable module name	*/
var LEN_SIFNAME = 	8;		/* Maximum length of SIF filename			*/
var LEN_EXTCODE = 	LEN_CODE*2; /* Code prefix + suffix */
var LEN_GSNAME =	15;		/* Group/Lib short name						*/
var LEN_GLNAME =	40;		/* Group/Lib long name						*/
var LEN_SSNAME =	25;		/* Sub/Dir short name						*/
var LEN_SLNAME =	40;		/* Sub/Dir long name						*/
						
var UINT16_T = 		2;
var UINT32_T = 		4;
var UCHAR = 		1;


/* CNF structure - 
object properties must be in the same order as they are read from file 
and any dependencies must be defined before a data structure that uses them */
var struct = {};

/* external program record structures */
struct.swap_t={
	cmd:		{bytes:LEN_CMD+1,		type:"str"}
};
struct.xedit_t={
	name:		{bytes:41,				type:"str"},
	code:		{bytes:LEN_CODE+1,		type:"str"},
	cmd:		{bytes:LEN_CMD+1,		type:"str"},	// was lcmd
	rcmd:		{bytes:LEN_CMD+1,		type:"str"},	// unused
	settings:	{bytes:UINT32_T,		type:"int"},	// was misc
	ars:		{bytes:LEN_ARSTR+1,		type:"str"},	// was arstr
	type:		{bytes:UCHAR,			type:"int"},
	__PADDING__:15
};
struct.xtrnsec_t={
	name:		{bytes:41,				type:"str"},
	code:		{bytes:LEN_CODE+1,		type:"str"},
	ars:		{bytes:LEN_ARSTR+1,		type:"str"},	// was arstr
	__PADDING__:16
};
struct.xtrn_t={
	sec:			{bytes:UINT16_T,		type:"int"},
	name:			{bytes:41,				type:"str"},
	code:			{bytes:LEN_CODE+1,		type:"str"},
	ars:			{bytes:LEN_ARSTR+1,		type:"str"},	// was arstr
	execution_ars:	{bytes:LEN_ARSTR+1,		type:"str"},	// was run_arstr
	type:			{bytes:UCHAR,			type:"int"},
	settings:		{bytes:UINT32_T,		type:"int"},	// was misc
	event:			{bytes:UCHAR,			type:"int"},
	cost:			{bytes:UINT32_T,		type:"int"},
	cmd:			{bytes:LEN_CMD+1,		type:"str"},
	clean_cmd:		{bytes:LEN_CMD+1,		type:"str"},	// was clean
	startup_dir:	{bytes:LEN_DIR+1,		type:"str"},	// was path
	textra:			{bytes:UCHAR,			type:"int"},
	max_time:		{bytes:UCHAR,			type:"int"},	// was maxtime
	__PADDING__:14
};
struct.event_t={
	code:			{bytes:LEN_CODE+1,		type:"str"},
	cmd:			{bytes:LEN_CMD+1,		type:"str"},
	days:			{bytes:1,				type:"int"},
	time:			{bytes:UINT16_T,		type:"int"},
	node_num:		{bytes:UINT16_T,		type:"int"},	// was node
	settings:		{bytes:UINT32_T,		type:"int"},	// was misc
	startup_dir:	{bytes:LEN_DIR+1,		type:"str"},	// was dir
	freq:			{bytes:UINT16_T,		type:"int"},
	mdays:			{bytes:UINT32_T,		type:"int"},
	months:			{bytes:UINT16_T,		type:"int"},
	__PADDING__:8
};
struct.natvpgm_t={
	name:		{bytes:13,				type:"str"},
	misc:		{bytes:UINT32_T,		type:"int"}			// unused
};
struct.hotkey_t={
	key:		{bytes:UCHAR,			type:"int"},
	cmd:		{bytes:LEN_CMD+1,		type:"str"},
	__PADDING__:16
};

/* message group/sub record structures */
struct.faddr_t={
	zone:		{bytes:2,				type:"int"},
	net:		{bytes:2,				type:"int"},
	node:		{bytes:2,				type:"int"},
	point:		{bytes:2,				type:"int"}
};
struct.sub_t={
	grp_number:		{bytes:UINT16_T,		type:"int"},	// was grp
	description:	{bytes:LEN_SLNAME+1,	type:"str"},	// was lname
	name:			{bytes:LEN_SSNAME+1,	type:"str"},	// was sname
	qwk_name:		{bytes:11,				type:"str"},	// was qwkname
	code_suffix:	{bytes:LEN_CODE+1,		type:"str"},
	data_dir:		{bytes:LEN_DIR+1,		type:"str"},
	ars:			{bytes:LEN_ARSTR+1,		type:"str"},	// was arstr
	read_ars:		{bytes:LEN_ARSTR+1,		type:"str"},	// was read_arstr
	post_ars:		{bytes:LEN_ARSTR+1,		type:"str"},	// was post_arstr
	operator_ars:	{bytes:LEN_ARSTR+1,		type:"str"},	// was op_arstr
	// uchar		*ar,
				// *read_ar,
				// *post_ar,
				// *op_ar,
	settings:		{bytes:UINT32_T,		type:"int"},	// was misc
	qwknet_tagline:	{bytes:81,				type:"str"},	// was tagline
	fidonet_origin:	{bytes:51,				type:"str"},	// was origline
	post_sem:		{bytes:LEN_DIR+1,		type:"str"},
	newsgroup:		{bytes:LEN_DIR+1,		type:"str"},
	fidonet_addr:	{bytes:struct.faddr_t,	type:"obj"},
	max_msgs:		{bytes:UINT32_T,		type:"int"},	// was maxmsgs
	max_crcs:		{bytes:UINT32_T,		type:"int"},	// was maxcrcs
	max_age:		{bytes:UINT16_T,		type:"int"},	// was maxage
	ptridx:			{bytes:UINT16_T,		type:"int"},
	moderated_ars:	{bytes:LEN_ARSTR+1,		type:"str"},	// was mod_arstr
	// *mod_ar;
	qwk_conf:		{bytes:UINT16_T,		type:"int"},	// was qwkconf
	__PADDING__:53
};
struct.grp_t={
	description:	{bytes:LEN_GLNAME+1,	type:"str"},	// was lname
	name:			{bytes:LEN_GSNAME+1,	type:"str"},	// was sname
	ars:			{bytes:LEN_ARSTR+1,		type:"str"},	// was arstr
	code_prefix:	{bytes:LEN_CODE+1,		type:"str"},
	// uchar		*ar;
	__PADDING__:87
};
struct.qhubsub_t={
	conf:		{bytes:UINT16_T,		type:"int"},
	sub:		{bytes:UINT16_T,		type:"int"},
	mode:		{bytes:UCHAR,			type:"int"}
};
struct.qhub_t={
	id:			{bytes:LEN_QWKID+1,		type:"str"},
	time:		{bytes:UINT16_T,		type:"int"},
	freq:		{bytes:UINT16_T,		type:"int"},
	days:		{bytes:UCHAR,			type:"int"},
	node:		{bytes:UINT16_T,		type:"int"},
	call:		{bytes:LEN_CMD+1,		type:"str"},
	pack:		{bytes:LEN_CMD+1,		type:"str"},
	unpack:		{bytes:LEN_CMD+1,		type:"str"},
	subs:		{bytes:struct.qhubsub_t,	type:"lst"},
	// uint16_t	*mode,					/* Mode for Ctrl-A codes for ea. sub */
	// 			*conf;					/* Conference number of ea. */
	// ulong	*sub;					/* Number of local sub-board for ea. */
	// time32_t	last;					/* Last network attempt */
	__PADDING__:64
};
struct.phub_t={
	days:		{bytes:1,				type:"str"},
	name:		{bytes:11,				type:"str"},
	call:		{bytes:LEN_CMD+1,		type:"str"},
	time:		{bytes:UINT16_T,		type:"str"},
	node:		{bytes:UINT16_T,		type:"str"},
	freq:		{bytes:UINT16_T,		type:"str"},
// time32_t	last;						/* Last network attempt */
	__PADDING__:64
};

struct.node_dir_t={
	path:			{bytes:LEN_DIR+1,		type:"str"},
};

struct.validation_set_t={
	level:		{bytes:UCHAR,				type:"int"},
	expire:		{bytes:UINT16_T,			type:"int"},
	flags1:		{bytes:UINT32_T,			type:"int"},
	flags2:		{bytes:UINT32_T,			type:"int"},
	flags3:		{bytes:UINT32_T,			type:"int"},
	flags4:		{bytes:UINT32_T,			type:"int"},
	cdt:		{bytes:UINT32_T,			type:"int"},
	exempt:		{bytes:UINT32_T,			type:"int"},
	rest:		{bytes:UINT32_T,			type:"int"},
	__PADDING__:16
};

struct.sec_level_t={
	timeperday:	{bytes:UINT16_T,			type:"int"},
	timepercall:{bytes:UINT16_T,			type:"int"},
	callsperday:{bytes:UINT16_T,			type:"int"},
	freecdtperd:{bytes:UINT32_T,			type:"int"},
	linespermsg:{bytes:UINT16_T,			type:"int"},
	postsperday:{bytes:UINT16_T,			type:"int"},
	emailperday:{bytes:UINT16_T,			type:"int"},
	misc:		{bytes:UINT32_T,			type:"int"},
	expireto:	{bytes:UCHAR,				type:"int"},
	__PADDING__:11
};

struct.cmd_shell_t={
	name:		{bytes:40+1,				type:"str"},
	code:		{bytes:LEN_CODE+1,			type:"str"},
	ars:		{bytes:LEN_ARSTR+1,			type:"str"},
	misc:		{bytes:UINT32_T,			type:"int"},
	__PADDING__:16
};

/* NOTE: top-level data structures only below this point 
as they may contain references to the data structures above
and will not work if they are defined first */

/* main config file structure (main.cnf) */
struct.main={
	sys_name:			{bytes:40+1,				type:"str"},
	sys_qwk_id:			{bytes:LEN_QWKID+1,			type:"str"},	// sys_id
	sys_location:		{bytes:40+1,				type:"str"},
	sys_phonefmt:		{bytes:12+1,				type:"str"},
	sys_operator:		{bytes:40+1,				type:"str"},	// sys_op
	sys_guru:			{bytes:40+1,				type:"str"},
	sys_pass:			{bytes:40+1,				type:"str"},
	node_dir:			{bytes:struct.node_dir_t,	type:"lst"},
	data_dir:			{bytes:LEN_DIR+1,			type:"str"},
	exec_dir:			{bytes:LEN_DIR+1,			type:"str"},
	sys_logon:			{bytes:LEN_CMD+1,			type:"str"},
	sys_logout:			{bytes:LEN_CMD+1,			type:"str"},
	sys_daily:			{bytes:LEN_CMD+1,			type:"str"},
	sys_timezone:		{bytes:UINT16_T,			type:"int"},
	sys_settings:		{bytes:UINT32_T,			type:"int"},	// sys_misc
	sys_lastnode:		{bytes:UINT16_T,			type:"int"},
	sys_autonode:		{bytes:UINT16_T,			type:"int"},
	newuser_questions:	{bytes:UINT32_T,			type:"int"},	// uq
	sys_pwdays:			{bytes:UINT16_T,			type:"int"},
	sys_deldays:		{bytes:UINT16_T,			type:"int"},
	sys_exp_warn:		{bytes:UCHAR,				type:"int"},
	sys_autodel:		{bytes:UINT16_T,			type:"int"},
	__PADDING__:1,
	sys_chat_ars:		{bytes:LEN_ARSTR+1,			type:"str"},	// sys_chat_ar
	cdt_min_value:		{bytes:UINT16_T,			type:"int"},
	max_minutes:		{bytes:UINT32_T,			type:"int"},
	cdt_per_dollar:		{bytes:UINT32_T,			type:"int"},
	newuser_pass:				{bytes:40+1,		type:"str"},	// new_pass
	newuser_magic_word:			{bytes:20+1,		type:"str"},	// new_magic
	newuser_sif:				{bytes:8+1,			type:"str"},
	newuser_sof:				{bytes:8+1,			type:"str"},
	newuser_level:				{bytes:UCHAR,		type:"int"},
	newuser_flags1:				{bytes:UINT32_T,	type:"int"},
	newuser_flags2:				{bytes:UINT32_T,	type:"int"},
	newuser_flags3:				{bytes:UINT32_T,	type:"int"},
	newuser_flags4:				{bytes:UINT32_T,	type:"int"},
	newuser_exemptions:			{bytes:UINT32_T,	type:"int"},	// new_exempt
	newuser_restrictions:		{bytes:UINT32_T,	type:"int"},	// new_rest
	newuser_credits:			{bytes:UINT32_T,	type:"int"},	// new_cdt
	newuser_minutes:			{bytes:UINT32_T,	type:"int"},	// new_min
	newuser_editor:				{bytes:LEN_CODE+1,	type:"str"},	// new_xedit
	newuser_expiration_days:	{bytes:UINT16_T,	type:"int"},	// new_expire
	newuser_command_shell:		{bytes:UINT16_T,	type:"int"},	// new_shell
	newuser_settings:			{bytes:UINT32_T,	type:"int"},	// new_misc
	newuser_download_protocol:	{bytes:UCHAR,		type:"int"},	// new_prot
	new_install:				{bytes:UCHAR,		type:"int"},
	newuser_msgscan_init:		{bytes:UINT16_T,	type:"int"},	// new_msgcan_init
	guest_msgscan_init:			{bytes:UINT16_T,	type:"int"},
	__PADDING1__:10,
	expired_level:				{bytes:UCHAR,		type:"int"},
	expired_flags1:				{bytes:UINT32_T,	type:"int"},
	expired_flags2:				{bytes:UINT32_T,	type:"int"},
	expired_flags3:				{bytes:UINT32_T,	type:"int"},
	expired_flags4:				{bytes:UINT32_T,	type:"int"},
	expired_exemptions:			{bytes:UINT32_T,	type:"int"},
	expired_restrictions:		{bytes:UINT32_T,	type:"int"},
	logon_mod:			{bytes:LEN_MODNAME+1,		type:"str"},
	logoff_mod:			{bytes:LEN_MODNAME+1,		type:"str"},
	newuser_mod:		{bytes:LEN_MODNAME+1,		type:"str"},
	login_mod:			{bytes:LEN_MODNAME+1,		type:"str"},
	logout_mod:			{bytes:LEN_MODNAME+1,		type:"str"},
	sync_mod:			{bytes:LEN_MODNAME+1,		type:"str"},
	expire_mod:			{bytes:LEN_MODNAME+1,		type:"str"},
	ctrlkey_passthru:	{bytes:UINT32_T,			type:"int"},
	mods_dir:			{bytes:LEN_DIR+1,			type:"str"},
	logs_dir:			{bytes:LEN_DIR+1,			type:"str"},
	readmail_mod:		{bytes:LEN_CMD+1,			type:"str"},
	scanposts_mod:		{bytes:LEN_CMD+1,			type:"str"},
	scansubs_mod:		{bytes:LEN_CMD+1,			type:"str"},
	listmsgs_mod:		{bytes:LEN_CMD+1,			type:"str"},
	__PADDING2__:569,
	user_backup_level:	{bytes:UINT16_T,			type:"int"},
	mail_backup_level:	{bytes:UINT16_T,			type:"int"},
	validation_set:		{bytes:struct.validation_set_t, type:"lst", length: 10},
	sec_level:			{bytes:struct.sec_level_t, 	type:"lst", length: 100},
	command_shell:		{bytes:struct.cmd_shell_t,	type:"lst"},	// cmd_shell
};

/* main external programs file structure (xtrn.cnf) */
struct.xtrn={
	swap:			{bytes:struct.swap_t,		type:"lst"},
	xedit:			{bytes:struct.xedit_t,		type:"lst"},
	xtrnsec:		{bytes:struct.xtrnsec_t,	type:"lst"},
	xtrn:			{bytes:struct.xtrn_t,		type:"lst"},
	event:			{bytes:struct.event_t,		type:"lst"},
	natvpgm:		{bytes:struct.natvpgm_t,	type:"ntv"},
	hotkey:			{bytes:struct.hotkey_t,		type:"lst"},
	xtrn_misc:		{bytes:UINT32_T,			type:"int"}
};
	
/* main message group/sub file structure (msgs.cnf) */
struct.msg={
	max_qwkmsgs:	{bytes:UINT32_T,		type:"int"},
	mail_maxcrcs:	{bytes:UINT32_T, 		type:"int"},
	mail_maxage:	{bytes:UINT16_T, 		type:"int"},
	preqwk_ars:		{bytes:LEN_ARSTR+1, 	type:"str"},	// preqwk_ar
	smb_retry_time:	{bytes:UCHAR, 			type:"int"},
	max_qwkmsgage:	{bytes:UINT16_T, 		type:"int"},
	__PADDING1__:466,
	settings:		{bytes:UINT32_T, 		type:"int"},	// was msg_misc
	__PADDING2__:510,
	grp:			{bytes:struct.grp_t, 	type:"lst"},
	sub:			{bytes:struct.sub_t, 	type:"lst"},
	fido_addr_list:	{bytes:struct.faddr_t, 	type:"lst"},
	fido_default_origin:{bytes:51, 			type:"str"},
	fido_netmail_sem:	{bytes:LEN_DIR+1, 	type:"str"},
	fido_echomail_sem:	{bytes:LEN_DIR+1, 	type:"str"},
	fido_netmail_dir:	{bytes:LEN_DIR+1, 	type:"str"},
	fido_echomail_dir:	{bytes:LEN_DIR+1, 	type:"str"},
	fido_file_dir:		{bytes:LEN_DIR+1, 	type:"str"},
	fido_netmail_misc:	{bytes:UINT16_T, 	type:"int"},
	fido_netmail_cost:	{bytes:UINT32_T, 	type:"int"},
	fido_default_addr:	{bytes:struct.faddr_t, 	type:"obj"},
	__PADDING3__:56,
	qwknet_default_tagline:{bytes:128, 		type:"str"},
	qwknet_hub:		{bytes:struct.qhub_t, 	type:"lst"},
	__PADDING4__:64,
	__PADDING5__:11,
	postlink_num:	{bytes:UINT32_T, 		type:"int"},
	postlink_hub:	{bytes:struct.phub_t, 	type:"lst"},
	postlink_name:	{bytes:13, 				type:"str"},
	__PADDING6__:64,
	sys_inetaddr:	{bytes:128, 			type:"str"},
	inetmail_sem:	{bytes:LEN_DIR+1, 		type:"str"},
	inetmail_misc:	{bytes:UINT32_T, 		type:"str"},
	inetmail_cost:	{bytes:UINT32_T, 		type:"str"},
	smtpmail_sem:	{bytes:LEN_DIR+1, 		type:"str"}	
};

/************/
/* file.cnf */
/************/
struct.dir_t={
	lib_number:		{bytes:UINT16_T,		type:"int"},
	description:	{bytes:LEN_SLNAME+1,	type:"str"},
	name:			{bytes:LEN_SSNAME+1,	type:"str"},
	code_suffix:	{bytes:LEN_CODE+1,		type:"str"},
	data_dir:		{bytes:LEN_DIR+1,		type:"str"},
	ars:			{bytes:LEN_ARSTR+1,		type:"str"},
	upload_ars:		{bytes:LEN_ARSTR+1,		type:"str"},
	download_ars:	{bytes:LEN_ARSTR+1,		type:"str"},
	operator_ars:	{bytes:LEN_ARSTR+1,		type:"str"},
	path:			{bytes:LEN_DIR+1,		type:"str"},
	upload_sem:		{bytes:LEN_DIR+1,		type:"str"},
	max_files:		{bytes:UINT16_T,		type:"int"},
	extensions:		{bytes:41,				type:"str"},
	settings:		{bytes:UINT32_T,		type:"int"},
	seq_dev:		{bytes:1,				type:"int"},	// seqdev
	sort:			{bytes:1,				type:"int"},
	exempt_ars:		{bytes:LEN_ARSTR+1,		type:"str"},
	max_age:		{bytes:UINT16_T,		type:"int"},
	upload_pct:		{bytes:UINT16_T,		type:"int"},
	download_pct:	{bytes:UINT16_T,		type:"int"},
	__PADDING__:49
};
struct.lib_t={
	description:	{bytes:LEN_GLNAME+1,	type:"str"},
	name:			{bytes:LEN_GSNAME+1,	type:"str"},
	ars:			{bytes:LEN_ARSTR+1,		type:"str"},
	parent_path:	{bytes:48,				type:"str"},
	code_prefix:	{bytes:LEN_CODE+1,		type:"str"},
	sort:			{bytes:1,				type:"int"},
	settings:		{bytes:UINT32_T,		type:"int"},
	__PADDING__:34
};

/* Extractable File Types */
struct.fextr_t={
	extension:		{bytes:4,				type:"str"},
	cmd:			{bytes:LEN_CMD+1,		type:"str"},
	ars:			{bytes:LEN_ARSTR+1,		type:"str"},
	__PADDING__:16
};

/* Compressable File Types */
struct.fcomp_t={
	extension:		{bytes:4,				type:"str"},
	cmd:			{bytes:LEN_CMD+1,		type:"str"},
	ars:			{bytes:LEN_ARSTR+1,		type:"str"},
	__PADDING__:16
};

/* Viewable File Types */
struct.fview_t={
	extension:		{bytes:4,				type:"str"},
	cmd:			{bytes:LEN_CMD+1,		type:"str"},
	ars:			{bytes:LEN_ARSTR+1,		type:"str"},
	__PADDING__:16
};

/* Testable File Types */
struct.ftest_t={
	extension:		{bytes:4,				type:"str"},
	cmd:			{bytes:LEN_CMD+1,		type:"str"},
	working:		{bytes:41,				type:"str"},
	ars:			{bytes:LEN_ARSTR+1,		type:"str"},
	__PADDING__:16
};

/* Download Events */
struct.dlevent_t={
	extension:		{bytes:4,				type:"str"},
	cmd:			{bytes:LEN_CMD+1,		type:"str"},
	working:		{bytes:41,				type:"str"},
	ars:			{bytes:LEN_ARSTR+1,		type:"str"},
	__PADDING__:16
};

/* File Transfer Protocol (drivers) */
struct.prot_t={
	key:			{bytes:UCHAR,			type:"str"},
	name:			{bytes:26,				type:"str"},
	ulcmd:			{bytes:LEN_CMD+1,		type:"str"},
	dlcmd:			{bytes:LEN_CMD+1,		type:"str"},
	batulcmd:		{bytes:LEN_CMD+1,		type:"str"},
	batdlcmd:		{bytes:LEN_CMD+1,		type:"str"},
	blindcmd:		{bytes:LEN_CMD+1,		type:"str"},	// unused
	bicmd:			{bytes:LEN_CMD+1,		type:"str"},
	settings:		{bytes:UINT32_T,		type:"int"},	// misc
	ars:			{bytes:LEN_ARSTR+1,		type:"str"},
	__PADDING__:16
};

struct.altpath_t={
	path:			{bytes:LEN_DIR+1,		type:"str"},
	__PADDING__:16
};

struct.txtsec_t={
	name:			{bytes:41,				type:"str"},
	code:			{bytes:LEN_CODE+1,		type:"str"},
	ars:			{bytes:LEN_ARSTR+1,		type:"str"},
	__PADDING__:16
};

/* file transfer configuration (file.cnf) */
struct.file={
	min_dspace:		{bytes:UINT16_T,		type:"int"},
	max_batup:		{bytes:UINT16_T,		type:"int"},
	max_batdn:		{bytes:UINT16_T,		type:"int"},
	max_userxfer:	{bytes:UINT16_T,		type:"int"},
	__PADDING1__:4,
	cdt_up_pct:		{bytes:UINT16_T,		type:"int"},
	cdt_dn_pct:		{bytes:UINT16_T,		type:"int"},
	__PADDING2__:68,
	leech_pct:		{bytes:UINT16_T,		type:"int"},
	leech_sec:		{bytes:UINT16_T,		type:"int"},
	settings:		{bytes:UINT32_T,		type:"int"},
	__PADDING3__:60,
	fextr:			{bytes:struct.fextr_t,	type:"lst"},
	fcomp:			{bytes:struct.fcomp_t,	type:"lst"},
	fview:			{bytes:struct.fview_t,	type:"lst"},
	ftest:			{bytes:struct.ftest_t,	type:"lst"},
	dlevent:		{bytes:struct.dlevent_t,type:"lst"},
	prot:			{bytes:struct.prot_t,	type:"lst"},
	altpath:		{bytes:struct.altpath_t,type:"lst"},
	lib:			{bytes:struct.lib_t,	type:"lst"},
	dir:			{bytes:struct.dir_t,	type:"lst"},
	txtsec:			{bytes:struct.txtsec_t,	type:"lst"},
};