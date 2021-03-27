/* Synchronet JavaScript "Archive" Object */

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
#include "filedat.h"

/* libarchive: */
#include <archive.h>
#include <archive_entry.h>

#include <stdbool.h>

typedef struct
{
	char name[MAX_PATH + 1];
} archive_private_t;

JSClass js_archive_class;

static JSBool
js_create(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv = JS_ARGV(cx, arglist);
	JSObject *obj = JS_THIS_OBJECT(cx, arglist);
	char		format[32] = "";
	str_list_t	file_list = NULL;
	char		path[MAX_PATH + 1];
	bool		with_path = false;
	char		error[256] = "";
	jsval		val;
	jsrefcount	rc;
	archive_private_t* p;

	if((p = (archive_private_t*)js_GetClassPrivate(cx, obj, &js_archive_class)) == NULL)
		return JS_FALSE;

	if(!js_argc(cx, argc, 1))
		return JS_FALSE;

	uintN argn = 0;
	if(argn < argc && JSVAL_IS_STRING(argv[argn])) {
		JSString* js_str = JS_ValueToString(cx, argv[argn]);
		if(js_str == NULL) {
			JS_ReportError(cx, "string conversion error");
			return JS_FALSE;
		}
		JSSTRING_TO_STRBUF(cx, js_str, format, sizeof(format), NULL);
		argn++;
	}
	if(argc > argn && JSVAL_IS_BOOLEAN(argv[argn])) {
		with_path = JSVAL_TO_BOOLEAN(argv[argn]);
		argn++;
	}
	if(argn < argc && JSVAL_IS_OBJECT(argv[argn])) {
		JSObject* array = JSVAL_TO_OBJECT(argv[argn]);
		if(!JS_IsArrayObject(cx, array)) {
			JS_ReportError(cx, "invalid array object");
			return JS_FALSE;
		}
		file_list = strListInit();
		for(jsint i = 0;; i++)  {
			if(!JS_GetElement(cx, array, i, &val))
				break;
			if(!JSVAL_IS_STRING(val))
				break;
			JSVALUE_TO_STRBUF(cx, val, path, sizeof(path), NULL);
			strListPush(&file_list, path);
		}
		argn++;
	}

	char* fext;
	if(*format == '\0' && (fext = getfext(p->name)) != NULL)
		SAFECOPY(format,  fext + 1);
	if(*format == '\0')
		SAFECOPY(format, "zip");
	rc = JS_SUSPENDREQUEST(cx);
	long file_count = create_archive(p->name, format, with_path, file_list, error, sizeof(error));
	strListFree(&file_list);
	JS_RESUMEREQUEST(cx, rc);
	if(file_count < 0) {
		JS_ReportError(cx, error);
		return JS_FALSE;
	}
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(file_count));
	return JS_TRUE;
}

static JSBool
js_extract(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv = JS_ARGV(cx, arglist);
	JSObject *obj = JS_THIS_OBJECT(cx, arglist);
	char*		outdir = NULL;
	char*		allowed_filename_chars = SAFEST_FILENAME_CHARS;
	str_list_t	file_list = NULL;
	bool		with_path = false;
	int32		max_files = 0;
	char		error[256] = "";
	jsrefcount	rc;
	archive_private_t* p;
	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p = (archive_private_t*)js_GetClassPrivate(cx, obj, &js_archive_class)) == NULL)
		return JS_FALSE;

 	if(!js_argc(cx, argc, 1))
		return JS_FALSE;

	if(JSVAL_NULL_OR_VOID(argv[0]))
		JS_ReportError(cx, "Invalid output directory specified (null or undefined)");
	else
		JSVALUE_TO_MSTRING(cx, argv[0], outdir, NULL);
	if(JS_IsExceptionPending(cx)) {
		free(outdir);
		return JS_FALSE;
	}
	uintN argn = 1;
	if(argc > argn && JSVAL_IS_BOOLEAN(argv[argn])) {
		with_path = JSVAL_TO_BOOLEAN(argv[argn]);
		if(with_path)
			allowed_filename_chars = NULL;	// We trust this archive
		argn++;
	}
	if(argc > argn && JSVAL_IS_NUMBER(argv[argn])) {
		if(!JS_ValueToInt32(cx, argv[argn], &max_files)) {
			free(outdir);
			strListFree(&file_list);
			return JS_FALSE;
		}
		argn++;
	}
	for(; argn < argc; argn++) {
		if(JSVAL_IS_STRING(argv[argn])) {
			char path[MAX_PATH + 1];
			JSVALUE_TO_STRBUF(cx, argv[argn], path, sizeof(path), NULL);
			strListPush(&file_list, path);
		}
	}

	rc = JS_SUSPENDREQUEST(cx);
	long extracted = extract_files_from_archive(p->name, outdir, allowed_filename_chars
		,with_path, (ulong)max_files, file_list, error, sizeof(error));
	strListFree(&file_list);
	free(outdir);
	JS_RESUMEREQUEST(cx, rc);
	if(*error != '\0') {
		if(extracted >= 0)
			JS_ReportError(cx, "%s (after extracting %ld items successfully)", error, extracted);
		else
			JS_ReportError(cx, "%s", error);
		return JS_FALSE;
	}
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(extracted));
	return JS_TRUE;
}

/* getter */
static JSBool
js_archive_type(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	char		type[256] = "";
	jsrefcount	rc;
	archive_private_t* p;

	if((p = (archive_private_t*)js_GetClassPrivate(cx, obj, &js_archive_class)) == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	int result = archive_type(p->name, type, sizeof(type));
	JS_RESUMEREQUEST(cx, rc);
	if(result >= 0) {
		JSString* js_str = JS_NewStringCopyZ(cx, type);
		*vp = STRING_TO_JSVAL(js_str);
	} else
		*vp = JSVAL_NULL;
	return JS_TRUE;
}

/* getter */
static JSBool
js_archive_directory(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval val;
	jsrefcount	rc;
	JSObject* array;
	JSString* js_str;
	struct archive *ar;
	struct archive_entry *entry;
	int	result;
	archive_private_t* p;

	if((p = (archive_private_t*)js_GetClassPrivate(cx, obj, &js_archive_class)) == NULL)
		return JS_FALSE;

	if((ar = archive_read_new()) == NULL) {
		JS_ReportError(cx, "archive_read_new() returned NULL");
		return JS_FALSE;
	}
	archive_read_support_filter_all(ar);
	archive_read_support_format_all(ar);
	if((result = archive_read_open_filename(ar, p->name, 10240)) != ARCHIVE_OK) {
		JS_ReportError(cx, "archive_read_open_filename() returned %d: %s"
			,result, archive_error_string(ar));
		archive_read_free(ar);
		return JS_FALSE;
	}

    if((array = JS_NewArrayObject(cx, 0, NULL))==NULL) {
		JS_ReportError(cx, "JS_NewArrayObject() returned NULL");
		archive_read_free(ar);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	jsint len = 0;
	while(1) {
		result = archive_read_next_header(ar, &entry);
		if(result != ARCHIVE_OK) {
			if(result != ARCHIVE_EOF)
				JS_ReportError(cx, "archive_read_next_header() returned %d: %s"
					,result, archive_error_string(ar));
			break;
		}

		const char* p = archive_entry_pathname(entry);
		if(p == NULL)
			continue;

		const char* type;
		switch(archive_entry_filetype(entry)) {
			case AE_IFREG:
				type = "file";
				break;
			case AE_IFLNK:
				type = "link";
				break;
			case AE_IFDIR:
				type = "directory";
				break;
			default:
				continue;
		}

		JSObject* obj = JS_NewObject(cx, NULL, NULL, NULL);
		if(obj == NULL)
			break;

		js_str = JS_NewStringCopyZ(cx, type);
		if(js_str == NULL)
			break;
		val = STRING_TO_JSVAL(js_str);
		JS_SetProperty(cx, obj, "type", &val);

		js_str = JS_NewStringCopyZ(cx, p);
		if(js_str == NULL)
			break;
		val = STRING_TO_JSVAL(js_str);
		JS_SetProperty(cx, obj, "name", &val);

		if((p = archive_entry_sourcepath(entry)) != NULL) {
			js_str = JS_NewStringCopyZ(cx, p);
			if(js_str == NULL)
				break;
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, obj, "path", &val);
		}

		if((p = archive_entry_symlink(entry)) != NULL) {
			js_str = JS_NewStringCopyZ(cx, p);
			if(js_str == NULL)
				break;
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, obj, "symlink", &val);

//			val = INT_TO_JSVAL(archive_entry_symlink_type(entry));
//			JS_SetProperty(cx, obj, "symlink_type", &val);
		}

		if((p = archive_entry_hardlink(entry)) != NULL) {
			js_str = JS_NewStringCopyZ(cx, p);
			if(js_str == NULL)
				break;
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, obj, "hardlink", &val);
		}

		val = DOUBLE_TO_JSVAL((jsdouble)archive_entry_size(entry));
		JS_SetProperty(cx, obj, "size", &val);

		val = DOUBLE_TO_JSVAL((jsdouble)archive_entry_mtime(entry));
		JS_SetProperty(cx, obj, "time", &val);

		val = INT_TO_JSVAL(archive_entry_mode(entry));
		JS_SetProperty(cx, obj, "mode", &val);

		if((p = archive_entry_uname(entry)) != NULL) {
			js_str = JS_NewStringCopyZ(cx, p);
			if(js_str == NULL)
				break;
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, obj, "user", &val);
		}

		if((p = archive_entry_gname(entry)) != NULL) {
			js_str = JS_NewStringCopyZ(cx, p);
			if(js_str == NULL)
				break;
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, obj, "group", &val);
		}

		if((p = archive_format_name(ar)) != NULL) {
			js_str = JS_NewStringCopyZ(cx, p);
			if(js_str == NULL)
				break;
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, obj, "format", &val);
		}

		if((p = archive_filter_name(ar, 0)) != NULL) {
			js_str = JS_NewStringCopyZ(cx, p);
			if(js_str == NULL)
				break;
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, obj, "compression", &val);
		}

		if((p = archive_entry_fflags_text(entry)) != NULL) {
			js_str = JS_NewStringCopyZ(cx, p);
			if(js_str == NULL)
				break;
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, obj, "fflags", &val);
		}

		val = OBJECT_TO_JSVAL(obj);
        if(!JS_SetElement(cx, array, len++, &val))
			break;
	}
	archive_read_free(ar);
	JS_RESUMEREQUEST(cx, rc);
	*vp = OBJECT_TO_JSVAL(array);

	return JS_TRUE;
}

static jsSyncMethodSpec js_archive_functions[] = {
	{ "create",		js_create,		1,	JSTYPE_NUMBER
		,JSDOCSTR("[string format] [,boolean with_path = <tt>false</tt>] [,array file_list]")
		,JSDOCSTR("create an archive of the specified format, returns the number of files archived, will throw exception upon error")
		,31900
	},
	{ "extract",	js_extract,		1,	JSTYPE_NUMBER
		,JSDOCSTR("output_directory [,boolean with_path = <tt>false</tt>] [,number max_files = 0] [,string file/pattern [...]]")
		,JSDOCSTR("extract files from an archive to specified output directory, returns the number of files extracted, will throw exception upon error")
		,31900
	},
	{0}
};

static JSBool
js_archive_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj = JS_THIS_OBJECT(cx, arglist);
	jsval *argv = JS_ARGV(cx, arglist);
	JSString* str;
	archive_private_t* p;

	obj = JS_NewObject(cx, &js_archive_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
	if(argc < 1 || (str = JS_ValueToString(cx, argv[0]))==NULL) {
		JS_ReportError(cx, "No filename specified");
		return JS_FALSE;
	}

	if((p=(archive_private_t*)calloc(1,sizeof(*p)))==NULL) {
		JS_ReportError(cx, "calloc failed");
		return JS_FALSE;
	}

	JSSTRING_TO_STRBUF(cx, str, p->name, sizeof(p->name), NULL);

	if(!JS_SetPrivate(cx, obj, p)) {
		JS_ReportError(cx, "JS_SetPrivate failed");
		return JS_FALSE;
	}

	if(!JS_DefineProperty(cx, obj, "type", JSVAL_VOID, js_archive_type, NULL, 0)) {
		JS_ReportError(cx, "JS_DefineProperty failed");
		return JS_FALSE;
	}

	if(!JS_DefineProperty(cx, obj, "directory", JSVAL_VOID, js_archive_directory, NULL, 0)) {
		JS_ReportError(cx, "JS_DefineProperty failed");
		return JS_FALSE;
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,obj,"Class used for opening, creating, reading, or writing files on the local file system<p>"
		"Special features include:</h2><ol type=disc>"
			"<li>Exclusive-access files (default) or shared files<ol type=circle>"
				"<li>optional record-locking"
				"<li>buffered or non-buffered I/O"
				"</ol>"
			"<li>Support for binary files<ol type=circle>"
				"<li>native or network byte order (endian)"
				"<li>automatic Unix-to-Unix (<i>UUE</i>), yEncode (<i>yEnc</i>) or Base64 encoding/decoding"
				"</ol>"
			"<li>Support for ASCII text files<ol type=circle>"
				"<li>supports line-based I/O<ol type=square>"
					"<li>entire file may be read or written as an array of strings"
					"<li>individual lines may be read or written one line at a time"
					"</ol>"
				"<li>supports fixed-length records<ol type=square>"
					"<li>optional end-of-text (<i>etx</i>) character for automatic record padding/termination"
					"<li>Synchronet <tt>.dat</tt> files use an <i>etx</i> value of 3 (Ctrl-C)"
					"</ol>"
				"<li>supports <tt>.ini</tt> formated configuration files<ol type=square>"
					"<li>concept and support of <i>root</i> ini sections added in v3.12"
					"</ol>"
				"<li>optional ROT13 encoding/translation"
				"</ol>"
			"<li>Dynamically-calculated industry standard checksums (e.g. CRC-16, CRC-32, MD5)"
			"</ol>"
			,310
			);
	js_DescribeSyncConstructor(cx,obj,"To create a new File object: <tt>var f = new File(<i>filename</i>)</tt>");
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", file_prop_desc, JSPROP_READONLY);
#endif

	return JS_TRUE;
}

static void js_finalize_archive(JSContext *cx, JSObject *obj)
{
	archive_private_t* p;

	if((p = (archive_private_t*)JS_GetPrivate(cx, obj)) == NULL)
		return;
	free(p);
	JS_SetPrivate(cx, obj, NULL);
}

static JSBool js_archive_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*			name=NULL;
	JSBool			ret;

	if(id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;

		JS_IdToValue(cx, id, &idval);
		if(JSVAL_IS_STRING(idval))
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
	}

	ret=js_SyncResolve(cx, obj, name, NULL, js_archive_functions, NULL, 0);
	if(name)
		free(name);
	return ret;
}

static JSBool js_archive_enumerate(JSContext *cx, JSObject *obj)
{
	return js_archive_resolve(cx, obj, JSID_VOID);
}

JSClass js_archive_class = {
     "Archive"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,JS_PropertyStub		/* getProperty	*/
	,JS_StrictPropertyStub	/* setProperty	*/
	,js_archive_enumerate	/* enumerate	*/
	,js_archive_resolve		/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_finalize_archive	/* finalize		*/
};

JSObject* js_CreateArchiveClass(JSContext* cx, JSObject* parent)
{
	return JS_InitClass(cx, parent, NULL
		,&js_archive_class
		,js_archive_constructor
		,1		/* number of constructor args */
		,NULL	/* props, set in constructor */
		,NULL	/* funcs, set in constructor */
		,NULL, NULL);
}
