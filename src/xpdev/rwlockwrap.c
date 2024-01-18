#include "rwlockwrap.h"

#if defined(__BORLANDC__)

// Do nothing...

#elif defined(_WIN32)

bool
rwlock_init(rwlock_t *lock)
{
	InitializeCriticalSection(&lock->lk);
	InitializeCriticalSection(&lock->wlk);
	readers = 0;
	writers = 0;
	writers_waiting = 0;
	writer = (DWORD)-1;
}

bool
rwlock_rdlock(rwlock_t *lock)
{
	DWORD obj;

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
			return true;
		}
	}
	lock->readers++;
	LeaveCriticalSection(&lock->lk);
	return true;
}

bool
rwlock_tryrdlock(rwlock_t *lock)
{
	bool ret = false;

	EnterCriticalSection(&lock->lk);
	if (lock->writers == 0 && lock->writers_waiting == 0) {
		lock->readers++;
		ret = true;
	}
	LeaveCriticalSection(&lock->lk);
	return ret;
}

bool
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
		return true;
	}
	LeaveCriticalSection(&lock->lk);
	LeaveCriticalSection(&lock->wlk);
	return false;
}

bool
rwlock_trywrlock(rwlock_t *lock)
{
	if (TryEnterCriticalSection(&lock->wlk)) {
		EnterCriticalSection(&lock->lk);
		// Prevent recursing on writer locks
		if (lock->writers == 0) {
			lock->writers++;
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
	bool ret = false;
	EnterCriticalSection(&lock->lk);
	if (lock->writers) {
		if (lock->writer == GetCurrentThreadId()) {
			lock->writers--;
			LeaveCriticalSection(&lock->lk);
			LeaveCriticalSection(&lock->wlk);
			return true;
		}
		LeaveCriticalSection(&lock->lk);
		return false;
	}
	if (lock->readers) {
		lock->readers--;
		return true;
	}
	return false;
}

#elif defined(__unix__)

// All macros...

#else

#error no rwlock wrapper for this platform

#endif
