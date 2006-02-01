/* js_console.cpp */

/* Synchronet JavaScript "Console" Object */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2006 Rob Swindell - http://www.synchro.net/copyright.html		*
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
	,CON_PROP_COLUMNS
	,CON_PROP_AUTOTERM
	,CON_PROP_TERMINAL
	,CON_PROP_WORDWRAP
	,CON_PROP_QUESTION
	,CON_PROP_TIMEOUT			/* User inactivity timeout reference */
	,CON_PROP_TIMELEFT_WARN		/* low timeleft warning flag */
	,CON_PROP_ABORTED
	,CON_PROP_ABORTABLE
	,CON_PROP_TELNET_MODE
	,CON_PROP_GETSTR_OFFSET
	,CON_PROP_CTRLKEY_PASSTHRU
	/* read only */
	,CON_PROP_INBUF_LEVEL
	,CON_PROP_INBUF_SPACE
	,CON_PROP_OUTBUF_LEVEL
	,CON_PROP_OUTBUF_SPACE
};

static JSBool js_console_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	ulong		val;
    jsint       tiny;
	JSString*	js_str;
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
		case CON_PROP_COLUMNS:
			val=sbbs->cols;
			break;
		case CON_PROP_AUTOTERM:
			val=sbbs->autoterm;
			break;
		case CON_PROP_TERMINAL:
			if((js_str=JS_NewStringCopyZ(cx, sbbs->terminal))==NULL)
				return(JS_FALSE);
			*vp = STRING_TO_JSVAL(js_str);
			return(JS_TRUE);
		case CON_PROP_TIMEOUT:
			val=sbbs->timeout;
			break;
		case CON_PROP_TIMELEFT_WARN:
			val=sbbs->timeleft_warn;
			break;
		case CON_PROP_ABORTED:
			*vp=BOOLEAN_TO_JSVAL(INT_TO_BOOL(sbbs->sys_status&SS_ABORT));
			return(JS_TRUE);
		case CON_PROP_ABORTABLE:
			*vp=BOOLEAN_TO_JSVAL(INT_TO_BOOL(sbbs->rio_abortable));
			return(JS_TRUE);
		case CON_PROP_TELNET_MODE:
			val=sbbs->telnet_mode;
			break;
		case CON_PROP_GETSTR_OFFSET:
			val=sbbs->getstr_offset;
			break;
		case CON_PROP_WORDWRAP:
			if((js_str=JS_NewStringCopyZ(cx, sbbs->wordwrap))==NULL)
				return(JS_FALSE);
			*vp = STRING_TO_JSVAL(js_str);
			return(JS_TRUE);
		case CON_PROP_QUESTION:
			if((js_str=JS_NewStringCopyZ(cx, sbbs->question))==NULL)
				return(JS_FALSE);
			*vp = STRING_TO_JSVAL(js_str);
			return(JS_TRUE);
		case CON_PROP_CTRLKEY_PASSTHRU:
			val=sbbs->cfg.ctrlkey_passthru;
			break;
		case CON_PROP_INBUF_LEVEL:
			val=RingBufFull(&sbbs->inbuf);
			break;
		case CON_PROP_INBUF_SPACE:
			val=RingBufFree(&sbbs->inbuf);
			break;
		case CON_PROP_OUTBUF_LEVEL:
			val=RingBufFull(&sbbs->outbuf);
			break;
		case CON_PROP_OUTBUF_SPACE:
			val=RingBufFree(&sbbs->outbuf);
			break;
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

	if(JSVAL_IS_NUMBER(*vp) || JSVAL_IS_BOOLEAN(*vp))
		JS_ValueToInt32(cx, *vp, &val);

	switch(tiny) {
		case CON_PROP_STATUS:
			sbbs->console=val;
			break;
		case CON_PROP_LNCNTR:
			sbbs->lncntr=val;
			break;
		case CON_PROP_ATTR:
			if(JSVAL_IS_STRING(*vp)) {
				if((str=JS_ValueToString(cx, *vp))==NULL)
					break;
				val=attrstr(JS_GetStringBytes(str));
			}
			sbbs->attr(val);
			break;
		case CON_PROP_TOS:
			sbbs->tos=val;
			break;
		case CON_PROP_ROWS:
			sbbs->rows=val;
			break;
		case CON_PROP_COLUMNS:
			sbbs->cols=val;
			break;
		case CON_PROP_AUTOTERM:
			sbbs->autoterm=val;
			break;
		case CON_PROP_TERMINAL:
			if((str=JS_ValueToString(cx, *vp))==NULL)
				break;
			SAFECOPY(sbbs->terminal,JS_GetStringBytes(str));
			break;
		case CON_PROP_TIMEOUT:
			sbbs->timeout=val;
			break;
		case CON_PROP_TIMELEFT_WARN:
			sbbs->timeleft_warn=val;
			break;
		case CON_PROP_ABORTED:
			if(val)
				sbbs->sys_status|=SS_ABORT;
			else
				sbbs->sys_status&=~SS_ABORT;
			break;
		case CON_PROP_ABORTABLE:
			sbbs->rio_abortable=val 
				? true:false; // This is a dumb bool conversion to make BC++ happy
			break;
		case CON_PROP_TELNET_MODE:
			sbbs->telnet_mode=val;
			break;
		case CON_PROP_GETSTR_OFFSET:
			sbbs->getstr_offset=val;
			break;
		case CON_PROP_QUESTION:
			if((str=JS_ValueToString(cx, *vp))==NULL)
				break;
			SAFECOPY(sbbs->question,JS_GetStringBytes(str));
			break;
		case CON_PROP_CTRLKEY_PASSTHRU:
			sbbs->cfg.ctrlkey_passthru=val;
			break;
		default:
			return(JS_TRUE);
	}

	return(JS_TRUE);
}

#define CON_PROP_FLAGS JSPROP_ENUMERATE

static jsSyncPropertySpec js_console_properties[] = {
/*		 name				,tinyid						,flags			,ver	*/

	{	"status"			,CON_PROP_STATUS			,CON_PROP_FLAGS	,310},
	{	"line_counter"		,CON_PROP_LNCNTR 			,CON_PROP_FLAGS	,310},
	{	"attributes"		,CON_PROP_ATTR				,CON_PROP_FLAGS	,310},
	{	"top_of_screen"		,CON_PROP_TOS				,CON_PROP_FLAGS	,310},
	{	"screen_rows"		,CON_PROP_ROWS				,CON_PROP_FLAGS	,310},
	{	"screen_columns"	,CON_PROP_COLUMNS			,CON_PROP_FLAGS	,311},
	{	"autoterm"			,CON_PROP_AUTOTERM			,CON_PROP_FLAGS	,310},
	{	"terminal"			,CON_PROP_TERMINAL			,CON_PROP_FLAGS ,311},
	{	"timeout"			,CON_PROP_TIMEOUT			,CON_PROP_FLAGS	,310},
	{	"timeleft_warning"	,CON_PROP_TIMELEFT_WARN		,CON_PROP_FLAGS	,310},
	{	"aborted"			,CON_PROP_ABORTED			,CON_PROP_FLAGS	,310},
	{	"abortable"			,CON_PROP_ABORTABLE			,CON_PROP_FLAGS	,310},
	{	"telnet_mode"		,CON_PROP_TELNET_MODE		,CON_PROP_FLAGS	,310},
	{	"wordwrap"			,CON_PROP_WORDWRAP			,JSPROP_ENUMERATE|JSPROP_READONLY ,310},
	{	"question"			,CON_PROP_QUESTION			,CON_PROP_FLAGS ,310},
	{	"getstr_offset"		,CON_PROP_GETSTR_OFFSET		,CON_PROP_FLAGS ,311},
	{	"ctrlkey_passthru"	,CON_PROP_CTRLKEY_PASSTHRU	,CON_PROP_FLAGS	,310},
	{	"input_buffer_level",CON_PROP_INBUF_LEVEL		,JSPROP_ENUMERATE|JSPROP_READONLY, 312},
	{	"input_buffer_space",CON_PROP_INBUF_SPACE		,JSPROP_ENUMERATE|JSPROP_READONLY, 312},
	{	"output_buffer_level",CON_PROP_OUTBUF_LEVEL		,JSPROP_ENUMERATE|JSPROP_READONLY, 312},
	{	"output_buffer_space",CON_PROP_OUTBUF_SPACE		,JSPROP_ENUMERATE|JSPROP_READONLY, 312},
	{0}
};

#ifdef BUILD_JSDOCS
static char* con_prop_desc[] = {
	 "status bitfield (see <tt>CON_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions)"
	,"current line counter (used for automatic screen pause)"
	,"current display attributes (set with number or string value)"
	,"set to 1 if the terminal cursor is already at the top of the screen"
	,"number of terminal screen rows (in lines)"
	,"number of terminal screen columns (in character cells)"
	,"bitfield of automatically detected terminal settings "
		"(see <tt>USER_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions)"
	,"terminal type description (e.g. 'ANSI')"
	,"user inactivity timeout reference"
	,"low timeleft warning flag"
	,"input/output has been aborted"
	,"output can be aborted with Ctrl-C"
	,"current telnet mode bitfield (see <tt>TELNET_MODE_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions)"
	,"word-wrap buffer (used by getstr) - <small>READ ONLY</small>"
	,"current yes/no question (set by yesno and noyes)"
	,"cursor position offset for use with <tt>getstr(K_USEOFFSET)</tt>"
	,"control key pass-through bitmask, set bits represent control key combinations "
		"<i>not</i> handled by <tt>inkey()</tt> method"
	,"number of bytes currently in the input buffer (from the remote client) - <small>READ ONLY</small>"
	,"number of bytes available in the input buffer	- <small>READ ONLY</small>"
	,"number of bytes currently in the output buffer (from the local server) - <small>READ ONLY</small>"
	,"number of bytes available in the output buffer - <small>READ ONLY</small>"
	,NULL
};
#endif

/**************************/
/* Console Object Methods */
/**************************/

static JSBool
js_inkey(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		key[2];
	int32		mode=0;
	int32		timeout=0;
	sbbs_t*		sbbs;
    JSString*	js_str;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc)
		JS_ValueToInt32(cx,argv[0],&mode);
	if(argc>1)
		JS_ValueToInt32(cx,argv[1],&timeout);
	key[0]=sbbs->inkey(mode,timeout);
	key[1]=0;

	if((js_str = JS_NewStringCopyZ(cx, key))==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
    return(JS_TRUE);
}

static JSBool
js_getkey(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		key[2];
	int32		mode=0;
	sbbs_t*		sbbs;
    JSString*	js_str;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc)
		JS_ValueToInt32(cx,argv[0],&mode);
	key[0]=sbbs->getkey(mode);
	key[1]=0;

	if((js_str = JS_NewStringCopyZ(cx, key))==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
    return(JS_TRUE);
}

static JSBool
js_handle_ctrlkey(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		key;
	int32		mode=0;
	sbbs_t*		sbbs;
    JSString*	js_str;

	*rval = JSVAL_FALSE;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(JSVAL_IS_INT(argv[0]))
		key=(char)JSVAL_TO_INT(argv[0]);
	else {
		if((js_str=JS_ValueToString(cx, argv[0]))==NULL)
			return(JS_FALSE);
		key=*JS_GetStringBytes(js_str);
	}

	if(argc>1)
		JS_ValueToInt32(cx, argv[1], &mode);

	*rval = BOOLEAN_TO_JSVAL(sbbs->handle_ctrlkey(key,mode)==0);
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
		if(JSVAL_IS_NUMBER(argv[i])) {
			if(!maxlen)
				JS_ValueToInt32(cx,argv[i],(int32*)&maxlen);
			else
				JS_ValueToInt32(cx,argv[i],(int32*)&mode);
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

	if(js_str==NULL)
		return(JS_FALSE);

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

	if(argc && JSVAL_IS_NUMBER(argv[0]))
		JS_ValueToInt32(cx,argv[0],(int32*)&maxnum);

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
		if(JSVAL_IS_NUMBER(argv[i])) {
			JS_ValueToInt32(cx,argv[i],(int32*)&maxnum);
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
		if((js_str = JS_NewStringCopyZ(cx, key))==NULL)
			return(JS_FALSE);
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
		} else if(JSVAL_IS_NUMBER(argv[i]))
			JS_ValueToInt32(cx,argv[i],(int32*)&mode);
	}

	if(js_fmt==NULL)
		return(JS_FALSE);

	if(js_str==NULL)
		str[0]=0;
	else
		SAFECOPY(str,JS_GetStringBytes(js_str));

	sbbs->gettmplt(str,JS_GetStringBytes(js_fmt),mode);

	if((js_str=JS_NewStringCopyZ(cx, str))==NULL)
		return(JS_FALSE);

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
    return(JS_TRUE);
}

static void
js_set_attr(JSContext* cx, sbbs_t* sbbs, jsval val)
{
	int32		attr;

	if(JSVAL_IS_STRING(val))
		attr=attrstr(js_ValueToStringBytes(cx,val,NULL));
	else
		JS_ValueToInt32(cx, val, &attr);

	sbbs->attr(attr);
}

static JSBool
js_clear(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc)
		js_set_attr(cx, sbbs, argv[0]);

	sbbs->CLS;
    return(JS_TRUE);
}

static JSBool
js_clearline(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc)
		js_set_attr(cx, sbbs, argv[0]);

	sbbs->clearline();
    return(JS_TRUE);
}

static JSBool
js_cleartoeol(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc)
		js_set_attr(cx, sbbs, argv[0]);

	sbbs->cleartoeol();
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
js_pause(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->pause();
    return(JS_TRUE);
}

static JSBool
js_beep(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;
	int32		i;
	int32		count=1;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc)
		JS_ValueToInt32(cx, argv[0], &count);
	for(i=0;i<count;i++)
		sbbs->outchar('\a');
	
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
    return(JS_TRUE);
}

static JSBool
js_strlen(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString*	str;

	if((str=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	*rval = INT_TO_JSVAL(bstrlen(JS_GetStringBytes(str)));
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

	if(argc>1 && JSVAL_IS_NUMBER(argv[1]))
		JS_ValueToInt32(cx,argv[1],(int32*)&mode);

	sbbs->putmsg(JS_GetStringBytes(str),mode);
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

	if(argc>1 && JSVAL_IS_NUMBER(argv[1]))
		JS_ValueToInt32(cx,argv[1],(int32*)&mode);

	sbbs->printfile(JS_GetStringBytes(str),mode);
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
		if(JSVAL_IS_NUMBER(argv[i])) {
			if(!lines)
				JS_ValueToInt32(cx,argv[i],(int32*)&lines);
			else
				JS_ValueToInt32(cx,argv[i],(int32*)&mode);
		} else if(JSVAL_IS_STRING(argv[i]))
			js_str = JS_ValueToString(cx, argv[i]);
	}

	if(js_str==NULL)
		return(JS_FALSE);

	if(!lines) 
		lines=5;

	sbbs->printtail(JS_GetStringBytes(js_str),lines,mode);
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
    return(JS_TRUE);
}


static JSBool
js_uselect(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uintN		i;
	int32		num=0;
	char*		title=NULL;
	char*		item=NULL;
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
		if(JSVAL_IS_NUMBER(argv[i])) {
			JS_ValueToInt32(cx,argv[i],&num);
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
js_ansi(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		attr=0;
	JSString*	js_str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc)
		JS_ValueToInt32(cx,argv[0],&attr);
	if((js_str=JS_NewStringCopyZ(cx,sbbs->ansi(attr)))==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
    return(JS_TRUE);
}

static JSBool
js_pushxy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->ANSI_SAVE();
    return(JS_TRUE);
}

static JSBool
js_popxy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->ANSI_RESTORE();
    return(JS_TRUE);
}

static JSBool
js_gotoxy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		x=1,y=1;
	jsval		val;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(JSVAL_IS_OBJECT(argv[0])) {
		JS_GetProperty(cx, JSVAL_TO_OBJECT(argv[0]),"x", &val);
		JS_ValueToInt32(cx,val,&x);
		JS_GetProperty(cx, JSVAL_TO_OBJECT(argv[0]),"y", &val);
		JS_ValueToInt32(cx,val,&y);
	} else {
		JS_ValueToInt32(cx,argv[0],&x);
		JS_ValueToInt32(cx,argv[1],&y);
	}

	sbbs->GOTOXY(x,y);
    return(JS_TRUE);
}


static JSBool
js_getxy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;
	int			x,y;
	JSObject*	screen;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
 		return(JS_FALSE);
 
	sbbs->ansi_getxy(&x,&y);

	if((screen=JS_NewObject(cx,NULL,NULL,obj))==NULL)
		return(JS_TRUE);

	JS_DefineProperty(cx, screen, "x", INT_TO_JSVAL(x)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, screen, "y", INT_TO_JSVAL(y)
		,NULL,NULL,JSPROP_ENUMERATE);

	*rval = OBJECT_TO_JSVAL(screen);
    return(JS_TRUE);
}

static JSBool
js_cursor_home(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->cursor_home();
    return(JS_TRUE);
}

static JSBool
js_cursor_up(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		val=1;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc)
		JS_ValueToInt32(cx,argv[0],&val);
	sbbs->cursor_up(val);
    return(JS_TRUE);
}

static JSBool
js_cursor_down(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		val=1;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc)
		JS_ValueToInt32(cx,argv[0],&val);
	sbbs->cursor_down(val);
    return(JS_TRUE);
}

static JSBool
js_cursor_right(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		val=1;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc)
		JS_ValueToInt32(cx,argv[0],&val);
	sbbs->cursor_right(val);
    return(JS_TRUE);
}

static JSBool
js_cursor_left(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		val=1;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc)
		JS_ValueToInt32(cx,argv[0],&val);
	sbbs->cursor_left(val);
    return(JS_TRUE);
}

static JSBool
js_getlines(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->ansi_getlines();
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

	if(lock) {
		pthread_mutex_lock(&sbbs->input_thread_mutex);
		sbbs->input_thread_mutex_locked=true;
	} else if(sbbs->input_thread_mutex_locked) {
		pthread_mutex_unlock(&sbbs->input_thread_mutex);
		sbbs->input_thread_mutex_locked=false;
	}

    return(JS_TRUE);
}

static JSBool
js_telnet_cmd(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;
	int32		cmd,opt=0;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	JS_ValueToInt32(cx,argv[0],&cmd);
	if(argc>1)
		JS_ValueToInt32(cx,argv[1],&opt);

	sbbs->send_telnet_cmd((uchar)cmd,(uchar)opt);

    return(JS_TRUE);
}


static jsSyncMethodSpec js_console_functions[] = {
	{"inkey",			js_inkey,			0, JSTYPE_STRING,	JSDOCSTR("[mode=<tt>K_NONE</tt>] [,timeout=<tt>0</tt>]")
	,JSDOCSTR("get a single key with optional <i>timeout</i> in milliseconds (defaults to 0, for no wait), "
		"see <tt>K_*</tt> in <tt>sbbsdefs.js</tt> for <i>mode</i> bits")
	,311
	},		
	{"getkey",			js_getkey,			0, JSTYPE_STRING,	JSDOCSTR("[mode=<tt>K_NONE</tt>]")
	,JSDOCSTR("get a single key, with wait, "
		"see <tt>K_*</tt> in <tt>sbbsdefs.js</tt> for <i>mode</i> bits")
	,310
	},		
	{"getstr",			js_getstr,			0, JSTYPE_STRING,	JSDOCSTR("[string] [,maxlen=<tt>128</tt>] [,mode=<tt>K_NONE</tt>]")
	,JSDOCSTR("get a text string from the user, "
		"see <tt>K_*</tt> in <tt>sbbsdefs.js</tt> for <i>mode</i> bits")
	,310
	},		
	{"getnum",			js_getnum,			0, JSTYPE_NUMBER,	JSDOCSTR("[maxnum]")
	,JSDOCSTR("get a number between 1 and <i>maxnum</i> from the user")
	,310
	},		
	{"getkeys",			js_getkeys,			1, JSTYPE_NUMBER,	JSDOCSTR("string keys [,maxnum]")
	,JSDOCSTR("get one key from of a list of valid command <i>keys</i>, "
		"or a number between 1 and <i>maxnum</i>")
	,310
	},		
	{"gettemplate",		js_gettemplate,		1, JSTYPE_STRING,	JSDOCSTR("format [,string] [,mode=<tt>0</tt>]")
	,JSDOCSTR("get a string based on template")
	,310
	},		
	{"ungetstr",		js_ungetstr,		1, JSTYPE_VOID,		JSDOCSTR("")
	,JSDOCSTR("put a string in the keyboard buffer")
	,310
	},		
	{"yesno",			js_yesno,			1, JSTYPE_BOOLEAN,	JSDOCSTR("question")
	,JSDOCSTR("YES/no question")
	,310
	},		
	{"noyes",			js_noyes,			1, JSTYPE_BOOLEAN,	JSDOCSTR("question")
	,JSDOCSTR("NO/yes question")
	,310
	},		
	{"mnemonics",		js_mnemonics,		1, JSTYPE_VOID,		JSDOCSTR("text")
	,JSDOCSTR("print a mnemonics string, command keys highlighted with tilde (~) characters")
	,310
	},		
	{"clear",           js_clear,			0, JSTYPE_VOID,		JSDOCSTR("[attribute]")
	,JSDOCSTR("clear screen and home cursor, "
		"optionally (in v3.13b+) setting current attribute first")
	,310
	},
	{"home",            js_cursor_home,		0, JSTYPE_VOID,		JSDOCSTR("")
	,JSDOCSTR("send cursor to home position (x,y:1,1)")
	,311
	},
	{"clearline",       js_clearline,		0, JSTYPE_VOID,		JSDOCSTR("[attribute]")
	,JSDOCSTR("clear current line, "
		"optionally (in v3.13b+) setting current attribute first")
	,310
	},		
	{"cleartoeol",      js_cleartoeol,		0, JSTYPE_VOID,		JSDOCSTR("[attribute]")
	,JSDOCSTR("clear to end-of-line (ANSI), "
		"optionally (in v3.13b+) setting current attribute first")
	,311
	},		
	{"crlf",            js_crlf,			0, JSTYPE_VOID,		JSDOCSTR("")
	,JSDOCSTR("output a carriage-return/line-feed pair (new-line)")
	,310
	},		
	{"pause",			js_pause,			0, JSTYPE_VOID,		JSDOCSTR("")
	,JSDOCSTR("display pause prompt and wait for key hit")
	,310
	},
	{"beep",			js_beep,			1, JSTYPE_VOID,		JSDOCSTR("[count=<tt>1</tt>]")
	,JSDOCSTR("beep for count number of times (default count is 1)")
	,311
	},
	{"print",			js_print,			1, JSTYPE_VOID,		JSDOCSTR("text")
	,JSDOCSTR("display a string (supports Ctrl-A codes)")
	,310
	},		
	{"write",			js_write,			1, JSTYPE_VOID,		JSDOCSTR("text")
	,JSDOCSTR("display a raw string")
	,310
	},		
	{"putmsg",			js_putmsg,			1, JSTYPE_VOID,		JSDOCSTR("text [,mode=<tt>P_NONE</tt>]")
	,JSDOCSTR("display message text (Ctrl-A codes, @-codes, pipe codes, etc), "
		"see <tt>P_*</tt> in <tt>sbbsdefs.js</tt> for <i>mode</i> bits")
	,310
	},		
	{"center",			js_center,			1, JSTYPE_VOID,		JSDOCSTR("text")
	,JSDOCSTR("display a string centered on the screen")
	,310
	},
	{"strlen",			js_strlen,			1, JSTYPE_NUMBER,	JSDOCSTR("text")
	,JSDOCSTR("returns the number of characters in text, excluding Ctrl-A codes")
	,310
	},
	{"printfile",		js_printfile,		1, JSTYPE_VOID,		JSDOCSTR("filename [,mode=<tt>P_NONE</tt>]")
	,JSDOCSTR("print a message text file with optional mode")
	,310
	},		
	{"printtail",		js_printtail,		2, JSTYPE_VOID,		JSDOCSTR("filename, lines [,mode=<tt>P_NONE</tt>]")
	,JSDOCSTR("print last x lines of file with optional mode")
	,310
	},		
	{"editfile",		js_editfile,		1, JSTYPE_VOID,		JSDOCSTR("filename")
	,JSDOCSTR("edit/create a text file using the user's preferred message editor")
	,310
	},		
	{"uselect",			js_uselect,			0, JSTYPE_NUMBER,	JSDOCSTR("[number, title, item] [,ars]")
	,JSDOCSTR("user selection menu, call for each item, then with no args to display select menu")
	,312
	},		
	{"saveline",		js_saveline,		0, JSTYPE_VOID,		JSDOCSTR("")
	,JSDOCSTR("save last output line")
	,310
	},		
	{"restoreline",		js_restoreline,		0, JSTYPE_VOID,		JSDOCSTR("")
	,JSDOCSTR("restore last output line")
	,310
	},		
	{"ansi",			js_ansi,			1, JSTYPE_STRING,	JSDOCSTR("attribute_number")
	,JSDOCSTR("returns ANSI encoding of specified <i>attribute_number</i>")
	,310
	},		
	{"ansi_save",		js_pushxy,			0, JSTYPE_ALIAS	},
	{"ansi_pushxy",		js_pushxy,			0, JSTYPE_ALIAS	},
	{"pushxy",			js_pushxy,			0, JSTYPE_VOID,		JSDOCSTR("")
	,JSDOCSTR("save current cursor position (AKA ansi_save)")
	,311
	},
	{"ansi_restore",	js_popxy,			0, JSTYPE_ALIAS },
	{"ansi_popxy",		js_popxy,			0, JSTYPE_ALIAS },
	{"popxy",			js_popxy,			0, JSTYPE_VOID,		JSDOCSTR("")
	,JSDOCSTR("restore saved cursor position (AKA ansi_restore)")
	,311
	},
	{"ansi_gotoxy",		js_gotoxy,			1, JSTYPE_ALIAS },
	{"gotoxy",			js_gotoxy,			1, JSTYPE_VOID,		JSDOCSTR("[x,y] or [object { x,y }]")
	,JSDOCSTR("move cursor to a specific screen coordinate (ANSI), "
	"arguments can be separate x and y cooridinates or an object with x and y properites "
	"(like that returned from <tt>console.getxy()</tt>)")
	,311
	},
	{"ansi_up",			js_cursor_up,		0, JSTYPE_ALIAS },
	{"up",				js_cursor_up,		0, JSTYPE_VOID,		JSDOCSTR("[rows=<tt>1</tt>]")
	,JSDOCSTR("move cursor up one or more rows (ANSI)")
	,311
	},
	{"ansi_down",		js_cursor_down,		0, JSTYPE_ALIAS },
	{"down",			js_cursor_down,		0, JSTYPE_VOID,		JSDOCSTR("[rows=<tt>1</tt>]")
	,JSDOCSTR("move cursor down one or more rows (ANSI)")
	,311
	},
	{"ansi_right",		js_cursor_right,	0, JSTYPE_ALIAS },
	{"right",			js_cursor_right,	0, JSTYPE_VOID,		JSDOCSTR("[columns=<tt>1</tt>]")
	,JSDOCSTR("move cursor right one or more columns (ANSI)")
	,311
	},
	{"ansi_left",		js_cursor_left,		0, JSTYPE_ALIAS },
	{"left",			js_cursor_left,		0, JSTYPE_VOID,		JSDOCSTR("[columns=<tt>1</tt>]")
	,JSDOCSTR("move cursor left one or more columns (ANSI)")
	,311
	},
	{"ansi_getlines",	js_getlines,		0, JSTYPE_ALIAS },
	{"getlines",		js_getlines,		0, JSTYPE_VOID,		JSDOCSTR("")
	,JSDOCSTR("auto-detect the number of rows/lines on the user's terminal (ANSI)")
	,311
	},
	{"ansi_getxy",		js_getxy,			0, JSTYPE_ALIAS },
	{"getxy",			js_getxy,			0, JSTYPE_OBJECT,	JSDOCSTR("")
	,JSDOCSTR("returns the current cursor position as an object (with x and y properties)")
	,311
	},
	{"lock_input",		js_lock_input,		1, JSTYPE_VOID,		JSDOCSTR("[lock=<tt>true</tt>]")
	,JSDOCSTR("lock the user input thread (allowing direct client socket access)")
	,310
	},
	{"telnet_cmd",		js_telnet_cmd,		2, JSTYPE_VOID,		JSDOCSTR("command [,option=<tt>0</tt>]")
	,JSDOCSTR("send Telnet command (with optional command option) to remote client")
	,310
	},
	{"handle_ctrlkey",	js_handle_ctrlkey,	1, JSTYPE_BOOLEAN,	JSDOCSTR("key [,mode=<tt>K_NONE</tt>]")
	,JSDOCSTR("call internal control key handler for specified control key, returns <tt>true</tt> if handled")
	,311
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
	JSObject*	obj;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(NULL);

	if((obj=JS_DefineObject(cx, parent, "console", &js_console_class, NULL
		,JSPROP_ENUMERATE|JSPROP_READONLY))==NULL)
		return(NULL);

	if(!js_DefineSyncProperties(cx, obj, js_console_properties))
		return(NULL);

	if (!js_DefineSyncMethods(cx, obj, js_console_functions, FALSE)) 
		return(NULL);

	/* Create an array of pre-defined colors */

	JSObject* color_list;

	if((color_list=JS_NewArrayObject(cx,0,NULL))==NULL)
		return(NULL);

	if(!JS_DefineProperty(cx, obj, "color_list", OBJECT_TO_JSVAL(color_list)
		,NULL, NULL, 0))
		return(NULL);

	for(uint i=0;i<sbbs->cfg.total_colors;i++) {

		jsval val=INT_TO_JSVAL(sbbs->cfg.color[i]);
		if(!JS_SetElement(cx, color_list, i, &val))
			return(NULL);
	}	

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,obj,"Controls the user's Telnet/RLogin terminal",310);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", con_prop_desc, JSPROP_READONLY);
#endif

	return(obj);
}

#endif	/* JAVSCRIPT */
