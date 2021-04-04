/* Synchronet JavaScript "Message Area" Object */

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

#ifdef JAVASCRIPT

#ifdef BUILD_JSDOCS

static char* msg_area_prop_desc[] = {
	  "message area settings (bitfield) - see <tt>MM_*</tt> in <tt>sbbsdefs.js</tt> for details"
	 ,"FidoNet NetMail settings (bitfield) - see <tt>NMAIL_*</tt> in <tt>sbbsdefs.js</tt> for details"
	 ,"Internet NetMail settings (bitfield) - see <tt>NMAIL_*</tt> in <tt>sbbsdefs.js</tt> for details"
	,NULL
};

static char* msg_grp_prop_desc[] = {
	 "index into grp_list array (or -1 if not in array) <i>(introduced in v3.12)</i>"
	,"unique number for this message group"
	,"group name"
	,"group description"
	,"group access requirements"
	,"user has sufficient access to list this group's sub-boards <i>(introduced in v3.18)</i>"
	,"internal code prefix (for sub-boards) <i>(introduced in v3.18c)</i>"
	,NULL
};

static char* msg_sub_prop_desc[] = {

	 "index into sub_list array (or -1 if not in array) <i>(introduced in v3.12)</i>"
	,"group's index into grp_list array <i>(introduced in v3.12)</i>"
	,"unique number for this sub-board"
	,"group number"
	,"group name <i>(introduced in v3.12)</i>"
	,"sub-board internal code"
	,"sub-board name"
	,"sub-board description"
	,"sub-board QWK name"
	,"newsgroup name (as configured or dymamically generated)"
	,"sub-board access requirements"
	,"sub-board reading requirements"
	,"sub-board posting requirements"
	,"sub-board operator requirements"
	,"sub-board moderated-user requirements (if non-blank)"
	,"sub-board data storage location"
	,"FidoNet origin line"
	,"QWK Network tagline"
	,"toggle options (bitfield) - see <tt>SUB_*</tt> in <tt>sbbsdefs.js</tt> for details"
	,"index into message scan configuration/pointer file"
	,"QWK conference number"
	,"configured maximum number of message CRCs to store (for dupe checking)"
	,"configured maximum number of messages before purging"
	,"configured maximum age (in days) of messages before expiration"
	,"additional print mode flags to use when printing messages - see <tt>P_*</tt> in <tt>sbbsdefs.js</tt> for details"
	,"print mode flags to <i>negate</i> when printing messages - see <tt>P_*</tt> in <tt>sbbsdefs.js</tt> for details"
	/* Insert here */
	,"user has sufficient access to see this sub-board"
	,"user has sufficient access to read messages in this sub-board"
	,"user has sufficient access to post messages in this sub-board"
	,"user has operator access to this sub-board"
	,"user's posts are moderated"
	,"user's current new message scan pointer (highest-read message number)"
	,"user's message scan configuration (bitfield) - see <tt>SCAN_CFG_*</tt> in <tt>sbbsdefs.js</tt> for details"
	,"user's last-read message number"
	,"number of messages currently posted to this sub-board <i>(introduced in v3.18c)</i>"
	,NULL
};
#endif

struct js_msg_area_priv {
	scfg_t		*cfg;
	user_t		*user;
	client_t	*client;
	subscan_t	*subscan;
	uint		subnum;
};

BOOL DLLCALL js_CreateMsgAreaProperties(JSContext* cx, scfg_t* cfg, JSObject* subobj, uint subnum)
{
	char		str[128];
	JSString*	js_str;
	jsval		val;
	sub_t*		sub;

	if(subnum==INVALID_SUB || subnum>=cfg->total_subs)
		return(FALSE);

	sub=cfg->sub[subnum];

	if(!JS_DefineProperty(cx, subobj, "number", INT_TO_JSVAL(subnum)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if(!JS_DefineProperty(cx, subobj, "grp_number", INT_TO_JSVAL(sub->grp)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if((js_str=JS_NewStringCopyZ(cx, cfg->grp[sub->grp]->sname))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, subobj, "grp_name", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if((js_str=JS_NewStringCopyZ(cx, sub->code))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, subobj, "code", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if((js_str=JS_NewStringCopyZ(cx, sub->sname))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, subobj, "name", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if((js_str=JS_NewStringCopyZ(cx, sub->lname))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, subobj, "description", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if((js_str=JS_NewStringCopyZ(cx, sub->qwkname))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, subobj, "qwk_name", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);
	if((js_str=JS_NewStringCopyZ(cx, subnewsgroupname(cfg, sub, str, sizeof(str))))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, subobj, "newsgroup", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if((js_str=JS_NewStringCopyZ(cx, sub->arstr))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, subobj, "ars", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if((js_str=JS_NewStringCopyZ(cx, sub->read_arstr))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, subobj, "read_ars", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if((js_str=JS_NewStringCopyZ(cx, sub->post_arstr))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, subobj, "post_ars", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if((js_str=JS_NewStringCopyZ(cx, sub->op_arstr))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, subobj, "operator_ars", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if((js_str=JS_NewStringCopyZ(cx, sub->mod_arstr))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, subobj, "moderated_ars", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if((js_str=JS_NewStringCopyZ(cx, sub->data_dir))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, subobj, "data_dir", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if((js_str=JS_NewStringCopyZ(cx, sub->origline))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, subobj, "fidonet_origin", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if((js_str=JS_NewStringCopyZ(cx, sub->tagline))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, subobj, "qwknet_tagline", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	val=UINT_TO_JSVAL(sub->misc);
	if(!JS_DefineProperty(cx, subobj, "settings", val
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if(!JS_DefineProperty(cx, subobj, "ptridx", INT_TO_JSVAL(sub->ptridx)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if(!JS_DefineProperty(cx, subobj, "qwk_conf", INT_TO_JSVAL(sub->qwkconf)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if(!JS_DefineProperty(cx, subobj, "max_crcs", INT_TO_JSVAL(sub->maxcrcs)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if(!JS_DefineProperty(cx, subobj, "max_msgs", INT_TO_JSVAL(sub->maxmsgs)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if(!JS_DefineProperty(cx, subobj, "max_age", INT_TO_JSVAL(sub->maxage)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if(!JS_DefineProperty(cx, subobj, "print_mode", INT_TO_JSVAL(sub->pmode)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if(!JS_DefineProperty(cx, subobj, "print_mode_neg", INT_TO_JSVAL(sub->n_pmode)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);


#ifdef BUILD_JSDOCS
	js_CreateArrayOfStrings(cx, subobj, "_property_desc_list", msg_sub_prop_desc, JSPROP_READONLY);
#endif

	return(TRUE);
}

/***************************************/
/* Dynamic Sub-board Object Properites */
/***************************************/
enum {
	 SUB_PROP_CAN_ACCESS
	,SUB_PROP_CAN_READ
	,SUB_PROP_CAN_POST
	,SUB_PROP_IS_OPERATOR
	,SUB_PROP_IS_MODERATED
	,SUB_PROP_SCAN_PTR
	,SUB_PROP_SCAN_CFG
	,SUB_PROP_LAST_READ
	,SUB_PROP_POSTS
};

static JSBool js_sub_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval idval;
    jsint       tiny;
	struct js_msg_area_priv *p;

	if((p=(struct js_msg_area_priv*)JS_GetPrivate(cx, obj))==NULL)
		return JS_TRUE;
	subscan_t*	scan = p->subscan;

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch(tiny) {
		case SUB_PROP_CAN_ACCESS:
			*vp = BOOLEAN_TO_JSVAL(p->user==NULL || can_user_access_sub(p->cfg, p->subnum, p->user, p->client));
			break;
		case SUB_PROP_CAN_READ:
			*vp = BOOLEAN_TO_JSVAL(p->user==NULL || can_user_read_sub(p->cfg, p->subnum, p->user, p->client));
			break;
		case SUB_PROP_CAN_POST:
			*vp = BOOLEAN_TO_JSVAL(p->user==NULL || can_user_post(p->cfg, p->subnum, p->user, p->client,/* reason: */NULL));
			break;
		case SUB_PROP_IS_OPERATOR:
			*vp = BOOLEAN_TO_JSVAL(p->user == NULL || is_user_subop(p->cfg, p->subnum, p->user, p->client));
			break;
		case SUB_PROP_IS_MODERATED:
			if(p->cfg->sub[p->subnum]->mod_ar[0] != 0
				&& p->user != NULL 
				&& chk_ar(p->cfg,p->cfg->sub[p->subnum]->mod_ar, p->user, p->client))
				*vp = JSVAL_TRUE;
			else
				*vp = JSVAL_FALSE;
			break;
		case SUB_PROP_SCAN_PTR:
			if(scan != NULL) *vp=UINT_TO_JSVAL(scan->ptr);
			break;
		case SUB_PROP_SCAN_CFG:
			if(scan != NULL) *vp=UINT_TO_JSVAL(scan->cfg);
			break;
		case SUB_PROP_LAST_READ:
			if(scan != NULL) *vp=UINT_TO_JSVAL(scan->last);
			break;
		case SUB_PROP_POSTS:
			*vp=UINT_TO_JSVAL(getposts(p->cfg, p->subnum));
			break;
	}

	return(JS_TRUE);
}

static JSBool js_sub_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsval idval;
	int32		val=0;
    jsint       tiny;
	struct js_msg_area_priv *p;

	if((p=(struct js_msg_area_priv*)JS_GetPrivate(cx, obj))==NULL)
		return JS_FALSE;

	subscan_t*	scan = p->subscan;
	if(scan == NULL)
		return JS_TRUE;

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch(tiny) {
		case SUB_PROP_SCAN_PTR:
			if(!JS_ValueToInt32(cx, *vp, (int32*)&scan->ptr))
				return JS_FALSE;
			break;
		case SUB_PROP_SCAN_CFG:
			if(!JS_ValueToInt32(cx, *vp, &val))
				return JS_FALSE;
			scan->cfg=(ushort)val;
			break;
		case SUB_PROP_LAST_READ:
			if(!JS_ValueToInt32(cx, *vp, (int32*)&scan->last))
				return JS_FALSE;
			break;
	}

	return(JS_TRUE);
}

static struct JSPropertySpec js_sub_properties[] = {
/*		 name				,tinyid		,flags	*/

	{	"can_access"	,SUB_PROP_CAN_ACCESS	,JSPROP_ENUMERATE|JSPROP_SHARED|JSPROP_READONLY },
	{	"can_read"		,SUB_PROP_CAN_READ		,JSPROP_ENUMERATE|JSPROP_SHARED|JSPROP_READONLY },
	{	"can_post"		,SUB_PROP_CAN_POST		,JSPROP_ENUMERATE|JSPROP_SHARED|JSPROP_READONLY },
	{	"is_operator"	,SUB_PROP_IS_OPERATOR	,JSPROP_ENUMERATE|JSPROP_SHARED|JSPROP_READONLY },
	{	"is_moderated"	,SUB_PROP_IS_MODERATED	,JSPROP_ENUMERATE|JSPROP_SHARED|JSPROP_READONLY },
	{	"scan_ptr"		,SUB_PROP_SCAN_PTR		,JSPROP_ENUMERATE|JSPROP_SHARED },
	{	"scan_cfg"		,SUB_PROP_SCAN_CFG		,JSPROP_ENUMERATE|JSPROP_SHARED },
	{	"last_read"		,SUB_PROP_LAST_READ		,JSPROP_ENUMERATE|JSPROP_SHARED },
	{	"posts"			,SUB_PROP_POSTS			,JSPROP_ENUMERATE|JSPROP_SHARED|JSPROP_READONLY },
	{0}
};

static void 
js_msg_area_finalize(JSContext *cx, JSObject *obj)
{
	struct js_msg_area_priv *p;

	if((p=(struct js_msg_area_priv*)JS_GetPrivate(cx,obj))==NULL)
		return;

	free(p);
	JS_SetPrivate(cx,obj,NULL);
}

static JSClass js_sub_class = {
     "MsgSub"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_sub_get				/* getProperty	*/
	,js_sub_set				/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_msg_area_finalize	/* finalize		*/
};

JSBool DLLCALL js_msg_area_resolve(JSContext* cx, JSObject* areaobj, jsid id)
{
	JSObject*	allgrps;
	JSObject*	allsubs;
	JSObject*	grpobj_proto;
	JSObject*	grpobj;
	JSObject*	subobj_proto;
	JSObject*	subobj;
	JSObject*	grp_list;
	JSObject*	sub_list;
	JSString*	js_str;
	jsval		val;
	jsint		grp_index;
	jsint		sub_index;
	uint		l,d;
	char*		name=NULL;
	struct js_msg_area_priv *p;

	if((p=(struct js_msg_area_priv*)JS_GetPrivate(cx,areaobj))==NULL)
		return JS_FALSE;

	if(id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		
		JS_IdToValue(cx, id, &idval);
		if(JSVAL_IS_STRING(idval))
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,areaobj,"Message Areas",310);
#endif

	/* msg_area.properties */
	if (name==NULL || strcmp(name, "settings")==0) {
		if (name)
			free(name);
		if(!JS_NewNumberValue(cx,p->cfg->msg_misc,&val))
			return JS_FALSE;
		if(!JS_SetProperty(cx, areaobj, "settings", &val)) 
			return JS_FALSE;
		if (name)
			return JS_TRUE;
	}
	if (name==NULL || strcmp(name, "fido_netmail_settings")==0) {
		if (name)
			free(name);
		if(!JS_NewNumberValue(cx,p->cfg->netmail_misc,&val))
			return JS_FALSE;
		if(!JS_SetProperty(cx, areaobj, "fido_netmail_settings", &val)) 
			return JS_FALSE;
		if (name)
			return JS_TRUE;
	}
	if (name==NULL || strcmp(name, "inet_netmail_settings")==0) {
		if (name)
			free(name);
		if(!JS_NewNumberValue(cx,p->cfg->inetmail_misc,&val))
			return JS_FALSE;
		if(!JS_SetProperty(cx, areaobj, "inet_netmail_settings", &val)) 
			return JS_FALSE;
		if (name)
			return JS_TRUE;
	}

	if (name==NULL || strcmp(name, "grp")==0 || strcmp(name, "sub")==0 || strcmp(name, "grp_list")==0) {
		if (name)
			FREE_AND_NULL(name);
		/* msg_area.grp[] */
		if((allgrps=JS_NewObject(cx, NULL, NULL, areaobj))==NULL)
			return JS_FALSE;

		val=OBJECT_TO_JSVAL(allgrps);
		if(!JS_SetProperty(cx, areaobj, "grp", &val))
			return JS_FALSE;

		/* msg_area.sub[] */
		if((allsubs=JS_NewObject(cx, NULL, NULL, areaobj))==NULL)
			return JS_FALSE;

		val=OBJECT_TO_JSVAL(allsubs);
		if(!JS_SetProperty(cx, areaobj, "sub", &val))
			return JS_FALSE;

		/* msg_area.grp_list[] */
		if((grp_list=JS_NewArrayObject(cx, 0, NULL))==NULL) 
			return JS_FALSE;

		val=OBJECT_TO_JSVAL(grp_list);
		if(!JS_SetProperty(cx, areaobj, "grp_list", &val)) 
			return JS_FALSE;

		if((grpobj_proto=JS_NewObject(cx, NULL, NULL, areaobj))==NULL)
			return JS_FALSE;
		if((subobj_proto=JS_NewObject(cx, NULL, NULL, areaobj))==NULL)
			return JS_FALSE;
		for(l=0;l<p->cfg->total_grps;l++) {

			if((grpobj=JS_NewObject(cx, NULL, grpobj_proto, NULL))==NULL)
				return JS_FALSE;

			val=OBJECT_TO_JSVAL(grpobj);
			grp_index=-1;
			if(p->user==NULL || chk_ar(p->cfg,p->cfg->grp[l]->ar,p->user,p->client)) {

				if(!JS_GetArrayLength(cx, grp_list, (jsuint*)&grp_index))
					return JS_FALSE;

				if(!JS_SetElement(cx, grp_list, grp_index, &val))
					return JS_FALSE;
			}

			/* Add as property (associative array element) */
			if(!JS_DefineProperty(cx, allgrps, p->cfg->grp[l]->sname, val
				,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE))
				return JS_FALSE;

			val=INT_TO_JSVAL(grp_index);
			if(!JS_SetProperty(cx, grpobj, "index", &val))
				return JS_FALSE;

			val=INT_TO_JSVAL(l);
			if(!JS_SetProperty(cx, grpobj, "number", &val))
				return JS_FALSE;

			if((js_str=JS_NewStringCopyZ(cx, p->cfg->grp[l]->sname))==NULL)
				return JS_FALSE;
			val=STRING_TO_JSVAL(js_str);
			if(!JS_SetProperty(cx, grpobj, "name", &val))
				return JS_FALSE;

			if((js_str=JS_NewStringCopyZ(cx, p->cfg->grp[l]->lname))==NULL)
				return JS_FALSE;
			val=STRING_TO_JSVAL(js_str);
			if(!JS_SetProperty(cx, grpobj, "description", &val))
				return JS_FALSE;

			if((js_str=JS_NewStringCopyZ(cx, p->cfg->grp[l]->arstr))==NULL)
				return JS_FALSE;
			if(!JS_DefineProperty(cx, grpobj, "ars", STRING_TO_JSVAL(js_str)
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
				return JS_FALSE;

			val = BOOLEAN_TO_JSVAL(grp_index >= 0);
			if(!JS_SetProperty(cx, grpobj, "can_access", &val))
				return JS_FALSE;

			if((js_str=JS_NewStringCopyZ(cx, p->cfg->grp[l]->code_prefix))==NULL)
				return JS_FALSE;
			val=STRING_TO_JSVAL(js_str);
			if(!JS_SetProperty(cx, grpobj, "code_prefix", &val))
				return JS_FALSE;

#ifdef BUILD_JSDOCS
			js_DescribeSyncObject(cx,grpobj,"Message Groups (current user has access to)",310);
#endif

			/* sub_list[] */
			if((sub_list=JS_NewArrayObject(cx, 0, NULL))==NULL) 
				return JS_FALSE;

			val=OBJECT_TO_JSVAL(sub_list);
			if(!JS_SetProperty(cx, grpobj, "sub_list", &val)) 
				return JS_FALSE;

			for(d=0;d<p->cfg->total_subs;d++) {
				if(p->cfg->sub[d]->grp!=l)
					continue;
				if((subobj=JS_NewObject(cx, &js_sub_class, subobj_proto, NULL))==NULL)
					return JS_FALSE;
				struct js_msg_area_priv *np = malloc(sizeof(struct js_msg_area_priv));
				if(np == NULL)
					continue;
				*np = *p;
				if(p->subscan != NULL)
					np->subscan = &p->subscan[d];
				np->subnum = d;
				JS_SetPrivate(cx, subobj, np);

				val=OBJECT_TO_JSVAL(subobj);
				sub_index=-1;
				if(p->user==NULL || can_user_access_sub(p->cfg,d,p->user,p->client)) {

					if(!JS_GetArrayLength(cx, sub_list, (jsuint*)&sub_index))
						return JS_FALSE;

					if(!JS_SetElement(cx, sub_list, sub_index, &val))
						return JS_FALSE;
				}

				/* Add as property (associative array element) */
				if(!JS_DefineProperty(cx, allsubs, p->cfg->sub[d]->code, val
					,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE))
					return JS_FALSE;

				val=INT_TO_JSVAL(sub_index);
				if(!JS_SetProperty(cx, subobj, "index", &val))
					return JS_FALSE;

				val=INT_TO_JSVAL(grp_index);
				if(!JS_SetProperty(cx, subobj, "grp_index", &val))
					return JS_FALSE;

				if(!js_CreateMsgAreaProperties(cx, p->cfg, subobj, d))
					return JS_FALSE;

				if(!JS_DefineProperties(cx, subobj, js_sub_properties))
					return JS_FALSE;

#ifdef BUILD_JSDOCS
				js_DescribeSyncObject(cx,subobj,"Message Sub-boards (current user has access to)</h2>"
					"(all properties are <small>READ ONLY</small> except for "
					"<i>scan_ptr</i>, <i>scan_cfg</i>, and <i>last_read</i>)"
					,310);
#endif

			}

#ifdef BUILD_JSDOCS
			js_CreateArrayOfStrings(cx, grpobj, "_property_desc_list", msg_grp_prop_desc, JSPROP_READONLY);
#endif
		}

#ifdef BUILD_JSDOCS
		js_CreateArrayOfStrings(cx, areaobj, "_property_desc_list", msg_area_prop_desc, JSPROP_READONLY);

		js_DescribeSyncObject(cx,allgrps,"Associative array of all groups (use name as index)",312);
		JS_DefineProperty(cx,allgrps,"_dont_document",JSVAL_TRUE,NULL,NULL,JSPROP_READONLY);
#endif

#ifdef BUILD_JSDOCS
		js_DescribeSyncObject(cx,allsubs,"Associative array of all sub-boards (use internal code as index)",311);
		JS_DefineProperty(cx,allsubs,"_dont_document",JSVAL_TRUE,NULL,NULL,JSPROP_READONLY);
#endif
	}
	if(name)
		free(name);

	return JS_TRUE;
}

static JSBool js_msg_area_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_msg_area_resolve(cx, obj, JSID_VOID));
}

static JSClass js_msg_area_class = {
     "MsgArea"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,JS_PropertyStub		/* getProperty	*/
	,JS_StrictPropertyStub		/* setProperty	*/
	,js_msg_area_enumerate	/* enumerate	*/
	,js_msg_area_resolve	/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_msg_area_finalize	/* finalize		*/
};

JSObject* DLLCALL js_CreateMsgAreaObject(JSContext* cx, JSObject* parent, scfg_t* cfg
								  ,user_t* user, client_t* client, subscan_t* subscan)
{
	JSObject* obj;
	struct js_msg_area_priv *p;

	obj = JS_DefineObject(cx, parent, "msg_area", &js_msg_area_class, NULL
		,JSPROP_ENUMERATE|JSPROP_READONLY);

	if(obj==NULL)
		return(NULL);

	p = (struct js_msg_area_priv *)malloc(sizeof(struct js_msg_area_priv));
	if (p == NULL)
		return NULL;

	memset(p,0,sizeof(*p));
	p->cfg = cfg;
	p->user = user;
	p->client = client;
	p->subscan = subscan;

	if(!JS_SetPrivate(cx, obj, p)) {
		free(p);
		return(NULL);
	}

#ifdef BUILD_JSDOCS
	// Ensure they're all created for JSDOCS
	js_msg_area_enumerate(cx, obj);
#endif

	return(obj);
}

#endif	/* JAVSCRIPT */
