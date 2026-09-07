#include "rwlockwrap.h"

#if defined(_WIN32)

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

static struct rwlock_reader_thread *
find_self(rwlock_t *lock, struct rwlock_reader_thread ***prev, bool create)
{
	DWORD                        self = GetCurrentThreadId();
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
	if (!create)
		return NULL;
	ret = calloc(1, sizeof(*ret));
	if (ret == NULL)
		return ret;
	ret->next = lock->rthreads;
	ret->id = self;
	lock->rthreads = ret;
	return ret;
}

static void
remove_reader(rwlock_t *lock, struct rwlock_reader_thread *reader)
{
	struct rwlock_reader_thread **entry;

	for (entry = &lock->rthreads; *entry != NULL; entry = &(*entry)->next) {
		if (*entry == reader) {
			*entry = reader->next;
			free(reader);
			return;
		}
	}
}

bool
rwlock_init(rwlock_t *lock)
{
	if (lock == NULL)
		return false;
	if (!InitializeCriticalSectionAndSpinCount(&lock->lk, 0))
		return false;
	if (!InitializeCriticalSectionAndSpinCount(&lock->wlk, 0)) {
		DeleteCriticalSection(&lock->lk);
		return false;
	}
	lock->zeror = CreateEvent(NULL, true, true, NULL);
	if (lock->zeror == NULL) {
		DeleteCriticalSection(&lock->wlk);
		DeleteCriticalSection(&lock->lk);
		return false;
	}
	lock->zerow = CreateEvent(NULL, true, true, NULL);
	if (lock->zerow == NULL) {
		CloseHandle(lock->zeror);
		DeleteCriticalSection(&lock->wlk);
		DeleteCriticalSection(&lock->lk);
		return false;
	}
	lock->readers = 0;
	lock->writers = 0;
	lock->writers_waiting = 0;
	lock->writer = (DWORD)-1;
	lock->rthreads = NULL;
	return true;
}

bool
rwlock_rdlock(rwlock_t *lock)
{
	struct rwlock_reader_thread *rc;

	EnterCriticalSection(&lock->lk);
	rc = find_self(lock, NULL, true);
	if (rc == NULL) {
		LeaveCriticalSection(&lock->lk);
		return false;
	}
	while (rc->count == 0 && (lock->writers || lock->writers_waiting)) {
		LeaveCriticalSection(&lock->lk);
		if (WaitForSingleObject(lock->zerow, INFINITE) != WAIT_OBJECT_0) {
			EnterCriticalSection(&lock->lk);
			remove_reader(lock, rc);
			LeaveCriticalSection(&lock->lk);
			return false;
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
			if (lock->readers == UINT_MAX || rc->count == UINT_MAX) {
				remove_reader(lock, rc);
				LeaveCriticalSection(&lock->lk);
				LeaveCriticalSection(&lock->wlk);
				return false;
			}
			lock->readers++;
			rc->count++;
			ResetEvent(lock->zeror);
			LeaveCriticalSection(&lock->lk);
			LeaveCriticalSection(&lock->wlk);
			return true;
		}
	}
	if (lock->readers == UINT_MAX || rc->count == UINT_MAX) {
		if (rc->count == 0)
			remove_reader(lock, rc);
		LeaveCriticalSection(&lock->lk);
		return false;
	}
	lock->readers++;
	ResetEvent(lock->zeror);
	rc->count++;
	LeaveCriticalSection(&lock->lk);
	return true;
}

bool
rwlock_tryrdlock(rwlock_t *lock)
{
	bool                         ret = false;
	struct rwlock_reader_thread *rc;

	EnterCriticalSection(&lock->lk);
	rc = find_self(lock, NULL, true);
	if (rc == NULL) {
		LeaveCriticalSection(&lock->lk);
		return false;
	}
	if ((rc->count || (lock->writers == 0 && lock->writers_waiting == 0))
	    && rc->count < UINT_MAX && lock->readers < UINT_MAX) {
		rc->count++;
		lock->readers++;
		ResetEvent(lock->zeror);
		ret = true;
	}
	if (!ret && rc->count == 0)
		remove_reader(lock, rc);
	LeaveCriticalSection(&lock->lk);
	return ret;
}

bool
rwlock_wrlock(rwlock_t *lock)
{
	bool ret = false;
	EnterCriticalSection(&lock->lk);
	if (lock->writers_waiting == UINT_MAX) {
		LeaveCriticalSection(&lock->lk);
		return false;
	}
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
			lock->writers_waiting--;
			if (lock->writers_waiting == 0 && lock->writers == 0)
				SetEvent(lock->zerow);
			LeaveCriticalSection(&lock->lk);
			return false;
		}
		EnterCriticalSection(&lock->wlk);
		EnterCriticalSection(&lock->lk);
	}
	if (lock->writers) {
		lock->writers_waiting--;
		ret = false;
	}
	else {
		lock->writers_waiting--;
		lock->writers++;
		ResetEvent(lock->zerow);
		lock->writer = GetCurrentThreadId();
		ret = true;
	}
	LeaveCriticalSection(&lock->lk);
	if (!ret)
		LeaveCriticalSection(&lock->wlk);
	return ret;
}

bool
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
			return true;
		}
		LeaveCriticalSection(&lock->lk);
		LeaveCriticalSection(&lock->wlk);
		return false;
	}
	return false;
}

bool
rwlock_unlock(rwlock_t *lock)
{
	struct rwlock_reader_thread * rc;
	struct rwlock_reader_thread **prev;

	EnterCriticalSection(&lock->lk);
	if (lock->writers) {
		if (lock->writer == GetCurrentThreadId()) {
			lock->writers--;
			if (lock->writers_waiting == 0 && lock->writers == 0)
				SetEvent(lock->zerow);
			LeaveCriticalSection(&lock->lk);
			LeaveCriticalSection(&lock->wlk);
			return true;
		}
		LeaveCriticalSection(&lock->lk);
		return false;
	}
	if (lock->readers) {
		rc = find_self(lock, &prev, false);
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
			return true;
		}
	}
	LeaveCriticalSection(&lock->lk);
	return false;
}

bool
rwlock_destroy(rwlock_t *lock)
{
	bool ret = true;

	if (lock == NULL)
		return false;
	EnterCriticalSection(&lock->lk);
	if (lock->readers || lock->writers || lock->writers_waiting || lock->rthreads) {
		LeaveCriticalSection(&lock->lk);
		return false;
	}
	LeaveCriticalSection(&lock->lk);
	if (!CloseHandle(lock->zeror))
		ret = false;
	if (!CloseHandle(lock->zerow))
		ret = false;
	lock->zeror = NULL;
	lock->zerow = NULL;
	DeleteCriticalSection(&lock->lk);
	DeleteCriticalSection(&lock->wlk);
	return ret;
}

#elif defined(__unix__)

// All static inline functions

#else

#error no rwlock wrapper for this platform

#endif
