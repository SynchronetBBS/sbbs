/* Synchronet JavaScript "SQLite" Class */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
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
#ifdef USE_SQLITE

#include "js_request.h"
#include <sqlite3.h>
/* jsvalue.h (pulled in transitively here) uses JS_STATIC_ASSERT macros
 * that trip -Wunused-local-typedef under jsdocs build flags. The asserts
 * are SpiderMonkey-internal; silence the warning around this include. */
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#include <jstypedarray.h>
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif
#include <math.h>

/*
 * SQLite JS bindings.
 *
 * Four classes:
 *   SQLite           - wraps sqlite3*
 *   SQLiteStatement  - wraps sqlite3_stmt*
 *   SQLiteRow        - immutable view over the current row of a statement;
 *                      supports both row[colidx] and row[colname]
 *   SQLiteValue      - live-ref to a single column of the current row
 *
 * Injection resistance is a primary design goal:
 *   - SQLite.exec() takes no parameters by design; it is for DDL only
 *   - SQLite.query()/run() refuse to run when SQL has placeholders but no
 *     params array is supplied
 *   - SQLiteStatement.bind() type-dispatches via sqlite3_bind_*; bound values
 *     never re-enter the SQL parser
 *   - There are no escape/quote helpers exposed
 *
 * Lifetime:
 *   - SQLiteStatement and SQLiteRow root their parent (connection / statement)
 *     by holding it as a hidden property.
 *   - SQLiteRow is a live view of the statement's current row; it stores a
 *     generation counter snapshotted at creation and stale reads throw after
 *     step/reset/finalize.
 *   - SQLiteValue is an eager snapshot. At row[col] time we fetch using the
 *     native column type and cache the resulting JS primitive (Number, String,
 *     ArrayBuffer, or null) as a hidden property; the SQLiteValue holds no
 *     reference to the statement afterward and outlives step/reset/close.
 *     The typed accessors (.integer, .real, .text, .utf8, .blob, .bytes) are
 *     JS-level coercions of that cached primitive.
 *
 * Text encoding:
 *   Two parallel APIs:
 *     bindText / .text      - Synchronet narrow bytes. JS string converted
 *                             via the standard narrow macros (high byte of
 *                             each jschar dropped on bind, bytes zero-extended
 *                             on read). Round-trips inside Synchronet but the
 *                             bytes in the SQLite file are not valid UTF-8 for
 *                             non-ASCII. This is the default for .value and
 *                             snapshot rows, matching the rest of Synchronet.
 *     bindUtf8 / .utf8      - Proper Unicode. Uses sqlite3_bind_text16 and
 *                             sqlite3_column_text16, with jschar mapping
 *                             directly to UTF-16 host byte order. Stored as
 *                             portable UTF-8 in the SQLite file.
 *   toString() returns the .text form, so implicit string coercion matches
 *   Synchronet convention. Pick one consistently per column; mixing the two
 *   within one column gives mismatched expectations.
 */

#define ROOT_PROP_NAME "__sqlite_parent"

extern "C" {

extern JSClass js_sqlite_class;
extern JSClass js_sqlite_stmt_class;
extern JSClass js_sqlite_row_class;
extern JSClass js_sqlite_value_class;
extern JSClass js_sqlite_table_class;
extern JSClass js_sqlite_table_ns_class;
extern JSClass js_sqlite_record_class;

}

/* Forward decls for the table cache types so sqlite_conn_private_t can
 * reference them. */
struct sqlite_table_cache_node;
typedef struct sqlite_table_cache_node sqlite_table_cache_node_t;

typedef struct {
	sqlite3* db;
	char*    path;       /* malloc'd */
	int      last_rc;
	int      busy_timeout_ms;
	/* db.table namespace object + cached SQLiteTable lookup chain. Schema is
	 * loaded once per table at first access and cached for the connection's
	 * lifetime (see plan: scripts that need to see post-DDL schema changes
	 * must reopen the connection). The table_obj is rooted as a property of
	 * the SQLite connection JSObject; cached tables are rooted as properties
	 * of table_obj with JSPROP_PERMANENT so the linked-list pointers below
	 * can't dangle. */
	JSObject*                  table_obj;
	sqlite_table_cache_node_t* table_cache;
} sqlite_conn_private_t;

typedef struct {
	sqlite3*       db;     /* borrowed; kept alive by sqlite3_close_v2 deferred-close */
	sqlite3_stmt*  stmt;
	char*          sql;    /* malloc'd copy of original SQL */
	uint32_t       generation;
	bool           row_pending;
} sqlite_stmt_private_t;

typedef struct {
	sqlite_stmt_private_t* stmt;  /* borrowed; rooted via __sqlite_parent */
	uint32_t               generation;
} sqlite_row_private_t;

/* Eager snapshot: SQLiteValue caches its data at row[col] time and has no
 * live reference to the statement afterward, so it outlives step()/reset()/
 * close(). The cached primitive lives as the __value property (and __utf8
 * for TEXT cells); the GC traces those automatically. */
typedef struct {
	int  type;   /* SQLite storage class snapshotted at fetch time */
	int  bytes;  /* sqlite3_column_bytes for TEXT/BLOB, 0 otherwise */
} sqlite_value_private_t;

/* ---------- SQLiteTable / SQLiteRecord types ---------- */

/* One column descriptor as held by SQLiteTable. Strings are malloc'd.
 * `dflt_value` is the SQL text of the DEFAULT expression (or NULL); we
 * evaluate it via SQLite at new_row() time rather than parsing it here. */
typedef struct {
	char* name;
	char* type;        /* declared type, may be "" */
	bool  notnull;
	char* dflt_value;  /* SQL text or NULL */
	int   pk;          /* 0 if not PK, otherwise 1-based position in compound PK */
} sqlite_table_col_t;

/* One foreign-key descriptor. ncol > 1 indicates a composite FK; @col
 * accessors refuse to resolve composite FKs (per plan). Strings are
 * malloc'd; `from_col`/`to_col` arrays are length ncol. */
typedef struct {
	int    ncol;
	char*  to_table;
	char** from_col;
	char** to_col;
} sqlite_table_fk_t;

/* Private state for a SQLiteTable JSObject. The connection back-pointer
 * is borrowed (conn lifetime exceeds table lifetime; see lifecycle notes
 * on sqlite_conn_private_t). */
typedef struct {
	sqlite_conn_private_t* conn;
	char* name;                  /* canonical case from sqlite_master */
	sqlite_table_col_t* cols;
	int   ncols;
	int*  pk_cols;               /* indices into cols[] in pk order, length npk */
	int   npk;
	sqlite_table_fk_t* fks;
	int   nfks;
} sqlite_table_private_t;

/* Linked-list node for the connection's case-insensitive table lookup. */
struct sqlite_table_cache_node {
	char*                            name_lower;
	JSObject*                        table_obj;
	struct sqlite_table_cache_node*  next;
};

/* Private state for a SQLiteRecord JSObject. Column data lives in JS
 * arrays (so the GC traces any string/object slots automatically), not
 * as own properties keyed by column name (avoids name collisions with
 * methods; matches the "column data isn't JS object data" stance). A
 * slot of JSVAL_VOID means "not defined" — JSVAL_NULL is a real value
 * distinct from undefined. `inserted` is true once insert() succeeds
 * OR the record was constructed from row()/select() with PK values
 * captured. */
typedef struct {
	JSObject*               table_obj;    /* SQLiteTable, rooted via __sqlite_parent */
	sqlite_table_private_t* table;        /* borrowed from table_obj's private */
	JSObject*               data_arr;     /* JSArray length ncols; rooted via __data */
	JSObject*               orig_pk_arr;  /* JSArray length npk; rooted via __orig_pk */
	bool                    inserted;
} sqlite_record_private_t;

/* ---------- error helpers ---------- */

static JSBool sqlite_throw(JSContext* cx, sqlite3* db, int rc, const char* prefix)
{
	const char* msg = db ? sqlite3_errmsg(db) : sqlite3_errstr(rc);
	JS_ReportError(cx, "%s: %s (rc=%d)", prefix ? prefix : "SQLite", msg, rc);
	return JS_FALSE;
}

/* ---------- forward declarations ---------- */

static JSObject* new_stmt_object(JSContext* cx, JSObject* conn_obj,
                                 sqlite3* db, sqlite3_stmt* stmt, const char* sql);
static JSObject* new_row_object(JSContext* cx, JSObject* stmt_obj,
                                sqlite_stmt_private_t* sp);
static JSObject* new_value_object(JSContext* cx, sqlite3_stmt* stmt, int col);
static JSBool    bind_jsval(JSContext* cx, sqlite3_stmt* stmt, int idx, jsval v);
static int       resolve_param_index(sqlite3_stmt* stmt, const char* name);

static JSObject* new_table_ns_object(JSContext* cx, JSObject* conn_obj);
static JSObject* new_table_object(JSContext* cx, JSObject* conn_obj,
                                  sqlite_conn_private_t* cp, const char* name);
static JSObject* new_record_object(JSContext* cx, JSObject* table_obj,
                                   sqlite_table_private_t* tp);
static JSBool    table_load_schema(JSContext* cx, sqlite_conn_private_t* cp,
                                   const char* req_name, sqlite_table_private_t* out);
static void      table_free_schema(sqlite_table_private_t* tp);
static char*     str_to_lower_dup(const char* s);
static JSBool    table_load_by_pk(JSContext* cx, JSObject* table_obj,
                                  sqlite_table_private_t* tp,
                                  int npk, jsval* pk_args, jsval* out);

/* ---------- SQLiteValue ---------- */

static void js_finalize_value(JSContext* cx, JSObject* obj)
{
	sqlite_value_private_t* p = (sqlite_value_private_t*)JS_GetPrivate(cx, obj);
	if (p == NULL)
		return;
	free(p);
	JS_SetPrivate(cx, obj, NULL);
}

static const char* sqlite_type_name(int t)
{
	switch (t) {
		case SQLITE_INTEGER: return "integer";
		case SQLITE_FLOAT:   return "real";
		case SQLITE_TEXT:    return "text";
		case SQLITE_BLOB:    return "blob";
		case SQLITE_NULL:    return "null";
	}
	return "unknown";
}

enum {
	VALUE_PROP_TYPE,
	VALUE_PROP_TYPE_CODE,
	VALUE_PROP_INTEGER,
	VALUE_PROP_REAL,
	VALUE_PROP_TEXT,
	VALUE_PROP_UTF8,
	VALUE_PROP_BLOB,
	VALUE_PROP_BYTES,
	VALUE_PROP_VALUE,
};

#define VALUE_PROP_FLAGS (JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)

static jsSyncPropertySpec js_sqlite_value_properties[] = {
	{"type",       VALUE_PROP_TYPE,       VALUE_PROP_FLAGS, 32200},
	{"type_code",  VALUE_PROP_TYPE_CODE,  VALUE_PROP_FLAGS, 32200},
	{"integer",    VALUE_PROP_INTEGER,    VALUE_PROP_FLAGS, 32200},
	{"real",       VALUE_PROP_REAL,       VALUE_PROP_FLAGS, 32200},
	{"text",       VALUE_PROP_TEXT,       VALUE_PROP_FLAGS, 32200},
	{"utf8",       VALUE_PROP_UTF8,       VALUE_PROP_FLAGS, 32200},
	{"blob",       VALUE_PROP_BLOB,       VALUE_PROP_FLAGS, 32200},
	{"bytes",      VALUE_PROP_BYTES,      VALUE_PROP_FLAGS, 32200},
	{"value",      VALUE_PROP_VALUE,      VALUE_PROP_FLAGS, 32200},
	{0}
};

#ifdef BUILD_JSDOCS
static const char* sqlite_value_prop_desc[] = {
	  "Storage class name: \"integer\", \"real\", \"text\", \"blob\", or \"null\""
	, "Storage class as SQLite type code (SQLite.INTEGER, .FLOAT, .TEXT, .BLOB, .NULL_TYPE)"
	, "Value coerced to a JS Number (lossy for int64 values above 2^53)"
	, "Value coerced to a JS Number (double-precision float)"
	, "Value as a Synchronet narrow string (high byte of each jschar dropped on store)"
	, "Value as a proper Unicode string via UTF-16 round-trip (TEXT cells only; falls back to .toString for non-TEXT)"
	, "Value as an ArrayBuffer (BLOB cells)"
	, "Byte length of TEXT or BLOB cells, 0 otherwise"
	, "Value as its natural JS primitive: Number for INTEGER/REAL, String for TEXT, ArrayBuffer for BLOB, null for NULL"
	, NULL
};
#endif

/* Hidden property names where the eager-fetched data lives. The GC traces
 * these so the cached String/ArrayBuffer/Number stays alive as long as the
 * SQLiteValue does. */
#define VALUE_VALUE_PROP "__value"  /* natural primitive (or ArrayBuffer for BLOB) */
#define VALUE_UTF8_PROP  "__utf8"   /* UTF-16 form for TEXT cells; absent otherwise */

static JSBool js_sqlite_value_get(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	sqlite_value_private_t* p = (sqlite_value_private_t*)JS_GetPrivate(cx, obj);
	if (p == NULL) return JS_TRUE;

	jsval idval;
	if (!JS_IdToValue(cx, id, &idval) || !JSVAL_IS_INT(idval))
		return JS_TRUE;

	switch (JSVAL_TO_INT(idval)) {
		case VALUE_PROP_TYPE: {
			JSString* s = JS_NewStringCopyZ(cx, sqlite_type_name(p->type));
			if (s == NULL) return JS_FALSE;
			*vp = STRING_TO_JSVAL(s);
			break;
		}
		case VALUE_PROP_TYPE_CODE:
			*vp = INT_TO_JSVAL(p->type);
			break;
		case VALUE_PROP_VALUE:
			return JS_GetProperty(cx, obj, VALUE_VALUE_PROP, vp);
		case VALUE_PROP_INTEGER:
		case VALUE_PROP_REAL: {
			jsval v;
			double d;
			if (!JS_GetProperty(cx, obj, VALUE_VALUE_PROP, &v)) return JS_FALSE;
			if (!JS_ValueToNumber(cx, v, &d)) return JS_FALSE;
			return JS_NewNumberValue(cx, d, vp);
		}
		case VALUE_PROP_TEXT: {
			jsval v;
			if (!JS_GetProperty(cx, obj, VALUE_VALUE_PROP, &v)) return JS_FALSE;
			if (JSVAL_IS_NULL(v)) { *vp = JSVAL_NULL; break; }
			JSString* s = JS_ValueToString(cx, v);
			if (s == NULL) return JS_FALSE;
			*vp = STRING_TO_JSVAL(s);
			break;
		}
		case VALUE_PROP_UTF8:
			if (p->type == SQLITE_TEXT)
				return JS_GetProperty(cx, obj, VALUE_UTF8_PROP, vp);
			/* Non-TEXT: fall back to toString of the cached value. */
			{
			jsval v;
			if (!JS_GetProperty(cx, obj, VALUE_VALUE_PROP, &v)) return JS_FALSE;
			if (JSVAL_IS_NULL(v)) { *vp = JSVAL_NULL; break; }
			JSString* s = JS_ValueToString(cx, v);
			if (s == NULL) return JS_FALSE;
			*vp = STRING_TO_JSVAL(s);
			}
			break;
		case VALUE_PROP_BLOB:
			return JS_GetProperty(cx, obj, VALUE_VALUE_PROP, vp);
		case VALUE_PROP_BYTES:
			*vp = INT_TO_JSVAL(p->bytes);
			break;
	}
	return JS_TRUE;
}

static JSBool js_value_toString(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	if (js_GetClassPrivate(cx, obj, &js_sqlite_value_class) == NULL) return JS_FALSE;
	jsval v;
	if (!JS_GetProperty(cx, obj, VALUE_VALUE_PROP, &v)) return JS_FALSE;
	JSString* s = JS_ValueToString(cx, v);
	if (s == NULL) return JS_FALSE;
	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(s));
	return JS_TRUE;
}

static JSBool js_value_valueOf(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	sqlite_value_private_t* p = (sqlite_value_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_value_class);
	if (p == NULL) return JS_FALSE;
	jsval v;
	if (!JS_GetProperty(cx, obj, VALUE_VALUE_PROP, &v)) return JS_FALSE;
	/* No useful primitive for BLOB; returning the object falls the JS
	 * ToPrimitive algorithm through to toString(). */
	if (p->type == SQLITE_BLOB)
		v = OBJECT_TO_JSVAL(obj);
	JS_SET_RVAL(cx, arglist, v);
	return JS_TRUE;
}

static jsSyncMethodSpec js_sqlite_value_functions[] = {
	{"toString", js_value_toString, 0, JSTYPE_STRING,
	 JSDOCSTR(""),
	 JSDOCSTR("Return the column value as a Synchronet narrow string (same as .text). "
	          "Called implicitly when the Value is coerced to string."),
	 32200},
	{"valueOf",  js_value_valueOf,  0, JSTYPE_UNDEF,
	 JSDOCSTR(""),
	 JSDOCSTR("Return the column value as its natural primitive: Number for INTEGER/REAL, "
	          "String for TEXT, null for NULL, the value object itself for BLOB. "
	          "Called implicitly by ToPrimitive (arithmetic, equality, etc.)."),
	 32200},
	{0}
};

static JSBool js_sqlite_value_resolve(JSContext* cx, JSObject* obj, jsid id)
{
	char*  name = NULL;
	JSBool ret;
	if (id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		JS_IdToValue(cx, id, &idval);
		if (JSVAL_IS_STRING(idval))
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
	}
	ret = js_SyncResolve(cx, obj, name, js_sqlite_value_properties,
	    js_sqlite_value_functions, NULL, 0);
	if (name) free(name);
	return ret;
}

static JSBool js_sqlite_value_enumerate(JSContext* cx, JSObject* obj)
{
	return js_sqlite_value_resolve(cx, obj, JSID_VOID);
}

JSClass js_sqlite_value_class = {
	"SQLiteValue"
	, JSCLASS_HAS_PRIVATE
	, JS_PropertyStub
	, JS_PropertyStub
	, js_sqlite_value_get
	, JS_StrictPropertyStub
	, js_sqlite_value_enumerate
	, js_sqlite_value_resolve
	, JS_ConvertStub
	, js_finalize_value
};

static JSObject* new_value_object(JSContext* cx, sqlite3_stmt* stmt, int col)
{
	int   type = sqlite3_column_type(stmt, col);
	int   bytes = 0;
	jsval natural = JSVAL_VOID;
	jsval utf8 = JSVAL_VOID;

	switch (type) {
		case SQLITE_INTEGER: {
			sqlite3_int64 i = sqlite3_column_int64(stmt, col);
			/* TODO: lossy round-to-nearest-even above 2^53. Switch to BigInt
			 * once the JS engine has it. */
			if (!JS_NewNumberValue(cx, (double)i, &natural)) return NULL;
			break;
		}
		case SQLITE_FLOAT:
			if (!JS_NewNumberValue(cx, sqlite3_column_double(stmt, col), &natural))
				return NULL;
			break;
		case SQLITE_TEXT: {
			const char* s = (const char*)sqlite3_column_text(stmt, col);
			int n = sqlite3_column_bytes(stmt, col);
			JSString* nstr = JS_NewStringCopyN(cx, s ? s : "", n);
			if (nstr == NULL) return NULL;
			natural = STRING_TO_JSVAL(nstr);
			bytes = n;
			/* Also fetch the UTF-16 form for .utf8. SQLite may invalidate the
			 * pointer from column_text once column_text16 runs an in-place
			 * conversion, which is fine because we already copied. */
			const jschar* w = (const jschar*)sqlite3_column_text16(stmt, col);
			int wbytes = sqlite3_column_bytes16(stmt, col);
			size_t nchars = wbytes / sizeof(jschar);
			JSString* wstr = JS_NewUCStringCopyN(cx,
			    w ? w : (const jschar*)"\0\0", w ? nchars : 0);
			if (wstr == NULL) return NULL;
			utf8 = STRING_TO_JSVAL(wstr);
			break;
		}
		case SQLITE_BLOB: {
			int n = sqlite3_column_bytes(stmt, col);
			const void* data = sqlite3_column_blob(stmt, col);
			JSObject* ab = js_CreateArrayBuffer(cx, n);
			if (ab == NULL) {
				JS_ReportError(cx, "ArrayBuffer allocation failed (%d bytes)", n);
				return NULL;
			}
			if (n > 0) {
				js::ArrayBuffer* buf = js::ArrayBuffer::fromJSObject(ab);
				if (buf && buf->data && data) memcpy(buf->data, data, n);
			}
			natural = OBJECT_TO_JSVAL(ab);
			bytes = n;
			break;
		}
		case SQLITE_NULL:
		default:
			natural = JSVAL_NULL;
			break;
	}

	JSObject* obj = JS_NewObject(cx, &js_sqlite_value_class, NULL, NULL);
	if (obj == NULL) return NULL;
	sqlite_value_private_t* p = (sqlite_value_private_t*)calloc(1, sizeof(*p));
	if (p == NULL) { JS_ReportError(cx, "calloc failed"); return NULL; }
	p->type = type;
	p->bytes = bytes;
	if (!JS_SetPrivate(cx, obj, p)) {
		free(p);
		JS_ReportError(cx, "JS_SetPrivate failed");
		return NULL;
	}
	JS_DefineProperty(cx, obj, VALUE_VALUE_PROP, natural, NULL, NULL,
	    JSPROP_PERMANENT | JSPROP_READONLY);
	if (type == SQLITE_TEXT) {
		JS_DefineProperty(cx, obj, VALUE_UTF8_PROP, utf8, NULL, NULL,
		    JSPROP_PERMANENT | JSPROP_READONLY);
	}
#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj,
	    "A single column value from a result row. Eagerly snapshotted at "
	    "row[col] time; outlives step()/reset()/close().", 32200);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list",
	    sqlite_value_prop_desc, JSPROP_READONLY);
#endif
	return obj;
}

/* ---------- SQLiteRow ---------- */

static void js_finalize_row(JSContext* cx, JSObject* obj)
{
	sqlite_row_private_t* p = (sqlite_row_private_t*)JS_GetPrivate(cx, obj);
	if (p == NULL)
		return;
	free(p);
	JS_SetPrivate(cx, obj, NULL);
}

static JSBool row_check_live(JSContext* cx, sqlite_row_private_t* p)
{
	if (p == NULL || p->stmt == NULL) {
		JS_ReportError(cx, "SQLiteRow: detached");
		return JS_FALSE;
	}
	if (p->generation != p->stmt->generation) {
		JS_ReportError(cx, "SQLiteRow: row no longer valid (statement advanced)");
		return JS_FALSE;
	}
	return JS_TRUE;
}

/* Static Row properties are $-prefixed so they can't collide with real
 * column names. Tinyids are negative so they can't collide with valid
 * column indices (which are always >=0); js_DefineSyncProperties stores
 * the shortid as int8, so values outside -128..127 silently bypass the
 * class get hook and read as undefined. */
enum {
	ROW_PROP_LENGTH       = -1,
	ROW_PROP_COLUMN_NAMES = -2,
	ROW_PROP_VALID        = -3,
};

#define ROW_PROP_FLAGS (JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)

static jsSyncPropertySpec js_sqlite_row_properties[] = {
	{"$length",       ROW_PROP_LENGTH,       ROW_PROP_FLAGS, 32200},
	{"$column_names", ROW_PROP_COLUMN_NAMES, ROW_PROP_FLAGS, 32200},
	{"$valid",        ROW_PROP_VALID,        ROW_PROP_FLAGS, 32200},
	{0}
};

#ifdef BUILD_JSDOCS
static const char* sqlite_row_prop_desc[] = {
	  "Number of columns in the row"
	, "Array of column names"
	, "false once the underlying statement has been step()ed past this row, reset, or closed"
	, NULL
};
#endif

static JSBool js_sqlite_row_get(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	sqlite_row_private_t* p = (sqlite_row_private_t*)JS_GetPrivate(cx, obj);
	if (p == NULL) return JS_TRUE;

	jsval idval;
	if (!JS_IdToValue(cx, id, &idval))
		return JS_TRUE;

	if (JSVAL_IS_INT(idval)) {
		int n = JSVAL_TO_INT(idval);
		/* Static-property tinyids are negative; column indices are >=0. */
		switch (n) {
			case ROW_PROP_LENGTH:
				if (!row_check_live(cx, p)) return JS_FALSE;
				*vp = INT_TO_JSVAL(sqlite3_column_count(p->stmt->stmt));
				return JS_TRUE;
			case ROW_PROP_VALID:
				*vp = BOOLEAN_TO_JSVAL(p->stmt != NULL && p->generation == p->stmt->generation);
				return JS_TRUE;
			case ROW_PROP_COLUMN_NAMES: {
				if (!row_check_live(cx, p)) return JS_FALSE;
				int ncols = sqlite3_column_count(p->stmt->stmt);
				JSObject* arr = JS_NewArrayObject(cx, ncols, NULL);
				if (arr == NULL) return JS_FALSE;
				for (int i = 0; i < ncols; i++) {
					const char* cn = sqlite3_column_name(p->stmt->stmt, i);
					JSString* s = JS_NewStringCopyZ(cx, cn ? cn : "");
					if (s == NULL) return JS_FALSE;
					jsval v = STRING_TO_JSVAL(s);
					if (!JS_SetElement(cx, arr, i, &v)) return JS_FALSE;
				}
				*vp = OBJECT_TO_JSVAL(arr);
				return JS_TRUE;
			}
		}
		/* Otherwise it's a column index. */
		if (!row_check_live(cx, p)) return JS_FALSE;
		int ncols = sqlite3_column_count(p->stmt->stmt);
		if (n < 0 || n >= ncols) { *vp = JSVAL_VOID; return JS_TRUE; }
		JSObject* v = new_value_object(cx, p->stmt->stmt, n);
		if (v == NULL) return JS_FALSE;
		*vp = OBJECT_TO_JSVAL(v);
		return JS_TRUE;
	}

	if (JSVAL_IS_STRING(idval)) {
		char* name = NULL;
		JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
		if (name == NULL) return JS_TRUE;
		/* Hidden parent and $-prefixed static properties are handled by the
		 * resolved spec; reject so the spec slot wins. */
		if (strcmp(name, ROOT_PROP_NAME) == 0
		    || strcmp(name, "$length") == 0
		    || strcmp(name, "$column_names") == 0
		    || strcmp(name, "$valid") == 0) {
			free(name);
			return JS_TRUE;
		}
		/* Otherwise: try as column name, but leave *vp alone if no match
		 * so any slot-resident property value (e.g. _description set via
		 * js_DescribeSyncObject) survives. */
		if (!row_check_live(cx, p)) { free(name); return JS_FALSE; }
		int ncols = sqlite3_column_count(p->stmt->stmt);
		int col = -1;
		for (int i = 0; i < ncols; i++) {
			const char* cn = sqlite3_column_name(p->stmt->stmt, i);
			if (cn && strcmp(cn, name) == 0) { col = i; break; }
		}
		free(name);
		if (col < 0) return JS_TRUE;  /* leave *vp untouched */
		JSObject* v = new_value_object(cx, p->stmt->stmt, col);
		if (v == NULL) return JS_FALSE;
		*vp = OBJECT_TO_JSVAL(v);
		return JS_TRUE;
	}
	return JS_TRUE;
}

static JSBool js_sqlite_row_set(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	JS_ReportError(cx, "SQLiteRow is immutable");
	return JS_FALSE;
}

static JSBool js_sqlite_row_resolve(JSContext* cx, JSObject* obj, jsid id)
{
	char*  name = NULL;
	JSBool ret;
	if (id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		JS_IdToValue(cx, id, &idval);
		if (JSVAL_IS_STRING(idval))
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
	}
	ret = js_SyncResolve(cx, obj, name, js_sqlite_row_properties, NULL, NULL, 0);
	if (name) free(name);
	return ret;
}

static JSBool js_sqlite_row_enumerate(JSContext* cx, JSObject* obj)
{
	return js_sqlite_row_resolve(cx, obj, JSID_VOID);
}

JSClass js_sqlite_row_class = {
	"SQLiteRow"
	, JSCLASS_HAS_PRIVATE
	, JS_PropertyStub
	, JS_PropertyStub
	, js_sqlite_row_get
	, js_sqlite_row_set
	, js_sqlite_row_enumerate
	, js_sqlite_row_resolve
	, JS_ConvertStub
	, js_finalize_row
};

static JSObject* new_row_object(JSContext* cx, JSObject* stmt_obj,
                                sqlite_stmt_private_t* sp)
{
	JSObject* obj = JS_NewObject(cx, &js_sqlite_row_class, NULL, NULL);
	if (obj == NULL) return NULL;
	sqlite_row_private_t* p = (sqlite_row_private_t*)calloc(1, sizeof(*p));
	if (p == NULL) { JS_ReportError(cx, "calloc failed"); return NULL; }
	p->stmt = sp;
	p->generation = sp->generation;
	if (!JS_SetPrivate(cx, obj, p)) {
		free(p);
		JS_ReportError(cx, "JS_SetPrivate failed");
		return NULL;
	}
	JS_DefineProperty(cx, obj, ROOT_PROP_NAME, OBJECT_TO_JSVAL(stmt_obj),
	    NULL, NULL, JSPROP_PERMANENT | JSPROP_READONLY);
#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj,
	    "One row of a result set. Access columns by zero-based index "
	    "(<tt>row[0]</tt>) or by column name (<tt>row[\"name\"]</tt> or "
	    "<tt>row.name</tt>); both return a SQLiteValue. Each access creates "
	    "a fresh SQLiteValue instance, so <tt>row[0] !== row[0]</tt>; assign "
	    "to a local if you intend to reference the same Value object more "
	    "than once. Static metadata properties use a $ prefix to avoid "
	    "colliding with real column names. The Row is a live view of the "
	    "statement's current row and is invalidated by the next "
	    "step()/reset()/close(); SQLiteValue objects obtained from it are "
	    "eager snapshots that survive.", 32200);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list",
	    sqlite_row_prop_desc, JSPROP_READONLY);
#endif
	return obj;
}

/* ---------- bind helpers ---------- */

static int resolve_param_index(sqlite3_stmt* stmt, const char* name)
{
	int idx = sqlite3_bind_parameter_index(stmt, name);
	if (idx > 0) return idx;
	/* Try common prefixes if user gave bare name. */
	if (name && name[0] != ':' && name[0] != '@' && name[0] != '$') {
		size_t len = strlen(name);
		char* buf = (char*)malloc(len + 2);
		if (buf == NULL) return 0;
		memcpy(buf + 1, name, len + 1);
		const char* prefixes = ":@$";
		for (const char* px = prefixes; *px; px++) {
			buf[0] = *px;
			idx = sqlite3_bind_parameter_index(stmt, buf);
			if (idx > 0) { free(buf); return idx; }
		}
		free(buf);
	}
	return 0;
}

static JSBool bind_jsval(JSContext* cx, sqlite3_stmt* stmt, int idx, jsval v)
{
	int rc = SQLITE_OK;
	if (JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v)) {
		rc = sqlite3_bind_null(stmt, idx);
	} else if (JSVAL_IS_BOOLEAN(v)) {
		rc = sqlite3_bind_int(stmt, idx, JSVAL_TO_BOOLEAN(v) ? 1 : 0);
	} else if (JSVAL_IS_INT(v)) {
		rc = sqlite3_bind_int64(stmt, idx, (sqlite3_int64)JSVAL_TO_INT(v));
	} else if (JSVAL_IS_DOUBLE(v) || JSVAL_IS_NUMBER(v)) {
		double d;
		if (!JS_ValueToNumber(cx, v, &d)) return JS_FALSE;
		/* TODO: JS Number can't represent int64 past 2^53; switch to BigInt
		 * once the JS engine has it. */
		if (d == floor(d) && d >= -9007199254740992.0 && d <= 9007199254740992.0)
			rc = sqlite3_bind_int64(stmt, idx, (sqlite3_int64)d);
		else
			rc = sqlite3_bind_double(stmt, idx, d);
	} else if (JSVAL_IS_STRING(v)) {
		char* s = NULL;
		size_t len = 0;
		JSVALUE_TO_MSTRING(cx, v, s, &len);
		if (s == NULL) return JS_FALSE;
		rc = sqlite3_bind_text(stmt, idx, s, (int)len, SQLITE_TRANSIENT);
		free(s);
	} else if (JSVAL_IS_OBJECT(v) && !JSVAL_IS_NULL(v)) {
		JSObject* o = JSVAL_TO_OBJECT(v);
		if (js_IsArrayBuffer(o)) {
			js::ArrayBuffer* ab = js::ArrayBuffer::fromJSObject(o);
			if (ab == NULL) {
				JS_ReportError(cx, "ArrayBuffer is empty");
				return JS_FALSE;
			}
			rc = sqlite3_bind_blob(stmt, idx, ab->data, (int)ab->byteLength, SQLITE_TRANSIENT);
		} else if (js_IsTypedArray(o)) {
			js::TypedArray* ta = js::TypedArray::fromJSObject(o);
			if (ta == NULL) {
				JS_ReportError(cx, "TypedArray is empty");
				return JS_FALSE;
			}
			rc = sqlite3_bind_blob(stmt, idx, ta->data, (int)ta->byteLength, SQLITE_TRANSIENT);
		} else {
			JS_ReportError(cx, "Cannot bind object value (only ArrayBuffer / TypedArray accepted)");
			return JS_FALSE;
		}
	} else {
		JS_ReportError(cx, "Cannot bind value of this type");
		return JS_FALSE;
	}
	if (rc != SQLITE_OK) {
		JS_ReportError(cx, "sqlite3_bind: rc=%d", rc);
		return JS_FALSE;
	}
	return JS_TRUE;
}

/* ---------- SQLiteStatement ---------- */

static void js_finalize_stmt(JSContext* cx, JSObject* obj)
{
	sqlite_stmt_private_t* p = (sqlite_stmt_private_t*)JS_GetPrivate(cx, obj);
	if (p == NULL)
		return;
	if (p->stmt) {
		sqlite3_finalize(p->stmt);
		p->stmt = NULL;
	}
	if (p->sql) {
		free(p->sql);
		p->sql = NULL;
	}
	free(p);
	JS_SetPrivate(cx, obj, NULL);
}

static JSBool js_stmt_bind(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	jsval*    argv = JS_ARGV(cx, arglist);
	sqlite_stmt_private_t* p = (sqlite_stmt_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_stmt_class);
	if (p == NULL) return JS_FALSE;
	if (p->stmt == NULL) {
		JS_ReportError(cx, "Statement is finalized");
		return JS_FALSE;
	}
	if (argc < 1) {
		JS_ReportError(cx, "bind() requires an array or object argument");
		return JS_FALSE;
	}
	sqlite3_clear_bindings(p->stmt);

	if (JSVAL_IS_OBJECT(argv[0]) && !JSVAL_IS_NULL(argv[0])) {
		JSObject* o = JSVAL_TO_OBJECT(argv[0]);
		JSBool is_array = JS_IsArrayObject(cx, o);
		if (is_array) {
			jsuint length = 0;
			if (!JS_GetArrayLength(cx, o, &length)) return JS_FALSE;
			for (jsuint i = 0; i < length; i++) {
				jsval v;
				if (!JS_GetElement(cx, o, (jsint)i, &v)) return JS_FALSE;
				if (!bind_jsval(cx, p->stmt, (int)i + 1, v)) return JS_FALSE;
			}
		} else {
			JSObject* iter = JS_NewPropertyIterator(cx, o);
			if (iter == NULL) return JS_FALSE;
			jsid pid;
			while (JS_NextProperty(cx, iter, &pid) && pid != JSID_VOID) {
				jsval idv;
				if (!JS_IdToValue(cx, pid, &idv)) return JS_FALSE;
				if (!JSVAL_IS_STRING(idv)) continue;
				char* key = NULL;
				JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idv), key, NULL);
				if (key == NULL) return JS_FALSE;
				int idx = resolve_param_index(p->stmt, key);
				if (idx <= 0) {
					/* Object keys that don't match a placeholder are
					 * silently skipped — supports the common case where
					 * the same object (e.g. a record.$toObject() snapshot
					 * or a config blob) gets bound into queries that
					 * reference only a subset of its keys. */
					free(key);
					continue;
				}
				jsval v;
				if (!JS_GetProperty(cx, o, key, &v)) { free(key); return JS_FALSE; }
				free(key);
				if (!bind_jsval(cx, p->stmt, idx, v)) return JS_FALSE;
			}
		}
		JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));  /* chainable */
		return JS_TRUE;
	}
	JS_ReportError(cx, "bind() argument must be an array or object");
	return JS_FALSE;
}

static JSBool js_stmt_bind_typed(JSContext* cx, uintN argc, jsval* arglist, int sqlite_type)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	jsval*    argv = JS_ARGV(cx, arglist);
	sqlite_stmt_private_t* p = (sqlite_stmt_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_stmt_class);
	if (p == NULL) return JS_FALSE;
	if (p->stmt == NULL) { JS_ReportError(cx, "Statement is finalized"); return JS_FALSE; }
	if (argc < 1) { JS_ReportError(cx, "missing parameter index/name"); return JS_FALSE; }

	int idx = 0;
	if (JSVAL_IS_INT(argv[0])) {
		idx = JSVAL_TO_INT(argv[0]);
	} else if (JSVAL_IS_STRING(argv[0])) {
		char* name = NULL;
		JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[0]), name, NULL);
		if (name == NULL) return JS_FALSE;
		idx = resolve_param_index(p->stmt, name);
		if (idx <= 0) { JS_ReportError(cx, "No such parameter: %s", name); free(name); return JS_FALSE; }
		free(name);
	} else {
		JS_ReportError(cx, "Parameter index must be integer or string");
		return JS_FALSE;
	}

	int rc = SQLITE_OK;
	if (sqlite_type == SQLITE_NULL) {
		rc = sqlite3_bind_null(p->stmt, idx);
	} else {
		if (argc < 2) { JS_ReportError(cx, "missing value"); return JS_FALSE; }
		jsval v = argv[1];
		switch (sqlite_type) {
			case SQLITE_INTEGER: {
				double d;
				if (!JS_ValueToNumber(cx, v, &d)) return JS_FALSE;
				rc = sqlite3_bind_int64(p->stmt, idx, (sqlite3_int64)d);
				break;
			}
			case SQLITE_FLOAT: {
				double d;
				if (!JS_ValueToNumber(cx, v, &d)) return JS_FALSE;
				rc = sqlite3_bind_double(p->stmt, idx, d);
				break;
			}
			case SQLITE_TEXT: {
				char* s = NULL;
				size_t len = 0;
				JSVALUE_TO_MSTRING(cx, v, s, &len);
				if (s == NULL) return JS_FALSE;
				rc = sqlite3_bind_text(p->stmt, idx, s, (int)len, SQLITE_TRANSIENT);
				free(s);
				break;
			}
			case SQLITE_BLOB: {
				if (!JSVAL_IS_OBJECT(v) || JSVAL_IS_NULL(v)) {
					JS_ReportError(cx, "bindBlob requires an ArrayBuffer");
					return JS_FALSE;
				}
				JSObject* o = JSVAL_TO_OBJECT(v);
				const void* data = NULL;
				int n = 0;
				if (js_IsArrayBuffer(o)) {
					js::ArrayBuffer* ab = js::ArrayBuffer::fromJSObject(o);
					data = ab ? ab->data : NULL;
					n = ab ? (int)ab->byteLength : 0;
				} else if (js_IsTypedArray(o)) {
					js::TypedArray* ta = js::TypedArray::fromJSObject(o);
					data = ta ? ta->data : NULL;
					n = ta ? (int)ta->byteLength : 0;
				} else {
					JS_ReportError(cx, "bindBlob requires an ArrayBuffer or TypedArray");
					return JS_FALSE;
				}
				rc = sqlite3_bind_blob(p->stmt, idx, data, n, SQLITE_TRANSIENT);
				break;
			}
		}
	}
	if (rc != SQLITE_OK)
		return sqlite_throw(cx, p->db, rc, "bind");
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
	return JS_TRUE;
}

static JSBool js_stmt_bindInt(JSContext* cx, uintN argc, jsval* arglist)   { return js_stmt_bind_typed(cx, argc, arglist, SQLITE_INTEGER); }
static JSBool js_stmt_bindReal(JSContext* cx, uintN argc, jsval* arglist)  { return js_stmt_bind_typed(cx, argc, arglist, SQLITE_FLOAT); }
static JSBool js_stmt_bindText(JSContext* cx, uintN argc, jsval* arglist)  { return js_stmt_bind_typed(cx, argc, arglist, SQLITE_TEXT); }
static JSBool js_stmt_bindBlob(JSContext* cx, uintN argc, jsval* arglist)  { return js_stmt_bind_typed(cx, argc, arglist, SQLITE_BLOB); }
static JSBool js_stmt_bindNull(JSContext* cx, uintN argc, jsval* arglist)  { return js_stmt_bind_typed(cx, argc, arglist, SQLITE_NULL); }

static JSBool js_stmt_bindUtf8(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	jsval*    argv = JS_ARGV(cx, arglist);
	sqlite_stmt_private_t* p = (sqlite_stmt_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_stmt_class);
	if (p == NULL) return JS_FALSE;
	if (p->stmt == NULL) { JS_ReportError(cx, "Statement is finalized"); return JS_FALSE; }
	if (argc < 2) { JS_ReportError(cx, "bindUtf8 requires (index, value)"); return JS_FALSE; }

	int idx = 0;
	if (JSVAL_IS_INT(argv[0])) {
		idx = JSVAL_TO_INT(argv[0]);
	} else if (JSVAL_IS_STRING(argv[0])) {
		char* name = NULL;
		JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[0]), name, NULL);
		if (name == NULL) return JS_FALSE;
		idx = resolve_param_index(p->stmt, name);
		if (idx <= 0) { JS_ReportError(cx, "No such parameter: %s", name); free(name); return JS_FALSE; }
		free(name);
	} else {
		JS_ReportError(cx, "Parameter index must be integer or string");
		return JS_FALSE;
	}

	JSString* str = JS_ValueToString(cx, argv[1]);
	if (str == NULL) return JS_FALSE;
	size_t nchars = 0;
	const jschar* chars = JS_GetStringCharsAndLength(cx, str, &nchars);
	if (chars == NULL) return JS_FALSE;
	int rc = sqlite3_bind_text16(p->stmt, idx, chars,
	    (int)(nchars * sizeof(jschar)), SQLITE_TRANSIENT);
	if (rc != SQLITE_OK)
		return sqlite_throw(cx, p->db, rc, "bindUtf8");
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
	return JS_TRUE;
}

static JSBool js_stmt_step(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	sqlite_stmt_private_t* p = (sqlite_stmt_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_stmt_class);
	if (p == NULL) return JS_FALSE;
	if (p->stmt == NULL) { JS_ReportError(cx, "Statement is finalized"); return JS_FALSE; }

	p->generation++;
	jsrefcount rc_susp = JS_SUSPENDREQUEST(cx);
	int rc = sqlite3_step(p->stmt);
	JS_RESUMEREQUEST(cx, rc_susp);

	if (rc == SQLITE_ROW) {
		p->row_pending = true;
		JSObject* row = new_row_object(cx, obj, p);
		if (row == NULL) return JS_FALSE;
		JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(row));
		return JS_TRUE;
	}
	p->row_pending = false;
	if (rc == SQLITE_DONE) {
		JS_SET_RVAL(cx, arglist, JSVAL_NULL);
		return JS_TRUE;
	}
	return sqlite_throw(cx, p->db, rc, "step");
}

static JSBool js_stmt_reset(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	sqlite_stmt_private_t* p = (sqlite_stmt_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_stmt_class);
	if (p == NULL) return JS_FALSE;
	if (p->stmt == NULL) { JS_ReportError(cx, "Statement is finalized"); return JS_FALSE; }
	p->generation++;
	p->row_pending = false;
	int rc = sqlite3_reset(p->stmt);
	if (rc != SQLITE_OK)
		return sqlite_throw(cx, p->db, rc, "reset");
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
	return JS_TRUE;
}

static JSBool js_stmt_clearBindings(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	sqlite_stmt_private_t* p = (sqlite_stmt_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_stmt_class);
	if (p == NULL) return JS_FALSE;
	if (p->stmt == NULL) { JS_ReportError(cx, "Statement is finalized"); return JS_FALSE; }
	int rc = sqlite3_clear_bindings(p->stmt);
	if (rc != SQLITE_OK)
		return sqlite_throw(cx, p->db, rc, "clearBindings");
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
	return JS_TRUE;
}

static JSBool js_stmt_close(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	sqlite_stmt_private_t* p = (sqlite_stmt_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_stmt_class);
	if (p == NULL) return JS_FALSE;
	if (p->stmt) {
		p->generation++;
		p->row_pending = false;
		sqlite3_finalize(p->stmt);
		p->stmt = NULL;
	}
	return JS_TRUE;
}

/* Build a plain object snapshot of the current row: { colname: value, ... } */
static JSBool snapshot_row(JSContext* cx, sqlite_stmt_private_t* p, jsval* out)
{
	JSObject* o = JS_NewObject(cx, NULL, NULL, NULL);
	if (o == NULL) return JS_FALSE;
	int ncols = sqlite3_column_count(p->stmt);
	for (int i = 0; i < ncols; i++) {
		const char* cn = sqlite3_column_name(p->stmt, i);
		int t = sqlite3_column_type(p->stmt, i);
		jsval v = JSVAL_NULL;
		switch (t) {
			case SQLITE_INTEGER: {
				sqlite3_int64 iv = sqlite3_column_int64(p->stmt, i);
				if (!JS_NewNumberValue(cx, (double)iv, &v)) return JS_FALSE;
				break;
			}
			case SQLITE_FLOAT:
				if (!JS_NewNumberValue(cx, sqlite3_column_double(p->stmt, i), &v))
					return JS_FALSE;
				break;
			case SQLITE_TEXT: {
				const char* s = (const char*)sqlite3_column_text(p->stmt, i);
				int n = sqlite3_column_bytes(p->stmt, i);
				JSString* js = JS_NewStringCopyN(cx, s ? s : "", n);
				if (js == NULL) return JS_FALSE;
				v = STRING_TO_JSVAL(js);
				break;
			}
			case SQLITE_BLOB: {
				int n = sqlite3_column_bytes(p->stmt, i);
				const void* d = sqlite3_column_blob(p->stmt, i);
				JSObject* ab = js_CreateArrayBuffer(cx, n);
				if (ab == NULL) return JS_FALSE;
				if (n > 0) {
					js::ArrayBuffer* buf = js::ArrayBuffer::fromJSObject(ab);
					if (buf && buf->data && d) memcpy(buf->data, d, n);
				}
				v = OBJECT_TO_JSVAL(ab);
				break;
			}
			case SQLITE_NULL:
			default:
				v = JSVAL_NULL;
				break;
		}
		if (!JS_DefineProperty(cx, o, cn ? cn : "", v, NULL, NULL, JSPROP_ENUMERATE))
			return JS_FALSE;
	}
	*out = OBJECT_TO_JSVAL(o);
	return JS_TRUE;
}

static JSBool js_stmt_all(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	sqlite_stmt_private_t* p = (sqlite_stmt_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_stmt_class);
	if (p == NULL) return JS_FALSE;
	if (p->stmt == NULL) { JS_ReportError(cx, "Statement is finalized"); return JS_FALSE; }

	JSObject* arr = JS_NewArrayObject(cx, 0, NULL);
	if (arr == NULL) return JS_FALSE;
	jsuint n = 0;
	for (;;) {
		jsrefcount rc_susp = JS_SUSPENDREQUEST(cx);
		int rc = sqlite3_step(p->stmt);
		JS_RESUMEREQUEST(cx, rc_susp);
		if (rc == SQLITE_DONE) break;
		if (rc != SQLITE_ROW) return sqlite_throw(cx, p->db, rc, "step");
		p->row_pending = true;
		jsval rowv;
		if (!snapshot_row(cx, p, &rowv)) return JS_FALSE;
		if (!JS_SetElement(cx, arr, (jsint)n, &rowv)) return JS_FALSE;
		n++;
	}
	p->generation++;
	p->row_pending = false;
	sqlite3_reset(p->stmt);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(arr));
	return JS_TRUE;
}

static JSBool js_stmt_forEach(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	jsval*    argv = JS_ARGV(cx, arglist);
	sqlite_stmt_private_t* p = (sqlite_stmt_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_stmt_class);
	if (p == NULL) return JS_FALSE;
	if (p->stmt == NULL) { JS_ReportError(cx, "Statement is finalized"); return JS_FALSE; }
	/* SM 1.8.5 quirk: JSVAL_IS_OBJECT(JSVAL_NULL) is true and
	 * JSVAL_TO_OBJECT(JSVAL_NULL) is NULL, which JS_ObjectIsFunction would
	 * dereference. Reject null explicitly. */
	if (argc < 1 || !JSVAL_IS_OBJECT(argv[0]) || JSVAL_IS_NULL(argv[0])
	    || !JS_ObjectIsFunction(cx, JSVAL_TO_OBJECT(argv[0]))) {
		JS_ReportError(cx, "forEach() requires a function argument");
		return JS_FALSE;
	}
	jsval fn = argv[0];
	jsuint i = 0;
	for (;;) {
		jsrefcount rc_susp = JS_SUSPENDREQUEST(cx);
		int rc = sqlite3_step(p->stmt);
		JS_RESUMEREQUEST(cx, rc_susp);
		if (rc == SQLITE_DONE) break;
		if (rc != SQLITE_ROW) return sqlite_throw(cx, p->db, rc, "step");
		p->generation++;
		p->row_pending = true;
		JSObject* row = new_row_object(cx, obj, p);
		if (row == NULL) return JS_FALSE;
		jsval args[2] = { OBJECT_TO_JSVAL(row), INT_TO_JSVAL((jsint)i) };
		jsval rv;
		if (!JS_CallFunctionValue(cx, obj, fn, 2, args, &rv)) return JS_FALSE;
		i++;
	}
	p->generation++;
	p->row_pending = false;
	sqlite3_reset(p->stmt);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL((jsint)i));
	return JS_TRUE;
}

enum {
	STMT_PROP_SQL,
	STMT_PROP_EXPANDED_SQL,
	STMT_PROP_COLUMN_COUNT,
	STMT_PROP_COLUMN_NAMES,
	STMT_PROP_DECLARED_TYPES,
	STMT_PROP_PARAMETER_COUNT,
	STMT_PROP_PARAMETER_NAMES,
	STMT_PROP_BUSY,
};

#define STMT_PROP_FLAGS (JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)

static jsSyncPropertySpec js_sqlite_stmt_properties[] = {
	{"sql",             STMT_PROP_SQL,             STMT_PROP_FLAGS, 32200},
	{"expanded_sql",    STMT_PROP_EXPANDED_SQL,    STMT_PROP_FLAGS, 32200},
	{"column_count",    STMT_PROP_COLUMN_COUNT,    STMT_PROP_FLAGS, 32200},
	{"column_names",    STMT_PROP_COLUMN_NAMES,    STMT_PROP_FLAGS, 32200},
	{"declared_types",  STMT_PROP_DECLARED_TYPES,  STMT_PROP_FLAGS, 32200},
	{"parameter_count", STMT_PROP_PARAMETER_COUNT, STMT_PROP_FLAGS, 32200},
	{"parameter_names", STMT_PROP_PARAMETER_NAMES, STMT_PROP_FLAGS, 32200},
	{"busy",            STMT_PROP_BUSY,            STMT_PROP_FLAGS, 32200},
	{0}
};

#ifdef BUILD_JSDOCS
static const char* sqlite_stmt_prop_desc[] = {
	  "Original SQL text passed to prepare()"
	, "SQL with bound parameter values interpolated (for logging/debugging only - do not feed back into prepare())"
	, "Number of result columns"
	, "Array of result column names"
	, "Array of per-column declared types (verbatim from CREATE TABLE); null for computed/aggregate/literal columns"
	, "Number of placeholder parameters in the SQL"
	, "Array of parameter names (\":foo\", \"@foo\", etc.); null entries for anonymous ? placeholders"
	, "true between step() returning a row and the next step()/reset()"
	, NULL
};
#endif

static JSBool js_sqlite_stmt_get(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	sqlite_stmt_private_t* p = (sqlite_stmt_private_t*)JS_GetPrivate(cx, obj);
	if (p == NULL) return JS_TRUE;
	if (p->stmt == NULL) return JS_TRUE;

	jsval idval;
	if (!JS_IdToValue(cx, id, &idval) || !JSVAL_IS_INT(idval))
		return JS_TRUE;

	switch (JSVAL_TO_INT(idval)) {
		case STMT_PROP_SQL: {
			JSString* s = JS_NewStringCopyZ(cx, p->sql ? p->sql : "");
			if (s == NULL) return JS_FALSE;
			*vp = STRING_TO_JSVAL(s);
			break;
		}
		case STMT_PROP_EXPANDED_SQL: {
			char* expanded = sqlite3_expanded_sql(p->stmt);
			JSString* s = JS_NewStringCopyZ(cx, expanded ? expanded : "");
			if (expanded) sqlite3_free(expanded);
			if (s == NULL) return JS_FALSE;
			*vp = STRING_TO_JSVAL(s);
			break;
		}
		case STMT_PROP_COLUMN_COUNT:
			*vp = INT_TO_JSVAL(sqlite3_column_count(p->stmt));
			break;
		case STMT_PROP_PARAMETER_COUNT:
			*vp = INT_TO_JSVAL(sqlite3_bind_parameter_count(p->stmt));
			break;
		case STMT_PROP_BUSY:
			*vp = BOOLEAN_TO_JSVAL(p->row_pending);
			break;
		case STMT_PROP_COLUMN_NAMES: {
			int ncols = sqlite3_column_count(p->stmt);
			JSObject* arr = JS_NewArrayObject(cx, ncols, NULL);
			if (arr == NULL) return JS_FALSE;
			for (int i = 0; i < ncols; i++) {
				const char* cn = sqlite3_column_name(p->stmt, i);
				JSString* s = JS_NewStringCopyZ(cx, cn ? cn : "");
				if (s == NULL) return JS_FALSE;
				jsval v = STRING_TO_JSVAL(s);
				if (!JS_SetElement(cx, arr, i, &v)) return JS_FALSE;
			}
			*vp = OBJECT_TO_JSVAL(arr);
			break;
		}
		case STMT_PROP_DECLARED_TYPES: {
			int ncols = sqlite3_column_count(p->stmt);
			JSObject* arr = JS_NewArrayObject(cx, ncols, NULL);
			if (arr == NULL) return JS_FALSE;
			for (int i = 0; i < ncols; i++) {
				const char* dt = sqlite3_column_decltype(p->stmt, i);
				jsval v;
				if (dt == NULL) v = JSVAL_NULL;
				else {
					JSString* s = JS_NewStringCopyZ(cx, dt);
					if (s == NULL) return JS_FALSE;
					v = STRING_TO_JSVAL(s);
				}
				if (!JS_SetElement(cx, arr, i, &v)) return JS_FALSE;
			}
			*vp = OBJECT_TO_JSVAL(arr);
			break;
		}
		case STMT_PROP_PARAMETER_NAMES: {
			int n = sqlite3_bind_parameter_count(p->stmt);
			JSObject* arr = JS_NewArrayObject(cx, n, NULL);
			if (arr == NULL) return JS_FALSE;
			for (int i = 0; i < n; i++) {
				const char* pn = sqlite3_bind_parameter_name(p->stmt, i + 1);
				jsval v;
				if (pn == NULL) v = JSVAL_NULL;
				else {
					JSString* s = JS_NewStringCopyZ(cx, pn);
					if (s == NULL) return JS_FALSE;
					v = STRING_TO_JSVAL(s);
				}
				if (!JS_SetElement(cx, arr, i, &v)) return JS_FALSE;
			}
			*vp = OBJECT_TO_JSVAL(arr);
			break;
		}
	}
	return JS_TRUE;
}

static jsSyncMethodSpec js_sqlite_stmt_functions[] = {
	{"bind",          js_stmt_bind,           1, JSTYPE_OBJECT,
	 JSDOCSTR("array | object"),
	 JSDOCSTR("Bind parameters in one shot. With an array, element 0 binds to the first placeholder, element 1 to the second, etc. With an object, keys are matched against named placeholders (\":foo\", \"@foo\", \"$foo\"); a bare key like \"foo\" matches any of those. Object keys that don't correspond to any placeholder are silently ignored — so a SQLiteRecord.$toObject() snapshot, or any other object that carries more fields than the SQL references, can be passed directly. "
	          "<b>Note:</b> bind() calls sqlite3_clear_bindings() first, so each call fully replaces the statement's binding state. Parameters not mentioned in the new array or object reset to NULL. To override a single slot without disturbing the others, use the typed binders (bindInt / bindText / etc.), which do not clear."),
	 32200},
	{"bindInt",       js_stmt_bindInt,        2, JSTYPE_OBJECT,
	 JSDOCSTR("index, value"),
	 JSDOCSTR("Bind an integer (int64). <i>index</i> is either a 1-based "
	          "integer position (per SQLite convention) or a string parameter name."),
	 32200},
	{"bindReal",      js_stmt_bindReal,       2, JSTYPE_OBJECT,
	 JSDOCSTR("index, value"),
	 JSDOCSTR("Bind a double-precision float. <i>index</i> is either a 1-based "
	          "integer position or a string parameter name."),
	 32200},
	{"bindText",      js_stmt_bindText,       2, JSTYPE_OBJECT,
	 JSDOCSTR("index, value"),
	 JSDOCSTR("Bind a text string. <i>index</i> is either a 1-based integer "
	          "position or a string parameter name. Strings are passed through "
	          "as narrow bytes (Synchronet convention); non-ASCII chars are stored "
	          "as their low byte and round-trip within Synchronet but are not "
	          "portable UTF-8."),
	 32200},
	{"bindBlob",      js_stmt_bindBlob,       2, JSTYPE_OBJECT,
	 JSDOCSTR("index, ArrayBuffer | TypedArray"),
	 JSDOCSTR("Bind a binary blob. <i>index</i> is either a 1-based integer "
	          "position or a string parameter name."),
	 32200},
	{"bindNull",      js_stmt_bindNull,       1, JSTYPE_OBJECT,
	 JSDOCSTR("index"),
	 JSDOCSTR("Bind SQL NULL. <i>index</i> is either a 1-based integer position "
	          "or a string parameter name."),
	 32200},
	{"bindUtf8",      js_stmt_bindUtf8,       2, JSTYPE_OBJECT,
	 JSDOCSTR("index, string"),
	 JSDOCSTR("Bind a string as proper Unicode via UTF-16. <i>index</i> is either "
	          "a 1-based integer position or a string parameter name. Stored as "
	          "portable UTF-8 in the SQLite file. Pair with the .utf8 accessor on read."),
	 32200},
	{"step",          js_stmt_step,           0, JSTYPE_OBJECT,
	 JSDOCSTR(""),
	 JSDOCSTR("Advance the statement. Returns a SQLiteRow on row available, null when done."),
	 32200},
	{"reset",         js_stmt_reset,          0, JSTYPE_OBJECT,
	 JSDOCSTR(""),
	 JSDOCSTR("Reset the statement so it can be re-executed. Does not clear bindings."),
	 32200},
	{"clearBindings", js_stmt_clearBindings,  0, JSTYPE_OBJECT,
	 JSDOCSTR(""),
	 JSDOCSTR("Clear all bound parameters."),
	 32200},
	{"close",         js_stmt_close,          0, JSTYPE_VOID,
	 JSDOCSTR(""),
	 JSDOCSTR("Finalize the statement. Same as letting it be garbage-collected."),
	 32200},
	{"finalize",      js_stmt_close,          0, JSTYPE_VOID,
	 JSDOCSTR(""),
	 JSDOCSTR("Alias for close()."),
	 32200},
	{"all",           js_stmt_all,            0, JSTYPE_ARRAY,
	 JSDOCSTR(""),
	 JSDOCSTR("Step through all rows, returning an array of plain row objects (snapshot copies)."),
	 32200},
	{"forEach",       js_stmt_forEach,        1, JSTYPE_NUMBER,
	 JSDOCSTR("function(row, index)"),
	 JSDOCSTR("Call function for each row. Returns the row count."),
	 32200},
	{0}
};

static JSBool js_sqlite_stmt_resolve(JSContext* cx, JSObject* obj, jsid id)
{
	char*  name = NULL;
	JSBool ret;
	if (id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		JS_IdToValue(cx, id, &idval);
		if (JSVAL_IS_STRING(idval))
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
	}
	ret = js_SyncResolve(cx, obj, name, js_sqlite_stmt_properties,
	    js_sqlite_stmt_functions, NULL, 0);
	if (name) free(name);
	return ret;
}

static JSBool js_sqlite_stmt_enumerate(JSContext* cx, JSObject* obj)
{
	return js_sqlite_stmt_resolve(cx, obj, JSID_VOID);
}

JSClass js_sqlite_stmt_class = {
	"SQLiteStatement"
	, JSCLASS_HAS_PRIVATE
	, JS_PropertyStub
	, JS_PropertyStub
	, js_sqlite_stmt_get
	, JS_StrictPropertyStub
	, js_sqlite_stmt_enumerate
	, js_sqlite_stmt_resolve
	, JS_ConvertStub
	, js_finalize_stmt
};

static JSObject* new_stmt_object(JSContext* cx, JSObject* conn_obj,
                                 sqlite3* db, sqlite3_stmt* stmt, const char* sql)
{
	JSObject* obj = JS_NewObject(cx, &js_sqlite_stmt_class, NULL, NULL);
	if (obj == NULL) return NULL;
	sqlite_stmt_private_t* p = (sqlite_stmt_private_t*)calloc(1, sizeof(*p));
	if (p == NULL) { JS_ReportError(cx, "calloc failed"); return NULL; }
	p->db = db;
	p->stmt = stmt;
	p->sql = sql ? strdup(sql) : NULL;
	p->generation = 1;
	p->row_pending = false;
	if (!JS_SetPrivate(cx, obj, p)) {
		free(p->sql);
		free(p);
		JS_ReportError(cx, "JS_SetPrivate failed");
		return NULL;
	}
	JS_DefineProperty(cx, obj, ROOT_PROP_NAME, OBJECT_TO_JSVAL(conn_obj),
	    NULL, NULL, JSPROP_PERMANENT | JSPROP_READONLY);
#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj,
	    "Prepared SQL statement. Obtained from db.prepare(sql); not directly "
	    "constructible. Bind parameters before stepping; never concatenate "
	    "untrusted values into the SQL.", 32200);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list",
	    sqlite_stmt_prop_desc, JSPROP_READONLY);
#endif
	return obj;
}

/* ---------- SQLite (connection) ---------- */

static void js_finalize_conn(JSContext* cx, JSObject* obj)
{
	sqlite_conn_private_t* p = (sqlite_conn_private_t*)JS_GetPrivate(cx, obj);
	if (p == NULL)
		return;
	if (p->db) {
		sqlite3_close_v2(p->db);
		p->db = NULL;
	}
	if (p->path) {
		free(p->path);
		p->path = NULL;
	}
	/* Free the table-name lookup chain. The cached SQLiteTable JSObjects
	 * themselves are owned by the GC (rooted as permanent properties of
	 * the db.table namespace) and will be finalized through their own
	 * finalize hooks; we only own the lookup nodes here. */
	while (p->table_cache) {
		sqlite_table_cache_node_t* next = p->table_cache->next;
		free(p->table_cache->name_lower);
		free(p->table_cache);
		p->table_cache = next;
	}
	free(p);
	JS_SetPrivate(cx, obj, NULL);
}

static JSBool js_conn_prepare(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	jsval*    argv = JS_ARGV(cx, arglist);
	sqlite_conn_private_t* p = (sqlite_conn_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_class);
	if (p == NULL) return JS_FALSE;
	if (p->db == NULL) { JS_ReportError(cx, "Connection is closed"); return JS_FALSE; }
	if (argc < 1 || !JSVAL_IS_STRING(argv[0])) {
		JS_ReportError(cx, "prepare() requires an SQL string");
		return JS_FALSE;
	}
	char* sql = NULL;
	JSVALUE_TO_MSTRING(cx, argv[0], sql, NULL);
	if (sql == NULL) return JS_FALSE;

	sqlite3_stmt* stmt = NULL;
	jsrefcount rc_susp = JS_SUSPENDREQUEST(cx);
	int rc = sqlite3_prepare_v2(p->db, sql, -1, &stmt, NULL);
	JS_RESUMEREQUEST(cx, rc_susp);
	if (rc != SQLITE_OK) {
		JSBool b = sqlite_throw(cx, p->db, rc, "prepare");
		free(sql);
		if (stmt) sqlite3_finalize(stmt);
		return b;
	}
	if (stmt == NULL) {
		JS_ReportError(cx, "prepare: empty SQL");
		free(sql);
		return JS_FALSE;
	}
	JSObject* so = new_stmt_object(cx, obj, p->db, stmt, sql);
	free(sql);
	if (so == NULL) { sqlite3_finalize(stmt); return JS_FALSE; }
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(so));
	return JS_TRUE;
}

static JSBool js_conn_exec(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	jsval*    argv = JS_ARGV(cx, arglist);
	sqlite_conn_private_t* p = (sqlite_conn_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_class);
	if (p == NULL) return JS_FALSE;
	if (p->db == NULL) { JS_ReportError(cx, "Connection is closed"); return JS_FALSE; }
	if (argc != 1 || !JSVAL_IS_STRING(argv[0])) {
		JS_ReportError(cx, "exec(sql) takes exactly one string argument and accepts no parameters by design. For values, use prepare()+bind() or query()/run().");
		return JS_FALSE;
	}
	char* sql = NULL;
	JSVALUE_TO_MSTRING(cx, argv[0], sql, NULL);
	if (sql == NULL) return JS_FALSE;
	char* errmsg = NULL;
	jsrefcount rc_susp = JS_SUSPENDREQUEST(cx);
	int rc = sqlite3_exec(p->db, sql, NULL, NULL, &errmsg);
	JS_RESUMEREQUEST(cx, rc_susp);
	free(sql);
	if (rc != SQLITE_OK) {
		JS_ReportError(cx, "exec: %s (rc=%d)", errmsg ? errmsg : sqlite3_errstr(rc), rc);
		if (errmsg) sqlite3_free(errmsg);
		return JS_FALSE;
	}
	if (errmsg) sqlite3_free(errmsg);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(sqlite3_changes(p->db)));
	return JS_TRUE;
}

/* Common helper for query()/run(): prepare → bind → step* → return */
static JSBool query_or_run(JSContext* cx, JSObject* conn_obj, sqlite_conn_private_t* p,
                           jsval sql_val, jsval params_val, bool have_params,
                           bool collect_rows, jsval* rval)
{
	if (!JSVAL_IS_STRING(sql_val)) {
		JS_ReportError(cx, "first argument must be an SQL string");
		return JS_FALSE;
	}
	char* sql = NULL;
	JSVALUE_TO_MSTRING(cx, sql_val, sql, NULL);
	if (sql == NULL) return JS_FALSE;

	sqlite3_stmt* stmt = NULL;
	jsrefcount rc_susp = JS_SUSPENDREQUEST(cx);
	int rc = sqlite3_prepare_v2(p->db, sql, -1, &stmt, NULL);
	JS_RESUMEREQUEST(cx, rc_susp);
	free(sql);
	if (rc != SQLITE_OK) {
		if (stmt) sqlite3_finalize(stmt);
		return sqlite_throw(cx, p->db, rc, "prepare");
	}
	if (stmt == NULL) {
		JS_ReportError(cx, "empty SQL");
		return JS_FALSE;
	}

	int pcount = sqlite3_bind_parameter_count(stmt);
	if (pcount > 0 && !have_params) {
		sqlite3_finalize(stmt);
		JS_ReportError(cx, "SQL has %d parameter(s) but no params argument supplied. Pass an array or object as the second argument.", pcount);
		return JS_FALSE;
	}

	if (have_params && !JSVAL_IS_NULL(params_val) && !JSVAL_IS_VOID(params_val)) {
		if (!JSVAL_IS_OBJECT(params_val)) {
			sqlite3_finalize(stmt);
			JS_ReportError(cx, "params must be array or object");
			return JS_FALSE;
		}
		JSObject* o = JSVAL_TO_OBJECT(params_val);
		if (JS_IsArrayObject(cx, o)) {
			jsuint length = 0;
			if (!JS_GetArrayLength(cx, o, &length)) { sqlite3_finalize(stmt); return JS_FALSE; }
			for (jsuint i = 0; i < length; i++) {
				jsval v;
				if (!JS_GetElement(cx, o, (jsint)i, &v)) { sqlite3_finalize(stmt); return JS_FALSE; }
				if (!bind_jsval(cx, stmt, (int)i + 1, v)) { sqlite3_finalize(stmt); return JS_FALSE; }
			}
		} else {
			JSObject* iter = JS_NewPropertyIterator(cx, o);
			if (iter == NULL) { sqlite3_finalize(stmt); return JS_FALSE; }
			jsid pid;
			while (JS_NextProperty(cx, iter, &pid) && pid != JSID_VOID) {
				jsval idv;
				if (!JS_IdToValue(cx, pid, &idv)) { sqlite3_finalize(stmt); return JS_FALSE; }
				if (!JSVAL_IS_STRING(idv)) continue;
				char* key = NULL;
				JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idv), key, NULL);
				if (key == NULL) { sqlite3_finalize(stmt); return JS_FALSE; }
				int idx = resolve_param_index(stmt, key);
				if (idx <= 0) {
					/* Object keys that don't match a placeholder are
					 * silently skipped — see stmt.bind() comment. */
					free(key);
					continue;
				}
				jsval v;
				if (!JS_GetProperty(cx, o, key, &v)) { free(key); sqlite3_finalize(stmt); return JS_FALSE; }
				free(key);
				if (!bind_jsval(cx, stmt, idx, v)) { sqlite3_finalize(stmt); return JS_FALSE; }
			}
		}
	}

	/* Temporary stmt_private for snapshot_row(). */
	sqlite_stmt_private_t tmp;
	memset(&tmp, 0, sizeof(tmp));
	tmp.db = p->db;
	tmp.stmt = stmt;

	if (collect_rows) {
		JSObject* arr = JS_NewArrayObject(cx, 0, NULL);
		if (arr == NULL) { sqlite3_finalize(stmt); return JS_FALSE; }
		jsuint n = 0;
		for (;;) {
			rc_susp = JS_SUSPENDREQUEST(cx);
			rc = sqlite3_step(stmt);
			JS_RESUMEREQUEST(cx, rc_susp);
			if (rc == SQLITE_DONE) break;
			if (rc != SQLITE_ROW) {
				JSBool b = sqlite_throw(cx, p->db, rc, "step");
				sqlite3_finalize(stmt);
				return b;
			}
			jsval rowv;
			if (!snapshot_row(cx, &tmp, &rowv)) { sqlite3_finalize(stmt); return JS_FALSE; }
			if (!JS_SetElement(cx, arr, (jsint)n, &rowv)) { sqlite3_finalize(stmt); return JS_FALSE; }
			n++;
		}
		sqlite3_finalize(stmt);
		*rval = OBJECT_TO_JSVAL(arr);
		return JS_TRUE;
	}
	/* run(): step until DONE, return changes */
	for (;;) {
		rc_susp = JS_SUSPENDREQUEST(cx);
		rc = sqlite3_step(stmt);
		JS_RESUMEREQUEST(cx, rc_susp);
		if (rc == SQLITE_DONE) break;
		if (rc != SQLITE_ROW) {
			JSBool b = sqlite_throw(cx, p->db, rc, "step");
			sqlite3_finalize(stmt);
			return b;
		}
		/* Allow run() to consume row-returning SQL silently. */
	}
	int changes = sqlite3_changes(p->db);
	sqlite3_finalize(stmt);
	*rval = INT_TO_JSVAL(changes);
	return JS_TRUE;
}

static JSBool js_conn_query(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	jsval*    argv = JS_ARGV(cx, arglist);
	sqlite_conn_private_t* p = (sqlite_conn_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_class);
	if (p == NULL) return JS_FALSE;
	if (p->db == NULL) { JS_ReportError(cx, "Connection is closed"); return JS_FALSE; }
	if (argc < 1) { JS_ReportError(cx, "query() requires an SQL string"); return JS_FALSE; }
	jsval rv = JSVAL_VOID;
	if (!query_or_run(cx, obj, p, argv[0], argc > 1 ? argv[1] : JSVAL_VOID,
	                  argc > 1, true, &rv))
		return JS_FALSE;
	JS_SET_RVAL(cx, arglist, rv);
	return JS_TRUE;
}

static JSBool js_conn_run(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	jsval*    argv = JS_ARGV(cx, arglist);
	sqlite_conn_private_t* p = (sqlite_conn_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_class);
	if (p == NULL) return JS_FALSE;
	if (p->db == NULL) { JS_ReportError(cx, "Connection is closed"); return JS_FALSE; }
	if (argc < 1) { JS_ReportError(cx, "run() requires an SQL string"); return JS_FALSE; }
	jsval rv = JSVAL_VOID;
	if (!query_or_run(cx, obj, p, argv[0], argc > 1 ? argv[1] : JSVAL_VOID,
	                  argc > 1, false, &rv))
		return JS_FALSE;
	JS_SET_RVAL(cx, arglist, rv);
	return JS_TRUE;
}

static JSBool js_conn_transaction(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	jsval*    argv = JS_ARGV(cx, arglist);
	sqlite_conn_private_t* p = (sqlite_conn_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_class);
	if (p == NULL) return JS_FALSE;
	if (p->db == NULL) { JS_ReportError(cx, "Connection is closed"); return JS_FALSE; }
	/* SM 1.8.5 quirk: JSVAL_IS_OBJECT(JSVAL_NULL) is true and
	 * JSVAL_TO_OBJECT(JSVAL_NULL) is NULL, which JS_ObjectIsFunction would
	 * dereference. Reject null explicitly. */
	if (argc < 1 || !JSVAL_IS_OBJECT(argv[0]) || JSVAL_IS_NULL(argv[0])
	    || !JS_ObjectIsFunction(cx, JSVAL_TO_OBJECT(argv[0]))) {
		JS_ReportError(cx, "transaction(fn) requires a function argument");
		return JS_FALSE;
	}

	char* err = NULL;
	jsrefcount rc_susp = JS_SUSPENDREQUEST(cx);
	int rc = sqlite3_exec(p->db, "BEGIN IMMEDIATE", NULL, NULL, &err);
	JS_RESUMEREQUEST(cx, rc_susp);
	if (rc != SQLITE_OK) {
		JS_ReportError(cx, "transaction begin: %s (rc=%d)",
		    err ? err : sqlite3_errstr(rc), rc);
		if (err) sqlite3_free(err);
		return JS_FALSE;
	}

	jsval rv;
	JSBool ok = JS_CallFunctionValue(cx, obj, argv[0], 0, NULL, &rv);

	/* The callback may have closed the connection. If so, the BEGIN was
	 * implicitly rolled back by sqlite3_close_v2 and there's no handle to
	 * COMMIT/ROLLBACK against. */
	if (p->db == NULL) {
		if (!ok) return JS_FALSE;
		JS_ReportError(cx, "transaction(fn): connection was closed inside callback");
		return JS_FALSE;
	}

	rc_susp = JS_SUSPENDREQUEST(cx);
	rc = sqlite3_exec(p->db, ok ? "COMMIT" : "ROLLBACK", NULL, NULL, NULL);
	JS_RESUMEREQUEST(cx, rc_susp);
	if (!ok) return JS_FALSE;
	if (rc != SQLITE_OK)
		return sqlite_throw(cx, p->db, rc, "commit");
	JS_SET_RVAL(cx, arglist, rv);
	return JS_TRUE;
}

static JSBool js_conn_close(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	sqlite_conn_private_t* p = (sqlite_conn_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_class);
	if (p == NULL) return JS_FALSE;
	if (p->db) {
		sqlite3_close_v2(p->db);
		p->db = NULL;
	}
	return JS_TRUE;
}

static JSBool js_conn_tables(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	sqlite_conn_private_t* p = (sqlite_conn_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_class);
	if (p == NULL) return JS_FALSE;
	if (p->db == NULL) { JS_ReportError(cx, "Connection is closed"); return JS_FALSE; }

	const char* sql = "SELECT name FROM sqlite_master "
	                  "WHERE type = 'table' AND name NOT LIKE 'sqlite_%' "
	                  "ORDER BY name";
	sqlite3_stmt* stmt = NULL;
	jsrefcount rc_susp = JS_SUSPENDREQUEST(cx);
	int rc = sqlite3_prepare_v2(p->db, sql, -1, &stmt, NULL);
	JS_RESUMEREQUEST(cx, rc_susp);
	if (rc != SQLITE_OK) return sqlite_throw(cx, p->db, rc, "tables");

	JSObject* arr = JS_NewArrayObject(cx, 0, NULL);
	if (arr == NULL) { sqlite3_finalize(stmt); return JS_FALSE; }
	jsuint n = 0;
	for (;;) {
		rc_susp = JS_SUSPENDREQUEST(cx);
		rc = sqlite3_step(stmt);
		JS_RESUMEREQUEST(cx, rc_susp);
		if (rc == SQLITE_DONE) break;
		if (rc != SQLITE_ROW) {
			JSBool b = sqlite_throw(cx, p->db, rc, "tables");
			sqlite3_finalize(stmt);
			return b;
		}
		const char* s = (const char*)sqlite3_column_text(stmt, 0);
		int nlen = sqlite3_column_bytes(stmt, 0);
		JSString* js = JS_NewStringCopyN(cx, s ? s : "", nlen);
		if (js == NULL) { sqlite3_finalize(stmt); return JS_FALSE; }
		jsval v = STRING_TO_JSVAL(js);
		if (!JS_SetElement(cx, arr, (jsint)n, &v)) { sqlite3_finalize(stmt); return JS_FALSE; }
		n++;
	}
	sqlite3_finalize(stmt);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(arr));
	return JS_TRUE;
}

static JSBool js_conn_has_table(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	jsval*    argv = JS_ARGV(cx, arglist);
	sqlite_conn_private_t* p = (sqlite_conn_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_class);
	if (p == NULL) return JS_FALSE;
	if (p->db == NULL) { JS_ReportError(cx, "Connection is closed"); return JS_FALSE; }
	if (argc < 1 || !JSVAL_IS_STRING(argv[0])) {
		JS_ReportError(cx, "has_table() requires a table name");
		return JS_FALSE;
	}

	const char* sql = "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?";
	sqlite3_stmt* stmt = NULL;
	jsrefcount rc_susp = JS_SUSPENDREQUEST(cx);
	int rc = sqlite3_prepare_v2(p->db, sql, -1, &stmt, NULL);
	JS_RESUMEREQUEST(cx, rc_susp);
	if (rc != SQLITE_OK) return sqlite_throw(cx, p->db, rc, "has_table/prepare");

	char* tname = NULL;
	JSVALUE_TO_MSTRING(cx, argv[0], tname, NULL);
	if (tname == NULL) { sqlite3_finalize(stmt); return JS_FALSE; }
	rc = sqlite3_bind_text(stmt, 1, tname, -1, SQLITE_TRANSIENT);
	free(tname);
	if (rc != SQLITE_OK) {
		JSBool b = sqlite_throw(cx, p->db, rc, "has_table/bind");
		sqlite3_finalize(stmt);
		return b;
	}

	rc_susp = JS_SUSPENDREQUEST(cx);
	rc = sqlite3_step(stmt);
	JS_RESUMEREQUEST(cx, rc_susp);
	sqlite3_finalize(stmt);
	if (rc == SQLITE_ROW) { JS_SET_RVAL(cx, arglist, JSVAL_TRUE);  return JS_TRUE; }
	if (rc == SQLITE_DONE){ JS_SET_RVAL(cx, arglist, JSVAL_FALSE); return JS_TRUE; }
	return sqlite_throw(cx, p->db, rc, "has_table/step");
}

static JSBool js_conn_backup(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	jsval*    argv = JS_ARGV(cx, arglist);
	sqlite_conn_private_t* p = (sqlite_conn_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_class);
	if (p == NULL) return JS_FALSE;
	if (p->db == NULL) { JS_ReportError(cx, "Connection is closed"); return JS_FALSE; }
	if (argc < 1 || !JSVAL_IS_STRING(argv[0])) {
		JS_ReportError(cx, "backup() requires a destination path");
		return JS_FALSE;
	}
	char* dest_path = NULL;
	JSVALUE_TO_MSTRING(cx, argv[0], dest_path, NULL);
	if (dest_path == NULL) return JS_FALSE;

	sqlite3* dest = NULL;
	int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
	jsrefcount rc_susp = JS_SUSPENDREQUEST(cx);
	int rc = sqlite3_open_v2(dest_path, &dest, flags, NULL);
	JS_RESUMEREQUEST(cx, rc_susp);
	if (rc != SQLITE_OK) {
		JS_ReportError(cx, "backup: open '%s': %s (rc=%d)",
		    dest_path, dest ? sqlite3_errmsg(dest) : sqlite3_errstr(rc), rc);
		if (dest) sqlite3_close_v2(dest);
		free(dest_path);
		return JS_FALSE;
	}
	free(dest_path);

	sqlite3_backup* bkp = sqlite3_backup_init(dest, "main", p->db, "main");
	if (bkp == NULL) {
		JSBool b = sqlite_throw(cx, dest, sqlite3_errcode(dest), "backup_init");
		sqlite3_close_v2(dest);
		return b;
	}

	rc_susp = JS_SUSPENDREQUEST(cx);
	rc = sqlite3_backup_step(bkp, -1);  /* -1 = all remaining pages */
	int finish_rc = sqlite3_backup_finish(bkp);
	int dest_close_rc = sqlite3_close_v2(dest);
	JS_RESUMEREQUEST(cx, rc_susp);
	(void)dest_close_rc;

	if (rc != SQLITE_DONE)
		return sqlite_throw(cx, p->db, rc, "backup_step");
	if (finish_rc != SQLITE_OK)
		return sqlite_throw(cx, p->db, finish_rc, "backup_finish");
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	return JS_TRUE;
}

static JSBool js_conn_columns(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	jsval*    argv = JS_ARGV(cx, arglist);
	sqlite_conn_private_t* p = (sqlite_conn_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_class);
	if (p == NULL) return JS_FALSE;
	if (p->db == NULL) { JS_ReportError(cx, "Connection is closed"); return JS_FALSE; }
	if (argc < 1 || !JSVAL_IS_STRING(argv[0])) {
		JS_ReportError(cx, "columns() requires a table name");
		return JS_FALSE;
	}

	/* pragma_table_info() takes a bound parameter, avoiding the injection
	 * risk of the classical PRAGMA syntax. */
	const char* sql = "SELECT cid, name, type, \"notnull\", dflt_value, pk "
	                  "FROM pragma_table_info(?) ORDER BY cid";
	sqlite3_stmt* stmt = NULL;
	jsrefcount rc_susp = JS_SUSPENDREQUEST(cx);
	int rc = sqlite3_prepare_v2(p->db, sql, -1, &stmt, NULL);
	JS_RESUMEREQUEST(cx, rc_susp);
	if (rc != SQLITE_OK) return sqlite_throw(cx, p->db, rc, "columns/prepare");

	char* tname = NULL;
	JSVALUE_TO_MSTRING(cx, argv[0], tname, NULL);
	if (tname == NULL) { sqlite3_finalize(stmt); return JS_FALSE; }
	rc = sqlite3_bind_text(stmt, 1, tname, -1, SQLITE_TRANSIENT);
	free(tname);
	if (rc != SQLITE_OK) {
		JSBool b = sqlite_throw(cx, p->db, rc, "columns/bind");
		sqlite3_finalize(stmt);
		return b;
	}

	JSObject* arr = JS_NewArrayObject(cx, 0, NULL);
	if (arr == NULL) { sqlite3_finalize(stmt); return JS_FALSE; }
	jsuint n = 0;
	for (;;) {
		rc_susp = JS_SUSPENDREQUEST(cx);
		rc = sqlite3_step(stmt);
		JS_RESUMEREQUEST(cx, rc_susp);
		if (rc == SQLITE_DONE) break;
		if (rc != SQLITE_ROW) {
			JSBool b = sqlite_throw(cx, p->db, rc, "columns/step");
			sqlite3_finalize(stmt);
			return b;
		}
		JSObject* o = JS_NewObject(cx, NULL, NULL, NULL);
		if (o == NULL) { sqlite3_finalize(stmt); return JS_FALSE; }
		jsval v;

		v = INT_TO_JSVAL(sqlite3_column_int(stmt, 0));
		JS_DefineProperty(cx, o, "cid", v, NULL, NULL, JSPROP_ENUMERATE);

		const char* s = (const char*)sqlite3_column_text(stmt, 1);
		int slen = sqlite3_column_bytes(stmt, 1);
		JSString* js = JS_NewStringCopyN(cx, s ? s : "", slen);
		if (js == NULL) { sqlite3_finalize(stmt); return JS_FALSE; }
		v = STRING_TO_JSVAL(js);
		JS_DefineProperty(cx, o, "name", v, NULL, NULL, JSPROP_ENUMERATE);

		s = (const char*)sqlite3_column_text(stmt, 2);
		slen = sqlite3_column_bytes(stmt, 2);
		js = JS_NewStringCopyN(cx, s ? s : "", slen);
		if (js == NULL) { sqlite3_finalize(stmt); return JS_FALSE; }
		v = STRING_TO_JSVAL(js);
		JS_DefineProperty(cx, o, "type", v, NULL, NULL, JSPROP_ENUMERATE);

		v = BOOLEAN_TO_JSVAL(sqlite3_column_int(stmt, 3) != 0);
		JS_DefineProperty(cx, o, "notnull", v, NULL, NULL, JSPROP_ENUMERATE);

		if (sqlite3_column_type(stmt, 4) == SQLITE_NULL) {
			v = JSVAL_NULL;
		} else {
			s = (const char*)sqlite3_column_text(stmt, 4);
			slen = sqlite3_column_bytes(stmt, 4);
			js = JS_NewStringCopyN(cx, s ? s : "", slen);
			if (js == NULL) { sqlite3_finalize(stmt); return JS_FALSE; }
			v = STRING_TO_JSVAL(js);
		}
		JS_DefineProperty(cx, o, "dflt_value", v, NULL, NULL, JSPROP_ENUMERATE);

		v = INT_TO_JSVAL(sqlite3_column_int(stmt, 5));
		JS_DefineProperty(cx, o, "pk", v, NULL, NULL, JSPROP_ENUMERATE);

		v = OBJECT_TO_JSVAL(o);
		if (!JS_SetElement(cx, arr, (jsint)n, &v)) { sqlite3_finalize(stmt); return JS_FALSE; }
		n++;
	}
	sqlite3_finalize(stmt);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(arr));
	return JS_TRUE;
}

enum {
	CONN_PROP_PATH,
	CONN_PROP_LAST_INSERT_ID,
	CONN_PROP_CHANGES,
	CONN_PROP_TOTAL_CHANGES,
	CONN_PROP_ERROR_CODE,
	CONN_PROP_ERROR_MESSAGE,
	CONN_PROP_IN_TRANSACTION,
	CONN_PROP_BUSY_TIMEOUT,
};

#define CONN_PROP_RO_FLAGS (JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)
#define CONN_PROP_RW_FLAGS (JSPROP_ENUMERATE | JSPROP_PERMANENT)

static jsSyncPropertySpec js_sqlite_conn_properties[] = {
	{"path",           CONN_PROP_PATH,           CONN_PROP_RO_FLAGS, 32200},
	{"last_insert_id", CONN_PROP_LAST_INSERT_ID, CONN_PROP_RO_FLAGS, 32200},
	{"changes",        CONN_PROP_CHANGES,        CONN_PROP_RO_FLAGS, 32200},
	{"total_changes",  CONN_PROP_TOTAL_CHANGES,  CONN_PROP_RO_FLAGS, 32200},
	{"error_code",     CONN_PROP_ERROR_CODE,     CONN_PROP_RO_FLAGS, 32200},
	{"error_message",  CONN_PROP_ERROR_MESSAGE,  CONN_PROP_RO_FLAGS, 32200},
	{"in_transaction", CONN_PROP_IN_TRANSACTION, CONN_PROP_RO_FLAGS, 32200},
	{"busy_timeout",   CONN_PROP_BUSY_TIMEOUT,   CONN_PROP_RW_FLAGS, 32200},
	{0}
};

#ifdef BUILD_JSDOCS
static const char* sqlite_conn_prop_desc[] = {
	  "Path passed to the constructor (\":memory:\" for an in-memory database)"
	, "Row ID of the most recent successful INSERT into a rowid table"
	, "Number of rows changed by the most recent INSERT/UPDATE/DELETE"
	, "Total number of rows changed by all INSERT/UPDATE/DELETE since the connection was opened"
	, "Extended SQLite result code from the most recent failed call (SQLite.OK on success). Compare against the SQLite.* constants directly; the primary code is the low byte (<tt>(db.error_code & 0xFF) === SQLite.CONSTRAINT</tt>)"
	, "Human-readable message for the most recent error"
	, "true while inside a BEGIN ... COMMIT/ROLLBACK"
	, "Maximum milliseconds to wait for a locked database before failing with SQLITE_BUSY (defaults to 5000)"
	, NULL
};
#endif

static JSBool js_sqlite_conn_get(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	sqlite_conn_private_t* p = (sqlite_conn_private_t*)JS_GetPrivate(cx, obj);
	if (p == NULL) return JS_TRUE;

	jsval idval;
	if (!JS_IdToValue(cx, id, &idval) || !JSVAL_IS_INT(idval))
		return JS_TRUE;

	switch (JSVAL_TO_INT(idval)) {
		case CONN_PROP_PATH: {
			JSString* s = JS_NewStringCopyZ(cx, p->path ? p->path : "");
			if (s == NULL) return JS_FALSE;
			*vp = STRING_TO_JSVAL(s);
			break;
		}
		case CONN_PROP_LAST_INSERT_ID:
			if (p->db) return JS_NewNumberValue(cx, (double)sqlite3_last_insert_rowid(p->db), vp);
			*vp = INT_TO_JSVAL(0);
			break;
		case CONN_PROP_CHANGES:
			*vp = INT_TO_JSVAL(p->db ? sqlite3_changes(p->db) : 0);
			break;
		case CONN_PROP_TOTAL_CHANGES:
			*vp = INT_TO_JSVAL(p->db ? sqlite3_total_changes(p->db) : 0);
			break;
		case CONN_PROP_ERROR_CODE:
			/* Extended codes are enabled per-connection, so this returns
			 * the specific variant (e.g. SQLITE_CONSTRAINT_UNIQUE) rather
			 * than just the primary code. The primary is the low byte. */
			*vp = INT_TO_JSVAL(p->db ? sqlite3_extended_errcode(p->db) : SQLITE_OK);
			break;
		case CONN_PROP_ERROR_MESSAGE: {
			const char* m = p->db ? sqlite3_errmsg(p->db) : "";
			JSString* s = JS_NewStringCopyZ(cx, m ? m : "");
			if (s == NULL) return JS_FALSE;
			*vp = STRING_TO_JSVAL(s);
			break;
		}
		case CONN_PROP_IN_TRANSACTION:
			*vp = BOOLEAN_TO_JSVAL(p->db ? !sqlite3_get_autocommit(p->db) : false);
			break;
		case CONN_PROP_BUSY_TIMEOUT:
			*vp = INT_TO_JSVAL(p->busy_timeout_ms);
			break;
	}
	return JS_TRUE;
}

static JSBool js_sqlite_conn_set(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	sqlite_conn_private_t* p = (sqlite_conn_private_t*)JS_GetPrivate(cx, obj);
	if (p == NULL) return JS_TRUE;

	jsval idval;
	if (!JS_IdToValue(cx, id, &idval) || !JSVAL_IS_INT(idval))
		return JS_TRUE;

	switch (JSVAL_TO_INT(idval)) {
		case CONN_PROP_BUSY_TIMEOUT: {
			int32 ms = 0;
			if (!JS_ValueToInt32(cx, *vp, &ms)) return JS_FALSE;
			p->busy_timeout_ms = ms;
			if (p->db) sqlite3_busy_timeout(p->db, ms);
			break;
		}
	}
	return JS_TRUE;
}

static jsSyncMethodSpec js_sqlite_conn_functions[] = {
	{"prepare",     js_conn_prepare,     1, JSTYPE_OBJECT,
	 JSDOCSTR("sql"),
	 JSDOCSTR("Prepare an SQL statement. Returns a SQLiteStatement."),
	 32200},
	{"exec",        js_conn_exec,        1, JSTYPE_NUMBER,
	 JSDOCSTR("sql"),
	 JSDOCSTR("Execute one or more SQL statements that take no parameters. "
	          "For values, use prepare()+bind() or query()/run() instead."),
	 32200},
	{"query",       js_conn_query,       1, JSTYPE_ARRAY,
	 JSDOCSTR("sql [, params]"),
	 JSDOCSTR("One-shot SELECT: prepare, bind params, step all rows, return array of row objects. "
	          "If sql has placeholders, params is required."),
	 32200},
	{"run",         js_conn_run,         1, JSTYPE_NUMBER,
	 JSDOCSTR("sql [, params]"),
	 JSDOCSTR("One-shot write: prepare, bind params, step to completion, return changes. "
	          "If sql has placeholders, params is required."),
	 32200},
	{"transaction", js_conn_transaction, 1, JSTYPE_UNDEF,
	 JSDOCSTR("function"),
	 JSDOCSTR("Run the given function inside a BEGIN IMMEDIATE / COMMIT pair. "
	          "If the function returns normally, COMMIT is issued and the function's "
	          "return value is the return value of transaction(). If the function "
	          "throws, ROLLBACK is issued and the exception propagates out."),
	 32200},
	{"close",       js_conn_close,       0, JSTYPE_VOID,
	 JSDOCSTR(""),
	 JSDOCSTR("Close the database connection."),
	 32200},
	{"tables",      js_conn_tables,      0, JSTYPE_ARRAY,
	 JSDOCSTR(""),
	 JSDOCSTR("Return an array of user table names (excluding views and sqlite_* internals)."),
	 32200},
	{"has_table",   js_conn_has_table,   1, JSTYPE_BOOLEAN,
	 JSDOCSTR("table"),
	 JSDOCSTR("Return true if a table with the given name exists."),
	 32200},
	{"columns",     js_conn_columns,     1, JSTYPE_ARRAY,
	 JSDOCSTR("table"),
	 JSDOCSTR("Return an array of column descriptors for a table. Each descriptor has "
	          "cid (column index), name, type (declared type string), notnull (boolean), "
	          "dflt_value (default expression or null), and pk (0 or the 1-based index in "
	          "a compound primary key)."),
	 32200},
	{"backup",      js_conn_backup,      1, JSTYPE_BOOLEAN,
	 JSDOCSTR("destPath"),
	 JSDOCSTR("Atomically copy the live database to a new file using SQLite's online "
	          "backup API. Returns true on success; throws on error."),
	 32200},
	{0}
};

static JSBool js_sqlite_conn_resolve(JSContext* cx, JSObject* obj, jsid id)
{
	char*  name = NULL;
	JSBool ret;
	if (id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		JS_IdToValue(cx, id, &idval);
		if (JSVAL_IS_STRING(idval))
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
	}
	ret = js_SyncResolve(cx, obj, name, js_sqlite_conn_properties,
	    js_sqlite_conn_functions, NULL, 0);
	if (name) free(name);
	return ret;
}

static JSBool js_sqlite_conn_enumerate(JSContext* cx, JSObject* obj)
{
	return js_sqlite_conn_resolve(cx, obj, JSID_VOID);
}

JSClass js_sqlite_class = {
	"SQLite"
	, JSCLASS_HAS_PRIVATE
	, JS_PropertyStub
	, JS_PropertyStub
	, js_sqlite_conn_get
	, js_sqlite_conn_set
	, js_sqlite_conn_enumerate
	, js_sqlite_conn_resolve
	, JS_ConvertStub
	, js_finalize_conn
};

static JSBool js_sqlite_constructor(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_NewObject(cx, &js_sqlite_class, NULL, NULL);
	jsval*    argv = JS_ARGV(cx, arglist);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));

	if (argc < 1 || !JSVAL_IS_STRING(argv[0])) {
		JS_ReportError(cx, "SQLite(path [, mode]) requires a path string");
		return JS_FALSE;
	}
	char* path = NULL;
	JSVALUE_TO_MSTRING(cx, argv[0], path, NULL);
	if (path == NULL) return JS_FALSE;

	int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
	if (argc > 1 && JSVAL_IS_STRING(argv[1])) {
		char* mode = NULL;
		JSVALUE_TO_MSTRING(cx, argv[1], mode, NULL);
		if (mode != NULL) {
			flags = SQLITE_OPEN_FULLMUTEX;
			if (strcmp(mode, "r") == 0)        flags |= SQLITE_OPEN_READONLY;
			else if (strcmp(mode, "rw") == 0)  flags |= SQLITE_OPEN_READWRITE;
			else if (strcmp(mode, "rwc") == 0) flags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
			else {
				JS_ReportError(cx, "Unknown mode: %s (expected r, rw, or rwc)", mode);
				free(mode); free(path);
				return JS_FALSE;
			}
			free(mode);
		}
	}

	sqlite_conn_private_t* p = (sqlite_conn_private_t*)calloc(1, sizeof(*p));
	if (p == NULL) { JS_ReportError(cx, "calloc failed"); free(path); return JS_FALSE; }

	jsrefcount rc_susp = JS_SUSPENDREQUEST(cx);
	int rc = sqlite3_open_v2(path, &p->db, flags, NULL);
	JS_RESUMEREQUEST(cx, rc_susp);
	if (rc != SQLITE_OK) {
		JS_ReportError(cx, "SQLite: open '%s': %s (rc=%d)",
		    path, p->db ? sqlite3_errmsg(p->db) : sqlite3_errstr(rc), rc);
		if (p->db) sqlite3_close_v2(p->db);
		free(p); free(path);
		return JS_FALSE;
	}
	p->path = path;
	/* Default to 5s so concurrent processes don't see immediate SQLITE_BUSY
	 * on contention. Scripts can change it via db.busy_timeout = N. */
	p->busy_timeout_ms = 5000;
	sqlite3_busy_timeout(p->db, p->busy_timeout_ms);
	/* Enable extended result codes so the connection can report specifics
	 * like SQLITE_CONSTRAINT_UNIQUE via .extended_error_code. */
	sqlite3_extended_result_codes(p->db, 1);

	if (!JS_SetPrivate(cx, obj, p)) {
		sqlite3_close_v2(p->db);
		free(p->path); free(p);
		JS_ReportError(cx, "JS_SetPrivate failed");
		return JS_FALSE;
	}

	/* Create db.table namespace object. Rooted via being a permanent
	 * property of the SQLite connection. Cached SQLiteTables (added by
	 * the namespace's resolve hook) are in turn rooted as permanent
	 * properties of db.table, so the borrowed pointers in
	 * cp->table_cache can't dangle. */
	p->table_obj = new_table_ns_object(cx, obj);
	if (p->table_obj == NULL) {
		JS_ReportError(cx, "Failed to create db.table namespace");
		return JS_FALSE;
	}
	JS_DefineProperty(cx, obj, "table", OBJECT_TO_JSVAL(p->table_obj),
	    NULL, NULL, JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY);

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj,
	    "SQLite database connection. Use prepare()+bind() (or query()/run() with a "
	    "params arg) to pass values into SQL safely; never concatenate values into "
	    "SQL strings.", 32200);
	js_DescribeSyncConstructor(cx, obj,
	    "To open a database: <tt>var db = new SQLite(<i>path</i> [, <i>mode</i>])</tt>. "
	    "Mode is \"r\", \"rw\", or \"rwc\" (default).");
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list",
	    sqlite_conn_prop_desc, JSPROP_READONLY);
#endif

	return JS_TRUE;
}

/* Routable log handler.
 *
 *   g_sqlite_log.fn = JSVAL_VOID  => default: route to lprintf().
 *   g_sqlite_log.fn = JSVAL_NULL  => silence; drop messages on the floor.
 *   g_sqlite_log.fn = function    => JS callback invoked as fn(rc, msg)
 *                                    on the thread that installed it.
 *
 * SQLite's log is process-global; it can fire from any thread. JS can
 * only safely run on its own context's thread, so we record the
 * installer's pthread_t and drop log events that arrive from any other
 * thread (uncommon: WAL recovery, async checkpointing). The lprintf
 * default path is thread-safe and handles all threads cleanly. */
static struct {
	JSContext* cx;
	pthread_t  tid;
	jsval      fn;
	bool       rooted;
} g_sqlite_log = { NULL, 0, JSVAL_VOID, false };

static void sqlite_log_cb(void* user, int rc, const char* msg)
{
	(void)user;
	jsval fn = g_sqlite_log.fn;
	if (JSVAL_IS_VOID(fn)) {
		int level;
		switch (rc & 0xFF) {
			case SQLITE_NOTICE:  level = LOG_NOTICE;  break;
			case SQLITE_WARNING: level = LOG_WARNING; break;
			default:             level = LOG_ERR;     break;
		}
		lprintf(level, "sqlite (rc=%d): %s", rc, msg ? msg : "");
		return;
	}
	if (JSVAL_IS_NULL(fn)) return;  /* silenced */
	if (!pthread_equal(g_sqlite_log.tid, pthread_self())) return;

	JSContext* cx = g_sqlite_log.cx;
	jsval args[2];
	args[0] = INT_TO_JSVAL(rc);
	JSString* s = JS_NewStringCopyZ(cx, msg ? msg : "");
	if (s == NULL) { JS_ClearPendingException(cx); return; }
	args[1] = STRING_TO_JSVAL(s);
	jsval rval;
	if (!JS_CallFunctionValue(cx, JS_GetGlobalObject(cx), fn, 2, args, &rval))
		JS_ClearPendingException(cx);
}

/* Static method: SQLite.set_log(fn|null|undefined) - install or clear a
 * handler for SQLite's process-global log stream. Returns the previous
 * handler (or undefined if the default lprintf route was active), so
 * tests / hot-path code can save and restore around expected-error
 * regions:
 *
 *   var prev = SQLite.set_log(null);
 *   try { ... expected error ... } catch (e) {}
 *   SQLite.set_log(prev);
 *
 * Callback signature: fn(rc, msg). The handler runs only on the thread
 * that installed it; messages SQLite raises on other threads are
 * dropped (only the lprintf default reliably handles cross-thread). */
static JSBool js_sqlite_static_set_log(JSContext* cx, uintN argc, jsval* arglist)
{
	jsval*    argv = JS_ARGV(cx, arglist);
	jsval     prev = g_sqlite_log.fn;
	jsval     in   = (argc < 1) ? JSVAL_VOID : argv[0];

	if (!JSVAL_IS_VOID(in) && !JSVAL_IS_NULL(in)) {
		if (!JSVAL_IS_OBJECT(in)
		    || !JS_ObjectIsFunction(cx, JSVAL_TO_OBJECT(in))) {
			JS_ReportError(cx, "SQLite.set_log: argument must be a function, "
			    "null (silence), or undefined (default)");
			return JS_FALSE;
		}
	}

	if (g_sqlite_log.rooted) {
		JS_RemoveValueRoot(cx, &g_sqlite_log.fn);
		g_sqlite_log.rooted = false;
	}
	g_sqlite_log.fn = in;
	if (JSVAL_IS_OBJECT(in) && !JSVAL_IS_NULL(in)) {
		g_sqlite_log.cx  = cx;
		g_sqlite_log.tid = pthread_self();
		if (!JS_AddNamedValueRoot(cx, &g_sqlite_log.fn, "sqlite_log_cb_fn")) {
			g_sqlite_log.fn = JSVAL_VOID;
			JS_ReportError(cx, "JS_AddNamedValueRoot failed");
			return JS_FALSE;
		}
		g_sqlite_log.rooted = true;
	} else {
		g_sqlite_log.cx = NULL;
	}

	JS_SET_RVAL(cx, arglist, prev);
	return JS_TRUE;
}

/* Static method: SQLite.escape_like(string, [escape_char]) - escapes the
 * LIKE/GLOB metacharacters % and _ (and the escape char itself) so the
 * result can be used as a literal substring inside a LIKE/GLOB pattern.
 * Pair with an ESCAPE clause in the SQL. ONLY for LIKE/GLOB patterns;
 * regular bound parameters need no escaping at all. */
static JSBool js_sqlite_static_escape_like(JSContext* cx, uintN argc, jsval* arglist)
{
	jsval* argv = JS_ARGV(cx, arglist);
	if (argc < 1 || !JSVAL_IS_STRING(argv[0])) {
		JS_ReportError(cx, "escape_like(string [, escape_char]) requires a string");
		return JS_FALSE;
	}
	char esc = '\\';
	if (argc > 1 && !JSVAL_IS_VOID(argv[1])) {
		char* e = NULL;
		JSVALUE_TO_MSTRING(cx, argv[1], e, NULL);
		if (e == NULL) return JS_FALSE;
		if (strlen(e) != 1) {
			JS_ReportError(cx, "escape_char must be a single character");
			free(e);
			return JS_FALSE;
		}
		esc = e[0];
		free(e);
	}
	char* s = NULL;
	size_t len = 0;
	JSVALUE_TO_MSTRING(cx, argv[0], s, &len);
	if (s == NULL) return JS_FALSE;

	/* Worst case is len*2: every byte is a metacharacter. */
	char* out = (char*)malloc(len * 2 + 1);
	if (out == NULL) { free(s); JS_ReportError(cx, "malloc failed"); return JS_FALSE; }
	size_t out_len = 0;
	for (size_t i = 0; i < len; i++) {
		if (s[i] == '%' || s[i] == '_' || s[i] == esc)
			out[out_len++] = esc;
		out[out_len++] = s[i];
	}
	out[out_len] = '\0';
	free(s);

	JSString* js = JS_NewStringCopyN(cx, out, out_len);
	free(out);
	if (js == NULL) return JS_FALSE;
	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js));
	return JS_TRUE;
}

/* Static method: SQLite.errstr(code) - returns the English description of
 * any SQLite result code (primary or extended). Useful when scripts have
 * a code but no connection handle. */
static JSBool js_sqlite_static_errstr(JSContext* cx, uintN argc, jsval* arglist)
{
	jsval* argv = JS_ARGV(cx, arglist);
	if (argc < 1) {
		JS_ReportError(cx, "SQLite.errstr(code) requires a result code");
		return JS_FALSE;
	}
	int32 code = 0;
	if (!JS_ValueToInt32(cx, argv[0], &code)) return JS_FALSE;
	JSString* s = JS_NewStringCopyZ(cx, sqlite3_errstr(code));
	if (s == NULL) return JS_FALSE;
	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(s));
	return JS_TRUE;
}

static JSBool js_no_construct(JSContext* cx, uintN argc, jsval* arglist)
{
	JS_ReportError(cx, "This class is not directly constructible; obtain instances via SQLite methods.");
	return JS_FALSE;
}

#ifdef BUILD_JSDOCS
/* Descriptions for the static integer constants on the SQLite constructor,
 * in the same order they're added via JS_DefineProperty below. jsdocs reads
 * these via the constructor's _property_desc_list. */
static const char* sqlite_const_desc[] = {
	  "Result code: success"
	, "Result code from step(): a row is available"
	, "Result code from step(): no more rows"
	, "Result code: database is locked by another process (see busy_timeout)"
	, "Result code: database is locked by another connection in the same process"
	, "Result code: attempt to write to a read-only database"
	, "Primary result code: a constraint was violated (see CONSTRAINT_* for specifics)"
	, "Result code: data type mismatch"
	, "Storage class / column type code: INTEGER"
	, "Storage class / column type code: REAL (floating point)"
	, "Storage class / column type code: TEXT"
	, "Storage class / column type code: BLOB"
	, "Storage class / column type code: NULL"
	, "Extended result code: CHECK constraint failed"
	, "Extended result code: FOREIGN KEY constraint failed"
	, "Extended result code: NOT NULL constraint failed"
	, "Extended result code: PRIMARY KEY constraint failed"
	, "Extended result code: UNIQUE constraint failed"
	, NULL
};
#endif

/* sqlite_log_cb / g_sqlite_log moved above the static methods so
 * SQLite.set_log() can reference the state struct. */

static pthread_once_t sqlite_log_once = PTHREAD_ONCE_INIT;
static void sqlite_log_install(void)
{
	sqlite3_config(SQLITE_CONFIG_LOG, sqlite_log_cb, (void*)NULL);
}

/* Static methods on the SQLite constructor. Defined via the standard
 * jsSyncMethodSpec mechanism so jsdocs picks up the descriptions. */
static jsSyncMethodSpec js_sqlite_static_functions[] = {
	{"errstr",      js_sqlite_static_errstr,      1, JSTYPE_STRING,
	 JSDOCSTR("code"),
	 JSDOCSTR("Return the English description of any SQLite result code (primary or extended). "
	          "Useful when you have a code but no connection handle, or to render a code from "
	          "db.error_code for logging."),
	 32200},
	{"escape_like", js_sqlite_static_escape_like, 1, JSTYPE_STRING,
	 JSDOCSTR("string [, escape_char]"),
	 JSDOCSTR("Escape the LIKE / GLOB metacharacters % and _ (and the escape char itself) "
	          "so the result can be used as a literal fragment inside a LIKE or GLOB pattern. "
	          "Pair with an ESCAPE clause in the SQL. <b>Use ONLY for LIKE/GLOB patterns</b>; "
	          "regular bound parameters in =, INSERT VALUES, etc. need no escaping at all "
	          "(binding already handles them). Misapplying this function will store literal "
	          "backslashes in your data."),
	 32200},
	{"set_log",     js_sqlite_static_set_log,     1, JSTYPE_UNDEF,
	 JSDOCSTR("fn | null | undefined"),
	 JSDOCSTR("Install a handler for SQLite's process-global log stream. "
	          "Pass a function to receive <tt>fn(rc, msg)</tt>; pass <tt>null</tt> "
	          "to silence the log entirely; pass <tt>undefined</tt> (or no argument) "
	          "to restore the default lprintf routing. Returns the previously installed "
	          "handler so it can be saved and restored around an expected-error region. "
	          "Custom JS callbacks fire only on the thread that installed them; messages "
	          "raised from other threads (rare: WAL recovery, async checkpointing) are "
	          "dropped — use the default lprintf path if you need to capture those."),
	 32200},
	{0}
};

/* =====================================================================
 * SQLiteTable, SQLiteRecord, and the db.table namespace
 *
 * The schema for each table is loaded lazily on first access (PRAGMA
 * table_info + foreign_key_list, both as bound parameters not string
 * interpolation), cached on the connection's private struct, and
 * rooted as a JSPROP_PERMANENT property on the db.table namespace so
 * the borrowed JSObject* in the lookup chain can't dangle.
 * ===================================================================== */

static char* str_to_lower_dup(const char* s)
{
	if (s == NULL) return NULL;
	size_t n = strlen(s);
	char*  out = (char*)malloc(n + 1);
	if (out == NULL) return NULL;
	for (size_t i = 0; i < n; i++) {
		char c = s[i];
		if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
		out[i] = c;
	}
	out[n] = '\0';
	return out;
}

static void table_free_schema(sqlite_table_private_t* tp)
{
	if (tp == NULL) return;
	if (tp->name) { free(tp->name); tp->name = NULL; }
	if (tp->cols) {
		for (int i = 0; i < tp->ncols; i++) {
			free(tp->cols[i].name);
			free(tp->cols[i].type);
			free(tp->cols[i].dflt_value);
		}
		free(tp->cols);
		tp->cols = NULL;
		tp->ncols = 0;
	}
	if (tp->pk_cols) { free(tp->pk_cols); tp->pk_cols = NULL; tp->npk = 0; }
	if (tp->fks) {
		for (int i = 0; i < tp->nfks; i++) {
			free(tp->fks[i].to_table);
			if (tp->fks[i].from_col) {
				for (int j = 0; j < tp->fks[i].ncol; j++)
					free(tp->fks[i].from_col[j]);
				free(tp->fks[i].from_col);
			}
			if (tp->fks[i].to_col) {
				for (int j = 0; j < tp->fks[i].ncol; j++)
					free(tp->fks[i].to_col[j]);
				free(tp->fks[i].to_col);
			}
		}
		free(tp->fks);
		tp->fks = NULL;
		tp->nfks = 0;
	}
}

/* Loads schema info into `out`. Returns JS_TRUE with out->name == NULL
 * when the table simply doesn't exist (caller treats as "undefined"),
 * JS_TRUE with out->name set on success, JS_FALSE on JS-reportable
 * error. */
static JSBool table_load_schema(JSContext* cx, sqlite_conn_private_t* cp,
                                const char* req_name, sqlite_table_private_t* out)
{
	if (cp->db == NULL) {
		JS_ReportError(cx, "Connection is closed");
		return JS_FALSE;
	}

	/* Canonicalize name + verify table exists. */
	const char* sql = "SELECT name FROM sqlite_master WHERE type='table' "
	                  "AND name=? COLLATE NOCASE";
	sqlite3_stmt* stmt = NULL;
	jsrefcount susp = JS_SUSPENDREQUEST(cx);
	int rc = sqlite3_prepare_v2(cp->db, sql, -1, &stmt, NULL);
	JS_RESUMEREQUEST(cx, susp);
	if (rc != SQLITE_OK) return sqlite_throw(cx, cp->db, rc, "schema/sqlite_master prepare");
	rc = sqlite3_bind_text(stmt, 1, req_name, -1, SQLITE_TRANSIENT);
	if (rc != SQLITE_OK) {
		JSBool b = sqlite_throw(cx, cp->db, rc, "schema/sqlite_master bind");
		sqlite3_finalize(stmt);
		return b;
	}
	susp = JS_SUSPENDREQUEST(cx);
	rc = sqlite3_step(stmt);
	JS_RESUMEREQUEST(cx, susp);
	if (rc == SQLITE_DONE) {
		sqlite3_finalize(stmt);
		return JS_TRUE;  /* out->name stays NULL = "doesn't exist" */
	}
	if (rc != SQLITE_ROW) {
		JSBool b = sqlite_throw(cx, cp->db, rc, "schema/sqlite_master step");
		sqlite3_finalize(stmt);
		return b;
	}
	const char* cn = (const char*)sqlite3_column_text(stmt, 0);
	out->name = strdup(cn ? cn : req_name);
	sqlite3_finalize(stmt);
	if (out->name == NULL) { JS_ReportError(cx, "strdup failed"); return JS_FALSE; }

	/* Columns via pragma_table_info. */
	const char* csql = "SELECT cid, name, type, \"notnull\", dflt_value, pk "
	                   "FROM pragma_table_info(?) ORDER BY cid";
	susp = JS_SUSPENDREQUEST(cx);
	rc = sqlite3_prepare_v2(cp->db, csql, -1, &stmt, NULL);
	JS_RESUMEREQUEST(cx, susp);
	if (rc != SQLITE_OK) return sqlite_throw(cx, cp->db, rc, "schema/table_info prepare");
	rc = sqlite3_bind_text(stmt, 1, out->name, -1, SQLITE_TRANSIENT);
	if (rc != SQLITE_OK) {
		JSBool b = sqlite_throw(cx, cp->db, rc, "schema/table_info bind");
		sqlite3_finalize(stmt);
		return b;
	}

	int cap = 8;
	out->cols = (sqlite_table_col_t*)calloc(cap, sizeof(*out->cols));
	if (out->cols == NULL) { sqlite3_finalize(stmt); JS_ReportError(cx, "calloc failed"); return JS_FALSE; }
	for (;;) {
		susp = JS_SUSPENDREQUEST(cx);
		rc = sqlite3_step(stmt);
		JS_RESUMEREQUEST(cx, susp);
		if (rc == SQLITE_DONE) break;
		if (rc != SQLITE_ROW) {
			JSBool b = sqlite_throw(cx, cp->db, rc, "schema/table_info step");
			sqlite3_finalize(stmt);
			return b;
		}
		if (out->ncols >= cap) {
			cap *= 2;
			sqlite_table_col_t* n2 = (sqlite_table_col_t*)
			    realloc(out->cols, cap * sizeof(*out->cols));
			if (n2 == NULL) { sqlite3_finalize(stmt); JS_ReportError(cx, "realloc failed"); return JS_FALSE; }
			memset(n2 + out->ncols, 0, (cap - out->ncols) * sizeof(*out->cols));
			out->cols = n2;
		}
		sqlite_table_col_t* c = &out->cols[out->ncols];
		const char* s;
		s = (const char*)sqlite3_column_text(stmt, 1);
		c->name = strdup(s ? s : "");
		s = (const char*)sqlite3_column_text(stmt, 2);
		c->type = strdup(s ? s : "");
		c->notnull = sqlite3_column_int(stmt, 3) != 0;
		if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
			s = (const char*)sqlite3_column_text(stmt, 4);
			c->dflt_value = strdup(s ? s : "");
			if (c->dflt_value == NULL) { sqlite3_finalize(stmt); JS_ReportError(cx, "strdup failed"); return JS_FALSE; }
		}
		c->pk = sqlite3_column_int(stmt, 5);
		if (c->name == NULL || c->type == NULL) {
			sqlite3_finalize(stmt); JS_ReportError(cx, "strdup failed"); return JS_FALSE;
		}
		out->ncols++;
	}
	sqlite3_finalize(stmt);

	/* PK column indices in pk-position order. */
	out->npk = 0;
	for (int i = 0; i < out->ncols; i++)
		if (out->cols[i].pk > 0) out->npk++;
	if (out->npk > 0) {
		out->pk_cols = (int*)calloc(out->npk, sizeof(int));
		if (out->pk_cols == NULL) { JS_ReportError(cx, "calloc failed"); return JS_FALSE; }
		for (int i = 0; i < out->ncols; i++) {
			int p = out->cols[i].pk;
			if (p > 0 && p <= out->npk) out->pk_cols[p - 1] = i;
		}
	}

	/* Foreign keys via pragma_foreign_key_list, grouped by id. */
	const char* fsql = "SELECT id, seq, \"table\", \"from\", \"to\" "
	                   "FROM pragma_foreign_key_list(?) ORDER BY id, seq";
	susp = JS_SUSPENDREQUEST(cx);
	rc = sqlite3_prepare_v2(cp->db, fsql, -1, &stmt, NULL);
	JS_RESUMEREQUEST(cx, susp);
	if (rc != SQLITE_OK) return sqlite_throw(cx, cp->db, rc, "schema/foreign_key_list prepare");
	rc = sqlite3_bind_text(stmt, 1, out->name, -1, SQLITE_TRANSIENT);
	if (rc != SQLITE_OK) {
		JSBool b = sqlite_throw(cx, cp->db, rc, "schema/foreign_key_list bind");
		sqlite3_finalize(stmt);
		return b;
	}

	struct fk_row { int id; int seq; char* table; char* from; char* to; };
	int fcap = 4, fn = 0;
	struct fk_row* rows = (struct fk_row*)calloc(fcap, sizeof(*rows));
	if (rows == NULL) { sqlite3_finalize(stmt); JS_ReportError(cx, "calloc failed"); return JS_FALSE; }
	JSBool ok = JS_TRUE;
	for (;;) {
		susp = JS_SUSPENDREQUEST(cx);
		rc = sqlite3_step(stmt);
		JS_RESUMEREQUEST(cx, susp);
		if (rc == SQLITE_DONE) break;
		if (rc != SQLITE_ROW) {
			ok = sqlite_throw(cx, cp->db, rc, "schema/foreign_key_list step");
			break;
		}
		if (fn >= fcap) {
			fcap *= 2;
			struct fk_row* n2 = (struct fk_row*)realloc(rows, fcap * sizeof(*rows));
			if (n2 == NULL) { JS_ReportError(cx, "realloc failed"); ok = JS_FALSE; break; }
			memset(n2 + fn, 0, (fcap - fn) * sizeof(*rows));
			rows = n2;
		}
		rows[fn].id = sqlite3_column_int(stmt, 0);
		rows[fn].seq = sqlite3_column_int(stmt, 1);
		const char* s;
		s = (const char*)sqlite3_column_text(stmt, 2);
		rows[fn].table = strdup(s ? s : "");
		s = (const char*)sqlite3_column_text(stmt, 3);
		rows[fn].from = strdup(s ? s : "");
		s = (const char*)sqlite3_column_text(stmt, 4);
		rows[fn].to = strdup(s ? s : "");
		if (!rows[fn].table || !rows[fn].from || !rows[fn].to) {
			JS_ReportError(cx, "strdup failed");
			ok = JS_FALSE;
			fn++;
			break;
		}
		fn++;
	}
	sqlite3_finalize(stmt);
	if (ok && fn > 0) {
		int distinct_ids = 1;
		for (int i = 1; i < fn; i++) if (rows[i].id != rows[i-1].id) distinct_ids++;
		out->fks = (sqlite_table_fk_t*)calloc(distinct_ids, sizeof(*out->fks));
		if (out->fks == NULL) { JS_ReportError(cx, "calloc failed"); ok = JS_FALSE; }
		else {
			int fi = 0;
			int start = 0;
			for (int i = 1; i <= fn && ok; i++) {
				if (i == fn || rows[i].id != rows[start].id) {
					int n = i - start;
					out->fks[fi].ncol = n;
					out->fks[fi].to_table = strdup(rows[start].table);
					out->fks[fi].from_col = (char**)calloc(n, sizeof(char*));
					out->fks[fi].to_col   = (char**)calloc(n, sizeof(char*));
					if (!out->fks[fi].to_table || !out->fks[fi].from_col || !out->fks[fi].to_col) {
						JS_ReportError(cx, "alloc failed");
						ok = JS_FALSE;
						break;
					}
					for (int j = 0; j < n; j++) {
						out->fks[fi].from_col[j] = strdup(rows[start + j].from);
						out->fks[fi].to_col[j]   = strdup(rows[start + j].to);
						if (!out->fks[fi].from_col[j] || !out->fks[fi].to_col[j]) {
							JS_ReportError(cx, "strdup failed");
							ok = JS_FALSE;
							break;
						}
					}
					fi++;
					start = i;
				}
			}
			out->nfks = fi;
		}
	}
	for (int i = 0; i < fn; i++) {
		free(rows[i].table);
		free(rows[i].from);
		free(rows[i].to);
	}
	free(rows);
	return ok;
}

/* ---------- SQLiteTable ---------- */

static void js_finalize_table(JSContext* cx, JSObject* obj)
{
	sqlite_table_private_t* p = (sqlite_table_private_t*)JS_GetPrivate(cx, obj);
	if (p == NULL) return;
	table_free_schema(p);
	free(p);
	JS_SetPrivate(cx, obj, NULL);
}

enum {
	TABLE_PROP_NAME         = -1,
	TABLE_PROP_COLUMNS      = -2,
	TABLE_PROP_PK           = -3,
	TABLE_PROP_FOREIGN_KEYS = -4,
};

#define TABLE_PROP_FLAGS (JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)

static jsSyncPropertySpec js_sqlite_table_properties[] = {
	{"name",         TABLE_PROP_NAME,         TABLE_PROP_FLAGS, 32200},
	{"columns",      TABLE_PROP_COLUMNS,      TABLE_PROP_FLAGS, 32200},
	{"pk",           TABLE_PROP_PK,           TABLE_PROP_FLAGS, 32200},
	{"foreign_keys", TABLE_PROP_FOREIGN_KEYS, TABLE_PROP_FLAGS, 32200},
	{0}
};

#ifdef BUILD_JSDOCS
static const char* sqlite_table_prop_desc[] = {
	  "Canonical table name as recorded in the schema"
	, "Array of column descriptors: cid, name, type, notnull, dflt_value, pk (same shape as SQLite.columns())"
	, "Array of primary-key column names, in PK position order (empty for a table with no PK)"
	, "Array of foreign-key descriptors: { ncol, from_col[], to_table, to_col[] }. Composite FKs have ncol > 1"
	, NULL
};
#endif

static JSBool js_sqlite_table_get(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	sqlite_table_private_t* p = (sqlite_table_private_t*)JS_GetPrivate(cx, obj);
	if (p == NULL) return JS_TRUE;

	jsval idval;
	if (!JS_IdToValue(cx, id, &idval) || !JSVAL_IS_INT(idval))
		return JS_TRUE;

	switch (JSVAL_TO_INT(idval)) {
		case TABLE_PROP_NAME: {
			JSString* s = JS_NewStringCopyZ(cx, p->name);
			if (s == NULL) return JS_FALSE;
			*vp = STRING_TO_JSVAL(s);
			return JS_TRUE;
		}
		case TABLE_PROP_COLUMNS: {
			JSObject* arr = JS_NewArrayObject(cx, 0, NULL);
			if (arr == NULL) return JS_FALSE;
			for (int i = 0; i < p->ncols; i++) {
				JSObject* o = JS_NewObject(cx, NULL, NULL, NULL);
				if (o == NULL) return JS_FALSE;
				jsval v;
				v = INT_TO_JSVAL(i);
				JS_DefineProperty(cx, o, "cid", v, NULL, NULL, JSPROP_ENUMERATE);
				JSString* s = JS_NewStringCopyZ(cx, p->cols[i].name);
				if (s == NULL) return JS_FALSE;
				v = STRING_TO_JSVAL(s);
				JS_DefineProperty(cx, o, "name", v, NULL, NULL, JSPROP_ENUMERATE);
				s = JS_NewStringCopyZ(cx, p->cols[i].type);
				if (s == NULL) return JS_FALSE;
				v = STRING_TO_JSVAL(s);
				JS_DefineProperty(cx, o, "type", v, NULL, NULL, JSPROP_ENUMERATE);
				v = BOOLEAN_TO_JSVAL(p->cols[i].notnull);
				JS_DefineProperty(cx, o, "notnull", v, NULL, NULL, JSPROP_ENUMERATE);
				if (p->cols[i].dflt_value) {
					s = JS_NewStringCopyZ(cx, p->cols[i].dflt_value);
					if (s == NULL) return JS_FALSE;
					v = STRING_TO_JSVAL(s);
				} else {
					v = JSVAL_NULL;
				}
				JS_DefineProperty(cx, o, "dflt_value", v, NULL, NULL, JSPROP_ENUMERATE);
				v = INT_TO_JSVAL(p->cols[i].pk);
				JS_DefineProperty(cx, o, "pk", v, NULL, NULL, JSPROP_ENUMERATE);
				v = OBJECT_TO_JSVAL(o);
				JS_SetElement(cx, arr, i, &v);
			}
			*vp = OBJECT_TO_JSVAL(arr);
			return JS_TRUE;
		}
		case TABLE_PROP_PK: {
			JSObject* arr = JS_NewArrayObject(cx, p->npk, NULL);
			if (arr == NULL) return JS_FALSE;
			for (int i = 0; i < p->npk; i++) {
				JSString* s = JS_NewStringCopyZ(cx, p->cols[p->pk_cols[i]].name);
				if (s == NULL) return JS_FALSE;
				jsval v = STRING_TO_JSVAL(s);
				JS_SetElement(cx, arr, i, &v);
			}
			*vp = OBJECT_TO_JSVAL(arr);
			return JS_TRUE;
		}
		case TABLE_PROP_FOREIGN_KEYS: {
			JSObject* arr = JS_NewArrayObject(cx, p->nfks, NULL);
			if (arr == NULL) return JS_FALSE;
			for (int i = 0; i < p->nfks; i++) {
				JSObject* o = JS_NewObject(cx, NULL, NULL, NULL);
				if (o == NULL) return JS_FALSE;
				jsval v;
				v = INT_TO_JSVAL(p->fks[i].ncol);
				JS_DefineProperty(cx, o, "ncol", v, NULL, NULL, JSPROP_ENUMERATE);
				JSString* s = JS_NewStringCopyZ(cx, p->fks[i].to_table);
				if (s == NULL) return JS_FALSE;
				v = STRING_TO_JSVAL(s);
				JS_DefineProperty(cx, o, "to_table", v, NULL, NULL, JSPROP_ENUMERATE);
				JSObject* fa = JS_NewArrayObject(cx, p->fks[i].ncol, NULL);
				JSObject* ta = JS_NewArrayObject(cx, p->fks[i].ncol, NULL);
				if (fa == NULL || ta == NULL) return JS_FALSE;
				for (int j = 0; j < p->fks[i].ncol; j++) {
					JSString* fs = JS_NewStringCopyZ(cx, p->fks[i].from_col[j]);
					JSString* ts = JS_NewStringCopyZ(cx, p->fks[i].to_col[j]);
					if (fs == NULL || ts == NULL) return JS_FALSE;
					jsval fv = STRING_TO_JSVAL(fs);
					jsval tv = STRING_TO_JSVAL(ts);
					JS_SetElement(cx, fa, j, &fv);
					JS_SetElement(cx, ta, j, &tv);
				}
				v = OBJECT_TO_JSVAL(fa);
				JS_DefineProperty(cx, o, "from_col", v, NULL, NULL, JSPROP_ENUMERATE);
				v = OBJECT_TO_JSVAL(ta);
				JS_DefineProperty(cx, o, "to_col", v, NULL, NULL, JSPROP_ENUMERATE);
				v = OBJECT_TO_JSVAL(o);
				JS_SetElement(cx, arr, i, &v);
			}
			*vp = OBJECT_TO_JSVAL(arr);
			return JS_TRUE;
		}
	}
	return JS_TRUE;
}

static JSBool js_table_new_row(JSContext* cx, uintN argc, jsval* arglist);
static JSBool js_table_row    (JSContext* cx, uintN argc, jsval* arglist);
static JSBool js_table_select (JSContext* cx, uintN argc, jsval* arglist);

static jsSyncMethodSpec js_sqlite_table_functions[] = {
	{"new_row", js_table_new_row, 0, JSTYPE_OBJECT,
	 JSDOCSTR(""),
	 JSDOCSTR("Return a new SQLiteRecord for this table with each column's "
	          "declared <tt>DEFAULT</tt> expression evaluated and stored on "
	          "the record. Columns with no default are left undefined. The "
	          "record is not yet persisted; call <tt>r.$insert()</tt> to "
	          "write it. <tt>CURRENT_TIMESTAMP</tt> and similar are "
	          "re-evaluated on every call."),
	 32200},
	{"row",     js_table_row,     1, JSTYPE_OBJECT,
	 JSDOCSTR("pk [, pk2, ...]"),
	 JSDOCSTR("Load and return the SQLiteRecord whose primary key matches the "
	          "given value(s), or <tt>null</tt> if no such row exists. "
	          "Composite-PK tables expect one argument per PK column, in PK "
	          "position order. Original PK values are captured so a subsequent "
	          "<tt>update()</tt> or <tt>remove()</tt> finds the row correctly "
	          "even if the script changes a PK column."),
	 32200},
	{"select",  js_table_select,  1, JSTYPE_ARRAY,
	 JSDOCSTR("where_sql [, params]"),
	 JSDOCSTR("Return an array of SQLiteRecord rows matching the WHERE clause. "
	          "<tt>where_sql</tt> is a string SQL fragment without the leading "
	          "<i>WHERE</i> keyword, or <tt>null</tt> for all rows. If the "
	          "fragment contains placeholders (<tt>?</tt>, <tt>:name</tt>, "
	          "<tt>@name</tt>, <tt>$name</tt>), <tt>params</tt> is required "
	          "(array for positional, object for named)."),
	 32200},
	{0}
};

static JSBool js_sqlite_table_resolve(JSContext* cx, JSObject* obj, jsid id)
{
	char*  name = NULL;
	JSBool ret;
	if (id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		JS_IdToValue(cx, id, &idval);
		if (JSVAL_IS_STRING(idval))
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
	}
	ret = js_SyncResolve(cx, obj, name, js_sqlite_table_properties,
	    js_sqlite_table_functions, NULL, 0);
	if (name) free(name);
	return ret;
}

static JSBool js_sqlite_table_enumerate(JSContext* cx, JSObject* obj)
{
	return js_sqlite_table_resolve(cx, obj, JSID_VOID);
}

JSClass js_sqlite_table_class = {
	"SQLiteTable"
	, JSCLASS_HAS_PRIVATE
	, JS_PropertyStub
	, JS_PropertyStub
	, js_sqlite_table_get
	, JS_StrictPropertyStub
	, js_sqlite_table_enumerate
	, js_sqlite_table_resolve
	, JS_ConvertStub
	, js_finalize_table
};

static JSObject* new_table_object(JSContext* cx, JSObject* conn_obj,
                                  sqlite_conn_private_t* cp, const char* req_name)
{
	sqlite_table_private_t* tp = (sqlite_table_private_t*)calloc(1, sizeof(*tp));
	if (tp == NULL) { JS_ReportError(cx, "calloc failed"); return NULL; }
	tp->conn = cp;
	if (!table_load_schema(cx, cp, req_name, tp)) {
		table_free_schema(tp);
		free(tp);
		return NULL;
	}
	if (tp->name == NULL) {
		/* table doesn't exist; signal to caller via NULL with no JS error */
		table_free_schema(tp);
		free(tp);
		return NULL;
	}
	JSObject* obj = JS_NewObject(cx, &js_sqlite_table_class, NULL, NULL);
	if (obj == NULL) { table_free_schema(tp); free(tp); return NULL; }
	if (!JS_SetPrivate(cx, obj, tp)) {
		table_free_schema(tp);
		free(tp);
		JS_ReportError(cx, "JS_SetPrivate failed");
		return NULL;
	}
	JS_DefineProperty(cx, obj, ROOT_PROP_NAME, OBJECT_TO_JSVAL(conn_obj),
	    NULL, NULL, JSPROP_PERMANENT | JSPROP_READONLY);
#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj,
	    "Table-scoped accessor obtained via <tt>db.table.<i>name</i></tt>. "
	    "Schema (columns, primary key, foreign keys) is loaded at first "
	    "access and cached for the connection's lifetime. Use the methods "
	    "below to construct, fetch, and query SQLiteRecord instances.", 32200);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list",
	    sqlite_table_prop_desc, JSPROP_READONLY);
#endif
	return obj;
}

/* ---------- SQLiteRecord ---------- */

/* Hidden property names that root the GC-traced arrays held in the
 * record's private struct. */
#define RECORD_DATA_PROP    "__data"
#define RECORD_ORIG_PK_PROP "__orig_pk"

static void js_finalize_record(JSContext* cx, JSObject* obj)
{
	sqlite_record_private_t* p = (sqlite_record_private_t*)JS_GetPrivate(cx, obj);
	if (p == NULL) return;
	free(p);
	JS_SetPrivate(cx, obj, NULL);
}

/* -1 if not a column of this table. */
static int record_col_index(sqlite_table_private_t* tp, const char* name)
{
	for (int i = 0; i < tp->ncols; i++) {
		if (strcmp(tp->cols[i].name, name) == 0) return i;
	}
	return -1;
}

/* Returns the index of the FK in tp->fks whose single from-column matches
 * `name`. Sets *is_composite to true if a multi-column FK was found.
 * Returns -1 if no matching FK at all. */
static int record_fk_index(sqlite_table_private_t* tp, const char* name, bool* is_composite)
{
	*is_composite = false;
	for (int i = 0; i < tp->nfks; i++) {
		if (tp->fks[i].ncol == 1 && strcmp(tp->fks[i].from_col[0], name) == 0)
			return i;
	}
	/* Also check composite FKs so we can throw a clear error. */
	for (int i = 0; i < tp->nfks; i++) {
		if (tp->fks[i].ncol > 1) {
			for (int j = 0; j < tp->fks[i].ncol; j++) {
				if (strcmp(tp->fks[i].from_col[j], name) == 0) {
					*is_composite = true;
					return i;
				}
			}
		}
	}
	return -1;
}

static JSBool js_sqlite_record_get(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	sqlite_record_private_t* p = (sqlite_record_private_t*)JS_GetPrivate(cx, obj);
	if (p == NULL) return JS_TRUE;

	jsval idval;
	if (!JS_IdToValue(cx, id, &idval) || !JSVAL_IS_STRING(idval))
		return JS_TRUE;

	char* name = NULL;
	JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
	if (name == NULL) return JS_TRUE;

	/* Skip hidden/internal property names; let normal lookup find them. */
	if (name[0] == '_' || name[0] == '$') {
		free(name);
		return JS_TRUE;
	}

	/* Column read. */
	int col = record_col_index(p->table, name);
	if (col >= 0) {
		free(name);
		return JS_GetElement(cx, p->data_arr, col, vp);
	}

	/* @<col>: lazily fetch the referenced row via a single-column FK
	 * declared on this table. Each access is a fresh fetch — assign to
	 * a local if you want a stable reference. Composite FKs throw with
	 * a clear message rather than silently doing the wrong thing. */
	if (name[0] == '@' && name[1] != '\0') {
		const char* col_name = name + 1;
		bool is_composite = false;
		int fk = record_fk_index(p->table, col_name, &is_composite);
		if (fk < 0) {
			/* No FK at all on this column. Leave *vp as JSVAL_VOID so
			 * the lookup falls through to the prototype (won't find
			 * anything; the user sees undefined). */
			free(name);
			return JS_TRUE;
		}
		if (is_composite) {
			JS_ReportError(cx, "SQLiteRecord: column '%s' is part of a composite "
			    "foreign key; @col cannot resolve it (use a manual join via db.query)",
			    col_name);
			free(name);
			return JS_FALSE;
		}
		/* Single-column FK. Look up the from-col's current value; if it's
		 * undefined or null, the join is null. */
		int from_idx = record_col_index(p->table, p->table->fks[fk].from_col[0]);
		if (from_idx < 0) { free(name); return JS_TRUE; }
		jsval fk_val;
		if (!JS_GetElement(cx, p->data_arr, from_idx, &fk_val)) {
			free(name);
			return JS_FALSE;
		}
		if (JSVAL_IS_VOID(fk_val) || JSVAL_IS_NULL(fk_val)) {
			free(name);
			*vp = JSVAL_NULL;
			return JS_TRUE;
		}

		/* Locate the target SQLiteTable. Walk record -> table -> conn;
		 * the conn's table namespace's resolve hook will create/cache
		 * the target SQLiteTable lazily. */
		sqlite_conn_private_t* cp = p->table->conn;
		if (cp->table_obj == NULL || cp->db == NULL) {
			JS_ReportError(cx, "SQLiteRecord: connection is closed");
			free(name);
			return JS_FALSE;
		}
		jsval target_val;
		if (!JS_GetProperty(cx, cp->table_obj, p->table->fks[fk].to_table, &target_val)) {
			free(name);
			return JS_FALSE;
		}
		if (!JSVAL_IS_OBJECT(target_val) || JSVAL_IS_NULL(target_val)) {
			JS_ReportError(cx, "SQLiteRecord: FK target table '%s' does not exist",
			    p->table->fks[fk].to_table);
			free(name);
			return JS_FALSE;
		}
		JSObject* target_obj = JSVAL_TO_OBJECT(target_val);
		sqlite_table_private_t* target_tp = (sqlite_table_private_t*)
		    js_GetClassPrivate(cx, target_obj, &js_sqlite_table_class);
		if (target_tp == NULL) {
			JS_ReportError(cx, "SQLiteRecord: FK target '%s' is not a SQLiteTable",
			    p->table->fks[fk].to_table);
			free(name);
			return JS_FALSE;
		}
		/* The FK might reference a non-PK column on the target table.
		 * SQLiteRecord.row() uses the target's declared PK, which only
		 * works when the FK's to_col matches that PK. Verify, and fall
		 * back to a manual SELECT otherwise. */
		bool pk_match = (target_tp->npk == 1)
		    && (strcmp(target_tp->cols[target_tp->pk_cols[0]].name,
		               p->table->fks[fk].to_col[0]) == 0);
		free(name);
		if (!pk_match) {
			JS_ReportError(cx, "SQLiteRecord: FK references non-PK column "
			    "'%s.%s'; @col only supports PK-targeted FKs (use a manual "
			    "select with WHERE)",
			    target_tp->name, p->table->fks[fk].to_col[0]);
			return JS_FALSE;
		}
		return table_load_by_pk(cx, target_obj, target_tp, 1, &fk_val, vp);
	}

	free(name);
	return JS_TRUE;
}

static JSBool js_sqlite_record_set(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	sqlite_record_private_t* p = (sqlite_record_private_t*)JS_GetPrivate(cx, obj);
	if (p == NULL) return JS_TRUE;

	jsval idval;
	if (!JS_IdToValue(cx, id, &idval) || !JSVAL_IS_STRING(idval))
		return JS_TRUE;

	char* name = NULL;
	JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
	if (name == NULL) return JS_TRUE;

	/* Allow writes to hidden/internal names (used by the resolve hook for
	 * method/property definitions and by us for the data array). */
	if (name[0] == '_' || name[0] == '$') {
		free(name);
		return JS_TRUE;
	}

	int col = record_col_index(p->table, name);
	if (col < 0) {
		JS_ReportError(cx, "SQLiteRecord: no column '%s' on table '%s'",
		    name, p->table->name);
		free(name);
		return JS_FALSE;
	}
	free(name);
	return JS_SetElement(cx, p->data_arr, col, vp);
}

static JSBool js_record_insert(JSContext* cx, uintN argc, jsval* arglist);
static JSBool js_record_update(JSContext* cx, uintN argc, jsval* arglist);
static JSBool js_record_remove(JSContext* cx, uintN argc, jsval* arglist);
static JSBool js_record_toObject(JSContext* cx, uintN argc, jsval* arglist);

static jsSyncMethodSpec js_sqlite_record_functions[] = {
	{"$insert",   js_record_insert,   0, JSTYPE_OBJECT,
	 JSDOCSTR(""),
	 JSDOCSTR("Insert the record's defined columns as a new row in its table. "
	          "Undefined columns are omitted so SQLite applies the declared "
	          "DEFAULT (use <tt>null</tt> if you want to write NULL explicitly). "
	          "If the table has a single INTEGER PRIMARY KEY column and the script "
	          "didn't set it, the auto-assigned rowid is back-filled onto the "
	          "record. Returns the record itself for chaining."),
	 32200},
	{"$update",   js_record_update,   0, JSTYPE_NUMBER,
	 JSDOCSTR(""),
	 JSDOCSTR("Write the record's defined columns back to the row identified by "
	          "the original primary-key values captured at <tt>row()</tt>/"
	          "<tt>select()</tt> time (so changing a PK column is a valid rename). "
	          "Throws if the table has no PK, or if the record was created with "
	          "<tt>new_row()</tt> and never $insert()ed. Returns the number of rows "
	          "actually modified."),
	 32200},
	{"$remove",   js_record_remove,   0, JSTYPE_NUMBER,
	 JSDOCSTR(""),
	 JSDOCSTR("Delete the row identified by the original primary-key values. "
	          "Same preconditions as $update(). Returns the number of rows deleted "
	          "(0 if the row had already been removed by another process)."),
	 32200},
	{"$toObject", js_record_toObject, 0, JSTYPE_OBJECT,
	 JSDOCSTR(""),
	 JSDOCSTR("Return a plain JS object with the record's defined column values. "
	          "Suitable for binding into other queries via bind() and for external "
	          "serialization. Undefined columns are omitted."),
	 32200},
	{0}
};

static JSBool js_sqlite_record_resolve(JSContext* cx, JSObject* obj, jsid id)
{
	char*  name = NULL;
	JSBool ret;
	if (id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		JS_IdToValue(cx, id, &idval);
		if (JSVAL_IS_STRING(idval))
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
	}
	ret = js_SyncResolve(cx, obj, name, NULL, js_sqlite_record_functions, NULL, 0);
	if (name) free(name);
	return ret;
}

static JSBool js_sqlite_record_enumerate(JSContext* cx, JSObject* obj)
{
	return js_sqlite_record_resolve(cx, obj, JSID_VOID);
}

JSClass js_sqlite_record_class = {
	"SQLiteRecord"
	, JSCLASS_HAS_PRIVATE
	, JS_PropertyStub
	, JS_PropertyStub
	, js_sqlite_record_get
	, js_sqlite_record_set
	, js_sqlite_record_enumerate
	, js_sqlite_record_resolve
	, JS_ConvertStub
	, js_finalize_record
};

static JSObject* new_record_object(JSContext* cx, JSObject* table_obj,
                                   sqlite_table_private_t* tp)
{
	JSObject* obj = JS_NewObject(cx, &js_sqlite_record_class, NULL, NULL);
	if (obj == NULL) return NULL;

	sqlite_record_private_t* rp =
	    (sqlite_record_private_t*)calloc(1, sizeof(*rp));
	if (rp == NULL) { JS_ReportError(cx, "calloc failed"); return NULL; }
	rp->table_obj = table_obj;
	rp->table     = tp;
	rp->inserted  = false;

	if (!JS_SetPrivate(cx, obj, rp)) {
		free(rp);
		JS_ReportError(cx, "JS_SetPrivate failed");
		return NULL;
	}

	/* Data and orig-pk arrays must be allocated AFTER SetPrivate so the
	 * finalizer cleans up on partial-construction failures. Initialize all
	 * data slots to JSVAL_VOID (= "not defined"). */
	rp->data_arr = JS_NewArrayObject(cx, tp->ncols, NULL);
	if (rp->data_arr == NULL) return NULL;
	for (int i = 0; i < tp->ncols; i++) {
		jsval v = JSVAL_VOID;
		JS_SetElement(cx, rp->data_arr, i, &v);
	}
	JS_DefineProperty(cx, obj, RECORD_DATA_PROP, OBJECT_TO_JSVAL(rp->data_arr),
	    NULL, NULL, JSPROP_PERMANENT | JSPROP_READONLY);

	rp->orig_pk_arr = JS_NewArrayObject(cx, tp->npk, NULL);
	if (rp->orig_pk_arr == NULL) return NULL;
	JS_DefineProperty(cx, obj, RECORD_ORIG_PK_PROP, OBJECT_TO_JSVAL(rp->orig_pk_arr),
	    NULL, NULL, JSPROP_PERMANENT | JSPROP_READONLY);

	JS_DefineProperty(cx, obj, ROOT_PROP_NAME, OBJECT_TO_JSVAL(table_obj),
	    NULL, NULL, JSPROP_PERMANENT | JSPROP_READONLY);

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj,
	    "A row of a specific table. Column values are read/written as properties "
	    "(<tt>r.email = \"...\"</tt>); typos to non-existent column names throw "
	    "on write so they surface at the call site. Created by "
	    "<tt>db.table.X.new_row()</tt> (empty, defaults filled in), "
	    "<tt>db.table.X.row(pk)</tt> (loaded from the database), or "
	    "<tt>db.table.X.select(...)</tt> (array of records). The "
	    "<tt>$insert</tt> / <tt>$update</tt> / <tt>$remove</tt> / "
	    "<tt>$toObject</tt> methods are <tt>$</tt>-prefixed so they don't "
	    "shadow any column with the same name. Pass a record directly to "
	    "<tt>stmt.bind()</tt> or <tt>db.run()</tt> with named placeholders to "
	    "use its values in a query.", 32200);
#endif
	return obj;
}

/* Evaluate each column's DEFAULT expression via SQLite and stuff the
 * results into the record's data array. The dflt_value strings came from
 * the schema (trusted), so concatenating them into a SELECT is safe.
 * CURRENT_TIMESTAMP etc. are re-evaluated on every call. */
static JSBool record_apply_defaults(JSContext* cx, sqlite_record_private_t* rp)
{
	sqlite_table_private_t* tp = rp->table;
	sqlite_conn_private_t*  cp = tp->conn;
	if (cp->db == NULL) {
		JS_ReportError(cx, "Connection is closed");
		return JS_FALSE;
	}

	int  ndef = 0;
	int* idx  = (int*)malloc(tp->ncols * sizeof(int));
	if (idx == NULL) { JS_ReportError(cx, "malloc failed"); return JS_FALSE; }
	for (int i = 0; i < tp->ncols; i++) {
		if (tp->cols[i].dflt_value != NULL) idx[ndef++] = i;
	}
	if (ndef == 0) { free(idx); return JS_TRUE; }

	size_t bufsz = 16;
	for (int i = 0; i < ndef; i++)
		bufsz += strlen(tp->cols[idx[i]].dflt_value) + 4;
	char* sql = (char*)malloc(bufsz);
	if (sql == NULL) { free(idx); JS_ReportError(cx, "malloc failed"); return JS_FALSE; }
	size_t off = 0;
	off += (size_t)snprintf(sql, bufsz, "SELECT ");
	for (int i = 0; i < ndef; i++) {
		off += (size_t)snprintf(sql + off, bufsz - off, "%s%s",
		    i ? ", " : "", tp->cols[idx[i]].dflt_value);
	}

	sqlite3_stmt* stmt = NULL;
	jsrefcount susp = JS_SUSPENDREQUEST(cx);
	int rc = sqlite3_prepare_v2(cp->db, sql, -1, &stmt, NULL);
	JS_RESUMEREQUEST(cx, susp);
	free(sql);
	if (rc != SQLITE_OK) { free(idx); return sqlite_throw(cx, cp->db, rc, "new_row defaults prepare"); }

	susp = JS_SUSPENDREQUEST(cx);
	rc = sqlite3_step(stmt);
	JS_RESUMEREQUEST(cx, susp);
	if (rc != SQLITE_ROW) {
		JSBool b = sqlite_throw(cx, cp->db, rc, "new_row defaults step");
		sqlite3_finalize(stmt);
		free(idx);
		return b;
	}

	for (int i = 0; i < ndef; i++) {
		jsval v = JSVAL_VOID;
		int t = sqlite3_column_type(stmt, i);
		switch (t) {
			case SQLITE_INTEGER: {
				sqlite3_int64 n = sqlite3_column_int64(stmt, i);
				/* JS Number is double; lossy above 2^53. Matches bind side. */
				if (!JS_NewNumberValue(cx, (double)n, &v)) {
					sqlite3_finalize(stmt); free(idx); return JS_FALSE;
				}
				break;
			}
			case SQLITE_FLOAT: {
				double d = sqlite3_column_double(stmt, i);
				if (!JS_NewNumberValue(cx, d, &v)) {
					sqlite3_finalize(stmt); free(idx); return JS_FALSE;
				}
				break;
			}
			case SQLITE_TEXT: {
				const char* s = (const char*)sqlite3_column_text(stmt, i);
				int len = sqlite3_column_bytes(stmt, i);
				JSString* js = JS_NewStringCopyN(cx, s ? s : "", len);
				if (js == NULL) { sqlite3_finalize(stmt); free(idx); return JS_FALSE; }
				v = STRING_TO_JSVAL(js);
				break;
			}
			case SQLITE_BLOB: {
				const void* data = sqlite3_column_blob(stmt, i);
				int len = sqlite3_column_bytes(stmt, i);
				JSObject* ab = js_CreateArrayBuffer(cx, len);
				if (ab == NULL) { sqlite3_finalize(stmt); free(idx); return JS_FALSE; }
				if (len > 0 && data) {
					js::ArrayBuffer* abuf = js::ArrayBuffer::fromJSObject(ab);
					if (abuf) memcpy(abuf->data, data, len);
				}
				v = OBJECT_TO_JSVAL(ab);
				break;
			}
			case SQLITE_NULL:
				v = JSVAL_NULL;
				break;
		}
		JS_SetElement(cx, rp->data_arr, idx[i], &v);
	}
	sqlite3_finalize(stmt);
	free(idx);
	return JS_TRUE;
}

/* ---------- SQL identifier quoting ---------- */

/* Quote a column or table name for safe embedding in generated SQL.
 * Wraps in double quotes and doubles any embedded double quote. Names
 * come from the cached schema (which itself came from sqlite_master),
 * so they're trusted — this is defense in depth against odd identifiers
 * (spaces, reserved words, etc.). Returns malloc'd string or NULL. */
static char* sql_quote_ident(const char* name)
{
	if (name == NULL) return NULL;
	size_t len = strlen(name);
	char* out = (char*)malloc(len * 2 + 3);
	if (out == NULL) return NULL;
	size_t o = 0;
	out[o++] = '"';
	for (size_t i = 0; i < len; i++) {
		out[o++] = name[i];
		if (name[i] == '"') out[o++] = '"';
	}
	out[o++] = '"';
	out[o] = '\0';
	return out;
}

/* ---------- SQLiteRecord.insert() ---------- */

static JSBool js_record_insert(JSContext* cx, uintN argc, jsval* arglist)
{
	(void)argc;
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	sqlite_record_private_t* rp = (sqlite_record_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_record_class);
	if (rp == NULL) return JS_FALSE;
	sqlite_table_private_t* tp = rp->table;
	sqlite_conn_private_t*  cp = tp->conn;
	if (cp->db == NULL) {
		JS_ReportError(cx, "Connection is closed");
		return JS_FALSE;
	}

	/* Collect defined column indices (slot != JSVAL_VOID). */
	int  ndef = 0;
	int* idx  = (int*)malloc(tp->ncols * sizeof(int));
	if (idx == NULL) { JS_ReportError(cx, "malloc failed"); return JS_FALSE; }
	for (int i = 0; i < tp->ncols; i++) {
		jsval v;
		if (!JS_GetElement(cx, rp->data_arr, i, &v)) { free(idx); return JS_FALSE; }
		if (!JSVAL_IS_VOID(v)) idx[ndef++] = i;
	}

	/* Build SQL: INSERT INTO "t" ("a", "b") VALUES (?, ?) — or
	 * INSERT INTO "t" DEFAULT VALUES if no columns are defined. */
	char* qt = sql_quote_ident(tp->name);
	if (qt == NULL) { free(idx); JS_ReportError(cx, "malloc failed"); return JS_FALSE; }

	char*  sql = NULL;
	size_t sqlcap = 0;
	if (ndef == 0) {
		sqlcap = strlen(qt) + 32;
		sql = (char*)malloc(sqlcap);
		if (sql == NULL) { free(qt); free(idx); JS_ReportError(cx, "malloc failed"); return JS_FALSE; }
		snprintf(sql, sqlcap, "INSERT INTO %s DEFAULT VALUES", qt);
	} else {
		/* size: "INSERT INTO " + qt + " (" + cols + ") VALUES (" + ?s + ")" */
		sqlcap = strlen(qt) + 64;
		for (int i = 0; i < ndef; i++) sqlcap += strlen(tp->cols[idx[i]].name) * 2 + 8;
		sql = (char*)malloc(sqlcap);
		if (sql == NULL) { free(qt); free(idx); JS_ReportError(cx, "malloc failed"); return JS_FALSE; }
		size_t off = (size_t)snprintf(sql, sqlcap, "INSERT INTO %s (", qt);
		for (int i = 0; i < ndef; i++) {
			char* qc = sql_quote_ident(tp->cols[idx[i]].name);
			if (qc == NULL) { free(sql); free(qt); free(idx); JS_ReportError(cx, "malloc failed"); return JS_FALSE; }
			off += (size_t)snprintf(sql + off, sqlcap - off, "%s%s", i ? ", " : "", qc);
			free(qc);
		}
		off += (size_t)snprintf(sql + off, sqlcap - off, ") VALUES (");
		for (int i = 0; i < ndef; i++)
			off += (size_t)snprintf(sql + off, sqlcap - off, "%s?", i ? ", " : "");
		off += (size_t)snprintf(sql + off, sqlcap - off, ")");
	}
	free(qt);

	sqlite3_stmt* stmt = NULL;
	jsrefcount susp = JS_SUSPENDREQUEST(cx);
	int rc = sqlite3_prepare_v2(cp->db, sql, -1, &stmt, NULL);
	JS_RESUMEREQUEST(cx, susp);
	free(sql);
	if (rc != SQLITE_OK) { free(idx); return sqlite_throw(cx, cp->db, rc, "insert/prepare"); }

	/* Bind defined values. */
	for (int i = 0; i < ndef; i++) {
		jsval v;
		if (!JS_GetElement(cx, rp->data_arr, idx[i], &v)) {
			sqlite3_finalize(stmt); free(idx); return JS_FALSE;
		}
		if (!bind_jsval(cx, stmt, i + 1, v)) {
			sqlite3_finalize(stmt); free(idx); return JS_FALSE;
		}
	}

	susp = JS_SUSPENDREQUEST(cx);
	rc = sqlite3_step(stmt);
	JS_RESUMEREQUEST(cx, susp);
	sqlite3_finalize(stmt);
	free(idx);
	if (rc != SQLITE_DONE) return sqlite_throw(cx, cp->db, rc, "insert/step");

	/* Backfill auto-assigned rowid for tables whose PK is a single
	 * INTEGER PRIMARY KEY column (the SQLite rowid alias). Only do so
	 * when the script didn't set that column itself. */
	if (tp->npk == 1) {
		int pk_idx = tp->pk_cols[0];
		sqlite_table_col_t* pc = &tp->cols[pk_idx];
		/* "INTEGER PRIMARY KEY" specifically aliases rowid (case-
		 * insensitive type match). Composite-PK rows and non-INTEGER
		 * PKs don't get this treatment. */
		bool is_rowid_alias = false;
		if (pc->type && (strcasecmp(pc->type, "INTEGER") == 0))
			is_rowid_alias = true;
		jsval cur;
		if (!JS_GetElement(cx, rp->data_arr, pk_idx, &cur)) return JS_FALSE;
		if (is_rowid_alias && JSVAL_IS_VOID(cur)) {
			sqlite3_int64 rid = sqlite3_last_insert_rowid(cp->db);
			jsval rv;
			if (!JS_NewNumberValue(cx, (double)rid, &rv)) return JS_FALSE;
			if (!JS_SetElement(cx, rp->data_arr, pk_idx, &rv)) return JS_FALSE;
		}
	}

	/* Capture original PK values (current row state, post-backfill). */
	for (int i = 0; i < tp->npk; i++) {
		jsval v;
		if (!JS_GetElement(cx, rp->data_arr, tp->pk_cols[i], &v)) return JS_FALSE;
		if (!JS_SetElement(cx, rp->orig_pk_arr, i, &v)) return JS_FALSE;
	}
	rp->inserted = true;

	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
	return JS_TRUE;
}

/* Common preflight for update() / remove(): connection alive, table has
 * a PK, record was loaded or inserted (so orig_pk values are present). */
static JSBool record_preflight_mutation(JSContext* cx, sqlite_record_private_t* rp,
                                        const char* op)
{
	if (rp->table->conn->db == NULL) {
		JS_ReportError(cx, "Connection is closed");
		return JS_FALSE;
	}
	if (rp->table->npk == 0) {
		JS_ReportError(cx, "SQLiteRecord.$%s(): table '%s' has no primary key",
		    op, rp->table->name);
		return JS_FALSE;
	}
	if (!rp->inserted) {
		JS_ReportError(cx, "SQLiteRecord.$%s(): record was never $insert()ed or "
		    "loaded from the database (no row to %s)", op, op);
		return JS_FALSE;
	}
	return JS_TRUE;
}

static JSBool js_record_update(JSContext* cx, uintN argc, jsval* arglist)
{
	(void)argc;
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	sqlite_record_private_t* rp = (sqlite_record_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_record_class);
	if (rp == NULL) return JS_FALSE;
	if (!record_preflight_mutation(cx, rp, "update")) return JS_FALSE;

	sqlite_table_private_t* tp = rp->table;
	sqlite_conn_private_t*  cp = tp->conn;

	/* Collect indices of defined columns for the SET list. */
	int  nset = 0;
	int* setidx = (int*)malloc(tp->ncols * sizeof(int));
	if (setidx == NULL) { JS_ReportError(cx, "malloc failed"); return JS_FALSE; }
	for (int i = 0; i < tp->ncols; i++) {
		jsval v;
		if (!JS_GetElement(cx, rp->data_arr, i, &v)) { free(setidx); return JS_FALSE; }
		if (!JSVAL_IS_VOID(v)) setidx[nset++] = i;
	}
	if (nset == 0) {
		/* No columns defined => no-op. Refuse explicitly rather than
		 * generating invalid SQL. */
		free(setidx);
		JS_ReportError(cx, "SQLiteRecord.$update(): no defined columns to write");
		return JS_FALSE;
	}

	/* Build: UPDATE "t" SET "c1"=?, "c2"=? WHERE "pk1"=? AND "pk2"=? */
	char* qt = sql_quote_ident(tp->name);
	if (qt == NULL) { free(setidx); JS_ReportError(cx, "malloc failed"); return JS_FALSE; }
	size_t sqlcap = strlen(qt) + 64;
	for (int i = 0; i < nset; i++)    sqlcap += strlen(tp->cols[setidx[i]].name) * 2 + 12;
	for (int i = 0; i < tp->npk; i++) sqlcap += strlen(tp->cols[tp->pk_cols[i]].name) * 2 + 12;
	char* sql = (char*)malloc(sqlcap);
	if (sql == NULL) { free(qt); free(setidx); JS_ReportError(cx, "malloc failed"); return JS_FALSE; }
	size_t off = (size_t)snprintf(sql, sqlcap, "UPDATE %s SET ", qt);
	for (int i = 0; i < nset; i++) {
		char* qc = sql_quote_ident(tp->cols[setidx[i]].name);
		if (qc == NULL) { free(sql); free(qt); free(setidx); JS_ReportError(cx, "malloc failed"); return JS_FALSE; }
		off += (size_t)snprintf(sql + off, sqlcap - off, "%s%s = ?", i ? ", " : "", qc);
		free(qc);
	}
	off += (size_t)snprintf(sql + off, sqlcap - off, " WHERE ");
	for (int i = 0; i < tp->npk; i++) {
		char* qc = sql_quote_ident(tp->cols[tp->pk_cols[i]].name);
		if (qc == NULL) { free(sql); free(qt); free(setidx); JS_ReportError(cx, "malloc failed"); return JS_FALSE; }
		off += (size_t)snprintf(sql + off, sqlcap - off, "%s%s = ?", i ? " AND " : "", qc);
		free(qc);
	}
	free(qt);

	sqlite3_stmt* stmt = NULL;
	jsrefcount susp = JS_SUSPENDREQUEST(cx);
	int rc = sqlite3_prepare_v2(cp->db, sql, -1, &stmt, NULL);
	JS_RESUMEREQUEST(cx, susp);
	free(sql);
	if (rc != SQLITE_OK) { free(setidx); return sqlite_throw(cx, cp->db, rc, "update/prepare"); }

	/* Bind SET values (current), then WHERE (original PK). */
	int bind_pos = 1;
	for (int i = 0; i < nset; i++) {
		jsval v;
		if (!JS_GetElement(cx, rp->data_arr, setidx[i], &v)) {
			sqlite3_finalize(stmt); free(setidx); return JS_FALSE;
		}
		if (!bind_jsval(cx, stmt, bind_pos++, v)) {
			sqlite3_finalize(stmt); free(setidx); return JS_FALSE;
		}
	}
	for (int i = 0; i < tp->npk; i++) {
		jsval v;
		if (!JS_GetElement(cx, rp->orig_pk_arr, i, &v)) {
			sqlite3_finalize(stmt); free(setidx); return JS_FALSE;
		}
		if (!bind_jsval(cx, stmt, bind_pos++, v)) {
			sqlite3_finalize(stmt); free(setidx); return JS_FALSE;
		}
	}
	free(setidx);

	susp = JS_SUSPENDREQUEST(cx);
	rc = sqlite3_step(stmt);
	JS_RESUMEREQUEST(cx, susp);
	int changes = sqlite3_changes(cp->db);
	sqlite3_finalize(stmt);
	if (rc != SQLITE_DONE) return sqlite_throw(cx, cp->db, rc, "update/step");

	/* Refresh orig_pk from current data so a subsequent update() works
	 * even if the script just renamed the PK. */
	for (int i = 0; i < tp->npk; i++) {
		jsval v;
		if (!JS_GetElement(cx, rp->data_arr, tp->pk_cols[i], &v)) return JS_FALSE;
		if (!JS_SetElement(cx, rp->orig_pk_arr, i, &v)) return JS_FALSE;
	}

	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(changes));
	return JS_TRUE;
}

static JSBool js_record_remove(JSContext* cx, uintN argc, jsval* arglist)
{
	(void)argc;
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	sqlite_record_private_t* rp = (sqlite_record_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_record_class);
	if (rp == NULL) return JS_FALSE;
	if (!record_preflight_mutation(cx, rp, "remove")) return JS_FALSE;

	sqlite_table_private_t* tp = rp->table;
	sqlite_conn_private_t*  cp = tp->conn;

	/* DELETE FROM "t" WHERE "pk1"=? AND "pk2"=? */
	char* qt = sql_quote_ident(tp->name);
	if (qt == NULL) { JS_ReportError(cx, "malloc failed"); return JS_FALSE; }
	size_t sqlcap = strlen(qt) + 32;
	for (int i = 0; i < tp->npk; i++)
		sqlcap += strlen(tp->cols[tp->pk_cols[i]].name) * 2 + 12;
	char* sql = (char*)malloc(sqlcap);
	if (sql == NULL) { free(qt); JS_ReportError(cx, "malloc failed"); return JS_FALSE; }
	size_t off = (size_t)snprintf(sql, sqlcap, "DELETE FROM %s WHERE ", qt);
	for (int i = 0; i < tp->npk; i++) {
		char* qc = sql_quote_ident(tp->cols[tp->pk_cols[i]].name);
		if (qc == NULL) { free(sql); free(qt); JS_ReportError(cx, "malloc failed"); return JS_FALSE; }
		off += (size_t)snprintf(sql + off, sqlcap - off, "%s%s = ?", i ? " AND " : "", qc);
		free(qc);
	}
	free(qt);

	sqlite3_stmt* stmt = NULL;
	jsrefcount susp = JS_SUSPENDREQUEST(cx);
	int rc = sqlite3_prepare_v2(cp->db, sql, -1, &stmt, NULL);
	JS_RESUMEREQUEST(cx, susp);
	free(sql);
	if (rc != SQLITE_OK) return sqlite_throw(cx, cp->db, rc, "remove/prepare");

	for (int i = 0; i < tp->npk; i++) {
		jsval v;
		if (!JS_GetElement(cx, rp->orig_pk_arr, i, &v)) {
			sqlite3_finalize(stmt); return JS_FALSE;
		}
		if (!bind_jsval(cx, stmt, i + 1, v)) {
			sqlite3_finalize(stmt); return JS_FALSE;
		}
	}

	susp = JS_SUSPENDREQUEST(cx);
	rc = sqlite3_step(stmt);
	JS_RESUMEREQUEST(cx, susp);
	int changes = sqlite3_changes(cp->db);
	sqlite3_finalize(stmt);
	if (rc != SQLITE_DONE) return sqlite_throw(cx, cp->db, rc, "remove/step");

	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(changes));
	return JS_TRUE;
}

static JSBool js_record_toObject(JSContext* cx, uintN argc, jsval* arglist)
{
	(void)argc;
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	sqlite_record_private_t* rp = (sqlite_record_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_record_class);
	if (rp == NULL) return JS_FALSE;
	sqlite_table_private_t* tp = rp->table;

	JSObject* out = JS_NewObject(cx, NULL, NULL, NULL);
	if (out == NULL) return JS_FALSE;
	for (int i = 0; i < tp->ncols; i++) {
		jsval v;
		if (!JS_GetElement(cx, rp->data_arr, i, &v)) return JS_FALSE;
		if (JSVAL_IS_VOID(v)) continue;  /* skip undefined; null IS written */
		if (!JS_DefineProperty(cx, out, tp->cols[i].name, v,
		    NULL, NULL, JSPROP_ENUMERATE)) return JS_FALSE;
	}
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(out));
	return JS_TRUE;
}

/* ---------- column read into a record ---------- */

/* Pull a single column out of a stepped statement and store it into the
 * record's data array at column index `col_idx`. Shared by row() and
 * select(). Mirrors the type dispatch in record_apply_defaults but reads
 * directly from a stepped row instead of a one-shot SELECT. */
static JSBool record_load_col(JSContext* cx, sqlite_record_private_t* rp,
                              int col_idx, sqlite3_stmt* stmt, int result_idx)
{
	jsval v = JSVAL_VOID;
	int t = sqlite3_column_type(stmt, result_idx);
	switch (t) {
		case SQLITE_INTEGER: {
			sqlite3_int64 n = sqlite3_column_int64(stmt, result_idx);
			if (!JS_NewNumberValue(cx, (double)n, &v)) return JS_FALSE;
			break;
		}
		case SQLITE_FLOAT: {
			double d = sqlite3_column_double(stmt, result_idx);
			if (!JS_NewNumberValue(cx, d, &v)) return JS_FALSE;
			break;
		}
		case SQLITE_TEXT: {
			const char* s = (const char*)sqlite3_column_text(stmt, result_idx);
			int len = sqlite3_column_bytes(stmt, result_idx);
			JSString* js = JS_NewStringCopyN(cx, s ? s : "", len);
			if (js == NULL) return JS_FALSE;
			v = STRING_TO_JSVAL(js);
			break;
		}
		case SQLITE_BLOB: {
			const void* data = sqlite3_column_blob(stmt, result_idx);
			int len = sqlite3_column_bytes(stmt, result_idx);
			JSObject* ab = js_CreateArrayBuffer(cx, len);
			if (ab == NULL) return JS_FALSE;
			if (len > 0 && data) {
				js::ArrayBuffer* abuf = js::ArrayBuffer::fromJSObject(ab);
				if (abuf) memcpy(abuf->data, data, len);
			}
			v = OBJECT_TO_JSVAL(ab);
			break;
		}
		case SQLITE_NULL:
			v = JSVAL_NULL;
			break;
	}
	return JS_SetElement(cx, rp->data_arr, col_idx, &v);
}

/* Builds "SELECT "c1", "c2", ... FROM "t"" into a malloc'd string.
 * Caller frees. Returns NULL on alloc failure (with JS error reported). */
static char* table_build_select_prefix(JSContext* cx, sqlite_table_private_t* tp)
{
	char* qt = sql_quote_ident(tp->name);
	if (qt == NULL) { JS_ReportError(cx, "malloc failed"); return NULL; }
	size_t cap = strlen(qt) + 32;
	for (int i = 0; i < tp->ncols; i++) cap += strlen(tp->cols[i].name) * 2 + 4;
	char* sql = (char*)malloc(cap);
	if (sql == NULL) { free(qt); JS_ReportError(cx, "malloc failed"); return NULL; }
	size_t off = (size_t)snprintf(sql, cap, "SELECT ");
	for (int i = 0; i < tp->ncols; i++) {
		char* qc = sql_quote_ident(tp->cols[i].name);
		if (qc == NULL) { free(sql); free(qt); JS_ReportError(cx, "malloc failed"); return NULL; }
		off += (size_t)snprintf(sql + off, cap - off, "%s%s", i ? ", " : "", qc);
		free(qc);
	}
	off += (size_t)snprintf(sql + off, cap - off, " FROM %s", qt);
	free(qt);
	return sql;
}

/* Populate a SQLiteRecord from the current row of a stepped statement
 * (column indices 0..ncols-1 match the SELECT prefix produced by
 * table_build_select_prefix). Captures original PK values and sets
 * inserted = true. */
static JSBool record_load_from_stmt(JSContext* cx, sqlite_record_private_t* rp,
                                    sqlite3_stmt* stmt)
{
	sqlite_table_private_t* tp = rp->table;
	for (int i = 0; i < tp->ncols; i++)
		if (!record_load_col(cx, rp, i, stmt, i)) return JS_FALSE;
	for (int i = 0; i < tp->npk; i++) {
		jsval v;
		if (!JS_GetElement(cx, rp->data_arr, tp->pk_cols[i], &v)) return JS_FALSE;
		if (!JS_SetElement(cx, rp->orig_pk_arr, i, &v)) return JS_FALSE;
	}
	rp->inserted = true;
	return JS_TRUE;
}

/* ---------- shared: fetch one row by PK ----------
 *
 * Builds and runs the SELECT-by-PK statement against table `tp` and
 * stores either the loaded SQLiteRecord OBJECT_TO_JSVAL or JSVAL_NULL
 * (no matching row) into *out. The `table_obj` is the SQLiteTable JS
 * object the resulting record should hang off (via its rooted parent
 * back-pointer). Caller is expected to have already validated that
 * `npk == tp->npk` and that pk_args has that many entries. */
static JSBool table_load_by_pk(JSContext* cx, JSObject* table_obj,
                               sqlite_table_private_t* tp,
                               int npk, jsval* pk_args, jsval* out)
{
	sqlite_conn_private_t* cp = tp->conn;
	if (cp->db == NULL) { JS_ReportError(cx, "Connection is closed"); return JS_FALSE; }

	char* sel = table_build_select_prefix(cx, tp);
	if (sel == NULL) return JS_FALSE;
	size_t cap = strlen(sel) + 64;
	for (int i = 0; i < npk; i++) cap += strlen(tp->cols[tp->pk_cols[i]].name) * 2 + 12;
	char* sql = (char*)malloc(cap);
	if (sql == NULL) { free(sel); JS_ReportError(cx, "malloc failed"); return JS_FALSE; }
	size_t off = (size_t)snprintf(sql, cap, "%s WHERE ", sel);
	free(sel);
	for (int i = 0; i < npk; i++) {
		char* qc = sql_quote_ident(tp->cols[tp->pk_cols[i]].name);
		if (qc == NULL) { free(sql); JS_ReportError(cx, "malloc failed"); return JS_FALSE; }
		off += (size_t)snprintf(sql + off, cap - off, "%s%s = ?", i ? " AND " : "", qc);
		free(qc);
	}

	sqlite3_stmt* stmt = NULL;
	jsrefcount susp = JS_SUSPENDREQUEST(cx);
	int rc = sqlite3_prepare_v2(cp->db, sql, -1, &stmt, NULL);
	JS_RESUMEREQUEST(cx, susp);
	free(sql);
	if (rc != SQLITE_OK) return sqlite_throw(cx, cp->db, rc, "row/prepare");

	for (int i = 0; i < npk; i++) {
		if (!bind_jsval(cx, stmt, i + 1, pk_args[i])) {
			sqlite3_finalize(stmt);
			return JS_FALSE;
		}
	}

	susp = JS_SUSPENDREQUEST(cx);
	rc = sqlite3_step(stmt);
	JS_RESUMEREQUEST(cx, susp);
	if (rc == SQLITE_DONE) {
		sqlite3_finalize(stmt);
		*out = JSVAL_NULL;
		return JS_TRUE;
	}
	if (rc != SQLITE_ROW) {
		JSBool b = sqlite_throw(cx, cp->db, rc, "row/step");
		sqlite3_finalize(stmt);
		return b;
	}

	JSObject* rec = new_record_object(cx, table_obj, tp);
	if (rec == NULL) { sqlite3_finalize(stmt); return JS_FALSE; }
	sqlite_record_private_t* rp = (sqlite_record_private_t*)JS_GetPrivate(cx, rec);
	if (!record_load_from_stmt(cx, rp, stmt)) {
		sqlite3_finalize(stmt);
		return JS_FALSE;
	}
	sqlite3_finalize(stmt);
	*out = OBJECT_TO_JSVAL(rec);
	return JS_TRUE;
}

/* ---------- SQLiteTable.row(pk [, pk2, ...]) ---------- */

static JSBool js_table_row(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	jsval*    argv = JS_ARGV(cx, arglist);
	sqlite_table_private_t* tp = (sqlite_table_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_table_class);
	if (tp == NULL) return JS_FALSE;

	if (tp->npk == 0) {
		JS_ReportError(cx, "SQLiteTable.row(): table '%s' has no primary key; "
		    "use select() with an explicit WHERE clause instead", tp->name);
		return JS_FALSE;
	}
	if ((int)argc < tp->npk) {
		JS_ReportError(cx, "SQLiteTable.row(): table '%s' has %d-column PK; "
		    "expected %d argument(s), got %d", tp->name, tp->npk, tp->npk, (int)argc);
		return JS_FALSE;
	}

	jsval out;
	if (!table_load_by_pk(cx, obj, tp, tp->npk, argv, &out)) return JS_FALSE;
	JS_SET_RVAL(cx, arglist, out);
	return JS_TRUE;
}

/* ---------- SQLiteTable.select(where_sql, params) ---------- */

static JSBool js_table_select(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	jsval*    argv = JS_ARGV(cx, arglist);
	sqlite_table_private_t* tp = (sqlite_table_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_table_class);
	if (tp == NULL) return JS_FALSE;
	sqlite_conn_private_t* cp = tp->conn;
	if (cp->db == NULL) { JS_ReportError(cx, "Connection is closed"); return JS_FALSE; }

	/* where_sql: null/undefined -> "all rows"; string -> WHERE fragment. */
	char* where_sql = NULL;
	if (argc >= 1 && !JSVAL_IS_NULL(argv[0]) && !JSVAL_IS_VOID(argv[0])) {
		if (!JSVAL_IS_STRING(argv[0])) {
			JS_ReportError(cx, "SQLiteTable.select(where_sql, params): "
			    "where_sql must be a string or null");
			return JS_FALSE;
		}
		JSVALUE_TO_MSTRING(cx, argv[0], where_sql, NULL);
		if (where_sql == NULL) return JS_FALSE;
	}

	char* prefix = table_build_select_prefix(cx, tp);
	if (prefix == NULL) { free(where_sql); return JS_FALSE; }
	size_t cap = strlen(prefix) + (where_sql ? strlen(where_sql) + 16 : 4);
	char* sql = (char*)malloc(cap);
	if (sql == NULL) {
		free(prefix); free(where_sql);
		JS_ReportError(cx, "malloc failed");
		return JS_FALSE;
	}
	if (where_sql)
		snprintf(sql, cap, "%s WHERE %s", prefix, where_sql);
	else
		snprintf(sql, cap, "%s", prefix);
	free(prefix);
	free(where_sql);

	sqlite3_stmt* stmt = NULL;
	jsrefcount susp = JS_SUSPENDREQUEST(cx);
	int rc = sqlite3_prepare_v2(cp->db, sql, -1, &stmt, NULL);
	JS_RESUMEREQUEST(cx, susp);
	free(sql);
	if (rc != SQLITE_OK) return sqlite_throw(cx, cp->db, rc, "select/prepare");

	int nparams = sqlite3_bind_parameter_count(stmt);
	if (nparams > 0) {
		if (argc < 2) {
			sqlite3_finalize(stmt);
			JS_ReportError(cx, "SQLiteTable.select(): WHERE has %d placeholder%s "
			    "but no params arg was provided", nparams, nparams == 1 ? "" : "s");
			return JS_FALSE;
		}
		if (!JSVAL_IS_OBJECT(argv[1]) || JSVAL_IS_NULL(argv[1])) {
			sqlite3_finalize(stmt);
			JS_ReportError(cx, "SQLiteTable.select(): params must be an array or object");
			return JS_FALSE;
		}
		JSObject* po = JSVAL_TO_OBJECT(argv[1]);
		if (JS_IsArrayObject(cx, po)) {
			jsuint plen = 0;
			if (!JS_GetArrayLength(cx, po, &plen)) { sqlite3_finalize(stmt); return JS_FALSE; }
			if ((int)plen < nparams) {
				sqlite3_finalize(stmt);
				JS_ReportError(cx, "SQLiteTable.select(): WHERE has %d placeholder%s "
				    "but only %d param%s provided",
				    nparams, nparams == 1 ? "" : "s",
				    (int)plen, plen == 1 ? "" : "s");
				return JS_FALSE;
			}
			for (int i = 0; i < nparams; i++) {
				jsval v;
				if (!JS_GetElement(cx, po, (jsint)i, &v)) { sqlite3_finalize(stmt); return JS_FALSE; }
				if (!bind_jsval(cx, stmt, i + 1, v)) { sqlite3_finalize(stmt); return JS_FALSE; }
			}
		} else {
			/* Object form: iterate own props, look up by :name / @name / $name */
			JSObject* iter = JS_NewPropertyIterator(cx, po);
			if (iter == NULL) { sqlite3_finalize(stmt); return JS_FALSE; }
			jsid pid;
			while (JS_NextProperty(cx, iter, &pid) && pid != JSID_VOID) {
				jsval idv;
				if (!JS_IdToValue(cx, pid, &idv)) { sqlite3_finalize(stmt); return JS_FALSE; }
				if (!JSVAL_IS_STRING(idv)) continue;
				char* key = NULL;
				JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idv), key, NULL);
				if (key == NULL) { sqlite3_finalize(stmt); return JS_FALSE; }
				int pidx = resolve_param_index(stmt, key);
				if (pidx <= 0) { free(key); continue; }
				jsval pv;
				if (!JS_GetProperty(cx, po, key, &pv)) { free(key); sqlite3_finalize(stmt); return JS_FALSE; }
				free(key);
				if (!bind_jsval(cx, stmt, pidx, pv)) { sqlite3_finalize(stmt); return JS_FALSE; }
			}
		}
	}

	JSObject* arr = JS_NewArrayObject(cx, 0, NULL);
	if (arr == NULL) { sqlite3_finalize(stmt); return JS_FALSE; }
	jsuint n = 0;
	for (;;) {
		susp = JS_SUSPENDREQUEST(cx);
		rc = sqlite3_step(stmt);
		JS_RESUMEREQUEST(cx, susp);
		if (rc == SQLITE_DONE) break;
		if (rc != SQLITE_ROW) {
			JSBool b = sqlite_throw(cx, cp->db, rc, "select/step");
			sqlite3_finalize(stmt);
			return b;
		}
		JSObject* rec = new_record_object(cx, obj, tp);
		if (rec == NULL) { sqlite3_finalize(stmt); return JS_FALSE; }
		sqlite_record_private_t* rp = (sqlite_record_private_t*)JS_GetPrivate(cx, rec);
		if (!record_load_from_stmt(cx, rp, stmt)) { sqlite3_finalize(stmt); return JS_FALSE; }
		jsval rv = OBJECT_TO_JSVAL(rec);
		if (!JS_SetElement(cx, arr, n, &rv)) { sqlite3_finalize(stmt); return JS_FALSE; }
		n++;
	}
	sqlite3_finalize(stmt);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(arr));
	return JS_TRUE;
}

/* ---------- SQLiteTable.new_row() ---------- */

static JSBool js_table_new_row(JSContext* cx, uintN argc, jsval* arglist)
{
	JSObject* obj = JS_THIS_OBJECT(cx, arglist);
	sqlite_table_private_t* tp = (sqlite_table_private_t*)
	    js_GetClassPrivate(cx, obj, &js_sqlite_table_class);
	if (tp == NULL) return JS_FALSE;

	JSObject* rec = new_record_object(cx, obj, tp);
	if (rec == NULL) return JS_FALSE;
	sqlite_record_private_t* rp = (sqlite_record_private_t*)JS_GetPrivate(cx, rec);
	if (rp == NULL) return JS_FALSE;
	if (!record_apply_defaults(cx, rp)) return JS_FALSE;

	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(rec));
	return JS_TRUE;
}

/* ---------- db.table namespace ---------- */

static JSBool js_sqlite_table_ns_resolve(JSContext* cx, JSObject* obj, jsid id)
{
	if (id == JSID_VOID || id == JSID_EMPTY) return JS_TRUE;
	jsval idval;
	if (!JS_IdToValue(cx, id, &idval) || !JSVAL_IS_STRING(idval))
		return JS_TRUE;
	char* name = NULL;
	JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
	if (name == NULL) return JS_TRUE;
	/* Skip internal/parent props so the resolve hook doesn't try to look
	 * them up as tables. */
	if (strcmp(name, ROOT_PROP_NAME) == 0 || name[0] == '_') {
		free(name);
		return JS_TRUE;
	}

	jsval parent_val;
	if (!JS_GetProperty(cx, obj, ROOT_PROP_NAME, &parent_val)
	    || !JSVAL_IS_OBJECT(parent_val) || JSVAL_IS_NULL(parent_val)) {
		free(name);
		return JS_TRUE;
	}
	JSObject* conn_obj = JSVAL_TO_OBJECT(parent_val);
	sqlite_conn_private_t* cp = (sqlite_conn_private_t*)
	    js_GetClassPrivate(cx, conn_obj, &js_sqlite_class);
	if (cp == NULL || cp->db == NULL) {
		free(name);
		return JS_TRUE;
	}

	char* lower = str_to_lower_dup(name);
	if (lower == NULL) { free(name); JS_ReportError(cx, "alloc failed"); return JS_FALSE; }

	/* Cache hit? */
	for (sqlite_table_cache_node_t* n = cp->table_cache; n; n = n->next) {
		if (strcmp(n->name_lower, lower) == 0) {
			JS_DefineProperty(cx, obj, name, OBJECT_TO_JSVAL(n->table_obj),
			    NULL, NULL, JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY);
			free(lower);
			free(name);
			return JS_TRUE;
		}
	}

	/* Miss: try to construct. */
	JSObject* table_obj = new_table_object(cx, conn_obj, cp, name);
	if (table_obj == NULL) {
		/* JS error => propagate; otherwise table doesn't exist, leave undefined */
		free(lower);
		free(name);
		if (JS_IsExceptionPending(cx)) return JS_FALSE;
		return JS_TRUE;
	}

	sqlite_table_cache_node_t* node =
	    (sqlite_table_cache_node_t*)calloc(1, sizeof(*node));
	if (node == NULL) {
		free(lower); free(name);
		JS_ReportError(cx, "calloc failed");
		return JS_FALSE;
	}
	node->name_lower = lower;
	node->table_obj  = table_obj;
	node->next       = cp->table_cache;
	cp->table_cache  = node;

	JS_DefineProperty(cx, obj, name, OBJECT_TO_JSVAL(table_obj),
	    NULL, NULL, JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY);
	free(name);
	return JS_TRUE;
}

static JSBool js_sqlite_table_ns_enumerate(JSContext* cx, JSObject* obj)
{
	/* Only already-resolved (cached) tables enumerate. New tables won't
	 * appear until they've been accessed by name. */
	return JS_TRUE;
}

JSClass js_sqlite_table_ns_class = {
	"SQLiteTableNamespace"
	, 0
	, JS_PropertyStub
	, JS_PropertyStub
	, JS_PropertyStub
	, JS_StrictPropertyStub
	, js_sqlite_table_ns_enumerate
	, js_sqlite_table_ns_resolve
	, JS_ConvertStub
	, JS_FinalizeStub
};

static JSObject* new_table_ns_object(JSContext* cx, JSObject* conn_obj)
{
	JSObject* obj = JS_NewObject(cx, &js_sqlite_table_ns_class, NULL, NULL);
	if (obj == NULL) return NULL;
	JS_DefineProperty(cx, obj, ROOT_PROP_NAME, OBJECT_TO_JSVAL(conn_obj),
	    NULL, NULL, JSPROP_PERMANENT | JSPROP_READONLY);
#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj,
	    "Table accessor: <tt>db.table.<i>name</i></tt> resolves to a "
	    "SQLiteTable on first access (case-insensitive). Tables that "
	    "don't exist return <tt>undefined</tt>. Bracket form "
	    "(<tt>db.table[\"with-dash\"]</tt>) works for names that aren't "
	    "valid JS identifiers. Schema is cached per connection for the "
	    "lifetime of the database handle: DDL such as <tt>ALTER TABLE</tt>, "
	    "<tt>DROP TABLE</tt>, or <tt>CREATE TABLE</tt> issued after first "
	    "access is not reflected here, so scripts that need to see "
	    "post-DDL schema changes must reopen the connection.", 32200);
#endif
	return obj;
}

JSObject* js_CreateSQLiteClass(JSContext* cx, JSObject* parent)
{
	/* Install the log router once per process. pthread_once handles the
	 * race when multiple JS contexts initialize concurrently from
	 * different threads. Must run before any sqlite3_open (which
	 * implicitly calls sqlite3_initialize), and js_CreateSQLiteClass
	 * runs during JS-context setup before any script-driven open. */
	pthread_once(&sqlite_log_once, sqlite_log_install);

	JSObject* obj = JS_InitClass(cx, parent, NULL
	    , &js_sqlite_class
	    , js_sqlite_constructor
	    , 1
	    , NULL, NULL, NULL, NULL);
	if (obj == NULL) return obj;

	/* Register companion classes so their prototypes exist and instanceof
	 * works. Per-instance description / _property_desc_list are attached in
	 * the new_*_object helpers under #ifdef BUILD_JSDOCS. */
	JS_InitClass(cx, parent, NULL, &js_sqlite_stmt_class,     js_no_construct, 0, NULL, NULL, NULL, NULL);
	JS_InitClass(cx, parent, NULL, &js_sqlite_row_class,      js_no_construct, 0, NULL, NULL, NULL, NULL);
	JS_InitClass(cx, parent, NULL, &js_sqlite_value_class,    js_no_construct, 0, NULL, NULL, NULL, NULL);
	JS_InitClass(cx, parent, NULL, &js_sqlite_table_class,    js_no_construct, 0, NULL, NULL, NULL, NULL);
	JS_InitClass(cx, parent, NULL, &js_sqlite_table_ns_class, js_no_construct, 0, NULL, NULL, NULL, NULL);
	JS_InitClass(cx, parent, NULL, &js_sqlite_record_class,   js_no_construct, 0, NULL, NULL, NULL, NULL);

	/* Static constants on the SQLite constructor. */
	jsval     val;
	JSObject* ctor;
	if (JS_GetProperty(cx, parent, js_sqlite_class.name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JS_ValueToObject(cx, val, &ctor);
		struct { const char* name; int value; } consts[] = {
			{ "OK",                     SQLITE_OK },
			{ "ROW",                    SQLITE_ROW },
			{ "DONE",                   SQLITE_DONE },
			{ "BUSY",                   SQLITE_BUSY },
			{ "LOCKED",                 SQLITE_LOCKED },
			{ "READONLY",               SQLITE_READONLY },
			{ "CONSTRAINT",             SQLITE_CONSTRAINT },
			{ "MISMATCH",               SQLITE_MISMATCH },
			{ "INTEGER",                SQLITE_INTEGER },
			{ "FLOAT",                  SQLITE_FLOAT },
			{ "TEXT",                   SQLITE_TEXT },
			{ "BLOB",                   SQLITE_BLOB },
			{ "NULL_TYPE",              SQLITE_NULL },
			/* Extended constraint codes - the common ones scripts care
			 * about. Compare against db.extended_error_code. */
			{ "CONSTRAINT_CHECK",       SQLITE_CONSTRAINT_CHECK },
			{ "CONSTRAINT_FOREIGNKEY",  SQLITE_CONSTRAINT_FOREIGNKEY },
			{ "CONSTRAINT_NOTNULL",     SQLITE_CONSTRAINT_NOTNULL },
			{ "CONSTRAINT_PRIMARYKEY",  SQLITE_CONSTRAINT_PRIMARYKEY },
			{ "CONSTRAINT_UNIQUE",      SQLITE_CONSTRAINT_UNIQUE },
		};
		for (size_t i = 0; i < sizeof(consts) / sizeof(consts[0]); i++) {
			JS_DefineProperty(cx, ctor, consts[i].name, INT_TO_JSVAL(consts[i].value),
			    NULL, NULL, JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY);
		}
		js_DefineSyncMethods(cx, ctor, js_sqlite_static_functions);
#ifdef BUILD_JSDOCS
		js_CreateArrayOfStrings(cx, ctor, "_property_desc_list",
		    sqlite_const_desc, JSPROP_READONLY);
#endif
	}
	return obj;
}

#else /* !USE_SQLITE */

extern "C" JSObject* js_CreateSQLiteClass(JSContext* cx, JSObject* parent)
{
	(void)cx; (void)parent;
	return NULL;
}

#endif /* USE_SQLITE */
#endif /* JAVASCRIPT */
