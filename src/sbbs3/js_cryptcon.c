/* $Id: js_cryptcon.c,v 1.20 2018/04/06 02:42:37 rswindell Exp $ */

// Cyrptlib encryption context...

#include "sbbs.h"
#include <cryptlib.h>
#include "js_request.h"
#include "js_cryptcon.h"
#include "ssl.h"
#include "base64.h"

JSClass js_cryptcon_class;
static const char* getprivate_failure = "line %d %s %s JS_GetPrivate failed";

// Helpers
static void
js_cryptcon_error(JSContext *cx, CRYPT_CONTEXT ctx, int error)
{
	char *errstr;
	int errlen;

	if (cryptGetAttributeString(ctx, CRYPT_ATTRIBUTE_ERRORMESSAGE, NULL, &errlen) != CRYPT_OK) {
		JS_ReportError(cx, "CryptLib error %d", error);
		return;
	}
	if ((errstr = (char *)malloc(errlen+1)) == NULL) {
		JS_ReportError(cx, "CryptLib error %d", error);
		return;
	}
	if (cryptGetAttributeString(ctx, CRYPT_ATTRIBUTE_ERRORMESSAGE, errstr, &errlen) != CRYPT_OK) {
		free(errstr);
		JS_ReportError(cx, "CryptLib error %d", error);
		return;
	}
	errstr[errlen+1] = 0;

	JS_ReportError(cx, "Cryptlib error %d (%s)", error, errstr);
	free(errstr);
}

static size_t js_asn1_len(unsigned char *data, size_t len, size_t *off)
{
	size_t sz = 0;
	size_t bytes;
	size_t i;

	if (*off >= len)
		return 0;

	if ((data[*off] & 0x80) == 0) {
		
		sz = data[*off];
		(*off)++;
	}
	else if(data[*off] == 0x80) {
		// We can't actually handle this when we're this simple.
		(*off)++;
	}
	else {
		bytes = data[(*off)++] & 0x7f;
		for (i = 0; i < bytes && *off < len; i++) {
			sz <<= 8;
			sz |= data[(*off)++];
		}
	}

	return sz;
}

static unsigned char js_asn1_type(unsigned char *data, size_t len, size_t *off)
{
	unsigned char t = 0;

	if ((data[*off] & 0x1f) == 0x1f) {
		for ((*off)++; *off < len && data[*off]; (*off)++) {
			if ((data[*off] & 0x80) == 0)
				break;
		}
	}
	else {
		t = data[*off];
		(*off)++;
	}

	return t;
}

static int js_ecc_to_prop(unsigned char *data, size_t len, size_t *off, JSContext *cx, JSObject *parent)
{
	size_t sz;
	JSObject *obj;
	JSString* xstr;
	JSString* ystr;
	char *x;
	char *y;
	unsigned char *z;
	size_t zcnt;
	char *x64;
	char *y64;
	size_t half;

	if (js_asn1_type(data, len, off) == 3) {
		sz = js_asn1_len(data,len,off);
		if (data[*off] == 0 && data[(*off)+1] == 4 && ((sz%1)==0)) {
			half = (sz - 2) / 2;
			x = malloc(half);
			if (x == NULL)
				return 0;
			for (z=data+(*off)+2, zcnt=half; *z==0 && half; z++, zcnt--);
			memcpy(x, z, zcnt);
			x64 = malloc(zcnt*4/3+3);
			if (x64 == NULL) {
				free(x);
				return 0;
			}
			b64_encode(x64, zcnt*4/3+3, x, zcnt);
			free(x);
			for (x=x64; *x; x++) {
				if (*x == '+')
					*x = '-';
				else if (*x == '/')
					*x = '_';
				else if (*x == '=')
					*x = 0;
			}

			y = malloc(half);
			if (y == NULL)
				return 0;
			for (z=data+(*off)+2+half, zcnt=half; *z==0 && half; z++, zcnt--);
			memcpy(y, z, zcnt);
			y64 = malloc(zcnt*4/3+3);
			if (y64 == NULL) {
				free(y);
				return 0;
			}
			b64_encode(y64, zcnt*4/3+3, y, zcnt);
			free(y);
			for (y=y64; *y; y++) {
				if (*y == '+')
					*y = '-';
				else if (*y == '/')
					*y = '_';
				else if (*y == '=')
					*y = 0;
			}

			obj=JS_NewObject(cx, NULL, NULL, parent);
			JS_DefineProperty(cx, parent, "public_key", OBJECT_TO_JSVAL(obj), NULL, NULL, JSPROP_ENUMERATE|JSPROP_READONLY);
			xstr=JS_NewStringCopyZ(cx, x64);
			free(x64);
			if (xstr != NULL)
				JS_DefineProperty(cx, obj, "x", STRING_TO_JSVAL(xstr), NULL, NULL, JSPROP_ENUMERATE|JSPROP_READONLY);
			ystr=JS_NewStringCopyZ(cx, y64);
			free(y64);
			if (ystr != NULL)
				JS_DefineProperty(cx, obj, "y", STRING_TO_JSVAL(ystr), NULL, NULL, JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DeepFreezeObject(cx, obj);
			return 1;
		}
	}
	return 0;
}

static void js_simple_asn1(unsigned char *data, size_t len, JSContext *cx, JSObject *parent)
{
	unsigned char t;
	size_t off=0;
	size_t off2;
	size_t sz;
	char *e;
	char *n;
	char *e64;
	char *n64;
	JSObject *obj;
	JSString* estr;
	JSString* nstr;

	while(off < len) {
		/* Parse identifier */
		t = js_asn1_type(data, len, &off);

		/* Parse length */
		sz = js_asn1_len(data, len, &off);

		switch(t) {
			case 48:
				// Sequence, descend into it.
				break;
			case 6:
				// OID.... check if it's PKCS #1
				if (strncmp((char *)data+off, "\x2a\x86\x48\x86\xF7\x0D\x01\x01\x01", 9)==0) {
					// YEP!
					off += sz;
					for (; off < len;) {
						off2 = off;
						t = js_asn1_type(data, len, &off2);
						if (t == 2)
							break;
						off = off2;
						sz = js_asn1_len(data,len,&off);
						if (t != 48 && t != 3)
							off += sz;
						if (t == 3)
							off++;
					}
					if (off >= len)
						return;
					if (js_asn1_type(data, len, &off) == 2) {
						sz = js_asn1_len(data,len,&off);
						n = malloc(sz);
						if (n == NULL)
							return;
						while(data[off] == 0) {
							off++;
							sz--;
						}
						memcpy(n, data+off, sz);
						n64 = malloc(sz*4/3+3);
						if (n64 == NULL) {
							free(n);
							return;
						}
						b64_encode(n64, sz*4/3+3, n, sz);
						free(n);
						for (n=n64; *n; n++) {
							if (*n == '+')
								*n = '-';
							else if (*n == '/')
								*n = '_';
							else if (*n == '=')
								*n = 0;
						}
						off += sz;
						if (js_asn1_type(data, len, &off) != 2) {
							free(n64);
							return;
						}
						sz = js_asn1_len(data,len,&off);
						e = malloc(sz);
						if (e == NULL) {
							free(n64);
							return;
						}
						while(data[off] == 0) {
							off++;
							sz--;
						}
						memcpy(e, data+off, sz);
						e64 = malloc(sz*4/3+3);
						if (e64 == NULL) {
							free(e);
							free(n64);
							return;
						}
						b64_encode(e64, sz*4/3+3, e, sz);
						free(e);
						for (e=e64; *e; e++) {
							if (*e == '+')
								*e = '-';
							else if (*e == '/')
								*e = '_';
							else if (*e == '=')
								*e = 0;
						}
						obj=JS_NewObject(cx, NULL, NULL, parent);
						JS_DefineProperty(cx, parent, "public_key", OBJECT_TO_JSVAL(obj), NULL, NULL, JSPROP_ENUMERATE|JSPROP_READONLY);
						nstr=JS_NewStringCopyZ(cx, n64);
						free(n64);
						if (nstr != NULL)
							JS_DefineProperty(cx, obj, "n", STRING_TO_JSVAL(nstr), NULL, NULL, JSPROP_ENUMERATE|JSPROP_READONLY);
						estr=JS_NewStringCopyZ(cx, e64);
						free(e64);
						if (estr != NULL)
							JS_DefineProperty(cx, obj, "e", STRING_TO_JSVAL(estr), NULL, NULL, JSPROP_ENUMERATE|JSPROP_READONLY);
						JS_DeepFreezeObject(cx, obj);
					}
					off = len;
				}
				else if (strncmp((char *)data+off, "\x2A\x86\x48\xCE\x3D\x03\x01\x07", 8) == 0) {
					// P-256
					off += sz;
					if (js_ecc_to_prop(data, len, &off, cx, parent))
						off = len;
				}
				else if (strncmp((char *)data+off, "\x2B\x81\x04\x00\x22", 5) == 0) {
					// P-384
					off += sz;
					if (js_ecc_to_prop(data, len, &off, cx, parent))
						off = len;
				}
				else if (strncmp((char *)data+off, "\x2B\x81\x04\x00\x23", 5) == 0) {
					// P-521
					off += sz;
					if (js_ecc_to_prop(data, len, &off, cx, parent))
						off = len;
				}
				if (off < len)
					off += sz;
				break;
			default:
				off += sz;
				break;
		}
	}
}

static void js_create_key_object(JSContext *cx, JSObject *parent)
{
	struct js_cryptcon_private_data* p;
	jsrefcount rc;
	int status;
	int val;
	int sz;
	CRYPT_CERTIFICATE cert;	// Just to hold the public key...
	unsigned char *certbuf;

	if ((p=(struct js_cryptcon_private_data *)JS_GetPrivate(cx,parent))==NULL)
		return;

	rc = JS_SUSPENDREQUEST(cx);

	status = cryptGetAttribute(p->ctx, CRYPT_CTXINFO_ALGO, &val);
	if (status != CRYPT_OK) {
		lprintf(LOG_ERR, "cryptGetAttribute(ALGO) returned %d\n", status);
		goto resume;
	}
	if (val != CRYPT_ALGO_RSA && val != CRYPT_ALGO_ECDSA)
		goto resume;

	status = cryptCreateCert(&cert, CRYPT_UNUSED, CRYPT_CERTTYPE_CERTIFICATE);
	if (status != CRYPT_OK) {
		lprintf(LOG_ERR, "cryptCreateCert() returned %d\n", status);
		goto resume;
	}
	status = cryptSetAttribute(cert, CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, p->ctx);
	if (status != CRYPT_OK) {
		lprintf(LOG_ERR, "cryptSetAttribute(PKI) returned %d\n", status);
		goto resume;
	}
	status = cryptSetAttribute(cert, CRYPT_CERTINFO_XYZZY, 1);
	if (status != CRYPT_OK) {
		lprintf(LOG_ERR, "cryptSetAttribute(XYZZY) returned %d\n", status);
		goto resume;
	}
	status = cryptSetAttributeString(cert, CRYPT_CERTINFO_COMMONNAME, "Key", 3);
	if (status != CRYPT_OK) {
		lprintf(LOG_ERR, "cryptSetAttributeString(CN) returned %d\n", status);
		goto resume;
	}
	status = cryptSignCert(cert, p->ctx);
	if (status != CRYPT_OK) {
		lprintf(LOG_ERR, "cryptSignCert() returned %d\n", status);
		goto resume;
	}
	status = cryptExportCert(NULL, 0, &sz, CRYPT_CERTFORMAT_CERTIFICATE, cert);
	if (status != CRYPT_OK) {
		lprintf(LOG_ERR, "cryptExportCert(NULL) returned %d\n", status);
		goto resume;
	}
	certbuf = malloc(sz);
	if (certbuf == NULL) {
		lprintf(LOG_ERR, "Unable to allocate %d bytes\n", sz);
		goto resume;
	}
	status = cryptExportCert(certbuf, sz, &sz, CRYPT_CERTFORMAT_CERTIFICATE, cert);
	if (status != CRYPT_OK) {
		lprintf(LOG_ERR, "cryptExportCert(certbuf) returned %d\n", status);
		goto resume;
	}
	cryptDestroyCert(cert);

	js_simple_asn1(certbuf, sz, cx, parent);
	free(certbuf);

resume:
	JS_RESUMEREQUEST(cx, rc);
	return;
}

// Destructor

static void
js_finalize_cryptcon(JSContext *cx, JSObject *obj)
{
	struct js_cryptcon_private_data* p;

	if ((p=(struct js_cryptcon_private_data *)JS_GetPrivate(cx,obj))==NULL)
		return;

	cryptDestroyContext(p->ctx);
	free(p);

	JS_SetPrivate(cx, obj, NULL);
}

// Methods

static JSBool
js_generate_key(JSContext *cx, uintN argc, jsval *arglist)
{
	struct js_cryptcon_private_data* p;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsrefcount rc;
	int status;

	if ((p=(struct js_cryptcon_private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	status = cryptGenerateKey(p->ctx);
	JS_RESUMEREQUEST(cx, rc);
	if (status == CRYPT_OK)
		js_create_key_object(cx, obj);
	if (cryptStatusError(status)) {
		js_cryptcon_error(cx, p->ctx, status);
		return JS_FALSE;
	}
	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool
js_set_key(JSContext *cx, uintN argc, jsval *arglist)
{
	struct js_cryptcon_private_data* p;
	JSObject *obj;
	jsval *argv;
	size_t len;
	char* key = NULL;
	int status;
	jsrefcount rc;

	if (!js_argc(cx, argc, 1))
		return JS_FALSE;

	argv = JS_ARGV(cx, arglist);

	JSVALUE_TO_MSTRING(cx, argv[0], key, &len);
	HANDLE_PENDING(cx, key);
	if (key == NULL)
		return JS_FALSE;

	obj = JS_THIS_OBJECT(cx, arglist);
	if ((p=(struct js_cryptcon_private_data *)JS_GetPrivate(cx,obj))==NULL) {
		free(key);
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	status = cryptSetAttributeString(p->ctx, CRYPT_CTXINFO_KEY, key, len);
	free(key);
	JS_RESUMEREQUEST(cx, rc);
	if (cryptStatusError(status)) {
		js_cryptcon_error(cx, p->ctx, status);
		return JS_FALSE;
	}
	js_create_key_object(cx, obj);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool
js_derive_key(JSContext *cx, uintN argc, jsval *arglist)
{
	struct js_cryptcon_private_data* p;
	JSObject *obj;
	jsval *argv;
	size_t len;
	char* key = NULL;
	int status;
	jsrefcount rc;

	if (!js_argc(cx, argc, 1))
		return JS_FALSE;

	argv = JS_ARGV(cx, arglist);

	JSVALUE_TO_MSTRING(cx, argv[0], key, &len);
	HANDLE_PENDING(cx, key);
	if (key == NULL)
		return JS_FALSE;

	if (len < 8 || len > CRYPT_MAX_HASHSIZE) {
		free(key);
		JS_ReportError(cx, "Illegal key value length of %d (must be between 8 and %d)", len, CRYPT_MAX_HASHSIZE);
		return JS_FALSE;
	}

	obj = JS_THIS_OBJECT(cx, arglist);
	if ((p=(struct js_cryptcon_private_data *)JS_GetPrivate(cx,obj))==NULL) {
		free(key);
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	status = cryptSetAttributeString(p->ctx, CRYPT_CTXINFO_KEYING_VALUE, key, len);
	free(key);
	JS_RESUMEREQUEST(cx, rc);
	if (cryptStatusError(status)) {
		js_cryptcon_error(cx, p->ctx, status);
		return JS_FALSE;
	}
	js_create_key_object(cx, obj);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool
js_do_encrption(JSContext *cx, uintN argc, jsval *arglist, int encrypt)
{
	struct js_cryptcon_private_data* p;
	JSObject *obj;
	jsval *argv;
	size_t len;
	char *cipherText = NULL;
	int status;
	jsrefcount rc;
	JSString* str;

	if (!js_argc(cx, argc, 1))
		return JS_FALSE;

	argv = JS_ARGV(cx, arglist);

	obj = JS_THIS_OBJECT(cx, arglist);
	if ((p=(struct js_cryptcon_private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	JSVALUE_TO_MSTRING(cx, argv[0], cipherText, &len);
	HANDLE_PENDING(cx, cipherText);
	if (cipherText == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	if (encrypt)
		status = cryptEncrypt(p->ctx, cipherText, len);
	else
		status = cryptDecrypt(p->ctx, cipherText, len);
	JS_RESUMEREQUEST(cx, rc);
	if (cryptStatusError(status)) {
		free(cipherText);
		js_cryptcon_error(cx, p->ctx, status);
		return JS_FALSE;
	}
	str = JS_NewStringCopyN(cx, cipherText, len);
	free(cipherText);
	if(str==NULL)
		return(JS_FALSE);
	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));
	return JS_TRUE;
}


static JSBool
js_encrypt(JSContext *cx, uintN argc, jsval *arglist)
{
	return js_do_encrption(cx, argc, arglist, TRUE);
}

static JSBool
js_decrypt(JSContext *cx, uintN argc, jsval *arglist)
{
	return js_do_encrption(cx, argc, arglist, FALSE);
}

static JSBool
js_create_signature(JSContext *cx, uintN argc, jsval *arglist)
{
	struct js_cryptcon_private_data* p;
	struct js_cryptcon_private_data* scp;
	JSObject *sigCtx;
	JSObject *obj;
	jsval *argv;
	int len;
	char *signature = NULL;
	int status;
	jsrefcount rc;
	JSString* str;

	if (!js_argc(cx, argc, 1))
		return JS_FALSE;

	argv = JS_ARGV(cx, arglist);

	obj = JS_THIS_OBJECT(cx, arglist);
	if ((p=(struct js_cryptcon_private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}
	sigCtx = JSVAL_TO_OBJECT(argv[0]);
	if (!JS_InstanceOf(cx, sigCtx, &js_cryptcon_class, NULL))
		return JS_FALSE;
	HANDLE_PENDING(cx, NULL);
	if ((scp=(struct js_cryptcon_private_data *)JS_GetPrivate(cx,sigCtx))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	status = cryptCreateSignature(NULL, 0, &len, scp->ctx, p->ctx);
	if (cryptStatusError(status)) {
		JS_RESUMEREQUEST(cx, rc);
		js_cryptcon_error(cx, p->ctx, status);
		return JS_FALSE;
	}
	signature = malloc(len);
	if (signature == NULL) {
		lprintf(LOG_ERR, "Unable to allocate %u bytes\n", len);
		JS_RESUMEREQUEST(cx, rc);
		return JS_FALSE;
	}
	status = cryptCreateSignature(signature, len, &len, scp->ctx, p->ctx);
	JS_RESUMEREQUEST(cx, rc);
	if (cryptStatusError(status)) {
		free(signature);
		js_cryptcon_error(cx, p->ctx, status);
		return JS_FALSE;
	}
	str = JS_NewStringCopyN(cx, signature, len);
	free(signature);
	if(str==NULL)
		return(JS_FALSE);
	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));
	return JS_TRUE;
}

// Properties

enum {
	 CRYPTCON_PROP_ALGO
	,CRYPTCON_PROP_BLOCKSIZE
	,CRYPTCON_PROP_HASHVALUE
	,CRYPTCON_PROP_IV
	,CRYPTCON_PROP_IVSIZE
	,CRYPTCON_PROP_KEYING_ALGO
	,CRYPTCON_PROP_KEYING_ITERATIONS
	,CRYPTCON_PROP_KEYING_SALT
	,CRYPTCON_PROP_KEYSIZE
	,CRYPTCON_PROP_LABEL
	,CRYPTCON_PROP_MODE
	,CRYPTCON_PROP_NAME_ALGO
	,CRYPTCON_PROP_NAME_MODE
};

#ifdef BUILD_JSDOCS
static char* cryptcon_prop_desc[] = {
	 "Algorithm constant (CryptContext.ALGO.XXX):<ul class=\"showList\">\n"
	 "<li>CryptContext.ALGO.NONE</li>\n"
	 "<li>CryptContext.ALGO.DES</li>\n"
	 "<li>CryptContext.ALGO.3DES</li>\n"
	 "<li>CryptContext.ALGO.IDEA</li>\n"
	 "<li>CryptContext.ALGO.CAST</li>\n"
	 "<li>CryptContext.ALGO.RC2</li>\n"
	 "<li>CryptContext.ALGO.RC4</li>\n"
	 "<li>CryptContext.ALGO.AES</li>\n"
	 "<li>CryptContext.ALGO.DH</li>\n"
	 "<li>CryptContext.ALGO.RSA</li>\n"
	 "<li>CryptContext.ALGO.DSA</li>\n"
	 "<li>CryptContext.ALGO.ELGAMAL</li>\n"
	 "<li>CryptContext.ALGO.ECDSA</li>\n"
	 "<li>CryptContext.ALGO.ECDH</li>\n"
	 "<li>CryptContext.ALGO.MD5</li>\n"
	 "<li>CryptContext.ALGO.SHA1</li>\n"
	 "<li>CryptContext.ALGO.SHA2</li>\n"
	 "<li>CryptContext.ALGO.SHAng</li>\n"
	 "<li>CryptContext.ALGO.HMAC-SHA1</li>\n"
	 "<li>CryptContext.ALGO.HMAC-SHA2</li>\n"
	 "<li>CryptContext.ALGO.HMAC-SHAng</li></ul>"
	,"Cipher block size in bytes"
	,"Output of hasing algorithms (ie: MD5, SHA1, etc)"
	,"Cipher IV"
	,"Cipher IV size in bytes"
	,"The keying algorithm used to derive the key"
	,"The number of iterates used to derive the key"
	,"The salt value used to derive an encryption key from a key (Length must be between 8 and 64)"
	,"Key size in bytes"
	,"Key label"
	,"Mode constant (CryptContext.MODE.XXX):<ul class=\"showList\">\n"
	 "<li>CryptContext.MODE.None</li>\n"
	 "<li>CryptContext.MODE.ECB</li>\n"
	 "<li>CryptContext.MODE.CBC</li>\n"
	 "<li>CryptContext.MODE.CFB</li>\n"
	 "<li>CryptContext.MODE.GCM</li></ul>"
	,"Algorithm name"
	,"Mode name"
	,NULL
};
#endif

static JSBool
js_cryptcon_attr_set(JSContext *cx, jsval *vp, CRYPT_CONTEXT ctx, CRYPT_ATTRIBUTE_TYPE type)
{
	int status;
	int val;

	if (!JS_ValueToInt32(cx, *vp, &val))
		return JS_FALSE;
	status = cryptSetAttribute(ctx, type, val);
	if (cryptStatusError(status)) {
		js_cryptcon_error(cx, ctx, status);
		return JS_FALSE;
	}
	return JS_TRUE;
}

static JSBool
js_cryptcon_attrstr_set(JSContext *cx, jsval *vp, CRYPT_CONTEXT ctx, CRYPT_ATTRIBUTE_TYPE type)
{
	int status;
	char *val = NULL;
	size_t len;

	JSVALUE_TO_MSTRING(cx, *vp, val, &len);
	HANDLE_PENDING(cx, val);
	if (val == NULL)
		return JS_FALSE;

	status = cryptSetAttributeString(ctx, type, val, len);
	if (cryptStatusError(status)) {
		js_cryptcon_error(cx, ctx, status);
		return JS_FALSE;
	}
	return JS_TRUE;
}

static JSBool
js_cryptcon_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsval idval;
    jsint tiny;
	struct js_cryptcon_private_data* p;

	if ((p=(struct js_cryptcon_private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch (tiny) {
		case CRYPTCON_PROP_ALGO:
			return js_cryptcon_attr_set(cx, vp, p->ctx, CRYPT_CTXINFO_ALGO);
		case CRYPTCON_PROP_BLOCKSIZE:
			return js_cryptcon_attr_set(cx, vp, p->ctx, CRYPT_CTXINFO_BLOCKSIZE);
		case CRYPTCON_PROP_IVSIZE:
			return js_cryptcon_attr_set(cx, vp, p->ctx, CRYPT_CTXINFO_IVSIZE);
		case CRYPTCON_PROP_KEYING_ALGO:
			return js_cryptcon_attr_set(cx, vp, p->ctx, CRYPT_CTXINFO_KEYING_ALGO);
		case CRYPTCON_PROP_KEYING_ITERATIONS:
			return js_cryptcon_attr_set(cx, vp, p->ctx, CRYPT_CTXINFO_KEYING_ITERATIONS);
		case CRYPTCON_PROP_KEYSIZE:
			return js_cryptcon_attr_set(cx, vp, p->ctx, CRYPT_CTXINFO_KEYSIZE);
		case CRYPTCON_PROP_MODE:
			return js_cryptcon_attr_set(cx, vp, p->ctx, CRYPT_CTXINFO_MODE);
		case CRYPTCON_PROP_HASHVALUE:
			return js_cryptcon_attrstr_set(cx, vp, p->ctx, CRYPT_CTXINFO_HASHVALUE);
		case CRYPTCON_PROP_IV:
			return js_cryptcon_attrstr_set(cx, vp, p->ctx, CRYPT_CTXINFO_IV);
		case CRYPTCON_PROP_KEYING_SALT:
			return js_cryptcon_attrstr_set(cx, vp, p->ctx, CRYPT_CTXINFO_KEYING_SALT);
		case CRYPTCON_PROP_LABEL:
			return js_cryptcon_attrstr_set(cx, vp, p->ctx, CRYPT_CTXINFO_LABEL);
		case CRYPTCON_PROP_NAME_ALGO:
			return js_cryptcon_attrstr_set(cx, vp, p->ctx, CRYPT_CTXINFO_NAME_ALGO);
		case CRYPTCON_PROP_NAME_MODE:
			return js_cryptcon_attrstr_set(cx, vp, p->ctx, CRYPT_CTXINFO_NAME_MODE);
	}
	return JS_TRUE;
}

static JSBool
js_cryptcon_attr_get(JSContext *cx, jsval *vp, CRYPT_CONTEXT ctx, CRYPT_ATTRIBUTE_TYPE type)
{
	int status;
	int val;

	status = cryptGetAttribute(ctx, type, &val);
	if (cryptStatusError(status)) {
		//js_cryptcon_error(cx, ctx, status);
		//return JS_FALSE;
		*vp = JSVAL_VOID;
		return JS_TRUE;
	}
	*vp = INT_TO_JSVAL(val);

	return JS_TRUE;
}

static JSBool
js_cryptcon_attrstr_get(JSContext *cx, jsval *vp, CRYPT_CONTEXT ctx, CRYPT_ATTRIBUTE_TYPE type)
{
	int status;
	char *val;
	int len;
	JSString *js_str;

	status = cryptGetAttributeString(ctx, type, NULL, &len);
	if (cryptStatusError(status)) {
		*vp = JSVAL_VOID;
		return JS_TRUE;	// Do not return JS_FALSE here, or jsdocs build will break.
	}
	if ((val = (char *)malloc(len)) == NULL) {
		JS_ReportError(cx, "malloc(%d) failure", len);
		return JS_FALSE;
	}
	status = cryptGetAttributeString(ctx, type, val, &len);
	if (cryptStatusError(status)) {
		js_cryptcon_error(cx, ctx, status);
		return JS_FALSE;
	}
	if((js_str=JS_NewStringCopyN(cx, val, len))==NULL) {
		free(val);
		return(JS_FALSE);
	}
	free(val);
	*vp = STRING_TO_JSVAL(js_str);

	return JS_TRUE;
}

static JSBool
js_cryptcon_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval idval;
    jsint tiny;
	struct js_cryptcon_private_data* p;

	if ((p=(struct js_cryptcon_private_data *)JS_GetPrivate(cx,obj))==NULL) {
		return JS_TRUE;
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch (tiny) {
		case CRYPTCON_PROP_ALGO:
			return js_cryptcon_attr_get(cx, vp, p->ctx, CRYPT_CTXINFO_ALGO);
		case CRYPTCON_PROP_BLOCKSIZE:
			return js_cryptcon_attr_get(cx, vp, p->ctx, CRYPT_CTXINFO_BLOCKSIZE);
		case CRYPTCON_PROP_IVSIZE:
			return js_cryptcon_attr_get(cx, vp, p->ctx, CRYPT_CTXINFO_IVSIZE);
		case CRYPTCON_PROP_KEYING_ALGO:
			return js_cryptcon_attr_get(cx, vp, p->ctx, CRYPT_CTXINFO_KEYING_ALGO);
		case CRYPTCON_PROP_KEYING_ITERATIONS:
			return js_cryptcon_attr_get(cx, vp, p->ctx, CRYPT_CTXINFO_KEYING_ITERATIONS);
		case CRYPTCON_PROP_KEYSIZE:
			return js_cryptcon_attr_get(cx, vp, p->ctx, CRYPT_CTXINFO_KEYSIZE);
		case CRYPTCON_PROP_MODE:
			return js_cryptcon_attr_get(cx, vp, p->ctx, CRYPT_CTXINFO_MODE);
		case CRYPTCON_PROP_HASHVALUE:
			cryptEncrypt(p->ctx, p, 0);
			return js_cryptcon_attrstr_get(cx, vp, p->ctx, CRYPT_CTXINFO_HASHVALUE);
		case CRYPTCON_PROP_IV:
			return js_cryptcon_attrstr_get(cx, vp, p->ctx, CRYPT_CTXINFO_IV);
		case CRYPTCON_PROP_KEYING_SALT:
			return js_cryptcon_attrstr_get(cx, vp, p->ctx, CRYPT_CTXINFO_KEYING_SALT);
		case CRYPTCON_PROP_LABEL:
			return js_cryptcon_attrstr_get(cx, vp, p->ctx, CRYPT_CTXINFO_LABEL);
		case CRYPTCON_PROP_NAME_ALGO:
			return js_cryptcon_attrstr_get(cx, vp, p->ctx, CRYPT_CTXINFO_NAME_ALGO);
		case CRYPTCON_PROP_NAME_MODE:
			return js_cryptcon_attrstr_get(cx, vp, p->ctx, CRYPT_CTXINFO_NAME_MODE);
	}
	return JS_TRUE;
}

static jsSyncPropertySpec js_cryptcon_properties[] = {
/*	name				,tinyid						,flags,				ver	*/
	{"algo"				,CRYPTCON_PROP_ALGO				,JSPROP_ENUMERATE	,316},
	{"blocksize"		,CRYPTCON_PROP_BLOCKSIZE		,JSPROP_ENUMERATE	,316},
	{"hashvalue"		,CRYPTCON_PROP_HASHVALUE		,JSPROP_ENUMERATE	,316},
	{"iv"				,CRYPTCON_PROP_IV				,JSPROP_ENUMERATE	,316},
	{"ivsize"			,CRYPTCON_PROP_IVSIZE			,JSPROP_ENUMERATE	,316},
	{"keying_algo"		,CRYPTCON_PROP_KEYING_ITERATIONS,JSPROP_ENUMERATE	,316},
	{"keying_iterations",CRYPTCON_PROP_BLOCKSIZE		,JSPROP_ENUMERATE	,316},
	{"keying_salt"		,CRYPTCON_PROP_KEYING_SALT		,JSPROP_ENUMERATE	,316},
	{"keysize"			,CRYPTCON_PROP_KEYSIZE			,JSPROP_ENUMERATE	,316},
	{"label"			,CRYPTCON_PROP_LABEL			,JSPROP_ENUMERATE	,316},
	{"mode"				,CRYPTCON_PROP_MODE				,JSPROP_ENUMERATE	,316},
	{"algo_name"		,CRYPTCON_PROP_NAME_ALGO		,JSPROP_ENUMERATE	,316},
	{"mode_name"		,CRYPTCON_PROP_NAME_MODE		,JSPROP_ENUMERATE	,316},
	{0}
};

static jsSyncMethodSpec js_cryptcon_functions[] = {
	{"generate_key",	js_generate_key,	0,	JSTYPE_VOID,	""
	,JSDOCSTR("Generate a new key for the context.")
	,316
	},
	{"set_key",			js_set_key,			0,	JSTYPE_VOID,	"key_str"
	,JSDOCSTR("Set the key for the context directly.")
	,316
	},
	{"derive_key",		js_derive_key,		0,	JSTYPE_VOID,	"keying_data"
	,JSDOCSTR("Derive the key from the keying data using keying_algo, keying_iterations, and keying_salt.")
	,316
	},
	{"encrypt",			js_encrypt,			0,	JSTYPE_STRING,	"plaintext"
	,JSDOCSTR("Encrypt the string and return as a new string.")
	,316
	},
	{"decrypt",			js_decrypt,			0,	JSTYPE_STRING,	"ciphertext"
	,JSDOCSTR("Decrypt the string and return as a new string.")
	,316
	},
	{"create_signature",js_create_signature,0,	JSTYPE_STRING,	"sigContext"
	,JSDOCSTR("Create a signature signed with the sigContext CryptContext object.")
	,316
	},
	{0}
};

static JSBool js_cryptcon_resolve(JSContext *cx, JSObject *obj, jsid id)
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

	ret=js_SyncResolve(cx, obj, name, js_cryptcon_properties, js_cryptcon_functions, NULL, 0);
	if(name)
		free(name);
	return ret;
}

static JSBool js_cryptcon_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_cryptcon_resolve(cx, obj, JSID_VOID));
}

JSClass js_cryptcon_class = {
     "CryptContext"			/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_cryptcon_get		/* getProperty	*/
	,js_cryptcon_set		/* setProperty	*/
	,js_cryptcon_enumerate	/* enumerate	*/
	,js_cryptcon_resolve	/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_finalize_cryptcon		/* finalize		*/
};

JSObject* DLLCALL js_CreateCryptconObject(JSContext* cx, CRYPT_CONTEXT ctx)
{
	JSObject *obj;
	struct js_cryptcon_private_data *p;

	obj=JS_NewObject(cx, &js_cryptcon_class, NULL, NULL);

	if((p=(struct js_cryptcon_private_data *)malloc(sizeof(struct js_cryptcon_private_data)))==NULL) {
		JS_ReportError(cx,"malloc failed");
		return NULL;
	}
	memset(p,0,sizeof(struct js_cryptcon_private_data));
	p->ctx = ctx;

	if(!JS_SetPrivate(cx, obj, p)) {
		JS_ReportError(cx,"JS_SetPrivate failed");
		return NULL;
	}
	js_create_key_object(cx, obj);

	return obj;
}

// Constructor

static JSBool
js_cryptcon_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj;
	jsval *argv=JS_ARGV(cx, arglist);
	struct js_cryptcon_private_data *p;
	jsrefcount rc;
	int status;
	int algo;

	do_cryptInit();
	obj=JS_NewObject(cx, &js_cryptcon_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
	if(argc != 1) {
		JS_ReportError(cx, "Incorrect number of arguments to CryptContext constructor.  Got %d, expected 1.", argc);
		return JS_FALSE;
	}
	if (!JS_ValueToInt32(cx,argv[0],&algo))
		return JS_FALSE;

	if((p=(struct js_cryptcon_private_data *)malloc(sizeof(struct js_cryptcon_private_data)))==NULL) {
		JS_ReportError(cx,"malloc failed");
		return(JS_FALSE);
	}
	memset(p,0,sizeof(struct js_cryptcon_private_data));

	if(!JS_SetPrivate(cx, obj, p)) {
		JS_ReportError(cx,"JS_SetPrivate failed");
		return(JS_FALSE);
	}

	rc = JS_SUSPENDREQUEST(cx);
	status = cryptCreateContext(&p->ctx, CRYPT_UNUSED, algo);
	JS_RESUMEREQUEST(cx, rc);
	if (cryptStatusError(status)) {
		js_cryptcon_error(cx, p->ctx, status);
		return JS_FALSE;
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,obj,"Class used for encryption/decryption",31601);
	js_DescribeSyncConstructor(cx,obj,"To create a new CryptContext object: "
		"var c = new CryptContext('<i>algorithm</i>')</tt><br>"
		"where <i>algorithm</i> is a property of CryptContext.ALGO"
		);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", cryptcon_prop_desc, JSPROP_READONLY);
#endif

	return(JS_TRUE);
}

JSObject* DLLCALL js_CreateCryptContextClass(JSContext* cx, JSObject* parent)
{
	JSObject*	ccobj;
	JSObject*	constructor;
	JSObject*	algo;
	JSObject*	mode;
	jsval		val;

	ccobj = JS_InitClass(cx, parent, NULL
		,&js_cryptcon_class
		,js_cryptcon_constructor
		,1	/* number of constructor args */
		,NULL /* props, specified in constructor */
		,NULL /* funcs, specified in constructor */
		,NULL, NULL);

	if(JS_GetProperty(cx, parent, js_cryptcon_class.name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JS_ValueToObject(cx,val,&constructor);
		algo = JS_DefineObject(cx, constructor, "ALGO", NULL, NULL, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
		if(algo != NULL) {
			JS_DefineProperty(cx, algo, "None", INT_TO_JSVAL(CRYPT_ALGO_NONE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, algo, "DES", INT_TO_JSVAL(CRYPT_ALGO_DES), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, algo, "3DES", INT_TO_JSVAL(CRYPT_ALGO_3DES), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, algo, "IDEA", INT_TO_JSVAL(CRYPT_ALGO_IDEA), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, algo, "CAST", INT_TO_JSVAL(CRYPT_ALGO_CAST), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, algo, "RC2", INT_TO_JSVAL(CRYPT_ALGO_RC2), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, algo, "RC4", INT_TO_JSVAL(CRYPT_ALGO_RC4), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			/* CRYPT_ALGO_RC5 no longer supported. */
			JS_DefineProperty(cx, algo, "AES", INT_TO_JSVAL(CRYPT_ALGO_AES), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			/* CRYPT_ALGO_BLOWFISH no longer supported */
			JS_DefineProperty(cx, algo, "DH", INT_TO_JSVAL(CRYPT_ALGO_DH), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, algo, "RSA", INT_TO_JSVAL(CRYPT_ALGO_RSA), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, algo, "DSA", INT_TO_JSVAL(CRYPT_ALGO_DSA), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, algo, "ELGAMAL", INT_TO_JSVAL(CRYPT_ALGO_ELGAMAL), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, algo, "ECDSA", INT_TO_JSVAL(CRYPT_ALGO_ECDSA), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, algo, "ECDH", INT_TO_JSVAL(CRYPT_ALGO_ECDH), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, algo, "MD5", INT_TO_JSVAL(CRYPT_ALGO_MD5), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, algo, "SHA1", INT_TO_JSVAL(CRYPT_ALGO_SHA1), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, algo, "SHA2", INT_TO_JSVAL(CRYPT_ALGO_SHA2), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			/* CRYPT_ALGO_RIPEMD160 no longer supported */
			JS_DefineProperty(cx, algo, "SHAng", INT_TO_JSVAL(CRYPT_ALGO_SHAng), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			/* CRYPT_ALGO_HMAC_MD5 no longer supported */
			JS_DefineProperty(cx, algo, "HMAC-SHA1", INT_TO_JSVAL(CRYPT_ALGO_HMAC_SHA1), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, algo, "HMAC-SHA2", INT_TO_JSVAL(CRYPT_ALGO_HMAC_SHA2), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			/* CRYPT_ALGO_HMAC_RIPEMD160 no longer supported */
			JS_DefineProperty(cx, algo, "HMAC-SHAng", INT_TO_JSVAL(CRYPT_ALGO_HMAC_SHAng), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DeepFreezeObject(cx, algo);
		}
		mode = JS_DefineObject(cx, constructor, "MODE", NULL, NULL, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
		if(JS_GetProperty(cx, constructor, "MODE", &val) && !JSVAL_NULL_OR_VOID(val)) {
			JS_DefineProperty(cx, mode, "None", INT_TO_JSVAL(CRYPT_MODE_NONE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, mode, "ECB", INT_TO_JSVAL(CRYPT_MODE_ECB), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, mode, "CBC", INT_TO_JSVAL(CRYPT_MODE_CBC), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, mode, "CFB", INT_TO_JSVAL(CRYPT_MODE_CFB), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			/* CRYPT_MODE_OFB no longer supported */
			JS_DefineProperty(cx, mode, "GCM", INT_TO_JSVAL(CRYPT_MODE_GCM), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DeepFreezeObject(cx, mode);
		}
	}

	return(ccobj);
}
