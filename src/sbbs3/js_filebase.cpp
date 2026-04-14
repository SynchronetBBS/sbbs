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
#include "sauce.h"
#include "filedat.h"

typedef struct
{
	smb_t smb;
	int smb_result;

} private_t;

/* Destructor */

static void js_finalize_filebase(JS::GCContext* gcx, JSObject *obj)
{
	private_t* p;

	if ((p = (private_t*)JS_GetPrivate(obj)) == NULL)
		return;

	if (SMB_IS_OPEN(&(p->smb)))
		smb_close(&(p->smb));

	free(p);

	JS_SetPrivate(obj, NULL);
}

/* Methods */

extern JSClass js_filebase_class;

static JSBool
js_open(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	private_t* p;
	scfg_t*    scfg;

	scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));
	if (scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class)) == NULL) {
		return JS_FALSE;
	}

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if (!dirnum_is_valid(scfg, p->smb.dirnum)) {
		const char* fname = getfname(p->smb.file);
		if (fname == p->smb.file || illegal_filename(fname)) {
			JS_ReportError(cx, "Invalid filebase path: '%s'", p->smb.file);
			return JS_FALSE;
		}
	}

	if (!dirnum_is_valid(scfg, p->smb.dirnum))
		p->smb_result = smb_open(&(p->smb));
	else
		p->smb_result = smb_open_dir(scfg, &(p->smb), p->smb.dirnum);
	if (p->smb_result != SMB_SUCCESS) {
		return JS_TRUE;
	}

	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	return JS_TRUE;
}

static JSBool
js_close(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	private_t* p;

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class)) == NULL) {
		return JS_FALSE;
	}

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	smb_close(&(p->smb));

	return JS_TRUE;
}

static JSBool
js_lock(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	private_t* p;

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class)) == NULL) {
		return JS_FALSE;
	}

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	p->smb_result = smb_lock(&(p->smb));
	if (p->smb_result != SMB_SUCCESS) {
		return JS_TRUE;
	}
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);

	return JS_TRUE;
}

static JSBool
js_unlock(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	private_t* p;

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class)) == NULL) {
		return JS_FALSE;
	}

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	p->smb_result = smb_unlock(&(p->smb));
	if (p->smb_result != SMB_SUCCESS) {
		return JS_TRUE;
	}
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);

	return JS_TRUE;
}


static JSBool
js_dump_file(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	jsval*     argv = JS_ARGV(cx, arglist);
	private_t* p;
	char*      filename = NULL;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class)) == NULL)
		return JS_FALSE;

	if (!SMB_IS_OPEN(&(p->smb))) {
		JS_ReportError(cx, "FileBase is not open");
		return JS_FALSE;
	}

	uintN argn = 0;
	if (argn < argc) {
		JSVALUE_TO_MSTRING(cx, argv[argn], filename, NULL);
		HANDLE_PENDING(cx, filename);
		argn++;
	}
	if (filename == NULL)
		return JS_FALSE;

	file_t file;
	if ((p->smb_result = smb_loadfile(&p->smb, filename, &file, file_detail_normal)) == SMB_SUCCESS) {
		str_list_t list = smb_msghdr_str_list(&file);
		if (list != NULL) {
			JS::RootedObject array(cx);
			array = JS_NewArrayObject(cx, 0, NULL);
			if (array == nullptr) {
				free(filename);
				strListFree(&list);
				JS_ReportError(cx, "JS_NewArrayObject failure");
				return JS_FALSE;
			}
			JS::RootedString js_str(cx);
			for (int i = 0; list[i] != NULL; i++) {
				js_str = JS_NewStringCopyZ(cx, list[i]);
				if (js_str == nullptr)
					break;
				JS_DefineElement(cx, array.get(), i, STRING_TO_JSVAL(js_str.get()), NULL, NULL, JSPROP_ENUMERATE);
			}
			strListFree(&list);
			JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array.get()));
		}
	}
	smb_freefilemem(&file);
	free(filename);
	return JS_TRUE;
}

static bool
set_file_properties(JSContext *cx, JS::HandleObject obj, file_t* f, enum file_detail detail)
{
	char             path[MAX_PATH + 1];
	jsval            val;
	JS::RootedString js_str(cx);
	const uintN      flags = JSPROP_ENUMERATE;

	scfg_t*     scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));
	if (scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}
	if (f->name == NULL
	    || (js_str = JS_NewStringCopyZ(cx, f->name)) == nullptr
	    || !JS_DefineProperty(cx, obj.get(), "name", STRING_TO_JSVAL(js_str.get()), NULL, NULL, flags))
		return false;

	if ((js_str = JS_NewStringCopyZ(cx, getfilevpath(scfg, f, path, sizeof path))) == nullptr
	    || !JS_DefineProperty(cx, obj.get(), "vpath", STRING_TO_JSVAL(js_str.get()), NULL, NULL, flags | JSPROP_READONLY))
		return false;

	if (((f->from != NULL && *f->from != '\0') || detail > file_detail_auxdata)
	    && ((js_str = JS_NewStringCopyZ(cx, f->from)) == nullptr
	        || !JS_DefineProperty(cx, obj.get(), "from", STRING_TO_JSVAL(js_str.get()), NULL, NULL, flags)))
		return false;

	if (((f->from_ip != NULL && *f->from_ip != '\0') || detail > file_detail_auxdata)
	    && ((js_str = JS_NewStringCopyZ(cx, f->from_ip)) == nullptr
	        || !JS_DefineProperty(cx, obj.get(), "from_ip_addr", STRING_TO_JSVAL(js_str.get()), NULL, NULL, flags)))
		return false;

	if (((f->from_host != NULL && *f->from_host != '\0') || detail > file_detail_auxdata)
	    && ((js_str = JS_NewStringCopyZ(cx, f->from_host)) == nullptr
	        || !JS_DefineProperty(cx, obj.get(), "from_host_name", STRING_TO_JSVAL(js_str.get()), NULL, NULL, flags)))
		return false;

	if (((f->from_prot != NULL && *f->from_prot != '\0') || detail > file_detail_auxdata)
	    && ((js_str = JS_NewStringCopyZ(cx, f->from_prot)) == nullptr
	        || !JS_DefineProperty(cx, obj.get(), "from_protocol", STRING_TO_JSVAL(js_str.get()), NULL, NULL, flags)))
		return false;

	if (((f->from_port != NULL && *f->from_port != '\0') || detail > file_detail_auxdata)
	    && ((js_str = JS_NewStringCopyZ(cx, f->from_port)) == nullptr
	        || !JS_DefineProperty(cx, obj.get(), "from_port", STRING_TO_JSVAL(js_str.get()), NULL, NULL, flags)))
		return false;

	if (((f->author != NULL && *f->author != '\0') || detail > file_detail_auxdata)
	    && ((js_str = JS_NewStringCopyZ(cx, f->author)) == nullptr
	        || !JS_DefineProperty(cx, obj.get(), "author", STRING_TO_JSVAL(js_str.get()), NULL, NULL, flags)))
		return false;

	if (((f->author_org != NULL && *f->author_org != '\0') || detail > file_detail_auxdata)
	    && ((js_str = JS_NewStringCopyZ(cx, f->author_org)) == nullptr
	        || !JS_DefineProperty(cx, obj.get(), "author_org", STRING_TO_JSVAL(js_str.get()), NULL, NULL, flags)))
		return false;

	if (((f->to_list != NULL && *f->to_list != '\0') || detail > file_detail_auxdata)
	    && ((js_str = JS_NewStringCopyZ(cx, f->to_list)) == nullptr
	        || !JS_DefineProperty(cx, obj.get(), "to_list", STRING_TO_JSVAL(js_str.get()), NULL, NULL, flags)))
		return false;

	val = BOOLEAN_TO_JSVAL(f->idx.attr & FILE_ANONYMOUS);
	if ((val == JSVAL_TRUE || detail > file_detail_auxdata)
	    && !JS_DefineProperty(cx, obj.get(), "anon", val, NULL, NULL, flags))
		return false;

	if (((f->tags != NULL && *f->tags != '\0') || detail > file_detail_auxdata)
	    && ((js_str = JS_NewStringCopyZ(cx, f->tags)) == nullptr
	        || !JS_DefineProperty(cx, obj.get(), "tags", STRING_TO_JSVAL(js_str.get()), NULL, NULL, flags)))
		return false;

	if (((f->desc != NULL && *f->desc != '\0') || detail > file_detail_auxdata)
	    && ((js_str = JS_NewStringCopyZ(cx, f->desc)) == nullptr
	        || !JS_DefineProperty(cx, obj.get(), "desc", STRING_TO_JSVAL(js_str.get()), NULL, NULL, flags)))
		return false;

	if (((f->extdesc != NULL && *f->extdesc != '\0') || detail > file_detail_auxdata)
	    && ((js_str = JS_NewStringCopyZ(cx, f->extdesc)) == nullptr
	        || !JS_DefineProperty(cx, obj.get(), "extdesc", STRING_TO_JSVAL(js_str.get()), NULL, NULL, flags)))
		return false;

	if (f->cost > 0 || detail > file_detail_auxdata) {
		val = DOUBLE_TO_JSVAL((double)f->cost);
		if (!JS_DefineProperty(cx, obj.get(), "cost", val, NULL, NULL, flags))
			return false;
	}
	if (dirnum_is_valid(scfg, f->dir) && (scfg->dir[f->dir]->misc & DIR_FCHK) && detail >= file_detail_normal)
		val = DOUBLE_TO_JSVAL((double)getfilesize(scfg, f));
	else
		val = DOUBLE_TO_JSVAL((double)smb_getfilesize(&f->idx));
	if (!JS_DefineProperty(cx, obj.get(), "size", val, NULL, NULL, flags))
		return false;

	if (dirnum_is_valid(scfg, f->dir) && (scfg->dir[f->dir]->misc & DIR_FCHK) && detail >= file_detail_normal)
		val = DOUBLE_TO_JSVAL((double)getfiletime(scfg, f));
	else
		val = UINT_TO_JSVAL(f->hdr.when_written.time);
	if (!JS_DefineProperty(cx, obj.get(), "time", val, NULL, NULL, flags))
		return false;
	if (f->hdr.when_imported.time > 0 || detail > file_detail_auxdata) {
		val = UINT_TO_JSVAL(f->hdr.when_imported.time);
		if (!JS_DefineProperty(cx, obj.get(), "added", val, NULL, NULL, flags))
			return false;
	}
	if (f->hdr.last_downloaded > 0 || detail > file_detail_auxdata) {
		val = UINT_TO_JSVAL(f->hdr.last_downloaded);
		if (!JS_DefineProperty(cx, obj.get(), "last_downloaded", val, NULL, NULL, flags))
			return false;
	}
	if (f->hdr.times_downloaded > 0 || detail > file_detail_auxdata) {
		val = UINT_TO_JSVAL(f->hdr.times_downloaded);
		if (!JS_DefineProperty(cx, obj.get(), "times_downloaded", val, NULL, NULL, flags))
			return false;
	}
	if (f->file_idx.hash.flags & SMB_HASH_CRC16) {
		val = UINT_TO_JSVAL(f->file_idx.hash.data.crc16);
		if (!JS_DefineProperty(cx, obj.get(), "crc16", val, NULL, NULL, flags))
			return false;
	}
	if (f->file_idx.hash.flags & SMB_HASH_CRC32) {
		val = UINT_TO_JSVAL(f->file_idx.hash.data.crc32);
		if (!JS_DefineProperty(cx, obj.get(), "crc32", val, NULL, NULL, flags))
			return false;
	}
	if (f->file_idx.hash.flags & SMB_HASH_MD5) {
		char hex[128];
		if ((js_str = JS_NewStringCopyZ(cx, MD5_hex(hex, f->file_idx.hash.data.md5))) == nullptr
		    || !JS_DefineProperty(cx, obj.get(), "md5", STRING_TO_JSVAL(js_str.get()), NULL, NULL, flags))
			return false;
	}
	if (f->file_idx.hash.flags & SMB_HASH_SHA1) {
		char hex[128];
		if ((js_str = JS_NewStringCopyZ(cx, SHA1_hex(hex, f->file_idx.hash.data.sha1))) == nullptr
		    || !JS_DefineProperty(cx, obj.get(), "sha1", STRING_TO_JSVAL(js_str.get()), NULL, NULL, flags))
			return false;
	}

	if (((f->auxdata != NULL && *f->auxdata != '\0') || detail > file_detail_auxdata)
	    && ((js_str = JS_NewStringCopyZ(cx, f->auxdata)) == nullptr
	        || !JS_DefineProperty(cx, obj.get(), "auxdata", STRING_TO_JSVAL(js_str.get()), NULL, NULL, flags)))
		return false;

	return true;
}

static char*
parse_file_name(JSContext *cx, JSObject* obj)
{
	char*       cp = NULL;
	jsval       val;
	const char* prop_name = "name";

	if (JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_MSTRING(cx, val, cp, NULL);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return NULL;
		}
		return cp;
	}
	JS_ReportError(cx, "Missing '%s' string in file object", prop_name);
	return NULL;
}

static BOOL
parse_file_index_properties(JSContext *cx, JSObject* obj, fileidxrec_t* idx)
{
	char*       cp = NULL;
	size_t      cp_sz = 0;
	jsval       val;
	const char* prop_name;

	if (JS_GetProperty(cx, obj, prop_name = "name", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return FALSE;
		}
		SAFECOPY(idx->name, cp);
	}
	if (JS_GetProperty(cx, obj, prop_name = "size", &val) && !JSVAL_NULL_OR_VOID(val)) {
		jsdouble d;
		if (JS_ValueToNumber(cx, val, &d))
			smb_setfilesize(&idx->idx, (uint64_t)d);
	}
	if (JS_GetProperty(cx, obj, prop_name = "crc16", &val) && !JSVAL_NULL_OR_VOID(val)) {
		idx->hash.data.crc16 = JSVAL_TO_INT(val);
		idx->hash.flags |= SMB_HASH_CRC16;
	}
	if (JS_GetProperty(cx, obj, prop_name = "crc32", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToECMAUint32(cx, val, &idx->hash.data.crc32)) {
			free(cp);
			JS_ReportError(cx, "Error converting adding '%s' property to Uint32", prop_name);
			return FALSE;
		}
		idx->hash.flags |= SMB_HASH_CRC32;
	}
	if (JS_GetProperty(cx, obj, prop_name = "md5", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL || strlen(cp) != MD5_DIGEST_SIZE * 2) {
			free(cp);
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return FALSE;
		}
		for (int i = 0; i < MD5_DIGEST_SIZE * 2; i += 2) {
			idx->hash.data.md5[i / 2] = HEX_CHAR_TO_INT(*(cp + i)) * 16;
			idx->hash.data.md5[i / 2] += HEX_CHAR_TO_INT(*(cp + i + 1));
		}
		idx->hash.flags |= SMB_HASH_MD5;
	}
	if (JS_GetProperty(cx, obj, prop_name = "sha1", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL || strlen(cp) != SHA1_DIGEST_SIZE * 2) {
			free(cp);
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return FALSE;
		}
		for (int i = 0; i < SHA1_DIGEST_SIZE * 2; i += 2) {
			idx->hash.data.sha1[i / 2] = HEX_CHAR_TO_INT(*(cp + i)) * 16;
			idx->hash.data.sha1[i / 2] += HEX_CHAR_TO_INT(*(cp + i + 1));
		}
		idx->hash.flags |= SMB_HASH_SHA1;
	}
	free(cp);
	return TRUE;
}

static int
parse_file_properties(JSContext *cx, JSObject* obj, file_t* file, char** extdesc, char** auxdata)
{
	char*       cp = NULL;
	size_t      cp_sz = 0;
	jsval       val;
	int         result = SMB_ERR_NOT_FOUND;

	const char* prop_name = "name";
	if (JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return SMB_FAILURE;
		}
		if ((result = smb_new_hfield_str(file, SMB_FILENAME, cp)) != SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding '%s' property to file object", result, prop_name);
			return result;
		}
	}

	prop_name = "to_list";
	if (JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return SMB_FAILURE;
		}
		if ((file->to_list != NULL || *cp != '\0') && (result = smb_new_hfield_str(file, RECIPIENTLIST, cp)) != SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding '%s' property to file object", result, prop_name);
			return result;
		}
	}

	prop_name = "from";
	if (JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return SMB_FAILURE;
		}
		if ((file->from != NULL || *cp != '\0') && (result = smb_new_hfield_str(file, SMB_FILEUPLOADER, cp)) != SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding '%s' property to file object", result, prop_name);
			return result;
		}
	}

	prop_name = "from_ip_addr";
	if (JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return SMB_FAILURE;
		}
		if (smb_new_hfield_str(file, SENDERIPADDR, cp) != SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding '%s' property to file object", result, prop_name);
			return result;
		}
	}

	prop_name = "from_host_name";
	if (JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return SMB_FAILURE;
		}
		if (smb_new_hfield_str(file, SENDERHOSTNAME, cp) != SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding '%s' property to file object", result, prop_name);
			return result;
		}
	}

	prop_name = "from_protocol";
	if (JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return SMB_FAILURE;
		}
		if (smb_new_hfield_str(file, SENDERPROTOCOL, cp) != SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding '%s' property to file object", result, prop_name);
			return result;
		}
	}

	prop_name = "from_port";
	if (JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return SMB_FAILURE;
		}
		if (smb_new_hfield_str(file, SENDERPORT, cp) != SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding '%s' property to file object", result, prop_name);
			return result;
		}
	}

	prop_name = "author";
	if (JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return SMB_FAILURE;
		}
		if (smb_new_hfield_str(file, SMB_AUTHOR, cp) != SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding '%s' property to file object", result, prop_name);
			return result;
		}
	}

	prop_name = "author_org";
	if (JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return SMB_FAILURE;
		}
		if (smb_new_hfield_str(file, SMB_AUTHOR_ORG, cp) != SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding '%s' property to file object", result, prop_name);
			return result;
		}
	}

	prop_name = "desc";
	if (JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return SMB_FAILURE;
		}
		if ((file->desc != NULL || *cp != '\0') && (result = smb_new_hfield_str(file, SMB_FILEDESC, cp)) != SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding '%s' property to file object", result, prop_name);
			return result;
		}
	}
	prop_name = "extdesc";
	if (extdesc != NULL && JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		FREE_AND_NULL(*extdesc);
		JSVALUE_TO_MSTRING(cx, val, *extdesc, NULL);
		HANDLE_PENDING(cx, *extdesc);
		if (*extdesc == NULL) {
			free(cp);
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return SMB_ERR_MEM;
		}
		truncsp(*extdesc);
	}
	prop_name = "auxdata";
	if (auxdata != NULL && JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		FREE_AND_NULL(*auxdata);
		JSVALUE_TO_MSTRING(cx, val, *auxdata, NULL);
		HANDLE_PENDING(cx, *auxdata);
		if (*auxdata == NULL) {
			free(cp);
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return SMB_ERR_MEM;
		}
		truncsp(*auxdata);
	}
	prop_name = "tags";
	if (JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid '%s' string in file object", prop_name);
			return SMB_FAILURE;
		}
		if ((file->tags != NULL || *cp != '\0') && (result = smb_new_hfield_str(file, SMB_TAGS, cp)) != SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding '%s' property to file object", result, prop_name);
			return result;
		}
	}
	prop_name = "cost";
	if (JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		jsdouble d;
		if (JS_ValueToNumber(cx, val, &d)) {
			uint64_t cost = (uint64_t)d;
			if ((file->cost != 0 || cost != 0) && (result = smb_new_hfield(file, SMB_COST, sizeof(cost), &cost)) != SMB_SUCCESS) {
				free(cp);
				JS_ReportError(cx, "Error %d adding '%s' property to file object", result, prop_name);
				return result;
			}
		}
	}
	prop_name = "added";
	if (JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		uint32_t t = 0;
		if (!JS_ValueToECMAUint32(cx, val, &t)) {
			free(cp);
			JS_ReportError(cx, "Error converting '%s' property to Uint32", prop_name);
			return SMB_FAILURE;
		}
		file->hdr.when_imported.time = t;
	}
	prop_name = "last_downloaded";
	if (JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		uint32_t t = 0;
		if (!JS_ValueToECMAUint32(cx, val, &t)) {
			free(cp);
			JS_ReportError(cx, "Error converting '%s' property to Uint32", prop_name);
			return SMB_FAILURE;
		}
		file->hdr.last_downloaded = t;
	}
	prop_name = "times_downloaded";
	if (JS_GetProperty(cx, obj, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		uint32_t t = 0;
		if (!JS_ValueToECMAUint32(cx, val, &t)) {
			free(cp);
			JS_ReportError(cx, "Error converting '%s' property to Uint32", prop_name);
			return SMB_FAILURE;
		}
		file->hdr.times_downloaded = t;
	}

	if (JS_GetProperty(cx, obj, "anon", &val) && val == JSVAL_TRUE)
		file->hdr.attr |= FILE_ANONYMOUS;

	if (!parse_file_index_properties(cx, obj, &file->file_idx))
		result = SMB_FAILURE;

	free(cp);
	return result;
}

static JSBool
js_hash_file(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*        obj = JS_THIS_OBJECT(cx, arglist);
	jsval*           argv = JS_ARGV(cx, arglist);
	private_t*       p;
	char             path[MAX_PATH + 1];
	char*            filename = NULL;
	enum file_detail detail = file_detail_normal;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	scfg_t*          scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));
	if (scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class)) == NULL)
		return JS_FALSE;

	file_t file;
	ZERO_VAR(file);

	uintN  argn = 0;
	if (argn < argc && JSVAL_IS_STRING(argv[argn]))  {
		JSVALUE_TO_MSTRING(cx, argv[argn], filename, NULL);
		HANDLE_PENDING(cx, filename);
		argn++;
	}
	if (filename == NULL) {
		JS_ReportError(cx, "No filename argument");
		return JS_FALSE;
	}
	if (getfname(filename) != filename)
		SAFECOPY(path, filename);
	else {
		file.dir = p->smb.dirnum;
		file.name = filename;
		getfilepath(scfg, &file, path);
	}
	JSBool result = JS_FALSE;
	off_t size = flength(path);
	if (size == -1)
		JS_ReportError(cx, "File does not exist: %s", path);
	else {
		smb_setfilesize(&file.idx, size);
		if ((p->smb_result = smb_hashfile(path, size, &file.file_idx.hash.data)) > 0) {
			file.file_idx.hash.flags = p->smb_result;
			file.hdr.when_written.time = (uint32_t)fdate(path);
			JS::RootedObject fobj(cx);
			fobj = JS_NewObject(cx, NULL, NULL, obj);
			if (fobj == nullptr)
				JS_ReportError(cx, "object allocation failure, line %d", __LINE__);
			else {
				set_file_properties(cx, fobj, &file, detail);
				JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(fobj.get()));
				result = JS_TRUE;
			}
		}
	}
	free(filename);
	smb_freefilemem(&file);

	return result;
}

static JSBool
js_get_file(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*        obj = JS_THIS_OBJECT(cx, arglist);
	jsval*           argv = JS_ARGV(cx, arglist);
	private_t*       p;
	char*            filename = NULL;
	enum file_detail detail = file_detail_normal;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	scfg_t*          scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));
	if (scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class)) == NULL)
		return JS_FALSE;

	if (!SMB_IS_OPEN(&(p->smb))) {
		JS_ReportError(cx, "FileBase is not open");
		return JS_FALSE;
	}

	file_t file;
	ZERO_VAR(file);

	uintN  argn = 0;
	if (argn < argc && JSVAL_IS_STRING(argv[argn]))  {
		JSVALUE_TO_MSTRING(cx, argv[argn], filename, NULL);
		HANDLE_PENDING(cx, filename);
		if (filename == NULL)
			return JS_FALSE;
		argn++;
	}
	if (argn < argc && JSVAL_IS_OBJECT(argv[argn]) && !JSVAL_IS_NULL(argv[argn])) {
		free(filename);
		if ((filename = parse_file_name(cx, JSVAL_TO_OBJECT(argv[argn]))) == NULL)
			return JS_FALSE;
		argn++;
	}
	else if (filename == NULL)
		return JS_TRUE;
	if (argn < argc && JSVAL_IS_NUMBER(argv[argn])) {
		detail = static_cast<file_detail>(JSVAL_TO_INT(argv[argn]));
		argn++;
	}
	JSBool result = JS_TRUE;
	if ((p->smb_result = smb_findfile(&p->smb, filename, &file)) == SMB_SUCCESS
	    && (p->smb_result = smb_getfile(&p->smb, &file, detail)) == SMB_SUCCESS) {
		JS::RootedObject fobj(cx);
		fobj = JS_NewObject(cx, NULL, NULL, obj);
		if (fobj == nullptr) {
			JS_ReportError(cx, "object allocation failure, line %d", __LINE__);
			result = JS_FALSE;
		}
		else {
			set_file_properties(cx, fobj, &file, detail);
			JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(fobj.get()));
		}
	}
	free(filename);
	smb_freefilemem(&file);

	return result;
}

static JSBool
js_get_file_list(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*        obj = JS_THIS_OBJECT(cx, arglist);
	jsval*           argv = JS_ARGV(cx, arglist);
	private_t*       p;
	time_t           t = 0;
	char*            filespec = NULL;
	enum file_detail detail = file_detail_normal;
	enum file_sort   sort = FILE_SORT_NAME_A;

	scfg_t*          scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));
	if (scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class)) == NULL)
		return JS_FALSE;

	if (!SMB_IS_OPEN(&(p->smb))) {
		JS_ReportError(cx, "FileBase is not open");
		return JS_FALSE;
	}

	if (dirnum_is_valid(scfg, p->smb.dirnum))
		sort = static_cast<file_sort>(scfg->dir[p->smb.dirnum]->sort);

	uintN argn = 0;
	if (argn < argc && JSVAL_IS_STRING(argv[argn]))  {
		JSVALUE_TO_MSTRING(cx, argv[argn], filespec, NULL);
		HANDLE_PENDING(cx, filespec);
		if (filespec == NULL)
			return JS_FALSE;
		argn++;
	}
	if (argn < argc && JSVAL_IS_NUMBER(argv[argn])) {
		detail = static_cast<file_detail>(JSVAL_TO_INT(argv[argn]));
		argn++;
	}
	if (argn < argc && JSVAL_IS_NUMBER(argv[argn]))  {
		t = JSVAL_TO_INT(argv[argn]);
		argn++;
	}
	if (argn < argc && JSVAL_IS_BOOLEAN(argv[argn])) {
		if (argv[argn++] == JSVAL_FALSE)
			sort = FILE_SORT_NATURAL;
		else if (argn < argc && JSVAL_IS_NUMBER(argv[argn])) {
			sort = static_cast<file_sort>(JSVAL_TO_INT(argv[argn]));
			argn++;
		}
	}

	JS::RootedObject array(cx);
	array = JS_NewArrayObject(cx, 0, NULL);
	if (array == nullptr) {
		free(filespec);
		JS_ReportError(cx, "JS_NewArrayObject failure");
		return JS_FALSE;
	}

	JSBool result = JS_TRUE;
	size_t  file_count;
	file_t* file_list = loadfiles(&p->smb, filespec, t, detail, sort, &file_count);
	if (file_list != NULL) {
		JS::RootedObject fobj(cx);
		for (size_t i = 0; i < file_count; i++) {
			fobj = JS_NewObject(cx, NULL, NULL, array.get());
			if (fobj == nullptr) {
				JS_ReportError(cx, "object allocation failure, line %d", __LINE__);
				result = JS_FALSE;
				break;
			}
			set_file_properties(cx, fobj, &file_list[i], detail);
			JS_DefineElement(cx, array.get(), i, OBJECT_TO_JSVAL(fobj.get()), NULL, NULL, JSPROP_ENUMERATE);
		}
		freefiles(file_list, file_count);
	}
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array.get()));
	free(filespec);

	return result;
}

static JSBool
js_get_file_names(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*      obj = JS_THIS_OBJECT(cx, arglist);
	jsval*         argv = JS_ARGV(cx, arglist);
	private_t*     p;
	time_t         t = 0;
	char*          filespec = NULL;
	enum file_sort sort = FILE_SORT_NAME_A;

	scfg_t*        scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));
	if (scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class)) == NULL)
		return JS_FALSE;

	if (!SMB_IS_OPEN(&(p->smb))) {
		JS_ReportError(cx, "FileBase is not open");
		return JS_FALSE;
	}

	if (dirnum_is_valid(scfg, p->smb.dirnum))
		sort = static_cast<file_sort>(scfg->dir[p->smb.dirnum]->sort);

	uintN argn = 0;
	if (argn < argc && JSVAL_IS_STRING(argv[argn]))  {
		JSVALUE_TO_MSTRING(cx, argv[argn], filespec, NULL);
		HANDLE_PENDING(cx, filespec);
		if (filespec == NULL)
			return JS_FALSE;
		argn++;
	}
	if (argn < argc && JSVAL_IS_NUMBER(argv[argn]))  {
		t = JSVAL_TO_INT(argv[argn]);
		argn++;
	}
	if (argn < argc && JSVAL_IS_BOOLEAN(argv[argn])) {
		if (argv[argn++] == JSVAL_FALSE)
			sort = FILE_SORT_NATURAL;
		else if (argn < argc && JSVAL_IS_NUMBER(argv[argn])) {
			sort = static_cast<file_sort>(JSVAL_TO_INT(argv[argn]));
			argn++;
		}
	}

	JS::RootedObject array(cx);
	array = JS_NewArrayObject(cx, 0, NULL);
	if (array == nullptr) {
		free(filespec);
		JS_ReportError(cx, "JS_NewArrayObject failure");
		return JS_FALSE;
	}

	str_list_t file_list = loadfilenames(&p->smb, filespec, t, sort, NULL);
	if (file_list != NULL) {
		JS::RootedString js_str(cx);
		for (size_t i = 0; file_list[i] != NULL; i++) {
			js_str = JS_NewStringCopyZ(cx, file_list[i]);
			if (js_str == nullptr)
				return JS_FALSE;
			JS_DefineElement(cx, array.get(), i, STRING_TO_JSVAL(js_str.get()), NULL, NULL, JSPROP_ENUMERATE);
		}
		strListFree(&file_list);
	}
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array.get()));
	free(filespec);

	return JS_TRUE;
}

static JSBool
js_get_file_name(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval* argv = JS_ARGV(cx, arglist);
	char   filepath[MAX_PATH + 1] = "";
	char   filename[SMB_FILEIDX_NAMELEN + 1] = "";

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;

	uintN     argn = 0;
	JSVALUE_TO_STRBUF(cx, argv[argn], filepath, sizeof(filepath), NULL);
	JSString* js_str;
	if ((js_str = JS_NewStringCopyZ(cx, smb_fileidxname(getfname(filepath), filename, sizeof(filename)))) != NULL)
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));

	return JS_TRUE;
}

static JSBool
js_format_file_name(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval* argv = JS_ARGV(cx, arglist);
	char   filepath[MAX_PATH + 1] = "";
	int32  size = 12;
	bool   pad = false;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;

	uintN argn = 0;
	JSVALUE_TO_STRBUF(cx, argv[argn], filepath, sizeof(filepath), NULL);
	argn++;
	if (argn < argc && JSVAL_IS_NUMBER(argv[argn])) {
		if (!JS_ValueToInt32(cx, argv[argn], &size))
			return JS_FALSE;
		argn++;
	}
	if (argn < argc && JSVAL_IS_BOOLEAN(argv[argn])) {
		pad = JSVAL_TO_BOOLEAN(argv[argn]);
		argn++;
	}
	if (size < 1) {
		JS_ReportError(cx, "Invalid size: %d", size);
		return JS_FALSE;
	}
	char* buf = static_cast<char *>(calloc(size + 1, 1));
	if (buf == NULL) {
		JS_ReportError(cx, "malloc failure: %d", size + 1);
		return JS_FALSE;
	}
	JSString* js_str;
	if ((js_str = JS_NewStringCopyZ(cx, format_filename(getfname(filepath), buf, size, pad))) != NULL)
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	free(buf);

	return JS_TRUE;
}

static JSBool
js_get_file_path(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	jsval*     argv = JS_ARGV(cx, arglist);
	private_t* p;
	char*      filename = NULL;
	file_t     file;

	ZERO_VAR(file);
	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	scfg_t* scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));
	if (scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class)) == NULL)
		return JS_FALSE;

	uintN argn = 0;
	if (argn < argc && JSVAL_IS_STRING(argv[argn]))  {
		JSVALUE_TO_MSTRING(cx, argv[argn], filename, NULL);
		HANDLE_PENDING(cx, filename);
		argn++;
	}
	if (argn < argc && JSVAL_IS_OBJECT(argv[argn]) && !JSVAL_IS_NULL(argv[argn])) {
		free(filename);
		if ((filename = parse_file_name(cx, JSVAL_TO_OBJECT(argv[argn]))) == NULL)
			return JS_FALSE;
		argn++;
	}
	else if (filename == NULL)
		return JS_TRUE;

	JSBool result = JS_FALSE;
	if ((p->smb_result = smb_loadfile(&p->smb, filename, &file, file_detail_normal)) == SMB_SUCCESS) {
		char      path[MAX_PATH + 1];
		JSString* js_str;
		if ((js_str = JS_NewStringCopyZ(cx, getfilepath(scfg, &file, path))) != NULL)
			JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
		smb_freefilemem(&file);
		result = JS_TRUE;
	}
	else
		JS_ReportError(cx, "%d loading file '%s'", p->smb_result, filename);
	free(filename);

	return result;
}

static JSBool
js_get_file_size(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	jsval*     argv = JS_ARGV(cx, arglist);
	private_t* p;
	char*      filename = NULL;
	file_t     file;

	ZERO_VAR(file);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(-1));

	scfg_t* scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));
	if (scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class)) == NULL)
		return JS_FALSE;

	uintN argn = 0;
	if (argn < argc && JSVAL_IS_STRING(argv[argn]))  {
		JSVALUE_TO_MSTRING(cx, argv[argn], filename, NULL);
		HANDLE_PENDING(cx, filename);
		argn++;
	}
	if (argn < argc && JSVAL_IS_OBJECT(argv[argn]) && !JSVAL_IS_NULL(argv[argn])) {
		free(filename);
		if ((filename = parse_file_name(cx, JSVAL_TO_OBJECT(argv[argn]))) == NULL)
			return JS_FALSE;
		argn++;
	}
	else if (filename == NULL)
		return JS_TRUE;

	if ((p->smb_result = smb_loadfile(&p->smb, filename, &file, file_detail_normal)) == SMB_SUCCESS) {
		JS_SET_RVAL(cx, arglist, DOUBLE_TO_JSVAL((jsdouble)getfilesize(scfg, &file)));
		smb_freefilemem(&file);
	}
	free(filename);

	return JS_TRUE;
}

static JSBool
js_get_file_time(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	jsval*     argv = JS_ARGV(cx, arglist);
	private_t* p;
	char*      filename = NULL;
	file_t     file;

	ZERO_VAR(file);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(-1));

	scfg_t* scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));
	if (scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class)) == NULL)
		return JS_FALSE;

	uintN argn = 0;
	if (argn < argc && JSVAL_IS_STRING(argv[argn]))  {
		JSVALUE_TO_MSTRING(cx, argv[argn], filename, NULL);
		HANDLE_PENDING(cx, filename);
		argn++;
	}
	if (argn < argc && JSVAL_IS_OBJECT(argv[argn]) && !JSVAL_IS_NULL(argv[argn])) {
		free(filename);
		if ((filename = parse_file_name(cx, JSVAL_TO_OBJECT(argv[argn]))) == NULL)
			return JS_FALSE;
		argn++;
	}
	else if (filename == NULL)
		return JS_TRUE;

	if ((p->smb_result = smb_loadfile(&p->smb, filename, &file, file_detail_normal)) == SMB_SUCCESS) {
		JS_SET_RVAL(cx, arglist, UINT_TO_JSVAL((uint32)getfiletime(scfg, &file)));
		smb_freefilemem(&file);
	}
	free(filename);

	return JS_TRUE;
}

static void get_diz(scfg_t* scfg, file_t* file, char** extdesc)
{
	char diz_fpath[MAX_PATH + 1];
	if (extract_diz(scfg, file, /* diz_fnames: */ NULL, diz_fpath, sizeof(diz_fpath))) {
		char                  extbuf[LEN_EXTDESC + 1] = "";
		struct sauce_charinfo sauce;
		char*                 lines = read_diz(diz_fpath, &sauce);
		if (lines != NULL) {
			format_diz(lines, extbuf, sizeof(extbuf), sauce.width, sauce.ice_color);
			free_diz(lines);
			free(*extdesc);
			*extdesc = strdup(extbuf);
			file_sauce_hfields(file, &sauce);
			if (file->desc == NULL)
				smb_new_hfield_str(file, SMB_FILEDESC, prep_file_desc(extbuf, extbuf));
		}
		(void)remove(diz_fpath);
	}
}

static JSBool
js_add_file(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	jsval*     argv = JS_ARGV(cx, arglist);
	private_t* p;
	char*      extdesc = NULL;
	char*      auxdata = NULL;
	file_t     file;
	client_t*  client = NULL;
	bool       use_diz_always = false;
	bool       use_diz_never = false;

	ZERO_VAR(file);
	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	scfg_t* scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));
	if (scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class)) == NULL)
		return JS_FALSE;

	if (!SMB_IS_OPEN(&(p->smb))) {
		JS_ReportError(cx, "FileBase is not open");
		return JS_FALSE;
	}

	uintN argn = 0;
	if (argn < argc && JSVAL_IS_OBJECT(argv[argn]) && !JSVAL_IS_NULL(argv[argn])) {
		p->smb_result = parse_file_properties(cx, JSVAL_TO_OBJECT(argv[argn]), &file, &extdesc, &auxdata);
		if (p->smb_result != SMB_SUCCESS)
			return JS_FALSE;
		argn++;
	}
	if (argn < argc && JSVAL_IS_BOOLEAN(argv[argn])) {
		if (JSVAL_TO_BOOLEAN(argv[argn]))
			use_diz_always = true;
		else
			use_diz_never = true;
		argn++;
	}
	if (argn < argc && JSVAL_IS_OBJECT(argv[argn]) && !JSVAL_IS_NULL(argv[argn])) {
		JSObject* objarg = JSVAL_TO_OBJECT(argv[argn]);
		const JSClass*  cl;
		if (objarg != NULL && (cl = JS_GetClass(cx, objarg)) != NULL && strcmp(cl->name, "Client") == 0) {
			client = static_cast<client_t *>(JS_GetPrivate(cx, objarg));
		}
	}

	file.dir = p->smb.dirnum;
	if (file.name != NULL) {
		if ((extdesc == NULL || use_diz_always == true)
		    && !use_diz_never
		    && dirnum_is_valid(scfg, file.dir)
		    && (scfg->dir[file.dir]->misc & DIR_DIZ)) {
			get_diz(scfg, &file, &extdesc);
		}
		char fpath[MAX_PATH + 1];
		getfilepath(scfg, &file, fpath);
		if (file.from_ip == NULL)
			file_client_hfields(&file, client);
		p->smb_result = smb_addfile(&p->smb, &file, SMB_SELFPACK, extdesc, auxdata, fpath);
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(p->smb_result == SMB_SUCCESS));
	}
	smb_freefilemem(&file);
	free(extdesc);
	free(auxdata);

	return JS_TRUE;
}

static JSBool
js_update_file(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	jsval*     argv = JS_ARGV(cx, arglist);
	private_t* p;
	file_t     file;
	char*      filename = NULL;
	JSObject*  fileobj = NULL;
	bool       use_diz_always = false;
	bool       readd_always = false;

	ZERO_VAR(file);
	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	scfg_t* scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));
	if (scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class)) == NULL)
		return JS_FALSE;

	if (!SMB_IS_OPEN(&(p->smb))) {
		JS_ReportError(cx, "FileBase is not open");
		return JS_FALSE;
	}

	uintN argn = 0;
	if (argn < argc) {
		JSVALUE_TO_MSTRING(cx, argv[argn], filename, NULL);
		HANDLE_PENDING(cx, filename);
		argn++;
	}
	if (argn < argc && JSVAL_IS_OBJECT(argv[argn])) {
		fileobj = JSVAL_TO_OBJECT(argv[argn]);
		argn++;
	}
	if (argn < argc && JSVAL_IS_BOOLEAN(argv[argn])) {
		use_diz_always = JSVAL_TO_BOOLEAN(argv[argn]);
		argn++;
	}
	if (argn < argc && JSVAL_IS_BOOLEAN(argv[argn])) {
		readd_always = JSVAL_TO_BOOLEAN(argv[argn]);
		argn++;
	}

	JSBool result = JS_TRUE;
	char*  extdesc = NULL;
	char*  auxdata = NULL;
	if (filename != NULL && fileobj != NULL
	    && (p->smb_result = smb_loadfile(&p->smb, filename, &file, file_detail_extdesc)) == SMB_SUCCESS) {
		p->smb_result = parse_file_properties(cx, fileobj, &file, &extdesc, &auxdata);
		if (p->smb_result == SMB_SUCCESS
		    && strcmp(filename, file.name) != 0 && smb_findfile(&p->smb, file.name, NULL) == SMB_SUCCESS) {
			JS_ReportError(cx, "file (%s) already exists in base", file.name);
			p->smb_result = SMB_DUPE_MSG;
			result = JS_FALSE;
		}
		if (p->smb_result == SMB_SUCCESS
		    && (extdesc == NULL || use_diz_always == true)
		    && dirnum_is_valid(scfg, file.dir)
		    && (scfg->dir[file.dir]->misc & DIR_DIZ)) {
			get_diz(scfg, &file, &extdesc);
		}
		if (p->smb_result == SMB_SUCCESS) {
			char orgfname[MAX_PATH + 1];
			char newfname[MAX_PATH + 1];
			getfilepath(scfg, &file, newfname);
			SAFECOPY(orgfname, newfname);
			*getfname(orgfname) = '\0';
			SAFECAT(orgfname, filename);
			if (strcmp(orgfname, newfname) != 0 && fexistcase(orgfname) && rename(orgfname, newfname) != 0) {
				JS_ReportError(cx, "%d renaming '%s' to '%s'", errno, orgfname, newfname);
				result = JS_FALSE;
				p->smb_result = SMB_ERR_RENAME;
			} else {
				if (file.extdesc != NULL)
					truncsp(file.extdesc);
				if (!readd_always && strcmp(extdesc ? extdesc : "", file.extdesc ? file.extdesc : "") == 0
				    && strcmp(auxdata ? auxdata : "", file.auxdata ? file.auxdata : "") == 0) {
					p->smb_result = smb_putfile(&p->smb, &file);
					if (p->smb_result != SMB_SUCCESS) {
						JS_ReportError(cx, "%d writing '%s'", p->smb_result, file.name);
						result = JS_FALSE;
					}
				} else {
					if ((p->smb_result = smb_removefile_by_name(&p->smb, filename)) == SMB_SUCCESS) {
						if (readd_always)
							file.hdr.when_imported.time = 0; // we want the file to appear as "new"
						p->smb_result = smb_addfile(&p->smb, &file, SMB_SELFPACK, extdesc, auxdata, newfname);
					}
					else {
						JS_ReportError(cx, "%d removing '%s'", p->smb_result, filename);
						result = JS_FALSE;
					}
				}
			}
		}
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(p->smb_result == SMB_SUCCESS));
	}
	smb_freefilemem(&file);
	free(filename);
	free(extdesc);
	free(auxdata);

	return result;
}

static JSBool
js_renew_file(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	jsval*     argv = JS_ARGV(cx, arglist);
	private_t* p;
	char*      fname = NULL;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	scfg_t*    scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));
	if (scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class)) == NULL)
		return JS_FALSE;

	if (!SMB_IS_OPEN(&(p->smb))) {
		JS_ReportError(cx, "FileBase is not open");
		return JS_FALSE;
	}

	uintN argn = 0;
	if (argn < argc) {
		JSVALUE_TO_MSTRING(cx, argv[argn], fname, NULL);
		HANDLE_PENDING(cx, fname);
		argn++;
	}
	if (fname == NULL)
		return JS_TRUE;

	file_t file;
	if ((p->smb_result = smb_loadfile(&p->smb, fname, &file, file_detail_auxdata)) == SMB_SUCCESS) {
		char path[MAX_PATH + 1];
		p->smb_result = smb_renewfile(&p->smb, &file, SMB_SELFPACK, getfilepath(scfg, &file, path));
		smb_freefilemem(&file);
	}
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(p->smb_result == SMB_SUCCESS));
	free(fname);

	return JS_TRUE;
}

static JSBool
js_remove_file(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	jsval*     argv = JS_ARGV(cx, arglist);
	private_t* p;
	char*      fname = NULL;
	bool       delfile = false;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	scfg_t*    scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));
	if (scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class)) == NULL)
		return JS_FALSE;

	if (!SMB_IS_OPEN(&(p->smb))) {
		JS_ReportError(cx, "FileBase is not open");
		return JS_FALSE;
	}

	uintN argn = 0;
	if (argn < argc) {
		JSVALUE_TO_MSTRING(cx, argv[argn], fname, NULL);
		HANDLE_PENDING(cx, fname);
		argn++;
	}
	if (argn < argc && JSVAL_IS_BOOLEAN(argv[argn])) {
		delfile = JSVAL_TO_BOOLEAN(argv[argn]);
		argn++;
	}
	if (fname == NULL)
		return JS_TRUE;

	JSBool result = JS_TRUE;
	file_t file;
	if ((p->smb_result = smb_loadfile(&p->smb, fname, &file, file_detail_normal)) == SMB_SUCCESS) {
		char path[MAX_PATH + 1];
		if (delfile && remove(getfilepath(scfg, &file, path)) != 0) {
			JS_ReportError(cx, "%d removing '%s'", errno, path);
			p->smb_result = SMB_ERR_DELETE;
			result = JS_FALSE;
		} else
			p->smb_result = smb_removefile(&p->smb, &file);
		smb_freefilemem(&file);
	}
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(p->smb_result == SMB_SUCCESS));
	free(fname);

	return result;
}

/* FileBase Object Properties */
enum {
	FB_PROP_LAST_ERROR
	, FB_PROP_FILE
	, FB_PROP_DEBUG
	, FB_PROP_RETRY_TIME
	, FB_PROP_RETRY_DELAY
	, FB_PROP_FIRST_FILE     /* first file number */
	, FB_PROP_LAST_FILE      /* last file number */
	, FB_PROP_LAST_FILE_TIME /* last file index time */
	, FB_PROP_FILES          /* total files */
	, FB_PROP_UPDATE_TIME
	, FB_PROP_MAX_FILES      /* Maximum number of file to keep in dir */
	, FB_PROP_MAX_AGE        /* Maximum age of file to keep in dir (in days) */
	, FB_PROP_ATTR           /* Attributes for this file base (SMB_HYPER,etc) */
	, FB_PROP_DIRNUM         /* Directory number */
	, FB_PROP_IS_OPEN
	, FB_PROP_STATUS         /* Last SMBLIB returned status value (e.g. retval) */
};

static JSBool js_filebase_set_value(JSContext* cx, private_t* p, jsint tiny, jsval* vp)
{
	int32     val = 0;

	if (JSVAL_IS_NUMBER(*vp) || JSVAL_IS_BOOLEAN(*vp)) {
		if (!JS_ValueToInt32(cx, *vp, &val))
			return JS_FALSE;
	}

	switch (tiny) {
		case FB_PROP_RETRY_TIME:
			p->smb.retry_time = val;
			break;
		case FB_PROP_RETRY_DELAY:
			p->smb.retry_delay = val;
			break;
		case FB_PROP_UPDATE_TIME:
			update_newfiletime(&p->smb, val);
			break;
		default:
			return JS_TRUE;
	}

	return JS_TRUE;
}

/* SM128: populate a single filebase property value by tinyid */
static JSBool js_filebase_get_value(JSContext* cx, private_t* p, jsint tiny, jsval* vp)
{
	char*             s = NULL;
	idxrec_t          idx;
	JS::RootedString  js_str(cx);

	switch (tiny) {
		case FB_PROP_FILE:
			s = p->smb.file;
			break;
		case FB_PROP_LAST_ERROR:
			s = p->smb.last_error;
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
			memset(&idx, 0, sizeof(idx));
			smb_getfirstidx(&(p->smb), &idx);
			*vp = UINT_TO_JSVAL(idx.number);
			break;
		case FB_PROP_LAST_FILE:
			smb_getstatus(&(p->smb));
			*vp = UINT_TO_JSVAL(p->smb.status.last_file);
			break;
		case FB_PROP_LAST_FILE_TIME:
			*vp = UINT_TO_JSVAL((uint32_t)lastfiletime(&p->smb));
			break;
		case FB_PROP_FILES:
			smb_getstatus(&(p->smb));
			*vp = UINT_TO_JSVAL(p->smb.status.total_files);
			break;
		case FB_PROP_UPDATE_TIME:
			*vp = UINT_TO_JSVAL((uint32_t)newfiletime(&p->smb));
			break;
		case FB_PROP_MAX_FILES:
			*vp = UINT_TO_JSVAL(p->smb.status.max_files);
			break;
		case FB_PROP_MAX_AGE:
			*vp = UINT_TO_JSVAL(p->smb.status.max_age);
			break;
		case FB_PROP_ATTR:
			*vp = UINT_TO_JSVAL(p->smb.status.attr);
			break;
		case FB_PROP_DIRNUM:
			*vp = INT_TO_JSVAL(p->smb.dirnum);
			break;
		case FB_PROP_IS_OPEN:
			*vp = BOOLEAN_TO_JSVAL(SMB_IS_OPEN(&(p->smb)));
			break;
		default:
			return JS_TRUE;
	}

	if (s != NULL) {
		js_str = JS_NewStringCopyZ(cx, s);
		if (!js_str)
			return JS_FALSE;
		*vp = STRING_TO_JSVAL(js_str.get());
	}

	return JS_TRUE;
}

#define FB_PROP_FLAGS JSPROP_ENUMERATE | JSPROP_READONLY

static jsSyncPropertySpec js_filebase_properties[] = {
/*		 name				,tinyid					,flags,				ver	*/

	{   "error", FB_PROP_LAST_ERROR, FB_PROP_FLAGS,     31900 },
	{   "last_error", FB_PROP_LAST_ERROR, JSPROP_READONLY,   31900 },               /* alias */
	{   "status", FB_PROP_STATUS, FB_PROP_FLAGS,     31900 },
	{   "file", FB_PROP_FILE, FB_PROP_FLAGS,     31900 },
	{   "debug", FB_PROP_DEBUG, 0,                 31900 },
	{   "retry_time", FB_PROP_RETRY_TIME, JSPROP_ENUMERATE,  31900 },
	{   "retry_delay", FB_PROP_RETRY_DELAY, JSPROP_ENUMERATE,  31900 },
	{   "first_file", FB_PROP_FIRST_FILE, FB_PROP_FLAGS,     31900 },
	{   "last_file", FB_PROP_LAST_FILE, FB_PROP_FLAGS,     31900 },
	{   "last_file_time", FB_PROP_LAST_FILE_TIME, FB_PROP_FLAGS,     31900 },
	{   "files", FB_PROP_FILES, FB_PROP_FLAGS,     31900 },
	{   "update_time", FB_PROP_UPDATE_TIME, JSPROP_ENUMERATE,  31900 },
	{   "max_files", FB_PROP_MAX_FILES, FB_PROP_FLAGS,     31900 },
	{   "max_age", FB_PROP_MAX_AGE, FB_PROP_FLAGS,     31900 },
	{   "attributes", FB_PROP_ATTR, FB_PROP_FLAGS,     31900 },
	{   "dirnum", FB_PROP_DIRNUM, FB_PROP_FLAGS,     31900 },
	{   "is_open", FB_PROP_IS_OPEN, FB_PROP_FLAGS,     31900 },
	{0}
};

#ifdef BUILD_JSDOCS
static const char*      filebase_prop_desc[] = {

	"Last occurred file base error description - <small>READ ONLY</small>"
	, "Return value of last <i>SMB Library</i> function call - <small>READ ONLY</small>"
	, "Base path and filename of file base - <small>READ ONLY</small>"
	, "File base open/lock retry timeout (in seconds)"
	, "Delay between file base open/lock retries (in milliseconds)"
	, "First file number - <small>READ ONLY</small>"
	, "Last file number - <small>READ ONLY</small>"
	, "Time-stamp of last file - <small>READ ONLY</small>"
	, "Total number of files - <small>READ ONLY</small>"
	, "Time-stamp of file base index (only writable when file base is closed)"
	, "Maximum number of files before expiration - <small>READ ONLY</small>"
	, "Maximum age (in days) of files to store - <small>READ ONLY</small>"
	, "File base attributes - <small>READ ONLY</small>"
	, "Directory number (0-based, -1 if invalid) - <small>READ ONLY</small>"
	, "<i>true</i> if the file base has been opened successfully - <small>READ ONLY</small>"
	, NULL
};
#endif

static bool js_filebase_prop_setter(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject thisObj(cx);
	if (!args.computeThis(cx, &thisObj))
		return false;
	private_t* p = (private_t*)js_GetClassPrivate(cx, thisObj, &js_filebase_class);
	if (p == nullptr)
		return true;
	JSObject* callee = &args.callee();
	jsint tiny = js::GetFunctionNativeReserved(callee, 0).toInt32();
	jsval val = args.length() > 0 ? args[0] : JSVAL_VOID;
	if (!js_filebase_set_value(cx, p, tiny, &val))
		return false;
	args.rval().set(val);
	return true;
}

static bool js_filebase_prop_getter(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject thisObj(cx);
	if (!args.computeThis(cx, &thisObj))
		return false;
	private_t* p = (private_t*)js_GetClassPrivate(cx, thisObj, &js_filebase_class);
	if (p == nullptr) {
		args.rval().setUndefined();
		return true;
	}
	JSObject* callee = &args.callee();
	jsint tiny = js::GetFunctionNativeReserved(callee, 0).toInt32();
	jsval val = JSVAL_VOID;
	if (!js_filebase_get_value(cx, p, tiny, &val))
		return false;
	args.rval().set(val);
	return true;
}

/* SM128: fill all (or one named) filebase properties after js_SyncResolve */
static JSBool js_filebase_fill_properties(JSContext* cx, JSObject* obj, private_t* p, const char* name)
{
	(void)p;
	return js_DefineSyncAccessors(cx, obj, js_filebase_properties, js_filebase_prop_getter, name, js_filebase_prop_setter);
}

static jsSyncMethodSpec js_filebase_functions[] = {
	{"open",            js_open,            0, JSTYPE_BOOLEAN
	 , JSDOCSTR("")
	 , JSDOCSTR("Open file base")
	 , 31900},
	{"lock",            js_lock,           0, JSTYPE_BOOLEAN
	 , JSDOCSTR("")
	 , JSDOCSTR("Lock an open file base for maintenance (prevents subsequent/concurrent opens if locked successfully, i.e. returns <tt>true</tt>)")
	 , 32105},
	{"unlock",          js_unlock,         0, JSTYPE_BOOLEAN
	 , JSDOCSTR("")
	 , JSDOCSTR("Unlock a locked file base")
	 , 32105},
	{"close",           js_close,           0, JSTYPE_BOOLEAN
	 , JSDOCSTR("")
	 , JSDOCSTR("Close file base (if open), unlocks the base if it was previously locked")
	 , 31900},
	{"get",             js_get_file,        2, JSTYPE_OBJECT
	 , JSDOCSTR("<i>string</i> filename or <i>object</i> file-meta-object [,<i>number</i> detail=FileBase.DETAIL.NORM]")
	 , JSDOCSTR("Get a file metadata object or <tt>null</tt> on failure. "
		        "The file-meta-object may contain the following properties (depending on <i>detail</i> value):<br>"
		        "<table>"
		        "<tr><td align=top><tt>name</tt><td>Filename <i>(required)</i>"
		        "<tr><td align=top><tt>vpath</tt><td>Virtual path to file <i>READ ONLY</i>"
		        "<tr><td align=top><tt>desc</tt><td>Description (summary, 58 chars or less)"
		        "<tr><td align=top><tt>extdesc</tt><td>Extended description (multi-line description, e.g. DIZ)"
		        "<tr><td align=top><tt>author</tt><td>File author name (e.g. from SAUCE record)"
		        "<tr><td align=top><tt>author_org</tt><td>File author organization (group, e.g. from SAUCE record)"
		        "<tr><td align=top><tt>from</tt><td>Uploader's user name (e.g. for awarding credits)"
		        "<tr><td align=top><tt>from_ip_addr</tt><td>Uploader's IP address (if available, for security tracking)"
		        "<tr><td align=top><tt>from_host_name</tt><td>Uploader's host name (if available, for security tracking)"
		        "<tr><td align=top><tt>from_protocol</tt><td>TCP/IP protocol used by uploader (if available, for security tracking)"
		        "<tr><td align=top><tt>from_port</tt><td>TCP/UDP port number used by uploader (if available, for security tracking)"
		        "<tr><td align=top><tt>to_list</tt><td>Comma-separated list of recipient user numbers (for user-to-user transfers)"
		        "<tr><td align=top><tt>tags</tt><td>Space-separated list of tags"
		        "<tr><td align=top><tt>anon</tt><td><tt>true</tt> if the file was uploaded anonymously"
		        "<tr><td align=top><tt>size</tt><td>File size, in bytes, at the time of upload"
		        "<tr><td align=top><tt>cost</tt><td>File credit value (0=free)"
		        "<tr><td align=top><tt>time</tt><td>File modification date/time (in time_t format)"
		        "<tr><td align=top><tt>added</tt><td>Date/time file was uploaded/imported (in time_t format)"
		        "<tr><td align=top><tt>last_downloaded</tt><td>Date/time file was last downloaded (in time_t format) or 0=never"
		        "<tr><td align=top><tt>times_downloaded</tt><td>Total number of times file has been downloaded"
		        "<tr><td align=top><tt>crc16</tt><td>16-bit CRC of file contents"
		        "<tr><td align=top><tt>crc32</tt><td>32-bit CRC of file contents"
		        "<tr><td align=top><tt>md5</tt><td>128-bit MD5 digest of file contents (hexadecimal)"
		        "<tr><td align=top><tt>sha1</tt><td>160-bit SHA-1 digest of file contents (hexadecimal)"
		        "<tr><td align=top><tt>auxdata</tt><td>File auxiliary information (JSON)"
		        "</table>"
		        )
	 , 31900},
	{"get_list",        js_get_file_list,   4, JSTYPE_ARRAY
	 , JSDOCSTR("[<i>string</i> filespec] [,<i>number</i> detail=FileBase.DETAIL.NORM] [,<i>number</i> since-time=0] [,<i>bool</i> sort=true [,<i>number</i> order]]")
	 , JSDOCSTR("Get a list (array) of file metadata objects"
		        ", the default sort order is the sysop-configured order or <tt>FileBase.SORT.NAME_AI</tt>"
		        )
	 , 31900},
	{"get_name",        js_get_file_name,   1, JSTYPE_STRING
	 , JSDOCSTR("path/filename")
	 , JSDOCSTR("Return index-formatted (e.g. shortened) version of filename without path (file base does not have to be open)")
	 , 31900},
	{"get_names",       js_get_file_names,  3, JSTYPE_ARRAY
	 , JSDOCSTR("[<i>string</i> filespec] [,<i>number</i> since-time=0] [,<i>bool</i> sort=true [,<i>number</i> order]]")
	 , JSDOCSTR("Get a list of index-formatted (e.g. shortened) filenames (strings) from file base index"
		        ", the default sort order is the sysop-configured order or <tt>FileBase.SORT.NAME_AI</tt>")
	 , 31900},
	{"get_path",        js_get_file_path,   1, JSTYPE_STRING
	 , JSDOCSTR("<i>string</i> filename or <i>object</i> file-meta-object")
	 , JSDOCSTR("Get the full path to the local file")
	 , 31900},
	{"get_size",        js_get_file_size,   1, JSTYPE_NUMBER
	 , JSDOCSTR("<i>string</i> filename or <i>object</i> file-meta-object")
	 , JSDOCSTR("Get the size of the local file, in bytes, or -1 if it does not exist")
	 , 31900},
	{"get_time",        js_get_file_time,   1, JSTYPE_NUMBER
	 , JSDOCSTR("<i>string</i> filename or <i>object</i> file-meta-object")
	 , JSDOCSTR("Get the modification date/time stamp of the local file")
	 , 31900},
	{"add",             js_add_file,        1, JSTYPE_BOOLEAN
	 , JSDOCSTR("<i>object</i> file-meta-object [,<i>bool</i> use_diz=true-if-no-extdesc] [,<i>object</i> client=none]")
	 , JSDOCSTR("Add a file to the file base, returning <tt>true</tt> on success or <tt>false</tt> upon failure.  Pass <tt>use_diz</tt> parameter as <tt>true</tt> or <tt>false</tt> to force or prevent the extraction/import of description file (e.g. FILE_ID.DIZ) within archive (e.g. ZIP) file.")
	 , 31900},
	{"remove",          js_remove_file,     2, JSTYPE_BOOLEAN
	 , JSDOCSTR("filename [,<i>bool</i> delete=false]")
	 , JSDOCSTR("Remove an existing file from the file base and optionally delete file"
		        ", may throw exception on errors (e.g. file remove failure)")
	 , 31900},
	{"update",          js_update_file,     3, JSTYPE_BOOLEAN
	 , JSDOCSTR("filename, <i>object</i> file-meta-object [,<i>bool</i> use_diz_always=false] [,<i>bool</i> readd_always=false]")
	 , JSDOCSTR("Update metadata and/or rename an existing file in the file base"
		        ", may throw exception on errors (e.g. file rename failure)")
	 , 31900},
	{"renew",           js_renew_file,      1, JSTYPE_BOOLEAN
	 , JSDOCSTR("filename")
	 , JSDOCSTR("Remove and re-add (as new) an existing file in the file base")
	 , 31900},
	{"hash",            js_hash_file,       1, JSTYPE_OBJECT
	 , JSDOCSTR("<i>string</i> filename_or_fullpath")
	 , JSDOCSTR("Calculate hashes of a file's contents (file base does not have to be open)")
	 , 31900},
	{"dump",            js_dump_file,       1, JSTYPE_ARRAY
	 , JSDOCSTR("filename")
	 , JSDOCSTR("Dump file header fields to an array of strings for diagnostic uses")
	 , 31900},
	{"format_name",     js_format_file_name, 1, JSTYPE_STRING
	 , JSDOCSTR("path/filename [,<i>number</i> size=12] [,<i>bool</i> pad=false]")
	 , JSDOCSTR("Return formatted (e.g. shortened) version of filename without path (file base does not have to be open) for display (also available as a class method)")
	 , 31900},
	{0}
};

static bool js_filebase_resolve(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, bool* resolvedp)
{
	char* name = NULL;

	if (id.get().isString()) {
		JSString* str = id.get().toString();
		JSSTRING_TO_MSTRING(cx, str, name, NULL);
		HANDLE_PENDING(cx, name);
		if (name == NULL) return false;
	}

	bool ret = js_SyncResolve(cx, obj, name, js_filebase_properties, js_filebase_functions, NULL, 0);
	if (ret) {
		private_t* p = (private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class);
		if (p != NULL)
			ret = js_filebase_fill_properties(cx, obj, p, name);
	}
	if (name)
		free(name);
	if (resolvedp) *resolvedp = ret;
	if (JS_IsExceptionPending(cx))
		JS_ClearPendingException(cx);
	return true;
}

static bool js_filebase_enumerate(JSContext *cx, JS::Handle<JSObject*> obj)
{
	private_t* p = (private_t*)js_GetClassPrivate(cx, obj, &js_filebase_class);
	if (!js_SyncResolve(cx, obj, NULL, js_filebase_properties, js_filebase_functions, NULL, 0))
		return false;
	if (p != NULL)
		return js_filebase_fill_properties(cx, obj, p, NULL);
	return true;
}

static const JSClassOps js_filebase_classops = {
	nullptr,                /* addProperty  */
	nullptr,                /* delProperty  */
	js_filebase_enumerate,  /* enumerate    */
	nullptr,                /* newEnumerate */
	js_filebase_resolve,    /* resolve      */
	nullptr,                /* mayResolve   */
	js_finalize_filebase,   /* finalize     */
	nullptr, nullptr, nullptr /* call, construct, trace */
};

JSClass js_filebase_class = {
	"FileBase"
	, JSCLASS_HAS_PRIVATE | JSCLASS_FOREGROUND_FINALIZE
	, &js_filebase_classops
};

/* FileBase Constructor (open file base) */
static JSBool
js_filebase_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj;
	jsval *    argv = JS_ARGV(cx, arglist);
	char       base[MAX_PATH + 1] = "";
	bool       is_path = false;
	private_t* p;
	scfg_t*    scfg;

	scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));

	obj = JS_NewObject(cx, &js_filebase_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
	if ((p = (private_t*)malloc(sizeof(private_t))) == NULL) {
		JS_ReportError(cx, "malloc failed");
		return JS_FALSE;
	}

	memset(p, 0, sizeof(private_t));
	p->smb.retry_time = scfg->smb_retry_time;

	if(argc >= 1) {
		uintN argn = 0;
		JSVALUE_TO_STRBUF(cx, argv[argn], base, sizeof base, NULL);
		++argn;
		if (JS_IsExceptionPending(cx)) {
			free(p);
			return JS_FALSE;
		}
		if (*base == '\0') {
			JS_ReportError(cx, "Invalid 'path_or_code' parameter");
			free(p);
			return JS_FALSE;
		}
		if (argn < argc && JSVAL_IS_BOOLEAN(argv[argn])) {
			is_path = JSVAL_TO_BOOLEAN(argv[argn]);
			++argn;
		}
	}

	if (!JS_SetPrivate(cx, obj, p)) {
		JS_ReportError(cx, "JS_SetPrivate failed");
		free(p);
		return JS_FALSE;
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj, "Class used for accessing file databases", 31900);
	js_DescribeSyncConstructor(cx, obj, "To create a new FileBase object: "
	                           "<tt>var filebase = new FileBase(<i>string</i> code_or_path, [<i>bool</i> is_path=false])</tt><br>"
	                           "where <i>code_or_path</i> is a file directory internal code, or ...<br>"
	                           "when <i>is_path</i> is <tt>true</tt>, the string parameter specifies the path to a file base (not an internal code) - new behavior in v3.21."
	                           );
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", filebase_prop_desc, JSPROP_READONLY);
#endif

	if(argc >= 1) {
		if (is_path) {
			p->smb.dirnum = INVALID_DIR;
			SAFECOPY(p->smb.file, base);
		} else if (dirnum_is_valid(scfg, p->smb.dirnum = getdirnum(scfg, base))) {
			safe_snprintf(p->smb.file, sizeof(p->smb.file), "%s%s"
						  , scfg->dir[p->smb.dirnum]->data_dir, scfg->dir[p->smb.dirnum]->code);
		} else { /* unknown code */
			JS_ReportError(cx, "Unrecognized file area (directory) internal code: '%s'", base);
		}
	}

	return JS_TRUE;
}

#ifdef BUILD_JSDOCS
static const char* filebase_detail_prop_desc[] = {
	"Include indexed-filenames only",
	"Normal level of file detail (e.g. full filenames, minimal metadata)",
	"Normal level of file detail plus extended descriptions",
	"Normal level of file detail plus extended descriptions and auxiliary data (JSON format)",
	"Maximum file detail, include undefined/null property values",
	NULL
};

static const char* filebase_sort_prop_desc[] = {
	"Natural/index order (no sorting)",
	"Filename ascending, case insensitive sort order",
	"Filename descending, case insensitive sort order",
	"Filename ascending, case sensitive sort order",
	"Filename descending, case sensitive sort order",
	"Import date/time ascending sort order",
	"Import date/time descending sort order",
	"File size in bytes, ascending sort order",
	"File size in bytes, descending sort order",
	NULL
};
#endif

JSObject* js_CreateFileBaseClass(JSContext* cx, JSObject* parent)
{
	JSObject* obj;
	JS::RootedObject constructor(cx);
	jsval     val;

	obj = JS_InitClass(cx, parent, NULL
	                   , &js_filebase_class
	                   , js_filebase_constructor
	                   , 1 /* number of constructor args */
	                   , NULL
	                   , NULL
	                   , NULL, NULL);

	if (JS_GetProperty(cx, parent, js_filebase_class.name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JS_ValueToObject(cx, val, constructor.address());
		JS_DefineFunction(cx, constructor.get(), "format_name", js_format_file_name, 1, 0);
		JS::RootedObject detail(cx, JS_DefineObject(cx, constructor, "DETAIL", NULL, NULL, JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY));
		if (detail) {
			JS_DefineProperty(cx, detail, "MIN", INT_TO_JSVAL(file_detail_index), NULL, NULL
			                  , JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY);
			JS_DefineProperty(cx, detail, "NORM", INT_TO_JSVAL(file_detail_normal), NULL, NULL
			                  , JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY);
			JS_DefineProperty(cx, detail, "EXTENDED", INT_TO_JSVAL(file_detail_extdesc), NULL, NULL
			                  , JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY);
			JS_DefineProperty(cx, detail, "AUXDATA", INT_TO_JSVAL(file_detail_auxdata), NULL, NULL
			                  , JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY);
			JS_DefineProperty(cx, detail, "MAX", INT_TO_JSVAL(file_detail_auxdata + 1), NULL, NULL
			                  , JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY);
#ifdef BUILD_JSDOCS
			js_DescribeSyncObject(cx, detail, "Detail level numeric constants (in increasing verbosity)", 0);
			js_CreateArrayOfStrings(cx, detail, "_property_desc_list", filebase_detail_prop_desc, JSPROP_READONLY);
#endif
		}
		JS::RootedObject sort(cx, JS_DefineObject(cx, constructor, "SORT", NULL, NULL, JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY));
		if (sort) {
			JS_DefineProperty(cx, sort, "NATURAL", INT_TO_JSVAL(FILE_SORT_NATURAL), NULL, NULL
			                  , JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY);
			JS_DefineProperty(cx, sort, "NAME_AI", INT_TO_JSVAL(FILE_SORT_NAME_A), NULL, NULL
			                  , JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY);
			JS_DefineProperty(cx, sort, "NAME_DI", INT_TO_JSVAL(FILE_SORT_NAME_D), NULL, NULL
			                  , JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY);
			JS_DefineProperty(cx, sort, "NAME_AS", INT_TO_JSVAL(FILE_SORT_NAME_AC), NULL, NULL
			                  , JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY);
			JS_DefineProperty(cx, sort, "NAME_DS", INT_TO_JSVAL(FILE_SORT_NAME_DC), NULL, NULL
			                  , JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY);
			JS_DefineProperty(cx, sort, "DATE_A", INT_TO_JSVAL(FILE_SORT_DATE_A), NULL, NULL
			                  , JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY);
			JS_DefineProperty(cx, sort, "DATE_D", INT_TO_JSVAL(FILE_SORT_DATE_D), NULL, NULL
			                  , JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY);
			JS_DefineProperty(cx, sort, "SIZE_A", INT_TO_JSVAL(FILE_SORT_SIZE_A), NULL, NULL
			                  , JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY);
			JS_DefineProperty(cx, sort, "SIZE_D", INT_TO_JSVAL(FILE_SORT_SIZE_D), NULL, NULL
			                  , JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY);
#ifdef BUILD_JSDOCS
			js_DescribeSyncObject(cx, sort, "Sort order numeric constants", 0);
			js_CreateArrayOfStrings(cx, sort, "_property_desc_list", filebase_sort_prop_desc, JSPROP_READONLY);
#endif
		}
	}
	return obj;
}
