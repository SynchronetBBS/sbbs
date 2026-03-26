// RFC 4254: SSH Connection Protocol

#include <stdlib.h>
#include <string.h>

#include "deucessh-conn.h"
#include "ssh-internal.h"

#define INITIAL_WINDOW_SIZE 0x200000
#define MAX_PACKET_SIZE 0x8000

/* ================================================================
 * Parse helpers for channel request payloads
 * ================================================================ */

DSSH_PUBLIC int
dssh_parse_pty_req_data(const uint8_t *data, size_t data_len,
    struct dssh_pty_req *pty)
{
	size_t   pos = 0;
	int64_t  pv;
	uint32_t tlen;

	if (pos + 4 > data_len)
		return DSSH_ERROR_PARSE;
	pv = dssh_parse_uint32(&data[pos], data_len - pos, &tlen);
	if (pv < 0)
		return (int)pv;
	pos += 4;
	if (pos + tlen + 16 > data_len)
		return DSSH_ERROR_PARSE;
	pty->term = (const char *)&data[pos];
	pos += tlen;
	pv = dssh_parse_uint32(&data[pos], data_len - pos, &pty->cols);
	if (pv < 0)
		return (int)pv;
	pv = dssh_parse_uint32(&data[pos + 4], data_len - pos - 4, &pty->rows);
	if (pv < 0)
		return (int)pv;
	pv = dssh_parse_uint32(&data[pos + 8], data_len - pos - 8, &pty->wpx);
	if (pv < 0)
		return (int)pv;
	pv = dssh_parse_uint32(&data[pos + 12], data_len - pos - 12, &pty->hpx);
	if (pv < 0)
		return (int)pv;
	pos += 16;
	if (pos + 4 <= data_len) {
		uint32_t mlen;

		pv = dssh_parse_uint32(&data[pos], data_len - pos, &mlen);
		if (pv < 0)
			return (int)pv;
		pos += 4;
		if (pos + mlen <= data_len) {
			pty->modes = &data[pos];
			pty->modes_len = mlen;
		}
		else {
			pty->modes = NULL;
			pty->modes_len = 0;
		}
	}
	else {
		pty->modes = NULL;
		pty->modes_len = 0;
	}
	return 0;
}

DSSH_PUBLIC int
dssh_parse_env_data(const uint8_t *data, size_t data_len,
    const uint8_t **name, size_t *name_len,
    const uint8_t **value, size_t *value_len)
{
	size_t   pos = 0;
	int64_t  pv;
	uint32_t nlen;

	if (pos + 4 > data_len)
		return DSSH_ERROR_PARSE;
	pv = dssh_parse_uint32(&data[pos], data_len - pos, &nlen);
	if (pv < 0)
		return (int)pv;
	pos += 4;
	if (pos + nlen > data_len)
		return DSSH_ERROR_PARSE;
	*name = &data[pos];
	*name_len = nlen;
	pos += nlen;

	uint32_t vlen;

	if (pos + 4 > data_len)
		return DSSH_ERROR_PARSE;
	pv = dssh_parse_uint32(&data[pos], data_len - pos, &vlen);
	if (pv < 0)
		return (int)pv;
	pos += 4;
	if (pos + vlen > data_len)
		return DSSH_ERROR_PARSE;
	*value = &data[pos];
	*value_len = vlen;
	return 0;
}

DSSH_PUBLIC int
dssh_parse_exec_data(const uint8_t *data, size_t data_len,
    const uint8_t **command, size_t *command_len)
{
	if (data_len < 4)
		return DSSH_ERROR_PARSE;

	uint32_t clen;
	int64_t  pv;

	pv = dssh_parse_uint32(data, data_len, &clen);
	if (pv < 0)
		return (int)pv;
	if (4 + clen > data_len)
		return DSSH_ERROR_PARSE;
	*command = &data[4];
	*command_len = clen;
	return 0;
}

DSSH_PUBLIC int
dssh_parse_subsystem_data(const uint8_t *data, size_t data_len,
    const uint8_t **name, size_t *name_len)
{
	if (data_len < 4)
		return DSSH_ERROR_PARSE;

	uint32_t slen;
	int64_t  pv;

	pv = dssh_parse_uint32(data, data_len, &slen);
	if (pv < 0)
		return (int)pv;
	if (4 + slen > data_len)
		return DSSH_ERROR_PARSE;
	*name = &data[4];
	*name_len = slen;
	return 0;
}

/* Channel open failure reason codes (RFC 4254 s5.1) */
#define SSH_OPEN_ADMINISTRATIVELY_PROHIBITED UINT32_C(1)
#define SSH_OPEN_CONNECT_FAILED UINT32_C(2)
#define SSH_OPEN_UNKNOWN_CHANNEL_TYPE UINT32_C(3)
#define SSH_OPEN_RESOURCE_SHORTAGE UINT32_C(4)

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

DSSH_TESTABLE int
dssh_conn_send_data(dssh_session sess,
    dssh_channel ch, const uint8_t *data, size_t len)
{
	if ((len > ch->remote_window) || (len > ch->remote_max_packet))
		return DSSH_ERROR_TOOLONG;

	/* len <= remote_window (uint32_t), so fits in uint32_t */
	uint32_t len_u32 = (uint32_t)len;

	if (len > SIZE_MAX - 9)
		return DSSH_ERROR_INVALID;
	size_t   msg_len = 9 + len;
	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;
	int    ret;

	msg[pos++] = SSH_MSG_CHANNEL_DATA;
	ret = dssh_serialize_uint32(ch->remote_id, msg, msg_len, &pos);
	if (ret < 0)
		goto done;
	ret = dssh_serialize_uint32(len_u32, msg, msg_len, &pos);
	if (ret < 0)
		goto done;
	memcpy(&msg[pos], data, len);
	pos += len;

	ch->remote_window -= len_u32;

	ret = dssh_transport_send_packet(sess, msg, pos, NULL);

done:
	free(msg);
	return ret;
}

DSSH_TESTABLE int
dssh_conn_send_extended_data(dssh_session sess,
    dssh_channel ch, uint32_t data_type_code,
    const uint8_t *data, size_t len)
{
	if ((len > ch->remote_window) || (len > ch->remote_max_packet))
		return DSSH_ERROR_TOOLONG;

	/* len <= remote_window (uint32_t), so fits in uint32_t */
	uint32_t len_u32 = (uint32_t)len;

	if (len > SIZE_MAX - 13)
		return DSSH_ERROR_INVALID;
	size_t   msg_len = 13 + len;
	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;
	int    ret;

	msg[pos++] = SSH_MSG_CHANNEL_EXTENDED_DATA;
	ret = dssh_serialize_uint32(ch->remote_id, msg, msg_len, &pos);
	if (ret < 0)
		goto done;
	ret = dssh_serialize_uint32(data_type_code, msg, msg_len, &pos);
	if (ret < 0)
		goto done;
	ret = dssh_serialize_uint32(len_u32, msg, msg_len, &pos);
	if (ret < 0)
		goto done;
	memcpy(&msg[pos], data, len);
	pos += len;

	ch->remote_window -= len_u32;

	ret = dssh_transport_send_packet(sess, msg, pos, NULL);

done:
	free(msg);
	return ret;
}

#define SSH_EXTENDED_DATA_STDERR UINT32_C(1)

DSSH_TESTABLE int
dssh_conn_send_window_adjust(dssh_session sess,
    dssh_channel ch, uint32_t bytes)
{
	uint8_t msg[32];
	size_t  pos = 0;

	int ret;

	msg[pos++] = SSH_MSG_CHANNEL_WINDOW_ADJUST;
	ret = dssh_serialize_uint32(ch->remote_id, msg, sizeof(msg), &pos);
	if (ret < 0)
		return ret;
	ret = dssh_serialize_uint32(bytes, msg, sizeof(msg), &pos);
	if (ret < 0)
		return ret;

	ch->local_window = window_add(ch->local_window, bytes);
	return dssh_transport_send_packet(sess, msg, pos, NULL);
}

static int
dssh_conn_send_exit_status(dssh_session sess,
    dssh_channel ch, uint32_t exit_code)
{
	static const char req[] = "exit-status";
	uint8_t           msg[32];
	size_t            pos = 0;

	int ret;

	msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
	ret = dssh_serialize_uint32(ch->remote_id, msg, sizeof(msg), &pos);
	if (ret < 0)
		return ret;
	ret = dssh_serialize_uint32(sizeof(req) - 1, msg, sizeof(msg), &pos);
	if (ret < 0)
		return ret;
	memcpy(&msg[pos], req, sizeof(req) - 1);
	pos += sizeof(req) - 1;
	msg[pos++] = 0; /* want_reply = false */
	ret = dssh_serialize_uint32(exit_code, msg, sizeof(msg), &pos);
	if (ret < 0)
		return ret;
	return dssh_transport_send_packet(sess, msg, pos, NULL);
}

DSSH_TESTABLE int
dssh_conn_send_eof(dssh_session sess,
    dssh_channel                ch)
{
	if (ch->eof_sent)
		return 0;

	uint8_t msg[8];
	size_t  pos = 0;

	int ret;

	msg[pos++] = SSH_MSG_CHANNEL_EOF;
	ret = dssh_serialize_uint32(ch->remote_id, msg, sizeof(msg), &pos);
	if (ret < 0)
		return ret;

	ch->eof_sent = true;
	return dssh_transport_send_packet(sess, msg, pos, NULL);
}

DSSH_TESTABLE int
dssh_conn_close(dssh_session sess,
    dssh_channel             ch)
{
	if (ch->close_sent)
		return 0;

	uint8_t msg[8];
	size_t  pos = 0;
	int     ret;

	msg[pos++] = SSH_MSG_CHANNEL_CLOSE;
	ret = dssh_serialize_uint32(ch->remote_id, msg, sizeof(msg), &pos);
	if (ret < 0)
		return ret;

	ch->close_sent = true;
	ch->open = false;
	return dssh_transport_send_packet(sess, msg, pos, NULL);
}

/* ================================================================
 * Channel table helpers
 * ================================================================ */

static int
register_channel(dssh_session sess, dssh_channel ch)
{
	mtx_lock(&sess->channel_mtx);
	if (sess->channel_count >= sess->channel_capacity) {
		size_t newcap = sess->channel_capacity ? sess->channel_capacity * 2 : 8;

		if (sess->channel_capacity > SIZE_MAX / 2
		    || newcap > SIZE_MAX / sizeof(struct dssh_channel_s *)) {
			mtx_unlock(&sess->channel_mtx);
			return DSSH_ERROR_ALLOC;
		}
		struct dssh_channel_s **newt = realloc(sess->channels,
		        newcap * sizeof(*newt));

		if (newt == NULL) {
			mtx_unlock(&sess->channel_mtx);
			return DSSH_ERROR_ALLOC;
		}
		sess->channels = newt;
		sess->channel_capacity = newcap;
	}
	sess->channels[sess->channel_count++] = ch;
	mtx_unlock(&sess->channel_mtx);
	return 0;
}

static struct dssh_channel_s *

find_channel(dssh_session sess, uint32_t local_id)
{
	mtx_lock(&sess->channel_mtx);
	for (size_t i = 0; i < sess->channel_count; i++) {
		if (sess->channels[i]->local_id == local_id) {
			dssh_channel ch = sess->channels[i];

			mtx_unlock(&sess->channel_mtx);
			return ch;
		}
	}
	mtx_unlock(&sess->channel_mtx);
	return NULL;
}

static void
unregister_channel(dssh_session sess, dssh_channel ch)
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

static int64_t
alloc_channel_id(dssh_session sess)
{
	mtx_lock(&sess->channel_mtx);

	uint32_t start = sess->next_channel_id;

	for (;;) {
		uint32_t candidate = sess->next_channel_id++;
		bool     in_use = false;

		for (size_t i = 0; i < sess->channel_count; i++) {
			if (sess->channels[i]->local_id == candidate) {
				in_use = true;
				break;
			}
		}
		if (!in_use) {
			mtx_unlock(&sess->channel_mtx);
			return candidate;
		}
		if (sess->next_channel_id == start) {
			mtx_unlock(&sess->channel_mtx);
			return DSSH_ERROR_ALLOC;
		}
	}
}

/* ================================================================
 * Channel buffer initialization
 * ================================================================ */

static int
init_session_channel(dssh_channel ch, uint32_t window_max)
{
	ch->chan_type = DSSH_CHAN_SESSION;
	ch->window_max = window_max;
	ch->stdout_consumed = 0;
	ch->stderr_consumed = 0;
	ch->exit_code = 0;
	ch->exit_code_received = false;

	if (mtx_init(&ch->buf_mtx, mtx_plain) != thrd_success)
		return DSSH_ERROR_INIT;
	if (cnd_init(&ch->poll_cnd) != thrd_success) {
		mtx_destroy(&ch->buf_mtx);
		return DSSH_ERROR_INIT;
	}

	int res = dssh_bytebuf_init(&ch->buf.session.stdout_buf, window_max);

	if (res < 0) {
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->buf_mtx);
		return res;
	}
	res = dssh_bytebuf_init(&ch->buf.session.stderr_buf, window_max);
	if (res < 0) {
		dssh_bytebuf_free(&ch->buf.session.stdout_buf);
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->buf_mtx);
		return res;
	}
	dssh_sigqueue_init(&ch->buf.session.signals);
	return 0;
}

static int
init_raw_channel(dssh_channel ch, uint32_t window_max)
{
	ch->chan_type = DSSH_CHAN_RAW;
	ch->window_max = window_max;

	if (mtx_init(&ch->buf_mtx, mtx_plain) != thrd_success)
		return DSSH_ERROR_INIT;
	if (cnd_init(&ch->poll_cnd) != thrd_success) {
		mtx_destroy(&ch->buf_mtx);
		return DSSH_ERROR_INIT;
	}

	dssh_msgqueue_init(&ch->buf.raw.queue);
	return 0;
}

static void
cleanup_channel_buffers(dssh_channel ch)
{
	if (ch->chan_type == DSSH_CHAN_SESSION) {
		dssh_bytebuf_free(&ch->buf.session.stdout_buf);
		dssh_bytebuf_free(&ch->buf.session.stderr_buf);
		dssh_sigqueue_free(&ch->buf.session.signals);
	}
	else if (ch->chan_type == DSSH_CHAN_RAW) {
		dssh_msgqueue_free(&ch->buf.raw.queue);
	}
	free(ch->setup_payload);
	ch->setup_payload = NULL;

        /* chan_type is always set (SESSION or RAW) before cleanup. */
#ifdef DSSH_TESTING
	cnd_destroy(&ch->poll_cnd);
	mtx_destroy(&ch->buf_mtx);
	ch->chan_type = 0;
#else
	if (ch->chan_type != 0) {
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->buf_mtx);
		ch->chan_type = 0;
	}
#endif
}

/* ================================================================
 * Window replenishment — called after reads drain buffer
 * ================================================================ */

DSSH_TESTABLE int
maybe_replenish_window(dssh_session sess, dssh_channel ch)
{
        /* No point replenishing if peer is done sending */
	if (ch->eof_received || ch->close_received)
		return 0;

        /* Replenish when window drops below half the target */
	if (ch->local_window < ch->window_max / 2) {
		uint32_t add = ch->window_max - ch->local_window;

		return dssh_conn_send_window_adjust(sess, ch, add);
	}
	return 0;
}

/* ================================================================
 * Demux thread
 * ================================================================ */

/*
 * Dispatch a received packet to the appropriate channel buffer.
 * Called from the demux thread with no locks held.
 */
DSSH_TESTABLE int
demux_dispatch(dssh_session sess, uint8_t msg_type,
    uint8_t *payload, size_t payload_len)
{
        /* All channel messages have the recipient channel at payload[1..4] */
	if (payload_len < 5)
		return DSSH_ERROR_PARSE;

	uint32_t local_id;

	if (dssh_parse_uint32(&payload[1], payload_len - 1, &local_id) < 4)
		return DSSH_ERROR_PARSE;

	dssh_channel ch = find_channel(sess, local_id);

	if (ch == NULL)
		return 0; /* late message for closed channel */

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
		return 0;
	}

	if (ch->chan_type == 0) {
		mtx_unlock(&ch->buf_mtx);
		return 0;
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

				if (dssh_parse_uint32(&payload[5], payload_len - 5, &dlen) < 4)
					break;
				if (9 + dlen > payload_len)
					dlen = (uint32_t)(payload_len - 9);

				const uint8_t *data = &payload[9];

				if (ch->chan_type == DSSH_CHAN_SESSION)
					dssh_bytebuf_write(&ch->buf.session.stdout_buf, data, dlen);
				else if (ch->chan_type == DSSH_CHAN_RAW)
					dssh_msgqueue_push(&ch->buf.raw.queue, data, dlen);
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

				if (dssh_parse_uint32(&payload[5], payload_len - 5, &data_type) < 4)
					break;

				uint32_t dlen;

				if (dssh_parse_uint32(&payload[9], payload_len - 9, &dlen) < 4)
					break;
				if (13 + dlen > payload_len)
					dlen = (uint32_t)(payload_len - 13);

				const uint8_t *data = &payload[13];

				if ((ch->chan_type == DSSH_CHAN_SESSION) && (data_type == 1))
					dssh_bytebuf_write(&ch->buf.session.stderr_buf, data, dlen);
				if (dlen <= ch->local_window)
					ch->local_window -= dlen;
				else
					ch->local_window = 0;
			}
			break;

		case SSH_MSG_CHANNEL_WINDOW_ADJUST:
			if (payload_len >= 9) {
				uint32_t bytes;

				if (dssh_parse_uint32(&payload[5], payload_len - 5, &bytes) >= 4)
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
				size_t   rpos = 5;
				uint32_t rtype_len;

				if (dssh_parse_uint32(&payload[rpos], payload_len - rpos, &rtype_len) < 4)
					break;
				rpos += 4;
				if (rpos + rtype_len + 1 > payload_len)
					break;

				const uint8_t *rtype = &payload[rpos];

				rpos += rtype_len;

				bool want_reply = (payload[rpos] != 0);

				rpos++;

				if (ch->chan_type == DSSH_CHAN_SESSION) {
					if ((rtype_len == 6) && (memcmp(rtype, "signal", 6) == 0)) {
                                                /* Queue signal with current stream positions */
						if (rpos + 4 <= payload_len) {
							uint32_t sname_len;

							if (dssh_parse_uint32(&payload[rpos], payload_len - rpos,
							    &sname_len) < 4)
								break;
							rpos += 4;
							if (rpos + sname_len <= payload_len) {
								char   sname[32];
								size_t sn = sname_len
								    < sizeof(sname) - 1 ? sname_len : sizeof(sname) - 1;

								memcpy(sname, &payload[rpos], sn);
								sname[sn] = 0;
								dssh_sigqueue_push(&ch->buf.session.signals,
								    sname,
								    ch->buf.session.stdout_buf.total,
								    ch->buf.session.stderr_buf.total);
							}
						}
					}
					else if ((rtype_len == 11) && (memcmp(rtype, "exit-status", 11) == 0)) {
						if (rpos + 4 <= payload_len) {
							if (dssh_parse_uint32(&payload[rpos],
							    payload_len - rpos,
							    &ch->exit_code) >= 4)
								ch->exit_code_received = true;
						}
					}
					else if ((rtype_len == 13) && (memcmp(rtype, "window-change", 13) == 0)) {
						if ((ch->window_change_cb != NULL) && (rpos + 16 <= payload_len)) {
							uint32_t wc_cols, wc_rows, wc_wpx, wc_hpx;

							if (dssh_parse_uint32(&payload[rpos], payload_len - rpos, &wc_cols) < 4
							    || dssh_parse_uint32(&payload[rpos + 4],
							        payload_len - rpos - 4, &wc_rows) < 4
							    || dssh_parse_uint32(&payload[rpos + 8],
							        payload_len - rpos - 8, &wc_wpx) < 4
							    || dssh_parse_uint32(&payload[rpos + 12],
							        payload_len - rpos - 12, &wc_hpx) < 4)
								break;
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
					size_t  fp = 0;

					fail[fp++] = SSH_MSG_CHANNEL_FAILURE;
					if (dssh_serialize_uint32(ch->remote_id, fail, sizeof(fail), &fp) == 0) {
						mtx_unlock(&ch->buf_mtx);
						dssh_transport_send_packet(sess, fail, fp, NULL);
						mtx_lock(&ch->buf_mtx);
					}
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
	return 0;
}

/*
 * Handle CHANNEL_OPEN_CONFIRMATION — recipient_channel is at offset 1.
 */
static int
demux_open_confirmation(dssh_session sess,
    uint8_t *payload, size_t payload_len)
{
	if (payload_len < 1 + 16)
		return DSSH_ERROR_PARSE;

	uint32_t local_id;

	if (dssh_parse_uint32(&payload[1], payload_len - 1, &local_id) < 4)
		return DSSH_ERROR_PARSE;

	dssh_channel ch = find_channel(sess, local_id);

	if (ch == NULL)
		return 0; /* late message for closed channel */

	mtx_lock(&ch->buf_mtx);
	if (dssh_parse_uint32(&payload[5], payload_len - 5, &ch->remote_id) < 4
	    || dssh_parse_uint32(&payload[9], payload_len - 9, &ch->remote_window) < 4
	    || dssh_parse_uint32(&payload[13], payload_len - 13, &ch->remote_max_packet) < 4) {
		mtx_unlock(&ch->buf_mtx);
		return DSSH_ERROR_PARSE;
	}
	ch->open = true;
	cnd_signal(&ch->poll_cnd);
	mtx_unlock(&ch->buf_mtx);
	return 0;
}

/*
 * Handle incoming CHANNEL_OPEN — either queue for accept or auto-reject.
 */
static int
demux_channel_open(dssh_session sess, uint8_t *payload, size_t payload_len)
{
        /* Parse: string channel_type, uint32 sender_channel,
         * uint32 initial_window, uint32 max_packet */
	size_t   rpos = 1;
	uint32_t type_len;

	if (rpos + 4 > payload_len)
		return DSSH_ERROR_PARSE;
	if (dssh_parse_uint32(&payload[rpos], payload_len - rpos, &type_len) < 4)
		return DSSH_ERROR_PARSE;
	rpos += 4;
	if (rpos + type_len > payload_len)
		return DSSH_ERROR_PARSE;

	const uint8_t *ctype = &payload[rpos];

	rpos += type_len;

	if (rpos + 12 > payload_len)
		return DSSH_ERROR_PARSE;

	uint32_t peer_channel, peer_window, peer_max_packet;

	if (dssh_parse_uint32(&payload[rpos], payload_len - rpos, &peer_channel) < 4)
		return DSSH_ERROR_PARSE;
	rpos += 4;
	if (dssh_parse_uint32(&payload[rpos], payload_len - rpos, &peer_window) < 4)
		return DSSH_ERROR_PARSE;
	rpos += 4;
	if (dssh_parse_uint32(&payload[rpos], payload_len - rpos, &peer_max_packet) < 4)
		return DSSH_ERROR_PARSE;

        /* Auto-reject forbidden channel types */
	bool reject = false;

	if ((type_len == 3) && (memcmp(ctype, "x11", 3) == 0))
		reject = true;
	if ((type_len == 15) && (memcmp(ctype, "forwarded-tcpip", 15) == 0))
		reject = true;
	if ((type_len == 12) && (memcmp(ctype, "direct-tcpip", 12) == 0))
		reject = true;

        /* Client rejects "session" opens from server (RFC 4254 s6.1) */
	if (sess->trans.client && (type_len == 7) && (memcmp(ctype, "session", 7) == 0))
		reject = true;

	if (reject) {
                /* Send CHANNEL_OPEN_FAILURE */
		uint8_t fail[64];
		size_t  fp = 0;

		fail[fp++] = SSH_MSG_CHANNEL_OPEN_FAILURE;
		if (dssh_serialize_uint32(peer_channel, fail, sizeof(fail), &fp) < 0
		    || dssh_serialize_uint32(SSH_OPEN_ADMINISTRATIVELY_PROHIBITED, fail, sizeof(fail), &fp) < 0
		    || dssh_serialize_uint32(0, fail, sizeof(fail), &fp) < 0
		    || dssh_serialize_uint32(0, fail, sizeof(fail), &fp) < 0)
			return DSSH_ERROR_INIT;
		dssh_transport_send_packet(sess, fail, fp, NULL);
		return 0;
	}

        /* Queue for application's accept() call */
	mtx_lock(&sess->accept_mtx);
	dssh_acceptqueue_push(&sess->accept_queue,
	    peer_channel, peer_window, peer_max_packet, ctype, type_len);
	cnd_signal(&sess->accept_cnd);
	mtx_unlock(&sess->accept_mtx);
	return 0;
}

static int
demux_thread_func(void *arg)
{
	dssh_session sess = arg;
	uint8_t      msg_type;
	uint8_t     *payload;
	size_t       payload_len;

	while (!sess->terminate) {
		int res = dssh_transport_recv_packet(sess,
		        &msg_type, &payload, &payload_len);

		if (res < 0) {
			if (res == DSSH_ERROR_TERMINATED)
				break;

                        /* Other errors — terminate */
			dssh_session_set_terminate(sess);
			break;
		}

		int dres = 0;

		if (msg_type == SSH_MSG_CHANNEL_OPEN)
			dres = demux_channel_open(sess, payload, payload_len);
		else if (msg_type == SSH_MSG_CHANNEL_OPEN_CONFIRMATION)
			dres = demux_open_confirmation(sess, payload, payload_len);
		else if ((msg_type >= SSH_MSG_CHANNEL_OPEN_FAILURE)
		    && (msg_type <= SSH_MSG_CHANNEL_FAILURE))
			dres = demux_dispatch(sess, msg_type, payload, payload_len);

		/* Parse errors from malformed peer messages are
		 * non-fatal — skip the packet.  I/O and serialize
		 * errors mean the connection is broken. */
		if (dres < 0 && dres != DSSH_ERROR_PARSE) {
			dssh_session_set_terminate(sess);
			break;
		}
                /* All other message types (GLOBAL_REQUEST, KEXINIT, etc.)
                 * are already handled by recv_packet transparently */
	}

        /* Wake any waiters so they see termination */
	mtx_lock(&sess->accept_mtx);
	cnd_broadcast(&sess->accept_cnd);
	mtx_unlock(&sess->accept_mtx);

        /* During termination wake, all registered channels have
         * initialized buffers (chan_type is always set). */
	mtx_lock(&sess->channel_mtx);
	for (size_t i = 0; i < sess->channel_count; i++) {
		dssh_channel ch = sess->channels[i];
#ifdef DSSH_TESTING
		mtx_lock(&ch->buf_mtx);
		cnd_signal(&ch->poll_cnd);
		mtx_unlock(&ch->buf_mtx);
#else
		if (ch->chan_type != 0) {
			mtx_lock(&ch->buf_mtx);
			cnd_signal(&ch->poll_cnd);
			mtx_unlock(&ch->buf_mtx);
		}
#endif
	}
	mtx_unlock(&sess->channel_mtx);

	sess->demux_running = false;
	return 0;
}

DSSH_PUBLIC int
dssh_session_start(dssh_session sess)
{
	if (sess->demux_running)
		return DSSH_ERROR_INIT;

	if (mtx_init(&sess->channel_mtx, mtx_plain) != thrd_success)
		return DSSH_ERROR_INIT;
	if (mtx_init(&sess->accept_mtx, mtx_plain) != thrd_success) {
		mtx_destroy(&sess->channel_mtx);
		return DSSH_ERROR_INIT;
	}
	if (cnd_init(&sess->accept_cnd) != thrd_success) {
		mtx_destroy(&sess->accept_mtx);
		mtx_destroy(&sess->channel_mtx);
		return DSSH_ERROR_INIT;
	}

	sess->channels = NULL;
	sess->channel_count = 0;
	sess->channel_capacity = 0;
	sess->next_channel_id = 0;
	dssh_acceptqueue_init(&sess->accept_queue);
	sess->conn_initialized = true;

	sess->demux_running = true;
	if (thrd_create(&sess->demux_thread, demux_thread_func, sess) != thrd_success) {
		sess->demux_running = false;
		cnd_destroy(&sess->accept_cnd);
		mtx_destroy(&sess->accept_mtx);
		mtx_destroy(&sess->channel_mtx);
		return DSSH_ERROR_INIT;
	}

	return 0;
}

DSSH_PUBLIC void
dssh_session_stop(dssh_session sess)
{
	if (!sess->demux_running && (sess->channels == NULL))
		return;

        /* Signal termination and join the demux thread */
	if (sess->demux_running) {
		dssh_session_set_terminate(sess);

		int demux_res;

		thrd_join(sess->demux_thread, &demux_res);
	}

        /* Clean up registered channels */
	if (sess->channels != NULL) {
                /* Channels in the array are never NULL; registration
                 * always stores a valid pointer. */
		for (size_t i = 0; i < sess->channel_count; i++) {
			dssh_channel ch = sess->channels[i];

			if (ch != NULL) {
				cleanup_channel_buffers(ch);
				free(ch);
			}
		}
		free(sess->channels);
		sess->channels = NULL;
		sess->channel_count = 0;
		sess->channel_capacity = 0;
	}

	dssh_acceptqueue_free(&sess->accept_queue);
	cnd_destroy(&sess->accept_cnd);
	mtx_destroy(&sess->accept_mtx);
	mtx_destroy(&sess->channel_mtx);
}

/* ================================================================
 * Session accept — wait for incoming CHANNEL_OPEN
 * ================================================================ */

DSSH_PUBLIC int
dssh_session_accept(dssh_session sess,
    struct dssh_incoming_open **inc, int timeout_ms)
{
	mtx_lock(&sess->accept_mtx);

	while (sess->accept_queue.head == NULL) {
		if (sess->terminate || !sess->demux_running) {
			mtx_unlock(&sess->accept_mtx);
			return DSSH_ERROR_TERMINATED;
		}
		if (timeout_ms == 0) {
			mtx_unlock(&sess->accept_mtx);
			return DSSH_ERROR_NOMORE;
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
				return DSSH_ERROR_NOMORE;
			}
		}
	}

	*inc = dssh_acceptqueue_pop(&sess->accept_queue);
	mtx_unlock(&sess->accept_mtx);
	return 0;
}

DSSH_PUBLIC int
dssh_session_reject(dssh_session sess,
    struct dssh_incoming_open *inc, uint32_t reason_code,
    const char *description)
{
	size_t  dlen = description ? strlen(description) : 0;
	uint8_t msg[256];

	if (dlen > sizeof(msg) - 17) /* msg_type + 3×uint32 + language uint32 */
		dlen = sizeof(msg) - 17;

	size_t pos = 0;
	int    ret;

	msg[pos++] = SSH_MSG_CHANNEL_OPEN_FAILURE;
	ret = dssh_serialize_uint32(inc->peer_channel, msg, sizeof(msg), &pos);
	if (ret < 0)
		goto done;
	ret = dssh_serialize_uint32(reason_code, msg, sizeof(msg), &pos);
	if (ret < 0)
		goto done;
	ret = dssh_serialize_uint32((uint32_t)dlen, msg, sizeof(msg), &pos);
	if (ret < 0)
		goto done;
	if (dlen > 0) {
		if (dlen > sizeof(msg) - pos - 4)
			dlen = sizeof(msg) - pos - 4;
		memcpy(&msg[pos], description, dlen);
		pos += dlen;
	}
	ret = dssh_serialize_uint32(0, msg, sizeof(msg), &pos);
	if (ret < 0)
		goto done;
	ret = dssh_transport_send_packet(sess, msg, pos, NULL);
done:
	free(inc);
	return ret;
}

/* ================================================================
 * High-level channel open helpers
 * ================================================================ */

/*
 * Send CHANNEL_OPEN_CONFIRMATION for an incoming channel.
 */
static int
send_open_confirmation(dssh_session sess,
    dssh_channel ch, uint32_t peer_channel)
{
	uint8_t msg[32];
	size_t  pos = 0;

	int ret;

	msg[pos++] = SSH_MSG_CHANNEL_OPEN_CONFIRMATION;
	ret = dssh_serialize_uint32(peer_channel, msg, sizeof(msg), &pos);
	if (ret < 0)
		return ret;
	ret = dssh_serialize_uint32(ch->local_id, msg, sizeof(msg), &pos);
	if (ret < 0)
		return ret;
	ret = dssh_serialize_uint32(ch->local_window, msg, sizeof(msg), &pos);
	if (ret < 0)
		return ret;
	ret = dssh_serialize_uint32(MAX_PACKET_SIZE, msg, sizeof(msg), &pos);
	if (ret < 0)
		return ret;
	return dssh_transport_send_packet(sess, msg, pos, NULL);
}

/*
 * Common channel open: send CHANNEL_OPEN "session", wait for confirmation.
 * Does NOT start the demux — the channel is registered for demux dispatch.
 */
static int
open_session_channel(dssh_session sess, dssh_channel ch)
{
        /* Caller has already initialized buffers (init_session_channel
         * or init_raw_channel).  Set wire-level fields only. */
	int64_t cid = alloc_channel_id(sess);
	if (cid < 0)
		return (int)cid;
	ch->local_id = (uint32_t)cid;
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
	uint8_t           msg[256];
	size_t            pos = 0;

	int ret;

	msg[pos++] = SSH_MSG_CHANNEL_OPEN;
	ret = dssh_serialize_uint32(DSSH_STRLEN(type), msg, sizeof(msg), &pos);
	if (ret < 0)
		return ret;
	memcpy(&msg[pos], type, sizeof(type) - 1);
	pos += sizeof(type) - 1;
	ret = dssh_serialize_uint32(ch->local_id, msg, sizeof(msg), &pos);
	if (ret < 0)
		return ret;
	ret = dssh_serialize_uint32(ch->local_window, msg, sizeof(msg), &pos);
	if (ret < 0)
		return ret;
	ret = dssh_serialize_uint32(MAX_PACKET_SIZE, msg, sizeof(msg), &pos);
	if (ret < 0)
		return ret;

	int res = register_channel(sess, ch);

	if (res < 0)
		return res;

	res = dssh_transport_send_packet(sess, msg, pos, NULL);
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
		return DSSH_ERROR_INIT;
	}
	return 0;
}

/*
 * Send a CHANNEL_REQUEST with want_reply=true and wait for the demux
 * thread to deliver SUCCESS or FAILURE.  Returns 0 on SUCCESS,
 * DSSH_ERROR_INIT on FAILURE.
 */
static int
send_channel_request_wait(dssh_session sess,
    dssh_channel ch,
    const char *req_type, const uint8_t *extra, size_t extra_len)
{
	size_t   rtlen = strlen(req_type);

	if (rtlen > UINT32_MAX)
		return DSSH_ERROR_INVALID;
	if (extra_len > SIZE_MAX - 10 - rtlen)
		return DSSH_ERROR_INVALID;

	size_t   msg_len = 10 + rtlen + extra_len;
	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;
	int    ret;

	msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
	ret = dssh_serialize_uint32(ch->remote_id, msg, msg_len, &pos);
	if (ret < 0) {
		free(msg);
		return ret;
	}
	ret = dssh_serialize_uint32((uint32_t)rtlen, msg, msg_len, &pos);
	if (ret < 0) {
		free(msg);
		return ret;
	}
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

	int res = dssh_transport_send_packet(sess, msg, pos, NULL);

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
		return DSSH_ERROR_TERMINATED;
	return success ? 0 : DSSH_ERROR_INIT;
}

/* ================================================================
 * High-level open functions
 * ================================================================ */

DSSH_PUBLIC dssh_channel
dssh_session_open_shell(dssh_session sess,
    const struct dssh_pty_req       *pty)
{
	dssh_channel ch = calloc(1, sizeof(*ch));

	if (ch == NULL)
		return NULL;

	int res = init_session_channel(ch, INITIAL_WINDOW_SIZE);

	if (res < 0) {
		free(ch);
		return NULL;
	}

	res = open_session_channel(sess, ch);
	if (res < 0) {
		cleanup_channel_buffers(ch);
		free(ch);
		return NULL;
	}

        /* Send pty-req */
	{
		size_t   tlen = strlen(pty->term);
		size_t   mlen = pty->modes_len;

		if (tlen > UINT32_MAX || mlen > UINT32_MAX) {
			dssh_conn_close(sess, ch);
			unregister_channel(sess, ch);
			cleanup_channel_buffers(ch);
			free(ch);
			return NULL;
		}

		if (tlen > SIZE_MAX - 24 || mlen > SIZE_MAX - 24 - tlen) {
			dssh_conn_close(sess, ch);
			unregister_channel(sess, ch);
			cleanup_channel_buffers(ch);
			free(ch);
			return NULL;
		}
		size_t   extra_len = 24 + tlen + mlen;
		uint8_t *extra = malloc(extra_len);

		if (extra == NULL) {
			dssh_conn_close(sess, ch);
			unregister_channel(sess, ch);
			cleanup_channel_buffers(ch);
			free(ch);
			return NULL;
		}

		size_t ep = 0;

#define PTY_SER(v) do { res = dssh_serialize_uint32((v), extra, extra_len, &ep); \
	if (res < 0) { free(extra); goto open_fail; } } while (0)

		PTY_SER((uint32_t)tlen);
		memcpy(&extra[ep], pty->term, tlen);
		ep += tlen;
		PTY_SER(pty->cols);
		PTY_SER(pty->rows);
		PTY_SER(pty->wpx);
		PTY_SER(pty->hpx);
		PTY_SER((uint32_t)mlen);

#undef PTY_SER

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

	return ch;

open_fail:
	dssh_conn_close(sess, ch);
	unregister_channel(sess, ch);
	cleanup_channel_buffers(ch);
	free(ch);
	return NULL;
}

DSSH_PUBLIC dssh_channel
dssh_session_open_exec(dssh_session sess,
    const char                     *command)
{
	dssh_channel ch = calloc(1, sizeof(*ch));

	if (ch == NULL)
		return NULL;

	int res = init_session_channel(ch, INITIAL_WINDOW_SIZE);

	if (res < 0) {
		free(ch);
		return NULL;
	}

	res = open_session_channel(sess, ch);
	if (res < 0) {
		cleanup_channel_buffers(ch);
		free(ch);
		return NULL;
	}

        /* Send exec */
	size_t   cmdlen = strlen(command);

	if (cmdlen > UINT32_MAX || cmdlen > SIZE_MAX - 4) {
		res = DSSH_ERROR_INVALID;
		goto open_fail;
	}

	size_t   extra_len = 4 + cmdlen;
	uint8_t *extra = malloc(extra_len);

	if (extra == NULL) {
		res = DSSH_ERROR_ALLOC;
		goto open_fail;
	}

	size_t ep = 0;

	res = dssh_serialize_uint32((uint32_t)cmdlen, extra, extra_len, &ep);
	if (res < 0) {
		free(extra);
		goto open_fail;
	}
	memcpy(&extra[ep], command, cmdlen);
	ep += cmdlen;
	res = send_channel_request_wait(sess, ch, "exec", extra, ep);
	free(extra);
	if (res < 0)
		goto open_fail;

	return ch;

open_fail:
	dssh_conn_close(sess, ch);
	unregister_channel(sess, ch);
	cleanup_channel_buffers(ch);
	free(ch);
	return NULL;
}

DSSH_PUBLIC dssh_channel
dssh_channel_open_subsystem(dssh_session sess,
    const char                          *subsystem)
{
	dssh_channel ch = calloc(1, sizeof(*ch));

	if (ch == NULL)
		return NULL;

	int res = init_raw_channel(ch, INITIAL_WINDOW_SIZE);

	if (res < 0) {
		free(ch);
		return NULL;
	}

	res = open_session_channel(sess, ch);
	if (res < 0) {
		cleanup_channel_buffers(ch);
		free(ch);
		return NULL;
	}

        /* Send subsystem */
	size_t   slen = strlen(subsystem);

	if (slen > UINT32_MAX || slen > SIZE_MAX - 4) {
		res = DSSH_ERROR_INVALID;
		goto open_fail;
	}

	size_t   extra_len = 4 + slen;
	uint8_t *extra = malloc(extra_len);

	if (extra == NULL) {
		res = DSSH_ERROR_ALLOC;
		goto open_fail;
	}

	size_t ep = 0;

	res = dssh_serialize_uint32((uint32_t)slen, extra, extra_len, &ep);
	if (res < 0) {
		free(extra);
		goto open_fail;
	}
	memcpy(&extra[ep], subsystem, slen);
	ep += slen;
	res = send_channel_request_wait(sess, ch, "subsystem", extra, ep);
	free(extra);
	if (res < 0)
		goto open_fail;

	return ch;

open_fail:
	dssh_conn_close(sess, ch);
	unregister_channel(sess, ch);
	cleanup_channel_buffers(ch);
	free(ch);
	return NULL;
}

/* ================================================================
 * Server-side accept helpers
 * ================================================================ */

DSSH_PUBLIC dssh_channel
dssh_channel_accept_raw(dssh_session sess,
    struct dssh_incoming_open       *inc)
{
	dssh_channel ch = calloc(1, sizeof(*ch));

	if (ch == NULL)
		return NULL;

	int64_t cid = alloc_channel_id(sess);
	if (cid < 0) {
		free(ch);
		return NULL;
	}
	ch->local_id = (uint32_t)cid;
	ch->local_window = INITIAL_WINDOW_SIZE;
	ch->remote_id = inc->peer_channel;
	ch->remote_window = inc->peer_window;
	ch->remote_max_packet = inc->peer_max_packet;

	int res = init_raw_channel(ch, INITIAL_WINDOW_SIZE);

	if (res < 0) {
		free(ch);
		return NULL;
	}

	res = register_channel(sess, ch);
	if (res < 0) {
		cleanup_channel_buffers(ch);
		free(ch);
		return NULL;
	}

	res = send_open_confirmation(sess, ch, inc->peer_channel);
	free(inc);
	if (res < 0) {
		unregister_channel(sess, ch);
		cleanup_channel_buffers(ch);
		free(ch);
		return NULL;
	}

	ch->open = true;
	return ch;
}

/* ================================================================
 * Server-side session accept (callback-driven setup)
 * ================================================================ */

/*
 * Wait for the next message in the setup mailbox.
 * Returns 0 on success, negative on error/termination.
 */
static int
setup_recv(dssh_session sess, dssh_channel ch,
    uint8_t *msg_type, uint8_t **payload, size_t *payload_len)
{
	mtx_lock(&ch->buf_mtx);
	while (!ch->setup_ready && !sess->terminate && !ch->close_received)
		cnd_wait(&ch->poll_cnd, &ch->buf_mtx);

	if (!ch->setup_ready) {
		mtx_unlock(&ch->buf_mtx);
		return DSSH_ERROR_TERMINATED;
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
setup_reply(dssh_session sess, dssh_channel ch,
    bool success)
{
	uint8_t msg[8];
	size_t  pos = 0;

	int ret;

	msg[pos++] = success ? SSH_MSG_CHANNEL_SUCCESS : SSH_MSG_CHANNEL_FAILURE;
	ret = dssh_serialize_uint32(ch->remote_id, msg, sizeof(msg), &pos);
	if (ret < 0)
		return ret;
	return dssh_transport_send_packet(sess, msg, pos, NULL);
}

DSSH_PUBLIC dssh_channel
dssh_session_accept_channel(dssh_session sess,
    struct dssh_incoming_open *inc,
    const struct dssh_server_session_cbs *cbs,
    const char **request_type, const char **request_data)
{
	dssh_channel ch = calloc(1, sizeof(*ch));

	if (ch == NULL)
		return NULL;
	int64_t cid = alloc_channel_id(sess);
	if (cid < 0) {
		free(ch);
		return NULL;
	}
	ch->local_id = (uint32_t)cid;
	ch->local_window = INITIAL_WINDOW_SIZE;
	ch->remote_id = inc->peer_channel;
	ch->remote_window = inc->peer_window;
	ch->remote_max_packet = inc->peer_max_packet;

        /* Set up mailbox for receiving setup messages from demux */
	if (mtx_init(&ch->buf_mtx, mtx_plain) != thrd_success) {
		free(ch);
		return NULL;
	}
	if (cnd_init(&ch->poll_cnd) != thrd_success) {
		mtx_destroy(&ch->buf_mtx);
		free(ch);
		return NULL;
	}
	ch->setup_mode = true;

        /* Register so the demux thread can deliver messages to us */
	int res = register_channel(sess, ch);

	if (res < 0) {
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->buf_mtx);
		free(ch);
		return NULL;
	}

        /* Send CHANNEL_OPEN_CONFIRMATION */
	res = send_open_confirmation(sess, ch, inc->peer_channel);
	free(inc);
	if (res < 0) {
		unregister_channel(sess, ch);
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->buf_mtx);
		free(ch);
		return NULL;
	}
	ch->open = true;

        /* Process setup requests until a terminal request
         * (shell/exec/subsystem) is accepted or rejected. */
	ch->req_type[0] = 0;
	ch->req_data[0] = 0;

	bool is_subsystem = false;

	for (;;) {
		uint8_t  msg_type;
		uint8_t *payload;
		size_t   payload_len;

		res = setup_recv(sess, ch, &msg_type, &payload, &payload_len);
		if (res < 0)
			goto fail;

		if (msg_type != SSH_MSG_CHANNEL_REQUEST) {
                        /* Handle WINDOW_ADJUST during setup */
			if ((msg_type == SSH_MSG_CHANNEL_WINDOW_ADJUST) && (payload_len >= 9)) {
				uint32_t bytes;

				if (dssh_parse_uint32(&payload[5], payload_len - 5, &bytes) >= 4)
					ch->remote_window = window_add(ch->remote_window, bytes);
			}
			free(payload);
			continue;
		}

                /* Parse CHANNEL_REQUEST framing:
                 * byte    msg_type (already consumed by demux as [0])
                 * uint32  recipient_channel ([1..4])
                 * string  request_type
                 * boolean want_reply
                 * ...     type-specific data */
		size_t   rpos = 5;
		uint32_t rtype_len;

		if (rpos + 4 > payload_len) {
			free(payload);
			continue;
		}
		if (dssh_parse_uint32(&payload[rpos], payload_len - rpos, &rtype_len) < 4) {
			free(payload);
			continue;
		}
		rpos += 4;
		if (rpos + rtype_len + 1 > payload_len) {
			free(payload);
			continue;
		}

		const char *rtype = (const char *)&payload[rpos];

		rpos += rtype_len;

		bool want_reply = (payload[rpos] != 0);

		rpos++;

                /* Type-specific data starts at rpos */
		const uint8_t *req_data = &payload[rpos];
		size_t         req_data_len = payload_len - rpos;

                /* Call the application callback */
		int            cb_res = 0;

		if (cbs->request_cb != NULL) {
			cb_res = cbs->request_cb(rtype, rtype_len,
			        want_reply, req_data, req_data_len,
			        cbs->cbdata);
		}

                /* Is this a terminal request (shell/exec/subsystem)? */
		bool is_terminal =
		    (rtype_len == 5 && memcmp(rtype, "shell", 5) == 0)
		    || (rtype_len == 4 && memcmp(rtype, "exec", 4) == 0)
		    || (rtype_len == 9 && memcmp(rtype, "subsystem", 9) == 0);

		if (want_reply)
			setup_reply(sess, ch, cb_res >= 0);

		if (is_terminal && (cb_res < 0)) {
                        /* App rejected the terminal request — close channel */
			free(payload);
			dssh_conn_close(sess, ch);
			goto fail;
		}

		if (is_terminal && (cb_res >= 0)) {
                        /* Accepted — save type and data, determine channel kind */
			size_t tn = rtype_len < sizeof(ch->req_type) - 1
			    ? rtype_len : sizeof(ch->req_type) - 1;

			memcpy(ch->req_type, rtype, tn);
			ch->req_type[tn] = 0;

                        /* Extract command/subsystem name for convenience */
			if (req_data_len >= 4) {
				uint32_t dlen;

				if (dssh_parse_uint32(req_data, req_data_len, &dlen) < 4)
					dlen = 0;

				size_t dn = dlen < sizeof(ch->req_data) - 1
				    ? dlen : sizeof(ch->req_data) - 1;

				if (4 + dn <= req_data_len)
					memcpy(ch->req_data, &req_data[4], dn);
				ch->req_data[dn] = 0;
			}

			is_subsystem = (rtype_len == 9
			    && memcmp(rtype, "subsystem", 9) == 0);
			free(payload);
			break;
		}

		free(payload);
	}

        /* Transition from setup mode to normal buffered mode.
         * buf_mtx and poll_cnd are already initialized from setup.
         * Channel type depends on the terminal request:
         *   shell/exec → DSSH_CHAN_SESSION (stream-based bytebufs)
         *   subsystem  → DSSH_CHAN_RAW    (message-based msgqueue)
         */
	ch->setup_mode = false;
	ch->window_max = INITIAL_WINDOW_SIZE;

	if (is_subsystem) {
		ch->chan_type = DSSH_CHAN_RAW;
		dssh_msgqueue_init(&ch->buf.raw.queue);
	}
	else {
		ch->chan_type = DSSH_CHAN_SESSION;
		ch->stdout_consumed = 0;
		ch->stderr_consumed = 0;
		ch->exit_code = 0;
		ch->exit_code_received = false;
		res = dssh_bytebuf_init(&ch->buf.session.stdout_buf,
		        INITIAL_WINDOW_SIZE);
		if (res < 0)
			goto fail;
		res = dssh_bytebuf_init(&ch->buf.session.stderr_buf,
		        INITIAL_WINDOW_SIZE);
		if (res < 0) {
			dssh_bytebuf_free(&ch->buf.session.stdout_buf);
			goto fail;
		}
		dssh_sigqueue_init(&ch->buf.session.signals);
	}

        /* Install window-change callback for post-setup delivery */
	if (cbs->window_change != NULL) {
		ch->window_change_cb = cbs->window_change;
		ch->window_change_cbdata = cbs->cbdata;
	}

	if (request_type != NULL)
		*request_type = ch->req_type;
	if (request_data != NULL)
		*request_data = ch->req_data;
	return ch;

fail:
	ch->setup_mode = false;
	unregister_channel(sess, ch);
	cnd_destroy(&ch->poll_cnd);
	mtx_destroy(&ch->buf_mtx);
	free(ch);
	return NULL;
}

/* ================================================================
 * Poll / Read / Write — Session channels (stream-based)
 * ================================================================ */

/*
 * Compute how many bytes are readable on stdout before hitting
 * a signal mark.
 */
static size_t
session_stdout_readable(dssh_channel ch)
{
	size_t avail = dssh_bytebuf_available(&ch->buf.session.stdout_buf);

	if (avail == 0)
		return 0;

	struct dssh_signal_queue *sq = &ch->buf.session.signals;

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
session_stderr_readable(dssh_channel ch)
{
	size_t avail = dssh_bytebuf_available(&ch->buf.session.stderr_buf);

	if (avail == 0)
		return 0;

	struct dssh_signal_queue *sq = &ch->buf.session.signals;

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

DSSH_PUBLIC int
dssh_session_poll(dssh_session sess,
    dssh_channel ch, int events, int timeout_ms)
{
	mtx_lock(&ch->buf_mtx);

	for (;;) {
		int ready = 0;

		if ((events & DSSH_POLL_READ)
		    && ((session_stdout_readable(ch) > 0) || ch->eof_received || ch->close_received))
			ready |= DSSH_POLL_READ;

		if ((events & DSSH_POLL_READEXT)
		    && ((session_stderr_readable(ch) > 0) || ch->eof_received || ch->close_received))
			ready |= DSSH_POLL_READEXT;

		if ((events & DSSH_POLL_WRITE) && (ch->remote_window > 0) && ch->open && !ch->close_received)
			ready |= DSSH_POLL_WRITE;

		if ((events & DSSH_POLL_SIGNAL)
		    && dssh_sigqueue_ready(&ch->buf.session.signals,
		    ch->stdout_consumed, ch->stderr_consumed))
			ready |= DSSH_POLL_SIGNAL;

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

DSSH_PUBLIC int64_t
dssh_session_read(dssh_session sess,
    dssh_channel ch, uint8_t *buf, size_t bufsz)
{
	mtx_lock(&ch->buf_mtx);

	size_t limit = session_stdout_readable(ch);
	size_t n = dssh_bytebuf_read(&ch->buf.session.stdout_buf, buf, bufsz, limit);

	ch->stdout_consumed += n;
	mtx_unlock(&ch->buf_mtx);

	if (n > 0) {
		int wret = maybe_replenish_window(sess, ch);
		if (wret < 0)
			return wret;
	}
	return (int64_t)n;
}

DSSH_PUBLIC int64_t
dssh_session_read_ext(dssh_session sess,
    dssh_channel ch, uint8_t *buf, size_t bufsz)
{
	mtx_lock(&ch->buf_mtx);

	size_t limit = session_stderr_readable(ch);
	size_t n = dssh_bytebuf_read(&ch->buf.session.stderr_buf, buf, bufsz, limit);

	ch->stderr_consumed += n;
	mtx_unlock(&ch->buf_mtx);

	if (n > 0) {
		int wret = maybe_replenish_window(sess, ch);
		if (wret < 0)
			return wret;
	}
	return (int64_t)n;
}

DSSH_PUBLIC int64_t
dssh_session_write(dssh_session sess,
    dssh_channel ch, const uint8_t *buf, size_t bufsz)
{
	if (!ch->open || ch->eof_sent || ch->close_received)
		return DSSH_ERROR_INIT;

	uint32_t avail = ch->remote_window;

	if (avail == 0)
		return 0;

	size_t len = bufsz;

	if (len > avail)
		len = avail;
	if (len > ch->remote_max_packet)
		len = ch->remote_max_packet;

	int res = dssh_conn_send_data(sess, ch, buf, len);

	if (res < 0)
		return res;
	return (int64_t)len;
}

DSSH_PUBLIC int64_t
dssh_session_write_ext(dssh_session sess,
    dssh_channel ch, const uint8_t *buf, size_t bufsz)
{
	if (!ch->open || ch->eof_sent || ch->close_received)
		return DSSH_ERROR_INIT;

	uint32_t avail = ch->remote_window;

	if (avail == 0)
		return 0;

	size_t len = bufsz;

	if (len > avail)
		len = avail;
	if (len > ch->remote_max_packet)
		len = ch->remote_max_packet;

	int res = dssh_conn_send_extended_data(sess, ch,
	        SSH_EXTENDED_DATA_STDERR, buf, len);

	if (res < 0)
		return res;
	return (int64_t)len;
}

DSSH_PUBLIC int
dssh_session_read_signal(dssh_session sess,
    dssh_channel ch, const char **signal_name)
{
	mtx_lock(&ch->buf_mtx);

	const char *name = dssh_sigqueue_pop(&ch->buf.session.signals,
	        ch->stdout_consumed, ch->stderr_consumed,
	        ch->last_signal, sizeof(ch->last_signal));

	mtx_unlock(&ch->buf_mtx);

	if (name == NULL)
		return DSSH_ERROR_NOMORE;
	*signal_name = ch->last_signal;
	return 0;
}

DSSH_PUBLIC int
dssh_session_close(dssh_session sess,
    dssh_channel ch, uint32_t exit_code)
{
	dssh_conn_send_exit_status(sess, ch, exit_code);
	dssh_conn_send_eof(sess, ch);
	dssh_conn_close(sess, ch);
	unregister_channel(sess, ch);
	cleanup_channel_buffers(ch);
	free(ch);
	return 0;
}

/* ================================================================
 * Poll / Read / Write — Raw channels (message-based)
 * ================================================================ */

DSSH_PUBLIC int
dssh_channel_poll(dssh_session sess,
    dssh_channel ch, int events, int timeout_ms)
{
	mtx_lock(&ch->buf_mtx);

	for (;;) {
		int ready = 0;

		if ((events & DSSH_POLL_READ)
		    && ((ch->buf.raw.queue.count > 0) || ch->eof_received || ch->close_received))
			ready |= DSSH_POLL_READ;

		if ((events & DSSH_POLL_WRITE) && (ch->remote_window > 0) && ch->open && !ch->close_received)
			ready |= DSSH_POLL_WRITE;

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

DSSH_PUBLIC int64_t
dssh_channel_read(dssh_session sess,
    dssh_channel ch, uint8_t *buf, size_t bufsz)
{
	mtx_lock(&ch->buf_mtx);

	int64_t n = dssh_msgqueue_pop(&ch->buf.raw.queue, buf, bufsz);

	mtx_unlock(&ch->buf_mtx);

	if (n > 0) {
		int wret = maybe_replenish_window(sess, ch);
		if (wret < 0)
			return wret;
	}
	return n;
}

DSSH_PUBLIC int
dssh_channel_write(dssh_session sess,
    dssh_channel ch, const uint8_t *buf, size_t len)
{
	if (!ch->open || ch->eof_sent || ch->close_received)
		return DSSH_ERROR_INIT;
	if ((len > ch->remote_window) || (len > ch->remote_max_packet))
		return DSSH_ERROR_TOOLONG;

	return dssh_conn_send_data(sess, ch, buf, len);
}

DSSH_PUBLIC int
dssh_channel_close(dssh_session sess,
    dssh_channel                ch)
{
	dssh_conn_send_eof(sess, ch);
	dssh_conn_close(sess, ch);
	unregister_channel(sess, ch);
	cleanup_channel_buffers(ch);
	free(ch);
	return 0;
}

/* ================================================================
 * Window change (client-side)
 * ================================================================ */

DSSH_PUBLIC int
dssh_session_send_signal(dssh_session sess,
    dssh_channel ch, const char *signal_name)
{
	size_t   slen = strlen(signal_name);

	if (slen > UINT32_MAX || slen > SIZE_MAX - 20)
		return DSSH_ERROR_INVALID;

	size_t   rtlen = 6; /* "signal" */
	size_t   msg_len = 20 + slen; /* 1 + 4 + 4 + 6 + 1 + 4 */
	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;
	int    ret;

#define SIG_SER(v) do { ret = dssh_serialize_uint32((v), msg, msg_len, &pos); \
	if (ret < 0) { free(msg); return ret; } } while (0)

	msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
	SIG_SER(ch->remote_id);
	SIG_SER((uint32_t)rtlen);
	memcpy(&msg[pos], "signal", rtlen);
	pos += rtlen;
	msg[pos++] = 0; /* want_reply = FALSE */
	SIG_SER((uint32_t)slen);
	memcpy(&msg[pos], signal_name, slen);
	pos += slen;

#undef SIG_SER

	ret = dssh_transport_send_packet(sess, msg, pos, NULL);

	free(msg);
	return ret;
}

DSSH_PUBLIC int
dssh_session_send_window_change(dssh_session sess,
    dssh_channel ch,
    uint32_t cols, uint32_t rows, uint32_t wpx, uint32_t hpx)
{
	uint8_t extra[16];
	size_t  ep = 0;
	int     ret;

	ret = dssh_serialize_uint32(cols, extra, sizeof(extra), &ep);
	if (ret < 0)
		return ret;
	ret = dssh_serialize_uint32(rows, extra, sizeof(extra), &ep);
	if (ret < 0)
		return ret;
	ret = dssh_serialize_uint32(wpx, extra, sizeof(extra), &ep);
	if (ret < 0)
		return ret;
	ret = dssh_serialize_uint32(hpx, extra, sizeof(extra), &ep);
	if (ret < 0)
		return ret;

        /* window-change uses want_reply=false */
	size_t   rtlen = 13; /* "window-change" */
	size_t   msg_len = 1 + 4 + 4 + rtlen + 1 + ep;
	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;

	msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
	ret = dssh_serialize_uint32(ch->remote_id, msg, msg_len, &pos);
	if (ret < 0)
		goto wc_done;
	ret = dssh_serialize_uint32((uint32_t)rtlen, msg, msg_len, &pos);
	if (ret < 0)
		goto wc_done;
	memcpy(&msg[pos], "window-change", rtlen);
	pos += rtlen;
	msg[pos++] = 0; /* want_reply = FALSE */
	memcpy(&msg[pos], extra, ep);
	pos += ep;

	ret = dssh_transport_send_packet(sess, msg, pos, NULL);

wc_done:
	free(msg);
	return ret;
}
