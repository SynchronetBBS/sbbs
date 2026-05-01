/*
 * Part of the single-TU build.  All system and third-party includes
 * live in sftp.c; this file is #include'd from sftp.c and cannot be
 * compiled on its own.
 *
 * Async-first model
 * -----------------
 * Each sftpc_<op>() builds + sends the request packet, registers a
 * pending entry with the (cb, cbdata) the caller provides, and returns
 * immediately.  The recv thread (sftpc_recv) demuxes incoming packets
 * by req_id, parses the reply into the typed pending's fields, and
 * invokes the callback.  Callers consume the typed fields inside the
 * callback (memcpy out, signal an event, push onto a result queue)
 * and call sftpc_pending_free() when they're done with the pending.
 *
 * Sync wrappers belong outside this library — the caller wires up an
 * event in cbdata, registers a callback that signals it, and calls
 * sftpc_<op>() followed by WaitForEvent.  See src/syncterm/sftp_wait.c
 * for the SyncTERM-side shim.
 *
 * Outcome contract (per pending fields, valid when callback fires):
 *   err == SFTP_ERR_OK and result is an SSH_FX_* code
 *     — the RPC happened.  result == SSH_FX_OK is the typed-payload
 *     case (HANDLE / DATA / NAME / ATTRS / EXTENDED_REPLY) — the
 *     typed pending's other fields are populated.  Any other SSH_FX_*
 *     means the server returned a STATUS reply with that code; the
 *     typed fields are NOT populated.
 *   err != SFTP_ERR_OK
 *     — the RPC didn't complete.  estr (if non-NULL) carries human-
 *     readable diagnostic text.
 */

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
	/* `extensions` is set only inside sftpc_init's parse callback
	 * under mtx and read thereafter without mtx by sftpc_get_extensions.
	 * Callers must wait for sftpc_init to complete before issuing other
	 * ops or calling get_extensions. */
	uint32_t extensions;
	/* Path captured from the pubdir@syncterm.net extension's data field
	 * during VERSION; same lifecycle/threading rules as `extensions`.
	 * NUL-terminated heap copy owned by the library; freed in
	 * sftpc_finish.  NULL when the extension wasn't negotiated. */
	char *pubdir;
	bool terminating;
};

/* Pending list helpers; state->mtx must be held. */
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
	p->prev = p->next = NULL;
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

static uint32_t
next_req_id(sftpc_state_t state)
{
	if (++state->id == 0)
		++state->id;
	return state->id;
}

/* Dispatch a completed pending to its callback.  The pending has
 * been removed from the list before this runs.  state->mtx is HELD
 * while the callback runs — callbacks must be quick (memcpy + signal,
 * push onto a queue, etc.) and must not issue another sftpc_* call. */
static void
pend_dispatch(struct sftpc_pending *p)
{
	if (p->parse != NULL && p->reply != NULL && !p->aborted)
		p->parse(p);
	free_rx_pkt(p->reply);
	p->reply = NULL;
	if (p->cb != NULL)
		p->cb(p);
}

/* Parse a SSH_FXP_STATUS reply: extract the result code and the
 * optional UTF-8 message + lang tag.  Returns true if the reply was a
 * STATUS packet; false if it was anything else. */
static bool
parse_status_into_pending(struct sftpc_pending *p)
{
	sftp_rx_pkt_t reply = p->reply;
	if (reply->type != SSH_FXP_STATUS)
		return false;
	p->result = get32(reply);
	sftp_str_t msg = getstring(reply);
	sftp_str_t lang = getstring(reply);
	if (msg != NULL && msg->len > 0) {
		pending_record_reply(p,
		    (const char *)msg->c_str, msg->len,
		    lang ? (const char *)lang->c_str : "",
		    lang ? lang->len : 0);
	}
	free_sftp_str(msg);
	free_sftp_str(lang);
	return true;
}

/* Per-state enter/exit around the state mutex.  `enter` records err
 * on the pending and returns false if the state isn't usable;
 * running++ keeps sftpc_finish from tearing down the state out from
 * under an in-flight front half. */
static bool
client_enter(sftpc_state_t state, struct sftpc_pending *p)
{
	if (state == NULL) {
		PENDING_RECORD(p, SFTP_ERR_NULL_STATE, "state == NULL");
		return false;
	}
	if (pthread_mutex_lock(&state->mtx)) {
		PENDING_RECORD(p, SFTP_ERR_OOM, "pthread_mutex_lock failed");
		return false;
	}
	if (state->terminating) {
		pthread_mutex_unlock(&state->mtx);
		PENDING_RECORD(p, SFTP_ERR_ABORTED, "session terminating");
		return false;
	}
	state->running++;
	return true;
}

static void
client_exit(sftpc_state_t state)
{
	assert(state->running > 0);
	state->running--;
	pthread_mutex_unlock(&state->mtx);
}

/* Common front-half scaffolding: prep the txp packet, register the
 * pending, send.  Returns true on success.  On failure, the caller
 * fires p->cb synchronously to surface the error. */
static bool
front_send(sftpc_state_t state, struct sftpc_pending *p)
{
	uint8_t *buf;
	size_t   sz;
	if (!prep_tx_packet(state->txp, &buf, &sz)) {
		PENDING_RECORD(p, SFTP_ERR_PACKET_BUILD_FAILED,
		    "prep_tx_packet failed");
		return false;
	}
	pend_add(state, p);
	if (!state->send_cb(buf, sz, state->cb_data)) {
		pend_remove(state, p);
		PENDING_RECORD(p, SFTP_ERR_SEND_FAILED, "send_cb failed");
		return false;
	}
	return true;
}

/* sftpc_free_dir_entries is the public dir-entry deallocator and is
 * also called from free_self_readdir below; forward-declare so the
 * order of definition doesn't matter. */
void sftpc_free_dir_entries(struct sftpc_dir_entry *entries, uint32_t count);

/* Per-op typed-pending free helpers — the front half stores one of
 * these in p->free_self so sftpc_pending_free can dispatch generically. */
static void
free_self_base(struct sftpc_pending *p)
{
	free(p->estr);
	free(p);
}

static void
free_self_realpath(struct sftpc_pending *base)
{
	struct sftpc_realpath_pending *p =
	    (struct sftpc_realpath_pending *)base;
	free_sftp_str(p->ret);
	free(base->estr);
	free(p);
}

static void
free_self_open(struct sftpc_pending *base)
{
	struct sftpc_open_pending *p = (struct sftpc_open_pending *)base;
	free_sftp_str(p->handle);
	free(base->estr);
	free(p);
}

static void
free_self_read(struct sftpc_pending *base)
{
	struct sftpc_read_pending *p = (struct sftpc_read_pending *)base;
	free_sftp_str(p->data);
	free(base->estr);
	free(p);
}

static void
free_self_stat(struct sftpc_pending *base)
{
	struct sftpc_stat_pending *p = (struct sftpc_stat_pending *)base;
	sftp_fattr_free(p->attrs);
	free(base->estr);
	free(p);
}

static void
free_self_readdir(struct sftpc_pending *base)
{
	struct sftpc_readdir_pending *p =
	    (struct sftpc_readdir_pending *)base;
	sftpc_free_dir_entries(p->entries, p->count);
	free(base->estr);
	free(p);
}

static void
free_self_descs(struct sftpc_pending *base)
{
	struct sftpc_descs_pending *p = (struct sftpc_descs_pending *)base;
	free_sftp_str(p->desc);
	free(base->estr);
	free(p);
}

void
sftpc_pending_free(struct sftpc_pending *p)
{
	if (p == NULL)
		return;
	if (p->free_self != NULL)
		p->free_self(p);
	else {
		free(p->estr);
		free(p);
	}
}

/* ========================================================================
 * Lifecycle
 * ======================================================================== */

sftpc_state_t
sftpc_begin(bool (*send_cb)(uint8_t *buf, size_t len, void *cb_data),
            void *cb_data)
{
	sftpc_state_t ret = (sftpc_state_t)calloc(1,
	    sizeof(struct sftp_client_state));
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
	/* Abort every in-flight pending: detach from the list, mark
	 * aborted, fire the callback so the caller can unwind.  No reply
	 * to parse — pend_dispatch skips parse when aborted is set. */
	while (state->pending != NULL) {
		struct sftpc_pending *p = state->pending;
		state->pending = p->next;
		if (state->pending != NULL)
			state->pending->prev = NULL;
		p->prev = p->next = NULL;
		p->aborted = true;
		PENDING_RECORD(p, SFTP_ERR_ABORTED, "session terminating");
		pend_dispatch(p);
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
	free(state->pubdir);
	state->pubdir = NULL;
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
	if (state == NULL)
		return false;
	if (pthread_mutex_lock(&state->mtx))
		return false;
	if (state->terminating) {
		pthread_mutex_unlock(&state->mtx);
		return false;
	}
	state->running++;

	bool ok = rx_pkt_append(&state->rxp, buf, sz);
	while (ok && state->rxp != NULL && have_full_pkt(state->rxp)) {
		sftp_rx_pkt_t completed = extract_packet(state->rxp);
		if (completed == NULL)
			break;
		uint32_t req_id = (completed->type == SSH_FXP_VERSION) ? 0
		                                                      : get32(completed);
		struct sftpc_pending *p = pend_find(state, req_id);
		if (p == NULL) {
			free_rx_pkt(completed);
			continue;
		}
		pend_remove(state, p);
		p->reply = completed;
		p->delivered = true;
		pend_dispatch(p);
	}

	state->running--;
	pthread_mutex_unlock(&state->mtx);
	return ok;
}

uint32_t
sftpc_get_extensions(sftpc_state_t state)
{
	if (state == NULL)
		return 0;
	return state->extensions;
}

const char *
sftpc_get_pubdir(sftpc_state_t state)
{
	if (state == NULL)
		return NULL;
	return state->pubdir;
}

/* ========================================================================
 * INIT / VERSION — needs access to state for the parse, so we wrap a
 * typed pending with a (state, user_cb, user_cbdata) bundle and hand
 * pend_dispatch a trampoline that swaps cbdata back to the user's
 * before firing their callback.
 * ======================================================================== */

struct init_pending {
	struct sftpc_pending  base;
	sftpc_state_t         state;
	void                (*user_cb)(struct sftpc_pending *);
	void                 *user_cbdata;
};

static void
parse_init(struct sftpc_pending *base)
{
	struct init_pending *p = (struct init_pending *)base;
	sftp_rx_pkt_t reply = base->reply;
	if (reply->type != SSH_FXP_VERSION) {
		PENDING_RECORD(base, SFTP_ERR_REPLY_WRONG_TYPE,
		    "expected SSH_FXP_VERSION, got %s",
		    sftp_get_type_name(reply->type));
		return;
	}
	p->state->version = get32(reply);
	p->state->extensions = 0;
	uint32_t payload_len = pkt_sz(reply) - 1;
	while (reply->cur + sizeof(uint32_t) <= payload_len) {
		sftp_str_t ext_name = getstring(reply);
		sftp_str_t ext_data = getstring(reply);
		if (ext_name == NULL || ext_data == NULL) {
			free_sftp_str(ext_name);
			free_sftp_str(ext_data);
			break;
		}
		p->state->extensions |=
		    extension_match(ext_name, ext_data) & SFTP_EXT_ALL;
		/* pubdir@syncterm.net is asymmetric: the server's VERSION
		 * reply puts the actual public-dir path in the data field,
		 * so extension_match() doesn't fire here.  Recognise by
		 * name and capture the data as a NUL-terminated C string,
		 * setting the bit explicitly. */
		if (p->state->pubdir == NULL &&
		    ext_name->len == strlen(SFTP_EXT_NAME_PUBDIR) &&
		    memcmp(ext_name->c_str, SFTP_EXT_NAME_PUBDIR,
		        ext_name->len) == 0) {
			char *s = malloc((size_t)ext_data->len + 1);
			if (s != NULL) {
				memcpy(s, ext_data->c_str, ext_data->len);
				s[ext_data->len] = '\0';
				p->state->pubdir = s;
				p->state->extensions |= SFTP_EXT_PUBDIR;
			}
		}
		free_sftp_str(ext_name);
		free_sftp_str(ext_data);
	}
	if (p->state->version > SFTP_VERSION) {
		PENDING_RECORD(base, SFTP_ERR_PARSE_FAILED,
		    "server VERSION %u above SFTP_VERSION %u",
		    p->state->version, SFTP_VERSION);
	}
	else {
		base->result = SSH_FX_OK;
	}
}

static void
init_trampoline(struct sftpc_pending *base)
{
	struct init_pending *p = (struct init_pending *)base;
	if (p->user_cb != NULL) {
		base->cbdata = p->user_cbdata;
		p->user_cb(base);
	}
}

static void
free_self_init(struct sftpc_pending *base)
{
	free(base->estr);
	free(base);
}

struct sftpc_pending *
sftpc_init(sftpc_state_t state, void (*cb)(struct sftpc_pending *),
           void *cbdata)
{
	struct init_pending *p = calloc(1, sizeof(*p));
	if (p == NULL)
		return NULL;
	p->base.cb = init_trampoline;
	p->base.cbdata = NULL;
	p->base.parse = parse_init;
	p->base.free_self = free_self_init;
	p->user_cb = cb;
	p->user_cbdata = cbdata;
	p->state = state;

	if (!client_enter(state, &p->base)) {
		init_trampoline(&p->base);
		return &p->base;
	}
	p->base.req_id = 0;  /* INIT/VERSION carries no id on the wire */

	if (!tx_pkt_reset(&state->txp) ||
	    !appendbyte(&state->txp, SSH_FXP_INIT) ||
	    !append32(&state->txp, SFTP_VERSION) ||
	    !append_extensions(&state->txp, SFTP_EXT_ALL)) {
		PENDING_RECORD(&p->base, SFTP_ERR_PACKET_BUILD_FAILED,
		    "init build failed");
		client_exit(state);
		init_trampoline(&p->base);
		return &p->base;
	}
	if (!front_send(state, &p->base)) {
		client_exit(state);
		init_trampoline(&p->base);
		return &p->base;
	}
	client_exit(state);
	return &p->base;
}

/* ========================================================================
 * Helper: fire the user's callback synchronously after an early
 * failure has been recorded.  Used by every front half on every
 * error path before sending — keeps the pattern uniform.
 * ======================================================================== */

static inline void
fire_sync(struct sftpc_pending *p)
{
	if (p->cb != NULL)
		p->cb(p);
}

/* ========================================================================
 * realpath
 * ======================================================================== */

static void
parse_realpath(struct sftpc_pending *base)
{
	struct sftpc_realpath_pending *p =
	    (struct sftpc_realpath_pending *)base;
	sftp_rx_pkt_t reply = base->reply;
	if (reply->type == SSH_FXP_NAME && get32(reply) == 1) {
		p->ret = getstring(reply);
		if (p->ret == NULL) {
			PENDING_RECORD(base, SFTP_ERR_REPLY_BAD_STRING,
			    "getstring failed on NAME reply");
			return;
		}
		base->result = SSH_FX_OK;
		return;
	}
	if (parse_status_into_pending(base))
		return;
	PENDING_RECORD(base, SFTP_ERR_REPLY_WRONG_TYPE,
	    "expected SSH_FXP_NAME or SSH_FXP_STATUS, got %s",
	    sftp_get_type_name(reply->type));
}

struct sftpc_realpath_pending *
sftpc_realpath(sftpc_state_t state, const char *path,
               void (*cb)(struct sftpc_pending *), void *cbdata)
{
	struct sftpc_realpath_pending *p = calloc(1, sizeof(*p));
	if (p == NULL)
		return NULL;
	p->base.cb = cb;
	p->base.cbdata = cbdata;
	p->base.parse = parse_realpath;
	p->base.free_self = free_self_realpath;

	if (path == NULL) {
		PENDING_RECORD(&p->base, SFTP_ERR_NULL_PATH, "path == NULL");
		fire_sync(&p->base);
		return p;
	}
	if (!client_enter(state, &p->base)) {
		fire_sync(&p->base);
		return p;
	}
	p->base.req_id = next_req_id(state);
	if (!appendheader(&state->txp, SSH_FXP_REALPATH, p->base.req_id) ||
	    !appendcstring(&state->txp, path)) {
		PENDING_RECORD(&p->base, SFTP_ERR_PACKET_BUILD_FAILED,
		    "appendheader/appendcstring failed");
		client_exit(state);
		fire_sync(&p->base);
		return p;
	}
	if (!front_send(state, &p->base)) {
		client_exit(state);
		fire_sync(&p->base);
		return p;
	}
	client_exit(state);
	return p;
}

/* ========================================================================
 * open / opendir
 * ======================================================================== */

static void
parse_open(struct sftpc_pending *base)
{
	struct sftpc_open_pending *p = (struct sftpc_open_pending *)base;
	sftp_rx_pkt_t reply = base->reply;
	if (reply->type == SSH_FXP_HANDLE) {
		p->handle = getstring(reply);
		if (p->handle == NULL) {
			PENDING_RECORD(base, SFTP_ERR_REPLY_BAD_STRING,
			    "getstring failed on HANDLE reply");
			return;
		}
		base->result = SSH_FX_OK;
		return;
	}
	if (parse_status_into_pending(base))
		return;
	PENDING_RECORD(base, SFTP_ERR_REPLY_WRONG_TYPE,
	    "expected SSH_FXP_HANDLE or SSH_FXP_STATUS, got %s",
	    sftp_get_type_name(reply->type));
}

static struct sftpc_open_pending *
do_open(sftpc_state_t state, uint8_t type, const char *path, uint32_t flags,
        void (*cb)(struct sftpc_pending *), void *cbdata)
{
	struct sftpc_open_pending *p = calloc(1, sizeof(*p));
	if (p == NULL)
		return NULL;
	p->base.cb = cb;
	p->base.cbdata = cbdata;
	p->base.parse = parse_open;
	p->base.free_self = free_self_open;

	if (path == NULL) {
		PENDING_RECORD(&p->base, SFTP_ERR_NULL_PATH, "path == NULL");
		fire_sync(&p->base);
		return p;
	}
	if (!client_enter(state, &p->base)) {
		fire_sync(&p->base);
		return p;
	}
	p->base.req_id = next_req_id(state);
	bool built = appendheader(&state->txp, type, p->base.req_id) &&
	             appendcstring(&state->txp, path);
	if (built && type == SSH_FXP_OPEN) {
		sftp_file_attr_t a = sftp_fattr_alloc();
		if (a == NULL) {
			built = false;
		}
		else {
			built = append32(&state->txp, flags) &&
			        appendfattr(&state->txp, a);
			sftp_fattr_free(a);
		}
	}
	if (!built) {
		PENDING_RECORD(&p->base, SFTP_ERR_PACKET_BUILD_FAILED,
		    "open build failed");
		client_exit(state);
		fire_sync(&p->base);
		return p;
	}
	if (!front_send(state, &p->base)) {
		client_exit(state);
		fire_sync(&p->base);
		return p;
	}
	client_exit(state);
	return p;
}

struct sftpc_open_pending *
sftpc_open(sftpc_state_t state, const char *path, uint32_t flags,
           void (*cb)(struct sftpc_pending *), void *cbdata)
{
	return do_open(state, SSH_FXP_OPEN, path, flags, cb, cbdata);
}

struct sftpc_open_pending *
sftpc_opendir(sftpc_state_t state, const char *path,
              void (*cb)(struct sftpc_pending *), void *cbdata)
{
	return do_open(state, SSH_FXP_OPENDIR, path, 0, cb, cbdata);
}

/* ========================================================================
 * close / write / setstat — STATUS-only replies share parse_status_only
 * ======================================================================== */

static void
parse_status_only(struct sftpc_pending *p)
{
	if (parse_status_into_pending(p))
		return;
	PENDING_RECORD(p, SFTP_ERR_REPLY_WRONG_TYPE,
	    "expected SSH_FXP_STATUS, got %s",
	    sftp_get_type_name(p->reply->type));
}

struct sftpc_pending *
sftpc_close(sftpc_state_t state, sftp_str_t handle,
            void (*cb)(struct sftpc_pending *), void *cbdata)
{
	struct sftpc_pending *p = calloc(1, sizeof(*p));
	if (p == NULL)
		return NULL;
	p->cb = cb;
	p->cbdata = cbdata;
	p->parse = parse_status_only;
	p->free_self = free_self_base;

	if (handle == NULL) {
		PENDING_RECORD(p, SFTP_ERR_NULL_HANDLE, "handle == NULL");
		fire_sync(p);
		return p;
	}
	if (!client_enter(state, p)) {
		fire_sync(p);
		return p;
	}
	p->req_id = next_req_id(state);
	if (!appendheader(&state->txp, SSH_FXP_CLOSE, p->req_id) ||
	    !appendstring(&state->txp, handle)) {
		PENDING_RECORD(p, SFTP_ERR_PACKET_BUILD_FAILED,
		    "appendheader/appendstring failed");
		client_exit(state);
		fire_sync(p);
		return p;
	}
	if (!front_send(state, p)) {
		client_exit(state);
		fire_sync(p);
		return p;
	}
	client_exit(state);
	return p;
}

struct sftpc_pending *
sftpc_write(sftpc_state_t state, sftp_str_t handle, uint64_t offset,
            sftp_str_t data, void (*cb)(struct sftpc_pending *), void *cbdata)
{
	struct sftpc_pending *p = calloc(1, sizeof(*p));
	if (p == NULL)
		return NULL;
	p->cb = cb;
	p->cbdata = cbdata;
	p->parse = parse_status_only;
	p->free_self = free_self_base;

	if (handle == NULL) {
		PENDING_RECORD(p, SFTP_ERR_NULL_HANDLE, "handle == NULL");
		fire_sync(p);
		return p;
	}
	if (data == NULL) {
		PENDING_RECORD(p, SFTP_ERR_NULL_DATA, "data == NULL");
		fire_sync(p);
		return p;
	}
	if (!client_enter(state, p)) {
		fire_sync(p);
		return p;
	}
	p->req_id = next_req_id(state);
	if (!appendheader(&state->txp, SSH_FXP_WRITE, p->req_id) ||
	    !appendstring(&state->txp, handle) ||
	    !append64(&state->txp, offset) ||
	    !appendstring(&state->txp, data)) {
		PENDING_RECORD(p, SFTP_ERR_PACKET_BUILD_FAILED,
		    "write build failed");
		client_exit(state);
		fire_sync(p);
		return p;
	}
	if (!front_send(state, p)) {
		client_exit(state);
		fire_sync(p);
		return p;
	}
	client_exit(state);
	return p;
}

struct sftpc_pending *
sftpc_setstat(sftpc_state_t state, const char *path, sftp_file_attr_t attr,
              void (*cb)(struct sftpc_pending *), void *cbdata)
{
	struct sftpc_pending *p = calloc(1, sizeof(*p));
	if (p == NULL)
		return NULL;
	p->cb = cb;
	p->cbdata = cbdata;
	p->parse = parse_status_only;
	p->free_self = free_self_base;

	if (path == NULL) {
		PENDING_RECORD(p, SFTP_ERR_NULL_PATH, "path == NULL");
		fire_sync(p);
		return p;
	}
	if (attr == NULL) {
		PENDING_RECORD(p, SFTP_ERR_NULL_DATA, "attr == NULL");
		fire_sync(p);
		return p;
	}
	if (!client_enter(state, p)) {
		fire_sync(p);
		return p;
	}
	p->req_id = next_req_id(state);
	if (!appendheader(&state->txp, SSH_FXP_SETSTAT, p->req_id) ||
	    !appendcstring(&state->txp, path) ||
	    !appendfattr(&state->txp, attr)) {
		PENDING_RECORD(p, SFTP_ERR_PACKET_BUILD_FAILED,
		    "setstat build failed");
		client_exit(state);
		fire_sync(p);
		return p;
	}
	if (!front_send(state, p)) {
		client_exit(state);
		fire_sync(p);
		return p;
	}
	client_exit(state);
	return p;
}

/* ========================================================================
 * mkdir / rmdir / remove / rename — STATUS-only replies
 * ======================================================================== */

static struct sftpc_pending *
do_one_path(sftpc_state_t state, uint8_t type, const char *path,
            void (*cb)(struct sftpc_pending *), void *cbdata)
{
	struct sftpc_pending *p = calloc(1, sizeof(*p));
	if (p == NULL)
		return NULL;
	p->cb = cb;
	p->cbdata = cbdata;
	p->parse = parse_status_only;
	p->free_self = free_self_base;

	if (path == NULL) {
		PENDING_RECORD(p, SFTP_ERR_NULL_PATH, "path == NULL");
		fire_sync(p);
		return p;
	}
	if (!client_enter(state, p)) {
		fire_sync(p);
		return p;
	}
	p->req_id = next_req_id(state);
	if (!appendheader(&state->txp, type, p->req_id) ||
	    !appendcstring(&state->txp, path)) {
		PENDING_RECORD(p, SFTP_ERR_PACKET_BUILD_FAILED,
		    "appendheader/appendcstring failed");
		client_exit(state);
		fire_sync(p);
		return p;
	}
	if (!front_send(state, p)) {
		client_exit(state);
		fire_sync(p);
		return p;
	}
	client_exit(state);
	return p;
}

struct sftpc_pending *
sftpc_rmdir(sftpc_state_t state, const char *path,
            void (*cb)(struct sftpc_pending *), void *cbdata)
{
	return do_one_path(state, SSH_FXP_RMDIR, path, cb, cbdata);
}

struct sftpc_pending *
sftpc_remove(sftpc_state_t state, const char *path,
             void (*cb)(struct sftpc_pending *), void *cbdata)
{
	return do_one_path(state, SSH_FXP_REMOVE, path, cb, cbdata);
}

struct sftpc_pending *
sftpc_mkdir(sftpc_state_t state, const char *path, sftp_file_attr_t attr,
            void (*cb)(struct sftpc_pending *), void *cbdata)
{
	struct sftpc_pending *p = calloc(1, sizeof(*p));
	if (p == NULL)
		return NULL;
	p->cb = cb;
	p->cbdata = cbdata;
	p->parse = parse_status_only;
	p->free_self = free_self_base;

	if (path == NULL) {
		PENDING_RECORD(p, SFTP_ERR_NULL_PATH, "path == NULL");
		fire_sync(p);
		return p;
	}
	if (!client_enter(state, p)) {
		fire_sync(p);
		return p;
	}
	p->req_id = next_req_id(state);
	sftp_file_attr_t a = attr;
	bool need_free = false;
	if (a == NULL) {
		a = sftp_fattr_alloc();
		if (a == NULL) {
			PENDING_RECORD(p, SFTP_ERR_OOM,
			    "sftp_fattr_alloc failed");
			client_exit(state);
			fire_sync(p);
			return p;
		}
		need_free = true;
	}
	bool built = appendheader(&state->txp, SSH_FXP_MKDIR, p->req_id) &&
	             appendcstring(&state->txp, path) &&
	             appendfattr(&state->txp, a);
	if (need_free)
		sftp_fattr_free(a);
	if (!built) {
		PENDING_RECORD(p, SFTP_ERR_PACKET_BUILD_FAILED,
		    "mkdir build failed");
		client_exit(state);
		fire_sync(p);
		return p;
	}
	if (!front_send(state, p)) {
		client_exit(state);
		fire_sync(p);
		return p;
	}
	client_exit(state);
	return p;
}

struct sftpc_pending *
sftpc_rename(sftpc_state_t state, const char *oldpath, const char *newpath,
             void (*cb)(struct sftpc_pending *), void *cbdata)
{
	struct sftpc_pending *p = calloc(1, sizeof(*p));
	if (p == NULL)
		return NULL;
	p->cb = cb;
	p->cbdata = cbdata;
	p->parse = parse_status_only;
	p->free_self = free_self_base;

	if (oldpath == NULL || newpath == NULL) {
		PENDING_RECORD(p, SFTP_ERR_NULL_PATH,
		    "oldpath/newpath == NULL");
		fire_sync(p);
		return p;
	}
	if (!client_enter(state, p)) {
		fire_sync(p);
		return p;
	}
	p->req_id = next_req_id(state);
	if (!appendheader(&state->txp, SSH_FXP_RENAME, p->req_id) ||
	    !appendcstring(&state->txp, oldpath) ||
	    !appendcstring(&state->txp, newpath)) {
		PENDING_RECORD(p, SFTP_ERR_PACKET_BUILD_FAILED,
		    "rename build failed");
		client_exit(state);
		fire_sync(p);
		return p;
	}
	if (!front_send(state, p)) {
		client_exit(state);
		fire_sync(p);
		return p;
	}
	client_exit(state);
	return p;
}

/* ========================================================================
 * read
 * ======================================================================== */

static void
parse_read(struct sftpc_pending *base)
{
	struct sftpc_read_pending *p = (struct sftpc_read_pending *)base;
	sftp_rx_pkt_t reply = base->reply;
	if (reply->type == SSH_FXP_DATA) {
		p->data = getstring(reply);
		if (p->data == NULL) {
			PENDING_RECORD(base, SFTP_ERR_REPLY_BAD_STRING,
			    "getstring failed on DATA reply");
			return;
		}
		base->result = SSH_FX_OK;
		return;
	}
	if (parse_status_into_pending(base))
		return;
	PENDING_RECORD(base, SFTP_ERR_REPLY_WRONG_TYPE,
	    "expected SSH_FXP_DATA or SSH_FXP_STATUS, got %s",
	    sftp_get_type_name(reply->type));
}

struct sftpc_read_pending *
sftpc_read(sftpc_state_t state, sftp_str_t handle, uint64_t offset,
           uint32_t len, void (*cb)(struct sftpc_pending *), void *cbdata)
{
	struct sftpc_read_pending *p = calloc(1, sizeof(*p));
	if (p == NULL)
		return NULL;
	p->base.cb = cb;
	p->base.cbdata = cbdata;
	p->base.parse = parse_read;
	p->base.free_self = free_self_read;

	if (handle == NULL) {
		PENDING_RECORD(&p->base, SFTP_ERR_NULL_HANDLE, "handle == NULL");
		fire_sync(&p->base);
		return p;
	}
	if (!client_enter(state, &p->base)) {
		fire_sync(&p->base);
		return p;
	}
	p->base.req_id = next_req_id(state);
	if (!appendheader(&state->txp, SSH_FXP_READ, p->base.req_id) ||
	    !appendstring(&state->txp, handle) ||
	    !append64(&state->txp, offset) ||
	    !append32(&state->txp, len)) {
		PENDING_RECORD(&p->base, SFTP_ERR_PACKET_BUILD_FAILED,
		    "read build failed");
		client_exit(state);
		fire_sync(&p->base);
		return p;
	}
	if (!front_send(state, &p->base)) {
		client_exit(state);
		fire_sync(&p->base);
		return p;
	}
	client_exit(state);
	return p;
}

/* ========================================================================
 * stat
 * ======================================================================== */

static void
parse_stat(struct sftpc_pending *base)
{
	struct sftpc_stat_pending *p = (struct sftpc_stat_pending *)base;
	sftp_rx_pkt_t reply = base->reply;
	if (reply->type == SSH_FXP_ATTRS) {
		p->attrs = getfattr(reply);
		if (p->attrs == NULL) {
			PENDING_RECORD(base, SFTP_ERR_REPLY_BAD_STRING,
			    "getfattr failed on ATTRS reply");
			return;
		}
		base->result = SSH_FX_OK;
		return;
	}
	if (parse_status_into_pending(base))
		return;
	PENDING_RECORD(base, SFTP_ERR_REPLY_WRONG_TYPE,
	    "expected SSH_FXP_ATTRS or SSH_FXP_STATUS, got %s",
	    sftp_get_type_name(reply->type));
}

struct sftpc_stat_pending *
sftpc_stat(sftpc_state_t state, const char *path,
           void (*cb)(struct sftpc_pending *), void *cbdata)
{
	struct sftpc_stat_pending *p = calloc(1, sizeof(*p));
	if (p == NULL)
		return NULL;
	p->base.cb = cb;
	p->base.cbdata = cbdata;
	p->base.parse = parse_stat;
	p->base.free_self = free_self_stat;

	if (path == NULL) {
		PENDING_RECORD(&p->base, SFTP_ERR_NULL_PATH, "path == NULL");
		fire_sync(&p->base);
		return p;
	}
	if (!client_enter(state, &p->base)) {
		fire_sync(&p->base);
		return p;
	}
	p->base.req_id = next_req_id(state);
	if (!appendheader(&state->txp, SSH_FXP_STAT, p->base.req_id) ||
	    !appendcstring(&state->txp, path)) {
		PENDING_RECORD(&p->base, SFTP_ERR_PACKET_BUILD_FAILED,
		    "stat build failed");
		client_exit(state);
		fire_sync(&p->base);
		return p;
	}
	if (!front_send(state, &p->base)) {
		client_exit(state);
		fire_sync(&p->base);
		return p;
	}
	client_exit(state);
	return p;
}

/* ========================================================================
 * readdir
 * ======================================================================== */

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

static void
parse_readdir(struct sftpc_pending *base)
{
	struct sftpc_readdir_pending *p = (struct sftpc_readdir_pending *)base;
	sftp_rx_pkt_t reply = base->reply;
	if (reply->type == SSH_FXP_NAME) {
		uint32_t n = get32(reply);
		if (n == 0) {
			base->result = SSH_FX_OK;
			return;
		}
		p->entries = calloc(n, sizeof(*p->entries));
		if (p->entries == NULL) {
			PENDING_RECORD(base, SFTP_ERR_OOM,
			    "calloc(%" PRIu32 " entries) failed", n);
			return;
		}
		for (uint32_t i = 0; i < n; i++) {
			p->entries[i].filename = getstring(reply);
			if (p->entries[i].filename == NULL) {
				PENDING_RECORD(base, SFTP_ERR_REPLY_BAD_STRING,
				    "filename string failed at entry %"
				    PRIu32 " of %" PRIu32, i, n);
				sftpc_free_dir_entries(p->entries, i + 1);
				p->entries = NULL;
				return;
			}
			p->entries[i].longname = getstring(reply);
			if (p->entries[i].longname == NULL) {
				PENDING_RECORD(base, SFTP_ERR_REPLY_BAD_STRING,
				    "longname string failed at entry %"
				    PRIu32 " of %" PRIu32, i, n);
				sftpc_free_dir_entries(p->entries, i + 1);
				p->entries = NULL;
				return;
			}
			p->entries[i].attrs = getfattr(reply);
			if (p->entries[i].attrs == NULL) {
				PENDING_RECORD(base, SFTP_ERR_PARSE_FAILED,
				    "fattr parse failed at entry %"
				    PRIu32 " of %" PRIu32, i, n);
				sftpc_free_dir_entries(p->entries, i + 1);
				p->entries = NULL;
				return;
			}
		}
		p->count = n;
		base->result = SSH_FX_OK;
		return;
	}
	if (parse_status_into_pending(base)) {
		if (base->result == SSH_FX_EOF)
			p->eof = true;
		return;
	}
	PENDING_RECORD(base, SFTP_ERR_REPLY_WRONG_TYPE,
	    "expected SSH_FXP_NAME or SSH_FXP_STATUS, got %s",
	    sftp_get_type_name(reply->type));
}

struct sftpc_readdir_pending *
sftpc_readdir(sftpc_state_t state, sftp_str_t handle,
              void (*cb)(struct sftpc_pending *), void *cbdata)
{
	struct sftpc_readdir_pending *p = calloc(1, sizeof(*p));
	if (p == NULL)
		return NULL;
	p->base.cb = cb;
	p->base.cbdata = cbdata;
	p->base.parse = parse_readdir;
	p->base.free_self = free_self_readdir;

	if (handle == NULL) {
		PENDING_RECORD(&p->base, SFTP_ERR_NULL_HANDLE, "handle == NULL");
		fire_sync(&p->base);
		return p;
	}
	if (!client_enter(state, &p->base)) {
		fire_sync(&p->base);
		return p;
	}
	p->base.req_id = next_req_id(state);
	if (!appendheader(&state->txp, SSH_FXP_READDIR, p->base.req_id) ||
	    !appendstring(&state->txp, handle)) {
		PENDING_RECORD(&p->base, SFTP_ERR_PACKET_BUILD_FAILED,
		    "readdir build failed");
		client_exit(state);
		fire_sync(&p->base);
		return p;
	}
	if (!front_send(state, &p->base)) {
		client_exit(state);
		fire_sync(&p->base);
		return p;
	}
	client_exit(state);
	return p;
}

/* ========================================================================
 * descs (SyncTERM extension; only valid when SFTP_EXT_DESCS negotiated)
 * ======================================================================== */

static void
parse_descs(struct sftpc_pending *base)
{
	struct sftpc_descs_pending *p = (struct sftpc_descs_pending *)base;
	sftp_rx_pkt_t reply = base->reply;
	if (reply->type == SSH_FXP_EXTENDED_REPLY) {
		p->desc = getstring(reply);
		if (p->desc == NULL) {
			PENDING_RECORD(base, SFTP_ERR_REPLY_BAD_STRING,
			    "getstring failed on EXTENDED_REPLY");
			return;
		}
		base->result = SSH_FX_OK;
		return;
	}
	if (parse_status_into_pending(base))
		return;
	PENDING_RECORD(base, SFTP_ERR_REPLY_WRONG_TYPE,
	    "expected SSH_FXP_EXTENDED_REPLY or SSH_FXP_STATUS, got %s",
	    sftp_get_type_name(reply->type));
}

struct sftpc_descs_pending *
sftpc_descs(sftpc_state_t state, const char *path,
            void (*cb)(struct sftpc_pending *), void *cbdata)
{
	struct sftpc_descs_pending *p = calloc(1, sizeof(*p));
	if (p == NULL)
		return NULL;
	p->base.cb = cb;
	p->base.cbdata = cbdata;
	p->base.parse = parse_descs;
	p->base.free_self = free_self_descs;

	if (path == NULL) {
		PENDING_RECORD(&p->base, SFTP_ERR_NULL_PATH, "path == NULL");
		fire_sync(&p->base);
		return p;
	}
	if (!client_enter(state, &p->base)) {
		fire_sync(&p->base);
		return p;
	}
	p->base.req_id = next_req_id(state);
	if (!appendheader(&state->txp, SSH_FXP_EXTENDED, p->base.req_id) ||
	    !appendcstring(&state->txp, SFTP_EXT_NAME_DESCS) ||
	    !appendcstring(&state->txp, path)) {
		PENDING_RECORD(&p->base, SFTP_ERR_PACKET_BUILD_FAILED,
		    "descs build failed");
		client_exit(state);
		fire_sync(&p->base);
		return p;
	}
	if (!front_send(state, &p->base)) {
		client_exit(state);
		fire_sync(&p->base);
		return p;
	}
	client_exit(state);
	return p;
}
