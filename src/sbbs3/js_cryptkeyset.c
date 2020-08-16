/* $Id: js_cryptkeyset.c,v 1.5 2018/02/23 11:40:14 deuce Exp $ */

// Cyrptlib Keyset...

#include "sbbs.h"
#include <cryptlib.h>
#include "js_request.h"
#include "js_cryptcon.h"
#include "js_cryptcert.h"
#include "ssl.h"

struct private_data {
	CRYPT_KEYSET	ks;
	char			*name;
};

static JSClass js_cryptkeyset_class;
static const char* getprivate_failure = "line %d %s %s JS_GetPrivate failed";

// Helpers

// Destructor

static void
js_finalize_cryptkeyset(JSContext *cx, JSObject *obj)
{
	jsrefcount rc;
	struct private_data* p;

	if ((p=(struct private_data *)JS_GetPrivate(cx,obj))==NULL)
		return;

	rc = JS_SUSPENDREQUEST(cx);
	if (p->ks != CRYPT_UNUSED)
		cryptKeysetClose(p->ks);
	FREE_AND_NULL(p->name);
	free(p);
	JS_RESUMEREQUEST(cx, rc);

	JS_SetPrivate(cx, obj, NULL);
}

// Methods

static JSBool
js_add_private_key(JSContext *cx, uintN argc, jsval *arglist)
{
	struct private_data* p;
	struct js_cryptcon_private_data* ctx;
	jsval *argv=JS_ARGV(cx, arglist);
	char* pw = NULL;
	int status;
	jsrefcount rc;
	JSObject *key;
	JSString *jspw;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);

	if (!js_argc(cx, argc, 2))
		return JS_FALSE;
	if (argc > 2) {
		JS_ReportError(cx, "Too many arguments");
		return JS_FALSE;
	}
	key = JSVAL_TO_OBJECT(argv[0]);
	if (!JS_InstanceOf(cx, key, &js_cryptcon_class, NULL)) {
		JS_ReportError(cx, "Invalid CryptContext");
		return JS_FALSE;
	}

	if ((jspw = JS_ValueToString(cx, argv[1])) == NULL) {
		JS_ReportError(cx, "Invalid password");
		return JS_FALSE;
	}

	if ((p=(struct private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	if ((ctx=(struct js_cryptcon_private_data *)JS_GetPrivate(cx,key))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	JSSTRING_TO_MSTRING(cx, jspw, pw, NULL);
	HANDLE_PENDING(cx, pw);
	rc = JS_SUSPENDREQUEST(cx);
	status = cryptAddPrivateKey(p->ks, ctx->ctx, pw);
	free(pw);
	JS_RESUMEREQUEST(cx, rc);

	if (cryptStatusError(status)) {
		JS_ReportError(cx, "Error %d calling cryptAddPrivateKey()\n", status);
		return JS_FALSE;
	}
	return JS_TRUE;
}

static JSBool
js_add_public_key(JSContext *cx, uintN argc, jsval *arglist)
{
	struct private_data* p;
	struct js_cryptcert_private_data* pcert;
	jsval *argv=JS_ARGV(cx, arglist);
	int status;
	jsrefcount rc;
	JSObject *cert;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);

	if (!js_argc(cx, argc, 1))
		return JS_FALSE;
	if (argc > 1) {
		JS_ReportError(cx, "Too many arguments");
		return JS_FALSE;
	}
	cert = JSVAL_TO_OBJECT(argv[0]);
	if (!JS_InstanceOf(cx, cert, &js_cryptcert_class, NULL)) {
		JS_ReportError(cx, "Invalid CryptCert");
		return JS_FALSE;
	}

	if ((p=(struct private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	if ((pcert=(struct js_cryptcert_private_data *)JS_GetPrivate(cx,cert))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	status = cryptAddPublicKey(p->ks, pcert->cert);
	JS_RESUMEREQUEST(cx, rc);

	if (cryptStatusError(status)) {
		JS_ReportError(cx, "Error %d calling cryptAddPublicKey()\n", status);
		return JS_FALSE;
	}
	return JS_TRUE;
}

static JSBool
js_close(JSContext *cx, uintN argc, jsval *arglist)
{
	struct private_data* p;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsrefcount rc;
	int status;

	if (argc) {
		JS_ReportError(cx, "close() takes no arguments");
		return JS_FALSE;
	}
	if ((p=(struct private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}
	if (p->ks == CRYPT_UNUSED) {
		JS_ReportError(cx, "already closed");
		return JS_FALSE;
	}
	rc = JS_SUSPENDREQUEST(cx);
	status = cryptKeysetClose(p->ks);
	JS_RESUMEREQUEST(cx, rc);
	if (cryptStatusError(status)) {
		JS_ReportError(cx, "Error %d calling cryptKeysetClose()\n", status);
		return JS_FALSE;
	}

	return JS_TRUE;
}

static JSBool
js_delete_key(JSContext *cx, uintN argc, jsval *arglist)
{
	struct private_data* p;
	jsval *argv=JS_ARGV(cx, arglist);
	int status;
	jsrefcount rc;
	char* label = NULL;
	JSString *jslabel;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);

	if (!js_argc(cx, argc, 1))
		return JS_FALSE;
	if (argc > 1) {
		JS_ReportError(cx, "Too many arguments");
		return JS_FALSE;
	}
	if ((jslabel = JS_ValueToString(cx, argv[0])) == NULL) {
		JS_ReportError(cx, "Invalid label");
		return JS_FALSE;
	}

	if ((p=(struct private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	JSSTRING_TO_MSTRING(cx, jslabel, label, NULL);
	HANDLE_PENDING(cx, label);
	rc = JS_SUSPENDREQUEST(cx);
	status = cryptDeleteKey(p->ks, CRYPT_KEYID_NAME, label);
	free(label);
	JS_RESUMEREQUEST(cx, rc);
	if (cryptStatusError(status)) {
		JS_ReportError(cx, "Error %d calling cryptDeleteKey()\n", status);
		return JS_FALSE;
	}
	return JS_TRUE;
}

static JSBool
js_get_private_key(JSContext *cx, uintN argc, jsval *arglist)
{
	struct private_data* p;
	jsval *argv=JS_ARGV(cx, arglist);
	int status;
	jsrefcount rc;
	JSObject *key;
	char* pw = NULL;
	char* label = NULL;
	JSString *jspw;
	JSString *jslabel;
	CRYPT_CONTEXT ctx;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);

	if (!js_argc(cx, argc, 2))
		return JS_FALSE;
	if (argc > 2) {
		JS_ReportError(cx, "Too many arguments");
		return JS_FALSE;
	}
	if ((jslabel = JS_ValueToString(cx, argv[0])) == NULL) {
		JS_ReportError(cx, "Invalid label");
		return JS_FALSE;
	}
	if ((jspw = JS_ValueToString(cx, argv[1])) == NULL) {
		JS_ReportError(cx, "Invalid password");
		return JS_FALSE;
	}

	if ((p=(struct private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	JSSTRING_TO_MSTRING(cx, jslabel, label, NULL);
	HANDLE_PENDING(cx, label);
	JSSTRING_TO_MSTRING(cx, jspw, pw, NULL);
	if (JS_IsExceptionPending(cx)) {
		FREE_AND_NULL(label);
		FREE_AND_NULL(pw);
		return JS_FALSE;
	}
	rc = JS_SUSPENDREQUEST(cx);
	status = cryptGetPrivateKey(p->ks, &ctx, CRYPT_KEYID_NAME, label, pw);
	free(label);
	free(pw);
	JS_RESUMEREQUEST(cx, rc);
	if (cryptStatusError(status)) {
		JS_ReportError(cx, "Error %d calling cryptGetPrivateKey()\n", status);
		return JS_FALSE;
	}
	key = js_CreateCryptconObject(cx, ctx);
	if (key == NULL)
		return JS_FALSE;
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(key));

	return JS_TRUE;
}

static JSBool
js_get_public_key(JSContext *cx, uintN argc, jsval *arglist)
{
	struct private_data* p;
	jsval *argv=JS_ARGV(cx, arglist);
	int status;
	jsrefcount rc;
	JSObject *cert;
	char* label = NULL;
	JSString *jslabel;
	CRYPT_CERTIFICATE ncert;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);

	if (!js_argc(cx, argc, 1))
		return JS_FALSE;
	if (argc > 1) {
		JS_ReportError(cx, "Too many arguments");
		return JS_FALSE;
	}
	if ((jslabel = JS_ValueToString(cx, argv[0])) == NULL) {
		JS_ReportError(cx, "Invalid label");
		return JS_FALSE;
	}

	if ((p=(struct private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	JSSTRING_TO_MSTRING(cx, jslabel, label, NULL);
	HANDLE_PENDING(cx, label);
	rc = JS_SUSPENDREQUEST(cx);
	status = cryptGetPublicKey(p->ks, &ncert, CRYPT_KEYID_NAME, label);
	free(label);
	JS_RESUMEREQUEST(cx, rc);
	if (cryptStatusError(status)) {
		JS_ReportError(cx, "Error %d calling cryptGetPublicKey()\n", status);
		return JS_FALSE;
	}
	cert = js_CreateCryptCertObject(cx, ncert);
	if (cert == NULL)
		return JS_FALSE;
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(cert));

	return JS_TRUE;
}

// Properties

static JSBool
js_cryptkeyset_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval idval;
    jsint tiny;
	struct private_data* p;

	if ((p=(struct private_data *)JS_GetPrivate(cx,obj))==NULL) {
		return JS_TRUE;
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch (tiny) {
	}
	return JS_TRUE;
}

static JSBool
js_cryptkeyset_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsval idval;
    jsint tiny;
	struct private_data* p;

	if ((p=(struct private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch (tiny) {
	}
	return JS_TRUE;
}


#ifdef BUILD_JSDOCS
static char* cryptkeyset_prop_desc[] = {
	 "Keyopt constant (CryptKeyset.KEYOPT.XXX):<ul class=\"showList\">\n"
	 "<li>CryptKeyset.KEYOPT.NONE</li>\n"
	 "<li>CryptKeyset.KEYOPT.READONLY</li>\n"
	 "<li>CryptKeyset.KEYOPT.CREATE</li>\n"
	,NULL
};
#endif

static jsSyncPropertySpec js_cryptkeyset_properties[] = {
/*	name				,tinyid						,flags,				ver	*/
	{0}
};

static jsSyncMethodSpec js_cryptkeyset_functions[] = {
	{"add_private_key",	js_add_private_key,	0,	JSTYPE_VOID,	"CryptContext, password"
	,JSDOCSTR("Add a private key to the keyset, encrypting it with <password>.")
	,316
	},
	{"add_public_key",	js_add_public_key,	0,	JSTYPE_VOID,	"CryptCert"
	,JSDOCSTR("Add a public key (certificate) to the keyset.")
	,316
	},
	{"close",			js_close,			0,	JSTYPE_VOID,	""
	,JSDOCSTR("Close the keyset.")
	,316
	},
	{"delete_key",		js_delete_key,		0,	JSTYPE_VOID,	"label"
	,JSDOCSTR("Delete the key with <label> from the keyset.")
	,316
	},
	{"get_private_key",	js_get_private_key,	0,	JSTYPE_OBJECT,	"label, password"
	,JSDOCSTR("Returns a CryptContext from the private key with <label> encrypted with <password>.")
	,316
	},
	{"get_public_key",	js_get_public_key,	0,	JSTYPE_OBJECT,	"label"
	,JSDOCSTR("Returns a CryptCert from the public key with <label>.")
	,316
	},
	{0}
};

static JSBool js_cryptkeyset_resolve(JSContext *cx, JSObject *obj, jsid id)
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

	ret=js_SyncResolve(cx, obj, name, js_cryptkeyset_properties, js_cryptkeyset_functions, NULL, 0);
	if(name)
		free(name);
	return ret;
}

static JSBool js_cryptkeyset_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_cryptkeyset_resolve(cx, obj, JSID_VOID));
}

static JSClass js_cryptkeyset_class = {
     "CryptKeyset"				/* name			*/
    ,JSCLASS_HAS_PRIVATE		/* flags		*/
	,JS_PropertyStub			/* addProperty	*/
	,JS_PropertyStub			/* delProperty	*/
	,js_cryptkeyset_get			/* getProperty	*/
	,js_cryptkeyset_set			/* setProperty	*/
	,js_cryptkeyset_enumerate	/* enumerate	*/
	,js_cryptkeyset_resolve		/* resolve		*/
	,JS_ConvertStub				/* convert		*/
	,js_finalize_cryptkeyset	/* finalize		*/
};

// Constructor

static JSBool
js_cryptkeyset_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj;
	jsval *argv=JS_ARGV(cx, arglist);
	struct private_data *p;
	jsrefcount rc;
	int status;
	int opts = CRYPT_KEYOPT_NONE;
	JSString *fn;
	size_t fnslen;

	do_cryptInit();
	obj=JS_NewObject(cx, &js_cryptkeyset_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
	if(argc < 1 || argc > 2) {
		JS_ReportError(cx, "Incorrect number of arguments to CryptKeyset constructor.  Got %d, expected 1 or 2.", argc);
		return JS_FALSE;
	}
	if ((fn = JS_ValueToString(cx, argv[0])) == NULL)
		return JS_FALSE;
	if (argc == 2)
		if (!JS_ValueToInt32(cx,argv[1],&opts))
			return JS_FALSE;

	if((p=(struct private_data *)malloc(sizeof(struct private_data)))==NULL) {
		JS_ReportError(cx,"malloc failed");
		return(JS_FALSE);
	}
	memset(p,0,sizeof(struct private_data));

	if(!JS_SetPrivate(cx, obj, p)) {
		JS_ReportError(cx,"JS_SetPrivate failed");
		return(JS_FALSE);
	}

	JSSTRING_TO_MSTRING(cx, fn, p->name, &fnslen);
	if (p->name == NULL) {
		free(p);
		return JS_FALSE;
	}
	rc = JS_SUSPENDREQUEST(cx);
	status = cryptKeysetOpen(&p->ks, CRYPT_UNUSED, CRYPT_KEYSET_FILE, p->name, opts);
	JS_RESUMEREQUEST(cx, rc);
	if (cryptStatusError(status)) {
		JS_ReportError(cx, "CryptLib error %d", status);
		FREE_AND_NULL(p->name);
		free(p);
		return JS_FALSE;
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,obj,"Class used for storing CryptContext keys",31601);
	js_DescribeSyncConstructor(cx,obj,"To create a new CryptKeyset object: "
		"var c = new CryptKeyset('<i>filename</i>' [ <i>opts = CryptKeyset.KEYOPT.NONE</i> ])</tt><br> "
		"where <i>filename</i> is the name of the file to create, and <i>opts</i> "
		"is a value from CryptKeyset.KEYOPT"
		);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", cryptkeyset_prop_desc, JSPROP_READONLY);
#endif

	return(JS_TRUE);
}

JSObject* DLLCALL js_CreateCryptKeysetClass(JSContext* cx, JSObject* parent)
{
	JSObject*	cksobj;
	JSObject*	constructor;
	JSObject*	opts;
	jsval		val;

	cksobj = JS_InitClass(cx, parent, NULL
		,&js_cryptkeyset_class
		,js_cryptkeyset_constructor
		,1	/* number of constructor args */
		,NULL /* props, specified in constructor */
		,NULL /* funcs, specified in constructor */
		,NULL, NULL);

	if(JS_GetProperty(cx, parent, js_cryptkeyset_class.name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JS_ValueToObject(cx,val,&constructor);
		opts = JS_DefineObject(cx, constructor, "KEYOPT", NULL, NULL, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
		if(opts != NULL) {
			JS_DefineProperty(cx, opts, "NONE", INT_TO_JSVAL(CRYPT_KEYOPT_NONE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, opts, "READONLY", INT_TO_JSVAL(CRYPT_KEYOPT_READONLY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, opts, "CREATE", INT_TO_JSVAL(CRYPT_KEYOPT_CREATE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DeepFreezeObject(cx, opts);
		}
	}

	return(cksobj);
}
