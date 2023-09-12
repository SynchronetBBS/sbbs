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
	,USER_PROP_DLCPS
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
	,USER_PROP_MAIL
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
		ushort usernumber = p->user->number;
		if(fgetuserdat(scfg, p->user, p->file)==0)
			p->cached=TRUE;
		p->user->number = usernumber; // Can be zeroed by fgetuserdat() failure
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
				,(scfg->inetmail_misc&NMAIL_ALIAS) || (p->user->rest&FLAG('O')) ? p->user->alias : p->user->name);
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
			*vp = DOUBLE_TO_JSVAL((jsdouble)p->user->ulb);
			JS_RESUMEREQUEST(cx, rc);
			return JS_TRUE;	/* intentional early return */
			break;
		case USER_PROP_ULS:
			val=p->user->uls;
			break;
		case USER_PROP_DLB:
			*vp = DOUBLE_TO_JSVAL((jsdouble)p->user->dlb);
			JS_RESUMEREQUEST(cx, rc);
			return JS_TRUE;	/* intentional early return */
			break;
		case USER_PROP_DLS:
			val=p->user->dls;
			break;
		case USER_PROP_DLCPS:
			val=p->user->dlcps;
			break;
		case USER_PROP_CDT:
			*vp = DOUBLE_TO_JSVAL((jsdouble)p->user->cdt);
			JS_RESUMEREQUEST(cx, rc);
			return JS_TRUE;	/* intentional early return */
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
			*vp = DOUBLE_TO_JSVAL((jsdouble)p->user->freecdt);
			JS_RESUMEREQUEST(cx, rc);
			return JS_TRUE;	/* intentional early return */
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
		case USER_PROP_MAIL:
			val = p->user->mail;
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
			*vp = BOOLEAN_TO_JSVAL(is_user_sysop(p->user));
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
			/* update user.tab */
			putuserstr(scfg, p->user->number, USER_ALIAS, str);

			/* update name.dat */
			usermisc = getusermisc(scfg, p->user->number);
			if(!(usermisc&DELETED))
				putusername(scfg,p->user->number,str);
			break;
		case USER_PROP_NAME:
			SAFECOPY(p->user->name,str);
			putuserstr(scfg, p->user->number, USER_NAME, str);
			break;
		case USER_PROP_HANDLE:
			SAFECOPY(p->user->handle,str);
			putuserstr(scfg, p->user->number, USER_HANDLE, str);
			break;
		case USER_PROP_NOTE:
			SAFECOPY(p->user->note,str);
			putuserstr(scfg, p->user->number, USER_NOTE, str);
			break;
		case USER_PROP_IPADDR:
			SAFECOPY(p->user->ipaddr,str);
			putuserstr(scfg, p->user->number, USER_IPADDR, str);
			break;
		case USER_PROP_COMP:
			SAFECOPY(p->user->comp,str);
			putuserstr(scfg, p->user->number, USER_HOST, str);
			break;
		case USER_PROP_COMMENT:
			SAFECOPY(p->user->comment,str);
			putuserstr(scfg, p->user->number, USER_COMMENT, str);
			break;
		case USER_PROP_NETMAIL:
			SAFECOPY(p->user->netmail,str);
			putuserstr(scfg, p->user->number, USER_NETMAIL, str);
			break;
		case USER_PROP_ADDRESS:
			SAFECOPY(p->user->address,str);
			putuserstr(scfg, p->user->number, USER_ADDRESS, str);
			break;
		case USER_PROP_LOCATION:
			SAFECOPY(p->user->location,str);
			putuserstr(scfg, p->user->number, USER_LOCATION, str);
			break;
		case USER_PROP_ZIPCODE:
			SAFECOPY(p->user->zipcode,str);
			putuserstr(scfg, p->user->number, USER_ZIPCODE, str);
			break;
		case USER_PROP_PHONE:
			SAFECOPY(p->user->phone,str);
			putuserstr(scfg, p->user->number, USER_PHONE, str);
			break;
		case USER_PROP_BIRTH:
			SAFECOPY(p->user->birth,str);
			putuserstr(scfg, p->user->number, USER_BIRTH, str);
			break;
		case USER_PROP_BIRTHYEAR:
			if(JS_ValueToECMAUint32(cx, *vp, &val))
				putuserdec32(scfg, p->user->number, USER_BIRTH, isoDate_create(val, getbirthmonth(scfg, p->user->birth), getbirthday(scfg, p->user->birth)));
			break;
		case USER_PROP_BIRTHMONTH:
			if(JS_ValueToECMAUint32(cx, *vp, &val))
				putuserdec32(scfg, p->user->number, USER_BIRTH, isoDate_create(getbirthyear(p->user->birth), val, getbirthday(scfg, p->user->birth)));
			break;
		case USER_PROP_BIRTHDAY:
			if(JS_ValueToECMAUint32(cx, *vp, &val))
				putuserdec32(scfg, p->user->number, USER_BIRTH, isoDate_create(getbirthyear(p->user->birth), getbirthmonth(scfg, p->user->birth), val));
			break;
		case USER_PROP_MODEM:
			SAFECOPY(p->user->modem,str);
			putuserstr(scfg, p->user->number, USER_CONNECTION, str);
			break;
		case USER_PROP_ROWS:
			p->user->rows=atoi(str);
			putuserdec32(scfg, p->user->number, USER_ROWS, p->user->rows);
			break;
		case USER_PROP_COLS:
			p->user->cols=atoi(str);
			putuserdec32(scfg, p->user->number, USER_COLS, p->user->cols);
			break;
		case USER_PROP_SEX:
			p->user->sex=toupper(str[0]);
			putuserstr(scfg, p->user->number, USER_GENDER, strupr(str));	/* single char */
			break;
		case USER_PROP_CURSUB:
			SAFECOPY(p->user->cursub,str);
			putuserstr(scfg, p->user->number, USER_CURSUB, str);
			break;
		case USER_PROP_CURDIR:
			SAFECOPY(p->user->curdir,str);
			putuserstr(scfg, p->user->number, USER_CURDIR, str);
			break;
		case USER_PROP_CURXTRN:
			SAFECOPY(p->user->curxtrn,str);
			putuserstr(scfg, p->user->number, USER_CURXTRN, str);
			break;
		case USER_PROP_XEDIT:
			p->user->xedit = getxeditnum(scfg, str);
			putuserstr(scfg, p->user->number, USER_XEDIT, str);
			break;
		case USER_PROP_SHELL:
			p->user->shell = getshellnum(scfg, str);
			putuserstr(scfg, p->user->number, USER_SHELL, str);
			break;
		case USER_PROP_MISC:
			JS_RESUMEREQUEST(cx, rc);
			if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
				free(str);
				return JS_FALSE;
			}
			putusermisc(scfg, p->user->number, p->user->misc = val);
			rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_QWK:
			JS_RESUMEREQUEST(cx, rc);
			if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
				free(str);
				return JS_FALSE;
			}
			putuserqwk(scfg, p->user->number, p->user->qwk = val);
			rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_CHAT:
			JS_RESUMEREQUEST(cx, rc);
			if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
				free(str);
				return JS_FALSE;
			}
			putuserchat(scfg, p->user->number, p->user->chat = val);
			rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_MAIL:
			JS_RESUMEREQUEST(cx, rc);
			if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
				free(str);
				return JS_FALSE;
			}
			putusermail(scfg, p->user->number, p->user->mail = val);
			rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_TMPEXT:
			SAFECOPY(p->user->tmpext,str);
			putuserstr(scfg, p->user->number, USER_TMPEXT, str);
			break;
		case USER_PROP_NS_TIME:
			JS_RESUMEREQUEST(cx, rc);
			if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
				free(str);
				return JS_FALSE;
			}
			putuserdatetime(scfg, p->user->number, USER_NS_TIME, p->user->ns_time = val);
			rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_PROT:
			p->user->prot=toupper(str[0]);
			putuserstr(scfg, p->user->number, USER_PROT, strupr(str)); /* single char */
			break;
		case USER_PROP_LOGONTIME:
			JS_RESUMEREQUEST(cx, rc);
			if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
				free(str);
				return JS_FALSE;
			}
			putuserdatetime(scfg, p->user->number, USER_LOGONTIME, p->user->logontime = val);
			rc=JS_SUSPENDREQUEST(cx);
			break;

		/* security properties*/
		case USER_PROP_PASS:
			SAFECOPY(p->user->pass,str);
			putuserstr(scfg, p->user->number, USER_PASS, strupr(str));
			break;
		case USER_PROP_PWMOD:
			JS_RESUMEREQUEST(cx, rc);
			if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
				free(str);
				return JS_FALSE;
			}
			putuserdatetime(scfg, p->user->number, USER_PWMOD, p->user->pwmod = val);
			rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_LEVEL:
			p->user->level=atoi(str);
			putuserdec32(scfg, p->user->number, USER_LEVEL, p->user->level);
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
			putuserflags(scfg, p->user->number, USER_FLAGS1, p->user->flags1 = val);
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
			putuserflags(scfg, p->user->number, USER_FLAGS2, p->user->flags2 = val);
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
			putuserflags(scfg, p->user->number, USER_FLAGS3, p->user->flags3 = val);
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
			putuserflags(scfg, p->user->number, USER_FLAGS4, p->user->flags4 = val);
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
			putuserflags(scfg, p->user->number, USER_EXEMPT, p->user->exempt = val);
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
			putuserflags(scfg, p->user->number, USER_REST, p->user->rest = val);
			rc=JS_SUSPENDREQUEST(cx);
			break;
		case USER_PROP_CDT:
			p->user->cdt=strtoull(str,NULL,0);
			putuserdec64(scfg, p->user->number, USER_CDT, p->user->cdt);
			break;
		case USER_PROP_FREECDT:
			p->user->freecdt=strtoull(str,NULL,0);
			putuserdec64(scfg, p->user->number, USER_FREECDT, p->user->freecdt);
			break;
		case USER_PROP_MIN:
			p->user->min=strtoul(str,NULL,0);
			putuserdec32(scfg, p->user->number, USER_MIN, p->user->min);
			break;
		case USER_PROP_TEXTRA:
			p->user->textra=(ushort)strtoul(str,NULL,0);
			putuserdec32(scfg, p->user->number, USER_TEXTRA, p->user->textra);
			break;
		case USER_PROP_EXPIRE:
			JS_RESUMEREQUEST(cx, rc);
			if(!JS_ValueToECMAUint32(cx,*vp,&val)) {
				free(str);
				return JS_FALSE;
			}
			putuserdatetime(scfg,p->user->number, USER_EXPIRE, p->user->expire = val);
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
	{	"mail_settings"		,USER_PROP_MAIL 		,USER_PROP_FLAGS,		320},
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

	 "Record number (1-based)"
	,"Alias/name"
	,"Real name"
	,"Chat handle"
	,"IP address last logged on from"
	,"Sysop note (AKA ip_address on 3.16 and before)"
	,"Host name last logged on from"
	,"AKA host_name"
	,"Sysop's comment"
	,"External e-mail address"
	,"Local Internet e-mail address	- <small>READ ONLY</small>"
	,"Street address"
	,"Location (e.g. city, state)"
	,"Zip/postal code"
	,"Phone number"
	,"Birth date in 'YYYYMMDD' format or legacy format: 'MM/DD/YY' or 'DD/MM/YY', depending on system configuration"
	,"Birth year"
	,"Birth month (1-12)"
	,"Birth day of month (1-31)"
	,"Calculated age in years - <small>READ ONLY</small>"
	,"Connection type (protocol)"
	,"AKA connection"
	,"Terminal rows (0 = auto-detect)"
	,"Terminal columns (0 = auto-detect)"
	,"Gender type (e.g. M or F or any single-character)"
	,"Current/last message sub-board (internal code)"
	,"Current/last file directory (internal code)"
	,"Current/last external program (internal code) run"
	,"External message editor (internal code) or <i>blank</i> if none"
	,"Command shell (internal code)"
	,"Settings bit-flags - see <tt>USER_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions"
	,"QWK packet settings bit-flags - see <tt>QWK_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions"
	,"Chat settings bit-flags - see <tt>CHAT_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions"
	,"Mail settings bit-flags - see <tt>MAIL_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions"
	,"Temporary file type (extension)"
	,"New file scan date/time (time_t format)"
	,"File transfer protocol (command key)"
	,"Logon time (time_t format)"
	,"Record is currently cached in memory"
	,"User has a System Operator's security level"
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

	 "Password"
	,"Date password last modified (time_t format)"
	,"Security level (0-99)"
	,"Flag set #1 (bit-flags) can use +/-[A-?] notation"
	,"Flag set #2 (bit-flags) can use +/-[A-?] notation"
	,"Flag set #3 (bit-flags) can use +/-[A-?] notation"
	,"Flag set #4 (bit-flags) can use +/-[A-?] notation"
	,"Exemption flags (bit-flags) can use +/-[A-?] notation"
	,"Restriction flags (bit-flags) can use +/-[A-?] notation"
	,"Credits"
	,"Free credits (for today only)"
	,"Extra minutes (time bank)"
	,"Extra minutes (for today only)"
	,"Expiration date/time (time_t format)"
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

	 "Time (in minutes) per logon"
	,"Time (in minutes) per day"
	,"Logons per day"
	,"Lines per message (post or email)"
	,"Email sent per day"
	,"Messages posted per day"
	,"Free credits given per day"
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
	{	"download_cps"		,USER_PROP_DLCPS       	,USER_PROP_FLAGS,		320 },
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

	 "Date of previous logon (time_t format)"
	,"Date of first logon (time_t format)"
	,"Total number of logons"
	,"Total logons today"
	,"Total time used (in minutes)"
	,"Time used today (in minutes)"
	,"Time used last session (in minutes)"
	,"Total messages posted"
	,"Total e-mails sent"
	,"Total feedback messages sent"
	,"E-mail sent today"
	,"Messages posted today"
	,"Total bytes uploaded"
	,"Total files uploaded"
	,"Total bytes downloaded"
	,"Total files downloaded"
	,"Latest average download rate, in characters (bytes) per second"
	,"Suspected leech downloads"
	,"Total number of e-mail messages currently waiting in inbox"
	,"Number of read e-mail messages currently waiting in inbox"
	,"Number of unread e-mail messages currently waiting in inbox"
	,"Number of SPAM e-mail messages currently waiting in inbox"
	,"Number of e-mail messages sent, currently pending deletion"
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
	int dirnum=INVALID_DIR;
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
	if(fname != NULL && is_valid_dirnum(scfg, dirnum)) {
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

static JSBool
js_user_close(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	private_t*	p;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_user_class))==NULL)
		return JS_FALSE;

	rc=JS_SUSPENDREQUEST(cx);
	if(p->file > 0)
		closeuserdat(p->file);
	p->file = -1;
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
	{"close",			js_user_close,		0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("Close the <tt>user.tab</tt> file, if open. The file will be auto-reopened if necessary.")
	,31902
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

JSObject* js_CreateUserClass(JSContext* cx, JSObject* parent)
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

JSObject* js_CreateUserObject(JSContext* cx, JSObject* parent, char* name
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

	if((p=JS_GetPrivate(cx, userobj)) == NULL) {	/* Uses existing private pointer: Fix memory leak? */
		if((p=(private_t*)malloc(sizeof(private_t)))==NULL)
			return(NULL);
		memset(p,0,sizeof(private_t));
	}

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
	if(js_CreateUserObject(cx,parent,"user",user,client,/* global_user */TRUE)==NULL)
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
