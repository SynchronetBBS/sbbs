/* Real native descriptor properties and public key-file entry validation. */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "dssh_test.h"
#include "dssh_test_internal.h"
#include "dssh_test_alloc.h"
#include "dssh_test_ossl.h"
#include "deucessh-algorithms.h"
#include "key_algo/private-key-file.h"

struct descriptor_ops {
	int (*reg)(void);
	int (*load)(const char *, dssh_pem_password_cb, void *);
	int (*save)(const char *, dssh_pem_password_cb, void *);
	int (*public_save)(const char *);
	const char *name, *cache;
};
static const struct descriptor_ops operations[] = {
	{dssh_register_ssh_ed25519, dssh_ed25519_load_key_file, dssh_ed25519_save_key_file,
	    dssh_ed25519_save_pub_file, "ssh-ed25519", "DSSH_TEST_ED25519_KEY"},
	{dssh_register_rsa_sha2_256, dssh_rsa_sha2_256_load_key_file, dssh_rsa_sha2_256_save_key_file,
	    dssh_rsa_sha2_256_save_pub_file, "rsa-sha2-256", "DSSH_TEST_RSA_KEY"},
	{dssh_register_rsa_sha2_512, dssh_rsa_sha2_512_load_key_file, dssh_rsa_sha2_512_save_key_file,
	    dssh_rsa_sha2_512_save_pub_file, "rsa-sha2-512", "DSSH_TEST_RSA_KEY"},
};

static const char stage_marker[] = "native key-file stage control";

/* Test data is nonsensitive. Inspect the actual file, not an observer's model. */
static bool
stage_file_state(const char *path, unsigned int kind)
{
	struct stat info;
	mode_t expected_mode = kind == 5 || kind == 7 || kind >= 10 ? 0640 : 0600;
	bool empty = kind == 3 || kind == 9;
	if (stat(path, &info) != 0 || (info.st_mode & 0777) != expected_mode
	    || info.st_size != (empty ? 0 : sizeof(stage_marker)))
		return false;
	FILE *fp = fopen(path, "rb");
	if (fp == NULL)
		return false;
	char observed[sizeof(stage_marker)];
	size_t count = fread(observed, 1, sizeof(observed), fp);
	bool valid = count == (empty ? 0 : sizeof(stage_marker))
	    && (empty || memcmp(observed, stage_marker, sizeof(stage_marker)) == 0)
	    && fgetc(fp) == EOF && ferror(fp) == 0;
	if (fclose(fp) != 0)
		valid = false;
	return valid;
}

static int
stage_password(char *buffer, int capacity, int writing, void *data)
{
	unsigned int *calls = data;
	if (*calls < UINT_MAX)
		(*calls)++;
	if (buffer == NULL || capacity < 1 || writing != 1)
		return -1;
	buffer[0] = 'p';
	return 1;
}

static bool
stage_close_error(unsigned int kind)
{
	return (kind >= 7 && kind <= 9) || kind >= 13;
}

static void
arm_stage(unsigned int kind)
{
	if (kind == 3 || kind == 9)
		dssh_test_key_fdopen_error_once();
	else if (kind == 5 || kind == 7)
		dssh_test_key_permission_error_once();
	else if (kind == 6 || kind == 8)
		dssh_test_key_truncate_error_once();
	else if (kind == 10 || kind == 13)
		dssh_test_key_target_error_once();
	else if (kind == 11 || kind == 14)
		dssh_test_key_status_error_once();
	else if (kind == 12 || kind == 15)
		dssh_test_key_different_owner_once();
	if (stage_close_error(kind))
		dssh_test_key_raw_close_error_once();
}

static bool
stage_report_valid(struct dssh_test_key_open_report report, unsigned int kind)
{
	bool permission = kind == 5 || kind == 7;
	bool truncate = kind == 6 || kind == 8;
	bool metadata = kind >= 10;
	bool target_error = kind == 10 || kind == 13;
	bool status_error = kind == 11 || kind == 14;
	bool owner = kind == 12 || kind == 15;
	bool associated = !metadata && !permission && !truncate;
	return report.stdio_opens == 0 && report.native_opens == 1
	    && report.target_checks == 1 && report.target_errors == (target_error ? 1U : 0U)
	    && report.status_updates == (!metadata || status_error ? 1U : 0U)
	    && report.status_errors == (status_error ? 1U : 0U)
	    && report.reported_owners == (owner ? 1U : 0U)
	    && (metadata && !status_error ? !report.status_arguments_valid : report.status_arguments_valid)
	    && report.permissions == (metadata ? 0U : 1U)
	    && report.permission_errors == (permission ? 1U : 0U)
	    && report.truncations == (permission || metadata ? 0U : 1U)
	    && report.truncation_errors == (truncate ? 1U : 0U)
	    && report.associations == (associated ? 1U : 0U)
	    && report.rejected_associations == (associated ? 1U : 0U)
	    && report.raw_closes == 1 && report.raw_close_errors == (stage_close_error(kind) ? 1U : 0U)
	    && report.native_close_errors == 0 && !report.descriptor_valid;
}

/* kind0 normal property, kind1 NULL, kind2 empty, kind3 rejected fdopen,
 * kind4 actual missing-parent open error, kind5/6 metadata errors,
 * kind7/8/9 metadata/association error with reported post-retirement close error;
 * kind10..15 target/status errors and reported different ownership, plus close.
 * Never inspect a native object layout. */
static bool
file_case(const struct descriptor_ops *ops, unsigned int action, unsigned int kind, bool encrypted)
{
	char output[] = "/tmp/dssh-fd-output-XXXXXX";
	char absent[] = "/tmp/dssh-fd-absent-XXXXXX";
	char missing[128];
	bool output_created = false, directory_created = false, valid = false;
	struct dssh_bytes_result held = {0}, next = {0};
	uint8_t public[512], held_copy[1024];
	size_t public_length = 0;
	struct dssh_test_key_open_report report = {0};
	bool closed = false;
	int status = DSSH_ERROR_INIT;
	unsigned int password_calls = 0;
	const char *cache = getenv(ops->cache);
	dssh_test_alloc_reset();
	dssh_test_key_stdio_reset();
	dssh_test_reset_global_config();
	if (cache == NULL || ops->reg() != 0 || ops->load(cache, NULL, NULL) != 0)
		goto done;
	int fd = mkstemp(output);
	if (fd < 0)
		goto done;
	output_created = true;
	bool seeded = write(fd, stage_marker, sizeof(stage_marker)) == sizeof(stage_marker)
	    && fchmod(fd, 0640) == 0;
	if (close(fd) != 0)
		seeded = false;
	if (!seeded || mkdtemp(absent) == NULL)
		goto done;
	directory_created = true;
	if (rmdir(absent) != 0)
		goto done;
	directory_created = false;
	int count = snprintf(missing, sizeof(missing), "%s/key", absent);
	if (count < 1 || count >= 128)
		goto done;
	dssh_key_algo key = find_key_algo(ops->name);
	dssh_key_algo_ctx *prior = key->ctx;
	const uint8_t *blob = NULL;
	if (key->pubkey(&blob, &public_length, prior) != 0 || blob == NULL
	    || public_length == 0 || public_length > sizeof(public))
		goto done;
	memcpy(public, blob, public_length);
	const uint8_t message[] = {'f', 'i', 'l', 'e'};
	if (key->sign(&held, message, sizeof(message), prior) != 0 || held.bytes.data == NULL
	    || held.bytes.len == 0 || held.bytes.len > sizeof(held_copy) || held.lease.release == NULL)
		goto done;
	memcpy(held_copy, held.bytes.data, held.bytes.len);
	const char *path = action == 0 ? cache : output;
	if (kind == 1)
		path = NULL;
	else if (kind == 2)
		path = "";
	else if (kind == 4)
		path = missing;
	dssh_test_key_stdio_reset();
	if (action == 1 && (kind == 3 || kind >= 5))
		dssh_test_key_watch_stream();
	arm_stage(kind);
	status = action == 0 ? ops->load(path, NULL, NULL)
	    : action == 1 ? ops->save(path, encrypted ? stage_password : NULL, &password_calls) : ops->public_save(path);
	report = dssh_test_key_open_report();
	if (kind == 3 || kind >= 5)
		closed = dssh_test_key_failed_fd_closed();
	unsigned int calls = report.stdio_opens + report.native_opens;
	valid = status == (kind == 0 ? 0 : DSSH_ERROR_INIT)
	    && (kind >= 5 ? stage_report_valid(report, kind) && closed
	        : kind == 0 ? report.descriptor_valid && report.close_on_exec && report.blocking_io && calls == 1
	        : kind == 1 || kind == 2 ? calls == 0 && report.associations == 0
	        : kind == 3 ? report.rejected_associations == 1 && closed
	        : calls == 1 && report.associations == 0 && !report.descriptor_valid)
	    && (kind == 3 || kind == 9 || report.rejected_associations == 0)
	    && password_calls == 0;
	if (valid && action == 1 && (kind == 3 || kind >= 5)) {
		struct dssh_test_key_stream_report stream = dssh_test_key_stream_report();
		valid = stage_report_valid(report, kind) && closed && stage_file_state(output, kind)
		    && stream.valid && !stream.active && stream.configurations == 0
		    && stream.closes == 0 && stream.clears == 0 && dssh_test_key_close_calls() == 0;
	}
	const uint8_t *after = NULL;
	size_t after_length = 0;
	valid = valid && key->pubkey(&after, &after_length, key->ctx) == 0 && after_length == public_length
	    && after != NULL && memcmp(after, public, public_length) == 0
	    && ((kind == 0 && action == 0) || (key->ctx == prior && after == blob))
	    && memcmp(held_copy, held.bytes.data, held.bytes.len) == 0
	    && key->verify(public, public_length, held.bytes.data, held.bytes.len, message, sizeof(message)) == 0;
	if (valid)
		valid = key->sign(&next, message, sizeof(message), key->ctx) == 0
		    && next.bytes.data != NULL && next.bytes.len != 0 && next.lease.release != NULL
		    && next.bytes.data != held.bytes.data
		    && key->verify(public, public_length, next.bytes.data, next.bytes.len, message, sizeof(message)) == 0;
	if (valid && kind == 0 && action != 0) {
		struct stat info;
		valid = stat(output, &info) == 0 && info.st_size > 0;
	}
done:
	dssh_result_release(&next.lease);
	dssh_result_release(&held.lease);
	dssh_test_key_stdio_reset();
	dssh_test_alloc_reset();
	dssh_test_reset_global_config();
	if (output_created && unlink(output) != 0)
		valid = false;
	if (directory_created && rmdir(absent) != 0)
		valid = false;
	printf("key descriptors: algorithm=%s action=%u kind=%u status=%d stdio=%u native=%u associations=%u "
	    "rejected=%u observed-valid=%u cloexec=%u retired=%u valid=%u "
	    "encrypted=%u password-calls=%u chmod=%u truncate=%u raw-close=%u close-error=%u "
	    "targets=%u target-errors=%u flags=%u flag-errors=%u reported-owner=%u\n", ops->name, action, kind, status,
	    report.stdio_opens, report.native_opens, report.associations, report.rejected_associations,
	    report.descriptor_valid ? 1U : 0U, report.close_on_exec ? 1U : 0U, closed ? 1U : 0U, valid ? 1U : 0U,
	    encrypted ? 1U : 0U, password_calls, report.permissions, report.truncations, report.raw_closes, report.raw_close_errors,
	    report.target_checks, report.target_errors, report.status_updates, report.status_errors, report.reported_owners);
	return valid;
}

static int
cases(unsigned int first, unsigned int last)
{
	unsigned int completed = 0;
	for (size_t i = 0; i < sizeof(operations) / sizeof(operations[0]); i++) {
		for (unsigned int action = 0; action < 3; action++) {
			for (unsigned int kind = first; kind <= last; kind++) {
				if (file_case(&operations[i], action, kind, false))
					completed++;
			}
		}
	}
	unsigned int expected = first == last ? 9U : 18U;
	printf("key descriptors summary: first=%u last=%u expected=%u completed=%u\n", first, last, expected, completed);
	ASSERT_EQ(completed, expected);
	return TEST_PASS;
}
static int
test_modes(void)
{
	return cases(0, 0);
}
static int
test_invalid_paths(void)
{
	return cases(1, 2);
}
static int
test_association_failure(void)
{
	return cases(3, 3);
}
static int
test_native_open_failure(void)
{
	return cases(4, 4);
}
static int
test_exec_controls(void)
{
	unsigned int completed = 0;
	for (unsigned int sealed = 0; sealed < 2; sealed++) {
		char path[] = "/tmp/dssh-fd-control-XXXXXX";
		int fd = mkstemp(path);
		if (fd < 0)
			continue;
		int flags = fcntl(fd, F_GETFD);
		int selected = sealed != 0 ? flags | FD_CLOEXEC : flags & ~FD_CLOEXEC;
		bool valid = flags >= 0 && fcntl(fd, F_SETFD, selected) == 0;
		char number[32];
		int count = snprintf(number, sizeof(number), "%d", fd);
		valid = valid && count > 0 && count < 32;
		if (valid) {
			char *args[] = {DSSH_FD_EXEC_PROBE, number, path, sealed != 0 ? "closed" : "open", NULL};
			pid_t pid = fork();
			if (pid == 0) {
				execv(DSSH_FD_EXEC_PROBE, args);
				_exit(125);
			}
			if (pid < 0)
				valid = false;
			else {
				int status;
				pid_t waited;
				do {
					waited = waitpid(pid, &status, 0);
				} while (waited < 0 && errno == EINTR);
				valid = waited == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
			}
		}
		if (close(fd) != 0)
			valid = false;
		if (unlink(path) != 0)
			valid = false;
		if (valid)
			completed++;
	}
	printf("key descriptors exec controls: expected=2 completed=%u\n", completed);
	ASSERT_EQ(completed, 2);
	return TEST_PASS;
}
static int
private_cases(const unsigned int *kinds, size_t count)
{
	unsigned int completed = 0;
	for (size_t i = 0; i < sizeof(operations) / sizeof(operations[0]); i++) {
		for (unsigned int encrypted = 0; encrypted < 2; encrypted++) {
			for (size_t k = 0; k < count; k++) {
				if (file_case(&operations[i], 1, kinds[k], encrypted != 0))
					completed++;
			}
		}
	}
	printf("key native private stages: expected=36 completed=%u\n", completed);
	ASSERT_EQ(completed, 36);
	return TEST_PASS;
}

static int
test_private_stages(void)
{
	const unsigned int kinds[] = {3, 5, 6, 7, 8, 9};
	return private_cases(kinds, sizeof(kinds) / sizeof(kinds[0]));
}
static int
test_private_metadata(void)
{
	const unsigned int kinds[] = {10, 11, 12, 13, 14, 15};
	return private_cases(kinds, sizeof(kinds) / sizeof(kinds[0]));
}

/* Direct private-helper errors also distinguish preservation of the initial
 * errno from a later cleanup-close error. Public APIs make no errno promise. */
static int
helper_cases(const unsigned int *kinds, size_t count)
{
	unsigned int completed = 0;
	for (size_t k = 0; k < count; k++) {
		unsigned int kind = kinds[k];
		char path[] = "/tmp/dssh-native-errno-XXXXXX";
		int fd = mkstemp(path);
		if (fd < 0)
			continue;
		bool valid = write(fd, stage_marker, sizeof(stage_marker)) == sizeof(stage_marker)
		    && fchmod(fd, 0640) == 0;
		if (close(fd) != 0)
			valid = false;
		int saved = 0;
		int expected = stage_close_error(kind) || kind == 10 || kind == 11 ? EIO
		    : kind == 3 ? ENOMEM : kind == 5 || kind == 12 ? EACCES : ENOSPC;
		if (valid) {
			dssh_test_key_stdio_reset();
			arm_stage(kind);
			FILE *fp = dssh_private_key_open(path);
			saved = errno;
			struct dssh_test_key_open_report report = dssh_test_key_open_report();
			valid = fp == NULL && saved == expected && stage_report_valid(report, kind)
			    && dssh_test_key_failed_fd_closed() && stage_file_state(path, kind);
			if (fp != NULL && fclose(fp) != 0)
				valid = false;
		}
		dssh_test_key_stdio_reset();
		if (unlink(path) != 0)
			valid = false;
		if (valid)
			completed++;
		printf("key native errno: kind=%u expected=%d observed=%d valid=%u\n", kind, expected, saved, valid ? 1U : 0U);
	}
	printf("key native errno summary: expected=6 completed=%u\n", completed);
	ASSERT_EQ(completed, 6);
	return TEST_PASS;
}

static int
test_helper_errno(void)
{
	const unsigned int kinds[] = {3, 5, 6, 7, 8, 9};
	return helper_cases(kinds, sizeof(kinds) / sizeof(kinds[0]));
}
static int
test_helper_metadata_errno(void)
{
	const unsigned int kinds[] = {10, 11, 12, 13, 14, 15};
	return helper_cases(kinds, sizeof(kinds) / sizeof(kinds[0]));
}

static int
test_native_controls(void)
{
	unsigned int completed = 0;
	for (unsigned int fail = 0; fail < 2; fail++) {
		char path[] = "/tmp/dssh-native-control-XXXXXX";
		int fd = mkstemp(path);
		if (fd < 0)
			continue;
		bool valid = write(fd, stage_marker, sizeof(stage_marker)) == sizeof(stage_marker)
		    && fchmod(fd, 0640) == 0;
		dssh_test_key_stdio_reset();
		if (fail != 0) {
			dssh_test_key_permission_error_once();
			dssh_test_key_truncate_error_once();
			dssh_test_key_raw_close_error_once();
		}
		int permission = dssh_test_key_native_fchmod(fd, 0400);
		int permission_errno = errno;
		int truncate = dssh_test_key_native_ftruncate(fd, 3);
		int truncate_errno = errno;
		struct stat info;
		valid = valid && fstat(fd, &info) == 0
		    && (info.st_mode & 0777) == (fail != 0 ? 0640 : 0400)
		    && info.st_size == (fail != 0 ? sizeof(stage_marker) : 3);
		int closed = dssh_test_key_native_close(fd);
		int close_errno = errno;
		errno = 0;
		valid = valid && fcntl(fd, F_GETFD) == -1 && errno == EBADF;
		struct dssh_test_key_open_report report = dssh_test_key_open_report();
		valid = valid && permission == (fail != 0 ? -1 : 0) && truncate == (fail != 0 ? -1 : 0)
		    && closed == (fail != 0 ? -1 : 0)
		    && (fail == 0 || (permission_errno == EACCES && truncate_errno == ENOSPC && close_errno == EIO))
		    && report.permissions == 1 && report.truncations == 1 && report.raw_closes == 1
		    && report.permission_errors == fail && report.truncation_errors == fail
		    && report.raw_close_errors == fail && report.native_close_errors == 0;
		dssh_test_key_stdio_reset();
		if (unlink(path) != 0)
			valid = false;
		if (valid)
			completed++;
		printf("key native controls: fault=%u valid=%u\n", fail, valid ? 1U : 0U);
	}
	printf("key native controls summary: expected=2 completed=%u\n", completed);
	ASSERT_EQ(completed, 2);
	return TEST_PASS;
}

static struct dssh_test_entry tests[] = {
	{"modes", test_modes},
	{"invalid_paths", test_invalid_paths},
	{"association_failure", test_association_failure},
	{"native_open_failure", test_native_open_failure},
	{"exec_controls", test_exec_controls},
	{"private_stages", test_private_stages},
	{"private_metadata", test_private_metadata},
	{"helper_errno", test_helper_errno},
	{"helper_metadata_errno", test_helper_metadata_errno},
	{"native_controls", test_native_controls},
};
DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
