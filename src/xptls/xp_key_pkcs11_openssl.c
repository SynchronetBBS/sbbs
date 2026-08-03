#include "xp_key_internal.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/asn1.h>
#include <openssl/crypto.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/provider.h>
#include <openssl/rand.h>
#include <openssl/store.h>
#include <openssl/ui.h>

#if defined(XPTLS_HAS_P11KIT)
#include <p11-kit/p11-kit.h>
#include <p11-kit/pkcs11.h>
#endif

static enum xp_key_store_kind
configured_kind(const struct xp_key_store_config *store)
{
	if (store != NULL && store->store != NULL
	    && strcmp(store->store, "pkcs11") == 0)
		return XP_KEY_STORE_PKCS11;
	if (store != NULL && store->store != NULL
	    && strcmp(store->store, "tpm2") == 0)
		return XP_KEY_STORE_TPM2;
	return XP_KEY_STORE_PLATFORM;
}

#if defined(XPTLS_HAS_P11KIT)

struct p11_context {
	CK_FUNCTION_LIST *module;
	CK_SLOT_ID slot;
	CK_SESSION_HANDLE session;
	char serial[17];
};

static char *
uri_value(const char *uri, const char *name)
{
	if (uri == NULL)
		return NULL;
	size_t name_len = strlen(name);
	for (const char *p = uri; (p = strstr(p, name)) != NULL; p++) {
		if ((p == uri || p[-1] == ':' || p[-1] == ';' || p[-1] == '?'
		    || p[-1] == '&') && p[name_len] == '=') {
			const char *begin = p + name_len + 1;
			const char *end = begin;
			while (*end != 0 && *end != ';' && *end != '&') end++;
			size_t len = (size_t)(end - begin);
			char *value = malloc(len + 1);
			if (value != NULL) {
				memcpy(value, begin, len);
				value[len] = 0;
			}
			return value;
		}
	}
	return NULL;
}

static void
trim_token_field(char *out, size_t out_size,
	const unsigned char *value, size_t value_len)
{
	while (value_len != 0 && (value[value_len - 1] == ' '
	    || value[value_len - 1] == 0))
		value_len--;
	if (value_len >= out_size)
		value_len = out_size - 1;
	memcpy(out, value, value_len);
	out[value_len] = 0;
}

static int
pkcs11_status(CK_RV value)
{
	switch (value) {
		case CKR_OK: return XP_CRYPTO_OK;
		case CKR_PIN_INCORRECT:
		case CKR_PIN_INVALID:
		case CKR_PIN_EXPIRED:
		case CKR_USER_NOT_LOGGED_IN: return XP_CRYPTO_ERR_AUTHORIZATION;
		case CKR_TOKEN_NOT_PRESENT:
		case CKR_DEVICE_REMOVED: return XP_CRYPTO_ERR_NOT_FOUND;
		case CKR_SESSION_READ_ONLY: return XP_CRYPTO_ERR_READ_ONLY;
		case CKR_MECHANISM_INVALID:
		case CKR_MECHANISM_PARAM_INVALID:
		case CKR_KEY_TYPE_INCONSISTENT:
		case CKR_TEMPLATE_INCONSISTENT: return XP_CRYPTO_ERR_UNSUPPORTED;
		case CKR_SESSION_COUNT:
		case CKR_DEVICE_MEMORY: return XP_CRYPTO_ERR_BUSY;
		default: return XP_CRYPTO_ERR;
	}
}

static void
p11_close(struct p11_context *context)
{
	if (context->module != NULL && context->session != CK_INVALID_HANDLE) {
		context->module->C_Logout(context->session);
		context->module->C_CloseSession(context->session);
	}
	if (context->module != NULL) {
		p11_kit_module_finalize(context->module);
		p11_kit_module_release(context->module);
	}
	memset(context, 0, sizeof(*context));
	context->session = CK_INVALID_HANDLE;
}

static int
authorization_value(const struct xp_key_store_config *store,
	unsigned char **out, size_t *out_len)
{
	*out = NULL;
	*out_len = 0;
	if (store->authorize == NULL)
		return XP_CRYPTO_ERR_AUTHORIZATION;
	size_t len = 0;
	if (store->authorize(store->authorize_context, NULL, 0, &len) != 0)
		return XP_CRYPTO_ERR_AUTHORIZATION;
	unsigned char *value = malloc(len + 1);
	if (value == NULL)
		return XP_CRYPTO_ERR;
	size_t actual = len;
	if (store->authorize(store->authorize_context, value, len, &actual) != 0
	    || actual != len) {
		OPENSSL_cleanse(value, len + 1);
		free(value);
		return XP_CRYPTO_ERR_AUTHORIZATION;
	}
	value[len] = 0;
	*out = value;
	*out_len = len;
	return XP_CRYPTO_OK;
}

static int
p11_open(struct p11_context *context,
	const struct xp_key_store_config *store, bool login, bool write,
	const char *required_serial)
{
	memset(context, 0, sizeof(*context));
	context->session = CK_INVALID_HANDLE;
	if (store == NULL || store->store_uri == NULL
	    || strstr(store->store_uri, "pin-value=") != NULL)
		return XP_CRYPTO_ERR_INVALID;
	char *module_path = uri_value(store->store_uri, "module-path");
	char *slot_value = uri_value(store->store_uri, "slot-id");
	char *token_value = uri_value(store->store_uri, "token");
	if (module_path == NULL) {
		free(slot_value); free(token_value);
		return XP_CRYPTO_ERR_INVALID;
	}
	context->module = p11_kit_module_load(module_path, 0);
	free(module_path);
	if (context->module == NULL) {
		free(slot_value); free(token_value);
		return XP_CRYPTO_ERR_UNAVAILABLE;
	}
	CK_RV rv = p11_kit_module_initialize(context->module);
	if (rv != CKR_OK) {
		free(slot_value); free(token_value);
		p11_close(context);
		return pkcs11_status(rv);
	}
	CK_ULONG count = 0;
	rv = context->module->C_GetSlotList(CK_TRUE, NULL, &count);
	CK_SLOT_ID *slots = count == 0 ? NULL : malloc(sizeof(*slots) * count);
	if (rv == CKR_OK && count != 0 && slots == NULL)
		rv = CKR_HOST_MEMORY;
	if (rv == CKR_OK)
		rv = context->module->C_GetSlotList(CK_TRUE, slots, &count);
	bool found = false;
	unsigned long configured_slot = ULONG_MAX;
	if (slot_value != NULL) {
		char *end = NULL;
		errno = 0;
		configured_slot = strtoul(slot_value, &end, 10);
		if (errno != 0 || end == slot_value || *end != 0)
			rv = CKR_SLOT_ID_INVALID;
	}
	for (CK_ULONG i = 0; rv == CKR_OK && i < count; i++) {
		if (configured_slot != ULONG_MAX && slots[i] != configured_slot)
			continue;
		CK_TOKEN_INFO info;
		rv = context->module->C_GetTokenInfo(slots[i], &info);
		if (rv != CKR_OK)
			break;
		char serial[17], label[33];
		trim_token_field(serial, sizeof(serial), info.serialNumber,
			sizeof(info.serialNumber));
		trim_token_field(label, sizeof(label), info.label, sizeof(info.label));
		if ((required_serial != NULL && strcmp(required_serial, serial) != 0)
		    || (token_value != NULL && strcmp(token_value, label) != 0))
			continue;
		context->slot = slots[i];
		strncpy(context->serial, serial, sizeof(context->serial) - 1);
		found = true;
		break;
	}
	free(slots); free(slot_value); free(token_value);
	if (rv != CKR_OK || !found) {
		p11_close(context);
		return rv == CKR_OK ? XP_CRYPTO_ERR_NOT_FOUND : pkcs11_status(rv);
	}
	CK_FLAGS flags = CKF_SERIAL_SESSION | (write ? CKF_RW_SESSION : 0);
	rv = context->module->C_OpenSession(context->slot, flags, NULL, NULL,
		&context->session);
	if (rv == CKR_OK && login) {
		unsigned char *pin = NULL;
		size_t pin_len = 0;
		int status = authorization_value(store, &pin, &pin_len);
		if (status != XP_CRYPTO_OK) {
			p11_close(context);
			return status;
		}
		rv = context->module->C_Login(context->session, CKU_USER, pin, pin_len);
		OPENSSL_cleanse(pin, pin_len + 1);
		free(pin);
		if (rv == CKR_USER_ALREADY_LOGGED_IN)
			rv = CKR_OK;
	}
	if (rv != CKR_OK) {
		p11_close(context);
		return pkcs11_status(rv);
	}
	return XP_CRYPTO_OK;
}

static bool
mechanism_available(struct p11_context *context, CK_MECHANISM_TYPE required)
{
	CK_ULONG count = 0;
	if (context->module->C_GetMechanismList(context->slot, NULL, &count) != CKR_OK)
		return false;
	CK_MECHANISM_TYPE *values = count == 0 ? NULL : malloc(sizeof(*values) * count);
	if (count != 0 && values == NULL)
		return false;
	bool found = context->module->C_GetMechanismList(
		context->slot, values, &count) == CKR_OK;
	if (found) {
		found = false;
		for (CK_ULONG i = 0; i < count; i++)
			if (values[i] == required) { found = true; break; }
	}
	free(values);
	return found;
}

static OSSL_PROVIDER *pkcs11_provider;
static CRYPTO_ONCE provider_once = CRYPTO_ONCE_STATIC_INIT;

static void
load_pkcs11_provider(void)
{
	pkcs11_provider = OSSL_PROVIDER_load(NULL, "pkcs11");
}

static bool
provider_available(void)
{
	OSSL_STORE_LOADER *loader = OSSL_STORE_LOADER_fetch(NULL, "pkcs11", NULL);
	if (loader == NULL) {
		CRYPTO_THREAD_run_once(&provider_once, load_pkcs11_provider);
		loader = OSSL_STORE_LOADER_fetch(NULL, "pkcs11", NULL);
	}
	if (loader == NULL)
		return false;
	OSSL_STORE_LOADER_free(loader);
	return true;
}

static int
hex_value(char value)
{
	if (value >= '0' && value <= '9') return value - '0';
	if (value >= 'a' && value <= 'f') return value - 'a' + 10;
	if (value >= 'A' && value <= 'F') return value - 'A' + 10;
	return -1;
}

static bool
parse_locator(const void *locator, size_t locator_len,
	char **serial, unsigned char **id, size_t *id_len, int *algorithm)
{
	*serial = NULL; *id = NULL; *id_len = 0; *algorithm = 0;
	if (locator == NULL || locator_len == 0
	    || memchr(locator, 0, locator_len) != NULL)
		return false;
	char *copy = malloc(locator_len + 1);
	if (copy == NULL)
		return false;
	memcpy(copy, locator, locator_len);
	copy[locator_len] = 0;
	if (strncmp(copy, "pkcs11:", 7) != 0) {
		free(copy); return false;
	}
	char *encoded = uri_value(copy, "id");
	char *alg = uri_value(copy, "alg");
	*serial = uri_value(copy, "serial");
	free(copy);
	if (encoded == NULL || alg == NULL || *serial == NULL
	    || strlen(encoded) == 0 || strlen(encoded) % 2 != 0) {
		free(encoded); free(alg); free(*serial); *serial = NULL;
		return false;
	}
	*id_len = strlen(encoded) / 2;
	*id = malloc(*id_len);
	if (*id == NULL) {
		free(encoded); free(alg); free(*serial); *serial = NULL;
		return false;
	}
	for (size_t i = 0; i < *id_len; i++) {
		int high = hex_value(encoded[i * 2]);
		int low = hex_value(encoded[i * 2 + 1]);
		if (high < 0 || low < 0) {
			free(encoded); free(alg); free(*serial); free(*id);
			*serial = NULL; *id = NULL; return false;
		}
		(*id)[i] = (unsigned char)((high << 4) | low);
	}
	*algorithm = strcmp(alg, "rsa") == 0 ? XP_KEY_RSA
		: strcmp(alg, "ecdsa") == 0 ? XP_KEY_ECDSA : 0;
	free(encoded); free(alg);
	return *algorithm != 0;
}

static char *
percent_encode(const unsigned char *value, size_t len)
{
	static const char hex[] = "0123456789ABCDEF";
	char *result = malloc(len * 3 + 1);
	if (result == NULL)
		return NULL;
	for (size_t i = 0; i < len; i++) {
		result[i * 3] = '%';
		result[i * 3 + 1] = hex[value[i] >> 4];
		result[i * 3 + 2] = hex[value[i] & 15];
	}
	result[len * 3] = 0;
	return result;
}

static int
ui_read(UI *ui, UI_STRING *string)
{
	enum UI_string_types type = UI_get_string_type(string);
	if (type == UIT_INFO || type == UIT_ERROR)
		return 1;
	if (type != UIT_PROMPT && type != UIT_VERIFY)
		return 0;
	const struct xp_key_store_config *store = UI_get0_user_data(ui);
	unsigned char *secret = NULL;
	size_t secret_len = 0;
	if (store == NULL || authorization_value(store, &secret, &secret_len)
	    != XP_CRYPTO_OK || secret_len > INT_MAX) {
		if (secret != NULL) {
			OPENSSL_cleanse(secret, secret_len + 1); free(secret);
		}
		return 0;
	}
	int ok = UI_set_result_ex(ui, string, (const char *)secret,
		(int)secret_len) >= 0;
	OPENSSL_cleanse(secret, secret_len + 1);
	free(secret);
	return ok;
}

static EVP_PKEY *
load_provider_key(const struct xp_key_store_config *store,
	const char *serial, const unsigned char *id, size_t id_len)
{
	char *module_path = uri_value(store->store_uri, "module-path");
	char *serial_encoded = percent_encode((const unsigned char *)serial, strlen(serial));
	char *id_encoded = percent_encode(id, id_len);
	char *module_encoded = module_path == NULL ? NULL
		: percent_encode((const unsigned char *)module_path, strlen(module_path));
	free(module_path);
	if (serial_encoded == NULL || id_encoded == NULL || module_encoded == NULL) {
		free(serial_encoded); free(id_encoded); free(module_encoded);
		return NULL;
	}
	size_t uri_len = strlen(serial_encoded) + strlen(id_encoded)
		+ strlen(module_encoded) + 64;
	char *uri = malloc(uri_len);
	if (uri == NULL) {
		free(serial_encoded); free(id_encoded); free(module_encoded);
		return NULL;
	}
	snprintf(uri, uri_len, "pkcs11:serial=%s;id=%s;type=private?module-path=%s",
		serial_encoded, id_encoded, module_encoded);
	free(serial_encoded); free(id_encoded); free(module_encoded);
	UI_METHOD *method = UI_create_method("xptls PKCS11 authorization");
	if (method == NULL || UI_method_set_reader(method, ui_read) != 1) {
		UI_destroy_method(method); free(uri); return NULL;
	}
	OSSL_STORE_CTX *context = OSSL_STORE_open(uri, method, (void *)store, NULL, NULL);
	free(uri);
	EVP_PKEY *key = NULL;
	while (context != NULL && !OSSL_STORE_eof(context) && key == NULL) {
		OSSL_STORE_INFO *info = OSSL_STORE_load(context);
		if (info == NULL)
			break;
		if (OSSL_STORE_INFO_get_type(info) == OSSL_STORE_INFO_PKEY)
			key = OSSL_STORE_INFO_get1_PKEY(info);
		OSSL_STORE_INFO_free(info);
	}
	if (context != NULL)
		OSSL_STORE_close(context);
	UI_destroy_method(method);
	return key;
}

static int
generate_objects(struct p11_context *context, const struct xp_key_spec *spec,
	const unsigned char *id, size_t id_len, CK_OBJECT_HANDLE *public_key,
	CK_OBJECT_HANDLE *private_key)
{
	CK_BBOOL yes = CK_TRUE, no = CK_FALSE;
	char label[64];
	static const char digits[] = "0123456789abcdef";
	memcpy(label, "xptls-", 6);
	for (size_t i = 0; i < id_len && 6 + i * 2 + 1 < sizeof(label); i++) {
		label[6 + i * 2] = digits[id[i] >> 4];
		label[7 + i * 2] = digits[id[i] & 15];
	}
	size_t label_len = 6 + id_len * 2;
	CK_KEY_TYPE key_type = spec->algorithm == XP_KEY_RSA ? CKK_RSA : CKK_EC;
	CK_ATTRIBUTE public_template[10];
	CK_ULONG public_count = 0;
#define ADD_PUBLIC(type, value, length) do { \
	public_template[public_count++] = (CK_ATTRIBUTE){type, (void *)(value), (length)}; \
} while (0)
	ADD_PUBLIC(CKA_TOKEN, &yes, sizeof(yes));
	ADD_PUBLIC(CKA_PRIVATE, &no, sizeof(no));
	ADD_PUBLIC(CKA_LABEL, label, label_len);
	ADD_PUBLIC(CKA_ID, id, id_len);
	ADD_PUBLIC(CKA_KEY_TYPE, &key_type, sizeof(key_type));
	ADD_PUBLIC(CKA_VERIFY, &yes, sizeof(yes));
	ADD_PUBLIC(CKA_DESTROYABLE, &yes, sizeof(yes));
	CK_MECHANISM mechanism;
	unsigned char exponent[] = {1, 0, 1};
	CK_ULONG bits = spec->bits;
	unsigned char *ec_parameters = NULL;
	int ec_parameters_len = 0;
	if (spec->algorithm == XP_KEY_RSA) {
		mechanism = (CK_MECHANISM){CKM_RSA_PKCS_KEY_PAIR_GEN, NULL, 0};
		ADD_PUBLIC(CKA_MODULUS_BITS, &bits, sizeof(bits));
		ADD_PUBLIC(CKA_PUBLIC_EXPONENT, exponent, sizeof(exponent));
	}
	else {
		int nid = spec->curve == XP_KEY_CURVE_P256 ? NID_X9_62_prime256v1
			: spec->curve == XP_KEY_CURVE_P384 ? NID_secp384r1 : NID_secp521r1;
		ASN1_OBJECT *object = OBJ_nid2obj(nid);
		ec_parameters_len = i2d_ASN1_OBJECT(object, &ec_parameters);
		if (ec_parameters_len <= 0)
			return XP_CRYPTO_ERR;
		mechanism = (CK_MECHANISM){CKM_EC_KEY_PAIR_GEN, NULL, 0};
		ADD_PUBLIC(CKA_EC_PARAMS, ec_parameters, (CK_ULONG)ec_parameters_len);
	}
	CK_ATTRIBUTE private_template[] = {
		{CKA_TOKEN, &yes, sizeof(yes)},
		{CKA_PRIVATE, &yes, sizeof(yes)},
		{CKA_LABEL, label, label_len},
		{CKA_ID, (void *)id, id_len},
		{CKA_KEY_TYPE, &key_type, sizeof(key_type)},
		{CKA_SENSITIVE, &yes, sizeof(yes)},
		{CKA_EXTRACTABLE, &no, sizeof(no)},
		{CKA_SIGN, &yes, sizeof(yes)},
		{CKA_DESTROYABLE, &yes, sizeof(yes)},
	};
	CK_RV rv = context->module->C_GenerateKeyPair(context->session, &mechanism,
		public_template, public_count, private_template,
		sizeof(private_template) / sizeof(private_template[0]),
		public_key, private_key);
	OPENSSL_free(ec_parameters);
	return pkcs11_status(rv);
#undef ADD_PUBLIC
}

#endif

int
xp_key_provider_store_query(const struct xp_key_store_config *store,
	const struct xp_key_spec *spec, struct xp_key_store_capabilities *capabilities)
{
	if (store == NULL || spec == NULL || capabilities == NULL)
		return XP_CRYPTO_ERR_INVALID;
	memset(capabilities, 0, sizeof(*capabilities));
	capabilities->kind = configured_kind(store);
	capabilities->availability_status = XP_CRYPTO_ERR_UNAVAILABLE;
#if defined(XPTLS_HAS_P11KIT)
	if (capabilities->kind != XP_KEY_STORE_PKCS11 || !provider_available())
		return XP_CRYPTO_OK;
	struct p11_context context;
	int status = p11_open(&context, store, false, false, NULL);
	if (status != XP_CRYPTO_OK) {
		capabilities->availability_status = status;
		return XP_CRYPTO_OK;
	}
	CK_MECHANISM_TYPE generate = spec->algorithm == XP_KEY_RSA
		? CKM_RSA_PKCS_KEY_PAIR_GEN : spec->algorithm == XP_KEY_ECDSA
		? CKM_EC_KEY_PAIR_GEN : 0;
	CK_MECHANISM_TYPE sign = spec->algorithm == XP_KEY_RSA
		? CKM_RSA_PKCS : spec->algorithm == XP_KEY_ECDSA ? CKM_ECDSA : 0;
	if (generate == 0 || !mechanism_available(&context, generate)
	    || !mechanism_available(&context, sign))
		capabilities->availability_status = XP_CRYPTO_ERR_UNSUPPORTED;
	else {
		capabilities->available = true;
		capabilities->availability_status = XP_CRYPTO_OK;
		capabilities->operations = XP_KEY_STORE_CAN_GENERATE
			| XP_KEY_STORE_CAN_OPEN | XP_KEY_STORE_CAN_SIGN
			| XP_KEY_STORE_CAN_DESTROY;
	}
	p11_close(&context);
#endif
	return XP_CRYPTO_OK;
}

int
xp_key_provider_generate_stored(xp_key_t *out, void *locator,
	size_t *locator_len, const struct xp_key_store_config *store,
	const struct xp_key_spec *spec)
{
	if (out != NULL) *out = NULL;
#if !defined(XPTLS_HAS_P11KIT)
	(void)locator; (void)locator_len; (void)store; (void)spec;
	return XP_CRYPTO_ERR_UNAVAILABLE;
#else
	if (out == NULL || locator_len == NULL || store == NULL || spec == NULL
	    || configured_kind(store) != XP_KEY_STORE_PKCS11)
		return XP_CRYPTO_ERR_INVALID;
	struct p11_context context;
	int status = p11_open(&context, store, true, true, NULL);
	if (status != XP_CRYPTO_OK)
		return status;
	unsigned char id[20];
	if (RAND_bytes(id, sizeof(id)) != 1) {
		p11_close(&context); return XP_CRYPTO_ERR;
	}
	char encoded_id[sizeof(id) * 2 + 1];
	static const char digits[] = "0123456789abcdef";
	for (size_t i = 0; i < sizeof(id); i++) {
		encoded_id[i * 2] = digits[id[i] >> 4];
		encoded_id[i * 2 + 1] = digits[id[i] & 15];
	}
	encoded_id[sizeof(id) * 2] = 0;
	char value[160];
	int value_len = snprintf(value, sizeof(value),
		"pkcs11:serial=%s;id=%s;alg=%s", context.serial, encoded_id,
		spec->algorithm == XP_KEY_RSA ? "rsa" : "ecdsa");
	if (value_len <= 0 || (size_t)value_len >= sizeof(value)) {
		p11_close(&context); return XP_CRYPTO_ERR;
	}
	if (locator == NULL || *locator_len < (size_t)value_len) {
		*locator_len = (size_t)value_len;
		p11_close(&context);
		return XP_CRYPTO_ERR_BUFFER_TOO_SMALL;
	}
	CK_OBJECT_HANDLE public_key = CK_INVALID_HANDLE, private_key = CK_INVALID_HANDLE;
	status = generate_objects(&context, spec, id, sizeof(id),
		&public_key, &private_key);
	if (status != XP_CRYPTO_OK) {
		p11_close(&context);
		return status;
	}
	EVP_PKEY *native = load_provider_key(store, context.serial, id, sizeof(id));
	if (native == NULL) {
		context.module->C_DestroyObject(context.session, private_key);
		context.module->C_DestroyObject(context.session, public_key);
		p11_close(&context);
		return XP_CRYPTO_ERR_UNAVAILABLE;
	}
	p11_close(&context);
	*out = xp_key_wrap_native_private(native, XP_KEY_STORE_PKCS11, false);
	if (*out == NULL) {
		EVP_PKEY_free(native); return XP_CRYPTO_ERR;
	}
	memcpy(locator, value, (size_t)value_len);
	*locator_len = (size_t)value_len;
	return XP_CRYPTO_OK;
#endif
}

int
xp_key_provider_import_stored(xp_key_t *out, void *locator,
	size_t *locator_len, const struct xp_key_store_config *store, xp_key_t source)
{
	if (out != NULL) *out = NULL;
	(void)locator; (void)locator_len; (void)store; (void)source;
	return XP_CRYPTO_ERR_UNSUPPORTED;
}

int
xp_key_provider_open_stored(xp_key_t *out,
	const struct xp_key_store_config *store, const void *locator, size_t locator_len)
{
	if (out != NULL) *out = NULL;
#if !defined(XPTLS_HAS_P11KIT)
	(void)store; (void)locator; (void)locator_len;
	return XP_CRYPTO_ERR_UNAVAILABLE;
#else
	if (out == NULL || store == NULL || configured_kind(store) != XP_KEY_STORE_PKCS11)
		return XP_CRYPTO_ERR_INVALID;
	char *serial = NULL;
	unsigned char *id = NULL;
	size_t id_len = 0;
	int algorithm = 0;
	if (!parse_locator(locator, locator_len, &serial, &id, &id_len, &algorithm))
		return XP_CRYPTO_ERR_FORMAT;
	(void)algorithm;
	struct p11_context context;
	int status = p11_open(&context, store, true, false, serial);
	if (status == XP_CRYPTO_OK) {
		EVP_PKEY *native = load_provider_key(store, serial, id, id_len);
		if (native == NULL)
			status = XP_CRYPTO_ERR_NOT_FOUND;
		else {
			*out = xp_key_wrap_native_private(native, XP_KEY_STORE_PKCS11, false);
			if (*out == NULL) { EVP_PKEY_free(native); status = XP_CRYPTO_ERR; }
		}
	}
	p11_close(&context);
	free(serial); free(id);
	return status;
#endif
}

int
xp_key_provider_destroy_stored(const struct xp_key_store_config *store,
	const void *locator, size_t locator_len,
	const void *expected_fingerprint, size_t fingerprint_len)
{
	if (expected_fingerprint == NULL || fingerprint_len != 32)
		return XP_CRYPTO_ERR_INVALID;
#if !defined(XPTLS_HAS_P11KIT)
	(void)store; (void)locator; (void)locator_len;
	return XP_CRYPTO_ERR_UNAVAILABLE;
#else
	xp_key_t key = NULL;
	int status = xp_key_provider_open_stored(&key, store, locator, locator_len);
	unsigned char actual[32];
	size_t actual_len = sizeof(actual);
	if (status == XP_CRYPTO_OK)
		status = xp_key_fingerprint_sha256(key, actual, &actual_len);
	if (status == XP_CRYPTO_OK
	    && CRYPTO_memcmp(actual, expected_fingerprint, sizeof(actual)) != 0)
		status = XP_CRYPTO_ERR_CONFLICT;
	xp_key_release(key);
	if (status != XP_CRYPTO_OK) {
		OPENSSL_cleanse(actual, sizeof(actual)); return status;
	}
	char *serial = NULL;
	unsigned char *id = NULL;
	size_t id_len = 0;
	int algorithm = 0;
	if (!parse_locator(locator, locator_len, &serial, &id, &id_len, &algorithm))
		return XP_CRYPTO_ERR_FORMAT;
	struct p11_context context;
	status = p11_open(&context, store, true, true, serial);
	if (status == XP_CRYPTO_OK) {
		CK_OBJECT_CLASS classes[] = {CKO_PRIVATE_KEY, CKO_PUBLIC_KEY};
		for (size_t i = 0; status == XP_CRYPTO_OK && i < 2; i++) {
			CK_ATTRIBUTE attributes[] = {
				{CKA_CLASS, &classes[i], sizeof(classes[i])},
				{CKA_ID, id, id_len},
			};
			CK_RV rv = context.module->C_FindObjectsInit(context.session,
				attributes, sizeof(attributes) / sizeof(attributes[0]));
			CK_OBJECT_HANDLE object = CK_INVALID_HANDLE;
			CK_ULONG count = 0;
			if (rv == CKR_OK)
				rv = context.module->C_FindObjects(context.session, &object, 1, &count);
			if (rv == CKR_OK)
				rv = context.module->C_FindObjectsFinal(context.session);
			if (rv != CKR_OK)
				status = pkcs11_status(rv);
			else if (count == 1
			    && context.module->C_DestroyObject(context.session, object) != CKR_OK)
				status = XP_CRYPTO_ERR;
		}
	}
	p11_close(&context);
	OPENSSL_cleanse(actual, sizeof(actual));
	free(serial); free(id);
	return status;
#endif
}
