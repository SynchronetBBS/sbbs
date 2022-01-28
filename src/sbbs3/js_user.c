/* Synchronet JavaScript "User" Object */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "sbbs.h"
#include "js_request.h"

#ifdef JAVASCRIPT

typedef struct
{
	user_t*		user;
	user_t		storage;
	BOOL		cached;
	client_t*	client;
	int			file;		// for fast read operations, only

} private_t;

/* User Object Properties */
enum {
	 USER_PROP_NUMBER
	,USER_PROP_ALIAS 	
	,USER_PROP_NAME		
	,USER_PROP_HANDLE	
	,USER_PROP_NOTE		
	,USER_PROP_IPADDR
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
	,USER_PROP_BIRTHYEAR
	,USER_PROP_BIRTHMONTH
	,USER_PROP_BIRTHDAY
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
	,USER_PROP_READ_WAITING
	,USER_PROP_UNREAD_WAITING
	,USER_PROP_SPAM_WAITING
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
	,USER_PROP_COLS
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

static void js_getuserdat(scfg_t* scfg, private_t* p)
{
	if(p->user->number != 0 && !p->cached) {
		if(p->file < 1)
			p->file = openuserdat(scfg, /* for_modify: */FALSE);
		if(fgetuserdat(scfg, p->user, p->file)==0)
			p->cached=TRUE;
	}
}

static JSBool js_user_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval idval;
	char*		s=NULL;
	char		tmp[128];
	int64_t		val=0;
    jsint       tiny;
	JSString*	js_str;
	private_t*	p;
	jsrefcount	rc;
	scfg_t*			scfg;

	scfg=JS_GetRuntimePrivate(JS_GetRuntime(cx));

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_TRUE);

	rc=JS_SUSPENDREQUEST(cx);
	js_getuserdat(scfg,p);

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch(tiny) {
		case USER_PROP_NUMBER:
			val=p->user->number;
			break;
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
		case USER_PROP_IPADDR:
			s=p->user->ipaddr;
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
		case USER_PROP_EMAIL:
			s=usermailaddr(scfg, tmp
				,scfg->inetmail_misc&NMAIL_ALIAS ? p->user->alias : p->user->name);
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
		case USER_PROP_BIRTHYEAR:
			val = getbirthyear(p->user->birth);
			break;
		case USER_PROP_BIRTHMONTH:
			val = getbirthmonth(scfg, p->user->birth);
			break;
		case USER_PROP_BIRTHDAY:
			val = getbirthday(scfg, p->user->birth);
			break;
		case USER_PROP_AGE:
			val=getage(scfg,p->user->birth);
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
		case USER_PROP_COLS:
			val=p->user->cols;
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
		case USER_PROP_CURXTRN:
			s=p->user->curxtrn;
			break;

		case USER_PROP_FREECDT:
			val=p->user->freecdt;
			break;
		case USER_PROP_XEDIT:
			if(p->user->xedit>0 && p->user->xedit<=scfg->total_xedits)
				s=scfg->xedit[p->user->xedit-1]->code;
			else
				s=""; /* internal editor */
			break;
		case USER_PROP_SHELL:
			s=scfg->shell[p->user->shell]->code;
			break;
		case USER_PROP_QWK:
			val=p->user->qwk;
			break;
		case USER_PROP_TMPEXT:
			s=p->user->tmpext;
			break;
		case USER_PROP_CHAT:
			val=p->user->chat;
			break;
		case USER_PROP_NS_TIME:
			val=p->user->ns_time;
			break;
		case USER_PROP_PROT:
			sprintf(tmp,"%c",p->user->prot);
			s=tmp;
			break;
		case USER_PROP_LOGONTIME:
			val=p->user->logontime;
			break;
		case USER_PROP_TIMEPERCALL:
			val=scfg->level_timepercall[p->user->level];
			break;
		case USER_PROP_TIMEPERDAY:
			val=scfg->level_timeperday[p->user->level];
			break;
		case USER_PROP_CALLSPERDAY:
			val=scfg->level_callsperday[p->user->level];
			break;
		case USER_PROP_LINESPERMSG:
			val=scfg->level_linespermsg[p->user->level];
			break;
		case USER_PROP_POSTSPERDAY:
			val=scfg->level_postsperday[p->user->level];
			break;
		case USER_PROP_EMAILPERDAY:
			val=scfg->level_emailperday[p->user->level];
			break;
		case USER_PROP_FREECDTPERDAY:
			val=scfg->level_freecdtperday[p->user->level];
			break;
		case USER_PROP_MAIL_WAITING:
			val=getmail(scfg,p->user->number,/* sent? */FALSE, /* attr: */0);
			break;
		case USER_PROP_READ_WAITING:
			val=getmail(scfg,p->user->number,/* sent? */FALSE, /* attr: */MSG_READ);
			break;
		case USER_PROP_UNREAD_WAITING:
			val=getmail(scfg,p->user->number,/* sent? */FALSE, /* attr: */~MSG_READ);
			break;
		case USER_PROP_SPAM_WAITING:
			val=getmail(scfg,p->user->number,/* sent? */FALSE, /* attr: */MSG_SPAM);
			break;
		case USER_PROP_MAIL_PENDING:
			val=getmail(scfg,p->user->number,/* sent? */TRUE, /* SPAM: */FALSE);
			break;

		case USER_PROP_CACHED:
			*vp = BOOLEAN_TO_JSVAL(p->cached);
			JS_RESUMEREQUEST(cx, rc);
			return(JS_TRUE);	/* intentional early return */

		case USER_PROP_IS_SYSOP:
			*vp = BOOLEAN_TO_JSVAL(p->user->level >= SYSOP_LEVEL);
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
		*vp=DOUBLE_TO_JSVAL((double)val);

	return(JS_TRUE);
}

static JSBool js_user_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsval idval;
	char*		str = NULL;
	char		tmp[64];
	uint32		val;
	ulong		usermisc;
    jsint       tiny;
	private_t*	p;
	int32		usernumber;
	jsrefcount	rc;
	scfg_t*			scfg;

	scfg=JS_GetRuntimePrivate(JS_GetRuntime(cx));

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_TRUE);

	JSVALUE_TO_MSTRING(cx, *vp, str, NULL);
	HANDLE_PENDING(cx, str);
	if(str==NULL)
		return(JS_FALSE);

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	rc=JS_SUSPENDREQUEST(cx);
	switch(tiny) {
		case USER_PROP_NUMBER:
			JS_RESUMEREQUEST(cx, rc);
			if(!JS_ValueToInt32(cx, *vp, &usernumber)) {
				free(str);
				return JS_FALSE;
			}
			rc=JS_SUSPENDREQUEST(cx);
			if(usernumber!=p->user->number) {
				p->user->number=(ushort)usernumber;
				p->cached=FALSE;
			}
			break;
		case USER_PROP_ALIAS:
			SAFECOPY(p->user->alias,str);
			/* update USER.DAT */
			putuserrec(scfg,p->user->number,U_ALIAS,LEN_ALIAS,str);

			/* update NAME.DAT */
			getuserrec(scfg,p->user->number,U_MISC,8,tmp);
			usermisc=ahtoul(tmp);
			if(!(usermisc&DELETED))
				putusername(scfg,p->user->number,str);
			break;
		case USER_PROP_NAME:
			SAFECOPY(p->user->name,str);
			putuserrec(scfg,p->user->number,U_NAME,LEN_NAME,str);
			break;
		case USER_PROP_HANDLE:
			SAFECOPY(p->user->handle,str);
			putuserrec(scfg,p->user->number,U_HANDLE,LEN_HANDLE,str);
			break;
		case USER_PROP_NOTE:		 
			SAFECOPY(p->user->note,str);
			putuserrec(scfg,p->user->number,U_NOTE,LEN_NOTE,str);
			break;
		case USER_PROP_IPADDR:		 
			SAFECOPY(p->user->ipaddr,str);
			putuserrec(scfg,p->user->number,U_IPADDR,LEN_IPADDR,str);
			break;
		case USER_PROP_COMP:
			SAFECOPY(p->user->comp,str);
			putuserrec(scfg,p->user->number,U_COMP,LEN_COMP,str);
			break;
		case USER_PROP_COMMENT:	 
			SAFECOPY(p->user->comment,str);
			putuserrec(scfg,p->user->number,U_COMMENT,LEN_COMMENT,str);
			break;
		case USER_PROP_NETMAIL:	 
			SAFECOPY(p->user->netmail,str);
			putuserrec(scfg,p->user->number,U_NETMAIL,LEN_NETMAIL,str);
			break;
		case USER_PROP_ADDRESS:	 
			SAFECOPY(p->user->address,str);
			putuserrec(scfg,p->user->number,U_ADDRESS,LEN_ADDRESS,str);
			break;
		case USER_PROP_LOCATION:	 
			SAFECOPY(p->user->location,str);
			putuserrec(scfg,p->user->number,U_LOCATION,LEN_LOCATION,str);
			break;
		case USER_PROP_ZIPCODE:	 
			SAFECOPY(p->user->zipcode,str);
			putuserrec(scfg,p->user->number,U_ZIPCODE,LEN_ZIPCODE,str);
			break;
		case USER_PROP_PHONE:  	 
			SAFECOPY(p->user->phone,str);
			putuserrec(scfg,p->user->number,U_PHONE,LEN_PHONE,str);
			break;
		case USER_PROP_BIRTH:  	 
			SAFECOPY(p->user->birth,str);
			putuserrec(scfg,p->user->number,U_BIRTH,LEN_BIRTH,str);
			break;
		case USER_PROP_BIRTHYEAR:
			SAFEPRINTF(tmp, "%04u", atoi(str));
			putuserrec(scfg,p->user->number, U_BIRTH, 4, tmp);
			break;
		case USER_PROP_BIRTHMONTH:
			SAFEPRINTF(tmp, "%02u", atoi(str));
			putuserrec(scfg,p->user->number, U_BIRTH + 4, 2, tmp);
			break;
		case USER_PROP_BIRTHDAY:
			SAFEPRINTF(tmp, "%02u", atoi(str));
			putuserrec(scfg,p->user->number, U_BIRTH + 6, 2, tmp);
			break;
		case USER_PROP_MODEM:     
			SAFECOPY(p->user->modem,str);
			putuserrec(scfg,p->user->number,U_MODEM,LEN_MODEM,str);
			break;
		case USER_PROP_ROWS:	
			p->user->rows=atoi(str);
			putuserrec(scfg,p->user->number,U_ROWS,0,str);	/* base 10 */
			break;
		case USER_PROP_COLS:	
			p->user->cols=atoi(str);
			putuserrec(scfg,p->user->number,U_COLS,0,str);	/* base 10 */
			break;
		case USER_PROP_SEX:		 
			p->user->sex=toupper(str[0]);
			putuserrec(scfg,p->user->number,U_SEX,0,strupr(str));	/* single char */
			break;
		case USER_PROP_CURSUB:	 
			SAFECOPY(p->user->cursub,str);
			putuserrec(scfg,p->user->number,U_CURSUB,0,str);
			break;
		case USER_PROP_CURDIR:	 
			SAFECOPY(p->user->curdir,str);
			putuserrec(scfg,p->user->number,U_CURDIR,0,str);
			break;
		case USER_PROP_CURXTRN:	 
			SAFECOPY(p->user->curxtrn,str);
			putuserrec(scfg,p->user->number,U_CURXTRN,0,str);
			break;
		case USER_PROP_XEDIT: 	 
			putuserrec(scfg,p->user->number,U_XEDIT,0,str);
			break;
		case USER_PROP_SHELL: 	 
			putuserrec(scfg,p->user->number,U_SHELL,0,str);
			break;
		case USER_PROP_MISC:
			JS_RESUMEREQUEST(cx, rc);
			if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
				free(str);
				return JS_FALSE;
			}
			putuserrec(scfg,p->user->number,U_MISC,0,ultoa(p->user->misc=val,tmp,16));
			rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_QWK:		 
			JS_RESUMEREQUEST(cx, rc);
			if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
				free(str);
				return JS_FALSE;
			}
			putuserrec(scfg,p->user->number,U_QWK,0,ultoa(p->user->qwk=val,tmp,16));
			rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_CHAT:		 
			JS_RESUMEREQUEST(cx, rc);
			if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
				free(str);
				return JS_FALSE;
			}
			putuserrec(scfg,p->user->number,U_CHAT,0,ultoa(p->user->chat=val,tmp,16));
			rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_TMPEXT:	 
			SAFECOPY(p->user->tmpext,str);
			putuserrec(scfg,p->user->number,U_TMPEXT,0,str);
			break;
		case USER_PROP_NS_TIME:	 
			JS_RESUMEREQUEST(cx, rc);
			if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
				free(str);
				return JS_FALSE;
			}
			putuserrec(scfg,p->user->number,U_NS_TIME,0,ultoa((ulong)(p->user->ns_time=val),tmp,16));
			rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_PROT:	
			p->user->prot=toupper(str[0]);
			putuserrec(scfg,p->user->number,U_PROT,0,strupr(str)); /* single char */
			break;
		case USER_PROP_LOGONTIME:	 
			JS_RESUMEREQUEST(cx, rc);
			if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
				free(str);
				return JS_FALSE;
			}
			putuserrec(scfg,p->user->number,U_LOGONTIME,0,ultoa(p->user->logontime=val,tmp,16));
			rc=JS_SUSPENDREQUEST(cx);
			break;
			
		/* security properties*/
		case USER_PROP_PASS:	
			SAFECOPY(p->user->pass,str);
			putuserrec(scfg,p->user->number,U_PASS,LEN_PASS,strupr(str));
			break;
		case USER_PROP_PWMOD:
			JS_RESUMEREQUEST(cx, rc);
			if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
				free(str);
				return JS_FALSE;
			}
			putuserrec(scfg,p->user->number,U_PWMOD,0,ultoa(p->user->pwmod=val,tmp,16));
			rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_LEVEL: 
			p->user->level=atoi(str);
			putuserrec(scfg,p->user->number,U_LEVEL,0,str);
			break;
		case USER_PROP_FLAGS1:
			JS_RESUMEREQUEST(cx, rc);
			if(JSVAL_IS_STRING(*vp)) {
				val=str_to_bits(p->user->flags1 << 1, str) >> 1;
			}
			else {
				if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
					free(str);
					return JS_FALSE;
				}
			}
			putuserrec(scfg,p->user->number,U_FLAGS1,0,ultoa(p->user->flags1=val,tmp,16));
			rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_FLAGS2:
			JS_RESUMEREQUEST(cx, rc);
			if(JSVAL_IS_STRING(*vp)) {
				val=str_to_bits(p->user->flags2 << 1, str) >> 1;
			}
			else {
				if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
					free(str);
					return JS_FALSE;
				}
			}
			putuserrec(scfg,p->user->number,U_FLAGS2,0,ultoa(p->user->flags2=val,tmp,16));
			rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_FLAGS3:
			JS_RESUMEREQUEST(cx, rc);
			if(JSVAL_IS_STRING(*vp)) {
				val=str_to_bits(p->user->flags3 << 1, str) >> 1;
			}
			else {
				if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
					free(str);
					return JS_FALSE;
				}
			}
			putuserrec(scfg,p->user->number,U_FLAGS3,0,ultoa(p->user->flags3=val,tmp,16));
			rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_FLAGS4:
			JS_RESUMEREQUEST(cx, rc);
			if(JSVAL_IS_STRING(*vp)) {
				val=str_to_bits(p->user->flags4 << 1, str) >> 1;
			}
			else {
				if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
					free(str);
					return JS_FALSE;
				}
			}
			putuserrec(scfg,p->user->number,U_FLAGS4,0,ultoa(p->user->flags4=val,tmp,16));
			rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_EXEMPT:
			JS_RESUMEREQUEST(cx, rc);
			if(JSVAL_IS_STRING(*vp)) {
				val=str_to_bits(p->user->exempt << 1, str) >> 1;
			}
			else {
				if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
					free(str);
					return JS_FALSE;
				}
			}
			putuserrec(scfg,p->user->number,U_EXEMPT,0,ultoa(p->user->exempt=val,tmp,16));
			rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_REST:	
			JS_RESUMEREQUEST(cx, rc);
			if(JSVAL_IS_STRING(*vp)) {
				val=str_to_bits(p->user->rest << 1, str) >> 1;
			}
			else {
				if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
					free(str);
					return JS_FALSE;
				}
			}
			putuserrec(scfg,p->user->number,U_REST,0,ultoa(p->user->rest=val,tmp,16));
			rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_CDT:	
			p->user->cdt=strtoul(str,NULL,0);
			putuserrec(scfg,p->user->number,U_CDT,0,str);
			break;
		case USER_PROP_FREECDT:
			p->user->freecdt=strtoul(str,NULL,0);
			putuserrec(scfg,p->user->number,U_FREECDT,0,str);
			break;
		case USER_PROP_MIN:	
			p->user->min=strtoul(str,NULL,0);
			putuserrec(scfg,p->user->number,U_MIN,0,str);
			break;
		case USER_PROP_TEXTRA:  
			p->user->textra=(ushort)strtoul(str,NULL,0);
			putuserrec(scfg,p->user->number,U_TEXTRA,0,str);
			break;
		case USER_PROP_EXPIRE:  
			JS_RESUMEREQUEST(cx, rc);
			if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
				free(str);
				return JS_FALSE;
			}
			putuserrec(scfg,p->user->number,U_EXPIRE,0,ultoa(p->user->expire=val,tmp,16));
			rc=JS_SUSPENDREQUEST(cx);
			break;

		case USER_PROP_CACHED:
			JS_ValueToBoolean(cx, *vp, &p->cached);
			JS_RESUMEREQUEST(cx, rc);
			free(str);
			return(JS_TRUE);	/* intentional early return */

	}
	free(str);
	if(!(p->user->rest&FLAG('G')))
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
	{	"ip_address"		,USER_PROP_IPADDR	 	,USER_PROP_FLAGS,		310},
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
	{	"birthyear"			,USER_PROP_BIRTHYEAR   	,USER_PROP_FLAGS,		31802},
	{	"birthmonth"		,USER_PROP_BIRTHMONTH  	,USER_PROP_FLAGS,		31802},
	{	"birthday"			,USER_PROP_BIRTHDAY  	,USER_PROP_FLAGS,		31802},
	{	"age"				,USER_PROP_AGE			,USER_PROP_FLAGS|JSPROP_READONLY,		310},
	{	"connection"		,USER_PROP_MODEM      	,USER_PROP_FLAGS,		310},
	{	"modem"				,USER_PROP_MODEM      	,USER_PROP_FLAGS,		310},
	{	"screen_rows"		,USER_PROP_ROWS		 	,USER_PROP_FLAGS,		310},
	{	"screen_columns"	,USER_PROP_COLS		 	,USER_PROP_FLAGS,		31802},
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
	,"Sysop note (AKA ip_address on 3.16 and before)"
	,"host name last logged on from"
	,"AKA host_name"
	,"sysop's comment"
	,"external e-mail address"
	,"local Internet e-mail address	- <small>READ ONLY</small>"
	,"street address"
	,"location (e.g. city, state)"
	,"zip/postal code"
	,"phone number"
	,"birth date in 'YYYYMMDD' format or legacy format: 'MM/DD/YY' or 'DD/MM/YY', depending on system configuration"
	,"birth year"
	,"birth month (1-12)"
	,"birth day of month (1-31)"
	,"calculated age in years - <small>READ ONLY</small>"
	,"connection type (protocol)"
	,"AKA connection"
	,"terminal rows (0 = auto-detect)"
	,"terminal columns (0 = auto-detect)"
	,"gender type (e.g. M or F or any single-character)"
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
	,"flag set #1 (bitfield) can use +/-[A-?] notation"
	,"flag set #2 (bitfield) can use +/-[A-?] notation"
	,"flag set #3 (bitfield) can use +/-[A-?] notation"
	,"flag set #4 (bitfield) can use +/-[A-?] notation"
	,"exemption flags (bitfield) can use +/-[A-?] notation"
	,"restriction flags (bitfield) can use +/-[A-?] notation"
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
	{	"read_mail_waiting"	,USER_PROP_READ_WAITING	,USER_PROP_FLAGS,		31802 },
	{	"unread_mail_waiting",USER_PROP_UNREAD_WAITING,USER_PROP_FLAGS,		31802 },
	{	"spam_waiting"		,USER_PROP_SPAM_WAITING	,USER_PROP_FLAGS,		31802 },
	{	"mail_pending"		,USER_PROP_MAIL_PENDING	,USER_PROP_FLAGS,		312	},
	{0}
};

#ifdef BUILD_JSDOCS
static char* user_stats_prop_desc[] = {

	 "date of previous logon (time_t format)"
	,"date of first logon (time_t format)"
	,"total number of logons"
	,"total logons today"
	,"total time used (in minutes)"
	,"time used today (in minutes)"
	,"time used last session (in minutes)"
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
	,"total number of e-mail messages currently waiting in inbox"
	,"number of read e-mail messages currently waiting in inbox"
	,"number of unread e-mail messages currently waiting in inbox"
	,"number of SPAM e-mail messages currently waiting in inbox"
	,"number of e-mail messages sent, currently pending deletion"
	,NULL
};
#endif


static void js_user_finalize(JSContext *cx, JSObject *obj)
{
	private_t* p = (private_t*)JS_GetPrivate(cx,obj);

	if(p!=NULL) {
		if(p->file > 0)
			closeuserdat(p->file);
		free(p);
	}

	JS_SetPrivate(cx, obj, NULL);
}

extern JSClass js_user_class;

static JSBool
js_chk_ar(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	uchar*		ar;
	private_t*	p;
	jsrefcount	rc;
	char		*ars = NULL;
	scfg_t*		scfg;

	scfg=JS_GetRuntimePrivate(JS_GetRuntime(cx));

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_user_class))==NULL)
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx,argv[0], ars, NULL);
	HANDLE_PENDING(cx, ars);
	if(ars==NULL)
		return JS_FALSE;

	rc=JS_SUSPENDREQUEST(cx);
	ar = arstr(NULL,ars,scfg,NULL);
	free(ars);

	js_getuserdat(scfg,p);

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(chk_ar(scfg,ar,p->user,p->client)));

	if(ar!=NULL)
		free(ar);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_posted_msg(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	private_t*	p;
	uint32	count=1;
	jsrefcount	rc;
	scfg_t*		scfg;

	scfg=JS_GetRuntimePrivate(JS_GetRuntime(cx));

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_user_class))==NULL)
		return JS_FALSE;

	if(argc) {
		if(!JS_ValueToECMAUint32(cx, argv[0], &count))
			return JS_FALSE;
	}

	rc=JS_SUSPENDREQUEST(cx);
	js_getuserdat(scfg,p);

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(user_posted_msg(scfg, p->user, count)));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_sent_email(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	private_t*	p;
	uint32	count=1;
	BOOL	feedback=FALSE;
	jsrefcount	rc;
	scfg_t*		scfg;

	scfg=JS_GetRuntimePrivate(JS_GetRuntime(cx));

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_user_class))==NULL)
		return JS_FALSE;

	if(argc) {
		if(!JS_ValueToECMAUint32(cx, argv[0], &count))
			return JS_FALSE;
	}
	if(argc>1)
		JS_ValueToBoolean(cx, argv[1], &feedback);

	rc=JS_SUSPENDREQUEST(cx);
	js_getuserdat(scfg,p);

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(user_sent_email(scfg, p->user, count, feedback)));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_downloaded_file(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	private_t*	p;
	uint32	files=1;
	uint32	bytes=0;
	jsrefcount	rc;
	scfg_t*		scfg;
	uint dirnum=INVALID_DIR;
	char*	fname = NULL;

	scfg=JS_GetRuntimePrivate(JS_GetRuntime(cx));

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_user_class))==NULL)
		return JS_FALSE;

	uintN argn = 0;
	if(argc > argn && JSVAL_IS_STRING(argv[argn])) {
		char	*p;
		JSSTRING_TO_ASTRING(cx, JSVAL_TO_STRING(argv[argn]), p, LEN_EXTCODE+2, NULL);
		for(dirnum = 0; dirnum < scfg->total_dirs; dirnum++)
			if(!stricmp(scfg->dir[dirnum]->code,p))
				break;
		argn++;
	}
	if(argc > argn && JSVAL_IS_STRING(argv[argn])) {
		JSSTRING_TO_ASTRING(cx, JSVAL_TO_STRING(argv[argn]), fname, MAX_PATH + 1, NULL);
		argn++;
	}
	if(argc > argn && JSVAL_IS_NUMBER(argv[argn])) {
		if(!JS_ValueToECMAUint32(cx, argv[argn], &bytes))
			return JS_FALSE;
		argn++;
	}
	if(argc > argn && JSVAL_IS_NUMBER(argv[argn])) {
		if(!JS_ValueToECMAUint32(cx, argv[argn], &files))
			return JS_FALSE;
		argn++;
	}

	rc=JS_SUSPENDREQUEST(cx);
	js_getuserdat(scfg,p);
	if(fname != NULL && dirnum != INVALID_DIR && dirnum < scfg->total_dirs) {
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(user_downloaded_file(scfg, p->user, p->client, dirnum, fname, bytes)));
	} else {
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(user_downloaded(scfg, p->user, files, bytes)));
	}
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_uploaded_file(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	private_t*	p;
	uint32	files=1;
	uint32	bytes=0;
	jsrefcount	rc;
	scfg_t*		scfg;

	scfg=JS_GetRuntimePrivate(JS_GetRuntime(cx));

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_user_class))==NULL)
		return JS_FALSE;

	if(argc) {
		if(!JS_ValueToECMAUint32(cx, argv[0], &bytes))
			return JS_FALSE;
	}
	if(argc>1) {
		if(!JS_ValueToECMAUint32(cx, argv[1], &files))
			return JS_FALSE;
	}

	rc=JS_SUSPENDREQUEST(cx);
	js_getuserdat(scfg,p);

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(user_uploaded(scfg, p->user, files, bytes)));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_adjust_credits(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	private_t*	p;
	int32	count=0;
	jsrefcount	rc;
	scfg_t*		scfg;

	scfg=JS_GetRuntimePrivate(JS_GetRuntime(cx));

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_user_class))==NULL)
		return JS_FALSE;

	if(argc) {
		if(!JS_ValueToECMAInt32(cx, argv[0], &count))
			return JS_FALSE;
	}

	rc=JS_SUSPENDREQUEST(cx);
	js_getuserdat(scfg,p);

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(user_adjust_credits(scfg, p->user, count)));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_adjust_minutes(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	private_t*	p;
	int32	count=0;
	jsrefcount	rc;
	scfg_t*		scfg;

	scfg=JS_GetRuntimePrivate(JS_GetRuntime(cx));

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_user_class))==NULL)
		return JS_FALSE;

	if(argc) {
		if(!JS_ValueToECMAInt32(cx, argv[0], &count))	
			return JS_FALSE;
	}

	rc=JS_SUSPENDREQUEST(cx);
	js_getuserdat(scfg,p);

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(user_adjust_minutes(scfg, p->user, count)));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_get_time_left(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	private_t*	p;
	uint32	start_time=0;
	jsrefcount	rc;
	scfg_t*		scfg;
	time_t		tl;

	scfg=JS_GetRuntimePrivate(JS_GetRuntime(cx));

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_user_class))==NULL)
		return JS_FALSE;

	if(argc) {
		if(!JS_ValueToECMAUint32(cx, argv[0], &start_time))
			return JS_FALSE;
	}

	rc=JS_SUSPENDREQUEST(cx);
	js_getuserdat(scfg,p);

	tl = gettimeleft(scfg, p->user, start_time);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(tl > INT32_MAX ? INT32_MAX : (int32) tl));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}


static jsSyncMethodSpec js_user_functions[] = {
	{"compare_ars",		js_chk_ar,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("string ars")
	,JSDOCSTR("Verify user meets access requirements string<br>"
		"Note: For the current user of the terminal server, use <tt>bbs.compare_ars()</tt> instead.")
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
	{"downloaded_file",	js_downloaded_file,	1,	JSTYPE_BOOLEAN,	JSDOCSTR("[dir-code] [file path | name] [bytes] [,file-count]")
	,JSDOCSTR("Handle the full or partial successful download of a file.<br>"
		"Adjust user's files/bytes-downloaded statistics and credits, file's stats, system's stats, and uploader's stats and credits.")
	,31800
	},
	{"get_time_left",	js_get_time_left,	1,	JSTYPE_NUMBER,	JSDOCSTR("start_time")
	,JSDOCSTR("Returns the user's available remaining time online, in seconds,<br>"
	"based on the passed <i>start_time</i> value (in time_t format)<br>"
	"Note: this method does not account for pending forced timed events<br>"
	"Note: for the pre-defined user object on the BBS, you almost certainly want bbs.get_time_left() instead.")
	,31401
	},
	{0}
};

static JSBool js_user_stats_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*			name=NULL;
	JSBool			ret;

	if(id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		
		JS_IdToValue(cx, id, &idval);
		if(JSVAL_IS_STRING(idval)) {
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
			HANDLE_PENDING(cx, name);
		}
	}

	ret=js_SyncResolve(cx, obj, name, js_user_stats_properties, NULL, NULL, 0);
	if(name)
		free(name);
	return ret;
}

static JSBool js_user_stats_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_user_stats_resolve(cx, obj, JSID_VOID));
}

static JSBool js_user_security_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*			name=NULL;
	JSBool			ret;

	if(id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		
		JS_IdToValue(cx, id, &idval);
		if(JSVAL_IS_STRING(idval)) {
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
			HANDLE_PENDING(cx, name);
		}
	}

	ret=js_SyncResolve(cx, obj, name, js_user_security_properties, NULL, NULL, 0);
	if(name)
		free(name);
	return ret;
}

static JSBool js_user_security_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_user_security_resolve(cx, obj, JSID_VOID));
}

static JSBool js_user_limits_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*			name=NULL;
	JSBool			ret;

	if(id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		
		JS_IdToValue(cx, id, &idval);
		if(JSVAL_IS_STRING(idval)) {
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
			HANDLE_PENDING(cx, name);
		}
	}

	ret=js_SyncResolve(cx, obj, name, js_user_limits_properties, NULL, NULL, 0);
	if(name)
		free(name);
	return ret;
}

static JSBool js_user_limits_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_user_limits_resolve(cx, obj, JSID_VOID));
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

static JSBool js_user_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*			name=NULL;
	JSObject*		newobj;
	private_t*		p;
	JSBool			ret;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_TRUE);

	if(id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		
		JS_IdToValue(cx, id, &idval);
		if(JSVAL_IS_STRING(idval)) {
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
			HANDLE_PENDING(cx, name);
		}
	}

	if(name==NULL || strcmp(name, "stats")==0) {
		if(name)
			free(name);
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
		if(name)
			free(name);
		/* user.security */
		if((newobj=JS_DefineObject(cx, obj, "security"
			,&js_user_security_class, NULL, JSPROP_ENUMERATE|JSPROP_READONLY))==NULL) 
			return(JS_FALSE);
		JS_SetPrivate(cx, newobj, p);
#ifdef BUILD_JSDOCS
		js_DescribeSyncObject(cx,newobj,"User security settings",310);
		js_CreateArrayOfStrings(cx, newobj, "_property_desc_list", user_security_prop_desc, JSPROP_READONLY);
#endif
		if(name)
			return(JS_TRUE);
	}

	if(name==NULL || strcmp(name, "limits")==0) {
		if(name)
			free(name);
		/* user.limits */
		if((newobj=JS_DefineObject(cx, obj, "limits"
			,&js_user_limits_class, NULL, JSPROP_ENUMERATE|JSPROP_READONLY))==NULL) 
			return(JS_FALSE);
		JS_SetPrivate(cx, newobj, p);
#ifdef BUILD_JSDOCS
		js_DescribeSyncObject(cx,newobj,"User limitations based on security level (all <small>READ ONLY</small>)",311);
		js_CreateArrayOfStrings(cx, newobj, "_property_desc_list", user_limits_prop_desc, JSPROP_READONLY);
#endif
		if(name)
			return(JS_TRUE);
	}

	ret=js_SyncResolve(cx, obj, name, js_user_properties, js_user_functions, NULL, 0);
	if(name)
		free(name);
	return ret;
}

static JSBool js_user_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_user_resolve(cx, obj, JSID_VOID));
}

JSClass js_user_class = {
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
js_user_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj;
	jsval *argv=JS_ARGV(cx, arglist);
	int			i;
	int32		val=0;
	user_t		user;
	private_t*	p;
	scfg_t*			scfg;

	scfg=JS_GetRuntimePrivate(JS_GetRuntime(cx));

	obj=JS_NewObject(cx, &js_user_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));

	if(argc && (!JS_ValueToInt32(cx,argv[0],&val)))
		return JS_FALSE;
	memset(&user, 0, sizeof(user));
	user.number=(ushort)val;
	if(user.number!=0 && (i=getuserdat(scfg,&user))!=0) {
		JS_ReportError(cx,"Error %d reading user number %d",i,val);
		return(JS_FALSE);
	}

	if((p=(private_t*)malloc(sizeof(private_t)))==NULL)
		return(JS_FALSE);

	memset(p,0,sizeof(private_t));

	p->storage = user;
	p->user = &p->storage;
	p->cached = (user.number==0 ? FALSE : TRUE);

	JS_SetPrivate(cx, obj, p);

	return(JS_TRUE);
}

JSObject* js_CreateUserClass(JSContext* cx, JSObject* parent, scfg_t* cfg)
{
	JSObject*	userclass;

	userclass = JS_InitClass(cx, parent, NULL
		,&js_user_class
		,js_user_constructor
		,1	/* number of constructor args */
		,NULL /* props, defined in constructor */
		,NULL /* funcs, defined in constructor */
		,NULL,NULL);

	return(userclass);
}

JSObject* js_CreateUserObject(JSContext* cx, JSObject* parent, scfg_t* cfg, char* name
									  ,user_t* user, client_t* client, BOOL global_user)
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

	memset(p,0,sizeof(private_t));

	p->client = client;
	p->cached = FALSE;
	p->user = &p->storage;
	if(user!=NULL) {
		p->storage = *user;
		if(global_user)
			p->user = user;
		p->cached = TRUE;
	}

	JS_SetPrivate(cx, userobj, p);	

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,userobj
		,"Instance of <i>User</i> class, representing current user online"
		,310);
	js_DescribeSyncConstructor(cx,userobj
		,"To create a new user object: <tt>var u = new User;</tt> or: <tt>var u = new User(<i>number</i>);</tt>");
	js_CreateArrayOfStrings(cx, userobj
		,"_property_desc_list", user_prop_desc, JSPROP_READONLY);
#endif

	return(userobj);
}

/****************************************************************************/
/* Creates all the user-specific objects: user, msg_area, file_area			*/
/****************************************************************************/
JSBool
js_CreateUserObjects(JSContext* cx, JSObject* parent, scfg_t* cfg, user_t* user, client_t* client
					 ,const char* web_file_vpath_prefix, subscan_t* subscan)
{
	if(js_CreateUserObject(cx,parent,cfg,"user",user,client,/* global_user */TRUE)==NULL)
		return(JS_FALSE);
	if(js_CreateFileAreaObject(cx,parent,cfg,user,client,web_file_vpath_prefix)==NULL) 
		return(JS_FALSE);
	if(js_CreateMsgAreaObject(cx,parent,cfg,user,client,subscan)==NULL) 
		return(JS_FALSE);
	if(js_CreateXtrnAreaObject(cx,parent,cfg,user,client)==NULL)
		return(JS_FALSE);

	return(JS_TRUE);
}

#endif	/* JAVSCRIPT */
