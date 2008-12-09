/* js_user.c */

/* Synchronet JavaScript "User" Object */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2008 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#include "js_request.h"

#ifdef JAVASCRIPT

static scfg_t* scfg=NULL;

static const char* getprivate_failure = "line %d %s JS_GetPrivate failed";

typedef struct
{
	user_t	user;
	BOOL	cached;
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
	,USER_PROP_MAIL_WAITING
	,USER_PROP_MAIL_PENDING
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
	,USER_PROP_TIMEPERCALL
	,USER_PROP_TIMEPERDAY
	,USER_PROP_CALLSPERDAY
	,USER_PROP_LINESPERMSG
	,USER_PROP_EMAILPERDAY
	,USER_PROP_POSTSPERDAY
	,USER_PROP_FREECDTPERDAY
	,USER_PROP_CACHED
	,USER_PROP_IS_SYSOP
};

static void js_getuserdat(private_t* p)
{
	if(!p->cached) {
		
		if(getuserdat(p->cfg,&p->user)==0)
			p->cached=TRUE;
	}
}


static JSBool js_user_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	char*		s=NULL;
	char		tmp[128];
	ulong		val=0;
    jsint       tiny;
	JSString*	js_str;
	private_t*	p;
	jsrefcount	rc;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	js_getuserdat(p);

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case USER_PROP_NUMBER:
			val=p->user.number;
			break;
		case USER_PROP_ALIAS: 
			s=p->user.alias;
			break;
		case USER_PROP_NAME:
			s=p->user.name;
			break;
		case USER_PROP_HANDLE:
			s=p->user.handle;
			break;
		case USER_PROP_NOTE:
			s=p->user.note;
			break;
		case USER_PROP_COMP:
			s=p->user.comp;
			break;
		case USER_PROP_COMMENT:
			s=p->user.comment;
			break;
		case USER_PROP_NETMAIL:
			s=p->user.netmail;
			break;
		case USER_PROP_EMAIL:
			s=usermailaddr(p->cfg, tmp
				,p->cfg->inetmail_misc&NMAIL_ALIAS ? p->user.alias : p->user.name);
			break;
		case USER_PROP_ADDRESS:
			s=p->user.address;
			break;
		case USER_PROP_LOCATION:
			s=p->user.location;
			break;
		case USER_PROP_ZIPCODE:
			s=p->user.zipcode;
			break;
		case USER_PROP_PASS:
			s=p->user.pass;
			break;
		case USER_PROP_PHONE:
			s=p->user.phone;
			break;
		case USER_PROP_BIRTH:
			s=p->user.birth;
			break;
		case USER_PROP_AGE:
			val=getage(p->cfg,p->user.birth);
			break;
		case USER_PROP_MODEM:
			s=p->user.modem;
			break;
		case USER_PROP_LASTON:
			val=p->user.laston;
			break;
		case USER_PROP_FIRSTON:
			val=p->user.firston;
			break;
		case USER_PROP_EXPIRE:
			val=p->user.expire;
			break;
		case USER_PROP_PWMOD: 
			val=p->user.pwmod;
			break;
		case USER_PROP_LOGONS:
			val=p->user.logons;
			break;
		case USER_PROP_LTODAY:
			val=p->user.ltoday;
			break;
		case USER_PROP_TIMEON:
			val=p->user.timeon;
			break;
		case USER_PROP_TEXTRA:
			val=p->user.textra;
			break;
		case USER_PROP_TTODAY:
			val=p->user.ttoday;
			break;
		case USER_PROP_TLAST: 
			val=p->user.tlast;
			break;
		case USER_PROP_POSTS: 
			val=p->user.posts;
			break;
		case USER_PROP_EMAILS: 
			val=p->user.emails;
			break;
		case USER_PROP_FBACKS: 
			val=p->user.fbacks;
			break;
		case USER_PROP_ETODAY:	
			val=p->user.etoday;
			break;
		case USER_PROP_PTODAY:
			val=p->user.ptoday;
			break;
		case USER_PROP_ULB:
			val=p->user.ulb;
			break;
		case USER_PROP_ULS:
			val=p->user.uls;
			break;
		case USER_PROP_DLB:
			val=p->user.dlb;
			break;
		case USER_PROP_DLS:
			val=p->user.dls;
			break;
		case USER_PROP_CDT:
			val=p->user.cdt;
			break;
		case USER_PROP_MIN:
			val=p->user.min;
			break;
		case USER_PROP_LEVEL:
			val=p->user.level;
			break;
		case USER_PROP_FLAGS1:
			val=p->user.flags1;
			break;
		case USER_PROP_FLAGS2:
			val=p->user.flags2;
			break;
		case USER_PROP_FLAGS3:
			val=p->user.flags3;
			break;
		case USER_PROP_FLAGS4:
			val=p->user.flags4;
			break;
		case USER_PROP_EXEMPT:
			val=p->user.exempt;
			break;
		case USER_PROP_REST:
			val=p->user.rest;
			break;
		case USER_PROP_ROWS:
			val=p->user.rows;
			break;
		case USER_PROP_SEX:
			sprintf(tmp,"%c",p->user.sex);
			s=tmp;
			break;
		case USER_PROP_MISC:
			val=p->user.misc;
			break;
		case USER_PROP_LEECH:
			val=p->user.leech;
			break;
		case USER_PROP_CURSUB:
			s=p->user.cursub;
			break;
		case USER_PROP_CURDIR:
			s=p->user.curdir;
			break;
		case USER_PROP_CURXTRN:
			s=p->user.curxtrn;
			break;

		case USER_PROP_FREECDT:
			val=p->user.freecdt;
			break;
		case USER_PROP_XEDIT:
			if(p->user.xedit>0 && p->user.xedit<=p->cfg->total_xedits)
				s=p->cfg->xedit[p->user.xedit-1]->code;
			else
				s=""; /* internal editor */
			break;
		case USER_PROP_SHELL:
			s=p->cfg->shell[p->user.shell]->code;
			break;
		case USER_PROP_QWK:
			val=p->user.qwk;
			break;
		case USER_PROP_TMPEXT:
			s=p->user.tmpext;
			break;
		case USER_PROP_CHAT:
			val=p->user.chat;
			break;
		case USER_PROP_NS_TIME:
			val=p->user.ns_time;
			break;
		case USER_PROP_PROT:
			sprintf(tmp,"%c",p->user.prot);
			s=tmp;
			break;
		case USER_PROP_LOGONTIME:
			val=p->user.logontime;
			break;
		case USER_PROP_TIMEPERCALL:
			val=p->cfg->level_timepercall[p->user.level];
			break;
		case USER_PROP_TIMEPERDAY:
			val=p->cfg->level_timeperday[p->user.level];
			break;
		case USER_PROP_CALLSPERDAY:
			val=p->cfg->level_callsperday[p->user.level];
			break;
		case USER_PROP_LINESPERMSG:
			val=p->cfg->level_linespermsg[p->user.level];
			break;
		case USER_PROP_POSTSPERDAY:
			val=p->cfg->level_postsperday[p->user.level];
			break;
		case USER_PROP_EMAILPERDAY:
			val=p->cfg->level_emailperday[p->user.level];
			break;
		case USER_PROP_FREECDTPERDAY:
			val=p->cfg->level_freecdtperday[p->user.level];
			break;
		case USER_PROP_MAIL_WAITING:
			val=getmail(p->cfg,p->user.number,/* sent? */FALSE);
			break;
		case USER_PROP_MAIL_PENDING:
			val=getmail(p->cfg,p->user.number,/* sent? */TRUE);
			break;

		case USER_PROP_CACHED:
			*vp = BOOLEAN_TO_JSVAL(p->cached);
			JS_RESUMEREQUEST(cx, rc);
			return(JS_TRUE);	/* intentional early return */

		case USER_PROP_IS_SYSOP:
			*vp = BOOLEAN_TO_JSVAL(p->user.level >= SYSOP_LEVEL);
			JS_RESUMEREQUEST(cx, rc);
			return(JS_TRUE);	/* intentional early return */

		default:	
			/* This must not set vp in order for child objects to work (stats and security) */
			JS_RESUMEREQUEST(cx, rc);
			return(JS_TRUE);
	}
	JS_RESUMEREQUEST(cx, rc);
	if(s!=NULL) {
		if((js_str=JS_NewStringCopyZ(cx, s))==NULL)
			return(JS_FALSE);
		*vp = STRING_TO_JSVAL(js_str);
	} else
		JS_NewNumberValue(cx,val,vp);

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
	int32		usernumber;
	jsrefcount	rc;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if((js_str=JS_ValueToString(cx,*vp))==NULL)
		return(JS_FALSE);

	if((str=JS_GetStringBytes(js_str))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	rc=JS_SUSPENDREQUEST(cx);
	switch(tiny) {
		case USER_PROP_NUMBER:
			JS_RESUMEREQUEST(cx, rc);
			JS_ValueToInt32(cx, *vp, &usernumber);
			rc=JS_SUSPENDREQUEST(cx);
			if(usernumber!=p->user.number)
				p->user.number=(ushort)usernumber;
			break;
		case USER_PROP_ALIAS:
			/* update USER.DAT */
			putuserrec(p->cfg,p->user.number,U_ALIAS,LEN_ALIAS,str);

			/* update NAME.DAT */
			getuserrec(p->cfg,p->user.number,U_MISC,8,tmp);
			usermisc=ahtoul(tmp);
			if(!(usermisc&DELETED))
				putusername(p->cfg,p->user.number,str);
			break;
		case USER_PROP_NAME:
			putuserrec(p->cfg,p->user.number,U_NAME,LEN_NAME,str);
			break;
		case USER_PROP_HANDLE:
			putuserrec(p->cfg,p->user.number,U_HANDLE,LEN_HANDLE,str);
			break;
		case USER_PROP_NOTE:		 
			putuserrec(p->cfg,p->user.number,U_NOTE,LEN_NOTE,str);
			break;
		case USER_PROP_COMP:		 
			putuserrec(p->cfg,p->user.number,U_COMP,LEN_COMP,str);
			break;
		case USER_PROP_COMMENT:	 
			putuserrec(p->cfg,p->user.number,U_COMMENT,LEN_COMMENT,str);
			break;
		case USER_PROP_NETMAIL:	 
			putuserrec(p->cfg,p->user.number,U_NETMAIL,LEN_NETMAIL,str);
			break;
		case USER_PROP_ADDRESS:	 
			putuserrec(p->cfg,p->user.number,U_ADDRESS,LEN_ADDRESS,str);
			break;
		case USER_PROP_LOCATION:	 
			putuserrec(p->cfg,p->user.number,U_LOCATION,LEN_LOCATION,str);
			break;
		case USER_PROP_ZIPCODE:	 
			putuserrec(p->cfg,p->user.number,U_ZIPCODE,LEN_ZIPCODE,str);
			break;
		case USER_PROP_PHONE:  	 
			putuserrec(p->cfg,p->user.number,U_PHONE,LEN_PHONE,str);
			break;
		case USER_PROP_BIRTH:  	 
			putuserrec(p->cfg,p->user.number,U_BIRTH,LEN_BIRTH,str);
			break;
		case USER_PROP_MODEM:     
			putuserrec(p->cfg,p->user.number,U_MODEM,LEN_MODEM,str);
			break;
		case USER_PROP_ROWS:		 
			putuserrec(p->cfg,p->user.number,U_ROWS,0,str);	/* base 10 */
			break;
		case USER_PROP_SEX:		 
			putuserrec(p->cfg,p->user.number,U_SEX,0,strupr(str));	/* single char */
			break;
		case USER_PROP_CURSUB:	 
			putuserrec(p->cfg,p->user.number,U_CURSUB,0,str);
			break;
		case USER_PROP_CURDIR:	 
			putuserrec(p->cfg,p->user.number,U_CURDIR,0,str);
			break;
		case USER_PROP_CURXTRN:	 
			putuserrec(p->cfg,p->user.number,U_CURXTRN,0,str);
			break;
		case USER_PROP_XEDIT: 	 
			putuserrec(p->cfg,p->user.number,U_XEDIT,0,str);
			break;
		case USER_PROP_SHELL: 	 
			putuserrec(p->cfg,p->user.number,U_COMP,0,str);
			break;
		case USER_PROP_MISC:
			JS_RESUMEREQUEST(cx, rc);
			if(JS_ValueToInt32(cx,*vp,&val)) {
				rc=JS_SUSPENDREQUEST(cx);
				putuserrec(p->cfg,p->user.number,U_MISC,0,ultoa(val,tmp,16));
			}
			else
				rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_QWK:		 
			JS_RESUMEREQUEST(cx, rc);
			if(JS_ValueToInt32(cx,*vp,&val)) {
				rc=JS_SUSPENDREQUEST(cx);
				putuserrec(p->cfg,p->user.number,U_QWK,0,ultoa(val,tmp,16));
			}
			else
				rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_CHAT:		 
			JS_RESUMEREQUEST(cx, rc);
			if(JS_ValueToInt32(cx,*vp,&val)) {
				rc=JS_SUSPENDREQUEST(cx);
				putuserrec(p->cfg,p->user.number,U_CHAT,0,ultoa(val,tmp,16));
			}
			else
				rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_TMPEXT:	 
			putuserrec(p->cfg,p->user.number,U_TMPEXT,0,str);
			break;
		case USER_PROP_NS_TIME:	 
			JS_RESUMEREQUEST(cx, rc);
			if(JS_ValueToInt32(cx,*vp,&val)) {
				rc=JS_SUSPENDREQUEST(cx);
				putuserrec(p->cfg,p->user.number,U_NS_TIME,0,ultoa(val,tmp,16));
			}
			else
				rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_PROT:	
			putuserrec(p->cfg,p->user.number,U_PROT,0,strupr(str)); /* single char */
			break;
		case USER_PROP_LOGONTIME:	 
			JS_RESUMEREQUEST(cx, rc);
			if(JS_ValueToInt32(cx,*vp,&val)) {
				rc=JS_SUSPENDREQUEST(cx);
				putuserrec(p->cfg,p->user.number,U_LOGONTIME,0,ultoa(val,tmp,16));
			}
			else
				rc=JS_SUSPENDREQUEST(cx);
			break;
			
		/* security properties*/
		case USER_PROP_PASS:	
			putuserrec(p->cfg,p->user.number,U_PASS,LEN_PASS,strupr(str));
			break;
		case USER_PROP_PWMOD:
			JS_RESUMEREQUEST(cx, rc);
			if(JS_ValueToInt32(cx,*vp,&val)) {
				rc=JS_SUSPENDREQUEST(cx);
				putuserrec(p->cfg,p->user.number,U_PWMOD,0,ultoa(val,tmp,16));
			}
			else
				rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_LEVEL: 
			putuserrec(p->cfg,p->user.number,U_LEVEL,0,str);
			break;
		case USER_PROP_FLAGS1:
			JS_RESUMEREQUEST(cx, rc);
			if(JS_ValueToInt32(cx,*vp,&val)) {
				rc=JS_SUSPENDREQUEST(cx);
				putuserrec(p->cfg,p->user.number,U_FLAGS1,0,ultoa(val,tmp,16));
			}
			else
				rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_FLAGS2:
			JS_RESUMEREQUEST(cx, rc);
			if(JS_ValueToInt32(cx,*vp,&val)) {
				rc=JS_SUSPENDREQUEST(cx);
				putuserrec(p->cfg,p->user.number,U_FLAGS2,0,ultoa(val,tmp,16));
			}
			else
				rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_FLAGS3:
			JS_RESUMEREQUEST(cx, rc);
			if(JS_ValueToInt32(cx,*vp,&val)) {
				rc=JS_SUSPENDREQUEST(cx);
				putuserrec(p->cfg,p->user.number,U_FLAGS3,0,ultoa(val,tmp,16));
			}
			else
				rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_FLAGS4:
			JS_RESUMEREQUEST(cx, rc);
			if(JS_ValueToInt32(cx,*vp,&val)) {
				rc=JS_SUSPENDREQUEST(cx);
				putuserrec(p->cfg,p->user.number,U_FLAGS4,0,ultoa(val,tmp,16));
			}
			else
				rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_EXEMPT:
			JS_RESUMEREQUEST(cx, rc);
			if(JS_ValueToInt32(cx,*vp,&val)) {
				rc=JS_SUSPENDREQUEST(cx);
				putuserrec(p->cfg,p->user.number,U_EXEMPT,0,ultoa(val,tmp,16));
			}
			else
				rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_REST:	
			JS_RESUMEREQUEST(cx, rc);
			if(JS_ValueToInt32(cx,*vp,&val)) {
				rc=JS_SUSPENDREQUEST(cx);
				putuserrec(p->cfg,p->user.number,U_REST,0,ultoa(val,tmp,16));
			}
			else
				rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_CDT:	
			putuserrec(p->cfg,p->user.number,U_CDT,0,str);
			break;
		case USER_PROP_FREECDT:
			putuserrec(p->cfg,p->user.number,U_FREECDT,0,str);
			break;
		case USER_PROP_MIN:	
			putuserrec(p->cfg,p->user.number,U_MIN,0,str);
			break;
		case USER_PROP_TEXTRA:  
			putuserrec(p->cfg,p->user.number,U_TEXTRA,0,str);
			break;
		case USER_PROP_EXPIRE:  
			JS_RESUMEREQUEST(cx, rc);
			if(JS_ValueToInt32(cx,*vp,&val)) {
				rc=JS_SUSPENDREQUEST(cx);
				putuserrec(p->cfg,p->user.number,U_EXPIRE,0,ultoa(val,tmp,16));
			}
			else
				rc=JS_SUSPENDREQUEST(cx);
			break;

		case USER_PROP_CACHED:
			JS_ValueToBoolean(cx, *vp, &p->cached);
			JS_RESUMEREQUEST(cx, rc);
			return(JS_TRUE);	/* intentional early return */

	}
	p->cached=FALSE;

	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

#define USER_PROP_FLAGS JSPROP_ENUMERATE

static jsSyncPropertySpec js_user_properties[] = {
/*		 name				,tinyid					,flags,					ver	*/

	{	"number"			,USER_PROP_NUMBER		,USER_PROP_FLAGS,		310},
	{	"alias"				,USER_PROP_ALIAS 		,USER_PROP_FLAGS,		310},
	{	"name"				,USER_PROP_NAME		 	,USER_PROP_FLAGS,		310},
	{	"handle"			,USER_PROP_HANDLE	 	,USER_PROP_FLAGS,		310},
	{	"ip_address"		,USER_PROP_NOTE		 	,USER_PROP_FLAGS,		310},
	{	"note"				,USER_PROP_NOTE		 	,USER_PROP_FLAGS,		310},
	{	"host_name"			,USER_PROP_COMP		 	,USER_PROP_FLAGS,		310},
	{	"computer"			,USER_PROP_COMP		 	,USER_PROP_FLAGS,		310},
	{	"comment"			,USER_PROP_COMMENT	 	,USER_PROP_FLAGS,		310},
	{	"netmail"			,USER_PROP_NETMAIL	 	,USER_PROP_FLAGS,		310},
	{	"email"				,USER_PROP_EMAIL	 	,USER_PROP_FLAGS|JSPROP_READONLY,		310},
	{	"address"			,USER_PROP_ADDRESS	 	,USER_PROP_FLAGS,		310},
	{	"location"			,USER_PROP_LOCATION	 	,USER_PROP_FLAGS,		310},
	{	"zipcode"			,USER_PROP_ZIPCODE	 	,USER_PROP_FLAGS,		310},
	{	"phone"				,USER_PROP_PHONE  	 	,USER_PROP_FLAGS,		310},
	{	"birthdate"			,USER_PROP_BIRTH  	 	,USER_PROP_FLAGS,		310},
	{	"age"				,USER_PROP_AGE			,USER_PROP_FLAGS|JSPROP_READONLY,		310},
	{	"connection"		,USER_PROP_MODEM      	,USER_PROP_FLAGS,		310},
	{	"modem"				,USER_PROP_MODEM      	,USER_PROP_FLAGS,		310},
	{	"screen_rows"		,USER_PROP_ROWS		 	,USER_PROP_FLAGS,		310},
	{	"gender"			,USER_PROP_SEX		 	,USER_PROP_FLAGS,		310},
	{	"cursub"			,USER_PROP_CURSUB	 	,USER_PROP_FLAGS,		310},
	{	"curdir"			,USER_PROP_CURDIR	 	,USER_PROP_FLAGS,		310},
	{	"curxtrn"			,USER_PROP_CURXTRN	 	,USER_PROP_FLAGS,		310},
	{	"editor"			,USER_PROP_XEDIT 	 	,USER_PROP_FLAGS,		310},
	{	"command_shell"		,USER_PROP_SHELL 	 	,USER_PROP_FLAGS,		310},
	{	"settings"			,USER_PROP_MISC		 	,USER_PROP_FLAGS,		310},
	{	"qwk_settings"		,USER_PROP_QWK		 	,USER_PROP_FLAGS,		310},
	{	"chat_settings"		,USER_PROP_CHAT		 	,USER_PROP_FLAGS,		310},
	{	"temp_file_ext"		,USER_PROP_TMPEXT	 	,USER_PROP_FLAGS,		310},
	{	"new_file_time"		,USER_PROP_NS_TIME	 	,USER_PROP_FLAGS,		311},
	{	"newscan_date"		,USER_PROP_NS_TIME	 	,0, /* Alias */			310},
	{	"download_protocol"	,USER_PROP_PROT		 	,USER_PROP_FLAGS,		310},
	{	"logontime"			,USER_PROP_LOGONTIME 	,USER_PROP_FLAGS,		310},
	{	"cached"			,USER_PROP_CACHED		,USER_PROP_FLAGS,		314},
	{	"is_sysop"			,USER_PROP_IS_SYSOP		,JSPROP_ENUMERATE|JSPROP_READONLY,	315},
	{0}
};

#ifdef BUILD_JSDOCS
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
	,"current/last message sub-board (internal code)"
	,"current/last file directory (internal code)"
	,"current/last external program (internal code) run"
	,"external message editor (internal code) or <i>blank</i> if none"
	,"command shell (internal code)"
	,"settings bitfield - see <tt>USER_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions"
	,"QWK packet settings bitfield - see <tt>QWK_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions"
	,"chat settings bitfield - see <tt>CHAT_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions"
	,"temporary file type (extension)"
	,"new file scan date/time (time_t format)"
	,"file transfer protocol (command key)"
	,"logon time (time_t format)"
	,"record is currently cached in memory"
	,"user has a System Operator's security level"
	,NULL
};
#endif


/* user.security */
static jsSyncPropertySpec js_user_security_properties[] = {
/*		 name				,tinyid					,flags,				ver	*/

	{	"password"			,USER_PROP_PASS		 	,USER_PROP_FLAGS,	310 },
	{	"password_date"		,USER_PROP_PWMOD      	,USER_PROP_FLAGS,	310 },
	{	"level"				,USER_PROP_LEVEL 	 	,USER_PROP_FLAGS,	310 },
	{	"flags1"			,USER_PROP_FLAGS1	 	,USER_PROP_FLAGS,	310 },
	{	"flags2"			,USER_PROP_FLAGS2	 	,USER_PROP_FLAGS,	310 },
	{	"flags3"			,USER_PROP_FLAGS3	 	,USER_PROP_FLAGS,	310 },
	{	"flags4"			,USER_PROP_FLAGS4	 	,USER_PROP_FLAGS,	310 },
	{	"exemptions"		,USER_PROP_EXEMPT	 	,USER_PROP_FLAGS,	310 },
	{	"restrictions"		,USER_PROP_REST		 	,USER_PROP_FLAGS,	310 },
	{	"credits"			,USER_PROP_CDT		 	,USER_PROP_FLAGS,	310 },
	{	"free_credits"		,USER_PROP_FREECDT	 	,USER_PROP_FLAGS,	310 },
	{	"minutes"			,USER_PROP_MIN		 	,USER_PROP_FLAGS,	310 },
	{	"extra_time"		,USER_PROP_TEXTRA  	 	,USER_PROP_FLAGS,	310 },
	{	"expiration_date"	,USER_PROP_EXPIRE     	,USER_PROP_FLAGS,	310 },
	{0}
};

#ifdef BUILD_JSDOCS
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

/* user.limits: These should be READ ONLY by nature */
static jsSyncPropertySpec js_user_limits_properties[] = {
/*		 name					,tinyid					,flags,				ver	*/

	{	"time_per_logon"		,USER_PROP_TIMEPERCALL	,USER_PROP_FLAGS,	311 },
	{	"time_per_day"			,USER_PROP_TIMEPERDAY	,USER_PROP_FLAGS,	311 },
	{	"logons_per_day"		,USER_PROP_CALLSPERDAY	,USER_PROP_FLAGS,	311 },
	{	"lines_per_message"		,USER_PROP_LINESPERMSG	,USER_PROP_FLAGS,	311 },
	{	"email_per_day"			,USER_PROP_EMAILPERDAY	,USER_PROP_FLAGS,	311 },
	{	"posts_per_day"			,USER_PROP_POSTSPERDAY	,USER_PROP_FLAGS,	311 },
	{	"free_credits_per_day"	,USER_PROP_FREECDTPERDAY,USER_PROP_FLAGS,	311 },
	{0}
};


#ifdef BUILD_JSDOCS
static char* user_limits_prop_desc[] = {

	 "time (in minutes) per logon"
	,"time (in minutes) per day"
	,"logons per day"
	,"lines per message (post or email)"
	,"email sent per day"
	,"messages posted per day"
	,"free credits given per day"
	,NULL
};
#endif


#undef  USER_PROP_FLAGS
#define USER_PROP_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

/* user.stats: These should be READ ONLY by nature */
static jsSyncPropertySpec js_user_stats_properties[] = {
/*		 name				,tinyid					,flags,					ver	*/

	{	"laston_date"		,USER_PROP_LASTON	 	,USER_PROP_FLAGS,		310 },
	{	"firston_date"		,USER_PROP_FIRSTON	 	,USER_PROP_FLAGS,		310 },
	{	"total_logons"		,USER_PROP_LOGONS     	,USER_PROP_FLAGS,		310 },
	{	"logons_today"		,USER_PROP_LTODAY     	,USER_PROP_FLAGS,		310 },
	{	"total_timeon"		,USER_PROP_TIMEON     	,USER_PROP_FLAGS,		310 },
	{	"timeon_today"		,USER_PROP_TTODAY     	,USER_PROP_FLAGS,		310 },
	{	"timeon_last_logon"	,USER_PROP_TLAST      	,USER_PROP_FLAGS,		310 },
	{	"total_posts"		,USER_PROP_POSTS      	,USER_PROP_FLAGS,		310 },
	{	"total_emails"		,USER_PROP_EMAILS     	,USER_PROP_FLAGS,		310 },
	{	"total_feedbacks"	,USER_PROP_FBACKS     	,USER_PROP_FLAGS,		310 },
	{	"email_today"		,USER_PROP_ETODAY	 	,USER_PROP_FLAGS,		310 },
	{	"posts_today"		,USER_PROP_PTODAY	 	,USER_PROP_FLAGS,		310 },
	{	"bytes_uploaded"	,USER_PROP_ULB        	,USER_PROP_FLAGS,		310 },
	{	"files_uploaded"	,USER_PROP_ULS        	,USER_PROP_FLAGS,		310 },
	{	"bytes_downloaded"	,USER_PROP_DLB        	,USER_PROP_FLAGS,		310 },
	{	"files_downloaded"	,USER_PROP_DLS        	,USER_PROP_FLAGS,		310 },
	{	"leech_attempts"	,USER_PROP_LEECH 	 	,USER_PROP_FLAGS,		310 },
	{	"mail_waiting"		,USER_PROP_MAIL_WAITING	,USER_PROP_FLAGS,		312	},
	{	"mail_pending"		,USER_PROP_MAIL_PENDING	,USER_PROP_FLAGS,		312	},
	{0}
};

#ifdef BUILD_JSDOCS
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
	,"number of e-mail messages currently waiting"
	,"number of e-mail messages sent, currently pending deletion"
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
	private_t*	p;
	jsrefcount	rc;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_FALSE;

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL)
		return JS_FALSE;

	rc=JS_SUSPENDREQUEST(cx);
	ar = arstr(NULL,JS_GetStringBytes(js_str),p->cfg);

	js_getuserdat(p);

	*rval = BOOLEAN_TO_JSVAL(chk_ar(p->cfg,ar,&p->user));

	if(ar!=NULL && ar!=nular)
		free(ar);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_posted_msg(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	private_t*	p;
	int32	count=1;
	jsrefcount	rc;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_FALSE;

	if(argc)
		JS_ValueToInt32(cx, argv[0], &count);

	rc=JS_SUSPENDREQUEST(cx);
	js_getuserdat(p);

	*rval = BOOLEAN_TO_JSVAL(user_posted_msg(p->cfg, &p->user, count));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_sent_email(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	private_t*	p;
	int32	count=1;
	BOOL	feedback=FALSE;
	jsrefcount	rc;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_FALSE;

	if(argc)
		JS_ValueToInt32(cx, argv[0], &count);
	if(argc>1)
		JS_ValueToBoolean(cx, argv[1], &feedback);

	rc=JS_SUSPENDREQUEST(cx);
	js_getuserdat(p);

	*rval = BOOLEAN_TO_JSVAL(user_sent_email(p->cfg, &p->user, count, feedback));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_downloaded_file(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	private_t*	p;
	int32	files=1;
	int32	bytes=0;
	jsrefcount	rc;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_FALSE;

	if(argc)
		JS_ValueToInt32(cx, argv[0], &bytes);
	if(argc>1)
		JS_ValueToInt32(cx, argv[1], &files);

	rc=JS_SUSPENDREQUEST(cx);
	js_getuserdat(p);

	*rval = BOOLEAN_TO_JSVAL(user_downloaded(p->cfg, &p->user, files, bytes));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_uploaded_file(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	private_t*	p;
	int32	files=1;
	int32	bytes=0;
	jsrefcount	rc;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_FALSE;

	if(argc)
		JS_ValueToInt32(cx, argv[0], &bytes);
	if(argc>1)
		JS_ValueToInt32(cx, argv[1], &files);

	rc=JS_SUSPENDREQUEST(cx);
	js_getuserdat(p);

	*rval = BOOLEAN_TO_JSVAL(user_uploaded(p->cfg, &p->user, files, bytes));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_adjust_credits(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	private_t*	p;
	int32	count=0;
	jsrefcount	rc;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_FALSE;

	if(argc)
		JS_ValueToInt32(cx, argv[0], &count);

	rc=JS_SUSPENDREQUEST(cx);
	js_getuserdat(p);

	*rval = BOOLEAN_TO_JSVAL(user_adjust_credits(p->cfg, &p->user, count));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_adjust_minutes(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	private_t*	p;
	int32	count=0;
	jsrefcount	rc;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_FALSE;

	if(argc)
		JS_ValueToInt32(cx, argv[0], &count);

	rc=JS_SUSPENDREQUEST(cx);
	js_getuserdat(p);

	*rval = BOOLEAN_TO_JSVAL(user_adjust_minutes(p->cfg, &p->user, count));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_get_time_left(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	private_t*	p;
	int32	start_time=0;
	jsrefcount	rc;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_FALSE;

	if(argc)
		JS_ValueToInt32(cx, argv[0], &start_time);

	rc=JS_SUSPENDREQUEST(cx);
	js_getuserdat(p);

	*rval = INT_TO_JSVAL(gettimeleft(p->cfg, &p->user, (time_t)start_time));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}


static jsSyncMethodSpec js_user_functions[] = {
	{"compare_ars",		js_chk_ar,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("string ars")
	,JSDOCSTR("Verify user meets access requirements string")
	,310
	},		
	{"adjust_credits",	js_adjust_credits,	1,	JSTYPE_BOOLEAN,	JSDOCSTR("count")
	,JSDOCSTR("Adjust user's credits by <i>count</i> (negative to subtract)")
	,314
	},		
	{"adjust_minutes",	js_adjust_minutes,	1,	JSTYPE_BOOLEAN,	JSDOCSTR("count")
	,JSDOCSTR("Adjust user's extra minutes <i>count</i> (negative to subtract)")
	,314
	},		
	{"posted_message",	js_posted_msg,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("[count]")
	,JSDOCSTR("Adjust user's posted-messages statistics by <i>count</i> (default: 1) (negative to subtract)")
	,314
	},		
	{"sent_email",		js_sent_email,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("[count] [,bool feedback]")
	,JSDOCSTR("Adjust user's email/feedback-sent statistics by <i>count</i> (default: 1) (negative to subtract)")
	,314
	},		
	{"uploaded_file",	js_uploaded_file,	1,	JSTYPE_BOOLEAN,	JSDOCSTR("[bytes] [,files]")
	,JSDOCSTR("Adjust user's files/bytes-uploaded statistics")
	,314
	},		
	{"downloaded_file",	js_downloaded_file,	1,	JSTYPE_BOOLEAN,	JSDOCSTR("[bytes] [,files]")
	,JSDOCSTR("Adjust user's files/bytes-downloaded statistics")
	,314
	},
	{"get_time_left",	js_get_time_left,	1,	JSTYPE_NUMBER,	JSDOCSTR("start_time")
	,JSDOCSTR("Returns the user's available remaining time online, in seconds,<br>"
	"based on the passed <i>start_time</i> value (in time_t format)<br>"
	"Note: this method does not account for pending forced timed events")
	,31401
	},
	{0}
};

static JSBool js_user_stats_resolve(JSContext *cx, JSObject *obj, jsval id)
{
	char*			name=NULL;

	if(id != JSVAL_NULL)
		name=JS_GetStringBytes(JSVAL_TO_STRING(id));

	return(js_SyncResolve(cx, obj, name, js_user_stats_properties, NULL, NULL, 0));
}

static JSBool js_user_stats_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_user_stats_resolve(cx, obj, JSVAL_NULL));
}

static JSBool js_user_security_resolve(JSContext *cx, JSObject *obj, jsval id)
{
	char*			name=NULL;

	if(id != JSVAL_NULL)
		name=JS_GetStringBytes(JSVAL_TO_STRING(id));

	return(js_SyncResolve(cx, obj, name, js_user_security_properties, NULL, NULL, 0));
}

static JSBool js_user_security_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_user_security_resolve(cx, obj, JSVAL_NULL));
}

static JSBool js_user_limits_resolve(JSContext *cx, JSObject *obj, jsval id)
{
	char*			name=NULL;

	if(id != JSVAL_NULL)
		name=JS_GetStringBytes(JSVAL_TO_STRING(id));

	return(js_SyncResolve(cx, obj, name, js_user_limits_properties, NULL, NULL, 0));
}

static JSBool js_user_limits_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_user_limits_resolve(cx, obj, JSVAL_NULL));
}

static JSClass js_user_stats_class = {
     "UserStats"			/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_user_get			/* getProperty	*/
	,js_user_set			/* setProperty	*/
	,js_user_stats_enumerate		/* enumerate	*/
	,js_user_stats_resolve	/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub        /* finalize		*/
};

static JSClass js_user_security_class = {
     "UserSecurity"			/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_user_get			/* getProperty	*/
	,js_user_set			/* setProperty	*/
	,js_user_security_enumerate		/* enumerate	*/
	,js_user_security_resolve		/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub        /* finalize		*/
};

static JSClass js_user_limits_class = {
     "UserLimits"			/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_user_get			/* getProperty	*/
	,js_user_set			/* setProperty	*/
	,js_user_limits_enumerate		/* enumerate	*/
	,js_user_limits_resolve			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub        /* finalize		*/
};

static JSBool js_user_resolve(JSContext *cx, JSObject *obj, jsval id)
{
	char*			name=NULL;
	JSObject*		newobj;
	private_t*		p;

	if(id != JSVAL_NULL)
		name=JS_GetStringBytes(JSVAL_TO_STRING(id));

	if(name==NULL || strcmp(name, "stats")==0) {
		if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
			JS_ReportError(cx,getprivate_failure,WHERE);
			return(JS_FALSE);
		}
		/* user.stats */
		if((newobj=JS_DefineObject(cx, obj, "stats"
			,&js_user_stats_class, NULL, JSPROP_ENUMERATE|JSPROP_READONLY))==NULL) 
			return(JS_FALSE);
		JS_SetPrivate(cx, newobj, p);
#ifdef BUILD_JSDOCS
		js_DescribeSyncObject(cx,newobj,"User statistics (all <small>READ ONLY</small>)",310);
		js_CreateArrayOfStrings(cx, newobj, "_property_desc_list", user_stats_prop_desc, JSPROP_READONLY);
#endif
		if(name)
			return(JS_TRUE);

	}

	if(name==NULL || strcmp(name, "security")==0) {
		if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
			JS_ReportError(cx,getprivate_failure,WHERE);
			return(JS_FALSE);
		}
		/* user.security */
		if((newobj=JS_DefineObject(cx, obj, "security"
			,&js_user_security_class, NULL, JSPROP_ENUMERATE|JSPROP_READONLY))==NULL) 
			return(JS_FALSE);
		JS_SetPrivate(cx, newobj, p);
#ifdef BUILD_JSDOCS
		js_DescribeSyncObject(cx,newobj,"User limitations based on security level (all <small>READ ONLY</small>)",311);
		js_CreateArrayOfStrings(cx, newobj, "_property_desc_list", user_limits_prop_desc, JSPROP_READONLY);
#endif
		if(name)
			return(JS_TRUE);
	}

	if(name==NULL || strcmp(name, "limits")==0) {
		if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
			JS_ReportError(cx,getprivate_failure,WHERE);
			return(JS_FALSE);
		}
		/* user.limits */
		if((newobj=JS_DefineObject(cx, obj, "limits"
			,&js_user_limits_class, NULL, JSPROP_ENUMERATE|JSPROP_READONLY))==NULL) 
			return(JS_FALSE);
		JS_SetPrivate(cx, newobj, p);
#ifdef BUILD_JSDOCS
		js_DescribeSyncObject(cx,newobj,"User security settings",310);
		js_CreateArrayOfStrings(cx, newobj, "_property_desc_list", user_security_prop_desc, JSPROP_READONLY);
#endif
		if(name)
			return(JS_TRUE);
	}

	return(js_SyncResolve(cx, obj, name, js_user_properties, js_user_functions, NULL, 0));
}

static JSBool js_user_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_user_resolve(cx, obj, JSVAL_NULL));
}

static JSClass js_user_class = {
     "User"					/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_user_get			/* getProperty	*/
	,js_user_set			/* setProperty	*/
	,js_user_enumerate		/* enumerate	*/
	,js_user_resolve		/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_user_finalize		/* finalize		*/
};

/* User Constructor (creates instance of user class) */

static JSBool
js_user_constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int			i;
	int32		val=0;
	user_t		user;
	private_t*	p;

	JS_ValueToInt32(cx,argv[0],&val);
	user.number=(ushort)val;
	if(user.number!=0 && (i=getuserdat(scfg,&user))!=0) {
		JS_ReportError(cx,"Error %d reading user number %d",i,val);
		return(JS_FALSE);
	}

	if((p=(private_t*)malloc(sizeof(private_t)))==NULL)
		return(JS_FALSE);

	p->cfg = scfg;
	p->user = user;
	p->cached = (user.number==0 ? FALSE : TRUE);

	JS_SetPrivate(cx, obj, p);

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
		,NULL /* props, defined in constructor */
		,NULL /* funcs, defined in constructor */
		,NULL,NULL);

	return(userclass);
}

JSObject* DLLCALL js_CreateUserObject(JSContext* cx, JSObject* parent, scfg_t* cfg, char* name
									  , uint usernumber)
{
	JSObject*	userobj;
	private_t*	p;
	jsval		val;

	if(name==NULL)
	    userobj = JS_NewObject(cx, &js_user_class, NULL, parent);
	else if(JS_GetProperty(cx,parent,name,&val) && val!=JSVAL_VOID)
		userobj = JSVAL_TO_OBJECT(val);	/* Return existing user object */
	else
		userobj = JS_DefineObject(cx, parent, name, &js_user_class
								, NULL, JSPROP_ENUMERATE|JSPROP_READONLY);
	if(userobj==NULL)
		return(NULL);

	if((p=JS_GetPrivate(cx, userobj)) == NULL)	/* Uses existing private pointer: Fix memory leak? */
		if((p=(private_t*)malloc(sizeof(private_t)))==NULL)
			return(NULL);

	p->cfg = cfg;
	p->user.number = usernumber;
	p->cached = FALSE;

	JS_SetPrivate(cx, userobj, p);	

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,userobj
		,"Instance of <i>User</i> class, representing current user online"
		,310);
	js_DescribeSyncConstructor(cx,userobj
		,"To create a new user object: <tt>var u = new User(<i>number</i>)</tt>");
	js_CreateArrayOfStrings(cx, userobj
		,"_property_desc_list", user_prop_desc, JSPROP_READONLY);
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
