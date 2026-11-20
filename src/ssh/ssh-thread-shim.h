/* Private C11 subset, included after the selected platform's thread types.
 * Standard-looking inline names never define externally visible C11 symbols. */
#ifndef DSSH_THREAD_SHIM_H
#define DSSH_THREAD_SHIM_H

#include "deucessh-portable.h"
#define DSSH_THREADS_SHIM 1

#ifdef __cplusplus
extern "C" {
#endif

#define DSSH_THREAD_RESULT(name, parameters, arguments) \
	DSSH_PRIVATE int dssh_thread_##name parameters; \
	static inline int name parameters \
	{ \
		return dssh_thread_##name arguments; \
	}
#define DSSH_THREAD_VOID(name, parameters, arguments) \
	DSSH_PRIVATE void dssh_thread_##name parameters; \
	static inline void name parameters \
	{ \
		dssh_thread_##name arguments; \
	}

DSSH_THREAD_RESULT(mtx_init, (mtx_t *mutex, int type), (mutex, type))
DSSH_THREAD_VOID(mtx_destroy, (mtx_t *mutex), (mutex))
DSSH_THREAD_RESULT(mtx_lock, (mtx_t *mutex), (mutex))
DSSH_THREAD_RESULT(mtx_trylock, (mtx_t *mutex), (mutex))
DSSH_THREAD_RESULT(mtx_unlock, (mtx_t *mutex), (mutex))
DSSH_THREAD_RESULT(cnd_init, (cnd_t *condition), (condition))
DSSH_THREAD_VOID(cnd_destroy, (cnd_t *condition), (condition))
DSSH_THREAD_RESULT(cnd_signal, (cnd_t *condition), (condition))
DSSH_THREAD_RESULT(cnd_broadcast, (cnd_t *condition), (condition))
DSSH_THREAD_RESULT(cnd_wait, (cnd_t *condition, mtx_t *mutex), (condition, mutex))
DSSH_THREAD_RESULT(cnd_timedwait,
    (cnd_t *condition, mtx_t *mutex, const struct timespec *deadline),
    (condition, mutex, deadline))
DSSH_THREAD_RESULT(thrd_create, (thrd_t *thread, thrd_start_t start, void *arg),
    (thread, start, arg))
DSSH_THREAD_RESULT(thrd_join, (thrd_t thread, int *result), (thread, result))
DSSH_THREAD_RESULT(thrd_sleep,
    (const struct timespec *duration, struct timespec *remaining), (duration, remaining))
DSSH_THREAD_VOID(thrd_yield, (void), ())
#undef DSSH_THREAD_VOID
#undef DSSH_THREAD_RESULT

#ifdef __cplusplus
}
#endif
#endif
