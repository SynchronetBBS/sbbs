/* Directory / File / Host foreign surface for the Wren scripting host.
 * Public API is declared in wren_bind_fs.h; the BINDINGS table +
 * lookup_class dispatch live in wren_bind.c.  See wren_bind_internal.h
 * for the shared SWF_* type tags and helpers. */

#include "wren_bind_internal.h"
#include "wren_bind_fs.h"
#include "wren_host_internal.h"
#include "syncterm.h"
#include "term.h"        /* get_cache_fn_base */
#include "uifcinit.h"    /* uifcfilepick / uifcfilepick_multi */

#include "sha1.h"
#include "md5.h"
#include "xpmap.h"

#include "wren_token.h"

#include <dirwrap.h>     /* opendir / readdir / MKDIR */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ----- Directory / File -------------------------------------------- */

struct wren_directory {
	enum syncterm_wren_foreign type;
	/* When is_cache is true, the path is resolved lazily on each
	 * method call from the active BBS context (see wd_resolve).
	 * Otherwise `path` is the literal directory path with trailing
	 * slash. */
	bool is_cache;
	/* When true, the relaxed filename predicate
	 * (fname_is_clean_relaxed) is consulted instead of the strict
	 * fname_is_clean.  Set ONLY on the download root exposed by
	 * Host.downloadDir, and inherited by Directories produced from
	 * that root via list / createDir.  Cache and any of its
	 * descendants stay strict (this flag stays false). */
	bool relaxed_names;
	/* Set by fs_invalidate_subtree when a parent's Directory.delete
	 * removes this entry (or an ancestor of it).  Subsequent ops on
	 * the handle abort the fiber. */
	bool dead;
	/* Doubly-linked list pointers — every live Directory foreign
	 * self-registers on state.fs_dir_head; the finalizer unhooks. */
	struct wren_directory *fs_prev;
	struct wren_directory *fs_next;
	char path[MAX_PATH + 1];
};

/* Write-consent classes for Files minted from the save picker
 * (Host.pickSavePath).  Picker/token read Files are explicitly read-only;
 * sandbox Files from Cache/Download are read/write. */
enum wren_file_write_consent {
	WFC_NONE = 0,    /* ordinary sandbox or picker/token File */
	WFC_CREATE,      /* `wbx` — must not exist; race-safe single create */
	WFC_OVERWRITE,   /* `wb` — overwrite confirmed by picker prompt */
};

enum wren_file_rights {
	WFR_READ  = 1 << 0,
	WFR_WRITE = 1 << 1,
};

struct wren_file {
	enum syncterm_wren_foreign type;
	char  path[MAX_PATH + 1]; /* absolute path */
	FILE *fp;
	/* Opaque base64 consent token for picker-sourced Files
	 * (Host.pickFile / Host.pickFiles).  NULL for Files obtained
	 * via the sandboxed Cache / Download Directory listings —
	 * those don't need a token because their paths are already
	 * inside the predicate-validated sandbox.  Also NULL for
	 * write-consent Files (no persisted token — write consent is
	 * intentionally session-bound and single-shot).  Freed in
	 * wren_file_finalize.  See wren_token.h for the model. */
	char *token;
	unsigned rights;
	/* Write consent class (WFC_*).  WFC_NONE on ordinary Files;
	 * non-NONE on Files minted by Host.pickSavePath. */
	enum wren_file_write_consent write_consent;
	/* Set after the first successful open() on a write-consent
	 * File — any further open()/writeBytes/etc. abort the fiber.
	 * Single-shot is the whole point: an attacker who somehow
	 * grabbed the foreign handle can't replay the write. */
	bool  consent_used;
	/* Same dead/list mechanics as wren_directory.  A successful
	 * Directory.delete on the file (or on an ancestor directory)
	 * marks it dead; further ops abort the fiber. */
	bool  dead;
	struct wren_file *fs_prev;
	struct wren_file *fs_next;
};

/* Recoverable I/O failures (disk full, mmap fails, file vanished out
 * from under an open handle, etc.) surface as a FileError foreign
 * returned IN PLACE of the typed result, mirroring SFTPError.  Bad
 * args, dead handles, and "use after close" stay as Fiber.abort
 * (programmer errors should fail loud).  enum file_err_code is
 * declared in wren_bind_fs.h so other bindings can build FileErrors
 * into their result slots. */

struct wren_file_error {
	enum syncterm_wren_foreign type;
	uint32_t code;     /* enum file_err_code */
	int      err_no;   /* C errno at failure point, 0 if not applicable */
	char    *message;  /* malloc'd; may be NULL */
};

/* Build a FileError into `slot` and write its handle there.  Use
 * `errno_val == 0` when the failure didn't come from a libc call
 * (e.g. OOM in malloc has errno=ENOMEM but synthesised reasons
 * don't).  Never aborts.  Caller is expected to RETURN to the VM
 * immediately after — the error IS the typed-result-slot value.
 * Non-static — declared in wren_bind_fs.h so Capture and
 * CTerm.saveScreenshot can produce FileError typed results. */
void
file_build_error(WrenVM *vm, int slot, enum file_err_code code,
                 int errno_val, const char *msg)
{
	struct wren_host_state *st = wren_host_state();
	wrenEnsureSlots(vm, slot + 2);
	load_class_into_slot(vm, &st->file_error_class, "FileError",
	    slot + 1);
	struct wren_file_error *e =
	    wrenSetSlotNewForeign(vm, slot, slot + 1, sizeof(*e));
	memset(e, 0, sizeof(*e));
	e->type   = SWF_FILE_ERROR;
	e->code   = (uint32_t)code;
	e->err_no = errno_val;
	if (msg != NULL)
		e->message = strdup(msg);
}


/* Reject Windows reserved device names case-insensitively, matching
 * the basename (chars before the first '.').  Both fname_is_clean and
 * fname_is_clean_relaxed need this check; factored out so they share
 * one source of truth. */
static bool
is_reserved_device_basename_(const char *name, size_t base_len)
{
	static const char *reserved[] = {
		"CON", "PRN", "AUX", "NUL",
		"COM1", "COM2", "COM3", "COM4", "COM5",
		"COM6", "COM7", "COM8", "COM9",
		"LPT1", "LPT2", "LPT3", "LPT4", "LPT5",
		"LPT6", "LPT7", "LPT8", "LPT9",
		NULL
	};
	for (int i = 0; reserved[i] != NULL; i++) {
		size_t rlen = strlen(reserved[i]);
		if (base_len != rlen)
			continue;
		bool match = true;
		for (size_t j = 0; j < rlen; j++) {
			char nc = name[j];
			char rc = reserved[i][j];
			if (nc >= 'a' && nc <= 'z')
				nc = (char)(nc - 32);
			if (nc != rc) {
				match = false;
				break;
			}
		}
		if (match)
			return true;
	}
	return false;
}

/* Strict filename policy used by Cache (and any non-Host-downloadDir
 * Directory): 1..64 chars, only [A-Za-z0-9._-], no leading '.' or '-',
 * no trailing '.', no ".." substring, basename not a Windows reserved
 * device. */
static bool
fname_is_clean(const char *name)
{
	if (name == NULL)
		return false;
	size_t n = strlen(name);
	if (n < 1 || n > 64)
		return false;
	for (size_t i = 0; i < n; i++) {
		char c = name[i];
		bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
		    || (c >= '0' && c <= '9') || c == '.' || c == '_'
		    || c == '-';
		if (!ok)
			return false;
	}
	if (name[0] == '.' || name[0] == '-')
		return false;
	if (name[n - 1] == '.')
		return false;
	if (strstr(name, "..") != NULL)
		return false;
	const char *dot = strchr(name, '.');
	size_t base_len = dot ? (size_t)(dot - name) : n;
	if (is_reserved_device_basename_(name, base_len))
		return false;
	return true;
}

/* Relaxed filename policy for the download / upload roots and their
 * descendants: 1..255 bytes, no NUL, no control bytes (0x00..0x1F,
 * 0x7F), no path separators (/, \), name not "." or "..", basename
 * not a Windows reserved device.  Leading dot is ALLOWED (hidden
 * files are real on Unix); trailing dot, dashes, spaces, parens, and
 * UTF-8 bytes pass through. */
static bool
fname_is_clean_relaxed(const char *name)
{
	if (name == NULL)
		return false;
	size_t n = strlen(name);
	if (n < 1 || n > 255)
		return false;
	for (size_t i = 0; i < n; i++) {
		unsigned char c = (unsigned char)name[i];
		if (c <= 0x1F || c == 0x7F)
			return false;
		if (c == '/' || c == '\\')
			return false;
	}
	if (n == 1 && name[0] == '.')
		return false;
	if (n == 2 && name[0] == '.' && name[1] == '.')
		return false;
	const char *dot = strchr(name, '.');
	size_t base_len = dot ? (size_t)(dot - name) : n;
	if (is_reserved_device_basename_(name, base_len))
		return false;
	return true;
}

/* Per-directory filename predicate dispatch.  Strict for everything
 * except download / upload roots and their descendants. */
static bool
fname_check_(struct wren_directory *wd, const char *name)
{
	if (wd != NULL && wd->relaxed_names)
		return fname_is_clean_relaxed(name);
	return fname_is_clean(name);
}

/* Identify "unfortunate default" download / upload paths from older
 * SyncTERM versions: empty string, NULL, or the user's $HOME.
 * Comparing trailing-slash-insensitively in case bbslist normalized
 * one form vs the other.  Case-sensitive on POSIX, insensitive on
 * Windows. */
static bool
is_dangerous_default_(const char *path)
{
	if (path == NULL || path[0] == '\0')
		return true;
	const char *home = getenv("HOME");
#ifdef _WIN32
	if (home == NULL)
		home = getenv("USERPROFILE");
#endif
	if (home == NULL || home[0] == '\0')
		return false;
	size_t pn = strlen(path);
	size_t hn = strlen(home);
	while (pn > 1 && (path[pn - 1] == '/' || path[pn - 1] == '\\'))
		pn--;
	while (hn > 1 && (home[hn - 1] == '/' || home[hn - 1] == '\\'))
		hn--;
	if (pn != hn)
		return false;
#ifdef _WIN32
	for (size_t i = 0; i < pn; i++) {
		char p = path[i];
		char h = home[i];
		if (p >= 'a' && p <= 'z') p = (char)(p - 32);
		if (h >= 'a' && h <= 'z') h = (char)(h - 32);
		if (p != h)
			return false;
	}
#else
	if (memcmp(path, home, pn) != 0)
		return false;
#endif
	return true;
}

/* ----- File / Directory live-foreign registry ---------------------- *
 * Every live wren_file and wren_directory self-registers on a
 * doubly-linked list rooted at state.fs_*_head.  Allocation sites
 * (allocator callbacks + the C-side push helpers used by Directory
 * methods) call fs_register_*; finalizers call fs_unregister_*.
 *
 * Directory.delete walks both lists via fs_invalidate_subtree, marking
 * dead any handle whose path is at-or-below the removed entry.  All
 * subsequent ops on a dead handle hit fs_throw_if_dead and abort the
 * calling fiber — the handle never silently operates on the wrong
 * underlying path.
 *
 * Single-threaded: the Wren VM is owner-thread-only, so list mutations
 * don't need locks.  Finalizers run during GC, which only runs while
 * the VM is otherwise idle on the owner thread. */

static void
fs_register_file(struct wren_file *wf)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL)
		return;
	wf->fs_prev = NULL;
	wf->fs_next = st->fs_file_head;
	if (st->fs_file_head != NULL)
		st->fs_file_head->fs_prev = wf;
	st->fs_file_head = wf;
}

static void
fs_unregister_file(struct wren_file *wf)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL)
		return;
	if (wf->fs_prev != NULL)
		wf->fs_prev->fs_next = wf->fs_next;
	else if (st->fs_file_head == wf)
		st->fs_file_head = wf->fs_next;
	if (wf->fs_next != NULL)
		wf->fs_next->fs_prev = wf->fs_prev;
	wf->fs_prev = NULL;
	wf->fs_next = NULL;
}

static void
fs_register_dir(struct wren_directory *wd)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL)
		return;
	wd->fs_prev = NULL;
	wd->fs_next = st->fs_dir_head;
	if (st->fs_dir_head != NULL)
		st->fs_dir_head->fs_prev = wd;
	st->fs_dir_head = wd;
}

static void
fs_unregister_dir(struct wren_directory *wd)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL)
		return;
	if (wd->fs_prev != NULL)
		wd->fs_prev->fs_next = wd->fs_next;
	else if (st->fs_dir_head == wd)
		st->fs_dir_head = wd->fs_next;
	if (wd->fs_next != NULL)
		wd->fs_next->fs_prev = wd->fs_prev;
	wd->fs_prev = NULL;
	wd->fs_next = NULL;
}

/* Mark dead every live File whose path equals `path` exactly OR whose
 * path is `path + "/" + ...` (a child of a removed directory), and
 * every live Directory whose path starts with `path + "/"` (which
 * catches the directory itself when `path` was a directory, plus all
 * its descendants).  Caller has already removed `path` from disk. */
static void
fs_invalidate_subtree(const char *path)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || path == NULL)
		return;
	size_t plen = strlen(path);
	char pp[MAX_PATH + 2];
	if (plen + 1 >= sizeof(pp))
		return;
	memcpy(pp, path, plen);
	pp[plen]     = '/';
	pp[plen + 1] = '\0';
	size_t pplen = plen + 1;
	for (struct wren_file *wf = st->fs_file_head; wf != NULL;
	    wf = wf->fs_next) {
		/* Open files survive — on Unix the fd outlives unlink, and
		 * on Windows an open file can't be deleted in the first
		 * place.  fn_File_close re-checks existence at close time
		 * and marks the handle dead then if its path is gone. */
		if (wf->fp != NULL)
			continue;
		if (strcmp(wf->path, path) == 0 ||
		    strncmp(wf->path, pp, pplen) == 0)
			wf->dead = true;
	}
	for (struct wren_directory *wd = st->fs_dir_head; wd != NULL;
	    wd = wd->fs_next) {
		if (strncmp(wd->path, pp, pplen) == 0)
			wd->dead = true;
	}
}

void
wren_directory_allocate(WrenVM *vm)
{
	struct wren_directory *wd = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*wd));
	wd->type     = SWF_DIRECTORY;
	wd->is_cache = false;
	wd->dead     = false;
	wd->fs_prev  = NULL;
	wd->fs_next  = NULL;
	wd->path[0]  = '\0';
	fs_register_dir(wd);
}

static const char *
wd_resolve(WrenVM *vm, struct wren_directory *wd, char *scratch,
    size_t scratchsz)
{
	if (!wd->is_cache)
		return wd->path;
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->bbs == NULL) {
		wren_throw(vm, "Cache: no BBS context");
		return NULL;
	}
	if (!get_cache_fn_base(st->bbs, scratch, scratchsz)) {
		file_build_error(vm, 0, FILE_ERR_RESOLVE_FAILED, 0,
		    "Cache: failed to resolve path");
		return NULL;
	}
	return scratch;
}

void
wren_directory_finalize(void *data)
{
	struct wren_directory *wd = data;
	fs_unregister_dir(wd);
}

void
wren_file_allocate(WrenVM *vm)
{
	struct wren_file *wf = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*wf));
	wf->type    = SWF_FILE;
	wf->path[0] = '\0';
	wf->fp      = NULL;
	wf->token   = NULL;
	wf->rights        = WFR_READ | WFR_WRITE;
	wf->write_consent = WFC_NONE;
	wf->consent_used  = false;
	wf->dead    = false;
	wf->fs_prev = NULL;
	wf->fs_next = NULL;
	fs_register_file(wf);
}

void
wren_file_finalize(void *data)
{
	struct wren_file *wf = data;
	fs_unregister_file(wf);
	if (wf->fp != NULL) {
		fclose(wf->fp);
		wf->fp = NULL;
	}
	free(wf->token);
	wf->token = NULL;
}

void
wren_file_error_allocate(WrenVM *vm)
{
	struct wren_file_error *e =
	    wrenSetSlotNewForeign(vm, 0, 0, sizeof(*e));
	memset(e, 0, sizeof(*e));
	e->type = SWF_FILE_ERROR;
}

void
wren_file_error_finalize(void *data)
{
	struct wren_file_error *e = data;
	free(e->message);
}

void
fn_FileError_code(WrenVM *vm)
{
	struct wren_file_error *e = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)e->code);
}

void
fn_FileError_errno(WrenVM *vm)
{
	struct wren_file_error *e = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)e->err_no);
}

void
fn_FileError_message(WrenVM *vm)
{
	struct wren_file_error *e = wrenGetSlotForeign(vm, 0);
	if (e->message != NULL)
		wrenSetSlotString(vm, 0, e->message);
	else
		wrenSetSlotNull(vm, 0);
}

/* Human-friendly form: "FileError: <code-name>: <message>".
 * Mirrors SFTPError.toString. */
void
fn_FileError_toString(WrenVM *vm)
{
	struct wren_file_error *e = wrenGetSlotForeign(vm, 0);
	const char *name;
	switch ((enum file_err_code)e->code) {
	case FILE_ERR_OK:             name = "OK";              break;
	case FILE_ERR_OPEN_FAILED:    name = "OPEN_FAILED";     break;
	case FILE_ERR_WRITE_FAILED:   name = "WRITE_FAILED";    break;
	case FILE_ERR_STAT_FAILED:    name = "STAT_FAILED";     break;
	case FILE_ERR_MMAP_FAILED:    name = "MMAP_FAILED";     break;
	case FILE_ERR_OOM:            name = "OOM";             break;
	case FILE_ERR_VANISHED:       name = "VANISHED";        break;
	case FILE_ERR_RESOLVE_FAILED: name = "RESOLVE_FAILED";  break;
	default:                      name = "UNKNOWN";         break;
	}
	char buf[256];
	if (e->message != NULL && e->err_no != 0)
		snprintf(buf, sizeof(buf), "FileError: %s: %s (errno=%d)",
		    name, e->message, e->err_no);
	else if (e->message != NULL)
		snprintf(buf, sizeof(buf), "FileError: %s: %s",
		    name, e->message);
	else if (e->err_no != 0)
		snprintf(buf, sizeof(buf), "FileError: %s (errno=%d)",
		    name, e->err_no);
	else
		snprintf(buf, sizeof(buf), "FileError: %s", name);
	wrenSetSlotString(vm, 0, buf);
}

/* Mark a foreign as dead and unhook it from the live-list immediately.
 * The GC finalizer will run later but it's a no-op on an already-
 * unregistered entry (the unregister helpers tolerate that). */
static void
fs_kill_file(struct wren_file *wf)
{
	wf->dead = true;
	fs_unregister_file(wf);
}

static void
fs_kill_dir(struct wren_directory *wd)
{
	wd->dead = true;
	fs_unregister_dir(wd);
}

/* Receiver-validation helpers used at the top of every File / Directory
 * foreign method.  They abort the calling fiber on:
 *   - the `dead` flag set (a previous Directory.delete invalidated the
 *     handle, possibly transitively via an ancestor),
 *   - the underlying path no longer existing on disk (something
 *     outside the script poked the filesystem since the handle was
 *     issued).  In that case the handle is also marked dead and pulled
 *     from the live-list before the throw, so a subsequent
 *     Directory.delete walk doesn't re-encounter it. */
static struct wren_file *
file_check(WrenVM *vm)
{
	struct wren_file *wf = wrenGetSlotForeign(vm, 0);
	if (wf->dead) {
		wren_throw(vm, "File: handle is dead "
		    "(its parent directory invalidated it via Directory.delete)");
		return NULL;
	}
	/* Write-consent Files that have been used (opened then closed)
	 * are inert — single-shot is the whole point of the consent
	 * model.  Trips for any subsequent open(), writeBytes(), etc. */
	if (wf->write_consent != WFC_NONE && wf->consent_used && wf->fp == NULL) {
		wren_throw(vm, "File: write consent already used "
		    "(re-pick the path to write again)");
		return NULL;
	}
	/* Open files survive their backing path's removal — on Unix the
	 * fd stays valid after unlink, and on Windows you can't delete an
	 * open file at all.  Skip the existence check; let the OS return
	 * I/O errors through subsequent reads/writes if any.  The recheck
	 * happens in fn_File_close. */
	if (wf->fp != NULL)
		return wf;
	/* Pre-open write-consent Files may legitimately point at a path
	 * that doesn't exist yet (WFC_CREATE) — skip the existence check.
	 * fn_File_open's fopen mode ("wbx" / "wb") enforces the right
	 * race-safe semantics at open time. */
	if (wf->write_consent != WFC_NONE)
		return wf;
	if (!fexist(wf->path)) {
		fs_kill_file(wf);
		file_build_error(vm, 0, FILE_ERR_VANISHED, 0,
		    "File: backing file no longer exists");
		return NULL;
	}
	return wf;
}

static struct wren_directory *
dir_check(WrenVM *vm)
{
	struct wren_directory *wd = wrenGetSlotForeign(vm, 0);
	/* Cache directories resolve their path lazily on every call (see
	 * wd_resolve), so we can't fexist-check them here.  wd_resolve does
	 * its own existence check after resolving the BBS-relative path.
	 *
	 * Run the existence check before the `dead` check so a backing
	 * vanish reports its true cause every time, not just on the
	 * first call (fs_kill_dir sets `dead`, which would otherwise
	 * cause subsequent calls to surface the misleading
	 * "ancestor's Directory.delete" message). */
	if (!wd->is_cache && !isdir(wd->path)) {
		fs_kill_dir(wd);
		file_build_error(vm, 0, FILE_ERR_VANISHED, 0,
		    "Directory: backing directory no longer exists");
		return NULL;
	}
	if (wd->dead) {
		wren_throw(vm, "Directory: handle is dead "
		    "(an ancestor's Directory.delete invalidated it)");
		return NULL;
	}
	return wd;
}

/* Build a File foreign instance into `slot` (uses slot+1 as scratch
 * for the class lookup).  Caller must pre-validate `name`. */
static struct wren_file *
push_new_file(WrenVM *vm, int slot, const char *dir, const char *name)
{
	wrenEnsureSlots(vm, slot + 2);
	wrenGetVariable(vm, "syncterm", "File", slot + 1);
	struct wren_file *wf = wrenSetSlotNewForeign(vm, slot, slot + 1,
	    sizeof(*wf));
	wf->type          = SWF_FILE;
	wf->fp            = NULL;
	wf->token         = NULL;
	wf->rights        = WFR_READ | WFR_WRITE;
	wf->write_consent = WFC_NONE;
	wf->consent_used  = false;
	wf->dead          = false;
	wf->fs_prev       = NULL;
	wf->fs_next       = NULL;
	snprintf(wf->path, sizeof(wf->path), "%s%s", dir, name);
	fs_register_file(wf);
	return wf;
}

static void
dir_contains_impl(WrenVM *vm, struct wren_directory *parent,
    const char *dir, const char *name)
{
	if (!fname_check_(parent, name)) {
		wren_throw(vm, "Directory: invalid file name");
		return;
	}
	char p[MAX_PATH + 1];
	int len = snprintf(p, sizeof(p), "%s%s", dir, name);
	if (len < 0 || (size_t)len >= sizeof(p)) {
		wren_throw(vm, "Directory: path too long");
		return;
	}
	/* Regular-file probe via the portable xpdev helpers — fexist()
	 * confirms presence, isdir() rules out directories.  S_ISREG /
	 * struct stat aren't a POSIX baseline on Win32 / MSVC. */
	bool yes = fexist(p) && !isdir(p);
	wrenSetSlotBool(vm, 0, yes);
}

static void
dir_list_impl(WrenVM *vm, struct wren_directory *parent, const char *dir)
{
	wrenEnsureSlots(vm, 4);
	wrenSetSlotNewMap(vm, 0);
	DIR *dh = opendir(dir);
	if (dh == NULL)
		return;
	struct dirent *de;
	char p[MAX_PATH + 1];
	bool relaxed = parent != NULL && parent->relaxed_names;
	while ((de = readdir(dh)) != NULL) {
		if (!fname_check_(parent, de->d_name))
			continue;
		int len = snprintf(p, sizeof(p), "%s%s", dir, de->d_name);
		if (len < 0 || (size_t)len >= sizeof(p))
			continue;
		wrenSetSlotString(vm, 1, de->d_name);
		if (isdir(p)) {
			/* Subsequent dir_*_impl calls expect a trailing slash on
			 * the path so name concatenation works. */
			if ((size_t)len + 1 >= sizeof(p))
				continue;
			wrenGetVariable(vm, "syncterm", "Directory", 2);
			struct wren_directory *wd = wrenSetSlotNewForeign(vm, 3,
			    2, sizeof(*wd));
			wd->type          = SWF_DIRECTORY;
			wd->is_cache      = false;
			wd->relaxed_names = relaxed;
			wd->dead          = false;
			wd->fs_prev       = NULL;
			wd->fs_next       = NULL;
			memcpy(wd->path, p, (size_t)len);
			wd->path[len]     = '/';
			wd->path[len + 1] = '\0';
			fs_register_dir(wd);
			wrenSetMapValue(vm, 0, 1, 3);
		}
		else if (fexist(p)) {
			wrenGetVariable(vm, "syncterm", "File", 2);
			struct wren_file *wf = wrenSetSlotNewForeign(vm, 3, 2,
			    sizeof(*wf));
			wf->type          = SWF_FILE;
			wf->fp            = NULL;
			wf->token         = NULL;
			wf->rights        = WFR_READ | WFR_WRITE;
			wf->write_consent = WFC_NONE;
			wf->consent_used  = false;
			wf->dead          = false;
			wf->fs_prev       = NULL;
			wf->fs_next       = NULL;
			memcpy(wf->path, p, (size_t)len + 1);
			fs_register_file(wf);
			wrenSetMapValue(vm, 0, 1, 3);
		}
		/* Anything else (entries that don't exist by the time we
		 * stat them, or that the OS classifies as neither file nor
		 * directory) is silently skipped. */
	}
	closedir(dh);
}

static void
dir_create_impl(WrenVM *vm, struct wren_directory *parent,
    const char *dir, const char *name)
{
	wrenEnsureSlots(vm, 1);
	if (!fname_check_(parent, name)) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	char p[MAX_PATH + 1];
	int len = snprintf(p, sizeof(p), "%s%s", dir, name);
	if (len < 0 || (size_t)len >= sizeof(p)) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	/* "wbx" is C11 exclusive-create — fopen returns NULL atomically
	 * if the file already exists, sidestepping the stat-then-open
	 * race window.  Also covers "no permission", "directory missing",
	 * etc.; we don't distinguish, scripts get null on any failure. */
	FILE *f = fopen(p, "wbx");
	if (f == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	fclose(f);
	push_new_file(vm, 0, dir, name);
}

static void
dir_createdir_impl(WrenVM *vm, struct wren_directory *parent,
    const char *dir, const char *name)
{
	wrenEnsureSlots(vm, 2);
	if (!fname_check_(parent, name)) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	char p[MAX_PATH + 1];
	int len = snprintf(p, sizeof(p), "%s%s", dir, name);
	if (len < 0 || (size_t)len >= sizeof(p)) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	/* MKDIR fails atomically if the path already exists (EEXIST) —
	 * the portable equivalent of fopen("wbx") for directories.  Any
	 * other OS rejection (no perm, parent missing, …) lands in the
	 * same null-return branch. */
	if (MKDIR(p) != 0) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	/* Subsequent dir_*_impl calls on the returned Directory expect a
	 * trailing slash on the path so name concatenation works. */
	if ((size_t)len + 1 >= sizeof(p)) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenGetVariable(vm, "syncterm", "Directory", 1);
	struct wren_directory *wd = wrenSetSlotNewForeign(vm, 0, 1,
	    sizeof(*wd));
	wd->type          = SWF_DIRECTORY;
	wd->is_cache      = false;
	wd->relaxed_names = parent != NULL && parent->relaxed_names;
	wd->dead          = false;
	wd->fs_prev       = NULL;
	wd->fs_next       = NULL;
	memcpy(wd->path, p, (size_t)len);
	wd->path[len]     = '/';
	wd->path[len + 1] = '\0';
	fs_register_dir(wd);
}

/* ----- Directory foreign methods ----------------------------------- */

void
fn_Directory_contains(WrenVM *vm)
{
	struct wren_directory *wd = dir_check(vm);
	if (wd == NULL)
		return;
	char scratch[MAX_PATH + 1];
	const char *dir = wd_resolve(vm, wd, scratch, sizeof(scratch));
	if (dir == NULL)
		return;
	const char *name = wrenGetSlotString(vm, 1);
	dir_contains_impl(vm, wd, dir, name);
}

void
fn_Directory_list(WrenVM *vm)
{
	struct wren_directory *wd = dir_check(vm);
	if (wd == NULL)
		return;
	char scratch[MAX_PATH + 1];
	const char *dir = wd_resolve(vm, wd, scratch, sizeof(scratch));
	if (dir == NULL)
		return;
	dir_list_impl(vm, wd, dir);
}

void
fn_Directory_create(WrenVM *vm)
{
	struct wren_directory *wd = dir_check(vm);
	if (wd == NULL)
		return;
	char scratch[MAX_PATH + 1];
	const char *dir = wd_resolve(vm, wd, scratch, sizeof(scratch));
	if (dir == NULL)
		return;
	const char *name = wrenGetSlotString(vm, 1);
	dir_create_impl(vm, wd, dir, name);
}

void
fn_Directory_createDir(WrenVM *vm)
{
	struct wren_directory *wd = dir_check(vm);
	if (wd == NULL)
		return;
	char scratch[MAX_PATH + 1];
	const char *dir = wd_resolve(vm, wd, scratch, sizeof(scratch));
	if (dir == NULL)
		return;
	const char *name = wrenGetSlotString(vm, 1);
	dir_createdir_impl(vm, wd, dir, name);
}

static void
dir_delete_impl(WrenVM *vm, struct wren_directory *parent,
    const char *dir, const char *name)
{
	wrenEnsureSlots(vm, 1);
	if (!fname_check_(parent, name)) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	char p[MAX_PATH + 1];
	int len = snprintf(p, sizeof(p), "%s%s", dir, name);
	if (len < 0 || (size_t)len >= sizeof(p)) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	/* Refuse to act on anything that isn't an existing file or
	 * directory.  fexist() is xpdev's "exists as a non-directory"
	 * (true for regular files, symlinks, devices on POSIX); isdir()
	 * is the directory test.  fname_check_ already rejects path
	 * traversal in the name. */
	if (!fexist(p) && !isdir(p)) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	/* remove() handles both regular files and empty directories.
	 * Non-empty directories fail with ENOTEMPTY → false. */
	if (remove(p) != 0) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	fs_invalidate_subtree(p);
	wrenSetSlotBool(vm, 0, true);
}

void
fn_Directory_delete(WrenVM *vm)
{
	struct wren_directory *wd = dir_check(vm);
	if (wd == NULL)
		return;
	char scratch[MAX_PATH + 1];
	const char *dir = wd_resolve(vm, wd, scratch, sizeof(scratch));
	if (dir == NULL)
		return;
	const char *name = wrenGetSlotString(vm, 1);
	dir_delete_impl(vm, wd, dir, name);
}

/* Host.cacheDirectory — returns a fresh Directory foreign whose path
 * is resolved lazily from the current BBS context on every method
 * call.  No Wren-side constructor exists for an `is_cache` Directory,
 * so this is the only way to obtain one; syncterm.wren binds its
 * result to the module-level `Cache` variable at module-load time. */
void
fn_Host_cacheDirectory(WrenVM *vm)
{
	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "syncterm", "Directory", 1);
	struct wren_directory *wd = wrenSetSlotNewForeign(vm, 0, 1,
	    sizeof(*wd));
	wd->type          = SWF_DIRECTORY;
	wd->is_cache      = true;
	wd->relaxed_names = false;
	wd->dead          = false;
	wd->fs_prev       = NULL;
	wd->fs_next       = NULL;
	wd->path[0]       = '\0';
	fs_register_dir(wd);
}

/* Helper for Host.downloadDir — hands back a Directory rooted at
 * bbs->dldir with the relaxed filename predicate enabled.  Returns
 * null when:
 *   - the path is empty/unset, or
 *   - it equals $HOME (the unfortunate-default guard), or
 *   - the path doesn't exist as a directory on disk.
 * Consumers (sftp_app's status chip, sftp_queue's download path, …)
 * already null-check the Wren-side `Download` constant; routing the
 * "configured but missing" case through the same null gate avoids
 * making every call site re-discover the missing-dir state through
 * a `FILE_ERR_VANISHED` thrown three calls deep. */
static void
push_relaxed_root_(WrenVM *vm, const char *path)
{
	wrenEnsureSlots(vm, 2);
	if (is_dangerous_default_(path)) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	if (!isdir(path)) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	size_t pn = strlen(path);
	bool need_slash = pn > 0 && path[pn - 1] != '/' && path[pn - 1] != '\\';
	if (pn + (need_slash ? 1 : 0) >= MAX_PATH) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenGetVariable(vm, "syncterm", "Directory", 1);
	struct wren_directory *wd = wrenSetSlotNewForeign(vm, 0, 1,
	    sizeof(*wd));
	wd->type          = SWF_DIRECTORY;
	wd->is_cache      = false;
	wd->relaxed_names = true;
	wd->dead          = false;
	wd->fs_prev       = NULL;
	wd->fs_next       = NULL;
	memcpy(wd->path, path, pn);
	if (need_slash)
		wd->path[pn++] = '/';
	wd->path[pn] = '\0';
	fs_register_dir(wd);
}

/* Host.downloadDir — relaxed-name Directory rooted at bbs->dldir.
 * Returns null when dldir is empty, equals $HOME, or doesn't exist
 * as a directory on disk (see push_relaxed_root_ for the rationale
 * — consumer code already null-checks `Download`). */
void
fn_Host_downloadDir(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->bbs == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	push_relaxed_root_(vm, st->bbs->dldir);
}

/* Host.uploadPath — the BBS's configured UploadPath as a String, or
 * null when no BBS context or the path is empty.  Intended for use as
 * the initialDir argument to Host.pickFile / Host.pickFiles — uploads
 * must go through the picker (which mints a consent token) and never
 * by enumerating a script-controlled Directory.  No $HOME guard:
 * scripts get the configured value verbatim. */
void
fn_Host_uploadPath(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->bbs == NULL || st->bbs->uldir[0] == '\0') {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotString(vm, 0, st->bbs->uldir);
}

/* Resolve `slot`'s value into an initial-directory string for the
 * filepick widget.  Accepts:
 *   - String: used verbatim.
 *   - Directory foreign: uses its resolved path (handles Cache's
 *     lazy-resolve via wd_resolve).
 *   - Anything else (including null): falls back to bbs->uldir or
 *     NULL if no BBS / no uldir.
 * `scratch` is filled when a Cache Directory needs resolving;
 * otherwise the returned pointer aliases the slot's String or the
 * Directory's path field. */
static const char *
pick_extract_initial(WrenVM *vm, int slot, char *scratch, size_t scratchsz)
{
	if (wrenGetSlotType(vm, slot) == WREN_TYPE_STRING) {
		const char *s = wrenGetSlotString(vm, slot);
		if (s != NULL && s[0] != '\0')
			return s;
	}
	else if (slot_foreign_type(vm, slot) == SWF_DIRECTORY) {
		struct wren_directory *wd = wrenGetSlotForeign(vm, slot);
		if (!wd->dead) {
			const char *p = wd_resolve(vm, wd, scratch, scratchsz);
			if (p != NULL && p[0] != '\0')
				return p;
		}
	}
	struct wren_host_state *st = wren_host_state();
	if (st != NULL && st->bbs != NULL && st->bbs->uldir[0] != '\0')
		return st->bbs->uldir;
	return NULL;
}

/* Allocate a File foreign at `slot` for an absolute path returned
 * by the picker.  Sets path + signs a consent token via
 * wren_token_sign (NULL on no-key / unreadable / OOM — the File is
 * still valid for one-shot session use, but won't persist).
 * Returns the wren_file pointer, or NULL when path is too long. */
static struct wren_file *
push_picker_file(WrenVM *vm, int slot, const char *picked)
{
	if (picked == NULL || strlen(picked) >= MAX_PATH)
		return NULL;
	wrenEnsureSlots(vm, slot + 2);
	wrenGetVariable(vm, "syncterm", "File", slot + 1);
	struct wren_file *wf = wrenSetSlotNewForeign(vm, slot, slot + 1,
	    sizeof(*wf));
	wf->type          = SWF_FILE;
	wf->fp            = NULL;
	wf->token         = wren_token_sign(picked);
	wf->rights        = WFR_READ;
	wf->write_consent = WFC_NONE;
	wf->consent_used  = false;
	wf->dead          = false;
	wf->fs_prev       = NULL;
	wf->fs_next       = NULL;
	strlcpy(wf->path, picked, sizeof(wf->path));
	fs_register_file(wf);
	return wf;
}

/* Mint a write-consent File for a path produced by Host.pickSavePath.
 * `mode` records the authorized open mode (WFC_CREATE for new paths,
 * WFC_OVERWRITE when the picker's overwrite prompt got a yes).
 * Token is intentionally NULL — write consent is single-shot and
 * doesn't persist across sessions (replay would defeat the model).
 * Returns the wren_file pointer, or NULL when path is too long. */
static struct wren_file *
push_picker_save_file(WrenVM *vm, int slot, const char *picked,
                      enum wren_file_write_consent mode)
{
	if (picked == NULL || strlen(picked) >= MAX_PATH)
		return NULL;
	wrenEnsureSlots(vm, slot + 2);
	wrenGetVariable(vm, "syncterm", "File", slot + 1);
	struct wren_file *wf = wrenSetSlotNewForeign(vm, slot, slot + 1,
	    sizeof(*wf));
	wf->type          = SWF_FILE;
	wf->fp            = NULL;
	wf->token         = NULL;
	wf->rights        = WFR_WRITE;
	wf->write_consent = mode;
	wf->consent_used  = false;
	wf->dead          = false;
	wf->fs_prev       = NULL;
	wf->fs_next       = NULL;
	strlcpy(wf->path, picked, sizeof(wf->path));
	fs_register_file(wf);
	return wf;
}

/* Host.pickFile(initialDir, mask, opts) — wraps uifc/filepick's
 * single-file picker.  initialDir may be a String path, a Directory
 * foreign (uses its path; handles Cache lazy-resolve), or null
 * (defaults to bbs->uldir).  mask may be null (defaults to "*").
 * opts is the UIFC_FP_* bitmask.
 *
 * Returns a File foreign whose path is the absolute path the user
 * picked (with .token populated when the signing key is available),
 * or null on cancel / escape / OOM.  This is the user-consent
 * escape hatch for "upload from anywhere": the returned File's
 * path is NOT validated against fname_is_clean*, since the picker
 * UI itself is the sandbox boundary. */
static void
host_pick_file(WrenVM *vm, const char *title)
{
	char        scratch[MAX_PATH + 1];
	const char *initial = pick_extract_initial(vm, 1,
	    scratch, sizeof(scratch));
	const char *mask    = NULL;
	int         opts    = 0;
	if (wrenGetSlotType(vm, 2) == WREN_TYPE_STRING)
		mask = wrenGetSlotString(vm, 2);
	if (wrenGetSlotType(vm, 3) == WREN_TYPE_NUM)
		opts = (int)wrenGetSlotDouble(vm, 3);

	struct file_pick fp = { 0 };
	int rc = uifcfilepick(title, &fp, initial, mask, opts);
	if (rc <= 0 || fp.files < 1 || fp.selected == NULL ||
	    fp.selected[0] == NULL) {
		filepick_free(&fp);
		wrenSetSlotNull(vm, 0);
		return;
	}
	if (push_picker_file(vm, 0, fp.selected[0]) == NULL)
		wrenSetSlotNull(vm, 0);
	filepick_free(&fp);
}

void
fn_Host_pickFile(WrenVM *vm)
{
	host_pick_file(vm, "Pick a file");
}

void
fn_Host_pickFileTitle(WrenVM *vm)
{
	if (wrenGetSlotType(vm, 4) != WREN_TYPE_STRING) {
		wren_throw(vm, "Host.pickFile: title must be a String");
		return;
	}
	host_pick_file(vm, wrenGetSlotString(vm, 4));
}

/* Consume the write-consent on a File foreign in the given slot.
 * Opens the underlying path with the authorized mode (`wbx` for
 * WFC_CREATE, `wb` for WFC_OVERWRITE) and transfers ownership of the
 * resulting FILE* to the caller.  Marks the foreign's consent used
 * so subsequent open() / write / etc. abort the fiber.
 *
 * On programmer error (no consent / already used / read-mode File)
 * aborts the calling fiber via wren_throw and returns NULL.  On
 * fopen failure, builds a FileError into slot 0 and returns NULL —
 * caller should return immediately (the slot 0 contents are the
 * binding's typed-result).  *out_path receives a pointer into the
 * foreign's path field; valid for the duration of the binding call.
 *
 * Used by Capture.start (transfers the FILE* into cterm->logfile)
 * and CTerm.saveScreenshot (the binding does its own fwrite/fclose). */
/* Read-authorized path accessor for a File foreign in slot.  Returns the
 * absolute path on success, or NULL when the slot's not a File or
 * the handle is dead or write-only.  Caller must NOT free the returned
 * pointer (it lives inside the File foreign).  Used by foreigns that
 * hand the path off to a path-taking C API but don't need write consent;
 * the returned path is still consent-bounded by the File. */
const char *
wren_file_read_path(WrenVM *vm, int slot)
{
	if (slot_foreign_type(vm, slot) != SWF_FILE)
		return NULL;
	struct wren_file *wf = wrenGetSlotForeign(vm, slot);
	if (wf == NULL || wf->dead || (wf->rights & WFR_READ) == 0)
		return NULL;
	return wf->path;
}

FILE *
wren_file_consume_write(WrenVM *vm, int slot, const char **out_path)
{
	struct wren_file *wf = wrenGetSlotForeign(vm, slot);
	if (wf == NULL) {
		wren_throw(vm, "expected a File");
		return NULL;
	}
	if (wf->dead) {
		wren_throw(vm, "File: handle is dead");
		return NULL;
	}
	if (wf->write_consent == WFC_NONE) {
		wren_throw(vm,
		    "File: no write consent (use Host.pickSavePath)");
		return NULL;
	}
	if (wf->consent_used || wf->fp != NULL) {
		wren_throw(vm,
		    "File: write consent already used (re-pick to write again)");
		return NULL;
	}
	const char *mode =
	    (wf->write_consent == WFC_CREATE) ? "wbx" : "wb";
	FILE *fp = fopen(wf->path, mode);
	if (fp == NULL) {
		file_build_error(vm, 0, FILE_ERR_OPEN_FAILED, errno,
		    "fopen failed");
		return NULL;
	}
	wf->consent_used = true;
	if (out_path != NULL)
		*out_path = wf->path;
	return fp;
}

/* Host.pickSavePath(initialDir, mask) — wraps uifc filepick in save
 * mode (UIFC_FP_ALLOWENTRY | UIFC_FP_OVERPROMPT).  User can either
 * pick an existing file (overwrite-prompted) or type a new filename.
 *
 * Returns a write-consent File or null on cancel.  The File's
 * write_consent records which open mode the picker authorized:
 *   - WFC_CREATE     when the path didn't exist at pick time
 *                    (open() uses "wbx" — race-safe single create)
 *   - WFC_OVERWRITE  when the path existed and the user confirmed
 *                    overwrite (open() uses "wb" — truncate)
 *
 * The consent is single-shot: the first successful open()+close()
 * marks the File inert; subsequent ops abort.  No persisted token —
 * write consent doesn't replay across sessions (the historical
 * .token mechanism only covers read consent for upload resume). */
void
fn_Host_pickSavePath(WrenVM *vm)
{
	char        scratch[MAX_PATH + 1];
	const char *initial = pick_extract_initial(vm, 1,
	    scratch, sizeof(scratch));
	const char *mask    = NULL;
	if (wrenGetSlotType(vm, 2) == WREN_TYPE_STRING)
		mask = wrenGetSlotString(vm, 2);

	struct file_pick fp = { 0 };
	int rc = uifcfilepick("Save as", &fp, initial, mask,
	    UIFC_FP_ALLOWENTRY | UIFC_FP_OVERPROMPT);
	if (rc <= 0 || fp.files < 1 || fp.selected == NULL ||
	    fp.selected[0] == NULL) {
		filepick_free(&fp);
		wrenSetSlotNull(vm, 0);
		return;
	}
	enum wren_file_write_consent mode =
	    fexist(fp.selected[0]) ? WFC_OVERWRITE : WFC_CREATE;
	if (push_picker_save_file(vm, 0, fp.selected[0], mode) == NULL)
		wrenSetSlotNull(vm, 0);
	filepick_free(&fp);
}

/* Host.pickFiles(initialDir, mask, opts) — multi-select counterpart
 * of pickFile, wrapping uifc/filepick's filepick_multi.  Same
 * argument shape as pickFile (Directory or String for initialDir).
 * Returns a non-empty List<File> on OK (each File has a .token),
 * or null on cancel / empty selection / OOM.  Per filepick.h,
 * UIFC_FP_ALLOWENTRY / OVERPROMPT / CREATPROMPT cannot be combined
 * with multi-select — filepick_multi rejects them with -1, which
 * we surface as null. */
void
fn_Host_pickFiles(WrenVM *vm)
{
	char        scratch[MAX_PATH + 1];
	const char *initial = pick_extract_initial(vm, 1,
	    scratch, sizeof(scratch));
	const char *mask    = NULL;
	int         opts    = 0;
	if (wrenGetSlotType(vm, 2) == WREN_TYPE_STRING)
		mask = wrenGetSlotString(vm, 2);
	if (wrenGetSlotType(vm, 3) == WREN_TYPE_NUM)
		opts = (int)wrenGetSlotDouble(vm, 3);

	struct file_pick fp = { 0 };
	int rc = uifcfilepick_multi("Tag files to upload", &fp, initial,
	    mask, opts);
	if (rc <= 0 || fp.files < 1 || fp.selected == NULL) {
		filepick_free(&fp);
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenEnsureSlots(vm, 3);
	wrenSetSlotNewList(vm, 0);
	int produced = 0;
	for (int i = 0; i < fp.files; i++) {
		if (push_picker_file(vm, 1, fp.selected[i]) == NULL)
			continue;
		wrenInsertInList(vm, 0, -1, 1);
		produced++;
	}
	filepick_free(&fp);
	if (produced == 0)
		wrenSetSlotNull(vm, 0);
}

/* Host.openLocalFile(token) — verify a consent token previously
 * minted by pickFile / pickFiles and re-open the encoded path as
 * a fresh File foreign.  Returns null on:
 *   - bad token (HMAC mismatch, malformed, signing key rotated)
 *   - file no longer exists (verify recomputes file SHA-1)
 *   - file content changed since consent (SHA-1 mismatch)
 *   - OOM / path too long
 * This is the only path by which Wren can resurrect a non-sandbox
 * File handle across sessions; the token is the consent signal. */
void
fn_Host_openLocalFile(WrenVM *vm)
{
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_STRING) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	const char *token = wrenGetSlotString(vm, 1);
	char *path = wren_token_verify(token);
	if (path == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	if (push_picker_file(vm, 0, path) == NULL)
		wrenSetSlotNull(vm, 0);
	free(path);
}

/* ----- File foreign methods ---------------------------------------- */

static struct wren_file *
file_check_open(WrenVM *vm, unsigned rights)
{
	struct wren_file *wf = file_check(vm);
	if (wf == NULL)
		return NULL;
	if (wf->fp == NULL) {
		wren_throw(vm, "File: not open");
		return NULL;
	}
	if ((wf->rights & rights) != rights) {
		wren_throw(vm, (rights & WFR_WRITE) != 0
		    ? "File: handle does not grant write access"
		    : "File: handle does not grant read access");
		return NULL;
	}
	return wf;
}

static long
file_size_now(struct wren_file *wf)
{
	long pos = ftell(wf->fp);
	fseek(wf->fp, 0, SEEK_END);
	long sz = ftell(wf->fp);
	fseek(wf->fp, pos, SEEK_SET);
	return sz;
}

void
fn_File_open(WrenVM *vm)
{
	struct wren_file *wf = file_check(vm);
	if (wf == NULL)
		return;
	if (wf->fp != NULL) {
		wren_throw(vm, "File: already open");
		return;
	}
	const char *mode;
	switch (wf->write_consent) {
		case WFC_CREATE:
			/* Race-safe single create — fopen(_, "wbx") fails if
			 * the path now exists, even if it didn't at pick time.
			 * Prevents an attacker from substituting a different
			 * file between pick and open. */
			mode = "wbx";
			break;
		case WFC_OVERWRITE:
			/* User confirmed overwrite at pick time.  "wb" is
			 * truncate-or-create; consent is one-shot and the
			 * race window between pick and open is the user's
			 * accepted trade-off for the overwrite UX. */
			mode = "wb";
			break;
		case WFC_NONE:
		default:
			mode = (wf->rights & WFR_WRITE) != 0 ? "r+b" : "rb";
			break;
	}
	wf->fp = fopen(wf->path, mode);
	if (wf->fp == NULL) {
		file_build_error(vm, 0, FILE_ERR_OPEN_FAILED, errno,
		    "fopen failed");
		return;
	}
	if (wf->write_consent != WFC_NONE)
		wf->consent_used = true;
	fseek(wf->fp, 0, SEEK_SET);
}

void
fn_File_close(WrenVM *vm)
{
	struct wren_file *wf = file_check(vm);
	if (wf == NULL)
		return;
	if (wf->fp == NULL) {
		wren_throw(vm, "File: not open");
		return;
	}
	fclose(wf->fp);
	wf->fp = NULL;
	/* While the file was open, file_check skipped the existence
	 * check (its fd was authoritative).  Now that we've closed and
	 * the fd is gone, re-check: if the path was unlinked while we
	 * were holding the fd, the handle becomes dead.  No throw on
	 * close itself; the next op trips file_check's dead branch. */
	if (!fexist(wf->path))
		fs_kill_file(wf);
}

/* Read `count` bytes from `off`.  If `advance`, file offset becomes
 * off+got afterward; otherwise it's left at off+got too via fseek
 * (offset variant intentionally repositions for simplicity — pread
 * semantics could be added later if needed). */
static void
do_read_at(WrenVM *vm, struct wren_file *wf, long off, long count,
    bool advance)
{
	(void)advance;
	if (off < 0 || count < 0) {
		wren_throw(vm, "File: negative offset or count");
		return;
	}
	long sz = file_size_now(wf);
	if (off > sz) {
		wren_throw(vm, "File: offset past end");
		return;
	}
	if (off + count > sz)
		count = sz - off;
	fseek(wf->fp, off, SEEK_SET);
	if (count == 0) {
		wrenSetSlotBytes(vm, 0, "", 0);
		return;
	}
	char *buf = malloc((size_t)count);
	if (buf == NULL) {
		file_build_error(vm, 0, FILE_ERR_OOM, ENOMEM,
		    "malloc for read buffer");
		return;
	}
	size_t got = fread(buf, 1, (size_t)count, wf->fp);
	wrenSetSlotBytes(vm, 0, buf, got);
	free(buf);
}

void
fn_File_readBytes_1(WrenVM *vm)
{
	struct wren_file *wf = file_check_open(vm, WFR_READ);
	if (wf == NULL)
		return;
	long count = (long)wrenGetSlotDouble(vm, 1);
	long off   = ftell(wf->fp);
	do_read_at(vm, wf, off, count, true);
}

void
fn_File_readBytes_2(WrenVM *vm)
{
	struct wren_file *wf = file_check_open(vm, WFR_READ);
	if (wf == NULL)
		return;
	long count = (long)wrenGetSlotDouble(vm, 1);
	long off   = (long)wrenGetSlotDouble(vm, 2);
	do_read_at(vm, wf, off, count, false);
}

void
fn_File_read(WrenVM *vm)
{
	struct wren_file *wf = file_check_open(vm, WFR_READ);
	if (wf == NULL)
		return;
	long off = ftell(wf->fp);
	long sz  = file_size_now(wf);
	if (off > sz) {
		wren_throw(vm, "File: offset past end");
		return;
	}
	do_read_at(vm, wf, off, sz - off, true);
}

static void
do_write_at(WrenVM *vm, struct wren_file *wf, long off,
    const char *bytes, int len)
{
	if (off < 0 || len < 0) {
		wren_throw(vm, "File: negative offset or length");
		return;
	}
	long sz = file_size_now(wf);
	if (off > sz) {
		wren_throw(vm, "File: offset past end");
		return;
	}
	fseek(wf->fp, off, SEEK_SET);
	if (len > 0) {
		size_t put = fwrite(bytes, 1, (size_t)len, wf->fp);
		if ((int)put != len) {
			file_build_error(vm, 0, FILE_ERR_WRITE_FAILED,
			    errno, "fwrite short");
			return;
		}
	}
	fflush(wf->fp);
	fseek(wf->fp, off + len, SEEK_SET);
}

void
fn_File_writeBytes_1(WrenVM *vm)
{
	struct wren_file *wf = file_check_open(vm, WFR_WRITE);
	if (wf == NULL)
		return;
	int len = 0;
	const char *bytes = wrenGetSlotBytes(vm, 1, &len);
	long off = ftell(wf->fp);
	do_write_at(vm, wf, off, bytes, len);
}

void
fn_File_writeBytes_2(WrenVM *vm)
{
	struct wren_file *wf = file_check_open(vm, WFR_WRITE);
	if (wf == NULL)
		return;
	int len = 0;
	const char *bytes = wrenGetSlotBytes(vm, 1, &len);
	long off = (long)wrenGetSlotDouble(vm, 2);
	do_write_at(vm, wf, off, bytes, len);
}

/* readLine() — read from the current offset to either the first LF
 * (0x0A) or EOF, return the bytes read with any trailing LF removed.
 * The file offset advances past the LF on a hit, or to EOF if no LF
 * was found in the remainder.  Returns null when the offset is
 * already at EOF (so a `while (line != null) ...` loop terminates
 * cleanly).  A blank line (just LF) returns an empty string. */
void
fn_File_readLine(WrenVM *vm)
{
	struct wren_file *wf = file_check_open(vm, WFR_READ);
	if (wf == NULL)
		return;
	long off = ftell(wf->fp);
	long sz  = file_size_now(wf);
	if (off >= sz) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	fseek(wf->fp, off, SEEK_SET);

	/* Chunked read so a 100 GB file with short lines doesn't allocate
	 * the whole remainder up front.  Grows geometrically when a line
	 * happens to be longer than the chunk. */
	char   chunk[512];
	char  *line     = NULL;
	size_t line_len = 0;
	size_t line_cap = 0;
	bool   eol      = false;
	long   advance  = 0;
	while (off + advance < sz && !eol) {
		long want = sz - off - advance;
		if (want > (long)sizeof(chunk))
			want = sizeof(chunk);
		size_t got = fread(chunk, 1, (size_t)want, wf->fp);
		if (got == 0)
			break;
		size_t take = got;
		char  *lf   = memchr(chunk, '\n', got);
		if (lf != NULL) {
			take = (size_t)(lf - chunk);
			eol  = true;
		}
		if (line_len + take > line_cap) {
			size_t new_cap = line_cap == 0 ? 256 : line_cap * 2;
			while (new_cap < line_len + take)
				new_cap *= 2;
			char *nb = realloc(line, new_cap);
			if (nb == NULL) {
				free(line);
				file_build_error(vm, 0, FILE_ERR_OOM,
				    ENOMEM,
				    "realloc for readLine buffer");
				return;
			}
			line     = nb;
			line_cap = new_cap;
		}
		memcpy(line + line_len, chunk, take);
		line_len += take;
		advance  += eol ? (long)(take + 1) : (long)got;
	}
	fseek(wf->fp, off + advance, SEEK_SET);
	wrenSetSlotBytes(vm, 0, line != NULL ? line : "", line_len);
	free(line);
}

/* writeLine(s) — write the bytes of `s` at the current offset, then
 * append an LF.  The offset advances past the LF.  No trailing-LF
 * check on `s` itself — if the script wants raw control over
 * line termination, it should use writeBytes() instead. */
void
fn_File_writeLine(WrenVM *vm)
{
	struct wren_file *wf = file_check_open(vm, WFR_WRITE);
	if (wf == NULL)
		return;
	int len = 0;
	const char *bytes = wrenGetSlotBytes(vm, 1, &len);
	if (len < 0)
		len = 0;
	long off = ftell(wf->fp);
	long sz  = file_size_now(wf);
	if (off > sz) {
		wren_throw(vm, "File: offset past end");
		return;
	}
	fseek(wf->fp, off, SEEK_SET);
	if (len > 0) {
		if (fwrite(bytes, 1, (size_t)len, wf->fp) != (size_t)len) {
			file_build_error(vm, 0, FILE_ERR_WRITE_FAILED,
			    errno, "writeLine fwrite short");
			return;
		}
	}
	if (fwrite("\n", 1, 1, wf->fp) != 1) {
		file_build_error(vm, 0, FILE_ERR_WRITE_FAILED, errno,
		    "writeLine LF fwrite short");
		return;
	}
	fflush(wf->fp);
	fseek(wf->fp, off + len + 1, SEEK_SET);
}

/* write(s) — truncate then rewrite from offset 0; offset ends at len. */
void
fn_File_write(WrenVM *vm)
{
	struct wren_file *wf = file_check_open(vm, WFR_WRITE);
	if (wf == NULL)
		return;
	int len = 0;
	const char *bytes = wrenGetSlotBytes(vm, 1, &len);
	fclose(wf->fp);
	wf->fp = fopen(wf->path, "w+b");
	if (wf->fp == NULL) {
		file_build_error(vm, 0, FILE_ERR_OPEN_FAILED, errno,
		    "fopen for truncate-rewrite");
		return;
	}
	if (len > 0) {
		size_t put = fwrite(bytes, 1, (size_t)len, wf->fp);
		if ((int)put != len) {
			file_build_error(vm, 0, FILE_ERR_WRITE_FAILED,
			    errno, "write fwrite short");
			return;
		}
	}
	fflush(wf->fp);
}

void
fn_File_offset_get(WrenVM *vm)
{
	struct wren_file *wf = file_check_open(vm, 0);
	if (wf == NULL)
		return;
	wrenSetSlotDouble(vm, 0, (double)ftell(wf->fp));
}

void
fn_File_offset_set(WrenVM *vm)
{
	struct wren_file *wf = file_check_open(vm, 0);
	if (wf == NULL)
		return;
	long off = (long)wrenGetSlotDouble(vm, 1);
	if (off < 0) {
		wren_throw(vm, "File: negative offset");
		return;
	}
	long sz = file_size_now(wf);
	if (off > sz) {
		wren_throw(vm, "File: offset past end");
		return;
	}
	fseek(wf->fp, off, SEEK_SET);
}

void
fn_File_size(WrenVM *vm)
{
	struct wren_file *wf = file_check(vm);
	if (wf == NULL)
		return;
	if (wf->fp != NULL) {
		wrenSetSlotDouble(vm, 0, (double)file_size_now(wf));
		return;
	}
	off_t sz = flength(wf->path);
	if (sz < 0) {
		file_build_error(vm, 0, FILE_ERR_STAT_FAILED, errno,
		    "flength failed");
		return;
	}
	wrenSetSlotDouble(vm, 0, (double)sz);
}

/* File.mtime — modification time as POSIX seconds.  Returns 0 when
 * the file can't be stat'd (e.g. the path was unlinked while we
 * held a non-open handle).  Uses fdate by path for both open and
 * closed files; the path is preserved on wren_file. */
void
fn_File_mtime(WrenVM *vm)
{
	struct wren_file *wf = file_check(vm);
	if (wf == NULL)
		return;
	time_t t = fdate(wf->path);
	if (t < 0)
		t = 0;
	wrenSetSlotDouble(vm, 0, (double)t);
}

/* File.mtime=(t) — set modification time (POSIX seconds).  Works on
 * both open and closed files (setfdate uses utime by path).  Used by
 * the SFTP queue to stamp completed downloads with the remote's
 * mtime so the browser's status chip can match local-vs-remote
 * without needing server-side hash extensions. */
void
fn_File_mtime_set(WrenVM *vm)
{
	struct wren_file *wf = file_check(vm);
	if (wf == NULL)
		return;
	if ((wf->rights & WFR_WRITE) == 0) {
		wren_throw(vm, "File: handle does not grant write access");
		return;
	}
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_NUM) {
		wren_throw(vm, "File.mtime=: argument must be a Num");
		return;
	}
	double v = wrenGetSlotDouble(vm, 1);
	if (v < 0)
		v = 0;
	(void)setfdate(wf->path, (time_t)v);
}

void
fn_File_isOpen(WrenVM *vm)
{
	struct wren_file *wf = file_check(vm);
	if (wf == NULL)
		return;
	wrenSetSlotBool(vm, 0, wf->fp != NULL);
}

/* File.token — opaque consent token for picker-sourced Files; null
 * for Files obtained via Cache / Download Directory listings.  See
 * wren_token.h for the model.  Persist this in WON next to the path
 * string; pass back to Host.openLocalFile on resume. */
void
fn_File_token(WrenVM *vm)
{
	struct wren_file *wf = file_check(vm);
	if (wf == NULL)
		return;
	if (wf->token == NULL)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotString(vm, 0, wf->token);
}

/* Map a file's contents for hashing.  On success the mapping (if
 * any) is returned via *map_out and the byte range via *data_out /
 * *len_out; if the file is zero-length the mapping is NULL and the
 * data is an empty buffer (xpmap typically rejects 0-sized maps so
 * we synthesize one).  Returns false on stat / mmap failure with a
 * FileError already in slot 0 — caller just returns. */
static bool
file_map_for_hash(WrenVM *vm, struct wren_file *wf,
                  struct xpmapping **map_out, const void **data_out,
                  size_t *len_out)
{
	off_t sz = flength(wf->path);
	if (sz < 0) {
		file_build_error(vm, 0, FILE_ERR_STAT_FAILED, errno,
		    "flength failed before hash");
		return false;
	}
	if (sz == 0) {
		*map_out  = NULL;
		*data_out = "";
		*len_out  = 0;
		return true;
	}
	struct xpmapping *map = xpmap(wf->path, XPMAP_READ);
	if (map == NULL) {
		file_build_error(vm, 0, FILE_ERR_MMAP_FAILED, errno,
		    "xpmap failed");
		return false;
	}
	*map_out  = map;
	*data_out = map->addr;
	*len_out  = map->size;
	return true;
}

/* sha1 / md5 — return raw digest bytes (Wren strings are byte-safe).
 * Hex form left to scripts (cheap to format).  Useful primarily for
 * comparing against SFTPEntry.hash, which the sha1s/md5s extension
 * also returns as raw bytes. */
void
fn_File_sha1(WrenVM *vm)
{
	struct wren_file *wf = file_check(vm);
	if (wf == NULL)
		return;
	if ((wf->rights & WFR_READ) == 0) {
		wren_throw(vm, "File: handle does not grant read access");
		return;
	}
	struct xpmapping *map;
	const void *data;
	size_t len;
	if (!file_map_for_hash(vm, wf, &map, &data, &len))
		return;
	uint8_t digest[SHA1_DIGEST_SIZE];
	SHA1_calc(digest, data, len);
	if (map != NULL)
		xpunmap(map);
	wrenSetSlotBytes(vm, 0, (const char *)digest, SHA1_DIGEST_SIZE);
}

void
fn_File_md5(WrenVM *vm)
{
	struct wren_file *wf = file_check(vm);
	if (wf == NULL)
		return;
	if ((wf->rights & WFR_READ) == 0) {
		wren_throw(vm, "File: handle does not grant read access");
		return;
	}
	struct xpmapping *map;
	const void *data;
	size_t len;
	if (!file_map_for_hash(vm, wf, &map, &data, &len))
		return;
	uint8_t digest[MD5_DIGEST_SIZE];
	MD5_calc(digest, data, len);
	if (map != NULL)
		xpunmap(map);
	wrenSetSlotBytes(vm, 0, (const char *)digest, MD5_DIGEST_SIZE);
}

/* toString skips the usual file_check / dir_check guards so a dead
 * handle prints "(dead)" instead of aborting the fiber. */
void
fn_File_toString(WrenVM *vm)
{
	struct wren_file *wf = wrenGetSlotForeign(vm, 0);
	const char *suffix = wf->dead ? " (dead)" :
	                     (wf->fp != NULL ? " (open)" : "");
	char buf[MAX_PATH + 16];
	snprintf(buf, sizeof(buf), "%s%s", wf->path, suffix);
	wrenSetSlotString(vm, 0, buf);
}

void
fn_Directory_toString(WrenVM *vm)
{
	struct wren_directory *wd = wrenGetSlotForeign(vm, 0);
	const char *base = wd->is_cache ? "Cache/" : wd->path;
	if (wd->dead) {
		char buf[MAX_PATH + 16];
		snprintf(buf, sizeof(buf), "%s (dead)", base);
		wrenSetSlotString(vm, 0, buf);
	}
	else {
		wrenSetSlotString(vm, 0, base);
	}
}
