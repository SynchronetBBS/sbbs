#include <stdlib.h>
#include <string.h>

#include "ssh-internal.h"
#include "deucessh-conn.h"

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
	bool t = true;
	if (atomic_compare_exchange_strong(&sess->initialized, &t, false)) {
		sess->terminate = true;
		return true;
	}
	return false;
}

DSSH_PUBLIC bool
dssh_session_is_terminated(dssh_session sess)
{
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
	sess->tx_cbdata = tx_cbdata;
	sess->rx_cbdata = rx_cbdata;
	sess->rx_line_cbdata = rx_line_cbdata;
	sess->extra_line_cbdata = extra_line_cbdata;
}

DSSH_PUBLIC void
dssh_session_set_debug_cb(dssh_session sess,
    dssh_debug_cb cb, void *cbdata)
{
	sess->debug_cb = cb;
	sess->debug_cbdata = cbdata;
}

DSSH_PUBLIC void
dssh_session_set_unimplemented_cb(dssh_session sess,
    dssh_unimplemented_cb cb, void *cbdata)
{
	sess->unimplemented_cb = cb;
	sess->unimplemented_cbdata = cbdata;
}

DSSH_PUBLIC void
dssh_session_set_banner_cb(dssh_session sess,
    void *cb, void *cbdata)
{
	sess->banner_cb = cb;
	sess->banner_cbdata = cbdata;
}

DSSH_PUBLIC void
dssh_session_set_global_request_cb(dssh_session sess,
    void *cb, void *cbdata)
{
	sess->global_request_cb = cb;
	sess->global_request_cbdata = cbdata;
}
