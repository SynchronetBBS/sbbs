#include "rwlockwrap.h"

#if defined(_WIN32)

#include <stdlib.h>
#include <stdio.h>

static struct rwlock_reader_thread *
find_self(rwlock_t *lock, struct rwlock_reader_thread ***prev)
{
	DWORD self = GetCurrentThreadId();
	struct rwlock_reader_thread *ret;

	if (prev)
		*prev = &lock->rthreads;
	for (ret = lock->rthreads; ret; ret = ret->next) {
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
	lock->zeror = CreateEvent(NULL, TRUE, TRUE, NULL);
	lock->zerow = CreateEvent(NULL, TRUE, TRUE, NULL);
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
		if (WaitForSingleObject(lock->zerow, INFINITE) != WAIT_OBJECT_0) {
			EnterCriticalSection(&lock->lk);
			continue;
		}
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
			ResetEvent(lock->zeror);
			LeaveCriticalSection(&lock->lk);
			LeaveCriticalSection(&lock->wlk);
			return TRUE;
		}
	}
	lock->readers++;
	ResetEvent(lock->zeror);
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
	if (rc->count || (lock->writers == 0 && lock->writers_waiting == 0)) {
		rc->count++;
		lock->readers++;
		ResetEvent(lock->zeror);
		ret = TRUE;
	}
	LeaveCriticalSection(&lock->lk);
	return ret;
}

BOOL
rwlock_wrlock(rwlock_t *lock)
{
	BOOL ret = FALSE;
	EnterCriticalSection(&lock->lk);
	lock->writers_waiting++;
	ResetEvent(lock->zerow);
	LeaveCriticalSection(&lock->lk);
	EnterCriticalSection(&lock->wlk);
	EnterCriticalSection(&lock->lk);
	// No recursion
	while (lock->readers) {
		LeaveCriticalSection(&lock->lk);
		LeaveCriticalSection(&lock->wlk);
		if (WaitForSingleObject(lock->zeror, INFINITE) != WAIT_OBJECT_0) {
			EnterCriticalSection(&lock->lk);
			continue;
		}
		EnterCriticalSection(&lock->wlk);
		EnterCriticalSection(&lock->lk);
	}
	if (lock->writers) {
		lock->writers_waiting--;
		ret = FALSE;
	}
	else {
		lock->writers_waiting--;
		lock->writers++;
		ResetEvent(lock->zerow);
		lock->writer = GetCurrentThreadId();
		ret = TRUE;
	}
	LeaveCriticalSection(&lock->lk);
	LeaveCriticalSection(&lock->wlk);
	return ret;
}

BOOL
rwlock_trywrlock(rwlock_t *lock)
{
	if (TryEnterCriticalSection(&lock->wlk)) {
		EnterCriticalSection(&lock->lk);
		// Prevent recursing on writer locks
		if (lock->readers == 0 && lock->writers == 0) {
			lock->writers++;
			ResetEvent(lock->zerow);
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
			if ((lock->writers_waiting + lock->writers) == 0)
				SetEvent(lock->zerow);
			LeaveCriticalSection(&lock->lk);
			LeaveCriticalSection(&lock->wlk);
			return TRUE;
		}
		LeaveCriticalSection(&lock->lk);
		return FALSE;
	}
	if (lock->readers) {
		rc = find_self(lock, &prev);
		if (rc && rc->count) {
			rc->count--;
			lock->readers--;
			if (rc->count == 0) {
				*prev = rc->next;
				free(rc);
			}
			if (lock->readers == 0)
				SetEvent(lock->zeror);
			LeaveCriticalSection(&lock->lk);
			return TRUE;
		}
	}
	LeaveCriticalSection(&lock->lk);
	return FALSE;
}

BOOL
rwlock_destroy(rwlock_t *lock)
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
