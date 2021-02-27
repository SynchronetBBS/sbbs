/* $Id: js_cryptcert.c,v 1.8 2018/02/26 07:05:10 deuce Exp $ */

// Cyrptlib Certificates...

#include "sbbs.h"
#include <cryptlib.h>
#include "js_request.h"
#include "js_cryptcon.h"
#include "js_cryptcert.h"
#include "ssl.h"

static const char* getprivate_failure = "line %d %s %s JS_GetPrivate failed";

// Helpers

static void
js_cryptcert_error(JSContext *cx, CRYPT_CERTIFICATE cert, int error)
{
	char *errstr;
	int errlen;

	if (cryptGetAttributeString(cert, CRYPT_ATTRIBUTE_ERRORMESSAGE, NULL, &errlen) != CRYPT_OK) {
		JS_ReportError(cx, "CryptLib error %d", error);
		return;
	}
	if ((errstr = (char *)malloc(errlen+1)) == NULL) {
		JS_ReportError(cx, "CryptLib error %d", error);
		return;
	}
	if (cryptGetAttributeString(cert, CRYPT_ATTRIBUTE_ERRORMESSAGE, errstr, &errlen) != CRYPT_OK) {
		free(errstr);
		JS_ReportError(cx, "CryptLib error %d", error);
		return;
	}
	errstr[errlen+1] = 0;

	JS_ReportError(cx, "Cryptlib error %d (%s)", error, errstr);
	free(errstr);
}

// Destructor

static void
js_finalize_cryptcert(JSContext *cx, JSObject *obj)
{
	jsrefcount rc;
	struct js_cryptcert_private_data* p;

	if ((p=(struct js_cryptcert_private_data *)JS_GetPrivate(cx,obj))==NULL)
		return;

	rc = JS_SUSPENDREQUEST(cx);
	if (p->cert != CRYPT_UNUSED)
		cryptDestroyCert(p->cert);
	free(p);
	JS_RESUMEREQUEST(cx, rc);

	JS_SetPrivate(cx, obj, NULL);
}

// Methods

static JSBool
js_add_extension(JSContext *cx, uintN argc, jsval *arglist)
{
	struct js_cryptcert_private_data* p;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsrefcount rc;
	int status;
	jsval *argv=JS_ARGV(cx, arglist);
	JSString *jsoid;
	JSBool critical;
	JSString *jsextension;
	char *oid;
	char *extension;
	size_t oid_len;
	size_t ext_len;

	if ((p=(struct js_cryptcert_private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	if (argc != 3) {
		JS_ReportError(cx, "Incorrect number of arguments.  Got %d, expected 3.", argc);
		return JS_FALSE;
	}

	if ((jsoid = JS_ValueToString(cx, argv[0])) == NULL) {
		JS_ReportError(cx, "Invalid oid");
		return JS_FALSE;
	}
	if (!JS_ValueToBoolean(cx, argv[1], &critical)) {
		JS_ReportError(cx, "Invalid critical flag");
		return JS_FALSE;
	}
	if ((jsextension = JS_ValueToString(cx, argv[2])) == NULL) {
		JS_ReportError(cx, "Invalid extension");
		return JS_FALSE;
	}
	JSSTRING_TO_MSTRING(cx, jsoid, oid, &oid_len);
	HANDLE_PENDING(cx, oid);
	JSSTRING_TO_MSTRING(cx, jsextension, extension, &ext_len);
	if (JS_IsExceptionPending(cx)) {
		FREE_AND_NULL(oid);
		FREE_AND_NULL(extension);
		return JS_FALSE;
	}
	rc = JS_SUSPENDREQUEST(cx);
	status = cryptAddCertExtension(p->cert, oid, critical, extension, ext_len);
	JS_RESUMEREQUEST(cx, rc);
	if (cryptStatusError(status)) {
		js_cryptcert_error(cx, p->cert, status);
		return JS_FALSE;
	}
	return JS_TRUE;
}

static JSBool
js_check(JSContext *cx, uintN argc, jsval *arglist)
{
	struct js_cryptcert_private_data* p;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsrefcount rc;
	int status;

	if ((p=(struct js_cryptcert_private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}
	rc = JS_SUSPENDREQUEST(cx);
	status = cryptCheckCert(p->cert, CRYPT_UNUSED);	// TODO: Check against key/CRL/online
	JS_RESUMEREQUEST(cx, rc);
	if (cryptStatusError(status)) {
		js_cryptcert_error(cx, p->cert, status);
		return JS_FALSE;
	}
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	return JS_TRUE;
}

static JSBool
js_destroy(JSContext *cx, uintN argc, jsval *arglist)
{
	struct js_cryptcert_private_data* p;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsrefcount rc;
	int status;

	if ((p=(struct js_cryptcert_private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}
	rc = JS_SUSPENDREQUEST(cx);
	status = cryptDestroyCert(p->cert);
	JS_RESUMEREQUEST(cx, rc);
	if (cryptStatusError(status)) {
		js_cryptcert_error(cx, p->cert, status);
		return JS_FALSE;
	}
	p->cert = CRYPT_UNUSED;
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	return JS_TRUE;
}

static JSBool
js_export(JSContext *cx, uintN argc, jsval *arglist)
{
	struct js_cryptcert_private_data* p;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsrefcount rc;
	int status;
	int format;
	char *buf;
	JSString *ret;
	jsval *argv=JS_ARGV(cx, arglist);
	int len;

	if(argc != 1) {
		JS_ReportError(cx, "Incorrect number of arguments.  Got %d, expected 1.", argc);
		return JS_FALSE;
	}
	if (!JS_ValueToInt32(cx,argv[0],&format)) {
		JS_ReportError(cx, "Invalid format.");
		return JS_FALSE;
	}

	if ((p=(struct js_cryptcert_private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	status = cryptExportCert(NULL, 0, &len, format, p->cert);
	if (cryptStatusError(status)) {
		JS_RESUMEREQUEST(cx, rc);
		js_cryptcert_error(cx, p->cert, status);
		return JS_FALSE;
	}
	buf = malloc(len);
	if (buf == NULL) {
		JS_RESUMEREQUEST(cx, rc);
		JS_ReportError(cx, "Unable to allocate %d bytes\n", len);
		return JS_FALSE;
	}
	status = cryptExportCert(buf, len, &len, format, p->cert);
	JS_RESUMEREQUEST(cx, rc);
	if (cryptStatusError(status)) {
		JS_RESUMEREQUEST(cx, rc);
		js_cryptcert_error(cx, p->cert, status);
		return JS_FALSE;
	}
	ret = JS_NewStringCopyN(cx, buf, len);
	free(buf);
	if (ret == NULL) {
		JS_ReportError(cx, "Unable to allocate return string\n");
		return JS_FALSE;
	}
	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(ret));

	return JS_TRUE;
}

static JSBool
js_get_attribute(JSContext *cx, uintN argc, jsval *arglist)
{
	struct js_cryptcert_private_data* p;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsrefcount rc;
	int status;
	int attr;
	int val;
	jsval *argv=JS_ARGV(cx, arglist);

	if(argc != 1) {
		JS_ReportError(cx, "Incorrect number of arguments.  Got %d, expected 1.", argc);
		return JS_FALSE;
	}
	if (!JS_ValueToInt32(cx,argv[0],&attr)) {
		JS_ReportError(cx, "Invalid attribute.");
		return JS_FALSE;
	}

	if ((p=(struct js_cryptcert_private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	status = cryptGetAttribute(p->cert, attr, &val);
	JS_RESUMEREQUEST(cx, rc);
	if (cryptStatusError(status)) {
		js_cryptcert_error(cx, p->cert, status);
		return JS_FALSE;
	}
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(val));

	return JS_TRUE;

}

static JSBool
js_get_attribute_string(JSContext *cx, uintN argc, jsval *arglist)
{
	struct js_cryptcert_private_data* p;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsrefcount rc;
	int status;
	int attr;
	char *val;
	int len;
	JSString *js_str;
	jsval *argv=JS_ARGV(cx, arglist);

	if(argc != 1) {
		JS_ReportError(cx, "Incorrect number of arguments.  Got %d, expected 1.", argc);
		return JS_FALSE;
	}
	if (!JS_ValueToInt32(cx,argv[0],&attr)) {
		JS_ReportError(cx, "Invalid attribute.");
		return JS_FALSE;
	}

	if ((p=(struct js_cryptcert_private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	status = cryptGetAttributeString(p->cert, attr, NULL, &len);
	if (cryptStatusError(status)) {
		JS_RESUMEREQUEST(cx, rc);
		js_cryptcert_error(cx, p->cert, status);
		return JS_FALSE;	// Do not return JS_FALSE here, or jsdocs build will break.
	}
	if ((val = (char *)malloc(len)) == NULL) {
		JS_RESUMEREQUEST(cx, rc);
		JS_ReportError(cx, "malloc(%d) failure", len);
		return JS_FALSE;
	}
	status = cryptGetAttributeString(p->cert, attr, val, &len);
	if (cryptStatusError(status)) {
		JS_RESUMEREQUEST(cx, rc);
		js_cryptcert_error(cx, p->cert, status);
		return JS_FALSE;
	}
	JS_RESUMEREQUEST(cx, rc);
	if((js_str=JS_NewStringCopyN(cx, val, len))==NULL) {
		free(val);
		return(JS_FALSE);
	}
	free(val);

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_get_attribute_time(JSContext *cx, uintN argc, jsval *arglist)
{
	struct js_cryptcert_private_data* p;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsrefcount rc;
	int status;
	int attr;
	time_t val;
	int len;
	JSObject *dobj;
	jsval *argv=JS_ARGV(cx, arglist);
	jsdouble msec;

	if(argc != 1) {
		JS_ReportError(cx, "Incorrect number of arguments.  Got %d, expected 1.", argc);
		return JS_FALSE;
	}
	if (!JS_ValueToInt32(cx,argv[0],&attr)) {
		JS_ReportError(cx, "Invalid attribute.");
		return JS_FALSE;
	}

	if ((p=(struct js_cryptcert_private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);

	status = cryptGetAttributeString(p->cert, attr, NULL, &len);
	if (cryptStatusError(status)) {
		JS_RESUMEREQUEST(cx, rc);
		js_cryptcert_error(cx, p->cert, status);
		return JS_FALSE;
	}
	if (len != sizeof(val)) {
		JS_RESUMEREQUEST(cx, rc);
		JS_ReportError(cx, "Time size %d not sizeof(time_t) (%d)\n", len, sizeof(val));
		return JS_FALSE;
	}
	status = cryptGetAttributeString(p->cert, attr, &val, &len);
	if (cryptStatusError(status)) {
		JS_RESUMEREQUEST(cx, rc);
		js_cryptcert_error(cx, p->cert, status);
		return JS_FALSE;
	}
	msec = (jsdouble)val;
	msec *= 1000;
	dobj = JS_NewDateObjectMsec(cx, msec);
	if(dobj==NULL)
		return(JS_FALSE);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(dobj));
	return JS_TRUE;
}

static JSBool
js_set_attribute(JSContext *cx, uintN argc, jsval *arglist)
{
	struct js_cryptcert_private_data* p;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsrefcount rc;
	int status;
	int attr;
	int val;
	jsval *argv=JS_ARGV(cx, arglist);

	if(argc != 2) {
		JS_ReportError(cx, "Incorrect number of arguments.  Got %d, expected 2.", argc);
		return JS_FALSE;
	}
	if (!JS_ValueToInt32(cx,argv[0],&attr)) {
		JS_ReportError(cx, "Invalid attribute.");
		return JS_FALSE;
	}
	if (!JS_ValueToInt32(cx,argv[1],&val)) {
		JS_ReportError(cx, "Invalid value.");
		return JS_FALSE;
	}

	if ((p=(struct js_cryptcert_private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	status = cryptSetAttribute(p->cert, attr, val);
	JS_RESUMEREQUEST(cx, rc);
	if (cryptStatusError(status)) {
		js_cryptcert_error(cx, p->cert, status);
		return JS_FALSE;
	}
	return JS_TRUE;
}

static JSBool
js_set_attribute_string(JSContext *cx, uintN argc, jsval *arglist)
{
	struct js_cryptcert_private_data* p;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsrefcount rc;
	int status;
	int attr;
	char *val;
	size_t len;
	jsval *argv=JS_ARGV(cx, arglist);

	if(argc != 2) {
		JS_ReportError(cx, "Incorrect number of arguments.  Got %d, expected 2.", argc);
		return JS_FALSE;
	}
	if (!JS_ValueToInt32(cx,argv[0],&attr)) {
		JS_ReportError(cx, "Invalid attribute.");
		return JS_FALSE;
	}

	if ((p=(struct js_cryptcert_private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	JSVALUE_TO_MSTRING(cx, argv[1], val, &len);
	HANDLE_PENDING(cx, val);
	if (val == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	status = cryptSetAttributeString(p->cert, attr, val, len);
	free(val);
	JS_RESUMEREQUEST(cx, rc);
	if (cryptStatusError(status)) {
		js_cryptcert_error(cx, p->cert, status);
		return JS_FALSE;
	}
	return JS_TRUE;
}

static JSBool
js_set_attribute_time(JSContext *cx, uintN argc, jsval *arglist)
{
	struct js_cryptcert_private_data* p;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsrefcount rc;
	int status;
	int attr;
	time_t val;
	jsdouble sec;
	jsval *argv=JS_ARGV(cx, arglist);

	if(argc != 2) {
		JS_ReportError(cx, "Incorrect number of arguments.  Got %d, expected 2.", argc);
		return JS_FALSE;
	}
	if (!JS_ValueToInt32(cx,argv[0],&attr)) {
		JS_ReportError(cx, "Invalid attribute.");
		return JS_FALSE;
	}

	if (JSVAL_IS_OBJECT(argv[1])) {
		if (!JS_ObjectIsDate(cx, JSVAL_TO_OBJECT(argv[1]))) {
			JS_ReportError(cx, "Invalid Date");
			return JS_FALSE;
		}
		if (!JS_ValueToNumber(cx, argv[1], &sec)) {
			JS_ReportError(cx, "Invalid Date");
			return JS_FALSE;
		}
		sec /= 1000;
	}
	else {
		if (JSVAL_IS_NUMBER(argv[1])) {
			if (!JS_ValueToNumber(cx, argv[1], &sec)) {
				JS_ReportError(cx, "Invalid Date");
				return JS_FALSE;
			}
		}
		else {
			JS_ReportError(cx, "Invalid Date");
			return JS_FALSE;
		}
	}
	val = (time_t)sec;

	if ((p=(struct js_cryptcert_private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	status = cryptSetAttributeString(p->cert, attr, &val, sizeof(val));
	JS_RESUMEREQUEST(cx, rc);
	if (cryptStatusError(status)) {
		js_cryptcert_error(cx, p->cert, status);
		return JS_FALSE;
	}
	return JS_TRUE;
}

static JSBool
js_sign(JSContext *cx, uintN argc, jsval *arglist)
{
	struct js_cryptcon_private_data* ctx;
	struct js_cryptcert_private_data* p;
	jsval *argv=JS_ARGV(cx, arglist);
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsrefcount rc;
	int status;
	JSObject *key;

	if(argc != 1) {
		JS_ReportError(cx, "Incorrect number of arguments.  Got %d, expected 1.", argc);
		return JS_FALSE;
	}

	key = JSVAL_TO_OBJECT(argv[0]);
	if (!JS_InstanceOf(cx, key, &js_cryptcon_class, NULL)) {
		JS_ReportError(cx, "Invalid CryptContext");
		return JS_FALSE;
	}

	if ((p=(struct js_cryptcert_private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	if ((ctx=(struct js_cryptcon_private_data *)JS_GetPrivate(cx,key))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	cryptSetAttribute(p->cert, CRYPT_OPTION_CERT_SIGNUNRECOGNISEDATTRIBUTES, 1);
	status = cryptSignCert(p->cert, ctx->ctx);
	JS_RESUMEREQUEST(cx, rc);
	if (cryptStatusError(status)) {
		JS_RESUMEREQUEST(cx, rc);
		js_cryptcert_error(cx, p->cert, status);
		return JS_FALSE;
	}

	return JS_TRUE;
}

// Properties

static JSBool
js_cryptcert_attr_get(JSContext *cx, jsval *vp, CRYPT_CERTIFICATE cert, CRYPT_ATTRIBUTE_TYPE type)
{
	int status;
	int val;

	status = cryptGetAttribute(cert, type, &val);
	if (cryptStatusError(status)) {
		*vp = JSVAL_VOID;
		return JS_TRUE;
	}
	*vp = INT_TO_JSVAL(val);

	return JS_TRUE;
}

static JSBool
js_cryptcert_attrstr_get(JSContext *cx, jsval *vp, CRYPT_CERTIFICATE cert, CRYPT_ATTRIBUTE_TYPE type)
{
	int status;
	char *val;
	int len;
	JSString *js_str;

	status = cryptGetAttributeString(cert, type, NULL, &len);
	if (cryptStatusError(status)) {
		*vp = JSVAL_VOID;
		return JS_TRUE;	// Do not return JS_FALSE here, or jsdocs build will break.
	}
	if ((val = (char *)malloc(len)) == NULL) {
		JS_ReportError(cx, "malloc(%d) failure", len);
		return JS_FALSE;
	}
	status = cryptGetAttributeString(cert, type, val, &len);
	if (cryptStatusError(status)) {
		js_cryptcert_error(cx, cert, status);
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
js_cryptcert_attrtime_get(JSContext *cx, jsval *vp, CRYPT_CERTIFICATE cert, CRYPT_ATTRIBUTE_TYPE type)
{
	int status;
	int len;
	time_t t;
	jsdouble msec;
	JSObject *dobj;

	status = cryptGetAttributeString(cert, type, NULL, &len);
	if (cryptStatusError(status)) {
		*vp = JSVAL_VOID;
		return JS_TRUE;	// Do not return JS_FALSE here, or jsdocs build will break.
	}
	if (len != sizeof(t)) {
		JS_ReportError(cx, "Time size %d not sizeof(time_t) (%d)\n", len, sizeof(t));
		return JS_FALSE;
	}
	status = cryptGetAttributeString(cert, type, &t, &len);
	if (cryptStatusError(status)) {
		js_cryptcert_error(cx, cert, status);
		return JS_FALSE;
	}
	msec = (jsdouble)t;
	msec *= 1000;
	dobj = JS_NewDateObjectMsec(cx, msec);
	if(dobj==NULL)
		return(JS_FALSE);
	*vp = OBJECT_TO_JSVAL(dobj);

	return JS_TRUE;
}

enum {
	/**************************/
	/* Certificate attributes */
	/**************************/

	/* Because there are so many cert attributes, we break them down into
	   blocks to minimise the number of values that change if a new one is
	   added halfway through */

	/* Pseudo-information on a cert object or meta-information which is used
	   to control the way that a cert object is processed */
	CRYPTCERT_PROP_SELFSIGNED,		/* Cert is self-signed */
	CRYPTCERT_PROP_IMMUTABLE,		/* Cert is signed and immutable */
	CRYPTCERT_PROP_XYZZY,			/* Cert is a magic just-works cert */
	CRYPTCERT_PROP_CERTTYPE,		/* Certificate object type */
	CRYPTCERT_PROP_FINGERPRINT_SHA1,/* Certificate fingerprints */
	CRYPTCERT_PROP_FINGERPRINT_SHA2,
	CRYPTCERT_PROP_FINGERPRINT_SHAng,
	CRYPTCERT_PROP_CURRENT_CERTIFICATE,/* Cursor mgt: Rel.pos in chain/CRL/OCSP */
	CRYPTCERT_PROP_TRUSTED_USAGE,	/* Usage that cert is trusted for */
	CRYPTCERT_PROP_TRUSTED_IMPLICIT,/* Whether cert is implicitly trusted */
	CRYPTCERT_PROP_SIGNATURELEVEL,	/* Amount of detail to include in sigs.*/

	/* General certificate object information */
	CRYPTCERT_PROP_VERSION,			/* Cert.format version */
	CRYPTCERT_PROP_SERIALNUMBER,	/* Serial number */
	CRYPTCERT_PROP_SUBJECTPUBLICKEYINFO,	/* Public key */
	CRYPTCERT_PROP_CERTIFICATE,		/* User certificate */
	CRYPTCERT_PROP_CACERTIFICATE,	/* CA certificate */
	CRYPTCERT_PROP_ISSUERNAME,		/* Issuer DN */
	CRYPTCERT_PROP_VALIDFROM,		/* Cert valid-from time */
	CRYPTCERT_PROP_VALIDTO,			/* Cert valid-to time */
	CRYPTCERT_PROP_SUBJECTNAME,		/* Subject DN */
	CRYPTCERT_PROP_ISSUERUNIQUEID,	/* Issuer unique ID */
	CRYPTCERT_PROP_SUBJECTUNIQUEID,	/* Subject unique ID */
	CRYPTCERT_PROP_CERTREQUEST,		/* Cert.request (DN + public key) */
	CRYPTCERT_PROP_THISUPDATE,		/* CRL/OCSP current-update time */
	CRYPTCERT_PROP_NEXTUPDATE,		/* CRL/OCSP next-update time */
	CRYPTCERT_PROP_REVOCATIONDATE,	/* CRL/OCSP cert-revocation time */
	CRYPTCERT_PROP_REVOCATIONSTATUS,/* OCSP revocation status */
	CRYPTCERT_PROP_CERTSTATUS,		/* RTCS certificate status */
	CRYPTCERT_PROP_DN,				/* Currently selected DN in string form */
	CRYPTCERT_PROP_PKIUSER_ID,		/* PKI user ID */
	CRYPTCERT_PROP_PKIUSER_ISSUEPASSWORD,	/* PKI user issue password */
	CRYPTCERT_PROP_PKIUSER_REVPASSWORD,		/* PKI user revocation password */
	CRYPTCERT_PROP_PKIUSER_RA,		/* PKI user is an RA */

	/* X.520 Distinguished Name components.  This is a composite field, the
	   DN to be manipulated is selected through the addition of a
	   pseudocomponent, and then one of the following is used to access the
	   DN components directly */
	CRYPTCERT_PROP_COUNTRYNAME,	/* countryName */
	CRYPTCERT_PROP_STATEORPROVINCENAME,	/* stateOrProvinceName */
	CRYPTCERT_PROP_LOCALITYNAME,		/* localityName */
	CRYPTCERT_PROP_ORGANIZATIONNAME,	/* organizationName */
	CRYPTCERT_PROP_ORGANIZATIONALUNITNAME,	/* organizationalUnitName */
	CRYPTCERT_PROP_COMMONNAME,		/* commonName */

	/* X.509 General Name components.  These are handled in the same way as
	   the DN composite field, with the current GeneralName being selected by
	   a pseudo-component after which the individual components can be
	   modified through one of the following */
	CRYPTCERT_PROP_OTHERNAME_TYPEID,		/* otherName.typeID */
	CRYPTCERT_PROP_OTHERNAME_VALUE,			/* otherName.value */
	CRYPTCERT_PROP_RFC822NAME,				/* rfc822Name */
	CRYPTCERT_PROP_DNSNAME,					/* dNSName */
#if 0	/* Not supported, these are never used in practice and have an
		   insane internal structure */
	CRYPTCERT_PROP_X400ADDRESS,				/* x400Address */
#endif /* 0 */
	CRYPTCERT_PROP_DIRECTORYNAME,			/* directoryName */
	CRYPTCERT_PROP_EDIPARTYNAME_NAMEASSIGNER,	/* ediPartyName.nameAssigner */
	CRYPTCERT_PROP_EDIPARTYNAME_PARTYNAME,	/* ediPartyName.partyName */
	CRYPTCERT_PROP_UNIFORMRESOURCEIDENTIFIER,	/* uniformResourceIdentifier */
	CRYPTCERT_PROP_IPADDRESS,				/* iPAddress */
	CRYPTCERT_PROP_REGISTEREDID,			/* registeredID */

	/* X.509 certificate extensions.  Although it would be nicer to use names
	   that match the extensions more closely (e.g.
	   CRYPTCERT_PROP_BASICCONSTRAINTS_PATHLENCONSTRAINT), these exceed the
	   32-character ANSI minimum length for unique names, and get really
	   hairy once you get into the weird policy constraints extensions whose
	   names wrap around the screen about three times.

	   The following values are defined in OID order, this isn't absolutely
	   necessary but saves an extra layer of processing when encoding them */

	/* 1 2 840 113549 1 9 7 challengePassword.  This is here even though it's
	   a CMS attribute because SCEP stuffs it into PKCS #10 requests */
	CRYPTCERT_PROP_CHALLENGEPASSWORD,

	/* 1 3 6 1 4 1 3029 3 1 4 cRLExtReason */
	CRYPTCERT_PROP_CRLEXTREASON,

	/* 1 3 6 1 4 1 3029 3 1 5 keyFeatures */
	CRYPTCERT_PROP_KEYFEATURES,

	/* 1 3 6 1 5 5 7 1 1 authorityInfoAccess */
	CRYPTCERT_PROP_AUTHORITYINFOACCESS,
	CRYPTCERT_PROP_AUTHORITYINFO_RTCS,		/* accessDescription.accessLocation */
	CRYPTCERT_PROP_AUTHORITYINFO_OCSP,		/* accessDescription.accessLocation */
	CRYPTCERT_PROP_AUTHORITYINFO_CAISSUERS,	/* accessDescription.accessLocation */
	CRYPTCERT_PROP_AUTHORITYINFO_CERTSTORE,	/* accessDescription.accessLocation */
	CRYPTCERT_PROP_AUTHORITYINFO_CRLS,		/* accessDescription.accessLocation */

	/* 1 3 6 1 5 5 7 1 2 biometricInfo */
	CRYPTCERT_PROP_BIOMETRICINFO,
	CRYPTCERT_PROP_BIOMETRICINFO_TYPE,		/* biometricData.typeOfData */
	CRYPTCERT_PROP_BIOMETRICINFO_HASHALGO,	/* biometricData.hashAlgorithm */
	CRYPTCERT_PROP_BIOMETRICINFO_HASH,		/* biometricData.dataHash */
	CRYPTCERT_PROP_BIOMETRICINFO_URL,		/* biometricData.sourceDataUri */

	/* 1 3 6 1 5 5 7 1 3 qcStatements */
	CRYPTCERT_PROP_QCSTATEMENT,
	CRYPTCERT_PROP_QCSTATEMENT_SEMANTICS,
					/* qcStatement.statementInfo.semanticsIdentifier */
	CRYPTCERT_PROP_QCSTATEMENT_REGISTRATIONAUTHORITY,
					/* qcStatement.statementInfo.nameRegistrationAuthorities */

	/* 1 3 6 1 5 5 7 1 7 ipAddrBlocks */
	CRYPTCERT_PROP_IPADDRESSBLOCKS,
	CRYPTCERT_PROP_IPADDRESSBLOCKS_ADDRESSFAMILY,	/* addressFamily */
/*	CRYPTCERT_PROP_IPADDRESSBLOCKS_INHERIT,	// ipAddress.inherit */
	CRYPTCERT_PROP_IPADDRESSBLOCKS_PREFIX,	/* ipAddress.addressPrefix */
	CRYPTCERT_PROP_IPADDRESSBLOCKS_MIN,		/* ipAddress.addressRangeMin */
	CRYPTCERT_PROP_IPADDRESSBLOCKS_MAX,		/* ipAddress.addressRangeMax */

	/* 1 3 6 1 5 5 7 1 8 autonomousSysIds */
	CRYPTCERT_PROP_AUTONOMOUSSYSIDS,
/*	CRYPTCERT_PROP_AUTONOMOUSSYSIDS_ASNUM_INHERIT,// asNum.inherit */
	CRYPTCERT_PROP_AUTONOMOUSSYSIDS_ASNUM_ID,	/* asNum.id */
	CRYPTCERT_PROP_AUTONOMOUSSYSIDS_ASNUM_MIN,	/* asNum.min */
	CRYPTCERT_PROP_AUTONOMOUSSYSIDS_ASNUM_MAX,	/* asNum.max */

	/* 1 3 6 1 5 5 7 48 1 2 ocspNonce */
	CRYPTCERT_PROP_OCSP_NONCE,				/* nonce */

	/* 1 3 6 1 5 5 7 48 1 4 ocspAcceptableResponses */
	CRYPTCERT_PROP_OCSP_RESPONSE,
	CRYPTCERT_PROP_OCSP_RESPONSE_OCSP,		/* OCSP standard response */

	/* 1 3 6 1 5 5 7 48 1 5 ocspNoCheck */
	CRYPTCERT_PROP_OCSP_NOCHECK,

	/* 1 3 6 1 5 5 7 48 1 6 ocspArchiveCutoff */
	CRYPTCERT_PROP_OCSP_ARCHIVECUTOFF,

	/* 1 3 6 1 5 5 7 48 1 11 subjectInfoAccess */
	CRYPTCERT_PROP_SUBJECTINFOACCESS,
	CRYPTCERT_PROP_SUBJECTINFO_TIMESTAMPING,/* accessDescription.accessLocation */
	CRYPTCERT_PROP_SUBJECTINFO_CAREPOSITORY,/* accessDescription.accessLocation */
	CRYPTCERT_PROP_SUBJECTINFO_SIGNEDOBJECTREPOSITORY,/* accessDescription.accessLocation */
	CRYPTCERT_PROP_SUBJECTINFO_RPKIMANIFEST,/* accessDescription.accessLocation */
	CRYPTCERT_PROP_SUBJECTINFO_SIGNEDOBJECT,/* accessDescription.accessLocation */

	/* 1 3 36 8 3 1 siggDateOfCertGen */
	CRYPTCERT_PROP_SIGG_DATEOFCERTGEN,

	/* 1 3 36 8 3 2 siggProcuration */
	CRYPTCERT_PROP_SIGG_PROCURATION,
	CRYPTCERT_PROP_SIGG_PROCURE_COUNTRY,	/* country */
	CRYPTCERT_PROP_SIGG_PROCURE_TYPEOFSUBSTITUTION,	/* typeOfSubstitution */
	CRYPTCERT_PROP_SIGG_PROCURE_SIGNINGFOR,	/* signingFor.thirdPerson */

	/* 1 3 36 8 3 3 siggAdmissions */
	CRYPTCERT_PROP_SIGG_ADMISSIONS,
	CRYPTCERT_PROP_SIGG_ADMISSIONS_AUTHORITY,	/* authority */
	CRYPTCERT_PROP_SIGG_ADMISSIONS_NAMINGAUTHID,	/* namingAuth.iD */
	CRYPTCERT_PROP_SIGG_ADMISSIONS_NAMINGAUTHURL,	/* namingAuth.uRL */
	CRYPTCERT_PROP_SIGG_ADMISSIONS_NAMINGAUTHTEXT,	/* namingAuth.text */
	CRYPTCERT_PROP_SIGG_ADMISSIONS_PROFESSIONITEM,	/* professionItem */
	CRYPTCERT_PROP_SIGG_ADMISSIONS_PROFESSIONOID,	/* professionOID */
	CRYPTCERT_PROP_SIGG_ADMISSIONS_REGISTRATIONNUMBER,	/* registrationNumber */

	/* 1 3 36 8 3 4 siggMonetaryLimit */
	CRYPTCERT_PROP_SIGG_MONETARYLIMIT,
	CRYPTCERT_PROP_SIGG_MONETARY_CURRENCY,	/* currency */
	CRYPTCERT_PROP_SIGG_MONETARY_AMOUNT,	/* amount */
	CRYPTCERT_PROP_SIGG_MONETARY_EXPONENT,	/* exponent */

	/* 1 3 36 8 3 5 siggDeclarationOfMajority */
	CRYPTCERT_PROP_SIGG_DECLARATIONOFMAJORITY,
	CRYPTCERT_PROP_SIGG_DECLARATIONOFMAJORITY_COUNTRY,	/* fullAgeAtCountry */

	/* 1 3 36 8 3 8 siggRestriction */
	CRYPTCERT_PROP_SIGG_RESTRICTION,

	/* 1 3 36 8 3 13 siggCertHash */
	CRYPTCERT_PROP_SIGG_CERTHASH,

	/* 1 3 36 8 3 15 siggAdditionalInformation */
	CRYPTCERT_PROP_SIGG_ADDITIONALINFORMATION,

	/* 1 3 101 1 4 1 strongExtranet */
	CRYPTCERT_PROP_STRONGEXTRANET,
	CRYPTCERT_PROP_STRONGEXTRANET_ZONE,		/* sxNetIDList.sxNetID.zone */
	CRYPTCERT_PROP_STRONGEXTRANET_ID,		/* sxNetIDList.sxNetID.id */

	/* 2 5 29 9 subjectDirectoryAttributes */
	CRYPTCERT_PROP_SUBJECTDIRECTORYATTRIBUTES,
	CRYPTCERT_PROP_SUBJECTDIR_TYPE,			/* attribute.type */
	CRYPTCERT_PROP_SUBJECTDIR_VALUES,		/* attribute.values */

	/* 2 5 29 14 subjectKeyIdentifier */
	CRYPTCERT_PROP_SUBJECTKEYIDENTIFIER,

	/* 2 5 29 15 keyUsage */
	CRYPTCERT_PROP_KEYUSAGE,

	/* 2 5 29 16 privateKeyUsagePeriod */
	CRYPTCERT_PROP_PRIVATEKEYUSAGEPERIOD,
	CRYPTCERT_PROP_PRIVATEKEY_NOTBEFORE,	/* notBefore */
	CRYPTCERT_PROP_PRIVATEKEY_NOTAFTER,		/* notAfter */

	/* 2 5 29 17 subjectAltName */
	CRYPTCERT_PROP_SUBJECTALTNAME,

	/* 2 5 29 18 issuerAltName */
	CRYPTCERT_PROP_ISSUERALTNAME,

	/* 2 5 29 19 basicConstraints */
	CRYPTCERT_PROP_BASICCONSTRAINTS,
	CRYPTCERT_PROP_CA,						/* cA */
	CRYPTCERT_PROP_PATHLENCONSTRAINT,		/* pathLenConstraint */

	/* 2 5 29 20 cRLNumber */
	CRYPTCERT_PROP_CRLNUMBER,

	/* 2 5 29 21 cRLReason */
	CRYPTCERT_PROP_CRLREASON,

	/* 2 5 29 23 holdInstructionCode */
	CRYPTCERT_PROP_HOLDINSTRUCTIONCODE,

	/* 2 5 29 24 invalidityDate */
	CRYPTCERT_PROP_INVALIDITYDATE,

	/* 2 5 29 27 deltaCRLIndicator */
	CRYPTCERT_PROP_DELTACRLINDICATOR,

	/* 2 5 29 28 issuingDistributionPoint */
	CRYPTCERT_PROP_ISSUINGDISTRIBUTIONPOINT,
	CRYPTCERT_PROP_ISSUINGDIST_FULLNAME,	/* distributionPointName.fullName */
	CRYPTCERT_PROP_ISSUINGDIST_USERCERTSONLY,	/* onlyContainsUserCerts */
	CRYPTCERT_PROP_ISSUINGDIST_CACERTSONLY,	/* onlyContainsCACerts */
	CRYPTCERT_PROP_ISSUINGDIST_SOMEREASONSONLY,	/* onlySomeReasons */
	CRYPTCERT_PROP_ISSUINGDIST_INDIRECTCRL,	/* indirectCRL */

	/* 2 5 29 29 certificateIssuer */
	CRYPTCERT_PROP_CERTIFICATEISSUER,

	/* 2 5 29 30 nameConstraints */
	CRYPTCERT_PROP_NAMECONSTRAINTS,
	CRYPTCERT_PROP_PERMITTEDSUBTREES,		/* permittedSubtrees */
	CRYPTCERT_PROP_EXCLUDEDSUBTREES,		/* excludedSubtrees */

	/* 2 5 29 31 cRLDistributionPoint */
	CRYPTCERT_PROP_CRLDISTRIBUTIONPOINT,
	CRYPTCERT_PROP_CRLDIST_FULLNAME,		/* distributionPointName.fullName */
	CRYPTCERT_PROP_CRLDIST_REASONS,			/* reasons */
	CRYPTCERT_PROP_CRLDIST_CRLISSUER,		/* cRLIssuer */

	/* 2 5 29 32 certificatePolicies */
	CRYPTCERT_PROP_CERTIFICATEPOLICIES,
	CRYPTCERT_PROP_CERTPOLICYID,		/* policyInformation.policyIdentifier */
	CRYPTCERT_PROP_CERTPOLICY_CPSURI,
		/* policyInformation.policyQualifiers.qualifier.cPSuri */
	CRYPTCERT_PROP_CERTPOLICY_ORGANIZATION,
		/* policyInformation.policyQualifiers.qualifier.userNotice.noticeRef.organization */
	CRYPTCERT_PROP_CERTPOLICY_NOTICENUMBERS,
		/* policyInformation.policyQualifiers.qualifier.userNotice.noticeRef.noticeNumbers */
	CRYPTCERT_PROP_CERTPOLICY_EXPLICITTEXT,
		/* policyInformation.policyQualifiers.qualifier.userNotice.explicitText */

	/* 2 5 29 33 policyMappings */
	CRYPTCERT_PROP_POLICYMAPPINGS,
	CRYPTCERT_PROP_ISSUERDOMAINPOLICY,	/* policyMappings.issuerDomainPolicy */
	CRYPTCERT_PROP_SUBJECTDOMAINPOLICY,	/* policyMappings.subjectDomainPolicy */

	/* 2 5 29 35 authorityKeyIdentifier */
	CRYPTCERT_PROP_AUTHORITYKEYIDENTIFIER,
	CRYPTCERT_PROP_AUTHORITY_KEYIDENTIFIER,	/* keyIdentifier */
	CRYPTCERT_PROP_AUTHORITY_CERTISSUER,	/* authorityCertIssuer */
	CRYPTCERT_PROP_AUTHORITY_CERTSERIALNUMBER,	/* authorityCertSerialNumber */

	/* 2 5 29 36 policyConstraints */
	CRYPTCERT_PROP_POLICYCONSTRAINTS,
	CRYPTCERT_PROP_REQUIREEXPLICITPOLICY,	/* policyConstraints.requireExplicitPolicy */
	CRYPTCERT_PROP_INHIBITPOLICYMAPPING,	/* policyConstraints.inhibitPolicyMapping */

	/* 2 5 29 37 extKeyUsage */
	CRYPTCERT_PROP_EXTKEYUSAGE,
	CRYPTCERT_PROP_EXTKEY_MS_INDIVIDUALCODESIGNING,	/* individualCodeSigning */
	CRYPTCERT_PROP_EXTKEY_MS_COMMERCIALCODESIGNING,	/* commercialCodeSigning */
	CRYPTCERT_PROP_EXTKEY_MS_CERTTRUSTLISTSIGNING,	/* certTrustListSigning */
	CRYPTCERT_PROP_EXTKEY_MS_TIMESTAMPSIGNING,	/* timeStampSigning */
	CRYPTCERT_PROP_EXTKEY_MS_SERVERGATEDCRYPTO,	/* serverGatedCrypto */
	CRYPTCERT_PROP_EXTKEY_MS_ENCRYPTEDFILESYSTEM,	/* encrypedFileSystem */
	CRYPTCERT_PROP_EXTKEY_SERVERAUTH,		/* serverAuth */
	CRYPTCERT_PROP_EXTKEY_CLIENTAUTH,		/* clientAuth */
	CRYPTCERT_PROP_EXTKEY_CODESIGNING,		/* codeSigning */
	CRYPTCERT_PROP_EXTKEY_EMAILPROTECTION,	/* emailProtection */
	CRYPTCERT_PROP_EXTKEY_IPSECENDSYSTEM,	/* ipsecEndSystem */
	CRYPTCERT_PROP_EXTKEY_IPSECTUNNEL,		/* ipsecTunnel */
	CRYPTCERT_PROP_EXTKEY_IPSECUSER,		/* ipsecUser */
	CRYPTCERT_PROP_EXTKEY_TIMESTAMPING,		/* timeStamping */
	CRYPTCERT_PROP_EXTKEY_OCSPSIGNING,		/* ocspSigning */
	CRYPTCERT_PROP_EXTKEY_DIRECTORYSERVICE,	/* directoryService */
	CRYPTCERT_PROP_EXTKEY_ANYKEYUSAGE,		/* anyExtendedKeyUsage */
	CRYPTCERT_PROP_EXTKEY_NS_SERVERGATEDCRYPTO,	/* serverGatedCrypto */
	CRYPTCERT_PROP_EXTKEY_VS_SERVERGATEDCRYPTO_CA,	/* serverGatedCrypto CA */

	/* 2 5 29 40 crlStreamIdentifier */
	CRYPTCERT_PROP_CRLSTREAMIDENTIFIER,

	/* 2 5 29 46 freshestCRL */
	CRYPTCERT_PROP_FRESHESTCRL,
	CRYPTCERT_PROP_FRESHESTCRL_FULLNAME,	/* distributionPointName.fullName */
	CRYPTCERT_PROP_FRESHESTCRL_REASONS,		/* reasons */
	CRYPTCERT_PROP_FRESHESTCRL_CRLISSUER,	/* cRLIssuer */

	/* 2 5 29 47 orderedList */
	CRYPTCERT_PROP_ORDEREDLIST,

	/* 2 5 29 51 baseUpdateTime */
	CRYPTCERT_PROP_BASEUPDATETIME,

	/* 2 5 29 53 deltaInfo */
	CRYPTCERT_PROP_DELTAINFO,
	CRYPTCERT_PROP_DELTAINFO_LOCATION,		/* deltaLocation */
	CRYPTCERT_PROP_DELTAINFO_NEXTDELTA,		/* nextDelta */

	/* 2 5 29 54 inhibitAnyPolicy */
	CRYPTCERT_PROP_INHIBITANYPOLICY,

	/* 2 5 29 58 toBeRevoked */
	CRYPTCERT_PROP_TOBEREVOKED,
	CRYPTCERT_PROP_TOBEREVOKED_CERTISSUER,	/* certificateIssuer */
	CRYPTCERT_PROP_TOBEREVOKED_REASONCODE,	/* reasonCode */
	CRYPTCERT_PROP_TOBEREVOKED_REVOCATIONTIME,	/* revocationTime */
	CRYPTCERT_PROP_TOBEREVOKED_CERTSERIALNUMBER,/* certSerialNumber */

	/* 2 5 29 59 revokedGroups */
	CRYPTCERT_PROP_REVOKEDGROUPS,
	CRYPTCERT_PROP_REVOKEDGROUPS_CERTISSUER,/* certificateIssuer */
	CRYPTCERT_PROP_REVOKEDGROUPS_REASONCODE,/* reasonCode */
	CRYPTCERT_PROP_REVOKEDGROUPS_INVALIDITYDATE,/* invalidityDate */
	CRYPTCERT_PROP_REVOKEDGROUPS_STARTINGNUMBER,/* startingNumber */
	CRYPTCERT_PROP_REVOKEDGROUPS_ENDINGNUMBER,	/* endingNumber */

	/* 2 5 29 60 expiredCertsOnCRL */
	CRYPTCERT_PROP_EXPIREDCERTSONCRL,

	/* 2 5 29 63 aaIssuingDistributionPoint */
	CRYPTCERT_PROP_AAISSUINGDISTRIBUTIONPOINT,
	CRYPTCERT_PROP_AAISSUINGDIST_FULLNAME,	/* distributionPointName.fullName */
	CRYPTCERT_PROP_AAISSUINGDIST_SOMEREASONSONLY,/* onlySomeReasons */
	CRYPTCERT_PROP_AAISSUINGDIST_INDIRECTCRL,	/* indirectCRL */
	CRYPTCERT_PROP_AAISSUINGDIST_USERATTRCERTS,	/* containsUserAttributeCerts */
	CRYPTCERT_PROP_AAISSUINGDIST_AACERTS,	/* containsAACerts */
	CRYPTCERT_PROP_AAISSUINGDIST_SOACERTS,	/* containsSOAPublicKeyCerts */

	/* 2 16 840 1 113730 1 x Netscape extensions */
	CRYPTCERT_PROP_NS_CERTTYPE,				/* netscape-cert-type */
	CRYPTCERT_PROP_NS_BASEURL,				/* netscape-base-url */
	CRYPTCERT_PROP_NS_REVOCATIONURL,		/* netscape-revocation-url */
	CRYPTCERT_PROP_NS_CAREVOCATIONURL,		/* netscape-ca-revocation-url */
	CRYPTCERT_PROP_NS_CERTRENEWALURL,		/* netscape-cert-renewal-url */
	CRYPTCERT_PROP_NS_CAPOLICYURL,			/* netscape-ca-policy-url */
	CRYPTCERT_PROP_NS_SSLSERVERNAME,		/* netscape-ssl-server-name */
	CRYPTCERT_PROP_NS_COMMENT,				/* netscape-comment */

	/* 2 23 42 7 0 SET hashedRootKey */
	CRYPTCERT_PROP_SET_HASHEDROOTKEY,
	CRYPTCERT_PROP_SET_ROOTKEYTHUMBPRINT,	/* rootKeyThumbPrint */

	/* 2 23 42 7 1 SET certificateType */
	CRYPTCERT_PROP_SET_CERTIFICATETYPE,

	/* 2 23 42 7 2 SET merchantData */
	CRYPTCERT_PROP_SET_MERCHANTDATA,
	CRYPTCERT_PROP_SET_MERID,				/* merID */
	CRYPTCERT_PROP_SET_MERACQUIRERBIN,		/* merAcquirerBIN */
	CRYPTCERT_PROP_SET_MERCHANTLANGUAGE,	/* merNames.language */
	CRYPTCERT_PROP_SET_MERCHANTNAME,		/* merNames.name */
	CRYPTCERT_PROP_SET_MERCHANTCITY,		/* merNames.city */
	CRYPTCERT_PROP_SET_MERCHANTSTATEPROVINCE,/* merNames.stateProvince */
	CRYPTCERT_PROP_SET_MERCHANTPOSTALCODE,	/* merNames.postalCode */
	CRYPTCERT_PROP_SET_MERCHANTCOUNTRYNAME,	/* merNames.countryName */
	CRYPTCERT_PROP_SET_MERCOUNTRY,			/* merCountry */
	CRYPTCERT_PROP_SET_MERAUTHFLAG,			/* merAuthFlag */

	/* 2 23 42 7 3 SET certCardRequired */
	CRYPTCERT_PROP_SET_CERTCARDREQUIRED,

	/* 2 23 42 7 4 SET tunneling */
	CRYPTCERT_PROP_SET_TUNNELING,
	CRYPTCERT_PROP_SET_TUNNELINGFLAG,		/* tunneling */
	CRYPTCERT_PROP_SET_TUNNELINGALGID,		/* tunnelingAlgID */

	/* S/MIME attributes */

	/* 1 2 840 113549 1 9 3 contentType */
	CRYPTCERT_PROP_CMS_CONTENTTYPE,

	/* 1 2 840 113549 1 9 4 messageDigest */
	CRYPTCERT_PROP_CMS_MESSAGEDIGEST,

	/* 1 2 840 113549 1 9 5 signingTime */
	CRYPTCERT_PROP_CMS_SIGNINGTIME,

	/* 1 2 840 113549 1 9 6 counterSignature */
	CRYPTCERT_PROP_CMS_COUNTERSIGNATURE,	/* counterSignature */

	/* 1 2 840 113549 1 9 13 signingDescription */
	CRYPTCERT_PROP_CMS_SIGNINGDESCRIPTION,

	/* 1 2 840 113549 1 9 15 sMIMECapabilities */
	CRYPTCERT_PROP_CMS_SMIMECAPABILITIES,
	CRYPTCERT_PROP_CMS_SMIMECAP_3DES,		/* 3DES encryption */
	CRYPTCERT_PROP_CMS_SMIMECAP_AES,		/* AES encryption */
	CRYPTCERT_PROP_CMS_SMIMECAP_CAST128,	/* CAST-128 encryption */
	CRYPTCERT_PROP_CMS_SMIMECAP_SHAng,		/* SHA2-ng hash */
	CRYPTCERT_PROP_CMS_SMIMECAP_SHA2,		/* SHA2-256 hash */
	CRYPTCERT_PROP_CMS_SMIMECAP_SHA1,		/* SHA1 hash */
	CRYPTCERT_PROP_CMS_SMIMECAP_HMAC_SHAng,	/* HMAC-SHA2-ng MAC */
	CRYPTCERT_PROP_CMS_SMIMECAP_HMAC_SHA2,	/* HMAC-SHA2-256 MAC */
	CRYPTCERT_PROP_CMS_SMIMECAP_HMAC_SHA1,	/* HMAC-SHA1 MAC */
	CRYPTCERT_PROP_CMS_SMIMECAP_AUTHENC256,	/* AuthEnc w.256-bit key */
	CRYPTCERT_PROP_CMS_SMIMECAP_AUTHENC128,	/* AuthEnc w.128-bit key */
	CRYPTCERT_PROP_CMS_SMIMECAP_RSA_SHAng,	/* RSA with SHA-ng signing */
	CRYPTCERT_PROP_CMS_SMIMECAP_RSA_SHA2,	/* RSA with SHA2-256 signing */
	CRYPTCERT_PROP_CMS_SMIMECAP_RSA_SHA1,	/* RSA with SHA1 signing */
	CRYPTCERT_PROP_CMS_SMIMECAP_DSA_SHA1,	/* DSA with SHA-1 signing */
	CRYPTCERT_PROP_CMS_SMIMECAP_ECDSA_SHAng,/* ECDSA with SHA-ng signing */
	CRYPTCERT_PROP_CMS_SMIMECAP_ECDSA_SHA2,	/* ECDSA with SHA2-256 signing */
	CRYPTCERT_PROP_CMS_SMIMECAP_ECDSA_SHA1,	/* ECDSA with SHA-1 signing */
	CRYPTCERT_PROP_CMS_SMIMECAP_PREFERSIGNEDDATA,	/* preferSignedData */
	CRYPTCERT_PROP_CMS_SMIMECAP_CANNOTDECRYPTANY,	/* canNotDecryptAny */
	CRYPTCERT_PROP_CMS_SMIMECAP_PREFERBINARYINSIDE,	/* preferBinaryInside */

	/* 1 2 840 113549 1 9 16 2 1 receiptRequest */
	CRYPTCERT_PROP_CMS_RECEIPTREQUEST,
	CRYPTCERT_PROP_CMS_RECEIPT_CONTENTIDENTIFIER, /* contentIdentifier */
	CRYPTCERT_PROP_CMS_RECEIPT_FROM,		/* receiptsFrom */
	CRYPTCERT_PROP_CMS_RECEIPT_TO,			/* receiptsTo */

	/* 1 2 840 113549 1 9 16 2 2 essSecurityLabel */
	CRYPTCERT_PROP_CMS_SECURITYLABEL,
	CRYPTCERT_PROP_CMS_SECLABEL_POLICY,		/* securityPolicyIdentifier */
	CRYPTCERT_PROP_CMS_SECLABEL_CLASSIFICATION, /* securityClassification */
	CRYPTCERT_PROP_CMS_SECLABEL_PRIVACYMARK,/* privacyMark */
	CRYPTCERT_PROP_CMS_SECLABEL_CATTYPE,	/* securityCategories.securityCategory.type */
	CRYPTCERT_PROP_CMS_SECLABEL_CATVALUE,	/* securityCategories.securityCategory.value */

	/* 1 2 840 113549 1 9 16 2 3 mlExpansionHistory */
	CRYPTCERT_PROP_CMS_MLEXPANSIONHISTORY,
	CRYPTCERT_PROP_CMS_MLEXP_ENTITYIDENTIFIER, /* mlData.mailListIdentifier.issuerAndSerialNumber */
	CRYPTCERT_PROP_CMS_MLEXP_TIME,			/* mlData.expansionTime */
	CRYPTCERT_PROP_CMS_MLEXP_NONE,			/* mlData.mlReceiptPolicy.none */
	CRYPTCERT_PROP_CMS_MLEXP_INSTEADOF,		/* mlData.mlReceiptPolicy.insteadOf.generalNames.generalName */
	CRYPTCERT_PROP_CMS_MLEXP_INADDITIONTO,	/* mlData.mlReceiptPolicy.inAdditionTo.generalNames.generalName */

	/* 1 2 840 113549 1 9 16 2 4 contentHints */
	CRYPTCERT_PROP_CMS_CONTENTHINTS,
	CRYPTCERT_PROP_CMS_CONTENTHINT_DESCRIPTION,	/* contentDescription */
	CRYPTCERT_PROP_CMS_CONTENTHINT_TYPE,	/* contentType */

	/* 1 2 840 113549 1 9 16 2 9 equivalentLabels */
	CRYPTCERT_PROP_CMS_EQUIVALENTLABEL,
	CRYPTCERT_PROP_CMS_EQVLABEL_POLICY,		/* securityPolicyIdentifier */
	CRYPTCERT_PROP_CMS_EQVLABEL_CLASSIFICATION, /* securityClassification */
	CRYPTCERT_PROP_CMS_EQVLABEL_PRIVACYMARK,/* privacyMark */
	CRYPTCERT_PROP_CMS_EQVLABEL_CATTYPE,	/* securityCategories.securityCategory.type */
	CRYPTCERT_PROP_CMS_EQVLABEL_CATVALUE,	/* securityCategories.securityCategory.value */

	/* 1 2 840 113549 1 9 16 2 12 signingCertificate */
	CRYPTCERT_PROP_CMS_SIGNINGCERTIFICATE,
	CRYPTCERT_PROP_CMS_SIGNINGCERT_ESSCERTID, /* certs.essCertID */
	CRYPTCERT_PROP_CMS_SIGNINGCERT_POLICIES,/* policies.policyInformation.policyIdentifier */

	/* 1 2 840 113549 1 9 16 2 47 signingCertificateV2 */
	CRYPTCERT_PROP_CMS_SIGNINGCERTIFICATEV2,
	CRYPTCERT_PROP_CMS_SIGNINGCERTV2_ESSCERTIDV2, /* certs.essCertID */
	CRYPTCERT_PROP_CMS_SIGNINGCERTV2_POLICIES,/* policies.policyInformation.policyIdentifier */

	/* 1 2 840 113549 1 9 16 2 15 signaturePolicyID */
	CRYPTCERT_PROP_CMS_SIGNATUREPOLICYID,
	CRYPTCERT_PROP_CMS_SIGPOLICYID,			/* sigPolicyID */
	CRYPTCERT_PROP_CMS_SIGPOLICYHASH,		/* sigPolicyHash */
	CRYPTCERT_PROP_CMS_SIGPOLICY_CPSURI,	/* sigPolicyQualifiers.sigPolicyQualifier.cPSuri */
	CRYPTCERT_PROP_CMS_SIGPOLICY_ORGANIZATION,
		/* sigPolicyQualifiers.sigPolicyQualifier.userNotice.noticeRef.organization */
	CRYPTCERT_PROP_CMS_SIGPOLICY_NOTICENUMBERS,
		/* sigPolicyQualifiers.sigPolicyQualifier.userNotice.noticeRef.noticeNumbers */
	CRYPTCERT_PROP_CMS_SIGPOLICY_EXPLICITTEXT,
		/* sigPolicyQualifiers.sigPolicyQualifier.userNotice.explicitText */

	/* 1 2 840 113549 1 9 16 9 signatureTypeIdentifier */
	CRYPTCERT_PROP_CMS_SIGTYPEIDENTIFIER,
	CRYPTCERT_PROP_CMS_SIGTYPEID_ORIGINATORSIG, /* originatorSig */
	CRYPTCERT_PROP_CMS_SIGTYPEID_DOMAINSIG,	/* domainSig */
	CRYPTCERT_PROP_CMS_SIGTYPEID_ADDITIONALATTRIBUTES, /* additionalAttributesSig */
	CRYPTCERT_PROP_CMS_SIGTYPEID_REVIEWSIG,	/* reviewSig */

	/* 1 2 840 113549 1 9 25 3 randomNonce */
	CRYPTCERT_PROP_CMS_NONCE,				/* randomNonce */

	/* SCEP attributes:
	   2 16 840 1 113733 1 9 2 messageType
	   2 16 840 1 113733 1 9 3 pkiStatus
	   2 16 840 1 113733 1 9 4 failInfo
	   2 16 840 1 113733 1 9 5 senderNonce
	   2 16 840 1 113733 1 9 6 recipientNonce
	   2 16 840 1 113733 1 9 7 transID */
	CRYPTCERT_PROP_SCEP_MESSAGETYPE,		/* messageType */
	CRYPTCERT_PROP_SCEP_PKISTATUS,			/* pkiStatus */
	CRYPTCERT_PROP_SCEP_FAILINFO,			/* failInfo */
	CRYPTCERT_PROP_SCEP_SENDERNONCE,		/* senderNonce */
	CRYPTCERT_PROP_SCEP_RECIPIENTNONCE,		/* recipientNonce */
	CRYPTCERT_PROP_SCEP_TRANSACTIONID,		/* transID */

	/* 1 3 6 1 4 1 311 2 1 10 spcAgencyInfo */
	CRYPTCERT_PROP_CMS_SPCAGENCYINFO,
	CRYPTCERT_PROP_CMS_SPCAGENCYURL,		/* spcAgencyInfo.url */

	/* 1 3 6 1 4 1 311 2 1 11 spcStatementType */
	CRYPTCERT_PROP_CMS_SPCSTATEMENTTYPE,
	CRYPTCERT_PROP_CMS_SPCSTMT_INDIVIDUALCODESIGNING,	/* individualCodeSigning */
	CRYPTCERT_PROP_CMS_SPCSTMT_COMMERCIALCODESIGNING,	/* commercialCodeSigning */

	/* 1 3 6 1 4 1 311 2 1 12 spcOpusInfo */
	CRYPTCERT_PROP_CMS_SPCOPUSINFO,
	CRYPTCERT_PROP_CMS_SPCOPUSINFO_NAME,	/* spcOpusInfo.name */
	CRYPTCERT_PROP_CMS_SPCOPUSINFO_URL		/* spcOpusInfo.url */

};

static JSBool
js_cryptcert_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval idval;
	jsint tiny;
	struct js_cryptcert_private_data* p;

	if ((p=(struct js_cryptcert_private_data *)JS_GetPrivate(cx,obj))==NULL) {
		return JS_TRUE;
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	JS_IdToValue(cx, id, &idval);
	tiny = JSVAL_TO_INT(idval);

	switch (tiny) {
		case CRYPTCERT_PROP_SELFSIGNED:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SELFSIGNED);
		case CRYPTCERT_PROP_IMMUTABLE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_IMMUTABLE);
		case CRYPTCERT_PROP_XYZZY:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_XYZZY);
		case CRYPTCERT_PROP_CERTTYPE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CERTTYPE);
		case CRYPTCERT_PROP_FINGERPRINT_SHA1:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_FINGERPRINT_SHA1);
		case CRYPTCERT_PROP_FINGERPRINT_SHA2:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_FINGERPRINT_SHA2);
		case CRYPTCERT_PROP_FINGERPRINT_SHAng:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_FINGERPRINT_SHAng);
		case CRYPTCERT_PROP_CURRENT_CERTIFICATE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CURRENT_CERTIFICATE);
		case CRYPTCERT_PROP_TRUSTED_USAGE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_TRUSTED_USAGE);
		case CRYPTCERT_PROP_TRUSTED_IMPLICIT:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_TRUSTED_IMPLICIT);
		case CRYPTCERT_PROP_SIGNATURELEVEL:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGNATURELEVEL);
		case CRYPTCERT_PROP_VERSION:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_VERSION);
		case CRYPTCERT_PROP_SERIALNUMBER:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SERIALNUMBER);
		case CRYPTCERT_PROP_SUBJECTPUBLICKEYINFO:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO);
		case CRYPTCERT_PROP_CERTIFICATE:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CERTIFICATE);
		case CRYPTCERT_PROP_CACERTIFICATE:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CACERTIFICATE);
		case CRYPTCERT_PROP_ISSUERNAME:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_ISSUERNAME);
		case CRYPTCERT_PROP_VALIDFROM:
			return js_cryptcert_attrtime_get(cx, vp, p->cert, CRYPT_CERTINFO_VALIDFROM);
		case CRYPTCERT_PROP_VALIDTO:
			return js_cryptcert_attrtime_get(cx, vp, p->cert, CRYPT_CERTINFO_VALIDTO);
		case CRYPTCERT_PROP_SUBJECTNAME:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTNAME);
		case CRYPTCERT_PROP_ISSUERUNIQUEID:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_ISSUERUNIQUEID);
		case CRYPTCERT_PROP_SUBJECTUNIQUEID:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTUNIQUEID);
		case CRYPTCERT_PROP_CERTREQUEST:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CERTREQUEST);
		case CRYPTCERT_PROP_THISUPDATE:
			return js_cryptcert_attrtime_get(cx, vp, p->cert, CRYPT_CERTINFO_THISUPDATE);
		case CRYPTCERT_PROP_NEXTUPDATE:
			return js_cryptcert_attrtime_get(cx, vp, p->cert, CRYPT_CERTINFO_NEXTUPDATE);
		case CRYPTCERT_PROP_REVOCATIONDATE:
			return js_cryptcert_attrtime_get(cx, vp, p->cert, CRYPT_CERTINFO_REVOCATIONDATE);
		case CRYPTCERT_PROP_REVOCATIONSTATUS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_REVOCATIONSTATUS);
		case CRYPTCERT_PROP_CERTSTATUS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CERTSTATUS);
		case CRYPTCERT_PROP_DN:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_DN);
		case CRYPTCERT_PROP_PKIUSER_ID:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_PKIUSER_ID);
		case CRYPTCERT_PROP_PKIUSER_ISSUEPASSWORD:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_PKIUSER_ISSUEPASSWORD);
		case CRYPTCERT_PROP_PKIUSER_REVPASSWORD:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_PKIUSER_REVPASSWORD);
		case CRYPTCERT_PROP_PKIUSER_RA:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_PKIUSER_RA);
		case CRYPTCERT_PROP_COUNTRYNAME:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_COUNTRYNAME);
		case CRYPTCERT_PROP_STATEORPROVINCENAME:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_STATEORPROVINCENAME);
		case CRYPTCERT_PROP_LOCALITYNAME:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_LOCALITYNAME);
		case CRYPTCERT_PROP_ORGANIZATIONNAME:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_ORGANIZATIONNAME);
		case CRYPTCERT_PROP_ORGANIZATIONALUNITNAME:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_ORGANIZATIONALUNITNAME);
		case CRYPTCERT_PROP_COMMONNAME:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_COMMONNAME);
		case CRYPTCERT_PROP_OTHERNAME_TYPEID:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_OTHERNAME_TYPEID);
		case CRYPTCERT_PROP_OTHERNAME_VALUE:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_OTHERNAME_VALUE);
		case CRYPTCERT_PROP_RFC822NAME:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_RFC822NAME);
		case CRYPTCERT_PROP_DNSNAME:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_DNSNAME);
		case CRYPTCERT_PROP_DIRECTORYNAME:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_DIRECTORYNAME);
		case CRYPTCERT_PROP_EDIPARTYNAME_NAMEASSIGNER:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_EDIPARTYNAME_NAMEASSIGNER);
		case CRYPTCERT_PROP_EDIPARTYNAME_PARTYNAME:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_EDIPARTYNAME_PARTYNAME);
		case CRYPTCERT_PROP_UNIFORMRESOURCEIDENTIFIER:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER);
		case CRYPTCERT_PROP_IPADDRESS:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_IPADDRESS);
		case CRYPTCERT_PROP_REGISTEREDID:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_REGISTEREDID);
		case CRYPTCERT_PROP_CHALLENGEPASSWORD:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CHALLENGEPASSWORD);
		case CRYPTCERT_PROP_CRLEXTREASON:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CRLEXTREASON);
		case CRYPTCERT_PROP_KEYFEATURES:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_KEYFEATURES);
		case CRYPTCERT_PROP_AUTHORITYINFOACCESS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_AUTHORITYINFOACCESS);
		case CRYPTCERT_PROP_AUTHORITYINFO_RTCS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_AUTHORITYINFO_RTCS);
		case CRYPTCERT_PROP_AUTHORITYINFO_OCSP:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_AUTHORITYINFO_OCSP);
		case CRYPTCERT_PROP_AUTHORITYINFO_CAISSUERS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_AUTHORITYINFO_CAISSUERS);
		case CRYPTCERT_PROP_AUTHORITYINFO_CERTSTORE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_AUTHORITYINFO_CERTSTORE);
		case CRYPTCERT_PROP_AUTHORITYINFO_CRLS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_AUTHORITYINFO_CRLS);
		case CRYPTCERT_PROP_BIOMETRICINFO:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_BIOMETRICINFO);
		case CRYPTCERT_PROP_BIOMETRICINFO_TYPE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_BIOMETRICINFO_TYPE);
		case CRYPTCERT_PROP_BIOMETRICINFO_HASHALGO:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_BIOMETRICINFO_HASHALGO);
		case CRYPTCERT_PROP_BIOMETRICINFO_HASH:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_BIOMETRICINFO_HASH);
		case CRYPTCERT_PROP_BIOMETRICINFO_URL:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_BIOMETRICINFO_URL);
		case CRYPTCERT_PROP_QCSTATEMENT:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_QCSTATEMENT);
		case CRYPTCERT_PROP_QCSTATEMENT_SEMANTICS:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_QCSTATEMENT_SEMANTICS);
		case CRYPTCERT_PROP_QCSTATEMENT_REGISTRATIONAUTHORITY:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_QCSTATEMENT_REGISTRATIONAUTHORITY);
		case CRYPTCERT_PROP_IPADDRESSBLOCKS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_IPADDRESSBLOCKS);
		case CRYPTCERT_PROP_IPADDRESSBLOCKS_ADDRESSFAMILY:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_IPADDRESSBLOCKS_ADDRESSFAMILY);
		case CRYPTCERT_PROP_IPADDRESSBLOCKS_PREFIX:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_IPADDRESSBLOCKS_PREFIX);
		case CRYPTCERT_PROP_IPADDRESSBLOCKS_MIN:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_IPADDRESSBLOCKS_MIN);
		case CRYPTCERT_PROP_IPADDRESSBLOCKS_MAX:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_IPADDRESSBLOCKS_MAX);
		case CRYPTCERT_PROP_AUTONOMOUSSYSIDS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_AUTONOMOUSSYSIDS);
		case CRYPTCERT_PROP_AUTONOMOUSSYSIDS_ASNUM_ID:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_ID);
		case CRYPTCERT_PROP_AUTONOMOUSSYSIDS_ASNUM_MIN:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_MIN);
		case CRYPTCERT_PROP_AUTONOMOUSSYSIDS_ASNUM_MAX:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_MAX);
		case CRYPTCERT_PROP_OCSP_NONCE:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_OCSP_NONCE);
		case CRYPTCERT_PROP_OCSP_RESPONSE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_OCSP_RESPONSE);
		case CRYPTCERT_PROP_OCSP_RESPONSE_OCSP:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_OCSP_RESPONSE_OCSP);
		case CRYPTCERT_PROP_OCSP_NOCHECK:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_OCSP_NOCHECK);
		case CRYPTCERT_PROP_OCSP_ARCHIVECUTOFF:
			return js_cryptcert_attrtime_get(cx, vp, p->cert, CRYPT_CERTINFO_OCSP_ARCHIVECUTOFF);
		case CRYPTCERT_PROP_SUBJECTINFOACCESS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTINFOACCESS);
		case CRYPTCERT_PROP_SUBJECTINFO_TIMESTAMPING:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTINFO_TIMESTAMPING);
		case CRYPTCERT_PROP_SUBJECTINFO_CAREPOSITORY:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTINFO_CAREPOSITORY);
		case CRYPTCERT_PROP_SUBJECTINFO_SIGNEDOBJECTREPOSITORY:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTINFO_SIGNEDOBJECTREPOSITORY);
		case CRYPTCERT_PROP_SUBJECTINFO_RPKIMANIFEST:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTINFO_RPKIMANIFEST);
		case CRYPTCERT_PROP_SUBJECTINFO_SIGNEDOBJECT:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTINFO_SIGNEDOBJECT);
		case CRYPTCERT_PROP_SIGG_DATEOFCERTGEN:
			return js_cryptcert_attrtime_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_DATEOFCERTGEN);
		case CRYPTCERT_PROP_SIGG_PROCURATION:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_PROCURATION);
		case CRYPTCERT_PROP_SIGG_PROCURE_COUNTRY:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_PROCURE_COUNTRY);
		case CRYPTCERT_PROP_SIGG_PROCURE_TYPEOFSUBSTITUTION:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_PROCURE_TYPEOFSUBSTITUTION);
		case CRYPTCERT_PROP_SIGG_PROCURE_SIGNINGFOR:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_PROCURE_SIGNINGFOR);
		case CRYPTCERT_PROP_SIGG_ADMISSIONS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_ADMISSIONS);
		case CRYPTCERT_PROP_SIGG_ADMISSIONS_AUTHORITY:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_ADMISSIONS_AUTHORITY);
		case CRYPTCERT_PROP_SIGG_ADMISSIONS_NAMINGAUTHID:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHID);
		case CRYPTCERT_PROP_SIGG_ADMISSIONS_NAMINGAUTHURL:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHURL);
		case CRYPTCERT_PROP_SIGG_ADMISSIONS_NAMINGAUTHTEXT:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHTEXT);
		case CRYPTCERT_PROP_SIGG_ADMISSIONS_PROFESSIONITEM:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_ADMISSIONS_PROFESSIONITEM);
		case CRYPTCERT_PROP_SIGG_ADMISSIONS_PROFESSIONOID:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_ADMISSIONS_PROFESSIONOID);
		case CRYPTCERT_PROP_SIGG_ADMISSIONS_REGISTRATIONNUMBER:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_ADMISSIONS_REGISTRATIONNUMBER);
		case CRYPTCERT_PROP_SIGG_MONETARYLIMIT:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_MONETARYLIMIT);
		case CRYPTCERT_PROP_SIGG_MONETARY_CURRENCY:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_MONETARY_CURRENCY);
		case CRYPTCERT_PROP_SIGG_MONETARY_AMOUNT:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_MONETARY_AMOUNT);
		case CRYPTCERT_PROP_SIGG_MONETARY_EXPONENT:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_MONETARY_EXPONENT);
		case CRYPTCERT_PROP_SIGG_DECLARATIONOFMAJORITY:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_DECLARATIONOFMAJORITY);
		case CRYPTCERT_PROP_SIGG_DECLARATIONOFMAJORITY_COUNTRY:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_DECLARATIONOFMAJORITY_COUNTRY);
		case CRYPTCERT_PROP_SIGG_RESTRICTION:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_RESTRICTION);
		case CRYPTCERT_PROP_SIGG_CERTHASH:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_CERTHASH);
		case CRYPTCERT_PROP_SIGG_ADDITIONALINFORMATION:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_ADDITIONALINFORMATION);
		case CRYPTCERT_PROP_STRONGEXTRANET:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_STRONGEXTRANET);
		case CRYPTCERT_PROP_STRONGEXTRANET_ZONE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_STRONGEXTRANET_ZONE);
		case CRYPTCERT_PROP_STRONGEXTRANET_ID:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_STRONGEXTRANET_ID);
		case CRYPTCERT_PROP_SUBJECTDIRECTORYATTRIBUTES:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTDIRECTORYATTRIBUTES);
		case CRYPTCERT_PROP_SUBJECTDIR_TYPE:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTDIR_TYPE);
		case CRYPTCERT_PROP_SUBJECTDIR_VALUES:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTDIR_VALUES);
		case CRYPTCERT_PROP_SUBJECTKEYIDENTIFIER:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER);
		case CRYPTCERT_PROP_KEYUSAGE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_KEYUSAGE);
		case CRYPTCERT_PROP_PRIVATEKEYUSAGEPERIOD:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_PRIVATEKEYUSAGEPERIOD);
		case CRYPTCERT_PROP_PRIVATEKEY_NOTBEFORE:
			return js_cryptcert_attrtime_get(cx, vp, p->cert, CRYPT_CERTINFO_PRIVATEKEY_NOTBEFORE);
		case CRYPTCERT_PROP_PRIVATEKEY_NOTAFTER:
			return js_cryptcert_attrtime_get(cx, vp, p->cert, CRYPT_CERTINFO_PRIVATEKEY_NOTAFTER);
		case CRYPTCERT_PROP_SUBJECTALTNAME:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTALTNAME);
		case CRYPTCERT_PROP_ISSUERALTNAME:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_ISSUERALTNAME);
		case CRYPTCERT_PROP_BASICCONSTRAINTS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_BASICCONSTRAINTS);
		case CRYPTCERT_PROP_CA:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CA);
		case CRYPTCERT_PROP_PATHLENCONSTRAINT:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_PATHLENCONSTRAINT);
		case CRYPTCERT_PROP_CRLNUMBER:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CRLNUMBER);
		case CRYPTCERT_PROP_CRLREASON:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CRLREASON);
		case CRYPTCERT_PROP_HOLDINSTRUCTIONCODE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_HOLDINSTRUCTIONCODE);
		case CRYPTCERT_PROP_INVALIDITYDATE:
			return js_cryptcert_attrtime_get(cx, vp, p->cert, CRYPT_CERTINFO_INVALIDITYDATE);
		case CRYPTCERT_PROP_DELTACRLINDICATOR:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_DELTACRLINDICATOR);
		case CRYPTCERT_PROP_ISSUINGDISTRIBUTIONPOINT:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_ISSUINGDISTRIBUTIONPOINT);
		case CRYPTCERT_PROP_ISSUINGDIST_FULLNAME:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_ISSUINGDIST_FULLNAME);
		case CRYPTCERT_PROP_ISSUINGDIST_USERCERTSONLY:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_ISSUINGDIST_USERCERTSONLY);
		case CRYPTCERT_PROP_ISSUINGDIST_CACERTSONLY:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_ISSUINGDIST_CACERTSONLY);
		case CRYPTCERT_PROP_ISSUINGDIST_SOMEREASONSONLY:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_ISSUINGDIST_SOMEREASONSONLY);
		case CRYPTCERT_PROP_ISSUINGDIST_INDIRECTCRL:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_ISSUINGDIST_INDIRECTCRL);
		case CRYPTCERT_PROP_CERTIFICATEISSUER:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CERTIFICATEISSUER);
		case CRYPTCERT_PROP_NAMECONSTRAINTS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_NAMECONSTRAINTS);
		case CRYPTCERT_PROP_PERMITTEDSUBTREES:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_PERMITTEDSUBTREES);
		case CRYPTCERT_PROP_EXCLUDEDSUBTREES:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXCLUDEDSUBTREES);
		case CRYPTCERT_PROP_CRLDISTRIBUTIONPOINT:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CRLDISTRIBUTIONPOINT);
		case CRYPTCERT_PROP_CRLDIST_FULLNAME:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CRLDIST_FULLNAME);
		case CRYPTCERT_PROP_CRLDIST_REASONS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CRLDIST_REASONS);
		case CRYPTCERT_PROP_CRLDIST_CRLISSUER:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CRLDIST_CRLISSUER);
		case CRYPTCERT_PROP_CERTIFICATEPOLICIES:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CERTIFICATEPOLICIES);
		case CRYPTCERT_PROP_CERTPOLICYID:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CERTPOLICYID);
		case CRYPTCERT_PROP_CERTPOLICY_CPSURI:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CERTPOLICY_CPSURI);
		case CRYPTCERT_PROP_CERTPOLICY_ORGANIZATION:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CERTPOLICY_ORGANIZATION);
		case CRYPTCERT_PROP_CERTPOLICY_NOTICENUMBERS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CERTPOLICY_NOTICENUMBERS);
		case CRYPTCERT_PROP_CERTPOLICY_EXPLICITTEXT:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CERTPOLICY_EXPLICITTEXT);
		case CRYPTCERT_PROP_POLICYMAPPINGS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_POLICYMAPPINGS);
		case CRYPTCERT_PROP_ISSUERDOMAINPOLICY:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_ISSUERDOMAINPOLICY);
		case CRYPTCERT_PROP_SUBJECTDOMAINPOLICY:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTDOMAINPOLICY);
		case CRYPTCERT_PROP_AUTHORITYKEYIDENTIFIER:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_AUTHORITYKEYIDENTIFIER);
		case CRYPTCERT_PROP_AUTHORITY_KEYIDENTIFIER:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_AUTHORITY_KEYIDENTIFIER);
		case CRYPTCERT_PROP_AUTHORITY_CERTISSUER:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_AUTHORITY_CERTISSUER);
		case CRYPTCERT_PROP_AUTHORITY_CERTSERIALNUMBER:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_AUTHORITY_CERTSERIALNUMBER);
		case CRYPTCERT_PROP_POLICYCONSTRAINTS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_POLICYCONSTRAINTS);
		case CRYPTCERT_PROP_REQUIREEXPLICITPOLICY:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_REQUIREEXPLICITPOLICY);
		case CRYPTCERT_PROP_INHIBITPOLICYMAPPING:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_INHIBITPOLICYMAPPING);
		case CRYPTCERT_PROP_EXTKEYUSAGE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEYUSAGE);
		case CRYPTCERT_PROP_EXTKEY_MS_INDIVIDUALCODESIGNING:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_MS_INDIVIDUALCODESIGNING);
		case CRYPTCERT_PROP_EXTKEY_MS_COMMERCIALCODESIGNING:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_MS_COMMERCIALCODESIGNING);
		case CRYPTCERT_PROP_EXTKEY_MS_CERTTRUSTLISTSIGNING:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_MS_CERTTRUSTLISTSIGNING);
		case CRYPTCERT_PROP_EXTKEY_MS_TIMESTAMPSIGNING:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_MS_TIMESTAMPSIGNING);
		case CRYPTCERT_PROP_EXTKEY_MS_SERVERGATEDCRYPTO:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_MS_SERVERGATEDCRYPTO);
		case CRYPTCERT_PROP_EXTKEY_MS_ENCRYPTEDFILESYSTEM:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_MS_ENCRYPTEDFILESYSTEM);
		case CRYPTCERT_PROP_EXTKEY_SERVERAUTH:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_SERVERAUTH);
		case CRYPTCERT_PROP_EXTKEY_CLIENTAUTH:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_CLIENTAUTH);
		case CRYPTCERT_PROP_EXTKEY_CODESIGNING:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_CODESIGNING);
		case CRYPTCERT_PROP_EXTKEY_EMAILPROTECTION:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_EMAILPROTECTION);
		case CRYPTCERT_PROP_EXTKEY_IPSECENDSYSTEM:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_IPSECENDSYSTEM);
		case CRYPTCERT_PROP_EXTKEY_IPSECTUNNEL:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_IPSECTUNNEL);
		case CRYPTCERT_PROP_EXTKEY_IPSECUSER:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_IPSECUSER);
		case CRYPTCERT_PROP_EXTKEY_TIMESTAMPING:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_TIMESTAMPING);
		case CRYPTCERT_PROP_EXTKEY_OCSPSIGNING:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_OCSPSIGNING);
		case CRYPTCERT_PROP_EXTKEY_DIRECTORYSERVICE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_DIRECTORYSERVICE);
		case CRYPTCERT_PROP_EXTKEY_ANYKEYUSAGE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_ANYKEYUSAGE);
		case CRYPTCERT_PROP_EXTKEY_NS_SERVERGATEDCRYPTO:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_NS_SERVERGATEDCRYPTO);
		case CRYPTCERT_PROP_EXTKEY_VS_SERVERGATEDCRYPTO_CA:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_VS_SERVERGATEDCRYPTO_CA);
		case CRYPTCERT_PROP_CRLSTREAMIDENTIFIER:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CRLSTREAMIDENTIFIER);
		case CRYPTCERT_PROP_FRESHESTCRL:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_FRESHESTCRL);
		case CRYPTCERT_PROP_FRESHESTCRL_FULLNAME:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_FRESHESTCRL_FULLNAME);
		case CRYPTCERT_PROP_FRESHESTCRL_REASONS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_FRESHESTCRL_REASONS);
		case CRYPTCERT_PROP_FRESHESTCRL_CRLISSUER:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_FRESHESTCRL_CRLISSUER);
		case CRYPTCERT_PROP_ORDEREDLIST:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_ORDEREDLIST);
		case CRYPTCERT_PROP_BASEUPDATETIME:
			return js_cryptcert_attrtime_get(cx, vp, p->cert, CRYPT_CERTINFO_BASEUPDATETIME);
		case CRYPTCERT_PROP_DELTAINFO:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_DELTAINFO);
		case CRYPTCERT_PROP_DELTAINFO_LOCATION:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_DELTAINFO_LOCATION);
		case CRYPTCERT_PROP_DELTAINFO_NEXTDELTA:
			return js_cryptcert_attrtime_get(cx, vp, p->cert, CRYPT_CERTINFO_DELTAINFO_NEXTDELTA);
		case CRYPTCERT_PROP_INHIBITANYPOLICY:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_INHIBITANYPOLICY);
		case CRYPTCERT_PROP_TOBEREVOKED:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_TOBEREVOKED);
		case CRYPTCERT_PROP_TOBEREVOKED_CERTISSUER:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_TOBEREVOKED_CERTISSUER);
		case CRYPTCERT_PROP_TOBEREVOKED_REASONCODE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_TOBEREVOKED_REASONCODE);
		case CRYPTCERT_PROP_TOBEREVOKED_REVOCATIONTIME:
			return js_cryptcert_attrtime_get(cx, vp, p->cert, CRYPT_CERTINFO_TOBEREVOKED_REVOCATIONTIME);
		case CRYPTCERT_PROP_TOBEREVOKED_CERTSERIALNUMBER:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_TOBEREVOKED_CERTSERIALNUMBER);
		case CRYPTCERT_PROP_REVOKEDGROUPS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_REVOKEDGROUPS);
		case CRYPTCERT_PROP_REVOKEDGROUPS_CERTISSUER:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_REVOKEDGROUPS_CERTISSUER);
		case CRYPTCERT_PROP_REVOKEDGROUPS_REASONCODE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_REVOKEDGROUPS_REASONCODE);
		case CRYPTCERT_PROP_REVOKEDGROUPS_INVALIDITYDATE:
			return js_cryptcert_attrtime_get(cx, vp, p->cert, CRYPT_CERTINFO_REVOKEDGROUPS_INVALIDITYDATE);
		case CRYPTCERT_PROP_REVOKEDGROUPS_STARTINGNUMBER:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_REVOKEDGROUPS_STARTINGNUMBER);
		case CRYPTCERT_PROP_REVOKEDGROUPS_ENDINGNUMBER:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_REVOKEDGROUPS_ENDINGNUMBER);
		case CRYPTCERT_PROP_EXPIREDCERTSONCRL:
			return js_cryptcert_attrtime_get(cx, vp, p->cert, CRYPT_CERTINFO_EXPIREDCERTSONCRL);
		case CRYPTCERT_PROP_AAISSUINGDISTRIBUTIONPOINT:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_AAISSUINGDISTRIBUTIONPOINT);
		case CRYPTCERT_PROP_AAISSUINGDIST_FULLNAME:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_AAISSUINGDIST_FULLNAME);
		case CRYPTCERT_PROP_AAISSUINGDIST_SOMEREASONSONLY:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_AAISSUINGDIST_SOMEREASONSONLY);
		case CRYPTCERT_PROP_AAISSUINGDIST_INDIRECTCRL:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_AAISSUINGDIST_INDIRECTCRL);
		case CRYPTCERT_PROP_AAISSUINGDIST_USERATTRCERTS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_AAISSUINGDIST_USERATTRCERTS);
		case CRYPTCERT_PROP_AAISSUINGDIST_AACERTS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_AAISSUINGDIST_AACERTS);
		case CRYPTCERT_PROP_AAISSUINGDIST_SOACERTS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_AAISSUINGDIST_SOACERTS);
		case CRYPTCERT_PROP_NS_CERTTYPE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_NS_CERTTYPE);
		case CRYPTCERT_PROP_NS_BASEURL:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_NS_BASEURL);
		case CRYPTCERT_PROP_NS_REVOCATIONURL:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_NS_REVOCATIONURL);
		case CRYPTCERT_PROP_NS_CAREVOCATIONURL:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_NS_CAREVOCATIONURL);
		case CRYPTCERT_PROP_NS_CERTRENEWALURL:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_NS_CERTRENEWALURL);
		case CRYPTCERT_PROP_NS_CAPOLICYURL:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_NS_CAPOLICYURL);
		case CRYPTCERT_PROP_NS_SSLSERVERNAME:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_NS_SSLSERVERNAME);
		case CRYPTCERT_PROP_NS_COMMENT:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_NS_COMMENT);
		case CRYPTCERT_PROP_SET_HASHEDROOTKEY:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SET_HASHEDROOTKEY);
		case CRYPTCERT_PROP_SET_ROOTKEYTHUMBPRINT:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SET_ROOTKEYTHUMBPRINT);
		case CRYPTCERT_PROP_SET_CERTIFICATETYPE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SET_CERTIFICATETYPE);
		case CRYPTCERT_PROP_SET_MERCHANTDATA:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERCHANTDATA);
		case CRYPTCERT_PROP_SET_MERID:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERID);
		case CRYPTCERT_PROP_SET_MERACQUIRERBIN:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERACQUIRERBIN);
		case CRYPTCERT_PROP_SET_MERCHANTLANGUAGE:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERCHANTLANGUAGE);
		case CRYPTCERT_PROP_SET_MERCHANTNAME:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERCHANTNAME);
		case CRYPTCERT_PROP_SET_MERCHANTCITY:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERCHANTCITY);
		case CRYPTCERT_PROP_SET_MERCHANTSTATEPROVINCE:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERCHANTSTATEPROVINCE);
		case CRYPTCERT_PROP_SET_MERCHANTPOSTALCODE:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERCHANTPOSTALCODE);
		case CRYPTCERT_PROP_SET_MERCHANTCOUNTRYNAME:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERCHANTCOUNTRYNAME);
		case CRYPTCERT_PROP_SET_MERCOUNTRY:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERCOUNTRY);
		case CRYPTCERT_PROP_SET_MERAUTHFLAG:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERAUTHFLAG);
		case CRYPTCERT_PROP_SET_CERTCARDREQUIRED:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SET_CERTCARDREQUIRED);
		case CRYPTCERT_PROP_SET_TUNNELING:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SET_TUNNELING);
		case CRYPTCERT_PROP_SET_TUNNELINGFLAG:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_SET_TUNNELINGFLAG);
		case CRYPTCERT_PROP_SET_TUNNELINGALGID:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SET_TUNNELINGALGID);
		case CRYPTCERT_PROP_CMS_CONTENTTYPE:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_CONTENTTYPE);
		case CRYPTCERT_PROP_CMS_MESSAGEDIGEST:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_MESSAGEDIGEST);
		case CRYPTCERT_PROP_CMS_SIGNINGTIME:
			return js_cryptcert_attrtime_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGNINGTIME);
		case CRYPTCERT_PROP_CMS_COUNTERSIGNATURE:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_COUNTERSIGNATURE);
		case CRYPTCERT_PROP_CMS_SIGNINGDESCRIPTION:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGNINGDESCRIPTION);
		case CRYPTCERT_PROP_CMS_SMIMECAPABILITIES:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAPABILITIES);
		case CRYPTCERT_PROP_CMS_SMIMECAP_3DES:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_3DES);
		case CRYPTCERT_PROP_CMS_SMIMECAP_AES:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_AES);
		case CRYPTCERT_PROP_CMS_SMIMECAP_CAST128:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_CAST128);
		case CRYPTCERT_PROP_CMS_SMIMECAP_SHAng:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_SHAng);
		case CRYPTCERT_PROP_CMS_SMIMECAP_SHA2:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_SHA2);
		case CRYPTCERT_PROP_CMS_SMIMECAP_SHA1:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_SHA1);
		case CRYPTCERT_PROP_CMS_SMIMECAP_HMAC_SHAng:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHAng);
		case CRYPTCERT_PROP_CMS_SMIMECAP_HMAC_SHA2:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHA2);
		case CRYPTCERT_PROP_CMS_SMIMECAP_HMAC_SHA1:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHA1);
		case CRYPTCERT_PROP_CMS_SMIMECAP_AUTHENC256:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_AUTHENC256);
		case CRYPTCERT_PROP_CMS_SMIMECAP_AUTHENC128:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_AUTHENC128);
		case CRYPTCERT_PROP_CMS_SMIMECAP_RSA_SHAng:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHAng);
		case CRYPTCERT_PROP_CMS_SMIMECAP_RSA_SHA2:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHA2);
		case CRYPTCERT_PROP_CMS_SMIMECAP_RSA_SHA1:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHA1);
		case CRYPTCERT_PROP_CMS_SMIMECAP_DSA_SHA1:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_DSA_SHA1);
		case CRYPTCERT_PROP_CMS_SMIMECAP_ECDSA_SHAng:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHAng);
		case CRYPTCERT_PROP_CMS_SMIMECAP_ECDSA_SHA2:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHA2);
		case CRYPTCERT_PROP_CMS_SMIMECAP_ECDSA_SHA1:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHA1);
		case CRYPTCERT_PROP_CMS_SMIMECAP_PREFERSIGNEDDATA:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_PREFERSIGNEDDATA);
		case CRYPTCERT_PROP_CMS_SMIMECAP_CANNOTDECRYPTANY:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_CANNOTDECRYPTANY);
		case CRYPTCERT_PROP_CMS_SMIMECAP_PREFERBINARYINSIDE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_PREFERBINARYINSIDE);
		case CRYPTCERT_PROP_CMS_RECEIPTREQUEST:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_RECEIPTREQUEST);
		case CRYPTCERT_PROP_CMS_RECEIPT_CONTENTIDENTIFIER:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_RECEIPT_CONTENTIDENTIFIER);
		case CRYPTCERT_PROP_CMS_RECEIPT_FROM:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_RECEIPT_FROM);
		case CRYPTCERT_PROP_CMS_RECEIPT_TO:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_RECEIPT_TO);
		case CRYPTCERT_PROP_CMS_SECURITYLABEL:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SECURITYLABEL);
		case CRYPTCERT_PROP_CMS_SECLABEL_POLICY:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SECLABEL_POLICY);
		case CRYPTCERT_PROP_CMS_SECLABEL_CLASSIFICATION:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SECLABEL_CLASSIFICATION);
		case CRYPTCERT_PROP_CMS_SECLABEL_PRIVACYMARK:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SECLABEL_PRIVACYMARK);
		case CRYPTCERT_PROP_CMS_SECLABEL_CATTYPE:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SECLABEL_CATTYPE);
		case CRYPTCERT_PROP_CMS_SECLABEL_CATVALUE:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SECLABEL_CATVALUE);
		case CRYPTCERT_PROP_CMS_MLEXPANSIONHISTORY:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_MLEXPANSIONHISTORY);
		case CRYPTCERT_PROP_CMS_MLEXP_ENTITYIDENTIFIER:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_MLEXP_ENTITYIDENTIFIER);
		case CRYPTCERT_PROP_CMS_MLEXP_TIME:
			return js_cryptcert_attrtime_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_MLEXP_TIME);
		case CRYPTCERT_PROP_CMS_MLEXP_NONE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_MLEXP_NONE);
		case CRYPTCERT_PROP_CMS_MLEXP_INSTEADOF:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_MLEXP_INSTEADOF);
		case CRYPTCERT_PROP_CMS_MLEXP_INADDITIONTO:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_MLEXP_INADDITIONTO);
		case CRYPTCERT_PROP_CMS_CONTENTHINTS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_CONTENTHINTS);
		case CRYPTCERT_PROP_CMS_CONTENTHINT_DESCRIPTION:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_CONTENTHINT_DESCRIPTION);
		case CRYPTCERT_PROP_CMS_CONTENTHINT_TYPE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_CONTENTHINT_TYPE);
		case CRYPTCERT_PROP_CMS_EQUIVALENTLABEL:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_EQUIVALENTLABEL);
		case CRYPTCERT_PROP_CMS_EQVLABEL_POLICY:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_EQVLABEL_POLICY);
		case CRYPTCERT_PROP_CMS_EQVLABEL_CLASSIFICATION:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_EQVLABEL_CLASSIFICATION);
		case CRYPTCERT_PROP_CMS_EQVLABEL_PRIVACYMARK:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_EQVLABEL_PRIVACYMARK);
		case CRYPTCERT_PROP_CMS_EQVLABEL_CATTYPE:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_EQVLABEL_CATTYPE);
		case CRYPTCERT_PROP_CMS_EQVLABEL_CATVALUE:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_EQVLABEL_CATVALUE);
		case CRYPTCERT_PROP_CMS_SIGNINGCERTIFICATE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGNINGCERTIFICATE);
		case CRYPTCERT_PROP_CMS_SIGNINGCERT_ESSCERTID:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGNINGCERT_ESSCERTID);
		case CRYPTCERT_PROP_CMS_SIGNINGCERT_POLICIES:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGNINGCERT_POLICIES);
		case CRYPTCERT_PROP_CMS_SIGNINGCERTIFICATEV2:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGNINGCERTIFICATEV2);
		case CRYPTCERT_PROP_CMS_SIGNINGCERTV2_ESSCERTIDV2:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGNINGCERTV2_ESSCERTIDV2);
		case CRYPTCERT_PROP_CMS_SIGNINGCERTV2_POLICIES:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGNINGCERTV2_POLICIES);
		case CRYPTCERT_PROP_CMS_SIGNATUREPOLICYID:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGNATUREPOLICYID);
		case CRYPTCERT_PROP_CMS_SIGPOLICYID:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGPOLICYID);
		case CRYPTCERT_PROP_CMS_SIGPOLICYHASH:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGPOLICYHASH);
		case CRYPTCERT_PROP_CMS_SIGPOLICY_CPSURI:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGPOLICY_CPSURI);
		case CRYPTCERT_PROP_CMS_SIGPOLICY_ORGANIZATION:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGPOLICY_ORGANIZATION);
		case CRYPTCERT_PROP_CMS_SIGPOLICY_NOTICENUMBERS:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGPOLICY_NOTICENUMBERS);
		case CRYPTCERT_PROP_CMS_SIGPOLICY_EXPLICITTEXT:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGPOLICY_EXPLICITTEXT);
		case CRYPTCERT_PROP_CMS_SIGTYPEIDENTIFIER:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGTYPEIDENTIFIER);
		case CRYPTCERT_PROP_CMS_SIGTYPEID_ORIGINATORSIG:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGTYPEID_ORIGINATORSIG);
		case CRYPTCERT_PROP_CMS_SIGTYPEID_DOMAINSIG:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGTYPEID_DOMAINSIG);
		case CRYPTCERT_PROP_CMS_SIGTYPEID_ADDITIONALATTRIBUTES:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGTYPEID_ADDITIONALATTRIBUTES);
		case CRYPTCERT_PROP_CMS_SIGTYPEID_REVIEWSIG:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGTYPEID_REVIEWSIG);
		case CRYPTCERT_PROP_CMS_NONCE:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_NONCE);
		case CRYPTCERT_PROP_SCEP_MESSAGETYPE:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SCEP_MESSAGETYPE);
		case CRYPTCERT_PROP_SCEP_PKISTATUS:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SCEP_PKISTATUS);
		case CRYPTCERT_PROP_SCEP_FAILINFO:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SCEP_FAILINFO);
		case CRYPTCERT_PROP_SCEP_SENDERNONCE:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SCEP_SENDERNONCE);
		case CRYPTCERT_PROP_SCEP_RECIPIENTNONCE:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SCEP_RECIPIENTNONCE);
		case CRYPTCERT_PROP_SCEP_TRANSACTIONID:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_SCEP_TRANSACTIONID);
		case CRYPTCERT_PROP_CMS_SPCAGENCYINFO:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SPCAGENCYINFO);
		case CRYPTCERT_PROP_CMS_SPCAGENCYURL:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SPCAGENCYURL);
		case CRYPTCERT_PROP_CMS_SPCSTATEMENTTYPE:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SPCSTATEMENTTYPE);
		case CRYPTCERT_PROP_CMS_SPCSTMT_INDIVIDUALCODESIGNING:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SPCSTMT_INDIVIDUALCODESIGNING);
		case CRYPTCERT_PROP_CMS_SPCSTMT_COMMERCIALCODESIGNING:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SPCSTMT_COMMERCIALCODESIGNING);
		case CRYPTCERT_PROP_CMS_SPCOPUSINFO:
			return js_cryptcert_attr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SPCOPUSINFO);
		case CRYPTCERT_PROP_CMS_SPCOPUSINFO_NAME:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SPCOPUSINFO_NAME);
		case CRYPTCERT_PROP_CMS_SPCOPUSINFO_URL:
			return js_cryptcert_attrstr_get(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SPCOPUSINFO_URL);
	}
	return JS_TRUE;
}

static JSBool
js_cryptcert_attr_set(JSContext *cx, jsval *vp, CRYPT_CERTIFICATE cert, CRYPT_ATTRIBUTE_TYPE type)
{
	int status;
	int val;

	if (!JS_ValueToInt32(cx, *vp, &val))
		return JS_FALSE;
	status = cryptSetAttribute(cert, type, val);
	if (cryptStatusError(status)) {
		js_cryptcert_error(cx, cert, status);
		return JS_FALSE;
	}
	return JS_TRUE;
}

static JSBool
js_cryptcert_attrkey_set(JSContext *cx, jsval *vp, CRYPT_CERTIFICATE cert, CRYPT_ATTRIBUTE_TYPE type)
{
	int status;
	struct js_cryptcon_private_data* ctx;
	JSObject *key;

	key = JSVAL_TO_OBJECT(*vp);
	if (!JS_InstanceOf(cx, key, &js_cryptcon_class, NULL)) {
		JS_ReportError(cx, "Invalid CryptContext");
		return JS_FALSE;
	}

	if ((ctx=(struct js_cryptcon_private_data *)JS_GetPrivate(cx,key))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	status = cryptSetAttribute(cert, type, ctx->ctx);
	if (cryptStatusError(status)) {
		js_cryptcert_error(cx, cert, status);
		return JS_FALSE;
	}
	return JS_TRUE;
}

static JSBool
js_cryptcert_attrstr_set(JSContext *cx, jsval *vp, CRYPT_CERTIFICATE cert, CRYPT_ATTRIBUTE_TYPE type)
{
	int status;
	char *val = NULL;
	size_t len;

	JSVALUE_TO_MSTRING(cx, *vp, val, &len);
	HANDLE_PENDING(cx, val);
	if (val == NULL)
		return JS_FALSE;

	status = cryptSetAttributeString(cert, type, val, len);
	if (cryptStatusError(status)) {
		js_cryptcert_error(cx, cert, status);
		return JS_FALSE;
	}
	return JS_TRUE;
}

static JSBool
js_cryptcert_attrtime_set(JSContext *cx, jsval *vp, CRYPT_CERTIFICATE cert, CRYPT_ATTRIBUTE_TYPE type)
{
	int status;
	time_t t;
	jsdouble sec;

	if (JSVAL_IS_OBJECT(*vp)) {
		if (!JS_ObjectIsDate(cx, JSVAL_TO_OBJECT(*vp))) {
			JS_ReportError(cx, "Invalid Date");
			return JS_FALSE;
		}
		if (!JS_ValueToNumber(cx, *vp, &sec)) {
			JS_ReportError(cx, "Invalid Date");
			return JS_FALSE;
		}
		sec /= 1000;
	}
	else {
		if (JSVAL_IS_NUMBER(*vp)) {
			if (!JS_ValueToNumber(cx, *vp, &sec)) {
				JS_ReportError(cx, "Invalid Date");
				return JS_FALSE;
			}
		}
		else {
			JS_ReportError(cx, "Invalid Date");
			return JS_FALSE;
		}
	}
	t = (time_t)sec;

	status = cryptSetAttributeString(cert, type, &t, sizeof(t));
	if (cryptStatusError(status)) {
		js_cryptcert_error(cx, cert, status);
		return JS_FALSE;
	}
	return JS_TRUE;
}

static JSBool
js_cryptcert_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsval idval;
	jsint tiny;
	struct js_cryptcert_private_data* p;

	if ((p=(struct js_cryptcert_private_data *)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx, getprivate_failure, WHERE);
		return JS_FALSE;
	}

	JS_IdToValue(cx, id, &idval);
	tiny = JSVAL_TO_INT(idval);

	switch (tiny) {
		case CRYPTCERT_PROP_SELFSIGNED:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SELFSIGNED);
		case CRYPTCERT_PROP_IMMUTABLE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_IMMUTABLE);
		case CRYPTCERT_PROP_XYZZY:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_XYZZY);
		case CRYPTCERT_PROP_CERTTYPE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CERTTYPE);
		case CRYPTCERT_PROP_FINGERPRINT_SHA1:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_FINGERPRINT_SHA1);
		case CRYPTCERT_PROP_FINGERPRINT_SHA2:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_FINGERPRINT_SHA2);
		case CRYPTCERT_PROP_FINGERPRINT_SHAng:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_FINGERPRINT_SHAng);
		case CRYPTCERT_PROP_CURRENT_CERTIFICATE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CURRENT_CERTIFICATE);
		case CRYPTCERT_PROP_TRUSTED_USAGE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_TRUSTED_USAGE);
		case CRYPTCERT_PROP_TRUSTED_IMPLICIT:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_TRUSTED_IMPLICIT);
		case CRYPTCERT_PROP_SIGNATURELEVEL:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGNATURELEVEL);
		case CRYPTCERT_PROP_VERSION:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_VERSION);
		case CRYPTCERT_PROP_SERIALNUMBER:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SERIALNUMBER);
		case CRYPTCERT_PROP_SUBJECTPUBLICKEYINFO:
			return js_cryptcert_attrkey_set(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO);
		case CRYPTCERT_PROP_CERTIFICATE:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CERTIFICATE);
		case CRYPTCERT_PROP_CACERTIFICATE:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CACERTIFICATE);
		case CRYPTCERT_PROP_ISSUERNAME:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_ISSUERNAME);
		case CRYPTCERT_PROP_VALIDFROM:
			return js_cryptcert_attrtime_set(cx, vp, p->cert, CRYPT_CERTINFO_VALIDFROM);
		case CRYPTCERT_PROP_VALIDTO:
			return js_cryptcert_attrtime_set(cx, vp, p->cert, CRYPT_CERTINFO_VALIDTO);
		case CRYPTCERT_PROP_SUBJECTNAME:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTNAME);
		case CRYPTCERT_PROP_ISSUERUNIQUEID:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_ISSUERUNIQUEID);
		case CRYPTCERT_PROP_SUBJECTUNIQUEID:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTUNIQUEID);
		case CRYPTCERT_PROP_CERTREQUEST:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CERTREQUEST);
		case CRYPTCERT_PROP_THISUPDATE:
			return js_cryptcert_attrtime_set(cx, vp, p->cert, CRYPT_CERTINFO_THISUPDATE);
		case CRYPTCERT_PROP_NEXTUPDATE:
			return js_cryptcert_attrtime_set(cx, vp, p->cert, CRYPT_CERTINFO_NEXTUPDATE);
		case CRYPTCERT_PROP_REVOCATIONDATE:
			return js_cryptcert_attrtime_set(cx, vp, p->cert, CRYPT_CERTINFO_REVOCATIONDATE);
		case CRYPTCERT_PROP_REVOCATIONSTATUS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_REVOCATIONSTATUS);
		case CRYPTCERT_PROP_CERTSTATUS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CERTSTATUS);
		case CRYPTCERT_PROP_DN:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_DN);
		case CRYPTCERT_PROP_PKIUSER_ID:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_PKIUSER_ID);
		case CRYPTCERT_PROP_PKIUSER_ISSUEPASSWORD:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_PKIUSER_ISSUEPASSWORD);
		case CRYPTCERT_PROP_PKIUSER_REVPASSWORD:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_PKIUSER_REVPASSWORD);
		case CRYPTCERT_PROP_PKIUSER_RA:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_PKIUSER_RA);
		case CRYPTCERT_PROP_COUNTRYNAME:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_COUNTRYNAME);
		case CRYPTCERT_PROP_STATEORPROVINCENAME:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_STATEORPROVINCENAME);
		case CRYPTCERT_PROP_LOCALITYNAME:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_LOCALITYNAME);
		case CRYPTCERT_PROP_ORGANIZATIONNAME:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_ORGANIZATIONNAME);
		case CRYPTCERT_PROP_ORGANIZATIONALUNITNAME:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_ORGANIZATIONALUNITNAME);
		case CRYPTCERT_PROP_COMMONNAME:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_COMMONNAME);
		case CRYPTCERT_PROP_OTHERNAME_TYPEID:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_OTHERNAME_TYPEID);
		case CRYPTCERT_PROP_OTHERNAME_VALUE:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_OTHERNAME_VALUE);
		case CRYPTCERT_PROP_RFC822NAME:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_RFC822NAME);
		case CRYPTCERT_PROP_DNSNAME:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_DNSNAME);
		case CRYPTCERT_PROP_DIRECTORYNAME:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_DIRECTORYNAME);
		case CRYPTCERT_PROP_EDIPARTYNAME_NAMEASSIGNER:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_EDIPARTYNAME_NAMEASSIGNER);
		case CRYPTCERT_PROP_EDIPARTYNAME_PARTYNAME:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_EDIPARTYNAME_PARTYNAME);
		case CRYPTCERT_PROP_UNIFORMRESOURCEIDENTIFIER:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER);
		case CRYPTCERT_PROP_IPADDRESS:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_IPADDRESS);
		case CRYPTCERT_PROP_REGISTEREDID:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_REGISTEREDID);
		case CRYPTCERT_PROP_CHALLENGEPASSWORD:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CHALLENGEPASSWORD);
		case CRYPTCERT_PROP_CRLEXTREASON:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CRLEXTREASON);
		case CRYPTCERT_PROP_KEYFEATURES:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_KEYFEATURES);
		case CRYPTCERT_PROP_AUTHORITYINFOACCESS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_AUTHORITYINFOACCESS);
		case CRYPTCERT_PROP_AUTHORITYINFO_RTCS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_AUTHORITYINFO_RTCS);
		case CRYPTCERT_PROP_AUTHORITYINFO_OCSP:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_AUTHORITYINFO_OCSP);
		case CRYPTCERT_PROP_AUTHORITYINFO_CAISSUERS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_AUTHORITYINFO_CAISSUERS);
		case CRYPTCERT_PROP_AUTHORITYINFO_CERTSTORE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_AUTHORITYINFO_CERTSTORE);
		case CRYPTCERT_PROP_AUTHORITYINFO_CRLS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_AUTHORITYINFO_CRLS);
		case CRYPTCERT_PROP_BIOMETRICINFO:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_BIOMETRICINFO);
		case CRYPTCERT_PROP_BIOMETRICINFO_TYPE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_BIOMETRICINFO_TYPE);
		case CRYPTCERT_PROP_BIOMETRICINFO_HASHALGO:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_BIOMETRICINFO_HASHALGO);
		case CRYPTCERT_PROP_BIOMETRICINFO_HASH:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_BIOMETRICINFO_HASH);
		case CRYPTCERT_PROP_BIOMETRICINFO_URL:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_BIOMETRICINFO_URL);
		case CRYPTCERT_PROP_QCSTATEMENT:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_QCSTATEMENT);
		case CRYPTCERT_PROP_QCSTATEMENT_SEMANTICS:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_QCSTATEMENT_SEMANTICS);
		case CRYPTCERT_PROP_QCSTATEMENT_REGISTRATIONAUTHORITY:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_QCSTATEMENT_REGISTRATIONAUTHORITY);
		case CRYPTCERT_PROP_IPADDRESSBLOCKS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_IPADDRESSBLOCKS);
		case CRYPTCERT_PROP_IPADDRESSBLOCKS_ADDRESSFAMILY:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_IPADDRESSBLOCKS_ADDRESSFAMILY);
		case CRYPTCERT_PROP_IPADDRESSBLOCKS_PREFIX:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_IPADDRESSBLOCKS_PREFIX);
		case CRYPTCERT_PROP_IPADDRESSBLOCKS_MIN:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_IPADDRESSBLOCKS_MIN);
		case CRYPTCERT_PROP_IPADDRESSBLOCKS_MAX:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_IPADDRESSBLOCKS_MAX);
		case CRYPTCERT_PROP_AUTONOMOUSSYSIDS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_AUTONOMOUSSYSIDS);
		case CRYPTCERT_PROP_AUTONOMOUSSYSIDS_ASNUM_ID:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_ID);
		case CRYPTCERT_PROP_AUTONOMOUSSYSIDS_ASNUM_MIN:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_MIN);
		case CRYPTCERT_PROP_AUTONOMOUSSYSIDS_ASNUM_MAX:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_MAX);
		case CRYPTCERT_PROP_OCSP_NONCE:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_OCSP_NONCE);
		case CRYPTCERT_PROP_OCSP_RESPONSE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_OCSP_RESPONSE);
		case CRYPTCERT_PROP_OCSP_RESPONSE_OCSP:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_OCSP_RESPONSE_OCSP);
		case CRYPTCERT_PROP_OCSP_NOCHECK:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_OCSP_NOCHECK);
		case CRYPTCERT_PROP_OCSP_ARCHIVECUTOFF:
			return js_cryptcert_attrtime_set(cx, vp, p->cert, CRYPT_CERTINFO_OCSP_ARCHIVECUTOFF);
		case CRYPTCERT_PROP_SUBJECTINFOACCESS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTINFOACCESS);
		case CRYPTCERT_PROP_SUBJECTINFO_TIMESTAMPING:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTINFO_TIMESTAMPING);
		case CRYPTCERT_PROP_SUBJECTINFO_CAREPOSITORY:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTINFO_CAREPOSITORY);
		case CRYPTCERT_PROP_SUBJECTINFO_SIGNEDOBJECTREPOSITORY:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTINFO_SIGNEDOBJECTREPOSITORY);
		case CRYPTCERT_PROP_SUBJECTINFO_RPKIMANIFEST:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTINFO_RPKIMANIFEST);
		case CRYPTCERT_PROP_SUBJECTINFO_SIGNEDOBJECT:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTINFO_SIGNEDOBJECT);
		case CRYPTCERT_PROP_SIGG_DATEOFCERTGEN:
			return js_cryptcert_attrtime_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_DATEOFCERTGEN);
		case CRYPTCERT_PROP_SIGG_PROCURATION:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_PROCURATION);
		case CRYPTCERT_PROP_SIGG_PROCURE_COUNTRY:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_PROCURE_COUNTRY);
		case CRYPTCERT_PROP_SIGG_PROCURE_TYPEOFSUBSTITUTION:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_PROCURE_TYPEOFSUBSTITUTION);
		case CRYPTCERT_PROP_SIGG_PROCURE_SIGNINGFOR:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_PROCURE_SIGNINGFOR);
		case CRYPTCERT_PROP_SIGG_ADMISSIONS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_ADMISSIONS);
		case CRYPTCERT_PROP_SIGG_ADMISSIONS_AUTHORITY:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_ADMISSIONS_AUTHORITY);
		case CRYPTCERT_PROP_SIGG_ADMISSIONS_NAMINGAUTHID:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHID);
		case CRYPTCERT_PROP_SIGG_ADMISSIONS_NAMINGAUTHURL:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHURL);
		case CRYPTCERT_PROP_SIGG_ADMISSIONS_NAMINGAUTHTEXT:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHTEXT);
		case CRYPTCERT_PROP_SIGG_ADMISSIONS_PROFESSIONITEM:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_ADMISSIONS_PROFESSIONITEM);
		case CRYPTCERT_PROP_SIGG_ADMISSIONS_PROFESSIONOID:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_ADMISSIONS_PROFESSIONOID);
		case CRYPTCERT_PROP_SIGG_ADMISSIONS_REGISTRATIONNUMBER:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_ADMISSIONS_REGISTRATIONNUMBER);
		case CRYPTCERT_PROP_SIGG_MONETARYLIMIT:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_MONETARYLIMIT);
		case CRYPTCERT_PROP_SIGG_MONETARY_CURRENCY:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_MONETARY_CURRENCY);
		case CRYPTCERT_PROP_SIGG_MONETARY_AMOUNT:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_MONETARY_AMOUNT);
		case CRYPTCERT_PROP_SIGG_MONETARY_EXPONENT:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_MONETARY_EXPONENT);
		case CRYPTCERT_PROP_SIGG_DECLARATIONOFMAJORITY:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_DECLARATIONOFMAJORITY);
		case CRYPTCERT_PROP_SIGG_DECLARATIONOFMAJORITY_COUNTRY:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_DECLARATIONOFMAJORITY_COUNTRY);
		case CRYPTCERT_PROP_SIGG_RESTRICTION:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_RESTRICTION);
		case CRYPTCERT_PROP_SIGG_CERTHASH:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_CERTHASH);
		case CRYPTCERT_PROP_SIGG_ADDITIONALINFORMATION:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SIGG_ADDITIONALINFORMATION);
		case CRYPTCERT_PROP_STRONGEXTRANET:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_STRONGEXTRANET);
		case CRYPTCERT_PROP_STRONGEXTRANET_ZONE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_STRONGEXTRANET_ZONE);
		case CRYPTCERT_PROP_STRONGEXTRANET_ID:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_STRONGEXTRANET_ID);
		case CRYPTCERT_PROP_SUBJECTDIRECTORYATTRIBUTES:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTDIRECTORYATTRIBUTES);
		case CRYPTCERT_PROP_SUBJECTDIR_TYPE:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTDIR_TYPE);
		case CRYPTCERT_PROP_SUBJECTDIR_VALUES:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTDIR_VALUES);
		case CRYPTCERT_PROP_SUBJECTKEYIDENTIFIER:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER);
		case CRYPTCERT_PROP_KEYUSAGE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_KEYUSAGE);
		case CRYPTCERT_PROP_PRIVATEKEYUSAGEPERIOD:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_PRIVATEKEYUSAGEPERIOD);
		case CRYPTCERT_PROP_PRIVATEKEY_NOTBEFORE:
			return js_cryptcert_attrtime_set(cx, vp, p->cert, CRYPT_CERTINFO_PRIVATEKEY_NOTBEFORE);
		case CRYPTCERT_PROP_PRIVATEKEY_NOTAFTER:
			return js_cryptcert_attrtime_set(cx, vp, p->cert, CRYPT_CERTINFO_PRIVATEKEY_NOTAFTER);
		case CRYPTCERT_PROP_SUBJECTALTNAME:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTALTNAME);
		case CRYPTCERT_PROP_ISSUERALTNAME:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_ISSUERALTNAME);
		case CRYPTCERT_PROP_BASICCONSTRAINTS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_BASICCONSTRAINTS);
		case CRYPTCERT_PROP_CA:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CA);
		case CRYPTCERT_PROP_PATHLENCONSTRAINT:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_PATHLENCONSTRAINT);
		case CRYPTCERT_PROP_CRLNUMBER:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CRLNUMBER);
		case CRYPTCERT_PROP_CRLREASON:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CRLREASON);
		case CRYPTCERT_PROP_HOLDINSTRUCTIONCODE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_HOLDINSTRUCTIONCODE);
		case CRYPTCERT_PROP_INVALIDITYDATE:
			return js_cryptcert_attrtime_set(cx, vp, p->cert, CRYPT_CERTINFO_INVALIDITYDATE);
		case CRYPTCERT_PROP_DELTACRLINDICATOR:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_DELTACRLINDICATOR);
		case CRYPTCERT_PROP_ISSUINGDISTRIBUTIONPOINT:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_ISSUINGDISTRIBUTIONPOINT);
		case CRYPTCERT_PROP_ISSUINGDIST_FULLNAME:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_ISSUINGDIST_FULLNAME);
		case CRYPTCERT_PROP_ISSUINGDIST_USERCERTSONLY:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_ISSUINGDIST_USERCERTSONLY);
		case CRYPTCERT_PROP_ISSUINGDIST_CACERTSONLY:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_ISSUINGDIST_CACERTSONLY);
		case CRYPTCERT_PROP_ISSUINGDIST_SOMEREASONSONLY:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_ISSUINGDIST_SOMEREASONSONLY);
		case CRYPTCERT_PROP_ISSUINGDIST_INDIRECTCRL:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_ISSUINGDIST_INDIRECTCRL);
		case CRYPTCERT_PROP_CERTIFICATEISSUER:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CERTIFICATEISSUER);
		case CRYPTCERT_PROP_NAMECONSTRAINTS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_NAMECONSTRAINTS);
		case CRYPTCERT_PROP_PERMITTEDSUBTREES:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_PERMITTEDSUBTREES);
		case CRYPTCERT_PROP_EXCLUDEDSUBTREES:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXCLUDEDSUBTREES);
		case CRYPTCERT_PROP_CRLDISTRIBUTIONPOINT:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CRLDISTRIBUTIONPOINT);
		case CRYPTCERT_PROP_CRLDIST_FULLNAME:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CRLDIST_FULLNAME);
		case CRYPTCERT_PROP_CRLDIST_REASONS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CRLDIST_REASONS);
		case CRYPTCERT_PROP_CRLDIST_CRLISSUER:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CRLDIST_CRLISSUER);
		case CRYPTCERT_PROP_CERTIFICATEPOLICIES:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CERTIFICATEPOLICIES);
		case CRYPTCERT_PROP_CERTPOLICYID:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CERTPOLICYID);
		case CRYPTCERT_PROP_CERTPOLICY_CPSURI:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CERTPOLICY_CPSURI);
		case CRYPTCERT_PROP_CERTPOLICY_ORGANIZATION:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CERTPOLICY_ORGANIZATION);
		case CRYPTCERT_PROP_CERTPOLICY_NOTICENUMBERS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CERTPOLICY_NOTICENUMBERS);
		case CRYPTCERT_PROP_CERTPOLICY_EXPLICITTEXT:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CERTPOLICY_EXPLICITTEXT);
		case CRYPTCERT_PROP_POLICYMAPPINGS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_POLICYMAPPINGS);
		case CRYPTCERT_PROP_ISSUERDOMAINPOLICY:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_ISSUERDOMAINPOLICY);
		case CRYPTCERT_PROP_SUBJECTDOMAINPOLICY:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SUBJECTDOMAINPOLICY);
		case CRYPTCERT_PROP_AUTHORITYKEYIDENTIFIER:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_AUTHORITYKEYIDENTIFIER);
		case CRYPTCERT_PROP_AUTHORITY_KEYIDENTIFIER:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_AUTHORITY_KEYIDENTIFIER);
		case CRYPTCERT_PROP_AUTHORITY_CERTISSUER:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_AUTHORITY_CERTISSUER);
		case CRYPTCERT_PROP_AUTHORITY_CERTSERIALNUMBER:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_AUTHORITY_CERTSERIALNUMBER);
		case CRYPTCERT_PROP_POLICYCONSTRAINTS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_POLICYCONSTRAINTS);
		case CRYPTCERT_PROP_REQUIREEXPLICITPOLICY:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_REQUIREEXPLICITPOLICY);
		case CRYPTCERT_PROP_INHIBITPOLICYMAPPING:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_INHIBITPOLICYMAPPING);
		case CRYPTCERT_PROP_EXTKEYUSAGE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEYUSAGE);
		case CRYPTCERT_PROP_EXTKEY_MS_INDIVIDUALCODESIGNING:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_MS_INDIVIDUALCODESIGNING);
		case CRYPTCERT_PROP_EXTKEY_MS_COMMERCIALCODESIGNING:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_MS_COMMERCIALCODESIGNING);
		case CRYPTCERT_PROP_EXTKEY_MS_CERTTRUSTLISTSIGNING:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_MS_CERTTRUSTLISTSIGNING);
		case CRYPTCERT_PROP_EXTKEY_MS_TIMESTAMPSIGNING:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_MS_TIMESTAMPSIGNING);
		case CRYPTCERT_PROP_EXTKEY_MS_SERVERGATEDCRYPTO:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_MS_SERVERGATEDCRYPTO);
		case CRYPTCERT_PROP_EXTKEY_MS_ENCRYPTEDFILESYSTEM:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_MS_ENCRYPTEDFILESYSTEM);
		case CRYPTCERT_PROP_EXTKEY_SERVERAUTH:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_SERVERAUTH);
		case CRYPTCERT_PROP_EXTKEY_CLIENTAUTH:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_CLIENTAUTH);
		case CRYPTCERT_PROP_EXTKEY_CODESIGNING:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_CODESIGNING);
		case CRYPTCERT_PROP_EXTKEY_EMAILPROTECTION:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_EMAILPROTECTION);
		case CRYPTCERT_PROP_EXTKEY_IPSECENDSYSTEM:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_IPSECENDSYSTEM);
		case CRYPTCERT_PROP_EXTKEY_IPSECTUNNEL:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_IPSECTUNNEL);
		case CRYPTCERT_PROP_EXTKEY_IPSECUSER:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_IPSECUSER);
		case CRYPTCERT_PROP_EXTKEY_TIMESTAMPING:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_TIMESTAMPING);
		case CRYPTCERT_PROP_EXTKEY_OCSPSIGNING:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_OCSPSIGNING);
		case CRYPTCERT_PROP_EXTKEY_DIRECTORYSERVICE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_DIRECTORYSERVICE);
		case CRYPTCERT_PROP_EXTKEY_ANYKEYUSAGE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_ANYKEYUSAGE);
		case CRYPTCERT_PROP_EXTKEY_NS_SERVERGATEDCRYPTO:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_NS_SERVERGATEDCRYPTO);
		case CRYPTCERT_PROP_EXTKEY_VS_SERVERGATEDCRYPTO_CA:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_EXTKEY_VS_SERVERGATEDCRYPTO_CA);
		case CRYPTCERT_PROP_CRLSTREAMIDENTIFIER:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CRLSTREAMIDENTIFIER);
		case CRYPTCERT_PROP_FRESHESTCRL:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_FRESHESTCRL);
		case CRYPTCERT_PROP_FRESHESTCRL_FULLNAME:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_FRESHESTCRL_FULLNAME);
		case CRYPTCERT_PROP_FRESHESTCRL_REASONS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_FRESHESTCRL_REASONS);
		case CRYPTCERT_PROP_FRESHESTCRL_CRLISSUER:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_FRESHESTCRL_CRLISSUER);
		case CRYPTCERT_PROP_ORDEREDLIST:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_ORDEREDLIST);
		case CRYPTCERT_PROP_BASEUPDATETIME:
			return js_cryptcert_attrtime_set(cx, vp, p->cert, CRYPT_CERTINFO_BASEUPDATETIME);
		case CRYPTCERT_PROP_DELTAINFO:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_DELTAINFO);
		case CRYPTCERT_PROP_DELTAINFO_LOCATION:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_DELTAINFO_LOCATION);
		case CRYPTCERT_PROP_DELTAINFO_NEXTDELTA:
			return js_cryptcert_attrtime_set(cx, vp, p->cert, CRYPT_CERTINFO_DELTAINFO_NEXTDELTA);
		case CRYPTCERT_PROP_INHIBITANYPOLICY:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_INHIBITANYPOLICY);
		case CRYPTCERT_PROP_TOBEREVOKED:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_TOBEREVOKED);
		case CRYPTCERT_PROP_TOBEREVOKED_CERTISSUER:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_TOBEREVOKED_CERTISSUER);
		case CRYPTCERT_PROP_TOBEREVOKED_REASONCODE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_TOBEREVOKED_REASONCODE);
		case CRYPTCERT_PROP_TOBEREVOKED_REVOCATIONTIME:
			return js_cryptcert_attrtime_set(cx, vp, p->cert, CRYPT_CERTINFO_TOBEREVOKED_REVOCATIONTIME);
		case CRYPTCERT_PROP_TOBEREVOKED_CERTSERIALNUMBER:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_TOBEREVOKED_CERTSERIALNUMBER);
		case CRYPTCERT_PROP_REVOKEDGROUPS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_REVOKEDGROUPS);
		case CRYPTCERT_PROP_REVOKEDGROUPS_CERTISSUER:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_REVOKEDGROUPS_CERTISSUER);
		case CRYPTCERT_PROP_REVOKEDGROUPS_REASONCODE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_REVOKEDGROUPS_REASONCODE);
		case CRYPTCERT_PROP_REVOKEDGROUPS_INVALIDITYDATE:
			return js_cryptcert_attrtime_set(cx, vp, p->cert, CRYPT_CERTINFO_REVOKEDGROUPS_INVALIDITYDATE);
		case CRYPTCERT_PROP_REVOKEDGROUPS_STARTINGNUMBER:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_REVOKEDGROUPS_STARTINGNUMBER);
		case CRYPTCERT_PROP_REVOKEDGROUPS_ENDINGNUMBER:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_REVOKEDGROUPS_ENDINGNUMBER);
		case CRYPTCERT_PROP_EXPIREDCERTSONCRL:
			return js_cryptcert_attrtime_set(cx, vp, p->cert, CRYPT_CERTINFO_EXPIREDCERTSONCRL);
		case CRYPTCERT_PROP_AAISSUINGDISTRIBUTIONPOINT:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_AAISSUINGDISTRIBUTIONPOINT);
		case CRYPTCERT_PROP_AAISSUINGDIST_FULLNAME:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_AAISSUINGDIST_FULLNAME);
		case CRYPTCERT_PROP_AAISSUINGDIST_SOMEREASONSONLY:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_AAISSUINGDIST_SOMEREASONSONLY);
		case CRYPTCERT_PROP_AAISSUINGDIST_INDIRECTCRL:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_AAISSUINGDIST_INDIRECTCRL);
		case CRYPTCERT_PROP_AAISSUINGDIST_USERATTRCERTS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_AAISSUINGDIST_USERATTRCERTS);
		case CRYPTCERT_PROP_AAISSUINGDIST_AACERTS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_AAISSUINGDIST_AACERTS);
		case CRYPTCERT_PROP_AAISSUINGDIST_SOACERTS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_AAISSUINGDIST_SOACERTS);
		case CRYPTCERT_PROP_NS_CERTTYPE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_NS_CERTTYPE);
		case CRYPTCERT_PROP_NS_BASEURL:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_NS_BASEURL);
		case CRYPTCERT_PROP_NS_REVOCATIONURL:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_NS_REVOCATIONURL);
		case CRYPTCERT_PROP_NS_CAREVOCATIONURL:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_NS_CAREVOCATIONURL);
		case CRYPTCERT_PROP_NS_CERTRENEWALURL:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_NS_CERTRENEWALURL);
		case CRYPTCERT_PROP_NS_CAPOLICYURL:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_NS_CAPOLICYURL);
		case CRYPTCERT_PROP_NS_SSLSERVERNAME:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_NS_SSLSERVERNAME);
		case CRYPTCERT_PROP_NS_COMMENT:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_NS_COMMENT);
		case CRYPTCERT_PROP_SET_HASHEDROOTKEY:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SET_HASHEDROOTKEY);
		case CRYPTCERT_PROP_SET_ROOTKEYTHUMBPRINT:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SET_ROOTKEYTHUMBPRINT);
		case CRYPTCERT_PROP_SET_CERTIFICATETYPE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SET_CERTIFICATETYPE);
		case CRYPTCERT_PROP_SET_MERCHANTDATA:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERCHANTDATA);
		case CRYPTCERT_PROP_SET_MERID:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERID);
		case CRYPTCERT_PROP_SET_MERACQUIRERBIN:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERACQUIRERBIN);
		case CRYPTCERT_PROP_SET_MERCHANTLANGUAGE:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERCHANTLANGUAGE);
		case CRYPTCERT_PROP_SET_MERCHANTNAME:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERCHANTNAME);
		case CRYPTCERT_PROP_SET_MERCHANTCITY:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERCHANTCITY);
		case CRYPTCERT_PROP_SET_MERCHANTSTATEPROVINCE:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERCHANTSTATEPROVINCE);
		case CRYPTCERT_PROP_SET_MERCHANTPOSTALCODE:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERCHANTPOSTALCODE);
		case CRYPTCERT_PROP_SET_MERCHANTCOUNTRYNAME:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERCHANTCOUNTRYNAME);
		case CRYPTCERT_PROP_SET_MERCOUNTRY:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERCOUNTRY);
		case CRYPTCERT_PROP_SET_MERAUTHFLAG:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SET_MERAUTHFLAG);
		case CRYPTCERT_PROP_SET_CERTCARDREQUIRED:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SET_CERTCARDREQUIRED);
		case CRYPTCERT_PROP_SET_TUNNELING:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SET_TUNNELING);
		case CRYPTCERT_PROP_SET_TUNNELINGFLAG:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_SET_TUNNELINGFLAG);
		case CRYPTCERT_PROP_SET_TUNNELINGALGID:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SET_TUNNELINGALGID);
		case CRYPTCERT_PROP_CMS_CONTENTTYPE:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_CONTENTTYPE);
		case CRYPTCERT_PROP_CMS_MESSAGEDIGEST:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_MESSAGEDIGEST);
		case CRYPTCERT_PROP_CMS_SIGNINGTIME:
			return js_cryptcert_attrtime_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGNINGTIME);
		case CRYPTCERT_PROP_CMS_COUNTERSIGNATURE:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_COUNTERSIGNATURE);
		case CRYPTCERT_PROP_CMS_SIGNINGDESCRIPTION:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGNINGDESCRIPTION);
		case CRYPTCERT_PROP_CMS_SMIMECAPABILITIES:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAPABILITIES);
		case CRYPTCERT_PROP_CMS_SMIMECAP_3DES:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_3DES);
		case CRYPTCERT_PROP_CMS_SMIMECAP_AES:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_AES);
		case CRYPTCERT_PROP_CMS_SMIMECAP_CAST128:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_CAST128);
		case CRYPTCERT_PROP_CMS_SMIMECAP_SHAng:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_SHAng);
		case CRYPTCERT_PROP_CMS_SMIMECAP_SHA2:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_SHA2);
		case CRYPTCERT_PROP_CMS_SMIMECAP_SHA1:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_SHA1);
		case CRYPTCERT_PROP_CMS_SMIMECAP_HMAC_SHAng:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHAng);
		case CRYPTCERT_PROP_CMS_SMIMECAP_HMAC_SHA2:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHA2);
		case CRYPTCERT_PROP_CMS_SMIMECAP_HMAC_SHA1:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHA1);
		case CRYPTCERT_PROP_CMS_SMIMECAP_AUTHENC256:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_AUTHENC256);
		case CRYPTCERT_PROP_CMS_SMIMECAP_AUTHENC128:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_AUTHENC128);
		case CRYPTCERT_PROP_CMS_SMIMECAP_RSA_SHAng:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHAng);
		case CRYPTCERT_PROP_CMS_SMIMECAP_RSA_SHA2:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHA2);
		case CRYPTCERT_PROP_CMS_SMIMECAP_RSA_SHA1:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHA1);
		case CRYPTCERT_PROP_CMS_SMIMECAP_DSA_SHA1:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_DSA_SHA1);
		case CRYPTCERT_PROP_CMS_SMIMECAP_ECDSA_SHAng:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHAng);
		case CRYPTCERT_PROP_CMS_SMIMECAP_ECDSA_SHA2:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHA2);
		case CRYPTCERT_PROP_CMS_SMIMECAP_ECDSA_SHA1:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHA1);
		case CRYPTCERT_PROP_CMS_SMIMECAP_PREFERSIGNEDDATA:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_PREFERSIGNEDDATA);
		case CRYPTCERT_PROP_CMS_SMIMECAP_CANNOTDECRYPTANY:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_CANNOTDECRYPTANY);
		case CRYPTCERT_PROP_CMS_SMIMECAP_PREFERBINARYINSIDE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SMIMECAP_PREFERBINARYINSIDE);
		case CRYPTCERT_PROP_CMS_RECEIPTREQUEST:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_RECEIPTREQUEST);
		case CRYPTCERT_PROP_CMS_RECEIPT_CONTENTIDENTIFIER:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_RECEIPT_CONTENTIDENTIFIER);
		case CRYPTCERT_PROP_CMS_RECEIPT_FROM:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_RECEIPT_FROM);
		case CRYPTCERT_PROP_CMS_RECEIPT_TO:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_RECEIPT_TO);
		case CRYPTCERT_PROP_CMS_SECURITYLABEL:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SECURITYLABEL);
		case CRYPTCERT_PROP_CMS_SECLABEL_POLICY:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SECLABEL_POLICY);
		case CRYPTCERT_PROP_CMS_SECLABEL_CLASSIFICATION:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SECLABEL_CLASSIFICATION);
		case CRYPTCERT_PROP_CMS_SECLABEL_PRIVACYMARK:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SECLABEL_PRIVACYMARK);
		case CRYPTCERT_PROP_CMS_SECLABEL_CATTYPE:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SECLABEL_CATTYPE);
		case CRYPTCERT_PROP_CMS_SECLABEL_CATVALUE:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SECLABEL_CATVALUE);
		case CRYPTCERT_PROP_CMS_MLEXPANSIONHISTORY:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_MLEXPANSIONHISTORY);
		case CRYPTCERT_PROP_CMS_MLEXP_ENTITYIDENTIFIER:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_MLEXP_ENTITYIDENTIFIER);
		case CRYPTCERT_PROP_CMS_MLEXP_TIME:
			return js_cryptcert_attrtime_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_MLEXP_TIME);
		case CRYPTCERT_PROP_CMS_MLEXP_NONE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_MLEXP_NONE);
		case CRYPTCERT_PROP_CMS_MLEXP_INSTEADOF:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_MLEXP_INSTEADOF);
		case CRYPTCERT_PROP_CMS_MLEXP_INADDITIONTO:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_MLEXP_INADDITIONTO);
		case CRYPTCERT_PROP_CMS_CONTENTHINTS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_CONTENTHINTS);
		case CRYPTCERT_PROP_CMS_CONTENTHINT_DESCRIPTION:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_CONTENTHINT_DESCRIPTION);
		case CRYPTCERT_PROP_CMS_CONTENTHINT_TYPE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_CONTENTHINT_TYPE);
		case CRYPTCERT_PROP_CMS_EQUIVALENTLABEL:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_EQUIVALENTLABEL);
		case CRYPTCERT_PROP_CMS_EQVLABEL_POLICY:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_EQVLABEL_POLICY);
		case CRYPTCERT_PROP_CMS_EQVLABEL_CLASSIFICATION:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_EQVLABEL_CLASSIFICATION);
		case CRYPTCERT_PROP_CMS_EQVLABEL_PRIVACYMARK:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_EQVLABEL_PRIVACYMARK);
		case CRYPTCERT_PROP_CMS_EQVLABEL_CATTYPE:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_EQVLABEL_CATTYPE);
		case CRYPTCERT_PROP_CMS_EQVLABEL_CATVALUE:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_EQVLABEL_CATVALUE);
		case CRYPTCERT_PROP_CMS_SIGNINGCERTIFICATE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGNINGCERTIFICATE);
		case CRYPTCERT_PROP_CMS_SIGNINGCERT_ESSCERTID:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGNINGCERT_ESSCERTID);
		case CRYPTCERT_PROP_CMS_SIGNINGCERT_POLICIES:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGNINGCERT_POLICIES);
		case CRYPTCERT_PROP_CMS_SIGNINGCERTIFICATEV2:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGNINGCERTIFICATEV2);
		case CRYPTCERT_PROP_CMS_SIGNINGCERTV2_ESSCERTIDV2:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGNINGCERTV2_ESSCERTIDV2);
		case CRYPTCERT_PROP_CMS_SIGNINGCERTV2_POLICIES:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGNINGCERTV2_POLICIES);
		case CRYPTCERT_PROP_CMS_SIGNATUREPOLICYID:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGNATUREPOLICYID);
		case CRYPTCERT_PROP_CMS_SIGPOLICYID:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGPOLICYID);
		case CRYPTCERT_PROP_CMS_SIGPOLICYHASH:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGPOLICYHASH);
		case CRYPTCERT_PROP_CMS_SIGPOLICY_CPSURI:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGPOLICY_CPSURI);
		case CRYPTCERT_PROP_CMS_SIGPOLICY_ORGANIZATION:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGPOLICY_ORGANIZATION);
		case CRYPTCERT_PROP_CMS_SIGPOLICY_NOTICENUMBERS:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGPOLICY_NOTICENUMBERS);
		case CRYPTCERT_PROP_CMS_SIGPOLICY_EXPLICITTEXT:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGPOLICY_EXPLICITTEXT);
		case CRYPTCERT_PROP_CMS_SIGTYPEIDENTIFIER:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGTYPEIDENTIFIER);
		case CRYPTCERT_PROP_CMS_SIGTYPEID_ORIGINATORSIG:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGTYPEID_ORIGINATORSIG);
		case CRYPTCERT_PROP_CMS_SIGTYPEID_DOMAINSIG:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGTYPEID_DOMAINSIG);
		case CRYPTCERT_PROP_CMS_SIGTYPEID_ADDITIONALATTRIBUTES:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGTYPEID_ADDITIONALATTRIBUTES);
		case CRYPTCERT_PROP_CMS_SIGTYPEID_REVIEWSIG:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SIGTYPEID_REVIEWSIG);
		case CRYPTCERT_PROP_CMS_NONCE:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_NONCE);
		case CRYPTCERT_PROP_SCEP_MESSAGETYPE:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SCEP_MESSAGETYPE);
		case CRYPTCERT_PROP_SCEP_PKISTATUS:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SCEP_PKISTATUS);
		case CRYPTCERT_PROP_SCEP_FAILINFO:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SCEP_FAILINFO);
		case CRYPTCERT_PROP_SCEP_SENDERNONCE:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SCEP_SENDERNONCE);
		case CRYPTCERT_PROP_SCEP_RECIPIENTNONCE:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SCEP_RECIPIENTNONCE);
		case CRYPTCERT_PROP_SCEP_TRANSACTIONID:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_SCEP_TRANSACTIONID);
		case CRYPTCERT_PROP_CMS_SPCAGENCYINFO:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SPCAGENCYINFO);
		case CRYPTCERT_PROP_CMS_SPCAGENCYURL:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SPCAGENCYURL);
		case CRYPTCERT_PROP_CMS_SPCSTATEMENTTYPE:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SPCSTATEMENTTYPE);
		case CRYPTCERT_PROP_CMS_SPCSTMT_INDIVIDUALCODESIGNING:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SPCSTMT_INDIVIDUALCODESIGNING);
		case CRYPTCERT_PROP_CMS_SPCSTMT_COMMERCIALCODESIGNING:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SPCSTMT_COMMERCIALCODESIGNING);
		case CRYPTCERT_PROP_CMS_SPCOPUSINFO:
			return js_cryptcert_attr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SPCOPUSINFO);
		case CRYPTCERT_PROP_CMS_SPCOPUSINFO_NAME:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SPCOPUSINFO_NAME);
		case CRYPTCERT_PROP_CMS_SPCOPUSINFO_URL:
			return js_cryptcert_attrstr_set(cx, vp, p->cert, CRYPT_CERTINFO_CMS_SPCOPUSINFO_URL);
	}
	return JS_TRUE;
}

#ifdef BUILD_JSDOCS
static char* cryptcert_prop_desc[] = {
	 "Keyopt constant (CryptCert.KEYOPT.XXX):<ul class=\"showList\">\n"
	 "<li>CryptCert.KEYOPT.NONE</li>\n"
	 "<li>CryptCert.KEYOPT.READONLY</li>\n"
	 "<li>CryptCert.KEYOPT.CREATE</li>\n"
	,NULL
};
#endif

static jsSyncPropertySpec js_cryptcert_properties[] = {
/*	name						,tinyid									,flags,				ver	*/
	{"selfsigned"				,CRYPTCERT_PROP_SELFSIGNED	,JSPROP_ENUMERATE	,316},
	{"immutable"				,CRYPTCERT_PROP_IMMUTABLE	,JSPROP_ENUMERATE	,316},
	{"xyzzy"					,CRYPTCERT_PROP_XYZZY	,JSPROP_ENUMERATE	,316},
	{"certtype"					,CRYPTCERT_PROP_CERTTYPE	,JSPROP_ENUMERATE	,316},
	{"fingerprint_sha1"			,CRYPTCERT_PROP_FINGERPRINT_SHA1	,JSPROP_ENUMERATE	,316},
	{"fingerprint_sha2"			,CRYPTCERT_PROP_FINGERPRINT_SHA2	,JSPROP_ENUMERATE	,316},
	{"fingerprint_shang"		,CRYPTCERT_PROP_FINGERPRINT_SHAng	,JSPROP_ENUMERATE	,316},
	{"current_certificate"		,CRYPTCERT_PROP_CURRENT_CERTIFICATE	,JSPROP_ENUMERATE	,316},
	{"trusted_usage"			,CRYPTCERT_PROP_TRUSTED_USAGE	,JSPROP_ENUMERATE	,316},
	{"trusted_implicit"			,CRYPTCERT_PROP_TRUSTED_IMPLICIT	,JSPROP_ENUMERATE	,316},
	{"signaturelevel"			,CRYPTCERT_PROP_SIGNATURELEVEL	,JSPROP_ENUMERATE	,316},
	{"version"					,CRYPTCERT_PROP_VERSION	,JSPROP_ENUMERATE	,316},
	{"serialnumber"				,CRYPTCERT_PROP_SERIALNUMBER	,JSPROP_ENUMERATE	,316},
	{"subjectpublickeyinfo"		,CRYPTCERT_PROP_SUBJECTPUBLICKEYINFO	,JSPROP_ENUMERATE	,316},
	{"certificate"				,CRYPTCERT_PROP_CERTIFICATE	,JSPROP_ENUMERATE	,316},
	{"cacertificate"			,CRYPTCERT_PROP_CACERTIFICATE	,JSPROP_ENUMERATE	,316},
	{"issuername"				,CRYPTCERT_PROP_ISSUERNAME	,JSPROP_ENUMERATE	,316},
	{"validfrom"				,CRYPTCERT_PROP_VALIDFROM	,JSPROP_ENUMERATE	,316},
	{"validto"					,CRYPTCERT_PROP_VALIDTO	,JSPROP_ENUMERATE	,316},
	{"subjectname"				,CRYPTCERT_PROP_SUBJECTNAME	,JSPROP_ENUMERATE	,316},
	{"issueruniqueid"			,CRYPTCERT_PROP_ISSUERUNIQUEID	,JSPROP_ENUMERATE	,316},
	{"subjectuniqueid"			,CRYPTCERT_PROP_SUBJECTUNIQUEID	,JSPROP_ENUMERATE	,316},
	{"certrequest"				,CRYPTCERT_PROP_CERTREQUEST	,JSPROP_ENUMERATE	,316},
	{"thisupdate"				,CRYPTCERT_PROP_THISUPDATE	,JSPROP_ENUMERATE	,316},
	{"nextupdate"				,CRYPTCERT_PROP_NEXTUPDATE	,JSPROP_ENUMERATE	,316},
	{"revocationdate"			,CRYPTCERT_PROP_REVOCATIONDATE	,JSPROP_ENUMERATE	,316},
	{"revocationstatus"			,CRYPTCERT_PROP_REVOCATIONSTATUS	,JSPROP_ENUMERATE	,316},
	{"certstatus"				,CRYPTCERT_PROP_CERTSTATUS	,JSPROP_ENUMERATE	,316},
	{"dn"						,CRYPTCERT_PROP_DN	,JSPROP_ENUMERATE	,316},
	{"pkiuser_id"				,CRYPTCERT_PROP_PKIUSER_ID	,JSPROP_ENUMERATE	,316},
	{"pkiuser_issuepassword"	,CRYPTCERT_PROP_PKIUSER_ISSUEPASSWORD	,JSPROP_ENUMERATE	,316},
	{"pkiuser_revpassword"		,CRYPTCERT_PROP_PKIUSER_REVPASSWORD	,JSPROP_ENUMERATE	,316},
	{"pkiuser_ra"				,CRYPTCERT_PROP_PKIUSER_RA	,JSPROP_ENUMERATE	,316},
	{"countryname"				,CRYPTCERT_PROP_COUNTRYNAME	,JSPROP_ENUMERATE	,316},
	{"stateorprovincename"		,CRYPTCERT_PROP_STATEORPROVINCENAME	,JSPROP_ENUMERATE	,316},
	{"localityname"				,CRYPTCERT_PROP_LOCALITYNAME	,JSPROP_ENUMERATE	,316},
	{"organizationname"			,CRYPTCERT_PROP_ORGANIZATIONNAME	,JSPROP_ENUMERATE	,316},
	{"organizationalunitname"	,CRYPTCERT_PROP_ORGANIZATIONALUNITNAME	,JSPROP_ENUMERATE	,316},
	{"commonname"				,CRYPTCERT_PROP_COMMONNAME	,JSPROP_ENUMERATE	,316},
	{"othername_typeid"			,CRYPTCERT_PROP_OTHERNAME_TYPEID	,JSPROP_ENUMERATE	,316},
	{"othername_value"			,CRYPTCERT_PROP_OTHERNAME_VALUE	,JSPROP_ENUMERATE	,316},
	{"rfc822name"				,CRYPTCERT_PROP_RFC822NAME	,JSPROP_ENUMERATE	,316},
	{"dnsname"					,CRYPTCERT_PROP_DNSNAME	,JSPROP_ENUMERATE	,316},
	{"directoryname"			,CRYPTCERT_PROP_DIRECTORYNAME	,JSPROP_ENUMERATE	,316},
	{"edipartyname_nameassigner",CRYPTCERT_PROP_EDIPARTYNAME_NAMEASSIGNER	,JSPROP_ENUMERATE	,316},
	{"edipartyname_partyname"	,CRYPTCERT_PROP_EDIPARTYNAME_PARTYNAME	,JSPROP_ENUMERATE	,316},
	{"uniformresourceidentifier",CRYPTCERT_PROP_UNIFORMRESOURCEIDENTIFIER	,JSPROP_ENUMERATE	,316},
	{"ipaddress"				,CRYPTCERT_PROP_IPADDRESS	,JSPROP_ENUMERATE	,316},
	{"registeredid"				,CRYPTCERT_PROP_REGISTEREDID	,JSPROP_ENUMERATE	,316},
	{"challengepassword"		,CRYPTCERT_PROP_CHALLENGEPASSWORD	,JSPROP_ENUMERATE	,316},
	{"crlextreason"				,CRYPTCERT_PROP_CRLEXTREASON	,JSPROP_ENUMERATE	,316},
	{"keyfeatures"				,CRYPTCERT_PROP_KEYFEATURES	,JSPROP_ENUMERATE	,316},
	{"authorityinfoaccess"		,CRYPTCERT_PROP_AUTHORITYINFOACCESS	,JSPROP_ENUMERATE	,316},
	{"authorityinfo_rtcs"		,CRYPTCERT_PROP_AUTHORITYINFO_RTCS	,JSPROP_ENUMERATE	,316},
	{"authorityinfo_ocsp"		,CRYPTCERT_PROP_AUTHORITYINFO_OCSP	,JSPROP_ENUMERATE	,316},
	{"authorityinfo_caissuers"	,CRYPTCERT_PROP_AUTHORITYINFO_CAISSUERS	,JSPROP_ENUMERATE	,316},
	{"authorityinfo_certstore"	,CRYPTCERT_PROP_AUTHORITYINFO_CERTSTORE	,JSPROP_ENUMERATE	,316},
	{"authorityinfo_crls"		,CRYPTCERT_PROP_AUTHORITYINFO_CRLS	,JSPROP_ENUMERATE	,316},
	{"biometricinfo"			,CRYPTCERT_PROP_BIOMETRICINFO	,JSPROP_ENUMERATE	,316},
	{"biometricinfo_type"		,CRYPTCERT_PROP_BIOMETRICINFO_TYPE	,JSPROP_ENUMERATE	,316},
	{"biometricinfo_hashalgo"	,CRYPTCERT_PROP_BIOMETRICINFO_HASHALGO	,JSPROP_ENUMERATE	,316},
	{"biometricinfo_hash"		,CRYPTCERT_PROP_BIOMETRICINFO_HASH	,JSPROP_ENUMERATE	,316},
	{"biometricinfo_url"		,CRYPTCERT_PROP_BIOMETRICINFO_URL	,JSPROP_ENUMERATE	,316},
	{"qcstatement"				,CRYPTCERT_PROP_QCSTATEMENT	,JSPROP_ENUMERATE	,316},
	{"qcstatement_semantics"	,CRYPTCERT_PROP_QCSTATEMENT_SEMANTICS	,JSPROP_ENUMERATE	,316},
	{"qcstatement_registrationauthority",CRYPTCERT_PROP_QCSTATEMENT_REGISTRATIONAUTHORITY	,JSPROP_ENUMERATE	,316},
	{"ipaddressblocks"			,CRYPTCERT_PROP_IPADDRESSBLOCKS	,JSPROP_ENUMERATE	,316},
	{"ipaddressblocks_addressfamily",CRYPTCERT_PROP_IPADDRESSBLOCKS_ADDRESSFAMILY	,JSPROP_ENUMERATE	,316},
	{"ipaddressblocks_prefix"	,CRYPTCERT_PROP_IPADDRESSBLOCKS_PREFIX	,JSPROP_ENUMERATE	,316},
	{"ipaddressblocks_min"		,CRYPTCERT_PROP_IPADDRESSBLOCKS_MIN	,JSPROP_ENUMERATE	,316},
	{"ipaddressblocks_max"		,CRYPTCERT_PROP_IPADDRESSBLOCKS_MAX	,JSPROP_ENUMERATE	,316},
	{"autonomoussysids"			,CRYPTCERT_PROP_AUTONOMOUSSYSIDS	,JSPROP_ENUMERATE	,316},
	{"autonomoussysids_asnum_id",CRYPTCERT_PROP_AUTONOMOUSSYSIDS_ASNUM_ID	,JSPROP_ENUMERATE	,316},
	{"autonomoussysids_asnum_min",CRYPTCERT_PROP_AUTONOMOUSSYSIDS_ASNUM_MIN	,JSPROP_ENUMERATE	,316},
	{"autonomoussysids_asnum_max",CRYPTCERT_PROP_AUTONOMOUSSYSIDS_ASNUM_MAX	,JSPROP_ENUMERATE	,316},
	{"ocsp_nonce"				,CRYPTCERT_PROP_OCSP_NONCE	,JSPROP_ENUMERATE	,316},
	{"ocsp_response"			,CRYPTCERT_PROP_OCSP_RESPONSE	,JSPROP_ENUMERATE	,316},
	{"ocsp_response_ocsp"		,CRYPTCERT_PROP_OCSP_RESPONSE_OCSP	,JSPROP_ENUMERATE	,316},
	{"ocsp_nocheck"				,CRYPTCERT_PROP_OCSP_NOCHECK	,JSPROP_ENUMERATE	,316},
	{"ocsp_archivecutoff"		,CRYPTCERT_PROP_OCSP_ARCHIVECUTOFF	,JSPROP_ENUMERATE	,316},
	{"subjectinfoaccess"		,CRYPTCERT_PROP_SUBJECTINFOACCESS	,JSPROP_ENUMERATE	,316},
	{"subjectinfo_timestamping"	,CRYPTCERT_PROP_SUBJECTINFO_TIMESTAMPING	,JSPROP_ENUMERATE	,316},
	{"subjectinfo_carepository"	,CRYPTCERT_PROP_SUBJECTINFO_CAREPOSITORY	,JSPROP_ENUMERATE	,316},
	{"subjectinfo_signedobjectrepository",CRYPTCERT_PROP_SUBJECTINFO_SIGNEDOBJECTREPOSITORY	,JSPROP_ENUMERATE	,316},
	{"subjectinfo_rpkimanifest"	,CRYPTCERT_PROP_SUBJECTINFO_RPKIMANIFEST	,JSPROP_ENUMERATE	,316},
	{"subjectinfo_signedobject"	,CRYPTCERT_PROP_SUBJECTINFO_SIGNEDOBJECT	,JSPROP_ENUMERATE	,316},
	{"sigg_dateofcertgen"		,CRYPTCERT_PROP_SIGG_DATEOFCERTGEN	,JSPROP_ENUMERATE	,316},
	{"sigg_procuration"			,CRYPTCERT_PROP_SIGG_PROCURATION	,JSPROP_ENUMERATE	,316},
	{"sigg_procure_country"		,CRYPTCERT_PROP_SIGG_PROCURE_COUNTRY	,JSPROP_ENUMERATE	,316},
	{"sigg_procure_typeofsubstitution",CRYPTCERT_PROP_SIGG_PROCURE_TYPEOFSUBSTITUTION	,JSPROP_ENUMERATE	,316},
	{"sigg_procure_signingfor"	,CRYPTCERT_PROP_SIGG_PROCURE_SIGNINGFOR	,JSPROP_ENUMERATE	,316},
	{"sigg_admissions"			,CRYPTCERT_PROP_SIGG_ADMISSIONS	,JSPROP_ENUMERATE	,316},
	{"sigg_admissions_authority",CRYPTCERT_PROP_SIGG_ADMISSIONS_AUTHORITY	,JSPROP_ENUMERATE	,316},
	{"sigg_admissions_namingauthid",CRYPTCERT_PROP_SIGG_ADMISSIONS_NAMINGAUTHID	,JSPROP_ENUMERATE	,316},
	{"sigg_admissions_namingauthurl",CRYPTCERT_PROP_SIGG_ADMISSIONS_NAMINGAUTHURL	,JSPROP_ENUMERATE	,316},
	{"sigg_admissions_namingauthtext",CRYPTCERT_PROP_SIGG_ADMISSIONS_NAMINGAUTHTEXT	,JSPROP_ENUMERATE	,316},
	{"sigg_admissions_professionitem",CRYPTCERT_PROP_SIGG_ADMISSIONS_PROFESSIONITEM	,JSPROP_ENUMERATE	,316},
	{"sigg_admissions_professionoid",CRYPTCERT_PROP_SIGG_ADMISSIONS_PROFESSIONOID	,JSPROP_ENUMERATE	,316},
	{"sigg_admissions_registrationnumber",CRYPTCERT_PROP_SIGG_ADMISSIONS_REGISTRATIONNUMBER	,JSPROP_ENUMERATE	,316},
	{"sigg_monetarylimit"		,CRYPTCERT_PROP_SIGG_MONETARYLIMIT	,JSPROP_ENUMERATE	,316},
	{"sigg_monetary_currency"	,CRYPTCERT_PROP_SIGG_MONETARY_CURRENCY	,JSPROP_ENUMERATE	,316},
	{"sigg_monetary_amount"		,CRYPTCERT_PROP_SIGG_MONETARY_AMOUNT	,JSPROP_ENUMERATE	,316},
	{"sigg_monetary_exponent"	,CRYPTCERT_PROP_SIGG_MONETARY_EXPONENT	,JSPROP_ENUMERATE	,316},
	{"sigg_declarationofmajority",CRYPTCERT_PROP_SIGG_DECLARATIONOFMAJORITY	,JSPROP_ENUMERATE	,316},
	{"sigg_declarationofmajority_country",CRYPTCERT_PROP_SIGG_DECLARATIONOFMAJORITY_COUNTRY	,JSPROP_ENUMERATE	,316},
	{"sigg_restriction"			,CRYPTCERT_PROP_SIGG_RESTRICTION	,JSPROP_ENUMERATE	,316},
	{"sigg_certhash"			,CRYPTCERT_PROP_SIGG_CERTHASH	,JSPROP_ENUMERATE	,316},
	{"sigg_additionalinformation",CRYPTCERT_PROP_SIGG_ADDITIONALINFORMATION	,JSPROP_ENUMERATE	,316},
	{"strongextranet"			,CRYPTCERT_PROP_STRONGEXTRANET	,JSPROP_ENUMERATE	,316},
	{"strongextranet_zone"		,CRYPTCERT_PROP_STRONGEXTRANET_ZONE	,JSPROP_ENUMERATE	,316},
	{"strongextranet_id"		,CRYPTCERT_PROP_STRONGEXTRANET_ID	,JSPROP_ENUMERATE	,316},
	{"subjectdirectoryattributes",CRYPTCERT_PROP_SUBJECTDIRECTORYATTRIBUTES	,JSPROP_ENUMERATE	,316},
	{"subjectdir_type"			,CRYPTCERT_PROP_SUBJECTDIR_TYPE	,JSPROP_ENUMERATE	,316},
	{"subjectdir_values"		,CRYPTCERT_PROP_SUBJECTDIR_VALUES	,JSPROP_ENUMERATE	,316},
	{"subjectkeyidentifier"		,CRYPTCERT_PROP_SUBJECTKEYIDENTIFIER	,JSPROP_ENUMERATE	,316},
	{"keyusage"					,CRYPTCERT_PROP_KEYUSAGE	,JSPROP_ENUMERATE	,316},
	{"privatekeyusageperiod"	,CRYPTCERT_PROP_PRIVATEKEYUSAGEPERIOD	,JSPROP_ENUMERATE	,316},
	{"privatekey_notbefore"		,CRYPTCERT_PROP_PRIVATEKEY_NOTBEFORE	,JSPROP_ENUMERATE	,316},
	{"privatekey_notafter"		,CRYPTCERT_PROP_PRIVATEKEY_NOTAFTER	,JSPROP_ENUMERATE	,316},
	{"subjectaltname"			,CRYPTCERT_PROP_SUBJECTALTNAME	,JSPROP_ENUMERATE	,316},
	{"issueraltname"			,CRYPTCERT_PROP_ISSUERALTNAME	,JSPROP_ENUMERATE	,316},
	{"basicconstraints"			,CRYPTCERT_PROP_BASICCONSTRAINTS	,JSPROP_ENUMERATE	,316},
	{"ca"						,CRYPTCERT_PROP_CA	,JSPROP_ENUMERATE	,316},
	{"pathlenconstraint"		,CRYPTCERT_PROP_PATHLENCONSTRAINT	,JSPROP_ENUMERATE	,316},
	{"crlnumber"				,CRYPTCERT_PROP_CRLNUMBER	,JSPROP_ENUMERATE	,316},
	{"crlreason"				,CRYPTCERT_PROP_CRLREASON	,JSPROP_ENUMERATE	,316},
	{"holdinstructioncode"		,CRYPTCERT_PROP_HOLDINSTRUCTIONCODE	,JSPROP_ENUMERATE	,316},
	{"invaliditydate"			,CRYPTCERT_PROP_INVALIDITYDATE	,JSPROP_ENUMERATE	,316},
	{"deltacrlindicator"		,CRYPTCERT_PROP_DELTACRLINDICATOR	,JSPROP_ENUMERATE	,316},
	{"issuingdistributionpoint"	,CRYPTCERT_PROP_ISSUINGDISTRIBUTIONPOINT	,JSPROP_ENUMERATE	,316},
	{"issuingdist_fullname"		,CRYPTCERT_PROP_ISSUINGDIST_FULLNAME	,JSPROP_ENUMERATE	,316},
	{"issuingdist_usercertsonly",CRYPTCERT_PROP_ISSUINGDIST_USERCERTSONLY	,JSPROP_ENUMERATE	,316},
	{"issuingdist_cacertsonly"	,CRYPTCERT_PROP_ISSUINGDIST_CACERTSONLY	,JSPROP_ENUMERATE	,316},
	{"issuingdist_somereasonsonly",CRYPTCERT_PROP_ISSUINGDIST_SOMEREASONSONLY	,JSPROP_ENUMERATE	,316},
	{"issuingdist_indirectcrl"	,CRYPTCERT_PROP_ISSUINGDIST_INDIRECTCRL	,JSPROP_ENUMERATE	,316},
	{"certificateissuer"		,CRYPTCERT_PROP_CERTIFICATEISSUER	,JSPROP_ENUMERATE	,316},
	{"nameconstraints"			,CRYPTCERT_PROP_NAMECONSTRAINTS	,JSPROP_ENUMERATE	,316},
	{"permittedsubtrees"		,CRYPTCERT_PROP_PERMITTEDSUBTREES	,JSPROP_ENUMERATE	,316},
	{"excludedsubtrees"			,CRYPTCERT_PROP_EXCLUDEDSUBTREES	,JSPROP_ENUMERATE	,316},
	{"crldistributionpoint"		,CRYPTCERT_PROP_CRLDISTRIBUTIONPOINT	,JSPROP_ENUMERATE	,316},
	{"crldist_fullname"			,CRYPTCERT_PROP_CRLDIST_FULLNAME	,JSPROP_ENUMERATE	,316},
	{"crldist_reasons"			,CRYPTCERT_PROP_CRLDIST_REASONS	,JSPROP_ENUMERATE	,316},
	{"crldist_crlissuer"		,CRYPTCERT_PROP_CRLDIST_CRLISSUER	,JSPROP_ENUMERATE	,316},
	{"certificatepolicies"		,CRYPTCERT_PROP_CERTIFICATEPOLICIES	,JSPROP_ENUMERATE	,316},
	{"certpolicyid"				,CRYPTCERT_PROP_CERTPOLICYID	,JSPROP_ENUMERATE	,316},
	{"certpolicy_cpsuri"		,CRYPTCERT_PROP_CERTPOLICY_CPSURI	,JSPROP_ENUMERATE	,316},
	{"certpolicy_organization"	,CRYPTCERT_PROP_CERTPOLICY_ORGANIZATION	,JSPROP_ENUMERATE	,316},
	{"certpolicy_noticenumbers"	,CRYPTCERT_PROP_CERTPOLICY_NOTICENUMBERS	,JSPROP_ENUMERATE	,316},
	{"certpolicy_explicittext"	,CRYPTCERT_PROP_CERTPOLICY_EXPLICITTEXT	,JSPROP_ENUMERATE	,316},
	{"policymappings"			,CRYPTCERT_PROP_POLICYMAPPINGS	,JSPROP_ENUMERATE	,316},
	{"issuerdomainpolicy"		,CRYPTCERT_PROP_ISSUERDOMAINPOLICY	,JSPROP_ENUMERATE	,316},
	{"subjectdomainpolicy"		,CRYPTCERT_PROP_SUBJECTDOMAINPOLICY	,JSPROP_ENUMERATE	,316},
	{"authoritykeyidentifier"	,CRYPTCERT_PROP_AUTHORITYKEYIDENTIFIER	,JSPROP_ENUMERATE	,316},
	{"authority_keyidentifier"	,CRYPTCERT_PROP_AUTHORITY_KEYIDENTIFIER	,JSPROP_ENUMERATE	,316},
	{"authority_certissuer"		,CRYPTCERT_PROP_AUTHORITY_CERTISSUER	,JSPROP_ENUMERATE	,316},
	{"authority_certserialnumber",CRYPTCERT_PROP_AUTHORITY_CERTSERIALNUMBER	,JSPROP_ENUMERATE	,316},
	{"policyconstraints"		,CRYPTCERT_PROP_POLICYCONSTRAINTS	,JSPROP_ENUMERATE	,316},
	{"requireexplicitpolicy"	,CRYPTCERT_PROP_REQUIREEXPLICITPOLICY	,JSPROP_ENUMERATE	,316},
	{"inhibitpolicymapping"		,CRYPTCERT_PROP_INHIBITPOLICYMAPPING	,JSPROP_ENUMERATE	,316},
	{"extkeyusage"				,CRYPTCERT_PROP_EXTKEYUSAGE	,JSPROP_ENUMERATE	,316},
	{"extkey_ms_individualcodesigning",CRYPTCERT_PROP_EXTKEY_MS_INDIVIDUALCODESIGNING	,JSPROP_ENUMERATE	,316},
	{"extkey_ms_commercialcodesigning",CRYPTCERT_PROP_EXTKEY_MS_COMMERCIALCODESIGNING	,JSPROP_ENUMERATE	,316},
	{"extkey_ms_certtrustlistsigning",CRYPTCERT_PROP_EXTKEY_MS_CERTTRUSTLISTSIGNING	,JSPROP_ENUMERATE	,316},
	{"extkey_ms_timestampsigning",CRYPTCERT_PROP_EXTKEY_MS_TIMESTAMPSIGNING	,JSPROP_ENUMERATE	,316},
	{"extkey_ms_servergatedcrypto",CRYPTCERT_PROP_EXTKEY_MS_SERVERGATEDCRYPTO	,JSPROP_ENUMERATE	,316},
	{"extkey_ms_encryptedfilesystem",CRYPTCERT_PROP_EXTKEY_MS_ENCRYPTEDFILESYSTEM	,JSPROP_ENUMERATE	,316},
	{"extkey_serverauth"		,CRYPTCERT_PROP_EXTKEY_SERVERAUTH	,JSPROP_ENUMERATE	,316},
	{"extkey_clientauth"		,CRYPTCERT_PROP_EXTKEY_CLIENTAUTH	,JSPROP_ENUMERATE	,316},
	{"extkey_codesigning"		,CRYPTCERT_PROP_EXTKEY_CODESIGNING	,JSPROP_ENUMERATE	,316},
	{"extkey_emailprotection"	,CRYPTCERT_PROP_EXTKEY_EMAILPROTECTION	,JSPROP_ENUMERATE	,316},
	{"extkey_ipsecendsystem"	,CRYPTCERT_PROP_EXTKEY_IPSECENDSYSTEM	,JSPROP_ENUMERATE	,316},
	{"extkey_ipsectunnel"		,CRYPTCERT_PROP_EXTKEY_IPSECTUNNEL	,JSPROP_ENUMERATE	,316},
	{"extkey_ipsecuser"			,CRYPTCERT_PROP_EXTKEY_IPSECUSER	,JSPROP_ENUMERATE	,316},
	{"extkey_timestamping"		,CRYPTCERT_PROP_EXTKEY_TIMESTAMPING	,JSPROP_ENUMERATE	,316},
	{"extkey_ocspsigning"		,CRYPTCERT_PROP_EXTKEY_OCSPSIGNING	,JSPROP_ENUMERATE	,316},
	{"extkey_directoryservice"	,CRYPTCERT_PROP_EXTKEY_DIRECTORYSERVICE	,JSPROP_ENUMERATE	,316},
	{"extkey_anykeyusage"		,CRYPTCERT_PROP_EXTKEY_ANYKEYUSAGE	,JSPROP_ENUMERATE	,316},
	{"extkey_ns_servergatedcrypto",CRYPTCERT_PROP_EXTKEY_NS_SERVERGATEDCRYPTO	,JSPROP_ENUMERATE	,316},
	{"extkey_vs_servergatedcrypto_ca",CRYPTCERT_PROP_EXTKEY_VS_SERVERGATEDCRYPTO_CA	,JSPROP_ENUMERATE	,316},
	{"crlstreamidentifier"		,CRYPTCERT_PROP_CRLSTREAMIDENTIFIER	,JSPROP_ENUMERATE	,316},
	{"freshestcrl"				,CRYPTCERT_PROP_FRESHESTCRL	,JSPROP_ENUMERATE	,316},
	{"freshestcrl_fullname"		,CRYPTCERT_PROP_FRESHESTCRL_FULLNAME	,JSPROP_ENUMERATE	,316},
	{"freshestcrl_reasons"		,CRYPTCERT_PROP_FRESHESTCRL_REASONS	,JSPROP_ENUMERATE	,316},
	{"freshestcrl_crlissuer"	,CRYPTCERT_PROP_FRESHESTCRL_CRLISSUER	,JSPROP_ENUMERATE	,316},
	{"orderedlist"				,CRYPTCERT_PROP_ORDEREDLIST	,JSPROP_ENUMERATE	,316},
	{"baseupdatetime"			,CRYPTCERT_PROP_BASEUPDATETIME	,JSPROP_ENUMERATE	,316},
	{"deltainfo"				,CRYPTCERT_PROP_DELTAINFO	,JSPROP_ENUMERATE	,316},
	{"deltainfo_location"	,CRYPTCERT_PROP_DELTAINFO_LOCATION	,JSPROP_ENUMERATE	,316},
	{"deltainfo_nextdelta"	,CRYPTCERT_PROP_DELTAINFO_NEXTDELTA	,JSPROP_ENUMERATE	,316},
	{"inhibitanypolicy"	,CRYPTCERT_PROP_INHIBITANYPOLICY	,JSPROP_ENUMERATE	,316},
	{"toberevoked"	,CRYPTCERT_PROP_TOBEREVOKED	,JSPROP_ENUMERATE	,316},
	{"toberevoked_certissuer"	,CRYPTCERT_PROP_TOBEREVOKED_CERTISSUER	,JSPROP_ENUMERATE	,316},
	{"toberevoked_reasoncode"	,CRYPTCERT_PROP_TOBEREVOKED_REASONCODE	,JSPROP_ENUMERATE	,316},
	{"toberevoked_revocationtime"	,CRYPTCERT_PROP_TOBEREVOKED_REVOCATIONTIME	,JSPROP_ENUMERATE	,316},
	{"toberevoked_certserialnumber"	,CRYPTCERT_PROP_TOBEREVOKED_CERTSERIALNUMBER	,JSPROP_ENUMERATE	,316},
	{"revokedgroups"	,CRYPTCERT_PROP_REVOKEDGROUPS	,JSPROP_ENUMERATE	,316},
	{"revokedgroups_certissuer"	,CRYPTCERT_PROP_REVOKEDGROUPS_CERTISSUER	,JSPROP_ENUMERATE	,316},
	{"revokedgroups_reasoncode"	,CRYPTCERT_PROP_REVOKEDGROUPS_REASONCODE	,JSPROP_ENUMERATE	,316},
	{"revokedgroups_invaliditydate"	,CRYPTCERT_PROP_REVOKEDGROUPS_INVALIDITYDATE	,JSPROP_ENUMERATE	,316},
	{"revokedgroups_startingnumber"	,CRYPTCERT_PROP_REVOKEDGROUPS_STARTINGNUMBER	,JSPROP_ENUMERATE	,316},
	{"revokedgroups_endingnumber"	,CRYPTCERT_PROP_REVOKEDGROUPS_ENDINGNUMBER	,JSPROP_ENUMERATE	,316},
	{"expiredcertsoncrl"	,CRYPTCERT_PROP_EXPIREDCERTSONCRL	,JSPROP_ENUMERATE	,316},
	{"aaissuingdistributionpoint"	,CRYPTCERT_PROP_AAISSUINGDISTRIBUTIONPOINT	,JSPROP_ENUMERATE	,316},
	{"aaissuingdist_fullname"	,CRYPTCERT_PROP_AAISSUINGDIST_FULLNAME	,JSPROP_ENUMERATE	,316},
	{"aaissuingdist_somereasonsonly"	,CRYPTCERT_PROP_AAISSUINGDIST_SOMEREASONSONLY	,JSPROP_ENUMERATE	,316},
	{"aaissuingdist_indirectcrl"	,CRYPTCERT_PROP_AAISSUINGDIST_INDIRECTCRL	,JSPROP_ENUMERATE	,316},
	{"aaissuingdist_userattrcerts"	,CRYPTCERT_PROP_AAISSUINGDIST_USERATTRCERTS	,JSPROP_ENUMERATE	,316},
	{"aaissuingdist_aacerts"	,CRYPTCERT_PROP_AAISSUINGDIST_AACERTS	,JSPROP_ENUMERATE	,316},
	{"aaissuingdist_soacerts"	,CRYPTCERT_PROP_AAISSUINGDIST_SOACERTS	,JSPROP_ENUMERATE	,316},
	{"ns_certtype"	,CRYPTCERT_PROP_NS_CERTTYPE	,JSPROP_ENUMERATE	,316},
	{"ns_baseurl"	,CRYPTCERT_PROP_NS_BASEURL	,JSPROP_ENUMERATE	,316},
	{"ns_revocationurl"	,CRYPTCERT_PROP_NS_REVOCATIONURL	,JSPROP_ENUMERATE	,316},
	{"ns_carevocationurl"	,CRYPTCERT_PROP_NS_CAREVOCATIONURL	,JSPROP_ENUMERATE	,316},
	{"ns_certrenewalurl"	,CRYPTCERT_PROP_NS_CERTRENEWALURL	,JSPROP_ENUMERATE	,316},
	{"ns_capolicyurl"	,CRYPTCERT_PROP_NS_CAPOLICYURL	,JSPROP_ENUMERATE	,316},
	{"ns_sslservername"	,CRYPTCERT_PROP_NS_SSLSERVERNAME	,JSPROP_ENUMERATE	,316},
	{"ns_comment"	,CRYPTCERT_PROP_NS_COMMENT	,JSPROP_ENUMERATE	,316},
	{"set_hashedrootkey"	,CRYPTCERT_PROP_SET_HASHEDROOTKEY	,JSPROP_ENUMERATE	,316},
	{"set_rootkeythumbprint"	,CRYPTCERT_PROP_SET_ROOTKEYTHUMBPRINT	,JSPROP_ENUMERATE	,316},
	{"set_certificatetype"	,CRYPTCERT_PROP_SET_CERTIFICATETYPE	,JSPROP_ENUMERATE	,316},
	{"set_merchantdata"	,CRYPTCERT_PROP_SET_MERCHANTDATA	,JSPROP_ENUMERATE	,316},
	{"set_merid"	,CRYPTCERT_PROP_SET_MERID	,JSPROP_ENUMERATE	,316},
	{"set_meracquirerbin"	,CRYPTCERT_PROP_SET_MERACQUIRERBIN	,JSPROP_ENUMERATE	,316},
	{"set_merchantlanguage"	,CRYPTCERT_PROP_SET_MERCHANTLANGUAGE	,JSPROP_ENUMERATE	,316},
	{"set_merchantname"	,CRYPTCERT_PROP_SET_MERCHANTNAME	,JSPROP_ENUMERATE	,316},
	{"set_merchantcity"	,CRYPTCERT_PROP_SET_MERCHANTCITY	,JSPROP_ENUMERATE	,316},
	{"set_merchantstateprovince"	,CRYPTCERT_PROP_SET_MERCHANTSTATEPROVINCE	,JSPROP_ENUMERATE	,316},
	{"set_merchantpostalcode"	,CRYPTCERT_PROP_SET_MERCHANTPOSTALCODE	,JSPROP_ENUMERATE	,316},
	{"set_merchantcountryname"	,CRYPTCERT_PROP_SET_MERCHANTCOUNTRYNAME	,JSPROP_ENUMERATE	,316},
	{"set_mercountry"	,CRYPTCERT_PROP_SET_MERCOUNTRY	,JSPROP_ENUMERATE	,316},
	{"set_merauthflag"	,CRYPTCERT_PROP_SET_MERAUTHFLAG	,JSPROP_ENUMERATE	,316},
	{"set_certcardrequired"	,CRYPTCERT_PROP_SET_CERTCARDREQUIRED	,JSPROP_ENUMERATE	,316},
	{"set_tunneling"	,CRYPTCERT_PROP_SET_TUNNELING	,JSPROP_ENUMERATE	,316},
	{"set_tunnelingflag"	,CRYPTCERT_PROP_SET_TUNNELINGFLAG	,JSPROP_ENUMERATE	,316},
	{"set_tunnelingalgid"	,CRYPTCERT_PROP_SET_TUNNELINGALGID	,JSPROP_ENUMERATE	,316},
	{"cms_contenttype"	,CRYPTCERT_PROP_CMS_CONTENTTYPE	,JSPROP_ENUMERATE	,316},
	{"cms_messagedigest"	,CRYPTCERT_PROP_CMS_MESSAGEDIGEST	,JSPROP_ENUMERATE	,316},
	{"cms_signingtime"	,CRYPTCERT_PROP_CMS_SIGNINGTIME	,JSPROP_ENUMERATE	,316},
	{"cms_countersignature"	,CRYPTCERT_PROP_CMS_COUNTERSIGNATURE	,JSPROP_ENUMERATE	,316},
	{"cms_signingdescription"	,CRYPTCERT_PROP_CMS_SIGNINGDESCRIPTION	,JSPROP_ENUMERATE	,316},
	{"cms_smimecapabilities"	,CRYPTCERT_PROP_CMS_SMIMECAPABILITIES	,JSPROP_ENUMERATE	,316},
	{"cms_smimecap_3des"	,CRYPTCERT_PROP_CMS_SMIMECAP_3DES	,JSPROP_ENUMERATE	,316},
	{"cms_smimecap_aes"	,CRYPTCERT_PROP_CMS_SMIMECAP_AES	,JSPROP_ENUMERATE	,316},
	{"cms_smimecap_cast128"	,CRYPTCERT_PROP_CMS_SMIMECAP_CAST128	,JSPROP_ENUMERATE	,316},
	{"cms_smimecap_shang"	,CRYPTCERT_PROP_CMS_SMIMECAP_SHAng	,JSPROP_ENUMERATE	,316},
	{"cms_smimecap_sha2"	,CRYPTCERT_PROP_CMS_SMIMECAP_SHA2	,JSPROP_ENUMERATE	,316},
	{"cms_smimecap_sha1"	,CRYPTCERT_PROP_CMS_SMIMECAP_SHA1	,JSPROP_ENUMERATE	,316},
	{"cms_smimecap_hmac_shang"	,CRYPTCERT_PROP_CMS_SMIMECAP_HMAC_SHAng	,JSPROP_ENUMERATE	,316},
	{"cms_smimecap_hmac_sha2"	,CRYPTCERT_PROP_CMS_SMIMECAP_HMAC_SHA2	,JSPROP_ENUMERATE	,316},
	{"cms_smimecap_hmac_sha1"	,CRYPTCERT_PROP_CMS_SMIMECAP_HMAC_SHA1	,JSPROP_ENUMERATE	,316},
	{"cms_smimecap_authenc256"	,CRYPTCERT_PROP_CMS_SMIMECAP_AUTHENC256	,JSPROP_ENUMERATE	,316},
	{"cms_smimecap_authenc128"	,CRYPTCERT_PROP_CMS_SMIMECAP_AUTHENC128	,JSPROP_ENUMERATE	,316},
	{"cms_smimecap_rsa_shang"	,CRYPTCERT_PROP_CMS_SMIMECAP_RSA_SHAng	,JSPROP_ENUMERATE	,316},
	{"cms_smimecap_rsa_sha2"	,CRYPTCERT_PROP_CMS_SMIMECAP_RSA_SHA2	,JSPROP_ENUMERATE	,316},
	{"cms_smimecap_rsa_sha1"	,CRYPTCERT_PROP_CMS_SMIMECAP_RSA_SHA1	,JSPROP_ENUMERATE	,316},
	{"cms_smimecap_dsa_sha1"	,CRYPTCERT_PROP_CMS_SMIMECAP_DSA_SHA1	,JSPROP_ENUMERATE	,316},
#if 0	// TODO: TinyID only goes to 255!
	{"cms_smimecap_ecdsa_shang"	,CRYPTCERT_PROP_CMS_SMIMECAP_ECDSA_SHAng	,JSPROP_ENUMERATE	,316},
	{"cms_smimecap_ecdsa_sha2"	,CRYPTCERT_PROP_CMS_SMIMECAP_ECDSA_SHA2	,JSPROP_ENUMERATE	,316},
	{"cms_smimecap_ecdsa_sha1"	,CRYPTCERT_PROP_CMS_SMIMECAP_ECDSA_SHA1	,JSPROP_ENUMERATE	,316},
	{"cms_smimecap_prefersigneddata"	,CRYPTCERT_PROP_CMS_SMIMECAP_PREFERSIGNEDDATA	,JSPROP_ENUMERATE	,316},
	{"cms_smimecap_cannotdecryptany"	,CRYPTCERT_PROP_CMS_SMIMECAP_CANNOTDECRYPTANY	,JSPROP_ENUMERATE	,316},
	{"cms_smimecap_preferbinaryinside"	,CRYPTCERT_PROP_CMS_SMIMECAP_PREFERBINARYINSIDE	,JSPROP_ENUMERATE	,316},
	{"cms_receiptrequest"	,CRYPTCERT_PROP_CMS_RECEIPTREQUEST	,JSPROP_ENUMERATE	,316},
	{"cms_receipt_contentidentifier"	,CRYPTCERT_PROP_CMS_RECEIPT_CONTENTIDENTIFIER	,JSPROP_ENUMERATE	,316},
	{"cms_receipt_from"	,CRYPTCERT_PROP_CMS_RECEIPT_FROM	,JSPROP_ENUMERATE	,316},
	{"cms_receipt_to"	,CRYPTCERT_PROP_CMS_RECEIPT_TO	,JSPROP_ENUMERATE	,316},
	{"cms_securitylabel"	,CRYPTCERT_PROP_CMS_SECURITYLABEL	,JSPROP_ENUMERATE	,316},
	{"cms_seclabel_policy"	,CRYPTCERT_PROP_CMS_SECLABEL_POLICY	,JSPROP_ENUMERATE	,316},
	{"cms_seclabel_classification"	,CRYPTCERT_PROP_CMS_SECLABEL_CLASSIFICATION	,JSPROP_ENUMERATE	,316},
	{"cms_seclabel_privacymark"	,CRYPTCERT_PROP_CMS_SECLABEL_PRIVACYMARK	,JSPROP_ENUMERATE	,316},
	{"cms_seclabel_cattype"	,CRYPTCERT_PROP_CMS_SECLABEL_CATTYPE	,JSPROP_ENUMERATE	,316},
	{"cms_seclabel_catvalue"	,CRYPTCERT_PROP_CMS_SECLABEL_CATVALUE	,JSPROP_ENUMERATE	,316},
	{"cms_mlexpansionhistory"	,CRYPTCERT_PROP_CMS_MLEXPANSIONHISTORY	,JSPROP_ENUMERATE	,316},
	{"cms_mlexp_entityidentifier"	,CRYPTCERT_PROP_CMS_MLEXP_ENTITYIDENTIFIER	,JSPROP_ENUMERATE	,316},
	{"cms_mlexp_time"	,CRYPTCERT_PROP_CMS_MLEXP_TIME	,JSPROP_ENUMERATE	,316},
	{"cms_mlexp_none"	,CRYPTCERT_PROP_CMS_MLEXP_NONE	,JSPROP_ENUMERATE	,316},
	{"cms_mlexp_insteadof"	,CRYPTCERT_PROP_CMS_MLEXP_INSTEADOF	,JSPROP_ENUMERATE	,316},
	{"cms_mlexp_inadditionto"	,CRYPTCERT_PROP_CMS_MLEXP_INADDITIONTO	,JSPROP_ENUMERATE	,316},
	{"cms_contenthints"	,CRYPTCERT_PROP_CMS_CONTENTHINTS	,JSPROP_ENUMERATE	,316},
	{"cms_contenthint_description"	,CRYPTCERT_PROP_CMS_CONTENTHINT_DESCRIPTION	,JSPROP_ENUMERATE	,316},
	{"cms_contenthint_type"	,CRYPTCERT_PROP_CMS_CONTENTHINT_TYPE	,JSPROP_ENUMERATE	,316},
	{"cms_equivalentlabel"	,CRYPTCERT_PROP_CMS_EQUIVALENTLABEL	,JSPROP_ENUMERATE	,316},
	{"cms_eqvlabel_policy"	,CRYPTCERT_PROP_CMS_EQVLABEL_POLICY	,JSPROP_ENUMERATE	,316},
	{"cms_eqvlabel_classification"	,CRYPTCERT_PROP_CMS_EQVLABEL_CLASSIFICATION	,JSPROP_ENUMERATE	,316},
	{"cms_eqvlabel_privacymark"	,CRYPTCERT_PROP_CMS_EQVLABEL_PRIVACYMARK	,JSPROP_ENUMERATE	,316},
	{"cms_eqvlabel_cattype"	,CRYPTCERT_PROP_CMS_EQVLABEL_CATTYPE	,JSPROP_ENUMERATE	,316},
	{"cms_eqvlabel_catvalue"	,CRYPTCERT_PROP_CMS_EQVLABEL_CATVALUE	,JSPROP_ENUMERATE	,316},
	{"cms_signingcertificate"	,CRYPTCERT_PROP_CMS_SIGNINGCERTIFICATE	,JSPROP_ENUMERATE	,316},
	{"cms_signingcert_esscertid"	,CRYPTCERT_PROP_CMS_SIGNINGCERT_ESSCERTID	,JSPROP_ENUMERATE	,316},
	{"cms_signingcert_policies"	,CRYPTCERT_PROP_CMS_SIGNINGCERT_POLICIES	,JSPROP_ENUMERATE	,316},
	{"cms_signingcertificatev2"	,CRYPTCERT_PROP_CMS_SIGNINGCERTIFICATEV2	,JSPROP_ENUMERATE	,316},
	{"cms_signingcertv2_esscertidv2"	,CRYPTCERT_PROP_CMS_SIGNINGCERTV2_ESSCERTIDV2	,JSPROP_ENUMERATE	,316},
	{"cms_signingcertv2_policies"	,CRYPTCERT_PROP_CMS_SIGNINGCERTV2_POLICIES	,JSPROP_ENUMERATE	,316},
	{"cms_signaturepolicyid"	,CRYPTCERT_PROP_CMS_SIGNATUREPOLICYID	,JSPROP_ENUMERATE	,316},
	{"cms_sigpolicyid"	,CRYPTCERT_PROP_CMS_SIGPOLICYID	,JSPROP_ENUMERATE	,316},
	{"cms_sigpolicyhash"	,CRYPTCERT_PROP_CMS_SIGPOLICYHASH	,JSPROP_ENUMERATE	,316},
	{"cms_sigpolicy_cpsuri"	,CRYPTCERT_PROP_CMS_SIGPOLICY_CPSURI	,JSPROP_ENUMERATE	,316},
	{"cms_sigpolicy_organization"	,CRYPTCERT_PROP_CMS_SIGPOLICY_ORGANIZATION	,JSPROP_ENUMERATE	,316},
	{"cms_sigpolicy_noticenumbers"	,CRYPTCERT_PROP_CMS_SIGPOLICY_NOTICENUMBERS	,JSPROP_ENUMERATE	,316},
	{"cms_sigpolicy_explicittext"	,CRYPTCERT_PROP_CMS_SIGPOLICY_EXPLICITTEXT	,JSPROP_ENUMERATE	,316},
	{"cms_sigtypeidentifier"	,CRYPTCERT_PROP_CMS_SIGTYPEIDENTIFIER	,JSPROP_ENUMERATE	,316},
	{"cms_sigtypeid_originatorsig"	,CRYPTCERT_PROP_CMS_SIGTYPEID_ORIGINATORSIG	,JSPROP_ENUMERATE	,316},
	{"cms_sigtypeid_domainsig"	,CRYPTCERT_PROP_CMS_SIGTYPEID_DOMAINSIG	,JSPROP_ENUMERATE	,316},
	{"cms_sigtypeid_additionalattributes"	,CRYPTCERT_PROP_CMS_SIGTYPEID_ADDITIONALATTRIBUTES	,JSPROP_ENUMERATE	,316},
	{"cms_sigtypeid_reviewsig"	,CRYPTCERT_PROP_CMS_SIGTYPEID_REVIEWSIG	,JSPROP_ENUMERATE	,316},
	{"cms_nonce"	,CRYPTCERT_PROP_CMS_NONCE	,JSPROP_ENUMERATE	,316},
	{"scep_messagetype"	,CRYPTCERT_PROP_SCEP_MESSAGETYPE	,JSPROP_ENUMERATE	,316},
	{"scep_pkistatus"	,CRYPTCERT_PROP_SCEP_PKISTATUS	,JSPROP_ENUMERATE	,316},
	{"scep_failinfo"	,CRYPTCERT_PROP_SCEP_FAILINFO	,JSPROP_ENUMERATE	,316},
	{"scep_sendernonce"	,CRYPTCERT_PROP_SCEP_SENDERNONCE	,JSPROP_ENUMERATE	,316},
	{"scep_recipientnonce"	,CRYPTCERT_PROP_SCEP_RECIPIENTNONCE	,JSPROP_ENUMERATE	,316},
	{"scep_transactionid"	,CRYPTCERT_PROP_SCEP_TRANSACTIONID	,JSPROP_ENUMERATE	,316},
	{"cms_spcagencyinfo"	,CRYPTCERT_PROP_CMS_SPCAGENCYINFO	,JSPROP_ENUMERATE	,316},
	{"cms_spcagencyurl"	,CRYPTCERT_PROP_CMS_SPCAGENCYURL	,JSPROP_ENUMERATE	,316},
	{"cms_spcstatementtype"	,CRYPTCERT_PROP_CMS_SPCSTATEMENTTYPE	,JSPROP_ENUMERATE	,316},
	{"cms_spcstmt_individualcodesigning"	,CRYPTCERT_PROP_CMS_SPCSTMT_INDIVIDUALCODESIGNING	,JSPROP_ENUMERATE	,316},
	{"cms_spcstmt_commercialcodesigning"	,CRYPTCERT_PROP_CMS_SPCSTMT_COMMERCIALCODESIGNING	,JSPROP_ENUMERATE	,316},
	{"cms_spcopusinfo"	,CRYPTCERT_PROP_CMS_SPCOPUSINFO	,JSPROP_ENUMERATE	,316},
	{"cms_spcopusinfo_name"	,CRYPTCERT_PROP_CMS_SPCOPUSINFO_NAME	,JSPROP_ENUMERATE	,316},
	{"cms_spcopusinfo_url"	,CRYPTCERT_PROP_CMS_SPCOPUSINFO_URL	,JSPROP_ENUMERATE	,316},
#endif
	{0}
};

static jsSyncMethodSpec js_cryptcert_functions[] = {
	{"add_extension", js_add_extension, 0, JSTYPE_VOID, "oid, critical, extension"
	,JSDOCSTR("Adds a DER encoded certificate extension.")
	,316
	},
	{"check",	js_check,	0,	JSTYPE_BOOLEAN,	""
	,JSDOCSTR("Checks the certificate for validity.")
	,316
	},
	{"destroy",	js_destroy,	0,	JSTYPE_VOID,	""
	,JSDOCSTR("Destroys the certificate.")
	,316
	},
	{"export_cert",	js_export,	0,	JSTYPE_STRING,	"format"
	,JSDOCSTR("Exports the certificate in the format chosen from CryptCert.CERTFORMAT.")
	,316
	},
	{"get_attribute", js_get_attribute,	0,	JSTYPE_VOID,	"attr, value"
	,JSDOCSTR("Sets the specified attribute to the specified value")
	,316
	},
	{"get_attribute_string", js_get_attribute_string,	0,	JSTYPE_VOID,	"attr, value"
	,JSDOCSTR("Sets the specified attribute to the specified value")
	,316
	},
	{"get_attribute_time", js_get_attribute_time,	0,	JSTYPE_VOID,	"attr, value"
	,JSDOCSTR("Sets the specified attribute to the specified value")
	,316
	},
	{"set_attribute", js_set_attribute,	0,	JSTYPE_VOID,	"attr, value"
	,JSDOCSTR("Sets the specified attribute to the specified value")
	,316
	},
	{"set_attribute_string", js_set_attribute_string,	0,	JSTYPE_VOID,	"attr, value"
	,JSDOCSTR("Sets the specified attribute to the specified value")
	,316
	},
	{"set_attribute_time", js_set_attribute_time,	0,	JSTYPE_VOID,	"attr, value"
	,JSDOCSTR("Sets the specified attribute to the specified value")
	,316
	},
	{"sign",	js_sign,	0,	JSTYPE_VOID,	"key"
	,JSDOCSTR("Signs the certificate with the specified CryptContext")
	,316
	},
	{0}
};

static JSBool js_cryptcert_resolve(JSContext *cx, JSObject *obj, jsid id)
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

	ret=js_SyncResolve(cx, obj, name, js_cryptcert_properties, js_cryptcert_functions, NULL, 0);
	if(name)
		free(name);
	return ret;
}

static JSBool js_cryptcert_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_cryptcert_resolve(cx, obj, JSID_VOID));
}

JSClass js_cryptcert_class = {
     "CryptCert"				/* name			*/
    ,JSCLASS_HAS_PRIVATE		/* flags		*/
	,JS_PropertyStub			/* addProperty	*/
	,JS_PropertyStub			/* delProperty	*/
	,js_cryptcert_get			/* getProperty	*/
	,js_cryptcert_set			/* setProperty	*/
	,js_cryptcert_enumerate		/* enumerate	*/
	,js_cryptcert_resolve		/* resolve		*/
	,JS_ConvertStub				/* convert		*/
	,js_finalize_cryptcert		/* finalize		*/
};

JSObject* DLLCALL js_CreateCryptCertObject(JSContext* cx, CRYPT_CERTIFICATE cert)
{
	JSObject *obj;
	struct js_cryptcert_private_data *p;

	obj=JS_NewObject(cx, &js_cryptcert_class, NULL, NULL);

	if((p=(struct js_cryptcert_private_data *)malloc(sizeof(struct js_cryptcert_private_data)))==NULL) {
		JS_ReportError(cx,"malloc failed");
		return NULL;
	}
	memset(p,0,sizeof(struct js_cryptcert_private_data));
	p->cert = cert;

	if(!JS_SetPrivate(cx, obj, p)) {
		JS_ReportError(cx,"JS_SetPrivate failed");
		return NULL;
	}

	return obj;
}

// Constructor

static JSBool
js_cryptcert_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj;
	jsval *argv=JS_ARGV(cx, arglist);
	struct js_cryptcert_private_data *p;
	jsrefcount rc;
	int status;
	int type;
	char *buf;
	JSString *jscert;
	size_t len;

	do_cryptInit();
	obj=JS_NewObject(cx, &js_cryptcert_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
	if(argc != 1) {
		JS_ReportError(cx, "Incorrect number of arguments to CryptCert constructor.  Got %d, expected 1.", argc);
		return JS_FALSE;
	}

	if((p=(struct js_cryptcert_private_data *)malloc(sizeof(struct js_cryptcert_private_data)))==NULL) {
		JS_ReportError(cx,"malloc failed");
		return(JS_FALSE);
	}
	memset(p,0,sizeof(struct js_cryptcert_private_data));

	if(!JS_SetPrivate(cx, obj, p)) {
		JS_ReportError(cx,"JS_SetPrivate failed");
		return(JS_FALSE);
	}

	if (JSVAL_IS_STRING(argv[0])) {
		// Import certificate
		if ((jscert = JS_ValueToString(cx, argv[0])) == NULL) {
			JS_ReportError(cx, "Invalid cert");
			return JS_FALSE;
		}
		JSSTRING_TO_MSTRING(cx, jscert, buf, &len);
		HANDLE_PENDING(cx, buf);
		rc = JS_SUSPENDREQUEST(cx);
		status = cryptImportCert(buf, len, CRYPT_UNUSED, &p->cert);
		free(buf);
		JS_RESUMEREQUEST(cx, rc);
	}
	else {
		if (!JS_ValueToInt32(cx,argv[0],&type))
			return JS_FALSE;
		rc = JS_SUSPENDREQUEST(cx);
		status = cryptCreateCert(&p->cert, CRYPT_UNUSED, type);
		JS_RESUMEREQUEST(cx, rc);
	}

	if (cryptStatusError(status)) {
		JS_ReportError(cx, "CryptLib error %d", status);
		free(p);
		return JS_FALSE;
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,obj,"Class used for Certificates",31601);
	js_DescribeSyncConstructor(cx,obj,"To create a new CryptCert object: "
		"var c = new CryptCert('<i>type</i> | <i>cert</i>')</tt><br> "
		"where <i>type</i> "
		"is a value from CryptCert.TYPE and <i>cert</i> is a DER encoded certificate string"
		);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", cryptcert_prop_desc, JSPROP_READONLY);
#endif

	return(JS_TRUE);
}

JSObject* DLLCALL js_CreateCryptCertClass(JSContext* cx, JSObject* parent)
{
	JSObject*	cksobj;
	JSObject*	constructor;
	JSObject*	type;
	JSObject*	format;
	JSObject*	attr;
	JSObject*	cursor;
	jsval		val;

	cksobj = JS_InitClass(cx, parent, NULL
		,&js_cryptcert_class
		,js_cryptcert_constructor
		,1	/* number of constructor args */
		,NULL /* props, specified in constructor */
		,NULL /* funcs, specified in constructor */
		,NULL, NULL);

	if(JS_GetProperty(cx, parent, js_cryptcert_class.name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JS_ValueToObject(cx,val,&constructor);
		cursor = JS_DefineObject(cx, constructor, "CURSOR", NULL, NULL, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
		if(cursor != NULL) {
			JS_DefineProperty(cx, cursor, "FIRST", INT_TO_JSVAL(CRYPT_CURSOR_FIRST), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, cursor, "PREVIOUS", INT_TO_JSVAL(CRYPT_CURSOR_PREVIOUS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, cursor, "NEXT", INT_TO_JSVAL(CRYPT_CURSOR_NEXT), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, cursor, "LAST", INT_TO_JSVAL(CRYPT_CURSOR_LAST), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DeepFreezeObject(cx, cursor);
		}
		type = JS_DefineObject(cx, constructor, "TYPE", NULL, NULL, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
		if(type != NULL) {
			JS_DefineProperty(cx, type, "NONE", INT_TO_JSVAL(CRYPT_CERTTYPE_NONE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, type, "CERTIFICATE", INT_TO_JSVAL(CRYPT_CERTTYPE_CERTIFICATE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, type, "ATTRIBUTE_CERT", INT_TO_JSVAL(CRYPT_CERTTYPE_ATTRIBUTE_CERT), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, type, "CERTCHAIN", INT_TO_JSVAL(CRYPT_CERTTYPE_CERTCHAIN), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, type, "CERTREQUEST", INT_TO_JSVAL(CRYPT_CERTTYPE_CERTREQUEST), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, type, "REQUEST_CERT", INT_TO_JSVAL(CRYPT_CERTTYPE_REQUEST_CERT), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, type, "REQUEST_REVOCATION", INT_TO_JSVAL(CRYPT_CERTTYPE_REQUEST_REVOCATION), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, type, "CRL", INT_TO_JSVAL(CRYPT_CERTTYPE_CRL), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, type, "CMS_ATTRIBUTES", INT_TO_JSVAL(CRYPT_CERTTYPE_CMS_ATTRIBUTES), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, type, "RTCS_REQUEST", INT_TO_JSVAL(CRYPT_CERTTYPE_RTCS_REQUEST), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, type, "RTCS_RESPONSE", INT_TO_JSVAL(CRYPT_CERTTYPE_RTCS_RESPONSE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, type, "OCSP_REQUEST", INT_TO_JSVAL(CRYPT_CERTTYPE_OCSP_REQUEST), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, type, "OCSP_RESPONSE", INT_TO_JSVAL(CRYPT_CERTTYPE_OCSP_RESPONSE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, type, "PKIUSER", INT_TO_JSVAL(CRYPT_CERTTYPE_PKIUSER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DeepFreezeObject(cx, type);
		}
		format = JS_DefineObject(cx, constructor, "FORMAT", NULL, NULL, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
		if(format != NULL) {
			JS_DefineProperty(cx, format, "NONE", INT_TO_JSVAL(CRYPT_CERTFORMAT_NONE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, format, "CERTIFICATE", INT_TO_JSVAL(CRYPT_CERTFORMAT_CERTIFICATE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, format, "CERTCHAIN", INT_TO_JSVAL(CRYPT_CERTFORMAT_CERTCHAIN), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, format, "TEXT_CERTIFICATE", INT_TO_JSVAL(CRYPT_CERTFORMAT_TEXT_CERTIFICATE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, format, "TEXT_CERTCHAIN", INT_TO_JSVAL(CRYPT_CERTFORMAT_TEXT_CERTCHAIN), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, format, "XML_CERTIFICATE", INT_TO_JSVAL(CRYPT_CERTFORMAT_XML_CERTIFICATE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, format, "XML_CERTCHAIN", INT_TO_JSVAL(CRYPT_CERTFORMAT_XML_CERTCHAIN), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DeepFreezeObject(cx, format);
		}
		attr = JS_DefineObject(cx, constructor, "ATTR", NULL, NULL, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
		if(attr != NULL) {
			JS_DefineProperty(cx, attr, "SELFSIGNED", INT_TO_JSVAL(CRYPT_CERTINFO_SELFSIGNED), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "IMMUTABLE", INT_TO_JSVAL(CRYPT_CERTINFO_IMMUTABLE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "XYZZY", INT_TO_JSVAL(CRYPT_CERTINFO_XYZZY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CERTTYPE", INT_TO_JSVAL(CRYPT_CERTINFO_CERTTYPE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "FINGERPRINT_SHA1", INT_TO_JSVAL(CRYPT_CERTINFO_FINGERPRINT_SHA1), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "FINGERPRINT_SHA2", INT_TO_JSVAL(CRYPT_CERTINFO_FINGERPRINT_SHA2), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "FINGERPRINT_SHAng", INT_TO_JSVAL(CRYPT_CERTINFO_FINGERPRINT_SHAng), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CURRENT_CERTIFICATE", INT_TO_JSVAL(CRYPT_CERTINFO_CURRENT_CERTIFICATE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "TRUSTED_USAGE", INT_TO_JSVAL(CRYPT_CERTINFO_TRUSTED_USAGE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "TRUSTED_IMPLICIT", INT_TO_JSVAL(CRYPT_CERTINFO_TRUSTED_IMPLICIT), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGNATURELEVEL", INT_TO_JSVAL(CRYPT_CERTINFO_SIGNATURELEVEL), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "VERSION", INT_TO_JSVAL(CRYPT_CERTINFO_VERSION), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SERIALNUMBER", INT_TO_JSVAL(CRYPT_CERTINFO_SERIALNUMBER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SUBJECTPUBLICKEYINFO", INT_TO_JSVAL(CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CERTIFICATE", INT_TO_JSVAL(CRYPT_CERTINFO_CERTIFICATE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CACERTIFICATE", INT_TO_JSVAL(CRYPT_CERTINFO_CACERTIFICATE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "ISSUERNAME", INT_TO_JSVAL(CRYPT_CERTINFO_ISSUERNAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "VALIDFROM", INT_TO_JSVAL(CRYPT_CERTINFO_VALIDFROM), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "VALIDTO", INT_TO_JSVAL(CRYPT_CERTINFO_VALIDTO), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SUBJECTNAME", INT_TO_JSVAL(CRYPT_CERTINFO_SUBJECTNAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "ISSUERUNIQUEID", INT_TO_JSVAL(CRYPT_CERTINFO_ISSUERUNIQUEID), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SUBJECTUNIQUEID", INT_TO_JSVAL(CRYPT_CERTINFO_SUBJECTUNIQUEID), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CERTREQUEST", INT_TO_JSVAL(CRYPT_CERTINFO_CERTREQUEST), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "THISUPDATE", INT_TO_JSVAL(CRYPT_CERTINFO_THISUPDATE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "NEXTUPDATE", INT_TO_JSVAL(CRYPT_CERTINFO_NEXTUPDATE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "REVOCATIONDATE", INT_TO_JSVAL(CRYPT_CERTINFO_REVOCATIONDATE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "REVOCATIONSTATUS", INT_TO_JSVAL(CRYPT_CERTINFO_REVOCATIONSTATUS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CERTSTATUS", INT_TO_JSVAL(CRYPT_CERTINFO_CERTSTATUS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "DN", INT_TO_JSVAL(CRYPT_CERTINFO_DN), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "PKIUSER_ID", INT_TO_JSVAL(CRYPT_CERTINFO_PKIUSER_ID), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "PKIUSER_ISSUEPASSWORD", INT_TO_JSVAL(CRYPT_CERTINFO_PKIUSER_ISSUEPASSWORD), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "PKIUSER_REVPASSWORD", INT_TO_JSVAL(CRYPT_CERTINFO_PKIUSER_REVPASSWORD), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "PKIUSER_RA", INT_TO_JSVAL(CRYPT_CERTINFO_PKIUSER_RA), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "COUNTRYNAME", INT_TO_JSVAL(CRYPT_CERTINFO_COUNTRYNAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "STATEORPROVINCENAME", INT_TO_JSVAL(CRYPT_CERTINFO_STATEORPROVINCENAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "LOCALITYNAME", INT_TO_JSVAL(CRYPT_CERTINFO_LOCALITYNAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "ORGANIZATIONNAME", INT_TO_JSVAL(CRYPT_CERTINFO_ORGANIZATIONNAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "ORGANIZATIONALUNITNAME", INT_TO_JSVAL(CRYPT_CERTINFO_ORGANIZATIONALUNITNAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "COMMONNAME", INT_TO_JSVAL(CRYPT_CERTINFO_COMMONNAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "OTHERNAME_TYPEID", INT_TO_JSVAL(CRYPT_CERTINFO_OTHERNAME_TYPEID), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "OTHERNAME_VALUE", INT_TO_JSVAL(CRYPT_CERTINFO_OTHERNAME_VALUE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "RFC822NAME", INT_TO_JSVAL(CRYPT_CERTINFO_RFC822NAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "DNSNAME", INT_TO_JSVAL(CRYPT_CERTINFO_DNSNAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "DIRECTORYNAME", INT_TO_JSVAL(CRYPT_CERTINFO_DIRECTORYNAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EDIPARTYNAME_NAMEASSIGNER", INT_TO_JSVAL(CRYPT_CERTINFO_EDIPARTYNAME_NAMEASSIGNER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EDIPARTYNAME_PARTYNAME", INT_TO_JSVAL(CRYPT_CERTINFO_EDIPARTYNAME_PARTYNAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "UNIFORMRESOURCEIDENTIFIER", INT_TO_JSVAL(CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "IPADDRESS", INT_TO_JSVAL(CRYPT_CERTINFO_IPADDRESS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "REGISTEREDID", INT_TO_JSVAL(CRYPT_CERTINFO_REGISTEREDID), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CHALLENGEPASSWORD", INT_TO_JSVAL(CRYPT_CERTINFO_CHALLENGEPASSWORD), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CRLEXTREASON", INT_TO_JSVAL(CRYPT_CERTINFO_CRLEXTREASON), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "KEYFEATURES", INT_TO_JSVAL(CRYPT_CERTINFO_KEYFEATURES), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AUTHORITYINFOACCESS", INT_TO_JSVAL(CRYPT_CERTINFO_AUTHORITYINFOACCESS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AUTHORITYINFO_RTCS", INT_TO_JSVAL(CRYPT_CERTINFO_AUTHORITYINFO_RTCS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AUTHORITYINFO_OCSP", INT_TO_JSVAL(CRYPT_CERTINFO_AUTHORITYINFO_OCSP), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AUTHORITYINFO_CAISSUERS", INT_TO_JSVAL(CRYPT_CERTINFO_AUTHORITYINFO_CAISSUERS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AUTHORITYINFO_CERTSTORE", INT_TO_JSVAL(CRYPT_CERTINFO_AUTHORITYINFO_CERTSTORE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AUTHORITYINFO_CRLS", INT_TO_JSVAL(CRYPT_CERTINFO_AUTHORITYINFO_CRLS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "BIOMETRICINFO", INT_TO_JSVAL(CRYPT_CERTINFO_BIOMETRICINFO), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "BIOMETRICINFO_TYPE", INT_TO_JSVAL(CRYPT_CERTINFO_BIOMETRICINFO_TYPE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "BIOMETRICINFO_HASHALGO", INT_TO_JSVAL(CRYPT_CERTINFO_BIOMETRICINFO_HASHALGO), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "BIOMETRICINFO_HASH", INT_TO_JSVAL(CRYPT_CERTINFO_BIOMETRICINFO_HASH), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "BIOMETRICINFO_URL", INT_TO_JSVAL(CRYPT_CERTINFO_BIOMETRICINFO_URL), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "QCSTATEMENT", INT_TO_JSVAL(CRYPT_CERTINFO_QCSTATEMENT), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "QCSTATEMENT_SEMANTICS", INT_TO_JSVAL(CRYPT_CERTINFO_QCSTATEMENT_SEMANTICS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "QCSTATEMENT_REGISTRATIONAUTHORITY", INT_TO_JSVAL(CRYPT_CERTINFO_QCSTATEMENT_REGISTRATIONAUTHORITY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "IPADDRESSBLOCKS", INT_TO_JSVAL(CRYPT_CERTINFO_IPADDRESSBLOCKS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "IPADDRESSBLOCKS_ADDRESSFAMILY", INT_TO_JSVAL(CRYPT_CERTINFO_IPADDRESSBLOCKS_ADDRESSFAMILY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "IPADDRESSBLOCKS_PREFIX", INT_TO_JSVAL(CRYPT_CERTINFO_IPADDRESSBLOCKS_PREFIX), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "IPADDRESSBLOCKS_MIN", INT_TO_JSVAL(CRYPT_CERTINFO_IPADDRESSBLOCKS_MIN), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "IPADDRESSBLOCKS_MAX", INT_TO_JSVAL(CRYPT_CERTINFO_IPADDRESSBLOCKS_MAX), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AUTONOMOUSSYSIDS", INT_TO_JSVAL(CRYPT_CERTINFO_AUTONOMOUSSYSIDS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AUTONOMOUSSYSIDS_ASNUM_ID", INT_TO_JSVAL(CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_ID), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AUTONOMOUSSYSIDS_ASNUM_MIN", INT_TO_JSVAL(CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_MIN), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AUTONOMOUSSYSIDS_ASNUM_MAX", INT_TO_JSVAL(CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_MAX), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "OCSP_NONCE", INT_TO_JSVAL(CRYPT_CERTINFO_OCSP_NONCE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "OCSP_RESPONSE", INT_TO_JSVAL(CRYPT_CERTINFO_OCSP_RESPONSE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "OCSP_RESPONSE_OCSP", INT_TO_JSVAL(CRYPT_CERTINFO_OCSP_RESPONSE_OCSP), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "OCSP_NOCHECK", INT_TO_JSVAL(CRYPT_CERTINFO_OCSP_NOCHECK), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "OCSP_ARCHIVECUTOFF", INT_TO_JSVAL(CRYPT_CERTINFO_OCSP_ARCHIVECUTOFF), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SUBJECTINFOACCESS", INT_TO_JSVAL(CRYPT_CERTINFO_SUBJECTINFOACCESS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SUBJECTINFO_TIMESTAMPING", INT_TO_JSVAL(CRYPT_CERTINFO_SUBJECTINFO_TIMESTAMPING), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SUBJECTINFO_CAREPOSITORY", INT_TO_JSVAL(CRYPT_CERTINFO_SUBJECTINFO_CAREPOSITORY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SUBJECTINFO_SIGNEDOBJECTREPOSITORY", INT_TO_JSVAL(CRYPT_CERTINFO_SUBJECTINFO_SIGNEDOBJECTREPOSITORY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SUBJECTINFO_RPKIMANIFEST", INT_TO_JSVAL(CRYPT_CERTINFO_SUBJECTINFO_RPKIMANIFEST), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SUBJECTINFO_SIGNEDOBJECT", INT_TO_JSVAL(CRYPT_CERTINFO_SUBJECTINFO_SIGNEDOBJECT), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_DATEOFCERTGEN", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_DATEOFCERTGEN), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_PROCURATION", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_PROCURATION), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_PROCURE_COUNTRY", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_PROCURE_COUNTRY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_PROCURE_TYPEOFSUBSTITUTION", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_PROCURE_TYPEOFSUBSTITUTION), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_PROCURE_SIGNINGFOR", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_PROCURE_SIGNINGFOR), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_ADMISSIONS", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_ADMISSIONS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_ADMISSIONS_AUTHORITY", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_ADMISSIONS_AUTHORITY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_ADMISSIONS_NAMINGAUTHID", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHID), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_ADMISSIONS_NAMINGAUTHURL", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHURL), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_ADMISSIONS_NAMINGAUTHTEXT", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHTEXT), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_ADMISSIONS_PROFESSIONITEM", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_ADMISSIONS_PROFESSIONITEM), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_ADMISSIONS_PROFESSIONOID", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_ADMISSIONS_PROFESSIONOID), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_ADMISSIONS_REGISTRATIONNUMBER", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_ADMISSIONS_REGISTRATIONNUMBER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_MONETARYLIMIT", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_MONETARYLIMIT), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_MONETARY_CURRENCY", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_MONETARY_CURRENCY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_MONETARY_AMOUNT", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_MONETARY_AMOUNT), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_MONETARY_EXPONENT", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_MONETARY_EXPONENT), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_DECLARATIONOFMAJORITY", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_DECLARATIONOFMAJORITY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_DECLARATIONOFMAJORITY_COUNTRY", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_DECLARATIONOFMAJORITY_COUNTRY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_RESTRICTION", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_RESTRICTION), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_CERTHASH", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_CERTHASH), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SIGG_ADDITIONALINFORMATION", INT_TO_JSVAL(CRYPT_CERTINFO_SIGG_ADDITIONALINFORMATION), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "STRONGEXTRANET", INT_TO_JSVAL(CRYPT_CERTINFO_STRONGEXTRANET), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "STRONGEXTRANET_ZONE", INT_TO_JSVAL(CRYPT_CERTINFO_STRONGEXTRANET_ZONE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "STRONGEXTRANET_ID", INT_TO_JSVAL(CRYPT_CERTINFO_STRONGEXTRANET_ID), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SUBJECTDIRECTORYATTRIBUTES", INT_TO_JSVAL(CRYPT_CERTINFO_SUBJECTDIRECTORYATTRIBUTES), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SUBJECTDIR_TYPE", INT_TO_JSVAL(CRYPT_CERTINFO_SUBJECTDIR_TYPE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SUBJECTDIR_VALUES", INT_TO_JSVAL(CRYPT_CERTINFO_SUBJECTDIR_VALUES), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SUBJECTKEYIDENTIFIER", INT_TO_JSVAL(CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "KEYUSAGE", INT_TO_JSVAL(CRYPT_CERTINFO_KEYUSAGE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "PRIVATEKEYUSAGEPERIOD", INT_TO_JSVAL(CRYPT_CERTINFO_PRIVATEKEYUSAGEPERIOD), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "PRIVATEKEY_NOTBEFORE", INT_TO_JSVAL(CRYPT_CERTINFO_PRIVATEKEY_NOTBEFORE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "PRIVATEKEY_NOTAFTER", INT_TO_JSVAL(CRYPT_CERTINFO_PRIVATEKEY_NOTAFTER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SUBJECTALTNAME", INT_TO_JSVAL(CRYPT_CERTINFO_SUBJECTALTNAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "ISSUERALTNAME", INT_TO_JSVAL(CRYPT_CERTINFO_ISSUERALTNAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "BASICCONSTRAINTS", INT_TO_JSVAL(CRYPT_CERTINFO_BASICCONSTRAINTS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CA", INT_TO_JSVAL(CRYPT_CERTINFO_CA), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "PATHLENCONSTRAINT", INT_TO_JSVAL(CRYPT_CERTINFO_PATHLENCONSTRAINT), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CRLNUMBER", INT_TO_JSVAL(CRYPT_CERTINFO_CRLNUMBER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CRLREASON", INT_TO_JSVAL(CRYPT_CERTINFO_CRLREASON), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "HOLDINSTRUCTIONCODE", INT_TO_JSVAL(CRYPT_CERTINFO_HOLDINSTRUCTIONCODE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "INVALIDITYDATE", INT_TO_JSVAL(CRYPT_CERTINFO_INVALIDITYDATE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "DELTACRLINDICATOR", INT_TO_JSVAL(CRYPT_CERTINFO_DELTACRLINDICATOR), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "ISSUINGDISTRIBUTIONPOINT", INT_TO_JSVAL(CRYPT_CERTINFO_ISSUINGDISTRIBUTIONPOINT), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "ISSUINGDIST_FULLNAME", INT_TO_JSVAL(CRYPT_CERTINFO_ISSUINGDIST_FULLNAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "ISSUINGDIST_USERCERTSONLY", INT_TO_JSVAL(CRYPT_CERTINFO_ISSUINGDIST_USERCERTSONLY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "ISSUINGDIST_CACERTSONLY", INT_TO_JSVAL(CRYPT_CERTINFO_ISSUINGDIST_CACERTSONLY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "ISSUINGDIST_SOMEREASONSONLY", INT_TO_JSVAL(CRYPT_CERTINFO_ISSUINGDIST_SOMEREASONSONLY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "ISSUINGDIST_INDIRECTCRL", INT_TO_JSVAL(CRYPT_CERTINFO_ISSUINGDIST_INDIRECTCRL), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CERTIFICATEISSUER", INT_TO_JSVAL(CRYPT_CERTINFO_CERTIFICATEISSUER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "NAMECONSTRAINTS", INT_TO_JSVAL(CRYPT_CERTINFO_NAMECONSTRAINTS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "PERMITTEDSUBTREES", INT_TO_JSVAL(CRYPT_CERTINFO_PERMITTEDSUBTREES), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXCLUDEDSUBTREES", INT_TO_JSVAL(CRYPT_CERTINFO_EXCLUDEDSUBTREES), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CRLDISTRIBUTIONPOINT", INT_TO_JSVAL(CRYPT_CERTINFO_CRLDISTRIBUTIONPOINT), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CRLDIST_FULLNAME", INT_TO_JSVAL(CRYPT_CERTINFO_CRLDIST_FULLNAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CRLDIST_REASONS", INT_TO_JSVAL(CRYPT_CERTINFO_CRLDIST_REASONS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CRLDIST_CRLISSUER", INT_TO_JSVAL(CRYPT_CERTINFO_CRLDIST_CRLISSUER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CERTIFICATEPOLICIES", INT_TO_JSVAL(CRYPT_CERTINFO_CERTIFICATEPOLICIES), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CERTPOLICYID", INT_TO_JSVAL(CRYPT_CERTINFO_CERTPOLICYID), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CERTPOLICY_CPSURI", INT_TO_JSVAL(CRYPT_CERTINFO_CERTPOLICY_CPSURI), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CERTPOLICY_ORGANIZATION", INT_TO_JSVAL(CRYPT_CERTINFO_CERTPOLICY_ORGANIZATION), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CERTPOLICY_NOTICENUMBERS", INT_TO_JSVAL(CRYPT_CERTINFO_CERTPOLICY_NOTICENUMBERS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CERTPOLICY_EXPLICITTEXT", INT_TO_JSVAL(CRYPT_CERTINFO_CERTPOLICY_EXPLICITTEXT), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "POLICYMAPPINGS", INT_TO_JSVAL(CRYPT_CERTINFO_POLICYMAPPINGS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "ISSUERDOMAINPOLICY", INT_TO_JSVAL(CRYPT_CERTINFO_ISSUERDOMAINPOLICY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SUBJECTDOMAINPOLICY", INT_TO_JSVAL(CRYPT_CERTINFO_SUBJECTDOMAINPOLICY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AUTHORITYKEYIDENTIFIER", INT_TO_JSVAL(CRYPT_CERTINFO_AUTHORITYKEYIDENTIFIER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AUTHORITY_KEYIDENTIFIER", INT_TO_JSVAL(CRYPT_CERTINFO_AUTHORITY_KEYIDENTIFIER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AUTHORITY_CERTISSUER", INT_TO_JSVAL(CRYPT_CERTINFO_AUTHORITY_CERTISSUER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AUTHORITY_CERTSERIALNUMBER", INT_TO_JSVAL(CRYPT_CERTINFO_AUTHORITY_CERTSERIALNUMBER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "POLICYCONSTRAINTS", INT_TO_JSVAL(CRYPT_CERTINFO_POLICYCONSTRAINTS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "REQUIREEXPLICITPOLICY", INT_TO_JSVAL(CRYPT_CERTINFO_REQUIREEXPLICITPOLICY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "INHIBITPOLICYMAPPING", INT_TO_JSVAL(CRYPT_CERTINFO_INHIBITPOLICYMAPPING), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXTKEYUSAGE", INT_TO_JSVAL(CRYPT_CERTINFO_EXTKEYUSAGE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXTKEY_MS_INDIVIDUALCODESIGNING", INT_TO_JSVAL(CRYPT_CERTINFO_EXTKEY_MS_INDIVIDUALCODESIGNING), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXTKEY_MS_COMMERCIALCODESIGNING", INT_TO_JSVAL(CRYPT_CERTINFO_EXTKEY_MS_COMMERCIALCODESIGNING), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXTKEY_MS_CERTTRUSTLISTSIGNING", INT_TO_JSVAL(CRYPT_CERTINFO_EXTKEY_MS_CERTTRUSTLISTSIGNING), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXTKEY_MS_TIMESTAMPSIGNING", INT_TO_JSVAL(CRYPT_CERTINFO_EXTKEY_MS_TIMESTAMPSIGNING), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXTKEY_MS_SERVERGATEDCRYPTO", INT_TO_JSVAL(CRYPT_CERTINFO_EXTKEY_MS_SERVERGATEDCRYPTO), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXTKEY_MS_ENCRYPTEDFILESYSTEM", INT_TO_JSVAL(CRYPT_CERTINFO_EXTKEY_MS_ENCRYPTEDFILESYSTEM), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXTKEY_SERVERAUTH", INT_TO_JSVAL(CRYPT_CERTINFO_EXTKEY_SERVERAUTH), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXTKEY_CLIENTAUTH", INT_TO_JSVAL(CRYPT_CERTINFO_EXTKEY_CLIENTAUTH), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXTKEY_CODESIGNING", INT_TO_JSVAL(CRYPT_CERTINFO_EXTKEY_CODESIGNING), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXTKEY_EMAILPROTECTION", INT_TO_JSVAL(CRYPT_CERTINFO_EXTKEY_EMAILPROTECTION), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXTKEY_IPSECENDSYSTEM", INT_TO_JSVAL(CRYPT_CERTINFO_EXTKEY_IPSECENDSYSTEM), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXTKEY_IPSECTUNNEL", INT_TO_JSVAL(CRYPT_CERTINFO_EXTKEY_IPSECTUNNEL), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXTKEY_IPSECUSER", INT_TO_JSVAL(CRYPT_CERTINFO_EXTKEY_IPSECUSER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXTKEY_TIMESTAMPING", INT_TO_JSVAL(CRYPT_CERTINFO_EXTKEY_TIMESTAMPING), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXTKEY_OCSPSIGNING", INT_TO_JSVAL(CRYPT_CERTINFO_EXTKEY_OCSPSIGNING), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXTKEY_DIRECTORYSERVICE", INT_TO_JSVAL(CRYPT_CERTINFO_EXTKEY_DIRECTORYSERVICE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXTKEY_ANYKEYUSAGE", INT_TO_JSVAL(CRYPT_CERTINFO_EXTKEY_ANYKEYUSAGE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXTKEY_NS_SERVERGATEDCRYPTO", INT_TO_JSVAL(CRYPT_CERTINFO_EXTKEY_NS_SERVERGATEDCRYPTO), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXTKEY_VS_SERVERGATEDCRYPTO_CA", INT_TO_JSVAL(CRYPT_CERTINFO_EXTKEY_VS_SERVERGATEDCRYPTO_CA), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CRLSTREAMIDENTIFIER", INT_TO_JSVAL(CRYPT_CERTINFO_CRLSTREAMIDENTIFIER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "FRESHESTCRL", INT_TO_JSVAL(CRYPT_CERTINFO_FRESHESTCRL), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "FRESHESTCRL_FULLNAME", INT_TO_JSVAL(CRYPT_CERTINFO_FRESHESTCRL_FULLNAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "FRESHESTCRL_REASONS", INT_TO_JSVAL(CRYPT_CERTINFO_FRESHESTCRL_REASONS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "FRESHESTCRL_CRLISSUER", INT_TO_JSVAL(CRYPT_CERTINFO_FRESHESTCRL_CRLISSUER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "ORDEREDLIST", INT_TO_JSVAL(CRYPT_CERTINFO_ORDEREDLIST), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "BASEUPDATETIME", INT_TO_JSVAL(CRYPT_CERTINFO_BASEUPDATETIME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "DELTAINFO", INT_TO_JSVAL(CRYPT_CERTINFO_DELTAINFO), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "DELTAINFO_LOCATION", INT_TO_JSVAL(CRYPT_CERTINFO_DELTAINFO_LOCATION), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "DELTAINFO_NEXTDELTA", INT_TO_JSVAL(CRYPT_CERTINFO_DELTAINFO_NEXTDELTA), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "INHIBITANYPOLICY", INT_TO_JSVAL(CRYPT_CERTINFO_INHIBITANYPOLICY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "TOBEREVOKED", INT_TO_JSVAL(CRYPT_CERTINFO_TOBEREVOKED), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "TOBEREVOKED_CERTISSUER", INT_TO_JSVAL(CRYPT_CERTINFO_TOBEREVOKED_CERTISSUER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "TOBEREVOKED_REASONCODE", INT_TO_JSVAL(CRYPT_CERTINFO_TOBEREVOKED_REASONCODE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "TOBEREVOKED_REVOCATIONTIME", INT_TO_JSVAL(CRYPT_CERTINFO_TOBEREVOKED_REVOCATIONTIME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "TOBEREVOKED_CERTSERIALNUMBER", INT_TO_JSVAL(CRYPT_CERTINFO_TOBEREVOKED_CERTSERIALNUMBER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "REVOKEDGROUPS", INT_TO_JSVAL(CRYPT_CERTINFO_REVOKEDGROUPS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "REVOKEDGROUPS_CERTISSUER", INT_TO_JSVAL(CRYPT_CERTINFO_REVOKEDGROUPS_CERTISSUER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "REVOKEDGROUPS_REASONCODE", INT_TO_JSVAL(CRYPT_CERTINFO_REVOKEDGROUPS_REASONCODE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "REVOKEDGROUPS_INVALIDITYDATE", INT_TO_JSVAL(CRYPT_CERTINFO_REVOKEDGROUPS_INVALIDITYDATE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "REVOKEDGROUPS_STARTINGNUMBER", INT_TO_JSVAL(CRYPT_CERTINFO_REVOKEDGROUPS_STARTINGNUMBER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "REVOKEDGROUPS_ENDINGNUMBER", INT_TO_JSVAL(CRYPT_CERTINFO_REVOKEDGROUPS_ENDINGNUMBER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "EXPIREDCERTSONCRL", INT_TO_JSVAL(CRYPT_CERTINFO_EXPIREDCERTSONCRL), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AAISSUINGDISTRIBUTIONPOINT", INT_TO_JSVAL(CRYPT_CERTINFO_AAISSUINGDISTRIBUTIONPOINT), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AAISSUINGDIST_FULLNAME", INT_TO_JSVAL(CRYPT_CERTINFO_AAISSUINGDIST_FULLNAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AAISSUINGDIST_SOMEREASONSONLY", INT_TO_JSVAL(CRYPT_CERTINFO_AAISSUINGDIST_SOMEREASONSONLY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AAISSUINGDIST_INDIRECTCRL", INT_TO_JSVAL(CRYPT_CERTINFO_AAISSUINGDIST_INDIRECTCRL), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AAISSUINGDIST_USERATTRCERTS", INT_TO_JSVAL(CRYPT_CERTINFO_AAISSUINGDIST_USERATTRCERTS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AAISSUINGDIST_AACERTS", INT_TO_JSVAL(CRYPT_CERTINFO_AAISSUINGDIST_AACERTS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "AAISSUINGDIST_SOACERTS", INT_TO_JSVAL(CRYPT_CERTINFO_AAISSUINGDIST_SOACERTS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "NS_CERTTYPE", INT_TO_JSVAL(CRYPT_CERTINFO_NS_CERTTYPE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "NS_BASEURL", INT_TO_JSVAL(CRYPT_CERTINFO_NS_BASEURL), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "NS_REVOCATIONURL", INT_TO_JSVAL(CRYPT_CERTINFO_NS_REVOCATIONURL), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "NS_CAREVOCATIONURL", INT_TO_JSVAL(CRYPT_CERTINFO_NS_CAREVOCATIONURL), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "NS_CERTRENEWALURL", INT_TO_JSVAL(CRYPT_CERTINFO_NS_CERTRENEWALURL), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "NS_CAPOLICYURL", INT_TO_JSVAL(CRYPT_CERTINFO_NS_CAPOLICYURL), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "NS_SSLSERVERNAME", INT_TO_JSVAL(CRYPT_CERTINFO_NS_SSLSERVERNAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "NS_COMMENT", INT_TO_JSVAL(CRYPT_CERTINFO_NS_COMMENT), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SET_HASHEDROOTKEY", INT_TO_JSVAL(CRYPT_CERTINFO_SET_HASHEDROOTKEY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SET_ROOTKEYTHUMBPRINT", INT_TO_JSVAL(CRYPT_CERTINFO_SET_ROOTKEYTHUMBPRINT), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SET_CERTIFICATETYPE", INT_TO_JSVAL(CRYPT_CERTINFO_SET_CERTIFICATETYPE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SET_MERCHANTDATA", INT_TO_JSVAL(CRYPT_CERTINFO_SET_MERCHANTDATA), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SET_MERID", INT_TO_JSVAL(CRYPT_CERTINFO_SET_MERID), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SET_MERACQUIRERBIN", INT_TO_JSVAL(CRYPT_CERTINFO_SET_MERACQUIRERBIN), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SET_MERCHANTLANGUAGE", INT_TO_JSVAL(CRYPT_CERTINFO_SET_MERCHANTLANGUAGE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SET_MERCHANTNAME", INT_TO_JSVAL(CRYPT_CERTINFO_SET_MERCHANTNAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SET_MERCHANTCITY", INT_TO_JSVAL(CRYPT_CERTINFO_SET_MERCHANTCITY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SET_MERCHANTSTATEPROVINCE", INT_TO_JSVAL(CRYPT_CERTINFO_SET_MERCHANTSTATEPROVINCE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SET_MERCHANTPOSTALCODE", INT_TO_JSVAL(CRYPT_CERTINFO_SET_MERCHANTPOSTALCODE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SET_MERCHANTCOUNTRYNAME", INT_TO_JSVAL(CRYPT_CERTINFO_SET_MERCHANTCOUNTRYNAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SET_MERCOUNTRY", INT_TO_JSVAL(CRYPT_CERTINFO_SET_MERCOUNTRY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SET_MERAUTHFLAG", INT_TO_JSVAL(CRYPT_CERTINFO_SET_MERAUTHFLAG), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SET_CERTCARDREQUIRED", INT_TO_JSVAL(CRYPT_CERTINFO_SET_CERTCARDREQUIRED), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SET_TUNNELING", INT_TO_JSVAL(CRYPT_CERTINFO_SET_TUNNELING), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SET_TUNNELINGFLAG", INT_TO_JSVAL(CRYPT_CERTINFO_SET_TUNNELINGFLAG), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SET_TUNNELINGALGID", INT_TO_JSVAL(CRYPT_CERTINFO_SET_TUNNELINGALGID), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_CONTENTTYPE", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_CONTENTTYPE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_MESSAGEDIGEST", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_MESSAGEDIGEST), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SIGNINGTIME", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SIGNINGTIME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_COUNTERSIGNATURE", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_COUNTERSIGNATURE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SIGNINGDESCRIPTION", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SIGNINGDESCRIPTION), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAPABILITIES", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAPABILITIES), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_3DES", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_3DES), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_AES", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_AES), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_CAST128", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_CAST128), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_SHAng", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_SHAng), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_SHA2", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_SHA2), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_SHA1", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_SHA1), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_HMAC_SHAng", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHAng), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_HMAC_SHA2", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHA2), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_HMAC_SHA1", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHA1), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_AUTHENC256", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_AUTHENC256), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_AUTHENC128", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_AUTHENC128), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_RSA_SHAng", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHAng), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_RSA_SHA2", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHA2), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_RSA_SHA1", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHA1), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_DSA_SHA1", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_DSA_SHA1), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_ECDSA_SHAng", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHAng), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_ECDSA_SHA2", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHA2), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_ECDSA_SHA1", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHA1), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_PREFERSIGNEDDATA", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_PREFERSIGNEDDATA), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_CANNOTDECRYPTANY", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_CANNOTDECRYPTANY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SMIMECAP_PREFERBINARYINSIDE", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SMIMECAP_PREFERBINARYINSIDE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_RECEIPTREQUEST", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_RECEIPTREQUEST), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_RECEIPT_CONTENTIDENTIFIER", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_RECEIPT_CONTENTIDENTIFIER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_RECEIPT_FROM", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_RECEIPT_FROM), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_RECEIPT_TO", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_RECEIPT_TO), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SECURITYLABEL", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SECURITYLABEL), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SECLABEL_POLICY", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SECLABEL_POLICY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SECLABEL_CLASSIFICATION", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SECLABEL_CLASSIFICATION), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SECLABEL_PRIVACYMARK", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SECLABEL_PRIVACYMARK), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SECLABEL_CATTYPE", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SECLABEL_CATTYPE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SECLABEL_CATVALUE", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SECLABEL_CATVALUE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_MLEXPANSIONHISTORY", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_MLEXPANSIONHISTORY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_MLEXP_ENTITYIDENTIFIER", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_MLEXP_ENTITYIDENTIFIER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_MLEXP_TIME", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_MLEXP_TIME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_MLEXP_NONE", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_MLEXP_NONE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_MLEXP_INSTEADOF", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_MLEXP_INSTEADOF), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_MLEXP_INADDITIONTO", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_MLEXP_INADDITIONTO), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_CONTENTHINTS", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_CONTENTHINTS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_CONTENTHINT_DESCRIPTION", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_CONTENTHINT_DESCRIPTION), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_CONTENTHINT_TYPE", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_CONTENTHINT_TYPE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_EQUIVALENTLABEL", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_EQUIVALENTLABEL), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_EQVLABEL_POLICY", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_EQVLABEL_POLICY), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_EQVLABEL_CLASSIFICATION", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_EQVLABEL_CLASSIFICATION), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_EQVLABEL_PRIVACYMARK", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_EQVLABEL_PRIVACYMARK), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_EQVLABEL_CATTYPE", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_EQVLABEL_CATTYPE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_EQVLABEL_CATVALUE", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_EQVLABEL_CATVALUE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SIGNINGCERTIFICATE", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SIGNINGCERTIFICATE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SIGNINGCERT_ESSCERTID", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SIGNINGCERT_ESSCERTID), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SIGNINGCERT_POLICIES", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SIGNINGCERT_POLICIES), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SIGNINGCERTIFICATEV2", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SIGNINGCERTIFICATEV2), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SIGNINGCERTV2_ESSCERTIDV2", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SIGNINGCERTV2_ESSCERTIDV2), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SIGNINGCERTV2_POLICIES", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SIGNINGCERTV2_POLICIES), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SIGNATUREPOLICYID", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SIGNATUREPOLICYID), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SIGPOLICYID", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SIGPOLICYID), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SIGPOLICYHASH", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SIGPOLICYHASH), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SIGPOLICY_CPSURI", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SIGPOLICY_CPSURI), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SIGPOLICY_ORGANIZATION", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SIGPOLICY_ORGANIZATION), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SIGPOLICY_NOTICENUMBERS", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SIGPOLICY_NOTICENUMBERS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SIGPOLICY_EXPLICITTEXT", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SIGPOLICY_EXPLICITTEXT), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SIGTYPEIDENTIFIER", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SIGTYPEIDENTIFIER), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SIGTYPEID_ORIGINATORSIG", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SIGTYPEID_ORIGINATORSIG), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SIGTYPEID_DOMAINSIG", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SIGTYPEID_DOMAINSIG), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SIGTYPEID_ADDITIONALATTRIBUTES", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SIGTYPEID_ADDITIONALATTRIBUTES), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SIGTYPEID_REVIEWSIG", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SIGTYPEID_REVIEWSIG), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_NONCE", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_NONCE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SCEP_MESSAGETYPE", INT_TO_JSVAL(CRYPT_CERTINFO_SCEP_MESSAGETYPE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SCEP_PKISTATUS", INT_TO_JSVAL(CRYPT_CERTINFO_SCEP_PKISTATUS), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SCEP_FAILINFO", INT_TO_JSVAL(CRYPT_CERTINFO_SCEP_FAILINFO), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SCEP_SENDERNONCE", INT_TO_JSVAL(CRYPT_CERTINFO_SCEP_SENDERNONCE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SCEP_RECIPIENTNONCE", INT_TO_JSVAL(CRYPT_CERTINFO_SCEP_RECIPIENTNONCE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "SCEP_TRANSACTIONID", INT_TO_JSVAL(CRYPT_CERTINFO_SCEP_TRANSACTIONID), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SPCAGENCYINFO", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SPCAGENCYINFO), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SPCAGENCYURL", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SPCAGENCYURL), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SPCSTATEMENTTYPE", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SPCSTATEMENTTYPE), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SPCSTMT_INDIVIDUALCODESIGNING", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SPCSTMT_INDIVIDUALCODESIGNING), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SPCSTMT_COMMERCIALCODESIGNING", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SPCSTMT_COMMERCIALCODESIGNING), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SPCOPUSINFO", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SPCOPUSINFO), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SPCOPUSINFO_NAME", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SPCOPUSINFO_NAME), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DefineProperty(cx, attr, "CMS_SPCOPUSINFO_URL", INT_TO_JSVAL(CRYPT_CERTINFO_CMS_SPCOPUSINFO_URL), NULL, NULL
				, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
			JS_DeepFreezeObject(cx, attr);
		}
	}

	return(cksobj);
}
