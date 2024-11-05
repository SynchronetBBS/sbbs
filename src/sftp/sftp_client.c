#include <assert.h>
#include <genwrap.h>
#include <stdlib.h>
#include <threadwrap.h>

#include "sftp.h"

struct sftp_client_state {
	bool (*send_cb)(uint8_t *buf, size_t len, void *cb_data);
	xpevent_t recv_event;
	sftp_rx_pkt_t rxp;
	sftp_tx_pkt_t txp;
	void *cb_data;
	sftp_str_t err_msg;
	sftp_str_t err_lang;
	pthread_mutex_t mtx;
	uint32_t version;
	uint32_t running;
	uint32_t id;
	uint32_t err_id;
	uint32_t err_code;
	bool terminating;
};

#define SFTP_STATIC_TYPE sftpc_state_t
#include "sftp_static.h"
#undef SFTP_STATIC_TYPE

static bool
append64(sftpc_state_t state, uint64_t u)
{
	return sftp_append64(&state->txp, u);
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
appendfhandle(sftpc_state_t state, sftp_filehandle_t handle)
{
	return appendstring(state, (sftp_str_t)handle);
}

#ifdef NOTYET
static bool
appenddhandle(sftpc_state_t state, sftp_dirhandle_t handle)
{
	return appendstring(state, (sftp_str_t)handle);
}
#endif

static bool
cappendheader(sftpc_state_t state, uint8_t type)
{
	state->err_code = 0;
	state->err_id = 0;
	free_sftp_str(state->err_lang);
	state->err_lang = NULL;
	free_sftp_str(state->err_msg);
	state->err_msg = NULL;
	state->id++;
	return appendheader(state, type);
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
	pthread_mutex_unlock(&state->mtx);
	if (WaitForEvent(state->recv_event, INFINITE) != WAIT_OBJECT_0) {
		pthread_mutex_lock(&state->mtx);
		return false;
	}
	pthread_mutex_lock(&state->mtx);
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
response_handled(sftpc_state_t state)
{
	sftp_remove_packet(state->rxp);
	if (!sftp_have_full_pkt(state->rxp))
		ResetEvent(state->recv_event);
}

static void
handle_error(sftpc_state_t state)
{
	if (state->rxp->type == SSH_FXP_STATUS) {
		state->err_code = get32(state);
		if (state->version > 2) {
			if (state->err_msg != NULL)
				free_sftp_str(state->err_msg);
			state->err_msg = getstring(state);
			if (state->err_lang != NULL)
				free_sftp_str(state->err_lang);
			state->err_lang = getstring(state);
		}
	}
	response_handled(state);
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

void
sftpc_finish(sftpc_state_t state)
{
	assert(state);
	if (state == NULL)
		return;
	pthread_mutex_lock(&state->mtx);
	if (state->terminating == true) {
		pthread_mutex_unlock(&state->mtx);
		return;
	}
	state->terminating = true;
	pthread_mutex_unlock(&state->mtx);
	// TODO: Close all open handles
	while (!CloseEvent(state->recv_event)) {
		assert(errno == EBUSY);
		if (errno != EBUSY)
			break;
		SetEvent(state->recv_event);
		SLEEP(1);
	}
	pthread_mutex_lock(&state->mtx);
	while (state->running) {
		pthread_mutex_unlock(&state->mtx);
		SLEEP(1);
		pthread_mutex_lock(&state->mtx);
	}
	sftp_free_rx_pkt(state->rxp);
	sftp_free_tx_pkt(state->txp);
	pthread_mutex_unlock(&state->mtx);
}

void
sftpc_end(sftpc_state_t state)
{
	assert(state);
	if (state == NULL)
		return;
	pthread_mutex_lock(&state->mtx);
	assert(state->terminating);
	pthread_mutex_unlock(&state->mtx);
	pthread_mutex_destroy(&state->mtx);
	free(state);
}

sftpc_state_t
sftpc_begin(bool (*send_cb)(uint8_t *buf, size_t len, void *cb_data), void *cb_data)
{
	sftpc_state_t ret = (sftpc_state_t)malloc(sizeof(struct sftp_client_state));
	if (ret == NULL)
		return NULL;
	ret->recv_event = CreateEvent(NULL, TRUE, FALSE, NULL);
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
	ret->running = 0;
	pthread_mutex_init(&ret->mtx, NULL);
	ret->terminating = false;
	return ret;
}

bool
sftpc_init(sftpc_state_t state)
{
	if (!enter_function(state))
		return false;
	if (!cappendheader(state, SSH_FXP_INIT))
		return exit_function(state, false);
	if (!append32(state, SFTP_VERSION))
		return exit_function(state, false);
	if (!get_result(state))
		return exit_function(state, false);
	if (state->rxp->type != SSH_FXP_VERSION)
		return exit_function(state, false);
	state->version = get32(state);
	if (state->version > SFTP_VERSION) {
		response_handled(state);
		return exit_function(state, false);
	}
	response_handled(state);
	state->id = 0;
	return exit_function(state, true);
}

bool
sftpc_recv(sftpc_state_t state, uint8_t *buf, uint32_t sz)
{
	if (!enter_function(state))
		return false;
	if (!sftp_rx_pkt_append(&state->rxp, buf, sz))
		return exit_function(state, false);
	if (sftp_have_full_pkt(state->rxp))
		SetEvent(state->recv_event);
	return exit_function(state, true);
}

bool
sftpc_realpath(sftpc_state_t state, char *path, sftp_str_t *ret)
{
	if (!enter_function(state))
		return false;
	assert(ret);
	if (ret == NULL)
		return exit_function(state, false);
	if (*ret != NULL)
		return exit_function(state, false);
	if (!cappendheader(state, SSH_FXP_REALPATH))
		return exit_function(state, false);
	sftp_str_t pstr = sftp_strdup(path);
	if (!appendandfreestring(state, &pstr))
		return exit_function(state, false);
	if (!get_result(state))
		return exit_function(state, false);
	if (state->rxp->type == SSH_FXP_NAME) {
		if (get32(state) != 1) {
			response_handled(state);
			return exit_function(state, false);
		}
		*ret = getstring(state);
		response_handled(state);
		return exit_function(state, true);
	}
	handle_error(state);
	return exit_function(state, false);
}

bool
sftpc_open(sftpc_state_t state, char *path, uint32_t flags, sftp_file_attr_t attr, sftp_dirhandle_t *handle)
{
	if (!enter_function(state))
		return false;
	assert(path);
	if (path == NULL)
		return exit_function(state, false);
	assert(handle);
	if (handle == NULL)
		return exit_function(state, false);
	assert(!*handle);
	if (*handle != NULL)
		return exit_function(state, false);
	if (!cappendheader(state, SSH_FXP_OPEN))
		return exit_function(state, false);
	sftp_str_t pstr = sftp_strdup(path);
	if (!appendandfreestring(state, &pstr))
		return exit_function(state, false);
	if (!append32(state, flags))
		return exit_function(state, false);
	sftp_file_attr_t a = attr;
	if (a == NULL) {
		a = sftp_fattr_alloc();
		if (a == NULL)
			return exit_function(state, false);
	}
	if (!appendfattr(state, a)) {
		if (a != attr)
			sftp_fattr_free(a);
		return exit_function(state, false);
	}
	if (a != attr)
		sftp_fattr_free(a);
	if (!get_result(state))
		return exit_function(state, false);
	if (state->rxp->type == SSH_FXP_HANDLE) {
		if (!parse_handle(state, (sftp_str_t *)handle)) {
			response_handled(state);
			return exit_function(state, false);
		}
		response_handled(state);
		return exit_function(state, true);
	}
	handle_error(state);
	return exit_function(state, false);
}

bool
sftpc_close(sftpc_state_t state, sftp_filehandle_t *handle)
{
	if (!enter_function(state))
		return false;
	if (!cappendheader(state, SSH_FXP_CLOSE))
		return exit_function(state, false);
	if (!appendfhandle(state, *handle))
		return exit_function(state, false);
	if (!get_result(state))
		return exit_function(state, false);
	handle_error(state);
	free_sftp_str(*handle);
	*handle = NULL;
	return exit_function(state, state->err_code == SSH_FX_OK);
}

bool
sftpc_read(sftpc_state_t state, sftp_filehandle_t handle, uint64_t offset, uint32_t len, sftp_str_t *ret)
{
	if (!enter_function(state))
		return false;
	assert(ret);
	if (ret == NULL)
		return exit_function(state, false);
	assert(*ret == NULL);
	if (*ret != NULL)
		return exit_function(state, false);
	if (!cappendheader(state, SSH_FXP_READ))
		return exit_function(state, false);
	if (!appendfhandle(state, handle))
		return exit_function(state, false);
	if (!append64(state, offset))
		return exit_function(state, false);
	if (!append32(state, len))
		return exit_function(state, false);
	if (!get_result(state))
		return exit_function(state, false);
	if (state->rxp->type == SSH_FXP_DATA) {
		*ret = getstring(state);
		response_handled(state);
		return exit_function(state, *ret != NULL);
	}
	handle_error(state);
	return exit_function(state, false);
}

bool
sftpc_write(sftpc_state_t state, sftp_filehandle_t handle, uint64_t offset, sftp_str_t data)
{
	if (!enter_function(state))
		return false;
	assert(data);
	if (data == NULL)
		return exit_function(state, false);
	if (!cappendheader(state, SSH_FXP_WRITE))
		return exit_function(state, false);
	if (!appendfhandle(state, handle))
		return exit_function(state, false);
	if (!append64(state, offset))
		return exit_function(state, false);
	if (!appendstring(state, data))
		return exit_function(state, false);
	if (!get_result(state))
		return exit_function(state, false);
	handle_error(state);
	return exit_function(state, state->err_code == SSH_FX_OK);
}

bool
sftpc_reclaim(sftpc_state_t state)
{
	bool ret = true;
	ret = sftp_tx_pkt_reclaim(&state->txp) && ret;
	ret = sftp_rx_pkt_reclaim(&state->rxp) && ret;
	return ret;
}

uint32_t
sftpc_get_err(sftpc_state_t state)
{
	uint32_t ret = SSH_FX_FAILURE;
	if (!enter_function(state))
		return ret;
	ret = state->err_code;
	exit_function(state, true);
	return ret;
}
