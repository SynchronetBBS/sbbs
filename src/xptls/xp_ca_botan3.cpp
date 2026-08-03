#include <algorithm>
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
#include "xp_ca_policy.h"

#include <botan/auto_rng.h>
#include <botan/ber_dec.h>
#include <botan/certstor.h>
#include <botan/data_src.h>
#include <botan/der_enc.h>
#include <botan/ed25519.h>
#include <botan/pkcs10.h>
#include <botan/pkcs8.h>
#include <botan/pkix_types.h>
#include <botan/pubkey.h>
#include <botan/x509_ca.h>
#include <botan/x509_ext.h>
#include <botan/x509_key.h>
#include <botan/x509_obj.h>
#include <botan/x509path.h>

struct xp_ca_key {
	std::unique_ptr<Botan::Private_Key> native;
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

/*
 * Botan can decode issuingDistributionPoint, but its public encoder throws
 * Not_Implemented through at least Botan 3.12. Keep the standards-compliant
 * encoding workaround contained in this provider until Botan can emit it.
 */
class xp_ca_issuing_distribution_point final
	: public Botan::Certificate_Extension {
public:
	explicit xp_ca_issuing_distribution_point(std::string uri)
		: uri_(std::move(uri))
	{
	}

	std::unique_ptr<Botan::Certificate_Extension> copy() const override
	{
		return std::make_unique<xp_ca_issuing_distribution_point>(uri_);
	}

	Botan::OID oid_of() const override
	{
		return Botan::OID({ 2, 5, 29, 28 });
	}

	std::string oid_name() const override
	{
		return "X509v3.CRLIssuingDistributionPoint";
	}

private:
	std::vector<uint8_t> encode_inner() const override
	{
		Botan::AlternativeName names;
		names.add_uri(uri_);
		Botan::Cert_Extension::CRL_Distribution_Points::Distribution_Point point(
			names);
		std::vector<uint8_t> encoded;
		Botan::DER_Encoder(encoded).encode(point);
		return encoded;
	}

	void decode_inner(const std::vector<uint8_t>&) override
	{
	}

	std::string uri_;
};

static thread_local std::string error_text;

static const Botan::Cert_Extension::CRL_Distribution_Points *
crl_distribution_points(const Botan::X509_Certificate& certificate)
{
	return certificate.v3_extensions().get_extension_object_as<
		Botan::Cert_Extension::CRL_Distribution_Points>();
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
		const auto& uris = distribution_point.point().uris();
		if (uris.find(std::string(expected)) != uris.end())
			return true;
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
	const auto& names = extension->distribution_points()[0].point();
	return names.count() == 1
		&& names.uris().size() == 1
		&& *names.uris().begin() == expected;
}

static Botan::OID
crl_issuing_distribution_point_oid()
{
	return Botan::OID({ 2, 5, 29, 28 });
}

static std::optional<Botan::AlternativeName>
crl_issuing_distribution_point(const Botan::X509_CRL& crl)
{
	const auto oid = crl_issuing_distribution_point_oid();
	if (!crl.extensions().extension_set(oid))
		return std::nullopt;
	Botan::Cert_Extension::CRL_Distribution_Points::Distribution_Point point;
	Botan::BER_Decoder(crl.extensions().get_extension_bits(oid))
		.decode(point)
		.verify_end();
	return point.point();
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
		&& names->uris().size() == 1
		&& *names->uris().begin() == expected;
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
xp_ca_key_generate(xp_ca_key_t *out, enum xp_ca_key_algorithm algorithm)
{
	if (out == nullptr)
		return XP_CA_ERR;
	*out = nullptr;
	if (algorithm != XP_CA_KEY_ED25519)
		return XP_CA_ERR_POLICY;
	try {
		Botan::AutoSeeded_RNG rng;
		auto                  key = std::make_unique<xp_ca_key>();
		key->native = std::make_unique<Botan::Ed25519_PrivateKey>(rng);
		*out = key.release();
		return XP_CA_OK;
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR, e.what());
	}
}

extern "C" void
xp_ca_key_free(xp_ca_key_t key)
{
	delete key;
}

extern "C" int
xp_ca_key_save_pem(xp_ca_key_t key, const char *path, const char *password)
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
xp_ca_key_delete_pem(const char *path)
{
	if (path == nullptr || std::remove(path) != 0)
		return fail(XP_CA_ERR, "private key deletion failed");
	return XP_CA_OK;
}

extern "C" int
xp_ca_key_load_pem(xp_ca_key_t *out, const char *path, const char *password)
{
	if (out == nullptr)
		return XP_CA_ERR;
	*out = nullptr;
	if (path == nullptr || password == nullptr || password[0] == '\0')
		return XP_CA_ERR;
	try {
		Botan::DataSource_Stream source(path);
		auto                     key = std::make_unique<xp_ca_key>();
		key->native = Botan::PKCS8::load_key(source, password);
		if (key->native->algo_name() != "Ed25519")
			return fail(XP_CA_ERR_FORMAT, "unsupported private key algorithm");
		*out = key.release();
		return XP_CA_OK;
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR, e.what());
	}
}

extern "C" int
xp_ca_csr_create(xp_ca_csr_t *out, xp_ca_key_t key)
{
	if (out == nullptr)
		return XP_CA_ERR;
	*out = nullptr;
	if (key == nullptr)
		return XP_CA_ERR;
	try {
		Botan::AutoSeeded_RNG rng;
		Botan::X509_DN        subject;
		Botan::Extensions     extensions;
		auto                  csr = std::make_unique<xp_ca_csr>();
		csr->native = std::make_unique<Botan::PKCS10_Request>(
			Botan::PKCS10_Request::create(*key->native, subject, extensions,
			                              "SHA-512", rng, "Pure"));
		*out = csr.release();
		return XP_CA_OK;
	} catch (const std::exception& e) {
		return fail(XP_CA_ERR, e.what());
	}
}

extern "C" int
xp_ca_csr_verify(xp_ca_csr_t csr)
{
	if (csr == nullptr)
		return XP_CA_ERR;
	try {
		auto key = csr->native->subject_public_key();
		return key->algo_name() == "Ed25519" && csr->native->check_signature(*key)
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
	std::optional<size_t> path_length;
	if (request->policy.path_length >= 0)
		path_length = (size_t)request->policy.path_length;
	extensions.add(std::make_unique<Botan::Cert_Extension::Basic_Constraints>(
					   request->policy.is_ca, path_length),
	               request->policy.basic_constraints_critical);
	extensions.add(std::make_unique<Botan::Cert_Extension::Key_Usage>(key_usage),
	               request->policy.key_usage_critical);
	Botan::Cert_Extension::Subject_Key_ID subject_key_id(subject_public_key, "SHA-256");
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
	return Botan::X509_DN { { "X520.CommonName", request->subject.common_name } };
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
xp_ca_cert_create_self_signed(xp_ca_cert_t *out, xp_ca_key_t key,
                              const struct xp_ca_issue_request *request)
{
	if (out == nullptr)
		return XP_CA_ERR;
	*out = nullptr;
	if (key == nullptr)
		return XP_CA_ERR;
	if (!xp_ca_issue_request_is_valid(request))
		return XP_CA_ERR_POLICY;
	try {
		Botan::AutoSeeded_RNG rng;
		Botan::PK_Signer      signer(*key->native, rng, "Pure");
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
xp_ca_cert_issue(xp_ca_cert_t *out, xp_ca_key_t issuer_key, xp_ca_cert_t issuer,
                 xp_ca_csr_t csr, const struct xp_ca_issue_request *request)
{
	if (out == nullptr)
		return XP_CA_ERR;
	*out = nullptr;
	if (issuer_key == nullptr || issuer == nullptr || csr == nullptr)
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
		Botan::X509_CA        ca(*issuer->native, *issuer_key->native, "SHA-512", "Pure", rng);
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
		std::make_unique<xp_ca_issuing_distribution_point>(
			request->issuing_distribution_point),
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
xp_ca_crl_create(xp_ca_crl_t *out, xp_ca_key_t issuer_key, xp_ca_cert_t issuer,
	             xp_ca_crl_t previous,
	             const struct xp_ca_crl_request *request)
{
	if (out == nullptr)
		return XP_CA_ERR;
	*out = nullptr;
	if (issuer_key == nullptr || issuer == nullptr)
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
		Botan::X509_CA                ca(*issuer->native, *issuer_key->native, "SHA-512", "Pure", rng);
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
			: previous->native->crl_number() + 1;
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
		info->number = crl->native->crl_number();
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
	const auto& dns_names = alternative_names.dns();
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
			if (dns_names_equal(actual, identity->dns_names[expected])) {
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
	for (const auto& scope : idp->uris()) {
		if (certificate_has_crl_distribution_point(certificate, scope))
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
			    || candidate.crl_number()
		        > selected[certificate_index]->crl_number()
			    || (candidate.crl_number()
		            == selected[certificate_index]->crl_number()
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

	/*
	 * Botan 3.11 compares a display-form CRLDP string to the raw IDP URI.
	 * Scope was checked from the typed extensions above, so discard only that
	 * spurious status while retaining Botan's signature, time, usage,
	 * critical-extension, and revocation checks.
	 */
	auto statuses = Botan::PKIX::check_crl(path, selected, when);
	for (auto& status : statuses)
		status.erase(Botan::Certificate_Status_Code::NO_MATCHING_CRLDP);
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
