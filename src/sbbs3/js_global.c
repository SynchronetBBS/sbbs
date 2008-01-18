/* js_global.c */

/* Synchronet JavaScript "global" object properties/methods for all servers */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2007 Rob Swindell - http://www.synchro.net/copyright.html		*
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
 *												
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#define JS_THREADSAFE	/* needed for JS_SetContextThread */

#include "sbbs.h"
#include "md5.h"
#include "base64.h"
#include "htmlansi.h"
#include "ini_file.h"

/* SpiderMonkey: */
#include <jsfun.h>

#define MAX_ANSI_SEQ	16
#define MAX_ANSI_PARAMS	8

#ifdef JAVASCRIPT

typedef struct {
	scfg_t				*cfg;
	jsSyncMethodSpec	*methods;
} private_t;

/* Global Object Properites */
enum {
	 GLOB_PROP_ERRNO
	,GLOB_PROP_ERRNO_STR
	,GLOB_PROP_SOCKET_ERRNO
};

static JSBool js_system_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint       tiny;
	JSString*	js_str;

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case GLOB_PROP_SOCKET_ERRNO:
			JS_NewNumberValue(cx,ERROR_VALUE,vp);
			break;
		case GLOB_PROP_ERRNO:
			JS_NewNumberValue(cx,errno,vp);
			break;
		case GLOB_PROP_ERRNO_STR:
			if((js_str=JS_NewStringCopyZ(cx, strerror(errno)))==NULL)
				return(JS_FALSE);
	        *vp = STRING_TO_JSVAL(js_str);
			break;
	}
	return(JS_TRUE);
}

#define GLOBOBJ_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_SHARED

static jsSyncPropertySpec js_global_properties[] = {
/*		 name,			tinyid,					flags,			ver */

	{	"errno"			,GLOB_PROP_ERRNO		,GLOBOBJ_FLAGS, 310 },
	{	"errno_str"		,GLOB_PROP_ERRNO_STR	,GLOBOBJ_FLAGS, 310 },
	{	"socket_errno"	,GLOB_PROP_SOCKET_ERRNO	,GLOBOBJ_FLAGS, 310 },
	{0}
};

typedef struct {
	JSRuntime*		runtime;
	JSContext*		cx;
	JSContext*		parent_cx;
	JSObject*		obj;
	JSScript*		script;
	msg_queue_t*	msg_queue;
	js_branch_t		branch;
	JSErrorReporter error_reporter;
	JSNative		log;
} background_data_t;

static void background_thread(void* arg)
{
	background_data_t* bg = (background_data_t*)arg;
	jsval result=JSVAL_VOID;
	jsval exit_code;

	msgQueueAttach(bg->msg_queue);
	JS_SetContextThread(bg->cx);
	if(!JS_ExecuteScript(bg->cx, bg->obj, bg->script, &result)
		&& JS_GetProperty(bg->cx, bg->obj, "exit_code", &exit_code))
		result=exit_code;
	js_EvalOnExit(bg->cx, bg->obj, &bg->branch);
	js_enqueue_value(bg->cx, bg->msg_queue, result, NULL);
	JS_DestroyScript(bg->cx, bg->script);
	JS_DestroyContext(bg->cx);
	JS_DestroyRuntime(bg->runtime);
	free(bg);
}

static void
js_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
	background_data_t* bg;

	if((bg=(background_data_t*)JS_GetContextPrivate(cx))==NULL)
		return;

	/* Use parent's context private data */
	JS_SetContextPrivate(cx, JS_GetContextPrivate(bg->parent_cx));

	/* Call parent's error reporter */
	bg->error_reporter(cx, message, report);

	/* Restore our context private data */
	JS_SetContextPrivate(cx, bg);
}

static JSBool js_BranchCallback(JSContext *cx, JSScript* script)
{
	background_data_t* bg;

	if((bg=(background_data_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(bg->parent_cx!=NULL && !JS_IsRunning(bg->parent_cx)) 	/* die when parent dies */
		return(JS_FALSE);

	return js_CommonBranchCallback(cx,&bg->branch);
}

static JSBool
js_log(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSBool retval;
	background_data_t* bg;

	if((bg=(background_data_t*)JS_GetContextPrivate(cx))==NULL)
		return JS_FALSE;

	/* Use parent's context private data */
	JS_SetContextPrivate(cx, JS_GetContextPrivate(bg->parent_cx));

	/* Call parent's log() function */
	retval = bg->log(cx, obj, argc, argv, rval);

	/* Restore our context private data */
	JS_SetContextPrivate(cx, bg);

	return retval;
}

/* Create a new value in the new context with a value from the original context */
/* Note: objects (including arrays) not currently supported */
static jsval* js_CopyValue(JSContext* cx, jsval val, JSContext* new_cx, jsval* rval)
{
	*rval = JSVAL_VOID;

	if(cx==new_cx
		|| JSVAL_IS_BOOLEAN(val) 
		|| JSVAL_IS_NULL(val) 
		|| JSVAL_IS_VOID(val) 
		|| JSVAL_IS_INT(val))
		*rval = val;
	else if(JSVAL_IS_NUMBER(val)) {
		jsdouble	d;
		if(JS_ValueToNumber(cx,val,&d))
			JS_NewNumberValue(new_cx,d,rval);
	}
	else {
		JSString*	str;
		size_t		len;
		char*		p;

		if((p=js_ValueToStringBytes(cx,val,&len)) != NULL
			&& (str=JS_NewStringCopyN(new_cx,p,len)) != NULL)
			*rval=STRING_TO_JSVAL(str);
	}

	return rval;
}

static JSBool
js_load(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		path[MAX_PATH+1];
    uintN		i;
	uintN		argn=0;
    const char*	filename;
    JSScript*	script;
	private_t*	p;
	jsval		val;
	JSObject*	js_argv;
	JSObject*	exec_obj;
	JSContext*	exec_cx=cx;
	JSBool		success;
	JSBool		background=JS_FALSE;
	background_data_t* bg;

	*rval=JSVAL_VOID;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	exec_obj=JS_GetScopeChain(cx);

	if(JSVAL_IS_BOOLEAN(argv[argn]))
		background=JSVAL_TO_BOOLEAN(argv[argn++]);

	if(background) {

		if((bg=(background_data_t*)malloc(sizeof(background_data_t)))==NULL)
			return(JS_FALSE);
		memset(bg,0,sizeof(background_data_t));

		bg->parent_cx = cx;

		/* Setup default values for branch settings */
		bg->branch.limit=JAVASCRIPT_BRANCH_LIMIT;
		bg->branch.gc_interval=JAVASCRIPT_GC_INTERVAL;
		bg->branch.yield_interval=JAVASCRIPT_YIELD_INTERVAL;
		if(JS_GetProperty(cx, obj,"js",&val))	/* copy branch settings from parent */
			memcpy(&bg->branch,JS_GetPrivate(cx,JSVAL_TO_OBJECT(val)),sizeof(bg->branch));
		bg->branch.terminated=NULL;	/* could be bad pointer at any time */
		bg->branch.counter=0;
		bg->branch.gc_attempts=0;

		if((bg->runtime = JS_NewRuntime(JAVASCRIPT_MAX_BYTES))==NULL)
			return(JS_FALSE);

	    if((bg->cx = JS_NewContext(bg->runtime, JAVASCRIPT_CONTEXT_STACK))==NULL)
			return(JS_FALSE);

		if((bg->obj=js_CreateCommonObjects(bg->cx
				,p->cfg			/* common config */
				,NULL			/* node-specific config */
				,NULL			/* additional global methods */
				,0				/* uptime */
				,""				/* hostname */
				,""				/* socklib_desc */
				,&bg->branch	/* js */
				,NULL			/* client */
				,INVALID_SOCKET	/* client_socket */
				,NULL			/* server props */
				))==NULL) 
			return(JS_FALSE);

		bg->msg_queue = msgQueueInit(NULL,MSG_QUEUE_BIDIR);

		js_CreateQueueObject(bg->cx, bg->obj, "parent_queue", bg->msg_queue);

		/* Save parent's error reporter (for later use by our error reporter) */
		bg->error_reporter=JS_SetErrorReporter(cx,NULL);
		JS_SetErrorReporter(cx,bg->error_reporter);
		JS_SetErrorReporter(bg->cx,js_ErrorReporter);

		/* Set our branch callback (which calls the generic branch callback) */
		JS_SetContextPrivate(bg->cx, bg);
		JS_SetBranchCallback(bg->cx, js_BranchCallback);

		/* Save parent's 'log' function (for later use by our log function) */
		if(JS_GetProperty(cx, obj, "log", &val)) {
			JSFunction* func;
			if((func=JS_ValueToFunction(cx, val))!=NULL && !func->interpreted) {
				bg->log=func->u.native;
				JS_DefineFunction(bg->cx, bg->obj
					,"log", js_log, func->nargs, func->flags);
			}
		}

		exec_cx = bg->cx;
		exec_obj = bg->obj;
		
	} else if(JSVAL_IS_OBJECT(argv[argn])) {
		JSObject* tmp_obj=JSVAL_TO_OBJECT(argv[argn++]);
		if(!JS_ObjectIsFunction(cx,tmp_obj))	/* Scope specified */
			exec_obj=tmp_obj;
	}

	if(argn==argc) {
		JS_ReportError(cx,"no filename specified");
		return(JS_FALSE);
	}
	if((filename=js_ValueToStringBytes(cx, argv[argn++], NULL))==NULL)
		return(JS_FALSE);

	if(argc>argn || background) {

		if((js_argv=JS_NewArrayObject(exec_cx, 0, NULL)) == NULL)
			return(JS_FALSE);

		JS_DefineProperty(exec_cx, exec_obj, "argv", OBJECT_TO_JSVAL(js_argv)
			,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

		for(i=argn; i<argc; i++)
			JS_SetElement(exec_cx, js_argv, i-argn, js_CopyValue(cx,argv[i],exec_cx,&val));

		JS_DefineProperty(exec_cx, exec_obj, "argc", INT_TO_JSVAL(argc-argn)
			,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);
	}

	errno = 0;
	if(isfullpath(filename))
		strcpy(path,filename);
	else {
		sprintf(path,"%s%s",p->cfg->mods_dir,filename);
		if(p->cfg->mods_dir[0]==0 || !fexistcase(path))
			sprintf(path,"%s%s",p->cfg->exec_dir,filename);
	}

	JS_ClearPendingException(exec_cx);

	if((script=JS_CompileFile(exec_cx, exec_obj, path))==NULL)
		return(JS_FALSE);

	if(background) {

		bg->script = script;
		*rval = OBJECT_TO_JSVAL(js_CreateQueueObject(cx, obj, NULL, bg->msg_queue));
		success = _beginthread(background_thread,0,bg)!=-1;

	} else {

		success = JS_ExecuteScript(exec_cx, exec_obj, script, rval);
		JS_DestroyScript(exec_cx, script);
	}

    return(success);
}

static JSBool
js_format(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
    JSString*	str;

	if((p=js_sprintf(cx, 0, argc, argv))==NULL) {
		JS_ReportError(cx,"js_sprintf failed");
		return(JS_FALSE);
	}

	str = JS_NewStringCopyZ(cx, p);
	js_sprintf_free(p);

	if(str==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(str);
    return(JS_TRUE);
}

static JSBool
js_yield(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	BOOL forced=TRUE;

	if(argc)
		JS_ValueToBoolean(cx, argv[0], &forced);
	if(forced) {
		YIELD();
	} else {
		MAYBE_YIELD();
	}

	return(JS_TRUE);
}

static JSBool
js_mswait(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32 val=1;
	clock_t start=msclock();

	if(argc)
		JS_ValueToInt32(cx,argv[0],&val);
	mswait(val);

	JS_NewNumberValue(cx,msclock()-start,rval);

	return(JS_TRUE);
}

static JSBool
js_random(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32 val=100;

	if(argc)
		JS_ValueToInt32(cx,argv[0],&val);

	JS_NewNumberValue(cx,sbbs_random(val),rval);
	return(JS_TRUE);
}

static JSBool
js_time(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JS_NewNumberValue(cx,time(NULL),rval);
	return(JS_TRUE);
}


static JSBool
js_beep(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32 freq=500;
	int32 dur=500;

	if(argc)
		JS_ValueToInt32(cx,argv[0],&freq);
	if(argc>1)
		JS_ValueToInt32(cx,argv[1],&dur);

	sbbs_beep(freq,dur);
	return(JS_TRUE);
}

static JSBool
js_exit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	if(argc)
		JS_DefineProperty(cx, obj, "exit_code", argv[0]
			,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	return(JS_FALSE);
}

static JSBool
js_crc16(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	size_t		len;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=js_ValueToStringBytes(cx, argv[0], &len))==NULL)
		return(JS_FALSE);

	*rval = INT_TO_JSVAL(crc16(p,len));
	return(JS_TRUE);
}

static JSBool
js_crc32(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	size_t		len;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=js_ValueToStringBytes(cx, argv[0], &len))==NULL)
		return(JS_FALSE);

	JS_NewNumberValue(cx,crc32(p,len),rval);
	return(JS_TRUE);
}

static JSBool
js_chksum(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	ulong		sum=0;
	char*		p;
	size_t		len;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=js_ValueToStringBytes(cx, argv[0], &len))==NULL)
		return(JS_FALSE);

	while(len--) sum+=*(p++);

	JS_NewNumberValue(cx,sum,rval);
	return(JS_TRUE);
}

static JSBool
js_ascii(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char		str[2];
	int32		i=0;
	JSString*	js_str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if(JSVAL_IS_STRING(argv[0])) {	/* string to ascii-int */

		if((p=JS_GetStringBytes(JSVAL_TO_STRING(argv[0])))==NULL) 
			return(JS_FALSE);

		*rval=INT_TO_JSVAL(*p);
		return(JS_TRUE);
	}

	/* ascii-int to str */
	JS_ValueToInt32(cx,argv[0],&i);
	str[0]=(uchar)i;
	str[1]=0;

	if((js_str = JS_NewStringCopyZ(cx, str))==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_ctrl(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		ch;
	char*		p;
	char		str[2];
	int32		i=0;
	JSString*	js_str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if(JSVAL_IS_STRING(argv[0])) {	

		if((p=JS_GetStringBytes(JSVAL_TO_STRING(argv[0])))==NULL) 
			return(JS_FALSE);
		ch=*p;
	} else {
		JS_ValueToInt32(cx,argv[0],&i);
		ch=(char)i;
	}

	str[0]=toupper(ch)&~0x40;
	str[1]=0;

	if((js_str = JS_NewStringCopyZ(cx, str))==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_ascii_str(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char*		buf;
	JSString*	str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=js_ValueToStringBytes(cx, argv[0], NULL))==NULL)
		return(JS_FALSE);

	if((buf=strdup(p))==NULL)
		return(JS_FALSE);

	ascii_str(buf);

	str = JS_NewStringCopyZ(cx, buf);
	free(buf);
	if(str==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(str);
	return(JS_TRUE);
}


static JSBool
js_strip_ctrl(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char*		buf;
	JSString*	js_str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	if((buf=strdup(p))==NULL)
		return(JS_FALSE);

	strip_ctrl(buf);

	js_str = JS_NewStringCopyZ(cx, buf);
	free(buf);
	if(js_str==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_strip_exascii(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char*		buf;
	JSString*	js_str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	if((buf=strdup(p))==NULL)
		return(JS_FALSE);

	strip_exascii(buf);

	js_str = JS_NewStringCopyZ(cx, buf);
	free(buf);
	if(js_str==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_lfexpand(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	ulong		i,j;
	char*		inbuf;
	char*		outbuf;
	JSString*	str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((inbuf=js_ValueToStringBytes(cx, argv[0], NULL))==NULL)
		return(JS_FALSE);

	if((outbuf=(char*)malloc((strlen(inbuf)*2)+1))==NULL)
		return(JS_FALSE);

	for(i=j=0;inbuf[i];i++) {
		if(inbuf[i]=='\n' && (!i || inbuf[i-1]!='\r'))
			outbuf[j++]='\r';
		outbuf[j++]=inbuf[i];
	}
	outbuf[j]=0;

	str = JS_NewStringCopyZ(cx, outbuf);
	free(outbuf);
	if(str==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(str);
	return(JS_TRUE);
}

static int get_prefix(char *text, int *bytes, int *len, int maxlen)
{
	int		tmp_prefix_bytes,tmp_prefix_len;
	int		expect;
	int		depth;

	*bytes=0;
	*len=0;
	tmp_prefix_bytes=0;
	tmp_prefix_len=0;
	depth=0;
	expect=1;
	if(text[0]!=' ')
		expect=2;
	while(expect) {
		tmp_prefix_bytes++;
		/* Skip CTRL-A codes */
		while(text[tmp_prefix_bytes-1]=='\x01') {
			tmp_prefix_bytes++;
			if(text[tmp_prefix_bytes-1]=='\x01')
				break;
			tmp_prefix_bytes++;
		}
		tmp_prefix_len++;
		if(text[tmp_prefix_bytes-1]==0 || text[tmp_prefix_bytes-1]=='\n' || text[tmp_prefix_bytes-1]=='\r')
			break;
		switch(expect) {
			case 1:		/* At start of possible quote (Next char should be space) */
				if(text[tmp_prefix_bytes-1]!=' ')
					expect=0;
				else
					expect++;
				break;
			case 2:		/* At start of nick (next char should be alphanum or '>') */
			case 3:		/* At second nick initial (next char should be alphanum or '>') */
			case 4:		/* At third nick initial (next char should be alphanum or '>') */
				if(text[tmp_prefix_bytes-1]==' ' || text[tmp_prefix_bytes-1]==0)
					expect=0;
				else
					if(text[tmp_prefix_bytes-1]=='>')
						expect=6;
					else
						expect++;
				break;
			case 5:		/* After three regular chars, next HAS to be a '>') */
				if(text[tmp_prefix_bytes-1]!='>')
					expect=0;
				else
					expect++;
				break;
			case 6:		/* At '>' next char must be a space */
				if(text[tmp_prefix_bytes-1]!=' ')
					expect=0;
				else {
					expect=1;
					*len=tmp_prefix_len;
					*bytes=tmp_prefix_bytes;
					depth++;
					/* Some editors don't put double spaces in between */
					if(text[tmp_prefix_bytes]!=' ')
						expect++;
				}
				break;
			default:
				expect=0;
				break;
		}
	}
	if(*bytes >= maxlen) {
		lprintf(LOG_CRIT, "Prefix bytes %u is larger than buffer (%u) here: %*.*s",*bytes,maxlen,maxlen,maxlen,text);
		*bytes=maxlen-1;
	}
	return(depth);
}

static void outbuf_append(char **outbuf, char **outp, char *append, int len, int *outlen)
{
	char	*p;

	/* Terminate outbuf */
	**outp=0;
	/* Check if there's room */
	if(*outp - *outbuf + len < *outlen) {
		memcpy(*outp, append, len);
		*outp+=len;
		return;
	}
	/* Not enough room, double the size. */
	*outlen *= 2;
	p=realloc(*outbuf, *outlen);
	if(p==NULL) {
		/* Can't do it. */
		*outlen/=2;
		return;
	}
	/* Set outp for new buffer */
	*outp=p+(*outp - *outbuf);
	*outbuf=p;
	memcpy(*outp, append, len);
	*outp+=len;
	return;
}

static JSBool
js_word_wrap(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		l,len=79;
	int32		oldlen=79;
	int32		crcount=0;
	JSBool		handle_quotes=JS_TRUE;
	long		i,k,t;
	int			ocol=1;
	int			icol=1;
	uchar*		inbuf;
	char*		outbuf;
	char*		outp;
	char*		linebuf;
	char*		prefix=NULL;
	int			prefix_len=0;
	int			prefix_bytes=0;
	int			quote_count=0;
	int			old_prefix_bytes=0;
	int			outbuf_size=0;
	JSString*	js_str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((inbuf=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	outbuf_size=strlen(inbuf)*3+1;
	if((outbuf=(char*)malloc(outbuf_size))==NULL)
		return(JS_FALSE);
	outp=outbuf;

	if(argc>1)
		JS_ValueToInt32(cx,argv[1],&len);

	if(argc>2)
		JS_ValueToInt32(cx,argv[2],&oldlen);

	if(argc>3 && JSVAL_IS_BOOLEAN(argv[3]))
		handle_quotes=JSVAL_TO_BOOLEAN(argv[3]);

	if((linebuf=(char*)alloca((len*2)+2))==NULL) /* room for ^A codes ToDo: This isn't actually "enough" room */
		return(JS_FALSE);

	if(handle_quotes) {
		if((prefix=(char *)alloca((len*2)+2))==NULL) /* room for ^A codes ToDo: This isn't actually "enough" room */
			return(JS_FALSE);
		prefix[0]=0;
	}

	outbuf[0]=0;
	/* Get prefix from the first line (ouch) */
	l=0;
	i=0;
	if(handle_quotes && (quote_count=get_prefix(inbuf, &prefix_bytes, &prefix_len, len*2+2))!=0) {
		i+=prefix_bytes;
		if(prefix_len>len/3*2) {
			/* This prefix is insane (more than 2/3rds of the new width) hack it down to size */
			/* Since we're hacking it, we will always end up with a hardcr on this line. */
			/* ToDo: Something prettier would be nice. */
			sprintf(prefix," %d> ",quote_count);
			prefix_len=strlen(prefix);
			prefix_bytes=strlen(prefix);
		}
		else {
			memcpy(prefix,inbuf,prefix_bytes);
			/* Terminate prefix */
			prefix[prefix_bytes]=0;
		}
		memcpy(linebuf,prefix,prefix_bytes);
		l=prefix_bytes;
		ocol=prefix_len+1;
		icol=prefix_len+1;
		old_prefix_bytes=prefix_bytes;
	}
	for(; inbuf[i]; i++) {
		if(l>=len*2+2) {
			l-=4;
			linebuf[l]=0;
			lprintf(LOG_CRIT, "Word wrap line buffer exceeded... munging line %s",linebuf);
		}
		switch(inbuf[i]) {
			case '\r':
				crcount++;
				break;
			case '\n':
				if(handle_quotes && (quote_count=get_prefix(inbuf+i+1, &prefix_bytes, &prefix_len, len*2+2))!=0) {
					/* Move the input pointer offset to the last char of the prefix */
					i+=prefix_bytes;
				}
				if(!inbuf[i+1]) {			/* EOF */
					linebuf[l++]='\r';
					linebuf[l++]='\n';
					outbuf_append(&outbuf, &outp, linebuf, l, &outbuf_size);
					l=0;
					ocol=1;
				}
				/* If there's a new prefix, it is a hardcr */
				else if(prefix_bytes != old_prefix_bytes || (memcmp(prefix,inbuf+i+1-prefix_bytes,prefix_bytes))) {
					if(prefix_len>len/3*2) {
						/* This prefix is insane (more than 2/3rds of the new width) hack it down to size */
						/* Since we're hacking it, we will always end up with a hardcr on this line. */
						/* ToDo: Something prettier would be nice. */
						sprintf(prefix," %d> ",quote_count);
						prefix_len=strlen(prefix);
						prefix_bytes=strlen(prefix);
					}
					else {
						memcpy(prefix,inbuf+i+1-prefix_bytes,prefix_bytes);
						/* Terminate prefix */
						prefix[prefix_bytes]=0;
					}
					linebuf[l++]='\r';
					linebuf[l++]='\n';
					outbuf_append(&outbuf, &outp, linebuf, l, &outbuf_size);
					memcpy(linebuf,prefix,prefix_bytes);
					l=prefix_bytes;
					ocol=prefix_len+1;
					old_prefix_bytes=prefix_bytes;
				}
				else if(isspace(inbuf[i+1])) {	/* Next line starts with whitespace.  This is a "hard" CR. */
					linebuf[l++]='\r';
					linebuf[l++]='\n';
					outbuf_append(&outbuf, &outp, linebuf, l, &outbuf_size);
					l=0;
					ocol=1;
				}
				else {
					if(icol < oldlen) {			/* If this line is overly long, It's impossible for the next word to fit */
						/* k will equal the length of the first word on the next line */
						for(k=0; inbuf[i+1+k] && (!isspace(inbuf[i+1+k])); k++);
						if(icol+k+1 < oldlen) {	/* The next word would have fit but isn't here.  Must be a hard CR */
							linebuf[l++]='\r';
							linebuf[l++]='\n';
							outbuf_append(&outbuf, &outp, linebuf, l, &outbuf_size);
							if(prefix)
								memcpy(linebuf,prefix,prefix_bytes);
							l=prefix_bytes;
							ocol=prefix_len+1;
						}
						else {		/* Not a hard CR... add space if needed */
							if(l<1 || !isspace(linebuf[l-1])) {
								linebuf[l++]=' ';
								ocol++;
							}
						}
					}
					else {			/* Not a hard CR... add space if needed */
						if(l<1 || !isspace(linebuf[l-1])) {
							linebuf[l++]=' ';
							ocol++;
						}
					}
				}
				icol=prefix_len+1;
				break;
			case '\x1f':	/* Delete... meaningless... strip. */
				break;
			case '\b':		/* Backspace... handle if possible, but don't go crazy. */
				if(l>0) {
					if(l>1 && linebuf[l-2]=='\x01') {
						if(linebuf[l-1]=='\x01') {
							ocol--;
							icol--;
						}
						l-=2;
					}
					else {
						l--;
						ocol--;
						icol--;
					}
				}
				break;
			case '\t':		/* TAB */
				linebuf[l++]=inbuf[i];
				/* Can't ever wrap on whitespace remember. */
				icol++;
				ocol++;
				while(ocol%8)
					ocol++;
				while(icol%8)
					icol++;
				break;
			case '\x01':	/* CTRL-A */
				linebuf[l++]=inbuf[i++];
				if(inbuf[i]!='\x01') {
					linebuf[l++]=inbuf[i];
					break;
				}
			default:
				linebuf[l++]=inbuf[i];
				ocol++;
				icol++;
				if(ocol>len && !isspace(inbuf[i])) {		/* Need to wrap here */
					/* Find the start of the last word */
					k=l;									/* Original next char */
					l--;									/* Move back to the last char */
					while((!isspace(linebuf[l])) && l>0)		/* Move back to the last non-space char */
						l--;
					if(l==0) {		/* Couldn't wrap... must chop. */
						l=k;
						while(l>1 && linebuf[l-2]=='\x01' && linebuf[l-1]!='\x01')
							l-=2;
						if(l>0 && linebuf[l-1]=='\x01')
							l--;
						if(l>0)
							l--;
					}
					t=l+1;									/* Store start position of next line */
					/* Move to start of whitespace */
					while(l>0 && isspace(l))
						l--;
					outbuf_append(&outbuf, &outp, linebuf, l+1, &outbuf_size);
					outbuf_append(&outbuf, &outp, "\r\n", 2, &outbuf_size);
					/* Move trailing words to start of buffer. */
					l=prefix_bytes;
					if(k-t>0)							/* k-1 is the last char position.  t is the start of the next line position */
						memmove(linebuf+l, linebuf+t, k-t);
					l+=k-t;
					/* Find new ocol */
					for(ocol=prefix_len+1,t=prefix_bytes; t<l; t++) {
						switch(linebuf[t]) {
							case '\x01':	/* CTRL-A */
								if(linebuf[t+1]!='\x01')
									break;
								t++;
								/* Fall-through */
							default:
								ocol++;
						}
					}
				}
		}
	}
	/* Trailing bits. */
	if(l) {
		linebuf[l++]='\r';
		linebuf[l++]='\n';
		outbuf_append(&outbuf, &outp, linebuf, l, &outbuf_size);
	}
	*outp=0;
	/* If there were no CRs in the input, strip all CRs */
	if(!crcount) {
		for(inbuf=outbuf; *inbuf; inbuf++) {
			if(*inbuf=='\r')
				memmove(inbuf, inbuf+1, strlen(inbuf));
		}
	}

	js_str = JS_NewStringCopyZ(cx, outbuf);
	free(outbuf);
	if(js_str==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_quote_msg(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		len=79;
	int			i,l,clen;
	uchar*		inbuf;
	char*		outbuf;
	char*		linebuf;
	char*		prefix=" > ";
	JSString*	js_str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((inbuf=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	if(argc>1)
		JS_ValueToInt32(cx,argv[1],&len);

	if(argc>2)
		prefix=js_ValueToStringBytes(cx, argv[2], NULL);

	if((outbuf=(char*)malloc((strlen(inbuf)*strlen(prefix))+1))==NULL)
		return(JS_FALSE);

	len-=strlen(prefix);
	if(len<=0)
		return(JS_FALSE);

	if((linebuf=(char*)alloca(len*2+2))==NULL)	/* (Hopefully) Room for ^A codes.  ToDo */
		return(JS_FALSE);

	outbuf[0]=0;
	clen=0;
	for(i=l=0;inbuf[i];i++) {
		if(l==0)	/* Do not use clen here since if the line starts with ^A, could stay at zero */
			strcat(outbuf,prefix);
		if(clen<len || inbuf[i]=='\n' || inbuf[i]=='\r')
			linebuf[l++]=inbuf[i];
		if(inbuf[i]=='\x01') {		/* Handle CTRL-A */
			linebuf[l++]=inbuf[++i];
			if(inbuf[i]=='\x01')
				clen++;
		}
		else
			clen++;
		/* ToDo: Handle TABs etc. */
		if(inbuf[i]=='\n') {
			strncat(outbuf,linebuf,l);
			l=0;
			clen=0;
		}
	}
	if(l)	/* remainder */
		strncat(outbuf,linebuf,l);

	js_str = JS_NewStringCopyZ(cx, outbuf);
	free(outbuf);
	if(js_str==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_netaddr_type(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*	str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((str=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	*rval = INT_TO_JSVAL(smb_netaddr_type(str));

	return(JS_TRUE);
}

static JSBool
js_rot13(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char*		str;
	JSString*	js_str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((str=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	if((p=strdup(str))==NULL)
		return(JS_FALSE);

	js_str = JS_NewStringCopyZ(cx, rot13(p));
	free(p);
	if(js_str==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

/* This table is used to convert between IBM ex-ASCII and HTML character entities */
/* Much of this table supplied by Deuce (thanks!) */
static struct { 
	int		value;
	char*	name;
} exasctbl[128] = {
/*  HTML val,name             ASCII  description */
	{ 199	,"Ccedil"	}, /* 128 C, cedilla */
	{ 252	,"uuml"		}, /* 129 u, umlaut */
	{ 233	,"eacute"	}, /* 130 e, acute accent */
	{ 226	,"acirc"	}, /* 131 a, circumflex accent */
	{ 228	,"auml"		}, /* 132 a, umlaut */
	{ 224	,"agrave"	}, /* 133 a, grave accent */
	{ 229	,"aring"	}, /* 134 a, ring */
	{ 231	,"ccedil"	}, /* 135 c, cedilla */
	{ 234	,"ecirc"	}, /* 136 e, circumflex accent */
	{ 235	,"euml"		}, /* 137 e, umlaut */
	{ 232	,"egrave"	}, /* 138 e, grave accent */
	{ 239	,"iuml"		}, /* 139 i, umlaut */
	{ 238	,"icirc"	}, /* 140 i, circumflex accent */
	{ 236	,"igrave"	}, /* 141 i, grave accent */
	{ 196	,"Auml"		}, /* 142 A, umlaut */
	{ 197	,"Aring"	}, /* 143 A, ring */
	{ 201	,"Eacute"	}, /* 144 E, acute accent */
	{ 230	,"aelig"	}, /* 145 ae ligature */
	{ 198	,"AElig"	}, /* 146 AE ligature */
	{ 244	,"ocirc"	}, /* 147 o, circumflex accent */
	{ 246	,"ouml"		}, /* 148 o, umlaut */
	{ 242	,"ograve"	}, /* 149 o, grave accent */
	{ 251	,"ucirc"	}, /* 150 u, circumflex accent */
	{ 249	,"ugrave"	}, /* 151 u, grave accent */
	{ 255	,"yuml"		}, /* 152 y, umlaut */
	{ 214	,"Ouml"		}, /* 153 O, umlaut */
	{ 220	,"Uuml"		}, /* 154 U, umlaut */
	{ 162	,"cent"		}, /* 155 Cent sign */
	{ 163	,"pound"	}, /* 156 Pound sign */
	{ 165	,"yen"		}, /* 157 Yen sign */
	{ 8359	,NULL		}, /* 158 Pt (unicode) */
	{ 402	,NULL		}, /* 402 Florin (non-standard alsi 159?) */
	{ 225	,"aacute"	}, /* 160 a, acute accent */
	{ 237	,"iacute"	}, /* 161 i, acute accent */
	{ 243	,"oacute"	}, /* 162 o, acute accent */
	{ 250	,"uacute"	}, /* 163 u, acute accent */
	{ 241	,"ntilde"	}, /* 164 n, tilde */
	{ 209	,"Ntilde"	}, /* 165 N, tilde */
	{ 170	,"ordf"		}, /* 166 Feminine ordinal */
	{ 186	,"ordm"		}, /* 167 Masculine ordinal */
	{ 191	,"iquest"	}, /* 168 Inverted question mark */
	{ 8976	,NULL		}, /* 169 Inverse "Not sign" (unicode) */
	{ 172	,"not"		}, /* 170 Not sign */
	{ 189	,"frac12"	}, /* 171 Fraction one-half */
	{ 188	,"frac14"	}, /* 172 Fraction one-fourth */
	{ 161	,"iexcl"	}, /* 173 Inverted exclamation point */
	{ 171	,"laquo"	}, /* 174 Left angle quote */
	{ 187	,"raquo"	}, /* 175 Right angle quote */
	{ 9617	,NULL		}, /* 176 drawing symbol (unicode) */
	{ 9618	,NULL		}, /* 177 drawing symbol (unicode) */
	{ 9619	,NULL		}, /* 178 drawing symbol (unicode) */
	{ 9474	,NULL		}, /* 179 drawing symbol (unicode) */
	{ 9508	,NULL		}, /* 180 drawing symbol (unicode) */
	{ 9569	,NULL		}, /* 181 drawing symbol (unicode) */
	{ 9570	,NULL		}, /* 182 drawing symbol (unicode) */
	{ 9558	,NULL		}, /* 183 drawing symbol (unicode) */
	{ 9557	,NULL		}, /* 184 drawing symbol (unicode) */
	{ 9571	,NULL		}, /* 185 drawing symbol (unicode) */
	{ 9553	,NULL		}, /* 186 drawing symbol (unicode) */
	{ 9559	,NULL		}, /* 187 drawing symbol (unicode) */
	{ 9565	,NULL		}, /* 188 drawing symbol (unicode) */
	{ 9564	,NULL		}, /* 189 drawing symbol (unicode) */
	{ 9563	,NULL		}, /* 190 drawing symbol (unicode) */
	{ 9488	,NULL		}, /* 191 drawing symbol (unicode) */
	{ 9492	,NULL		}, /* 192 drawing symbol (unicode) */
	{ 9524	,NULL		}, /* 193 drawing symbol (unicode) */
	{ 9516	,NULL		}, /* 194 drawing symbol (unicode) */
	{ 9500	,NULL		}, /* 195 drawing symbol (unicode) */
	{ 9472	,NULL		}, /* 196 drawing symbol (unicode) */
	{ 9532	,NULL		}, /* 197 drawing symbol (unicode) */
	{ 9566	,NULL		}, /* 198 drawing symbol (unicode) */
	{ 9567	,NULL		}, /* 199 drawing symbol (unicode) */
	{ 9562	,NULL		}, /* 200 drawing symbol (unicode) */
	{ 9556	,NULL		}, /* 201 drawing symbol (unicode) */
	{ 9577	,NULL		}, /* 202 drawing symbol (unicode) */
	{ 9574	,NULL		}, /* 203 drawing symbol (unicode) */
	{ 9568	,NULL		}, /* 204 drawing symbol (unicode) */
	{ 9552	,NULL		}, /* 205 drawing symbol (unicode) */
	{ 9580	,NULL		}, /* 206 drawing symbol (unicode) */
	{ 9575	,NULL		}, /* 207 drawing symbol (unicode) */
	{ 9576	,NULL		}, /* 208 drawing symbol (unicode) */
	{ 9572	,NULL		}, /* 209 drawing symbol (unicode) */
	{ 9573	,NULL		}, /* 210 drawing symbol (unicode) */
	{ 9561	,NULL		}, /* 211 drawing symbol (unicode) */
	{ 9560	,NULL		}, /* 212 drawing symbol (unicode) */
	{ 9554	,NULL		}, /* 213 drawing symbol (unicode) */
	{ 9555	,NULL		}, /* 214 drawing symbol (unicode) */
	{ 9579	,NULL		}, /* 215 drawing symbol (unicode) */
	{ 9578	,NULL		}, /* 216 drawing symbol (unicode) */
	{ 9496	,NULL		}, /* 217 drawing symbol (unicode) */
	{ 9484	,NULL		}, /* 218 drawing symbol (unicode) */
	{ 9608	,NULL		}, /* 219 drawing symbol (unicode) */
	{ 9604	,NULL		}, /* 220 drawing symbol (unicode) */
	{ 9612	,NULL		}, /* 221 drawing symbol (unicode) */
	{ 9616	,NULL		}, /* 222 drawing symbol (unicode) */
	{ 9600	,NULL		}, /* 223 drawing symbol (unicode) */
	{ 945	,NULL		}, /* 224 alpha symbol */
	{ 223	,"szlig"	}, /* 225 sz ligature (beta symbol) */
	{ 915	,NULL		}, /* 226 omega symbol */
	{ 960	,NULL		}, /* 227 pi symbol*/
	{ 931	,NULL		}, /* 228 epsilon symbol */
	{ 963	,NULL		}, /* 229 o with stick */
	{ 181	,"micro"	}, /* 230 Micro sign (Greek mu) */
	{ 964	,NULL		}, /* 231 greek char? */
	{ 934	,NULL		}, /* 232 greek char? */
	{ 920	,NULL		}, /* 233 greek char? */
	{ 937	,NULL		}, /* 234 greek char? */
	{ 948	,NULL		}, /* 235 greek char? */
	{ 8734	,NULL		}, /* 236 infinity symbol (unicode) */
	{ 966	,"oslash"	}, /* 237 Greek Phi */
	{ 949	,NULL		}, /* 238 rounded E */
	{ 8745	,NULL		}, /* 239 unside down U (unicode) */
	{ 8801	,NULL		}, /* 240 drawing symbol (unicode) */
	{ 177	,"plusmn"	}, /* 241 Plus or minus */
	{ 8805	,NULL		}, /* 242 drawing symbol (unicode) */
	{ 8804	,NULL		}, /* 243 drawing symbol (unicode) */
	{ 8992	,NULL		}, /* 244 drawing symbol (unicode) */
	{ 8993	,NULL		}, /* 245 drawing symbol (unicode) */
	{ 247	,"divide"	}, /* 246 Division sign */
	{ 8776	,NULL		}, /* 247 two squiggles (unicode) */
	{ 176	,"deg"		}, /* 248 Degree sign */
	{ 8729	,NULL		}, /* 249 drawing symbol (unicode) */
	{ 183	,"middot"	}, /* 250 Middle dot */
	{ 8730	,NULL		}, /* 251 check mark (unicode) */
	{ 8319	,NULL		}, /* 252 superscript n (unicode) */
	{ 178	,"sup2"		}, /* 253 superscript 2 */
	{ 9632	,NULL		}, /* 254 drawing symbol (unicode) */
	{ 160	,"nbsp"		}  /* 255 non-breaking space */
};

static struct { 
	int		value;
	char*	name;
} lowasctbl[32] = {
	{ 160	,"nbsp"		}, /* NULL non-breaking space */
	{ 9786	,NULL		}, /* white smiling face */
	{ 9787	,NULL		}, /* black smiling face */
	{ 9829	,"hearts"	}, /* black heart suit */
	{ 9830	,"diams"	}, /* black diamond suit */
	{ 9827	,"clubs"	}, /* black club suit */
	{ 9824	,"spades"	}, /* black spade suit */
	{ 8226	,"bull"		}, /* bullet */
	{ 9688	,NULL		}, /* inverse bullet */
	{ 9702	,NULL		}, /* white bullet */
	{ 9689	,NULL		}, /* inverse white circle */
	{ 9794	,NULL		}, /* male sign */
	{ 9792	,NULL		}, /* female sign */
	{ 9834	,NULL		}, /* eighth note */
	{ 9835	,NULL		}, /* beamed eighth notes */
	{ 9788	,NULL		}, /* white sun with rays */
	{ 9654	,NULL		}, /* black right-pointing triangle */
	{ 9664	,NULL		}, /* black left-pointing triangle */
	{ 8597	,NULL		}, /* up down arrow */
	{ 8252	,NULL		}, /* double exclamation mark */
	{ 182	,"para"		}, /* pilcrow sign */
	{ 167	,"sect"		}, /* section sign */
	{ 9644	,NULL		}, /* black rectangle */
	{ 8616	,NULL		}, /* up down arrow with base */
	{ 8593	,"uarr"		}, /* upwards arrow */
	{ 8595	,"darr"		}, /* downwards arrow */
	{ 8594	,"rarr"		}, /* rightwards arrow */
	{ 8592	,"larr"		}, /* leftwards arrow */
	{ 8985	,NULL		}, /* turned not sign */
	{ 8596	,"harr"		}, /* left right arrow */
	{ 9650	,NULL		}, /* black up-pointing triangle */
	{ 9660	,NULL		}  /* black down-pointing triangle */
};

static JSBool
js_html_encode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int			ch;
	ulong		i,j;
	uchar*		inbuf;
	uchar*		tmpbuf;
	uchar*		outbuf;
	uchar*		param;
	char*		lastparam;
	JSBool		exascii=JS_TRUE;
	JSBool		wsp=JS_TRUE;
	JSBool		ansi=JS_TRUE;
	JSBool		ctrl_a=JS_TRUE;
	JSString*	js_str;
	int32		fg=7;
	int32		bg=0;
	JSBool		blink=FALSE;
	JSBool		bold=FALSE;
	int			esccount=0;
	char		ansi_seq[MAX_ANSI_SEQ+1];
	int			ansi_param[MAX_ANSI_PARAMS];
	int			k,l;
	ulong		savepos=0;
	int32		hpos=0;
	int32		currrow=0;
	int			savehpos=0;
	int			savevpos=0;
	int32		wraphpos=-2;
	int32		wrapvpos=-2;
	int32		wrappos=0;
	BOOL		extchar=FALSE;
	ulong		obsize;
	int32		lastcolor=7;
	char		tmp1[128];
	struct		tm tm;
	time_t		now;
	BOOL		nodisplay=FALSE;
	private_t*	p;
	uchar   	attr_stack[64]; /* Saved attributes (stack) */
	int     	attr_sp=0;                /* Attribute stack pointer */
	ulong		clear_screen=0;
	JSObject*	stateobj=NULL;
	jsval		val;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)		/* Will this work?  Ask DM */
		return(JS_FALSE);

	if((inbuf=js_ValueToStringBytes(cx, argv[0], NULL))==NULL)
		return(JS_FALSE);

	if(argc>1 && JSVAL_IS_BOOLEAN(argv[1]))
		exascii=JSVAL_TO_BOOLEAN(argv[1]);

	if(argc>2 && JSVAL_IS_BOOLEAN(argv[2]))
		wsp=JSVAL_TO_BOOLEAN(argv[2]);

	if(argc>3 && JSVAL_IS_BOOLEAN(argv[3]))
		ansi=JSVAL_TO_BOOLEAN(argv[3]);

	if(argc>4 && JSVAL_IS_BOOLEAN(argv[4]))
	{
		ctrl_a=JSVAL_TO_BOOLEAN(argv[4]);
		if(ctrl_a)
			ansi=ctrl_a;
	}

	if(argc>5 && JSVAL_IS_OBJECT(argv[5])) {
		stateobj=JSVAL_TO_OBJECT(argv[5]);
		JS_GetProperty(cx,stateobj,"fg",&val);
		if(JSVAL_IS_NUMBER(val))
			fg=JS_ValueToInt32(cx, val, &fg);
		JS_GetProperty(cx,stateobj,"bg",&val);
		if(JSVAL_IS_NUMBER(val))
			fg=JS_ValueToInt32(cx, val, &bg);
		JS_GetProperty(cx,stateobj,"lastcolor",&val);
		if(JSVAL_IS_NUMBER(val))
			fg=JS_ValueToInt32(cx, val, &lastcolor);
		JS_GetProperty(cx,stateobj,"blink",&val);
		if(JSVAL_IS_BOOLEAN(val))
			fg=JS_ValueToBoolean(cx, val, &blink);
		JS_GetProperty(cx,stateobj,"bold",&val);
		if(JSVAL_IS_BOOLEAN(val))
			fg=JS_ValueToBoolean(cx, val, &bold);
		JS_GetProperty(cx,stateobj,"hpos",&val);
		if(JSVAL_IS_NUMBER(val))
			fg=JS_ValueToInt32(cx, val, &hpos);
		JS_GetProperty(cx,stateobj,"currrow",&val);
		if(JSVAL_IS_NUMBER(val))
			fg=JS_ValueToInt32(cx, val, &currrow);
		JS_GetProperty(cx,stateobj,"wraphpos",&val);
		if(JSVAL_IS_NUMBER(val))
			fg=JS_ValueToInt32(cx, val, &wraphpos);
		JS_GetProperty(cx,stateobj,"wrapvpos",&val);
		if(JSVAL_IS_NUMBER(val))
			fg=JS_ValueToInt32(cx, val, &wrapvpos);
		JS_GetProperty(cx,stateobj,"wrappos",&val);
		if(JSVAL_IS_NUMBER(val))
			fg=JS_ValueToInt32(cx, val, &wrappos);
	}

	if((tmpbuf=(char*)malloc((strlen(inbuf)*10)+1))==NULL)
		return(JS_FALSE);

	for(i=j=0;inbuf[i];i++) {
		switch(inbuf[i]) {
			case TAB:
			case LF:
			case CR:
				if(wsp)
					j+=sprintf(tmpbuf+j,"&#%u;",inbuf[i]);
				else
					tmpbuf[j++]=inbuf[i];
				break;
			case '"':
				j+=sprintf(tmpbuf+j,"&quot;");
				break;
			case '&':
				j+=sprintf(tmpbuf+j,"&amp;");
				break;
			case '<':
				j+=sprintf(tmpbuf+j,"&lt;");
				break;
			case '>':
				j+=sprintf(tmpbuf+j,"&gt;");
				break;
			case '\b':
				j--;
				break;
			default:
				if(inbuf[i]&0x80) {
					if(exascii) {
						ch=inbuf[i]^0x80;
						if(exasctbl[ch].name!=NULL)
							j+=sprintf(tmpbuf+j,"&%s;",exasctbl[ch].name);
						else
							j+=sprintf(tmpbuf+j,"&#%u;",exasctbl[ch].value);
					} else
						tmpbuf[j++]=inbuf[i];
				}
				else if(inbuf[i]>=' ' && inbuf[i]<DEL)
					tmpbuf[j++]=inbuf[i];
#if 0		/* ASCII 127 - Not displayed? */
				else if(inbuf[i]==DEL && exascii)
					j+=sprintf(tmpbuf+j,"&#8962;",exasctbl[ch].value);
#endif
				else if(inbuf[i]<' ') /* unknown control chars */
				{
					if(ansi && inbuf[i]==ESC)
					{
						esccount++;
						tmpbuf[j++]=inbuf[i];
					}
					else if(ctrl_a && inbuf[i]==1)
					{
						esccount++;
						tmpbuf[j++]=inbuf[i];
						tmpbuf[j++]=inbuf[++i];
					}
					else if(exascii) {
						ch=inbuf[i];
						if(lowasctbl[ch].name!=NULL)
							j+=sprintf(tmpbuf+j,"&%s;",lowasctbl[ch].name);
						else
							j+=sprintf(tmpbuf+j,"&#%u;",lowasctbl[ch].value);
					} else
						j+=sprintf(tmpbuf+j,"&#%u;",inbuf[i]);
				}
				break;
		}
	}
	tmpbuf[j]=0;

	if(ansi)
	{
		obsize=(strlen(tmpbuf)+(esccount+1)*MAX_COLOR_STRING)+1;
		if(obsize<2048)
			obsize=2048;
		if((outbuf=(uchar*)malloc(obsize))==NULL)
		{
			free(tmpbuf);
			return(JS_FALSE);
		}
		j=sprintf(outbuf,"<span style=\"%s\">",htmlansi[7]);
		clear_screen=j;
		for(i=0;tmpbuf[i];i++) {
			if(j>(obsize/2))		/* Completely arbitrary here... must be carefull with this eventually ToDo */
			{
				obsize+=(obsize/2);
				if((param=realloc(outbuf,obsize))==NULL)
				{
					free(tmpbuf);
					free(outbuf);
					return(JS_FALSE);
				}
				outbuf=param;
			}
			if(tmpbuf[i]==ESC && tmpbuf[i+1]=='[')
			{
				if(nodisplay)
					continue;
				k=0;
				memset(&ansi_param,0xff,sizeof(int)*MAX_ANSI_PARAMS);
				strncpy(ansi_seq, tmpbuf+i+2, MAX_ANSI_SEQ);
				ansi_seq[MAX_ANSI_SEQ]=0;
				for(lastparam=ansi_seq;*lastparam;lastparam++)
				{
					if(isalpha(*lastparam))
					{
						*(++lastparam)=0;
						break;
					}
				}
				k=0;
				param=ansi_seq;
				if(*param=='?')		/* This is to fix ESC[?7 whatever that is */
					param++;
				if(isdigit(*param))
					ansi_param[k++]=atoi(ansi_seq);
				while(isspace(*param) || isdigit(*param))
					param++;
				lastparam=param;
				while((param=strchr(param,';'))!=NULL)
				{
					param++;
					ansi_param[k++]=atoi(param);
					while(isspace(*param) || isdigit(*param))
						param++;
					lastparam=param;
				}
				switch(*lastparam)
				{
					case 'm':	/* Colour */
						for(k=0;ansi_param[k]>=0;k++)
						{
							switch(ansi_param[k])
							{
								case 0:
									fg=7;
									bg=0;
									blink=FALSE;
									bold=FALSE;
									break;
								case 1:
									bold=TRUE;
									break;
								case 2:
									bold=FALSE;
									break;
								case 5:
									blink=TRUE;
									break;
								case 6:
									blink=TRUE;
									break;
								case 7:
									l=fg;
									fg=bg;
									bg=l;
									break;
								case 8:
									fg=bg;
									blink=FALSE;
									bold=FALSE;
									break;
								case 30:
								case 31:
								case 32:
								case 33:
								case 34:
								case 35:
								case 36:
								case 37:
									fg=ansi_param[k]-30;
									break;
								case 40:
								case 41:
								case 42:
								case 43:
								case 44:
								case 45:
								case 46:
								case 47:
									bg=ansi_param[k]-40;
									break;
							}
						}
						break;
					case 'C': /* Move right */
						j+=sprintf(outbuf+j,"%s%s%s",HTML_COLOR_PREFIX,htmlansi[0],HTML_COLOR_SUFFIX);
						lastcolor=0;
						l=ansi_param[0]>0?ansi_param[0]:1;
						if(wrappos==0 && wrapvpos==currrow)  {
							/* j+=sprintf(outbuf+j,"<!-- \r\nC after A l=%d hpos=%d -->",l,hpos); */
							l=l-hpos;
							wrapvpos=-2;	/* Prevent additional move right */
						}
						if(l>81-hpos)
							l=81-hpos;
						for(k=0; k<l; k++)
						{
							j+=sprintf(outbuf+j,"%s","&nbsp;");
							hpos++;
						}
						break;
					case 's': /* Save position */
						savepos=j;
						savehpos=hpos;
						savevpos=currrow;
						break;
					case 'u': /* Restore saved position */
						j=savepos;
						hpos=savehpos;
						currrow=savevpos;
						break;
					case 'H': /* Move */
						k=ansi_param[0];
						if(k<=0)
							k=1;
						k--;
						l=ansi_param[1];
						if(l<=0)
							l=1;
						l--;
						while(k>currrow)
						{
							hpos=0;
							currrow++;
							outbuf[j++]='\r';
							outbuf[j++]='\n';
						}
						if(l>hpos)
						{
							j+=sprintf(outbuf+j,"%s%s%s",HTML_COLOR_PREFIX,htmlansi[0],HTML_COLOR_SUFFIX);
							lastcolor=0;
							while(l>hpos)
							{
								j+=sprintf(outbuf+j,"%s","&nbsp;");
								hpos++;
							}
						}
						break;
					case 'B': /* Move down */
						l=ansi_param[0];
						if(l<=0)
							l=1;
						for(k=0; k < l; k++)
						{
							currrow++;
							outbuf[j++]='\r';
							outbuf[j++]='\n';
						}
						if(hpos!=0 && tmpbuf[i+1]!=CR)
						{
							j+=sprintf(outbuf+j,"%s%s%s",HTML_COLOR_PREFIX,htmlansi[0],HTML_COLOR_SUFFIX);
							lastcolor=0;
							for(k=0; k<hpos ; k++)
							{
								j+=sprintf(outbuf+j,"%s","&nbsp;");
							}
							break;
						}
						break;
					case 'A': /* Move up */
						l=wrappos;
						if(j > (ulong)wrappos && hpos==0 && currrow==wrapvpos+1 && ansi_param[0]<=1)  {
							hpos=wraphpos;
							currrow=wrapvpos;
							j=wrappos;
							wrappos=0; /* Prevent additional move up */
						}
						break;
					case 'J': /* Clear */
						if(ansi_param[0]==2)  {
							j=clear_screen;
							hpos=0;
							currrow=0;
							wraphpos=-2;
							wrapvpos=-2;
							wrappos=0;
						}
						break;
				}
				i+=(int)(lastparam-ansi_seq)+2;
			}
			else if(ctrl_a && tmpbuf[i]==1)		/* CTRL-A codes */
			{
/*				j+=sprintf(outbuf+j,"<!-- CTRL-A-%c (%u) -->",tmpbuf[i+1],tmpbuf[i+1]); */
				if(nodisplay && tmpbuf[i+1] != ')')
					continue;
				if(tmpbuf[i+1]>0x7f)
				{
					j+=sprintf(outbuf+j,"%s%s%s",HTML_COLOR_PREFIX,htmlansi[0],HTML_COLOR_SUFFIX);
					lastcolor=0;
					l=tmpbuf[i+1]-0x7f;
					if(l>81-hpos)
						l=81-hpos;
					for(k=0; k<l; k++)
					{
						j+=sprintf(outbuf+j,"%s","&nbsp;");
						hpos++;
					}
				}
				else switch(toupper(tmpbuf[i+1]))
				{
					case 'K':
						fg=0;
						break;
					case 'R':
						fg=1;
						break;
					case 'G':
						fg=2;
						break;
					case 'Y':
						fg=3;
						break;
					case 'B':
						fg=4;
						break;
					case 'M':
						fg=5;
						break;
					case 'C':
						fg=6;
						break;
					case 'W':
						fg=7;
						break;
					case '0':
						bg=0;
						break;
					case '1':
						bg=1;
						break;
					case '2':
						bg=2;
						break;
					case '3':
						bg=3;
						break;
					case '4':
						bg=4;
						break;
					case '5':
						bg=5;
						break;
					case '6':
						bg=6;
						break;
					case '7':
						bg=7;
						break;
					case 'H':
						bold=TRUE;
						break;
					case 'I':
						blink=TRUE;
						break;
					case '+':
						if(attr_sp<(int)sizeof(attr_stack))
							attr_stack[attr_sp++]=(blink?(1<<7):0) | (bg << 4) | (bold?(1<<3):0) | (int)fg;
						break;
					case '-':
						if(attr_sp>0)
						{
							blink=(attr_stack[--attr_sp]&(1<<7))?TRUE:FALSE;
							bg=(attr_stack[attr_sp] >> 4) & 7;
							blink=(attr_stack[attr_sp]&(1<<3))?TRUE:FALSE;
							fg=attr_stack[attr_sp] & 7;
						}
						else if(bold || blink || bg)
						{
							bold=FALSE;
							blink=FALSE;
							fg=7;
							bg=0;
						}
						break;
					case '_':
						if(blink || bg)
						{
							bold=FALSE;
							blink=FALSE;
							fg=7;
							bg=0;
						}
						break;
					case 'N':
						bold=FALSE;
						blink=FALSE;
						fg=7;
						bg=0;
						break;
					case 'P':
					case 'Q':
					case ',':
					case ';':
					case '.':
					case 'S':
					case '>':
					case '<':
						break;

					case '!':		/* This needs to be fixed! (Somehow) */
					case '@':
					case '#':
					case '$':
					case '%':
					case '^':
					case '&':
					case '*':
					case '(':
						nodisplay=TRUE;
						break;
					case ')':
						nodisplay=FALSE;
						break;

					case 'D':
						now=time(NULL);
						j+=sprintf(outbuf+j,"%s",unixtodstr(p->cfg,now,tmp1));
						break;
					case 'T':
						now=time(NULL);
						localtime_r(&now,&tm);
						j+=sprintf(outbuf+j,"%02d:%02d %s"
							,tm.tm_hour==0 ? 12
							: tm.tm_hour>12 ? tm.tm_hour-12
							: tm.tm_hour, tm.tm_min, tm.tm_hour>11 ? "pm":"am");
						break;
						
					case 'L':
						currrow=0;
						hpos=0;
						outbuf[j++]='\r';
						outbuf[j++]='\n';
						break;
					case ']':
						currrow++;
						if(hpos!=0 && tmpbuf[i+2]!=CR && !(tmpbuf[i+2]==1 && tmpbuf[i+3]=='['))
						{
							outbuf[j++]='\r';
							outbuf[j++]='\n';
							j+=sprintf(outbuf+j,"%s%s%s",HTML_COLOR_PREFIX,htmlansi[0],HTML_COLOR_SUFFIX);
							lastcolor=0;
							for(k=0; k<hpos ; k++)
							{
								j+=sprintf(outbuf+j,"%s","&nbsp;");
							}
							break;
						}
						outbuf[j++]='\n';
						break;
					case '[':
						outbuf[j++]='\r';
						hpos=0;
						break;
					case 'Z':
						outbuf[j++]=0;
						break;
					case 'A':
					default:
						if(exascii) {
							ch=tmpbuf[i];
							if(lowasctbl[ch].name!=NULL)
								j+=sprintf(outbuf+j,"&%s;",lowasctbl[ch].name);
							else
								j+=sprintf(outbuf+j,"&#%u;",lowasctbl[ch].value);
						} else
							j+=sprintf(outbuf+j,"&#%u;",inbuf[i]);
						i--;
				}
				i++;
			}
			else
			{
				if(nodisplay)
					continue;
				switch(tmpbuf[i])
				{
					case TAB:			/* This assumes that tabs do NOT use the current background. */
						l=hpos%8;
						if(l==0)
							l=8;
						j+=sprintf(outbuf+j,"%s%s%s",HTML_COLOR_PREFIX,htmlansi[0],HTML_COLOR_SUFFIX);
						lastcolor=0;
						for(k=0; k<l ; k++)
						{
							j+=sprintf(outbuf+j,"%s","&nbsp;");
							hpos++;
						}
						break;
					case LF:
						wrapvpos=currrow;
						if((ulong)wrappos<j-3)
							wrappos=j;
						currrow++;
						if(hpos!=0 && tmpbuf[i+1]!=CR)
						{
							outbuf[j++]='\r';
							outbuf[j++]='\n';
							j+=sprintf(outbuf+j,"%s%s%s",HTML_COLOR_PREFIX,htmlansi[0],HTML_COLOR_SUFFIX);
							lastcolor=0;
							for(k=0; k<hpos ; k++)
							{
								j+=sprintf(outbuf+j,"%s","&nbsp;");
							}
							break;
						}
					case CR:
						if(wraphpos==-2 || hpos!=0)
							wraphpos=hpos;
						if((ulong)wrappos<j-3)
							wrappos=j;
						outbuf[j++]=tmpbuf[i];
						hpos=0;
						break;
					default:
						if(lastcolor != ((blink?(1<<7):0) | (bg << 4) | (bold?(1<<3):0) | fg))
						{
							lastcolor=(blink?(1<<7):0) | (bg << 4) | (bold?(1<<3):0) | fg;
							j+=sprintf(outbuf+j,"%s%s%s",HTML_COLOR_PREFIX,htmlansi[lastcolor],HTML_COLOR_SUFFIX);
						}
						outbuf[j++]=tmpbuf[i];
						if(tmpbuf[i]=='&')
							extchar=TRUE;
						if(tmpbuf[i]==';')
							extchar=FALSE;
						if(!extchar)
							hpos++;
						/* ToDo: Fix hard-coded terminal window width (80) */
						if(hpos>=80 && tmpbuf[i+1] != '\r' && tmpbuf[i+1] != '\n' && tmpbuf[i+1] != ESC)
						{
							wrapvpos=-2;
							wraphpos=-2;
							wrappos=0;
							hpos=0;
							currrow++;
							outbuf[j++]='\r';
							outbuf[j++]='\n';
						}
				}
			}
		}
		strcpy(outbuf+j,"</span>");

		js_str = JS_NewStringCopyZ(cx, outbuf);
		free(outbuf);
	}
	else
		js_str = JS_NewStringCopyZ(cx, tmpbuf);

	free(tmpbuf);	/* assertion here, Feb-20-2006 */
	if(js_str==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);

	if(stateobj!=NULL) {
		JS_DefineProperty(cx, stateobj, "fg", INT_TO_JSVAL(fg)
			,NULL,NULL,JSPROP_ENUMERATE);
		JS_DefineProperty(cx, stateobj, "bg", INT_TO_JSVAL(bg)
			,NULL,NULL,JSPROP_ENUMERATE);
		JS_DefineProperty(cx, stateobj, "lastcolor", INT_TO_JSVAL(lastcolor)
			,NULL,NULL,JSPROP_ENUMERATE);
		JS_DefineProperty(cx, stateobj, "blink", BOOLEAN_TO_JSVAL(blink)
			,NULL,NULL,JSPROP_ENUMERATE);
		JS_DefineProperty(cx, stateobj, "bold", BOOLEAN_TO_JSVAL(bold)
			,NULL,NULL,JSPROP_ENUMERATE);
		JS_DefineProperty(cx, stateobj, "hpos", INT_TO_JSVAL(hpos)
			,NULL,NULL,JSPROP_ENUMERATE);
		JS_DefineProperty(cx, stateobj, "currrow", INT_TO_JSVAL(currrow)
			,NULL,NULL,JSPROP_ENUMERATE);
		JS_DefineProperty(cx, stateobj, "wraphpos", INT_TO_JSVAL(wraphpos)
			,NULL,NULL,JSPROP_ENUMERATE);
		JS_DefineProperty(cx, stateobj, "wrapvpos", INT_TO_JSVAL(wrapvpos)
			,NULL,NULL,JSPROP_ENUMERATE);
		JS_DefineProperty(cx, stateobj, "wrappos", INT_TO_JSVAL(wrappos)
			,NULL,NULL,JSPROP_ENUMERATE);
	}

	return(JS_TRUE);
}

static JSBool
js_html_decode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int			ch;
	int			val;
	ulong		i,j;
	uchar*		inbuf;
	uchar*		outbuf;
	char		token[16];
	size_t		t;
	JSString*	js_str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((inbuf=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	if((outbuf=(char*)malloc(strlen(inbuf)+1))==NULL)
		return(JS_FALSE);

	for(i=j=0;inbuf[i];i++) {
		if(inbuf[i]!='&') {
			outbuf[j++]=inbuf[i];
			continue;
		}
		for(i++,t=0; inbuf[i]!=0 && inbuf[i]!=';' && t<sizeof(token)-1; i++, t++)
			token[t]=inbuf[i];
		if(inbuf[i]==0)
			break;
		token[t]=0;

		/* First search the ex-ascii table for a name match */
		for(ch=0;ch<128;ch++)
			if(exasctbl[ch].name!=NULL && strcmp(token,exasctbl[ch].name)==0)
				break;
		if(ch<128) {
			outbuf[j++]=ch|0x80;
			continue;
		}
		if(token[0]=='#') {		/* numeric constant */
			val=atoi(token+1);

			/* search ex-ascii table for a value match */
			for(ch=0;ch<128;ch++)
				if(exasctbl[ch].value==val)
					break;
			if(ch<128) {
				outbuf[j++]=ch|0x80;
				continue;
			}

			if((val>=' ' && val<=0xff) || val=='\r' || val=='\n' || val=='\t') {
				outbuf[j++]=val;
				continue;
			}
		}
		if(strcmp(token,"quot")==0) {
			outbuf[j++]='"';
			continue;
		}
		if(strcmp(token,"amp")==0) {
			outbuf[j++]='&';
			continue;
		}
		if(strcmp(token,"lt")==0) {
			outbuf[j++]='<';
			continue;
		}
		if(strcmp(token,"gt")==0) {
			outbuf[j++]='>';
			continue;
		}
		if(strcmp(token,"curren")==0) {
			outbuf[j++]=CTRL_O;
			continue;
		}
		if(strcmp(token,"para")==0) {
			outbuf[j++]=CTRL_T;
			continue;
		}
		if(strcmp(token,"sect")==0) {
			outbuf[j++]=CTRL_U;
			continue;
		}
		/* Unknown character entity, leave intact */
		j+=sprintf(outbuf+j,"&%s;",token);
		
	}
	outbuf[j]=0;

	js_str = JS_NewStringCopyZ(cx, outbuf);
	free(outbuf);
	if(js_str==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_b64_encode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int			res;
	size_t		len;
	size_t		inbuf_len;
	uchar*		inbuf;
	uchar*		outbuf;
	JSString*	js_str;

	*rval = JSVAL_NULL;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((inbuf=js_ValueToStringBytes(cx, argv[0], &inbuf_len))==NULL)
		return(JS_FALSE);

	len=(inbuf_len*10)+1;

	if((outbuf=(char*)malloc(len))==NULL)
		return(JS_FALSE);

	res=b64_encode(outbuf,len,inbuf,inbuf_len);

	if(res<1) {
		free(outbuf);
		return(JS_TRUE);
	}

	js_str = JS_NewStringCopyZ(cx, outbuf);
	free(outbuf);
	if(js_str==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_b64_decode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int			res;
	size_t		len;
	uchar*		inbuf;
	uchar*		outbuf;
	JSString*	js_str;

	*rval = JSVAL_NULL;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((inbuf=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	len=strlen(inbuf)+1;

	if((outbuf=(char*)malloc(len))==NULL)
		return(JS_FALSE);

	res=b64_decode(outbuf,len,inbuf,strlen(inbuf));

	if(res<1) {
		free(outbuf);
		return(JS_TRUE);
	}

	js_str = JS_NewStringCopyN(cx, outbuf, res);
	free(outbuf);
	if(js_str==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_md5_calc(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	BYTE		digest[MD5_DIGEST_SIZE];
	JSBool		hex=JS_FALSE;
	size_t		inbuf_len;
	char*		inbuf;
	char		outbuf[64];
	JSString*	js_str;

	*rval = JSVAL_NULL;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((inbuf=js_ValueToStringBytes(cx, argv[0], &inbuf_len))==NULL)
		return(JS_FALSE);

	if(argc>1 && JSVAL_IS_BOOLEAN(argv[1]))
		hex=JSVAL_TO_BOOLEAN(argv[1]);

	MD5_calc(digest,inbuf,inbuf_len);

	if(hex)
		MD5_hex(outbuf,digest);
	else
		b64_encode(outbuf,sizeof(outbuf),digest,sizeof(digest));

	js_str = JS_NewStringCopyZ(cx, outbuf);
	if(js_str==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_skipsp(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		str;
	JSString*	js_str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((str=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	js_str = JS_NewStringCopyZ(cx, skipsp(str));
	if(js_str==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_truncsp(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char*		str;
	JSString*	js_str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((str=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	if((p=strdup(str))==NULL)
		return(JS_FALSE);

	truncsp(p);

	js_str = JS_NewStringCopyZ(cx, p);
	free(p);
	if(js_str==NULL)
		return(JS_FALSE);

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

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((str=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	if((set=js_ValueToStringBytes(cx, argv[1], NULL))==NULL) 
		return(JS_FALSE);

	if((p=strdup(str))==NULL)
		return(JS_FALSE);

	truncstr(p,set);

	js_str = JS_NewStringCopyZ(cx, p);
	free(p);
	if(js_str==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_backslash(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		path[MAX_PATH+1];
	char*		str;
	JSString*	js_str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((str=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);
	
	SAFECOPY(path,str);
	backslash(path);

	if((js_str = JS_NewStringCopyZ(cx, path))==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_fullpath(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		path[MAX_PATH+1];
	char*		str;
	JSString*	js_str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((str=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	SAFECOPY(path,str);
	_fullpath(path, str, sizeof(path));

	if((js_str = JS_NewStringCopyZ(cx, path))==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}


static JSBool
js_getfname(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		str;
	JSString*	js_str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((str=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	js_str = JS_NewStringCopyZ(cx, getfname(str));
	if(js_str==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_getfext(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		str;
	char*		p;
	JSString*	js_str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((str=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	if((p=getfext(str))==NULL)
		return(JS_TRUE);

	js_str = JS_NewStringCopyZ(cx, p);
	if(js_str==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_getfcase(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		str;
	char		path[MAX_PATH+1];
	JSString*	js_str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((str=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	SAFECOPY(path,str);
	if(fexistcase(path)) {
		js_str = JS_NewStringCopyZ(cx, path);
		if(js_str==NULL)
			return(JS_FALSE);

		*rval = STRING_TO_JSVAL(js_str);
	}
	return(JS_TRUE);
}

static JSBool
js_dosfname(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		str;
	char		path[MAX_PATH+1];
	JSString*	js_str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

#if defined(_WIN32)

	if((str=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	if(GetShortPathName(str,path,sizeof(path))) {
		js_str = JS_NewStringCopyZ(cx, path);
		if(js_str==NULL)
			return(JS_FALSE);

		*rval = STRING_TO_JSVAL(js_str);
	}

#else	/* No non-Windows equivalent */

	*rval = argv[0];

#endif

	return(JS_TRUE);
}

static JSBool
js_cfgfname(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		path;
	char*		fname;
	char		result[MAX_PATH+1];

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((path=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	if((fname=js_ValueToStringBytes(cx, argv[1], NULL))==NULL) 
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,iniFileName(result,sizeof(result),path,fname)));

	return(JS_TRUE);
}

static JSBool
js_fexist(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(fexist(p));
	return(JS_TRUE);
}

static JSBool
js_removecase(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(removecase(p)==0);
	return(JS_TRUE);
}

static JSBool
js_remove(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(remove(p)==0);
	return(JS_TRUE);
}

static JSBool
js_rename(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		oldname;
	char*		newname;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	if((oldname=js_ValueToStringBytes(cx, argv[0], NULL))==NULL)
		return(JS_TRUE);
	if((newname=js_ValueToStringBytes(cx, argv[1], NULL))==NULL)
		return(JS_TRUE);

	*rval = BOOLEAN_TO_JSVAL(rename(oldname,newname)==0);
	return(JS_TRUE);
}

static JSBool
js_fcopy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		src;
	char*		dest;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	if((src=js_ValueToStringBytes(cx, argv[0], NULL))==NULL)
		return(JS_TRUE);
	if((dest=js_ValueToStringBytes(cx, argv[1], NULL))==NULL)
		return(JS_TRUE);

	*rval = BOOLEAN_TO_JSVAL(fcopy(src,dest));
	return(JS_TRUE);
}

static JSBool
js_fcompare(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		fn1;
	char*		fn2;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	if((fn1=js_ValueToStringBytes(cx, argv[0], NULL))==NULL)
		return(JS_TRUE);
	if((fn2=js_ValueToStringBytes(cx, argv[1], NULL))==NULL)
		return(JS_TRUE);

	*rval = BOOLEAN_TO_JSVAL(fcompare(fn1,fn2));
	return(JS_TRUE);
}

static JSBool
js_backup(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		fname;
	int32		level=5;
	BOOL		ren=FALSE;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	if((fname=js_ValueToStringBytes(cx, argv[0], NULL))==NULL)
		return(JS_TRUE);

	if(argc>1)
		JS_ValueToInt32(cx,argv[1],&level);
	if(argc>2)
		JS_ValueToBoolean(cx,argv[2],&ren);

	*rval = BOOLEAN_TO_JSVAL(backup(fname,level,ren));
	return(JS_TRUE);
}

static JSBool
js_isdir(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(isdir(p));
	return(JS_TRUE);
}

static JSBool
js_fattr(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	JS_NewNumberValue(cx,getfattr(p),rval);
	return(JS_TRUE);
}

static JSBool
js_fdate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	JS_NewNumberValue(cx,fdate(p),rval);
	return(JS_TRUE);
}

static JSBool
js_utime(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*			fname;
	int32			actime;
	int32			modtime;
	struct utimbuf	ut;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	*rval = JSVAL_FALSE;

	if((fname=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	/* use current time as default */
	ut.actime = ut.modtime = time(NULL);

	if(argc>1) {
		actime=modtime=ut.actime;
		JS_ValueToInt32(cx,argv[1],&actime);
		JS_ValueToInt32(cx,argv[2],&modtime);
		ut.actime=actime;
		ut.modtime=modtime;
	}

	*rval = BOOLEAN_TO_JSVAL(utime(fname,&ut)==0);

	return(JS_TRUE);
}


static JSBool
js_flength(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	JS_NewNumberValue(cx,flength(p),rval);
	return(JS_TRUE);
}


static JSBool
js_ftouch(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		fname;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((fname=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(ftouch(fname));
	return(JS_TRUE);
}

static JSBool
js_fmutex(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		fname;
	char*		text=NULL;
	int32		max_age=0;
	uintN		argn=0;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((fname=js_ValueToStringBytes(cx, argv[argn++], NULL))==NULL) 
		return(JS_FALSE);
	if(argc > argn && JSVAL_IS_STRING(argv[argn]))
		text=js_ValueToStringBytes(cx, argv[argn++], NULL);
	if(argc > argn && JSVAL_IS_NUMBER(argv[argn]))
		JS_ValueToInt32(cx, argv[argn++], &max_age);

	*rval = BOOLEAN_TO_JSVAL(fmutex(fname,text,max_age));
	return(JS_TRUE);
}
		
static JSBool
js_sound(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;

	if(!argc) {	/* Stop playing sound */
#ifdef _WIN32
		PlaySound(NULL,NULL,0);
#endif
		*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
		return(JS_TRUE);
	}

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

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

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	*rval = JSVAL_NULL;

	if((p=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	if(argc>1)
		JS_ValueToInt32(cx,argv[1],&flags);

    if((array = JS_NewArrayObject(cx, 0, NULL))==NULL)
		return(JS_FALSE);

	glob(p,flags,NULL,&g);
	for(i=0;i<(int)g.gl_pathc;i++) {
		if((js_str=JS_NewStringCopyZ(cx,g.gl_pathv[i]))==NULL)
			break;
		val=STRING_TO_JSVAL(js_str);
        if(!JS_SetElement(cx, array, len++, &val))
			break;
	}
	globfree(&g);

    *rval = OBJECT_TO_JSVAL(array);

    return(JS_TRUE);
}

static JSBool
js_wildmatch(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	BOOL		case_sensitive=FALSE;
	BOOL		path=FALSE;
	char*		fname;
	char*		spec="*";
	uintN		argn=0;

	if(JSVAL_IS_BOOLEAN(argv[argn]))
		JS_ValueToBoolean(cx, argv[argn++], &case_sensitive);

	if((fname=js_ValueToStringBytes(cx, argv[argn++], NULL))==NULL) 
		return(JS_FALSE);

	if(argn<argc && argv[argn]!=JSVAL_VOID)
		if((spec=js_ValueToStringBytes(cx, argv[argn++], NULL))==NULL) 
			return(JS_FALSE);

	if(argn<argc && argv[argn]!=JSVAL_VOID)
		JS_ValueToBoolean(cx, argv[argn++], &path);
	
	if(case_sensitive)
		*rval = BOOLEAN_TO_JSVAL(wildmatch(fname, spec, path));
	else
		*rval = BOOLEAN_TO_JSVAL(wildmatchi(fname, spec, path));

	return(JS_TRUE);
}


static JSBool
js_freediskspace(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		unit=0;
	char*		p;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	if(argc>1)
		JS_ValueToInt32(cx,argv[1],&unit);

	JS_NewNumberValue(cx,getfreediskspace(p,unit),rval);

    return(JS_TRUE);
}

static JSBool
js_disksize(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		unit=0;
	char*		p;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	if(argc>1)
		JS_ValueToInt32(cx,argv[1],&unit);

	JS_NewNumberValue(cx,getdisksize(p,unit),rval);

    return(JS_TRUE);
}


static JSBool
js_socket_select(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject*	inarray=NULL;
	JSObject*	rarray;
	BOOL		poll_for_write=FALSE;
	fd_set		socket_set;
	fd_set*		rd_set=NULL;
	fd_set*		wr_set=NULL;
	uintN		argn;
	SOCKET		sock;
	SOCKET		maxsock=0;
	struct		timeval tv = {0, 0};
	jsuint		i;
    jsuint      limit;
	SOCKET*		index;
	jsval		val;
	int			len=0;

	*rval = JSVAL_NULL;

	for(argn=0;argn<argc;argn++) {
		if(JSVAL_IS_BOOLEAN(argv[argn]))
			poll_for_write=JSVAL_TO_BOOLEAN(argv[argn]);
		else if(JSVAL_IS_OBJECT(argv[argn]))
			inarray = JSVAL_TO_OBJECT(argv[argn]);
		else if(JSVAL_IS_NUMBER(argv[argn]))
			js_timeval(cx,argv[argn],&tv);
	}

    if(inarray==NULL || !JS_IsArrayObject(cx, inarray))
		return(JS_TRUE);	/* This not a fatal error */

    if(!JS_GetArrayLength(cx, inarray, &limit))
		return(JS_TRUE);

	/* Return array */
    if((rarray = JS_NewArrayObject(cx, 0, NULL))==NULL)
		return(JS_FALSE);

	if((index=(SOCKET *)alloca(sizeof(SOCKET)*limit))==NULL)
		return(JS_FALSE);

	FD_ZERO(&socket_set);
	if(poll_for_write)
		wr_set=&socket_set;
	else
		rd_set=&socket_set;

    for(i=0;i<limit;i++) {
        if(!JS_GetElement(cx, inarray, i, &val))
			break;
		sock=js_socket(cx,val);
		index[i]=sock;
		if(sock!=INVALID_SOCKET) {
			FD_SET(sock,&socket_set);
			if(sock>maxsock)
				maxsock=sock;
		}
    }

	if(select(maxsock+1,rd_set,wr_set,NULL,&tv) >= 0) {

		for(i=0;i<limit;i++) {
			if(index[i]!=INVALID_SOCKET && FD_ISSET(index[i],&socket_set)) {
				val=INT_TO_JSVAL(i);
   				if(!JS_SetElement(cx, rarray, len++, &val))
					break;
			}
		}

		*rval = OBJECT_TO_JSVAL(rarray);
	}

    return(JS_TRUE);
}

static JSBool
js_mkdir(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(MKDIR(p)==0);
	return(JS_TRUE);
}

static JSBool
js_rmdir(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(rmdir(p)==0);
	return(JS_TRUE);
}


static JSBool
js_strftime(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		str[128];
	char*		fmt;
	int32		i=time(NULL);
	time_t		t;
	struct tm	tm;
	JSString*	js_str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((fmt=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	if(argc>1)
		JS_ValueToInt32(cx,argv[1],&i);

	strcpy(str,"-Invalid time-");
	t=i;
	if(localtime_r(&t,&tm)==NULL)
		memset(&tm,0,sizeof(tm));
	strftime(str,sizeof(str),fmt,&tm);

	if((js_str=JS_NewStringCopyZ(cx, str))==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_resolve_ip(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct in_addr addr;
	JSString*	str;
	char*		p;

	*rval = JSVAL_NULL;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	if((addr.s_addr=resolve_ip(p))==INADDR_NONE)
		return(JS_TRUE);
	
	if((str=JS_NewStringCopyZ(cx, inet_ntoa(addr)))==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(str);
	return(JS_TRUE);
}


static JSBool
js_resolve_host(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct in_addr addr;
	HOSTENT*	h;
	char*		p;

	*rval = JSVAL_NULL;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if((p=js_ValueToStringBytes(cx, argv[0], NULL))==NULL) 
		return(JS_FALSE);

	addr.s_addr=inet_addr(p);
	h=gethostbyaddr((char *)&addr,sizeof(addr),AF_INET);

	if(h!=NULL && h->h_name!=NULL)
		*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,h->h_name));

	return(JS_TRUE);

}

extern link_list_t named_queues;	/* js_queue.c */

static JSBool
js_list_named_queues(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject*	array;
    jsint       len=0;
	jsval		val;
	list_node_t* node;
	msg_queue_t* q;

    if((array = JS_NewArrayObject(cx, 0, NULL))==NULL)
		return(JS_FALSE);

	for(node=listFirstNode(&named_queues);node!=NULL;node=listNextNode(node)) {
		if((q=listNodeData(node))==NULL)
			continue;
		val=STRING_TO_JSVAL(JS_NewStringCopyZ(cx,q->name));
        if(!JS_SetElement(cx, array, len++, &val))
			break;
	}

    *rval = OBJECT_TO_JSVAL(array);

    return(JS_TRUE);
}

static JSBool
js_flags_str(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char		str[64];
	jsdouble	d;
	JSString*	js_str;

	if(JSVAL_IS_VOID(argv[0]))
		return(JS_TRUE);

	if(JSVAL_IS_STRING(argv[0])) {	/* string to long */

		if((p=JS_GetStringBytes(JSVAL_TO_STRING(argv[0])))==NULL) 
			return(JS_FALSE);

		JS_NewNumberValue(cx,aftol(p),rval);
		return(JS_TRUE);
	}

	/* number to string */
	JS_ValueToNumber(cx,argv[0],&d);

	if((js_str = JS_NewStringCopyZ(cx, ltoaf((long)d,str)))==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}
	
static jsSyncMethodSpec js_global_functions[] = {
	{"exit",			js_exit,			0,	JSTYPE_VOID,	"[exit_code]"
	,JSDOCSTR("stop script execution, "
		"optionally setting the global property <tt>exit_code</tt> to the specified numeric value")
	,311
	},		
	{"load",            js_load,            1,	JSTYPE_UNDEF
	,JSDOCSTR("[<i>bool</i> background or <i>object</i> scope,] <i>string</i> filename [,args]")
	,JSDOCSTR("load and execute a JavaScript module (<i>filename</i>), "
		"optionally specifying a target <i>scope</i> object (default: <i>this</i>) "
		"and a list of arguments to pass to the module (as <i>argv</i>). "
		"Returns the result (last executed statement) of the executed script "
		"or a newly created <i>Queue</i> object if <i>background</i> is <i>true</i>).<br><br>"
		"<b>Background</b> (added in v3.12):<br>"
		"If <i>background</i> is <i>true</i>, the loaded script runs in the background "
		"(in a child thread) but may communicate with the parent "
		"script/thread by reading from and/or writing to the <i>parent_queue</i> "
		"(an automatically created <i>Queue</i> object). " 
		"The result (last executed statement) of the executed script "
		"(or the optional <i>exit_code</i> passed to the <i>exit()/<i> function) "
		"will be automatically written to the <i>parent_queue</i> "
		"which may be read later by the parent script (using <i>load_result.read()</i>, for example).")
	,312
	},		
	{"sleep",			js_mswait,			0,	JSTYPE_ALIAS },
	{"mswait",			js_mswait,			0,	JSTYPE_NUMBER,	JSDOCSTR("[milliseconds=<tt>1</tt>]")
	,JSDOCSTR("millisecond wait/sleep routine (AKA sleep), returns number of elapsed clock ticks (in v3.13)")
	,313
	},
	{"yield",			js_yield,			0,	JSTYPE_VOID,	JSDOCSTR("[forced=<tt>true</tt>]")
	,JSDOCSTR("release current thread time-slice, "
		"a <i>forced</i> yield will yield to all other pending tasks (lowering CPU utilization), "
		"a non-<i>forced</i> yield will yield only to pending tasks of equal or higher priority. "
		"<i>forced</i> defaults to <i>true</i>")
	,311
	},
	{"random",			js_random,			1,	JSTYPE_NUMBER,	JSDOCSTR("max_number=<tt>100</tt>")
	,JSDOCSTR("return random integer between <tt>0</tt> and <i>max_number</i>-1")
	,310
	},		
	{"time",			js_time,			0,	JSTYPE_NUMBER,	""
	,JSDOCSTR("return current time and date in Unix (time_t) format "
		"(number of seconds since Jan-01-1970)")
	,310
	},		
	{"beep",			js_beep,			0,	JSTYPE_VOID,	JSDOCSTR("[frequency=<tt>500</tt>] [,duration=<tt>500</tt>]")
	,JSDOCSTR("produce a tone on the local speaker at specified frequency for specified duration (in milliseconds)")
	,310
	},		
	{"sound",			js_sound,			0,	JSTYPE_BOOLEAN,	JSDOCSTR("[filename]")
	,JSDOCSTR("play a waveform (.wav) sound file (currently, on Windows platforms only)")
	,310
	},		
	{"ctrl",			js_ctrl,			1,	JSTYPE_STRING,	JSDOCSTR("number or string")
	,JSDOCSTR("return ASCII control character representing character passed - Example: <tt>ctrl('C') returns '\3'</tt>")
	,311
	},
	{"ascii",			js_ascii,			1,	JSTYPE_UNDEF,	JSDOCSTR("[string text] or [number value]")
	,JSDOCSTR("convert single character to numeric ASCII value or vice-versa (returns number OR string)")
	,310
	},		
	{"ascii_str",		js_ascii_str,		1,	JSTYPE_STRING,	JSDOCSTR("text")
	,JSDOCSTR("convert extended-ASCII in text string to plain ASCII, returns modified string")
	,310
	},		
	{"strip_ctrl",		js_strip_ctrl,		1,	JSTYPE_STRING,	JSDOCSTR("text")
	,JSDOCSTR("strip control characters from string, returns modified string")
	,310
	},		
	{"strip_exascii",	js_strip_exascii,	1,	JSTYPE_STRING,	JSDOCSTR("text")
	,JSDOCSTR("strip extended-ASCII characters from string, returns modified string")
	,310
	},		
	{"skipsp",			js_skipsp,			1,	JSTYPE_STRING,	JSDOCSTR("text")
	,JSDOCSTR("skip (trim) white-space characters off <b>beginning</b> of string, returns modified string")
	,315
	},
	{"truncsp",			js_truncsp,			1,	JSTYPE_STRING,	JSDOCSTR("text")
	,JSDOCSTR("truncate (trim) white-space characters off <b>end</b> of string, returns modified string")
	,310
	},
	{"truncstr",		js_truncstr,		2,	JSTYPE_STRING,	JSDOCSTR("text, charset")
	,JSDOCSTR("truncate (trim) string at first char in <i>charset</i>, returns modified string")
	,310
	},		
	{"lfexpand",		js_lfexpand,		1,	JSTYPE_STRING,	JSDOCSTR("text")
	,JSDOCSTR("expand line-feeds (LF) to carriage-return/line-feeds (CRLF), returns modified string")
	,310
	},
	{"wildmatch",		js_wildmatch,		2,	JSTYPE_BOOLEAN, JSDOCSTR("[case_sensitive=<tt>false</tt>,] string [,pattern=<tt>'*'</tt>] [,path=<tt>false</tt>]")
	,JSDOCSTR("returns <tt>true</tt> if the <i>string</i> matches the wildcard <i>pattern</i> (wildcards supported are '*' and '?'), "
	"if <i>path</i> is <tt>true</tt>, '*' will not match path delimeter characters (e.g. '/')")
	,314
	},
	{"backslash",		js_backslash,		1,	JSTYPE_STRING,	JSDOCSTR("path")
	,JSDOCSTR("returns directory path with trailing (platform-specific) path delimeter "
		"(i.e. \"slash\" or \"backslash\")")
	,312
	},
	{"fullpath",		js_fullpath,		1,	JSTYPE_STRING,	JSDOCSTR("path")
	,JSDOCSTR("Creates an absolute or full path name for the specified relative path name.")
	,315
	},
	{"file_getname",	js_getfname,		1,	JSTYPE_STRING,	JSDOCSTR("path/filename")
	,JSDOCSTR("returns filename portion of passed path string")
	,311
	},
	{"file_getext",		js_getfext,			1,	JSTYPE_STRING,	JSDOCSTR("path/filename")
	,JSDOCSTR("returns file extension portion of passed path/filename string (including '.') "
		"or <i>undefined</i> if no extension is found")
	,311
	},
	{"file_getcase",	js_getfcase,		1,	JSTYPE_STRING,	JSDOCSTR("path/filename")
	,JSDOCSTR("returns correct case of filename (long version of filename on Windows) "
		"or <i>undefined</i> if the file doesn't exist")
	,311
	},
	{"file_cfgname",	js_cfgfname,		2,	JSTYPE_STRING,	JSDOCSTR("path, filename")
	,JSDOCSTR("returns completed configuration filename from supplied <i>path</i> and <i>filename</i>, "
	"optionally including the local hostname (e.g. <tt>path/file.<i>host</i>.<i>domain</i>.ext</tt> "
	"or <tt>path/file.<i>host</i>.ext</tt>) if such a variation of the filename exists")
	,312
	},
	{"file_getdosname",	js_dosfname,		1,	JSTYPE_STRING,	JSDOCSTR("path/filename")
	,JSDOCSTR("returns DOS-compatible (Micros~1 shortened) version of specified <i>path/filename</i>"
		"(on Windows only)<br>"
		"returns unmodified <i>path/filename</i> on other platforms")
	,315
	},
	{"file_exists",		js_fexist,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("path/filename")
	,JSDOCSTR("verify a file's existence")
	,310
	},		
	{"file_remove",		js_remove,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("path/filename")
	,JSDOCSTR("delete a file")
	,310
	},		
	{"file_removecase",	js_removecase,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("path/filename")
	,JSDOCSTR("delete files case insensitively")
	,314
	},		
	{"file_rename",		js_rename,			2,	JSTYPE_BOOLEAN,	JSDOCSTR("path/oldname, path/newname")
	,JSDOCSTR("rename a file, possibly moving it to another directory in the process")
	,311
	},
	{"file_copy",		js_fcopy,			2,	JSTYPE_BOOLEAN,	JSDOCSTR("path/source, path/destination")
	,JSDOCSTR("copy a file from one directory or filename to another")
	,311
	},
	{"file_backup",		js_backup,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("path/filename [,level=<tt>5</tt>] [,rename=<tt>false</tt>]")
	,JSDOCSTR("backup the specified <i>filename</i> as <tt>filename.<i>number</i>.extension</tt> "
		"where <i>number</i> is the backup number 0 through <i>level</i>-1 "
		"(default backup <i>level</i> is 5), "
		"if <i>rename</i> is <i>true</i>, the original file is renamed instead of copied "
		"(default is <i>false</i>)")
	,311
	},
	{"file_isdir",		js_isdir,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("path/filename")
	,JSDOCSTR("check if specified <i>filename</i> is a directory")
	,310
	},		
	{"file_attrib",		js_fattr,			1,	JSTYPE_NUMBER,	JSDOCSTR("path/filename")
	,JSDOCSTR("get a file's permissions/attributes")
	,310
	},		
	{"file_date",		js_fdate,			1,	JSTYPE_NUMBER,	JSDOCSTR("path/filename")
	,JSDOCSTR("get a file's last modified date/time (in time_t format)")
	,310
	},
	{"file_size",		js_flength,			1,	JSTYPE_NUMBER,	JSDOCSTR("path/filename")
	,JSDOCSTR("get a file's length (in bytes)")
	,310
	},
	{"file_utime",		js_utime,			3,	JSTYPE_BOOLEAN,	JSDOCSTR("path/filename [,access_time=<i>current</i>] [,mod_time=<i>current</i>]")
	,JSDOCSTR("change a file's last accessed and modification date/time (in time_t format), "
		"or change to current time")
	,311
	},
	{"file_touch",		js_ftouch,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("path/filename")
	,JSDOCSTR("updates a file's last modification date/time to current time, "
		"creating an empty file if it doesn't already exist")
	,311
	},
	{"file_mutex",		js_fmutex,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("path/filename [,text=<i>local_hostname</i>] [,max_age=<tt>0</tt>]")
	,JSDOCSTR("attempts to create an mutual-exclusion (e.g. lock) file, "
		"optionally with the contents of <i>text</i>. "
		"If a non-zero <i>max_age</i> (supported in v3.13b+) is specified "
		"and the lock file exists, but is older than this value (in seconds), "
		"it is presumed stale and removed/over-written")
	,312
	},
	{"file_compare",	js_fcompare,		2,	JSTYPE_BOOLEAN,	JSDOCSTR("path/file1, path/file2")
	,JSDOCSTR("compare 2 files, returning <i>true</i> if they are identical, <i>false</i> otherwise")
	,314
	},
	{"directory",		js_directory,		1,	JSTYPE_ARRAY,	JSDOCSTR("path/pattern [,flags=<tt>GLOB_MARK</tt>]")
	,JSDOCSTR("returns an array of directory entries, "
		"<i>pattern</i> is the path and filename or wildcards to search for (e.g. '/subdir/*.txt'), "
		"<i>flags</i> is a bitfield of optional <tt>glob</tt> flags (default is <tt>GLOB_MARK</tt>)")
	,310
	},
	{"dir_freespace",	js_freediskspace,	2,	JSTYPE_NUMBER,	JSDOCSTR("directory [,unit_size=<tt>1</tt>]")
	,JSDOCSTR("returns the amount of available disk space in the specified <i>directory</i> "
		"using the specified <i>unit_size</i> in bytes (default: 1), "
		"specify a <i>unit_size</i> of <tt>1024</tt> to return the available space in <i>kilobytes</i>.")
	,311
	},
	{"disk_size",		js_disksize,		2,	JSTYPE_NUMBER,	JSDOCSTR("directory [,unit_size=<tt>1</tt>]")
	,JSDOCSTR("returns the total disk size of the specified <i>directory</i> "
		"using the specified <i>unit_size</i> in bytes (default: 1), "
		"specify a <i>unit_size</i> of <tt>1024</tt> to return the total disk size in <i>kilobytes</i>.")
	,314
	},
	{"socket_select",	js_socket_select,	0,	JSTYPE_ARRAY,	JSDOCSTR("[array of socket objects or descriptors] [,timeout=<tt>0</tt>] [,write=<tt>false</tt>]")
	,JSDOCSTR("checks an array of socket objects or descriptors for read or write ability (default is <i>read</i>), "
		"default timeout value is 0.0 seconds (immediate timeout), "
		"returns an array of 0-based index values into the socket array, representing the sockets that were ready for reading or writing, or <i>null</i> on error")
	,311
	},
	{"mkdir",			js_mkdir,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("path/directory")
	,JSDOCSTR("make a directory")
	,310
	},		
	{"rmdir",			js_rmdir,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("path/directory")
	,JSDOCSTR("remove a directory")
	,310
	},		
	{"strftime",		js_strftime,		1,	JSTYPE_STRING,	JSDOCSTR("format [,time=<i>current</i>]")
	,JSDOCSTR("return a formatted time string (ala C strftime)")
	,310
	},		
	{"format",			js_format,			1,	JSTYPE_STRING,	JSDOCSTR("format [,args]")
	,JSDOCSTR("return a formatted string (ala the standard C <tt>sprintf</tt> function)")
	,310
	},
	{"html_encode",		js_html_encode,		1,	JSTYPE_STRING,	JSDOCSTR("text [,ex_ascii=<tt>true</tt>] [,white_space=<tt>true</tt>] [,ansi=<tt>true</tt>] [,ctrl_a=<tt>true</tt>] [, state (object)]")
	,JSDOCSTR("return an HTML-encoded text string (using standard HTML character entities), "
		"escaping IBM extended-ASCII, white-space characters, ANSI codes, and CTRL-A codes by default."
		"Optionally storing the current ANSI state in <i>state</i> object")
	,311
	},
	{"html_decode",		js_html_decode,		1,	JSTYPE_STRING,	JSDOCSTR("html")
	,JSDOCSTR("return a decoded HTML-encoded text string")
	,311
	},
	{"word_wrap",		js_word_wrap,		1,	JSTYPE_STRING,	JSDOCSTR("text [,line_length=<tt>79</tt> [, orig_line_length=<tt>79</tt> [, handle_quotes=<tt>true</tt>]]]]")
	,JSDOCSTR("returns a word-wrapped version of the text string argument optionally handing quotes magically, "
		"<i>line_length</i> defaults to <i>79</i> <i>orig_line_length</i> defaults to <i>79</i> "
		"and <i>handle_quotes</i> defaults to <i>true</i>")
	,311
	},
	{"quote_msg",		js_quote_msg,		1,	JSTYPE_STRING,	JSDOCSTR("text [,line_length=<tt>79</tt>] [,prefix=<tt>\" > \"</tt>]")
	,JSDOCSTR("returns a quoted version of the message text string argument, <i>line_length</i> defaults to <i>79</i>, "
		"<i>prefix</i> defaults to <tt>\" > \"</tt>")
	,311
	},
	{"rot13_translate",	js_rot13,			1,	JSTYPE_STRING,	JSDOCSTR("text")
	,JSDOCSTR("returns ROT13-translated version of text string (will encode or decode text)")
	,311
	},
	{"base64_encode",	js_b64_encode,		1,	JSTYPE_STRING,	JSDOCSTR("text")
	,JSDOCSTR("returns base64-encoded version of text string or <i>null</i> on error")
	,311
	},
	{"base64_decode",	js_b64_decode,		1,	JSTYPE_STRING,	JSDOCSTR("text")
	,JSDOCSTR("returns base64-decoded text string or <i>null</i> on error")
	,311
	},
	{"crc16_calc",		js_crc16,			1,	JSTYPE_NUMBER,	JSDOCSTR("text")
	,JSDOCSTR("calculate and return 16-bit CRC of text string")
	,311
	},		
	{"crc32_calc",		js_crc32,			1,	JSTYPE_NUMBER,	JSDOCSTR("text")
	,JSDOCSTR("calculate and return 32-bit CRC of text string")
	,311
	},		
	{"chksum_calc",		js_chksum,			1,	JSTYPE_NUMBER,	JSDOCSTR("text")
	,JSDOCSTR("calculate and return 32-bit checksum of text string")
	,311
	},
	{"md5_calc",		js_md5_calc,		1,	JSTYPE_STRING,	JSDOCSTR("text [,hex=<tt>false</tt>]")
	,JSDOCSTR("calculate and return 128-bit MD5 digest of text string, result encoded in base64 (default) or hexadecimal")
	,311
	},
	{"gethostbyname",	js_resolve_ip,		1,	JSTYPE_ALIAS },
	{"resolve_ip",		js_resolve_ip,		1,	JSTYPE_STRING,	JSDOCSTR("hostname")
	,JSDOCSTR("resolve IP address of specified hostname (AKA gethostbyname)")
	,311
	},
	{"gethostbyaddr",	js_resolve_host,	1,	JSTYPE_ALIAS },
	{"resolve_host",	js_resolve_host,	1,	JSTYPE_STRING,	JSDOCSTR("ip_address")
	,JSDOCSTR("resolve hostname of specified IP address (AKA gethostbyaddr)")
	,311
	},
	{"netaddr_type",	js_netaddr_type,	1,	JSTYPE_NUMBER,	JSDOCSTR("email_address")
	,JSDOCSTR("returns the proper message <i>net_type</i> for the specified <i>email_address</i>, "
		"(e.g. <tt>NET_INTERNET</tt> for Internet e-mail or <tt>NET_NONE</tt> for local e-mail)")
	,312
	},
	{"list_named_queues",js_list_named_queues,0,JSTYPE_ARRAY,	JSDOCSTR("")
	,JSDOCSTR("returns an array of <i>named queues</i> (created with the <i>Queue</i> constructor)")
	,312
	},
	{"flags_str",		js_flags_str,		1,	JSTYPE_UNDEF,	JSDOCSTR("[string] or [number]")
	,JSDOCSTR("convert a string of security flags (letters) into their numeric value or vice-versa "
	"(returns number OR string) - (added in v3.13)")
	,313
	},
	{0}
};

static jsConstIntSpec js_global_const_ints[] = {
	/* Numeric error constants from errno.h (platform-dependant) */
	{"EPERM"		,EPERM			},
	{"ENOENT"		,ENOENT			},
	{"ESRCH"		,ESRCH			},
	{"EIO"			,EIO			},
	{"ENXIO"		,ENXIO			},
	{"E2BIG"		,E2BIG			},
	{"ENOEXEC"		,ENOEXEC		},
	{"EBADF"		,EBADF			},
	{"ECHILD"		,ECHILD			},
	{"EAGAIN"		,EAGAIN			},
	{"ENOMEM"		,ENOMEM			},
	{"EACCES"		,EACCES			},
	{"EFAULT"		,EFAULT			},
	{"EBUSY"		,EBUSY			},
	{"EEXIST"		,EEXIST			},
	{"EXDEV"		,EXDEV			},
	{"ENODEV"		,ENODEV			},
	{"ENOTDIR"		,ENOTDIR		},
	{"EISDIR"		,EISDIR			},
	{"EINVAL"		,EINVAL			},
	{"ENFILE"		,ENFILE			},
	{"EMFILE"		,EMFILE			},
	{"ENOTTY"		,ENOTTY			},
	{"EFBIG"		,EFBIG			},
	{"ENOSPC"		,ENOSPC			},
	{"ESPIPE"		,ESPIPE			},
	{"EROFS"		,EROFS			},
	{"EMLINK"		,EMLINK			},
	{"EPIPE"		,EPIPE			},
	{"EDOM"			,EDOM			},
	{"ERANGE"		,ERANGE			},
	{"EDEADLOCK"	,EDEADLOCK		},
	{"ENAMETOOLONG"	,ENAMETOOLONG	},
	{"ENOTEMPTY"	,ENOTEMPTY		},

	/* Socket errors */
	{"EINTR"			,EINTR			},
	{"ENOTSOCK"			,ENOTSOCK		},
	{"EMSGSIZE"			,EMSGSIZE		},
	{"EWOULDBLOCK"		,EWOULDBLOCK	},
	{"EPROTOTYPE"		,EPROTOTYPE		},
	{"ENOPROTOOPT"		,ENOPROTOOPT	},
	{"EPROTONOSUPPORT"	,EPROTONOSUPPORT},
	{"ESOCKTNOSUPPORT"	,ESOCKTNOSUPPORT},
	{"EOPNOTSUPP"		,EOPNOTSUPP		},
	{"EPFNOSUPPORT"		,EPFNOSUPPORT	},
	{"EAFNOSUPPORT"		,EAFNOSUPPORT	},
	{"EADDRINUSE"		,EADDRINUSE		},
	{"EADDRNOTAVAIL"	,EADDRNOTAVAIL	},
	{"ECONNABORTED"		,ECONNABORTED	},
	{"ECONNRESET"		,ECONNRESET		},
	{"ENOBUFS"			,ENOBUFS		},
	{"EISCONN"			,EISCONN		},
	{"ENOTCONN"			,ENOTCONN		},
	{"ESHUTDOWN"		,ESHUTDOWN		},
	{"ETIMEDOUT"		,ETIMEDOUT		},
	{"ECONNREFUSED"		,ECONNREFUSED	},
	{"EINPROGRESS"		,EINPROGRESS	},

	/* Log priority values from syslog.h/sbbsdefs.h (possibly platform-dependant) */
	{"LOG_EMERG"		,LOG_EMERG		},
	{"LOG_ALERT"		,LOG_ALERT		},
	{"LOG_CRIT"			,LOG_CRIT		},
	{"LOG_ERR"			,LOG_ERR		},
	{"LOG_ERROR"		,LOG_ERR		},
	{"LOG_WARNING"		,LOG_WARNING	},
	{"LOG_NOTICE"		,LOG_NOTICE		},
	{"LOG_INFO"			,LOG_INFO		},
	{"LOG_DEBUG"		,LOG_DEBUG		},

	/* Other useful constants */
	{"INVALID_SOCKET"	,INVALID_SOCKET	},

	/* Terminator (Governor Arnold) */
	{0}
};

static void js_global_finalize(JSContext *cx, JSObject *obj)
{
	private_t* p;

	p=(private_t*)JS_GetPrivate(cx,obj);

	if(p!=NULL)
		free(p);

	p=NULL;
	JS_SetPrivate(cx,obj,p);
}

static JSBool js_global_resolve(JSContext *cx, JSObject *obj, jsval id)
{
	char*		name=NULL;
	private_t*	p;
	JSBool		ret=JS_TRUE;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if(id != JSVAL_NULL)
		name=JS_GetStringBytes(JSVAL_TO_STRING(id));

	if(p->methods) {
		if(js_SyncResolve(cx, obj, name, NULL, p->methods, NULL, 0)==JS_FALSE)
			ret=JS_FALSE;
	}
	if(js_SyncResolve(cx, obj, name, js_global_properties, js_global_functions, js_global_const_ints, 0)==JS_FALSE)
		ret=JS_FALSE;
	return(ret);
}

static JSBool js_global_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_global_resolve(cx, obj, JSVAL_NULL));
}

static JSClass js_global_class = {
     "Global"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_system_get			/* getProperty	*/
	,JS_PropertyStub		/* setProperty	*/
	,js_global_enumerate	/* enumerate	*/
	,js_global_resolve		/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_global_finalize		/* finalize		*/
};

JSObject* DLLCALL js_CreateGlobalObject(JSContext* cx, scfg_t* cfg, jsSyncMethodSpec* methods)
{
	JSObject*	glob;
	private_t*	p;

	if((p = (private_t*)malloc(sizeof(private_t)))==NULL)
		return(NULL);

	p->cfg = cfg;
	p->methods = methods;

	if((glob = JS_NewObject(cx, &js_global_class, NULL, NULL)) ==NULL)
		return(NULL);

	if(!JS_SetPrivate(cx, glob, p))	/* Store a pointer to scfg_t and the new methods */
		return(NULL);

	if (!JS_InitStandardClasses(cx, glob))
		return(NULL);

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,glob
		,"Top-level functions and properties (common to all servers and services)",310);
#endif

	return(glob);
}

JSObject* DLLCALL js_CreateCommonObjects(JSContext* js_cx
										,scfg_t* cfg				/* common */
										,scfg_t* node_cfg			/* node-specific */
										,jsSyncMethodSpec* methods	/* global */
										,time_t uptime				/* system */
										,char* host_name			/* system */
										,char* socklib_desc			/* system */
										,js_branch_t* branch		/* js */
										,client_t* client			/* client */
										,SOCKET client_socket		/* client */
										,js_server_props_t* props	/* server */
										)
{
	JSObject*	js_glob;

	if(node_cfg==NULL)
		node_cfg=cfg;

	/* Global Object */
	if((js_glob=js_CreateGlobalObject(js_cx, cfg, methods))==NULL)
		return(NULL);

	/* System Object */
	if(js_CreateSystemObject(js_cx, js_glob, node_cfg, uptime, host_name, socklib_desc)==NULL)
		return(NULL);

	/* Internal JS Object */
	if(branch!=NULL 
		&& js_CreateInternalJsObject(js_cx, js_glob, branch)==NULL)
		return(NULL);

	/* Client Object */
	if(client!=NULL 
		&& js_CreateClientObject(js_cx, js_glob, "client", client, client_socket)==NULL)
		return(NULL);

	/* Server */
	if(props!=NULL
		&& js_CreateServerObject(js_cx, js_glob, props)==NULL)
		return(NULL);

	/* Socket Class */
	if(js_CreateSocketClass(js_cx, js_glob)==NULL)
		return(NULL);

	/* Queue Class */
	if(js_CreateQueueClass(js_cx, js_glob)==NULL)
		return(NULL);

	/* MsgBase Class */
	if(js_CreateMsgBaseClass(js_cx, js_glob, cfg)==NULL)
		return(NULL);

	/* File Class */
	if(js_CreateFileClass(js_cx, js_glob)==NULL)
		return(NULL);

	/* User class */
	if(js_CreateUserClass(js_cx, js_glob, cfg)==NULL) 
		return(NULL);

	/* Area Objects */
	if(!js_CreateUserObjects(js_cx, js_glob, cfg, NULL, NULL, NULL)) 
		return(NULL);

	return(js_glob);
}


#endif	/* JAVSCRIPT */
