/* Synchronet JavaScript "FileBase" Object */

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
#include <stdbool.h>

typedef struct
{
	smb_t	smb;
	int		smb_result;

} private_t;

/* Destructor */

static void js_finalize_filebase(JSContext *cx, JSObject *obj)
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

extern JSClass js_filebase_class;

static JSBool
js_open(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	private_t* p;
	jsrefcount	rc;
	scfg_t*		scfg;

	scfg = JS_GetRuntimePrivate(JS_GetRuntime(cx));
	if(scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class))==NULL) {
		return JS_FALSE;
	}

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if(p->smb.dirnum==INVALID_DIR
		&& strchr(p->smb.file,'/')==NULL
		&& strchr(p->smb.file,'\\')==NULL) {
		JS_ReportError(cx,"Unrecognized filebase code: %s",p->smb.file);
		return JS_TRUE;
	}

	rc=JS_SUSPENDREQUEST(cx);
	if((p->smb_result = smb_open_dir(scfg, &(p->smb), p->smb.dirnum)) != SMB_SUCCESS) {
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	return JS_TRUE;
}

static JSBool
js_close(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	private_t* p;
	jsrefcount	rc;

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class))==NULL) {
		return JS_FALSE;
	}

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	smb_close(&(p->smb));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_dump_file(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*	obj = JS_THIS_OBJECT(cx, arglist);
	jsval*		argv = JS_ARGV(cx, arglist);
	private_t*	p;
	char*		filename = NULL;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class))==NULL)
		return JS_FALSE;

	if(!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	uintN argn = 0;
	if(argn < argc && JSVAL_IS_STRING(argv[argn]))	{
		JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[argn]), filename, NULL);
		HANDLE_PENDING(cx, filename);
		if(filename == NULL)
			return JS_FALSE;
		argn++;
	}

	smbfile_t file;
	if((p->smb_result = smb_loadfile(&p->smb, filename, &file, file_detail_normal)) == SMB_SUCCESS) {
		str_list_t list = smb_msghdr_str_list(&file);
		if(list != NULL) {
			JSObject* array;
			if((array = JS_NewArrayObject(cx, 0, NULL)) == NULL) {
				free(filename);
				JS_ReportError(cx, "JS_NewArrayObject failure");
				return JS_FALSE;
			}
			JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array));
			for(int i = 0; list[i] != NULL; i++) {
				JSString* js_str = JS_NewStringCopyZ(cx, list[i]);
				if(js_str == NULL)
					break;
				JS_DefineElement(cx, array, i, STRING_TO_JSVAL(js_str), NULL, NULL, JSPROP_ENUMERATE);
			}
			strListFree(&list);
		}
	}
	smb_freefilemem(&file);
	free(filename);
	return JS_TRUE;
}

static bool
set_file_properties(JSContext *cx, JSObject* obj, smbfile_t* f, enum file_detail detail)
{
	jsval		val;
	JSString*	js_str;
	const uintN flags = JSPROP_ENUMERATE | JSPROP_READONLY;

	scfg_t* scfg = JS_GetRuntimePrivate(JS_GetRuntime(cx));
	if(scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}
	if(f->name == NULL
		|| (js_str = JS_NewStringCopyZ(cx, f->name)) == NULL
		|| !JS_DefineProperty(cx, obj, "name", STRING_TO_JSVAL(js_str), NULL, NULL, flags))
		return false;

	if(f->from != NULL
		&& ((js_str = JS_NewStringCopyZ(cx, f->from)) == NULL
			|| !JS_DefineProperty(cx, obj, "from", STRING_TO_JSVAL(js_str), NULL, NULL, flags)))
		return false;

	val = BOOLEAN_TO_JSVAL(f->idx.attr & FILE_ANONYMOUS);
	if((val == JSVAL_TRUE || detail > file_detail_extdesc)
		&& !JS_DefineProperty(cx, obj, "anon", val, NULL, NULL, flags))
		return false;

	if(f->desc != NULL
		&& ((js_str = JS_NewStringCopyZ(cx, f->desc)) == NULL
			|| !JS_DefineProperty(cx, obj, "desc", STRING_TO_JSVAL(js_str), NULL, NULL, flags)))
		return false;

	if(f->extdesc != NULL && *f->extdesc != '\0'
		&& ((js_str = JS_NewStringCopyZ(cx, f->extdesc)) == NULL
			|| !JS_DefineProperty(cx, obj, "extdesc", STRING_TO_JSVAL(js_str), NULL, NULL, flags)))
		return false;

	if(f->cost > 0 || detail > file_detail_extdesc) {
		val = UINT_TO_JSVAL(f->cost);
		if(!JS_DefineProperty(cx, obj, "cost", val, NULL, NULL, flags))
			return false;
	}
	val = UINT_TO_JSVAL(f->idx.size);
	if(!JS_DefineProperty(cx, obj, "size", val, NULL, NULL, flags))
		return false;

	if(f->idx.altpath > 0 || detail > file_detail_extdesc) {
		val = UINT_TO_JSVAL(f->idx.altpath);
		if(!JS_DefineProperty(cx, obj, "altpath", val, NULL, NULL, flags))
			return false;
	}
	val = UINT_TO_JSVAL(f->hdr.when_written.time);
	if(!JS_DefineProperty(cx, obj, "time", val, NULL, NULL, flags))
		return false;
	if(f->hdr.when_imported.time > 0 || detail > file_detail_extdesc) {
		val = UINT_TO_JSVAL(f->hdr.when_imported.time);
		if(!JS_DefineProperty(cx, obj, "added", val, NULL, NULL, flags))
			return false;
	}
	if(f->hdr.last_downloaded > 0 || detail > file_detail_extdesc) {
		val = UINT_TO_JSVAL(f->hdr.last_downloaded);
		if(!JS_DefineProperty(cx, obj, "last_downloaded", val, NULL, NULL, flags))
			return false;
	}
	if(f->hdr.times_downloaded > 0 || detail > file_detail_extdesc) {
		val = UINT_TO_JSVAL(f->hdr.times_downloaded);
		if(!JS_DefineProperty(cx, obj, "times_downloaded", val, NULL, NULL, flags))
			return false;
	}
	if(f->file_idx.hash.flags & SMB_HASH_CRC16) {
		val = UINT_TO_JSVAL(f->file_idx.hash.data.crc16);
		if(!JS_DefineProperty(cx, obj, "crc16", val, NULL, NULL, flags))
			return false;
	}
	if(f->file_idx.hash.flags & SMB_HASH_CRC32) {
		val = UINT_TO_JSVAL(f->file_idx.hash.data.crc32);
		if(!JS_DefineProperty(cx, obj, "crc32", val, NULL, NULL, flags))
			return false;
	}
	if(f->file_idx.hash.flags & SMB_HASH_MD5) {
		char hex[128];
		if((js_str = JS_NewStringCopyZ(cx, MD5_hex(hex, f->file_idx.hash.data.md5))) == NULL
			|| !JS_DefineProperty(cx, obj, "md5", STRING_TO_JSVAL(js_str), NULL, NULL, flags))
			return false;
	}
	return true;
}

static BOOL
parse_file_index_properties(JSContext *cx, JSObject* obj, fileidxrec_t* idx)
{
	char*		cp = NULL;
	size_t		cp_sz = 0;
	jsval		val;

	const char* prop_name = "name";
	if(JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return FALSE;
		}
		SAFECOPY(idx->name, cp);
	}
	prop_name = "size";
	if(JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		if(!JS_ValueToECMAUint32(cx, val, &idx->idx.size)) {
			JS_ReportError(cx, "Error converting adding '%s' property to Uint32", prop_name);
			return FALSE;
		}
	}
	prop_name = "added";
	if(JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		if(!JS_ValueToECMAUint32(cx, val, &idx->idx.time)) {
			JS_ReportError(cx, "Error converting adding '%s' property to Uint32", prop_name);
			return FALSE;
		}
	}
	prop_name = "crc16";
	if(JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		idx->hash.data.crc16 = JSVAL_TO_INT(val);
		idx->hash.flags |= SMB_HASH_CRC16;
	}
	prop_name = "crc32";
	if(JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		if(!JS_ValueToECMAUint32(cx, val, &idx->hash.data.crc32)) {
			JS_ReportError(cx, "Error converting adding '%s' property to Uint32", prop_name);
			return FALSE;
		}
		idx->hash.flags |= SMB_HASH_CRC32;
	}
	prop_name = "md5";
	if(JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL || strlen(cp) != MD5_DIGEST_SIZE * 2) {
			free(cp);
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return FALSE;
		}
		for(int i = 0; i < MD5_DIGEST_SIZE * 2; i += 2) {
			idx->hash.data.md5[i/2] = HEX_CHAR_TO_INT(*(cp + i)) * 16;
			idx->hash.data.md5[i/2] += HEX_CHAR_TO_INT(*(cp + i + 1));
		}
		idx->hash.flags |= SMB_HASH_MD5;
	}
	free(cp);
	return TRUE;
}

static int
parse_file_properties(JSContext *cx, JSObject* obj, smbfile_t* file, char** extdesc)
{
	char*		cp = NULL;
	size_t		cp_sz = 0;
	jsval		val;
	int result = SMB_ERR_NOT_FOUND;

	const char* prop_name = "name";
	if(JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return SMB_FAILURE;
		}
		if((result = smb_hfield_str(file, SMB_FILENAME, cp)) != SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding '%s' property to message header", result, prop_name);
			return result;
		}
	}

	prop_name = "from";
	if(JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return SMB_FAILURE;
		}
		if((result = smb_hfield_str(file, SMB_FILEUPLOADER, cp)) != SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding '%s' property to message header", result, prop_name);
			return result;
		}
	}

	prop_name = "desc";
	if(JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if(cp==NULL) {
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return SMB_FAILURE;
		}
		if((result = smb_hfield_str(file, SMB_FILEDESC, cp)) != SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding '%s' property to message header", result, prop_name);
			return result;
		}
	}
	prop_name = "extdesc";
	if(extdesc != NULL && JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		FREE_AND_NULL(*extdesc);
		JSVALUE_TO_MSTRING(cx, val, *extdesc, NULL);
		HANDLE_PENDING(cx, *extdesc);
		if(*extdesc == NULL) {
			JS_ReportError(cx, "Invalid '%s' string in recipient object", prop_name);
			return SMB_ERR_MEM;
		}
	}

	if(JS_GetProperty(cx, obj, "anon", &val) && val == JSVAL_TRUE)
		file->hdr.attr |= FILE_ANONYMOUS;

	if(!parse_file_index_properties(cx, obj, &file->file_idx))
		result = SMB_FAILURE;

	free(cp);
	return result;
}

static JSBool
js_get_file(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*	obj = JS_THIS_OBJECT(cx, arglist);
	jsval*		argv = JS_ARGV(cx, arglist);
	private_t*	p;
	char*		filename = NULL;
	enum file_detail detail = file_detail_normal;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	scfg_t* scfg = JS_GetRuntimePrivate(JS_GetRuntime(cx));
	if(scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class))==NULL)
		return JS_FALSE;

	if(!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	smbfile_t file;
	ZERO_VAR(file);

	uintN argn = 0;
	if(argn < argc && JSVAL_IS_STRING(argv[argn]))	{
		JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[argn]), filename, NULL);
		HANDLE_PENDING(cx, filename);
		if(filename == NULL)
			return JS_FALSE;
		argn++;
	}
	if(argn < argc && JSVAL_IS_OBJECT(argv[argn])) {
		if(!parse_file_index_properties(cx, JSVAL_TO_OBJECT(argv[argn]), &file.file_idx))
			return JS_TRUE;
		filename = strdup(file.file_idx.name);
	}
	else if(filename == NULL)
		return JS_TRUE;
	if(argn < argc && JSVAL_IS_NUMBER(argv[argn])) {
		detail = JSVAL_TO_INT(argv[argn]);
		argn++;
	}
	rc=JS_SUSPENDREQUEST(cx);
	if((p->smb_result = smb_findfile(&p->smb, filename, &file.file_idx)) == SMB_SUCCESS
		&& (p->smb_result = smb_getfile(&p->smb, &file, detail)) == SMB_SUCCESS) {
		JSObject* fobj;
		if((fobj = JS_NewObject(cx, NULL, NULL, obj)) == NULL)
			JS_ReportError(cx, "object allocation failure, line %d", __LINE__);
		else {
			set_file_properties(cx, fobj, &file, detail);
		    JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(fobj));
		}
	}
	JS_RESUMEREQUEST(cx, rc);
	free(filename);
	smb_freefilemem(&file);

	return JS_TRUE;
}

static JSBool
js_get_file_list(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*	obj = JS_THIS_OBJECT(cx, arglist);
	jsval*		argv = JS_ARGV(cx, arglist);
	private_t*	p;
	time_t		t = 0;
	char*		filespec = NULL;
	enum file_detail detail = file_detail_normal;
	BOOL		sort = TRUE;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	scfg_t* scfg = JS_GetRuntimePrivate(JS_GetRuntime(cx));
	if(scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class))==NULL)
		return JS_FALSE;

	if(!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	uintN argn = 0;
	if(argn < argc && JSVAL_IS_STRING(argv[argn]))	{
		JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[argn]), filespec, NULL);
		HANDLE_PENDING(cx, filespec);
		if(filespec == NULL)
			return JS_FALSE;
		argn++;
	}
	if(argn < argc && JSVAL_IS_NUMBER(argv[argn])) {
		detail = JSVAL_TO_INT(argv[argn]);
		argn++;
	}
	if(argn < argc && JSVAL_IS_NUMBER(argv[argn]))	{
		t = JSVAL_TO_INT(argv[argn]);
		argn++;
	}
	if(argn < argc && JSVAL_IS_BOOLEAN(argv[argn])) {
		sort = JSVAL_TO_BOOLEAN(argv[argn]);
		argn++;
	}

	rc=JS_SUSPENDREQUEST(cx);
	JSObject* array;
    if((array = JS_NewArrayObject(cx, 0, NULL)) == NULL) {
		free(filespec);
		JS_RESUMEREQUEST(cx, rc);
		JS_ReportError(cx, "JS_NewArrayObject failure");
		return JS_FALSE;
	}

	size_t file_count;
	smbfile_t* file_list = loadfiles(scfg, &p->smb, filespec, t, detail, sort, &file_count);
	if(file_list != NULL) {
		for(size_t i = 0; i < file_count; i++) {
			JSObject* fobj;
			if((fobj = JS_NewObject(cx, NULL, NULL, array)) == NULL) {
				JS_ReportError(cx, "object allocation failure, line %d", __LINE__);
				break;
			}
			set_file_properties(cx, fobj, &file_list[i], detail);
			JS_DefineElement(cx, array, i, OBJECT_TO_JSVAL(fobj), NULL, NULL, JSPROP_ENUMERATE);
		}
		freefiles(file_list, file_count);
	}
    JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array));
	JS_RESUMEREQUEST(cx, rc);
	free(filespec);

	return JS_TRUE;
}

static JSBool
js_get_file_names(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*	obj = JS_THIS_OBJECT(cx, arglist);
	jsval*		argv = JS_ARGV(cx, arglist);
	private_t*	p;
	time_t		t = 0;
	char*		filespec = NULL;
	BOOL		sort = TRUE;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	scfg_t* scfg = JS_GetRuntimePrivate(JS_GetRuntime(cx));
	if(scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class))==NULL)
		return JS_FALSE;

	if(!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	uintN argn = 0;
	if(argn < argc && JSVAL_IS_STRING(argv[argn]))	{
		JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[argn]), filespec, NULL);
		HANDLE_PENDING(cx, filespec);
		if(filespec == NULL)
			return JS_FALSE;
		argn++;
	}
	if(argn < argc && JSVAL_IS_NUMBER(argv[argn]))	{
		t = JSVAL_TO_INT(argv[argn]);
		argn++;
	}
	if(argn < argc && JSVAL_IS_BOOLEAN(argv[argn])) {
		sort = JSVAL_TO_BOOLEAN(argv[argn]);
		argn++;
	}

	rc=JS_SUSPENDREQUEST(cx);
	JSObject* array;
    if((array = JS_NewArrayObject(cx, 0, NULL)) == NULL) {
		free(filespec);
		JS_RESUMEREQUEST(cx, rc);
		JS_ReportError(cx, "JS_NewArrayObject failure");
		return JS_FALSE;
	}

	str_list_t file_list = loadfilenames(scfg, &p->smb, filespec, t, sort, NULL);
	if(file_list != NULL) {
		for(size_t i = 0; file_list[i] != NULL; i++) {
			JSString* js_str;
			if((js_str = JS_NewStringCopyZ(cx, file_list[i]))==NULL)
				return JS_FALSE;
			JS_DefineElement(cx, array, i, STRING_TO_JSVAL(js_str), NULL, NULL, JSPROP_ENUMERATE);
		}
		strListFree(&file_list);
	}
    JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array));
	JS_RESUMEREQUEST(cx, rc);
	free(filespec);

	return JS_TRUE;
}

static JSBool
js_get_file_path(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*	obj = JS_THIS_OBJECT(cx, arglist);
	jsval*		argv = JS_ARGV(cx, arglist);
	private_t*	p;
	char*		filename = NULL;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	scfg_t* scfg = JS_GetRuntimePrivate(JS_GetRuntime(cx));
	if(scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class))==NULL)
		return JS_FALSE;

	uintN argn = 0;
	if(argn < argc && JSVAL_IS_STRING(argv[argn]))	{
		JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[argn]), filename, NULL);
		HANDLE_PENDING(cx, filename);
		if(filename == NULL)
			return JS_FALSE;
		argn++;
	}
	if(filename == NULL)
		return JS_TRUE;

	rc=JS_SUSPENDREQUEST(cx);
	smbfile_t file;
	if((p->smb_result = smb_loadfile(&p->smb, filename, &file, file_detail_index)) == SMB_SUCCESS) {
		char path[MAX_PATH + 1];
		JSString* js_str;
		if((js_str = JS_NewStringCopyZ(cx, getfilepath(scfg, &file, path))) != NULL)
			JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
		smb_freefilemem(&file);
	}
	JS_RESUMEREQUEST(cx, rc);
	free(filename);

	return JS_TRUE;
}

static JSBool
js_get_file_size(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*	obj = JS_THIS_OBJECT(cx, arglist);
	jsval*		argv = JS_ARGV(cx, arglist);
	private_t*	p;
	char*		filename = NULL;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	scfg_t* scfg = JS_GetRuntimePrivate(JS_GetRuntime(cx));
	if(scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class))==NULL)
		return JS_FALSE;

	uintN argn = 0;
	if(argn < argc && JSVAL_IS_STRING(argv[argn]))	{
		JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[argn]), filename, NULL);
		HANDLE_PENDING(cx, filename);
		if(filename == NULL)
			return JS_FALSE;
		argn++;
	}
	if(filename == NULL)
		return JS_TRUE;

	rc=JS_SUSPENDREQUEST(cx);
	smbfile_t file;
	if((p->smb_result = smb_loadfile(&p->smb, filename, &file, file_detail_index)) == SMB_SUCCESS) {
		char path[MAX_PATH + 1];
		getfilepath(scfg, &file, path);
	    JS_SET_RVAL(cx, arglist, UINT_TO_JSVAL(getfilesize(scfg, &file)));
		smb_freefilemem(&file);
	}
	JS_RESUMEREQUEST(cx, rc);
	free(filename);

	return JS_TRUE;
}

static JSBool
js_get_file_time(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*	obj = JS_THIS_OBJECT(cx, arglist);
	jsval*		argv = JS_ARGV(cx, arglist);
	private_t*	p;
	char*		filename = NULL;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	scfg_t* scfg = JS_GetRuntimePrivate(JS_GetRuntime(cx));
	if(scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class))==NULL)
		return JS_FALSE;

	uintN argn = 0;
	if(argn < argc && JSVAL_IS_STRING(argv[argn]))	{
		JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[argn]), filename, NULL);
		HANDLE_PENDING(cx, filename);
		if(filename == NULL)
			return JS_FALSE;
		argn++;
	}
	if(filename == NULL)
		return JS_TRUE;

	rc=JS_SUSPENDREQUEST(cx);
	smbfile_t file;
	if((p->smb_result = smb_loadfile(&p->smb, filename, &file, file_detail_index)) == SMB_SUCCESS) {
		char path[MAX_PATH + 1];
		getfilepath(scfg, &file, path);
	    JS_SET_RVAL(cx, arglist, UINT_TO_JSVAL((uint32)getfiletime(scfg, &file)));
		smb_freefilemem(&file);
	}
	JS_RESUMEREQUEST(cx, rc);
	free(filename);

	return JS_TRUE;
}

static JSBool
js_add_file(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*	obj = JS_THIS_OBJECT(cx, arglist);
	jsval*		argv = JS_ARGV(cx, arglist);
	private_t*	p;
	char*		str = NULL;
	char*		extdesc = NULL;
	smbfile_t	file;
	jsrefcount	rc;

	ZERO_VAR(file);
	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	scfg_t* scfg = JS_GetRuntimePrivate(JS_GetRuntime(cx));
	if(scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class))==NULL)
		return JS_FALSE;

	if(!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	for(uintN argn = 0; argn < argc; argn++) {
		jsval arg = argv[argn];
		if(JSVAL_IS_OBJECT(arg)) {
			p->smb_result = parse_file_properties(cx, JSVAL_TO_OBJECT(arg), &file, &extdesc);
			if(p->smb_result != SMB_SUCCESS)
				return JS_TRUE;
		}
	}

	if(file.name != NULL) {
		char path[MAX_PATH + 1];
		rc=JS_SUSPENDREQUEST(cx);
		p->smb_result = smb_addfile(&p->smb, &file, SMB_SELFPACK, extdesc, getfilepath(scfg, &file, path));
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(p->smb_result == SMB_SUCCESS));
		JS_RESUMEREQUEST(cx, rc);
	}
	smb_freefilemem(&file);
	free(extdesc);

	return JS_TRUE;
}

// NOTE: extended description not supported for update!
static JSBool
js_update_file(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*	obj = JS_THIS_OBJECT(cx, arglist);
	jsval*		argv = JS_ARGV(cx, arglist);
	private_t*	p;
	char*		str = NULL;
	smbfile_t	file;
	jsrefcount	rc;

	ZERO_VAR(file);
	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	scfg_t* scfg = JS_GetRuntimePrivate(JS_GetRuntime(cx));
	if(scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class))==NULL)
		return JS_FALSE;

	if(!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	for(uintN argn = 0; argn < argc; argn++) {
		jsval arg = argv[argn];
		if(JSVAL_IS_OBJECT(arg)) {
			p->smb_result = parse_file_properties(cx, JSVAL_TO_OBJECT(arg), &file, NULL);
			if(p->smb_result != SMB_SUCCESS)
				return JS_TRUE;
		}
	}

	if(file.name != NULL
		&& (p->smb_result = smb_loadfile(&p->smb, file.name, &file, file_detail_normal)) == SMB_SUCCESS) {
		rc=JS_SUSPENDREQUEST(cx);
		p->smb_result = smb_putmsg(&p->smb, &file);
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(p->smb_result == SMB_SUCCESS));
		JS_RESUMEREQUEST(cx, rc);
	}
	smb_freefilemem(&file);

	return JS_TRUE;
}

static JSBool
js_renew_file(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*	obj = JS_THIS_OBJECT(cx, arglist);
	jsval*		argv = JS_ARGV(cx, arglist);
	private_t*	p;
	char*		fname = NULL;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	scfg_t* scfg = JS_GetRuntimePrivate(JS_GetRuntime(cx));
	if(scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class))==NULL)
		return JS_FALSE;

	if(!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	for(uintN argn=0; argn < argc; argn++) {
		if(JSVAL_IS_STRING(argv[argn]))	{
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[argn]), fname, NULL);
			HANDLE_PENDING(cx, fname);
			if(fname == NULL)
				return JS_FALSE;
		}
	}
	if(fname == NULL)
		return JS_TRUE;

	rc=JS_SUSPENDREQUEST(cx);
	smbfile_t file;
	if((p->smb_result = smb_loadfile(&p->smb, fname, &file, file_detail_index)) == SMB_SUCCESS) {
		char path[MAX_PATH + 1];
		p->smb_result = smb_renewfile(&p->smb, &file, SMB_SELFPACK, getfilepath(scfg, &file, path));
		smb_freefilemem(&file);
	}
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(p->smb_result == SMB_SUCCESS));
	JS_RESUMEREQUEST(cx, rc);
	free(fname);

	return JS_TRUE;
}

static JSBool
js_remove_file(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*	obj = JS_THIS_OBJECT(cx, arglist);
	jsval*		argv = JS_ARGV(cx, arglist);
	private_t*	p;
	char*		fname = NULL;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class))==NULL)
		return JS_FALSE;

	if(!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	for(uintN argn=0; argn < argc; argn++) {
		if(JSVAL_IS_STRING(argv[argn]))	{
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[argn]), fname, NULL);
			HANDLE_PENDING(cx, fname);
			if(fname == NULL)
				return JS_FALSE;
		}
	}
	if(fname == NULL)
		return JS_TRUE;

	rc=JS_SUSPENDREQUEST(cx);
	smbfile_t file;
	if((p->smb_result = smb_loadfile(&p->smb, fname, &file, file_detail_index)) == SMB_SUCCESS) {
		p->smb_result = smb_removefile(&p->smb, &file);
		smb_freefilemem(&file);
	}
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(p->smb_result == SMB_SUCCESS));
	JS_RESUMEREQUEST(cx, rc);
	free(fname);

	return JS_TRUE;
}

/* FileBase Object Properties */
enum {
	 FB_PROP_LAST_ERROR
	,FB_PROP_FILE
	,FB_PROP_DEBUG
	,FB_PROP_RETRY_TIME
	,FB_PROP_RETRY_DELAY
	,FB_PROP_FIRST_FILE		/* first file number */
	,FB_PROP_LAST_FILE		/* last file number */
	,FB_PROP_LAST_FILE_TIME	/* last file index time */
	,FB_PROP_FILES 			/* total files */
	,FB_PROP_NEW_FILE_TIME
	,FB_PROP_MAX_CRCS		/* Maximum number of CRCs to keep in history */
    ,FB_PROP_MAX_FILES		/* Maximum number of file to keep in dir */
    ,FB_PROP_MAX_AGE		/* Maximum age of file to keep in dir (in days) */
	,FB_PROP_ATTR			/* Attributes for this file base (SMB_HYPER,etc) */
	,FB_PROP_DIRNUM			/* Directory number */
	,FB_PROP_IS_OPEN
	,FB_PROP_STATUS			/* Last SMBLIB returned status value (e.g. retval) */
};

static JSBool js_filebase_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	time32_t t;
	jsval idval;
    jsint       tiny;
	private_t*	p;

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class))==NULL) {
		return JS_FALSE;
	}

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch(tiny) {
		case FB_PROP_RETRY_TIME:
			if(!JS_ValueToInt32(cx,*vp,(int32*)&(p->smb).retry_time))
				return JS_FALSE;
			break;
		case FB_PROP_RETRY_DELAY:
			if(!JS_ValueToInt32(cx,*vp,(int32*)&(p->smb).retry_delay))
				return JS_FALSE;
			break;
		case FB_PROP_NEW_FILE_TIME:
			if(!JS_ValueToInt32(cx, *vp, (int32*)&t))
				return JS_FALSE;
			update_newfiletime(&(p->smb), t);
			break;
	}

	return JS_TRUE;
}

static JSBool js_filebase_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval idval;
	char*		s=NULL;
	JSString*	js_str;
    jsint       tiny;
	idxrec_t	idx;
	private_t*	p;
	jsrefcount	rc;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_FALSE;

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch(tiny) {
		case FB_PROP_FILE:
			s=p->smb.file;
			break;
		case FB_PROP_LAST_ERROR:
			s=p->smb.last_error;
			break;
		case FB_PROP_STATUS:
			*vp = INT_TO_JSVAL(p->smb_result);
			break;
		case FB_PROP_RETRY_TIME:
			*vp = INT_TO_JSVAL(p->smb.retry_time);
			break;
		case FB_PROP_RETRY_DELAY:
			*vp = INT_TO_JSVAL(p->smb.retry_delay);
			break;
		case FB_PROP_FIRST_FILE:
			rc=JS_SUSPENDREQUEST(cx);
			memset(&idx,0,sizeof(idx));
			smb_getfirstidx(&(p->smb),&idx);
			JS_RESUMEREQUEST(cx, rc);
			*vp=UINT_TO_JSVAL(idx.number);
			break;
		case FB_PROP_LAST_FILE:
			rc=JS_SUSPENDREQUEST(cx);
			smb_getstatus(&(p->smb));
			JS_RESUMEREQUEST(cx, rc);
			*vp=UINT_TO_JSVAL(p->smb.status.last_file);
			break;
		case FB_PROP_LAST_FILE_TIME:
			rc=JS_SUSPENDREQUEST(cx);
			*vp = UINT_TO_JSVAL((uint32_t)lastfiletime(&p->smb));
			JS_RESUMEREQUEST(cx, rc);
			break;
		case FB_PROP_FILES:
			rc=JS_SUSPENDREQUEST(cx);
			smb_getstatus(&(p->smb));
			JS_RESUMEREQUEST(cx, rc);
			*vp=UINT_TO_JSVAL(p->smb.status.total_files);
			break;
		case FB_PROP_NEW_FILE_TIME:
			*vp = UINT_TO_JSVAL((uint32_t)newfiletime(&p->smb));
			break;
		case FB_PROP_MAX_CRCS:
			*vp=UINT_TO_JSVAL(p->smb.status.max_crcs);
			break;
		case FB_PROP_MAX_FILES:
			*vp=UINT_TO_JSVAL(p->smb.status.max_files);
			break;
		case FB_PROP_MAX_AGE:
			*vp=UINT_TO_JSVAL(p->smb.status.max_age);
			break;
		case FB_PROP_ATTR:
			*vp=UINT_TO_JSVAL(p->smb.status.attr);
			break;
		case FB_PROP_DIRNUM:
			*vp = INT_TO_JSVAL(p->smb.dirnum);
			break;
		case FB_PROP_IS_OPEN:
			*vp = BOOLEAN_TO_JSVAL(SMB_IS_OPEN(&(p->smb)));
			break;
	}

	if(s!=NULL) {
		if((js_str=JS_NewStringCopyZ(cx, s))==NULL)
			return JS_FALSE;
		*vp = STRING_TO_JSVAL(js_str);
	}

	return JS_TRUE;
}

#define FB_PROP_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static jsSyncPropertySpec js_filebase_properties[] = {
/*		 name				,tinyid					,flags,				ver	*/

	{	"error"				,FB_PROP_LAST_ERROR		,FB_PROP_FLAGS,		31900 },
	{	"last_error"		,FB_PROP_LAST_ERROR		,JSPROP_READONLY,	31900 },	/* alias */
	{	"status"			,FB_PROP_STATUS			,FB_PROP_FLAGS,		31900 },
	{	"file"				,FB_PROP_FILE			,FB_PROP_FLAGS,		31900 },
	{	"debug"				,FB_PROP_DEBUG			,0,					31900 },
	{	"retry_time"		,FB_PROP_RETRY_TIME		,JSPROP_ENUMERATE,	31900 },
	{	"retry_delay"		,FB_PROP_RETRY_DELAY	,JSPROP_ENUMERATE,	31900 },
	{	"first_file"		,FB_PROP_FIRST_FILE		,FB_PROP_FLAGS,		31900 },
	{	"last_file"			,FB_PROP_LAST_FILE		,FB_PROP_FLAGS,		31900 },
	{	"last_file_time"	,FB_PROP_LAST_FILE_TIME	,FB_PROP_FLAGS,		31900 },
	{	"files"				,FB_PROP_FILES			,FB_PROP_FLAGS,		31900 },
	{	"new_file_time"		,FB_PROP_NEW_FILE_TIME	,JSPROP_ENUMERATE,	31900 },
	{	"max_crcs"			,FB_PROP_MAX_CRCS		,FB_PROP_FLAGS,		31900 },
	{	"max_files"			,FB_PROP_MAX_FILES  	,FB_PROP_FLAGS,		31900 },
	{	"max_age"			,FB_PROP_MAX_AGE   		,FB_PROP_FLAGS,		31900 },
	{	"attributes"		,FB_PROP_ATTR			,FB_PROP_FLAGS,		31900 },
	{	"dirnum"			,FB_PROP_DIRNUM			,FB_PROP_FLAGS,		31900 },
	{	"is_open"			,FB_PROP_IS_OPEN		,FB_PROP_FLAGS,		31900 },
	{0}
};

#ifdef BUILD_JSDOCS
static char* filebase_prop_desc[] = {

	 "last occurred file base error - <small>READ ONLY</small>"
	,"return value of last <i>SMB Library</i> function call - <small>READ ONLY</small>"
	,"base path and filename of file base - <small>READ ONLY</small>"
	,"file base open/lock retry timeout (in seconds)"
	,"delay between file base open/lock retries (in milliseconds)"
	,"first file number - <small>READ ONLY</small>"
	,"last file number - <small>READ ONLY</small>"
	,"timestamp of last file - <small>READ ONLY</small>"
	,"total number of files - <small>READ ONLY</small>"
	,"timestamp of directory"
	,"maximum number of file CRCs to store (for dupe checking) - <small>READ ONLY</small>"
	,"maximum number of files before expiration - <small>READ ONLY</small>"
	,"maximum age (in days) of files to store - <small>READ ONLY</small>"
	,"file base attributes - <small>READ ONLY</small>"
	,"directory number (0-based) - <small>READ ONLY</small>"
	,"<i>true</i> if the file base has been opened successfully - <small>READ ONLY</small>"
	,NULL
};
#endif

static jsSyncMethodSpec js_filebase_functions[] = {
	{"open",			js_open,			0, JSTYPE_BOOLEAN
		,JSDOCSTR("")
		,JSDOCSTR("open file base")
		,31900
	},
	{"close",			js_close,			0, JSTYPE_BOOLEAN
		,JSDOCSTR("")
		,JSDOCSTR("close file base (if open)")
		,31900
	},
	{"get_file",		js_get_file,		0, JSTYPE_OBJECT
		,JSDOCSTR("string filename | object [,detail=FileBase.DETAIL.NORM]")
		,JSDOCSTR("get a file object")
		,31900
	},
	{"get_file_list",	js_get_file_list,	0, JSTYPE_ARRAY
		,JSDOCSTR("[string filespec] [,detail=FileBase.DETAIL.NORM] [,since_time=0] [,sort=true]")
		,JSDOCSTR("get a list of file objects")
		,31900
	},
	{"get_file_names",	js_get_file_names,	0, JSTYPE_ARRAY
		,JSDOCSTR("[string filespec] [,since_time=0] [,sort=true]")
		,JSDOCSTR("get a list of file names")
		,31900
	},
	{"get_file_path",	js_get_file_path,	1, JSTYPE_STRING
		,JSDOCSTR("string filename")
		,JSDOCSTR("get the full path to the local file")
		,31900
	},
	{"get_file_size",	js_get_file_size,	1, JSTYPE_NUMBER
		,JSDOCSTR("string filename")
		,JSDOCSTR("get the size of the local file, in bytes")
		,31900
	},
	{"get_file_time",	js_get_file_time,	1, JSTYPE_NUMBER
		,JSDOCSTR("string filename")
		,JSDOCSTR("get the modification date/time stamp of the local file")
		,31900
	},
	{"add_file",		js_add_file,		1, JSTYPE_BOOLEAN
		,JSDOCSTR("object")
		,JSDOCSTR("add a file to the file base")
		,31900
	},
	{"update_file",		js_update_file,		1, JSTYPE_BOOLEAN
		,JSDOCSTR("object")
		,JSDOCSTR("update an existing file in the file base")
		,31900
	},
	{"renew_file",		js_renew_file,		1, JSTYPE_BOOLEAN
		,JSDOCSTR("string filename")
		,JSDOCSTR("remove and re-add (as new) an existing file in the file base")
		,31900
	},
	{"remove_file",		js_remove_file,		1, JSTYPE_BOOLEAN
		,JSDOCSTR("string filename")
		,JSDOCSTR("remove an existing file from the file base")
		,31900
	},
	{"dump_file",		js_dump_file,		1, JSTYPE_ARRAY
		,JSDOCSTR("string filename")
		,JSDOCSTR("dump file metadata to an array of strings for diagnostic uses")
		,31900
	},
	{0}
};

static JSBool js_filebase_resolve(JSContext *cx, JSObject *obj, jsid id)
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

	ret=js_SyncResolve(cx, obj, name, js_filebase_properties, js_filebase_functions, NULL, 0);
	if(name)
		free(name);
	return ret;
}

static JSBool js_filebase_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_filebase_resolve(cx, obj, JSID_VOID));
}

JSClass js_filebase_class = {
     "FileBase"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_filebase_get		/* getProperty	*/
	,js_filebase_set		/* setProperty	*/
	,js_filebase_enumerate	/* enumerate	*/
	,js_filebase_resolve	/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_finalize_filebase	/* finalize		*/
};

/* FileBase Constructor (open file base) */

static JSBool
js_filebase_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *		obj;
	jsval *			argv=JS_ARGV(cx, arglist);
	JSString*		js_str;
	char*			base = NULL;
	private_t*		p;
	scfg_t*			scfg;

	scfg=JS_GetRuntimePrivate(JS_GetRuntime(cx));

	obj=JS_NewObject(cx, &js_filebase_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
	if((p=(private_t*)malloc(sizeof(private_t)))==NULL) {
		JS_ReportError(cx,"malloc failed");
		return JS_FALSE;
	}

	memset(p,0,sizeof(private_t));
	p->smb.retry_time=scfg->smb_retry_time;

	js_str = JS_ValueToString(cx, argv[0]);
	JSSTRING_TO_MSTRING(cx, js_str, base, NULL);
	if(JS_IsExceptionPending(cx)) {
		if(base != NULL)
			free(base);
		free(p);
		return JS_FALSE;
	}
	if(base==NULL) {
		JS_ReportError(cx, "Invalid base parameter");
		free(p);
		return JS_FALSE;
	}

	if(!JS_SetPrivate(cx, obj, p)) {
		JS_ReportError(cx,"JS_SetPrivate failed");
		free(p);
		free(base);
		return JS_FALSE;
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,obj,"Class used for accessing the file transfer databases", 31900);
	js_DescribeSyncConstructor(cx,obj,"To create a new FileBase object: "
		"<tt>var filebase = new FileBase('<i>code</i>')</tt><br>"
		"where <i>code</i> is a directory internal code."
		);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", filebase_prop_desc, JSPROP_READONLY);
#endif

	p->smb.dirnum = getdirnum(scfg, base);
	if(p->smb.dirnum >= 0 && p->smb.dirnum < scfg->total_dirs) {
		safe_snprintf(p->smb.file, sizeof(p->smb.file), "%s%s"
			,scfg->dir[p->smb.dirnum]->data_dir, scfg->dir[p->smb.dirnum]->code);
	} else { /* unknown code */
		SAFECOPY(p->smb.file, base);
	}

	free(base);
	return JS_TRUE;
}

JSObject* DLLCALL js_CreateFileBaseClass(JSContext* cx, JSObject* parent, scfg_t* cfg)
{
	JSObject*	obj;
	JSObject*	constructor;
	JSObject*	detail;
	jsval		val;

	obj = JS_InitClass(cx, parent, NULL
		,&js_filebase_class
		,js_filebase_constructor
		,1	/* number of constructor args */
		,NULL
		,NULL
		,NULL, NULL);

	if(JS_GetProperty(cx, parent, js_filebase_class.name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JS_ValueToObject(cx, val, &constructor);
		detail = JS_DefineObject(cx, constructor, "DETAIL", NULL, NULL, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
		if(detail != NULL) {
			JS_DefineProperty(cx, detail, "MIN", INT_TO_JSVAL(file_detail_index), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, detail, "NORM", INT_TO_JSVAL(file_detail_normal), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, detail, "EXT", INT_TO_JSVAL(file_detail_extdesc), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, detail, "MAX", INT_TO_JSVAL(file_detail_extdesc + 1), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
		}
	}
	return obj;
}
