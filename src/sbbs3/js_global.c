/* js_global.c */

/* Synchronet JavaScript "global" object properties/methods for all servers */

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

static JSBool
js_load(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		path[MAX_PATH+1];
    uintN		i;
    JSString*	str;
    const char*	filename;
    JSScript*	script;
    JSBool		ok;
    jsval		result;
	scfg_t*		cfg;

	if((cfg=(scfg_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_FALSE;

    for (i = 0; i < argc; i++) {
		str = JS_ValueToString(cx, argv[i]);
		if (!str)
			return JS_FALSE;
		argv[i] = STRING_TO_JSVAL(str);
		filename = JS_GetStringBytes(str);
		errno = 0;
		if(!strchr(filename,BACKSLASH))
			sprintf(path,"%s%s",cfg->exec_dir,filename);
		else
			strcpy(path,filename);
		script = JS_CompileFile(cx, obj, path);
		if (!script)
			ok = JS_FALSE;
		else {
			ok = JS_ExecuteScript(cx, obj, script, &result);
			JS_DestroyScript(cx, script);
			}
		if (!ok)
			return JS_FALSE;
    }

    return JS_TRUE;
}

static JSBool
js_format(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
    uintN		i;
	JSString *	fmt;
    JSString *	str;
	va_list		arglist[64];

	fmt = JS_ValueToString(cx, argv[0]);
	if (!fmt)
		return JS_FALSE;

    for (i = 1; i < argc && i<sizeof(arglist)/sizeof(arglist[0]); i++) {
		if(JSVAL_IS_STRING(argv[i])) {
			str = JS_ValueToString(cx, argv[i]);
			if (!str)
			    return JS_FALSE;
			arglist[i-1]=JS_GetStringBytes(str);
		} else if(JSVAL_IS_INT(argv[i]) || JSVAL_IS_BOOLEAN(argv[i]))
			arglist[i-1]=(char *)JSVAL_TO_INT(argv[i]);
		else
			arglist[i-1]=NULL;
	}
	
	if((p=JS_vsmprintf(JS_GetStringBytes(fmt),(char*)arglist))==NULL)
		return(JS_FALSE);

	str = JS_NewStringCopyZ(cx, p);
	JS_smprintf_free(p);

	*rval = STRING_TO_JSVAL(str);

    return JS_TRUE;
}

static JSBool
js_mswait(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int val=1;

	if(argc)
		val=JSVAL_TO_INT(argv[0]);
	mswait(val);

	return(JS_TRUE);
}

static JSBool
js_random(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	*rval = INT_TO_JSVAL(sbbs_random(JSVAL_TO_INT(argv[0])));
	return(JS_TRUE);
}

static JSBool
js_time(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	*rval = INT_TO_JSVAL(time(NULL));
	return(JS_TRUE);
}


static JSBool
js_beep(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int freq=500;
	int	dur=500;

	if(argc)
		freq=JSVAL_TO_INT(argv[0]);
	if(argc>1)
		dur=JSVAL_TO_INT(argv[1]);

	sbbs_beep(freq,dur);

	return(JS_TRUE);
}

static JSBool
js_exit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return(JS_FALSE);
}

static JSBool
js_crc16(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		str;
	JSString*	js_str;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) 
		return(JS_FALSE);

	if((str=JS_GetStringBytes(js_str))==NULL) 
		return(JS_FALSE);

	*rval = INT_TO_JSVAL(crc16(str));
	return(JS_TRUE);
}

static JSBool
js_crc32(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		str;
	JSString*	js_str;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) 
		return(JS_FALSE);

	if((str=JS_GetStringBytes(js_str))==NULL) 
		return(JS_FALSE);

	*rval = INT_TO_JSVAL(crc32(str,strlen(str)));
	return(JS_TRUE);
}

static JSBool
js_chksum(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	ulong		sum=0;
	char*		p;
	JSString*	js_str;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) 
		return(JS_FALSE);

	if((p=JS_GetStringBytes(js_str))==NULL) 
		return(JS_FALSE);

	while(*p) sum+=*(p++);

	*rval = INT_TO_JSVAL(sum);
	return(JS_TRUE);
}

static JSBool
js_ascii(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char		str[2];
	JSString*	js_str;

	if(JSVAL_IS_STRING(argv[0])) {	/* string to ascii-int */
		if((js_str=JS_ValueToString(cx, argv[0]))==NULL) 
			return(JS_FALSE);

		if((p=JS_GetStringBytes(js_str))==NULL) 
			return(JS_FALSE);

		*rval = INT_TO_JSVAL(*p);
		return(JS_TRUE);
	}

	/* ascii-int to str */
	str[0]=(uchar)JSVAL_TO_INT(argv[0]);
	str[1]=0;

	js_str = JS_NewStringCopyZ(cx, str);

	*rval = STRING_TO_JSVAL(js_str);

	return(JS_TRUE);
}

static JSBool
js_strip_ctrl(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	ulong		sum=0;
	char*		p;
	JSString*	js_str;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) 
		return(JS_FALSE);

	if((p=JS_GetStringBytes(js_str))==NULL) 
		return(JS_FALSE);

	strip_ctrl(p);

	js_str = JS_NewStringCopyZ(cx, p);

	*rval = STRING_TO_JSVAL(js_str);

	return(JS_TRUE);
}

static JSClass js_global_class ={
        "Global",
		JSCLASS_HAS_PRIVATE, /* needed for scfg_t ptr */
        JS_PropertyStub,JS_PropertyStub,JS_PropertyStub,JS_PropertyStub, 
        JS_EnumerateStub,JS_ResolveStub,JS_ConvertStub,JS_FinalizeStub 
    }; 

static JSFunctionSpec js_global_functions[] = {
	{"exit",			js_exit,			0},		/* stop execution */
	{"load",            js_load,            1},		/* Load and execute a javascript file */
	{"format",			js_format,			1},		/* return a formatted string (ala printf) */
	{"mswait",			js_mswait,			0},		/* millisecond wait/sleep routine */
	{"sleep",			js_mswait,			0},		/* millisecond wait/sleep routine */
	{"random",			js_random,			1},		/* return random int between 0 and n */
	{"time",			js_time,			0},		/* return time in Unix format */
	{"beep",			js_beep,			0},		/* local beep (freq, dur) */
	{"crc16",			js_crc16,			1},		/* calculate 16-bit CRC of string */
	{"crc32",			js_crc32,			1},		/* calculate 32-bit CRC of string */
	{"chksum",			js_chksum,			1},		/* calculate 32-bit chksum of string */
	{"ascii",			js_ascii,			1},		/* convert str to ascii-val or vice-versa */
	{"strip_ctrl",		js_strip_ctrl,		1},		/* strip ctrl chars from string */
	{0}
};

JSObject* DLLCALL js_CreateGlobalObject(JSContext* cx, scfg_t* cfg)
{
	JSObject*	glob;

	if((glob = JS_NewObject(cx, &js_global_class, NULL, NULL)) ==NULL)
		return(NULL);

	if (!JS_InitStandardClasses(cx, glob))
		return(NULL);

	if (!JS_DefineFunctions(cx, glob, js_global_functions)) 
		return(NULL);

	if(!JS_SetPrivate(cx, glob, cfg))	/* Store a pointer to scfg_t */
		return(NULL);

	return(glob);
}

#endif	/* JAVSCRIPT */