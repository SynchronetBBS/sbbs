/*
 * key_algo/ssh-ed25519-botan.cpp -- Ed25519 host key algorithm via Botan3 native C++ API.
 *
 * The dssh_register_ssh_ed25519() function lives in the companion .c file
 * to avoid C++ exposure to C flexible array member structs.
 */

#include <botan/ed25519.h>
#include <botan/pk_keys.h>
#include <botan/pkcs8.h>
#include <botan/pubkey.h>
#include <botan/system_rng.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "deucessh-algorithms.h"
#include "deucessh-crypto.h"
#include "deucessh.h"

#define ED25519_RAW_PUB_LEN 32
#define ED25519_RAW_SIG_LEN 64
#define ED25519_NAME        "ssh-ed25519"
#define ED25519_NAME_LEN    11

struct cbdata {
	std::unique_ptr<Botan::Private_Key> privkey;
	uint8_t                            *pub_blob;
	size_t                              pub_blob_len;
};

static struct cbdata *ed25519_ctx;

static int
sign_impl(uint8_t **out, size_t *outlen, const uint8_t *data, size_t data_len, struct cbdata *cbd)
{
	if (cbd == NULL || cbd->privkey == NULL)
		return DSSH_ERROR_INIT;

	try {
		Botan::PK_Signer signer(*cbd->privkey, Botan::system_rng(), "");
		signer.update(data, data_len);
		auto raw_sig = signer.signature(Botan::system_rng());

		if (raw_sig.size() != ED25519_RAW_SIG_LEN)
			return DSSH_ERROR_INIT;

		size_t   needed = 4 + ED25519_NAME_LEN + 4 + raw_sig.size();
		uint8_t *buf    = static_cast<uint8_t *>(malloc(needed));

		if (buf == NULL)
			return DSSH_ERROR_ALLOC;

		size_t pos = 0;
		int    ret = dssh_serialize_uint32(ED25519_NAME_LEN, buf, needed, &pos);
		if (ret < 0) {
			free(buf);
			return ret;
		}
		memcpy(&buf[pos], ED25519_NAME, ED25519_NAME_LEN);
		pos += ED25519_NAME_LEN;
		ret = dssh_serialize_uint32(static_cast<uint32_t>(raw_sig.size()), buf, needed, &pos);
		if (ret < 0) {
			free(buf);
			return ret;
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
		auto *ed_priv = dynamic_cast<Botan::Ed25519_PrivateKey *>(cbd->privkey.get());
		if (ed_priv == NULL)
			return DSSH_ERROR_INIT;

		/* raw_public_key_bits() replaces get_public_key() from Botan 3.9. */
		auto raw_pub = ed_priv->raw_public_key_bits();

		if (raw_pub.size() != ED25519_RAW_PUB_LEN)
			return DSSH_ERROR_INIT;

		size_t   needed = 4 + ED25519_NAME_LEN + 4 + ED25519_RAW_PUB_LEN;
		uint8_t *buf    = static_cast<uint8_t *>(malloc(needed));

		if (buf == NULL)
			return DSSH_ERROR_ALLOC;

		size_t pos = 0;
		int    ret = dssh_serialize_uint32(ED25519_NAME_LEN, buf, needed, &pos);
		if (ret < 0) {
			free(buf);
			return ret;
		}
		memcpy(&buf[pos], ED25519_NAME, ED25519_NAME_LEN);
		pos += ED25519_NAME_LEN;
		ret = dssh_serialize_uint32(ED25519_RAW_PUB_LEN, buf, needed, &pos);
		if (ret < 0) {
			free(buf);
			return ret;
		}
		memcpy(&buf[pos], raw_pub.data(), ED25519_RAW_PUB_LEN);
		pos += ED25519_RAW_PUB_LEN;

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
dssh_botan_ed25519_sign(uint8_t **out, size_t *outlen, const uint8_t *data, size_t data_len, void *ctx)
{
	try {
		return sign_impl(out, outlen, data, data_len, static_cast<struct cbdata *>(ctx));
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

extern "C" int
dssh_botan_ed25519_verify(const uint8_t *key_blob, size_t key_blob_len, const uint8_t *sig_blob,
    size_t sig_blob_len, const uint8_t *data, size_t data_len)
{
	size_t   kp = 0;
	uint32_t algo_len;
	if (dssh_parse_uint32(&key_blob[kp], key_blob_len, &algo_len) < 4 || kp + 4 + algo_len > key_blob_len)
		return DSSH_ERROR_PARSE;
	kp += 4;
	if (algo_len != ED25519_NAME_LEN || memcmp(&key_blob[kp], ED25519_NAME, ED25519_NAME_LEN) != 0)
		return DSSH_ERROR_INVALID;
	kp += algo_len;
	uint32_t raw_pub_len;
	if (dssh_parse_uint32(&key_blob[kp], key_blob_len - kp, &raw_pub_len) < 4
	    || kp + 4 + raw_pub_len > key_blob_len)
		return DSSH_ERROR_PARSE;
	kp += 4;
	const uint8_t *raw_pub = &key_blob[kp];
	kp += raw_pub_len;

	if (raw_pub_len != ED25519_RAW_PUB_LEN)
		return DSSH_ERROR_INVALID;
	if (kp != key_blob_len)
		return DSSH_ERROR_PARSE;

	size_t   sp = 0;
	uint32_t sig_algo_len;
	if (dssh_parse_uint32(&sig_blob[sp], sig_blob_len, &sig_algo_len) < 4
	    || sp + 4 + sig_algo_len > sig_blob_len)
		return DSSH_ERROR_PARSE;
	sp += 4;
	if (sig_algo_len != ED25519_NAME_LEN || memcmp(&sig_blob[sp], ED25519_NAME, ED25519_NAME_LEN) != 0)
		return DSSH_ERROR_INVALID;
	sp += sig_algo_len;
	uint32_t raw_sig_len;
	if (dssh_parse_uint32(&sig_blob[sp], sig_blob_len - sp, &raw_sig_len) < 4
	    || sp + 4 + raw_sig_len > sig_blob_len)
		return DSSH_ERROR_PARSE;
	sp += 4;
	const uint8_t *raw_sig = &sig_blob[sp];
	sp += raw_sig_len;

	if (raw_sig_len != ED25519_RAW_SIG_LEN)
		return DSSH_ERROR_INVALID;
	if (sp != sig_blob_len)
		return DSSH_ERROR_PARSE;

	try {
		Botan::Ed25519_PublicKey pubkey_h(std::span<const uint8_t>(raw_pub, ED25519_RAW_PUB_LEN));

		Botan::PK_Verifier verifier(pubkey_h, "");
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
dssh_botan_ed25519_pubkey(const uint8_t **out, size_t *outlen, void *ctx)
{
	try {
		return pubkey_impl(out, outlen, static_cast<struct cbdata *>(ctx));
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

extern "C" int
dssh_botan_ed25519_haskey(void *ctx)
{
	struct cbdata *cbd = static_cast<struct cbdata *>(ctx);
	if (cbd == NULL || cbd->privkey == NULL)
		return 0;
	try {
		return (cbd->privkey->algo_name() == "Ed25519");
	}
	catch (...) {
		return 0;
	}
}

extern "C" void
dssh_botan_ed25519_cleanup(void *ctx)
{
	struct cbdata *cbd = static_cast<struct cbdata *>(ctx);

	if (cbd == ed25519_ctx)
		ed25519_ctx = NULL;
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
dssh_ed25519_load_key_file(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata)
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

	if (privkey->algo_name() != "Ed25519")
		return DSSH_ERROR_INVALID;

	if (ed25519_ctx == NULL) {
		ed25519_ctx = new (std::nothrow) cbdata;
		if (ed25519_ctx == NULL)
			return DSSH_ERROR_ALLOC;
		ed25519_ctx->pub_blob     = NULL;
		ed25519_ctx->pub_blob_len = 0;
		int res                   = dssh_key_algo_set_ctx(ED25519_NAME, ed25519_ctx);
		if (res < 0) {
			delete ed25519_ctx;
			ed25519_ctx = NULL;
			return res;
		}
	}
	else {
		free(ed25519_ctx->pub_blob);
		ed25519_ctx->pub_blob     = NULL;
		ed25519_ctx->pub_blob_len = 0;
	}

	ed25519_ctx->privkey = std::move(privkey);
	return 0;
}

static bool
write_pem_to_file(FILE *fp, const std::string& pem)
{
	return fwrite(pem.data(), 1, pem.size(), fp) == pem.size();
}

DSSH_PUBLIC int
dssh_ed25519_save_key_file(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata)
{
	if (path == NULL)
		return DSSH_ERROR_INIT;
	if (ed25519_ctx == NULL || ed25519_ctx->privkey == NULL)
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
			pem            = Botan::PKCS8::PEM_encode_encrypted_pbkdf_msec(*ed25519_ctx->privkey,
				       Botan::system_rng(), pw_buf, std::chrono::milliseconds(300), nullptr, "AES-256/CBC");
			dssh_cleanse(pw_buf, sizeof(pw_buf));
		}
		else {
			pem = Botan::PKCS8::PEM_encode(*ed25519_ctx->privkey);
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
dssh_ed25519_get_pub_str(char *buf, size_t bufsz)
{
	const uint8_t *blob;
	size_t         blob_len;
	int            res = pubkey_impl(&blob, &blob_len, ed25519_ctx);
	if (res < 0)
		return res;

	size_t b64_len = 4 * ((blob_len + 2) / 3);
	size_t needed  = ED25519_NAME_LEN + 1 + b64_len + 1;

	if (buf == NULL || bufsz == 0)
		return static_cast<int64_t>(needed);
	if (bufsz < needed)
		return DSSH_ERROR_TOOLONG;

	memcpy(buf, ED25519_NAME, ED25519_NAME_LEN);
	buf[ED25519_NAME_LEN] = ' ';
	int encret            = dssh_base64_encode(blob, blob_len, &buf[ED25519_NAME_LEN + 1], b64_len + 1);
	if (encret < 0)
		return encret;
	return static_cast<int64_t>(needed);
}

DSSH_PUBLIC int
dssh_ed25519_save_pub_file(const char *path)
{
	if (path == NULL)
		return DSSH_ERROR_INIT;
	char    str[256];
	int64_t len = dssh_ed25519_get_pub_str(str, sizeof(str));
	if (len < 0)
		return static_cast<int>(len);

	FILE *fp = fopen(path, "w");
	if (fp == NULL)
		return DSSH_ERROR_INIT;
	int wok = fprintf(fp, "%s\n", str) >= 0;
	if (fclose(fp) != 0)
		wok = 0;
	return wok ? 0 : DSSH_ERROR_INIT;
}

DSSH_PUBLIC int
dssh_ed25519_generate_key(void)
{
	std::unique_ptr<Botan::Private_Key> privkey;

	try {
		privkey = std::make_unique<Botan::Ed25519_PrivateKey>(Botan::system_rng());
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}

	if (ed25519_ctx == NULL) {
		ed25519_ctx = new (std::nothrow) cbdata;
		if (ed25519_ctx == NULL)
			return DSSH_ERROR_ALLOC;
		ed25519_ctx->pub_blob     = NULL;
		ed25519_ctx->pub_blob_len = 0;
		int res                   = dssh_key_algo_set_ctx(ED25519_NAME, ed25519_ctx);
		if (res < 0) {
			delete ed25519_ctx;
			ed25519_ctx = NULL;
			return res;
		}
	}
	else {
		free(ed25519_ctx->pub_blob);
		ed25519_ctx->pub_blob     = NULL;
		ed25519_ctx->pub_blob_len = 0;
	}

	ed25519_ctx->privkey = std::move(privkey);
	return 0;
}
