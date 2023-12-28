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
	int (*PopData)(C_IN CRYPT_HANDLE envelope, C_OUT void C_PTR buffer,
	    C_IN int length, C_OUT int C_PTR bytesCopied);
	int (*PushData)(C_IN CRYPT_HANDLE envelope, C_IN void C_PTR buffer,
	    C_IN int length, C_OUT int C_PTR bytesCopied);
	int (*FlushData)(C_IN CRYPT_HANDLE envelope);
	int (*Init)(void);
	int (*End)(void);
	int (*CreateSession)(C_OUT CRYPT_SESSION C_PTR session,
	    C_IN CRYPT_USER                            cryptUser,
	    C_IN CRYPT_SESSION_TYPE                    formatType);
	int (*GetAttribute)(C_IN CRYPT_HANDLE cryptHandle,
	    C_IN CRYPT_ATTRIBUTE_TYPE         attributeType,
	    C_OUT int C_PTR                   value);
	int (*GetAttributeString)(C_IN CRYPT_HANDLE cryptHandle,
	    C_IN CRYPT_ATTRIBUTE_TYPE               attributeType,
	    C_OUT void C_PTR                        value,
	    C_OUT int C_PTR                         valueLength);
	int (*SetAttribute)(C_IN CRYPT_HANDLE cryptHandle,
	    C_IN CRYPT_ATTRIBUTE_TYPE         attributeType,
	    C_IN int                          value);
	int (*SetAttributeString)(C_IN CRYPT_HANDLE cryptHandle,
	    C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
	    C_IN void C_PTR value, C_IN int valueLength);
	int (*DestroySession)(C_IN CRYPT_SESSION session);
	int (*AddRandom)(C_IN void C_PTR randomData, C_IN int randomDataLength);
	int (*DeleteAttribute)(C_IN CRYPT_HANDLE cryptHandle,
	    C_IN CRYPT_ATTRIBUTE_TYPE            attributeType);
	int (*KeysetOpen)(C_OUT CRYPT_KEYSET C_PTR keyset,
            C_IN CRYPT_USER cryptUser,
            C_IN CRYPT_KEYSET_TYPE keysetType,
            C_IN C_STR name, C_IN CRYPT_KEYOPT_TYPE options);
        int (*KeysetClose)(C_IN CRYPT_KEYSET keyset);
        int (*GenerateKey)(C_IN CRYPT_CONTEXT cryptContext);
	int (*AddPrivateKey)(C_IN CRYPT_KEYSET keyset,
	    C_IN CRYPT_HANDLE cryptKey,
	    C_IN C_STR password );
	int (*GetPrivateKey)(C_IN CRYPT_KEYSET keyset,
	    C_OUT CRYPT_CONTEXT C_PTR cryptContext,
	    C_IN CRYPT_KEYID_TYPE keyIDtype,
	    C_IN C_STR keyID, C_IN_OPT C_STR password );
	int (*CreateContext)(C_OUT CRYPT_CONTEXT C_PTR cryptContext,
	    C_IN CRYPT_USER cryptUser,
	    C_IN CRYPT_ALGO_TYPE cryptAlgo);
	int (*DestroyContext)(C_IN CRYPT_CONTEXT cryptContext);
};

#endif // ifndef WITHOUT_CRYPTLIB

extern struct crypt_funcs cl;
extern int                crypt_loaded;
int init_crypt(void);
void exit_crypt(void);

#endif // ifndef _ST_CRYPT_H_
