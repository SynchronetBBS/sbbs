/* js_file_area.c */

/* Synchronet JavaScript "File Area" Object */

/* $Id$ */

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

#ifdef BUILD_JSDOCS

static char* file_area_prop_desc[] = {
	 "minimum amount of available disk space (in kilobytes) required for user uploads to be allowed"
	,"file area settings (bitfield) - see <tt>FM_*</tt> in <tt>sbbsdefs.js</tt> for details"
	,"array of alternative file paths.  NOTE: this array is zero-based, but alt path fields are one-based."
	,NULL
};

static char* lib_prop_desc[] = {
	 "index into lib_list array (or -1 if not in array) <i>(introduced in v3.12)</i>"
	,"unique number for this library"
	,"library name"
	,"library description"
	,"library access requirements"
	,"library link (for HTML index)"
	,NULL
};

static char* dir_prop_desc[] = {

	 "index into dir_list array (or -1 if not in array) <i>(introduced in v3.12)</i>"
	,"unique number for this directory"
	,"library index <i>(introduced in v3.12)</i>"
	,"library number"
	,"library name <i>(introduced in v3.12)</i>"
	,"directory internal code"
	,"directory name"
	,"directory description"
	,"directory file storage location"
	,"directory access requirements"
	,"directory upload requirements"
	,"directory download requirements"
	,"directory exemption requirements"
	,"directory operator requirements"
	,"allowed file extensions (comma delimited)"
	,"upload semaphore file"
	,"directory data storage location"
	,"toggle options (bitfield)"
	,"sequential (slow storage) device number"
	,"sort order (see <tt>SORT_*</tt> in <tt>sbbsdefs.js</tt> for valid values)"
	,"configured maximum number of files"
	,"configured maximum age (in days) of files before expiration"
	,"percent of file size awarded uploader in credits upon file upload"
	,"percent of file size awarded uploader in credits upon subsequent downloads"
	,"directory link (for HTML index)"
	,"user has sufficient access to upload files"
	,"user has sufficient access to download files"
	,"user is exempt from download credit costs"
	,"user has operator access to this directory"
	,"directory is for offline storage <i>(introduced in v3.14)</i>"
	,"directory is for uploads only <i>(introduced in v3.14)</i>"
	,"directory is for uploads to sysop only <i>(introduced in v3.14)</i>"
	,NULL
};
#endif

struct js_file_area_priv {
	scfg_t		*cfg;
	user_t		*user;
	client_t	*client;
	char		*html_index_file;
};

JSBool DLLCALL js_file_area_resolve(JSContext* cx, JSObject* areaobj, jsid id)
{
	char		vpath[MAX_PATH+1];
	JSObject*	alllibs;
	JSObject*	alldirs;
	JSObject*	libobj;
	JSObject*	dirobj;
	JSObject*	alt_list;
	JSObject*	lib_list;
	JSObject*	dir_list;
	JSString*	js_str;
	jsval		val;
	jsuint		lib_index;
	jsuint		dir_index;
	uint		l,d;
	BOOL		is_op;
	char*		name=NULL;
	struct js_file_area_priv *p;

	if((p=(struct js_file_area_priv*)JS_GetPrivate(cx,areaobj))==NULL)
		return JS_FALSE;

	if(id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		
		JS_IdToValue(cx, id, &idval);
		if(JSVAL_IS_STRING(idval))
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
	}

	/* file_area.properties */
	if(name==NULL || strcmp(name, "min_diskspace")==0) {
		if(name)
			free(name);
		val=UINT_TO_JSVAL(p->cfg->min_dspace);
		JS_DefineProperty(cx, areaobj, "min_diskspace", val, NULL, NULL, JSPROP_ENUMERATE);
		if(name)
			return(JS_TRUE);
	}

	if(name==NULL || strcmp(name, "settings")==0) {
		if(name)
			free(name);
		val=UINT_TO_JSVAL(p->cfg->file_misc);
		JS_DefineProperty(cx, areaobj, "settings", val, NULL, NULL, JSPROP_ENUMERATE);
		if(name)
			return(JS_TRUE);
	}

	if(name==NULL || strcmp(name, "alt_paths")==0) {
		if(name)
			free(name);
		/* file_area.alt_paths[] */
		if((alt_list=JS_NewArrayObject(cx, 0, NULL))==NULL) 
			return JS_FALSE;

		val=OBJECT_TO_JSVAL(alt_list);
		if(!JS_SetProperty(cx, areaobj, "alt_paths", &val)) 
			return JS_FALSE;

		for (l=0; l<p->cfg->altpaths; l++) {
			if((js_str=JS_NewStringCopyZ(cx, p->cfg->altpath[l]))==NULL)
				return JS_FALSE;
			val=STRING_TO_JSVAL(js_str);

			if(!JS_SetElement(cx, alt_list, l, &val))
				return JS_FALSE;
		}
		if(name)
			return(JS_TRUE);
	}

#ifdef BUILD_JSDOCS
	js_CreateArrayOfStrings(cx, areaobj, "_property_desc_list", file_area_prop_desc, JSPROP_READONLY);
#endif

	if (name==NULL || strcmp(name, "lib")==0 || strcmp(name, "dir")==0 || strcmp(name, "lib_list")==0) {
		if(name)
			FREE_AND_NULL(name);
		if((alllibs=JS_NewObject(cx, NULL, NULL, areaobj))==NULL)
			return JS_FALSE;

		val=OBJECT_TO_JSVAL(alllibs);
		if (!JS_SetProperty(cx, areaobj, "lib", &val))
			return JS_FALSE;

		if((alldirs=JS_NewObject(cx, NULL, NULL, areaobj))==NULL)
			return JS_FALSE;

		val=OBJECT_TO_JSVAL(alldirs);
		if(!JS_SetProperty(cx, areaobj, "dir", &val))
			return JS_FALSE;

#ifdef BUILD_JSDOCS
		js_DescribeSyncObject(cx,areaobj,"File Transfer Areas",310);
#endif

		/* file_area.lib_list[] */
		if((lib_list=JS_NewArrayObject(cx, 0, NULL))==NULL) 
			return JS_FALSE;

		val=OBJECT_TO_JSVAL(lib_list);
		if(!JS_SetProperty(cx, areaobj, "lib_list", &val)) 
			return JS_FALSE;

		for(l=0;l<p->cfg->total_libs;l++) {

			if((libobj=JS_NewObject(cx, NULL, NULL, NULL))==NULL)
				return JS_FALSE;

			val=OBJECT_TO_JSVAL(libobj);
			lib_index=-1;
			if(p->user==NULL || chk_ar(p->cfg,p->cfg->lib[l]->ar,p->user,p->client)) {

				if(!JS_GetArrayLength(cx, lib_list, &lib_index))
					return JS_FALSE;

				if(!JS_SetElement(cx, lib_list, lib_index, &val))
					return JS_FALSE;
			}

			/* Add as property (associative array element) */
			if(!JS_DefineProperty(cx, alllibs, p->cfg->lib[l]->sname, val
				,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE))
				return JS_FALSE;

			val=INT_TO_JSVAL(lib_index);
			if(!JS_SetProperty(cx, libobj, "index", &val))
				return JS_FALSE;

			val=INT_TO_JSVAL(l);
			if(!JS_SetProperty(cx, libobj, "number", &val))
				return JS_FALSE;

			if((js_str=JS_NewStringCopyZ(cx, p->cfg->lib[l]->sname))==NULL)
				return JS_FALSE;
			val=STRING_TO_JSVAL(js_str);
			if(!JS_SetProperty(cx, libobj, "name", &val))
				return JS_FALSE;

			if((js_str=JS_NewStringCopyZ(cx, p->cfg->lib[l]->lname))==NULL)
				return JS_FALSE;
			val=STRING_TO_JSVAL(js_str);
			if(!JS_SetProperty(cx, libobj, "description", &val))
				return JS_FALSE;

			if((js_str=JS_NewStringCopyZ(cx, p->cfg->lib[l]->arstr))==NULL)
				return JS_FALSE;
			val=STRING_TO_JSVAL(js_str);
			if(!JS_SetProperty(cx, libobj, "ars", &val))
				return JS_FALSE;

			sprintf(vpath,"/%s/%s",p->cfg->lib[l]->sname,p->html_index_file);
			if((js_str=JS_NewStringCopyZ(cx, vpath))==NULL)
				return JS_FALSE;
			val=STRING_TO_JSVAL(js_str);
			if(!JS_SetProperty(cx, libobj, "link", &val))
				return JS_FALSE;

#ifdef BUILD_JSDOCS
			js_DescribeSyncObject(cx,libobj,"File Transfer Libraries (current user has access to)",310);
#endif

			/* dir_list[] */
			if((dir_list=JS_NewArrayObject(cx, 0, NULL))==NULL) 
				return JS_FALSE;

			val=OBJECT_TO_JSVAL(dir_list);
			if(!JS_SetProperty(cx, libobj, "dir_list", &val)) 
				return JS_FALSE;

			for(d=0;d<p->cfg->total_dirs;d++) {
				if(p->cfg->dir[d]->lib!=l)
					continue;

				if((dirobj=JS_NewObject(cx, NULL, NULL, NULL))==NULL)
					return JS_FALSE;

				val=OBJECT_TO_JSVAL(dirobj);
				dir_index=-1;
				if(p->user==NULL || chk_ar(p->cfg,p->cfg->dir[d]->ar,p->user,p->client)) {

					if(!JS_GetArrayLength(cx, dir_list, &dir_index))
						return JS_FALSE;

					if(!JS_SetElement(cx, dir_list, dir_index, &val))
						return JS_FALSE;
				}

				/* Add as property (associative array element) */
				if(!JS_DefineProperty(cx, alldirs, p->cfg->dir[d]->code, val
					,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE))
					return JS_FALSE;

				if(d==p->cfg->user_dir 
					&& !JS_DefineProperty(cx, areaobj, "user_dir", val
						,NULL,NULL,JSPROP_READONLY))
					return JS_FALSE;

				if(d==p->cfg->sysop_dir 
					&& !JS_DefineProperty(cx, areaobj, "sysop_dir", val
						,NULL,NULL,JSPROP_READONLY))
					return JS_FALSE;

				if(d==p->cfg->upload_dir 
					&& !JS_DefineProperty(cx, areaobj, "upload_dir", val
						,NULL,NULL,JSPROP_READONLY))
					return JS_FALSE;

				if(d==p->cfg->lib[l]->offline_dir
					&& !JS_DefineProperty(cx, libobj, "offline_dir", val
						,NULL,NULL,JSPROP_READONLY))
					return JS_FALSE;

				val=INT_TO_JSVAL(dir_index);
				if(!JS_SetProperty(cx, dirobj, "index", &val))
					return JS_FALSE;

				val=INT_TO_JSVAL(d);
				if(!JS_SetProperty(cx, dirobj, "number", &val))
					return JS_FALSE;

				val=INT_TO_JSVAL(lib_index);
				if(!JS_SetProperty(cx, dirobj, "lib_index", &val))
					return JS_FALSE;

				val=INT_TO_JSVAL(p->cfg->dir[d]->lib);
				if(!JS_SetProperty(cx, dirobj, "lib_number", &val))
					return JS_FALSE;

				if((js_str=JS_NewStringCopyZ(cx, p->cfg->lib[p->cfg->dir[d]->lib]->sname))==NULL)
					return JS_FALSE;
				val=STRING_TO_JSVAL(js_str);
				if(!JS_SetProperty(cx, dirobj, "lib_name", &val))
					return JS_FALSE;

				if((js_str=JS_NewStringCopyZ(cx, p->cfg->dir[d]->code))==NULL)
					return JS_FALSE;
				val=STRING_TO_JSVAL(js_str);
				if(!JS_SetProperty(cx, dirobj, "code", &val))
					return JS_FALSE;

				if((js_str=JS_NewStringCopyZ(cx, p->cfg->dir[d]->sname))==NULL)
					return JS_FALSE;
				val=STRING_TO_JSVAL(js_str);
				if(!JS_SetProperty(cx, dirobj, "name", &val))
					return JS_FALSE;

				if((js_str=JS_NewStringCopyZ(cx, p->cfg->dir[d]->lname))==NULL)
					return JS_FALSE;
				val=STRING_TO_JSVAL(js_str);
				if(!JS_SetProperty(cx, dirobj, "description", &val))
					return JS_FALSE;

				if((js_str=JS_NewStringCopyZ(cx, p->cfg->dir[d]->path))==NULL)
					return JS_FALSE;
				val=STRING_TO_JSVAL(js_str);
				if(!JS_SetProperty(cx, dirobj, "path", &val))
					return JS_FALSE;

				if((js_str=JS_NewStringCopyZ(cx, p->cfg->dir[d]->arstr))==NULL)
					return JS_FALSE;
				if(!JS_DefineProperty(cx, dirobj, "ars", STRING_TO_JSVAL(js_str)
					,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
					return JS_FALSE;

				if((js_str=JS_NewStringCopyZ(cx, p->cfg->dir[d]->ul_arstr))==NULL)
					return JS_FALSE;
				if(!JS_DefineProperty(cx, dirobj, "upload_ars", STRING_TO_JSVAL(js_str)
					,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
					return JS_FALSE;

				if((js_str=JS_NewStringCopyZ(cx, p->cfg->dir[d]->dl_arstr))==NULL)
					return JS_FALSE;
				if(!JS_DefineProperty(cx, dirobj, "download_ars", STRING_TO_JSVAL(js_str)
					,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
					return JS_FALSE;

				if((js_str=JS_NewStringCopyZ(cx, p->cfg->dir[d]->ex_arstr))==NULL)
					return JS_FALSE;
				if(!JS_DefineProperty(cx, dirobj, "exempt_ars", STRING_TO_JSVAL(js_str)
					,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))	/* exception here: Oct-15-2006 */
					return JS_FALSE;

				if((js_str=JS_NewStringCopyZ(cx, p->cfg->dir[d]->op_arstr))==NULL)
					return JS_FALSE;
				if(!JS_DefineProperty(cx, dirobj, "operator_ars", STRING_TO_JSVAL(js_str)
					,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
					return JS_FALSE;

				if((js_str=JS_NewStringCopyZ(cx, p->cfg->dir[d]->exts))==NULL)
					return JS_FALSE;
				val=STRING_TO_JSVAL(js_str);
				if(!JS_SetProperty(cx, dirobj, "extensions", &val))
					return JS_FALSE;

				if((js_str=JS_NewStringCopyZ(cx, p->cfg->dir[d]->upload_sem))==NULL)
					return JS_FALSE;
				val=STRING_TO_JSVAL(js_str);
				if(!JS_SetProperty(cx, dirobj, "upload_sem", &val))
					return JS_FALSE;

				if((js_str=JS_NewStringCopyZ(cx, p->cfg->dir[d]->data_dir))==NULL)
					return JS_FALSE;
				val=STRING_TO_JSVAL(js_str);
				if(!JS_SetProperty(cx, dirobj, "data_dir", &val))
					return JS_FALSE;

				val=UINT_TO_JSVAL(p->cfg->dir[d]->misc);
				if(!JS_SetProperty(cx, dirobj, "settings", &val))
					return JS_FALSE;

				val=INT_TO_JSVAL(p->cfg->dir[d]->seqdev);
				if(!JS_SetProperty(cx, dirobj, "seqdev", &val))
					return JS_FALSE;

				val=INT_TO_JSVAL(p->cfg->dir[d]->sort);
				if(!JS_SetProperty(cx, dirobj, "sort", &val))
					return JS_FALSE;

				val=INT_TO_JSVAL(p->cfg->dir[d]->maxfiles);
				if(!JS_SetProperty(cx, dirobj, "max_files", &val))
					return JS_FALSE;

				val=INT_TO_JSVAL(p->cfg->dir[d]->maxage);
				if(!JS_SetProperty(cx, dirobj, "max_age", &val))
					return JS_FALSE;

				val=INT_TO_JSVAL(p->cfg->dir[d]->up_pct);
				if(!JS_SetProperty(cx, dirobj, "upload_credit_pct", &val))
					return JS_FALSE;

				val=INT_TO_JSVAL(p->cfg->dir[d]->dn_pct);
				if(!JS_SetProperty(cx, dirobj, "download_credit_pct", &val))
					return JS_FALSE;

				sprintf(vpath,"/%s/%s/%s"
					,p->cfg->lib[l]->sname
					,p->cfg->dir[d]->code_suffix
					,p->html_index_file);
				if((js_str=JS_NewStringCopyZ(cx, vpath))==NULL)
					return JS_FALSE;
				val=STRING_TO_JSVAL(js_str);
				if(!JS_SetProperty(cx, dirobj, "link", &val))
					return JS_FALSE;

				if(p->user!=NULL 
					&& (p->user->level>=SYSOP_LEVEL 
						|| (p->cfg->dir[d]->op_ar!=NULL && p->cfg->dir[d]->op_ar[0]!=0 
							&& chk_ar(p->cfg,p->cfg->dir[d]->op_ar,p->user,p->client))))
					is_op=TRUE;
				else
					is_op=FALSE;

				if(p->user==NULL 
					|| ((is_op || p->user->exempt&FLAG('U')
					|| chk_ar(p->cfg,p->cfg->dir[d]->ul_ar,p->user,p->client))
					&& !(p->user->rest&FLAG('T')) && !(p->user->rest&FLAG('U'))))
					val=JSVAL_TRUE;
				else
					val=JSVAL_FALSE;
				if(!JS_SetProperty(cx, dirobj, "can_upload", &val))
					return JS_FALSE;

				if(p->user==NULL 
					|| (chk_ar(p->cfg,p->cfg->dir[d]->dl_ar,p->user,p->client)
					&& !(p->user->rest&FLAG('T')) && !(p->user->rest&FLAG('D'))))
					val=JSVAL_TRUE;
				else
					val=JSVAL_FALSE;
				if(!JS_SetProperty(cx, dirobj, "can_download", &val))
					return JS_FALSE;

				if(is_download_free(p->cfg,d,p->user,p->client))
					val=JSVAL_TRUE;
				else
					val=JSVAL_FALSE;
				if(!JS_SetProperty(cx, dirobj, "is_exempt", &val))
					return JS_FALSE;

				if(is_op)
					val=JSVAL_TRUE;
				else
					val=JSVAL_FALSE;
				if(!JS_SetProperty(cx, dirobj, "is_operator", &val))
					return JS_FALSE;

				val=BOOLEAN_TO_JSVAL(d==p->cfg->lib[l]->offline_dir);
				if(!JS_SetProperty(cx, dirobj, "is_offline", &val))
					return JS_FALSE;

				val=BOOLEAN_TO_JSVAL(d==p->cfg->upload_dir);
				if(!JS_SetProperty(cx, dirobj, "is_upload", &val))
					return JS_FALSE;

				val=BOOLEAN_TO_JSVAL(d==p->cfg->sysop_dir);
				if(!JS_SetProperty(cx, dirobj, "is_sysop", &val))
					return JS_FALSE;

#ifdef BUILD_JSDOCS
				js_CreateArrayOfStrings(cx, dirobj, "_property_desc_list", dir_prop_desc, JSPROP_READONLY);
				js_DescribeSyncObject(cx,dirobj,"File Transfer Directories  (current user has access to)",310);
#endif
			}

#ifdef BUILD_JSDOCS
			js_CreateArrayOfStrings(cx, libobj, "_property_desc_list", lib_prop_desc, JSPROP_READONLY);
#endif
		}
#ifdef BUILD_JSDOCS
		js_DescribeSyncObject(cx,alllibs,"Associative array of all libraries (use name as index)",312);
		JS_DefineProperty(cx,alllibs,"_dont_document",JSVAL_TRUE,NULL,NULL,JSPROP_READONLY);

		js_DescribeSyncObject(cx,alldirs,"Associative array of all directories (use internal code as index)",311);
		JS_DefineProperty(cx,alldirs,"_dont_document",JSVAL_TRUE,NULL,NULL,JSPROP_READONLY);
#endif
	}
	if (name)
		free(name);

	return JS_TRUE;
}

static JSBool js_file_area_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_file_area_resolve(cx, obj, JSID_VOID));
}

static void 
js_file_area_finalize(JSContext *cx, JSObject *obj)
{
	struct js_file_area_priv *p;

	if((p=(struct js_file_area_priv*)JS_GetPrivate(cx,obj))==NULL)
		return;

	free(p->html_index_file);
	free(p);
	JS_SetPrivate(cx,obj,NULL);
}


static JSClass js_file_area_class = {
     "FileArea"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,JS_PropertyStub		/* getProperty	*/
	,JS_StrictPropertyStub		/* setProperty	*/
	,js_file_area_enumerate	/* enumerate	*/
	,js_file_area_resolve	/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_file_area_finalize	/* finalize		*/
};

DLLEXPORT JSObject* DLLCALL js_CreateFileAreaObject(JSContext* cx, JSObject* parent, scfg_t* cfg
					,user_t* user, client_t* client, char* html_index_file) {
	JSObject* obj;
	struct js_file_area_priv *p;

	obj = JS_DefineObject(cx, parent, "file_area", &js_file_area_class, NULL
		,JSPROP_ENUMERATE|JSPROP_READONLY);

	if(obj==NULL)
		return(NULL);

	p = (struct js_file_area_priv *)malloc(sizeof(struct js_file_area_priv));
	if (p == NULL)
		return NULL;

	memset(p,0,sizeof(*p));
	p->cfg = cfg;
	p->user = user;
	p->client = client;
	if (html_index_file == NULL)
		html_index_file = "";
	p->html_index_file = strdup(html_index_file);

	if(!JS_SetPrivate(cx, obj, p)) {
		free(p->html_index_file);
		free(p);
		return(NULL);
	}

#ifdef BUILD_JSDOCS
	// Ensure they're all created for JSDOCS
	js_file_area_enumerate(cx, obj);
#endif

	return(obj);
}

#endif	/* JAVSCRIPT */
