#include <stdlib.h>

#include "deucessh.h"
#include "ssh-conn.h"

DEUCE_SSH_PUBLIC int
deuce_ssh_session_init(deuce_ssh_session sess, size_t max_packet_size)
{
	int res = mtx_init(&sess->mtx, mtx_plain);
	if (res != thrd_success)
		return DEUCE_SSH_ERROR_INIT;

	res = deuce_ssh_transport_init(sess, max_packet_size);
	if (res < 0)
		return res;

	sess->demux_running = false;
	sess->channels = NULL;
	sess->channel_count = 0;
	sess->channel_capacity = 0;
	sess->next_channel_id = 0;

	sess->initialized = true;
	return 0;
}

DEUCE_SSH_PUBLIC bool
deuce_ssh_session_terminate(deuce_ssh_session sess)
{
	bool t = true;
	if (atomic_compare_exchange_strong(&sess->initialized, &t, false)) {
		sess->terminate = true;
		return true;
	}
	return false;
}

DEUCE_SSH_PUBLIC void
deuce_ssh_session_cleanup(deuce_ssh_session sess)
{
	deuce_ssh_session_terminate(sess);
	deuce_ssh_session_stop(sess);
	deuce_ssh_transport_cleanup(sess);
	mtx_destroy(&sess->mtx);
}
