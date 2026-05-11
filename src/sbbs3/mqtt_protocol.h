#ifndef SBBS_MQTT_PROTOCOL_H
#define SBBS_MQTT_PROTOCOL_H

#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

namespace mqtt5 {

enum PacketType : uint8_t {
	CONNECT     = 1,
	CONNACK     = 2,
	PUBLISH     = 3,
	PUBACK      = 4,
	PUBREC      = 5,
	PUBREL      = 6,
	PUBCOMP     = 7,
	SUBSCRIBE   = 8,
	SUBACK      = 9,
	UNSUBSCRIBE = 10,
	UNSUBACK    = 11,
	PINGREQ     = 12,
	PINGRESP    = 13,
	DISCONNECT  = 14,
	AUTH        = 15,
};

const char *packet_type_name(uint8_t type);

enum PropertyId : uint8_t {
	PROP_PAYLOAD_FORMAT         = 1,
	PROP_MESSAGE_EXPIRY         = 2,
	PROP_CONTENT_TYPE           = 3,
	PROP_RESPONSE_TOPIC         = 8,
	PROP_CORRELATION_DATA       = 9,
	PROP_SUBSCRIPTION_ID        = 11,
	PROP_SESSION_EXPIRY         = 17,
	PROP_ASSIGNED_CLIENT_ID     = 18,
	PROP_SERVER_KEEP_ALIVE      = 19,
	PROP_AUTH_METHOD             = 21,
	PROP_AUTH_DATA               = 22,
	PROP_REQUEST_PROBLEM_INFO   = 23,
	PROP_WILL_DELAY             = 24,
	PROP_REQUEST_RESPONSE_INFO  = 25,
	PROP_RESPONSE_INFO          = 26,
	PROP_SERVER_REFERENCE       = 28,
	PROP_REASON_STRING          = 31,
	PROP_RECEIVE_MAXIMUM        = 33,
	PROP_TOPIC_ALIAS_MAXIMUM    = 34,
	PROP_TOPIC_ALIAS            = 35,
	PROP_MAXIMUM_QOS            = 36,
	PROP_RETAIN_AVAILABLE       = 37,
	PROP_USER_PROPERTY          = 38,
	PROP_MAXIMUM_PACKET_SIZE    = 39,
	PROP_WILDCARD_SUB_AVAILABLE = 40,
	PROP_SUB_ID_AVAILABLE       = 41,
	PROP_SHARED_SUB_AVAILABLE   = 42,
};

enum PropertyType : uint8_t {
	PT_BYTE,
	PT_TWO_BYTE,
	PT_FOUR_BYTE,
	PT_VBI,
	PT_UTF8,
	PT_BINARY,
	PT_UTF8_PAIR,
};

struct PropertyDef {
	PropertyId id;
	const char *name;
	PropertyType type;
	bool multiple;
};

const PropertyDef *property_def(uint8_t id);

using StringPair = std::pair<std::string, std::string>;

struct PropertyValue {
	PropertyType type;
	uint32_t int_val;
	std::string str_val;
	std::string str_val2;
	std::vector<uint8_t> bin_val;

	PropertyValue() : type(PT_BYTE), int_val(0) {}

	static PropertyValue from_byte(uint8_t v)     { PropertyValue p; p.type = PT_BYTE; p.int_val = v; return p; }
	static PropertyValue from_u16(uint16_t v)      { PropertyValue p; p.type = PT_TWO_BYTE; p.int_val = v; return p; }
	static PropertyValue from_u32(uint32_t v)      { PropertyValue p; p.type = PT_FOUR_BYTE; p.int_val = v; return p; }
	static PropertyValue from_vbi(uint32_t v)      { PropertyValue p; p.type = PT_VBI; p.int_val = v; return p; }
	static PropertyValue from_utf8(std::string v)  { PropertyValue p; p.type = PT_UTF8; p.str_val = std::move(v); return p; }
	static PropertyValue from_binary(std::vector<uint8_t> v) { PropertyValue p; p.type = PT_BINARY; p.bin_val = std::move(v); return p; }
	static PropertyValue from_pair(std::string k, std::string v) {
		PropertyValue p; p.type = PT_UTF8_PAIR; p.str_val = std::move(k); p.str_val2 = std::move(v); return p;
	}
	static PropertyValue from_pair(StringPair sp) {
		return from_pair(std::move(sp.first), std::move(sp.second));
	}

	uint8_t as_byte() const  { return static_cast<uint8_t>(int_val); }
	uint16_t as_u16() const  { return static_cast<uint16_t>(int_val); }
	uint32_t as_u32() const  { return int_val; }
	StringPair as_pair() const { return StringPair(str_val, str_val2); }
};

struct Property {
	uint8_t id;
	PropertyValue value;
};

class Properties {
public:
	void add(uint8_t id, PropertyValue val);
	bool has(uint8_t id) const;

	uint8_t get_byte(uint8_t id, uint8_t def = 0) const;
	uint16_t get_u16(uint8_t id, uint16_t def = 0) const;
	uint32_t get_u32(uint8_t id, uint32_t def = 0) const;
	const std::string *get_string(uint8_t id) const;
	const std::vector<uint8_t> *get_binary(uint8_t id) const;

	std::vector<uint32_t> get_all_u32(uint8_t id) const;
	std::vector<StringPair> get_all_pairs(uint8_t id) const;

	const std::vector<Property> &entries() const { return m_entries; }
	void clear() { m_entries.clear(); }
	bool empty() const { return m_entries.empty(); }

private:
	std::vector<Property> m_entries;
};

// Immutable after creation — shared across subscriber queues via shared_ptr
struct Message {
	uint8_t type;
	uint8_t flags;
	std::string topic;
	std::vector<uint8_t> payload;
	Properties props;
	time_t created_at;

	uint8_t qos() const { return (flags >> 1) & 3; }
	bool retain() const { return flags & 1; }
	bool dup() const { return (flags >> 3) & 1; }
};

struct SubscriptionOptions {
	uint8_t max_qos = 2;
	uint32_t subscription_id = 0;
	bool no_local = false;
	bool retain_as_published = false;
	uint8_t retain_handling = 0;
};

// Per-message-per-queue entry — only what varies
struct Queued {
	std::shared_ptr<Message> msg;
	uint16_t pid = 0;
	bool dup = false;
};

// CONNECT packet payload
struct ConnectData {
	std::string protocol_name;
	uint8_t protocol_version = 0;
	uint8_t connect_flags = 0;
	uint16_t keep_alive = 0;
	Properties props;
	std::string client_id;
	Properties will_props;
	std::string will_topic;
	std::vector<uint8_t> will_payload;
	std::string username;
	std::string password;

	bool clean_start() const { return connect_flags & 0x02; }
	bool will_flag() const   { return connect_flags & 0x04; }
	uint8_t will_qos() const { return (connect_flags >> 3) & 3; }
	bool will_retain() const { return connect_flags & 0x20; }
	bool has_password() const { return connect_flags & 0x40; }
	bool has_username() const { return connect_flags & 0x80; }
};

// SUBSCRIBE payload
struct TopicFilter {
	std::string filter;
	uint8_t options = 0;

	uint8_t qos() const { return options & 3; }
	bool no_local() const { return options & 0x04; }
	bool retain_as_published() const { return options & 0x08; }
	uint8_t retain_handling() const { return (options >> 4) & 3; }
};

// ── Binary codec ────────────────────────────────────────────────

class Reader {
public:
	Reader(const uint8_t *data, size_t len) : m_data(data), m_len(len), m_pos(0) {}

	size_t remaining() const { return m_len - m_pos; }
	bool at_end() const { return m_pos >= m_len; }

	bool read_byte(uint8_t &out);
	bool read_u16(uint16_t &out);
	bool read_u32(uint32_t &out);
	bool read_vbi(uint32_t &out);
	bool read_utf8(std::string &out);
	bool read_binary(std::vector<uint8_t> &out);
	bool read_utf8_pair(StringPair &out);
	bool read_bytes(uint8_t *out, size_t count);
	bool read_bytes(std::vector<uint8_t> &out, size_t count);
	bool skip(size_t count);

	bool read_properties(Properties &props);

private:
	const uint8_t *m_data;
	size_t m_len;
	size_t m_pos;
};

class Writer {
public:
	void write_byte(uint8_t val);
	void write_u16(uint16_t val);
	void write_u32(uint32_t val);
	void write_vbi(uint32_t val);
	void write_utf8(const std::string &str);
	void write_binary(const std::vector<uint8_t> &data);
	void write_utf8_pair(const StringPair &pair);
	void write_bytes(const uint8_t *data, size_t len);

	void write_properties(const Properties &props);

	const std::vector<uint8_t> &data() const { return m_buf; }
	std::vector<uint8_t> take() { return std::move(m_buf); }
	size_t size() const { return m_buf.size(); }
	void clear() { m_buf.clear(); }

private:
	std::vector<uint8_t> m_buf;
};

// ── Packet-level codec ──────────────────────────────────────────

// Parse a complete MQTT packet (fixed header + remaining bytes already read).
// Returns false on malformed packet. reason_code set on protocol error.
bool parse_connect(Reader &r, ConnectData &out, uint8_t &reason_code);
bool parse_publish(Reader &r, uint8_t flags, std::shared_ptr<Message> &out, uint16_t &pid);
bool parse_subscribe(Reader &r, uint16_t &pid, std::vector<TopicFilter> &filters, Properties &props);
bool parse_unsubscribe(Reader &r, uint16_t &pid, std::vector<std::string> &filters, Properties &props);
bool parse_ack(Reader &r, uint16_t &pid, uint8_t &reason_code, Properties &props);
bool parse_disconnect(Reader &r, uint8_t &reason_code, Properties &props);

// Serialize packets for sending.
std::vector<uint8_t> build_connack(bool session_present, uint8_t reason_code, const Properties &props);
std::vector<uint8_t> build_publish(const Message &msg, uint16_t pid, bool dup,
                                    uint8_t qos, const Properties *extra_props = nullptr);
std::vector<uint8_t> build_ack(PacketType type, uint16_t pid, uint8_t reason_code = 0,
                                const Properties &props = {});
std::vector<uint8_t> build_suback(uint16_t pid, const std::vector<uint8_t> &reason_codes,
                                   const Properties &props = {});
std::vector<uint8_t> build_unsuback(uint16_t pid, const std::vector<uint8_t> &reason_codes,
                                     const Properties &props = {});
std::vector<uint8_t> build_pingresp();
std::vector<uint8_t> build_disconnect(uint8_t reason_code = 0, const Properties &props = {});

// Build a complete packet with fixed header (type+flags byte + remaining length).
std::vector<uint8_t> frame_packet(uint8_t type_flags, const std::vector<uint8_t> &payload);
std::vector<uint8_t> frame_packet(uint8_t type_flags, const uint8_t *payload, size_t len);

// Read fixed header from stream. Returns bytes consumed, 0 if incomplete, -1 on error.
// On success: type, flags, remaining_length are set.
int read_fixed_header(const uint8_t *data, size_t len,
                      uint8_t &type, uint8_t &flags, uint32_t &remaining_length);

// UTF-8 validation per MQTT spec (rejects U+0000, U+D800-DFFF, > U+10FFFF)
bool validate_utf8(const std::string &str);

} // namespace mqtt5

#endif
