/* sysobj.c */

/* Synchronet JavaScript "System" Object */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2001 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#ifdef JAVASCRIPT

/* System Object Properites */
enum {
	 SYS_PROP_NAME
	,SYS_PROP_OP
	,SYS_PROP_ID
	,SYS_PROP_MISC
	,SYS_PROP_PSNAME
	,SYS_PROP_PSNUM
	,SYS_PROP_INETADDR
	,SYS_PROP_LOCATION
	,SYS_PROP_TIMEZONE
	,SYS_PROP_NODES
	,SYS_PROP_LASTNODE
	,SYS_PROP_PWDAYS
	,SYS_PROP_DELDAYS

	,SYS_PROP_LASTUSERON
	,SYS_PROP_FREEDISKSPACE
};

static JSBool js_system_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint       tiny;
	scfg_t*		cfg;

	if((cfg=(scfg_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_FALSE;

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case SYS_PROP_NAME:
	        *vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, cfg->sys_name));
			break;
		case SYS_PROP_OP:
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, cfg->sys_op));
			break;
		case SYS_PROP_ID:
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, cfg->sys_id));
			break;
		case SYS_PROP_MISC:
			*vp = INT_TO_JSVAL(cfg->sys_misc);
			break;
		case SYS_PROP_PSNAME:
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, cfg->sys_psname));
			break;
		case SYS_PROP_PSNUM:
			*vp = INT_TO_JSVAL(cfg->sys_psnum);
			break;
		case SYS_PROP_INETADDR:
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, cfg->sys_inetaddr));
			break;
		case SYS_PROP_LOCATION:
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, cfg->sys_location));
			break;
		case SYS_PROP_TIMEZONE:
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
		case SYS_PROP_DELDAYS:
			*vp = INT_TO_JSVAL(cfg->sys_deldays);
			break;

		case SYS_PROP_LASTUSERON:
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, lastuseron));
			break;
		case SYS_PROP_FREEDISKSPACE:
			*vp = INT_TO_JSVAL(getfreediskspace(cfg->temp_dir));
			break;
	}

	return(TRUE);
}

static JSBool js_system_set(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint       tiny;
	scfg_t*		cfg;

	if((cfg=(scfg_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_FALSE;

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case SYS_PROP_MISC:
			JS_ValueToInt32(cx, *vp, &cfg->sys_misc);
			break;
	}

	return(TRUE);
}


#define SYSOBJ_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static struct JSPropertySpec js_system_properties[] = {
/*		 name,		tinyid,				flags,				getter,	setter	*/
	{	"name",		SYS_PROP_NAME,		SYSOBJ_FLAGS,		NULL,	NULL },
	{	"operator",	SYS_PROP_OP,		SYSOBJ_FLAGS,		NULL,	NULL },
	{	"qwk_id",	SYS_PROP_ID,		SYSOBJ_FLAGS,		NULL,	NULL },
	{	"misc",		SYS_PROP_MISC,		JSPROP_ENUMERATE,	NULL,	NULL },
	{	"status",	SYS_PROP_MISC,		JSPROP_ENUMERATE,	NULL,	NULL },
	{	"psname",	SYS_PROP_PSNAME,	SYSOBJ_FLAGS,		NULL,	NULL },
	{	"psnum",	SYS_PROP_PSNUM,		SYSOBJ_FLAGS,		NULL,	NULL },
	{	"inetaddr",	SYS_PROP_INETADDR,	SYSOBJ_FLAGS,		NULL,	NULL },
	{	"location",	SYS_PROP_LOCATION,	SYSOBJ_FLAGS,		NULL,	NULL },
	{	"timezone",	SYS_PROP_TIMEZONE,	SYSOBJ_FLAGS,		NULL,	NULL },
	{	"nodes",	SYS_PROP_NODES,		SYSOBJ_FLAGS,		NULL,	NULL },
	{	"lastnode",	SYS_PROP_LASTNODE,	SYSOBJ_FLAGS,		NULL,	NULL },
	{	"pwdays",	SYS_PROP_PWDAYS,	SYSOBJ_FLAGS,		NULL,	NULL },
	{	"deldays",	SYS_PROP_DELDAYS,	SYSOBJ_FLAGS,		NULL,	NULL },
	{	"freediskspace", SYS_PROP_FREEDISKSPACE,	SYSOBJ_FLAGS,	NULL,	NULL },
	{0}
};

static JSClass js_system_class = {
     "System"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_system_get			/* getProperty	*/
	,js_system_set			/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};

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

};

static JSBool js_sysstats_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint       tiny;
	scfg_t*		cfg;
	stats_t		stats;
	uint		i;
	ulong		l;

	if((cfg=(scfg_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_FALSE;

    tiny = JSVAL_TO_INT(id);

	if(!getstats(cfg, 0, &stats))
		return(FALSE);

	switch(tiny) {
		case SYSSTAT_PROP_LOGONS:
			*vp = INT_TO_JSVAL(stats.logons);
			break;
		case SYSSTAT_PROP_LTODAY:
			*vp = INT_TO_JSVAL(stats.ltoday);
			break;
		case SYSSTAT_PROP_TIMEON:
			*vp = INT_TO_JSVAL(stats.timeon);
			break;
		case SYSSTAT_PROP_TTODAY:
			*vp = INT_TO_JSVAL(stats.ttoday);
			break;
		case SYSSTAT_PROP_ULS:
			*vp = INT_TO_JSVAL(stats.uls);
			break;
		case SYSSTAT_PROP_ULB:
			*vp = INT_TO_JSVAL(stats.ulb);
			break;
		case SYSSTAT_PROP_DLS:
			*vp = INT_TO_JSVAL(stats.dls);
			break;
		case SYSSTAT_PROP_DLB:
			*vp = INT_TO_JSVAL(stats.dlb);
			break;
		case SYSSTAT_PROP_PTODAY:
			*vp = INT_TO_JSVAL(stats.ptoday);
			break;
		case SYSSTAT_PROP_ETODAY:
			*vp = INT_TO_JSVAL(stats.etoday);
			break;
		case SYSSTAT_PROP_FTODAY:
			*vp = INT_TO_JSVAL(stats.ftoday);
			break;
		case SYSSTAT_PROP_NUSERS:
			*vp = INT_TO_JSVAL(stats.nusers);
			break;

		case SYSSTAT_PROP_TOTALUSERS:
			*vp = INT_TO_JSVAL(lastuser(cfg));
			break;
		case SYSSTAT_PROP_TOTALMSGS:
			l=0;
			for(i=0;i<cfg->total_subs;i++)
				l+=getposts(cfg,i); 
			*vp = INT_TO_JSVAL(l); 
			break;
		case SYSSTAT_PROP_TOTALFILES:
			l=0;
			for(i=0;i<cfg->total_dirs;i++)
				l+=getfiles(cfg,i);
			*vp = INT_TO_JSVAL(l);
			break;
		case SYSSTAT_PROP_TOTALMAIL:
			*vp = INT_TO_JSVAL(getmail(cfg, 0,0));
			break;
		case SYSSTAT_PROP_FEEDBACK:
			*vp = INT_TO_JSVAL(getmail(cfg, 1,0));
			break;
	}

	return(TRUE);
}

#define SYSSTAT_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static struct JSPropertySpec js_sysstats_properties[] = {
/*		 name,						tinyid,						flags,			getter,	setter	*/
	{	"total_logons",				SYSSTAT_PROP_LOGONS,		SYSSTAT_FLAGS,	NULL,	NULL },
	{	"logons_today",				SYSSTAT_PROP_LTODAY,		SYSSTAT_FLAGS,	NULL,	NULL },
	{	"total_timeon",				SYSSTAT_PROP_TIMEON,		SYSSTAT_FLAGS,	NULL,	NULL },
	{	"timeon_today",				SYSSTAT_PROP_TTODAY,		SYSSTAT_FLAGS,	NULL,	NULL },
	{	"total_files",				SYSSTAT_PROP_TOTALFILES,	SYSSTAT_FLAGS,	NULL,	NULL },
	{	"files_uploaded_today",		SYSSTAT_PROP_ULS,			SYSSTAT_FLAGS,	NULL,	NULL },
	{	"bytes_uploaded_today",		SYSSTAT_PROP_ULB,			SYSSTAT_FLAGS,	NULL,	NULL },
	{	"files_downloaded_today",	SYSSTAT_PROP_DLS,			SYSSTAT_FLAGS,	NULL,	NULL },
	{	"bytes_downloaded_today",	SYSSTAT_PROP_DLB,			SYSSTAT_FLAGS,	NULL,	NULL },
	{	"total_messages",			SYSSTAT_PROP_TOTALMSGS,		SYSSTAT_FLAGS,	NULL,	NULL },
	{	"messages_posted_today",	SYSSTAT_PROP_PTODAY,		SYSSTAT_FLAGS,	NULL,	NULL },
	{	"total_email",				SYSSTAT_PROP_TOTALMAIL,		SYSSTAT_FLAGS,	NULL,	NULL },
	{	"email_sent_today",			SYSSTAT_PROP_ETODAY,		SYSSTAT_FLAGS,	NULL,	NULL },
	{	"total_feedback",			SYSSTAT_PROP_FEEDBACK,		SYSSTAT_FLAGS,	NULL,	NULL },
	{	"feedback_sent_today",		SYSSTAT_PROP_FTODAY,		SYSSTAT_FLAGS,	NULL,	NULL },
	{	"total_users",				SYSSTAT_PROP_TOTALUSERS,	SYSSTAT_FLAGS,	NULL,	NULL },
	{	"new_users_today",			SYSSTAT_PROP_NUSERS,		SYSSTAT_FLAGS,	NULL,	NULL },
	{0}
};

static JSClass js_sysstats_class = {
     "Stats"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_sysstats_get		/* getProperty	*/
	,JS_PropertyStub		/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};


JSObject* DLLCALL CreateSystemObject(scfg_t* cfg, JSContext* cx, JSObject* parent)
{
	JSObject*	sysobj;
	JSObject*	statsobj;

	sysobj = JS_DefineObject(cx, parent, "system", &js_system_class, NULL, 0);

	if(sysobj==NULL)
		return(NULL);

	JS_SetPrivate(cx, sysobj, cfg);	/* Store a pointer to scfg_t */

	JS_DefineProperties(cx, sysobj, js_system_properties);

	statsobj = JS_DefineObject(cx, sysobj, "stats", &js_sysstats_class, NULL, 0);

	if(statsobj==NULL)
		return(NULL);

	JS_SetPrivate(cx, statsobj, cfg);	/* Store a pointer to scfg_t */

	JS_DefineProperties(cx, statsobj, js_sysstats_properties);

	return(sysobj);
}

#endif	/* JAVSCRIPT */