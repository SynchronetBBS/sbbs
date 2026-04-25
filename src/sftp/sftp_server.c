/*
 * Part of the single-TU build.  All system and third-party includes
 * live in sftp.c; this file is #include'd from sftp.c and cannot be
 * compiled on its own.
 *
 * Public op contract (server side):
 *   true  — op completed (the reply, if any, is on the wire).  out->err
 *           and out->estr are diagnostic only; mustn't drive control
 *           flow.
 *   false — op didn't complete.  out->err is a machine-readable lib
 *           failure code; out->estr is human-readable text ready to
 *           display.
 *
 * Pass NULL for `out` to ignore everything.
 *
 * sftps_outcome carries no `result` field — the SSH_FX_* code in a
 * STATUS reply is supplied by the caller of sftps_send_error, so
 * recording it back is redundant.
 */

/*
 * Private server state.  Everything the library manipulates and the
 * consumer cannot touch.  Lives entirely inside this TU so consumers
 * have no way to reach through to rxp, txp, the mutex, or the running
 * counter.
 */
struct sftp_server_state_private {
	sftp_rx_pkt_t rxp;
	sftp_tx_pkt_t txp;
	pthread_mutex_t mtx;
	uint32_t running;
	uint32_t id;
	bool terminating;
};

static bool sftps_reclaim(sftps_state_t state);

/*
 * Enter/exit around the state mutex.  The server holds the lock for
 * the full duration of an op, including any consumer callback, so the
 * consumer never needs to lock.
 */
static bool
server_enter(sftps_state_t state, struct sftps_outcome *out)
{
	if (state == NULL) {
		sftps_outcome_record(out, SFTP_ERR_NULL_STATE, "state == NULL");
		return false;
	}
	if (pthread_mutex_lock(&state->priv->mtx)) {
		sftps_outcome_record(out, SFTP_ERR_OOM,
		    "pthread_mutex_lock failed");
		return false;
	}
	if (state->priv->terminating) {
		pthread_mutex_unlock(&state->priv->mtx);
		sftps_outcome_record(out, SFTP_ERR_ABORTED,
		    "session terminating");
		return false;
	}
	state->priv->running++;
	return true;
}

static bool
server_exit(sftps_state_t state, bool retval)
{
	assert(state->priv->running > 0);
	state->priv->running--;
	pthread_mutex_unlock(&state->priv->mtx);
	return retval;
}

/*
 * C-string-safe getstring: bails on embedded NULs so a malicious peer
 * can't smuggle a truncated filename past a callback that assumes
 * c_str is a C string.
 */
static sftp_str_t
getcstring(sftps_state_t state)
{
	sftp_str_t str = getstring(state->priv->rxp);
	if (str == NULL)
		return NULL;
	if (memchr(str->c_str, 0, str->len) != NULL) {
		free_sftp_str(str);
		return NULL;
	}
	return str;
}

static bool
init(sftps_state_t state, struct sftps_outcome *out)
{
	state->version = get32(state->priv->rxp);
	if (state->version > SFTP_VERSION)
		state->version = SFTP_VERSION;
	/* Intersect client's advertised extensions with the ones we support.
	 * The result is what we enable for this session AND what we echo
	 * back to the client in VERSION. */
	state->extensions = 0;
	uint32_t payload_len = pkt_sz(state->priv->rxp) - 1;
	while (state->priv->rxp->cur + sizeof(uint32_t) <= payload_len) {
		sftp_str_t ext_name = getstring(state->priv->rxp);
		sftp_str_t ext_data = getstring(state->priv->rxp);
		if (ext_name == NULL || ext_data == NULL) {
			free_sftp_str(ext_name);
			free_sftp_str(ext_data);
			break;
		}
		state->extensions |= extension_match(ext_name, ext_data) & SFTP_EXT_ALL;
		free_sftp_str(ext_name);
		free_sftp_str(ext_data);
	}
	if (!appendheader(&state->priv->txp, SSH_FXP_VERSION, state->priv->id) ||
	    !append32(&state->priv->txp, state->version) ||
	    !append_extensions(&state->priv->txp, state->extensions)) {
		sftps_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
		    "build VERSION reply failed");
		return false;
	}
	return sftps_send_packet(state, out);
}

uint32_t
sftps_get_extensions(sftps_state_t state)
{
	if (state == NULL)
		return 0;
	return state->extensions;
}

static bool
s_open(sftps_state_t state, struct sftps_outcome *out)
{
	bool ret;
	sftp_str_t fname;
	uint32_t flags;
	sftp_file_attr_t attrs;

	state->priv->id = get32(state->priv->rxp);
	fname = getcstring(state);
	if (fname == NULL) {
		sftps_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
		    "OPEN: filename getcstring failed");
		return false;
	}
	flags = get32(state->priv->rxp);
	if (!(flags & SSH_FXF_WRITE)) {
		if (flags & SSH_FXF_CREAT) {
			sftps_send_error(state, SSH_FX_OP_UNSUPPORTED,
			    "Can't create unless writing", out);
			free_sftp_str(fname);
			return true;
		}
		if (flags & SSH_FXF_APPEND) {
			sftps_send_error(state, SSH_FX_OP_UNSUPPORTED,
			    "Can't append unless writing", out);
			free_sftp_str(fname);
			return true;
		}
	}
	if (!(flags & SSH_FXF_CREAT)) {
		if (flags & SSH_FXF_TRUNC) {
			sftps_send_error(state, SSH_FX_OP_UNSUPPORTED,
			    "Can't truncate unless creating", out);
			free_sftp_str(fname);
			return true;
		}
		if (flags & SSH_FXF_EXCL) {
			sftps_send_error(state, SSH_FX_OP_UNSUPPORTED,
			    "Can't open exclusive unless creating", out);
			free_sftp_str(fname);
			return true;
		}
	}
	attrs = getfattr(state->priv->rxp);
	if (attrs == NULL) {
		sftps_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
		    "OPEN: getfattr failed");
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
			sftps_send_error(state, SSH_FX_OP_UNSUPPORTED,
			    "Attributes invalid unless creating", out);
		}
	}
	ret = state->open(fname, flags, attrs, state->cb_data);
	sftp_fattr_free(attrs);
	free_sftp_str(fname);
	if (!ret)
		sftps_outcome_record(out, SFTP_ERR_SEND_FAILED,
		    "OPEN: callback failed");
	return ret;
}

static bool
s_read(sftps_state_t state, struct sftps_outcome *out)
{
	bool ret;
	sftp_filehandle_t handle;
	uint64_t offset;
	uint32_t len;

	state->priv->id = get32(state->priv->rxp);
	handle = getstring(state->priv->rxp);
	if (handle == NULL || handle->len < 1) {
		sftps_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
		    "READ: handle getstring failed");
		free_sftp_str(handle);
		return false;
	}
	offset = get64(state->priv->rxp);
	len = get32(state->priv->rxp);
	ret = state->read(handle, offset, len, state->cb_data);
	free_sftp_str(handle);
	if (!ret)
		sftps_outcome_record(out, SFTP_ERR_SEND_FAILED,
		    "READ: callback failed");
	return ret;
}

static bool
s_write(sftps_state_t state, struct sftps_outcome *out)
{
	bool ret;
	sftp_filehandle_t handle;
	uint64_t offset;
	sftp_str_t data;

	state->priv->id = get32(state->priv->rxp);
	handle = getstring(state->priv->rxp);
	if (handle == NULL || handle->len < 1) {
		sftps_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
		    "WRITE: handle getstring failed");
		free_sftp_str(handle);
		return false;
	}
	offset = get64(state->priv->rxp);
	data = getstring(state->priv->rxp);
	if (data == NULL) {
		sftps_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
		    "WRITE: data getstring failed");
		free_sftp_str(handle);
		return false;
	}
	ret = state->write(handle, offset, data, state->cb_data);
	free_sftp_str(handle);
	free_sftp_str(data);
	if (!ret)
		sftps_outcome_record(out, SFTP_ERR_SEND_FAILED,
		    "WRITE: callback failed");
	return ret;
}

static bool
s_close(sftps_state_t state, struct sftps_outcome *out)
{
	bool ret;
	sftp_str_t handle;

	state->priv->id = get32(state->priv->rxp);
	handle = getstring(state->priv->rxp);
	if (handle == NULL || handle->len < 1) {
		sftps_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
		    "CLOSE: handle getstring failed");
		free_sftp_str(handle);
		return false;
	}
	ret = state->close(handle, state->cb_data);
	free_sftp_str(handle);
	if (!ret)
		sftps_outcome_record(out, SFTP_ERR_SEND_FAILED,
		    "CLOSE: callback failed");
	return ret;
}

static bool
s_readdir(sftps_state_t state, struct sftps_outcome *out)
{
	bool ret;
	sftp_dirhandle_t handle;

	state->priv->id = get32(state->priv->rxp);
	handle = getstring(state->priv->rxp);
	if (handle == NULL || handle->len < 1) {
		sftps_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
		    "READDIR: handle getstring failed");
		free_sftp_str(handle);
		return false;
	}
	ret = state->readdir(handle, state->cb_data);
	free_sftp_str(handle);
	if (!ret)
		sftps_outcome_record(out, SFTP_ERR_SEND_FAILED,
		    "READDIR: callback failed");
	return ret;
}

static bool
s_id_str(sftps_state_t state, bool (*cb)(sftp_str_t, void *),
         struct sftps_outcome *out)
{
	bool ret;
	sftp_str_t str;

	state->priv->id = get32(state->priv->rxp);
	str = getcstring(state);
	if (str == NULL) {
		sftps_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
		    "id_str: getcstring failed");
		return false;
	}
	ret = cb(str, state->cb_data);
	free_sftp_str(str);
	if (!ret)
		sftps_outcome_record(out, SFTP_ERR_SEND_FAILED,
		    "id_str: callback failed");
	return ret;
}

static bool
s_fstat(sftps_state_t state, struct sftps_outcome *out)
{
	bool ret;
	sftp_filehandle_t handle;

	state->priv->id = get32(state->priv->rxp);
	handle = getstring(state->priv->rxp);
	if (handle == NULL || handle->len < 1) {
		sftps_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
		    "FSTAT: handle getstring failed");
		free_sftp_str(handle);
		return false;
	}
	ret = state->fstat(handle, state->cb_data);
	free_sftp_str(handle);
	if (!ret)
		sftps_outcome_record(out, SFTP_ERR_SEND_FAILED,
		    "FSTAT: callback failed");
	return ret;
}

static bool
s_id_str_attr(sftps_state_t state,
              bool (*cb)(sftp_str_t, sftp_file_attr_t, void *),
              struct sftps_outcome *out)
{
	bool ret;
	sftp_str_t str;
	sftp_file_attr_t attrs;

	state->priv->id = get32(state->priv->rxp);
	str = getcstring(state);
	if (str == NULL) {
		sftps_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
		    "id_str_attr: getcstring failed");
		return false;
	}
	attrs = getfattr(state->priv->rxp);
	if (attrs == NULL) {
		sftps_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
		    "id_str_attr: getfattr failed");
		free_sftp_str(str);
		return false;
	}
	ret = cb(str, attrs, state->cb_data);
	free_sftp_str(str);
	sftp_fattr_free(attrs);
	if (!ret)
		sftps_outcome_record(out, SFTP_ERR_SEND_FAILED,
		    "id_str_attr: callback failed");
	return ret;
}

static bool
s_fsetstat(sftps_state_t state, struct sftps_outcome *out)
{
	bool ret;
	sftp_filehandle_t handle;
	sftp_file_attr_t attrs;

	state->priv->id = get32(state->priv->rxp);
	handle = getstring(state->priv->rxp);
	if (handle == NULL || handle->len < 1) {
		sftps_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
		    "FSETSTAT: handle getstring failed");
		free_sftp_str(handle);
		return false;
	}
	attrs = getfattr(state->priv->rxp);
	if (attrs == NULL) {
		sftps_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
		    "FSETSTAT: getfattr failed");
		free_sftp_str(handle);
		return false;
	}
	ret = state->fsetstat(handle, attrs, state->cb_data);
	free_sftp_str(handle);
	sftp_fattr_free(attrs);
	if (!ret)
		sftps_outcome_record(out, SFTP_ERR_SEND_FAILED,
		    "FSETSTAT: callback failed");
	return ret;
}

static bool
s_id_str_str(sftps_state_t state,
             bool (*cb)(sftp_str_t, sftp_str_t, void *),
             struct sftps_outcome *out)
{
	bool ret;
	sftp_str_t str1;
	sftp_str_t str2;

	state->priv->id = get32(state->priv->rxp);
	str1 = getcstring(state);
	if (str1 == NULL) {
		sftps_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
		    "id_str_str: first getcstring failed");
		return false;
	}
	str2 = getcstring(state);
	if (str2 == NULL) {
		sftps_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
		    "id_str_str: second getcstring failed");
		free_sftp_str(str1);
		return false;
	}
	ret = cb(str1, str2, state->cb_data);
	free_sftp_str(str1);
	free_sftp_str(str2);
	if (!ret)
		sftps_outcome_record(out, SFTP_ERR_SEND_FAILED,
		    "id_str_str: callback failed");
	return ret;
}

sftps_state_t
sftps_begin(bool (*send_cb)(uint8_t *buf, size_t len, void *cb_data), void *cb_data)
{
	sftps_state_t ret = (sftps_state_t)calloc(1, sizeof(struct sftp_server_state));
	if (ret == NULL)
		return NULL;
	ret->priv = (struct sftp_server_state_private *)calloc(1, sizeof(*ret->priv));
	if (ret->priv == NULL) {
		free(ret);
		return NULL;
	}
	ret->send_cb = send_cb;
	ret->cb_data = cb_data;
	pthread_mutex_init(&ret->priv->mtx, NULL);
	return ret;
}

bool
sftps_send_packet(sftps_state_t state, struct sftps_outcome *out)
{
	uint8_t *txbuf;
	size_t txsz;

	if (!prep_tx_packet(state->priv->txp, &txbuf, &txsz)) {
		sftps_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
		    "prep_tx_packet failed");
		return false;
	}
	if (!state->send_cb(txbuf, txsz, state->cb_data)) {
		sftps_outcome_record(out, SFTP_ERR_SEND_FAILED,
		    "send_cb failed");
		return false;
	}
	return true;
}

static void
lprintf(sftps_state_t state, uint32_t code, const char *fmt, ...)
{
	char *msg;
	va_list va;
	int rc;

	if (state->lprint == NULL)
		return;
	if (fmt == NULL)
		return;
	va_start(va, fmt);
	rc = vasprintf(&msg, fmt, va);
	va_end(va);
	if (rc == -1)
		return;
	state->lprint(state->cb_data, code, msg);
}

bool
sftps_send_error(sftps_state_t state, uint32_t code, const char *msg,
                 struct sftps_outcome *out)
{
	lprintf(state, code, "%s", msg);
	if (!appendheader(&state->priv->txp, SSH_FXP_STATUS, state->priv->id) ||
	    !append32(&state->priv->txp, code)) {
		sftps_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
		    "build STATUS header failed");
		return false;
	}
	if (state->version >= 3) {
		if (!appendcstring(&state->priv->txp, msg) ||
		    !appendcstring(&state->priv->txp, "en-CA")) {
			sftps_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
			    "build STATUS message/lang failed");
			return false;
		}
	}
	bool ret = sftps_send_packet(state, out);
	/* Shrink TX buffer on "errors" (which can also be successes). */
	sftps_reclaim(state);
	return ret;
}

bool
sftps_end(sftps_state_t state)
{
	pthread_mutex_lock(&state->priv->mtx);
	state->priv->terminating = true;
	if (state->cleanup_callback)
		state->cleanup_callback(state->cb_data);
	while (state->priv->running) {
		pthread_mutex_unlock(&state->priv->mtx);
		SLEEP(1);
		pthread_mutex_lock(&state->priv->mtx);
	}
	free_rx_pkt(state->priv->rxp);
	free_tx_pkt(state->priv->txp);
	pthread_mutex_unlock(&state->priv->mtx);
	pthread_mutex_destroy(&state->priv->mtx);
	free(state->priv);
	free(state);
	return true;
}

bool
sftps_recv(sftps_state_t state, uint8_t *buf, uint32_t sz,
           struct sftps_outcome *out)
{
	if (!server_enter(state, out))
		return false;
	if (!rx_pkt_append(&state->priv->rxp, buf, sz)) {
		sftps_outcome_record(out, SFTP_ERR_OOM,
		    "rx_pkt_append failed (%" PRIu32 " bytes)", sz);
		return server_exit(state, false);
	}
	if (have_pkt_sz(state->priv->rxp)) {
		uint32_t psz = pkt_sz(state->priv->rxp);
		if (psz > SFTP_MAX_PACKET_SIZE) {
			lprintf(state, SSH_FX_FAILURE,
			    "Packet too large (%" PRIu32 " bytes)", psz);
			sftps_outcome_record(out, SFTP_ERR_PARSE_FAILED,
			    "packet too large (%" PRIu32 " bytes)", psz);
			return server_exit(state, false);
		}
	}
	while (have_full_pkt(state->priv->rxp)) {
		bool handled = false;

		switch(state->priv->rxp->type) {
			case SSH_FXP_INIT:
				if (!init(state, out))
					return server_exit(state, false);
				handled = true;
				break;
			case SSH_FXP_OPEN:
				if (state->open) {
					if (!s_open(state, out))
						return server_exit(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_CLOSE:
				if (state->close) {
					if (!s_close(state, out))
						return server_exit(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_READ:
				if (state->read) {
					if (!s_read(state, out))
						return server_exit(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_WRITE:
				if (state->write) {
					if (!s_write(state, out))
						return server_exit(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_LSTAT:
				if (state->lstat) {
					if (!s_id_str(state, state->lstat, out))
						return server_exit(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_FSTAT:
				if (state->fstat) {
					if (!s_fstat(state, out))
						return server_exit(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_SETSTAT:
				if (state->setstat) {
					if (!s_id_str_attr(state, state->setstat, out))
						return server_exit(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_FSETSTAT:
				if (state->fsetstat) {
					if (!s_fsetstat(state, out))
						return server_exit(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_OPENDIR:
				if (state->opendir) {
					if (!s_id_str(state, state->opendir, out))
						return server_exit(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_READDIR:
				if (state->readdir) {
					if (!s_readdir(state, out))
						return server_exit(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_REMOVE:
				if (state->remove) {
					if (!s_id_str(state, state->remove, out))
						return server_exit(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_MKDIR:
				if (state->mkdir) {
					if (!s_id_str_attr(state, state->mkdir, out))
						return server_exit(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_RMDIR:
				if (state->rmdir) {
					if (!s_id_str(state, state->rmdir, out))
						return server_exit(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_REALPATH:
				if (state->realpath) {
					if (!s_id_str(state, state->realpath, out))
						return server_exit(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_STAT:
				if (state->stat) {
					if (!s_id_str(state, state->stat, out))
						return server_exit(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_RENAME:
				if (state->version >= 2 && state->rename) {
					if (!s_id_str_str(state, state->rename, out))
						return server_exit(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_READLINK:
				if (state->version >= 3 && state->readlink) {
					if (!s_id_str(state, state->readlink, out))
						return server_exit(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_SYMLINK:
				if (state->version >= 3 && state->symlink) {
					if (!s_id_str_str(state, state->symlink, out))
						return server_exit(state, false);
					handled = true;
				}
				break;
			case SSH_FXP_EXTENDED:
				if (state->version >= 3 && state->extended) {
					state->priv->id = get32(state->priv->rxp);
					sftp_str_t request = getcstring(state);
					if (request == NULL) {
						sftps_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
						    "EXTENDED: request getcstring failed");
						return server_exit(state, false);
					}
					handled = state->extended(request, state->priv->rxp, state->cb_data);
					free_sftp_str(request);
					if (!handled)
						sftps_outcome_record(out, SFTP_ERR_SEND_FAILED,
						    "EXTENDED: callback failed");
				}
				break;
			default:
				break;
		}
		if (!handled) {
			lprintf(state, SSH_FX_FAILURE, "Unhandled request type: %s (%d)",
			    sftp_get_type_name(state->priv->rxp->type),
			    state->priv->rxp->type);
			state->priv->id = get32(state->priv->rxp);
			if (!sftps_send_error(state, SSH_FX_OP_UNSUPPORTED,
			                      "Operation not implemented", out))
				return server_exit(state, false);
		}
		remove_packet(state->priv->rxp);
	}
	return server_exit(state, true);
}

bool
sftps_send_handle(sftps_state_t state, sftp_str_t handle,
                  struct sftps_outcome *out)
{
	if (!appendheader(&state->priv->txp, SSH_FXP_HANDLE, state->priv->id) ||
	    !appendstring(&state->priv->txp, handle)) {
		sftps_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
		    "build HANDLE reply failed");
		return false;
	}
	return sftps_send_packet(state, out);
}

bool
sftps_send_data(sftps_state_t state, sftp_str_t data,
                struct sftps_outcome *out)
{
	if (!appendheader(&state->priv->txp, SSH_FXP_DATA, state->priv->id) ||
	    !appendstring(&state->priv->txp, data)) {
		sftps_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
		    "build DATA reply failed");
		return false;
	}
	return sftps_send_packet(state, out);
}

bool
sftps_send_extended_reply(sftps_state_t state, sftp_str_t data,
                          struct sftps_outcome *out)
{
	if (!appendheader(&state->priv->txp, SSH_FXP_EXTENDED_REPLY, state->priv->id) ||
	    !appendstring(&state->priv->txp, data)) {
		sftps_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
		    "build EXTENDED_REPLY failed");
		return false;
	}
	return sftps_send_packet(state, out);
}

sftp_str_t
sftp_rx_get_string(sftp_rx_pkt_t pkt)
{
	return getstring(pkt);
}

bool
sftps_send_name(sftps_state_t state, uint32_t count, str_list_t fnames,
                str_list_t lnames, sftp_file_attr_t *attrs,
                struct sftps_outcome *out)
{
	if (!appendheader(&state->priv->txp, SSH_FXP_NAME, state->priv->id) ||
	    !append32(&state->priv->txp, count)) {
		sftps_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
		    "build NAME header failed");
		return false;
	}
	for (uint32_t idx = 0; idx < count; idx++) {
		if (fnames[idx] == NULL) {
			lprintf(state, SSH_FX_FAILURE,
			    "Reached fnames terminator at position %" PRIu32 " of %" PRIu32,
			    idx, count);
			sftps_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
			    "fnames truncated at %" PRIu32 "/%" PRIu32, idx, count);
			return false;
		}
		if (!appendcstring(&state->priv->txp, fnames[idx])) {
			sftps_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
			    "appendcstring fnames[%" PRIu32 "] failed", idx);
			return false;
		}
		if (lnames[idx] == NULL) {
			lprintf(state, SSH_FX_FAILURE,
			    "Reached lnames terminator at position %" PRIu32 " of %" PRIu32,
			    idx, count);
			sftps_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
			    "lnames truncated at %" PRIu32 "/%" PRIu32, idx, count);
			return false;
		}
		if (!appendcstring(&state->priv->txp, lnames[idx])) {
			sftps_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
			    "appendcstring lnames[%" PRIu32 "] failed", idx);
			return false;
		}
		if (attrs[idx] == NULL) {
			lprintf(state, SSH_FX_FAILURE,
			    "Reached attrs terminator at position %" PRIu32 " of %" PRIu32,
			    idx, count);
			sftps_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
			    "attrs truncated at %" PRIu32 "/%" PRIu32, idx, count);
			return false;
		}
		if (!appendfattr(&state->priv->txp, attrs[idx])) {
			sftps_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
			    "appendfattr attrs[%" PRIu32 "] failed", idx);
			return false;
		}
	}
	return sftps_send_packet(state, out);
}

bool
sftps_send_attrs(sftps_state_t state, sftp_file_attr_t attr,
                 struct sftps_outcome *out)
{
	if (!appendheader(&state->priv->txp, SSH_FXP_ATTRS, state->priv->id) ||
	    !appendfattr(&state->priv->txp, attr)) {
		sftps_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
		    "build ATTRS reply failed");
		return false;
	}
	return sftps_send_packet(state, out);
}

static bool
sftps_reclaim(sftps_state_t state)
{
	bool ret = true;
	ret = tx_pkt_reclaim(&state->priv->txp) && ret;
	ret = rx_pkt_reclaim(&state->priv->rxp) && ret;
	return ret;
}
