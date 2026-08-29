/* xp_ca_file.h includes <windows.h>, whose min macro breaks Botan's x509_ext.h.
 * Suppress it before either header can be included. */
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "xp_ca.h"
#include "xp_ca_file.h"
#include "xp_key_internal.h"
#include "xp_ca_policy.h"
#include "xp_sign.h"

#include <botan/auto_rng.h>
#include <botan/ber_dec.h>
#include <botan/certstor.h>
#include <botan/data_src.h>
#include <botan/der_enc.h>
#include <botan/ed25519.h>
#include <botan/ecdsa.h>
#include <botan/ec_group.h>
#include <botan/hash.h>
#include <botan/pkcs10.h>
#include <botan/pkcs8.h>
#include <botan/pem.h>
#include <botan/pkix_types.h>
#include <botan/pubkey.h>
#include <botan/rsa.h>
#include <botan/x509_ca.h>
#include <botan/x509_ext.h>
#include <botan/x509_key.h>
#include <botan/x509_obj.h>
#include <botan/x509path.h>
#include <botan/build.h>

/* Botan's build configuration can advertise PKCS#11 while its public p11.h
 * still requires a separately discoverable <pkcs11.h>.  Keep the optional
 * adapter out of direct-source consumers unless that complete public-header
 * dependency is actually available on their include path. */
#if defined(BOTAN_HAS_PKCS11) && defined(__has_include)
#if __has_include(<pkcs11.h>)
#define XPTLS_BOTAN_HAS_PKCS11 1
#endif
#endif

#if defined(XPTLS_BOTAN_HAS_PKCS11)
#include <botan/ec_group.h>
#include <botan/p11_ecdsa.h>
#include <botan/p11_object.h>
#include <botan/p11_rsa.h>
#include <botan/p11_types.h>
#endif

struct xp_key {
	std::atomic_size_t references{1};
	std::unique_ptr<Botan::Private_Key> native;
	std::unique_ptr<Botan::Public_Key> public_native;
#if defined(XPTLS_BOTAN_HAS_PKCS11)
	std::shared_ptr<Botan::PKCS11::Module> pkcs11_module;
	std::shared_ptr<Botan::PKCS11::Slot> pkcs11_slot;
	std::shared_ptr<Botan::PKCS11::Session> pkcs11_session;
#endif
	bool exportable{true};
	enum xp_key_store_kind storage{XP_KEY_STORE_MEMORY};
	std::vector<uint8_t> reference;
	std::string err;
};

struct xp_ca_csr {
	std::unique_ptr<Botan::PKCS10_Request> native;
};

struct xp_ca_cert {
	std::unique_ptr<Botan::X509_Certificate> native;
};

struct xp_ca_crl {
	std::unique_ptr<Botan::X509_CRL> native;
};

static thread_local std::string error_text;

static Botan::Public_Key *
public_key(xp_key_t key)
{
	if (key == nullptr)
		return nullptr;
	return key->native != nullptr ? key->native.get() : key->public_native.get();
}

static const char *
key_hash(const Botan::Public_Key& key)
{
	if (key.algo_name() == "ECDSA") {
		auto& ec = dynamic_cast<const Botan::ECDSA_PublicKey&>(key);
		size_t bits = ec.domain().get_p_bits();
		return bits <= 256 ? "SHA-256" : bits <= 384 ? "SHA-384" : "SHA-512";
	}
	return key.algo_name() == "Ed25519" ? "SHA-512" : "SHA-256";
}

static const char *
key_padding(const Botan::Public_Key& key)
{
	return key.algo_name() == "Ed25519" ? "Pure" : "";
}

static std::string
signing_scheme(const Botan::Public_Key& key)
{
	if (key.algo_name() == "RSA")
		return std::string("EMSA3(") + key_hash(key) + ")";
	if (key.algo_name() == "ECDSA")
		return key_hash(key);
	return "Pure";
}

static bool
supported_key(const Botan::Public_Key *key)
{
	return key != nullptr && (key->algo_name() == "Ed25519"
		|| key->algo_name() == "RSA" || key->algo_name() == "ECDSA");
}

static const Botan::Cert_Extension::CRL_Distribution_Points *
crl_distribution_points(const Botan::X509_Certificate& certificate)
{
	return certificate.v3_extensions().get_extension_object_as<
		Botan::Cert_Extension::CRL_Distribution_Points>();
}

static const Botan::AlternativeName *
distribution_point_name(
	const Botan::Cert_Extension::CRL_Distribution_Points::Distribution_Point& point)
{
	const auto& name = point.distribution_point_name();
	return name.has_value() && name->full_name().has_value()
		? &*name->full_name() : nullptr;
}

static bool
certificate_has_crl_distribution_point(
	const Botan::X509_Certificate& certificate,
	std::string_view expected)
{
	const auto *extension = crl_distribution_points(certificate);
	if (extension == nullptr)
		return false;
	for (const auto& distribution_point : extension->distribution_points()) {
		const auto *names = distribution_point_name(distribution_point);
		if (names != nullptr) {
			for (const auto& uri : names->uri_names()) {
				if (uri.original_input() == expected)
					return true;
			}
		}
	}
	return false;
}

static bool
certificate_has_exact_crl_distribution_point(
	const Botan::X509_Certificate& certificate,
	const char *expected)
{
	const auto *extension = crl_distribution_points(certificate);
	if (expected == nullptr)
		return extension == nullptr;
	if (extension == nullptr || extension->distribution_points().size() != 1)
		return false;
	const auto *names = distribution_point_name(extension->distribution_points()[0]);
	return names != nullptr && names->count() == 1
		&& names->uri_names().size() == 1
		&& names->uri_names().begin()->original_input() == expected;
}

static Botan::OID
crl_issuing_distribution_point_oid()
{
	return Botan::OID({ 2, 5, 29, 28 });
}

static std::optional<Botan::AlternativeName>
crl_issuing_distribution_point(const Botan::X509_CRL& crl)
{
	const auto *extension = crl.extensions().get_extension_object_as<
		Botan::Cert_Extension::CRL_Issuing_Distribution_Point>();
	if (extension == nullptr)
		return std::nullopt;
	const auto& name = extension->distribution_point_name();
	if (!name.has_value() || !name->full_name().has_value())
		return std::nullopt;
	return *name->full_name();
}

static uint32_t
crl_number(const Botan::X509_CRL& crl)
{
	const auto& number = crl.crl_number_bigint();
	if (!number.has_value())
		return 0;
	if (number->signum() < 0 || number->bits() > 32)
		throw std::invalid_argument("CRL number does not fit in uint32_t");
	return static_cast<uint32_t>(number->word_at(0));
}

static bool
crl_has_exact_issuing_distribution_point(
	const Botan::X509_CRL& crl,
	std::string_view expected)
{
	const auto names = crl_issuing_distribution_point(crl);
	if (!names.has_value()
	    || !crl.extensions().critical_extension_set(
	        crl_issuing_distribution_point_oid()))
		return false;
	return names->count() == 1
		&& names->uri_names().size() == 1
		&& names->uri_names().begin()->original_input() == expected;
}

static int
fail(int code, const char *message) noexcept
{
	try {
		error_text = message;
	} catch (...) {
		/* Preserve the original failure code if diagnostics cannot allocate. */
	}
	return code;
}

extern "C" const char *
xp_ca_last_error(void)
{
	return error_text.empty() ? "no error" : error_text.c_str();
}

extern "C" int
xp_ca_cert_tls_server_usable(xp_ca_cert_t certificate)
{
	if (certificate == nullptr || certificate->native == nullptr)
		return XP_CRYPTO_ERR_INVALID;
	try {
		return !certificate->native->is_CA_cert() &&
		    certificate->native->allowed_usage(Botan::Usage_Type::TLS_SERVER_AUTH)
		    ? XP_CRYPTO_OK : XP_CRYPTO_ERR_POLICY;
	}
	catch (...) {
		return XP_CRYPTO_ERR_POLICY;
	}
}

extern "C" int
xp_key_generate(xp_key_t *out, const struct xp_key_spec *spec)
{
	if (out == nullptr)
		return XP_CA_ERR;
	*out = nullptr;
	if (spec == nullptr)
		return XP_CRYPTO_ERR_INVALID;
	try {
		Botan::AutoSeeded_RNG rng;
		auto                  key = std::make_unique<xp_key>();
		switch (spec->algorithm) {
			case XP_KEY_ED25519:
				if (spec->bits != 0 || spec->curve != XP_KEY_CURVE_NONE)
					return XP_CA_ERR_POLICY;
				key->native = std::make_unique<Botan::Ed25519_PrivateKey>(rng);
				break;
			case XP_KEY_RSA:
				if ((spec->bits != 2048 && spec->bits != 3072
				     && spec->bits != 4096)
				    || spec->curve != XP_KEY_CURVE_NONE)
					return XP_CA_ERR_POLICY;
				key->native = std::make_unique<Botan::RSA_PrivateKey>(rng, spec->bits);
				break;
			case XP_KEY_ECDSA: {
				if (spec->bits != 0)
					return XP_CA_ERR_POLICY;
				const char *curve = spec->curve == XP_KEY_CURVE_P256 ? "secp256r1"
					: spec->curve == XP_KEY_CURVE_P384 ? "secp384r1"
					: spec->curve == XP_KEY_CURVE_P521 ? "secp521r1" : nullptr;
				if (curve == nullptr)
					return XP_CA_ERR_POLICY;
				key->native = std::make_unique<Botan::ECDSA_PrivateKey>(
					rng, Botan::EC_Group::from_name(curve));
				break;
			}
			default: return XP_CA_ERR_POLICY;
		}
		*out = key.release();
		return XP_CA_OK;
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR, e.what());
	}
}

extern "C" void
xp_key_release(xp_key_t key)
{
	if (key != nullptr && key->references.fetch_sub(1, std::memory_order_acq_rel) == 1)
		delete key;
}

extern "C" void
xp_key_retain(xp_key_t key)
{
	if (key != nullptr)
		key->references.fetch_add(1, std::memory_order_relaxed);
}

extern "C" const char *
xp_key_errstr(xp_key_t key)
{
	return key == nullptr ? "(null xp_key_t)"
		: key->err.empty() ? "no error" : key->err.c_str();
}

extern "C" void
xp_key_set_storage_metadata(
	xp_key_t key, enum xp_key_store_kind storage, bool exportable)
{
	if (key != nullptr) {
		key->storage = storage;
		key->exportable = exportable;
	}
}

extern "C" int
xp_key_set_reference(xp_key_t key, const void *reference, size_t len)
{
	if (key == nullptr || (reference == nullptr && len != 0))
		return XP_CRYPTO_ERR_INVALID;
	try {
		key->reference.clear();
		if (len != 0) {
			const auto *bytes = static_cast<const uint8_t *>(reference);
			key->reference.assign(bytes, bytes + len);
		}
		return XP_CRYPTO_OK;
	}
	catch (...) {
		return XP_CRYPTO_ERR;
	}
}

extern "C" int
xp_key_reference(xp_key_t key, void *out, size_t *len)
{
	if (key == nullptr || len == nullptr)
		return XP_CRYPTO_ERR_INVALID;
	if (key->reference.empty()) {
		*len = 0;
		return XP_CRYPTO_ERR_NOT_FOUND;
	}
	if (out == nullptr) {
		*len = key->reference.size();
		return XP_CRYPTO_OK;
	}
	if (*len < key->reference.size()) {
		*len = key->reference.size();
		return XP_CRYPTO_ERR_BUFFER_TOO_SMALL;
	}
	std::memcpy(out, key->reference.data(), key->reference.size());
	*len = key->reference.size();
	return XP_CRYPTO_OK;
}

extern "C" void *xp_key_native_private(xp_key_t key)
{
	return key == nullptr ? nullptr : key->native.get();
}

extern "C" int
xp_key_save_private_pem_file(xp_key_t key, const char *path, const char *password)
{
	if (key == nullptr || path == nullptr || password == nullptr
	    || password[0] == '\0')
		return XP_CA_ERR;
	try {
		Botan::AutoSeeded_RNG rng;
		std::string           encoded = Botan::PKCS8::PEM_encode(*key->native, rng, password);
		char                  temporary[4096];
		FILE *                file = xp_ca_open_private_temporary(path, temporary,
		                                                          sizeof(temporary));
		bool                  written = file != nullptr
		                                && fwrite(encoded.data(), 1, encoded.size(), file)
		                                == encoded.size();
		xp_ca_scrub_memory(encoded.data(), encoded.size());
		if (!written) {
			if (file != nullptr)
				xp_ca_discard_private_temporary(file, temporary);
			return fail(XP_CA_ERR, "private key write failed");
		}
		if (xp_ca_commit_private_temporary(file, temporary, path) != 0)
			return fail(XP_CA_ERR, "private key commit failed");
		return XP_CA_OK;
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR, e.what());
	}
}

extern "C" int
xp_key_delete_private_pem_file(const char *path)
{
	if (path == nullptr || std::remove(path) != 0)
		return fail(XP_CA_ERR, "private key deletion failed");
	return XP_CA_OK;
}

extern "C" int
xp_key_load_private_pem_file(xp_key_t *out, const char *path, const char *password)
{
	if (out == nullptr)
		return XP_CA_ERR;
	*out = nullptr;
	if (path == nullptr || password == nullptr || password[0] == '\0')
		return XP_CA_ERR;
	try {
		Botan::DataSource_Stream source(path);
		auto                     key = std::make_unique<xp_key>();
		key->native = Botan::PKCS8::load_key(source, password);
		if (!supported_key(key->native.get()))
			return fail(XP_CA_ERR_FORMAT, "unsupported private key algorithm");
		*out = key.release();
		return XP_CA_OK;
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR, e.what());
	}
}

static int
password_value(xp_ca_password_callback_t callback, void *context,
	           std::string& value)
{
	value.clear();
	if (callback == nullptr)
		return XP_CA_OK;
	size_t length = 0;
	if (callback(context, nullptr, 0, &length) != 0 || length == 0)
		return XP_CA_ERR;
	value.resize(length);
	size_t actual = length;
	if (callback(context, value.data(), value.size(), &actual) != 0
	    || actual != length) {
		xp_ca_scrub_memory(value.data(), value.size());
		value.clear();
		return XP_CA_ERR;
	}
	return XP_CA_OK;
}

struct scrubbed_string {
	std::string value;
	~scrubbed_string()
	{
		if (!value.empty())
			xp_ca_scrub_memory(value.data(), value.size());
	}
};

extern "C" int
xp_key_get_info(xp_key_t key, struct xp_key_info *info)
{
	auto *native = public_key(key);
	if (!supported_key(native) || info == nullptr)
		return XP_CA_ERR;
	*info = {};
	info->has_private = key->native != nullptr;
	info->exportable = key->exportable;
	info->storage = key->storage;
	if (native->algo_name() == "Ed25519") {
		info->spec.algorithm = XP_KEY_ED25519;
		info->spec.bits = 256;
	} else if (native->algo_name() == "RSA") {
		info->spec.algorithm = XP_KEY_RSA;
		info->spec.bits = static_cast<unsigned>(
			dynamic_cast<const Botan::RSA_PublicKey&>(*native).get_n().bits());
	} else {
		auto& ec = dynamic_cast<const Botan::ECDSA_PublicKey&>(*native);
		info->spec.algorithm = XP_KEY_ECDSA;
		info->spec.bits = static_cast<unsigned>(ec.domain().get_p_bits());
		info->spec.curve = info->spec.bits == 256 ? XP_KEY_CURVE_P256
			: info->spec.bits == 384 ? XP_KEY_CURVE_P384
			: info->spec.bits == 521 ? XP_KEY_CURVE_P521 : XP_KEY_CURVE_NONE;
		if (info->spec.curve == XP_KEY_CURVE_NONE)
			return XP_CA_ERR_FORMAT;
	}
	return XP_CA_OK;
}

extern "C" int
xp_key_import_private_pem(xp_key_t *out, const void *pem, size_t len,
	                 xp_ca_password_callback_t password, void *context)
{
	if (out == nullptr)
		return XP_CA_ERR;
	*out = nullptr;
	if (pem == nullptr || len == 0)
		return XP_CA_ERR;
	try {
		scrubbed_string pass;
		if (password_value(password, context, pass.value) != XP_CA_OK)
			return XP_CA_ERR;
		Botan::DataSource_Memory source(static_cast<const uint8_t *>(pem), len);
		std::unique_ptr<Botan::Private_Key> native;
		try {
			native = password == nullptr ? Botan::PKCS8::load_key(source)
				: Botan::PKCS8::load_key(source, pass.value);
		} catch (...) {
			if (password != nullptr)
				throw;
			std::string label;
			auto decoded = Botan::PEM_Code::decode(
				std::string_view(static_cast<const char *>(pem), len), label);
			if (label != "RSA PRIVATE KEY")
				throw;
			native = std::make_unique<Botan::RSA_PrivateKey>(
				Botan::AlgorithmIdentifier("RSA", Botan::AlgorithmIdentifier::USE_NULL_PARAM),
				std::span<const uint8_t>(decoded.data(), decoded.size()));
		}
		if (!supported_key(native.get()))
			return XP_CA_ERR_FORMAT;
		auto result = std::make_unique<xp_key>();
		result->native = std::move(native);
		*out = result.release();
		return XP_CA_OK;
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR_FORMAT, e.what());
	}
}

extern "C" int
xp_key_export_private_pem(xp_key_t key, xp_ca_password_callback_t password,
	                 void *context, void *out, size_t *len)
{
	if (key == nullptr || len == nullptr)
		return XP_CA_ERR;
	if (key->native == nullptr)
		return XP_CA_ERR_POLICY;
	if (!key->exportable)
		return XP_CRYPTO_ERR_NOT_EXPORTABLE;
	try {
		scrubbed_string pass;
		if (password_value(password, context, pass.value) != XP_CA_OK)
			return XP_CA_ERR;
		scrubbed_string encoded;
		if (password == nullptr)
			encoded.value = Botan::PKCS8::PEM_encode(*key->native);
		else {
			Botan::AutoSeeded_RNG rng;
			encoded.value = Botan::PKCS8::PEM_encode(*key->native, rng, pass.value);
		}
		if (out == nullptr) {
			*len = encoded.value.size();
			return XP_CA_OK;
		}
		if (*len < encoded.value.size()) {
			*len = encoded.value.size();
			return XP_CA_ERR;
		}
		std::memcpy(out, encoded.value.data(), encoded.value.size());
		*len = encoded.value.size();
		return XP_CA_OK;
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR, e.what());
	}
}

extern "C" int
xp_key_import_spki_der(xp_key_t *out, const void *der, size_t len)
{
	if (out == nullptr)
		return XP_CA_ERR;
	*out = nullptr;
	if (der == nullptr || len == 0)
		return XP_CA_ERR;
	try {
		auto native = Botan::X509::load_key(
			std::span<const uint8_t>(static_cast<const uint8_t *>(der), len));
		if (!supported_key(native.get()))
			return XP_CA_ERR_FORMAT;
		auto result = std::make_unique<xp_key>();
		result->public_native = std::move(native);
		*out = result.release();
		return XP_CA_OK;
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR_FORMAT, e.what());
	}
}

extern "C" int
xp_key_export_spki_der(xp_key_t key, void *out, size_t *len)
{
	if (!supported_key(public_key(key)) || len == nullptr)
		return XP_CA_ERR;
	try {
		auto encoded = Botan::X509::BER_encode(*public_key(key));
		if (out == nullptr) { *len = encoded.size(); return XP_CA_OK; }
		if (*len < encoded.size()) { *len = encoded.size(); return XP_CA_ERR; }
		std::memcpy(out, encoded.data(), encoded.size());
		*len = encoded.size();
		return XP_CA_OK;
	} catch (const std::exception& e) { return fail(XP_CA_ERR, e.what()); }
}

extern "C" int
xp_key_fingerprint_sha256(xp_key_t key, void *out, size_t *len)
{
	if (!supported_key(public_key(key)) || len == nullptr)
		return XP_CRYPTO_ERR_INVALID;
	if (out == nullptr) {
		*len = 32;
		return XP_CRYPTO_OK;
	}
	if (*len < 32) {
		*len = 32;
		return XP_CRYPTO_ERR_BUFFER_TOO_SMALL;
	}
	try {
		auto encoded = Botan::X509::BER_encode(*public_key(key));
		auto hash = Botan::HashFunction::create_or_throw("SHA-256");
		hash->update(encoded);
		hash->final(static_cast<uint8_t *>(out));
		*len = 32;
		return XP_CRYPTO_OK;
	} catch (const std::exception& e) {
		return fail(XP_CRYPTO_ERR, e.what());
	}
}

static int copy_bytes(const std::vector<uint8_t>& value, void *out, size_t *len)
{
	if (len == nullptr) return XP_CA_ERR;
	if (out == nullptr) { *len = value.size(); return XP_CA_OK; }
	if (*len < value.size()) { *len = value.size(); return XP_CA_ERR; }
	std::memcpy(out, value.data(), value.size()); *len = value.size(); return XP_CA_OK;
}

extern "C" int
xp_key_get_rsa_public(xp_key_t key, void *modulus, size_t *modulus_len,
	                     void *exponent, size_t *exponent_len)
{
	auto *rsa = dynamic_cast<Botan::RSA_PublicKey *>(public_key(key));
	if (rsa == nullptr) return XP_CA_ERR_POLICY;
	auto n = rsa->get_n().serialize<std::vector<uint8_t>>();
	auto e = rsa->get_e().serialize<std::vector<uint8_t>>();
	return copy_bytes(n, modulus, modulus_len) != XP_CA_OK ? XP_CA_ERR
		: copy_bytes(e, exponent, exponent_len);
}

extern "C" int
xp_key_get_ec_public(xp_key_t key, void *x, size_t *x_len,
	                    void *y, size_t *y_len)
{
	auto *ec = dynamic_cast<Botan::ECDSA_PublicKey *>(public_key(key));
	if (ec == nullptr) return XP_CA_ERR_POLICY;
	auto x_value = ec->_public_ec_point().x_bytes<std::vector<uint8_t>>();
	auto y_value = ec->_public_ec_point().y_bytes<std::vector<uint8_t>>();
	return copy_bytes(x_value, x, x_len) != XP_CA_OK ? XP_CA_ERR
		: copy_bytes(y_value, y, y_len);
}

static bool
csr_identity_valid(const struct xp_ca_identity *identity)
{
	return xp_ca_identity_is_valid(identity, true);
}

static Botan::X509_DN
identity_dn(const struct xp_ca_identity& identity)
{
	Botan::X509_DN name;
	if (identity.country) name.add_attribute("X520.Country", identity.country);
	if (identity.state_or_province) name.add_attribute("X520.State", identity.state_or_province);
	if (identity.locality) name.add_attribute("X520.Locality", identity.locality);
	if (identity.organization) name.add_attribute("X520.Organization", identity.organization);
	if (identity.organizational_unit) name.add_attribute("X520.OrganizationalUnit", identity.organizational_unit);
	if (identity.common_name) name.add_attribute("X520.CommonName", identity.common_name);
	if (identity.email_address) name.add_attribute(Botan::OID::from_string("1.2.840.113549.1.9.1"), identity.email_address);
	return name;
}

class Raw_CSR_Extension final : public Botan::Certificate_Extension {
public:
	Raw_CSR_Extension(Botan::OID oid, std::vector<uint8_t> value)
		: oid_(std::move(oid)), value_(std::move(value)) {}
	Botan::OID oid_of() const override { return oid_; }
	std::string oid_name() const override { return ""; }
	std::unique_ptr<Botan::Certificate_Extension> copy() const override {
		return std::make_unique<Raw_CSR_Extension>(oid_, value_);
	}
	bool is_appropriate_context(Botan::Extension_Context) const override {
		return true;
	}
protected:
	std::vector<uint8_t> encode_inner() const override { return value_; }
	void decode_inner(const std::vector<uint8_t>& value) override { value_ = value; }
private:
	Botan::OID oid_;
	std::vector<uint8_t> value_;
};

static int
csr_create(xp_ca_csr_t *out, xp_key_t key,
	       const struct xp_ca_identity *identity,
	       const struct xp_ca_extension *requested, size_t requested_count)
{
	if (out == nullptr)
		return XP_CA_ERR;
	*out = nullptr;
	if (key == nullptr || key->native == nullptr)
		return XP_CA_ERR;
	if ((identity != nullptr && !csr_identity_valid(identity))
	    || (requested_count != 0 && requested == nullptr))
		return XP_CA_ERR_POLICY;
	try {
		Botan::AutoSeeded_RNG rng;
		Botan::X509_DN        subject = identity == nullptr ? Botan::X509_DN()
			: identity_dn(*identity);
		Botan::Extensions     extensions;
		if (identity != nullptr && identity->dns_name_count != 0) {
			Botan::AlternativeName names;
			for (size_t i = 0; i < identity->dns_name_count; i++)
				names.add_dns(identity->dns_names[i]);
			extensions.add(std::make_unique<
				Botan::Cert_Extension::Subject_Alternative_Name>(names));
		}
		for (size_t i = 0; i < requested_count; i++) {
			if (requested[i].oid == nullptr || requested[i].value_der == nullptr
			    || requested[i].value_der_len == 0)
				return XP_CRYPTO_ERR_INVALID;
			std::vector<uint8_t> value(
				static_cast<const uint8_t *>(requested[i].value_der),
				static_cast<const uint8_t *>(requested[i].value_der)
					+ requested[i].value_der_len);
			extensions.add(std::make_unique<Raw_CSR_Extension>(
				Botan::OID::from_string(requested[i].oid), std::move(value)),
				requested[i].critical);
		}
		auto                  csr = std::make_unique<xp_ca_csr>();
		csr->native = std::make_unique<Botan::PKCS10_Request>(
			Botan::PKCS10_Request::create(*key->native, subject, extensions,
			                              key_hash(*key->native), rng,
			                              key_padding(*key->native)));
		*out = csr.release();
		return XP_CA_OK;
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR, e.what());
	}
}

extern "C" int
xp_ca_csr_create(xp_ca_csr_t *out, xp_key_t key)
{
	return csr_create(out, key, nullptr, nullptr, 0);
}

extern "C" int
xp_ca_csr_create_with_identity(xp_ca_csr_t *out, xp_key_t key,
	                           const struct xp_ca_identity *identity)
{
	return csr_create(out, key, identity, nullptr, 0);
}

extern "C" int
xp_ca_csr_create_request(xp_ca_csr_t *out, xp_key_t key,
	const struct xp_ca_csr_request *request)
{
	if (request == nullptr)
		return XP_CRYPTO_ERR_INVALID;
	return csr_create(out, key, &request->subject,
		request->extensions, request->extension_count);
}

extern "C" int
xp_ca_csr_verify(xp_ca_csr_t csr)
{
	if (csr == nullptr)
		return XP_CA_ERR;
	try {
		auto key = csr->native->subject_public_key();
		return supported_key(key.get()) && csr->native->check_signature(*key)
			? XP_CA_OK : XP_CA_ERR_VERIFY;
	} catch (...) {
		return XP_CA_ERR_VERIFY;
	}
}

extern "C" void
xp_ca_csr_free(xp_ca_csr_t csr)
{
	delete csr;
}

static Botan::Extensions
make_extensions(const struct xp_ca_issue_request *request,
                const std::vector<uint8_t>& subject_public_key,
                const Botan::X509_Certificate *issuer)
{
	Botan::Extensions      extensions;
	Botan::Key_Constraints key_usage;
	if (request->policy.key_usage & XP_CA_KEY_USE_SIGN)
		key_usage |= Botan::Key_Constraints::DigitalSignature;
	if (request->policy.key_usage & XP_CA_KEY_USE_CERT_SIGN)
		key_usage |= Botan::Key_Constraints::KeyCertSign;
	if (request->policy.key_usage & XP_CA_KEY_USE_CRL_SIGN)
		key_usage |= Botan::Key_Constraints::CrlSign;
	if (request->policy.key_usage & XP_CA_KEY_USE_KEY_ENCIPHERMENT)
		key_usage |= Botan::Key_Constraints::KeyEncipherment;
	if (request->policy.key_usage & XP_CA_KEY_USE_DATA_ENCIPHERMENT)
		key_usage |= Botan::Key_Constraints::DataEncipherment;
	if (request->policy.key_usage & XP_CA_KEY_USE_KEY_AGREEMENT)
		key_usage |= Botan::Key_Constraints::KeyAgreement;
	if (request->policy.key_usage & XP_CA_KEY_USE_NON_REPUDIATION)
		key_usage |= Botan::Key_Constraints::NonRepudiation;
	if (request->policy.key_usage & XP_CA_KEY_USE_ENCIPHER_ONLY)
		key_usage |= Botan::Key_Constraints::EncipherOnly;
	if (request->policy.key_usage & XP_CA_KEY_USE_DECIPHER_ONLY)
		key_usage |= Botan::Key_Constraints::DecipherOnly;
	std::optional<size_t> path_length;
	if (request->policy.path_length >= 0)
		path_length = (size_t)request->policy.path_length;
	extensions.add(std::make_unique<Botan::Cert_Extension::Basic_Constraints>(
					   request->policy.is_ca, path_length),
	               request->policy.basic_constraints_critical);
	extensions.add(std::make_unique<Botan::Cert_Extension::Key_Usage>(key_usage),
	               request->policy.key_usage_critical);
	auto subject_key_hash = Botan::HashFunction::create_or_throw("SHA-256");
	subject_key_hash->update(subject_public_key);
	auto subject_key_digest = subject_key_hash->final();
	Botan::Cert_Extension::Subject_Key_ID subject_key_id(
		std::vector<uint8_t>(subject_key_digest.begin(), subject_key_digest.end()));
	extensions.add(std::make_unique<Botan::Cert_Extension::Subject_Key_ID>(subject_key_id));
	if (issuer != nullptr)
		extensions.add(std::make_unique<Botan::Cert_Extension::Authority_Key_ID>(
						   issuer->subject_key_id()));
	else
		extensions.add(std::make_unique<Botan::Cert_Extension::Authority_Key_ID>(
						   subject_key_id.get_key_id()));
	if (request->subject.dns_name_count != 0) {
		Botan::AlternativeName names;
		for (size_t i = 0; i < request->subject.dns_name_count; i++)
			names.add_dns(request->subject.dns_names[i]);
		extensions.add(std::make_unique<Botan::Cert_Extension::Subject_Alternative_Name>(names));
	}
	std::vector<Botan::OID> eku;
	if (request->policy.extended_key_usage & XP_CA_EKU_SERVER_AUTH)
		eku.push_back(Botan::OID::from_string("PKIX.ServerAuth"));
	if (request->policy.extended_key_usage & XP_CA_EKU_CLIENT_AUTH)
		eku.push_back(Botan::OID::from_string("PKIX.ClientAuth"));
	if (request->policy.extended_key_usage & XP_CA_EKU_CODE_SIGNING)
		eku.push_back(Botan::OID::from_string("PKIX.CodeSigning"));
	if (request->policy.extended_key_usage & XP_CA_EKU_EMAIL_PROTECTION)
		eku.push_back(Botan::OID::from_string("1.3.6.1.5.5.7.3.4"));
	if (request->policy.extended_key_usage & XP_CA_EKU_TIME_STAMPING)
		eku.push_back(Botan::OID::from_string("1.3.6.1.5.5.7.3.8"));
	if (request->policy.extended_key_usage & XP_CA_EKU_OCSP_SIGNING)
		eku.push_back(Botan::OID::from_string("PKIX.OCSPSigning"));
	if (!eku.empty())
		extensions.add(std::make_unique<Botan::Cert_Extension::Extended_Key_Usage>(eku));
	if (request->policy.crl_distribution_point != nullptr) {
		Botan::AlternativeName                                                          uri;
		uri.add_uri(request->policy.crl_distribution_point);
		std::vector<Botan::Cert_Extension::CRL_Distribution_Points::Distribution_Point> points;
		points.emplace_back(uri);
		extensions.add(std::make_unique<Botan::Cert_Extension::CRL_Distribution_Points>(points));
	}
	return extensions;
}

static Botan::X509_DN
subject_dn(const struct xp_ca_issue_request *request)
{
	return identity_dn(request->subject);
}

static bool
issuer_can_sign(
	const Botan::X509_Certificate& issuer,
	const Botan::Private_Key& issuer_key,
	Botan::Key_Constraints::Bits required_usage)
{
	try {
		return issuer.is_CA_cert()
			&& issuer.constraints().includes(required_usage)
			&& issuer.subject_public_key_info()
			== Botan::X509::BER_encode(issuer_key);
	} catch (...) {
		return false;
	}
}

extern "C" int
xp_ca_cert_create_self_signed(xp_ca_cert_t *out, xp_key_t key,
                              const struct xp_ca_issue_request *request)
{
	if (out == nullptr)
		return XP_CA_ERR;
	*out = nullptr;
	if (key == nullptr || key->native == nullptr)
		return XP_CA_ERR;
	if (!xp_ca_issue_request_is_valid(request))
		return XP_CA_ERR_POLICY;
	try {
		Botan::AutoSeeded_RNG rng;
		Botan::PK_Signer      signer(*key->native, rng, signing_scheme(*key->native));
		auto                  public_key = Botan::X509::BER_encode(*key->native);
		auto                  not_before = Botan::ASN1_Time(
			std::chrono::system_clock::from_time_t(request->not_before));
		auto                  not_after = Botan::ASN1_Time(
			std::chrono::system_clock::from_time_t(request->not_after));
		auto                  subject = subject_dn(request);
		auto                  extensions = make_extensions(request, public_key, nullptr);
		auto                  cert = Botan::X509_CA::make_cert(
			signer,
			rng,
			signer.algorithm_identifier(),
			public_key,
			not_before,
			not_after,
			subject,
			subject,
			extensions);
		auto result = std::make_unique<xp_ca_cert>();
		result->native = std::make_unique<Botan::X509_Certificate>(std::move(cert));
		*out = result.release();
		return XP_CA_OK;
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR, e.what());
	}
}

extern "C" int
xp_ca_cert_issue(xp_ca_cert_t *out, xp_key_t issuer_key, xp_ca_cert_t issuer,
                 xp_ca_csr_t csr, const struct xp_ca_issue_request *request)
{
	if (out == nullptr)
		return XP_CA_ERR;
	*out = nullptr;
	if (issuer_key == nullptr || issuer_key->native == nullptr
	    || issuer == nullptr || csr == nullptr)
		return XP_CA_ERR;
	if (!xp_ca_issue_request_is_valid(request))
		return XP_CA_ERR_POLICY;
	if (xp_ca_csr_verify(csr) != XP_CA_OK)
		return XP_CA_ERR_VERIFY;
	if (!issuer_can_sign(*issuer->native, *issuer_key->native,
	                     Botan::Key_Constraints::KeyCertSign))
		return XP_CA_ERR_POLICY;
	try {
		if (std::chrono::system_clock::from_time_t(request->not_before)
		    < issuer->native->not_before().to_std_timepoint()
		    || std::chrono::system_clock::from_time_t(request->not_after)
		    > issuer->native->not_after().to_std_timepoint())
			return XP_CA_ERR_POLICY;
		Botan::AutoSeeded_RNG rng;
		Botan::X509_CA        ca(*issuer->native, *issuer_key->native,
		                         key_hash(*issuer_key->native),
		                         key_padding(*issuer_key->native), rng);
		auto                  public_key = csr->native->raw_public_key();
		auto                  not_before = Botan::ASN1_Time(
			std::chrono::system_clock::from_time_t(request->not_before));
		auto                  not_after = Botan::ASN1_Time(
			std::chrono::system_clock::from_time_t(request->not_after));
		auto                  extensions = make_extensions(
			request, public_key, issuer->native.get());
		auto                  cert = Botan::X509_CA::make_cert(
			ca.signature_op(),
			rng,
			ca.algorithm_identifier(),
			public_key,
			not_before,
			not_after,
			issuer->native->subject_dn(),
			subject_dn(request),
			extensions);
		auto result = std::make_unique<xp_ca_cert>();
		result->native = std::make_unique<Botan::X509_Certificate>(std::move(cert));
		*out = result.release();
		return XP_CA_OK;
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR, e.what());
	}
}

static int
export_der(const std::vector<uint8_t>& input, void *out, size_t *len)
{
	if (len == nullptr)
		return XP_CA_ERR;
	if (out == nullptr) {
		*len = input.size();
		return XP_CA_OK;
	}
	if (*len < input.size()) {
		*len = input.size();
		return XP_CA_ERR;
	}
	memcpy(out, input.data(), input.size());
	*len = input.size();
	return XP_CA_OK;
}

static bool
is_canonical_der(const std::vector<uint8_t>& canonical, const void *der,
	size_t len)
{
	return canonical.size() == len
		&& memcmp(canonical.data(), der, len) == 0;
}

extern "C" int
xp_ca_cert_import_der(xp_ca_cert_t *out, const void *der, size_t len)
{
	if (out == nullptr)
		return XP_CA_ERR;
	*out = nullptr;
	if (der == nullptr || len == 0)
		return XP_CA_ERR;
	try {
		auto cert = std::make_unique<xp_ca_cert>();
		cert->native = std::make_unique<Botan::X509_Certificate>(
			std::vector<uint8_t>((const uint8_t *)der, (const uint8_t *)der + len));
		if (!is_canonical_der(cert->native->BER_encode(), der, len))
			return XP_CA_ERR_FORMAT;
		*out = cert.release();
		return XP_CA_OK;
	} catch (...) {
		return XP_CA_ERR_FORMAT;
	}
}

extern "C" int
xp_ca_cert_export_der(xp_ca_cert_t cert, void *out, size_t *len)
{
	if (cert == nullptr)
		return XP_CA_ERR;
	try {
		return export_der(cert->native->BER_encode(), out, len);
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR, e.what());
	}
}

extern "C" int
xp_ca_cert_chain_import_pem(xp_ca_cert_t **out, size_t *count,
	                        const void *pem, size_t len)
{
	if (out == nullptr || count == nullptr)
		return XP_CA_ERR;
	*out = nullptr;
	*count = 0;
	if (pem == nullptr || len == 0)
		return XP_CA_ERR;
	try {
		std::string input(static_cast<const char *>(pem), len);
		const std::string begin = "-----BEGIN CERTIFICATE-----";
		const std::string end = "-----END CERTIFICATE-----";
		std::vector<std::unique_ptr<xp_ca_cert>> items;
		size_t position = 0;
		while (position < input.size()) {
			while (position < input.size()
			       && std::isspace(static_cast<unsigned char>(input[position])))
				position++;
			if (position == input.size())
				break;
			if (input.compare(position, begin.size(), begin) != 0)
				throw std::runtime_error("unexpected PEM object");
			size_t finish = input.find(end, position + begin.size());
			if (finish == std::string::npos)
				throw std::runtime_error("unterminated certificate PEM");
			finish += end.size();
			auto item = std::make_unique<xp_ca_cert>();
			Botan::DataSource_Memory source(
				reinterpret_cast<const uint8_t *>(input.data() + position),
				finish - position);
			item->native = std::make_unique<Botan::X509_Certificate>(source);
			items.push_back(std::move(item));
			position = finish;
		}
		if (items.empty())
			return XP_CA_ERR_FORMAT;
		auto result = std::make_unique<xp_ca_cert_t[]>(items.size());
		for (size_t i = 0; i < items.size(); i++)
			result[i] = items[i].release();
		*count = items.size();
		*out = result.release();
		return XP_CA_OK;
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR_FORMAT, e.what());
	}
}

extern "C" int
xp_ca_cert_chain_export_pem(const xp_ca_cert_t *certs, size_t count,
	                        void *out, size_t *len)
{
	if (certs == nullptr || count == 0 || len == nullptr)
		return XP_CA_ERR;
	try {
		std::string encoded;
		for (size_t i = 0; i < count; i++) {
			if (certs[i] == nullptr)
				return XP_CA_ERR;
			encoded += certs[i]->native->PEM_encode();
		}
		if (out == nullptr) {
			*len = encoded.size();
			return XP_CA_OK;
		}
		if (*len < encoded.size()) {
			*len = encoded.size();
			return XP_CA_ERR;
		}
		std::memcpy(out, encoded.data(), encoded.size());
		*len = encoded.size();
		return XP_CA_OK;
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR, e.what());
	}
}

extern "C" void
xp_ca_cert_chain_free(xp_ca_cert_t *certs, size_t count)
{
	if (certs != nullptr) {
		for (size_t i = 0; i < count; i++) delete certs[i];
		delete[] certs;
	}
}

static std::vector<uint8_t>
pkcs7_encode(const xp_ca_cert_t *certs, size_t count)
{
	std::vector<uint8_t> encoded;
	Botan::DER_Encoder encoder(encoded);
	encoder.start_sequence()
		.encode(Botan::OID::from_string("1.2.840.113549.1.7.2"))
		.start_explicit(0)
			.start_sequence()
				.encode((size_t)1)
				.start_set().end_cons()
				.start_sequence()
					.encode(Botan::OID::from_string("1.2.840.113549.1.7.1"))
				.end_cons()
				.start_context_specific(0);
	for (size_t i = 0; i < count; i++) {
		if (certs[i] == nullptr) throw std::invalid_argument("null certificate");
		auto der = certs[i]->native->BER_encode();
		encoder.raw_bytes(der);
	}
	encoder.end_cons().start_set().end_cons().end_cons().end_explicit().end_cons();
	return encoded;
}

static std::vector<std::vector<uint8_t>>
pkcs7_decode(std::span<const uint8_t> encoded)
{
	std::vector<std::vector<uint8_t>> certs;
	Botan::BER_Decoder input(encoded, Botan::BER_Decoder::Limits::DER());
	auto outer = input.start_sequence();
	Botan::OID content_type;
	outer.decode(content_type);
	if (content_type.to_string() != "1.2.840.113549.1.7.2")
		throw std::invalid_argument("not PKCS7 SignedData");
	auto explicit_data = outer.start_explicit_context_specific(0);
	auto signed_data = explicit_data.start_sequence();
	size_t version = 0; signed_data.decode(version);
	if (version < 1) throw std::invalid_argument("invalid SignedData version");
	signed_data.start_set().discard_remaining().end_cons();
	signed_data.start_sequence().discard_remaining().end_cons();
	if (!signed_data.more_items())
		throw std::invalid_argument("PKCS7 has no certificates");
	auto certificate_set = signed_data.start_context_specific(0);
	while (certificate_set.more_items()) {
		auto object = certificate_set.get_next_object();
		object.assert_is_a(Botan::ASN1_Type::Sequence, Botan::ASN1_Class::Constructed,
			"certificate");
		std::vector<uint8_t> certificate;
		Botan::DER_Encoder(certificate).start_sequence()
			.raw_bytes(object.bits(), object.length()).end_cons();
		certs.push_back(std::move(certificate));
	}
	certificate_set.end_cons();
	signed_data.discard_remaining().end_cons();
	explicit_data.end_cons(); outer.end_cons(); input.verify_end();
	if (certs.empty()) throw std::invalid_argument("empty PKCS7 certificate set");
	return certs;
}

extern "C" int
xp_ca_cert_bundle_import(xp_ca_cert_t **out, size_t *count,
	enum xp_ca_encoding encoding, const void *data, size_t len)
{
	if (out == nullptr || count == nullptr) return XP_CRYPTO_ERR_INVALID;
	*out = nullptr; *count = 0;
	if (encoding == XP_CA_ENCODING_PEM)
		return xp_ca_cert_chain_import_pem(out, count, data, len);
	if (encoding == XP_CA_ENCODING_DER) {
		xp_ca_cert_t cert = nullptr; int status = xp_ca_cert_import_der(&cert, data, len);
		if (status != XP_CA_OK) return status;
		auto result = new (std::nothrow) xp_ca_cert_t[1];
		if (result == nullptr) { xp_ca_cert_free(cert); return XP_CA_ERR; }
		result[0] = cert; *out = result; *count = 1; return XP_CA_OK;
	}
	if ((encoding != XP_CA_ENCODING_PKCS7_DER
	    && encoding != XP_CA_ENCODING_PKCS7_PEM) || data == nullptr || len == 0)
		return XP_CRYPTO_ERR_INVALID;
	try {
		std::vector<uint8_t> der;
		if (encoding == XP_CA_ENCODING_PKCS7_PEM) {
			auto decoded = Botan::PEM_Code::decode_check_label(
				std::string_view(static_cast<const char *>(data), len), "PKCS7");
			der.assign(decoded.begin(), decoded.end());
		}
		else der.assign(static_cast<const uint8_t *>(data),
			static_cast<const uint8_t *>(data) + len);
		auto values = pkcs7_decode(der);
		auto result = std::make_unique<xp_ca_cert_t[]>(values.size());
		std::fill(result.get(), result.get() + values.size(), nullptr);
		for (size_t i = 0; i < values.size(); i++) {
			int status = xp_ca_cert_import_der(&result[i], values[i].data(), values[i].size());
			if (status != XP_CA_OK) {
				for (size_t j = 0; j < values.size(); j++) delete result[j];
				return status;
			}
		}
		*out = result.release(); *count = values.size(); return XP_CA_OK;
	} catch (const std::exception& e) { return fail(XP_CA_ERR_FORMAT, e.what()); }
}

extern "C" int
xp_ca_cert_bundle_export(const xp_ca_cert_t *certs, size_t count,
	enum xp_ca_encoding encoding, void *out, size_t *len)
{
	if (encoding == XP_CA_ENCODING_PEM)
		return xp_ca_cert_chain_export_pem(certs, count, out, len);
	if (encoding == XP_CA_ENCODING_DER)
		return count == 1 ? xp_ca_cert_export_der(certs[0], out, len)
			: XP_CRYPTO_ERR_INVALID;
	if (certs == nullptr || count == 0 || len == nullptr
	    || (encoding != XP_CA_ENCODING_PKCS7_DER
	        && encoding != XP_CA_ENCODING_PKCS7_PEM))
		return XP_CRYPTO_ERR_INVALID;
	try {
		auto der = pkcs7_encode(certs, count);
		if (encoding == XP_CA_ENCODING_PKCS7_PEM) {
			auto pem = Botan::PEM_Code::encode(der, "PKCS7");
			std::vector<uint8_t> bytes(pem.begin(), pem.end());
			return copy_bytes(bytes, out, len);
		}
		return copy_bytes(der, out, len);
	} catch (const std::exception& e) { return fail(XP_CA_ERR, e.what()); }
}

extern "C" int
xp_ca_cert_get_validity(xp_ca_cert_t cert, time_t *not_before, time_t *not_after)
{
	if (cert == nullptr || not_before == nullptr || not_after == nullptr)
		return XP_CA_ERR;
	*not_before = std::chrono::system_clock::to_time_t(
		cert->native->not_before().to_std_timepoint());
	*not_after = std::chrono::system_clock::to_time_t(
		cert->native->not_after().to_std_timepoint());
	return XP_CA_OK;
}

extern "C" int
xp_ca_cert_get_public_key(xp_key_t *out, xp_ca_cert_t cert)
{
	if (out == nullptr) return XP_CA_ERR;
	*out = nullptr;
	if (cert == nullptr) return XP_CA_ERR;
	try {
		auto result = std::make_unique<xp_key>();
		result->public_native = cert->native->subject_public_key();
		if (!supported_key(result->public_native.get())) return XP_CA_ERR_FORMAT;
		*out = result.release(); return XP_CA_OK;
	} catch (const std::exception& e) { return fail(XP_CA_ERR, e.what()); }
}

extern "C" int
xp_ca_cert_verify_signature(xp_ca_cert_t cert, xp_ca_cert_t issuer)
{
	if (cert == nullptr) return XP_CRYPTO_ERR_INVALID;
	try {
		auto key = (issuer == nullptr ? cert : issuer)->native->subject_public_key();
		return cert->native->check_signature(*key) ? XP_CA_OK : XP_CA_ERR_VERIFY;
	} catch (...) { return XP_CA_ERR_VERIFY; }
}

extern "C" int
xp_ca_cert_get_info(xp_ca_cert_t cert, struct xp_ca_cert_info *info)
{
	if (cert == nullptr || info == nullptr) return XP_CRYPTO_ERR_INVALID;
	std::memset(info, 0, sizeof(*info));
	try {
		info->version = cert->native->x509_version();
		info->self_signed = cert->native->is_self_signed();
		info->is_ca = cert->native->is_CA_cert();
		auto path = cert->native->path_length_constraint();
		info->has_path_length = path.has_value();
		if (path) info->path_length = static_cast<int>(*path);
		auto usage = cert->native->constraints();
		if (usage.includes(Botan::Key_Constraints::DigitalSignature)) info->key_usage |= XP_CA_KEY_USE_SIGN;
		if (usage.includes(Botan::Key_Constraints::KeyCertSign)) info->key_usage |= XP_CA_KEY_USE_CERT_SIGN;
		if (usage.includes(Botan::Key_Constraints::CrlSign)) info->key_usage |= XP_CA_KEY_USE_CRL_SIGN;
		if (usage.includes(Botan::Key_Constraints::KeyEncipherment)) info->key_usage |= XP_CA_KEY_USE_KEY_ENCIPHERMENT;
		if (usage.includes(Botan::Key_Constraints::DataEncipherment)) info->key_usage |= XP_CA_KEY_USE_DATA_ENCIPHERMENT;
		if (usage.includes(Botan::Key_Constraints::KeyAgreement)) info->key_usage |= XP_CA_KEY_USE_KEY_AGREEMENT;
		if (usage.includes(Botan::Key_Constraints::NonRepudiation)) info->key_usage |= XP_CA_KEY_USE_NON_REPUDIATION;
		if (usage.includes(Botan::Key_Constraints::EncipherOnly)) info->key_usage |= XP_CA_KEY_USE_ENCIPHER_ONLY;
		if (usage.includes(Botan::Key_Constraints::DecipherOnly)) info->key_usage |= XP_CA_KEY_USE_DECIPHER_ONLY;
		for (const auto& oid : cert->native->extended_key_usage()) {
			auto value = oid.to_string();
			if (value == "1.3.6.1.5.5.7.3.1") info->extended_key_usage |= XP_CA_EKU_SERVER_AUTH;
			else if (value == "1.3.6.1.5.5.7.3.2") info->extended_key_usage |= XP_CA_EKU_CLIENT_AUTH;
			else if (value == "1.3.6.1.5.5.7.3.3") info->extended_key_usage |= XP_CA_EKU_CODE_SIGNING;
			else if (value == "1.3.6.1.5.5.7.3.4") info->extended_key_usage |= XP_CA_EKU_EMAIL_PROTECTION;
			else if (value == "1.3.6.1.5.5.7.3.8") info->extended_key_usage |= XP_CA_EKU_TIME_STAMPING;
			else if (value == "1.3.6.1.5.5.7.3.9") info->extended_key_usage |= XP_CA_EKU_OCSP_SIGNING;
		}
		return XP_CA_OK;
	} catch (const std::exception& e) { return fail(XP_CA_ERR, e.what()); }
}

extern "C" int
xp_ca_cert_get_serial(xp_ca_cert_t cert, void *out, size_t *len)
{
	if (cert == nullptr) return XP_CRYPTO_ERR_INVALID;
	return copy_bytes(cert->native->serial_number(), out, len);
}

extern "C" int
xp_ca_cert_get_fingerprint(xp_ca_cert_t cert,
	enum xp_digest_algorithm digest, void *out, size_t *len)
{
	if (cert == nullptr) return XP_CRYPTO_ERR_INVALID;
	try {
		auto encoded = cert->native->BER_encode();
		return xp_digest(digest, encoded.data(), encoded.size(), out, len);
	} catch (const std::exception& e) { return fail(XP_CA_ERR, e.what()); }
}

static const char *
botan_name_field(enum xp_ca_name_field field)
{
	switch (field) {
		case XP_CA_NAME_COUNTRY: return "X520.Country";
		case XP_CA_NAME_STATE_OR_PROVINCE: return "X520.State";
		case XP_CA_NAME_LOCALITY: return "X520.Locality";
		case XP_CA_NAME_ORGANIZATION: return "X520.Organization";
		case XP_CA_NAME_ORGANIZATIONAL_UNIT: return "X520.OrganizationalUnit";
		case XP_CA_NAME_COMMON_NAME: return "X520.CommonName";
		case XP_CA_NAME_EMAIL_ADDRESS: return "1.2.840.113549.1.9.1";
		default: return nullptr;
	}
}

static const Botan::X509_DN *
botan_certificate_name(xp_ca_cert_t cert, enum xp_ca_name_kind kind)
{
	if (cert == nullptr) return nullptr;
	return kind == XP_CA_NAME_SUBJECT ? &cert->native->subject_dn()
		: kind == XP_CA_NAME_ISSUER ? &cert->native->issuer_dn() : nullptr;
}

extern "C" int
xp_ca_cert_get_name_count(xp_ca_cert_t cert, enum xp_ca_name_kind kind,
	enum xp_ca_name_field field, size_t *count)
{
	auto name = botan_certificate_name(cert, kind); auto attribute = botan_name_field(field);
	if (name == nullptr || attribute == nullptr || count == nullptr) return XP_CRYPTO_ERR_INVALID;
	try { *count = name->get_attribute(attribute).size(); return XP_CA_OK; }
	catch (...) { return XP_CA_ERR_FORMAT; }
}

extern "C" int
xp_ca_cert_get_name(xp_ca_cert_t cert, enum xp_ca_name_kind kind,
	enum xp_ca_name_field field, size_t index, void *out, size_t *len)
{
	auto name = botan_certificate_name(cert, kind); auto attribute = botan_name_field(field);
	if (name == nullptr || attribute == nullptr) return XP_CRYPTO_ERR_INVALID;
	try {
		auto values = name->get_attribute(attribute);
		if (index >= values.size()) return XP_CRYPTO_ERR_NOT_FOUND;
		std::vector<uint8_t> value(values[index].begin(), values[index].end());
		return copy_bytes(value, out, len);
	} catch (...) { return XP_CA_ERR_FORMAT; }
}

static std::optional<std::vector<std::string>>
botan_string_sans(const Botan::AlternativeName& names, enum xp_ca_san_type type)
{
	std::vector<std::string> result;
	if (type == XP_CA_SAN_DNS) {
		for (const auto& name : names.dns_names())
			result.push_back(name.to_string());
	}
	else if (type == XP_CA_SAN_EMAIL) {
		for (const auto& address : names.email_addresses())
			result.push_back(address.to_string());
	}
	else if (type == XP_CA_SAN_URI) {
		for (const auto& uri : names.uri_names())
			result.push_back(uri.original_input());
	}
	else
		return std::nullopt;
	return result;
}

static std::vector<std::vector<uint8_t>>
botan_ip_sans(xp_ca_cert_t cert)
{
	std::vector<std::vector<uint8_t>> addresses;
	const auto& names = cert->native->subject_alt_name();
	for (const auto& address : names.ipv4_addresses()) {
		auto bytes = address.to_bytes();
		addresses.emplace_back(bytes.begin(), bytes.end());
	}
	for (const auto& address : names.ipv6_addresses()) {
		auto bytes = address.address();
		addresses.emplace_back(bytes.begin(), bytes.end());
	}
	return addresses;
}

extern "C" int
xp_ca_cert_get_san_count(xp_ca_cert_t cert, enum xp_ca_san_type type,
	size_t *count)
{
	if (cert == nullptr || count == nullptr) return XP_CRYPTO_ERR_INVALID;
	const auto& names = cert->native->subject_alt_name();
	if (auto values = botan_string_sans(names, type)) *count = values->size();
	else if (type == XP_CA_SAN_IP_ADDRESS)
		*count = botan_ip_sans(cert).size();
	else return XP_CRYPTO_ERR_INVALID;
	return XP_CA_OK;
}

extern "C" int
xp_ca_cert_get_san(xp_ca_cert_t cert, enum xp_ca_san_type type,
	size_t index, void *out, size_t *len)
{
	if (cert == nullptr) return XP_CRYPTO_ERR_INVALID;
	const auto& names = cert->native->subject_alt_name();
	if (auto values = botan_string_sans(names, type)) {
		if (index >= values->size()) return XP_CRYPTO_ERR_NOT_FOUND;
		auto item = values->begin(); std::advance(item, index);
		std::vector<uint8_t> value(item->begin(), item->end()); return copy_bytes(value, out, len);
	}
	if (type != XP_CA_SAN_IP_ADDRESS) return XP_CRYPTO_ERR_INVALID;
	auto addresses = botan_ip_sans(cert);
	if (index >= addresses.size()) return XP_CRYPTO_ERR_NOT_FOUND;
	return copy_bytes(addresses[index], out, len);
}

extern "C" void
xp_ca_cert_free(xp_ca_cert_t cert)
{
	delete cert;
}

extern "C" int
xp_ca_csr_import_der(xp_ca_csr_t *out, const void *der, size_t len)
{
	if (out == nullptr)
		return XP_CA_ERR;
	*out = nullptr;
	if (der == nullptr || len == 0)
		return XP_CA_ERR;
	try {
		auto csr = std::make_unique<xp_ca_csr>();
		csr->native = std::make_unique<Botan::PKCS10_Request>(
			std::vector<uint8_t>((const uint8_t *)der, (const uint8_t *)der + len));
		if (!is_canonical_der(csr->native->BER_encode(), der, len))
			return XP_CA_ERR_FORMAT;
		*out = csr.release();
		return XP_CA_OK;
	} catch (...) {
		return XP_CA_ERR_FORMAT;
	}
}

extern "C" int
xp_ca_csr_export_der(xp_ca_csr_t csr, void *out, size_t *len)
{
	if (csr == nullptr)
		return XP_CA_ERR;
	try {
		return export_der(csr->native->BER_encode(), out, len);
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR, e.what());
	}
}

extern "C" int
xp_ca_csr_get_public_key(xp_key_t *out, xp_ca_csr_t csr)
{
	if (out == nullptr) return XP_CRYPTO_ERR_INVALID;
	*out = nullptr;
	if (csr == nullptr) return XP_CRYPTO_ERR_INVALID;
	try {
		auto result = std::make_unique<xp_key>();
		result->public_native = csr->native->subject_public_key();
		if (!supported_key(result->public_native.get())) return XP_CA_ERR_FORMAT;
		*out = result.release(); return XP_CA_OK;
	} catch (const std::exception& e) { return fail(XP_CA_ERR, e.what()); }
}

extern "C" int
xp_ca_csr_import(xp_ca_csr_t *out, enum xp_ca_encoding encoding,
	const void *data, size_t len)
{
	if (encoding == XP_CA_ENCODING_DER)
		return xp_ca_csr_import_der(out, data, len);
	if (out == nullptr || encoding != XP_CA_ENCODING_PEM || data == nullptr
	    || len == 0) return XP_CRYPTO_ERR_INVALID;
	*out = nullptr;
	try {
		auto decoded = Botan::PEM_Code::decode_check_label(
			std::string_view(static_cast<const char *>(data), len),
			"CERTIFICATE REQUEST");
		auto result = std::make_unique<xp_ca_csr>();
		result->native = std::make_unique<Botan::PKCS10_Request>(
			std::vector<uint8_t>(decoded.begin(), decoded.end()));
		*out = result.release(); return XP_CA_OK;
	} catch (...) { return XP_CA_ERR_FORMAT; }
}

extern "C" int
xp_ca_csr_export(xp_ca_csr_t csr, enum xp_ca_encoding encoding,
	void *out, size_t *len)
{
	if (encoding == XP_CA_ENCODING_DER)
		return xp_ca_csr_export_der(csr, out, len);
	if (csr == nullptr || encoding != XP_CA_ENCODING_PEM || len == nullptr)
		return XP_CRYPTO_ERR_INVALID;
	try {
		auto value = csr->native->PEM_encode();
		if (out == nullptr) { *len = value.size(); return XP_CA_OK; }
		if (*len < value.size()) { *len = value.size(); return XP_CRYPTO_ERR_BUFFER_TOO_SMALL; }
		std::memcpy(out, value.data(), value.size()); *len = value.size(); return XP_CA_OK;
	} catch (const std::exception& e) { return fail(XP_CA_ERR, e.what()); }
}

static Botan::X509_CRL
make_scoped_crl(
	Botan::X509_CA& ca,
	const Botan::X509_Certificate& issuer,
	const std::vector<Botan::CRL_Entry>& entries,
	uint32_t crl_number,
	Botan::RandomNumberGenerator& rng,
	const struct xp_ca_crl_request *request)
{
	Botan::Extensions extensions;
	extensions.add(
		std::make_unique<Botan::Cert_Extension::Authority_Key_ID>(
			issuer.subject_key_id()));
	extensions.add(
		std::make_unique<Botan::Cert_Extension::CRL_Number>(crl_number));
	extensions.add(
		std::make_unique<Botan::Cert_Extension::CRL_Issuing_Distribution_Point>(
			Botan::Cert_Extension::DistributionPointName([&] {
				Botan::AlternativeName names;
				names.add_uri(request->issuing_distribution_point);
				return names;
			}())),
		true);

	std::vector<uint8_t> to_be_signed;
	Botan::DER_Encoder encoder(to_be_signed);
	encoder.start_sequence()
		.encode((size_t)1)
		.encode(ca.algorithm_identifier())
		.encode(issuer.subject_dn())
		.encode(Botan::X509_Time(
			std::chrono::system_clock::from_time_t(request->this_update)))
		.encode(Botan::X509_Time(
			std::chrono::system_clock::from_time_t(request->next_update)));
	if (!entries.empty())
		encoder.start_sequence().encode_list(entries).end_cons();
	encoder.start_explicit(0)
		.start_sequence()
		.encode(extensions)
		.end_cons()
		.end_explicit()
		.end_cons();

	auto encoded = Botan::X509_Object::make_signed(
		ca.signature_op(), rng, ca.algorithm_identifier(), to_be_signed);
	return Botan::X509_CRL(encoded);
}

extern "C" int
xp_ca_crl_create(xp_ca_crl_t *out, xp_key_t issuer_key, xp_ca_cert_t issuer,
	             xp_ca_crl_t previous,
	             const struct xp_ca_crl_request *request)
{
	if (out == nullptr)
		return XP_CA_ERR;
	*out = nullptr;
	if (issuer_key == nullptr || issuer_key->native == nullptr || issuer == nullptr)
		return XP_CA_ERR;
	if (!xp_ca_crl_request_is_valid(request))
		return XP_CA_ERR_POLICY;
	if (!issuer_can_sign(*issuer->native, *issuer_key->native,
	                     Botan::Key_Constraints::CrlSign))
		return XP_CA_ERR_POLICY;
	try {
		auto issuer_public_key = issuer->native->subject_public_key();
		if (previous != nullptr
		    && (previous->native->issuer_dn() != issuer->native->subject_dn()
		        || previous->native->authority_key_id() != issuer->native->subject_key_id()
		        || !crl_has_exact_issuing_distribution_point(
		            *previous->native,
		            request->issuing_distribution_point)
		        || !previous->native->check_signature(*issuer_public_key)))
			return XP_CA_ERR_VERIFY;
		Botan::AutoSeeded_RNG         rng;
		Botan::X509_CA                ca(*issuer->native, *issuer_key->native,
		                                         key_hash(*issuer_key->native),
		                                         key_padding(*issuer_key->native), rng);
		std::vector<Botan::CRL_Entry> entries = previous == nullptr
			? std::vector<Botan::CRL_Entry>()
			: previous->native->get_revoked();
		for (size_t i = 0; i < request->newly_revoked_count; i++) {
			if (request->newly_revoked[i] == nullptr
			    || request->newly_revoked[i]->native->issuer_dn()
		        != issuer->native->subject_dn()
			    || !request->newly_revoked[i]->native->check_signature(
			        *issuer_public_key)
			    || !certificate_has_crl_distribution_point(
			        *request->newly_revoked[i]->native,
			        request->issuing_distribution_point))
				return XP_CA_ERR_VERIFY;
			entries.emplace_back(*request->newly_revoked[i]->native);
		}
		uint32_t crl_number = previous == nullptr
			? 1
			: ::crl_number(*previous->native) + 1;
		if (previous != nullptr && crl_number == 0)
			return XP_CA_ERR_POLICY;
		Botan::X509_CRL crl = make_scoped_crl(
			ca, *issuer->native, entries, crl_number, rng, request);
		auto                          result = std::make_unique<xp_ca_crl>();
		result->native = std::make_unique<Botan::X509_CRL>(std::move(crl));
		*out = result.release();
		return XP_CA_OK;
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR, e.what());
	}
}

extern "C" int
xp_ca_crl_import_der(xp_ca_crl_t *out, const void *der, size_t len)
{
	if (out == nullptr)
		return XP_CA_ERR;
	*out = nullptr;
	if (der == nullptr || len == 0)
		return XP_CA_ERR;
	try {
		auto crl = std::make_unique<xp_ca_crl>();
		crl->native = std::make_unique<Botan::X509_CRL>(
			std::vector<uint8_t>((const uint8_t *)der, (const uint8_t *)der + len));
		if (!is_canonical_der(crl->native->BER_encode(), der, len))
			return XP_CA_ERR_FORMAT;
		*out = crl.release();
		return XP_CA_OK;
	} catch (...) {
		return XP_CA_ERR_FORMAT;
	}
}

extern "C" int
xp_ca_crl_export_der(xp_ca_crl_t crl, void *out, size_t *len)
{
	if (crl == nullptr)
		return XP_CA_ERR;
	try {
		return export_der(crl->native->BER_encode(), out, len);
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR, e.what());
	}
}

extern "C" int
xp_ca_crl_get_info(xp_ca_crl_t crl, struct xp_ca_crl_info *info)
{
	if (crl == nullptr || info == nullptr)
		return XP_CA_ERR;
	try {
		info->number = crl_number(*crl->native);
		info->this_update = std::chrono::system_clock::to_time_t(
			crl->native->this_update().to_std_timepoint());
		if (!crl->native->next_update().time_is_set())
			return fail(XP_CA_ERR_FORMAT, "CRL has no nextUpdate");
		info->next_update = std::chrono::system_clock::to_time_t(
			crl->native->next_update().to_std_timepoint());
		return XP_CA_OK;
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR_FORMAT, e.what());
	}
}

extern "C" void
xp_ca_crl_free(xp_ca_crl_t crl)
{
	delete crl;
}

static bool
dns_names_equal(std::string_view first, std::string_view second)
{
	if (first.size() != second.size())
		return false;
	for (size_t i = 0; i < first.size(); i++) {
		if (std::tolower((unsigned char)first[i])
		    != std::tolower((unsigned char)second[i]))
			return false;
	}
	return true;
}

static bool
certificate_matches_identity(const Botan::X509_Certificate& cert,
	const struct xp_ca_identity *identity)
{
	if (identity == nullptr
	    || (identity->dns_name_count != 0 && identity->dns_names == nullptr))
		return false;
	bool san_extension_present = cert.v3_extensions().extension_set(
		Botan::Cert_Extension::Subject_Alternative_Name::static_oid());
	if (san_extension_present != (identity->dns_name_count != 0))
		return false;
	const auto& alternative_names = cert.subject_alt_name();
	const auto& dns_names = alternative_names.dns_names();
	if (alternative_names.count() != identity->dns_name_count
	    || dns_names.size() != identity->dns_name_count)
		return false;
	for (size_t expected = 0; expected < identity->dns_name_count; expected++) {
		if (identity->dns_names[expected] == nullptr)
			return false;
		for (size_t earlier = 0; earlier < expected; earlier++) {
			if (dns_names_equal(identity->dns_names[earlier],
			                    identity->dns_names[expected]))
				return false;
		}
		bool found = false;
		for (const auto& actual : dns_names) {
			if (dns_names_equal(actual.to_string(), identity->dns_names[expected])) {
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}
	if (identity->common_name != nullptr) {
		auto common_names = cert.subject_dn().get_attribute("X520.CommonName");
		if (common_names.size() != 1 || common_names[0] != identity->common_name)
			return false;
	}
	return true;
}

static bool
crl_scope_matches_certificate(
	const Botan::X509_CRL& crl,
	const Botan::X509_Certificate& certificate)
{
	const auto& extensions = crl.extensions();
	const auto idp_oid = crl_issuing_distribution_point_oid();
	if (!extensions.extension_set(idp_oid))
		return true;
	if (!extensions.critical_extension_set(idp_oid))
		return false;
	const auto idp = crl_issuing_distribution_point(crl);
	if (!idp.has_value())
		return false;
	for (const auto& scope : idp->uri_names()) {
		if (certificate_has_crl_distribution_point(
		    certificate, scope.original_input()))
			return true;
	}
	return false;
}

static std::vector<std::optional<Botan::X509_CRL>>
select_applicable_crls(
	const std::vector<Botan::X509_Certificate>& path,
	const xp_ca_crl_t *crls,
	size_t crl_count)
{
	std::vector<std::optional<Botan::X509_CRL>> selected(path.size());
	for (size_t certificate_index = 0;
	     certificate_index + 1 < path.size();
	     certificate_index++) {
		const auto& certificate = path[certificate_index];
		const auto& issuer = path[certificate_index + 1];
		for (size_t crl_index = 0; crl_index < crl_count; crl_index++) {
			const auto& candidate = *crls[crl_index]->native;
			if (candidate.issuer_dn() != issuer.subject_dn()
			    || candidate.authority_key_id() != issuer.subject_key_id()
			    || !crl_scope_matches_certificate(candidate, certificate))
				continue;
			if (!selected[certificate_index].has_value()
			    || crl_number(candidate)
		        > crl_number(*selected[certificate_index])
			    || (crl_number(candidate)
		            == crl_number(*selected[certificate_index])
		        && candidate.this_update()
		            > selected[certificate_index]->this_update()))
				selected[certificate_index] = candidate;
		}
	}
	return selected;
}

static int
validate_crls(
	const std::vector<Botan::X509_Certificate>& path,
	const xp_ca_crl_t *crls,
	size_t crl_count,
	bool require_crl,
	std::chrono::system_clock::time_point when)
{
	if (path.size() < 2)
		return fail(XP_CA_ERR_VERIFY, "validated certificate path has no issuer");
	auto selected = select_applicable_crls(path, crls, crl_count);
	if (require_crl) {
		for (size_t i = 0; i + 1 < path.size(); i++) {
			if (!selected[i].has_value())
				return fail(XP_CA_ERR_VERIFY, "no applicable CRL for certificate");
		}
	}
	if (crl_count == 0)
		return XP_CA_OK;

	auto statuses = Botan::PKIX::check_crl(path, selected, when);
	auto overall = Botan::PKIX::overall_status(statuses);
	if (overall != Botan::Certificate_Status_Code::OK)
		return fail(XP_CA_ERR_VERIFY, Botan::to_string(overall));
	return XP_CA_OK;
}

extern "C" int
xp_ca_cert_validate(xp_ca_cert_t leaf, const xp_ca_cert_t *chain, size_t chain_count,
                    xp_ca_cert_t root, const xp_ca_crl_t *crls, size_t crl_count,
                    const struct xp_ca_validation_policy *policy)
{
	if (leaf == nullptr || root == nullptr
	    || (chain_count != 0 && chain == nullptr)
	    || (crl_count != 0 && crls == nullptr))
		return XP_CA_ERR;
	try {
		Botan::Certificate_Store_In_Memory store;
		store.add_certificate(*root->native);
		std::vector<Botan::X509_Certificate> path;
		path.push_back(*leaf->native);
		for (size_t i = 0; i < chain_count; i++) {
			if (chain[i] == nullptr)
				return XP_CA_ERR;
			path.push_back(*chain[i]->native);
		}
		for (size_t i = 0; i < crl_count; i++) {
			if (crls[i] == nullptr)
				return XP_CA_ERR;
		}
		if (policy != nullptr && policy->expected_identity != nullptr
		    && !certificate_matches_identity(*leaf->native,
			policy->expected_identity))
			return XP_CA_ERR_VERIFY;
		if (policy != nullptr && policy->required_policy != nullptr) {
			const auto *required = policy->required_policy;
			const auto& extensions = leaf->native->v3_extensions();
			if (!extensions.extension_set(
			        Botan::Cert_Extension::Basic_Constraints::static_oid())
			    || !extensions.extension_set(
			        Botan::Cert_Extension::Key_Usage::static_oid()))
				return XP_CA_ERR_VERIFY;
			if (leaf->native->is_CA_cert() != required->is_ca)
				return XP_CA_ERR_VERIFY;
			std::optional<size_t> required_path_length;
			if (required->path_length >= 0)
				required_path_length = (size_t)required->path_length;
			if (leaf->native->path_length_constraint() != required_path_length)
				return XP_CA_ERR_VERIFY;
			if (leaf->native->is_critical("X509v3.BasicConstraints")
			    != required->basic_constraints_critical)
				return XP_CA_ERR_VERIFY;
			if (leaf->native->is_critical("X509v3.KeyUsage")
			    != required->key_usage_critical)
				return XP_CA_ERR_VERIFY;
			Botan::Key_Constraints expected_usage;
			if (required->key_usage & XP_CA_KEY_USE_SIGN)
				expected_usage |= Botan::Key_Constraints::DigitalSignature;
			if (required->key_usage & XP_CA_KEY_USE_CERT_SIGN)
				expected_usage |= Botan::Key_Constraints::KeyCertSign;
			if (required->key_usage & XP_CA_KEY_USE_CRL_SIGN)
				expected_usage |= Botan::Key_Constraints::CrlSign;
			if (leaf->native->constraints() != expected_usage)
				return XP_CA_ERR_VERIFY;
			unsigned actual_eku = XP_CA_EKU_NONE;
			if (leaf->native->has_ex_constraint("PKIX.ServerAuth"))
				actual_eku |= XP_CA_EKU_SERVER_AUTH;
			if (leaf->native->has_ex_constraint("PKIX.ClientAuth"))
				actual_eku |= XP_CA_EKU_CLIENT_AUTH;
			size_t known_eku_count = (actual_eku & XP_CA_EKU_SERVER_AUTH ? 1 : 0)
				+ (actual_eku & XP_CA_EKU_CLIENT_AUTH ? 1 : 0);
			bool eku_extension_present = extensions.extension_set(
				Botan::Cert_Extension::Extended_Key_Usage::static_oid());
			if (eku_extension_present
			        != (required->extended_key_usage != XP_CA_EKU_NONE)
			    || actual_eku != required->extended_key_usage
			    || leaf->native->extended_key_usage().size() != known_eku_count)
				return XP_CA_ERR_VERIFY;
			bool distribution_point_extension_present = extensions.extension_set(
				Botan::Cert_Extension::CRL_Distribution_Points::static_oid());
			if (distribution_point_extension_present
			        != (required->crl_distribution_point != nullptr)) {
				return XP_CA_ERR_VERIFY;
			} else if (!certificate_has_exact_crl_distribution_point(
			               *leaf->native,
			               required->crl_distribution_point)) {
				return XP_CA_ERR_VERIFY;
			}
		}
		auto                                when = policy != nullptr && policy->validation_time != 0
		    ? std::chrono::system_clock::from_time_t(policy->validation_time)
		    : std::chrono::system_clock::now();
		Botan::Path_Validation_Restrictions restrictions(false);
		auto result = Botan::x509_path_validate(path, restrictions, store,
			"", Botan::Usage_Type::UNSPECIFIED, when);
		if (!result.successful_validation())
			return fail(XP_CA_ERR_VERIFY, result.result_string().c_str());
		return validate_crls(
			result.cert_path(), crls, crl_count,
			policy != nullptr && policy->require_crl, when);
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR_VERIFY, e.what());
	}
}

static const char *
signature_scheme(enum xp_sign_algorithm algorithm)
{
	switch (algorithm) {
		case XP_SIGN_RSA_PKCS1_SHA256: return "EMSA3(SHA-256)";
		case XP_SIGN_RSA_PKCS1_SHA384: return "EMSA3(SHA-384)";
		case XP_SIGN_RSA_PKCS1_SHA512: return "EMSA3(SHA-512)";
		case XP_SIGN_ECDSA_SHA256: return "SHA-256";
		case XP_SIGN_ECDSA_SHA384: return "SHA-384";
		case XP_SIGN_ECDSA_SHA512: return "SHA-512";
		case XP_SIGN_ED25519: return "Pure";
		default: return nullptr;
	}
}

static enum xp_key_algorithm
signature_family(enum xp_sign_algorithm algorithm)
{
	if (algorithm >= XP_SIGN_RSA_PKCS1_SHA256
	    && algorithm <= XP_SIGN_RSA_PKCS1_SHA512)
		return XP_KEY_RSA;
	if (algorithm >= XP_SIGN_ECDSA_SHA256
	    && algorithm <= XP_SIGN_ECDSA_SHA512)
		return XP_KEY_ECDSA;
	return XP_KEY_ED25519;
}

static Botan::Signature_Format
signature_format(enum xp_sign_algorithm algorithm, enum xp_signature_encoding format)
{
	return signature_family(algorithm) == XP_KEY_ECDSA
	    && format == XP_SIGNATURE_ENCODING_STANDARD
		? Botan::Signature_Format::DerSequence
		: Botan::Signature_Format::Standard;
}

extern "C" int
xp_sign(xp_key_t key, enum xp_sign_algorithm algorithm,
	enum xp_signature_encoding format, const void *data, size_t data_len,
	void *signature, size_t *signature_len)
{
	if (key == nullptr || key->native == nullptr || signature_len == nullptr
	    || (data == nullptr && data_len != 0)
	    || signature_scheme(algorithm) == nullptr
	    || (format != XP_SIGNATURE_ENCODING_STANDARD && format != XP_SIGNATURE_ENCODING_P1363)
	    || (format == XP_SIGNATURE_ENCODING_P1363
	        && signature_family(algorithm) != XP_KEY_ECDSA))
		return XP_CA_ERR_POLICY;
	struct xp_key_info info;
	if (xp_key_get_info(key, &info) != XP_CA_OK
	    || info.spec.algorithm != signature_family(algorithm))
		return XP_CA_ERR_POLICY;
	try {
		Botan::AutoSeeded_RNG rng;
		Botan::PK_Signer signer(*key->native, rng, signature_scheme(algorithm),
		                        signature_format(algorithm, format));
		if (signature == nullptr) {
			*signature_len = signer.signature_length();
			return XP_CA_OK;
		}
		auto encoded = signer.sign_message(
			static_cast<const uint8_t *>(data), data_len, rng);
		if (*signature_len < encoded.size()) {
			*signature_len = encoded.size();
			return XP_CA_ERR;
		}
		std::memcpy(signature, encoded.data(), encoded.size());
		*signature_len = encoded.size();
		return XP_CA_OK;
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR, e.what());
	}
}

extern "C" int
xp_verify(xp_key_t key, enum xp_sign_algorithm algorithm,
	enum xp_signature_encoding format, const void *data, size_t data_len,
	const void *signature, size_t signature_len)
{
	auto *native = public_key(key);
	if (!supported_key(native) || signature == nullptr
	    || (data == nullptr && data_len != 0)
	    || signature_scheme(algorithm) == nullptr
	    || (format != XP_SIGNATURE_ENCODING_STANDARD && format != XP_SIGNATURE_ENCODING_P1363)
	    || (format == XP_SIGNATURE_ENCODING_P1363
	        && signature_family(algorithm) != XP_KEY_ECDSA))
		return XP_CA_ERR_POLICY;
	struct xp_key_info info;
	if (xp_key_get_info(key, &info) != XP_CA_OK
	    || info.spec.algorithm != signature_family(algorithm))
		return XP_CA_ERR_POLICY;
	if (format == XP_SIGNATURE_ENCODING_P1363
	    && signature_len != 2u * ((info.spec.bits + 7u) / 8u))
		return XP_CA_ERR_FORMAT;
	try {
		Botan::PK_Verifier verifier(*native, signature_scheme(algorithm),
		                            signature_format(algorithm, format));
		return verifier.verify_message(static_cast<const uint8_t *>(data), data_len,
			static_cast<const uint8_t *>(signature), signature_len)
			? XP_CA_OK : XP_CA_ERR_VERIFY;
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR_VERIFY, e.what());
	}
}

#if defined(XPTLS_BOTAN_HAS_PKCS11)
static int
p11_status(Botan::PKCS11::ReturnValue value)
{
	using ReturnValue = Botan::PKCS11::ReturnValue;
	switch (value) {
		case ReturnValue::PinIncorrect:
		case ReturnValue::PinInvalid:
		case ReturnValue::PinExpired:
		case ReturnValue::PinLocked:
		case ReturnValue::UserNotLoggedIn:
		case ReturnValue::UserPinNotInitialized:
			return XP_CRYPTO_ERR_AUTHORIZATION;
		case ReturnValue::TokenNotPresent:
		case ReturnValue::DeviceRemoved:
		case ReturnValue::ObjectHandleInvalid:
		case ReturnValue::KeyHandleInvalid:
			return XP_CRYPTO_ERR_NOT_FOUND;
		case ReturnValue::SessionReadOnly:
		case ReturnValue::TokenWriteProtected:
		case ReturnValue::AttributeReadOnly:
		case ReturnValue::ActionProhibited:
			return XP_CRYPTO_ERR_READ_ONLY;
		case ReturnValue::MechanismInvalid:
		case ReturnValue::MechanismParamInvalid:
		case ReturnValue::KeyTypeInconsistent:
		case ReturnValue::KeySizeRange:
		case ReturnValue::TemplateInconsistent:
		case ReturnValue::FunctionNotSupported:
			return XP_CRYPTO_ERR_UNSUPPORTED;
		case ReturnValue::SessionCount:
		case ReturnValue::DeviceMemory:
			return XP_CRYPTO_ERR_BUSY;
		default:
			return XP_CRYPTO_ERR;
	}
}
#endif

extern "C" int
xp_key_provider_store_query(const struct xp_key_store_config *store,
	const struct xp_key_spec *spec, struct xp_key_store_capabilities *capabilities)
{
	if (store == nullptr || capabilities == nullptr)
		return XP_CRYPTO_ERR_INVALID;
	std::memset(capabilities, 0, sizeof(*capabilities));
	capabilities->kind = store->store != nullptr && std::strcmp(store->store, "pkcs11") == 0
		? XP_KEY_STORE_PKCS11
		: store->store != nullptr && std::strcmp(store->store, "tpm2") == 0
		? XP_KEY_STORE_TPM2 : XP_KEY_STORE_PLATFORM;
	capabilities->availability_status = XP_CRYPTO_ERR_UNAVAILABLE;
	if (capabilities->kind != XP_KEY_STORE_PKCS11)
		return XP_CRYPTO_OK;
#if defined(XPTLS_BOTAN_HAS_PKCS11)
	try {
		if (store->store_uri == nullptr
		    || std::strstr(store->store_uri, "pin-value=") != nullptr)
			return XP_CRYPTO_OK;
		auto uri_value = [](const char *uri, const char *name) -> std::string {
			const size_t name_len = std::strlen(name);
			for (const char *p = uri; (p = std::strstr(p, name)) != nullptr; ++p) {
				if ((p == uri || p[-1] == ':' || p[-1] == ';' || p[-1] == '?'
				    || p[-1] == '&') && p[name_len] == '=') {
					const char *begin = p + name_len + 1;
					const char *end = begin;
					while (*end != 0 && *end != ';' && *end != '&') ++end;
					return std::string(begin, end);
				}
			}
			return {};
		};
		std::string module_path = uri_value(store->store_uri, "module-path");
		if (module_path.empty())
			return XP_CRYPTO_OK;
		Botan::PKCS11::Module module(module_path);
		auto slots = Botan::PKCS11::Slot::get_available_slots(module, true);
		if (slots.empty())
			return XP_CRYPTO_OK;
		std::string slot_text = uri_value(store->store_uri, "slot-id");
		Botan::PKCS11::SlotId selected = slots.front();
		if (!slot_text.empty()) {
			selected = static_cast<Botan::PKCS11::SlotId>(std::stoul(slot_text));
			if (std::find(slots.begin(), slots.end(), selected) == slots.end())
				return XP_CRYPTO_OK;
		}
		Botan::PKCS11::Slot slot(module, selected);
		auto mechanisms = slot.get_mechanism_list();
		Botan::PKCS11::MechanismType generate;
		Botan::PKCS11::MechanismType sign;
		if (spec->algorithm == XP_KEY_RSA) {
			generate = Botan::PKCS11::MechanismType::RsaPkcsKeyPairGen;
			sign = Botan::PKCS11::MechanismType::RsaPkcs;
		}
		else if (spec->algorithm == XP_KEY_ECDSA) {
			generate = Botan::PKCS11::MechanismType::EcKeyPairGen;
			sign = Botan::PKCS11::MechanismType::Ecdsa;
		}
		else {
			capabilities->availability_status = XP_CRYPTO_ERR_UNSUPPORTED;
			return XP_CRYPTO_OK;
		}
		if (std::find(mechanisms.begin(), mechanisms.end(), generate)
		        == mechanisms.end()
		    || std::find(mechanisms.begin(), mechanisms.end(), sign)
		        == mechanisms.end()) {
			capabilities->availability_status = XP_CRYPTO_ERR_UNSUPPORTED;
			return XP_CRYPTO_OK;
		}
		capabilities->available = true;
		capabilities->availability_status = XP_CRYPTO_OK;
		capabilities->operations = XP_KEY_STORE_CAN_GENERATE
			| XP_KEY_STORE_CAN_OPEN | XP_KEY_STORE_CAN_SIGN
			| XP_KEY_STORE_CAN_DESTROY;
	}
	catch (const Botan::PKCS11::PKCS11_ReturnError& e) {
		capabilities->availability_status = p11_status(e.get_return_value());
		fail(capabilities->availability_status, e.what());
	}
	catch (const std::exception& e) {
		fail(XP_CRYPTO_ERR_UNAVAILABLE, e.what());
	}
#else
	(void)spec;
#endif
	return XP_CRYPTO_OK;
}

#if defined(XPTLS_BOTAN_HAS_PKCS11)
namespace {

std::string p11_uri_value(const char *uri, const char *name)
{
	if (uri == nullptr)
		return {};
	const size_t name_len = std::strlen(name);
	for (const char *p = uri; (p = std::strstr(p, name)) != nullptr; ++p) {
		if ((p == uri || p[-1] == ':' || p[-1] == ';' || p[-1] == '?'
		    || p[-1] == '&') && p[name_len] == '=') {
			const char *begin = p + name_len + 1;
			const char *end = begin;
			while (*end != 0 && *end != ';' && *end != '&') ++end;
			return std::string(begin, end);
		}
	}
	return {};
}

std::string p11_trim(const Botan::PKCS11::Utf8Char *value, size_t len)
{
	while (len != 0 && (value[len - 1] == ' ' || value[len - 1] == 0)) --len;
	return std::string(reinterpret_cast<const char *>(value), len);
}

struct P11Context {
	std::shared_ptr<Botan::PKCS11::Module> module;
	std::shared_ptr<Botan::PKCS11::Slot> slot;
	std::shared_ptr<Botan::PKCS11::Session> session;
	std::string serial;
};

int p11_context(P11Context& context, const struct xp_key_store_config *store,
	bool write, const std::string& required_serial)
{
	if (store == nullptr || store->store_uri == nullptr
	    || std::strstr(store->store_uri, "pin-value=") != nullptr)
		return XP_CRYPTO_ERR_INVALID;
	std::string module_path = p11_uri_value(store->store_uri, "module-path");
	if (module_path.empty())
		return XP_CRYPTO_ERR_INVALID;
	context.module = std::make_shared<Botan::PKCS11::Module>(module_path);
	auto slots = Botan::PKCS11::Slot::get_available_slots(*context.module, true);
	if (slots.empty())
		return XP_CRYPTO_ERR_UNAVAILABLE;
	std::string configured_slot = p11_uri_value(store->store_uri, "slot-id");
	std::string configured_token = p11_uri_value(store->store_uri, "token");
	for (auto slot_id : slots) {
		if (!configured_slot.empty()
		    && slot_id != static_cast<Botan::PKCS11::SlotId>(std::stoul(configured_slot)))
			continue;
		auto slot = std::make_shared<Botan::PKCS11::Slot>(*context.module, slot_id);
		auto info = slot->get_token_info();
		std::string serial = p11_trim(info.serialNumber, sizeof(info.serialNumber));
		std::string label = p11_trim(info.label, sizeof(info.label));
		if ((!required_serial.empty() && serial != required_serial)
		    || (!configured_token.empty() && label != configured_token))
			continue;
		context.slot = std::move(slot);
		context.serial = std::move(serial);
		break;
	}
	if (context.slot == nullptr)
		return XP_CRYPTO_ERR_NOT_FOUND;
	context.session = std::make_shared<Botan::PKCS11::Session>(*context.slot, !write);
	if (store->authorize == nullptr)
		return XP_CRYPTO_ERR_AUTHORIZATION;
	size_t pin_len = 0;
	if (store->authorize(store->authorize_context, nullptr, 0, &pin_len) != 0)
		return XP_CRYPTO_ERR_AUTHORIZATION;
	Botan::PKCS11::secure_string pin(pin_len, 0);
	size_t actual = pin_len;
	if (store->authorize(store->authorize_context, pin.data(), pin.size(), &actual) != 0
	    || actual != pin_len)
		return XP_CRYPTO_ERR_AUTHORIZATION;
	try {
		context.session->login(Botan::PKCS11::UserType::User, pin);
	}
	catch (const Botan::PKCS11::PKCS11_ReturnError& e) {
		if (e.get_return_value() != Botan::PKCS11::ReturnValue::UserAlreadyLoggedIn)
			throw;
	}
	return XP_CRYPTO_OK;
}

std::string hex_id(const std::vector<uint8_t>& id)
{
	static const char digits[] = "0123456789abcdef";
	std::string value(id.size() * 2, '0');
	for (size_t i = 0; i < id.size(); ++i) {
		value[i * 2] = digits[id[i] >> 4];
		value[i * 2 + 1] = digits[id[i] & 15];
	}
	return value;
}

bool parse_hex_id(const std::string& value, std::vector<uint8_t>& id)
{
	if (value.empty() || value.size() % 2 != 0)
		return false;
	id.resize(value.size() / 2);
	for (size_t i = 0; i < id.size(); ++i) {
		auto nibble = [](char c) -> int {
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'a' && c <= 'f') return c - 'a' + 10;
			if (c >= 'A' && c <= 'F') return c - 'A' + 10;
			return -1;
		};
		int high = nibble(value[i * 2]);
		int low = nibble(value[i * 2 + 1]);
		if (high < 0 || low < 0)
			return false;
		id[i] = static_cast<uint8_t>((high << 4) | low);
	}
	return true;
}

bool parse_p11_locator(const void *locator, size_t locator_len,
	std::string& serial, std::vector<uint8_t>& id,
	enum xp_key_algorithm& algorithm)
{
	if (locator == nullptr || locator_len == 0
	    || std::memchr(locator, 0, locator_len) != nullptr)
		return false;
	std::string value(static_cast<const char *>(locator), locator_len);
	if (value.rfind("pkcs11:", 0) != 0)
		return false;
	serial = p11_uri_value(value.c_str(), "serial");
	std::string encoded_id = p11_uri_value(value.c_str(), "id");
	std::string alg = p11_uri_value(value.c_str(), "alg");
	if (alg == "rsa")
		algorithm = XP_KEY_RSA;
	else if (alg == "ecdsa")
		algorithm = XP_KEY_ECDSA;
	else
		return false;
	return !serial.empty() && parse_hex_id(encoded_id, id);
}

std::string curve_name(enum xp_key_curve curve)
{
	switch (curve) {
		case XP_KEY_CURVE_P256: return "secp256r1";
		case XP_KEY_CURVE_P384: return "secp384r1";
		case XP_KEY_CURVE_P521: return "secp521r1";
		default: return {};
	}
}

void set_private_properties(Botan::PKCS11::PrivateKeyProperties& properties,
	const std::vector<uint8_t>& id, const std::string& label)
{
	properties.set_id(id);
	properties.set_label(label);
	properties.set_token(true);
	properties.set_private(true);
	properties.set_sensitive(true);
	properties.set_extractable(false);
	properties.set_sign(true);
	properties.set_destroyable(true);
}

} // namespace
#endif

extern "C" int
xp_key_provider_generate_stored(xp_key_t *out, void *locator,
	size_t *locator_len, const struct xp_key_store_config *store,
	const struct xp_key_spec *spec)
{
	if (out != nullptr) *out = nullptr;
#if !defined(XPTLS_BOTAN_HAS_PKCS11)
	(void)locator; (void)locator_len; (void)store; (void)spec;
	return XP_CRYPTO_ERR_UNAVAILABLE;
#else
	if (out == nullptr || locator_len == nullptr || store == nullptr || spec == nullptr
	    || store->store == nullptr || std::strcmp(store->store, "pkcs11") != 0)
		return XP_CRYPTO_ERR_INVALID;
	try {
		P11Context context;
		int status = p11_context(context, store, true, {});
		if (status != XP_CRYPTO_OK)
			return status;
		Botan::AutoSeeded_RNG rng;
		std::vector<uint8_t> id(20);
		rng.randomize(id.data(), id.size());
		std::string label = "xptls-" + hex_id(id);
		std::string value = "pkcs11:serial=" + context.serial + ";id="
			+ hex_id(id) + ";alg="
			+ (spec->algorithm == XP_KEY_RSA ? "rsa" : "ecdsa");
		if (locator == nullptr || *locator_len < value.size()) {
			*locator_len = value.size();
			return XP_CRYPTO_ERR_BUFFER_TOO_SMALL;
		}
		auto key = std::make_unique<xp_key>();
		if (spec->algorithm == XP_KEY_RSA) {
			Botan::PKCS11::RSA_PrivateKeyGenerationProperties properties;
			set_private_properties(properties, id, label);
			key->native = std::make_unique<Botan::PKCS11::PKCS11_RSA_PrivateKey>(
				*context.session, spec->bits, properties);
		}
		else if (spec->algorithm == XP_KEY_ECDSA) {
			Botan::PKCS11::EC_PrivateKeyGenerationProperties properties;
			set_private_properties(properties, id, label);
			auto parameters = Botan::EC_Group::from_name(curve_name(spec->curve)).DER_encode();
			key->native = std::make_unique<Botan::PKCS11::PKCS11_ECDSA_PrivateKey>(
				*context.session, parameters, properties);
		}
		else
			return XP_CRYPTO_ERR_UNSUPPORTED;
		key->pkcs11_module = std::move(context.module);
		key->pkcs11_slot = std::move(context.slot);
		key->pkcs11_session = std::move(context.session);
		key->storage = XP_KEY_STORE_PKCS11;
		key->exportable = false;
		std::memcpy(locator, value.data(), value.size());
		*locator_len = value.size();
		*out = key.release();
		return XP_CRYPTO_OK;
	}
	catch (const Botan::PKCS11::PKCS11_ReturnError& e) {
		return fail(p11_status(e.get_return_value()), e.what());
	}
	catch (const std::exception& e) {
		return fail(XP_CRYPTO_ERR, e.what());
	}
#endif
}

extern "C" int
xp_key_provider_import_stored(xp_key_t *out, void *locator,
	size_t *locator_len, const struct xp_key_store_config *store, xp_key_t source)
{
	if (out != nullptr) *out = nullptr;
	(void)locator; (void)locator_len; (void)store; (void)source;
	return XP_CRYPTO_ERR_UNSUPPORTED;
}

extern "C" int
xp_key_provider_open_stored(xp_key_t *out,
	const struct xp_key_store_config *store, const void *locator, size_t locator_len)
{
	if (out != nullptr) *out = nullptr;
#if !defined(XPTLS_BOTAN_HAS_PKCS11)
	(void)store; (void)locator; (void)locator_len;
	return XP_CRYPTO_ERR_UNAVAILABLE;
#else
	if (out == nullptr || store == nullptr || store->store == nullptr
	    || std::strcmp(store->store, "pkcs11") != 0)
		return XP_CRYPTO_ERR_INVALID;
	std::string serial;
	std::vector<uint8_t> id;
	enum xp_key_algorithm algorithm;
	if (!parse_p11_locator(locator, locator_len, serial, id, algorithm))
		return XP_CRYPTO_ERR_FORMAT;
	try {
		P11Context context;
		int status = p11_context(context, store, false, serial);
		if (status != XP_CRYPTO_OK)
			return status;
		auto key = std::make_unique<xp_key>();
		if (algorithm == XP_KEY_RSA) {
			auto matches = Botan::PKCS11::Object::search<
				Botan::PKCS11::PKCS11_RSA_PrivateKey>(*context.session, id);
			if (matches.size() != 1)
				return matches.empty() ? XP_CRYPTO_ERR_NOT_FOUND
					: XP_CRYPTO_ERR_CONFLICT;
			key->native = std::make_unique<Botan::PKCS11::PKCS11_RSA_PrivateKey>(
				std::move(matches.front()));
		}
		else {
			auto matches = Botan::PKCS11::Object::search<
				Botan::PKCS11::PKCS11_ECDSA_PrivateKey>(*context.session, id);
			if (matches.size() != 1)
				return matches.empty() ? XP_CRYPTO_ERR_NOT_FOUND
					: XP_CRYPTO_ERR_CONFLICT;
			key->native = std::make_unique<Botan::PKCS11::PKCS11_ECDSA_PrivateKey>(
				std::move(matches.front()));
		}
		key->pkcs11_module = std::move(context.module);
		key->pkcs11_slot = std::move(context.slot);
		key->pkcs11_session = std::move(context.session);
		key->storage = XP_KEY_STORE_PKCS11;
		key->exportable = false;
		*out = key.release();
		return XP_CRYPTO_OK;
	}
	catch (const Botan::PKCS11::PKCS11_ReturnError& e) {
		return fail(p11_status(e.get_return_value()), e.what());
	}
	catch (const std::exception& e) {
		return fail(XP_CRYPTO_ERR, e.what());
	}
#endif
}

extern "C" int
xp_key_provider_destroy_stored(const struct xp_key_store_config *store,
	const void *locator, size_t locator_len,
	const void *expected_fingerprint, size_t fingerprint_len)
{
	if (expected_fingerprint == nullptr || fingerprint_len != 32)
		return XP_CRYPTO_ERR_INVALID;
	xp_key_t key = nullptr;
	int status = xp_key_provider_open_stored(
		&key, store, locator, locator_len);
	unsigned char actual[32];
	size_t actual_len = sizeof(actual);
	if (status == XP_CRYPTO_OK)
		status = xp_key_fingerprint_sha256(key, actual, &actual_len);
	if (status == XP_CRYPTO_OK
	    && std::memcmp(actual, expected_fingerprint, sizeof(actual)) != 0)
		status = XP_CRYPTO_ERR_CONFLICT;
#if defined(XPTLS_BOTAN_HAS_PKCS11)
	if (status == XP_CRYPTO_OK) {
		auto *object = dynamic_cast<Botan::PKCS11::Object *>(key->native.get());
		if (object == nullptr)
			status = XP_CRYPTO_ERR;
		else {
			try { object->destroy(); }
			catch (const Botan::PKCS11::PKCS11_ReturnError& e) {
				status = fail(p11_status(e.get_return_value()), e.what());
			}
			catch (const std::exception& e) {
				status = fail(XP_CRYPTO_ERR, e.what());
			}
		}
	}
#endif
	xp_ca_scrub_memory(actual, sizeof(actual));
	xp_key_release(key);
	return status;
}
