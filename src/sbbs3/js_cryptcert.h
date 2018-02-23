#ifndef _JS_CRYPTCERT_H_
#define _JS_CRYPTCERT_H_

struct js_cryptcert_private_data {
	CRYPT_CERTIFICATE	cert;
};

JSObject* DLLCALL js_CreateCryptCertObject(JSContext* cx, CRYPT_CERTIFICATE cert);

#endif
