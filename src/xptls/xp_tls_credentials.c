#include "xp_tls_internal.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "xp_ca.h"

struct xp_tls_server_credentials {
	atomic_size_t references;
	unsigned char *chain_pem;
	size_t chain_pem_len;
	xp_key_t key;
};

static int
read_file(const char *path, unsigned char **out, size_t *len)
{
	*out = NULL; *len = 0;
	FILE *file = fopen(path, "rb");
	if (file == NULL) return XP_CRYPTO_ERR_IO;
	if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return XP_CRYPTO_ERR_IO; }
	long size = ftell(file);
	if (size <= 0 || fseek(file, 0, SEEK_SET) != 0) { fclose(file); return XP_CRYPTO_ERR_FORMAT; }
	unsigned char *data = malloc((size_t)size);
	if (data == NULL) { fclose(file); return XP_CRYPTO_ERR; }
	if (fread(data, 1, (size_t)size, file) != (size_t)size) {
		free(data); fclose(file); return XP_CRYPTO_ERR_IO;
	}
	fclose(file); *out = data; *len = (size_t)size; return XP_CRYPTO_OK;
}

int
xp_tls_server_credentials_load(xp_tls_server_credentials_t *out,
	const struct xp_tls_server_credentials_config *config)
{
	if (out == NULL) return XP_CRYPTO_ERR_INVALID;
	*out = NULL;
	if (config == NULL || config->certificate_chain_file == NULL
	    || config->private_key == NULL) return XP_CRYPTO_ERR_INVALID;
	struct xp_key_info key_info;
	int status = xp_key_get_info(config->private_key, &key_info);
	if (status != XP_CRYPTO_OK)
		return status;
	if (!key_info.has_private)
		return XP_CRYPTO_ERR_POLICY;
	unsigned char *pem = NULL; size_t pem_len = 0;
	status = read_file(config->certificate_chain_file, &pem, &pem_len);
	xp_ca_cert_t *chain = NULL; size_t count = 0;
	if (status == XP_CRYPTO_OK)
		status = xp_ca_cert_chain_import_pem(&chain, &count, pem, pem_len);
	if (status != XP_CRYPTO_OK || count == 0) {
		free(pem);
		xp_ca_cert_chain_free(chain, count);
		return status == XP_CRYPTO_OK ? XP_CRYPTO_ERR_FORMAT : status;
	}
	time_t from = 0, to = 0, now = time(NULL);
	if (xp_ca_cert_get_validity(chain[0], &from, &to) != XP_CRYPTO_OK
	    || now < from || now > to) status = XP_CRYPTO_ERR_POLICY;
	if (status == XP_CRYPTO_OK)
		status = xp_ca_cert_tls_server_usable(chain[0]);
	if (status == XP_CRYPTO_OK && count > 1)
		status = xp_ca_cert_validate(chain[0],
			count > 2 ? chain + 1 : NULL, count > 2 ? count - 2 : 0,
			chain[count - 1], NULL, 0, NULL);
	xp_key_t public_key = NULL;
	unsigned char first[32], second[32]; size_t first_len = 32, second_len = 32;
	if (status == XP_CRYPTO_OK)
		status = xp_ca_cert_get_public_key(&public_key, chain[0]);
	if (status == XP_CRYPTO_OK)
		status = xp_key_fingerprint_sha256(public_key, first, &first_len);
	if (status == XP_CRYPTO_OK)
		status = xp_key_fingerprint_sha256(config->private_key, second, &second_len);
	if (status == XP_CRYPTO_OK && memcmp(first, second, 32) != 0)
		status = XP_CRYPTO_ERR_CONFLICT;
	xp_key_release(public_key);
	xp_ca_cert_chain_free(chain, count);
	if (status != XP_CRYPTO_OK) { free(pem); return status; }
	xp_tls_server_credentials_t value = calloc(1, sizeof(*value));
	if (value == NULL) { free(pem); return XP_CRYPTO_ERR; }
	atomic_init(&value->references, 1);
	value->chain_pem = pem;
	value->chain_pem_len = pem_len;
	value->key = config->private_key;
	xp_key_retain(value->key);
	*out = value;
	return XP_CRYPTO_OK;
}

void xp_tls_server_credentials_retain(xp_tls_server_credentials_t value)
{
	if (value != NULL) atomic_fetch_add_explicit(&value->references, 1, memory_order_relaxed);
}
void xp_tls_server_credentials_release(xp_tls_server_credentials_t value)
{
	if (value != NULL && atomic_fetch_sub_explicit(&value->references, 1,
	    memory_order_acq_rel) == 1) {
		xp_key_release(value->key); free(value->chain_pem); free(value);
	}
}
const void *xp_tls_credentials_chain_pem(
	xp_tls_server_credentials_t value, size_t *length)
{
	if (length != NULL)
		*length = value == NULL ? 0 : value->chain_pem_len;
	return value == NULL ? NULL : value->chain_pem;
}
xp_key_t xp_tls_credentials_key(xp_tls_server_credentials_t value)
{
	return value == NULL ? NULL : value->key;
}

xp_tls_t
xp_tls_server_open(SOCKET socket, xp_tls_server_credentials_t credentials,
	const struct xp_tls_server_config *config)
{
	size_t chain_pem_length = 0;
	const void *chain_pem = xp_tls_credentials_chain_pem(
		credentials, &chain_pem_length);
	return xp_tls_provider_server_open(socket, chain_pem, chain_pem_length,
		xp_tls_credentials_key(credentials), config);
}
