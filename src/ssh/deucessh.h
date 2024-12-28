#ifndef DEUCE_SSH_H
#define DEUCE_SSH_H

#include <assert.h>

#ifdef __STDC_NO_ATOMICS__
static_assert(0, "stdatomic.h support required");
#endif

#ifdef __STDC_NO_THREADS__
static_assert(0, "threads.h support required");
#endif

#include <inttypes.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <threads.h>

#define DEUCE_SSH_ERROR_NONE           0
#define DEUCE_SSH_ERROR_PARSE         -1
#define DEUCE_SSH_ERROR_INVALID       -2
#define DEUCE_SSH_ERROR_ALLOC         -3
#define DEUCE_SSH_ERROR_INIT          -4
#define DEUCE_SSH_ERROR_TERMINATED    -5
#define DEUCE_SSH_ERROR_TOOLATE       -6
#define DEUCE_SSH_ERROR_TOOMANY       -7
#define DEUCE_SSH_ERROR_TOOLONG       -8
#define DEUCE_SSH_ERROR_MUST_BE_NULL  -9
#define DEUCE_SSH_ERROR_NOMORE       -10

typedef struct deuce_ssh_transport_state *deuce_ssh_transport_state_t;
typedef int (*deuce_ssh_transport_io_cb_t)(uint8_t *buf, size_t bufsz, atomic_bool *terminate, void *cbdata);
typedef int (*deuce_ssh_transport_rxline_cb_t)(uint8_t *buf, size_t bufsz, size_t *bytes_received, atomic_bool *terminate, void *cbdata);
typedef int (*deuce_ssh_transport_extra_line_cb_t)(uint8_t *buf, size_t bufsz, void *cbdata);

typedef struct deuce_ssh_session *deuce_ssh_session_t;

#include "ssh-arch.h"
#include "ssh-trans.h"

struct deuce_ssh_session {
	/* Global */
	mtx_t mtx;
	atomic_bool initialized;
	atomic_bool terminate;
	bool is_server;

	/* Transport Remote information */
	size_t remote_software_id_string_sz;
	char remote_software_id_string[254];
	char **remote_languages;

	void *tx_cbdata;
	void *rx_cbdata;
	void *rx_line_cbdata;
	void *extra_line_cbdata;

	struct deuce_ssh_transport_state trans;
};

int deuce_ssh_session_init(deuce_ssh_session_t sess);
bool deuce_ssh_session_terminate(deuce_ssh_session_t sess);
void deuce_ssh_session_cleanup(deuce_ssh_session_t sess);

int deuce_ssh_transport_set_callbacks(deuce_ssh_transport_io_cb_t tx, deuce_ssh_transport_io_cb_t rx, deuce_ssh_transport_rxline_cb_t rx_line, deuce_ssh_transport_extra_line_cb_t extra_line_cb);

#endif
