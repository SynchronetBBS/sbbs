/* Directory / File / Host foreign surface for the Wren scripting host.
 * Public API is declared in wren_bind_fs.h; the BINDINGS table +
 * lookup_class dispatch live in wren_bind.c.  See wren_bind_internal.h
 * for the shared SWF_* type tags and helpers. */

#include "wren_bind_internal.h"
#include "wren_bind_fs.h"
#include "wren_host_internal.h"
#include "syncterm.h"
#include "term.h"        /* get_cache_fn_base */

#include "sha1.h"
#include "md5.h"
#include "xpmap.h"

#include <dirwrap.h>     /* opendir / readdir / MKDIR */

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

struct wren_file {
	enum syncterm_wren_foreign type;
	char  path[MAX_PATH + 1]; /* absolute path */
	FILE *fp;
	/* Same dead/list mechanics as wren_directory.  A successful
	 * Directory.delete on the file (or on an ancestor directory)
	 * marks it dead; further ops abort the fiber. */
	bool  dead;
	struct wren_file *fs_prev;
	struct wren_file *fs_next;
};


/* Filename policy: 1..64 chars, only [A-Za-z0-9._-], no leading '.'
 * or '-', no trailing '.', no ".." substring, basename (chars before
 * first '.') not a Windows reserved device. */
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
	static const char *reserved[] = {
		"CON", "PRN", "AUX", "NUL",
		"COM1", "COM2", "COM3", "COM4", "COM5",
		"COM6", "COM7", "COM8", "COM9",
		"LPT1", "LPT2", "LPT3", "LPT4", "LPT5",
		"LPT6", "LPT7", "LPT8", "LPT9",
		NULL
	};
	const char *dot = strchr(name, '.');
	size_t base_len = dot ? (size_t)(dot - name) : n;
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
			return false;
	}
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
		wren_throw(vm, "Cache: failed to resolve path");
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
	/* Open files survive their backing path's removal — on Unix the
	 * fd stays valid after unlink, and on Windows you can't delete an
	 * open file at all.  Skip the existence check; let the OS return
	 * I/O errors through subsequent reads/writes if any.  The recheck
	 * happens in fn_File_close. */
	if (wf->fp != NULL)
		return wf;
	if (!fexist(wf->path)) {
		fs_kill_file(wf);
		wren_throw(vm, "File: backing file no longer exists");
		return NULL;
	}
	return wf;
}

static struct wren_directory *
dir_check(WrenVM *vm)
{
	struct wren_directory *wd = wrenGetSlotForeign(vm, 0);
	if (wd->dead) {
		wren_throw(vm, "Directory: handle is dead "
		    "(an ancestor's Directory.delete invalidated it)");
		return NULL;
	}
	/* Cache directories resolve their path lazily on every call (see
	 * wd_resolve), so we can't fexist-check them here.  wd_resolve does
	 * its own existence check after resolving the BBS-relative path. */
	if (!wd->is_cache && !isdir(wd->path)) {
		fs_kill_dir(wd);
		wren_throw(vm, "Directory: backing directory no longer exists");
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
	wf->type    = SWF_FILE;
	wf->fp      = NULL;
	wf->dead    = false;
	wf->fs_prev = NULL;
	wf->fs_next = NULL;
	snprintf(wf->path, sizeof(wf->path), "%s%s", dir, name);
	fs_register_file(wf);
	return wf;
}

static void
dir_contains_impl(WrenVM *vm, const char *dir, const char *name)
{
	if (!fname_is_clean(name)) {
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
dir_list_impl(WrenVM *vm, const char *dir)
{
	wrenEnsureSlots(vm, 4);
	wrenSetSlotNewMap(vm, 0);
	DIR *dh = opendir(dir);
	if (dh == NULL)
		return;
	struct dirent *de;
	char p[MAX_PATH + 1];
	while ((de = readdir(dh)) != NULL) {
		if (!fname_is_clean(de->d_name))
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
			wd->type     = SWF_DIRECTORY;
			wd->is_cache = false;
			wd->dead     = false;
			wd->fs_prev  = NULL;
			wd->fs_next  = NULL;
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
			wf->type    = SWF_FILE;
			wf->fp      = NULL;
			wf->dead    = false;
			wf->fs_prev = NULL;
			wf->fs_next = NULL;
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
dir_create_impl(WrenVM *vm, const char *dir, const char *name)
{
	wrenEnsureSlots(vm, 1);
	if (!fname_is_clean(name)) {
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
dir_createdir_impl(WrenVM *vm, const char *dir, const char *name)
{
	wrenEnsureSlots(vm, 2);
	if (!fname_is_clean(name)) {
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
	wd->type     = SWF_DIRECTORY;
	wd->is_cache = false;
	wd->dead     = false;
	wd->fs_prev  = NULL;
	wd->fs_next  = NULL;
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
	dir_contains_impl(vm, dir, name);
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
	dir_list_impl(vm, dir);
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
	dir_create_impl(vm, dir, name);
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
	dir_createdir_impl(vm, dir, name);
}

static void
dir_delete_impl(WrenVM *vm, const char *dir, const char *name)
{
	wrenEnsureSlots(vm, 1);
	if (!fname_is_clean(name)) {
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
	 * is the directory test.  fname_is_clean already rejects path
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
	dir_delete_impl(vm, dir, name);
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
	wd->type     = SWF_DIRECTORY;
	wd->is_cache = true;
	wd->dead     = false;
	wd->fs_prev  = NULL;
	wd->fs_next  = NULL;
	wd->path[0]  = '\0';
	fs_register_dir(wd);
}

/* ----- File foreign methods ---------------------------------------- */

static struct wren_file *
file_check_open(WrenVM *vm)
{
	struct wren_file *wf = file_check(vm);
	if (wf == NULL)
		return NULL;
	if (wf->fp == NULL) {
		wren_throw(vm, "File: not open");
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
	wf->fp = fopen(wf->path, "r+b");
	if (wf->fp == NULL) {
		wren_throw(vm, "File: open failed");
		return;
	}
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
		wren_throw(vm, "File: out of memory");
		return;
	}
	size_t got = fread(buf, 1, (size_t)count, wf->fp);
	wrenSetSlotBytes(vm, 0, buf, got);
	free(buf);
}

void
fn_File_readBytes_1(WrenVM *vm)
{
	struct wren_file *wf = file_check_open(vm);
	if (wf == NULL)
		return;
	long count = (long)wrenGetSlotDouble(vm, 1);
	long off   = ftell(wf->fp);
	do_read_at(vm, wf, off, count, true);
}

void
fn_File_readBytes_2(WrenVM *vm)
{
	struct wren_file *wf = file_check_open(vm);
	if (wf == NULL)
		return;
	long count = (long)wrenGetSlotDouble(vm, 1);
	long off   = (long)wrenGetSlotDouble(vm, 2);
	do_read_at(vm, wf, off, count, false);
}

void
fn_File_read(WrenVM *vm)
{
	struct wren_file *wf = file_check_open(vm);
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
			wren_throw(vm, "File: write failed");
			return;
		}
	}
	fflush(wf->fp);
	fseek(wf->fp, off + len, SEEK_SET);
}

void
fn_File_writeBytes_1(WrenVM *vm)
{
	struct wren_file *wf = file_check_open(vm);
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
	struct wren_file *wf = file_check_open(vm);
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
	struct wren_file *wf = file_check_open(vm);
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
				wren_throw(vm, "File: out of memory");
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
	struct wren_file *wf = file_check_open(vm);
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
			wren_throw(vm, "File: write failed");
			return;
		}
	}
	if (fwrite("\n", 1, 1, wf->fp) != 1) {
		wren_throw(vm, "File: write failed");
		return;
	}
	fflush(wf->fp);
	fseek(wf->fp, off + len + 1, SEEK_SET);
}

/* write(s) — truncate then rewrite from offset 0; offset ends at len. */
void
fn_File_write(WrenVM *vm)
{
	struct wren_file *wf = file_check_open(vm);
	if (wf == NULL)
		return;
	int len = 0;
	const char *bytes = wrenGetSlotBytes(vm, 1, &len);
	fclose(wf->fp);
	wf->fp = fopen(wf->path, "w+b");
	if (wf->fp == NULL) {
		wren_throw(vm, "File: write failed (reopen)");
		return;
	}
	if (len > 0) {
		size_t put = fwrite(bytes, 1, (size_t)len, wf->fp);
		if ((int)put != len) {
			wren_throw(vm, "File: write failed");
			return;
		}
	}
	fflush(wf->fp);
}

void
fn_File_offset_get(WrenVM *vm)
{
	struct wren_file *wf = file_check_open(vm);
	if (wf == NULL)
		return;
	wrenSetSlotDouble(vm, 0, (double)ftell(wf->fp));
}

void
fn_File_offset_set(WrenVM *vm)
{
	struct wren_file *wf = file_check_open(vm);
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
		wren_throw(vm, "File: flength failed");
		return;
	}
	wrenSetSlotDouble(vm, 0, (double)sz);
}

void
fn_File_isOpen(WrenVM *vm)
{
	struct wren_file *wf = file_check(vm);
	if (wf == NULL)
		return;
	wrenSetSlotBool(vm, 0, wf->fp != NULL);
}

/* Map a file's contents for hashing.  On success the mapping (if
 * any) is returned via *map_out and the byte range via *data_out /
 * *len_out; if the file is zero-length the mapping is NULL and the
 * data is an empty buffer (xpmap typically rejects 0-sized maps so
 * we synthesize one).  Returns false on stat / mmap failure with a
 * fiber abort already raised. */
static bool
file_map_for_hash(WrenVM *vm, struct wren_file *wf,
                  struct xpmapping **map_out, const void **data_out,
                  size_t *len_out)
{
	off_t sz = flength(wf->path);
	if (sz < 0) {
		wren_throw(vm, "File: flength failed");
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
		wren_throw(vm, "File: mmap failed");
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
