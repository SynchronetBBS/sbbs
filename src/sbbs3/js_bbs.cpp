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
};

static JSBool js_bbs_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	ulong		val;
    jsint       tiny;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case BBS_PROP_ONLINE:
			val=sbbs->online;
			break;
		case BBS_PROP_STATUS:
			val=sbbs->bbs;
			break;
		case BBS_PROP_LNCNTR:
			val=sbbs->lncntr;
			break;
		case BBS_PROP_TOS:
			val=sbbs->tos;
			break;
		case BBS_PROP_ROWS:
			val=sbbs->rows;
			break;
		case BBS_PROP_AUTOTERM:
			val=sbbs->autoterm;
			break;
		case BBS_PROP_TIMEOUT:
			val=sbbs->timeout;
			break;
		case BBS_PROP_TIMELEFT_WARN:
			val=sbbs->timeleft_warn;
			break;
		default:
			return(JS_TRUE);
	}

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
		case BBS_PROP_ONLINE:
			sbbs->online=val;
			break;
		case BBS_PROP_STATUS:
			sbbs->bbs=val;
			break;
		case BBS_PROP_LNCNTR:
			sbbs->lncntr=val;
			break;
		case BBS_PROP_TOS:
			sbbs->tos=val;
			break;
		case BBS_PROP_ROWS:
			sbbs->rows=val;
			break;
		case BBS_PROP_AUTOTERM:
			sbbs->autoterm=val;
			break;
		case BBS_PROP_TIMEOUT:
			sbbs->timeout=val;
			break;
		case BBS_PROP_TIMELEFT_WARN:
			sbbs->timeleft_warn=val;
			break;
		default:
			return(JS_TRUE);
	}

	return(JS_TRUE);
}

#define BBS_PROP_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static struct JSPropertySpec js_bbs_properties[] = {
/*		 name				,tinyid					,flags			,getter,setter	*/

	{	"online"			,BBS_PROP_ONLINE		,BBS_PROP_FLAGS	,NULL,NULL},
	{	"status"			,BBS_PROP_STATUS		,BBS_PROP_FLAGS	,NULL,NULL},
	{	"line_counter"		,BBS_PROP_LNCNTR 		,BBS_PROP_FLAGS	,NULL,NULL},
	{	"top_of_screen"		,BBS_PROP_TOS			,BBS_PROP_FLAGS	,NULL,NULL},
	{	"rows"				,BBS_PROP_ROWS			,BBS_PROP_FLAGS	,NULL,NULL},
	{	"autoterm"			,BBS_PROP_AUTOTERM		,BBS_PROP_FLAGS	,NULL,NULL},
	{	"timeout"			,BBS_PROP_TIMEOUT		,BBS_PROP_FLAGS	,NULL,NULL},
	{	"timeleft_warning"	,BBS_PROP_TIMELEFT_WARN	,BBS_PROP_FLAGS	,NULL,NULL},
	{0}
};

/**************************/
/* bbs Object Methods */
/**************************/

static JSBool
js_exec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    return(JS_TRUE);
}

static JSFunctionSpec js_bbs_functions[] = {
	{"exec",			js_exec,			2},		// execute command line with mode
	{"exec_xtrn",		js_exec_xtrn,		1},		// execute external program by code
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