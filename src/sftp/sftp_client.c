/*
 * Part of the single-TU build.  All system and third-party includes
 * live in sftp.c; this file is #include'd from sftp.c and cannot be
 * compiled on its own.
 *
 * Public op contract:
 *   true  — the RPC happened.  The caller acts on out->result (an
 *           SSH_FX_* code from the server's reply, or SSH_FX_OK if the
 *           reply was a typed payload like SSH_FXP_HANDLE).  out->err
 *           and out->estr are diagnostic only; mustn't drive control
 *           flow.
 *   false — the RPC didn't happen.  out->result is undefined.  out->err
 *           is a machine-readable lib failure code (see
 *           sftp_err_code_t).  out->estr is human-readable text ready
 *           to display.
 *
 * Pass NULL for `out` to ignore everything.
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
	struct sftpc_pending *pending;
	pthread_mutex_t mtx;
	uint32_t version;
	uint32_t running;
	uint32_t id;
	/* `extensions` is set only inside sftpc_init under mtx and read
	 * thereafter without mtx by sftpc_get_extensions.  Callers must
	 * complete sftpc_init before any other op or call to get_extensions
	 * (init runs to completion before queue/browser threads start in
	 * SyncTERM today). */
	uint32_t extensions;
	bool terminating;
};

/*
 * Per-state enter/exit around the state mutex.  Holds the lock for
 * the full duration of an op including any callback (no callbacks
 * today on the client, but the contract is the same as the server).
 * send_and_wait below releases and reacquires the mutex around the
 * WaitForEvent so the recv thread can deliver replies.
 */
static bool
client_enter(sftpc_state_t state, struct sftpc_outcome *out)
{
	if (state == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_STATE, "state == NULL");
		return false;
	}
	if (pthread_mutex_lock(&state->mtx)) {
		sftpc_outcome_record(out, SFTP_ERR_OOM,
		    "pthread_mutex_lock failed");
		return false;
	}
	if (state->terminating) {
		pthread_mutex_unlock(&state->mtx);
		sftpc_outcome_record(out, SFTP_ERR_ABORTED,
		    "session terminating");
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
 * Returns true if a reply was delivered into p->reply; false on any
 * failure (records out->err + estr describing what failed).
 */
static bool
send_and_wait(sftpc_state_t state, struct sftpc_pending *p,
              struct sftpc_outcome *out)
{
	uint8_t *buf;
	size_t sz;
	if (!prep_tx_packet(state->txp, &buf, &sz)) {
		sftpc_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
		    "prep_tx_packet failed");
		return false;
	}
	if (!state->send_cb(buf, sz, state->cb_data)) {
		sftpc_outcome_record(out, SFTP_ERR_SEND_FAILED,
		    "send_cb failed");
		return false;
	}
	pthread_mutex_unlock(&state->mtx);
	WaitForEvent(p->evt, INFINITE);
	pthread_mutex_lock(&state->mtx);
	if (p->aborted) {
		sftpc_outcome_record(out, SFTP_ERR_ABORTED,
		    "aborted while waiting for reply");
		return false;
	}
	if (!p->delivered || p->reply == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_CHANNEL_CLOSED,
		    "no reply delivered");
		return false;
	}
	return true;
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

/*
 * Parse a SSH_FXP_STATUS reply: extract the result code (into
 * out->result) and the optional UTF-8 message + lang tag (into
 * out->estr as a `Reply: "..."` line if non-empty).  Returns true if
 * the reply was a STATUS packet; false if it was anything else (caller
 * decides what to do).
 */
static bool
parse_status(sftp_rx_pkt_t reply, struct sftpc_outcome *out)
{
	if (reply->type != SSH_FXP_STATUS)
		return false;
	uint32_t code = get32(reply);
	if (out != NULL)
		out->result = code;
	sftp_str_t msg = getstring(reply);
	sftp_str_t lang = getstring(reply);
	if (msg != NULL && msg->len > 0) {
		sftpc_outcome_reply(out,
		    (const char *)msg->c_str, msg->len,
		    lang ? (const char *)lang->c_str : "",
		    lang ? lang->len : 0);
	}
	free_sftp_str(msg);
	free_sftp_str(lang);
	return true;
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
	pthread_mutex_destroy(&state->mtx);
	free(state);
}

bool
sftpc_recv(sftpc_state_t state, uint8_t *buf, uint32_t sz)
{
	if (!client_enter(state, NULL))
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
	if (!client_enter(state, NULL))
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
	          send_and_wait(state, &p, NULL);

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
sftpc_get_extensions(sftpc_state_t state)
{
	if (state == NULL)
		return 0;
	return state->extensions;
}

/*
 * Shared op scaffolding — each op:
 *   1. arg-check + record SFTP_ERR_NULL_* into outcome on bad args
 *   2. client_enter (locks mtx, running++, records err if state bad)
 *   3. allocate req_id and register pending
 *   4. appendheader + fields
 *   5. send_and_wait (unlocks+relocks mtx around the WaitForEvent;
 *      records err on transport / abort)
 *   6. inspect reply, populate out->result / data, unregister pending
 *   7. return under client_exit
 */

bool
sftpc_realpath(sftpc_state_t state, const char *path, sftp_str_t *ret,
               struct sftpc_outcome *out)
{
	if (path == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_PATH, "path == NULL");
		return false;
	}
	if (ret == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_OUT, "ret == NULL");
		return false;
	}
	if (*ret != NULL) {
		sftpc_outcome_record(out, SFTP_ERR_HANDLE_NOT_NULL,
		    "*ret must be NULL on entry");
		return false;
	}
	if (!client_enter(state, out))
		return false;

	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_OOM, "CreateEvent failed");
		return client_exit(state, false);
	}
	p.req_id = next_req_id(state);
	pend_add(state, &p);

	bool rv = false;
	if (!appendheader(&state->txp, SSH_FXP_REALPATH, state->id) ||
	    !appendcstring(&state->txp, path)) {
		sftpc_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
		    "appendheader/appendcstring failed");
	}
	else if (send_and_wait(state, &p, out)) {
		if (p.reply->type == SSH_FXP_NAME && get32(p.reply) == 1) {
			*ret = getstring(p.reply);
			if (*ret == NULL)
				sftpc_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
				    "getstring failed on NAME reply");
			else
				rv = true;
		}
		else if (p.reply->type == SSH_FXP_STATUS) {
			parse_status(p.reply, out);
			rv = true;
		}
		else {
			sftpc_outcome_record(out, SFTP_ERR_REPLY_WRONG_TYPE,
			    "expected SSH_FXP_NAME or SSH_FXP_STATUS, got %s",
			    sftp_get_type_name(p.reply->type));
		}
	}
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	return client_exit(state, rv);
}

/* Opens a file or directory (path + optional flags for SSH_FXP_OPEN). */
static bool
do_open(sftpc_state_t state, uint8_t type, const char *path, uint32_t flags,
        sftp_str_t *handle_out, struct sftpc_outcome *out)
{
	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_OOM, "CreateEvent failed");
		return false;
	}
	p.req_id = next_req_id(state);
	pend_add(state, &p);

	bool built = appendheader(&state->txp, type, state->id) &&
	             appendcstring(&state->txp, path);
	if (built && type == SSH_FXP_OPEN) {
		sftp_file_attr_t a = sftp_fattr_alloc();
		if (a == NULL) {
			built = false;
		}
		else {
			built = append32(&state->txp, flags) && appendfattr(&state->txp, a);
			sftp_fattr_free(a);
		}
	}

	bool rv = false;
	if (!built) {
		sftpc_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
		    "packet build failed");
	}
	else if (send_and_wait(state, &p, out)) {
		if (p.reply->type == SSH_FXP_HANDLE) {
			*handle_out = getstring(p.reply);
			if (*handle_out == NULL)
				sftpc_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
				    "getstring failed on HANDLE reply");
			else
				rv = true;
		}
		else if (p.reply->type == SSH_FXP_STATUS) {
			parse_status(p.reply, out);
			rv = true;
		}
		else {
			sftpc_outcome_record(out, SFTP_ERR_REPLY_WRONG_TYPE,
			    "expected SSH_FXP_HANDLE or SSH_FXP_STATUS, got %s",
			    sftp_get_type_name(p.reply->type));
		}
	}
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	return rv;
}

bool
sftpc_open(sftpc_state_t state, const char *path, uint32_t flags,
           sftp_str_t *handle, struct sftpc_outcome *out)
{
	if (path == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_PATH, "path == NULL");
		return false;
	}
	if (handle == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_HANDLE_OUT,
		    "handle == NULL");
		return false;
	}
	if (*handle != NULL) {
		sftpc_outcome_record(out, SFTP_ERR_HANDLE_NOT_NULL,
		    "*handle must be NULL on entry");
		return false;
	}
	if (!client_enter(state, out))
		return false;
	return client_exit(state, do_open(state, SSH_FXP_OPEN, path, flags,
	                                  handle, out));
}

bool
sftpc_opendir(sftpc_state_t state, const char *path, sftp_str_t *handle,
              struct sftpc_outcome *out)
{
	if (path == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_PATH, "path == NULL");
		return false;
	}
	if (handle == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_HANDLE_OUT,
		    "handle == NULL");
		return false;
	}
	if (*handle != NULL) {
		sftpc_outcome_record(out, SFTP_ERR_HANDLE_NOT_NULL,
		    "*handle must be NULL on entry");
		return false;
	}
	if (!client_enter(state, out))
		return false;
	return client_exit(state, do_open(state, SSH_FXP_OPENDIR, path, 0,
	                                  handle, out));
}

bool
sftpc_close(sftpc_state_t state, sftp_str_t *handle, struct sftpc_outcome *out)
{
	if (handle == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_HANDLE_OUT,
		    "handle == NULL");
		return false;
	}
	if (*handle == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_HANDLE,
		    "*handle == NULL");
		return false;
	}
	if (!client_enter(state, out))
		return false;

	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_OOM, "CreateEvent failed");
		return client_exit(state, false);
	}
	p.req_id = next_req_id(state);
	pend_add(state, &p);

	bool rv = false;
	if (!appendheader(&state->txp, SSH_FXP_CLOSE, state->id) ||
	    !appendstring(&state->txp, *handle)) {
		sftpc_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
		    "appendheader/appendstring failed");
	}
	else if (send_and_wait(state, &p, out)) {
		if (parse_status(p.reply, out)) {
			rv = true;
		}
		else {
			sftpc_outcome_record(out, SFTP_ERR_REPLY_WRONG_TYPE,
			    "expected SSH_FXP_STATUS, got %s",
			    sftp_get_type_name(p.reply->type));
		}
	}
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	free_sftp_str(*handle);
	*handle = NULL;
	return client_exit(state, rv);
}

bool
sftpc_read(sftpc_state_t state, sftp_str_t handle, uint64_t offset,
           uint32_t len, sftp_str_t *ret, struct sftpc_outcome *out)
{
	if (handle == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_HANDLE,
		    "handle == NULL");
		return false;
	}
	if (ret == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_OUT, "ret == NULL");
		return false;
	}
	if (*ret != NULL) {
		sftpc_outcome_record(out, SFTP_ERR_HANDLE_NOT_NULL,
		    "*ret must be NULL on entry");
		return false;
	}
	if (!client_enter(state, out))
		return false;

	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_OOM, "CreateEvent failed");
		return client_exit(state, false);
	}
	p.req_id = next_req_id(state);
	pend_add(state, &p);

	bool rv = false;
	if (!appendheader(&state->txp, SSH_FXP_READ, state->id) ||
	    !appendstring(&state->txp, handle) ||
	    !append64(&state->txp, offset) ||
	    !append32(&state->txp, len)) {
		sftpc_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
		    "appendheader/append failed");
	}
	else if (send_and_wait(state, &p, out)) {
		if (p.reply->type == SSH_FXP_DATA) {
			*ret = getstring(p.reply);
			if (*ret == NULL)
				sftpc_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
				    "getstring failed on DATA reply");
			else
				rv = true;
		}
		else if (p.reply->type == SSH_FXP_STATUS) {
			parse_status(p.reply, out);
			rv = true;
		}
		else {
			sftpc_outcome_record(out, SFTP_ERR_REPLY_WRONG_TYPE,
			    "expected SSH_FXP_DATA or SSH_FXP_STATUS, got %s",
			    sftp_get_type_name(p.reply->type));
		}
	}
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	return client_exit(state, rv);
}

bool
sftpc_write(sftpc_state_t state, sftp_str_t handle, uint64_t offset,
            sftp_str_t data, struct sftpc_outcome *out)
{
	if (handle == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_HANDLE,
		    "handle == NULL");
		return false;
	}
	if (data == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_DATA, "data == NULL");
		return false;
	}
	if (!client_enter(state, out))
		return false;

	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_OOM, "CreateEvent failed");
		return client_exit(state, false);
	}
	p.req_id = next_req_id(state);
	pend_add(state, &p);

	bool rv = false;
	if (!appendheader(&state->txp, SSH_FXP_WRITE, state->id) ||
	    !appendstring(&state->txp, handle) ||
	    !append64(&state->txp, offset) ||
	    !appendstring(&state->txp, data)) {
		sftpc_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
		    "appendheader/append failed");
	}
	else if (send_and_wait(state, &p, out)) {
		if (parse_status(p.reply, out)) {
			rv = true;
		}
		else {
			sftpc_outcome_record(out, SFTP_ERR_REPLY_WRONG_TYPE,
			    "expected SSH_FXP_STATUS, got %s",
			    sftp_get_type_name(p.reply->type));
		}
	}
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	return client_exit(state, rv);
}

bool
sftpc_stat(sftpc_state_t state, const char *path, sftp_file_attr_t *attr_out,
           struct sftpc_outcome *out)
{
	if (path == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_PATH, "path == NULL");
		return false;
	}
	if (attr_out == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_OUT, "attr_out == NULL");
		return false;
	}
	if (*attr_out != NULL) {
		sftpc_outcome_record(out, SFTP_ERR_HANDLE_NOT_NULL,
		    "*attr_out must be NULL on entry");
		return false;
	}
	if (!client_enter(state, out))
		return false;

	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_OOM, "CreateEvent failed");
		return client_exit(state, false);
	}
	p.req_id = next_req_id(state);
	pend_add(state, &p);

	bool rv = false;
	if (!appendheader(&state->txp, SSH_FXP_STAT, state->id) ||
	    !appendcstring(&state->txp, path)) {
		sftpc_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
		    "appendheader/appendcstring failed");
	}
	else if (send_and_wait(state, &p, out)) {
		if (p.reply->type == SSH_FXP_ATTRS) {
			*attr_out = getfattr(p.reply);
			if (*attr_out == NULL)
				sftpc_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
				    "getfattr failed on ATTRS reply");
			else
				rv = true;
		}
		else if (p.reply->type == SSH_FXP_STATUS) {
			parse_status(p.reply, out);
			rv = true;
		}
		else {
			sftpc_outcome_record(out, SFTP_ERR_REPLY_WRONG_TYPE,
			    "expected SSH_FXP_ATTRS or SSH_FXP_STATUS, got %s",
			    sftp_get_type_name(p.reply->type));
		}
	}
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	return client_exit(state, rv);
}

bool
sftpc_setstat(sftpc_state_t state, const char *path, sftp_file_attr_t attr,
              struct sftpc_outcome *out)
{
	if (path == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_PATH, "path == NULL");
		return false;
	}
	if (attr == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_DATA, "attr == NULL");
		return false;
	}
	if (!client_enter(state, out))
		return false;

	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_OOM, "CreateEvent failed");
		return client_exit(state, false);
	}
	p.req_id = next_req_id(state);
	pend_add(state, &p);

	bool rv = false;
	if (!appendheader(&state->txp, SSH_FXP_SETSTAT, state->id) ||
	    !appendcstring(&state->txp, path) ||
	    !appendfattr(&state->txp, attr)) {
		sftpc_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
		    "appendheader/append failed");
	}
	else if (send_and_wait(state, &p, out)) {
		if (parse_status(p.reply, out)) {
			rv = true;
		}
		else {
			sftpc_outcome_record(out, SFTP_ERR_REPLY_WRONG_TYPE,
			    "expected SSH_FXP_STATUS, got %s",
			    sftp_get_type_name(p.reply->type));
		}
	}
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	return client_exit(state, rv);
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
sftpc_readdir(sftpc_state_t state, sftp_str_t handle,
              struct sftpc_dir_entry **entries_out, uint32_t *count_out,
              bool *eof_out, struct sftpc_outcome *out)
{
	if (handle == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_HANDLE,
		    "handle == NULL");
		return false;
	}
	if (entries_out == NULL || count_out == NULL || eof_out == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_OUT,
		    "entries_out/count_out/eof_out NULL");
		return false;
	}
	*entries_out = NULL;
	*count_out = 0;
	*eof_out = false;
	if (!client_enter(state, out))
		return false;

	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_OOM, "CreateEvent failed");
		return client_exit(state, false);
	}
	p.req_id = next_req_id(state);
	pend_add(state, &p);

	bool rv = false;
	if (!appendheader(&state->txp, SSH_FXP_READDIR, state->id) ||
	    !appendstring(&state->txp, handle)) {
		sftpc_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
		    "appendheader/appendstring failed");
	}
	else if (send_and_wait(state, &p, out)) {
		if (p.reply->type == SSH_FXP_NAME) {
			uint32_t n = get32(p.reply);
			struct sftpc_dir_entry *entries = NULL;
			if (n > 0) {
				entries = (struct sftpc_dir_entry *)calloc(n, sizeof(*entries));
				if (entries == NULL) {
					sftpc_outcome_record(out, SFTP_ERR_OOM,
					    "calloc(%" PRIu32 " entries) failed", n);
					goto done;
				}
			}
			rv = true;
			for (uint32_t i = 0; i < n; i++) {
				entries[i].filename = getstring(p.reply);
				entries[i].longname = getstring(p.reply);
				entries[i].attrs = getfattr(p.reply);
				if (entries[i].filename == NULL ||
				    entries[i].longname == NULL ||
				    entries[i].attrs == NULL) {
					sftpc_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
					    "getstring/getfattr failed at entry %" PRIu32, i);
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
			parse_status(p.reply, out);
			if (out != NULL && out->result == SSH_FX_EOF)
				*eof_out = true;
			rv = true;
		}
		else {
			sftpc_outcome_record(out, SFTP_ERR_REPLY_WRONG_TYPE,
			    "expected SSH_FXP_NAME or SSH_FXP_STATUS, got %s",
			    sftp_get_type_name(p.reply->type));
		}
	}
done:
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	return client_exit(state, rv);
}

/*
 * Query the extended description for a file via the descs@syncterm.net
 * extension.  The caller is expected to have checked
 * sftpc_get_extensions(state) & SFTP_EXT_DESCS before calling; if the
 * extension wasn't negotiated the server will reply with OP_UNSUPPORTED.
 */
bool
sftpc_descs(sftpc_state_t state, const char *path, sftp_str_t *desc,
            struct sftpc_outcome *out)
{
	if (path == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_PATH, "path == NULL");
		return false;
	}
	if (desc == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_NULL_OUT, "desc == NULL");
		return false;
	}
	if (*desc != NULL) {
		sftpc_outcome_record(out, SFTP_ERR_HANDLE_NOT_NULL,
		    "*desc must be NULL on entry");
		return false;
	}
	if (!client_enter(state, out))
		return false;

	struct sftpc_pending p = {0};
	p.evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (p.evt == NULL) {
		sftpc_outcome_record(out, SFTP_ERR_OOM, "CreateEvent failed");
		return client_exit(state, false);
	}
	p.req_id = next_req_id(state);
	pend_add(state, &p);

	bool rv = false;
	if (!appendheader(&state->txp, SSH_FXP_EXTENDED, state->id) ||
	    !appendcstring(&state->txp, SFTP_EXT_NAME_DESCS) ||
	    !appendcstring(&state->txp, path)) {
		sftpc_outcome_record(out, SFTP_ERR_PACKET_BUILD_FAILED,
		    "appendheader/appendcstring failed");
	}
	else if (send_and_wait(state, &p, out)) {
		if (p.reply->type == SSH_FXP_EXTENDED_REPLY) {
			*desc = getstring(p.reply);
			if (*desc == NULL)
				sftpc_outcome_record(out, SFTP_ERR_REPLY_BAD_STRING,
				    "getstring failed on EXTENDED_REPLY");
			else
				rv = true;
		}
		else if (p.reply->type == SSH_FXP_STATUS) {
			parse_status(p.reply, out);
			rv = true;
		}
		else {
			sftpc_outcome_record(out, SFTP_ERR_REPLY_WRONG_TYPE,
			    "expected SSH_FXP_EXTENDED_REPLY or SSH_FXP_STATUS, got %s",
			    sftp_get_type_name(p.reply->type));
		}
	}
	pend_remove(state, &p);
	free_rx_pkt(p.reply);
	CloseEvent(p.evt);
	return client_exit(state, rv);
}
