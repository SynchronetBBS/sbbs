/* js_file.c */

/* Synchronet JavaScript "File" Object */

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

typedef struct
{
	FILE*	fp;
	char	name[MAX_PATH+1];
	char	mode[4];
	uchar	etx;
	BOOL	external;	/* externally created, don't close */
	BOOL	debug;

} private_t;

static const char* getprivate_failure = "line %d %s JS_GetPrivate failed";

static void dbprintf(BOOL error, private_t* p, char* fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

	if(p==NULL || (!p->debug && !error))
		return;

    va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
	
	lprintf("%04u File %s%s",p->fp ? fileno(p->fp) : 0,error ? "ERROR: ":"",sbuf);
}

/* Converts fopen() style 'mode' string into open() style 'flags' integer */

static int fopenflags(char *mode)
{
	int flags=0;

	if(strchr(mode,'b'))
		flags|=O_BINARY;
	else
		flags|=O_TEXT;

	if(strchr(mode,'w')) {
		flags|=O_CREAT|O_TRUNC;
		if(strchr(mode,'+'))
			flags|=O_RDWR;
		else
			flags|=O_WRONLY;
		return(flags);
	}

	if(strchr(mode,'a')) {
		flags|=O_CREAT|O_APPEND;
		if(strchr(mode,'+'))
			flags|=O_RDWR;
		else
			flags|=O_WRONLY;
		return(flags);
	}

	if(strchr(mode,'r')) {
		if(strchr(mode,'+'))
			flags|=O_RDWR;
		else
			flags|=O_RDONLY;
	}

	return(flags);
}

/* File Object Methods */

static JSBool
js_open(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		mode="w+";	/* default mode */
	BOOL		shareable=FALSE;
	int			file;
	uintN		i;
	jsint		bufsize=2*1024;
	JSString*	str;
	private_t*	p;

	*rval = JSVAL_FALSE;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(p->fp!=NULL)  
		return(JS_TRUE);

	for(i=0;i<argc;i++) {
		if(JSVAL_IS_STRING(argv[i])) {	/* mode */
			if((str = JS_ValueToString(cx, argv[i]))==NULL) {
				JS_ReportError(cx,"Invalid mode specified: %s",str);
				return(JS_TRUE);
			}
			mode=JS_GetStringBytes(str);
		} else if(JSVAL_IS_BOOLEAN(argv[i]))	/* shareable */
			shareable=JSVAL_TO_BOOLEAN(argv[i]);
		else	/* bufsize */
			JS_ValueToInt32(cx,argv[i],&bufsize);
	}
	SAFECOPY(p->mode,mode);

	if(shareable)
		p->fp=fopen(p->name,p->mode);
	else {
		if((file=nopen(p->name,fopenflags(p->mode)))!=-1) {
			if((p->fp=fdopen(file,p->mode))==NULL)
				close(file);
		}
	}
	if(p->fp!=NULL) {
		*rval = JSVAL_TRUE;
		dbprintf(FALSE, p, "opened: %s",p->name);
		if(!bufsize)
			setvbuf(p->fp,NULL,_IONBF,0);	/* no buffering */
		else
			setvbuf(p->fp,NULL,_IOFBF,bufsize);
	}

	return(JS_TRUE);
}


static JSBool
js_close(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	*rval = JSVAL_VOID;

	if(p->fp==NULL)
		return(JS_TRUE);

	fclose(p->fp);

	dbprintf(FALSE, p, "closed");

	p->fp=NULL; 

	return(JS_TRUE);
}

static JSBool
js_read(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		cp;
	char*		buf;
	int32		len=512;
	JSString*	str;
	private_t*	p;

	*rval = JSVAL_NULL;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(p->fp==NULL)
		return(JS_TRUE);

	if(argc)
		JS_ValueToInt32(cx,argv[0],&len);

	if((buf=malloc(len+1))==NULL)
		return(JS_TRUE);

	len = fread(buf,1,len,p->fp);
	if(len<0) 
		len=0;
	buf[len]=0;

	if(p->etx) {
		cp=strchr(buf,p->etx);
		if(cp) *cp=0; 
	}

	str = JS_NewStringCopyZ(cx, buf);

	free(buf);

	if(str==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(str);

	dbprintf(FALSE, p, "read %u bytes",len);
		
	return(JS_TRUE);
}

static JSBool
js_readln(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		cp;
	char*		buf;
	int32		len=512;
	JSString*	js_str;
	private_t*	p;

	*rval = JSVAL_NULL;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(p->fp==NULL)
		return(JS_TRUE);
	
	if(argc)
		JS_ValueToInt32(cx,argv[0],&len);

	if((buf=malloc(len))==NULL)
		return(JS_TRUE);

	if(fgets(buf,len,p->fp)!=NULL) {
		len=strlen(buf);
		while(len>0 && (buf[len-1]=='\r' || buf[len-1]=='\n'))
			len--;
		buf[len]=0;
		if(p->etx) {
			cp=strchr(buf,p->etx);
			if(cp) *cp=0; 
		}
		if((js_str=JS_NewStringCopyZ(cx,buf))!=NULL)
			*rval = STRING_TO_JSVAL(js_str);
	}

	free(buf);

	return(JS_TRUE);
}

static JSBool
js_readbin(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	BYTE		b;
	WORD		w;
	DWORD		l;
	size_t		size=sizeof(DWORD);
	private_t*	p;

	*rval = INT_TO_JSVAL(-1);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(p->fp==NULL)
		return(JS_TRUE);

	if(argc) 
		JS_ValueToInt32(cx,argv[0],(int32*)&size);

	switch(size) {
		case sizeof(BYTE):
			if(fread(&b,1,size,p->fp)==size)
				*rval = INT_TO_JSVAL(b);
			break;
		case sizeof(WORD):
			if(fread(&w,1,size,p->fp)==size)
				*rval = INT_TO_JSVAL(w);
			break;
		case sizeof(DWORD):
			if(fread(&l,1,size,p->fp)==size)
				*rval = INT_TO_JSVAL(l);
			break;
	}
		
	return(JS_TRUE);
}

static JSBool
js_readall(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsint       len=0;
    jsval       line;
    JSObject*	array;
	private_t*	p;

	*rval = JSVAL_NULL;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(p->fp==NULL)
		return(JS_TRUE);

    array = JS_NewArrayObject(cx, 0, NULL);

    while(!feof(p->fp)) {
		js_readln(cx, obj, 0, NULL, &line); 
		if(line==JSVAL_NULL)
			break;
        if(!JS_SetElement(cx, array, len++, &line))
			break;
	}
    *rval = OBJECT_TO_JSVAL(array);

    return(JS_TRUE);
}

static JSBool
js_write(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		cp;
	size_t		len;	/* string length */
	size_t		tlen;	/* total length to write (may be greater than len) */
	JSString*	str;
	private_t*	p;

	*rval = JSVAL_FALSE;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(p->fp==NULL)
		return(JS_TRUE);

	str = JS_ValueToString(cx, argv[0]);
	cp = JS_GetStringBytes(str);
	len = strlen(cp);
	tlen = len;
	if(argc>1) {
		JS_ValueToInt32(cx,argv[1],(int32*)&tlen);
		if(len>tlen)
			len=tlen;
	}
	if(fwrite(cp,1,len,p->fp)==len) {
		if(tlen>len) {
			len=tlen-len;
			if((cp=malloc(len))==NULL) {
				dbprintf(TRUE, p, "malloc failure of %u bytes", len);
				*rval = JSVAL_FALSE;
				return(JS_TRUE);
			}
			memset(cp,p->etx,len);
			fwrite(cp,1,len,p->fp);
			free(cp);
		}
		dbprintf(FALSE, p, "wrote %u bytes",tlen);
		*rval = JSVAL_TRUE;
	} else 
		dbprintf(TRUE, p, "write of %u bytes failed",len);
		
	return(JS_TRUE);
}

static JSBool
js_writeln(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		cp="";
	JSString*	str;
	private_t*	p;

	*rval = JSVAL_FALSE;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(p->fp==NULL)
		return(JS_TRUE);

	if(argc) {
		if((str = JS_ValueToString(cx, argv[0]))==NULL) {
			JS_ReportError(cx,"JS_ValueToString failed");
			return(JS_FALSE);
		}
		cp = JS_GetStringBytes(str);
	}

	if(fprintf(p->fp,"%s\n",cp)!=0)
		*rval = JSVAL_TRUE;

	return(JS_TRUE);
}

static JSBool
js_writebin(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	BYTE		b;
	WORD		w;
	DWORD		l;
	int32		val=0;
	size_t		wr=0;
	size_t		size=sizeof(DWORD);
	private_t*	p;

	*rval = JSVAL_FALSE;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(p->fp==NULL)
		return(JS_TRUE);

	if(argc>1) 
		JS_ValueToInt32(cx,argv[1],(int32*)&size);

	switch(size) {
		case sizeof(BYTE):
			JS_ValueToInt32(cx,argv[0],&val);
			b = (BYTE)val;
			wr=fwrite(&b,1,size,p->fp);
			break;
		case sizeof(WORD):
			JS_ValueToInt32(cx,argv[0],&val);
			w = (WORD)val;
			wr=fwrite(&w,1,size,p->fp);
			break;
		case sizeof(DWORD):
			JS_ValueToInt32(cx,argv[0],&val);
			l = val;
			wr=fwrite(&l,1,size,p->fp);
			break;
		default:	
			/* unknown size */
			dbprintf(TRUE, p, "unsupported binary write size: %d",size);
			break;
	}
	if(wr==size)
		*rval = JSVAL_TRUE;
		
	return(JS_TRUE);
}

static JSBool
js_writeall(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsuint      i;
    jsuint      limit;
    JSObject*	array;
    jsval       elemval;
	private_t*	p;

	*rval = JSVAL_FALSE;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(p->fp==NULL)
		return(JS_TRUE);

	if(!JSVAL_IS_OBJECT(argv[0]))
		return(JS_TRUE);

    array = JSVAL_TO_OBJECT(argv[0]);

    if(!JS_IsArrayObject(cx, array))
		return(JS_TRUE);

    if(!JS_GetArrayLength(cx, array, &limit))
		return(JS_FALSE);

    *rval = JSVAL_TRUE;

    for(i=0;i<limit;i++) {
        if(!JS_GetElement(cx, array, i, &elemval))
			break;
        js_writeln(cx, obj, 1, &elemval, rval);
		if(*rval!=JSVAL_TRUE)
			break;
    }

    return(JS_TRUE);
}

static JSBool
js_lock(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		offset=0;
	int32		len=0;
	private_t*	p;

	*rval = JSVAL_FALSE;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(p->fp==NULL)
		return(JS_TRUE);

	/* offset */
	if(argc)
		JS_ValueToInt32(cx,argv[0],&offset);

	/* length */
	if(argc>1)
		JS_ValueToInt32(cx,argv[1],&len);

	if(len==0)
		len=filelength(fileno(p->fp))-offset;

	if(lock(fileno(p->fp),offset,len)==0)
		*rval = JSVAL_TRUE;

	return(JS_TRUE);
}

static JSBool
js_unlock(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		offset=0;
	int32		len=0;
	private_t*	p;

	*rval = JSVAL_FALSE;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(p->fp==NULL)
		return(JS_TRUE);

	/* offset */
	if(argc)
		JS_ValueToInt32(cx,argv[0],&offset);

	/* length */
	if(argc>1)
		JS_ValueToInt32(cx,argv[1],&len);

	if(len==0)
		len=filelength(fileno(p->fp))-offset;

	if(unlock(fileno(p->fp),offset,len)==0)
		*rval = JSVAL_TRUE;

	return(JS_TRUE);
}

static JSBool
js_delete(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(p->fp!=NULL) {	/* close it if it's open */
		fclose(p->fp);
		p->fp=NULL;
	}

	*rval = BOOLEAN_TO_JSVAL(remove(p->name)==0);

	return(JS_TRUE);
}

static JSBool
js_flush(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(p->fp==NULL)
		*rval = JSVAL_FALSE;
	else 
		*rval = BOOLEAN_TO_JSVAL(fflush(p->fp)==0);

	return(JS_TRUE);
}

static JSBool
js_clear_error(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(p->fp==NULL)
		*rval = JSVAL_FALSE;
	else  {
		clearerr(p->fp);
		*rval = JSVAL_TRUE;
	}

	return(JS_TRUE);
}

static JSBool
js_fprintf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		cp;
    uintN		i;
	JSString *	fmt;
    JSString *	str;
	va_list		arglist[64];
	private_t*	p;

	*rval = JSVAL_FALSE;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(p->fp==NULL)
		return(JS_TRUE);

	if((fmt=JS_ValueToString(cx, argv[0]))==NULL) {
		JS_ReportError(cx,"JS_ValueToString failed");
		return(JS_FALSE);
	}

	memset(arglist,0,sizeof(arglist));	/* Initialize arglist to NULLs */

    for (i = 1; i < argc && i<sizeof(arglist)/sizeof(arglist[0]); i++) {
		if(JSVAL_IS_STRING(argv[i])) {
			if((str=JS_ValueToString(cx, argv[i]))==NULL) {
				JS_ReportError(cx,"JS_ValueToString failed");
			    return(JS_FALSE);
			}
			arglist[i-1]=JS_GetStringBytes(str);	/* exception here July-29-2002 */
		}
		else if(JSVAL_IS_DOUBLE(argv[i]))
			arglist[i-1]=(char*)(unsigned long)*JSVAL_TO_DOUBLE(argv[i]);
		else if(JSVAL_IS_INT(argv[i]) || JSVAL_IS_BOOLEAN(argv[i]))
			arglist[i-1]=(char *)JSVAL_TO_INT(argv[i]);
		else
			arglist[i-1]=NULL;
	}

	if((cp=JS_vsmprintf(JS_GetStringBytes(fmt),(char*)arglist))==NULL) {
		JS_ReportError(cx,"JS_vsmprintf failed");
		return(JS_FALSE);
	}

	*rval = INT_TO_JSVAL(fwrite(cp,1,strlen(cp),p->fp));
	JS_smprintf_free(cp);
	
    return(JS_TRUE);
}


/* File Object Properites */
enum {
	 FILE_PROP_NAME		
	,FILE_PROP_MODE
	,FILE_PROP_ETX
	,FILE_PROP_EXISTS	
	,FILE_PROP_DATE		
	,FILE_PROP_IS_OPEN	
	,FILE_PROP_EOF		
	,FILE_PROP_ERROR	
	,FILE_PROP_DESCRIPTOR
	,FILE_PROP_DEBUG	
	,FILE_PROP_POSITION	
	,FILE_PROP_LENGTH	
	,FILE_PROP_ATTRIBUTES
};


static JSBool js_file_set(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint       tiny;
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

    tiny = JSVAL_TO_INT(id);

	dbprintf(FALSE, p, "setting property %d",tiny);

	switch(tiny) {
		case FILE_PROP_DEBUG:
			p->debug = JSVAL_TO_BOOLEAN(*vp);
			break;
		case FILE_PROP_POSITION:
			if(p->fp!=NULL)
				fseek(p->fp,JSVAL_TO_INT(*vp),SEEK_SET);
			break;
		case FILE_PROP_LENGTH:
			if(p->fp!=NULL)
				chsize(fileno(p->fp),JSVAL_TO_INT(*vp));
			break;
		case FILE_PROP_ATTRIBUTES:
			CHMOD(p->name,JSVAL_TO_INT(*vp));
			break;
		case FILE_PROP_ETX:
			p->etx = (uchar)JSVAL_TO_INT(*vp);
			break;
	}

	return(JS_TRUE);
}

static JSBool js_file_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint       tiny;
	JSString*	js_str;
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

    tiny = JSVAL_TO_INT(id);

#if 0 /* just too much */
	dbprintf(FALSE, sock, "getting property %d",tiny);
#endif

	switch(tiny) {
		case FILE_PROP_NAME:
			if((js_str=JS_NewStringCopyZ(cx, p->name))==NULL)
				return(JS_FALSE);
			*vp = STRING_TO_JSVAL(js_str);
			break;
		case FILE_PROP_MODE:
			if((js_str=JS_NewStringCopyZ(cx, p->mode))==NULL)
				return(JS_FALSE);
			*vp = STRING_TO_JSVAL(js_str);
			break;
		case FILE_PROP_EXISTS:
			if(p->fp)	/* open? */
				*vp = JSVAL_TRUE;
			else
				*vp = BOOLEAN_TO_JSVAL(fexist(p->name));
			break;
		case FILE_PROP_DATE:
			*vp = INT_TO_JSVAL(fdate(p->name));
			break;
		case FILE_PROP_IS_OPEN:
			*vp = BOOLEAN_TO_JSVAL(p->fp!=NULL);
			break;
		case FILE_PROP_EOF:
			if(p->fp)
				*vp = BOOLEAN_TO_JSVAL(feof(p->fp)!=0);
			else
				*vp = JSVAL_TRUE;
			break;
		case FILE_PROP_ERROR:
			if(p->fp)
				*vp = INT_TO_JSVAL(ferror(p->fp));
			else
				*vp = INT_TO_JSVAL(0);
			break;
		case FILE_PROP_POSITION:
			if(p->fp)
				*vp = INT_TO_JSVAL(ftell(p->fp));
			else
				*vp = INT_TO_JSVAL(-1);
			break;
		case FILE_PROP_LENGTH:
			if(p->fp)	/* open? */
				*vp = INT_TO_JSVAL(filelength(fileno(p->fp)));
			else
				*vp = INT_TO_JSVAL(flength(p->name));
			break;
		case FILE_PROP_ATTRIBUTES:
			*vp = INT_TO_JSVAL(getfattr(p->name));
			break;
		case FILE_PROP_DEBUG:
			*vp = INT_TO_JSVAL(p->debug);
			break;
		case FILE_PROP_DESCRIPTOR:
			if(p->fp)
				*vp = INT_TO_JSVAL(fileno(p->fp));
			else
				*vp = INT_TO_JSVAL(-1);
			break;
		case FILE_PROP_ETX:
			*vp = INT_TO_JSVAL(p->etx);
			break;
	}

	return(TRUE);
}

#define FILE_PROP_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static struct JSPropertySpec js_file_properties[] = {
/*		 name				,tinyid					,flags,				getter,	setter	*/
	{	"name"				,FILE_PROP_NAME			,FILE_PROP_FLAGS,	NULL,NULL},
	{	"mode"				,FILE_PROP_MODE			,FILE_PROP_FLAGS,	NULL,NULL},
	{	"exists"			,FILE_PROP_EXISTS		,FILE_PROP_FLAGS,	NULL,NULL},
	{	"date"				,FILE_PROP_DATE			,FILE_PROP_FLAGS,	NULL,NULL},
	{	"is_open"			,FILE_PROP_IS_OPEN		,FILE_PROP_FLAGS,	NULL,NULL},
	{	"eof"				,FILE_PROP_EOF			,FILE_PROP_FLAGS,	NULL,NULL},
	{	"error"				,FILE_PROP_ERROR		,FILE_PROP_FLAGS,	NULL,NULL},
	{	"descriptor"		,FILE_PROP_DESCRIPTOR	,FILE_PROP_FLAGS,	NULL,NULL},
	/* writeable */
	{	"etx"				,FILE_PROP_ETX			,JSPROP_ENUMERATE,  NULL,NULL},
	{	"debug"				,FILE_PROP_DEBUG		,JSPROP_ENUMERATE,	NULL,NULL},
	{	"position"			,FILE_PROP_POSITION		,JSPROP_ENUMERATE,	NULL,NULL},
	{	"length"			,FILE_PROP_LENGTH		,JSPROP_ENUMERATE,	NULL,NULL},
	{	"attributes"		,FILE_PROP_ATTRIBUTES	,JSPROP_ENUMERATE,	NULL,NULL},
	{0}
};

#ifdef _DEBUG
static char* file_prop_desc[] = {
	 "filename specified in constructor - <small>READ ONLY</small>"
	,"mode string specified in <i>open</i> call - <small>READ ONLY</small>"
	,"<i>true</i> if the file exists - <small>READ ONLY</small>"
	,"last modified date/time (time_t format) - <small>READ ONLY</small>"
	,"<i>true</i> if the file has been opened successfully - <small>READ ONLY</small>"
	,"<i>true</i> if the current file position is at the <b>end of file</b> - <small>READ ONLY</small>"
	,"the last occurred error value (use clear_error to clear) - <small>READ ONLY</small>"
	,"the open file descriptor (advanced use only) - <small>READ ONLY</small>"
	,"end-of-text character (advanced use only), if non-zero used by <i>read</i>, <i>readln</i>, and <i>write</i>"
	,"<i>true</i> if debug output is enabled"
	,"the current file position (offset in bytes), change value to seek within file"
	,"the current length of the file (in bytes)"
	,"file mode/attributes"
	,NULL
};
#endif


static jsMethodSpec js_file_functions[] = {
	{"open",			js_open,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("[string mode, boolean shareable, number buflen]")
	,JSDOCSTR("open file, <i>shareable</i> defaults to <i>false</i>, <i>buflen</i> defaults to 2048 bytes, "
		"mode (default: <tt>w+</tt>) specifies the type of access requested for the file, as follows:<br>"
		"<tt>r&nbsp</tt> open for reading; if the file does not exist or cannot be found, the open call fails<br>"
		"<tt>w&nbsp</tt> open an empty file for writing; if the given file exists, its contents are destroyed<br>"
		"<tt>a&nbsp</tt> open for writing at the end of the file (appending); creates the file first if it doesn’t exist<br>"
		"<tt>r+</tt> open for both reading and writing (the file must exist)<br>"
		"<tt>w+</tt> open an empty file for both reading and writing; if the given file exists, its contents are destroyed<br>"
		"<tt>a+</tt> open for reading and appending<br>"
		"<tt>b&nbsp</tt> open in binary (untranslated) mode; translations involving carriage-return and linefeed characters are suppressed (e.g. <tt>r+b</tt>)<br>"
		)
	},		
	{"close",			js_close,			0,	JSTYPE_VOID,	""
	,JSDOCSTR("close file")
	},		
	{"remove",			js_delete,			0,	JSTYPE_BOOLEAN, ""
	,JSDOCSTR("remove the file from the disk")
	},
	{"clearError",		js_clear_error,		0,	JSTYPE_ALIAS },
	{"clear_error",		js_clear_error,		0,	JSTYPE_BOOLEAN, ""
	,JSDOCSTR("clears the current error value (AKA clearError)")
	},
	{"flush",			js_flush,			0,	JSTYPE_BOOLEAN,	""
	,JSDOCSTR("flush/commit buffers to disk")
	},
	{"lock",			js_lock,			2,	JSTYPE_BOOLEAN,	JSDOCSTR("[offset, length]")
	,JSDOCSTR("lock file record for exclusive access (file must be opened <i>shareable</i>)")
	},		
	{"unlock",			js_unlock,			2,	JSTYPE_BOOLEAN,	JSDOCSTR("[offset, length]")
	,JSDOCSTR("unlock file record for exclusive access")
	},		
	{"read",			js_read,			0,	JSTYPE_STRING,	JSDOCSTR("[number maxlen]")
	,JSDOCSTR("read a string from file, maxlen defaults to 512 characters")
	},
	{"readln",			js_readln,			0,	JSTYPE_STRING,	JSDOCSTR("[number maxlen]")
	,JSDOCSTR("read a line-feed terminated string, maxlen defaults 512 characters")
	},		
	{"readBin",			js_readbin,			0,	JSTYPE_NUMBER,	JSDOCSTR("[number bytes]")
	,JSDOCSTR("read a binary integer from the file, default number of bytes is 4 (32-bits)")
	},
	{"readAll",			js_readall,			0,	JSTYPE_ARRAY,	""
	,JSDOCSTR("read all lines into an array of strings")
	},
	{"write",			js_write,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("string text [,number len]")
	,JSDOCSTR("write a string to the file")
	},
	{"writeln",			js_writeln,			0,	JSTYPE_BOOLEAN, JSDOCSTR("[string text]")
	,JSDOCSTR("write a line-feed terminated string to the file")
	},
	{"writeBin",		js_writebin,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("number value [,number bytes]")
	,JSDOCSTR("write a binary integer to the file, default number of bytes is 4 (32-bits)")
	},
	{"writeAll",		js_writeall,		0,	JSTYPE_BOOLEAN,	JSDOCSTR("array lines")
	,JSDOCSTR("write an array of strings to file")
	},		
	{"printf",			js_fprintf,			0,	JSTYPE_NUMBER,	JSDOCSTR("string format [,args]")
	,JSDOCSTR("write a formatted string to the file (ala fprintf)")
	},		
	{0}
};

/* File Destructor */

static void js_finalize_file(JSContext *cx, JSObject *obj)
{
	private_t* p;
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return;

	if(p->external==JS_FALSE && p->fp!=NULL)
		fclose(p->fp);

	dbprintf(FALSE, p, "closed: %s",p->name);

	free(p);

	JS_SetPrivate(cx, obj, NULL);
}

static JSClass js_file_class = {
     "File"					/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_file_get			/* getProperty	*/
	,js_file_set			/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_finalize_file		/* finalize		*/
};

/* File Constructor (creates file descriptor) */

static JSBool
js_file_constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSString*	str;
	private_t*	p;

	if((str = JS_ValueToString(cx, argv[0]))==NULL) {
		JS_ReportError(cx,"No filename specified");
		return(JS_FALSE);
	}

	*rval = JSVAL_VOID;

	if((p=(private_t*)calloc(1,sizeof(private_t)))==NULL) {
		JS_ReportError(cx,"calloc failed");
		return(JS_FALSE);
	}

	SAFECOPY(p->name,JS_GetStringBytes(str));

	if(!JS_SetPrivate(cx, obj, p)) {
		dbprintf(TRUE, p, "JS_SetPrivate failed");
		return(JS_FALSE);
	}

	if(!js_DefineMethods(cx, obj, js_file_functions)) {
		dbprintf(TRUE, p, "js_DefineMethods failed");
		return(JS_FALSE);
	}

#ifdef _DEBUG
	js_DescribeObject(cx,obj,"Class used for opening/creating files on the local file system");
	js_DescribeConstructor(cx,obj,"To create a new File object: <tt>var f = new File(filename)</tt>");
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", file_prop_desc, JSPROP_READONLY);
#endif

	dbprintf(FALSE, p, "object constructed");
	return(JS_TRUE);
}

JSObject* DLLCALL js_CreateFileClass(JSContext* cx, JSObject* parent)
{
	JSObject*	sockobj;

	sockobj = JS_InitClass(cx, parent, NULL
		,&js_file_class
		,js_file_constructor
		,1		/* number of constructor args */
		,js_file_properties
		,NULL	/* funcs, set in constructor */
		,NULL,NULL);

	return(sockobj);
}

JSObject* DLLCALL js_CreateFileObject(JSContext* cx, JSObject* parent, char *name, FILE* fp)
{
	JSObject* obj;
	private_t*	p;

	obj = JS_DefineObject(cx, parent, name, &js_file_class, NULL, JSPROP_ENUMERATE);

	if(obj==NULL)
		return(NULL);

	if(!JS_DefineProperties(cx, obj, js_file_properties))
		return(NULL);

	if (!js_DefineMethods(cx, obj, js_file_functions)) 
		return(NULL);

	if((p=(private_t*)calloc(1,sizeof(private_t)))==NULL)
		return(NULL);

	p->fp=fp;
	p->debug=JS_FALSE;
	p->external=JS_TRUE;

	if(!JS_SetPrivate(cx, obj, p)) {
		dbprintf(TRUE, p, "JS_SetPrivate failed");
		return(NULL);
	}

	dbprintf(FALSE, p, "object created");

	return(obj);
}


#endif	/* JAVSCRIPT */
