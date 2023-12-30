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
appendfattr(sftpc_state_t state, sftp_file_attr_t fattr)
{
	return sftp_appendfattr(&state->txp, fattr);
}

static bool
appendandfreestring(sftpc_state_t state, sftp_str_t *s)
{
	bool ret = sftp_appendstring(&state->txp, *s);
	free_sftp_str(*s);
	*s = NULL;
	return ret;
}

static bool
appendstring(sftpc_state_t state, sftp_str_t s)
{
	bool ret = sftp_appendstring(&state->txp, s);
	return ret;
}

void
sftpc_finish(sftpc_state_t state)
{
	assert(state);
	if (state == NULL)
		return;
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
	return true;
}

static bool
appendheader(sftpc_state_t state, uint8_t type)
{
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
appendfhandle(sftpc_state_t state, sftp_filehandle_t handle)
{
	return appendstring(state, (sftp_str_t)handle);
}

static bool
appenddhandle(sftpc_state_t state, sftp_dirhandle_t handle)
{
	return appendstring(state, (sftp_str_t)handle);
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
	if (state->rxp == NULL)
		return false;
	if (state->rxp->type != SSH_FXP_VERSION) {
		uint32_t id = sftp_get32(state->rxp);
		if (id != state->id) {
			free(state->rxp);
			state->rxp = NULL;
			return false;
		}
	}
	return true;
}

static void
handle_error(sftpc_state_t state)
{
	if (state->rxp->type == SSH_FXP_STATUS) {
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
	if (!check_state(state))
		return false;
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
	if (!check_state(state))
		return false;
	if (!sftp_rx_pkt_append(&state->rxp, buf, sz))
		return false;
	if (sftp_have_full_pkt(state->rxp))
		SetEvent(state->recv_event);
	return true;
}

bool
sftpc_realpath(sftpc_state_t state, char *path, sftp_str_t *ret)
{
	if (!check_state(state))
		return false;
	assert(ret);
	if (ret == NULL)
		return false;
	if (*ret != NULL)
		return false;
	if (!appendheader(state, SSH_FXP_REALPATH))
		return false;
	sftp_str_t pstr = sftp_strdup(path);
	if (!appendandfreestring(state, &pstr))
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

static bool
parse_handle(sftpc_state_t state, sftp_str_t *handle)
{
	assert(state->rxp);
	if (state->rxp == NULL)
		return false;
	assert(state->rxp->type == SSH_FXP_HANDLE);
	if (state->rxp->type != SSH_FXP_HANDLE)
		return false;
	assert(handle);
	if (handle == NULL)
		return false;
	*handle = getstring(state);
	if (*handle == NULL)
		return false;
	return true;
}

bool
sftpc_open(sftpc_state_t state, char *path, uint32_t flags, sftp_file_attr_t attr, sftp_dirhandle_t *handle)
{
	if (!check_state(state))
		return false;
	assert(path);
	if (path == NULL)
		return false;
	assert(handle);
	if (handle == NULL)
		return false;
	assert(!*handle);
	if (*handle != NULL)
		return false;
	if (!appendheader(state, SSH_FXP_OPEN))
		return false;
	sftp_str_t pstr = sftp_strdup(path);
	if (!appendandfreestring(state, &pstr))
		return false;
	if (!append32(state, flags))
		return false;
	sftp_file_attr_t a = attr;
	if (a == NULL) {
		a = sftp_fattr_alloc();
		if (a == NULL)
			return false;
	}
	if (!appendfattr(state, a)) {
		if (a != attr)
			sftp_fattr_free(a);
		return false;
	}
	if (a != attr)
		sftp_fattr_free(a);
	if (!get_result(state))
		return false;
	if (state->rxp->type == SSH_FXP_HANDLE) {
		if (!parse_handle(state, (sftp_str_t *)handle)) {
			response_handled(state);
			return false;
		}
		response_handled(state);
		return true;
	}
	handle_error(state);
	return false;
}

bool
sftpc_close(sftpc_state_t state, sftp_filehandle_t *handle)
{
	if (!check_state(state))
		return false;
	if (!appendheader(state, SSH_FXP_CLOSE))
		return false;
	if (!appendfhandle(state, *handle))
		return false;
	if (!get_result(state))
		return false;
	handle_error(state);
	free_sftp_str(*handle);
	*handle = NULL;
	return state->err_code == SSH_FX_OK;
}

bool
sftpc_read(sftpc_state_t state, sftp_filehandle_t handle, uint64_t offset, uint32_t len, sftp_str_t *ret)
{
	if (!check_state(state))
		return false;
	assert(ret);
	if (ret == NULL)
		return false;
	assert(*ret == NULL);
	if (*ret != NULL)
		return false;
	if (!appendheader(state, SSH_FXP_READ))
		return false;
	if (!appendfhandle(state, handle))
		return false;
	if (!append64(state, offset))
		return false;
	if (!append32(state, len))
		return false;
	if (!get_result(state))
		return false;
	if (state->rxp->type == SSH_FXP_DATA) {
		*ret = getstring(state);
		response_handled(state);
		return *ret != NULL;
	}
	handle_error(state);
	return false;
}

bool
sftpc_write(sftpc_state_t state, sftp_filehandle_t handle, uint64_t offset, sftp_str_t data)
{
	if (!check_state(state))
		return false;
	assert(data);
	if (data == NULL)
		return false;
	if (!appendheader(state, SSH_FXP_WRITE))
		return false;
	if (!appendfhandle(state, handle))
		return false;
	if (!append64(state, offset))
		return false;
	if (!appendstring(state, data))
		return false;
	if (!get_result(state))
		return false;
	handle_error(state);
	return state->err_code == SSH_FX_OK;
}
