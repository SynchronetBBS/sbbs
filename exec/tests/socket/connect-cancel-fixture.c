/* POSIX-only fault/delay interposer for connect-cancel.py. Not production code. */
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static _Thread_local bool delayed_worker;
static int (*real_close)(int);
static int tracked_fd = -1;
static bool fault_used;

static void mark(const char *key, const char *text)
{
	const char *path = getenv(key);
	if (!path) return;
	if (!real_close) real_close = dlsym(RTLD_NEXT, "close");
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd >= 0) { write(fd, text, strlen(text)); real_close(fd); }
}

int close(int fd)
{
	if (!real_close) real_close = dlsym(RTLD_NEXT, "close");
	if (tracked_fd < 0) {
		const char *path = getenv("CONNECT_TEST_TRACKED");
		if (path) {
			int file = open(path, O_RDONLY);
			if (file >= 0) {
				char value[32]; ssize_t count = read(file, value, sizeof(value) - 1);
				if (count > 0) { value[count] = 0; tracked_fd = atoi(value); }
				real_close(file);
			}
		}
	}
	if (fd == tracked_fd)
		mark("CONNECT_TEST_CLOSED", delayed_worker ? "worker" : "main");
	return real_close(fd);
}

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **result)
{
	int (*lookup)(const char *, const char *, const struct addrinfo *, struct addrinfo **) = dlsym(RTLD_NEXT, "getaddrinfo");
	if (node && strcmp(node, "socket-delay.invalid") == 0) {
		delayed_worker = true;
		mark("CONNECT_TEST_READY", "ready");
		const char *release = getenv("CONNECT_TEST_RELEASE");
		int tries;
		for (tries = 0; release && access(release, F_OK) && tries < 15000; tries++) usleep(1000);
		if (tries == 15000 || getenv("CONNECT_TEST_DNS_FAIL")) { *result = NULL; return EAI_NONAME; }
		node = "127.0.0.1";
	}
	return lookup(node, service, hints, result);
}

ssize_t send(int fd, const void *data, size_t size, int flags)
{
	ssize_t (*write_socket)(int, const void *, size_t, int) = dlsym(RTLD_NEXT, "send");
	ssize_t result = write_socket(fd, data, size, flags);
	if (delayed_worker && size == 2 * sizeof(int) && result == (ssize_t)size)
		mark("CONNECT_TEST_NOTIFIED", "reported");
	return result;
}

static bool inject(const char *kind)
{
	const char *fault = getenv("CONNECT_TEST_FAULT"), *arm = getenv("CONNECT_TEST_ARM");
	if (fault_used || !fault || strcmp(fault, kind) || !arm || access(arm, F_OK)) return false;
	fault_used = true;
	return true;
}

int socketpair(int domain, int type, int protocol, int pair[2])
{
	int (*make_pair)(int, int, int, int *) = dlsym(RTLD_NEXT, "socketpair");
	if (inject("pair")) { errno = EMFILE; return -1; }
	return make_pair(domain, type, protocol, pair);
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start)(void *), void *arg)
{
	int (*start_thread)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *) = dlsym(RTLD_NEXT, "pthread_create");
	if (inject("thread")) return EAGAIN;
	return start_thread(thread, attr, start, arg);
}
