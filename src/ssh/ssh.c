#include <openssl/crypto.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh-conn.h"
#include "ssh-internal.h"

/*
 * Set the terminate flag and wake all library-owned condvar waiters.
 * This ensures that any thread blocked on rekey_cnd, accept_cnd,
 * or a channel poll_cnd sees termination promptly instead of
 * blocking until an external event (like a socket close) wakes it.
 */
DSSH_PRIVATE void
dssh_session_set_terminate(dssh_session sess)
{
	if (atomic_exchange(&sess->terminate, true))
		return;

	/* Notify the application so it can close sockets or signal
	 * its event loop, unblocking any I/O callbacks. */
	if (sess->terminate_cb)
		sess->terminate_cb(sess, sess->terminate_cbdata);

        /* Wake senders blocked during rekey.
         * Hold tx_mtx so the broadcast cannot land between a sender's
         * while-condition check and cnd_wait entry.  Use trylock because
         * send_packet() calls set_terminate on fatal errors while already
         * holding tx_mtx -- in that case the lock is already held so the
         * broadcast is safe without re-acquiring.
         *
         * All lock/broadcast/unlock operations here are best-effort.
         * dssh_thrd_check will not recurse because the atomic_exchange
         * above ensures this function runs at most once. */
	int tr = mtx_trylock(&sess->trans.tx_mtx);
	if (tr == thrd_success) {
		dssh_thrd_check(sess, cnd_broadcast(&sess->trans.rekey_cnd));
		dssh_thrd_check(sess, mtx_unlock(&sess->trans.tx_mtx));
	}
	else if (tr == thrd_busy) {
		/* tx_mtx held by our caller -- broadcast is still safe
		 * because the caller's critical section prevents any
		 * sender from entering cnd_wait. */
		dssh_thrd_check(sess, cnd_broadcast(&sess->trans.rekey_cnd));
	}
	/* thrd_error: mutex corrupted, skip broadcast */

        /* Wake conn-layer waiters if initialized */
	if (sess->conn_initialized) {
		if (dssh_thrd_check(sess, mtx_lock(&sess->accept_mtx))
		    == thrd_success) {
			dssh_thrd_check(sess,
			    cnd_broadcast(&sess->accept_cnd));
			dssh_thrd_check(sess,
			    mtx_unlock(&sess->accept_mtx));
		}

		/* Lock order: channel_mtx then buf_mtx. */
		if (dssh_thrd_check(sess, mtx_lock(&sess->channel_mtx))
		    == thrd_success) {
			for (size_t i = 0; i < sess->channel_count; i++) {
				dssh_channel ch = sess->channels[i];

				if (dssh_thrd_check(sess,
				    mtx_lock(&ch->buf_mtx))
				    == thrd_success) {
					dssh_thrd_check(sess,
					    cnd_broadcast(&ch->poll_cnd));
					dssh_thrd_check(sess,
					    mtx_unlock(&ch->buf_mtx));
				}
			}
			dssh_thrd_check(sess,
			    mtx_unlock(&sess->channel_mtx));
		}
	}
}

DSSH_PUBLIC dssh_session
dssh_session_init(bool client, size_t max_packet_size)
{
	dssh_session sess = calloc(1, sizeof(*sess));

	if (sess == NULL)
		return NULL;

	sess->trans.client = client;

	int res = mtx_init(&sess->mtx, mtx_plain);

	if (res != thrd_success) {
		free(sess);
		return NULL;
	}

	res = dssh_transport_init(sess, max_packet_size);
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
dssh_session_terminate(dssh_session sess)
{
	if (sess == NULL)
		return false;

	bool t = true;

	if (atomic_compare_exchange_strong(&sess->initialized, &t, false)) {
		dssh_session_set_terminate(sess);
		return true;
	}
	return false;
}

DSSH_PUBLIC bool
dssh_session_is_terminated(dssh_session sess)
{
	if (sess == NULL)
		return true;
	return sess->terminate;
}

DSSH_PUBLIC void
dssh_session_cleanup(dssh_session sess)
{
	if (sess == NULL)
		return;
	dssh_session_terminate(sess);
	dssh_session_stop(sess);
	dssh_transport_cleanup(sess);
	free(sess->pending_banner);
	free(sess->pending_banner_lang);
	mtx_destroy(&sess->mtx);
	free(sess);
}

DSSH_PUBLIC void
dssh_cleanse(void *buf, size_t len)
{
	if (buf != NULL && len > 0)
		OPENSSL_cleanse(buf, len);
}

DSSH_PUBLIC void
dssh_session_set_cbdata(dssh_session sess,
    void *tx_cbdata, void *rx_cbdata, void *rx_line_cbdata,
    void *extra_line_cbdata)
{
	if (sess == NULL)
		return;
	sess->tx_cbdata = tx_cbdata;
	sess->rx_cbdata = rx_cbdata;
	sess->rx_line_cbdata = rx_line_cbdata;
	sess->extra_line_cbdata = extra_line_cbdata;
}

/* Callback setters below must be called before dssh_session_start().
 * The thrd_create in dssh_session_start() provides the C11
 * happens-before guarantee that makes these writes visible to the
 * demux thread.  Calling after start is undefined behavior. */

DSSH_PUBLIC void
dssh_session_set_debug_cb(dssh_session sess,
    dssh_debug_cb cb, void *cbdata)
{
	if (sess == NULL)
		return;
	sess->debug_cb = cb;
	sess->debug_cbdata = cbdata;
}

DSSH_PUBLIC void
dssh_session_set_unimplemented_cb(dssh_session sess,
    dssh_unimplemented_cb cb, void *cbdata)
{
	if (sess == NULL)
		return;
	sess->unimplemented_cb = cb;
	sess->unimplemented_cbdata = cbdata;
}

DSSH_PUBLIC void
dssh_session_set_banner_cb(dssh_session sess,
    dssh_auth_banner_cb cb, void *cbdata)
{
	if (sess == NULL)
		return;
	sess->banner_cb = cb;
	sess->banner_cbdata = cbdata;
}

DSSH_PUBLIC void
dssh_session_set_global_request_cb(dssh_session sess,
    dssh_global_request_cb cb, void *cbdata)
{
	if (sess == NULL)
		return;
	sess->global_request_cb = cb;
	sess->global_request_cbdata = cbdata;
}

DSSH_PUBLIC void
dssh_session_set_terminate_cb(dssh_session sess,
    dssh_terminate_cb cb, void *cbdata)
{
	if (sess == NULL)
		return;
	sess->terminate_cb = cb;
	sess->terminate_cbdata = cbdata;
}

DSSH_PUBLIC void
dssh_session_set_timeout(dssh_session sess, int timeout_ms)
{
	if (sess == NULL)
		return;
	sess->timeout_ms = timeout_ms;
}
