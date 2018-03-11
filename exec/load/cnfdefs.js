/* 	CNF file reader/writer - mcmlxxix - 2013
	$Id$
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
	faddr1:		{bytes:2,				type:"int"},
	faddr2:		{bytes:2,				type:"int"},
	faddr3:		{bytes:2,				type:"int"},
	faddr4:		{bytes:2,				type:"int"}
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
	faddr:			{bytes:struct.faddr_t,	type:"obj"},
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

/* NOTE: top-level data structures only below this point 
as they may contain references to the data structures above
and will not work if they are defined first */

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
	preqwk_ar:		{bytes:LEN_ARSTR+1, 	type:"str"},
	smb_retry_time:	{bytes:UCHAR, 			type:"int"},
	max_qwkmsgage:	{bytes:UINT16_T, 		type:"int"},
	__PADDING1__:466,
	settings:		{bytes:UINT32_T, 		type:"int"},	// was msg_misc
	__PADDING2__:510,
	grp:			{bytes:struct.grp_t, 	type:"lst"},
	sub:			{bytes:struct.sub_t, 	type:"lst"},
	faddr:			{bytes:struct.faddr_t, 	type:"lst"},
	origline:		{bytes:51, 				type:"str"},
	netmail_sem:	{bytes:LEN_DIR+1, 		type:"str"},
	echomail_sem:	{bytes:LEN_DIR+1, 		type:"str"},
	netmail_dir:	{bytes:LEN_DIR+1, 		type:"str"},
	echomail_dir:	{bytes:LEN_DIR+1, 		type:"str"},
	fidofile_dir:	{bytes:LEN_DIR+1, 		type:"str"},
	netmail_misc:	{bytes:UINT16_T, 		type:"int"},
	netmail_cost:	{bytes:UINT32_T, 		type:"int"},
	dflt_faddr:		{bytes:struct.faddr_t, 	type:"obj"},
	__PADDING3__:56,
	qnet_tagline:	{bytes:128, 			type:"str"},
	qhub:			{bytes:struct.qhub_t, 	type:"lst"},
	__PADDING4__:64,
	__PADDING5__:11,
	sys_psnum:		{bytes:UINT32_T, 		type:"int"},
	phub:			{bytes:struct.phub_t, 	type:"lst"},
	sys_psname:		{bytes:13, 				type:"str"},
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
	mnemonic:		{bytes:1,				type:"str"},
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