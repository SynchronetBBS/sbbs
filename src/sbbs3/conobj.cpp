/* conobj.cpp */

/* Synchronet JavaScript "Console" Object */

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

/* Console Object Properites */
enum {
	 CON_PROP_ONLINE
	,CON_PROP_STATUS
	,CON_PROP_LNCNTR 
	,CON_PROP_TOS
	,CON_PROP_ROWS
	,CON_PROP_AUTOTERM
	,CON_PROP_TIMEOUT			/* User inactivity timeout reference */
	,CON_PROP_TIMELEFT_WARN		/* low timeleft warning flag */
};

static JSBool js_console_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	ulong		val;
    jsint       tiny;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case CON_PROP_ONLINE:
			val=sbbs->online;
			break;
		case CON_PROP_STATUS:
			val=sbbs->console;
			break;
		case CON_PROP_LNCNTR:
			val=sbbs->lncntr;
			break;
		case CON_PROP_TOS:
			val=sbbs->tos;
			break;
		case CON_PROP_ROWS:
			val=sbbs->rows;
			break;
		case CON_PROP_AUTOTERM:
			val=sbbs->autoterm;
			break;
		case CON_PROP_TIMEOUT:
			val=sbbs->timeout;
			break;
		case CON_PROP_TIMELEFT_WARN:
			val=sbbs->timeleft_warn;
			break;
		default:
			return(JS_TRUE);
	}

	*vp = INT_TO_JSVAL(val);

	return(JS_TRUE);
}

static JSBool js_console_set(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	long		val;
    jsint       tiny;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	JS_ValueToInt32(cx, *vp, &val);

	switch(tiny) {
		case CON_PROP_ONLINE:
			sbbs->online=val;
			break;
		case CON_PROP_STATUS:
			sbbs->console=val;
			break;
		case CON_PROP_LNCNTR:
			sbbs->lncntr=val;
			break;
		case CON_PROP_TOS:
			sbbs->tos=val;
			break;
		case CON_PROP_ROWS:
			sbbs->rows=val;
			break;
		case CON_PROP_AUTOTERM:
			sbbs->autoterm=val;
			break;
		case CON_PROP_TIMEOUT:
			sbbs->timeout=val;
			break;
		case CON_PROP_TIMELEFT_WARN:
			sbbs->timeleft_warn=val;
			break;
		default:
			return(JS_TRUE);
	}

	return(JS_TRUE);
}

static JSBool
js_cls(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->CLS;

    return(JS_TRUE);
}

static JSBool
js_crlf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->outchar(CR);
	sbbs->outchar(LF);

    return(JS_TRUE);
}

static JSBool
js_attr(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->attr(JSVAL_TO_INT(argv[0]));

    return(JS_TRUE);
}

static JSBool
js_pause(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->pause();

    return(JS_TRUE);
}

static JSBool
js_print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString*	str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	str = JS_ValueToString(cx, argv[0]);
	if (!str)
		return(JS_FALSE);

	sbbs->rputs(JS_GetStringBytes(str));

    return(JS_TRUE);
}

static JSBool
js_center(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString*	str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	str = JS_ValueToString(cx, argv[0]);
	if (!str)
		return(JS_FALSE);

	sbbs->center(JS_GetStringBytes(str));

    return(JS_TRUE);
}

static JSBool
js_saveline(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->slatr[sbbs->slcnt]=sbbs->latr; 
	sprintf(sbbs->slbuf[sbbs->slcnt<SAVE_LINES ? sbbs->slcnt++ : sbbs->slcnt] 
			,"%.*s",sbbs->lbuflen,sbbs->lbuf); 
	sbbs->lbuflen=0; 

    return(JS_TRUE);
}

static JSBool
js_restoreline(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->lbuflen=0; 
	sbbs->attr(sbbs->slatr[--sbbs->slcnt]);
	sbbs->rputs(sbbs->slbuf[sbbs->slcnt]); 
	sbbs->curatr=LIGHTGRAY;

    return(JS_TRUE);
}

static JSBool
js_ansi_save(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->ANSI_SAVE();

    return(JS_TRUE);
}


static JSBool
js_ansi_restore(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->ANSI_RESTORE();

    return(JS_TRUE);
}

static JSBool
js_ansi_gotoxy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if(argc<2)
		return(JS_FALSE);

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->GOTOXY(JSVAL_TO_INT(argv[0]),JSVAL_TO_INT(argv[1]));

    return(JS_TRUE);
}

static JSBool
js_ansi_up(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc)
		sbbs->rprintf("\x1b[%dA",JSVAL_TO_INT(argv[0]));
	else
		sbbs->rputs("\x1b[A");

    return(JS_TRUE);
}

static JSBool
js_ansi_down(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc)
		sbbs->rprintf("\x1b[%dB",JSVAL_TO_INT(argv[0]));
	else
		sbbs->rputs("\x1b[B");

    return(JS_TRUE);
}

static JSBool
js_ansi_right(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc)
		sbbs->rprintf("\x1b[%dC",JSVAL_TO_INT(argv[0]));
	else
		sbbs->rputs("\x1b[C");

    return(JS_TRUE);
}

static JSBool
js_ansi_left(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc)
		sbbs->rprintf("\x1b[%dD",JSVAL_TO_INT(argv[0]));
	else
		sbbs->rputs("\x1b[D");

    return(JS_TRUE);
}

static JSBool
js_ansi_getlines(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->ansi_getlines();

    return(JS_TRUE);
}

static JSFunctionSpec js_console_functions[] = {
	{"cls",             js_cls,				0},		/* clear screen */
	{"crlf",            js_crlf,			0},		/* output cr/lf */
	{"attr",			js_attr,			1},		/* set current text attribute */
	{"pause",			js_pause,			0},		/* pause */
	{"print",			js_print,			1},		/* display a raw string */
	{"center",			js_center,			1},		/* display a string centered on the screen */
	{"saveline",		js_saveline,		0},		/* save last output line */
	{"restoreline",		js_restoreline,		0},		/* restore last output line */
	{"ansi_pushxy",		js_ansi_save,		0},
	{"ansi_popxy",		js_ansi_restore,	0},
	{"ansi_gotoxy",		js_ansi_gotoxy,		2},
	{"ansi_up",			js_ansi_up,			0},
	{"ansi_down",		js_ansi_down,		0},
	{"ansi_right",		js_ansi_right,		0},
	{"ansi_left",		js_ansi_left,		0},
	{"ansi_getlines",	js_ansi_getlines,	0},
	{0}
};


static JSClass js_console_class = {
     "Console"				/* name			*/
    ,0						/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_console_get		/* getProperty	*/
	,js_console_set		/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};

#define CON_PROP_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static struct JSPropertySpec js_console_properties[] = {
/*		 name				,tinyid					,flags			,getter,setter	*/

	{	"online"			,CON_PROP_ONLINE		,CON_PROP_FLAGS	,NULL,NULL},
	{	"status"			,CON_PROP_STATUS		,CON_PROP_FLAGS	,NULL,NULL},
	{	"line_counter"		,CON_PROP_LNCNTR 		,CON_PROP_FLAGS	,NULL,NULL},
	{	"top_of_screen"		,CON_PROP_TOS			,CON_PROP_FLAGS	,NULL,NULL},
	{	"rows"				,CON_PROP_ROWS			,CON_PROP_FLAGS	,NULL,NULL},
	{	"autoterm"			,CON_PROP_AUTOTERM		,CON_PROP_FLAGS	,NULL,NULL},
	{	"timeout"			,CON_PROP_TIMEOUT		,CON_PROP_FLAGS	,NULL,NULL},
	{	"timeleft_warning"	,CON_PROP_TIMELEFT_WARN	,CON_PROP_FLAGS	,NULL,NULL},
	{0}
};

JSObject* js_CreateConsoleObject(JSContext* cx, JSObject* parent)
{
	JSObject* obj;

	obj = JS_DefineObject(cx, parent, "console", &js_console_class, NULL, 0);

	if(obj==NULL)
		return(NULL);

	if(!JS_DefineProperties(cx, obj, js_console_properties))
		return(NULL);

	if (!JS_DefineFunctions(cx, obj, js_console_functions)) 
		return(NULL);

	return(obj);
}

#endif	/* JAVSCRIPT */