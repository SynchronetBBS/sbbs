#include "mqtt_client.h"

#include "sockwrap.h"
#include "threadwrap.h"

#include "scfgdefs.h"
#include "userdat.h"
#include "sbbsdefs.h"

namespace mqtt5 {

Client::Client()
	: m_sock(INVALID_SOCKET)
	, m_tls_sess(CRYPT_UNUSED)
	, m_connected(false)
	, m_last_error(0)
	, m_next_pid(1)
	, m_will_qos(0)
	, m_will_retain(false)
	, m_has_will(false)
	, m_keepalive(0)
	, m_last_send(0)
{
}

void Client::set_will(const char *topic, const void *payload, size_t len,
                      int qos, bool retain)
{
	if (topic && *topic) {
		m_will_topic = topic;
		if (payload && len > 0)
			m_will_payload.assign(static_cast<const uint8_t *>(payload),
			                     static_cast<const uint8_t *>(payload) + len);
		else
			m_will_payload.clear();
		m_will_qos = qos;
		m_will_retain = retain;
		m_has_will = true;
	} else {
		m_has_will = false;
	}
}

Client::~Client()
{
	disconnect();
}

const char *Client::error_str() const
{
	switch (m_last_error) {
	case 0: return "Success";
	case -1: return "Socket error";
	case -2: return "TLS error";
	case -3: return "Protocol error";
	case -4: return "Timeout";
	case -5: return "Auth rejected";
	default: return "Unknown error";
	}
}

bool Client::send_packet(const std::vector<uint8_t> &pkt)
{
	bool ok;
	if (m_tls_sess != CRYPT_UNUSED) {
		int sent;
		int ret = cryptPushData(m_tls_sess, pkt.data(), pkt.size(), &sent);
		if (ret != CRYPT_OK) return false;
		ret = cryptFlushData(m_tls_sess);
		ok = (ret == CRYPT_OK);
	} else {
		int sent = sendsocket(m_sock, (const char *)pkt.data(), pkt.size());
		ok = (sent == (int)pkt.size());
	}
	if (ok)
		m_last_send = time(NULL);
	return ok;
}

int Client::recv_data(int timeout_ms)
{
	if (m_sock == INVALID_SOCKET)
		return -1;

	if (!socket_readable(m_sock, timeout_ms))
		return 0;

	uint8_t buf[65536];
	int n;
	if (m_tls_sess != CRYPT_UNUSED) {
		int ret = cryptPopData(m_tls_sess, buf, sizeof(buf), &n);
		if (ret != CRYPT_OK || n <= 0)
			return -1;
		m_recv_buf.insert(m_recv_buf.end(), buf, buf + n);
		while (n > 0) {
			cryptSetAttribute(m_tls_sess, CRYPT_OPTION_NET_READTIMEOUT, 0);
			ret = cryptPopData(m_tls_sess, buf, sizeof(buf), &n);
			if (ret == CRYPT_OK && n > 0)
				m_recv_buf.insert(m_recv_buf.end(), buf, buf + n);
			else
				break;
		}
	} else {
		n = recv(m_sock, (char *)buf, sizeof(buf), 0);
		if (n <= 0)
			return -1;
		m_recv_buf.insert(m_recv_buf.end(), buf, buf + n);
	}
	return 1;
}

int Client::pump(int timeout_ms)
{
	if (m_connected && m_keepalive > 0) {
		time_t now = time(NULL);
		if (now - m_last_send >= m_keepalive / 2) {
			uint8_t pingreq[] = { PINGREQ << 4, 0 };
			send_packet(std::vector<uint8_t>(pingreq, pingreq + 2));
		}
	}

	int rc = recv_data(timeout_ms);
	if (rc < 0) {
		m_last_error = -1;
		return -1;
	}

	while (m_recv_buf.size() >= 2) {
		uint8_t type, flags;
		uint32_t remaining;
		int hdr_len = read_fixed_header(m_recv_buf.data(), m_recv_buf.size(),
		                                type, flags, remaining);
		if (hdr_len == 0) break;
		if (hdr_len < 0) {
			m_last_error = -3;
			return -1;
		}
		size_t total = hdr_len + remaining;
		if (m_recv_buf.size() < total) break;

		handle_packet(type, flags, m_recv_buf.data() + hdr_len, remaining);
		m_recv_buf.erase(m_recv_buf.begin(), m_recv_buf.begin() + total);
	}
	return 0;
}

void Client::handle_packet(uint8_t type, uint8_t flags, const uint8_t *data, size_t len)
{
	switch (type) {
	case PUBLISH: {
		Reader r(data, len);
		std::shared_ptr<Message> msg;
		uint16_t pid;
		if (!parse_publish(r, flags, msg, pid))
			break;
		ReceivedMessage rm;
		rm.topic = msg->topic;
		rm.payload = msg->payload;
		rm.mid = pid;
		rm.qos = msg->qos();
		rm.retain = msg->retain();
		m_queue.push_back(rm);
		if (msg->qos() == 1)
			send_packet(build_ack(PUBACK, pid, 0));
		else if (msg->qos() == 2) {
			send_packet(build_ack(PUBREC, pid, 0));
		}
		break;
	}
	case PUBACK: {
		Reader r(data, len);
		uint16_t pid;
		uint8_t reason;
		Properties props;
		if (parse_ack(r, pid, reason, props))
			m_acked_pids.insert(pid);
		break;
	}
	case PUBCOMP: {
		Reader r(data, len);
		uint16_t pid;
		uint8_t reason;
		Properties props;
		if (parse_ack(r, pid, reason, props))
			m_acked_pids.insert(pid);
		break;
	}
	case PUBREC: {
		Reader r(data, len);
		uint16_t pid;
		uint8_t reason;
		Properties props;
		if (parse_ack(r, pid, reason, props))
			send_packet(build_ack(PUBREL, pid, 0));
		break;
	}
	case PUBREL: {
		Reader r(data, len);
		uint16_t pid;
		uint8_t reason;
		Properties props;
		if (parse_ack(r, pid, reason, props))
			send_packet(build_ack(PUBCOMP, pid, 0));
		break;
	}
	case SUBACK: {
		Reader r(data, len);
		uint16_t pid;
		if (r.read_u16(pid))
			m_acked_pids.insert(pid);
		break;
	}
	case PINGRESP:
		break;
	default:
		break;
	}
}

int Client::connect(const char *host, uint16_t port, const char *client_id,
                    const char *username, const char *password,
                    int keepalive, int protocol_version,
                    int tls_mode, const char *psk, const char *psk_identity,
                    const char *cafile, const char *certfile,
                    const char *keyfile, const char *keypass,
                    scfg_t *scfg,
                    int (*lprintf)(int level, const char *fmt, ...))
{
	if (protocol_version != 5) {
		m_last_error = -3;
		return -1;
	}

	disconnect();

	char portstr[16];
	struct addrinfo hints = {};
	struct addrinfo *res = NULL, *cur;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;
	snprintf(portstr, sizeof(portstr), "%hu", port);
	if (getaddrinfo(host, portstr, &hints, &res) != 0 || res == NULL) {
		m_last_error = -1;
		return -1;
	}

	for (cur = res; cur; cur = cur->ai_next) {
		m_sock = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
		if (m_sock == INVALID_SOCKET)
			continue;
		if (::connect(m_sock, cur->ai_addr, cur->ai_addrlen) == 0)
			break;
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
	}
	freeaddrinfo(res);

	if (m_sock == INVALID_SOCKET) {
		m_last_error = -1;
		return -1;
	}

	int nodelay = 1;
	setsockopt(m_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay, sizeof(nodelay));

	if (tls_mode != MQTT_TLS_DISABLED) {
		if (!do_cryptInit(lprintf)) {
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			m_last_error = -2;
			return -1;
		}

		int ret = cryptCreateSession(&m_tls_sess, CRYPT_UNUSED, CRYPT_SESSION_TLS);
		if (ret != CRYPT_OK) {
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			m_last_error = -2;
			return -1;
		}

		cryptSetAttribute(m_tls_sess, CRYPT_SESSINFO_TLS_OPTIONS, CRYPT_TLSOPTION_MINVER_TLS12);

		if (tls_mode == MQTT_TLS_PSK || tls_mode == MQTT_TLS_SBBS) {
			const char *id = psk_identity;
			char key_buf[LEN_PASS + 1] = {};
			size_t key_len = 0;
			char alias_buf[LEN_ALIAS + 1] = {};

			if (tls_mode == MQTT_TLS_SBBS && scfg) {
				user_t user = {1};
				if (getuserdat(scfg, &user) == USER_SUCCESS
				    && user.number == 1
				    && user_is_sysop(&user)) {
					SAFECOPY(key_buf, user.pass);
					strlwr(key_buf);
					key_len = strlen(key_buf);
					strlwr(user.alias);
					SAFECOPY(alias_buf, user.alias);
					id = alias_buf;
				}
			} else if (tls_mode == MQTT_TLS_PSK && psk && *psk) {
				for (size_t hi = 0; psk[hi] && psk[hi + 1] && key_len < sizeof(key_buf) - 1; hi += 2) {
					unsigned byte;
					if (sscanf(psk + hi, "%2x", &byte) == 1)
						key_buf[key_len++] = (char)byte;
				}
			}
			if (id && *id) {
				if (cryptSetAttributeString(m_tls_sess, CRYPT_SESSINFO_USERNAME, id, strlen(id)) == CRYPT_OK)
					cryptSetAttributeString(m_tls_sess, CRYPT_SESSINFO_PASSWORD, key_buf, key_len);
			}
		} else if (tls_mode == MQTT_TLS_CERT) {
			if (scfg)
				add_private_key(scfg, lprintf, m_tls_sess);
		}

		cryptSetAttribute(m_tls_sess, CRYPT_SESSINFO_NETWORKSOCKET, m_sock);
		if (tls_mode == MQTT_TLS_PSK || tls_mode == MQTT_TLS_SBBS) {
			cryptSetAttribute(m_tls_sess, CRYPT_SESSINFO_TLS_OPTIONS, CRYPT_TLSOPTION_DISABLE_CERTVERIFY);
			cryptSetAttribute(m_tls_sess, CRYPT_SESSINFO_TLS_OPTIONS, CRYPT_TLSOPTION_DISABLE_NAMEVERIFY);
		}
		cryptSetAttributeString(m_tls_sess, CRYPT_SESSINFO_SERVER_NAME, host, strlen(host));
		ret = cryptSetAttribute(m_tls_sess, CRYPT_SESSINFO_ACTIVE, 1);
		if (ret != CRYPT_OK) {
			if (lprintf) {
				char *estr = NULL;
				get_crypt_error_string(ret, m_tls_sess, &estr, "TLS handshake", NULL);
				lprintf(LOG_ERR, "MQTT client: %s", estr ? estr : "TLS handshake failed");
				free(estr);
			}
			cryptDestroySession(m_tls_sess);
			m_tls_sess = CRYPT_UNUSED;
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			m_last_error = -2;
			return -1;
		}
		cryptSetAttribute(m_tls_sess, CRYPT_OPTION_NET_READTIMEOUT, 0);
	}

	// Build and send CONNECT
	Writer w;
	w.write_utf8("MQTT");
	w.write_byte(5); // protocol version
	uint8_t connect_flags = 0x02; // clean start
	if (username && *username) connect_flags |= 0x80;
	if (password && *password) connect_flags |= 0x40;
	if (m_has_will) {
		connect_flags |= 0x04; // will flag
		connect_flags |= (m_will_qos & 3) << 3;
		if (m_will_retain)
			connect_flags |= 0x20;
	}
	w.write_byte(connect_flags);
	w.write_u16(keepalive);

	Properties connect_props;
	w.write_properties(connect_props);
	w.write_utf8(client_id ? client_id : "");
	if (m_has_will) {
		Properties will_props;
		w.write_properties(will_props);
		w.write_utf8(m_will_topic);
		w.write_u16(m_will_payload.size());
		w.write_bytes(m_will_payload.data(), m_will_payload.size());
	}
	if (username && *username)
		w.write_utf8(username);
	if (password && *password)
		w.write_utf8(password);

	auto pkt = frame_packet((CONNECT << 4), w.take());
	if (!send_packet(pkt)) {
		if (lprintf)
			lprintf(LOG_ERR, "MQTT client: failed to send CONNECT packet");
		disconnect();
		m_last_error = -1;
		return -1;
	}

	// Wait for CONNACK
	for (int elapsed = 0; elapsed < 10000; elapsed += 100) {
		int rc = recv_data(100);
		if (rc < 0) {
			if (lprintf)
				lprintf(LOG_ERR, "MQTT client: connection lost waiting for CONNACK");
			disconnect();
			m_last_error = -1;
			return -1;
		}
		if (m_recv_buf.size() >= 2) {
			uint8_t type, flags;
			uint32_t remaining;
			int hdr_len = read_fixed_header(m_recv_buf.data(), m_recv_buf.size(),
			                                type, flags, remaining);
			if (hdr_len > 0 && m_recv_buf.size() >= (size_t)(hdr_len + remaining)) {
				if ((type) == CONNACK) {
					Reader r(m_recv_buf.data() + hdr_len, remaining);
					uint8_t ack_flags, reason;
					r.read_byte(ack_flags);
					r.read_byte(reason);
					m_recv_buf.erase(m_recv_buf.begin(),
					                 m_recv_buf.begin() + hdr_len + remaining);
					if (reason != 0) {
						disconnect();
						m_last_error = -5;
						return -1;
					}
					m_connected = true;
					m_keepalive = keepalive;
					m_last_error = 0;
					return 0;
				}
				m_recv_buf.erase(m_recv_buf.begin(),
				                 m_recv_buf.begin() + hdr_len + remaining);
			}
		}
	}
	disconnect();
	m_last_error = -4;
	return -1;
}

void Client::disconnect()
{
	if (m_connected) {
		auto pkt = build_disconnect(0);
		send_packet(pkt);
		m_connected = false;
	}
	if (m_tls_sess != CRYPT_UNUSED) {
		cryptDestroySession(m_tls_sess);
		m_tls_sess = CRYPT_UNUSED;
	}
	if (m_sock != INVALID_SOCKET) {
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
	}
	m_recv_buf.clear();
	m_queue.clear();
}

int Client::publish(const char *topic, const void *payload, size_t len,
                    int qos, bool retain, const Properties *props)
{
	if (!m_connected) {
		m_last_error = -1;
		return -1;
	}

	auto msg = std::make_shared<Message>();
	msg->type = PUBLISH;
	msg->flags = (qos << 1) | (retain ? 1 : 0);
	msg->topic = topic;
	if (payload && len > 0)
		msg->payload.assign((const uint8_t *)payload, (const uint8_t *)payload + len);
	if (props)
		msg->props = *props;

	uint16_t pid = 0;
	if (qos > 0)
		pid = m_next_pid++;
	if (m_next_pid == 0) m_next_pid = 1;

	auto pkt = build_publish(*msg, pid, false, qos);
	if (!send_packet(pkt)) {
		m_last_error = -1;
		return -1;
	}

	if (qos > 0) {
		for (int elapsed = 0; elapsed < 5000; elapsed += 50) {
			if (pump(50) < 0)
				return -1;
			if (m_acked_pids.count(pid)) {
				m_acked_pids.erase(pid);
				break;
			}
		}
	}

	m_last_error = 0;
	return 0;
}

int Client::subscribe(const char *topic, int qos)
{
	if (!m_connected) {
		m_last_error = -1;
		return -1;
	}

	uint16_t pid = m_next_pid++;
	if (m_next_pid == 0) m_next_pid = 1;

	Writer w;
	w.write_u16(pid);
	Properties props;
	w.write_properties(props);
	w.write_utf8(topic);
	w.write_byte(qos);

	auto pkt = frame_packet((SUBSCRIBE << 4) | 0x02, w.take());
	if (!send_packet(pkt)) {
		m_last_error = -1;
		return -1;
	}

	for (int elapsed = 0; elapsed < 5000; elapsed += 50) {
		if (pump(50) < 0)
			return -1;
		if (m_acked_pids.count(pid)) {
			m_acked_pids.erase(pid);
			break;
		}
	}

	m_last_error = 0;
	return 0;
}

ReceivedMessage *Client::read(int timeout_ms)
{
	if (!m_queue.empty()) {
		m_last_read = m_queue.front();
		m_queue.pop_front();
		return &m_last_read;
	}

	int remaining = timeout_ms;
	while (remaining > 0) {
		int chunk = remaining > 100 ? 100 : remaining;
		if (pump(chunk) < 0)
			return NULL;
		if (!m_queue.empty()) {
			m_last_read = m_queue.front();
			m_queue.pop_front();
			return &m_last_read;
		}
		remaining -= chunk;
	}
	return NULL;
}

bool Client::data_waiting()
{
	pump(0);
	return !m_queue.empty();
}

int Client::read_level()
{
	pump(0);
	return (int)m_queue.size();
}

} // namespace mqtt5
