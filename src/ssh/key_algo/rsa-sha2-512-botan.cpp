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

#define RSA_SHA2_512_NAME     "rsa-sha2-512"
#define RSA_SHA2_512_NAME_LEN 12
#define RSA_KEY_TYPE_NAME     "ssh-rsa"
#define RSA_KEY_TYPE_NAME_LEN 7
/* See rsa-sha2-256-botan.cpp for why we use the PKCS1v15 spelling
   instead of the deprecated EMSA_PKCS1 name. */
#define RSA_SIGN_PADDING      "PKCS1v15(SHA-512)"

struct cbdata {
	std::unique_ptr<Botan::Private_Key> privkey;
	uint8_t                            *pub_blob;
	size_t                              pub_blob_len;
};

static struct cbdata *rsa_ctx;

static int
sign_impl(uint8_t **out, size_t *outlen, const uint8_t *data, size_t data_len, struct cbdata *cbd)
{
	if (cbd == NULL || cbd->privkey == NULL)
		return DSSH_ERROR_INIT;

	try {
		Botan::PK_Signer signer(*cbd->privkey, Botan::system_rng(), RSA_SIGN_PADDING);
		signer.update(data, data_len);
		auto raw_sig = signer.signature(Botan::system_rng());

		if (raw_sig.size() > UINT32_MAX)
			return DSSH_ERROR_INVALID;
		if (raw_sig.size() > SIZE_MAX - 20)
			return DSSH_ERROR_INVALID;
		size_t needed = 20 + raw_sig.size();

		uint8_t *buf = static_cast<uint8_t *>(malloc(needed));

		if (buf == NULL)
			return DSSH_ERROR_ALLOC;

		size_t pos  = 0;
		int    sret = dssh_serialize_uint32(RSA_SHA2_512_NAME_LEN, buf, needed, &pos);
		if (sret < 0) {
			free(buf);
			return sret;
		}
		memcpy(&buf[pos], RSA_SHA2_512_NAME, RSA_SHA2_512_NAME_LEN);
		pos += RSA_SHA2_512_NAME_LEN;
		uint32_t siglen_u32 = static_cast<uint32_t>(raw_sig.size());
		sret                = dssh_serialize_uint32(siglen_u32, buf, needed, &pos);
		if (sret < 0) {
			free(buf);
			return sret;
		}
		memcpy(&buf[pos], raw_sig.data(), raw_sig.size());
		pos += raw_sig.size();

		*out    = buf;
		*outlen = pos;
		return 0;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

static int
pubkey_impl(const uint8_t **out, size_t *outlen, struct cbdata *cbd)
{
	if (cbd == NULL || cbd->privkey == NULL)
		return DSSH_ERROR_INIT;

	if (cbd->pub_blob != NULL) {
		*out    = cbd->pub_blob;
		*outlen = cbd->pub_blob_len;
		return 0;
	}

	try {
		auto *rsa_priv = dynamic_cast<Botan::RSA_PrivateKey *>(cbd->privkey.get());
		if (rsa_priv == NULL)
			return DSSH_ERROR_INIT;

		auto   e_raw = rsa_priv->get_e().serialize();
		auto   n_raw = rsa_priv->get_n().serialize();
		size_t e_sz  = e_raw.size();
		size_t n_sz  = n_raw.size();

		bool e_pad = (e_sz > 0 && (e_raw[0] & DSSH_MPINT_SIGN_BIT));
		bool n_pad = (n_sz > 0 && (n_raw[0] & DSSH_MPINT_SIGN_BIT));

		if (e_sz > UINT32_MAX - 1 || n_sz > UINT32_MAX - 1)
			return DSSH_ERROR_INVALID;
		uint32_t e_u32  = static_cast<uint32_t>(e_sz);
		uint32_t n_u32  = static_cast<uint32_t>(n_sz);
		uint32_t e_wire = e_u32 + (e_pad ? 1 : 0);
		uint32_t n_wire = n_u32 + (n_pad ? 1 : 0);

		if (static_cast<size_t>(e_wire) + n_wire > SIZE_MAX - 19)
			return DSSH_ERROR_INVALID;
		size_t needed = 19 + e_wire + n_wire;

		uint8_t *buf = static_cast<uint8_t *>(malloc(needed));

		if (buf == NULL)
			return DSSH_ERROR_ALLOC;

		size_t pos = 0;
		int    sret;

#define PK_SER(v)                                                     \
	do {                                                          \
		sret = dssh_serialize_uint32((v), buf, needed, &pos); \
		if (sret < 0) {                                       \
			free(buf);                                    \
			return sret;                                  \
		}                                                     \
	} while (0)

		PK_SER(RSA_KEY_TYPE_NAME_LEN);
		memcpy(&buf[pos], RSA_KEY_TYPE_NAME, RSA_KEY_TYPE_NAME_LEN);
		pos += RSA_KEY_TYPE_NAME_LEN;

		PK_SER(e_wire);
		if (e_pad)
			buf[pos++] = 0;
		memcpy(&buf[pos], e_raw.data(), e_sz);
		pos += e_sz;

		PK_SER(n_wire);
		if (n_pad)
			buf[pos++] = 0;
		memcpy(&buf[pos], n_raw.data(), n_sz);
		pos += n_sz;

#undef PK_SER

		cbd->pub_blob     = buf;
		cbd->pub_blob_len = pos;
		*out              = buf;
		*outlen           = pos;
		return 0;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

/* --- extern "C" wrappers for function pointer vtable --- */

extern "C" int
dssh_botan_rsa512_sign(uint8_t **out, size_t *outlen, const uint8_t *data, size_t data_len, void *ctx)
{
	try {
		return sign_impl(out, outlen, data, data_len, static_cast<struct cbdata *>(ctx));
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

extern "C" int
dssh_botan_rsa512_verify(const uint8_t *key_blob, size_t key_blob_len, const uint8_t *sig_blob,
    size_t sig_blob_len, const uint8_t *data, size_t data_len)
{
	size_t   kp = 0;
	uint32_t slen;

	if (dssh_parse_uint32(&key_blob[kp], key_blob_len - kp, &slen) < 4 || kp + 4 + slen > key_blob_len)
		return DSSH_ERROR_PARSE;
	kp += 4;
	if (slen != RSA_KEY_TYPE_NAME_LEN || memcmp(&key_blob[kp], RSA_KEY_TYPE_NAME, RSA_KEY_TYPE_NAME_LEN) != 0)
		return DSSH_ERROR_INVALID;
	kp += slen;

	if (dssh_parse_uint32(&key_blob[kp], key_blob_len - kp, &slen) < 4 || kp + 4 + slen > key_blob_len)
		return DSSH_ERROR_PARSE;
	kp += 4;
	const uint8_t *e_bytes = &key_blob[kp];
	size_t         e_len   = slen;
	kp += slen;

	if (dssh_parse_uint32(&key_blob[kp], key_blob_len - kp, &slen) < 4 || kp + 4 + slen > key_blob_len)
		return DSSH_ERROR_PARSE;
	kp += 4;
	const uint8_t *n_bytes = &key_blob[kp];
	size_t         n_len   = slen;
	kp += slen;

	if (kp != key_blob_len)
		return DSSH_ERROR_PARSE;

	size_t sp = 0;

	if (dssh_parse_uint32(&sig_blob[sp], sig_blob_len - sp, &slen) < 4 || sp + 4 + slen > sig_blob_len)
		return DSSH_ERROR_PARSE;
	sp += 4;
	if (slen != RSA_SHA2_512_NAME_LEN || memcmp(&sig_blob[sp], RSA_SHA2_512_NAME, RSA_SHA2_512_NAME_LEN) != 0)
		return DSSH_ERROR_INVALID;
	sp += slen;

	uint32_t raw_sig_len;
	if (dssh_parse_uint32(&sig_blob[sp], sig_blob_len - sp, &raw_sig_len) < 4
	    || sp + 4 + raw_sig_len > sig_blob_len)
		return DSSH_ERROR_PARSE;
	sp += 4;
	const uint8_t *raw_sig = &sig_blob[sp];
	sp += raw_sig_len;
	if (sp != sig_blob_len)
		return DSSH_ERROR_PARSE;

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
dssh_botan_rsa512_pubkey(const uint8_t **out, size_t *outlen, void *ctx)
{
	try {
		return pubkey_impl(out, outlen, static_cast<struct cbdata *>(ctx));
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

extern "C" int
dssh_botan_rsa512_haskey(void *ctx)
{
	struct cbdata *cbd = static_cast<struct cbdata *>(ctx);
	if (cbd == NULL || cbd->privkey == NULL)
		return 0;
	try {
		return (cbd->privkey->algo_name() == "RSA");
	}
	catch (...) {
		return 0;
	}
}

extern "C" void
dssh_botan_rsa512_cleanup(void *ctx)
{
	struct cbdata *cbd = static_cast<struct cbdata *>(ctx);

	if (cbd == rsa_ctx)
		rsa_ctx = NULL;
	if (cbd != NULL) {
		free(cbd->pub_blob);
		try {
			delete cbd;
		}
		catch (...) {
		}
	}
}

/* --- Public API functions --- */

DSSH_PUBLIC int
dssh_rsa_sha2_512_load_key_file(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata)
{
	if (path == NULL)
		return DSSH_ERROR_INIT;

	FILE *fp = fopen(path, "r");
	if (fp == NULL)
		return DSSH_ERROR_INIT;

	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		return DSSH_ERROR_INIT;
	}
	long fsize = ftell(fp);
	if (fsize <= 0) {
		fclose(fp);
		return DSSH_ERROR_INIT;
	}
	if (fseek(fp, 0, SEEK_SET) != 0) {
		fclose(fp);
		return DSSH_ERROR_INIT;
	}
	size_t   file_sz  = static_cast<size_t>(fsize);
	uint8_t *pem_data = static_cast<uint8_t *>(malloc(file_sz));
	if (pem_data == NULL) {
		fclose(fp);
		return DSSH_ERROR_ALLOC;
	}
	if (fread(pem_data, 1, file_sz, fp) != file_sz) {
		free(pem_data);
		fclose(fp);
		return DSSH_ERROR_INIT;
	}
	fclose(fp);

	char        pw_buf[256];
	const char *password = NULL;
	if (pw_cb != NULL) {
		int pw_len = pw_cb(pw_buf, static_cast<int>(sizeof(pw_buf)), 0, pw_cbdata);
		if (pw_len > 0 && pw_len < static_cast<int>(sizeof(pw_buf))) {
			pw_buf[pw_len] = '\0';
			password       = pw_buf;
		}
	}

	std::unique_ptr<Botan::Private_Key> privkey;

	try {
		Botan::DataSource_Memory ds(pem_data, file_sz);
		std::string              pw_str = password ? password : "";

		privkey = Botan::PKCS8::load_key(ds, pw_str);
	}
	catch (...) {
		free(pem_data);
		dssh_cleanse(pw_buf, sizeof(pw_buf));
		return DSSH_ERROR_INIT;
	}
	free(pem_data);
	dssh_cleanse(pw_buf, sizeof(pw_buf));

	if (privkey->algo_name() != "RSA")
		return DSSH_ERROR_INVALID;

	if (rsa_ctx == NULL) {
		rsa_ctx = new (std::nothrow) cbdata;
		if (rsa_ctx == NULL)
			return DSSH_ERROR_ALLOC;
		rsa_ctx->pub_blob     = NULL;
		rsa_ctx->pub_blob_len = 0;
		int res               = dssh_key_algo_set_ctx(RSA_SHA2_512_NAME, rsa_ctx);
		if (res < 0) {
			delete rsa_ctx;
			rsa_ctx = NULL;
			return res;
		}
	}
	else {
		free(rsa_ctx->pub_blob);
		rsa_ctx->pub_blob     = NULL;
		rsa_ctx->pub_blob_len = 0;
	}

	rsa_ctx->privkey = std::move(privkey);
	return 0;
}

static bool
write_pem_to_file(FILE *fp, const std::string& pem)
{
	return fwrite(pem.data(), 1, pem.size(), fp) == pem.size();
}

DSSH_PUBLIC int
dssh_rsa_sha2_512_save_key_file(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata)
{
	if (path == NULL)
		return DSSH_ERROR_INIT;
	if (rsa_ctx == NULL || rsa_ctx->privkey == NULL)
		return DSSH_ERROR_INIT;

	FILE *fp = fopen(path, "w");
	if (fp == NULL)
		return DSSH_ERROR_INIT;

	try {
		std::string pem;

		if (pw_cb != NULL) {
			char pw_buf[256];
			int  pw_len = pw_cb(pw_buf, static_cast<int>(sizeof(pw_buf)), 1, pw_cbdata);
			if (pw_len <= 0 || pw_len >= static_cast<int>(sizeof(pw_buf))) {
				dssh_cleanse(pw_buf, sizeof(pw_buf));
				fclose(fp);
				return DSSH_ERROR_INIT;
			}
			pw_buf[pw_len] = '\0';
			pem = Botan::PKCS8::PEM_encode_encrypted_pbkdf_msec(*rsa_ctx->privkey, Botan::system_rng(),
			    pw_buf, std::chrono::milliseconds(300), nullptr, "AES-256/CBC");
			dssh_cleanse(pw_buf, sizeof(pw_buf));
		}
		else {
			pem = Botan::PKCS8::PEM_encode(*rsa_ctx->privkey);
		}

		bool wok = write_pem_to_file(fp, pem);

		if (fclose(fp) != 0)
			return DSSH_ERROR_INIT;
		return wok ? 0 : DSSH_ERROR_INIT;
	}
	catch (...) {
		fclose(fp);
		return DSSH_ERROR_INIT;
	}
}

DSSH_PUBLIC int64_t
dssh_rsa_sha2_512_get_pub_str(char *buf, size_t bufsz)
{
	const uint8_t *blob;
	size_t         blob_len;
	int            res = pubkey_impl(&blob, &blob_len, rsa_ctx);
	if (res < 0)
		return res;

	size_t b64_len = 4 * ((blob_len + 2) / 3);
	size_t needed  = RSA_KEY_TYPE_NAME_LEN + 1 + b64_len + 1;

	if (buf == NULL || bufsz == 0)
		return static_cast<int64_t>(needed);
	if (bufsz < needed)
		return DSSH_ERROR_TOOLONG;

	memcpy(buf, RSA_KEY_TYPE_NAME, RSA_KEY_TYPE_NAME_LEN);
	buf[RSA_KEY_TYPE_NAME_LEN] = ' ';
	int encret = dssh_base64_encode(blob, blob_len, &buf[RSA_KEY_TYPE_NAME_LEN + 1], b64_len + 1);
	if (encret < 0)
		return encret;
	return static_cast<int64_t>(needed);
}

DSSH_PUBLIC int
dssh_rsa_sha2_512_save_pub_file(const char *path)
{
	if (path == NULL)
		return DSSH_ERROR_INIT;
	char   *str = NULL;
	int64_t len = dssh_rsa_sha2_512_get_pub_str(NULL, 0);
	if (len < 0)
		return static_cast<int>(len);
	str = static_cast<char *>(malloc(static_cast<size_t>(len)));
	if (str == NULL)
		return DSSH_ERROR_ALLOC;
	int64_t res = dssh_rsa_sha2_512_get_pub_str(str, static_cast<size_t>(len));
	if (res < 0) {
		free(str);
		return static_cast<int>(res);
	}

	FILE *fp = fopen(path, "w");
	if (fp == NULL) {
		free(str);
		return DSSH_ERROR_INIT;
	}
	int wok = fprintf(fp, "%s\n", str) >= 0;
	if (fclose(fp) != 0)
		wok = 0;
	free(str);
	return wok ? 0 : DSSH_ERROR_INIT;
}

DSSH_PUBLIC int
dssh_rsa_sha2_512_generate_key(unsigned int bits)
{
	std::unique_ptr<Botan::Private_Key> privkey;

	try {
		privkey = std::make_unique<Botan::RSA_PrivateKey>(Botan::system_rng(), bits);
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}

	if (rsa_ctx == NULL) {
		rsa_ctx = new (std::nothrow) cbdata;
		if (rsa_ctx == NULL)
			return DSSH_ERROR_ALLOC;
		rsa_ctx->pub_blob     = NULL;
		rsa_ctx->pub_blob_len = 0;
		int res               = dssh_key_algo_set_ctx(RSA_SHA2_512_NAME, rsa_ctx);
		if (res < 0) {
			delete rsa_ctx;
			rsa_ctx = NULL;
			return res;
		}
	}
	else {
		free(rsa_ctx->pub_blob);
		rsa_ctx->pub_blob     = NULL;
		rsa_ctx->pub_blob_len = 0;
	}

	rsa_ctx->privkey = std::move(privkey);
	return 0;
}
