/* js_msgbase.c */

/* Synchronet JavaScript "MsgBase" Object */

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

static BOOL parse_header_object(JSContext* cx, private_t* p, JSObject* hdr, smbmsg_t* msg)
{
	char*		cp;
	char		to[256];
	char		from[256];
	ushort		nettype;
	ushort		type;
	ushort		agent;
	int32		i32;
	jsval		val;
	JSObject*	array;
	JSObject*	field;
	jsuint		i,len;

	if(hdr==NULL)
		return(FALSE);

	/* Required Header Fields */
	if(JS_GetProperty(cx, hdr, "subject", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
	} else
		cp="";
	smb_hfield(msg, SUBJECT, (ushort)strlen(cp), cp);
	msg->idx.subj=subject_crc(cp);

	if(JS_GetProperty(cx, hdr, "to", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
	} else {
		if(p->smb.status.attr&SMB_EMAIL)	/* e-mail */
			return(FALSE);					/* "to" property required */
		cp="All";
	}
	smb_hfield(msg, RECIPIENT, (ushort)strlen(cp), cp);
	if(!(p->smb.status.attr&SMB_EMAIL)) {
		SAFECOPY(to,cp);
		strlwr(to);
		msg->idx.to=crc16(to,0);
	}

	if(JS_GetProperty(cx, hdr, "from", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
	} else
		return(FALSE);	/* "from" property required */
	smb_hfield(msg, SENDER, (ushort)strlen(cp), cp);
	if(!(p->smb.status.attr&SMB_EMAIL)) {
		SAFECOPY(from,cp);
		strlwr(from);
		msg->idx.from=crc16(from,0);
	}

	/* Optional Header Fields */
	if(JS_GetProperty(cx, hdr, "from_ext", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, SENDEREXT, (ushort)strlen(cp), cp);
		if(p->smb.status.attr&SMB_EMAIL)
			msg->idx.from=atoi(cp);
	}

	if(JS_GetProperty(cx, hdr, "from_org", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, SENDERORG, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "from_net_type", &val) && val!=JSVAL_VOID) {
		JS_ValueToInt32(cx,val,&i32);
		nettype=(ushort)i32;
		smb_hfield(msg, SENDERNETTYPE, sizeof(nettype), &nettype);
		if(p->smb.status.attr&SMB_EMAIL && nettype!=NET_NONE)
			msg->idx.from=0;
	}

	if(JS_GetProperty(cx, hdr, "from_net_addr", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, SENDERNETADDR, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "from_agent", &val) && val!=JSVAL_VOID) {
		JS_ValueToInt32(cx,val,&i32);
		agent=(ushort)i32;
		smb_hfield(msg, SENDERAGENT, sizeof(agent), &agent);
	}

	if(JS_GetProperty(cx, hdr, "to_ext", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, RECIPIENTEXT, (ushort)strlen(cp), cp);
		if(p->smb.status.attr&SMB_EMAIL)
			msg->idx.to=atoi(cp);
	}

	if(JS_GetProperty(cx, hdr, "to_org", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, RECIPIENTORG, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "to_net_type", &val) && val!=JSVAL_VOID) {
		JS_ValueToInt32(cx,val,&i32);
		nettype=(ushort)i32;
		smb_hfield(msg, RECIPIENTNETTYPE, sizeof(nettype), &nettype);
		if(p->smb.status.attr&SMB_EMAIL && nettype!=NET_NONE)
			msg->idx.to=0;
	}

	if(JS_GetProperty(cx, hdr, "to_net_addr", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, RECIPIENTNETADDR, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "to_agent", &val) && val!=JSVAL_VOID) {
		JS_ValueToInt32(cx,val,&i32);
		agent=(ushort)i32;
		smb_hfield(msg, RECIPIENTAGENT, sizeof(agent), &agent);
	}

	if(JS_GetProperty(cx, hdr, "replyto", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, REPLYTO, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "replyto_ext", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, REPLYTOEXT, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "replyto_org", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, REPLYTOORG, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "replyto_net_type", &val) && val!=JSVAL_VOID) {
		JS_ValueToInt32(cx,val,&i32);
		nettype=(ushort)i32;
		smb_hfield(msg, REPLYTONETTYPE, sizeof(nettype), &nettype);
	}

	if(JS_GetProperty(cx, hdr, "replyto_net_addr", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, REPLYTONETADDR, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "replyto_agent", &val) && val!=JSVAL_VOID) {
		JS_ValueToInt32(cx,val,&i32);
		agent=(ushort)i32;
		smb_hfield(msg, REPLYTOAGENT, sizeof(agent), &agent);
	}

	/* RFC822 headers */
	if(JS_GetProperty(cx, hdr, "id", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, RFC822MSGID, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "reply_id", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, RFC822REPLYID, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "reverse_path", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, SMTPREVERSEPATH, (ushort)strlen(cp), cp);
	}

	/* USENET headers */
	if(JS_GetProperty(cx, hdr, "path", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, USENETPATH, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "newsgroups", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, USENETNEWSGROUPS, (ushort)strlen(cp), cp);
	}

	/* FTN headers */
	if(JS_GetProperty(cx, hdr, "ftn_msgid", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, FIDOMSGID, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "ftn_reply", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, FIDOREPLYID, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "ftn_area", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, FIDOAREA, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "ftn_flags", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, FIDOFLAGS, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "ftn_pid", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, FIDOPID, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "ftn_tid", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		smb_hfield(msg, FIDOTID, (ushort)strlen(cp), cp);
	}

	if(JS_GetProperty(cx, hdr, "date", &val) && val!=JSVAL_VOID) {
		if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
			return(FALSE);
		msg->hdr.when_written=rfc822date(cp);
	}
	
	/* Numeric Header Fields */
	if(JS_GetProperty(cx, hdr, "attr", &val) && val!=JSVAL_VOID) {
		JS_ValueToInt32(cx,val,&i32);
		msg->hdr.attr=(ushort)i32;
		msg->idx.attr=msg->hdr.attr;
	}
	if(JS_GetProperty(cx, hdr, "auxattr", &val) && val!=JSVAL_VOID) {
		JS_ValueToInt32(cx,val,&i32);
		msg->hdr.auxattr=i32;
	}
	if(JS_GetProperty(cx, hdr, "netattr", &val) && val!=JSVAL_VOID) {
		JS_ValueToInt32(cx,val,&i32);
		msg->hdr.netattr=i32;
	}
	if(JS_GetProperty(cx, hdr, "when_written_time", &val) && val!=JSVAL_VOID)  {
		JS_ValueToInt32(cx,val,&i32);
		msg->hdr.when_written.time=i32;
	}
	if(JS_GetProperty(cx, hdr, "when_written_zone", &val) && val!=JSVAL_VOID) {
		JS_ValueToInt32(cx,val,&i32);
		msg->hdr.when_written.zone=(short)i32;
	}
	if(JS_GetProperty(cx, hdr, "when_imported_time", &val) && val!=JSVAL_VOID) {
		JS_ValueToInt32(cx,val,&i32);
		msg->hdr.when_imported.time=i32;
	}
	if(JS_GetProperty(cx, hdr, "when_imported_zone", &val) && val!=JSVAL_VOID) {
		JS_ValueToInt32(cx,val,&i32);
		msg->hdr.when_imported.zone=(short)i32;
	}

	if(JS_GetProperty(cx, hdr, "thread_orig", &val) && val!=JSVAL_VOID) {
		JS_ValueToInt32(cx,val,&i32);
		msg->hdr.thread_orig=i32;
	}
	if(JS_GetProperty(cx, hdr, "thread_next", &val) && val!=JSVAL_VOID) {
		JS_ValueToInt32(cx,val,&i32);
		msg->hdr.thread_next=i32;
	}
	if(JS_GetProperty(cx, hdr, "thread_first", &val) && val!=JSVAL_VOID) {
		JS_ValueToInt32(cx,val,&i32);
		msg->hdr.thread_first=i32;
	}

	if(JS_GetProperty(cx, hdr, "field_list", &val) && JSVAL_IS_OBJECT(val)) {
		array=JSVAL_TO_OBJECT(val);
		len=0;
		if(!JS_GetArrayLength(cx, array, &len))
			return(FALSE);

		for(i=0;i<len;i++) {
			if(!JS_GetElement(cx, array, i, &val))
				continue;
			if(!JSVAL_IS_OBJECT(val))
				continue;
			field=JSVAL_TO_OBJECT(val);
			if(!JS_GetProperty(cx, field, "type", &val))
				continue;
			JS_ValueToInt32(cx,val,&i32);
			type=(ushort)i32;
			if(!JS_GetProperty(cx, field, "data", &val))
				continue;
			if((cp=JS_GetStringBytes(JS_ValueToString(cx,val)))==NULL)
				return(FALSE);
			smb_hfield(msg, type, (ushort)strlen(cp), cp);
		}
	}

	return(TRUE);
}

BOOL msg_offset_by_id(scfg_t* scfg, smb_t* smb, char* id, ulong* offset)
{
	smbmsg_t msg;

	if(!get_msg_by_id(scfg,smb,id,&msg))
		return(FALSE);

	smb_freemsgmem(&msg);

	*offset = msg.offset;
	return(TRUE);
}

static JSBool
js_get_msg_index(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uintN		n;
	jsval		val;
	smbmsg_t	msg;
	JSObject*	idxobj;
	JSBool		by_offset=JS_FALSE;
	private_t*	p;

	*rval = JSVAL_NULL;
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(!SMB_IS_OPEN(&(p->smb)))
		return(JS_TRUE);

	memset(&msg,0,sizeof(msg));

	for(n=0;n<argc;n++) {
		if(JSVAL_IS_BOOLEAN(argv[n]))
			by_offset=JSVAL_TO_BOOLEAN(argv[n]);
		else if(JSVAL_IS_INT(argv[n])) {
			if(by_offset)							/* Get by offset */
				msg.offset=JSVAL_TO_INT(argv[n]);
			else									/* Get by number */
				msg.hdr.number=JSVAL_TO_INT(argv[n]);

			if(smb_getmsgidx(&(p->smb), &msg)!=0)
				return(JS_TRUE);

			break;
		}
	}

	if((idxobj=JS_NewObject(cx,NULL,NULL,obj))==NULL)
		return(JS_TRUE);

	JS_DefineProperty(cx, idxobj, "number", INT_TO_JSVAL(msg.idx.number)
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	JS_DefineProperty(cx, idxobj, "to" ,INT_TO_JSVAL(msg.idx.to)
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	JS_DefineProperty(cx, idxobj, "from" ,INT_TO_JSVAL(msg.idx.from)
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	JS_DefineProperty(cx, idxobj, "subject" ,INT_TO_JSVAL(msg.idx.subj)
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	JS_DefineProperty(cx, idxobj, "attr" ,INT_TO_JSVAL(msg.idx.attr)
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	JS_NewNumberValue(cx,msg.idx.offset,&val);
	JS_DefineProperty(cx, idxobj, "offset", val
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	JS_NewNumberValue(cx,msg.idx.time,&val);
	JS_DefineProperty(cx, idxobj, "time", val
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	*rval = OBJECT_TO_JSVAL(idxobj);

	return(JS_TRUE);
}

static JSBool
js_get_msg_header(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		date[128];
	char		msg_id[256];
	char		reply_id[256];
	char*		val;
	int			i;
	uintN		n;
	smbmsg_t	msg;
	smbmsg_t	orig_msg;
	JSObject*	hdrobj;
	JSObject*	array;
	JSObject*	field;
	JSString*	js_str;
	jsint		items;
	jsval		v;
	JSBool		by_offset=JS_FALSE;
	private_t*	p;

	*rval = JSVAL_NULL;
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(!SMB_IS_OPEN(&(p->smb)))
		return(JS_TRUE);

	memset(&msg,0,sizeof(msg));

	for(n=0;n<argc;n++) {
		if(JSVAL_IS_BOOLEAN(argv[n]))
			by_offset=JSVAL_TO_BOOLEAN(argv[n]);
		else if(JSVAL_IS_INT(argv[n])) {
			if(by_offset)							/* Get by offset */
				msg.offset=JSVAL_TO_INT(argv[n]);
			else									/* Get by number */
				msg.hdr.number=JSVAL_TO_INT(argv[n]);

			if(smb_getmsgidx(&(p->smb), &msg)!=0)
				return(JS_TRUE);

			if(smb_lockmsghdr(&(p->smb),&msg)!=0)
				return(JS_TRUE);

			if(smb_getmsghdr(&(p->smb), &msg)!=0) {
				smb_unlockmsghdr(&(p->smb),&msg); 
				return(JS_TRUE);
			}

			smb_unlockmsghdr(&(p->smb),&msg); 
			break;
		} else if(JSVAL_IS_STRING(argv[n]))	{		/* Get by ID */
			if(!get_msg_by_id(scfg,&(p->smb)
				,JS_GetStringBytes(JSVAL_TO_STRING(argv[n]))
				,&msg))
				return(JS_TRUE);	/* ID not found */

			break;
		}
	}

	if(msg.hdr.number==0)	/* No valid message number/id/offset specified */
		return(JS_TRUE);

	if((hdrobj=JS_NewObject(cx,NULL,NULL,obj))==NULL) {
		smb_freemsgmem(&msg);
		return(JS_TRUE);
	}

	JS_NewNumberValue(cx,msg.hdr.number,&v);
	JS_DefineProperty(cx, hdrobj, "number", v, NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	if((js_str=JS_NewStringCopyZ(cx,truncsp(msg.to)))==NULL)
		return(JS_FALSE);
	JS_DefineProperty(cx, hdrobj, "to"
		,STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE);

	if((js_str=JS_NewStringCopyZ(cx,truncsp(msg.from)))==NULL)
		return(JS_FALSE);
	JS_DefineProperty(cx, hdrobj, "from"
		,STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE);

	if((js_str=JS_NewStringCopyZ(cx,truncsp(msg.subj)))==NULL)
		return(JS_FALSE);
	JS_DefineProperty(cx, hdrobj, "subject"
		,STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE);
	if(msg.summary!=NULL 
		&& (js_str=JS_NewStringCopyZ(cx,truncsp(msg.summary)))!=NULL)
		JS_DefineProperty(cx, hdrobj, "summary"
			,STRING_TO_JSVAL(js_str)
			,NULL,NULL,JSPROP_ENUMERATE);
	if(msg.to_ext!=NULL 
		&& (js_str=JS_NewStringCopyZ(cx,truncsp(msg.to_ext)))!=NULL)
		JS_DefineProperty(cx, hdrobj, "to_ext"
			,STRING_TO_JSVAL(js_str)
			,NULL,NULL,JSPROP_ENUMERATE);
	if(msg.from_ext!=NULL 
		&& (js_str=JS_NewStringCopyZ(cx,truncsp(msg.from_ext)))!=NULL)
		JS_DefineProperty(cx, hdrobj, "from_ext"
			,STRING_TO_JSVAL(js_str)
			,NULL,NULL,JSPROP_ENUMERATE);
	if(msg.from_org!=NULL
		&& (js_str=JS_NewStringCopyZ(cx,truncsp(msg.from_org)))!=NULL)
		JS_DefineProperty(cx, hdrobj, "from_org"
			,STRING_TO_JSVAL(js_str)
			,NULL,NULL,JSPROP_ENUMERATE);
	if(msg.replyto!=NULL
		&& (js_str=JS_NewStringCopyZ(cx,truncsp(msg.replyto)))!=NULL)
		JS_DefineProperty(cx, hdrobj, "replyto"
			,STRING_TO_JSVAL(js_str)
			,NULL,NULL,JSPROP_ENUMERATE);
	if(msg.replyto_ext!=NULL
		&& (js_str=JS_NewStringCopyZ(cx,truncsp(msg.replyto_ext)))!=NULL)
		JS_DefineProperty(cx, hdrobj, "replyto_ext"
			,STRING_TO_JSVAL(js_str)
			,NULL,NULL,JSPROP_ENUMERATE);
	if(msg.reverse_path!=NULL
		&& (js_str=JS_NewStringCopyZ(cx,truncsp(msg.reverse_path)))!=NULL)
		JS_DefineProperty(cx, hdrobj, "reverse_path"
			,STRING_TO_JSVAL(js_str)
			,NULL,NULL,JSPROP_ENUMERATE);

	JS_DefineProperty(cx, hdrobj, "to_agent",INT_TO_JSVAL(msg.to_agent)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "from_agent",INT_TO_JSVAL(msg.from_agent)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "replyto_agent",INT_TO_JSVAL(msg.replyto_agent)
		,NULL,NULL,JSPROP_ENUMERATE);

	JS_DefineProperty(cx, hdrobj, "to_net_type",INT_TO_JSVAL(msg.to_net.type)
		,NULL,NULL,JSPROP_ENUMERATE);
	if(msg.to_net.type
		&& (js_str=JS_NewStringCopyZ(cx,net_addr(&msg.to_net)))!=NULL)
		JS_DefineProperty(cx, hdrobj, "to_net_addr"
			,STRING_TO_JSVAL(js_str)
			,NULL,NULL,JSPROP_ENUMERATE);

	JS_DefineProperty(cx, hdrobj, "from_net_type",INT_TO_JSVAL(msg.from_net.type)
		,NULL,NULL,JSPROP_ENUMERATE);
	if(msg.from_net.type
		&& (js_str=JS_NewStringCopyZ(cx,net_addr(&msg.from_net)))!=NULL)
		JS_DefineProperty(cx, hdrobj, "from_net_addr"
			,STRING_TO_JSVAL(js_str)
			,NULL,NULL,JSPROP_ENUMERATE);

	JS_DefineProperty(cx, hdrobj, "replyto_net_type",INT_TO_JSVAL(msg.replyto_net.type)
		,NULL,NULL,JSPROP_ENUMERATE);
	if(msg.replyto_net.type
		&& (js_str=JS_NewStringCopyZ(cx,net_addr(&msg.replyto_net)))!=NULL)
		JS_DefineProperty(cx, hdrobj, "replyto_net_addr"
			,STRING_TO_JSVAL(js_str)
			,NULL,NULL,JSPROP_ENUMERATE);

	JS_DefineProperty(cx, hdrobj, "forwarded",INT_TO_JSVAL(msg.forwarded)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_NewNumberValue(cx,msg.expiration,&v);
	JS_DefineProperty(cx, hdrobj, "expiration",v,NULL,NULL,JSPROP_ENUMERATE);
	JS_NewNumberValue(cx,msg.priority,&v);
	JS_DefineProperty(cx, hdrobj, "priority",v,NULL,NULL,JSPROP_ENUMERATE);
	JS_NewNumberValue(cx,msg.cost,&v);
	JS_DefineProperty(cx, hdrobj, "cost",v,NULL,NULL,JSPROP_ENUMERATE);


	JS_DefineProperty(cx, hdrobj, "type", INT_TO_JSVAL(msg.hdr.type)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "version", INT_TO_JSVAL(msg.hdr.version)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_DefineProperty(cx, hdrobj, "attr", INT_TO_JSVAL(msg.hdr.attr)
		,NULL,NULL,JSPROP_ENUMERATE);
	JS_NewNumberValue(cx,msg.hdr.auxattr,&v);
	JS_DefineProperty(cx, hdrobj, "auxattr", v, NULL,NULL,JSPROP_ENUMERATE);
	JS_NewNumberValue(cx,msg.hdr.netattr,&v);
	JS_DefineProperty(cx, hdrobj, "netattr", v, NULL,NULL,JSPROP_ENUMERATE);

	JS_NewNumberValue(cx,msg.hdr.when_written.time,&v);
	JS_DefineProperty(cx, hdrobj, "when_written_time", v, NULL,NULL,JSPROP_ENUMERATE);
	JS_NewNumberValue(cx,msg.hdr.when_written.zone,&v);
	JS_DefineProperty(cx, hdrobj, "when_written_zone", v, NULL,NULL,JSPROP_ENUMERATE);
	JS_NewNumberValue(cx,msg.hdr.when_imported.time,&v);
	JS_DefineProperty(cx, hdrobj, "when_imported_time", v, NULL,NULL,JSPROP_ENUMERATE);
	JS_NewNumberValue(cx,msg.hdr.when_imported.zone,&v);
	JS_DefineProperty(cx, hdrobj, "when_imported_zone", v, NULL,NULL,JSPROP_ENUMERATE);

	JS_NewNumberValue(cx,msg.hdr.thread_orig,&v);
	JS_DefineProperty(cx, hdrobj, "thread_orig", v, NULL,NULL,JSPROP_ENUMERATE);
	JS_NewNumberValue(cx,msg.hdr.thread_next,&v);
	JS_DefineProperty(cx, hdrobj, "thread_next", v, NULL,NULL,JSPROP_ENUMERATE);
	JS_NewNumberValue(cx,msg.hdr.thread_first,&v);
	JS_DefineProperty(cx, hdrobj, "thread_first", v, NULL,NULL,JSPROP_ENUMERATE);

	JS_NewNumberValue(cx,msg.hdr.delivery_attempts,&v);
	JS_DefineProperty(cx, hdrobj, "delivery_attempts", v, NULL,NULL,JSPROP_ENUMERATE);
	JS_NewNumberValue(cx,msg.hdr.last_downloaded,&v);
	JS_DefineProperty(cx, hdrobj, "last_downloaded", v, NULL,NULL,JSPROP_ENUMERATE);
	JS_NewNumberValue(cx,msg.hdr.times_downloaded,&v);
	JS_DefineProperty(cx, hdrobj, "times_downloaded", v, NULL,NULL,JSPROP_ENUMERATE);

	JS_NewNumberValue(cx,smb_getmsgdatlen(&msg),&v);
	JS_DefineProperty(cx, hdrobj, "data_length", v, NULL,NULL,JSPROP_ENUMERATE);

	if((js_str=JS_NewStringCopyZ(cx,msgdate(msg.hdr.when_written,date)))==NULL)
		return(JS_FALSE);
	JS_DefineProperty(cx, hdrobj, "date"
		,STRING_TO_JSVAL(js_str)
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
	if(val[0] && (js_str=JS_NewStringCopyZ(cx,truncsp(val)))!=NULL)
		JS_DefineProperty(cx, hdrobj, "reply_id"
			, STRING_TO_JSVAL(js_str)
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	/* Message-ID */
	SAFECOPY(msg_id,get_msgid(scfg,p->smb.subnum,&msg));
	val=msg_id;
	if((js_str=JS_NewStringCopyZ(cx,truncsp(val)))==NULL)
		return(JS_FALSE);
	JS_DefineProperty(cx, hdrobj, "id"
		,STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	/* USENET Fields */
	if(msg.path!=NULL
		&& (js_str=JS_NewStringCopyZ(cx,truncsp(msg.path)))!=NULL)
		JS_DefineProperty(cx, hdrobj, "path"
			,STRING_TO_JSVAL(js_str)
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);
	if(msg.newsgroups!=NULL
		&& (js_str=JS_NewStringCopyZ(cx,truncsp(msg.newsgroups)))!=NULL)
		JS_DefineProperty(cx, hdrobj, "newsgroups"
			,STRING_TO_JSVAL(js_str)
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	/* FidoNet Header Fields */
	if(msg.ftn_msgid!=NULL
		&& (js_str=JS_NewStringCopyZ(cx,truncsp(msg.ftn_msgid)))!=NULL)
		JS_DefineProperty(cx, hdrobj, "ftn_msgid"
			,STRING_TO_JSVAL(js_str)
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);
	if(msg.ftn_reply!=NULL
		&& (js_str=JS_NewStringCopyZ(cx,truncsp(msg.ftn_reply)))!=NULL)
		JS_DefineProperty(cx, hdrobj, "ftn_reply"
			,STRING_TO_JSVAL(js_str)
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);
	if(msg.ftn_pid!=NULL
		&& (js_str=JS_NewStringCopyZ(cx,truncsp(msg.ftn_pid)))!=NULL)
		JS_DefineProperty(cx, hdrobj, "ftn_pid"
			,STRING_TO_JSVAL(js_str)
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);
	if(msg.ftn_tid!=NULL
		&& (js_str=JS_NewStringCopyZ(cx,truncsp(msg.ftn_tid)))!=NULL)
		JS_DefineProperty(cx, hdrobj, "ftn_tid"
			,STRING_TO_JSVAL(js_str)
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);
	if(msg.ftn_area!=NULL
		&& (js_str=JS_NewStringCopyZ(cx,truncsp(msg.ftn_area)))!=NULL)
		JS_DefineProperty(cx, hdrobj, "ftn_area"
			,STRING_TO_JSVAL(js_str)
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);
	if(msg.ftn_flags!=NULL
		&& (js_str=JS_NewStringCopyZ(cx,truncsp(msg.ftn_flags)))!=NULL)
		JS_DefineProperty(cx, hdrobj, "ftn_flags"
			,STRING_TO_JSVAL(js_str)
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	/* Create hdr.field_list[] with repeating header fields (including type and data) */
	if((array=JS_NewArrayObject(cx,0,NULL))!=NULL) {
		items=0;
		for(i=0;i<msg.total_hfields;i++) {
			switch(msg.hfield[i].type) {
				case SMB_COMMENT:
				case SMB_CARBONCOPY:
				case SMB_GROUP:
				case FILEATTACH:
				case DESTFILE:
				case FILEATTACHLIST:
				case DESTFILELIST:
				case FILEREQUEST:
				case FILEPASSWORD:
				case FILEREQUESTLIST:
				case FILEPASSWORDLIST:
				case FIDOCTRL:
				case FIDOSEENBY:
				case FIDOPATH:
				case RFC822HEADER:
				case UNKNOWNASCII:
					/* only support these header field types */
					break;
				default:
					/* dupe or possibly binary header field */
					continue;
			}
			if((field=JS_NewObject(cx,NULL,NULL,array))==NULL)
				continue;
			JS_DefineProperty(cx,field,"type"
				,INT_TO_JSVAL(msg.hfield[i].type)
				,NULL,NULL,JSPROP_ENUMERATE);
			if((js_str=JS_NewStringCopyN(cx,msg.hfield_dat[i],msg.hfield[i].length))==NULL)
				break;
			JS_DefineProperty(cx,field,"data"
				,STRING_TO_JSVAL(js_str)
				,NULL,NULL,JSPROP_ENUMERATE);
			JS_DefineElement(cx,array,items,OBJECT_TO_JSVAL(field)
				,NULL,NULL,JSPROP_ENUMERATE);
			items++;
		}
		JS_DefineProperty(cx,hdrobj,"field_list",OBJECT_TO_JSVAL(array)
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);
	}

	smb_freemsgmem(&msg);

	*rval = OBJECT_TO_JSVAL(hdrobj);

	return(JS_TRUE);
}

static JSBool
js_put_msg_header(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uintN		n;
	JSBool		by_offset=JS_FALSE;
	JSBool		msg_specified=JS_FALSE;
	smbmsg_t	msg;
	JSObject*	hdr=NULL;
	private_t*	p;

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(!SMB_IS_OPEN(&(p->smb)))
		return(JS_TRUE);

	memset(&msg,0,sizeof(msg));

	for(n=0;n<argc;n++) {
		if(JSVAL_IS_BOOLEAN(argv[n]))
			by_offset=JSVAL_TO_BOOLEAN(argv[n]);
		else if(JSVAL_IS_INT(argv[n])) {
			if(by_offset)							/* Get by offset */
				msg.offset=JSVAL_TO_INT(argv[n]);
			else									/* Get by number */
				msg.hdr.number=JSVAL_TO_INT(argv[n]);
			msg_specified=JS_TRUE;
			n++;
			break;
		} else if(JSVAL_IS_STRING(argv[n]))	{		/* Get by ID */
			if(!msg_offset_by_id(scfg,&(p->smb)
				,JS_GetStringBytes(JSVAL_TO_STRING(argv[n]))
				,&msg.offset))
				return(JS_TRUE);	/* ID not found */
			msg_specified=JS_TRUE;
			n++;
			break;
		}
	}

	if(!msg_specified)
		return(JS_TRUE);

	if(n==argc || !JSVAL_IS_OBJECT(argv[n])) /* no header supplied? */
		return(JS_TRUE);

	hdr = JSVAL_TO_OBJECT(argv[n++]);

	if(smb_getmsgidx(&(p->smb), &msg)!=0)
		return(JS_TRUE);

	if(smb_lockmsghdr(&(p->smb),&msg)!=0)
		return(JS_TRUE);

	do {
		if(smb_getmsghdr(&(p->smb), &msg)!=0)
			break;

		smb_freemsghdrmem(&msg);	/* prevent duplicate header fields */

		if(!parse_header_object(cx, p, hdr, &msg))
			break;

		if(smb_putmsg(&(p->smb), &msg)!=0)
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
	uintN		n;
	smbmsg_t	msg;
	JSBool		by_offset=JS_FALSE;
	JSBool		strip_ctrl_a=JS_FALSE;
	JSBool		tails=JS_TRUE;
	JSBool		rfc822=JS_FALSE;
	JSBool		msg_specified=JS_FALSE;
	JSString*	js_str;
	private_t*	p;

	*rval = JSVAL_NULL;
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(!SMB_IS_OPEN(&(p->smb)))
		return(JS_TRUE);

	memset(&msg,0,sizeof(msg));

	for(n=0;n<argc;n++) {
		if(JSVAL_IS_BOOLEAN(argv[n]))
			by_offset=JSVAL_TO_BOOLEAN(argv[n]);
		else if(JSVAL_IS_INT(argv[n])) {
			if(by_offset)							/* Get by offset */
				msg.offset=JSVAL_TO_INT(argv[n]);
			else									/* Get by number */
				msg.hdr.number=JSVAL_TO_INT(argv[n]);
			msg_specified=JS_TRUE;
			n++;
			break;
		} else if(JSVAL_IS_STRING(argv[n]))	{		/* Get by ID */
			if(!msg_offset_by_id(scfg,&(p->smb)
				,JS_GetStringBytes(JSVAL_TO_STRING(argv[n]))
				,&msg.offset))
				return(JS_TRUE);	/* ID not found */
			msg_specified=JS_TRUE;
			n++;
			break;
		}
	}

	if(!msg_specified)	/* No message number or id specified */
		return(JS_TRUE);

	if(n<argc && JSVAL_IS_BOOLEAN(argv[n]))
		strip_ctrl_a=JSVAL_TO_BOOLEAN(argv[n++]);

	if(n<argc && JSVAL_IS_BOOLEAN(argv[n]))
		rfc822=JSVAL_TO_BOOLEAN(argv[n++]);

	if(n<argc && JSVAL_IS_BOOLEAN(argv[n]))
		tails=JSVAL_TO_BOOLEAN(argv[n++]);

	buf = get_msg_text(&(p->smb), &msg, strip_ctrl_a, rfc822, tails ? GETMSGTXT_TAILS : 0);
	if(buf==NULL)
		return(JS_TRUE);

	if((js_str=JS_NewStringCopyZ(cx,buf))!=NULL)
		*rval = STRING_TO_JSVAL(js_str);

	smb_freemsgtxt(buf);

	return(JS_TRUE);
}

static JSBool
js_get_msg_tail(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		buf;
	uintN		n;
	smbmsg_t	msg;
	JSBool		by_offset=JS_FALSE;
	JSBool		strip_ctrl_a=JS_FALSE;
	JSBool		rfc822=JS_FALSE;
	JSBool		msg_specified=JS_FALSE;
	JSString*	js_str;
	private_t*	p;

	*rval = JSVAL_NULL;
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(!SMB_IS_OPEN(&(p->smb)))
		return(JS_TRUE);

	memset(&msg,0,sizeof(msg));

	for(n=0;n<argc;n++) {
		if(JSVAL_IS_BOOLEAN(argv[n]))
			by_offset=JSVAL_TO_BOOLEAN(argv[n]);
		else if(JSVAL_IS_INT(argv[n])) {
			if(by_offset)							/* Get by offset */
				msg.offset=JSVAL_TO_INT(argv[n]);
			else									/* Get by number */
				msg.hdr.number=JSVAL_TO_INT(argv[n]);
			msg_specified=JS_TRUE;
			n++;
			break;
		} else if(JSVAL_IS_STRING(argv[n]))	{		/* Get by ID */
			if(!msg_offset_by_id(scfg,&(p->smb)
				,JS_GetStringBytes(JSVAL_TO_STRING(argv[n]))
				,&msg.offset))
				return(JS_TRUE);	/* ID not found */
			msg_specified=JS_TRUE;
			n++;
			break;
		}
	}

	if(!msg_specified)	/* No message number or id specified */
		return(JS_TRUE);

	if(n<argc && JSVAL_IS_BOOLEAN(argv[n]))
		strip_ctrl_a=JSVAL_TO_BOOLEAN(argv[n++]);

	if(n<argc && JSVAL_IS_BOOLEAN(argv[n]))
		rfc822=JSVAL_TO_BOOLEAN(argv[n++]);

	buf = get_msg_text(&(p->smb), &msg, strip_ctrl_a, rfc822, GETMSGTXT_TAILS|GETMSGTXT_NO_BODY);
	if(buf==NULL)
		return(JS_TRUE);

	if((js_str=JS_NewStringCopyZ(cx,buf))!=NULL)
		*rval = STRING_TO_JSVAL(js_str);

	smb_freemsgtxt(buf);

	return(JS_TRUE);
}

static JSBool
js_save_msg(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		body=NULL;
	uintN		n;
	JSObject*	hdr=NULL;
	smbmsg_t	msg;
	jsval		open_rval;
	private_t*	p;

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);

	if(argc<2)
		return(JS_TRUE);
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(!SMB_IS_OPEN(&(p->smb))) {
		if(!js_open(cx, obj, 0, NULL, &open_rval))
			return(JS_FALSE);
		if(open_rval == JSVAL_FALSE)
			return(JS_TRUE);
	}

	memset(&msg,0,sizeof(msg));

	for(n=0;n<argc;n++) {
		if(JSVAL_IS_OBJECT(argv[n]))
			hdr = JSVAL_TO_OBJECT(argv[n]);
		else if(JSVAL_IS_STRING(argv[n])) {
			if((body=JS_GetStringBytes(JSVAL_TO_STRING(argv[n])))==NULL) {
				JS_ReportError(cx,"JS_GetStringBytes failed");
				return(JS_FALSE);
			}
		}
	}

	if(hdr==NULL || body==NULL)
		return(JS_TRUE);

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
	,SMB_PROP_RETRY_DELAY
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
			p->smb.retry_time = JSVAL_TO_INT(*vp);
			break;
		case SMB_PROP_RETRY_DELAY:
			p->smb.retry_delay = JSVAL_TO_INT(*vp);
			break;
		case SMB_PROP_DEBUG:
			p->debug = JSVAL_TO_BOOLEAN(*vp);
			break;
	}

	return(JS_TRUE);
}

static JSBool js_msgbase_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	char*		s=NULL;
	JSString*	js_str;
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
			s=p->smb.file;
			break;
		case SMB_PROP_LAST_ERROR:
			s=p->smb.last_error;
			break;
		case SMB_PROP_RETRY_TIME:
			*vp = INT_TO_JSVAL(p->smb.retry_time);
			break;
		case SMB_PROP_RETRY_DELAY:
			*vp = INT_TO_JSVAL(p->smb.retry_delay);
			break;
		case SMB_PROP_DEBUG:
			*vp = INT_TO_JSVAL(p->debug);
			break;
		case SMB_PROP_FIRST_MSG:
			memset(&idx,0,sizeof(idx));
			smb_getfirstidx(&(p->smb),&idx);
			JS_NewNumberValue(cx,idx.number,vp);
			break;
		case SMB_PROP_LAST_MSG:
			smb_getstatus(&(p->smb));
			JS_NewNumberValue(cx,p->smb.status.last_msg,vp);
			break;
		case SMB_PROP_TOTAL_MSGS:
			smb_getstatus(&(p->smb));
			JS_NewNumberValue(cx,p->smb.status.total_msgs,vp);
			break;
		case SMB_PROP_MAX_CRCS:
			JS_NewNumberValue(cx,p->smb.status.max_crcs,vp);
			break;
		case SMB_PROP_MAX_MSGS:
			JS_NewNumberValue(cx,p->smb.status.max_msgs,vp);
			break;
		case SMB_PROP_MAX_AGE:
			JS_NewNumberValue(cx,p->smb.status.max_age,vp);
			break;
		case SMB_PROP_ATTR:
			JS_NewNumberValue(cx,p->smb.status.attr,vp);
			break;
		case SMB_PROP_SUBNUM:
			*vp = INT_TO_JSVAL(p->smb.subnum);
			break;
		case SMB_PROP_IS_OPEN:
			*vp = BOOLEAN_TO_JSVAL(SMB_IS_OPEN(&(p->smb)));
			break;
	}

	if(s!=NULL) {
		if((js_str=JS_NewStringCopyZ(cx, s))==NULL)
			return(JS_FALSE);
		*vp = STRING_TO_JSVAL(js_str);
	}

	return(JS_TRUE);
}

#define SMB_PROP_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static struct JSPropertySpec js_msgbase_properties[] = {
/*		 name				,tinyid					,flags,				getter,	setter	*/

	{	"error"				,SMB_PROP_LAST_ERROR	,SMB_PROP_FLAGS,	NULL,NULL},
	{	"last_error"		,SMB_PROP_LAST_ERROR	,JSPROP_READONLY,	NULL,NULL},		/* alias */
	{	"file"				,SMB_PROP_FILE			,SMB_PROP_FLAGS,	NULL,NULL},
	{	"debug"				,SMB_PROP_DEBUG			,0,					NULL,NULL},
	{	"retry_time"		,SMB_PROP_RETRY_TIME	,JSPROP_ENUMERATE,	NULL,NULL},
	{	"retry_delay"		,SMB_PROP_RETRY_DELAY	,JSPROP_ENUMERATE,	NULL,NULL},
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
	,"delay between message base open/lock retries (in milliseconds)"
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
	{"get_msg_header",	js_get_msg_header,	2, JSTYPE_OBJECT,	JSDOCSTR("boolean by_offset, number_or_id")
	,JSDOCSTR("returns a specific message header, <i>null</i> on failure")
	},
	{"put_msg_header",	js_put_msg_header,	2, JSTYPE_BOOLEAN,	JSDOCSTR("boolean by_offset, number, object header")
	,JSDOCSTR("write a message header")
	},
	{"get_msg_body",	js_get_msg_body,	2, JSTYPE_STRING,	JSDOCSTR("boolean by_offset, number_or_id [,boolean strip_ctrl_a]")
	,JSDOCSTR("returns the body text of a specific message, <i>null</i> on failure")
	},
	{"get_msg_tail",	js_get_msg_tail,	2, JSTYPE_STRING,	JSDOCSTR("boolean by_offset, number_or_id [,boolean strip_ctrl_a]")
	,JSDOCSTR("returns the tail text of a specific message, <i>null</i> on failure")
	},
	{"get_msg_index",	js_get_msg_index,	2, JSTYPE_OBJECT,	JSDOCSTR("boolean by_offset, number")
	,JSDOCSTR("returns a specific message index, <i>null</i> on failure. "
	"The index object will contain the following properties:<br>"
	"<table>"
	"<tr><td><tt>subject</tt><td>CRC-16 of lowercase message subject"
	"<tr><td><tt>to</tt><td>CRC-16 of lowercase recipient's name (or user number if e-mail)"
	"<tr><td><tt>from</tt><td>CRC-16 of lowercase sender's name (or user number if e-mail)"
	"<tr><td><tt>attr</tt><td>Attribute bitfield"
	"<tr><td><tt>time</tt><td>Date/time (in time_t format)"
	"<tr><td><tt>number</tt><td>Message number"
	"<tr><td><tt>offset</tt><td>Byte-offset into header file"
	"</table>")
	},
	{"save_msg",		js_save_msg,		2, JSTYPE_BOOLEAN,	JSDOCSTR("object header, string body_text")
	,JSDOCSTR("create a new message in message base, the <i>header</i> object may contain the following properties:<br>"
	"<table>"
	"<tr><td><tt>subject</tt><td>Message subject <i>(required)</i>"
	"<tr><td><tt>to</tt><td>Recipient's name <i>(required)</i>"
	"<tr><td><tt>to_ext</tt><td>Recipient's user number (for local e-mail)"
	"<tr><td><tt>to_org</tt><td>Recipient's organization"
	"<tr><td><tt>to_net_type</tt><td>Recipient's network type (default: 0 for local)"
	"<tr><td><tt>to_net_addr</tt><td>Recipient's network address"
	"<tr><td><tt>to_agent</tt><td>Recipient's agent type"
	"<tr><td><tt>from</tt><td>Sender's name <i>(required)</i>"
	"<tr><td><tt>from_ext</tt><td>Sender's user number"
	"<tr><td><tt>from_org</tt><td>Sender's organization"
	"<tr><td><tt>from_net_type</tt><td>Sender's network type (default: 0 for local)"
	"<tr><td><tt>from_net_addr</tt><td>Sender's network address"
	"<tr><td><tt>replyto</tt><td>Replies should be sent to this name"
	"<tr><td><tt>replyto_ext</tt><td>Replies should be sent to this user number"
	"<tr><td><tt>replyto_org</tt><td>Replies should be sent to organization"
	"<tr><td><tt>replyto_net_type</tt><td>Replies should be sent to this network type"
	"<tr><td><tt>replyto_net_addr</tt><td>Replies should be sent to this network address"
	"<tr><td><tt>replyto_agent</tt><td>Replies should be sent to this agent type"
	"<tr><td><tt>id</tt><td>Message's RFC-822 compliant Message-ID"
	"<tr><td><tt>reply_id</tt><td>Message's RFC-822 compliant Reply-ID"
	"<tr><td><tt>reverse_path</tt><td>Message's SMTP sender address"
	"<tr><td><tt>path</tt><td>Messages's NNTP path"
	"<tr><td><tt>newsgroups</tt><td>Message's NNTP newsgroups header"
	"<tr><td><tt>ftn_msgid</tt><td>FidoNet FTS-9 Message-ID"
	"<tr><td><tt>ftn_reply</tt><td>FidoNet FTS-9 Reply-ID"
	"<tr><td><tt>ftn_area</tt><td>FidoNet FTS-4 echomail AREA tag"
	"<tr><td><tt>ftn_flags</tt><td>FidoNet FSC-53 FLAGS"
	"<tr><td><tt>ftn_pid</tt><td>FidoNet FSC-46 Program Identifier"
	"<tr><td><tt>ftn_tid</tt><td>FidoNet FSC-46 Tosser Identifier"
	"<tr><td><tt>date</tt><td>RFC-822 formatted date/time"
	"<tr><td><tt>attr</tt><td>Attribute bitfield"
	"<tr><td><tt>auxattr</tt><td>Auxillary attribute bitfield"
	"<tr><td><tt>netattr</tt><td>Network attribute bitfield"
	"<tr><td><tt>when_written_time</tt><td>Date/time (in time_t format)"
	"<tr><td><tt>when_written_zone</tt><td>Time zone"
	"<tr><td><tt>when_imported_time</tt><td>Date/time message was imported"
	"<tr><td><tt>when_imported_zone</tt><td>Time zone"
	"<tr><td><tt>thread_orig</tt><td>Replying to this message number"
	"<tr><td><tt>thread_next</tt><td>Number of next message in this thread"
	"<tr><td><tt>thread_first</tt><td>Number of first reply to this message"
	"<tr><td><tt>field_list[].type</tt><td>Other SMB header fields (type)"
	"<tr><td><tt>field_list[].data</tt><td>Other SMB header fields (data)"
	"</table>")
	},
	{0}
};

/* MsgBase Constructor (open message base) */

static JSBool
js_msgbase_constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSString*	js_str;
	JSObject*	cfgobj;
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

	JS_DefineProperties(cx,obj,js_msgbase_properties);

#ifdef _DEBUG
	js_DescribeObject(cx,obj,"Class used for accessing message bases");
	js_DescribeConstructor(cx,obj,"To create a new MsgBase object: "
		"<tt>var msgbase = new MsgBase('<i>code</i>')</tt><br>"
		"where <i>code</i> is a sub-board internal code, or <tt>mail</tt> for the e-mail message base");
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", msgbase_prop_desc, JSPROP_READONLY);
#endif

	if(stricmp(base,"mail")==0) {
		p->smb.subnum=INVALID_SUB;
		snprintf(p->smb.file,sizeof(p->smb.file),"%s%s",scfg->data_dir,"mail");
	} else {
		for(p->smb.subnum=0;p->smb.subnum<scfg->total_subs;p->smb.subnum++) {
			if(!stricmp(scfg->sub[p->smb.subnum]->code,base))	/* null ptr dereference here Apr-16-2003 */
				break;
		}
		if(p->smb.subnum<scfg->total_subs) {
			cfgobj=JS_NewObject(cx,NULL,NULL,obj);
			js_CreateMsgAreaProperties(cx, cfgobj, scfg->sub[p->smb.subnum]);
#ifdef _DEBUG
			js_DescribeObject(cx,cfgobj
				,"Configuration parameters for this message area (<i>sub-boards only</i>) "
				"- <small>READ ONLY</small>");
#endif
			JS_DefineProperty(cx,obj,"cfg",OBJECT_TO_JSVAL(cfgobj)
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);
			snprintf(p->smb.file,sizeof(p->smb.file),"%s%s"
				,scfg->sub[p->smb.subnum]->data_dir,scfg->sub[p->smb.subnum]->code);
		} else { /* unknown code */
			SAFECOPY(p->smb.file,base);
			p->smb.subnum=INVALID_SUB;
		}
	}

	if(!js_DefineMethods(cx, obj, js_msgbase_functions, FALSE)) {
		JS_ReportError(cx,"js_DefineMethods failed");
		return(JS_FALSE);
	}

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
		,NULL //js_msgbase_properties
		,NULL //js_msgbase_functions
		,NULL,NULL);

	return(obj);
}


#endif	/* JAVSCRIPT */
