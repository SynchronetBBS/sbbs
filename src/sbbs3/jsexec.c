/* jsexec.c */

/* Execute a Synchronet JavaScript from the command-line */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#define JAVASCRIPT

#include "sbbs.h"

JSRuntime*	js_runtime;
JSContext*	js_cx;
JSObject*	js_glob;
ulong		js_loop=0;
scfg_t		scfg;
ulong		js_max_bytes=JAVASCRIPT_MAX_BYTES;

char *usage="\nusage: jsexec [-opts] module[.js] [args]\n"
	"\navailable opts:"
	"\n"
	;

static JSBool
js_log(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN		i;
    JSString*	str;

    for (i = 0; i < argc; i++) {
		if((str=JS_ValueToString(cx, argv[i]))==NULL)
		    return(JS_FALSE);
		puts(JS_GetStringBytes(str));
	}

	*rval = JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN		i;
    JSString *	str;

    for (i = 0; i < argc; i++) {
		if((str=JS_ValueToString(cx, argv[i]))==NULL)
		    return(JS_FALSE);
		printf("%s",JS_GetStringBytes(str));
	}
	printf("\n");

	*rval = JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_printf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
    uintN		i;
	JSString *	fmt;
    JSString *	str;
	va_list		arglist[64];

	if((fmt = JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	memset(arglist,0,sizeof(arglist));	// Initialize arglist to NULLs

    for (i = 1; i < argc && i<sizeof(arglist)/sizeof(arglist[0]); i++) {
		if(JSVAL_IS_STRING(argv[i])) {
			if((str=JS_ValueToString(cx, argv[i]))==NULL)
			    return(JS_FALSE);
			arglist[i-1]=JS_GetStringBytes(str);
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

	printf("%s",p);
	JS_smprintf_free(p);

	*rval = JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_alert(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *	str;

	if((str=JS_ValueToString(cx, argv[0]))==NULL)
	    return(JS_FALSE);

	printf("!%s\n",JS_GetStringBytes(str));

	*rval = JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_confirm(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *	str;

	if((str=JS_ValueToString(cx, argv[0]))==NULL)
	    return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(FALSE);
	return(JS_TRUE);
}

static JSBool
js_prompt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		instr[81];
    JSString *	prompt;
    JSString *	str;

	if((prompt=JS_ValueToString(cx, argv[0]))==NULL)
	    return(JS_FALSE);

	if(argc>1) {
		if((str=JS_ValueToString(cx, argv[1]))==NULL)
		    return(JS_FALSE);
		SAFECOPY(instr,JS_GetStringBytes(str));
	} else
		instr[0]=0;

	printf("%s: ",JS_GetStringBytes(prompt));

	if(!fgets(instr,sizeof(instr),stdin)) {
		*rval = JSVAL_NULL;
		return(JS_TRUE);
	}

	if((str=JS_NewStringCopyZ(cx, instr))==NULL)
	    return(JS_FALSE);

	*rval = STRING_TO_JSVAL(str);
    return(JS_TRUE);
}

static jsMethodSpec js_global_functions[] = {
	{"log",				js_log,				1},
    {"print",           js_print,           0},
    {"printf",          js_printf,          1},	
	{"alert",			js_alert,			1},
	{"prompt",			js_prompt,			1},
	{"confirm",			js_confirm,			1},
    {0}
};

static void
js_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
	char	line[64];
	char	file[MAX_PATH+1];
	const char*	warning;

	if(report==NULL) {
		fprintf(stderr,"!JavaScript: %s", message);
		return;
    }

	if(report->filename)
		sprintf(file," %s",report->filename);
	else
		file[0]=0;

	if(report->lineno)
		sprintf(line," line %d",report->lineno);
	else
		line[0]=0;

	if(JSREPORT_IS_WARNING(report->flags)) {
		if(JSREPORT_IS_STRICT(report->flags))
			warning="strict warning";
		else
			warning="warning";
	} else
		warning="";

	fprintf(stderr,"!JavaScript %s%s%s: %s",warning,file,line,message);
}

static JSBool
js_BranchCallback(JSContext *cx, JSScript *script)
{
	js_loop++;

#if 0 
	/* Terminated? */
	if(sbbs->terminated) {
		JS_ReportError(cx,"Terminated");
		sbbs->js_loop=0;
		return(JS_FALSE);
	}
#endif
	/* Infinite loop? */
	if(js_loop>JAVASCRIPT_BRANCH_LIMIT) {
		JS_ReportError(cx,"Infinite loop (%lu branches) detected",js_loop);
		js_loop=0;
		return(JS_FALSE);
	}
	/* Give up timeslices every once in a while */
	if(!(js_loop%JAVASCRIPT_YIELD_FREQUENCY))
		YIELD();

	if(!(js_loop%JAVASCRIPT_GC_FREQUENCY))
		JS_MaybeGC(cx);

    return(JS_TRUE);
}

static BOOL js_init(void)
{
	fprintf(stderr,"JavaScript: Creating runtime: %lu bytes\n"
		,js_max_bytes);

	if((js_runtime = JS_NewRuntime(js_max_bytes))==NULL)
		return(FALSE);

	fprintf(stderr,"JavaScript: Initializing context (stack: %lu bytes)\n"
		,JAVASCRIPT_CONTEXT_STACK);

    if((js_cx = JS_NewContext(js_runtime, JAVASCRIPT_CONTEXT_STACK))==NULL)
		return(FALSE);

	JS_SetErrorReporter(js_cx, js_ErrorReporter);

	/* Global Object */
	if((js_glob=js_CreateGlobalObject(js_cx, &scfg, js_global_functions))==NULL)
		return(FALSE);

	/* System Object */
	if(js_CreateSystemObject(js_cx, js_glob, &scfg, time(NULL), scfg.sys_inetaddr)==NULL)
		return(FALSE);

	/* Socket Class */
	if(js_CreateSocketClass(js_cx, js_glob)==NULL)
		return(FALSE);

	/* MsgBase Class */
	if(js_CreateMsgBaseClass(js_cx, js_glob, &scfg)==NULL)
		return(FALSE);

	/* File Class */
	if(js_CreateFileClass(js_cx, js_glob)==NULL)
		return(FALSE);

	/* User class */
	if(js_CreateUserClass(js_cx, js_glob, &scfg)==NULL) 
		return(FALSE);

	/* Area Objects */
	if(!js_CreateUserObjects(js_cx, js_glob, &scfg, NULL, NULL, NULL)) 
		return(FALSE);

	return(TRUE);
}

long js_exec(const char *fname, char** args)
{
	int			argc=0;
	char		path[MAX_PATH+1];
	JSObject*	js_scope=NULL;
	JSScript*	js_script=NULL;
	JSString*	arg;
	JSObject*	argv;
	jsval		val;
	jsval		rval;
	
	if(strcspn(fname,"/\\")==strlen(fname)) {
		sprintf(path,"%s%s",scfg.mods_dir,fname);
		if(scfg.mods_dir[0]==0 || !fexistcase(path))
			sprintf(path,"%s%s",scfg.exec_dir,fname);
	} else
		sprintf(path,"%.*s",(int)sizeof(path)-4,fname);
	/* Add extension if not specified */
	if(!strchr(path,'.'))
		strcat(path,".js");

	if(!fexistcase(path)) {
		fprintf(stderr,"!Module file (%s) doesn't exist\n",path);
		return(-1); 
	}

	js_scope=JS_NewObject(js_cx, NULL, NULL, js_glob);

	if(js_scope==NULL) {
		fprintf(stderr,"!Error creating JS scope\n");
		return(-1);
	}

	argv=JS_NewArrayObject(js_cx, 0, NULL);

	for(argc=0;args[argc];argc++) {
		arg = JS_NewStringCopyZ(js_cx, args[argc]);
		if(arg==NULL)
			break;
		val=STRING_TO_JSVAL(arg);
		if(!JS_SetElement(js_cx, argv, argc, &val))
			break;
	}
	JS_DefineProperty(js_cx, js_scope, "argv", OBJECT_TO_JSVAL(argv)
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);
	JS_DefineProperty(js_cx, js_scope, "argc", INT_TO_JSVAL(argc)
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	if((js_script=JS_CompileFile(js_cx, js_scope, path))==NULL) {
		fprintf(stderr,"!Error executing %s\n",fname);
		return(-1);
	}

	JS_SetBranchCallback(js_cx, js_BranchCallback);

	JS_ExecuteScript(js_cx, js_scope, js_script, &rval);

	JS_DestroyScript(js_cx, js_script);

	JS_ClearScope(js_cx, js_scope);

	JS_GC(js_cx);

	return(JSVAL_TO_INT(rval));
}

/*********************/
/* Entry point (duh) */
/*********************/
int main(int argc, char **argv)
{
	char error[512];
	char revision[16];
	char* module=NULL;
	char* p;

	sscanf("$Revision$", "%*s %s", revision);

	fprintf(stderr,"\nJSexec v%s%c-%s (rev %s) - "
		"Execute Synchronet JavaScript Modules\n"
		,VERSION,REVISION
		,PLATFORM_DESC
		,revision
		);

	if(argc<2) {
		fprintf(stderr,usage);
		return(1); 
	}

	module=argv[1];

	if(module==NULL) {
		fprintf(stderr,usage);
		return(1); 
	}

	p=getenv("SBBSCTRL");
	if(p==NULL) {
		fprintf(stderr,"\nSBBSCTRL environment variable not set.\n");
		fprintf(stderr,"\nExample: SET SBBSCTRL=/sbbs/ctrl\n");
		exit(1); 
	}

	memset(&scfg,0,sizeof(scfg));
	scfg.size=sizeof(scfg);
	SAFECOPY(scfg.ctrl_dir,p);

	if(chdir(scfg.ctrl_dir)!=0)
		fprintf(stderr,"!ERROR changing directory to: %s", scfg.ctrl_dir);

	printf("\nLoading configuration files from %s\n",scfg.ctrl_dir);
	if(!load_cfg(&scfg,NULL,TRUE,error)) {
		fprintf(stderr,"!ERROR loading configuration files: %s\n",error);
		exit(1);
	}
	prep_dir(scfg.data_dir, scfg.temp_dir, sizeof(scfg.temp_dir));

	if(!(scfg.sys_misc&SM_LOCAL_TZ))
		putenv("TZ=UTC0");

	if(!js_init())
		return(1);

	return(js_exec(module,&argv[2]));
}

