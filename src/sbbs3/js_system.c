/* Synchronet JavaScript "system" Object */
// vi: tabstop=4

/* $Id: js_system.c,v 1.179 2020/03/31 18:32:34 rswindell Exp $ */

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

#include "sbbs.h"
#include "js_request.h"

#ifdef JAVASCRIPT

typedef struct {
	scfg_t* cfg;
	int nodefile;
	int nodegets;
} js_system_private_t;

extern JSClass js_system_class;

/* System Object Properties */
enum {
	 SYS_PROP_NAME
	,SYS_PROP_OP
	,SYS_PROP_ID
	,SYS_PROP_MISC
	,SYS_PROP_INETADDR
	,SYS_PROP_LOCATION
	,SYS_PROP_TIMEZONE
	,SYS_PROP_PWDAYS
	,SYS_PROP_MINPWLEN
	,SYS_PROP_MAXPWLEN
	,SYS_PROP_DELDAYS
	,SYS_PROP_AUTODEL

	,SYS_PROP_LASTUSER
	,SYS_PROP_LASTUSERON
	,SYS_PROP_FREEDISKSPACE
	,SYS_PROP_FREEDISKSPACEK

	,SYS_PROP_NODES
	,SYS_PROP_LASTNODE

	,SYS_PROP_NEW_PASS
	,SYS_PROP_NEW_MAGIC
	,SYS_PROP_NEW_LEVEL
	,SYS_PROP_NEW_FLAGS1
	,SYS_PROP_NEW_FLAGS2
	,SYS_PROP_NEW_FLAGS3
	,SYS_PROP_NEW_FLAGS4
	,SYS_PROP_NEW_REST
	,SYS_PROP_NEW_EXEMPT
	,SYS_PROP_NEW_CDT
	,SYS_PROP_NEW_MIN
	,SYS_PROP_NEW_SHELL
	,SYS_PROP_NEW_XEDIT
	,SYS_PROP_NEW_MISC
	,SYS_PROP_NEW_PROT
	,SYS_PROP_NEW_EXPIRE
	,SYS_PROP_NEW_UQ

	,SYS_PROP_EXPIRED_LEVEL
	,SYS_PROP_EXPIRED_FLAGS1
	,SYS_PROP_EXPIRED_FLAGS2
	,SYS_PROP_EXPIRED_FLAGS3
	,SYS_PROP_EXPIRED_FLAGS4
	,SYS_PROP_EXPIRED_REST
	,SYS_PROP_EXPIRED_EXEMPT

	/* directories */
	,SYS_PROP_NODE_DIR
	,SYS_PROP_CTRL_DIR
	,SYS_PROP_DATA_DIR
	,SYS_PROP_TEXT_DIR
	,SYS_PROP_TEMP_DIR
	,SYS_PROP_EXEC_DIR
	,SYS_PROP_MODS_DIR
	,SYS_PROP_LOGS_DIR

	/* msclock() access */
	,SYS_PROP_CLOCK
	,SYS_PROP_CLOCK_PER_SEC
	,SYS_PROP_TIMER

	/* filenames */
	,SYS_PROP_DEVNULL
	,SYS_PROP_TEMP_PATH
	,SYS_PROP_CMD_SHELL

	/* last */
	,SYS_PROP_LOCAL_HOSTNAME
};

static JSBool js_system_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval idval;
	char		str[128];
	char*		p=NULL;
    jsint       tiny;
	JSString*	js_str;
	ulong		val;
	jsrefcount	rc;

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;
	scfg_t* cfg = sys->cfg;

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch(tiny) {
#ifndef JSDOOR
		case SYS_PROP_NAME:
	        p=cfg->sys_name;
			break;
		case SYS_PROP_OP:
			p=cfg->sys_op;
			break;
		case SYS_PROP_ID:
			p=cfg->sys_id;
			break;
		case SYS_PROP_MISC:
			*vp=UINT_TO_JSVAL(cfg->sys_misc);
			break;
		case SYS_PROP_INETADDR:
			p=cfg->sys_inetaddr;
			break;
		case SYS_PROP_LOCATION:
			p=cfg->sys_location;
			break;
		case SYS_PROP_TIMEZONE:
			sys_timezone(cfg);
			*vp = INT_TO_JSVAL(cfg->sys_timezone);
			break;
		case SYS_PROP_NODES:
			*vp = INT_TO_JSVAL(cfg->sys_nodes);
			break;
		case SYS_PROP_LASTNODE:
			*vp = INT_TO_JSVAL(cfg->sys_lastnode);
			break;
		case SYS_PROP_PWDAYS:
			*vp = INT_TO_JSVAL(cfg->sys_pwdays);
			break;
		case SYS_PROP_MINPWLEN:
			*vp = INT_TO_JSVAL(MIN_PASS_LEN);
			break;
		case SYS_PROP_MAXPWLEN:
			*vp = INT_TO_JSVAL(LEN_PASS);
			break;
		case SYS_PROP_DELDAYS:
			*vp = INT_TO_JSVAL(cfg->sys_deldays);
			break;
		case SYS_PROP_AUTODEL:
			*vp = INT_TO_JSVAL(cfg->sys_autodel);
			break;
		case SYS_PROP_LASTUSER:
			*vp = INT_TO_JSVAL(lastuser(cfg));
			break;
		case SYS_PROP_LASTUSERON:
			p=lastuseron;
			break;
#endif
		case SYS_PROP_FREEDISKSPACE:
		case SYS_PROP_FREEDISKSPACEK:
			rc=JS_SUSPENDREQUEST(cx);
			if(tiny==SYS_PROP_FREEDISKSPACE)
				val = getfreediskspace(cfg->temp_dir,0);
			else
				val = getfreediskspace(cfg->temp_dir,1024);
			JS_RESUMEREQUEST(cx, rc);
			*vp=DOUBLE_TO_JSVAL((double)val);
			break;
#ifndef JSDOOR
		case SYS_PROP_NEW_PASS:
			p=cfg->new_pass;
			break;
		case SYS_PROP_NEW_MAGIC:
			p=cfg->new_magic;
			break;
		case SYS_PROP_NEW_LEVEL:
			*vp = INT_TO_JSVAL(cfg->new_level);
			break;
		case SYS_PROP_NEW_FLAGS1:
			*vp = INT_TO_JSVAL(cfg->new_flags1);
			break;
		case SYS_PROP_NEW_FLAGS2:
			*vp = INT_TO_JSVAL(cfg->new_flags2);
			break;
		case SYS_PROP_NEW_FLAGS3:
			*vp = INT_TO_JSVAL(cfg->new_flags3);
			break;
		case SYS_PROP_NEW_FLAGS4:
			*vp = INT_TO_JSVAL(cfg->new_flags4);
			break;
		case SYS_PROP_NEW_REST:
			*vp = INT_TO_JSVAL(cfg->new_rest);
			break;
		case SYS_PROP_NEW_EXEMPT:
			*vp = INT_TO_JSVAL(cfg->new_exempt);
			break;
		case SYS_PROP_NEW_CDT:
			*vp=UINT_TO_JSVAL(cfg->new_cdt);
			break;
		case SYS_PROP_NEW_MIN:
			*vp=UINT_TO_JSVAL(cfg->new_min);
			break;
		case SYS_PROP_NEW_SHELL:
			if(cfg->new_shell<cfg->total_shells)
				p=cfg->shell[cfg->new_shell]->code;
			break;
		case SYS_PROP_NEW_XEDIT:
			p=cfg->new_xedit;
			break;
		case SYS_PROP_NEW_MISC:
			*vp=UINT_TO_JSVAL(cfg->new_misc);
			break;
		case SYS_PROP_NEW_PROT:
			sprintf(str,"%c",cfg->new_prot);
			p=str;
			break;
		case SYS_PROP_NEW_EXPIRE:
			*vp=UINT_TO_JSVAL(cfg->new_expire);
			break;
		case SYS_PROP_NEW_UQ:
			*vp=UINT_TO_JSVAL(cfg->uq);
			break;

		case SYS_PROP_EXPIRED_LEVEL:
			*vp = INT_TO_JSVAL(cfg->expired_level);
			break;
		case SYS_PROP_EXPIRED_FLAGS1:
			*vp = INT_TO_JSVAL(cfg->expired_flags1);
			break;
		case SYS_PROP_EXPIRED_FLAGS2:
			*vp = INT_TO_JSVAL(cfg->expired_flags2);
			break;
		case SYS_PROP_EXPIRED_FLAGS3:
			*vp = INT_TO_JSVAL(cfg->expired_flags3);
			break;
		case SYS_PROP_EXPIRED_FLAGS4:
			*vp = INT_TO_JSVAL(cfg->expired_flags4);
			break;
		case SYS_PROP_EXPIRED_REST:
			*vp = INT_TO_JSVAL(cfg->expired_rest);
			break;
		case SYS_PROP_EXPIRED_EXEMPT:
			*vp = INT_TO_JSVAL(cfg->expired_exempt);
			break;
		case SYS_PROP_NODE_DIR:
			p=cfg->node_dir;
			break;
#endif
		case SYS_PROP_CTRL_DIR:
			p=cfg->ctrl_dir;
			break;
		case SYS_PROP_DATA_DIR:
			p=cfg->data_dir;
			break;
		case SYS_PROP_TEXT_DIR:
			p=cfg->text_dir;
			break;
		case SYS_PROP_TEMP_DIR:
			p=cfg->temp_dir;
			break;
		case SYS_PROP_EXEC_DIR:
			p=cfg->exec_dir;
			break;
		case SYS_PROP_MODS_DIR:
			p=cfg->mods_dir;
			break;
		case SYS_PROP_LOGS_DIR:
			p=cfg->logs_dir;
			break;

		case SYS_PROP_DEVNULL:
			p=_PATH_DEVNULL;
			break;
		case SYS_PROP_TEMP_PATH:
			p=_PATH_TMP;
			break;

		case SYS_PROP_CMD_SHELL:
			rc=JS_SUSPENDREQUEST(cx);
			p=os_cmdshell();
			JS_RESUMEREQUEST(cx, rc);
			break;

		case SYS_PROP_CLOCK:
			*vp=DOUBLE_TO_JSVAL((double)msclock());
			break;
		case SYS_PROP_CLOCK_PER_SEC:
			*vp=UINT_TO_JSVAL(MSCLOCKS_PER_SEC);
			break;
		case SYS_PROP_TIMER:
			*vp=DOUBLE_TO_JSVAL(xp_timer());
			break;

		case SYS_PROP_LOCAL_HOSTNAME:
			rc=JS_SUSPENDREQUEST(cx);
			gethostname(str,sizeof(str));
			JS_RESUMEREQUEST(cx, rc);
			p=str;
			break;
	}

	if(p!=NULL) {	/* string property */
		if((js_str=JS_NewStringCopyZ(cx, p))==NULL)
			return(JS_FALSE);
		*vp = STRING_TO_JSVAL(js_str);
	}

	return(JS_TRUE);
}

static JSBool js_system_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsval idval;
    jsint       tiny;

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

#ifndef JSDOOR
	switch(tiny) {
		case SYS_PROP_MISC:
			JS_ValueToInt32(cx, *vp, &sys->cfg->sys_misc);
			break;
	}
#endif

	return(TRUE);
}


#define SYSOBJ_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static jsSyncPropertySpec js_system_properties[] = {
/*		 name,						tinyid,				flags,				ver	*/

#ifndef JSDOOR
	{	"name",						SYS_PROP_NAME,		SYSOBJ_FLAGS,		310  },
	{	"operator",					SYS_PROP_OP,		SYSOBJ_FLAGS,		310  },
	{	"qwk_id",					SYS_PROP_ID,		SYSOBJ_FLAGS,		310  },
	{	"settings",					SYS_PROP_MISC,		JSPROP_ENUMERATE,	310  },
	{	"inetaddr",					SYS_PROP_INETADDR,	JSPROP_READONLY,	310  },	/* alias */
	{	"inet_addr",				SYS_PROP_INETADDR,	SYSOBJ_FLAGS,		311  },
	{	"location",					SYS_PROP_LOCATION,	SYSOBJ_FLAGS,		310  },
	{	"timezone",					SYS_PROP_TIMEZONE,	SYSOBJ_FLAGS,		310  },
	{	"pwdays",					SYS_PROP_PWDAYS,	SYSOBJ_FLAGS,		310  },
	{	"min_password_length",		SYS_PROP_MINPWLEN,	SYSOBJ_FLAGS,		31702  },
	{	"max_password_length",		SYS_PROP_MAXPWLEN,	SYSOBJ_FLAGS,		31702  },
	{	"deldays",					SYS_PROP_DELDAYS,	SYSOBJ_FLAGS,		310  },
	{	"autodel",					SYS_PROP_AUTODEL,	SYSOBJ_FLAGS,		31702  },

	{	"lastuser",					SYS_PROP_LASTUSER		,SYSOBJ_FLAGS,	311  },
	{	"lastuseron",				SYS_PROP_LASTUSERON		,SYSOBJ_FLAGS,	310  },
#endif
	{	"freediskspace",			SYS_PROP_FREEDISKSPACE	,SYSOBJ_FLAGS,	310  },
	{	"freediskspacek",			SYS_PROP_FREEDISKSPACEK	,SYSOBJ_FLAGS,	310  },

#ifndef JSDOOR
	{	"nodes",					SYS_PROP_NODES,		SYSOBJ_FLAGS,		310  },
	{	"lastnode",					SYS_PROP_LASTNODE,	SYSOBJ_FLAGS,		310  },

	{	"newuser_password",			SYS_PROP_NEW_PASS		,SYSOBJ_FLAGS,	310  },
	{	"newuser_magic_word",		SYS_PROP_NEW_MAGIC		,SYSOBJ_FLAGS,	310  },
	{	"newuser_level",			SYS_PROP_NEW_LEVEL		,SYSOBJ_FLAGS,	310  },
	{	"newuser_flags1",			SYS_PROP_NEW_FLAGS1		,SYSOBJ_FLAGS,	310  },
	{	"newuser_flags2",			SYS_PROP_NEW_FLAGS2		,SYSOBJ_FLAGS,	310  },
	{	"newuser_flags3",			SYS_PROP_NEW_FLAGS3		,SYSOBJ_FLAGS,	310  },
	{	"newuser_flags4",			SYS_PROP_NEW_FLAGS4		,SYSOBJ_FLAGS,	310  },
	{	"newuser_restrictions",		SYS_PROP_NEW_REST		,SYSOBJ_FLAGS,	310  },
	{	"newuser_exemptions",		SYS_PROP_NEW_EXEMPT		,SYSOBJ_FLAGS,	310  },
	{	"newuser_credits",			SYS_PROP_NEW_CDT		,SYSOBJ_FLAGS,	310  },
	{	"newuser_minutes",			SYS_PROP_NEW_MIN		,SYSOBJ_FLAGS,	310  },
	{	"newuser_command_shell",	SYS_PROP_NEW_SHELL		,SYSOBJ_FLAGS,	310  },
	{	"newuser_editor",			SYS_PROP_NEW_XEDIT		,SYSOBJ_FLAGS,	310  },
	{	"newuser_settings",			SYS_PROP_NEW_MISC		,SYSOBJ_FLAGS,	310  },
	{	"newuser_download_protocol",SYS_PROP_NEW_PROT		,SYSOBJ_FLAGS,	310  },
	{	"newuser_expiration_days",	SYS_PROP_NEW_EXPIRE		,SYSOBJ_FLAGS,	310  },
	{	"newuser_questions",		SYS_PROP_NEW_UQ			,SYSOBJ_FLAGS,	310  },

	{	"expired_level",			SYS_PROP_EXPIRED_LEVEL	,SYSOBJ_FLAGS,	310  },
	{	"expired_flags1",			SYS_PROP_EXPIRED_FLAGS1	,SYSOBJ_FLAGS,	310  },
	{	"expired_flags2",			SYS_PROP_EXPIRED_FLAGS2	,SYSOBJ_FLAGS,	310  },
	{	"expired_flags3",			SYS_PROP_EXPIRED_FLAGS3	,SYSOBJ_FLAGS,	310  },
	{	"expired_flags4",			SYS_PROP_EXPIRED_FLAGS4	,SYSOBJ_FLAGS,	310  },
	{	"expired_restrictions",		SYS_PROP_EXPIRED_REST	,SYSOBJ_FLAGS,	310  },
	{	"expired_exemptions",		SYS_PROP_EXPIRED_EXEMPT	,SYSOBJ_FLAGS,	310  },	

	/* directories */
	{	"node_dir",					SYS_PROP_NODE_DIR		,SYSOBJ_FLAGS,	310  },	
#endif
	{	"ctrl_dir",					SYS_PROP_CTRL_DIR		,SYSOBJ_FLAGS,	310  },	
	{	"data_dir",					SYS_PROP_DATA_DIR		,SYSOBJ_FLAGS,	310  },	
	{	"text_dir",					SYS_PROP_TEXT_DIR		,SYSOBJ_FLAGS,	310  },	
	{	"temp_dir",					SYS_PROP_TEMP_DIR		,SYSOBJ_FLAGS,	310  },	
	{	"exec_dir",					SYS_PROP_EXEC_DIR		,SYSOBJ_FLAGS,	310  },	
	{	"mods_dir",					SYS_PROP_MODS_DIR		,SYSOBJ_FLAGS,	310  },	
	{	"logs_dir",					SYS_PROP_LOGS_DIR		,SYSOBJ_FLAGS,	310  },	

	/* filenames */
	{	"devnull",					SYS_PROP_DEVNULL		,SYSOBJ_FLAGS,	311  },
	{	"temp_path",				SYS_PROP_TEMP_PATH		,SYSOBJ_FLAGS,	312	 },
	{	"cmd_shell",				SYS_PROP_CMD_SHELL		,SYSOBJ_FLAGS,	314	 },

	/* clock access */
	{	"clock_ticks",				SYS_PROP_CLOCK			,SYSOBJ_FLAGS,	311  },
	{	"clock_ticks_per_second",	SYS_PROP_CLOCK_PER_SEC	,SYSOBJ_FLAGS,	311  },
	{	"timer",					SYS_PROP_TIMER			,SYSOBJ_FLAGS,	314	 },

	/* last */
	{	"local_host_name",			SYS_PROP_LOCAL_HOSTNAME	,SYSOBJ_FLAGS,	311  },
	{0}
};

#ifdef BUILD_JSDOCS
static char* sys_prop_desc[] = {
	 "BBS name"
	,"operator name"
	,"system QWK-ID (for QWK packets)"
	,"settings bitfield (see <tt>SYS_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions)"
	,"Internet address (host or domain name)"
	,"location (city, state)"
	,"timezone (use <i>system.zonestr()</i> to get string representation)"
	,"days between forced user password changes (<tt>0</tt>=<i>never</i>)"
	,"minimum number of characters in user passwords"
	,"maximum number of characters in user passwords"
	,"days to preserve deleted user records, record will not be reused/overwritten during this period"
	,"days of user inactivity before auto-deletion (<tt>0</tt>=<i>disabled</i>), N/A to P-exempt users"

	,"last user record number in user database (includes deleted and inactive user records)"
	,"name of last user to logoff"
	,"amount of free disk space (in bytes)"
	,"amount of free disk space (in kilobytes)"

	,"total number of BBS nodes"
	,"last displayable node number"

	,"new user password"
	,"new user magic word"
	,"new user level"
	,"new user flag set #1"
	,"new user flag set #2"
	,"new user flag set #3"
	,"new user flag set #4"
	,"new user restriction flags"
	,"new user exemption flags"
	,"new user credits"
	,"new user extra minutes"
	,"new user command shell"
	,"new user external editor"
	,"new user settings"
	,"new user file transfer protocol (command key)"
	,"new user expiration days"
	,"new user questions bitfield (see <tt>UQ_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions)"

	,"expired user level"
	,"expired user flag set #1"
	,"expired user flag set #2"
	,"expired user flag set #3"
	,"expired user flag set #4"
	,"expired user restriction flags"
	,"expired user exemption flags"

	/* directories */
	,"node directory"
	,"control file directory"
	,"data file directory"
	,"text file directory"
	,"temporary file directory"
	,"executable file directory"
	,"modified modules directory (optional)"
	,"log file directory"

	/* filenames */
	,"platform-specific \"null\" device filename"
	,"platform-specific temporary file directory"
	,"platform-specific command processor/shell"

	/* clock */
	,"amount of elapsed processor time in clock 'ticks'"
	,"number of clock ticks per second"
	,"high-resolution timer, in seconds (fractional seconds supported)"

	/* INSERT new tabled properties here */
	,"private host name that uniquely identifies this system on the local network"

	/* Manually created (non-tabled) properties */
	,"public host name that uniquely identifies this system on the Internet (usually the same as <i>system.inet_addr</i>)"
	,"socket library version information"
	,"time/date system was brought online (in time_t format)"
	,"Synchronet full version information (e.g. '3.10k Beta Debug')"
	,"date and time compiled"
	,"Synchronet version number (e.g. '3.10')"
	,"Synchronet revision letter (e.g. 'k')"
	,"Synchronet alpha/beta designation (e.g. ' beta')"
	,"Synchronet version notice (includes version and platform)"
	,"Synchronet version number in decimal (e.g. 31301 for v3.13b)"
	,"Synchronet version number in hexadecimal (e.g. 0x31301 for v3.13b)"
	,"platform description (e.g. 'Win32', 'Linux', 'FreeBSD')"
	,"architecture description (e.g. 'i386', 'i686', 'x86_64')"
	,"message base library version information"
	,"compiler used to build Synchronet"
	,"Synchronet copyright display"
	,"JavaScript engine version information"
	,"operating system version information"
	,"array of FidoNet Technology Network (FTN) addresses associated with this system"
	,NULL
};
#endif


/* System Stats Propertiess */
enum {
	 SYSSTAT_PROP_LOGONS
	,SYSSTAT_PROP_LTODAY
	,SYSSTAT_PROP_TIMEON
	,SYSSTAT_PROP_TTODAY
	,SYSSTAT_PROP_ULS
	,SYSSTAT_PROP_ULB
	,SYSSTAT_PROP_DLS
	,SYSSTAT_PROP_DLB
	,SYSSTAT_PROP_PTODAY
	,SYSSTAT_PROP_ETODAY
	,SYSSTAT_PROP_FTODAY
	,SYSSTAT_PROP_NUSERS

	,SYSSTAT_PROP_TOTALUSERS
	,SYSSTAT_PROP_TOTALFILES
	,SYSSTAT_PROP_TOTALMSGS
	,SYSSTAT_PROP_TOTALMAIL
	,SYSSTAT_PROP_FEEDBACK

	,SYSSTAT_PROP_NODE_GETS
};

#ifndef JSDOOR
static JSBool js_sysstats_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval idval;
    jsint       tiny;
	stats_t		stats;
	uint		i;
	ulong		l;
	jsrefcount	rc;

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, "JS_GetPrivate failure in %s", __FUNCTION__);
		return JS_FALSE;
	}
	scfg_t* cfg = sys->cfg;

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	if(tiny < SYSSTAT_PROP_TOTALUSERS) {
		rc=JS_SUSPENDREQUEST(cx);
		if(!getstats(cfg, 0, &stats)) {
			JS_RESUMEREQUEST(cx, rc);
			JS_ReportError(cx, "getstats failure in %s", __FUNCTION__);
			return JS_FALSE;
		}
		JS_RESUMEREQUEST(cx, rc);
	}

	switch(tiny) {
		case SYSSTAT_PROP_LOGONS:
			*vp=UINT_TO_JSVAL(stats.logons);
			break;
		case SYSSTAT_PROP_LTODAY:
			*vp=UINT_TO_JSVAL(stats.ltoday);
			break;
		case SYSSTAT_PROP_TIMEON:
			*vp=UINT_TO_JSVAL(stats.timeon);
			break;
		case SYSSTAT_PROP_TTODAY:
			*vp=UINT_TO_JSVAL(stats.ttoday);
			break;
		case SYSSTAT_PROP_ULS:
			*vp=UINT_TO_JSVAL(stats.uls);
			break;
		case SYSSTAT_PROP_ULB:
			*vp=UINT_TO_JSVAL(stats.ulb);
			break;
		case SYSSTAT_PROP_DLS:
			*vp=UINT_TO_JSVAL(stats.dls);
			break;
		case SYSSTAT_PROP_DLB:
			*vp=UINT_TO_JSVAL(stats.dlb);
			break;
		case SYSSTAT_PROP_PTODAY:
			*vp=UINT_TO_JSVAL(stats.ptoday);
			break;
		case SYSSTAT_PROP_ETODAY:
			*vp=UINT_TO_JSVAL(stats.etoday);
			break;
		case SYSSTAT_PROP_FTODAY:
			*vp=UINT_TO_JSVAL(stats.ftoday);
			break;
		case SYSSTAT_PROP_NUSERS:
			*vp=UINT_TO_JSVAL(stats.nusers);
			break;

		case SYSSTAT_PROP_TOTALUSERS:
			rc=JS_SUSPENDREQUEST(cx);
			*vp = INT_TO_JSVAL(total_users(cfg));
			JS_RESUMEREQUEST(cx, rc);
			break;
		case SYSSTAT_PROP_TOTALMSGS:
			l=0;
			rc=JS_SUSPENDREQUEST(cx);
			for(i=0;i<cfg->total_subs;i++)
				l+=getposts(cfg,i); 
			JS_RESUMEREQUEST(cx, rc);
			*vp=DOUBLE_TO_JSVAL((double)l); 
			break;
		case SYSSTAT_PROP_TOTALFILES:
			l=0;
			rc=JS_SUSPENDREQUEST(cx);
			for(i=0;i<cfg->total_dirs;i++)
				l+=getfiles(cfg,i);
			JS_RESUMEREQUEST(cx, rc);
			*vp=DOUBLE_TO_JSVAL((double)l);
			break;
		case SYSSTAT_PROP_TOTALMAIL:
			rc=JS_SUSPENDREQUEST(cx);
			*vp = INT_TO_JSVAL(getmail(cfg, /* user: */0, /* Sent: */FALSE, /* SPAM: */FALSE));
			JS_RESUMEREQUEST(cx, rc);
			break;
		case SYSSTAT_PROP_FEEDBACK:
			rc=JS_SUSPENDREQUEST(cx);
			*vp = INT_TO_JSVAL(getmail(cfg, /* user: */1, /* Sent: */FALSE, /* SPAM: */FALSE));
			JS_RESUMEREQUEST(cx, rc);
			break;

		case SYSSTAT_PROP_NODE_GETS:
			*vp = INT_TO_JSVAL(sys->nodegets);
			break;
	}

	return(TRUE);
}

#define SYSSTAT_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static jsSyncPropertySpec js_sysstats_properties[] = {
/*		 name,						tinyid,						flags,			ver	*/

	{	"total_logons",				SYSSTAT_PROP_LOGONS,		SYSSTAT_FLAGS,	310 },
	{	"logons_today",				SYSSTAT_PROP_LTODAY,		SYSSTAT_FLAGS,	310 },
	{	"total_timeon",				SYSSTAT_PROP_TIMEON,		SYSSTAT_FLAGS,	310 },
	{	"timeon_today",				SYSSTAT_PROP_TTODAY,		SYSSTAT_FLAGS,	310 },
	{	"total_files",				SYSSTAT_PROP_TOTALFILES,	SYSSTAT_FLAGS,	310 },
	{	"files_uploaded_today",		SYSSTAT_PROP_ULS,			SYSSTAT_FLAGS,	310 },
	{	"bytes_uploaded_today",		SYSSTAT_PROP_ULB,			SYSSTAT_FLAGS,	310 },
	{	"files_downloaded_today",	SYSSTAT_PROP_DLS,			SYSSTAT_FLAGS,	310 },
	{	"bytes_downloaded_today",	SYSSTAT_PROP_DLB,			SYSSTAT_FLAGS,	310 },
	{	"total_messages",			SYSSTAT_PROP_TOTALMSGS,		SYSSTAT_FLAGS,	310 },
	{	"messages_posted_today",	SYSSTAT_PROP_PTODAY,		SYSSTAT_FLAGS,	310 },
	{	"total_email",				SYSSTAT_PROP_TOTALMAIL,		SYSSTAT_FLAGS,	310 },
	{	"email_sent_today",			SYSSTAT_PROP_ETODAY,		SYSSTAT_FLAGS,	310 },
	{	"total_feedback",			SYSSTAT_PROP_FEEDBACK,		SYSSTAT_FLAGS,	310 },
	{	"feedback_sent_today",		SYSSTAT_PROP_FTODAY,		SYSSTAT_FLAGS,	310 },
	{	"total_users",				SYSSTAT_PROP_TOTALUSERS,	SYSSTAT_FLAGS,	310 },
	{	"new_users_today",			SYSSTAT_PROP_NUSERS,		SYSSTAT_FLAGS,	310 },
	{	"node_gets",				SYSSTAT_PROP_NODE_GETS,		JSPROP_READONLY, 31702 },
	{0}
};

#if !defined(JSDOOR) && defined(BUILD_JSDOCS)
static char* sysstat_prop_desc[] = {
	 "total logons"
	,"logons today"
	,"total time used"
	,"time used today"
	,"total files in file bases"
	,"files uploaded today"
	,"bytes uploaded today"
	,"files downloaded today"
	,"bytes downloaded today"
	,"total messages in message bases"
	,"messages posted today"
	,"total messages in mail base"
	,"email sent today"
	,"total feedback messages waiting"
	,"feedback sent today"
	,"total user records (does not include deleted or inactive user records)"
	,"new users today"
	,NULL
};
#endif

static JSBool js_sysstats_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*			name=NULL;
	JSBool			ret;

	if(id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		
		JS_IdToValue(cx, id, &idval);
		if(JSVAL_IS_STRING(idval)) {
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
			HANDLE_PENDING(cx, name);
		}
	}

	ret=js_SyncResolve(cx, obj, name, js_sysstats_properties, NULL, NULL, 0);
	if(name)
		free(name);
	return ret;
}

static JSBool js_sysstats_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_sysstats_resolve(cx, obj, JSID_VOID));
}

static JSClass js_sysstats_class = {
     "Stats"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_sysstats_get		/* getProperty	*/
	,JS_StrictPropertyStub	/* setProperty	*/
	,js_sysstats_enumerate	/* enumerate	*/
	,js_sysstats_resolve	/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};

static JSBool
js_alias(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		p;
	char		buf[128];
	JSString*	js_str;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(0));
		return(JS_TRUE);
	}

	JSSTRING_TO_ASTRING(cx, js_str, p, 128, NULL);
	if(p==NULL) {
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(0));
		return(JS_TRUE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	p=alias(sys->cfg,p,buf);
	JS_RESUMEREQUEST(cx, rc);

	if((js_str = JS_NewStringCopyZ(cx, p))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return(JS_TRUE);
}

static JSBool
js_username(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	int32		val;
	char		buf[128];
	JSString*	js_str;
	char*		cstr;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;

	val=0;
	JS_ValueToInt32(cx,argv[0],&val);

	rc=JS_SUSPENDREQUEST(cx);
	cstr=username(sys->cfg,val,buf);
	JS_RESUMEREQUEST(cx, rc);
	if((js_str = JS_NewStringCopyZ(cx, cstr))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return(JS_TRUE);
}

static JSBool
js_matchuser(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		p;
	JSString*	js_str;
	BOOL		sysop_alias=TRUE;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(0));
		return(JS_TRUE);
	}

	if(argc>1)
		JS_ValueToBoolean(cx,argv[1],&sysop_alias);

	JSSTRING_TO_ASTRING(cx, js_str, p, (LEN_ALIAS > LEN_NAME) ? LEN_ALIAS+2:LEN_NAME+2, NULL);
	if(p==NULL) {
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(0));
		return(JS_TRUE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(matchuser(sys->cfg,p,sysop_alias)));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_matchuserdata(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		p;
	JSString*	js_str;
	int32		offset=0;
	int32		usernumber=0;
	int			len;
	jsrefcount	rc;
	BOOL		match_next=FALSE;
	int 		argnum=2;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;

	JS_ValueToInt32(cx,argv[0],&offset);
	rc=JS_SUSPENDREQUEST(cx);
	len=user_rec_len(offset);
	JS_RESUMEREQUEST(cx, rc);
	if(len<0) {
		JS_ReportError(cx,"Invalid user data offset: %d", offset);
		return(JS_FALSE);
	}

	if((js_str=JS_ValueToString(cx, argv[1]))==NULL) {
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(0));
		return(JS_TRUE);
	}

	if(JSVAL_IS_NUMBER(argv[argnum]))
		JS_ValueToInt32(cx, argv[argnum++], &usernumber);
	if(JSVAL_IS_BOOLEAN(argv[argnum]))
		JS_ValueToBoolean(cx, argv[argnum], &match_next);
	
	JSSTRING_TO_ASTRING(cx, js_str, p, 128, NULL);
	if(p==NULL) {
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(0));
		return(JS_TRUE);
	}
	
	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(userdatdupe(sys->cfg,usernumber,offset,len,p,FALSE,match_next,NULL,NULL)));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}
#endif

static JSBool
js_trashcan(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		str;
	char*		can = NULL;
	JSString*	js_str;
	JSString*	js_can;
	jsrefcount	rc;
	BOOL		ret;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;

	if((js_can=JS_ValueToString(cx, argv[0]))==NULL) {
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(JS_FALSE));
		return(JS_TRUE);
	}

	if((js_str=JS_ValueToString(cx, argv[1]))==NULL) {
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(JS_FALSE));
		return(JS_TRUE);
	}

	JSSTRING_TO_MSTRING(cx, js_can, can, NULL);
	HANDLE_PENDING(cx, can);
	if(can==NULL) {
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(JS_FALSE));
		return(JS_TRUE);
	}

	JSSTRING_TO_MSTRING(cx, js_str, str, NULL);
	if(JS_IsExceptionPending(cx)) {
		FREE_AND_NULL(str);
		free(can);
		return JS_FALSE;
	}
	if(str==NULL) {
		free(can);
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(JS_FALSE));
		return(JS_TRUE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	ret=trashcan(sys->cfg,str,can);
	free(can);
	free(str);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return(JS_TRUE);
}

static JSBool
js_findstr(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char*		str;
	char*		fname = NULL;
	JSString*	js_str;
	JSString*	js_fname;
	jsrefcount	rc;
	BOOL		ret;

	if((js_fname=JS_ValueToString(cx, argv[0]))==NULL) {
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(JS_FALSE));
		return(JS_TRUE);
	}

	if((js_str=JS_ValueToString(cx, argv[1]))==NULL) {
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(JS_FALSE));
		return(JS_TRUE);
	}

	JSSTRING_TO_MSTRING(cx, js_fname, fname, NULL);
	HANDLE_PENDING(cx, fname);
	if(fname==NULL) {
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(JS_FALSE));
		return(JS_TRUE);
	}

	JSSTRING_TO_MSTRING(cx, js_str, str, NULL);
	if(JS_IsExceptionPending(cx)) {
		FREE_AND_NULL(str);
		free(fname);
		return JS_FALSE;
	}
	if(str==NULL) {
		free(fname);
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(JS_FALSE));
		return(JS_TRUE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	ret = findstr(str,fname);
	free(str);
	free(fname);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return(JS_TRUE);
}

static JSBool
js_zonestr(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	JSString*	js_str;
	short		zone;
	int32		val=0;
	jsrefcount	rc;
	char*		cstr;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;

	if(argc<1)
		zone=sys_timezone(sys->cfg);
	else {
		JS_ValueToInt32(cx,argv[0],&val);
		zone=(short)val;
	}

	rc=JS_SUSPENDREQUEST(cx);
	cstr=smb_zonestr(zone,NULL);
	JS_RESUMEREQUEST(cx, rc);
	if((js_str = JS_NewStringCopyZ(cx, cstr))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return(JS_TRUE);
}

/* Returns a ctime()-like string in the system-preferred time format */
static JSBool
js_timestr(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char		str[128];
	jsdouble	ti;
	JSString*	js_str;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;

	if(argc<1)
		ti=(jsdouble)time(NULL);	/* use current time */
	else
		JS_ValueToNumber(cx,argv[0],&ti);
	rc=JS_SUSPENDREQUEST(cx);
	timestr(sys->cfg,(time32_t)ti,str);
	JS_RESUMEREQUEST(cx, rc);
	if((js_str = JS_NewStringCopyZ(cx, str))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return(JS_TRUE);
}

/* Returns a mm/dd/yy or dd/mm/yy formated string */
static JSBool
js_datestr(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char		str[128];
	time32_t	t;
	JSString*	js_str;
	char		*p;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;

	if(argc<1)
		t=time32(NULL);	/* use current time */
	else {
		if(JSVAL_IS_STRING(argv[0])) {	/* convert from string to time_t? */
			JSVALUE_TO_ASTRING(cx, argv[0], p, 10, NULL);
			JS_SET_RVAL(cx, arglist, DOUBLE_TO_JSVAL((double)dstrtounix(sys->cfg, p)));
			return(JS_TRUE);
		}
		JS_ValueToInt32(cx,argv[0],&t);
	}
	unixtodstr(sys->cfg,t,str);
	if((js_str = JS_NewStringCopyZ(cx, str))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return(JS_TRUE);
}

static JSBool
js_secondstr(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char		str[128];
	int32		t=0;
	JSString*	js_str;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if(argc<1)
		return(JS_TRUE);

	JS_ValueToInt32(cx,argv[0],&t);
	sectostr(t,str);
	if((js_str = JS_NewStringCopyZ(cx, str))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return(JS_TRUE);
}

#ifndef JSDOOR
static JSBool
js_spamlog(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	uintN		i;
	char*		p=NULL;
	char*		prot=NULL;
	char*		action=NULL;
	char*		reason=NULL;
	char*		host=NULL;
	char*		ip_addr=NULL;
	char*		to=NULL;
	char*		from=NULL;
	jsrefcount	rc;
	BOOL		ret;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;

	for(i=0;i<argc;i++) {
		if(!JSVAL_IS_STRING(argv[i]))
			continue;
		if(from==NULL) {
			JSVALUE_TO_MSTRING(cx, argv[i], p, NULL);
			if(JS_IsExceptionPending(cx)) {
				if(prot)
					free(prot);
				if(action)
					free(action);
				if(reason)
					free(reason);
				if(host)
					free(host);
				if(ip_addr)
					free(ip_addr);
				if(to)
					free(to);
				if(from)
					free(from);
				if(p)
					free(p);
				return JS_FALSE;
			}
			if(p==NULL)
				continue;
			if(prot==NULL)
				prot=p;
			else if(action==NULL)
				action=p;
			else if(reason==NULL)
				reason=p;
			else if(host==NULL)
				host=p;
			else if(ip_addr==NULL)
				ip_addr=p;
			else if(to==NULL)
				to=p;
			else if(from==NULL)
				from=p;
			else
				free(p);
		}
	}
	rc=JS_SUSPENDREQUEST(cx);
	ret=spamlog(sys->cfg,prot,action,reason,host,ip_addr,to,from);
	if(prot)
		free(prot);
	if(action)
		free(action);
	if(reason)
		free(reason);
	if(host)
		free(host);
	if(ip_addr)
		free(ip_addr);
	if(to)
		free(to);
	if(from)
		free(from);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return(JS_TRUE);
}

static JSBool
js_hacklog(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	uintN		i;
	int32		i32=0;
	char*		p=NULL;
	char*		prot=NULL;
	char*		user=NULL;
	char*		text=NULL;
	char*		host=NULL;
	union xp_sockaddr	addr;
	jsrefcount	rc;
	BOOL		ret;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;

	memset(&addr,0,sizeof(addr));
	for(i=0;i<argc;i++) {
		if(JSVAL_IS_NUMBER(argv[i])) {
			JS_ValueToInt32(cx,argv[i],&i32);
			if(addr.in.sin_addr.s_addr==0)
				addr.in.sin_addr.s_addr=i32;
			else
				addr.in.sin_port=(ushort)i32;
			continue;
		}
		if(!JSVAL_IS_STRING(argv[i]))
			continue;
		if(host==NULL) {
			JSVALUE_TO_MSTRING(cx, argv[i], p, NULL);
			if(JS_IsExceptionPending(cx)) {
				if(prot)
					free(prot);
				if(user)
					free(user);
				if(text)
					free(text);
				if(host)
					free(host);
				if(p)
					free(p);
				return JS_FALSE;
			}
		}
		if(p==NULL)
			continue;
		if(prot==NULL)
			prot=p;
		else if(user==NULL)
			user=p;
		else if(text==NULL)
			text=p;
		else if(host==NULL)
			host=p;
	}
	rc=JS_SUSPENDREQUEST(cx);
	ret=hacklog(sys->cfg,prot,user,text,host,&addr);
	if(prot)
		free(prot);
	if(user)
		free(user);
	if(text)
		free(text);
	if(host)
		free(host);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return(JS_TRUE);
}

static JSBool
js_filter_ip(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	uintN		i;
	char*		p=NULL;
	char*		prot=NULL;
	char*		reason=NULL;
	char*		host=NULL;
	char*		ip_addr=NULL;
	char*		from=NULL;
	char*		fname=NULL;
	jsrefcount	rc;
	BOOL		ret;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;

	for(i=0;i<argc;i++) {
		if(!JSVAL_IS_STRING(argv[i]))
			continue;
		if(fname==NULL) {
			JSVALUE_TO_MSTRING(cx, argv[i], p, NULL);
			if(JS_IsExceptionPending(cx)) {
				if(prot)
					free(prot);
				if(reason)
					free(reason);
				if(host)
					free(host);
				if(ip_addr)
					free(ip_addr);
				if(from)
					free(from);
				if(fname)
					free(fname);
				if(p)
					free(p);
				return JS_FALSE;
			}
		}
		if(p==NULL)
			continue;
		if(prot==NULL)
			prot=p;
		else if(reason==NULL)
			reason=p;
		else if(host==NULL)
			host=p;
		else if(ip_addr==NULL)
			ip_addr=p;
		else if(from==NULL)
			from=p;
		else if(fname==NULL)
			fname=p;
	}
	rc=JS_SUSPENDREQUEST(cx);
	ret=filter_ip(sys->cfg,prot,reason,host,ip_addr,from,fname);
	if(prot)
		free(prot);
	if(reason)
		free(reason);
	if(host)
		free(host);
	if(ip_addr)
		free(ip_addr);
	if(from)
		free(from);
	if(fname)
		free(fname);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return(JS_TRUE);
}

static JSBool
js_get_node(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*	obj=JS_THIS_OBJECT(cx, arglist);
	JSObject*	nodeobj;
	jsval*		argv=JS_ARGV(cx, arglist);
	node_t		node = {0};
	int32		node_num;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;
	scfg_t* cfg = sys->cfg;

	node_num=cfg->node_num;
	if(argc) 
		JS_ValueToInt32(cx,argv[0],&node_num);
	if(node_num<1)
		node_num=1;

	rc=JS_SUSPENDREQUEST(cx);

	int retval = getnodedat(sys->cfg, node_num, &node, /* lockit: */FALSE, &sys->nodefile);
	sys->nodegets++;
	JS_RESUMEREQUEST(cx, rc);
	if(retval != 0) {
		JS_ReportError(cx, "getnodat(%d) returned %d", node_num, retval);
		return JS_TRUE;
	}
	if((nodeobj = JS_NewObject(cx, NULL, NULL, obj)) == NULL) {
		JS_ReportError(cx, "JS_NewObject failure");
		return JS_TRUE;
	}
	JS_DefineProperty(cx, nodeobj, "status", INT_TO_JSVAL((int)node.status), NULL, NULL, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, nodeobj, "errors", INT_TO_JSVAL((int)node.errors), NULL, NULL, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, nodeobj, "action", INT_TO_JSVAL((int)node.action), NULL, NULL, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, nodeobj, "useron", INT_TO_JSVAL((int)node.useron), NULL, NULL, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, nodeobj, "connection", INT_TO_JSVAL((int)node.connection), NULL, NULL, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, nodeobj, "misc", INT_TO_JSVAL((int)node.misc), NULL, NULL, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, nodeobj, "aux", INT_TO_JSVAL((int)node.aux), NULL, NULL, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, nodeobj, "extaux", INT_TO_JSVAL((int)node.extaux), NULL, NULL, JSPROP_ENUMERATE);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(nodeobj));
	return JS_TRUE;
}

static JSBool
js_get_node_message(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		buf;
	int32		node_num;
	JSString*	js_str;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;
	scfg_t* cfg = sys->cfg;

	node_num=cfg->node_num;
	if(argc) 
		JS_ValueToInt32(cx,argv[0],&node_num);
	if(node_num<1)
		node_num=1;

	rc=JS_SUSPENDREQUEST(cx);
	buf=getnmsg(sys->cfg,node_num);
	JS_RESUMEREQUEST(cx, rc);
	if(buf==NULL)
		return(JS_TRUE);

	js_str=JS_NewStringCopyZ(cx, buf);
	free(buf);

	if(js_str==NULL)
		return(JS_FALSE);
	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return(JS_TRUE);
}

static JSBool
js_put_node_message(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	int32		node=1;
	JSString*	js_msg;
	char*		msg = NULL;
	jsrefcount	rc;
	BOOL		ret;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;

	JS_ValueToInt32(cx,argv[0],&node);
	if(node<1)
		node=1;

	if((js_msg=JS_ValueToString(cx, argv[1]))==NULL) 
		return(JS_FALSE);

	JSSTRING_TO_MSTRING(cx, js_msg, msg, NULL);
	HANDLE_PENDING(cx, msg);
	if(msg==NULL) 
		return(JS_TRUE);

	rc=JS_SUSPENDREQUEST(cx);
	ret=putnmsg(sys->cfg,node,msg)==0;
	free(msg);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));

	return(JS_TRUE);
}

static JSBool
js_get_telegram(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		buf;
	int32		usernumber=1;
	JSString*	js_str;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;

	JS_ValueToInt32(cx,argv[0],&usernumber);
	if(usernumber<1)
		usernumber=1;

	rc=JS_SUSPENDREQUEST(cx);
	buf=getsmsg(sys->cfg,usernumber);
	JS_RESUMEREQUEST(cx, rc);
	if(buf==NULL)
		return(JS_TRUE);

	js_str=JS_NewStringCopyZ(cx, buf);
	free(buf);

	if(js_str==NULL)
		return(JS_FALSE);
	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return(JS_TRUE);
}

static JSBool
js_put_telegram(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	int32		usernumber=1;
	JSString*	js_msg;
	char*		msg = NULL;
	jsrefcount	rc;
	BOOL		ret;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;

	JS_ValueToInt32(cx,argv[0],&usernumber);
	if(usernumber<1)
		usernumber=1;

	if((js_msg=JS_ValueToString(cx, argv[1]))==NULL) 
		return(JS_FALSE);

	JSSTRING_TO_MSTRING(cx, js_msg, msg, NULL);
	HANDLE_PENDING(cx, msg);
	if(msg==NULL)
		return(JS_TRUE);

	rc=JS_SUSPENDREQUEST(cx);
	ret=putsmsg(sys->cfg,usernumber,msg)==0;
	free(msg);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));

	return(JS_TRUE);
}

static JSBool
js_new_user(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		alias;
	int			i;
	uintN		n;
	user_t		user;
	JSObject*	userobj;
	JSObject*	objarg;
	JSClass*	cl;
	jsrefcount	rc;
	client_t*	client=NULL;
	jsval		val;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;
	scfg_t* cfg = sys->cfg;

	if(argc<1 || JSVAL_NULL_OR_VOID(argv[0])) {
		JS_ReportError(cx,"Missing or invalid argument");
		return JS_FALSE;
	}

	JSVALUE_TO_ASTRING(cx, argv[0], alias, LEN_ALIAS+2, NULL);

	rc=JS_SUSPENDREQUEST(cx);
	if(!check_name(cfg,alias)) {
		JS_RESUMEREQUEST(cx, rc);
		JS_ReportError(cx,"Invalid or duplicate user alias: %s", alias);
		return JS_FALSE;
	}

	memset(&user,0,sizeof(user));
	for(n=0;n<argc;n++) {
		if(JSVAL_IS_OBJECT(argv[n])) {
			objarg = JSVAL_TO_OBJECT(argv[n]);
			if((cl=JS_GetClass(cx,objarg))!=NULL && strcmp(cl->name,"Client")==0) {
				client=JS_GetPrivate(cx,objarg);
				continue;
			}
		}
	}
	// Find and use the global client object if possible...
	if(client==NULL) {
		if(JS_GetProperty(cx, JS_GetGlobalObject(cx), "client", &val) && !JSVAL_NULL_OR_VOID(val)) {
			objarg = JSVAL_TO_OBJECT(val);
			if((cl=JS_GetClass(cx,objarg))!=NULL && strcmp(cl->name,"Client")==0)
				client=JS_GetPrivate(cx,objarg);
		}
	}
	if(client!=NULL) {
		SAFECOPY(user.modem,client->protocol);
		SAFECOPY(user.comp,client->host);
		SAFECOPY(user.ipaddr,client->addr);
	}

	user.sex=' ';
	SAFECOPY(user.alias,alias);

	/* statistics */
	user.firston=user.laston=user.pwmod=time32(NULL);

	/* security */
	user.level=cfg->new_level;
	user.flags1=cfg->new_flags1;
	user.flags2=cfg->new_flags2;
	user.flags3=cfg->new_flags3;
	user.flags4=cfg->new_flags4;
	user.rest=cfg->new_rest;
	user.exempt=cfg->new_exempt;

	user.cdt=cfg->new_cdt;
	user.min=cfg->new_min;
	user.freecdt=cfg->level_freecdtperday[user.level];
	if(cfg->new_expire)
		user.expire=user.firston+((long)cfg->new_expire*24L*60L*60L);
	else
		user.expire=0;

	/* settings */
	if(cfg->total_fcomps)
		strcpy(user.tmpext,cfg->fcomp[0]->ext);
	else
		strcpy(user.tmpext,"ZIP");

	user.shell=cfg->new_shell;
	user.misc=cfg->new_misc|(AUTOTERM|COLOR);
	user.prot=cfg->new_prot;
	user.qwk=QWK_DEFAULT;

	for(i=0;i<cfg->total_xedits;i++)
		if(!stricmp(cfg->xedit[i]->code,cfg->new_xedit) && chk_ar(cfg,cfg->xedit[i]->ar,&user,/* client: */NULL))
			break;
	if(i<cfg->total_xedits)
		user.xedit=i+1;

	i=newuserdat(cfg,&user);
	JS_RESUMEREQUEST(cx, rc);

	if(i==0) {
		userobj=js_CreateUserObject(cx, obj, cfg, NULL, &user, /* client: */NULL, /* global_user: */FALSE);
		JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(userobj));
	} else
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(i));

	return(JS_TRUE);
}

static JSBool
js_del_user(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	jsrefcount	rc;
	int32		n;
	user_t		user;
	char		str[128];

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;

	if(!JS_ValueToInt32(cx,argv[0],&n))
		return(JS_FALSE);
	user.number=n;
	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);	/* fail, by default */
	if(getuserdat(sys->cfg, &user)==0
		&& putuserrec(sys->cfg,n,U_MISC,8,ultoa(user.misc|DELETED,str,16))==0
		&& putusername(sys->cfg,n,nulstr)==0)
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);	/* success */
	JS_RESUMEREQUEST(cx, rc);
	
	return(JS_TRUE);
}
#endif

static JSBool
js_sys_exec(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	jsrefcount	rc;
	char	*cmd = NULL;
	int		ret;

	JSVALUE_TO_MSTRING(cx, argv[0], cmd, NULL);
	HANDLE_PENDING(cx, cmd);
	if(cmd==NULL) {
		JS_ReportError(cx, "Illegal NULL command");
		return JS_FALSE;
	}
	if(*cmd == 0) {
		free(cmd);
		JS_ReportError(cx, "Missing or invalid argument");
		return JS_FALSE;
	}
	rc=JS_SUSPENDREQUEST(cx);
	ret=system(cmd);
	free(cmd);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(ret));
	
	return(JS_TRUE);
}

static JSBool
js_popen(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char		str[1024];
	char*		cmd = NULL;
	FILE*		fp;
	jsint		line=0;
	jsval		val;
	JSObject*	array;
	JSString*	js_str;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc<1)
		return(JS_TRUE);

	if((array=JS_NewArrayObject(cx,0,NULL))==NULL)
		return(JS_FALSE);

	JSVALUE_TO_MSTRING(cx, argv[0], cmd, NULL);
	HANDLE_PENDING(cx, cmd);
	if(cmd==NULL) {
		JS_ReportError(cx, "Illegal NULL command");
		return JS_FALSE;
	}
	rc=JS_SUSPENDREQUEST(cx);
	if((fp=popen(cmd,"r"))==NULL) {
		free(cmd);
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}
	free(cmd);
	while(!feof(fp)) {
		if(fgets(str,sizeof(str),fp)==NULL)
			break;
		JS_RESUMEREQUEST(cx, rc);
		if((js_str=JS_NewStringCopyZ(cx,str))==NULL) {
			rc=JS_SUSPENDREQUEST(cx);
			break;
		}
		val=STRING_TO_JSVAL(js_str);
        if(!JS_SetElement(cx, array, line++, &val)) {
			rc=JS_SUSPENDREQUEST(cx);
			break;
		}
		rc=JS_SUSPENDREQUEST(cx);
	}
	pclose(fp);
	JS_RESUMEREQUEST(cx, rc);

    JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array));

    return(JS_TRUE);
}

#ifndef JSDOOR
static JSBool
js_chksyspass(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char		*pass;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;

	JSVALUE_TO_ASTRING(cx, argv[0], pass, LEN_PASS+2, NULL); // +2 is so overly long passwords fail.
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(stricmp(pass,sys->cfg->sys_pass)==0));

	return(JS_TRUE);
}

static JSBool 
js_chkname(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*	str;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	JSVALUE_TO_ASTRING(cx, argv[0], str, (LEN_ALIAS > LEN_NAME)?LEN_ALIAS+2:LEN_NAME+2, NULL);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
		return JS_FALSE;

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(check_name(sys->cfg,str)));
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}
#endif

static JSBool 
js_chkpid(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	int32		pid=0;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if(argc<1)
		return(JS_TRUE);

	JS_ValueToInt32(cx,argv[0],&pid);

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(check_pid(pid)));
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool 
js_killpid(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	int32		pid=0;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if(argc<1)
		return(JS_TRUE);

	JS_ValueToInt32(cx,argv[0],&pid);

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(terminate_pid(pid)));
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}


static jsSyncMethodSpec js_system_functions[] = {
#ifndef JSDOOR
	{"username",		js_username,		1,	JSTYPE_STRING,	JSDOCSTR("number")
	,JSDOCSTR("returns name of user in specified user record <i>number</i>, or empty string if not found")
	,311
	},
	{"alias",			js_alias,			1,	JSTYPE_STRING,	JSDOCSTR("alias")
	,JSDOCSTR("returns name of user that matches alias (if found in <tt>ctrl/alias.cfg</tt>)")
	,310
	},		
	{"matchuser",		js_matchuser,		1,	JSTYPE_NUMBER,	JSDOCSTR("username [,sysop_alias=<tt>true</tt>]")
	,JSDOCSTR("exact user name matching, returns number of user whose name/alias matches <i>username</i> "
		" or 0 if not found, matches well-known sysop aliases by default")
	,310
	},		
	{"matchuserdata",	js_matchuserdata,	2,	JSTYPE_NUMBER,	JSDOCSTR("field, data [,usernumber, match_next=<tt>false</tt>]")
	,JSDOCSTR("search user database for data in a specific field (see <tt>U_*</tt> in <tt>sbbsdefs.js</tt>), "
		"returns first matching user record number, optional <i>usernumber</i> specifies user record to skip, "
		"or record at which to begin searching if optional <i>match_next</i> is <tt>true</tt>")
	,310
	},
#endif
	{"trashcan",		js_trashcan,		2,	JSTYPE_BOOLEAN,	JSDOCSTR("basename, find_string")
	,JSDOCSTR("search <tt>text/<i>basename</i>.can</tt> for pseudo-regexp")
	,310
	},		
	{"findstr",			js_findstr,			2,	JSTYPE_BOOLEAN,	JSDOCSTR("path/filename, find_string")
	,JSDOCSTR("search any file for pseudo-regexp")
	,310
	},		
	{"zonestr",			js_zonestr,			0,	JSTYPE_STRING,	JSDOCSTR("[timezone=<i>local</i>]")
	,JSDOCSTR("convert time zone integer to string, defaults to system timezone if <i>timezone</i> not specified")
	,310
	},		
	{"timestr",			js_timestr,			0,	JSTYPE_STRING,	JSDOCSTR("[time=<i>current</i>]")
	,JSDOCSTR("convert time_t integer into a time string, "
		"defaults to current time if <i>time</i> not specified")
	,310
	},		
	{"datestr",			js_datestr,			0,	JSTYPE_STRING,	JSDOCSTR("[time=<i>current</i>]")
	,JSDOCSTR("convert time_t integer into a date string (in either <tt>MM/DD/YY</tt> or <tt>DD/MM/YY</tt> format), "
		"defaults to current date if <i>time</i> not specified. "
		"If <i>time</i> is a string in the appropriate format, returns the time_t.")
	,310
	},		
	{"secondstr",		js_secondstr,		0,	JSTYPE_STRING,	JSDOCSTR("seconds")
	,JSDOCSTR("convert elapsed time in seconds into a string in <tt>hh:mm:ss</tt> format")
	,310
	},		
#ifndef JSDOOR
	{"spamlog",			js_spamlog,			6,	JSTYPE_BOOLEAN,	JSDOCSTR("[protocol, action, reason, host, ip, to, from]")
	,JSDOCSTR("log a suspected SPAM attempt")
	,310
	},		
	{"hacklog",			js_hacklog,			5,	JSTYPE_BOOLEAN,	JSDOCSTR("[protocol, user, text, host, ip, port]")
	,JSDOCSTR("log a suspected hack attempt")
	,310
	},
	{"filter_ip",		js_filter_ip,		4,	JSTYPE_BOOLEAN,	JSDOCSTR("[protocol, reason, host, ip, username, filename]")
	,JSDOCSTR("add an IP address (with comment) to an IP filter file. If filename is not specified, the ip.can file is used")
	,311
	},
	{"get_node",		js_get_node,		1,	JSTYPE_OBJECT,	JSDOCSTR("node_number")
	,JSDOCSTR("read a node data record all at once (and leaving the record unlocked) "
		"returning an object matching the elements of <tt>system.node_list</tt>")
	,31702
	},
	{"get_node_message",js_get_node_message,0,	JSTYPE_STRING,	JSDOCSTR("node_number")
	,JSDOCSTR("read any messages waiting for the specified node and return in a single string")
	,311
	},		
	{"put_node_message",js_put_node_message,2,	JSTYPE_BOOLEAN,	JSDOCSTR("node_number, message_text")
	,JSDOCSTR("send a node a short text message, delivered immediately")
	,310
	},		
	{"get_telegram",	js_get_telegram,	1,	JSTYPE_STRING,	JSDOCSTR("user_number")
	,JSDOCSTR("returns any short text messages waiting for the specified user")
	,311
	},		
	{"put_telegram",	js_put_telegram,	2,	JSTYPE_BOOLEAN,	JSDOCSTR("user_number, message_text")
	,JSDOCSTR("sends a user a short text message, delivered immediately or during next logon")
	,310
	},		
	{"newuser",			js_new_user,		1,	JSTYPE_ALIAS },
	{"new_user",		js_new_user,		1,	JSTYPE_OBJECT,	JSDOCSTR("name/alias [,client object]")
	,JSDOCSTR("creates a new user record, returns a new <a href=#User>User</a> object representing the new user account, on success.<br>"
	"returns an numeric error code on failure (optional <i>client</i> object argument added in v3.15a.  As of 3.16c, the global "
	"client object is used if the argument is omitted)")
	,310
	},
	{"del_user",		js_del_user,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("number")
	,JSDOCSTR("delete the specified user account")
	,316
	},
#endif
	{"exec",			js_sys_exec,		0,	JSTYPE_NUMBER,	JSDOCSTR("command-line")
	,JSDOCSTR("executes a native system/shell command-line, returns <i>0</i> on success")
	,311
	},
	{"popen",			js_popen,			0,	JSTYPE_ARRAY,	JSDOCSTR("command-line")
	,JSDOCSTR("executes a native system/shell command-line, returns array of captured output lines on success "
		"(<b>only functional on UNIX systems</b>)")
	,311
	},
#ifndef JSDOOR
	{"check_syspass",	js_chksyspass,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("password")
	,JSDOCSTR("compares the supplied <i>password</i> against the system password and returns <i>true</i> if it matches")
	,311
	},
	{"check_name",		js_chkname,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("name/alias")
	,JSDOCSTR("checks that the provided name/alias string is suitable for a new user account, "
		"returns <i>true</i> if it is valid")
	,315
	},
#endif
	{"check_pid",		js_chkpid,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("process-ID")
	,JSDOCSTR("checks that the provided process ID is a valid executing process on the system, "
		"returns <i>true</i> if it is valid")
	,315
	},
	{"terminate_pid",	js_killpid,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("process-ID")
	,JSDOCSTR("terminates executing process on the system with the specified process ID, "
		"returns <i>true</i> on success")
	,315
	},
	{0}
};


/* node properties */
enum {
	/* raw node_t fields */
	 NODE_PROP_STATUS
	,NODE_PROP_ERRORS
	,NODE_PROP_ACTION
	,NODE_PROP_USERON
	,NODE_PROP_CONNECTION
	,NODE_PROP_MISC
	,NODE_PROP_AUX
	,NODE_PROP_EXTAUX
	,NODE_PROP_DIR
};

#ifdef BUILD_JSDOCS
static char* node_prop_desc[] = {
	 "status (see <tt>nodedefs.js</tt> for valid values)"
	,"error counter"
	,"current user action (see <tt>nodedefs.js</tt>)"
	,"current user number"
	,"connection speed (<tt>0xffff</tt> = Telnet or RLogin)"
	,"miscellaneous bitfield (see <tt>nodedefs.js</tt>)"
	,"auxillary value"
	,"extended auxillary value"
	,"node directory"
	,NULL
};
#endif


static JSBool js_node_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval idval;
	uint		node_num;
    jsint       tiny;
	node_t		node;
	JSObject*	sysobj;
	JSObject*	node_list;
	jsrefcount	rc;
	JSString*	js_str;

	JS_IdToValue(cx, id, &idval);
	tiny = JSVAL_TO_INT(idval);

	if((node_list=JS_GetParent(cx, obj))==NULL)
		return(JS_FALSE);

	if((sysobj=JS_GetParent(cx, node_list))==NULL)
		return(JS_FALSE);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,sysobj,&js_system_class))==NULL)
		return JS_FALSE;

	node_num=(uintptr_t)JS_GetPrivate(cx,obj)>>1;

	rc=JS_SUSPENDREQUEST(cx);
	memset(&node,0,sizeof(node));
	if(getnodedat(sys->cfg, node_num, &node, /* lockit: */FALSE, &sys->nodefile)) {
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}
	JS_RESUMEREQUEST(cx, rc);
	sys->nodegets++;
	
    switch(tiny) {
		case NODE_PROP_STATUS:
			*vp = INT_TO_JSVAL((int)node.status);
			break;
		case NODE_PROP_ERRORS:	
			*vp = INT_TO_JSVAL((int)node.errors);
			break;
		case NODE_PROP_ACTION:	
			*vp = INT_TO_JSVAL((int)node.action);
			break;
		case NODE_PROP_USERON:	
			*vp = INT_TO_JSVAL((int)node.useron);
			break;
		case NODE_PROP_CONNECTION:
			*vp = INT_TO_JSVAL((int)node.connection);
			break;
		case NODE_PROP_MISC:		
			*vp = INT_TO_JSVAL((int)node.misc);
			break;
		case NODE_PROP_AUX:		
			*vp = INT_TO_JSVAL((int)node.aux);
			break;
		case NODE_PROP_EXTAUX:	
			*vp=UINT_TO_JSVAL(node.extaux);
			break;
		case NODE_PROP_DIR:
			if((js_str=JS_NewStringCopyZ(cx, sys->cfg->node_path[node_num-1]))==NULL)
				return(JS_FALSE);
			*vp = STRING_TO_JSVAL(js_str);
			break;
	}
	return(JS_TRUE);
}

static JSBool js_node_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsval idval;
	uint		node_num;
	jsint		val=0;
    jsint       tiny;
	node_t		node;
	JSObject*	sysobj;
	JSObject*	node_list;
	jsrefcount	rc;

	if((node_list=JS_GetParent(cx, obj))==NULL)
		return(JS_FALSE);

	if((sysobj=JS_GetParent(cx, node_list))==NULL)
		return(JS_FALSE);

	js_system_private_t* sys;
	if((sys = (js_system_private_t*)js_GetClassPrivate(cx,sysobj,&js_system_class))==NULL)
		return JS_FALSE;

	node_num=(uintptr_t)JS_GetPrivate(cx,obj)>>1;

	rc=JS_SUSPENDREQUEST(cx);
	memset(&node,0,sizeof(node));
	if(getnodedat(sys->cfg, node_num, &node, /* lockit: */TRUE, &sys->nodefile)) {
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}

	JS_RESUMEREQUEST(cx, rc);
	if(JSVAL_IS_NUMBER(*vp))
		JS_ValueToInt32(cx, *vp, &val);

	JS_IdToValue(cx, id, &idval);
	tiny = JSVAL_TO_INT(idval);
	rc=JS_SUSPENDREQUEST(cx);
	
    switch(tiny) {
		case NODE_PROP_STATUS:
			node.status=(BYTE)val;
			break;
		case NODE_PROP_ERRORS:	
			node.errors=(BYTE)val;
			break;
		case NODE_PROP_ACTION:	
			node.action=(BYTE)val;
			break;
		case NODE_PROP_USERON:	
			node.useron=(WORD)val;
			break;
		case NODE_PROP_CONNECTION:
			node.connection=(WORD)val;
			break;
		case NODE_PROP_MISC:		
			node.misc=(WORD)val;
			break;
		case NODE_PROP_AUX:		
			node.aux=(WORD)val;
			break;
		case NODE_PROP_EXTAUX:	
			node.extaux=val;
			break;
	}
	putnodedat(sys->cfg,node_num,&node, /* closeit: */FALSE, sys->nodefile);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static jsSyncPropertySpec js_node_properties[] = {
/*		 name,						tinyid,					flags,				ver	*/

/* raw node_t fields */
	{	"status",					NODE_PROP_STATUS,		JSPROP_ENUMERATE,	310 },
	{	"errors",					NODE_PROP_ERRORS,		JSPROP_ENUMERATE,	310 },
	{	"action",					NODE_PROP_ACTION,		JSPROP_ENUMERATE,	310 },
	{	"useron",					NODE_PROP_USERON,		JSPROP_ENUMERATE,	310 },
	{	"connection",				NODE_PROP_CONNECTION,	JSPROP_ENUMERATE,	310 },
	{	"misc",						NODE_PROP_MISC,			JSPROP_ENUMERATE,	310 },
	{	"aux",						NODE_PROP_AUX,			JSPROP_ENUMERATE,	310 },
	{	"extaux",					NODE_PROP_EXTAUX,		JSPROP_ENUMERATE,	310 },
	{	"dir",						NODE_PROP_DIR,			JSPROP_ENUMERATE|JSPROP_READONLY,	315 },
	{0}
};

static JSBool js_node_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*			name=NULL;
	JSBool			ret;

	if(id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		
		JS_IdToValue(cx, id, &idval);
		if(JSVAL_IS_STRING(idval)) {
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
			HANDLE_PENDING(cx, name);
		}
	}

	ret=js_SyncResolve(cx, obj, name, js_node_properties, NULL, NULL, 0);
	if(name)
		free(name);
	return(ret);
}

static JSBool js_node_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_node_resolve(cx, obj, JSID_VOID));
}

static JSClass js_node_class = {
     "Node"					/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_node_get			/* getProperty	*/
	,js_node_set			/* setProperty	*/
	,js_node_enumerate		/* enumerate	*/
	,js_node_resolve		/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};

#define LAZY_INTEGER(PropName, PropValue) \
	if(name==NULL || strcmp(name, (PropName))==0) { \
		val=UINT_TO_JSVAL((PropValue)); \
		JS_DefineProperty(cx, obj, (PropName), val, NULL,NULL,JSPROP_ENUMERATE); \
		if(name) { \
			free(name); \
			return(JS_TRUE); \
		} \
	}

#define LAZY_STRING(PropName, PropValue) \
	if(name==NULL || strcmp(name, (PropName))==0) { \
		if((js_str=JS_NewStringCopyZ(cx, (PropValue)))!=NULL) { \
			JS_DefineProperty(cx, obj, PropName, STRING_TO_JSVAL(js_str), NULL, NULL, JSPROP_ENUMERATE); \
			if(name) { \
				free(name); \
				return(JS_TRUE); \
			} \
		} \
		else if(name) { \
			free(name); \
			return(JS_TRUE); \
		} \
	}

#define LAZY_STRFUNC(PropName, Function, PropValue) \
	if(name==NULL || strcmp(name, (PropName))==0) { \
		Function; \
		if((js_str=JS_NewStringCopyZ(cx, (PropValue)))!=NULL) { \
			JS_DefineProperty(cx, obj, PropName, STRING_TO_JSVAL(js_str), NULL, NULL, JSPROP_ENUMERATE); \
			if(name) { \
				free(name); \
				return(JS_TRUE); \
			} \
		} \
		else if(name) { \
			free(name); \
			return(JS_TRUE); \
		} \
	}

#define LAZY_STRFUNC_TRUNCSP(PropName, Function, PropValue) \
	if(name==NULL || strcmp(name, (PropName))==0) { \
		Function; \
		if((js_str=JS_NewStringCopyZ(cx, truncsp(PropValue)))!=NULL) { \
			JS_DefineProperty(cx, obj, PropName, STRING_TO_JSVAL(js_str), NULL, NULL, JSPROP_ENUMERATE); \
			if(name) { \
				free(name); \
				return(JS_TRUE); \
			} \
		} \
		else if(name) { \
			free(name); \
			return(JS_TRUE); \
		} \
	}

static JSBool js_system_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*		name=NULL;
	jsval		val;
	char		str[256];
	JSString*	js_str;
	JSBool		ret;
#ifndef JSDOOR
	JSObject*	newobj;
	JSObject*	nodeobj;
	uint		i;
#endif

	if(id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		
		JS_IdToValue(cx, id, &idval);
		if(JSVAL_IS_STRING(idval)) {
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
			HANDLE_PENDING(cx, name);
		}
	}

	/****************************/
	/* static string properties */
	LAZY_STRING("version", VERSION);
	LAZY_STRFUNC("revision", sprintf(str,"%c",REVISION), str);
	LAZY_STRFUNC_TRUNCSP("beta_version", SAFECOPY(str, beta_version), str);

	if(name==NULL || strcmp(name, "full_version")==0) {
		sprintf(str,"%s%c%s",VERSION,REVISION,beta_version);
		truncsp(str);
#if defined(_DEBUG)
		strcat(str," Debug");
#endif
		if(name) free(name);
		if((js_str=JS_NewStringCopyZ(cx, str))!=NULL) {
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, obj, "full_version", &val);
			if(name) return(JS_TRUE);
		}
		else if(name) return(JS_TRUE);
	}

	LAZY_STRING("version_notice", VERSION_NOTICE);

	/* Numeric version properties */
	LAZY_INTEGER("version_num", VERSION_NUM);
	LAZY_INTEGER("version_hex", VERSION_HEX);

	LAZY_STRING("platform", PLATFORM_DESC);
	LAZY_STRING("architecture", ARCHITECTURE_DESC);
	LAZY_STRFUNC("msgbase_lib", sprintf(str,"SMBLIB %s",smb_lib_ver()), str);
	LAZY_STRFUNC("compiled_with", DESCRIBE_COMPILER(str), str);
	LAZY_STRFUNC("compiled_when", sprintf(str,"%s %.5s",__DATE__,__TIME__), str);
	LAZY_STRING("copyright", COPYRIGHT_NOTICE);
	LAZY_STRING("js_version", (char *)JS_GetImplementationVersion());
	LAZY_STRING("os_version", os_version(str));

#ifndef JSDOOR
	/* fido_addr_list property */
	if(name==NULL || strcmp(name, "fido_addr_list")==0) {
		if(name) free(name);

		js_system_private_t* sys;
		if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
			return JS_FALSE;

		if((newobj=JS_NewArrayObject(cx, 0, NULL))==NULL)
			return(JS_FALSE);

		if(!JS_SetParent(cx, newobj, obj))
			return(JS_FALSE);

		if(!JS_DefineProperty(cx, obj, "fido_addr_list", OBJECT_TO_JSVAL(newobj)
			, NULL, NULL, JSPROP_ENUMERATE))
			return(JS_FALSE);

		for(i=0;i<sys->cfg->total_faddrs;i++) {
			val=STRING_TO_JSVAL(JS_NewStringCopyZ(cx,smb_faddrtoa(&sys->cfg->faddr[i],str)));
			JS_SetElement(cx, newobj, i, &val);
		}
		if(name) return(JS_TRUE);
	}

	if(name==NULL || strcmp(name, "stats")==0) {
		if(name) free(name);

		js_system_private_t* sys;
		if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
			return JS_FALSE;

		newobj = JS_DefineObject(cx, obj, "stats", &js_sysstats_class, NULL
			,JSPROP_ENUMERATE|JSPROP_READONLY);

		if(newobj==NULL)
			return(JS_FALSE);

		JS_SetPrivate(cx, newobj, sys);
#ifdef BUILD_JSDOCS
		js_DescribeSyncObject(cx,newobj,"System statistics",310);
		js_CreateArrayOfStrings(cx, newobj, "_property_desc_list", sysstat_prop_desc, JSPROP_READONLY);
#endif
		if(name) return JS_TRUE;
	}

	/* node_list property */
	if(name==NULL || strcmp(name, "node_list")==0) {
		if(name) free(name);

		js_system_private_t* sys;
		if((sys = (js_system_private_t*)js_GetClassPrivate(cx,obj,&js_system_class))==NULL)
			return JS_FALSE;

		if((newobj=JS_NewArrayObject(cx, 0, NULL))==NULL) 
			return(JS_FALSE);

		if(!JS_SetParent(cx, newobj, obj))
			return(JS_FALSE);

		if(!JS_DefineProperty(cx, obj, "node_list", OBJECT_TO_JSVAL(newobj)
			, NULL, NULL, JSPROP_ENUMERATE))
			return(JS_FALSE);

		for(i=0;i<sys->cfg->sys_nodes && i<sys->cfg->sys_lastnode;i++) {

			nodeobj = JS_NewObject(cx, &js_node_class, NULL, newobj);

			if(nodeobj==NULL)
				return(JS_FALSE);

			/* Store node number */
			/* We have to shift it to make it look like a pointer to JS. :-( */
			if(!JS_SetPrivate(cx, nodeobj, (char*)(((uintptr_t)i+1)<<1)))
				return(JS_FALSE);

	#ifdef BUILD_JSDOCS
			if(i==0) {
				js_DescribeSyncObject(cx,nodeobj,"BBS node listing",310);
				js_CreateArrayOfStrings(cx, nodeobj, "_property_desc_list", node_prop_desc, JSPROP_READONLY);
			}
	#endif

			val=OBJECT_TO_JSVAL(nodeobj);
			if(!JS_SetElement(cx, newobj, i, &val))
				return(JS_FALSE);
		}
		if(name) return(JS_TRUE);
	}
#endif

	ret = js_SyncResolve(cx, obj, name, js_system_properties, js_system_functions, NULL, 0);
	if(name) free(name);
	return ret;
}

static JSBool js_system_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_system_resolve(cx, obj, JSID_VOID));
}

static void js_system_finalize(JSContext *cx, JSObject *obj)
{
	js_system_private_t* sys;
	if((sys = (js_system_private_t*)JS_GetPrivate(cx, obj)) == NULL)
		return;

	CLOSE_OPEN_FILE(sys->nodefile);
	free(sys);
	JS_SetPrivate(cx, obj, NULL);
}

JSClass js_system_class = {
     "System"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_system_get			/* getProperty	*/
	,js_system_set			/* setProperty	*/
	,js_system_enumerate	/* enumerate	*/
	,js_system_resolve		/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_system_finalize		/* finalize		*/
};

JSObject* DLLCALL js_CreateSystemObject(JSContext* cx, JSObject* parent
										,scfg_t* cfg, time_t uptime, char* host_name, char* socklib_desc)
{
	jsval		val;
	JSObject*	sysobj;
	JSString*	js_str;
	char		str[256];

	sysobj = JS_DefineObject(cx, parent, "system", &js_system_class, NULL
		,JSPROP_ENUMERATE|JSPROP_READONLY);
	if(sysobj==NULL)
		return(NULL);

	js_system_private_t* sys;
	if((sys = calloc(sizeof(*sys), 1)) == NULL)
		return NULL;

	sys->cfg = cfg;
	sys->nodefile = -1;

	if(!JS_SetPrivate(cx, sysobj, sys))
		return(NULL);

	/****************************/
	/* static string properties */
#ifndef JSDOOR
	if((js_str=JS_NewStringCopyZ(cx, host_name))==NULL)
		return(NULL);
	val = STRING_TO_JSVAL(js_str);
	if(!JS_SetProperty(cx, sysobj, "host_name", &val))
		return(NULL);
#endif

	if((js_str=JS_NewStringCopyZ(cx, socklib_version(str, socklib_desc)))==NULL)
		return(NULL);
	val = STRING_TO_JSVAL(js_str);
	if(!JS_SetProperty(cx, sysobj, "socket_lib", &val))
		return(NULL);

	/***********************/

#ifndef JSDOOR
	val=DOUBLE_TO_JSVAL((double)uptime);
	if(!JS_SetProperty(cx, sysobj, "uptime", &val))
		return(NULL);
#endif

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,sysobj,"Global system-related properties and methods",310);
	js_CreateArrayOfStrings(cx, sysobj, "_property_desc_list", sys_prop_desc, JSPROP_READONLY);
#endif

	return(sysobj);
}

#endif	/* JAVSCRIPT */
