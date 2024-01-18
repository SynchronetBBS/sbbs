#ifndef RWLOCKWRAP_H
#define RWLOCKWRAP_H

#if defined(__unix__)

#include <pthread.h>
#include <stdbool.h>
typedef pthread_rwlock_t rwlock_t;

#define rwlock_init(lock) (pthread_rwlock_init(lock, NULL) == 0)
#define rwlock_rdlock(lock) (pthread_rwlock_rdlock(lock) == 0)
#define rwlock_tryrdlock(lock) (pthread_rwlock_tryrdlock(lock) == 0)
#define rwlock_wrlock(lock) (pthread_rwlock_wrlock(lock) == 0)
#define rwlock_trywrlock(lock) (pthread_rwlock_trywrlock(lock) == 0)
#define rwlock_unlock(lock) (pthread_rwlock_unlock(lock) == 0)

#elif defined(__BORLANDC__)

// Not supported, but ignored...

#elif defined(_WIN32)

#include "gen_defs.h"	// For windows.h and bool :(
#include "threadwrap.h"

typedef struct {
	CRITICAL_SECTION lk;       // Protects access to all elements
	CRITICAL_SECTION wlk;      // Locked by an active writer
	unsigned readers;
	unsigned writers;
	unsigned writers_waiting;
	DWORD writer;
} rwlock_t;

bool rwlock_init(rwlock_t *lock);
bool rwlock_rdlock(rwlock_t *lock);
bool rwlock_tryrdlock(rwlock_t *lock);
bool rwlock_wrlock(rwlock_t *lock);
bool rwlock_trywrlock(rwlock_t *lock);
bool rwlock_unlock(rwlock_t *lock);

#else
#error Not implemented
#endif

#endif
