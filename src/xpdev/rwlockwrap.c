#include "rwlockwrap.h"

#if defined(_WIN32)

#include <stdlib.h>

static struct rwlock_reader_thread *
find_self(rwlock_t *lock, struct rwlock_reader_thread ***prev)
{
	DWORD self = GetCurrentThreadId();
	struct rwlock_reader_thread *ret;

	if (prev)
		*prev = &lock->rthreads;
	for (ret = NULL; ret; ret = ret->next) {
		if (ret->id == self)
			return ret;
		if (prev) {
			*prev = &ret->next;
		}
	}
	ret = calloc(1, sizeof(*ret));
	if (ret == NULL)
		return ret;
	ret->next = lock->rthreads;
	ret->id = self;
	lock->rthreads = ret;
	return ret;
}

BOOL
rwlock_init(rwlock_t *lock)
{
	InitializeCriticalSection(&lock->lk);
	InitializeCriticalSection(&lock->wlk);
	lock->readers = 0;
	lock->writers = 0;
	lock->writers_waiting = 0;
	lock->writer = (DWORD)-1;
	lock->rthreads = NULL;
	return TRUE;
}

BOOL
rwlock_rdlock(rwlock_t *lock)
{
	struct rwlock_reader_thread *rc;

	EnterCriticalSection(&lock->lk);
	rc = find_self(lock, NULL);
	if (rc == NULL) {
		LeaveCriticalSection(&lock->lk);
		return FALSE;
	}
	while(rc->count == 0 && (lock->writers || lock->writers_waiting)) {
		LeaveCriticalSection(&lock->lk);
		// Wait for current writer to release
		EnterCriticalSection(&lock->wlk);
		EnterCriticalSection(&lock->lk);
		if (lock->writers || lock->writers_waiting) {
			LeaveCriticalSection(&lock->lk);
			LeaveCriticalSection(&lock->wlk);
			/*
			 * Just in case thread scheduling is weird on
			 * Win32, allow time for a writer to grab wlk
			 */
			Sleep(1);
			EnterCriticalSection(&lock->lk);
			continue;
		}
		else {
			lock->readers++;
			rc->count++;
			LeaveCriticalSection(&lock->lk);
			LeaveCriticalSection(&lock->wlk);
			return TRUE;
		}
	}
	lock->readers++;
	rc->count++;
	LeaveCriticalSection(&lock->lk);
	return TRUE;
}

BOOL
rwlock_tryrdlock(rwlock_t *lock)
{
	BOOL ret = FALSE;
	struct rwlock_reader_thread *rc;

	EnterCriticalSection(&lock->lk);
	rc = find_self(lock, NULL);
	if (rc == NULL) {
		LeaveCriticalSection(&lock->lk);
		return FALSE;
	}
	if (lock->writers == 0 && lock->writers_waiting == 0) {
		rc->count++;
		lock->readers++;
		ret = TRUE;
	}
	LeaveCriticalSection(&lock->lk);
	return ret;
}

BOOL
rwlock_wrlock(rwlock_t *lock)
{
	EnterCriticalSection(&lock->lk);
	lock->writers_waiting++;
	LeaveCriticalSection(&lock->lk);
	EnterCriticalSection(&lock->wlk);
	EnterCriticalSection(&lock->lk);
	// No recursion
	if (lock->writers == 0) {
		lock->writers_waiting--;
		lock->writers++;
		lock->writer = GetCurrentThreadId();
		LeaveCriticalSection(&lock->lk);
		// Keep holding wlk
		return TRUE;
	}
	LeaveCriticalSection(&lock->lk);
	LeaveCriticalSection(&lock->wlk);
	return FALSE;
}

BOOL
rwlock_trywrlock(rwlock_t *lock)
{
	if (TryEnterCriticalSection(&lock->wlk)) {
		EnterCriticalSection(&lock->lk);
		// Prevent recursing on writer locks
		if (lock->writers == 0) {
			lock->writers++;
			lock->writer = GetCurrentThreadId();
			LeaveCriticalSection(&lock->lk);
			return TRUE;
		}
		LeaveCriticalSection(&lock->lk);
		LeaveCriticalSection(&lock->wlk);
		return FALSE;
	}
	return FALSE;
}

BOOL
rwlock_unlock(rwlock_t *lock)
{
	BOOL ret = FALSE;
	struct rwlock_reader_thread *rc;
	struct rwlock_reader_thread **prev;

	EnterCriticalSection(&lock->lk);
	if (lock->writers) {
		if (lock->writer == GetCurrentThreadId()) {
			lock->writers--;
			LeaveCriticalSection(&lock->lk);
			LeaveCriticalSection(&lock->wlk);
			return TRUE;
		}
		LeaveCriticalSection(&lock->lk);
		return FALSE;
	}
	if (lock->readers) {
		rc = find_self(lock, NULL);
		if (rc && rc->count) {
			rc->count--;
			lock->readers--;
			if (rc->count == 0) {
				*prev = rc->next;
				free(rc);
			}
			LeaveCriticalSection(&lock->lk);
			return TRUE;
		}
	}
	LeaveCriticalSection(&lock->lk);
	return FALSE;
}

BOOL
rwlock_destory(rwlock_t *lock)
{
	EnterCriticalSection(&lock->lk);
	if (lock->readers || lock->writers || lock->writers_waiting || lock->rthreads) {
		LeaveCriticalSection(&lock->lk);
		return FALSE;
	}
	LeaveCriticalSection(&lock->lk);
	DeleteCriticalSection(&lock->lk);
	DeleteCriticalSection(&lock->wlk);
	return TRUE;
}

#elif defined(__unix__)

// All macros...

#else

#error no rwlock wrapper for this platform

#endif
