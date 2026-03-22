// RFC-4253: SSH Transport Layer Protocol

#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include "deucessh.h"

typedef struct deuce_ssh_transport_global_config {
	atomic_bool used;
	const char *software_version;
	const char *version_comment;

	int (*tx)(uint8_t *buf, size_t bufsz, atomic_bool *terminate, void *cbdata);
	int (*rx)(uint8_t *buf, size_t bufsz, atomic_bool *terminate, void *cbdata);
	int (*rx_line)(uint8_t *buf, size_t bufsz, size_t *bytes_received, atomic_bool *terminate, void *cbdata);
	int (*extra_line_cb)(uint8_t *buf, size_t bufsz, void *cbdata);

	size_t kex_entries;
	deuce_ssh_kex kex_head;
	deuce_ssh_kex kex_tail;

	size_t key_algo_entries;
	deuce_ssh_key_algo key_algo_head;
	deuce_ssh_key_algo key_algo_tail;

	size_t enc_entries;
	deuce_ssh_enc enc_head;
	deuce_ssh_enc enc_tail;

	size_t mac_entries;
	deuce_ssh_mac mac_head;
	deuce_ssh_mac mac_tail;

	size_t comp_entries;
	deuce_ssh_comp comp_head;
	deuce_ssh_comp comp_tail;

	size_t lang_entries;
	deuce_ssh_language lang_head;
	deuce_ssh_language lang_tail;
} *deuce_ssh_transport_global_config;

static const char * const sw_ver = "DeuceSSH-0.0";
static struct deuce_ssh_transport_global_config gconf;

/* ================================================================
 * Version exchange (RFC 4253 s4.2)
 * ================================================================ */

static inline bool
has_nulls(uint8_t *buf, size_t buflen)
{
	return memchr(buf, 0, buflen) != NULL;
}

static inline bool
missing_crlf(uint8_t *buf, size_t buflen)
{
	if (buflen < 2)
		return true;
	return (buf[buflen - 1] != '\n' || buf[buflen - 2] != '\r');
}

static inline bool
is_version_line(uint8_t *buf, size_t buflen)
{
	if (buflen < 4)
		return false;
	return (buf[0] == 'S' && buf[1] == 'S' && buf[2] == 'H' && buf[3] == '-');
}

static inline bool
is_20(uint8_t *buf, size_t buflen)
{
	if (buflen < 8)
		return false;
	return (buf[4] == '2' && buf[5] == '.' && buf[6] == '0' && buf[7] == '-');
}

static int
version_rx(deuce_ssh_session sess)
{
	size_t received;
	int res;

	while (!sess->terminate) {
		res = gconf.rx_line(sess->trans.rx_packet, sess->trans.packet_buf_sz - 1, &received, &sess->terminate, sess->rx_line_cbdata);
		if (res < 0) {
			sess->terminate = true;
			return res;
		}
		if (is_version_line(sess->trans.rx_packet, received)) {
			if (received > 255 || has_nulls(sess->trans.rx_packet, received) || missing_crlf(sess->trans.rx_packet, received) || !is_20(sess->trans.rx_packet, received)) {
				sess->terminate = true;
				return DEUCE_SSH_ERROR_INVALID;
			}
			sess->trans.remote_id_str_sz = received - 2;
			memcpy(sess->trans.remote_id_str, sess->trans.rx_packet, sess->trans.remote_id_str_sz);
			sess->trans.remote_id_str[sess->trans.remote_id_str_sz] = 0;
			return 0;
		}
		if (gconf.extra_line_cb) {
			sess->trans.rx_packet[received] = 0;
			res = gconf.extra_line_cb(sess->trans.rx_packet, received, sess->extra_line_cbdata);
			if (res < 0) {
				sess->terminate = true;
				return res;
			}
		}
	}
	return DEUCE_SSH_ERROR_TERMINATED;
}

static int
version_tx(deuce_ssh_session sess)
{
	int res;
	size_t sz = 0;

	memcpy(sess->trans.tx_packet, "SSH-2.0-", 8);
	sz += 8;
	size_t asz = strlen(gconf.software_version);
	if (sz + asz + 2 > 255)
		return DEUCE_SSH_ERROR_TOOLONG;
	memcpy(&sess->trans.tx_packet[sz], gconf.software_version, asz);
	sz += asz;
	if (gconf.version_comment != NULL) {
		memcpy(&sess->trans.tx_packet[sz], " ", 1);
		sz += 1;
		asz = strlen(gconf.version_comment);
		if (sz + asz + 2 > 255)
			return DEUCE_SSH_ERROR_TOOLONG;
		memcpy(&sess->trans.tx_packet[sz], gconf.version_comment, asz);
		sz += asz;
	}
	memcpy(sess->trans.id_str, sess->trans.tx_packet, sz);
	sess->trans.id_str_sz = sz;
	sess->trans.id_str[sz] = 0;
	memcpy(&sess->trans.tx_packet[sz], "\r\n", 2);
	sz += 2;
	res = gconf.tx(sess->trans.tx_packet, sz, &sess->terminate, sess->tx_cbdata);
	if (res < 0) {
		sess->terminate = true;
		return res;
	}
	return 0;
}

const char *
deuce_ssh_transport_get_remote_version(deuce_ssh_session sess)
{
	return sess->trans.remote_id_str;
}

const char *
deuce_ssh_transport_get_kex_name(deuce_ssh_session sess)
{
	return sess->trans.kex_selected ? sess->trans.kex_selected->name : NULL;
}

const char *
deuce_ssh_transport_get_hostkey_name(deuce_ssh_session sess)
{
	return sess->trans.key_algo_selected ? sess->trans.key_algo_selected->name : NULL;
}

const char *
deuce_ssh_transport_get_enc_name(deuce_ssh_session sess)
{
	return sess->trans.enc_c2s_selected ? sess->trans.enc_c2s_selected->name : NULL;
}

const char *
deuce_ssh_transport_get_mac_name(deuce_ssh_session sess)
{
	return sess->trans.mac_c2s_selected ? sess->trans.mac_c2s_selected->name : NULL;
}

int
deuce_ssh_transport_version_exchange(deuce_ssh_session sess)
{
	int res = version_tx(sess);
	if (res < 0)
		return res;
	return version_rx(sess);
}

/* ================================================================
 * SSH_MSG_DISCONNECT (RFC 4253 s11.1)
 * ================================================================ */

int
deuce_ssh_transport_disconnect(deuce_ssh_session sess,
    uint32_t reason, const char *desc)
{
	size_t dlen = strlen(desc);
	if (dlen > 230)
		dlen = 230;
	uint8_t msg[256];
	size_t pos = 0;

	msg[pos++] = SSH_MSG_DISCONNECT;
	deuce_ssh_serialize_uint32(reason, msg, sizeof(msg), &pos);
	deuce_ssh_serialize_uint32((uint32_t)dlen, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], desc, dlen);
	pos += dlen;
	deuce_ssh_serialize_uint32(0, msg, sizeof(msg), &pos); /* language tag */

	deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
	sess->terminate = true;
	return 0;
}

/* ================================================================
 * Binary Packet Protocol (RFC 4253 s6)
 * ================================================================ */

static size_t
tx_block_size(deuce_ssh_session sess)
{
	deuce_ssh_enc enc;
	void *cbd;
	if (sess->trans.client) {
		enc = sess->trans.enc_c2s_selected;
		cbd = sess->trans.enc_c2s_ctx;
	}
	else {
		enc = sess->trans.enc_s2c_selected;
		cbd = sess->trans.enc_s2c_ctx;
	}
	if (enc == NULL || cbd == NULL || enc->blocksize < 8)
		return 8;
	return enc->blocksize;
}

static size_t
rx_block_size(deuce_ssh_session sess)
{
	deuce_ssh_enc enc;
	void *cbd;
	if (sess->trans.client) {
		enc = sess->trans.enc_s2c_selected;
		cbd = sess->trans.enc_s2c_ctx;
	}
	else {
		enc = sess->trans.enc_c2s_selected;
		cbd = sess->trans.enc_c2s_ctx;
	}
	if (enc == NULL || cbd == NULL || enc->blocksize < 8)
		return 8;
	return enc->blocksize;
}

static uint16_t
tx_mac_size(deuce_ssh_session sess)
{
	deuce_ssh_mac mac;
	void *cbd;
	if (sess->trans.client) {
		mac = sess->trans.mac_c2s_selected;
		cbd = sess->trans.mac_c2s_ctx;
	}
	else {
		mac = sess->trans.mac_s2c_selected;
		cbd = sess->trans.mac_s2c_ctx;
	}
	if (mac == NULL || cbd == NULL)
		return 0;
	return mac->digest_size;
}

static uint16_t
rx_mac_size(deuce_ssh_session sess)
{
	deuce_ssh_mac mac;
	void *cbd;
	if (sess->trans.client) {
		mac = sess->trans.mac_s2c_selected;
		cbd = sess->trans.mac_s2c_ctx;
	}
	else {
		mac = sess->trans.mac_c2s_selected;
		cbd = sess->trans.mac_c2s_ctx;
	}
	if (mac == NULL || cbd == NULL)
		return 0;
	return mac->digest_size;
}

int
deuce_ssh_transport_send_packet(deuce_ssh_session sess,
    const uint8_t *payload, size_t payload_len, uint32_t *seq_out)
{
	int ret;
	mtx_lock(&sess->trans.tx_mtx);
	size_t bs = tx_block_size(sess);

	size_t padding_len = bs - ((5 + payload_len) % bs);
	if (padding_len < 4)
		padding_len += bs;

	uint32_t packet_length = (uint32_t)(1 + payload_len + padding_len);
	size_t total = 4 + packet_length;

	if (total > sess->trans.packet_buf_sz) {
		ret = DEUCE_SSH_ERROR_TOOLONG;
		goto tx_done;
	}

	size_t pos = 0;
	deuce_ssh_serialize_uint32(packet_length, sess->trans.tx_packet, sess->trans.packet_buf_sz, &pos);
	sess->trans.tx_packet[pos++] = (uint8_t)padding_len;
	memcpy(&sess->trans.tx_packet[pos], payload, payload_len);
	pos += payload_len;
	if (RAND_bytes(&sess->trans.tx_packet[pos], (int)padding_len) != 1) {
		ret = DEUCE_SSH_ERROR_INIT;
		goto tx_done;
	}
	pos += padding_len;

	/* MAC: mac(key, seq || unencrypted_packet) */
	uint16_t mac_len = tx_mac_size(sess);
	if (mac_len > 0) {
		uint8_t *mac_input = sess->trans.tx_mac_scratch;
		size_t mi_pos = 0;
		deuce_ssh_serialize_uint32(sess->trans.tx_seq, mac_input, 4 + sess->trans.packet_buf_sz, &mi_pos);
		memcpy(&mac_input[mi_pos], sess->trans.tx_packet, total);
		mi_pos += total;

		deuce_ssh_mac mac;
		uint8_t *mac_key;
		if (sess->trans.client) {
			mac = sess->trans.mac_c2s_selected;
			mac_key = (uint8_t *)sess->trans.mac_c2s_ctx;
		}
		else {
			mac = sess->trans.mac_s2c_selected;
			mac_key = (uint8_t *)sess->trans.mac_s2c_ctx;
		}
		ret = mac->generate(mac_key, mac_input, mi_pos, &sess->trans.tx_packet[pos]);
		if (ret < 0)
			goto tx_done;
		pos += mac_len;
	}

	/* Encrypt (the packet, not the MAC) */
	{
		deuce_ssh_enc_ctx *enc_ctx;
		deuce_ssh_enc enc;
		if (sess->trans.client) {
			enc = sess->trans.enc_c2s_selected;
			enc_ctx = sess->trans.enc_c2s_ctx;
		}
		else {
			enc = sess->trans.enc_s2c_selected;
			enc_ctx = sess->trans.enc_s2c_ctx;
		}
		if (enc != NULL && enc->encrypt != NULL && enc_ctx != NULL) {
			ret = enc->encrypt(sess->trans.tx_packet, total, enc_ctx);
			if (ret < 0)
				goto tx_done;
		}
	}

	ret = gconf.tx(sess->trans.tx_packet, pos, &sess->terminate, sess->tx_cbdata);
	if (ret == 0) {
		if (seq_out != NULL)
			*seq_out = sess->trans.tx_seq;
		sess->trans.tx_seq++;
	}

tx_done:
	mtx_unlock(&sess->trans.tx_mtx);
	return ret;
}

/*
 * Receive a single raw packet.  Does NOT handle transport messages
 * (IGNORE, DEBUG, UNIMPLEMENTED, DISCONNECT) — that's done by the
 * public recv_packet wrapper below.
 */
static int
recv_packet_raw(deuce_ssh_session sess,
    uint8_t *msg_type, uint8_t **payload, size_t *payload_len)
{
	int ret;
	mtx_lock(&sess->trans.rx_mtx);
	size_t bs = rx_block_size(sess);

	ret = gconf.rx(sess->trans.rx_packet, bs, &sess->terminate, sess->rx_cbdata);
	if (ret < 0)
		goto rx_done;

	/* Decrypt first block */
	deuce_ssh_enc enc;
	deuce_ssh_enc_ctx *enc_ctx;
	if (sess->trans.client) {
		enc = sess->trans.enc_s2c_selected;
		enc_ctx = sess->trans.enc_s2c_ctx;
	}
	else {
		enc = sess->trans.enc_c2s_selected;
		enc_ctx = sess->trans.enc_c2s_ctx;
	}
	if (enc != NULL && enc->decrypt != NULL && enc_ctx != NULL) {
		ret = enc->decrypt(sess->trans.rx_packet, bs, enc_ctx);
		if (ret < 0)
			goto rx_done;
	}

	uint32_t packet_length;
	deuce_ssh_parse_uint32(sess->trans.rx_packet, 4, &packet_length);

	if (packet_length < 2 || packet_length + 4 > sess->trans.packet_buf_sz) {
		ret = DEUCE_SSH_ERROR_TOOLONG;
		goto rx_done;
	}

	if (packet_length + 4 < bs) {
		ret = DEUCE_SSH_ERROR_PARSE;
		goto rx_done;
	}

	size_t remaining = packet_length + 4 - bs;
	if (remaining > 0) {
		ret = gconf.rx(&sess->trans.rx_packet[bs], remaining, &sess->terminate, sess->rx_cbdata);
		if (ret < 0)
			goto rx_done;
		if (enc != NULL && enc->decrypt != NULL && enc_ctx != NULL) {
			ret = enc->decrypt(&sess->trans.rx_packet[bs], remaining, enc_ctx);
			if (ret < 0)
				goto rx_done;
		}
	}

	/* Verify MAC */
	uint16_t mac_len = rx_mac_size(sess);
	if (mac_len > 0) {
		uint8_t received_mac[64];
		if (mac_len > sizeof(received_mac)) {
			ret = DEUCE_SSH_ERROR_TOOLONG;
			goto rx_done;
		}
		ret = gconf.rx(received_mac, mac_len, &sess->terminate, sess->rx_cbdata);
		if (ret < 0)
			goto rx_done;

		uint8_t *mac_input = sess->trans.rx_mac_scratch;
		size_t mi_pos = 0;
		deuce_ssh_serialize_uint32(sess->trans.rx_seq, mac_input, 4 + sess->trans.packet_buf_sz, &mi_pos);
		memcpy(&mac_input[mi_pos], sess->trans.rx_packet, packet_length + 4);
		mi_pos += packet_length + 4;

		uint8_t computed_mac[64];
		deuce_ssh_mac mac;
		uint8_t *mac_key;
		if (sess->trans.client) {
			mac = sess->trans.mac_s2c_selected;
			mac_key = (uint8_t *)sess->trans.mac_s2c_ctx;
		}
		else {
			mac = sess->trans.mac_c2s_selected;
			mac_key = (uint8_t *)sess->trans.mac_c2s_ctx;
		}
		ret = mac->generate(mac_key, mac_input, mi_pos, computed_mac);
		if (ret < 0)
			goto rx_done;
		if (memcmp(received_mac, computed_mac, mac_len) != 0) {
			deuce_ssh_transport_disconnect(sess,
			    SSH_DISCONNECT_MAC_ERROR, "MAC verification failed");
			ret = DEUCE_SSH_ERROR_INVALID;
			goto rx_done;
		}
	}

	uint8_t padding_length = sess->trans.rx_packet[4];
	if (padding_length + 1 >= packet_length) {
		ret = DEUCE_SSH_ERROR_PARSE;
		goto rx_done;
	}
	*payload = &sess->trans.rx_packet[5];
	*payload_len = packet_length - padding_length - 1;
	*msg_type = sess->trans.rx_packet[5];

	sess->trans.rx_seq++;
	ret = 0;

rx_done:
	mtx_unlock(&sess->trans.rx_mtx);
	return ret;
}

/*
 * Public recv_packet: transparently handles transport messages
 * per RFC 4253 s11:
 *   SSH_MSG_IGNORE (2)        — silently discarded
 *   SSH_MSG_DEBUG (4)         — invokes debug callback if set, then discarded
 *   SSH_MSG_UNIMPLEMENTED (3) — silently discarded (TODO: needs design work)
 *   SSH_MSG_DISCONNECT (1)    — sets terminate, returns error
 */
int
deuce_ssh_transport_recv_packet(deuce_ssh_session sess,
    uint8_t *msg_type, uint8_t **payload, size_t *payload_len)
{
	for (;;) {
		int res = recv_packet_raw(sess, msg_type, payload, payload_len);
		if (res < 0)
			return res;

		switch (*msg_type) {
		case SSH_MSG_DISCONNECT:
			sess->terminate = true;
			return DEUCE_SSH_ERROR_TERMINATED;
		case SSH_MSG_IGNORE:
			continue;
		case SSH_MSG_DEBUG:
			if (sess->debug_cb != NULL && *payload_len >= 2) {
				bool always_display = (*payload)[1];
				/* Parse: msg_type(1) + bool(1) + string message */
				size_t dpos = 2;
				uint32_t msg_len = 0;
				if (dpos + 4 <= *payload_len) {
					deuce_ssh_parse_uint32(&(*payload)[dpos], *payload_len - dpos, &msg_len);
					dpos += 4;
					if (dpos + msg_len > *payload_len)
						msg_len = 0;
				}
				sess->debug_cb(always_display,
				    msg_len > 0 ? &(*payload)[dpos] : NULL,
				    msg_len, sess->debug_cbdata);
			}
			continue;
		case SSH_MSG_UNIMPLEMENTED:
			if (sess->unimplemented_cb != NULL && *payload_len >= 5) {
				uint32_t rejected_seq;
				deuce_ssh_parse_uint32(&(*payload)[1], *payload_len - 1, &rejected_seq);
				sess->unimplemented_cb(rejected_seq, sess->unimplemented_cbdata);
			}
			continue;
		default:
			return 0;
		}
	}
}

/* ================================================================
 * KEXINIT (RFC 4253 s7.1)
 * ================================================================ */

static size_t
build_namelist(void *head, size_t name_offset, char *buf, size_t bufsz)
{
	size_t pos = 0;
	bool first = true;
	for (uint8_t *node = head; node != NULL; ) {
		const char *name = (const char *)(node + name_offset);
		size_t nlen = strlen(name);
		if (!first) {
			if (pos + 1 >= bufsz)
				break;
			buf[pos++] = ',';
		}
		if (pos + nlen >= bufsz)
			break;
		memcpy(&buf[pos], name, nlen);
		pos += nlen;
		first = false;
		memcpy(&node, node, sizeof(void *));
	}
	buf[pos] = 0;
	return pos;
}

/*
 * Extract the first name from a comma-separated list into buf.
 * Returns the length of the name.
 */
static size_t
first_name(const char *list, char *buf, size_t bufsz)
{
	const char *comma = strchr(list, ',');
	size_t len = comma ? (size_t)(comma - list) : strlen(list);
	if (len >= bufsz)
		len = bufsz - 1;
	memcpy(buf, list, len);
	buf[len] = 0;
	return len;
}

static void *
negotiate_algo(const char *client_list, const char *server_list,
    void *head, size_t name_offset)
{
	const char *cp = client_list;
	while (*cp) {
		const char *comma = strchr(cp, ',');
		size_t clen = comma ? (size_t)(comma - cp) : strlen(cp);

		const char *sp = server_list;
		while (*sp) {
			const char *sc = strchr(sp, ',');
			size_t slen = sc ? (size_t)(sc - sp) : strlen(sp);
			if (clen == slen && memcmp(cp, sp, clen) == 0) {
				for (uint8_t *node = head; node != NULL; ) {
					const char *name = (const char *)(node + name_offset);
					if (strlen(name) == clen && memcmp(name, cp, clen) == 0)
						return node;
					memcpy(&node, node, sizeof(void *));
				}
			}
			sp += slen;
			if (sc)
				sp++;
			else
				break;
		}

		cp += clen;
		if (comma)
			cp++;
		else
			break;
	}
	return NULL;
}

static void
serialize_namelist_from_str(const char *str, uint8_t *buf, size_t bufsz, size_t *pos)
{
	uint32_t len = (uint32_t)strlen(str);
	deuce_ssh_serialize_uint32(len, buf, bufsz, pos);
	memcpy(&buf[*pos], str, len);
	*pos += len;
}

int
deuce_ssh_transport_kexinit(deuce_ssh_session sess)
{
	size_t kexinit_bufsz = sess->trans.packet_buf_sz;
	uint8_t *kexinit = malloc(kexinit_bufsz);
	if (kexinit == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	size_t pos = 0;

	kexinit[pos++] = SSH_MSG_KEXINIT;

	if (RAND_bytes(&kexinit[pos], 16) != 1)
		return DEUCE_SSH_ERROR_INIT;
	pos += 16;

	char namelist[1024];
	size_t noff;

	noff = offsetof(struct deuce_ssh_kex_s, name);
	build_namelist(gconf.kex_head, noff, namelist, sizeof(namelist));
	serialize_namelist_from_str(namelist, kexinit, sess->trans.packet_buf_sz, &pos);

	noff = offsetof(struct deuce_ssh_key_algo_s, name);
	build_namelist(gconf.key_algo_head, noff, namelist, sizeof(namelist));
	serialize_namelist_from_str(namelist, kexinit, sess->trans.packet_buf_sz, &pos);

	noff = offsetof(struct deuce_ssh_enc_s, name);
	build_namelist(gconf.enc_head, noff, namelist, sizeof(namelist));
	serialize_namelist_from_str(namelist, kexinit, sess->trans.packet_buf_sz, &pos);
	serialize_namelist_from_str(namelist, kexinit, sess->trans.packet_buf_sz, &pos);

	noff = offsetof(struct deuce_ssh_mac_s, name);
	build_namelist(gconf.mac_head, noff, namelist, sizeof(namelist));
	serialize_namelist_from_str(namelist, kexinit, sess->trans.packet_buf_sz, &pos);
	serialize_namelist_from_str(namelist, kexinit, sess->trans.packet_buf_sz, &pos);

	noff = offsetof(struct deuce_ssh_comp_s, name);
	build_namelist(gconf.comp_head, noff, namelist, sizeof(namelist));
	serialize_namelist_from_str(namelist, kexinit, sess->trans.packet_buf_sz, &pos);
	serialize_namelist_from_str(namelist, kexinit, sess->trans.packet_buf_sz, &pos);

	serialize_namelist_from_str("", kexinit, sess->trans.packet_buf_sz, &pos);
	serialize_namelist_from_str("", kexinit, sess->trans.packet_buf_sz, &pos);

	/* first_kex_packet_follows = false */
	kexinit[pos++] = 0;

	/* reserved */
	deuce_ssh_serialize_uint32(0, kexinit, sess->trans.packet_buf_sz, &pos);

	/* Save our KEXINIT for exchange hash — transfer ownership */
	free(sess->trans.our_kexinit);
	sess->trans.our_kexinit = kexinit;
	sess->trans.our_kexinit_sz = pos;

	/* send_packet reads from kexinit (our_kexinit) into tx_packet */
	int res = deuce_ssh_transport_send_packet(sess, kexinit, pos, NULL);
	if (res < 0)
		return res;

	/* Receive peer's KEXINIT */
	uint8_t msg_type;
	uint8_t *peer_payload;
	size_t peer_len;
	res = deuce_ssh_transport_recv_packet(sess, &msg_type, &peer_payload, &peer_len);
	if (res < 0)
		return res;
	if (msg_type != SSH_MSG_KEXINIT)
		return DEUCE_SSH_ERROR_PARSE;

	free(sess->trans.peer_kexinit);
	sess->trans.peer_kexinit = malloc(peer_len);
	if (sess->trans.peer_kexinit == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	memcpy(sess->trans.peer_kexinit, peer_payload, peer_len);
	sess->trans.peer_kexinit_sz = peer_len;

	/* Parse peer's 10 name-lists (skip msg_type(1) + cookie(16)).
	 * RFC 4251 s6: algorithm names MUST NOT contain control characters
	 * (ASCII codes 32 or less) or DEL (127).  Reject violations. */
	size_t ppos = 17;
	char peer_lists[10][1024];
	for (int i = 0; i < 10; i++) {
		uint32_t nlen;
		if (deuce_ssh_parse_uint32(&peer_payload[ppos], peer_len - ppos, &nlen) < 4)
			return DEUCE_SSH_ERROR_PARSE;
		ppos += 4;
		if (ppos + nlen > peer_len)
			return DEUCE_SSH_ERROR_PARSE;
		for (uint32_t j = 0; j < nlen; j++) {
			uint8_t ch = peer_payload[ppos + j];
			if (ch <= ' ' || ch >= 127)
				return DEUCE_SSH_ERROR_PARSE;
		}
		size_t copylen = nlen < sizeof(peer_lists[i]) - 1 ? nlen : sizeof(peer_lists[i]) - 1;
		memcpy(peer_lists[i], &peer_payload[ppos], copylen);
		peer_lists[i][copylen] = 0;
		ppos += nlen;
	}

	/* Parse first_kex_packet_follows boolean */
	bool peer_first_kex_follows = false;
	if (ppos < peer_len)
		peer_first_kex_follows = (peer_payload[ppos] != 0);

	/* Negotiate algorithms */
	const char *client_kex, *server_kex;
	const char *client_hostkey, *server_hostkey;
	const char *client_enc_c2s, *server_enc_c2s;
	const char *client_enc_s2c, *server_enc_s2c;
	const char *client_mac_c2s, *server_mac_c2s;
	const char *client_mac_s2c, *server_mac_s2c;
	const char *client_comp_c2s, *server_comp_c2s;
	const char *client_comp_s2c, *server_comp_s2c;

	char our_lists[5][1024];
	build_namelist(gconf.kex_head, offsetof(struct deuce_ssh_kex_s, name), our_lists[0], sizeof(our_lists[0]));
	build_namelist(gconf.key_algo_head, offsetof(struct deuce_ssh_key_algo_s, name), our_lists[1], sizeof(our_lists[1]));
	build_namelist(gconf.enc_head, offsetof(struct deuce_ssh_enc_s, name), our_lists[2], sizeof(our_lists[2]));
	build_namelist(gconf.mac_head, offsetof(struct deuce_ssh_mac_s, name), our_lists[3], sizeof(our_lists[3]));
	build_namelist(gconf.comp_head, offsetof(struct deuce_ssh_comp_s, name), our_lists[4], sizeof(our_lists[4]));

	if (sess->trans.client) {
		client_kex = our_lists[0];      server_kex = peer_lists[0];
		client_hostkey = our_lists[1];  server_hostkey = peer_lists[1];
		client_enc_c2s = our_lists[2];  server_enc_c2s = peer_lists[2];
		client_enc_s2c = our_lists[2];  server_enc_s2c = peer_lists[3];
		client_mac_c2s = our_lists[3];  server_mac_c2s = peer_lists[4];
		client_mac_s2c = our_lists[3];  server_mac_s2c = peer_lists[5];
		client_comp_c2s = our_lists[4]; server_comp_c2s = peer_lists[6];
		client_comp_s2c = our_lists[4]; server_comp_s2c = peer_lists[7];
	}
	else {
		client_kex = peer_lists[0];      server_kex = our_lists[0];
		client_hostkey = peer_lists[1];  server_hostkey = our_lists[1];
		client_enc_c2s = peer_lists[2];  server_enc_c2s = our_lists[2];
		client_enc_s2c = peer_lists[3];  server_enc_s2c = our_lists[2];
		client_mac_c2s = peer_lists[4];  server_mac_c2s = our_lists[3];
		client_mac_s2c = peer_lists[5];  server_mac_s2c = our_lists[3];
		client_comp_c2s = peer_lists[6]; server_comp_c2s = our_lists[4];
		client_comp_s2c = peer_lists[7]; server_comp_s2c = our_lists[4];
	}

	sess->trans.kex_selected = negotiate_algo(client_kex, server_kex,
	    gconf.kex_head, offsetof(struct deuce_ssh_kex_s, name));
	sess->trans.key_algo_selected = negotiate_algo(client_hostkey, server_hostkey,
	    gconf.key_algo_head, offsetof(struct deuce_ssh_key_algo_s, name));
	sess->trans.enc_c2s_selected = negotiate_algo(client_enc_c2s, server_enc_c2s,
	    gconf.enc_head, offsetof(struct deuce_ssh_enc_s, name));
	sess->trans.enc_s2c_selected = negotiate_algo(client_enc_s2c, server_enc_s2c,
	    gconf.enc_head, offsetof(struct deuce_ssh_enc_s, name));
	sess->trans.mac_c2s_selected = negotiate_algo(client_mac_c2s, server_mac_c2s,
	    gconf.mac_head, offsetof(struct deuce_ssh_mac_s, name));
	sess->trans.mac_s2c_selected = negotiate_algo(client_mac_s2c, server_mac_s2c,
	    gconf.mac_head, offsetof(struct deuce_ssh_mac_s, name));
	sess->trans.comp_c2s_selected = negotiate_algo(client_comp_c2s, server_comp_c2s,
	    gconf.comp_head, offsetof(struct deuce_ssh_comp_s, name));
	sess->trans.comp_s2c_selected = negotiate_algo(client_comp_s2c, server_comp_s2c,
	    gconf.comp_head, offsetof(struct deuce_ssh_comp_s, name));

	if (sess->trans.kex_selected == NULL ||
	    sess->trans.key_algo_selected == NULL ||
	    sess->trans.enc_c2s_selected == NULL ||
	    sess->trans.enc_s2c_selected == NULL ||
	    sess->trans.mac_c2s_selected == NULL ||
	    sess->trans.mac_s2c_selected == NULL ||
	    sess->trans.comp_c2s_selected == NULL ||
	    sess->trans.comp_s2c_selected == NULL) {
		deuce_ssh_transport_disconnect(sess,
		    SSH_DISCONNECT_KEY_EXCHANGE_FAILED,
		    "no common algorithm");
		return DEUCE_SSH_ERROR_INIT;
	}

	/* Handle first_kex_packet_follows (RFC 4253 s7.1):
	 * If the peer sent a guessed KEX packet and the guess was wrong
	 * (negotiated != peer's first choice), discard it. */
	if (peer_first_kex_follows) {
		char peer_guess_kex[256];
		char peer_guess_hostkey[256];
		first_name(peer_lists[0], peer_guess_kex, sizeof(peer_guess_kex));
		first_name(peer_lists[1], peer_guess_hostkey, sizeof(peer_guess_hostkey));

		if (strcmp(sess->trans.kex_selected->name, peer_guess_kex) != 0 ||
		    strcmp(sess->trans.key_algo_selected->name, peer_guess_hostkey) != 0) {
			/* Wrong guess — discard the next packet */
			uint8_t discard_type;
			uint8_t *discard_payload;
			size_t discard_len;
			res = recv_packet_raw(sess, &discard_type, &discard_payload, &discard_len);
			if (res < 0)
				return res;
		}
	}

	return 0;
}

/* ================================================================
 * Key exchange (RFC 4253 s7)
 * ================================================================ */

int
deuce_ssh_transport_kex(deuce_ssh_session sess)
{
	if (sess->trans.kex_selected == NULL || sess->trans.kex_selected->handler == NULL)
		return DEUCE_SSH_ERROR_INIT;
	return sess->trans.kex_selected->handler(sess);
}

/* ================================================================
 * Key derivation and NEWKEYS (RFC 4253 s7.2, s7.3)
 * ================================================================ */

static int
derive_key(const char *hash_name,
    const uint8_t *shared_secret, size_t shared_secret_sz,
    const uint8_t *hash, size_t hash_sz,
    uint8_t letter,
    const uint8_t *session_id, size_t session_id_sz,
    uint8_t *out, size_t needed)
{
	EVP_MD *md = EVP_MD_fetch(NULL, hash_name, NULL);
	if (md == NULL)
		return DEUCE_SSH_ERROR_INIT;

	EVP_MD_CTX *ctx = EVP_MD_CTX_new();
	if (ctx == NULL) {
		EVP_MD_free(md);
		return DEUCE_SSH_ERROR_ALLOC;
	}

	size_t md_len = EVP_MD_get_size(md);
	uint8_t tmp[EVP_MAX_MD_SIZE];
	size_t have = 0;

	if (EVP_DigestInit_ex(ctx, md, NULL) != 1 ||
	    EVP_DigestUpdate(ctx, shared_secret, shared_secret_sz) != 1 ||
	    EVP_DigestUpdate(ctx, hash, hash_sz) != 1 ||
	    EVP_DigestUpdate(ctx, &letter, 1) != 1 ||
	    EVP_DigestUpdate(ctx, session_id, session_id_sz) != 1 ||
	    EVP_DigestFinal_ex(ctx, tmp, NULL) != 1) {
		EVP_MD_CTX_free(ctx);
		EVP_MD_free(md);
		return DEUCE_SSH_ERROR_INIT;
	}
	size_t copy = md_len < needed ? md_len : needed;
	memcpy(out, tmp, copy);
	have = copy;

	while (have < needed) {
		if (EVP_DigestInit_ex(ctx, md, NULL) != 1 ||
		    EVP_DigestUpdate(ctx, shared_secret, shared_secret_sz) != 1 ||
		    EVP_DigestUpdate(ctx, hash, hash_sz) != 1 ||
		    EVP_DigestUpdate(ctx, out, have) != 1 ||
		    EVP_DigestFinal_ex(ctx, tmp, NULL) != 1) {
			EVP_MD_CTX_free(ctx);
			EVP_MD_free(md);
			return DEUCE_SSH_ERROR_INIT;
		}
		copy = md_len < (needed - have) ? md_len : (needed - have);
		memcpy(&out[have], tmp, copy);
		have += copy;
	}

	EVP_MD_CTX_free(ctx);
	EVP_MD_free(md);
	return 0;
}

int
deuce_ssh_transport_newkeys(deuce_ssh_session sess)
{
	uint8_t newkeys_msg = SSH_MSG_NEWKEYS;
	int res = deuce_ssh_transport_send_packet(sess, &newkeys_msg, 1, NULL);
	if (res < 0)
		return res;

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	res = deuce_ssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0)
		return res;
	if (msg_type != SSH_MSG_NEWKEYS)
		return DEUCE_SSH_ERROR_PARSE;

	if (sess->trans.session_id == NULL) {
		sess->trans.session_id = malloc(sess->trans.exchange_hash_sz);
		if (sess->trans.session_id == NULL)
			return DEUCE_SSH_ERROR_ALLOC;
		memcpy(sess->trans.session_id, sess->trans.exchange_hash,
		    sess->trans.exchange_hash_sz);
		sess->trans.session_id_sz = sess->trans.exchange_hash_sz;
	}

	/* Encode shared secret as mpint */
	size_t ss_sz = sess->trans.shared_secret_sz;
	uint8_t *ss = sess->trans.shared_secret;
	bool need_pad = (ss_sz > 0 && (ss[0] & 0x80));
	size_t mpint_data_len = ss_sz + (need_pad ? 1 : 0);
	size_t mpint_wire_len = 4 + mpint_data_len;
	uint8_t *k_mpint = malloc(mpint_wire_len);
	if (k_mpint == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	size_t kpos = 0;
	deuce_ssh_serialize_uint32((uint32_t)mpint_data_len, k_mpint, mpint_wire_len, &kpos);
	if (need_pad)
		k_mpint[kpos++] = 0;
	memcpy(&k_mpint[kpos], ss, ss_sz);

	/* Derive keys using the hash algorithm from the KEX method */
	const char *hash_name = sess->trans.kex_selected->hash_name;
	uint16_t enc_key_sz = sess->trans.enc_c2s_selected->key_size;
	uint16_t enc_bs = sess->trans.enc_c2s_selected->blocksize;
	uint16_t mac_key_sz = sess->trans.mac_c2s_selected->key_size;

	uint8_t *iv_c2s = malloc(enc_bs);
	uint8_t *iv_s2c = malloc(enc_bs);
	uint8_t *key_c2s = malloc(enc_key_sz);
	uint8_t *key_s2c = malloc(enc_key_sz);
	uint8_t *integ_c2s = calloc(1, mac_key_sz > 0 ? mac_key_sz : 1);
	uint8_t *integ_s2c = calloc(1, mac_key_sz > 0 ? mac_key_sz : 1);

	if (!iv_c2s || !iv_s2c || !key_c2s || !key_s2c || !integ_c2s || !integ_s2c) {
		free(k_mpint);
		free(iv_c2s); free(iv_s2c);
		free(key_c2s); free(key_s2c);
		free(integ_c2s); free(integ_s2c);
		return DEUCE_SSH_ERROR_ALLOC;
	}

	const uint8_t *H = sess->trans.exchange_hash;
	size_t H_sz = sess->trans.exchange_hash_sz;
	const uint8_t *sid = sess->trans.session_id;
	size_t sid_sz = sess->trans.session_id_sz;

	res = derive_key(hash_name, k_mpint, mpint_wire_len, H, H_sz, 'A', sid, sid_sz, iv_c2s, enc_bs);
	if (res == 0) res = derive_key(hash_name, k_mpint, mpint_wire_len, H, H_sz, 'B', sid, sid_sz, iv_s2c, enc_bs);
	if (res == 0) res = derive_key(hash_name, k_mpint, mpint_wire_len, H, H_sz, 'C', sid, sid_sz, key_c2s, enc_key_sz);
	if (res == 0) res = derive_key(hash_name, k_mpint, mpint_wire_len, H, H_sz, 'D', sid, sid_sz, key_s2c, enc_key_sz);
	if (res == 0 && mac_key_sz > 0) res = derive_key(hash_name, k_mpint, mpint_wire_len, H, H_sz, 'E', sid, sid_sz, integ_c2s, mac_key_sz);
	if (res == 0 && mac_key_sz > 0) res = derive_key(hash_name, k_mpint, mpint_wire_len, H, H_sz, 'F', sid, sid_sz, integ_s2c, mac_key_sz);

	free(k_mpint);

	if (res < 0) {
		free(iv_c2s); free(iv_s2c);
		free(key_c2s); free(key_s2c);
		free(integ_c2s); free(integ_s2c);
		return res;
	}

	/* Initialize encryption contexts via the enc module */
	deuce_ssh_enc enc_c2s = sess->trans.enc_c2s_selected;
	deuce_ssh_enc enc_s2c = sess->trans.enc_s2c_selected;

	if (enc_c2s->init != NULL) {
		res = enc_c2s->init(key_c2s, iv_c2s, sess->trans.client,
		    &sess->trans.enc_c2s_ctx);
		if (res < 0)
			goto keys_cleanup;
	}
	if (enc_s2c->init != NULL) {
		res = enc_s2c->init(key_s2c, iv_s2c, !sess->trans.client,
		    &sess->trans.enc_s2c_ctx);
		if (res < 0)
			goto keys_cleanup;
	}

	/* Store MAC keys as context (the key IS the context for HMAC) */
	if (mac_key_sz > 0) {
		sess->trans.mac_c2s_ctx = (deuce_ssh_mac_ctx *)integ_c2s;
		sess->trans.mac_s2c_ctx = (deuce_ssh_mac_ctx *)integ_s2c;
		integ_c2s = NULL;
		integ_s2c = NULL;
	}

	free(sess->trans.our_kexinit);
	sess->trans.our_kexinit = NULL;
	sess->trans.our_kexinit_sz = 0;
	free(sess->trans.peer_kexinit);
	sess->trans.peer_kexinit = NULL;
	sess->trans.peer_kexinit_sz = 0;

keys_cleanup:
	free(iv_c2s); free(iv_s2c);
	free(key_c2s); free(key_s2c);
	free(integ_c2s); free(integ_s2c);
	return res;
}

/* ================================================================
 * Init / cleanup / registration
 * ================================================================ */

int
deuce_ssh_transport_init(deuce_ssh_session sess, size_t max_packet_size)
{
	gconf.used = true;
	if (gconf.software_version == NULL)
		gconf.software_version = sw_ver;


	if (max_packet_size < SSH_BPP_PACKET_SIZE_MIN)
		max_packet_size = SSH_BPP_PACKET_SIZE_MIN;

	sess->trans.packet_buf_sz = max_packet_size;
	sess->trans.tx_packet = malloc(max_packet_size);
	sess->trans.rx_packet = malloc(max_packet_size);
	sess->trans.tx_mac_scratch = malloc(4 + max_packet_size);
	sess->trans.rx_mac_scratch = malloc(4 + max_packet_size);
	if (!sess->trans.tx_packet || !sess->trans.rx_packet ||
	    !sess->trans.tx_mac_scratch || !sess->trans.rx_mac_scratch) {
		free(sess->trans.tx_packet);
		free(sess->trans.rx_packet);
		free(sess->trans.tx_mac_scratch);
		free(sess->trans.rx_mac_scratch);
		sess->trans.tx_packet = NULL;
		sess->trans.rx_packet = NULL;
		sess->trans.tx_mac_scratch = NULL;
		sess->trans.rx_mac_scratch = NULL;
		return DEUCE_SSH_ERROR_ALLOC;
	}

	if (mtx_init(&sess->trans.tx_mtx, mtx_plain) != thrd_success ||
	    mtx_init(&sess->trans.rx_mtx, mtx_plain) != thrd_success) {
		free(sess->trans.tx_packet);
		free(sess->trans.rx_packet);
		free(sess->trans.tx_mac_scratch);
		free(sess->trans.rx_mac_scratch);
		return DEUCE_SSH_ERROR_INIT;
	}

	sess->trans.kex_selected = NULL;
	sess->trans.key_algo_selected = NULL;
	sess->trans.enc_c2s_selected = NULL;
	sess->trans.enc_s2c_selected = NULL;
	sess->trans.mac_c2s_selected = NULL;
	sess->trans.mac_s2c_selected = NULL;
	sess->trans.comp_c2s_selected = NULL;
	sess->trans.comp_s2c_selected = NULL;
	sess->trans.session_id = NULL;
	sess->trans.our_kexinit = NULL;
	sess->trans.peer_kexinit = NULL;
	sess->trans.shared_secret = NULL;
	sess->trans.exchange_hash = NULL;
	sess->trans.tx_seq = 0;
	sess->trans.rx_seq = 0;

	return 0;
}

void
deuce_ssh_transport_cleanup(deuce_ssh_session sess)
{
	if (sess->trans.kex_selected) {
		if (sess->trans.kex_selected->cleanup != NULL)
			sess->trans.kex_selected->cleanup(sess);
		sess->trans.kex_selected = NULL;
	}
	if (sess->trans.key_algo_selected) {
		if (sess->trans.key_algo_selected->cleanup != NULL)
			sess->trans.key_algo_selected->cleanup(sess->trans.key_algo_ctx);
		sess->trans.key_algo_ctx = NULL;
		sess->trans.key_algo_selected = NULL;
	}
	if (sess->trans.enc_c2s_selected) {
		if (sess->trans.enc_c2s_selected->cleanup != NULL)
			sess->trans.enc_c2s_selected->cleanup(sess->trans.enc_c2s_ctx);
		sess->trans.enc_c2s_ctx = NULL;
		sess->trans.enc_c2s_selected = NULL;
	}
	if (sess->trans.enc_s2c_selected) {
		if (sess->trans.enc_s2c_selected->cleanup != NULL)
			sess->trans.enc_s2c_selected->cleanup(sess->trans.enc_s2c_ctx);
		sess->trans.enc_s2c_ctx = NULL;
		sess->trans.enc_s2c_selected = NULL;
	}
	if (sess->trans.mac_c2s_selected) {
		if (sess->trans.mac_c2s_selected->cleanup != NULL)
			sess->trans.mac_c2s_selected->cleanup(sess->trans.mac_c2s_ctx);
		sess->trans.mac_c2s_ctx = NULL;
		sess->trans.mac_c2s_selected = NULL;
	}
	if (sess->trans.mac_s2c_selected) {
		if (sess->trans.mac_s2c_selected->cleanup != NULL)
			sess->trans.mac_s2c_selected->cleanup(sess->trans.mac_s2c_ctx);
		sess->trans.mac_s2c_ctx = NULL;
		sess->trans.mac_s2c_selected = NULL;
	}
	if (sess->trans.comp_c2s_selected) {
		if (sess->trans.comp_c2s_selected->cleanup != NULL)
			sess->trans.comp_c2s_selected->cleanup(sess->trans.comp_c2s_ctx);
		sess->trans.comp_c2s_ctx = NULL;
		sess->trans.comp_c2s_selected = NULL;
	}
	if (sess->trans.comp_s2c_selected) {
		if (sess->trans.comp_s2c_selected->cleanup != NULL)
			sess->trans.comp_s2c_selected->cleanup(sess->trans.comp_s2c_ctx);
		sess->trans.comp_s2c_ctx = NULL;
		sess->trans.comp_s2c_selected = NULL;
	}

	mtx_destroy(&sess->trans.tx_mtx);
	mtx_destroy(&sess->trans.rx_mtx);

	free(sess->trans.tx_packet);
	sess->trans.tx_packet = NULL;
	free(sess->trans.rx_packet);
	sess->trans.rx_packet = NULL;
	free(sess->trans.tx_mac_scratch);
	sess->trans.tx_mac_scratch = NULL;
	free(sess->trans.rx_mac_scratch);
	sess->trans.rx_mac_scratch = NULL;
	sess->trans.packet_buf_sz = 0;

	free(sess->trans.session_id);
	sess->trans.session_id = NULL;
	free(sess->trans.our_kexinit);
	sess->trans.our_kexinit = NULL;
	free(sess->trans.peer_kexinit);
	sess->trans.peer_kexinit = NULL;
	free(sess->trans.shared_secret);
	sess->trans.shared_secret = NULL;
	free(sess->trans.exchange_hash);
	sess->trans.exchange_hash = NULL;

	sess->trans.remote_id_str_sz = 0;
	sess->trans.remote_id_str[0] = 0;
	if (sess->trans.remote_languages) {
		for (size_t i = 0; sess->trans.remote_languages[i]; i++)
			free(sess->trans.remote_languages[i]);
		free(sess->trans.remote_languages);
		sess->trans.remote_languages = NULL;
	}
}

int
deuce_ssh_transport_register_kex(deuce_ssh_kex kex)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	if (kex->next)
		return DEUCE_SSH_ERROR_MUST_BE_NULL;
	if (gconf.kex_entries + 1 == SIZE_MAX)
		return DEUCE_SSH_ERROR_TOOMANY;
	if (gconf.kex_head == NULL)
		gconf.kex_head = kex;
	if (gconf.kex_tail != NULL)
		gconf.kex_tail->next = kex;
	gconf.kex_tail = kex;
	gconf.kex_entries++;
	return 0;
}

int
deuce_ssh_transport_register_key_algo(deuce_ssh_key_algo key_algo)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	if (key_algo->next)
		return DEUCE_SSH_ERROR_MUST_BE_NULL;
	if (gconf.key_algo_entries + 1 == SIZE_MAX)
		return DEUCE_SSH_ERROR_TOOMANY;
	if (gconf.key_algo_head == NULL)
		gconf.key_algo_head = key_algo;
	if (gconf.key_algo_tail != NULL)
		gconf.key_algo_tail->next = key_algo;
	gconf.key_algo_tail = key_algo;
	gconf.key_algo_entries++;
	return 0;
}

int
deuce_ssh_transport_register_enc(deuce_ssh_enc enc)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	if (enc->next)
		return DEUCE_SSH_ERROR_MUST_BE_NULL;
	if (gconf.enc_entries + 1 == SIZE_MAX)
		return DEUCE_SSH_ERROR_TOOMANY;
	if (gconf.enc_head == NULL)
		gconf.enc_head = enc;
	if (gconf.enc_tail != NULL)
		gconf.enc_tail->next = enc;
	gconf.enc_tail = enc;
	gconf.enc_entries++;
	return 0;
}

int
deuce_ssh_transport_register_mac(deuce_ssh_mac mac)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	if (mac->next)
		return DEUCE_SSH_ERROR_MUST_BE_NULL;
	if (gconf.mac_entries + 1 == SIZE_MAX)
		return DEUCE_SSH_ERROR_TOOMANY;
	if (gconf.mac_head == NULL)
		gconf.mac_head = mac;
	if (gconf.mac_tail != NULL)
		gconf.mac_tail->next = mac;
	gconf.mac_tail = mac;
	gconf.mac_entries++;
	return 0;
}

int
deuce_ssh_transport_register_comp(deuce_ssh_comp comp)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	if (comp->next)
		return DEUCE_SSH_ERROR_MUST_BE_NULL;
	if (gconf.comp_entries + 1 == SIZE_MAX)
		return DEUCE_SSH_ERROR_TOOMANY;
	if (gconf.comp_head == NULL)
		gconf.comp_head = comp;
	if (gconf.comp_tail != NULL)
		gconf.comp_tail->next = comp;
	gconf.comp_tail = comp;
	gconf.comp_entries++;
	return 0;
}

int
deuce_ssh_transport_register_lang(deuce_ssh_language lang)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	if (lang->next)
		return DEUCE_SSH_ERROR_MUST_BE_NULL;
	if (gconf.lang_entries + 1 == SIZE_MAX)
		return DEUCE_SSH_ERROR_TOOMANY;
	if (gconf.lang_head == NULL)
		gconf.lang_head = lang;
	if (gconf.lang_tail != NULL)
		gconf.lang_tail->next = lang;
	gconf.lang_tail = lang;
	gconf.lang_entries++;
	return 0;
}

int
deuce_ssh_transport_set_callbacks(deuce_ssh_transport_io_cb tx, deuce_ssh_transport_io_cb rx, deuce_ssh_transport_rxline_cb rx_line, deuce_ssh_transport_extra_line_cb extra_line_cb)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	gconf.tx = tx;
	gconf.rx = rx;
	gconf.rx_line = rx_line;
	gconf.extra_line_cb = extra_line_cb;
	return 0;
}
