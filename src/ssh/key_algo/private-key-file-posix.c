/* FreeBSD's native ACL capability identifiers need its default namespace.
 * This translation unit is inside the private key-file OS exception. */
#ifdef __FreeBSD__
#undef _POSIX_C_SOURCE
#undef _POSIX_SOURCE
#undef _XOPEN_SOURCE
#endif
#ifdef __APPLE__
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#endif
/* Native key-file streams. Each descriptor is non-inheritable on creation. */
#include "private-key-file.h"
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __FreeBSD__
#include <sys/acl.h>
#endif
#ifdef DSSH_TESTING
#include "dssh_test_key_file.h"
#define open(path, flags, mode) dssh_test_key_native_open(path, flags, mode)
#define fdopen(fd, mode) dssh_test_key_native_fdopen(fd, mode)
#define fchmod(fd, mode) dssh_test_key_native_fchmod(fd, mode)
#define ftruncate(fd, length) dssh_test_key_native_ftruncate(fd, length)
#define close(fd) dssh_test_key_native_close(fd)
#define fstat(fd, info) dssh_test_key_native_fstat(fd, info)
#define fcntl(fd, command, value) dssh_test_key_native_fcntl(fd, command, value)
#ifdef __FreeBSD__
#define fpathconf(fd, name) dssh_test_key_acl_capability(fd, name)
#define acl_from_text(text) dssh_test_key_acl_from_text(text)
#define acl_set_fd_np(fd, acl, type) dssh_test_key_acl_set(fd, acl, type)
#define acl_free(acl) dssh_test_key_acl_free(acl)
#endif
#ifdef __APPLE__
#define filesec_init() (dssh_test_key_darwin_permission(1) != 0 ? NULL : filesec_init())
#define filesec_set_property(sec, property, value) \
	(dssh_test_key_darwin_permission(2) != 0 ? -1 : filesec_set_property(sec, property, value))
#define fchmodx_np(fd, sec) (dssh_test_key_darwin_permission(3) != 0 ? -1 : fchmodx_np(fd, sec))
#define filesec_free(sec) (filesec_free(sec), dssh_test_key_darwin_retired())
#endif
#endif

static bool
private_permissions(int fd)
{
	if (fchmod(fd, S_IRUSR | S_IWUSR) != 0)
		return false;
#ifdef __FreeBSD__
	long nfs4 = fpathconf(fd, _PC_ACL_NFS4);
	if (nfs4 == 0)
		return true;
	if (nfs4 != 1) {
		if (nfs4 > 1)
			errno = EINVAL;
		return false;
	}
	/* Replace inherited/named entries rather than depending on a dataset's
	 * chmod policy. These are the native trivial-ACL permissions for 0600. */
	acl_t acl = acl_from_text("owner@:rw-p--aARWcCos::allow,"
	    "group@:------a-R-c--s::allow,everyone@:------a-R-c--s::allow");
	if (acl == NULL)
		return false;
	int ret = acl_set_fd_np(fd, acl, ACL_TYPE_NFS4);
	int saved_error = errno;
	if (acl_free(acl) != 0)
		return false;
	errno = saved_error;
	return ret == 0;
#elif defined(__APPLE__)
	/* Darwin's extended ACL is independent of the mode bits. Remove it on
	 * the same validated descriptor, preserving its existing owner/group. */
	filesec_t security = filesec_init();
	if (security == NULL)
		return false;
	int ret = filesec_set_property(security, FILESEC_ACL, _FILESEC_REMOVE_ACL);
	if (ret == 0)
		ret = fchmodx_np(fd, security);
	int saved_error = errno;
	filesec_free(security);
	errno = saved_error;
	return ret == 0;
#else
	return true;
#endif
}

/* Validate the acquired object before modifying permissions or contents.
 * The opening flags reject a final symlink and avoid waiting on a FIFO.
 * Clear the temporary status flag before handing a regular file to stdio. */
static bool
private_descriptor(int fd)
{
	struct stat info;
	if (fstat(fd, &info) != 0)
		return false;
	if (!S_ISREG(info.st_mode) || info.st_nlink != 1 || info.st_uid != geteuid()) {
		errno = EACCES;
		return false;
	}
	if (fcntl(fd, F_SETFL, 0) != 0)
		return false;
	return private_permissions(fd) && ftruncate(fd, 0) == 0;
}

/* Paths are validated by public wrappers. POSIX.1-2008 supplies O_CLOEXEC;
 * never create an inheritable descriptor and try to fix it afterward. */
static FILE *
open_stream(const char *path, int flags, mode_t mode, const char *access, bool private_output)
{
	int fd = open(path, flags | O_CLOEXEC, mode);
	if (fd < 0)
		return NULL;
	FILE *fp = NULL;
	if (!private_output || private_descriptor(fd))
		fp = fdopen(fd, access);
	if (fp == NULL) {
		int saved_error = errno;
		if (close(fd) != 0)
			return NULL;
		errno = saved_error;
	}
	return fp;
}

FILE *
dssh_private_key_open(const char *path)
{
	return open_stream(path, O_CREAT | O_WRONLY | O_NOFOLLOW | O_NONBLOCK | O_NOCTTY,
	    S_IRUSR | S_IWUSR, "wb", true);
}

DSSH_PRIVATE FILE *
dssh_key_file_open_read(const char *path)
{
	return open_stream(path, O_RDONLY, 0, "rb", false);
}

DSSH_PRIVATE FILE *
dssh_key_file_open_public(const char *path)
{
	return open_stream(path, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR
	    | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, "w", false);
}
