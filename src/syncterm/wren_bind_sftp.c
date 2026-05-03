/* SFTP foreign surface for the Wren scripting host.  Public API is
 * declared in wren_bind_sftp.h; the BINDINGS table + lookup_class
 * dispatch live in wren_bind.c.  See wren_bind_internal.h for the
 * shared SWF_* type tags and helpers (load_class_into_slot, wren_throw)
 * used here. */

#include "wren_bind_internal.h"
#include "wren_bind_sftp.h"
#include "wren_host_internal.h"
#include "wren_host.h"

#ifndef WITHOUT_DEUCESSH

#include "sftp.h"
#include "ssh.h"   /* sftp_state, sftp_available */

#include <dirwrap.h>     /* MAX_PATH */
#include <genwrap.h>     /* strlcpy on platforms where libc lacks it */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ----- Foreign struct definitions ---------------------------------- */

struct wren_sftp_entry {
	enum syncterm_wren_foreign type;
	char     *name;       /* malloc'd, NUL-terminated */
	char     *longname;   /* malloc'd, NUL-terminated; NULL if no lname */
	uint64_t  size;
	uint32_t  mtime;
	bool      is_dir;
	bool      has_long_desc;
	uint8_t  *hash;       /* malloc'd; NULL if no sha1s/md5s ext */
	uint32_t  hash_len;
};

struct wren_sftp_stat {
	enum syncterm_wren_foreign type;
	uint64_t size;
	uint32_t mtime;
	uint32_t atime;
	uint32_t mode;
	uint32_t uid;
	uint32_t gid;
};

/* Server file/dir handle.  `handle` is owned by this foreign — copied
 * out of pending->handle by the open deliver fn, freed here on close
 * or finalize.  `dead` flips true after explicit SFTP.close so a
 * later read/write/close on the same handle aborts the calling
 * fiber rather than reusing already-closed bytes. */
struct wren_sftp_handle {
	enum syncterm_wren_foreign type;
	sftp_str_t handle;
	bool       dead;
};

/* Distinguishing library-level from server-status errors:
 *   code == SFTP_ERR_OK and serverStatus != SSH_FX_OK
 *     — server replied with a STATUS code (file not found, perm denied,
 *       …); read serverStatus for the SSH_FX_* value.
 *   code != SFTP_ERR_OK
 *     — local / transport failure before a reply was parsed; serverStatus
 *       is SSH_FX_OK and meaningless. */
struct wren_sftp_error {
	enum syncterm_wren_foreign type;
	uint32_t code;          /* sftp_err_code_t */
	uint32_t server_status; /* SSH_FX_* */
	char    *message;       /* malloc'd; may be NULL */
	bool     transient;
};

/* ----- Allocators / finalizers ------------------------------------- */

void
wren_sftp_entry_allocate(WrenVM *vm)
{
	struct wren_sftp_entry *e = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*e));
	memset(e, 0, sizeof(*e));
	e->type = SWF_SFTP_ENTRY;
}

void
wren_sftp_entry_finalize(void *data)
{
	struct wren_sftp_entry *e = data;
	free(e->name);
	free(e->longname);
	free(e->hash);
}

void
wren_sftp_stat_allocate(WrenVM *vm)
{
	struct wren_sftp_stat *s = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*s));
	memset(s, 0, sizeof(*s));
	s->type = SWF_SFTP_STAT;
}

void
wren_sftp_stat_finalize(void *data)
{
	(void)data;
}

void
wren_sftp_handle_allocate(WrenVM *vm)
{
	struct wren_sftp_handle *h = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*h));
	memset(h, 0, sizeof(*h));
	h->type = SWF_SFTP_HANDLE;
}

/* Best-effort close on GC.  Scripts should call SFTP.close explicitly;
 * this is a safety net for handles that go unreferenced.  Status reply
 * is discarded. */
void
wren_sftp_handle_finalize(void *data)
{
	struct wren_sftp_handle *h = data;
	if (h->handle != NULL) {
		if (!h->dead && sftp_available && sftp_state != NULL) {
			struct sftpc_pending *p =
			    sftpc_close(sftp_state, h->handle, NULL, NULL);
			sftpc_pending_free(p);
		}
		free_sftp_str(h->handle);
		h->handle = NULL;
	}
}

void
wren_sftp_error_allocate(WrenVM *vm)
{
	struct wren_sftp_error *e = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*e));
	memset(e, 0, sizeof(*e));
	e->type = SWF_SFTP_ERROR;
}

void
wren_sftp_error_finalize(void *data)
{
	struct wren_sftp_error *e = data;
	free(e->message);
}

/* ----- SFTP static getters ---------------------------------------- */

void
fn_SFTP_available(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, sftp_available && sftp_state != NULL);
}

void
fn_SFTP_pubdir(WrenVM *vm)
{
	if (!sftp_available || sftp_state == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	const char *p = sftpc_get_pubdir(sftp_state);
	if (p == NULL)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotString(vm, 0, p);
}

/* True iff the session negotiated the lname@syncterm.net extension —
 * the per-entry friendly long name (directories) / short description
 * (files) the browser shows.  Lets the UI skip reserving a row for a
 * description bar that would always be blank. */
void
fn_SFTP_lname(WrenVM *vm)
{
	if (!sftp_available || sftp_state == NULL) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	uint32_t exts = sftpc_get_extensions(sftp_state);
	wrenSetSlotBool(vm, 0, (exts & SFTP_EXT_LNAME) != 0);
}

/* ----- SFTPEntry field accessors ---------------------------------- */

void
fn_SFTPEntry_name(WrenVM *vm)
{
	struct wren_sftp_entry *e = wrenGetSlotForeign(vm, 0);
	if (e->name == NULL)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotString(vm, 0, e->name);
}

void
fn_SFTPEntry_longname(WrenVM *vm)
{
	struct wren_sftp_entry *e = wrenGetSlotForeign(vm, 0);
	if (e->longname == NULL)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotString(vm, 0, e->longname);
}

void
fn_SFTPEntry_size(WrenVM *vm)
{
	struct wren_sftp_entry *e = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)e->size);
}

void
fn_SFTPEntry_mtime(WrenVM *vm)
{
	struct wren_sftp_entry *e = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)e->mtime);
}

void
fn_SFTPEntry_isDir(WrenVM *vm)
{
	struct wren_sftp_entry *e = wrenGetSlotForeign(vm, 0);
	wrenSetSlotBool(vm, 0, e->is_dir);
}

void
fn_SFTPEntry_hasLongDesc(WrenVM *vm)
{
	struct wren_sftp_entry *e = wrenGetSlotForeign(vm, 0);
	wrenSetSlotBool(vm, 0, e->has_long_desc);
}

void
fn_SFTPEntry_hash(WrenVM *vm)
{
	struct wren_sftp_entry *e = wrenGetSlotForeign(vm, 0);
	if (e->hash == NULL || e->hash_len == 0) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotBytes(vm, 0, (const char *)e->hash, e->hash_len);
}

/* ----- SFTPStat field accessors ----------------------------------- */

void
fn_SFTPStat_size(WrenVM *vm)
{
	struct wren_sftp_stat *s = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)s->size);
}

void
fn_SFTPStat_mtime(WrenVM *vm)
{
	struct wren_sftp_stat *s = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)s->mtime);
}

void
fn_SFTPStat_atime(WrenVM *vm)
{
	struct wren_sftp_stat *s = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)s->atime);
}

void
fn_SFTPStat_mode(WrenVM *vm)
{
	struct wren_sftp_stat *s = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)s->mode);
}

void
fn_SFTPStat_uid(WrenVM *vm)
{
	struct wren_sftp_stat *s = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)s->uid);
}

void
fn_SFTPStat_gid(WrenVM *vm)
{
	struct wren_sftp_stat *s = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)s->gid);
}

/* ----- SFTPError field accessors ---------------------------------- */

void
fn_SFTPError_code(WrenVM *vm)
{
	struct wren_sftp_error *e = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)e->code);
}

void
fn_SFTPError_message(WrenVM *vm)
{
	struct wren_sftp_error *e = wrenGetSlotForeign(vm, 0);
	if (e->message == NULL)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotString(vm, 0, e->message);
}

void
fn_SFTPError_isTransient(WrenVM *vm)
{
	struct wren_sftp_error *e = wrenGetSlotForeign(vm, 0);
	wrenSetSlotBool(vm, 0, e->transient);
}

void
fn_SFTPError_serverStatus(WrenVM *vm)
{
	struct wren_sftp_error *e = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)e->server_status);
}

/* Short symbolic name for sftp_err_code_t — used by SFTPError.toString
 * since the lib's sftp_get_errcode_name covers SSH_FX_* not the
 * library-side enum. */
static const char *
sftp_err_short_name(sftp_err_code_t err)
{
	switch (err) {
	case SFTP_ERR_OK:                   return "OK";
	case SFTP_ERR_NULL_STATE:           return "NULL_STATE";
	case SFTP_ERR_NULL_PATH:            return "NULL_PATH";
	case SFTP_ERR_NULL_HANDLE_OUT:      return "NULL_HANDLE_OUT";
	case SFTP_ERR_NULL_HANDLE:          return "NULL_HANDLE";
	case SFTP_ERR_HANDLE_NOT_NULL:      return "HANDLE_NOT_NULL";
	case SFTP_ERR_NULL_DATA:            return "NULL_DATA";
	case SFTP_ERR_NULL_OUT:             return "NULL_OUT";
	case SFTP_ERR_OOM:                  return "OOM";
	case SFTP_ERR_PACKET_BUILD_FAILED:  return "PACKET_BUILD_FAILED";
	case SFTP_ERR_SEND_FAILED:          return "SEND_FAILED";
	case SFTP_ERR_CHANNEL_CLOSED:       return "CHANNEL_CLOSED";
	case SFTP_ERR_ABORTED:              return "ABORTED";
	case SFTP_ERR_REPLY_NULL:           return "REPLY_NULL";
	case SFTP_ERR_REPLY_WRONG_TYPE:     return "REPLY_WRONG_TYPE";
	case SFTP_ERR_REPLY_BAD_STRING:     return "REPLY_BAD_STRING";
	case SFTP_ERR_PARSE_FAILED:         return "PARSE_FAILED";
	}
	return "UNKNOWN";
}

/* ----- toString — SFTP* -------------------------------------------- */

void
fn_SFTPEntry_toString(WrenVM *vm)
{
	struct wren_sftp_entry *e = wrenGetSlotForeign(vm, 0);
	if (e->longname != NULL && e->longname[0] != '\0')
		wrenSetSlotString(vm, 0, e->longname);
	else if (e->name != NULL && e->name[0] != '\0')
		wrenSetSlotString(vm, 0, e->name);
	else
		wrenSetSlotString(vm, 0, "SFTPEntry");
}

void
fn_SFTPStat_toString(WrenVM *vm)
{
	struct wren_sftp_stat *s = wrenGetSlotForeign(vm, 0);
	char buf[128];
	snprintf(buf, sizeof(buf),
	    "SFTPStat(size=%" PRIu64 ", mode=0%o, mtime=%" PRIu32 ")",
	    s->size, s->mode, s->mtime);
	wrenSetSlotString(vm, 0, buf);
}

void
fn_SFTPError_toString(WrenVM *vm)
{
	struct wren_sftp_error *e = wrenGetSlotForeign(vm, 0);
	const char *name;
	if (e->code != SFTP_ERR_OK)
		name = sftp_err_short_name((sftp_err_code_t)e->code);
	else if (e->server_status != SSH_FX_OK)
		name = sftp_get_errcode_name(e->server_status);
	else
		name = "OK";
	char buf[256];
	if (e->message != NULL && e->message[0] != '\0')
		snprintf(buf, sizeof(buf), "SFTPError: %s: %s", name,
		    e->message);
	else
		snprintf(buf, sizeof(buf), "SFTPError: %s", name);
	wrenSetSlotString(vm, 0, buf);
}

/* ----- SFTP parking infrastructure --------------------------------- */

/* Threaded by wren_result_push: the recv-thread cb stores `pending`
 * here and pushes `ctx` onto the result queue.  The owner-thread
 * deliver fn reads pending fields straight off and frees the pending
 * — no intermediate copy.  Sync failures (session-dead, OOM) skip
 * the queue entirely; the foreign method writes an SFTPError into
 * slot 0 and returns. */
struct sftp_call_ctx {
	WrenHandle             *fiber;       /* owned; released by framework */
	struct sftpc_pending   *pending;     /* set by cb; freed by deliver/free */
	wren_result_deliver_fn  deliver;     /* per-op result builder */
};

/* Owner-thread free fn — runs after the deliver fn (or instead of it
 * if the fiber's already done).  sftpc_pending_free is a no-op on
 * NULL, so the deliver fn can null out ctx->pending after consuming
 * to avoid re-freeing a borrowed reference (none of our deliver fns
 * do today; this is just defensive).  ctx itself is always freed. */
static void
sftp_call_ctx_free(void *data)
{
	struct sftp_call_ctx *ctx = data;
	sftpc_pending_free(ctx->pending);
	free(ctx);
}

/* Recv-thread cb (state->mtx held) — every parking op shares this.
 * Records the pending pointer and pushes onto the owner-thread queue.
 * No allocation, no copying. */
static void
sftp_call_cb(struct sftpc_pending *p)
{
	struct sftp_call_ctx *ctx = p->cbdata;
	ctx->pending = p;
	wren_result_push(ctx->fiber, ctx, ctx->deliver, sftp_call_ctx_free);
}

/* Build an SFTPError into `slot` directly from explicit values —
 * used for synchronous failures the foreign method returns before
 * the queue (session-dead, OOM).  Lifetime fields cleared. */
static void
sftp_build_synth_error(WrenVM *vm, int slot, sftp_err_code_t err,
                       const char *msg)
{
	struct wren_host_state *st = wren_host_state();
	wrenEnsureSlots(vm, slot + 2);
	load_class_into_slot(vm, &st->sftp_error_class, "SFTPError",
	    slot + 1);
	struct wren_sftp_error *e =
	    wrenSetSlotNewForeign(vm, slot, slot + 1, sizeof(*e));
	memset(e, 0, sizeof(*e));
	e->type      = SWF_SFTP_ERROR;
	e->code      = (uint32_t)err;
	e->transient = sftp_err_is_transient(err);
	if (msg != NULL)
		e->message = strdup(msg);
}

/* Build an SFTPError into `slot` from a queued ctx if the call
 * failed; return true.  Returns false (no change to slot) on
 * success, signaling the caller to populate the typed result
 * foreign instead.  Always called from the deliver fn — the queue
 * path always has a non-NULL pending. */
static bool
sftp_deliver_error(WrenVM *vm, int slot, struct sftp_call_ctx *ctx)
{
	struct sftpc_pending *p = ctx->pending;
	if (p->err == SFTP_ERR_OK && p->result == SSH_FX_OK)
		return false;

	struct wren_host_state *st = wren_host_state();
	wrenEnsureSlots(vm, slot + 2);
	load_class_into_slot(vm, &st->sftp_error_class, "SFTPError",
	    slot + 1);
	struct wren_sftp_error *e =
	    wrenSetSlotNewForeign(vm, slot, slot + 1, sizeof(*e));
	memset(e, 0, sizeof(*e));
	e->type = SWF_SFTP_ERROR;
	if (p->err != SFTP_ERR_OK) {
		e->code      = (uint32_t)p->err;
		e->transient = sftp_err_is_transient(p->err);
	}
	else {
		e->code          = (uint32_t)SFTP_ERR_OK;
		e->server_status = p->result;
	}
	if (p->estr != NULL)
		e->message = strdup(p->estr);
	return true;
}

/* Allocate a sftp_call_ctx and capture the fiber handle in slot 1.
 * Returns NULL on OOM; the caller surfaces that as a synthetic
 * SFTPError into slot 0. */
static struct sftp_call_ctx *
sftp_call_ctx_new(WrenVM *vm, wren_result_deliver_fn deliver)
{
	struct sftp_call_ctx *ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL)
		return NULL;
	ctx->fiber   = wrenGetSlotHandle(vm, 1);
	ctx->deliver = deliver;
	return ctx;
}

/* True iff sftp_state is non-NULL and the session is still up.  The
 * session pointer is owned by ssh.c; sftp_available flips false at
 * the start of teardown so callers can drop us before we reach into
 * a state that's about to be torn down. */
static bool
sftp_session_live(void)
{
	return sftp_available && sftp_state != NULL;
}

/* Standard prelude for every parking foreign — checks session,
 * allocates ctx.  On any failure, builds an SFTPError into slot 0
 * and returns NULL; the caller just `return`s.  On success returns
 * the ctx; the caller fires the lib op and then sets slot 0 null. */
static struct sftp_call_ctx *
sftp_call_prelude(WrenVM *vm, wren_result_deliver_fn deliver)
{
	if (!sftp_session_live()) {
		sftp_build_synth_error(vm, 0, SFTP_ERR_ABORTED,
		    "SFTP session is not available");
		return NULL;
	}
	struct sftp_call_ctx *ctx = sftp_call_ctx_new(vm, deliver);
	if (ctx == NULL) {
		sftp_build_synth_error(vm, 0, SFTP_ERR_OOM,
		    "out of memory allocating SFTP request context");
		return NULL;
	}
	return ctx;
}

/* ----- realpath ---------------------------------------------------- */

static void
sftp_realpath_deliver(WrenVM *vm, int slot, void *data)
{
	struct sftp_call_ctx *ctx = data;
	if (sftp_deliver_error(vm, slot, ctx))
		return;
	struct sftpc_realpath_pending *rp =
	    (struct sftpc_realpath_pending *)ctx->pending;
	sftp_str_t r = rp->ret;
	if (r == NULL || r->len == 0)
		wrenSetSlotString(vm, slot, "");
	else
		wrenSetSlotBytes(vm, slot, (const char *)r->c_str, r->len);
}

void
fn_SFTP_realpath(WrenVM *vm)
{
	struct sftp_call_ctx *ctx = sftp_call_prelude(vm, sftp_realpath_deliver);
	if (ctx == NULL)
		return;
	const char *path = wrenGetSlotString(vm, 2);
	sftpc_realpath(sftp_state, path, sftp_call_cb, ctx);
	wrenSetSlotNull(vm, 0);
}

/* ----- stat -------------------------------------------------------- */

static void
sftp_stat_deliver(WrenVM *vm, int slot, void *data)
{
	struct sftp_call_ctx *ctx = data;
	if (sftp_deliver_error(vm, slot, ctx))
		return;
	struct sftpc_stat_pending *sp =
	    (struct sftpc_stat_pending *)ctx->pending;
	struct wren_host_state *st = wren_host_state();
	wrenEnsureSlots(vm, slot + 2);
	load_class_into_slot(vm, &st->sftp_stat_class, "SFTPStat", slot + 1);
	struct wren_sftp_stat *s =
	    wrenSetSlotNewForeign(vm, slot, slot + 1, sizeof(*s));
	memset(s, 0, sizeof(*s));
	s->type = SWF_SFTP_STAT;
	sftp_fattr_get_size(sp->attrs, &s->size);
	sftp_fattr_get_mtime(sp->attrs, &s->mtime);
	sftp_fattr_get_atime(sp->attrs, &s->atime);
	sftp_fattr_get_permissions(sp->attrs, &s->mode);
	sftp_fattr_get_uid(sp->attrs, &s->uid);
	sftp_fattr_get_gid(sp->attrs, &s->gid);
}

void
fn_SFTP_stat(WrenVM *vm)
{
	struct sftp_call_ctx *ctx = sftp_call_prelude(vm, sftp_stat_deliver);
	if (ctx == NULL)
		return;
	const char *path = wrenGetSlotString(vm, 2);
	sftpc_stat(sftp_state, path, sftp_call_cb, ctx);
	wrenSetSlotNull(vm, 0);
}

/* ----- readdir ----------------------------------------------------- */

/* Copy a sftp_str_t into a fresh malloc'd NUL-terminated string.
 * Returns NULL on OOM or empty/missing input. */
static char *
sftp_str_to_cstr(sftp_str_t s)
{
	if (s == NULL || s->len == 0)
		return NULL;
	char *out = malloc(s->len + 1);
	if (out == NULL)
		return NULL;
	memcpy(out, s->c_str, s->len);
	out[s->len] = '\0';
	return out;
}

/* Pull the sha1@syncterm.net or md5@syncterm.net hash bytes out of a
 * file_attr's extension list.  Caller owns the returned malloc'd
 * buffer; *len_out receives the length.  Returns NULL when the
 * attribute has no hash extension. */
static uint8_t *
sftp_attr_extract_hash(sftp_file_attr_t a, uint32_t *len_out)
{
	*len_out = 0;
	sftp_str_t s = sftp_fattr_get_ext_by_type(a, "sha1@syncterm.net");
	if (s == NULL)
		s = sftp_fattr_get_ext_by_type(a, "md5@syncterm.net");
	if (s == NULL)
		return NULL;
	uint8_t *out = NULL;
	if (s->len > 0) {
		out = malloc(s->len);
		if (out != NULL) {
			memcpy(out, s->c_str, s->len);
			*len_out = s->len;
		}
	}
	free_sftp_str(s);
	return out;
}

/* Build a SFTPEntry foreign in `slot+1` from one dir entry, slot+2 as
 * scratch for the class lookup.
 *
 * `longname` is populated from the `lname@syncterm.net` per-attribute
 * extension when the session negotiated it — that's the rich
 * Synchronet description (file caption / filebase blurb).  The SFTP
 * v3 standard ls-l style longname (`de->longname`) is intentionally
 * NOT exposed: it's only useful as a permissions-bits fallback for
 * is_dir, which the C side has already resolved into `e->is_dir` by
 * this point. */
static void
build_sftp_entry(WrenVM *vm, int slot,
                 struct wren_host_state *st,
                 struct sftpc_dir_entry *de)
{
	load_class_into_slot(vm, &st->sftp_entry_class, "SFTPEntry", slot + 1);
	struct wren_sftp_entry *e =
	    wrenSetSlotNewForeign(vm, slot, slot + 1, sizeof(*e));
	memset(e, 0, sizeof(*e));
	e->type     = SWF_SFTP_ENTRY;
	e->name     = sftp_str_to_cstr(de->filename);
	if (de->attrs != NULL) {
		sftp_fattr_get_size(de->attrs,  &e->size);
		sftp_fattr_get_mtime(de->attrs, &e->mtime);
		uint32_t perm;
		bool have_perm = sftp_fattr_get_permissions(de->attrs, &perm);
		if (have_perm) {
			e->is_dir = (perm & S_IFMT) == S_IFDIR;
		}
		else if (de->longname != NULL && de->longname->len > 0) {
			/* No mode bits — fall back to the v3 longname's first
			 * byte (`d` for directory in ls -l style). */
			e->is_dir = de->longname->c_str[0] == 'd';
		}
		e->hash = sftp_attr_extract_hash(de->attrs, &e->hash_len);
		uint32_t exts = sftpc_get_extensions(sftp_state);
		if (exts & SFTP_EXT_LNAME) {
			sftp_str_t ln = sftp_fattr_get_ext_by_type(de->attrs,
			    SFTP_EXT_NAME_LNAME);
			if (ln != NULL && ln->len > 0)
				e->longname = sftp_str_to_cstr(ln);
		}
		if (exts & SFTP_EXT_DESCS) {
			/* Presence of the descs extension on a fattr means the
			 * server has a long description ready for a separate
			 * SFTP_EXT_NAME_DESCS request.  Value is empty — the
			 * extension acts as a one-bit availability marker. */
			if (sftp_fattr_get_ext_by_type(de->attrs,
			        SFTP_EXT_NAME_DESCS) != NULL)
				e->has_long_desc = true;
		}
	}
}

/* ----- open / opendir / readdir(handle) / close -------------------- */

/* Shared by open and opendir — both use sftpc_open_pending with a
 * server-side handle.  Transfers ownership of the bytes into a fresh
 * SFTPHandle foreign by stealing pending->handle (NULL'd before
 * sftpc_pending_free runs in sftp_call_ctx_free). */
static void
sftp_open_deliver(WrenVM *vm, int slot, void *data)
{
	struct sftp_call_ctx *ctx = data;
	if (sftp_deliver_error(vm, slot, ctx))
		return;
	struct sftpc_open_pending *op =
	    (struct sftpc_open_pending *)ctx->pending;
	struct wren_host_state *st = wren_host_state();
	wrenEnsureSlots(vm, slot + 2);
	load_class_into_slot(vm, &st->sftp_handle_class, "SFTPHandle",
	    slot + 1);
	struct wren_sftp_handle *h =
	    wrenSetSlotNewForeign(vm, slot, slot + 1, sizeof(*h));
	memset(h, 0, sizeof(*h));
	h->type   = SWF_SFTP_HANDLE;
	h->handle = op->handle;
	op->handle = NULL;
}

void
fn_SFTP_opendir(WrenVM *vm)
{
	struct sftp_call_ctx *ctx = sftp_call_prelude(vm, sftp_open_deliver);
	if (ctx == NULL)
		return;
	const char *path = wrenGetSlotString(vm, 2);
	sftpc_opendir(sftp_state, path, sftp_call_cb, ctx);
	wrenSetSlotNull(vm, 0);
}

/* SFTPHandle receiver validation — abort the calling fiber if the
 * caller already closed the handle (in which case the bytes are
 * gone).  Returns the foreign on success, NULL after aborting. */
static struct wren_sftp_handle *
sftp_handle_check(WrenVM *vm, int slot)
{
	if (wrenGetSlotType(vm, slot) != WREN_TYPE_FOREIGN) {
		wren_throw(vm, "SFTP: expected SFTPHandle");
		return NULL;
	}
	struct wren_foreign_header *hdr = wrenGetSlotForeign(vm, slot);
	if (hdr->type != SWF_SFTP_HANDLE) {
		wren_throw(vm, "SFTP: expected SFTPHandle");
		return NULL;
	}
	struct wren_sftp_handle *h = (struct wren_sftp_handle *)hdr;
	if (h->dead || h->handle == NULL) {
		wren_throw(vm, "SFTP: handle is closed");
		return NULL;
	}
	return h;
}

static void
sftp_readdir_deliver(WrenVM *vm, int slot, void *data)
{
	struct sftp_call_ctx *ctx = data;
	struct sftpc_pending *p = ctx->pending;
	/* readdir's "EOF" is a STATUS reply with result == SSH_FX_EOF.
	 * Surface that as null rather than as an SFTPError so callers
	 * can `while (chunk = readdir(h))` cleanly. */
	if (p != NULL && p->err == SFTP_ERR_OK && p->result == SSH_FX_EOF) {
		wrenSetSlotNull(vm, slot);
		return;
	}
	if (sftp_deliver_error(vm, slot, ctx))
		return;
	struct sftpc_readdir_pending *rp =
	    (struct sftpc_readdir_pending *)ctx->pending;
	struct wren_host_state *st = wren_host_state();
	wrenEnsureSlots(vm, slot + 3);
	wrenSetSlotNewList(vm, slot);
	for (uint32_t i = 0; i < rp->count; i++) {
		build_sftp_entry(vm, slot + 1, st, &rp->entries[i]);
		wrenInsertInList(vm, slot, -1, slot + 1);
	}
}

void
fn_SFTP_readdir(WrenVM *vm)
{
	struct sftp_call_ctx *ctx = sftp_call_prelude(vm, sftp_readdir_deliver);
	if (ctx == NULL)
		return;
	struct wren_sftp_handle *h = sftp_handle_check(vm, 2);
	if (h == NULL) {
		/* Aborted by sftp_handle_check; clean up the captured fiber
		 * handle + ctx the prelude allocated. */
		wrenReleaseHandle(vm, ctx->fiber);
		free(ctx);
		return;
	}
	sftpc_readdir(sftp_state, h->handle, sftp_call_cb, ctx);
	wrenSetSlotNull(vm, 0);
}

/* Shared by every status-only op: close, mkdir, rmdir, remove,
 * rename.  Returns null on success or SFTPError on failure (server
 * STATUS reply with code != SSH_FX_OK, or a library-level error). */
static void
sftp_status_deliver(WrenVM *vm, int slot, void *data)
{
	struct sftp_call_ctx *ctx = data;
	if (sftp_deliver_error(vm, slot, ctx))
		return;
	wrenSetSlotNull(vm, slot);
}

void
fn_SFTP_close(WrenVM *vm)
{
	struct sftp_call_ctx *ctx = sftp_call_prelude(vm, sftp_status_deliver);
	if (ctx == NULL)
		return;
	struct wren_sftp_handle *h = sftp_handle_check(vm, 2);
	if (h == NULL) {
		wrenReleaseHandle(vm, ctx->fiber);
		free(ctx);
		return;
	}
	/* Mark dead immediately so concurrent finalize / further
	 * read/write/close reject the handle.  Steal the handle bytes
	 * (owned by the SFTPHandle foreign) into a local; sftpc_close
	 * uses them while building the tx packet, after which we free
	 * them ourselves on return. */
	sftp_str_t handle_bytes = h->handle;
	h->handle = NULL;
	h->dead   = true;
	sftpc_close(sftp_state, handle_bytes, sftp_call_cb, ctx);
	free_sftp_str(handle_bytes);
	wrenSetSlotNull(vm, 0);
}

/* ----- open ------------------------------------------------------- */

void
fn_SFTP_open(WrenVM *vm)
{
	struct sftp_call_ctx *ctx = sftp_call_prelude(vm, sftp_open_deliver);
	if (ctx == NULL)
		return;
	const char *path  = wrenGetSlotString(vm, 2);
	uint32_t    flags = (uint32_t)wrenGetSlotDouble(vm, 3);
	sftpc_open(sftp_state, path, flags, sftp_call_cb, ctx);
	wrenSetSlotNull(vm, 0);
}

/* ----- read -------------------------------------------------------- */

static void
sftp_read_deliver(WrenVM *vm, int slot, void *data)
{
	struct sftp_call_ctx *ctx = data;
	struct sftpc_pending *p = ctx->pending;
	/* read's "EOF" is a STATUS reply with result == SSH_FX_EOF.
	 * Surface as null so callers can `while (chunk = read(...))`. */
	if (p != NULL && p->err == SFTP_ERR_OK && p->result == SSH_FX_EOF) {
		wrenSetSlotNull(vm, slot);
		return;
	}
	if (sftp_deliver_error(vm, slot, ctx))
		return;
	struct sftpc_read_pending *rp =
	    (struct sftpc_read_pending *)ctx->pending;
	sftp_str_t d = rp->data;
	if (d == NULL || d->len == 0)
		wrenSetSlotBytes(vm, slot, "", 0);
	else
		wrenSetSlotBytes(vm, slot, (const char *)d->c_str, d->len);
}

void
fn_SFTP_read(WrenVM *vm)
{
	struct sftp_call_ctx *ctx = sftp_call_prelude(vm, sftp_read_deliver);
	if (ctx == NULL)
		return;
	struct wren_sftp_handle *h = sftp_handle_check(vm, 2);
	if (h == NULL) {
		wrenReleaseHandle(vm, ctx->fiber);
		free(ctx);
		return;
	}
	uint32_t count  = (uint32_t)wrenGetSlotDouble(vm, 3);
	uint64_t offset = (uint64_t)wrenGetSlotDouble(vm, 4);
	sftpc_read(sftp_state, h->handle, offset, count, sftp_call_cb, ctx);
	wrenSetSlotNull(vm, 0);
}

/* ----- write ------------------------------------------------------- */

/* SFTP write is all-or-nothing on the wire: SSH_FX_OK means every
 * byte was accepted, anything else is a failure with no partial
 * progress.  Return null on success and SFTPError on failure — the
 * caller already knows how many bytes they asked to write. */
static void
sftp_write_deliver(WrenVM *vm, int slot, void *data)
{
	struct sftp_call_ctx *ctx = data;
	if (sftp_deliver_error(vm, slot, ctx))
		return;
	wrenSetSlotNull(vm, slot);
}

void
fn_SFTP_write(WrenVM *vm)
{
	struct sftp_call_ctx *ctx = sftp_call_prelude(vm, sftp_write_deliver);
	if (ctx == NULL)
		return;
	struct wren_sftp_handle *h = sftp_handle_check(vm, 2);
	if (h == NULL) {
		wrenReleaseHandle(vm, ctx->fiber);
		free(ctx);
		return;
	}
	uint64_t    offset = (uint64_t)wrenGetSlotDouble(vm, 3);
	int         blen   = 0;
	const char *bytes  = wrenGetSlotBytes(vm, 4, &blen);
	/* Borrow the Wren-slot bytes for the duration of sftpc_write —
	 * the lib copies into the tx packet via appendstring, so the
	 * borrow is fine to release on return. */
	struct sftp_string data;
	sftp_memborrow(&data, (const uint8_t *)bytes, (uint32_t)blen,
	    NULL, NULL);
	sftpc_write(sftp_state, h->handle, offset, &data, sftp_call_cb, ctx);
	wrenSetSlotNull(vm, 0);
}

/* ----- mkdir / rmdir / remove / rename ----------------------------- */

void
fn_SFTP_mkdir(WrenVM *vm)
{
	struct sftp_call_ctx *ctx = sftp_call_prelude(vm, sftp_status_deliver);
	if (ctx == NULL)
		return;
	const char *path = wrenGetSlotString(vm, 2);
	/* NULL attr — sftpc_mkdir builds a default fattr block on our
	 * behalf so the server picks the umask-derived permissions. */
	sftpc_mkdir(sftp_state, path, NULL, sftp_call_cb, ctx);
	wrenSetSlotNull(vm, 0);
}

void
fn_SFTP_rmdir(WrenVM *vm)
{
	struct sftp_call_ctx *ctx = sftp_call_prelude(vm, sftp_status_deliver);
	if (ctx == NULL)
		return;
	const char *path = wrenGetSlotString(vm, 2);
	sftpc_rmdir(sftp_state, path, sftp_call_cb, ctx);
	wrenSetSlotNull(vm, 0);
}

void
fn_SFTP_remove(WrenVM *vm)
{
	struct sftp_call_ctx *ctx = sftp_call_prelude(vm, sftp_status_deliver);
	if (ctx == NULL)
		return;
	const char *path = wrenGetSlotString(vm, 2);
	sftpc_remove(sftp_state, path, sftp_call_cb, ctx);
	wrenSetSlotNull(vm, 0);
}

void
fn_SFTP_rename(WrenVM *vm)
{
	struct sftp_call_ctx *ctx = sftp_call_prelude(vm, sftp_status_deliver);
	if (ctx == NULL)
		return;
	const char *oldpath = wrenGetSlotString(vm, 2);
	const char *newpath = wrenGetSlotString(vm, 3);
	sftpc_rename(sftp_state, oldpath, newpath, sftp_call_cb, ctx);
	wrenSetSlotNull(vm, 0);
}

/* ----- setMtime ---------------------------------------------------- */

/* SFTP.setMtime(fiber, path, t) — set both atime and mtime of the
 * remote path to `t` (POSIX seconds).  Used by the queue to stamp
 * completed uploads with the local file's mtime so a subsequent
 * browse shows [==] without depending on hash extensions.  Returns
 * null on success, SFTPError on failure. */
void
fn_SFTP_setMtime(WrenVM *vm)
{
	/* Copy the path into a C buffer before any other VM activity:
	 * wrenGetSlotString pointers are invalidated by the next VM call
	 * (wren.md §6), and sftp_call_prelude / sftp_build_synth_error /
	 * even wrenGetSlotDouble are all VM calls.  Today the slot 2
	 * ObjString stays rooted so the original pointer happens to keep
	 * working, but that's a property of Wren's current GC, not a
	 * guarantee of the API. */
	const char *path_in = wrenGetSlotString(vm, 2);
	char        path[MAX_PATH + 1];
	if (path_in == NULL) {
		sftp_build_synth_error(vm, 0, SFTP_ERR_ABORTED,
		    "SFTP.setMtime: path must be a String");
		return;
	}
	strlcpy(path, path_in, sizeof(path));
	uint32_t mtime = (uint32_t)wrenGetSlotDouble(vm, 3);

	struct sftp_call_ctx *ctx = sftp_call_prelude(vm, sftp_status_deliver);
	if (ctx == NULL)
		return;
	sftp_file_attr_t a = sftp_fattr_alloc();
	if (a == NULL) {
		sftp_build_synth_error(vm, 0, SFTP_ERR_OOM,
		    "out of memory allocating fattr for setstat");
		wrenReleaseHandle(vm, ctx->fiber);
		free(ctx);
		return;
	}
	/* atime == mtime: matches "touch" semantics; the local file's
	 * exact atime is rarely meaningful and not worth a second stat. */
	sftp_fattr_set_times(a, mtime, mtime);
	sftpc_setstat(sftp_state, path, a, sftp_call_cb, ctx);
	sftp_fattr_free(a);
	wrenSetSlotNull(vm, 0);
}

/* ----- descs ------------------------------------------------------- */

static void
sftp_descs_deliver(WrenVM *vm, int slot, void *data)
{
	struct sftp_call_ctx *ctx = data;
	if (sftp_deliver_error(vm, slot, ctx))
		return;
	struct sftpc_descs_pending *dp =
	    (struct sftpc_descs_pending *)ctx->pending;
	sftp_str_t d = dp->desc;
	if (d == NULL || d->len == 0)
		wrenSetSlotString(vm, slot, "");
	else
		wrenSetSlotBytes(vm, slot, (const char *)d->c_str, d->len);
}

void
fn_SFTP_descs(WrenVM *vm)
{
	struct sftp_call_ctx *ctx = sftp_call_prelude(vm, sftp_descs_deliver);
	if (ctx == NULL)
		return;
	const char *path = wrenGetSlotString(vm, 2);
	sftpc_descs(sftp_state, path, sftp_call_cb, ctx);
	wrenSetSlotNull(vm, 0);
}

#else  /* WITHOUT_DEUCESSH ------------------------------------------- */

/* No SFTP without DeuceSSH — the SSH transport that carries it isn't
 * compiled in.  SFTP.available reports false so well-behaved scripts
 * skip the subsystem entirely; everything else aborts the calling
 * fiber.  Foreign instances can never be constructed in this build,
 * so the per-instance accessors are unreachable but still need
 * symbols for the BINDINGS table. */

static const char SFTP_UNAVAILABLE[] = "SFTP not available in this build";

#define SFTP_STUB_ALLOC(fn, tag)                                       \
	void fn(WrenVM *vm)                                            \
	{                                                              \
		struct wren_foreign_header *h =                        \
		    wrenSetSlotNewForeign(vm, 0, 0, sizeof(*h));       \
		h->type = (tag);                                       \
	}

SFTP_STUB_ALLOC(wren_sftp_entry_allocate,  SWF_SFTP_ENTRY)
SFTP_STUB_ALLOC(wren_sftp_stat_allocate,   SWF_SFTP_STAT)
SFTP_STUB_ALLOC(wren_sftp_handle_allocate, SWF_SFTP_HANDLE)
SFTP_STUB_ALLOC(wren_sftp_error_allocate,  SWF_SFTP_ERROR)

#undef SFTP_STUB_ALLOC

void wren_sftp_entry_finalize(void *data)  { (void)data; }
void wren_sftp_stat_finalize(void *data)   { (void)data; }
void wren_sftp_handle_finalize(void *data) { (void)data; }
void wren_sftp_error_finalize(void *data)  { (void)data; }

void fn_SFTP_available(WrenVM *vm) { wrenSetSlotBool(vm, 0, false); }
void fn_SFTP_pubdir(WrenVM *vm)    { wrenSetSlotNull(vm, 0); }
void fn_SFTP_lname(WrenVM *vm)     { wrenSetSlotBool(vm, 0, false); }

#define SFTP_STUB(fn) \
	void fn(WrenVM *vm) { wren_throw(vm, SFTP_UNAVAILABLE); }

SFTP_STUB(fn_SFTP_realpath)
SFTP_STUB(fn_SFTP_stat)
SFTP_STUB(fn_SFTP_opendir)
SFTP_STUB(fn_SFTP_readdir)
SFTP_STUB(fn_SFTP_close)
SFTP_STUB(fn_SFTP_open)
SFTP_STUB(fn_SFTP_read)
SFTP_STUB(fn_SFTP_write)
SFTP_STUB(fn_SFTP_mkdir)
SFTP_STUB(fn_SFTP_rmdir)
SFTP_STUB(fn_SFTP_remove)
SFTP_STUB(fn_SFTP_rename)
SFTP_STUB(fn_SFTP_setMtime)
SFTP_STUB(fn_SFTP_descs)

SFTP_STUB(fn_SFTPEntry_name)
SFTP_STUB(fn_SFTPEntry_longname)
SFTP_STUB(fn_SFTPEntry_size)
SFTP_STUB(fn_SFTPEntry_mtime)
SFTP_STUB(fn_SFTPEntry_isDir)
SFTP_STUB(fn_SFTPEntry_hasLongDesc)
SFTP_STUB(fn_SFTPEntry_hash)
SFTP_STUB(fn_SFTPEntry_toString)

SFTP_STUB(fn_SFTPStat_size)
SFTP_STUB(fn_SFTPStat_mtime)
SFTP_STUB(fn_SFTPStat_atime)
SFTP_STUB(fn_SFTPStat_mode)
SFTP_STUB(fn_SFTPStat_uid)
SFTP_STUB(fn_SFTPStat_gid)
SFTP_STUB(fn_SFTPStat_toString)

SFTP_STUB(fn_SFTPError_code)
SFTP_STUB(fn_SFTPError_message)
SFTP_STUB(fn_SFTPError_isTransient)
SFTP_STUB(fn_SFTPError_serverStatus)
SFTP_STUB(fn_SFTPError_toString)

#undef SFTP_STUB

#endif /* WITHOUT_DEUCESSH ------------------------------------------- */

