/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * Licensed under the BSD 2-Clause License. See LICENSE in this directory.	*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 ****************************************************************************/

/*
 * xp_tls — Botan 3 backend.
 *
 * Botan's TLS API is callback-driven: the application feeds ciphertext in
 * via received_data(), and Botan invokes virtual callback methods to emit
 * ciphertext or deliver decrypted records. The xp_tls contract is
 * blocking push/pop/flush, so this file bridges the two by driving the
 * socket from inside xp_tls_push / xp_tls_pop and buffering plaintext
 * between calls.
 *
 * Legacy callers retain the Cryptlib-era permissive posture. Configured
 * callers can require hostname-checked certificate verification using either
 * system Web-PKI roots or an explicit trust anchor, and can present a
 * client certificate.
 */

/* sockwrap.h pulls in <windows.h> on Win32, which #defines `max` and
 * `min` as macros and breaks `std::chrono::seconds::max()` in
 * <botan/tls_session.h>.  NOMINMAX has to be set before that include
 * chain runs. */
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

/* xp_tls.h pulls in sockwrap.h, which lands the OS socket + timeval
 * headers in the right order (winsock2 + ws2tcpip + wspiapi on Win32,
 * sys/socket + sys/time + friends on Unix).  Don't reach past it. */
#include "xp_tls.h"
#include "xp_key_internal.h"
#include "xp_tls_internal.h"

#include <botan/auto_rng.h>
#include <botan/certstor.h>
#include <botan/certstor_system.h>
#include <botan/credentials_manager.h>
#include <botan/data_src.h>
#include <botan/pkcs8.h>
#include <botan/pem.h>
#include <botan/secmem.h>
#include <botan/tls_alert.h>
#include <botan/tls_callbacks.h>
#include <botan/tls_client.h>
#include <botan/tls_external_psk.h>
#include <botan/tls_policy.h>
#include <botan/tls_server_info.h>
#include <botan/tls_session.h>
#include <botan/tls_session_manager_noop.h>
#include <botan/tls_server.h>
#include <botan/x509path.h>

/* ---------------------------------------------------------------- errors */

static thread_local char last_err_buf[256];

extern "C" const char *
xp_tls_last_err(void)
{
	return last_err_buf[0] ? last_err_buf : "no error";
}

static void
set_last_err(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(last_err_buf, sizeof(last_err_buf), fmt, ap);
	va_end(ap);
}

static std::string
readable_time(const Botan::ASN1_Time &time)
{
	std::string ret = time.readable_string();
	if (ret.size() >= 10 && ret[4] == '/' && ret[7] == '/') {
		ret[4] = '-';
		ret[7] = '-';
	}
	return ret;
}

/* ----------------------------------------------------- Credentials */

/*
 * Permissive credentials manager: no client certificate, no trusted CAs.
 * Combined with the verify callback below this gives us peer-verify=none
 * behaviour equivalent to the OpenSSL backend.
 *
 * Optionally also supplies a PSK for TLS-PSK clients.  When psk_secret_ is
 * empty, behaves as a pure cert-mode credentials manager (no PSK offered).
 */
class ClientCredentials : public Botan::Credentials_Manager {
public:
	ClientCredentials() = default;
	ClientCredentials(std::string identity, Botan::secure_vector<uint8_t> psk)
	    : psk_identity_(std::move(identity)), psk_secret_(std::move(psk)) {}
	ClientCredentials(enum xp_tls_server_auth server_auth,
	                  const char *trusted_cert_file,
	                  const char *client_cert_file,
	                  const char *client_key_file,
	                  const struct xp_tls_client_identity *client_identity,
	                  xp_crypto_secret_callback_t key_password,
	                  void *key_password_arg)
	{
		if (server_auth == XP_TLS_SERVER_AUTH_WEB_PKI)
			system_store_ = std::make_unique<Botan::System_Certificate_Store>();
		else if (server_auth == XP_TLS_SERVER_AUTH_CERTIFICATE) {
			custom_trust_cert_.emplace(trusted_cert_file);
			custom_store_ =
			    std::make_unique<Botan::Certificate_Store_In_Memory>(
			        *custom_trust_cert_);
		}
		if (client_cert_file != nullptr && client_cert_file[0] != 0) {
			Botan::DataSource_Stream cert_source(client_cert_file);
			while (Botan::PEM_Code::matches(cert_source))
				client_chain_.emplace_back(cert_source);
			if (client_chain_.empty())
				throw std::runtime_error(
				    "TLS client certificate file contains no PEM certificate");
			Botan::DataSource_Stream key_source(client_key_file);
			if (key_password == nullptr)
				client_key_ = Botan::PKCS8::load_key(key_source);
			else {
				size_t length = 0;
				if (key_password(key_password_arg, nullptr, 0, &length) != 0)
					throw std::runtime_error("TLS client key password unavailable");
				Botan::secure_vector<uint8_t> secret(length);
				size_t actual = length;
				if (key_password(key_password_arg, secret.data(), secret.size(),
				                 &actual) != 0 || actual != length)
					throw std::runtime_error("TLS client key password unavailable");
				std::string password(reinterpret_cast<const char *>(secret.data()),
				                     secret.size());
				try {
					client_key_ = Botan::PKCS8::load_key(key_source, password);
				}
				catch (...) {
					std::fill(password.begin(), password.end(), '\0');
					throw;
				}
				std::fill(password.begin(), password.end(), '\0');
			}
		}
		else if (client_identity != nullptr) {
			const auto *pem = static_cast<const uint8_t *>(
				client_identity->certificate_chain_pem);
			Botan::DataSource_Memory source(pem,
				client_identity->certificate_chain_pem_len);
			while (Botan::PEM_Code::matches(source))
				client_chain_.emplace_back(source);
			auto *native = static_cast<Botan::Private_Key *>(
				xp_key_native_private(client_identity->private_key));
			if (client_chain_.empty() || native == nullptr)
				throw std::runtime_error("invalid TLS client identity");
			client_key_ = std::shared_ptr<Botan::Private_Key>(
				native, [](Botan::Private_Key *) {});
			client_key_handle_ = client_identity->private_key;
			xp_key_retain(client_key_handle_);
		}
	}
	~ClientCredentials() override
	{
		client_key_.reset();
		if (client_key_handle_ != nullptr)
			xp_key_release(client_key_handle_);
	}

	const Botan::X509_Certificate *custom_trust_certificate() const
	{
		return custom_trust_cert_ ? &*custom_trust_cert_ : nullptr;
	}

	std::vector<Botan::Certificate_Store *>
	trusted_certificate_authorities(const std::string &, const std::string &) override
	{
		std::vector<Botan::Certificate_Store *> stores;
		if (system_store_ != nullptr)
			stores.push_back(system_store_.get());
		if (custom_store_ != nullptr)
			stores.push_back(custom_store_.get());
		return stores;
	}

	std::vector<Botan::X509_Certificate>
	find_cert_chain(const std::vector<std::string> &key_types,
	                const std::vector<Botan::AlgorithmIdentifier> &,
	                const std::vector<Botan::X509_DN> &,
	                const std::string &type,
	                const std::string &) override
	{
		if (type != "tls-client" || client_key_ == nullptr)
			return {};
		if (!key_types.empty() && std::find(key_types.begin(), key_types.end(),
		    client_key_->algo_name()) == key_types.end())
			return {};
		return client_chain_;
	}

	std::shared_ptr<Botan::Private_Key>
	private_key_for(const Botan::X509_Certificate &,
	                const std::string &type,
	                const std::string &) override
	{
		if (type != "tls-client")
			return nullptr;
		return client_key_;
	}

	std::string psk_identity(const std::string &type, const std::string &context,
	                         const std::string &identity_hint) override
	{
		(void)type; (void)context; (void)identity_hint;
		return psk_identity_;
	}

	Botan::SymmetricKey psk(const std::string &type, const std::string &context,
	                        const std::string &identity) override
	{
		(void)type; (void)context; (void)identity;
		if (psk_secret_.empty())
			return Botan::Credentials_Manager::psk(type, context, identity);
		return Botan::SymmetricKey(psk_secret_.data(), psk_secret_.size());
	}

	std::vector<Botan::TLS::ExternalPSK>
	find_preshared_keys(std::string_view,
	                    Botan::TLS::Connection_Side whoami,
	                    const std::vector<std::string> &identities,
	                    const std::optional<std::string> &prf) override
	{
		if (psk_secret_.empty() ||
		    whoami != Botan::TLS::Connection_Side::Client ||
		    (prf && *prf != "SHA-256") ||
		    (!identities.empty() && std::find(identities.begin(),
		    identities.end(), psk_identity_) == identities.end()))
			return {};
		Botan::secure_vector<uint8_t> secret(psk_secret_.begin(),
		    psk_secret_.end());
		std::vector<Botan::TLS::ExternalPSK> result;
		result.emplace_back(psk_identity_, "SHA-256", std::move(secret));
		return result;
	}

private:
	/* secure_vector wipes its backing storage on destruction via Botan's
	   secure_allocator — std::vector<uint8_t> would leave the password
	   bytes in freed heap. */
	std::string                   psk_identity_;
	Botan::secure_vector<uint8_t> psk_secret_;
	std::unique_ptr<Botan::System_Certificate_Store> system_store_;
	std::unique_ptr<Botan::Certificate_Store_In_Memory> custom_store_;
	std::optional<Botan::X509_Certificate> custom_trust_cert_;
	std::vector<Botan::X509_Certificate> client_chain_;
	std::shared_ptr<Botan::Private_Key> client_key_;
	xp_key_t client_key_handle_ = nullptr;
};

/* Web-PKI clients normally soft-fail unavailable revocation information.
   Botan's base policy requires CRL/OCSP proof, which rejects many otherwise
   valid public sites, so retain signature/path/hostname checking without
   making revocation information mandatory. */
class CertificatePolicy : public Botan::TLS::Policy {
public:
	CertificatePolicy(enum xp_tls_version minimum = XP_TLS_VERSION_1_2,
	                  enum xp_tls_version maximum = XP_TLS_VERSION_UNKNOWN)
	    : minimum_(minimum), maximum_(maximum) {}
	bool require_cert_revocation_info() const override { return false; }
	bool allow_tls12() const override
	{
		return minimum_ <= XP_TLS_VERSION_1_2 &&
		    (maximum_ == XP_TLS_VERSION_UNKNOWN || maximum_ >= XP_TLS_VERSION_1_2);
	}
	bool allow_tls13() const override
	{
		return minimum_ <= XP_TLS_VERSION_1_3 &&
		    (maximum_ == XP_TLS_VERSION_UNKNOWN || maximum_ >= XP_TLS_VERSION_1_3);
	}

private:
	enum xp_tls_version minimum_;
	enum xp_tls_version maximum_;
};

class ServerPolicy : public CertificatePolicy {
public:
	ServerPolicy(enum xp_tls_version minimum, enum xp_tls_version maximum,
	             enum xp_tls_client_auth client_auth, bool psk,
	             enum xp_tls_psk_policy psk_policy)
	    : CertificatePolicy(minimum, maximum), client_auth_(client_auth),
	      tls12_(minimum <= XP_TLS_VERSION_1_2 &&
	          (maximum == XP_TLS_VERSION_UNKNOWN ||
	           maximum >= XP_TLS_VERSION_1_2)), psk_(psk),
	      psk_policy_(psk_policy) {}
	bool require_client_certificate_authentication() const override
	{
		return client_auth_ == XP_TLS_CLIENT_AUTH_REQUIRE_VALID;
	}
	bool request_client_certificate_authentication() const override
	{
		return client_auth_ != XP_TLS_CLIENT_AUTH_NONE;
	}
	std::vector<std::string> allowed_key_exchange_methods() const override
	{
		auto methods = CertificatePolicy::allowed_key_exchange_methods();
		if (tls12_) {
			methods.push_back("ECDHE_PSK");
			methods.push_back("DHE_PSK");
			if (psk_policy_ == XP_TLS_PSK_POLICY_TLS12_COMPATIBILITY)
				methods.push_back("PSK");
		}
		return methods;
	}
	std::vector<std::string> allowed_ciphers() const override
	{
		if (psk_ && psk_policy_ == XP_TLS_PSK_POLICY_TLS12_COMPATIBILITY)
			return {"AES-128", "AES-256", "AES-128/GCM",
			        "AES-256/GCM", "ChaCha20Poly1305"};
		if (psk_)
			return {"AES-128/GCM", "AES-256/GCM", "ChaCha20Poly1305"};
		return CertificatePolicy::allowed_ciphers();
	}
	std::vector<std::string> allowed_macs() const override
	{
		if (!psk_)
			return CertificatePolicy::allowed_macs();
		if (psk_policy_ == XP_TLS_PSK_POLICY_TLS12_COMPATIBILITY)
			return {"SHA-256", "SHA-384", "AEAD", "SHA-1"};
		return {"AEAD"};
	}
	std::vector<std::string> allowed_signature_methods() const override
	{
		auto methods = CertificatePolicy::allowed_signature_methods();
		if (tls12_)
			methods.push_back("IMPLICIT");
		return methods;
	}
	Botan::TLS::Group_Params choose_key_exchange_group(
	    const std::vector<Botan::TLS::Group_Params>& supported,
	    const std::vector<Botan::TLS::Group_Params>& offered) const override
	{
		const auto allowed = key_exchange_groups();
		for (auto group : offered) {
			if (std::find(supported.begin(), supported.end(), group) != supported.end()
			    && std::find(allowed.begin(), allowed.end(), group) != allowed.end())
				return group;
		}
		return CertificatePolicy::choose_key_exchange_group(supported, offered);
	}

private:
	enum xp_tls_client_auth client_auth_;
	bool tls12_;
	bool psk_;
	enum xp_tls_psk_policy psk_policy_;
};

class ServerCredentials : public Botan::Credentials_Manager {
public:
	ServerCredentials(const void *chain_pem, size_t chain_pem_length,
	                  xp_key_t private_key,
	                  const char *client_ca_file,
	                  xp_tls_psk_lookup_cb psk_lookup, void *psk_arg,
	                  std::string *selected_identity)
	    : psk_lookup_(psk_lookup), psk_arg_(psk_arg),
	      selected_identity_(selected_identity)
	{
		if (private_key != nullptr) {
			const auto *pem = static_cast<const uint8_t *>(chain_pem);
			Botan::DataSource_Memory source(pem, chain_pem_length);
			while (Botan::PEM_Code::matches(source))
				server_chain_.emplace_back(source);
			auto *native = static_cast<Botan::Private_Key *>(
			    xp_key_native_private(private_key));
			if (server_chain_.empty() || native == nullptr)
				throw std::runtime_error("invalid TLS server credentials");
			server_key_ = std::shared_ptr<Botan::Private_Key>(
			    native, [](Botan::Private_Key *) {});
		}
		if (client_ca_file != nullptr) {
			client_ca_.emplace(client_ca_file);
			client_store_ = std::make_unique<Botan::Certificate_Store_In_Memory>(
			    *client_ca_);
		}
	}

	std::vector<Botan::X509_Certificate> find_cert_chain(
	    const std::vector<std::string>& key_types,
	    const std::vector<Botan::AlgorithmIdentifier>&,
	    const std::vector<Botan::X509_DN>&,
	    const std::string& type, const std::string&) override
	{
		if (type != "tls-server" || server_key_ == nullptr)
			return {};
		if (!key_types.empty() && std::find(key_types.begin(), key_types.end(),
		    server_key_->algo_name()) == key_types.end())
			return {};
		return server_chain_;
	}

	std::shared_ptr<Botan::Private_Key> private_key_for(
	    const Botan::X509_Certificate&, const std::string& type,
	    const std::string&) override
	{
		return type == "tls-server" ? server_key_ : nullptr;
	}

	std::vector<Botan::Certificate_Store *> trusted_certificate_authorities(
	    const std::string&, const std::string&) override
	{
		return client_store_ == nullptr
		    ? std::vector<Botan::Certificate_Store *>{}
		    : std::vector<Botan::Certificate_Store *>{client_store_.get()};
	}

	Botan::SymmetricKey psk(const std::string&, const std::string&,
	                        const std::string& identity) override
	{
		auto secret = lookup(identity);
		return Botan::SymmetricKey(secret.data(), secret.size());
	}

	std::vector<Botan::TLS::ExternalPSK> find_preshared_keys(
	    std::string_view, Botan::TLS::Connection_Side whoami,
	    const std::vector<std::string>& identities,
	    const std::optional<std::string>& prf) override
	{
		if (whoami != Botan::TLS::Connection_Side::Server || psk_lookup_ == nullptr)
			return {};
		for (const auto& identity : identities) {
			try {
				auto secret = lookup(identity);
				std::vector<Botan::TLS::ExternalPSK> result;
				result.emplace_back(identity, prf.value_or("SHA-256"),
				                    std::move(secret));
				return result;
			}
			catch (...) {
			}
		}
		return {};
	}

private:
	Botan::secure_vector<uint8_t> lookup(const std::string& identity)
	{
		if (psk_lookup_ == nullptr)
			throw std::runtime_error("unknown PSK identity");
		Botan::secure_vector<uint8_t> value(1024);
		size_t length = value.size();
		if (psk_lookup_(psk_arg_, identity.data(), identity.size(),
		                value.data(), &length) != 0 || length > value.size())
			throw std::runtime_error("unknown PSK identity");
		value.resize(length);
		if (selected_identity_ != nullptr)
			*selected_identity_ = identity;
		return value;
	}

	std::vector<Botan::X509_Certificate> server_chain_;
	std::shared_ptr<Botan::Private_Key> server_key_;
	std::optional<Botan::X509_Certificate> client_ca_;
	std::unique_ptr<Botan::Certificate_Store_In_Memory> client_store_;
	xp_tls_psk_lookup_cb psk_lookup_;
	void *psk_arg_;
	std::string *selected_identity_;
};

/* A configured external PSK is bound to one exact protocol version. The
   modern policy restricts TLS 1.2 to ephemeral PSK exchanges, while the
   explicit compatibility policy permits plain PSK for older brokers. Botan's
   TLS 1.3 client offers only the forward-secret psk_dhe_ke mode. */
class PskPolicy : public Botan::TLS::Policy {
public:
	PskPolicy(enum xp_tls_version version, enum xp_tls_psk_policy policy)
	    : version_(version), policy_(policy) {}
	bool allow_tls12() const override { return version_ == XP_TLS_VERSION_1_2; }
	bool allow_tls13() const override { return version_ == XP_TLS_VERSION_1_3; }
	Botan::TLS::Protocol_Version
	latest_supported_version(bool datagram) const override
	{
		if (version_ == XP_TLS_VERSION_1_3)
			return datagram ? Botan::TLS::Protocol_Version::DTLS_V13
			                : Botan::TLS::Protocol_Version::TLS_V13;
		return datagram ? Botan::TLS::Protocol_Version::DTLS_V12
		                : Botan::TLS::Protocol_Version::TLS_V12;
	}
	std::vector<std::string> allowed_key_exchange_methods() const override
	{
		if (version_ == XP_TLS_VERSION_1_3)
			return Botan::TLS::Policy::allowed_key_exchange_methods();
		if (policy_ == XP_TLS_PSK_POLICY_TLS12_COMPATIBILITY)
			return { "DHE_PSK", "ECDHE_PSK", "PSK" };
		return { "DHE_PSK", "ECDHE_PSK" };
	}
	std::vector<std::string> allowed_signature_methods() const override
	{
		if (version_ == XP_TLS_VERSION_1_3)
			return Botan::TLS::Policy::allowed_signature_methods();
		/* TLS 1.2 PSK suites authenticate implicitly through the key
		   exchange rather than with a certificate signature. */
		return { "IMPLICIT" };
	}
	std::vector<Botan::TLS::Group_Params> key_exchange_groups_to_offer() const override
	{
		if (version_ == XP_TLS_VERSION_1_3)
			return {Botan::TLS::Group_Params::X25519,
			        Botan::TLS::Group_Params::SECP256R1};
		return Botan::TLS::Policy::key_exchange_groups_to_offer();
	}
	std::vector<std::string> allowed_ciphers() const override
	{
		if (policy_ == XP_TLS_PSK_POLICY_MODERN)
			return { "AES-128/GCM", "AES-256/GCM",
			         "ChaCha20Poly1305" };
		return {
		    "AES-128",
		    "AES-256",
		    "AES-128/GCM",
		    "AES-256/GCM",
		    "ChaCha20Poly1305",
		};
	}
	std::vector<std::string> allowed_macs() const override
	{
		if (policy_ == XP_TLS_PSK_POLICY_MODERN)
			return { "AEAD" };
		return { "SHA-256", "SHA-384", "AEAD", "SHA-1" };
	}

private:
	enum xp_tls_version version_;
	enum xp_tls_psk_policy policy_;
};

/* ------------------------------------------------------- Callbacks */

struct xp_tls_ctx {
	SOCKET                                   sock;
	std::string                              sni;
	char                                     err[256];
	std::deque<uint8_t>                      plaintext;	/* decrypted data awaiting pop() */
	std::vector<uint8_t>                     pending_tx;	/* ciphertext waiting to go out */
	bool                                     peer_closed;
	bool                                     fatal_alert;
	bool                                     psk_used;
	enum xp_tls_version                      protocol_version;
	std::string                              cipher_name;
	std::string                              selected_psk_identity;
	std::vector<std::vector<uint8_t>>        peer_chain_der;
	xp_tls_peer_chain_cb                     peer_chain_cb;
	void                                    *peer_chain_cb_arg;
	/* Botan 3's TLS::Client takes shared_ptrs for its dependencies, so
	   the surrounding objects have to be shared_ptr too. */
	std::shared_ptr<Botan::AutoSeeded_RNG>       rng;
	std::shared_ptr<Botan::TLS::Session_Manager> sess_mgr;
	std::shared_ptr<Botan::Credentials_Manager>  creds;
	std::shared_ptr<Botan::TLS::Policy>          policy;
	std::shared_ptr<Botan::TLS::Callbacks>       callbacks;
	std::unique_ptr<Botan::TLS::Channel>         channel;
	xp_key_t                                     server_key;
	std::mutex                                  read_lock;
	std::mutex                                  write_lock;
	std::mutex                                  provider_lock;
	std::mutex                                  wire_write_lock;
	std::atomic_bool                            terminating{false};
};

class XpTlsCallbacks : public Botan::TLS::Callbacks {
public:
	XpTlsCallbacks(xp_tls_ctx *c, bool verify,
	               const Botan::X509_Certificate *pinned)
	    : ctx(c), verify_peer(verify), pinned_cert(pinned) {}

	void tls_session_established(const Botan::TLS::Session_Summary &s) override
	{
		ctx->psk_used = s.psk_used();
		ctx->cipher_name = s.ciphersuite().to_string();
		if (s.version() == Botan::TLS::Protocol_Version::TLS_V12)
			ctx->protocol_version = XP_TLS_VERSION_1_2;
		else if (s.version() == Botan::TLS::Protocol_Version::TLS_V13)
			ctx->protocol_version = XP_TLS_VERSION_1_3;
		else
			ctx->protocol_version = XP_TLS_VERSION_UNKNOWN;
	}

	void tls_emit_data(std::span<const uint8_t> data) override
	{
		ctx->pending_tx.insert(ctx->pending_tx.end(), data.begin(), data.end());
	}

	void tls_record_received(uint64_t, std::span<const uint8_t> data) override
	{
		ctx->plaintext.insert(ctx->plaintext.end(), data.begin(), data.end());
	}

	void tls_alert(Botan::TLS::Alert alert) override
	{
		if (alert.type() == Botan::TLS::AlertType::CloseNotify)
			ctx->peer_closed = true;
		else if (alert.is_fatal())
			ctx->fatal_alert = true;
	}

	void tls_verify_cert_chain(
	    const std::vector<Botan::X509_Certificate> &cert_chain,
	    const std::vector<std::optional<Botan::OCSP::Response>> &ocsp_responses,
	    const std::vector<Botan::Certificate_Store *> &trusted_roots,
	    Botan::Usage_Type usage, std::string_view hostname,
	    const Botan::TLS::Policy &policy) override
	{
		ctx->peer_chain_der.clear();
		for (const auto& certificate : cert_chain)
			ctx->peer_chain_der.push_back(certificate.BER_encode());
		if (verify_peer) {
			try {
			if (pinned_cert != nullptr) {
				/* Explicit per-connection trust deliberately permits a leaf or
				   intermediate as the terminal anchor, matching OpenSSL's
				   X509_V_FLAG_PARTIAL_CHAIN. All certificates below the anchor,
				   plus leaf hostname, usage, validity, and strength, are still
				   validated normally. */
				Botan::Path_Validation_Restrictions restrictions(
				    policy.require_cert_revocation_info(),
				    policy.minimum_signature_strength(), false,
				    std::chrono::seconds::zero(),
				    std::make_unique<Botan::Certificate_Store_In_Memory>(),
				    false, false);
				auto result = Botan::x509_path_validate(cert_chain,
				    restrictions, trusted_roots, hostname, usage,
				    std::chrono::system_clock::now(),
				    std::chrono::milliseconds(0), ocsp_responses);
				if (!result.successful_validation())
					throw std::runtime_error(result.result_string());
				return;
			}
			Botan::TLS::Callbacks::tls_verify_cert_chain(cert_chain,
			    ocsp_responses, trusted_roots, usage, hostname, policy);
			}
			catch (...) {
				report_peer_chain(cert_chain);
				throw;
			}
		}
	}

private:
	struct CertificateStrings {
		std::string subject;
		std::string issuer;
		std::string not_before;
		std::string not_after;
		std::string fingerprint;
		std::string pem;
	};

	void report_peer_chain(
	    const std::vector<Botan::X509_Certificate> &cert_chain)
	{
		if (ctx->peer_chain_cb == nullptr || cert_chain.empty())
			return;
		try {
			std::vector<CertificateStrings> strings;
			strings.reserve(cert_chain.size());
			for (const auto &cert : cert_chain) {
				CertificateStrings item;
				item.subject = cert.subject_dn().to_string();
				item.issuer = cert.issuer_dn().to_string();
				item.not_before = readable_time(cert.not_before());
				item.not_after = readable_time(cert.not_after());
				item.fingerprint = cert.fingerprint("SHA-256");
				item.pem = cert.PEM_encode();
				strings.push_back(std::move(item));
			}
			std::vector<xp_tls_peer_certificate> certificates;
			certificates.reserve(strings.size());
			for (const auto &item : strings) {
				certificates.push_back({
				    item.subject.c_str(), item.issuer.c_str(),
				    item.not_before.c_str(), item.not_after.c_str(),
				    item.fingerprint.c_str(), item.pem.c_str()});
			}
			ctx->peer_chain_cb(ctx->peer_chain_cb_arg,
			    certificates.data(), certificates.size());
		}
		catch (...) {
			/* Diagnostics must never change handshake success or failure. */
		}
	}

	xp_tls_ctx *ctx;
	bool verify_peer;
	const Botan::X509_Certificate *pinned_cert;
};

/* --------------------------------------------------- socket helpers */

class OperationDeadline {
public:
	explicit OperationDeadline(int timeout_ms)
	    : infinite_(timeout_ms < 0), end_(std::chrono::steady_clock::now())
	{
		if (!infinite_)
			end_ += std::chrono::milliseconds(timeout_ms);
	}

	int remaining() const
	{
		if (infinite_)
			return -1;
		auto value = std::chrono::duration_cast<std::chrono::milliseconds>(
		    end_ - std::chrono::steady_clock::now()).count();
		return value <= 0 ? 0 : static_cast<int>(value);
	}

private:
	bool infinite_;
	std::chrono::steady_clock::time_point end_;
};

/*
 * Flush pending_tx out to the socket.  Returns XP_TLS_OK on success,
 * XP_TLS_ERR_CLOSED on peer close, XP_TLS_ERR on other error.
 *
 * Each send() is gated on socket_writable() — same pattern that
 * rlogin_output_thread uses — so a peer-closed or locally-shut-down
 * socket reports as not-writable and we surface XP_TLS_ERR_CLOSED
 * rather than calling send() and triggering SIGPIPE.
 */
static int
flush_pending(xp_tls_ctx *ctx, OperationDeadline& deadline)
{
	std::lock_guard<std::mutex> wire_guard(ctx->wire_write_lock);
	for (;;) {
		std::vector<uint8_t> pending;
		{
			std::lock_guard<std::mutex> provider_guard(ctx->provider_lock);
			if (ctx->pending_tx.empty())
				return XP_TLS_OK;
			pending = ctx->pending_tx;
		}
		if (!socket_writable(ctx->sock, deadline.remaining())) {
			return XP_TLS_TIMEOUT;
		}
		int flags = 0;
#ifdef MSG_NOSIGNAL
		flags = MSG_NOSIGNAL;
#endif
		ssize_t n = send(ctx->sock,
		    reinterpret_cast<const char *>(pending.data()), pending.size(), flags);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			if (errno == EPIPE || errno == ECONNRESET)
				return XP_TLS_ERR_CLOSED;
			snprintf(ctx->err, sizeof(ctx->err),
			    "send syscall error (errno=%d)", errno);
			return XP_TLS_ERR;
		}
		if (n == 0)
			return XP_TLS_ERR_CLOSED;
		{
			std::lock_guard<std::mutex> provider_guard(ctx->provider_lock);
			size_t sent = std::min(static_cast<size_t>(n), ctx->pending_tx.size());
			ctx->pending_tx.erase(ctx->pending_tx.begin(),
			    ctx->pending_tx.begin() + sent);
		}
	}
}

/*
 * Read some ciphertext from the socket and feed it to Botan.  Returns
 * XP_TLS_OK on progress, XP_TLS_TIMEOUT if the read-timeout elapsed
 * with no data, XP_TLS_ERR_CLOSED on clean close, XP_TLS_ERR otherwise.
 */
static int
pump_recv(xp_tls_ctx *ctx, OperationDeadline& deadline)
{
	if (!socket_readable(ctx->sock, deadline.remaining()))
		return XP_TLS_TIMEOUT;
	uint8_t buf[4096];
	ssize_t n = recv(ctx->sock, reinterpret_cast<char *>(buf), sizeof(buf), 0);
	if (n < 0) {
		if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
			return XP_TLS_TIMEOUT;
		if (errno == ECONNRESET || errno == EPIPE)
			return XP_TLS_ERR_CLOSED;
		snprintf(ctx->err, sizeof(ctx->err),
		    "recv syscall error (errno=%d)", errno);
		return XP_TLS_ERR;
	}
	if (n == 0)
		return XP_TLS_ERR_CLOSED;
	{
		std::lock_guard<std::mutex> provider_guard(ctx->provider_lock);
		if (ctx->terminating)
			return XP_TLS_ERR_CLOSED;
		try {
			ctx->channel->received_data(std::span(buf, static_cast<size_t>(n)));
		}
		catch (const std::exception &e) {
			snprintf(ctx->err, sizeof(ctx->err), "TLS: %s", e.what());
			return XP_TLS_ERR;
		}
		/* received_data() can deliver application data and a following alert
		   from the same socket read. Preserve the provider-neutral stream
		   contract by making the authenticated plaintext visible before the
		   subsequent call reports the alert or orderly close. */
		if (!ctx->plaintext.empty())
			return XP_TLS_OK;
		if (ctx->fatal_alert) {
			snprintf(ctx->err, sizeof(ctx->err), "TLS fatal alert received");
			return XP_TLS_ERR;
		}
		if (ctx->peer_closed)
			return XP_TLS_ERR_CLOSED;
	}
	return XP_TLS_OK;
}

/* ---------------------------------------------------------- open */

static xp_tls_t
client_open_inner(SOCKET sock, const struct xp_tls_client_config *config)
{
	const char *sni = config->server_name;
	const char *psk_identity = config->psk_identity;
	const void *psk = config->psk;
	size_t psk_len = config->psk_len;
	last_err_buf[0] = 0;
	auto *ctx = new (std::nothrow) xp_tls_ctx{};
	if (ctx == nullptr) {
		set_last_err("xp_tls_client_open: out of memory");
		return nullptr;
	}
	ctx->sock               = sock;
	ctx->peer_closed        = false;
	ctx->fatal_alert        = false;
	ctx->psk_used           = false;
	ctx->protocol_version   = XP_TLS_VERSION_UNKNOWN;
	ctx->peer_chain_cb      = config->peer_chain_cb;
	ctx->peer_chain_cb_arg  = config->peer_chain_cb_arg;
	if (sni != nullptr && sni[0] != 0)
		ctx->sni = sni;
	if (psk_identity != nullptr)
		ctx->selected_psk_identity = psk_identity;

	try {
		const Botan::X509_Certificate *pinned = nullptr;
		ctx->rng       = std::make_shared<Botan::AutoSeeded_RNG>();
		ctx->sess_mgr  = std::make_shared<Botan::TLS::Session_Manager_Noop>();
		if (psk_identity != nullptr && psk != nullptr && psk_len > 0) {
			Botan::secure_vector<uint8_t> secret(
			    static_cast<const uint8_t *>(psk),
			    static_cast<const uint8_t *>(psk) + psk_len);
			ctx->creds = std::make_shared<ClientCredentials>(
			    std::string(psk_identity), std::move(secret));
			if (config->psk_version == XP_TLS_VERSION_1_3)
				ctx->policy = std::make_shared<CertificatePolicy>(
				    XP_TLS_VERSION_1_3, XP_TLS_VERSION_1_3);
			else
				ctx->policy = std::make_shared<PskPolicy>(config->psk_version,
				    config->psk_policy);
		}
		else {
			auto client_creds = std::make_shared<ClientCredentials>(
			    config->server_auth, config->trusted_cert_file,
			    config->client_cert_file, config->client_key_file,
			    config->client_identity,
			    config->private_key_password,
			    config->private_key_password_arg);
			pinned = client_creds->custom_trust_certificate();
			ctx->creds = std::move(client_creds);
			auto minimum = config->min_version == XP_TLS_VERSION_UNKNOWN
			    ? XP_TLS_VERSION_1_2 : config->min_version;
			ctx->policy = std::make_shared<CertificatePolicy>(minimum,
			    config->max_version);
		}
		ctx->callbacks = std::make_shared<XpTlsCallbacks>(ctx,
		    psk_identity == nullptr &&
		        config->server_auth != XP_TLS_SERVER_AUTH_NONE,
		    pinned);

		Botan::TLS::Server_Information server_info(ctx->sni);
		Botan::TLS::Protocol_Version offer =
		    psk_identity == nullptr
		    ? (config->max_version == XP_TLS_VERSION_1_2
		        ? Botan::TLS::Protocol_Version::TLS_V12
		        : Botan::TLS::Protocol_Version::TLS_V13)
		    : (config->psk_version == XP_TLS_VERSION_1_3
		        ? Botan::TLS::Protocol_Version::TLS_V13
		        : Botan::TLS::Protocol_Version::TLS_V12);
		ctx->channel = std::make_unique<Botan::TLS::Client>(
		    ctx->callbacks, ctx->sess_mgr, ctx->creds, ctx->policy, ctx->rng,
		    server_info, offer);
	}
	catch (const std::exception &e) {
		set_last_err("TLS client construct: %s", e.what());
		delete ctx;
		return nullptr;
	}

	/*
	 * Drive the handshake: emit any queued ClientHello, then read
	 * responses until the session is active. The handshake has one monotonic
	 * deadline and never modifies timeout state on the caller-owned socket.
	 */
	OperationDeadline deadline(30000);
	while (!ctx->channel->is_active()) {
		int rc = flush_pending(ctx, deadline);
		if (rc == XP_TLS_TIMEOUT) {
			set_last_err("TLS handshake timed out");
			delete ctx;
			return nullptr;
		}
		if (rc < 0) {
			set_last_err("handshake flush: %s",
			    rc == XP_TLS_ERR_CLOSED ? "peer closed" : ctx->err);
			delete ctx;
			return nullptr;
		}
		rc = pump_recv(ctx, deadline);
		if (rc == XP_TLS_TIMEOUT) {
			set_last_err("TLS handshake timed out");
			delete ctx;
			return nullptr;
		}
		if (rc < 0) {
			set_last_err("handshake recv: %s",
			    rc == XP_TLS_ERR_CLOSED ? "peer closed" : ctx->err);
			delete ctx;
			return nullptr;
		}
	}
	if (psk_identity != nullptr && !ctx->psk_used) {
		set_last_err("TLS handshake did not select the configured PSK");
		delete ctx;
		return nullptr;
	}

	/* The server might have queued session-ticket messages etc after
	   handshake completion; flush anything still pending. */
	(void)flush_pending(ctx, deadline);

	return ctx;
}

extern "C" xp_tls_t
xp_tls_client_open_config(SOCKET sock,
                          const struct xp_tls_client_config *config)
{
	if (config == nullptr) {
		set_last_err("xp_tls_client_open_config: missing configuration");
		return nullptr;
	}
	bool have_psk_identity = config->psk_identity != nullptr &&
	    config->psk_identity[0] != 0;
	bool have_psk = config->psk != nullptr && config->psk_len > 0;
	if (static_cast<unsigned>(config->psk_policy) >
	    XP_TLS_PSK_POLICY_TLS12_COMPATIBILITY) {
		set_last_err("xp_tls_client_open_config: invalid PSK policy");
		return nullptr;
	}
	if (have_psk_identity != have_psk) {
		set_last_err("xp_tls_client_open_config: incomplete PSK configuration");
		return nullptr;
	}
	if (have_psk && config->psk_version != XP_TLS_VERSION_1_2 &&
	    config->psk_version != XP_TLS_VERSION_1_3) {
		set_last_err("xp_tls_client_open_config: PSK protocol version must be TLS 1.2 or TLS 1.3");
		return nullptr;
	}
	if (!have_psk && config->psk_version != XP_TLS_VERSION_UNKNOWN) {
		set_last_err("xp_tls_client_open_config: PSK protocol version set without a PSK");
		return nullptr;
	}
	bool have_client_cert = config->client_cert_file != nullptr &&
	    config->client_cert_file[0] != 0;
	bool have_client_key = config->client_key_file != nullptr &&
	    config->client_key_file[0] != 0;
	bool have_client_identity = config->client_identity != nullptr;
	if (!have_psk && have_client_cert != have_client_key) {
		set_last_err("xp_tls_client_open_config: client certificate and key must both be set");
		return nullptr;
	}
	if (!have_psk && have_client_identity
	    && (have_client_cert || have_client_key)) {
		set_last_err("xp_tls_client_open_config: client identity and client identity files are mutually exclusive");
		return nullptr;
	}
	if (!have_psk && have_client_identity) {
		int status = xp_tls_client_identity_validate(config->client_identity);
		if (status != XP_CRYPTO_OK) {
			set_last_err("xp_tls_client_open_config: invalid client identity: %s",
			             xp_crypto_status_string(status));
			return nullptr;
		}
	}
	if (!have_psk &&
	    static_cast<unsigned>(config->server_auth) > XP_TLS_SERVER_AUTH_UNTRUSTED) {
		set_last_err("xp_tls_client_open_config: invalid server authentication mode");
		return nullptr;
	}
	if (!have_psk && config->server_auth == XP_TLS_SERVER_AUTH_CERTIFICATE &&
	    (config->trusted_cert_file == nullptr ||
	     config->trusted_cert_file[0] == 0)) {
		set_last_err("xp_tls_client_open_config: certificate authentication has no trust anchor");
		return nullptr;
	}
	if (!have_psk && config->server_auth != XP_TLS_SERVER_AUTH_CERTIFICATE &&
	    config->trusted_cert_file != nullptr && config->trusted_cert_file[0] != 0) {
		set_last_err("xp_tls_client_open_config: trusted certificate set for the wrong authentication mode");
		return nullptr;
	}
	if (!have_psk && config->server_auth != XP_TLS_SERVER_AUTH_NONE &&
	    (config->server_name == nullptr || config->server_name[0] == 0)) {
		set_last_err("xp_tls_client_open_config: authenticated TLS requires a server name");
		return nullptr;
	}
	if (!have_psk) {
		auto minimum = config->min_version == XP_TLS_VERSION_UNKNOWN
		    ? XP_TLS_VERSION_1_2 : config->min_version;
		if (minimum < XP_TLS_VERSION_1_2 ||
		    (config->max_version != XP_TLS_VERSION_UNKNOWN &&
		     config->max_version < XP_TLS_VERSION_1_2) ||
		    (config->max_version != XP_TLS_VERSION_UNKNOWN &&
		     minimum > config->max_version)) {
			set_last_err("Botan does not support the requested TLS version range");
			return nullptr;
		}
	}
	return client_open_inner(sock, config);
}

extern "C" enum xp_tls_version
xp_tls_protocol_version(xp_tls_t ctx)
{
	return ctx == nullptr ? XP_TLS_VERSION_UNKNOWN : ctx->protocol_version;
}

extern "C" xp_tls_t
xp_tls_provider_server_open(SOCKET socket, const void *chain_pem,
	size_t chain_pem_length, xp_key_t private_key,
	const struct xp_tls_server_config *config)
{
	if (config == nullptr
	    || ((chain_pem == nullptr) != (private_key == nullptr))
	    || (private_key == nullptr && config->psk_lookup == nullptr)
	    || static_cast<unsigned>(config->psk_policy) >
	       XP_TLS_PSK_POLICY_TLS12_COMPATIBILITY
	    || static_cast<unsigned>(config->client_auth) >
	       XP_TLS_CLIENT_AUTH_REQUIRE_VALID
	    || (config->client_auth == XP_TLS_CLIENT_AUTH_REQUIRE_VALID
	        && (config->client_ca_file == nullptr || config->client_ca_file[0] == 0))) {
		set_last_err("xp_tls_server_open: invalid configuration");
		return nullptr;
	}
	auto minimum = config->min_version == XP_TLS_VERSION_UNKNOWN
	    ? XP_TLS_VERSION_1_2 : config->min_version;
	if (minimum < XP_TLS_VERSION_1_2
	    || (config->max_version != XP_TLS_VERSION_UNKNOWN
	        && config->max_version < XP_TLS_VERSION_1_2)
	    || (config->max_version != XP_TLS_VERSION_UNKNOWN
	        && minimum > config->max_version)) {
		set_last_err("Botan does not support the requested TLS version range");
		return nullptr;
	}
	auto *ctx = new (std::nothrow) xp_tls_ctx{};
	if (ctx == nullptr) {
		set_last_err("xp_tls_server_open: out of memory");
		return nullptr;
	}
	ctx->sock = socket;
	ctx->protocol_version = XP_TLS_VERSION_UNKNOWN;
	ctx->server_key = private_key;
	if (private_key != nullptr)
		xp_key_retain(private_key);
	try {
		ctx->rng = std::make_shared<Botan::AutoSeeded_RNG>();
		ctx->sess_mgr = std::make_shared<Botan::TLS::Session_Manager_Noop>();
		ctx->creds = std::make_shared<ServerCredentials>(
		    chain_pem, chain_pem_length, private_key,
		    config->client_auth == XP_TLS_CLIENT_AUTH_REQUIRE_VALID
		        ? config->client_ca_file : nullptr,
		    config->psk_lookup, config->psk_lookup_arg,
		    &ctx->selected_psk_identity);
		ctx->policy = std::make_shared<ServerPolicy>(minimum,
		    config->max_version, config->client_auth,
		    config->psk_lookup != nullptr, config->psk_policy);
		ctx->callbacks = std::make_shared<XpTlsCallbacks>(ctx,
		    config->client_auth == XP_TLS_CLIENT_AUTH_REQUIRE_VALID, nullptr);
		ctx->channel = std::make_unique<Botan::TLS::Server>(
		    ctx->callbacks, ctx->sess_mgr, ctx->creds, ctx->policy, ctx->rng);
	}
	catch (const std::exception& e) {
		set_last_err("TLS server construct: %s", e.what());
		xp_tls_close(ctx, false);
		return nullptr;
	}
	OperationDeadline handshake_deadline(config->handshake_timeout_ms);
	/* Botan can make a server channel writable after sending its Finished but
	   before it has authenticated the client's Finished.  The public server
	   constructor is synchronous, so wait for the established-session callback,
	   not merely for is_active(). */
	while (ctx->protocol_version == XP_TLS_VERSION_UNKNOWN) {
		int status = pump_recv(ctx, handshake_deadline);
		if (status == XP_TLS_TIMEOUT) {
			set_last_err("TLS server handshake timed out");
			xp_tls_close(ctx, false);
			return nullptr;
		}
		if (status < 0) {
			set_last_err("TLS server handshake receive failed: %s", ctx->err);
			xp_tls_close(ctx, false);
			return nullptr;
		}
		status = flush_pending(ctx, handshake_deadline);
		if (status == XP_TLS_TIMEOUT) {
			set_last_err("TLS server handshake timed out");
			xp_tls_close(ctx, false);
			return nullptr;
		}
		if (status < 0) {
			set_last_err("TLS server handshake send failed: %s", ctx->err);
			xp_tls_close(ctx, false);
			return nullptr;
		}
	}
	(void)flush_pending(ctx, handshake_deadline);
	return ctx;
}

/* ---------------------------------------------------------- push */

extern "C" int
xp_tls_pop_timeout(xp_tls_t ctx, void *buf, size_t n, size_t *copied,
	int timeout_ms)
{
	if (copied != nullptr) *copied = 0;
	if (ctx == nullptr || (buf == nullptr && n != 0)) return XP_TLS_ERR;
	if (n == 0) return XP_TLS_OK;
	std::lock_guard<std::mutex> read_guard(ctx->read_lock);
	OperationDeadline deadline(timeout_ms);
	for (;;) {
		{
			std::lock_guard<std::mutex> provider_guard(ctx->provider_lock);
			if (ctx->terminating)
				return XP_TLS_ERR_CLOSED;
			if (!ctx->plaintext.empty())
				break;
			if (ctx->fatal_alert) {
				snprintf(ctx->err, sizeof(ctx->err),
				    "TLS fatal alert received");
				return XP_TLS_ERR;
			}
			if (ctx->peer_closed)
				return XP_TLS_ERR_CLOSED;
		}
		int status = pump_recv(ctx, deadline);
		if (status != XP_TLS_OK)
			return status;
		/* A read may generate protocol traffic. Its initiating read deadline
		   applies to that write as well. Authenticated plaintext still takes
		   precedence if that reply cannot be sent after a simultaneous peer
		   close. */
		status = flush_pending(ctx, deadline);
		if (status != XP_TLS_OK) {
			std::lock_guard<std::mutex> provider_guard(ctx->provider_lock);
			if (ctx->plaintext.empty())
				return status;
		}
	}
	size_t take;
	{
		std::lock_guard<std::mutex> provider_guard(ctx->provider_lock);
		take = std::min(n, ctx->plaintext.size());
		for (size_t i = 0; i < take; ++i) {
			static_cast<uint8_t *>(buf)[i] = ctx->plaintext.front();
			ctx->plaintext.pop_front();
		}
	}
	if (copied != nullptr) *copied = take;
	return XP_TLS_OK;
}

extern "C" int
xp_tls_push_timeout(xp_tls_t ctx, const void *buf, size_t n, size_t *copied,
	int timeout_ms)
{
	if (copied != nullptr) *copied = 0;
	if (ctx == nullptr || (buf == nullptr && n != 0)) return XP_TLS_ERR;
	if (n == 0) return XP_TLS_OK;
	std::lock_guard<std::mutex> write_guard(ctx->write_lock);
	if (ctx->terminating)
		return XP_TLS_ERR_CLOSED;
	OperationDeadline deadline(timeout_ms);
	int status = flush_pending(ctx, deadline);
	if (status != XP_TLS_OK)
		return status;
	{
		std::lock_guard<std::mutex> provider_guard(ctx->provider_lock);
		if (ctx->terminating)
			return XP_TLS_ERR_CLOSED;
		try {
			ctx->channel->send(std::span(static_cast<const uint8_t *>(buf), n));
		}
		catch (const std::exception& e) {
			snprintf(ctx->err, sizeof(ctx->err), "TLS send: %s", e.what());
			return XP_TLS_ERR;
		}
	}
	if (copied != nullptr) *copied = n;
	status = flush_pending(ctx, deadline);
	/* Botan has accepted all application bytes at this point. Ciphertext may
	   remain queued for a later flush, but replaying the plaintext would be
	   incorrect, so report the completed application write. */
	return status == XP_TLS_TIMEOUT ? XP_TLS_OK : status;
}

extern "C" int xp_tls_flush_timeout(xp_tls_t ctx, int timeout_ms)
{
	if (ctx == nullptr) return XP_TLS_ERR;
	std::lock_guard<std::mutex> write_guard(ctx->write_lock);
	if (ctx->terminating)
		return XP_TLS_ERR_CLOSED;
	OperationDeadline deadline(timeout_ms);
	return flush_pending(ctx, deadline);
}

extern "C" const char *xp_tls_cipher_name(xp_tls_t ctx)
{
	return ctx == nullptr || ctx->cipher_name.empty() ? nullptr : ctx->cipher_name.c_str();
}

extern "C" enum xp_tls_auth_method xp_tls_authentication_method(xp_tls_t ctx)
{
	if (ctx == nullptr) return XP_TLS_AUTH_NONE;
	return ctx->psk_used ? XP_TLS_AUTH_PSK : XP_TLS_AUTH_CERTIFICATE;
}

extern "C" int xp_tls_psk_identity(xp_tls_t ctx, void *out, size_t *len)
{
	if (ctx == nullptr || len == nullptr) return XP_CRYPTO_ERR_INVALID;
	if (!ctx->psk_used || ctx->selected_psk_identity.empty()) {
		*len = 0; return XP_CRYPTO_ERR_NOT_FOUND;
	}
	if (out == nullptr) { *len = ctx->selected_psk_identity.size(); return XP_CRYPTO_OK; }
	if (*len < ctx->selected_psk_identity.size()) {
		*len = ctx->selected_psk_identity.size(); return XP_CRYPTO_ERR_BUFFER_TOO_SMALL;
	}
	std::memcpy(out, ctx->selected_psk_identity.data(), ctx->selected_psk_identity.size());
	*len = ctx->selected_psk_identity.size(); return XP_CRYPTO_OK;
}

extern "C" size_t xp_tls_peer_certificate_count(xp_tls_t ctx)
{
	return ctx == nullptr ? 0 : ctx->peer_chain_der.size();
}

extern "C" int xp_tls_peer_certificate_der(
	xp_tls_t ctx, size_t index, void *out, size_t *len)
{
	if (ctx == nullptr || len == nullptr) return XP_CRYPTO_ERR_INVALID;
	if (index >= ctx->peer_chain_der.size()) return XP_CRYPTO_ERR_NOT_FOUND;
	const auto& value = ctx->peer_chain_der[index];
	if (out == nullptr) { *len = value.size(); return XP_CRYPTO_OK; }
	if (*len < value.size()) { *len = value.size(); return XP_CRYPTO_ERR_BUFFER_TOO_SMALL; }
	std::memcpy(out, value.data(), value.size()); *len = value.size(); return XP_CRYPTO_OK;
}

extern "C" bool
xp_tls_has_pending(xp_tls_t ctx)
{
	if (ctx == nullptr)
		return false;
	std::lock_guard<std::mutex> provider_guard(ctx->provider_lock);
	return !ctx->plaintext.empty();
}

extern "C" bool
xp_tls_used_psk(xp_tls_t ctx)
{
	return ctx != nullptr && ctx->psk_used;
}

/* --------------------------------------------------------- close */

extern "C" int
xp_tls_terminate(xp_tls_t ctx)
{
	if (ctx == nullptr)
		return XP_TLS_ERR;
	bool was_terminating = ctx->terminating.exchange(true);
	if (!was_terminating) {
		std::lock_guard<std::mutex> provider_guard(ctx->provider_lock);
		try {
			if (ctx->channel && !ctx->channel->is_closed())
				ctx->channel->close();
		}
		catch (...) {
			/* The owner still performs socket shutdown to wake I/O. */
		}
	}
	return XP_TLS_OK;
}

extern "C" void
xp_tls_close(xp_tls_t ctx, bool close_socket)
{
	if (ctx == nullptr)
		return;
	/* Best-effort send the TLS close_notify alert, but only if the
	   socket is in a writable state.  Otherwise the send() inside
	   flush_pending() raises SIGPIPE on a peer-closed or
	   locally-shut-down socket and tears the whole process down.
	   socket_writable() with a 0ms timeout is the same gate
	   rlogin_output_thread() uses before each send(). */
	if (!ctx->terminating && ctx->sock != INVALID_SOCKET
	    && socket_writable(ctx->sock, 0)) {
		try {
			if (ctx->channel && !ctx->channel->is_closed())
				ctx->channel->close();
		}
		catch (...) {
			/* Peer may have already disconnected. */
		}
		OperationDeadline deadline(0);
		(void)flush_pending(ctx, deadline);
	}
	if (close_socket && ctx->sock != INVALID_SOCKET)
		closesocket(ctx->sock);
	if (ctx->server_key != nullptr)
		xp_key_release(ctx->server_key);
	delete ctx;
}

extern "C" const char *
xp_tls_errstr(xp_tls_t ctx)
{
	if (ctx == nullptr)
		return "(null xp_tls_t)";
	if (ctx->err[0] == 0)
		return "no error";
	return ctx->err;
}
