/* js_global.c */

/* Synchronet JavaScript "global" object properties/methods for all servers */

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

#ifdef JAVASCRIPT

/* Global Object Properites */
enum {
	 GLOB_PROP_ERRNO
	,GLOB_PROP_ERRNO_STR
};

static JSBool js_system_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint       tiny;
	JSString*	js_str;

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
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
    const char*	filename;
    JSScript*	script;
    jsval		result;
	scfg_t*		cfg;
	JSObject*	js_argv;
	JSBool		success;

	*rval=JSVAL_FALSE;

	if((cfg=(scfg_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if(argc>1) {

		if((js_argv=JS_NewArrayObject(cx, 0, NULL)) == NULL)
			return(JS_FALSE);

		JS_DefineProperty(cx, obj, "argv", OBJECT_TO_JSVAL(js_argv)
			,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

		for(i=1; i<argc; i++)
			JS_SetElement(cx, js_argv, i-1, &argv[i]);

		JS_DefineProperty(cx, obj, "argc", INT_TO_JSVAL(argc-1)
			,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);
	}

	if((filename=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL)
		return(JS_FALSE);

	errno = 0;
	if(strcspn(filename,"/\\")==strlen(filename)) {
		sprintf(path,"%s%s",cfg->mods_dir,filename);
		if(cfg->mods_dir[0]==0 || !fexistcase(path))
			sprintf(path,"%s%s",cfg->exec_dir,filename);
	} else
		strcpy(path,filename);

	JS_ClearPendingException(cx);

	if((script=JS_CompileFile(cx, obj, path))==NULL)
		return(JS_FALSE);

	success = JS_ExecuteScript(cx, obj, script, &result);

	JS_DestroyScript(cx, script);

	if(!success)
		return(JS_FALSE);

	*rval = result;

    return(JS_TRUE);
}

static JSBool
js_format(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char*		fmt;
    uintN		i;
    JSString *	str;
	va_list		arglist[64];

	if((fmt=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL)
		return(JS_FALSE);

	memset(arglist,0,sizeof(arglist));	/* Initialize arglist to NULLs */

    for (i = 1; i < argc && i<sizeof(arglist)/sizeof(arglist[0]); i++) {
		if(JSVAL_IS_STRING(argv[i]))
			arglist[i-1]=JS_GetStringBytes(JSVAL_TO_STRING(argv[i]));	/* exception here July-29-2002 */
		else if(JSVAL_IS_DOUBLE(argv[i]))
			arglist[i-1]=(char*)(unsigned long)*JSVAL_TO_DOUBLE(argv[i]);
		else if(JSVAL_IS_INT(argv[i]) || JSVAL_IS_BOOLEAN(argv[i]))
			arglist[i-1]=(char *)JSVAL_TO_INT(argv[i]);
		else
			arglist[i-1]=NULL;
	}
	
	if((p=JS_vsmprintf(fmt,(char*)arglist))==NULL)
		return(JS_FALSE);

	str = JS_NewStringCopyZ(cx, p);
	JS_smprintf_free(p);

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

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_mswait(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32 val=1;

	if(argc)
		JS_ValueToInt32(cx,argv[0],&val);
	mswait(val);

	*rval = JSVAL_VOID;
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
	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_exit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	*rval = JSVAL_VOID;
	if(argc)
		JS_DefineProperty(cx, obj, "exit_code", argv[0]
			,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	return(JS_FALSE);
}

static JSBool
js_crc16(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		str;

	if((str=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
		return(JS_FALSE);

	*rval = INT_TO_JSVAL(crc16(str,0));
	return(JS_TRUE);
}

static JSBool
js_crc32(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		str;

	if((str=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
		return(JS_FALSE);

	JS_NewNumberValue(cx,crc32(str,strlen(str)),rval);
	return(JS_TRUE);
}

static JSBool
js_chksum(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	ulong		sum=0;
	char*		p;

	if((p=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
		return(JS_FALSE);

	while(*p) sum+=*(p++);

	JS_NewNumberValue(cx,sum,rval);
	return(JS_TRUE);
}

static JSBool
js_ascii(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char		str[2];
	JSString*	js_str;

	if(JSVAL_IS_STRING(argv[0])) {	/* string to ascii-int */

		if((p=JS_GetStringBytes(JSVAL_TO_STRING(argv[0])))==NULL) 
			return(JS_FALSE);

		*rval=INT_TO_JSVAL(*p);
		return(JS_TRUE);
	}

	/* ascii-int to str */
	str[0]=(uchar)JSVAL_TO_INT(argv[0]);
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
	JSString*	js_str;

	if(JSVAL_IS_STRING(argv[0])) {	

		if((p=JS_GetStringBytes(JSVAL_TO_STRING(argv[0])))==NULL) 
			return(JS_FALSE);
		ch=*p;
	} else
		ch=(char)JSVAL_TO_INT(argv[0]);

	str[0]=toupper(ch)&~0x20;
	str[1]=0;

	if((js_str = JS_NewStringCopyZ(cx, str))==NULL)
		return(JS_FALSE);

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

	if((str=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
		return(JS_FALSE);

	if((p=dupestr(str))==NULL)
		return(JS_FALSE);

	ascii_str(p);

	js_str = JS_NewStringCopyZ(cx, p);
	free(p);
	if(js_str==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}


static JSBool
js_strip_ctrl(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char*		str;
	JSString*	js_str;

	if((str=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
		return(JS_FALSE);

	if((p=dupestr(str))==NULL)
		return(JS_FALSE);

	strip_ctrl(p);

	js_str = JS_NewStringCopyZ(cx, p);
	free(p);
	if(js_str==NULL)
		return(JS_FALSE);

	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_strip_exascii(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char*		str;
	JSString*	js_str;

	if((str=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
		return(JS_FALSE);

	if((p=dupestr(str))==NULL)
		return(JS_FALSE);

	strip_exascii(p);

	js_str = JS_NewStringCopyZ(cx, p);
	free(p);
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
	JSString*	js_str;

	if((inbuf=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
		return(JS_FALSE);

	if((outbuf=(char*)malloc((strlen(inbuf)*2)+1))==NULL)
		return(JS_FALSE);

	for(i=j=0;inbuf[i];i++) {
		if(inbuf[i]=='\n' && (!i || inbuf[i-1]!='\r'))
			outbuf[j++]='\r';
		outbuf[j++]=inbuf[i];
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
js_word_wrap(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		l,len=79;
	ulong		i,k;
	int			col=1;
	uchar*		inbuf;
	char*		outbuf;
	char*		linebuf;
	JSString*	js_str;

	if((inbuf=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
		return(JS_FALSE);

	if((outbuf=(char*)malloc((strlen(inbuf)*2)+1))==NULL)
		return(JS_FALSE);

	if(argc>1)
		JS_ValueToInt32(cx,argv[1],&len);

	if((linebuf=(char*)malloc((len*2)+2))==NULL) /* room for ^A codes */
		return(JS_FALSE);

	outbuf[0]=0;
	for(i=l=0;inbuf[i];) {
		if(inbuf[i]=='\r' || inbuf[i]==FF) {
			strncat(outbuf,linebuf,l);
			l=0;
			col=1;
		}
		else if(inbuf[i]=='\t') {
			if((col%8)==0)
				col++;
			col+=(col%8);
		} else if(inbuf[i]==CTRL_A && inbuf[i+1]!=0) {
			if(l<(len*2)) {
				strncpy(linebuf+l,inbuf+i,2);
				l+=2;
			}
			i+=2;
			continue;
		} else if(inbuf[i]>=' ')
			col++;
		linebuf[l]=inbuf[i++];
		if(col<=len) {
			l++;
			continue;
		}
		/* wrap line here */
		k=l;
		while(k && linebuf[k]>' ' && linebuf[k-1]!=CTRL_A) k--;
		if(k==0)	/* continuous printing chars, no word wrap possible */
			strncat(outbuf,linebuf,l+1);
		else {
			i-=(l-k);	/* rewind to start of next word */
			linebuf[k]=0;
			truncsp(linebuf);
			strcat(outbuf,linebuf);
		}
		strcat(outbuf,"\r\n");
		/* skip white space (but no more than one LF) for starting of new line */
		while(inbuf[i] && inbuf[i]<=' ' && inbuf[i]!='\n' && inbuf[i]!=CTRL_A) i++;	
		if(inbuf[i]=='\n') i++;
		l=0;
		col=1;
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
js_quote_msg(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		len=79;
	int			i,l;
	uchar*		inbuf;
	char*		outbuf;
	char*		linebuf;
	char*		prefix=" > ";
	JSString*	js_str;

	if((inbuf=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
		return(JS_FALSE);

	if(argc>1)
		JS_ValueToInt32(cx,argv[1],&len);

	if(argc>2)
		prefix=JS_GetStringBytes(JS_ValueToString(cx, argv[2]));

	if((outbuf=(char*)malloc((strlen(inbuf)*strlen(prefix))+1))==NULL)
		return(JS_FALSE);

	if((linebuf=(char*)malloc(len+1))==NULL)
		return(JS_FALSE);

	outbuf[0]=0;
	for(i=l=0;inbuf[i];i++) {
		if(l==0)
			strcat(outbuf,prefix);
		if(l<len)
			linebuf[l++]=inbuf[i];
		if(inbuf[i]=='\n') {
			strncat(outbuf,linebuf,l);
			l=0;
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
js_rot13(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char*		str;
	JSString*	js_str;

	if((str=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
		return(JS_FALSE);

	if((p=dupestr(str))==NULL)
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
	{ 131	,NULL		}, /* 159 Florin (non-standard) */
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
	{ 248	,"oslash"	}, /* 237 o, slash (also #966?) */
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

static JSBool
js_html_encode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int			ch;
	ulong		i,j;
	uchar*		inbuf;
	uchar*		outbuf;
	JSBool		exascii=JS_TRUE;
	JSBool		wsp=JS_TRUE;
	JSString*	js_str;

	if((inbuf=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
		return(JS_FALSE);

	if(argc>1 && JSVAL_IS_BOOLEAN(argv[1]))
		exascii=JSVAL_TO_BOOLEAN(argv[1]);

	if(argc>2 && JSVAL_IS_BOOLEAN(argv[2]))
		wsp=JSVAL_TO_BOOLEAN(argv[2]);

	if((outbuf=(char*)malloc((strlen(inbuf)*10)+1))==NULL)
		return(JS_FALSE);

	for(i=j=0;inbuf[i];i++) {
		switch(inbuf[i]) {
			case TAB:
			case LF:
			case CR:
				if(wsp)
					j+=sprintf(outbuf+j,"&#%u;",inbuf[i]);
				else
					outbuf[j++]=inbuf[i];
				break;
			case '"':
				j+=sprintf(outbuf+j,"&quot;");
				break;
			case '&':
				j+=sprintf(outbuf+j,"&amp;");
				break;
			case '<':
				j+=sprintf(outbuf+j,"&lt;");
				break;
			case '>':
				j+=sprintf(outbuf+j,"&gt;");
				break;
			case CTRL_O:	/* General currency symbol */
				j+=sprintf(outbuf+j,"&curren;");
				break;
			case CTRL_T:	/* Paragraph sign */
				j+=sprintf(outbuf+j,"&para;");
				break;
			case CTRL_U:	/* Section sign */
				j+=sprintf(outbuf+j,"&sect;");
				break;
			default:
				if(inbuf[i]&0x80) {
					if(exascii) {
						ch=inbuf[i]^0x80;
						if(exasctbl[ch].name!=NULL)
							j+=sprintf(outbuf+j,"&%s;",exasctbl[ch].name);
						else
							j+=sprintf(outbuf+j,"&#%u;",exasctbl[ch].value);
					} else
						outbuf[j++]=inbuf[i];
				}
				else if(inbuf[i]>=' ' && inbuf[i]<DEL)
					outbuf[j++]=inbuf[i];
				else if(inbuf[i]>' ') /* strip unknown control chars */
					j+=sprintf(outbuf+j,"&#%u;",inbuf[i]);
				break;
		}
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

	if((inbuf=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
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
	uchar*		inbuf;
	uchar*		outbuf;
	JSString*	js_str;

	*rval = JSVAL_NULL;

	if((inbuf=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
		return(JS_FALSE);

	len=(strlen(inbuf)*10)+1;
	if((outbuf=(char*)malloc(len))==NULL)
		return(JS_FALSE);

	res=b64_encode(outbuf,len,inbuf,strlen(inbuf));

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

	if((inbuf=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
		return(JS_FALSE);

	len=strlen(inbuf)+1;
	if((outbuf=(char*)malloc(len))==NULL)
		return(JS_FALSE);

	res=b64_decode(outbuf,len,inbuf,strlen(inbuf));

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
js_md5_calc(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	BYTE		digest[MD5_DIGEST_SIZE];
	JSBool		hex=JS_FALSE;
	char*		inbuf;
	char		outbuf[64];
	JSString*	js_str;

	*rval = JSVAL_NULL;

	if((inbuf=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
		return(JS_FALSE);

	if(argc>1 && JSVAL_IS_BOOLEAN(argv[1]))
		hex=JSVAL_TO_BOOLEAN(argv[1]);

	MD5_calc(digest,inbuf,strlen(inbuf));

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
js_truncsp(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char*		str;
	JSString*	js_str;

	if((str=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
		return(JS_FALSE);

	if((p=dupestr(str))==NULL)
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

	if((str=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
		return(JS_FALSE);

	if((set=JS_GetStringBytes(JS_ValueToString(cx, argv[1])))==NULL) 
		return(JS_FALSE);


	if((p=dupestr(str))==NULL)
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
js_getfname(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		str;
	JSString*	js_str;

	if((str=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
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

	if((str=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
		return(JS_FALSE);

	*rval = JSVAL_VOID;

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

	if((str=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
		return(JS_FALSE);

	*rval = JSVAL_VOID;

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
js_fexist(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;

	if((p=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) {
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

	if((p=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	*rval = BOOLEAN_TO_JSVAL(remove(p)==0);
	return(JS_TRUE);
}

static JSBool
js_rename(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		oldname;
	char*		newname;

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	if((oldname=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL)
		return(JS_TRUE);
	if((newname=JS_GetStringBytes(JS_ValueToString(cx, argv[1])))==NULL)
		return(JS_TRUE);

	*rval = BOOLEAN_TO_JSVAL(rename(oldname,newname)==0);
	return(JS_TRUE);
}

static JSBool
js_fcopy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		src;
	char*		dest;

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	if((src=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL)
		return(JS_TRUE);
	if((dest=JS_GetStringBytes(JS_ValueToString(cx, argv[1])))==NULL)
		return(JS_TRUE);

	*rval = BOOLEAN_TO_JSVAL(fcopy(src,dest));
	return(JS_TRUE);
}

static JSBool
js_backup(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		fname;
	int32		level=5;
	BOOL		ren=FALSE;

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	if((fname=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL)
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

	if((p=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) {
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

	if((p=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) {
		*rval = INT_TO_JSVAL(-1);
		return(JS_TRUE);
	}

	JS_NewNumberValue(cx,getfattr(p),rval);
	return(JS_TRUE);
}

static JSBool
js_fdate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;

	if((p=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) {
		*rval = INT_TO_JSVAL(-1);
		return(JS_TRUE);
	}

	JS_NewNumberValue(cx,fdate(p),rval);
	return(JS_TRUE);
}

static JSBool
js_utime(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*			fname;
	int32			actime;
	int32			modtime;
	struct utimbuf	tbuf;
	struct utimbuf*	t=NULL;

	*rval = JSVAL_FALSE;

	if((fname=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL)
		return(JS_TRUE);

	if(argc>1) {
		memset(&tbuf,0,sizeof(tbuf));
		actime=modtime=time(NULL);
		JS_ValueToInt32(cx,argv[1],&actime);
		JS_ValueToInt32(cx,argv[2],&modtime);
		tbuf.actime=actime;
		tbuf.modtime=modtime;
		t=&tbuf;
	}

	*rval = BOOLEAN_TO_JSVAL(utime(fname,t)==0);

	return(JS_TRUE);
}


static JSBool
js_flength(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;

	if((p=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) {
		*rval = INT_TO_JSVAL(-1);
		return(JS_TRUE);
	}

	JS_NewNumberValue(cx,flength(p),rval);
	return(JS_TRUE);
}


static JSBool
js_touch(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;

	if((p=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	*rval = BOOLEAN_TO_JSVAL(ftouch(p));
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

	if((p=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) {
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

	if((p=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) 
		return(JS_TRUE);

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
js_mkdir(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;

	if((p=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) {
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

	if((p=JS_GetStringBytes(JS_ValueToString(cx, argv[0])))==NULL) {
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
	time_t		t=time(NULL);
	struct tm	tm;
	JSString*	js_str;

	fmt=JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
	if(argc>1)
		JS_ValueToInt32(cx,argv[1],(int32*)&t);

	strcpy(str,"-Invalid time-");
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

	*rval = JSVAL_NULL;

	if(argv[0]==JSVAL_VOID)
		return(JS_TRUE);

	if((addr.s_addr=resolve_ip(JS_GetStringBytes(JS_ValueToString(cx, argv[0]))))
		==INADDR_NONE)
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

	*rval = JSVAL_NULL;

	if(argv[0]==JSVAL_VOID)
		return(JS_TRUE);

	addr.s_addr=inet_addr(JS_GetStringBytes(JS_ValueToString(cx, argv[0])));
	h=gethostbyaddr((char *)&addr,sizeof(addr),AF_INET);

	if(h!=NULL && h->h_name!=NULL)
		*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,h->h_name));

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

static jsMethodSpec js_global_functions[] = {
	{"exit",			js_exit,			0,	JSTYPE_VOID,	"[number exit_code]"
	,JSDOCSTR("stop script execution, "
		"optionally setting the global property <tt>exit_code</tt> to the specified numeric value")
	},		
	{"load",            js_load,            1,	JSTYPE_BOOLEAN,	JSDOCSTR("string filename [,args]")
	,JSDOCSTR("load and execute a JavaScript file, returns <i>true</i> if the execution was successful")
	},		
	{"sleep",			js_mswait,			0,	JSTYPE_ALIAS },
	{"mswait",			js_mswait,			0,	JSTYPE_VOID,	JSDOCSTR("[number milliseconds]")
	,JSDOCSTR("millisecond wait/sleep routine (AKA sleep)")
	},
	{"yield",			js_yield,			0,	JSTYPE_VOID,	JSDOCSTR("[bool forced]")
	,JSDOCSTR("release current thread time-slice, "
		"a <i>forced</i> yield will yield to all other pending tasks (lowering CPU utilization), "
		"a non-<i>forced</i> yield will yield only to pending tasks of equal or higher priority. "
		"<i>forced</i> defaults to <i>true</i>")
	},
	{"random",			js_random,			1,	JSTYPE_NUMBER,	JSDOCSTR("number max")
	,JSDOCSTR("return random integer between 0 and max-1")
	},		
	{"time",			js_time,			0,	JSTYPE_NUMBER,	""
	,JSDOCSTR("return current time in Unix (time_t) format (number of seconds since Jan-01-1970)")
	},		
	{"beep",			js_beep,			0,	JSTYPE_VOID,	JSDOCSTR("[number freq, duration]")
	,JSDOCSTR("produce a tone on the local speaker at specified frequency for specified duration (in milliseconds)")
	},		
	{"sound",			js_sound,			0,	JSTYPE_BOOLEAN,	JSDOCSTR("[string filename]")
	,JSDOCSTR("play a waveform (.wav) sound file")
	},		
	{"ctrl",			js_ctrl,			1,	JSTYPE_STRING,	JSDOCSTR("number or string")
	,JSDOCSTR("return ASCII control character representing character passed - Example: <tt>ctrl('C') returns '\3'</tt>")
	},
	{"ascii",			js_ascii,			1,	JSTYPE_NUMBER,	JSDOCSTR("[string text] or [number value]")
	,JSDOCSTR("convert string to ASCII value or vice-versa (returns number OR string)")
	},		
	{"ascii_str",		js_ascii_str,		1,	JSTYPE_STRING,	JSDOCSTR("string text")
	,JSDOCSTR("convert extended-ASCII in string to plain ASCII")
	},		
	{"strip_ctrl",		js_strip_ctrl,		1,	JSTYPE_STRING,	JSDOCSTR("string text")
	,JSDOCSTR("strip control characters from string")
	},		
	{"strip_exascii",	js_strip_exascii,	1,	JSTYPE_STRING,	JSDOCSTR("string text")
	,JSDOCSTR("strip extended-ASCII characters from string")
	},		
	{"truncsp",			js_truncsp,			1,	JSTYPE_STRING,	JSDOCSTR("string text")
	,JSDOCSTR("truncate white-space characters off end of string")
	},		
	{"truncstr",		js_truncstr,		2,	JSTYPE_STRING,	JSDOCSTR("string text, charset")
	,JSDOCSTR("truncate string at first char in <i>charset</i>")
	},		
	{"lfexpand",		js_lfexpand,		1,	JSTYPE_STRING,	JSDOCSTR("string text")
	,JSDOCSTR("expand line-feeds (LF) to carriage-return/line-feeds (CRLF)")
	},		
	{"file_getname",	js_getfname,		1,	JSTYPE_STRING,	JSDOCSTR("string path")
	,JSDOCSTR("returns filename portion of passed path string")
	},
	{"file_getext",		js_getfext,			1,	JSTYPE_STRING,	JSDOCSTR("string path")
	,JSDOCSTR("returns file extension portion of passed path/filename string (including '.') "
		"or <i>undefined</i> if no extension is found")
	},
	{"file_getcase",	js_getfcase,		1,	JSTYPE_STRING,	JSDOCSTR("string filename")
	,JSDOCSTR("returns correct case of filename (long version of filename on Win32) "
		"or <i>undefined</i> if the file doesn't exist")
	},
	{"file_exists",		js_fexist,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("string filename")
	,JSDOCSTR("verify a file's existence")
	},		
	{"file_remove",		js_remove,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("string filename")
	,JSDOCSTR("delete a file")
	},		
	{"file_rename",		js_rename,			2,	JSTYPE_BOOLEAN,	JSDOCSTR("oldname, newname")
	,JSDOCSTR("rename a file, possibly moving it to another directory in the process")
	},
	{"file_copy",		js_fcopy,			2,	JSTYPE_BOOLEAN,	JSDOCSTR("source, destination")
	,JSDOCSTR("copy a file from one directory or filename to another")
	},
	{"file_backup",		js_backup,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("string filename [,number level] [,bool rename]")
	,JSDOCSTR("backup the specified <i>filename</i> as <tt>filename.<i>number</i>.extension</tt> "
		"where <i>number</i> is the backup number 0 through <i>level</i>-1 "
		"(default backup <i>level</i> is 5), "
		"if <i>rename</i> is <i>true</i>, the original file is renamed instead of copied "
		"(default is <i>false</i>)")
	},
	{"file_isdir",		js_isdir,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("string filename")
	,JSDOCSTR("check if specified <i>filename</i> is a directory")
	},		
	{"file_attrib",		js_fattr,			1,	JSTYPE_NUMBER,	JSDOCSTR("string filename")
	,JSDOCSTR("get a file's permissions/attributes")
	},		
	{"file_date",		js_fdate,			1,	JSTYPE_NUMBER,	JSDOCSTR("string filename")
	,JSDOCSTR("get a file's last modified date/time (in time_t format)")
	},
	{"file_size",		js_flength,			1,	JSTYPE_NUMBER,	JSDOCSTR("string filename")
	,JSDOCSTR("get a file's length (in bytes)")
	},
	{"file_utime",		js_utime,			3,	JSTYPE_BOOLEAN,	JSDOCSTR("string filename [,access_time] [,mod_time]")
	,JSDOCSTR("change a file's last accessed and modification date/time (in time_t format), "
		"or change to current time")
	},
	{"file_touch",		js_touch,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("string filename")
	,JSDOCSTR("updates a file's last modification date/time to current time, "
		"creating an empty file if it doesn't already exist")
	},
	{"directory",		js_directory,		1,	JSTYPE_ARRAY,	JSDOCSTR("string pattern [,flags]")
	,JSDOCSTR("returns an array of directory entries, "
		"<i>pattern</i> is the path and filename or wildcards to search for (e.g. '/subdir/*.txt'), "
		"<i>flags</i> is a bitfield of optional <tt>glob</tt> flags (default is <tt>GLOB_MARK</tt>)")
	},		
	{"mkdir",			js_mkdir,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("string directory")
	,JSDOCSTR("make a directory")
	},		
	{"rmdir",			js_rmdir,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("string directory")
	,JSDOCSTR("remove a directory")
	},		
	{"strftime",		js_strftime,		1,	JSTYPE_STRING,	JSDOCSTR("string format [,number time]")
	,JSDOCSTR("return a formatted time string (ala C strftime)")
	},		
	{"format",			js_format,			1,	JSTYPE_STRING,	JSDOCSTR("string format [,args]")
	,JSDOCSTR("return a formatted string (ala sprintf) - "
		"<small>CAUTION: for experienced C programmers ONLY</small>")
	},
	{"html_encode",		js_html_encode,		1,	JSTYPE_STRING,	JSDOCSTR("string text [,bool ex_ascii] [,bool white_space]")
	,JSDOCSTR("return an HTML-encoded text string (using standard HTML character entities), "
		"escaping IBM extended-ASCII and white-space characters by default")
	},
	{"html_decode",		js_html_decode,		1,	JSTYPE_STRING,	JSDOCSTR("string text")
	,JSDOCSTR("return a decoded HTML-encoded text string")
	},
	{"word_wrap",		js_word_wrap,		1,	JSTYPE_STRING,	JSDOCSTR("string text [,line_length]")
	,JSDOCSTR("returns a word-wrapped version of the text string argument, <i>line_length</i> defaults to <i>79</i>")
	},
	{"quote_msg",		js_quote_msg,		1,	JSTYPE_STRING,	JSDOCSTR("string text [,line_length] [,prefix]")
	,JSDOCSTR("returns a quoted version of the message text string argumnet, <i>line_length</i> defaults to <i>79</i>, "
		"<i>prefix</i> defaults to <tt>\" > \"</tt>")
	},
	{"rot13_translate",	js_rot13,			1,	JSTYPE_STRING,	JSDOCSTR("string text")
	,JSDOCSTR("returns ROT13-translated version of text string (will encode or decode text)")
	},
	{"base64_encode",	js_b64_encode,		1,	JSTYPE_STRING,	JSDOCSTR("string text")
	,JSDOCSTR("returns base64-encoded version of text string or <i>null</i> on error")
	},
	{"base64_decode",	js_b64_decode,		1,	JSTYPE_STRING,	JSDOCSTR("string text")
	,JSDOCSTR("returns base64-decoded text string or <i>null</i> on error (not useful for binary data)")
	},
	{"crc16_calc",		js_crc16,			1,	JSTYPE_NUMBER,	JSDOCSTR("string text")
	,JSDOCSTR("calculate and return 16-bit CRC of text string")
	},		
	{"crc32_calc",		js_crc32,			1,	JSTYPE_NUMBER,	JSDOCSTR("string text")
	,JSDOCSTR("calculate and return 32-bit CRC of text string")
	},		
	{"chksum_calc",		js_chksum,			1,	JSTYPE_NUMBER,	JSDOCSTR("string text")
	,JSDOCSTR("calculate and return 32-bit checksum of text string")
	},
	{"md5_calc",		js_md5_calc,		1,	JSTYPE_STRING,	JSDOCSTR("string text [,bool hex]")
	,JSDOCSTR("calculate and return 128-bit MD5 digest of text string, result encoded in base64 (default) or hexadecimal")
	},
	{"gethostbyname",	js_resolve_ip,		1,	JSTYPE_ALIAS },
	{"resolve_ip",		js_resolve_ip,		1,	JSTYPE_STRING,	JSDOCSTR("string hostname")
	,JSDOCSTR("resolve IP address of specified hostname (AKA gethostbyname)")
	},
	{"gethostbyaddr",	js_resolve_host,	1,	JSTYPE_ALIAS },
	{"resolve_host",	js_resolve_host,	1,	JSTYPE_STRING,	JSDOCSTR("string ip_address")
	,JSDOCSTR("resolve hostname of specified IP address (AKA gethostbyaddr)")
	},
	{0}
};

JSObject* DLLCALL js_CreateGlobalObject(JSContext* cx, scfg_t* cfg, jsMethodSpec* methods)
{
	JSObject*	glob;

	if((glob = JS_NewObject(cx, &js_global_class, NULL, NULL)) ==NULL)
		return(NULL);

	if (!JS_InitStandardClasses(cx, glob))
		return(NULL);

	if(methods!=NULL && !js_DefineMethods(cx, glob, methods, TRUE)) 
		return(NULL);

	if (!js_DefineMethods(cx, glob, js_global_functions, TRUE)) 
		return(NULL);

	if(!JS_DefineProperties(cx, glob, js_global_properties))
		return(NULL);

	if(!JS_SetPrivate(cx, glob, cfg))	/* Store a pointer to scfg_t */
		return(NULL);

#ifdef _DEBUG
	js_DescribeObject(cx,glob,"Top-level functions and properties (common to all servers and services)");
#endif

	return(glob);
}

#endif	/* JAVSCRIPT */
