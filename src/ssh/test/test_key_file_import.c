/* Key-file import transaction checks using real keys and native stdio. */
#include <unistd.h>
#include <limits.h>
#ifndef _WIN32
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#endif
#ifdef DSSH_CRYPTO_OPENSSL
#include <openssl/ui.h>
#endif
#include "dssh_test.h"
#include "dssh_test_internal.h"
#include "dssh_test_alloc.h"
#include "dssh_test_ossl.h"
#include "deucessh-algorithms.h"
#include "deucessh-key-algo.h"
#include "key_algo/key-file-common.h"
#ifdef setvbuf
#undef setvbuf
#endif
#ifdef fclose
#undef fclose
#endif
#ifdef fread
#undef fread
#endif
#ifdef fwrite
#undef fwrite
#endif
#ifdef fputc
#undef fputc
#endif

struct key_file_ops {
	int (*reg)(void);
	int (*generate)(void);
	int (*load)(const char *, dssh_pem_password_cb, void *);
	int (*save)(const char *, dssh_pem_password_cb, void *);
	int64_t (*text)(char *, size_t);
	const char *cache;
	const char *name;
};

static int
rsa256_generate(void)
{
	return dssh_rsa_sha2_256_generate_key(2048);
}

static int
rsa512_generate(void)
{
	return dssh_rsa_sha2_512_generate_key(2048);
}

static const struct key_file_ops algorithms[] = {
	{dssh_register_ssh_ed25519, dssh_ed25519_generate_key,
	    dssh_ed25519_load_key_file, dssh_ed25519_save_key_file,
	    dssh_ed25519_get_pub_str, "DSSH_TEST_ED25519_KEY", "ssh-ed25519"},
	{dssh_register_rsa_sha2_256, rsa256_generate,
	    dssh_rsa_sha2_256_load_key_file, dssh_rsa_sha2_256_save_key_file,
	    dssh_rsa_sha2_256_get_pub_str, "DSSH_TEST_RSA_KEY", "rsa-sha2-256"},
	{dssh_register_rsa_sha2_512, rsa512_generate,
	    dssh_rsa_sha2_512_load_key_file, dssh_rsa_sha2_512_save_key_file,
	    dssh_rsa_sha2_512_get_pub_str, "DSSH_TEST_RSA_KEY", "rsa-sha2-512"},
};

/* Test keys are Ed25519 / 2048-bit RSA. Every use still checks the actual
 * formatter length before inspecting the concrete 512-byte snapshot. */
static bool
snapshot(const struct key_file_ops *ops, char *text, size_t *length)
{
	int64_t ret = ops->text(text, 512);
	if (ret < 1 || ret > 512)
		return false;
	size_t count = (size_t)ret;
	*length = count;
	return true;
}

static bool
public_snapshot(const struct key_file_ops *ops, uint8_t *out, size_t *length)
{
	dssh_key_algo key = find_key_algo(ops->name);
	const uint8_t *blob = NULL;
	size_t count = 0;
	if (key == NULL || key->pubkey == NULL
	    || key->pubkey(&blob, &count, key->ctx) != 0 || blob == NULL || count == 0 || count > 512)
		return false;
	memcpy(out, blob, count);
	*length = count;
	return true;
}

static bool
signable(const struct key_file_ops *ops, const uint8_t *public_blob, size_t public_length)
{
	dssh_key_algo key = find_key_algo(ops->name);
	if (key == NULL || key->sign == NULL || key->verify == NULL)
		return false;
	const uint8_t message[] = {'v', 'a', 'l', 'i', 'd'};
	struct dssh_bytes_result signature = {0};
	int ret = key->sign(&signature, message, sizeof(message), key->ctx);
	bool valid = ret == 0 && signature.bytes.data != NULL && signature.bytes.len != 0
	    && signature.lease.release != NULL;
	if (valid)
		valid = key->verify(public_blob, public_length, signature.bytes.data,
		    signature.bytes.len, message, sizeof(message)) == 0;
	dssh_result_release(&signature.lease);
	return valid;
}

static bool
temporary(char *path, bool *created)
{
	int fd = mkstemp(path);
	if (fd < 0)
		return false;
	*created = true;
	return close(fd) == 0;
}

static bool
start(const struct key_file_ops *ops)
{
	dssh_test_key_stdio_reset();
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	dssh_test_reset_global_config();
	if (ops->reg() != 0)
		return false;
	const char *cache = getenv(ops->cache);
	return (cache != NULL ? ops->load(cache, NULL, NULL) : ops->generate()) == 0;
}

static bool
close_case(const struct key_file_ops *ops, bool fail,
    unsigned int *injected, unsigned int *rejected, unsigned int *preserved)
{
	char path[] = "/tmp/dssh-key-import-XXXXXX";
	char candidate[512], before[512], after[512];
	uint8_t candidate_public[512], before_public[512];
	size_t candidate_public_len = 0, before_public_len = 0;
	size_t candidate_len = 0, before_len = 0, after_len = 0;
	bool created = false;
	bool valid = false;
	int ret;
	if (!start(ops) || !temporary(path, &created)
	    || ops->save(path, NULL, NULL) != 0
	    || !snapshot(ops, candidate, &candidate_len)
	    || !public_snapshot(ops, candidate_public, &candidate_public_len)
	    || ops->generate() != 0 || !snapshot(ops, before, &before_len)
	    || !public_snapshot(ops, before_public, &before_public_len))
		goto done;
	if (candidate_len == before_len && memcmp(candidate, before, before_len) == 0)
		goto done;
	dssh_test_key_stdio_reset();
	if (fail)
		dssh_test_key_close_error_once();
	ret = ops->load(path, NULL, NULL);
	*injected += dssh_test_key_close_errors();
	if (ret < 0)
		(*rejected)++;
	if (!snapshot(ops, after, &after_len))
		goto done;
	bool same = before_len == after_len && memcmp(before, after, before_len) == 0;
	if (same)
		(*preserved)++;
	valid = dssh_test_key_close_calls() == 1
	    && dssh_test_key_native_close_errors() == 0
	    && dssh_test_key_close_errors() == (fail ? 1U : 0U)
	    && (fail ? ret < 0 && same : ret == 0 && candidate_len == after_len
	        && memcmp(candidate, after, candidate_len) == 0)
	    && signable(ops, fail ? before_public : candidate_public,
	        fail ? before_public_len : candidate_public_len);
done:
	dssh_test_key_stdio_reset();
	dssh_test_alloc_reset();
	dssh_test_reset_global_config();
	if (created && unlink(path) != 0)
		valid = false;
	return valid;
}

static int
close_cases(bool fail)
{
	bool valid = true;
	unsigned int injected = 0, rejected = 0, preserved = 0;
	for (size_t i = 0; i < sizeof(algorithms) / sizeof(algorithms[0]); i++) {
		bool one = close_case(&algorithms[i], fail, &injected, &rejected, &preserved);
		valid = one && valid;
	}
	printf("key-import close: algorithms=3 fail=%u injected=%u rejected=%u preserved=%u valid=%u\n",
	    fail ? 1U : 0U, injected, rejected, preserved, valid ? 1U : 0U);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_close_success(void)
{
	return close_cases(false);
}

static int
test_close_failure(void)
{
	return close_cases(true);
}

#ifdef DSSH_CRYPTO_BOTAN
static bool
retirement_case(const struct key_file_ops *ops, unsigned int *seen,
    unsigned int *cleared, unsigned int *short_reads)
{
	char candidate[] = "/tmp/dssh-key-input-a-XXXXXX";
	char installed[] = "/tmp/dssh-key-input-b-XXXXXX";
	char garbage[] = "/tmp/dssh-key-input-x-XXXXXX";
	bool a = false, b = false, x = false;
	bool valid = false;
	char before[512], after[512];
	uint8_t candidate_public[512], before_public[512];
	size_t candidate_public_len = 0, before_public_len = 0;
	size_t before_len, after_len;
	FILE *fp = NULL;
	if (!start(ops) || !temporary(candidate, &a) || !temporary(installed, &b)
	    || !temporary(garbage, &x) || ops->save(candidate, NULL, NULL) != 0
	    || !public_snapshot(ops, candidate_public, &candidate_public_len)
	    || ops->generate() != 0 || ops->save(installed, NULL, NULL) != 0)
		goto done;
	fp = fopen(garbage, "wb");
	if (fp == NULL)
		goto done;
	bool written = fwrite("invalid PEM\n", 1, 12, fp) == 12;
	if (fclose(fp) != 0)
		written = false;
	fp = NULL;
	if (!written)
		goto done;
	valid = true;
	for (unsigned int mode = 0; mode < 4; mode++) {
		if (ops->load(installed, NULL, NULL) != 0 || !snapshot(ops, before, &before_len)
		    || !public_snapshot(ops, before_public, &before_public_len)) {
			valid = false;
			break;
		}
		dssh_test_key_stdio_reset();
		dssh_test_alloc_reset();
		dssh_test_key_watch_read_free();
		if (mode == 2)
			dssh_test_key_close_error_once();
		if (mode == 3)
			dssh_test_key_read_short_once();
		int ret = ops->load(mode == 1 ? garbage : candidate, NULL, NULL);
		bool zero = false;
		bool observed = dssh_test_alloc_watched_free(&zero);
		*seen += observed ? 1U : 0U;
		*cleared += observed && zero ? 1U : 0U;
		*short_reads += dssh_test_key_short_read_calls();
		bool saved = snapshot(ops, after, &after_len);
		bool unchanged = saved && before_len == after_len && memcmp(before, after, before_len) == 0;
		valid = observed && zero && saved && dssh_test_key_close_calls() == 1
		    && dssh_test_key_native_close_errors() == 0 && dssh_test_key_read_calls() == 1
		    && dssh_test_key_close_errors() == (mode == 2 ? 1U : 0U)
		    && dssh_test_key_short_read_calls() == (mode == 3 ? 1U : 0U)
		    && (mode == 0 ? ret == 0 : ret < 0 && unchanged)
		    && signable(ops, mode == 0 ? candidate_public : before_public,
		        mode == 0 ? candidate_public_len : before_public_len) && valid;
		dssh_test_key_stdio_reset();
		dssh_test_alloc_reset();
	}
done:
	dssh_test_key_stdio_reset();
	dssh_test_alloc_reset();
	dssh_test_reset_global_config();
	if (fp != NULL && fclose(fp) != 0)
		valid = false;
	if (a && unlink(candidate) != 0)
		valid = false;
	if (b && unlink(installed) != 0)
		valid = false;
	if (x && unlink(garbage) != 0)
		valid = false;
	return valid;
}

static int
test_input_retirement(void)
{
	bool valid = true;
	unsigned int seen = 0, cleared = 0, short_reads = 0;
	for (size_t i = 0; i < sizeof(algorithms) / sizeof(algorithms[0]); i++) {
		bool one = retirement_case(&algorithms[i], &seen, &cleared, &short_reads);
		valid = one && valid;
	}
	printf("key-import retirement: algorithms=3 modes=4 observed=%u cleared=%u short_reads=%u valid=%u\n",
	    seen, cleared, short_reads, valid ? 1U : 0U);
	ASSERT_TRUE(valid && seen == 12 && cleared == 12 && short_reads == 3);
	return TEST_PASS;
}
#endif


struct password_control {
	unsigned int mode;
	bool wrong;
	bool valid;
	unsigned int reads;
	unsigned int writes;
	uintptr_t watched_address;
	bool watching;
	size_t capacity;
	unsigned int bad_reports;
	unsigned int cleared;
	bool zero;
	const struct key_file_ops *replacement;
	bool cancel_after_replace;
	unsigned int replaced;
};

static int
password_callback(char *buf, int size, int rwflag, void *opaque)
{
	struct password_control *control = opaque;
	if (buf == NULL || size < 1 || (rwflag != 0 && rwflag != 1)) {
		control->valid = false;
		return -1;
	}
#if INT_MAX > SIZE_MAX
	if (size > SIZE_MAX) {
		control->valid = false;
		return -1;
	}
#endif
	if (rwflag == 0 && control->reads < UINT_MAX)
		control->reads++;
	if (rwflag == 1 && control->writes < UINT_MAX)
		control->writes++;
	if (rwflag == 1 && control->replacement != NULL && control->replaced == 0) {
		if (control->replacement->generate() != 0) {
			control->valid = false;
			return -1;
		}
		control->replaced++;
		if (control->cancel_after_replace)
			return -1;
	}
	if (control->mode == 4 || control->mode == 5) {
		size_t capacity = (size_t)size;
		memset(buf, 'R', capacity);
		uintptr_t address = (uintptr_t)buf;
		control->watched_address = address;
		control->watching = true;
		control->capacity = capacity;
		if (control->bad_reports < UINT_MAX)
			control->bad_reports++;
		if (control->mode == 4)
			return -1;
		if (size == INT_MAX) {
			control->valid = false;
			return -1;
		}
		return size + 1;
	}
	if (control->mode == 1) {
		size_t capacity = (size_t)size;
		memset(buf, control->wrong ? 'G' : 'F', capacity);
		return size;
	}
	if (control->mode == 2) {
		if (control->wrong) {
			buf[0] = 'x';
			return 1;
		}
		return 0;
	}
	if (size < 4) {
		control->valid = false;
		return -1;
	}
	const uint8_t binary[] = {'p', 0, 'a', 255};
	const char text[] = "pass";
	const void *source = binary;
	if (control->mode == 3)
		source = text;
	memcpy(buf, source, 4);
	if (control->wrong)
		buf[2] = 'b';
	return 4;
}

#ifdef DSSH_CRYPTO_OPENSSL
static unsigned int ui_observations;

static int
ui_open(UI *ui)
{
	(void)ui;
	if (ui_observations < UINT_MAX)
		ui_observations++;
	return 1;
}

static int
ui_write(UI *ui, UI_STRING *text)
{
	(void)ui;
	(void)text;
	if (ui_observations < UINT_MAX)
		ui_observations++;
	return 1;
}

static int
ui_cancel(UI *ui, UI_STRING *text)
{
	(void)ui;
	(void)text;
	if (ui_observations < UINT_MAX)
		ui_observations++;
	return -1;
}
#endif

static bool
password_case(const struct key_file_ops *ops, unsigned int mode,
    unsigned int *completed, unsigned int *rejected, unsigned int *no_ui)
{
	char path[] = "/tmp/dssh-key-password-XXXXXX";
	char candidate[512], before[512], after[512];
	uint8_t candidate_public[512], before_public[512];
	size_t candidate_public_len = 0, before_public_len = 0;
	size_t candidate_len = 0, before_len = 0, after_len = 0;
	bool created = false;
	bool valid = false;
	FILE *fp = NULL;
	struct password_control password = {.mode = mode, .valid = true};
#ifdef DSSH_CRYPTO_OPENSSL
	UI_METHOD *method = NULL;
	const UI_METHOD *saved = NULL;
	bool installed = false;
	UI *probe = NULL;
#endif
	if (!start(ops) || !temporary(path, &created)
	    || ops->save(path, password_callback, &password) != 0
	    || !snapshot(ops, candidate, &candidate_len)
	    || !public_snapshot(ops, candidate_public, &candidate_public_len))
		goto done;
	fp = fopen(path, "rb");
	if (fp == NULL)
		goto done;
	char header[128];
	bool encrypted = fgets(header, sizeof(header), fp) != NULL
	    && strcmp(header, "-----BEGIN ENCRYPTED PRIVATE KEY-----\n") == 0;
	if (fclose(fp) != 0)
		encrypted = false;
	fp = NULL;
	if (!encrypted || ops->generate() != 0 || !snapshot(ops, before, &before_len)
	    || !public_snapshot(ops, before_public, &before_public_len)
	    || (before_len == candidate_len && memcmp(before, candidate, before_len) == 0))
		goto done;
	if (mode == 3) {
#ifdef DSSH_CRYPTO_OPENSSL
		method = UI_create_method("deucessh native UI observation");
		if (method == NULL || UI_method_set_opener(method, ui_open) != 0
		    || UI_method_set_writer(method, ui_write) != 0
		    || UI_method_set_reader(method, ui_cancel) != 0)
			goto done;
		saved = UI_get_default_method();
		UI_set_default_method(method);
		installed = true;
		ui_observations = 0;
		probe = UI_new();
		char ignored[8];
		if (probe == NULL || UI_add_input_string(probe, "observation", 0,
		        ignored, 0, 7) < 0 || UI_process(probe) >= 0 || ui_observations == 0)
			goto done;
		UI_free(probe);
		probe = NULL;
#endif
		valid = true;
		for (unsigned int attempt = 0; attempt < 2; attempt++) {
#ifdef DSSH_CRYPTO_OPENSSL
			ui_observations = 0;
#endif
			char opaque_password[] = "pass";
			int ret = ops->load(path, NULL, attempt == 0 ? NULL : opaque_password);
			bool unchanged = snapshot(ops, after, &after_len) && before_len == after_len
			    && memcmp(before, after, before_len) == 0;
			if (ret < 0)
				(*rejected)++;
			bool quiet = true;
#ifdef DSSH_CRYPTO_OPENSSL
			quiet = ui_observations == 0;
#endif
			if (quiet)
				(*no_ui)++;
			valid = ret < 0 && unchanged && quiet
			    && signable(ops, before_public, before_public_len) && valid;
		}
	}
	else {
		password.wrong = true;
		int ret = ops->load(path, password_callback, &password);
		bool unchanged = snapshot(ops, after, &after_len) && before_len == after_len
		    && memcmp(before, after, before_len) == 0;
		if (ret < 0)
			(*rejected)++;
		valid = ret < 0 && unchanged && signable(ops, before_public, before_public_len);
		password.wrong = false;
		ret = ops->load(path, password_callback, &password);
		valid = ret == 0 && snapshot(ops, after, &after_len)
		    && candidate_len == after_len && memcmp(candidate, after, candidate_len) == 0
		    && password.valid && password.reads >= 2 && password.writes >= 1
		    && signable(ops, candidate_public, candidate_public_len) && valid;
	}
	(*completed)++;
done:
#ifdef DSSH_CRYPTO_OPENSSL
	if (probe != NULL)
		UI_free(probe);
	if (installed)
		UI_set_default_method(saved);
	if (method != NULL)
		UI_destroy_method(method);
#endif
	if (fp != NULL && fclose(fp) != 0)
		valid = false;
	dssh_test_key_stdio_reset();
	dssh_test_alloc_reset();
	dssh_test_reset_global_config();
	if (created && unlink(path) != 0)
		valid = false;
	return valid;
}

static int
password_cases(unsigned int mode)
{
	bool valid = true;
	unsigned int completed = 0, rejected = 0, quiet = 0;
	for (size_t i = 0; i < sizeof(algorithms) / sizeof(algorithms[0]); i++) {
		bool one = password_case(&algorithms[i], mode, &completed, &rejected, &quiet);
		valid = one && valid;
	}
	printf("key-import password: mode=%u algorithms=3 completed=%u rejected=%u quiet=%u valid=%u\n",
	    mode, completed, rejected, quiet, valid ? 1U : 0U);
	ASSERT_TRUE(valid && completed == 3 && rejected == (mode == 3 ? 6U : 3U));
	return TEST_PASS;
}

static int
test_password_binary(void)
{
	return password_cases(0);
}

static int
test_password_full(void)
{
	return password_cases(1);
}

static int
test_password_empty(void)
{
	return password_cases(2);
}

static int
test_no_implicit_password(void)
{
	return password_cases(3);
}



static void
password_cleared(const void *ptr, size_t length, void *opaque)
{
	struct password_control *control = opaque;
	uintptr_t address = (uintptr_t)ptr;
	if (!control->watching || control->watched_address != address)
		return;
	if (length != control->capacity)
		control->zero = false;
	const uint8_t *bytes = ptr;
	for (size_t i = 0; i < length; i++) {
		if (bytes[i] != 0)
			control->zero = false;
	}
	control->watching = false;
	if (control->cleared < UINT_MAX)
		control->cleared++;
}

static bool
password_error_case(const struct key_file_ops *ops, unsigned int *rejected,
    unsigned int *cleared)
{
	char input[] = "/tmp/dssh-key-pw-input-XXXXXX";
	char output[] = "/tmp/dssh-key-pw-output-XXXXXX";
	bool a = false, b = false;
	bool valid = false;
	char before[512], after[512];
	uint8_t public_blob[512];
	size_t before_len = 0, after_len = 0, public_len = 0;
	struct password_control good = {.mode = 3, .valid = true};
	if (!start(ops) || !temporary(input, &a) || !temporary(output, &b)
	    || ops->save(input, password_callback, &good) != 0 || ops->generate() != 0
	    || !snapshot(ops, before, &before_len) || !public_snapshot(ops, public_blob, &public_len))
		goto done;
	valid = true;
	for (unsigned int mode = 4; mode <= 5; mode++) {
		struct password_control bad = {.mode = mode, .valid = true, .zero = true};
		dssh_test_key_observe_clear(password_cleared, &bad);
		int ret = ops->load(input, password_callback, &bad);
		if (bad.watching)
			bad.zero = false;
		bad.watching = false;
		int expected = mode == 4 ? DSSH_ERROR_INIT : DSSH_ERROR_INVALID;
		if (ret == expected)
			(*rejected)++;
		bool unchanged = snapshot(ops, after, &after_len) && before_len == after_len
		    && memcmp(before, after, before_len) == 0;
		valid = ret == expected && unchanged && signable(ops, public_blob, public_len) && valid;
		ret = ops->save(output, password_callback, &bad);
		if (bad.watching)
			bad.zero = false;
		bad.watching = false;
		if (ret == expected)
			(*rejected)++;
		valid = ret == expected && signable(ops, public_blob, public_len) && bad.valid
		    && bad.reads >= 1 && bad.writes >= 1 && bad.bad_reports >= 2
		    && bad.cleared == bad.bad_reports && bad.zero && valid;
		*cleared += bad.cleared;
		dssh_test_key_observe_clear(NULL, NULL);
	}
done:
	dssh_test_key_stdio_reset();
	dssh_test_alloc_reset();
	dssh_test_reset_global_config();
	if (a && unlink(input) != 0)
		valid = false;
	if (b && unlink(output) != 0)
		valid = false;
	return valid;
}

static int
test_password_errors(void)
{
	bool valid = true;
	unsigned int rejected = 0, cleared = 0;
	for (size_t i = 0; i < sizeof(algorithms) / sizeof(algorithms[0]); i++) {
		bool one = password_error_case(&algorithms[i], &rejected, &cleared);
		valid = one && valid;
	}
	printf("key-import password errors: algorithms=3 modes=2 rejected=%u cleared=%u valid=%u\n",
	    rejected, cleared, valid ? 1U : 0U);
	ASSERT_TRUE(valid && rejected == 12 && cleared >= 12);
	return TEST_PASS;
}


static bool
replacement_case(const struct key_file_ops *ops, bool cancel, unsigned int *completed)
{
	char path[] = "/tmp/dssh-key-callback-XXXXXX";
	bool created = false;
	bool valid = false;
	char original[512], replacement[512], after[512];
	uint8_t original_public[512], replacement_public[512];
	size_t original_len = 0, replacement_len = 0, after_len = 0;
	size_t original_public_len = 0, replacement_public_len = 0;
	struct password_control password = {.mode = 3, .valid = true,
	    .replacement = ops, .cancel_after_replace = cancel};
	if (!start(ops) || !temporary(path, &created) || !snapshot(ops, original, &original_len)
	    || !public_snapshot(ops, original_public, &original_public_len))
		goto done;
	int ret = ops->save(path, password_callback, &password);
	if (!password.valid || password.replaced != 1 || (cancel ? ret >= 0 : ret != 0)
	    || !snapshot(ops, replacement, &replacement_len)
	    || !public_snapshot(ops, replacement_public, &replacement_public_len)
	    || (original_len == replacement_len && memcmp(original, replacement, original_len) == 0)
	    || !signable(ops, replacement_public, replacement_public_len))
		goto done;
	valid = true;
	if (!cancel) {
		password.replacement = NULL;
		valid = ops->load(path, password_callback, &password) == 0
		    && snapshot(ops, after, &after_len) && original_len == after_len
		    && memcmp(original, after, original_len) == 0
		    && signable(ops, original_public, original_public_len);
	}
	(*completed)++;
done:
	dssh_test_key_stdio_reset();
	dssh_test_alloc_reset();
	dssh_test_reset_global_config();
	if (created && unlink(path) != 0)
		valid = false;
	return valid;
}

static int
test_callback_replacement(void)
{
	bool valid = true;
	unsigned int completed = 0;
	for (size_t i = 0; i < sizeof(algorithms) / sizeof(algorithms[0]); i++) {
		for (unsigned int cancel = 0; cancel < 2; cancel++) {
			bool one = replacement_case(&algorithms[i], cancel != 0, &completed);
			valid = one && valid;
		}
	}
	printf("key-file callback target: algorithms=3 modes=2 completed=%u valid=%u\n",
	    completed, valid ? 1U : 0U);
	ASSERT_TRUE(valid && completed == 6);
	return TEST_PASS;
}

#ifdef DSSH_CRYPTO_OPENSSL
static bool
reference_error_case(const struct key_file_ops *ops)
{
	char path[] = "/tmp/dssh-key-retain-XXXXXX";
	bool created = false;
	bool valid = false;
	FILE *fp = NULL;
	uint8_t public_blob[512];
	size_t public_len = 0;
	const char marker[] = "retained\n";
	char actual[sizeof(marker)];
	if (!start(ops) || !temporary(path, &created)
	    || !public_snapshot(ops, public_blob, &public_len))
		goto done;
	fp = fopen(path, "wb");
	if (fp == NULL)
		goto done;
	bool written = fwrite(marker, 1, sizeof(marker) - 1, fp) == sizeof(marker) - 1;
	if (fclose(fp) != 0)
		written = false;
	fp = NULL;
	if (!written)
		goto done;
	dssh_test_key_stdio_reset();
	dssh_test_ossl_fail_after(0);
	int ret = ops->save(path, NULL, NULL);
	int calls = dssh_test_ossl_count();
	dssh_test_ossl_reset();
	if (ret != DSSH_ERROR_INIT || calls != 1 || dssh_test_key_close_calls() != 0
	    || !signable(ops, public_blob, public_len))
		goto done;
	fp = fopen(path, "rb");
	if (fp == NULL)
		goto done;
	valid = fread(actual, 1, sizeof(actual), fp) == sizeof(marker) - 1
	    && ferror(fp) == 0 && memcmp(actual, marker, sizeof(marker) - 1) == 0;
	if (fclose(fp) != 0)
		valid = false;
	fp = NULL;
done:
	dssh_test_ossl_reset();
	dssh_test_key_stdio_reset();
	dssh_test_alloc_reset();
	dssh_test_reset_global_config();
	if (fp != NULL && fclose(fp) != 0)
		valid = false;
	if (created && unlink(path) != 0)
		valid = false;
	return valid;
}

static int
test_reference_failure(void)
{
	bool valid = true;
	for (size_t i = 0; i < sizeof(algorithms) / sizeof(algorithms[0]); i++) {
		bool one = reference_error_case(&algorithms[i]);
		valid = one && valid;
	}
	printf("key-file native retain: algorithms=3 rejected-before-open=%u\n", valid ? 3U : 0U);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}
#endif

static int
test_file_length_models(void)
{
	const size_t limits[] = {0, 1, 7, 255, 32767, 65535};
	unsigned long comparisons = 0;
	for (long value = -1; value <= 65536L; value++) {
		for (size_t i = 0; i < sizeof(limits) / sizeof(limits[0]); i++) {
			size_t output = 42;
			int ret = dssh_key_file_length(value, limits[i], &output);
			bool accepted = value > 0 && value <= 65535L;
			size_t expected = 0;
			if (accepted) {
				size_t small = (size_t)value;
				expected = small;
				accepted = small <= limits[i];
			}
			ASSERT_TRUE(accepted ? ret == 0 && output == expected : ret < 0 && output == 42);
			comparisons++;
		}
	}
	size_t output = 42;
	int ret = dssh_key_file_length(LONG_MAX, SIZE_MAX, &output);
#if LONG_MAX > SIZE_MAX
	ASSERT_TRUE(ret == DSSH_ERROR_INVALID && output == 42);
#else
	size_t maximum_long = (size_t)LONG_MAX;
	ASSERT_TRUE(ret == 0 && output == maximum_long);
#endif
	printf("key-import file length: values=65538 limits=6 comparisons=%lu terminal-valid=1\n", comparisons);
	return TEST_PASS;
}

/* Positive controls forward to real native stdio before reporting a fault. */
static int
test_stdio_control(void)
{
	unsigned int completed = 0;
	const char payload[] = "stdio-hook";
	for (unsigned int kind = 0; kind < 3; kind++) {
		char buffer[BUFSIZ] = {0}, received[sizeof(payload)] = {0};
		dssh_test_key_stdio_reset();
		FILE *fp = tmpfile();
		if (fp == NULL)
			continue;
		dssh_test_key_watch_stream();
		if (kind == 1)
			dssh_test_key_setvbuf_error_once();
		int configured = dssh_test_key_file_setvbuf(fp, buffer, _IOFBF, sizeof(buffer));
		bool native_data = fwrite(payload, 1, sizeof(payload), fp) == sizeof(payload)
		    && fseek(fp, 0, SEEK_SET) == 0 && fread(received, 1, sizeof(received), fp) == sizeof(received)
		    && memcmp(received, payload, sizeof(payload)) == 0;
		bool valid = configured == (kind == 1 ? EOF : 0) && native_data;
		if (kind == 2)
			dssh_test_key_close_error_once();
		int closed = dssh_test_key_file_close(fp);
		dssh_key_file_clear(buffer, sizeof(buffer));
		/* This application test is compiled without DSSH_TESTING; feed the
		 * real post-cleanse notification to the observer explicitly. */
		dssh_test_key_file_cleared(buffer, sizeof(buffer));
		struct dssh_test_key_stream_report report = dssh_test_key_stream_report();
		valid = valid && closed == (kind == 2 ? EOF : 0) && report.valid && !report.active
		    && report.configurations == 1 && report.closes == 1 && report.clears == 1 && report.nonzero == 1
		    && report.injected == (kind == 1 ? 1U : 0U) && report.native_errors == 0
		    && dssh_test_key_close_calls() == 1 && dssh_test_key_close_errors() == (kind == 2 ? 1U : 0U)
		    && dssh_test_key_native_close_errors() == 0;
		printf("key stdio control: kind=%u native-data=%u native-setup-errors=%u closed=%u cleared=%u nonzero=%u injected=%u valid=%u\n",
		    kind, native_data ? 1U : 0U, report.native_errors, report.closes, report.clears, report.nonzero, report.injected, valid ? 1U : 0U);
		if (valid)
			completed++;
	}
	dssh_test_key_stdio_reset();
	ASSERT_EQ(completed, 3);
	return TEST_PASS;
}

struct stream_password_state {
	unsigned int calls;
	bool valid;
};
static int
stream_password(char *buf, int size, int writing, void *data)
{
	struct stream_password_state *state = data;
	const char phrase[] = {'s', 't', 'r', 0, 'e', 'a', 'm', 's'};
	if (buf == NULL || size < 8 || (writing != 0 && writing != 1) || state->calls == UINT_MAX) {
		state->valid = false;
		return -1;
	}
	state->calls++;
	memcpy(buf, phrase, sizeof(phrase));
	return 8;
}

/* kind0 normal, kind1 setup failure, kind2 reported close failure,
 * kind3 actual short read, kind4 native flush error under a process-local limit,
 * kind5/6 pre-close native write errors with a16-byte supplied-buffer extent
 * and a0/32-byte file limit. Their reported close success is modeled only
 * after the real stream has been closed and its actual result counted.
 * kind7 is the successful native small-buffer control.
 * All storage observations precede destruction of the real local buffer. */
static bool
stream_case(const struct key_file_ops *ops, bool save, bool encrypted, unsigned int kind)
{
	char candidate_path[] = "/tmp/dssh-stream-candidate-XXXXXX";
	char output_path[] = "/tmp/dssh-stream-output-XXXXXX";
	bool candidate_created = false, output_created = false, valid = false;
	uint8_t candidate_public[512], before_public[512], after_public[512], held_copy[1024];
	size_t candidate_length = 0, before_length = 0, after_length = 0;
	char before_text[512], after_text[512];
	size_t before_text_length = 0, after_text_length = 0;
	struct dssh_bytes_result held = {0}, next = {0};
	struct stream_password_state password_state = {.valid = true};
	struct dssh_test_key_stream_report report = {0};
	unsigned int callbacks = 0, close_calls = 0, close_errors = 0, native_close_errors = 0, short_reads = 0;
	unsigned int native_write_errors = 0;
	bool write_failure = kind == 5 || kind == 6;
	bool small_buffer = write_failure || kind == 7;
	int64_t output_length = -1;
	int status = DSSH_ERROR_INIT;
#ifndef _WIN32
	struct rlimit saved_limit;
	struct stat output_before;
	bool limited = false, signal_changed = false;
	void (*saved_handler)(int) = SIG_ERR;
#endif
	if (!start(ops) || !temporary(candidate_path, &candidate_created)
	    || !temporary(output_path, &output_created)
	    || !public_snapshot(ops, candidate_public, &candidate_length)
	    || ops->save(candidate_path, encrypted ? stream_password : NULL,
	        encrypted ? &password_state : NULL) != 0
	    || !password_state.valid || ops->generate() != 0
	    || !public_snapshot(ops, before_public, &before_length)
	    || !snapshot(ops, before_text, &before_text_length)
	    || (candidate_length == before_length && memcmp(candidate_public, before_public, before_length) == 0))
		goto done;
	dssh_key_algo key = find_key_algo(ops->name);
	dssh_key_algo_ctx *prior = key->ctx;
	const uint8_t *prior_cache = NULL;
	size_t prior_cache_length = 0;
	const uint8_t message[] = {'s', 't', 'r', 'e', 'a', 'm'};
	if (key->pubkey(&prior_cache, &prior_cache_length, prior) != 0
	    || prior_cache_length != before_length
	    || key->sign(&held, message, sizeof(message), prior) != 0
	    || held.bytes.data == NULL || held.bytes.len == 0 || held.bytes.len > sizeof(held_copy)
	    || held.lease.release == NULL)
		goto done;
	memcpy(held_copy, held.bytes.data, held.bytes.len);
	password_state = (struct stream_password_state){.valid = true};
	dssh_test_key_stdio_reset();
	dssh_test_key_watch_stream();
	if (kind == 1)
		dssh_test_key_setvbuf_error_once();
	else if (kind == 2)
		dssh_test_key_close_error_once();
	else if (kind == 3)
		dssh_test_key_read_short_once();
	if (small_buffer)
		dssh_test_key_small_buffer();
	if (write_failure)
		dssh_test_key_close_success_once();
#ifndef _WIN32
	if (kind == 4 || write_failure) {
		struct stat info;
		if (!save || stat(candidate_path, &info) != 0 || info.st_size < 1
		    || (kind == 4 && info.st_size >= BUFSIZ) || (write_failure && info.st_size <= 32)
		    || stat(output_path, &output_before) != 0 || getrlimit(RLIMIT_FSIZE, &saved_limit) != 0)
			goto done;
		saved_handler = signal(SIGXFSZ, SIG_IGN);
		if (saved_handler == SIG_ERR)
			goto done;
		signal_changed = true;
		struct rlimit limit = saved_limit;
		limit.rlim_cur = kind == 6 ? 32U : 0U;
		if (setrlimit(RLIMIT_FSIZE, &limit) != 0)
			goto done;
		limited = true;
	}
#else
	if (kind == 4 || write_failure)
		goto done;
#endif
	status = save ? ops->save(output_path, encrypted ? stream_password : NULL,
	    encrypted ? &password_state : NULL) : ops->load(candidate_path,
	    encrypted ? stream_password : NULL, encrypted ? &password_state : NULL);
	report = dssh_test_key_stream_report();
	callbacks = password_state.calls;
	close_calls = dssh_test_key_close_calls();
	close_errors = dssh_test_key_close_errors();
	native_close_errors = dssh_test_key_native_close_errors();
	native_write_errors = dssh_test_key_native_write_errors();
	short_reads = dssh_test_key_short_read_calls();
	valid = true;
#ifndef _WIN32
	if (limited) {
		limited = false;
		if (setrlimit(RLIMIT_FSIZE, &saved_limit) != 0)
			valid = false;
	}
	if (signal_changed) {
		signal_changed = false;
		if (signal(SIGXFSZ, saved_handler) == SIG_ERR)
			valid = false;
	}
	if (write_failure) {
		struct stat info;
		valid = valid && stat(output_path, &info) == 0 && S_ISREG(info.st_mode)
		    && info.st_dev == output_before.st_dev && info.st_ino == output_before.st_ino
		    && info.st_uid == geteuid() && info.st_nlink == 1 && (info.st_mode & 0777) == 0600
		    && info.st_size == (kind == 6 ? 32 : 0);
		if (valid)
			output_length = kind == 6 ? 32 : 0;
	}
#endif
	dssh_test_key_stdio_reset();
	unsigned int expected_callbacks = encrypted && kind != 1 && kind != 3 ? 1U : 0U;
#ifdef DSSH_CRYPTO_BOTAN
	if (!save && kind == 2)
		expected_callbacks = 0;
#endif
	bool replaced = !save && kind == 0;
	const uint8_t *expected_public = replaced ? candidate_public : before_public;
	size_t expected_length = replaced ? candidate_length : before_length;
	const uint8_t *current_cache = NULL;
	size_t current_cache_length = 0;
	valid = valid && report.valid && !report.active && report.configurations == 1
	    && report.closes == 1 && report.clears == 1
	    && (small_buffer ? report.nonzero <= 1 : report.nonzero == (kind == 1 ? 0U : 1U))
	    && report.injected == (kind == 1 ? 1U : 0U) && report.native_errors == 0
	    && report.native_capacity == (small_buffer ? 16U : BUFSIZ)
	    && report.errors_before_close == (write_failure ? 1U : 0U)
	    && report.close_successes == (write_failure ? 1U : 0U)
	    && close_calls == 1 && close_errors == (kind == 2 ? 1U : 0U)
	    && (write_failure ? native_close_errors <= 1 : native_close_errors == (kind == 4 ? 1U : 0U))
	    && short_reads == (kind == 3 ? 1U : 0U)
	    && status == (kind == 0 || kind == 7 ? 0 : DSSH_ERROR_INIT)
	    && password_state.valid && callbacks == expected_callbacks
	    && public_snapshot(ops, after_public, &after_length)
	    && after_length == expected_length && memcmp(after_public, expected_public, expected_length) == 0
	    && key->pubkey(&current_cache, &current_cache_length, key->ctx) == 0
	    && current_cache_length == after_length
	    && (replaced ? key->ctx != prior && current_cache != prior_cache
	        : key->ctx == prior && current_cache == prior_cache)
	    && (replaced || (snapshot(ops, after_text, &after_text_length)
	        && before_text_length == after_text_length && memcmp(before_text, after_text, before_text_length) == 0))
	    && memcmp(held_copy, held.bytes.data, held.bytes.len) == 0
	    && key->verify(before_public, before_length, held.bytes.data, held.bytes.len, message, sizeof(message)) == 0;
#ifdef DSSH_CRYPTO_BOTAN
	if (write_failure)
		valid = valid && native_write_errors == 1;
#else
	if (write_failure)
		valid = valid && native_write_errors == 0; /* Provider-internal stdio is not interposed. */
#endif
	if (valid)
		valid = key->sign(&next, message, sizeof(message), key->ctx) == 0
		    && next.bytes.data != NULL && next.bytes.len != 0 && next.lease.release != NULL
		    && next.bytes.data != held.bytes.data
		    && key->verify(expected_public, expected_length, next.bytes.data, next.bytes.len, message, sizeof(message)) == 0;
	if (valid && save && (kind == 0 || kind == 7)) {
		password_state = (struct stream_password_state){.valid = true};
		valid = ops->load(output_path, encrypted ? stream_password : NULL,
		    encrypted ? &password_state : NULL) == 0 && password_state.valid
		    && public_snapshot(ops, after_public, &after_length) && after_length == before_length
		    && memcmp(after_public, before_public, before_length) == 0
		    && signable(ops, before_public, before_length)
		    && memcmp(held_copy, held.bytes.data, held.bytes.len) == 0
		    && key->verify(before_public, before_length, held.bytes.data, held.bytes.len, message, sizeof(message)) == 0;
	}
done:
#ifndef _WIN32
	if (limited && setrlimit(RLIMIT_FSIZE, &saved_limit) != 0)
		valid = false;
	if (signal_changed && signal(SIGXFSZ, saved_handler) == SIG_ERR)
		valid = false;
#endif
	dssh_result_release(&next.lease);
	dssh_result_release(&held.lease);
	dssh_test_key_stdio_reset();
	dssh_test_alloc_reset();
	dssh_test_reset_global_config();
	if (candidate_created && unlink(candidate_path) != 0)
		valid = false;
	if (output_created && unlink(output_path) != 0)
		valid = false;
	printf("key stdio: algorithm=%s save=%u encrypted=%u kind=%u status=%d configured=%u closed=%u cleared=%u "
	    "nonzero=%u injected=%u native-setup-errors=%u close-calls=%u close-errors=%u native-close-errors=%u "
	    "short-reads=%u callbacks=%u native-write-errors=%u before-close-errors=%u close-successes=%u "
	    "native-capacity=%zu output-length=%" PRId64 " valid=%u\n", ops->name, save ? 1U : 0U, encrypted ? 1U : 0U, kind, status,
	    report.configurations, report.closes, report.clears, report.nonzero, report.injected, report.native_errors,
	    close_calls, close_errors, native_close_errors, short_reads, callbacks, native_write_errors,
	    report.errors_before_close, report.close_successes, report.native_capacity, output_length, valid ? 1U : 0U);
	return valid;
}

static int
stream_cases(unsigned int kind)
{
	unsigned int completed = 0, expected = kind == 4 ? 2U : kind == 3 || kind >= 5 ? 6U : 12U;
	size_t count = kind == 4 ? 1 : sizeof(algorithms) / sizeof(algorithms[0]);
	for (size_t i = 0; i < count; i++) {
		for (unsigned int encrypted = 0; encrypted < 2; encrypted++) {
			for (unsigned int save = 0; save < 2; save++) {
				if ((kind == 3 && save != 0) || (kind >= 4 && save == 0))
					continue;
				if (stream_case(&algorithms[i], save != 0, encrypted != 0, kind))
					completed++;
			}
		}
	}
	printf("key stdio summary: kind=%u expected=%u completed=%u\n", kind, expected, completed);
	ASSERT_EQ(completed, expected);
	return TEST_PASS;
}
static int
test_stdio_success(void)
{
	return stream_cases(0);
}
static int
test_stdio_setup_failure(void)
{
	return stream_cases(1);
}
static int
test_stdio_close_failure(void)
{
	return stream_cases(2);
}
#ifdef DSSH_CRYPTO_BOTAN
static int
test_stdio_short_read(void)
{
	return stream_cases(3);
}
#endif
#ifndef _WIN32
static int
test_stdio_native_flush(void)
{
	return stream_cases(4);
}
static int
test_stdio_native_write_zero(void)
{
	return stream_cases(5);
}
static int
test_stdio_native_write_partial(void)
{
	return stream_cases(6);
}
static int
test_stdio_small_buffer_control(void)
{
	return stream_cases(7);
}
#endif

static struct dssh_test_entry tests[] = {
	{"file_length_models", test_file_length_models},
	{"stdio_control", test_stdio_control},
	{"stdio_success", test_stdio_success},
	{"stdio_setup_failure", test_stdio_setup_failure},
	{"stdio_close_failure", test_stdio_close_failure},
#ifdef DSSH_CRYPTO_BOTAN
	{"stdio_short_read", test_stdio_short_read},
#endif
#ifndef _WIN32
	{"stdio_native_flush", test_stdio_native_flush},
	{"stdio_native_write_zero", test_stdio_native_write_zero},
	{"stdio_native_write_partial", test_stdio_native_write_partial},
	{"stdio_small_buffer_control", test_stdio_small_buffer_control},
#endif
	{"callback_replacement", test_callback_replacement},
#ifdef DSSH_CRYPTO_OPENSSL
	{"reference_failure", test_reference_failure},
#endif
	{"close_success", test_close_success},
	{"password_errors", test_password_errors},
	{"password_binary", test_password_binary},
	{"password_full", test_password_full},
	{"password_empty", test_password_empty},
	{"no_implicit_password", test_no_implicit_password},
	{"close_failure", test_close_failure},
#ifdef DSSH_CRYPTO_BOTAN
	{"input_retirement", test_input_retirement},
#endif
};

DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
