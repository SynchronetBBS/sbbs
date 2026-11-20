/* Actual builtin key text/file paths plus independent bounded arithmetic. */
#include <unistd.h>
#include "dssh_test.h"
#include "dssh_test_internal.h"
#include "dssh_test_alloc.h"
#include "dssh_test_ossl.h"
#include "deucessh-algorithms.h"
/* Compile this private helper with the same stdio seam as the library. */
#define DSSH_TESTING
#include "key_algo/key-text.h"
#undef DSSH_TESTING
#undef fread
#undef fwrite
#undef fclose
#undef fputc

struct text_ops {
	const char *name;
	const char *prefix;
	const char *cache;
	int (*reg)(void);
	int (*load)(const char *, dssh_pem_password_cb, void *);
	int64_t (*format)(char *, size_t);
	int (*save)(const char *);
};

static const struct text_ops algorithms[] = {
	{"ssh-ed25519", "ssh-ed25519", "DSSH_TEST_ED25519_KEY", dssh_register_ssh_ed25519,
	    dssh_ed25519_load_key_file, dssh_ed25519_get_pub_str, dssh_ed25519_save_pub_file},
	{"rsa-sha2-256", "ssh-rsa", "DSSH_TEST_RSA_KEY", dssh_register_rsa_sha2_256,
	    dssh_rsa_sha2_256_load_key_file, dssh_rsa_sha2_256_get_pub_str, dssh_rsa_sha2_256_save_pub_file},
	{"rsa-sha2-512", "ssh-rsa", "DSSH_TEST_RSA_KEY", dssh_register_rsa_sha2_512,
	    dssh_rsa_sha2_512_load_key_file, dssh_rsa_sha2_512_get_pub_str, dssh_rsa_sha2_512_save_pub_file},
};

static bool
filled(const void *ptr, size_t length, uint8_t value)
{
	const uint8_t *bytes = ptr;
	for (size_t i = 0; i < length; i++) {
		if (bytes[i] != value)
			return false;
	}
	return true;
}

#include "key_text_reference.h"

static bool
start(const struct text_ops *ops, char *expected, size_t *length)
{
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	dssh_test_key_stdio_reset();
	dssh_test_reset_global_config();
	const char *path = getenv(ops->cache);
	if (path == NULL || ops->reg() != 0 || ops->load(path, NULL, NULL) != 0)
		return false;
	dssh_key_algo key = find_key_algo(ops->name);
	const uint8_t *blob = NULL;
	size_t bytes = 0;
	return key != NULL && key->pubkey(&blob, &bytes, key->ctx) == 0
	    && blob != NULL && bytes <= 512 && key_text_reference(blob, bytes, ops->prefix, expected, 1024, length);
}

static int
test_formats(void)
{
	bool valid = true;
	for (size_t a = 0; a < sizeof(algorithms) / sizeof(algorithms[0]); a++) {
		const struct text_ops *ops = &algorithms[a];
		char expected[1024];
		size_t length = 0;
		bool one = start(ops, expected, &length);
		char frame[1040];
		if (one) {
			int64_t wanted = (int64_t)length; /* actual checked oracle extent <=1024 */
			dssh_test_alloc_fail_after(0);
			for (unsigned int n = 0; n < 16; n++) {
				memset(frame, 0xa5, sizeof(frame));
				one = ops->format(NULL, 0) == wanted && ops->format(NULL, sizeof(frame)) == wanted
				    && ops->format(frame + 8, 0) == wanted && filled(frame, sizeof(frame), 0xa5)
				    && ops->format(frame + 8, length - 1) == DSSH_ERROR_TOOLONG
				    && filled(frame, sizeof(frame), 0xa5)
				    && ops->format(frame + 8, length) == wanted
				    && memcmp(frame + 8, expected, length) == 0 && filled(frame, 8, 0xa5)
				    && filled(frame + 8 + length, sizeof(frame) - 8 - length, 0xa5) && one;
			}
			one = dssh_test_alloc_count() == 0 && one;
		}
		dssh_test_alloc_reset();
		dssh_test_reset_global_config();
		valid = one && valid;
	}
	printf("key-text formats: families=3 repeats=16 valid=%u\n", valid ? 1U : 0U);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#ifdef DSSH_CRYPTO_OPENSSL
static int
test_encoder_reports(void)
{
	bool valid = true;
	unsigned int rejected = 0, accepted = 0;
	for (size_t a = 0; a < sizeof(algorithms) / sizeof(algorithms[0]); a++) {
		const struct text_ops *ops = &algorithms[a];
		char expected[1024], frame[1040];
		size_t length = 0;
		bool one = start(ops, expected, &length);
		if (one) {
			size_t count = length - strlen(ops->prefix) - 2;
			if (count > INT_MAX - 1)
				one = false;
			else {
				int correct = (int)count;
				const int reports[] = {-1, 0, correct - 1, correct + 1, correct};
				for (size_t i = 0; i < sizeof(reports) / sizeof(reports[0]); i++) {
					memset(frame, 0xa5, sizeof(frame));
					dssh_test_ossl_encoded_length(reports[i]);
					int64_t ret = ops->format(frame + 8, length);
					dssh_test_ossl_reset();
					if (reports[i] == correct) {
						int64_t wanted = (int64_t)length;
						one = ret == wanted && memcmp(frame + 8, expected, length) == 0 && one;
						if (ret == wanted)
							accepted++;
					}
					else {
						one = ret == DSSH_ERROR_INIT && one;
						if (ret == DSSH_ERROR_INIT)
							rejected++;
					}
					one = filled(frame, 8, 0xa5) && filled(frame + 8 + length,
					    sizeof(frame) - 8 - length, 0xa5) && one;
				}
			}
		}
		valid = one && valid;
		dssh_test_reset_global_config();
	}
	printf("key-text encoder reports: families=3 rejected=%u positive=%u valid=%u\n",
	    rejected, accepted, valid ? 1U : 0U);
	ASSERT_TRUE(valid && rejected == 12 && accepted == 3);
	return TEST_PASS;
}
#endif

static bool
write_case(const struct text_ops *ops, unsigned int mode)
{
	char path[] = "/tmp/dssh-key-text-XXXXXX";
	char expected[1024], actual[1040];
	size_t length = 0;
	bool created = false, valid = false;
	FILE *fp = NULL;
	if (!start(ops, expected, &length))
		goto done;
	int fd = mkstemp(path);
	if (fd < 0)
		goto done;
	created = true;
	if (close(fd) != 0)
		goto done;
	dssh_test_key_stdio_reset();
	if (mode == 1 || mode == 2)
		dssh_test_key_write_short_after(mode - 1);
	if (mode == 3 || mode == 4)
		dssh_test_key_put_error_after(mode - 3);
	if (mode == 5)
		dssh_test_key_close_error_once();
	int ret = ops->save(path);
	valid = (mode == 0 ? ret == 0 : ret == DSSH_ERROR_INIT)
	    && dssh_test_key_close_calls() == 1 && dssh_test_key_native_close_errors() == 0
	    && dssh_test_key_native_write_errors() == 0
	    && dssh_test_key_close_errors() == (mode == 5 ? 1U : 0U)
	    && dssh_test_key_short_write_calls() == ((mode == 1 || mode == 2) ? 1U : 0U)
	    && dssh_test_key_put_errors() == ((mode == 3 || mode == 4) ? 1U : 0U)
	    && dssh_test_key_write_calls() == ((mode == 1 || mode == 3) ? 1U : 2U)
	    && dssh_test_key_put_calls() == (mode == 1 ? 0U : (mode == 2 || mode == 3) ? 1U : 2U);
	if (mode == 0) {
		fp = fopen(path, "rb");
		if (fp == NULL) {
			valid = false;
			goto done;
		}
		size_t count = fread(actual, 1, sizeof(actual), fp);
		valid = count == length && ferror(fp) == 0
		    && memcmp(actual, expected, length - 1) == 0 && actual[length - 1] == '\n' && valid;
	}
done:
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
test_file_stages(void)
{
	bool valid = true;
	unsigned int completed = 0;
	for (size_t a = 0; a < sizeof(algorithms) / sizeof(algorithms[0]); a++) {
		for (unsigned int mode = 0; mode < 6; mode++) {
			bool one = write_case(&algorithms[a], mode);
			completed += one ? 1U : 0U;
			valid = one && valid;
		}
	}
	printf("key-text file stages: families=3 modes=6 completed=%u valid=%u\n",
	    completed, valid ? 1U : 0U);
	ASSERT_TRUE(valid && completed == 18);
	return TEST_PASS;
}

static int
test_extent_models(void)
{
	const size_t limits[] = {0, 1, 15, 31, 32767, 65535};
	const size_t names[] = {7, 11};
	unsigned long comparisons = 0;
	for (unsigned long n = 0; n < 65536UL; n++) {
		size_t bytes = (size_t)n;
		for (size_t a = 0; a < sizeof(names) / sizeof(names[0]); a++) {
			uint64_t expected = names[a] + UINT64_C(2) + UINT64_C(4) * ((n + UINT64_C(2)) / 3);
			for (size_t b = 0; b < sizeof(limits) / sizeof(limits[0]); b++) {
				for (unsigned int small_int = 0; small_int < 2; small_int++) {
					size_t encoded_limit = small_int == 0 ? 32767 : 65535;
					size_t output = 42;
					int ret = dssh_key_text_extent(bytes, names[a], limits[b], encoded_limit, &output);
					bool accept = expected <= limits[b] && expected - names[a] - 2 <= encoded_limit;
					ASSERT_TRUE(accept ? ret == 0 && output == expected : ret == DSSH_ERROR_TOOLONG && output == 42);
					comparisons++;
				}
			}
		}
	}
	const size_t boundaries[] = {SIZE_MAX, SIZE_MAX - 1, SIZE_MAX - 2};
	for (size_t i = 0; i < sizeof(boundaries) / sizeof(boundaries[0]); i++) {
		size_t output = 42;
		ASSERT_EQ(dssh_key_text_size(boundaries[i], 7, &output), DSSH_ERROR_TOOLONG);
		ASSERT_EQ(output, 42);
		ASSERT_EQ(dssh_key_text_extent(1, boundaries[i], SIZE_MAX, SIZE_MAX, &output), DSSH_ERROR_TOOLONG);
		ASSERT_EQ(output, 42);
	}
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
		size_t limit = SIZE_MAX;
#if SIZE_MAX > INT64_MAX
		limit = INT64_MAX;
#endif
		limit -= names[i] + 2;
#if SIZE_MAX > INT_MAX
		if (limit > INT_MAX)
			limit = INT_MAX;
#endif
		size_t groups = limit / 4;
		size_t maximum_input = groups * 3;
		size_t expected = names[i] + 2 + groups * 4;
		size_t output = 42;
		ASSERT_EQ(dssh_key_text_size(maximum_input, names[i], &output), 0);
		ASSERT_EQ(output, expected);
		output = 42;
		ASSERT_EQ(dssh_key_text_size(maximum_input + 1, names[i], &output), DSSH_ERROR_TOOLONG);
		ASSERT_EQ(output, 42);
	}
	printf("key-text extents: values=65536 names=2 limits=6 int-models=2 comparisons=%lu native-edges=10\n", comparisons);
	return TEST_PASS;
}

static int
test_chunk_boundaries(void)
{
	uint8_t input[2306];
	char expected[3200], actual[3200];
	for (size_t i = 0; i < sizeof(input); i++) {
		uint8_t byte = (uint8_t)(i % 256);
		input[i] = byte;
	}
	const size_t lengths[] = {0, 1, 2, 3, 767, 768, 769, 770, 1536, 2306};
	bool valid = true;
	unsigned int completed = 0;
	for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
		char path[] = "/tmp/dssh-key-chunk-XXXXXX";
		bool created = false, one = false;
		FILE *fp = NULL;
		size_t length = 0;
		int fd = mkstemp(path);
		if (fd < 0)
			goto next;
		created = true;
		if (close(fd) != 0)
			goto next;
		if (!key_text_reference(input, lengths[i], "ssh-rsa", expected, sizeof(expected), &length))
			goto next;
		dssh_test_key_stdio_reset();
		dssh_test_alloc_fail_after(0);
		int ret = dssh_key_text_write(path, input, lengths[i], "ssh-rsa", 7);
		one = ret == 0 && dssh_test_alloc_count() == 0 && dssh_test_key_close_calls() == 1;
		dssh_test_alloc_reset();
		fp = fopen(path, "rb");
		if (fp == NULL) {
			one = false;
			goto next;
		}
		size_t count = fread(actual, 1, sizeof(actual), fp);
		one = count == length && ferror(fp) == 0 && memcmp(actual, expected, length - 1) == 0
		    && actual[length - 1] == '\n' && one;
next:
		if (fp != NULL && fclose(fp) != 0)
			one = false;
		if (created && unlink(path) != 0)
			one = false;
		dssh_test_alloc_reset();
		dssh_test_key_stdio_reset();
		completed += one ? 1U : 0U;
		valid = one && valid;
	}
	printf("key-text chunks: cases=10 completed=%u valid=%u\n", completed, valid ? 1U : 0U);
	ASSERT_TRUE(valid && completed == 10);
	return TEST_PASS;
}

static struct dssh_test_entry tests[] = {
	{"formats", test_formats},
	{"file_stages", test_file_stages},
	{"extent_models", test_extent_models},
	{"chunk_boundaries", test_chunk_boundaries},
#ifdef DSSH_CRYPTO_OPENSSL
	{"encoder_reports", test_encoder_reports},
#endif
};
DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
