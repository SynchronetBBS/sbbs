/* js_file_area.c */

/* Synchronet JavaScript "File Area" Object */

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

#ifdef _DEBUG

static char* lib_prop_desc[] = {
	 "library number"
	,"library name"
	,"library description"
	,"library link (for HTML index)"
	,NULL
};

static char* dir_prop_desc[] = {

	 "directory number"
	,"library number"
	,"directory internal code"
	,"directory name"
	,"directory description"
	,"directory file storage location"
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
	,NULL
};
#endif


JSObject* DLLCALL js_CreateFileAreaObject(JSContext* cx, JSObject* parent, scfg_t* cfg
										  ,user_t* user, char* html_index_file)
{
	char		vpath[MAX_PATH+1];
	JSObject*	areaobj;
	JSObject*	alldirs;
	JSObject*	libobj;
	JSObject*	dirobj;
	JSObject*	lib_list;
	JSObject*	dir_list;
	JSString*	js_str;
	jsval		val;
	jsuint		index;
	uint		l,d;

	/* Return existing object if it's already been created */
	if(JS_GetProperty(cx,parent,"file_area",&val) && val!=JSVAL_VOID)
		areaobj = JSVAL_TO_OBJECT(val);
	else
		areaobj = JS_DefineObject(cx, parent, "file_area", NULL
								, NULL, JSPROP_ENUMERATE|JSPROP_READONLY);
	if(areaobj==NULL)
		return(NULL);

	if((alldirs=JS_NewObject(cx, NULL, NULL, areaobj))==NULL)
		return(NULL);

	JS_NewNumberValue(cx,cfg->min_dspace,&val);
	if(!JS_SetProperty(cx, areaobj, "min_diskspace", &val)) 
		return(NULL);

	JS_NewNumberValue(cx,cfg->file_misc,&val);
	if(!JS_SetProperty(cx, areaobj, "settings", &val)) 
		return(NULL);

	if(html_index_file==NULL)
		html_index_file="";

#ifdef _DEBUG
	js_DescribeObject(cx,areaobj,"File Transfer Areas");
#endif

	/* lib_list[] */
	if((lib_list=JS_NewArrayObject(cx, 0, NULL))==NULL) 
		return(NULL);

	val=OBJECT_TO_JSVAL(lib_list);
	if(!JS_SetProperty(cx, areaobj, "lib_list", &val)) 
		return(NULL);

	for(l=0;l<cfg->total_libs;l++) {

		if(user==NULL && (*cfg->lib[l]->ar)!=AR_NULL)
			continue;

		if(user!=NULL && !chk_ar(cfg,cfg->lib[l]->ar,user))
			continue;

		if((libobj=JS_NewObject(cx, NULL, NULL, NULL))==NULL)
			return(NULL);

		val=INT_TO_JSVAL(l);
		if(!JS_SetProperty(cx, libobj, "number", &val))
			return(NULL);

		if((js_str=JS_NewStringCopyZ(cx, cfg->lib[l]->sname))==NULL)
			return(NULL);
		val=STRING_TO_JSVAL(js_str);
		if(!JS_SetProperty(cx, libobj, "name", &val))
			return(NULL);

		if((js_str=JS_NewStringCopyZ(cx, cfg->lib[l]->lname))==NULL)
			return(NULL);
		val=STRING_TO_JSVAL(js_str);
		if(!JS_SetProperty(cx, libobj, "description", &val))
			return(NULL);

		sprintf(vpath,"/%s/%s",cfg->lib[l]->sname,html_index_file);
		if((js_str=JS_NewStringCopyZ(cx, vpath))==NULL)
			return(NULL);
		val=STRING_TO_JSVAL(js_str);
		if(!JS_SetProperty(cx, libobj, "link", &val))
			return(NULL);

#ifdef _DEBUG
		js_DescribeObject(cx,libobj,"File Transfer Libraries");
#endif

		/* dir_list[] */
		if((dir_list=JS_NewArrayObject(cx, 0, NULL))==NULL) 
			return(NULL);

		val=OBJECT_TO_JSVAL(dir_list);
		if(!JS_SetProperty(cx, libobj, "dir_list", &val)) 
			return(NULL);

		for(d=0;d<cfg->total_dirs;d++) {
			if(cfg->dir[d]->lib!=l)
				continue;

			if(user==NULL && (*cfg->dir[d]->ar)!=AR_NULL)
				continue;

			if(user!=NULL && !chk_ar(cfg,cfg->dir[d]->ar,user))
				continue;

			if((dirobj=JS_NewObject(cx, NULL, NULL, NULL))==NULL)
				return(NULL);

			val=INT_TO_JSVAL(d);
			if(!JS_SetProperty(cx, dirobj, "number", &val))
				return(NULL);

			val=INT_TO_JSVAL(cfg->dir[d]->lib);
			if(!JS_SetProperty(cx, dirobj, "lib_number", &val))
				return(NULL);

			if((js_str=JS_NewStringCopyZ(cx, cfg->dir[d]->code))==NULL)
				return(NULL);
			val=STRING_TO_JSVAL(js_str);
			if(!JS_SetProperty(cx, dirobj, "code", &val))
				return(NULL);

			if((js_str=JS_NewStringCopyZ(cx, cfg->dir[d]->sname))==NULL)
				return(NULL);
			val=STRING_TO_JSVAL(js_str);
			if(!JS_SetProperty(cx, dirobj, "name", &val))
				return(NULL);

			if((js_str=JS_NewStringCopyZ(cx, cfg->dir[d]->lname))==NULL)
				return(NULL);
			val=STRING_TO_JSVAL(js_str);
			if(!JS_SetProperty(cx, dirobj, "description", &val))
				return(NULL);

			if((js_str=JS_NewStringCopyZ(cx, cfg->dir[d]->path))==NULL)
				return(NULL);
			val=STRING_TO_JSVAL(js_str);
			if(!JS_SetProperty(cx, dirobj, "path", &val))
				return(NULL);

			if((js_str=JS_NewStringCopyZ(cx, cfg->dir[d]->exts))==NULL)
				return(NULL);
			val=STRING_TO_JSVAL(js_str);
			if(!JS_SetProperty(cx, dirobj, "extensions", &val))
				return(NULL);

			if((js_str=JS_NewStringCopyZ(cx, cfg->dir[d]->upload_sem))==NULL)
				return(NULL);
			val=STRING_TO_JSVAL(js_str);
			if(!JS_SetProperty(cx, dirobj, "upload_sem", &val))
				return(NULL);

			if((js_str=JS_NewStringCopyZ(cx, cfg->dir[d]->data_dir))==NULL)
				return(NULL);
			val=STRING_TO_JSVAL(js_str);
			if(!JS_SetProperty(cx, dirobj, "data_dir", &val))
				return(NULL);

			JS_NewNumberValue(cx,cfg->dir[d]->misc,&val);
			if(!JS_SetProperty(cx, dirobj, "settings", &val))
				return(NULL);

			val=INT_TO_JSVAL(cfg->dir[d]->seqdev);
			if(!JS_SetProperty(cx, dirobj, "seqdev", &val))
				return(NULL);

			val=INT_TO_JSVAL(cfg->dir[d]->sort);
			if(!JS_SetProperty(cx, dirobj, "sort", &val))
				return(NULL);

			val=INT_TO_JSVAL(cfg->dir[d]->maxfiles);
			if(!JS_SetProperty(cx, dirobj, "max_files", &val))
				return(NULL);

			val=INT_TO_JSVAL(cfg->dir[d]->maxage);
			if(!JS_SetProperty(cx, dirobj, "max_age", &val))
				return(NULL);

			val=INT_TO_JSVAL(cfg->dir[d]->up_pct);
			if(!JS_SetProperty(cx, dirobj, "upload_credit_pct", &val))
				return(NULL);

			val=INT_TO_JSVAL(cfg->dir[d]->dn_pct);
			if(!JS_SetProperty(cx, dirobj, "download_credit_pct", &val))
				return(NULL);

			sprintf(vpath,"/%s/%s/%s"
				,cfg->lib[l]->sname
				,cfg->dir[d]->code
				,html_index_file);
			if((js_str=JS_NewStringCopyZ(cx, vpath))==NULL)
				return(NULL);
			val=STRING_TO_JSVAL(js_str);
			if(!JS_SetProperty(cx, dirobj, "link", &val))
				return(NULL);

			if(user==NULL || chk_ar(cfg,cfg->dir[d]->ul_ar,user))
				val=BOOLEAN_TO_JSVAL(JS_TRUE);
			else
				val=BOOLEAN_TO_JSVAL(JS_FALSE);
			if(!JS_SetProperty(cx, dirobj, "can_upload", &val))
				return(NULL);

			if(user==NULL || chk_ar(cfg,cfg->dir[d]->dl_ar,user))
				val=BOOLEAN_TO_JSVAL(JS_TRUE);
			else
				val=BOOLEAN_TO_JSVAL(JS_FALSE);
			if(!JS_SetProperty(cx, dirobj, "can_download", &val))
				return(NULL);

			if(user==NULL || chk_ar(cfg,cfg->dir[d]->ex_ar,user))
				val=BOOLEAN_TO_JSVAL(JS_TRUE);
			else
				val=BOOLEAN_TO_JSVAL(JS_FALSE);
			if(!JS_SetProperty(cx, dirobj, "is_exempt", &val))
				return(NULL);

			if(user==NULL || chk_ar(cfg,cfg->dir[d]->op_ar,user))
				val=BOOLEAN_TO_JSVAL(JS_TRUE);
			else
				val=BOOLEAN_TO_JSVAL(JS_FALSE);
			if(!JS_SetProperty(cx, dirobj, "is_operator", &val))
				return(NULL);

#ifdef _DEBUG
			js_CreateArrayOfStrings(cx, dirobj, "_property_desc_list", dir_prop_desc, JSPROP_READONLY);
#endif

			if(!JS_GetArrayLength(cx, dir_list, &index))	/* inexplicable exception here on Jul-6-2001 */
				return(NULL);								/* and again on Aug-7-2001 and Oct-21-2001 */

			val=OBJECT_TO_JSVAL(dirobj);
			if(!JS_SetElement(cx, dir_list, index, &val))
				return(NULL);

			/* Add as property (associative array element) */
			if(!JS_DefineProperty(cx, alldirs, cfg->dir[d]->code, val
				,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE))
				return(NULL);

#ifdef _DEBUG
			js_DescribeObject(cx,dirobj,"File Transfer Directories");
#endif
		}

#ifdef _DEBUG
		js_CreateArrayOfStrings(cx, libobj, "_property_desc_list", lib_prop_desc, JSPROP_READONLY);
#endif

		if(!JS_GetArrayLength(cx, lib_list, &index))
			return(NULL);

		val=OBJECT_TO_JSVAL(libobj);
		if(!JS_SetElement(cx, lib_list, index, &val))
			return(NULL);

	}

	val=OBJECT_TO_JSVAL(alldirs);
	if(!JS_SetProperty(cx, areaobj, "dir", &val))
		return(NULL);

#ifdef _DEBUG
	js_DescribeObject(cx,alldirs,"Associative array of all directories (use internal code as index)");
	JS_DefineProperty(cx,alldirs,"_dont_document",JSVAL_TRUE,NULL,NULL,JSPROP_READONLY);
#endif

	return(areaobj);
}

#endif	/* JAVSCRIPT */
