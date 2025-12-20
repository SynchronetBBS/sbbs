/* wraptest.c */

/* Verification of cross-platform development wrappers */

#include <time.h>   /* ctime */

#include "genwrap.h"
#include "conwrap.h"
#include "dirwrap.h"
#include "filewrap.h"
#include "rwlockwrap.h"
#include "sockwrap.h"
#include "threadwrap.h"
#include "xpbeep.h"
#include "xpendian.h"

#define LOCK_FNAME  "test.fil"
#define LOCK_OFFSET 0
#define LOCK_LEN    4

static void getkey(void);
static void sem_test_thread(void* arg);
static void sem_test_thread_block(void* arg);
static void sleep_test_thread(void* arg);
static void sopen_test_thread(void* arg);
static void sopen_child_thread(void* arg);
static void lock_test_thread(void* arg);
static void rwlock_rdlock_thread(void *arg);
static void rwlock_wrlock_thread(void *arg);

typedef struct {
	sem_t parent_sem;
	sem_t child_sem;
} thread_data_t;

int main()
{
	char          str[128];
	char          compiler[128];
	char          fpath[MAX_PATH + 1];
	char*         path = ".";
	char*         glob_pattern = "*wrap*";
	intptr_t      i;
	int           ch;
	uint          u;
	time_t        t;
	glob_t        g;
	DIR*          dir;
	DIRENT*       dirent;
	thread_data_t thread_data;
	int           fd;
	int           fd2;
	int           canrelock = 0;
	clock_t       ticks;

	/* Show platform details */
	DESCRIBE_COMPILER(compiler);
	printf("%-15s: %s\n", "Platform", PLATFORM_DESC);
	printf("%-15s: %s\n", "Version", os_version(str, sizeof(str)));
	printf("%-15s: %s\n", "Compiler", compiler);
	printf("%-15s: %ld\n", "Random Number", xp_random(1000));

	printf("Testing rwlocks...\n");
	do {
		rwlock_t lock;
		if (!rwlock_init(&lock)) {
			printf("rwlock_init() failed (%d)\n", errno);
			continue;
		}
		// Start two copies of the rdlock thread...
		if (_beginthread(rwlock_rdlock_thread, 0, &lock) == -1UL) {
			printf("Unable to start rwlock_rdlock_thread\n");
			continue;
		}
		if (_beginthread(rwlock_rdlock_thread, 0, &lock) == -1UL) {
			printf("Unable to start rwlock_rdlock_thread\n");
			continue;
		}
		rwlock_wrlock_thread(&lock);
		printf("Grabbing wrlock to clean up\n");
		if (rwlock_trywrlock(&lock)) {
			rwlock_unlock(&lock);
		}
		else {
			printf("Unable to grab lock after wrlock thread complete!\n");
		}
		printf("Destroying rwlock\n");
		if (!rwlock_destroy(&lock)) {
			printf("Unable to destroy rwlock\n");
		}
	} while (0);

	for (i = 0; i < 3; i++) {
		if (_beginthread(
				sopen_child_thread  /* entry point */
				, 0                 /* stack size (0=auto) */
				, (void*)i          /* data */
				) == (unsigned long)-1)
			printf("_beginthread failed\n");
		else
			SLEEP(1);
	}
	printf("Waiting for all sopen_child_threads to close...\n");
	SLEEP(5000);    /* wait for all threads to quit */

	/* Exclusive sopen test */
	printf("\nsopen() test\n");
	if ((fd = sopen(LOCK_FNAME, O_RDWR | O_CREAT, SH_DENYRW, S_IREAD | S_IWRITE)) == -1) {
		perror(LOCK_FNAME);
		return errno;
	}
	printf("%s is opened with an exclusive (read/write) lock\n", LOCK_FNAME);
	getkey();
	if (_beginthread(
			sopen_test_thread /* entry point */
			, 0             /* stack size (0=auto) */
			, NULL          /* data */
			) == (unsigned long)-1)
		printf("_beginthread failed\n");
	else
		SLEEP(1000);
	close(fd);

	/* sopen()/lock test */
	printf("\nlock() test\n");
	if ((fd = sopen(LOCK_FNAME, O_RDWR | O_CREAT, SH_DENYNO, S_IREAD | S_IWRITE)) == -1) {
		perror(LOCK_FNAME);
		return errno;
	}
	write(fd, "lock testing\n", LOCK_LEN);
	if (lock(fd, LOCK_OFFSET, LOCK_LEN) == 0)
		printf("lock() succeeds\n");
	else
		printf("!FAILURE: lock() non-functional (or file already locked)\n");
	if (lock(fd, LOCK_OFFSET, LOCK_LEN) == 0)  {
		printf("!FAILURE: Subsequent lock of region was allowed (will skip some tests)\n");
		canrelock = 1;
	}

	if (_beginthread(
			lock_test_thread /* entry point */
			, 0             /* stack size (0=auto) */
			, NULL          /* data */
			) == (unsigned long)-1)
		printf("_beginthread failed\n");
	else
		SLEEP(1000);
	if (canrelock)
		printf("?? Skipping some tests due to inability to detect own locks\n");
	else  {
		if (lock(fd, LOCK_OFFSET, LOCK_LEN))
			printf("Locks in first thread survive open()/close() in other thread\n");
		else
			printf("!FAILURE: lock() in first thread lost by open()/close() in other thread\n");
		if (lock(fd, LOCK_OFFSET + LOCK_LEN + 1, LOCK_LEN))
			printf("!FAILURE: file locking\n");
		else
			printf("Record locking\n");
	}
	if ((fd2 = sopen(LOCK_FNAME, O_RDWR, SH_DENYRW)) == -1) {
		printf("Cannot reopen SH_DENYRW while lock is held\n");
		close(fd2);
	}
	else  {
		printf("!FAILURE: can reopen SH_DENYRW while lock is held\n");
	}
	if (unlock(fd, LOCK_OFFSET, LOCK_LEN))
		printf("!FAILURE: unlock() non-functional\n");
	if (lock(fd, LOCK_OFFSET + LOCK_LEN + 1, LOCK_LEN))
		printf("Cannot re-lock after non-overlapping unlock()\n");
	else
		printf("!FAILURE: can re-lock after non-overlappping unlock()\n");
	if (lock(fd, LOCK_OFFSET, LOCK_LEN))
		printf("!FAILURE: cannot re-lock unlocked area\n");
	close(fd);

	/* getch test */
	printf("\ngetch() test (ESC to continue)\n");
	do {
		ch = getch();
		printf("getch() returned %d\n", ch);
	} while (ch != ESC);

	/* kbhit test */
	printf("\nkbhit() test (any key to continue)\n");
	while (!kbhit()) {
		printf(".");
		fflush(stdout);
		SLEEP(500);
	}
	getch();    /* remove character from keyboard buffer */

	/* BEEP test */
	printf("\nBEEP() test\n");
	getkey();
	for (i = 750; i > 250; i -= 5)
		BEEP(i, 15);
	for (; i < 1000; i += 5)
		BEEP(i, 15);

	/* SLEEP test */
	printf("\nSLEEP(5 second) test\n");
	getkey();
	t = time(NULL);
	printf("sleeping... ");
	fflush(stdout);
	ticks = msclock();
	SLEEP(5000);
	printf("slept %ld seconds (%ld according to msclock)\n", time(NULL) - t, (msclock() - ticks) / MSCLOCKS_PER_SEC);

	/* Thread SLEEP test */
	printf("\nThread SLEEP(5 second) test\n");
	getkey();
	i = 0;
	if (_beginthread(
			sleep_test_thread /* entry point */
			, 0             /* stack size (0=auto) */
			, &i            /* data */
			) == (unsigned long)-1)
		printf("_beginthread failed\n");
	else {
		SLEEP(1);           /* yield to child thread */
		while (i == 0) {
			printf(".");
			fflush(stdout);
			SLEEP(1000);
		}
	}

	/* glob test */
	printf("\nglob(%s) test\n", glob_pattern);
	getkey();
	i = glob(glob_pattern, GLOB_MARK, NULL, &g);
	if (i == 0) {
		for (u = 0; u < g.gl_pathc; u++)
			printf("%s\n", g.gl_pathv[u]);
		globfree(&g);
	} else
		printf("glob(%s) returned %d\n", glob_pattern, (int)i);

	/* opendir (and other directory functions) test */
	printf("\nopendir(%s) test\n", path);
	getkey();
	printf("\nDirectory of %s\n\n", FULLPATH(fpath, path, sizeof(fpath)));
	dir = opendir(path);
	while (dir != NULL && (dirent = readdir(dir)) != NULL) {
		t = fdate(dirent->d_name);
		printf("%.24s %10" PRIuOFF "  %06o  %s%c\n"
		       , ctime(&t)
		       , flength(dirent->d_name)
		       , getfattr(dirent->d_name)
		       , dirent->d_name
		       , isdir(dirent->d_name) ? '/':0
		       );
	}
	if (dir != NULL)
		closedir(dir);
	printf("\nFree disk space: %" PRIu64 " kbytes\n", getfreediskspace(path, 1024));

	/* Thread (and inter-process communication) test */
	printf("\nSemaphore test\n");
	getkey();
	if (sem_init(&thread_data.parent_sem
	             , 0 /* shared between processes */
	             , 0 /* initial count */
	             )) {
		printf("sem_init failed\n");
	}
	if (sem_init(&thread_data.child_sem
	             , 0 /* shared between processes */
	             , 0 /* initial count */
	             )) {
		printf("sem_init failed\n");
	}
	if (_beginthread(
			sem_test_thread /* entry point */
			, 0             /* stack size (0=auto) */
			, &thread_data  /* data */
			) == (unsigned long)-1)
		printf("_beginthread failed\n");
	else {
		sem_wait(&thread_data.child_sem);   /* wait for thread to begin */
		for (i = 0; i < 10; i++) {
			printf("<parent>");
			sem_post(&thread_data.parent_sem);
			sem_wait(&thread_data.child_sem);
		}
		sem_wait(&thread_data.child_sem);   /* wait for thread to end */
	}
	sem_destroy(&thread_data.parent_sem);
	sem_destroy(&thread_data.child_sem);

	printf("\nSemaphore blocking test\n");
	getkey();
	sem_init(&thread_data.parent_sem
	         , 0 /* shared between processes */
	         , 0 /* initial count */
	         );
	sem_init(&thread_data.child_sem
	         , 0 /* shared between processes */
	         , 0 /* initial count */
	         );
	if (_beginthread(
			sem_test_thread_block /* entry point */
			, 0             /* stack size (0=auto) */
			, &thread_data  /* data */
			) == (unsigned long)-1)
		printf("_beginthread failed\n");
	else {
		sem_wait(&thread_data.child_sem);   /* wait for thread to begin */
		for (i = 0; i < 10; i++) {
			printf("<parent>");
			SLEEP(5000);
			sem_post(&thread_data.parent_sem);
			sem_wait(&thread_data.child_sem);
		}
		sem_wait(&thread_data.child_sem);   /* wait for thread to end */
	}
	printf("\nsem_trywait_block test...");
	t = time(NULL);
	sem_trywait_block(&thread_data.parent_sem, 5000);
	printf("\ntimed-out after %ld seconds (should be 5 seconds)\n", time(NULL) - t);
	sem_destroy(&thread_data.parent_sem);
	sem_destroy(&thread_data.child_sem);
	printf("\nendian check...");
	memcpy(&i, "\x01\x02\x03\x04", 4);
	if (LE_LONG(i) == 67305985) {
		printf("OK!\n");
	}
	else {
		printf("FAILED!\n");
	}
	return 0;
}

static void getkey(void)
{
	printf("Hit any key to continue...");
	fflush(stdout);
	getch();
	printf("\r%30s\r", "");
	fflush(stdout);
}

static void sleep_test_thread(void* arg)
{
	time_t t = time(NULL);
	printf("child sleeping");
	fflush(stdout);
	SLEEP(5000);
	printf("\nchild awake after %ld seconds\n", time(NULL) - t);

	*(int*)arg = 1;   /* signal parent: we're done */
}

static void sem_test_thread(void* arg)
{
	ulong          i;
	thread_data_t* data = (thread_data_t*)arg;

	printf("sem_test_thread entry\n");
	sem_post(&data->child_sem);     /* signal parent: we've started */

	for (i = 0; i < 10; i++) {
		sem_wait(&data->parent_sem);
		printf(" <child>\n");
		sem_post(&data->child_sem);
	}

	printf("sem_test_thread exit\n");
	sem_post(&data->child_sem);     /* signal parent: we're done */
}

static void sem_test_thread_block(void* arg)
{
	ulong          i;
	thread_data_t* data = (thread_data_t*)arg;

	printf("sem_test_thread_block entry\n");
	sem_post(&data->child_sem);     /* signal parent: we've started */

	for (i = 0; i < 10; i++) {
		if (sem_trywait_block(&data->parent_sem, 500))  {
			printf(" sem_trywait_block() timed out");
			sem_wait(&data->parent_sem);
		}
		else  {
			printf(" FAILURE: Didn't block");
		}
		printf(" <child>\n");
		sem_post(&data->child_sem);
	}

	printf("sem_test_thread_block exit\n");
	sem_post(&data->child_sem);     /* signal parent: we're done */
}

static void lock_test_thread(void* arg)
{
	int fd;

	fd = sopen(LOCK_FNAME, O_RDWR, SH_DENYNO);
	if (lock(fd, LOCK_OFFSET, LOCK_LEN) == 0)
		printf("!FAILURE: Lock not effective between threads\n");
	else
		printf("Locks effective between threads\n");
	close(fd);
	fd = sopen(LOCK_FNAME, O_RDWR, SH_DENYNO);
	if (lock(fd, LOCK_OFFSET, LOCK_LEN))
		printf("Locks survive file open()/close() in other thread\n");
	else
		printf("!FAILURE: Locks do not survive file open()/close() in other thread\n");
	close(fd);
}

static void sopen_test_thread(void* arg)
{
	int fd;

	if ((fd = sopen(LOCK_FNAME, O_RDWR, SH_DENYWR)) != -1)
		printf("!FAILURE: allowed to reopen with SH_DENYWR\n");
	else if ((fd = sopen(LOCK_FNAME, O_RDWR, SH_DENYRW)) != -1)
		printf("!FAILURE: allowed to reopen with SH_DENYRW\n");
	else
		printf("reopen disallowed\n");

	if (fd != -1)
		close(fd);
}

static void sopen_child_thread(void* arg)
{
	int fd;
	int instance = (intptr_t)arg;

	printf("sopen_child_thread: %d begin\n", instance);
	if ((fd = sopen(LOCK_FNAME, O_RDWR, SH_DENYRW)) != -1) {
		if (arg)
			printf("!FAILURE: was able to reopen in child thread\n");
		else {
			SLEEP(5000);
		}
		close(fd);
	} else if (arg == 0)
		perror(LOCK_FNAME);
	printf("sopen_child_thread: %d end\n", instance);
}

static void rwlock_rdlock_thread(void *arg)
{
	rwlock_t *lock = arg;
	int       locks = 0;

	// Grab the lock three times...
	for (locks = 0; locks < 3; locks++) {
		if (!rwlock_rdlock(lock)) {
			printf("Failed to obtain rdlock #%d\n", locks + 1);
			break;
		}
	}
	// Try to grab the lock (should succeed)
	if (rwlock_tryrdlock(lock)) {
		locks++;
	}
	else {
		printf("tryrdlock failed\n");
	}
	printf("Obtained %d locks (should be 4)\n", locks);
	for (; locks > 0; locks--) {
		SLEEP(1000);
		// Ensure recursion works when writers are waiting...
		if (rwlock_rdlock(lock)) {
			if (!rwlock_unlock(lock)) {
				printf("Failed to unlock recursive rdlock #%d with write waiter\n", locks + 1);
			}
		}
		else {
			printf("Failed to lock recursive rdlock #%d with write waiter\n", locks + 1);
		}
		if (!rwlock_unlock(lock)) {
			printf("Failed to unlock rdlock #%d\n", locks);
		}
	}
	SLEEP(1000);
	// Try to grab the lock (should fail)
	if (rwlock_tryrdlock(lock)) {
		printf("tryrdlock incorrectly succeeded\n");
		rwlock_unlock(lock);
	}
	printf("Waiting for lock\n");
	if (rwlock_rdlock(lock)) {
		locks++;
	}
	else {
		printf("timedrdlock failed\n");
	}
	printf("Wait done\n");
	SLEEP(1000);
	for (; locks > 0; locks--) {
		rwlock_unlock(lock);
	}
}

static void rwlock_wrlock_thread(void *arg)
{
	rwlock_t *lock = arg;

	// Sleep to allow readers to grab locks...
	SLEEP(1000);
	// Try to grab lock (should fail)
	if (rwlock_trywrlock(lock)) {
		printf("trywrlock() incorrectly succeeded\n");
		rwlock_unlock(lock);
	}
	// Try to grab lock (should succeed)
	printf("Waiting for write lock\n");
	if (!rwlock_wrlock(lock)) {
		printf("wrlock() failed\n");
	}
	printf("wrlock Wait done\n");
	// Enjoy the lock for a bit...
	SLEEP(2000);
	printf("wrlock unlocking\n");
	rwlock_unlock(lock);
	SLEEP(100);
	// Wait for the lock (should succeed)
	printf("Waiting wrlock\n");
	if (!rwlock_wrlock(lock)) {
		printf("wrlock failed\n");
	}
	else {
		SLEEP(1000);
		rwlock_unlock(lock);
	}
	printf("wrlock wait done\n");
}

/* End of wraptest.c */
