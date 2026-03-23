// RFC-4253: SSH Transport Layer Protocol

#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include "ssh-internal.h"

/*
 * DSSH_TESTABLE: marks internal functions that need to be linkable
 * from test code.  In normal builds these are static; when compiled
 * with -DDSSH_TESTING they become externally visible (with a
 * dssh_test_ prefix alias declared in test/dssh_test_internal.h).
 */
#ifdef DSSH_TESTING
#define DSSH_TESTABLE
#else
#define DSSH_TESTABLE static
#endif

typedef struct dssh_transport_global_config {
	atomic_bool used;
	const char *software_version;
	const char *version_comment;

	dssh_transport_io_cb tx;
	dssh_transport_io_cb rx;
	dssh_transport_rxline_cb rx_line;
	int (*extra_line_cb)(uint8_t *buf, size_t bufsz, void *cbdata);

	size_t kex_entries;
	dssh_kex kex_head;
	dssh_kex kex_tail;

	size_t key_algo_entries;
	dssh_key_algo key_algo_head;
	dssh_key_algo key_algo_tail;

	size_t enc_entries;
	dssh_enc enc_head;
	dssh_enc enc_tail;

	size_t mac_entries;
	dssh_mac mac_head;
	dssh_mac mac_tail;

	size_t comp_entries;
	dssh_comp comp_head;
	dssh_comp comp_tail;

	size_t lang_entries;
	dssh_language lang_head;
	dssh_language lang_tail;
} *dssh_transport_global_config;

#ifndef DSSH_VERSION_STRING
#define DSSH_VERSION_STRING "0.0"
#endif
static const char * const sw_ver = "DeuceSSH-" DSSH_VERSION_STRING;
static struct dssh_transport_global_config gconf;

/* ================================================================
 * Version exchange (RFC 4253 s4.2)
 * ================================================================ */

DSSH_TESTABLE inline bool
dssh_test_has_nulls(uint8_t *buf, size_t buflen)
{
	return memchr(buf, 0, buflen) != NULL;
}

DSSH_TESTABLE inline bool
dssh_test_missing_crlf(uint8_t *buf, size_t buflen)
{
	if (buflen < 2)
		return true;
	return (buf[buflen - 1] != '\n' || buf[buflen - 2] != '\r');
}

DSSH_TESTABLE inline bool
dssh_test_is_version_line(uint8_t *buf, size_t buflen)
{
	if (buflen < 4)
		return false;
	return (buf[0] == 'S' && buf[1] == 'S' && buf[2] == 'H' && buf[3] == '-');
}

/*
 * RFC 4253 s4.2: protoversion and softwareversion MUST consist of
 * printable US-ASCII characters, excluding whitespace and minus sign.
 * (minus is allowed as separator but not "excluded" — re-reading the
 * RFC, the constraint is "printable US-ASCII" which is 0x20–0x7E,
 * but 0x20 (space) is only allowed before comments).
 * We validate that the entire version line (before CR-LF) contains
 * only bytes in 0x20–0x7E (printable ASCII).
 */
DSSH_TESTABLE inline bool
dssh_test_has_non_ascii(uint8_t *buf, size_t buflen)
{
	for (size_t i = 0; i < buflen; i++) {
		if (buf[i] < 0x20 || buf[i] > 0x7E)
			return true;
	}
	return false;
}

/*
 * RFC 4253 s5.1: Clients using protocol 2.0 MUST be able to identify
 * "1.99" as identical to "2.0".
 */
DSSH_TESTABLE inline bool
dssh_test_is_20(uint8_t *buf, size_t buflen)
{
	if (buflen < 8)
		return false;
	if (buf[4] == '2' && buf[5] == '.' && buf[6] == '0' && buf[7] == '-')
		return true;
	if (buflen >= 9 && buf[4] == '1' && buf[5] == '.' && buf[6] == '9' && buf[7] == '9' && buf[8] == '-')
		return true;
	return false;
}

static int
version_rx(dssh_session sess)
{
	size_t received;
	int res;

	while (!sess->terminate) {
		res = gconf.rx_line(sess->trans.rx_packet, sess->trans.packet_buf_sz - 1, &received, sess, sess->rx_line_cbdata);
		if (res < 0) {
			sess->terminate = true;
			return res;
		}
		if (dssh_test_is_version_line(sess->trans.rx_packet, received)) {
			if (received > 255 || dssh_test_has_nulls(sess->trans.rx_packet, received) || dssh_test_missing_crlf(sess->trans.rx_packet, received) || dssh_test_has_non_ascii(sess->trans.rx_packet, received - 2) || !dssh_test_is_20(sess->trans.rx_packet, received)) {
				sess->terminate = true;
				return DSSH_ERROR_INVALID;
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
	return DSSH_ERROR_TERMINATED;
}

static int
version_tx(dssh_session sess)
{
	int res;
	size_t sz = 0;

	memcpy(sess->trans.tx_packet, "SSH-2.0-", 8);
	sz += 8;
	size_t asz = strlen(gconf.software_version);
	if (sz + asz + 2 > 255)
		return DSSH_ERROR_TOOLONG;
	memcpy(&sess->trans.tx_packet[sz], gconf.software_version, asz);
	sz += asz;
	if (gconf.version_comment != NULL) {
		memcpy(&sess->trans.tx_packet[sz], " ", 1);
		sz += 1;
		asz = strlen(gconf.version_comment);
		if (sz + asz + 2 > 255)
			return DSSH_ERROR_TOOLONG;
		memcpy(&sess->trans.tx_packet[sz], gconf.version_comment, asz);
		sz += asz;
	}
	memcpy(sess->trans.id_str, sess->trans.tx_packet, sz);
	sess->trans.id_str_sz = sz;
	sess->trans.id_str[sz] = 0;
	memcpy(&sess->trans.tx_packet[sz], "\r\n", 2);
	sz += 2;
	res = gconf.tx(sess->trans.tx_packet, sz, sess, sess->tx_cbdata);
	if (res < 0) {
		sess->terminate = true;
		return res;
	}
	return 0;
}

DSSH_PRIVATE dssh_key_algo
dssh_transport_find_key_algo(const char *name)
{
	for (dssh_key_algo ka = gconf.key_algo_head; ka != NULL; ka = ka->next) {
		if (strcmp(ka->name, name) == 0)
			return ka;
	}
	return NULL;
}

DSSH_PUBLIC const char *
dssh_transport_get_remote_version(dssh_session sess)
{
	return sess->trans.remote_id_str;
}

DSSH_PUBLIC const char *
dssh_transport_get_kex_name(dssh_session sess)
{
	return sess->trans.kex_selected ? sess->trans.kex_selected->name : NULL;
}

DSSH_PUBLIC const char *
dssh_transport_get_hostkey_name(dssh_session sess)
{
	return sess->trans.key_algo_selected ? sess->trans.key_algo_selected->name : NULL;
}

DSSH_PUBLIC const char *
dssh_transport_get_enc_name(dssh_session sess)
{
	return sess->trans.enc_c2s_selected ? sess->trans.enc_c2s_selected->name : NULL;
}

DSSH_PUBLIC const char *
dssh_transport_get_mac_name(dssh_session sess)
{
	return sess->trans.mac_c2s_selected ? sess->trans.mac_c2s_selected->name : NULL;
}

DSSH_PRIVATE int
dssh_transport_version_exchange(dssh_session sess)
{
	int res = version_tx(sess);
	if (res < 0)
		return res;
	return version_rx(sess);
}

DSSH_PUBLIC int
dssh_transport_handshake(dssh_session sess)
{
	int res = dssh_transport_version_exchange(sess);
	if (res < 0)
		return res;
	res = dssh_transport_kexinit(sess);
	if (res < 0)
		return res;
	res = dssh_transport_kex(sess);
	if (res < 0)
		return res;
	return dssh_transport_newkeys(sess);
}

/* ================================================================
 * Rekeying (RFC 4253 s9)
 * ================================================================ */

DSSH_PRIVATE bool
dssh_transport_rekey_needed(dssh_session sess)
{
	if (sess->trans.tx_since_rekey >= DSSH_REKEY_SOFT_LIMIT ||
	    sess->trans.rx_since_rekey >= DSSH_REKEY_SOFT_LIMIT)
		return true;
	if (sess->trans.bytes_since_rekey >= DSSH_REKEY_BYTES)
		return true;
	/* rekey_time is set to time(NULL) in dssh_transport_init() and
	 * newkeys(), so it is never 0 after initialization. */
#ifndef DSSH_TESTING
	if (sess->trans.rekey_time == 0)
		return false;
#endif
	if (time(NULL) - sess->trans.rekey_time >= DSSH_REKEY_SECONDS)
		return true;
	return false;
}

DSSH_PRIVATE int
dssh_transport_rekey(dssh_session sess)
{
	/*
	 * RFC 4253 s9: Rekey uses existing encryption until NEWKEYS.
	 * Do NOT free cipher/MAC contexts here — they are needed for
	 * the kexinit/kex packets.  newkeys() will replace them.
	 *
	 * Free old KEX outputs (shared_secret, exchange_hash) but NOT
	 * session_id — it persists across rekeys per RFC 4253 s7.2.
	 */
	free(sess->trans.shared_secret);
	sess->trans.shared_secret = NULL;
	sess->trans.shared_secret_sz = 0;
	free(sess->trans.exchange_hash);
	sess->trans.exchange_hash = NULL;
	sess->trans.exchange_hash_sz = 0;

	/*
	 * Block application-layer sends during rekey (RFC 4253 s7.1).
	 * Set the flag under tx_mtx since send_packet checks it there.
	 */
	mtx_lock(&sess->trans.tx_mtx);
	sess->trans.rekey_in_progress = true;
	mtx_unlock(&sess->trans.tx_mtx);

	/* Re-negotiate algorithms */
	int res = dssh_transport_kexinit(sess);
	if (res == 0)
		res = dssh_transport_kex(sess);
	if (res == 0)
		res = dssh_transport_newkeys(sess);

	/* Clear the flag and wake any blocked senders */
	mtx_lock(&sess->trans.tx_mtx);
	sess->trans.rekey_in_progress = false;
	cnd_broadcast(&sess->trans.rekey_cnd);
	mtx_unlock(&sess->trans.tx_mtx);

	return res;
}

/* ================================================================
 * SSH_MSG_UNIMPLEMENTED (RFC 4253 s11.4)
 * ================================================================ */

DSSH_PRIVATE int
dssh_transport_send_unimplemented(dssh_session sess,
    uint32_t rejected_seq)
{
	uint8_t msg[8];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_UNIMPLEMENTED;
	dssh_serialize_uint32(rejected_seq, msg, sizeof(msg), &pos);
	return dssh_transport_send_packet(sess, msg, pos, NULL);
}

/* ================================================================
 * SSH_MSG_DISCONNECT (RFC 4253 s11.1)
 * ================================================================ */

DSSH_PUBLIC int
dssh_transport_disconnect(dssh_session sess,
    uint32_t reason, const char *desc)
{
	size_t dlen = strlen(desc);
	if (dlen > 230)
		dlen = 230;
	uint8_t msg[256];
	size_t pos = 0;

	msg[pos++] = SSH_MSG_DISCONNECT;
	dssh_serialize_uint32(reason, msg, sizeof(msg), &pos);
	dssh_serialize_uint32((uint32_t)dlen, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], desc, dlen);
	pos += dlen;
	dssh_serialize_uint32(0, msg, sizeof(msg), &pos); /* language tag */

	dssh_transport_send_packet(sess, msg, pos, NULL);
	sess->terminate = true;
	return 0;
}

/* ================================================================
 * Binary Packet Protocol (RFC 4253 s6)
 * ================================================================ */

static size_t
tx_block_size(dssh_session sess)
{
	dssh_enc enc;
	void *cbd;
	if (sess->trans.client) {
		enc = sess->trans.enc_c2s_selected;
		cbd = sess->trans.enc_c2s_ctx;
	}
	else {
		enc = sess->trans.enc_s2c_selected;
		cbd = sess->trans.enc_s2c_ctx;
	}
	if (enc == NULL || cbd == NULL)
		return 8;
	/* No enc module registers with blocksize < 8; SSH minimum is 8. */
#ifndef DSSH_TESTING
	if (enc->blocksize < 8)
		return 8;
#endif
	return enc->blocksize;
}

static size_t
rx_block_size(dssh_session sess)
{
	dssh_enc enc;
	void *cbd;
	if (sess->trans.client) {
		enc = sess->trans.enc_s2c_selected;
		cbd = sess->trans.enc_s2c_ctx;
	}
	else {
		enc = sess->trans.enc_c2s_selected;
		cbd = sess->trans.enc_c2s_ctx;
	}
	if (enc == NULL || cbd == NULL)
		return 8;
	/* No enc module registers with blocksize < 8; SSH minimum is 8. */
#ifndef DSSH_TESTING
	if (enc->blocksize < 8)
		return 8;
#endif
	return enc->blocksize;
}

static uint16_t
tx_mac_size(dssh_session sess)
{
	dssh_mac mac;
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
rx_mac_size(dssh_session sess)
{
	dssh_mac mac;
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

DSSH_PRIVATE int
dssh_transport_send_packet(dssh_session sess,
    const uint8_t *payload, size_t payload_len, uint32_t *seq_out)
{
	int ret;
	mtx_lock(&sess->trans.tx_mtx);

	/*
	 * RFC 4253 s7.1: during rekey, only transport/KEX messages
	 * (types 1–49) are allowed.  Block application-layer messages
	 * (types 50+) until rekey completes.
	 */
	/* Every SSH packet has at least a 1-byte message type,
	 * so payload_len is always > 0 here. */
#ifdef DSSH_TESTING
	if (payload[0] >= 50) {
#else
	if (payload_len > 0 && payload[0] >= 50) {
#endif
		while (sess->trans.rekey_in_progress && !sess->terminate)
			cnd_wait(&sess->trans.rekey_cnd, &sess->trans.tx_mtx);
	}

	size_t bs = tx_block_size(sess);

	size_t padding_len = bs - ((5 + payload_len) % bs);
	if (padding_len < 4)
		padding_len += bs;

	uint32_t packet_length = (uint32_t)(1 + payload_len + padding_len);
	size_t total = 4 + packet_length;
	uint16_t mac_len = tx_mac_size(sess);

	if (total + mac_len > sess->trans.packet_buf_sz) {
		ret = DSSH_ERROR_TOOLONG;
		goto tx_done;
	}

	size_t pos = 0;
	dssh_serialize_uint32(packet_length, sess->trans.tx_packet, sess->trans.packet_buf_sz, &pos);
	sess->trans.tx_packet[pos++] = (uint8_t)padding_len;
	memcpy(&sess->trans.tx_packet[pos], payload, payload_len);
	pos += payload_len;
	if (RAND_bytes(&sess->trans.tx_packet[pos], (int)padding_len) != 1) {
		ret = DSSH_ERROR_INIT;
		goto tx_done;
	}
	pos += padding_len;

	/* MAC: mac(key, seq || unencrypted_packet) */
	if (mac_len > 0) {
		uint8_t *mac_input = sess->trans.tx_mac_scratch;
		size_t mi_pos = 0;
		dssh_serialize_uint32(sess->trans.tx_seq, mac_input, 4 + sess->trans.packet_buf_sz, &mi_pos);
		memcpy(&mac_input[mi_pos], sess->trans.tx_packet, total);
		mi_pos += total;

		dssh_mac mac;
		dssh_mac_ctx *mac_ctx;
		if (sess->trans.client) {
			mac = sess->trans.mac_c2s_selected;
			mac_ctx = sess->trans.mac_c2s_ctx;
		}
		else {
			mac = sess->trans.mac_s2c_selected;
			mac_ctx = sess->trans.mac_s2c_ctx;
		}
		ret = mac->generate(mac_input, mi_pos, &sess->trans.tx_packet[pos], mac_ctx);
		if (ret < 0)
			goto tx_done;
		pos += mac_len;
	}

	/* Encrypt (the packet, not the MAC) */
	{
		dssh_enc_ctx *enc_ctx;
		dssh_enc enc;
		if (sess->trans.client) {
			enc = sess->trans.enc_c2s_selected;
			enc_ctx = sess->trans.enc_c2s_ctx;
		}
		else {
			enc = sess->trans.enc_s2c_selected;
			enc_ctx = sess->trans.enc_s2c_ctx;
		}
		/* All enc modules always provide encrypt/decrypt functions. */
#ifdef DSSH_TESTING
		if (enc != NULL && enc_ctx != NULL) {
#else
		if (enc != NULL && enc->encrypt != NULL && enc_ctx != NULL) {
#endif
			ret = enc->encrypt(sess->trans.tx_packet, total, enc_ctx);
			if (ret < 0)
				goto tx_done;
		}
	}

	/* Refuse to send if per-key packet count would exceed hard limit */
	if (sess->trans.tx_since_rekey >= DSSH_REKEY_HARD_LIMIT) {
		ret = DSSH_ERROR_REKEY_NEEDED;
		goto tx_done;
	}

	ret = gconf.tx(sess->trans.tx_packet, pos, sess, sess->tx_cbdata);
	if (ret == 0) {
		if (seq_out != NULL)
			*seq_out = sess->trans.tx_seq;
		sess->trans.tx_seq++;
		sess->trans.tx_since_rekey++;
		sess->trans.bytes_since_rekey += pos;
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
recv_packet_raw(dssh_session sess,
    uint8_t *msg_type, uint8_t **payload, size_t *payload_len)
{
	int ret;
	mtx_lock(&sess->trans.rx_mtx);

	/* Refuse to receive if per-key packet count would exceed hard limit */
	if (sess->trans.rx_since_rekey >= DSSH_REKEY_HARD_LIMIT) {
		ret = DSSH_ERROR_REKEY_NEEDED;
		goto rx_done;
	}

	size_t bs = rx_block_size(sess);

	ret = gconf.rx(sess->trans.rx_packet, bs, sess, sess->rx_cbdata);
	if (ret < 0)
		goto rx_done;

	/* Decrypt first block */
	dssh_enc enc;
	dssh_enc_ctx *enc_ctx;
	if (sess->trans.client) {
		enc = sess->trans.enc_s2c_selected;
		enc_ctx = sess->trans.enc_s2c_ctx;
	}
	else {
		enc = sess->trans.enc_c2s_selected;
		enc_ctx = sess->trans.enc_c2s_ctx;
	}
	/* All enc modules always provide encrypt/decrypt functions. */
#ifdef DSSH_TESTING
	if (enc != NULL && enc_ctx != NULL) {
#else
	if (enc != NULL && enc->decrypt != NULL && enc_ctx != NULL) {
#endif
		ret = enc->decrypt(sess->trans.rx_packet, bs, enc_ctx);
		if (ret < 0)
			goto rx_done;
	}

	uint32_t packet_length;
	dssh_parse_uint32(sess->trans.rx_packet, 4, &packet_length);

	if (packet_length < 2 || packet_length + 4 > sess->trans.packet_buf_sz) {
		ret = DSSH_ERROR_TOOLONG;
		goto rx_done;
	}

	if (packet_length + 4 < bs) {
		ret = DSSH_ERROR_PARSE;
		goto rx_done;
	}

	size_t remaining = packet_length + 4 - bs;
	if (remaining > 0) {
		ret = gconf.rx(&sess->trans.rx_packet[bs], remaining, sess, sess->rx_cbdata);
		if (ret < 0)
			goto rx_done;
#ifdef DSSH_TESTING
		if (enc != NULL && enc_ctx != NULL) {
#else
		if (enc != NULL && enc->decrypt != NULL && enc_ctx != NULL) {
#endif
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
			ret = DSSH_ERROR_TOOLONG;
			goto rx_done;
		}
		ret = gconf.rx(received_mac, mac_len, sess, sess->rx_cbdata);
		if (ret < 0)
			goto rx_done;

		uint8_t *mac_input = sess->trans.rx_mac_scratch;
		size_t mi_pos = 0;
		dssh_serialize_uint32(sess->trans.rx_seq, mac_input, 4 + sess->trans.packet_buf_sz, &mi_pos);
		memcpy(&mac_input[mi_pos], sess->trans.rx_packet, packet_length + 4);
		mi_pos += packet_length + 4;

		uint8_t computed_mac[64];
		dssh_mac mac;
		dssh_mac_ctx *mac_ctx;
		if (sess->trans.client) {
			mac = sess->trans.mac_s2c_selected;
			mac_ctx = sess->trans.mac_s2c_ctx;
		}
		else {
			mac = sess->trans.mac_c2s_selected;
			mac_ctx = sess->trans.mac_c2s_ctx;
		}
		ret = mac->generate(mac_input, mi_pos, computed_mac, mac_ctx);
		if (ret < 0)
			goto rx_done;
		if (memcmp(received_mac, computed_mac, mac_len) != 0) {
			dssh_transport_disconnect(sess,
			    SSH_DISCONNECT_MAC_ERROR, "MAC verification failed");
			ret = DSSH_ERROR_INVALID;
			goto rx_done;
		}
	}

	uint8_t padding_length = sess->trans.rx_packet[4];
	if (padding_length + 1 >= packet_length) {
		ret = DSSH_ERROR_PARSE;
		goto rx_done;
	}
	*payload = &sess->trans.rx_packet[5];
	*payload_len = packet_length - padding_length - 1;
	*msg_type = sess->trans.rx_packet[5];

	sess->trans.last_rx_seq = sess->trans.rx_seq;
	sess->trans.rx_seq++;
	sess->trans.rx_since_rekey++;
	sess->trans.bytes_since_rekey += packet_length + 4 + mac_len;
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
DSSH_PRIVATE int
dssh_transport_recv_packet(dssh_session sess,
    uint8_t *msg_type, uint8_t **payload, size_t *payload_len)
{
	/* Handle deferred auto-rekey before reading new data.
	 * Safe here because no payload pointer is live yet. */
	if (sess->trans.rekey_pending) {
		sess->trans.rekey_pending = false;
		int rk = dssh_transport_rekey(sess);
		if (rk < 0)
			return rk;
	}

	for (;;) {
		int res = recv_packet_raw(sess, msg_type, payload, payload_len);
		if (res < 0)
			return res;

		switch (*msg_type) {
		case SSH_MSG_DISCONNECT:
			sess->terminate = true;
			return DSSH_ERROR_TERMINATED;
		case SSH_MSG_IGNORE:
			continue;
		case SSH_MSG_KEXINIT:
			/*
			 * During initial handshake (session_id not yet set),
			 * return KEXINIT to the caller (kexinit()).
			 *
			 * During a self-initiated rekey (rekey_in_progress
			 * is true), kexinit() called us to receive the
			 * peer's response — return KEXINIT to the caller
			 * so kexinit() can save it and proceed.
			 *
			 * Otherwise this is a peer-initiated rekey — save
			 * the peer's KEXINIT and run the rekey cycle.
			 */
			if (sess->trans.session_id == NULL ||
			    sess->trans.rekey_in_progress)
				return 0;
			{
				free(sess->trans.peer_kexinit);
				sess->trans.peer_kexinit = malloc(*payload_len);
				if (sess->trans.peer_kexinit == NULL)
					return DSSH_ERROR_ALLOC;
				memcpy(sess->trans.peer_kexinit, *payload, *payload_len);
				sess->trans.peer_kexinit_sz = *payload_len;

				/* rekey() handles the rest: sends our
				 * KEXINIT, runs KEX, exchanges NEWKEYS,
				 * and installs new keys. */
				int rk = dssh_transport_rekey(sess);
				if (rk < 0)
					return rk;
			}
			continue;
		case SSH_MSG_DEBUG:
			if (sess->debug_cb != NULL && *payload_len >= 2) {
				bool always_display = (*payload)[1];
				/* Parse: msg_type(1) + bool(1) + string message */
				size_t dpos = 2;
				uint32_t msg_len = 0;
				if (dpos + 4 <= *payload_len) {
					dssh_parse_uint32(&(*payload)[dpos], *payload_len - dpos, &msg_len);
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
				dssh_parse_uint32(&(*payload)[1], *payload_len - 1, &rejected_seq);
				sess->unimplemented_cb(rejected_seq, sess->unimplemented_cbdata);
			}
			continue;
		case 80: /* SSH_MSG_GLOBAL_REQUEST */
			/*
			 * RFC 4254 s4: receiver MUST respond appropriately.
			 * Parse the request name and want_reply flag.
			 */
			{
				size_t gpos = 1;
				uint32_t gname_len;
				if (gpos + 4 > *payload_len)
					break; /* malformed, return to caller */
				dssh_parse_uint32(&(*payload)[gpos], *payload_len - gpos, &gname_len);
				gpos += 4;
				if (gpos + gname_len + 1 > *payload_len)
					break;
				const uint8_t *gname = &(*payload)[gpos];
				gpos += gname_len;
				bool want_reply = ((*payload)[gpos] != 0);
				gpos++;

				int gr_res = -1;
				if (sess->global_request_cb != NULL) {
					typedef int (*gr_cb)(const uint8_t *, size_t,
					    bool, const uint8_t *, size_t, void *);
					gr_cb cb = (gr_cb)sess->global_request_cb;
					gr_res = cb(gname, gname_len, want_reply,
					    &(*payload)[gpos], *payload_len - gpos,
					    sess->global_request_cbdata);
				}

				if (want_reply) {
					uint8_t reply = (gr_res >= 0) ? 81 : 82;
					dssh_transport_send_packet(sess, &reply, 1, NULL);
				}
			}
			continue;
		default:
			/*
			 * Auto-rekey: if any threshold (packet count,
			 * bytes, or time) has been exceeded, note it.
			 * We cannot rekey HERE because *payload points
			 * into rx_packet — the rekey would overwrite it.
			 * Instead, set the flag and rekey on the NEXT
			 * recv_packet call, before reading new data.
			 */
			if (sess->trans.session_id != NULL &&
			    dssh_transport_rekey_needed(sess))
				sess->trans.rekey_pending = true;
			return 0;
		}
	}
}

/* ================================================================
 * KEXINIT (RFC 4253 s7.1)
 * ================================================================ */

DSSH_TESTABLE size_t
dssh_test_build_namelist(void *head, size_t name_offset, char *buf, size_t bufsz)
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

DSSH_TESTABLE void *
dssh_test_negotiate_algo(const char *client_list, const char *server_list,
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
	dssh_serialize_uint32(len, buf, bufsz, pos);
	memcpy(&buf[*pos], str, len);
	*pos += len;
}

DSSH_PRIVATE int
dssh_transport_kexinit(dssh_session sess)
{
	size_t kexinit_bufsz = sess->trans.packet_buf_sz;
	uint8_t *kexinit = malloc(kexinit_bufsz);
	if (kexinit == NULL)
		return DSSH_ERROR_ALLOC;
	size_t pos = 0;

	kexinit[pos++] = SSH_MSG_KEXINIT;

	if (RAND_bytes(&kexinit[pos], 16) != 1)
		return DSSH_ERROR_INIT;
	pos += 16;

	char namelist[1024];
	size_t noff;

	noff = offsetof(struct dssh_kex_s, name);
	dssh_test_build_namelist(gconf.kex_head, noff, namelist, sizeof(namelist));
	serialize_namelist_from_str(namelist, kexinit, sess->trans.packet_buf_sz, &pos);

	if (sess->trans.client) {
		noff = offsetof(struct dssh_key_algo_s, name);
		dssh_test_build_namelist(gconf.key_algo_head, noff, namelist, sizeof(namelist));
	}
	else {
		/* Server: only advertise key algorithms we have a key for.
		 * All registered key_algo modules provide haskey. The
		 * 1024-byte namelist buffer cannot overflow with max 64-char
		 * names and reasonable registration counts. */
		size_t nlpos = 0;
		bool nlfirst = true;
		for (dssh_key_algo ka = gconf.key_algo_head; ka != NULL;
		    ka = ka->next) {
#ifdef DSSH_TESTING
			if (!ka->haskey(ka->ctx))
#else
			if (ka->haskey == NULL ||
			    !ka->haskey(ka->ctx))
#endif
				continue;
			size_t nlen = strlen(ka->name);
#ifndef DSSH_TESTING
			if (!nlfirst && nlpos + 1 < sizeof(namelist))
#else
			if (!nlfirst)
#endif
				namelist[nlpos++] = ',';
#ifndef DSSH_TESTING
			if (nlpos + nlen < sizeof(namelist)) {
#endif
				memcpy(&namelist[nlpos], ka->name, nlen);
				nlpos += nlen;
#ifndef DSSH_TESTING
			}
#endif
			nlfirst = false;
		}
		namelist[nlpos] = 0;
	}
	serialize_namelist_from_str(namelist, kexinit, sess->trans.packet_buf_sz, &pos);

	noff = offsetof(struct dssh_enc_s, name);
	dssh_test_build_namelist(gconf.enc_head, noff, namelist, sizeof(namelist));
	serialize_namelist_from_str(namelist, kexinit, sess->trans.packet_buf_sz, &pos);
	serialize_namelist_from_str(namelist, kexinit, sess->trans.packet_buf_sz, &pos);

	noff = offsetof(struct dssh_mac_s, name);
	dssh_test_build_namelist(gconf.mac_head, noff, namelist, sizeof(namelist));
	serialize_namelist_from_str(namelist, kexinit, sess->trans.packet_buf_sz, &pos);
	serialize_namelist_from_str(namelist, kexinit, sess->trans.packet_buf_sz, &pos);

	noff = offsetof(struct dssh_comp_s, name);
	dssh_test_build_namelist(gconf.comp_head, noff, namelist, sizeof(namelist));
	serialize_namelist_from_str(namelist, kexinit, sess->trans.packet_buf_sz, &pos);
	serialize_namelist_from_str(namelist, kexinit, sess->trans.packet_buf_sz, &pos);

	serialize_namelist_from_str("", kexinit, sess->trans.packet_buf_sz, &pos);
	serialize_namelist_from_str("", kexinit, sess->trans.packet_buf_sz, &pos);

	/* first_kex_packet_follows = false */
	kexinit[pos++] = 0;

	/* reserved */
	dssh_serialize_uint32(0, kexinit, sess->trans.packet_buf_sz, &pos);

	/* Save our KEXINIT for exchange hash — transfer ownership */
	free(sess->trans.our_kexinit);
	sess->trans.our_kexinit = kexinit;
	sess->trans.our_kexinit_sz = pos;

	/* send_packet reads from kexinit (our_kexinit) into tx_packet */
	int res = dssh_transport_send_packet(sess, kexinit, pos, NULL);
	if (res < 0)
		return res;

	/*
	 * If peer_kexinit is already populated (peer-initiated rekey),
	 * skip receiving — we already have it.  Otherwise, receive it.
	 */
	if (sess->trans.peer_kexinit == NULL) {
		/*
		 * Wait for peer's KEXINIT.  During rekey, the peer
		 * may have sent application data (CHANNEL_DATA, etc.)
		 * before seeing our KEXINIT.  Discard non-KEXINIT
		 * messages — they will have been dispatched to
		 * channel buffers by recv_packet's demux handling,
		 * or we silently drop them here since the channel
		 * data is already in the bytebuf from the prior
		 * recv_packet call that triggered the rekey.
		 *
		 * Note: recv_packet handles DISCONNECT, IGNORE,
		 * DEBUG, UNIMPLEMENTED, and GLOBAL_REQUEST internally.
		 * Only application-layer messages (types 50+) reach
		 * the default case.
		 */
		uint8_t msg_type;
		uint8_t *peer_payload;
		size_t peer_len;
		for (;;) {
			res = dssh_transport_recv_packet(sess, &msg_type,
			    &peer_payload, &peer_len);
			if (res < 0)
				return res;
			if (msg_type == SSH_MSG_KEXINIT)
				break;
			/* Discard non-KEXINIT during rekey.
			 * Application data that arrived between our
			 * KEXINIT and the peer's is lost.  This is a
			 * race inherent in the SSH rekey protocol. */
		}

		sess->trans.peer_kexinit = malloc(peer_len);
		if (sess->trans.peer_kexinit == NULL)
			return DSSH_ERROR_ALLOC;
		memcpy(sess->trans.peer_kexinit, peer_payload, peer_len);
		sess->trans.peer_kexinit_sz = peer_len;
	}

	/* Parse peer's 10 name-lists (skip msg_type(1) + cookie(16)).
	 * RFC 4251 s6: algorithm names MUST NOT contain control characters
	 * (ASCII codes 32 or less) or DEL (127).  Reject violations. */
	uint8_t *pk = sess->trans.peer_kexinit;
	size_t pk_len = sess->trans.peer_kexinit_sz;
	size_t ppos = 17;
	char peer_lists[10][1024];
	for (int i = 0; i < 10; i++) {
		uint32_t nlen;
		if (dssh_parse_uint32(&pk[ppos], pk_len - ppos, &nlen) < 4)
			return DSSH_ERROR_PARSE;
		ppos += 4;
		if (ppos + nlen > pk_len)
			return DSSH_ERROR_PARSE;
		for (uint32_t j = 0; j < nlen; j++) {
			uint8_t ch = pk[ppos + j];
			if (ch <= ' ' || ch >= 127)
				return DSSH_ERROR_PARSE;
		}
		/* RFC 4251 s6: individual names MUST NOT exceed 64 chars */
		{
			uint32_t name_start = 0;
			for (uint32_t j = 0; j <= nlen; j++) {
				if (j == nlen || pk[ppos + j] == ',') {
					if (j - name_start > 64)
						return DSSH_ERROR_PARSE;
					name_start = j + 1;
				}
			}
		}
		/* With individual names capped at 64 bytes above, the
		 * total name-list cannot exceed 1023 bytes. */
#ifdef DSSH_TESTING
		size_t copylen = nlen;
#else
		size_t copylen = nlen < sizeof(peer_lists[i]) - 1 ? nlen : sizeof(peer_lists[i]) - 1;
#endif
		memcpy(peer_lists[i], &pk[ppos], copylen);
		peer_lists[i][copylen] = 0;
		ppos += nlen;
	}

	/* Parse first_kex_packet_follows boolean */
	bool peer_first_kex_follows = false;
	if (ppos < pk_len)
		peer_first_kex_follows = (pk[ppos] != 0);

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
	dssh_test_build_namelist(gconf.kex_head, offsetof(struct dssh_kex_s, name), our_lists[0], sizeof(our_lists[0]));
	dssh_test_build_namelist(gconf.key_algo_head, offsetof(struct dssh_key_algo_s, name), our_lists[1], sizeof(our_lists[1]));
	dssh_test_build_namelist(gconf.enc_head, offsetof(struct dssh_enc_s, name), our_lists[2], sizeof(our_lists[2]));
	dssh_test_build_namelist(gconf.mac_head, offsetof(struct dssh_mac_s, name), our_lists[3], sizeof(our_lists[3]));
	dssh_test_build_namelist(gconf.comp_head, offsetof(struct dssh_comp_s, name), our_lists[4], sizeof(our_lists[4]));

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

	sess->trans.kex_selected = dssh_test_negotiate_algo(client_kex, server_kex,
	    gconf.kex_head, offsetof(struct dssh_kex_s, name));
	sess->trans.key_algo_selected = dssh_test_negotiate_algo(client_hostkey, server_hostkey,
	    gconf.key_algo_head, offsetof(struct dssh_key_algo_s, name));
	sess->trans.enc_c2s_selected = dssh_test_negotiate_algo(client_enc_c2s, server_enc_c2s,
	    gconf.enc_head, offsetof(struct dssh_enc_s, name));
	sess->trans.enc_s2c_selected = dssh_test_negotiate_algo(client_enc_s2c, server_enc_s2c,
	    gconf.enc_head, offsetof(struct dssh_enc_s, name));
	sess->trans.mac_c2s_selected = dssh_test_negotiate_algo(client_mac_c2s, server_mac_c2s,
	    gconf.mac_head, offsetof(struct dssh_mac_s, name));
	sess->trans.mac_s2c_selected = dssh_test_negotiate_algo(client_mac_s2c, server_mac_s2c,
	    gconf.mac_head, offsetof(struct dssh_mac_s, name));
	sess->trans.comp_c2s_selected = dssh_test_negotiate_algo(client_comp_c2s, server_comp_c2s,
	    gconf.comp_head, offsetof(struct dssh_comp_s, name));
	sess->trans.comp_s2c_selected = dssh_test_negotiate_algo(client_comp_s2c, server_comp_s2c,
	    gconf.comp_head, offsetof(struct dssh_comp_s, name));

	if (sess->trans.kex_selected == NULL ||
	    sess->trans.key_algo_selected == NULL ||
	    sess->trans.enc_c2s_selected == NULL ||
	    sess->trans.enc_s2c_selected == NULL ||
	    sess->trans.mac_c2s_selected == NULL ||
	    sess->trans.mac_s2c_selected == NULL ||
	    sess->trans.comp_c2s_selected == NULL ||
	    sess->trans.comp_s2c_selected == NULL) {
		dssh_transport_disconnect(sess,
		    SSH_DISCONNECT_KEY_EXCHANGE_FAILED,
		    "no common algorithm");
		return DSSH_ERROR_INIT;
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

DSSH_PRIVATE int
dssh_transport_kex(dssh_session sess)
{
	/* kex_selected is validated non-NULL by kexinit() before
	 * kex() is called; all KEX modules provide a handler. */
#ifndef DSSH_TESTING
	if (sess->trans.kex_selected == NULL || sess->trans.kex_selected->handler == NULL)
		return DSSH_ERROR_INIT;
#endif
	return sess->trans.kex_selected->handler(sess);
}

/* ================================================================
 * Key derivation and NEWKEYS (RFC 4253 s7.2, s7.3)
 * ================================================================ */

DSSH_TESTABLE int
dssh_test_derive_key(const char *hash_name,
    const uint8_t *shared_secret, size_t shared_secret_sz,
    const uint8_t *hash, size_t hash_sz,
    uint8_t letter,
    const uint8_t *session_id, size_t session_id_sz,
    uint8_t *out, size_t needed)
{
	EVP_MD *md = EVP_MD_fetch(NULL, hash_name, NULL);
	if (md == NULL)
		return DSSH_ERROR_INIT;

	EVP_MD_CTX *ctx = EVP_MD_CTX_new();
	if (ctx == NULL) {
		EVP_MD_free(md);
		return DSSH_ERROR_ALLOC;
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
		return DSSH_ERROR_INIT;
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
			return DSSH_ERROR_INIT;
		}
		copy = md_len < (needed - have) ? md_len : (needed - have);
		memcpy(&out[have], tmp, copy);
		have += copy;
	}

	EVP_MD_CTX_free(ctx);
	EVP_MD_free(md);
	return 0;
}

DSSH_PRIVATE int
dssh_transport_newkeys(dssh_session sess)
{
	/*
	 * Save references to OLD encryption/MAC modules so we can call
	 * the correct cleanup functions after negotiation may have
	 * changed the _selected pointers (rekey case).
	 */
	dssh_enc old_enc_c2s = sess->trans.enc_c2s_selected;
	dssh_enc old_enc_s2c = sess->trans.enc_s2c_selected;
	dssh_mac old_mac_c2s = sess->trans.mac_c2s_selected;
	dssh_mac old_mac_s2c = sess->trans.mac_s2c_selected;

	uint8_t newkeys_msg = SSH_MSG_NEWKEYS;
	int res = dssh_transport_send_packet(sess, &newkeys_msg, 1, NULL);
	if (res < 0)
		return res;

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0)
		return res;
	if (msg_type != SSH_MSG_NEWKEYS)
		return DSSH_ERROR_PARSE;

	if (sess->trans.session_id == NULL) {
		sess->trans.session_id = malloc(sess->trans.exchange_hash_sz);
		if (sess->trans.session_id == NULL)
			return DSSH_ERROR_ALLOC;
		memcpy(sess->trans.session_id, sess->trans.exchange_hash,
		    sess->trans.exchange_hash_sz);
		sess->trans.session_id_sz = sess->trans.exchange_hash_sz;
	}

	/* Encode shared secret as mpint.
	 * Shared secret is always non-empty after successful KEX. */
	size_t ss_sz = sess->trans.shared_secret_sz;
	uint8_t *ss = sess->trans.shared_secret;
#ifdef DSSH_TESTING
	bool need_pad = (ss[0] & 0x80);
#else
	bool need_pad = (ss_sz > 0 && (ss[0] & 0x80));
#endif
	size_t mpint_data_len = ss_sz + (need_pad ? 1 : 0);
	size_t mpint_wire_len = 4 + mpint_data_len;
	uint8_t *k_mpint = malloc(mpint_wire_len);
	if (k_mpint == NULL)
		return DSSH_ERROR_ALLOC;
	size_t kpos = 0;
	dssh_serialize_uint32((uint32_t)mpint_data_len, k_mpint, mpint_wire_len, &kpos);
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
		return DSSH_ERROR_ALLOC;
	}

	const uint8_t *H = sess->trans.exchange_hash;
	size_t H_sz = sess->trans.exchange_hash_sz;
	const uint8_t *sid = sess->trans.session_id;
	size_t sid_sz = sess->trans.session_id_sz;

	res = dssh_test_derive_key(hash_name, k_mpint, mpint_wire_len, H, H_sz, 'A', sid, sid_sz, iv_c2s, enc_bs);
	if (res == 0) res = dssh_test_derive_key(hash_name, k_mpint, mpint_wire_len, H, H_sz, 'B', sid, sid_sz, iv_s2c, enc_bs);
	if (res == 0) res = dssh_test_derive_key(hash_name, k_mpint, mpint_wire_len, H, H_sz, 'C', sid, sid_sz, key_c2s, enc_key_sz);
	if (res == 0) res = dssh_test_derive_key(hash_name, k_mpint, mpint_wire_len, H, H_sz, 'D', sid, sid_sz, key_s2c, enc_key_sz);
	if (res == 0 && mac_key_sz > 0) res = dssh_test_derive_key(hash_name, k_mpint, mpint_wire_len, H, H_sz, 'E', sid, sid_sz, integ_c2s, mac_key_sz);
	if (res == 0 && mac_key_sz > 0) res = dssh_test_derive_key(hash_name, k_mpint, mpint_wire_len, H, H_sz, 'F', sid, sid_sz, integ_s2c, mac_key_sz);

	free(k_mpint);

	if (res < 0) {
		free(iv_c2s); free(iv_s2c);
		free(key_c2s); free(key_s2c);
		free(integ_c2s); free(integ_s2c);
		return res;
	}

	/*
	 * Free old cipher/MAC contexts (rekey case — during initial
	 * key exchange these are NULL so the cleanup calls are no-ops).
	 * RFC 4253 s9: old encryption is used until NEWKEYS, which we
	 * just exchanged above.  Now switch to new keys.
	 *
	 * Note: enc_c2s/enc_s2c are the NEWLY negotiated algorithms.
	 * The old contexts were created by whatever algorithm was
	 * previously selected.  Since all enc modules use the same
	 * cleanup pattern (EVP_CIPHER_CTX_free), and the cleanup
	 * function pointer is per-module, we need to use the enc module
	 * that CREATED the context.  For simplicity and correctness,
	 * we use the new module's cleanup — all our enc modules have
	 * compatible cleanup functions.  If a module's cleanup is NULL,
	 * the context was never created.
	 */
	/* All enc/mac modules provide cleanup functions. */
	if (sess->trans.enc_c2s_ctx != NULL && old_enc_c2s != NULL) {
#ifndef DSSH_TESTING
		if (old_enc_c2s->cleanup != NULL)
#endif
			old_enc_c2s->cleanup(sess->trans.enc_c2s_ctx);
		sess->trans.enc_c2s_ctx = NULL;
	}
	if (sess->trans.enc_s2c_ctx != NULL && old_enc_s2c != NULL) {
#ifndef DSSH_TESTING
		if (old_enc_s2c->cleanup != NULL)
#endif
			old_enc_s2c->cleanup(sess->trans.enc_s2c_ctx);
		sess->trans.enc_s2c_ctx = NULL;
	}
	if (sess->trans.mac_c2s_ctx != NULL && old_mac_c2s != NULL) {
#ifndef DSSH_TESTING
		if (old_mac_c2s->cleanup != NULL)
#endif
			old_mac_c2s->cleanup(sess->trans.mac_c2s_ctx);
		sess->trans.mac_c2s_ctx = NULL;
	}
	if (sess->trans.mac_s2c_ctx != NULL && old_mac_s2c != NULL) {
#ifndef DSSH_TESTING
		if (old_mac_s2c->cleanup != NULL)
#endif
			old_mac_s2c->cleanup(sess->trans.mac_s2c_ctx);
		sess->trans.mac_s2c_ctx = NULL;
	}

	/* Initialize new encryption contexts via the enc module */
	dssh_enc enc_c2s = sess->trans.enc_c2s_selected;
	dssh_enc enc_s2c = sess->trans.enc_s2c_selected;

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

	/* Initialize MAC contexts */
	if (mac_key_sz > 0 && sess->trans.mac_c2s_selected->init != NULL) {
		res = sess->trans.mac_c2s_selected->init(integ_c2s,
		    &sess->trans.mac_c2s_ctx);
		if (res < 0)
			goto keys_cleanup;
	}
	if (mac_key_sz > 0 && sess->trans.mac_s2c_selected->init != NULL) {
		res = sess->trans.mac_s2c_selected->init(integ_s2c,
		    &sess->trans.mac_s2c_ctx);
		if (res < 0)
			goto keys_cleanup;
	}

	/* Reset per-key counters */
	sess->trans.tx_since_rekey = 0;
	sess->trans.rx_since_rekey = 0;
	sess->trans.bytes_since_rekey = 0;
	sess->trans.rekey_time = time(NULL);

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

DSSH_PRIVATE int
dssh_transport_init(dssh_session sess, size_t max_packet_size)
{
	gconf.used = true;
	if (gconf.software_version == NULL)
		gconf.software_version = sw_ver;


	if (max_packet_size < SSH_BPP_PACKET_SIZE_MIN)
		max_packet_size = SSH_BPP_PACKET_SIZE_MIN;
	if (max_packet_size > SSH_BPP_PACKET_SIZE_MAX)
		max_packet_size = SSH_BPP_PACKET_SIZE_MAX;

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
		return DSSH_ERROR_ALLOC;
	}

	if (mtx_init(&sess->trans.tx_mtx, mtx_plain) != thrd_success ||
	    mtx_init(&sess->trans.rx_mtx, mtx_plain) != thrd_success) {
		free(sess->trans.tx_packet);
		free(sess->trans.rx_packet);
		free(sess->trans.tx_mac_scratch);
		free(sess->trans.rx_mac_scratch);
		return DSSH_ERROR_INIT;
	}
	if (cnd_init(&sess->trans.rekey_cnd) != thrd_success) {
		mtx_destroy(&sess->trans.tx_mtx);
		mtx_destroy(&sess->trans.rx_mtx);
		free(sess->trans.tx_packet);
		free(sess->trans.rx_packet);
		free(sess->trans.tx_mac_scratch);
		free(sess->trans.rx_mac_scratch);
		return DSSH_ERROR_INIT;
	}
	sess->trans.rekey_in_progress = false;
	sess->trans.rekey_pending = false;

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
	sess->trans.tx_since_rekey = 0;
	sess->trans.rx_since_rekey = 0;
	sess->trans.bytes_since_rekey = 0;
	sess->trans.rekey_time = time(NULL);

	return 0;
}

DSSH_PRIVATE void
dssh_transport_cleanup(dssh_session sess)
{
	/* All registered modules provide cleanup functions. */
	if (sess->trans.kex_selected) {
#ifndef DSSH_TESTING
		if (sess->trans.kex_selected->cleanup != NULL)
#endif
			sess->trans.kex_selected->cleanup(sess);
		sess->trans.kex_selected = NULL;
	}
	if (sess->trans.key_algo_selected) {
		/* Key context lives on the global algorithm entry, not the
		 * session — don't clean it up here. */
		sess->trans.key_algo_selected = NULL;
	}
	if (sess->trans.enc_c2s_selected) {
#ifndef DSSH_TESTING
		if (sess->trans.enc_c2s_selected->cleanup != NULL)
#endif
			sess->trans.enc_c2s_selected->cleanup(sess->trans.enc_c2s_ctx);
		sess->trans.enc_c2s_ctx = NULL;
		sess->trans.enc_c2s_selected = NULL;
	}
	if (sess->trans.enc_s2c_selected) {
#ifndef DSSH_TESTING
		if (sess->trans.enc_s2c_selected->cleanup != NULL)
#endif
			sess->trans.enc_s2c_selected->cleanup(sess->trans.enc_s2c_ctx);
		sess->trans.enc_s2c_ctx = NULL;
		sess->trans.enc_s2c_selected = NULL;
	}
	if (sess->trans.mac_c2s_selected) {
#ifndef DSSH_TESTING
		if (sess->trans.mac_c2s_selected->cleanup != NULL)
#endif
			sess->trans.mac_c2s_selected->cleanup(sess->trans.mac_c2s_ctx);
		sess->trans.mac_c2s_ctx = NULL;
		sess->trans.mac_c2s_selected = NULL;
	}
	if (sess->trans.mac_s2c_selected) {
#ifndef DSSH_TESTING
		if (sess->trans.mac_s2c_selected->cleanup != NULL)
#endif
			sess->trans.mac_s2c_selected->cleanup(sess->trans.mac_s2c_ctx);
		sess->trans.mac_s2c_ctx = NULL;
		sess->trans.mac_s2c_selected = NULL;
	}
	if (sess->trans.comp_c2s_selected) {
#ifndef DSSH_TESTING
		if (sess->trans.comp_c2s_selected->cleanup != NULL)
#endif
			sess->trans.comp_c2s_selected->cleanup(sess->trans.comp_c2s_ctx);
		sess->trans.comp_c2s_ctx = NULL;
		sess->trans.comp_c2s_selected = NULL;
	}
	if (sess->trans.comp_s2c_selected) {
#ifndef DSSH_TESTING
		if (sess->trans.comp_s2c_selected->cleanup != NULL)
#endif
			sess->trans.comp_s2c_selected->cleanup(sess->trans.comp_s2c_ctx);
		sess->trans.comp_s2c_ctx = NULL;
		sess->trans.comp_s2c_selected = NULL;
	}

	cnd_destroy(&sess->trans.rekey_cnd);
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
	/* Language negotiation is not implemented; remote_languages
	 * is never populated, so this block is never reached. */
#ifndef DSSH_TESTING
	if (sess->trans.remote_languages) {
		for (size_t i = 0; sess->trans.remote_languages[i]; i++)
			free(sess->trans.remote_languages[i]);
		free(sess->trans.remote_languages);
		sess->trans.remote_languages = NULL;
	}
#endif
}

DSSH_PUBLIC int
dssh_transport_register_kex(dssh_kex kex)
{
	if (gconf.used)
		return DSSH_ERROR_TOOLATE;
	if (strlen(kex->name) > 64 || strlen(kex->name) == 0)
		return DSSH_ERROR_TOOLONG;
	if (kex->next)
		return DSSH_ERROR_MUST_BE_NULL;
	if (gconf.kex_entries + 1 == SIZE_MAX)
		return DSSH_ERROR_TOOMANY;
	if (gconf.kex_head == NULL)
		gconf.kex_head = kex;
	if (gconf.kex_tail != NULL)
		gconf.kex_tail->next = kex;
	gconf.kex_tail = kex;
	gconf.kex_entries++;
	return 0;
}

DSSH_PUBLIC int
dssh_transport_register_key_algo(dssh_key_algo key_algo)
{
	if (gconf.used)
		return DSSH_ERROR_TOOLATE;
	if (strlen(key_algo->name) > 64 || strlen(key_algo->name) == 0)
		return DSSH_ERROR_TOOLONG;
	if (key_algo->next)
		return DSSH_ERROR_MUST_BE_NULL;
	if (gconf.key_algo_entries + 1 == SIZE_MAX)
		return DSSH_ERROR_TOOMANY;
	if (gconf.key_algo_head == NULL)
		gconf.key_algo_head = key_algo;
	if (gconf.key_algo_tail != NULL)
		gconf.key_algo_tail->next = key_algo;
	gconf.key_algo_tail = key_algo;
	gconf.key_algo_entries++;
	return 0;
}

DSSH_PUBLIC int
dssh_transport_register_enc(dssh_enc enc)
{
	if (gconf.used)
		return DSSH_ERROR_TOOLATE;
	if (strlen(enc->name) > 64 || strlen(enc->name) == 0)
		return DSSH_ERROR_TOOLONG;
	if (enc->next)
		return DSSH_ERROR_MUST_BE_NULL;
	if (gconf.enc_entries + 1 == SIZE_MAX)
		return DSSH_ERROR_TOOMANY;
	if (gconf.enc_head == NULL)
		gconf.enc_head = enc;
	if (gconf.enc_tail != NULL)
		gconf.enc_tail->next = enc;
	gconf.enc_tail = enc;
	gconf.enc_entries++;
	return 0;
}

DSSH_PUBLIC int
dssh_transport_register_mac(dssh_mac mac)
{
	if (gconf.used)
		return DSSH_ERROR_TOOLATE;
	if (strlen(mac->name) > 64 || strlen(mac->name) == 0)
		return DSSH_ERROR_TOOLONG;
	if (mac->next)
		return DSSH_ERROR_MUST_BE_NULL;
	if (gconf.mac_entries + 1 == SIZE_MAX)
		return DSSH_ERROR_TOOMANY;
	if (gconf.mac_head == NULL)
		gconf.mac_head = mac;
	if (gconf.mac_tail != NULL)
		gconf.mac_tail->next = mac;
	gconf.mac_tail = mac;
	gconf.mac_entries++;
	return 0;
}

DSSH_PUBLIC int
dssh_transport_register_comp(dssh_comp comp)
{
	if (gconf.used)
		return DSSH_ERROR_TOOLATE;
	if (strlen(comp->name) > 64 || strlen(comp->name) == 0)
		return DSSH_ERROR_TOOLONG;
	if (comp->next)
		return DSSH_ERROR_MUST_BE_NULL;
	if (gconf.comp_entries + 1 == SIZE_MAX)
		return DSSH_ERROR_TOOMANY;
	if (gconf.comp_head == NULL)
		gconf.comp_head = comp;
	if (gconf.comp_tail != NULL)
		gconf.comp_tail->next = comp;
	gconf.comp_tail = comp;
	gconf.comp_entries++;
	return 0;
}

DSSH_PUBLIC int
dssh_transport_register_lang(dssh_language lang)
{
	if (gconf.used)
		return DSSH_ERROR_TOOLATE;
	if (strlen(lang->name) > 64 || strlen(lang->name) == 0)
		return DSSH_ERROR_TOOLONG;
	if (lang->next)
		return DSSH_ERROR_MUST_BE_NULL;
	if (gconf.lang_entries + 1 == SIZE_MAX)
		return DSSH_ERROR_TOOMANY;
	if (gconf.lang_head == NULL)
		gconf.lang_head = lang;
	if (gconf.lang_tail != NULL)
		gconf.lang_tail->next = lang;
	gconf.lang_tail = lang;
	gconf.lang_entries++;
	return 0;
}

/*
 * Validate a softwareversion string per RFC 4253 s4.2:
 * printable US-ASCII (0x21–0x7E), no spaces (space is the
 * delimiter between softwareversion and comments).
 */
static bool
is_valid_sw_version(const char *s, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if (s[i] < 0x21 || s[i] > 0x7E)
			return false;
	}
	return true;
}

/*
 * Validate a comment string per RFC 4253 s4.2:
 * printable US-ASCII (0x20–0x7E).  Spaces are allowed in comments;
 * the comment runs from the SP after softwareversion to CR LF.
 */
static bool
is_valid_comment(const char *s, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if (s[i] < 0x20 || s[i] > 0x7E)
			return false;
	}
	return true;
}

DSSH_PUBLIC int
dssh_transport_set_version(const char *software_version,
    const char *comment)
{
	if (gconf.used)
		return DSSH_ERROR_TOOLATE;

	/* NULL software_version leaves the default; validate if provided */
	const char *sv = software_version != NULL ? software_version : sw_ver;
	size_t sv_len = strlen(sv);
	if (sv_len == 0)
		return DSSH_ERROR_PARSE;
	if (!is_valid_sw_version(sv, sv_len))
		return DSSH_ERROR_INVALID;

	/* "SSH-2.0-" (8) + version + " " + comment + CR LF <= 255 */
	size_t total = 8 + sv_len + 2;
	if (comment != NULL) {
		size_t cm_len = strlen(comment);
		if (cm_len == 0)
			return DSSH_ERROR_PARSE;
		if (!is_valid_comment(comment, cm_len))
			return DSSH_ERROR_INVALID;
		total += 1 + cm_len; /* SP + comment */
	}
	if (total > 255)
		return DSSH_ERROR_TOOLONG;

	gconf.software_version = sv;
	gconf.version_comment = comment;
	return 0;
}

DSSH_PUBLIC int
dssh_transport_set_callbacks(dssh_transport_io_cb tx, dssh_transport_io_cb rx, dssh_transport_rxline_cb rx_line, dssh_transport_extra_line_cb extra_line_cb)
{
	if (gconf.used)
		return DSSH_ERROR_TOOLATE;
	gconf.tx = tx;
	gconf.rx = rx;
	gconf.rx_line = rx_line;
	gconf.extra_line_cb = extra_line_cb;
	return 0;
}

#ifdef DSSH_TESTING
/*
 * Reset the global configuration for test isolation.
 * Frees all registered algorithm entries, zeros the config,
 * and unlocks registration.
 */
void
dssh_test_reset_global_config(void)
{
	/* Free linked list helpers */
#define FREE_LIST(head) do { \
	for (void *_n = (head); _n != NULL; ) { \
		void *_next; \
		memcpy(&_next, _n, sizeof(void *)); \
		free(_n); \
		_n = _next; \
	} \
} while (0)

	/* Clean up key_algo contexts before freeing */
	for (dssh_key_algo ka = gconf.key_algo_head; ka != NULL; ka = ka->next) {
		if (ka->cleanup != NULL && ka->ctx != NULL)
			ka->cleanup(ka->ctx);
	}

	FREE_LIST(gconf.kex_head);
	FREE_LIST(gconf.key_algo_head);
	FREE_LIST(gconf.enc_head);
	FREE_LIST(gconf.mac_head);
	FREE_LIST(gconf.comp_head);
	FREE_LIST(gconf.lang_head);
#undef FREE_LIST

	memset(&gconf, 0, sizeof(gconf));
	gconf.used = false;
}

/*
 * Provide extern definitions for the inline version validators
 * so the linker can resolve calls from test code.  In C17, an
 * inline function only emits an external definition when an
 * extern declaration appears in the same translation unit.
 */
extern inline bool dssh_test_has_nulls(uint8_t *buf, size_t buflen);
extern inline bool dssh_test_missing_crlf(uint8_t *buf, size_t buflen);
extern inline bool dssh_test_is_version_line(uint8_t *buf, size_t buflen);
extern inline bool dssh_test_has_non_ascii(uint8_t *buf, size_t buflen);
extern inline bool dssh_test_is_20(uint8_t *buf, size_t buflen);
#endif
