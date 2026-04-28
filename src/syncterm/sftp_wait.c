/*
 * See sftp_wait.h — throwaway wait shim for the C-side SFTP UI.
 */

#include "sftp_wait.h"

bool
sftp_wait_init(struct sftp_wait *w)
{
	w->evt = CreateEvent(NULL, FALSE /* auto-reset */, FALSE, NULL);
	return w->evt != NULL;
}

void
sftp_wait_destroy(struct sftp_wait *w)
{
	if (w->evt != NULL) {
		CloseEvent(w->evt);
		w->evt = NULL;
	}
}

void
sftp_wait_cb(struct sftpc_pending *p)
{
	struct sftp_wait *w = p->cbdata;
	if (w != NULL && w->evt != NULL)
		SetEvent(w->evt);
}

/* Boilerplate sync wrappers — same pattern for every op:
 *   set up wait state, call async front half, wait if it didn't fail
 *   on alloc, tear down wait state, return the pending.
 * The caller reads pending->err/result/estr and the typed fields,
 * then sftpc_pending_free()s.
 */
#define SFTP_SYNC_DEF(name, ret_type, args_decl, args_call)                \
	ret_type *                                                         \
	sftp_sync_##name args_decl                                         \
	{                                                                  \
		struct sftp_wait w;                                        \
		if (!sftp_wait_init(&w))                                   \
			return NULL;                                       \
		ret_type *p = sftpc_##name args_call;                      \
		if (p != NULL)                                             \
			WaitForEvent(w.evt, INFINITE);                     \
		sftp_wait_destroy(&w);                                     \
		return p;                                                  \
	}

SFTP_SYNC_DEF(init,
    struct sftpc_pending,
    (sftpc_state_t state),
    (state, sftp_wait_cb, &w))

SFTP_SYNC_DEF(realpath,
    struct sftpc_realpath_pending,
    (sftpc_state_t state, const char *path),
    (state, path, sftp_wait_cb, &w))

SFTP_SYNC_DEF(open,
    struct sftpc_open_pending,
    (sftpc_state_t state, const char *path, uint32_t flags),
    (state, path, flags, sftp_wait_cb, &w))

SFTP_SYNC_DEF(opendir,
    struct sftpc_open_pending,
    (sftpc_state_t state, const char *path),
    (state, path, sftp_wait_cb, &w))

SFTP_SYNC_DEF(close,
    struct sftpc_pending,
    (sftpc_state_t state, sftp_str_t handle),
    (state, handle, sftp_wait_cb, &w))

SFTP_SYNC_DEF(read,
    struct sftpc_read_pending,
    (sftpc_state_t state, sftp_str_t handle, uint64_t offset, uint32_t len),
    (state, handle, offset, len, sftp_wait_cb, &w))

SFTP_SYNC_DEF(write,
    struct sftpc_pending,
    (sftpc_state_t state, sftp_str_t handle, uint64_t offset, sftp_str_t data),
    (state, handle, offset, data, sftp_wait_cb, &w))

SFTP_SYNC_DEF(stat,
    struct sftpc_stat_pending,
    (sftpc_state_t state, const char *path),
    (state, path, sftp_wait_cb, &w))

SFTP_SYNC_DEF(setstat,
    struct sftpc_pending,
    (sftpc_state_t state, const char *path, sftp_file_attr_t attr),
    (state, path, attr, sftp_wait_cb, &w))

SFTP_SYNC_DEF(readdir,
    struct sftpc_readdir_pending,
    (sftpc_state_t state, sftp_str_t handle),
    (state, handle, sftp_wait_cb, &w))

SFTP_SYNC_DEF(descs,
    struct sftpc_descs_pending,
    (sftpc_state_t state, const char *path),
    (state, path, sftp_wait_cb, &w))
