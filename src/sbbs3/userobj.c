/* userobj.c */

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

static JSBool js_user_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	char*		p=NULL;
	char		tmp[128];
	ulong		val;
    jsint       tiny;
	user_t*		user;

	if((user=(user_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_FALSE;

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
	case USER_PROP_ALIAS: 
		p=user->alias;
		break;
	case USER_PROP_NAME:
		p=user->name;
		break;
	case USER_PROP_HANDLE:
		p=user->handle;
		break;
	case USER_PROP_NOTE:
		p=user->note;
		break;
	case USER_PROP_COMP:
		p=user->comp;
		break;
	case USER_PROP_COMMENT:
		p=user->comment;
		break;
	case USER_PROP_NETMAIL:
		p=user->netmail;
		break;
	case USER_PROP_ADDRESS:
		p=user->address;
		break;
	case USER_PROP_LOCATION:
		p=user->location;
		break;
	case USER_PROP_ZIPCODE:
		p=user->zipcode;
		break;
	case USER_PROP_PASS:
		p=user->pass;
		break;
	case USER_PROP_PHONE:
		p=user->phone;
		break;
	case USER_PROP_BIRTH:
		p=user->birth;
		break;
	case USER_PROP_MODEM:
		p=user->modem;
		break;
	case USER_PROP_LASTON:
		val=user->laston;
		break;
	case USER_PROP_FIRSTON:
		val=user->firston;
		break;
	case USER_PROP_EXPIRE:
		val=user->expire;
		break;
	case USER_PROP_PWMOD: 
		val=user->pwmod;
		break;
	case USER_PROP_LOGONS:
		val=user->logons;
		break;
	case USER_PROP_LTODAY:
		val=user->ltoday;
		break;
	case USER_PROP_TIMEON:
		val=user->timeon;
		break;
	case USER_PROP_TEXTRA:
		val=user->textra;
		break;
	case USER_PROP_TTODAY:
		val=user->ttoday;
		break;
	case USER_PROP_TLAST: 
		val=user->tlast;
		break;
	case USER_PROP_POSTS: 
		val=user->posts;
		break;
	case USER_PROP_EMAILS: 
		val=user->emails;
		break;
	case USER_PROP_FBACKS: 
		val=user->fbacks;
		break;
	case USER_PROP_ETODAY:	
		val=user->etoday;
		break;
	case USER_PROP_PTODAY:
		val=user->ptoday;
		break;
	case USER_PROP_ULB:
		val=user->ulb;
		break;
	case USER_PROP_ULS:
		val=user->uls;
		break;
	case USER_PROP_DLB:
		val=user->dlb;
		break;
	case USER_PROP_DLS:
		val=user->dls;
		break;
	case USER_PROP_CDT:
		val=user->cdt;
		break;
	case USER_PROP_MIN:
		val=user->min;
		break;
	case USER_PROP_LEVEL:
		val=user->level;
		break;
	case USER_PROP_FLAGS1:
		val=user->flags1;
		break;
	case USER_PROP_FLAGS2:
		val=user->flags2;
		break;
	case USER_PROP_FLAGS3:
		val=user->flags3;
		break;
	case USER_PROP_FLAGS4:
		val=user->flags4;
		break;
	case USER_PROP_EXEMPT:
		val=user->exempt;
		break;
	case USER_PROP_REST:
		val=user->rest;
		break;
	case USER_PROP_ROWS:
		val=user->rows;
		break;
	case USER_PROP_SEX:
		sprintf(tmp,"%c",user->sex);
		p=tmp;
		break;
	case USER_PROP_MISC:
		val=user->misc;
		break;
	case USER_PROP_LEECH:
		val=user->leech;
		break;
	case USER_PROP_CURSUB:
		p=user->cursub;
		break;
	case USER_PROP_CURDIR:
		p=user->curdir;
		break;
	case USER_PROP_FREECDT:
		val=user->freecdt;
		break;
	case USER_PROP_XEDIT:
		val=user->xedit;
		break;
	case USER_PROP_SHELL:
		val=user->shell;
		break;
	case USER_PROP_QWK:
		val=user->qwk;
		break;
	case USER_PROP_TMPEXT:
		p=user->tmpext;
		break;
	case USER_PROP_CHAT:
		val=user->laston;
		break;
	case USER_PROP_NS_TIME:
		val=user->laston;
		break;
	case USER_PROP_PROT:
		sprintf(tmp,"%c",user->prot);
		p=tmp;
		break;
	}
	if(p!=NULL) 
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, p));
	else
		*vp = INT_TO_JSVAL(val);

	return(TRUE);
}

#define USEROBJ_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static struct JSPropertySpec js_user_properties[] = {
/*		 name			,tinyid						,flags,				getter,	setter	*/

	{	"alias"				,USER_PROP_ALIAS 		,USEROBJ_FLAGS,		NULL,NULL},
	{	"name"				,USER_PROP_NAME		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"handle"			,USER_PROP_HANDLE	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"note"				,USER_PROP_NOTE		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"computer"			,USER_PROP_COMP		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"comment"			,USER_PROP_COMMENT	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"netmail"			,USER_PROP_NETMAIL	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"address"			,USER_PROP_ADDRESS	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"location"			,USER_PROP_LOCATION	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"zipcode"			,USER_PROP_ZIPCODE	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"password"			,USER_PROP_PASS		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"phone"				,USER_PROP_PHONE  	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"birthdate"			,USER_PROP_BIRTH  	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"modem"				,USER_PROP_MODEM      	,USEROBJ_FLAGS,		NULL,NULL},
	{	"laston_date"		,USER_PROP_LASTON	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"firston_date"		,USER_PROP_FIRSTON	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"expiration_date"	,USER_PROP_EXPIRE     	,USEROBJ_FLAGS,		NULL,NULL},
	{	"password_date"		,USER_PROP_PWMOD      	,USEROBJ_FLAGS,		NULL,NULL},
	{	"total_logons"		,USER_PROP_LOGONS     	,USEROBJ_FLAGS,		NULL,NULL},
	{	"logons_today"		,USER_PROP_LTODAY     	,USEROBJ_FLAGS,		NULL,NULL},
	{	"total_timeon"		,USER_PROP_TIMEON     	,USEROBJ_FLAGS,		NULL,NULL},
	{	"extra_time"		,USER_PROP_TEXTRA  	 	,USEROBJ_FLAGS,		NULL,NULL},
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
	{	"credits"			,USER_PROP_CDT		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"minutes"			,USER_PROP_MIN		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"level"				,USER_PROP_LEVEL 	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"flags1"			,USER_PROP_FLAGS1	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"flags2"			,USER_PROP_FLAGS2	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"flags3"			,USER_PROP_FLAGS3	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"flags4"			,USER_PROP_FLAGS4	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"exemptions"		,USER_PROP_EXEMPT	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"restrictions"		,USER_PROP_REST		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"screen_rows"		,USER_PROP_ROWS		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"gender"			,USER_PROP_SEX		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"settings"			,USER_PROP_MISC		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"leech_attempts"	,USER_PROP_LEECH 	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"cursub"			,USER_PROP_CURSUB	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"curdir"			,USER_PROP_CURDIR	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"free_credits"		,USER_PROP_FREECDT	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"editor"			,USER_PROP_XEDIT 	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"command_shell"		,USER_PROP_SHELL 	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"qwk_settings"		,USER_PROP_QWK		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"temp_file_ext"		,USER_PROP_TMPEXT	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"chat_settings"		,USER_PROP_CHAT		 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"newscan_date"		,USER_PROP_NS_TIME	 	,USEROBJ_FLAGS,		NULL,NULL},
	{	"download_protocol"	,USER_PROP_PROT		 	,USEROBJ_FLAGS,		NULL,NULL},
	{0}
};

static JSClass js_user_class = {
     "User"					/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_user_get			/* getProperty	*/
	,JS_PropertyStub		/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};

static JSClass js_userstats_class = {
     "Stats"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,NULL /* js_sysstats_get		/* getProperty	*/
	,JS_PropertyStub		/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};


JSObject* DLLCALL CreateUserObject(scfg_t* cfg, JSContext* cx, JSObject* parent, char* name, user_t* user)
{
	JSObject*	userobj;

	userobj = JS_DefineObject(cx, parent, name, &js_user_class, NULL, 0);

	if(userobj==NULL)
		return(NULL);

	JS_SetPrivate(cx, userobj, user);	/* Store a pointer to scfg_t */

	JS_DefineProperties(cx, userobj, js_user_properties);

	return(userobj);
}

#endif	/* JAVSCRIPT */