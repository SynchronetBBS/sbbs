/* js_bbs.cpp */

/* Synchronet JavaScript "bbs" Object */

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

/*****************************/
/* BBS Object Properites */
/*****************************/
enum {
	 BBS_PROP_SYS_STATUS
	,BBS_PROP_STARTUP_OPT
	,BBS_PROP_ANSWER_TIME
	,BBS_PROP_LOGON_TIME
	,BBS_PROP_NS_TIME
	,BBS_PROP_LAST_NS_TIME
	,BBS_PROP_ONLINE
	,BBS_PROP_TIMELEFT

	,BBS_PROP_NODE_NUM
	,BBS_PROP_NODE_MISC
	,BBS_PROP_NODE_ACTION
	,BBS_PROP_NODE_VAL_USER

	,BBS_PROP_LOGON_ULB
	,BBS_PROP_LOGON_DLB
	,BBS_PROP_LOGON_ULS
	,BBS_PROP_LOGON_DLS
	,BBS_PROP_LOGON_POSTS
	,BBS_PROP_LOGON_EMAILS
	,BBS_PROP_LOGON_FBACKS
	,BBS_PROP_POSTS_READ

	,BBS_PROP_MENU_DIR
	,BBS_PROP_MENU_FILE
	,BBS_PROP_MAIN_CMDS
	,BBS_PROP_FILE_CMDS

	,BBS_PROP_CURGRP
	,BBS_PROP_CURSUB
	,BBS_PROP_CURLIB
	,BBS_PROP_CURDIR

	,BBS_PROP_CONNECTION		/* READ ONLY */
	,BBS_PROP_RLOGIN_NAME
	,BBS_PROP_CLIENT_NAME

	,BBS_PROP_ALTUL
};

static JSBool js_bbs_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	char*		p=NULL;
	ulong		val=0;
    jsint       tiny;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case BBS_PROP_SYS_STATUS:
			val=sbbs->sys_status;
			break;
		case BBS_PROP_STARTUP_OPT:
			val=sbbs->startup->options;
			break;
		case BBS_PROP_ANSWER_TIME:
			val=sbbs->answertime;
			break;
		case BBS_PROP_LOGON_TIME:
			val=sbbs->logontime;
			break;
		case BBS_PROP_NS_TIME:
			val=sbbs->ns_time;
			break;
		case BBS_PROP_LAST_NS_TIME:
			val=sbbs->last_ns_time;
			break;
		case BBS_PROP_ONLINE:
			val=sbbs->online;
			break;
		case BBS_PROP_TIMELEFT:
			val=sbbs->timeleft;
			break;

		case BBS_PROP_NODE_NUM:
			val=sbbs->cfg.node_num;
			break;
		case BBS_PROP_NODE_MISC:
			val=sbbs->cfg.node_misc;
			break;
		case BBS_PROP_NODE_ACTION:
			val=sbbs->action;
			break;
		case BBS_PROP_NODE_VAL_USER:
			val=sbbs->cfg.node_valuser;
			break;

		case BBS_PROP_LOGON_ULB:
			val=sbbs->logon_ulb;
			break;
		case BBS_PROP_LOGON_DLB:
			val=sbbs->logon_dlb;
			break;
		case BBS_PROP_LOGON_ULS:
			val=sbbs->logon_uls;
			break;
		case BBS_PROP_LOGON_DLS:
			val=sbbs->logon_dls;
			break;
		case BBS_PROP_LOGON_POSTS:
			val=sbbs->logon_posts;
			break;
		case BBS_PROP_LOGON_EMAILS:
			val=sbbs->logon_emails;
			break;
		case BBS_PROP_LOGON_FBACKS:
			val=sbbs->logon_fbacks;
			break;
		case BBS_PROP_POSTS_READ:
			val=sbbs->posts_read;
			break;

		case BBS_PROP_MENU_DIR:
			p=sbbs->menu_dir;
			break;
		case BBS_PROP_MENU_FILE:
			p=sbbs->menu_file;
			break;
		case BBS_PROP_MAIN_CMDS:
			val=sbbs->main_cmds;
			break;
		case BBS_PROP_FILE_CMDS:
			val=sbbs->xfer_cmds;
			break;

		case BBS_PROP_CURGRP:
			val=sbbs->curgrp;
			break;
		case BBS_PROP_CURSUB:
			if(sbbs->curgrp<sbbs->usrgrps)
				val=sbbs->cursub[sbbs->curgrp];
			break;
		case BBS_PROP_CURLIB:
			val=sbbs->curlib;
			break;
		case BBS_PROP_CURDIR:
			if(sbbs->curlib<sbbs->usrlibs)
				val=sbbs->curdir[sbbs->curlib];
			break;

		case BBS_PROP_CONNECTION:
			p=sbbs->connection;
			break;
		case BBS_PROP_RLOGIN_NAME:
			p=sbbs->rlogin_name;
			break;
		case BBS_PROP_CLIENT_NAME:
			p=sbbs->client_name;
			break;

		case BBS_PROP_ALTUL:
			val=sbbs->altul;
			break;

		default:
			return(JS_TRUE);
	}
	if(p!=NULL) 
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, p));
	else
		*vp = INT_TO_JSVAL(val);

	return(JS_TRUE);
}

static JSBool js_bbs_set(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	char*		p=NULL;
	int32		val=0;
    jsint       tiny;
	JSString*	js_str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	if(JSVAL_IS_INT(*vp) || JSVAL_IS_BOOLEAN(*vp))
		JS_ValueToInt32(cx, *vp, &val);
	else if(JSVAL_IS_STRING(*vp)) {
		if((js_str = JS_ValueToString(cx, *vp))==NULL)
			return(JS_FALSE);
		p=JS_GetStringBytes(js_str);
	}

	switch(tiny) {
		case BBS_PROP_SYS_STATUS:
			sbbs->sys_status=val;
			break;
		case BBS_PROP_STARTUP_OPT:
			sbbs->startup->options=val;
			break;
		case BBS_PROP_ANSWER_TIME:
			sbbs->answertime=val;
			break;
		case BBS_PROP_LOGON_TIME:
			sbbs->logontime=val;
			break;
		case BBS_PROP_NS_TIME:
			sbbs->ns_time=val;
			break;
		case BBS_PROP_LAST_NS_TIME:
			sbbs->last_ns_time=val;
			break;
		case BBS_PROP_ONLINE:
			sbbs->online=val;
			break;
		case BBS_PROP_TIMELEFT:
			sbbs->timeleft=val;
			break;
		case BBS_PROP_NODE_MISC:
			sbbs->cfg.node_misc=val;
			break;
		case BBS_PROP_NODE_ACTION:
			sbbs->action=(uchar)val;
			break;
		case BBS_PROP_NODE_VAL_USER:
			sbbs->cfg.node_valuser=(ushort)val;
			break;
		case BBS_PROP_LOGON_ULB:
			sbbs->logon_ulb=val;
			break;
		case BBS_PROP_LOGON_DLB:
			sbbs->logon_dlb=val;
			break;
		case BBS_PROP_LOGON_ULS:
			sbbs->logon_uls=val;
			break;
		case BBS_PROP_LOGON_DLS:
			sbbs->logon_dls=val;
			break;
		case BBS_PROP_LOGON_POSTS:
			sbbs->logon_posts=val;
			break;
		case BBS_PROP_LOGON_EMAILS:
			sbbs->logon_emails=val;
			break;
		case BBS_PROP_LOGON_FBACKS:
			sbbs->logon_fbacks=val;
			break;
		case BBS_PROP_POSTS_READ:
			sbbs->posts_read=val;
			break;
		case BBS_PROP_MENU_DIR:
			sprintf(sbbs->menu_dir,"%.*s",sizeof(sbbs->menu_dir)-1,p);
			break;
		case BBS_PROP_MENU_FILE:
			sprintf(sbbs->menu_file,"%.*s",sizeof(sbbs->menu_file)-1,p);
			break;
		case BBS_PROP_MAIN_CMDS:
			sbbs->main_cmds=val;
			break;
		case BBS_PROP_FILE_CMDS:
			sbbs->xfer_cmds=val;
			break;

		case BBS_PROP_CURGRP:
			if((uint)val<sbbs->cfg.total_grps && (uint)val<sbbs->usrgrps)
				sbbs->curgrp=val;
			break;
		case BBS_PROP_CURSUB:
			if(sbbs->curgrp<sbbs->cfg.total_grps && (uint)val<sbbs->usrsubs[sbbs->curgrp])
				sbbs->cursub[sbbs->curgrp]=val;
			break;
		case BBS_PROP_CURLIB:
			if((uint)val<sbbs->cfg.total_libs && (uint)val<sbbs->usrlibs)
				sbbs->curlib=val;
			break;
		case BBS_PROP_CURDIR:
			if(sbbs->curlib<sbbs->cfg.total_libs && (uint)val<sbbs->usrdirs[sbbs->curlib])
				sbbs->curdir[sbbs->curlib]=val;
			break;

		case BBS_PROP_RLOGIN_NAME:
			sprintf(sbbs->rlogin_name,"%.*s",sizeof(sbbs->rlogin_name)-1,p);
			break;
		case BBS_PROP_CLIENT_NAME:
			sprintf(sbbs->client_name,"%.*s",sizeof(sbbs->client_name)-1,p);
			break;

		case BBS_PROP_ALTUL:
			if(val<sbbs->cfg.altpaths)
				sbbs->altul=(ushort)val;
			break;
		default:
			return(JS_TRUE);
	}

	if(sbbs->usrgrps)
		sbbs->cursubnum=sbbs->usrsub[sbbs->curgrp][sbbs->cursub[sbbs->curgrp]];		/* Used for ARS */
	else
		sbbs->cursubnum=INVALID_SUB;
	if(sbbs->usrlibs) 
		sbbs->curdirnum=sbbs->usrdir[sbbs->curlib][sbbs->curdir[sbbs->curlib]];		/* Used for ARS */
	else 
		sbbs->curdirnum=INVALID_DIR;

	return(JS_TRUE);
}

#define BBS_PROP_READONLY JSPROP_ENUMERATE|JSPROP_READONLY

static struct JSPropertySpec js_bbs_properties[] = {
/*		 name				,tinyid					,flags				,getter,setter	*/

	{	"sys_status"		,BBS_PROP_SYS_STATUS	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"startup_options"	,BBS_PROP_STARTUP_OPT	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"answer_time"		,BBS_PROP_ANSWER_TIME	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"logon_time"		,BBS_PROP_LOGON_TIME	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"new_file_time"		,BBS_PROP_NS_TIME		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"last_new_file_time",BBS_PROP_LAST_NS_TIME	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"online"			,BBS_PROP_ONLINE		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"time_left"			,BBS_PROP_TIMELEFT		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"node_num"			,BBS_PROP_NODE_NUM		,BBS_PROP_READONLY	,NULL,NULL},
	{	"node_settings"		,BBS_PROP_NODE_MISC		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"node_action"		,BBS_PROP_NODE_ACTION	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"node_val_user"		,BBS_PROP_NODE_VAL_USER	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"logon_ulb"			,BBS_PROP_LOGON_ULB		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"logon_dlb"			,BBS_PROP_LOGON_DLB		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"logon_uls"			,BBS_PROP_LOGON_ULS		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"logon_dls"			,BBS_PROP_LOGON_DLS		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"logon_posts"		,BBS_PROP_LOGON_POSTS	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"logon_emails"		,BBS_PROP_LOGON_EMAILS	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"logon_fbacks"		,BBS_PROP_LOGON_FBACKS	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"posts_read"		,BBS_PROP_POSTS_READ	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"menu_dir"			,BBS_PROP_MENU_DIR		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"menu_file"			,BBS_PROP_MENU_FILE		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"main_cmds"			,BBS_PROP_MAIN_CMDS		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"file_cmds"			,BBS_PROP_FILE_CMDS		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"curgrp"			,BBS_PROP_CURGRP		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"cursub"			,BBS_PROP_CURSUB		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"curlib"			,BBS_PROP_CURLIB		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"curdir"			,BBS_PROP_CURDIR		,JSPROP_ENUMERATE	,NULL,NULL},
	{	"connection"		,BBS_PROP_CONNECTION	,BBS_PROP_READONLY	,NULL,NULL},
	{	"rlogin_name"		,BBS_PROP_RLOGIN_NAME	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"client_name"		,BBS_PROP_CLIENT_NAME	,JSPROP_ENUMERATE	,NULL,NULL},
	{	"alt_ul_dir"		,BBS_PROP_ALTUL			,JSPROP_ENUMERATE	,NULL,NULL},
	{0}
};

/**************************/
/* bbs Object Methods */
/**************************/

static JSBool
js_menu(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString*	str;
 	sbbs_t*		sbbs;
 
 	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
 		return(JS_FALSE);
 
 	str = JS_ValueToString(cx, argv[0]);
 	if (!str)
 		return(JS_FALSE);
 
	sbbs->menu(JS_GetStringBytes(str));
 	*rval=JSVAL_VOID;

    return(JS_TRUE);
}
 
static JSBool
js_hangup(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->hangup();
	*rval=JSVAL_VOID;

	return(JS_TRUE);
}

static JSBool
js_nodesync(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->getnodedat(sbbs->cfg.node_num,&sbbs->thisnode,0);
	sbbs->nodesync();
	*rval=JSVAL_VOID;

	return(JS_TRUE);
}

static JSBool
js_exec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uintN		i;
	sbbs_t*		sbbs;
	long		mode=0;
    JSString*	cmd;
	JSString*	startup_dir=NULL;
	char*		p_startup_dir=NULL;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((cmd=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	for(i=1;i<argc;i++) {
		if(JSVAL_IS_INT(argv[i]))
			mode=JSVAL_TO_INT(argv[i]);
		else if(JSVAL_IS_STRING(argv[i]))
			startup_dir=JS_ValueToString(cx,argv[i]);
	}

	if(startup_dir!=NULL)
		p_startup_dir=JS_GetStringBytes(startup_dir);

	*rval = INT_TO_JSVAL(sbbs->external(JS_GetStringBytes(cmd),mode,p_startup_dir));

    return(JS_TRUE);
}

static JSBool
js_exec_xtrn(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uint		i;
	char*		code;
	sbbs_t*		sbbs;
    JSString*	str;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((str=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	if((code=JS_GetStringBytes(str))==NULL)
		return(JS_FALSE);

	for(i=0;i<sbbs->cfg.total_xtrns;i++)
		if(!stricmp(sbbs->cfg.xtrn[i]->code,code))
			break;

	if(i>=sbbs->cfg.total_xtrns) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	*rval = BOOLEAN_TO_JSVAL(sbbs->exec_xtrn(i));
	return(JS_TRUE);
}

static JSBool
js_user_event(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(
		sbbs->user_event((user_event_t)JSVAL_TO_INT(argv[0])));
	return(JS_TRUE);
}

static JSBool
js_chksyspass(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(sbbs->chksyspass());
	return(JS_TRUE);
}

static JSBool
js_text(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int			i;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	i=JSVAL_TO_INT(argv[0]);
	i--;

	if(i<0 || i>=TOTAL_TEXT)
		*rval = *rval = JSVAL_NULL;
	else
		*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, sbbs->text[i]));

	return(JS_TRUE);
}

static JSBool
js_replace_text(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	int			i,len;
	JSString*	js_str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	i=JSVAL_TO_INT(argv[0]);
	i--;

	if(i<0 || i>=TOTAL_TEXT) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	if(sbbs->text[i]!=sbbs->text_sav[i] && sbbs->text[i]!=nulstr)
		FREE(sbbs->text[i]);

	if((js_str=JS_ValueToString(cx, argv[1]))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	if((p=JS_GetStringBytes(js_str))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	len=strlen(p);
	if(!len)
		sbbs->text[i]=nulstr;
	else
		sbbs->text[i]=(char *)MALLOC(len+1);
	if(sbbs->text[i]==NULL) {
		sbbs->text[i]=sbbs->text_sav[i];
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	} else {
		strcpy(sbbs->text[i],p);
		*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
	}

	return(JS_TRUE);
}

static JSBool
js_revert_text(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int			i;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	i=JSVAL_TO_INT(argv[0]);
	i--;

	if(i<0 || i>=TOTAL_TEXT) {
		for(i=0;i<TOTAL_TEXT;i++) {
			if(sbbs->text[i]!=sbbs->text_sav[i] && sbbs->text[i]!=nulstr)
				FREE(sbbs->text[i]);
			sbbs->text[i]=sbbs->text_sav[i]; 
		}
	} else {
		if(sbbs->text[i]!=sbbs->text_sav[i] && sbbs->text[i]!=nulstr)
			FREE(sbbs->text[i]);
		sbbs->text[i]=sbbs->text_sav[i];
	}

	*rval = BOOLEAN_TO_JSVAL(JS_TRUE);

	return(JS_TRUE);
}

static JSBool
js_load_text(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int			i;
	char		path[MAX_PATH+1];
	FILE*		stream;
	JSString*	js_str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	for(i=0;i<TOTAL_TEXT;i++) {
		if(sbbs->text[i]!=sbbs->text_sav[i]) {
			if(sbbs->text[i]!=nulstr)
				FREE(sbbs->text[i]);
			sbbs->text[i]=sbbs->text_sav[i]; 
		}
	}
	sprintf(path,"%s%s.dat"
		,sbbs->cfg.ctrl_dir,JS_GetStringBytes(js_str));

	if((stream=fnopen(NULL,path,O_RDONLY))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}
	for(i=0;i<TOTAL_TEXT && !feof(stream);i++) {
		if((sbbs->text[i]=readtext((long *)NULL,stream))==NULL) {
			i--;
			continue; 
		}
		if(!strcmp(sbbs->text[i],sbbs->text_sav[i])) {	/* If identical */
			FREE(sbbs->text[i]);					/* Don't alloc */
			sbbs->text[i]=sbbs->text_sav[i]; 
		}
		else if(sbbs->text[i][0]==0) {
			FREE(sbbs->text[i]);
			sbbs->text[i]=nulstr; 
		} 
	}
	if(i<TOTAL_TEXT) 
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	else
		*rval = BOOLEAN_TO_JSVAL(JS_TRUE);

	fclose(stream);

	return(JS_TRUE);
}

static JSBool
js_logkey(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	JSBool		comma=false;
	JSString*	js_str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	if(argc>1)
		comma=JS_ValueToBoolean(cx,argv[1],&comma);

	if((p=JS_GetStringBytes(js_str))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	sbbs->logch(*p
		,comma ? true:false	// This is a dumb bool conversion to make BC++ happy
		);

	*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
	return(JS_TRUE);
}

static JSBool
js_logstr(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	JSString*	js_str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	if((p=JS_GetStringBytes(js_str))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	sbbs->log(p);

	*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
	return(JS_TRUE);
}

static JSBool
js_finduser(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	JSString*	js_str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		*rval = INT_TO_JSVAL(0);
		return(JS_TRUE);
	}

	if((p=JS_GetStringBytes(js_str))==NULL) {
		*rval = INT_TO_JSVAL(0);
		return(JS_TRUE);
	}

	*rval = INT_TO_JSVAL(sbbs->finduser(p));
	return(JS_TRUE);
}

static JSBool
js_trashcan(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		str;
	char*		can;
	JSString*	js_str;
	JSString*	js_can;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((js_can=JS_ValueToString(cx, argv[0]))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	if((js_str=JS_ValueToString(cx, argv[1]))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	if((can=JS_GetStringBytes(js_can))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	if((str=JS_GetStringBytes(js_str))==NULL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	*rval = BOOLEAN_TO_JSVAL(sbbs->trashcan(str,can));	// user args are reversed
	return(JS_TRUE);
}

static JSBool
js_newuser(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->newuser();

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_logon(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(sbbs->logon());
	return(JS_TRUE);
}

static JSBool
js_login(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		name;
	char*		pw;
	JSString*	js_name;
	JSString*	js_pw;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((js_name=JS_ValueToString(cx, argv[0]))==NULL) 
		return(JS_FALSE);

	if((js_pw=JS_ValueToString(cx, argv[1]))==NULL) 
		return(JS_FALSE);

	if((name=JS_GetStringBytes(js_name))==NULL) 
		return(JS_FALSE);

	if((pw=JS_GetStringBytes(js_pw))==NULL) 
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(sbbs->login(name,pw)==LOGIC_TRUE ? JS_TRUE:JS_FALSE);
	return(JS_TRUE);
}


static JSBool
js_logoff(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(!sbbs->noyes(sbbs->text[LogOffQ])) {
		if(sbbs->cfg.logoff_mod[0])
			sbbs->exec_bin(sbbs->cfg.logoff_mod,&sbbs->main_csi);
		sbbs->user_event(EVENT_LOGOFF);
		sbbs->menu("logoff");
		sbbs->riosync(0);
		sbbs->hangup(); 
	}

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_logout(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->logout();

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_automsg(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->automsg();

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_time_bank(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->time_bank();

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_text_sec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->text_sec();

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_xtrn_sec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->xtrn_sec();

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_xfer_policy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->xfer_policy();

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_batchmenu(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->batchmenu();

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_batchdownload(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->start_batch_download();

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_batchaddlist(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->batch_add_list(JS_GetStringBytes(JS_ValueToString(cx, argv[0])));

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_temp_xfer(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->temp_xfer();

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_user_config(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->maindflts(&sbbs->useron);
	if(!(sbbs->useron.rest&FLAG('G')))    /* not guest */
		getuserdat(&sbbs->cfg,&sbbs->useron);

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_sys_info(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->sys_info();

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_sub_info(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(sbbs->usrgrps>0)
		sbbs->subinfo(sbbs->usrsub[sbbs->curgrp][sbbs->cursub[sbbs->curgrp]]);

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_dir_info(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(sbbs->usrlibs>0)
		sbbs->dirinfo(sbbs->usrdir[sbbs->curlib][sbbs->curdir[sbbs->curlib]]);

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_user_info(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->user_info();

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_ver(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->ver();

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_sys_stats(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->sys_stats();

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_node_stats(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uint		node_num=0;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc>0)
		node_num=JSVAL_TO_INT(argv[0]);

	sbbs->node_stats(node_num);

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_userlist(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int			mode=UL_ALL;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc>0)
		mode=JSVAL_TO_INT(argv[0]);

	sbbs->userlist(mode);

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_useredit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uint		usernumber=0;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc>0)
		usernumber=JSVAL_TO_INT(argv[0]);

	sbbs->useredit(usernumber);

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_change_user(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uint		usernumber=0;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->change_user();

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_logonlist(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->logonlist();

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_nodelist(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->nodelist();

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_whos_online(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->whos_online(true);

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_spy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->spy(JSVAL_TO_INT(argv[0]));

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_readmail(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int			readwhich=MAIL_YOUR;
	uint		usernumber;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	usernumber=sbbs->useron.number;
	if(argc>0)
		readwhich=JSVAL_TO_INT(argv[0]);
	if(argc>1)
		usernumber=JSVAL_TO_INT(argv[1]);

	sbbs->readmail(usernumber,readwhich);

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_email(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uint		usernumber;
	long		mode=WM_EMAIL;
	char*		top="";
	char*		subj="";
	JSString*	js_top=NULL;
	JSString*	js_subj=NULL;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	usernumber=JSVAL_TO_INT(argv[0]);
	for(uintN i=1;i<argc;i++) {
		if(JSVAL_IS_INT(argv[i]))
			mode=JSVAL_TO_INT(argv[i]);
		else if(JSVAL_IS_STRING(argv[i]) && js_top==NULL)
			js_top=JS_ValueToString(cx,argv[i]);
		else if(JSVAL_IS_STRING(argv[i]))
			js_subj=JS_ValueToString(cx,argv[i]);
	}

	if(js_top!=NULL)
		top=JS_GetStringBytes(js_top);
	if(js_subj!=NULL)
		subj=JS_GetStringBytes(js_subj);

	*rval = BOOLEAN_TO_JSVAL(sbbs->email(usernumber,top,subj,mode));
	return(JS_TRUE);
}
static JSBool
js_netmail(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	long		mode=0;
	char*		subj="";
	JSString*	js_to;
	JSString*	js_subj=NULL;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((js_to=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	for(uintN i=1;i<argc;i++) {
		if(JSVAL_IS_INT(argv[i]))
			mode=JSVAL_TO_INT(argv[i]);
		else if(JSVAL_IS_STRING(argv[i]))
			js_subj=JS_ValueToString(cx,argv[i]);
	}

	if(js_subj!=NULL)
		subj=JS_GetStringBytes(js_subj);

	*rval = BOOLEAN_TO_JSVAL(sbbs->netmail(JS_GetStringBytes(js_to),subj,mode));
	return(JS_TRUE);
}

static JSBool
js_bulkmail(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uchar*		ar=(uchar*)"";
	JSString*	js_ars=NULL;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc) {
		if((js_ars=JS_ValueToString(cx, argv[0]))==NULL)
			return(JS_FALSE);

		ar=arstr(NULL,JS_GetStringBytes(js_ars), &sbbs->cfg);
	}
	sbbs->bulkmail(ar);
	if(ar && ar[0])
		FREE(ar);

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

js_bulkupload(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uint		i;
	char*		code;
	sbbs_t*		sbbs;
    JSString*	str;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((str=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	if((code=JS_GetStringBytes(str))==NULL)
		return(JS_FALSE);

	for(i=0;i<sbbs->cfg.total_dirs;i++)
		if(!stricmp(sbbs->cfg.dir[i]->code,code))
			break;

	if(i>=sbbs->cfg.total_dirs) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	sbbs->bulkupload(i);

	*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
	return(JS_TRUE);
}

js_resort_dir(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uint		i;
	char*		code;
	sbbs_t*		sbbs;
    JSString*	str;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((str=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	if((code=JS_GetStringBytes(str))==NULL)
		return(JS_FALSE);

	for(i=0;i<sbbs->cfg.total_dirs;i++)
		if(!stricmp(sbbs->cfg.dir[i]->code,code))
			break;

	if(i>=sbbs->cfg.total_dirs) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	sbbs->resort(i);

	*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
	return(JS_TRUE);
}

static JSBool
js_telnet_gate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		addr;
	int			mode=0;
	JSString*	js_addr;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((js_addr=JS_ValueToString(cx, argv[0]))==NULL) 
		return(JS_FALSE);

	if((addr=JS_GetStringBytes(js_addr))==NULL) 
		return(JS_FALSE);

	if(argc>1)
		mode=JSVAL_TO_INT(argv[1]);

	sbbs->telnet_gate(addr,mode);
	
	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_pagesysop(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(sbbs->sysop_page());
	return(JS_TRUE);
}

static JSBool
js_pageguru(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(sbbs->guru_page());
	return(JS_TRUE);
}

static JSBool
js_multinode_chat(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;
	int			channel=1;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc>1)
		channel=JSVAL_TO_INT(argv[1]);

	sbbs->multinodechat(channel);

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_private_message(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->nodemsg();

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_private_chat(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->privchat();

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_get_node_message(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->getnmsg();

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_put_node_message(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;
	int			node;
	JSString*	js_msg;
	char*		msg;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((node=JSVAL_TO_INT(argv[0]))<1)
		node=1;

	if((js_msg=JS_ValueToString(cx, argv[1]))==NULL) 
		return(JS_FALSE);

	if((msg=JS_GetStringBytes(js_msg))==NULL) 
		return(JS_FALSE);

	sbbs->putnmsg(node,msg);

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_get_telegram(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;
	int			usernumber;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	usernumber=sbbs->useron.number;
	if(argc>1)
		usernumber=JSVAL_TO_INT(argv[0]);

	sbbs->getsmsg(usernumber);

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_put_telegram(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;
	int			usernumber;
	JSString*	js_msg;
	char*		msg;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((usernumber=JSVAL_TO_INT(argv[0]))<1)
		usernumber=1;

	if((js_msg=JS_ValueToString(cx, argv[1]))==NULL) 
		return(JS_FALSE);

	if((msg=JS_GetStringBytes(js_msg))==NULL) 
		return(JS_FALSE);

	putsmsg(&sbbs->cfg,usernumber,msg);

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_cmdstr(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char*		fpath="";
	char*		fspec="";
	JSString*	js_str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

 	js_str = JS_ValueToString(cx, argv[0]);
 	if (!js_str)
 		return(JS_FALSE);

	p=JS_GetStringBytes(js_str);

	for(uintN i=1;i<argc;i++) {
		if(JSVAL_IS_STRING(argv[i])) {
			js_str = JS_ValueToString(cx, argv[i]);
			if(fpath==NULL)
				fpath=JS_GetStringBytes(js_str);
			else
				fspec=JS_GetStringBytes(js_str);
		}
	}

	p=sbbs->cmdstr(p,fpath,fspec,NULL);

	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, p));
	return(JS_TRUE);
}

static JSBool
js_getfilespec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char		tmp[128];
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	p=sbbs->getfilespec(tmp);

	if(p==NULL)
		*rval=JSVAL_NULL;
	else
		*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, p));
	return(JS_TRUE);
}

static JSBool
js_listfiles(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	long		mode=0;
	char*		code;
	char*		fspec=ALLFILES;
	uint		dirnum;
    JSString*	js_str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	if((code=JS_GetStringBytes(js_str))==NULL)
		return(JS_FALSE);

	for(dirnum=0;dirnum<sbbs->cfg.total_dirs;dirnum++)
		if(!stricmp(sbbs->cfg.dir[dirnum]->code,code))
			break;

	if(dirnum>=sbbs->cfg.total_dirs) {
		*rval = INT_TO_JSVAL(0);
		return(JS_TRUE);
	}

	for(uintN i=1;i<argc;i++) {
		if(JSVAL_IS_INT(argv[i]))
			mode=JSVAL_TO_INT(argv[i]);
		else if(JSVAL_IS_STRING(argv[i])) {
			js_str = JS_ValueToString(cx, argv[i]);
			fspec=JS_GetStringBytes(js_str);
		}
	}

	*rval = INT_TO_JSVAL(sbbs->listfiles(dirnum,fspec,0 /* tofile */,mode));
	return(JS_TRUE);
}


static JSBool
js_listfileinfo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	long		mode=FI_INFO;
	char*		code;
	char*		fspec=ALLFILES;
	uint		dirnum;
    JSString*	js_str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	if((code=JS_GetStringBytes(js_str))==NULL)
		return(JS_FALSE);

	for(dirnum=0;dirnum<sbbs->cfg.total_dirs;dirnum++)
		if(!stricmp(sbbs->cfg.dir[dirnum]->code,code))
			break;

	if(dirnum>=sbbs->cfg.total_dirs) {
		*rval = INT_TO_JSVAL(0);
		return(JS_TRUE);
	}

	for(uintN i=1;i<argc;i++) {
		if(JSVAL_IS_INT(argv[i]))
			mode=JSVAL_TO_INT(argv[i]);
		else if(JSVAL_IS_STRING(argv[i])) {
			js_str = JS_ValueToString(cx, argv[i]);
			fspec=JS_GetStringBytes(js_str);
		}
	}

	*rval = INT_TO_JSVAL(sbbs->listfileinfo(dirnum,fspec,mode));
	return(JS_TRUE);
}

static JSBool
js_scansubs(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	long		mode=SCAN_NEW;
	BOOL		all=FALSE;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	for(uintN i=1;i<argc;i++) {
		if(JSVAL_IS_INT(argv[i]))
			mode=JSVAL_TO_INT(argv[i]);
		else if(JSVAL_IS_BOOLEAN(argv[i]))
			all=JSVAL_TO_BOOLEAN(argv[i]);
	}

	if(all)
		sbbs->scanallsubs(mode);
	else
		sbbs->scansubs(mode);

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_scandirs(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	long		mode=0;
	BOOL		all=FALSE;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	for(uintN i=1;i<argc;i++) {
		if(JSVAL_IS_INT(argv[i]))
			mode=JSVAL_TO_INT(argv[i]);
		else if(JSVAL_IS_BOOLEAN(argv[i]))
			all=JSVAL_TO_BOOLEAN(argv[i]);
	}

	if(all)
		sbbs->scanalldirs(mode);
	else
		sbbs->scandirs(mode);

	*rval = JSVAL_VOID;
	return(JS_TRUE);
}

static JSBool
js_getnstime(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	time_t		t;
	sbbs_t*		sbbs;

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	t = JSVAL_TO_INT(argv[0]);

	if(sbbs->inputnstime(&t)==true) {
		*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
		argv[0] = INT_TO_JSVAL(t);
	}

	return(JS_TRUE);
}


static JSFunctionSpec js_bbs_functions[] = {
	/* text.dat */
	{"text",			js_text,			1},		// return text string from text.dat
	{"replace_text",	js_replace_text,	2},		// replace a text string
	{"revert_text",		js_revert_text,		0},		// revert to original text string
	{"load_text",		js_load_text,		1},		// load an alternate text.dat
	/* procedures */
	{"newuser",			js_newuser,			0},		// new user procedure
	{"login",			js_login,			2},		// login with username and pw prompt
	{"logon",			js_logon,			0},		// logon procedure
	{"logoff",			js_logoff,			0},		// logoff procedure
	{"logout",			js_logout,			0},		// logout procedure
	{"hangup",			js_hangup,			0},		// hangup immediately
	{"nodesync",		js_nodesync,		0},		// synchronize node with system
	{"auto_msg",		js_automsg,			0},		// edit/create auto-message
	{"time_bank",		js_time_bank,		0},		// time bank
	{"text_sec",		js_text_sec,		0},		// text section
	{"xtrn_sec",		js_xtrn_sec,		0},		// external programs section
	{"xfer_policy",		js_xfer_policy,		0},		// display file transfer policy
	{"batch_menu",		js_batchmenu,		0},		// batch file transfer menu
	{"batch_download",	js_batchdownload,	0},		// start batch download
	{"batch_add_list",	js_batchaddlist,	1},		// add file list to batch download queue
	{"temp_xfer",		js_temp_xfer,		0},		// temp xfer menu
	{"user_config",		js_user_config,		0},		// user config
	{"sys_info",		js_sys_info,		0},		// system info
	{"sub_info",		js_sub_info,		0},		// sub-board info
	{"dir_info",		js_dir_info,		0},		// directory info
	{"user_info",		js_user_info,		0},		// current user info
	{"ver",				js_ver,				0},		// version info
	{"sys_stats",		js_sys_stats,		0},		// system stats
	{"node_stats",		js_node_stats,		0},		// node stats
	{"userlist",		js_userlist,		0},		// user list
	{"useredit",		js_useredit,		0},		// user edit
	{"change_user",		js_change_user,		0},		// change to a different user
	{"logonlist",		js_logonlist,		0},		// logon list
	{"readmail",		js_readmail,		0},		// read private mail
	{"email",			js_email,			1},		// send private e-mail
	{"netmail",			js_netmail,			1},		// send private netmail
	{"bulk_mail",		js_bulkmail,		0},		// send bulk private e-mail
	{"bulk_upload",		js_bulkupload,		1},		// local upload of files to dir
	{"resort_dir",		js_resort_dir,		1},		// re-sort file directory
	{"listfiles",		js_listfiles,		1},		// listfiles(dirnum,filespec,mode)
	{"listfileinfo",	js_listfileinfo,	1},		// listfileinfo(dirnum,filespec,mode)
	{"scan_subs",		js_scansubs,		0},		// scansubs(mode,all)
	{"scan_dirs",		js_scandirs,		0},		// scandirs(mode,all)
	/* menuing */
	{"menu",			js_menu,			1},		// show menu
	{"log_key",			js_logkey,			1},		// log key to node.log (comma optional)
	{"log_str",			js_logstr,			1},		// log string to node.log
	/* users */
	{"finduser",		js_finduser,		1},		// find user (partial name support)
	{"trashcan",		js_trashcan,		2},		// search file for psuedo-regexp
	/* xtrn programs/modules */
	{"exec",			js_exec,			2},		// execute command line with mode
	{"exec_xtrn",		js_exec_xtrn,		1},		// execute external program by code
	{"user_event",		js_user_event,		1},		// execute user event by event type
	{"telnet_gate",		js_telnet_gate,		1},		// external telnet gateway (w/opt mode)
	/* security */
	{"check_syspass",	js_chksyspass,		0},		// verify system password
	/* chat/node stuff */
	{"page_sysop",		js_pagesysop,		0},		// page sysop for chat
	{"page_guru",		js_pageguru,		0},		// page guru for chat
	{"multinode_chat",	js_multinode_chat,	0},		// multi-node chat
	{"private_message",	js_private_message,	0},		// private inter-node message
	{"private_chat",	js_private_chat,	0},		// private inter-node chat
	{"get_node_message",js_get_node_message,0},		// getnmsg()
	{"put_node_message",js_put_node_message,2},		// putnmsg(nodenum,str)
	{"get_telegram",	js_get_telegram,	1},		// getsmsg(usernum)
	{"put_telegram",	js_put_telegram,	2},		// putsmsg(usernum,str)
	{"nodelist",		js_nodelist,		0},		// list all nodes
	{"whos_online",		js_whos_online,		0},		// list active nodes
	{"spy",				js_spy,				1},		// spy on node
	/* misc */
	{"cmdstr",			js_cmdstr,			1},		// command string
	/* input */
	{"get_filespec",	js_getfilespec,		0},		// get file specification
	{"get_newscantime",	js_getnstime,		1},		// get newscan time
	{0}
};


static JSClass js_bbs_class = {
     "BBS"					/* name			*/
    ,0						/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_bbs_get				/* getProperty	*/
	,js_bbs_set				/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};

JSObject* js_CreateBbsObject(JSContext* cx, JSObject* parent)
{
	JSObject* obj;

	obj = JS_DefineObject(cx, parent, "bbs", &js_bbs_class, NULL, 0);

	if(obj==NULL)
		return(NULL);

	if(!JS_DefineProperties(cx, obj, js_bbs_properties))
		return(NULL);

	if (!JS_DefineFunctions(cx, obj, js_bbs_functions)) 
		return(NULL);

	return(obj);
}

#endif	/* JAVSCRIPT */
