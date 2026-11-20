// RFC-4253: SSH Transport Layer Protocol

#include <stdlib.h>
#include <string.h>
#include <threads.h>
#ifdef DSSH_TESTING
 #include <limits.h>
#endif

#include "deucessh-algorithms.h"
#include "deucessh-auth.h"
#include "deucessh-crypto.h"
#include "ssh-internal.h"
#include "ssh-window.h"
#include "ssh-packet.h"
#include "key_algo/private-key-file.h"

/* DSSH_TESTABLE is defined in ssh-internal.h */

/* Forward declarations for functions defined later in this file
 * but called before their definitions. */
static int        transport_init(struct dssh_session_s *sess, size_t max_packet_size);
static void       transport_cleanup(struct dssh_session_s *sess);
static void kex_release_result(struct dssh_session_s *sess);
static void kex_cleanup_state(struct dssh_session_s *sess);
static int send_packet_inner(struct dssh_session_s *sess, const uint8_t *payload,
    size_t payload_len, uint32_t *seq_out);
DSSH_TESTABLE int kexinit(struct dssh_session_s *sess);
static int finish_kexinit(struct dssh_session_s *sess);
static int prepare_kexinit(struct dssh_session_s *sess);
DSSH_TESTABLE int kex(struct dssh_session_s *sess);
static int kex_exchange(struct dssh_session_s *sess);
DSSH_TESTABLE int newkeys(struct dssh_session_s *sess);
static int send_server_ext_info(struct dssh_session_s *sess);

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
static const char sw_ver[]             = "DeuceSSH_" DSSH_VERSION_STRING;
static const char ssh_version_prefix[] = "SSH-2.0-";

DSSH_TESTABLE struct dssh_transport_global_config gconf = {
	.software_version = sw_ver,
};

static _Thread_local struct dssh_session_s *io_callback_session;
static _Thread_local struct dssh_session_s *tx_owned_session;

/* Only the packet-engine owner stages a batch. Queued slot buffers can be
 * reused by producers as soon as they are retired, so retain their wire bytes
 * separately. The caller's final packet needs no copy. */
#define DSSH_TX_BATCH_PACKETS 16
struct dssh_tx_batch {
	struct dssh_session_s *sess;
	size_t offsets[DSSH_TX_BATCH_PACKETS];
	size_t lengths[DSSH_TX_BATCH_PACKETS];
	size_t count;
	size_t bytes;
};
static _Thread_local struct dssh_tx_batch *tx_batch;

static dssh_enc
enc_c2s_module(struct dssh_session_s *sess)
{
	if (sess->trans.enc_c2s_active != NULL)
		return sess->trans.enc_c2s_active;
	return sess->trans.enc_c2s_ctx != NULL ? sess->trans.enc_c2s_selected : NULL;
}

static dssh_enc
enc_s2c_module(struct dssh_session_s *sess)
{
	if (sess->trans.enc_s2c_active != NULL)
		return sess->trans.enc_s2c_active;
	return sess->trans.enc_s2c_ctx != NULL ? sess->trans.enc_s2c_selected : NULL;
}

static dssh_mac
mac_c2s_module(struct dssh_session_s *sess)
{
	if (sess->trans.mac_c2s_active != NULL)
		return sess->trans.mac_c2s_active;
	return sess->trans.mac_c2s_ctx != NULL ? sess->trans.mac_c2s_selected : NULL;
}

static dssh_mac
mac_s2c_module(struct dssh_session_s *sess)
{
	if (sess->trans.mac_s2c_active != NULL)
		return sess->trans.mac_s2c_active;
	return sess->trans.mac_s2c_ctx != NULL ? sess->trans.mac_s2c_selected : NULL;
}

DSSH_PRIVATE bool
dssh_io_callback_active(struct dssh_session_s *sess)
{
	return io_callback_session != NULL || dssh_result_callback_active()
	    || session_termination_callback_active(sess);
}

static int
call_io(struct dssh_session_s *sess, dssh_transport_io_cb cb, uint8_t *buf, size_t bufsz, void *cbdata)
{
	struct dssh_session_s *previous = io_callback_session;

	io_callback_session = sess;
	int ret = cb(buf, bufsz, sess, cbdata);
	io_callback_session = previous;
	return ret > 0 ? DSSH_ERROR_INVALID : ret;
}

static bool
compression_enabled(dssh_comp comp)
{
	return comp != NULL && strcmp(comp->name, "none") != 0;
}

static int
call_comp_init(struct dssh_session_s *sess, dssh_comp comp, bool compress, dssh_comp_ctx **ctx)
{
	struct dssh_session_s *previous = io_callback_session;
	io_callback_session = sess;
	int ret = comp->init(compress, ctx);
	io_callback_session = previous;
	return ret > 0 ? DSSH_ERROR_INVALID : ret;
}

static void
call_comp_cleanup(struct dssh_session_s *sess, dssh_comp comp, dssh_comp_ctx *ctx)
{
	struct dssh_session_s *previous = io_callback_session;
	io_callback_session = sess;
	comp->cleanup(ctx);
	io_callback_session = previous;
}

static int
call_comp_bound(struct dssh_session_s *sess, dssh_comp comp, size_t input_len,
    size_t *output_bound, dssh_comp_ctx *ctx)
{
	struct dssh_session_s *previous = io_callback_session;
	*output_bound = SIZE_MAX;
	io_callback_session = sess;
	int ret = comp->bound(input_len, output_bound, ctx);
	io_callback_session = previous;
	return ret > 0 ? DSSH_ERROR_INVALID : ret;
}

static int
call_comp_transform(struct dssh_session_s *sess, dssh_comp_transform transform,
    const uint8_t *input, size_t input_len, uint8_t *output, size_t output_capacity,
    size_t *output_len, dssh_comp_ctx *ctx)
{
	struct dssh_session_s *previous = io_callback_session;
	*output_len = SIZE_MAX;
	io_callback_session = sess;
	int ret = transform(input, input_len, output, output_capacity, output_len, ctx);
	io_callback_session = previous;
	if (ret > 0 || (ret == 0 && *output_len > output_capacity))
		return DSSH_ERROR_INVALID;
	return ret;
}

static int
call_rx_line(struct dssh_session_s *sess, uint8_t *buf, size_t bufsz, size_t *received)
{
	struct dssh_session_s *previous = io_callback_session;

	io_callback_session = sess;
	int ret = gconf.rx_line(buf, bufsz, received, sess, sess->rx_line_cbdata);
	io_callback_session = previous;
	return ret > 0 ? DSSH_ERROR_INVALID : ret;
}

static int
call_extra_line(struct dssh_session_s *sess, uint8_t *buf, size_t bufsz)
{
	struct dssh_session_s *previous = io_callback_session;

	io_callback_session = sess;
	int ret = gconf.extra_line_cb(buf, bufsz, sess->extra_line_cbdata);
	io_callback_session = previous;
	return ret > 0 ? DSSH_ERROR_INVALID : ret;
}

static int
call_tx_gather(struct dssh_session_s *sess, const struct dssh_tx_iov *iov, size_t iovcnt)
{
	struct dssh_session_s *previous = io_callback_session;

	io_callback_session = sess;
	int ret = gconf.tx_gather(iov, iovcnt, sess, sess->tx_cbdata);
	io_callback_session = previous;
	return ret > 0 ? DSSH_ERROR_INVALID : ret;
}

#ifdef DSSH_TESTING
static _Atomic(struct dssh_session_s *) dssh_test_rx_count_sess;
static _Atomic uint64_t dssh_test_rx_count_value;

DSSH_TESTABLE void
dssh_test_arm_rx_bytes_before_count(struct dssh_session_s *sess,
    uint64_t value)
{
	atomic_store_explicit(&dssh_test_rx_count_value, value,
	    memory_order_relaxed);
	atomic_store_explicit(&dssh_test_rx_count_sess, sess,
	    memory_order_release);
}

DSSH_TESTABLE unsigned
dssh_test_rekeys_completed(struct dssh_session_s *sess)
{
	return atomic_load_explicit(&sess->trans.test_rekeys_completed,
	    memory_order_acquire);
}

/* Compare actual library layout with a consumer compiled without injector
 * redirections. Keep this observation out of production builds and exports. */
DSSH_TESTABLE void
dssh_test_session_layout(size_t out[6])
{
	out[0] = sizeof(struct dssh_transport_state_s);
	out[1] = sizeof(struct dssh_session_s);
	out[2] = offsetof(struct dssh_transport_state_s, id_str);
	out[3] = offsetof(struct dssh_transport_state_s, remote_id_str);
	out[4] = offsetof(struct dssh_session_s, initialized);
	out[5] = offsetof(struct dssh_session_s, timeout_ms);
}
#endif

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

/* Whole-line character validation is separate from software-token grammar.
 * Spaces delimit the optional comment, which may contain minus. */
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

/* RFC 4253 s4.2: printable non-space US-ASCII, excluding minus. */
static bool
is_valid_sw_version(const char *s, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if (s[i] < 0x21 || s[i] > 0x7E || s[i] == '-')
			return false;
	}
	return true;
}

static bool
version_identification_valid(uint8_t *buf, size_t length)
{
	if (length > DSSH_VERSION_STRING_MAX || has_nulls(buf, length)
	    || missing_crlf(buf, length) || has_non_ascii(buf, length - 2)
	    || !is_20(buf, length))
		return false;
	/* is_20 established one of the complete supported protocol prefixes.
	 * The first space starts comments; it cannot replace the software token. */
	size_t software_pos = buf[4] == '1' ? 9 : 8;
	if (length - 2 <= software_pos)
		return false;
	size_t end = software_pos;
	while (end < length - 2 && buf[end] != ' ')
		end++;
	const char *software = (const char *)&buf[software_pos];
	return end > software_pos && is_valid_sw_version(software, end - software_pos);
}

static int
version_rx(struct dssh_session_s *sess)
{
	size_t received = 0;
	int    res;

	while (!sess->terminate) {
		received = 0;
		res = call_rx_line(sess, sess->trans.rx_packet, sess->trans.packet_buf_sz - 1, &received);
		if (res < 0) {
			session_set_terminate(sess);
			return res;
		}
		if (res != 0 || received == 0 || received > sess->trans.packet_buf_sz - 1) {
			session_set_terminate(sess);
			return DSSH_ERROR_INVALID;
		}
		if (is_version_line(sess->trans.rx_packet, received)) {
			if (!version_identification_valid(sess->trans.rx_packet, received)) {
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
			res = call_extra_line(sess, sess->trans.rx_packet, received);
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
	if (asz == 0 || !is_valid_sw_version(gconf.software_version, asz))
		return DSSH_ERROR_INVALID;

	if (!dssh_buffer_has(DSSH_VERSION_STRING_MAX, sz, asz)
	    || !dssh_buffer_has(DSSH_VERSION_STRING_MAX, sz + asz, 2))
		return DSSH_ERROR_TOOLONG;
	memcpy(&sess->trans.tx_packet[sz], gconf.software_version, asz);
	sz += asz;
	if (gconf.version_comment != NULL) {
		memcpy(&sess->trans.tx_packet[sz], " ", 1);
		sz += 1;
		asz = strlen(gconf.version_comment);
		if (!dssh_buffer_has(DSSH_VERSION_STRING_MAX, sz, asz)
		    || !dssh_buffer_has(DSSH_VERSION_STRING_MAX, sz + asz, 2))
			return DSSH_ERROR_TOOLONG;
		memcpy(&sess->trans.tx_packet[sz], gconf.version_comment, asz);
		sz += asz;
	}
	memcpy(sess->trans.id_str, sess->trans.tx_packet, sz);
	sess->trans.id_str_sz  = sz;
	sess->trans.id_str[sz] = 0;
	memcpy(&sess->trans.tx_packet[sz], "\r\n", 2);
	sz += 2;
	res = call_io(sess, gconf.tx, sess->trans.tx_packet, sz, sess->tx_cbdata);
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
	if (sess == NULL || dssh_io_callback_active(sess))
		return NULL;
	return sess->trans.remote_id_str;
}

DSSH_PUBLIC const char *
dssh_transport_get_kex_name(struct dssh_session_s *sess)
{
	if (sess == NULL || dssh_io_callback_active(sess))
		return NULL;
	dssh_kex selected = sess->trans.kex_selected;
	return selected != NULL ? selected->name : NULL;
}

DSSH_PUBLIC const char *
dssh_transport_get_hostkey_name(struct dssh_session_s *sess)
{
	if (sess == NULL || dssh_io_callback_active(sess))
		return NULL;
	dssh_key_algo selected = sess->trans.key_algo_selected;
	return selected != NULL ? selected->name : NULL;
}

DSSH_PUBLIC const char *
dssh_transport_get_enc_name(struct dssh_session_s *sess, enum dssh_direction direction)
{
	if (sess == NULL || (direction != DSSH_CLIENT_TO_SERVER && direction != DSSH_SERVER_TO_CLIENT)
	    || dssh_io_callback_active(sess))
		return NULL;
	dssh_enc selected = direction == DSSH_CLIENT_TO_SERVER ? sess->trans.enc_c2s_selected
	    : sess->trans.enc_s2c_selected;
	return selected != NULL ? selected->name : NULL;
}

DSSH_PUBLIC const char *
dssh_transport_get_mac_name(struct dssh_session_s *sess, enum dssh_direction direction)
{
	if (sess == NULL || (direction != DSSH_CLIENT_TO_SERVER && direction != DSSH_SERVER_TO_CLIENT)
	    || dssh_io_callback_active(sess))
		return NULL;
	dssh_mac selected = direction == DSSH_CLIENT_TO_SERVER ? sess->trans.mac_c2s_selected
	    : sess->trans.mac_s2c_selected;
	return selected != NULL ? selected->name : NULL;
}

DSSH_PUBLIC const char *
dssh_transport_get_comp_name(struct dssh_session_s *sess, enum dssh_direction direction)
{
	if (sess == NULL || (direction != DSSH_CLIENT_TO_SERVER && direction != DSSH_SERVER_TO_CLIENT)
	    || dssh_io_callback_active(sess))
		return NULL;
	dssh_comp selected = direction == DSSH_CLIENT_TO_SERVER ? sess->trans.comp_c2s_selected
	    : sess->trans.comp_s2c_selected;
	return selected != NULL ? selected->name : NULL;
}

DSSH_TESTABLE int
version_exchange(struct dssh_session_s *sess)
{
	int res = version_tx(sess);

	if (res < 0)
		return res;
	return version_rx(sess);
}

static int
transport_handshake_inner(struct dssh_session_s *sess)
{
	if (sess->trans.client && sess->hostkey_verify_cb == NULL) {
		DSSH_LOGF(sess, DSSH_LOG_ERROR, DSSH_ERROR_INIT,
		    "Client handshake requires a host-key verification callback");
		return DSSH_ERROR_INIT;
	}
	int res = version_exchange(sess);

	if (res < 0)
		goto fail;
	res = kexinit(sess);
	if (res < 0)
		goto fail;
	res = kex_exchange(sess);
	if (res < 0)
		goto fail;
	res = newkeys(sess);
	if (res < 0)
		goto fail;
	DSSH_LOGF(sess, DSSH_LOG_DEBUG, DSSH_ERROR_NONE,
	    "Handshake complete: kex=%s hostkey=%s cipher=%s mac=%s", sess->trans.kex_selected->name,
	    sess->trans.key_algo_selected->name, sess->trans.enc_c2s_selected->name,
	    sess->trans.mac_c2s_selected->name);
	return 0;

fail:
	DSSH_LOGF(sess, DSSH_LOG_ERROR, res, "SSH transport handshake failed: %s", dssh_strerror(res));
	session_set_terminate(sess);
	dssh_log_termination(sess, res);
	return res;
}

DSSH_PUBLIC int
dssh_transport_handshake(struct dssh_session_s *sess)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (dssh_io_callback_active(sess) || dssh_setup_callback_active(sess))
		return DSSH_ERROR_INVALID;
	if (session_receive_api_conflict(sess))
		return DSSH_ERROR_INVALID;
	if (sess->terminate)
		return DSSH_ERROR_TERMINATED;
	if (sess->handshake_complete)
		return DSSH_ERROR_INVALID;
	struct dssh_session_scope scope;

	session_scope_enter(&scope, sess);
	int ret = transport_handshake_inner(sess);

	if (ret == 0)
		sess->handshake_complete = true;
	session_scope_leave(&scope);
	return ret;
}

/* ================================================================
 * Rekeying (RFC 4253 s9)
 * ================================================================ */

/* TX counters and selected descriptors can be read by either owner. */
static bool
rekey_transmit_needed(struct dssh_session_s *sess, uint_fast32_t packets)
{
	if (packets >= DSSH_REKEY_SOFT_LIMIT)
		return true;
	dssh_enc enc = sess->trans.client ? atomic_load(&sess->trans.enc_c2s_selected)
	                                 : atomic_load(&sess->trans.enc_s2c_selected);
	return enc != NULL && enc->bytes_per_key != UINT64_MAX
	    && atomic_load(&sess->trans.tx_bytes_since_rekey) >= enc->bytes_per_key;
}

static bool
rekey_transmit_due(struct dssh_session_s *sess)
{
	uint_fast32_t packets = atomic_load(&sess->trans.tx_since_rekey);
	/* KEXINIT itself still uses the active key and its hard packet guard. */
	return packets < DSSH_REKEY_HARD_LIMIT && rekey_transmit_needed(sess, packets);
}

DSSH_TESTABLE bool
rekey_needed(struct dssh_session_s *sess)
{
	if (rekey_transmit_needed(sess, atomic_load(&sess->trans.tx_since_rekey))
	    || sess->trans.rx_since_rekey >= DSSH_REKEY_SOFT_LIMIT)
		return true;
	dssh_enc enc = sess->trans.client ? atomic_load(&sess->trans.enc_s2c_selected)
	                                 : atomic_load(&sess->trans.enc_c2s_selected);
	if (enc != NULL && enc->bytes_per_key != UINT64_MAX
	    && sess->trans.rx_bytes_since_rekey >= enc->bytes_per_key)
		return true;
	/* Time threshold has no cryptographic justification (the byte
	 * and packet thresholds do), so apps that need to interop with
	 * peers that refuse mid-stream KEXINIT (e.g. Cryptlib-based
	 * servers like Mystic BBS) can override or disable it via
	 * dssh_session_set_rekey_seconds(). */
	uint_least32_t seconds = atomic_load_explicit(&sess->trans.rekey_seconds, memory_order_relaxed);
	if (seconds == 0)
		return false;
	struct timespec now;
	if (dssh_clock_read(&now) < 0) {
		session_set_terminate(sess);
		return false;
	}
	/* A clock adjustment backwards has not reached the requested interval. */
	/* The setter's uint32_t input is the only writer. */
	uint32_t interval = (uint32_t)seconds;
	return dssh_time_interval_reached(now.tv_sec, sess->trans.rekey_time, interval);
}

DSSH_TESTABLE int
rekey(struct dssh_session_s *sess)
{
	/*
         * RFC 4253 s9: Rekey uses existing encryption until NEWKEYS.
         * Do NOT free cipher/MAC contexts here -- they are needed for
         * the kexinit/kex packets.  newkeys() will replace them.
         *
         * Release any prior KEX result. Retain initialized method state and
         * session_id -- the latter persists across rekeys per RFC 4253 s7.2.
         */
	kex_release_result(sess);

	/* Drain work that was queued before rekey, then publish the rekey
	 * predicate while ownership prevents another application sender from
	 * entering the packet engine between those operations. */
	int res = tx_acquire(sess, SSH_MSG_IGNORE);

	if (res < 0)
		return res;
	/* Synchronize with any transmitter that claimed the read boundary.
	 * Its offer and transcript are complete before the engine is free. */
	bool offered = sess->trans.rekey_offer_sent;
	sess->trans.rekey_offer_sent = false;
	if (!offered) {
		res = drain_tx_slots(sess);
		if (res < 0)
			return tx_unlock_with_result(sess, res);
	}
	if (atomic_load_explicit(&sess->terminate, memory_order_acquire)) {
		tx_unlock_with_slots(sess);
		return DSSH_ERROR_TERMINATED;
	}
	int state_lr = mtx_lock(&sess->trans.tx_mtx);

	if (state_lr != thrd_success) {
		dssh_thrd_check(sess, state_lr);
		tx_unlock_with_slots(sess);
		return DSSH_ERROR_TERMINATED;
	}
	atomic_store_explicit(&sess->trans.rekey_in_progress, true, memory_order_release);
	int state_ur = dssh_mtx_unlock(&sess->trans.tx_mtx);

	if (state_ur != thrd_success) {
		dssh_thrd_check_unlock(sess, state_ur);
		tx_unlock_with_slots(sess);
		return DSSH_ERROR_TERMINATED;
	}
	tx_unlock_with_slots(sess);

	res = offered ? finish_kexinit(sess) : kexinit(sess);

	if (res == 0)
		res = kex_exchange(sess);
	if (res == 0)
		res = newkeys(sess);

	/* An offer may already be on the wire. Publish failure before
	 * removing the send gate so old-key application traffic cannot resume. */
	if (res < 0)
		session_set_terminate_no_wake(sess);
	state_lr = mtx_lock(&sess->trans.tx_mtx);
	if (state_lr == thrd_success) {
		atomic_store_explicit(&sess->trans.rekey_in_progress, false, memory_order_release);
		int br = cnd_broadcast(&sess->trans.rekey_cnd);
		if (br != thrd_success) {
			session_set_terminate_no_wake(sess);
			res = DSSH_ERROR_TERMINATED;
		}
		dssh_mtx_unlock(&sess->trans.tx_mtx);
	}
	else {
		session_set_terminate_no_wake(sess);
		res = DSSH_ERROR_TERMINATED;
	}
	/* Claim and release once to drain connection work accumulated during
	 * rekey before application senders resume. */
	if (res == 0) {
		if (!tx_owner_is_current(sess))
			res = tx_acquire(sess, SSH_MSG_IGNORE);
		if (res == 0) {
			res = drain_tx_slots(sess);
			res = tx_unlock_with_result(sess, res);
		}
	}

	if (res < 0) {
		if (tx_owner_is_current(sess))
			tx_unlock_with_slots(sess);
		else
			session_set_terminate(sess);
	}
#ifdef DSSH_TESTING
	if (res == 0) {
		unsigned before = atomic_load_explicit(&sess->trans.test_rekeys_completed,
		    memory_order_relaxed);

		/* The sole receive owner records completion; saturate so a wrap
		 * cannot look like a new exchange to an integration observer. */
		if (before < UINT_MAX)
			atomic_store_explicit(&sess->trans.test_rekeys_completed,
			    before + 1, memory_order_release);
	}
#endif
	return res;
}

/* ================================================================
 * SSH_MSG_UNIMPLEMENTED (RFC 4253 s11.4)
 * ================================================================ */

DSSH_PRIVATE int
send_unimplemented(struct dssh_session_s *sess, uint32_t rejected_seq)
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

DSSH_PRIVATE int
transport_disconnect_inner(struct dssh_session_s *sess, uint32_t reason, const char *desc)
{
	if (reason == 0)
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

	int ret = tx_acquire(sess, SSH_MSG_DISCONNECT);
	bool acquired = ret == 0;
	if (acquired)
		ret = send_packet_inner(sess, msg, pos, NULL);
	/* Publish termination before releasing the packet engine: its normal
	 * release path must not send queued messages after DISCONNECT. */
	session_set_terminate(sess);
	if (acquired)
		tx_unlock_with_slots(sess);
	return ret;
}

/* Preserve the caller's protocol result after a successful disconnect send,
 * while still reporting a failed attempt. Termination occurs in either case. */
DSSH_PRIVATE int
transport_disconnect_result(struct dssh_session_s *sess, uint32_t reason, const char *desc, int result)
{
	int ret = transport_disconnect_inner(sess, reason, desc);
	return ret < 0 ? ret : result;
}

DSSH_PUBLIC int
dssh_transport_disconnect(struct dssh_session_s *sess, uint32_t reason, const char *desc)
{
	if (sess == NULL || desc == NULL)
		return DSSH_ERROR_INVALID;
	if (dssh_io_callback_active(sess) || dssh_setup_callback_active(sess))
		return DSSH_ERROR_INVALID;
	return transport_disconnect_inner(sess, reason, desc);
}

/* Forward declarations for slot helpers */
DSSH_TESTABLE size_t tx_block_size(struct dssh_session_s *sess);
static uint16_t      tx_mac_size(struct dssh_session_s *sess);

static bool
parse_log_string(const uint8_t *payload, size_t payload_len, size_t *pos, const uint8_t **str, size_t *str_len)
{
	if (!dssh_buffer_has(payload_len, *pos, 4))
		return false;
	uint32_t len_u32 = DSSH_GET_U32(&payload[*pos]);

	*pos += 4;
#if SIZE_MAX < UINT32_MAX
	if (len_u32 > SIZE_MAX)
		return false;
#endif
	size_t len = (size_t)len_u32;

	if (!dssh_buffer_has(payload_len, *pos, len))
		return false;
	*str     = len > 0 ? &payload[*pos] : NULL;
	*str_len = len;
	*pos += len;
	return true;
}

/* ================================================================
 * TX slot helpers
 * ================================================================ */

/* Zero denotes padding that cannot be encoded in the one-byte wire field. */
static size_t
packet_padding(size_t payload, size_t block)
{
	if (block < 8)
		block = 8;
	if (payload > SIZE_MAX - 5)
		return 0;
	size_t pad = block - ((5 + payload) % block);
	if (pad < 4) {
		if (block > UINT8_MAX - pad)
			return 0;
		pad += block;
	}
	return pad <= UINT8_MAX ? pad : 0;
}

/*
 * Compute the exact buffer size for a TX slot holding a packet with
 * the given max_payload, block_size, and MAC digest size.  Layout:
 *   [seq(4)][pkt_length(4)][pad_len(1)][payload][padding][MAC]
 * Returns zero if the packet or its allocation size is not representable.
 */
DSSH_PRIVATE size_t
tx_slot_buf_size(size_t max_payload, size_t block_size, uint16_t mac_digest)
{
	size_t pad = packet_padding(max_payload, block_size);
	if (pad == 0)
		return 0;
	size_t total = 5 + max_payload;
	if (!dssh_buffer_has(SIZE_MAX, total, pad))
		return 0;
	total += pad;
	if (total - 4 > UINT32_MAX || !dssh_buffer_has(SIZE_MAX, total, 4))
		return 0;
	total += 4;
	if (!dssh_buffer_has(SIZE_MAX, total, mac_digest))
		return 0;
	return total + mac_digest;
}

/*
 * Allocate (or reallocate) a TX slot buffer sized for the negotiated
 * algorithms. Returns 0 on success, INVALID for an unrepresentable size,
 * or ALLOC on allocation failure.
 * If the existing allocation is already the right size, this is a no-op.
 */
DSSH_PRIVATE int
alloc_tx_slot(struct dssh_tx_slot *slot, size_t max_payload, size_t block_size, uint16_t mac_digest)
{
	size_t need = tx_slot_buf_size(max_payload, block_size, mac_digest);

	if (need == 0)
		return DSSH_ERROR_INVALID;
	if (slot->buf != NULL && slot->buf_sz == need) {
		slot->payload_capacity = max_payload;
		return 0;
	}
	uint8_t *nb = realloc(slot->buf, need);

	if (nb == NULL)
		return DSSH_ERROR_ALLOC;
	slot->buf    = nb;
	slot->buf_sz = need;
	slot->payload_capacity = max_payload;
	return 0;
}

DSSH_PRIVATE void
free_tx_slot(struct dssh_tx_slot *slot)
{
	free(slot->buf);
	slot->buf    = NULL;
	slot->buf_sz = 0;
	slot->payload_capacity = 0;
	atomic_store(&slot->ready, false);
	slot->payload_len = 0;
}

/* The packet engine excludes other resizers and drainers. A producer may
 * fill the old buffer while the replacement is allocated, so the queue
 * mutex serializes the final payload copy and storage publication. */
DSSH_TESTABLE int
resize_tx_slot_live(struct dssh_session_s *sess, struct dssh_tx_slot *slot, size_t max_payload,
    size_t block_size, uint16_t mac_digest)
{
	size_t need = tx_slot_buf_size(max_payload, block_size, mac_digest);

	if (need == 0)
		return DSSH_ERROR_INVALID;
	if (slot->buf != NULL && slot->buf_sz == need && slot->payload_capacity == max_payload)
		return 0;
	uint8_t *replacement = malloc(need);

	if (replacement == NULL)
		return DSSH_ERROR_ALLOC;
	int ret = mtx_lock(&sess->trans.tx_queue_mtx);

	if (ret != thrd_success) {
		free(replacement);
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	uint8_t *old = slot->buf;

	if (old != NULL && atomic_load_explicit(&slot->ready, memory_order_acquire)) {
		if (slot->payload_len > max_payload
		    || !dssh_buffer_has(slot->buf_sz, 9, slot->payload_len)) {
			dssh_mtx_unlock(&sess->trans.tx_queue_mtx);
			free(replacement);
			return DSSH_ERROR_INVALID;
		}
		memcpy(&replacement[9], &old[9], slot->payload_len);
	}
	slot->buf = replacement;
	slot->buf_sz = need;
	slot->payload_capacity = max_payload;
	dssh_mtx_unlock(&sess->trans.tx_queue_mtx);
	free(old);
	return 0;
}

/*
 * Allocate (or resize) session-level TX slots for the current
 * negotiated algorithms.  Called from newkeys() after
 * derive_and_apply_keys() installs new enc/mac contexts.
 */
static int
alloc_session_slots(struct dssh_session_s *sess)
{
	size_t   bs  = tx_block_size(sess);
	uint16_t mac = tx_mac_size(sess);
	int      ret;

	ret = resize_tx_slot_live(sess, &sess->trans.global_reply_slot, 1, bs, mac);
	if (ret < 0)
		return ret;
	ret = resize_tx_slot_live(sess, &sess->trans.open_fail_slot, 17, bs, mac);
	return ret;
}

/*
 * Allocate channel-level TX slots for the current negotiated algorithms.
 * Called during channel init after KEX has completed.
 */
DSSH_PRIVATE int
alloc_channel_slots(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	bool acquired = false;

	if (!tx_owner_is_current(sess)) {
		int acquire_ret = tx_acquire(sess, SSH_MSG_CHANNEL_OPEN);

		if (acquire_ret < 0)
			return acquire_ret;
		acquired = true;
	}
	size_t   bs  = tx_block_size(sess);
	uint16_t mac = tx_mac_size(sess);
	int      ret;

	ret = alloc_tx_slot(&ch->wa_slot, 9, bs, mac);
	if (ret == 0)
		ret = alloc_tx_slot(&ch->chan_reply_slot, 8, bs, mac);
	if (acquired)
		return tx_unlock_with_result(sess, ret);
	return ret;
}

/*
 * Resize all active channel slots after rekey.  The registry mutex is held
 * only while pinning the current channel set; allocation happens lock-free.
 */
static int
resize_channel_slots(struct dssh_session_s *sess)
{
	size_t   bs  = tx_block_size(sess);
	uint16_t mac = tx_mac_size(sess);
	struct dssh_channel_s **channels = NULL;
	size_t                   capacity = 0;
	size_t                   count;
	int                      ret;

	for (;;) {
		ret = mtx_lock(&sess->channel_mtx);
		if (ret != thrd_success) {
			dssh_thrd_check(sess, ret);
			free(channels);
			return DSSH_ERROR_TERMINATED;
		}
		count = sess->channel_count;
		if (count <= capacity) {
			for (size_t i = 0; i < count; i++) {
				channels[i] = sess->channels[i];
				atomic_fetch_add_explicit(&channels[i]->tx_refs, 1, memory_order_acq_rel);
			}
			ret = dssh_mtx_unlock(&sess->channel_mtx);
			if (ret != thrd_success) {
				for (size_t i = 0; i < count; i++)
					atomic_fetch_sub_explicit(&channels[i]->tx_refs, 1, memory_order_acq_rel);
				dssh_thrd_check_unlock(sess, ret);
				free(channels);
				return DSSH_ERROR_TERMINATED;
			}
			break;
		}
		ret = dssh_mtx_unlock(&sess->channel_mtx);
		if (ret != thrd_success) {
			dssh_thrd_check_unlock(sess, ret);
			free(channels);
			return DSSH_ERROR_TERMINATED;
		}
		if (count > SIZE_MAX / sizeof(*channels)) {
			free(channels);
			return DSSH_ERROR_ALLOC;
		}
		struct dssh_channel_s **larger = realloc(channels, count * sizeof(*channels));

		if (larger == NULL) {
			free(channels);
			return DSSH_ERROR_ALLOC;
		}
		channels = larger;
		capacity = count;
	}

	ret = 0;
	for (size_t i = 0; i < count; i++) {
		struct dssh_channel_s *ch = channels[i];
		int                    alloc_ret;

		alloc_ret = resize_tx_slot_live(sess, &ch->wa_slot, 9, bs, mac);
		if (alloc_ret == 0)
			alloc_ret = resize_tx_slot_live(sess, &ch->chan_reply_slot, 8, bs, mac);
		if (ret == 0 && alloc_ret < 0)
			ret = alloc_ret;
	}
	for (size_t i = 0; i < count; i++)
		atomic_fetch_sub_explicit(&channels[i]->tx_refs, 1, memory_order_acq_rel);
	free(channels);
	return ret;
}

/* ================================================================
 * Binary Packet Protocol (RFC 4253 s6)
 * ================================================================ */

DSSH_TESTABLE size_t
tx_block_size(struct dssh_session_s *sess)
{
	dssh_enc enc;

	if (sess->trans.client) {
		enc = enc_c2s_module(sess);
	}
	else {
		enc = enc_s2c_module(sess);
	}
	if (enc == NULL)
		return 8;
	if (enc->blocksize < 8)
		return 8;
	return enc->blocksize;
}

DSSH_TESTABLE size_t
rx_block_size(struct dssh_session_s *sess)
{
	dssh_enc enc;

	if (sess->trans.client) {
		enc = enc_s2c_module(sess);
	}
	else {
		enc = enc_c2s_module(sess);
	}
	if (enc == NULL)
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

	if (sess->trans.client) {
		mac = mac_c2s_module(sess);
	}
	else {
		mac = mac_s2c_module(sess);
	}
	if (mac == NULL)
		return 0;
	return mac->digest_size;
}

static uint16_t
rx_mac_size(struct dssh_session_s *sess)
{
	dssh_mac mac;

	if (sess->trans.client) {
		mac = mac_s2c_module(sess);
	}
	else {
		mac = mac_c2s_module(sess);
	}
	if (mac == NULL)
		return 0;
	return mac->digest_size;
}

/* Largest wire payload whose complete packet, padding, and MAC fit. */
static size_t
tx_wire_payload_capacity(struct dssh_session_s *sess)
{
	size_t block = tx_block_size(sess);
	size_t mac = tx_mac_size(sess);
	if (mac > sess->trans.packet_buf_sz)
		return 0;
	size_t total = sess->trans.packet_buf_sz - mac;
	total -= total % block;
	/* Four length bytes, one padding-length byte, four padding bytes. */
	return total > 9 ? total - 9 : 0;
}

/* Writable uncompressed input capacity. The cached compression bound includes
 * worst-case growth; it is installed with the directional key epoch. */
DSSH_PRIVATE size_t
tx_payload_capacity(struct dssh_session_s *sess)
{
	size_t capacity = tx_wire_payload_capacity(sess);
	dssh_comp comp = sess->trans.client ? sess->trans.comp_c2s_active : sess->trans.comp_s2c_active;
	if (compression_enabled(comp) && sess->trans.comp_tx_payload_limit < capacity)
		capacity = sess->trans.comp_tx_payload_limit;
	return capacity;
}

/*
 * Finalize and send a packet whose payload is already in tx_packet.
 * Caller MUST own the packet engine.  No mutex is held.  The payload starts at tx_packet[9]
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
/*
 * Prepare a packet for transmission: padding, MAC, encrypt, advance
 * tx_seq.  Does NOT call the TX callback or update byte/packet
 * counters.  Sets *wire_out to the start of wire data (&buf[4])
 * and *wire_len_out to the total wire length.
 *
 * Caller MUST own the packet engine.  No mutex is held.  Used by tx_finalize (per-packet send)
 * and by the gather path (batch send).
 */
DSSH_PRIVATE int
tx_finalize_prepare(struct dssh_session_s *sess, uint8_t *buf, size_t payload_len, uint8_t **wire_out,
    size_t *wire_len_out)
{
	int ret;
	bool transformed = false;
	bool crypto_called = false;
	if (sess->terminate)
		return DSSH_ERROR_TERMINATED;
	/* A recoverable key-limit refusal must not advance cipher/compressor state. */
	if (sess->trans.tx_since_rekey >= DSSH_REKEY_HARD_LIMIT)
		return DSSH_ERROR_REKEY_NEEDED;
	if (payload_len > tx_payload_capacity(sess))
		return DSSH_ERROR_TOOLONG;

	/* Single trace point for every packet about to hit the wire,
	 * regardless of which code path led here (send_packet,
	 * zc_send_inner, drain_one_slot, gather_prepare_one). */
	DSSH_TRACE_W(sess, "tx msg_type=%d len=%zu", buf[9], payload_len);

	dssh_comp comp = sess->trans.client ? sess->trans.comp_c2s_active : sess->trans.comp_s2c_active;
	if (compression_enabled(comp)) {
		dssh_comp_ctx *ctx = sess->trans.client ? sess->trans.comp_c2s_ctx : sess->trans.comp_s2c_ctx;
		size_t compressed_len;
		transformed = true;
		ret = call_comp_transform(sess, comp->compress, &buf[9], payload_len,
		    &sess->trans.comp_tx_packet[9], tx_wire_payload_capacity(sess), &compressed_len, ctx);
		if (ret < 0)
			goto done;
		if (sess->terminate) {
			ret = DSSH_ERROR_TERMINATED;
			goto done;
		}
		buf = sess->trans.comp_tx_packet;
		payload_len = compressed_len;
	}
	size_t bs = tx_block_size(sess);

	if (payload_len > SIZE_MAX - 5) {
		ret = DSSH_ERROR_TOOLONG;
		goto done;
	}
	size_t padding_len = packet_padding(payload_len, bs);
	if (padding_len == 0) {
		ret = DSSH_ERROR_INVALID;
		goto done;
	}
	if (!dssh_buffer_has(SIZE_MAX, 1 + payload_len, padding_len)) {
		ret = DSSH_ERROR_TOOLONG;
		goto done;
	}

	size_t pkt_sz = 1 + payload_len + padding_len;

	if (pkt_sz > UINT32_MAX) {
		ret = DSSH_ERROR_TOOLONG;
		goto done;
	}
	uint32_t packet_length = (uint32_t)pkt_sz;

	size_t total;
	size_t mac_input;
	if (!dssh_packet_extents(packet_length, sess->trans.packet_buf_sz, &total, &mac_input)) {
		ret = DSSH_ERROR_TOOLONG;
		goto done;
	}
	uint16_t mac_len = tx_mac_size(sess);

	if (!dssh_buffer_has(sess->trans.packet_buf_sz, total, mac_len)) {
		ret = DSSH_ERROR_TOOLONG;
		goto done;
	}

	/* Write pkt_length and pad_len at buf[4..8] */
	size_t pos = 4;
	DSSH_PUT_U32(packet_length, buf, &pos);
	buf[pos] = (uint8_t)padding_len;

	/* Random padding after payload: buf[9 + payload_len] */
	ret = dssh_random(&buf[9 + payload_len], padding_len);
	if (ret < 0) {
		goto done;
	}

	/* MAC: mac(key, seq || unencrypted_packet)
         * Write seq at buf[0]; MAC input is contiguous at [0..4+total). */
	if (mac_len > 0) {
		size_t seq_pos = 0;

		DSSH_PUT_U32(sess->trans.tx_seq, buf, &seq_pos);

		dssh_mac      mac;
		dssh_mac_ctx *mac_ctx;

		if (sess->trans.client) {
			mac     = mac_c2s_module(sess);
			mac_ctx = sess->trans.mac_c2s_ctx;
		}
		else {
			mac     = mac_s2c_module(sess);
			mac_ctx = sess->trans.mac_s2c_ctx;
		}
		crypto_called = true;
		ret = mac->generate(buf, mac_input, &buf[mac_input], mac_ctx);
		if (ret > 0)
			ret = DSSH_ERROR_INVALID;
		if (ret < 0) {
			goto done;
		}
	}

	/* Encrypt (the packet at [4..4+total), not the seq prefix or MAC) */
	{
		dssh_enc_ctx *enc_ctx;
		dssh_enc      enc;

		if (sess->trans.client) {
			enc     = enc_c2s_module(sess);
			enc_ctx = sess->trans.enc_c2s_ctx;
		}
		else {
			enc     = enc_s2c_module(sess);
			enc_ctx = sess->trans.enc_s2c_ctx;
		}

		if ((enc != NULL) && (enc->encrypt != NULL)) {
			crypto_called = true;
			ret = enc->encrypt(&buf[4], total, enc_ctx);
			if (ret > 0)
				ret = DSSH_ERROR_INVALID;
			if (ret < 0) {
				goto done;
			}
		}
	}

	sess->trans.tx_seq++;
	*wire_out     = &buf[4];
	*wire_len_out = total + mac_len;
	return 0;

done:
	if (transformed) {
		dssh_cleanse(sess->trans.comp_tx_packet, 4);
		dssh_cleanse(&sess->trans.comp_tx_packet[4], sess->trans.packet_buf_sz);
	}
	/* A failed module may already have changed its stream state. Notification
	 * waits until sensitive caller storage is cleared and ownership released. */
	if (transformed || crypto_called)
		session_set_terminate_no_wake(sess);
	return ret;
}

/*
 * Account for a sent packet or reserve its key budget while its complete wire
 * bytes await a gathered callback. A failed callback terminates the session;
 * staged packets are never prepared again or counted twice.
 */
static void
tx_update_counters(struct dssh_session_s *sess, size_t wire_len)
{
	atomic_fetch_add(&sess->trans.tx_since_rekey, 1);
	{
		uint64_t old_tb = atomic_load(&sess->trans.tx_bytes_since_rekey);
		if (wire_len > UINT64_MAX - old_tb)
			atomic_store(&sess->trans.tx_bytes_since_rekey, UINT64_MAX);
		else
			atomic_store(&sess->trans.tx_bytes_since_rekey, old_tb + wire_len);
	}
}

/* Flush retained controls and an optional last packet without lending live
 * slot storage. No mutex is held and the caller owns the packet engine. */
static int
tx_batch_flush(struct dssh_tx_batch *batch, const uint8_t *tail, size_t tail_len)
{
	struct dssh_session_s *sess = batch->sess;
	struct dssh_tx_iov iov[DSSH_TX_BATCH_PACKETS + 1];
	size_t count = batch->count;
	int ret = 0;
	for (size_t i = 0; i < count; i++) {
		iov[i].base = &sess->trans.tx_gather_buf[batch->offsets[i]];
		iov[i].len = batch->lengths[i];
	}
	if (tail != NULL) {
		iov[count].base = tail;
		iov[count].len = tail_len;
		count++;
	}
	if (count != 0) {
		ret = sess->terminate ? DSSH_ERROR_TERMINATED : call_tx_gather(sess, iov, count);
		if (ret < 0)
			session_set_terminate_no_wake(sess);
	}
	if (batch->bytes != 0)
		dssh_cleanse(sess->trans.tx_gather_buf, batch->bytes);
	batch->count = 0;
	batch->bytes = 0;
	return ret;
}

/* The fixed packet count bounds retained control storage. Grow its allocation
 * only when required and keep offsets rather than pointers across realloc. */
static int
tx_batch_append(struct dssh_tx_batch *batch, const uint8_t *wire, size_t wire_len)
{
	struct dssh_session_s *sess = batch->sess;
	int ret;
	if (batch->count == DSSH_TX_BATCH_PACKETS || wire_len > SIZE_MAX - batch->bytes) {
		ret = tx_batch_flush(batch, NULL, 0);
		if (ret < 0)
			return ret;
	}
	size_t needed = batch->bytes + wire_len;
	if (needed > sess->trans.tx_gather_capacity) {
		size_t capacity = sess->trans.tx_gather_capacity;
		if (capacity < 1024)
			capacity = 1024;
		while (capacity < needed) {
			if (capacity > SIZE_MAX / 2) {
				capacity = needed;
				break;
			}
			capacity *= 2;
		}
		uint8_t *storage = realloc(sess->trans.tx_gather_buf, capacity);
		if (storage == NULL) {
			session_set_terminate_no_wake(sess);
			return DSSH_ERROR_ALLOC;
		}
		sess->trans.tx_gather_buf = storage;
		sess->trans.tx_gather_capacity = capacity;
	}
	memcpy(&sess->trans.tx_gather_buf[batch->bytes], wire, wire_len);
	batch->offsets[batch->count] = batch->bytes;
	batch->lengths[batch->count] = wire_len;
	batch->count++;
	batch->bytes = needed;
	tx_update_counters(sess, wire_len);
	return 0;
}

/*
 * Finalize and send a packet.  Caller owns the packet engine; no mutex is held.
 * Prepares the packet (padding/MAC/encrypt), sends via gconf.tx,
 * and updates counters on success.
 */
DSSH_PRIVATE int
tx_finalize(struct dssh_session_s *sess, uint8_t *buf, size_t payload_len)
{
	uint8_t *wire;
	size_t   wire_len;
	int      ret = tx_finalize_prepare(sess, buf, payload_len, &wire, &wire_len);

	if (ret < 0)
		return ret;
	bool staged = tx_batch != NULL && tx_batch->sess == sess;
	ret = staged ? tx_batch_append(tx_batch, wire, wire_len)
	    : call_io(sess, gconf.tx, wire, wire_len, sess->tx_cbdata);
	if (sess->trans.comp_tx_packet != NULL) {
		dssh_cleanse(sess->trans.comp_tx_packet, 4);
		dssh_cleanse(&sess->trans.comp_tx_packet[4], sess->trans.packet_buf_sz);
	}
	if (ret == 0 && !staged)
		tx_update_counters(sess, wire_len);
	else if (ret < 0)
		session_set_terminate_no_wake(sess);
	return ret;
}

/*
 * Core send logic.  Caller owns the packet engine.  Copies payload into
 * tx_packet then calls tx_finalize for MAC/encrypt/send.
 */
static int
send_packet_inner(struct dssh_session_s *sess, const uint8_t *payload, size_t payload_len, uint32_t *seq_out)
{
	if (payload_len > sess->trans.packet_buf_sz - 9)
		return DSSH_ERROR_TOOLONG;

	/* Copy payload into tx_packet[9] (past seq + pkt_length + pad_len) */
	memcpy(&sess->trans.tx_packet[9], payload, payload_len);

	int ret = tx_finalize(sess, sess->trans.tx_packet, payload_len);

	if (ret == 0 && seq_out != NULL)
		*seq_out = sess->trans.tx_seq - 1;
	return ret;
}

/* Drain callback-approved SSH_MSG_DEBUG records.  Caller owns the packet engine.
 * The queue lock is held only while copying one fixed-size entry. */
DSSH_TESTABLE int
drain_log_forwards(struct dssh_session_s *sess)
{
	struct dssh_log_forward_queue *queue = sess->log_forward;

	if (queue == NULL || sess->terminate || sess->trans.remote_id_str_sz == 0)
		return 0;
	for (;;) {
		if (mtx_trylock(&queue->mtx) != thrd_success)
			return 0;
		if (queue->count == 0) {
			dssh_mtx_unlock(&queue->mtx);
			return 0;
		}
		struct dssh_log_forward_entry entry = queue->entries[queue->head];

		queue->head = (queue->head + 1) % DSSH_LOG_FORWARD_MAX;
		queue->count--;
		dssh_mtx_unlock(&queue->mtx);

		uint8_t  payload[1 + 1 + 4 + DSSH_LOG_MESSAGE_MAX + 4];
		size_t   pos = 0;
		uint32_t message_len = (uint32_t)entry.message_len;

		payload[pos++] = SSH_MSG_DEBUG;
		payload[pos++] = entry.always_display ? 1 : 0;
		DSSH_PUT_U32(message_len, payload, &pos);
		if (entry.message_len > 0) {
			memcpy(&payload[pos], entry.message, entry.message_len);
			pos += entry.message_len;
		}
		DSSH_PUT_U32(0, payload, &pos);
		atomic_store_explicit(&sess->log_mirror_suppressed, true, memory_order_relaxed);
		int ret = send_packet_inner(sess, payload, pos, NULL);

		atomic_store_explicit(&sess->log_mirror_suppressed, false, memory_order_relaxed);
		if (ret < 0)
			return ret;
	}
}

/* ================================================================
 * Slot-based TX queue
 * ================================================================ */

/* A transform or I/O callback can return a normally recoverable code after
 * changing stream state. Such a result is terminal, not a retry opportunity. */
static bool
tx_drain_refused(struct dssh_session_s *sess, int result)
{
	return !sess->terminate && (result == DSSH_ERROR_TOOLONG || result == DSSH_ERROR_REKEY_NEEDED);
}

/* Caller owns the packet engine. Set progress only after retiring this entry;
 * a recoverable refusal preserves its bytes, length and ready publication. */
static int
drain_one_slot(struct dssh_session_s *sess, struct dssh_tx_slot *slot, bool *progress, bool discard)
{
	if (!atomic_load(&slot->ready))
		return 0;
	int ret = discard ? 0 : tx_finalize(sess, slot->buf, slot->payload_len);
	if (tx_drain_refused(sess, ret))
		return ret;
	atomic_store(&slot->ready, false);
	*progress = true;
	return sess->terminate ? DSSH_ERROR_TERMINATED : ret;
}

/* Producers can queue more credit while the old grant is in flight. On a
 * pre-wire refusal, move that grant back under BUF without replacing new work.
 * The packet-engine owner exclusively uses wa_slot.buf; no mutex spans I/O. */
DSSH_TESTABLE int
drain_one_wa_slot(struct dssh_session_s *sess, struct dssh_channel_s *ch, bool *progress, bool discard)
{
	int lr = mtx_lock(&ch->buf_mtx);
	if (lr != thrd_success) {
		channel_defer_read_error(sess, ch, DSSH_ERROR_TERMINATED);
		return DSSH_ERROR_TERMINATED;
	}
	if (!atomic_load(&ch->wa_slot.ready)) {
		dssh_mtx_unlock(&ch->buf_mtx);
		return 0;
	}
	uint32_t bytes = discard ? 0 : ch->window_adjust_pending;
	uint32_t remote_id = ch->remote_id;
	ch->window_adjust_pending = 0;
	atomic_store(&ch->wa_slot.ready, false);
	if (bytes > 0)
		ch->window_adjust_inflight = window_add(ch->window_adjust_inflight, bytes);
	dssh_mtx_unlock(&ch->buf_mtx);
	if (bytes == 0) {
		*progress = true;
		return 0;
	}

	uint8_t *buf = ch->wa_slot.buf;
	buf[9] = SSH_MSG_CHANNEL_WINDOW_ADJUST;
	size_t wp = 10;
	DSSH_PUT_U32(remote_id, buf, &wp);
	DSSH_PUT_U32(bytes, buf, &wp);
	ch->wa_slot.payload_len = 9;
	int ret = tx_finalize(sess, buf, 9);
	int read_error = ret;
	bool refused = tx_drain_refused(sess, ret);
	bool fatal = ret < 0 && !refused;

	lr = mtx_lock(&ch->buf_mtx);
	if (lr != thrd_success) {
		channel_defer_read_error(sess, ch, read_error < 0 ? read_error : DSSH_ERROR_TERMINATED);
		return DSSH_ERROR_TERMINATED;
	}
	if (ret == 0)
		ch->local_window = window_add(ch->local_window, bytes);
	if (!fatal) {
		if (bytes <= ch->window_adjust_inflight)
			ch->window_adjust_inflight -= bytes;
		else
			ch->window_adjust_inflight = 0;
		if (refused) {
			ch->window_adjust_pending = window_add(ch->window_adjust_pending, bytes);
			atomic_store(&ch->wa_slot.ready, true);
		}
		int br = cnd_broadcast(&ch->poll_cnd);
		dssh_mtx_unlock(&ch->buf_mtx);
		if (br != thrd_success)
			ret = DSSH_ERROR_TERMINATED;
	}
	else
		dssh_mtx_unlock(&ch->buf_mtx);
	if (ret < 0)
		channel_defer_read_error(sess, ch, read_error < 0 ? read_error : ret);
	if (!refused)
		*progress = true;
	return sess->terminate ? DSSH_ERROR_TERMINATED : ret;
}

/* Drain in protocol priority order. Stop at the first refusal so the owner
 * cannot repeatedly select an unchanged ready entry or send later work first.
 * Pins and physical locks are released before notification or transport I/O. */
DSSH_PRIVATE int
drain_tx_slots(struct dssh_session_s *sess)
{
	if (sess->terminate)
		return DSSH_ERROR_TERMINATED;
	if (atomic_load_explicit(&sess->trans.rekey_in_progress, memory_order_acquire))
		return 0;
	bool progress = false;
	int result = drain_one_slot(sess, &sess->trans.global_reply_slot, &progress, false);
	if (result == 0)
		result = drain_one_slot(sess, &sess->trans.open_fail_slot, &progress, false);

	if (result == 0 && sess->conn_initialized) {
		for (;;) {
			struct dssh_channel_s *ch = NULL;
			bool discard = false;
			int lr = mtx_lock(&sess->channel_mtx);
			if (lr != thrd_success) {
				result = DSSH_ERROR_TERMINATED;
				break;
			}
			size_t n = sess->channel_count;
			if (n > 0) {
				size_t idx = sess->trans.drain_next_ch % n;
				for (size_t j = 0; j < n; j++) {
					struct dssh_channel_s *candidate = sess->channels[idx];
					if (atomic_load(&candidate->wa_slot.ready)
					    || atomic_load(&candidate->chan_reply_slot.ready)) {
						atomic_fetch_add_explicit(&candidate->tx_refs, 1, memory_order_acq_rel);
						ch = candidate;
						discard = candidate->protocol_id->close_ordered;
						sess->trans.drain_next_ch = idx == n - 1 ? 0 : idx + 1;
						break;
					}
					idx = idx == n - 1 ? 0 : idx + 1;
				}
			}
			dssh_mtx_unlock(&sess->channel_mtx);
			if (ch == NULL)
				break;
			result = drain_one_wa_slot(sess, ch, &progress, discard);
			if (result == 0)
				result = drain_one_slot(sess, &ch->chan_reply_slot, &progress, discard);
			atomic_fetch_sub_explicit(&ch->tx_refs, 1, memory_order_acq_rel);
			if (result < 0)
				break;
		}
	}
	if (result < 0 && !tx_drain_refused(sess, result))
		session_set_terminate(sess);
	if (progress) {
		int lr = mtx_lock(&sess->trans.tx_queue_mtx);
		if (lr != thrd_success) {
			session_set_terminate(sess);
			return DSSH_ERROR_TERMINATED;
		}
		int br = cnd_broadcast(&sess->trans.tx_slot_cnd);
		dssh_mtx_unlock(&sess->trans.tx_queue_mtx);
		if (br != thrd_success) {
			session_set_terminate(sess);
			return DSSH_ERROR_TERMINATED;
		}
	}
	return sess->terminate ? DSSH_ERROR_TERMINATED : result;
}

DSSH_TESTABLE void
tx_wake_owners(struct dssh_session_s *sess)
{
	int lr = mtx_lock(&sess->trans.tx_mtx);

	if (lr != thrd_success) {
		dssh_thrd_check(sess, lr);
		return;
	}
	int br = cnd_broadcast(&sess->trans.rekey_cnd);
	int ur = dssh_mtx_unlock(&sess->trans.tx_mtx);

	if (ur != thrd_success)
		dssh_thrd_check_unlock(sess, ur);
	else if (br != thrd_success)
		dssh_thrd_check(sess, br);
}

/* Acquire exclusive ownership of the packet engine.  tx_mtx is only held
 * while testing/updating predicates and is released before this returns. */
DSSH_PRIVATE int
tx_acquire(struct dssh_session_s *sess, uint8_t msg_type)
{
	if (dssh_channel_rx_active() || dssh_io_callback_active(sess) || dssh_setup_callback_active(sess))
		return DSSH_ERROR_INVALID;
	if (tx_owned_session != NULL)
		return DSSH_ERROR_INVALID;
	struct timespec ts;
	if (sess->timeout_ms > 0) {
		int ret = dssh_deadline_from_ms(&ts, sess->timeout_ms);
		if (ret < 0) {
			session_set_terminate(sess);
			return DSSH_ERROR_TERMINATED;
		}
	}
	int lr = mtx_lock(&sess->trans.tx_mtx);

	if (lr != thrd_success) {
		dssh_thrd_check(sess, lr);
		return DSSH_ERROR_TERMINATED;
	}
	while ((atomic_load_explicit(&sess->trans.tx_owner_active, memory_order_acquire)
	           || (msg_type >= SSH_MSG_USERAUTH_REQUEST
	               && atomic_load_explicit(&sess->trans.rekey_in_progress, memory_order_acquire)))
	    && !sess->terminate) {
		int wr;

		if (sess->timeout_ms <= 0)
			wr = cnd_wait(&sess->trans.rekey_cnd, &sess->trans.tx_mtx);
		else
			wr = cnd_timedwait(&sess->trans.rekey_cnd, &sess->trans.tx_mtx, &ts);
		if (wr == thrd_timedout) {
			int ur = dssh_mtx_unlock(&sess->trans.tx_mtx);

			if (ur != thrd_success) {
				dssh_thrd_check_unlock(sess, ur);
			}
			else
				session_set_terminate(sess);
			return DSSH_ERROR_TERMINATED;
		}
		if (wr != thrd_success) {
			int ur = dssh_mtx_unlock(&sess->trans.tx_mtx);

			if (ur != thrd_success) {
				dssh_thrd_check_unlock(sess, ur);
			}
			else
				dssh_thrd_check(sess, wr);
			return DSSH_ERROR_TERMINATED;
		}
	}
	if (sess->terminate) {
		int ur = dssh_mtx_unlock(&sess->trans.tx_mtx);

		if (ur != thrd_success)
			dssh_thrd_check_unlock(sess, ur);
		return DSSH_ERROR_TERMINATED;
	}
	tx_owned_session = sess;
	atomic_store_explicit(&sess->trans.tx_owner_active, true, memory_order_release);
	int ur = dssh_mtx_unlock(&sess->trans.tx_mtx);

	if (ur != thrd_success) {
		tx_owned_session = NULL;
		atomic_store_explicit(&sess->trans.tx_owner_active, false, memory_order_release);
		dssh_thrd_check_unlock(sess, ur);
		return DSSH_ERROR_TERMINATED;
	}
	return 0;
}

DSSH_PRIVATE bool
tx_slot_context_allowed(struct dssh_session_s *sess)
{
	return (tx_owned_session == NULL || tx_owned_session == sess)
	    && !dssh_channel_rx_active() && !dssh_io_callback_active(sess)
	    && !dssh_setup_callback_active(sess);
}

/* Try the engine predicate without waiting for ownership. Acquire the short
 * state mutex normally so transient mutex contention cannot strand a slot
 * with no engine owner to drain it. Caller holds no physical lock. */
static int
tx_try_acquire(struct dssh_session_s *sess, uint8_t msg_type)
{
	if (tx_owned_session == sess)
		return thrd_busy;
	int lr = mtx_lock(&sess->trans.tx_mtx);
	if (lr != thrd_success)
		return lr;
	int ret = thrd_busy;
	if (!sess->terminate
	    && !atomic_load_explicit(&sess->trans.tx_owner_active, memory_order_acquire)
	    && (msg_type < SSH_MSG_USERAUTH_REQUEST
	        || !atomic_load_explicit(&sess->trans.rekey_in_progress, memory_order_acquire))) {
		tx_owned_session = sess;
		atomic_store_explicit(&sess->trans.tx_owner_active, true, memory_order_release);
		ret = thrd_success;
	}
	dssh_mtx_unlock(&sess->trans.tx_mtx);
	return ret;
}

DSSH_PRIVATE bool
tx_owner_is_current(struct dssh_session_s *sess)
{
	return tx_owned_session == sess;
}

/* Called after the final slot drain, with queue mutex and engine owned.
 * Claiming the read boundary prevents dispatch of a second occupied-slot
 * reply while the receive owner must finish this exchange. */
static bool
claim_rekey_read(struct dssh_session_s *sess)
{
	if (session_on_demux_thread(sess)
	    || atomic_load(&sess->trans.rekey_read) != DSSH_REKEY_READ_AVAILABLE
	    || !rekey_transmit_due(sess))
		return false;
	enum dssh_rekey_read_state expected = DSSH_REKEY_READ_AVAILABLE;
	if (!atomic_compare_exchange_strong(&sess->trans.rekey_read, &expected,
	    DSSH_REKEY_READ_CLAIMED))
		return false;
	atomic_store(&sess->trans.rekey_in_progress, true);
	return true;
}

static void
send_claimed_rekey_offer(struct dssh_session_s *sess)
{
	int ret = prepare_kexinit(sess);
	if (ret == 0)
		ret = send_packet_inner(sess, sess->trans.our_kexinit, sess->trans.our_kexinit_sz, NULL);
	if (ret == 0)
		sess->trans.rekey_offer_sent = true;
	else
		session_set_terminate_no_wake(sess);
	tx_owned_session = NULL;
	atomic_store_explicit(&sess->trans.tx_owner_active, false, memory_order_release);
	if (ret < 0) {
		DSSH_LOGF(sess, DSSH_LOG_ERROR, ret, "Failed to start transmit rekey: %s",
		    dssh_strerror(ret));
	}
	if (sess->terminate)
		session_set_terminate(sess);
	tx_wake_owners(sess);
}

/* Drain slots queued while a TX callback was active, then publish the
 * packet engine as free.  tx_queue_mtx closes the otherwise-stranded gap
 * between the last ready check and ownership release. */
DSSH_PRIVATE void
tx_unlock_with_slots(struct dssh_session_s *sess)
{
	if (!tx_owner_is_current(sess))
		return;
	if (sess->terminate) {
		tx_owned_session = NULL;
		atomic_store_explicit(&sess->trans.tx_owner_active, false, memory_order_release);
		/* A stateful transform may have failed with a normally recoverable
		 * error. Complete deferred notification after caller storage is clear. */
		session_set_terminate(sess);
		tx_wake_owners(sess);
		return;
	}
	/* Connection-layer slots must not cross a key-exchange boundary.
	 * Leave them queued until rekey clears the predicate. */
	if (atomic_load_explicit(&sess->trans.rekey_in_progress, memory_order_acquire)) {
		tx_owned_session = NULL;
		atomic_store_explicit(&sess->trans.tx_owner_active, false, memory_order_release);
		tx_wake_owners(sess);
		return;
	}

	for (;;) {
		int lr = mtx_lock(&sess->trans.tx_queue_mtx);

		if (lr != thrd_success) {
			tx_owned_session = NULL;
			atomic_store_explicit(&sess->trans.tx_owner_active, false, memory_order_release);
			dssh_thrd_check(sess, lr);
			tx_wake_owners(sess);
			return;
		}
		sess->trans.tx_slots_pending = false;
		int ur = dssh_mtx_unlock(&sess->trans.tx_queue_mtx);

		if (ur != thrd_success) {
			tx_owned_session = NULL;
			atomic_store_explicit(&sess->trans.tx_owner_active, false, memory_order_release);
			dssh_thrd_check_unlock(sess, ur);
			tx_wake_owners(sess);
			return;
		}

		int drain_ret = drain_tx_slots(sess);
		if (sess->terminate) {
			tx_owned_session = NULL;
			atomic_store_explicit(&sess->trans.tx_owner_active, false, memory_order_release);
			session_set_terminate(sess);
			tx_wake_owners(sess);
			return;
		}

		lr = mtx_lock(&sess->trans.tx_queue_mtx);
		if (lr != thrd_success) {
			tx_owned_session = NULL;
			atomic_store_explicit(&sess->trans.tx_owner_active, false, memory_order_release);
			dssh_thrd_check(sess, lr);
			tx_wake_owners(sess);
			return;
		}
		if (drain_ret < 0)
			sess->trans.tx_slots_pending = true;
		if (drain_ret == 0 && sess->trans.tx_slots_pending) {
			ur = dssh_mtx_unlock(&sess->trans.tx_queue_mtx);
			if (ur != thrd_success) {
				tx_owned_session = NULL;
				atomic_store_explicit(&sess->trans.tx_owner_active, false, memory_order_release);
				dssh_thrd_check_unlock(sess, ur);
				tx_wake_owners(sess);
				return;
			}
			continue;
		}
		sess->trans.tx_slot_refusal = drain_ret;
		if (drain_ret == 0 && claim_rekey_read(sess)) {
			dssh_mtx_unlock(&sess->trans.tx_queue_mtx);
			send_claimed_rekey_offer(sess);
			return;
		}
		tx_owned_session = NULL;
		atomic_store_explicit(&sess->trans.tx_owner_active, false, memory_order_release);
		/* The occupied predicate stayed true. Wake producers so they can
		 * observe the refusal instead of waiting for an idle engine. */
		int br = drain_ret < 0 ? cnd_broadcast(&sess->trans.tx_slot_cnd) : thrd_success;
		dssh_mtx_unlock(&sess->trans.tx_queue_mtx);
		if (br != thrd_success)
			dssh_thrd_check(sess, br);
		tx_wake_owners(sess);
		return;
	}
}

/* Report failures that occur while releasing ownership or draining replies. */
DSSH_PRIVATE int
tx_unlock_with_result(struct dssh_session_s *sess, int result)
{
	tx_unlock_with_slots(sess);
	if (result == 0 && sess->terminate)
		return DSSH_ERROR_TERMINATED;
	return result;
}

/*
 * Gather queued control packets with the caller's packet. Each retired slot
 * has an independently retained wire copy, including compressed output, and
 * reserves its packet budget before the next packet is prepared. Bounded full
 * batches are sent before collecting further controls. The final caller packet
 * remains in its original wire buffer until callback return.
 * Caller MUST own the packet engine.
 */
DSSH_PRIVATE int
tx_gather_with_packet(struct dssh_session_s *sess, uint8_t *buf, size_t payload_len)
{
	if (tx_batch != NULL)
		return DSSH_ERROR_INVALID;
	struct dssh_tx_batch batch = {.sess = sess};
	tx_batch = &batch;
	int ret = 0;
	if (!atomic_load_explicit(&sess->trans.rekey_in_progress, memory_order_acquire)) {
		ret = drain_tx_slots(sess);
		if (ret < 0) {
			/* Earlier controls may already be prepared when a later slot is
			 * refused before touching stream state. Send that valid prefix. */
			if (tx_drain_refused(sess, ret)) {
				int flush_ret = tx_batch_flush(&batch, NULL, 0);
				if (flush_ret < 0)
					ret = flush_ret;
			}
			goto done;
		}
	}
	if (sess->terminate) {
		ret = DSSH_ERROR_TERMINATED;
		goto done;
	}

	uint8_t *wire;
	size_t   wire_len;
	ret = tx_finalize_prepare(sess, buf, payload_len, &wire, &wire_len);

	if (ret < 0) {
		if (tx_drain_refused(sess, ret)) {
			int flush_ret = tx_batch_flush(&batch, NULL, 0);
			if (flush_ret < 0)
				ret = flush_ret;
		}
		goto done;
	}
	/* The complete vector's byte sum must also fit size_t. A narrow platform
	 * may need to send its valid control prefix separately from a large tail. */
	if (wire_len > SIZE_MAX - batch.bytes) {
		ret = tx_batch_flush(&batch, NULL, 0);
		if (ret < 0)
			goto done;
	}
	ret = tx_batch_flush(&batch, wire, wire_len);
	if (ret == 0)
		tx_update_counters(sess, wire_len);

done:
	if (batch.bytes != 0)
		dssh_cleanse(sess->trans.tx_gather_buf, batch.bytes);
	tx_batch = NULL;
	if (sess->trans.comp_tx_packet != NULL) {
		dssh_cleanse(sess->trans.comp_tx_packet, 4);
		dssh_cleanse(&sess->trans.comp_tx_packet[4], sess->trans.packet_buf_sz);
	}
	return ret;
}

/*
 * Write a fire-and-forget payload to a pre-allocated TX slot.
 * Fast path: reserve the packet engine, drain slots, finalize+send directly.
 * Slow path: wait for slot to become free, write payload, mark ready.
 *
 * The stall on an occupied slot applies RX backpressure — the demux
 * thread stops reading packets until the slot is drained by the next
 * send_packet() caller.
 */
static int
send_to_slot_inner(struct dssh_session_s *sess, struct dssh_tx_slot *slot,
    const uint8_t *payload, size_t payload_len, struct dssh_channel_s *channel)
{
	if (payload == NULL || payload_len == 0 || payload_len > UINT8_MAX
	    || !tx_slot_context_allowed(sess))
		return DSSH_ERROR_INVALID;
	if (sess->terminate)
		return DSSH_ERROR_TERMINATED;

	/* Fast path: try to reserve the packet engine immediately */
	int tr = tx_try_acquire(sess, payload[0]);

	if (tr == thrd_success) {
		/* Drain any prior slow-path payload, then use tx_packet for this
		 * fast send.  A new slow producer may be filling slot->buf under
		 * tx_queue_mtx, so the fast path must not write slot storage. */
		int drain_ret = drain_tx_slots(sess);
		if (drain_ret < 0)
			return tx_unlock_with_result(sess, drain_ret);
		if (sess->terminate) {
			tx_unlock_with_slots(sess);
			return DSSH_ERROR_TERMINATED;
		}
		/* A channel failure prepared before CLOSE can acquire the engine
		 * after it. The queued path checks the same state in the drainer. */
		if (channel != NULL) {
			int lr = mtx_lock(&sess->channel_mtx);
			if (lr != thrd_success) {
				session_set_terminate_no_wake(sess);
				return tx_unlock_with_result(sess, DSSH_ERROR_TERMINATED);
			}
			bool closed = channel->protocol_id->close_ordered;
			dssh_mtx_unlock(&sess->channel_mtx);
			if (closed)
				return tx_unlock_with_result(sess, 0);
		}
		int ret = send_packet_inner(sess, payload, payload_len, NULL);

		if ((ret < 0) && (ret != DSSH_ERROR_TOOLONG) && (ret != DSSH_ERROR_REKEY_NEEDED))
			session_set_terminate(sess);
		return tx_unlock_with_result(sess, ret);
	}
	if (tr != thrd_busy) {
		dssh_thrd_check(sess, tr);
		return DSSH_ERROR_TERMINATED;
	}

	/* Only the queue lock exposes live slot storage to a producer. Recheck
	 * the buffer and occupancy after each native condition wait. */
	if (dssh_thrd_check(sess, mtx_lock(&sess->trans.tx_queue_mtx)) != thrd_success)
		return DSSH_ERROR_TERMINATED;
	for (;;) {
		if (sess->terminate) {
			dssh_mtx_unlock(&sess->trans.tx_queue_mtx);
			return DSSH_ERROR_TERMINATED;
		}
		if (slot->buf == NULL) {
			/* An unallocated slot can only be sent on the direct path. */
			dssh_mtx_unlock(&sess->trans.tx_queue_mtx);
			session_set_terminate(sess);
			return DSSH_ERROR_TERMINATED;
		}
		if (payload_len > slot->payload_capacity
		    || !dssh_buffer_has(slot->buf_sz, 9, payload_len)) {
			dssh_mtx_unlock(&sess->trans.tx_queue_mtx);
			return DSSH_ERROR_TOOLONG;
		}
		if (!atomic_load(&slot->ready))
			break;
		if (tx_owned_session == sess) {
			dssh_mtx_unlock(&sess->trans.tx_queue_mtx);
			return DSSH_ERROR_INVALID;
		}
		if (sess->trans.tx_slot_refusal < 0
		    && !atomic_load(&sess->trans.tx_owner_active)
		    && !atomic_load(&sess->trans.rekey_in_progress)) {
			int refusal = sess->trans.tx_slot_refusal;
			dssh_mtx_unlock(&sess->trans.tx_queue_mtx);
			return refusal;
		}
		if (cnd_wait(&sess->trans.tx_slot_cnd, &sess->trans.tx_queue_mtx) != thrd_success) {
			dssh_mtx_unlock(&sess->trans.tx_queue_mtx);
			session_set_terminate(sess);
			return DSSH_ERROR_TERMINATED;
		}
	}

	memcpy(&slot->buf[9], payload, payload_len);
	slot->payload_len = (uint8_t)payload_len;
	atomic_store(&slot->ready, true);
	sess->trans.tx_slots_pending = true;
	if (dssh_thrd_check_unlock(sess, dssh_mtx_unlock(&sess->trans.tx_queue_mtx)) != thrd_success)
		return DSSH_ERROR_TERMINATED;

	tr = tx_try_acquire(sess, payload[0]);
	if (tr == thrd_success)
		return tx_unlock_with_result(sess, 0);
	else if (tr != thrd_busy) {
		dssh_thrd_check(sess, tr);
		return DSSH_ERROR_TERMINATED;
	}
	return 0;
}

DSSH_PRIVATE int
send_to_slot(struct dssh_session_s *sess, struct dssh_tx_slot *slot, const uint8_t *payload,
    size_t payload_len)
{
	return send_to_slot_inner(sess, slot, payload, payload_len, NULL);
}

DSSH_PRIVATE int
send_channel_reply(struct dssh_session_s *sess, struct dssh_channel_s *ch,
    const uint8_t *payload, size_t payload_len)
{
	return send_to_slot_inner(sess, &ch->chan_reply_slot, payload, payload_len, ch);
}

/*
 * Mark a pre-reserved CHANNEL_WINDOW_ADJUST ready for the current packet
 * engine owner.  If the engine is free, this caller drains it immediately.
 */
DSSH_PRIVATE int
schedule_wa_slot(struct dssh_session_s *sess)
{
	if (!tx_slot_context_allowed(sess))
		return DSSH_ERROR_INVALID;
	if (sess->terminate)
		return DSSH_ERROR_TERMINATED;

	if (dssh_thrd_check(sess, mtx_lock(&sess->trans.tx_queue_mtx)) != thrd_success)
		return DSSH_ERROR_TERMINATED;
	sess->trans.tx_slots_pending = true;
	if (dssh_thrd_check_unlock(sess, dssh_mtx_unlock(&sess->trans.tx_queue_mtx)) != thrd_success)
		return DSSH_ERROR_TERMINATED;

	int tr = tx_try_acquire(sess, SSH_MSG_CHANNEL_WINDOW_ADJUST);

	if (tr == thrd_success)
		return tx_unlock_with_result(sess, 0);
	else if (tr != thrd_busy) {
		dssh_thrd_check(sess, tr);
		return DSSH_ERROR_TERMINATED;
	}
	return 0;
}

DSSH_PRIVATE int
send_packet(struct dssh_session_s *sess, const uint8_t *payload, size_t payload_len, uint32_t *seq_out)
{
	int      err;
	uint8_t  msg_type = (payload_len > 0) ? payload[0] : 0;
	size_t   max;
	uint8_t *buf = send_begin(sess, msg_type, &max, &err);

	if (buf == NULL)
		return err;
	if (payload_len > max) {
		send_cancel(sess);
		return DSSH_ERROR_TOOLONG;
	}
	memcpy(buf, payload, payload_len);
	return send_commit(sess, payload_len, seq_out);
}

/*
 * Reserve the packet engine, block on rekey for application messages
 * (type >= 50), and drain pending tx slots.  Returns a pointer into
 * tx_packet[9] where the
 * caller serializes the payload directly.  *max_len is the maximum
 * payload the caller may write.  Caller MUST call send_commit(),
 * send_commit_sensitive(), or send_cancel() to release the reservation.
 * A sender publishing packet-dependent state may instead use send_commit_held()
 * and finish with tx_unlock_with_result() after that publication.
 *
 * Returns NULL on error (timeout, terminate); *err_out is set.
 */
DSSH_PRIVATE uint8_t *
send_begin(struct dssh_session_s *sess, uint8_t msg_type, size_t *max_len, int *err_out)
{
	int acquire_ret = tx_acquire(sess, msg_type);

	if (acquire_ret < 0) {
		*err_out = acquire_ret;
		return NULL;
	}

	if (!tx_gather_enabled()
	    && !atomic_load_explicit(&sess->trans.rekey_in_progress, memory_order_acquire)) {
		int drain_ret = drain_tx_slots(sess);
		if (drain_ret < 0) {
			*err_out = tx_unlock_with_result(sess, drain_ret);
			return NULL;
		}
	}
	if (sess->terminate) {
		tx_unlock_with_slots(sess);
		*err_out = DSSH_ERROR_TERMINATED;
		return NULL;
	}
	int log_ret = drain_log_forwards(sess);

	if (log_ret < 0 && log_ret != DSSH_ERROR_TOOLONG && log_ret != DSSH_ERROR_REKEY_NEEDED) {
		atomic_store_explicit(&sess->log_mirror_suppressed, true, memory_order_relaxed);
		tx_unlock_with_slots(sess);
		DSSH_LOGF(sess, DSSH_LOG_ERROR, log_ret, "Failed to forward SSH diagnostic: %s",
		    dssh_strerror(log_ret));
		session_set_terminate(sess);
		*err_out = log_ret;
		return NULL;
	}

	*max_len = tx_payload_capacity(sess);
	return &sess->trans.tx_packet[9];
}

/* Finalize and send the payload already at tx_packet[9], retaining the
 * packet engine for caller state publication. When sensitive is true, scrub
 * the payload before invoking a termination callback. */
static int
send_commit_inner(struct dssh_session_s *sess, size_t payload_len, uint32_t *seq_out,
    bool sensitive, bool required)
{
	if (!tx_owner_is_current(sess))
		return DSSH_ERROR_INVALID;
	int ret;
	size_t max_payload = tx_payload_capacity(sess);

	if (payload_len > max_payload) {
		ret = DSSH_ERROR_TOOLONG;
		goto done;
	}

	if (tx_gather_enabled()) {
		ret = tx_gather_with_packet(sess, sess->trans.tx_packet, payload_len);
		if (ret == 0 && seq_out != NULL)
			*seq_out = sess->trans.tx_seq - 1;
	}
	else {
		ret = tx_finalize(sess, sess->trans.tx_packet, payload_len);
		if (ret == 0 && seq_out != NULL)
			*seq_out = sess->trans.tx_seq - 1;
	}

done:
	if (sensitive) {
		size_t cleanse_len = payload_len < max_payload ? payload_len : max_payload;

		dssh_cleanse(&sess->trans.tx_packet[9], cleanse_len);
	}
	if (ret < 0 && (required || (ret != DSSH_ERROR_TOOLONG && ret != DSSH_ERROR_REKEY_NEEDED)))
		session_set_terminate(sess);
	return ret;
}

/* Retain the engine until the caller has published its packet-dependent
 * state. The caller must finish with tx_unlock_with_result(). */
DSSH_PRIVATE int
send_commit_held(struct dssh_session_s *sess, size_t payload_len)
{
	return send_commit_inner(sess, payload_len, NULL, false, false);
}

/*
 * Finalize and send the payload already at tx_packet[9].
 * Releases packet-engine ownership.  Caller MUST have called send_begin() first.
 */
DSSH_PRIVATE int
send_commit(struct dssh_session_s *sess, size_t payload_len, uint32_t *seq_out)
{
	int ret = send_commit_inner(sess, payload_len, seq_out, false, false);
	return tx_unlock_with_result(sess, ret);
}

/* A protocol ownership transition already orders this packet ahead of later
 * traffic. Terminate on pre-wire refusal before relinquishing the engine. */
DSSH_PRIVATE int
send_commit_required(struct dssh_session_s *sess, size_t payload_len)
{
	int ret = send_commit_inner(sess, payload_len, NULL, false, true);
	return tx_unlock_with_result(sess, ret);
}

/*
 * Finalize, send, and scrub a sensitive payload while packet-engine ownership
 * remains reserved.  Caller MUST have called send_begin() first.
 */
DSSH_PRIVATE int
send_commit_sensitive(struct dssh_session_s *sess, size_t payload_len, uint32_t *seq_out)
{
	int ret = send_commit_inner(sess, payload_len, seq_out, true, false);
	return tx_unlock_with_result(sess, ret);
}

/*
 * Release packet-engine ownership without sending.  Use when serialization fails
 * after send_begin() but before either commit function.
 */
DSSH_PRIVATE void
send_cancel(struct dssh_session_s *sess)
{
	if (!tx_owner_is_current(sess))
		return;
	tx_unlock_with_slots(sess);
}

/*
 * Receive a single raw packet.  Does NOT handle transport messages
 * (IGNORE, DEBUG, UNIMPLEMENTED, DISCONNECT) -- that's done by the
 * public recv_packet wrapper below.
 */
DSSH_TESTABLE int
recv_packet_raw(struct dssh_session_s *sess, uint8_t *msg_type, uint8_t **payload, size_t *payload_len)
{
	int ret;

	/* Refuse to receive if per-key packet count would exceed hard limit */
	if (sess->trans.rx_since_rekey >= DSSH_REKEY_HARD_LIMIT) {
		return DSSH_ERROR_REKEY_NEEDED;
	}

	size_t bs = rx_block_size(sess);

	if (bs > sess->trans.packet_buf_sz) {
		ret = DSSH_ERROR_INVALID;
		goto rx_done;
	}
	/* Receive into rx_packet[4..], leaving [0..4) for seq prefix */
	ret = call_io(sess, gconf.rx, &sess->trans.rx_packet[4], bs, sess->rx_cbdata);
	if (ret < 0)
		goto rx_done;

	/* Decrypt first block */
	dssh_enc      enc;
	dssh_enc_ctx *enc_ctx;

	if (sess->trans.client) {
		enc     = enc_s2c_module(sess);
		enc_ctx = sess->trans.enc_s2c_ctx;
	}
	else {
		enc     = enc_c2s_module(sess);
		enc_ctx = sess->trans.enc_c2s_ctx;
	}

	if ((enc != NULL) && (enc->decrypt != NULL)) {
		ret = enc->decrypt(&sess->trans.rx_packet[4], bs, enc_ctx);
		if (ret > 0)
			ret = DSSH_ERROR_INVALID;
		if (ret < 0)
			goto rx_done;
	}

	/* First block (>= 8 bytes) is decrypted; read packet_length */
	uint32_t packet_length = DSSH_GET_U32(&sess->trans.rx_packet[4]);

	if (packet_length < 2) {
		ret = DSSH_ERROR_PARSE;
		goto rx_done;
	}
	size_t packet_total;
	size_t mac_input;
	if (!dssh_packet_extents(packet_length, sess->trans.packet_buf_sz, &packet_total, &mac_input)) {
		ret = DSSH_ERROR_TOOLONG;
		goto rx_done;
	}
	if (packet_total < bs || packet_total % bs != 0) {
		ret = DSSH_ERROR_PARSE;
		goto rx_done;
	}

	size_t remaining = packet_total - bs;
	if (remaining > 0) {
		ret = call_io(sess, gconf.rx, &sess->trans.rx_packet[4 + bs], remaining, sess->rx_cbdata);
		if (ret < 0)
			goto rx_done;
		if ((enc != NULL) && (enc->decrypt != NULL)) {
			ret = enc->decrypt(&sess->trans.rx_packet[4 + bs], remaining, enc_ctx);
			if (ret > 0)
				ret = DSSH_ERROR_INVALID;
			if (ret < 0)
				goto rx_done;
		}
	}

	/* Verify MAC.  Write seq at rx_packet[0]; MAC input is
         * contiguous at rx_packet[0..4 + packet_length + 4). */
	uint16_t mac_len = rx_mac_size(sess);
	if (mac_len > 0) {
		ret = call_io(sess, gconf.rx, sess->trans.rx_mac_buf, mac_len, sess->rx_cbdata);
		if (ret < 0)
			goto rx_done;

		size_t seq_pos = 0;

		DSSH_PUT_U32(sess->trans.rx_seq, sess->trans.rx_packet, &seq_pos);

		dssh_mac      mac;
		dssh_mac_ctx *mac_ctx;

		if (sess->trans.client) {
			mac     = mac_s2c_module(sess);
			mac_ctx = sess->trans.mac_s2c_ctx;
		}
		else {
			mac     = mac_c2s_module(sess);
			mac_ctx = sess->trans.mac_c2s_ctx;
		}
		ret = mac->generate(sess->trans.rx_packet, mac_input, sess->trans.rx_mac_computed,
		    mac_ctx);
		if (ret > 0)
			ret = DSSH_ERROR_INVALID;
		if (ret < 0)
			goto rx_done;
		if (dssh_crypto_memcmp(sess->trans.rx_mac_buf, sess->trans.rx_mac_computed, mac_len) != 0) {
			ret = transport_disconnect_result(sess, SSH_DISCONNECT_MAC_ERROR,
			    "MAC verification failed", DSSH_ERROR_INVALID);
			goto rx_done;
		}
	}

	uint8_t padding_length = sess->trans.rx_packet[8];
	if (padding_length < 4 || padding_length + 1 > packet_length) {
		ret = DSSH_ERROR_PARSE;
		goto rx_done;
	}
	*payload     = &sess->trans.rx_packet[9];
	*payload_len = packet_total - padding_length - 5;
	dssh_comp comp = sess->trans.client ? sess->trans.comp_s2c_active : sess->trans.comp_c2s_active;
	if (compression_enabled(comp)) {
		dssh_comp_ctx *ctx = sess->trans.client ? sess->trans.comp_s2c_ctx : sess->trans.comp_c2s_ctx;
		memcpy(sess->trans.comp_rx_input, *payload, *payload_len);
		size_t decoded_len;
		ret = call_comp_transform(sess, comp->uncompress, sess->trans.comp_rx_input, *payload_len,
		    *payload, sess->trans.packet_buf_sz - 9, &decoded_len, ctx);
		dssh_cleanse(sess->trans.comp_rx_input, sess->trans.packet_buf_sz);
		if (ret < 0 || decoded_len == 0 || sess->terminate) {
			if (ret == 0)
				ret = sess->terminate ? DSSH_ERROR_TERMINATED : DSSH_ERROR_PARSE;
			dssh_cleanse(*payload, sess->trans.packet_buf_sz - 9);
			/* Every transform failure is final, including error codes that
			 * normally request a retry before packet processing starts. */
			session_set_terminate(sess);
			goto rx_done;
		}
		*payload_len = decoded_len;
	}
	else if (*payload_len == 0) {
		ret = DSSH_ERROR_PARSE;
		goto rx_done;
	}
	*msg_type    = sess->trans.rx_packet[9];

	DSSH_TRACE_W(sess, "rx msg_type=%d len=%zu", *msg_type, *payload_len);

#ifdef DSSH_TESTING
	struct dssh_session_s *count_sess = sess;

	if (atomic_compare_exchange_strong_explicit(&dssh_test_rx_count_sess,
	    &count_sess, NULL, memory_order_acq_rel, memory_order_acquire)) {
		sess->trans.rx_bytes_since_rekey = atomic_load_explicit(
		    &dssh_test_rx_count_value, memory_order_relaxed);
	}
#endif
	sess->trans.last_rx_seq = sess->trans.rx_seq;
	sess->trans.rx_seq++;
	sess->trans.rx_since_rekey++;
	{
		uint64_t rx_bytes = dssh_packet_wire_bytes(packet_length, mac_len);

		if (rx_bytes > UINT64_MAX - sess->trans.rx_bytes_since_rekey)
			sess->trans.rx_bytes_since_rekey = UINT64_MAX;
		else
			sess->trans.rx_bytes_since_rekey += rx_bytes;
	}
	ret = 0;

rx_done:

	/* The recoverable hard-limit refusal returns before any I/O. Every error
	 * reaching here follows packet processing, whose stream state is final. */
	if (ret < 0) {
		dssh_cleanse(sess->trans.rx_packet, 4 + sess->trans.packet_buf_sz);
		session_set_terminate(sess);
	}
	return ret;
}

/* Apply the same authentication phase rule to new packets and packets
 * replayed after a key exchange. */
static int
check_auth_receive_phase(struct dssh_session_s *sess, uint8_t msg_type)
{
	if (sess->server_auth_active && msg_type >= SSH_MSG_GLOBAL_REQUEST)
		return transport_disconnect_result(sess, SSH_DISCONNECT_PROTOCOL_ERROR,
		    "unexpected message during authentication", DSSH_ERROR_PARSE);
	return 0;
}

/* The public auth API takes an explicit algorithm, so no peer hint needs to
 * be retained. Validate recognized server values, and ignore all other binary
 * values without allocating or imposing a smaller bound than the packet. */
DSSH_TESTABLE int
parse_ext_info(const uint8_t *payload, size_t length, bool client)
{
	if (length < 5 || payload[0] != SSH_MSG_EXT_INFO)
		return DSSH_ERROR_PARSE;
	uint32_t count = DSSH_GET_U32(&payload[1]);
	size_t pos = 5;
	if (count > (length - pos) / 8)
		return DSSH_ERROR_PARSE;
	for (uint32_t i = 0; i < count; i++) {
		const uint8_t *name;
		const uint8_t *value;
		size_t name_length, value_length;
		if (!parse_log_string(payload, length, &pos, &name, &name_length)
		    || name_length == 0 || name_length > DSSH_ALGO_NAME_MAX
		    || !parse_log_string(payload, length, &pos, &value, &value_length))
			return DSSH_ERROR_PARSE;
		for (size_t j = 0; j < name_length; j++) {
			if (name[j] <= ' ' || name[j] >= DSSH_ASCII_DEL || name[j] == ',')
				return DSSH_ERROR_PARSE;
		}
		if (!client || name_length != DSSH_STRLEN("server-sig-algs")
		    || memcmp(name, "server-sig-algs", name_length) != 0)
			continue;
		size_t token_length = 0;
		for (size_t j = 0; j < value_length; j++) {
			if (value[j] == ',') {
				if (token_length == 0)
					return DSSH_ERROR_PARSE;
				token_length = 0;
			}
			else {
				if (value[j] <= ' ' || value[j] >= DSSH_ASCII_DEL
				    || token_length == DSSH_ALGO_NAME_MAX)
					return DSSH_ERROR_PARSE;
				token_length++;
			}
		}
		if (value_length != 0 && token_length == 0)
			return DSSH_ERROR_PARSE;
	}
	return pos == length ? 0 : DSSH_ERROR_PARSE;
}

/* Apply exactly once in physical receive order, before dispatch/rekey replay.
 * Optional extension packets do not leak into service/auth/channel receivers. */
static int
check_ext_info_position(struct dssh_session_s *sess, uint8_t type,
    const uint8_t *payload, size_t length)
{
	int ret;
	enum dssh_ext_info_state state = sess->trans.ext_info_state;
	/* Disconnect is permitted at every protocol position, including while
	 * authentication success would otherwise have to follow an extension. */
	if (type == SSH_MSG_DISCONNECT)
		return 0;
	if (state == DSSH_EXT_INFO_BEFORE_SUCCESS) {
		if (type != SSH_MSG_USERAUTH_SUCCESS || length != 1)
			return DSSH_ERROR_PARSE;
		sess->trans.ext_info_state = DSSH_EXT_INFO_CLOSED;
		return 0;
	}
	if (type == SSH_MSG_EXT_INFO) {
		bool first = state == DSSH_EXT_INFO_INITIAL;
		bool second = state == DSSH_EXT_INFO_AUTH && sess->trans.client
		    && sess->auth_service_requested && !sess->conn_initialized;
		if (!first && !second)
			return DSSH_ERROR_PARSE;
		ret = parse_ext_info(payload, length, sess->trans.client);
		if (ret < 0)
			return ret;
		sess->trans.ext_info_state = second ? DSSH_EXT_INFO_BEFORE_SUCCESS :
		    sess->trans.client ? DSSH_EXT_INFO_AUTH : DSSH_EXT_INFO_CLOSED;
		return 0;
	}
	if (state == DSSH_EXT_INFO_INITIAL)
		sess->trans.ext_info_state = sess->trans.client ? DSSH_EXT_INFO_AUTH : DSSH_EXT_INFO_CLOSED;
	if (sess->trans.client && (type == SSH_MSG_USERAUTH_SUCCESS || sess->conn_initialized))
		sess->trans.ext_info_state = DSSH_EXT_INFO_CLOSED;
	return 0;
}

/* The receive owner retains at most one bounded transcript per exchange. */
static int
retain_peer_kexinit(struct dssh_session_s *sess, const uint8_t *payload, size_t len)
{
	uint8_t *copy = malloc(len);
	if (copy == NULL)
		return DSSH_ERROR_ALLOC;
	memcpy(copy, payload, len);
	free(sess->trans.peer_kexinit);
	sess->trans.peer_kexinit = copy;
	sess->trans.peer_kexinit_sz = len;
	return 0;
}

static int
queue_rekey_packet(struct dssh_session_s *sess, uint8_t msg_type, const uint8_t *payload, size_t len)
{
	if (sess->trans.rekey_queue_count >= DSSH_REKEY_QUEUE_MAX_MESSAGES)
		return DSSH_ERROR_TOOLONG;
	if (len > SIZE_MAX - sizeof(struct dssh_rekey_msg))
		return DSSH_ERROR_TOOLONG;
	size_t alloc_size = sizeof(struct dssh_rekey_msg) + len;
	if (alloc_size > DSSH_REKEY_QUEUE_MAX_BYTES
	    || alloc_size > SIZE_MAX - sess->trans.rekey_queue_bytes
	    || sess->trans.rekey_queue_bytes > DSSH_REKEY_QUEUE_MAX_BYTES - alloc_size)
		return DSSH_ERROR_TOOLONG;

	/* Buffer for replay after rekey completes */
	struct dssh_rekey_msg *rmsg = malloc(alloc_size);
	if (rmsg == NULL)
		return DSSH_ERROR_ALLOC;
	rmsg->next     = NULL;
	rmsg->msg_type = msg_type;
	rmsg->sequence = sess->trans.last_rx_seq;
	rmsg->len      = len;
	memcpy(rmsg->data, payload, len);
	if (sess->trans.rekey_queue_tail != NULL)
		sess->trans.rekey_queue_tail->next = rmsg;
	else
		sess->trans.rekey_queue_head = rmsg;
	sess->trans.rekey_queue_tail = rmsg;
	sess->trans.rekey_queue_count++;
	sess->trans.rekey_queue_bytes += alloc_size;
	return 0;
}

/*
 * Public recv_packet: transparently handles transport messages
 * per RFC 4253 s11:
 *   SSH_MSG_IGNORE (2)        -- silently discarded
 *   SSH_MSG_DEBUG (4)         -- invokes debug callback if set, then discarded
 *   SSH_MSG_UNIMPLEMENTED (3) -- silently discarded (see TODO.md item 44)
 *   SSH_MSG_DISCONNECT (1)    -- sets terminate, returns error
 */
static int
recv_packet_inner(struct dssh_session_s *sess, uint8_t *msg_type, uint8_t **payload, size_t *payload_len)
{
	int res;
	bool consumed_packet = false;
	bool finish_transmit_rekey = false;
	for (;;) {
		if (atomic_load_explicit(&sess->terminate, memory_order_acquire))
			return DSSH_ERROR_TERMINATED;
		/* Control messages have finished their callbacks before continuing
		 * here. No borrowed payload is live, so they share the same deferred
		 * threshold check as messages returned to the application below. */
		if (consumed_packet && sess->trans.session_id != NULL
		    && !sess->trans.rekey_in_progress && rekey_needed(sess))
			sess->trans.rekey_pending = true;
		if (sess->terminate)
			return DSSH_ERROR_TERMINATED;
		if (finish_transmit_rekey
		    || (sess->trans.rekey_pending && !sess->trans.rekey_in_progress)) {
			finish_transmit_rekey = false;
			sess->trans.rekey_pending = false;
			res = rekey(sess);
			if (res < 0)
				return res;
		}

		bool replayed = false;
		/* A rekey completed inside this receive loop may have buffered
		 * application messages. Replay them before another physical read,
		 * preserving their original receive sequence and authentication gate. */
		if (!sess->trans.rekey_in_progress && sess->trans.rekey_queue_head != NULL) {
			struct dssh_rekey_msg *rmsg = sess->trans.rekey_queue_head;

			sess->trans.rekey_queue_head = rmsg->next;
			if (sess->trans.rekey_queue_head == NULL)
				sess->trans.rekey_queue_tail = NULL;
			sess->trans.rekey_queue_count--;
			sess->trans.rekey_queue_bytes -= sizeof(*rmsg) + rmsg->len;
			memcpy(&sess->trans.rx_packet[5], rmsg->data, rmsg->len);
			*payload = &sess->trans.rx_packet[5];
			*payload_len = rmsg->len;
			*msg_type = rmsg->msg_type;
			sess->trans.last_rx_seq = rmsg->sequence;
			cleanse_free(rmsg, sizeof(*rmsg) + rmsg->len);
			replayed = true;
		}
		else {
			bool advertise_read = session_on_demux_thread(sess)
			    && sess->trans.session_id != NULL && !sess->trans.rekey_in_progress;
			if (advertise_read) {
				atomic_store(&sess->trans.rekey_read, DSSH_REKEY_READ_AVAILABLE);
				/* Close the race with a send finishing just before the
				 * receive boundary was published. */
				if (rekey_transmit_due(sess)) {
					atomic_store(&sess->trans.rekey_read, DSSH_REKEY_READ_IDLE);
					finish_transmit_rekey = true;
					continue;
				}
			}
			res = recv_packet_raw(sess, msg_type, payload, payload_len);
			if (advertise_read)
				finish_transmit_rekey = atomic_exchange(&sess->trans.rekey_read,
				    DSSH_REKEY_READ_IDLE) == DSSH_REKEY_READ_CLAIMED;
			if (res < 0)
				return res;
			res = check_ext_info_position(sess, *msg_type, *payload, *payload_len);
			if (res < 0)
				return transport_disconnect_result(sess, SSH_DISCONNECT_PROTOCOL_ERROR,
				    "unexpected or malformed SSH_MSG_EXT_INFO", res);
		}
		/* Include globals intercepted below in the authentication gate. */
		res = check_auth_receive_phase(sess, *msg_type);
		if (res < 0)
			return res;
		/* An intercepted KEXINIT completes its own exchange below. */
		consumed_packet = !replayed && *msg_type != SSH_MSG_KEXINIT;
		/* A diagnostic callback may itself try to send. Keep it behind
		 * the exchange when this receive owner must finish rekey. */
		bool defer_callbacks = finish_transmit_rekey
		    || (sess->trans.session_id != NULL && sess->trans.rekey_in_progress);

		switch (*msg_type) {
			case SSH_MSG_EXT_INFO:
				continue;
			case SSH_MSG_DISCONNECT:
			{
				size_t         pos = 1;
				const uint8_t *description;
				const uint8_t *language;
				size_t         description_len;
				size_t         language_len;
				uint32_t       reason = 0;
				bool valid = dssh_buffer_has(*payload_len, pos, 4);

				if (valid) {
					reason = DSSH_GET_U32(&(*payload)[pos]);
					pos += 4;
					valid = reason != 0 && parse_log_string(*payload, *payload_len, &pos,
					    &description, &description_len);
				}
				if (valid)
					valid = parse_log_string(*payload, *payload_len, &pos, &language, &language_len);
				if (valid)
					valid = pos == *payload_len;
				if (valid) {
					dssh_log_emit(sess, DSSH_LOG_ERROR, DSSH_LOG_SOURCE_PEER_DISCONNECT,
					    DSSH_ERROR_TERMINATED, reason, false, false, description, description_len,
					    language, language_len);
				}
				else {
					DSSH_LOGF(sess, DSSH_LOG_ERROR, DSSH_ERROR_PARSE,
					    "Malformed SSH_MSG_DISCONNECT received from peer");
				}
				session_set_terminate(sess);
				dssh_log_termination(sess, DSSH_ERROR_TERMINATED);
				return DSSH_ERROR_TERMINATED;
			}
			case SSH_MSG_IGNORE:
				continue;
			case SSH_MSG_KEXINIT:
				/* Initial and nested receives return the offer to their
				 * current exchange. The outer receiver takes over a
				 * transmit-started exchange or starts a peer-initiated one. */
				if (sess->trans.session_id == NULL
				    || (sess->trans.rekey_in_progress && !finish_transmit_rekey))
					return 0;
				res = retain_peer_kexinit(sess, *payload, *payload_len);
				if (res < 0)
					return res;
				finish_transmit_rekey = true;
				continue;
			case SSH_MSG_DEBUG:
			{
				if (*payload_len < 2) {
					DSSH_LOGF(sess, DSSH_LOG_ERROR, DSSH_ERROR_PARSE,
					    "Malformed SSH_MSG_DEBUG received from peer");
					return transport_disconnect_result(sess, SSH_DISCONNECT_PROTOCOL_ERROR,
					    "malformed SSH_MSG_DEBUG", DSSH_ERROR_PARSE);
				}
				bool           always_display = (*payload)[1] != 0;
				size_t         pos = 2;
				const uint8_t *message;
				const uint8_t *language;
				size_t         message_len;
				size_t         language_len;
				bool valid = parse_log_string(*payload, *payload_len, &pos, &message, &message_len);

				if (valid)
					valid = parse_log_string(*payload, *payload_len, &pos, &language, &language_len);
				if (valid)
					valid = pos == *payload_len;
				if (!valid) {
					DSSH_LOGF(sess, DSSH_LOG_ERROR, DSSH_ERROR_PARSE,
					    "Malformed SSH_MSG_DEBUG received from peer");
					return transport_disconnect_result(sess, SSH_DISCONNECT_PROTOCOL_ERROR,
					    "malformed SSH_MSG_DEBUG", DSSH_ERROR_PARSE);
				}
				if (defer_callbacks) {
					res = queue_rekey_packet(sess, *msg_type, *payload, *payload_len);
					if (res < 0)
						return res;
					continue;
				}
				dssh_debug_cb dcb = sess->debug_cb;

				if (dcb != NULL)
					dcb(always_display, message, message_len, sess->debug_cbdata);
				dssh_log_emit(sess, DSSH_LOG_DEBUG, DSSH_LOG_SOURCE_PEER_DEBUG, DSSH_ERROR_NONE, 0,
				    always_display, false, message, message_len, language, language_len);
				continue;
			}
			case SSH_MSG_UNIMPLEMENTED:
			{
				if (defer_callbacks && *payload_len >= 5) {
					res = queue_rekey_packet(sess, *msg_type, *payload, *payload_len);
					if (res < 0)
						return res;
					continue;
				}
				dssh_unimplemented_cb ucb = sess->unimplemented_cb;

				if ((ucb != NULL) && (*payload_len >= 5)) {
					uint32_t rejected_seq = DSSH_GET_U32(&(*payload)[1]);

					ucb(rejected_seq, sess->unimplemented_cbdata);
				}
				continue;
			}
			case SSH_MSG_GLOBAL_REQUEST:
			{
				size_t pos = 1;
				const uint8_t *name;
				size_t name_len;
				bool valid = parse_log_string(*payload, *payload_len, &pos, &name, &name_len)
				    && name_len > 0 && name_len <= DSSH_ALGO_NAME_MAX
				    && dssh_buffer_has(*payload_len, pos, 1);
				for (size_t i = 0; valid && i < name_len; i++) {
					if (name[i] <= ' ' || name[i] >= DSSH_ASCII_DEL || name[i] == ',')
						valid = false;
				}
				if (!valid)
					return transport_disconnect_result(sess, SSH_DISCONNECT_PROTOCOL_ERROR,
					    "malformed GLOBAL_REQUEST", DSSH_ERROR_PARSE);
				/* Initial negotiation rejects higher-layer messages. Before
				 * the peer's rekey KEXINIT, the negotiation receiver queues
				 * in-flight higher-layer messages. Dispatch their callbacks
				 * and replies through this same path when replayed afterward.
				 * During the method exchange an unexpected message instead
				 * reaches the method's normal type validation. */
				if (finish_transmit_rekey) {
					res = queue_rekey_packet(sess, *msg_type, *payload, *payload_len);
					if (res < 0)
						return res;
					continue;
				}
				if (sess->trans.session_id == NULL || sess->trans.rekey_in_progress)
					return 0;
				bool want_reply = (*payload)[pos] != 0;
				pos++;
				int result = DSSH_ERROR_REJECTED;
				if (sess->global_request_cb != NULL)
					result = sess->global_request_cb(name, name_len, want_reply,
					    &(*payload)[pos], *payload_len - pos, sess->global_request_cbdata);
				if (sess->terminate)
					return DSSH_ERROR_TERMINATED;
				if (want_reply) {
					uint8_t reply = result >= 0 ? SSH_MSG_REQUEST_SUCCESS : SSH_MSG_REQUEST_FAILURE;
					res = send_to_slot(sess, &sess->trans.global_reply_slot, &reply, 1);
					if (res < 0) {
						session_set_terminate(sess);
						return res;
					}
				}
				continue;
			}
			default:
				if (finish_transmit_rekey) {
					res = queue_rekey_packet(sess, *msg_type, *payload, *payload_len);
					if (res < 0)
						return res;
					continue;
				}
				/*
                                 * Auto-rekey: if any threshold (packet count,
                                 * bytes, or time) has been exceeded, note it.
                                 * We cannot rekey HERE because *payload points
                                 * into rx_packet -- the rekey would overwrite it.
                                 * Instead, set the flag and rekey on the NEXT
                                 * recv_packet call, before reading new data.
                                 */
				if ((sess->trans.session_id != NULL) && !sess->trans.rekey_in_progress
				    && rekey_needed(sess))
					sess->trans.rekey_pending = true;
				if (sess->terminate)
					return DSSH_ERROR_TERMINATED;
				return 0;
		}
	}
}

DSSH_PRIVATE int
recv_packet(struct dssh_session_s *sess, uint8_t *msg_type, uint8_t **payload, size_t *payload_len)
{
	struct dssh_session_scope scope;

	session_scope_enter(&scope, sess);
	int ret = recv_packet_inner(sess, msg_type, payload, payload_len);

	session_scope_leave(&scope);
	return ret;
}

/* ================================================================
 * KEXINIT (RFC 4253 s7.1)
 * ================================================================ */

/*
 * Length-exact membership test: does `name` (length nlen) appear as
 * a comma-separated token in `filter`?  filter == NULL is treated
 * as "match everything" by callers; this helper returns false on
 * NULL filter so callers must short-circuit.
 */
DSSH_TESTABLE bool
name_in_filter(const char *name, size_t nlen, const char *filter)
{
	if (filter == NULL)
		return false;

	const char *fp = filter;

	while (*fp) {
		const char *fc   = strchr(fp, ',');
		size_t      flen = fc ? (size_t)(fc - fp) : strlen(fp);

		if ((flen == nlen) && (memcmp(fp, name, nlen) == 0))
			return true;
		fp += flen;
		if (fc)
			fp++;
		else
			break;
	}
	return false;
}

/* Read each link with its declared pointer type, then convert its value.
 * Generic name operations must not copy pointer representations between types. */
#define DEFINE_REGISTRY_NEXT(suffix, type) \
	DSSH_TESTABLE void *registry_next_##suffix(void *node) \
	{ \
		type entry = node; \
		return entry->next; \
	}
DEFINE_REGISTRY_NEXT(kex, dssh_kex)
DEFINE_REGISTRY_NEXT(key_algo, dssh_key_algo)
DEFINE_REGISTRY_NEXT(enc, dssh_enc)
DEFINE_REGISTRY_NEXT(mac, dssh_mac)
DEFINE_REGISTRY_NEXT(comp, dssh_comp)
#undef DEFINE_REGISTRY_NEXT

/*
 * Look up a node in `head` by exact-length name match.  Returns
 * the node pointer or NULL.  Used by build_namelist when iterating
 * the filter (filter order, not registration order).
 */
static void *
find_node_by_name(void *head, size_t name_offset, dssh_registry_next next,
    const char *name, size_t nlen)
{
	for (void *node = head; node != NULL; node = next(node)) {
		const unsigned char *entry = node;
		const char *nm = (const char *)(entry + name_offset);

		if ((strlen(nm) == nlen) && (memcmp(nm, name, nlen) == 0))
			return node;
	}
	return NULL;
}

static bool
next_name(struct dssh_name_list list, size_t *pos, struct dssh_name_list *name)
{
	if (*pos >= list.len)
		return false;
	size_t end = *pos;
	while (end < list.len && list.data[end] != ',')
		end++;
	name->data = &list.data[*pos];
	name->len = end - *pos;
	*pos = end < list.len ? end + 1 : end;
	return true;
}

static bool
list_has_name(struct dssh_name_list list, struct dssh_name_list wanted)
{
	size_t pos = 0;
	struct dssh_name_list name;
	while (next_name(list, &pos, &name)) {
		if (name.len == wanted.len && memcmp(name.data, wanted.data, name.len) == 0)
			return true;
	}
	return false;
}

static bool
list_has_text(struct dssh_name_list list, const char *text)
{
	const uint8_t *bytes = (const uint8_t *)text;
	struct dssh_name_list wanted = { bytes, strlen(text) };
	return list_has_name(list, wanted);
}

static int
append_registered_name(void *node, size_t name_offset, char *buf, size_t bufsz,
    size_t *pos, bool hostkeys)
{
	if (hostkeys) {
		dssh_key_algo key = node;
		if (key->haskey == NULL)
			return 0;
		int available = key->haskey(key->ctx);
		if (available < 0)
			return available;
		if (available == 0)
			return 0;
	}
	const unsigned char *entry = node;
	const char *name = (const char *)&entry[name_offset];
	size_t len = strlen(name);
	if (len == 0 || len > DSSH_ALGO_NAME_MAX)
		return DSSH_ERROR_INVALID;
	size_t comma = *pos > 0 ? 1 : 0;
	if (!dssh_buffer_has(bufsz, *pos, comma + len + 1))
		return DSSH_ERROR_TOOLONG;
	if (comma != 0)
		buf[(*pos)++] = ',';
	memcpy(&buf[*pos], name, len);
	*pos += len;
	buf[*pos] = 0;
	return 0;
}

static int64_t
build_namelist_inner(void *head, size_t name_offset, dssh_registry_next next,
    char *buf, size_t bufsz,
    const char *filter, bool hostkeys)
{
	if (bufsz == 0)
		return DSSH_ERROR_TOOLONG;
	buf[0] = 0;
	size_t pos = 0;
	int ret;
	if (filter == NULL) {
		for (void *node = head; node != NULL; node = next(node)) {
			ret = append_registered_name(node, name_offset, buf, bufsz, &pos, hostkeys);
			if (ret < 0)
				return ret;
		}
	}
	else {
		struct dssh_name_list names = { (const uint8_t *)filter, strlen(filter) };
		struct dssh_name_list name;
		size_t offset = 0;
		while (next_name(names, &offset, &name)) {
			const char *text = (const char *)name.data;
			void *node = find_node_by_name(head, name_offset, next, text, name.len);
			if (node == NULL)
				continue;
			ret = append_registered_name(node, name_offset, buf, bufsz, &pos, hostkeys);
			if (ret < 0)
				return ret;
		}
	}
#if SIZE_MAX > INT64_MAX
	if (pos > INT64_MAX)
		return DSSH_ERROR_TOOLONG;
#endif
	int64_t len = (int64_t)pos;
	return len;
}

DSSH_TESTABLE int64_t
build_namelist(void *head, size_t name_offset, dssh_registry_next next,
    char *buf, size_t bufsz, const char *filter)
{
	return build_namelist_inner(head, name_offset, next, buf, bufsz, filter, false);
}

DSSH_TESTABLE size_t
first_name(struct dssh_name_list list, char *buf, size_t bufsz)
{
	if (bufsz == 0)
		return 0;
	struct dssh_name_list name = { NULL, 0 };
	size_t pos = 0;
	next_name(list, &pos, &name);
	size_t len = name.len < bufsz ? name.len : bufsz - 1;
	if (len != 0)
		memcpy(buf, name.data, len);
	buf[len] = 0;
	return len;
}

DSSH_TESTABLE void *
negotiate_algo(struct dssh_name_list client, struct dssh_name_list server,
    void *head, size_t name_offset, dssh_registry_next next, const char *filter)
{
	size_t pos = 0;
	struct dssh_name_list name;
	while (next_name(client, &pos, &name)) {
		const char *text = (const char *)name.data;
		if (filter != NULL && !name_in_filter(text, name.len, filter))
			continue;
		void *node = find_node_by_name(head, name_offset, next, text, name.len);
		if (node != NULL && list_has_name(server, name))
			return node;
	}
	return NULL;
}

static int
append_registry_namelist(uint8_t *buf, size_t bufsz,
    size_t *pos, void *head, size_t name_offset, dssh_registry_next next,
    const char *filter, bool hostkeys)
{
	if (!dssh_buffer_has(bufsz, *pos, 4))
		return DSSH_ERROR_TOOLONG;
	size_t prefix = *pos;
	*pos += 4;
	char *names = (char *)&buf[*pos];
	int64_t length = hostkeys
	    ? build_namelist_inner(head, name_offset, next, names, bufsz - *pos, filter, true)
	    : build_namelist(head, name_offset, next, names, bufsz - *pos, filter);
	if (length < 0) {
		int ret = (int)length;
		return ret;
	}
	if (length == 0)
		return DSSH_ERROR_INIT;
	if (length > UINT32_MAX)
		return DSSH_ERROR_TOOLONG;
#if SIZE_MAX < INT64_MAX
	if (length > SIZE_MAX)
		return DSSH_ERROR_TOOLONG;
#endif
	uint32_t wire_length = (uint32_t)length;
	DSSH_PUT_U32(wire_length, buf, &prefix);
	size_t name_length = (size_t)length;
	*pos += name_length;
	return 0;
}

/* Capture both configured fields together in the retained local transcript. */
static int
append_language_fields(struct dssh_session_s *sess, uint8_t *buf, size_t capacity, size_t *pos)
{
	int lr = mtx_lock(&sess->mtx);
	if (lr != thrd_success) {
		session_set_terminate(sess);
		return DSSH_ERROR_TERMINATED;
	}
	size_t first = sess->local_languages[0].len;
	size_t second = sess->local_languages[1].len;
	/* Reserve both length prefixes and the KEXINIT Boolean/reserved tail. */
	if (!dssh_buffer_has(capacity, *pos, 13)
	    || first > capacity - *pos - 13
	    || second > capacity - *pos - 13 - first) {
		dssh_mtx_unlock(&sess->mtx);
		return DSSH_ERROR_TOOLONG;
	}
	for (size_t i = 0; i < 2; i++) {
		struct dssh_language_field *field = &sess->local_languages[i];
		/* The checked packet capacity is at most 64 MiB. */
		uint32_t length = (uint32_t)field->len;
		DSSH_PUT_U32(length, buf, pos);
		if (field->len != 0)
			memcpy(&buf[*pos], field->data, field->len);
		*pos += field->len;
	}
	dssh_mtx_unlock(&sess->mtx);
	return 0;
}

/* Both views belong to the validated retained peer transcript. Store one
 * reusable, bounded string pair for public queries after transcript release. */
static int
publish_peer_languages(struct dssh_session_s *sess, const struct dssh_name_list lists[10])
{
	size_t first = lists[8].len;
	size_t second = lists[9].len;
	if (first > SIZE_MAX - 2 || second > SIZE_MAX - 2 - first)
		return DSSH_ERROR_TOOLONG;
	size_t needed = first + second + 2;
	if (needed > sess->trans.packet_buf_sz)
		return DSSH_ERROR_TOOLONG;
	if (first == 0 && second == 0)
		needed = 0;
	char *candidate = NULL;
	char *old = NULL;
	int lr = mtx_lock(&sess->mtx);
	if (lr != thrd_success) {
		session_set_terminate(sess);
		return DSSH_ERROR_TERMINATED;
	}
	if (needed > sess->peer_languages_capacity) {
		dssh_mtx_unlock(&sess->mtx);
		candidate = malloc(needed);
		if (candidate == NULL)
			return DSSH_ERROR_ALLOC;
		lr = mtx_lock(&sess->mtx);
		if (lr != thrd_success) {
			free(candidate);
			session_set_terminate(sess);
			return DSSH_ERROR_TERMINATED;
		}
	}
	if (needed > sess->peer_languages_capacity) {
		old = sess->peer_languages;
		sess->peer_languages = candidate;
		sess->peer_languages_capacity = needed;
		candidate = NULL;
	}
	char *data = sess->peer_languages;
	if (data != NULL) {
		if (first != 0)
			memcpy(data, lists[8].data, first);
		data[first] = 0;
		if (second != 0)
			memcpy(&data[first + 1], lists[9].data, second);
		data[first + 1 + second] = 0;
	}
	sess->peer_languages_len[0] = first;
	sess->peer_languages_len[1] = second;
	sess->peer_languages_available = true;
	dssh_mtx_unlock(&sess->mtx);
	free(old);
	free(candidate);
	return 0;
}

/* Build the complete offer directly in its retained transcript buffer. */
static int
build_kexinit_packet(struct dssh_session_s *sess, uint8_t **buf_out, size_t *pos_out)
{
	size_t bufsz = tx_payload_capacity(sess);
	uint8_t *buf = malloc(bufsz);
	if (buf == NULL)
		return DSSH_ERROR_ALLOC;
	size_t pos = 0;
	int ret;
	if (!dssh_buffer_has(bufsz, 0, 1 + DSSH_KEXINIT_COOKIE_SIZE)) {
		free(buf);
		return DSSH_ERROR_TOOLONG;
	}
	buf[pos++] = SSH_MSG_KEXINIT;
	ret = dssh_random(&buf[pos], DSSH_KEXINIT_COOKIE_SIZE);
	if (ret < 0) {
		free(buf);
		return ret;
	}
	pos += DSSH_KEXINIT_COOKIE_SIZE;
#define OFFER(head, type, suffix, filter, hostkeys) \
	do { \
		ret = append_registry_namelist(buf, bufsz, &pos, (head), offsetof(type, name), registry_next_##suffix, (filter), (hostkeys)); \
		if (ret < 0) { \
			free(buf); \
			return ret; \
		} \
	} while (0)
	OFFER(gconf.kex_head, struct dssh_kex_s, kex, sess->kex_filter, false);
	if (sess->trans.session_id == NULL) {
		const char *indicator = sess->trans.client ? "ext-info-c" : "ext-info-s";
		size_t length = strlen(indicator);
		if (!dssh_buffer_has(bufsz, pos, 1 + length)) {
			free(buf);
			return DSSH_ERROR_TOOLONG;
		}
		buf[pos++] = ',';
		memcpy(&buf[pos], indicator, length);
		pos += length;
		/* The KEX list immediately follows the fixed cookie/header. Its
		 * nonempty registered prefix was already checked by OFFER. */
		size_t prefix = 1 + DSSH_KEXINIT_COOKIE_SIZE;
		size_t list_length = pos - prefix - 4;
		if (list_length > UINT32_MAX) {
			free(buf);
			return DSSH_ERROR_TOOLONG;
		}
		uint32_t wire_length = (uint32_t)list_length;
		DSSH_PUT_U32(wire_length, buf, &prefix);
	}
	OFFER(gconf.key_algo_head, struct dssh_key_algo_s, key_algo, sess->key_algo_filter, !sess->trans.client);
	OFFER(gconf.enc_head, struct dssh_enc_s, enc, sess->enc_filter, false);
	OFFER(gconf.enc_head, struct dssh_enc_s, enc, sess->enc_filter, false);
	OFFER(gconf.mac_head, struct dssh_mac_s, mac, sess->mac_filter, false);
	OFFER(gconf.mac_head, struct dssh_mac_s, mac, sess->mac_filter, false);
	OFFER(gconf.comp_head, struct dssh_comp_s, comp, sess->comp_filter, false);
	OFFER(gconf.comp_head, struct dssh_comp_s, comp, sess->comp_filter, false);
#undef OFFER
	ret = append_language_fields(sess, buf, bufsz, &pos);
	if (ret < 0) {
		free(buf);
		return ret;
	}
	buf[pos++] = 0;
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
		int res = recv_packet(sess, &msg_type, &peer_payload, &peer_len);
		if (res < 0)
			return res;
		if (msg_type == SSH_MSG_KEXINIT)
			break;
		/* Only an established session can have in-flight messages from
		 * before a rekey. Initial KEX must not populate the replay queue. */
		if (sess->trans.session_id == NULL)
			return DSSH_ERROR_PARSE;
		res = queue_rekey_packet(sess, msg_type, peer_payload, peer_len);
		if (res < 0)
			return res;
	}

	return retain_peer_kexinit(sess, peer_payload, peer_len);
}

/* Views remain valid while their saved KEXINIT buffer is retained. */
DSSH_TESTABLE int
parse_peer_kexinit(const uint8_t *buf, size_t bufsz,
    struct dssh_name_list lists[DSSH_KEXINIT_NAMELIST_COUNT], bool *first_kex_follows)
{
	if (bufsz < 1 + DSSH_KEXINIT_COOKIE_SIZE || buf[0] != SSH_MSG_KEXINIT)
		return DSSH_ERROR_PARSE;
	size_t pos = 1 + DSSH_KEXINIT_COOKIE_SIZE;
	for (size_t i = 0; i < DSSH_KEXINIT_NAMELIST_COUNT; i++) {
		if (!dssh_buffer_has(bufsz, pos, 4))
			return DSSH_ERROR_PARSE;
		uint32_t wire_len = DSSH_GET_U32(&buf[pos]);
		pos += 4;
		if (wire_len > bufsz - pos)
			return DSSH_ERROR_PARSE;
		size_t len = (size_t)wire_len;
		if (len == 0 && i < 8)
			return DSSH_ERROR_PARSE;
		size_t name_len = 0;
		for (size_t j = 0; j < len; j++) {
			uint8_t ch = buf[pos + j];
			if (ch == ',') {
				if (name_len == 0)
					return DSSH_ERROR_PARSE;
				name_len = 0;
			}
			else {
				if (ch <= ' ' || ch >= DSSH_ASCII_DEL)
					return DSSH_ERROR_PARSE;
				name_len++;
				if (i < 8 && name_len > DSSH_ALGO_NAME_MAX)
					return DSSH_ERROR_PARSE;
			}
		}
		if (len != 0 && name_len == 0)
			return DSSH_ERROR_PARSE;
		lists[i].data = &buf[pos];
		lists[i].len = len;
		pos += len;
	}
	if (bufsz - pos != 5)
		return DSSH_ERROR_PARSE;
	*first_kex_follows = buf[pos] != 0;
	/* The reserved uint32 is ignored, as required by RFC 4253. */
	return 0;
}

static dssh_key_algo
negotiate_key(struct dssh_name_list client, struct dssh_name_list server,
    uint32_t required, const char *filter)
{
	size_t pos = 0;
	struct dssh_name_list name;
	while (next_name(client, &pos, &name)) {
		const char *text = (const char *)name.data;
		if (filter != NULL && !name_in_filter(text, name.len, filter))
			continue;
		dssh_key_algo key = find_node_by_name(gconf.key_algo_head,
		    offsetof(struct dssh_key_algo_s, name), registry_next_key_algo, text, name.len);
		if (key != NULL && (key->flags & required) == required && list_has_name(server, name))
			return key;
	}
	return NULL;
}

static int
negotiate_algorithms(struct dssh_session_s *sess,
    const struct dssh_name_list peer[DSSH_KEXINIT_NAMELIST_COUNT], bool peer_first_kex_follows)
{
	struct dssh_name_list ours[DSSH_KEXINIT_NAMELIST_COUNT];
	bool unused;
	int ret = parse_peer_kexinit(sess->trans.our_kexinit, sess->trans.our_kexinit_sz, ours, &unused);
	if (ret < 0)
		return ret;
	const struct dssh_name_list *client = sess->trans.client ? ours : peer;
	const struct dssh_name_list *server = sess->trans.client ? peer : ours;
	if (list_has_text(peer[0], sess->trans.client ? "ext-info-c" : "ext-info-s"))
		return DSSH_ERROR_PARSE;
	if (sess->trans.session_id == NULL) {
		sess->trans.ext_info_receive = list_has_text(ours[0],
		    sess->trans.client ? "ext-info-c" : "ext-info-s");
		sess->trans.ext_info_peer = list_has_text(peer[0],
		    sess->trans.client ? "ext-info-s" : "ext-info-c");
	}
	sess->trans.kex_selected = NULL;
	sess->trans.key_algo_selected = NULL;
	struct dssh_name_list kex_client = client[0];
	struct dssh_name_list client_guess, server_guess;
	size_t client_pos = 0, server_pos = 0;
	if (!next_name(client[0], &client_pos, &client_guess)
	    || !next_name(server[0], &server_pos, &server_guess))
		return DSSH_ERROR_PARSE;
	if (client_guess.len == server_guess.len
	    && memcmp(client_guess.data, server_guess.data, client_guess.len) == 0)
		kex_client.len = client_guess.len;
	size_t pos = 0;
	struct dssh_name_list name;
	while (next_name(kex_client, &pos, &name)) {
		const char *text = (const char *)name.data;
		if ((name.len == DSSH_STRLEN("ext-info-c") && memcmp(text, "ext-info-c", name.len) == 0)
		    || (name.len == DSSH_STRLEN("ext-info-s") && memcmp(text, "ext-info-s", name.len) == 0)) {
			if (list_has_name(server[0], name))
				return DSSH_ERROR_PARSE;
			continue;
		}
		if (sess->kex_filter != NULL && !name_in_filter(text, name.len, sess->kex_filter))
			continue;
		dssh_kex kex = find_node_by_name(gconf.kex_head, offsetof(struct dssh_kex_s, name), registry_next_kex, text, name.len);
		if (kex == NULL || !list_has_name(server[0], name))
			continue;
		uint32_t required = 0;
		if ((kex->flags & DSSH_KEX_FLAG_NEEDS_ENCRYPTION_CAPABLE) != 0)
			required |= DSSH_KEY_ALGO_FLAG_ENCRYPTION_CAPABLE;
		if ((kex->flags & DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE) != 0)
			required |= DSSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE;
		dssh_key_algo key = negotiate_key(client[1], server[1], required, sess->key_algo_filter);
		if (key != NULL) {
			sess->trans.kex_selected = kex;
			sess->trans.key_algo_selected = key;
			break;
		}
	}
#define SELECT(field, index, head, type, suffix, filter) \
	sess->trans.field = negotiate_algo(client[index], server[index], (head), offsetof(type, name), registry_next_##suffix, (filter))
	SELECT(enc_c2s_selected, 2, gconf.enc_head, struct dssh_enc_s, enc, sess->enc_filter);
	SELECT(enc_s2c_selected, 3, gconf.enc_head, struct dssh_enc_s, enc, sess->enc_filter);
	SELECT(mac_c2s_selected, 4, gconf.mac_head, struct dssh_mac_s, mac, sess->mac_filter);
	SELECT(mac_s2c_selected, 5, gconf.mac_head, struct dssh_mac_s, mac, sess->mac_filter);
	SELECT(comp_c2s_selected, 6, gconf.comp_head, struct dssh_comp_s, comp, sess->comp_filter);
	SELECT(comp_s2c_selected, 7, gconf.comp_head, struct dssh_comp_s, comp, sess->comp_filter);
#undef SELECT
	if (sess->trans.kex_selected == NULL || sess->trans.key_algo_selected == NULL
	    || sess->trans.enc_c2s_selected == NULL || sess->trans.enc_s2c_selected == NULL
	    || sess->trans.mac_c2s_selected == NULL || sess->trans.mac_s2c_selected == NULL
	    || sess->trans.comp_c2s_selected == NULL || sess->trans.comp_s2c_selected == NULL) {
		ret = transport_disconnect_inner(sess, SSH_DISCONNECT_KEY_EXCHANGE_FAILED, "no common algorithm");
		return ret < 0 ? ret : DSSH_ERROR_INVALID;
	}
	if (peer_first_kex_follows) {
		char kex_guess[DSSH_ALGO_NAME_MAX + 1];
		char key_guess[DSSH_ALGO_NAME_MAX + 1];
		first_name(peer[0], kex_guess, sizeof(kex_guess));
		first_name(peer[1], key_guess, sizeof(key_guess));
		if (strcmp(sess->trans.kex_selected->name, kex_guess) != 0
		    || strcmp(sess->trans.key_algo_selected->name, key_guess) != 0) {
			uint8_t type;
			uint8_t *payload;
			size_t len;
			ret = recv_packet_raw(sess, &type, &payload, &len);
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

static int
prepare_kexinit(struct dssh_session_s *sess)
{
	uint8_t *kexinit;
	size_t   kexinit_sz;
	int      res = build_kexinit_packet(sess, &kexinit, &kexinit_sz);

	if (res < 0)
		return res;

	/* Save our KEXINIT for exchange hash -- transfer ownership */
	free(sess->trans.our_kexinit);
	sess->trans.our_kexinit    = kexinit;
	sess->trans.our_kexinit_sz = kexinit_sz;

	return 0;
}

static int
finish_kexinit(struct dssh_session_s *sess)
{
	int res = receive_peer_kexinit(sess);
	if (res < 0)
		return res;

	struct dssh_name_list peer_lists[DSSH_KEXINIT_NAMELIST_COUNT];
	bool peer_first_kex_follows;

	res = parse_peer_kexinit(sess->trans.peer_kexinit, sess->trans.peer_kexinit_sz, peer_lists,
	    &peer_first_kex_follows);
	if (res < 0)
		return res;

	res = publish_peer_languages(sess, peer_lists);
	if (res < 0)
		return res;

	return negotiate_algorithms(sess, peer_lists, peer_first_kex_follows);
}

DSSH_TESTABLE int
kexinit(struct dssh_session_s *sess)
{
	int res = prepare_kexinit(sess);
	if (res < 0)
		return res;
	res = send_packet(sess, sess->trans.our_kexinit, sess->trans.our_kexinit_sz, NULL);
	return res < 0 ? res : finish_kexinit(sess);
}

/* ================================================================
 * Key exchange (RFC 4253 s7)
 * ================================================================ */

static int
kex_send_wrapper(const uint8_t *payload, size_t len, void *io_ctx)
{
	if (io_ctx == NULL || payload == NULL || len == 0)
		return DSSH_ERROR_INVALID;
	struct dssh_session_s *sess = io_ctx;
	return send_packet(sess, payload, len, NULL);
}

static int
kex_recv_wrapper(uint8_t *msg_type, uint8_t **payload, size_t *payload_len, void *io_ctx)
{
	if (io_ctx == NULL || msg_type == NULL || payload == NULL || payload_len == NULL)
		return DSSH_ERROR_INVALID;
	struct dssh_session_s *sess = io_ctx;
	return recv_packet(sess, msg_type, payload, payload_len);
}

static void
kex_release_result(struct dssh_session_s *sess)
{
	/* Keep the lease record at the address given to its producer. Clear views
	 * before release so a callback cannot observe stale consumer metadata. */
	sess->trans.kex_result.shared_secret = (struct dssh_bytes){0};
	sess->trans.kex_result.exchange_hash = (struct dssh_bytes){0};
	dssh_result_release(&sess->trans.kex_result.lease);
}

static void
kex_cleanup_state(struct dssh_session_s *sess)
{
	kex_release_result(sess);
	dssh_kex creator = sess->trans.kex_active;
	void *state = sess->trans.kex_state;
	sess->trans.kex_active = NULL;
	sess->trans.kex_state = NULL;
	if (creator != NULL) {
		struct dssh_session_s *previous = io_callback_session;
		io_callback_session = sess;
		creator->cleanup(state);
		io_callback_session = previous;
	}
}

DSSH_TESTABLE int
kex(struct dssh_session_s *sess)
{
	kex_release_result(sess);
	dssh_kex selected = sess->trans.kex_selected;
	if (selected == NULL || selected->init == NULL || selected->handler == NULL
	    || selected->cleanup == NULL) {
		kex_cleanup_state(sess);
		return DSSH_ERROR_INIT;
	}
	if (sess->trans.kex_active != selected)
		kex_cleanup_state(sess);
	bool initialized = sess->trans.kex_active == selected;
	struct dssh_session_s *previous = io_callback_session;
	io_callback_session = sess;
	int res = selected->init(sess->trans.client, selected->config, &sess->trans.kex_state);
	io_callback_session = previous;
	/* A NULL success still has an initialized creator. A failed fresh
	 * candidate owns nothing unless it published cleanup-valid state. */
	if (res == 0 || initialized || sess->trans.kex_state != NULL)
		sess->trans.kex_active = selected;
	if (res > 0)
		res = DSSH_ERROR_INVALID;
	if (res < 0) {
		kex_cleanup_state(sess);
		return res;
	}
	if (atomic_load_explicit(&sess->terminate, memory_order_acquire)) {
		kex_cleanup_state(sess);
		return DSSH_ERROR_TERMINATED;
	}
	const struct dssh_kex_context kctx = {
	    .client   = sess->trans.client,
	    .v_c      = sess->trans.client ? sess->trans.id_str : sess->trans.remote_id_str,
	    .v_c_len  = sess->trans.client ? sess->trans.id_str_sz : sess->trans.remote_id_str_sz,
	    .v_s      = sess->trans.client ? sess->trans.remote_id_str : sess->trans.id_str,
	    .v_s_len  = sess->trans.client ? sess->trans.remote_id_str_sz : sess->trans.id_str_sz,
	    .i_c      = sess->trans.client ? sess->trans.our_kexinit : sess->trans.peer_kexinit,
	    .i_c_len  = sess->trans.client ? sess->trans.our_kexinit_sz : sess->trans.peer_kexinit_sz,
	    .i_s      = sess->trans.client ? sess->trans.peer_kexinit : sess->trans.our_kexinit,
	    .i_s_len  = sess->trans.client ? sess->trans.peer_kexinit_sz : sess->trans.our_kexinit_sz,
	    .key_algo = sess->trans.key_algo_selected,
	    .config   = selected->config,
	    .state    = sess->trans.kex_state,
	    .send     = kex_send_wrapper,
	    .recv     = kex_recv_wrapper,
	    .io_ctx   = sess,
	    .hostkey_verify        = sess->hostkey_verify_cb,
	    .hostkey_verify_cbdata = sess->hostkey_verify_cbdata,
	};
	struct dssh_kex_result *out = &sess->trans.kex_result;
	res = selected->handler(&kctx, out);
	if (res > 0)
		res = DSSH_ERROR_INVALID;
	if (res == 0 && (out->lease.release == NULL || out->shared_secret.data == NULL
	    || out->shared_secret.len == 0 || out->shared_secret.len > UINT32_MAX
	    || out->exchange_hash.data == NULL || out->exchange_hash.len == 0
	    || out->exchange_hash.len > UINT32_MAX))
		res = DSSH_ERROR_INVALID;
	if (res == 0) {
		dssh_hash_ctx *hash_ctx = NULL;
		size_t digest_len;
		res = dssh_hash_init(&hash_ctx, selected->hash_name, &digest_len);
		if (res == 0 && out->exchange_hash.len != digest_len)
			res = DSSH_ERROR_INVALID;
		dssh_hash_free(hash_ctx);
	}
	if (res == 0 && atomic_load_explicit(&sess->terminate, memory_order_acquire))
		res = DSSH_ERROR_TERMINATED;
	if (res < 0)
		kex_cleanup_state(sess);
	return res;
}

/* Method rejection is a wire disconnect in both the initial exchange and
 * rekey. kex() has already retired the result and method state. A transport
 * failure, peer disconnect or application cancellation may have terminated
 * the session first; do not attempt further output in that case. */
static int
kex_exchange(struct dssh_session_s *sess)
{
	int res = kex(sess);
	if (res < 0 && !atomic_load_explicit(&sess->terminate, memory_order_acquire))
		return transport_disconnect_result(sess, SSH_DISCONNECT_KEY_EXCHANGE_FAILED,
		    "key exchange failed", res);
	return res;
}

/* ================================================================
 * Key derivation and NEWKEYS (RFC 4253 s7.2, s7.3)
 * ================================================================ */

DSSH_TESTABLE int
derive_key(const char *hash_name, const uint8_t *shared_secret, size_t shared_secret_sz, const uint8_t *hash,
    size_t hash_sz, uint8_t letter, const uint8_t *session_id, size_t session_id_sz, uint8_t *out, size_t needed)
{
	/* Stateless algorithms request no output. Do not allocate/hash or require
	 * an output address for an empty derived key. */
	if (needed == 0)
		return 0;
	dssh_hash_ctx *ctx = NULL;
	size_t         md_len;
	int            res = dssh_hash_init(&ctx, hash_name, &md_len);

	if (res < 0)
		return res;

	uint8_t *tmp = malloc(md_len);

	if (tmp == NULL) {
		dssh_hash_free(ctx);
		return DSSH_ERROR_ALLOC;
	}

	size_t have = 0;

#define DK(call)                      \
	do {                          \
		res = (call);         \
		if (res < 0)          \
			goto cleanup; \
	} while (0)

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
		DK(dssh_hash_update(ctx, shared_secret, shared_secret_sz));
		DK(dssh_hash_update(ctx, hash, hash_sz));
		DK(dssh_hash_update(ctx, out, have));
		DK(dssh_hash_final(ctx, tmp, md_len));
		copy = md_len < (needed - have) ? md_len : (needed - have);
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
 * Hybrid methods opt into opaque string encoding through their registration.
 *
 * On success, *out is a malloc'd buffer and *out_sz is its size.
 */
DSSH_TESTABLE int
encode_k_wire(const uint8_t *raw, size_t raw_sz, bool k_as_string, uint8_t **out, size_t *out_sz)
{
	/* kex() enforces the same raw result bound. Check before inspecting its
	 * bytes; only integer K may discard redundant leading zeroes. */
#if SIZE_MAX > UINT32_MAX
	if (raw_sz > UINT32_MAX)
		return DSSH_ERROR_INVALID;
#endif
	if (!k_as_string) {
		while (raw_sz > 0 && raw[0] == 0) {
			raw++;
			raw_sz--;
		}
	}
	bool need_pad = !k_as_string && raw_sz > 0 && (raw[0] & DSSH_MPINT_SIGN_BIT) != 0;
	size_t padding = need_pad ? 1 : 0;
	if (raw_sz > SIZE_MAX - 4 - padding)
		return DSSH_ERROR_INVALID;
	size_t k_data_len = raw_sz + padding;
#if SIZE_MAX > UINT32_MAX
	if (k_data_len > UINT32_MAX)
		return DSSH_ERROR_INVALID;
#endif
	uint32_t wire_data_len = (uint32_t)k_data_len;

	size_t   k_wire_len = 4 + k_data_len;
	uint8_t *buf        = malloc(k_wire_len);

	if (buf == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;

	DSSH_PUT_U32(wire_data_len, buf, &pos);
	if (need_pad)
		buf[pos++] = 0; /* mpint sign-bit padding */
	if (raw_sz > 0)
		memcpy(&buf[pos], raw, raw_sz);

	*out    = buf;
	*out_sz = k_wire_len;
	return 0;
}

/*
 * Derive 6 session keys, apply them (init new cipher/MAC contexts,
 * clean up old ones), allocate MAC verification buffers, reset
 * counters, and free KEXINIT buffers.  Key material is local and
 * cleansed on exit.
 */
/* Determine a safe input size once per epoch. The module's nondecreasing bound
 * includes worst-case expansion and remains valid for all future stream states. */
static int
compression_input_limit(struct dssh_session_s *sess, dssh_comp comp, dssh_comp_ctx *ctx,
    size_t *limit)
{
	size_t low = 0;
	size_t high = tx_wire_payload_capacity(sess);
	size_t capacity = high;
	while (low < high) {
		size_t middle = low + (high - low) / 2 + (high - low) % 2;
		size_t bound;
		int ret = call_comp_bound(sess, comp, middle, &bound, ctx);
		if (ret < 0 && ret != DSSH_ERROR_TOOLONG)
			return ret;
		if (ret == 0 && bound < middle)
			return DSSH_ERROR_INVALID;
		if (ret == 0 && bound <= capacity)
			low = middle;
		else
			high = middle - 1;
	}
	if (low == 0)
		return DSSH_ERROR_TOOLONG;
	*limit = low;
	return 0;
}

static int
apply_compression(struct dssh_session_s *sess)
{
	dssh_comp c2s = sess->trans.comp_c2s_selected;
	dssh_comp s2c = sess->trans.comp_s2c_selected;
	if (c2s == NULL || s2c == NULL)
		return DSSH_ERROR_INIT;
	dssh_comp tx = sess->trans.client ? c2s : s2c;
	dssh_comp rx = sess->trans.client ? s2c : c2s;
	dssh_comp_ctx *c2s_ctx = NULL;
	dssh_comp_ctx *s2c_ctx = NULL;
	bool c2s_initialized = false;
	bool s2c_initialized = false;
	size_t input_limit = tx_wire_payload_capacity(sess);
	int ret = DSSH_ERROR_ALLOC;
	if (compression_enabled(tx) && sess->trans.comp_tx_packet == NULL) {
		sess->trans.comp_tx_packet = malloc(4 + sess->trans.packet_buf_sz);
		if (sess->trans.comp_tx_packet == NULL)
			goto done;
	}
	if (compression_enabled(rx) && sess->trans.comp_rx_input == NULL) {
		sess->trans.comp_rx_input = malloc(sess->trans.packet_buf_sz);
		if (sess->trans.comp_rx_input == NULL)
			goto done;
	}
	/* Transfer existing same-module contexts to the reset callback. The module
	 * may reuse or replace the allocation, so its old pointer must not remain
	 * separately owned by failure/teardown cleanup. */
	if (sess->trans.comp_c2s_active == c2s) {
		c2s_ctx = sess->trans.comp_c2s_ctx;
		sess->trans.comp_c2s_ctx = NULL;
		sess->trans.comp_c2s_active = NULL;
	}
	ret = call_comp_init(sess, c2s, sess->trans.client, &c2s_ctx);
	if (ret < 0)
		goto done;
	c2s_initialized = true;
	if (sess->trans.comp_s2c_active == s2c) {
		s2c_ctx = sess->trans.comp_s2c_ctx;
		sess->trans.comp_s2c_ctx = NULL;
		sess->trans.comp_s2c_active = NULL;
	}
	ret = call_comp_init(sess, s2c, !sess->trans.client, &s2c_ctx);
	if (ret < 0)
		goto done;
	s2c_initialized = true;
	if (compression_enabled(tx)) {
		dssh_comp_ctx *tx_ctx = sess->trans.client ? c2s_ctx : s2c_ctx;
		ret = compression_input_limit(sess, tx, tx_ctx, &input_limit);
		if (ret < 0)
			goto done;
	}
	if (sess->terminate) {
		ret = DSSH_ERROR_TERMINATED;
		goto done;
	}
	if (sess->trans.comp_c2s_active != NULL)
		call_comp_cleanup(sess, sess->trans.comp_c2s_active, sess->trans.comp_c2s_ctx);
	if (sess->trans.comp_s2c_active != NULL)
		call_comp_cleanup(sess, sess->trans.comp_s2c_active, sess->trans.comp_s2c_ctx);
	sess->trans.comp_c2s_active = c2s;
	sess->trans.comp_c2s_ctx = c2s_ctx;
	sess->trans.comp_s2c_active = s2c;
	sess->trans.comp_s2c_ctx = s2c_ctx;
	sess->trans.comp_tx_payload_limit = input_limit;
	return sess->terminate ? DSSH_ERROR_TERMINATED : 0;

done:
	if (c2s_initialized || c2s_ctx != NULL)
		call_comp_cleanup(sess, c2s, c2s_ctx);
	if (s2c_initialized || s2c_ctx != NULL)
		call_comp_cleanup(sess, s2c, s2c_ctx);
	return ret;
}

/* Keep the verification pair across key changes. Grow both before replacing
 * either, so an allocation failure leaves their old ownership intact. */
DSSH_TESTABLE int
reserve_mac_buffers(struct dssh_session_s *sess, uint16_t needed)
{
	if (needed > sess->trans.rx_mac_capacity) {
		uint8_t *received = malloc(needed);
		if (received == NULL)
			return DSSH_ERROR_ALLOC;
		uint8_t *computed = malloc(needed);
		if (computed == NULL) {
			cleanse_free(received, needed);
			return DSSH_ERROR_ALLOC;
		}
		cleanse_free(sess->trans.rx_mac_buf, sess->trans.rx_mac_capacity);
		cleanse_free(sess->trans.rx_mac_computed, sess->trans.rx_mac_capacity);
		sess->trans.rx_mac_buf = received;
		sess->trans.rx_mac_computed = computed;
		sess->trans.rx_mac_capacity = needed;
	}
	if (sess->trans.rx_mac_capacity > 0) {
		dssh_cleanse(sess->trans.rx_mac_buf, sess->trans.rx_mac_capacity);
		dssh_cleanse(sess->trans.rx_mac_computed, sess->trans.rx_mac_capacity);
	}
	return 0;
}

/* An active stateless module may have a NULL context. A failed initializer
 * owns cleanup work only when it actually returned a partial context. */
static int
initialize_enc(dssh_enc module, const uint8_t *key, const uint8_t *iv, bool encrypt,
    dssh_enc_ctx **ctx, dssh_enc *active)
{
	int ret = 0;
	if (module->init != NULL) {
		ret = module->init(key, iv, encrypt, ctx);
		if (ret > 0)
			ret = DSSH_ERROR_INVALID;
	}
	if (ret == 0 || *ctx != NULL)
		*active = module;
	return ret;
}

static int
initialize_mac(dssh_mac module, const uint8_t *key, dssh_mac_ctx **ctx, dssh_mac *active)
{
	int ret = 0;
	if (module->init != NULL) {
		ret = module->init(key, ctx);
		if (ret > 0)
			ret = DSSH_ERROR_INVALID;
	}
	if (ret == 0 || *ctx != NULL)
		*active = module;
	return ret;
}

DSSH_TESTABLE int
derive_and_apply_keys(struct dssh_session_s *sess, const uint8_t *k_mpint, size_t k_mpint_sz, dssh_enc old_enc_c2s,
    dssh_enc old_enc_s2c, dssh_mac old_mac_c2s, dssh_mac old_mac_s2c)
{
	struct timespec key_time;
	int clock_ret = dssh_clock_read(&key_time);
	if (clock_ret < 0) {
		kex_release_result(sess);
		return clock_ret;
	}

	const char *hash_name   = sess->trans.kex_selected->hash_name;
	uint16_t    enc_key_c2s = sess->trans.enc_c2s_selected->key_size;
	uint16_t    enc_key_s2c = sess->trans.enc_s2c_selected->key_size;
	uint16_t    enc_bs_c2s  = sess->trans.enc_c2s_selected->blocksize;
	uint16_t    enc_bs_s2c  = sess->trans.enc_s2c_selected->blocksize;
	uint16_t    mac_key_c2s = sess->trans.mac_c2s_selected->key_size;
	uint16_t    mac_key_s2c = sess->trans.mac_s2c_selected->key_size;
	uint8_t    *iv_c2s      = NULL;
	uint8_t    *iv_s2c      = NULL;
	uint8_t    *key_c2s     = NULL;
	uint8_t    *key_s2c     = NULL;
	uint8_t    *integ_c2s   = NULL;
	uint8_t    *integ_s2c   = NULL;
	int         res;

	iv_c2s    = malloc(enc_bs_c2s > 0 ? enc_bs_c2s : 1);
	iv_s2c    = malloc(enc_bs_s2c > 0 ? enc_bs_s2c : 1);
	key_c2s   = malloc(enc_key_c2s > 0 ? enc_key_c2s : 1);
	key_s2c   = malloc(enc_key_s2c > 0 ? enc_key_s2c : 1);
	integ_c2s = calloc(1, mac_key_c2s > 0 ? mac_key_c2s : 1);
	integ_s2c = calloc(1, mac_key_s2c > 0 ? mac_key_s2c : 1);

	if (!iv_c2s || !iv_s2c || !key_c2s || !key_s2c || !integ_c2s || !integ_s2c) {
		res = DSSH_ERROR_ALLOC;
		goto keys_cleanup;
	}

	/* Derive keys A-F (RFC 4253 s7.2) */
	const uint8_t *H      = sess->trans.kex_result.exchange_hash.data;
	size_t         H_sz   = sess->trans.kex_result.exchange_hash.len;
	const uint8_t *sid    = sess->trans.session_id;
	size_t         sid_sz = sess->trans.session_id_sz;

	res = derive_key(hash_name, k_mpint, k_mpint_sz, H, H_sz, 'A', sid, sid_sz, iv_c2s, enc_bs_c2s);
	if (res == 0)
		res = derive_key(hash_name, k_mpint, k_mpint_sz, H, H_sz, 'B', sid, sid_sz, iv_s2c, enc_bs_s2c);
	if (res == 0)
		res = derive_key(hash_name, k_mpint, k_mpint_sz, H, H_sz, 'C', sid, sid_sz, key_c2s, enc_key_c2s);
	if (res == 0)
		res = derive_key(hash_name, k_mpint, k_mpint_sz, H, H_sz, 'D', sid, sid_sz, key_s2c, enc_key_s2c);
	if ((res == 0) && (mac_key_c2s > 0))
		res =
		    derive_key(hash_name, k_mpint, k_mpint_sz, H, H_sz, 'E', sid, sid_sz, integ_c2s, mac_key_c2s);
	if ((res == 0) && (mac_key_s2c > 0))
		res =
		    derive_key(hash_name, k_mpint, k_mpint_sz, H, H_sz, 'F', sid, sid_sz, integ_s2c, mac_key_s2c);

	kex_release_result(sess);
	if (res < 0)
		goto keys_cleanup;

	/* Release each old active module, including its stateless NULL context. */
	if (old_enc_c2s != NULL) {
		if (old_enc_c2s->cleanup != NULL)
			old_enc_c2s->cleanup(sess->trans.enc_c2s_ctx);
		sess->trans.enc_c2s_ctx = NULL;
	}
	if (old_enc_s2c != NULL) {
		if (old_enc_s2c->cleanup != NULL)
			old_enc_s2c->cleanup(sess->trans.enc_s2c_ctx);
		sess->trans.enc_s2c_ctx = NULL;
	}
	if (old_mac_c2s != NULL) {
		if (old_mac_c2s->cleanup != NULL)
			old_mac_c2s->cleanup(sess->trans.mac_c2s_ctx);
		sess->trans.mac_c2s_ctx = NULL;
	}
	if (old_mac_s2c != NULL) {
		if (old_mac_s2c->cleanup != NULL)
			old_mac_s2c->cleanup(sess->trans.mac_s2c_ctx);
		sess->trans.mac_s2c_ctx = NULL;
	}

	/* Do not publish ownership for a direction whose initializer has not run. */
	sess->trans.enc_c2s_active = NULL;
	sess->trans.enc_s2c_active = NULL;
	sess->trans.mac_c2s_active = NULL;
	sess->trans.mac_s2c_active = NULL;

	/* Initialize new encryption contexts */
	dssh_enc enc_c2s = sess->trans.enc_c2s_selected;
	dssh_enc enc_s2c = sess->trans.enc_s2c_selected;

	res = initialize_enc(enc_c2s, key_c2s, iv_c2s, sess->trans.client,
	    &sess->trans.enc_c2s_ctx, &sess->trans.enc_c2s_active);
	if (res < 0)
		goto keys_cleanup;
	res = initialize_enc(enc_s2c, key_s2c, iv_s2c, !sess->trans.client,
	    &sess->trans.enc_s2c_ctx, &sess->trans.enc_s2c_active);
	if (res < 0)
		goto keys_cleanup;

	/* Initialize MAC contexts */
	res = initialize_mac(sess->trans.mac_c2s_selected, integ_c2s,
	    &sess->trans.mac_c2s_ctx, &sess->trans.mac_c2s_active);
	if (res < 0)
		goto keys_cleanup;
	res = initialize_mac(sess->trans.mac_s2c_selected, integ_s2c,
	    &sess->trans.mac_s2c_ctx, &sess->trans.mac_s2c_active);
	if (res < 0)
		goto keys_cleanup;

	/* Allocate MAC verification buffers.
	 * rx direction is s2c for clients, c2s for servers. */
	uint16_t mac_digest_rx = sess->trans.client ? sess->trans.mac_s2c_selected->digest_size
	                                            : sess->trans.mac_c2s_selected->digest_size;

	res = reserve_mac_buffers(sess, mac_digest_rx);
	if (res < 0)
		goto keys_cleanup;

	/* Reset per-key counters */
	atomic_store(&sess->trans.tx_since_rekey, 0);
	atomic_store(&sess->trans.tx_bytes_since_rekey, (uint64_t)0);
	sess->trans.rx_since_rekey       = 0;
	sess->trans.rx_bytes_since_rekey = 0;
	sess->trans.rekey_time           = key_time.tv_sec;

	free(sess->trans.our_kexinit);
	sess->trans.our_kexinit    = NULL;
	sess->trans.our_kexinit_sz = 0;
	free(sess->trans.peer_kexinit);
	sess->trans.peer_kexinit    = NULL;
	sess->trans.peer_kexinit_sz = 0;

keys_cleanup:
	kex_release_result(sess);
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
	dssh_enc old_enc_c2s = enc_c2s_module(sess);
	dssh_enc old_enc_s2c = enc_s2c_module(sess);
	dssh_mac old_mac_c2s = mac_c2s_module(sess);
	dssh_mac old_mac_s2c = mac_s2c_module(sess);

	bool initial = sess->trans.session_id == NULL;
	bool keep_owner = atomic_load_explicit(&sess->trans.rekey_in_progress, memory_order_acquire);
	int  res        = tx_acquire(sess, SSH_MSG_NEWKEYS);

	if (res < 0) {
		kex_cleanup_state(sess);
		return res;
	}
	uint8_t newkeys_msg = SSH_MSG_NEWKEYS;

	res = send_packet_inner(sess, &newkeys_msg, 1, NULL);
	if (res < 0)
		goto newkeys_done;

	uint8_t  msg_type;
	uint8_t *payload;
	size_t   payload_len;

	res = recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0)
		goto newkeys_done;
	if (msg_type != SSH_MSG_NEWKEYS || payload_len != 1) {
		res = DSSH_ERROR_PARSE;
		goto newkeys_done;
	}

	/* Save session ID on first KEX */
	if (sess->trans.session_id == NULL) {
		sess->trans.session_id = malloc(sess->trans.kex_result.exchange_hash.len);
		if (sess->trans.session_id == NULL) {
			res = DSSH_ERROR_ALLOC;
			goto newkeys_done;
		}
		memcpy(sess->trans.session_id, sess->trans.kex_result.exchange_hash.data, sess->trans.kex_result.exchange_hash.len);
		sess->trans.session_id_sz = sess->trans.kex_result.exchange_hash.len;
	}

	/* Encode shared secret K for key derivation */
	bool     k_as_string = (sess->trans.kex_selected->flags & DSSH_KEX_FLAG_K_ENCODING_STRING) != 0;
	uint8_t *k_mpint;
	size_t   k_mpint_sz;

	res = encode_k_wire(sess->trans.kex_result.shared_secret.data, sess->trans.kex_result.shared_secret.len, k_as_string, &k_mpint,
	    &k_mpint_sz);
	if (res < 0)
		goto newkeys_done;

	/* Derive keys, init contexts, reset counters */
	res = derive_and_apply_keys(sess, k_mpint, k_mpint_sz, old_enc_c2s, old_enc_s2c, old_mac_c2s, old_mac_s2c);
	cleanse_free(k_mpint, k_mpint_sz);
	if (res < 0)
		goto newkeys_done;
	res = apply_compression(sess);
	if (res < 0)
		goto newkeys_done;

	/* Allocate or resize TX slot buffers for the new algorithms.  A
	 * producer may have queued work while rekey was in progress, so the
	 * resize path preserves any live slot payload while replacing its
	 * backing buffer. */
	res = alloc_session_slots(sess);
	if (res < 0)
		goto newkeys_done;

	/* Resize channel slots if demux thread is running (rekey case).
	 * On initial KEX there are no channels yet. */
	if (sess->conn_initialized) {
		res = resize_channel_slots(sess);
	}
	if (res == 0 && initial) {
		sess->trans.ext_info_state = sess->trans.ext_info_receive
		    ? DSSH_EXT_INFO_INITIAL : DSSH_EXT_INFO_DISABLED;
		/* Keep exclusive packet-engine ownership: EXT_INFO is the first
		 * new-key packet, before diagnostics, queued slots or other senders. */
		if (!sess->trans.client && sess->trans.ext_info_peer)
			res = send_server_ext_info(sess);
	}

newkeys_done:
	kex_release_result(sess);
	if (res < 0)
		kex_cleanup_state(sess);
	/* During rekey the caller clears rekey_in_progress before releasing
	 * ownership, so no old-key connection packet can follow NEWKEYS. */
	if (res < 0)
		session_set_terminate_no_wake(sess);
	if (res < 0 || !keep_owner)
		tx_unlock_with_slots(sess);
	return res;
}

DSSH_TESTABLE int
build_server_ext_info(uint8_t *buf, size_t capacity, size_t *out_length)
{
	size_t fixed = 13 + DSSH_STRLEN("server-sig-algs");
	if (capacity < fixed)
		return DSSH_ERROR_TOOLONG;
	size_t pos = 0;
	buf[pos++] = SSH_MSG_EXT_INFO;
	DSSH_PUT_U32(1, buf, &pos);
	DSSH_PUT_U32(DSSH_STRLEN("server-sig-algs"), buf, &pos);
	memcpy(&buf[pos], "server-sig-algs", DSSH_STRLEN("server-sig-algs"));
	pos += DSSH_STRLEN("server-sig-algs");
	size_t prefix = pos;
	pos += 4;
	size_t start = pos;
	/* Auth finds every registered verifier independently of host-key filter,
	 * capability flags, loaded private keys or per-user authorization. */
	for (dssh_key_algo key = gconf.key_algo_head; key != NULL; key = key->next) {
		if (key->verify == NULL)
			continue;
		size_t length = strlen(key->name);
		if (length == 0 || length > DSSH_ALGO_NAME_MAX)
			return DSSH_ERROR_INVALID;
		size_t comma = pos > start ? 1 : 0;
		if (!dssh_buffer_has(capacity, pos, comma + length))
			return DSSH_ERROR_TOOLONG;
		if (comma != 0)
			buf[pos++] = ',';
		memcpy(&buf[pos], key->name, length);
		pos += length;
	}
	size_t length = pos - start;
	if (length > UINT32_MAX)
		return DSSH_ERROR_TOOLONG;
	uint32_t wire_length = (uint32_t)length;
	DSSH_PUT_U32(wire_length, buf, &prefix);
	*out_length = pos;
	return 0;
}

static int
send_server_ext_info(struct dssh_session_s *sess)
{
	size_t length;
	int ret = build_server_ext_info(&sess->trans.tx_packet[9], tx_payload_capacity(sess), &length);
	if (ret < 0)
		return ret;
	return tx_finalize(sess, sess->trans.tx_packet, length);
}

/* ================================================================
 * Init / cleanup / registration
 * ================================================================ */

static int
transport_init(struct dssh_session_s *sess, size_t max_packet_size)
{
	struct timespec key_time;
	int clock_ret = dssh_clock_read(&key_time);
	if (clock_ret < 0)
		return clock_ret;

	if (max_packet_size < SSH_BPP_PACKET_SIZE_MIN)
		max_packet_size = SSH_BPP_PACKET_SIZE_MIN;
#if SIZE_MAX < SSH_BPP_PACKET_SIZE_MAX
	if (max_packet_size > SIZE_MAX - 4)
		max_packet_size = SIZE_MAX - 4;
#else
	if (max_packet_size > SSH_BPP_PACKET_SIZE_MAX)
		max_packet_size = SSH_BPP_PACKET_SIZE_MAX;
#endif

	int  ret             = DSSH_ERROR_ALLOC;
	bool tx_mtx_ok       = false;
	bool tx_queue_mtx_ok = false;
	bool tx_slot_cnd_ok  = false;

	sess->trans.packet_buf_sz = max_packet_size;
	sess->trans.tx_packet     = malloc(4 + max_packet_size);
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
	if (cnd_init(&sess->trans.tx_slot_cnd) != thrd_success)
		goto init_cleanup;
	tx_slot_cnd_ok = true;
	if (cnd_init(&sess->trans.rekey_cnd) != thrd_success)
		goto init_cleanup;

	sess->trans.rekey_pending     = false;

	sess->trans.session_id    = NULL;
	sess->trans.our_kexinit   = NULL;
	sess->trans.peer_kexinit  = NULL;
	sess->trans.kex_active = NULL;
	sess->trans.kex_state = NULL;
	sess->trans.kex_result = (struct dssh_kex_result){0};
	sess->trans.tx_seq        = 0;
	sess->trans.rx_seq        = 0;
	sess->trans.drain_next_ch = 0;
	sess->trans.rx_since_rekey = 0;
	sess->trans.rx_bytes_since_rekey = 0;
	sess->trans.rekey_time           = key_time.tv_sec;

	return 0;

init_cleanup:
	if (tx_slot_cnd_ok)
		cnd_destroy(&sess->trans.tx_slot_cnd);
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
	kex_cleanup_state(sess);
	if (sess->trans.kex_selected)
		sess->trans.kex_selected = NULL;
	if (sess->trans.key_algo_selected)
		sess->trans.key_algo_selected = NULL;
	if (enc_c2s_module(sess) != NULL) {
		if (enc_c2s_module(sess)->cleanup != NULL)
			enc_c2s_module(sess)->cleanup(sess->trans.enc_c2s_ctx);
		sess->trans.enc_c2s_ctx      = NULL;
		sess->trans.enc_c2s_selected = NULL;
		sess->trans.enc_c2s_active = NULL;
	}
	if (enc_s2c_module(sess) != NULL) {
		if (enc_s2c_module(sess)->cleanup != NULL)
			enc_s2c_module(sess)->cleanup(sess->trans.enc_s2c_ctx);
		sess->trans.enc_s2c_ctx      = NULL;
		sess->trans.enc_s2c_selected = NULL;
		sess->trans.enc_s2c_active = NULL;
	}
	if (mac_c2s_module(sess) != NULL) {
		if (mac_c2s_module(sess)->cleanup != NULL)
			mac_c2s_module(sess)->cleanup(sess->trans.mac_c2s_ctx);
		sess->trans.mac_c2s_ctx      = NULL;
		sess->trans.mac_c2s_selected = NULL;
		sess->trans.mac_c2s_active = NULL;
	}
	if (mac_s2c_module(sess) != NULL) {
		if (mac_s2c_module(sess)->cleanup != NULL)
			mac_s2c_module(sess)->cleanup(sess->trans.mac_s2c_ctx);
		sess->trans.mac_s2c_ctx      = NULL;
		sess->trans.mac_s2c_selected = NULL;
		sess->trans.mac_s2c_active = NULL;
	}
	if (sess->trans.comp_c2s_active != NULL)
		call_comp_cleanup(sess, sess->trans.comp_c2s_active, sess->trans.comp_c2s_ctx);
	if (sess->trans.comp_s2c_active != NULL)
		call_comp_cleanup(sess, sess->trans.comp_s2c_active, sess->trans.comp_s2c_ctx);
	sess->trans.comp_c2s_ctx = NULL;
	sess->trans.comp_c2s_active = NULL;
	sess->trans.comp_c2s_selected = NULL;
	sess->trans.comp_s2c_ctx = NULL;
	sess->trans.comp_s2c_active = NULL;
	sess->trans.comp_s2c_selected = NULL;

	cnd_destroy(&sess->trans.rekey_cnd);
	cnd_destroy(&sess->trans.tx_slot_cnd);
	mtx_destroy(&sess->trans.tx_mtx);
	mtx_destroy(&sess->trans.tx_queue_mtx);

	/* Free session-level TX slot buffers */
	free_tx_slot(&sess->trans.global_reply_slot);
	free_tx_slot(&sess->trans.open_fail_slot);

	/* Free rekey queue (non-empty only if rekey failed mid-flight) */
	{
		struct dssh_rekey_msg *rmsg = sess->trans.rekey_queue_head;

		while (rmsg != NULL) {
			struct dssh_rekey_msg *next = rmsg->next;

			cleanse_free(rmsg, sizeof(*rmsg) + rmsg->len);
			rmsg = next;
		}
		sess->trans.rekey_queue_head = NULL;
		sess->trans.rekey_queue_tail = NULL;
	}

	cleanse_free(sess->trans.comp_tx_packet, 4 + sess->trans.packet_buf_sz);
	sess->trans.comp_tx_packet = NULL;
	cleanse_free(sess->trans.comp_rx_input, sess->trans.packet_buf_sz);
	sess->trans.comp_rx_input = NULL;
	cleanse_free(sess->trans.tx_gather_buf, sess->trans.tx_gather_capacity);
	sess->trans.tx_gather_buf = NULL;
	sess->trans.tx_gather_capacity = 0;
	cleanse_free(sess->trans.tx_packet, 4 + sess->trans.packet_buf_sz);
	sess->trans.tx_packet = NULL;
	cleanse_free(sess->trans.rx_packet, 4 + sess->trans.packet_buf_sz);
	sess->trans.rx_packet = NULL;
	cleanse_free(sess->trans.rx_mac_buf, sess->trans.rx_mac_capacity);
	sess->trans.rx_mac_buf = NULL;
	cleanse_free(sess->trans.rx_mac_computed, sess->trans.rx_mac_capacity);
	sess->trans.rx_mac_computed = NULL;
	sess->trans.rx_mac_capacity = 0;
	sess->trans.packet_buf_sz   = 0;

	cleanse_free(sess->trans.session_id, sess->trans.session_id_sz);
	sess->trans.session_id = NULL;
	free(sess->trans.our_kexinit);
	sess->trans.our_kexinit = NULL;
	free(sess->trans.peer_kexinit);
	sess->trans.peer_kexinit = NULL;

	sess->trans.remote_id_str_sz = 0;
	sess->trans.remote_id_str[0] = 0;
	for (size_t i = 0; i < 2; i++) {
		free(sess->local_languages[i].data);
		sess->local_languages[i].data = NULL;
		sess->local_languages[i].len = 0;
		sess->local_languages[i].capacity = 0;
		sess->peer_languages_len[i] = 0;
	}
	free(sess->peer_languages);
	sess->peer_languages = NULL;
	sess->peer_languages_capacity = 0;
	sess->peer_languages_available = false;
}

/* ================================================================
 * Session lifecycle (formerly in ssh.c)
 * ================================================================ */

DSSH_PUBLIC struct dssh_session_s *
dssh_session_init(bool client, size_t max_packet_size)
{
	if (dssh_result_callback_active())
		return NULL;
	/* Application-serialized mutable setup is complete before this entry.
	 * Freeze every accepted attempt before its first fallible allocation. */
	gconf.used = true;
	struct dssh_session_s *sess = malloc(sizeof(*sess));

	if (sess == NULL)
		return NULL;

	/* This unpublished object needs C pointer/scalar values, not a byte
	 * pattern. Initialize each atomic's additional implementation state
	 * before any atomic operation, native construction or publication. */
	*sess = (struct dssh_session_s){.trans = {.client = client},
	    .timeout_ms = DSSH_DEFAULT_TIMEOUT_MS, .default_max_events = DSSH_DEFAULT_MAX_EVENTS};
	atomic_init(&sess->initialized, false);
	atomic_init(&sess->terminate, false);
	atomic_init(&sess->demux_running, false);
	atomic_init(&sess->conn_initialized, false);
	atomic_init(&sess->log_level, DSSH_LOG_ERROR);
	atomic_init(&sess->termination_logged, false);
	atomic_init(&sess->terminate_notified, false);
	atomic_init(&sess->terminate_wake_started, false);
	atomic_init(&sess->lifecycle_busy, false);
	atomic_init(&sess->log_mirror_suppressed, false);
	atomic_init(&sess->trans.kex_selected, NULL);
	atomic_init(&sess->trans.key_algo_selected, NULL);
	atomic_init(&sess->trans.enc_c2s_selected, NULL);
	atomic_init(&sess->trans.enc_s2c_selected, NULL);
	atomic_init(&sess->trans.mac_c2s_selected, NULL);
	atomic_init(&sess->trans.mac_s2c_selected, NULL);
	atomic_init(&sess->trans.comp_c2s_selected, NULL);
	atomic_init(&sess->trans.comp_s2c_selected, NULL);
	atomic_init(&sess->trans.tx_since_rekey, 0);
	atomic_init(&sess->trans.tx_bytes_since_rekey, 0);
	atomic_init(&sess->trans.rekey_in_progress, false);
#ifdef DSSH_TESTING
	atomic_init(&sess->trans.test_rekeys_completed, 0);
#endif
	atomic_init(&sess->trans.rekey_read, DSSH_REKEY_READ_IDLE);
	atomic_init(&sess->trans.tx_owner_active, false);
	/* TX slots remain empty until newkeys allocates their byte buffers. */
	atomic_init(&sess->trans.global_reply_slot.ready, false);
	atomic_init(&sess->trans.open_fail_slot.ready, false);
	/* Time-based rekey is OFF by default: the byte and packet
	 * thresholds cover key-material wear, and some peers (Cryptlib-
	 * based servers like Mystic BBS) refuse mid-stream KEXINIT
	 * outright.  RFC 4253 s9 calls time rekey RECOMMENDED, not
	 * required.  Apps that need it can opt in via
	 * dssh_session_set_rekey_seconds(). */
	atomic_init(&sess->trans.rekey_seconds, 0);

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
	sess->initialized        = true;
	return sess;
}

DSSH_PUBLIC bool
dssh_session_terminate(struct dssh_session_s *sess)
{
	if (sess == NULL)
		return false;
	if (dssh_io_callback_active(sess))
		return false;

	bool t = true;

	if (atomic_compare_exchange_strong(&sess->initialized, &t, false)) {
		int cause = sess->terminate ? DSSH_ERROR_TERMINATED : DSSH_ERROR_NONE;

		session_set_terminate(sess);
		dssh_log_termination(sess, cause);
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
	if (dssh_io_callback_active(sess))
		return;
	if (!session_lifecycle_begin(sess))
		return;
	dssh_session_terminate(sess);
	session_stop_inner(sess);
	dssh_log_cleanup(sess);
	dssh_client_keys_cleanup(sess);
	transport_cleanup(sess);
	free(sess->pending_banner);
	free(sess->pending_banner_lang);
	free(sess->auth_methods);
	dssh_cleanse(sess->server_auth_user, sess->server_auth_user_capacity);
	free(sess->server_auth_user);
	free(sess->kex_filter);
	free(sess->key_algo_filter);
	free(sess->enc_filter);
	free(sess->mac_filter);
	free(sess->comp_filter);
	mtx_destroy(&sess->mtx);
	free(sess);
}

/* Called only with a known nonnull name. Eligibility checks do not publish. */
static int
key_algo_setup_inner(const char *name, dssh_key_algo *out)
{
	if (dssh_result_callback_active())
		return DSSH_ERROR_INVALID;
	/* Refuse after first accepted session-init attempt, including failure. */
	if (gconf.used)
		return DSSH_ERROR_TOOLATE;
	dssh_key_algo ka = find_key_algo(name);
	if (ka == NULL)
		return DSSH_ERROR_INIT;
	*out = ka;
	return 0;
}

DSSH_PRIVATE int
dssh_key_algo_setup_check(const char *name)
{
	dssh_key_algo ka;
	return key_algo_setup_inner(name, &ka);
}

DSSH_PUBLIC int
dssh_key_algo_set_ctx(const char *name, void *ctx)
{
	if (name == NULL)
		return DSSH_ERROR_INVALID;
	dssh_key_algo ka;
	int ret = key_algo_setup_inner(name, &ka);
	if (ret < 0)
		return ret;
	ka->ctx = (dssh_key_algo_ctx *)ctx;
	return 0;
}

DSSH_PUBLIC int
dssh_kex_set_ctx(const char *name, void *ctx)
{
	if (dssh_result_callback_active())
		return DSSH_ERROR_INVALID;
	if (name == NULL)
		return DSSH_ERROR_INVALID;
	/* Refuse after first session init -- the global registry is
	 * read concurrently by active sessions during KEX. */
	if (gconf.used)
		return DSSH_ERROR_TOOLATE;
	dssh_kex k = find_kex(name);

	if (k == NULL)
		return DSSH_ERROR_INIT;
	k->config = ctx;
	return 0;
}

#define DEFINE_REGISTER(func_name, param_type, head, tail, entries, valid) \
	DSSH_PUBLIC int func_name(param_type item) \
	{ \
		if (dssh_result_callback_active()) \
			return DSSH_ERROR_INVALID; \
		if (item == NULL) \
			return DSSH_ERROR_INVALID; \
		if (gconf.used) \
			return DSSH_ERROR_TOOLATE; \
		size_t name_len = 0; \
		while (name_len <= DSSH_ALGO_NAME_MAX && item->name[name_len] != 0) \
			name_len++; \
		if (name_len == 0) \
			return DSSH_ERROR_INVALID; \
		if (name_len > DSSH_ALGO_NAME_MAX) \
			return DSSH_ERROR_TOOLONG; \
		for (size_t i = 0; i < name_len; i++) { \
			unsigned char ch = (unsigned char)item->name[i]; \
			if (ch <= ' ' || ch >= DSSH_ASCII_DEL || ch == ',') \
				return DSSH_ERROR_INVALID; \
		} \
		if (item->next != NULL) \
			return DSSH_ERROR_MUST_BE_NULL; \
		if (gconf.entries >= SIZE_MAX - 1) \
			return DSSH_ERROR_TOOMANY; \
		for (param_type current = gconf.head; current != NULL; current = current->next) { \
			if (current == item || strcmp(current->name, item->name) == 0) \
				return DSSH_ERROR_INVALID; \
		} \
		if (!(valid)) \
			return DSSH_ERROR_INVALID; \
		if (gconf.head == NULL) \
			gconf.head = item; \
		if (gconf.tail != NULL) \
			gconf.tail->next = item; \
		gconf.tail = item; \
		gconf.entries++; \
		return 0; \
	}

DEFINE_REGISTER(dssh_transport_register_kex, dssh_kex, kex_head, kex_tail, kex_entries,
    item->init != NULL && item->handler != NULL && item->cleanup != NULL && item->hash_name != NULL
        && strcmp(item->name, "ext-info-c") != 0 && strcmp(item->name, "ext-info-s") != 0)
DEFINE_REGISTER(dssh_transport_register_key_algo, dssh_key_algo, key_algo_head, key_algo_tail, key_algo_entries, true)
DEFINE_REGISTER(dssh_transport_register_enc, dssh_enc, enc_head, enc_tail, enc_entries,
    item->key_size == 0 || (item->init != NULL && item->encrypt != NULL && item->decrypt != NULL))
DEFINE_REGISTER(dssh_transport_register_mac, dssh_mac, mac_head, mac_tail, mac_entries,
    (item->key_size == 0 || item->init != NULL) && (item->digest_size == 0 || item->generate != NULL))
DEFINE_REGISTER(dssh_transport_register_comp, dssh_comp, comp_head, comp_tail, comp_entries,
    item->init != NULL && item->bound != NULL && item->compress != NULL
        && item->uncompress != NULL && item->cleanup != NULL)
#undef DEFINE_REGISTER

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
dssh_transport_set_version(const char *software_version, const char *comment)
{
	if (dssh_result_callback_active())
		return DSSH_ERROR_INVALID;
	if (gconf.used)
		return DSSH_ERROR_TOOLATE;

	/* NULL software_version leaves the default; validate if provided */
	const char *sv     = software_version != NULL ? software_version : sw_ver;
	size_t      sv_len = strlen(sv);

	if (sv_len == 0)
		return DSSH_ERROR_PARSE;
	if (!is_valid_sw_version(sv, sv_len))
		return DSSH_ERROR_INVALID;

	/* ssh_version_prefix + version + " " + comment + CR LF <= DSSH_VERSION_STRING_MAX */
	size_t prefix_len = DSSH_STRLEN(ssh_version_prefix);

	if (!dssh_buffer_has(DSSH_VERSION_STRING_MAX, prefix_len, sv_len)
	    || !dssh_buffer_has(DSSH_VERSION_STRING_MAX, prefix_len + sv_len, 2))
		return DSSH_ERROR_TOOLONG;
	size_t total = prefix_len + sv_len + 2;

	if (comment != NULL) {
		size_t cm_len = strlen(comment);

		if (cm_len == 0)
			return DSSH_ERROR_PARSE;
		if (!is_valid_comment(comment, cm_len))
			return DSSH_ERROR_INVALID;
		if (!dssh_buffer_has(DSSH_VERSION_STRING_MAX, total, 1)
		    || !dssh_buffer_has(DSSH_VERSION_STRING_MAX, total + 1, cm_len))
			return DSSH_ERROR_TOOLONG;
		total += 1 + cm_len; /* SP + comment */
	}
	if (total > DSSH_VERSION_STRING_MAX)
		return DSSH_ERROR_TOOLONG;

	gconf.software_version = sv;
	gconf.version_comment  = comment;
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
rxline_from_rx(uint8_t *buf, size_t bufsz, size_t *bytes_received, struct dssh_session_s *sess, void *cbdata)
{
	size_t have = 0;
	int    res;

	while (have < bufsz) {
		res = call_io(sess, gconf.rx, &buf[have], 1, cbdata);
		if (res < 0)
			return res;
		have++;
		if (buf[have - 1] == '\r') {
			if (have >= bufsz)
				return DSSH_ERROR_TOOLONG;
			res = call_io(sess, gconf.rx, &buf[have], 1, cbdata);
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
dssh_transport_set_callbacks(dssh_transport_io_cb tx, dssh_transport_io_cb rx, dssh_transport_rxline_cb rx_line,
    dssh_transport_extra_line_cb extra_line_cb)
{
	if (dssh_result_callback_active())
		return DSSH_ERROR_INVALID;
	if (gconf.used)
		return DSSH_ERROR_TOOLATE;
	if (tx == NULL || rx == NULL)
		return DSSH_ERROR_INVALID;
	gconf.tx            = tx;
	gconf.rx            = rx;
	gconf.rx_line       = (rx_line != NULL) ? rx_line : rxline_from_rx;
	gconf.extra_line_cb = extra_line_cb;
	return 0;
}

DSSH_PUBLIC int
dssh_transport_set_tx_gather(dssh_transport_tx_gather_cb cb)
{
	if (dssh_result_callback_active())
		return DSSH_ERROR_INVALID;
	if (gconf.used)
		return DSSH_ERROR_TOOLATE;
	gconf.tx_gather = cb;
	return 0;
}

DSSH_PRIVATE bool
tx_gather_enabled(void)
{
	return gconf.tx_gather != NULL;
}

#ifdef DSSH_TESTING

/*
 * Reset the global configuration for test isolation.
 * Frees all registered algorithm entries, restores typed defaults,
 * and unlocks registration.
 */
DSSH_PRIVATE void
dssh_test_reset_global_config(void)
{
 #define FREE_LIST(type, head)                         \
	 do {                                          \
		 for (type _n = (head); _n != NULL;) { \
			 type _next = _n->next;        \
			 free(_n);                     \
			 _n = _next;                   \
		 }                                     \
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
 #undef FREE_LIST

	gconf = (struct dssh_transport_global_config){.software_version = sw_ver};
	atomic_init(&gconf.used, false);
	atomic_store(&dssh_test_rx_count_sess, NULL);
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
