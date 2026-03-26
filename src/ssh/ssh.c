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
	sess->terminate = true;

        /* Wake senders blocked during rekey */
	cnd_broadcast(&sess->trans.rekey_cnd);

        /* Wake conn-layer waiters if initialized */
	if (sess->conn_initialized) {
		mtx_lock(&sess->accept_mtx);
		cnd_broadcast(&sess->accept_cnd);
		mtx_unlock(&sess->accept_mtx);

		mtx_lock(&sess->channel_mtx);
		for (size_t i = 0; i < sess->channel_count; i++) {
			dssh_channel ch = sess->channels[i];

			if (ch->chan_type != 0) {
				mtx_lock(&ch->buf_mtx);
				cnd_signal(&ch->poll_cnd);
				mtx_unlock(&ch->buf_mtx);
			}
		}
		mtx_unlock(&sess->channel_mtx);
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
	mtx_destroy(&sess->mtx);
	free(sess);
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
