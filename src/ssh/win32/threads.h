/*
 * Minimal C11 <threads.h> shim for MinGW-w64.
 *
 * MinGW-w64 doesn't ship <threads.h> (nor does the mingw-w64 headers
 * package).  This file provides the subset of the C11 threads API
 * that DeuceSSH actually uses — mtx_*, cnd_*, thrd_* — as thin
 * wrappers over Windows primitives.  Platform-gated in DeuceSSH's
 * CMakeLists: only added to the include path when targeting MinGW.
 *
 * Deliberately minimal: no tss_* (thread-specific storage), no
 * call_once, no mtx_timed / mtx_recursive support.  Add as needed.
 */

#ifndef DSSH_WIN32_THREADS_H
#define DSSH_WIN32_THREADS_H

#include <windows.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef CRITICAL_SECTION   mtx_t;
typedef CONDITION_VARIABLE cnd_t;
typedef HANDLE             thrd_t;
typedef int              (*thrd_start_t)(void *);

enum {
	mtx_plain      = 0,
	mtx_timed      = 1,
	mtx_recursive  = 2,
};

enum {
	thrd_success   = 0,
	thrd_error     = 1,
	thrd_nomem     = 2,
	thrd_busy      = 3,
	thrd_timedout  = 4,
};

#include "../ssh-thread-shim.h"

#ifdef __cplusplus
}
#endif

#endif
