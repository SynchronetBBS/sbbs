#ifndef DEUCE_SSH_PLATWRAP_H
#define DEUCE_SSH_PLATWRAP_H

#include <stddef.h>

/*
 * This file is here to hold all the ugly compiler and platform
 * "stuff" that ends up being needed.
 */

// C23
#ifndef unreachable
	#ifdef __GNUC__
		#define unreachable() (__builtin_unreachable())
	#elif defined _MSC_VER
		#define unreachable() (__assume(false))
	#endif
#endif

/*
 * Symbol visibility for shared libraries.
 *
 * DEUCE_SSH_PUBLIC  — exported from the shared library (public API)
 * DEUCE_SSH_PRIVATE — hidden, only visible within the library
 *
 * When building as a shared library, compile with -DDEUCE_SSH_SHARED.
 * When building as a static library or consuming the library, these
 * are no-ops.
 */
#if defined(DEUCE_SSH_SHARED)
	#if defined(__GNUC__) || defined(__clang__)
		#define DEUCE_SSH_PUBLIC  __attribute__((visibility("default")))
		#define DEUCE_SSH_PRIVATE __attribute__((visibility("hidden")))
	#elif defined(_MSC_VER)
		#define DEUCE_SSH_PUBLIC  __declspec(dllexport)
		#define DEUCE_SSH_PRIVATE
	#else
		#define DEUCE_SSH_PUBLIC
		#define DEUCE_SSH_PRIVATE
	#endif
#elif defined(DEUCE_SSH_SHARED_IMPORT)
	/* Consumer of the shared library on Windows */
	#if defined(_MSC_VER)
		#define DEUCE_SSH_PUBLIC  __declspec(dllimport)
		#define DEUCE_SSH_PRIVATE
	#else
		#define DEUCE_SSH_PUBLIC
		#define DEUCE_SSH_PRIVATE
	#endif
#else
	/* Static library — no visibility annotations needed */
	#define DEUCE_SSH_PUBLIC
	#define DEUCE_SSH_PRIVATE
#endif

#endif
