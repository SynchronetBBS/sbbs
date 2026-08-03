#include "xp_key_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dirwrap.h"
#include "xp_ca_file.h"

#define REFERENCE_PREFIX "xptls-keyref-v1:"
#define PROVIDER_LOCATOR_MAX 2048

static bool
valid_spec(const struct xp_key_spec *spec)
{
	if (spec == NULL)
		return false;
	if (spec->algorithm == XP_KEY_ED25519)
		return spec->bits == 0 && spec->curve == XP_KEY_CURVE_NONE;
	if (spec->algorithm == XP_KEY_RSA)
		return (spec->bits == 2048 || spec->bits == 3072 || spec->bits == 4096)
			&& spec->curve == XP_KEY_CURVE_NONE;
	if (spec->algorithm == XP_KEY_ECDSA)
		return spec->bits == 0 && (spec->curve == XP_KEY_CURVE_P256
			|| spec->curve == XP_KEY_CURVE_P384 || spec->curve == XP_KEY_CURVE_P521);
	return false;
}

static bool
store_locator_contains_authorization(const struct xp_key_store_config *store)
{
	return store != NULL && store->store_uri != NULL
		&& (strstr(store->store_uri, "pin-value=") != NULL
		    || strstr(store->store_uri, "pin-source=") != NULL);
}

static const char *
kind_name(enum xp_key_store_kind kind)
{
	switch (kind) {
		case XP_KEY_STORE_FILE: return "file";
		case XP_KEY_STORE_PKCS11: return "pkcs11";
		case XP_KEY_STORE_TPM2: return "tpm2";
		case XP_KEY_STORE_PLATFORM: return "platform";
		default: return NULL;
	}
}

static enum xp_key_store_kind
kind_from_name(const char *name, size_t len)
{
	if (len == 4 && memcmp(name, "file", 4) == 0)
		return XP_KEY_STORE_FILE;
	if (len == 6 && memcmp(name, "pkcs11", 6) == 0)
		return XP_KEY_STORE_PKCS11;
	if (len == 4 && memcmp(name, "tpm2", 4) == 0)
		return XP_KEY_STORE_TPM2;
	if (len == 8 && memcmp(name, "platform", 8) == 0)
		return XP_KEY_STORE_PLATFORM;
	return XP_KEY_STORE_MEMORY;
}

static const char base64url[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static size_t
encoded_size(size_t len)
{
	return (len / 3) * 4 + (len % 3 == 0 ? 0 : len % 3 + 1);
}

static void
encode_locator(const unsigned char *in, size_t len, char *out)
{
	size_t i = 0, o = 0;
	while (len - i >= 3) {
		unsigned value = ((unsigned)in[i] << 16) | ((unsigned)in[i + 1] << 8)
			| in[i + 2];
		out[o++] = base64url[(value >> 18) & 63];
		out[o++] = base64url[(value >> 12) & 63];
		out[o++] = base64url[(value >> 6) & 63];
		out[o++] = base64url[value & 63];
		i += 3;
	}
	if (len - i == 1) {
		out[o++] = base64url[in[i] >> 2];
		out[o] = base64url[(in[i] & 3) << 4];
	}
	else if (len - i == 2) {
		unsigned value = ((unsigned)in[i] << 8) | in[i + 1];
		out[o++] = base64url[(value >> 10) & 63];
		out[o++] = base64url[(value >> 4) & 63];
		out[o] = base64url[(value & 15) << 2];
	}
}

static int
decode_character(unsigned char value)
{
	if (value >= 'A' && value <= 'Z') return value - 'A';
	if (value >= 'a' && value <= 'z') return value - 'a' + 26;
	if (value >= '0' && value <= '9') return value - '0' + 52;
	if (value == '-') return 62;
	if (value == '_') return 63;
	return -1;
}

static int
decode_locator(const char *in, size_t len, unsigned char **out, size_t *out_len)
{
	if (len == 0 || len % 4 == 1)
		return XP_CRYPTO_ERR_FORMAT;
	size_t required = (len / 4) * 3 + (len % 4 == 0 ? 0 : len % 4 - 1);
	unsigned char *value = malloc(required + 1);
	if (value == NULL)
		return XP_CRYPTO_ERR;
	size_t i = 0, o = 0;
	while (len - i >= 4) {
		int a = decode_character((unsigned char)in[i]);
		int b = decode_character((unsigned char)in[i + 1]);
		int c = decode_character((unsigned char)in[i + 2]);
		int d = decode_character((unsigned char)in[i + 3]);
		if (a < 0 || b < 0 || c < 0 || d < 0) {
			free(value);
			return XP_CRYPTO_ERR_FORMAT;
		}
		value[o++] = (unsigned char)((a << 2) | (b >> 4));
		value[o++] = (unsigned char)((b << 4) | (c >> 2));
		value[o++] = (unsigned char)((c << 6) | d);
		i += 4;
	}
	if (len - i >= 2) {
		int a = decode_character((unsigned char)in[i]);
		int b = decode_character((unsigned char)in[i + 1]);
		if (a < 0 || b < 0) {
			free(value);
			return XP_CRYPTO_ERR_FORMAT;
		}
		value[o++] = (unsigned char)((a << 2) | (b >> 4));
		if (len - i == 3) {
			int c = decode_character((unsigned char)in[i + 2]);
			if (c < 0) {
				free(value);
				return XP_CRYPTO_ERR_FORMAT;
			}
			value[o++] = (unsigned char)((b << 4) | (c >> 2));
		}
	}
	value[required] = 0;
	*out = value;
	*out_len = required;
	return XP_CRYPTO_OK;
}

static int
attach_reference(xp_key_t key, enum xp_key_store_kind kind,
	const void *locator, size_t locator_len)
{
	const char *name = kind_name(kind);
	if (key == NULL || name == NULL || locator == NULL || locator_len == 0)
		return XP_CRYPTO_ERR_INVALID;
	size_t prefix_len = sizeof(REFERENCE_PREFIX) - 1;
	size_t name_len = strlen(name);
	size_t length = prefix_len + name_len + 1 + encoded_size(locator_len);
	char *reference = malloc(length);
	if (reference == NULL)
		return XP_CRYPTO_ERR;
	memcpy(reference, REFERENCE_PREFIX, prefix_len);
	memcpy(reference + prefix_len, name, name_len);
	reference[prefix_len + name_len] = ':';
	encode_locator(locator, locator_len, reference + prefix_len + name_len + 1);
	int status = xp_key_set_reference(key, reference, length);
	free(reference);
	return status;
}

static int
parse_reference(const void *reference, size_t reference_len,
	enum xp_key_store_kind *kind, unsigned char **locator, size_t *locator_len)
{
	const size_t prefix_len = sizeof(REFERENCE_PREFIX) - 1;
	if (reference == NULL || kind == NULL || locator == NULL || locator_len == NULL
	    || reference_len <= prefix_len
	    || memcmp(reference, REFERENCE_PREFIX, prefix_len) != 0
	    || memchr(reference, '\0', reference_len) != NULL)
		return XP_CRYPTO_ERR_FORMAT;
	const char *begin = (const char *)reference + prefix_len;
	const char *colon = memchr(begin, ':', reference_len - prefix_len);
	if (colon == NULL)
		return XP_CRYPTO_ERR_FORMAT;
	*kind = kind_from_name(begin, (size_t)(colon - begin));
	if (*kind == XP_KEY_STORE_MEMORY)
		return XP_CRYPTO_ERR_FORMAT;
	size_t encoded_len = reference_len - (size_t)(colon + 1 - (const char *)reference);
	return decode_locator(colon + 1, encoded_len, locator, locator_len);
}

static const char *
file_path(const struct xp_key_store_config *store)
{
	if (store == NULL)
		return NULL;
	if (store->file_path != NULL && store->file_path[0] != 0)
		return store->file_path;
	/* Accept the original development API while callers migrate. */
	if (store->store != NULL && strcmp(store->store, "file") == 0)
		return store->store_uri;
	return NULL;
}

static int
canonical_file_path(const char *path, char *out, size_t out_size)
{
	if (path == NULL || out == NULL || out_size == 0
	    || FULLPATH(out, path, out_size) == NULL || out[0] == 0)
		return XP_CRYPTO_ERR_INVALID;
	return XP_CRYPTO_OK;
}

static void
file_capabilities(const struct xp_key_store_config *store,
	struct xp_key_store_capabilities *capabilities)
{
	memset(capabilities, 0, sizeof(*capabilities));
	capabilities->kind = XP_KEY_STORE_FILE;
	if (strcmp(xp_crypto_provider_name(), "none") == 0)
		capabilities->availability_status = XP_CRYPTO_ERR_DISABLED;
	else if (file_path(store) == NULL)
		capabilities->availability_status = XP_CRYPTO_ERR_INVALID;
	else {
		capabilities->available = true;
		capabilities->availability_status = XP_CRYPTO_OK;
		capabilities->operations = XP_KEY_STORE_CAN_GENERATE
			| XP_KEY_STORE_CAN_IMPORT | XP_KEY_STORE_CAN_OPEN
			| XP_KEY_STORE_CAN_SIGN | XP_KEY_STORE_CAN_DESTROY
			| XP_KEY_STORE_CAN_EXPORT | XP_KEY_STORE_TRANSACTIONAL_IMPORT
			| XP_KEY_STORE_TRANSACTIONAL_DESTROY;
	}
}

static bool
explicit_file(const struct xp_key_store_config *store)
{
	return store != NULL && store->policy == XP_KEY_STORAGE_FILE;
}

static bool
explicit_hardware(const struct xp_key_store_config *store)
{
	return store != NULL && (store->policy == XP_KEY_STORAGE_HARDWARE
		|| store->policy == XP_KEY_STORAGE_NAMED);
}

static int
select_store(const struct xp_key_store_config *store,
	const struct xp_key_spec *spec, unsigned required_operation,
	struct xp_key_store_capabilities *selected)
{
	if (store == NULL || !valid_spec(spec))
		return XP_CRYPTO_ERR_INVALID;
	if (store_locator_contains_authorization(store))
		return XP_CRYPTO_ERR_INVALID;
	if (explicit_file(store)) {
		file_capabilities(store, selected);
		return selected->available ? XP_CRYPTO_OK : selected->availability_status;
	}
	if (store->store != NULL && strcmp(store->store, "file") == 0)
		return XP_CRYPTO_ERR_POLICY;
	if (store->store != NULL && store->store[0] != 0) {
		int status = xp_key_provider_store_query(store, spec, selected);
		if (status != XP_CRYPTO_OK)
			return status;
		if (selected->available
		    && (required_operation == 0
		        || (selected->operations & required_operation) != 0))
			return XP_CRYPTO_OK;
		if (explicit_hardware(store))
			return selected->availability_status == XP_CRYPTO_OK
				? XP_CRYPTO_ERR_UNSUPPORTED : selected->availability_status;
	}
	else if (explicit_hardware(store))
		return XP_CRYPTO_ERR_INVALID;
	if (store->policy != XP_KEY_STORAGE_AUTO)
		return XP_CRYPTO_ERR_POLICY;
	file_capabilities(store, selected);
	return selected->available ? XP_CRYPTO_OK : selected->availability_status;
}

static int
write_private_key(const char *path, xp_key_t key,
	xp_crypto_secret_callback_t password, void *password_context)
{
	if (path == NULL || password == NULL)
		return XP_CRYPTO_ERR_AUTHORIZATION;
	size_t encoded_len = 0;
	int status = xp_key_export_private_pem(
		key, password, password_context, NULL, &encoded_len);
	if (status != XP_CRYPTO_OK)
		return status;
	unsigned char *encoded = malloc(encoded_len);
	if (encoded == NULL)
		return XP_CRYPTO_ERR;
	size_t actual = encoded_len;
	status = xp_key_export_private_pem(
		key, password, password_context, encoded, &actual);
	if (status == XP_CRYPTO_OK) {
		char temporary[4096];
		FILE *file = xp_ca_open_private_temporary(path, temporary, sizeof(temporary));
		if (file == NULL || fwrite(encoded, 1, actual, file) != actual) {
			if (file != NULL)
				xp_ca_discard_private_temporary(file, temporary);
			status = XP_CRYPTO_ERR_IO;
		}
		else
			status = xp_ca_commit_private_temporary(file, temporary, path) == 0
				? XP_CRYPTO_OK : XP_CRYPTO_ERR_IO;
	}
	xp_ca_scrub_memory(encoded, encoded_len);
	free(encoded);
	return status;
}

static int
read_private_key(xp_key_t *out, const char *path,
	xp_crypto_secret_callback_t password, void *password_context)
{
	if (out == NULL || path == NULL)
		return XP_CRYPTO_ERR_INVALID;
	*out = NULL;
	FILE *file = fopen(path, "rb");
	if (file == NULL)
		return XP_CRYPTO_ERR_NOT_FOUND;
	int status = XP_CRYPTO_ERR_IO;
	if (fseek(file, 0, SEEK_END) != 0)
		goto done;
	long end = ftell(file);
	if (end <= 0 || fseek(file, 0, SEEK_SET) != 0)
		goto done;
	unsigned char *encoded = malloc((size_t)end);
	if (encoded == NULL) {
		status = XP_CRYPTO_ERR;
		goto done;
	}
	if (fread(encoded, 1, (size_t)end, file) == (size_t)end)
		status = xp_key_import_private_pem(
			out, encoded, (size_t)end, password, password_context);
	xp_ca_scrub_memory(encoded, (size_t)end);
	free(encoded);
done:
	fclose(file);
	return status;
}

int
xp_key_store_query(const struct xp_key_store_config *store,
	const struct xp_key_spec *spec,
	struct xp_key_store_capabilities *capabilities)
{
	if (store == NULL || !valid_spec(spec) || capabilities == NULL)
		return XP_CRYPTO_ERR_INVALID;
	return select_store(store, spec, 0, capabilities);
}

int
xp_key_generate_stored(xp_key_t *out, const struct xp_key_store_config *store,
	const struct xp_key_spec *spec)
{
	if (out == NULL)
		return XP_CRYPTO_ERR_INVALID;
	*out = NULL;
	struct xp_key_store_capabilities selected;
	int status = select_store(store, spec, XP_KEY_STORE_CAN_GENERATE, &selected);
	if (status != XP_CRYPTO_OK)
		return status;
	if (selected.kind == XP_KEY_STORE_FILE) {
		const char *path = file_path(store);
		char locator[MAX_PATH + 1];
		status = canonical_file_path(path, locator, sizeof(locator));
		if (status == XP_CRYPTO_OK)
			status = xp_key_generate(out, spec);
		if (status == XP_CRYPTO_OK)
			status = write_private_key(path, *out,
				store->authorize, store->authorize_context);
		if (status == XP_CRYPTO_OK) {
			xp_key_set_storage_metadata(*out, XP_KEY_STORE_FILE, true);
			status = attach_reference(*out, XP_KEY_STORE_FILE,
				locator, strlen(locator));
		}
	}
	else {
		unsigned char locator[PROVIDER_LOCATOR_MAX];
		size_t locator_len = sizeof(locator);
		status = xp_key_provider_generate_stored(
			out, locator, &locator_len, store, spec);
		if (status == XP_CRYPTO_OK)
			status = attach_reference(*out, selected.kind, locator, locator_len);
	}
	if (status != XP_CRYPTO_OK) {
		xp_key_release(*out);
		*out = NULL;
	}
	return status;
}

int
xp_key_import_stored(xp_key_t *out, const struct xp_key_store_config *store,
	xp_key_t source)
{
	if (out == NULL || source == NULL)
		return XP_CRYPTO_ERR_INVALID;
	*out = NULL;
	struct xp_key_info info;
	int status = xp_key_get_info(source, &info);
	if (status != XP_CRYPTO_OK)
		return status;
	struct xp_key_store_capabilities selected;
	status = select_store(store, &info.spec, XP_KEY_STORE_CAN_IMPORT, &selected);
	if (status != XP_CRYPTO_OK)
		return status;
	if (selected.kind == XP_KEY_STORE_FILE) {
		const char *path = file_path(store);
		char locator[MAX_PATH + 1];
		status = canonical_file_path(path, locator, sizeof(locator));
		if (status == XP_CRYPTO_OK)
			status = write_private_key(path, source,
				store->authorize, store->authorize_context);
		if (status == XP_CRYPTO_OK)
			status = read_private_key(out, path,
				store->authorize, store->authorize_context);
		if (status == XP_CRYPTO_OK) {
			xp_key_set_storage_metadata(*out, XP_KEY_STORE_FILE, true);
			status = attach_reference(*out, XP_KEY_STORE_FILE,
				locator, strlen(locator));
		}
	}
	else {
		unsigned char locator[PROVIDER_LOCATOR_MAX];
		size_t locator_len = sizeof(locator);
		status = xp_key_provider_import_stored(
			out, locator, &locator_len, store, source);
		if (status == XP_CRYPTO_OK)
			status = attach_reference(*out, selected.kind, locator, locator_len);
	}
	if (status != XP_CRYPTO_OK) {
		xp_key_release(*out);
		*out = NULL;
	}
	return status;
}

int
xp_key_open_stored(xp_key_t *out, const struct xp_key_store_config *store,
	const void *reference, size_t reference_len)
{
	if (out == NULL || store == NULL)
		return XP_CRYPTO_ERR_INVALID;
	if (store_locator_contains_authorization(store))
		return XP_CRYPTO_ERR_INVALID;
	*out = NULL;
	enum xp_key_store_kind kind;
	unsigned char *locator = NULL;
	size_t locator_len = 0;
	int status = parse_reference(reference, reference_len,
		&kind, &locator, &locator_len);
	if (status != XP_CRYPTO_OK)
		return status;
	if (kind == XP_KEY_STORE_FILE) {
		const char *configured = file_path(store);
		char configured_path[MAX_PATH + 1];
		if (memchr(locator, '\0', locator_len) != NULL)
			status = XP_CRYPTO_ERR_FORMAT;
		else if (configured != NULL) {
			status = canonical_file_path(configured, configured_path,
				sizeof(configured_path));
			if (status == XP_CRYPTO_OK
			    && (strlen(configured_path) != locator_len
			        || memcmp(configured_path, locator, locator_len) != 0))
				status = XP_CRYPTO_ERR_CONFLICT;
			if (status == XP_CRYPTO_OK)
				status = read_private_key(out, (const char *)locator,
					store->authorize, store->authorize_context);
		}
		else
			status = read_private_key(out, (const char *)locator,
				store->authorize, store->authorize_context);
		if (status == XP_CRYPTO_OK)
			xp_key_set_storage_metadata(*out, XP_KEY_STORE_FILE, true);
	}
	else
		status = xp_key_provider_open_stored(
			out, store, locator, locator_len);
	if (status == XP_CRYPTO_OK)
		status = xp_key_set_reference(*out, reference, reference_len);
	if (status != XP_CRYPTO_OK) {
		xp_key_release(*out);
		*out = NULL;
	}
	free(locator);
	return status;
}

int
xp_key_destroy_stored(const struct xp_key_store_config *store,
	const void *reference, size_t reference_len,
	const void *expected_fingerprint, size_t fingerprint_len)
{
	if (store == NULL || expected_fingerprint == NULL || fingerprint_len != 32)
		return XP_CRYPTO_ERR_INVALID;
	if (store_locator_contains_authorization(store))
		return XP_CRYPTO_ERR_INVALID;
	enum xp_key_store_kind kind;
	unsigned char *locator = NULL;
	size_t locator_len = 0;
	int status = parse_reference(reference, reference_len,
		&kind, &locator, &locator_len);
	if (status != XP_CRYPTO_OK)
		return status;
	if (kind == XP_KEY_STORE_FILE) {
		xp_key_t key = NULL;
		status = xp_key_open_stored(&key, store, reference, reference_len);
		unsigned char actual[32];
		size_t actual_len = sizeof(actual);
		if (status == XP_CRYPTO_OK)
			status = xp_key_fingerprint_sha256(key, actual, &actual_len);
		if (status == XP_CRYPTO_OK
		    && memcmp(actual, expected_fingerprint, sizeof(actual)) != 0)
			status = XP_CRYPTO_ERR_CONFLICT;
		if (status == XP_CRYPTO_OK && remove((const char *)locator) != 0)
			status = XP_CRYPTO_ERR_IO;
		xp_key_release(key);
		xp_ca_scrub_memory(actual, sizeof(actual));
	}
	else
		status = xp_key_provider_destroy_stored(store, locator, locator_len,
			expected_fingerprint, fingerprint_len);
	free(locator);
	return status;
}
