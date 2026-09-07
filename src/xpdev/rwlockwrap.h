#ifndef RWLOCKWRAP_H
#define RWLOCKWRAP_H

#ifndef NDEBUG
#include <assert.h>
#endif

#if defined(__unix__)

#include <pthread.h>
#include <stdbool.h>

typedef pthread_rwlock_t rwlock_t;

static inline bool
rwlock_init(rwlock_t *lock)
{
	return pthread_rwlock_init(lock, NULL) == 0;
}

static inline bool
rwlock_rdlock(rwlock_t *lock)
{
	return pthread_rwlock_rdlock(lock) == 0;
}

static inline bool
rwlock_tryrdlock(rwlock_t *lock)
{
	return pthread_rwlock_tryrdlock(lock) == 0;
}

static inline bool
rwlock_wrlock(rwlock_t *lock)
{
	return pthread_rwlock_wrlock(lock) == 0;
}

static inline bool
rwlock_trywrlock(rwlock_t *lock)
{
	return pthread_rwlock_trywrlock(lock) == 0;
}

static inline bool
rwlock_unlock(rwlock_t *lock)
{
	return pthread_rwlock_unlock(lock) == 0;
}

static inline bool
rwlock_destroy(rwlock_t *lock)
{
	return pthread_rwlock_destroy(lock) == 0;
}

#elif defined(_WIN32)

#include "gen_defs.h"   // For windows.h and bool
#include "threadwrap.h"
#include "wrapdll.h"

struct rwlock;
typedef struct rwlock *rwlock_t;

DLLEXPORT bool rwlock_init(rwlock_t *lock);
DLLEXPORT bool rwlock_rdlock(rwlock_t *lock);
DLLEXPORT bool rwlock_tryrdlock(rwlock_t *lock);
DLLEXPORT bool rwlock_wrlock(rwlock_t *lock);
DLLEXPORT bool rwlock_trywrlock(rwlock_t *lock);
DLLEXPORT bool rwlock_unlock(rwlock_t *lock);
DLLEXPORT bool rwlock_destroy(rwlock_t *lock);

#else
#error Not implemented
#endif

#define rwlock_destroy_ign(lock) ((void)rwlock_destroy(lock))
#ifdef NDEBUG
#define assert_rwlock_rdlock(lk) rwlock_rdlock(lk)
#define assert_rwlock_wrlock(lk) rwlock_wrlock(lk)
#define assert_rwlock_unlock(lk) rwlock_unlock(lk)
#define assert_rwlock_init(lk)   rwlock_init(lk)
#else
#define assert_rwlock_rdlock(lk) assert(rwlock_rdlock(lk))
#define assert_rwlock_wrlock(lk) assert(rwlock_wrlock(lk))
#define assert_rwlock_unlock(lk) assert(rwlock_unlock(lk))
#define assert_rwlock_init(lk)   assert(rwlock_init(lk))
#endif

#endif
