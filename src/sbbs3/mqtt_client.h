#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include "sockwrap.h"
#include "scfgdefs.h"

#ifdef __cplusplus

#include "mqtt_protocol.h"
#include "cryptlib.h"
#include "ssl.h"

#include <deque>
#include <set>
#include <string>
#include <vector>

namespace mqtt5 {

struct ReceivedMessage {
	std::string topic;
	std::vector<uint8_t> payload;
	uint16_t mid;
	uint8_t qos;
	bool retain;
};

class Client {
public:
	Client();
	~Client();

	void set_will(const char *topic, const void *payload, size_t len,
	              int qos, bool retain);

	int connect(const char *host, uint16_t port, const char *client_id,
	            const char *username, const char *password,
	            int keepalive, int protocol_version,
	            int tls_mode, const char *psk, const char *psk_identity,
	            const char *cafile, const char *certfile,
	            const char *keyfile, const char *keypass,
	            scfg_t *scfg,
	            int (*lprintf)(int level, const char *fmt, ...) = NULL);
	void disconnect();

	int publish(const char *topic, const void *payload, size_t len,
	            int qos, bool retain, const Properties *props = nullptr);
	int subscribe(const char *topic, int qos);

	ReceivedMessage *read(int timeout_ms);

	bool data_waiting();
	int read_level();

	int pump(int timeout_ms);

	int last_error() const { return m_last_error; }
	const char *error_str() const;

private:
	bool send_packet(const std::vector<uint8_t> &pkt);
	int recv_data(int timeout_ms);
	void handle_packet(uint8_t type, uint8_t flags, const uint8_t *data, size_t len);

	SOCKET m_sock;
	CRYPT_SESSION m_tls_sess;
	bool m_connected;
	int m_last_error;
	uint16_t m_next_pid;

	std::vector<uint8_t> m_recv_buf;
	std::deque<ReceivedMessage> m_queue;
	ReceivedMessage m_last_read;
	std::set<uint16_t> m_acked_pids;

	std::string m_will_topic;
	std::vector<uint8_t> m_will_payload;
	uint8_t m_will_qos;
	bool m_will_retain;
	bool m_has_will;

	int m_keepalive;
	time_t m_last_send;
};

} // namespace mqtt5

extern "C" {
#endif

/* C-callable wrappers for mqtt5::Client */
void *mqtt5client_open(void);
void  mqtt5client_close(void *handle);
void  mqtt5client_set_will(void *handle, const char *topic,
          const void *payload, size_t len, int qos, int retain);
int   mqtt5client_connect(void *handle, const char *host, uint16_t port,
          const char *client_id, const char *username, const char *password,
          int keepalive, int tls_mode,
          const char *psk, const char *psk_identity,
          const char *cafile, const char *certfile,
          const char *keyfile, const char *keypass,
          scfg_t *scfg,
          int (*lprintf)(int level, const char *fmt, ...));
void  mqtt5client_disconnect(void *handle);
int   mqtt5client_publish(void *handle, const char *topic,
          const void *payload, size_t len, int qos, int retain,
          const void *props);
int   mqtt5client_subscribe(void *handle, const char *topic, int qos);
int   mqtt5client_pump(void *handle, int timeout_ms);

struct mqtt5client_msg {
	char *topic;
	void *payload;
	size_t payload_len;
};
struct mqtt5client_msg *mqtt5client_read(void *handle);
void  mqtt5client_read_free(struct mqtt5client_msg *msg);

#ifdef __cplusplus
}
#endif

#endif
