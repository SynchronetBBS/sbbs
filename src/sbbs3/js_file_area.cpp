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
#include "filedat.h"

#ifdef JAVASCRIPT

#ifdef BUILD_JSDOCS

static const char* file_area_prop_desc[] = {
	"Minimum amount of available disk space (in bytes) required for user uploads to be allowed"
	, "Maximum allowed length of filenames (in characters) uploaded by users"
	, "File area settings (bit-flags) - see <tt>FM_*</tt> in <tt>sbbsdefs.js</tt> for details"
	, "Web file virtual path prefix"
	, NULL
};

static const char* lib_prop_desc[] = {
	"Index into lib_list array (or -1 if not in array)"
	, "Unique (zero-based) number for this library"
	, "Name"
	, "Description"
	, "Access requirements"
	, "Virtual directory name (for FTP or Web access)"
	, "User has sufficient access to this library's directories"
	, "Internal code prefix (for directories)"
	, NULL
};

static const char* dir_prop_desc[] = {

	"Index into dir_list array (or -1 if not in array)"
	, "Unique (zero-based) number for this directory"
	, "Library index"
	, "Library number"
	, "Library name"
	, "Directory internal code"
	, "Directory name"
	, "Directory description"
	, "Directory area tag for file echoes"
	, "Directory file storage location"
	, "Directory access requirements"
	, "Directory upload requirements"
	, "Directory download requirements"
	, "Directory exemption requirements"
	, "Directory operator requirements"
	, "Allowed file extensions (comma delimited)"
	, "Upload semaphore file"
	, "Directory data storage location"
	, "Toggle options (bit-flags)"
	, "Sequential (slow storage) device number"
	, "Sort order (see <tt>FileBase.SORT</tt> for valid values)"
	, "Configured maximum number of files"
	, "Configured maximum age (in days) of files before expiration"
	, "Percent of file size awarded uploader in credits upon file upload"
	, "Percent of file size awarded uploader in credits upon subsequent downloads"
	, "Virtual directory name (for FTP or Web access)"
	, "Virtual path (for FTP or Web access), with trailing slash"
	, "Number of files currently in this directory"
	, "Time-stamp of file base index of this directory"
	, "User has sufficient access to view this directory (e.g. list files)"
	, "User has sufficient access to upload files to this directory"
	, "User has sufficient access to download files from this directory"
	, "User is exempt from download credit costs (or the directory is configured for free downloads)"
	, "User has operator access to this directory"
	, "Virtual shortcut (for FTP or Web access), optional"
	, "Directory is for off-line storage"
	, "Directory is for uploads only"
	, "Directory is for uploads to sysop only"
	, NULL
};
#endif

struct js_file_area_priv {
	scfg_t *cfg;
	user_t *user;
	client_t *client;
	const char *web_file_prefix;
	uint dirnum;
	/* SM128: JS_SetProperty/JS_DefineProperty on areaobj from within the
	 * resolver triggers the resolver again (SM128 calls the resolve hook
	 * during property lookup before any set/define).  Guard against re-entry
	 * so the outer invocation completes without infinite recursion. */
	bool resolving;
};

static void
js_file_area_finalize(JS::GCContext* gcx, JSObject *obj)
{
	struct js_file_area_priv *p;

	if ((p = (struct js_file_area_priv*)JS_GetPrivate(obj)) == NULL)
		return;

	free(p);
	JS_SetPrivate(obj, NULL);
}

/***************************************/
/* Dynamic Directory Object Properties */
/***************************************/
enum {
	DIR_PROP_FILES
	, DIR_PROP_UPDATE_TIME
	, DIR_PROP_CAN_ACCESS
	, DIR_PROP_CAN_UPLOAD
	, DIR_PROP_CAN_DOWNLOAD
	, DIR_PROP_IS_EXEMPT
	, DIR_PROP_IS_OPERATOR
};

static jsSyncPropertySpec js_dir_properties[] = {
/*		 name				,tinyid		,flags	*/

	{   "files", DIR_PROP_FILES, JSPROP_ENUMERATE  | JSPROP_READONLY },
	{   "update_time", DIR_PROP_UPDATE_TIME, JSPROP_ENUMERATE  | JSPROP_READONLY },
	{   "can_access", DIR_PROP_CAN_ACCESS, JSPROP_ENUMERATE  | JSPROP_READONLY },
	{   "can_upload", DIR_PROP_CAN_UPLOAD, JSPROP_ENUMERATE  | JSPROP_READONLY },
	{   "can_download", DIR_PROP_CAN_DOWNLOAD, JSPROP_ENUMERATE  | JSPROP_READONLY },
	{   "is_exempt", DIR_PROP_IS_EXEMPT, JSPROP_ENUMERATE  | JSPROP_READONLY },
	{   "is_operator", DIR_PROP_IS_OPERATOR, JSPROP_ENUMERATE  | JSPROP_READONLY },
	{0}
};

/* SM128: populate js_dir_properties values — called after JS_DefineProperties */
static JSBool js_dir_get_value(JSContext* cx, struct js_file_area_priv* p, jsint tiny, jsval* vp)
{
	switch (tiny) {
		case DIR_PROP_FILES:
			*vp = UINT_TO_JSVAL(getfiles(p->cfg, p->dirnum));
			break;
		case DIR_PROP_UPDATE_TIME:
			*vp = UINT_TO_JSVAL((uint32_t)dir_newfiletime(p->cfg, p->dirnum));
			break;
		case DIR_PROP_CAN_ACCESS:
			*vp = BOOLEAN_TO_JSVAL(p->user == NULL || user_can_access_dir(p->cfg, p->dirnum, p->user, p->client));
			break;
		case DIR_PROP_CAN_UPLOAD:
			*vp = BOOLEAN_TO_JSVAL(p->user == NULL || user_can_upload(p->cfg, p->dirnum, p->user, p->client, NULL));
			break;
		case DIR_PROP_CAN_DOWNLOAD:
			*vp = BOOLEAN_TO_JSVAL(p->user == NULL || user_can_download(p->cfg, p->dirnum, p->user, p->client, NULL));
			break;
		case DIR_PROP_IS_EXEMPT:
			*vp = BOOLEAN_TO_JSVAL(p->user == NULL || download_is_free(p->cfg, p->dirnum, p->user, p->client));
			break;
		case DIR_PROP_IS_OPERATOR:
			*vp = BOOLEAN_TO_JSVAL(p->user == NULL || user_is_dirop(p->cfg, p->dirnum, p->user, p->client));
			break;
		default:
			return JS_TRUE;
	}
	return JS_TRUE;
}

static bool js_dir_prop_getter(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject thisObj(cx);
	if (!args.computeThis(cx, &thisObj))
		return false;
	struct js_file_area_priv* p = (struct js_file_area_priv*)JS_GetPrivate(thisObj);
	if (p == nullptr) {
		args.rval().setUndefined();
		return true;
	}
	JSObject* callee = &args.callee();
	jsint tiny = js::GetFunctionNativeReserved(callee, 0).toInt32();
	jsval val = JSVAL_VOID;
	if (!js_dir_get_value(cx, p, tiny, &val))
		return false;
	args.rval().set(val);
	return true;
}

static JSBool js_dir_fill_properties(JSContext* cx, JSObject* obj, struct js_file_area_priv* p)
{
	(void)p;
	return js_DefineSyncAccessors(cx, obj, js_dir_properties, js_dir_prop_getter, NULL);
}

static const JSClassOps js_dir_classops = {
	nullptr,                /* addProperty  */
	nullptr,                /* delProperty  */
	nullptr,                /* enumerate    */
	nullptr,                /* newEnumerate */
	nullptr,                /* resolve      */
	nullptr,                /* mayResolve   */
	js_file_area_finalize,  /* finalize     */
	nullptr, nullptr, nullptr /* call, construct, trace */
};

static JSClass js_dir_class = {
	"FileDir"
	, JSCLASS_HAS_PRIVATE
	, &js_dir_classops
};

static bool js_file_area_resolve_impl(JSContext* cx, JSObject* areaobj, char* name);

bool js_file_area_resolve(JSContext* cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, bool* resolvedp)
{
	char* name = NULL;

	if (id.get().isString()) {
		JSString* str = id.get().toString();
		JSSTRING_TO_MSTRING(cx, str, name, NULL);
		HANDLE_PENDING(cx, name);
		if (name == NULL) return false;
	}

	bool ret = js_file_area_resolve_impl(cx, obj, name);
	if (name)
		free(name);
	if (resolvedp) *resolvedp = ret;
	return true;
}

static bool js_file_area_resolve_impl(JSContext* cx, JSObject* areaobj, char* name)
{
	char                      str[128];
	char                      vpath[128];
	/* SM128: all GC-managed locals must be JS::Rooted<T>.  The nursery GC
	 * moves objects; raw JSObject/JSString/jsval pointers become stale after
	 * any allocating call (JS_NewObject, JS_NewStringCopyZ, etc.). */
	JS::RootedObject          alllibs(cx);
	JS::RootedObject          alldirs(cx);
	JS::RootedObject          libobj(cx);
	JS::RootedObject          dirobj(cx);
	JS::RootedObject          lib_list(cx);
	JS::RootedObject          dir_list(cx);
	JS::RootedString          js_str(cx);
	JS::RootedValue           val(cx);
	jsint                     lib_index;
	jsint                     dir_index;
	int                       l, d;
	struct js_file_area_priv *p;

	if ((p = (struct js_file_area_priv*)JS_GetPrivate(cx, areaobj)) == NULL)
		return false;

	/* SM128: guard against re-entrant resolver calls.  Setting a property on
	 * areaobj from within the resolver causes SM128 to call the resolver again
	 * for each property being set.  Return false (not resolved) so the outer
	 * invocation's JS_SetProperty/JS_DefineProperty proceeds to define the
	 * property directly. */
	if (p->resolving)
		return false;

	/* RAII guard: resets p->resolving on any exit path (normal or error) */
	struct AutoResolving {
		bool& flag;
		AutoResolving(bool& f) : flag(f) { flag = true; }
		~AutoResolving() { flag = false; }
	} _ar(p->resolving);

	/* file_area.properties */
	if (name == NULL || strcmp(name, "min_diskspace") == 0) {
		val = DOUBLE_TO_JSVAL((jsdouble)p->cfg->min_dspace);
		JS_DefineProperty(cx, areaobj, "min_diskspace", val, NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
		if (name)
			return true;
	}

	if (name == NULL || strcmp(name, "max_filename_length") == 0) {
		val = UINT_TO_JSVAL(p->cfg->filename_maxlen);
		JS_DefineProperty(cx, areaobj, "max_filename_length", val, NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
		if (name)
			return true;
	}

	if (name == NULL || strcmp(name, "settings") == 0) {
		val = UINT_TO_JSVAL(p->cfg->file_misc);
		JS_DefineProperty(cx, areaobj, "settings", val, NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
		if (name)
			return true;
	}

	if (name == NULL || strcmp(name, "web_vpath_prefix") == 0) {
		if (p->web_file_prefix == NULL)
			val = JSVAL_VOID;
		else {
			if ((js_str = JS_NewStringCopyZ(cx, p->web_file_prefix)) == nullptr)
				return false;
			val = STRING_TO_JSVAL(js_str.get());
		}
		JS_DefineProperty(cx, areaobj, "web_vpath_prefix", val, NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
		if (name)
			return true;
	}

#ifdef BUILD_JSDOCS
	js_CreateArrayOfStrings(cx, areaobj, "_property_desc_list", file_area_prop_desc, JSPROP_READONLY);
#endif

	if (name == NULL || strcmp(name, "lib") == 0 || strcmp(name, "dir") == 0 || strcmp(name, "lib_list") == 0
	    || strcmp(name, "user_dir") == 0 || strcmp(name, "sysop_dir") == 0 || strcmp(name, "upload_dir") == 0
	    || strcmp(name, "offline_dir") == 0) {
		if ((alllibs = JS_NewObject(cx, NULL, NULL, areaobj)) == nullptr)
			return false;

		val = OBJECT_TO_JSVAL(alllibs.get());
		if (!JS_SetProperty(cx, areaobj, "lib", val.address()))
			return false;

		if ((alldirs = JS_NewObject(cx, NULL, NULL, areaobj)) == nullptr)
			return false;

		val = OBJECT_TO_JSVAL(alldirs.get());
		if (!JS_SetProperty(cx, areaobj, "dir", val.address()))
			return false;

#ifdef BUILD_JSDOCS
		js_DescribeSyncObject(cx, areaobj, "File Transfer Areas", 310);
#endif

		/* file_area.lib_list[] */
		if ((lib_list = JS_NewArrayObject(cx, 0, NULL)) == nullptr)
			return false;

		val = OBJECT_TO_JSVAL(lib_list.get());
		if (!JS_SetProperty(cx, areaobj, "lib_list", val.address()))
			return false;

		for (l = 0; l < p->cfg->total_libs; l++) {

			if ((libobj = JS_NewObject(cx, NULL, NULL, NULL)) == nullptr)
				return false;

			val = OBJECT_TO_JSVAL(libobj.get());
			lib_index = -1;
			if (p->user == NULL || user_can_access_lib(p->cfg, l, p->user, p->client)) {

				if (!JS_GetArrayLength(cx, lib_list.get(), (jsuint*)&lib_index))
					return false;

				if (!JS_SetElement(cx, lib_list.get(), lib_index, val.address()))
					return false;
			}

			/* Add as property (associative array element) */
			if (!JS_DefineProperty(cx, alllibs.get(), p->cfg->lib[l]->sname, val
			                       , NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE))
				return false;

			val = INT_TO_JSVAL(lib_index);
			if (!JS_SetProperty(cx, libobj.get(), "index", val.address()))
				return false;

			val = INT_TO_JSVAL(l);
			if (!JS_SetProperty(cx, libobj.get(), "number", val.address()))
				return false;

			if ((js_str = JS_NewStringCopyZ(cx, p->cfg->lib[l]->sname)) == nullptr)
				return false;
			val = STRING_TO_JSVAL(js_str.get());
			if (!JS_SetProperty(cx, libobj.get(), "name", val.address()))
				return false;

			if ((js_str = JS_NewStringCopyZ(cx, p->cfg->lib[l]->lname)) == nullptr)
				return false;
			val = STRING_TO_JSVAL(js_str.get());
			if (!JS_SetProperty(cx, libobj.get(), "description", val.address()))
				return false;

			if ((js_str = JS_NewStringCopyZ(cx, p->cfg->lib[l]->arstr)) == nullptr)
				return false;
			val = STRING_TO_JSVAL(js_str.get());
			if (!JS_SetProperty(cx, libobj.get(), "ars", val.address()))
				return false;

			if ((js_str = JS_NewStringCopyZ(cx, p->cfg->lib[l]->vdir)) == nullptr)
				return false;
			val = STRING_TO_JSVAL(js_str.get());
			if (!JS_SetProperty(cx, libobj.get(), "vdir", val.address()))
				return false;

			val = BOOLEAN_TO_JSVAL(lib_index >= 0);
			if (!JS_SetProperty(cx, libobj.get(), "can_access", val.address()))
				return false;

			if ((js_str = JS_NewStringCopyZ(cx, p->cfg->lib[l]->code_prefix)) == nullptr)
				return false;
			val = STRING_TO_JSVAL(js_str.get());
			if (!JS_SetProperty(cx, libobj.get(), "code_prefix", val.address()))
				return false;

#ifdef BUILD_JSDOCS
			js_DescribeSyncObject(cx, libobj.get(), "File Transfer Libraries (current user has access to)", 310);
#endif

			/* dir_list[] */
			if ((dir_list = JS_NewArrayObject(cx, 0, NULL)) == nullptr)
				return false;

			val = OBJECT_TO_JSVAL(dir_list.get());
			if (!JS_SetProperty(cx, libobj.get(), "dir_list", val.address()))
				return false;

			for (d = 0; d < p->cfg->total_dirs; d++) {
				if (p->cfg->dir[d]->lib != l)
					continue;

				if ((dirobj = JS_NewObject(cx, &js_dir_class, NULL, NULL)) == nullptr)
					return false;
				struct js_file_area_priv *np = static_cast<js_file_area_priv *>(malloc(sizeof(struct js_file_area_priv)));
				if (np == NULL)
					continue;
				*np = *p;
				np->dirnum = d;
				JS_SetPrivate(cx, dirobj.get(), np);

				val = OBJECT_TO_JSVAL(dirobj.get());
				dir_index = -1;
				if (p->user == NULL || user_can_access_dir(p->cfg, d, p->user, p->client)) {

					if (!JS_GetArrayLength(cx, dir_list.get(), (jsuint*)&dir_index))
						return false;

					if (!JS_SetElement(cx, dir_list.get(), dir_index, val.address()))
						return false;
				}

				/* Add as property (associative array element) */
				if (!JS_DefineProperty(cx, alldirs.get(), p->cfg->dir[d]->code, val
				                       , NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE))
					return false;

				if (d == p->cfg->user_dir
				    && !JS_DefineProperty(cx, areaobj, "user_dir", val
				                          , NULL, NULL, JSPROP_READONLY))
					return false;

				if (d == p->cfg->sysop_dir
				    && !JS_DefineProperty(cx, areaobj, "sysop_dir", val
				                          , NULL, NULL, JSPROP_READONLY))
					return false;

				if (d == p->cfg->upload_dir
				    && !JS_DefineProperty(cx, areaobj, "upload_dir", val
				                          , NULL, NULL, JSPROP_READONLY))
					return false;

				if (d == p->cfg->lib[l]->offline_dir
				    && !JS_DefineProperty(cx, libobj.get(), "offline_dir", val
				                          , NULL, NULL, JSPROP_READONLY))
					return false;

				val = INT_TO_JSVAL(dir_index);
				if (!JS_SetProperty(cx, dirobj.get(), "index", val.address()))
					return false;

				val = INT_TO_JSVAL(d);
				if (!JS_SetProperty(cx, dirobj.get(), "number", val.address()))
					return false;

				val = INT_TO_JSVAL(lib_index);
				if (!JS_SetProperty(cx, dirobj.get(), "lib_index", val.address()))
					return false;

				val = INT_TO_JSVAL(p->cfg->dir[d]->lib);
				if (!JS_SetProperty(cx, dirobj.get(), "lib_number", val.address()))
					return false;

				if ((js_str = JS_NewStringCopyZ(cx, p->cfg->lib[p->cfg->dir[d]->lib]->sname)) == nullptr)
					return false;
				val = STRING_TO_JSVAL(js_str.get());
				if (!JS_SetProperty(cx, dirobj.get(), "lib_name", val.address()))
					return false;

				if ((js_str = JS_NewStringCopyZ(cx, p->cfg->dir[d]->code)) == nullptr)
					return false;
				val = STRING_TO_JSVAL(js_str.get());
				if (!JS_SetProperty(cx, dirobj.get(), "code", val.address()))
					return false;

				if ((js_str = JS_NewStringCopyZ(cx, p->cfg->dir[d]->sname)) == nullptr)
					return false;
				val = STRING_TO_JSVAL(js_str.get());
				if (!JS_SetProperty(cx, dirobj.get(), "name", val.address()))
					return false;

				if ((js_str = JS_NewStringCopyZ(cx, p->cfg->dir[d]->lname)) == nullptr)
					return false;
				val = STRING_TO_JSVAL(js_str.get());
				if (!JS_SetProperty(cx, dirobj.get(), "description", val.address()))
					return false;

				if ((js_str = JS_NewStringCopyZ(cx, dir_area_tag(p->cfg, p->cfg->dir[d], str, sizeof(str)))) == nullptr)
					return false;
				val = STRING_TO_JSVAL(js_str.get());
				if (!JS_SetProperty(cx, dirobj.get(), "area_tag", val.address()))
					return false;

				if ((js_str = JS_NewStringCopyZ(cx, p->cfg->dir[d]->path)) == nullptr)
					return false;
				val = STRING_TO_JSVAL(js_str.get());
				if (!JS_SetProperty(cx, dirobj.get(), "path", val.address()))
					return false;

				if ((js_str = JS_NewStringCopyZ(cx, p->cfg->dir[d]->arstr)) == nullptr)
					return false;
				if (!JS_DefineProperty(cx, dirobj.get(), "ars", STRING_TO_JSVAL(js_str.get())
				                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
					return false;

				if ((js_str = JS_NewStringCopyZ(cx, p->cfg->dir[d]->ul_arstr)) == nullptr)
					return false;
				if (!JS_DefineProperty(cx, dirobj.get(), "upload_ars", STRING_TO_JSVAL(js_str.get())
				                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
					return false;

				if ((js_str = JS_NewStringCopyZ(cx, p->cfg->dir[d]->dl_arstr)) == nullptr)
					return false;
				if (!JS_DefineProperty(cx, dirobj.get(), "download_ars", STRING_TO_JSVAL(js_str.get())
				                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
					return false;

				if ((js_str = JS_NewStringCopyZ(cx, p->cfg->dir[d]->ex_arstr)) == nullptr)
					return false;
				if (!JS_DefineProperty(cx, dirobj.get(), "exempt_ars", STRING_TO_JSVAL(js_str.get())
				                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY)) /* exception here: Oct-15-2006 */
					return false;

				if ((js_str = JS_NewStringCopyZ(cx, p->cfg->dir[d]->op_arstr)) == nullptr)
					return false;
				if (!JS_DefineProperty(cx, dirobj.get(), "operator_ars", STRING_TO_JSVAL(js_str.get())
				                       , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY))
					return false;

				if ((js_str = JS_NewStringCopyZ(cx, p->cfg->dir[d]->exts)) == nullptr)
					return false;
				val = STRING_TO_JSVAL(js_str.get());
				if (!JS_SetProperty(cx, dirobj.get(), "extensions", val.address()))
					return false;

				if ((js_str = JS_NewStringCopyZ(cx, p->cfg->dir[d]->upload_sem)) == nullptr)
					return false;
				val = STRING_TO_JSVAL(js_str.get());
				if (!JS_SetProperty(cx, dirobj.get(), "upload_sem", val.address()))
					return false;

				if ((js_str = JS_NewStringCopyZ(cx, p->cfg->dir[d]->data_dir)) == nullptr)
					return false;
				val = STRING_TO_JSVAL(js_str.get());
				if (!JS_SetProperty(cx, dirobj.get(), "data_dir", val.address()))
					return false;

				val = UINT_TO_JSVAL(p->cfg->dir[d]->misc);
				if (!JS_SetProperty(cx, dirobj.get(), "settings", val.address()))
					return false;

				val = INT_TO_JSVAL(p->cfg->dir[d]->seqdev);
				if (!JS_SetProperty(cx, dirobj.get(), "seqdev", val.address()))
					return false;

				val = INT_TO_JSVAL(p->cfg->dir[d]->sort);
				if (!JS_SetProperty(cx, dirobj.get(), "sort", val.address()))
					return false;

				val = INT_TO_JSVAL(p->cfg->dir[d]->maxfiles);
				if (!JS_SetProperty(cx, dirobj.get(), "max_files", val.address()))
					return false;

				val = INT_TO_JSVAL(p->cfg->dir[d]->maxage);
				if (!JS_SetProperty(cx, dirobj.get(), "max_age", val.address()))
					return false;

				val = INT_TO_JSVAL(p->cfg->dir[d]->up_pct);
				if (!JS_SetProperty(cx, dirobj.get(), "upload_credit_pct", val.address()))
					return false;

				val = INT_TO_JSVAL(p->cfg->dir[d]->dn_pct);
				if (!JS_SetProperty(cx, dirobj.get(), "download_credit_pct", val.address()))
					return false;

				if ((js_str = JS_NewStringCopyZ(cx, p->cfg->dir[d]->vdir)) == nullptr)
					return false;
				val = STRING_TO_JSVAL(js_str.get());
				if (!JS_SetProperty(cx, dirobj.get(), "vdir", val.address()))
					return false;

				snprintf(vpath, sizeof vpath, "%s/", dir_vpath(p->cfg, p->cfg->dir[d], str, sizeof str));
				if ((js_str = JS_NewStringCopyZ(cx, vpath)) == nullptr)
					return false;
				val = STRING_TO_JSVAL(js_str.get());
				if (!JS_SetProperty(cx, dirobj.get(), "vpath", val.address()))
					return false;
				if (!js_dir_fill_properties(cx, dirobj.get(), np))
					return false;

				if ((js_str = JS_NewStringCopyZ(cx, p->cfg->dir[d]->vshortcut)) == nullptr)
					return false;
				val = STRING_TO_JSVAL(js_str.get());
				if (!JS_SetProperty(cx, dirobj.get(), "vshortcut", val.address()))
					return false;

				val = BOOLEAN_TO_JSVAL(d == p->cfg->lib[l]->offline_dir);
				if (!JS_SetProperty(cx, dirobj.get(), "is_offline", val.address()))
					return false;

				val = BOOLEAN_TO_JSVAL(d == p->cfg->upload_dir);
				if (!JS_SetProperty(cx, dirobj.get(), "is_upload", val.address()))
					return false;

				val = BOOLEAN_TO_JSVAL(d == p->cfg->sysop_dir);
				if (!JS_SetProperty(cx, dirobj.get(), "is_sysop", val.address()))
					return false;

#ifdef BUILD_JSDOCS
				js_CreateArrayOfStrings(cx, dirobj.get(), "_property_desc_list", dir_prop_desc, JSPROP_READONLY);
				js_DescribeSyncObject(cx, dirobj, "File Transfer Directories  (current user has access to)", 310);
#endif
			}

#ifdef BUILD_JSDOCS
			js_CreateArrayOfStrings(cx, libobj.get(), "_property_desc_list", lib_prop_desc, JSPROP_READONLY);
#endif
		}
#ifdef BUILD_JSDOCS
		js_DescribeSyncObject(cx, alllibs.get(), "Associative array of all libraries (use name as index)", 312);
		JS_DefineProperty(cx, alllibs.get(), "_dont_document", JSVAL_TRUE, NULL, NULL, JSPROP_READONLY);

		js_DescribeSyncObject(cx, alldirs.get(), "Associative array of all directories (use internal code as index)", 311);
		JS_DefineProperty(cx, alldirs.get(), "_dont_document", JSVAL_TRUE, NULL, NULL, JSPROP_READONLY);
#endif
	}

	return true;
}

static bool js_file_area_enumerate(JSContext *cx, JS::Handle<JSObject*> obj)
{
	return js_file_area_resolve_impl(cx, obj, NULL);
}

static const JSClassOps js_file_area_classops = {
	nullptr,                    /* addProperty  */
	nullptr,                    /* delProperty  */
	js_file_area_enumerate,     /* enumerate    */
	nullptr,                    /* newEnumerate */
	js_file_area_resolve,       /* resolve      */
	nullptr,                    /* mayResolve   */
	js_file_area_finalize,      /* finalize     */
	nullptr, nullptr, nullptr   /* call, construct, trace */
};

static JSClass js_file_area_class = {
	"FileArea"
	, JSCLASS_HAS_PRIVATE
	, &js_file_area_classops
};

DLLEXPORT JSObject* js_CreateFileAreaObject(JSContext* cx, JSObject* parent, scfg_t* cfg
                                            , user_t* user, client_t* client, const char* web_file_prefix) {
	JSObject*                 obj;
	struct js_file_area_priv *p;

	obj = JS_DefineObject(cx, parent, "file_area", &js_file_area_class, NULL
	                      , JSPROP_ENUMERATE | JSPROP_READONLY);

	if (obj == NULL)
		return NULL;

	p = (struct js_file_area_priv *)malloc(sizeof(struct js_file_area_priv));
	if (p == NULL)
		return NULL;

	memset(p, 0, sizeof(*p));
	p->cfg = cfg;
	p->user = user;
	p->client = client;
	p->web_file_prefix = web_file_prefix;

	if (!JS_SetPrivate(cx, obj, p)) {
		free(p);
		return NULL;
	}

#ifdef BUILD_JSDOCS
	// Ensure they're all created for JSDOCS
	js_file_area_enumerate(cx, obj);
#endif

	return obj;
}

#endif  /* JAVSCRIPT */
