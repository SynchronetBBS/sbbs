/*
 * ssh-internal.h — Internal struct definitions.
 * Not a public header.  Included by library .c files only.
 */

#ifndef DSSH_INTERNAL_H
#define DSSH_INTERNAL_H

#include <stdatomic.h>
#include <threads.h>

#include "deucessh.h"
#include "ssh-trans.h"
#include "ssh-chan.h"

/* Channel types */
#define DSSH_CHAN_SESSION 1
#define DSSH_CHAN_RAW     2

/* SSH connection protocol message types (RFC 4254) */
#define SSH_MSG_GLOBAL_REQUEST            UINT8_C(80)
#define SSH_MSG_REQUEST_SUCCESS           UINT8_C(81)
#define SSH_MSG_REQUEST_FAILURE           UINT8_C(82)
#define SSH_MSG_CHANNEL_OPEN              UINT8_C(90)
#define SSH_MSG_CHANNEL_OPEN_CONFIRMATION UINT8_C(91)
#define SSH_MSG_CHANNEL_OPEN_FAILURE      UINT8_C(92)
#define SSH_MSG_CHANNEL_WINDOW_ADJUST     UINT8_C(93)
#define SSH_MSG_CHANNEL_DATA              UINT8_C(94)
#define SSH_MSG_CHANNEL_EXTENDED_DATA     UINT8_C(95)
#define SSH_MSG_CHANNEL_EOF               UINT8_C(96)
#define SSH_MSG_CHANNEL_CLOSE             UINT8_C(97)
#define SSH_MSG_CHANNEL_REQUEST           UINT8_C(98)
#define SSH_MSG_CHANNEL_SUCCESS           UINT8_C(99)
#define SSH_MSG_CHANNEL_FAILURE           UINT8_C(100)

#define SSH_OPEN_ADMINISTRATIVELY_PROHIBITED UINT32_C(1)

struct dssh_channel_s {
	/* Wire-level fields */
	uint32_t local_id;
	uint32_t remote_id;
	uint32_t local_window;
	uint32_t remote_window;
	uint32_t remote_max_packet;
	bool open;
	bool close_sent;
	bool close_received;
	bool eof_sent;
	bool eof_received;

	/* Buffered mode fields (chan_type > 0) */
	int chan_type;
	mtx_t buf_mtx;
	cnd_t poll_cnd;

	/* Consumed byte counters (for signal mark tracking) */
	size_t stdout_consumed;
	size_t stderr_consumed;

	/* Window replenishment threshold */
	uint32_t window_max;

	/* Exit status from peer (session channels) */
	uint32_t exit_code;
	bool exit_code_received;

	/* Channel request response (for send_channel_request_wait) */
	bool request_pending;
	bool request_responded;
	bool request_success;

	/* Setup mailbox (for session_accept_channel) */
	bool setup_mode;
	bool setup_ready;
	uint8_t setup_msg_type;
	uint8_t *setup_payload;
	size_t setup_payload_len;

	/* Window-change callback (server-side, post-setup) */
	void (*window_change_cb)(uint32_t cols, uint32_t rows,
	    uint32_t wpx, uint32_t hpx, void *cbdata);
	void *window_change_cbdata;

	/* Per-channel string buffers (avoids static storage) */
	char last_signal[32];
	char req_type[32];
	char req_data[1024];

	union {
		struct {
			struct dssh_bytebuf stdout_buf;
			struct dssh_bytebuf stderr_buf;
			struct dssh_signal_queue signals;
		} session;
		struct {
			struct dssh_msgqueue queue;
		} raw;
	} buf;
};

struct dssh_session_s {
	mtx_t mtx;
	atomic_bool initialized;
	atomic_bool terminate;

	/* Per-session I/O callback data (passed to the global I/O callbacks) */
	void *tx_cbdata;
	void *rx_cbdata;
	void *rx_line_cbdata;
	void *extra_line_cbdata;

	/* Optional notification callbacks */
	dssh_debug_cb debug_cb;
	void *debug_cbdata;
	dssh_unimplemented_cb unimplemented_cb;
	void *unimplemented_cbdata;

	/* Auth banner callback (set before authentication) */
	void *banner_cb;
	void *banner_cbdata;

	/*
	 * Optional global request callback (RFC 4254 s4).
	 * If set, called for every SSH_MSG_GLOBAL_REQUEST.
	 * Return 0 for REQUEST_SUCCESS, negative for REQUEST_FAILURE.
	 * If NULL, REQUEST_FAILURE is sent automatically.
	 */
	void *global_request_cb;
	void *global_request_cbdata;

	/* Auth state */
	bool auth_service_requested;

	/* Transport layer state */
	struct dssh_transport_state_s trans;

	/* Demux thread and channel table */
	thrd_t demux_thread;
	atomic_bool demux_running;

	struct dssh_channel_s **channels;
	size_t channel_count;
	size_t channel_capacity;
	mtx_t channel_mtx;

	/* Incoming channel open queue */
	struct dssh_accept_queue accept_queue;
	mtx_t accept_mtx;
	cnd_t accept_cnd;

	/* Next auto-assigned local channel ID */
	uint32_t next_channel_id;
};

#endif
