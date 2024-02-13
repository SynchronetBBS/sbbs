/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _ST_CRYPT_H_
#define _ST_CRYPT_H_

#ifndef WITHOUT_CRYPTLIB
 #include <cryptlib.h>
 #if CRYPTLIB_VERSION < 3400
  #if CRYPTLIB_VERSION < 340 || CRYPTLIB_VERSION > 999
   #define CRYPT_ATTRIBUTE_ERRORMESSAGE CRYPT_ATTRIBUTE_INT_ERRORMESSAGE
  #endif
 #endif

struct crypt_funcs {
	C_RET (*PopData)(C_IN CRYPT_HANDLE envelope, C_OUT void C_PTR buffer,
	    C_IN int length, C_OUT int C_PTR bytesCopied);
	C_RET (*PushData)(C_IN CRYPT_HANDLE envelope, C_IN void C_PTR buffer,
	    C_IN int length, C_OUT int C_PTR bytesCopied);
	C_RET (*FlushData)(C_IN CRYPT_HANDLE envelope);
	C_RET (*Init)(void);
	C_RET (*End)(void);
	C_RET (*CreateSession)(C_OUT CRYPT_SESSION C_PTR session,
	    C_IN CRYPT_USER                            cryptUser,
	    C_IN CRYPT_SESSION_TYPE                    formatType);
	C_RET (*GetAttribute)(C_IN CRYPT_HANDLE cryptHandle,
	    C_IN CRYPT_ATTRIBUTE_TYPE         attributeType,
	    C_OUT int C_PTR                   value);
	C_RET (*GetAttributeString)(C_IN CRYPT_HANDLE cryptHandle,
	    C_IN CRYPT_ATTRIBUTE_TYPE               attributeType,
	    C_OUT void C_PTR                        value,
	    C_OUT int C_PTR                         valueLength);
	C_RET (*SetAttribute)(C_IN CRYPT_HANDLE cryptHandle,
	    C_IN CRYPT_ATTRIBUTE_TYPE         attributeType,
	    C_IN int                          value);
	C_RET (*SetAttributeString)(C_IN CRYPT_HANDLE cryptHandle,
	    C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
	    C_IN void C_PTR value, C_IN int valueLength);
	C_RET (*DestroySession)(C_IN CRYPT_SESSION session);
	C_RET (*AddRandom)(C_IN void C_PTR randomData, C_IN int randomDataLength);
	C_RET (*DeleteAttribute)(C_IN CRYPT_HANDLE cryptHandle,
	    C_IN CRYPT_ATTRIBUTE_TYPE            attributeType);
	C_RET (*KeysetOpen)(C_OUT CRYPT_KEYSET C_PTR keyset,
            C_IN CRYPT_USER cryptUser,
            C_IN CRYPT_KEYSET_TYPE keysetType,
            C_IN C_STR name, C_IN CRYPT_KEYOPT_TYPE options);
        C_RET (*KeysetClose)(C_IN CRYPT_KEYSET keyset);
        C_RET (*GenerateKey)(C_IN CRYPT_CONTEXT cryptContext);
	C_RET (*AddPrivateKey)(C_IN CRYPT_KEYSET keyset,
	    C_IN CRYPT_HANDLE cryptKey,
	    C_IN C_STR password );
	C_RET (*GetPrivateKey)(C_IN CRYPT_KEYSET keyset,
	    C_OUT CRYPT_CONTEXT C_PTR cryptContext,
	    C_IN CRYPT_KEYID_TYPE keyIDtype,
	    C_IN C_STR keyID, C_IN_OPT C_STR password );
	C_RET (*CreateContext)(C_OUT CRYPT_CONTEXT C_PTR cryptContext,
	    C_IN CRYPT_USER cryptUser,
	    C_IN CRYPT_ALGO_TYPE cryptAlgo);
	C_RET (*DestroyContext)(C_IN CRYPT_CONTEXT cryptContext);
};

#endif // ifndef WITHOUT_CRYPTLIB

extern struct crypt_funcs cl;
extern int                crypt_loaded;
int init_crypt(void);
void exit_crypt(void);

#endif // ifndef _ST_CRYPT_H_
