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

/* Global Object Properites */
enum {
	 GLOB_PROP_ERRNO
	,GLOB_PROP_ERRNO_STR
};

static JSBool js_system_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint       tiny;

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case GLOB_PROP_ERRNO:
	        *vp = INT_TO_JSVAL(errno);
			break;
		case GLOB_PROP_ERRNO_STR:
	        *vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, strerror(errno)));
			break;
	}
	return(TRUE);
}

#define GLOBOBJ_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static struct JSPropertySpec js_global_properties[] = {
/*		 name,		tinyid,				flags,				getter,	setter	*/

	{	"errno",	GLOB_PROP_ERRNO,	GLOBOBJ_FLAGS,		NULL,	NULL },
	{	"errno_str",GLOB_PROP_ERRNO_STR,GLOBOBJ_FLAGS,		NULL,	NULL },
	{0}
};

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
	JSObject*	js_argv;

	*rval=JSVAL_VOID;

	if((cfg=(scfg_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if(argc<1)
		return(JS_TRUE);

	if(argc>1) {

		if((js_argv=JS_NewArrayObject(cx, 0, NULL)) == NULL)
			return(JS_FALSE);

		for(i=1; i<argc; i++)
			JS_SetElement(cx, js_argv, i-1, &argv[i]);

		JS_DefineProperty(cx, obj, "argv", OBJECT_TO_JSVAL(js_argv)
			,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);
		JS_DefineProperty(cx, obj, "argc", INT_TO_JSVAL(argc-1)
			,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);
	}

	if((str=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);
	if((filename=JS_GetStringBytes(str))==NULL)
		return(JS_FALSE);

	errno = 0;
	if(!strchr(filename,BACKSLASH))
		sprintf(path,"%s%s",cfg->exec_dir,filename);
	else
		strcpy(path,filename);
	if((script=JS_CompileFile(cx, obj, path))==NULL)
		return(JS_FALSE);

	ok = JS_ExecuteScript(cx, obj, script, &result);
	JS_DestroyScript(cx, script);
	if (!ok)
		return(JS_FALSE);

    return(JS_TRUE);
}

static JSBool
js_format(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
    uintN		i;
	JSString *	fmt;
    JSString *	str;
	va_list		arglist[64];

	if((fmt=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	memset(arglist,0,sizeof(arglist));	/* Initialize arglist to NULLs */

    for (i = 1; i < argc && i<sizeof(arglist)/sizeof(arglist[0]); i++) {
		if(JSVAL_IS_STRING(argv[i])) {
			if((str=JS_ValueToString(cx, argv[i]))==NULL)
			    return(JS_FALSE);
			arglist[i-1]=JS_GetStringBytes(str);	/* exception here July-29-2002 */
		}
		else if(JSVAL_IS_DOUBLE(argv[i]))
			arglist[i-1]=(char*)(unsigned long)*JSVAL_TO_DOUBLE(argv[i]);
		else if(JSVAL_IS_INT(argv[i]) || JSVAL_IS_BOOLEAN(argv[i]))
			arglist[i-1]=(char *)JSVAL_TO_INT(argv[i]);
		else
			arglist[i-1]=NULL;
	}
	
	if((p=JS_vsmprintf(JS_GetStringBytes(fmt),(char*)arglist))==NULL)
		return(JS_FALSE);

	str = JS_NewStringCopyZ(cx, p);
	JS_smprintf_free(p);

	*rval = STRING_TO_JSVAL(str);
    return(JS_TRUE);
}

static JSBool
js_mswait(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int val=1;

	if(argc)
		val=JSVAL_TO_INT(argv[0]);
	mswait(val);

	*rval = JSVAL_VOID;
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
	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_exit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JS_ClearPendingException(cx);
	*rval = JSVAL_VOID;
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

		*rval=INT_TO_JSVAL(*p);
		return(JS_TRUE);
	}

	/* ascii-int to str */
	str[0]=(uchar)JSVAL_TO_INT(argv[0]);
	str[1]=0;

	js_str = JS_NewStringCopyZ(cx, str);
	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static char* dupestr(char* str)
{
	char* p;

	p = (char*)malloc(strlen(str)+1);

	if(p == NULL)
		return(NULL);

	return(strcpy(p,str));
}

static JSBool
js_ascii_str(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char*		str;
	JSString*	js_str;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) 
		return(JS_FALSE);

	if((str=JS_GetStringBytes(js_str))==NULL) 
		return(JS_FALSE);

	if((p=dupestr(str))==NULL)
		return(JS_FALSE);

	ascii_str(p);

	js_str = JS_NewStringCopyZ(cx, p);
	free(p);
	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}


static JSBool
js_strip_ctrl(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char*		str;
	JSString*	js_str;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) 
		return(JS_FALSE);

	if((str=JS_GetStringBytes(js_str))==NULL) 
		return(JS_FALSE);

	if((p=dupestr(str))==NULL)
		return(JS_FALSE);

	strip_ctrl(p);

	js_str = JS_NewStringCopyZ(cx, p);
	free(p);
	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_strip_exascii(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char*		str;
	JSString*	js_str;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) 
		return(JS_FALSE);

	if((str=JS_GetStringBytes(js_str))==NULL) 
		return(JS_FALSE);

	if((p=dupestr(str))==NULL)
		return(JS_FALSE);

	strip_exascii(p);

	js_str = JS_NewStringCopyZ(cx, p);
	free(p);
	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_truncsp(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char*		str;
	JSString*	js_str;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) 
		return(JS_FALSE);

	if((str=JS_GetStringBytes(js_str))==NULL) 
		return(JS_FALSE);

	if((p=dupestr(str))==NULL)
		return(JS_FALSE);

	truncsp(p);

	js_str = JS_NewStringCopyZ(cx, p);
	free(p);
	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_truncstr(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char*		str;
	char*		set;
	JSString*	js_str;
	JSString*	js_set;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) 
		return(JS_FALSE);

	if((js_set=JS_ValueToString(cx, argv[1]))==NULL) 
		return(JS_FALSE);

	if((str=JS_GetStringBytes(js_str))==NULL) 
		return(JS_FALSE);

	if((set=JS_GetStringBytes(js_set))==NULL) 
		return(JS_FALSE);


	if((p=dupestr(str))==NULL)
		return(JS_FALSE);

	truncstr(p,set);

	js_str = JS_NewStringCopyZ(cx, p);
	free(p);
	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}


static JSBool
js_fexist(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	JSString*	js_str;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	if((p=JS_GetStringBytes(js_str))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	*rval = BOOLEAN_TO_JSVAL(fexist(p));
	return(JS_TRUE);
}

static JSBool
js_remove(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	JSString*	js_str;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	if((p=JS_GetStringBytes(js_str))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	*rval = BOOLEAN_TO_JSVAL(remove(p)==0);
	return(JS_TRUE);
}


static JSBool
js_isdir(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	JSString*	js_str;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	if((p=JS_GetStringBytes(js_str))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	*rval = BOOLEAN_TO_JSVAL(isdir(p));
	return(JS_TRUE);
}

static JSBool
js_fattr(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	JSString*	js_str;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		*rval = INT_TO_JSVAL(-1);
		return(JS_TRUE);
	}

	if((p=JS_GetStringBytes(js_str))==NULL) {
		*rval = INT_TO_JSVAL(-1);
		return(JS_TRUE);
	}

	*rval = INT_TO_JSVAL(getfattr(p));
	return(JS_TRUE);
}

static JSBool
js_fdate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	JSString*	js_str;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		*rval = INT_TO_JSVAL(-1);
		return(JS_TRUE);
	}

	if((p=JS_GetStringBytes(js_str))==NULL) {
		*rval = INT_TO_JSVAL(-1);
		return(JS_TRUE);
	}

	*rval = INT_TO_JSVAL(fdate(p));
	return(JS_TRUE);
}

static JSBool
js_flength(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	JSString*	js_str;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		*rval = INT_TO_JSVAL(-1);
		return(JS_TRUE);
	}

	if((p=JS_GetStringBytes(js_str))==NULL) {
		*rval = INT_TO_JSVAL(-1);
		return(JS_TRUE);
	}

	*rval = INT_TO_JSVAL(flength(p));
	return(JS_TRUE);
}

		
static JSBool
js_sound(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	JSString*	js_str;

	if(!argc) {	/* Stop playing sound */
#ifdef _WIN32
		PlaySound(NULL,NULL,0);
#endif
		*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
		return(JS_TRUE);
	}

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	if((p=JS_GetStringBytes(js_str))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

#ifdef _WIN32
	*rval = BOOLEAN_TO_JSVAL(PlaySound(p, NULL, SND_ASYNC|SND_FILENAME));
#else
	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
#endif

	return(JS_TRUE);
}

static JSBool
js_directory(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int			i;
	int32		flags=GLOB_MARK;
	char*		p;
	glob_t		g;
	JSObject*	array;
	JSString*	js_str;
    jsint       len=0;
	jsval		val;

	*rval = JSVAL_NULL;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) 
		return(JS_TRUE);

	if((p=JS_GetStringBytes(js_str))==NULL) 
		return(JS_TRUE);

	if(argc>1)
		JS_ValueToInt32(cx,argv[1],&flags);

    if((array = JS_NewArrayObject(cx, 0, NULL))==NULL)
		return(JS_FALSE);

	glob(p,flags,NULL,&g);
	for(i=0;i<(int)g.gl_pathc;i++) {
		val=STRING_TO_JSVAL(JS_NewStringCopyZ(cx,g.gl_pathv[i]));
        if(!JS_SetElement(cx, array, len++, &val))
			break;
	}
	globfree(&g);

    *rval = OBJECT_TO_JSVAL(array);

    return(JS_TRUE);
}

static JSBool
js_mkdir(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	JSString*	js_str;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		*rval = INT_TO_JSVAL(-1);
		return(JS_TRUE);
	}

	if((p=JS_GetStringBytes(js_str))==NULL) {
		*rval = INT_TO_JSVAL(-1);
		return(JS_TRUE);
	}

	*rval = BOOLEAN_TO_JSVAL(MKDIR(p)==0);
	return(JS_TRUE);
}

static JSBool
js_rmdir(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	JSString*	js_str;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		*rval = INT_TO_JSVAL(-1);
		return(JS_TRUE);
	}

	if((p=JS_GetStringBytes(js_str))==NULL) {
		*rval = INT_TO_JSVAL(-1);
		return(JS_TRUE);
	}

	*rval = BOOLEAN_TO_JSVAL(rmdir(p)==0);
	return(JS_TRUE);
}


static JSBool
js_strftime(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		str[128];
	char*		fmt;
	time_t		t;
	struct tm*	tm_p;

	fmt=JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
	t=JSVAL_TO_INT(argv[1]);

	strcpy(str,"-Invalid time-");
	tm_p=localtime(&t);
	if(tm_p)
		strftime(str,sizeof(str),fmt,tm_p);

	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, str));
	return(JS_TRUE);
}
	
static JSClass js_global_class = {
     "Global"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_system_get			/* getProperty	*/
	,JS_PropertyStub		/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
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
	{"sound",			js_sound,			0},		/* play sound file */
	{"crc16",			js_crc16,			1},		/* calculate 16-bit CRC of string */
	{"crc32",			js_crc32,			1},		/* calculate 32-bit CRC of string */
	{"chksum",			js_chksum,			1},		/* calculate 32-bit chksum of string */
	{"ascii",			js_ascii,			1},		/* convert str to ascii-val or vice-versa */
	{"ascii_str",		js_ascii_str,		1},		/* convert ex-ascii in str to plain ascii */
	{"strip_ctrl",		js_strip_ctrl,		1},		/* strip ctrl chars from string */
	{"strip_exascii",	js_strip_exascii,	1},		/* strip ex-ascii chars from string */
	{"truncsp",			js_truncsp,			1},		/* truncate space off end of string */
	{"truncstr",		js_truncstr,		2},		/* truncate string at first char in set */
	{"file_exists",		js_fexist,			1},		/* verify file existence */
	{"file_remove",		js_remove,			1},		/* delete a file */
	{"file_isdir",		js_isdir,			1},		/* check if directory */
	{"file_attrib",		js_fattr,			1},		/* get file mode/attributes */
	{"file_date",		js_fdate,			1},		/* get file last modified date/time */
	{"file_size",		js_flength,			1},		/* get file length (in bytes) */
	{"directory",		js_directory,		1},		/* get directory listing (pattern, flags) */
	{"mkdir",			js_mkdir,			1},		/* make directory */
	{"rmdir",			js_rmdir,			1},		/* remove directory */
	{"strftime",		js_strftime,		2},		/* format time string */
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

	if(!JS_DefineProperties(cx, glob, js_global_properties))
		return(NULL);

	if(!JS_SetPrivate(cx, glob, cfg))	/* Store a pointer to scfg_t */
		return(NULL);

	return(glob);
}

#endif	/* JAVSCRIPT */
