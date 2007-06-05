#ifndef _ST_CRYPT_H_
#define _ST_CRYPT_H_

#include"cryptlib.h"

#if defined(_MSC_VER)
	#undef C_RET 
	#define C_RET int
#endif

#ifdef _WIN32
	#define HACK_HACK_HACK __stdcall
#else
	#define HACK_HACK_HACK
#endif

struct crypt_funcs {
	C_RET (HACK_HACK_HACK *PopData)( C_IN CRYPT_HANDLE envelope, C_OUT void C_PTR buffer,
		C_IN int length, C_OUT int C_PTR bytesCopied );
	C_RET (HACK_HACK_HACK *PushData)( C_IN CRYPT_HANDLE envelope, C_IN void C_PTR buffer,
		C_IN int length, C_OUT int C_PTR bytesCopied );
	C_RET (HACK_HACK_HACK *FlushData)( C_IN CRYPT_HANDLE envelope );
	C_RET (HACK_HACK_HACK *Init)( void );
	C_RET (HACK_HACK_HACK *End)( void );
	C_RET (HACK_HACK_HACK *CreateSession)( C_OUT CRYPT_SESSION C_PTR session,
		C_IN CRYPT_USER cryptUser,
		C_IN CRYPT_SESSION_TYPE formatType );
	C_RET (HACK_HACK_HACK *GetAttribute)( C_IN CRYPT_HANDLE cryptHandle,
		C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
		C_OUT int C_PTR value );
	C_RET (HACK_HACK_HACK *GetAttributeString)( C_IN CRYPT_HANDLE cryptHandle,
		C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
		C_OUT void C_PTR value,
		C_OUT int C_PTR valueLength );
	C_RET (HACK_HACK_HACK *SetAttribute)( C_IN CRYPT_HANDLE cryptHandle,
		C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
		C_IN int value );
	C_RET (HACK_HACK_HACK *SetAttributeString)( C_IN CRYPT_HANDLE cryptHandle,
		C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
		C_IN void C_PTR value, C_IN int valueLength );
	C_RET (HACK_HACK_HACK *DestroySession)( C_IN CRYPT_SESSION session );
	C_RET (HACK_HACK_HACK *AddRandom)( C_IN void C_PTR randomData, C_IN int randomDataLength );
};

#undef HACK_HACK_HACK

extern struct crypt_funcs cl;
extern int crypt_loaded;

int init_crypt(void);
void exit_crypt(void);

#endif
