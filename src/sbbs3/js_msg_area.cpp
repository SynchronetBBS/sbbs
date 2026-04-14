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

static const char* msg_area_prop_desc[] = {
	"Message area settings (bit-flags) - see <tt>MM_*</tt> in <tt>sbbsdefs.js</tt> for details"
	, "FidoNet NetMail settings (bit-flags) - see <tt>NMAIL_*</tt> in <tt>sbbsdefs.js</tt> for details"
	, "Internet NetMail settings (bit-flags) - see <tt>NMAIL_*</tt> in <tt>sbbsdefs.js</tt> for details"
	, NULL
};

static const char* msg_grp_prop_desc[] = {
	"Index into grp_list array (or -1 if not in array)"
	, "Unique (zero-based) number for this message group"
	, "Group name"
	, "Group description"
	, "Group access requirements"
	, "User has sufficient access to list this group's sub-boards"
	, "Internal code prefix (for sub-boards)"
	, NULL
};

static const char* msg_sub_prop_desc[] = {

	"Index into sub_list array (or -1 if not in array)</i>"
	, "Group's index into grp_list array</i>"
	, "Unique (zero-based) number for this sub-board"
	, "Group number"
	, "Group name</i>"
	, "Sub-board internal code"
	, "Sub-board name"
	, "Sub-board description"
	, "QWK conference name"
	, "Area tag for FidoNet-style echoes, a.k.a. EchoTag"
	, "Newsgroup name (as configured or dynamically generated)"
	, "Sub-board access requirements"
	, "Sub-board reading requirements"
	, "Sub-board posting requirements"
	, "Sub-board operator requirements"
	, "Sub-board moderated-user requirements (if non-blank)"
	, "Sub-board data storage location"
	, "FidoNet node address"
	, "FidoNet origin line"
	, "QWK Network tagline"
	, "Toggle options (bit-flags) - see <tt>SUB_*</tt> in <tt>sbbsdefs.js</tt> for details"
	, "Index into message scan configuration/pointer file"
	, "QWK conference number"
	, "Configured maximum number of message CRCs to store (for dupe checking)"
	, "Configured maximum number of messages before purging"
	, "Configured maximum age (in days) of messages before expiration"
	, "Additional print mode flags to use when printing messages - see <tt>P_*</tt> in <tt>sbbsdefs.js</tt> for details"
	, "Print mode flags to <i>negate</i> when printing messages - see <tt>P_*</tt> in <tt>sbbsdefs.js</tt> for details"
	/* Insert here */
	, "User has sufficient access to see this sub-board"
	, "User has sufficient access to read messages in this sub-board"
	, "User has sufficient access to post messages in this sub-board"
	, "User has operator access to this sub-board"
	, "User's posts are moderated"
	, "User's current new message scan pointer (highest-read message number)"
	, "User's message scan configuration (bit-flags) - see <tt>SCAN_CFG_*</tt> in <tt>sbbsdefs.js</tt> for details"
	, "User's last-read message number"
	, "Number of messages currently posted to this sub-board"
	, NULL
};
#endif

struct js_msg_area_priv {
	scfg_t *cfg;
	user_t *user;
	client_t *client;
	subscan_t *subscan;
	int subnum;
};

bool js_CreateMsgAreaProperties(JSContext* cx, scfg_t* cfg, JSObject* subobj, int subnum)
{
	char      str[128];
	JSString* js_str;
	jsval     val;
	sub_t*    sub;

	if (!subnum_is_valid(cfg, subnum))
		return false;

	sub = cfg->sub[subnum];

	if (!JS_DefineProperty(cx, subobj, "number", INT_TO_JSVAL(subnum)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if (!JS_DefineProperty(cx, subobj, "grp_number", INT_TO_JSVAL(sub->grp)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if ((js_str = JS_NewStringCopyZ(cx, cfg->grp[sub->grp]->sname)) == NULL)
		return false;
	if (!JS_DefineProperty(cx, subobj, "grp_name", STRING_TO_JSVAL(js_str)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if ((js_str = JS_NewStringCopyZ(cx, sub->code)) == NULL)
		return false;
	if (!JS_DefineProperty(cx, subobj, "code", STRING_TO_JSVAL(js_str)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if ((js_str = JS_NewStringCopyZ(cx, sub->sname)) == NULL)
		return false;
	if (!JS_DefineProperty(cx, subobj, "name", STRING_TO_JSVAL(js_str)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if ((js_str = JS_NewStringCopyZ(cx, sub->lname)) == NULL)
		return false;
	if (!JS_DefineProperty(cx, subobj, "description", STRING_TO_JSVAL(js_str)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if ((js_str = JS_NewStringCopyZ(cx, sub->qwkname)) == NULL)
		return false;
	if (!JS_DefineProperty(cx, subobj, "qwk_name", STRING_TO_JSVAL(js_str)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if ((js_str = JS_NewStringCopyZ(cx, sub_area_tag(cfg, sub, str, sizeof(str)))) == NULL)
		return false;
	if (!JS_DefineProperty(cx, subobj, "area_tag", STRING_TO_JSVAL(js_str)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if ((js_str = JS_NewStringCopyZ(cx, sub_newsgroup_name(cfg, sub, str, sizeof(str)))) == NULL)
		return false;
	if (!JS_DefineProperty(cx, subobj, "newsgroup", STRING_TO_JSVAL(js_str)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if ((js_str = JS_NewStringCopyZ(cx, sub->arstr)) == NULL)
		return false;
	if (!JS_DefineProperty(cx, subobj, "ars", STRING_TO_JSVAL(js_str)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if ((js_str = JS_NewStringCopyZ(cx, sub->read_arstr)) == NULL)
		return false;
	if (!JS_DefineProperty(cx, subobj, "read_ars", STRING_TO_JSVAL(js_str)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if ((js_str = JS_NewStringCopyZ(cx, sub->post_arstr)) == NULL)
		return false;
	if (!JS_DefineProperty(cx, subobj, "post_ars", STRING_TO_JSVAL(js_str)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if ((js_str = JS_NewStringCopyZ(cx, sub->op_arstr)) == NULL)
		return false;
	if (!JS_DefineProperty(cx, subobj, "operator_ars", STRING_TO_JSVAL(js_str)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if ((js_str = JS_NewStringCopyZ(cx, sub->mod_arstr)) == NULL)
		return false;
	if (!JS_DefineProperty(cx, subobj, "moderated_ars", STRING_TO_JSVAL(js_str)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if ((js_str = JS_NewStringCopyZ(cx, sub->data_dir)) == NULL)
		return false;
	if (!JS_DefineProperty(cx, subobj, "data_dir", STRING_TO_JSVAL(js_str)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if ((js_str = JS_NewStringCopyZ(cx, smb_faddrtoa(&sub->faddr, str))) == NULL)
		return false;
	if (!JS_DefineProperty(cx, subobj, "fidonet_addr", STRING_TO_JSVAL(js_str)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if ((js_str = JS_NewStringCopyZ(cx, sub->origline)) == NULL)
		return false;
	if (!JS_DefineProperty(cx, subobj, "fidonet_origin", STRING_TO_JSVAL(js_str)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if ((js_str = JS_NewStringCopyZ(cx, sub->tagline)) == NULL)
		return false;
	if (!JS_DefineProperty(cx, subobj, "qwknet_tagline", STRING_TO_JSVAL(js_str)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	val = UINT_TO_JSVAL(sub->misc);
	if (!JS_DefineProperty(cx, subobj, "settings", val
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if (!JS_DefineProperty(cx, subobj, "ptridx", INT_TO_JSVAL(sub->ptridx)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if (!JS_DefineProperty(cx, subobj, "qwk_conf", INT_TO_JSVAL(sub->qwkconf)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if (!JS_DefineProperty(cx, subobj, "max_crcs", INT_TO_JSVAL(sub->maxcrcs)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if (!JS_DefineProperty(cx, subobj, "max_msgs", INT_TO_JSVAL(sub->maxmsgs)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if (!JS_DefineProperty(cx, subobj, "max_age", INT_TO_JSVAL(sub->maxage)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if (!JS_DefineProperty(cx, subobj, "print_mode", INT_TO_JSVAL(sub->pmode)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;

	if (!JS_DefineProperty(cx, subobj, "print_mode_neg", INT_TO_JSVAL(sub->n_pmode)
	                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
		return false;


#ifdef BUILD_JSDOCS
	js_CreateArrayOfStrings(cx, subobj, "_property_desc_list", msg_sub_prop_desc, JSPROP_READONLY);
#endif

	return true;
}

/***************************************/
/* Dynamic Sub-board Object Properites */
/***************************************/
enum {
	SUB_PROP_CAN_ACCESS
	, SUB_PROP_CAN_READ
	, SUB_PROP_CAN_POST
	, SUB_PROP_IS_OPERATOR
	, SUB_PROP_IS_MODERATED
	, SUB_PROP_SCAN_PTR
	, SUB_PROP_SCAN_CFG
	, SUB_PROP_LAST_READ
	, SUB_PROP_POSTS
};

static JSBool js_sub_get_value(JSContext* cx, struct js_msg_area_priv* p, jsint tiny, jsval* vp)
{
	subscan_t* scan = p->subscan;

	switch (tiny) {
		case SUB_PROP_CAN_ACCESS:
			*vp = BOOLEAN_TO_JSVAL(p->user == NULL || user_can_access_sub(p->cfg, p->subnum, p->user, p->client));
			break;
		case SUB_PROP_CAN_READ:
			*vp = BOOLEAN_TO_JSVAL(p->user == NULL || user_can_read_sub(p->cfg, p->subnum, p->user, p->client));
			break;
		case SUB_PROP_CAN_POST:
			*vp = BOOLEAN_TO_JSVAL(p->user == NULL || user_can_post(p->cfg, p->subnum, p->user, p->client, /* reason: */ NULL));
			break;
		case SUB_PROP_IS_OPERATOR:
			*vp = BOOLEAN_TO_JSVAL(p->user == NULL || user_is_subop(p->cfg, p->subnum, p->user, p->client));
			break;
		case SUB_PROP_IS_MODERATED:
			if (p->cfg->sub[p->subnum]->mod_ar[0] != 0
			    && p->user != NULL
			    && chk_ar(p->cfg, p->cfg->sub[p->subnum]->mod_ar, p->user, p->client))
				*vp = JSVAL_TRUE;
			else
				*vp = JSVAL_FALSE;
			break;
		case SUB_PROP_SCAN_PTR:
			if (scan != NULL)
				*vp = UINT_TO_JSVAL(scan->ptr);
			break;
		case SUB_PROP_SCAN_CFG:
			if (scan != NULL)
				*vp = UINT_TO_JSVAL(scan->cfg);
			break;
		case SUB_PROP_LAST_READ:
			if (scan != NULL)
				*vp = UINT_TO_JSVAL(scan->last);
			break;
		case SUB_PROP_POSTS:
			*vp = UINT_TO_JSVAL(getposts(p->cfg, p->subnum));
			break;
		default:
			return JS_TRUE;
	}
	return JS_TRUE;
}

static JSBool js_sub_set_value(JSContext* cx, struct js_msg_area_priv* p, jsint tiny, jsval* vp)
{
	uint32_t                 val = 0;

	subscan_t* scan = p->subscan;
	if (scan == NULL)
		return JS_TRUE;

	switch (tiny) {
		case SUB_PROP_SCAN_PTR:
			if (!JS_ValueToECMAUint32(cx, *vp, &scan->ptr))
				return JS_FALSE;
			break;
		case SUB_PROP_SCAN_CFG:
			if (!JS_ValueToECMAUint32(cx, *vp, &val))
				return JS_FALSE;
			scan->cfg = (ushort)val;
			break;
		case SUB_PROP_LAST_READ:
			if (!JS_ValueToECMAUint32(cx, *vp, &scan->last))
				return JS_FALSE;
			break;
	}

	return JS_TRUE;
}

static jsSyncPropertySpec js_sub_properties[] = {
/*		 name				,tinyid		,flags	*/

	{   "can_access", SUB_PROP_CAN_ACCESS, JSPROP_ENUMERATE  | JSPROP_READONLY },
	{   "can_read", SUB_PROP_CAN_READ, JSPROP_ENUMERATE  | JSPROP_READONLY },
	{   "can_post", SUB_PROP_CAN_POST, JSPROP_ENUMERATE  | JSPROP_READONLY },
	{   "is_operator", SUB_PROP_IS_OPERATOR, JSPROP_ENUMERATE  | JSPROP_READONLY },
	{   "is_moderated", SUB_PROP_IS_MODERATED, JSPROP_ENUMERATE  | JSPROP_READONLY },
	{   "scan_ptr", SUB_PROP_SCAN_PTR, JSPROP_ENUMERATE  },
	{   "scan_cfg", SUB_PROP_SCAN_CFG, JSPROP_ENUMERATE  },
	{   "last_read", SUB_PROP_LAST_READ, JSPROP_ENUMERATE  },
	{   "posts", SUB_PROP_POSTS, JSPROP_ENUMERATE  | JSPROP_READONLY },
	{0}
};

static bool js_sub_prop_getter(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject thisObj(cx);
	if (!args.computeThis(cx, &thisObj))
		return false;
	struct js_msg_area_priv* p = (struct js_msg_area_priv*)JS_GetPrivate(thisObj);
	if (p == nullptr) {
		args.rval().setUndefined();
		return true;
	}
	JSObject* callee = &args.callee();
	jsint tiny = js::GetFunctionNativeReserved(callee, 0).toInt32();
	jsval val = JSVAL_VOID;
	if (!js_sub_get_value(cx, p, tiny, &val))
		return false;
	args.rval().set(val);
	return true;
}

static bool js_sub_prop_setter(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject thisObj(cx);
	if (!args.computeThis(cx, &thisObj))
		return false;
	struct js_msg_area_priv* p = (struct js_msg_area_priv*)JS_GetPrivate(thisObj);
	if (p == nullptr)
		return true;
	JSObject* callee = &args.callee();
	jsint tiny = js::GetFunctionNativeReserved(callee, 0).toInt32();
	jsval val = args.length() > 0 ? args[0] : JSVAL_VOID;
	if (!js_sub_set_value(cx, p, tiny, &val))
		return false;
	args.rval().set(val);
	return true;
}

static JSBool js_sub_fill_properties(JSContext* cx, JSObject* obj, struct js_msg_area_priv* p, const char* name)
{
	(void)p;
	return js_DefineSyncAccessors(cx, obj, js_sub_properties, js_sub_prop_getter, name, js_sub_prop_setter);
}

static void
js_msg_area_finalize(JS::GCContext* gcx, JSObject *obj)
{
	struct js_msg_area_priv *p;

	if ((p = (struct js_msg_area_priv*)JS_GetPrivate(obj)) == NULL)
		return;

	free(p);
	JS_SetPrivate(obj, NULL);
}

static const JSClassOps js_sub_classops = {
	nullptr,                /* addProperty  */
	nullptr,                /* delProperty  */
	nullptr,                /* enumerate    */
	nullptr,                /* newEnumerate */
	nullptr,                /* resolve      */
	nullptr,                /* mayResolve   */
	js_msg_area_finalize,   /* finalize     */
	nullptr, nullptr, nullptr /* call, construct, trace */
};

static JSClass js_sub_class = {
	"MsgSub"
	, JSCLASS_HAS_PRIVATE | JSCLASS_FOREGROUND_FINALIZE
	, &js_sub_classops
};

static bool js_msg_area_resolve_impl(JSContext* cx, JSObject* areaobj, char* name);

bool js_msg_area_resolve(JSContext* cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, bool* resolvedp)
{
	char* name = NULL;

	if (id.get().isString()) {
		JSString* str = id.get().toString();
		JSSTRING_TO_MSTRING(cx, str, name, NULL);
		HANDLE_PENDING(cx, name);
		if (name == NULL) return false;
	}

	bool ret = js_msg_area_resolve_impl(cx, obj, name);
	if (name)
		free(name);
	if (resolvedp) *resolvedp = ret;
	/* SM128: resolve hook must not return true with a pending exception. */
	if (JS_IsExceptionPending(cx))
		JS_ClearPendingException(cx);
	return true;
}

static bool js_msg_area_resolve_impl(JSContext* cx, JSObject* areaobj, char* name)
{
	/* SM128: root all JSObject/JSString locals to prevent GC crashes */
	JS::RootedObject         allgrps(cx);
	JS::RootedObject         allsubs(cx);
	JS::RootedObject         grpobj_proto(cx);
	JS::RootedObject         grpobj(cx);
	JS::RootedObject         subobj_proto(cx);
	JS::RootedObject         subobj(cx);
	JS::RootedObject         grp_list(cx);
	JS::RootedObject         sub_list(cx);
	JSString*                js_str;
	jsval                    val;
	jsint                    grp_index;
	jsint                    sub_index;
	int                      l, d;
	struct js_msg_area_priv *p;

	if ((p = (struct js_msg_area_priv*)JS_GetPrivate(cx, areaobj)) == NULL)
		return false;

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, areaobj, "Message Areas", 310);
#endif

	/* msg_area.properties */
	if (name == NULL || strcmp(name, "settings") == 0) {
		if (!JS_NewNumberValue(cx, p->cfg->msg_misc, &val))
			return false;
		if (!JS_SetProperty(cx, areaobj, "settings", &val))
			return false;
		if (name)
			return true;
	}
	if (name == NULL || strcmp(name, "fido_netmail_settings") == 0) {
		if (!JS_NewNumberValue(cx, p->cfg->netmail_misc, &val))
			return false;
		if (!JS_SetProperty(cx, areaobj, "fido_netmail_settings", &val))
			return false;
		if (name)
			return true;
	}
	if (name == NULL || strcmp(name, "inet_netmail_settings") == 0) {
		if (!JS_NewNumberValue(cx, p->cfg->inetmail_misc, &val))
			return false;
		if (!JS_SetProperty(cx, areaobj, "inet_netmail_settings", &val))
			return false;
		if (name)
			return true;
	}

	if (name == NULL || strcmp(name, "grp") == 0 || strcmp(name, "sub") == 0 || strcmp(name, "grp_list") == 0) {
		/* msg_area.grp[] */
		if ((allgrps = JS_NewObject(cx, NULL, NULL, areaobj)) == nullptr)
			return false;

		val = OBJECT_TO_JSVAL(allgrps);
		if (!JS_SetProperty(cx, areaobj, "grp", &val))
			return false;

		/* msg_area.sub[] */
		if ((allsubs = JS_NewObject(cx, NULL, NULL, areaobj)) == nullptr)
			return false;

		val = OBJECT_TO_JSVAL(allsubs);
		if (!JS_SetProperty(cx, areaobj, "sub", &val))
			return false;

		/* msg_area.grp_list[] */
		if ((grp_list = JS_NewArrayObject(cx, 0, NULL)) == nullptr)
			return false;

		val = OBJECT_TO_JSVAL(grp_list);
		if (!JS_SetProperty(cx, areaobj, "grp_list", &val))
			return false;

		if ((grpobj_proto = JS_NewObject(cx, NULL, NULL, areaobj)) == nullptr)
			return false;
		if ((subobj_proto = JS_NewObject(cx, NULL, NULL, areaobj)) == nullptr)
			return false;
		for (l = 0; l < p->cfg->total_grps; l++) {

			if ((grpobj = JS_NewObject(cx, NULL, grpobj_proto, NULL)) == nullptr)
				return false;

			val = OBJECT_TO_JSVAL(grpobj);
			grp_index = -1;
			if (p->user == NULL || user_can_access_grp(p->cfg, l, p->user, p->client)) {

				if (!JS_GetArrayLength(cx, grp_list, (jsuint*)&grp_index))
					return false;

				if (!JS_SetElement(cx, grp_list, grp_index, &val))
					return false;
			}

			/* Add as property (associative array element) */
			if (!JS_DefineProperty(cx, allgrps, p->cfg->grp[l]->sname, val
			                       , NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE))
				return false;

			val = INT_TO_JSVAL(grp_index);
			if (!JS_SetProperty(cx, grpobj, "index", &val))
				return false;

			val = INT_TO_JSVAL(l);
			if (!JS_SetProperty(cx, grpobj, "number", &val))
				return false;

			if ((js_str = JS_NewStringCopyZ(cx, p->cfg->grp[l]->sname)) == NULL)
				return false;
			val = STRING_TO_JSVAL(js_str);
			if (!JS_SetProperty(cx, grpobj, "name", &val))
				return false;

			if ((js_str = JS_NewStringCopyZ(cx, p->cfg->grp[l]->lname)) == NULL)
				return false;
			val = STRING_TO_JSVAL(js_str);
			if (!JS_SetProperty(cx, grpobj, "description", &val))
				return false;

			if ((js_str = JS_NewStringCopyZ(cx, p->cfg->grp[l]->arstr)) == NULL)
				return false;
			if (!JS_DefineProperty(cx, grpobj, "ars", STRING_TO_JSVAL(js_str)
			                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
				return false;

			val = BOOLEAN_TO_JSVAL(grp_index >= 0);
			if (!JS_SetProperty(cx, grpobj, "can_access", &val))
				return false;

			if ((js_str = JS_NewStringCopyZ(cx, p->cfg->grp[l]->code_prefix)) == NULL)
				return false;
			val = STRING_TO_JSVAL(js_str);
			if (!JS_SetProperty(cx, grpobj, "code_prefix", &val))
				return false;

#ifdef BUILD_JSDOCS
			js_DescribeSyncObject(cx, grpobj, "Message Groups (current user has access to)", 310);
#endif

			/* sub_list[] */
			if ((sub_list = JS_NewArrayObject(cx, 0, NULL)) == nullptr)
				return false;

			val = OBJECT_TO_JSVAL(sub_list);
			if (!JS_SetProperty(cx, grpobj, "sub_list", &val))
				return false;

			for (d = 0; d < p->cfg->total_subs; d++) {
				if (p->cfg->sub[d]->grp != l)
					continue;
				if ((subobj = JS_NewObject(cx, &js_sub_class, subobj_proto, NULL)) == nullptr)
					return false;
				struct js_msg_area_priv *np = static_cast<js_msg_area_priv *>(malloc(sizeof(struct js_msg_area_priv)));
				if (np == NULL)
					continue;
				*np = *p;
				if (p->subscan != NULL)
					np->subscan = &p->subscan[d];
				np->subnum = d;
				JS_SetPrivate(cx, subobj, np);

				val = OBJECT_TO_JSVAL(subobj);
				sub_index = -1;
				if (p->user == NULL || user_can_access_sub(p->cfg, d, p->user, p->client)) {

					if (!JS_GetArrayLength(cx, sub_list, (jsuint*)&sub_index))
						return false;

					if (!JS_SetElement(cx, sub_list, sub_index, &val))
						return false;
				}

				/* Add as property (associative array element) */
				if (!JS_DefineProperty(cx, allsubs, p->cfg->sub[d]->code, val
				                       , NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE))
					return false;

				val = INT_TO_JSVAL(sub_index);
				if (!JS_SetProperty(cx, subobj, "index", &val))
					return false;

				val = INT_TO_JSVAL(grp_index);
				if (!JS_SetProperty(cx, subobj, "grp_index", &val))
					return false;

				if (!js_CreateMsgAreaProperties(cx, p->cfg, subobj, d))
					return false;

				if (!js_sub_fill_properties(cx, subobj, np, NULL))
					return false;

#ifdef BUILD_JSDOCS
				js_DescribeSyncObject(cx, subobj, "Message Sub-boards (current user has access to)</h2>"
				                      "(all properties are <small>READ ONLY</small> except for "
				                      "<i>scan_ptr</i>, <i>scan_cfg</i>, and <i>last_read</i>)"
				                      , 310);
#endif

			}

#ifdef BUILD_JSDOCS
			js_CreateArrayOfStrings(cx, grpobj, "_property_desc_list", msg_grp_prop_desc, JSPROP_READONLY);
#endif
		}

#ifdef BUILD_JSDOCS
		js_CreateArrayOfStrings(cx, areaobj, "_property_desc_list", msg_area_prop_desc, JSPROP_READONLY);

		js_DescribeSyncObject(cx, allgrps, "Associative array of all groups (use name as index)", 312);
		JS_DefineProperty(cx, allgrps, "_dont_document", JSVAL_TRUE, NULL, NULL, JSPROP_READONLY);
#endif

#ifdef BUILD_JSDOCS
		js_DescribeSyncObject(cx, allsubs, "Associative array of all sub-boards (use internal code as index)", 311);
		JS_DefineProperty(cx, allsubs, "_dont_document", JSVAL_TRUE, NULL, NULL, JSPROP_READONLY);
#endif
	}

	return true;
}

static bool js_msg_area_enumerate(JSContext *cx, JS::Handle<JSObject*> obj)
{
	return js_msg_area_resolve_impl(cx, obj, NULL);
}

static const JSClassOps js_msg_area_classops = {
	nullptr,                    /* addProperty  */
	nullptr,                    /* delProperty  */
	js_msg_area_enumerate,      /* enumerate    */
	nullptr,                    /* newEnumerate */
	js_msg_area_resolve,        /* resolve      */
	nullptr,                    /* mayResolve   */
	js_msg_area_finalize,       /* finalize     */
	nullptr, nullptr, nullptr   /* call, construct, trace */
};

static JSClass js_msg_area_class = {
	"MsgArea"
	, JSCLASS_HAS_PRIVATE | JSCLASS_FOREGROUND_FINALIZE
	, &js_msg_area_classops
};

JSObject* js_CreateMsgAreaObject(JSContext* cx, JSObject* parent, scfg_t* cfg
                                 , user_t* user, client_t* client, subscan_t* subscan)
{
	JSObject*                obj;
	struct js_msg_area_priv *p;

	obj = JS_DefineObject(cx, parent, "msg_area", &js_msg_area_class, NULL
	                      , JSPROP_ENUMERATE | JSPROP_READONLY);

	if (obj == NULL)
		return NULL;

	p = (struct js_msg_area_priv *)malloc(sizeof(struct js_msg_area_priv));
	if (p == NULL)
		return NULL;

	memset(p, 0, sizeof(*p));
	p->cfg = cfg;
	p->user = user;
	p->client = client;
	p->subscan = subscan;

	if (!JS_SetPrivate(cx, obj, p)) {
		free(p);
		return NULL;
	}

#ifdef BUILD_JSDOCS
	// Ensure they're all created for JSDOCS
	js_msg_area_enumerate(cx, obj);
#endif

	return obj;
}

#endif  /* JAVSCRIPT */
