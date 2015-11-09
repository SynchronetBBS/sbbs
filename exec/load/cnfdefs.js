/* 	CNF file reader/writer - mcmlxxix - 2013
	$id: $
*/

/* miscellaneous constants required for cnf parsing
(if adding definitions to this file, make sure the required
constants are defined... if not, add them */

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
	lcmd:		{bytes:LEN_CMD+1,		type:"str"},
	rcmd:		{bytes:LEN_CMD+1,		type:"str"},
	misc:		{bytes:UINT32_T,		type:"int"},
	arstr:		{bytes:LEN_ARSTR+1,		type:"str"},
	type:		{bytes:UCHAR,			type:"int"},
	__PADDING__:15
};
struct.xtrnsec_t={
	name:		{bytes:41,				type:"str"},
	code:		{bytes:LEN_CODE+1,		type:"str"},
	arstr:		{bytes:LEN_ARSTR+1,		type:"str"},
	__PADDING__:16
};
struct.xtrn_t={
	sec:		{bytes:UINT16_T,		type:"int"},
	name:		{bytes:41,				type:"str"},
	code:		{bytes:LEN_CODE+1,		type:"str"},
	arstr:		{bytes:LEN_ARSTR+1,		type:"str"},
	run_arstr:	{bytes:LEN_ARSTR+1,		type:"str"},
	type:		{bytes:UCHAR,			type:"int"},
	misc:		{bytes:UINT32_T,		type:"int"},
	event:		{bytes:UCHAR,			type:"int"},
	cost:		{bytes:UINT32_T,		type:"int"},
	cmd:		{bytes:LEN_CMD+1,		type:"str"},
	clean:		{bytes:LEN_CMD+1,		type:"str"},
	path:		{bytes:LEN_DIR+1,		type:"str"},
	textra:		{bytes:UCHAR,			type:"int"},
	maxtime:	{bytes:UCHAR,			type:"int"},
	__PADDING__:14
};
struct.event_t={
	code:		{bytes:LEN_CODE+1,		type:"str"},
	cmd:		{bytes:LEN_CMD+1,		type:"str"},
	days:		{bytes:1,				type:"int"},
	time:		{bytes:UINT16_T,		type:"int"},
	node:		{bytes:UINT16_T,		type:"int"},
	misc:		{bytes:UINT32_T,		type:"int"},
	dir:		{bytes:LEN_DIR+1,		type:"str"},
	freq:		{bytes:UINT16_T,		type:"int"},
	mdays:		{bytes:UINT32_T,		type:"int"},
	months:		{bytes:UINT16_T,		type:"int"},
	__PADDING__:8
};
struct.natvpgm_t={
	name:		{bytes:13,				type:"str"},
	misc:		{bytes:UINT32_T,		type:"int"}
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
	grp:		{bytes:UINT16_T,		type:"int"},
	lname:		{bytes:LEN_SLNAME+1,	type:"str"},
	sname:		{bytes:LEN_SSNAME+1,	type:"str"},
	qwkname:	{bytes:11,				type:"str"},
	code_suffix:{bytes:LEN_CODE+1,		type:"str"},
	data_dir:	{bytes:LEN_DIR+1,		type:"str"},
	arstr:		{bytes:LEN_ARSTR+1,		type:"str"},
	read_arstr:	{bytes:LEN_ARSTR+1,		type:"str"},
	post_arstr:	{bytes:LEN_ARSTR+1,		type:"str"},
	op_arstr:	{bytes:LEN_ARSTR+1,		type:"str"},
	// uchar		*ar,
				// *read_ar,
				// *post_ar,
				// *op_ar,
	misc:		{bytes:UINT32_T,		type:"int"},
	tagline:	{bytes:81,				type:"str"},
	origline:	{bytes:51,				type:"str"},
	post_sem:	{bytes:LEN_DIR+1,		type:"str"},
	newsgroup:	{bytes:LEN_DIR+1,		type:"str"},
	faddr:		{bytes:struct.faddr_t,	type:"obj"},
	maxmsgs:	{bytes:UINT32_T,		type:"int"},
	maxcrcs:	{bytes:UINT32_T,		type:"int"},
	maxage:		{bytes:UINT16_T,		type:"int"},
	ptridx:		{bytes:UINT16_T,		type:"int"},
	mod_arstr:	{bytes:LEN_ARSTR+1,		type:"str"},
	// *mod_ar;
	qwkconf:	{bytes:UINT16_T,		type:"int"},
	__PADDING__:53
};
struct.grp_t={
	lname:		{bytes:LEN_GLNAME+1,	type:"str"},
	sname:		{bytes:LEN_GSNAME+1,	type:"str"},
	arstr:		{bytes:LEN_ARSTR+1,		type:"str"},
	code_prefix:{bytes:LEN_CODE+1,		type:"str"},
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
	msg_misc:		{bytes:UINT32_T, 		type:"int"},
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
	
