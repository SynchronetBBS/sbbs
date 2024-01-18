#include "rwlockwrap.h"

#if defined(__BORLANDC__)

// Do nothing...

#elif defined(_WIN32)

BOOL
rwlock_init(rwlock_t *lock)
{
	InitializeCriticalSection(&lock->lk);
	InitializeCriticalSection(&lock->wlk);
	lock->readers = 0;
	lock->writers = 0;
	lock->writers_waiting = 0;
	lock->writer = (DWORD)-1;
}

BOOL
rwlock_rdlock(rwlock_t *lock)
{
	EnterCriticalSection(&lock->lk);
	while(lock->writers || lock->writers_waiting) {
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
			LeaveCriticalSection(&lock->lk);
			LeaveCriticalSection(&lock->wlk);
			return TRUE;
		}
	}
	lock->readers++;
	LeaveCriticalSection(&lock->lk);
	return TRUE;
}

BOOL
rwlock_tryrdlock(rwlock_t *lock)
{
	BOOL ret = FALSE;

	EnterCriticalSection(&lock->lk);
	if (lock->writers == 0 && lock->writers_waiting == 0) {
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
		lock->readers--;
		return TRUE;
	}
	return FALSE;
}

#elif defined(__unix__)

// All macros...

#else

#error no rwlock wrapper for this platform

#endif
