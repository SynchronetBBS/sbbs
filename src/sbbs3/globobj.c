/* globobj.c */

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
	char		tmp[1024];
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
		} else if(JSVAL_IS_INT(argv[i]))
			arglist[i-1]=(char *)JSVAL_TO_INT(argv[i]);
		else
			arglist[i-1]=NULL;
	}
	
	vsprintf(tmp,JS_GetStringBytes(fmt),(char*)arglist);

	str = JS_NewStringCopyZ(cx, tmp);
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
	{"beep",			js_beep,			0},		/* local beep (freq, dur) */
	{0}
};

JSObject* DLLCALL js_CreateGlobalObject(scfg_t* cfg, JSContext* cx)
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