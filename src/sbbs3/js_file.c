/* js_file.c */

/* Synchronet JavaScript "File" Object */

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

#include "sbbs.h"
#include "md5.h"
#include "base64.h"
#include "uucode.h"
#include "yenc.h"
#include "ini_file.h"

#ifdef JAVASCRIPT

typedef struct
{
	FILE*	fp;
	char	name[MAX_PATH+1];
	char	mode[4];
	uchar	etx;
	BOOL	external;	/* externally created, don't close */
	BOOL	debug;
	BOOL	rot13;
	BOOL	yencoded;
	BOOL	uuencoded;
	BOOL	b64encoded;
	BOOL	network_byte_order;

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
	char*		uubuf;
	int32		len;
	int32		offset;
	int32		uulen;
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
	else {
		len=filelength(fileno(p->fp));
		offset=ftell(p->fp);
		if(offset>0)
			len-=offset;
	}
	if(len<0)
		len=512;

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

	if(p->rot13)
		rot13(buf);

	if(p->uuencoded || p->b64encoded || p->yencoded) {
		uulen=len*2;
		if((uubuf=malloc(uulen))==NULL)
			return(JS_TRUE);
		if(p->uuencoded)
			uulen=uuencode(uubuf,uulen,buf,len);
		else if(p->yencoded)
			uulen=yencode(uubuf,uulen,buf,len);
		else
			uulen=b64_encode(uubuf,uulen,buf,len);
		if(uulen>=0) {
			free(buf);
			buf=uubuf;
		} else
			free(uubuf);
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
		if(p->rot13)
			rot13(buf);
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
			if(fread(&w,1,size,p->fp)==size) {
				if(p->network_byte_order)
					w=ntohs(w);
				*rval = INT_TO_JSVAL(w);
			}
			break;
		case sizeof(DWORD):
			if(fread(&l,1,size,p->fp)==size) {
				if(p->network_byte_order)
					l=ntohl(l);
				JS_NewNumberValue(cx,l,rval);
			}
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
js_iniGetValue(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*	section;
	char*	key;
	char**	list;
	char	buf[MAX_VALUE_LEN];
	int32	i;
	jsval	val;
	jsval	dflt=argv[2];
	private_t*	p;
	JSObject*	array;

	*rval = JSVAL_VOID;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	section=JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
	key=JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
	switch(JSVAL_TAG(dflt)) {
		case JSVAL_STRING:
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,
				iniGetString(p->fp,section,key
					,JS_GetStringBytes(JS_ValueToString(cx,dflt)),buf)));
			break;
		case JSVAL_BOOLEAN:
			*rval = BOOLEAN_TO_JSVAL(
				iniGetBool(p->fp,section,key,JSVAL_TO_BOOLEAN(dflt)));
			break;
		case JSVAL_DOUBLE:
			JS_NewNumberValue(cx
				,iniGetFloat(p->fp,section,key,*JSVAL_TO_DOUBLE(dflt)),rval);
			break;
		case JSVAL_OBJECT:
		    array = JS_NewArrayObject(cx, 0, NULL);
			list=iniGetStringList(p->fp,section,key,",",JS_GetStringBytes(JS_ValueToString(cx,dflt)));
			for(i=0;list && list[i];i++) {
				val=STRING_TO_JSVAL(JS_NewStringCopyZ(cx,list[i]));
				if(!JS_SetElement(cx, array, i, &val))
					break;
			}
			iniFreeStringList(list);
			*rval = OBJECT_TO_JSVAL(array);
			break;
		default:
			if(JSVAL_IS_INT(dflt)) {
				*rval = INT_TO_JSVAL(
					iniGetInteger(p->fp,section,key,JSVAL_TO_INT(dflt)));
				break;
			}
			break;
	}

	return(JS_TRUE);
}

static JSBool
js_iniGetSections(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		prefix=NULL;
	char**		list;
    jsint       i;
    jsval       val;
    JSObject*	array;
	private_t*	p;

	*rval = JSVAL_NULL;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(argc)
		prefix=JS_GetStringBytes(JS_ValueToString(cx, argv[0]));

    array = JS_NewArrayObject(cx, 0, NULL);

	list = iniGetSectionList(p->fp,prefix);
    for(i=0;list && list[i];i++) {
		val=STRING_TO_JSVAL(JS_NewStringCopyZ(cx,list[i]));
        if(!JS_SetElement(cx, array, i, &val))
			break;
	}
	iniFreeStringList(list);

    *rval = OBJECT_TO_JSVAL(array);

    return(JS_TRUE);
}

static JSBool
js_iniGetKeys(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		section;
	char**		list;
    jsint       i;
    jsval       val;
    JSObject*	array;
	private_t*	p;

	*rval = JSVAL_NULL;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	section=JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
    array = JS_NewArrayObject(cx, 0, NULL);

	list = iniGetKeyList(p->fp,section);
    for(i=0;list && list[i];i++) {
		val=STRING_TO_JSVAL(JS_NewStringCopyZ(cx,list[i]));
        if(!JS_SetElement(cx, array, i, &val))
			break;
	}
	iniFreeStringList(list);

    *rval = OBJECT_TO_JSVAL(array);

    return(JS_TRUE);
}

static JSBool
js_iniGetObject(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		section;
    jsint       i;
    JSObject*	object;
	private_t*	p;
	named_string_t** list;

	*rval = JSVAL_NULL;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	section=JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
    object = JS_NewObject(cx, NULL, NULL, obj);

	list = iniGetNamedStringList(p->fp,section);
    for(i=0;list && list[i];i++) {
		JS_DefineProperty(cx, object, list[i]->name
			,STRING_TO_JSVAL(JS_NewStringCopyZ(cx,list[i]->value))
			,NULL,NULL,JSPROP_ENUMERATE);

	}
	iniFreeNamedStringList(list);

    *rval = OBJECT_TO_JSVAL(object);

    return(JS_TRUE);
}

static JSBool
js_iniGetAllObjects(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		name="name";
	char*		sec_name;
	char*		prefix=NULL;
	char**		sec_list;
    jsint       i,k;
    jsval       val;
    JSObject*	array;
    JSObject*	object;
	private_t*	p;
	named_string_t** key_list;

	*rval = JSVAL_NULL;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(argc)
		name=JS_GetStringBytes(JS_ValueToString(cx, argv[0]));

	if(argc>1)
		prefix=JS_GetStringBytes(JS_ValueToString(cx, argv[1]));

    array = JS_NewArrayObject(cx, 0, NULL);

	sec_list = iniGetSectionList(p->fp,prefix);
    for(i=0;sec_list && sec_list[i];i++) {
	    object = JS_NewObject(cx, NULL, NULL, obj);

		sec_name=sec_list[i];
		if(prefix!=NULL)
			sec_name+=strlen(prefix);
		JS_DefineProperty(cx, object, name
			,STRING_TO_JSVAL(JS_NewStringCopyZ(cx,sec_name))
			,NULL,NULL,JSPROP_ENUMERATE);

		key_list = iniGetNamedStringList(p->fp,sec_list[i]);
		for(k=0;key_list && key_list[k];k++)
			JS_DefineProperty(cx, object, key_list[k]->name
				,STRING_TO_JSVAL(JS_NewStringCopyZ(cx,key_list[k]->value))
				,NULL,NULL,JSPROP_ENUMERATE);
		iniFreeNamedStringList(key_list);

		val=OBJECT_TO_JSVAL(object);
        if(!JS_SetElement(cx, array, i, &val))
			break;
	}
	iniFreeStringList(sec_list);

    *rval = OBJECT_TO_JSVAL(array);

    return(JS_TRUE);
}

static JSBool
js_write(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		cp;
	char*		uubuf=NULL;
	int			len;	/* string length */
	int			tlen;	/* total length to write (may be greater than len) */
	private_t*	p;

	*rval = JSVAL_FALSE;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(p->fp==NULL)
		return(JS_TRUE);

	cp=JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
	len=strlen(cp);

	if((p->uuencoded || p->b64encoded || p->yencoded)
		&& len && (uubuf=malloc(len))!=NULL) {
		if(p->uuencoded)
			len=uudecode(uubuf,len,cp,len);
		else if(p->yencoded)
			len=ydecode(uubuf,len,cp,len);
		else
			len=b64_decode(uubuf,len,cp,len);
		if(len<0) {
			free(uubuf);
			return(JS_TRUE);
		}
		cp=uubuf;
	}

	if(p->rot13)
		rot13(cp);

	tlen=len;
	if(argc>1) {
		JS_ValueToInt32(cx,argv[1],(int32*)&tlen);
		if(len>tlen)
			len=tlen;
	}

	if(fwrite(cp,1,len,p->fp)==(size_t)len) {
		if(tlen>len) {
			len=tlen-len;
			if((cp=malloc(len))==NULL) {
				dbprintf(TRUE, p, "malloc failure of %u bytes", len);
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
		
	if(uubuf!=NULL)
		free(uubuf);

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

	if(p->rot13)
		rot13(cp);

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

	JS_ValueToInt32(cx,argv[0],&val);
	if(argc>1) 
		JS_ValueToInt32(cx,argv[1],(int32*)&size);

	switch(size) {
		case sizeof(BYTE):
			b = (BYTE)val;
			wr=fwrite(&b,1,size,p->fp);
			break;
		case sizeof(WORD):
			w = (WORD)val;
			if(p->network_byte_order)
				w=htons(w);
			wr=fwrite(&w,1,size,p->fp);
			break;
		case sizeof(DWORD):
			l = val;
			if(p->network_byte_order)
				l=htonl(l);
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
	,FILE_PROP_YENCODED
	,FILE_PROP_UUENCODED
	,FILE_PROP_B64ENCODED
	,FILE_PROP_ROT13
	,FILE_PROP_NETWORK_ORDER
	/* dynamically calculated */
	,FILE_PROP_CHKSUM
	,FILE_PROP_CRC16
	,FILE_PROP_CRC32
	,FILE_PROP_MD5_HEX
	,FILE_PROP_MD5_B64
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
			JS_ValueToBoolean(cx,*vp,&(p->debug));
			break;
		case FILE_PROP_YENCODED:
			JS_ValueToBoolean(cx,*vp,&(p->yencoded));
			break;
		case FILE_PROP_UUENCODED:
			JS_ValueToBoolean(cx,*vp,&(p->uuencoded));
			break;
		case FILE_PROP_B64ENCODED:
			JS_ValueToBoolean(cx,*vp,&(p->b64encoded));
			break;
		case FILE_PROP_ROT13:
			JS_ValueToBoolean(cx,*vp,&(p->rot13));
			break;
		case FILE_PROP_NETWORK_ORDER:
			JS_ValueToBoolean(cx,*vp,&(p->network_byte_order));
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
	char		str[128];
	BYTE*		buf;
	long		l;
	long		len;
	long		offset;
	ulong		sum;
	BYTE		digest[MD5_DIGEST_SIZE];
    jsint       tiny;
	JSString*	js_str=NULL;
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
			JS_NewNumberValue(cx,fdate(p->name),vp);
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
				*vp = INT_TO_JSVAL(errno);
			break;
		case FILE_PROP_POSITION:
			if(p->fp)
				JS_NewNumberValue(cx,ftell(p->fp),vp);
			else
				*vp = INT_TO_JSVAL(-1);
			break;
		case FILE_PROP_LENGTH:
			if(p->fp)	/* open? */
				JS_NewNumberValue(cx,filelength(fileno(p->fp)),vp);
			else
				JS_NewNumberValue(cx,flength(p->name),vp);
			break;
		case FILE_PROP_ATTRIBUTES:
			JS_NewNumberValue(cx,getfattr(p->name),vp);
			break;
		case FILE_PROP_DEBUG:
			*vp = BOOLEAN_TO_JSVAL(p->debug);
			break;
		case FILE_PROP_YENCODED:
			*vp = BOOLEAN_TO_JSVAL(p->yencoded);
			break;
		case FILE_PROP_UUENCODED:
			*vp = BOOLEAN_TO_JSVAL(p->uuencoded);
			break;
		case FILE_PROP_B64ENCODED:
			*vp = BOOLEAN_TO_JSVAL(p->b64encoded);
			break;
		case FILE_PROP_ROT13:
			*vp = BOOLEAN_TO_JSVAL(p->rot13);
			break;
		case FILE_PROP_NETWORK_ORDER:
			*vp = BOOLEAN_TO_JSVAL(p->network_byte_order);
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
		case FILE_PROP_CHKSUM:
		case FILE_PROP_CRC16:
		case FILE_PROP_CRC32:
			*vp = JSVAL_ZERO;
			if(p->fp==NULL)
				break;
			/* fall-through */
		case FILE_PROP_MD5_HEX:
		case FILE_PROP_MD5_B64:
			*vp = JSVAL_VOID;
			if(p->fp==NULL)
				break;
			offset=ftell(p->fp);			/* save current file position */
			fseek(p->fp,0,SEEK_SET);
			len=filelength(fileno(p->fp));
			if(len<1)
				break;
			if((buf=malloc(len*2))==NULL)
				break;
			len=fread(buf,sizeof(BYTE),len,p->fp);
			if(len>0) 
				switch(tiny) {
					case FILE_PROP_CHKSUM:
						for(sum=l=0;l<len;l++)
							sum+=buf[l];
						JS_NewNumberValue(cx,sum,vp);
						break;
					case FILE_PROP_CRC16:
						if(!JS_NewNumberValue(cx,crc16(buf,len),vp))
							*vp=JSVAL_ZERO;
						break;
					case FILE_PROP_CRC32:
						sum=crc32(buf,len);
						if(!JS_NewNumberValue(cx,sum,vp))
							*vp=JSVAL_ZERO;
						break;
					case FILE_PROP_MD5_HEX:
						MD5_calc(digest,buf,len);
						MD5_hex(str,digest);
						js_str=JS_NewStringCopyZ(cx, str);
						break;
					case FILE_PROP_MD5_B64:
						MD5_calc(digest,buf,len);
						b64_encode(str,sizeof(str)-1,digest,sizeof(digest));
						js_str=JS_NewStringCopyZ(cx, str);
						break;
				}
			free(buf);
			fseek(p->fp,offset,SEEK_SET);	/* restore saved file position */
			if(js_str!=NULL)
				*vp = STRING_TO_JSVAL(js_str);
			break;
	}

	return(JS_TRUE);
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
	{	"network_byte_order",FILE_PROP_NETWORK_ORDER,JSPROP_ENUMERATE,	NULL,NULL},
	{	"rot13"				,FILE_PROP_ROT13		,JSPROP_ENUMERATE,	NULL,NULL},
	{	"uue"				,FILE_PROP_UUENCODED	,JSPROP_ENUMERATE,	NULL,NULL},
	{	"yenc"				,FILE_PROP_YENCODED		,JSPROP_ENUMERATE,	NULL,NULL},
	{	"base64"			,FILE_PROP_B64ENCODED	,JSPROP_ENUMERATE,	NULL,NULL},
	/* dynamically calculated */
	{	"crc16"				,FILE_PROP_CRC16		,FILE_PROP_FLAGS,	NULL,NULL},
	{	"crc32"				,FILE_PROP_CRC32		,FILE_PROP_FLAGS,	NULL,NULL},
	{	"chksum"			,FILE_PROP_CHKSUM		,FILE_PROP_FLAGS,	NULL,NULL},
	{	"md5_hex"			,FILE_PROP_MD5_HEX		,FILE_PROP_FLAGS,	NULL,NULL},
	{	"md5_base64"		,FILE_PROP_MD5_B64		,FILE_PROP_FLAGS,	NULL,NULL},
	{0}
};

#ifdef _DEBUG
static char* file_prop_desc[] = {
	 "filename specified in constructor - <small>READ ONLY</small>"
	,"mode string specified in <i>open</i> call - <small>READ ONLY</small>"
	,"<i>true</i> if the file exists - <small>READ ONLY</small>"
	,"last modified date/time (time_t format) - <small>READ ONLY</small>"
	,"<i>true</i> if the file has been opened successfully - <small>READ ONLY</small>"
	,"<i>true</i> if the current file position is at the <i>end of file</i> - <small>READ ONLY</small>"
	,"the last occurred error value (use clear_error to clear) - <small>READ ONLY</small>"
	,"the open file descriptor (advanced use only) - <small>READ ONLY</small>"
	,"end-of-text character (advanced use only), if non-zero used by <i>read</i>, <i>readln</i>, and <i>write</i>"
	,"set to <i>true</i> to enable debug log output"
	,"the current file position (offset in bytes), change value to seek within file"
	,"the current length of the file (in bytes)"
	,"file mode/attributes"
	,"set to <i>true</i> if binary data is to be written and read in Network Byte Order (big end first)"
	,"set to <i>true</i> to enable automatic ROT13 translatation of text"
	,"set to <i>true</i> to enable automatic Unix-to-Unix encode and decode on <tt>read</tt> and <tt>write</tt> calls"
	,"set to <i>true</i> to enable automatic yEnc encode and decode on <tt>read</tt> and <tt>write</tt> calls"
	,"set to <i>true</i> to enable automatic Base64 encode and decode on <tt>read</tt> and <tt>write</tt> calls"
	,"calculated 16-bit CRC of file contents - <small>READ ONLY</small>"
	,"calculated 32-bit CRC of file contents - <small>READ ONLY</small>"
	,"calculated 32-bit checksum of file contents - <small>READ ONLY</small>"
	,"calculated 128-bit MD5 digest of file contents as hexadecimal string - <small>READ ONLY</small>"
	,"calculated 128-bit MD5 digest of file contents as base64-encoded string - <small>READ ONLY</small>"
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
	{"read",			js_read,			0,	JSTYPE_STRING,	JSDOCSTR("[maxlen]")
	,JSDOCSTR("read a string from file (optionally unix-to-unix or base64 decoding in the process), "
		"<i>maxlen</i> defaults to the current length of the file minus the current file position")
	},
	{"readln",			js_readln,			0,	JSTYPE_STRING,	JSDOCSTR("[maxlen]")
	,JSDOCSTR("read a line-feed terminated string, <i>maxlen</i> defaults to 512 characters")
	},		
	{"readBin",			js_readbin,			0,	JSTYPE_NUMBER,	JSDOCSTR("[bytes]")
	,JSDOCSTR("read a binary integer from the file, default number of <i>bytes</i> is 4 (32-bits)")
	},
	{"readAll",			js_readall,			0,	JSTYPE_ARRAY,	""
	,JSDOCSTR("read all lines into an array of strings")
	},
	{"write",			js_write,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("string text [,len]")
	,JSDOCSTR("write a string to the file (optionally unix-to-unix or base64 decoding in the process)")
	},
	{"writeln",			js_writeln,			0,	JSTYPE_BOOLEAN, JSDOCSTR("[string text]")
	,JSDOCSTR("write a line-feed terminated string to the file")
	},
	{"writeBin",		js_writebin,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("value [,bytes]")
	,JSDOCSTR("write a binary integer to the file, default number of <i>bytes</i> is 4 (32-bits)")
	},
	{"writeAll",		js_writeall,		0,	JSTYPE_BOOLEAN,	JSDOCSTR("array lines")
	,JSDOCSTR("write an array of strings to file")
	},		
	{"printf",			js_fprintf,			0,	JSTYPE_NUMBER,	JSDOCSTR("string format [,args]")
	,JSDOCSTR("write a formatted string to the file (ala fprintf) - "
		"<small>CAUTION: for experienced C programmers ONLY</small>")
	},
	{"iniGetSections",	js_iniGetSections,	0,	JSTYPE_ARRAY,	JSDOCSTR("[prefix]")
	,JSDOCSTR("parse all section names from a <tt>.ini</tt> file (format = '<tt>[section]</tt>') "
		"and return the section names as an <i>array of strings</i>, "
		"optionally, only those section names that begin with the specified <i>prefix</i>")
	},
	{"iniGetKeys",		js_iniGetKeys,		0,	JSTYPE_ARRAY,	JSDOCSTR("section")
	,JSDOCSTR("parse all key names from the specified <i>section</i> in a <tt>.ini</tt> file "
		"and return the key names as an <i>array of strings</i>")
	},
	{"iniGetValue",		js_iniGetValue,		3,	JSTYPE_STRING,	JSDOCSTR("section, key, default")
	,JSDOCSTR("parse a key from a <tt>.ini</tt> file and return its value (format = '<tt>key = value</tt>'). "
		"returns the specified <i>default</i> value if the key or value is missing or invalid. "
		"will return a <i>bool</i>, <i>number</i>, <i>string</i>, or an <i>array of strings</i> "
		"determined by the type of <i>default</i> value specified")
	},
	{"iniGetObject",	js_iniGetObject,	1,	JSTYPE_OBJECT,	JSDOCSTR("section")
	,JSDOCSTR("parse an entire section from a .ini file "
		"and return all of its keys and values as properties of an object")
	},
	{"iniGetAllObjects",js_iniGetAllObjects,1,	JSTYPE_ARRAY,	JSDOCSTR("[name_property] [,prefix]")
	,JSDOCSTR("parse all sections from a .ini file and return all sections and keys "
		"an array of objects with each section's keys as properties of each section object, "
		"<i>name_property</i> is the name of the property to create to contain the section's name "
		"(default is <tt>\"name\"</tt>), "
		"the optional <i>prefix</i> has the same use as in the <tt>iniGetSections</tt> method, "
		"if a <i>prefix</i> is specified, it is removed from each section's name" )
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

	if(!js_DefineMethods(cx, obj, js_file_functions, FALSE)) {
		dbprintf(TRUE, p, "js_DefineMethods failed");
		return(JS_FALSE);
	}

#ifdef _DEBUG
	js_DescribeObject(cx,obj,"Class used for opening, creating, reading, or writing files on the local file system<p>"
		"Special features include:</h2><ol type=disc>"
			"<li>Exclusive-access files (default) or shared files<ol type=circle>"
				"<li>optional record-locking"
				"<li>buffered or non-buffered I/O"
				"</ol>"
			"<li>Support for binary files<ol type=circle>"
				"<li>native or network byte order (endian)"
				"<li>automatic Unix-to-Unix (<i>UUE</i>), yEncode (<i>yEnc</i>) or Base64 encoding/decoding"
				"</ol>"
			"<li>Support for ASCII text files<ol type=circle>"
				"<li>supports line-based I/O<ol type=square>"
					"<li>entire file may be read or written as an array of strings"
					"<li>individual lines may be read or written one line at a time"
					"</ol>"
				"<li>supports fixed-length records<ol type=square>"
					"<li>optional end-of-text (<i>etx</i>) character for automatic record padding/termination"
					"<li>Synchronet <tt>.dat</tt> files use an <i>etx</i> value of 3 (Ctrl-C)"
					"</ol>"
				"<li>supports <tt>.ini</tt> formated configuration files"
				"<li>optional ROT13 encoding/translation"
				"</ol>"
			"<li>Dynamically-calculated industry standard checksums (e.g. CRC-16, CRC-32, MD5)"
			"</ol>"
			);
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

	obj = JS_DefineObject(cx, parent, name, &js_file_class, NULL
		,JSPROP_ENUMERATE|JSPROP_READONLY);

	if(obj==NULL)
		return(NULL);

	if(!JS_DefineProperties(cx, obj, js_file_properties))
		return(NULL);

	if (!js_DefineMethods(cx, obj, js_file_functions, FALSE)) 
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
