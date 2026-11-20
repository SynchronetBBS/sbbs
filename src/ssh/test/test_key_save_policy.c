/* This native-only fixture also uses FreeBSD ACL capability identifiers. */
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
/* Selected direct-overwrite contract, using only private nonsensitive fixtures. */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/acl.h>
#include <uuid/uuid.h>
#endif
#include "dssh_test.h"
#include "dssh_test_internal.h"
#include "dssh_test_alloc.h"
#include "dssh_test_ossl.h"
#include "deucessh-algorithms.h"

struct policy_ops {
	int (*reg)(void);
	int (*load)(const char *, dssh_pem_password_cb, void *);
	int (*save)(const char *, dssh_pem_password_cb, void *);
	const char *name, *cache;
};
static const struct policy_ops operations[] = {
	{dssh_register_ssh_ed25519, dssh_ed25519_load_key_file, dssh_ed25519_save_key_file,
	    "ssh-ed25519", "DSSH_TEST_ED25519_KEY"},
	{dssh_register_rsa_sha2_256, dssh_rsa_sha2_256_load_key_file, dssh_rsa_sha2_256_save_key_file,
	    "rsa-sha2-256", "DSSH_TEST_RSA_KEY"},
	{dssh_register_rsa_sha2_512, dssh_rsa_sha2_512_load_key_file, dssh_rsa_sha2_512_save_key_file,
	    "rsa-sha2-512", "DSSH_TEST_RSA_KEY"},
};
static const char marker[] = "private output policy fixture";
static bool policy_workspace;

#ifdef __APPLE__
static bool
darwin_acl_prepare(const char *path)
{
	int fd = open(path, O_RDWR | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0)
		return false;
	acl_t acl = acl_init(1);
	acl_entry_t entry;
	acl_permset_t permissions;
	uuid_t principal;
	bool valid = acl != NULL && fchmod(fd, 0600) == 0
	    && uuid_parse("FFFFEEEE-DDDD-CCCC-BBBB-AAAA0000FFFE", principal) == 0
	    && acl_create_entry(&acl, &entry) == 0 && acl_set_tag_type(entry, ACL_EXTENDED_ALLOW) == 0
	    && acl_set_qualifier(entry, principal) == 0 && acl_get_permset(entry, &permissions) == 0
	    && acl_add_perm(permissions, ACL_READ_DATA) == 0 && acl_set_fd(fd, acl) == 0;
	if (acl != NULL && acl_free(acl) != 0)
		valid = false;
	if (close(fd) != 0)
		valid = false;
	return valid;
}

static int
darwin_acl_state(const char *path)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0)
		return -1;
	struct stat info;
	int result = -1;
	errno = 0;
	acl_t acl = acl_get_fd(fd);
	bool empty = acl == NULL && errno == ENOENT;
	if (acl != NULL) {
		acl_entry_t entry;
		errno = 0;
		int ret = acl_get_entry(acl, ACL_FIRST_ENTRY, &entry);
		if (ret == 0)
			result = 0;
		else if (ret == -1 && errno == EINVAL)
			empty = true;
	}
	if (empty && fstat(fd, &info) == 0)
		result = (info.st_mode & 0777) == 0600 && info.st_uid == geteuid() ? 1 : 0;
	if (acl != NULL && acl_free(acl) != 0)
		result = -1;
	if (close(fd) != 0)
		result = -1;
	return result;
}
#endif

#ifdef __FreeBSD__
static bool
acl_prepare(const char *path)
{
	int fd = open(path, O_RDWR | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0)
		return false;
	bool valid = fchmod(fd, 0600) == 0 && fpathconf(fd, _PC_ACL_NFS4) == 1;
	acl_t acl = NULL;
	if (valid) {
		acl = acl_from_text("user:65534:r::allow,owner@:rw-p--aARWcCos::allow,"
		    "group@:------a-R-c--s::allow,everyone@:------a-R-c--s::allow");
		valid = acl != NULL && acl_set_fd_np(fd, acl, ACL_TYPE_NFS4) == 0;
	}
	if (acl != NULL && acl_free(acl) != 0)
		valid = false;
	if (close(fd) != 0)
		valid = false;
	return valid;
}

/* -1 means inspection failed, 0 nontrivial access, 1 owner-private. */
static int
acl_state(const char *path)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0)
		return -1;
	int result = -1, trivial = 0;
	struct stat info;
	acl_t acl = acl_get_fd_np(fd, ACL_TYPE_NFS4);
	if (acl != NULL && fstat(fd, &info) == 0 && acl_is_trivial_np(acl, &trivial) == 0)
		result = trivial == 1 && (info.st_mode & 0777) == 0600 && info.st_uid == geteuid() ? 1 : 0;
	if (acl != NULL && acl_free(acl) != 0)
		result = -1;
	if (close(fd) != 0)
		result = -1;
	return result;
}
#endif


struct password_state {
	unsigned int calls;
	int writing;
	bool valid;
};
static int
password(char *buffer, int capacity, int writing, void *data)
{
	struct password_state *state = data;
	if (buffer == NULL || capacity < 3 || writing != state->writing || state->calls == UINT_MAX) {
		state->valid = false;
		return -1;
	}
	state->calls++;
	memcpy(buffer, "pem", 3);
	return 3;
}
static bool
join(char *out, const char *root, const char *name)
{
	int count = snprintf(out, 128, "%s/%s", root, name);
	return count > 0 && count < 128;
}
static bool
seed(const char *path)
{
	FILE *fp = fopen(path, "wb");
	if (fp == NULL)
		return false;
	bool valid = fwrite(marker, 1, sizeof(marker), fp) == sizeof(marker);
	if (fclose(fp) != 0)
		valid = false;
	return chmod(path, 0640) == 0 && valid;
}
static bool
unchanged_bytes(const char *path)
{
	FILE *fp = fopen(path, "rb");
	if (fp == NULL)
		return false;
	char observed[sizeof(marker)];
	bool valid = fread(observed, 1, sizeof(observed), fp) == sizeof(marker)
	    && memcmp(observed, marker, sizeof(marker)) == 0 && fgetc(fp) == EOF && ferror(fp) == 0;
	if (fclose(fp) != 0)
		valid = false;
	return valid;
}
static bool
same_object(const struct stat *before, const struct stat *after)
{
	return before->st_dev == after->st_dev && before->st_ino == after->st_ino
	    && before->st_mode == after->st_mode && before->st_uid == after->st_uid
	    && before->st_gid == after->st_gid && before->st_nlink == after->st_nlink
	    && before->st_size == after->st_size;
}

/* 0 existing, 1 new, 2 writable file/read-only parent, 3 native parent symlink;
 * 4 final symlink, 5 multiple links, 6 directory, 7 FIFO with peer,
 * 8 dangling final symlink, 9 FIFO without peer. Only 0..3 may succeed. */
static bool
policy_case(const struct policy_ops *ops, unsigned int kind, bool encrypted)
{
	char root[] = "/tmp/dssh-save-policy-XXXXXX";
	char actual[128] = {0}, alias[128] = {0}, inner[128] = {0}, chosen[128] = {0};
	bool created = false, alias_created = false, inner_created = false, had_target = false;
	bool valid = false, retired = false;
	bool accepted = kind < 4 || kind == 10 || kind == 15 || kind == 20;
	bool acl_case = kind >= 10;
	int acl_after = -2;
#ifdef __FreeBSD__
	struct dssh_test_key_acl_report acl_report = {0};
#endif
#ifdef __APPLE__
	struct dssh_test_key_darwin_report darwin_report = {0};
#endif
	int peer = -1, status = DSSH_ERROR_INIT;
	struct stat before, after, alias_before, alias_after;
	struct dssh_test_key_open_report report = {0};
	struct dssh_test_key_stream_report stream = {0};
	struct dssh_bytes_result held = {0}, next = {0};
	uint8_t public[512], held_copy[1024];
	size_t public_length = 0;
	struct password_state pw = {.writing = 1, .valid = true};
	const char *cache = getenv(ops->cache);
	if (policy_workspace) {
		_Static_assert(sizeof("dssh-save-policy-XXXXXX") <= sizeof(root), "relative fixture fits");
		memcpy(root, "dssh-save-policy-XXXXXX", sizeof("dssh-save-policy-XXXXXX"));
	}

	dssh_test_key_stdio_reset();
	dssh_test_alloc_reset();
	dssh_test_reset_global_config();
	if (cache == NULL || ops->reg() != 0 || ops->load(cache, NULL, NULL) != 0 || mkdtemp(root) == NULL)
		goto done;
	created = true;
	if (!join(actual, root, "key") || !join(alias, root, "selected") || !join(inner, root, "inner"))
		goto done;
	const char *path = actual;
	if (kind == 3) {
		if (mkdir(inner, 0700) != 0)
			goto done;
		inner_created = true;
		if (!join(actual, inner, "key") || !join(chosen, alias, "key") || symlink("inner", alias) != 0)
			goto done;
		alias_created = true;
		path = chosen;
	}
	if (kind == 6) {
		if (mkdir(actual, 0750) != 0)
			goto done;
	}
	else if (kind == 7 || kind == 9) {
		if (mkfifo(actual, 0640) != 0 || chmod(actual, 0640) != 0)
			goto done;
		if (kind == 7) {
			peer = open(actual, O_RDONLY | O_NONBLOCK);
			if (peer < 0)
				goto done;
		}
	}
	else if (kind != 1 && kind != 8 && !seed(actual))
		goto done;
	if (kind == 4 || kind == 8) {
		if (symlink("key", alias) != 0)
			goto done;
		alias_created = true;
		path = alias;
	}
	else if (kind == 5) {
		if (link(actual, alias) != 0)
			goto done;
		alias_created = true;
		path = alias;
	}
#ifdef __FreeBSD__
	if (acl_case && (!acl_prepare(actual) || acl_state(actual) != 0))
		goto done;
#endif
#ifdef __APPLE__
	if (acl_case && (!darwin_acl_prepare(actual) || darwin_acl_state(actual) != 0))
		goto done;
#endif
	if (kind == 2 && chmod(root, 0500) != 0)
		goto done;
	had_target = kind != 1 && kind != 8;
	if ((had_target && lstat(actual, &before) != 0)
	    || (alias_created && lstat(alias, &alias_before) != 0))
		goto done;
	dssh_key_algo key = find_key_algo(ops->name);
	dssh_key_algo_ctx *prior = key->ctx;
	const uint8_t *blob = NULL;
	if (key->pubkey(&blob, &public_length, prior) != 0 || blob == NULL
	    || public_length == 0 || public_length > sizeof(public))
		goto done;
	memcpy(public, blob, public_length);
	const uint8_t message[] = {'p', 'o', 'l', 'i', 'c', 'y'};
	if (key->sign(&held, message, sizeof(message), prior) != 0 || held.bytes.data == NULL
	    || held.bytes.len == 0 || held.bytes.len > sizeof(held_copy) || held.lease.release == NULL)
		goto done;
	memcpy(held_copy, held.bytes.data, held.bytes.len);
	dssh_test_key_stdio_reset();
	dssh_test_key_watch_stream();
#ifdef __FreeBSD__
	if (acl_case) {
		dssh_test_key_acl_case(kind - 10);
		if (kind != 15)
			dssh_test_key_permission_noop_once();
	}
#endif
#ifdef __APPLE__
	if (acl_case)
		dssh_test_key_darwin_case(kind - 20);
#endif
	status = ops->save(path, encrypted ? password : NULL, &pw);
	report = dssh_test_key_open_report();
	stream = dssh_test_key_stream_report();
	if (!accepted && report.target_checks != 0)
		retired = dssh_test_key_failed_fd_closed();
	bool acquired_rejection = kind == 5 || kind == 7 || (acl_case && !accepted);
	valid = status == (accepted ? 0 : DSSH_ERROR_INIT) && pw.valid
	    && pw.calls == (accepted && encrypted ? 1U : 0U)
	    && report.native_opens == 1 && report.stdio_opens == 0
	    && report.target_checks == (accepted || acquired_rejection ? 1U : 0U)
	    && report.status_updates == (accepted || acl_case ? 1U : 0U)
	    && report.permissions == (accepted || acl_case ? 1U : 0U) && report.truncations == (accepted ? 1U : 0U)
	    && report.associations == (accepted ? 1U : 0U)
	    && report.raw_closes == (acquired_rejection ? 1U : 0U)
	    && (!acquired_rejection || retired)
	    && (!accepted || (report.descriptor_valid && report.close_on_exec && report.blocking_io && report.status_arguments_valid))
	    && stream.valid && !stream.active && stream.configurations == (accepted ? 1U : 0U)
	    && stream.closes == (accepted ? 1U : 0U) && stream.clears == (accepted ? 1U : 0U);
#ifdef __FreeBSD__
	if (acl_case) {
		acl_report = dssh_test_key_acl_report();
		acl_after = acl_state(actual);
		bool allocated = kind == 10 || kind == 12 || kind == 13 || kind == 14;
		bool produced = kind == 10 || kind == 13 || kind == 14;
		valid = valid && acl_report.queries == 1 && acl_report.capability == 1
		    && acl_report.allocations == (allocated ? 1U : 0U)
		    && acl_report.produced == (produced ? 1U : 0U)
		    && acl_report.sets == (produced ? 1U : 0U)
		    && acl_report.frees == (produced ? 1U : 0U) && acl_report.retired == (produced ? 1U : 0U)
		    && acl_report.injected == (kind == 10 ? 0U : 1U)
		    && acl_report.mode_noops == (kind == 15 ? 0U : 1U)
		    && acl_report.native_free_errors == 0
		    && acl_after == (accepted || kind == 14 ? 1 : 0);
	}
#endif
#ifdef __APPLE__
	if (acl_case) {
		darwin_report = dssh_test_key_darwin_report();
		acl_after = darwin_acl_state(actual);
		valid = valid && darwin_report.allocations == 1
		    && darwin_report.properties == (kind == 21 ? 0U : 1U)
		    && darwin_report.applications == (kind == 21 || kind == 22 ? 0U : 1U)
		    && darwin_report.retired == (kind == 21 ? 0U : 1U)
		    && darwin_report.injected == (kind == 20 ? 0U : 1U)
		    && acl_after == (accepted ? 1 : 0);
	}
#endif
	dssh_test_key_stdio_reset();
	const uint8_t *current = NULL;
	size_t current_length = 0;
	valid = valid && key->ctx == prior && key->pubkey(&current, &current_length, prior) == 0
	    && current == blob && current_length == public_length && memcmp(current, public, public_length) == 0
	    && memcmp(held_copy, held.bytes.data, held.bytes.len) == 0
	    && key->verify(public, public_length, held.bytes.data, held.bytes.len, message, sizeof(message)) == 0;
	if (valid)
		valid = key->sign(&next, message, sizeof(message), prior) == 0 && next.bytes.data != NULL
		    && next.bytes.len != 0 && next.lease.release != NULL && next.bytes.data != held.bytes.data
		    && key->verify(public, public_length, next.bytes.data, next.bytes.len, message, sizeof(message)) == 0;
	if (valid && alias_created)
		valid = lstat(alias, &alias_after) == 0 && same_object(&alias_before, &alias_after);
	if (valid && accepted) {
		valid = stat(actual, &after) == 0 && S_ISREG(after.st_mode) && after.st_uid == geteuid()
		    && after.st_nlink == 1 && (after.st_mode & 0777) == 0600 && after.st_size > 0
		    && (!had_target || (before.st_dev == after.st_dev && before.st_ino == after.st_ino));
		pw = (struct password_state){.writing = 0, .valid = true};
		valid = valid && ops->load(path, encrypted ? password : NULL, &pw) == 0 && pw.valid
		    && pw.calls == (encrypted ? 1U : 0U)
		    && key->pubkey(&current, &current_length, key->ctx) == 0 && current_length == public_length
		    && memcmp(current, public, public_length) == 0
		    && memcmp(held_copy, held.bytes.data, held.bytes.len) == 0
		    && key->verify(public, public_length, held.bytes.data, held.bytes.len, message, sizeof(message)) == 0;
	}
	else if (valid && had_target) {
		valid = lstat(actual, &after) == 0 && same_object(&before, &after)
		    && (!S_ISREG(before.st_mode) || unchanged_bytes(actual));
		if (valid && peer >= 0) {
			char byte;
			valid = read(peer, &byte, 1) == 0;
		}
	}
	else if (valid) {
		errno = 0;
		valid = lstat(actual, &after) == -1 && errno == ENOENT;
	}
done:
	dssh_result_release(&next.lease);
	dssh_result_release(&held.lease);
	dssh_test_key_stdio_reset();
	dssh_test_alloc_reset();
	dssh_test_reset_global_config();
	if (peer >= 0 && close(peer) != 0)
		valid = false;
	if (created) {
		if (chmod(root, 0700) != 0)
			valid = false;
		if (alias_created && unlink(alias) != 0)
			valid = false;
		if (actual[0] != 0) {
			struct stat cleanup;
			if (lstat(actual, &cleanup) == 0) {
				if ((S_ISDIR(cleanup.st_mode) ? rmdir(actual) : unlink(actual)) != 0)
					valid = false;
			}
			else if (errno != ENOENT)
				valid = false;
		}
		if (inner_created && rmdir(inner) != 0)
			valid = false;
		if (rmdir(root) != 0)
			valid = false;
	}
	printf("key save policy: algorithm=%s kind=%u encrypted=%u status=%d targets=%u flags=%u chmod=%u truncate=%u "
	    "associations=%u raw-close=%u retired=%u configured=%u closed=%u cleared=%u valid=%u acl-private=%d\n",
	    ops->name, kind, encrypted ? 1U : 0U, status, report.target_checks, report.status_updates,
	    report.permissions, report.truncations, report.associations, report.raw_closes, retired ? 1U : 0U,
	    stream.configurations, stream.closes, stream.clears, valid ? 1U : 0U, acl_after);
#ifdef __FreeBSD__
	if (acl_case)
		printf("key ACL: algorithm=%s kind=%u encrypted=%u queries=%u allocations=%u produced=%u sets=%u "
		    "frees=%u retired=%u injected=%u noop=%u private=%d valid=%u\n", ops->name, kind, encrypted ? 1U : 0U,
		    acl_report.queries, acl_report.allocations, acl_report.produced, acl_report.sets,
		    acl_report.frees, acl_report.retired, acl_report.injected, acl_report.mode_noops, acl_after, valid ? 1U : 0U);
#endif
#ifdef __APPLE__
	if (acl_case)
		printf("key Darwin ACL: algorithm=%s kind=%u encrypted=%u allocations=%u properties=%u "
		    "applications=%u retired=%u injected=%u private=%d valid=%u\n", ops->name, kind, encrypted ? 1U : 0U,
		    darwin_report.allocations, darwin_report.properties, darwin_report.applications,
		    darwin_report.retired, darwin_report.injected, acl_after, valid ? 1U : 0U);
#endif
	return valid;
}
static int
cases(unsigned int first, unsigned int last, bool both)
{
	unsigned int completed = 0;
	for (size_t i = 0; i < sizeof(operations) / sizeof(operations[0]); i++) {
		for (unsigned int kind = first; kind <= last; kind++) {
			if (first == 11 && kind == 15)
				continue;
			for (unsigned int encrypted = both ? 0U : 1U; encrypted < 2; encrypted++) {
				if (policy_case(&operations[i], kind, encrypted != 0))
					completed++;
			}
		}
	}
	unsigned int expected = first == 0 ? 24U : first == 4 ? 15U : first == 9 ? 3U
	    : first == 11 ? 36U : first == 21 ? 18U : 6U;
	printf("key save policy summary: first=%u last=%u expected=%u completed=%u\n", first, last, expected, completed);
	ASSERT_EQ(completed, expected);
	return TEST_PASS;
}
static int
test_identity(void)
{
	return cases(0, 3, true);
}
static int
test_rejected(void)
{
	return cases(4, 8, false);
}
static int
test_nonblocking(void)
{
	return cases(9, 9, false);
}
#ifdef __FreeBSD__
static int
acl_cases(unsigned int first, unsigned int last, bool workspace)
{
	bool prior = policy_workspace;
	policy_workspace = workspace;
	int ret = TEST_FAIL;
	int fd = open(workspace ? "." : "/tmp", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd >= 0) {
		long capability = fpathconf(fd, _PC_ACL_NFS4);
		int closed = close(fd);
		if (closed == 0 && capability == 1)
			ret = cases(first, last, true);
		else if (closed == 0 && capability == 0) {
			printf("native NFSv4 ACLs unavailable for this fixture directory\n");
			ret = TEST_SKIP;
		}
	}
	policy_workspace = prior;
	return ret;
}
static int
test_acl_owner_only(void)
{
	return acl_cases(10, 10, false);
}
static int
test_acl_failures(void)
{
	return acl_cases(11, 17, false);
}
static int
test_acl_capability_absent(void)
{
	return acl_cases(15, 15, false);
}
static int
test_acl_workspace(void)
{
	return acl_cases(10, 10, true);
}
#endif
#ifdef __APPLE__
static int
test_darwin_acl_owner_only(void)
{
	return cases(20, 20, true);
}
static int
test_darwin_acl_failures(void)
{
	return cases(21, 23, true);
}
static int
test_darwin_acl_workspace(void)
{
	policy_workspace = true;
	int ret = cases(20, 20, true);
	policy_workspace = false;
	return ret;
}
#endif
static struct dssh_test_entry tests[] = {
	{"identity", test_identity},
	{"rejected", test_rejected},
	{"nonblocking", test_nonblocking},
#ifdef __FreeBSD__
	{"acl_owner_only", test_acl_owner_only},
	{"acl_failures", test_acl_failures},
	{"acl_capability_absent", test_acl_capability_absent},
	{"acl_workspace", test_acl_workspace},
#endif
#ifdef __APPLE__
	{"darwin_acl_owner_only", test_darwin_acl_owner_only},
	{"darwin_acl_failures", test_darwin_acl_failures},
	{"darwin_acl_workspace", test_darwin_acl_workspace},
#endif
};
DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
