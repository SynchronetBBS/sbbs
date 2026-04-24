/*
 * Part of the single-TU build.  All system and third-party includes
 * live in sftp.c; this file is #include'd from sftp.c and cannot be
 * compiled on its own.
 *
 * One pending entry per in-flight request.  Each call to a blocking
 * sftpc_* op allocates one on its stack, registers it while holding
 * state->mtx, issues the packet, and waits on its own event.  The recv
 * thread demuxes incoming packets by req_id and delivers the raw reply
 * here.
 */
struct sftpc_pending {
	uint32_t req_id;
	xpevent_t evt;
	sftp_rx_pkt_t reply;
	struct sftpc_pending *prev;
	struct sftpc_pending *next;
	bool delivered;
	bool aborted;
};

struct sftp_client_state {
	bool (*send_cb)(uint8_t *buf, size_t len, void *cb_data);
	sftp_rx_pkt_t rxp;
	sftp_tx_pkt_t txp;
	void *cb_data;
	pthread_key_t err_key;
	struct sftpc_pending *pending;
	pthread_mutex_t mtx;
	uint32_t version;
	uint32_t running;
	uint32_t id;
	uint32_t extensions;
	bool terminating;
	bool err_key_ok;
};

/*
 * Per-state enter/exit around the state mutex.  Holds the lock for
 * the full duration of an op including any callback (no callbacks
 * today on the client, but the contract is the same as the server).
 * send_and_wait below releases and reacquires the mutex around the
 * WaitForEvent so the recv thread can deliver replies.
 */
static bool
client_enter(sftpc_state_t state)
{
	assert(state);
	if (state == NULL)
		return false;
	if (pthread_mutex_lock(&state->mtx))
		return false;
	if (state->terminating) {
		pthread_mutex_unlock(&state->mtx);
		return false;
	}
	state->running++;
	return true;
}

static bool
client_exit(sftpc_state_t state, bool retval)
{
	assert(state->running > 0);
	state->running--;
	pthread_mutex_unlock(&state->mtx);
	return retval;
}

/* Per-thread last-error stored as uintptr_t in a pthread key so callers
 * on different threads don't clobber each other's status. */
static void
set_thread_err(sftpc_state_t state, uint32_t code)
{
	if (state->err_key_ok)
		pthread_setspecific(state->err_key, (void *)(uintptr_t)code);
}

static uint32_t
get_thread_err(sftpc_state_t state)
{
	if (!state->err_key_ok)
		return SSH_FX_FAILURE;
	return (uint32_t)(uintptr_t)pthread_getspecific(state->err_key);
}

/* Pending-list helpers; state->mtx must be held. */
static void
pend_add(sftpc_state_t state, struct sftpc_pending *p)
{
	p->prev = NULL;
	p->next = state->pending;
	if (state->pending)
		state->pending->prev = p;
	state->pending = p;
}

static void
pend_remove(sftpc_state_t state, struct sftpc_pending *p)
{
	if (p->prev)
		p->prev->next = p->next;
	else
		state->pending = p->next;
	if (p->next)
		p->next->prev = p->prev;
}

static struct sftpc_pending *
pend_find(sftpc_state_t state, uint32_t req_id)
{
	for (struct sftpc_pending *p = state->pending; p != NULL; p = p->next) {
		if (p->req_id == req_id)
			return p;
	}
	return NULL;
}

/*
 * Precondition: state->mtx is held (via client_enter); state->txp has
 * the request packet built in it; `p` is already in the pending list.
 *
 * Sends the packet, releases state->mtx around the wait, reacquires it.
 * Returns true if a reply was delivered into p->reply; false on send
 * failure or abort.
 */
static bool
send_and_wait(sftpc_state_t state, struct sftpc_pending *p)
{
	uint8_t *buf;
	size_t sz;
	if (!prep_tx_packet(state->txp, &buf, &sz))
		return false;
	if (!state->send_cb(buf, sz, state->cb_data))
		return false;
	pthread_mutex_unlock(&state->mtx);
	WaitForEvent(p->evt, INFINITE);
	pthread_mutex_lock(&state->mtx);
	return p->delivered && !p->aborted;
}

/* Allocates the next request id.  Skips 0, which is reserved for the
 * INIT/VERSION exchange (which carries no req_id on the wire). */
static uint32_t
next_req_id(sftpc_state_t state)
{
	if (++state->id == 0)
		++state->id;
	return state->id;
}

static bool
status_ok(sftpc_state_t state, sftp_rx_pkt_t reply)
{
	if (reply->type != SSH_FXP_STATUS)
		return false;
	uint32_t code = get32(reply);
	set_thread_err(state, code);
	return code == SSH_FX_OK;
}

/* ========================================================================
 * Public API
 * ======================================================================== */

sftpc_state_t
sftpc_begin(bool (*send_cb)(uint8_t *buf, size_t len, void *cb_data), void *cb_data)
{
	sftpc_state_t ret = (sftpc_state_t)calloc(1, sizeof(struct sftp_client_state));
	if (ret == NULL)
		return NULL;
	ret->send_cb = send_cb;
	ret->cb_data = cb_data;
	pthread_mutex_init(&ret->mtx, NULL);
	if (pthread_key_create(&ret->err_key, NULL) == 0)
		ret->err_key_ok = true;
	return ret;
}

void
sftpc_finish(sftpc_state_t state)
{
	if (state == NULL)
		return;
	pthread_mutex_lock(&state->mtx);
	if (state->terminating) {
		pthread_mutex_unlock(&state->mtx);
		return;
	}
	state->terminating = true;
	/* Wake every waiter.  They'll observe aborted=true and unwind. */
	for (struct sftpc_pending *p = state->pending; p != NULL; p = p->next) {
		p->aborted = true;
		SetEvent(p->evt);
	}
	while (state->running) {
		pthread_mutex_unlock(&state->mtx);
		SLEEP(1);
		pthread_mutex_lock(&state->mtx);
	}
	free_rx_pkt(state->rxp);
	state->rxp = NULL;
	free_tx_pkt(state->txp);
	state->txp = NULL;
	pthread_mutex_unlock(&state->mtx);
}

void
sftpc_end(sftpc_state_t state)
{
	if (state == NULL)
		return;
	if (!state->terminating)
		sftpc_finish(state);
	if (state->err_key_ok)
		pthread_key_delete(state->err_key);
	pthread_mutex_destroy(&state->mtx);
	free(state);
}

bool
sftpc_recv(sftpc_state_t state, uint8_t *buf, uint32_t sz)
{
	if (!client_enter(state))
		return false;
	if (!rx_pkt_append(&state->rxp, buf, sz))
		return client_exit(state, false);
	while (state->rxp != NULL && have_full_pkt(state->rxp)) {
		sftp_rx_pkt_t completed = extract_packet(state->rxp);
		if (completed == NULL)
			break;
		uint32_t req_id = (completed->type == SSH_FXP_VERSION) ? 0
		                                                      : get32(completed);
		struct sftpc_pending *p = pend_find(state, req_id);
		if (p != NULL) {
			p->reply = completed;
			p->delivered = true;
			SetEvent(p->evt);
		}
		else {
			free_rx_pkt(completed);
		}
	}
	return client_exit(state, true);
}

bool
sftpc_init(sftpc_state_t state)
{
	if (!client_enter(state))
		return false;

	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL)
		return client_exit(state, false);
	pend_add(state, &p);

	/* req_id = 0 for INIT; server VERSION response is dispatched to id 0. */
	p.req_id = 0;

	bool ok = tx_pkt_reset(&state->txp) &&
	          appendbyte(&state->txp, SSH_FXP_INIT) &&
	          append32(&state->txp, SFTP_VERSION) &&
	          append_extensions(&state->txp, SFTP_EXT_ALL) &&
	          send_and_wait(state, &p);

	bool rv = false;
	if (ok && p.reply != NULL && p.reply->type == SSH_FXP_VERSION) {
		state->version = get32(p.reply);
		state->extensions = 0;
		uint32_t payload_len = pkt_sz(p.reply) - 1;
		while (p.reply->cur + sizeof(uint32_t) <= payload_len) {
			sftp_str_t ext_name = getstring(p.reply);
			sftp_str_t ext_data = getstring(p.reply);
			if (ext_name == NULL || ext_data == NULL) {
				free_sftp_str(ext_name);
				free_sftp_str(ext_data);
				break;
			}
			state->extensions |= extension_match(ext_name, ext_data) & SFTP_EXT_ALL;
			free_sftp_str(ext_name);
			free_sftp_str(ext_data);
		}
		rv = state->version <= SFTP_VERSION;
	}
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	return client_exit(state, rv);
}

uint32_t
sftpc_get_err(sftpc_state_t state)
{
	if (state == NULL)
		return SSH_FX_FAILURE;
	return get_thread_err(state);
}

uint32_t
sftpc_get_extensions(sftpc_state_t state)
{
	if (state == NULL)
		return 0;
	return state->extensions;
}

bool
sftpc_reclaim(sftpc_state_t state)
{
	if (!client_enter(state))
		return false;
	bool ret = true;
	ret = tx_pkt_reclaim(&state->txp) && ret;
	ret = rx_pkt_reclaim(&state->rxp) && ret;
	return client_exit(state, ret);
}

/*
 * Shared op scaffolding — each caller builds its packet into state->txp
 * via the sftp_static.h helpers, then calls send_and_wait.
 *
 * Every op:
 *   1. client_enter (locks mtx, running++)
 *   2. allocate req_id and register pending
 *   3. appendheader + fields
 *   4. send_and_wait (unlocks+relocks mtx around the WaitForEvent)
 *   5. parse reply, unregister pending, return under client_exit
 *
 * Keeping the sequence inline in each op is more lines than a callback
 * indirection, but it's straight-through code and matches the existing
 * appendXxx helper style.
 */

bool
sftpc_realpath(sftpc_state_t state, char *path, sftp_str_t *ret)
{
	if (path == NULL || ret == NULL || *ret != NULL)
		return false;
	if (!client_enter(state))
		return false;

	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL)
		return client_exit(state, false);
	p.req_id = next_req_id(state);
	pend_add(state, &p);

	bool ok = appendheader(&state->txp, SSH_FXP_REALPATH, state->id) &&
	          appendcstring(&state->txp, path) &&
	          send_and_wait(state, &p);
	bool rv = false;
	if (ok && p.reply != NULL) {
		if (p.reply->type == SSH_FXP_NAME && get32(p.reply) == 1) {
			*ret = getstring(p.reply);
			rv = (*ret != NULL);
		}
		else if (p.reply->type == SSH_FXP_STATUS) {
			status_ok(state, p.reply);
		}
	}
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	return client_exit(state, rv);
}

/* Opens a file or directory (path + optional flags/attrs for SSH_FXP_OPEN). */
static bool
do_open(sftpc_state_t state, uint8_t type, const char *path, uint32_t flags,
        sftp_file_attr_t attr, sftp_str_t *handle_out)
{
	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL)
		return false;
	p.req_id = next_req_id(state);
	pend_add(state, &p);

	bool built = appendheader(&state->txp, type, state->id) &&
	             appendcstring(&state->txp, path);
	if (built && type == SSH_FXP_OPEN) {
		sftp_file_attr_t a = attr;
		bool need_free = false;
		if (a == NULL) {
			a = sftp_fattr_alloc();
			need_free = (a != NULL);
		}
		built = a != NULL && append32(&state->txp, flags) && appendfattr(&state->txp, a);
		if (need_free)
			sftp_fattr_free(a);
	}
	bool rv = false;
	if (built && send_and_wait(state, &p) && p.reply != NULL) {
		if (p.reply->type == SSH_FXP_HANDLE) {
			*handle_out = getstring(p.reply);
			rv = (*handle_out != NULL);
		}
		else if (p.reply->type == SSH_FXP_STATUS) {
			status_ok(state, p.reply);
		}
	}
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	return rv;
}

bool
sftpc_open(sftpc_state_t state, char *path, uint32_t flags, sftp_file_attr_t attr,
           sftp_dirhandle_t *handle)
{
	if (path == NULL || handle == NULL || *handle != NULL)
		return false;
	if (!client_enter(state))
		return false;
	return client_exit(state, do_open(state, SSH_FXP_OPEN, path, flags, attr,
	                                    (sftp_str_t *)handle));
}

bool
sftpc_opendir(sftpc_state_t state, const char *path, sftp_dirhandle_t *handle)
{
	if (path == NULL || handle == NULL || *handle != NULL)
		return false;
	if (!client_enter(state))
		return false;
	return client_exit(state, do_open(state, SSH_FXP_OPENDIR, path, 0, NULL,
	                                    (sftp_str_t *)handle));
}

bool
sftpc_close(sftpc_state_t state, sftp_filehandle_t *handle)
{
	if (handle == NULL || *handle == NULL)
		return false;
	if (!client_enter(state))
		return false;

	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL)
		return client_exit(state, false);
	p.req_id = next_req_id(state);
	pend_add(state, &p);

	bool ok = appendheader(&state->txp, SSH_FXP_CLOSE, state->id) &&
	          appendstring(&state->txp, (sftp_str_t)*handle) &&
	          send_and_wait(state, &p);
	bool rv = ok && p.reply != NULL && status_ok(state, p.reply);
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	free_sftp_str(*handle);
	*handle = NULL;
	return client_exit(state, rv);
}

bool
sftpc_read(sftpc_state_t state, sftp_filehandle_t handle, uint64_t offset,
           uint32_t len, sftp_str_t *ret)
{
	if (handle == NULL || ret == NULL || *ret != NULL)
		return false;
	if (!client_enter(state))
		return false;

	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL)
		return client_exit(state, false);
	p.req_id = next_req_id(state);
	pend_add(state, &p);

	bool ok = appendheader(&state->txp, SSH_FXP_READ, state->id) &&
	          appendstring(&state->txp, (sftp_str_t)handle) &&
	          append64(&state->txp, offset) &&
	          append32(&state->txp, len) &&
	          send_and_wait(state, &p);
	bool rv = false;
	if (ok && p.reply != NULL) {
		if (p.reply->type == SSH_FXP_DATA) {
			*ret = getstring(p.reply);
			rv = (*ret != NULL);
		}
		else if (p.reply->type == SSH_FXP_STATUS) {
			status_ok(state, p.reply);
		}
	}
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	return client_exit(state, rv);
}

bool
sftpc_write(sftpc_state_t state, sftp_filehandle_t handle, uint64_t offset,
            sftp_str_t data)
{
	if (handle == NULL || data == NULL)
		return false;
	if (!client_enter(state))
		return false;

	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL)
		return client_exit(state, false);
	p.req_id = next_req_id(state);
	pend_add(state, &p);

	bool ok = appendheader(&state->txp, SSH_FXP_WRITE, state->id) &&
	          appendstring(&state->txp, (sftp_str_t)handle) &&
	          append64(&state->txp, offset) &&
	          appendstring(&state->txp, data) &&
	          send_and_wait(state, &p);
	bool rv = ok && p.reply != NULL && status_ok(state, p.reply);
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	return client_exit(state, rv);
}

/* Single-path op expecting a STATUS reply (REMOVE, RMDIR, ...). */
static bool
do_one_path(sftpc_state_t state, uint8_t type, const char *path)
{
	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL)
		return false;
	p.req_id = next_req_id(state);
	pend_add(state, &p);

	bool ok = appendheader(&state->txp, type, state->id) &&
	          appendcstring(&state->txp, path) &&
	          send_and_wait(state, &p);
	bool rv = ok && p.reply != NULL && status_ok(state, p.reply);
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	return rv;
}

bool
sftpc_remove(sftpc_state_t state, const char *path)
{
	if (path == NULL)
		return false;
	if (!client_enter(state))
		return false;
	return client_exit(state, do_one_path(state, SSH_FXP_REMOVE, path));
}

bool
sftpc_rmdir(sftpc_state_t state, const char *path)
{
	if (path == NULL)
		return false;
	if (!client_enter(state))
		return false;
	return client_exit(state, do_one_path(state, SSH_FXP_RMDIR, path));
}

bool
sftpc_mkdir(sftpc_state_t state, const char *path, sftp_file_attr_t attr)
{
	if (path == NULL)
		return false;
	if (!client_enter(state))
		return false;

	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL)
		return client_exit(state, false);
	p.req_id = next_req_id(state);
	pend_add(state, &p);

	sftp_file_attr_t a = attr;
	bool need_free = false;
	if (a == NULL) {
		a = sftp_fattr_alloc();
		need_free = (a != NULL);
	}
	bool ok = a != NULL &&
	          appendheader(&state->txp, SSH_FXP_MKDIR, state->id) &&
	          appendcstring(&state->txp, path) &&
	          appendfattr(&state->txp, a) &&
	          send_and_wait(state, &p);
	if (need_free)
		sftp_fattr_free(a);
	bool rv = ok && p.reply != NULL && status_ok(state, p.reply);
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	return client_exit(state, rv);
}

bool
sftpc_rename(sftpc_state_t state, const char *from, const char *to)
{
	if (from == NULL || to == NULL)
		return false;
	if (!client_enter(state))
		return false;

	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL)
		return client_exit(state, false);
	p.req_id = next_req_id(state);
	pend_add(state, &p);

	bool ok = appendheader(&state->txp, SSH_FXP_RENAME, state->id) &&
	          appendcstring(&state->txp, from) &&
	          appendcstring(&state->txp, to) &&
	          send_and_wait(state, &p);
	bool rv = ok && p.reply != NULL && status_ok(state, p.reply);
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	return client_exit(state, rv);
}

static bool
do_stat(sftpc_state_t state, uint8_t type, const char *path,
        sftp_filehandle_t handle, sftp_file_attr_t *attr_out)
{
	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL)
		return false;
	p.req_id = next_req_id(state);
	pend_add(state, &p);

	bool ok;
	if (type == SSH_FXP_FSTAT)
		ok = appendheader(&state->txp, type, state->id) &&
		     appendstring(&state->txp, (sftp_str_t)handle);
	else
		ok = appendheader(&state->txp, type, state->id) &&
		     appendcstring(&state->txp, path);
	ok = ok && send_and_wait(state, &p);

	bool rv = false;
	if (ok && p.reply != NULL) {
		if (p.reply->type == SSH_FXP_ATTRS) {
			*attr_out = getfattr(p.reply);
			rv = (*attr_out != NULL);
		}
		else if (p.reply->type == SSH_FXP_STATUS) {
			status_ok(state, p.reply);
		}
	}
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	return rv;
}

bool
sftpc_stat(sftpc_state_t state, const char *path, sftp_file_attr_t *attr_out)
{
	if (path == NULL || attr_out == NULL || *attr_out != NULL)
		return false;
	if (!client_enter(state))
		return false;
	return client_exit(state, do_stat(state, SSH_FXP_STAT, path, NULL, attr_out));
}

bool
sftpc_lstat(sftpc_state_t state, const char *path, sftp_file_attr_t *attr_out)
{
	if (path == NULL || attr_out == NULL || *attr_out != NULL)
		return false;
	if (!client_enter(state))
		return false;
	return client_exit(state, do_stat(state, SSH_FXP_LSTAT, path, NULL, attr_out));
}

bool
sftpc_fstat(sftpc_state_t state, sftp_filehandle_t handle, sftp_file_attr_t *attr_out)
{
	if (handle == NULL || attr_out == NULL || *attr_out != NULL)
		return false;
	if (!client_enter(state))
		return false;
	return client_exit(state, do_stat(state, SSH_FXP_FSTAT, NULL, handle, attr_out));
}

static bool
do_setstat(sftpc_state_t state, uint8_t type, const char *path,
           sftp_filehandle_t handle, sftp_file_attr_t attr)
{
	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL)
		return false;
	p.req_id = next_req_id(state);
	pend_add(state, &p);

	bool ok;
	if (type == SSH_FXP_FSETSTAT)
		ok = appendheader(&state->txp, type, state->id) &&
		     appendstring(&state->txp, (sftp_str_t)handle);
	else
		ok = appendheader(&state->txp, type, state->id) &&
		     appendcstring(&state->txp, path);
	ok = ok && appendfattr(&state->txp, attr) && send_and_wait(state, &p);
	bool rv = ok && p.reply != NULL && status_ok(state, p.reply);
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	return rv;
}

bool
sftpc_setstat(sftpc_state_t state, const char *path, sftp_file_attr_t attr)
{
	if (path == NULL || attr == NULL)
		return false;
	if (!client_enter(state))
		return false;
	return client_exit(state, do_setstat(state, SSH_FXP_SETSTAT, path, NULL, attr));
}

bool
sftpc_fsetstat(sftpc_state_t state, sftp_filehandle_t handle, sftp_file_attr_t attr)
{
	if (handle == NULL || attr == NULL)
		return false;
	if (!client_enter(state))
		return false;
	return client_exit(state, do_setstat(state, SSH_FXP_FSETSTAT, NULL, handle, attr));
}

void
sftpc_free_dir_entries(struct sftpc_dir_entry *entries, uint32_t count)
{
	if (entries == NULL)
		return;
	for (uint32_t i = 0; i < count; i++) {
		free_sftp_str(entries[i].filename);
		free_sftp_str(entries[i].longname);
		sftp_fattr_free(entries[i].attrs);
	}
	free(entries);
}

bool
sftpc_readdir(sftpc_state_t state, sftp_dirhandle_t handle,
              struct sftpc_dir_entry **entries_out, uint32_t *count_out, bool *eof_out)
{
	if (handle == NULL || entries_out == NULL || count_out == NULL || eof_out == NULL)
		return false;
	*entries_out = NULL;
	*count_out = 0;
	*eof_out = false;
	if (!client_enter(state))
		return false;

	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL)
		return client_exit(state, false);
	p.req_id = next_req_id(state);
	pend_add(state, &p);

	bool ok = appendheader(&state->txp, SSH_FXP_READDIR, state->id) &&
	          appendstring(&state->txp, (sftp_str_t)handle) &&
	          send_and_wait(state, &p);
	bool rv = false;
	if (ok && p.reply != NULL) {
		if (p.reply->type == SSH_FXP_NAME) {
			uint32_t n = get32(p.reply);
			struct sftpc_dir_entry *entries = NULL;
			if (n > 0) {
				entries = (struct sftpc_dir_entry *)calloc(n, sizeof(*entries));
				if (entries == NULL)
					goto done;
			}
			rv = true;
			for (uint32_t i = 0; i < n; i++) {
				entries[i].filename = getstring(p.reply);
				entries[i].longname = getstring(p.reply);
				entries[i].attrs = getfattr(p.reply);
				if (entries[i].filename == NULL ||
				    entries[i].longname == NULL ||
				    entries[i].attrs == NULL) {
					sftpc_free_dir_entries(entries, i + 1);
					entries = NULL;
					rv = false;
					break;
				}
			}
			if (rv) {
				*entries_out = entries;
				*count_out = n;
			}
		}
		else if (p.reply->type == SSH_FXP_STATUS) {
			uint32_t code = get32(p.reply);
			set_thread_err(state, code);
			if (code == SSH_FX_EOF) {
				*eof_out = true;
				rv = true;
			}
		}
	}
done:
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	return client_exit(state, rv);
}
