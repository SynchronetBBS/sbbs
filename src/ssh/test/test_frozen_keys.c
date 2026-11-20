/* Selected A: real frozen key readers/exports and live signing results. */
#include <errno.h>
#include <limits.h>
#include <stdatomic.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>
#include "dssh_test.h"
#include "dssh_test_internal.h"
#include "deucessh-algorithms.h"

struct key_ops {
	int (*reg)(void);
	int (*load)(const char *, dssh_pem_password_cb, void *);
	int (*save)(const char *, dssh_pem_password_cb, void *);
	int (*save_public)(const char *);
	int64_t (*text)(char *, size_t);
	const char *name;
	const char *cache;
};
static const struct key_ops operations[] = {
	{dssh_register_ssh_ed25519, dssh_ed25519_load_key_file, dssh_ed25519_save_key_file,
	    dssh_ed25519_save_pub_file, dssh_ed25519_get_pub_str, "ssh-ed25519", "DSSH_TEST_ED25519_KEY"},
	{dssh_register_rsa_sha2_256, dssh_rsa_sha2_256_load_key_file, dssh_rsa_sha2_256_save_key_file,
	    dssh_rsa_sha2_256_save_pub_file, dssh_rsa_sha2_256_get_pub_str, "rsa-sha2-256", "DSSH_TEST_RSA_KEY"},
	{dssh_register_rsa_sha2_512, dssh_rsa_sha2_512_load_key_file, dssh_rsa_sha2_512_save_key_file,
	    dssh_rsa_sha2_512_save_pub_file, dssh_rsa_sha2_512_get_pub_str, "rsa-sha2-512", "DSSH_TEST_RSA_KEY"},
};
struct expected_key {
	const struct key_ops *ops;
	dssh_key_algo key;
	uint8_t public[512];
	size_t public_len;
	char text[1024];
	size_t text_len;
};
struct rendezvous {
	atomic_uint ready, finished;
	atomic_bool go, valid;
};
struct reader {
	struct expected_key *expected;
	struct rendezvous *rendezvous;
	char public_path[256], plain_path[256], encrypted_path[256];
	bool pause_once, valid;
	unsigned int reads, writes;
};

static bool
wait_go(atomic_bool *go)
{
	struct timespec start, now;
	if (timespec_get(&start, TIME_UTC) != TIME_UTC)
		return false;
	while (!atomic_load(go)) {
		if (timespec_get(&now, TIME_UTC) != TIME_UTC || difftime(now.tv_sec, start.tv_sec) > 15)
			return false;
		thrd_yield();
	}
	return true;
}

static int
password(char *buf, int size, int rwflag, void *opaque)
{
	static const char phrase[] = {'k', 'e', 'y', 0, 'p', 'a', 's', 's'};
	struct reader *reader = opaque;
	if (buf == NULL || size < 8 || (rwflag != 0 && rwflag != 1)) {
		reader->valid = false;
		return -1;
	}
	if (rwflag == 1) {
		if (reader->writes == UINT_MAX) {
			reader->valid = false;
			return -1;
		}
		reader->writes++;
		if (reader->pause_once) {
			reader->pause_once = false;
			atomic_fetch_add(&reader->rendezvous->ready, 1);
			if (!wait_go(&reader->rendezvous->go)) {
				atomic_store(&reader->rendezvous->valid, false);
				reader->valid = false;
				return -1;
			}
		}
	}
	else {
		if (reader->reads == UINT_MAX) {
			reader->valid = false;
			return -1;
		}
		reader->reads++;
	}
	memcpy(buf, phrase, sizeof(phrase));
	return 8;
}

static bool
verify(const struct expected_key *expected, const struct dssh_bytes_result *signature)
{
	const uint8_t message[] = {'f', 'r', 'o', 'z', 'e', 'n'};
	return signature->bytes.data != NULL && signature->bytes.len != 0 && signature->lease.release != NULL
	    && expected->key->verify(expected->public, expected->public_len,
	        signature->bytes.data, signature->bytes.len, message, sizeof(message)) == 0;
}

static int
sign(const struct expected_key *expected, struct dssh_bytes_result *signature)
{
	const uint8_t message[] = {'f', 'r', 'o', 'z', 'e', 'n'};
	return expected->key->sign(signature, message, sizeof(message), expected->key->ctx);
}

static int
read_and_export(void *opaque)
{
	struct reader *reader = opaque;
	struct expected_key *expected = reader->expected;
	struct dssh_bytes_result held = {0}, next = {0};
	uint8_t signature_copy[1024];
	if (sign(expected, &held) != 0 || !verify(expected, &held) || held.bytes.len > sizeof(signature_copy)) {
		reader->valid = false;
		goto done;
	}
	memcpy(signature_copy, held.bytes.data, held.bytes.len);
	for (unsigned int i = 0; i < 16; i++) {
		char text[1024];
		int64_t length = expected->ops->text(text, sizeof(text));
		if (length < 1 || length > 1024) {
			reader->valid = false;
			goto done;
		}
		size_t count = (size_t)length;
		if (count != expected->text_len || memcmp(text, expected->text, count) != 0) {
			reader->valid = false;
			goto done;
		}
	}
	if (expected->ops->save_public(reader->public_path) != 0
	    || expected->ops->save(reader->plain_path, NULL, NULL) != 0
	    || expected->ops->save(reader->encrypted_path, password, reader) != 0
	    || sign(expected, &next) != 0 || !verify(expected, &next)
	    || next.bytes.data == held.bytes.data || !verify(expected, &held)
	    || memcmp(held.bytes.data, signature_copy, held.bytes.len) != 0)
		reader->valid = false;
done:
	dssh_result_release(&next.lease);
	dssh_result_release(&held.lease);
	atomic_fetch_add(&reader->rendezvous->finished, 1);
	return 0;
}

static bool
prepare_expected(struct expected_key *expected, size_t index)
{
	expected->ops = &operations[index];
	const char *cache = getenv(expected->ops->cache);
	if (expected->ops->reg() != 0)
		return false;
	int ret = index == 2 ? dssh_rsa_sha2_512_generate_key(2048)
	    : cache == NULL ? DSSH_ERROR_INIT : expected->ops->load(cache, NULL, NULL);
	if (ret != 0)
		return false;
	expected->key = find_key_algo(expected->ops->name);
	const uint8_t *blob = NULL;
	size_t length = 0;
	if (expected->key == NULL || expected->key->pubkey(&blob, &length, expected->key->ctx) != 0
	    || blob == NULL || length == 0 || length > sizeof(expected->public))
		return false;
	memcpy(expected->public, blob, length);
	expected->public_len = length;
	int64_t count = expected->ops->text(expected->text, sizeof(expected->text));
	if (count < 1 || count > 1024)
		return false;
	size_t text_len = (size_t)count;
	if (expected->text[text_len - 1] != 0)
		return false;
	expected->text_len = text_len;
	return true;
}

static bool
public_file(struct reader *reader)
{
	FILE *stream = fopen(reader->public_path, "rb");
	if (stream == NULL)
		return false;
	char bytes[1025];
	size_t length = fread(bytes, 1, sizeof(bytes), stream);
	struct expected_key *expected = reader->expected;
	bool valid = !ferror(stream) && length == expected->text_len
	    && memcmp(bytes, expected->text, expected->text_len - 1) == 0
	    && bytes[length - 1] == '\n';
	if (fclose(stream) != 0)
		valid = false;
	return valid;
}

static bool
reload(struct reader *reader, bool encrypted)
{
	struct expected_key expected = *reader->expected;
	const struct key_ops *ops = expected.ops;
	dssh_test_reset_global_config();
	if (ops->reg() != 0 || ops->load(encrypted ? reader->encrypted_path : reader->plain_path,
	    encrypted ? password : NULL, encrypted ? reader : NULL) != 0)
		return false;
	expected.key = find_key_algo(ops->name);
	const uint8_t *blob = NULL;
	size_t length = 0;
	if (expected.key == NULL || expected.key->pubkey(&blob, &length, expected.key->ctx) != 0
	    || blob == NULL || length != expected.public_len || memcmp(blob, expected.public, length) != 0)
		return false;
	struct dssh_bytes_result signature = {0};
	bool valid = sign(&expected, &signature) == 0 && verify(&expected, &signature);
	dssh_result_release(&signature.lease);
	return valid;
}

static bool
remove_file(const char *path)
{
	return path[0] == 0 || unlink(path) == 0 || errno == ENOENT;
}

static int
test_readers(void)
{
	dssh_test_reset_global_config();
	struct expected_key expected[3] = {0};
	struct reader readers[6] = {0};
	struct rendezvous rendezvous;
	atomic_init(&rendezvous.ready, 0);
	atomic_init(&rendezvous.finished, 0);
	atomic_init(&rendezvous.go, false);
	atomic_init(&rendezvous.valid, true);
	thrd_t workers[6];
	unsigned int started = 0, signed_while_paused = 0, public_files = 0, private_files = 0;
	dssh_session session = NULL;
	char directory[] = "/tmp/dssh-frozen-keys-XXXXXX";
	bool created = mkdtemp(directory) != NULL;
	bool valid = created;
	for (size_t i = 0; valid && i < 3; i++)
		valid = prepare_expected(&expected[i], i);
	valid = valid && (expected[1].public_len != expected[2].public_len
	    || memcmp(expected[1].public, expected[2].public, expected[1].public_len) != 0);
	if (valid) {
		session = dssh_session_init(true, 0);
		valid = session != NULL;
	}
	for (unsigned int i = 0; valid && i < 6; i++) {
		struct reader *reader = &readers[i];
		reader->expected = &expected[i / 2];
		reader->rendezvous = &rendezvous;
		reader->pause_once = true;
		reader->valid = true;
		int a = snprintf(reader->public_path, sizeof(reader->public_path), "%s/%u.pub", directory, i);
		int b = snprintf(reader->plain_path, sizeof(reader->plain_path), "%s/%u.plain", directory, i);
		int c = snprintf(reader->encrypted_path, sizeof(reader->encrypted_path), "%s/%u.encrypted", directory, i);
		if (a < 0 || a >= 256 || b < 0 || b >= 256 || c < 0 || c >= 256)
			valid = false;
	}
	if (valid) {
		for (; started < 6; started++) {
			if (thrd_create(&workers[started], read_and_export, &readers[started]) != thrd_success)
				break;
		}
		struct timespec start, now;
		valid = timespec_get(&start, TIME_UTC) == TIME_UTC && started == 6;
		while (valid && atomic_load(&rendezvous.ready) != 6) {
			valid = timespec_get(&now, TIME_UTC) == TIME_UTC
			    && difftime(now.tv_sec, start.tv_sec) <= 15 && atomic_load(&rendezvous.finished) != started;
			thrd_yield();
		}
		for (size_t i = 0; valid && i < 3; i++) {
			struct dssh_bytes_result signature = {0};
			bool one = sign(&expected[i], &signature) == 0 && verify(&expected[i], &signature);
			dssh_result_release(&signature.lease);
			signed_while_paused += one ? 1U : 0U;
			valid = one && valid;
		}
	}
	atomic_store(&rendezvous.go, true);
	for (unsigned int i = 0; i < started; i++) {
		if (thrd_join(workers[i], NULL) != thrd_success)
			abort();
		valid = readers[i].valid && valid;
	}
	valid = valid && started == 6 && signed_while_paused == 3 && atomic_load(&rendezvous.ready) == 6
	    && atomic_load(&rendezvous.valid);
	dssh_session_cleanup(session);
	/* Every prior user has joined and its session/results are retired before
	 * resetting the test-only registry for independent output import checks. */
	for (unsigned int i = 0; i < started; i++) {
		struct reader *reader = &readers[i];
		bool pub = public_file(reader);
		bool plain = reload(reader, false);
		bool encrypted = reload(reader, true);
		public_files += pub ? 1U : 0U;
		private_files += (plain ? 1U : 0U) + (encrypted ? 1U : 0U);
		valid = pub && plain && encrypted && reader->valid && reader->writes >= 1 && reader->reads >= 1 && valid;
	}
	dssh_test_reset_global_config();
	for (size_t i = 0; i < 6; i++) {
		valid = remove_file(readers[i].public_path) && valid;
		valid = remove_file(readers[i].plain_path) && valid;
		valid = remove_file(readers[i].encrypted_path) && valid;
	}
	if (created && rmdir(directory) != 0)
		valid = false;
	printf("frozen key readers: families=3 workers=%u paused-signatures=%u public-files=%u private-files=%u valid=%u\n",
	    started, signed_while_paused, public_files, private_files, valid ? 1U : 0U);
	ASSERT_TRUE(valid && public_files == 6 && private_files == 12);
	return TEST_PASS;
}

static struct dssh_test_entry tests[] = {{"readers", test_readers}};
DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
