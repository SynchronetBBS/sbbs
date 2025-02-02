/* Synchronet JavaScript "MQTT" Class */

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

#if defined USE_MOSQUITTO
#include "sbbs.h"
#include "js_request.h"

typedef struct
{
	int retval;
	mqtt_handle_t handle;
	struct mqtt_cfg cfg;
	msg_queue_t q;

} private_t;

static void js_finalize_mqtt(JSContext* cx, JSObject* obj)
{
	private_t*                p;
	struct mosquitto_message* msg;

	if ((p = (private_t*)JS_GetPrivate(cx, obj)) == NULL)
		return;

	if (p->handle != NULL) {
		mosquitto_disconnect(p->handle);
		mosquitto_loop_stop(p->handle, /* force: */ false);
		mosquitto_destroy(p->handle);
	}
	while ((msg = msgQueueRead(&p->q, /* timeout: */ 0)) != NULL) {
		mosquitto_message_free_contents(msg);
		free(msg);
	}
	msgQueueFree(&p->q);
	free(p);

	JS_SetPrivate(cx, obj, NULL);
}

extern JSClass js_mqtt_class;

static JSBool js_disconnect(JSContext* cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	private_t* p;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_mqtt_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->handle == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	p->retval = mosquitto_disconnect(p->handle);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(p->retval == MOSQ_ERR_SUCCESS));
	JS_RESUMEREQUEST(cx, rc);
	return JS_TRUE;
}

static int pw_callback(char* buf, int size, int rwflag, void* userdata)
{
	private_t* p = (private_t*)userdata;

	strncpy(buf, p->cfg.tls.keypass, size);
	return strlen(p->cfg.tls.keypass);
}

static JSBool js_connect(JSContext* cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	jsval*     argv = JS_ARGV(cx, arglist);
	private_t* p;
	jsrefcount rc;
	char       broker_addr[sizeof p->cfg.broker_addr];
	uint16_t   broker_port;
	char       username[sizeof p->cfg.username];
	char       password[sizeof p->cfg.password];

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_mqtt_class)) == NULL)
		return JS_FALSE;
	scfg_t*    scfg = JS_GetRuntimePrivate(JS_GetRuntime(cx));

	rc = JS_SUSPENDREQUEST(cx);

	SAFECOPY(broker_addr, p->cfg.broker_addr);
	broker_port = p->cfg.broker_port;
	SAFECOPY(username, p->cfg.username);
	SAFECOPY(password, p->cfg.password);
	uintN argn = 0;
	if (argn < argc && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_STRBUF(cx, argv[argn], broker_addr, sizeof broker_addr, NULL);
		++argn;
	}
	if (argn < argc && JSVAL_IS_NUMBER(argv[argn])) {
		broker_port = JSVAL_TO_INT(argv[argn]);
		++argn;
	}
	if (argn < argc && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_STRBUF(cx, argv[argn], username, sizeof username, NULL);
		++argn;
	}
	if (argn < argc && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_STRBUF(cx, argv[argn], password, sizeof password, NULL);
		++argn;
	}

	if (p->cfg.tls.mode == MQTT_TLS_SBBS) {
		username[0] = 0;
		strlcpy(password, scfg->sys_pass, sizeof(password));
		mosquitto_int_option(p->handle, MOSQ_OPT_PROTOCOL_VERSION, 5);
	}
	else
		mosquitto_int_option(p->handle, MOSQ_OPT_PROTOCOL_VERSION, p->cfg.protocol_version);
	mosquitto_username_pw_set(p->handle, *username ? username : NULL, *password ? password : NULL);
	p->retval = MOSQ_ERR_SUCCESS;

	if (p->cfg.tls.mode == MQTT_TLS_CERT) {
		char* certfile = NULL;
		char* keyfile = NULL;
		if (p->cfg.tls.certfile[0] && p->cfg.tls.keyfile[0]) {
			certfile = p->cfg.tls.certfile;
			keyfile = p->cfg.tls.keyfile;
		}
		p->retval = mosquitto_tls_set(p->handle,
		                              p->cfg.tls.cafile,
		                              NULL, // capath
		                              certfile,
		                              keyfile,
		                              pw_callback);
	}
	else if (p->cfg.tls.mode == MQTT_TLS_PSK) {
		p->retval = mosquitto_tls_psk_set(p->handle,
		                                  p->cfg.tls.psk,
		                                  p->cfg.tls.identity,
		                                  NULL // ciphers (default)
		                                  );
	}
	else if (p->cfg.tls.mode == MQTT_TLS_SBBS) {
		user_t user = {
			.number = 1
		};
		if ((getuserdat(scfg, &user) == USER_SUCCESS)
		    && user.number == 1
		    && user_is_sysop(&user)) {
			char hexpass[LEN_PASS * 2 + 1];

			strlwr(user.pass);
			for (size_t i = 0; user.pass[i]; i++) {
				const char hd[] = "0123456789ABCDEF";
				hexpass[i*2] = hd[(((uint8_t *)user.pass)[i] & 0xf0) >> 4];
				hexpass[i*2+1] = hd[user.pass[i] & 0x0f];
			}
			strlwr(user.alias);
			p->retval = mosquitto_tls_psk_set(p->handle,
							   hexpass,
							   user.alias,
							   NULL // ciphers (default)
							   );
		}
		else
			p->retval = MQTT_FAILURE;
	}
	if (p->retval == MOSQ_ERR_SUCCESS)
		p->retval = mosquitto_connect_bind(p->handle,
		                                   broker_addr,
		                                   broker_port,
		                                   p->cfg.keepalive,
		                                   /* bind_address */ NULL);

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(p->retval == MOSQ_ERR_SUCCESS));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool js_publish(JSContext* cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	jsval*     argv = JS_ARGV(cx, arglist);
	char*      topic = NULL;
	char*      data = NULL;
	size_t     len = 0;
	private_t* p;
	jsrefcount rc;
	bool       retain = false;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_mqtt_class)) == NULL)
		return JS_FALSE;

	int qos = p->cfg.publish_qos;
	if (js_argcIsInsufficient(cx, argc, 2))
		return JS_FALSE;

	if (p->handle == NULL)
		return JS_TRUE;

	uintN argn = 0;
	if (argn < argc && JSVAL_IS_BOOLEAN(argv[argn])) {
		retain = JSVAL_TO_BOOLEAN(argv[argn]);
		++argn;
	}
	if (argn < argc && JSVAL_IS_NUMBER(argv[argn])) {
		qos = JSVAL_TO_INT(argv[argn]);
		++argn;
	}
	JSVALUE_TO_MSTRING(cx, argv[argn], topic, NULL);
	HANDLE_PENDING(cx, topic);
	++argn;

	JSVALUE_TO_MSTRING(cx, argv[argn], data, &len);
	HANDLE_PENDING(cx, data);
	++argn;

	rc = JS_SUSPENDREQUEST(cx);
	p->retval = mosquitto_publish_v5(p->handle,
	                                 /* mid: */ NULL,
	                                 /* topic: */ topic,
	                                 /* payloadlen */ len,
	                                 /* payload */ data,
	                                 qos,
	                                 retain,
	                                 /* properties */ NULL);

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(p->retval == MOSQ_ERR_SUCCESS));
	free(data);
	free(topic);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool js_subscribe(JSContext* cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	jsval*     argv = JS_ARGV(cx, arglist);
	char*      topic = NULL;
	private_t* p;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_mqtt_class)) == NULL)
		return JS_FALSE;

	int qos = p->cfg.subscribe_qos;
	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;

	if (p->handle == NULL)
		return JS_TRUE;

	uintN argn = 0;
	if (argn < argc && JSVAL_IS_NUMBER(argv[argn])) {
		qos = JSVAL_TO_INT(argv[argn]);
		++argn;
	}
	JSVALUE_TO_MSTRING(cx, argv[argn], topic, NULL);
	HANDLE_PENDING(cx, topic);
	++argn;

	rc = JS_SUSPENDREQUEST(cx);
	p->retval = mosquitto_subscribe(p->handle, /* msg-id: */ NULL, topic, qos);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(p->retval == MOSQ_ERR_SUCCESS));
	free(topic);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool js_read(JSContext* cx, uintN argc, jsval *arglist)
{
	JSObject*                 obj = JS_THIS_OBJECT(cx, arglist);
	jsval*                    argv = JS_ARGV(cx, arglist);
	private_t*                p;
	jsrefcount                rc;
	int                       timeout = 0;
	bool                      verbose = false;
	struct mosquitto_message* msg;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_mqtt_class)) == NULL)
		return JS_FALSE;

	if (p->handle == NULL)
		return JS_TRUE;

	uintN argn = 0;
	if (argn < argc && JSVAL_IS_NUMBER(argv[argn])) {
		timeout = JSVAL_TO_INT(argv[argn]);
		++argn;
	}
	if (argn < argc && JSVAL_IS_BOOLEAN(argv[argn])) {
		verbose = JSVAL_TO_BOOLEAN(argv[argn]);
		++argn;
	}

	rc = JS_SUSPENDREQUEST(cx);
	msg = msgQueueRead(&p->q, timeout);
	if (msg != NULL) {
		if (verbose) {
			JSString* str = JS_NewStringCopyZ(cx, msg->topic);
			obj = JS_NewObject(cx, NULL, NULL, obj);
			if (obj != NULL) {
				JS_DefineProperty(cx, obj, "topic"
				                  , STRING_TO_JSVAL(str)
				                  , NULL, NULL, JSPROP_ENUMERATE);
				str = JS_NewStringCopyN(cx, msg->payload, msg->payloadlen);
				JS_DefineProperty(cx, obj, "data"
				                  , STRING_TO_JSVAL(str)
				                  , NULL, NULL, JSPROP_ENUMERATE);
				JS_DefineProperty(cx, obj, "mid", INT_TO_JSVAL(msg->mid)
				                  , NULL, NULL, JSPROP_ENUMERATE);
				JS_DefineProperty(cx, obj, "qos", INT_TO_JSVAL(msg->qos)
				                  , NULL, NULL, JSPROP_ENUMERATE);
				JS_DefineProperty(cx, obj, "retain", BOOLEAN_TO_JSVAL(msg->retain)
				                  , NULL, NULL, JSPROP_ENUMERATE);

				JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
			}
		} else {
			JSString* str = JS_NewStringCopyN(cx, msg->payload, msg->payloadlen);
			if (str != NULL)
				JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));
		}
		mosquitto_message_free_contents(msg);
		free(msg);
	}
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

/* Properites */
enum {
	MQTT_PROP_ERROR
	, MQTT_PROP_ERROR_STR
	, MQTT_PROP_LIB
	, MQTT_PROP_BROKER_ADDR
	, MQTT_PROP_BROKER_PORT
	, MQTT_PROP_USERNAME
	, MQTT_PROP_PASSWORD
	, MQTT_PROP_KEEPALIVE
	, MQTT_PROP_PROT_VER
	, MQTT_PROP_PUB_QOS
	, MQTT_PROP_SUB_QOS
	, MQTT_PROP_TLS_MODE
	, MQTT_PROP_TLS_CAFILE
	, MQTT_PROP_TLS_CERTFILE
	, MQTT_PROP_TLS_KEYFILE
	, MQTT_PROP_TLS_KEYPASS
	, MQTT_PROP_TLS_PSK
	, MQTT_PROP_TLS_IDENTITY
	, MQTT_PROP_DATA_WAITING
	, MQTT_PROP_READ_LEVEL
};

#ifdef BUILD_JSDOCS
static const char* prop_desc[] = {
	"Result (error value) of last MQTT library function call - <small>READ ONLY</small>"
	, "Result description of last MQTT library function call - <small>READ ONLY</small>"
	, "MQTT library name and version - <small>READ ONLY</small>"
	, "IP address or hostname of MQTT broker to connect to, by default"
	, "TCP port number of MQTT broker to connect to, by default"
	, "Username to use when authenticating with MQTT broker, by default"
	, "Password to use when authenticating with MQTT broker, by default"
	, "Seconds of time to keep inactive connection alive"
	, "Protocol version number (3 = 3.1.0, 4 = 3.1.1, 5 = 5.0)"
	, "Quality Of Service (QOS) value to use when publishing, by default"
	, "Quality Of Service (QOS) value to use when subscribing, by default"
	, "TLS (encryption) mode"
	, "TLS Certificate Authority (CA) certificate (file path)"
	, "TLS Client certificate (file path)"
	, "Private key file"
	, "Private key file password"
	, "TLS Pre-Shared-Key"
	, "TLS PSK Identity"
	, "<i>true</i> if messages are waiting to be read"
	, "Number of messages waiting to be read"
	, NULL
};
#endif

static JSBool js_mqtt_set(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval *vp)
{
	jsval      idval;
	jsint      tiny;
	private_t* p;
	int32      i;

	if ((p = (private_t*)JS_GetPrivate(cx, obj)) == NULL) {
		// Prototype access
		return JS_TRUE;
	}

	JS_IdToValue(cx, id, &idval);
	tiny = JSVAL_TO_INT(idval);

	switch (tiny) {
		case MQTT_PROP_BROKER_ADDR:
			JSVALUE_TO_STRBUF(cx, *vp, p->cfg.broker_addr, sizeof p->cfg.broker_addr, NULL);
			break;
		case MQTT_PROP_BROKER_PORT:
			if (!JS_ValueToInt32(cx, *vp, &i))
				return JS_FALSE;
			p->cfg.broker_port = i;
			break;
		case MQTT_PROP_USERNAME:
			JSVALUE_TO_STRBUF(cx, *vp, p->cfg.username, sizeof p->cfg.username, NULL);
			break;
		case MQTT_PROP_PASSWORD:
			JSVALUE_TO_STRBUF(cx, *vp, p->cfg.password, sizeof p->cfg.password, NULL);
			break;
		case MQTT_PROP_KEEPALIVE:
			if (!JS_ValueToInt32(cx, *vp, &i))
				return JS_FALSE;
			p->cfg.keepalive = i;
			break;
		case MQTT_PROP_PROT_VER:
			if (!JS_ValueToInt32(cx, *vp, &i))
				return JS_FALSE;
			p->cfg.protocol_version = i;
			break;
		case MQTT_PROP_PUB_QOS:
			if (!JS_ValueToInt32(cx, *vp, &i))
				return JS_FALSE;
			p->cfg.publish_qos = i;
			break;
		case MQTT_PROP_SUB_QOS:
			if (!JS_ValueToInt32(cx, *vp, &i))
				return JS_FALSE;
			p->cfg.subscribe_qos = i;
			break;
		case MQTT_PROP_TLS_MODE:
			if (!JS_ValueToInt32(cx, *vp, &i))
				return JS_FALSE;
			p->cfg.tls.mode = i;
			break;
		case MQTT_PROP_TLS_CAFILE:
			JSVALUE_TO_STRBUF(cx, *vp, p->cfg.tls.cafile, sizeof p->cfg.tls.cafile, NULL);
			break;
		case MQTT_PROP_TLS_CERTFILE:
			JSVALUE_TO_STRBUF(cx, *vp, p->cfg.tls.certfile, sizeof p->cfg.tls.certfile, NULL);
			break;
		case MQTT_PROP_TLS_KEYFILE:
			JSVALUE_TO_STRBUF(cx, *vp, p->cfg.tls.keyfile, sizeof p->cfg.tls.keyfile, NULL);
			break;
		case MQTT_PROP_TLS_KEYPASS:
			JSVALUE_TO_STRBUF(cx, *vp, p->cfg.tls.keypass, sizeof p->cfg.tls.keypass, NULL);
			break;
		case MQTT_PROP_TLS_PSK:
			JSVALUE_TO_STRBUF(cx, *vp, p->cfg.tls.psk, sizeof p->cfg.tls.psk, NULL);
			break;
		case MQTT_PROP_TLS_IDENTITY:
			JSVALUE_TO_STRBUF(cx, *vp, p->cfg.tls.identity, sizeof p->cfg.tls.identity, NULL);
			break;
	}
	return JS_TRUE;
}

static JSBool js_mqtt_get(JSContext* cx, JSObject* obj, jsid id, jsval *vp)
{
	jsval      idval;
	jsint      tiny;
	private_t* p;
	JSString*  js_str;
	jsrefcount rc;

	if ((p = (private_t*)JS_GetPrivate(cx, obj)) == NULL) {
		// Protoype access
		return JS_TRUE;
	}

	JS_IdToValue(cx, id, &idval);
	tiny = JSVAL_TO_INT(idval);

	rc = JS_SUSPENDREQUEST(cx);

	switch (tiny) {
		case MQTT_PROP_ERROR:
			*vp = INT_TO_JSVAL(p->retval);
			break;
		case MQTT_PROP_ERROR_STR:
			JS_RESUMEREQUEST(cx, rc);
			if ((js_str = JS_NewStringCopyZ(cx, mosquitto_strerror(p->retval))) == NULL)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str);
			rc = JS_SUSPENDREQUEST(cx);
			break;
		case MQTT_PROP_LIB:
		{
			char str[128];
			JS_RESUMEREQUEST(cx, rc);
			if ((js_str = JS_NewStringCopyZ(cx, mqtt_libver(str, sizeof str))) == NULL)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str);
			rc = JS_SUSPENDREQUEST(cx);
			break;
		}
		case MQTT_PROP_BROKER_ADDR:
			JS_RESUMEREQUEST(cx, rc);
			if ((js_str = JS_NewStringCopyZ(cx, p->cfg.broker_addr)) == NULL)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str);
			rc = JS_SUSPENDREQUEST(cx);
			break;
		case MQTT_PROP_BROKER_PORT:
			*vp = INT_TO_JSVAL(p->cfg.broker_port);
			break;
		case MQTT_PROP_USERNAME:
			JS_RESUMEREQUEST(cx, rc);
			if ((js_str = JS_NewStringCopyZ(cx, p->cfg.username)) == NULL)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str);
			rc = JS_SUSPENDREQUEST(cx);
			break;
		case MQTT_PROP_PASSWORD:
			JS_RESUMEREQUEST(cx, rc);
			if ((js_str = JS_NewStringCopyZ(cx, p->cfg.password)) == NULL)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str);
			rc = JS_SUSPENDREQUEST(cx);
			break;
		case MQTT_PROP_KEEPALIVE:
			*vp = INT_TO_JSVAL(p->cfg.keepalive);
			break;
		case MQTT_PROP_PROT_VER:
			*vp = INT_TO_JSVAL(p->cfg.protocol_version);
			break;
		case MQTT_PROP_PUB_QOS:
			*vp = INT_TO_JSVAL(p->cfg.publish_qos);
			break;
		case MQTT_PROP_SUB_QOS:
			*vp = INT_TO_JSVAL(p->cfg.subscribe_qos);
			break;
		case MQTT_PROP_TLS_MODE:
			*vp = INT_TO_JSVAL(p->cfg.tls.mode);
			break;
		case MQTT_PROP_TLS_CAFILE:
			JS_RESUMEREQUEST(cx, rc);
			if ((js_str = JS_NewStringCopyZ(cx, p->cfg.tls.cafile)) == NULL)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str);
			rc = JS_SUSPENDREQUEST(cx);
			break;
		case MQTT_PROP_TLS_CERTFILE:
			JS_RESUMEREQUEST(cx, rc);
			if ((js_str = JS_NewStringCopyZ(cx, p->cfg.tls.certfile)) == NULL)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str);
			rc = JS_SUSPENDREQUEST(cx);
			break;
		case MQTT_PROP_TLS_KEYFILE:
			JS_RESUMEREQUEST(cx, rc);
			if ((js_str = JS_NewStringCopyZ(cx, p->cfg.tls.keyfile)) == NULL)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str);
			rc = JS_SUSPENDREQUEST(cx);
			break;
		case MQTT_PROP_TLS_KEYPASS:
			JS_RESUMEREQUEST(cx, rc);
			if ((js_str = JS_NewStringCopyZ(cx, p->cfg.tls.keypass)) == NULL)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str);
			rc = JS_SUSPENDREQUEST(cx);
			break;
		case MQTT_PROP_TLS_PSK:
			JS_RESUMEREQUEST(cx, rc);
			if ((js_str = JS_NewStringCopyZ(cx, p->cfg.tls.psk)) == NULL)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str);
			rc = JS_SUSPENDREQUEST(cx);
			break;
		case MQTT_PROP_TLS_IDENTITY:
			JS_RESUMEREQUEST(cx, rc);
			if ((js_str = JS_NewStringCopyZ(cx, p->cfg.tls.identity)) == NULL)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str);
			rc = JS_SUSPENDREQUEST(cx);
			break;
		case MQTT_PROP_DATA_WAITING:
			*vp = BOOLEAN_TO_JSVAL(INT_TO_BOOL(msgQueueReadLevel(&p->q)));
			break;
		case MQTT_PROP_READ_LEVEL:
			*vp = INT_TO_JSVAL(msgQueueReadLevel(&p->q));
			break;
	}

	JS_RESUMEREQUEST(cx, rc);
	return JS_TRUE;
}

#define MQTT_PROP_FLAGS JSPROP_ENUMERATE
#define MQTT_PROP_ROFLAGS JSPROP_ENUMERATE | JSPROP_READONLY

static jsSyncPropertySpec js_mqtt_properties[] = {
/*		 name				,tinyid					,flags,				ver	*/
	{   "error", MQTT_PROP_ERROR, MQTT_PROP_ROFLAGS, 320 },
	{   "error_str", MQTT_PROP_ERROR_STR, MQTT_PROP_ROFLAGS, 320 },
	{   "library", MQTT_PROP_LIB, MQTT_PROP_ROFLAGS, 320 },
	{   "broker_addr", MQTT_PROP_BROKER_ADDR, MQTT_PROP_FLAGS,   320 },
	{   "broker_port", MQTT_PROP_BROKER_PORT, MQTT_PROP_FLAGS,   320 },
	{   "username", MQTT_PROP_USERNAME, MQTT_PROP_FLAGS,   320 },
	{   "password", MQTT_PROP_PASSWORD, MQTT_PROP_FLAGS,   320 },
	{   "keepalive", MQTT_PROP_KEEPALIVE, MQTT_PROP_FLAGS,   320 },
	{   "protocol_version", MQTT_PROP_PROT_VER, MQTT_PROP_FLAGS,   320 },
	{   "publish_qos", MQTT_PROP_PUB_QOS, MQTT_PROP_FLAGS,   320 },
	{   "subscribe_qos", MQTT_PROP_SUB_QOS, MQTT_PROP_FLAGS,   320 },
	{   "tls_mode", MQTT_PROP_TLS_MODE, MQTT_PROP_FLAGS,   320 },
	{   "tls_ca_cert", MQTT_PROP_TLS_CAFILE, MQTT_PROP_FLAGS,   320 },
	{   "tls_client_cert", MQTT_PROP_TLS_CERTFILE, MQTT_PROP_FLAGS,   320 },
	{   "tls_private_key", MQTT_PROP_TLS_KEYFILE, MQTT_PROP_FLAGS,   320 },
	{   "tls_key_password", MQTT_PROP_TLS_KEYPASS, MQTT_PROP_FLAGS,   320 },
	{   "tls_psk", MQTT_PROP_TLS_PSK, MQTT_PROP_FLAGS,   320 },
	{   "tls_psk_identity", MQTT_PROP_TLS_IDENTITY, MQTT_PROP_FLAGS,   320 },
	{   "data_waiting", MQTT_PROP_DATA_WAITING, MQTT_PROP_ROFLAGS, 320 },
	{   "read_level", MQTT_PROP_READ_LEVEL, MQTT_PROP_ROFLAGS, 320 },
	{0}
};

static jsSyncMethodSpec js_mqtt_functions[] = {
	{"connect",     js_connect,     0,  JSTYPE_BOOLEAN
	 , JSDOCSTR("[<i>string</i> broker_address] [,<i>number</i> broker_port] [,<i>string</i> username] [,<i>string</i> password]")
	 , JSDOCSTR("Connect to an MQTT broker, by default (i.e. no arguments provided), the broker configured in SCFG->Networks->MQTT")
	 , 320},
	{"disconnect",  js_disconnect,  0,  JSTYPE_VOID
	 , JSDOCSTR("")
	 , JSDOCSTR("Close an open connection to the MQTT broker")
	 , 320},
	{"publish",     js_publish,     4,  JSTYPE_BOOLEAN
	 , JSDOCSTR("[<i>bool</i> retain=false,] [<i></i>number qos,] topic, data")
	 , JSDOCSTR("Publish a string to specified topic")
	 , 320},
	{"subscribe",   js_subscribe,   2,  JSTYPE_BOOLEAN
	 , JSDOCSTR("[<i>number</i> qos,] topic")
	 , JSDOCSTR("Subscribe to specified topic at (optional) QOS level")
	 , 320},
	{"read",        js_read,        0,  JSTYPE_STRING
	 , JSDOCSTR("[<i>number</i> timeout=0] [,<i>bool</i> verbose=false]")
	 , JSDOCSTR("Read next message, optionally waiting for <i>timeout</i> milliseconds, "
		        "returns an object instead of a string when <i>verbose</i> is <tt>true</tt>. "
		        "Returns <tt>false</tt> when no message is available.")
	 , 320},
	{0}
};

static JSBool js_mqtt_resolve(JSContext* cx, JSObject* obj, jsid id)
{
	char*  name = NULL;
	JSBool ret;

	if (id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;

		JS_IdToValue(cx, id, &idval);
		if (JSVAL_IS_STRING(idval)) {
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
			HANDLE_PENDING(cx, name);
		}
	}

	ret = js_SyncResolve(cx, obj, name, js_mqtt_properties, js_mqtt_functions, NULL, 0);
	free(name);
	return ret;
}

static JSBool js_mqtt_enumerate(JSContext* cx, JSObject* obj)
{
	return js_mqtt_resolve(cx, obj, JSID_VOID);
}

JSClass js_mqtt_class = {
	"MQTT"                  /* name			*/
	, JSCLASS_HAS_PRIVATE    /* flags		*/
	, JS_PropertyStub        /* addProperty	*/
	, JS_PropertyStub        /* delProperty	*/
	, js_mqtt_get            /* getProperty	*/
	, js_mqtt_set            /* setProperty	*/
	, js_mqtt_enumerate      /* enumerate	*/
	, js_mqtt_resolve        /* resolve		*/
	, JS_ConvertStub         /* convert		*/
	, js_finalize_mqtt       /* finalize		*/
};

static void mqtt_message_received(struct mosquitto* mosq, void* cbdata, const struct mosquitto_message* msg)
{
	private_t* p = (private_t*)cbdata;

	if (p != NULL && msg != NULL) {
		struct mosquitto_message m;
		if (mosquitto_message_copy(&m, msg) == MOSQ_ERR_SUCCESS)
			msgQueueWrite(&p->q, &m, sizeof m);
	}
}

static JSBool js_mqtt_constructor(JSContext* cx, uintN argc, jsval *arglist)
{
	JSObject*  obj;
	jsval*     argv = JS_ARGV(cx, arglist);
	private_t* p;
	char*      client_id = NULL;

	scfg_t*    scfg = JS_GetRuntimePrivate(JS_GetRuntime(cx));
	if (scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	obj = JS_NewObject(cx, &js_mqtt_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));

	if (argc > 0) {
		JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[0]), client_id, NULL);
		HANDLE_PENDING(cx, client_id);
	}

	if ((p = (private_t*)malloc(sizeof(private_t))) == NULL) {
		JS_ReportError(cx, "malloc failed");
		free(client_id);
		return JS_FALSE;
	}
	memset(p, 0, sizeof *p);
	msgQueueInit(&p->q, /* flags: */ 0);
	p->cfg = scfg->mqtt;
	p->handle = mosquitto_new(client_id, /* clean_session: */ true, /* userdata: */ p);
	free(client_id);
	if (p->handle == NULL) {
		JS_ReportError(cx, "mosquitto_new failure (errno=%d)", errno);
		free(p);
		return JS_FALSE;
	}
	if (!JS_SetPrivate(cx, obj, p)) {
		JS_ReportError(cx, "JS_SetPrivate failed");
		free(p);
		return JS_FALSE;
	}
	mosquitto_message_callback_set(p->handle, mqtt_message_received);
	int result = mosquitto_loop_start(p->handle);
	if (result != MOSQ_ERR_SUCCESS) {
		JS_ReportError(cx, "mosquitto_loop_start error %d", result);
		free(p);
		return JS_FALSE;
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj, "Class used for MQTT communications", 320);
	js_DescribeSyncConstructor(cx, obj, "To create a new MQTT object: "
	                           "<tt>var mqtt = new MQTT([<i>client_id</i>])</tt><br>"
	                           );
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", prop_desc, JSPROP_READONLY);
#endif

	return JS_TRUE;
}

JSObject* js_CreateMQTTClass(JSContext* cx, JSObject* parent)
{
	return JS_InitClass(cx, parent, NULL
	                    , &js_mqtt_class
	                    , js_mqtt_constructor
	                    , 0 /* number of constructor args */
	                    , NULL /* props, specified in constructor */
	                    , NULL /* funcs, specified in constructor */
	                    , NULL, NULL);
}

#endif // USE_MOSQUITTO
