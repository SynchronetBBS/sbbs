/* termobj.cpp */

/* Synchronet JavaScript "Terminal" Object */

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

/* Terminal Object Properites */
enum {
	 TERM_PROP_ONLINE
	,TERM_PROP_STATUS
	,TERM_PROP_CONSOLE
	,TERM_PROP_LNCNTR 
	,TERM_PROP_TOS
	,TERM_PROP_ROWS
	,TERM_PROP_AUTOTERM
	,TERM_PROP_TIMEOUT			/* User inactivity timeout reference */
	,TERM_PROP_TIMELEFT_WARN	/* low timeleft warning flag */
};

static JSBool js_terminal_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	ulong		val;
    jsint       tiny;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case TERM_PROP_ONLINE:
			val=sbbs->online;
			break;
		case TERM_PROP_STATUS:
			val=sbbs->sys_status;
			break;
		case TERM_PROP_CONSOLE:
			val=sbbs->console;
			break;
		case TERM_PROP_LNCNTR:
			val=sbbs->lncntr;
			break;
		case TERM_PROP_TOS:
			val=sbbs->tos;
			break;
		case TERM_PROP_ROWS:
			val=sbbs->rows;
			break;
		case TERM_PROP_AUTOTERM:
			val=sbbs->autoterm;
			break;
		case TERM_PROP_TIMEOUT:
			val=sbbs->timeout;
			break;
		case TERM_PROP_TIMELEFT_WARN:
			val=sbbs->timeleft_warn;
			break;
		default:
			return(JS_TRUE);
	}

	*vp = INT_TO_JSVAL(val);

	return(JS_TRUE);
}

static JSBool js_terminal_set(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	long		val;
    jsint       tiny;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	JS_ValueToInt32(cx, *vp, &val);

	switch(tiny) {
		case TERM_PROP_ONLINE:
			sbbs->online=val;
			break;
		case TERM_PROP_STATUS:
			sbbs->sys_status=val;
			break;
		case TERM_PROP_CONSOLE:
			sbbs->console=val;
			break;
		case TERM_PROP_LNCNTR:
			sbbs->lncntr=val;
			break;
		case TERM_PROP_TOS:
			sbbs->tos=val;
			break;
		case TERM_PROP_ROWS:
			sbbs->rows=val;
			break;
		case TERM_PROP_AUTOTERM:
			sbbs->autoterm=val;
			break;
		case TERM_PROP_TIMEOUT:
			sbbs->timeout=val;
			break;
		case TERM_PROP_TIMELEFT_WARN:
			sbbs->timeleft_warn=val;
			break;
		default:
			return(JS_TRUE);
	}

	return(JS_TRUE);
}


static JSClass js_terminal_class = {
     "Terminal"				/* name			*/
    ,0						/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_terminal_get		/* getProperty	*/
	,js_terminal_set		/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};

#define TERM_PROP_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static struct JSPropertySpec js_terminal_properties[] = {
/*		 name				,tinyid					,flags				,getter,	setter	*/

	{	"online"			,TERM_PROP_ONLINE		,TERM_PROP_FLAGS	,NULL,NULL},
	{	"status"			,TERM_PROP_STATUS		,TERM_PROP_FLAGS	,NULL,NULL},
	{	"console"			,TERM_PROP_CONSOLE		,TERM_PROP_FLAGS	,NULL,NULL},
	{	"line_counter"		,TERM_PROP_LNCNTR 		,TERM_PROP_FLAGS	,NULL,NULL},
	{	"top_of_screen"		,TERM_PROP_TOS			,TERM_PROP_FLAGS	,NULL,NULL},
	{	"rows"				,TERM_PROP_ROWS			,TERM_PROP_FLAGS	,NULL,NULL},
	{	"autoterm"			,TERM_PROP_AUTOTERM		,TERM_PROP_FLAGS	,NULL,NULL},
	{	"timeout"			,TERM_PROP_TIMEOUT		,TERM_PROP_FLAGS	,NULL,NULL},
	{	"timeleft_warning"	,TERM_PROP_TIMELEFT_WARN,TERM_PROP_FLAGS	,NULL,NULL},
	{0}
};

JSObject* js_CreateTerminalObject(JSContext* cx, JSObject* parent)
{
	JSObject* termobj;

	termobj = JS_DefineObject(cx, parent, "terminal", &js_terminal_class, NULL, 0);

	if(termobj==NULL)
		return(NULL);

	if(!JS_DefineProperties(cx, termobj, js_terminal_properties))
		return(NULL);

	return(termobj);
}

#endif	/* JAVSCRIPT */