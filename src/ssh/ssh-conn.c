// RFC 4254: SSH Connection Protocol

#include <stdlib.h>
#include <string.h>

#include "deucessh.h"
#include "ssh-conn.h"

#define INITIAL_WINDOW_SIZE  0x200000
#define MAX_PACKET_SIZE      0x8000

/* Channel open failure reason codes (RFC 4254 s5.1) */
#define SSH_OPEN_ADMINISTRATIVELY_PROHIBITED  UINT32_C(1)
#define SSH_OPEN_CONNECT_FAILED               UINT32_C(2)
#define SSH_OPEN_UNKNOWN_CHANNEL_TYPE         UINT32_C(3)
#define SSH_OPEN_RESOURCE_SHORTAGE            UINT32_C(4)

/*
 * Saturating add for window sizes — clamp at UINT32_MAX per RFC 4254 s5.2.
 */
static inline uint32_t
window_add(uint32_t current, uint32_t bytes)
{
	uint32_t result = current + bytes;
	if (result < current)
		return UINT32_MAX;
	return result;
}

/* ================================================================
 * Low-level wire functions — internal only.
 * Used by the high-level API; not part of the public interface.
 * ================================================================ */

static int
deuce_ssh_conn_send_data(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const uint8_t *data, size_t len)
{
	if (len > ch->remote_window || len > ch->remote_max_packet)
		return DEUCE_SSH_ERROR_TOOLONG;

	size_t msg_len = 9 + len;
	uint8_t *msg = malloc(msg_len);
	if (msg == NULL)
		return DEUCE_SSH_ERROR_ALLOC;

	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_DATA;
	deuce_ssh_serialize_uint32(ch->remote_id, msg, msg_len, &pos);
	deuce_ssh_serialize_uint32((uint32_t)len, msg, msg_len, &pos);
	memcpy(&msg[pos], data, len);
	pos += len;

	ch->remote_window -= (uint32_t)len;
	int res = deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
	free(msg);
	return res;
}

static int
deuce_ssh_conn_send_extended_data(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint32_t data_type_code,
    const uint8_t *data, size_t len)
{
	if (len > ch->remote_window || len > ch->remote_max_packet)
		return DEUCE_SSH_ERROR_TOOLONG;

	size_t msg_len = 13 + len;
	uint8_t *msg = malloc(msg_len);
	if (msg == NULL)
		return DEUCE_SSH_ERROR_ALLOC;

	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_EXTENDED_DATA;
	deuce_ssh_serialize_uint32(ch->remote_id, msg, msg_len, &pos);
	deuce_ssh_serialize_uint32(data_type_code, msg, msg_len, &pos);
	deuce_ssh_serialize_uint32((uint32_t)len, msg, msg_len, &pos);
	memcpy(&msg[pos], data, len);
	pos += len;

	ch->remote_window -= (uint32_t)len;
	int res = deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
	free(msg);
	return res;
}

#define SSH_EXTENDED_DATA_STDERR  UINT32_C(1)

static int
deuce_ssh_conn_send_window_adjust(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint32_t bytes)
{
	uint8_t msg[32];
	size_t pos = 0;

	msg[pos++] = SSH_MSG_CHANNEL_WINDOW_ADJUST;
	deuce_ssh_serialize_uint32(ch->remote_id, msg, sizeof(msg), &pos);
	deuce_ssh_serialize_uint32(bytes, msg, sizeof(msg), &pos);

	ch->local_window = window_add(ch->local_window, bytes);
	return deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
}

static int
deuce_ssh_conn_send_exit_status(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint32_t exit_code)
{
	static const char req[] = "exit-status";
	uint8_t msg[32];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
	deuce_ssh_serialize_uint32(ch->remote_id, msg, sizeof(msg), &pos);
	deuce_ssh_serialize_uint32(sizeof(req) - 1, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], req, sizeof(req) - 1);
	pos += sizeof(req) - 1;
	msg[pos++] = 0; /* want_reply = false */
	deuce_ssh_serialize_uint32(exit_code, msg, sizeof(msg), &pos);
	return deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
}

static int
deuce_ssh_conn_send_eof(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch)
{
	if (ch->eof_sent)
		return 0;

	uint8_t msg[8];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_EOF;
	deuce_ssh_serialize_uint32(ch->remote_id, msg, sizeof(msg), &pos);

	ch->eof_sent = true;
	return deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
}

static int
deuce_ssh_conn_close(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch)
{
	if (ch->close_sent)
		return 0;

	uint8_t msg[8];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_CLOSE;
	deuce_ssh_serialize_uint32(ch->remote_id, msg, sizeof(msg), &pos);

	ch->close_sent = true;
	ch->open = false;
	return deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
}

/* ================================================================
 * Channel table helpers
 * ================================================================ */

static int
register_channel(deuce_ssh_session sess, struct deuce_ssh_channel_s *ch)
{
	mtx_lock(&sess->channel_mtx);
	if (sess->channel_count >= sess->channel_capacity) {
		size_t newcap = sess->channel_capacity ? sess->channel_capacity * 2 : 8;
		struct deuce_ssh_channel_s **newt = realloc(sess->channels,
		    newcap * sizeof(*newt));
		if (newt == NULL) {
			mtx_unlock(&sess->channel_mtx);
			return DEUCE_SSH_ERROR_ALLOC;
		}
		sess->channels = newt;
		sess->channel_capacity = newcap;
	}
	sess->channels[sess->channel_count++] = ch;
	mtx_unlock(&sess->channel_mtx);
	return 0;
}

static struct deuce_ssh_channel_s *
find_channel(deuce_ssh_session sess, uint32_t local_id)
{
	mtx_lock(&sess->channel_mtx);
	for (size_t i = 0; i < sess->channel_count; i++) {
		if (sess->channels[i]->local_id == local_id) {
			struct deuce_ssh_channel_s *ch = sess->channels[i];
			mtx_unlock(&sess->channel_mtx);
			return ch;
		}
	}
	mtx_unlock(&sess->channel_mtx);
	return NULL;
}

static void
unregister_channel(deuce_ssh_session sess, struct deuce_ssh_channel_s *ch)
{
	mtx_lock(&sess->channel_mtx);
	for (size_t i = 0; i < sess->channel_count; i++) {
		if (sess->channels[i] == ch) {
			sess->channels[i] = sess->channels[--sess->channel_count];
			break;
		}
	}
	mtx_unlock(&sess->channel_mtx);
}

static uint32_t
alloc_channel_id(deuce_ssh_session sess)
{
	return sess->next_channel_id++;
}

/* ================================================================
 * Channel buffer initialization
 * ================================================================ */

static int
init_session_channel(struct deuce_ssh_channel_s *ch, uint32_t window_max)
{
	ch->chan_type = DEUCE_SSH_CHAN_SESSION;
	ch->window_max = window_max;
	ch->stdout_consumed = 0;
	ch->stderr_consumed = 0;
	ch->exit_code = 0;
	ch->exit_code_received = false;

	if (mtx_init(&ch->buf_mtx, mtx_plain) != thrd_success)
		return DEUCE_SSH_ERROR_INIT;
	if (cnd_init(&ch->poll_cnd) != thrd_success) {
		mtx_destroy(&ch->buf_mtx);
		return DEUCE_SSH_ERROR_INIT;
	}

	int res = deuce_ssh_bytebuf_init(&ch->buf.session.stdout_buf, window_max);
	if (res < 0) {
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->buf_mtx);
		return res;
	}
	res = deuce_ssh_bytebuf_init(&ch->buf.session.stderr_buf, window_max);
	if (res < 0) {
		deuce_ssh_bytebuf_free(&ch->buf.session.stdout_buf);
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->buf_mtx);
		return res;
	}
	deuce_ssh_sigqueue_init(&ch->buf.session.signals);
	return 0;
}

static int
init_raw_channel(struct deuce_ssh_channel_s *ch, uint32_t window_max)
{
	ch->chan_type = DEUCE_SSH_CHAN_RAW;
	ch->window_max = window_max;

	if (mtx_init(&ch->buf_mtx, mtx_plain) != thrd_success)
		return DEUCE_SSH_ERROR_INIT;
	if (cnd_init(&ch->poll_cnd) != thrd_success) {
		mtx_destroy(&ch->buf_mtx);
		return DEUCE_SSH_ERROR_INIT;
	}

	deuce_ssh_msgqueue_init(&ch->buf.raw.queue);
	return 0;
}

static void
cleanup_channel_buffers(struct deuce_ssh_channel_s *ch)
{
	if (ch->chan_type == DEUCE_SSH_CHAN_SESSION) {
		deuce_ssh_bytebuf_free(&ch->buf.session.stdout_buf);
		deuce_ssh_bytebuf_free(&ch->buf.session.stderr_buf);
		deuce_ssh_sigqueue_free(&ch->buf.session.signals);
	}
	else if (ch->chan_type == DEUCE_SSH_CHAN_RAW) {
		deuce_ssh_msgqueue_free(&ch->buf.raw.queue);
	}
	free(ch->setup_payload);
	ch->setup_payload = NULL;
	if (ch->chan_type != 0) {
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->buf_mtx);
		ch->chan_type = 0;
	}
}

/* ================================================================
 * Window replenishment — called after reads drain buffer
 * ================================================================ */

static void
maybe_replenish_window(deuce_ssh_session sess, struct deuce_ssh_channel_s *ch)
{
	/* No point replenishing if peer is done sending */
	if (ch->eof_received || ch->close_received)
		return;
	/* Replenish when window drops below half the target */
	if (ch->local_window < ch->window_max / 2) {
		uint32_t add = ch->window_max - ch->local_window;
		deuce_ssh_conn_send_window_adjust(sess, ch, add);
	}
}

/* ================================================================
 * Demux thread
 * ================================================================ */

/*
 * Dispatch a received packet to the appropriate channel buffer.
 * Called from the demux thread with no locks held.
 */
static void
demux_dispatch(deuce_ssh_session sess, uint8_t msg_type,
    uint8_t *payload, size_t payload_len)
{
	/* All channel messages have the recipient channel at payload[1..4] */
	if (payload_len < 5)
		return;
	uint32_t local_id;
	deuce_ssh_parse_uint32(&payload[1], payload_len - 1, &local_id);
	struct deuce_ssh_channel_s *ch = find_channel(sess, local_id);
	if (ch == NULL)
		return;

	mtx_lock(&ch->buf_mtx);

	/*
	 * Setup mode: during session_accept_channel, deliver messages
	 * via the mailbox instead of processing them normally.
	 */
	if (ch->setup_mode) {
		free(ch->setup_payload);
		ch->setup_payload = malloc(payload_len);
		if (ch->setup_payload != NULL) {
			memcpy(ch->setup_payload, payload, payload_len);
			ch->setup_payload_len = payload_len;
			ch->setup_msg_type = msg_type;
			ch->setup_ready = true;
		}
		cnd_signal(&ch->poll_cnd);
		mtx_unlock(&ch->buf_mtx);
		return;
	}

	/* Normal mode requires initialized buffers */
	if (ch->chan_type == 0) {
		mtx_unlock(&ch->buf_mtx);
		return;
	}

	switch (msg_type) {
	case SSH_MSG_CHANNEL_DATA:
		if (payload_len < 9)
			break;
		/* Discard data after peer sent EOF or CLOSE */
		if (ch->eof_received || ch->close_received)
			break;
		{
			uint32_t dlen;
			deuce_ssh_parse_uint32(&payload[5], payload_len - 5, &dlen);
			if (9 + dlen > payload_len)
				dlen = (uint32_t)(payload_len - 9);
			const uint8_t *data = &payload[9];

			if (ch->chan_type == DEUCE_SSH_CHAN_SESSION) {
				deuce_ssh_bytebuf_write(&ch->buf.session.stdout_buf, data, dlen);
			}
			else if (ch->chan_type == DEUCE_SSH_CHAN_RAW) {
				deuce_ssh_msgqueue_push(&ch->buf.raw.queue, data, dlen);
			}
			if (dlen <= ch->local_window)
				ch->local_window -= dlen;
			else
				ch->local_window = 0;
		}
		break;

	case SSH_MSG_CHANNEL_EXTENDED_DATA:
		if (payload_len < 13)
			break;
		if (ch->eof_received || ch->close_received)
			break;
		{
			uint32_t data_type;
			deuce_ssh_parse_uint32(&payload[5], payload_len - 5, &data_type);
			uint32_t dlen;
			deuce_ssh_parse_uint32(&payload[9], payload_len - 9, &dlen);
			if (13 + dlen > payload_len)
				dlen = (uint32_t)(payload_len - 13);
			const uint8_t *data = &payload[13];

			if (ch->chan_type == DEUCE_SSH_CHAN_SESSION && data_type == 1) {
				deuce_ssh_bytebuf_write(&ch->buf.session.stderr_buf, data, dlen);
			}
			if (dlen <= ch->local_window)
				ch->local_window -= dlen;
			else
				ch->local_window = 0;
		}
		break;

	case SSH_MSG_CHANNEL_WINDOW_ADJUST:
		if (payload_len >= 9) {
			uint32_t bytes;
			deuce_ssh_parse_uint32(&payload[5], payload_len - 5, &bytes);
			ch->remote_window = window_add(ch->remote_window, bytes);
		}
		break;

	case SSH_MSG_CHANNEL_EOF:
		ch->eof_received = true;
		break;

	case SSH_MSG_CHANNEL_CLOSE:
		ch->close_received = true;
		/*
		 * Don't auto-send the reciprocal CLOSE here.  The
		 * application needs a chance to drain buffered data,
		 * send exit-status, and clean up before the channel
		 * is fully closed.  poll() will wake with POLL_READ
		 * (read returns 0), and the application calls
		 * session_close() or channel_close() which sends
		 * the reciprocal CLOSE via conn_close() (idempotent).
		 */
		break;

	case SSH_MSG_CHANNEL_REQUEST:
		/* Parse request type */
		if (payload_len < 6)
			break;
		{
			size_t rpos = 5;
			uint32_t rtype_len;
			if (deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &rtype_len) < 4)
				break;
			rpos += 4;
			if (rpos + rtype_len + 1 > payload_len)
				break;
			const uint8_t *rtype = &payload[rpos];
			rpos += rtype_len;
			bool want_reply = (payload[rpos] != 0);
			rpos++;

			if (ch->chan_type == DEUCE_SSH_CHAN_SESSION) {
				if (rtype_len == 6 && memcmp(rtype, "signal", 6) == 0) {
					/* Queue signal with current stream positions */
					if (rpos + 4 <= payload_len) {
						uint32_t sname_len;
						deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &sname_len);
						rpos += 4;
						if (rpos + sname_len <= payload_len) {
							char sname[32];
							size_t sn = sname_len < sizeof(sname) - 1 ? sname_len : sizeof(sname) - 1;
							memcpy(sname, &payload[rpos], sn);
							sname[sn] = 0;
							deuce_ssh_sigqueue_push(
							    &ch->buf.session.signals,
							    sname,
							    ch->buf.session.stdout_buf.total,
							    ch->buf.session.stderr_buf.total);
						}
					}
				}
				else if (rtype_len == 11 && memcmp(rtype, "exit-status", 11) == 0) {
					if (rpos + 4 <= payload_len) {
						deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &ch->exit_code);
						ch->exit_code_received = true;
					}
				}
				else if (rtype_len == 13 && memcmp(rtype, "window-change", 13) == 0) {
					if (ch->window_change_cb != NULL && rpos + 16 <= payload_len) {
						uint32_t wc_cols, wc_rows, wc_wpx, wc_hpx;
						deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &wc_cols);
						deuce_ssh_parse_uint32(&payload[rpos + 4], payload_len - rpos - 4, &wc_rows);
						deuce_ssh_parse_uint32(&payload[rpos + 8], payload_len - rpos - 8, &wc_wpx);
						deuce_ssh_parse_uint32(&payload[rpos + 12], payload_len - rpos - 12, &wc_hpx);
						mtx_unlock(&ch->buf_mtx);
						ch->window_change_cb(wc_cols, wc_rows, wc_wpx, wc_hpx,
						    ch->window_change_cbdata);
						mtx_lock(&ch->buf_mtx);
					}
				}
				/* Ignore: xon-xoff, exit-signal, pty-req from server, etc. */
			}

			/* Send FAILURE for unhandled requests with want_reply */
			if (want_reply) {
				uint8_t fail[8];
				size_t fp = 0;
				fail[fp++] = SSH_MSG_CHANNEL_FAILURE;
				deuce_ssh_serialize_uint32(ch->remote_id, fail, sizeof(fail), &fp);
				mtx_unlock(&ch->buf_mtx);
				deuce_ssh_transport_send_packet(sess, fail, fp, NULL);
				mtx_lock(&ch->buf_mtx);
			}
		}
		break;

	case SSH_MSG_CHANNEL_OPEN_FAILURE:
		/* Server rejected our CHANNEL_OPEN.  Mark the channel
		 * so open_session_channel's wait loop exits. */
		ch->close_received = true;
		break;

	case SSH_MSG_CHANNEL_SUCCESS:
	case SSH_MSG_CHANNEL_FAILURE:
		if (ch->request_pending) {
			ch->request_success = (msg_type == SSH_MSG_CHANNEL_SUCCESS);
			ch->request_responded = true;
		}
		break;
	}

	cnd_signal(&ch->poll_cnd);
	mtx_unlock(&ch->buf_mtx);
}

/*
 * Handle CHANNEL_OPEN_CONFIRMATION — recipient_channel is at offset 1.
 */
static void
demux_open_confirmation(deuce_ssh_session sess,
    uint8_t *payload, size_t payload_len)
{
	if (payload_len < 1 + 16)
		return;
	uint32_t local_id;
	deuce_ssh_parse_uint32(&payload[1], payload_len - 1, &local_id);
	struct deuce_ssh_channel_s *ch = find_channel(sess, local_id);
	if (ch == NULL)
		return;

	mtx_lock(&ch->buf_mtx);
	deuce_ssh_parse_uint32(&payload[5], payload_len - 5, &ch->remote_id);
	deuce_ssh_parse_uint32(&payload[9], payload_len - 9, &ch->remote_window);
	deuce_ssh_parse_uint32(&payload[13], payload_len - 13, &ch->remote_max_packet);
	ch->open = true;
	cnd_signal(&ch->poll_cnd);
	mtx_unlock(&ch->buf_mtx);
}

/*
 * Handle incoming CHANNEL_OPEN — either queue for accept or auto-reject.
 */
static void
demux_channel_open(deuce_ssh_session sess, uint8_t *payload, size_t payload_len)
{
	/* Parse: string channel_type, uint32 sender_channel,
	 * uint32 initial_window, uint32 max_packet */
	size_t rpos = 1;
	uint32_t type_len;
	if (rpos + 4 > payload_len)
		return;
	deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &type_len);
	rpos += 4;
	if (rpos + type_len > payload_len)
		return;
	const uint8_t *ctype = &payload[rpos];
	rpos += type_len;

	if (rpos + 12 > payload_len)
		return;
	uint32_t peer_channel, peer_window, peer_max_packet;
	deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &peer_channel);
	rpos += 4;
	deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &peer_window);
	rpos += 4;
	deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &peer_max_packet);

	/* Auto-reject forbidden channel types */
	bool reject = false;
	if (type_len == 2 && memcmp(ctype, "x11", 3) == 0)
		reject = true;
	if (type_len == 3 && memcmp(ctype, "x11", 3) == 0)
		reject = true;
	if (type_len == 15 && memcmp(ctype, "forwarded-tcpip", 15) == 0)
		reject = true;
	if (type_len == 12 && memcmp(ctype, "direct-tcpip", 12) == 0)
		reject = true;
	/* Client rejects "session" opens from server (RFC 4254 s6.1) */
	if (sess->trans.client && type_len == 7 && memcmp(ctype, "session", 7) == 0)
		reject = true;

	if (reject) {
		/* Send CHANNEL_OPEN_FAILURE */
		uint8_t fail[64];
		size_t fp = 0;
		fail[fp++] = SSH_MSG_CHANNEL_OPEN_FAILURE;
		deuce_ssh_serialize_uint32(peer_channel, fail, sizeof(fail), &fp);
		deuce_ssh_serialize_uint32(SSH_OPEN_ADMINISTRATIVELY_PROHIBITED, fail, sizeof(fail), &fp);
		deuce_ssh_serialize_uint32(0, fail, sizeof(fail), &fp); /* empty description */
		deuce_ssh_serialize_uint32(0, fail, sizeof(fail), &fp); /* empty language */
		deuce_ssh_transport_send_packet(sess, fail, fp, NULL);
		return;
	}

	/* Queue for application's accept() call */
	mtx_lock(&sess->accept_mtx);
	deuce_ssh_acceptqueue_push(&sess->accept_queue,
	    peer_channel, peer_window, peer_max_packet, ctype, type_len);
	cnd_signal(&sess->accept_cnd);
	mtx_unlock(&sess->accept_mtx);
}

static int
demux_thread_func(void *arg)
{
	deuce_ssh_session sess = arg;
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;

	while (!sess->terminate) {
		int res = deuce_ssh_transport_recv_packet(sess,
		    &msg_type, &payload, &payload_len);
		if (res < 0) {
			if (res == DEUCE_SSH_ERROR_TERMINATED)
				break;
			/* Other errors — terminate */
			sess->terminate = true;
			break;
		}

		if (msg_type == SSH_MSG_CHANNEL_OPEN) {
			demux_channel_open(sess, payload, payload_len);
		}
		else if (msg_type == SSH_MSG_CHANNEL_OPEN_CONFIRMATION) {
			demux_open_confirmation(sess, payload, payload_len);
		}
		else if (msg_type >= SSH_MSG_CHANNEL_OPEN_FAILURE &&
		    msg_type <= SSH_MSG_CHANNEL_FAILURE) {
			demux_dispatch(sess, msg_type, payload, payload_len);
		}
		/* All other message types (GLOBAL_REQUEST, KEXINIT, etc.)
		 * are already handled by recv_packet transparently */
	}

	/* Wake any waiters so they see termination */
	mtx_lock(&sess->accept_mtx);
	cnd_broadcast(&sess->accept_cnd);
	mtx_unlock(&sess->accept_mtx);

	mtx_lock(&sess->channel_mtx);
	for (size_t i = 0; i < sess->channel_count; i++) {
		struct deuce_ssh_channel_s *ch = sess->channels[i];
		if (ch->chan_type != 0) {
			mtx_lock(&ch->buf_mtx);
			cnd_signal(&ch->poll_cnd);
			mtx_unlock(&ch->buf_mtx);
		}
	}
	mtx_unlock(&sess->channel_mtx);

	sess->demux_running = false;
	return 0;
}

DEUCE_SSH_PUBLIC int
deuce_ssh_session_start(deuce_ssh_session sess)
{
	if (sess->demux_running)
		return DEUCE_SSH_ERROR_INIT;

	if (mtx_init(&sess->channel_mtx, mtx_plain) != thrd_success)
		return DEUCE_SSH_ERROR_INIT;
	if (mtx_init(&sess->accept_mtx, mtx_plain) != thrd_success) {
		mtx_destroy(&sess->channel_mtx);
		return DEUCE_SSH_ERROR_INIT;
	}
	if (cnd_init(&sess->accept_cnd) != thrd_success) {
		mtx_destroy(&sess->accept_mtx);
		mtx_destroy(&sess->channel_mtx);
		return DEUCE_SSH_ERROR_INIT;
	}

	sess->channels = NULL;
	sess->channel_count = 0;
	sess->channel_capacity = 0;
	sess->next_channel_id = 0;
	deuce_ssh_acceptqueue_init(&sess->accept_queue);

	sess->demux_running = true;
	if (thrd_create(&sess->demux_thread, demux_thread_func, sess) != thrd_success) {
		sess->demux_running = false;
		cnd_destroy(&sess->accept_cnd);
		mtx_destroy(&sess->accept_mtx);
		mtx_destroy(&sess->channel_mtx);
		return DEUCE_SSH_ERROR_INIT;
	}

	return 0;
}

DEUCE_SSH_PUBLIC void
deuce_ssh_session_stop(deuce_ssh_session sess)
{
	if (!sess->demux_running && sess->channels == NULL)
		return;

	/* Signal termination and join the demux thread */
	if (sess->demux_running) {
		sess->terminate = true;
		int demux_res;
		thrd_join(sess->demux_thread, &demux_res);
	}

	/* Clean up registered channels */
	if (sess->channels != NULL) {
		for (size_t i = 0; i < sess->channel_count; i++) {
			struct deuce_ssh_channel_s *ch = sess->channels[i];
			if (ch != NULL)
				cleanup_channel_buffers(ch);
		}
		free(sess->channels);
		sess->channels = NULL;
		sess->channel_count = 0;
		sess->channel_capacity = 0;
	}

	deuce_ssh_acceptqueue_free(&sess->accept_queue);
	cnd_destroy(&sess->accept_cnd);
	mtx_destroy(&sess->accept_mtx);
	mtx_destroy(&sess->channel_mtx);
}

/* ================================================================
 * Session accept — wait for incoming CHANNEL_OPEN
 * ================================================================ */

DEUCE_SSH_PUBLIC int
deuce_ssh_session_accept(deuce_ssh_session sess,
    struct deuce_ssh_incoming_open **inc, int timeout_ms)
{
	mtx_lock(&sess->accept_mtx);

	while (sess->accept_queue.head == NULL) {
		if (sess->terminate || !sess->demux_running) {
			mtx_unlock(&sess->accept_mtx);
			return DEUCE_SSH_ERROR_TERMINATED;
		}
		if (timeout_ms == 0) {
			mtx_unlock(&sess->accept_mtx);
			return DEUCE_SSH_ERROR_NOMORE;
		}
		if (timeout_ms < 0) {
			cnd_wait(&sess->accept_cnd, &sess->accept_mtx);
		}
		else {
			struct timespec ts;
			timespec_get(&ts, TIME_UTC);
			ts.tv_sec += timeout_ms / 1000;
			ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
			if (ts.tv_nsec >= 1000000000L) {
				ts.tv_sec++;
				ts.tv_nsec -= 1000000000L;
			}
			if (cnd_timedwait(&sess->accept_cnd, &sess->accept_mtx, &ts) == thrd_timedout) {
				mtx_unlock(&sess->accept_mtx);
				return DEUCE_SSH_ERROR_NOMORE;
			}
		}
	}

	*inc = deuce_ssh_acceptqueue_pop(&sess->accept_queue);
	mtx_unlock(&sess->accept_mtx);
	return 0;
}

DEUCE_SSH_PUBLIC void
deuce_ssh_session_reject(deuce_ssh_session sess,
    struct deuce_ssh_incoming_open *inc, uint32_t reason_code,
    const char *description)
{
	size_t dlen = description ? strlen(description) : 0;
	uint8_t msg[256];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_OPEN_FAILURE;
	deuce_ssh_serialize_uint32(inc->peer_channel, msg, sizeof(msg), &pos);
	deuce_ssh_serialize_uint32(reason_code, msg, sizeof(msg), &pos);
	deuce_ssh_serialize_uint32((uint32_t)dlen, msg, sizeof(msg), &pos);
	if (dlen > 0) {
		if (dlen > sizeof(msg) - pos - 4)
			dlen = sizeof(msg) - pos - 4;
		memcpy(&msg[pos], description, dlen);
		pos += dlen;
	}
	deuce_ssh_serialize_uint32(0, msg, sizeof(msg), &pos); /* language */
	deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
	free(inc);
}

/* ================================================================
 * High-level channel open helpers
 * ================================================================ */

/*
 * Send CHANNEL_OPEN_CONFIRMATION for an incoming channel.
 */
static int
send_open_confirmation(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint32_t peer_channel)
{
	uint8_t msg[32];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_OPEN_CONFIRMATION;
	deuce_ssh_serialize_uint32(peer_channel, msg, sizeof(msg), &pos);
	deuce_ssh_serialize_uint32(ch->local_id, msg, sizeof(msg), &pos);
	deuce_ssh_serialize_uint32(ch->local_window, msg, sizeof(msg), &pos);
	deuce_ssh_serialize_uint32(MAX_PACKET_SIZE, msg, sizeof(msg), &pos);
	return deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
}

/*
 * Common channel open: send CHANNEL_OPEN "session", wait for confirmation.
 * Does NOT start the demux — the channel is registered for demux dispatch.
 */
static int
open_session_channel(deuce_ssh_session sess, struct deuce_ssh_channel_s *ch)
{
	/* Caller has already initialized buffers (init_session_channel
	 * or init_raw_channel).  Set wire-level fields only. */
	ch->local_id = alloc_channel_id(sess);
	ch->local_window = INITIAL_WINDOW_SIZE;
	ch->open = false;
	ch->close_sent = false;
	ch->close_received = false;
	ch->eof_sent = false;
	ch->eof_received = false;

	/* Send CHANNEL_OPEN via the low-level function.
	 * The demux thread is running, but this message's response
	 * (CHANNEL_OPEN_CONFIRMATION) will be dispatched to our channel
	 * once registered.  However, we're not registered yet and the
	 * demux thread would drop it.
	 *
	 * Solution: use the low-level conn_open_session which does
	 * send + synchronous recv (before the message reaches demux).
	 * This works because recv_packet is called from our thread,
	 * not the demux thread — BUT the demux thread is also calling
	 * recv_packet, so there's a race.
	 *
	 * The correct approach: register the channel first, have
	 * the demux thread handle the CONFIRMATION and wake us via
	 * the poll condition variable.
	 *
	 * For now, use a simpler approach: send the CHANNEL_OPEN,
	 * register the channel, then wait for the open flag.
	 */
	static const char type[] = "session";
	uint8_t msg[256];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_OPEN;
	deuce_ssh_serialize_uint32((uint32_t)(sizeof(type) - 1), msg, sizeof(msg), &pos);
	memcpy(&msg[pos], type, sizeof(type) - 1);
	pos += sizeof(type) - 1;
	deuce_ssh_serialize_uint32(ch->local_id, msg, sizeof(msg), &pos);
	deuce_ssh_serialize_uint32(ch->local_window, msg, sizeof(msg), &pos);
	deuce_ssh_serialize_uint32(MAX_PACKET_SIZE, msg, sizeof(msg), &pos);

	int res = register_channel(sess, ch);
	if (res < 0)
		return res;

	res = deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
	if (res < 0) {
		unregister_channel(sess, ch);
		return res;
	}

	/* Wait for CONFIRMATION — the demux thread handles it */
	mtx_lock(&ch->buf_mtx);
	while (!ch->open && !ch->close_received && !sess->terminate)
		cnd_wait(&ch->poll_cnd, &ch->buf_mtx);
	mtx_unlock(&ch->buf_mtx);

	if (!ch->open) {
		unregister_channel(sess, ch);
		return DEUCE_SSH_ERROR_INIT;
	}
	return 0;
}

/*
 * Send a CHANNEL_REQUEST with want_reply=true and wait for the demux
 * thread to deliver SUCCESS or FAILURE.  Returns 0 on SUCCESS,
 * DEUCE_SSH_ERROR_INIT on FAILURE.
 */
static int
send_channel_request_wait(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch,
    const char *req_type, const uint8_t *extra, size_t extra_len)
{
	size_t rtlen = strlen(req_type);
	size_t msg_len = 1 + 4 + 4 + rtlen + 1 + extra_len;
	uint8_t *msg = malloc(msg_len);
	if (msg == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
	deuce_ssh_serialize_uint32(ch->remote_id, msg, msg_len, &pos);
	deuce_ssh_serialize_uint32((uint32_t)rtlen, msg, msg_len, &pos);
	memcpy(&msg[pos], req_type, rtlen);
	pos += rtlen;
	msg[pos++] = 1; /* want_reply = TRUE */
	if (extra_len > 0) {
		memcpy(&msg[pos], extra, extra_len);
		pos += extra_len;
	}

	/* Set up the pending response slot */
	mtx_lock(&ch->buf_mtx);
	ch->request_pending = true;
	ch->request_responded = false;
	ch->request_success = false;
	mtx_unlock(&ch->buf_mtx);

	int res = deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
	free(msg);
	if (res < 0) {
		mtx_lock(&ch->buf_mtx);
		ch->request_pending = false;
		mtx_unlock(&ch->buf_mtx);
		return res;
	}

	/* Wait for the demux thread to deliver the response */
	mtx_lock(&ch->buf_mtx);
	while (!ch->request_responded && !sess->terminate && !ch->close_received)
		cnd_wait(&ch->poll_cnd, &ch->buf_mtx);

	bool success = ch->request_success;
	ch->request_pending = false;
	ch->request_responded = false;
	mtx_unlock(&ch->buf_mtx);

	if (sess->terminate || ch->close_received)
		return DEUCE_SSH_ERROR_TERMINATED;
	return success ? 0 : DEUCE_SSH_ERROR_INIT;
}

/* ================================================================
 * High-level open functions
 * ================================================================ */

DEUCE_SSH_PUBLIC int
deuce_ssh_session_open_shell(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch,
    const struct deuce_ssh_pty_req *pty)
{
	int res = init_session_channel(ch, INITIAL_WINDOW_SIZE);
	if (res < 0)
		return res;

	res = open_session_channel(sess, ch);
	if (res < 0) {
		cleanup_channel_buffers(ch);
		return res;
	}

	/* Send pty-req */
	{
		size_t tlen = strlen(pty->term);
		size_t mlen = pty->modes_len;
		size_t extra_len = 4 + tlen + 4 + 4 + 4 + 4 + 4 + mlen;
		uint8_t *extra = malloc(extra_len);
		if (extra == NULL) {
			deuce_ssh_conn_close(sess, ch);
			unregister_channel(sess, ch);
			cleanup_channel_buffers(ch);
			return DEUCE_SSH_ERROR_ALLOC;
		}
		size_t ep = 0;
		deuce_ssh_serialize_uint32((uint32_t)tlen, extra, extra_len, &ep);
		memcpy(&extra[ep], pty->term, tlen);
		ep += tlen;
		deuce_ssh_serialize_uint32(pty->cols, extra, extra_len, &ep);
		deuce_ssh_serialize_uint32(pty->rows, extra, extra_len, &ep);
		deuce_ssh_serialize_uint32(pty->wpx, extra, extra_len, &ep);
		deuce_ssh_serialize_uint32(pty->hpx, extra, extra_len, &ep);
		deuce_ssh_serialize_uint32((uint32_t)mlen, extra, extra_len, &ep);
		if (mlen > 0)
			memcpy(&extra[ep], pty->modes, mlen);
		ep += mlen;
		res = send_channel_request_wait(sess, ch, "pty-req", extra, ep);
		free(extra);
		if (res < 0)
			goto open_fail;
	}

	/* Send shell */
	res = send_channel_request_wait(sess, ch, "shell", NULL, 0);
	if (res < 0)
		goto open_fail;

	return 0;

open_fail:
	deuce_ssh_conn_close(sess, ch);
	unregister_channel(sess, ch);
	cleanup_channel_buffers(ch);
	return res;
}

DEUCE_SSH_PUBLIC int
deuce_ssh_session_open_exec(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch,
    const char *command)
{
	int res = init_session_channel(ch, INITIAL_WINDOW_SIZE);
	if (res < 0)
		return res;

	res = open_session_channel(sess, ch);
	if (res < 0) {
		cleanup_channel_buffers(ch);
		return res;
	}

	/* Send exec */
	size_t cmdlen = strlen(command);
	size_t extra_len = 4 + cmdlen;
	uint8_t *extra = malloc(extra_len);
	if (extra == NULL) {
		res = DEUCE_SSH_ERROR_ALLOC;
		goto open_fail;
	}
	size_t ep = 0;
	deuce_ssh_serialize_uint32((uint32_t)cmdlen, extra, extra_len, &ep);
	memcpy(&extra[ep], command, cmdlen);
	ep += cmdlen;
	res = send_channel_request_wait(sess, ch, "exec", extra, ep);
	free(extra);
	if (res < 0)
		goto open_fail;

	return 0;

open_fail:
	deuce_ssh_conn_close(sess, ch);
	unregister_channel(sess, ch);
	cleanup_channel_buffers(ch);
	return res;
}

DEUCE_SSH_PUBLIC int
deuce_ssh_channel_open_subsystem(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch,
    const char *subsystem)
{
	int res = init_raw_channel(ch, INITIAL_WINDOW_SIZE);
	if (res < 0)
		return res;

	res = open_session_channel(sess, ch);
	if (res < 0) {
		cleanup_channel_buffers(ch);
		return res;
	}

	/* Send subsystem */
	size_t slen = strlen(subsystem);
	size_t extra_len = 4 + slen;
	uint8_t *extra = malloc(extra_len);
	if (extra == NULL) {
		res = DEUCE_SSH_ERROR_ALLOC;
		goto open_fail;
	}
	size_t ep = 0;
	deuce_ssh_serialize_uint32((uint32_t)slen, extra, extra_len, &ep);
	memcpy(&extra[ep], subsystem, slen);
	ep += slen;
	res = send_channel_request_wait(sess, ch, "subsystem", extra, ep);
	free(extra);
	if (res < 0)
		goto open_fail;

	return 0;

open_fail:
	deuce_ssh_conn_close(sess, ch);
	unregister_channel(sess, ch);
	cleanup_channel_buffers(ch);
	return res;
}

/* ================================================================
 * Server-side accept helpers
 * ================================================================ */

DEUCE_SSH_PUBLIC int
deuce_ssh_channel_accept_raw(deuce_ssh_session sess,
    struct deuce_ssh_incoming_open *inc,
    struct deuce_ssh_channel_s *ch)
{
	memset(ch, 0, sizeof(*ch));
	ch->local_id = alloc_channel_id(sess);
	ch->local_window = INITIAL_WINDOW_SIZE;
	ch->remote_id = inc->peer_channel;
	ch->remote_window = inc->peer_window;
	ch->remote_max_packet = inc->peer_max_packet;

	int res = init_raw_channel(ch, INITIAL_WINDOW_SIZE);
	if (res < 0)
		return res;

	res = register_channel(sess, ch);
	if (res < 0) {
		cleanup_channel_buffers(ch);
		return res;
	}

	res = send_open_confirmation(sess, ch, inc->peer_channel);
	free(inc);
	if (res < 0) {
		unregister_channel(sess, ch);
		cleanup_channel_buffers(ch);
		return res;
	}

	ch->open = true;
	return 0;
}

/* ================================================================
 * Server-side session accept (callback-driven setup)
 * ================================================================ */

/*
 * Wait for the next message in the setup mailbox.
 * Returns 0 on success, negative on error/termination.
 */
static int
setup_recv(deuce_ssh_session sess, struct deuce_ssh_channel_s *ch,
    uint8_t *msg_type, uint8_t **payload, size_t *payload_len)
{
	mtx_lock(&ch->buf_mtx);
	while (!ch->setup_ready && !sess->terminate && !ch->close_received)
		cnd_wait(&ch->poll_cnd, &ch->buf_mtx);

	if (!ch->setup_ready) {
		mtx_unlock(&ch->buf_mtx);
		return DEUCE_SSH_ERROR_TERMINATED;
	}

	*msg_type = ch->setup_msg_type;
	*payload = ch->setup_payload;
	*payload_len = ch->setup_payload_len;
	ch->setup_ready = false;
	/* Caller takes ownership of setup_payload and must free it */
	ch->setup_payload = NULL;
	ch->setup_payload_len = 0;
	mtx_unlock(&ch->buf_mtx);
	return 0;
}

/*
 * Send CHANNEL_SUCCESS or CHANNEL_FAILURE for a setup request.
 */
static int
setup_reply(deuce_ssh_session sess, struct deuce_ssh_channel_s *ch,
    bool success)
{
	uint8_t msg[8];
	size_t pos = 0;
	msg[pos++] = success ? SSH_MSG_CHANNEL_SUCCESS : SSH_MSG_CHANNEL_FAILURE;
	deuce_ssh_serialize_uint32(ch->remote_id, msg, sizeof(msg), &pos);
	return deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
}

DEUCE_SSH_PUBLIC int
deuce_ssh_session_accept_channel(deuce_ssh_session sess,
    struct deuce_ssh_incoming_open *inc,
    struct deuce_ssh_channel_s *ch,
    const struct deuce_ssh_server_session_cbs *cbs,
    const char **request_type, const char **request_data)
{
	memset(ch, 0, sizeof(*ch));
	ch->local_id = alloc_channel_id(sess);
	ch->local_window = INITIAL_WINDOW_SIZE;
	ch->remote_id = inc->peer_channel;
	ch->remote_window = inc->peer_window;
	ch->remote_max_packet = inc->peer_max_packet;

	/* Set up mailbox for receiving setup messages from demux */
	if (mtx_init(&ch->buf_mtx, mtx_plain) != thrd_success)
		return DEUCE_SSH_ERROR_INIT;
	if (cnd_init(&ch->poll_cnd) != thrd_success) {
		mtx_destroy(&ch->buf_mtx);
		return DEUCE_SSH_ERROR_INIT;
	}
	ch->setup_mode = true;

	/* Register so the demux thread can deliver messages to us */
	int res = register_channel(sess, ch);
	if (res < 0) {
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->buf_mtx);
		return res;
	}

	/* Send CHANNEL_OPEN_CONFIRMATION */
	res = send_open_confirmation(sess, ch, inc->peer_channel);
	free(inc);
	if (res < 0) {
		unregister_channel(sess, ch);
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->buf_mtx);
		return res;
	}
	ch->open = true;

	/* Process setup requests until shell/exec/subsystem arrives */
	ch->req_type[0] = 0;
	ch->req_data[0] = 0;

	for (;;) {
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		res = setup_recv(sess, ch, &msg_type, &payload, &payload_len);
		if (res < 0)
			goto fail;

		if (msg_type != SSH_MSG_CHANNEL_REQUEST) {
			free(payload);
			/* Handle WINDOW_ADJUST during setup */
			if (msg_type == SSH_MSG_CHANNEL_WINDOW_ADJUST && payload_len >= 9) {
				uint32_t bytes;
				deuce_ssh_parse_uint32(&payload[5], payload_len - 5, &bytes);
				ch->remote_window = window_add(ch->remote_window, bytes);
			}
			continue;
		}

		/* Parse CHANNEL_REQUEST: channel(4) + string type + bool want_reply + data */
		size_t rpos = 5;
		uint32_t rtype_len;
		if (rpos + 4 > payload_len) {
			free(payload);
			continue;
		}
		deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &rtype_len);
		rpos += 4;
		if (rpos + rtype_len + 1 > payload_len) {
			free(payload);
			continue;
		}
		const uint8_t *rtype = &payload[rpos];
		rpos += rtype_len;
		bool want_reply = (payload[rpos] != 0);
		rpos++;

		/* pty-req */
		if (rtype_len == 7 && memcmp(rtype, "pty-req", 7) == 0) {
			int cb_res = -1;
			if (cbs->pty_req != NULL && rpos + 4 <= payload_len) {
				uint32_t tlen;
				deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &tlen);
				rpos += 4;
				if (rpos + tlen + 16 <= payload_len) {
					struct deuce_ssh_pty_req pty;
					/* NUL-terminate term for callback */
					char term_buf[256];
					size_t tn = tlen < sizeof(term_buf) - 1 ? tlen : sizeof(term_buf) - 1;
					memcpy(term_buf, &payload[rpos], tn);
					term_buf[tn] = 0;
					pty.term = term_buf;
					rpos += tlen;
					deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &pty.cols);
					deuce_ssh_parse_uint32(&payload[rpos + 4], payload_len - rpos - 4, &pty.rows);
					deuce_ssh_parse_uint32(&payload[rpos + 8], payload_len - rpos - 8, &pty.wpx);
					deuce_ssh_parse_uint32(&payload[rpos + 12], payload_len - rpos - 12, &pty.hpx);
					rpos += 16;
					/* Terminal modes */
					if (rpos + 4 <= payload_len) {
						uint32_t mlen;
						deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &mlen);
						rpos += 4;
						if (rpos + mlen <= payload_len) {
							pty.modes = &payload[rpos];
							pty.modes_len = mlen;
						}
						else {
							pty.modes = NULL;
							pty.modes_len = 0;
						}
					}
					else {
						pty.modes = NULL;
						pty.modes_len = 0;
					}
					cb_res = cbs->pty_req(&pty, cbs->cbdata);
				}
			}
			if (want_reply)
				setup_reply(sess, ch, cb_res >= 0);
			free(payload);
			continue;
		}

		/* env */
		if (rtype_len == 3 && memcmp(rtype, "env", 3) == 0) {
			int cb_res = -1;
			if (cbs->env != NULL && rpos + 4 <= payload_len) {
				uint32_t nlen;
				deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &nlen);
				rpos += 4;
				if (rpos + nlen + 4 <= payload_len) {
					const uint8_t *name = &payload[rpos];
					rpos += nlen;
					uint32_t vlen;
					deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &vlen);
					rpos += 4;
					if (rpos + vlen <= payload_len) {
						const uint8_t *value = &payload[rpos];
						cb_res = cbs->env(name, nlen, value, vlen, cbs->cbdata);
					}
				}
			}
			if (want_reply)
				setup_reply(sess, ch, cb_res >= 0);
			free(payload);
			continue;
		}

		/* window-change */
		if (rtype_len == 13 && memcmp(rtype, "window-change", 13) == 0) {
			if (cbs->window_change != NULL && rpos + 16 <= payload_len) {
				uint32_t cols, rows, wpx, hpx;
				deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &cols);
				deuce_ssh_parse_uint32(&payload[rpos + 4], payload_len - rpos - 4, &rows);
				deuce_ssh_parse_uint32(&payload[rpos + 8], payload_len - rpos - 8, &wpx);
				deuce_ssh_parse_uint32(&payload[rpos + 12], payload_len - rpos - 12, &hpx);
				cbs->window_change(cols, rows, wpx, hpx, cbs->cbdata);
			}
			/* want_reply is FALSE per RFC */
			free(payload);
			continue;
		}

		/* shell */
		if (rtype_len == 5 && memcmp(rtype, "shell", 5) == 0) {
			memcpy(ch->req_type, "shell", 6);
			ch->req_data[0] = 0;
			if (want_reply)
				setup_reply(sess, ch, true);
			free(payload);
			break;
		}

		/* exec */
		if (rtype_len == 4 && memcmp(rtype, "exec", 4) == 0) {
			memcpy(ch->req_type, "exec", 5);
			if (rpos + 4 <= payload_len) {
				uint32_t clen;
				deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &clen);
				rpos += 4;
				size_t cn = clen < sizeof(ch->req_data) - 1 ? clen : sizeof(ch->req_data) - 1;
				if (rpos + cn <= payload_len)
					memcpy(ch->req_data, &payload[rpos], cn);
				ch->req_data[cn] = 0;
			}
			if (want_reply)
				setup_reply(sess, ch, true);
			free(payload);
			break;
		}

		/* subsystem */
		if (rtype_len == 9 && memcmp(rtype, "subsystem", 9) == 0) {
			memcpy(ch->req_type, "subsystem", 10);
			if (rpos + 4 <= payload_len) {
				uint32_t slen;
				deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &slen);
				rpos += 4;
				size_t sn = slen < sizeof(ch->req_data) - 1 ? slen : sizeof(ch->req_data) - 1;
				if (rpos + sn <= payload_len)
					memcpy(ch->req_data, &payload[rpos], sn);
				ch->req_data[sn] = 0;
			}
			if (want_reply)
				setup_reply(sess, ch, true);
			free(payload);
			break;
		}

		/* Unknown request — reject */
		if (want_reply)
			setup_reply(sess, ch, false);
		free(payload);
	}

	/* Transition from setup mode to normal buffered mode.
	 * buf_mtx and poll_cnd are already initialized from setup —
	 * just init the buffers directly. */
	ch->setup_mode = false;
	ch->chan_type = DEUCE_SSH_CHAN_SESSION;
	ch->window_max = INITIAL_WINDOW_SIZE;
	ch->stdout_consumed = 0;
	ch->stderr_consumed = 0;
	ch->exit_code = 0;
	ch->exit_code_received = false;
	res = deuce_ssh_bytebuf_init(&ch->buf.session.stdout_buf, INITIAL_WINDOW_SIZE);
	if (res < 0)
		goto fail;
	res = deuce_ssh_bytebuf_init(&ch->buf.session.stderr_buf, INITIAL_WINDOW_SIZE);
	if (res < 0) {
		deuce_ssh_bytebuf_free(&ch->buf.session.stdout_buf);
		goto fail;
	}
	deuce_ssh_sigqueue_init(&ch->buf.session.signals);

	/* Install window-change callback for post-setup delivery */
	if (cbs->window_change != NULL) {
		ch->window_change_cb = cbs->window_change;
		ch->window_change_cbdata = cbs->cbdata;
	}

	if (request_type != NULL)
		*request_type = ch->req_type;
	if (request_data != NULL)
		*request_data = ch->req_data;
	return 0;

fail:
	ch->setup_mode = false;
	unregister_channel(sess, ch);
	cnd_destroy(&ch->poll_cnd);
	mtx_destroy(&ch->buf_mtx);
	return res;
}

/* ================================================================
 * Poll / Read / Write — Session channels (stream-based)
 * ================================================================ */

/*
 * Compute how many bytes are readable on stdout before hitting
 * a signal mark.
 */
static size_t
session_stdout_readable(struct deuce_ssh_channel_s *ch)
{
	size_t avail = deuce_ssh_bytebuf_available(&ch->buf.session.stdout_buf);
	if (avail == 0)
		return 0;
	struct deuce_ssh_signal_queue *sq = &ch->buf.session.signals;
	if (sq->head != NULL) {
		size_t mark = sq->head->stdout_pos;
		if (mark > ch->stdout_consumed) {
			size_t to_mark = mark - ch->stdout_consumed;
			if (to_mark < avail)
				avail = to_mark;
		}
	}
	return avail;
}

static size_t
session_stderr_readable(struct deuce_ssh_channel_s *ch)
{
	size_t avail = deuce_ssh_bytebuf_available(&ch->buf.session.stderr_buf);
	if (avail == 0)
		return 0;
	struct deuce_ssh_signal_queue *sq = &ch->buf.session.signals;
	if (sq->head != NULL) {
		size_t mark = sq->head->stderr_pos;
		if (mark > ch->stderr_consumed) {
			size_t to_mark = mark - ch->stderr_consumed;
			if (to_mark < avail)
				avail = to_mark;
		}
	}
	return avail;
}

DEUCE_SSH_PUBLIC int
deuce_ssh_session_poll(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, int events, int timeout_ms)
{
	mtx_lock(&ch->buf_mtx);

	for (;;) {
		int ready = 0;

		if ((events & DEUCE_SSH_POLL_READ) &&
		    (session_stdout_readable(ch) > 0 || ch->eof_received || ch->close_received))
			ready |= DEUCE_SSH_POLL_READ;

		if ((events & DEUCE_SSH_POLL_READEXT) &&
		    (session_stderr_readable(ch) > 0 || ch->eof_received || ch->close_received))
			ready |= DEUCE_SSH_POLL_READEXT;

		if ((events & DEUCE_SSH_POLL_WRITE) && ch->remote_window > 0 && ch->open && !ch->close_received)
			ready |= DEUCE_SSH_POLL_WRITE;

		if ((events & DEUCE_SSH_POLL_SIGNAL) &&
		    deuce_ssh_sigqueue_ready(&ch->buf.session.signals,
		        ch->stdout_consumed, ch->stderr_consumed))
			ready |= DEUCE_SSH_POLL_SIGNAL;

		if (ready || sess->terminate) {
			mtx_unlock(&ch->buf_mtx);
			return ready;
		}

		if (timeout_ms == 0) {
			mtx_unlock(&ch->buf_mtx);
			return 0;
		}

		if (timeout_ms < 0) {
			cnd_wait(&ch->poll_cnd, &ch->buf_mtx);
		}
		else {
			struct timespec ts;
			timespec_get(&ts, TIME_UTC);
			ts.tv_sec += timeout_ms / 1000;
			ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
			if (ts.tv_nsec >= 1000000000L) {
				ts.tv_sec++;
				ts.tv_nsec -= 1000000000L;
			}
			if (cnd_timedwait(&ch->poll_cnd, &ch->buf_mtx, &ts) == thrd_timedout) {
				mtx_unlock(&ch->buf_mtx);
				return 0;
			}
		}
	}
}

DEUCE_SSH_PUBLIC int64_t
deuce_ssh_session_read(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint8_t *buf, size_t bufsz)
{
	mtx_lock(&ch->buf_mtx);
	size_t limit = session_stdout_readable(ch);
	size_t n = deuce_ssh_bytebuf_read(&ch->buf.session.stdout_buf, buf, bufsz, limit);
	ch->stdout_consumed += n;
	mtx_unlock(&ch->buf_mtx);

	if (n > 0)
		maybe_replenish_window(sess, ch);
	return (int64_t)n;
}

DEUCE_SSH_PUBLIC int64_t
deuce_ssh_session_read_ext(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint8_t *buf, size_t bufsz)
{
	mtx_lock(&ch->buf_mtx);
	size_t limit = session_stderr_readable(ch);
	size_t n = deuce_ssh_bytebuf_read(&ch->buf.session.stderr_buf, buf, bufsz, limit);
	ch->stderr_consumed += n;
	mtx_unlock(&ch->buf_mtx);

	if (n > 0)
		maybe_replenish_window(sess, ch);
	return (int64_t)n;
}

DEUCE_SSH_PUBLIC int64_t
deuce_ssh_session_write(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const uint8_t *buf, size_t bufsz)
{
	if (!ch->open || ch->eof_sent || ch->close_received)
		return DEUCE_SSH_ERROR_INIT;

	uint32_t avail = ch->remote_window;
	if (avail == 0)
		return 0;
	size_t len = bufsz;
	if (len > avail)
		len = avail;
	if (len > ch->remote_max_packet)
		len = ch->remote_max_packet;

	int res = deuce_ssh_conn_send_data(sess, ch, buf, len);
	if (res < 0)
		return res;
	return (int64_t)len;
}

DEUCE_SSH_PUBLIC int64_t
deuce_ssh_session_write_ext(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const uint8_t *buf, size_t bufsz)
{
	if (!ch->open || ch->eof_sent || ch->close_received)
		return DEUCE_SSH_ERROR_INIT;

	uint32_t avail = ch->remote_window;
	if (avail == 0)
		return 0;
	size_t len = bufsz;
	if (len > avail)
		len = avail;
	if (len > ch->remote_max_packet)
		len = ch->remote_max_packet;

	int res = deuce_ssh_conn_send_extended_data(sess, ch,
	    SSH_EXTENDED_DATA_STDERR, buf, len);
	if (res < 0)
		return res;
	return (int64_t)len;
}

DEUCE_SSH_PUBLIC int
deuce_ssh_session_read_signal(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const char **signal_name)
{
	mtx_lock(&ch->buf_mtx);
	const char *name = deuce_ssh_sigqueue_pop(
	    &ch->buf.session.signals,
	    ch->stdout_consumed, ch->stderr_consumed,
	    ch->last_signal, sizeof(ch->last_signal));
	mtx_unlock(&ch->buf_mtx);

	if (name == NULL)
		return DEUCE_SSH_ERROR_NOMORE;
	*signal_name = ch->last_signal;
	return 0;
}

DEUCE_SSH_PUBLIC int
deuce_ssh_session_close(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint32_t exit_code)
{
	deuce_ssh_conn_send_exit_status(sess, ch, exit_code);
	deuce_ssh_conn_send_eof(sess, ch);
	deuce_ssh_conn_close(sess, ch);
	unregister_channel(sess, ch);
	cleanup_channel_buffers(ch);
	return 0;
}

/* ================================================================
 * Poll / Read / Write — Raw channels (message-based)
 * ================================================================ */

DEUCE_SSH_PUBLIC int
deuce_ssh_channel_poll(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, int events, int timeout_ms)
{
	mtx_lock(&ch->buf_mtx);

	for (;;) {
		int ready = 0;

		if ((events & DEUCE_SSH_POLL_READ) &&
		    (ch->buf.raw.queue.count > 0 || ch->eof_received || ch->close_received))
			ready |= DEUCE_SSH_POLL_READ;

		if ((events & DEUCE_SSH_POLL_WRITE) && ch->remote_window > 0 && ch->open && !ch->close_received)
			ready |= DEUCE_SSH_POLL_WRITE;

		if (ready || sess->terminate) {
			mtx_unlock(&ch->buf_mtx);
			return ready;
		}

		if (timeout_ms == 0) {
			mtx_unlock(&ch->buf_mtx);
			return 0;
		}

		if (timeout_ms < 0) {
			cnd_wait(&ch->poll_cnd, &ch->buf_mtx);
		}
		else {
			struct timespec ts;
			timespec_get(&ts, TIME_UTC);
			ts.tv_sec += timeout_ms / 1000;
			ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
			if (ts.tv_nsec >= 1000000000L) {
				ts.tv_sec++;
				ts.tv_nsec -= 1000000000L;
			}
			if (cnd_timedwait(&ch->poll_cnd, &ch->buf_mtx, &ts) == thrd_timedout) {
				mtx_unlock(&ch->buf_mtx);
				return 0;
			}
		}
	}
}

DEUCE_SSH_PUBLIC int64_t
deuce_ssh_channel_read(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint8_t *buf, size_t bufsz)
{
	mtx_lock(&ch->buf_mtx);
	int64_t n = deuce_ssh_msgqueue_pop(&ch->buf.raw.queue, buf, bufsz);
	mtx_unlock(&ch->buf_mtx);

	if (n > 0)
		maybe_replenish_window(sess, ch);
	return n;
}

DEUCE_SSH_PUBLIC int
deuce_ssh_channel_write(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const uint8_t *buf, size_t len)
{
	if (!ch->open || ch->eof_sent || ch->close_received)
		return DEUCE_SSH_ERROR_INIT;
	if (len > ch->remote_window || len > ch->remote_max_packet)
		return DEUCE_SSH_ERROR_TOOLONG;

	return deuce_ssh_conn_send_data(sess, ch, buf, len);
}

DEUCE_SSH_PUBLIC int
deuce_ssh_channel_close(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch)
{
	deuce_ssh_conn_send_eof(sess, ch);
	deuce_ssh_conn_close(sess, ch);
	unregister_channel(sess, ch);
	cleanup_channel_buffers(ch);
	return 0;
}

/* ================================================================
 * Window change (client-side)
 * ================================================================ */

DEUCE_SSH_PUBLIC int
deuce_ssh_session_send_signal(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const char *signal_name)
{
	size_t slen = strlen(signal_name);
	size_t rtlen = 6; /* "signal" */
	size_t msg_len = 1 + 4 + 4 + rtlen + 1 + 4 + slen;
	uint8_t *msg = malloc(msg_len);
	if (msg == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
	deuce_ssh_serialize_uint32(ch->remote_id, msg, msg_len, &pos);
	deuce_ssh_serialize_uint32((uint32_t)rtlen, msg, msg_len, &pos);
	memcpy(&msg[pos], "signal", rtlen);
	pos += rtlen;
	msg[pos++] = 0; /* want_reply = FALSE */
	deuce_ssh_serialize_uint32((uint32_t)slen, msg, msg_len, &pos);
	memcpy(&msg[pos], signal_name, slen);
	pos += slen;

	int res = deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
	free(msg);
	return res;
}

DEUCE_SSH_PUBLIC int
deuce_ssh_session_send_window_change(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch,
    uint32_t cols, uint32_t rows, uint32_t wpx, uint32_t hpx)
{
	uint8_t extra[16];
	size_t ep = 0;
	deuce_ssh_serialize_uint32(cols, extra, sizeof(extra), &ep);
	deuce_ssh_serialize_uint32(rows, extra, sizeof(extra), &ep);
	deuce_ssh_serialize_uint32(wpx, extra, sizeof(extra), &ep);
	deuce_ssh_serialize_uint32(hpx, extra, sizeof(extra), &ep);

	/* window-change uses want_reply=false */
	size_t rtlen = 13; /* "window-change" */
	size_t msg_len = 1 + 4 + 4 + rtlen + 1 + ep;
	uint8_t *msg = malloc(msg_len);
	if (msg == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
	deuce_ssh_serialize_uint32(ch->remote_id, msg, msg_len, &pos);
	deuce_ssh_serialize_uint32((uint32_t)rtlen, msg, msg_len, &pos);
	memcpy(&msg[pos], "window-change", rtlen);
	pos += rtlen;
	msg[pos++] = 0; /* want_reply = FALSE */
	memcpy(&msg[pos], extra, ep);
	pos += ep;

	int res = deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
	free(msg);
	return res;
}
