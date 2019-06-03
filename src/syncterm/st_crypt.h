/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _ST_CRYPT_H_
#define _ST_CRYPT_H_

#ifndef WITHOUT_CRYPTLIB
#include <cryptlib.h>
#if CRYPTLIB_VERSION < 3400
#if CRYPTLIB_VERSION < 340 || CRYPTLIB_VERSION > 999
#define CRYPT_ATTRIBUTE_ERRORMESSAGE	CRYPT_ATTRIBUTE_INT_ERRORMESSAGE
#endif
#endif

struct crypt_funcs {
	int (*PopData)( C_IN CRYPT_HANDLE envelope, C_OUT void C_PTR buffer,
		C_IN int length, C_OUT int C_PTR bytesCopied );
	int (*PushData)( C_IN CRYPT_HANDLE envelope, C_IN void C_PTR buffer,
		C_IN int length, C_OUT int C_PTR bytesCopied );
	int (*FlushData)( C_IN CRYPT_HANDLE envelope );
	int (*Init)( void );
	int (*End)( void );
	int (*CreateSession)( C_OUT CRYPT_SESSION C_PTR session,
		C_IN CRYPT_USER cryptUser,
		C_IN CRYPT_SESSION_TYPE formatType );
	int (*GetAttribute)( C_IN CRYPT_HANDLE cryptHandle,
		C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
		C_OUT int C_PTR value );
	int (*GetAttributeString)( C_IN CRYPT_HANDLE cryptHandle,
		C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
		C_OUT void C_PTR value,
		C_OUT int C_PTR valueLength );
	int (*SetAttribute)( C_IN CRYPT_HANDLE cryptHandle,
		C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
		C_IN int value );
	int (*SetAttributeString)( C_IN CRYPT_HANDLE cryptHandle,
		C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
		C_IN void C_PTR value, C_IN int valueLength );
	int (*DestroySession)( C_IN CRYPT_SESSION session );
	int (*AddRandom)( C_IN void C_PTR randomData, C_IN int randomDataLength );
};

#endif

extern struct crypt_funcs cl;
extern int crypt_loaded;

int init_crypt(void);
void exit_crypt(void);

#endif
