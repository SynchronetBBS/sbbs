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
	const char* filename;

	if((filename = js_GetClassPrivate(cx, obj, &js_archive_class)) == NULL)
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
	if(*format == '\0' && (fext = getfext(filename)) != NULL)
		SAFECOPY(format,  fext + 1);
	if(*format == '\0')
		SAFECOPY(format, "zip");
	rc = JS_SUSPENDREQUEST(cx);
	long file_count = create_archive(filename, format, with_path, file_list, error, sizeof(error));
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
	const char* filename;
	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((filename = js_GetClassPrivate(cx, obj, &js_archive_class)) == NULL)
		return JS_FALSE;

 	if(!js_argc(cx, argc, 1))
		return JS_FALSE;

	if(JSVAL_NULL_OR_VOID(argv[0]))
		JS_ReportError(cx, "Invalid output directory specified (null or undefined)");
	else
		JSVALUE_TO_MSTRING(cx, argv[0], outdir, NULL);
	if(JS_IsExceptionPending(cx) || outdir == NULL) {
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
	long extracted = extract_files_from_archive(filename, outdir, allowed_filename_chars
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
	const char* filename;

	if((filename = js_GetClassPrivate(cx, obj, &js_archive_class)) == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	int result = archive_type(filename, type, sizeof(type));
	JS_RESUMEREQUEST(cx, rc);
	if(result >= 0) {
		JSString* js_str = JS_NewStringCopyZ(cx, type);
		*vp = STRING_TO_JSVAL(js_str);
	} else
		*vp = JSVAL_NULL;
	return JS_TRUE;
}

// TODO: consider making 'path' and 'case-sensitive' arguments to wildmatch() configurable via method arguments
static JSBool
js_list(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv = JS_ARGV(cx, arglist);
	JSObject *obj = JS_THIS_OBJECT(cx, arglist);
	jsval val;
	jsrefcount	rc;
	JSObject* array;
	JSString* js_str;
	struct archive *ar;
	struct archive_entry *entry;
	bool hash = false;
	int	result;
	const char* filename;
	char pattern[MAX_PATH + 1] = "";

	if((filename = js_GetClassPrivate(cx, obj, &js_archive_class)) == NULL)
		return JS_FALSE;

	uintN argn = 0;
	if(argc > argn && JSVAL_IS_BOOLEAN(argv[argn])) {
		hash = JSVAL_TO_BOOLEAN(argv[argn]);
		argn++;
	}
	if(argc > argn && JSVAL_IS_STRING(argv[argn])) {
		JSString* js_str = JS_ValueToString(cx, argv[argn]);
		if(js_str == NULL) {
			JS_ReportError(cx, "string conversion error");
			return JS_FALSE;
		}
		JSSTRING_TO_STRBUF(cx, js_str, pattern, sizeof(pattern), NULL);
		argn++;
	}

	if((ar = archive_read_new()) == NULL) {
		JS_ReportError(cx, "archive_read_new() returned NULL");
		return JS_FALSE;
	}
	archive_read_support_filter_all(ar);
	archive_read_support_format_all(ar);
	if((result = archive_read_open_filename(ar, filename, 10240)) != ARCHIVE_OK) {
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

	JSBool retval = JS_TRUE;
	rc = JS_SUSPENDREQUEST(cx);
	jsint len = 0;
	while(1) {
		result = archive_read_next_header(ar, &entry);
		if(result != ARCHIVE_OK) {
			if(result != ARCHIVE_EOF) {
				JS_ReportError(cx, "archive_read_next_header() returned %d: %s"
					,result, archive_error_string(ar));
				retval = JS_FALSE;
			}
			break;
		}

		const char* p = archive_entry_pathname(entry);
		if(p == NULL)
			continue;

		if(*pattern && !wildmatch(p, pattern, /* path: */false, /* case-sensitive: */false))
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

		if(hash && archive_entry_filetype(entry) == AE_IFREG) {
			MD5 md5_ctx;
			SHA1_CTX sha1_ctx;
			uint8_t md5[MD5_DIGEST_SIZE];
			uint8_t sha1[SHA1_DIGEST_SIZE];
			uint16_t crc16 = 0;
			uint32_t crc32 = 0;

			MD5_open(&md5_ctx);
			SHA1Init(&sha1_ctx);

			const void *buff;
			size_t size;
			int64_t offset;

			for(;;) {
				result = archive_read_data_block(ar, &buff, &size, &offset);
				if(result != ARCHIVE_OK)
					break;
				crc32 = crc32i(~crc32, buff, size);
				crc16 = icrc16(crc16, buff, size);
				MD5_digest(&md5_ctx, buff, size);
				SHA1Update(&sha1_ctx, buff, size);
			}
			MD5_close(&md5_ctx, md5);
			SHA1Final(&sha1_ctx, sha1);
			val = UINT_TO_JSVAL(crc16);
			if(!JS_SetProperty(cx, obj, "crc16", &val))
				break;
			val = UINT_TO_JSVAL(crc32);
			if(!JS_SetProperty(cx, obj, "crc32", &val))
				break;
			char hex[128];
			if((js_str = JS_NewStringCopyZ(cx, MD5_hex(hex, md5))) == NULL)
				break;
			val = STRING_TO_JSVAL(js_str);
			if(!JS_SetProperty(cx, obj, "md5", &val))
				break;
			if((js_str = JS_NewStringCopyZ(cx, SHA1_hex(hex, sha1))) == NULL)
				break;
			val = STRING_TO_JSVAL(js_str);
			if(!JS_SetProperty(cx, obj, "sha1", &val))
				break;
		}

		val = OBJECT_TO_JSVAL(obj);
        if(!JS_SetElement(cx, array, len++, &val))
			break;
	}
	archive_read_free(ar);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array));

	return retval;
}

static JSBool
js_read(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv = JS_ARGV(cx, arglist);
	JSObject *obj = JS_THIS_OBJECT(cx, arglist);
	jsrefcount	rc;
	struct archive *ar;
	struct archive_entry *entry;
	int	result;
	const char* filename;
	char pattern[MAX_PATH + 1] = "";

	if((filename = js_GetClassPrivate(cx, obj, &js_archive_class)) == NULL)
		return JS_FALSE;

 	if(!js_argc(cx, argc, 1))
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	uintN argn = 0;
	if(argc > argn && JSVAL_IS_STRING(argv[argn])) {
		JSString* js_str = JS_ValueToString(cx, argv[argn]);
		if(js_str == NULL) {
			JS_ReportError(cx, "string conversion error");
			return JS_FALSE;
		}
		JSSTRING_TO_STRBUF(cx, js_str, pattern, sizeof(pattern), NULL);
		argn++;
	}

	if((ar = archive_read_new()) == NULL) {
		JS_ReportError(cx, "archive_read_new() returned NULL");
		return JS_FALSE;
	}
	archive_read_support_filter_all(ar);
	archive_read_support_format_all(ar);
	if((result = archive_read_open_filename(ar, filename, 10240)) != ARCHIVE_OK) {
		JS_ReportError(cx, "archive_read_open_filename() returned %d: %s"
			,result, archive_error_string(ar));
		archive_read_free(ar);
		return JS_FALSE;
	}

	JSBool retval = JS_TRUE;
	rc = JS_SUSPENDREQUEST(cx);
	while(1) {
		result = archive_read_next_header(ar, &entry);
		if(result != ARCHIVE_OK) {
			if(result != ARCHIVE_EOF) {
				JS_ReportError(cx, "archive_read_next_header() returned %d: %s"
					,result, archive_error_string(ar));
				retval = JS_FALSE;
			}
			break;
		}

		if(archive_entry_filetype(entry) != AE_IFREG)
			continue;

		const char* pathname = archive_entry_pathname(entry);
		if(pathname == NULL)
			continue;

		if(stricmp(pathname, pattern) != 0)
			continue;

		char* p = NULL;
		size_t total = 0;
		const void *buff;
		size_t size;
		int64_t offset;

		for(;;) {
			result = archive_read_data_block(ar, &buff, &size, &offset);
			if(result != ARCHIVE_OK)
				break;
			char* np = realloc(p, total + size);
			if(np == NULL)
				break;
			p = np;
			memcpy(p + total, buff, size);
			total += size;
		}
		JSString* js_str = JS_NewStringCopyN(cx, p, total);
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
		free(p);
		break;
	}
	archive_read_free(ar);
	JS_RESUMEREQUEST(cx, rc);

	return retval;
}

static jsSyncMethodSpec js_archive_functions[] = {
	{ "create",		js_create,		1,	JSTYPE_NUMBER
		,JSDOCSTR("[string format] [,boolean with_path = false] [,array file_list]")
		,JSDOCSTR("create an archive of the specified format, returns the number of files archived, will throw exception upon error")
		,31900
	},
	{ "read",		js_read,		1,	JSTYPE_STRING
		,JSDOCSTR("string path/filename")
		,JSDOCSTR("read and return the contents of the specified archived text file")
		,31900
	},
	{ "extract",	js_extract,		1,	JSTYPE_NUMBER
		,JSDOCSTR("output_directory [,boolean with_path = false] [,number max_files = 0] [,string file/pattern [...]]")
		,JSDOCSTR("extract files from an archive to specified output directory, returns the number of files extracted, will throw exception upon error")
		,31900
	},
	{ "list",		js_list,		1,	JSTYPE_ARRAY
		,JSDOCSTR("[,boolean hash = false] [,string file/pattern]")
		,JSDOCSTR("get list of archive contents as an array of objects<br>"
			"archived object properties:<br>"
			"<ul>"
			"<li>string <tt>type</tt> - item type: 'file', 'link', or 'directory'"
			"<li>string <tt>name</tt> - item path/name"
			"<li>string <tt>path</tt> - source path"
			"<li>string <tt>symlink</tt>"
			"<li>string <tt>hardlink</tt>"
			"<li>number <tt>size</tt> - item size in bytes"
			"<li>number <tt>time</tt> - modification date/time in time_t format"
			"<li>number <tt>mode</tt> - permissions/mode flags"
			"<li>string <tt>user</tt> - owner name"
			"<li>string <tt>group</tt> - owner group"
			"<li>string <tt>format</tt> - archive format"
			"<li>string <tt>compression</tt> - compression method"
			"<li>string <tt>fflags</tt>"
			"<li>number <tt>crc16</tt> - 16-bit CRC, when hash is true and type is file"
			"<li>number <tt>crc32</tt> - 32-bit CRC, when hash is true and type is file"
			"<li>string <tt>md5</tt> - hexadecimal MD-5 sum, when hash is true and type is file"
			"<li>string <tt>sha1</tt> - hexadecimal SHA-1 sum, when hash is true and type is file"
			"</ul>"
			"when <tt>hash</tt> is <tt>true</tt>, calculates and returns hash/digest values of files in stored archive")
		,31900
	},
	{0}
};

#ifdef BUILD_JSDOCS
static char* archive_prop_desc[] = {

	 "format/compression type of archive file - <small>READ ONLY</small>"
	,NULL
};
#endif

static JSBool
js_archive_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj = JS_THIS_OBJECT(cx, arglist);
	jsval *argv = JS_ARGV(cx, arglist);
	JSString* str;
	char* filename;

	obj = JS_NewObject(cx, &js_archive_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
	if(argc < 1 || (str = JS_ValueToString(cx, argv[0]))==NULL) {
		JS_ReportError(cx, "No filename specified");
		return JS_FALSE;
	}

	JSSTRING_TO_MSTRING(cx, str, filename, NULL);

	if(!JS_SetPrivate(cx, obj, filename)) {
		JS_ReportError(cx, "JS_SetPrivate failed");
		return JS_FALSE;
	}

	if(!JS_DefineProperty(cx, obj, "type", JSVAL_VOID, js_archive_type, NULL, JSPROP_ENUMERATE|JSPROP_READONLY)) {
		JS_ReportError(cx, "JS_DefineProperty failed");
		return JS_FALSE;
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,obj,"Class used for opening, creating, reading, or writing archive files on the local file system"
		,31900
		);
	js_DescribeSyncConstructor(cx,obj,"To create a new Archive object: <tt>var a = new Archive(<i>filename</i>)</tt>");
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", archive_prop_desc, JSPROP_READONLY);
#endif

	return JS_TRUE;
}

static void js_finalize_archive(JSContext *cx, JSObject *obj)
{
	void* p;

	if((p = JS_GetPrivate(cx, obj)) == NULL)
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
