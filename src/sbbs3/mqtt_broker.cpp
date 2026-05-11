#include "mqtt_broker.h"

#include <cstdarg>
#include <cstring>
#include <algorithm>
#ifdef PREFER_POLL
#include <poll.h>
#endif

#include "sockwrap.h"
#include "threadwrap.h"

extern "C" {
#include "gen_defs.h"
#include "ver.h"
#include "sbbsdefs.h"
}

static int (*s_broker_log_func)(void *, int, const char *) = NULL;
static void *s_broker_log_data = NULL;

static int broker_lprintf(int level, const char *fmt, ...)
{
	if (!s_broker_log_func)
		return 0;
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	return s_broker_log_func(s_broker_log_data, level, buf);
}

static const char *broker_ver()
{
	static char ver[256];
	if (ver[0] == 0) {
		char compiler[32];
		DESCRIBE_COMPILER(compiler);
		safe_snprintf(ver, sizeof(ver), "Synchronet MQTT Broker %s%c%s  Compiled %s/%s %s with %s"
		              , VERSION, REVISION
#ifdef _DEBUG
		              , " Debug"
#else
		              , ""
#endif
		              , git_branch, git_hash
		              , git_date, compiler
		              );
	}
	return ver;
}

namespace mqtt5 {

Broker *Broker::s_instance = nullptr;
std::mutex Broker::s_instance_mutex;

Broker *Broker::instance()
{
	return s_instance;
}

Broker::Broker()
{
}

Broker::~Broker()
{
	stop();
}

void Broker::log(int level, const char *fmt, ...)
{
	if (!m_lputs) return;
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	m_lputs(m_lputs_data, level, buf);
}

// ── PSK auth table ──────────────────────────────────────────────

void Broker::build_psk_table()
{
	m_psk_table.clear();
	if (!m_cfg) return;

	int last = lastuser(m_cfg);
	for (int i = 1; i <= last; ++i) {
		user_t user;
		user.number = i;
		if (getuserdat(m_cfg, &user) != 0)
			continue;
		if (!user_is_sysop(&user))
			continue;
		if (user.misc & (DELETED | INACTIVE))
			continue;
		std::string alias(user.alias);
		std::string pass(user.pass);
		std::transform(alias.begin(), alias.end(), alias.begin(), ::tolower);
		std::transform(pass.begin(), pass.end(), pass.begin(), ::tolower);
		m_psk_table[alias] = pass;
	}
	log(LOG_DEBUG, "MQTT broker: loaded %zu PSK identities", m_psk_table.size());
}

bool Broker::authenticate_psk(const std::string &identity, const std::string &password)
{
	auto it = m_psk_table.find(identity);
	if (it == m_psk_table.end())
		return false;
	return stricmp(password.c_str(), m_cfg->sys_pass) == 0;
}

bool Broker::authenticate_user(const std::string &username, const std::string &password)
{
	int usernum = matchuser(m_cfg, username.c_str(), false);
	if (usernum < 1)
		return false;
	user_t user;
	user.number = usernum;
	if (getuserdat(m_cfg, &user) != 0)
		return false;
	if (!user_is_sysop(&user))
		return false;
	if (user.misc & (DELETED | INACTIVE))
		return false;

	std::string user_pass(user.pass);
	std::transform(user_pass.begin(), user_pass.end(), user_pass.begin(), ::tolower);
	std::string expected = user_pass + ":";

	if (password.size() <= expected.size())
		return false;
	if (strnicmp(password.c_str(), expected.c_str(), expected.size()) != 0)
		return false;
	return stricmp(password.c_str() + expected.size(), m_cfg->sys_pass) == 0;
}

// ── Lifecycle ───────────────────────────────────────────────────

bool Broker::start(scfg_t *cfg, uint16_t port, int (*lputs)(void *, int, const char *), void *lputs_data)
{
	if (m_running)
		return true;

	m_cfg = cfg;
	m_lputs = lputs;
	m_lputs_data = lputs_data;
	s_broker_log_func = lputs;
	s_broker_log_data = lputs_data;

	build_psk_table();

	m_listen_sock = socket(AF_INET6, SOCK_STREAM, 0);
	if (m_listen_sock == INVALID_SOCKET) {
		m_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
		if (m_listen_sock == INVALID_SOCKET) {
			log(LOG_ERR, "MQTT broker: socket() failed: %d", ERROR_VALUE);
			return false;
		}
		struct sockaddr_in addr = {};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = INADDR_ANY;
		int opt = 1;
		setsockopt(m_listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
		if (bind(m_listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			log(LOG_ERR, "MQTT broker: bind() port %u failed: %d", port, ERROR_VALUE);
			closesocket(m_listen_sock);
			m_listen_sock = INVALID_SOCKET;
			return false;
		}
	} else {
		int off = 0;
		setsockopt(m_listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&off, sizeof(off));
		struct sockaddr_in6 addr = {};
		addr.sin6_family = AF_INET6;
		addr.sin6_port = htons(port);
		addr.sin6_addr = in6addr_any;
		int opt = 1;
		setsockopt(m_listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
		if (bind(m_listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			log(LOG_ERR, "MQTT broker: bind() port %u failed: %d", port, ERROR_VALUE);
			closesocket(m_listen_sock);
			m_listen_sock = INVALID_SOCKET;
			return false;
		}
	}

	if (listen(m_listen_sock, 16) < 0) {
		log(LOG_ERR, "MQTT broker: listen() failed: %d", ERROR_VALUE);
		closesocket(m_listen_sock);
		m_listen_sock = INVALID_SOCKET;
		return false;
	}

	m_running = true;
	m_thread = std::thread(&Broker::broker_thread, this);
	m_accept_thread = std::thread(&Broker::accept_thread, this);

	{
		std::lock_guard<std::mutex> lock(s_instance_mutex);
		s_instance = this;
	}

	{
		const char *ver = broker_ver();
		auto msg = std::make_shared<Message>();
		msg->type = PUBLISH;
		msg->flags = 1;
		msg->topic = "$SYS/broker/version";
		msg->payload.assign(ver, ver + strlen(ver));
		msg->created_at = time(nullptr);
		m_topics.set_retained("$SYS/broker/version", msg);
	}

	log(LOG_INFO, "MQTT broker: listening on port %u", port);
	return true;
}

void Broker::stop()
{
	if (!m_running)
		return;

	m_running = false;

	if (m_listen_sock != INVALID_SOCKET) {
		shutdown(m_listen_sock, SHUT_RDWR);
		closesocket(m_listen_sock);
		m_listen_sock = INVALID_SOCKET;
	}

	if (m_accept_thread.joinable())
		m_accept_thread.join();
	if (m_thread.joinable())
		m_thread.join();

	for (auto it_s = m_sessions.begin(); it_s != m_sessions.end(); ++it_s) {
		if (it_s->second.tls_sess != CRYPT_UNUSED)
			destroy_session(broker_lprintf, it_s->second.tls_sess);
		if (it_s->second.socket != INVALID_SOCKET)
			closesocket(it_s->second.socket);
	}
	m_sessions.clear();

	{
		std::lock_guard<std::mutex> lock(s_instance_mutex);
		if (s_instance == this)
			s_instance = nullptr;
	}

	log(LOG_INFO, "MQTT broker: stopped");
}

// ── Local client interface ──────────────────────────────────────

LocalClient *Broker::register_local(const std::string &client_id,
                                    void (*on_message)(void *, const char *, const void *, size_t),
                                    void *cbdata)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	std::unique_ptr<LocalClient> client(new LocalClient());
	client->client_id = client_id;
	client->on_message = on_message;
	client->cbdata = cbdata;
	auto *ptr = client.get();
	m_local_clients.push_back(std::move(client));
	log(LOG_DEBUG, "MQTT broker: local client registered: %s", client_id.c_str());
	return ptr;
}

void Broker::publish_sys(const char *topic, const void *payload, size_t len,
                         const Properties *props)
{
	auto msg = std::make_shared<Message>();
	msg->type = PUBLISH;
	msg->flags = 1;
	msg->topic = topic;
	if (payload && len > 0)
		msg->payload.assign(static_cast<const uint8_t *>(payload),
		                    static_cast<const uint8_t *>(payload) + len);
	if (props)
		msg->props = *props;
	msg->created_at = time(nullptr);

	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	route_publish("$SYS", msg);
	for (auto it_s = m_sessions.begin(); it_s != m_sessions.end(); ++it_s)
		if (!it_s->second.send_buf.empty())
			flush_network(it_s->second);
}

void Broker::deregister_local(LocalClient *client)
{
	bool last = false;
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		m_topics.unsubscribe_all(client->client_id);
		for (auto it = m_local_clients.begin(); it != m_local_clients.end(); ++it) {
			if (it->get() == client) {
				m_local_clients.erase(it);
				break;
			}
		}
		last = m_local_clients.empty();
	}
	if (last)
		stop();
}

int Broker::local_publish(LocalClient *client, const std::string &topic,
                          const void *payload, size_t len, int qos, bool retain,
                          const Properties *props)
{
	auto msg = std::make_shared<Message>();
	msg->type = PUBLISH;
	msg->flags = (qos << 1) | (retain ? 1 : 0);
	msg->topic = topic;
	if (payload && len > 0)
		msg->payload.assign(static_cast<const uint8_t *>(payload), static_cast<const uint8_t *>(payload) + len);
	if (props)
		msg->props = *props;
	msg->created_at = time(nullptr);

	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	route_publish(client->client_id, std::move(msg));
	for (auto it_s = m_sessions.begin(); it_s != m_sessions.end(); ++it_s)
		if (!it_s->second.send_buf.empty())
			flush_network(it_s->second);
	return 0;
}

int Broker::local_subscribe(LocalClient *client, const std::string &topic, int qos)
{
	SubscriptionOptions opts;
	opts.max_qos = qos;

	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	m_topics.subscribe(topic, client->client_id, opts);
	{
		std::lock_guard<std::mutex> slock(client->sub_mutex);
		client->subscriptions.push_back(topic);
	}

	m_topics.match_retained(topic, [&](const std::string &, std::shared_ptr<Message> msg) {
		if (msg) deliver_to_local(client, msg);
	});

	return 0;
}

// ── Message routing ─────────────────────────────────────────────

void Broker::route_publish(const std::string &sender_id, std::shared_ptr<Message> msg)
{
	if (msg->retain())
		m_topics.set_retained(msg->topic, msg);

	std::unordered_map<std::string, const SubscriptionOptions *> delivered;

	m_topics.publish(msg->topic, [&](const Subscriber &sub) {
		if (sub.opts.no_local && sub.client_id == sender_id)
			return;
		auto key = sub.client_id;
		if (delivered.count(key))
			return;
		delivered[key] = &sub.opts;

		for (const auto &lc : m_local_clients) {
			if (lc->client_id == sub.client_id) {
				deliver_to_local(lc.get(), msg);
				return;
			}
		}

		for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
			if (it->second.client_id == sub.client_id && it->second.connected) {
				deliver_to_network(it->second, msg, sub.opts);
				break;
			}
		}
	});
}

void Broker::deliver_to_local(LocalClient *client, std::shared_ptr<Message> msg)
{
	if (client->on_message) {
		client->on_message(client->cbdata, msg->topic.c_str(),
		                   msg->payload.data(), msg->payload.size());
	}
}

void Broker::deliver_to_network(NetworkSession &session, std::shared_ptr<Message> msg,
                                const SubscriptionOptions &opts)
{
	uint8_t qos = (std::min)(msg->qos(), opts.max_qos);

	Properties extra;
	if (opts.subscription_id > 0)
		extra.add(PROP_SUBSCRIPTION_ID, PropertyValue::from_vbi(opts.subscription_id));

	uint16_t pid = 0;
	if (qos > 0)
		pid = session.alloc_pid();

	auto pkt = build_publish(*msg, pid, false, qos,
	                          extra.empty() ? nullptr : &extra);
	send_to_network(session, pkt);

	if (qos > 0) {
		Queued q;
		q.msg = msg;
		q.pid = pid;
		session.tx_unacked[pid] = std::move(q);
	}
}

// ── Network session helpers ─────────────────────────────────────

uint16_t NetworkSession::alloc_pid()
{
	for (int i = 0; i < 65535; ++i) {
		uint16_t pid = next_pid++;
		if (next_pid == 0) next_pid = 1;
		if (tx_unacked.find(pid) == tx_unacked.end() && rx_unacked.find(pid) == rx_unacked.end())
			return pid;
	}
	return next_pid++;
}

void Broker::service_tx_queue(NetworkSession &session)
{
	while (!session.tx_queued.empty() &&
	       session.tx_unacked.size() < session.receive_maximum) {
		Queued &q = session.tx_queued.front();
		uint8_t qos = (std::min)(q.msg->qos(), (uint8_t)2);
		q.pid = session.alloc_pid();
		auto pkt = build_publish(*q.msg, q.pid, q.dup, qos, nullptr);
		send_to_network(session, pkt);
		session.tx_unacked[q.pid] = std::move(q);
		session.tx_queued.pop_front();
	}
}

void Broker::send_to_network(NetworkSession &session, const std::vector<uint8_t> &data)
{
	session.send_buf.insert(session.send_buf.end(), data.begin(), data.end());
}

void Broker::flush_network(NetworkSession &session)
{
	if (session.send_buf.empty() || session.socket == INVALID_SOCKET)
		return;
	int sent;
	if (session.tls_sess != CRYPT_UNUSED) {
		int ret = cryptPushData(session.tls_sess, session.send_buf.data(),
		                        session.send_buf.size(), &sent);
		if (ret == CRYPT_OK)
			ret = cryptFlushData(session.tls_sess);
		if (ret != CRYPT_OK) {
			char *estr = NULL;
			get_crypt_error_string(ret, session.tls_sess, &estr, "writing", NULL);
			log(LOG_ERR, "MQTT broker: TLS write error: %s (client %s)", estr ? estr : "unknown", session.client_id.c_str());
			free(estr);
			sent = -1;
		}
	} else {
		sent = send(session.socket, (const char *)session.send_buf.data(),
		            session.send_buf.size(), 0);
		if (sent < 0)
			log(LOG_ERR, "MQTT broker: send error: %d (client %s)", ERROR_VALUE, session.client_id.c_str());
	}
	if (sent < 0) {
		teardown_network(session);
		return;
	}
	if (sent > 0)
		session.send_buf.erase(session.send_buf.begin(), session.send_buf.begin() + sent);
}

void Broker::teardown_network(NetworkSession &session, uint8_t reason_code)
{
	if (session.socket == INVALID_SOCKET)
		return;

	if (reason_code != 0) {
		auto pkt = build_disconnect(reason_code);
		send_to_network(session, pkt);
		flush_network(session);
	}

	if (session.will) {
		if (session.will_delay > 0) {
			session.will_deliver_at = time(nullptr) + session.will_delay;
		} else {
			route_publish(session.client_id, session.will);
			session.will.reset();
		}
	}

	if (session.session_expiry > 0) {
		session.session_expires_at = time(nullptr) + session.session_expiry;
	} else {
		m_topics.unsubscribe_all(session.client_id);
	}

	log(LOG_INFO, "MQTT broker: client disconnected: %s", session.client_id.c_str());

	if (session.tls_sess != CRYPT_UNUSED) {
		destroy_session(broker_lprintf, session.tls_sess);
		session.tls_sess = CRYPT_UNUSED;
	}
	closesocket(session.socket);
	session.socket = INVALID_SOCKET;
	session.connected = false;
}

// ── Broker thread ───────────────────────────────────────────────

void Broker::accept_thread()
{
	SetThreadName("sbbs/mqttAccept");
	while (m_running) {
		struct sockaddr_storage addr;
		socklen_t addrlen = sizeof(addr);
		int sock = accept(m_listen_sock, (struct sockaddr *)&addr, &addrlen);
		if (sock < 0) {
			if (!m_running) break;
			continue;
		}
		accept_connection(sock);
	}
}

void Broker::broker_thread()
{
	SetThreadName("sbbs/mqttBroker");
	while (m_running) {
		std::vector<int> keys;
		std::vector<SOCKET> sockets;
		std::vector<bool> want_write;

		{
			std::lock_guard<std::recursive_mutex> lock(m_mutex);
			for (auto it_s = m_sessions.begin(); it_s != m_sessions.end(); ++it_s) {
				if (it_s->second.socket != INVALID_SOCKET) {
					keys.push_back(it_s->first);
					sockets.push_back(it_s->second.socket);
					want_write.push_back(!it_s->second.send_buf.empty());
				}
			}
		}

		if (sockets.empty()) {
			SLEEP(1000);
			std::lock_guard<std::recursive_mutex> lock(m_mutex);
			process_local_queues();
			continue;
		}

		int ready;
#ifdef PREFER_POLL
		std::vector<struct pollfd> pfds(sockets.size());
		for (size_t i = 0; i < sockets.size(); ++i) {
			pfds[i].fd = sockets[i];
			pfds[i].events = POLLIN;
			if (want_write[i])
				pfds[i].events |= POLLOUT;
			pfds[i].revents = 0;
		}
		ready = poll(pfds.data(), pfds.size(), 1000);
#else
		fd_set rfds, wfds;
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		int maxfd = 0;
		for (size_t i = 0; i < sockets.size(); ++i) {
			FD_SET(sockets[i], &rfds);
			if (want_write[i])
				FD_SET(sockets[i], &wfds);
			if ((int)sockets[i] > maxfd)
				maxfd = (int)sockets[i];
		}
		struct timeval tv = {1, 0};
		ready = select(maxfd + 1, &rfds, &wfds, NULL, &tv);
#endif
		if (ready < 0) {
			if (ERROR_VALUE == EINTR) continue;
			break;
		}

		{
			std::lock_guard<std::recursive_mutex> lock(m_mutex);

			std::vector<int> to_remove;
			for (size_t i = 0; i < keys.size(); ++i) {
				auto it_s = m_sessions.find(keys[i]);
				if (it_s == m_sessions.end()) continue;
				auto &session = it_s->second;
				if (session.socket == INVALID_SOCKET) {
					to_remove.push_back(it_s->first);
					continue;
				}
#ifdef PREFER_POLL
				bool readable = pfds[i].revents & (POLLIN | POLLHUP | POLLERR);
				bool writable = pfds[i].revents & POLLOUT;
#else
				bool readable = FD_ISSET(session.socket, &rfds);
				bool writable = FD_ISSET(session.socket, &wfds);
#endif
				if (writable && !session.send_buf.empty())
					flush_network(session);
				if (readable)
					handle_network_data(session);
				if (!session.send_buf.empty())
					flush_network(session);
				if (session.connected && session.keep_alive > 0) {
					time_t now = time(nullptr);
					if (now - session.last_activity > (time_t)(session.keep_alive * 3 / 2)) {
						log(LOG_NOTICE, "MQTT broker: keep-alive timeout: %s", session.client_id.c_str());
						teardown_network(session);
					}
				}
			}
			// Process will delays and session expiry
			time_t now = time(nullptr);
			for (auto it_s = m_sessions.begin(); it_s != m_sessions.end(); ++it_s) {
				auto &session = it_s->second;
				if (session.will && session.will_deliver_at > 0 && now >= session.will_deliver_at) {
					route_publish(session.client_id, session.will);
					session.will.reset();
					session.will_deliver_at = 0;
				}
				if (session.socket == INVALID_SOCKET && session.session_expires_at > 0 && now >= session.session_expires_at) {
					m_topics.unsubscribe_all(session.client_id);
					to_remove.push_back(it_s->first);
				}
			}

			for (const auto &id : to_remove)
				m_sessions.erase(id);

			static time_t last_purge = 0;
			if (now - last_purge >= 60) {
				size_t purged = m_topics.purge_expired_retained();
				if (purged > 0)
					log(LOG_DEBUG, "MQTT broker: purged %zu expired retained messages", purged);
				last_purge = now;
			}

			process_local_queues();
		}
	}
}

void Broker::process_local_queues()
{
	for (auto &lc : m_local_clients) {
		std::lock_guard<std::mutex> lock(lc->send_mutex);
		while (!lc->send_queue.empty()) {
			auto msg = std::move(lc->send_queue.front());
			lc->send_queue.pop_front();
			route_publish(lc->client_id, std::move(msg));
		}
	}
}

// ── Network accept ──────────────────────────────────────────────

void Broker::accept_connection(int sock)
{
	int nodelay = 1;
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay, sizeof(nodelay));

	CRYPT_SESSION tls_sess = CRYPT_UNUSED;
	int ret;

	if (!do_cryptInit(broker_lprintf)) {
		log(LOG_ERR, "MQTT broker: cryptlib init failed");
		closesocket(sock);
		return;
	}

	ret = cryptCreateSession(&tls_sess, CRYPT_UNUSED, CRYPT_SESSION_TLS_SERVER);
	if (ret != CRYPT_OK) {
		char *estr = NULL;
		get_crypt_error_string(ret, CRYPT_UNUSED, &estr, "creating TLS session", NULL);
		log(LOG_ERR, "MQTT broker: %s", estr ? estr : "cryptCreateSession failed");
		free(estr);
		closesocket(sock);
		return;
	}

	for (auto it = m_psk_table.begin(); it != m_psk_table.end(); ++it) {
		int psk_ret = cryptSetAttributeString(tls_sess, CRYPT_SESSINFO_USERNAME,
		                            it->first.c_str(), it->first.size());
		if (psk_ret == CRYPT_OK)
			cryptSetAttributeString(tls_sess, CRYPT_SESSINFO_PASSWORD,
			                        it->second.c_str(), it->second.size());
		else
			log(LOG_WARNING, "MQTT broker: failed to add PSK identity '%s': %d",
			    it->first.c_str(), psk_ret);
	}
	log(LOG_DEBUG, "MQTT broker: added %zu PSK identities to TLS session", m_psk_table.size());

	ret = add_private_key(m_cfg, broker_lprintf, tls_sess);
	if (ret != CRYPT_OK) {
		char *estr = NULL;
		get_crypt_error_string(ret, tls_sess, &estr, "setting private key", NULL);
		log(LOG_ERR, "MQTT broker: %s", estr ? estr : "add_private_key failed");
		free(estr);
		cryptDestroySession(tls_sess);
		closesocket(sock);
		return;
	}

	cryptSetAttribute(tls_sess, CRYPT_SESSINFO_TLS_OPTIONS, CRYPT_TLSOPTION_MINVER_TLS12);
	ret = cryptSetAttribute(tls_sess, CRYPT_SESSINFO_NETWORKSOCKET, sock);
	if (ret != CRYPT_OK) {
		char *estr = NULL;
		get_crypt_error_string(ret, tls_sess, &estr, "setting network socket", NULL);
		log(LOG_ERR, "MQTT broker: %s", estr ? estr : "set socket failed");
		free(estr);
		destroy_session(broker_lprintf, tls_sess);
		closesocket(sock);
		return;
	}

	ret = cryptSetAttribute(tls_sess, CRYPT_SESSINFO_ACTIVE, 1);
	if (ret != CRYPT_OK) {
		char *estr = NULL;
		get_crypt_error_string(ret, tls_sess, &estr, "TLS handshake", NULL);
		log(LOG_ERR, "MQTT broker: %s", estr ? estr : "TLS handshake failed");
		free(estr);
		destroy_session(broker_lprintf, tls_sess);
		closesocket(sock);
		return;
	}

	cryptSetAttribute(tls_sess, CRYPT_OPTION_NET_READTIMEOUT, 0);

	std::string psk_id;
	int attr_val = 0;
	if (cryptGetAttribute(tls_sess, CRYPT_SESSINFO_TLS_OPTIONS, &attr_val) == CRYPT_OK
	    && (attr_val & CRYPT_TLSOPTION_USED_PSK)) {
		char id_buf[256] = {};
		int id_len = 0;
		if (cryptGetAttributeString(tls_sess, CRYPT_SESSINFO_USERNAME, id_buf, &id_len) == CRYPT_OK)
			psk_id.assign(id_buf, id_len);
	}

	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	m_sessions.erase(sock);
	auto &session = m_sessions[sock];
	session.client_id = "socket-" + std::to_string(sock);
	session.socket = sock;
	session.tls_sess = tls_sess;
	session.tls_psk_id = psk_id;
	session.last_activity = time(nullptr);
	log(LOG_DEBUG, "MQTT broker: new TLS connection on socket %d", sock);
}

// ── Network data handling ───────────────────────────────────────

void Broker::handle_network_data(NetworkSession &session)
{
	uint8_t buf[65536];
	int n;
	if (session.tls_sess != CRYPT_UNUSED) {
		int ret = cryptPopData(session.tls_sess, buf, sizeof(buf), &n);
		if (ret != CRYPT_OK || n <= 0) {
			if (ret != CRYPT_OK && ret != CRYPT_ERROR_TIMEOUT) {
				char *estr = NULL;
				get_crypt_error_string(ret, session.tls_sess, &estr, "reading", NULL);
				log(LOG_ERR, "MQTT broker: TLS read error: %s (client %s)", estr ? estr : "unknown", session.client_id.c_str());
				free(estr);
			}
			if (ret != CRYPT_ERROR_TIMEOUT) {
				teardown_network(session);
				return;
			}
		}
		if (n > 0)
			session.recv_buf.insert(session.recv_buf.end(), buf, buf + n);
		// Drain any additional buffered TLS data
		while (n > 0) {
			cryptSetAttribute(session.tls_sess, CRYPT_OPTION_NET_READTIMEOUT, 0);
			ret = cryptPopData(session.tls_sess, buf, sizeof(buf), &n);
			if (ret == CRYPT_OK && n > 0)
				session.recv_buf.insert(session.recv_buf.end(), buf, buf + n);
			else
				break;
		}
	} else {
		n = recv(session.socket, (char *)buf, sizeof(buf), 0);
		if (n <= 0) {
			teardown_network(session);
			return;
		}
		session.recv_buf.insert(session.recv_buf.end(), buf, buf + n);
	}
	session.last_activity = time(nullptr);

	while (session.recv_buf.size() >= 2) {
		uint8_t type, flags;
		uint32_t remaining;
		int hdr_len = read_fixed_header(session.recv_buf.data(), session.recv_buf.size(),
		                                type, flags, remaining);
		if (hdr_len == 0) break;
		if (hdr_len < 0) {
			teardown_network(session, 0x81);
			return;
		}
		size_t total = hdr_len + remaining;
		if (session.recv_buf.size() < total) break;

		handle_packet(session, type, flags,
		              session.recv_buf.data() + hdr_len, remaining);

		if (session.socket == INVALID_SOCKET) return;

		session.recv_buf.erase(session.recv_buf.begin(), session.recv_buf.begin() + total);
	}
}

void Broker::handle_packet(NetworkSession &session, uint8_t type, uint8_t flags,
                           const uint8_t *data, size_t len)
{
	switch (type) {
	case CONNECT:    handle_connect(session, data, len); break;
	case PUBLISH:    handle_publish(session, flags, data, len); break;
	case PUBACK:     handle_puback(session, data, len); break;
	case PUBREC:     handle_pubrec(session, data, len); break;
	case PUBREL:     handle_pubrel(session, data, len); break;
	case SUBSCRIBE:  handle_subscribe(session, data, len); break;
	case UNSUBSCRIBE: handle_unsubscribe(session, data, len); break;
	case PINGREQ:    handle_pingreq(session); break;
	case DISCONNECT: handle_disconnect(session, data, len); break;
	default:
		log(LOG_WARNING, "MQTT broker: unsupported packet type %d", type);
		teardown_network(session, 0x81);
		break;
	}
}

// ── CONNECT ─────────────────────────────────────────────────────

void Broker::handle_connect(NetworkSession &session, const uint8_t *data, size_t len)
{
	Reader r(data, len);
	ConnectData cd;
	uint8_t reason_code;

	if (!parse_connect(r, cd, reason_code)) {
		Properties props;
		send_to_network(session, build_connack(false, reason_code, props));
		teardown_network(session);
		return;
	}

	if (session.tls_psk_id.empty()) {
		if (!cd.has_username() || !cd.has_password()) {
			log(LOG_WARNING, "MQTT broker: auth rejected: no username/password (non-PSK connection)");
			Properties props;
			send_to_network(session, build_connack(false, 0x86, props));
			teardown_network(session);
			return;
		}
		if (!authenticate_user(cd.username, cd.password)) {
			log(LOG_WARNING, "MQTT broker: auth rejected: invalid credentials for '%s'", cd.username.c_str());
			Properties props;
			send_to_network(session, build_connack(false, 0x86, props));
			teardown_network(session);
			return;
		}
	} else {
		if (!cd.has_password() || !authenticate_psk(session.tls_psk_id, cd.password)) {
			log(LOG_WARNING, "MQTT broker: auth rejected: PSK auth failed for '%s'", session.tls_psk_id.c_str());
			Properties props;
			send_to_network(session, build_connack(false, 0x86, props));
			teardown_network(session);
			return;
		}
	}

	std::string client_id = cd.client_id;
	if (client_id.empty()) {
		client_id = "auto-" + std::to_string(session.socket) + "-" + std::to_string(time(nullptr));
	}

	// Handle existing session with the same client_id
	bool session_present = false;
	for (auto it_s = m_sessions.begin(); it_s != m_sessions.end(); ++it_s) {
		if (&it_s->second != &session && it_s->second.client_id == client_id) {
			if (it_s->second.socket != INVALID_SOCKET)
				teardown_network(it_s->second);
			if (!cd.clean_start() && it_s->second.session_expiry > 0) {
				session.subscriptions = std::move(it_s->second.subscriptions);
				session.tx_unacked = std::move(it_s->second.tx_unacked);
				session.tx_queued = std::move(it_s->second.tx_queued);
				session_present = true;
			} else {
				m_topics.unsubscribe_all(client_id);
			}
			// Cancel pending will
			it_s->second.will.reset();
			it_s->second.will_deliver_at = 0;
			m_sessions.erase(it_s);
			break;
		}
	}

	session.client_id = client_id;
	session.connected = true;
	session.clean_start = cd.clean_start();
	session.keep_alive = cd.keep_alive;
	session.session_expiry = cd.props.get_u32(PROP_SESSION_EXPIRY, 0);
	session.receive_maximum = cd.props.get_u16(PROP_RECEIVE_MAXIMUM, 65535);
	session.max_packet_size = cd.props.get_u32(PROP_MAXIMUM_PACKET_SIZE, 0);
	session.last_activity = time(nullptr);

	if (cd.will_flag()) {
		auto will = std::make_shared<Message>();
		will->type = PUBLISH;
		will->flags = (cd.will_qos() << 1) | (cd.will_retain() ? 1 : 0);
		will->topic = cd.will_topic;
		will->payload = cd.will_payload;
		will->props = cd.will_props;
		will->created_at = time(nullptr);
		session.will = std::move(will);
		session.will_delay = cd.will_props.get_u32(PROP_WILL_DELAY, 0);
	}

	Properties connack_props;
	connack_props.add(PROP_RETAIN_AVAILABLE, PropertyValue::from_byte(1));
	connack_props.add(PROP_WILDCARD_SUB_AVAILABLE, PropertyValue::from_byte(1));
	connack_props.add(PROP_SUB_ID_AVAILABLE, PropertyValue::from_byte(1));
	connack_props.add(PROP_SHARED_SUB_AVAILABLE, PropertyValue::from_byte(0));
	if (client_id != cd.client_id)
		connack_props.add(PROP_ASSIGNED_CLIENT_ID, PropertyValue::from_utf8(client_id));

	send_to_network(session, build_connack(session_present, 0, connack_props));
	flush_network(session);
	log(LOG_INFO, "MQTT broker: client connected: %s", client_id.c_str());
}

// ── PUBLISH ─────────────────────────────────────────────────────

void Broker::handle_publish(NetworkSession &session, uint8_t flags, const uint8_t *data, size_t len)
{
	Reader r(data, len);
	std::shared_ptr<Message> msg;
	uint16_t pid;
	if (!parse_publish(r, flags, msg, pid)) {
		teardown_network(session, 0x81);
		return;
	}

	uint8_t qos = msg->qos();

	route_publish(session.client_id, msg);

	if (qos == 1) {
		send_to_network(session, build_ack(PUBACK, pid, 0));
	} else if (qos == 2) {
		session.rx_unacked[pid] = true;
		send_to_network(session, build_ack(PUBREC, pid, 0));
	}
}

// ── SUBSCRIBE ───────────────────────────────────────────────────

void Broker::handle_subscribe(NetworkSession &session, const uint8_t *data, size_t len)
{
	Reader r(data, len);
	uint16_t pid;
	std::vector<TopicFilter> filters;
	Properties props;

	if (!parse_subscribe(r, pid, filters, props)) {
		teardown_network(session, 0x81);
		return;
	}

	uint32_t sub_id = props.get_u32(PROP_SUBSCRIPTION_ID, 0);

	std::vector<uint8_t> reason_codes;
	for (const auto &tf : filters) {
		if (TopicTree::is_shared_filter(tf.filter)) {
			reason_codes.push_back(0x9E);
			continue;
		}
		if (!m_topics.validate_filter(tf.filter)) {
			reason_codes.push_back(0x8F);
			continue;
		}

		SubscriptionOptions opts;
		opts.max_qos = tf.qos();
		opts.no_local = tf.no_local();
		opts.retain_as_published = tf.retain_as_published();
		opts.retain_handling = tf.retain_handling();
		opts.subscription_id = sub_id;

		m_topics.subscribe(tf.filter, session.client_id, opts);
		session.subscriptions.push_back(tf.filter);
		log(LOG_DEBUG, "MQTT broker: %s subscribed to %s", session.client_id.c_str(), tf.filter.c_str());

		if (opts.retain_handling != 2) {
			m_topics.match_retained(tf.filter, [&](const std::string &, std::shared_ptr<Message> msg) {
				if (!msg) return;
				if (msg->props.has(PROP_MESSAGE_EXPIRY)) {
					uint32_t expiry = msg->props.get_u32(PROP_MESSAGE_EXPIRY, 0);
					if (time(nullptr) - msg->created_at > (time_t)expiry)
						return;
				}
				deliver_to_network(session, msg, opts);
			});
		}

		reason_codes.push_back(tf.qos());
	}

	send_to_network(session, build_suback(pid, reason_codes, {}));
}

// ── UNSUBSCRIBE ─────────────────────────────────────────────────

void Broker::handle_unsubscribe(NetworkSession &session, const uint8_t *data, size_t len)
{
	Reader r(data, len);
	uint16_t pid;
	std::vector<std::string> filters;
	Properties props;

	if (!parse_unsubscribe(r, pid, filters, props)) {
		teardown_network(session, 0x81);
		return;
	}

	std::vector<uint8_t> reason_codes;
	for (const auto &filter : filters) {
		m_topics.unsubscribe(filter, session.client_id);
		reason_codes.push_back(0);
		log(LOG_DEBUG, "MQTT broker: %s unsubscribed from %s",
		    session.client_id.c_str(), filter.c_str());
	}

	send_to_network(session, build_unsuback(pid, reason_codes, {}));
}

// ── QoS acks ────────────────────────────────────────────────────

void Broker::handle_puback(NetworkSession &session, const uint8_t *data, size_t len)
{
	Reader r(data, len);
	uint16_t pid;
	uint8_t reason;
	Properties props;
	if (!parse_ack(r, pid, reason, props)) return;
	session.tx_unacked.erase(pid);
}

void Broker::handle_pubrec(NetworkSession &session, const uint8_t *data, size_t len)
{
	Reader r(data, len);
	uint16_t pid;
	uint8_t reason;
	Properties props;
	if (!parse_ack(r, pid, reason, props)) return;

	if (reason >= 0x80) {
		session.tx_unacked.erase(pid);
		return;
	}
	send_to_network(session, build_ack(PUBREL, pid, 0));
}

void Broker::handle_pubrel(NetworkSession &session, const uint8_t *data, size_t len)
{
	Reader r(data, len);
	uint16_t pid;
	uint8_t reason;
	Properties props;
	if (!parse_ack(r, pid, reason, props)) return;

	session.rx_unacked.erase(pid);
	send_to_network(session, build_ack(PUBCOMP, pid, 0));
}

// ── PINGREQ / DISCONNECT ───────────────────────────────────────

void Broker::handle_pingreq(NetworkSession &session)
{
	log(LOG_DEBUG, "MQTT broker: ping from %s", session.client_id.c_str());
	send_to_network(session, build_pingresp());
}

void Broker::handle_disconnect(NetworkSession &session, const uint8_t *data, size_t len)
{
	Reader r(data, len);
	uint8_t reason_code;
	Properties props;
	parse_disconnect(r, reason_code, props);

	if (reason_code == 0)
		session.will.reset();

	closesocket(session.socket);
	session.socket = INVALID_SOCKET;
	session.connected = false;
	log(LOG_INFO, "MQTT broker: client disconnected gracefully: %s", session.client_id.c_str());
}

} // namespace mqtt5
