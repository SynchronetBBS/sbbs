#ifndef DSSH_PORTABLE_H
#define DSSH_PORTABLE_H

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
 * DSSH_PUBLIC  — exported from the shared library (public API)
 * DSSH_PRIVATE — hidden, only visible within the library
 *
 * When building as a shared library, compile with -DDSSH_SHARED.
 * When building as a static library or consuming the library, these
 * are no-ops.
 */
#if defined(DSSH_SHARED)
 #if defined(__GNUC__) || defined(__clang__)
  #define DSSH_PUBLIC __attribute__((visibility("default")))
  #define DSSH_PRIVATE __attribute__((visibility("hidden")))
 #elif defined(_MSC_VER)
  #define DSSH_PUBLIC __declspec(dllexport)
  #define DSSH_PRIVATE
 #else
  #define DSSH_PUBLIC
  #define DSSH_PRIVATE
 #endif
#elif defined(DSSH_SHARED_IMPORT)

/* Consumer of the shared library on Windows */
 #if defined(_MSC_VER)
  #define DSSH_PUBLIC __declspec(dllimport)
  #define DSSH_PRIVATE
 #else
  #define DSSH_PUBLIC
  #define DSSH_PRIVATE
 #endif
#else // if defined(DSSH_SHARED)

/* Static library — no visibility annotations needed */
 #define DSSH_PUBLIC
 #define DSSH_PRIVATE
#endif // if defined(DSSH_SHARED)

#endif // ifndef DSSH_PORTABLE_H
