/* md5.h - header file for md5.c */

/* $Id: md5.h,v 1.7 2019/03/22 21:29:12 rswindell Exp $ */

/* RSA Data Security, Inc., MD5 Message-Digest Algorithm */

/* NOTE: Numerous changes have been made; the following notice is
included to satisfy legal requirements.

Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA Data Security, Inc. MD5 Message-Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA Data
Security, Inc. MD5 Message-Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.
*/

#ifndef H__MD5
#define H__MD5

#include <stddef.h>		/* size_t */
#include <gen_defs.h>	/* uint32_t */

#define MD5_DIGEST_SIZE		16

#ifndef BYTE
	typedef unsigned char BYTE;
#endif

typedef struct
{
  uint32_t	state[4];
  uint32_t	count[2];
  BYTE	buffer[64];
} MD5;

#if defined(_WIN32) && (defined(MD5_IMPORTS) || defined(MD5_EXPORTS))
	#if defined(MD5_IMPORTS)
		#define MD5EXPORT	__declspec(dllimport)
	#else
		#define MD5EXPORT	__declspec(dllexport)
	#endif
	#if defined(__BORLANDC__)
		#define MD5CALL
	#else
		#define MD5CALL
	#endif
#else	/* !_WIN32 */
	#define MD5EXPORT
	#define MD5CALL
#endif


#ifdef __cplusplus
extern "C" {
#endif

MD5EXPORT void	MD5CALL MD5_open(MD5* ctx);
MD5EXPORT void	MD5CALL MD5_digest(MD5* ctx, const void* buf, size_t len);
MD5EXPORT void	MD5CALL MD5_close(MD5* ctx, BYTE digest[MD5_DIGEST_SIZE]);
MD5EXPORT BYTE*	MD5CALL MD5_calc(BYTE digest[MD5_DIGEST_SIZE], const void* buf, size_t len);
MD5EXPORT BYTE*	MD5CALL MD5_hex(BYTE* dest, const BYTE digest[MD5_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif

#endif
