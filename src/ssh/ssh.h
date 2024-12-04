#ifndef DEUCE_SSH_H
#define DEUCE_SSH_H

#ifdef __STDC_NO_ATOMICS__
_Static_assert(0, "stdatomic.h support required");
#endif

#ifdef __STDC_NO_THREADS__
_Static_assert(0, "threads.h support required");
#endif

#include <inttypes.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <threads.h>

#define DEUCE_SSH_ERROR_NONE        0
#define DEUCE_SSH_ERROR_PARSE      -1
#define DEUCE_SSH_ERROR_INVALID    -2
#define DEUCE_SSH_ERROR_ALLOC      -3
#define DEUCE_SSH_ERROR_INIT       -4
#define DEUCE_SSH_ERROR_TERMINATED -5

typedef struct deuce_ssh_session {
	/* Global */
	mtx_t mtx;
	atomic_bool initialized;
	atomic_bool terminate;

	/* Transport options */
	const char *software_version;
	const char *version_comment;
	void *tx_cbdata;
	int (*tx)(uint8_t *buf, size_t bufsz, atomic_bool *terminate, void *cbdata);
	void *rx_cbdata;
	int (*rx)(uint8_t *buf, size_t bufsz, atomic_bool *terminate, void *cbdata);
	void *rx_line_cbdata;
	int (*rx_line)(uint8_t *buf, size_t bufsz, size_t *bytes_received, atomic_bool *terminate, void *cbdata);
	void *extra_line_cbdata;
	int (*extra_line_cb)(uint8_t *buf, size_t bufsz, void *cbdata);
	thrd_t transport_thread;

	/* Transport Remote information */
	char *remote_software_version;
	char *remote_version_comment;
} *deuce_ssh_session_t;

int deuce_ssh_session_init(deuce_ssh_session_t sess);
bool deuce_ssh_session_terminate(deuce_ssh_session_t sess);
void deuce_ssh_session_cleanup(deuce_ssh_session_t sess);

#endif
