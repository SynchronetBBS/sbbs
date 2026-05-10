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
 * Peer-certificate verification is disabled (custom Credentials_Manager
 * returns no trusted CAs and the default verify callback accepts any
 * chain).  Matches the Cryptlib-era posture that the OpenSSL backend
 * also preserves — BBSes commonly serve self-signed certs.
 */

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <memory>
#include <span>
#include <string>
#include <vector>

/* xp_tls.h pulls in sockwrap.h, which lands the OS socket + timeval
 * headers in the right order (winsock2 + ws2tcpip + wspiapi on Win32,
 * sys/socket + sys/time + friends on Unix).  Don't reach past it. */
#include "xp_tls.h"

#include <botan/auto_rng.h>
#include <botan/credentials_manager.h>
#include <botan/secmem.h>
#include <botan/tls_alert.h>
#include <botan/tls_callbacks.h>
#include <botan/tls_client.h>
#include <botan/tls_policy.h>
#include <botan/tls_server_info.h>
#include <botan/tls_session.h>
#include <botan/tls_session_manager_noop.h>

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

/* ----------------------------------------------------- Credentials */

/*
 * Permissive credentials manager: no client certificate, no trusted CAs.
 * Combined with the verify callback below this gives us peer-verify=none
 * behaviour equivalent to the OpenSSL backend.
 *
 * Optionally also supplies a PSK for TLS-PSK clients.  When psk_secret_ is
 * empty, behaves as a pure cert-mode credentials manager (no PSK offered).
 */
class PermissiveCredentials : public Botan::Credentials_Manager {
public:
	PermissiveCredentials() = default;
	PermissiveCredentials(std::string identity, Botan::secure_vector<uint8_t> psk)
	    : psk_identity_(std::move(identity)), psk_secret_(std::move(psk)) {}

	std::vector<Botan::Certificate_Store *>
	trusted_certificate_authorities(const std::string &, const std::string &) override
	{
		return {};
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

private:
	/* secure_vector wipes its backing storage on destruction via Botan's
	   secure_allocator — std::vector<uint8_t> would leave the password
	   bytes in freed heap. */
	std::string                   psk_identity_;
	Botan::secure_vector<uint8_t> psk_secret_;
};

/*
 * Policy for the PSK leg of the MQTT spy connection.  Pinned to
 * TLS 1.2 with PSK-only kex.  We deliberately do NOT mix in cert
 * kex: cryptlib's TLS server (Synchronet internal broker) prefers
 * cert suites whenever they're offered, even with PSK ahead in the
 * client's list, and TLS 1.2 cipher-suite selection is server-driven
 * so client ordering is just a hint.  conn_mqtt.c therefore tries a
 * PSK-only handshake first; if that fails (external broker without
 * PSK identities), it reconnects and retries with the default policy
 * (cert auth).
 *
 * latest_supported_version() must also be overridden: Botan's
 * default computes it from allow_tls13(), but the Client
 * constructor's protocol-version check has been observed to ignore
 * allow_tls13() alone and throw with TLS_V13 as the offered version.
 */
class Tls12PskPolicy : public Botan::TLS::Policy {
public:
	bool allow_tls12() const override { return true; }
	bool allow_tls13() const override { return false; }
	Botan::TLS::Protocol_Version
	latest_supported_version(bool datagram) const override
	{
		if (datagram)
			return Botan::TLS::Protocol_Version::DTLS_V12;
		return Botan::TLS::Protocol_Version::TLS_V12;
	}
	std::vector<std::string> allowed_key_exchange_methods() const override
	{
		return { "DHE_PSK", "ECDHE_PSK", "PSK" };
	}
	std::vector<std::string> allowed_signature_methods() const override
	{
		return {};
	}
	std::vector<std::string> allowed_ciphers() const override
	{
		return {
		    "AES-128",
		    "AES-256",
		    "AES-128/GCM",
		    "AES-256/GCM",
		};
	}
	std::vector<std::string> allowed_macs() const override
	{
		return { "SHA-256", "SHA-384", "AEAD", "SHA-1" };
	}
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
	std::string                              kex_algo;	/* "PSK", "DHE_PSK", "ECDH", "RSA", ... */
	/* Botan 3's TLS::Client takes shared_ptrs for its dependencies, so
	   the surrounding objects have to be shared_ptr too. */
	std::shared_ptr<Botan::AutoSeeded_RNG>       rng;
	std::shared_ptr<Botan::TLS::Session_Manager> sess_mgr;
	std::shared_ptr<PermissiveCredentials>       creds;
	std::shared_ptr<Botan::TLS::Policy>          policy;
	std::shared_ptr<Botan::TLS::Callbacks>       callbacks;
	std::unique_ptr<Botan::TLS::Client>          client;
};

class XpTlsCallbacks : public Botan::TLS::Callbacks {
public:
	explicit XpTlsCallbacks(xp_tls_ctx *c) : ctx(c) {}

	void tls_session_established(const Botan::TLS::Session_Summary &s) override
	{
		ctx->kex_algo = s.kex_algo();
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

	void tls_verify_cert_chain(const std::vector<Botan::X509_Certificate> &,
	                           const std::vector<std::optional<Botan::OCSP::Response>> &,
	                           const std::vector<Botan::Certificate_Store *> &,
	                           Botan::Usage_Type,
	                           std::string_view,
	                           const Botan::TLS::Policy &) override
	{
		/* Accept any chain — matches the OpenSSL backend's
		   SSL_VERIFY_NONE.  See the TODO in xp_tls_openssl.c
		   client_open_inner(): the eventual cert-pinning path needs
		   to surface a yes/no/cancel modal on validation failure
		   (accept once / accept and update / abort), the same way
		   SyncTERM's SSH layer handles host-key changes — never
		   silently accept or reject; the user owns the trust call. */
	}

private:
	xp_tls_ctx *ctx;
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
client_open_inner(SOCKET sock, const char *sni, int read_timeout,
                  const char *psk_identity,
                  const void *psk, size_t psk_len)
{
	last_err_buf[0] = 0;
	auto *ctx = new (std::nothrow) xp_tls_ctx{};
	if (ctx == nullptr) {
		set_last_err("xp_tls_client_open: out of memory");
		return nullptr;
	}
	ctx->sock               = sock;
	ctx->read_timeout_sec   = read_timeout;
	ctx->peer_closed        = false;
	ctx->fatal_alert        = false;
	if (sni != nullptr && sni[0] != 0)
		ctx->sni = sni;

	try {
		ctx->rng       = std::make_shared<Botan::AutoSeeded_RNG>();
		ctx->sess_mgr  = std::make_shared<Botan::TLS::Session_Manager_Noop>();
		if (psk_identity != nullptr && psk != nullptr && psk_len > 0) {
			Botan::secure_vector<uint8_t> secret(
			    static_cast<const uint8_t *>(psk),
			    static_cast<const uint8_t *>(psk) + psk_len);
			ctx->creds = std::make_shared<PermissiveCredentials>(
			    std::string(psk_identity), std::move(secret));
			ctx->policy = std::make_shared<Tls12PskPolicy>();
		}
		else {
			ctx->creds  = std::make_shared<PermissiveCredentials>();
			ctx->policy = std::make_shared<Botan::TLS::Policy>();
		}
		ctx->callbacks = std::make_shared<XpTlsCallbacks>(ctx);

		Botan::TLS::Server_Information server_info(ctx->sni);
		/* Pass an explicit offer_version when running PSK: the Client
		   constructor's default is the compile-time latest_tls_version()
		   (TLS 1.3 when Botan was built with HAS_TLS_13), and the
		   constructor validates that against the policy *before*
		   consulting policy->latest_supported_version().  Passing
		   TLS_V12 directly sidesteps the contradiction. */
		Botan::TLS::Protocol_Version offer =
		    (psk_identity != nullptr && psk != nullptr && psk_len > 0)
		    ? Botan::TLS::Protocol_Version(Botan::TLS::Protocol_Version::TLS_V12)
		    : Botan::TLS::Protocol_Version::latest_tls_version();
		ctx->client = std::make_unique<Botan::TLS::Client>(
		    ctx->callbacks, ctx->sess_mgr, ctx->creds, ctx->policy, ctx->rng,
		    server_info, offer);
	}
	catch (const std::exception &e) {
		set_last_err("TLS client construct: %s", e.what());
		delete ctx;
		return nullptr;
	}

	if (set_recv_timeout(sock, read_timeout) != 0) {
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

	/* The server might have queued session-ticket messages etc after
	   handshake completion; flush anything still pending. */
	(void)flush_pending(ctx);

	return ctx;
}

extern "C" xp_tls_t
xp_tls_client_open(SOCKET sock, const char *sni, int read_timeout)
{
	return client_open_inner(sock, sni, read_timeout, nullptr, nullptr, 0);
}

extern "C" xp_tls_t
xp_tls_client_open_psk(SOCKET sock, const char *sni, int read_timeout,
                       const char *identity, const void *psk, size_t psk_len)
{
	if (identity == nullptr || psk == nullptr || psk_len == 0) {
		set_last_err("xp_tls_client_open_psk: missing identity or PSK");
		return nullptr;
	}
	return client_open_inner(sock, sni, read_timeout, identity, psk, psk_len);
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
	if (ctx == nullptr)
		return false;
	/* kex_algo is captured in tls_session_established (the
	   negotiation-complete callback).  Botan reports the PSK family as
	   "PSK", "DHE_PSK", "ECDHE_PSK", "RSA_PSK"; non-PSK kex algorithms
	   don't contain "PSK". */
	return ctx->kex_algo.find("PSK") != std::string::npos;
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
