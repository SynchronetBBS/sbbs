/* js_user.c */

/* Synchronet JavaScript "User" Object */

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
	user_t* user;
	scfg_t*	cfg;

} private_t;

/* User Object Properites */
enum {
	 USER_PROP_ALIAS 	
	,USER_PROP_NAME		
	,USER_PROP_HANDLE	
	,USER_PROP_NOTE		
	,USER_PROP_COMP		
	,USER_PROP_COMMENT	
	,USER_PROP_NETMAIL	
	,USER_PROP_ADDRESS	
	,USER_PROP_LOCATION	
	,USER_PROP_ZIPCODE	
	,USER_PROP_PASS		
	,USER_PROP_PHONE  	
	,USER_PROP_BIRTH  	
	,USER_PROP_MODEM     
	,USER_PROP_LASTON	
	,USER_PROP_FIRSTON	
	,USER_PROP_EXPIRE    
	,USER_PROP_PWMOD     
	,USER_PROP_LOGONS    
	,USER_PROP_LTODAY    
	,USER_PROP_TIMEON    
	,USER_PROP_TEXTRA  	
	,USER_PROP_TTODAY    
	,USER_PROP_TLAST     
	,USER_PROP_POSTS     
	,USER_PROP_EMAILS    
	,USER_PROP_FBACKS    
	,USER_PROP_ETODAY	
	,USER_PROP_PTODAY	
	,USER_PROP_ULB       
	,USER_PROP_ULS       
	,USER_PROP_DLB       
	,USER_PROP_DLS       
	,USER_PROP_CDT		
	,USER_PROP_MIN		
	,USER_PROP_LEVEL 	
	,USER_PROP_FLAGS1	
	,USER_PROP_FLAGS2	
	,USER_PROP_FLAGS3	
	,USER_PROP_FLAGS4	
	,USER_PROP_EXEMPT	
	,USER_PROP_REST		
	,USER_PROP_ROWS		
	,USER_PROP_SEX		
	,USER_PROP_MISC		
	,USER_PROP_LEECH 	
	,USER_PROP_CURSUB	
	,USER_PROP_CURDIR	
	,USER_PROP_FREECDT	
	,USER_PROP_XEDIT 	
	,USER_PROP_SHELL 	
	,USER_PROP_QWK		
	,USER_PROP_TMPEXT	
	,USER_PROP_CHAT		
	,USER_PROP_NS_TIME	
	,USER_PROP_PROT		
};

static JSBool js_user_set(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint       tiny;
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_FALSE;

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case USER_PROP_MISC:
			JS_ValueToInt32(cx, *vp, &p->user->misc);
			break;
	}

	return(TRUE);
}

static JSBool js_user_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	char*		s=NULL;
	char		tmp[128];
	ulong		val=0;
    jsint       tiny;
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_FALSE;

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
	case USER_PROP_ALIAS: 
		s=p->user->alias;
		break;
	case USER_PROP_NAME:
		s=p->user->name;
		break;
	case USER_PROP_HANDLE:
		s=p->user->handle;
		break;
	case USER_PROP_NOTE:
		s=p->user->note;
		break;
	case USER_PROP_COMP:
		s=p->user->comp;
		break;
	case USER_PROP_COMMENT:
		s=p->user->comment;
		break;
	case USER_PROP_NETMAIL:
		s=p->user->netmail;
		break;
	case USER_PROP_ADDRESS:
		s=p->user->address;
		break;
	case USER_PROP_LOCATION:
		s=p->user->location;
		break;
	case USER_PROP_ZIPCODE:
		s=p->user->zipcode;
		break;
	case USER_PROP_PASS:
		s=p->user->pass;
		break;
	case USER_PROP_PHONE:
		s=p->user->phone;
		break;
	case USER_PROP_BIRTH:
		s=p->user->birth;
		break;
	case USER_PROP_MODEM:
		s=p->user->modem;
		break;
	case USER_PROP_LASTON:
		val=p->user->laston;
		break;
	case USER_PROP_FIRSTON:
		val=p->user->firston;
		break;
	case USER_PROP_EXPIRE:
		val=p->user->expire;
		break;
	case USER_PROP_PWMOD: 
		val=p->user->pwmod;
		break;
	case USER_PROP_LOGONS:
		val=p->user->logons;
		break;
	case USER_PROP_LTODAY:
		val=p->user->ltoday;
		break;
	case USER_PROP_TIMEON:
		val=p->user->timeon;
		break;
	case USER_PROP_TEXTRA:
		val=p->user->textra;
		break;
	case USER_PROP_TTODAY:
		val=p->user->ttoday;
		break;
	case USER_PROP_TLAST: 
		val=p->user->tlast;
		break;
	case USER_PROP_POSTS: 
		val=p->user->posts;
		break;
	case USER_PROP_EMAILS: 
		val=p->user->emails;
		break;
	case USER_PROP_FBACKS: 
		val=p->user->fbacks;
		break;
	case USER_PROP_ETODAY:	
		val=p->user->etoday;
		break;
	case USER_PROP_PTODAY:
		val=p->user->ptoday;
		break;
	case USER_PROP_ULB:
		val=p->user->ulb;
		break;
	case USER_PROP_ULS:
		val=p->user->uls;
		break;
	case USER_PROP_DLB:
		val=p->user->dlb;
		break;
	case USER_PROP_DLS:
		val=p->user->dls;
		break;
	case USER_PROP_CDT:
		val=p->user->cdt;
		break;
	case USER_PROP_MIN:
		val=p->user->min;
		break;
	case USER_PROP_LEVEL:
		val=p->user->level;
		break;
	case USER_PROP_FLAGS1:
		val=p->user->flags1;
		break;
	case USER_PROP_FLAGS2:
		val=p->user->flags2;
		break;
	case USER_PROP_FLAGS3:
		val=p->user->flags3;
		break;
	case USER_PROP_FLAGS4:
		val=p->user->flags4;
		break;
	case USER_PROP_EXEMPT:
		val=p->user->exempt;
		break;
	case USER_PROP_REST:
		val=p->user->rest;
		break;
	case USER_PROP_ROWS:
		val=p->user->rows;
		break;
	case USER_PROP_SEX:
		sprintf(tmp,"%c",p->user->sex);
		s=tmp;
		break;
	case USER_PROP_MISC:
		val=p->user->misc;
		break;
	case USER_PROP_LEECH:
		val=p->user->leech;
		break;
	case USER_PROP_CURSUB:
		s=p->user->cursub;
		break;
	case USER_PROP_CURDIR:
		s=p->user->curdir;
		break;
	case USER_PROP_FREECDT:
		val=p->user->freecdt;
		break;
	case USER_PROP_XEDIT:
		val=p->user->xedit;
		break;
	case USER_PROP_SHELL:
		val=p->user->shell;
		break;
	case USER_PROP_QWK:
		val=p->user->qwk;
		break;
	case USER_PROP_TMPEXT:
		s=p->user->tmpext;
		break;
	case USER_PROP_CHAT:
		val=p->user->laston;
		break;
	case USER_PROP_NS_TIME:
		val=p->user->laston;
		break;
	case USER_PROP_PROT:
		sprintf(tmp,"%c",p->user->prot);
		s=tmp;
		break;
	default:
		return(TRUE);
	}
	if(s!=NULL) 
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, s));
	else
		*vp = INT_TO_JSVAL(val);

	return(TRUE);
}

#define USEROBJ_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static struct JSPropertySpec js_user_properties[] = {
/*		 name				,tinyid					,flags,				getter,	setter	*/

	{	"alias"				,USER_PROP_ALIAS 		,USEROBJ_FLAGS,		NULL,NULL},
	{	"name"				,USER_PROP_NAME		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"handle"			,USER_PROP_HANDLE	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"note"				,USER_PROP_NOTE		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"ip_address"		,USER_PROP_NOTE		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"computer"			,USER_PROP_COMP		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"host_name"			,USER_PROP_COMP		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"comment"			,USER_PROP_COMMENT	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"netmail"			,USER_PROP_NETMAIL	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"email"				,USER_PROP_NETMAIL	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"address"			,USER_PROP_ADDRESS	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"location"			,USER_PROP_LOCATION	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"zipcode"			,USER_PROP_ZIPCODE	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"phone"				,USER_PROP_PHONE  	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"birthdate"			,USER_PROP_BIRTH  	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"modem"				,USER_PROP_MODEM      	,USEROBJ_FLAGS,		NULL,NULL},
	{	"connection"		,USER_PROP_MODEM      	,USEROBJ_FLAGS,		NULL,NULL},
	{	"screen_rows"		,USER_PROP_ROWS		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"gender"			,USER_PROP_SEX		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"cursub"			,USER_PROP_CURSUB	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"curdir"			,USER_PROP_CURDIR	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"editor"			,USER_PROP_XEDIT 	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"command_shell"		,USER_PROP_SHELL 	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"settings"			,USER_PROP_MISC		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"qwk_settings"		,USER_PROP_QWK		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"chat_settings"		,USER_PROP_CHAT		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"temp_file_ext"		,USER_PROP_TMPEXT	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"newscan_date"		,USER_PROP_NS_TIME	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"download_protocol"	,USER_PROP_PROT		 	,USEROBJ_FLAGS,		NULL,NULL},
	{0}
};

/* user.stats: These should be READ ONLY by nature */
static struct JSPropertySpec js_user_stats_properties[] = {
/*		 name				,tinyid					,flags,				getter,	setter	*/

	{	"laston_date"		,USER_PROP_LASTON	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"firston_date"		,USER_PROP_FIRSTON	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"total_logons"		,USER_PROP_LOGONS     	,USEROBJ_FLAGS,		NULL,NULL},
	{	"logons_today"		,USER_PROP_LTODAY     	,USEROBJ_FLAGS,		NULL,NULL},
	{	"total_timeon"		,USER_PROP_TIMEON     	,USEROBJ_FLAGS,		NULL,NULL},
	{	"timeon_today"		,USER_PROP_TTODAY     	,USEROBJ_FLAGS,		NULL,NULL},
	{	"timeon_last_logon"	,USER_PROP_TLAST      	,USEROBJ_FLAGS,		NULL,NULL},
	{	"total_posts"		,USER_PROP_POSTS      	,USEROBJ_FLAGS,		NULL,NULL},
	{	"total_emails"		,USER_PROP_EMAILS     	,USEROBJ_FLAGS,		NULL,NULL},
	{	"total_feedbacks"	,USER_PROP_FBACKS     	,USEROBJ_FLAGS,		NULL,NULL},
	{	"email_today"		,USER_PROP_ETODAY	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"posts_today"		,USER_PROP_PTODAY	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"bytes_uploaded"	,USER_PROP_ULB        	,USEROBJ_FLAGS,		NULL,NULL},
	{	"files_uploaded"	,USER_PROP_ULS        	,USEROBJ_FLAGS,		NULL,NULL},
	{	"bytes_downloaded"	,USER_PROP_DLB        	,USEROBJ_FLAGS,		NULL,NULL},
	{	"files_downloaded"	,USER_PROP_DLS        	,USEROBJ_FLAGS,		NULL,NULL},
	{	"leech_attempts"	,USER_PROP_LEECH 	 	,USEROBJ_FLAGS,		NULL,NULL},
	{0}
};

/* user.security */
static struct JSPropertySpec js_user_security_properties[] = {
/*		 name				,tinyid					,flags,				getter,	setter	*/

	{	"password"			,USER_PROP_PASS		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"password_date"		,USER_PROP_PWMOD      	,USEROBJ_FLAGS,		NULL,NULL},
	{	"level"				,USER_PROP_LEVEL 	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"flags1"			,USER_PROP_FLAGS1	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"flags2"			,USER_PROP_FLAGS2	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"flags3"			,USER_PROP_FLAGS3	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"flags4"			,USER_PROP_FLAGS4	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"exemptions"		,USER_PROP_EXEMPT	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"restrictions"		,USER_PROP_REST		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"credits"			,USER_PROP_CDT		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"free_credits"		,USER_PROP_FREECDT	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"minutes"			,USER_PROP_MIN		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"extra_time"		,USER_PROP_TEXTRA  	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"expiration_date"	,USER_PROP_EXPIRE     	,USEROBJ_FLAGS,		NULL,NULL},
	{0}
};

static void js_user_finalize(JSContext *cx, JSObject *obj)
{
	private_t* p;

	p=(private_t*)JS_GetPrivate(cx,obj);

	if(p!=NULL)
		free(p);

	p=NULL;
	JS_SetPrivate(cx,obj,p);
}

static JSBool
js_chk_ar(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		ar;
	JSString*	js_str;
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_FALSE;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL)
		return JS_FALSE;

	ar = arstr(NULL,JS_GetStringBytes(js_str),p->cfg);

	*rval = BOOLEAN_TO_JSVAL(chk_ar(p->cfg,ar,p->user));

	if(ar!=NULL && ar!=nular)
		free(ar);

	return(JS_TRUE);
}

static JSFunctionSpec js_user_functions[] = {
	{"compare_ars",	js_chk_ar,			1},		/* Verify ARS */
	{0}
};

static JSClass js_user_class = {
     "User"					/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_user_get			/* getProperty	*/
	,js_user_set			/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_user_finalize		/* finalize		*/
};

static JSClass js_user_stats_class = {
     "UserStats"			/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_user_get			/* getProperty	*/
	,js_user_set			/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};


static JSClass js_user_security_class = {
     "UserSecurity"			/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_user_get			/* getProperty	*/
	,js_user_set			/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};

JSObject* DLLCALL js_CreateUserObject(JSContext* cx, JSObject* parent, scfg_t* cfg, char* name, user_t* user)
{
	JSObject*	userobj;
	JSObject*	statsobj;
	JSObject*	securityobj;
	private_t*	p;

	if((p=(private_t*)malloc(sizeof(private_t)))==NULL)
		return(NULL);

	p->cfg = cfg;
	p->user = user;

	userobj = JS_DefineObject(cx, parent, name, &js_user_class, NULL, 0);

	if(userobj==NULL)
		return(NULL);

	JS_SetPrivate(cx, userobj, p);	

	JS_DefineProperties(cx, userobj, js_user_properties);

	JS_DefineFunctions(cx, userobj, js_user_functions);

	/* user.stats */
	statsobj = JS_DefineObject(cx, userobj, "stats"
		,&js_user_stats_class, NULL, 0);

	if(statsobj==NULL)
		return(NULL);

	JS_SetPrivate(cx, statsobj, p);

	JS_DefineProperties(cx, statsobj, js_user_stats_properties);

	/* user.security */
	securityobj = JS_DefineObject(cx, userobj, "security"
		,&js_user_security_class, NULL, 0);

	if(securityobj==NULL)
		return(NULL);

	JS_SetPrivate(cx, securityobj, p);

	JS_DefineProperties(cx, securityobj, js_user_security_properties);

	return(userobj);
}

#endif	/* JAVSCRIPT */