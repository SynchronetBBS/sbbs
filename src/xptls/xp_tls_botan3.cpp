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
#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

/* xp_tls.h pulls in sockwrap.h, which lands the OS socket + timeval
 * headers in the right order (winsock2 + ws2tcpip + wspiapi on Win32,
 * sys/socket + sys/time + friends on Unix).  Don't reach past it. */
#include "xp_tls.h"

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
	                  const char *client_key_file)
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
			client_key_ = Botan::PKCS8::load_key(key_source);
		}
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
	find_cert_chain(const std::vector<std::string> &,
	                const std::vector<Botan::AlgorithmIdentifier> &,
	                const std::vector<Botan::X509_DN> &,
	                const std::string &type,
	                const std::string &) override
	{
		if (type != "tls-client")
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
};

/* Web-PKI clients normally soft-fail unavailable revocation information.
   Botan's base policy requires CRL/OCSP proof, which rejects many otherwise
   valid public sites, so retain signature/path/hostname checking without
   making revocation information mandatory. */
class CertificatePolicy : public Botan::TLS::Policy {
public:
	bool require_cert_revocation_info() const override { return false; }
};

/* A configured external PSK is bound to one exact protocol version. The new
   configuration API restricts TLS 1.2 to ephemeral PSK exchanges, and
   Botan's TLS 1.3 client offers only the forward-secret psk_dhe_ke mode. The
   legacy wrapper retains plain-PSK compatibility for existing MQTT users. */
class PskPolicy : public Botan::TLS::Policy {
public:
	PskPolicy(enum xp_tls_version version, bool legacy_psk)
	    : version_(version), legacy_psk_(legacy_psk) {}
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
		if (legacy_psk_)
			return { "DHE_PSK", "ECDHE_PSK", "PSK" };
		return { "DHE_PSK", "ECDHE_PSK" };
	}
	std::vector<std::string> allowed_signature_methods() const override
	{
		/* TLS 1.2 PSK suites authenticate implicitly through the key
		   exchange rather than with a certificate signature. */
		return { "IMPLICIT" };
	}
	std::vector<std::string> allowed_ciphers() const override
	{
		if (!legacy_psk_)
			return { "AES-128/GCM", "AES-256/GCM",
			         "ChaCha20Poly1305" };
		return {
		    "AES-128",
		    "AES-256",
		    "AES-128/GCM",
		    "AES-256/GCM",
		};
	}
	std::vector<std::string> allowed_macs() const override
	{
		if (!legacy_psk_)
			return { "AEAD" };
		return { "SHA-256", "SHA-384", "AEAD", "SHA-1" };
	}

private:
	enum xp_tls_version version_;
	bool legacy_psk_;
};

/* ------------------------------------------------------- Callbacks */

struct xp_tls_ctx {
	SOCKET                                   sock;
	int                                      read_timeout_sec;
	std::string                              sni;
	char                                     err[256];
	std::deque<uint8_t>                      plaintext;	/* decrypted data awaiting pop() */
	std::vector<uint8_t>                     pending_tx;	/* ciphertext waiting to go out */
	bool                                     peer_closed;
	bool                                     fatal_alert;
	bool                                     psk_used;
	enum xp_tls_version                      protocol_version;
	xp_tls_peer_chain_cb                     peer_chain_cb;
	void                                    *peer_chain_cb_arg;
	/* Botan 3's TLS::Client takes shared_ptrs for its dependencies, so
	   the surrounding objects have to be shared_ptr too. */
	std::shared_ptr<Botan::AutoSeeded_RNG>       rng;
	std::shared_ptr<Botan::TLS::Session_Manager> sess_mgr;
	std::shared_ptr<ClientCredentials>           creds;
	std::shared_ptr<Botan::TLS::Policy>          policy;
	std::shared_ptr<Botan::TLS::Callbacks>       callbacks;
	std::unique_ptr<Botan::TLS::Client>          client;
};

class XpTlsCallbacks : public Botan::TLS::Callbacks {
public:
	XpTlsCallbacks(xp_tls_ctx *c, bool verify,
	               const Botan::X509_Certificate *pinned)
	    : ctx(c), verify_peer(verify), pinned_cert(pinned) {}

	void tls_session_established(const Botan::TLS::Session_Summary &s) override
	{
		ctx->psk_used = s.psk_used();
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

static int
set_recv_timeout(SOCKET sock, int seconds)
{
	if (seconds <= 0)
		return 0;
#ifdef _WIN32
	DWORD tv = static_cast<DWORD>(seconds) * 1000;
	return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&tv), sizeof(tv));
#else
	struct timeval tv;
	tv.tv_sec  = seconds;
	tv.tv_usec = 0;
	return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&tv), sizeof(tv));
#endif
}

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
flush_pending(xp_tls_ctx *ctx)
{
	size_t off = 0;
	while (off < ctx->pending_tx.size()) {
		if (!socket_writable(ctx->sock, 100))
			return XP_TLS_ERR_CLOSED;
		ssize_t n = send(ctx->sock,
		    reinterpret_cast<const char *>(ctx->pending_tx.data() + off),
		    ctx->pending_tx.size() - off, 0);
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
		off += static_cast<size_t>(n);
	}
	ctx->pending_tx.clear();
	return XP_TLS_OK;
}

/*
 * Read some ciphertext from the socket and feed it to Botan.  Returns
 * XP_TLS_OK on progress, XP_TLS_TIMEOUT if the read-timeout elapsed
 * with no data, XP_TLS_ERR_CLOSED on clean close, XP_TLS_ERR otherwise.
 */
static int
pump_recv(xp_tls_ctx *ctx)
{
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
	try {
		ctx->client->received_data(std::span(buf, static_cast<size_t>(n)));
	}
	catch (const std::exception &e) {
		snprintf(ctx->err, sizeof(ctx->err), "TLS: %s", e.what());
		return XP_TLS_ERR;
	}
	if (ctx->fatal_alert) {
		snprintf(ctx->err, sizeof(ctx->err), "TLS fatal alert received");
		return XP_TLS_ERR;
	}
	if (ctx->peer_closed)
		return XP_TLS_ERR_CLOSED;
	return XP_TLS_OK;
}

/* ---------------------------------------------------------- open */

static xp_tls_t
client_open_inner(SOCKET sock, const struct xp_tls_client_config *config,
                  bool legacy_psk)
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
	ctx->read_timeout_sec   = config->read_timeout;
	ctx->peer_closed        = false;
	ctx->fatal_alert        = false;
	ctx->psk_used           = false;
	ctx->protocol_version   = XP_TLS_VERSION_UNKNOWN;
	ctx->peer_chain_cb      = config->peer_chain_cb;
	ctx->peer_chain_cb_arg  = config->peer_chain_cb_arg;
	if (sni != nullptr && sni[0] != 0)
		ctx->sni = sni;

	try {
		ctx->rng       = std::make_shared<Botan::AutoSeeded_RNG>();
		ctx->sess_mgr  = std::make_shared<Botan::TLS::Session_Manager_Noop>();
		if (psk_identity != nullptr && psk != nullptr && psk_len > 0) {
			Botan::secure_vector<uint8_t> secret(
			    static_cast<const uint8_t *>(psk),
			    static_cast<const uint8_t *>(psk) + psk_len);
			ctx->creds = std::make_shared<ClientCredentials>(
			    std::string(psk_identity), std::move(secret));
			ctx->policy = std::make_shared<PskPolicy>(config->psk_version,
			    legacy_psk);
		}
		else {
			ctx->creds = std::make_shared<ClientCredentials>(
			    config->server_auth, config->trusted_cert_file,
			    config->client_cert_file, config->client_key_file);
			ctx->policy = std::make_shared<CertificatePolicy>();
		}
		ctx->callbacks = std::make_shared<XpTlsCallbacks>(ctx,
		    psk_identity == nullptr &&
		        config->server_auth != XP_TLS_SERVER_AUTH_NONE,
		    ctx->creds->custom_trust_certificate());

		Botan::TLS::Server_Information server_info(ctx->sni);
		Botan::TLS::Protocol_Version offer =
		    psk_identity == nullptr
		    ? Botan::TLS::Protocol_Version::latest_tls_version()
		    : (config->psk_version == XP_TLS_VERSION_1_3
		        ? Botan::TLS::Protocol_Version::TLS_V13
		        : Botan::TLS::Protocol_Version::TLS_V12);
		ctx->client = std::make_unique<Botan::TLS::Client>(
		    ctx->callbacks, ctx->sess_mgr, ctx->creds, ctx->policy, ctx->rng,
		    server_info, offer);
	}
	catch (const std::exception &e) {
		set_last_err("TLS client construct: %s", e.what());
		delete ctx;
		return nullptr;
	}

	if (set_recv_timeout(sock, config->read_timeout) != 0) {
		set_last_err("setsockopt SO_RCVTIMEO failed (errno=%d)", errno);
		delete ctx;
		return nullptr;
	}

	/*
	 * Drive the handshake: emit any queued ClientHello, then read
	 * responses until the session is active.  recv() blocks with
	 * SO_RCVTIMEO so we loop past spurious timeouts.
	 */
	int attempts = 0;
	while (!ctx->client->is_active()) {
		int rc = flush_pending(ctx);
		if (rc < 0) {
			set_last_err("handshake flush: %s",
			    rc == XP_TLS_ERR_CLOSED ? "peer closed" : ctx->err);
			delete ctx;
			return nullptr;
		}
		rc = pump_recv(ctx);
		if (rc == XP_TLS_TIMEOUT) {
			if (++attempts > 30) {	/* ~30s worth of 1s timeouts */
				set_last_err("TLS handshake timed out");
				delete ctx;
				return nullptr;
			}
			continue;
		}
		if (rc < 0) {
			set_last_err("handshake recv: %s",
			    rc == XP_TLS_ERR_CLOSED ? "peer closed" : ctx->err);
			delete ctx;
			return nullptr;
		}
		attempts = 0;
	}
	if (psk_identity != nullptr && !ctx->psk_used) {
		set_last_err("TLS handshake did not select the configured PSK");
		delete ctx;
		return nullptr;
	}

	/* The server might have queued session-ticket messages etc after
	   handshake completion; flush anything still pending. */
	(void)flush_pending(ctx);

	return ctx;
}

extern "C" xp_tls_t
xp_tls_client_open(SOCKET sock, const char *sni, int read_timeout)
{
	struct xp_tls_client_config config{};
	config.server_name = sni;
	config.read_timeout = read_timeout;
	return client_open_inner(sock, &config, false);
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
	if (!have_psk && have_client_cert != have_client_key) {
		set_last_err("xp_tls_client_open_config: client certificate and key must both be set");
		return nullptr;
	}
	if (!have_psk && (config->server_auth <= XP_TLS_SERVER_AUTH_NONE ||
	    config->server_auth > XP_TLS_SERVER_AUTH_UNTRUSTED)) {
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
	return client_open_inner(sock, config, false);
}

extern "C" xp_tls_t
xp_tls_client_open_psk(SOCKET sock, const char *sni, int read_timeout,
                       const char *identity, const void *psk, size_t psk_len)
{
	if (identity == nullptr || psk == nullptr || psk_len == 0) {
		set_last_err("xp_tls_client_open_psk: missing identity or PSK");
		return nullptr;
	}
	struct xp_tls_client_config config{};
	config.server_name = sni;
	config.read_timeout = read_timeout;
	config.psk_identity = identity;
	config.psk = psk;
	config.psk_len = psk_len;
	config.psk_version = XP_TLS_VERSION_1_2;
	return client_open_inner(sock, &config, true);
}

extern "C" enum xp_tls_version
xp_tls_protocol_version(xp_tls_t ctx)
{
	return ctx == nullptr ? XP_TLS_VERSION_UNKNOWN : ctx->protocol_version;
}

/* ---------------------------------------------------------- push */

extern "C" int
xp_tls_push(xp_tls_t ctx, const void *buf, size_t n, size_t *copied)
{
	if (copied != nullptr)
		*copied = 0;
	if (ctx == nullptr || buf == nullptr)
		return XP_TLS_ERR;
	if (n == 0)
		return XP_TLS_OK;
	try {
		ctx->client->send(std::span(static_cast<const uint8_t *>(buf), n));
	}
	catch (const std::exception &e) {
		snprintf(ctx->err, sizeof(ctx->err), "TLS send: %s", e.what());
		return XP_TLS_ERR;
	}
	int rc = flush_pending(ctx);
	if (rc == XP_TLS_OK && copied != nullptr)
		*copied = n;
	return rc;
}

/* ---------------------------------------------------------- pop */

extern "C" int
xp_tls_pop(xp_tls_t ctx, void *buf, size_t n, size_t *copied)
{
	if (copied != nullptr)
		*copied = 0;
	if (ctx == nullptr || buf == nullptr)
		return XP_TLS_ERR;
	if (n == 0)
		return XP_TLS_OK;

	if (ctx->plaintext.empty()) {
		int rc = pump_recv(ctx);
		if (rc == XP_TLS_TIMEOUT)
			return XP_TLS_TIMEOUT;
		if (rc < 0)
			return rc;
		/* flush any response Botan generated (e.g. key-update ack) */
		int rc2 = flush_pending(ctx);
		if (rc2 < 0)
			return rc2;
	}

	size_t take = n < ctx->plaintext.size() ? n : ctx->plaintext.size();
	for (size_t i = 0; i < take; i++) {
		static_cast<uint8_t *>(buf)[i] = ctx->plaintext.front();
		ctx->plaintext.pop_front();
	}
	if (copied != nullptr)
		*copied = take;
	return XP_TLS_OK;
}

/* --------------------------------------------------------- flush */

extern "C" int
xp_tls_flush(xp_tls_t ctx)
{
	if (ctx == nullptr)
		return XP_TLS_OK;
	return flush_pending(ctx);
}

extern "C" bool
xp_tls_has_pending(xp_tls_t ctx)
{
	if (ctx == nullptr)
		return false;
	return !ctx->plaintext.empty();
}

extern "C" bool
xp_tls_used_psk(xp_tls_t ctx)
{
	return ctx != nullptr && ctx->psk_used;
}

/* --------------------------------------------------------- close */

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
	if (ctx->sock != INVALID_SOCKET && socket_writable(ctx->sock, 0)) {
		try {
			if (ctx->client && !ctx->client->is_closed())
				ctx->client->close();
		}
		catch (...) {
			/* Peer may have already disconnected. */
		}
		(void)flush_pending(ctx);
	}
	if (close_socket && ctx->sock != INVALID_SOCKET)
		closesocket(ctx->sock);
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
