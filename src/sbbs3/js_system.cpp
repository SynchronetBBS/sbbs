/* Synchronet JavaScript "system" Object */

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

#include "sbbs.h"

#include "filedat.h"
#include "js_request.h"
#include "os_info.h"
#include "ver.h"

#ifdef JAVASCRIPT

typedef struct {
	scfg_t* cfg;
	struct mqtt* mqtt;
	int nodefile;
	int nodegets;
	stats_t stats;
} js_system_private_t;

extern JSClass js_system_class;

/* System Object Properties */
enum {
	SYS_PROP_NAME
	, SYS_PROP_OP
	, SYS_PROP_OP_AVAIL
	, SYS_PROP_GURU
	, SYS_PROP_ID
	, SYS_PROP_MISC
	, SYS_PROP_LOGIN
	, SYS_PROP_INETADDR
	, SYS_PROP_LOCATION
	, SYS_PROP_TIMEZONE
	, SYS_PROP_TZ_OFFSET
	, SYS_PROP_DATE_FMT
	, SYS_PROP_DATE_SEP
	, SYS_PROP_DATE_VERBAL
	, SYS_PROP_BIRTHDATE_FMT
	, SYS_PROP_BIRTHDATE_TEMPLATE
	, SYS_PROP_PHONENUM_TEMPLATE
	, SYS_PROP_PWDAYS
	, SYS_PROP_MINPWLEN
	, SYS_PROP_MAXPWLEN
	, SYS_PROP_DELDAYS
	, SYS_PROP_AUTODEL

	, SYS_PROP_LASTUSER
	, SYS_PROP_LASTUSERON
	, SYS_PROP_FREEDISKSPACE
	, SYS_PROP_FREEDISKSPACEK

	, SYS_PROP_NODES
	, SYS_PROP_LASTNODE

	, SYS_PROP_MQTT_ENABLED

	, SYS_PROP_NEW_PASS
	, SYS_PROP_NEW_MAGIC
	, SYS_PROP_NEW_LEVEL
	, SYS_PROP_NEW_FLAGS1
	, SYS_PROP_NEW_FLAGS2
	, SYS_PROP_NEW_FLAGS3
	, SYS_PROP_NEW_FLAGS4
	, SYS_PROP_NEW_REST
	, SYS_PROP_NEW_EXEMPT
	, SYS_PROP_NEW_CDT
	, SYS_PROP_NEW_MIN
	, SYS_PROP_NEW_SHELL
	, SYS_PROP_NEW_XEDIT
	, SYS_PROP_NEW_MISC
	, SYS_PROP_NEW_PROT
	, SYS_PROP_NEW_EXPIRE
	, SYS_PROP_NEW_UQ

	, SYS_PROP_EXPIRED_LEVEL
	, SYS_PROP_EXPIRED_FLAGS1
	, SYS_PROP_EXPIRED_FLAGS2
	, SYS_PROP_EXPIRED_FLAGS3
	, SYS_PROP_EXPIRED_FLAGS4
	, SYS_PROP_EXPIRED_REST
	, SYS_PROP_EXPIRED_EXEMPT

	/* directories */
	, SYS_PROP_NODE_DIR
	, SYS_PROP_CTRL_DIR
	, SYS_PROP_DATA_DIR
	, SYS_PROP_TEXT_DIR
	, SYS_PROP_TEMP_DIR
	, SYS_PROP_EXEC_DIR
	, SYS_PROP_MODS_DIR
	, SYS_PROP_LOGS_DIR

	/* clock/timer access */
	, SYS_PROP_CLOCK
	, SYS_PROP_CLOCK_PER_SEC
	, SYS_PROP_TIMER
	, SYS_PROP_STATS_INTERVAL

	/* filenames */
	, SYS_PROP_DEVNULL
	, SYS_PROP_TEMP_PATH
	, SYS_PROP_CMD_SHELL

	, SYS_PROP_LOCAL_HOSTNAME
	/* last */
	, SYS_PROP_NAME_SERVERS
};

static JSBool js_system_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval                idval;
	char                 str[128];
	const char*          p = NULL;
	jsint                tiny;
	JSString*            js_str;
	uint64_t             space;
	jsrefcount           rc;
	JSObject *           robj;
	jsval                jval;
	str_list_t           list;
	int                  i;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;
	scfg_t*              cfg = sys->cfg;

	JS_IdToValue(cx, id, &idval);
	tiny = JSVAL_TO_INT(idval);

	switch (tiny) {
#ifndef JSDOOR
		case SYS_PROP_NAME:
			p = cfg->sys_name;
			break;
		case SYS_PROP_OP:
			p = cfg->sys_op;
			break;
		case SYS_PROP_OP_AVAIL:
			*vp = BOOLEAN_TO_JSVAL(sysop_available(cfg));
			break;
		case SYS_PROP_GURU:
			p = cfg->sys_guru;
			break;
		case SYS_PROP_ID:
			p = cfg->sys_id;
			break;
		case SYS_PROP_MISC:
			*vp = UINT_TO_JSVAL(cfg->sys_misc);
			break;
		case SYS_PROP_LOGIN:
			*vp = UINT_TO_JSVAL(cfg->sys_login);
			break;
		case SYS_PROP_INETADDR:
			p = cfg->sys_inetaddr;
			break;
		case SYS_PROP_LOCATION:
			p = cfg->sys_location;
			break;
		case SYS_PROP_TIMEZONE:
			*vp = INT_TO_JSVAL(sys_timezone(cfg));
			break;
		case SYS_PROP_TZ_OFFSET:
			*vp = INT_TO_JSVAL(smb_tzutc(sys_timezone(cfg)));
			break;
		case SYS_PROP_DATE_FMT:
			*vp = INT_TO_JSVAL(cfg->sys_date_fmt);
			break;
		case SYS_PROP_DATE_SEP:
			snprintf(str, sizeof str, "%c", cfg->sys_date_sep);
			p = str;
			break;
		case SYS_PROP_DATE_VERBAL:
			*vp = BOOLEAN_TO_JSVAL(cfg->sys_date_verbal);
			break;
		case SYS_PROP_BIRTHDATE_FMT:
			birthdate_format(cfg, str, sizeof str);
			p = str;
			break;
		case SYS_PROP_BIRTHDATE_TEMPLATE:
			birthdate_template(cfg, str, sizeof str);
			p = str;
			break;
		case SYS_PROP_PHONENUM_TEMPLATE:
			p = cfg->sys_phonefmt;
			break;
		case SYS_PROP_NODES:
			*vp = INT_TO_JSVAL(cfg->sys_nodes);
			break;
		case SYS_PROP_LASTNODE:
			*vp = INT_TO_JSVAL(cfg->sys_lastnode);
			break;
		case SYS_PROP_MQTT_ENABLED:
			*vp = BOOLEAN_TO_JSVAL(cfg->mqtt.enabled);
			break;
		case SYS_PROP_PWDAYS:
			*vp = INT_TO_JSVAL(cfg->sys_pwdays);
			break;
		case SYS_PROP_MINPWLEN:
			*vp = INT_TO_JSVAL(cfg->min_pwlen);
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
			p = lastuseron;
			break;
#endif
		case SYS_PROP_FREEDISKSPACE:
		case SYS_PROP_FREEDISKSPACEK:
			rc = JS_SUSPENDREQUEST(cx);
			if (tiny == SYS_PROP_FREEDISKSPACE)
				space = getfreediskspace(cfg->temp_dir, 0);
			else
				space = getfreediskspace(cfg->temp_dir, 1024);
			JS_RESUMEREQUEST(cx, rc);
			*vp = DOUBLE_TO_JSVAL((double)space);
			break;
#ifndef JSDOOR
		case SYS_PROP_NEW_PASS:
			p = cfg->new_pass;
			break;
		case SYS_PROP_NEW_MAGIC:
			p = cfg->new_magic;
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
			*vp = UINT_TO_JSVAL(cfg->new_cdt);
			break;
		case SYS_PROP_NEW_MIN:
			*vp = UINT_TO_JSVAL(cfg->new_min);
			break;
		case SYS_PROP_NEW_SHELL:
			if (cfg->new_shell < cfg->total_shells)
				p = cfg->shell[cfg->new_shell]->code;
			break;
		case SYS_PROP_NEW_XEDIT:
			p = cfg->new_xedit;
			break;
		case SYS_PROP_NEW_MISC:
			*vp = UINT_TO_JSVAL(cfg->new_misc);
			break;
		case SYS_PROP_NEW_PROT:
			sprintf(str, "%c", cfg->new_prot);
			p = str;
			break;
		case SYS_PROP_NEW_EXPIRE:
			*vp = UINT_TO_JSVAL(cfg->new_expire);
			break;
		case SYS_PROP_NEW_UQ:
			*vp = UINT_TO_JSVAL(cfg->uq);
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
			p = cfg->node_dir;
			break;
#endif
		case SYS_PROP_CTRL_DIR:
			p = cfg->ctrl_dir;
			break;
		case SYS_PROP_DATA_DIR:
			p = cfg->data_dir;
			break;
		case SYS_PROP_TEXT_DIR:
			p = cfg->text_dir;
			break;
		case SYS_PROP_TEMP_DIR:
			p = cfg->temp_dir;
			break;
		case SYS_PROP_EXEC_DIR:
			p = cfg->exec_dir;
			break;
		case SYS_PROP_MODS_DIR:
			p = cfg->mods_dir;
			break;
		case SYS_PROP_LOGS_DIR:
			p = cfg->logs_dir;
			break;

		case SYS_PROP_DEVNULL:
			p = _PATH_DEVNULL;
			break;
		case SYS_PROP_TEMP_PATH:
			p = _PATH_TMP;
			break;

		case SYS_PROP_CMD_SHELL:
			rc = JS_SUSPENDREQUEST(cx);
			p = os_cmdshell();
			JS_RESUMEREQUEST(cx, rc);
			break;

		case SYS_PROP_CLOCK:
			*vp = DOUBLE_TO_JSVAL((double)xp_timer64());
			break;
		case SYS_PROP_CLOCK_PER_SEC:
			*vp = UINT_TO_JSVAL(1000);
			break;
		case SYS_PROP_TIMER:
			*vp = DOUBLE_TO_JSVAL(xp_timer());
			break;
		case SYS_PROP_STATS_INTERVAL:
			*vp = UINT_TO_JSVAL(cfg->stats_interval);
			break;

		case SYS_PROP_LOCAL_HOSTNAME:
			rc = JS_SUSPENDREQUEST(cx);
			gethostname(str, sizeof(str));
			JS_RESUMEREQUEST(cx, rc);
			p = str;
			break;
		case SYS_PROP_NAME_SERVERS:
			rc = JS_SUSPENDREQUEST(cx);
			robj = JS_NewArrayObject(cx, 0, NULL);
			if (robj == NULL)
				return JS_FALSE;
			*vp = OBJECT_TO_JSVAL(robj);
			list = getNameServerList();
			if (list != NULL) {
				for (i = 0; list[i]; i++) {
					jval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, list[i]));
					if (!JS_SetElement(cx, robj, i, &jval))
						break;
				}
			}
			freeNameServerList(list);
			JS_RESUMEREQUEST(cx, rc);
			break;
	}

	if (p != NULL) {   /* string property */
		if ((js_str = JS_NewStringCopyZ(cx, p)) == NULL)
			return JS_FALSE;
		*vp = STRING_TO_JSVAL(js_str);
	}

	return JS_TRUE;
}

static JSBool js_system_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

#ifndef JSDOOR
	jsval                idval;
	jsint                tiny;

	JS_IdToValue(cx, id, &idval);
	tiny = JSVAL_TO_INT(idval);

	switch (tiny) {
		case SYS_PROP_MISC:
			JS_ValueToECMAUint32(cx, *vp, (uint32_t*)&sys->cfg->sys_misc);
			break;
		case SYS_PROP_LOGIN:
			JS_ValueToECMAUint32(cx, *vp, (uint32_t*)&sys->cfg->sys_login);
			break;
		case SYS_PROP_OP_AVAIL:
			if (!set_sysop_availability(sys->cfg, JSVAL_TO_BOOLEAN(*vp))) {
				JS_ReportError(cx, "%s: Failed to set sysop availability", __FUNCTION__);
				return JS_FALSE;
			}
			break;
		case SYS_PROP_STATS_INTERVAL:
			JS_ValueToECMAUint32(cx, *vp, &sys->cfg->stats_interval);
			break;
	}
#endif

	return JS_TRUE;
}


#define SYSOBJ_RO_FLAGS JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY
#define SYSOBJ_RW_FLAGS JSPROP_ENUMERATE | JSPROP_PERMANENT

static jsSyncPropertySpec js_system_properties[] = {
/*		 name,						tinyid,				flags,				ver	*/

#ifndef JSDOOR
	{   "name",                     SYS_PROP_NAME,      SYSOBJ_RO_FLAGS,       310
		, JSDOCSTR("BBS name")},
	{   "operator",                 SYS_PROP_OP,        SYSOBJ_RO_FLAGS,       310
		, JSDOCSTR("Operator name")},
	{   "operator_available",       SYS_PROP_OP_AVAIL,  SYSOBJ_RW_FLAGS,       31801
		, JSDOCSTR("Operator is available for chat")},
	{   "guru",                     SYS_PROP_GURU,      SYSOBJ_RO_FLAGS,       32000
		, JSDOCSTR("Default Guru (AI) name")},
	{   "qwk_id",                   SYS_PROP_ID,        SYSOBJ_RO_FLAGS,       310
		, JSDOCSTR("System QWK-ID (for QWK packets)")},
	{   "settings",                 SYS_PROP_MISC,      SYSOBJ_RW_FLAGS,      310
		, JSDOCSTR("Settings bit-flags (see <tt>SYS_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions)")},
	{   "login_settings",           SYS_PROP_LOGIN,     SYSOBJ_RW_FLAGS,      32000
		, JSDOCSTR("Login control settings bit-flags (see <tt>LOGIN_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions)")},
	{   "inetaddr",                 SYS_PROP_INETADDR,  JSPROP_READONLY,       310  }, /* alias */
	{   "inet_addr",                SYS_PROP_INETADDR,  SYSOBJ_RO_FLAGS,       311
		, JSDOCSTR("Internet address (host or domain name)")},
	{   "location",                 SYS_PROP_LOCATION,  SYSOBJ_RO_FLAGS,       310
		, JSDOCSTR("Location (city, state)")},
	{   "timezone",                 SYS_PROP_TIMEZONE,  SYSOBJ_RO_FLAGS,       310
		, JSDOCSTR("Local timezone in SMB format (use <i>system.zonestr()</i> to get string representation)")},
	{   "tz_offset",                SYS_PROP_TZ_OFFSET, SYSOBJ_RO_FLAGS,       320
		, JSDOCSTR("Local timezone offset, in minutes, from UTC (negative values represent zones <i>west</i> of UTC, positive values represent zones <i>east</i> of UTC)")},
	{   "date_format",              SYS_PROP_DATE_FMT,  SYSOBJ_RO_FLAGS,       32002
		, JSDOCSTR("Date representation (0=Month first, 1=Day first, 2=Year first)")},
	{   "date_separator",           SYS_PROP_DATE_SEP,  SYSOBJ_RO_FLAGS,       32002
		, JSDOCSTR("Short (8 character) date field-separator")},
	{   "date_verbal",              SYS_PROP_DATE_VERBAL, SYSOBJ_RO_FLAGS,     32002
		, JSDOCSTR("Short date month-name displayed verbally instead of numerically")},
	{   "birthdate_format",         SYS_PROP_BIRTHDATE_FMT, SYSOBJ_RO_FLAGS,   32002
		, JSDOCSTR("User birth date input and display format (MM=Month number, DD=Day of month, YYYY=Year)")},
	{   "birthdate_template",       SYS_PROP_BIRTHDATE_TEMPLATE, SYSOBJ_RO_FLAGS, 32002
		, JSDOCSTR("User birth date input template")},
	{   "phonenumber_template",     SYS_PROP_PHONENUM_TEMPLATE, SYSOBJ_RO_FLAGS, 321
		, JSDOCSTR("User phone number input template")},
	{   "pwdays",                   SYS_PROP_PWDAYS,    SYSOBJ_RO_FLAGS,       310
		, JSDOCSTR("Days between forced user password changes (<tt>0</tt>=<i>never</i>)")},
	{   "min_password_length",      SYS_PROP_MINPWLEN,  SYSOBJ_RO_FLAGS,       31702
		, JSDOCSTR("Minimum number of characters in user passwords")},
	{   "max_password_length",      SYS_PROP_MAXPWLEN,  SYSOBJ_RO_FLAGS,       31702
		, JSDOCSTR("Maximum number of characters in user passwords")},
	{   "deldays",                  SYS_PROP_DELDAYS,   SYSOBJ_RO_FLAGS,       310
		, JSDOCSTR("Days to preserve deleted user records, record will not be reused/overwritten during this period")},
	{   "autodel",                  SYS_PROP_AUTODEL,   SYSOBJ_RO_FLAGS,       31702
		, JSDOCSTR("Days of user inactivity before auto-deletion (<tt>0</tt>=<i>disabled</i>), N/A to P-exempt users")},

	{   "last_user",                SYS_PROP_LASTUSER, SYSOBJ_RO_FLAGS,  311
		, JSDOCSTR("Last user record number in user database (includes deleted and inactive user records)")},
	{   "lastuser",                 SYS_PROP_LASTUSER, JSPROP_READONLY,   311  },   /* alias */
	{   "last_useron",              SYS_PROP_LASTUSERON, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("Name of last user to logoff")},
	{   "lastuseron",               SYS_PROP_LASTUSERON, JSPROP_READONLY,   310  }, /* alias */
#endif
	{   "freediskspace",            SYS_PROP_FREEDISKSPACE, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("Amount of free disk space (in bytes)")},
	{   "freediskspacek",           SYS_PROP_FREEDISKSPACEK, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("Amount of free disk space (in kibibytes)")},

#ifndef JSDOOR
	{   "nodes",                    SYS_PROP_NODES,     SYSOBJ_RO_FLAGS,       310
		, JSDOCSTR("Total number of Terminal Server nodes")},
	{   "last_node",                SYS_PROP_LASTNODE,  SYSOBJ_RO_FLAGS,       310
		, JSDOCSTR("Last displayable node number")},
	{   "lastnode",                 SYS_PROP_LASTNODE,  JSPROP_READONLY,    310  }, /* alias */

	{   "mqtt_enabled",             SYS_PROP_MQTT_ENABLED,  SYSOBJ_RO_FLAGS,   320
		, JSDOCSTR("MQTT support (connection to MQTT broker) is enabled")},

	{   "newuser_password",         SYS_PROP_NEW_PASS, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("New user password (NUP, optional)")},
	{   "newuser_magic_word",       SYS_PROP_NEW_MAGIC, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("New user magic word (optional)")},
	{   "newuser_level",            SYS_PROP_NEW_LEVEL, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("New user security level")},
	{   "newuser_flags1",           SYS_PROP_NEW_FLAGS1, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("New user flag set #1")},
	{   "newuser_flags2",           SYS_PROP_NEW_FLAGS2, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("New user flag set #2")},
	{   "newuser_flags3",           SYS_PROP_NEW_FLAGS3, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("New user flag set #3")},
	{   "newuser_flags4",           SYS_PROP_NEW_FLAGS4, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("New user flag set #4")},
	{   "newuser_restrictions",     SYS_PROP_NEW_REST, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("New user restriction flags")},
	{   "newuser_exemptions",       SYS_PROP_NEW_EXEMPT, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("New user exemption flags")},
	{   "newuser_credits",          SYS_PROP_NEW_CDT, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("New user credits")},
	{   "newuser_minutes",          SYS_PROP_NEW_MIN, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("New user extra minutes")},
	{   "newuser_command_shell",    SYS_PROP_NEW_SHELL, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("New user default command shell")},
	{   "newuser_editor",           SYS_PROP_NEW_XEDIT, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("New user default external editor")},
	{   "newuser_settings",         SYS_PROP_NEW_MISC, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("New user default settings")},
	{   "newuser_download_protocol", SYS_PROP_NEW_PROT, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("New user default file transfer protocol (command key)")},
	{   "newuser_expiration_days",  SYS_PROP_NEW_EXPIRE, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("New user expiration days")},
	{   "newuser_questions",        SYS_PROP_NEW_UQ, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("New user questions/prompts (see <tt>UQ_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions)")},

	{   "expired_level",            SYS_PROP_EXPIRED_LEVEL, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("Expired user security level")},
	{   "expired_flags1",           SYS_PROP_EXPIRED_FLAGS1, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("Expired user flag set #1")},
	{   "expired_flags2",           SYS_PROP_EXPIRED_FLAGS2, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("Expired user flag set #2")},
	{   "expired_flags3",           SYS_PROP_EXPIRED_FLAGS3, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("Expired user flag set #3")},
	{   "expired_flags4",           SYS_PROP_EXPIRED_FLAGS4, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("Expired user flag set #4")},
	{   "expired_restrictions",     SYS_PROP_EXPIRED_REST, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("Expired user restriction flags")},
	{   "expired_exemptions",       SYS_PROP_EXPIRED_EXEMPT, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("Expired user exemption flags")},

	/* directories */
	{   "node_dir",                 SYS_PROP_NODE_DIR, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("Current node directory")},
#endif
	{   "ctrl_dir",                 SYS_PROP_CTRL_DIR, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("Control file directory")},
	{   "data_dir",                 SYS_PROP_DATA_DIR, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("Data file directory")},
	{   "text_dir",                 SYS_PROP_TEXT_DIR, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("Text file directory")},
	{   "temp_dir",                 SYS_PROP_TEMP_DIR, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("Temporary file directory")},
	{   "exec_dir",                 SYS_PROP_EXEC_DIR, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("Executable file directory")},
	{   "mods_dir",                 SYS_PROP_MODS_DIR, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("Modified modules directory (optional)")},
	{   "logs_dir",                 SYS_PROP_LOGS_DIR, SYSOBJ_RO_FLAGS,  310
		, JSDOCSTR("Log file directory")},

	/* filenames */
	{   "devnull",                  SYS_PROP_DEVNULL, SYSOBJ_RO_FLAGS,  311
		, JSDOCSTR("Platform-specific \"null\" device filename")},
	{   "temp_path",                SYS_PROP_TEMP_PATH, SYSOBJ_RO_FLAGS,  312
		, JSDOCSTR("Platform-specific temporary file directory")},
	{   "cmd_shell",                SYS_PROP_CMD_SHELL, SYSOBJ_RO_FLAGS,  314
		, JSDOCSTR("Platform-specific command processor/shell")},

	/* clock access */
	{   "clock_ticks",              SYS_PROP_CLOCK, SYSOBJ_RO_FLAGS,  311
		, JSDOCSTR("Amount of elapsed time in clock 'ticks'")},
	{   "clock_ticks_per_second",   SYS_PROP_CLOCK_PER_SEC, SYSOBJ_RO_FLAGS,  311
		, JSDOCSTR("Number of clock ticks per second")},
	{   "timer",                    SYS_PROP_TIMER, SYSOBJ_RO_FLAGS,  314
		, JSDOCSTR("High-resolution timer, in seconds (fractional seconds supported)")},
	{   "stats_interval",           SYS_PROP_STATS_INTERVAL, SYSOBJ_RW_FLAGS,  321
		, JSDOCSTR("System statistics interval (cache duration) in seconds")},
	{   "local_host_name",          SYS_PROP_LOCAL_HOSTNAME, SYSOBJ_RO_FLAGS,  311
		, JSDOCSTR("Private host name that uniquely identifies this system on the local network")},
	{   "name_servers",             SYS_PROP_NAME_SERVERS, SYSOBJ_RO_FLAGS,  31802
		, JSDOCSTR("Array of nameservers in use by the system")},
	/* last */
	{0}
};

#ifdef BUILD_JSDOCS
static const char* sys_prop_desc[] = {
	/* Manually created (non-tabled) properties */
	"Public host name that uniquely identifies this system on the Internet (usually the same as <i>system.inet_addr</i>)"
	, "Socket library version information"
	, "Time/date system was brought online (in time_t format)"
	, "Synchronet full version information (e.g. '3.10k Beta Debug')"
	, "Synchronet Git repository branch name"
	, "Synchronet Git repository commit hash"
	, "Synchronet Git repository commit date/time"
	, "Date and time compiled"
	, "Synchronet version number (e.g. '3.10')"
	, "Synchronet revision letter (e.g. 'k')"
	, "Synchronet alpha/beta designation (e.g. ' beta')"
	, "Synchronet version notice (includes version and platform)"
	, "Synchronet version number in decimal (e.g. 31301 for v3.13b)"
	, "Synchronet version number in hexadecimal (e.g. 0x31301 for v3.13b)"
	, "Synchronet Git repository commit date/time (seconds since Unix epoch)"
	, "Platform description (e.g. 'Win32', 'Linux', 'FreeBSD')"
	, "Architecture description (e.g. 'i386', 'i686', 'x86_64')"
	, "Message base library version information"
	, "Compiler used to build Synchronet"
	, "Synchronet copyright display"
	, "JavaScript engine version information"
	, "Operating system version information"
	, "Array of FidoNet Technology Network (FTN) addresses associated with this system"
	, NULL
};
#endif


/* System Stats Propertiess */
enum {
	  SYSSTAT_PROP_LOGONS
	, SYSSTAT_PROP_LTODAY
	, SYSSTAT_PROP_TIMEON
	, SYSSTAT_PROP_TTODAY
	, SYSSTAT_PROP_ULS
	, SYSSTAT_PROP_ULB
	, SYSSTAT_PROP_DLS
	, SYSSTAT_PROP_DLB
	, SYSSTAT_PROP_PTODAY
	, SYSSTAT_PROP_ETODAY
	, SYSSTAT_PROP_FTODAY
	, SYSSTAT_PROP_NUSERS

	, SYSSTAT_PROP_TOTALUSERS
	, SYSSTAT_PROP_TOTALFILES
	, SYSSTAT_PROP_TOTALMSGS
	, SYSSTAT_PROP_TOTALMAIL
	, SYSSTAT_PROP_FEEDBACK

	, SYSSTAT_PROP_NODE_GETS
};

#ifndef JSDOOR
static JSBool js_sysstats_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval                idval;
	jsint                tiny;
	int                  i;
	ulong                l;
	jsrefcount           rc;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)JS_GetPrivate(cx, obj)) == NULL) {
		JS_ReportError(cx, "JS_GetPrivate failure in %s", __FUNCTION__);
		return JS_FALSE;
	}
	scfg_t* cfg = sys->cfg;

	JS_IdToValue(cx, id, &idval);
	tiny = JSVAL_TO_INT(idval);

	if (tiny < SYSSTAT_PROP_TOTALUSERS) {
		rc = JS_SUSPENDREQUEST(cx);
		if (!getstats_cached(cfg, 0, &sys->stats)) {
			JS_RESUMEREQUEST(cx, rc);
			JS_ReportError(cx, "getstats failure in %s", __FUNCTION__);
			return JS_FALSE;
		}
		JS_RESUMEREQUEST(cx, rc);
	}

	switch (tiny) {
		case SYSSTAT_PROP_LOGONS:
			*vp = UINT_TO_JSVAL(sys->stats.logons);
			break;
		case SYSSTAT_PROP_LTODAY:
			*vp = UINT_TO_JSVAL(sys->stats.ltoday);
			break;
		case SYSSTAT_PROP_TIMEON:
			*vp = UINT_TO_JSVAL(sys->stats.timeon);
			break;
		case SYSSTAT_PROP_TTODAY:
			*vp = UINT_TO_JSVAL(sys->stats.ttoday);
			break;
		case SYSSTAT_PROP_ULS:
			*vp = UINT_TO_JSVAL(sys->stats.uls);
			break;
		case SYSSTAT_PROP_ULB:
			*vp = DOUBLE_TO_JSVAL((double)sys->stats.ulb);
			break;
		case SYSSTAT_PROP_DLS:
			*vp = UINT_TO_JSVAL(sys->stats.dls);
			break;
		case SYSSTAT_PROP_DLB:
			*vp = DOUBLE_TO_JSVAL((double)sys->stats.dlb);
			break;
		case SYSSTAT_PROP_PTODAY:
			*vp = UINT_TO_JSVAL(sys->stats.ptoday);
			break;
		case SYSSTAT_PROP_ETODAY:
			*vp = UINT_TO_JSVAL(sys->stats.etoday);
			break;
		case SYSSTAT_PROP_FTODAY:
			*vp = UINT_TO_JSVAL(sys->stats.ftoday);
			break;
		case SYSSTAT_PROP_NUSERS:
			*vp = UINT_TO_JSVAL(sys->stats.nusers);
			break;

		case SYSSTAT_PROP_TOTALUSERS:
			rc = JS_SUSPENDREQUEST(cx);
			*vp = INT_TO_JSVAL(total_users(cfg));
			JS_RESUMEREQUEST(cx, rc);
			break;
		case SYSSTAT_PROP_TOTALMSGS:
			l = 0;
			rc = JS_SUSPENDREQUEST(cx);
			for (i = 0; i < cfg->total_subs; i++)
				l += getposts(cfg, i);
			JS_RESUMEREQUEST(cx, rc);
			*vp = DOUBLE_TO_JSVAL((double)l);
			break;
		case SYSSTAT_PROP_TOTALFILES:
			l = 0;
			rc = JS_SUSPENDREQUEST(cx);
			for (i = 0; i < cfg->total_dirs; i++)
				l += getfiles(cfg, i);
			JS_RESUMEREQUEST(cx, rc);
			*vp = DOUBLE_TO_JSVAL((double)l);
			break;
		case SYSSTAT_PROP_TOTALMAIL:
			rc = JS_SUSPENDREQUEST(cx);
			*vp = INT_TO_JSVAL(getmail(cfg, /* user: */ 0, /* Sent: */ FALSE, /* SPAM: */ FALSE));
			JS_RESUMEREQUEST(cx, rc);
			break;
		case SYSSTAT_PROP_FEEDBACK:
			rc = JS_SUSPENDREQUEST(cx);
			*vp = INT_TO_JSVAL(getmail(cfg, /* user: */ 1, /* Sent: */ FALSE, /* SPAM: */ FALSE));
			JS_RESUMEREQUEST(cx, rc);
			break;

		case SYSSTAT_PROP_NODE_GETS:
			*vp = INT_TO_JSVAL(sys->nodegets);
			break;
	}

	return TRUE;
}

#define SYSSTAT_FLAGS JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT

static jsSyncPropertySpec js_sysstats_properties[] = {
/*		 name,						tinyid,						flags,			ver	*/

	{   "total_logons",             SYSSTAT_PROP_LOGONS,        SYSSTAT_FLAGS,  310 },
	{   "logons_today",             SYSSTAT_PROP_LTODAY,        SYSSTAT_FLAGS,  310 },
	{   "total_timeon",             SYSSTAT_PROP_TIMEON,        SYSSTAT_FLAGS,  310 },
	{   "timeon_today",             SYSSTAT_PROP_TTODAY,        SYSSTAT_FLAGS,  310 },
	{   "total_files",              SYSSTAT_PROP_TOTALFILES,    SYSSTAT_FLAGS,  310 },
	{   "files_uploaded_today",     SYSSTAT_PROP_ULS,           SYSSTAT_FLAGS,  310 },
	{   "bytes_uploaded_today",     SYSSTAT_PROP_ULB,           SYSSTAT_FLAGS,  310 },
	{   "files_downloaded_today",   SYSSTAT_PROP_DLS,           SYSSTAT_FLAGS,  310 },
	{   "bytes_downloaded_today",   SYSSTAT_PROP_DLB,           SYSSTAT_FLAGS,  310 },
	{   "total_messages",           SYSSTAT_PROP_TOTALMSGS,     SYSSTAT_FLAGS,  310 },
	{   "messages_posted_today",    SYSSTAT_PROP_PTODAY,        SYSSTAT_FLAGS,  310 },
	{   "total_email",              SYSSTAT_PROP_TOTALMAIL,     SYSSTAT_FLAGS,  310 },
	{   "email_sent_today",         SYSSTAT_PROP_ETODAY,        SYSSTAT_FLAGS,  310 },
	{   "total_feedback",           SYSSTAT_PROP_FEEDBACK,      SYSSTAT_FLAGS,  310 },
	{   "feedback_sent_today",      SYSSTAT_PROP_FTODAY,        SYSSTAT_FLAGS,  310 },
	{   "total_users",              SYSSTAT_PROP_TOTALUSERS,    SYSSTAT_FLAGS,  310 },
	{   "new_users_today",          SYSSTAT_PROP_NUSERS,        SYSSTAT_FLAGS,  310 },
	{   "node_gets",                SYSSTAT_PROP_NODE_GETS,     JSPROP_READONLY, 31702 },
	{0}
};

#if !defined(JSDOOR) && defined(BUILD_JSDOCS)
static const char* sysstat_prop_desc[] = {
	"Total logons"
	, "Logons today"
	, "Total time used"
	, "Time used today"
	, "Total files in file bases"
	, "Files uploaded today"
	, "Bytes uploaded today"
	, "Files downloaded today"
	, "Bytes downloaded today"
	, "Total messages in message bases"
	, "Messages posted today"
	, "Total messages in mail base"
	, "Email sent today"
	, "Total feedback messages waiting"
	, "Feedback sent today"
	, "Total user records (does not include deleted or inactive user records)"
	, "New users today"
	, NULL
};
#endif

static JSBool js_sysstats_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*  name = NULL;
	JSBool ret;

	if (id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;

		JS_IdToValue(cx, id, &idval);
		if (JSVAL_IS_STRING(idval)) {
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
			HANDLE_PENDING(cx, name);
		}
	}

	ret = js_SyncResolve(cx, obj, name, js_sysstats_properties, NULL, NULL, 0);
	if (name)
		free(name);
	return ret;
}

static JSBool js_sysstats_enumerate(JSContext *cx, JSObject *obj)
{
	return js_sysstats_resolve(cx, obj, JSID_VOID);
}

static JSClass js_sysstats_class = {
	"Stats"                 /* name			*/
	, JSCLASS_HAS_PRIVATE    /* flags		*/
	, JS_PropertyStub        /* addProperty	*/
	, JS_PropertyStub        /* delProperty	*/
	, js_sysstats_get        /* getProperty	*/
	, JS_StrictPropertyStub  /* setProperty	*/
	, js_sysstats_enumerate  /* enumerate	*/
	, js_sysstats_resolve    /* resolve		*/
	, JS_ConvertStub         /* convert		*/
	, JS_FinalizeStub        /* finalize		*/
};

static JSBool
js_alias(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	char*                p;
	char                 buf[128];
	JSString*            js_str;
	jsrefcount           rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	if ((js_str = JS_ValueToString(cx, argv[0])) == NULL) {
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(0));
		return JS_TRUE;
	}

	JSSTRING_TO_ASTRING(cx, js_str, p, 128, NULL);
	if (p == NULL) {
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(0));
		return JS_TRUE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	p = alias(sys->cfg, p, buf);
	JS_RESUMEREQUEST(cx, rc);

	if ((js_str = JS_NewStringCopyZ(cx, p)) == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_username(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	int32                val;
	char                 buf[128];
	JSString*            js_str;
	char*                cstr;
	jsrefcount           rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (JSVAL_NULL_OR_VOID(argv[0])) {
		JS_SET_RVAL(cx, arglist, JS_GetEmptyStringValue(cx));
		return JS_TRUE;
	}
	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	val = 0;
	JS_ValueToInt32(cx, argv[0], &val);

	rc = JS_SUSPENDREQUEST(cx);
	cstr = username(sys->cfg, val, buf);
	JS_RESUMEREQUEST(cx, rc);
	if ((js_str = JS_NewStringCopyZ(cx, cstr)) == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_matchuser(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	char*                p;
	JSString*            js_str;
	BOOL                 sysop_alias = TRUE;
	jsrefcount           rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (JSVAL_NULL_OR_VOID(argv[0])) {
		JS_SET_RVAL(cx, arglist, JSVAL_ZERO);
		return JS_TRUE;
	}
	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	if ((js_str = JS_ValueToString(cx, argv[0])) == NULL)
		return JS_FALSE;

	if (argc > 1)
		JS_ValueToBoolean(cx, argv[1], &sysop_alias);

	JSSTRING_TO_ASTRING(cx, js_str, p, (LEN_ALIAS > LEN_NAME) ? LEN_ALIAS + 2:LEN_NAME + 2, NULL);
	if (p == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(matchuser(sys->cfg, p, sysop_alias)));
	JS_RESUMEREQUEST(cx, rc);
	return JS_TRUE;
}

static JSBool
js_matchuserdata(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	char*                p;
	JSString*            js_str;
	int32                field = 0;
	int32                usernumber = 0;
	int                  len;
	jsrefcount           rc;
	BOOL                 match_del = FALSE;
	BOOL                 match_next = FALSE;
	uintN                argnum = 2;

	if (js_argcIsInsufficient(cx, argc, 2))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;
	if (JSVAL_NULL_OR_VOID(argv[1])) {
		JS_SET_RVAL(cx, arglist, JSVAL_ZERO);
		return JS_TRUE;
	}
	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	JS_ValueToInt32(cx, argv[0], &field);
	rc = JS_SUSPENDREQUEST(cx);
	len = user_field_len(static_cast<user_field>(field));
	JS_RESUMEREQUEST(cx, rc);
	if (len < 1) {
		JS_ReportError(cx, "Invalid user field: %d", field);
		return JS_FALSE;
	}

	if ((js_str = JS_ValueToString(cx, argv[1])) == NULL)
		return JS_FALSE;

	if (argnum < argc && JSVAL_IS_BOOLEAN(argv[argnum]))
		JS_ValueToBoolean(cx, argv[argnum++], &match_del);
	if (argnum < argc && JSVAL_IS_NUMBER(argv[argnum]))
		JS_ValueToInt32(cx, argv[argnum++], &usernumber);
	if (argnum < argc && JSVAL_IS_BOOLEAN(argv[argnum]))
		JS_ValueToBoolean(cx, argv[argnum++], &match_next);

	JSSTRING_TO_ASTRING(cx, js_str, p, 128, NULL);
	if (p == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	int result = finduserstr(sys->cfg, usernumber, static_cast<user_field>(field), p, match_del, match_next, NULL, NULL);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(result));
	JS_RESUMEREQUEST(cx, rc);
	return JS_TRUE;
}

static JSBool
js_find_login_id(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	char*                p;
	JSString*            js_str;
	jsrefcount           rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (JSVAL_NULL_OR_VOID(argv[0])) {
		JS_SET_RVAL(cx, arglist, JSVAL_ZERO);
		return JS_TRUE;
	}

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	if ((js_str = JS_ValueToString(cx, argv[0])) == NULL)
		return JS_FALSE;

	JSSTRING_TO_ASTRING(cx, js_str, p, (LEN_ALIAS > LEN_NAME) ? LEN_ALIAS + 2:LEN_NAME + 2, NULL);
	if (p == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(find_login_id(sys->cfg, p)));
	JS_RESUMEREQUEST(cx, rc);
	return JS_TRUE;
}

#endif

static JSBool
js_trashcan(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	char*                str;
	char*                can = NULL;
	JSString*            js_str;
	JSString*            js_can;
	jsrefcount           rc;
	BOOL                 ret;

	if (js_argcIsInsufficient(cx, argc, 2))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0) || js_argvIsNullOrVoid(cx, argv, 1))
		return JS_FALSE;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	if ((js_can = JS_ValueToString(cx, argv[0])) == NULL) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return JS_TRUE;
	}

	if ((js_str = JS_ValueToString(cx, argv[1])) == NULL) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return JS_TRUE;
	}

	JSSTRING_TO_MSTRING(cx, js_can, can, NULL);
	HANDLE_PENDING(cx, can);
	if (can == NULL) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return JS_TRUE;
	}

	JSSTRING_TO_MSTRING(cx, js_str, str, NULL);
	if (JS_IsExceptionPending(cx)) {
		FREE_AND_NULL(str);
		free(can);
		return JS_FALSE;
	}
	if (str == NULL) {
		free(can);
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return JS_TRUE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	ret = trashcan(sys->cfg, str, can);
	free(can);
	free(str);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return JS_TRUE;
}

static JSBool
js_findstr(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      str;
	char*      fname = NULL;
	JSString*  js_str;
	JSString*  js_fname;
	jsrefcount rc;
	BOOL       ret;
	str_list_t list = NULL;

	if (js_argcIsInsufficient(cx, argc, 2))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;
	if (JSVAL_NULL_OR_VOID(argv[1])) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return JS_TRUE;
	}
	if (JSVAL_IS_OBJECT(argv[0])) {
		JSObject* array = JSVAL_TO_OBJECT(argv[0]);
		if (array == NULL || !JS_IsArrayObject(cx, array))
			return JS_TRUE;
		jsuint    count;
		if (!JS_GetArrayLength(cx, array, &count))
			return JS_TRUE;
		char*     tmp = NULL;
		size_t    tmplen = 0;
		for (jsuint i = 0; i < count; i++) {
			jsval val;
			if (!JS_GetElement(cx, array, i, &val))
				break;
			if (!JSVAL_IS_STRING(val))   /* must be an array of strings */
				break;
			JSVALUE_TO_RASTRING(cx, val, tmp, &tmplen, NULL);
			HANDLE_PENDING(cx, tmp);
			strListPush(&list, tmp);
		}
		free(tmp);
	}
	else {
		if ((js_fname = JS_ValueToString(cx, argv[0])) == NULL)
			return JS_FALSE;
		JSSTRING_TO_MSTRING(cx, js_fname, fname, NULL);
		HANDLE_PENDING(cx, fname);
		if (fname == NULL)
			return JS_FALSE;
	}
	if ((js_str = JS_ValueToString(cx, argv[1])) == NULL) {
		free(fname);
		return JS_FALSE;
	}
	JSSTRING_TO_MSTRING(cx, js_str, str, NULL);
	if (JS_IsExceptionPending(cx)) {
		FREE_AND_NULL(str);
		free(fname);
		return JS_FALSE;
	}
	if (str == NULL) {
		free(fname);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	if (list != NULL)
		ret = findstr_in_list(str, list, NULL);
	else
		ret = findstr(str, fname);
	free(str);
	free(fname);
	strListFree(&list);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return JS_TRUE;
}

static JSBool
js_zonestr(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	JSString*            js_str;
	short                zone;
	int32                val = 0;
	jsrefcount           rc;
	char*                cstr;

	if (argc > 0 && js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	if (argc < 1)
		zone = sys_timezone(sys->cfg);
	else {
		JS_ValueToInt32(cx, argv[0], &val);
		zone = (short)val;
	}

	rc = JS_SUSPENDREQUEST(cx);
	cstr = smb_zonestr(zone, NULL);
	JS_RESUMEREQUEST(cx, rc);
	if ((js_str = JS_NewStringCopyZ(cx, cstr)) == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

/* Returns a ctime()-like string in the system-preferred time format */
static JSBool
js_timestr(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	char                 str[128];
	jsdouble             ti;
	JSString*            js_str;
	jsrefcount           rc;

	if (argc > 0 && js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	if (argc < 1)
		ti = (jsdouble)time(NULL);  /* use current time */
	else
		if (!JS_ValueToNumber(cx, argv[0], &ti))
			return JS_TRUE;
	rc = JS_SUSPENDREQUEST(cx);
	timestr(sys->cfg, (time32_t)ti, str);
	JS_RESUMEREQUEST(cx, rc);
	if ((js_str = JS_NewStringCopyZ(cx, str)) == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

/* Convert between string and time_t representations of date */
static JSBool
js_datestr(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	char                 str[128];
	time32_t             t;
	JSString*            js_str;
	char *               p;
	enum date_fmt        fmt;

	if (argc > 0 && js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	fmt = sys->cfg->sys_date_fmt;

	if (argc < 1)
		t = time32(NULL); /* use current time */
	else {
		if (JSVAL_IS_STRING(argv[0])) {  /* convert from string to time_t? */
			JSVALUE_TO_ASTRING(cx, argv[0], p, 10, NULL);
			JS_SET_RVAL(cx, arglist, DOUBLE_TO_JSVAL((double)dstrtounix(fmt, p)));
			return JS_TRUE;
		}
		JS_ValueToECMAUint32(cx, argv[0], (uint32_t*)&t);
	}
	if ((js_str = JS_NewStringCopyZ(cx, datestr(sys->cfg, t, str))) == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_secondstr(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *   argv = JS_ARGV(cx, arglist);
	char      str[128];
	uint32_t  t = 0;
	JSString* js_str;
	bool      verbose = true;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	if (argc > 1 && JSVAL_IS_BOOLEAN(argv[1]))
		verbose = JSVAL_TO_BOOLEAN(argv[1]);

	JS_ValueToECMAUint32(cx, argv[0], &t);
	if ((js_str = JS_NewStringCopyZ(cx, verbose ? sectostr(t, str) : seconds_to_str(t, str))) == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_minutestr(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *   argv = JS_ARGV(cx, arglist);
	char      str[128];
	uint32_t  t = 0;
	JSString* js_str;
	bool      estimate = false;
	bool      words = false;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	if (argc > 1 && JSVAL_IS_BOOLEAN(argv[1]))
		estimate = JSVAL_TO_BOOLEAN(argv[1]);

	if (argc > 2 && JSVAL_IS_BOOLEAN(argv[2]))
		words = JSVAL_TO_BOOLEAN(argv[2]);

	JS_ValueToECMAUint32(cx, argv[0], &t);
	if ((js_str = JS_NewStringCopyZ(cx, minutes_to_str(t, str, sizeof str, estimate, words))) == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

#ifndef JSDOOR
static JSBool
js_spamlog(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	uintN                i;
	char*                p = NULL;
	char*                prot = NULL;
	char*                action = NULL;
	char*                reason = NULL;
	char*                host = NULL;
	char*                ip_addr = NULL;
	char*                to = NULL;
	char*                from = NULL;
	jsrefcount           rc;
	BOOL                 ret;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	for (i = 0; i < argc && from == NULL; i++) {
		if (!JSVAL_IS_STRING(argv[i]))
			continue;
		JSVALUE_TO_MSTRING(cx, argv[i], p, NULL);
		if (p == NULL || JS_IsExceptionPending(cx)) {
			free(prot);
			free(action);
			free(reason);
			free(host);
			free(ip_addr);
			free(to);
			free(p);
			return JS_FALSE;
		}
		if (prot == NULL)
			prot = p;
		else if (action == NULL)
			action = p;
		else if (reason == NULL)
			reason = p;
		else if (host == NULL)
			host = p;
		else if (ip_addr == NULL)
			ip_addr = p;
		else if (to == NULL)
			to = p;
		else
			from = p;
	}
	rc = JS_SUSPENDREQUEST(cx);
	ret = spamlog(sys->cfg, sys->mqtt, prot, action, reason, host, ip_addr, to, from);
	free(prot);
	free(action);
	free(reason);
	free(host);
	free(ip_addr);
	free(to);
	free(from);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return JS_TRUE;
}

static JSBool
js_hacklog(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	uintN                i;
	int32                i32 = 0;
	char*                p = NULL;
	char*                prot = NULL;
	char*                user = NULL;
	char*                text = NULL;
	char*                host = NULL;
	union xp_sockaddr    addr;
	jsrefcount           rc;
	BOOL                 ret;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	memset(&addr, 0, sizeof(addr));
	for (i = 0; i < argc; i++) {
		if (JSVAL_IS_NUMBER(argv[i])) {
			JS_ValueToInt32(cx, argv[i], &i32);
			if (addr.in.sin_addr.s_addr == 0)
				addr.in.sin_addr.s_addr = i32;
			else
				addr.in.sin_port = (ushort)i32;
			continue;
		}
		if (!JSVAL_IS_STRING(argv[i]))
			continue;
		if (host == NULL) {
			JSVALUE_TO_MSTRING(cx, argv[i], p, NULL);
			if (JS_IsExceptionPending(cx) || p == NULL) {
				free(prot);
				free(user);
				free(text);
				free(p);
				return JS_FALSE;
			}
			if (prot == NULL)
				prot = p;
			else if (user == NULL)
				user = p;
			else if (text == NULL)
				text = p;
			else
				host = p;
		}
	}
	rc = JS_SUSPENDREQUEST(cx);
	ret = hacklog(sys->cfg, sys->mqtt, prot, user, text, host, &addr);
	free(prot);
	free(user);
	free(text);
	free(host);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return JS_TRUE;
}

static JSBool
js_filter_ip(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	uintN                i;
	char*                p = NULL;
	char*                prot = NULL;
	char*                reason = NULL;
	char*                host = NULL;
	char*                ip_addr = NULL;
	char*                from = NULL;
	char*                fname = NULL;
	jsint                duration = 0;
	jsrefcount           rc;
	BOOL                 ret;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	for (i = 0; i < argc && fname == NULL; i++) {
		if (JSVAL_IS_NUMBER(argv[i])) {
			JS_ValueToInt32(cx, argv[i], &duration);
			continue;
		}
		if (!JSVAL_IS_STRING(argv[i]))
			continue;
		JSVALUE_TO_MSTRING(cx, argv[i], p, NULL);
		if (JS_IsExceptionPending(cx) || p == NULL) {
			free(prot);
			free(reason);
			free(host);
			free(ip_addr);
			free(from);
			free(p);
			return JS_FALSE;
		}
		if (prot == NULL)
			prot = p;
		else if (reason == NULL)
			reason = p;
		else if (host == NULL)
			host = p;
		else if (ip_addr == NULL)
			ip_addr = p;
		else if (from == NULL)
			from = p;
		else
			fname = p;
	}
	rc = JS_SUSPENDREQUEST(cx);
	ret = filter_ip(sys->cfg, prot, reason, host, ip_addr, from, fname, duration);
	free(prot);
	free(reason);
	free(host);
	free(ip_addr);
	free(from);
	free(fname);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return JS_TRUE;
}

static JSBool
js_get_node(JSContext *cx, uintN argc, jsval *arglist)
{
	char                 str[128];
	JSObject*            obj = JS_THIS_OBJECT(cx, arglist);
	JSObject*            nodeobj;
	jsval*               argv = JS_ARGV(cx, arglist);
	node_t               node {};
	int32                node_num;
	jsrefcount           rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;
	scfg_t*              cfg = sys->cfg;

	node_num = cfg->node_num;
	if (argc)  {
		if (!JS_ValueToInt32(cx, argv[0], &node_num))
			return JS_FALSE;
	}
	if (node_num < 1)
		node_num = 1;

	rc = JS_SUSPENDREQUEST(cx);

	int retval = getnodedat(sys->cfg, node_num, &node, /* lockit: */ FALSE, &sys->nodefile);
	sys->nodegets++;
	JS_RESUMEREQUEST(cx, rc);
	if (retval != 0) {
		JS_ReportError(cx, "getnodedat(%d) returned %d", node_num, retval);
		return JS_FALSE;
	}
	if ((nodeobj = JS_NewObject(cx, NULL, NULL, obj)) == NULL) {
		JS_ReportError(cx, "JS_NewObject failure");
		return JS_FALSE;
	}

	JS_DefineProperty(cx, nodeobj, "status", INT_TO_JSVAL((int)node.status), NULL, NULL, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, nodeobj, "vstatus", STRING_TO_JSVAL(JS_NewStringCopyZ(cx, node_vstatus(sys->cfg, &node, str, sizeof str))), NULL, NULL, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, nodeobj, "errors", INT_TO_JSVAL((int)node.errors), NULL, NULL, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, nodeobj, "action", INT_TO_JSVAL((int)node.action), NULL, NULL, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, nodeobj, "activity", STRING_TO_JSVAL(JS_NewStringCopyZ(cx, node_activity(sys->cfg, &node, str, sizeof str, node_num))), NULL, NULL, JSPROP_ENUMERATE);
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
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	char*                buf;
	int32                node_num;
	JSString*            js_str;
	jsrefcount           rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;
	scfg_t*              cfg = sys->cfg;

	node_num = cfg->node_num;
	if (argc)
		JS_ValueToInt32(cx, argv[0], &node_num);
	if (node_num < 1)
		node_num = 1;

	rc = JS_SUSPENDREQUEST(cx);
	buf = getnmsg(sys->cfg, node_num);
	JS_RESUMEREQUEST(cx, rc);
	if (buf == NULL) {
		JS_SET_RVAL(cx, arglist, JSVAL_NULL);
		return JS_TRUE;
	}
	js_str = JS_NewStringCopyZ(cx, buf);
	free(buf);

	if (js_str == NULL)
		return JS_FALSE;
	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_put_node_message(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	int32                node = 1;
	JSString*            js_msg;
	char*                msg = NULL;
	jsrefcount           rc;
	BOOL                 ret;

	if (js_argcIsInsufficient(cx, argc, 2))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0) || js_argvIsNullOrVoid(cx, argv, 1))
		return JS_FALSE;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	JS_ValueToInt32(cx, argv[0], &node);
	if (node < 1)
		node = 1;

	if ((js_msg = JS_ValueToString(cx, argv[1])) == NULL)
		return JS_FALSE;

	JSSTRING_TO_MSTRING(cx, js_msg, msg, NULL);
	HANDLE_PENDING(cx, msg);
	if (msg == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	ret = putnmsg(sys->cfg, node, msg) == 0;
	free(msg);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));

	return JS_TRUE;
}

static JSBool
js_get_telegram(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	char*                buf;
	int32                usernumber = 1;
	JSString*            js_str;
	jsrefcount           rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	JS_ValueToInt32(cx, argv[0], &usernumber);
	if (usernumber < 1)
		usernumber = 1;

	rc = JS_SUSPENDREQUEST(cx);
	buf = getsmsg(sys->cfg, usernumber);
	JS_RESUMEREQUEST(cx, rc);
	if (buf == NULL) {
		JS_SET_RVAL(cx, arglist, JSVAL_NULL);
		return JS_TRUE;
	}
	js_str = JS_NewStringCopyZ(cx, buf);
	free(buf);

	if (js_str == NULL)
		return JS_FALSE;
	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_put_telegram(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	int32                usernumber = 1;
	JSString*            js_msg;
	char*                msg = NULL;
	jsrefcount           rc;
	BOOL                 ret;

	if (js_argcIsInsufficient(cx, argc, 2))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0) || js_argvIsNullOrVoid(cx, argv, 1))
		return JS_FALSE;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	JS_ValueToInt32(cx, argv[0], &usernumber);
	if (usernumber < 1)
		usernumber = 1;

	if ((js_msg = JS_ValueToString(cx, argv[1])) == NULL)
		return JS_FALSE;

	JSSTRING_TO_MSTRING(cx, js_msg, msg, NULL);
	HANDLE_PENDING(cx, msg);
	if (msg == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	ret = putsmsg(sys->cfg, usernumber, msg) == 0;
	free(msg);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));

	return JS_TRUE;
}

static JSBool
js_notify(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	int32                usernumber = 1;
	JSString*            js_subj;
	JSString*            js_str;
	char*                subj;
	char*                msg = NULL;
	char*                replyto = NULL;
	jsrefcount           rc;
	BOOL                 ret;

	if (js_argcIsInsufficient(cx, argc, 2))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0) || js_argvIsNullOrVoid(cx, argv, 1))
		return JS_FALSE;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	JS_ValueToInt32(cx, argv[0], &usernumber);
	if (usernumber < 1)
		usernumber = 1;

	if ((js_subj = JS_ValueToString(cx, argv[1])) == NULL)
		return JS_FALSE;

	if (argc > 2 && !JSVAL_NULL_OR_VOID(argv[2])) {
		if ((js_str = JS_ValueToString(cx, argv[2])) == NULL)
			return JS_FALSE;

		JSSTRING_TO_MSTRING(cx, js_str, msg, NULL);
		HANDLE_PENDING(cx, msg);
		if (msg == NULL)
			return JS_TRUE;
	}

	if (argc > 3 && !JSVAL_NULL_OR_VOID(argv[3])) {
		if ((js_str = JS_ValueToString(cx, argv[3])) == NULL) {
			free(msg);
			return JS_FALSE;
		}

		JSSTRING_TO_MSTRING(cx, js_str, replyto, NULL);
		HANDLE_PENDING(cx, replyto);
		if (replyto == NULL) {
			free(msg);
			return JS_TRUE;
		}
	}

	JSSTRING_TO_MSTRING(cx, js_subj, subj, NULL);
	HANDLE_PENDING(cx, subj);
	if (subj == NULL) {
		free(msg);
		free(replyto);
		return JS_TRUE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	ret = notify(sys->cfg, usernumber, subj, /* strip_ctrl: */false, msg, replyto) == 0;
	free(subj);
	free(msg);
	free(replyto);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));

	return JS_TRUE;
}

static JSBool
js_new_user(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	char*                alias;
	int                  i;
	uintN                n;
	user_t               user;
	JSObject*            userobj;
	JSObject*            objarg;
	JSClass*             cl;
	jsrefcount           rc;
	client_t*            client = NULL;
	jsval                val;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;
	scfg_t*              cfg = sys->cfg;

	JSVALUE_TO_ASTRING(cx, argv[0], alias, LEN_ALIAS + 2, NULL);

	rc = JS_SUSPENDREQUEST(cx);
	if (!check_name(cfg, alias, /* unique: */true)) {
		JS_RESUMEREQUEST(cx, rc);
		JS_ReportError(cx, "Invalid or duplicate user alias: %s", alias);
		return JS_FALSE;
	}

	memset(&user, 0, sizeof(user));
	for (n = 0; n < argc; n++) {
		if (JSVAL_IS_OBJECT(argv[n])) {
			objarg = JSVAL_TO_OBJECT(argv[n]);
			if (objarg != NULL && (cl = JS_GetClass(cx, objarg)) != NULL && strcmp(cl->name, "Client") == 0) {
				client = static_cast<client_t *>(JS_GetPrivate(cx, objarg));
				continue;
			}
		}
	}
	// Find and use the global client object if possible...
	if (client == NULL) {
		if (JS_GetProperty(cx, JS_GetGlobalObject(cx), "client", &val) && !JSVAL_NULL_OR_VOID(val)) {
			objarg = JSVAL_TO_OBJECT(val);
			if (objarg != NULL && (cl = JS_GetClass(cx, objarg)) != NULL && strcmp(cl->name, "Client") == 0)
				client = static_cast<client_t *>(JS_GetPrivate(cx, objarg));
		}
	}
	if (client != NULL) {
		SAFECOPY(user.connection, client->protocol);
		SAFECOPY(user.host, client->host);
		SAFECOPY(user.ipaddr, client->addr);
	}

	SAFECOPY(user.alias, alias);
	newuserdefaults(cfg, &user);
	i = newuserdat(cfg, &user);
	JS_RESUMEREQUEST(cx, rc);

	if (i == USER_SUCCESS) {
		userobj = js_CreateUserObject(cx, obj, NULL, &user, /* client: */ NULL, /* global_user: */ FALSE, (struct mqtt*)NULL);
		JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(userobj));
	} else
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(i));

	return JS_TRUE;
}

static JSBool
js_del_user(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	jsrefcount           rc;
	int32                n;
	user_t               user;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	if (!JS_ValueToInt32(cx, argv[0], &n))
		return JS_FALSE;
	user.number = n;
	rc = JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);  /* fail, by default */
	if (getuserdat(sys->cfg, &user) == USER_SUCCESS
	    && del_user(sys->cfg, &user) == USER_SUCCESS)
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);   /* success */
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_undel_user(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	jsrefcount           rc;
	int32                n;
	user_t               user;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	if (!JS_ValueToInt32(cx, argv[0], &n))
		return JS_FALSE;
	user.number = n;
	rc = JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);  /* fail, by default */
	if (getuserdat(sys->cfg, &user) == USER_SUCCESS
	    && undel_user(sys->cfg, &user) == USER_SUCCESS)
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);   /* success */
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}
#endif

static JSBool
js_sys_exec(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	jsrefcount rc;
	char *     cmd = NULL;
	int        ret;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], cmd, NULL);
	HANDLE_PENDING(cx, cmd);
	if (cmd == NULL) {
		JS_ReportError(cx, "Illegal NULL command");
		return JS_FALSE;
	}
	if (*cmd == 0) {
		free(cmd);
		JS_ReportError(cx, "Missing or invalid argument");
		return JS_FALSE;
	}
	rc = JS_SUSPENDREQUEST(cx);
	ret = system(cmd);
	free(cmd);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(ret));

	return JS_TRUE;
}

static JSBool
js_popen(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char       str[1024];
	char*      cmd = NULL;
	FILE*      fp;
	jsint      line = 0;
	jsval      val;
	JSObject*  array;
	JSString*  js_str;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	if ((array = JS_NewArrayObject(cx, 0, NULL)) == NULL)
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], cmd, NULL);
	HANDLE_PENDING(cx, cmd);
	if (cmd == NULL) {
		JS_ReportError(cx, "Illegal NULL command");
		return JS_FALSE;
	}
	rc = JS_SUSPENDREQUEST(cx);
	if ((fp = popen(cmd, "r")) == NULL) {
		free(cmd);
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}
	free(cmd);
	while (!feof(fp)) {
		if (fgets(str, sizeof(str), fp) == NULL)
			break;
		JS_RESUMEREQUEST(cx, rc);
		if ((js_str = JS_NewStringCopyZ(cx, str)) == NULL) {
			rc = JS_SUSPENDREQUEST(cx);
			break;
		}
		val = STRING_TO_JSVAL(js_str);
		if (!JS_SetElement(cx, array, line++, &val)) {
			rc = JS_SUSPENDREQUEST(cx);
			break;
		}
		rc = JS_SUSPENDREQUEST(cx);
	}
	pclose(fp);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array));

	return JS_TRUE;
}

#ifndef JSDOOR
static JSBool
js_chksyspass(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *           obj = JS_THIS_OBJECT(cx, arglist);
	jsval *              argv = JS_ARGV(cx, arglist);
	char *               pass;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (JSVAL_NULL_OR_VOID(argv[0])) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return JS_TRUE;
	}
	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	JSVALUE_TO_ASTRING(cx, argv[0], pass, LEN_PASS + 2, NULL); // +2 is so overly long passwords fail.
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(stricmp(pass, sys->cfg->sys_pass) == 0));

	return JS_TRUE;
}

static JSBool
js_chkname(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      str;
	jsrefcount rc;
	bool       unique = true;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (JSVAL_NULL_OR_VOID(argv[0])) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return JS_TRUE;
	}
	JSVALUE_TO_ASTRING(cx, argv[0], str, (LEN_ALIAS > LEN_NAME)?LEN_ALIAS + 2:LEN_NAME + 2, NULL);
	if (argc > 1 && JSVAL_IS_BOOLEAN(argv[1]))
		unique = JSVAL_TO_BOOLEAN(argv[1]);

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(check_name(sys->cfg, str, unique)));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_chkrealname(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      str;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (JSVAL_NULL_OR_VOID(argv[0])) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return JS_TRUE;
	}
	JSVALUE_TO_ASTRING(cx, argv[0], str, (LEN_ALIAS > LEN_NAME)?LEN_ALIAS + 2:LEN_NAME + 2, NULL);

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(check_realname(sys->cfg, str)));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_chkpassword(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      str;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (JSVAL_NULL_OR_VOID(argv[0])) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return JS_TRUE;
	}
	JSVALUE_TO_ASTRING(cx, argv[0], str, (LEN_ALIAS > LEN_NAME)?LEN_ALIAS + 2:LEN_NAME + 2, NULL);

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	bool result = check_pass(sys->cfg, str, /* user: */NULL, /* unique: */false, /* reason: */NULL);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(result));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_chkfname(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      fname = NULL;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return JS_TRUE;
	}
	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], fname, NULL);
	if (fname == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	bool result = !illegal_filename(fname)
	                  && allowed_filename(sys->cfg, fname)
	                  && !trashcan(sys->cfg, fname, "file");
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(result));
	JS_RESUMEREQUEST(cx, rc);
	free(fname);

	return JS_TRUE;
}

static JSBool
js_safest_fname(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      fname = NULL;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return JS_TRUE;
	}
	JSVALUE_TO_MSTRING(cx, argv[0], fname, NULL);
	if (fname == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	bool result = safest_filename(fname);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(result));
	JS_RESUMEREQUEST(cx, rc);
	free(fname);

	return JS_TRUE;
}

static JSBool
js_illegal_fname(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      fname = NULL;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return JS_TRUE;
	}
	JSVALUE_TO_MSTRING(cx, argv[0], fname, NULL);
	if (fname == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	bool result = illegal_filename(fname);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(result));
	JS_RESUMEREQUEST(cx, rc);
	free(fname);

	return JS_TRUE;
}

static JSBool
js_allowed_fname(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      fname = NULL;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return JS_TRUE;
	}
	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], fname, NULL);
	if (fname == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(allowed_filename(sys->cfg, fname)));
	JS_RESUMEREQUEST(cx, rc);
	free(fname);

	return JS_TRUE;
}

static JSBool
js_check_netmail_addr(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      addr = NULL;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return JS_TRUE;
	}
	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], addr, NULL);
	if (addr == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(netmail_addr_is_supported(sys->cfg, addr)));
	JS_RESUMEREQUEST(cx, rc);
	free(addr);

	return JS_TRUE;
}

static JSBool
js_check_birthdate(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      date = NULL;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return JS_TRUE;
	}
	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], date, NULL);
	if (date == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(birthdate_is_valid(sys->cfg, date)));
	JS_RESUMEREQUEST(cx, rc);
	free(date);

	return JS_TRUE;
}

#endif

static JSBool
js_chkpid(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	int32      pid = 0;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;
	JS_ValueToInt32(cx, argv[0], &pid);

	rc = JS_SUSPENDREQUEST(cx);
	bool result = check_pid(pid);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(result));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_killpid(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	int32      pid = 0;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;

	JS_ValueToInt32(cx, argv[0], &pid);

	rc = JS_SUSPENDREQUEST(cx);
	bool result = terminate_pid(pid);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(result));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_text(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	jsval*    argv = JS_ARGV(cx, arglist);
	uint32    i = 0;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;
	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
		return JS_FALSE;

	if (sys->cfg == NULL || sys->cfg->text == NULL)
		return JS_TRUE;

	if (JSVAL_IS_NUMBER(argv[0])) {
		if (!JS_ValueToECMAUint32(cx, argv[0], &i))
			return JS_FALSE;
	} else {
		JSString* js_str = JS_ValueToString(cx, argv[0]);
		if (js_str == NULL)
			return JS_FALSE;
		char*     id;
		JSSTRING_TO_MSTRING(cx, js_str, id, NULL);
		i = get_text_num(id) + 1; // Note: this is a non-caching look-up!
		free(id);
	}

	if (i > 0  && i <= TOTAL_TEXT) {
		JSString* js_str = JS_NewStringCopyZ(cx, sys->cfg->text[i - 1]);
		if (js_str == NULL)
			return JS_FALSE;
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	}
	return JS_TRUE;
}

static jsSyncMethodSpec js_system_functions[] = {
#ifndef JSDOOR
	{"username",        js_username,        1,  JSTYPE_STRING,  JSDOCSTR("user_number")
	 , JSDOCSTR("Return name of user in specified user record <i>number</i>, or empty string if not found")
	 , 311},
	{"alias",           js_alias,           1,  JSTYPE_STRING,  JSDOCSTR("alias")
	 , JSDOCSTR("Return name of user that matches alias (if found in <tt>ctrl/alias.cfg</tt>)")
	 , 310},
	{"find_login_id",   js_find_login_id,   1,  JSTYPE_NUMBER,  JSDOCSTR("user-id")
	 , JSDOCSTR("Find a user's login ID (alias, real name, or number), returns matching user record number or 0 if not found")
	 , 32000},
	{"matchuser",       js_matchuser,       1,  JSTYPE_NUMBER,  JSDOCSTR("username [,sysop_alias=true]")
	 , JSDOCSTR("Exact user name matching, returns number of user whose name/alias matches <i>username</i> "
		        " or 0 if not found, matches well-known sysop aliases by default")
	 , 310},
	{"matchuserdata",   js_matchuserdata,   2,  JSTYPE_NUMBER,  JSDOCSTR("field, data [,<i>bool</i> match_del=false] [,<i>number</i> usernumber, <i>bool</i> match_next=false]")
	 , JSDOCSTR("Search user database for data in a specific field (see <tt>U_*</tt> in <tt>sbbsdefs.js</tt>).<br>"
		        "If <i>match_del</i> is <tt>true</tt>, deleted user records are searched, "
		        "returns first matching user record number, optional <i>usernumber</i> specifies user record to skip, "
		        "or record at which to begin searching if optional <i>match_next</i> is <tt>true</tt>.")
	 , 310},
#endif
	{"trashcan",        js_trashcan,        2,  JSTYPE_BOOLEAN, JSDOCSTR("basename, find_string")
	 , JSDOCSTR("Search <tt>text/<i>basename</i>.can</tt> for pseudo-regexp")
	 , 310},
	{"findstr",         js_findstr,         2,  JSTYPE_BOOLEAN, JSDOCSTR("path/filename or <i>array</i> of strings, find_string")
	 , JSDOCSTR("Search any trashcan/filter file or array of pattern strings (in <tt>*.can</tt> format) for <i>find_string</i>")
	 , 310},
	{"zonestr",         js_zonestr,         0,  JSTYPE_STRING,  JSDOCSTR("[timezone=<i>local</i>]")
	 , JSDOCSTR("Convert time zone integer to string, defaults to system timezone if <i>timezone</i> not specified")
	 , 310},
	{"timestr",         js_timestr,         0,  JSTYPE_STRING,  JSDOCSTR("[time=<i>current</i>]")
	 , JSDOCSTR("Convert time_t integer into a time string, "
		        "defaults to current time if <i>time</i> not specified")
	 , 310},
	{"datestr",         js_datestr,         0,  JSTYPE_STRING,  JSDOCSTR("[time=<i>current</i>]")
	 , JSDOCSTR("Convert time_t integer into a short (8 character) date string, in either numeric or verbal format (depending on system preference), "
		        "defaults to current date if <i>time</i> not specified. "
		        "If <i>time</i> is a string in numeric date format, returns the parsed time_t value as a number.")
	 , 310},
	{"secondstr",       js_secondstr,       0,  JSTYPE_STRING,  JSDOCSTR("seconds [,<t>bool</t> verbose=true]")
	 , JSDOCSTR("Convert a duration in seconds into a string in <tt>hh:mm:ss</tt> format")
	 , 310},
	{"minutestr",       js_minutestr,       0,  JSTYPE_STRING,  JSDOCSTR("minutes [,<t>bool</t> estimate=false] [,<t>bool</t> words=false]")
	 , JSDOCSTR("Convert a duration in minutes into a string in <tt>'DDd HHh MMm'</tt>, <tt>'X.Yh'</tt> or <tt>'X.h hours'</tt> format")
	 , 321},
#ifndef JSDOOR
	{"spamlog",         js_spamlog,         6,  JSTYPE_BOOLEAN, JSDOCSTR("[protocol, action, reason, host, ip, to, from]")
	 , JSDOCSTR("Log a suspected SPAM attempt")
	 , 310},
	{"hacklog",         js_hacklog,         5,  JSTYPE_BOOLEAN, JSDOCSTR("[protocol, user, text, host, ip, port]")
	 , JSDOCSTR("Log a suspected hack attempt")
	 , 310},
	{"filter_ip",       js_filter_ip,       4,  JSTYPE_BOOLEAN, JSDOCSTR("[protocol, reason, host, ip, username, filename] [<i>number</i> duration-in-seconds]")
	 , JSDOCSTR("Add an IP address (with comment) to an IP filter file. If filename is not specified, the ip.can file is used")
	 , 311},
	{"get_node",        js_get_node,        1,  JSTYPE_OBJECT,  JSDOCSTR("node_number")
	 , JSDOCSTR("Read a node data record all at once (and leaving the record unlocked) "
		        "returning an object matching the elements of <tt>system.node_list</tt>")
	 , 31702},
	{"get_node_message", js_get_node_message, 0,  JSTYPE_STRING,  JSDOCSTR("node_number")
	 , JSDOCSTR("Read any messages waiting for the specified node and return in a single string or <i>null</i> if none are waiting")
	 , 311},
	{"put_node_message", js_put_node_message, 2,  JSTYPE_BOOLEAN, JSDOCSTR("node_number, message_text")
	 , JSDOCSTR("Send a node a short text message, delivered immediately")
	 , 310},
	{"get_telegram",    js_get_telegram,    1,  JSTYPE_STRING,  JSDOCSTR("user_number")
	 , JSDOCSTR("Return any short text messages waiting for the specified user or <i>null</i> if none are waiting")
	 , 311},
	{"put_telegram",    js_put_telegram,    2,  JSTYPE_BOOLEAN, JSDOCSTR("user_number, message_text")
	 , JSDOCSTR("Send a user a short text message, delivered immediately or during next logon")
	 , 310},
	{"notify",          js_notify,          2,  JSTYPE_BOOLEAN, JSDOCSTR("user_number, subject [,message_text] [,reply_to_address]")
	 , JSDOCSTR("Notify a user or operator via both email and a short text message about an important event")
	 , 31801},
	{"newuser",         js_new_user,        1,  JSTYPE_ALIAS },
	{"new_user",        js_new_user,        1,  JSTYPE_OBJECT,  JSDOCSTR("name/alias [,client object]")
	 , JSDOCSTR("Create a new user record, returns a new <a href=#User_class>User object</a> representing the new user account, on success.<br>"
		        "returns a numeric error code on failure")
	 , 310},
	{"del_user",        js_del_user,        1,  JSTYPE_BOOLEAN, JSDOCSTR("user_number")
	 , JSDOCSTR("Delete the specified user account")
	 , 316},
	{"undel_user",      js_undel_user,      1,  JSTYPE_BOOLEAN, JSDOCSTR("user_number")
	 , JSDOCSTR("Un-delete the specified user account")
	 , 321},
#endif
	{"exec",            js_sys_exec,        0,  JSTYPE_NUMBER,  JSDOCSTR("command-line")
	 , JSDOCSTR("Execute a native system/shell command-line, returns <i>0</i> on success")
	 , 311},
	{"popen",           js_popen,           0,  JSTYPE_ARRAY,   JSDOCSTR("command-line")
	 , JSDOCSTR("Execute a native system/shell command-line, returns array of captured output lines on success "
		        "(<b>only functional on UNIX systems</b>)")
	 , 311},
#ifndef JSDOOR
	{"check_syspass",   js_chksyspass,      1,  JSTYPE_BOOLEAN, JSDOCSTR("password")
	 , JSDOCSTR("Compare the supplied <i>password</i> against the system password and returns <tt>true</tt> if it matches")
	 , 311},
	{"check_name",      js_chkname,         1,  JSTYPE_BOOLEAN, JSDOCSTR("name/alias [,<i>bool</i> unique=true]")
	 , JSDOCSTR("Check that the provided name/alias string is suitable for a new user account, "
		        "returns <tt>true</tt> if it is valid and (optionally) if it is unique")
	 , 315},
	{"check_realname",  js_chkrealname,     1,  JSTYPE_BOOLEAN, JSDOCSTR("name")
	 , JSDOCSTR("Check that the provided real user name string is suitable for a new user account, "
		        "returns <tt>true</tt> if it is valid")
	 , 321},
	{"check_password",  js_chkpassword,     1,  JSTYPE_BOOLEAN, JSDOCSTR("password")
	 , JSDOCSTR("Check that the provided string is suitable for a new user password, "
		        "returns <tt>true</tt> if it meets the system criteria for a user password.<br>"
				"Does <b>not</b> check the <tt>password.can</tt> file.")
	 , 321},
	{"check_netmail_addr", js_check_netmail_addr, 1, JSTYPE_BOOLEAN, JSDOCSTR("address")
	 , JSDOCSTR("Check if the specified <i>address</i> is in a supported NetMail address format, "
				"returns <tt>true</tt> if it is supported")
	 , 321},
	{"check_birthdate", js_check_birthdate, 1, JSTYPE_BOOLEAN, JSDOCSTR("string")
	 , JSDOCSTR("Check if the specified <i>string</i> is a valid birth date, "
				"returns <tt>true</tt> if it is valid")
	 , 321},
	{"check_filename",  js_chkfname,        1,  JSTYPE_BOOLEAN, JSDOCSTR("filename")
	 , JSDOCSTR("Verify that the specified <i>filename</i> string is legal and allowed for upload by users "
		        "(based on system configuration and filter files), returns <tt>true</tt> if the filename is allowed")
	 , 31902},
	{"allowed_filename", js_allowed_fname,  1,  JSTYPE_BOOLEAN, JSDOCSTR("filename")
	 , JSDOCSTR("Verify that the specified <i>filename</i> string is allowed for upload by users "
		        "(based on system configuration), returns <tt>true</tt> if the filename is allowed")
	 , 31902},
	{"safest_filename", js_safest_fname,    1,  JSTYPE_BOOLEAN, JSDOCSTR("filename")
	 , JSDOCSTR("Verify that the specified <i>filename</i> string contains only the safest subset of characters")
	 , 31902},
	{"illegal_filename", js_illegal_fname,  1,  JSTYPE_BOOLEAN, JSDOCSTR("filename")
	 , JSDOCSTR("Check if the specified <i>filename</i> string contains illegal characters or sequences, "
		        "returns <tt>true</tt> if it is an illegal filename")
	 , 31902},
#endif
	{"check_pid",       js_chkpid,          1,  JSTYPE_BOOLEAN, JSDOCSTR("process-ID")
	 , JSDOCSTR("Check that the provided process ID is a valid executing process on the system, "
		        "returns <tt>true</tt> if it is valid")
	 , 315},
	{"terminate_pid",   js_killpid,         1,  JSTYPE_BOOLEAN, JSDOCSTR("process-ID")
	 , JSDOCSTR("Terminate executing process on the system with the specified process ID, "
		        "returns <tt>true</tt> on success")
	 , 315},
	{"text",            js_text,            1,  JSTYPE_STRING,  JSDOCSTR("<i>number</i> index or <i>string</i> id")
	 , JSDOCSTR("Return specified text string (see <tt>bbs.text()</tt> for details) or <tt>null</tt> if invalid <i>index</i> or <i>id</i> specified.<br>"
		        "The <i>string id</i> support was added in v3.20.")
	 , 31802},
	{0}
};


/* node properties */
enum {
	/* raw node_t fields */
	NODE_PROP_STATUS
	, NODE_PROP_VSTATUS
	, NODE_PROP_ERRORS
	, NODE_PROP_ACTION
	, NODE_PROP_ACTIVITY
	, NODE_PROP_USERON
	, NODE_PROP_CONNECTION
	, NODE_PROP_MISC
	, NODE_PROP_AUX
	, NODE_PROP_EXTAUX
	, NODE_PROP_DIR
};

#ifdef BUILD_JSDOCS
static const char* node_prop_desc[] = {
	"Status (see <tt>nodedefs.js</tt> for valid values)"
	, "Verbal status - <small>READ ONLY</small>"
	, "Error counter"
	, "Current user action (see <tt>nodedefs.js</tt>)"
	, "Current user activity - <small>READ ONLY</small>y"
	, "Current user number"
	, "Connection speed (<tt>0xffff</tt> = Telnet or RLogin)"
	, "Miscellaneous bit-flags (see <tt>nodedefs.js</tt>)"
	, "Auxiliary value"
	, "Extended auxiliary value"
	, "Node directory - <small>READ ONLY</small>"
	, NULL
};
#endif

static JSBool js_node_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	char       tmp[128];
	jsval      idval;
	uint       node_num;
	jsint      tiny;
	node_t     node;
	JSObject*  sysobj;
	JSObject*  node_list;
	jsrefcount rc;
	JSString*  js_str;

	JS_IdToValue(cx, id, &idval);
	tiny = JSVAL_TO_INT(idval);

	if ((node_list = JS_GetParent(cx, obj)) == NULL)
		return JS_FALSE;

	if ((sysobj = JS_GetParent(cx, node_list)) == NULL)
		return JS_FALSE;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, sysobj, &js_system_class)) == NULL)
		return JS_FALSE;

	node_num = (uintptr_t)JS_GetPrivate(cx, obj) >> 1;

	rc = JS_SUSPENDREQUEST(cx);
	memset(&node, 0, sizeof(node));
	if (getnodedat(sys->cfg, node_num, &node, /* lockit: */ FALSE, &sys->nodefile) != 0) {
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}
	JS_RESUMEREQUEST(cx, rc);
	sys->nodegets++;

	switch (tiny) {
		case NODE_PROP_STATUS:
			*vp = INT_TO_JSVAL((int)node.status);
			break;
		case NODE_PROP_VSTATUS:
			if ((js_str = JS_NewStringCopyZ(cx, node_vstatus(sys->cfg, &node, tmp, sizeof tmp))) == NULL)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str);
			break;
		case NODE_PROP_ERRORS:
			*vp = INT_TO_JSVAL((int)node.errors);
			break;
		case NODE_PROP_ACTION:
			*vp = INT_TO_JSVAL((int)node.action);
			break;
		case NODE_PROP_ACTIVITY:
			if ((js_str = JS_NewStringCopyZ(cx, node_activity(sys->cfg, &node, tmp, sizeof tmp, node_num))) == NULL)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str);
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
			*vp = UINT_TO_JSVAL(node.extaux);
			break;
		case NODE_PROP_DIR:
			if ((js_str = JS_NewStringCopyZ(cx, sys->cfg->node_path[node_num - 1])) == NULL)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str);
			break;
	}
	return JS_TRUE;
}

static JSBool js_node_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsval      idval;
	uint       node_num;
	jsint      val = 0;
	jsint      tiny;
	node_t     node;
	JSObject*  sysobj;
	JSObject*  node_list;
	jsrefcount rc;

	if ((node_list = JS_GetParent(cx, obj)) == NULL)
		return JS_FALSE;

	if ((sysobj = JS_GetParent(cx, node_list)) == NULL)
		return JS_FALSE;

	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, sysobj, &js_system_class)) == NULL)
		return JS_FALSE;

	node_num = (uintptr_t)JS_GetPrivate(cx, obj) >> 1;

	rc = JS_SUSPENDREQUEST(cx);
	memset(&node, 0, sizeof(node));
	if (getnodedat(sys->cfg, node_num, &node, /* lockit: */ TRUE, &sys->nodefile)) {
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}

	JS_RESUMEREQUEST(cx, rc);
	if (JSVAL_IS_NUMBER(*vp))
		JS_ValueToInt32(cx, *vp, &val);

	JS_IdToValue(cx, id, &idval);
	tiny = JSVAL_TO_INT(idval);
	rc = JS_SUSPENDREQUEST(cx);

	switch (tiny) {
		case NODE_PROP_STATUS:
			node.status = (BYTE)val;
			break;
		case NODE_PROP_ERRORS:
			node.errors = (BYTE)val;
			break;
		case NODE_PROP_ACTION:
			node.action = (BYTE)val;
			break;
		case NODE_PROP_USERON:
			node.useron = (WORD)val;
			break;
		case NODE_PROP_CONNECTION:
			node.connection = (WORD)val;
			break;
		case NODE_PROP_MISC:
			node.misc = (WORD)val;
			break;
		case NODE_PROP_AUX:
			node.aux = (WORD)val;
			break;
		case NODE_PROP_EXTAUX:
			node.extaux = val;
			break;
	}
	putnodedat(sys->cfg, node_num, &node, /* closeit: */ FALSE, sys->nodefile);
	mqtt_putnodedat(sys->mqtt, node_num, &node);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static jsSyncPropertySpec js_node_properties[] = {
/*		 name,						tinyid,					flags,								ver	*/

/* raw node_t fields */
	{   "status",                   NODE_PROP_STATUS,       JSPROP_ENUMERATE,                   310 },
	{   "vstatus",                  NODE_PROP_VSTATUS,      JSPROP_ENUMERATE | JSPROP_READONLY,   320 },
	{   "errors",                   NODE_PROP_ERRORS,       JSPROP_ENUMERATE,                   310 },
	{   "action",                   NODE_PROP_ACTION,       JSPROP_ENUMERATE,                   310 },
	{   "activity",                 NODE_PROP_ACTIVITY,     JSPROP_ENUMERATE | JSPROP_READONLY,   320 },
	{   "useron",                   NODE_PROP_USERON,       JSPROP_ENUMERATE,                   310 },
	{   "connection",               NODE_PROP_CONNECTION,   JSPROP_ENUMERATE,                   310 },
	{   "misc",                     NODE_PROP_MISC,         JSPROP_ENUMERATE,                   310 },
	{   "aux",                      NODE_PROP_AUX,          JSPROP_ENUMERATE,                   310 },
	{   "extaux",                   NODE_PROP_EXTAUX,       JSPROP_ENUMERATE,                   310 },
	{   "dir",                      NODE_PROP_DIR,          JSPROP_ENUMERATE | JSPROP_READONLY,   315 },
	{0}
};

static JSBool js_node_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*  name = NULL;
	JSBool ret;

	if (id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;

		JS_IdToValue(cx, id, &idval);
		if (JSVAL_IS_STRING(idval)) {
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
			HANDLE_PENDING(cx, name);
		}
	}

	ret = js_SyncResolve(cx, obj, name, js_node_properties, NULL, NULL, 0);
	if (name)
		free(name);
	return ret;
}

static JSBool js_node_enumerate(JSContext *cx, JSObject *obj)
{
	return js_node_resolve(cx, obj, JSID_VOID);
}

static JSClass js_node_class = {
	"Node"                  /* name			*/
	, JSCLASS_HAS_PRIVATE    /* flags		*/
	, JS_PropertyStub        /* addProperty	*/
	, JS_PropertyStub        /* delProperty	*/
	, js_node_get            /* getProperty	*/
	, js_node_set            /* setProperty	*/
	, js_node_enumerate      /* enumerate	*/
	, js_node_resolve        /* resolve		*/
	, JS_ConvertStub         /* convert		*/
	, JS_FinalizeStub        /* finalize		*/
};

#define LAZY_INTEGER(PropName, PropValue) \
		if (name == NULL || strcmp(name, (PropName)) == 0) { \
			val = UINT_TO_JSVAL((PropValue)); \
			JS_DefineProperty(cx, obj, (PropName), val, NULL, NULL, JSPROP_ENUMERATE); \
			if (name) { \
				free(name); \
				return JS_TRUE; \
			} \
		}

#define LAZY_STRING(PropName, PropValue) \
		if (name == NULL || strcmp(name, (PropName)) == 0) { \
			if ((js_str = JS_NewStringCopyZ(cx, (PropValue))) != NULL) { \
				JS_DefineProperty(cx, obj, PropName, STRING_TO_JSVAL(js_str), NULL, NULL, JSPROP_ENUMERATE); \
				if (name) { \
					free(name); \
					return JS_TRUE; \
				} \
			} \
			else if (name) { \
				free(name); \
				return JS_TRUE; \
			} \
		}

#define LAZY_STRFUNC(PropName, Function, PropValue) \
		if (name == NULL || strcmp(name, (PropName)) == 0) { \
			Function; \
			if ((js_str = JS_NewStringCopyZ(cx, (PropValue))) != NULL) { \
				JS_DefineProperty(cx, obj, PropName, STRING_TO_JSVAL(js_str), NULL, NULL, JSPROP_ENUMERATE); \
				if (name) { \
					free(name); \
					return JS_TRUE; \
				} \
			} \
			else if (name) { \
				free(name); \
				return JS_TRUE; \
			} \
		}

#define LAZY_STRFUNC_TRUNCSP(PropName, Function, PropValue) \
		if (name == NULL || strcmp(name, (PropName)) == 0) { \
			Function; \
			if ((js_str = JS_NewStringCopyZ(cx, truncsp(PropValue))) != NULL) { \
				JS_DefineProperty(cx, obj, PropName, STRING_TO_JSVAL(js_str), NULL, NULL, JSPROP_ENUMERATE); \
				if (name) { \
					free(name); \
					return JS_TRUE; \
				} \
			} \
			else if (name) { \
				free(name); \
				return JS_TRUE; \
			} \
		}

static JSBool js_system_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*     name = NULL;
	jsval     val;
	char      str[256];
	JSString* js_str;
	JSBool    ret;
#ifndef JSDOOR
	JSObject* newobj;
	JSObject* nodeobj;
	int       i;
#endif

	if (id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;

		JS_IdToValue(cx, id, &idval);
		if (JSVAL_IS_STRING(idval)) {
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
			HANDLE_PENDING(cx, name);
		}
	}

	/****************************/
	/* static string properties */
	LAZY_STRING("version", VERSION);
	LAZY_STRFUNC("revision", sprintf(str, "%c", REVISION), str);
	LAZY_STRFUNC_TRUNCSP("beta_version", SAFECOPY(str, beta_version), str);

	if (name == NULL || strcmp(name, "full_version") == 0) {
		sprintf(str, "%s%c%s", VERSION, REVISION, beta_version);
		truncsp(str);
#if defined(_DEBUG)
		strcat(str, " Debug");
#endif
		if (name)
			free(name);
		if ((js_str = JS_NewStringCopyZ(cx, str)) != NULL) {
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, obj, "full_version", &val);
			if (name)
				return JS_TRUE;
		}
		else if (name)
			return JS_TRUE;
	}

	LAZY_STRING("version_notice", VERSION_NOTICE);

	/* Numeric version properties */
	LAZY_INTEGER("version_num", VERSION_NUM);
	LAZY_INTEGER("version_hex", VERSION_HEX);

	/* Git repo details */
	LAZY_STRING("git_branch", git_branch);
	LAZY_STRING("git_hash", git_hash);
	LAZY_STRING("git_date", git_date);
	LAZY_INTEGER("git_time", (uint32_t)git_time);

	LAZY_STRING("platform", PLATFORM_DESC);
	LAZY_STRING("architecture", ARCHITECTURE_DESC);
	LAZY_STRFUNC("msgbase_lib", sprintf(str, "SMBLIB %s", smb_lib_ver()), str);
	LAZY_STRFUNC("compiled_with", DESCRIBE_COMPILER(str), str);
	LAZY_STRFUNC("compiled_when", sprintf(str, "%s %.5s", __DATE__, __TIME__), str);
	LAZY_STRING("copyright", COPYRIGHT_NOTICE);
	LAZY_STRING("js_version", (char *)JS_GetImplementationVersion());
	LAZY_STRING("os_version", os_version(str, sizeof(str)));

#ifndef JSDOOR
	/* fido_addr_list property */
	if (name == NULL || strcmp(name, "fido_addr_list") == 0) {
		if (name)
			free(name);

		js_system_private_t* sys;
		if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
			return JS_FALSE;

		if ((newobj = JS_NewArrayObject(cx, 0, NULL)) == NULL)
			return JS_FALSE;

		if (!JS_SetParent(cx, newobj, obj))
			return JS_FALSE;

		if (!JS_DefineProperty(cx, obj, "fido_addr_list", OBJECT_TO_JSVAL(newobj)
		                       , NULL, NULL, JSPROP_ENUMERATE))
			return JS_FALSE;

		for (i = 0; i < sys->cfg->total_faddrs; i++) {
			val = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, smb_faddrtoa(&sys->cfg->faddr[i], str)));
			JS_SetElement(cx, newobj, i, &val);
		}
		if (name)
			return JS_TRUE;
	}

	if (name == NULL || strcmp(name, "stats") == 0) {
		if (name)
			free(name);

		js_system_private_t* sys;
		if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
			return JS_FALSE;

		newobj = JS_DefineObject(cx, obj, "stats", &js_sysstats_class, NULL
		                         , JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY);

		if (newobj == NULL)
			return JS_FALSE;

		JS_SetPrivate(cx, newobj, sys);
#ifdef BUILD_JSDOCS
		js_DescribeSyncObject(cx, newobj, "System statistics", 310);
		js_CreateArrayOfStrings(cx, newobj, "_property_desc_list", sysstat_prop_desc, JSPROP_READONLY);
#endif
		if (name)
			return JS_TRUE;
	}

	/* node_list property */
	if (name == NULL || strcmp(name, "node_list") == 0) {
		if (name)
			free(name);

		js_system_private_t* sys;
		if ((sys = (js_system_private_t*)js_GetClassPrivate(cx, obj, &js_system_class)) == NULL)
			return JS_FALSE;

		if ((newobj = JS_NewArrayObject(cx, 0, NULL)) == NULL)
			return JS_FALSE;

		if (!JS_SetParent(cx, newobj, obj))
			return JS_FALSE;

		if (!JS_DefineProperty(cx, obj, "node_list", OBJECT_TO_JSVAL(newobj)
		                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_PERMANENT))
			return JS_FALSE;

		for (i = 0; i < sys->cfg->sys_nodes && i < sys->cfg->sys_lastnode; i++) {

			nodeobj = JS_NewObject(cx, &js_node_class, NULL, newobj);

			if (nodeobj == NULL)
				return JS_FALSE;

			/* Store node number */
			/* We have to shift it to make it look like a pointer to JS. :-( */
			if (!JS_SetPrivate(cx, nodeobj, (char*)(((uintptr_t)i + 1) << 1)))
				return JS_FALSE;

	#ifdef BUILD_JSDOCS
			if (i == 0) {
				js_DescribeSyncObject(cx, nodeobj, "Terminal Server node listing", 310);
				js_CreateArrayOfStrings(cx, nodeobj, "_property_desc_list", node_prop_desc, JSPROP_READONLY);
			}
	#endif

			val = OBJECT_TO_JSVAL(nodeobj);
			if (!JS_SetElement(cx, newobj, i, &val))
				return JS_FALSE;
		}
		if (name)
			return JS_TRUE;
	}
#endif

	ret = js_SyncResolve(cx, obj, name, js_system_properties, js_system_functions, NULL, 0);
	if (name)
		free(name);
	return ret;
}

static JSBool js_system_enumerate(JSContext *cx, JSObject *obj)
{
	return js_system_resolve(cx, obj, JSID_VOID);
}

static void js_system_finalize(JSContext *cx, JSObject *obj)
{
	js_system_private_t* sys;
	if ((sys = (js_system_private_t*)JS_GetPrivate(cx, obj)) == NULL)
		return;

	CLOSE_OPEN_FILE(sys->nodefile);
	free(sys);
	JS_SetPrivate(cx, obj, NULL);
}

JSBool js_CreateTextProperties(JSContext* cx, JSObject* parent)
{
	jsval val;

	if (!JS_GetProperty(cx, parent, "text", &val))
		return JS_FALSE;

	JSObject* text = JSVAL_TO_OBJECT(val);
	if (text == NULL)
		return JS_FALSE;
	for (int i = 0; i < TOTAL_TEXT; ++i) {
		val = INT_TO_JSVAL(i + 1);
		if (!JS_SetProperty(cx, text, text_id[i], &val))
			return JS_FALSE;
	}
	return JS_TRUE;
}

JSClass js_system_class = {
	"System"                /* name			*/
	, JSCLASS_HAS_PRIVATE    /* flags		*/
	, JS_PropertyStub        /* addProperty	*/
	, JS_PropertyStub        /* delProperty	*/
	, js_system_get          /* getProperty	*/
	, js_system_set          /* setProperty	*/
	, js_system_enumerate    /* enumerate	*/
	, js_system_resolve      /* resolve		*/
	, JS_ConvertStub         /* convert		*/
	, js_system_finalize     /* finalize		*/
};

JSObject* js_CreateSystemObject(JSContext* cx, JSObject* parent
                                , scfg_t* cfg, time_t uptime, const char* host_name, const char* socklib_desc, struct mqtt* mqtt)
{
	jsval     val;
	JSObject* sysobj;
	JSString* js_str;
	char      str[256];

	sysobj = JS_DefineObject(cx, parent, "system", &js_system_class, NULL
	                         , JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
	if (sysobj == NULL)
		return NULL;

	js_system_private_t* sys;
	if ((sys = static_cast<js_system_private_t *>(calloc(sizeof(*sys), 1))) == NULL)
		return NULL;

	sys->cfg = cfg;
	sys->mqtt = mqtt;
	sys->nodefile = -1;

	if (!JS_SetPrivate(cx, sysobj, sys))
		return NULL;

	/****************************/
	/* static string properties */
#ifndef JSDOOR
	if ((js_str = JS_NewStringCopyZ(cx, host_name)) == NULL)
		return NULL;
	val = STRING_TO_JSVAL(js_str);
	if (!JS_SetProperty(cx, sysobj, "host_name", &val))
		return NULL;
#endif

	if ((js_str = JS_NewStringCopyZ(cx, socklib_version(str, sizeof str, socklib_desc))) == NULL)
		return NULL;
	val = STRING_TO_JSVAL(js_str);
	if (!JS_SetProperty(cx, sysobj, "socket_lib", &val))
		return NULL;

	/***********************/

	val = DOUBLE_TO_JSVAL((double)uptime);
	if (!JS_SetProperty(cx, sysobj, "uptime", &val))
		return NULL;

	js_CreateTextProperties(cx, sysobj);

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, sysobj, "Global system-related properties and methods", 310);
	js_CreateArrayOfStrings(cx, sysobj, "_property_desc_list", sys_prop_desc, JSPROP_READONLY);
#endif

	return sysobj;
}

#endif  /* JAVSCRIPT */
