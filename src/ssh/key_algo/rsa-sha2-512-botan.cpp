/*
 * key_algo/rsa-sha2-512-botan.cpp -- RSA-SHA2-512 host key algorithm via Botan3 native C++ API.
 */

#include <botan/bigint.h>
#include <botan/pk_keys.h>
#include <botan/pkcs8.h>
#include <botan/pubkey.h>
#include <botan/rsa.h>
#include <botan/system_rng.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "deucessh-algorithms.h"
#include "deucessh-crypto.h"
#include "deucessh.h"
#include "private-key-file.h"
#include "client-key.h"
#include "key-context-botan.h"
#include "key-wire.h"
#include "key-file-common.h"
#include "key-text.h"
#define DSSH_RSA_CACHE_BOTAN
#include "key-rsa-cache.h"
#undef DSSH_RSA_CACHE_BOTAN

struct dssh_key_algo_ctx;

#define RSA_SHA2_512_NAME     "rsa-sha2-512"
#define RSA_SHA2_512_NAME_LEN 12
#define RSA_KEY_TYPE_NAME     "ssh-rsa"
#define RSA_KEY_TYPE_NAME_LEN 7
/* See rsa-sha2-256-botan.cpp for why we use the PKCS1v15 spelling
   instead of the deprecated EMSA_PKCS1 name. */
#define RSA_SIGN_PADDING      "PKCS1v15(SHA-512)"


static struct dssh_botan_key_context *rsa_ctx;

static int
sign_impl(struct dssh_bytes_result *out, const uint8_t *data, size_t data_len, struct dssh_botan_key_context *cbd)
{
	if (out == NULL || (data == NULL && data_len > 0)
	    || out->bytes.data != NULL || out->bytes.len != 0
	    || out->lease.release != NULL || out->lease.owner != NULL)
		return DSSH_ERROR_INVALID;
	if (cbd == NULL || cbd->keys == nullptr || cbd->kind != DSSH_BOTAN_KEY_RSA)
		return DSSH_ERROR_INIT;

	try {
		dssh_botan_key_guard key;
		int acquired_key = key.acquire(cbd->keys);
		if (acquired_key < 0)
			return acquired_key;
		Botan::PK_Signer signer(*key.get(), Botan::system_rng(), RSA_SIGN_PADDING);
		signer.update(data, data_len);
		auto raw_sig = signer.signature(Botan::system_rng());

		if (raw_sig.empty())
			return DSSH_ERROR_INIT;
		if (raw_sig.size() > UINT32_MAX)
			return DSSH_ERROR_INVALID;
		if (raw_sig.size() > SIZE_MAX - 20)
			return DSSH_ERROR_INVALID;
		size_t needed = 20 + raw_sig.size();

		uint8_t *buf = NULL;
		int acquired = dssh_signature_buffer_acquire(cbd->signatures, needed, out, &buf);
		if (acquired < 0)
			return acquired;

		size_t pos  = 0;
		int    sret = dssh_serialize_uint32(RSA_SHA2_512_NAME_LEN, buf, needed, &pos);
		if (sret < 0) {
			return sret;
		}
		memcpy(&buf[pos], RSA_SHA2_512_NAME, RSA_SHA2_512_NAME_LEN);
		pos += RSA_SHA2_512_NAME_LEN;
		uint32_t siglen_u32 = static_cast<uint32_t>(raw_sig.size());
		sret                = dssh_serialize_uint32(siglen_u32, buf, needed, &pos);
		if (sret < 0) {
			return sret;
		}
		memcpy(&buf[pos], raw_sig.data(), raw_sig.size());
		pos += raw_sig.size();

		out->bytes.len = pos;
		return 0;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

static int
pubkey_impl(const uint8_t **out, size_t *outlen, struct dssh_botan_key_context *cbd)
{
	if (cbd == NULL || (cbd->privkey == nullptr && cbd->keys == nullptr))
		return DSSH_ERROR_INIT;
	if (cbd->pub_blob != NULL) {
		*out = cbd->pub_blob;
		*outlen = cbd->pub_blob_len;
		return 0;
	}
	try {
		auto *rsa = dynamic_cast<Botan::RSA_PrivateKey *>(cbd->privkey.get());
		if (rsa == NULL)
			return DSSH_ERROR_INIT;
		const Botan::BigInt &e = rsa->get_e();
		const Botan::BigInt &n = rsa->get_n();
		struct dssh_rsa_cache_number exponent, modulus;
		struct dssh_rsa_cache_layout layout;
		int ret = dssh_rsa_cache_native_number(e, &exponent);
		if (ret < 0)
			return ret;
		ret = dssh_rsa_cache_native_number(n, &modulus);
		if (ret < 0)
			return ret;
		ret = dssh_rsa_cache_size_native(exponent, modulus, &layout);
		if (ret < 0)
			return ret;
		std::unique_ptr<uint8_t, dssh_rsa_cache_deleter> buf(dssh_rsa_cache_allocate(layout.length));
		if (buf == nullptr)
			return DSSH_ERROR_ALLOC;
		uint8_t *e_slot, *n_slot;
		ret = dssh_rsa_cache_prefix(buf.get(), &layout, &e_slot, &n_slot);
		if (ret < 0)
			return ret;
		e.serialize_to(std::span<uint8_t>(e_slot, exponent.bytes));
		n.serialize_to(std::span<uint8_t>(n_slot, modulus.bytes));
		cbd->pub_blob = buf.release();
		cbd->pub_blob_len = layout.length;
		*out = cbd->pub_blob;
		*outlen = layout.length;
		return 0;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

/* --- extern "C" wrappers for function pointer vtable --- */

extern "C" int
dssh_botan_rsa512_sign(struct dssh_bytes_result *out, const uint8_t *data, size_t data_len,
    dssh_key_algo_ctx *ctx)
{
	try {
		return sign_impl(out, data, data_len, reinterpret_cast<struct dssh_botan_key_context *>(ctx));
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

extern "C" int
dssh_botan_rsa512_verify(const uint8_t *key_blob, size_t key_blob_len, const uint8_t *sig_blob,
    size_t sig_blob_len, const uint8_t *data, size_t data_len)
{
	struct dssh_rsa_wire_key key;
	struct dssh_bytes signature;
	int ret;
	if (data == NULL && data_len != 0)
		return DSSH_ERROR_INVALID;
	ret = dssh_key_wire_rsa(key_blob, key_blob_len, &key);
	if (ret < 0)
		return ret;
	ret = dssh_key_wire_blob(sig_blob, sig_blob_len, RSA_SHA2_512_NAME,
	    RSA_SHA2_512_NAME_LEN, &signature);
	if (ret < 0)
		return ret;
	if (signature.len == 0)
		return DSSH_ERROR_INVALID;
	const uint8_t *e_bytes = key.exponent.data;
	size_t e_len = key.exponent.len;
	const uint8_t *n_bytes = key.modulus.data;
	size_t n_len = key.modulus.len;
	const uint8_t *raw_sig = signature.data;
	size_t raw_sig_len = signature.len;

	try {
		Botan::BigInt        e_bn = Botan::BigInt::from_bytes(std::span<const uint8_t>(e_bytes, e_len));
		Botan::BigInt        n_bn = Botan::BigInt::from_bytes(std::span<const uint8_t>(n_bytes, n_len));
		Botan::RSA_PublicKey pubkey_h(n_bn, e_bn);

		Botan::PK_Verifier verifier(pubkey_h, RSA_SIGN_PADDING);
		verifier.update(data, data_len);
		if (verifier.check_signature(raw_sig, raw_sig_len))
			return 0;
		return DSSH_ERROR_INVALID;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

extern "C" int
dssh_botan_rsa512_pubkey(const uint8_t **out, size_t *outlen, dssh_key_algo_ctx *ctx)
{
	try {
		return pubkey_impl(out, outlen, reinterpret_cast<struct dssh_botan_key_context *>(ctx));
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

extern "C" int
dssh_botan_rsa512_haskey(dssh_key_algo_ctx *ctx)
{
	struct dssh_botan_key_context *cbd = reinterpret_cast<struct dssh_botan_key_context *>(ctx);
	return cbd != nullptr && cbd->keys != nullptr && cbd->kind == DSSH_BOTAN_KEY_RSA;
}

extern "C" void
dssh_botan_rsa512_cleanup(dssh_key_algo_ctx *ctx)
{
	struct dssh_botan_key_context *cbd = reinterpret_cast<struct dssh_botan_key_context *>(ctx);

	if (cbd == rsa_ctx)
		rsa_ctx = NULL;
	if (cbd != NULL) {
		free(cbd->pub_blob);
		dssh_signature_pool_destroy(cbd->signatures);
		try {
			delete cbd;
		}
		catch (...) {
		}
	}
}

/* Finish construction and caching before the registry can expose the key. */
static int
install_key(std::unique_ptr<Botan::Private_Key> privkey, struct dssh_botan_key_context **local)
{
	try {
		std::unique_ptr<dssh_botan_key_context> replacement(new (std::nothrow) dssh_botan_key_context{});
		if (replacement == nullptr)
			return DSSH_ERROR_ALLOC;
		replacement->privkey = std::move(privkey);
		const uint8_t *blob;
		size_t blob_len;
		int ret = pubkey_impl(&blob, &blob_len, replacement.get());
		if (ret == 0)
			ret = dssh_botan_key_pool_create(&replacement->keys, replacement->privkey);
		if (ret == 0)
			replacement->kind = DSSH_BOTAN_KEY_RSA;
		if (ret == 0)
			ret = dssh_signature_pool_create(&replacement->signatures);
		if (ret == 0 && local == NULL)
			ret = dssh_key_algo_set_ctx(RSA_SHA2_512_NAME, replacement.get());
		if (ret < 0) {
			free(replacement->pub_blob);
			dssh_signature_pool_destroy(replacement->signatures);
			return ret;
		}
		if (local != NULL) {
			*local = replacement.release();
			return 0;
		}
		struct dssh_botan_key_context *old = rsa_ctx;
		dssh_key_algo_ctx *old_opaque = reinterpret_cast<dssh_key_algo_ctx *>(old);
		rsa_ctx = replacement.release();
		dssh_botan_rsa512_cleanup(old_opaque);
		return 0;
	}
	catch (const std::bad_alloc &) {
		return DSSH_ERROR_ALLOC;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

/* --- Public API functions --- */

static int
load_key_file_inner(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata, struct dssh_botan_key_context **local)
{
	if (path == NULL || path[0] == 0)
		return DSSH_ERROR_INIT;
	if (local == NULL) {
		int config_ret = dssh_key_algo_setup_check(RSA_SHA2_512_NAME);
		if (config_ret < 0)
			return config_ret;
	}
	FILE *fp = dssh_key_file_open_read(path);
	if (fp == NULL)
		return DSSH_ERROR_INIT;
	struct dssh_key_file_stream stream;
	int stream_ret = dssh_key_file_stream_init(&stream, fp);
	if (stream_ret < 0)
		return stream_ret;
	if (fseek(fp, 0, SEEK_END) != 0)
		return dssh_key_file_stream_close(&stream, DSSH_ERROR_INIT);
	long fsize = ftell(fp);
	size_t file_sz;
	int ret = dssh_key_file_length(fsize, SIZE_MAX, &file_sz);
	if (ret < 0)
		return dssh_key_file_stream_close(&stream, ret);
	if (fseek(fp, 0, SEEK_SET) != 0)
		return dssh_key_file_stream_close(&stream, DSSH_ERROR_INIT);
	uint8_t *pem_data = static_cast<uint8_t *>(malloc(file_sz));
	if (pem_data == NULL)
		return dssh_key_file_stream_close(&stream, DSSH_ERROR_ALLOC);
	dssh_key_file_buffer input(pem_data, file_sz);
	ret = fread(pem_data, 1, file_sz, fp) == file_sz ? 0 : DSSH_ERROR_INIT;
	ret = dssh_key_file_stream_close(&stream, ret);
	if (ret < 0)
		return ret;
	char pw_buf[256];
	dssh_key_file_secret_region password_bytes(pw_buf, sizeof(pw_buf));
	struct dssh_key_file_password_state password = {pw_cb, pw_cbdata, 0};
	try {
		Botan::DataSource_Memory ds(pem_data, file_sz);
		auto get_password = [&]() -> std::string {
			int length = dssh_key_file_password(pw_buf, 256, 0, &password);
			if (length < 0)
				throw dssh_key_file_password_error{};
			size_t count = static_cast<size_t>(length);
			return std::string(pw_buf, count);
		};
		auto privkey = pw_cb == NULL ? Botan::PKCS8::load_key(ds)
		    : Botan::PKCS8::load_key(ds, get_password);
		if (privkey == nullptr || privkey->algo_name() != "RSA")
			return DSSH_ERROR_INVALID;
		if (!privkey->check_key(Botan::system_rng(), true))
			return DSSH_ERROR_INVALID;
		return install_key(std::move(privkey), local);
	}
	catch (...) {
		return password.status < 0 ? password.status : DSSH_ERROR_INIT;
	}
}

static bool
write_pem_to_file(FILE *fp, const std::string& pem)
{
	return fwrite(pem.data(), 1, pem.size(), fp) == pem.size();
}

static int
save_key_file_inner(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata, struct dssh_botan_key_context *selected)
{
	if (path == NULL || path[0] == 0)
		return DSSH_ERROR_INIT;
	if (selected == NULL || selected->keys == nullptr || selected->kind != DSSH_BOTAN_KEY_RSA)
		return DSSH_ERROR_INIT;
	dssh_botan_key_guard key;
	int acquired_key = key.acquire(selected->keys);
	if (acquired_key < 0)
		return acquired_key;
	FILE *fp = dssh_private_key_open(path);
	if (fp == NULL)
		return DSSH_ERROR_INIT;
	struct dssh_key_file_stream stream;
	int stream_ret = dssh_key_file_stream_init(&stream, fp);
	if (stream_ret < 0)
		return stream_ret;
	char pw_buf[256];
	dssh_key_file_secret_region password_bytes(pw_buf, sizeof(pw_buf));
	struct dssh_key_file_password_state password = {pw_cb, pw_cbdata, 0};
	try {
		dssh_key_file_secret_string pem;
		if (pw_cb != NULL) {
			int length = dssh_key_file_password(pw_buf, 256, 1, &password);
			if (length < 0)
				return dssh_key_file_stream_close(&stream, password.status);
			size_t count = static_cast<size_t>(length);
			std::string_view phrase(pw_buf, count);
			pem.value = Botan::PKCS8::PEM_encode_encrypted_pbkdf_msec(*key.get(),
			    Botan::system_rng(), phrase, std::chrono::milliseconds(300), nullptr, "AES-256/CBC");
		}
		else {
			pem.value = Botan::PKCS8::PEM_encode(*key.get());
		}
		bool written = write_pem_to_file(fp, pem.value);
		return dssh_key_file_stream_close(&stream, written ? 0 : DSSH_ERROR_INIT);
	}
	catch (...) {
		return dssh_key_file_stream_close(&stream, password.status < 0 ? password.status : DSSH_ERROR_INIT);
	}
}

DSSH_PUBLIC int64_t
dssh_rsa_sha2_512_get_pub_str(char *buf, size_t bufsz)
{
	const uint8_t *blob;
	size_t length;
	int ret = pubkey_impl(&blob, &length, rsa_ctx);
	if (ret < 0)
		return ret;
	return dssh_key_text_format(blob, length, RSA_KEY_TYPE_NAME, RSA_KEY_TYPE_NAME_LEN, buf, bufsz);
}

DSSH_PUBLIC int
dssh_rsa_sha2_512_save_pub_file(const char *path)
{
	if (path == NULL || path[0] == 0)
		return DSSH_ERROR_INIT;
	const uint8_t *blob;
	size_t length;
	int ret = pubkey_impl(&blob, &length, rsa_ctx);
	if (ret < 0)
		return ret;
	return dssh_key_text_write(path, blob, length, RSA_KEY_TYPE_NAME, RSA_KEY_TYPE_NAME_LEN);
}

static int
generate_key_inner(unsigned int bits, struct dssh_botan_key_context **local)
{
	if (local == NULL) {
		int config_ret = dssh_key_algo_setup_check(RSA_SHA2_512_NAME);
		if (config_ret < 0)
			return config_ret;
	}
	std::unique_ptr<Botan::Private_Key> privkey;

	try {
		privkey = std::make_unique<Botan::RSA_PrivateKey>(Botan::system_rng(), bits);
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}

	return install_key(std::move(privkey), local);
}

DSSH_PUBLIC int
dssh_rsa_sha2_512_load_key_file(const char *path, dssh_pem_password_cb cb, void *cbdata)
{
	return load_key_file_inner(path, cb, cbdata, NULL);
}

DSSH_PUBLIC int
dssh_rsa_sha2_512_generate_key(unsigned int bits)
{
	return generate_key_inner(bits, NULL);
}

DSSH_PUBLIC int
dssh_rsa_sha2_512_save_key_file(const char *path, dssh_pem_password_cb cb, void *cbdata)
{
	return save_key_file_inner(path, cb, cbdata, rsa_ctx);
}

static int
client_load(void **out, const char *path, dssh_pem_password_cb cb, void *cbdata)
{
	struct dssh_botan_key_context *local = NULL;
	int ret = load_key_file_inner(path, cb, cbdata, &local);
	if (ret == 0)
		*out = local;
	return ret;
}

static int
client_generate(void **out, unsigned int bits)
{
	struct dssh_botan_key_context *local = NULL;
	int ret = generate_key_inner(bits, &local);
	if (ret == 0)
		*out = local;
	return ret;
}

static int
client_save(void *ctx, const char *path, dssh_pem_password_cb cb, void *cbdata)
{
	struct dssh_botan_key_context *selected = static_cast<struct dssh_botan_key_context *>(ctx);
	return save_key_file_inner(path, cb, cbdata, selected);
}

static void
client_release(void *ctx)
{
	dssh_key_algo_ctx *opaque = reinterpret_cast<dssh_key_algo_ctx *>(ctx);
	dssh_botan_rsa512_cleanup(opaque);
}

extern "C" DSSH_PRIVATE const struct dssh_client_key_ops *
dssh_botan_rsa512_client_ops(void)
{
	static const struct dssh_client_key_ops ops = {
		client_load, client_generate, client_save, client_release
	};
	return &ops;
}
