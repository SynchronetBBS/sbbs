/* Exercise the Windows CRT boundary on every OpenSSL build. */
#include <openssl/bio.h>
#include <openssl/pem.h>
#include "dssh_test.h"

static int setup_failure = -1, setup_calls;
static unsigned int live_methods, live_bios;
static bool close_failure;
enum io_failure {
	IO_OK, IO_READ, IO_GETS, IO_WRITE_ZERO, IO_WRITE_PARTIAL, IO_FLUSH, IO_SEEK, IO_TELL
};
static enum io_failure io_failure;

static bool
setup_fails(void)
{
	return setup_calls++ == setup_failure;
}

static int
test_bio_index(void)
{
	return setup_fails() ? -1 : BIO_get_new_index();
}

static BIO_METHOD *
test_bio_method(int type, const char *name)
{
	if (setup_fails())
		return NULL;
	BIO_METHOD *method = BIO_meth_new(type, name);
	if (method != NULL)
		live_methods++;
	return method;
}

static void
test_bio_method_free(BIO_METHOD *method)
{
	if (method != NULL)
		live_methods--;
	BIO_meth_free(method);
}

static BIO *
test_bio_new(const BIO_METHOD *method)
{
	if (setup_fails())
		return NULL;
	BIO *bio = BIO_new(method);
	if (bio != NULL)
		live_bios++;
	return bio;
}

static int
test_bio_free(BIO *bio)
{
	int ret = BIO_free(bio);
	live_bios--;
	return close_failure ? 0 : ret;
}

static size_t
test_file_read(void *out, size_t size, size_t count, FILE *file)
{
	return io_failure == IO_READ ? 0 : fread(out, size, count, file);
}

static char *
test_file_gets(char *out, int size, FILE *file)
{
	return io_failure == IO_GETS ? NULL : fgets(out, size, file);
}

static size_t
test_file_write(const void *input, size_t size, size_t count, FILE *file)
{
	if (io_failure == IO_WRITE_ZERO)
		return 0;
	if (io_failure == IO_WRITE_PARTIAL && count != 0)
		count--;
	return fwrite(input, size, count, file);
}

static int
test_file_error(FILE *file)
{
	return io_failure == IO_READ || io_failure == IO_GETS ? 1 : ferror(file);
}

static int
test_file_flush(FILE *file)
{
	int ret = fflush(file);
	return io_failure == IO_FLUSH ? EOF : ret;
}

static int
test_file_seek(FILE *file, long offset, int origin)
{
	return io_failure == IO_SEEK ? -1 : fseek(file, offset, origin);
}

static long
test_file_tell(FILE *file)
{
	return io_failure == IO_TELL ? -1 : ftell(file);
}

#ifndef DSSH_TEST_KEY_FILE_BIO
#define DSSH_TEST_KEY_FILE_BIO
#endif
#define BIO_get_new_index test_bio_index
#define BIO_meth_new test_bio_method
#define BIO_meth_free test_bio_method_free
#define BIO_new test_bio_new
#define BIO_free test_bio_free
#define BIO_meth_set_read_ex(m, f) (setup_fails() ? 0 : BIO_meth_set_read_ex(m, f))
#define BIO_meth_set_write_ex(m, f) (setup_fails() ? 0 : BIO_meth_set_write_ex(m, f))
#define BIO_meth_set_gets(m, f) (setup_fails() ? 0 : BIO_meth_set_gets(m, f))
#define BIO_meth_set_ctrl(m, f) (setup_fails() ? 0 : BIO_meth_set_ctrl(m, f))
#undef fread
#undef fwrite
#undef fgets
#undef ferror
#undef fflush
#undef fseek
#undef ftell
#define fread test_file_read
#define fwrite test_file_write
#define fgets test_file_gets
#define ferror test_file_error
#define fflush test_file_flush
#define fseek test_file_seek
#define ftell test_file_tell
#include "../key_algo/key-file-openssl.h"
#undef ftell
#undef fseek
#undef fflush
#undef ferror
#undef fgets
#undef fwrite
#undef fread
#undef BIO_meth_set_ctrl
#undef BIO_meth_set_gets
#undef BIO_meth_set_write_ex
#undef BIO_meth_set_read_ex
#undef BIO_free
#undef BIO_new
#undef BIO_meth_free
#undef BIO_meth_new
#undef BIO_get_new_index

static const char file_path[] = "dssh-key-file-bio.tmp";

static bool
close_file(FILE *file)
{
	bool valid = fputc('x', file) != EOF;
	if (fclose(file) != 0 || remove(file_path) != 0)
		valid = false;
	return valid && live_methods == 0 && live_bios == 0;
}

static int
test_setup(void)
{
	FILE *file = fopen(file_path, "w+b");
	ASSERT_TRUE(file != NULL);
	bool valid = true;
	for (int stage = 0; stage < 7; stage++) {
		struct dssh_key_file_bio stream;
		setup_failure = stage;
		setup_calls = 0;
		valid = dssh_key_file_bio_init(&stream, file) == DSSH_ERROR_INIT && valid;
		valid = valid && setup_calls == stage + 1 && stream.method == NULL && stream.bio == NULL
		    && live_methods == 0 && live_bios == 0;
		setup_failure = -1;
		setup_calls = 0;
		int ret = dssh_key_file_bio_init(&stream, file);
		valid = valid && ret == 0 && setup_calls == 7;
		if (ret == 0)
			valid = dssh_key_file_bio_close(&stream) == 0 && valid;
	}
	valid = close_file(file) && valid;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_io(void)
{
	FILE *file = fopen(file_path, "w+b");
	ASSERT_TRUE(file != NULL);
	struct dssh_key_file_bio stream;
	int ret = dssh_key_file_bio_init(&stream, file);
	bool valid = ret == 0;
	if (ret == 0) {
		const char text[] = "line\n";
		char out[16];
		size_t count = 0;
		valid = BIO_write_ex(stream.bio, text, sizeof(text) - 1, &count) == 1 && count == 5
		    && BIO_flush(stream.bio) == 1 && BIO_tell(stream.bio) == 5
		    && BIO_seek(stream.bio, 0) == 0 && BIO_gets(stream.bio, out, sizeof(out)) == 5
		    && memcmp(out, text, sizeof(text)) == 0 && BIO_eof(stream.bio) == 0;
		valid = valid && dssh_key_file_bio_gets(stream.bio, out, 0) == 0
		    && dssh_key_file_bio_gets(stream.bio, out, 1) == 0 && out[0] == 0
		    && BIO_gets(stream.bio, out, sizeof(out)) == 0 && BIO_eof(stream.bio) == 1
		    && BIO_reset(stream.bio) == 0
		    && BIO_read_ex(stream.bio, out, sizeof(out), &count) == 1 && count == 5
		    && memcmp(out, text, 5) == 0
		    && BIO_read_ex(stream.bio, out, sizeof(out), &count) == 0 && count == 0
		    && BIO_ctrl(stream.bio, BIO_CTRL_PENDING, 0, NULL) == 0;
		valid = dssh_key_file_bio_close(&stream) == 0 && valid;
	}
	valid = close_file(file) && valid;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_io_failures(void)
{
	bool valid = true;
	for (enum io_failure fault = IO_READ; fault <= IO_TELL; fault++) {
		FILE *file = fopen(file_path, "w+b");
		ASSERT_TRUE(file != NULL);
		struct dssh_key_file_bio stream;
		int ret = dssh_key_file_bio_init(&stream, file);
		valid = ret == 0 && valid;
		if (ret == 0) {
			char out[8];
			size_t count = 0;
			io_failure = fault;
			if (fault == IO_READ)
				valid = BIO_read_ex(stream.bio, out, sizeof(out), &count) == 0 && count == 0 && valid;
			else if (fault == IO_GETS)
				valid = BIO_gets(stream.bio, out, sizeof(out)) < 0 && valid;
			else if (fault == IO_WRITE_ZERO || fault == IO_WRITE_PARTIAL)
				valid = BIO_write_ex(stream.bio, "data", 4, &count) == 0 && valid;
			else if (fault == IO_FLUSH)
				valid = BIO_flush(stream.bio) == 0 && valid;
			else if (fault == IO_SEEK)
				valid = BIO_seek(stream.bio, 0) == -1 && valid;
			else
				valid = BIO_tell(stream.bio) == -1 && valid;
			io_failure = IO_OK;
			int expected = fault == IO_TELL ? 0 : DSSH_ERROR_INIT;
			valid = dssh_key_file_bio_close(&stream) == expected && valid;
		}
		valid = close_file(file) && valid;
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
password(char *out, int capacity, int writing, void *data)
{
	(void)writing;
	(void)data;
	if (capacity < 8)
		return -1;
	memcpy(out, "bio-test", 8);
	return 8;
}

static int
test_pem(void)
{
	bool valid = true;
	size_t bits = 2048;
	EVP_PKEY *keys[] = {EVP_PKEY_Q_keygen(NULL, NULL, "ED25519"), EVP_PKEY_Q_keygen(NULL, NULL, "RSA", bits)};
	EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-256-CBC", NULL);
	for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
		if (keys[k] == NULL || cipher == NULL) {
			valid = false;
			continue;
		}
		for (unsigned encrypted = 0; encrypted < 2; encrypted++) {
			FILE *file = fopen(file_path, "w+b");
			if (file == NULL) {
				valid = false;
				continue;
			}
			struct dssh_key_file_password_state state = {password, NULL, 0};
			valid = dssh_key_file_write_private(file, keys[k], encrypted ? cipher : NULL, &state) == 1
			    && state.status == 0 && valid;
			valid = fseek(file, 0, SEEK_SET) == 0 && valid;
			EVP_PKEY *loaded = dssh_key_file_read_private(file, &state);
			valid = valid && loaded != NULL && EVP_PKEY_eq(keys[k], loaded) == 1 && state.status == 0;
			EVP_PKEY_free(loaded);
			if (encrypted) {
				valid = fseek(file, 0, SEEK_SET) == 0 && valid;
				state.callback = NULL;
				loaded = dssh_key_file_read_private(file, &state);
				valid = loaded == NULL && state.status == DSSH_ERROR_INIT && valid;
				EVP_PKEY_free(loaded);
				state.callback = password;
				state.status = 0;
			}
			close_failure = true;
			valid = dssh_key_file_write_private(file, keys[k], NULL, &state) == 0 && valid;
			valid = fseek(file, 0, SEEK_SET) == 0 && valid;
			loaded = dssh_key_file_read_private(file, &state);
			valid = loaded == NULL && valid;
			EVP_PKEY_free(loaded);
			close_failure = false;
			for (int stage = 0; stage < 7; stage++) {
				setup_failure = stage;
				setup_calls = 0;
				valid = dssh_key_file_write_private(file, keys[k], NULL, &state) == 0 && valid;
				setup_calls = 0;
				loaded = dssh_key_file_read_private(file, &state);
				valid = loaded == NULL && valid;
				EVP_PKEY_free(loaded);
			}
			setup_failure = -1;
			valid = close_file(file) && valid;
		}
	}
	EVP_CIPHER_free(cipher);
	for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++)
		EVP_PKEY_free(keys[k]);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static struct dssh_test_entry tests[] = {
	{"setup", test_setup},
	{"io", test_io},
	{"io_failures", test_io_failures},
	{"pem", test_pem},
};
DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
