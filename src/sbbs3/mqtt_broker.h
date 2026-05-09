#ifndef MQTT_BROKER_H
#define MQTT_BROKER_H

#include "mqtt_protocol.h"
#include "mqtt_topic.h"

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "scfgdefs.h"
#include "userdat.h"
#include "ssl.h"
#include "cryptlib.h"

namespace mqtt5 {

struct LocalClient {
	std::string client_id;
	void (*on_message)(void *cbdata, const char *topic,
	                   const void *payload, size_t len);
	void *cbdata;

	std::mutex send_mutex;
	std::deque<std::shared_ptr<Message>> send_queue;

	std::mutex sub_mutex;
	std::vector<std::string> subscriptions;
};

struct NetworkSession {
	std::string client_id;
	int socket = -1;
	CRYPT_SESSION tls_sess = CRYPT_UNUSED;
	bool connected = false;
	bool clean_start = false;

	std::vector<uint8_t> recv_buf;
	std::vector<uint8_t> send_buf;

	uint32_t keep_alive = 0;
	uint32_t session_expiry = 0;
	uint16_t receive_maximum = 65535;
	uint32_t max_packet_size = 0;
	time_t last_activity = 0;

	uint16_t next_pid = 1;
	std::unordered_map<uint16_t, Queued> tx_unacked;
	std::deque<Queued> tx_queued;
	std::unordered_map<uint16_t, bool> rx_unacked;

	std::shared_ptr<Message> will;
	uint32_t will_delay = 0;
	time_t will_deliver_at = 0;
	time_t session_expires_at = 0;

	std::vector<std::string> subscriptions;

	std::string tls_psk_id;

	uint16_t alloc_pid();
};

class Broker {
public:
	Broker();
	~Broker();

	bool start(scfg_t *cfg, uint16_t port, int (*lputs)(void *, int, const char *), void *lputs_data);
	void stop();
	bool is_running() const { return m_running; }

	LocalClient *register_local(const std::string &client_id,
	                            void (*on_message)(void *, const char *, const void *, size_t),
	                            void *cbdata);
	void deregister_local(LocalClient *client);

	int local_publish(LocalClient *client, const std::string &topic,
	                  const void *payload, size_t len, int qos, bool retain);
	int local_subscribe(LocalClient *client, const std::string &topic, int qos);

	void publish_sys(const char *topic, const void *payload, size_t len);

	static Broker *instance();

private:
	void broker_thread();
	void log(int level, const char *fmt, ...);

	void process_local_queues();
	void route_publish(const std::string &sender_id, std::shared_ptr<Message> msg);
	void deliver_to_local(LocalClient *client, std::shared_ptr<Message> msg);
	void deliver_to_network(NetworkSession &session, std::shared_ptr<Message> msg,
	                        const SubscriptionOptions &opts);

	void accept_connection(int listen_sock);
	void handle_network_data(NetworkSession &session);
	void handle_packet(NetworkSession &session, uint8_t type, uint8_t flags,
	                   const uint8_t *data, size_t len);
	void handle_connect(NetworkSession &session, const uint8_t *data, size_t len);
	void handle_publish(NetworkSession &session, uint8_t flags, const uint8_t *data, size_t len);
	void handle_subscribe(NetworkSession &session, const uint8_t *data, size_t len);
	void handle_unsubscribe(NetworkSession &session, const uint8_t *data, size_t len);
	void handle_pingreq(NetworkSession &session);
	void handle_disconnect(NetworkSession &session, const uint8_t *data, size_t len);
	void handle_puback(NetworkSession &session, const uint8_t *data, size_t len);
	void handle_pubrec(NetworkSession &session, const uint8_t *data, size_t len);
	void handle_pubrel(NetworkSession &session, const uint8_t *data, size_t len);

	void send_to_network(NetworkSession &session, const std::vector<uint8_t> &data);
	void flush_network(NetworkSession &session);
	void service_tx_queue(NetworkSession &session);
	void teardown_network(NetworkSession &session, uint8_t reason_code = 0);

	bool authenticate_psk(const std::string &identity, const std::string &password);
	bool authenticate_user(const std::string &username, const std::string &password);
	void build_psk_table();

	TopicTree m_topics;
	std::recursive_mutex m_mutex;

	std::vector<std::unique_ptr<LocalClient>> m_local_clients;
	std::unordered_map<std::string, NetworkSession> m_sessions;

	std::unordered_map<std::string, std::string> m_psk_table;

	scfg_t *m_cfg = nullptr;
	int (*m_lputs)(void *, int, const char *) = nullptr;
	void *m_lputs_data = nullptr;

	std::atomic<bool> m_running{false};
	std::thread m_thread;
	int m_listen_sock = -1;
	int m_wakeup_pipe[2] = {-1, -1};

	static Broker *s_instance;
	static std::mutex s_instance_mutex;
};

} // namespace mqtt5

#endif
