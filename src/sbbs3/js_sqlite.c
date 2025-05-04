/* js_sqlite.c */

/* Synchronet JavaScript "Sqlite" Object */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2006 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://git.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Original source by Ragnarok. Heavily modified by Digital Man with ample 	*
 * help from nelgin.														*
 ****************************************************************************/

#include "sbbs.h"
#include "sqlite3.h"
#ifdef JAVASCRIPT

typedef struct
{
	sqlite3* db;    /* pointer to sqlite db */
	char name[MAX_PATH + 1];  /* db filename */
	char* stmt;   /* sql query */
	BOOL debug;
	char* errormsg;   /* last error message */

} private_t;

static const char* getprivate_failure = "line %d %s JS_GetPrivate failed";

static void dbprintf(BOOL error, private_t* p, char* fmt, ...)
{
	va_list argptr;
	char    sbuf[1024];

	if (p == NULL || (!p->debug && !error))
		return;

	va_start(argptr, fmt);
	vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1] = 0;
	va_end(argptr);
	lprintf(LOG_DEBUG, "Sqlite: %s", sbuf);
}


/* Sqlite Object Methods */

static JSBool
js_open(JSContext *cx, uintN argc, jsval *arglist)
{
	int        rc;
	private_t* p;
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if ((p = (private_t*)JS_GetPrivate(cx, obj)) == NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	if (p->db != NULL)  {
		dbprintf(FALSE, p, "db already open");
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
		return JS_TRUE;
	}
	else {
		dbprintf(FALSE, p, "trying open");
		rc = sqlite3_open(p->name, &p->db);

		if (rc) {
			sqlite3_close(p->db);
			JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
			p->errormsg = "can't open the database (path/permissions incorrect?)";
			dbprintf(FALSE, p, "can't open: %s", p->name);
			return JS_TRUE;
		}
	}
	dbprintf(FALSE, p, "opened!");
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	return JS_TRUE;
}


static JSBool
js_close(JSContext *cx, uintN argc, jsval *arglist)
{
	private_t* p;
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);

	if ((p = (private_t*)JS_GetPrivate(cx, obj)) == NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	if (p->db == NULL) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return JS_TRUE;
	}
	sqlite3_close(p->db);

	dbprintf(FALSE, p, "closed: %s", p->name);

	p->db = NULL;
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	return JS_TRUE;
}
static JSBool
js_exec(JSContext *cx, uintN argc, jsval *arglist)
{
	int           rc, i;
	private_t*    p;
	JSString*     str;
	JSObject*     Record;   // record object
	JSObject*     Result;   // array of records
	sqlite3_stmt *ppStmt;
	jsval         val;
	jsuint        idx;
	size_t        stmt_sz = 0;
	jsval *       argv = JS_ARGV(cx, arglist);
	JSObject *    parent = JS_THIS_OBJECT(cx, arglist);

	if ((p = (private_t*)JS_GetPrivate(cx, parent)) == NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}
	// use the parameter as query
	if (argc > 0) {
		dbprintf (FALSE, p, "exec has stmt");
		str = JS_ValueToString(cx, argv[0]);
		if (str != NULL) {
			JSSTRING_TO_RASTRING(cx, str, p->stmt, &stmt_sz, NULL);
			dbprintf (FALSE, p, "exec param: %s", p->stmt);
		}
	}
	if (p->db == NULL) {
		dbprintf(TRUE, p, "database is not opened");
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return JS_TRUE;
	}

	dbprintf(FALSE, p, "create Result object");
	if ((Result = JS_NewArrayObject(cx, 0, NULL)) == NULL)
		return JS_FALSE;

	if (p->stmt == NULL) {
		dbprintf(FALSE, p, "empy statement");
		p->errormsg = "empty statement";
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return JS_TRUE;
	}


	dbprintf (FALSE, p, "prepare: %s", p->stmt);
	rc = sqlite3_prepare(p->db, p->stmt, -1, &ppStmt, NULL);

	if (rc == SQLITE_OK) {
		while (sqlite3_step(ppStmt) == SQLITE_ROW) {
			dbprintf(FALSE, p, "create record object");
			if ((Record = JS_NewObject(cx, NULL, NULL, NULL)) == NULL)
				return JS_FALSE;

			for (i = 0; i < sqlite3_column_count(ppStmt); i++) {

				switch (sqlite3_column_type(ppStmt, i)) {
					case SQLITE_INTEGER:
						val = INT_TO_JSVAL(sqlite3_column_int(ppStmt, i));
						break;
					case SQLITE_TEXT:
						val = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, (const char *)sqlite3_column_text(ppStmt, i)));
						break;
					case SQLITE_BLOB:
						break;
				}

				if (!JS_SetProperty(cx, Record, sqlite3_column_name(ppStmt, i), &val))
					return JS_FALSE;
			}
			dbprintf (FALSE, p, "adding element to the Result");
			val = OBJECT_TO_JSVAL(Record);
			idx = -1;

			if (!JS_GetArrayLength(cx, Result, &idx))
				return JS_FALSE;

			if (!JS_SetElement(cx, Result, idx, &val))
				return JS_FALSE;
		}
		dbprintf(FALSE, p, "end prepare");
		JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(Result));
		return JS_TRUE;
	}
	else {
		p->errormsg = (char* ) sqlite3_errmsg(p->db);
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		dbprintf(FALSE, p, "prepare error: %s", sqlite3_errmsg(p->db));
		return JS_TRUE;
	}
}

/* Sqlite Object Properites */
enum {
	SQLITE_PROP_NAME
	, SQLITE_PROP_STMT
	, SQLITE_PROP_DEBUG
	, SQLITE_PROP_ERRORMSG
};


static JSBool js_sqlite_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsint      tiny;
	private_t* p;
	int32      intval = 0;
	JSString*  js_str;
	size_t     js_str_sz = 0;
	jsval      idval;

	if ((p = (private_t*)JS_GetPrivate(cx, obj)) == NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	JS_IdToValue(cx, id, &idval);
	tiny = JSVAL_TO_INT(idval);

	if (JSVAL_IS_NUMBER(*vp))
		JS_ValueToInt32(cx, *vp, &intval);
	else if (JSVAL_IS_BOOLEAN(*vp))
		JS_ValueToBoolean(cx, *vp, &intval);

	dbprintf(FALSE, p, "setting property %d", tiny);

	switch (tiny) {
		case SQLITE_PROP_STMT:
			if ((js_str = JS_ValueToString(cx, *vp)) == NULL)
				return JS_FALSE;
			JSSTRING_TO_RASTRING(cx, js_str, p->stmt, &js_str_sz, NULL);
			break;
		case SQLITE_PROP_DEBUG:
			p->debug = intval;
			break;
	}

	return JS_TRUE;
}

static JSBool js_sqlite_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	private_t* p;
	jsint      tiny;
	JSString*  js_str = NULL;
	jsval      idval;

	if ((p = (private_t*)JS_GetPrivate(cx, obj)) == NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	JS_IdToValue(cx, id, &idval);
	tiny = JSVAL_TO_INT(idval);

	dbprintf(FALSE, p, "getting property %d", tiny);

	switch (tiny) {
		case SQLITE_PROP_NAME:
			if ((js_str = JS_NewStringCopyZ(cx, p->name)) == NULL)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str);
			break;
		case SQLITE_PROP_STMT:
			if ((js_str = JS_NewStringCopyZ(cx, p->stmt)) == NULL)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str);
			break;
		case SQLITE_PROP_DEBUG:
			*vp = BOOLEAN_TO_JSVAL(p->debug);
			break;
		case SQLITE_PROP_ERRORMSG:
			if ((js_str = JS_NewStringCopyZ(cx, p->errormsg)) == NULL)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str);
			break;
	}

	return JS_TRUE;
}

#define SQLITE_PROP_FLAGS JSPROP_ENUMERATE
#define SQLITE_PROP_ROFLAGS JSPROP_ENUMERATE | JSPROP_READONLY

static jsSyncPropertySpec js_sqlite_properties[] = {
/*		 name				,tinyid					,flags,				ver	*/
	{   "name", SQLITE_PROP_NAME, SQLITE_PROP_ROFLAGS,  321},
	{   "stmt", SQLITE_PROP_STMT, JSPROP_ENUMERATE,  321},
	{   "debug", SQLITE_PROP_DEBUG, JSPROP_ENUMERATE,  321},
	{   "errormsg", SQLITE_PROP_ERRORMSG, SQLITE_PROP_ROFLAGS,  321},
	{0}
};

#ifdef BUILD_JSDOCS
static char*            sqlite_prop_desc[] = {
	"filename specified in constructor - <small>READ ONLY</small>"
	, "string sql statement"
	, "set to <i>true</i> to enable debug log output"
	, "get the last error message - <small>READ ONLY</small>"
	, NULL
};
#endif


static jsSyncMethodSpec js_sqlite_functions[] = {
	{"open",            js_open,            1,  JSTYPE_BOOLEAN, JSDOCSTR("Open the database")
	 , JSDOCSTR("open the sqlite3 database")
	 , 321},
	{"close",           js_close,           0,  JSTYPE_VOID,    JSDOCSTR("")
	 , JSDOCSTR("close database")
	 , 321},
	{"exec",            js_exec,            0,  JSTYPE_BOOLEAN, JSDOCSTR("")
	 , JSDOCSTR("exec the sql query on database")
	 , 321},
	{0}
};

/* File Destructor */

static void js_finalize_sqlite(JSContext *cx, JSObject *obj)
{
	private_t* p;

	if ((p = (private_t*)JS_GetPrivate(cx, obj)) == NULL)
		return;

	if (p->db != NULL)
		sqlite3_close(p->db);

	dbprintf(FALSE, p, "finalize Sqlite object");

	free(p->stmt);
	free(p);

	JS_SetPrivate(cx, obj, NULL);
}

static JSClass js_sqlite_class = {
	"Sqlite"                /* name			*/
	, JSCLASS_HAS_PRIVATE    /* flags		*/
	, JS_PropertyStub        /* addProperty	*/
	, JS_PropertyStub        /* delProperty	*/
	, js_sqlite_get          /* getProperty	*/
	, js_sqlite_set          /* setProperty	*/
	, JS_EnumerateStub       /* enumerate	*/
	, JS_ResolveStub         /* resolve		*/
	, JS_ConvertStub         /* convert		*/
	, js_finalize_sqlite     /* finalize		*/
};

/* Sqlite Constructor (creates database object) */

static JSBool
js_sqlite_constructor(JSContext* cx, uintN argc, jsval *arglist)
{
	JSString*  str;
	private_t* p;
	jsval *    argv = JS_ARGV(cx, arglist);
	JSObject*  obj;


	obj = JS_NewObject(cx, &js_sqlite_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));

	if ((str = JS_ValueToString(cx, argv[0])) == NULL) {
		JS_ReportError(cx, "No filename specified");
		return JS_FALSE;
	}

	if ((p = (private_t*)calloc(1, sizeof(private_t))) == NULL) {
		JS_ReportError(cx, "calloc failed");
		return JS_FALSE;
	}

	p->errormsg = "";

	JSSTRING_TO_STRBUF(cx, str, p->name, sizeof(p->name), NULL);

	if (!JS_SetPrivate(cx, obj, p)) {
		dbprintf(TRUE, p, "JS_SetPrivate failed\n");
		return JS_FALSE;
	}

	if (!js_DefineSyncProperties(cx, obj, js_sqlite_properties)) {
		dbprintf(TRUE, p, "js_DefineSyncProperties failed\n");
		return JS_FALSE;
	}

	if (!js_DefineSyncMethods(cx, obj, js_sqlite_functions /*, FALSE - only needs 3 parameters */)) {
		dbprintf(TRUE, p, "js_DefineSyncMethods failed\n");
		return JS_FALSE;
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj, "Can used to manipulate sqlite database"
	                      , 321
	                      );
	js_DescribeSyncConstructor(cx, obj, "To create a new Sqlite object: <tt>var f = new Sqlite(<i>filename</i>)</tt>");
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", sqlite_prop_desc, JSPROP_READONLY);
#endif

	dbprintf(FALSE, p, "object constructed\n");
	return JS_TRUE;
}

JSObject* js_CreateSqliteClass(JSContext* cx, JSObject* parent)
{
	return JS_InitClass(cx, parent, NULL
	                    , &js_sqlite_class
	                    , js_sqlite_constructor
	                    , 1 /* number of constructor args */
	                    , NULL /* props, set in constructor */
	                    , NULL /* funcs, set in constructor */
	                    , NULL, NULL);
}

#endif  /* JAVASCRIPT */
