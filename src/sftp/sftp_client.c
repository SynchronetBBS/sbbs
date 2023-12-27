#include <assert.h>
#include <genwrap.h>
#include <stdlib.h>
#include <threadwrap.h>

#include "sftp.h"

void
sftpc_finish(sftpc_state_t state)
{
	assert(state);
	if (state == NULL)
		return;
	assert(state->thread == pthread_self());
	// TODO: Close all open handles
	while (!CloseEvent(state->recv_event)) {
		assert(errno == EBUSY);
		if (errno != EBUSY)
			break;
		SetEvent(state->recv_event);
		SLEEP(1);
	}
	sftp_free_rx_pkt(state->rxp);
	sftp_free_tx_pkt(state->txp);
	free(state);
}

sftpc_state_t
sftpc_begin(bool (*send_cb)(uint8_t *buf, size_t len, void *cb_data), void *cb_data)
{
	sftpc_state_t ret = (sftpc_state_t)malloc(sizeof(struct sftp_client_state));
	if (ret == NULL)
		return NULL;
	ret->recv_event = CreateEvent(NULL, TRUE, FALSE, "sftp_recv");
	if (ret->recv_event == NULL) {
		free(ret);
		return NULL;
	}
	ret->rxp = NULL;
	ret->txp = NULL;
	ret->send_cb = send_cb;
	ret->cb_data = cb_data;
	ret->thread = pthread_self();
	return ret;
}

bool
sftpc_init(sftpc_state_t state)
{
	assert(state);
	if (!state)
		goto fail;
	assert(state->thread == pthread_self());
	if (state->thread != pthread_self())
		goto fail;
	if (!sftp_appendbyte(&state->txp, SSH_FXP_INIT))
		goto fail;
	if (!sftp_append32(&state->txp, SFTP_VERSION))
		goto fail;
	uint8_t *txbuf;
	size_t txsz;
	if (!sftp_prep_tx_packet(state->txp, &txbuf, &txsz))
		goto fail;
	if (!state->send_cb(txbuf, txsz, state->cb_data))
		goto fail;
	sftp_tx_pkt_reset(&state->txp);
	if (WaitForEvent(state->recv_event, INFINITE) != WAIT_OBJECT_0)
		goto fail;
	if (state->rxp->type != SSH_FXP_VERSION)
		goto fail;
	if (sftp_get32(state->rxp) != SFTP_VERSION)
		goto fail;
	sftp_remove_packet(state->rxp);
	if (!sftp_have_full_pkt(state->rxp))
		ResetEvent(state->recv_event);
	return true;
fail:
	sftp_tx_pkt_reset(&state->txp);
	return false;
}

bool
sftpc_recv(sftpc_state_t state, uint8_t *buf, uint32_t sz)
{
	if (!sftp_rx_pkt_append(&state->rxp, buf, sz))
		return false;
	if (sftp_have_full_pkt(state->rxp))
		SetEvent(state->recv_event);
	return true;
}
