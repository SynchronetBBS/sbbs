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

static scfg_t* scfg=NULL;

static const char* getprivate_failure = "line %d %s JS_GetPrivate failed";

typedef struct
{
	uint	usernumber;
	scfg_t*	cfg;

} private_t;

/* User Object Properites */
enum {
	 USER_PROP_NUMBER
	,USER_PROP_ALIAS 	
	,USER_PROP_NAME		
	,USER_PROP_HANDLE	
	,USER_PROP_NOTE		
	,USER_PROP_COMP		
	,USER_PROP_COMMENT	
	,USER_PROP_NETMAIL	
	,USER_PROP_EMAIL	/* READ ONLY */
	,USER_PROP_ADDRESS	
	,USER_PROP_LOCATION	
	,USER_PROP_ZIPCODE
	,USER_PROP_PASS
	,USER_PROP_PHONE  	
	,USER_PROP_BIRTH  
	,USER_PROP_AGE		/* READ ONLY */
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
	,USER_PROP_CURXTRN
	,USER_PROP_FREECDT	
	,USER_PROP_XEDIT 	
	,USER_PROP_SHELL 	
	,USER_PROP_QWK		
	,USER_PROP_TMPEXT	
	,USER_PROP_CHAT		
	,USER_PROP_NS_TIME	
	,USER_PROP_PROT		
	,USER_PROP_LOGONTIME
};


static JSBool js_user_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	char*		s=NULL;
	char		tmp[128];
	ulong		val=0;
    jsint       tiny;
	JSString*	js_str;
	user_t		user;
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	user.number=p->usernumber;
	getuserdat(p->cfg,&user);

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case USER_PROP_NUMBER:
			val=user.number;
			break;
		case USER_PROP_ALIAS: 
			s=user.alias;
			break;
		case USER_PROP_NAME:
			s=user.name;
			break;
		case USER_PROP_HANDLE:
			s=user.handle;
			break;
		case USER_PROP_NOTE:
			s=user.note;
			break;
		case USER_PROP_COMP:
			s=user.comp;
			break;
		case USER_PROP_COMMENT:
			s=user.comment;
			break;
		case USER_PROP_NETMAIL:
			s=user.netmail;
			break;
		case USER_PROP_EMAIL:
			s=usermailaddr(p->cfg, tmp
				,p->cfg->inetmail_misc&NMAIL_ALIAS ? user.alias : user.name);
			break;
		case USER_PROP_ADDRESS:
			s=user.address;
			break;
		case USER_PROP_LOCATION:
			s=user.location;
			break;
		case USER_PROP_ZIPCODE:
			s=user.zipcode;
			break;
		case USER_PROP_PASS:
			s=user.pass;
			break;
		case USER_PROP_PHONE:
			s=user.phone;
			break;
		case USER_PROP_BIRTH:
			s=user.birth;
			break;
		case USER_PROP_AGE:
			val=getage(p->cfg,user.birth);
			break;
		case USER_PROP_MODEM:
			s=user.modem;
			break;
		case USER_PROP_LASTON:
			val=user.laston;
			break;
		case USER_PROP_FIRSTON:
			val=user.firston;
			break;
		case USER_PROP_EXPIRE:
			val=user.expire;
			break;
		case USER_PROP_PWMOD: 
			val=user.pwmod;
			break;
		case USER_PROP_LOGONS:
			val=user.logons;
			break;
		case USER_PROP_LTODAY:
			val=user.ltoday;
			break;
		case USER_PROP_TIMEON:
			val=user.timeon;
			break;
		case USER_PROP_TEXTRA:
			val=user.textra;
			break;
		case USER_PROP_TTODAY:
			val=user.ttoday;
			break;
		case USER_PROP_TLAST: 
			val=user.tlast;
			break;
		case USER_PROP_POSTS: 
			val=user.posts;
			break;
		case USER_PROP_EMAILS: 
			val=user.emails;
			break;
		case USER_PROP_FBACKS: 
			val=user.fbacks;
			break;
		case USER_PROP_ETODAY:	
			val=user.etoday;
			break;
		case USER_PROP_PTODAY:
			val=user.ptoday;
			break;
		case USER_PROP_ULB:
			val=user.ulb;
			break;
		case USER_PROP_ULS:
			val=user.uls;
			break;
		case USER_PROP_DLB:
			val=user.dlb;
			break;
		case USER_PROP_DLS:
			val=user.dls;
			break;
		case USER_PROP_CDT:
			val=user.cdt;
			break;
		case USER_PROP_MIN:
			val=user.min;
			break;
		case USER_PROP_LEVEL:
			val=user.level;
			break;
		case USER_PROP_FLAGS1:
			val=user.flags1;
			break;
		case USER_PROP_FLAGS2:
			val=user.flags2;
			break;
		case USER_PROP_FLAGS3:
			val=user.flags3;
			break;
		case USER_PROP_FLAGS4:
			val=user.flags4;
			break;
		case USER_PROP_EXEMPT:
			val=user.exempt;
			break;
		case USER_PROP_REST:
			val=user.rest;
			break;
		case USER_PROP_ROWS:
			val=user.rows;
			break;
		case USER_PROP_SEX:
			sprintf(tmp,"%c",user.sex);
			s=tmp;
			break;
		case USER_PROP_MISC:
			val=user.misc;
			break;
		case USER_PROP_LEECH:
			val=user.leech;
			break;
		case USER_PROP_CURSUB:
			s=user.cursub;
			break;
		case USER_PROP_CURDIR:
			s=user.curdir;
			break;
		case USER_PROP_CURXTRN:
			s=user.curxtrn;
			break;

		case USER_PROP_FREECDT:
			val=user.freecdt;
			break;
		case USER_PROP_XEDIT:
			if(user.xedit>0 && user.xedit<=p->cfg->total_xedits)
				s=p->cfg->xedit[user.xedit-1]->code;
			else
				s=""; /* internal editor */
			break;
		case USER_PROP_SHELL:
			s=p->cfg->shell[user.shell]->code;
			break;
		case USER_PROP_QWK:
			val=user.qwk;
			break;
		case USER_PROP_TMPEXT:
			s=user.tmpext;
			break;
		case USER_PROP_CHAT:
			val=user.chat;
			break;
		case USER_PROP_NS_TIME:
			val=user.ns_time;
			break;
		case USER_PROP_PROT:
			sprintf(tmp,"%c",user.prot);
			s=tmp;
			break;
		case USER_PROP_LOGONTIME:
			val=user.logontime;
			break;

		default:	
			/* This must not set vp in order for child objects to work (stats and security) */
			return(JS_TRUE);
	}
	if(s!=NULL) {
		if((js_str=JS_NewStringCopyZ(cx, s))==NULL)
			return(JS_FALSE);
		*vp = STRING_TO_JSVAL(js_str);
	} else {
		if(INT_FITS_IN_JSVAL(val) && !(val&0x80000000))
			*vp = INT_TO_JSVAL(val);
		else
            JS_NewDoubleValue(cx, val, vp);
	}
	return(JS_TRUE);
}

static JSBool js_user_set(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	char*		str;
	char		tmp[64];
	jsint		val;
	ulong		usermisc;
    jsint       tiny;
	JSString*	js_str;
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if((js_str=JS_ValueToString(cx,*vp))==NULL)
		return(JS_FALSE);

	if((str=JS_GetStringBytes(js_str))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case USER_PROP_NUMBER:
			JS_ValueToInt32(cx, *vp, (jsint*)&p->usernumber);
			break;
		case USER_PROP_ALIAS:
			/* update USER.DAT */
			putuserrec(p->cfg,p->usernumber,U_ALIAS,LEN_ALIAS,str);

			/* update NAME.DAT */
			getuserrec(p->cfg,p->usernumber,U_MISC,8,tmp);
			usermisc=ahtoul(tmp);
			if(!(usermisc&DELETED))
				putusername(p->cfg,p->usernumber,str);
			break;
		case USER_PROP_NAME:
			putuserrec(p->cfg,p->usernumber,U_NAME,LEN_NAME,str);
			break;
		case USER_PROP_HANDLE:
			putuserrec(p->cfg,p->usernumber,U_HANDLE,LEN_HANDLE,str);
			break;
		case USER_PROP_NOTE:		 
			putuserrec(p->cfg,p->usernumber,U_NOTE,LEN_NOTE,str);
			break;
		case USER_PROP_COMP:		 
			putuserrec(p->cfg,p->usernumber,U_COMP,LEN_COMP,str);
			break;
		case USER_PROP_COMMENT:	 
			putuserrec(p->cfg,p->usernumber,U_COMMENT,LEN_COMMENT,str);
			break;
		case USER_PROP_NETMAIL:	 
			putuserrec(p->cfg,p->usernumber,U_NETMAIL,LEN_NETMAIL,str);
			break;
		case USER_PROP_ADDRESS:	 
			putuserrec(p->cfg,p->usernumber,U_ADDRESS,LEN_ADDRESS,str);
			break;
		case USER_PROP_LOCATION:	 
			putuserrec(p->cfg,p->usernumber,U_LOCATION,LEN_LOCATION,str);
			break;
		case USER_PROP_ZIPCODE:	 
			putuserrec(p->cfg,p->usernumber,U_ZIPCODE,LEN_ZIPCODE,str);
			break;
		case USER_PROP_PHONE:  	 
			putuserrec(p->cfg,p->usernumber,U_PHONE,LEN_PHONE,str);
			break;
		case USER_PROP_BIRTH:  	 
			putuserrec(p->cfg,p->usernumber,U_BIRTH,LEN_BIRTH,str);
			break;
		case USER_PROP_MODEM:     
			putuserrec(p->cfg,p->usernumber,U_MODEM,LEN_MODEM,str);
			break;
		case USER_PROP_ROWS:		 
			putuserrec(p->cfg,p->usernumber,U_ROWS,0,str);	/* base 10 */
			break;
		case USER_PROP_SEX:		 
			putuserrec(p->cfg,p->usernumber,U_SEX,0,strupr(str));	/* single char */
			break;
		case USER_PROP_CURSUB:	 
			putuserrec(p->cfg,p->usernumber,U_CURSUB,0,strupr(str));
			break;
		case USER_PROP_CURDIR:	 
			putuserrec(p->cfg,p->usernumber,U_CURDIR,0,strupr(str));
			break;
		case USER_PROP_CURXTRN:	 
			putuserrec(p->cfg,p->usernumber,U_CURXTRN,0,strupr(str));
			break;
		case USER_PROP_XEDIT: 	 
			putuserrec(p->cfg,p->usernumber,U_XEDIT,0,strupr(str));
			break;
		case USER_PROP_SHELL: 	 
			putuserrec(p->cfg,p->usernumber,U_COMP,0,strupr(str));
			break;
		case USER_PROP_MISC:
			JS_ValueToInt32(cx,*vp,&val);
			putuserrec(p->cfg,p->usernumber,U_MISC,0,ultoa(val,tmp,16));
			break;
		case USER_PROP_QWK:		 
			JS_ValueToInt32(cx,*vp,&val);
			putuserrec(p->cfg,p->usernumber,U_QWK,0,ultoa(val,tmp,16));
			break;
		case USER_PROP_CHAT:		 
			JS_ValueToInt32(cx,*vp,&val);
			putuserrec(p->cfg,p->usernumber,U_CHAT,0,ultoa(val,tmp,16));
			break;
		case USER_PROP_TMPEXT:	 
			putuserrec(p->cfg,p->usernumber,U_TMPEXT,0,str);
			break;
		case USER_PROP_NS_TIME:	 
			JS_ValueToInt32(cx,*vp,&val);
			putuserrec(p->cfg,p->usernumber,U_NS_TIME,0,ultoa(val,tmp,16));
			break;
		case USER_PROP_PROT:	
			putuserrec(p->cfg,p->usernumber,U_PROT,0,strupr(str)); /* single char */
			break;
		case USER_PROP_LOGONTIME:	 
			JS_ValueToInt32(cx,*vp,&val);
			putuserrec(p->cfg,p->usernumber,U_LOGONTIME,0,ultoa(val,tmp,16));
			break;
			
		/* security properties*/
		case USER_PROP_PASS:	
			putuserrec(p->cfg,p->usernumber,U_PASS,LEN_PASS,strupr(str));
			break;
		case USER_PROP_PWMOD:
			JS_ValueToInt32(cx,*vp,&val);
			putuserrec(p->cfg,p->usernumber,U_PWMOD,0,ultoa(val,tmp,16));
			break;
		case USER_PROP_LEVEL: 
			putuserrec(p->cfg,p->usernumber,U_LEVEL,0,str);
			break;
		case USER_PROP_FLAGS1:
			JS_ValueToInt32(cx,*vp,&val);
			putuserrec(p->cfg,p->usernumber,U_FLAGS1,0,ultoa(val,tmp,16));
			break;
		case USER_PROP_FLAGS2:
			JS_ValueToInt32(cx,*vp,&val);
			putuserrec(p->cfg,p->usernumber,U_FLAGS2,0,ultoa(val,tmp,16));
			break;
		case USER_PROP_FLAGS3:
			JS_ValueToInt32(cx,*vp,&val);
			putuserrec(p->cfg,p->usernumber,U_FLAGS3,0,ultoa(val,tmp,16));
			break;
		case USER_PROP_FLAGS4:
			JS_ValueToInt32(cx,*vp,&val);
			putuserrec(p->cfg,p->usernumber,U_FLAGS4,0,ultoa(val,tmp,16));
			break;
		case USER_PROP_EXEMPT:
			JS_ValueToInt32(cx,*vp,&val);
			putuserrec(p->cfg,p->usernumber,U_EXEMPT,0,ultoa(val,tmp,16));
			break;
		case USER_PROP_REST:	
			JS_ValueToInt32(cx,*vp,&val);
			putuserrec(p->cfg,p->usernumber,U_REST,0,ultoa(val,tmp,16));
			break;
		case USER_PROP_CDT:	
			putuserrec(p->cfg,p->usernumber,U_CDT,0,str);
			break;
		case USER_PROP_FREECDT:
			putuserrec(p->cfg,p->usernumber,U_FREECDT,0,str);
			break;
		case USER_PROP_MIN:	
			putuserrec(p->cfg,p->usernumber,U_MIN,0,str);
			break;
		case USER_PROP_TEXTRA:  
			putuserrec(p->cfg,p->usernumber,U_TEXTRA,0,str);
			break;
		case USER_PROP_EXPIRE:  
			JS_ValueToInt32(cx,*vp,&val);
			putuserrec(p->cfg,p->usernumber,U_EXPIRE,0,ultoa(val,tmp,16));
			break;
	}

	return(JS_TRUE);
}

#define USER_PROP_FLAGS JSPROP_ENUMERATE

static struct JSPropertySpec js_user_properties[] = {
/*		 name				,tinyid					,flags,				getter,	setter	*/

	{	"number"			,USER_PROP_NUMBER		,USER_PROP_FLAGS,		NULL,NULL},
	{	"alias"				,USER_PROP_ALIAS 		,USER_PROP_FLAGS,		NULL,NULL},
	{	"name"				,USER_PROP_NAME		 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"handle"			,USER_PROP_HANDLE	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"ip_address"		,USER_PROP_NOTE		 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"note"				,USER_PROP_NOTE		 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"host_name"			,USER_PROP_COMP		 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"computer"			,USER_PROP_COMP		 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"comment"			,USER_PROP_COMMENT	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"netmail"			,USER_PROP_NETMAIL	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"email"				,USER_PROP_EMAIL	 	,USER_PROP_FLAGS|JSPROP_READONLY,		NULL,NULL},
	{	"address"			,USER_PROP_ADDRESS	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"location"			,USER_PROP_LOCATION	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"zipcode"			,USER_PROP_ZIPCODE	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"phone"				,USER_PROP_PHONE  	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"birthdate"			,USER_PROP_BIRTH  	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"age"				,USER_PROP_AGE			,USER_PROP_FLAGS|JSPROP_READONLY,		NULL,NULL},
	{	"connection"		,USER_PROP_MODEM      	,USER_PROP_FLAGS,		NULL,NULL},
	{	"modem"				,USER_PROP_MODEM      	,USER_PROP_FLAGS,		NULL,NULL},
	{	"screen_rows"		,USER_PROP_ROWS		 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"gender"			,USER_PROP_SEX		 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"cursub"			,USER_PROP_CURSUB	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"curdir"			,USER_PROP_CURDIR	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"curxtrn"			,USER_PROP_CURXTRN	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"editor"			,USER_PROP_XEDIT 	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"command_shell"		,USER_PROP_SHELL 	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"settings"			,USER_PROP_MISC		 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"qwk_settings"		,USER_PROP_QWK		 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"chat_settings"		,USER_PROP_CHAT		 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"temp_file_ext"		,USER_PROP_TMPEXT	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"newscan_date"		,USER_PROP_NS_TIME	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"download_protocol"	,USER_PROP_PROT		 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"logontime"			,USER_PROP_LOGONTIME 	,USER_PROP_FLAGS,		NULL,NULL},
	{0}
};

#ifdef _DEBUG
static char* user_prop_desc[] = {

	 "record number (1-based)"
	,"alias/name"
	,"real name"
	,"chat handle"
	,"IP address last logged on from"
	,"AKA ip_address"
	,"host name last logged on from"
	,"AKA host_name"
	,"sysop's comment"
	,"external e-mail address"
	,"local Internet e-mail address	- <small>READ ONLY</small>"
	,"street address"
	,"location (city, state)"
	,"zip/postal code"
	,"phone number"
	,"birth date"
	,"calculated age in years - <small>READ ONLY</small>"
	,"connection type"
	,"AKA connection"
	,"terminal rows (lines)"
	,"gender type"
	,"current message sub-board"
	,"current file directory"
	,"current external program being run"
	,"external message editor"
	,"command shell"
	,"settings bitfield"
	,"QWK packet settings bitfield"
	,"chat settings bitfield"
	,"temporary file type (extension)"
	,"new file scan date/time (time_t format)"
	,"file transfer protocol (command key)"
	,"logon time (time_t format)"
	,NULL
};
#endif


/* user.security */
static struct JSPropertySpec js_user_security_properties[] = {
/*		 name				,tinyid					,flags,				getter,	setter	*/

	{	"password"			,USER_PROP_PASS		 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"password_date"		,USER_PROP_PWMOD      	,USER_PROP_FLAGS,		NULL,NULL},
	{	"level"				,USER_PROP_LEVEL 	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"flags1"			,USER_PROP_FLAGS1	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"flags2"			,USER_PROP_FLAGS2	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"flags3"			,USER_PROP_FLAGS3	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"flags4"			,USER_PROP_FLAGS4	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"exemptions"		,USER_PROP_EXEMPT	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"restrictions"		,USER_PROP_REST		 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"credits"			,USER_PROP_CDT		 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"free_credits"		,USER_PROP_FREECDT	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"minutes"			,USER_PROP_MIN		 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"extra_time"		,USER_PROP_TEXTRA  	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"expiration_date"	,USER_PROP_EXPIRE     	,USER_PROP_FLAGS,		NULL,NULL},
	{0}
};

#ifdef _DEBUG
static char* user_security_prop_desc[] = {

	 "password"
	,"date password last modified (time_t format)"
	,"security level (0-99)"
	,"flag set #1 (bitfield)"
	,"flag set #2 (bitfield)"
	,"flag set #3 (bitfield)"
	,"flag set #4 (bitfield)"
	,"exemption flags (bitfield)"
	,"restriction flags (bitfield)"
	,"credits"
	,"free credits (for today only)"
	,"extra minutes (time bank)"
	,"extra minutes (for today only)"
	,"expiration date/time (time_t format)"
	,NULL
};
#endif


#undef  USER_PROP_FLAGS
#define USER_PROP_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

/* user.stats: These should be READ ONLY by nature */
static struct JSPropertySpec js_user_stats_properties[] = {
/*		 name				,tinyid					,flags,				getter,	setter	*/

	{	"laston_date"		,USER_PROP_LASTON	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"firston_date"		,USER_PROP_FIRSTON	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"total_logons"		,USER_PROP_LOGONS     	,USER_PROP_FLAGS,		NULL,NULL},
	{	"logons_today"		,USER_PROP_LTODAY     	,USER_PROP_FLAGS,		NULL,NULL},
	{	"total_timeon"		,USER_PROP_TIMEON     	,USER_PROP_FLAGS,		NULL,NULL},
	{	"timeon_today"		,USER_PROP_TTODAY     	,USER_PROP_FLAGS,		NULL,NULL},
	{	"timeon_last_logon"	,USER_PROP_TLAST      	,USER_PROP_FLAGS,		NULL,NULL},
	{	"total_posts"		,USER_PROP_POSTS      	,USER_PROP_FLAGS,		NULL,NULL},
	{	"total_emails"		,USER_PROP_EMAILS     	,USER_PROP_FLAGS,		NULL,NULL},
	{	"total_feedbacks"	,USER_PROP_FBACKS     	,USER_PROP_FLAGS,		NULL,NULL},
	{	"email_today"		,USER_PROP_ETODAY	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"posts_today"		,USER_PROP_PTODAY	 	,USER_PROP_FLAGS,		NULL,NULL},
	{	"bytes_uploaded"	,USER_PROP_ULB        	,USER_PROP_FLAGS,		NULL,NULL},
	{	"files_uploaded"	,USER_PROP_ULS        	,USER_PROP_FLAGS,		NULL,NULL},
	{	"bytes_downloaded"	,USER_PROP_DLB        	,USER_PROP_FLAGS,		NULL,NULL},
	{	"files_downloaded"	,USER_PROP_DLS        	,USER_PROP_FLAGS,		NULL,NULL},
	{	"leech_attempts"	,USER_PROP_LEECH 	 	,USER_PROP_FLAGS,		NULL,NULL},
	{0}
};

#ifdef _DEBUG
static char* user_stats_prop_desc[] = {

	 "date of previous logon"
	,"date of first logon"
	,"total number of logons"
	,"total logons today"
	,"total time used (in minutes)"
	,"time used today"
	,"time used last session"
	,"total messages posted"
	,"total e-mails sent"
	,"total feedback messages sent"
	,"e-mail sent today"
	,"messages posted today"
	,"total bytes uploaded"
	,"total files uploaded"
	,"total bytes downloaded"
	,"total files downloaded"
	,"suspected leech downloads"
	,NULL
};
#endif


static void js_user_finalize(JSContext *cx, JSObject *obj)
{
	private_t* p;

	p=(private_t*)JS_GetPrivate(cx,obj);

	if(p!=NULL && (scfg_t*)p!=scfg)
		free(p);

	p=NULL;
	JS_SetPrivate(cx,obj,p);
}

static JSBool
js_chk_ar(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uchar*		ar;
	JSString*	js_str;
	user_t		user;
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_FALSE;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL)
		return JS_FALSE;

	ar = arstr(NULL,JS_GetStringBytes(js_str),p->cfg);

	user.number=p->usernumber;
	getuserdat(p->cfg,&user);

	*rval = BOOLEAN_TO_JSVAL(chk_ar(p->cfg,ar,&user));

	if(ar!=NULL && ar!=nular)
		free(ar);

	return(JS_TRUE);
}


static jsMethodSpec js_user_functions[] = {
	{"compare_ars",	js_chk_ar,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("string ars")
	,JSDOCSTR("Verify user meets access requirements string")
	},		
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

/* User Constructor (creates instance of user class) */

static JSBool
js_user_constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		val=0;
	user_t		user;
	private_t*	p;
	JSObject*	statsobj;
	JSObject*	securityobj;

	JS_ValueToInt32(cx,argv[0],&val);
	user.number=(ushort)val;
	if(user.number!=0 && getuserdat(scfg,&user)!=0) {
		JS_ReportError(cx,"Invalid user number: %d",val);
		return(JS_FALSE);
	}

	/* user.stats */
	if((statsobj=JS_DefineObject(cx, obj, "stats"
		,&js_user_stats_class, NULL, JSPROP_ENUMERATE|JSPROP_READONLY))==NULL) 
		return(JS_FALSE);

	JS_DefineProperties(cx, statsobj, js_user_stats_properties);

	/* user.security */
	if((securityobj=JS_DefineObject(cx, obj, "security"
		,&js_user_security_class, NULL, JSPROP_ENUMERATE|JSPROP_READONLY))==NULL) 
		return(JS_FALSE);

	JS_DefineProperties(cx, securityobj, js_user_security_properties);

	if((p=(private_t*)malloc(sizeof(private_t)))==NULL)
		return(JS_FALSE);

	p->cfg = scfg;
	p->usernumber = user.number;

	JS_SetPrivate(cx, obj, p);
	JS_SetPrivate(cx, statsobj, p);
	JS_SetPrivate(cx, securityobj, p);

	js_DefineMethods(cx, obj, js_user_functions, FALSE);

	return(JS_TRUE);
}

JSObject* DLLCALL js_CreateUserClass(JSContext* cx, JSObject* parent, scfg_t* cfg)
{
	JSObject*	userclass;

	scfg = cfg;
	userclass = JS_InitClass(cx, parent, NULL
		,&js_user_class
		,js_user_constructor
		,1	/* number of constructor args */
		,js_user_properties
		,NULL /* funcs, defined in constructor */
		,NULL,NULL);

	return(userclass);
}

JSObject* DLLCALL js_CreateUserObject(JSContext* cx, JSObject* parent, scfg_t* cfg, char* name
									  , uint usernumber)
{
	JSObject*	userobj;
	JSObject*	statsobj;
	JSObject*	securityobj;
	private_t*	p;
	jsval		val;

	/* Return existing user object if it's already been created */
	if(JS_GetProperty(cx,parent,name,&val) && val!=JSVAL_VOID)
		userobj = JSVAL_TO_OBJECT(val);
	else
		userobj = JS_DefineObject(cx, parent, name, &js_user_class
								, NULL, JSPROP_ENUMERATE|JSPROP_READONLY);
	if(userobj==NULL)
		return(NULL);

	if((p=(private_t*)malloc(sizeof(private_t)))==NULL)
		return(NULL);

	p->cfg = cfg;
	p->usernumber = usernumber;

	JS_SetPrivate(cx, userobj, p);	

	JS_DefineProperties(cx, userobj, js_user_properties);

#ifdef _DEBUG
	js_DescribeObject(cx,userobj
		,"Instance of <i>User</i> class, representing current user online");
	js_DescribeConstructor(cx,userobj
		,"To create a new user object: <tt>var u = new User(number)</tt>");
	js_CreateArrayOfStrings(cx, userobj
		,"_property_desc_list", user_prop_desc, JSPROP_READONLY);
#endif

	js_DefineMethods(cx, userobj, js_user_functions, FALSE);

	/* user.stats */
	statsobj = JS_DefineObject(cx, userobj, "stats"
		,&js_user_stats_class, NULL, JSPROP_ENUMERATE|JSPROP_READONLY);

	if(statsobj==NULL) {
		free(p);
		return(NULL);
	}

	JS_SetPrivate(cx, statsobj, p);

	JS_DefineProperties(cx, statsobj, js_user_stats_properties);

#ifdef _DEBUG
	js_DescribeObject(cx,statsobj,"User statistics (all <small>READ ONLY</small>)");
	js_CreateArrayOfStrings(cx, statsobj, "_property_desc_list", user_stats_prop_desc, JSPROP_READONLY);
#endif

	/* user.security */
	securityobj = JS_DefineObject(cx, userobj, "security"
		,&js_user_security_class, NULL, JSPROP_ENUMERATE|JSPROP_READONLY);

	if(securityobj==NULL) {
		free(p);
		return(NULL);
	}

	JS_SetPrivate(cx, securityobj, p);

	JS_DefineProperties(cx, securityobj, js_user_security_properties);

#ifdef _DEBUG
	js_DescribeObject(cx,securityobj,"User security settings");
	js_CreateArrayOfStrings(cx, securityobj, "_property_desc_list", user_security_prop_desc, JSPROP_READONLY);
#endif

	return(userobj);
}

/****************************************************************************/
/* Creates all the user-specific objects: user, msg_area, file_area			*/
/****************************************************************************/
JSBool DLLCALL
js_CreateUserObjects(JSContext* cx, JSObject* parent, scfg_t* cfg, user_t* user
					 ,char* html_index_file, subscan_t* subscan)
{
	if(js_CreateUserObject(cx,parent,cfg,"user",user==NULL ? 0 : user->number)==NULL)
		return(JS_FALSE);
	if(js_CreateFileAreaObject(cx,parent,cfg,user,html_index_file)==NULL) 
		return(JS_FALSE);
	if(js_CreateMsgAreaObject(cx,parent,cfg,user,subscan)==NULL) 
		return(JS_FALSE);
	if(js_CreateXtrnAreaObject(cx,parent,cfg,user)==NULL)
		return(JS_FALSE);

	return(JS_TRUE);
}

#endif	/* JAVSCRIPT */
