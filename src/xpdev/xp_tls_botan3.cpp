/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
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

#ifdef _WIN32
  #include <winsock2.h>
#else
  #include <sys/socket.h>
  #include <sys/time.h>
  #include <sys/types.h>
  #include <unistd.h>
#endif

#include <botan/auto_rng.h>
#include <botan/credentials_manager.h>
#include <botan/tls_alert.h>
#include <botan/tls_callbacks.h>
#include <botan/tls_client.h>
#include <botan/tls_policy.h>
#include <botan/tls_server_info.h>
#include <botan/tls_session_manager_noop.h>

#include "xp_tls.h"

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
 */
class PermissiveCredentials : public Botan::Credentials_Manager {
public:
	std::vector<Botan::Certificate_Store *>
	trusted_certificate_authorities(const std::string &, const std::string &) override
	{
		return {};
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
		/* Accept any chain — matches Cryptlib-era posture. */
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
 */
static int
flush_pending(xp_tls_ctx *ctx)
{
	size_t off = 0;
	while (off < ctx->pending_tx.size()) {
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

extern "C" xp_tls_t
xp_tls_client_open(SOCKET sock, const char *sni, int read_timeout)
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
		ctx->creds     = std::make_shared<PermissiveCredentials>();
		ctx->policy    = std::make_shared<Botan::TLS::Policy>();
		ctx->callbacks = std::make_shared<XpTlsCallbacks>(ctx);

		Botan::TLS::Server_Information server_info(ctx->sni);
		ctx->client = std::make_unique<Botan::TLS::Client>(
		    ctx->callbacks, ctx->sess_mgr, ctx->creds, ctx->policy, ctx->rng,
		    server_info);
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

/* --------------------------------------------------------- close */

extern "C" void
xp_tls_close(xp_tls_t ctx, bool close_socket)
{
	if (ctx == nullptr)
		return;
	try {
		if (ctx->client && !ctx->client->is_closed())
			ctx->client->close();
	}
	catch (...) {
		/* Best-effort close.  Peer may have already disconnected. */
	}
	(void)flush_pending(ctx);
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
