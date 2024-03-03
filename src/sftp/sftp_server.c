#include <assert.h>
#include <genwrap.h>
#include <stdlib.h>
#include <threadwrap.h>
#include <str_list.h>

#include "sftp.h"

#define SFTP_STATIC_TYPE sftps_state_t
#include "sftp_static.h"
#undef SFTP_STATIC_TYPE

static uint64_t
get64(sftps_state_t state)
{
	return sftp_get64(state->rxp);
}

static sftp_str_t
getcstring(sftps_state_t state)
{
	sftp_str_t str = getstring(state);
	if (str == NULL)
		return NULL;
	if (memchr(str->c_str, 0, str->len) != NULL) {
		free_sftp_str(str);
		return NULL;
	}
	return str;
}

static bool
appendcstring(sftps_state_t state, const char *s)
{
	bool ret = sftp_appendcstring(&state->txp, s);
	return ret;
}

static bool
init(sftps_state_t state)
{
	state->version = get32(state);
	if (state->version > SFTP_VERSION)
		state->version = SFTP_VERSION;
	if (!appendheader(state, SSH_FXP_VERSION))
		return false;
	if (!append32(state, state->version))
		return false;
	return sftps_send_packet(state);
}

static bool
s_open(sftps_state_t state)
{
	bool ret;
	sftp_str_t fname;
	uint32_t flags;
	sftp_file_attr_t attrs;

	state->id = get32(state);
	fname = getcstring(state);
	if (fname == NULL)
		return false;
	flags = get32(state);
	if (!(flags & SSH_FXF_WRITE)) {
		if (flags & SSH_FXF_CREAT) {
			sftps_send_error(state, SSH_FX_OP_UNSUPPORTED, "Can't create unless writing");
			free_sftp_str(fname);
			return true;
		}
		if (flags & SSH_FXF_APPEND) {
			sftps_send_error(state, SSH_FX_OP_UNSUPPORTED, "Can't append unless writing");
			free_sftp_str(fname);
			return true;
		}
	}
	if (!(flags & SSH_FXF_CREAT)) {
		if (flags & SSH_FXF_TRUNC) {
			sftps_send_error(state, SSH_FX_OP_UNSUPPORTED, "Can't truncate unless creating");
			free_sftp_str(fname);
			return true;
		}
		if (flags & SSH_FXF_EXCL) {
			sftps_send_error(state, SSH_FX_OP_UNSUPPORTED, "Can't open exclusive unless creating");
			free_sftp_str(fname);
			return true;
		}
	}
	attrs = sftp_getfattr(state->rxp);
	if (attrs == NULL) {
		free_sftp_str(fname);
		return false;
	}
	if (!(flags & SSH_FXF_CREAT)) {
		if (sftp_fattr_get_atime(attrs, NULL) ||
		    sftp_fattr_get_ext_count(attrs) != 0 ||
		    sftp_fattr_get_gid(attrs, NULL) ||
		    sftp_fattr_get_mtime(attrs, NULL) ||
		    sftp_fattr_get_permissions(attrs, NULL) ||
		    sftp_fattr_get_size(attrs, NULL)) {
			sftps_send_error(state, SSH_FX_OP_UNSUPPORTED, "Attributes invalid unless creating");
		}
	}
	ret = state->open(fname, flags, attrs, state->cb_data);
	sftp_fattr_free(attrs);
	free_sftp_str(fname);
	return ret;
}

static bool
s_read(sftps_state_t state)
{
	bool ret;
	sftp_filehandle_t handle;
	uint64_t offset;
	uint32_t len;

	state->id = get32(state);
	handle = getstring(state);
	if (handle == NULL)
		return false;
	if (handle->len < 1) {
		free_sftp_str(handle);
		return false;
	}
	offset = get64(state);
	len = get32(state);
	ret = state->read(handle, offset, len, state->cb_data);
	free_sftp_str(handle);
	return ret;
}

static bool
s_write(sftps_state_t state)
{
	bool ret;
	sftp_filehandle_t handle;
	uint64_t offset;
	sftp_str_t data;

	state->id = get32(state);
	handle = getstring(state);
	if (handle == NULL)
		return false;
	if (handle->len < 1) {
		free_sftp_str(handle);
		return false;
	}
	offset = get64(state);
	data = getstring(state);
	if (data == NULL) {
		free_sftp_str(handle);
		return false;
	}
	ret = state->write(handle, offset, data, state->cb_data);
	free_sftp_str(handle);
	free_sftp_str(data);
	return ret;
}

static bool
s_close(sftps_state_t state)
{
	bool ret;
	sftp_str_t handle;
	
	state->id = get32(state);
	handle = getstring(state);
	if (handle == NULL)
		return false;
	if (handle->len < 1) {
		free_sftp_str(handle);
		return false;
	}
	ret = state->close(handle, state->cb_data);
	free_sftp_str(handle);
	return ret;
}

static bool
s_readdir(sftps_state_t state)
{
	bool ret;
	sftp_dirhandle_t handle;

	state->id = get32(state);
	handle = getstring(state);
	if (handle == NULL)
		return false;
	if (handle->len < 1) {
		free_sftp_str(handle);
		return false;
	}
	ret = state->readdir(handle, state->cb_data);
	free_sftp_str(handle);
	return ret;
}

static bool
s_id_str(sftps_state_t state, bool (*cb)(sftp_str_t, void *))
{
	bool ret;
	sftp_str_t str;
	
	state->id = get32(state);
	str = getcstring(state);
	if (str == NULL)
		return false;
	ret = cb(str, state->cb_data);
	free_sftp_str(str);
	return ret;
}

static bool
s_fstat(sftps_state_t state)
{
	bool ret;
	sftp_filehandle_t handle;

	state->id = get32(state);
	handle = getstring(state);
	if (handle == NULL)
		return false;
	if (handle->len < 1) {
		free_sftp_str(handle);
		return false;
	}
	ret = state->fstat(handle, state->cb_data);
	free_sftp_str(handle);
	return ret;
}

static bool
s_id_str_attr(sftps_state_t state, bool (*cb)(sftp_str_t, sftp_file_attr_t, void *))
{
	bool ret;
	sftp_str_t str;
	sftp_file_attr_t attrs;
	
	state->id = get32(state);
	str = getcstring(state);
	if (str == NULL)
		return false;
	attrs = sftp_getfattr(state->rxp);
	if (attrs == NULL) {
		free_sftp_str(str);
		return false;
	}
	ret = cb(str, attrs, state->cb_data);
	free_sftp_str(str);
	sftp_fattr_free(attrs);
	return ret;
}

static bool
s_fsetstat(sftps_state_t state)
{
	bool ret;
	sftp_filehandle_t handle;
	sftp_file_attr_t attrs;
	
	state->id = get32(state);
	handle = getstring(state);
	if (handle == NULL)
		return false;
	if (handle->len < 1) {
		free_sftp_str(handle);
		return false;
	}
	attrs = sftp_getfattr(state->rxp);
	if (attrs == NULL) {
		free_sftp_str(handle);
		return false;
	}
	ret = state->fsetstat(handle, attrs, state->cb_data);
	free_sftp_str(handle);
	sftp_fattr_free(attrs);
	return ret;
}

static bool
s_id_str_str(sftps_state_t state, bool (*cb)(sftp_str_t, sftp_str_t, void *))
{
	bool ret;
	sftp_str_t str1;
	sftp_str_t str2;
	
	state->id = get32(state);
	str1 = getcstring(state);
	if (str1 == NULL)
		return false;
	str2 = getcstring(state);
	if (str2 == NULL) {
		free_sftp_str(str1);
		return false;
	}
	ret = cb(str1, str2, state->cb_data);
	free_sftp_str(str1);
	free_sftp_str(str2);
	return ret;
}

sftps_state_t
sftps_begin(bool (*send_cb)(uint8_t *buf, size_t len, void *cb_data), void *cb_data)
{
	sftps_state_t ret = (sftps_state_t)calloc(1, sizeof(struct sftp_server_state));
	if (ret == NULL)
		return NULL;
	ret->send_cb = send_cb;
	ret->cb_data = cb_data;
	pthread_mutex_init(&ret->mtx, NULL);
	ret->terminating = false;
	return ret;
}

bool
sftps_send_packet(sftps_state_t state)
{
	uint8_t *txbuf;
	size_t txsz;

	if (!sftp_prep_tx_packet(state->txp, &txbuf, &txsz))
		return false;
	if (!state->send_cb(txbuf, txsz, state->cb_data))
		return false;
	return true;
}

bool
sftps_send_error(sftps_state_t state, uint32_t code, const char *msg)
{
	if (state->lprintf)
		state->lprintf(state->cb_data, code, "%s", msg);
	if (!appendheader(state, SSH_FXP_STATUS))
		return false;
	if (!append32(state, code))
		return false;
	if (state->version >= 3) {
		if (!appendcstring(state, msg))
			return false;
		if (!appendcstring(state, "en-CA"))
			return false;
	}
	bool ret = sftps_send_packet(state);
	// Shrink TX buffer on "errors" (which can also be successes)
	sftps_reclaim(state);
	return ret;
}

bool
sftps_end(sftps_state_t state)
{
	pthread_mutex_lock(&state->mtx);
	state->terminating = true;
	if (state->cleanup_callback)
		state->cleanup_callback(state->cb_data);
	while (state->running) {
		pthread_mutex_unlock(&state->mtx);
		SLEEP(1);
		pthread_mutex_lock(&state->mtx);
	}
	pthread_mutex_unlock(&state->mtx);
	pthread_mutex_destroy(&state->mtx);
	free(state);
	return true;
}

bool
sftps_recv(sftps_state_t state, uint8_t *buf, uint32_t sz)
{
	if (!enter_function(state))
		return false;
	if (!sftp_rx_pkt_append(&state->rxp, buf, sz))
		return exit_function(state, false);
	if (sftp_have_pkt_sz(state->rxp)) {
		uint32_t psz = sftp_pkt_sz(state->rxp);
		if (psz > SFTP_MAX_PACKET_SIZE) {
			if (state->lprintf)
				state->lprintf(state->cb_data, SSH_FX_FAILURE, "Packet too large (%" PRIu32 " bytes)", psz);
			return exit_function(state, false);
		}
	}
	while (sftp_have_full_pkt(state->rxp)) {
		bool handled = false;

		switch(state->rxp->type) {
			case SSH_FXP_INIT:
				if (!init(state))
					return exit_function(state, false);
				handled = true;
				break;
			case SSH_FXP_OPEN:
				if (state->open) {
					if (!s_open(state))
						return exit_function(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_CLOSE:
				if (state->close) {
					if (!s_close(state))
						return exit_function(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_READ:
				if (state->read) {
					if (!s_read(state))
						return exit_function(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_WRITE:
				if (state->write) {
					if (!s_write(state))
						return exit_function(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_LSTAT:
				if (state->lstat) {
					if (!s_id_str(state, state->lstat))
						return exit_function(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_FSTAT:
				if (state->fstat) {
					if (!s_fstat(state))
						return exit_function(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_SETSTAT:
				if (state->setstat) {
					if (!s_id_str_attr(state, state->setstat))
						return exit_function(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_FSETSTAT:
				if (state->fsetstat) {
					if (!s_fsetstat(state))
						return exit_function(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_OPENDIR:
				if (state->opendir) {
					if (!s_id_str(state, state->opendir))
						return exit_function(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_READDIR:
				if (state->readdir) {
					if (!s_readdir(state))
						return exit_function(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_REMOVE:
				if (state->remove) {
					if (!s_id_str(state, state->remove))
						return exit_function(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_MKDIR:
				if (state->mkdir) {
					if (!s_id_str_attr(state, state->mkdir))
						return exit_function(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_RMDIR:
				if (state->rmdir) {
					if (!s_id_str(state, state->rmdir))
						return exit_function(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_REALPATH:
				if (state->realpath) {
					if (!s_id_str(state, state->realpath))
						return exit_function(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_STAT:
				if (state->stat) {
					if (!s_id_str(state, state->stat))
						return exit_function(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_RENAME:
				if (state->version >= 2 && state->rename) {
					if (!s_id_str_str(state, state->rename))
						return exit_function(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_READLINK:
				if (state->version >= 3 && state->readlink) {
					if (!s_id_str(state, state->readlink))
						return exit_function(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_SYMLINK:
				if (state->version >= 3 && state->symlink) {
					if (!s_id_str_str(state, state->symlink))
						return exit_function(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_EXTENDED:
				if (state->version >= 3 && state->extended) {
					state->id = get32(state);
					sftp_str_t request = getcstring(state);
					if (request == NULL)
						return exit_function(state, false);
					handled = state->extended(request, state->rxp, state->cb_data);
					free_sftp_str(request);
				}
			default:
				break;
		}
		if (!handled) {
			if (state->lprintf)
				state->lprintf(state->cb_data, SSH_FX_FAILURE, "Unhandled request type: %s (%d)", sftp_get_type_name(state->rxp->type), state->rxp->type);
			state->id = get32(state);
			if (!sftps_send_error(state, SSH_FX_OP_UNSUPPORTED, "Operation not implemented"))
				return exit_function(state, false);
		}
		sftp_remove_packet(state->rxp);
	}
	return exit_function(state, true);
}

bool
sftps_send_handle(sftps_state_t state, sftp_str_t handle)
{
	if (!appendheader(state, SSH_FXP_HANDLE))
		return false;
	if (!appendstring(state, handle))
		return false;
	return sftps_send_packet(state);
}

bool
sftps_send_data(sftps_state_t state, sftp_str_t data)
{
	if (!appendheader(state, SSH_FXP_DATA))
		return false;
	if (!appendstring(state, data))
		return false;
	return sftps_send_packet(state);
}

bool
sftps_send_name(sftps_state_t state, uint32_t count, str_list_t fnames, str_list_t lnames, sftp_file_attr_t *attrs)
{
	if (!appendheader(state, SSH_FXP_NAME))
		return false;
	if (!append32(state, count))
		return false;
	for (uint32_t idx = 0; idx < count; idx++) {
		if (fnames[idx] == NULL) {
			if (state->lprintf)
				state->lprintf(state->cb_data, SSH_FX_FAILURE, "Reached fnames terminator at position %" PRIu32 " of " PRIu32, idx, count);
			return false;
		}
		if (!appendcstring(state, fnames[idx]))
			return false;
		if (lnames[idx] == NULL) {
			if (state->lprintf)
				state->lprintf(state->cb_data, SSH_FX_FAILURE, "Reached lnames terminator at position %" PRIu32 " of " PRIu32, idx, count);
			return false;
		}
		if (!appendcstring(state, lnames[idx]))
			return false;
		if (attrs[idx] == NULL) {
			if (state->lprintf)
				state->lprintf(state->cb_data, SSH_FX_FAILURE, "Reached attrs terminator at position %" PRIu32 " of " PRIu32, idx, count);
			return false;
		}
		if (!appendfattr(state, attrs[idx]))
			return false;
	}
	return sftps_send_packet(state);
}

bool
sftps_send_attrs(sftps_state_t state, sftp_file_attr_t attr)
{
	if (!appendheader(state, SSH_FXP_ATTRS))
		return false;
	if (!appendfattr(state, attr))
		return false;
	return sftps_send_packet(state);
}

bool
sftps_reclaim(sftps_state_t state)
{
	bool ret = true;
	ret = sftp_tx_pkt_reclaim(&state->txp) && ret;
	ret = sftp_rx_pkt_reclaim(&state->rxp) && ret;
	return ret;
}
