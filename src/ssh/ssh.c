#include "ssh.h"
#include "ssh-trans.h"

int
deuce_ssh_session_init(deuce_ssh_session_t sess)
{
	int res = mtx_init(&sess->mtx, mtx_plain);
	if (res != thrd_success)
		return DEUCE_SSH_ERROR_INIT;

	res = deuce_ssh_transport_init(sess);
	if (res < 0) {
		return res;
	}

	sess->initialized = true;
	return 0;
}

bool
deuce_ssh_session_terminate(deuce_ssh_session_t sess)
{
	bool t = true;
	if (atomic_compare_exchange_strong(&sess->initialized, &t, false)) {
		sess->terminate = true;
		int tres;
		thrd_join(sess->transport_thread, &tres);
		sess->terminate = false;
		return true;
	}
	return false;
}

void
deuce_ssh_session_cleanup(deuce_ssh_session_t sess)
{
	deuce_ssh_transport_cleanup(sess);
	deuce_ssh_session_terminate(sess);
	mtx_destroy(&sess->mtx);
}
