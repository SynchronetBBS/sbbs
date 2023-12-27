#include <assert.h>
#include <genwrap.h>
#include <stdlib.h>
#include <threadwrap.h>

#include "sftp.h"

static uint32_t
get32(sftpc_state_t state)
{
	return sftp_get32(state->rxp);
}

static uint64_t
get64(sftpc_state_t state)
{
	return sftp_get64(state->rxp);
}

static sftp_str_t
getstring(sftpc_state_t state)
{
	return sftp_getstring(state->rxp);
}

static bool
appendbyte(sftpc_state_t state, uint8_t u)
{
	return sftp_appendbyte(&state->txp, u);
}

static bool
append32(sftpc_state_t state, uint32_t u)
{
	return sftp_append32(&state->txp, u);
}

static bool
append64(sftpc_state_t state, uint64_t u)
{
	return sftp_append64(&state->txp, u);
}

static bool
appendstring(sftpc_state_t state, sftp_str_t *s)
{
	bool ret = sftp_appendstring(&state->txp, *s);
	free_sftp_str(*s);
	*s = NULL;
	return ret;
}

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
	ret->id = 0;
	ret->err_lang = NULL;
	ret->err_msg = NULL;
	return ret;
}

static void
response_handled(sftpc_state_t state)
{
	sftp_remove_packet(state->rxp);
	if (!sftp_have_full_pkt(state->rxp))
		ResetEvent(state->recv_event);
}

static bool
check_state(sftpc_state_t state)
{
	assert(state);
	if (!state)
		return false;
	assert(state->thread == pthread_self());
	if (state->thread != pthread_self())
		return false;
	return true;
}

static bool
appendheader(sftpc_state_t state, uint8_t type)
{
	if (!check_state(state))
		return false;
	state->err_code = 0;
	state->err_id = 0;
	free_sftp_str(state->err_lang);
	state->err_lang = NULL;
	free_sftp_str(state->err_msg);
	state->err_msg = NULL;
	if (!sftp_tx_pkt_reset(&state->txp))
		return false;
	if (!sftp_appendbyte(&state->txp, type))
		return false;
	if (type != SSH_FXP_INIT) {
		if (!sftp_append32(&state->txp, ++state->id))
			return false;
	}
	return true;
}

static bool
get_result(sftpc_state_t state)
{
	uint8_t *txbuf;
	size_t txsz;

	if (!sftp_prep_tx_packet(state->txp, &txbuf, &txsz))
		return false;
	if (!state->send_cb(txbuf, txsz, state->cb_data))
		return false;
	if (WaitForEvent(state->recv_event, INFINITE) != WAIT_OBJECT_0)
		return false;
	if (state->rxp->type != SSH_FXP_VERSION) {
		uint32_t id = sftp_get32(state->rxp);
		if (id != state->id) {
			response_handled(state);
			return false;
		}
	}
	return true;
}

static void
handle_error(sftpc_state_t state)
{
	if (state->rxp->type == SSH_FXP_STATUS) {
		state->err_id = get32(state);
		state->err_code = get32(state);
		if (state->err_msg != NULL)
			free_sftp_str(state->err_msg);
		state->err_msg = getstring(state);
		if (state->err_lang != NULL)
			free_sftp_str(state->err_lang);
		state->err_lang = getstring(state);
	}
	response_handled(state);
}

bool
sftpc_init(sftpc_state_t state)
{
	if (!appendheader(state, SSH_FXP_INIT))
		return false;
	if (!append32(state, SFTP_VERSION))
		return false;
	if (!get_result(state))
		return false;
	if (state->rxp->type != SSH_FXP_VERSION)
		return false;
	if (get32(state) != SFTP_VERSION) {
		response_handled(state);
		return false;
	}
	response_handled(state);
	state->id = 0;
	return true;
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

bool
sftpc_realpath(sftpc_state_t state, char *path, sftp_str_t *ret)
{
	assert(ret);
	if (ret == NULL)
		return false;
	if (*ret != NULL)
		return false;
	if (!appendheader(state, SSH_FXP_REALPATH))
		return false;
	sftp_str_t pstr = sftp_strdup(path);
	if (!appendstring(state, &pstr))
		return false;
	if (!get_result(state))
		return false;
	if (state->rxp->type == SSH_FXP_NAME) {
		if (get32(state) != 1) {
			response_handled(state);
			return false;
		}
		*ret = getstring(state);
		response_handled(state);
		return true;
	}
	handle_error(state);
	return false;
}
