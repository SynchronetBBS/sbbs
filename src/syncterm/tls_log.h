#ifndef SYNCTERM_TLS_LOG_H
#define SYNCTERM_TLS_LOG_H

#include "xp_tls.h"

/* Attach SyncTERM's unified protocol-log adapter to an xptls client. */
void syncterm_tls_log_configure(struct xp_tls_client_config *config,
	int level);

#endif
