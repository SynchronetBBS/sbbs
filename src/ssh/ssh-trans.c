// RFC-4253: SSH Transport Layer Protocol

#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "ssh-internal.h"
#include "deucessh-algorithms.h"
#include "deucessh-auth.h"
#include "deucessh-crypto.h"

/* DSSH_TESTABLE is defined in ssh-internal.h */

/* Forward declarations for functions defined later in this file
 * but called before their definitions. */
static int transport_init(struct dssh_session_s *sess, size_t max_packet_size);
static void transport_cleanup(struct dssh_session_s *sess);
DSSH_TESTABLE int kexinit(struct dssh_session_s *sess);
DSSH_TESTABLE int kex(struct dssh_session_s *sess);
DSSH_TESTABLE int newkeys(struct dssh_session_s *sess);

/* Scrub and free security-sensitive heap buffers. */
static void
cleanse_free(void *ptr, size_t len)
{
	if (ptr != NULL) {
		dssh_cleanse(ptr, len);
		free(ptr);
	}
}

/* struct dssh_transport_global_config is defined in ssh-trans.h */

#ifndef DSSH_VERSION_STRING
 #define DSSH_VERSION_STRING "0.0"
#endif
static const char                                  sw_ver[] = "DeuceSSH-" DSSH_VERSION_STRING;
static const char                                  ssh_version_prefix[] = "SSH-2.0-";

DSSH_TESTABLE struct dssh_transport_global_config gconf;

/* ================================================================
 * Version exchange (RFC 4253 s4.2)
 * ================================================================ */

DSSH_TESTABLE inline bool
has_nulls(uint8_t *buf, size_t buflen)
{
	return memchr(buf, 0, buflen) != NULL;
}

DSSH_TESTABLE inline bool
missing_crlf(uint8_t *buf, size_t buflen)
{
	if (buflen < 2)
		return true;
	return buf[buflen - 1] != '\n' || buf[buflen - 2] != '\r';
}

DSSH_TESTABLE inline bool
is_version_line(uint8_t *buf, size_t buflen)
{
	if (buflen < 4)
		return false;
	return buf[0] == 'S' && buf[1] == 'S' && buf[2] == 'H' && buf[3] == '-';
}

/*
 * RFC 4253 s4.2: protoversion and softwareversion MUST consist of
 * printable US-ASCII characters, excluding whitespace and minus sign.
 * (minus is allowed as separator but not "excluded" -- re-reading the
 * RFC, the constraint is "printable US-ASCII" which is 0x20--0x7E,
 * but 0x20 (space) is only allowed before comments).
 * We validate that the entire version line (before CR-LF) contains
 * only bytes in 0x20--0x7E (printable ASCII).
 */
DSSH_TESTABLE inline bool
has_non_ascii(uint8_t *buf, size_t buflen)
{
	for (size_t i = 0; i < buflen; i++) {
		if ((buf[i] < 0x20) || (buf[i] > 0x7E))
			return true;
	}
	return false;
}

/*
 * RFC 4253 s5.1: Clients using protocol 2.0 MUST be able to identify
 * "1.99" as identical to "2.0".
 */
DSSH_TESTABLE inline bool
is_20(uint8_t *buf, size_t buflen)
{
	if (buflen < 8)
		return false;
	if ((buf[4] == '2') && (buf[5] == '.') && (buf[6] == '0') && (buf[7] == '-'))
		return true;
	if ((buflen >= 9) && (buf[4] == '1') && (buf[5] == '.') && (buf[6] == '9') && (buf[7] == '9')
	    && (buf[8] == '-'))
		return true;
	return false;
}

static int
version_rx(struct dssh_session_s *sess)
{
	size_t received;
	int    res;

	while (!sess->terminate) {
		res = gconf.rx_line(sess->trans.rx_packet,
		        sess->trans.packet_buf_sz - 1,
		        &received,
		        sess,
		        sess->rx_line_cbdata);
		if (res < 0) {
			session_set_terminate(sess);
			return res;
		}
		if (is_version_line(sess->trans.rx_packet, received)) {
			if ((received > DSSH_VERSION_STRING_MAX) || has_nulls(sess->trans.rx_packet,
			    received) || missing_crlf(sess->trans.rx_packet,
			    received) || has_non_ascii(sess->trans.rx_packet,
			    received - 2) || !is_20(sess->trans.rx_packet, received)) {
				session_set_terminate(sess);
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
				session_set_terminate(sess);
				return res;
			}
		}
	}
	return DSSH_ERROR_TERMINATED;
}

DSSH_TESTABLE int
version_tx(struct dssh_session_s *sess)
{
	int    res;
	size_t sz = 0;

	memcpy(sess->trans.tx_packet, ssh_version_prefix, DSSH_STRLEN(ssh_version_prefix));
	sz += DSSH_STRLEN(ssh_version_prefix);

	size_t asz = strlen(gconf.software_version);

	if (sz + asz + 2 > DSSH_VERSION_STRING_MAX)
		return DSSH_ERROR_TOOLONG;
	memcpy(&sess->trans.tx_packet[sz], gconf.software_version, asz);
	sz += asz;
	if (gconf.version_comment != NULL) {
		memcpy(&sess->trans.tx_packet[sz], " ", 1);
		sz += 1;
		asz = strlen(gconf.version_comment);
		if (sz + asz + 2 > DSSH_VERSION_STRING_MAX)
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
		session_set_terminate(sess);
		return res;
	}
	return 0;
}

static dssh_kex
find_kex(const char *name)
{
	for (dssh_kex k = gconf.kex_head; k != NULL; k = k->next) {
		if (strcmp(k->name, name) == 0)
			return k;
	}
	return NULL;
}

DSSH_PRIVATE dssh_key_algo
find_key_algo(const char *name)
{
	for (dssh_key_algo ka = gconf.key_algo_head; ka != NULL; ka = ka->next) {
		if (strcmp(ka->name, name) == 0)
			return ka;
	}
	return NULL;
}

DSSH_PUBLIC const char *
dssh_transport_get_remote_version(struct dssh_session_s *sess)
{
	if (sess == NULL)
		return NULL;
	return sess->trans.remote_id_str;
}

DSSH_PUBLIC const char *
dssh_transport_get_kex_name(struct dssh_session_s *sess)
{
	if (sess == NULL)
		return NULL;
	return sess->trans.kex_selected ? sess->trans.kex_selected->name : NULL;
}

DSSH_PUBLIC const char *
dssh_transport_get_hostkey_name(struct dssh_session_s *sess)
{
	if (sess == NULL)
		return NULL;
	return sess->trans.key_algo_selected ? sess->trans.key_algo_selected->name : NULL;
}

DSSH_PUBLIC const char *
dssh_transport_get_enc_name(struct dssh_session_s *sess)
{
	if (sess == NULL)
		return NULL;
	return sess->trans.enc_c2s_selected ? sess->trans.enc_c2s_selected->name : NULL;
}

DSSH_PUBLIC const char *
dssh_transport_get_mac_name(struct dssh_session_s *sess)
{
	if (sess == NULL)
		return NULL;
	return sess->trans.mac_c2s_selected ? sess->trans.mac_c2s_selected->name : NULL;
}

DSSH_TESTABLE int
version_exchange(struct dssh_session_s *sess)
{
	int res = version_tx(sess);

	if (res < 0)
		return res;
	return version_rx(sess);
}

DSSH_PUBLIC int
dssh_transport_handshake(struct dssh_session_s *sess)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	int res = version_exchange(sess);

	if (res < 0)
		goto fail;
	res = kexinit(sess);
	if (res < 0)
		goto fail;
	res = kex(sess);
	if (res < 0)
		goto fail;
	res = newkeys(sess);
	if (res < 0)
		goto fail;
	return 0;

fail:
	session_set_terminate(sess);
	return res;
}

/* ================================================================
 * Rekeying (RFC 4253 s9)
 * ================================================================ */

DSSH_TESTABLE bool
rekey_needed(struct dssh_session_s *sess)
{
	/* tx counters are atomic -- read without tx_mtx.
	 * rx counters are protected by rx_mtx (held by caller). */
	uint32_t tx_pkts = atomic_load(&sess->trans.tx_since_rekey);
	if ((tx_pkts >= DSSH_REKEY_SOFT_LIMIT)
	    || (sess->trans.rx_since_rekey >= DSSH_REKEY_SOFT_LIMIT))
		return true;

	/* Sum tx + rx bytes (saturating) for the byte threshold */
	uint64_t tx_bytes = atomic_load(&sess->trans.tx_bytes_since_rekey);
	uint64_t total_bytes;
	if (tx_bytes > UINT64_MAX - sess->trans.rx_bytes_since_rekey)
		total_bytes = UINT64_MAX;
	else
		total_bytes = tx_bytes + sess->trans.rx_bytes_since_rekey;
	if (total_bytes >= DSSH_REKEY_BYTES)
		return true;
	if (sess->trans.rekey_time == 0)
		return false;
	if (time(NULL) - sess->trans.rekey_time >= DSSH_REKEY_SECONDS)
		return true;
	return false;
}

DSSH_TESTABLE int
rekey(struct dssh_session_s *sess)
{
        /*
         * RFC 4253 s9: Rekey uses existing encryption until NEWKEYS.
         * Do NOT free cipher/MAC contexts here -- they are needed for
         * the kexinit/kex packets.  newkeys() will replace them.
         *
         * Free old KEX outputs (shared_secret, exchange_hash) but NOT
         * session_id -- it persists across rekeys per RFC 4253 s7.2.
         */
	cleanse_free(sess->trans.shared_secret, sess->trans.shared_secret_sz);
	sess->trans.shared_secret = NULL;
	sess->trans.shared_secret_sz = 0;
	cleanse_free(sess->trans.exchange_hash, sess->trans.exchange_hash_sz);
	sess->trans.exchange_hash = NULL;
	sess->trans.exchange_hash_sz = 0;

        /*
         * Block application-layer sends during rekey (RFC 4253 s7.1).
         * Set the flag under tx_mtx since send_packet checks it there.
         */
	dssh_thrd_check(sess, mtx_lock(&sess->trans.tx_mtx));
	sess->trans.rekey_in_progress = true;
	dssh_thrd_check(sess, mtx_unlock(&sess->trans.tx_mtx));

        /* Re-negotiate algorithms */
	int res = kexinit(sess);

	if (res == 0)
		res = kex(sess);
	if (res == 0)
		res = newkeys(sess);

        /* Clear the flag and wake any blocked senders */
	dssh_thrd_check(sess, mtx_lock(&sess->trans.tx_mtx));
	sess->trans.rekey_in_progress = false;
	dssh_thrd_check(sess, cnd_broadcast(&sess->trans.rekey_cnd));
	dssh_thrd_check(sess, mtx_unlock(&sess->trans.tx_mtx));

	if (res < 0)
		session_set_terminate(sess);
	return res;
}

/* ================================================================
 * SSH_MSG_UNIMPLEMENTED (RFC 4253 s11.4)
 * ================================================================ */

DSSH_PRIVATE int
send_unimplemented(struct dssh_session_s *sess,
    uint32_t                                   rejected_seq)
{
	uint8_t msg[8];
	size_t  pos = 0;

	msg[pos++] = SSH_MSG_UNIMPLEMENTED;
	DSSH_PUT_U32(rejected_seq, msg, &pos);
	return send_packet(sess, msg, pos, NULL);
}

/* ================================================================
 * SSH_MSG_DISCONNECT (RFC 4253 s11.1)
 * ================================================================ */

DSSH_PUBLIC int
dssh_transport_disconnect(struct dssh_session_s *sess,
    uint32_t reason, const char *desc)
{
	if (sess == NULL || desc == NULL)
		return DSSH_ERROR_INVALID;
	size_t dlen = strlen(desc);

	if (dlen > DSSH_DISCONNECT_DESC_MAX)
		dlen = DSSH_DISCONNECT_DESC_MAX;

	uint8_t msg[256];
	size_t  pos = 0;

	msg[pos++] = SSH_MSG_DISCONNECT;
	DSSH_PUT_U32(reason, msg, &pos);
	DSSH_PUT_U32((uint32_t)dlen, msg, &pos);
	memcpy(&msg[pos], desc, dlen);
	pos += dlen;
	DSSH_PUT_U32(0, msg, &pos);

	send_packet(sess, msg, pos, NULL); /* best-effort */
	session_set_terminate(sess);
	return 0;
}

/* ================================================================
 * Binary Packet Protocol (RFC 4253 s6)
 * ================================================================ */

DSSH_TESTABLE size_t
tx_block_size(struct dssh_session_s *sess)
{
	dssh_enc enc;
	void    *cbd;

	if (sess->trans.client) {
		enc = sess->trans.enc_c2s_selected;
		cbd = sess->trans.enc_c2s_ctx;
	}
	else {
		enc = sess->trans.enc_s2c_selected;
		cbd = sess->trans.enc_s2c_ctx;
	}
	if ((enc == NULL) || (cbd == NULL))
		return 8;
	if (enc->blocksize < 8)
		return 8;
	return enc->blocksize;
}

DSSH_TESTABLE size_t
rx_block_size(struct dssh_session_s *sess)
{
	dssh_enc enc;
	void    *cbd;

	if (sess->trans.client) {
		enc = sess->trans.enc_s2c_selected;
		cbd = sess->trans.enc_s2c_ctx;
	}
	else {
		enc = sess->trans.enc_c2s_selected;
		cbd = sess->trans.enc_c2s_ctx;
	}
	if ((enc == NULL) || (cbd == NULL))
		return 8;

        /* No enc module registers with blocksize < 8; SSH minimum is 8. */
	if (enc->blocksize < 8)
		return 8;
	return enc->blocksize;
}

static uint16_t
tx_mac_size(struct dssh_session_s *sess)
{
	dssh_mac mac;
	void    *cbd;

	if (sess->trans.client) {
		mac = sess->trans.mac_c2s_selected;
		cbd = sess->trans.mac_c2s_ctx;
	}
	else {
		mac = sess->trans.mac_s2c_selected;
		cbd = sess->trans.mac_s2c_ctx;
	}
	if ((mac == NULL) || (cbd == NULL))
		return 0;
	return mac->digest_size;
}

static uint16_t
rx_mac_size(struct dssh_session_s *sess)
{
	dssh_mac mac;
	void    *cbd;

	if (sess->trans.client) {
		mac = sess->trans.mac_s2c_selected;
		cbd = sess->trans.mac_s2c_ctx;
	}
	else {
		mac = sess->trans.mac_c2s_selected;
		cbd = sess->trans.mac_c2s_ctx;
	}
	if ((mac == NULL) || (cbd == NULL))
		return 0;
	return mac->digest_size;
}

/*
 * Finalize and send a packet whose payload is already in tx_packet.
 * Caller MUST hold tx_mtx.  The payload starts at tx_packet[9]
 * (past seq(4) + pkt_length(4) + pad_len(1)).  This function:
 *   - computes padding, writes pkt_length and pad_len
 *   - generates random padding
 *   - computes MAC (seq || unencrypted_packet, contiguous)
 *   - encrypts the packet (not seq prefix or MAC)
 *   - sends via I/O callback
 *   - updates seq, packet count, byte count
 *
 * Used by send_packet_inner (copies payload then calls this) and by
 * zc_send_inner (payload already written by app via zc_getbuf).
 */
DSSH_PRIVATE int
tx_finalize(struct dssh_session_s *sess, size_t payload_len)
{
	int ret;

	size_t bs = tx_block_size(sess);

	if (payload_len > SIZE_MAX - 5) {
		ret = DSSH_ERROR_TOOLONG;
		goto done;
	}
	size_t padding_len = bs - ((5 + payload_len) % bs);
	if (padding_len < 4)
		padding_len += bs;

	size_t pkt_sz = 1 + payload_len + padding_len;

	if (pkt_sz > UINT32_MAX) {
		ret = DSSH_ERROR_TOOLONG;
		goto done;
	}
	uint32_t packet_length = (uint32_t)pkt_sz;

#if UINT32_MAX > SIZE_MAX - 4
	if (packet_length > SIZE_MAX - 4) {
		ret = DSSH_ERROR_TOOLONG;
		goto done;
	}
#endif
	size_t total = 4 + packet_length;
	uint16_t mac_len = tx_mac_size(sess);

	if (total + mac_len > sess->trans.packet_buf_sz) {
		ret = DSSH_ERROR_TOOLONG;
		goto done;
	}

	/* Write pkt_length and pad_len at tx_packet[4..8] */
	size_t pos = 4;
	DSSH_PUT_U32(packet_length, sess->trans.tx_packet, &pos);
	sess->trans.tx_packet[pos] = (uint8_t)padding_len;

	/* Random padding after payload: tx_packet[9 + payload_len] */
	ret = dssh_random(&sess->trans.tx_packet[9 + payload_len],
	    padding_len);
	if (ret < 0) {
		goto done;
	}

        /* MAC: mac(key, seq || unencrypted_packet)
         * Write seq at tx_packet[0]; MAC input is contiguous at [0..4+total). */
	if (mac_len > 0) {
		size_t seq_pos = 0;

		DSSH_PUT_U32(sess->trans.tx_seq, sess->trans.tx_packet,
		    &seq_pos);

		dssh_mac      mac;
		dssh_mac_ctx *mac_ctx;

		if (sess->trans.client) {
			mac = sess->trans.mac_c2s_selected;
			mac_ctx = sess->trans.mac_c2s_ctx;
		}
		else {
			mac = sess->trans.mac_s2c_selected;
			mac_ctx = sess->trans.mac_s2c_ctx;
		}
		ret = mac->generate(sess->trans.tx_packet, 4 + total,
		    &sess->trans.tx_packet[4 + total], mac_ctx);
		if (ret < 0) {
			goto done;
		}
	}

        /* Encrypt (the packet at [4..4+total), not the seq prefix or MAC) */
	{
		dssh_enc_ctx *enc_ctx;
		dssh_enc      enc;

		if (sess->trans.client) {
			enc = sess->trans.enc_c2s_selected;
			enc_ctx = sess->trans.enc_c2s_ctx;
		}
		else {
			enc = sess->trans.enc_s2c_selected;
			enc_ctx = sess->trans.enc_s2c_ctx;
		}

		if ((enc != NULL) && (enc->encrypt != NULL)
		    && (enc_ctx != NULL)) {
			ret = enc->encrypt(&sess->trans.tx_packet[4],
			    total, enc_ctx);
			if (ret < 0) {
				goto done;
			}
		}
	}

        /* Refuse to send if per-key packet count would exceed hard limit */
	if (sess->trans.tx_since_rekey >= DSSH_REKEY_HARD_LIMIT) {
		ret = DSSH_ERROR_REKEY_NEEDED;
		goto done;
	}

	/* Send [4..4+total+mac_len) — skip the seq prefix */
	{
		size_t wire_len = total + mac_len;

		ret = gconf.tx(&sess->trans.tx_packet[4], wire_len, sess,
		    sess->tx_cbdata);
		if (ret == 0) {
			sess->trans.tx_seq++;
			atomic_fetch_add(&sess->trans.tx_since_rekey, 1);
			{
				uint64_t old_tb = atomic_load(
				    &sess->trans.tx_bytes_since_rekey);
				if (wire_len > UINT64_MAX - old_tb)
					atomic_store(
					    &sess->trans.tx_bytes_since_rekey,
					    UINT64_MAX);
				else
					atomic_store(
					    &sess->trans.tx_bytes_since_rekey,
					    old_tb + wire_len);
			}
		}
	}

done:
	return ret;
}

/*
 * Core send logic.  Caller MUST hold tx_mtx.  Copies payload into
 * tx_packet then calls tx_finalize for MAC/encrypt/send.
 */
static int
send_packet_inner(struct dssh_session_s *sess,
    const uint8_t *payload, size_t payload_len, uint32_t *seq_out)
{
	if (payload_len > sess->trans.packet_buf_sz - 9)
		return DSSH_ERROR_TOOLONG;

	/* Copy payload into tx_packet[9] (past seq + pkt_length + pad_len) */
	memcpy(&sess->trans.tx_packet[9], payload, payload_len);

	int ret = tx_finalize(sess, payload_len);

	if (ret == 0 && seq_out != NULL)
		*seq_out = sess->trans.tx_seq - 1;
	return ret;
}

/*
 * Drain the tx queue under tx_mtx.  Caller MUST hold tx_mtx.
 * Sends all queued fire-and-forget payloads in FIFO order.
 */
DSSH_PRIVATE void
drain_tx_queue(struct dssh_session_s *sess)
{
	dssh_thrd_check(sess, mtx_lock(&sess->trans.tx_queue_mtx));
	struct dssh_tx_queue_entry *list = sess->trans.tx_queue_head;
	sess->trans.tx_queue_head = NULL;
	sess->trans.tx_queue_tail = NULL;
	dssh_thrd_check(sess, mtx_unlock(&sess->trans.tx_queue_mtx));

	while (list != NULL) {
		struct dssh_tx_queue_entry *e = list;
		list = list->next;
		int qret = send_packet_inner(sess, e->data, e->len, NULL);
		free(e);
		if ((qret < 0) && (qret != DSSH_ERROR_TOOLONG)
		    && (qret != DSSH_ERROR_REKEY_NEEDED)) {
			session_set_terminate(sess);
			/* Free remaining entries */
			while (list != NULL) {
				struct dssh_tx_queue_entry *rem = list;
				list = list->next;
				free(rem);
			}
			return;
		}
	}
}

/*
 * Enqueue a payload for later sending by the next send_packet() caller.
 * Returns 0 on success, DSSH_ERROR_ALLOC on allocation failure.
 */
static int
enqueue_tx(struct dssh_session_s *sess,
    const uint8_t *payload, size_t payload_len)
{
	struct dssh_tx_queue_entry *e = malloc(
	    sizeof(*e) + payload_len);
	if (!e)
		return DSSH_ERROR_ALLOC;
	e->next = NULL;
	e->len = payload_len;
	memcpy(e->data, payload, payload_len);

	dssh_thrd_check(sess, mtx_lock(&sess->trans.tx_queue_mtx));
	if (sess->trans.tx_queue_tail)
		sess->trans.tx_queue_tail->next = e;
	else
		sess->trans.tx_queue_head = e;
	sess->trans.tx_queue_tail = e;
	dssh_thrd_check(sess, mtx_unlock(&sess->trans.tx_queue_mtx));
	return 0;
}

/*
 * Non-blocking send for the demux thread's fire-and-forget protocol
 * responses (CHANNEL_FAILURE, OPEN_FAILURE, REQUEST_SUCCESS/FAILURE).
 * Tries to send immediately; if tx_mtx is busy, queues the payload
 * for the next send_packet() caller to drain.
 */
DSSH_PRIVATE int
send_or_queue(struct dssh_session_s *sess,
    const uint8_t *payload, size_t payload_len)
{
	int tr = dssh_thrd_check(sess, mtx_trylock(&sess->trans.tx_mtx));
	if (tr == thrd_success) {
		drain_tx_queue(sess);
		int ret = send_packet_inner(sess, payload, payload_len, NULL);
		if ((ret < 0) && (ret != DSSH_ERROR_TOOLONG)
		    && (ret != DSSH_ERROR_REKEY_NEEDED))
			session_set_terminate(sess);
		dssh_thrd_check(sess, mtx_unlock(&sess->trans.tx_mtx));
		return ret;
	}
	return enqueue_tx(sess, payload, payload_len);
}

DSSH_PRIVATE int
send_packet(struct dssh_session_s *sess,
    const uint8_t *payload, size_t payload_len, uint32_t *seq_out)
{
	int ret;

	dssh_thrd_check(sess, mtx_lock(&sess->trans.tx_mtx));

        /*
         * RFC 4253 s7.1: during rekey, only transport/KEX messages
         * (types 1--49) are allowed.  Block application-layer messages
         * (types 50+) until rekey completes.
         */
	if ((payload_len > 0) && (payload[0] >= SSH_MSG_USERAUTH_REQUEST)) {
		struct timespec rk_ts;
		if (sess->timeout_ms > 0)
			dssh_deadline_from_ms(&rk_ts, sess->timeout_ms);
		while (sess->trans.rekey_in_progress && !sess->terminate) {
			if (sess->timeout_ms <= 0) {
				dssh_thrd_check(sess, cnd_wait(
				    &sess->trans.rekey_cnd,
				    &sess->trans.tx_mtx));
			}
			else {
				if (dssh_thrd_check(sess, cnd_timedwait(
				    &sess->trans.rekey_cnd,
				    &sess->trans.tx_mtx, &rk_ts))
				    == thrd_timedout) {
					session_set_terminate(sess);
					ret = DSSH_ERROR_TERMINATED;
					goto tx_done;
				}
			}
		}
	}

	drain_tx_queue(sess);
	ret = send_packet_inner(sess, payload, payload_len, seq_out);

tx_done:

        /* TOOLONG (packet too big) and REKEY_NEEDED (hard limit) are
        * recoverable -- the session is still usable.  All other errors
        * (I/O callback failure, encrypt/MAC failure) mean the connection
        * is broken and cannot recover without closing the socket. */
	if ((ret < 0) && (ret != DSSH_ERROR_TOOLONG) && (ret != DSSH_ERROR_REKEY_NEEDED))
		session_set_terminate(sess);
	dssh_thrd_check(sess, mtx_unlock(&sess->trans.tx_mtx));
	return ret;
}

/*
 * Receive a single raw packet.  Does NOT handle transport messages
 * (IGNORE, DEBUG, UNIMPLEMENTED, DISCONNECT) -- that's done by the
 * public recv_packet wrapper below.
 */
static int
recv_packet_raw(struct dssh_session_s *sess,
    uint8_t *msg_type, uint8_t **payload, size_t *payload_len)
{
	int ret;

	dssh_thrd_check(sess, mtx_lock(&sess->trans.rx_mtx));

        /* Refuse to receive if per-key packet count would exceed hard limit */
	if (sess->trans.rx_since_rekey >= DSSH_REKEY_HARD_LIMIT) {
		ret = DSSH_ERROR_REKEY_NEEDED;
		goto rx_done;
	}

	size_t bs = rx_block_size(sess);

	/* Receive into rx_packet[4..], leaving [0..4) for seq prefix */
	ret = gconf.rx(&sess->trans.rx_packet[4], bs, sess, sess->rx_cbdata);
	if (ret < 0)
		goto rx_done;

        /* Decrypt first block */
	dssh_enc      enc;
	dssh_enc_ctx *enc_ctx;

	if (sess->trans.client) {
		enc = sess->trans.enc_s2c_selected;
		enc_ctx = sess->trans.enc_s2c_ctx;
	}
	else {
		enc = sess->trans.enc_c2s_selected;
		enc_ctx = sess->trans.enc_c2s_ctx;
	}

	if ((enc != NULL) && (enc->decrypt != NULL) && (enc_ctx != NULL)) {
		ret = enc->decrypt(&sess->trans.rx_packet[4], bs, enc_ctx);
		if (ret < 0)
			goto rx_done;
	}

	/* First block (>= 8 bytes) is decrypted; read packet_length */
	uint32_t packet_length = DSSH_GET_U32(&sess->trans.rx_packet[4]);

	if (packet_length < 2) {
		ret = DSSH_ERROR_PARSE;
		goto rx_done;
	}
	if (packet_length + 4 > sess->trans.packet_buf_sz) {
		ret = DSSH_ERROR_TOOLONG;
		goto rx_done;
	}

	if (packet_length + 4 < bs) {
		ret = DSSH_ERROR_PARSE;
		goto rx_done;
	}

	size_t remaining = packet_length + 4 - bs;
	if (remaining > 0) {
		ret = gconf.rx(&sess->trans.rx_packet[4 + bs], remaining,
		    sess, sess->rx_cbdata);
		if (ret < 0)
			goto rx_done;
		if ((enc != NULL) && (enc->decrypt != NULL) && (enc_ctx != NULL)) {
			ret = enc->decrypt(&sess->trans.rx_packet[4 + bs],
			    remaining, enc_ctx);
			if (ret < 0)
				goto rx_done;
		}
	}

        /* Verify MAC.  Write seq at rx_packet[0]; MAC input is
         * contiguous at rx_packet[0..4 + packet_length + 4). */
	uint16_t mac_len = rx_mac_size(sess);
	if (mac_len > 0) {
		ret = gconf.rx(sess->trans.rx_mac_buf, mac_len, sess, sess->rx_cbdata);
		if (ret < 0)
			goto rx_done;

		size_t seq_pos = 0;

		DSSH_PUT_U32(sess->trans.rx_seq, sess->trans.rx_packet, &seq_pos);

		dssh_mac      mac;
		dssh_mac_ctx *mac_ctx;

		if (sess->trans.client) {
			mac = sess->trans.mac_s2c_selected;
			mac_ctx = sess->trans.mac_s2c_ctx;
		}
		else {
			mac = sess->trans.mac_c2s_selected;
			mac_ctx = sess->trans.mac_c2s_ctx;
		}
		ret = mac->generate(sess->trans.rx_packet,
		    4 + packet_length + 4, sess->trans.rx_mac_computed,
		    mac_ctx);
		if (ret < 0)
			goto rx_done;
		if (dssh_crypto_memcmp(sess->trans.rx_mac_buf, sess->trans.rx_mac_computed, mac_len) != 0) {
			dssh_transport_disconnect(sess,
			    SSH_DISCONNECT_MAC_ERROR, "MAC verification failed");
			ret = DSSH_ERROR_INVALID;
			goto rx_done;
		}
	}

	uint8_t padding_length = sess->trans.rx_packet[8];
	if (padding_length + 1 >= packet_length) {
		ret = DSSH_ERROR_PARSE;
		goto rx_done;
	}
	*payload = &sess->trans.rx_packet[9];
	*payload_len = packet_length - padding_length - 1;
	*msg_type = sess->trans.rx_packet[9];

	sess->trans.last_rx_seq = sess->trans.rx_seq;
	sess->trans.rx_seq++;
	sess->trans.rx_since_rekey++;
	{
		size_t rx_bytes = (size_t)packet_length + 4 + mac_len;

		if (rx_bytes > UINT64_MAX - sess->trans.rx_bytes_since_rekey)
			sess->trans.rx_bytes_since_rekey = UINT64_MAX;
		else
			sess->trans.rx_bytes_since_rekey += rx_bytes;
	}
	ret = 0;

rx_done:

        /* REKEY_NEEDED (hard limit) is recoverable -- the session is
         * still usable.  All other recv errors (I/O failure, oversized
         * packet, bad padding, MAC mismatch, decrypt failure) mean the
         * connection is broken. */
	if ((ret < 0) && (ret != DSSH_ERROR_REKEY_NEEDED))
		session_set_terminate(sess);
	dssh_thrd_check(sess, mtx_unlock(&sess->trans.rx_mtx));
	return ret;
}

/*
 * Public recv_packet: transparently handles transport messages
 * per RFC 4253 s11:
 *   SSH_MSG_IGNORE (2)        -- silently discarded
 *   SSH_MSG_DEBUG (4)         -- invokes debug callback if set, then discarded
 *   SSH_MSG_UNIMPLEMENTED (3) -- silently discarded (see TODO.md item 44)
 *   SSH_MSG_DISCONNECT (1)    -- sets terminate, returns error
 */
DSSH_PRIVATE int
recv_packet(struct dssh_session_s *sess,
    uint8_t *msg_type, uint8_t **payload, size_t *payload_len)
{
        /* Handle deferred auto-rekey before reading new data.
        * Safe here because no payload pointer is live yet. */
	if (sess->trans.rekey_pending) {
		sess->trans.rekey_pending = false;

		int rk = rekey(sess);

		if (rk < 0)
			return rk;
	}

        /* Replay messages buffered during kexinit wait loop.
         * These were valid connection-layer messages sent by the
         * peer before it saw our KEXINIT (RFC 4253 s7.1).
         * Skip during rekey itself (rekey_in_progress) to avoid
         * re-buffering messages that the kexinit loop is filling. */
	if (!sess->trans.rekey_in_progress
	    && sess->trans.rekey_queue_head != NULL) {
		struct dssh_rekey_msg *rmsg = sess->trans.rekey_queue_head;

		sess->trans.rekey_queue_head = rmsg->next;
		if (sess->trans.rekey_queue_head == NULL)
			sess->trans.rekey_queue_tail = NULL;
		memcpy(&sess->trans.rx_packet[5], rmsg->data, rmsg->len);
		*payload = &sess->trans.rx_packet[5];
		*payload_len = rmsg->len;
		*msg_type = rmsg->msg_type;
		free(rmsg);
		return 0;
	}

	for (;;) {
		int res = recv_packet_raw(sess, msg_type, payload, payload_len);

		if (res < 0)
			return res;

		switch (*msg_type) {
			case SSH_MSG_DISCONNECT:
				session_set_terminate(sess);
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
                                 * peer's response -- return KEXINIT to the caller
                                 * so kexinit() can save it and proceed.
                                 *
                                 * Otherwise this is a peer-initiated rekey -- save
                                 * the peer's KEXINIT and run the rekey cycle.
                                 */
				if ((sess->trans.session_id == NULL)
				    || sess->trans.rekey_in_progress)
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
					int rk = rekey(sess);

					if (rk < 0)
						return rk;
				}
				continue;
			case SSH_MSG_DEBUG:
			{
				dssh_debug_cb dcb = sess->debug_cb;

				if ((dcb != NULL) && (*payload_len >= 2)) {
					bool     always_display = (*payload)[1];

                                        /* Parse: msg_type(1) + bool(1) + string message */
					size_t   dpos = 2;
					uint32_t msg_len = 0;

					if (dpos + 4 <= *payload_len) {
						msg_len = DSSH_GET_U32(&(*payload)[dpos]);
						dpos += 4;
						if (dpos + msg_len > *payload_len)
							msg_len = 0;
					}
					dcb(always_display,
					    msg_len > 0 ? &(*payload)[dpos] : NULL,
					    msg_len, sess->debug_cbdata);
				}
				continue;
			}
			case SSH_MSG_UNIMPLEMENTED:
			{
				dssh_unimplemented_cb ucb = sess->unimplemented_cb;

				if ((ucb != NULL) && (*payload_len >= 5)) {
					uint32_t rejected_seq = DSSH_GET_U32(&(*payload)[1]);

					ucb(rejected_seq, sess->unimplemented_cbdata);
				}
				continue;
			}
			case SSH_MSG_GLOBAL_REQUEST:
                                /*
                                 * RFC 4254 s4: receiver MUST respond appropriately.
                                 * Parse the request name and want_reply flag.
                                 */
			{
				size_t   gpos = 1;
				uint32_t gname_len;

				if (gpos + 4 > *payload_len) {
					uint8_t fail = SSH_MSG_REQUEST_FAILURE;
					send_or_queue(sess, &fail, 1);
					dssh_transport_disconnect(sess,
					    SSH_DISCONNECT_PROTOCOL_ERROR,
					    "malformed GLOBAL_REQUEST");
					return DSSH_ERROR_PARSE;
				}
				gname_len = DSSH_GET_U32(&(*payload)[gpos]);
				gpos += 4;
				if (gpos + gname_len + 1 > *payload_len) {
					uint8_t fail = SSH_MSG_REQUEST_FAILURE;
					send_or_queue(sess, &fail, 1);
					dssh_transport_disconnect(sess,
					    SSH_DISCONNECT_PROTOCOL_ERROR,
					    "malformed GLOBAL_REQUEST");
					return DSSH_ERROR_PARSE;
				}

				const uint8_t *gname = &(*payload)[gpos];

				gpos += gname_len;

				bool want_reply = ((*payload)[gpos] != 0);

				gpos++;

				int gr_res = -1;
				dssh_global_request_cb grcb = sess->global_request_cb;

				if (grcb != NULL) {
					gr_res = grcb(
					        gname, gname_len, want_reply,
					        &(*payload)[gpos],
					        *payload_len - gpos,
					        sess->global_request_cbdata);
				}

				if (want_reply) {
					uint8_t reply = (gr_res >= 0) ? SSH_MSG_REQUEST_SUCCESS : SSH_MSG_REQUEST_FAILURE;

					send_or_queue(sess, &reply, 1);
				}
			}
				continue;
			default:
                                /*
                                 * Auto-rekey: if any threshold (packet count,
                                 * bytes, or time) has been exceeded, note it.
                                 * We cannot rekey HERE because *payload points
                                 * into rx_packet -- the rekey would overwrite it.
                                 * Instead, set the flag and rekey on the NEXT
                                 * recv_packet call, before reading new data.
                                 */
				if ((sess->trans.session_id != NULL)
				    && !sess->trans.rekey_in_progress
				    && rekey_needed(sess))
					sess->trans.rekey_pending = true;
				return 0;
		}
	}
}

/* ================================================================
 * KEXINIT (RFC 4253 s7.1)
 * ================================================================ */

DSSH_TESTABLE size_t
build_namelist(void *head, size_t name_offset, char *buf, size_t bufsz)
{
	size_t pos = 0;
	bool   first = true;

	for (uint8_t *node = head; node != NULL;) {
		const char *name = (const char *)(node + name_offset);
		size_t      nlen = strlen(name);

		if (!first) {
			if (pos >= bufsz || nlen >= bufsz - pos - 1)
				break;
			buf[pos++] = ',';
		}
		else if (pos >= bufsz || nlen >= bufsz - pos)
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
DSSH_TESTABLE size_t
first_name(const char *list, char *buf, size_t bufsz)
{
	const char *comma = strchr(list, ',');
	size_t      len = comma ? (size_t)(comma - list) : strlen(list);

	if (len >= bufsz)
		len = bufsz - 1;
	memcpy(buf, list, len);
	buf[len] = 0;
	return len;
}

DSSH_TESTABLE void *
negotiate_algo(const char *client_list, const char *server_list,
    void *head, size_t name_offset)
{
	const char *cp = client_list;

	while (*cp) {
		const char *comma = strchr(cp, ',');
		size_t      clen = comma ? (size_t)(comma - cp) : strlen(cp);

		const char *sp = server_list;

		while (*sp) {
			const char *sc = strchr(sp, ',');
			size_t      slen = sc ? (size_t)(sc - sp) : strlen(sp);

			if ((clen == slen) && (memcmp(cp, sp, clen) == 0)) {
				for (uint8_t *node = head; node != NULL;) {
					const char *name = (const char *)(node + name_offset);

					if ((strlen(name) == clen) && (memcmp(name, cp, clen) == 0))
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

static int
serialize_namelist_from_str(const char *str, uint8_t *buf, size_t bufsz, size_t *pos)
{
	size_t slen = strlen(str);

#if SIZE_MAX > UINT32_MAX
	if (slen > UINT32_MAX)
		return DSSH_ERROR_TOOLONG;
#endif

	uint32_t len = (uint32_t)slen;

	if (*pos > bufsz || bufsz - *pos < 4 + len)
		return DSSH_ERROR_TOOLONG;
	DSSH_PUT_U32(len, buf, pos);
	memcpy(&buf[*pos], str, len);
	*pos += len;
	return 0;
}

/*
 * Build our KEXINIT packet: msg_type, cookie, 10 name-lists,
 * first_kex_packet_follows, reserved.  On success, *buf_out
 * is the caller-owned packet and *pos_out is its length.
 */
static int
build_kexinit_packet(struct dssh_session_s *sess, uint8_t **buf_out, size_t *pos_out)
{
	size_t   bufsz = sess->trans.packet_buf_sz;
	uint8_t *buf = malloc(bufsz);

	if (buf == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;
	int    ret;

	buf[pos++] = SSH_MSG_KEXINIT;

	int rret = dssh_random(&buf[pos], DSSH_KEXINIT_COOKIE_SIZE);

	if (rret < 0) {
		free(buf);
		return rret;
	}
	pos += DSSH_KEXINIT_COOKIE_SIZE;

	char   namelist[DSSH_NAMELIST_BUF_SIZE];
	size_t noff;

	/* KEX algorithms */
	noff = offsetof(struct dssh_kex_s, name);
	build_namelist(gconf.kex_head, noff, namelist,
	    sizeof(namelist));
	ret = serialize_namelist_from_str(namelist, buf, bufsz, &pos);
	if (ret < 0) {
		free(buf);
		return ret;
	}

	/* Host key algorithms (server filters by haskey) */
	if (sess->trans.client) {
		noff = offsetof(struct dssh_key_algo_s, name);
		build_namelist(gconf.key_algo_head, noff,
		    namelist, sizeof(namelist));
	}
	else {
		size_t nlpos = 0;
		bool   nlfirst = true;

		for (dssh_key_algo ka = gconf.key_algo_head; ka != NULL;
		    ka = ka->next) {
			if ((ka->haskey == NULL)
			    || !ka->haskey(ka->ctx))
				continue;

			size_t nlen = strlen(ka->name);

			if (!nlfirst && (nlpos + 1 < sizeof(namelist)))
				namelist[nlpos++] = ',';
			if (nlpos + nlen < sizeof(namelist)) {
				memcpy(&namelist[nlpos], ka->name, nlen);
				nlpos += nlen;
			}
			nlfirst = false;
		}
		namelist[nlpos] = 0;
	}

	/* Host key, enc (c2s, s2c), mac (c2s, s2c), comp (c2s, s2c) */
	ret = serialize_namelist_from_str(namelist, buf, bufsz, &pos);
	if (ret < 0) {
		free(buf);
		return ret;
	}

	noff = offsetof(struct dssh_enc_s, name);
	build_namelist(gconf.enc_head, noff, namelist,
	    sizeof(namelist));
	ret = serialize_namelist_from_str(namelist, buf, bufsz, &pos);
	if (ret < 0) {
		free(buf);
		return ret;
	}
	ret = serialize_namelist_from_str(namelist, buf, bufsz, &pos);
	if (ret < 0) {
		free(buf);
		return ret;
	}

	noff = offsetof(struct dssh_mac_s, name);
	build_namelist(gconf.mac_head, noff, namelist,
	    sizeof(namelist));
	ret = serialize_namelist_from_str(namelist, buf, bufsz, &pos);
	if (ret < 0) {
		free(buf);
		return ret;
	}
	ret = serialize_namelist_from_str(namelist, buf, bufsz, &pos);
	if (ret < 0) {
		free(buf);
		return ret;
	}

	noff = offsetof(struct dssh_comp_s, name);
	build_namelist(gconf.comp_head, noff, namelist,
	    sizeof(namelist));
	ret = serialize_namelist_from_str(namelist, buf, bufsz, &pos);
	if (ret < 0) {
		free(buf);
		return ret;
	}
	ret = serialize_namelist_from_str(namelist, buf, bufsz, &pos);
	if (ret < 0) {
		free(buf);
		return ret;
	}

	/* Languages (empty) */
	ret = serialize_namelist_from_str("", buf, bufsz, &pos);
	if (ret < 0) {
		free(buf);
		return ret;
	}
	ret = serialize_namelist_from_str("", buf, bufsz, &pos);
	if (ret < 0) {
		free(buf);
		return ret;
	}

	/* first_kex_packet_follows = false */
	buf[pos++] = 0;

	/* reserved */
	DSSH_PUT_U32(0, buf, &pos);

	*buf_out = buf;
	*pos_out = pos;
	return 0;
}

/*
 * Wait for the peer's KEXINIT.  On peer-initiated rekey the
 * peer_kexinit is already stored, so this returns immediately.
 * Non-KEXINIT messages (types 50+) received while waiting are
 * buffered in the rekey queue for replay after rekey completes.
 */
static int
receive_peer_kexinit(struct dssh_session_s *sess)
{
	if (sess->trans.peer_kexinit != NULL)
		return 0;

	uint8_t  msg_type;
	uint8_t *peer_payload;
	size_t   peer_len;

	for (;;) {
		int res = recv_packet(sess, &msg_type,
		        &peer_payload, &peer_len);
		if (res < 0)
			return res;
		if (msg_type == SSH_MSG_KEXINIT)
			break;

		/* Buffer for replay after rekey completes */
		struct dssh_rekey_msg *rmsg = malloc(
		    sizeof(struct dssh_rekey_msg) + peer_len);
		if (rmsg == NULL)
			return DSSH_ERROR_ALLOC;
		rmsg->next = NULL;
		rmsg->msg_type = msg_type;
		rmsg->len = peer_len;
		memcpy(rmsg->data, peer_payload, peer_len);
		if (sess->trans.rekey_queue_tail != NULL)
			sess->trans.rekey_queue_tail->next = rmsg;
		else
			sess->trans.rekey_queue_head = rmsg;
		sess->trans.rekey_queue_tail = rmsg;
	}

	sess->trans.peer_kexinit = malloc(peer_len);
	if (sess->trans.peer_kexinit == NULL)
		return DSSH_ERROR_ALLOC;
	memcpy(sess->trans.peer_kexinit, peer_payload, peer_len);
	sess->trans.peer_kexinit_sz = peer_len;
	return 0;
}

/*
 * Parse a raw KEXINIT packet into 10 name-lists and the
 * first_kex_packet_follows flag.  Pure function -- no session
 * state, suitable for unit testing.
 *
 * buf/bufsz is the raw KEXINIT payload (starting with msg_type byte).
 * lists must have room for DSSH_KEXINIT_NAMELIST_COUNT entries of
 * DSSH_NAMELIST_BUF_SIZE each.
 *
 * Validates: control characters, individual name lengths, buffer bounds.
 */
DSSH_TESTABLE int
parse_peer_kexinit(const uint8_t *buf, size_t bufsz,
    char lists[][DSSH_NAMELIST_BUF_SIZE], bool *first_kex_follows)
{
	if (bufsz < 1 + DSSH_KEXINIT_COOKIE_SIZE)
		return DSSH_ERROR_PARSE;

	size_t ppos = 1 + DSSH_KEXINIT_COOKIE_SIZE;

	for (int i = 0; i < DSSH_KEXINIT_NAMELIST_COUNT; i++) {
		if (bufsz - ppos < 4)
			return DSSH_ERROR_PARSE;
		uint32_t nlen = DSSH_GET_U32(&buf[ppos]);
		ppos += 4;

		if (ppos + nlen > bufsz)
			return DSSH_ERROR_PARSE;

		/* RFC 4251 s6: reject control characters and DEL */
		for (uint32_t j = 0; j < nlen; j++) {
			uint8_t ch = buf[ppos + j];

			if ((ch <= ' ') || (ch >= DSSH_ASCII_DEL))
				return DSSH_ERROR_PARSE;
		}

		/* RFC 4251 s6: individual names MUST NOT exceed
		 * DSSH_ALGO_NAME_MAX */
		{
			uint32_t name_start = 0;

			for (uint32_t j = 0; ; j++) {
				if ((j == nlen)
				    || (buf[ppos + j] == ',')) {
					if (j - name_start
					    > DSSH_ALGO_NAME_MAX)
						return DSSH_ERROR_PARSE;
					if (j == nlen)
						break;
					name_start = j + 1;
				}
			}
		}

		size_t copylen = nlen < sizeof(lists[i]) - 1
		    ? nlen : sizeof(lists[i]) - 1;
		memcpy(lists[i], &buf[ppos], copylen);
		lists[i][copylen] = 0;
		ppos += nlen;
	}

	*first_kex_follows = false;
	if (ppos < bufsz)
		*first_kex_follows = (buf[ppos] != 0);

	return 0;
}

/*
 * Negotiate all 8 algorithm categories using the parsed peer
 * name-lists and our global registry.  On success, all 8
 * _selected pointers are set on sess->trans.  Handles the
 * first_kex_packet_follows discard if the peer guessed wrong.
 */
static int
negotiate_algorithms(struct dssh_session_s *sess,
    char peer_lists[][DSSH_NAMELIST_BUF_SIZE],
    bool peer_first_kex_follows)
{
	const char *client_kex, *server_kex;
	const char *client_hostkey, *server_hostkey;
	const char *client_enc_c2s, *server_enc_c2s;
	const char *client_enc_s2c, *server_enc_s2c;
	const char *client_mac_c2s, *server_mac_c2s;
	const char *client_mac_s2c, *server_mac_s2c;
	const char *client_comp_c2s, *server_comp_c2s;
	const char *client_comp_s2c, *server_comp_s2c;
	char        our_lists[5][DSSH_NAMELIST_BUF_SIZE];

	build_namelist(gconf.kex_head,
	    offsetof(struct dssh_kex_s, name),
	    our_lists[0], sizeof(our_lists[0]));
	build_namelist(gconf.key_algo_head,
	    offsetof(struct dssh_key_algo_s, name),
	    our_lists[1], sizeof(our_lists[1]));
	build_namelist(gconf.enc_head,
	    offsetof(struct dssh_enc_s, name),
	    our_lists[2], sizeof(our_lists[2]));
	build_namelist(gconf.mac_head,
	    offsetof(struct dssh_mac_s, name),
	    our_lists[3], sizeof(our_lists[3]));
	build_namelist(gconf.comp_head,
	    offsetof(struct dssh_comp_s, name),
	    our_lists[4], sizeof(our_lists[4]));

	if (sess->trans.client) {
		client_kex = our_lists[0];
		server_kex = peer_lists[0];
		client_hostkey = our_lists[1];
		server_hostkey = peer_lists[1];
		client_enc_c2s = our_lists[2];
		server_enc_c2s = peer_lists[2];
		client_enc_s2c = our_lists[2];
		server_enc_s2c = peer_lists[3];
		client_mac_c2s = our_lists[3];
		server_mac_c2s = peer_lists[4];
		client_mac_s2c = our_lists[3];
		server_mac_s2c = peer_lists[5];
		client_comp_c2s = our_lists[4];
		server_comp_c2s = peer_lists[6];
		client_comp_s2c = our_lists[4];
		server_comp_s2c = peer_lists[7];
	}
	else {
		client_kex = peer_lists[0];
		server_kex = our_lists[0];
		client_hostkey = peer_lists[1];
		server_hostkey = our_lists[1];
		client_enc_c2s = peer_lists[2];
		server_enc_c2s = our_lists[2];
		client_enc_s2c = peer_lists[3];
		server_enc_s2c = our_lists[2];
		client_mac_c2s = peer_lists[4];
		server_mac_c2s = our_lists[3];
		client_mac_s2c = peer_lists[5];
		server_mac_s2c = our_lists[3];
		client_comp_c2s = peer_lists[6];
		server_comp_c2s = our_lists[4];
		client_comp_s2c = peer_lists[7];
		server_comp_s2c = our_lists[4];
	}

	sess->trans.kex_selected = negotiate_algo(
	    client_kex, server_kex,
	    gconf.kex_head, offsetof(struct dssh_kex_s, name));
	sess->trans.key_algo_selected = negotiate_algo(
	    client_hostkey, server_hostkey,
	    gconf.key_algo_head,
	    offsetof(struct dssh_key_algo_s, name));
	sess->trans.enc_c2s_selected = negotiate_algo(
	    client_enc_c2s, server_enc_c2s,
	    gconf.enc_head, offsetof(struct dssh_enc_s, name));
	sess->trans.enc_s2c_selected = negotiate_algo(
	    client_enc_s2c, server_enc_s2c,
	    gconf.enc_head, offsetof(struct dssh_enc_s, name));
	sess->trans.mac_c2s_selected = negotiate_algo(
	    client_mac_c2s, server_mac_c2s,
	    gconf.mac_head, offsetof(struct dssh_mac_s, name));
	sess->trans.mac_s2c_selected = negotiate_algo(
	    client_mac_s2c, server_mac_s2c,
	    gconf.mac_head, offsetof(struct dssh_mac_s, name));
	sess->trans.comp_c2s_selected = negotiate_algo(
	    client_comp_c2s, server_comp_c2s,
	    gconf.comp_head, offsetof(struct dssh_comp_s, name));
	sess->trans.comp_s2c_selected = negotiate_algo(
	    client_comp_s2c, server_comp_s2c,
	    gconf.comp_head, offsetof(struct dssh_comp_s, name));

	if ((sess->trans.kex_selected == NULL)
	    || (sess->trans.key_algo_selected == NULL)
	    || (sess->trans.enc_c2s_selected == NULL)
	    || (sess->trans.enc_s2c_selected == NULL)
	    || (sess->trans.mac_c2s_selected == NULL)
	    || (sess->trans.mac_s2c_selected == NULL)
	    || (sess->trans.comp_c2s_selected == NULL)
	    || (sess->trans.comp_s2c_selected == NULL)) {
		dssh_transport_disconnect(sess,
		    SSH_DISCONNECT_KEY_EXCHANGE_FAILED,
		    "no common algorithm");
		return DSSH_ERROR_INVALID;
	}

	/* Handle first_kex_packet_follows (RFC 4253 s7.1):
	 * If the peer sent a guessed KEX packet and the guess was
	 * wrong (negotiated != peer's first choice), discard it. */
	if (peer_first_kex_follows) {
		char peer_guess_kex[DSSH_ALGO_NAME_MAX + 1];
		char peer_guess_hostkey[DSSH_ALGO_NAME_MAX + 1];

		first_name(peer_lists[0], peer_guess_kex,
		    sizeof(peer_guess_kex));
		first_name(peer_lists[1], peer_guess_hostkey,
		    sizeof(peer_guess_hostkey));

		if ((strcmp(sess->trans.kex_selected->name,
		    peer_guess_kex) != 0)
		    || (strcmp(sess->trans.key_algo_selected->name,
		    peer_guess_hostkey) != 0)) {
			uint8_t  discard_type;
			uint8_t *discard_payload;
			size_t   discard_len;
			int res = recv_packet_raw(sess, &discard_type,
			    &discard_payload, &discard_len);

			if (res < 0)
				return res;
		}
	}

	return 0;
}

DSSH_TESTABLE int
kexinit(struct dssh_session_s *sess)
{
	uint8_t *kexinit;
	size_t   kexinit_sz;
	int      res = build_kexinit_packet(sess, &kexinit, &kexinit_sz);

	if (res < 0)
		return res;

	/* Save our KEXINIT for exchange hash -- transfer ownership */
	free(sess->trans.our_kexinit);
	sess->trans.our_kexinit = kexinit;
	sess->trans.our_kexinit_sz = kexinit_sz;

	res = send_packet(sess, kexinit, kexinit_sz, NULL);
	if (res < 0)
		return res;

	res = receive_peer_kexinit(sess);
	if (res < 0)
		return res;

	char peer_lists[DSSH_KEXINIT_NAMELIST_COUNT][DSSH_NAMELIST_BUF_SIZE];
	bool peer_first_kex_follows;

	res = parse_peer_kexinit(sess->trans.peer_kexinit,
	    sess->trans.peer_kexinit_sz, peer_lists,
	    &peer_first_kex_follows);
	if (res < 0)
		return res;

	return negotiate_algorithms(sess, peer_lists,
	    peer_first_kex_follows);
}

/* ================================================================
 * Key exchange (RFC 4253 s7)
 * ================================================================ */

static int
kex_send_wrapper(const uint8_t *payload, size_t len, void *io_ctx)
{
	return send_packet((struct dssh_session_s *)io_ctx,
	    payload, len, NULL);
}

static int
kex_recv_wrapper(uint8_t *msg_type, uint8_t **payload,
    size_t *payload_len, void *io_ctx)
{
	return recv_packet((struct dssh_session_s *)io_ctx,
	    msg_type, payload, payload_len);
}

DSSH_TESTABLE int
kex(struct dssh_session_s *sess)
{
	if ((sess->trans.kex_selected == NULL)
	    || (sess->trans.kex_selected->handler == NULL))
		return DSSH_ERROR_INIT;

	struct dssh_kex_context kctx = {
		.client    = sess->trans.client,
		.v_c       = sess->trans.client ?
		    sess->trans.id_str : sess->trans.remote_id_str,
		.v_c_len   = sess->trans.client ?
		    sess->trans.id_str_sz : sess->trans.remote_id_str_sz,
		.v_s       = sess->trans.client ?
		    sess->trans.remote_id_str : sess->trans.id_str,
		.v_s_len   = sess->trans.client ?
		    sess->trans.remote_id_str_sz : sess->trans.id_str_sz,
		.i_c       = sess->trans.client ?
		    sess->trans.our_kexinit : sess->trans.peer_kexinit,
		.i_c_len   = sess->trans.client ?
		    sess->trans.our_kexinit_sz : sess->trans.peer_kexinit_sz,
		.i_s       = sess->trans.client ?
		    sess->trans.peer_kexinit : sess->trans.our_kexinit,
		.i_s_len   = sess->trans.client ?
		    sess->trans.peer_kexinit_sz : sess->trans.our_kexinit_sz,
		.key_algo  = sess->trans.key_algo_selected,
		.kex_data  = sess->trans.kex_selected->ctx,
		.send      = kex_send_wrapper,
		.recv      = kex_recv_wrapper,
		.io_ctx    = sess,
	};

	int res = sess->trans.kex_selected->handler(&kctx);

	if (res == 0) {
		sess->trans.shared_secret = kctx.shared_secret;
		sess->trans.shared_secret_sz = kctx.shared_secret_sz;
		sess->trans.exchange_hash = kctx.exchange_hash;
		sess->trans.exchange_hash_sz = kctx.exchange_hash_sz;
	}
	else {
		free(kctx.shared_secret);
		free(kctx.exchange_hash);
	}

	return res;
}

/* ================================================================
 * Key derivation and NEWKEYS (RFC 4253 s7.2, s7.3)
 * ================================================================ */

DSSH_TESTABLE int
derive_key(const char *hash_name,
    const uint8_t *shared_secret, size_t shared_secret_sz,
    const uint8_t *hash, size_t hash_sz,
    uint8_t letter,
    const uint8_t *session_id, size_t session_id_sz,
    uint8_t *out, size_t needed)
{
	dssh_hash_ctx *ctx = NULL;
	size_t md_len;
	int res = dssh_hash_init(&ctx, hash_name, &md_len);

	if (res < 0)
		return res;

	uint8_t *tmp = malloc(md_len);

	if (tmp == NULL) {
		dssh_hash_free(ctx);
		return DSSH_ERROR_ALLOC;
	}

	size_t have = 0;

#define DK(call) do { res = (call); if (res < 0) goto cleanup; } while (0)

	/* Initial hash: H(K || H || letter || session_id) */
	DK(dssh_hash_update(ctx, shared_secret, shared_secret_sz));
	DK(dssh_hash_update(ctx, hash, hash_sz));
	DK(dssh_hash_update(ctx, &letter, 1));
	DK(dssh_hash_update(ctx, session_id, session_id_sz));
	DK(dssh_hash_final(ctx, tmp, md_len));

	size_t copy = md_len < needed ? md_len : needed;

	memcpy(out, tmp, copy);
	have = copy;

	/* Extension loop: H(K || H || K1 || K2 || ...) */
	while (have < needed) {
		DK(dssh_hash_update(ctx, shared_secret,
		    shared_secret_sz));
		DK(dssh_hash_update(ctx, hash, hash_sz));
		DK(dssh_hash_update(ctx, out, have));
		DK(dssh_hash_final(ctx, tmp, md_len));
		copy = md_len < (needed - have)
		    ? md_len : (needed - have);
		memcpy(&out[have], tmp, copy);
		have += copy;
	}

#undef DK

	res = 0;

cleanup:
	dssh_cleanse(tmp, md_len);
	free(tmp);
	dssh_hash_free(ctx);
	return res;
}

/*
 * Encode shared secret K into wire format for key derivation.
 * Pure function -- no session state, suitable for unit testing.
 *
 * Most KEX methods encode K as mpint (RFC 4253 s7):
 *   uint32 length, sign-padded big-endian integer.
 * Hybrid post-quantum methods encode K as string (fixed-length,
 * no sign padding) per draft-josefsson-ntruprime-ssh-02.
 *
 * On success, *out is a malloc'd buffer and *out_sz is its size.
 */
DSSH_TESTABLE int
encode_k_wire(const uint8_t *raw, size_t raw_sz,
    bool k_as_string, uint8_t **out, size_t *out_sz)
{
	size_t k_data_len;

	if (k_as_string) {
		k_data_len = raw_sz;
	}
	else {
		bool need_pad = (raw_sz > 0
		    && (raw[0] & DSSH_MPINT_SIGN_BIT));
		k_data_len = raw_sz + (need_pad ? 1 : 0);
	}

	size_t   k_wire_len = 4 + k_data_len;
	uint8_t *buf = malloc(k_wire_len);

	if (buf == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;

	DSSH_PUT_U32((uint32_t)k_data_len, buf, &pos);
	if (!k_as_string && k_data_len > raw_sz)
		buf[pos++] = 0; /* mpint sign-bit padding */
	memcpy(&buf[pos], raw, raw_sz);

	*out = buf;
	*out_sz = k_wire_len;
	return 0;
}

/*
 * Derive 6 session keys, apply them (init new cipher/MAC contexts,
 * clean up old ones), allocate MAC verification buffers, reset
 * counters, and free KEXINIT buffers.  Key material is local and
 * cleansed on exit.
 */
static int
derive_and_apply_keys(struct dssh_session_s *sess, const uint8_t *k_mpint,
    size_t k_mpint_sz,
    dssh_enc old_enc_c2s, dssh_enc old_enc_s2c,
    dssh_mac old_mac_c2s, dssh_mac old_mac_s2c)
{
	const char *hash_name = sess->trans.kex_selected->hash_name;
	uint16_t    enc_key_c2s = sess->trans.enc_c2s_selected->key_size;
	uint16_t    enc_key_s2c = sess->trans.enc_s2c_selected->key_size;
	uint16_t    enc_bs_c2s = sess->trans.enc_c2s_selected->blocksize;
	uint16_t    enc_bs_s2c = sess->trans.enc_s2c_selected->blocksize;
	uint16_t    mac_key_c2s = sess->trans.mac_c2s_selected->key_size;
	uint16_t    mac_key_s2c = sess->trans.mac_s2c_selected->key_size;
	uint8_t    *iv_c2s = NULL;
	uint8_t    *iv_s2c = NULL;
	uint8_t    *key_c2s = NULL;
	uint8_t    *key_s2c = NULL;
	uint8_t    *integ_c2s = NULL;
	uint8_t    *integ_s2c = NULL;
	int         res;

	iv_c2s = malloc(enc_bs_c2s);
	iv_s2c = malloc(enc_bs_s2c);
	key_c2s = malloc(enc_key_c2s);
	key_s2c = malloc(enc_key_s2c);
	integ_c2s = calloc(1, mac_key_c2s > 0 ? mac_key_c2s : 1);
	integ_s2c = calloc(1, mac_key_s2c > 0 ? mac_key_s2c : 1);

	if (!iv_c2s || !iv_s2c || !key_c2s || !key_s2c
	    || !integ_c2s || !integ_s2c) {
		res = DSSH_ERROR_ALLOC;
		goto keys_cleanup;
	}

	/* Derive keys A-F (RFC 4253 s7.2) */
	const uint8_t *H = sess->trans.exchange_hash;
	size_t         H_sz = sess->trans.exchange_hash_sz;
	const uint8_t *sid = sess->trans.session_id;
	size_t         sid_sz = sess->trans.session_id_sz;

	res = derive_key(hash_name, k_mpint, k_mpint_sz,
	    H, H_sz, 'A', sid, sid_sz, iv_c2s, enc_bs_c2s);
	if (res == 0)
		res = derive_key(hash_name, k_mpint, k_mpint_sz,
		    H, H_sz, 'B', sid, sid_sz, iv_s2c, enc_bs_s2c);
	if (res == 0)
		res = derive_key(hash_name, k_mpint, k_mpint_sz,
		    H, H_sz, 'C', sid, sid_sz, key_c2s, enc_key_c2s);
	if (res == 0)
		res = derive_key(hash_name, k_mpint, k_mpint_sz,
		    H, H_sz, 'D', sid, sid_sz, key_s2c, enc_key_s2c);
	if ((res == 0) && (mac_key_c2s > 0))
		res = derive_key(hash_name, k_mpint, k_mpint_sz,
		    H, H_sz, 'E', sid, sid_sz, integ_c2s, mac_key_c2s);
	if ((res == 0) && (mac_key_s2c > 0))
		res = derive_key(hash_name, k_mpint, k_mpint_sz,
		    H, H_sz, 'F', sid, sid_sz, integ_s2c, mac_key_s2c);

	if (res < 0)
		goto keys_cleanup;

	/* Free old cipher/MAC contexts (rekey case -- during initial
	 * key exchange these are NULL so the cleanup calls are no-ops). */
	if ((sess->trans.enc_c2s_ctx != NULL) && (old_enc_c2s != NULL)) {
		if (old_enc_c2s->cleanup != NULL)
			old_enc_c2s->cleanup(sess->trans.enc_c2s_ctx);
		sess->trans.enc_c2s_ctx = NULL;
	}
	if ((sess->trans.enc_s2c_ctx != NULL) && (old_enc_s2c != NULL)) {
		if (old_enc_s2c->cleanup != NULL)
			old_enc_s2c->cleanup(sess->trans.enc_s2c_ctx);
		sess->trans.enc_s2c_ctx = NULL;
	}
	if ((sess->trans.mac_c2s_ctx != NULL) && (old_mac_c2s != NULL)) {
		if (old_mac_c2s->cleanup != NULL)
			old_mac_c2s->cleanup(sess->trans.mac_c2s_ctx);
		sess->trans.mac_c2s_ctx = NULL;
	}
	if ((sess->trans.mac_s2c_ctx != NULL) && (old_mac_s2c != NULL)) {
		if (old_mac_s2c->cleanup != NULL)
			old_mac_s2c->cleanup(sess->trans.mac_s2c_ctx);
		sess->trans.mac_s2c_ctx = NULL;
	}

	/* Initialize new encryption contexts */
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
	if ((mac_key_c2s > 0)
	    && (sess->trans.mac_c2s_selected->init != NULL)) {
		res = sess->trans.mac_c2s_selected->init(integ_c2s,
		    &sess->trans.mac_c2s_ctx);
		if (res < 0)
			goto keys_cleanup;
	}
	if ((mac_key_s2c > 0)
	    && (sess->trans.mac_s2c_selected->init != NULL)) {
		res = sess->trans.mac_s2c_selected->init(integ_s2c,
		    &sess->trans.mac_s2c_ctx);
		if (res < 0)
			goto keys_cleanup;
	}

	/* Allocate MAC verification buffers.
	 * rx direction is s2c for clients, c2s for servers. */
	uint16_t mac_digest_rx = sess->trans.client
	    ? sess->trans.mac_s2c_selected->digest_size
	    : sess->trans.mac_c2s_selected->digest_size;

	if (mac_digest_rx > 0) {
		free(sess->trans.rx_mac_buf);
		sess->trans.rx_mac_buf = NULL;
		free(sess->trans.rx_mac_computed);
		sess->trans.rx_mac_computed = NULL;
		sess->trans.rx_mac_buf = malloc(mac_digest_rx);
		if (sess->trans.rx_mac_buf == NULL) {
			res = DSSH_ERROR_ALLOC;
			goto keys_cleanup;
		}
		sess->trans.rx_mac_computed = malloc(mac_digest_rx);
		if (sess->trans.rx_mac_computed == NULL) {
			res = DSSH_ERROR_ALLOC;
			goto keys_cleanup;
		}
	}

	/* Reset per-key counters */
	atomic_store(&sess->trans.tx_since_rekey, 0);
	atomic_store(&sess->trans.tx_bytes_since_rekey, (uint64_t)0);
	sess->trans.rx_since_rekey = 0;
	sess->trans.rx_bytes_since_rekey = 0;
	sess->trans.rekey_time = time(NULL);

	free(sess->trans.our_kexinit);
	sess->trans.our_kexinit = NULL;
	sess->trans.our_kexinit_sz = 0;
	free(sess->trans.peer_kexinit);
	sess->trans.peer_kexinit = NULL;
	sess->trans.peer_kexinit_sz = 0;

keys_cleanup:
	cleanse_free(iv_c2s, enc_bs_c2s);
	cleanse_free(iv_s2c, enc_bs_s2c);
	cleanse_free(key_c2s, enc_key_c2s);
	cleanse_free(key_s2c, enc_key_s2c);
	cleanse_free(integ_c2s, mac_key_c2s);
	cleanse_free(integ_s2c, mac_key_s2c);
	return res;
}

DSSH_TESTABLE int
newkeys(struct dssh_session_s *sess)
{
	/* Save references to OLD modules for cleanup after rekey */
	dssh_enc old_enc_c2s = sess->trans.enc_c2s_selected;
	dssh_enc old_enc_s2c = sess->trans.enc_s2c_selected;
	dssh_mac old_mac_c2s = sess->trans.mac_c2s_selected;
	dssh_mac old_mac_s2c = sess->trans.mac_s2c_selected;

	uint8_t newkeys_msg = SSH_MSG_NEWKEYS;
	int     res = send_packet(sess, &newkeys_msg,
	    1, NULL);

	if (res < 0)
		return res;

	uint8_t  msg_type;
	uint8_t *payload;
	size_t   payload_len;

	res = recv_packet(sess, &msg_type, &payload,
	    &payload_len);
	if (res < 0)
		return res;
	if (msg_type != SSH_MSG_NEWKEYS)
		return DSSH_ERROR_PARSE;

	/* Save session ID on first KEX */
	if (sess->trans.session_id == NULL) {
		sess->trans.session_id =
		    malloc(sess->trans.exchange_hash_sz);
		if (sess->trans.session_id == NULL)
			return DSSH_ERROR_ALLOC;
		memcpy(sess->trans.session_id,
		    sess->trans.exchange_hash,
		    sess->trans.exchange_hash_sz);
		sess->trans.session_id_sz =
		    sess->trans.exchange_hash_sz;
	}

	/* Encode shared secret K for key derivation */
	bool     k_as_string = (sess->trans.kex_selected->flags
	    & DSSH_KEX_FLAG_K_ENCODING_STRING) != 0;
	uint8_t *k_mpint;
	size_t   k_mpint_sz;

	res = encode_k_wire(sess->trans.shared_secret,
	    sess->trans.shared_secret_sz, k_as_string,
	    &k_mpint, &k_mpint_sz);
	if (res < 0)
		return res;

	/* Derive keys, init contexts, reset counters */
	res = derive_and_apply_keys(sess, k_mpint, k_mpint_sz,
	    old_enc_c2s, old_enc_s2c, old_mac_c2s, old_mac_s2c);
	free(k_mpint);
	return res;
}

/* ================================================================
 * Init / cleanup / registration
 * ================================================================ */

static int
transport_init(struct dssh_session_s *sess, size_t max_packet_size)
{
	gconf.used = true;
	if (gconf.software_version == NULL)
		gconf.software_version = sw_ver;

	if (max_packet_size < SSH_BPP_PACKET_SIZE_MIN)
		max_packet_size = SSH_BPP_PACKET_SIZE_MIN;
	if (max_packet_size > SSH_BPP_PACKET_SIZE_MAX)
		max_packet_size = SSH_BPP_PACKET_SIZE_MAX;

	int  ret = DSSH_ERROR_ALLOC;
	bool tx_mtx_ok = false;
	bool tx_queue_mtx_ok = false;
	bool rx_mtx_ok = false;

	sess->trans.packet_buf_sz = max_packet_size;
	sess->trans.tx_packet = malloc(4 + max_packet_size);
	if (!sess->trans.tx_packet)
		goto init_cleanup;
	sess->trans.rx_packet = malloc(4 + max_packet_size);
	if (!sess->trans.rx_packet)
		goto init_cleanup;

	ret = DSSH_ERROR_INIT;
	if (mtx_init(&sess->trans.tx_mtx, mtx_plain) != thrd_success)
		goto init_cleanup;
	tx_mtx_ok = true;
	if (mtx_init(&sess->trans.tx_queue_mtx, mtx_plain) != thrd_success)
		goto init_cleanup;
	tx_queue_mtx_ok = true;
	if (mtx_init(&sess->trans.rx_mtx, mtx_plain) != thrd_success)
		goto init_cleanup;
	rx_mtx_ok = true;
	if (cnd_init(&sess->trans.rekey_cnd) != thrd_success)
		goto init_cleanup;

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
	sess->trans.tx_queue_head = NULL;
	sess->trans.tx_queue_tail = NULL;
	sess->trans.session_id = NULL;
	sess->trans.our_kexinit = NULL;
	sess->trans.peer_kexinit = NULL;
	sess->trans.shared_secret = NULL;
	sess->trans.exchange_hash = NULL;
	sess->trans.tx_seq = 0;
	sess->trans.rx_seq = 0;
	atomic_init(&sess->trans.tx_since_rekey, 0);
	sess->trans.rx_since_rekey = 0;
	atomic_init(&sess->trans.tx_bytes_since_rekey, (uint64_t)0);
	sess->trans.rx_bytes_since_rekey = 0;
	sess->trans.rekey_time = time(NULL);

	return 0;

init_cleanup:
	if (rx_mtx_ok)
		mtx_destroy(&sess->trans.rx_mtx);
	if (tx_queue_mtx_ok)
		mtx_destroy(&sess->trans.tx_queue_mtx);
	if (tx_mtx_ok)
		mtx_destroy(&sess->trans.tx_mtx);
	free(sess->trans.tx_packet);
	sess->trans.tx_packet = NULL;
	free(sess->trans.rx_packet);
	sess->trans.rx_packet = NULL;
	return ret;
}

static void
transport_cleanup(struct dssh_session_s *sess)
{
	if (sess->trans.kex_selected)
		sess->trans.kex_selected = NULL;
	if (sess->trans.key_algo_selected)
		sess->trans.key_algo_selected = NULL;
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

	cnd_destroy(&sess->trans.rekey_cnd);
	mtx_destroy(&sess->trans.tx_mtx);
	mtx_destroy(&sess->trans.tx_queue_mtx);
	mtx_destroy(&sess->trans.rx_mtx);

        /* Free tx queue (non-empty only if session terminated mid-flight) */
	{
		struct dssh_tx_queue_entry *tqe = sess->trans.tx_queue_head;

		while (tqe != NULL) {
			struct dssh_tx_queue_entry *next = tqe->next;

			free(tqe);
			tqe = next;
		}
		sess->trans.tx_queue_head = NULL;
		sess->trans.tx_queue_tail = NULL;
	}

        /* Free rekey queue (non-empty only if rekey failed mid-flight) */
	{
		struct dssh_rekey_msg *rmsg = sess->trans.rekey_queue_head;

		while (rmsg != NULL) {
			struct dssh_rekey_msg *next = rmsg->next;

			free(rmsg);
			rmsg = next;
		}
		sess->trans.rekey_queue_head = NULL;
		sess->trans.rekey_queue_tail = NULL;
	}

	free(sess->trans.tx_packet);
	sess->trans.tx_packet = NULL;
	free(sess->trans.rx_packet);
	sess->trans.rx_packet = NULL;
	free(sess->trans.rx_mac_buf);
	sess->trans.rx_mac_buf = NULL;
	free(sess->trans.rx_mac_computed);
	sess->trans.rx_mac_computed = NULL;
	sess->trans.packet_buf_sz = 0;

	cleanse_free(sess->trans.session_id, sess->trans.session_id_sz);
	sess->trans.session_id = NULL;
	free(sess->trans.our_kexinit);
	sess->trans.our_kexinit = NULL;
	free(sess->trans.peer_kexinit);
	sess->trans.peer_kexinit = NULL;
	cleanse_free(sess->trans.shared_secret, sess->trans.shared_secret_sz);
	sess->trans.shared_secret = NULL;
	cleanse_free(sess->trans.exchange_hash, sess->trans.exchange_hash_sz);
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

/* ================================================================
 * Session lifecycle (formerly in ssh.c)
 * ================================================================ */

DSSH_PUBLIC struct dssh_session_s *
dssh_session_init(bool client, size_t max_packet_size)
{
	struct dssh_session_s *sess = calloc(1, sizeof(*sess));

	if (sess == NULL)
		return NULL;

	sess->trans.client = client;

	int res = mtx_init(&sess->mtx, mtx_plain);

	if (res != thrd_success) {
		free(sess);
		return NULL;
	}

	res = transport_init(sess, max_packet_size);
	if (res < 0) {
		mtx_destroy(&sess->mtx);
		free(sess);
		return NULL;
	}

	sess->timeout_ms = 75000;
	sess->initialized = true;
	return sess;
}

DSSH_PUBLIC bool
dssh_session_terminate(struct dssh_session_s *sess)
{
	if (sess == NULL)
		return false;

	bool t = true;

	if (atomic_compare_exchange_strong(&sess->initialized, &t, false)) {
		session_set_terminate(sess);
		return true;
	}
	return false;
}

DSSH_PUBLIC bool
dssh_session_is_terminated(struct dssh_session_s *sess)
{
	if (sess == NULL)
		return true;
	return sess->terminate;
}

DSSH_PUBLIC void
dssh_session_cleanup(struct dssh_session_s *sess)
{
	if (sess == NULL)
		return;
	dssh_session_terminate(sess);
	dssh_session_stop(sess);
	transport_cleanup(sess);
	free(sess->pending_banner);
	free(sess->pending_banner_lang);
	mtx_destroy(&sess->mtx);
	free(sess);
}

DSSH_PUBLIC int
dssh_key_algo_set_ctx(const char *name, void *ctx)
{
	if (name == NULL)
		return DSSH_ERROR_INVALID;
	/* Refuse after first session init -- the global registry is
	 * read concurrently by active sessions during KEX. */
	if (gconf.used)
		return DSSH_ERROR_TOOLATE;
	dssh_key_algo ka = find_key_algo(name);

	if (ka == NULL)
		return DSSH_ERROR_INIT;
	ka->ctx = (dssh_key_algo_ctx *)ctx;
	return 0;
}

DSSH_PUBLIC int
dssh_kex_set_ctx(const char *name, void *ctx)
{
	if (name == NULL)
		return DSSH_ERROR_INVALID;
	/* Refuse after first session init -- the global registry is
	 * read concurrently by active sessions during KEX. */
	if (gconf.used)
		return DSSH_ERROR_TOOLATE;
	dssh_kex k = find_kex(name);

	if (k == NULL)
		return DSSH_ERROR_INIT;
	k->ctx = ctx;
	return 0;
}

#define DEFINE_REGISTER(func_name, param_type, head, tail, entries) \
DSSH_PUBLIC int                                                     \
func_name(param_type item)                                          \
{                                                                   \
	if (item == NULL)                                           \
		return DSSH_ERROR_INVALID;                          \
	if (gconf.used)                                             \
		return DSSH_ERROR_TOOLATE;                          \
	if (strlen(item->name) == 0)                                \
		return DSSH_ERROR_INVALID;                          \
	if (strlen(item->name) > 64)                                \
		return DSSH_ERROR_TOOLONG;                          \
	if (item->next)                                             \
		return DSSH_ERROR_MUST_BE_NULL;                     \
	if (gconf.entries + 1 == SIZE_MAX)                          \
		return DSSH_ERROR_TOOMANY;                          \
	if (gconf.head == NULL)                                     \
		gconf.head = item;                                  \
	if (gconf.tail != NULL)                                     \
		gconf.tail->next = item;                            \
	gconf.tail = item;                                          \
	gconf.entries++;                                            \
	return 0;                                                   \
}

DEFINE_REGISTER(dssh_transport_register_kex,      dssh_kex,      kex_head,      kex_tail,      kex_entries)
DEFINE_REGISTER(dssh_transport_register_key_algo, dssh_key_algo, key_algo_head, key_algo_tail, key_algo_entries)
DEFINE_REGISTER(dssh_transport_register_enc,      dssh_enc,      enc_head,      enc_tail,      enc_entries)
DEFINE_REGISTER(dssh_transport_register_mac,      dssh_mac,      mac_head,      mac_tail,      mac_entries)
DEFINE_REGISTER(dssh_transport_register_comp,     dssh_comp,     comp_head,     comp_tail,     comp_entries)
DEFINE_REGISTER(dssh_transport_register_lang,     dssh_language,  lang_head,     lang_tail,     lang_entries)
#undef DEFINE_REGISTER

/*
 * Validate a softwareversion string per RFC 4253 s4.2:
 * printable US-ASCII (0x21--0x7E), no spaces (space is the
 * delimiter between softwareversion and comments).
 */
static bool
is_valid_sw_version(const char *s, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if ((s[i] < 0x21) || (s[i] > 0x7E))
			return false;
	}
	return true;
}

/*
 * Validate a comment string per RFC 4253 s4.2:
 * printable US-ASCII (0x20--0x7E).  Spaces are allowed in comments;
 * the comment runs from the SP after softwareversion to CR LF.
 */
static bool
is_valid_comment(const char *s, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if ((s[i] < 0x20) || (s[i] > 0x7E))
			return false;
	}
	return true;
}

DSSH_PUBLIC int
dssh_transport_set_version(const char *software_version,
    const char                        *comment)
{
	if (gconf.used)
		return DSSH_ERROR_TOOLATE;

        /* NULL software_version leaves the default; validate if provided */
	const char *sv = software_version != NULL ? software_version : sw_ver;
	size_t      sv_len = strlen(sv);

	if (sv_len == 0)
		return DSSH_ERROR_PARSE;
	if (!is_valid_sw_version(sv, sv_len))
		return DSSH_ERROR_INVALID;

        /* ssh_version_prefix + version + " " + comment + CR LF <= DSSH_VERSION_STRING_MAX */
	size_t total = DSSH_STRLEN(ssh_version_prefix) + sv_len + 2;

	if (comment != NULL) {
		size_t cm_len = strlen(comment);

		if (cm_len == 0)
			return DSSH_ERROR_PARSE;
		if (!is_valid_comment(comment, cm_len))
			return DSSH_ERROR_INVALID;
		total += 1 + cm_len; /* SP + comment */
	}
	if (total > DSSH_VERSION_STRING_MAX)
		return DSSH_ERROR_TOOLONG;

	gconf.software_version = sv;
	gconf.version_comment = comment;
	return 0;
}

/*
 * Default rx_line implementation: reads one byte at a time via the rx
 * callback until CR-LF is found or the buffer is full.
 *
 * Strict: every CR must be immediately followed by LF, and every LF
 * must be preceded by CR.  Returns DSSH_ERROR_PARSE on bare CR or LF.
 */
static int
rxline_from_rx(uint8_t *buf, size_t bufsz,
    size_t *bytes_received, struct dssh_session_s *sess,
    void *cbdata)
{
	size_t have = 0;
	int    res;

	while (have < bufsz) {
		res = gconf.rx(&buf[have], 1, sess, cbdata);
		if (res < 0)
			return res;
		have++;
		if (buf[have - 1] == '\r') {
			if (have >= bufsz)
				return DSSH_ERROR_TOOLONG;
			res = gconf.rx(&buf[have], 1, sess, cbdata);
			if (res < 0)
				return res;
			have++;
			if (buf[have - 1] != '\n')
				return DSSH_ERROR_PARSE;
			*bytes_received = have;
			return 0;
		}
		if (buf[have - 1] == '\n')
			return DSSH_ERROR_PARSE;
	}
	return DSSH_ERROR_TOOLONG;
}

DSSH_PUBLIC int
dssh_transport_set_callbacks(dssh_transport_io_cb tx,
    dssh_transport_io_cb                          rx,
    dssh_transport_rxline_cb                      rx_line,
    dssh_transport_extra_line_cb                  extra_line_cb)
{
	if (gconf.used)
		return DSSH_ERROR_TOOLATE;
	if (tx == NULL || rx == NULL)
		return DSSH_ERROR_INVALID;
	gconf.tx = tx;
	gconf.rx = rx;
	gconf.rx_line = (rx_line != NULL) ? rx_line : rxline_from_rx;
	gconf.extra_line_cb = extra_line_cb;
	return 0;
}

#ifdef DSSH_TESTING

/*
 * Reset the global configuration for test isolation.
 * Frees all registered algorithm entries, zeros the config,
 * and unlocks registration.
 */
DSSH_PRIVATE void
dssh_test_reset_global_config(void)
{
 #define FREE_LIST(type, head) do { \
	for (type _n = (head); _n != NULL;) { \
		type _next = _n->next; \
		free(_n); \
		_n = _next; \
	} \
} while (0)

	/* Clean up key_algo contexts before freeing */
	for (dssh_key_algo ka = gconf.key_algo_head; ka != NULL; ka = ka->next) {
		if ((ka->cleanup != NULL) && (ka->ctx != NULL))
			ka->cleanup(ka->ctx);
	}

	FREE_LIST(dssh_kex, gconf.kex_head);
	FREE_LIST(dssh_key_algo, gconf.key_algo_head);
	FREE_LIST(dssh_enc, gconf.enc_head);
	FREE_LIST(dssh_mac, gconf.mac_head);
	FREE_LIST(dssh_comp, gconf.comp_head);
	FREE_LIST(dssh_language, gconf.lang_head);
 #undef FREE_LIST

	memset(&gconf, 0, sizeof(gconf));
	gconf.used = false;
}

/*
 * Test accessors for injecting version strings that bypass
 * set_version validation, allowing version_tx defense-in-depth
 * tests to reach the TOOLONG branches.
 */
DSSH_PRIVATE void
dssh_test_set_sw_version(const char *v)
{
	gconf.software_version = v;
}

DSSH_PRIVATE void
dssh_test_set_version_comment(const char *c)
{
	gconf.version_comment = c;
}
/*
 * Provide extern definitions for the inline version validators
 * so the linker can resolve calls from test code.  In C17, an
 * inline function only emits an external definition when an
 * extern declaration appears in the same translation unit.
 */
extern inline bool has_nulls(uint8_t *buf, size_t buflen);
extern inline bool missing_crlf(uint8_t *buf, size_t buflen);
extern inline bool is_version_line(uint8_t *buf, size_t buflen);
extern inline bool has_non_ascii(uint8_t *buf, size_t buflen);
extern inline bool is_20(uint8_t *buf, size_t buflen);

#endif /* ifdef DSSH_TESTING */
