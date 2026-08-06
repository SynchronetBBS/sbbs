#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "xp_ca.h"
#include "xp_tls.h"

namespace {

struct SocketPair {
	SOCKET client = INVALID_SOCKET;
	SOCKET server = INVALID_SOCKET;

	~SocketPair()
	{
		if (client != INVALID_SOCKET)
			closesocket(client);
		if (server != INVALID_SOCKET)
			closesocket(server);
	}
};

struct Session {
	xp_tls_t value = nullptr;
	~Session() { xp_tls_close(value, false); }
};

struct ServerResult {
	xp_tls_t session = nullptr;
	std::string error;
};

struct SocketTimeouts {
	long long receive = -1;
	long long send = -1;
	bool operator==(const SocketTimeouts& other) const
	{
		return receive == other.receive && send == other.send;
	}
};

long long socket_timeout(SOCKET socket, int option)
{
#ifdef _WIN32
	DWORD value = 0;
	int length = sizeof(value);
	return getsockopt(socket, SOL_SOCKET, option,
	                  reinterpret_cast<char *>(&value), &length) == 0
	    ? value : -1;
#else
	struct timeval value{};
	socklen_t length = sizeof(value);
	return getsockopt(socket, SOL_SOCKET, option, &value, &length) == 0
	    ? static_cast<long long>(value.tv_sec) * 1000 + value.tv_usec / 1000
	    : -1;
#endif
}

SocketTimeouts socket_timeouts(SOCKET socket)
{
	return {socket_timeout(socket, SO_RCVTIMEO),
	        socket_timeout(socket, SO_SNDTIMEO)};
}

class TempFile {
public:
	explicit TempFile(const char *tag)
	{
		char name[160];
		std::snprintf(name, sizeof(name), "xptls-%s-test-%ld.pem", tag,
		              static_cast<long>(getpid()));
		path_ = name;
	}
	~TempFile() { std::remove(path_.c_str()); }
	const char *path() const { return path_.c_str(); }

private:
	std::string path_;
};

#define REQUIRE(condition)                                                     \
	do {                                                                         \
		if (!(condition)) {                                                        \
			std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);  \
			return false;                                                            \
		}                                                                          \
	} while (0)

bool connected_sockets(SocketPair& pair)
{
	SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listener == INVALID_SOCKET)
		return false;
	struct sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	address.sin_port = 0;
	if (bind(listener, reinterpret_cast<struct sockaddr *>(&address),
	         sizeof(address)) != 0 || listen(listener, 1) != 0) {
		closesocket(listener);
		return false;
	}
	socklen_t address_len = sizeof(address);
	if (getsockname(listener, reinterpret_cast<struct sockaddr *>(&address),
	                &address_len) != 0) {
		closesocket(listener);
		return false;
	}
	pair.client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (pair.client == INVALID_SOCKET ||
	    connect(pair.client, reinterpret_cast<struct sockaddr *>(&address),
	            sizeof(address)) != 0) {
		closesocket(listener);
		return false;
	}
	pair.server = accept(listener, nullptr, nullptr);
	closesocket(listener);
	return pair.server != INVALID_SOCKET;
}

bool write_certificates(const char *path, const xp_ca_cert_t *certificates,
	                    size_t count)
{
	size_t length = 0;
	if (xp_ca_cert_chain_export_pem(certificates, count, nullptr, &length) !=
	    XP_CA_OK)
		return false;
	std::vector<unsigned char> pem(length);
	if (xp_ca_cert_chain_export_pem(certificates, count, pem.data(), &length) !=
	    XP_CA_OK)
		return false;
	FILE *file = std::fopen(path, "wb");
	if (file == nullptr)
		return false;
	bool ok = std::fwrite(pem.data(), 1, length, file) == length;
	ok = std::fclose(file) == 0 && ok;
	return ok;
}

bool write_certificate(const char *path, xp_ca_cert_t certificate)
{
	return write_certificates(path, &certificate, 1);
}

int password_callback(void *context, void *out, size_t capacity,
	                  size_t *length)
{
	const char *password = static_cast<const char *>(context);
	size_t required = std::strlen(password);
	if (length == nullptr)
		return -1;
	if (out == nullptr) {
		*length = required;
		return 0;
	}
	if (capacity < required)
		return -1;
	std::memcpy(out, password, required);
	*length = required;
	return 0;
}

bool write_private_key(const char *path, xp_key_t key, const char *password)
{
	size_t length = 0;
	if (xp_key_export_private_pem(key, password_callback,
	                              const_cast<char *>(password), nullptr,
	                              &length) != XP_CRYPTO_OK)
		return false;
	std::vector<unsigned char> pem(length);
	if (xp_key_export_private_pem(key, password_callback,
	                              const_cast<char *>(password), pem.data(),
	                              &length) != XP_CRYPTO_OK)
		return false;
	FILE *file = std::fopen(path, "wb");
	if (file == nullptr)
		return false;
	bool ok = std::fwrite(pem.data(), 1, length, file) == length;
	ok = std::fclose(file) == 0 && ok;
	std::fill(pem.begin(), pem.end(), 0);
	return ok;
}

bool make_server_identity(xp_key_t *key, xp_ca_cert_t *certificate,
	                      const char *certificate_path)
{
	const struct xp_key_spec key_spec = {
		XP_KEY_RSA, 2048, XP_KEY_CURVE_NONE
	};
	const char *names[] = {"localhost"};
	time_t now = time(nullptr);
	struct xp_ca_issue_request request{};
	request.subject.common_name = "localhost";
	request.subject.dns_names = names;
	request.subject.dns_name_count = 1;
	request.policy.basic_constraints_critical = true;
	request.policy.key_usage_critical = true;
	request.policy.key_usage = XP_CA_KEY_USE_SIGN;
	request.policy.extended_key_usage = XP_CA_EKU_SERVER_AUTH;
	request.policy.path_length = -1;
	request.not_before = now - 60;
	request.not_after = now + 3600;
	return xp_key_generate(key, &key_spec) == XP_CA_OK &&
	    xp_ca_cert_create_self_signed(certificate, *key, &request) == XP_CA_OK &&
	    write_certificate(certificate_path, *certificate);
}

ServerResult start_server(SOCKET socket,
	                      xp_tls_server_credentials_t credentials,
	                      const struct xp_tls_server_config& config)
{
	ServerResult result;
	result.session = xp_tls_server_open(socket, credentials, &config);
	if (result.session == nullptr)
		result.error = xp_tls_last_err();
	return result;
}

int reject_psk(void *, const void *, size_t, void *, size_t *)
{
	return -1;
}

bool transfer_and_inspect(xp_tls_t client, xp_tls_t server)
{
	const char request[] = "abcdef";
	const char response[] = "response";
	std::array<char, 32> buffer{};
	size_t copied = 99;
	REQUIRE(xp_tls_pop_timeout(server, nullptr, 0, &copied, 0) == XP_TLS_OK);
	REQUIRE(copied == 0);
	copied = 99;
	REQUIRE(xp_tls_push_timeout(client, nullptr, 0, &copied, 0) == XP_TLS_OK);
	REQUIRE(copied == 0);
	REQUIRE(xp_tls_cipher_name(client) != nullptr);
	REQUIRE(xp_tls_cipher_name(server) != nullptr);
	REQUIRE(xp_tls_push_timeout(client, request, sizeof(request) - 1,
	                            &copied, 1000) == XP_TLS_OK);
	REQUIRE(copied == sizeof(request) - 1);
	REQUIRE(xp_tls_pop_timeout(server, buffer.data(), 1, &copied, 1000) ==
	        XP_TLS_OK);
	REQUIRE(copied == 1 && buffer[0] == request[0]);
	REQUIRE(xp_tls_has_pending(server));
	REQUIRE(xp_tls_pop_timeout(server, buffer.data(), buffer.size(), &copied,
	                           0) == XP_TLS_OK);
	REQUIRE(copied == sizeof(request) - 2);
	REQUIRE(std::memcmp(buffer.data(), request + 1, copied) == 0);
	REQUIRE(xp_tls_pop_timeout(server, buffer.data(), buffer.size(), &copied,
	                           0) == XP_TLS_TIMEOUT);
	REQUIRE(copied == 0);
	REQUIRE(xp_tls_push_timeout(server, response, sizeof(response) - 1,
	                            &copied, 1000) == XP_TLS_OK);
	REQUIRE(copied == sizeof(response) - 1);
	REQUIRE(xp_tls_pop_timeout(client, buffer.data(), buffer.size(), &copied,
	                           1000) == XP_TLS_OK);
	REQUIRE(copied == sizeof(response) - 1);
	REQUIRE(std::memcmp(buffer.data(), response, copied) == 0);
	return true;
}

bool concurrent_directions(xp_tls_t client, xp_tls_t server)
{
	std::array<char, 32> read_buffer{};
	size_t read_copied = 0;
	int read_status = XP_TLS_ERR;
	auto started = std::chrono::steady_clock::now();
	std::thread reader([&] {
		read_status = xp_tls_pop_timeout(server, read_buffer.data(),
		                                 read_buffer.size(), &read_copied, 150);
	});
	std::this_thread::sleep_for(std::chrono::milliseconds(25));
	const char outbound[] = "opposite-direction";
	size_t copied = 0;
	bool ok = xp_tls_push_timeout(server, outbound, sizeof(outbound) - 1,
	                             &copied, 250) == XP_TLS_OK &&
	          copied == sizeof(outbound) - 1;
	std::array<char, 32> client_buffer{};
	ok = xp_tls_pop_timeout(client, client_buffer.data(), client_buffer.size(),
	                        &copied, 250) == XP_TLS_OK && ok;
	ok = copied == sizeof(outbound) - 1 && ok;
	reader.join();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
	    std::chrono::steady_clock::now() - started).count();
	return ok && read_status == XP_TLS_TIMEOUT && read_copied == 0
	    && elapsed >= 100 && elapsed < 1000;
}

bool termination_wakes_reader(xp_tls_t session, SOCKET socket)
{
	std::array<char, 8> buffer{};
	size_t copied = 99;
	int status = XP_TLS_ERR;
	std::thread reader([&] {
		status = xp_tls_pop_timeout(session, buffer.data(), buffer.size(),
		                            &copied, -1);
	});
	std::this_thread::sleep_for(std::chrono::milliseconds(25));
	REQUIRE(xp_tls_terminate(session) == XP_TLS_OK);
#ifdef _WIN32
	REQUIRE(shutdown(socket, SD_BOTH) == 0);
#else
	REQUIRE(shutdown(socket, SHUT_RDWR) == 0);
#endif
	reader.join();
	return status == XP_TLS_ERR_CLOSED && copied == 0;
}

bool set_tcp_coalescing(SOCKET socket, bool enabled)
{
#if defined(TCP_NOPUSH)
	int value = enabled ? 1 : 0;
	return setsockopt(socket, IPPROTO_TCP, TCP_NOPUSH, &value,
	                  sizeof(value)) == 0;
#elif defined(TCP_CORK)
	int value = enabled ? 1 : 0;
	return setsockopt(socket, IPPROTO_TCP, TCP_CORK, &value,
	                  sizeof(value)) == 0;
#else
	(void)socket;
	(void)enabled;
	return true;
#endif
}

bool data_before_graceful_close_is_preserved(
	Session& peer, SOCKET peer_socket, xp_tls_t reader)
{
	/* Wait to read until the writer has emitted both application data and
	   close_notify. A provider may receive both records from one socket read;
	   it must return all authenticated plaintext before reporting EOF. */
	const char payload[] = "4\r\ntest\r\n0\r\n\r\n";
	size_t copied = 0;
	REQUIRE(set_tcp_coalescing(peer_socket, true));
	REQUIRE(xp_tls_push_timeout(peer.value, payload, sizeof(payload) - 1,
	                            &copied, 1000) == XP_TLS_OK);
	REQUIRE(copied == sizeof(payload) - 1);
	xp_tls_close(peer.value, false);
	peer.value = nullptr;
	REQUIRE(set_tcp_coalescing(peer_socket, false));

	std::array<char, sizeof(payload)> received{};
	for (size_t offset = 0; offset < sizeof(payload) - 1;) {
		copied = 0;
		REQUIRE(xp_tls_pop_timeout(reader, received.data() + offset, 1,
		                           &copied, 1000) == XP_TLS_OK);
		REQUIRE(copied == 1);
		offset += copied;
	}
	REQUIRE(std::memcmp(received.data(), payload, sizeof(payload) - 1) == 0);
	copied = 99;
	REQUIRE(xp_tls_pop_timeout(reader, received.data(), 1, &copied, 1000) ==
	        XP_TLS_ERR_CLOSED);
	return copied == 0;
}

bool reset_is_reported(Session& peer, SOCKET& peer_socket, xp_tls_t reader)
{
	struct linger reset{};
	reset.l_onoff = 1;
	reset.l_linger = 0;
	if (setsockopt(peer_socket, SOL_SOCKET, SO_LINGER,
	#ifdef _WIN32
	               reinterpret_cast<const char *>(&reset),
	#else
	               &reset,
	#endif
	               sizeof(reset)) != 0)
		return false;
	closesocket(peer_socket);
	peer_socket = INVALID_SOCKET;
	std::array<char, 1> buffer{};
	size_t copied = 99;
	int status = xp_tls_pop_timeout(reader, buffer.data(), buffer.size(),
	                                &copied, 1000);
	xp_tls_close(peer.value, false);
	peer.value = nullptr;
	return status == XP_TLS_ERR_CLOSED && copied == 0;
}

bool certificate_session(enum xp_tls_version version, bool permissive)
{
	TempFile certificate_file("tls-certificate");
	TempFile malformed_file("tls-malformed-chain");
	xp_key_t key = nullptr;
	xp_ca_cert_t certificate = nullptr;
	xp_tls_server_credentials_t credentials = nullptr;
	REQUIRE(make_server_identity(&key, &certificate, certificate_file.path()));
	struct xp_tls_server_credentials_config identity{};
	identity.certificate_chain_file = certificate_file.path();
	identity.private_key = key;
	const struct xp_key_spec rsa = {XP_KEY_RSA, 2048, XP_KEY_CURVE_NONE};
	xp_key_t wrong_key = nullptr;
	REQUIRE(xp_key_generate(&wrong_key, &rsa) == XP_CRYPTO_OK);
	struct xp_tls_server_credentials_config wrong_identity = identity;
	wrong_identity.private_key = wrong_key;
	xp_tls_server_credentials_t rejected = nullptr;
	REQUIRE(xp_tls_server_credentials_load(&rejected, &wrong_identity) ==
	        XP_CRYPTO_ERR_CONFLICT);
	REQUIRE(rejected == nullptr);
	xp_key_release(wrong_key);
	REQUIRE(write_certificate(malformed_file.path(), certificate));
	FILE *malformed = std::fopen(malformed_file.path(), "ab");
	REQUIRE(malformed != nullptr);
	const char extra[] =
	    "-----BEGIN PRIVATE KEY-----\nAA==\n-----END PRIVATE KEY-----\n";
	REQUIRE(std::fwrite(extra, 1, sizeof(extra) - 1, malformed) ==
	        sizeof(extra) - 1);
	REQUIRE(std::fclose(malformed) == 0);
	struct xp_tls_server_credentials_config malformed_identity = identity;
	malformed_identity.certificate_chain_file = malformed_file.path();
	REQUIRE(xp_tls_server_credentials_load(&rejected, &malformed_identity) !=
	        XP_CRYPTO_OK);
	REQUIRE(rejected == nullptr);
	REQUIRE(xp_tls_server_credentials_load(&credentials, &identity) ==
	        XP_CRYPTO_OK);
	if (permissive)
		REQUIRE(std::remove(certificate_file.path()) == 0);

	SocketPair sockets;
	REQUIRE(connected_sockets(sockets));
	SocketTimeouts client_timeouts = socket_timeouts(sockets.client);
	SocketTimeouts server_timeouts = socket_timeouts(sockets.server);
	struct xp_tls_server_config server_config{};
	server_config.min_version = version;
	server_config.max_version = version;
	server_config.handshake_timeout_ms = 3000;
	server_config.client_auth = XP_TLS_CLIENT_AUTH_NONE;
	if (permissive)
		server_config.psk_lookup = reject_psk;
	ServerResult server;
	std::thread thread([&] {
		server = start_server(sockets.server, credentials, server_config);
	});

	struct xp_tls_client_config client_config{};
	client_config.server_name = "localhost";
	client_config.server_auth = permissive ? XP_TLS_SERVER_AUTH_NONE
	                                     : XP_TLS_SERVER_AUTH_CERTIFICATE;
	client_config.trusted_cert_file = permissive ? nullptr
	                                             : certificate_file.path();
	client_config.min_version = version;
	client_config.max_version = version;
	Session client;
	client.value = xp_tls_client_open_config(sockets.client, &client_config);
	std::string client_error = client.value == nullptr ? xp_tls_last_err() : "";
	thread.join();
	if (client.value == nullptr || server.session == nullptr) {
		std::fprintf(stderr, "certificate handshake: client=%s server=%s\n",
		             client_error.c_str(), server.error.c_str());
		xp_tls_close(server.session, false);
		xp_tls_server_credentials_release(credentials);
		xp_ca_cert_free(certificate);
		xp_key_release(key);
		return false;
	}
	Session server_session{server.session};
	REQUIRE(socket_timeouts(sockets.client) == client_timeouts);
	REQUIRE(socket_timeouts(sockets.server) == server_timeouts);
	REQUIRE(xp_tls_protocol_version(client.value) == version);
	REQUIRE(xp_tls_protocol_version(server_session.value) == version);
	REQUIRE(xp_tls_authentication_method(client.value) ==
	        XP_TLS_AUTH_CERTIFICATE);
	REQUIRE(xp_tls_authentication_method(server_session.value) ==
	        XP_TLS_AUTH_CERTIFICATE);
	REQUIRE(xp_tls_peer_certificate_count(client.value) >= 1);
	size_t der_length = 0;
	REQUIRE(xp_tls_peer_certificate_der(client.value, 0, nullptr,
	                                    &der_length) == XP_CRYPTO_OK);
	REQUIRE(der_length > 0);

	/* The session owns credential material independent of its caller. */
	xp_tls_server_credentials_release(credentials);
	credentials = nullptr;
	xp_ca_cert_free(certificate);
	xp_key_release(key);
	REQUIRE(transfer_and_inspect(client.value, server_session.value));
	REQUIRE(concurrent_directions(client.value, server_session.value));
	REQUIRE(data_before_graceful_close_is_preserved(server_session,
	                                                sockets.server,
	                                                client.value));
	return true;
}

struct PskState {
	std::string identity = "client-one";
	std::array<unsigned char, 32> key{};
	bool called = false;
};

int find_psk(void *arg, const void *identity, size_t identity_len,
	         void *key, size_t *key_len)
{
	auto *state = static_cast<PskState *>(arg);
	state->called = true;
	if (identity_len != state->identity.size() ||
	    std::memcmp(identity, state->identity.data(), identity_len) != 0 ||
	    key_len == nullptr || *key_len < state->key.size())
		return -1;
	std::memcpy(key, state->key.data(), state->key.size());
	*key_len = state->key.size();
	return 0;
}

bool psk_session(enum xp_tls_version version, bool coexist_with_certificate,
                 enum xp_tls_psk_policy policy)
{
	PskState state;
	for (size_t i = 0; i < state.key.size(); ++i)
		state.key[i] = static_cast<unsigned char>(i + 1);
	SocketPair sockets;
	REQUIRE(connected_sockets(sockets));
	TempFile certificate_file("tls-psk-certificate");
	xp_key_t certificate_key = nullptr;
	xp_ca_cert_t certificate = nullptr;
	xp_tls_server_credentials_t credentials = nullptr;
	if (coexist_with_certificate) {
		REQUIRE(make_server_identity(&certificate_key, &certificate,
		                             certificate_file.path()));
		struct xp_tls_server_credentials_config identity{};
		identity.certificate_chain_file = certificate_file.path();
		identity.private_key = certificate_key;
		REQUIRE(xp_tls_server_credentials_load(&credentials, &identity) ==
		        XP_CRYPTO_OK);
	}
	SocketTimeouts client_timeouts = socket_timeouts(sockets.client);
	SocketTimeouts server_timeouts = socket_timeouts(sockets.server);
	struct xp_tls_server_config server_config{};
	server_config.min_version = version;
	server_config.max_version = version;
	server_config.handshake_timeout_ms = 3000;
	server_config.psk_lookup = find_psk;
	server_config.psk_lookup_arg = &state;
	server_config.psk_policy = policy;
	ServerResult server;
	std::thread thread([&] {
		server = start_server(sockets.server, credentials, server_config);
	});
	struct xp_tls_client_config client_config{};
	client_config.psk_identity = state.identity.c_str();
	client_config.psk = state.key.data();
	client_config.psk_len = state.key.size();
	client_config.psk_version = version;
	client_config.psk_policy = policy;
	Session client;
	client.value = xp_tls_client_open_config(sockets.client, &client_config);
	std::string client_error = client.value == nullptr ? xp_tls_last_err() : "";
	thread.join();
	if (client.value == nullptr || server.session == nullptr) {
		std::fprintf(stderr, "PSK handshake: client=%s server=%s\n",
		             client_error.c_str(), server.error.c_str());
		xp_tls_close(server.session, false);
		return false;
	}
	Session server_session{server.session};
	REQUIRE(socket_timeouts(sockets.client) == client_timeouts);
	REQUIRE(socket_timeouts(sockets.server) == server_timeouts);
	REQUIRE(state.called);
	REQUIRE(xp_tls_authentication_method(client.value) == XP_TLS_AUTH_PSK);
	REQUIRE(xp_tls_authentication_method(server_session.value) == XP_TLS_AUTH_PSK);
	std::array<char, 32> identity{};
	size_t identity_len = identity.size();
	REQUIRE(xp_tls_psk_identity(server_session.value, identity.data(),
	                            &identity_len) == XP_CRYPTO_OK);
	REQUIRE(identity_len == state.identity.size());
	REQUIRE(std::memcmp(identity.data(), state.identity.data(), identity_len) == 0);
	REQUIRE(transfer_and_inspect(client.value, server_session.value));
	if (version == XP_TLS_VERSION_1_2)
		REQUIRE(reset_is_reported(client, sockets.client, server_session.value));
	else
		REQUIRE(termination_wakes_reader(server_session.value, sockets.server));
	xp_tls_server_credentials_release(credentials);
	xp_ca_cert_free(certificate);
	xp_key_release(certificate_key);
	return true;
}

bool unknown_psk_is_rejected()
{
	PskState state;
	SocketPair sockets;
	REQUIRE(connected_sockets(sockets));
	struct xp_tls_server_config server_config{};
	server_config.min_version = XP_TLS_VERSION_1_3;
	server_config.max_version = XP_TLS_VERSION_1_3;
	server_config.handshake_timeout_ms = 1000;
	server_config.psk_lookup = find_psk;
	server_config.psk_lookup_arg = &state;
	ServerResult server;
	std::thread thread([&] {
		server = start_server(sockets.server, nullptr, server_config);
	});
	const char unknown[] = "unknown-client";
	struct xp_tls_client_config client_config{};
	client_config.psk_identity = unknown;
	client_config.psk = state.key.data();
	client_config.psk_len = state.key.size();
	client_config.psk_version = XP_TLS_VERSION_1_3;
	Session client;
	client.value = xp_tls_client_open_config(sockets.client, &client_config);
	thread.join();
	if (server.session != nullptr)
		xp_tls_close(server.session, false);
	return client.value == nullptr && server.session == nullptr && state.called;
}

bool version_range_is_rejected()
{
	TempFile certificate_file("tls-version-rejection");
	xp_key_t key = nullptr;
	xp_ca_cert_t certificate = nullptr;
	xp_tls_server_credentials_t credentials = nullptr;
	REQUIRE(make_server_identity(&key, &certificate, certificate_file.path()));
	struct xp_tls_server_credentials_config identity{};
	identity.certificate_chain_file = certificate_file.path();
	identity.private_key = key;
	REQUIRE(xp_tls_server_credentials_load(&credentials, &identity) ==
	        XP_CRYPTO_OK);
	SocketPair sockets;
	REQUIRE(connected_sockets(sockets));
	struct xp_tls_server_config server_config{};
	server_config.min_version = XP_TLS_VERSION_1_3;
	server_config.max_version = XP_TLS_VERSION_1_3;
	server_config.handshake_timeout_ms = 1000;
	ServerResult server;
	std::thread thread([&] {
		server = start_server(sockets.server, credentials, server_config);
	});
	struct xp_tls_client_config client_config{};
	client_config.server_name = "localhost";
	client_config.server_auth = XP_TLS_SERVER_AUTH_CERTIFICATE;
	client_config.trusted_cert_file = certificate_file.path();
	client_config.min_version = XP_TLS_VERSION_1_2;
	client_config.max_version = XP_TLS_VERSION_1_2;
	Session client;
	client.value = xp_tls_client_open_config(sockets.client, &client_config);
	thread.join();
	if (server.session != nullptr)
		xp_tls_close(server.session, false);
	bool rejected = server.session == nullptr;
	if (client.value != nullptr) {
		std::array<char, 1> buffer{};
		size_t copied = 0;
		rejected = xp_tls_pop_timeout(client.value, buffer.data(), buffer.size(),
		                              &copied, 250) != XP_TLS_OK && rejected;
	}
	xp_tls_server_credentials_release(credentials);
	xp_ca_cert_free(certificate);
	xp_key_release(key);
	return rejected;
}

bool close_during_handshake_is_bounded()
{
	PskState state;
	SocketPair sockets;
	REQUIRE(connected_sockets(sockets));
	struct xp_tls_server_config server_config{};
	server_config.min_version = XP_TLS_VERSION_1_3;
	server_config.max_version = XP_TLS_VERSION_1_3;
	server_config.handshake_timeout_ms = 3000;
	server_config.psk_lookup = find_psk;
	server_config.psk_lookup_arg = &state;
	ServerResult server;
	auto started = std::chrono::steady_clock::now();
	std::thread thread([&] {
		server = start_server(sockets.server, nullptr, server_config);
	});
	std::this_thread::sleep_for(std::chrono::milliseconds(25));
	#ifdef _WIN32
	int shutdown_status = shutdown(sockets.client, SD_BOTH);
	#else
	int shutdown_status = shutdown(sockets.client, SHUT_RDWR);
	#endif
	thread.join();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
	    std::chrono::steady_clock::now() - started).count();
	if (server.session != nullptr)
		xp_tls_close(server.session, false);
	return shutdown_status == 0 && server.session == nullptr && elapsed < 1000;
}

bool client_certificate_session(enum xp_tls_client_auth auth_mode)
{
	TempFile server_file("tls-server");
	TempFile root_file("tls-client-root");
	TempFile client_file("tls-client-chain");
	TempFile client_key_file("tls-client-key");
	const char *password = "client-key-password";
	xp_key_t server_key = nullptr;
	xp_ca_cert_t server_cert = nullptr;
	REQUIRE(make_server_identity(&server_key, &server_cert, server_file.path()));

	const struct xp_key_spec rsa = {XP_KEY_RSA, 2048, XP_KEY_CURVE_NONE};
	const char *root_names[] = {"client-root.example.test"};
	const char *client_names[] = {"client.example.test"};
	time_t now = time(nullptr);
	struct xp_ca_issue_request root_request{};
	root_request.subject = {root_names[0], root_names, 1};
	root_request.policy.is_ca = true;
	root_request.policy.basic_constraints_critical = true;
	root_request.policy.key_usage_critical = true;
	root_request.policy.key_usage = XP_CA_KEY_USE_CERT_SIGN;
	root_request.policy.path_length = 1;
	root_request.not_before = now - 60;
	root_request.not_after = now + 3600;
	xp_key_t root_key = nullptr;
	xp_ca_cert_t root_cert = nullptr;
	REQUIRE(xp_key_generate(&root_key, &rsa) == XP_CRYPTO_OK);
	REQUIRE(xp_ca_cert_create_self_signed(&root_cert, root_key,
	                                      &root_request) == XP_CA_OK);
	REQUIRE(write_certificate(root_file.path(), root_cert));

	xp_key_t client_key = nullptr;
	xp_ca_csr_t client_csr = nullptr;
	xp_ca_cert_t client_cert = nullptr;
	REQUIRE(xp_key_generate(&client_key, &rsa) == XP_CRYPTO_OK);
	struct xp_ca_identity client_identity = {client_names[0], client_names, 1};
	REQUIRE(xp_ca_csr_create_with_identity(&client_csr, client_key,
	                                      &client_identity) == XP_CA_OK);
	struct xp_ca_issue_request client_request{};
	client_request.subject = client_identity;
	client_request.policy.basic_constraints_critical = true;
	client_request.policy.key_usage_critical = true;
	client_request.policy.key_usage = XP_CA_KEY_USE_SIGN;
	client_request.policy.extended_key_usage = XP_CA_EKU_CLIENT_AUTH;
	client_request.policy.path_length = -1;
	client_request.not_before = now - 60;
	client_request.not_after = now + 1800;
	REQUIRE(xp_ca_cert_issue(&client_cert, root_key, root_cert, client_csr,
	                         &client_request) == XP_CA_OK);
	const xp_ca_cert_t client_chain[] = {client_cert, root_cert};
	REQUIRE(write_certificates(client_file.path(), client_chain, 2));
	size_t client_chain_pem_len = 0;
	REQUIRE(xp_ca_cert_chain_export_pem(client_chain, 2, nullptr,
	                                    &client_chain_pem_len) == XP_CA_OK);
	std::vector<unsigned char> client_chain_pem(client_chain_pem_len);
	REQUIRE(xp_ca_cert_chain_export_pem(client_chain, 2,
	                                    client_chain_pem.data(),
	                                    &client_chain_pem_len) == XP_CA_OK);
	REQUIRE(write_private_key(client_key_file.path(), client_key, password));

	xp_tls_server_credentials_t credentials = nullptr;
	struct xp_tls_server_credentials_config identity{};
	identity.certificate_chain_file = server_file.path();
	identity.private_key = server_key;
	REQUIRE(xp_tls_server_credentials_load(&credentials, &identity) ==
	        XP_CRYPTO_OK);
	SocketPair sockets;
	REQUIRE(connected_sockets(sockets));
	struct xp_tls_server_config server_config{};
	server_config.min_version = XP_TLS_VERSION_1_3;
	server_config.max_version = XP_TLS_VERSION_1_3;
	server_config.handshake_timeout_ms = 3000;
	server_config.client_auth = auth_mode;
	server_config.client_ca_file = auth_mode == XP_TLS_CLIENT_AUTH_REQUIRE_VALID
	    ? root_file.path() : nullptr;
	ServerResult server;
	std::thread thread([&] {
		server = start_server(sockets.server, credentials, server_config);
	});
	struct xp_tls_client_config client_config{};
	client_config.server_name = "localhost";
	client_config.server_auth = XP_TLS_SERVER_AUTH_CERTIFICATE;
	client_config.trusted_cert_file = server_file.path();
	client_config.client_cert_file = client_file.path();
	client_config.client_key_file = client_key_file.path();
	client_config.private_key_password = password_callback;
	client_config.private_key_password_arg = const_cast<char *>(password);
	client_config.min_version = XP_TLS_VERSION_1_3;
	client_config.max_version = XP_TLS_VERSION_1_3;
	Session client;
	client.value = xp_tls_client_open_config(sockets.client, &client_config);
	std::string client_error = client.value == nullptr ? xp_tls_last_err() : "";
	thread.join();
	if (client.value == nullptr || server.session == nullptr) {
		std::fprintf(stderr, "client certificate handshake: client=%s server=%s\n",
		             client_error.c_str(), server.error.c_str());
		xp_tls_close(server.session, false);
		return false;
	}
	Session server_session{server.session};
	REQUIRE(xp_tls_peer_certificate_count(server_session.value) >= 1);
	size_t peer_der_len = 0;
	REQUIRE(xp_tls_peer_certificate_der(server_session.value, 0, nullptr,
	                                    &peer_der_len) == XP_CRYPTO_OK);
	REQUIRE(peer_der_len > 0);
	REQUIRE(transfer_and_inspect(client.value, server_session.value));

	/* The provider-neutral identity path must use the opaque key directly,
	   retain it for the session, and reject a mismatched key before I/O. */
	struct xp_tls_client_identity stored_identity {
		client_chain_pem.data(), client_chain_pem_len, client_key
	};
	struct xp_tls_client_config stored_config = client_config;
	stored_config.client_cert_file = nullptr;
	stored_config.client_key_file = nullptr;
	stored_config.client_identity = &stored_identity;
	stored_config.private_key_password = nullptr;
	stored_config.private_key_password_arg = nullptr;
	struct xp_tls_client_identity mismatched_identity = stored_identity;
	mismatched_identity.private_key = server_key;
	struct xp_tls_client_config mismatched_config = stored_config;
	mismatched_config.client_identity = &mismatched_identity;
	REQUIRE(xp_tls_client_open_config(INVALID_SOCKET, &mismatched_config) ==
	        nullptr);
	SocketPair stored_sockets;
	REQUIRE(connected_sockets(stored_sockets));
	ServerResult stored_server;
	std::thread stored_thread([&] {
		stored_server = start_server(stored_sockets.server, credentials,
		                             server_config);
	});
	Session stored_client;
	stored_client.value = xp_tls_client_open_config(stored_sockets.client,
	                                                &stored_config);
	std::string stored_error = stored_client.value == nullptr
	    ? xp_tls_last_err() : "";
	stored_thread.join();
	if (stored_client.value == nullptr || stored_server.session == nullptr) {
		std::fprintf(stderr,
		    "stored client identity handshake: client=%s server=%s\n",
		    stored_error.c_str(), stored_server.error.c_str());
		xp_tls_close(stored_server.session, false);
		return false;
	}
	Session stored_server_session{stored_server.session};
	xp_key_release(client_key);
	client_key = nullptr;
	REQUIRE(transfer_and_inspect(stored_client.value,
	                            stored_server_session.value));

	if (auth_mode == XP_TLS_CLIENT_AUTH_REQUIRE_VALID) {
		SocketPair rejected_sockets;
		REQUIRE(connected_sockets(rejected_sockets));
		ServerResult rejected_server;
		std::thread rejected_thread([&] {
			rejected_server = start_server(rejected_sockets.server,
			                               credentials, server_config);
		});
		struct xp_tls_client_config anonymous_config = client_config;
		anonymous_config.client_cert_file = nullptr;
		anonymous_config.client_key_file = nullptr;
		anonymous_config.private_key_password = nullptr;
		anonymous_config.private_key_password_arg = nullptr;
		Session anonymous_client;
		anonymous_client.value = xp_tls_client_open_config(
		    rejected_sockets.client, &anonymous_config);
		rejected_thread.join();
		if (rejected_server.session != nullptr)
			xp_tls_close(rejected_server.session, false);
		REQUIRE(rejected_server.session == nullptr);
		if (anonymous_client.value != nullptr) {
			std::array<char, 1> rejected_data{};
			size_t rejected_copied = 0;
			REQUIRE(xp_tls_pop_timeout(anonymous_client.value,
			    rejected_data.data(), rejected_data.size(), &rejected_copied,
			    250) != XP_TLS_OK);
		}
	}

	xp_tls_server_credentials_release(credentials);
	xp_ca_cert_free(client_cert);
	xp_ca_csr_free(client_csr);
	xp_key_release(client_key);
	xp_ca_cert_free(root_cert);
	xp_key_release(root_key);
	xp_ca_cert_free(server_cert);
	xp_key_release(server_key);
	return true;
}

} // namespace

int main()
{
#ifdef _WIN32
	WSADATA data;
	if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
		return 1;
#endif
	bool ok = certificate_session(XP_TLS_VERSION_1_2, false) &&
	          certificate_session(XP_TLS_VERSION_1_3, true) &&
	          psk_session(XP_TLS_VERSION_1_2, false,
	                      XP_TLS_PSK_POLICY_MODERN) &&
	          psk_session(XP_TLS_VERSION_1_2, false,
	                      XP_TLS_PSK_POLICY_TLS12_COMPATIBILITY) &&
	          psk_session(XP_TLS_VERSION_1_3, true,
	                      XP_TLS_PSK_POLICY_MODERN) &&
	          unknown_psk_is_rejected() &&
	          version_range_is_rejected() &&
	          close_during_handshake_is_bounded() &&
	          client_certificate_session(XP_TLS_CLIENT_AUTH_REQUEST_UNVERIFIED) &&
	          client_certificate_session(XP_TLS_CLIENT_AUTH_REQUIRE_VALID);
#ifdef _WIN32
	WSACleanup();
#endif
	return ok ? 0 : 1;
}
