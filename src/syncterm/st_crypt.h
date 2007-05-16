#ifndef _ST_CRYPT_H_
#define _ST_CRYPT_H_

#include"cryptlib.h"

#if defined(_MSC_VER)
	#undef C_RET 
	#define C_RET int
#endif

struct crypt_funcs {
	C_RET (*PopData)( C_IN CRYPT_HANDLE envelope, C_OUT void C_PTR buffer,
		C_IN int length, C_OUT int C_PTR bytesCopied );
	C_RET (*PushData)( C_IN CRYPT_HANDLE envelope, C_IN void C_PTR buffer,
		C_IN int length, C_OUT int C_PTR bytesCopied );
	C_RET (*FlushData)( C_IN CRYPT_HANDLE envelope );
	C_RET (*Init)( void );
	C_RET (*End)( void );
	C_RET (*CreateSession)( C_OUT CRYPT_SESSION C_PTR session,
		C_IN CRYPT_USER cryptUser,
		C_IN CRYPT_SESSION_TYPE formatType );
	C_RET (*GetAttribute)( C_IN CRYPT_HANDLE cryptHandle,
		C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
		C_OUT int C_PTR value );
	C_RET (*GetAttributeString)( C_IN CRYPT_HANDLE cryptHandle,
		C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
		C_OUT void C_PTR value,
		C_OUT int C_PTR valueLength );
	C_RET (*SetAttribute)( C_IN CRYPT_HANDLE cryptHandle,
		C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
		C_IN int value );
	C_RET (*SetAttributeString)( C_IN CRYPT_HANDLE cryptHandle,
		C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
		C_IN void C_PTR value, C_IN int valueLength );
	C_RET (*DestroySession)( C_IN CRYPT_SESSION session );
	C_RET (*AddRandom)( C_IN void C_PTR randomData, C_IN int randomDataLength );
};
extern struct crypt_funcs cl;
extern int crypt_loaded;

int init_crypt(void);
void exit_crypt(void);

#endif