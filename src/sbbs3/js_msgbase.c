/* js_msgbase.c */

/* Synchronet JavaScript "MsgBase" Object */

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

typedef struct
{
	smb_t	smb;
	BOOL	debug;

} private_t;

static const char* getprivate_failure = "line %d %s JS_GetPrivate failed";

/* Destructor */

static void js_finalize_msgbase(JSContext *cx, JSObject *obj)
{
	private_t* p;
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return;

	if(SMB_IS_OPEN(&(p->smb)))
		smb_close(&(p->smb));

	free(p);

	JS_SetPrivate(cx, obj, NULL);
}

/* Methods */

static JSBool
js_open(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	private_t* p;
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	*rval = JSVAL_FALSE;

	if(p->smb.subnum==INVALID_SUB 
		&& strchr(p->smb.file,'/')==NULL
		&& strchr(p->smb.file,'\\')==NULL) {
		JS_ReportError(cx,"Unrecognized msgbase code: %s",p->smb.file);
		return(JS_TRUE);
	}

	if(smb_open(&(p->smb))!=0)
		return(JS_TRUE);

	*rval = JSVAL_TRUE;
	return(JS_TRUE);
}


static JSBool
js_close(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	private_t* p;
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	smb_close(&(p->smb));

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSClass js_msghdr_class = {
     "MsgHeader"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,JS_PropertyStub		/* getProperty	*/
	,JS_PropertyStub		/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};

static BOOL parse_header_object(JSContext* cx, private_t* p, JSObject* hdr, smbmsg_t* msg)
{
	char*		cp;
	ushort		nettype;
	JSString*	js_str;
	jsval		val;

	/* Required Header Fields */
	if(JS_GetProperty(cx, hdr, "subject", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
	} else
		cp="";
	smb_hfield(msg, SUBJECT, (ushort)strlen(cp), cp);
	msg->idx.subj=subject_crc(cp);

	if(JS_GetProperty(cx, hdr, "to", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
	} else {
		if(p->smb.status.attr)	/* e-mail */
			return(FALSE);		/* "to" property required */
		cp="All";
	}
	smb_hfield(msg, RECIPIENT, (ushort)strlen(cp), cp);
	if(!(p->smb.status.attr&SMB_EMAIL)) {
		strlwr(cp);
		msg->idx.to=crc16(cp);
	}

	if(JS_GetProperty(cx, hdr, "from", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
	} else
		return(FALSE);	/* "from" property required */
	smb_hfield(msg, SENDER, (ushort)strlen(cp), cp);
	if(!(p->smb.status.attr&SMB_EMAIL)) {
		strlwr(cp);
		msg->idx.from=crc16(cp);
	}

	/* Optional Header Fields */
	if(JS_GetProperty(cx, hdr, "from_ext", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
		smb_hfield(msg, SENDEREXT, (ushort)strlen(cp), cp);
		if(p->smb.status.attr&SMB_EMAIL)
			msg->idx.from=atoi(cp);
	}

	if(JS_GetProperty(cx, hdr, "from_org", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
		smb_hfield(msg, SENDERORG, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "from_net_type", &val) && val!=JSVAL_VOID) {
		nettype=(ushort)JSVAL_TO_INT(val);
		smb_hfield(msg, SENDERNETTYPE, sizeof(nettype), &nettype);
		if(p->smb.status.attr&SMB_EMAIL && nettype!=NET_NONE)
			msg->idx.from=0;
	}

	if(JS_GetProperty(cx, hdr, "from_net_addr", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
		smb_hfield(msg, SENDERNETADDR, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "to_ext", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
		smb_hfield(msg, RECIPIENTEXT, (ushort)strlen(cp), cp);
		if(p->smb.status.attr&SMB_EMAIL)
			msg->idx.to=atoi(cp);
	}

	if(JS_GetProperty(cx, hdr, "to_net_type", &val) && val!=JSVAL_VOID) {
		nettype=(ushort)JSVAL_TO_INT(val);
		smb_hfield(msg, RECIPIENTNETTYPE, sizeof(nettype), &nettype);
		if(p->smb.status.attr&SMB_EMAIL && nettype!=NET_NONE)
			msg->idx.to=0;
	}

	if(JS_GetProperty(cx, hdr, "to_net_addr", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
		smb_hfield(msg, RECIPIENTNETADDR, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "replyto", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
		smb_hfield(msg, REPLYTO, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "replyto_ext", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
		smb_hfield(msg, REPLYTOEXT, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "replyto_net_type", &val) && val!=JSVAL_VOID) {
		nettype=(ushort)JSVAL_TO_INT(val);
		smb_hfield(msg, REPLYTONETTYPE, sizeof(nettype), &nettype);
	}

	if(JS_GetProperty(cx, hdr, "replyto_net_addr", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
		smb_hfield(msg, REPLYTONETADDR, (ushort)strlen(cp), cp);
	}

	/* RFC822 headers */
	if(JS_GetProperty(cx, hdr, "id", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
		smb_hfield(msg, RFC822MSGID, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "reply_id", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
		smb_hfield(msg, RFC822REPLYID, (ushort)strlen(cp), cp);
	}

	/* USENET headers */
	if(JS_GetProperty(cx, hdr, "path", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
		smb_hfield(msg, USENETPATH, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "newsgroups", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
		smb_hfield(msg, USENETNEWSGROUPS, (ushort)strlen(cp), cp);
	}

	/* FTN headers */
	if(JS_GetProperty(cx, hdr, "ftn_msgid", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
		smb_hfield(msg, FIDOMSGID, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "ftn_reply", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
		smb_hfield(msg, FIDOREPLYID, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "ftn_area", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
		smb_hfield(msg, FIDOAREA, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "ftn_flags", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
		smb_hfield(msg, FIDOFLAGS, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "ftn_pid", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
		smb_hfield(msg, FIDOPID, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "ftn_tid", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
		smb_hfield(msg, FIDOTID, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "date", &val) && val!=JSVAL_VOID) {
		if((js_str=JS_ValueToString(cx,val))==NULL)
			return(FALSE);
		if((cp=JS_GetStringBytes(js_str))==NULL)
			return(FALSE);
		msg->hdr.when_written=rfc822date(cp);
	}
	
	/* Numeric Header Fields */
	if(JS_GetProperty(cx, hdr, "attr", &val) && val!=JSVAL_VOID) 
		msg->hdr.attr=(ushort)JSVAL_TO_INT(val);
	if(JS_GetProperty(cx, hdr, "auxattr", &val) && val!=JSVAL_VOID) 
		msg->hdr.auxattr=JSVAL_TO_INT(val);
	if(JS_GetProperty(cx, hdr, "netattr", &val) && val!=JSVAL_VOID) 
		msg->hdr.netattr=JSVAL_TO_INT(val);
	if(JS_GetProperty(cx, hdr, "when_written_time", &val) && val!=JSVAL_VOID) 
		msg->hdr.when_written.time=JSVAL_TO_INT(val);
	if(JS_GetProperty(cx, hdr, "when_written_zone", &val) && val!=JSVAL_VOID) 
		msg->hdr.when_written.zone=(short)JSVAL_TO_INT(val);
	if(JS_GetProperty(cx, hdr, "when_imported_time", &val) && val!=JSVAL_VOID) 
		msg->hdr.when_imported.time=JSVAL_TO_INT(val);
	if(JS_GetProperty(cx, hdr, "when_imported_zone", &val) && val!=JSVAL_VOID) 
		msg->hdr.when_imported.zone=(short)JSVAL_TO_INT(val);

	return(TRUE);
}

static JSBool
js_get_msg_header(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		date[128];
	char		msg_id[256];
	char		reply_id[256];
	char		path[1024];
	char*		val;
	ulong		l;
	smbmsg_t	msg;
	smbmsg_t	orig_msg;
	JSObject*	hdrobj;
	private_t*	p;

	*rval = JSVAL_NULL;
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(!SMB_IS_OPEN(&(p->smb)))
		return(JS_TRUE);

	memset(&msg,0,sizeof(msg));

	if(JSVAL_TO_BOOLEAN(argv[0])==JS_TRUE)	/* Get by offset */
		msg.offset=JSVAL_TO_INT(argv[1]);
	else									/* Get by number */
		msg.hdr.number=JSVAL_TO_INT(argv[1]);

	if(smb_getmsgidx(&(p->smb), &msg)!=0)
		return(JS_TRUE);

	if(smb_lockmsghdr(&(p->smb),&msg)!=0)
		return(JS_TRUE);

	if(smb_getmsghdr(&(p->smb), &msg)!=0) {
		smb_unlockmsghdr(&(p->smb),&msg); 
		return(JS_TRUE);
	}

	smb_unlockmsghdr(&(p->smb),&msg); 

	if((hdrobj=JS_NewObject(cx,&js_msghdr_class,NULL,obj))==NULL) {
		smb_freemsgmem(&msg);
		return(JS_TRUE);
	}

	JS_DefineProperty(cx, hdrobj, "number", INT_TO_JSVAL(msg.hdr.number)
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "to",STRING_TO_JSVAL(JS_NewStringCopyZ(cx,msg.to))
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "from",STRING_TO_JSVAL(JS_NewStringCopyZ(cx,msg.from))
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "subject",STRING_TO_JSVAL(JS_NewStringCopyZ(cx,msg.subj))
		,NULL,NULL,JSPROP_ENUMERATE);
	if(msg.summary!=NULL)
		JS_DefineProperty(cx, hdrobj, "summary",STRING_TO_JSVAL(JS_NewStringCopyZ(cx,msg.summary))
			,NULL,NULL,JSPROP_ENUMERATE);
	if(msg.to_ext!=NULL)
		JS_DefineProperty(cx, hdrobj, "to_ext",STRING_TO_JSVAL(JS_NewStringCopyZ(cx,msg.to_ext))
			,NULL,NULL,JSPROP_ENUMERATE);
	if(msg.from_ext!=NULL)
		JS_DefineProperty(cx, hdrobj, "from_ext",STRING_TO_JSVAL(JS_NewStringCopyZ(cx,msg.from_ext))
			,NULL,NULL,JSPROP_ENUMERATE);
	if(msg.from_org!=NULL)
		JS_DefineProperty(cx, hdrobj, "from_org",STRING_TO_JSVAL(JS_NewStringCopyZ(cx,msg.from_org))
			,NULL,NULL,JSPROP_ENUMERATE);
	if(msg.replyto!=NULL)
		JS_DefineProperty(cx, hdrobj, "replyto",STRING_TO_JSVAL(JS_NewStringCopyZ(cx,msg.replyto))
			,NULL,NULL,JSPROP_ENUMERATE);
	if(msg.replyto_ext!=NULL)
		JS_DefineProperty(cx, hdrobj, "replyto_ext",STRING_TO_JSVAL(JS_NewStringCopyZ(cx,msg.replyto_ext))
			,NULL,NULL,JSPROP_ENUMERATE);

	JS_DefineProperty(cx, hdrobj, "to_agent",INT_TO_JSVAL(msg.to_agent)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "from_agent",INT_TO_JSVAL(msg.from_agent)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "replyto_agent",INT_TO_JSVAL(msg.replyto_agent)
		,NULL,NULL,JSPROP_ENUMERATE);

	JS_DefineProperty(cx, hdrobj, "to_net_type",INT_TO_JSVAL(msg.to_net.type)
		,NULL,NULL,JSPROP_ENUMERATE);
	if(msg.to_net.type)
		JS_DefineProperty(cx, hdrobj, "to_net_addr"
			,STRING_TO_JSVAL(JS_NewStringCopyZ(cx,net_addr(&msg.to_net)))
			,NULL,NULL,JSPROP_ENUMERATE);

	JS_DefineProperty(cx, hdrobj, "from_net_type",INT_TO_JSVAL(msg.from_net.type)
		,NULL,NULL,JSPROP_ENUMERATE);
	if(msg.from_net.type)
		JS_DefineProperty(cx, hdrobj, "from_net_addr"
			,STRING_TO_JSVAL(JS_NewStringCopyZ(cx,net_addr(&msg.from_net)))
			,NULL,NULL,JSPROP_ENUMERATE);

	JS_DefineProperty(cx, hdrobj, "replyto_net_type",INT_TO_JSVAL(msg.replyto_net.type)
		,NULL,NULL,JSPROP_ENUMERATE);
	if(msg.replyto_net.type)
		JS_DefineProperty(cx, hdrobj, "replyto_net_addr"
			,STRING_TO_JSVAL(JS_NewStringCopyZ(cx,net_addr(&msg.replyto_net)))
			,NULL,NULL,JSPROP_ENUMERATE);

	JS_DefineProperty(cx, hdrobj, "forwarded",INT_TO_JSVAL(msg.forwarded)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "expiration",INT_TO_JSVAL(msg.expiration)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "priority",INT_TO_JSVAL(msg.priority)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "cost",INT_TO_JSVAL(msg.cost)
		,NULL,NULL,JSPROP_ENUMERATE);


	JS_DefineProperty(cx, hdrobj, "type", INT_TO_JSVAL(msg.hdr.type)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "version", INT_TO_JSVAL(msg.hdr.version)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "attr", INT_TO_JSVAL(msg.hdr.attr)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "auxattr", INT_TO_JSVAL(msg.hdr.auxattr)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "netattr", INT_TO_JSVAL(msg.hdr.netattr)
		,NULL,NULL,JSPROP_ENUMERATE);

	JS_DefineProperty(cx, hdrobj, "when_written_time", INT_TO_JSVAL(msg.hdr.when_written.time)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "when_written_zone", INT_TO_JSVAL(msg.hdr.when_written.zone)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "when_imported_time", INT_TO_JSVAL(msg.hdr.when_imported.time)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "when_imported_zone", INT_TO_JSVAL(msg.hdr.when_imported.zone)
		,NULL,NULL,JSPROP_ENUMERATE);

	JS_DefineProperty(cx, hdrobj, "thread_orig", INT_TO_JSVAL(msg.hdr.thread_orig)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "thread_next", INT_TO_JSVAL(msg.hdr.thread_next)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "thread_first", INT_TO_JSVAL(msg.hdr.thread_first)
		,NULL,NULL,JSPROP_ENUMERATE);

	JS_DefineProperty(cx, hdrobj, "delivery_attempts", INT_TO_JSVAL(msg.hdr.delivery_attempts)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "last_downloaded", INT_TO_JSVAL(msg.hdr.last_downloaded)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "times_downloaded", INT_TO_JSVAL(msg.hdr.times_downloaded)
		,NULL,NULL,JSPROP_ENUMERATE);

	l=smb_getmsgdatlen(&msg);
	JS_DefineProperty(cx, hdrobj, "data_length", INT_TO_JSVAL(l)
		,NULL,NULL,JSPROP_ENUMERATE);

	JS_DefineProperty(cx, hdrobj, "date"
		,STRING_TO_JSVAL(JS_NewStringCopyZ(cx,msgdate(msg.hdr.when_written,date)))
		,NULL,NULL,JSPROP_ENUMERATE);

	/* Reply-ID (References) */
	if(msg.reply_id!=NULL)
		val=msg.reply_id;
	else {
		reply_id[0]=0;
		if(msg.hdr.thread_orig) {
			memset(&orig_msg,0,sizeof(orig_msg));
			orig_msg.hdr.number=msg.hdr.thread_orig;
			if(smb_getmsgidx(&(p->smb), &orig_msg))
				sprintf(reply_id,"<%s>",p->smb.last_error);
			else
				SAFECOPY(reply_id,get_msgid(scfg,p->smb.subnum,&orig_msg));
		}
		val=reply_id;
	}
	if(val[0])
		JS_DefineProperty(cx, hdrobj, "reply_id", STRING_TO_JSVAL(JS_NewStringCopyZ(cx,val))
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	/* Message-ID */
	SAFECOPY(msg_id,get_msgid(scfg,p->smb.subnum,&msg));
	val=msg_id;
	JS_DefineProperty(cx, hdrobj, "id", STRING_TO_JSVAL(JS_NewStringCopyZ(cx,val))
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	/* USENET Fields */
	if(msg.path!=NULL)
		sprintf(path, "%s!%.*s", scfg->sys_inetaddr, (int)sizeof(path)/2, msg.path);
	else
		sprintf(path, "%s!not-for-mail", scfg->sys_inetaddr);
	JS_DefineProperty(cx, hdrobj, "path", STRING_TO_JSVAL(JS_NewStringCopyZ(cx,path))
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);
	if(msg.newsgroups!=NULL)
		JS_DefineProperty(cx, hdrobj, "newsgroups"
			,STRING_TO_JSVAL(JS_NewStringCopyZ(cx,msg.newsgroups))
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	/* FidoNet Header Fields */
	if(msg.ftn_msgid!=NULL)
		JS_DefineProperty(cx, hdrobj, "ftn_msgid"
			,STRING_TO_JSVAL(JS_NewStringCopyZ(cx,msg.ftn_msgid))
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);
	if(msg.ftn_reply!=NULL)
		JS_DefineProperty(cx, hdrobj, "ftn_reply"
			,STRING_TO_JSVAL(JS_NewStringCopyZ(cx,msg.ftn_reply))
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);
	if(msg.ftn_pid!=NULL)
		JS_DefineProperty(cx, hdrobj, "ftn_pid"
			,STRING_TO_JSVAL(JS_NewStringCopyZ(cx,msg.ftn_pid))
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);
	if(msg.ftn_tid!=NULL)
		JS_DefineProperty(cx, hdrobj, "ftn_tid"
			,STRING_TO_JSVAL(JS_NewStringCopyZ(cx,msg.ftn_pid))
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);
	if(msg.ftn_area!=NULL)
		JS_DefineProperty(cx, hdrobj, "ftn_area"
			,STRING_TO_JSVAL(JS_NewStringCopyZ(cx,msg.ftn_area))
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);
	if(msg.ftn_flags!=NULL)
		JS_DefineProperty(cx, hdrobj, "ftn_flags"
			,STRING_TO_JSVAL(JS_NewStringCopyZ(cx,msg.ftn_flags))
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	smb_freemsgmem(&msg);

	*rval = OBJECT_TO_JSVAL(hdrobj);

	return(JS_TRUE);
}

static JSBool
js_put_msg_header(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	smbmsg_t	msg;
	JSObject*	hdr;
	private_t*	p;

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(!JSVAL_IS_OBJECT(argv[2]))
		return(JS_TRUE);
	hdr = JSVAL_TO_OBJECT(argv[2]);

	if(!SMB_IS_OPEN(&(p->smb)))
		return(JS_TRUE);

	memset(&msg,0,sizeof(msg));

	if(JSVAL_TO_BOOLEAN(argv[0])==JS_TRUE)	/* Get by offset */
		msg.offset=JSVAL_TO_INT(argv[1]);
	else									/* Get by number */
		msg.hdr.number=JSVAL_TO_INT(argv[1]);

	if(smb_getmsgidx(&(p->smb), &msg)!=0)
		return(JS_TRUE);

	if(smb_lockmsghdr(&(p->smb),&msg)!=0)
		return(JS_TRUE);

	do {
		if(smb_getmsghdr(&(p->smb), &msg)!=0)
			break;

		if(!parse_header_object(cx, p, hdr, &msg))
			break;

		if(smb_putmsghdr(&(p->smb), &msg)!=0)
			break;

		*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
	} while(0);

	smb_unlockmsghdr(&(p->smb),&msg); 
	smb_freemsgmem(&msg);

	return(JS_TRUE);
}

static char* get_msg_text(smb_t* smb, smbmsg_t* msg, BOOL strip_ctrl_a, BOOL rfc822, ulong mode)
{
	char*		buf;

	if(smb_getmsgidx(smb, msg)!=0)
		return(NULL);

	if(smb_lockmsghdr(smb,msg)!=0)
		return(NULL);

	if(smb_getmsghdr(smb, msg)!=0) {
		smb_unlockmsghdr(smb, msg); 
		return(NULL);
	}

	if((buf=smb_getmsgtxt(smb, msg, mode))==NULL) {
		smb_unlockmsghdr(smb,msg); 
		smb_freemsgmem(msg);
		return(NULL);
	}

	smb_unlockmsghdr(smb, msg); 
	smb_freemsgmem(msg);

	if(strip_ctrl_a) {
		char* newbuf;
		if((newbuf=malloc(strlen(buf)+1))!=NULL) {
			int i,j;
			for(i=j=0;buf[i];i++) {
				if(buf[i]==CTRL_A && buf[i+1]!=0)
					i++;
				else newbuf[j++]=buf[i]; 
			}
			newbuf[j]=0;
			strcpy(buf,newbuf);
			free(newbuf);
		}
	}

	if(rfc822) {	/* must escape lines starting with dot ('.') */
		char* newbuf;
		if((newbuf=malloc((strlen(buf)*2)+1))!=NULL) {
			int i,j;
			for(i=j=0;buf[i];i++) {
				if((i==0 || buf[i-1]=='\n') && buf[i]=='.')
					newbuf[j++]='.';
				newbuf[j++]=buf[i]; 
			}
			newbuf[j]=0;
			free(buf);
			buf = newbuf;
		}
	}

	return(buf);
}

static JSBool
js_get_msg_body(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		buf;
	smbmsg_t	msg;
	JSBool		strip_ctrl_a=JS_FALSE;
	JSBool		tails=JS_TRUE;
	JSBool		rfc822=JS_FALSE;
	private_t*	p;

	*rval = JSVAL_NULL;
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(!SMB_IS_OPEN(&(p->smb)))
		return(JS_TRUE);

	memset(&msg,0,sizeof(msg));

	if(JSVAL_TO_BOOLEAN(argv[0])==JS_TRUE)	/* Get by offset */
		msg.offset=JSVAL_TO_INT(argv[1]);
	else									/* Get by number */
		msg.hdr.number=JSVAL_TO_INT(argv[1]);

	if(argc>2)
		strip_ctrl_a=JSVAL_TO_BOOLEAN(argv[2]);

	if(argc>3)
		rfc822=JSVAL_TO_BOOLEAN(argv[3]);

	if(argc>4)
		tails=JSVAL_TO_BOOLEAN(argv[4]);

	buf = get_msg_text(&(p->smb), &msg, strip_ctrl_a, rfc822, tails ? GETMSGTXT_TAILS : 0);
	if(buf==NULL)
		return(JS_TRUE);

	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,buf));

	smb_freemsgtxt(buf);

	return(JS_TRUE);
}

static JSBool
js_get_msg_tail(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		buf;
	smbmsg_t	msg;
	JSBool		strip_ctrl_a=JS_FALSE;
	JSBool		rfc822=JS_FALSE;
	private_t*	p;

	*rval = JSVAL_NULL;
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(!SMB_IS_OPEN(&(p->smb)))
		return(JS_TRUE);

	memset(&msg,0,sizeof(msg));

	if(JSVAL_TO_BOOLEAN(argv[0])==JS_TRUE)	/* Get by offset */
		msg.offset=JSVAL_TO_INT(argv[1]);
	else									/* Get by number */
		msg.hdr.number=JSVAL_TO_INT(argv[1]);

	if(argc>2)
		strip_ctrl_a=JSVAL_TO_BOOLEAN(argv[2]);

	if(argc>3)
		rfc822=JSVAL_TO_BOOLEAN(argv[3]);

	buf = get_msg_text(&(p->smb), &msg, strip_ctrl_a, rfc822, GETMSGTXT_TAILS|GETMSGTXT_NO_BODY);
	if(buf==NULL)
		return(JS_TRUE);

	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,buf));

	smb_freemsgtxt(buf);

	return(JS_TRUE);
}

static JSBool
js_save_msg(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		body;
	JSString*	js_str;
	JSObject*	hdr;
	smbmsg_t	msg;
	private_t*	p;

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);

	if(argc<2)
		return(JS_TRUE);
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	memset(&msg,0,sizeof(msg));

	if(!JSVAL_IS_OBJECT(argv[0]))
		return(JS_TRUE);
	hdr = JSVAL_TO_OBJECT(argv[0]);

	if(!JSVAL_IS_STRING(argv[1]))
		return(JS_TRUE);
	if((js_str=JS_ValueToString(cx,argv[1]))==NULL) {
		JS_ReportError(cx,"JS_ValueToString failed");
		return(JS_FALSE);
	}
	if((body=JS_GetStringBytes(js_str))==NULL) {
		JS_ReportError(cx,"JS_GetStringBytes failed");
		return(JS_FALSE);
	}

	if(!parse_header_object(cx, p, hdr, &msg))
		return(JS_TRUE);

	if(msg.hdr.when_written.time==0) {
		msg.hdr.when_written.time=time(NULL);
		msg.hdr.when_written.zone=scfg->sys_timezone;
	}

	truncsp(body);
	if(savemsg(scfg, &(p->smb), &msg, body)==0)
		*rval = BOOLEAN_TO_JSVAL(JS_TRUE);

	return(JS_TRUE);
}

/* MsgBase Object Properites */
enum {
	 SMB_PROP_LAST_ERROR
	,SMB_PROP_FILE		
	,SMB_PROP_DEBUG		
	,SMB_PROP_RETRY_TIME
	,SMB_PROP_FIRST_MSG		// first message number
	,SMB_PROP_LAST_MSG		// last message number
	,SMB_PROP_TOTAL_MSGS 	// total messages
	,SMB_PROP_MAX_CRCS		// Maximum number of CRCs to keep in history
    ,SMB_PROP_MAX_MSGS      // Maximum number of message to keep in sub
    ,SMB_PROP_MAX_AGE       // Maximum age of message to keep in sub (in days)
	,SMB_PROP_ATTR			// Attributes for this message base (SMB_HYPER,etc)
	,SMB_PROP_SUBNUM		// sub-board number
	,SMB_PROP_IS_OPEN
};

static JSBool js_msgbase_set(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint       tiny;
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case SMB_PROP_RETRY_TIME:
			p->smb.retry_time = JSVAL_TO_BOOLEAN(*vp);
			break;
		case SMB_PROP_DEBUG:
			p->debug = JSVAL_TO_BOOLEAN(*vp);
			break;
	}

	return(JS_TRUE);
}

static JSBool js_msgbase_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint       tiny;
	idxrec_t	idx;
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case SMB_PROP_FILE:
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, p->smb.file));
			break;
		case SMB_PROP_LAST_ERROR:
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, p->smb.last_error));
			break;
		case SMB_PROP_RETRY_TIME:
			*vp = INT_TO_JSVAL(p->smb.retry_time);
			break;
		case SMB_PROP_DEBUG:
			*vp = INT_TO_JSVAL(p->debug);
			break;
		case SMB_PROP_FIRST_MSG:
			memset(&idx,0,sizeof(idx));
			smb_getfirstidx(&(p->smb),&idx);
			*vp = INT_TO_JSVAL(idx.number);
			break;
		case SMB_PROP_LAST_MSG:
			smb_getstatus(&(p->smb));
			*vp = INT_TO_JSVAL(p->smb.status.last_msg);
			break;
		case SMB_PROP_TOTAL_MSGS:
			smb_getstatus(&(p->smb));
			*vp = INT_TO_JSVAL(p->smb.status.total_msgs);
			break;
		case SMB_PROP_MAX_CRCS:
			*vp = INT_TO_JSVAL(p->smb.status.max_crcs);
			break;
		case SMB_PROP_MAX_MSGS:
			*vp = INT_TO_JSVAL(p->smb.status.max_msgs);
			break;
		case SMB_PROP_MAX_AGE:
			*vp = INT_TO_JSVAL(p->smb.status.max_age);
			break;
		case SMB_PROP_ATTR:
			*vp = INT_TO_JSVAL(p->smb.status.attr);
			break;
		case SMB_PROP_SUBNUM:
			*vp = INT_TO_JSVAL(p->smb.subnum);
			break;
		case SMB_PROP_IS_OPEN:
			*vp = BOOLEAN_TO_JSVAL(SMB_IS_OPEN(&(p->smb)));
			break;
	}

	return(JS_TRUE);
}

#define SMB_PROP_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static struct JSPropertySpec js_msgbase_properties[] = {
/*		 name				,tinyid					,flags,				getter,	setter	*/

	{	"last_error"		,SMB_PROP_LAST_ERROR	,SMB_PROP_FLAGS,	NULL,NULL},
	{	"file"				,SMB_PROP_FILE			,SMB_PROP_FLAGS,	NULL,NULL},
	{	"debug"				,SMB_PROP_DEBUG			,0,					NULL,NULL},
	{	"retry_time"		,SMB_PROP_RETRY_TIME	,JSPROP_ENUMERATE,	NULL,NULL},
	{	"first_msg"			,SMB_PROP_FIRST_MSG		,SMB_PROP_FLAGS,	NULL,NULL},
	{	"last_msg"			,SMB_PROP_LAST_MSG		,SMB_PROP_FLAGS,	NULL,NULL},
	{	"total_msgs"		,SMB_PROP_TOTAL_MSGS	,SMB_PROP_FLAGS,	NULL,NULL},
	{	"max_crcs"			,SMB_PROP_MAX_CRCS		,SMB_PROP_FLAGS,	NULL,NULL},
	{	"max_msgs"			,SMB_PROP_MAX_MSGS  	,SMB_PROP_FLAGS,	NULL,NULL},
	{	"max_age"			,SMB_PROP_MAX_AGE   	,SMB_PROP_FLAGS,	NULL,NULL},
	{	"attributes"		,SMB_PROP_ATTR			,SMB_PROP_FLAGS,	NULL,NULL},
	{	"subnum"			,SMB_PROP_SUBNUM		,SMB_PROP_FLAGS,	NULL,NULL},
	{	"is_open"			,SMB_PROP_IS_OPEN		,SMB_PROP_FLAGS,	NULL,NULL},
	{0}
};

#ifdef _DEBUG
static char* msgbase_prop_desc[] = {

	 "last occurred message base error - <small>READ ONLY</small>"
	,"base path and filename of message base - <small>READ ONLY</small>"
	,"message base open/lock retry timeout (in seconds)"
	,"first message number - <small>READ ONLY</small>"
	,"last message number - <small>READ ONLY</small>"
	,"total number of messages - <small>READ ONLY</small>"
	,"maximum number of message CRCs to store (for dupe checking) - <small>READ ONLY</small>"
	,"maximum number of messages before expiration - <small>READ ONLY</small>"
	,"maximum age (in days) of messages to store - <small>READ ONLY</small>"
	,"message base attributes - <small>READ ONLY</small>"
	,"sub-board number (0-based, -1 for e-mail) - <small>READ ONLY</small>"
	,"<i>true</i> if the message base has been opened successfully - <small>READ ONLY</small>"
	,NULL
};
#endif


static JSClass js_msgbase_class = {
     "MsgBase"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_msgbase_get			/* getProperty	*/
	,js_msgbase_set			/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_finalize_msgbase	/* finalize		*/
};

static jsMethodSpec js_msgbase_functions[] = {
	{"open",			js_open,			0, JSTYPE_BOOLEAN,	JSDOCSTR("")
	,JSDOCSTR("open message base")
	},
	{"close",			js_close,			0, JSTYPE_BOOLEAN,	JSDOCSTR("")
	,JSDOCSTR("close message base (if open)")
	},
	{"get_msg_header",	js_get_msg_header,	2, JSTYPE_OBJECT,	JSDOCSTR("boolean by_offset, number")
	,JSDOCSTR("returns a specific message header, <i>null</i> on failure")
	},
	{"put_msg_header",	js_put_msg_header,	2, JSTYPE_BOOLEAN,	JSDOCSTR("boolean by_offset, number, object header")
	,JSDOCSTR("write a message header")
	},
	{"get_msg_body",	js_get_msg_body,	2, JSTYPE_STRING,	JSDOCSTR("boolean by_offset, number [,boolean strip_ctrl_a]")
	,JSDOCSTR("returns the body text of a specific message, <i>null</i> on failure")
	},
	{"get_msg_tail",	js_get_msg_tail,	2, JSTYPE_STRING,	JSDOCSTR("boolean by_offset, number [,boolean strip_ctrl_a]")
	,JSDOCSTR("returns the tail text of a specific message, <i>null</i> on failure")
	},
	{"save_msg",		js_save_msg,		2, JSTYPE_BOOLEAN,	JSDOCSTR("object header, string body_text")
	,JSDOCSTR("create a new message in message base")
	},
	{0}
};

/* MsgBase Constructor (open message base) */

static JSBool
js_msgbase_constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSString*	js_str;
	char*		base;
	private_t*	p;

	if((p=(private_t*)malloc(sizeof(private_t)))==NULL) {
		JS_ReportError(cx,"malloc failed");
		return(JS_FALSE);
	}

	memset(p,0,sizeof(private_t));
	p->smb.retry_time=scfg->smb_retry_time;

	js_str = JS_ValueToString(cx, argv[0]);
	base=JS_GetStringBytes(js_str);

	p->debug=JS_FALSE;

	if(!JS_SetPrivate(cx, obj, p)) {
		JS_ReportError(cx,"JS_SetPrivate failed");
		free(p);
		return(JS_FALSE);
	}

	if(stricmp(base,"mail")==0) {
		p->smb.subnum=INVALID_SUB;
		snprintf(p->smb.file,sizeof(p->smb.file),"%s%s",scfg->data_dir,"mail");
	} else {
		for(p->smb.subnum=0;p->smb.subnum<scfg->total_subs;p->smb.subnum++) {
			if(!stricmp(scfg->sub[p->smb.subnum]->code,base))
				break;
		}
		if(p->smb.subnum<scfg->total_subs) {
			js_CreateMsgAreaProperties(cx, obj, scfg->sub[p->smb.subnum]);
			snprintf(p->smb.file,sizeof(p->smb.file),"%s%s"
				,scfg->sub[p->smb.subnum]->data_dir,scfg->sub[p->smb.subnum]->code);
		} else { /* unknown code */
			SAFECOPY(p->smb.file,base);
			p->smb.subnum=INVALID_SUB;
		}
	}

	if(!js_DefineMethods(cx, obj, js_msgbase_functions)) {
		JS_ReportError(cx,"js_DefineMethods failed");
		return(JS_FALSE);
	}

#ifdef _DEBUG
	js_DescribeObject(cx,obj,"Class used for accessing message bases");
	js_DescribeConstructor(cx,obj,"To create a new MsgBase object: "
		"<tt>var msgbase = new MsgBase('<i>code</i>')</tt><br>"
		"where <i>code</i> is a sub-board internal code, or <tt>mail</tt> for the e-mail message base");
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", msgbase_prop_desc, JSPROP_READONLY);
#endif

	return(JS_TRUE);
}

JSObject* DLLCALL js_CreateMsgBaseClass(JSContext* cx, JSObject* parent, scfg_t* cfg)
{
	JSObject*	obj;

	scfg = cfg;
	obj = JS_InitClass(cx, parent, NULL
		,&js_msgbase_class
		,js_msgbase_constructor
		,1	/* number of constructor args */
		,js_msgbase_properties
		,NULL //js_msgbase_functions
		,NULL,NULL);

	return(obj);
}


#endif	/* JAVSCRIPT */
