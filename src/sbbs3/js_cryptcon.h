#ifndef _JS_CRYPTCON_H_
#define _JS_CRYPTCON_H_

struct js_cryptcon_private_data {
	CRYPT_CONTEXT	ctx;
};

extern JSClass js_cryptcon_class;

JSObject* DLLCALL js_CreateCryptconObject(JSContext* cx, CRYPT_CONTEXT ctx);

#endif
