/* js_bbs.cpp */

/* Synchronet JavaScript "bbs" Object */

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

/*****************************/
/* BBS Object Properites */
/*****************************/
enum {
	 BBS_PROP_SYS_STATUS
	,BBS_PROP_STARTUP_OPT
	,BBS_PROP_ANSWER_TIME
	,BBS_PROP_LOGON_TIME
	,BBS_PROP_NS_TIME
	,BBS_PROP_LAST_NS_TIME
	,BBS_PROP_ONLINE
	,BBS_PROP_TIMELEFT

	,BBS_PROP_NODE_NUM
	,BBS_PROP_NODE_MISC
	,BBS_PROP_NODE_VAL_USER

	,BBS_PROP_LOGON_ULB
	,BBS_PROP_LOGON_DLB
	,BBS_PROP_LOGON_ULS
	,BBS_PROP_LOGON_DLS
	,BBS_PROP_LOGON_POSTS
	,BBS_PROP_LOGON_EMAILS
	,BBS_PROP_LOGON_FBACKS
	,BBS_PROP_POSTS_READ

	,BBS_PROP_MENU_DIR
	,BBS_PROP_MENU_FILE
	,BBS_PROP_MAIN_CMDS
	,BBS_PROP_XFER_CMDS

	,BBS_PROP_CONNECTION		/* READ ONLY */
	,BBS_PROP_RLOGIN_NAME
	,BBS_PROP_CLIENT_NAME
};

static JSBool js_bbs_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	char*		p=NULL;
	ulong		val=0;
    jsint       tiny;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case BBS_PROP_SYS_STATUS:
			val=sbbs->sys_status;
			break;
		case BBS_PROP_STARTUP_OPT:
			val=sbbs->startup->options;
			break;
		case BBS_PROP_ANSWER_TIME:
			val=sbbs->answertime;
			break;
		case BBS_PROP_LOGON_TIME:
			val=sbbs->logontime;
			break;
		case BBS_PROP_NS_TIME:
			val=sbbs->ns_time;
			break;
		case BBS_PROP_LAST_NS_TIME:
			val=sbbs->last_ns_time;
			break;
		case BBS_PROP_ONLINE:
			val=sbbs->online;
			break;
		case BBS_PROP_TIMELEFT:
			val=sbbs->timeleft;
			break;

		case BBS_PROP_NODE_NUM:
			val=sbbs->cfg.node_num;
			break;
		case BBS_PROP_NODE_MISC:
			val=sbbs->cfg.node_misc;
			break;
		case BBS_PROP_NODE_VAL_USER:
			val=sbbs->cfg.node_valuser;
			break;

		case BBS_PROP_LOGON_ULB:
			val=sbbs->logon_ulb;
			break;
		case BBS_PROP_LOGON_DLB:
			val=sbbs->logon_dlb;
			break;
		case BBS_PROP_LOGON_ULS:
			val=sbbs->logon_uls;
			break;
		case BBS_PROP_LOGON_DLS:
			val=sbbs->logon_dls;
			break;
		case BBS_PROP_LOGON_POSTS:
			val=sbbs->logon_posts;
			break;
		case BBS_PROP_LOGON_EMAILS:
			val=sbbs->logon_emails;
			break;
		case BBS_PROP_LOGON_FBACKS:
			val=sbbs->logon_fbacks;
			break;
		case BBS_PROP_POSTS_READ:
			val=sbbs->posts_read;
			break;

		case BBS_PROP_MENU_DIR:
			p=sbbs->menu_dir;
			break;
		case BBS_PROP_MENU_FILE:
			p=sbbs->menu_file;
			break;
		case BBS_PROP_MAIN_CMDS:
			val=sbbs->main_cmds;
			break;
		case BBS_PROP_XFER_CMDS:
			val=sbbs->xfer_cmds;
			break;
		case BBS_PROP_CONNECTION:
			p=sbbs->connection;
			break;
		case BBS_PROP_RLOGIN_NAME:
			p=sbbs->rlogin_name;
			break;
		case BBS_PROP_CLIENT_NAME:
			p=sbbs->client_name;
			break;

		default:
			return(JS_TRUE);
	}
	if(p!=NULL) 
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, p));
	else
		*vp = INT_TO_JSVAL(val);

	return(JS_TRUE);
}

static JSBool js_bbs_set(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	long		val=0;
    jsint       tiny;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	if(JSVAL_IS_INT(*vp))
		JS_ValueToInt32(cx, *vp, &val);

	switch(tiny) {
		case BBS_PROP_SYS_STATUS:
			sbbs->sys_status=val;
			break;
		case BBS_PROP_STARTUP_OPT:
			sbbs->startup->options=val;
			break;
		case BBS_PROP_ANSWER_TIME:
			sbbs->answertime=val;
			break;
		case BBS_PROP_LOGON_TIME:
			sbbs->logontime=val;
			break;
		case BBS_PROP_NS_TIME:
			sbbs->ns_time=val;
			break;
		case BBS_PROP_LAST_NS_TIME:
			sbbs->last_ns_time=val;
			break;
		case BBS_PROP_ONLINE:
			sbbs->online=val;
			break;
		case BBS_PROP_TIMELEFT:
			sbbs->timeleft=val;
			break;
		case BBS_PROP_NODE_MISC:
			sbbs->cfg.node_misc=val;
			break;
		case BBS_PROP_NODE_VAL_USER:
			sbbs->cfg.node_valuser=(ushort)val;
			break;
		case BBS_PROP_LOGON_ULB:
			sbbs->logon_ulb=val;
			break;
		case BBS_PROP_LOGON_DLB:
			sbbs->logon_dlb=val;
			break;
		case BBS_PROP_LOGON_ULS:
			sbbs->logon_uls=val;
			break;
		case BBS_PROP_LOGON_DLS:
			sbbs->logon_dls=val;
			break;
		case BBS_PROP_LOGON_POSTS:
			sbbs->logon_posts=val;
			break;
		case BBS_PROP_LOGON_EMAILS:
			sbbs->logon_emails=val;
			break;
		case BBS_PROP_LOGON_FBACKS:
			sbbs->logon_fbacks=val;
			break;
		case BBS_PROP_POSTS_READ:
			sbbs->posts_read=val;
			break;
		case BBS_PROP_MENU_DIR:
			break;
		case BBS_PROP_MENU_FILE:
			break;
		case BBS_PROP_MAIN_CMDS:
			sbbs->main_cmds=val;
			break;
		case BBS_PROP_XFER_CMDS:
			sbbs->xfer_cmds=val;
			break;
		case BBS_PROP_RLOGIN_NAME:
			break;
		case BBS_PROP_CLIENT_NAME:
			break;
		default:
			return(JS_TRUE);
	}

	return(JS_TRUE);
}

#define BBS_PROP_READONLY JSPROP_ENUMERATE|JSPROP_READONLY

static struct JSPropertySpec js_bbs_properties[] = {
/*		 name				,tinyid					,flags				,getter,setter	*/

	{	"sys_status"		,BBS_PROP_SYS_STATUS	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"startup_options"	,BBS_PROP_STARTUP_OPT	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"answer_time"		,BBS_PROP_ANSWER_TIME	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"logon_time"		,BBS_PROP_LOGON_TIME	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"new_file_time"		,BBS_PROP_NS_TIME		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"last_new_file_time",BBS_PROP_LAST_NS_TIME	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"online"			,BBS_PROP_ONLINE		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"time_left"			,BBS_PROP_TIMELEFT		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"node_num"			,BBS_PROP_NODE_NUM		,BBS_PROP_READONLY	,NULL,NULL},
	{	"node_settings"		,BBS_PROP_NODE_MISC		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"node_val_user"		,BBS_PROP_NODE_VAL_USER	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"logon_ulb"			,BBS_PROP_LOGON_ULB		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"logon_dlb"			,BBS_PROP_LOGON_DLB		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"logon_uls"			,BBS_PROP_LOGON_ULS		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"logon_dls"			,BBS_PROP_LOGON_DLS		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"logon_posts"		,BBS_PROP_LOGON_POSTS	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"logon_emails"		,BBS_PROP_LOGON_EMAILS	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"logon_fbacks"		,BBS_PROP_LOGON_FBACKS	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"posts_read"		,BBS_PROP_POSTS_READ	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"menu_dir"			,BBS_PROP_MENU_DIR		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"menu_file"			,BBS_PROP_MENU_FILE		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"main_cmds"			,BBS_PROP_MAIN_CMDS		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"xfer_cmds"			,BBS_PROP_XFER_CMDS		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"connection"		,BBS_PROP_CONNECTION	,BBS_PROP_READONLY	,NULL,NULL},
	{	"rlogin_name"		,BBS_PROP_RLOGIN_NAME	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"client_name"		,BBS_PROP_CLIENT_NAME	,JSPROP_ENUMERATE	,NULL,NULL},
	{0}
};

/**************************/
/* bbs Object Methods */
/**************************/

static JSBool
js_menu(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString*	str;
 	sbbs_t*		sbbs;
 
 	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
 		return(JS_FALSE);
 
 	str = JS_ValueToString(cx, argv[0]);
 	if (!str)
 		return(JS_FALSE);
 
	sbbs->menu(JS_GetStringBytes(str));
 
    return(JS_TRUE);
}
 
static JSBool
js_hangup(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->hangup();

	return(JS_TRUE);
}

static JSBool
js_exec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uintN		i;
	sbbs_t*		sbbs;
	long		mode=0;
    JSString*	cmd;
	JSString*	startup_dir=NULL;
	char*		p_startup_dir=NULL;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((cmd=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	for(i=1;i<argc;i++) {
		if(JSVAL_IS_INT(argv[i]))
			mode=JSVAL_TO_INT(argv[i]);
		else if(JSVAL_IS_STRING(argv[i]))
			startup_dir=JS_ValueToString(cx,argv[i]);
	}

	if(startup_dir!=NULL)
		p_startup_dir=JS_GetStringBytes(startup_dir);

	*rval = INT_TO_JSVAL(sbbs->external(JS_GetStringBytes(cmd),mode,p_startup_dir));

    return(JS_TRUE);
}

static JSBool
js_exec_xtrn(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uint		i;
	char*		code;
	sbbs_t*		sbbs;
    JSString*	str;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((str=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	if((code=JS_GetStringBytes(str))==NULL)
		return(JS_FALSE);

	for(i=0;i<sbbs->cfg.total_xtrns;i++)
		if(!stricmp(sbbs->cfg.xtrn[i]->code,code))
			break;

	if(i>=sbbs->cfg.total_xtrns) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	*rval = BOOLEAN_TO_JSVAL(sbbs->exec_xtrn(i));
	return(JS_TRUE);
}

static JSBool
js_user_event(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(
		sbbs->user_event((user_event_t)JSVAL_TO_INT(argv[0])));
	return(JS_TRUE);
}

static JSBool
js_chksyspass(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(sbbs->chksyspass());

	return(JS_TRUE);
}

static JSFunctionSpec js_bbs_functions[] = {
	{"menu",			js_menu,			1},		// show menu
	{"hangup",			js_hangup,			0},		// hangup immediately
	{"exec",			js_exec,			2},		// execute command line with mode
	{"exec_xtrn",		js_exec_xtrn,		1},		// execute external program by code
	{"user_event",		js_user_event,		1},		// execute user event by event type
	{"check_syspass",	js_chksyspass,		0},		// verify system password
	{0}
};


static JSClass js_bbs_class = {
     "BBS"					/* name			*/
    ,0						/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_bbs_get				/* getProperty	*/
	,js_bbs_set				/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};

JSObject* js_CreateBbsObject(JSContext* cx, JSObject* parent)
{
	JSObject* obj;

	obj = JS_DefineObject(cx, parent, "bbs", &js_bbs_class, NULL, 0);

	if(obj==NULL)
		return(NULL);

	if(!JS_DefineProperties(cx, obj, js_bbs_properties))
		return(NULL);

	if (!JS_DefineFunctions(cx, obj, js_bbs_functions)) 
		return(NULL);

	return(obj);
}

#endif	/* JAVSCRIPT */