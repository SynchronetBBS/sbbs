/* Keep CRT streams inside DeuceSSH when OpenSSL is a Windows DLL. */
#ifndef DSSH_KEY_FILE_OPENSSL_H
#define DSSH_KEY_FILE_OPENSSL_H

#include <openssl/pem.h>
#include "key-file-common.h"

#if defined(_WIN32) || defined(DSSH_TEST_KEY_FILE_BIO)
struct dssh_key_file_bio {
	FILE *file;
	BIO_METHOD *method;
	BIO *bio;
	bool failed;
};

static int
dssh_key_file_bio_read(BIO *bio, char *out, size_t length, size_t *received)
{
	struct dssh_key_file_bio *stream = BIO_get_data(bio);
	*received = fread(out, 1, length, stream->file);
	if (ferror(stream->file)) {
		stream->failed = true;
		return 0;
	}
	return *received != 0;
}

static int
dssh_key_file_bio_write(BIO *bio, const char *input, size_t length, size_t *written)
{
	struct dssh_key_file_bio *stream = BIO_get_data(bio);
	*written = fwrite(input, 1, length, stream->file);
	if (*written != length || ferror(stream->file)) {
		stream->failed = true;
		return 0;
	}
	return 1;
}

static int
dssh_key_file_bio_gets(BIO *bio, char *out, int capacity)
{
	struct dssh_key_file_bio *stream = BIO_get_data(bio);
	if (capacity < 1)
		return 0;
	out[0] = 0;
	if (capacity == 1)
		return 0;
	if (fgets(out, capacity, stream->file) == NULL) {
		if (ferror(stream->file)) {
			stream->failed = true;
			return -1;
		}
		return 0;
	}
	/* fgets writes at most capacity - 1 characters before its terminator. */
	int length = (int)strlen(out);
	return length;
}

static long
dssh_key_file_bio_ctrl(BIO *bio, int command, long offset, void *unused)
{
	struct dssh_key_file_bio *stream = BIO_get_data(bio);
	(void)unused;
	switch (command) {
		case BIO_CTRL_FLUSH:
			if (fflush(stream->file) != 0) {
				stream->failed = true;
				return 0;
			}
			return 1;
		case BIO_CTRL_EOF:
			return feof(stream->file) != 0;
		case BIO_C_FILE_TELL:
		case BIO_CTRL_INFO:
			return ftell(stream->file);
		case BIO_C_FILE_SEEK:
		case BIO_CTRL_RESET:
			if (fseek(stream->file, offset, SEEK_SET) != 0) {
				stream->failed = true;
				return -1;
			}
			return 0;
		default:
			return 0;
	}
}

/* The BIO borrows both this live operation state and its FILE. Neither is
 * released by OpenSSL. A method belongs to this operation, avoiding shared
 * mutable method state and permitting retries after allocation failures. */
static inline int
dssh_key_file_bio_close(struct dssh_key_file_bio *stream)
{
	if (ferror(stream->file))
		stream->failed = true;
	if (stream->bio != NULL && BIO_free(stream->bio) != 1)
		stream->failed = true;
	stream->bio = NULL;
	BIO_meth_free(stream->method);
	stream->method = NULL;
	return stream->failed ? DSSH_ERROR_INIT : 0;
}

static inline int
dssh_key_file_bio_init(struct dssh_key_file_bio *stream, FILE *file)
{
	stream->file = file;
	stream->method = NULL;
	stream->bio = NULL;
	stream->failed = false;
	int type = BIO_get_new_index();
	if (type < 0)
		return DSSH_ERROR_INIT;
	stream->method = BIO_meth_new(type | BIO_TYPE_SOURCE_SINK, "DeuceSSH key file");
	if (stream->method == NULL)
		return DSSH_ERROR_INIT;
	if (BIO_meth_set_read_ex(stream->method, dssh_key_file_bio_read) != 1
	    || BIO_meth_set_write_ex(stream->method, dssh_key_file_bio_write) != 1
	    || BIO_meth_set_gets(stream->method, dssh_key_file_bio_gets) != 1
	    || BIO_meth_set_ctrl(stream->method, dssh_key_file_bio_ctrl) != 1)
		goto fail;
	stream->bio = BIO_new(stream->method);
	if (stream->bio == NULL)
		goto fail;
	BIO_set_data(stream->bio, stream);
	BIO_set_init(stream->bio, 1);
	return 0;
fail:
	stream->failed = true;
	return dssh_key_file_bio_close(stream);
}
#endif

static inline EVP_PKEY *
dssh_key_file_read_private(FILE *file, struct dssh_key_file_password_state *password)
{
#if defined(_WIN32) || defined(DSSH_TEST_KEY_FILE_BIO)
	struct dssh_key_file_bio stream;
	if (dssh_key_file_bio_init(&stream, file) < 0)
		return NULL;
	EVP_PKEY *key = PEM_read_bio_PrivateKey(stream.bio, NULL, dssh_key_file_password, password);
	if (dssh_key_file_bio_close(&stream) < 0) {
		EVP_PKEY_free(key);
		return NULL;
	}
	return key;
#else
	return PEM_read_PrivateKey(file, NULL, dssh_key_file_password, password);
#endif
}

static inline int
dssh_key_file_write_private(FILE *file, const EVP_PKEY *key, const EVP_CIPHER *cipher,
    struct dssh_key_file_password_state *password)
{
#if defined(_WIN32) || defined(DSSH_TEST_KEY_FILE_BIO)
	struct dssh_key_file_bio stream;
	if (dssh_key_file_bio_init(&stream, file) < 0)
		return 0;
	int ret = PEM_write_bio_PrivateKey(stream.bio, key, cipher, NULL, 0, dssh_key_file_password, password);
	if (dssh_key_file_bio_close(&stream) < 0)
		return 0;
	return ret;
#else
	return PEM_write_PrivateKey(file, key, cipher, NULL, 0, dssh_key_file_password, password);
#endif
}

#endif
