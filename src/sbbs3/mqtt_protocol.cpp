#include "mqtt_protocol.h"
#include <cstring>

namespace mqtt5 {

const char *packet_type_name(uint8_t type)
{
	static const char *names[] = {
		nullptr, "CONNECT", "CONNACK", "PUBLISH", "PUBACK",
		"PUBREC", "PUBREL", "PUBCOMP", "SUBSCRIBE", "SUBACK",
		"UNSUBSCRIBE", "UNSUBACK", "PINGREQ", "PINGRESP",
		"DISCONNECT", "AUTH",
	};
	if (type < 1 || type > 15)
		return "UNKNOWN";
	return names[type];
}

static const PropertyDef property_defs[] = {
	{PROP_PAYLOAD_FORMAT,         "Payload Format Indicator",      PT_BYTE,      false},
	{PROP_MESSAGE_EXPIRY,         "Message Expiry Interval",       PT_FOUR_BYTE, false},
	{PROP_CONTENT_TYPE,           "Content Type",                  PT_UTF8,      false},
	{PROP_RESPONSE_TOPIC,         "Response Topic",                PT_UTF8,      false},
	{PROP_CORRELATION_DATA,       "Correlation Data",              PT_BINARY,    false},
	{PROP_SUBSCRIPTION_ID,        "Subscription Identifier",       PT_VBI,       true},
	{PROP_SESSION_EXPIRY,         "Session Expiry Interval",       PT_FOUR_BYTE, false},
	{PROP_ASSIGNED_CLIENT_ID,     "Assigned Client Identifier",    PT_UTF8,      false},
	{PROP_SERVER_KEEP_ALIVE,      "Server Keep Alive",             PT_TWO_BYTE,  false},
	{PROP_AUTH_METHOD,            "Authentication Method",         PT_UTF8,      false},
	{PROP_AUTH_DATA,              "Authentication Data",           PT_BINARY,    false},
	{PROP_REQUEST_PROBLEM_INFO,   "Request Problem Information",   PT_BYTE,      false},
	{PROP_WILL_DELAY,             "Will Delay Interval",           PT_FOUR_BYTE, false},
	{PROP_REQUEST_RESPONSE_INFO,  "Request Response Information",  PT_BYTE,      false},
	{PROP_RESPONSE_INFO,          "Response Information",          PT_BYTE,      false},
	{PROP_SERVER_REFERENCE,       "Server Reference",              PT_UTF8,      false},
	{PROP_REASON_STRING,          "Reason String",                 PT_UTF8,      false},
	{PROP_RECEIVE_MAXIMUM,        "Receive Maximum",               PT_TWO_BYTE,  false},
	{PROP_TOPIC_ALIAS_MAXIMUM,    "Topic Alias Maximum",           PT_TWO_BYTE,  false},
	{PROP_TOPIC_ALIAS,            "Topic Alias",                   PT_TWO_BYTE,  false},
	{PROP_MAXIMUM_QOS,            "Maximum QoS",                   PT_BYTE,      false},
	{PROP_RETAIN_AVAILABLE,       "Retain Available",              PT_BYTE,      false},
	{PROP_USER_PROPERTY,          "User Property",                 PT_UTF8_PAIR, true},
	{PROP_MAXIMUM_PACKET_SIZE,    "Maximum Packet Size",           PT_FOUR_BYTE, false},
	{PROP_WILDCARD_SUB_AVAILABLE, "Wildcard Subscription Available", PT_BYTE,    false},
	{PROP_SUB_ID_AVAILABLE,       "Subscription Identifier Available", PT_BYTE,  false},
	{PROP_SHARED_SUB_AVAILABLE,   "Shared Subscription Available", PT_BYTE,      false},
};

const PropertyDef *property_def(uint8_t id)
{
	for (const auto &d : property_defs)
		if (static_cast<uint8_t>(d.id) == id)
			return &d;
	return nullptr;
}

// ── Properties ──────────────────────────────────────────────────

void Properties::add(uint8_t id, PropertyValue val)
{
	m_entries.push_back({id, std::move(val)});
}

bool Properties::has(uint8_t id) const
{
	for (const auto &e : m_entries)
		if (e.id == id)
			return true;
	return false;
}

uint8_t Properties::get_byte(uint8_t id, uint8_t def) const
{
	for (const auto &e : m_entries)
		if (e.id == id)
			return e.value.as_byte();
	return def;
}

uint16_t Properties::get_u16(uint8_t id, uint16_t def) const
{
	for (const auto &e : m_entries)
		if (e.id == id)
			return e.value.as_u16();
	return def;
}

uint32_t Properties::get_u32(uint8_t id, uint32_t def) const
{
	for (const auto &e : m_entries)
		if (e.id == id)
			return e.value.as_u32();
	return def;
}

const std::string *Properties::get_string(uint8_t id) const
{
	for (const auto &e : m_entries)
		if (e.id == id && e.value.type == PT_UTF8)
			return &e.value.str_val;
	return nullptr;
}

const std::vector<uint8_t> *Properties::get_binary(uint8_t id) const
{
	for (const auto &e : m_entries)
		if (e.id == id && e.value.type == PT_BINARY)
			return &e.value.bin_val;
	return nullptr;
}

std::vector<uint32_t> Properties::get_all_u32(uint8_t id) const
{
	std::vector<uint32_t> result;
	for (const auto &e : m_entries)
		if (e.id == id)
			result.push_back(e.value.as_u32());
	return result;
}

std::vector<StringPair> Properties::get_all_pairs(uint8_t id) const
{
	std::vector<StringPair> result;
	for (const auto &e : m_entries)
		if (e.id == id && e.value.type == PT_UTF8_PAIR)
			result.push_back(e.value.as_pair());
	return result;
}

// ── Reader ──────────────────────────────────────────────────────

bool Reader::read_byte(uint8_t &out)
{
	if (m_pos >= m_len) return false;
	out = m_data[m_pos++];
	return true;
}

bool Reader::read_u16(uint16_t &out)
{
	if (m_pos + 2 > m_len) return false;
	out = (uint16_t)(m_data[m_pos] << 8) | m_data[m_pos + 1];
	m_pos += 2;
	return true;
}

bool Reader::read_u32(uint32_t &out)
{
	if (m_pos + 4 > m_len) return false;
	out = (uint32_t)m_data[m_pos] << 24 | (uint32_t)m_data[m_pos+1] << 16
	    | (uint32_t)m_data[m_pos+2] << 8 | m_data[m_pos+3];
	m_pos += 4;
	return true;
}

bool Reader::read_vbi(uint32_t &out)
{
	out = 0;
	for (int i = 0; i < 4; ++i) {
		if (m_pos >= m_len) return false;
		uint8_t b = m_data[m_pos++];
		out |= (uint32_t)(b & 0x7F) << (7 * i);
		if (!(b & 0x80))
			return true;
	}
	return false;
}

bool Reader::read_utf8(std::string &out)
{
	uint16_t len;
	if (!read_u16(len)) return false;
	if (m_pos + len > m_len) return false;
	out.assign(reinterpret_cast<const char *>(m_data + m_pos), len);
	m_pos += len;
	return true;
}

bool Reader::read_binary(std::vector<uint8_t> &out)
{
	uint16_t len;
	if (!read_u16(len)) return false;
	if (m_pos + len > m_len) return false;
	out.assign(m_data + m_pos, m_data + m_pos + len);
	m_pos += len;
	return true;
}

bool Reader::read_utf8_pair(StringPair &out)
{
	return read_utf8(out.first) && read_utf8(out.second);
}

bool Reader::read_bytes(uint8_t *out, size_t count)
{
	if (m_pos + count > m_len) return false;
	memcpy(out, m_data + m_pos, count);
	m_pos += count;
	return true;
}

bool Reader::read_bytes(std::vector<uint8_t> &out, size_t count)
{
	if (m_pos + count > m_len) return false;
	out.assign(m_data + m_pos, m_data + m_pos + count);
	m_pos += count;
	return true;
}

bool Reader::skip(size_t count)
{
	if (m_pos + count > m_len) return false;
	m_pos += count;
	return true;
}

bool Reader::read_properties(Properties &props)
{
	uint32_t prop_len;
	if (!read_vbi(prop_len)) return false;
	if (m_pos + prop_len > m_len) return false;

	size_t end = m_pos + prop_len;
	while (m_pos < end) {
		uint8_t id;
		if (!read_byte(id)) return false;

		const PropertyDef *def = property_def(id);
		if (!def) return false;

		PropertyValue val;
		switch (def->type) {
		case PT_BYTE: {
			uint8_t v;
			if (!read_byte(v)) return false;
			val = PropertyValue::from_byte(v);
			break;
		}
		case PT_TWO_BYTE: {
			uint16_t v;
			if (!read_u16(v)) return false;
			val = PropertyValue::from_u16(v);
			break;
		}
		case PT_FOUR_BYTE: {
			uint32_t v;
			if (!read_u32(v)) return false;
			val = PropertyValue::from_u32(v);
			break;
		}
		case PT_VBI: {
			uint32_t v;
			if (!read_vbi(v)) return false;
			val = PropertyValue::from_vbi(v);
			break;
		}
		case PT_UTF8: {
			std::string v;
			if (!read_utf8(v)) return false;
			val = PropertyValue::from_utf8(std::move(v));
			break;
		}
		case PT_BINARY: {
			std::vector<uint8_t> v;
			if (!read_binary(v)) return false;
			val = PropertyValue::from_binary(std::move(v));
			break;
		}
		case PT_UTF8_PAIR: {
			StringPair v;
			if (!read_utf8_pair(v)) return false;
			val = PropertyValue::from_pair(std::move(v));
			break;
		}
		}
		props.add(id, std::move(val));
	}
	return m_pos == end;
}

// ── Writer ──────────────────────────────────────────────────────

void Writer::write_byte(uint8_t val)
{
	m_buf.push_back(val);
}

void Writer::write_u16(uint16_t val)
{
	m_buf.push_back(val >> 8);
	m_buf.push_back(val & 0xFF);
}

void Writer::write_u32(uint32_t val)
{
	m_buf.push_back((val >> 24) & 0xFF);
	m_buf.push_back((val >> 16) & 0xFF);
	m_buf.push_back((val >> 8) & 0xFF);
	m_buf.push_back(val & 0xFF);
}

void Writer::write_vbi(uint32_t val)
{
	do {
		uint8_t b = val & 0x7F;
		val >>= 7;
		if (val > 0)
			b |= 0x80;
		m_buf.push_back(b);
	} while (val > 0);
}

void Writer::write_utf8(const std::string &str)
{
	write_u16(static_cast<uint16_t>(str.size()));
	m_buf.insert(m_buf.end(), str.begin(), str.end());
}

void Writer::write_binary(const std::vector<uint8_t> &data)
{
	write_u16(static_cast<uint16_t>(data.size()));
	m_buf.insert(m_buf.end(), data.begin(), data.end());
}

void Writer::write_utf8_pair(const StringPair &pair)
{
	write_utf8(pair.first);
	write_utf8(pair.second);
}

void Writer::write_bytes(const uint8_t *data, size_t len)
{
	m_buf.insert(m_buf.end(), data, data + len);
}

void Writer::write_properties(const Properties &props)
{
	if (props.empty()) {
		write_vbi(0);
		return;
	}
	Writer tmp;
	for (const auto &e : props.entries()) {
		const PropertyDef *def = property_def(e.id);
		if (!def) continue;
		tmp.write_byte(e.id);
		switch (def->type) {
		case PT_BYTE:
			tmp.write_byte(e.value.as_byte());
			break;
		case PT_TWO_BYTE:
			tmp.write_u16(e.value.as_u16());
			break;
		case PT_FOUR_BYTE:
			tmp.write_u32(e.value.as_u32());
			break;
		case PT_VBI:
			tmp.write_vbi(e.value.as_u32());
			break;
		case PT_UTF8:
			tmp.write_utf8(e.value.str_val);
			break;
		case PT_BINARY:
			tmp.write_binary(e.value.bin_val);
			break;
		case PT_UTF8_PAIR:
			tmp.write_utf8_pair(e.value.as_pair());
			break;
		}
	}
	write_vbi(static_cast<uint32_t>(tmp.size()));
	m_buf.insert(m_buf.end(), tmp.data().begin(), tmp.data().end());
}

// ── Fixed header ────────────────────────────────────────────────

int read_fixed_header(const uint8_t *data, size_t len,
                      uint8_t &type, uint8_t &flags, uint32_t &remaining_length)
{
	if (len < 2) return 0;
	type = data[0] >> 4;
	flags = data[0] & 0x0F;

	remaining_length = 0;
	size_t pos = 1;
	for (int i = 0; i < 4; ++i) {
		if (pos >= len) return 0;
		uint8_t b = data[pos++];
		remaining_length |= (uint32_t)(b & 0x7F) << (7 * i);
		if (!(b & 0x80))
			return static_cast<int>(pos);
	}
	return -1;
}

std::vector<uint8_t> frame_packet(uint8_t type_flags, const uint8_t *payload, size_t len)
{
	Writer w;
	w.write_byte(type_flags);
	w.write_vbi(static_cast<uint32_t>(len));
	if (len > 0)
		w.write_bytes(payload, len);
	return w.take();
}

std::vector<uint8_t> frame_packet(uint8_t type_flags, const std::vector<uint8_t> &payload)
{
	return frame_packet(type_flags, payload.data(), payload.size());
}

// ── UTF-8 validation ────────────────────────────────────────────

bool validate_utf8(const std::string &str)
{
	const auto *p = reinterpret_cast<const uint8_t *>(str.data());
	const auto *end = p + str.size();

	while (p < end) {
		uint32_t cp;
		int trail;
		uint8_t b = *p++;
		if (b < 0x80) {
			if (b == 0) return false;
			continue;
		} else if ((b & 0xE0) == 0xC0) {
			cp = b & 0x1F;
			trail = 1;
			if (cp < 2) return false;
		} else if ((b & 0xF0) == 0xE0) {
			cp = b & 0x0F;
			trail = 2;
		} else if ((b & 0xF8) == 0xF0) {
			cp = b & 0x07;
			trail = 3;
		} else {
			return false;
		}
		if (p + trail > end) return false;
		for (int i = 0; i < trail; ++i) {
			b = *p++;
			if ((b & 0xC0) != 0x80) return false;
			cp = (cp << 6) | (b & 0x3F);
		}
		if (cp > 0x10FFFF) return false;
		if (cp >= 0xD800 && cp <= 0xDFFF) return false;
		if (trail == 1 && cp < 0x80) return false;
		if (trail == 2 && cp < 0x800) return false;
		if (trail == 3 && cp < 0x10000) return false;
	}
	return true;
}

// ── Packet parsers ──────────────────────────────────────────────

bool parse_connect(Reader &r, ConnectData &out, uint8_t &reason_code)
{
	reason_code = 0x81; // Malformed Packet default

	if (!r.read_utf8(out.protocol_name)) return false;
	if (out.protocol_name != "MQTT") {
		reason_code = 0x84; // Unsupported Protocol Version
		return false;
	}
	if (!r.read_byte(out.protocol_version)) return false;
	if (out.protocol_version != 5) {
		reason_code = 0x84;
		return false;
	}
	if (!r.read_byte(out.connect_flags)) return false;
	if (!r.read_u16(out.keep_alive)) return false;
	if (!r.read_properties(out.props)) return false;
	if (!r.read_utf8(out.client_id)) return false;

	if (out.will_flag()) {
		if (!r.read_properties(out.will_props)) return false;
		if (!r.read_utf8(out.will_topic)) return false;
		if (!r.read_binary(out.will_payload)) return false;
	}
	if (out.has_username()) {
		if (!r.read_utf8(out.username)) return false;
	}
	if (out.has_password()) {
		if (!r.read_utf8(out.password)) return false;
	}

	reason_code = 0;
	return true;
}

bool parse_publish(Reader &r, uint8_t flags, std::shared_ptr<Message> &out, uint16_t &pid)
{
	out = std::make_shared<Message>();
	out->type = PUBLISH;
	out->flags = flags;
	out->created_at = time(nullptr);
	pid = 0;

	if (!r.read_utf8(out->topic)) return false;

	uint8_t qos = (flags >> 1) & 3;
	if (qos > 0) {
		if (!r.read_u16(pid)) return false;
	}
	if (!r.read_properties(out->props)) return false;

	size_t payload_len = r.remaining();
	if (payload_len > 0) {
		if (!r.read_bytes(out->payload, payload_len)) return false;
	}
	return true;
}

bool parse_subscribe(Reader &r, uint16_t &pid, std::vector<TopicFilter> &filters, Properties &props)
{
	if (!r.read_u16(pid)) return false;
	if (!r.read_properties(props)) return false;

	while (!r.at_end()) {
		TopicFilter tf;
		if (!r.read_utf8(tf.filter)) return false;
		if (!r.read_byte(tf.options)) return false;
		filters.push_back(std::move(tf));
	}
	return !filters.empty();
}

bool parse_unsubscribe(Reader &r, uint16_t &pid, std::vector<std::string> &filters, Properties &props)
{
	if (!r.read_u16(pid)) return false;
	if (!r.read_properties(props)) return false;

	while (!r.at_end()) {
		std::string filter;
		if (!r.read_utf8(filter)) return false;
		filters.push_back(std::move(filter));
	}
	return !filters.empty();
}

bool parse_ack(Reader &r, uint16_t &pid, uint8_t &reason_code, Properties &props)
{
	if (!r.read_u16(pid)) return false;
	reason_code = 0;
	if (r.remaining() > 0) {
		if (!r.read_byte(reason_code)) return false;
		if (r.remaining() > 0) {
			if (!r.read_properties(props)) return false;
		}
	}
	return true;
}

bool parse_disconnect(Reader &r, uint8_t &reason_code, Properties &props)
{
	reason_code = 0;
	if (r.remaining() > 0) {
		if (!r.read_byte(reason_code)) return false;
		if (r.remaining() > 0) {
			if (!r.read_properties(props)) return false;
		}
	}
	return true;
}

// ── Packet builders ─────────────────────────────────────────────

std::vector<uint8_t> build_connack(bool session_present, uint8_t reason_code, const Properties &props)
{
	Writer w;
	w.write_byte(session_present ? 1 : 0);
	w.write_byte(reason_code);
	w.write_properties(props);
	return frame_packet((CONNACK << 4), w.take());
}

std::vector<uint8_t> build_publish(const Message &msg, uint16_t pid, bool dup,
                                    uint8_t qos, const Properties *extra_props)
{
	uint8_t flags = (qos << 1);
	if (dup) flags |= 0x08;
	if (msg.retain()) flags |= 0x01;

	Writer w;
	w.write_utf8(msg.topic);
	if (qos > 0)
		w.write_u16(pid);

	if (extra_props && !extra_props->empty()) {
		Properties merged;
		for (const auto &e : msg.props.entries())
			merged.add(e.id, e.value);
		for (const auto &e : extra_props->entries())
			merged.add(e.id, e.value);
		w.write_properties(merged);
	} else {
		w.write_properties(msg.props);
	}

	w.write_bytes(msg.payload.data(), msg.payload.size());
	return frame_packet((PUBLISH << 4) | flags, w.take());
}

static std::vector<uint8_t> build_simple_ack(PacketType type, uint8_t required_flags,
                                              uint16_t pid, uint8_t reason_code,
                                              const Properties &props)
{
	Writer w;
	w.write_u16(pid);
	if (reason_code != 0 || !props.empty()) {
		w.write_byte(reason_code);
		if (!props.empty())
			w.write_properties(props);
	}
	return frame_packet((type << 4) | required_flags, w.take());
}

std::vector<uint8_t> build_ack(PacketType type, uint16_t pid, uint8_t reason_code,
                                const Properties &props)
{
	uint8_t required_flags = 0;
	if (type == PUBREL || type == SUBSCRIBE || type == UNSUBSCRIBE)
		required_flags = 0x02;
	return build_simple_ack(type, required_flags, pid, reason_code, props);
}

std::vector<uint8_t> build_suback(uint16_t pid, const std::vector<uint8_t> &reason_codes,
                                   const Properties &props)
{
	Writer w;
	w.write_u16(pid);
	w.write_properties(props);
	for (uint8_t rc : reason_codes)
		w.write_byte(rc);
	return frame_packet((SUBACK << 4), w.take());
}

std::vector<uint8_t> build_unsuback(uint16_t pid, const std::vector<uint8_t> &reason_codes,
                                     const Properties &props)
{
	Writer w;
	w.write_u16(pid);
	w.write_properties(props);
	for (uint8_t rc : reason_codes)
		w.write_byte(rc);
	return frame_packet((UNSUBACK << 4), w.take());
}

std::vector<uint8_t> build_pingresp()
{
	return frame_packet((PINGRESP << 4), nullptr, 0);
}

std::vector<uint8_t> build_disconnect(uint8_t reason_code, const Properties &props)
{
	if (reason_code == 0 && props.empty())
		return frame_packet((DISCONNECT << 4), nullptr, 0);
	Writer w;
	w.write_byte(reason_code);
	if (!props.empty())
		w.write_properties(props);
	return frame_packet((DISCONNECT << 4), w.take());
}

} // namespace mqtt5
