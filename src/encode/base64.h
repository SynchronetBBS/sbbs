/* SPDX-License-Identifier: LGPL-2.0-or-later */

/*
 * Public interface for the Base64 implementation in base64.c.
 *
 * This header is licensed under the same GNU Lesser General Public License
 * terms as the implementation. It replaces an older header whose GPL notice
 * was added in error and did not reflect the license of base64.c.
 */

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
