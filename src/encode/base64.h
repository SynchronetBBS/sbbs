/* Copyright (c) Stephen Hurd and Rob Swindell; SPDX-License-Identifier: BSD-2-Clause */

/* Public interface for the Base64 implementation in base64.c. */

#ifndef XPDEV_ENCODE_BASE64_H
#define XPDEV_ENCODE_BASE64_H

#include "gen_defs.h" // ssize_t

#if defined(_WIN32) && (defined(B64_IMPORTS) || defined(B64_EXPORTS))
	#if defined(B64_IMPORTS)
		#define B64EXPORT __declspec(dllimport)
	#else
		#define B64EXPORT __declspec(dllexport)
	#endif
#else
	#define B64EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

B64EXPORT ssize_t b64_encode(char* target, size_t target_size, const char* source, size_t source_length);
B64EXPORT ssize_t b64_decode(char* target, size_t target_size, const char* source, size_t source_length);

#ifdef __cplusplus
}
#endif

#endif /* XPDEV_ENCODE_BASE64_H */
