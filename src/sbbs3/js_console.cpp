/* js_console.cpp */

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

/*****************************/
/* Console Object Properites */
/*****************************/
enum {
	 CON_PROP_STATUS
	,CON_PROP_LNCNTR 
	,CON_PROP_ATTR
	,CON_PROP_TOS
	,CON_PROP_ROWS
	,CON_PROP_AUTOTERM
	,CON_PROP_WORDWRAP
	,CON_PROP_QUESTION
	,CON_PROP_TIMEOUT			/* User inactivity timeout reference */
	,CON_PROP_TIMELEFT_WARN		/* low timeleft warning flag */
	,CON_PROP_ABORTABLE
	,CON_PROP_TELNET_MODE

	/* Must be last */
	,CON_PROPERTIES
};

#ifdef _DEBUG
static char* con_prop_desc[CON_PROPERTIES+1] = {
	 "status bits (see CON_* in sbbsdefs.js)"
	,"current line counter (used for automatic screen pause"
	,"current display attributes"
	,"top-of-screen"
	,"number of terminal rows"
	,"automatically detected terminal settings"
	,"word-wrap buffer"
	,"current yes/no question"
	,"user inactivity timeout reference"
	,"low timeleft warning flag"
	,"output can be aborted with Ctrl-C"
	,"current telnet mode"
};

#endif

static JSBool js_console_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	ulong		val;
    jsint       tiny;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case CON_PROP_STATUS:
			val=sbbs->console;
			break;
		case CON_PROP_LNCNTR:
			val=sbbs->lncntr;
			break;
		case CON_PROP_ATTR:
			val=sbbs->curatr;
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
		case CON_PROP_ABORTABLE:
			val=sbbs->rio_abortable;
			break;
		case CON_PROP_TELNET_MODE:
			val=sbbs->telnet_mode;
			break;
		case CON_PROP_WORDWRAP:
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, sbbs->wordwrap));
			return(JS_TRUE);
		case CON_PROP_QUESTION:
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, sbbs->question));
			return(JS_TRUE);
		default:
			return(JS_TRUE);
	}

	*vp = INT_TO_JSVAL(val);

	return(JS_TRUE);
}

static JSBool js_console_set(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	int32		val=0;
    jsint       tiny;
	sbbs_t*		sbbs;
	JSString*	str;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	if(JSVAL_IS_INT(*vp) || JSVAL_IS_BOOLEAN(*vp))
		JS_ValueToInt32(cx, *vp, &val);

	switch(tiny) {
		case CON_PROP_STATUS:
			sbbs->console=val;
			break;
		case CON_PROP_LNCNTR:
			sbbs->lncntr=val;
			break;
		case CON_PROP_ATTR:
			sbbs->attr(val);
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
		case CON_PROP_ABORTABLE:
			sbbs->rio_abortable=val 
				? true:false; // This is a dumb bool conversion to make BC++ happy
			break;
		case CON_PROP_TELNET_MODE:
			sbbs->telnet_mode=val;
			break;
		case CON_PROP_QUESTION:
			if((str=JS_ValueToString(cx, *vp))==NULL)
				break;
			SAFECOPY(sbbs->question,JS_GetStringBytes(str));
			break;
		default:
			return(JS_TRUE);
	}

	return(JS_TRUE);
}

#define CON_PROP_FLAGS JSPROP_ENUMERATE

static struct JSPropertySpec js_console_properties[] = {
/*		 name				,tinyid					,flags			,getter,setter	*/

	{	"status"			,CON_PROP_STATUS		,CON_PROP_FLAGS	,NULL,NULL},
	{	"line_counter"		,CON_PROP_LNCNTR 		,CON_PROP_FLAGS	,NULL,NULL},
	{	"attributes"		,CON_PROP_ATTR			,CON_PROP_FLAGS	,NULL,NULL},
	{	"top_of_screen"		,CON_PROP_TOS			,CON_PROP_FLAGS	,NULL,NULL},
	{	"screen_rows"		,CON_PROP_ROWS			,CON_PROP_FLAGS	,NULL,NULL},
	{	"autoterm"			,CON_PROP_AUTOTERM		,CON_PROP_FLAGS	,NULL,NULL},
	{	"timeout"			,CON_PROP_TIMEOUT		,CON_PROP_FLAGS	,NULL,NULL},
	{	"timeleft_warning"	,CON_PROP_TIMELEFT_WARN	,CON_PROP_FLAGS	,NULL,NULL},
	{	"rio_abortable"		,CON_PROP_ABORTABLE		,CON_PROP_FLAGS	,NULL,NULL},
	{	"telnet_mode"		,CON_PROP_TELNET_MODE	,CON_PROP_FLAGS	,NULL,NULL},
	{	"wordwrap"			,CON_PROP_WORDWRAP		,JSPROP_ENUMERATE|JSPROP_READONLY ,NULL,NULL},
	{	"question"			,CON_PROP_QUESTION		,CON_PROP_FLAGS ,NULL,NULL},
	{0}
};

/**************************/
/* Console Object Methods */
/**************************/

static JSBool
js_inkey(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		key[2];
	long		mode=0;
	sbbs_t*		sbbs;
    JSString*	js_str;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc && JSVAL_IS_INT(argv[0]))
		mode=JSVAL_TO_INT(argv[0]);
	key[0]=sbbs->inkey(mode);
	key[1]=0;

	js_str = JS_NewStringCopyZ(cx, key);
	*rval = STRING_TO_JSVAL(js_str);
    return(JS_TRUE);
}

static JSBool
js_getkey(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		key[2];
	long		mode=0;
	sbbs_t*		sbbs;
    JSString*	js_str;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc && JSVAL_IS_INT(argv[0]))
		mode=JSVAL_TO_INT(argv[0]);
	key[0]=sbbs->getkey(mode);
	key[1]=0;

	js_str = JS_NewStringCopyZ(cx, key);
	*rval = STRING_TO_JSVAL(js_str);
    return(JS_TRUE);
}

static JSBool
js_getstr(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		*p;
	long		mode=0;
	uintN		i;
	size_t		maxlen=0;
	sbbs_t*		sbbs;
    JSString*	js_str=NULL;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	for(i=0;i<argc;i++) {
		if(JSVAL_IS_INT(argv[i])) {
			if(!maxlen)
				maxlen=JSVAL_TO_INT(argv[i]);
			else
				mode=JSVAL_TO_INT(argv[i]);
			continue;
		}
		if(JSVAL_IS_STRING(argv[i])) {
			js_str = JS_ValueToString(cx, argv[i]);
			if (!js_str)
			    return(JS_FALSE);
		}
	}

	if(!maxlen) maxlen=128;

	if((p=(char *)calloc(1,maxlen+1))==NULL)
		return(JS_FALSE);

	if(js_str!=NULL)
		sprintf(p,"%.*s",(int)maxlen,JS_GetStringBytes(js_str));

	sbbs->getstr(p,maxlen,mode);

	js_str = JS_NewStringCopyZ(cx, p);

	free(p);

	*rval = STRING_TO_JSVAL(js_str);
    return(JS_TRUE);
}

static JSBool
js_getnum(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	ulong		maxnum=~0;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc && JSVAL_IS_INT(argv[0]))
		maxnum=JSVAL_TO_INT(argv[0]);

	*rval = INT_TO_JSVAL(sbbs->getnum(maxnum));
    return(JS_TRUE);
}

static JSBool
js_getkeys(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		key[2];
	uintN		i;
	long		val;
	ulong		maxnum=~0;
	sbbs_t*		sbbs;
    JSString*	js_str=NULL;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	for(i=0;i<argc;i++) {
		if(JSVAL_IS_INT(argv[i])) {
			maxnum=JSVAL_TO_INT(argv[i]);
			continue;
		}
		if(JSVAL_IS_STRING(argv[i])) {
			js_str = JS_ValueToString(cx, argv[i]);
		}
	}
	if(js_str==NULL)
		return(JS_FALSE);

	val=sbbs->getkeys(JS_GetStringBytes(js_str),maxnum);

	if(val==-1) {			// abort
		*rval = INT_TO_JSVAL(0);
	} else if(val<0) {		// number
		val&=~0x80000000;
		*rval = INT_TO_JSVAL(val);
	} else {				// key
		key[0]=(uchar)val;
		key[1]=0;
		js_str = JS_NewStringCopyZ(cx, key);
		*rval = STRING_TO_JSVAL(js_str);
	}

    return(JS_TRUE);
}

static JSBool
js_gettemplate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		str[128];
	long		mode=0;
	uintN		i;
	sbbs_t*		sbbs;
    JSString*	js_str=NULL;
    JSString*	js_fmt=NULL;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	for(i=0;i<argc;i++) {
		if(JSVAL_IS_STRING(argv[i])) {
			if(js_fmt==NULL)
				js_fmt = JS_ValueToString(cx, argv[i]);
			else
				js_str = JS_ValueToString(cx, argv[i]);
		} else if (JSVAL_IS_INT(argv[i]))
			mode=JSVAL_TO_INT(argv[i]);
	}

	if(js_fmt==NULL)
		return(JS_FALSE);

	if(js_str==NULL)
		str[0]=0;
	else
		SAFECOPY(str,JS_GetStringBytes(js_str));

	sbbs->gettmplt(str,JS_GetStringBytes(js_fmt),mode);
	js_str = JS_NewStringCopyZ(cx, str);
	*rval = STRING_TO_JSVAL(js_str);
    return(JS_TRUE);
}

static JSBool
js_ungetstr(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	sbbs_t*		sbbs;
    JSString*	js_str;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);
	
	if((js_str=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	p=JS_GetStringBytes(js_str);

	while(p && *p)
		sbbs->ungetkey(*(p++));
	*rval=JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_yesno(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;
    JSString*	js_str;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);
	
	if((js_str=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(sbbs->yesno(JS_GetStringBytes(js_str)));
    return(JS_TRUE);
}

static JSBool
js_noyes(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;
    JSString*	js_str;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);
	
	if((js_str=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(sbbs->noyes(JS_GetStringBytes(js_str)));
    return(JS_TRUE);
}

static JSBool
js_mnemonics(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;
    JSString*	js_str;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);
	
	sbbs->mnemonics(JS_GetStringBytes(js_str));
	*rval=JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_clear(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->CLS;
	*rval=JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_clearline(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->clearline();
	*rval=JSVAL_VOID;
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
	*rval=JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_pause(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->pause();
	*rval=JSVAL_VOID;
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

	sbbs->bputs(JS_GetStringBytes(str));
	*rval=JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_write(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString*	str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	str = JS_ValueToString(cx, argv[0]);
	if (!str)
		return(JS_FALSE);

	sbbs->rputs(JS_GetStringBytes(str));
	*rval=JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_putmsg(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	long		mode=0;
    JSString*	str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	str = JS_ValueToString(cx, argv[0]);
	if (!str)
		return(JS_FALSE);

	if(argc>1 && JSVAL_IS_INT(argv[1]))
		mode=JSVAL_TO_INT(argv[1]);

	sbbs->putmsg(JS_GetStringBytes(str),mode);
	*rval=JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_printfile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	long		mode=0;
    JSString*	str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	str = JS_ValueToString(cx, argv[0]);
	if (!str)
		return(JS_FALSE);

	if(argc>1 && JSVAL_IS_INT(argv[1]))
		mode=JSVAL_TO_INT(argv[1]);

	sbbs->printfile(JS_GetStringBytes(str),mode);
	*rval=JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_printtail(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int			lines=0;
	long		mode=0;
	uintN		i;
	sbbs_t*		sbbs;
    JSString*	js_str=NULL;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	for(i=0;i<argc;i++) {
		if(JSVAL_IS_INT(argv[i])) {
			if(!lines)
				lines=JSVAL_TO_INT(argv[i]);
			else
				mode=JSVAL_TO_INT(argv[i]);
		} else if(JSVAL_IS_STRING(argv[i]))
			js_str = JS_ValueToString(cx, argv[i]);
	}

	if(js_str==NULL)
		return(JS_FALSE);

	if(!lines) 
		lines=5;

	sbbs->printtail(JS_GetStringBytes(js_str),lines,mode);
	*rval=JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_editfile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString*	str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((str=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	sbbs->editfile(JS_GetStringBytes(str));
	*rval=JSVAL_VOID;
    return(JS_TRUE);
}


static JSBool
js_uselect(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uintN		i;
	int			num=0;
	char*		title="";
	char*		item="";
	char*		ar_str;
	uchar*		ar=NULL;
	sbbs_t*		sbbs;
    JSString*	js_str;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);
	
	if(!argc) {
		*rval = INT_TO_JSVAL(sbbs->uselect(0,0,NULL,NULL,NULL));
		return(JS_TRUE);
	}
	
	for(i=0;i<argc;i++) {
		if(!JSVAL_IS_STRING(argv[i])) {
			num = JSVAL_TO_INT(argv[i]);
			continue;
		}
		if((js_str=JS_ValueToString(cx, argv[i]))==NULL)
			return(JS_FALSE);

		if(title==NULL) 
			title=JS_GetStringBytes(js_str);
		else if(item==NULL)
			item=JS_GetStringBytes(js_str);
		else {
			ar_str=JS_GetStringBytes(js_str);
			ar=arstr(NULL,ar_str,&sbbs->cfg);
		}
	}

	*rval = INT_TO_JSVAL(sbbs->uselect(1, num, title, item, ar));
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
	*rval=JSVAL_VOID;
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
	*rval=JSVAL_VOID;
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
	*rval=JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_ansi(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		attr=0;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	JS_ValueToInt32(cx,argv[0],&attr);
	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,sbbs->ansi(attr)));
    return(JS_TRUE);
}

static JSBool
js_ansi_save(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->ANSI_SAVE();
	*rval=JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_ansi_restore(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->ANSI_RESTORE();
	*rval=JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_ansi_gotoxy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		x=1,y=1;
	sbbs_t*		sbbs;

	if(argc<2)
		return(JS_FALSE);

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	JS_ValueToInt32(cx,argv[0],&x);
	JS_ValueToInt32(cx,argv[1],&y);

	sbbs->GOTOXY(x,y);
	*rval=JSVAL_VOID;
    return(JS_TRUE);
}

static JSClass js_screen_class = {
     "Screen"				/* name			*/
    ,0						/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,JS_PropertyStub		/* getProperty	*/
	,JS_PropertyStub		/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};


static JSBool
js_ansi_getxy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;
	int			x,y;
	JSObject*	screen;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
 		return(JS_FALSE);
 
	sbbs->ansi_getxy(&x,&y);

	if((screen=JS_NewObject(cx,&js_screen_class,NULL,obj))==NULL)
		return(JS_TRUE);

	JS_DefineProperty(cx, screen, "x", INT_TO_JSVAL(x)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, screen, "y", INT_TO_JSVAL(y)
		,NULL,NULL,JSPROP_ENUMERATE);

	*rval = OBJECT_TO_JSVAL(screen);
    return(JS_TRUE);
}


static JSBool
js_ansi_up(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		val=1;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc) {
		JS_ValueToInt32(cx,argv[0],&val);
		sbbs->rprintf("\x1b[%dA",val);
	} else
		sbbs->rputs("\x1b[A");
	*rval=JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_ansi_down(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		val=1;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc) {
		JS_ValueToInt32(cx,argv[0],&val);
		sbbs->rprintf("\x1b[%dB",val);
	} else
		sbbs->rputs("\x1b[B");
	*rval=JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_ansi_right(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		val=1;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc) {
		JS_ValueToInt32(cx,argv[0],&val);
		sbbs->rprintf("\x1b[%dC",val);
	} else
		sbbs->rputs("\x1b[C");
	*rval=JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_ansi_left(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		val=1;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc) {
		JS_ValueToInt32(cx,argv[0],&val);
		sbbs->rprintf("\x1b[%dD",val);
	} else
		sbbs->rputs("\x1b[D");
	*rval=JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_ansi_getlines(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->ansi_getlines();
	*rval=JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_lock_input(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;
	JSBool		lock=TRUE;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc)
		JS_ValueToBoolean(cx, argv[0], &lock);

	if(lock)
		pthread_mutex_lock(&sbbs->input_thread_mutex);
	else
		pthread_mutex_unlock(&sbbs->input_thread_mutex);

	*rval=JSVAL_VOID;
    return(JS_TRUE);
}

static jsMethodSpec js_console_functions[] = {
	{"inkey",			js_inkey,			0, JSTYPE_STRING,	JSDOCSTR("[number mode]")
	,JSDOCSTR("get a single key, no wait")
	},		
	{"getkey",			js_getkey,			0, JSTYPE_STRING,	JSDOCSTR("[number mode]")
	,JSDOCSTR("get a single key, with wait")
	},		
	{"getstr",			js_getstr,			0, JSTYPE_STRING,	JSDOCSTR("[string][,maxlen][,mode]")
	,JSDOCSTR("get a string")
	},		
	{"getnum",			js_getnum,			0, JSTYPE_NUMBER,	JSDOCSTR("[maxnum]")
	,JSDOCSTR("get a number")
	},		
	{"getkeys",			js_getkeys,			1, JSTYPE_NUMBER,	JSDOCSTR("string keys, [maxnum]")
	,JSDOCSTR("get one of a list of keys")
	},		
	{"gettemplate",		js_gettemplate,		1, JSTYPE_STRING,	JSDOCSTR("format [,string] [,mode]")
	,JSDOCSTR("get a string based on template")
	},		
	{"ungetstr",		js_ungetstr,		1, JSTYPE_VOID,		""
	,JSDOCSTR("put a string in the keyboard buffer")
	},		
	{"yesno",			js_yesno,			1, JSTYPE_BOOLEAN,	JSDOCSTR("string question")
	,JSDOCSTR("YES/no question")
	},		
	{"noyes",			js_noyes,			1, JSTYPE_BOOLEAN,	JSDOCSTR("string question")
	,JSDOCSTR("NO/yes question")
	},		
	{"mnemonics",		js_mnemonics,		1, JSTYPE_VOID,		JSDOCSTR("string text")
	,JSDOCSTR("print a mnemonics string")
	},		
	{"clear",           js_clear,			0, JSTYPE_VOID,		""
	,JSDOCSTR("clear screen")
	},		
	{"clearline",       js_clearline,		0, JSTYPE_VOID,		""
	,JSDOCSTR("clear current line")
	},		
	{"crlf",            js_crlf,			0, JSTYPE_VOID,		""
	,JSDOCSTR("output a carriage-return/line-feed pair (new-line)")
	},		
	{"pause",			js_pause,			0, JSTYPE_VOID,		""
	,JSDOCSTR("display pause prompt and wait for key hit")
	},		
	{"print",			js_print,			1, JSTYPE_VOID,		JSDOCSTR("string text")
	,JSDOCSTR("display a string (supports ^A and @-codes)")
	},		
	{"write",			js_write,			1, JSTYPE_VOID,		JSDOCSTR("string text")
	,JSDOCSTR("display a raw string")
	},		
	{"putmsg",			js_putmsg,			1, JSTYPE_VOID,		JSDOCSTR("string text [,number mode]")
	,JSDOCSTR("display message text (^A, @-codes, etc) with mode")
	},		
	{"center",			js_center,			1, JSTYPE_VOID,		JSDOCSTR("string text")
	,JSDOCSTR("display a string centered on the screen")
	},		
	{"printfile",		js_printfile,		1, JSTYPE_VOID,		JSDOCSTR("string text [,number mode]")
	,JSDOCSTR("print a file with optional mode")
	},		
	{"printtail",		js_printtail,		2, JSTYPE_VOID,		JSDOCSTR("string text, number lines [,number mode]")
	,JSDOCSTR("print last x lines of file with optional mode")
	},		
	{"editfile",		js_editfile,		1, JSTYPE_VOID,		JSDOCSTR("string filename")
	,JSDOCSTR("edit a text file")
	},		
	{"uselect",			js_uselect,			0, JSTYPE_NUMBER,	JSDOCSTR("[number, string title, string item, string ars]")
	,JSDOCSTR("user selection menu, call for each item, then with no args to display select menu")
	},		
	{"saveline",		js_saveline,		0, JSTYPE_VOID,		""
	,JSDOCSTR("save last output line")
	},		
	{"restoreline",		js_restoreline,		0, JSTYPE_VOID,		""
	,JSDOCSTR("restore last output line")
	},		
	{"ansi",			js_ansi,			1, JSTYPE_STRING,	JSDOCSTR("number attribute")
	,JSDOCSTR("returns ANSI encoding of specified attribute")
	},		
	{"ansi_pushxy",		js_ansi_save,		0, JSTYPE_ALIAS	},
	{"ansi_save",		js_ansi_save,		0, JSTYPE_VOID,		""
	,JSDOCSTR("save current cursor position (AKA ansi_pushxy)")
	},
	{"ansi_popxy",		js_ansi_restore,	0, JSTYPE_ALIAS },
	{"ansi_restore",	js_ansi_restore,	0, JSTYPE_VOID,		""
	,JSDOCSTR("restore saved cursor position (AKA ansi_popxy)")
	},
	{"ansi_gotoxy",		js_ansi_gotoxy,		2, JSTYPE_VOID,		JSDOCSTR("number x,y")
	,JSDOCSTR("Move cursor to a specific screen coordinate (ANSI)")
	},
	{"ansi_up",			js_ansi_up,			0, JSTYPE_VOID,		JSDOCSTR("[number rows]")
	,JSDOCSTR("Move cursor up one or more rows (ANSI)")
	},
	{"ansi_down",		js_ansi_down,		0, JSTYPE_VOID,		JSDOCSTR("[number rows]")
	,JSDOCSTR("Move cursor down one or more rows (ANSI)")
	},
	{"ansi_right",		js_ansi_right,		0, JSTYPE_VOID,		JSDOCSTR("[number columns]")
	,JSDOCSTR("Move cursor right one or more columns (ANSI)")
	},
	{"ansi_left",		js_ansi_left,		0, JSTYPE_VOID,		JSDOCSTR("[number columns]")
	,JSDOCSTR("Move cursor left one or more columns (ANSI)")
	},
	{"ansi_getlines",	js_ansi_getlines,	0, JSTYPE_VOID,		""
	,JSDOCSTR("Auto-detect the number of rows/lines on the user's terminal (ANSI)")
	},
	{"ansi_getxy",		js_ansi_getxy,		0, JSTYPE_OBJECT,	""
	,JSDOCSTR("Returns the current cursor position as an object (with x and y properties)")
	},
	{"lock_input",		js_lock_input,		1, JSTYPE_VOID,		JSDOCSTR("[boolean lock]")
	,JSDOCSTR("Lock the user input thread (allowing direct client socket access)")
	},
	{0}
};


static JSClass js_console_class = {
     "Console"				/* name			*/
    ,0						/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_console_get			/* getProperty	*/
	,js_console_set			/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};

JSObject* js_CreateConsoleObject(JSContext* cx, JSObject* parent)
{
	JSObject* obj;

	obj = JS_DefineObject(cx, parent, "console", &js_console_class, NULL, JSPROP_ENUMERATE);

	if(obj==NULL)
		return(NULL);

	if(!JS_DefineProperties(cx, obj, js_console_properties))
		return(NULL);

	if (!js_DefineMethods(cx, obj, js_console_functions)) 
		return(NULL);

#ifdef _DEBUG
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", con_prop_desc, JSPROP_READONLY);
#endif

	return(obj);
}

#endif	/* JAVSCRIPT */
