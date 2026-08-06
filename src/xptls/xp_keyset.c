#include "xp_keyset.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filewrap.h"
#include "xp_ca_file.h"
#include "xp_cipher.h"
#include "xp_digest.h"
#include "xp_kdf.h"

#define MANIFEST_HEADER "xptls-keyset-manifest-v1\n"
#define KEYSET_LIMIT (16u * 1024u * 1024u)

struct keyset_entry {
	char *label;
	unsigned char *private_data;
	size_t private_len;
	bool private_reference;
	xp_ca_cert_t *certs;
	size_t cert_count;
};

struct xp_keyset {
	char *path;
	enum xp_keyset_mode mode;
	enum xp_keyset_format format;
	xp_crypto_secret_callback_t password;
	void *password_context;
	const struct xp_key_store_config *key_store;
	FILE *lock_file;
	struct keyset_entry *entries;
	size_t entry_count;
};

static int pkcs12_import(
	xp_key_t *key, char **label, xp_ca_cert_t **certs, size_t *cert_count,
	const void *data, size_t len, xp_crypto_secret_callback_t password,
	void *password_context);
static int pkcs12_export(
	xp_key_t key, const char *label, const xp_ca_cert_t *certs,
	size_t cert_count, xp_crypto_secret_callback_t password,
	void *password_context, void *out, size_t *len);

static char *
copy_string(const char *value) {
	size_t len = strlen(value) + 1;
	char * copy = malloc(len);
	if (copy != NULL)
		memcpy(copy, value, len);
	return copy;
}

static bool
valid_utf8_label(const char *label) {
	if (label == NULL || label[0] == 0)
		return false;
	const unsigned char *p = (const unsigned char *)label;
	while (*p != 0) {
		if (*p < 0x20 || *p == 0x7f)
			return false;
		if (*p < 0x80) {
			p++;
			continue;
		}
		unsigned      need = (*p & 0xe0) == 0xc0 ? 1 : (*p & 0xf0) == 0xe0 ? 2
		                : (*p & 0xf8) == 0xf0 ? 3 : 99;
		if (need == 99 || (need == 1 && *p < 0xc2))
			return false;
		unsigned char first = *p++;
		for (unsigned i = 0; i < need; i++)
			if ((p[i] & 0xc0) != 0x80)
				return false;
		if ((first == 0xe0 && p[0] < 0xa0) || (first == 0xed && p[0] >= 0xa0)
		    || (first == 0xf0 && p[0] < 0x90)
		    || (first == 0xf4 && p[0] >= 0x90) || first > 0xf4)
			return false;
		p += need;
	}
	return true;
}

static void
free_entry(struct keyset_entry *entry) {
	if (entry == NULL)
		return;
	free(entry->label);
	if (entry->private_data != NULL) {
		xp_ca_scrub_memory(entry->private_data, entry->private_len);
		free(entry->private_data);
	}
	xp_ca_cert_chain_free(entry->certs, entry->cert_count);
	memset(entry, 0, sizeof(*entry));
}

static struct keyset_entry *
find_entry(xp_keyset_t keyset, const char *label) {
	for (size_t i = 0; i < keyset->entry_count; i++)
		if (strcmp(keyset->entries[i].label, label) == 0)
			return &keyset->entries[i];
	return NULL;
}

static struct keyset_entry *
add_entry(xp_keyset_t keyset, const char *label) {
	struct keyset_entry *grown = realloc(keyset->entries,
	                                     (keyset->entry_count + 1) * sizeof(*grown));
	if (grown == NULL)
		return NULL;
	keyset->entries = grown;
	struct keyset_entry *entry = &grown[keyset->entry_count];
	memset(entry, 0, sizeof(*entry));
	entry->label = copy_string(label);
	if (entry->label == NULL)
		return NULL;
	keyset->entry_count++;
	return entry;
}

static int
validate_entry_identity(xp_keyset_t keyset,
                        struct keyset_entry *entry) {
	if (entry->private_data == NULL || entry->cert_count == 0)
		return XP_CRYPTO_OK;
	xp_key_t      private_key = NULL;
	xp_key_t      certificate_key = NULL;
	int           status = xp_keyset_get_private_key(&private_key, keyset, entry->label);
	if (status == XP_CRYPTO_OK)
		status = xp_ca_cert_get_public_key(&certificate_key, entry->certs[0]);
	unsigned char private_fingerprint[32], certificate_fingerprint[32];
	size_t        private_len = sizeof(private_fingerprint);
	size_t        certificate_len = sizeof(certificate_fingerprint);
	if (status == XP_CRYPTO_OK)
		status = xp_key_fingerprint_sha256(private_key, private_fingerprint,
		                                   &private_len);
	if (status == XP_CRYPTO_OK)
		status = xp_key_fingerprint_sha256(certificate_key,
		                                   certificate_fingerprint, &certificate_len);
	if (status == XP_CRYPTO_OK && (private_len != certificate_len
	                               || memcmp(private_fingerprint, certificate_fingerprint,
	                                         private_len) != 0))
		status = XP_CRYPTO_ERR_VERIFY;
	xp_ca_scrub_memory(private_fingerprint, sizeof(private_fingerprint));
	xp_ca_scrub_memory(certificate_fingerprint,
	                   sizeof(certificate_fingerprint));
	xp_key_release(certificate_key);
	xp_key_release(private_key);
	return status;
}

static const char base64url[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static size_t
encoded_size(size_t len) {
	return (len / 3) * 4 + (len % 3 == 0 ? 0 : len % 3 + 1);
}

static char *
encode64(const void *input, size_t len) {
	const unsigned char *in = input;
	char *               out = malloc(encoded_size(len) + 1);
	if (out == NULL)
		return NULL;
	size_t               i = 0, o = 0;
	while (len - i >= 3) {
		unsigned value = ((unsigned)in[i] << 16) | ((unsigned)in[i + 1] << 8) | in[i + 2];
		out[o++] = base64url[(value >> 18) & 63];
		out[o++] = base64url[(value >> 12) & 63];
		out[o++] = base64url[(value >> 6) & 63];
		out[o++] = base64url[value & 63];
		i += 3;
	}
	if (len - i == 1) {
		out[o++] = base64url[in[i] >> 2];
		out[o++] = base64url[(in[i] & 3) << 4];
	} else if (len - i == 2) {
		unsigned value = ((unsigned)in[i] << 8) | in[i + 1];
		out[o++] = base64url[(value >> 10) & 63];
		out[o++] = base64url[(value >> 4) & 63];
		out[o++] = base64url[(value & 15) << 2];
	}
	out[o] = 0;
	return out;
}

static int
decode_character(unsigned char value) {
	if (value >= 'A' && value <= 'Z')
		return value - 'A';
	if (value >= 'a' && value <= 'z')
		return value - 'a' + 26;
	if (value >= '0' && value <= '9')
		return value - '0' + 52;
	if (value == '-' || value == '+')
		return 62;
	if (value == '_' || value == '/')
		return 63;
	return -1;
}

static int
decode64(const char *input, size_t len, unsigned char **out, size_t *out_len) {
	while (len != 0 && input[len - 1] == '=') len--;
	if (len == 0 || len % 4 == 1)
		return XP_CRYPTO_ERR_FORMAT;
	size_t         required = (len / 4) * 3 + (len % 4 == 0 ? 0 : len % 4 - 1);
	unsigned char *value = malloc(required + 1);
	if (value == NULL)
		return XP_CRYPTO_ERR;
	size_t         i = 0, o = 0;
	while (len - i >= 4) {
		int a = decode_character(input[i]), b = decode_character(input[i + 1]),
		    c = decode_character(input[i + 2]), d = decode_character(input[i + 3]);
		if (a < 0 || b < 0 || c < 0 || d < 0) {
			free(value);
			return XP_CRYPTO_ERR_FORMAT;
		}
		value[o++] = (a << 2) | (b >> 4);
		value[o++] = (b << 4) | (c >> 2);
		value[o++] = (c << 6) | d;
		i += 4;
	}
	if (len - i >= 2) {
		int a = decode_character(input[i]), b = decode_character(input[i + 1]);
		if (a < 0 || b < 0) {
			free(value);
			return XP_CRYPTO_ERR_FORMAT;
		}
		value[o++] = (a << 2) | (b >> 4);
		if (len - i == 3) {
			int c = decode_character(input[i + 2]);
			if (c < 0) {
				free(value);
				return XP_CRYPTO_ERR_FORMAT;
			}
			value[o++] = (b << 4) | (c >> 2);
		}
	}
	value[required] = 0;
	*out = value;
	*out_len = required;
	return XP_CRYPTO_OK;
}

static int
read_file(const char *path, unsigned char **out, size_t *len) {
	*out = NULL;
	*len = 0;
	FILE *file = fopen(path, "rb");
	if (file == NULL)
		return XP_CRYPTO_ERR_NOT_FOUND;
	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return XP_CRYPTO_ERR_IO;
	}
	long size = ftell(file);
	if (size <= 0 || (unsigned long)size > KEYSET_LIMIT || fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return XP_CRYPTO_ERR_FORMAT;
	}
	unsigned char *data = malloc((size_t)size + 1);
	if (data == NULL) {
		fclose(file);
		return XP_CRYPTO_ERR;
	}
	if (fread(data, 1, (size_t)size, file) != (size_t)size) {
		free(data);
		fclose(file);
		return XP_CRYPTO_ERR_IO;
	}
	fclose(file);
	data[size] = 0;
	*out = data;
	*len = (size_t)size;
	return XP_CRYPTO_OK;
}

static enum xp_keyset_format
selected_format(const struct xp_keyset_config *config) {
	if (config->format != XP_KEYSET_FORMAT_AUTO)
		return config->format;
	const char *dot = strrchr(config->path, '.');
	if (dot == NULL)
		return XP_KEYSET_FORMAT_MANIFEST;
	char ext[5] = {0};
	size_t n = strlen(dot);
	if (n >= sizeof(ext))
		return XP_KEYSET_FORMAT_MANIFEST;
	for (size_t i = 0; i < n; i++) ext[i] = (char)tolower((unsigned char)dot[i]);
	return strcmp(ext, ".p12") == 0 ||
	       strcmp(ext, ".pfx") == 0 ? XP_KEYSET_FORMAT_PKCS12 : XP_KEYSET_FORMAT_MANIFEST;
}

static int
acquire_lock(xp_keyset_t keyset) {
	size_t size = strlen(keyset->path) + 6;
	char * path = malloc(size);
	if (path == NULL)
		return XP_CRYPTO_ERR;
	snprintf(path, size, "%s.lock", keyset->path);
	keyset->lock_file = fopen(path, "a+b");
	free(path);
	if (keyset->lock_file == NULL)
		return XP_CRYPTO_ERR_IO;
	if (xp_lockfile(fileno(keyset->lock_file), 0, 1, false) != 0) {
		fclose(keyset->lock_file);
		keyset->lock_file = NULL;
		return XP_CRYPTO_ERR_BUSY;
	}
	return XP_CRYPTO_OK;
}

static int
encrypted_pem_to_der(const unsigned char *pem, size_t len,
                     unsigned char **out, size_t *out_len) {
	const char begin[] = "-----BEGIN ENCRYPTED PRIVATE KEY-----";
	const char end[] = "-----END ENCRYPTED PRIVATE KEY-----";
	char *     text = malloc(len + 1);
	if (text == NULL)
		return XP_CRYPTO_ERR;
	memcpy(text, pem, len);
	text[len] = 0;
	const char *first = strstr(text, begin);
	const char *last = strstr(text, end);
	if (first == NULL || last == NULL || last <= first) {
		xp_ca_scrub_memory(text, len + 1);
		free(text);
		return XP_CRYPTO_ERR_FORMAT;
	}
	first += sizeof(begin) - 1;
	size_t cap = (size_t)(last - first);
	char * compact = malloc(cap + 1);
	if (compact == NULL) {
		xp_ca_scrub_memory(text, len + 1);
		free(text);
		return XP_CRYPTO_ERR;
	}
	size_t used = 0;
	while (first < last) {
		if (!isspace((unsigned char) *first))
			compact[used++] = *first;
		first++;
	}
	compact[used] = 0;
	int status = decode64(compact, used, out, out_len);
	xp_ca_scrub_memory(compact, cap + 1);
	free(compact);
	xp_ca_scrub_memory(text, len + 1);
	free(text);
	return status;
}

static int
der_to_pem(const char *label, const unsigned char *der, size_t len,
           unsigned char **out, size_t *out_len) {
	char *body = encode64(der, len);
	if (body == NULL)
		return XP_CRYPTO_ERR;
	for (char *p = body; *p; p++) {
		if (*p == '-')
			*p = '+';
		else if (*p == '_')
			*p = '/';
	}
	size_t         body_len = strlen(body), padding = (4 - body_len % 4) % 4,
	               lines = (body_len + padding + 63) / 64;
	size_t         label_len = strlen(label);
	size_t         total = 11 + label_len + 6 + body_len + padding + lines + 9 + label_len + 6;
	unsigned char *pem = malloc(total + 1);
	if (pem == NULL) {
		xp_ca_scrub_memory(body, body_len);
		free(body);
		return XP_CRYPTO_ERR;
	}
	size_t o = 0;
	o += (size_t)sprintf((char *)pem + o, "-----BEGIN %s-----\n", label);
	for (size_t i = 0; i < body_len + padding; i++) {
		pem[o++] = i < body_len ? body[i] : '=';
		if ((i + 1) % 64 == 0 || i + 1 == body_len + padding)
			pem[o++] = '\n';
	}
	o += (size_t)sprintf((char *)pem + o, "-----END %s-----\n", label);
	pem[o] = 0;
	xp_ca_scrub_memory(body, body_len);
	free(body);
	*out = pem;
	*out_len = o;
	return XP_CRYPTO_OK;
}

static int
encrypted_der_to_pem(const unsigned char *der, size_t len,
                     unsigned char **out, size_t *out_len) {
	return der_to_pem("ENCRYPTED PRIVATE KEY", der, len, out, out_len);
}

static int
parse_manifest(xp_keyset_t keyset, unsigned char *data, size_t len) {
	const size_t header_len = sizeof(MANIFEST_HEADER) - 1;
	if (len < header_len || memcmp(data, MANIFEST_HEADER, header_len) != 0)
		return len >= 6 &&
		       memcmp(data, "xptls-", 6) == 0 ? XP_CRYPTO_ERR_FORMAT : XP_CRYPTO_ERR_MIGRATION_REQUIRED;
	char *       cursor = (char *)data + header_len, *finish = (char *)data + len;
	while (cursor < finish) {
		char *newline = memchr(cursor, '\n', (size_t)(finish - cursor));
		if (newline == NULL)
			newline = finish;
		*newline = 0;
		if (*cursor != 0) {
			char *f1 = strchr(cursor, '\t'), *f2 = f1 ? strchr(f1 + 1, '\t') : NULL, *f3 = f2 ? strchr(f2 + 1,
			                                                                                           '\t') : NULL;
			if (f1 == NULL || f2 == NULL) {
				return XP_CRYPTO_ERR_FORMAT;
			}
			*f1++ = 0;
			*f2++ = 0;
			if (f3)
				*f3++ = 0;
			unsigned char *label_bytes = NULL, *value = NULL;
			size_t         label_len = 0, value_len = 0;
			if (decode64(f1, strlen(f1), &label_bytes, &label_len) != XP_CRYPTO_OK ||
			    memchr(label_bytes, 0, label_len) != NULL) {
				free(label_bytes);
				return XP_CRYPTO_ERR_FORMAT;
			}
			label_bytes[label_len] = 0;
			if (!valid_utf8_label((char *)label_bytes)) {
				free(label_bytes);
				return XP_CRYPTO_ERR_FORMAT;
			}
			struct keyset_entry *entry = find_entry(keyset, (char *)label_bytes);
			if (entry == NULL)
				entry = add_entry(keyset, (char *)label_bytes);
			free(label_bytes);
			if (entry == NULL)
				return XP_CRYPTO_ERR;
			const char *encoded = f3 ? f3 : f2;
			if (decode64(encoded, strlen(encoded), &value,
			             &value_len) != XP_CRYPTO_OK)
				return XP_CRYPTO_ERR_FORMAT;
			if (strcmp(cursor, "K") == 0) {
				if (entry->private_data != NULL || (strcmp(f2, "P") != 0 && strcmp(f2, "R") != 0)) {
					free(value);
					return XP_CRYPTO_ERR_FORMAT;
				}
				entry->private_data = value;
				entry->private_len = value_len;
				entry->private_reference = strcmp(f2, "R") == 0;
			} else if (strcmp(cursor, "C") == 0) {
				char *        endptr = NULL;
				unsigned long index = strtoul(f2, &endptr, 10);
				if (*f2 == 0 || *endptr != 0 || index != entry->cert_count) {
					free(value);
					return XP_CRYPTO_ERR_FORMAT;
				}
				xp_ca_cert_t  cert = NULL;
				int           status = xp_ca_cert_import_der(&cert, value, value_len);
				free(value);
				if (status != XP_CA_OK)
					return status;
				xp_ca_cert_t *grown = realloc(entry->certs, (entry->cert_count + 1) * sizeof(*grown));
				if (grown == NULL) {
					xp_ca_cert_free(cert);
					return XP_CRYPTO_ERR;
				}
				entry->certs = grown;
				entry->certs[entry->cert_count++] = cert;
			} else {
				free(value);
				return XP_CRYPTO_ERR_FORMAT;
			}
		}
		cursor = newline < finish ? newline + 1 : finish;
	}
	return XP_CRYPTO_OK;
}

static int
write_manifest(xp_keyset_t keyset, FILE *file) {
	if (fwrite(MANIFEST_HEADER, 1, sizeof(MANIFEST_HEADER) - 1,
	           file) != sizeof(MANIFEST_HEADER) - 1)
		return XP_CRYPTO_ERR_IO;
	for (size_t i = 0; i < keyset->entry_count; i++) {
		struct keyset_entry *entry = &keyset->entries[i];
		char *               label = encode64(entry->label, strlen(entry->label));
		if (label == NULL)
			return XP_CRYPTO_ERR;
		if (entry->private_data) {
			char *value = encode64(entry->private_data, entry->private_len);
			if (value == NULL) {
				free(label);
				return XP_CRYPTO_ERR;
			}
			if (fprintf(file, "K\t%s\t%c\t%s\n", label, entry->private_reference ? 'R' : 'P', value) < 0) {
				free(value);
				free(label);
				return XP_CRYPTO_ERR_IO;
			}
			free(value);
		}
		for (size_t j = 0; j < entry->cert_count; j++) {
			size_t         der_len = 0;
			int            status = xp_ca_cert_export_der(entry->certs[j], NULL, &der_len);
			unsigned char *der = status == XP_CA_OK ? malloc(der_len) : NULL;
			if (der == NULL) {
				free(label);
				return status == XP_CA_OK ? XP_CRYPTO_ERR : status;
			}
			status = xp_ca_cert_export_der(entry->certs[j], der, &der_len);
			char *value = status == XP_CA_OK ? encode64(der, der_len) : NULL;
			free(der);
			if (value == NULL) {
				free(label);
				return status == XP_CA_OK ? XP_CRYPTO_ERR : status;
			}
			if (fprintf(file, "C\t%s\t%zu\t%s\n", label, j, value) < 0) {
				free(value);
				free(label);
				return XP_CRYPTO_ERR_IO;
			}
			free(value);
		}
		free(label);
	}
	return XP_CRYPTO_OK;
}

static int
commit_keyset(xp_keyset_t keyset) {
	char  temporary[MAX_PATH + 1];
	FILE *file = xp_ca_open_private_temporary(keyset->path, temporary, sizeof(temporary));
	if (file == NULL)
		return XP_CRYPTO_ERR_IO;
	int   status = XP_CRYPTO_OK;
	if (keyset->format == XP_KEYSET_FORMAT_MANIFEST)
		status = write_manifest(keyset, file);
	else {
		struct keyset_entry *identity = NULL;
		for (size_t i = 0; i < keyset->entry_count; i++) if (keyset->entries[i].private_data
			                                                 || keyset->entries[i].cert_count != 0) {
				if (identity != NULL) {
					status = XP_CRYPTO_ERR_CONFLICT;
					break;
				}
				identity = &keyset->entries[i];
			}
		const char *label = identity == NULL ? "default" : identity->label;
		xp_key_t    key = NULL;
		if (status == XP_CRYPTO_OK && identity != NULL
		    && identity->private_data != NULL)
			status = xp_keyset_get_private_key(&key, keyset, identity->label);
		size_t      size = 0;
		if (status == XP_CRYPTO_OK) status = pkcs12_export(key, label,
			                                               identity == NULL ? NULL : identity->certs,
			                                               identity == NULL ? 0 : identity->cert_count,
			                                               keyset->password, keyset->password_context, NULL, &size);
		unsigned char *data = status == XP_CRYPTO_OK ? malloc(size) : NULL;
		if (status == XP_CRYPTO_OK && data == NULL)
			status = XP_CRYPTO_ERR;
		if (status == XP_CRYPTO_OK) status = pkcs12_export(key, label,
			                                               identity == NULL ? NULL : identity->certs,
			                                               identity == NULL ? 0 : identity->cert_count,
			                                               keyset->password, keyset->password_context, data, &size);
		if (status == XP_CRYPTO_OK && fwrite(data, 1, size, file) != size)
			status = XP_CRYPTO_ERR_IO;
		if (data) {
			xp_ca_scrub_memory(data, size);
			free(data);
		}
		xp_key_release(key);
	}
	if (status != XP_CRYPTO_OK) {
		xp_ca_discard_private_temporary(file, temporary);
		return status;
	}
	return xp_ca_commit_private_temporary(file, temporary, keyset->path) == 0
	       ? XP_CRYPTO_OK : XP_CRYPTO_ERR_IO;
}

int
xp_keyset_open(xp_keyset_t *out, const struct xp_keyset_config *config) {
	if (out == NULL || config == NULL || config->path == NULL || config->path[0] == 0 ||
	    config->mode < XP_KEYSET_CREATE || config->mode > XP_KEYSET_READ_ONLY)
		return XP_CRYPTO_ERR_INVALID;
	*out = NULL;
	if (strcmp(xp_crypto_provider_name(), "none") == 0)
		return XP_CRYPTO_ERR_DISABLED;
	xp_keyset_t keyset = calloc(1, sizeof(*keyset));
	if (keyset == NULL)
		return XP_CRYPTO_ERR;
	keyset->path = copy_string(config->path);
	keyset->mode = config->mode;
	keyset->format = selected_format(config);
	keyset->password = config->password;
	keyset->password_context = config->password_context;
	keyset->key_store = config->key_store;
	if (keyset->path == NULL || (keyset->format != XP_KEYSET_FORMAT_MANIFEST &&
	                             keyset->format != XP_KEYSET_FORMAT_PKCS12)) {
		xp_keyset_close(keyset);
		return XP_CRYPTO_ERR_INVALID;
	}
	if (config->mode != XP_KEYSET_READ_ONLY) {
		int status = acquire_lock(keyset);
		if (status != XP_CRYPTO_OK) {
			xp_keyset_close(keyset);
			return status;
		}
	}
	unsigned char *data = NULL;
	size_t         len = 0;
	int            status = read_file(config->path, &data, &len);
	if (config->mode == XP_KEYSET_CREATE) {
		if (status == XP_CRYPTO_OK) {
			free(data);
			xp_keyset_close(keyset);
			return XP_CRYPTO_ERR_CONFLICT;
		}
		if (status != XP_CRYPTO_ERR_NOT_FOUND) {
			xp_keyset_close(keyset);
			return status;
		}
		if (keyset->format == XP_KEYSET_FORMAT_MANIFEST && commit_keyset(keyset) != XP_CRYPTO_OK) {
			xp_keyset_close(keyset);
			return XP_CRYPTO_ERR_IO;
		}
	} else {
		if (status != XP_CRYPTO_OK) {
			xp_keyset_close(keyset);
			return status;
		}
		if (keyset->format == XP_KEYSET_FORMAT_MANIFEST)
			status = parse_manifest(keyset, data, len);
		else {
			xp_key_t      key = NULL;
			char *        label = NULL;
			xp_ca_cert_t *certs = NULL;
			size_t        cert_count = 0;
			status = pkcs12_import(&key, &label, &certs, &cert_count, data, len,
			                       keyset->password, keyset->password_context);
			if (status == XP_CRYPTO_OK) {
				struct keyset_entry *entry = add_entry(keyset, label);
				if (entry == NULL)
					status = XP_CRYPTO_ERR;
				else {
					if (key != NULL) {
						size_t ref_len = 0;
						int    rs = xp_key_reference(key, NULL, &ref_len);
						if (rs == XP_CRYPTO_OK) {
							entry->private_data = malloc(ref_len);
							entry->private_len = ref_len;
							entry->private_reference = true;
							if (entry->private_data == NULL)
								status = XP_CRYPTO_ERR;
							else
								status = xp_key_reference(key, entry->private_data, &entry->private_len);
						} else {
							size_t         pem_len = 0;
							status = xp_key_export_private_pem(key, keyset->password, keyset->password_context, NULL, &pem_len);
							unsigned char *pem = status == XP_CRYPTO_OK ? malloc(pem_len + 1) : NULL;
							if (status == XP_CRYPTO_OK && pem == NULL)
								status = XP_CRYPTO_ERR;
							if (status == XP_CRYPTO_OK) {
								status = xp_key_export_private_pem(key, keyset->password, keyset->password_context, pem, &pem_len);
								if (status == XP_CRYPTO_OK) status = encrypted_pem_to_der(pem, pem_len, &entry->private_data,
									                                                      &entry->private_len);
							}
							if (pem) {
								xp_ca_scrub_memory(pem, pem_len);
								free(pem);
							}
						}
					}
					entry->certs = certs;
					entry->cert_count = cert_count;
					certs = NULL;
				}
				free(label);
				xp_key_release(key);
				xp_ca_cert_chain_free(certs, cert_count);
			}
		}
		free(data);
		if (status != XP_CRYPTO_OK) {
			xp_keyset_close(keyset);
			return status;
		}
	}
	for (size_t i = 0; i < keyset->entry_count; i++) {
		status = validate_entry_identity(keyset, &keyset->entries[i]);
		if (status != XP_CRYPTO_OK) {
			xp_keyset_close(keyset);
			return status;
		}
	}
	*out = keyset;
	return XP_CRYPTO_OK;
}

int
xp_keyset_close(xp_keyset_t keyset) {
	if (keyset == NULL)
		return XP_CRYPTO_ERR_INVALID;
	for (size_t i = 0; i < keyset->entry_count; i++) free_entry(&keyset->entries[i]);
	free(keyset->entries);
	if (keyset->lock_file)
		fclose(keyset->lock_file);
	free(keyset->path);
	free(keyset);
	return XP_CRYPTO_OK;
}

int
xp_keyset_add_private_key(xp_keyset_t keyset, const char *label, xp_key_t key) {
	if (keyset == NULL || key == NULL || !valid_utf8_label(label))
		return XP_CRYPTO_ERR_INVALID;
	if (keyset->mode == XP_KEYSET_READ_ONLY)
		return XP_CRYPTO_ERR_READ_ONLY;
	struct keyset_entry *entry = find_entry(keyset, label);
	bool                 created = false;
	if (entry == NULL) {
		entry = add_entry(keyset, label);
		created = true;
	}
	if (entry == NULL)
		return XP_CRYPTO_ERR;
	if (entry->private_data != NULL)
		return XP_CRYPTO_ERR_CONFLICT;
	size_t len = 0;
	int    status = xp_key_reference(key, NULL, &len);
	if (status == XP_CRYPTO_OK) {
		entry->private_data = malloc(len);
		entry->private_len = len;
		entry->private_reference = true;
		if (entry->private_data == NULL)
			status = XP_CRYPTO_ERR;
		else
			status = xp_key_reference(key, entry->private_data, &entry->private_len);
	} else if (status == XP_CRYPTO_ERR_NOT_FOUND) {
		struct xp_key_info info;
		status = xp_key_get_info(key, &info);
		if (status == XP_CRYPTO_OK && !info.exportable)
			status = XP_CRYPTO_ERR_NOT_EXPORTABLE;
		size_t             pem_len = 0;
		if (status == XP_CRYPTO_OK) status = xp_key_export_private_pem(key, keyset->password,
			                                                           keyset->password_context, NULL, &pem_len);
		unsigned char *    pem = status == XP_CRYPTO_OK ? malloc(pem_len + 1) : NULL;
		if (status == XP_CRYPTO_OK && pem == NULL)
			status = XP_CRYPTO_ERR;
		if (status == XP_CRYPTO_OK) {
			status = xp_key_export_private_pem(key, keyset->password, keyset->password_context, pem, &pem_len);
			if (status == XP_CRYPTO_OK) status = encrypted_pem_to_der(pem, pem_len, &entry->private_data,
				                                                      &entry->private_len);
		}
		if (pem) {
			xp_ca_scrub_memory(pem, pem_len);
			free(pem);
		}
	}
	if (status == XP_CRYPTO_OK)
		status = validate_entry_identity(keyset, entry);
	if (status == XP_CRYPTO_OK)
		status = commit_keyset(keyset);
	if (status != XP_CRYPTO_OK) {
		if (entry->private_data) {
			xp_ca_scrub_memory(entry->private_data, entry->private_len);
			free(entry->private_data);
			entry->private_data = NULL;
			entry->private_len = 0;
		}
		if (created) {
			free_entry(entry);
			keyset->entry_count--;
		}
	}
	return status;
}

int
xp_keyset_get_private_key(xp_key_t *out, xp_keyset_t keyset, const char *label) {
	if (out == NULL || keyset == NULL || !valid_utf8_label(label))
		return XP_CRYPTO_ERR_INVALID;
	*out = NULL;
	struct keyset_entry *entry = find_entry(keyset, label);
	if (entry == NULL || entry->private_data == NULL)
		return XP_CRYPTO_ERR_NOT_FOUND;
	if (entry->private_reference) {
		if (keyset->key_store == NULL)
			return XP_CRYPTO_ERR_UNAVAILABLE;
		return xp_key_open_stored(out, keyset->key_store, entry->private_data, entry->private_len);
	}
	unsigned char *pem = NULL;
	size_t         pem_len = 0;
	int            status = encrypted_der_to_pem(entry->private_data, entry->private_len, &pem, &pem_len);
	if (status == XP_CRYPTO_OK) status = xp_key_import_private_pem(out, pem, pem_len, keyset->password,
		                                                           keyset->password_context);
	if (pem) {
		xp_ca_scrub_memory(pem, pem_len);
		free(pem);
	}
	return status;
}

int
xp_keyset_add_certificate_chain(xp_keyset_t keyset, const char *label, const xp_ca_cert_t *certs,
                                size_t count) {
	if (keyset == NULL || !valid_utf8_label(label) || certs == NULL ||
	    count == 0)
		return XP_CRYPTO_ERR_INVALID;
	if (keyset->mode == XP_KEYSET_READ_ONLY)
		return XP_CRYPTO_ERR_READ_ONLY;
	struct keyset_entry *entry = find_entry(keyset, label);
	bool                 created = false;
	if (entry == NULL) {
		entry = add_entry(keyset, label);
		created = true;
	}
	if (entry == NULL)
		return XP_CRYPTO_ERR;
	if (entry->cert_count != 0)
		return XP_CRYPTO_ERR_CONFLICT;
	entry->certs = calloc(count, sizeof(*entry->certs));
	if (entry->certs == NULL)
		return XP_CRYPTO_ERR;
	int status = XP_CRYPTO_OK;
	for (size_t i = 0; i < count && status == XP_CRYPTO_OK; i++) {
		size_t         len = 0;
		status = xp_ca_cert_export_der(certs[i], NULL, &len);
		unsigned char *der = status == XP_CA_OK ? malloc(len) : NULL;
		if (status == XP_CA_OK && der == NULL)
			status = XP_CRYPTO_ERR;
		if (status == XP_CA_OK)
			status = xp_ca_cert_export_der(certs[i], der, &len);
		if (status == XP_CA_OK)
			status = xp_ca_cert_import_der(&entry->certs[i], der, len);
		free(der);
		if (status == XP_CA_OK)
			entry->cert_count++;
	}
	if (status == XP_CA_OK)
		status = validate_entry_identity(keyset, entry);
	if (status == XP_CA_OK)
		status = commit_keyset(keyset);
	if (status != XP_CA_OK) {
		xp_ca_cert_chain_free(entry->certs, entry->cert_count);
		entry->certs = NULL;
		entry->cert_count = 0;
		if (created) {
			free_entry(entry);
			keyset->entry_count--;
		}
	}
	return status;
}

int
xp_keyset_get_certificate_chain(xp_ca_cert_t **out, size_t *count, xp_keyset_t keyset,
                                const char *label) {
	if (out == NULL || count == NULL || keyset == NULL ||
	    !valid_utf8_label(label))
		return XP_CRYPTO_ERR_INVALID;
	*out = NULL;
	*count = 0;
	struct keyset_entry *entry = find_entry(keyset, label);
	if (entry == NULL || entry->cert_count == 0)
		return XP_CRYPTO_ERR_NOT_FOUND;
	xp_ca_cert_t *       result = calloc(entry->cert_count, sizeof(*result));
	if (result == NULL)
		return XP_CRYPTO_ERR;
	int                  status = XP_CA_OK;
	for (size_t i = 0; i < entry->cert_count && status == XP_CA_OK; i++) {
		size_t         len = 0;
		status = xp_ca_cert_export_der(entry->certs[i], NULL, &len);
		unsigned char *der = status == XP_CA_OK ? malloc(len) : NULL;
		if (status == XP_CA_OK && der == NULL)
			status = XP_CRYPTO_ERR;
		if (status == XP_CA_OK)
			status = xp_ca_cert_export_der(entry->certs[i], der, &len);
		if (status == XP_CA_OK)
			status = xp_ca_cert_import_der(&result[i], der, len);
		free(der);
	}
	if (status != XP_CA_OK) {
		xp_ca_cert_chain_free(result, entry->cert_count);
		return status;
	}
	*out = result;
	*count = entry->cert_count;
	return XP_CA_OK;
}

int
xp_keyset_delete(xp_keyset_t keyset, const char *label, unsigned objects) {
	if (keyset == NULL || !valid_utf8_label(label) ||
	    (objects & ~(XP_KEYSET_PRIVATE_KEY | XP_KEYSET_CERTIFICATE_CHAIN)) != 0 ||
	    objects == 0)
		return XP_CRYPTO_ERR_INVALID;
	if (keyset->mode == XP_KEYSET_READ_ONLY)
		return XP_CRYPTO_ERR_READ_ONLY;
	struct keyset_entry *entry = find_entry(keyset, label);
	if (entry == NULL)
		return XP_CRYPTO_ERR_NOT_FOUND;
	unsigned char *      private_data = entry->private_data;
	size_t               private_len = entry->private_len;
	bool                 private_reference = entry->private_reference;
	xp_ca_cert_t *       certs = entry->certs;
	size_t               cert_count = entry->cert_count;
	bool                 changed = false;
	if ((objects & XP_KEYSET_PRIVATE_KEY) && entry->private_data) {
		entry->private_data = NULL;
		entry->private_len = 0;
		changed = true;
	}
	if ((objects & XP_KEYSET_CERTIFICATE_CHAIN) && entry->certs) {
		entry->certs = NULL;
		entry->cert_count = 0;
		changed = true;
	}
	if (!changed)
		return XP_CRYPTO_ERR_NOT_FOUND;
	int status = commit_keyset(keyset);
	if (status != XP_CRYPTO_OK) {
		entry->private_data = private_data;
		entry->private_len = private_len;
		entry->private_reference = private_reference;
		entry->certs = certs;
		entry->cert_count = cert_count;
		return status;
	}
	if ((objects & XP_KEYSET_PRIVATE_KEY) && private_data) {
		xp_ca_scrub_memory(private_data, private_len);
		free(private_data);
	}
	if ((objects & XP_KEYSET_CERTIFICATE_CHAIN) && certs)
		xp_ca_cert_chain_free(certs, cert_count);
	return XP_CRYPTO_OK;
}

/* Provider-neutral PFX v3 framing.  The private-key bag contains the
 * provider's PBES2 EncryptedPrivateKeyInfo, while the common layer adds and
 * verifies the PKCS#12 SHA-256 integrity MAC. */
struct bytes {
	unsigned char *data;
	size_t len;
};
static void
bytes_free(struct bytes *b) {
	if (b && b->data) {
		xp_ca_scrub_memory(b->data, b->len);
		free(b->data);
	}
	if (b) {
		b->data = NULL;
		b->len = 0;
	}
}
static int
bytes_join(struct bytes *out, const struct bytes *items, size_t count) {
	size_t total = 0;
	for (size_t i = 0; i < count; i++) {
		if (SIZE_MAX - total < items[i].len)
			return XP_CRYPTO_ERR;
		total += items[i].len;
	}
	out->data = malloc(total?total:1);
	if (out->data == NULL)
		return XP_CRYPTO_ERR;
	out->len = total;
	size_t p = 0;
	for (size_t i = 0; i < count; i++) {
		memcpy(out->data + p, items[i].data, items[i].len);
		p += items[i].len;
	}
	return XP_CRYPTO_OK;
}
static size_t
der_len_size(size_t n) {
	if (n < 128)
		return 1;
	size_t z = 1;
	do {
		z++;
		n >>= 8;
	} while (n);
	return z;
}
static int
der_wrap(struct bytes *out, unsigned char tag, const void *data, size_t len) {
	size_t ls = der_len_size(len);
	out->len = 1 + ls + len;
	out->data = malloc(out->len);
	if (out->data == NULL)
		return XP_CRYPTO_ERR;
	size_t p = 0;
	out->data[p++] = tag;
	if (len < 128)
		out->data[p++] = (unsigned char)len;
	else {
		size_t n = 0, v = len;
		while (v) {
			n++;
			v >>= 8;
		}
		out->data[p++] = 0x80 | (unsigned char)n;
		for (size_t i = n; i > 0; i--) out->data[p++] = (unsigned char)(len >> (8 * (i - 1)));
	}
	if (len != 0)
		memcpy(out->data + p, data, len);
	return XP_CRYPTO_OK;
}
static int
der_container(struct bytes *out, unsigned char tag, const struct bytes *items, size_t count) {
	struct bytes joined = {0};
	int          s = bytes_join(&joined, items, count);
	if (s == XP_CRYPTO_OK)
		s = der_wrap(out, tag, joined.data, joined.len);
	bytes_free(&joined);
	return s;
}
static int
der_oid(struct bytes *out, const unsigned char *oid, size_t len) {
	return der_wrap(out, 0x06, oid, len);
}

#define PFX_MAC_ITERATIONS 2048u
#define PFX_SHA256_SIZE 32u
#define PFX_SHA256_BLOCK_SIZE 64u

static int
keyset_secret(xp_crypto_secret_callback_t callback, void *context,
              unsigned char **out, size_t *out_len) {
	if (callback == NULL || out == NULL || out_len == NULL)
		return XP_CRYPTO_ERR_AUTHORIZATION;
	*out = NULL;
	*out_len = 0;
	size_t         required = 0;
	if (callback(context, NULL, 0, &required) != XP_CRYPTO_OK || required == 0)
		return XP_CRYPTO_ERR_AUTHORIZATION;
	unsigned char *secret = malloc(required);
	if (secret == NULL)
		return XP_CRYPTO_ERR;
	size_t         actual = required;
	if (callback(context, secret, required, &actual) != XP_CRYPTO_OK
	    || actual != required) {
		xp_ca_scrub_memory(secret, required);
		free(secret);
		return XP_CRYPTO_ERR_AUTHORIZATION;
	}
	*out = secret;
	*out_len = actual;
	return XP_CRYPTO_OK;
}

static int
sha256_parts(const struct bytes *parts, size_t count,
             unsigned char output[PFX_SHA256_SIZE]) {
	xp_digest_t digest = xp_digest_create(XP_DIGEST_SHA256);
	if (digest == NULL)
		return XP_CRYPTO_ERR;
	int         status = XP_CRYPTO_OK;
	for (size_t i = 0; i < count && status == XP_CRYPTO_OK; i++)
		status = xp_digest_update(digest, parts[i].data, parts[i].len);
	size_t      output_len = PFX_SHA256_SIZE;
	if (status == XP_CRYPTO_OK)
		status = xp_digest_final(digest, output, &output_len);
	xp_digest_free(digest);
	return status == XP_CRYPTO_OK && output_len == PFX_SHA256_SIZE
	       ? XP_CRYPTO_OK : status == XP_CRYPTO_OK ? XP_CRYPTO_ERR : status;
}

static int
hmac_sha256(const unsigned char *key, size_t key_len,
            const void *data, size_t data_len,
            unsigned char output[PFX_SHA256_SIZE]) {
	unsigned char normalized[PFX_SHA256_SIZE];
	unsigned char inner[PFX_SHA256_SIZE];
	unsigned char ipad[PFX_SHA256_BLOCK_SIZE];
	unsigned char opad[PFX_SHA256_BLOCK_SIZE];
	if (key_len > PFX_SHA256_BLOCK_SIZE) {
		struct bytes key_part = {(unsigned char *)key, key_len};
		int          status = sha256_parts(&key_part, 1, normalized);
		if (status != XP_CRYPTO_OK)
			return status;
		key = normalized;
		key_len = sizeof(normalized);
	}
	memset(ipad, 0x36, sizeof(ipad));
	memset(opad, 0x5c, sizeof(opad));
	for (size_t i = 0; i < key_len; i++) {
		ipad[i] ^= key[i];
		opad[i] ^= key[i];
	}
	struct bytes inner_parts[] = {
		{ipad, sizeof(ipad)}, {(unsigned char *)data, data_len},
	};
	int          status = sha256_parts(inner_parts, 2, inner);
	if (status == XP_CRYPTO_OK) {
		struct bytes outer_parts[] = {
			{opad, sizeof(opad)}, {inner, sizeof(inner)},
		};
		status = sha256_parts(outer_parts, 2, output);
	}
	xp_ca_scrub_memory(normalized, sizeof(normalized));
	xp_ca_scrub_memory(inner, sizeof(inner));
	xp_ca_scrub_memory(ipad, sizeof(ipad));
	xp_ca_scrub_memory(opad, sizeof(opad));
	return status;
}

/* RFC 7292 Appendix B, using SHA-256 and diversifier 3 (MAC material). */
static int
pkcs12_mac_key(const unsigned char *password, size_t password_len,
               const unsigned char *salt, size_t salt_len, uint32_t iterations,
               unsigned char output[PFX_SHA256_SIZE]) {
	if (iterations == 0 || password_len > (SIZE_MAX / 2) - 1)
		return XP_CRYPTO_ERR_INVALID;
	size_t         unicode_len = (password_len + 1) * 2;
	unsigned char *unicode = calloc(unicode_len, 1);
	if (unicode == NULL)
		return XP_CRYPTO_ERR;
	for (size_t i = 0; i < password_len; i++)
		unicode[2 * i + 1] = password[i];

	size_t salt_repeated = salt_len == 0 ? 0
	                       : PFX_SHA256_BLOCK_SIZE * ((salt_len + PFX_SHA256_BLOCK_SIZE - 1)
	                                                  / PFX_SHA256_BLOCK_SIZE);
	size_t password_repeated = PFX_SHA256_BLOCK_SIZE
	                           * ((unicode_len + PFX_SHA256_BLOCK_SIZE - 1) / PFX_SHA256_BLOCK_SIZE);
	if (salt_repeated > SIZE_MAX - password_repeated) {
		xp_ca_scrub_memory(unicode, unicode_len);
		free(unicode);
		return XP_CRYPTO_ERR;
	}
	size_t         input_len = salt_repeated + password_repeated;
	unsigned char *input = malloc(input_len);
	if (input == NULL) {
		xp_ca_scrub_memory(unicode, unicode_len);
		free(unicode);
		return XP_CRYPTO_ERR;
	}
	for (size_t i = 0; i < salt_repeated; i++)
		input[i] = salt[i % salt_len];
	for (size_t i = 0; i < password_repeated; i++)
		input[salt_repeated + i] = unicode[i % unicode_len];
	unsigned char diversifier[PFX_SHA256_BLOCK_SIZE];
	memset(diversifier, 3, sizeof(diversifier));
	struct bytes  initial[] = {
		{diversifier, sizeof(diversifier)}, {input, input_len},
	};
	int           status = sha256_parts(initial, 2, output);
	for (uint32_t i = 1; i < iterations && status == XP_CRYPTO_OK; i++) {
		struct bytes prior = {output, PFX_SHA256_SIZE};
		status = sha256_parts(&prior, 1, output);
	}
	xp_ca_scrub_memory(diversifier, sizeof(diversifier));
	xp_ca_scrub_memory(input, input_len);
	free(input);
	xp_ca_scrub_memory(unicode, unicode_len);
	free(unicode);
	return status;
}

static int
der_uint32(struct bytes *out, uint32_t value) {
	unsigned char encoded[5];
	size_t        first = sizeof(encoded);
	do {
		encoded[--first] = (unsigned char)value;
		value >>= 8;
	} while (value != 0);
	if ((encoded[first] & 0x80) != 0)
		encoded[--first] = 0;
	return der_wrap(out, 0x02, encoded + first, sizeof(encoded) - first);
}

static int
make_mac_data(struct bytes *out, const struct bytes *authenticated_safe,
              xp_crypto_secret_callback_t password, void *password_context) {
	static const unsigned char sha256_oid[] = {
		0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01,
	};
	unsigned char              salt[16];
	unsigned char              key[PFX_SHA256_SIZE];
	unsigned char              mac[PFX_SHA256_SIZE];
	unsigned char *            secret = NULL;
	size_t                     secret_len = 0;
	int                        status = keyset_secret(password, password_context, &secret, &secret_len);
	if (status == XP_CRYPTO_OK)
		status = xp_crypto_random(salt, sizeof(salt));
	if (status == XP_CRYPTO_OK)
		status = pkcs12_mac_key(secret, secret_len, salt, sizeof(salt),
		                        PFX_MAC_ITERATIONS, key);
	if (status == XP_CRYPTO_OK)
		status = hmac_sha256(key, sizeof(key), authenticated_safe->data,
		                     authenticated_safe->len, mac);
	struct bytes oid = {0}, null_value = {0}, algorithm = {0};
	struct bytes digest = {0}, digest_info = {0}, salt_value = {0};
	struct bytes iterations = {0};
	if (status == XP_CRYPTO_OK)
		status = der_oid(&oid, sha256_oid, sizeof(sha256_oid));
	if (status == XP_CRYPTO_OK)
		status = der_wrap(&null_value, 0x05, NULL, 0);
	struct bytes algorithm_parts[] = {oid, null_value};
	if (status == XP_CRYPTO_OK)
		status = der_container(&algorithm, 0x30, algorithm_parts, 2);
	if (status == XP_CRYPTO_OK)
		status = der_wrap(&digest, 0x04, mac, sizeof(mac));
	struct bytes digest_parts[] = {algorithm, digest};
	if (status == XP_CRYPTO_OK)
		status = der_container(&digest_info, 0x30, digest_parts, 2);
	if (status == XP_CRYPTO_OK)
		status = der_wrap(&salt_value, 0x04, salt, sizeof(salt));
	if (status == XP_CRYPTO_OK)
		status = der_uint32(&iterations, PFX_MAC_ITERATIONS);
	struct bytes mac_parts[] = {digest_info, salt_value, iterations};
	if (status == XP_CRYPTO_OK)
		status = der_container(out, 0x30, mac_parts, 3);
	bytes_free(&oid);
	bytes_free(&null_value);
	bytes_free(&algorithm);
	bytes_free(&digest);
	bytes_free(&digest_info);
	bytes_free(&salt_value);
	bytes_free(&iterations);
	if (secret != NULL) {
		xp_ca_scrub_memory(secret, secret_len);
		free(secret);
	}
	xp_ca_scrub_memory(key, sizeof(key));
	xp_ca_scrub_memory(mac, sizeof(mac));
	return status;
}
static int
friendly_attrs(struct bytes *out, const char *label) {
	static const unsigned char friendly[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x14};
	size_t                     n = strlen(label);
	unsigned char *            wide = malloc(n * 2);
	if (wide == NULL)
		return XP_CRYPTO_ERR;
	for (size_t i = 0; i < n; i++) {
		if ((unsigned char)label[i] >= 0x80) {
			free(wide);
			return XP_CRYPTO_ERR_UNSUPPORTED;
		}
		wide[2 * i] = 0;
		wide[2 * i + 1] = (unsigned char)label[i];
	}
	struct bytes bmp = {0}, setv = {0}, oid = {0}, attr = {0};
	int          s = der_wrap(&bmp, 0x1e, wide, n * 2);
	free(wide);
	if (s == XP_CRYPTO_OK)
		s = der_container(&setv, 0x31, &bmp, 1);
	if (s == XP_CRYPTO_OK)
		s = der_oid(&oid, friendly, sizeof(friendly));
	struct bytes parts[] = {oid, setv};
	if (s == XP_CRYPTO_OK)
		s = der_container(&attr, 0x30, parts, 2);
	if (s == XP_CRYPTO_OK)
		s = der_container(out, 0x31, &attr, 1);
	bytes_free(&bmp);
	bytes_free(&setv);
	bytes_free(&oid);
	bytes_free(&attr);
	return s;
}
static int
make_key_bag(struct bytes *out, const unsigned char *key, size_t key_len, const char *label) {
	static const unsigned char oidv[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x0c, 0x0a, 0x01, 0x02};
	struct bytes               oid = {0}, value = {0}, attrs = {0};
	int                        s = der_oid(&oid, oidv, sizeof(oidv));
	if (s == XP_CRYPTO_OK)
		s = der_wrap(&value, 0xa0, key, key_len);
	if (s == XP_CRYPTO_OK)
		s = friendly_attrs(&attrs, label);
	struct bytes p[] = {oid, value, attrs};
	if (s == XP_CRYPTO_OK)
		s = der_container(out, 0x30, p, 3);
	bytes_free(&oid);
	bytes_free(&value);
	bytes_free(&attrs);
	return s;
}
static int
make_cert_bag(struct bytes *out, const unsigned char *cert, size_t cert_len, const char *label) {
	static const unsigned char bagoid[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x0c, 0x0a, 0x01, 0x03}, x509oid[]
	    = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x16, 0x01};
	struct bytes               bo = {0}, xo = {0}, oct = {0}, cv = {0}, explicitv = {0}, attrs = {0};
	int                        s = der_oid(&bo, bagoid, sizeof(bagoid));
	if (s == XP_CRYPTO_OK)
		s = der_oid(&xo, x509oid, sizeof(x509oid));
	if (s == XP_CRYPTO_OK)
		s = der_wrap(&oct, 0x04, cert, cert_len);
	if (s == XP_CRYPTO_OK)
		s = der_wrap(&explicitv, 0xa0, oct.data, oct.len);
	struct bytes cp[] = {xo, explicitv};
	if (s == XP_CRYPTO_OK)
		s = der_container(&cv, 0x30, cp, 2);
	struct bytes wrapped = {0};
	if (s == XP_CRYPTO_OK)
		s = der_wrap(&wrapped, 0xa0, cv.data, cv.len);
	if (s == XP_CRYPTO_OK)
		s = friendly_attrs(&attrs, label);
	struct bytes bp[] = {bo, wrapped, attrs};
	if (s == XP_CRYPTO_OK)
		s = der_container(out, 0x30, bp, 3);
	bytes_free(&bo);
	bytes_free(&xo);
	bytes_free(&oct);
	bytes_free(&cv);
	bytes_free(&explicitv);
	bytes_free(&wrapped);
	bytes_free(&attrs);
	return s;
}
static int
content_info_data(struct bytes *out, const struct bytes *payload) {
	static const unsigned char dataoid[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01};
	struct bytes               oid = {0}, oct = {0}, explicitv = {0};
	int                        s = der_oid(&oid, dataoid, sizeof(dataoid));
	if (s == XP_CRYPTO_OK)
		s = der_wrap(&oct, 0x04, payload->data, payload->len);
	if (s == XP_CRYPTO_OK)
		s = der_wrap(&explicitv, 0xa0, oct.data, oct.len);
	struct bytes p[] = {oid, explicitv};
	if (s == XP_CRYPTO_OK)
		s = der_container(out, 0x30, p, 2);
	bytes_free(&oid);
	bytes_free(&oct);
	bytes_free(&explicitv);
	return s;
}

static int
pkcs12_export(xp_key_t key, const char *label, const xp_ca_cert_t *certs,
              size_t cert_count, xp_crypto_secret_callback_t password, void *password_context, void *out,
              size_t *len) {
	if (label == NULL || len == NULL || (cert_count != 0 && certs == NULL))
		return XP_CRYPTO_ERR_INVALID;
	size_t         pem_len = 0;
	int            s = XP_CRYPTO_OK;
	unsigned char *pem = NULL;
	unsigned char *keyder = NULL;
	size_t         keylen = 0;
	if (key != NULL)
		s = xp_key_export_private_pem(key, password, password_context, NULL, &pem_len);
	if (s == XP_CRYPTO_OK && key != NULL) {
		pem = malloc(pem_len + 1);
		if (pem == NULL)
			s = XP_CRYPTO_ERR;
	}
	if (s == XP_CRYPTO_OK && key != NULL) {
		s = xp_key_export_private_pem(key, password, password_context, pem, &pem_len);
		pem[pem_len] = 0;
	}
	if (s == XP_CRYPTO_OK && key != NULL)
		s = encrypted_pem_to_der(pem, pem_len, &keyder, &keylen);
	if (pem) {
		xp_ca_scrub_memory(pem, pem_len);
		free(pem);
	}
	if (s != XP_CRYPTO_OK)
		return s;
	size_t        key_bags = key == NULL ? 0 : 1;
	size_t        bag_count = cert_count + key_bags;
	struct bytes *bags = calloc(bag_count == 0 ? 1 : bag_count, sizeof(*bags));
	if (bags == NULL) {
		if (keyder != NULL)
			xp_ca_scrub_memory(keyder, keylen);
		free(keyder);
		return XP_CRYPTO_ERR;
	}
	if (key != NULL) {
		s = make_key_bag(&bags[0], keyder, keylen, label);
		xp_ca_scrub_memory(keyder, keylen);
		free(keyder);
	}
	for (size_t i = 0; i < cert_count && s == XP_CRYPTO_OK; i++) {
		size_t         dl = 0;
		s = xp_ca_cert_export_der(certs[i], NULL, &dl);
		unsigned char *d = s == XP_CA_OK ? malloc(dl) : NULL;
		if (s == XP_CA_OK && d == NULL)
			s = XP_CRYPTO_ERR;
		if (s == XP_CA_OK)
			s = xp_ca_cert_export_der(certs[i], d, &dl);
		if (s == XP_CA_OK)
			s = make_cert_bag(&bags[i + key_bags], d, dl, label);
		free(d);
	}
	struct bytes safe = {0}, inner = {0}, authsafe = {0}, outerci = {0}, version = {0}, macdata = {0}, pfx
	    = {0};
	if (s == XP_CRYPTO_OK)
		s = der_container(&safe, 0x30, bags, bag_count);
	if (s == XP_CRYPTO_OK)
		s = content_info_data(&inner, &safe);
	if (s == XP_CRYPTO_OK)
		s = der_container(&authsafe, 0x30, &inner, 1);
	if (s == XP_CRYPTO_OK)
		s = content_info_data(&outerci, &authsafe);
	unsigned char three = 3;
	if (s == XP_CRYPTO_OK)
		s = der_wrap(&version, 0x02, &three, 1);
	if (s == XP_CRYPTO_OK)
		s = make_mac_data(&macdata, &authsafe, password, password_context);
	struct bytes pp[] = {version, outerci, macdata};
	if (s == XP_CRYPTO_OK)
		s = der_container(&pfx, 0x30, pp, 3);
	for (size_t i = 0; i < bag_count; i++) bytes_free(&bags[i]);
	free(bags);
	bytes_free(&safe);
	bytes_free(&inner);
	bytes_free(&authsafe);
	bytes_free(&outerci);
	bytes_free(&version);
	bytes_free(&macdata);
	if (s == XP_CRYPTO_OK) {
		if (out == NULL)
			*len = pfx.len;
		else if (*len < pfx.len) {
			*len = pfx.len;
			s = XP_CRYPTO_ERR_BUFFER_TOO_SMALL;
		} else {
			memcpy(out, pfx.data, pfx.len);
			*len = pfx.len;
		}
	}
	bytes_free(&pfx);
	return s;
}

struct view {
	unsigned char tag;
	const unsigned char *data;
	size_t len;
};
static int
take_view(const unsigned char **p, size_t *left, struct view *v) {
	if (*left < 2)
		return XP_CRYPTO_ERR_FORMAT;
	v->tag = *(*p)++;
	(*left)--;
	unsigned char b = *(*p)++;
	(*left)--;
	size_t        n = 0;
	if (!(b & 0x80))
		n = b;
	else {
		size_t z = b & 0x7f;
		if (z == 0 || z > sizeof(size_t) || *left < z)
			return XP_CRYPTO_ERR_FORMAT;
		for (size_t i = 0; i < z; i++) n = (n << 8) | *(*p)++;
		*left -= z;
	}
	if (n > *left)
		return XP_CRYPTO_ERR_FORMAT;
	v->data = *p;
	v->len = n;
	*p += n;
	*left -= n;
	return XP_CRYPTO_OK;
}
static bool
oid_equal(struct view v, const unsigned char *oid, size_t n) {
	return v.tag == 0x06 && v.len == n && memcmp(v.data, oid, n) == 0;
}
static char *
parse_friendly(struct view set) {
	static const unsigned char foid[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x14};
	struct view                seq, oid, values, bmp;
	if (set.tag != 0x31)
		return NULL;
	const unsigned char *      p = set.data;
	size_t                     left = set.len;
	if (take_view(&p, &left, &seq) != XP_CRYPTO_OK || seq.tag != 0x30)
		return NULL;
	p = seq.data;
	left = seq.len;
	if (take_view(&p, &left, &oid) != XP_CRYPTO_OK || !oid_equal(oid, foid, sizeof(foid)) ||
	    take_view(&p, &left, &values) != XP_CRYPTO_OK || values.tag != 0x31)
		return NULL;
	p = values.data;
	left = values.len;
	if (take_view(&p, &left, &bmp) != XP_CRYPTO_OK || bmp.tag != 0x1e || bmp.len % 2)
		return NULL;
	char *s = malloc(bmp.len / 2 + 1);
	if (s == NULL)
		return NULL;
	for (size_t i = 0; i < bmp.len / 2; i++) {
		if (bmp.data[2 * i] != 0 || bmp.data[2 * i + 1] == 0) {
			free(s);
			return NULL;
		}
		s[i] = (char)bmp.data[2 * i + 1];
	}
	s[bmp.len / 2] = 0;
	return s;
}

static int
view_uint32(struct view value, uint32_t *out) {
	if (value.tag != 0x02 || value.len == 0 || value.len > 5
	    || (value.data[0] & 0x80) != 0
	    || (value.len == 5 && value.data[0] != 0))
		return XP_CRYPTO_ERR_FORMAT;
	uint32_t result = 0;
	for (size_t i = value.len > 4 ? 1 : 0; i < value.len; i++)
		result = (result << 8) | value.data[i];
	*out = result;
	return XP_CRYPTO_OK;
}

static void hmac_sha1(const unsigned char *key, size_t key_len,
                      const void *data, size_t data_len, unsigned char output[20]);
static int pkcs12_sha1_key(const unsigned char *password, size_t password_len,
                           const unsigned char *salt, size_t salt_len, uint32_t iterations,
                           unsigned diversifier_value, unsigned char *output, size_t output_len);

static int
verify_mac_data(struct view mac_data, const void *authenticated_safe,
                size_t authenticated_safe_len, xp_crypto_secret_callback_t password,
                void *password_context) {
	static const unsigned char sha256_oid[] = {
		0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01,
	};
	static const unsigned char sha1_oid[] = {
		0x2b, 0x0e, 0x03, 0x02, 0x1a,
	};
	if (mac_data.tag != 0x30)
		return XP_CRYPTO_ERR_FORMAT;
	const unsigned char *      p = mac_data.data;
	size_t                     left = mac_data.len;
	struct view                digest_info, salt, iterations_view;
	if (take_view(&p, &left, &digest_info) != XP_CRYPTO_OK
	    || digest_info.tag != 0x30
	    || take_view(&p, &left, &salt) != XP_CRYPTO_OK || salt.tag != 0x04)
		return XP_CRYPTO_ERR_FORMAT;
	uint32_t iterations = 1;
	if (left != 0) {
		if (take_view(&p, &left, &iterations_view) != XP_CRYPTO_OK
		    || view_uint32(iterations_view, &iterations) != XP_CRYPTO_OK
		    || iterations == 0 || left != 0)
			return XP_CRYPTO_ERR_FORMAT;
	}
	p = digest_info.data;
	left = digest_info.len;
	struct view algorithm, digest;
	if (take_view(&p, &left, &algorithm) != XP_CRYPTO_OK
	    || algorithm.tag != 0x30
	    || take_view(&p, &left, &digest) != XP_CRYPTO_OK
	    || digest.tag != 0x04 || left != 0)
		return XP_CRYPTO_ERR_FORMAT;
	p = algorithm.data;
	left = algorithm.len;
	struct view oid, parameters;
	if (take_view(&p, &left, &oid) != XP_CRYPTO_OK)
		return XP_CRYPTO_ERR_FORMAT;
	bool        legacy_sha1 = oid_equal(oid, sha1_oid, sizeof(sha1_oid));
	if (!legacy_sha1 && !oid_equal(oid, sha256_oid, sizeof(sha256_oid)))
		return XP_CRYPTO_ERR_UNSUPPORTED;
	if (digest.len != (legacy_sha1 ? 20u : PFX_SHA256_SIZE))
		return XP_CRYPTO_ERR_FORMAT;
	if (left != 0 && (take_view(&p, &left, &parameters) != XP_CRYPTO_OK
	                  || parameters.tag != 0x05 || parameters.len != 0 || left != 0))
		return XP_CRYPTO_ERR_FORMAT;

	unsigned char *secret = NULL;
	size_t         secret_len = 0;
	unsigned char  key[PFX_SHA256_SIZE];
	unsigned char  actual[PFX_SHA256_SIZE];
	int            status = keyset_secret(password, password_context, &secret, &secret_len);
	if (status == XP_CRYPTO_OK) {
		if (legacy_sha1) {
			status = pkcs12_sha1_key(secret, secret_len, salt.data, salt.len,
			                         iterations, 3, key, 20);
			if (status == XP_CRYPTO_OK)
				hmac_sha1(key, 20, authenticated_safe,
				          authenticated_safe_len, actual);
		} else {
			status = pkcs12_mac_key(secret, secret_len, salt.data, salt.len,
			                        iterations, key);
			if (status == XP_CRYPTO_OK)
				status = hmac_sha256(key, PFX_SHA256_SIZE, authenticated_safe,
				                     authenticated_safe_len, actual);
		}
	}
	unsigned difference = 0;
	if (status == XP_CRYPTO_OK) {
		for (size_t i = 0; i < digest.len; i++)
			difference |= actual[i] ^ digest.data[i];
		if (difference != 0)
			status = XP_CRYPTO_ERR_AUTHORIZATION;
	}
	if (secret != NULL) {
		xp_ca_scrub_memory(secret, secret_len);
		free(secret);
	}
	xp_ca_scrub_memory(key, sizeof(key));
	xp_ca_scrub_memory(actual, sizeof(actual));
	return status;
}

/* SHA-1 is intentionally private to PKCS#12 legacy import.  It is not an
 * advertised digest or signing primitive. */
struct legacy_sha1 {
	uint32_t state[5];
	uint64_t bytes;
	unsigned char block[64];
	size_t used;
};

static uint32_t
sha1_rotate(uint32_t value, unsigned bits) {
	return (value << bits) | (value >> (32 - bits));
}

static void
sha1_compress(struct legacy_sha1 *context, const unsigned char block[64]) {
	uint32_t words[80];
	for (size_t i = 0; i < 16; i++)
		words[i] = ((uint32_t)block[4 * i] << 24)
		           | ((uint32_t)block[4 * i + 1] << 16)
		           | ((uint32_t)block[4 * i + 2] << 8) | block[4 * i + 3];
	for (size_t i = 16; i < 80; i++)
		words[i] = sha1_rotate(words[i - 3] ^ words[i - 8]
		                       ^ words[i - 14] ^ words[i - 16], 1);
	uint32_t a = context->state[0], b = context->state[1];
	uint32_t c = context->state[2], d = context->state[3];
	uint32_t e = context->state[4];
	for (size_t i = 0; i < 80; i++) {
		uint32_t function, constant;
		if (i < 20) {
			function = (b & c) | (~b & d);
			constant = UINT32_C(0x5a827999);
		} else if (i < 40) {
			function = b ^ c ^ d;
			constant = UINT32_C(0x6ed9eba1);
		} else if (i < 60) {
			function = (b & c) | (b & d) | (c & d);
			constant = UINT32_C(0x8f1bbcdc);
		} else {
			function = b ^ c ^ d;
			constant = UINT32_C(0xca62c1d6);
		}
		uint32_t temporary = sha1_rotate(a, 5) + function + e
		                     + constant + words[i];
		e = d;
		d = c;
		c = sha1_rotate(b, 30);
		b = a;
		a = temporary;
	}
	context->state[0] += a;
	context->state[1] += b;
	context->state[2] += c;
	context->state[3] += d;
	context->state[4] += e;
	xp_ca_scrub_memory(words, sizeof(words));
}

static void
sha1_init(struct legacy_sha1 *context) {
	context->state[0] = UINT32_C(0x67452301);
	context->state[1] = UINT32_C(0xefcdab89);
	context->state[2] = UINT32_C(0x98badcfe);
	context->state[3] = UINT32_C(0x10325476);
	context->state[4] = UINT32_C(0xc3d2e1f0);
	context->bytes = 0;
	context->used = 0;
}

static void
sha1_update(struct legacy_sha1 *context, const void *data, size_t len) {
	const unsigned char *input = data;
	context->bytes += len;
	while (len != 0) {
		size_t take = sizeof(context->block) - context->used;
		if (take > len)
			take = len;
		memcpy(context->block + context->used, input, take);
		context->used += take;
		input += take;
		len -= take;
		if (context->used == sizeof(context->block)) {
			sha1_compress(context, context->block);
			context->used = 0;
		}
	}
}

static void
sha1_final(struct legacy_sha1 *context, unsigned char output[20]) {
	uint64_t bits = context->bytes * 8;
	context->block[context->used++] = 0x80;
	if (context->used > 56) {
		memset(context->block + context->used, 0,
		       sizeof(context->block) - context->used);
		sha1_compress(context, context->block);
		context->used = 0;
	}
	memset(context->block + context->used, 0, 56 - context->used);
	for (size_t i = 0; i < 8; i++)
		context->block[63 - i] = (unsigned char)(bits >> (8 * i));
	sha1_compress(context, context->block);
	for (size_t i = 0; i < 5; i++) {
		output[4 * i] = (unsigned char)(context->state[i] >> 24);
		output[4 * i + 1] = (unsigned char)(context->state[i] >> 16);
		output[4 * i + 2] = (unsigned char)(context->state[i] >> 8);
		output[4 * i + 3] = (unsigned char)context->state[i];
	}
	xp_ca_scrub_memory(context, sizeof(*context));
}

static void
sha1_parts(const struct bytes *parts, size_t count, unsigned char output[20]) {
	struct legacy_sha1 context;
	sha1_init(&context);
	for (size_t i = 0; i < count; i++)
		sha1_update(&context, parts[i].data, parts[i].len);
	sha1_final(&context, output);
}

static void
hmac_sha1(const unsigned char *key, size_t key_len, const void *data,
          size_t data_len, unsigned char output[20]) {
	unsigned char normalized[20] = {0};
	unsigned char inner[20];
	unsigned char ipad[64], opad[64];
	if (key_len > sizeof(ipad)) {
		struct bytes part = {(unsigned char *)key, key_len};
		sha1_parts(&part, 1, normalized);
		key = normalized;
		key_len = sizeof(normalized);
	}
	memset(ipad, 0x36, sizeof(ipad));
	memset(opad, 0x5c, sizeof(opad));
	for (size_t i = 0; i < key_len; i++) {
		ipad[i] ^= key[i];
		opad[i] ^= key[i];
	}
	struct bytes inner_parts[] = {
		{ipad, sizeof(ipad)}, {(unsigned char *)data, data_len},
	};
	sha1_parts(inner_parts, 2, inner);
	struct bytes outer_parts[] = {
		{opad, sizeof(opad)}, {inner, sizeof(inner)},
	};
	sha1_parts(outer_parts, 2, output);
	xp_ca_scrub_memory(normalized, sizeof(normalized));
	xp_ca_scrub_memory(inner, sizeof(inner));
	xp_ca_scrub_memory(ipad, sizeof(ipad));
	xp_ca_scrub_memory(opad, sizeof(opad));
}

static int
pkcs12_sha1_key(const unsigned char *password, size_t password_len,
                const unsigned char *salt, size_t salt_len, uint32_t iterations,
                unsigned diversifier_value, unsigned char *output, size_t output_len) {
	if (iterations == 0 || password_len > (SIZE_MAX / 2) - 1)
		return XP_CRYPTO_ERR_INVALID;
	size_t         unicode_len = (password_len + 1) * 2;
	unsigned char *unicode = calloc(unicode_len, 1);
	if (unicode == NULL)
		return XP_CRYPTO_ERR;
	for (size_t i = 0; i < password_len; i++)
		unicode[2 * i + 1] = password[i];
	const size_t block_size = 64;
	size_t       salt_repeated = salt_len == 0 ? 0
	                       : block_size * ((salt_len + block_size - 1) / block_size);
	size_t       password_repeated = block_size
	                                 * ((unicode_len + block_size - 1) / block_size);
	if (salt_repeated > SIZE_MAX - password_repeated) {
		xp_ca_scrub_memory(unicode, unicode_len);
		free(unicode);
		return XP_CRYPTO_ERR;
	}
	size_t         input_len = salt_repeated + password_repeated;
	unsigned char *input = malloc(input_len);
	if (input == NULL) {
		xp_ca_scrub_memory(unicode, unicode_len);
		free(unicode);
		return XP_CRYPTO_ERR;
	}
	for (size_t i = 0; i < salt_repeated; i++)
		input[i] = salt[i % salt_len];
	for (size_t i = 0; i < password_repeated; i++)
		input[salt_repeated + i] = unicode[i % unicode_len];
	unsigned char diversifier[64], a[20], b[64];
	memset(diversifier, (unsigned char)diversifier_value, sizeof(diversifier));
	for (size_t offset = 0; offset < output_len; offset += sizeof(a)) {
		struct bytes initial[] = {
			{diversifier, sizeof(diversifier)}, {input, input_len},
		};
		sha1_parts(initial, 2, a);
		for (uint32_t i = 1; i < iterations; i++) {
			struct bytes prior = {a, sizeof(a)};
			sha1_parts(&prior, 1, a);
		}
		for (size_t i = 0; i < sizeof(b); i++)
			b[i] = a[i % sizeof(a)];
		for (size_t block = 0; block < input_len; block += sizeof(b)) {
			unsigned carry = 1;
			for (size_t i = sizeof(b); i > 0; i--) {
				unsigned value = input[block + i - 1] + b[i - 1] + carry;
				input[block + i - 1] = (unsigned char)value;
				carry = value >> 8;
			}
		}
		size_t take = output_len - offset;
		if (take > sizeof(a))
			take = sizeof(a);
		memcpy(output + offset, a, take);
	}
	xp_ca_scrub_memory(diversifier, sizeof(diversifier));
	xp_ca_scrub_memory(a, sizeof(a));
	xp_ca_scrub_memory(b, sizeof(b));
	xp_ca_scrub_memory(input, input_len);
	free(input);
	xp_ca_scrub_memory(unicode, unicode_len);
	free(unicode);
	return XP_CRYPTO_OK;
}

static int
decrypt_pkcs12_3des(struct view parameters, const unsigned char *ciphertext,
                    size_t ciphertext_len, const unsigned char *password, size_t password_len,
                    struct bytes *plaintext) {
	if (parameters.tag != 0x30 || ciphertext_len == 0)
		return XP_CRYPTO_ERR_FORMAT;
	const unsigned char *p = parameters.data;
	size_t               left = parameters.len;
	struct view          salt, iteration_value;
	uint32_t             iterations = 0;
	if (take_view(&p, &left, &salt) != XP_CRYPTO_OK || salt.tag != 0x04
	    || take_view(&p, &left, &iteration_value) != XP_CRYPTO_OK
	    || view_uint32(iteration_value, &iterations) != XP_CRYPTO_OK
	    || iterations == 0 || left != 0)
		return XP_CRYPTO_ERR_FORMAT;
	unsigned char key[24], iv[8];
	int           status = pkcs12_sha1_key(password, password_len, salt.data, salt.len,
	                                       iterations, 1, key, sizeof(key));
	if (status == XP_CRYPTO_OK)
		status = pkcs12_sha1_key(password, password_len, salt.data, salt.len,
		                         iterations, 2, iv, sizeof(iv));
	if (status != XP_CRYPTO_OK) {
		xp_ca_scrub_memory(key, sizeof(key));
		xp_ca_scrub_memory(iv, sizeof(iv));
		return status;
	}
	struct xp_cipher_config config = {
		.algorithm = XP_CIPHER_3DES,
		.mode = XP_CIPHER_MODE_CBC,
		.direction = XP_CIPHER_DECRYPT,
		.padding = XP_CIPHER_PADDING_PKCS7,
		.key = key,
		.key_len = sizeof(key),
		.iv = iv,
		.iv_len = sizeof(iv),
	};
	xp_cipher_t             context = NULL;
	status = xp_cipher_create(&context, &config);
	xp_ca_scrub_memory(key, sizeof(key));
	xp_ca_scrub_memory(iv, sizeof(iv));
	if (status != XP_CRYPTO_OK)
		return status;
	plaintext->data = malloc(ciphertext_len + 8);
	if (plaintext->data == NULL) {
		xp_cipher_free(context);
		return XP_CRYPTO_ERR;
	}
	size_t first_len = ciphertext_len + 8;
	status = xp_cipher_update(context, ciphertext, ciphertext_len,
	                          plaintext->data, &first_len);
	size_t final_len = ciphertext_len + 8 - first_len;
	if (status == XP_CRYPTO_OK)
		status = xp_cipher_final(context, plaintext->data + first_len,
		                         &final_len);
	xp_cipher_free(context);
	if (status != XP_CRYPTO_OK) {
		bytes_free(plaintext);
		return XP_CRYPTO_ERR_AUTHORIZATION;
	}
	plaintext->len = first_len + final_len;
	return XP_CRYPTO_OK;
}

static int
decrypt_pbes2(struct view algorithm, const unsigned char *ciphertext,
              size_t ciphertext_len, const unsigned char *password, size_t password_len,
              struct bytes *plaintext) {
	static const unsigned char pbes2_oid[] = {
		0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x05, 0x0d,
	};
	static const unsigned char pkcs12_3des_oid[] = {
		0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x0c, 0x01, 0x03,
	};
	static const unsigned char pbkdf2_oid[] = {
		0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x05, 0x0c,
	};
	static const unsigned char hmac_sha256_oid[] = {
		0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x09,
	};
	static const unsigned char aes128_cbc_oid[] = {
		0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01, 0x02,
	};
	static const unsigned char aes256_cbc_oid[] = {
		0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01, 0x2a,
	};
	if (algorithm.tag != 0x30 || ciphertext_len == 0)
		return XP_CRYPTO_ERR_FORMAT;
	const unsigned char *      p = algorithm.data;
	size_t                     left = algorithm.len;
	struct view                oid, parameters;
	if (take_view(&p, &left, &oid) != XP_CRYPTO_OK
	    || take_view(&p, &left, &parameters) != XP_CRYPTO_OK
	    || parameters.tag != 0x30 || left != 0)
		return XP_CRYPTO_ERR_FORMAT;
	if (oid_equal(oid, pkcs12_3des_oid, sizeof(pkcs12_3des_oid)))
		return decrypt_pkcs12_3des(parameters, ciphertext, ciphertext_len,
		                           password, password_len, plaintext);
	if (!oid_equal(oid, pbes2_oid, sizeof(pbes2_oid)))
		return XP_CRYPTO_ERR_UNSUPPORTED;
	p = parameters.data;
	left = parameters.len;
	struct view kdf, cipher;
	if (take_view(&p, &left, &kdf) != XP_CRYPTO_OK || kdf.tag != 0x30
	    || take_view(&p, &left, &cipher) != XP_CRYPTO_OK
	    || cipher.tag != 0x30 || left != 0)
		return XP_CRYPTO_ERR_FORMAT;

	p = kdf.data;
	left = kdf.len;
	struct view kdf_oid, kdf_parameters;
	if (take_view(&p, &left, &kdf_oid) != XP_CRYPTO_OK
	    || !oid_equal(kdf_oid, pbkdf2_oid, sizeof(pbkdf2_oid))
	    || take_view(&p, &left, &kdf_parameters) != XP_CRYPTO_OK
	    || kdf_parameters.tag != 0x30 || left != 0)
		return XP_CRYPTO_ERR_UNSUPPORTED;
	p = kdf_parameters.data;
	left = kdf_parameters.len;
	struct view salt, iteration_value;
	uint32_t    iterations = 0;
	if (take_view(&p, &left, &salt) != XP_CRYPTO_OK || salt.tag != 0x04
	    || take_view(&p, &left, &iteration_value) != XP_CRYPTO_OK
	    || view_uint32(iteration_value, &iterations) != XP_CRYPTO_OK
	    || iterations == 0)
		return XP_CRYPTO_ERR_FORMAT;
	uint32_t explicit_key_len = 0;
	if (left != 0 && *p == 0x02) {
		struct view key_length;
		if (take_view(&p, &left, &key_length) != XP_CRYPTO_OK
		    || view_uint32(key_length, &explicit_key_len) != XP_CRYPTO_OK)
			return XP_CRYPTO_ERR_FORMAT;
	}
	if (left == 0)
		return XP_CRYPTO_ERR_UNSUPPORTED;
	struct view prf;
	if (take_view(&p, &left, &prf) != XP_CRYPTO_OK || prf.tag != 0x30
	    || left != 0)
		return XP_CRYPTO_ERR_FORMAT;
	p = prf.data;
	left = prf.len;
	struct view prf_oid, prf_parameters;
	if (take_view(&p, &left, &prf_oid) != XP_CRYPTO_OK
	    || !oid_equal(prf_oid, hmac_sha256_oid, sizeof(hmac_sha256_oid)))
		return XP_CRYPTO_ERR_UNSUPPORTED;
	if (left != 0 && (take_view(&p, &left, &prf_parameters) != XP_CRYPTO_OK
	                  || prf_parameters.tag != 0x05 || prf_parameters.len != 0
	                  || left != 0))
		return XP_CRYPTO_ERR_FORMAT;

	p = cipher.data;
	left = cipher.len;
	struct view cipher_oid, iv;
	if (take_view(&p, &left, &cipher_oid) != XP_CRYPTO_OK
	    || take_view(&p, &left, &iv) != XP_CRYPTO_OK || iv.tag != 0x04
	    || iv.len != 16 || left != 0)
		return XP_CRYPTO_ERR_FORMAT;
	size_t key_len;
	if (oid_equal(cipher_oid, aes256_cbc_oid, sizeof(aes256_cbc_oid)))
		key_len = 32;
	else if (oid_equal(cipher_oid, aes128_cbc_oid, sizeof(aes128_cbc_oid)))
		key_len = 16;
	else
		return XP_CRYPTO_ERR_UNSUPPORTED;
	if (explicit_key_len != 0 && explicit_key_len != key_len)
		return XP_CRYPTO_ERR_FORMAT;
	unsigned char           derived_key[32];
	int                     status = xp_kdf_pbkdf2(XP_DIGEST_SHA256, password, password_len,
	                                               salt.data, salt.len, iterations, derived_key, key_len);
	if (status != XP_CRYPTO_OK)
		return status;
	struct xp_cipher_config config = {
		.algorithm = XP_CIPHER_AES,
		.mode = XP_CIPHER_MODE_CBC,
		.direction = XP_CIPHER_DECRYPT,
		.padding = XP_CIPHER_PADDING_PKCS7,
		.key = derived_key,
		.key_len = key_len,
		.iv = iv.data,
		.iv_len = iv.len,
	};
	xp_cipher_t             context = NULL;
	status = xp_cipher_create(&context, &config);
	xp_ca_scrub_memory(derived_key, sizeof(derived_key));
	if (status != XP_CRYPTO_OK)
		return status;
	plaintext->data = malloc(ciphertext_len + 16);
	if (plaintext->data == NULL) {
		xp_cipher_free(context);
		return XP_CRYPTO_ERR;
	}
	size_t first_len = ciphertext_len + 16;
	status = xp_cipher_update(context, ciphertext, ciphertext_len,
	                          plaintext->data, &first_len);
	size_t final_len = ciphertext_len + 16 - first_len;
	if (status == XP_CRYPTO_OK)
		status = xp_cipher_final(context, plaintext->data + first_len,
		                         &final_len);
	xp_cipher_free(context);
	if (status != XP_CRYPTO_OK) {
		bytes_free(plaintext);
		return status == XP_CRYPTO_ERR_VERIFY
		       ? XP_CRYPTO_ERR_AUTHORIZATION : status;
	}
	plaintext->len = first_len + final_len;
	return XP_CRYPTO_OK;
}

static int
decrypt_private_key_info(const unsigned char *encoded, size_t encoded_len,
                         const unsigned char *password, size_t password_len, struct bytes *plaintext) {
	const unsigned char *p = encoded;
	size_t               left = encoded_len;
	struct view          outer, algorithm, ciphertext;
	if (take_view(&p, &left, &outer) != XP_CRYPTO_OK || outer.tag != 0x30
	    || left != 0)
		return XP_CRYPTO_ERR_FORMAT;
	p = outer.data;
	left = outer.len;
	if (take_view(&p, &left, &algorithm) != XP_CRYPTO_OK
	    || algorithm.tag != 0x30
	    || take_view(&p, &left, &ciphertext) != XP_CRYPTO_OK
	    || ciphertext.tag != 0x04 || left != 0)
		return XP_CRYPTO_ERR_FORMAT;
	return decrypt_pbes2(algorithm, ciphertext.data, ciphertext.len,
	                     password, password_len, plaintext);
}

static int
decode_content_info(struct view content_info, const unsigned char *password,
                    size_t password_len, struct bytes *owned, struct view *safe_contents) {
	static const unsigned char data_oid[] = {
		0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01,
	};
	static const unsigned char encrypted_data_oid[] = {
		0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x06,
	};
	const unsigned char *      p = content_info.data;
	size_t                     left = content_info.len;
	struct view                oid, explicit_value;
	if (content_info.tag != 0x30
	    || take_view(&p, &left, &oid) != XP_CRYPTO_OK
	    || take_view(&p, &left, &explicit_value) != XP_CRYPTO_OK
	    || explicit_value.tag != 0xa0 || left != 0)
		return XP_CRYPTO_ERR_FORMAT;
	p = explicit_value.data;
	left = explicit_value.len;
	if (oid_equal(oid, data_oid, sizeof(data_oid))) {
		struct view octets;
		if (take_view(&p, &left, &octets) != XP_CRYPTO_OK
		    || octets.tag != 0x04 || left != 0)
			return XP_CRYPTO_ERR_FORMAT;
		p = octets.data;
		left = octets.len;
	} else if (oid_equal(oid, encrypted_data_oid, sizeof(encrypted_data_oid))) {
		struct view encrypted_data, version, encrypted_content_info;
		uint32_t    version_number = 0;
		if (take_view(&p, &left, &encrypted_data) != XP_CRYPTO_OK
		    || encrypted_data.tag != 0x30 || left != 0)
			return XP_CRYPTO_ERR_FORMAT;
		p = encrypted_data.data;
		left = encrypted_data.len;
		if (take_view(&p, &left, &version) != XP_CRYPTO_OK
		    || view_uint32(version, &version_number) != XP_CRYPTO_OK
		    || version_number != 0
		    || take_view(&p, &left, &encrypted_content_info) != XP_CRYPTO_OK
		    || encrypted_content_info.tag != 0x30)
			return XP_CRYPTO_ERR_FORMAT;
		p = encrypted_content_info.data;
		left = encrypted_content_info.len;
		struct view content_type, algorithm, ciphertext;
		if (take_view(&p, &left, &content_type) != XP_CRYPTO_OK
		    || !oid_equal(content_type, data_oid, sizeof(data_oid))
		    || take_view(&p, &left, &algorithm) != XP_CRYPTO_OK
		    || take_view(&p, &left, &ciphertext) != XP_CRYPTO_OK
		    || ciphertext.tag != 0x80 || left != 0)
			return XP_CRYPTO_ERR_FORMAT;
		int status = decrypt_pbes2(algorithm, ciphertext.data, ciphertext.len,
		                           password, password_len, owned);
		if (status != XP_CRYPTO_OK)
			return status;
		p = owned->data;
		left = owned->len;
	} else
		return XP_CRYPTO_ERR_UNSUPPORTED;
	if (take_view(&p, &left, safe_contents) != XP_CRYPTO_OK
	    || safe_contents->tag != 0x30 || left != 0)
		return XP_CRYPTO_ERR_FORMAT;
	return XP_CRYPTO_OK;
}

static int
pkcs12_import(xp_key_t *key, char **label,
              xp_ca_cert_t **certs, size_t *cert_count, const void *data, size_t len,
              xp_crypto_secret_callback_t password, void *password_context) {
	static const unsigned char data_oid[] = {
		0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01,
	};
	static const unsigned char key_oid[] = {
		0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x0c, 0x0a, 0x01, 0x02,
	};
	static const unsigned char cert_oid[] = {
		0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x0c, 0x0a, 0x01, 0x03,
	};
	if (key == NULL || label == NULL || certs == NULL || cert_count == NULL
	    || data == NULL || len == 0)
		return XP_CRYPTO_ERR_INVALID;
	*key = NULL;
	*label = NULL;
	*certs = NULL;
	*cert_count = 0;
	int                  status = XP_CRYPTO_OK;
	unsigned char *      key_der = NULL;
	size_t               key_len = 0;
	unsigned char *      pfx_password = NULL;
	size_t               pfx_password_len = 0;
	xp_ca_cert_t *       chain = NULL;
	size_t               count = 0;

	const unsigned char *p = data;
	size_t               left = len;
	struct view          pfx, version, content_info;
	if (take_view(&p, &left, &pfx) != XP_CRYPTO_OK || pfx.tag != 0x30
	    || left != 0)
		goto format;
	p = pfx.data;
	left = pfx.len;
	uint32_t version_number = 0;
	if (take_view(&p, &left, &version) != XP_CRYPTO_OK
	    || view_uint32(version, &version_number) != XP_CRYPTO_OK
	    || version_number != 3
	    || take_view(&p, &left, &content_info) != XP_CRYPTO_OK
	    || content_info.tag != 0x30)
		goto format;
	const unsigned char *pfx_tail = p;
	size_t               pfx_tail_len = left;

	p = content_info.data;
	left = content_info.len;
	struct view          oid, explicit_value, authenticated_safe;
	if (take_view(&p, &left, &oid) != XP_CRYPTO_OK
	    || !oid_equal(oid, data_oid, sizeof(data_oid))
	    || take_view(&p, &left, &explicit_value) != XP_CRYPTO_OK
	    || explicit_value.tag != 0xa0 || left != 0)
		goto format;
	p = explicit_value.data;
	left = explicit_value.len;
	if (take_view(&p, &left, &authenticated_safe) != XP_CRYPTO_OK
	    || authenticated_safe.tag != 0x04 || left != 0)
		goto format;
	if (pfx_tail_len != 0) {
		struct view mac_data;
		if (take_view(&pfx_tail, &pfx_tail_len, &mac_data) != XP_CRYPTO_OK
		    || pfx_tail_len != 0)
			goto format;
		status = verify_mac_data(mac_data, authenticated_safe.data,
		                         authenticated_safe.len, password, password_context);
		if (status != XP_CRYPTO_OK)
			goto fail;
	}

	status = keyset_secret(password, password_context, &pfx_password,
	                       &pfx_password_len);
	if (status != XP_CRYPTO_OK)
		goto fail;
	p = authenticated_safe.data;
	left = authenticated_safe.len;
	struct view authenticated_sequence;
	if (take_view(&p, &left, &authenticated_sequence) != XP_CRYPTO_OK
	    || authenticated_sequence.tag != 0x30 || left != 0)
		goto format;
	p = authenticated_sequence.data;
	left = authenticated_sequence.len;
	while (left != 0) {
		struct view  inner_content_info, safe_contents;
		struct bytes owned_safe = {0};
		if (take_view(&p, &left, &inner_content_info) != XP_CRYPTO_OK
		    || inner_content_info.tag != 0x30)
			goto format;
		status = decode_content_info(inner_content_info, pfx_password,
		                             pfx_password_len, &owned_safe, &safe_contents);
		if (status != XP_CRYPTO_OK) {
			bytes_free(&owned_safe);
			goto fail;
		}
		const unsigned char *bag_cursor = safe_contents.data;
		size_t               bags_left = safe_contents.len;
		while (bags_left != 0) {
			struct view bag, bag_oid, bag_value, attributes = {0};
			if (take_view(&bag_cursor, &bags_left, &bag) != XP_CRYPTO_OK
			    || bag.tag != 0x30) {
				bytes_free(&owned_safe);
				goto format;
			}
			const unsigned char *q = bag.data;
			size_t               q_left = bag.len;
			if (take_view(&q, &q_left, &bag_oid) != XP_CRYPTO_OK
			    || take_view(&q, &q_left, &bag_value) != XP_CRYPTO_OK
			    || bag_value.tag != 0xa0) {
				bytes_free(&owned_safe);
				goto format;
			}
			if (q_left != 0) {
				if (take_view(&q, &q_left, &attributes) != XP_CRYPTO_OK
				    || q_left != 0) {
					bytes_free(&owned_safe);
					goto format;
				}
				if (*label == NULL)
					*label = parse_friendly(attributes);
			}
			if (oid_equal(bag_oid, key_oid, sizeof(key_oid))) {
				if (key_der != NULL) {
					bytes_free(&owned_safe);
					goto format;
				}
				key_der = malloc(bag_value.len);
				if (key_der == NULL) {
					bytes_free(&owned_safe);
					goto memory;
				}
				memcpy(key_der, bag_value.data, bag_value.len);
				key_len = bag_value.len;
			} else if (oid_equal(bag_oid, cert_oid, sizeof(cert_oid))) {
				const unsigned char *r = bag_value.data;
				size_t               r_left = bag_value.len;
				struct view          cert_value, cert_type, cert_explicit, cert_octets;
				if (take_view(&r, &r_left, &cert_value) != XP_CRYPTO_OK
				    || cert_value.tag != 0x30 || r_left != 0) {
					bytes_free(&owned_safe);
					goto format;
				}
				r = cert_value.data;
				r_left = cert_value.len;
				if (take_view(&r, &r_left, &cert_type) != XP_CRYPTO_OK
				    || take_view(&r, &r_left, &cert_explicit) != XP_CRYPTO_OK
				    || cert_explicit.tag != 0xa0 || r_left != 0) {
					bytes_free(&owned_safe);
					goto format;
				}
				r = cert_explicit.data;
				r_left = cert_explicit.len;
				if (take_view(&r, &r_left, &cert_octets) != XP_CRYPTO_OK
				    || cert_octets.tag != 0x04 || r_left != 0) {
					bytes_free(&owned_safe);
					goto format;
				}
				xp_ca_cert_t cert = NULL;
				status = xp_ca_cert_import_der(&cert, cert_octets.data,
				                               cert_octets.len);
				if (status != XP_CA_OK) {
					bytes_free(&owned_safe);
					goto fail;
				}
				xp_ca_cert_t *grown = realloc(chain,
				                              (count + 1) * sizeof(*grown));
				if (grown == NULL) {
					xp_ca_cert_free(cert);
					bytes_free(&owned_safe);
					goto memory;
				}
				chain = grown;
				chain[count++] = cert;
			}
		}
		bytes_free(&owned_safe);
	}
	if (*label == NULL) {
		*label = copy_string("default");
		if (*label == NULL)
			goto memory;
	}
	if (key_der != NULL) {
		unsigned char *pem = NULL;
		size_t         pem_len = 0;
		struct bytes   private_info = {0};
		status = decrypt_private_key_info(key_der, key_len, pfx_password,
		                                  pfx_password_len, &private_info);
		xp_ca_scrub_memory(key_der, key_len);
		free(key_der);
		key_der = NULL;
		if (status == XP_CRYPTO_OK)
			status = der_to_pem("PRIVATE KEY", private_info.data, private_info.len,
			                    &pem, &pem_len);
		bytes_free(&private_info);
		if (status == XP_CRYPTO_OK)
			status = xp_key_import_private_pem(key, pem, pem_len, password,
			                                   password_context);
		if (pem != NULL) {
			xp_ca_scrub_memory(pem, pem_len);
			free(pem);
		}
	}
	if (status != XP_CRYPTO_OK)
		goto fail;
	xp_ca_scrub_memory(pfx_password, pfx_password_len);
	free(pfx_password);
	pfx_password = NULL;
	*certs = chain;
	*cert_count = count;
	return XP_CRYPTO_OK;

format:
	status = XP_CA_ERR_FORMAT;
	goto fail;
memory:
	status = XP_CRYPTO_ERR;
fail:
	if (key_der != NULL) {
		xp_ca_scrub_memory(key_der, key_len);
		free(key_der);
	}
	if (pfx_password != NULL) {
		xp_ca_scrub_memory(pfx_password, pfx_password_len);
		free(pfx_password);
	}
	free(*label);
	*label = NULL;
	xp_ca_cert_chain_free(chain, count);
	return status;
}
