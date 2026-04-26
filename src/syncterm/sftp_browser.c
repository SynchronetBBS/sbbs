/*
 * SFTP directory browser — modal screen invoked from the terminal via
 * Alt-S.  Phase 3: read-only list of remote files in the current
 * directory, with a status column showing how each remote file
 * compares to its local counterpart in the session's download dir.
 *
 * Rendering uses uifc.list() rather than hand-rolled vmem so the
 * standard SyncTERM look-and-feel (scrollbar, highlight, etc.) is free.
 * Each directory change exits uifc.list, re-builds the entry list for
 * the new cwd, and re-enters.
 */

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <datewrap.h>
#include <genwrap.h>
#include <md5.h>
#include <sha1.h>
#include <uifc.h>

#include "bbslist.h"
#include "ciolib.h"
#include "sftp.h"
#include "sftp_browser.h"
#include "sftp_queue.h"
#include "sftp_session.h"
#include "uifcinit.h"

/* cp437_savescrn is defined in term.c but not in any public header. */
struct ciolib_screen *cp437_savescrn(void);

#define STATUS_COL_W 4   /* width of the [==] / [<>] / [  ] marker */

/* ---- Path helpers ------------------------------------------------------ */

static char *
path_join(const char *base, const char *name)
{
	size_t blen = strlen(base);
	size_t nlen = strlen(name);
	int need_sep = (blen > 0 && base[blen - 1] != '/');
	char *out = (char *)malloc(blen + nlen + need_sep + 1);
	if (out == NULL)
		return NULL;
	memcpy(out, base, blen);
	if (need_sep)
		out[blen++] = '/';
	memcpy(out + blen, name, nlen);
	out[blen + nlen] = '\0';
	return out;
}

static char *
path_parent(const char *path)
{
	if (path == NULL || path[0] == '\0')
		return strdup("/");
	size_t len = strlen(path);
	while (len > 1 && path[len - 1] == '/')
		len--;
	while (len > 0 && path[len - 1] != '/')
		len--;
	if (len == 0)
		return strdup("/");
	if (len > 1)
		len--;           /* drop the trailing slash except on "/" */
	char *out = (char *)malloc(len + 1);
	if (out == NULL)
		return NULL;
	memcpy(out, path, len);
	out[len] = '\0';
	if (out[0] == '\0') {
		free(out);
		return strdup("/");
	}
	return out;
}

/* ---- Formatting helpers ------------------------------------------------ */

static void
format_size(uint64_t sz, bool is_dir, char *buf, size_t bufsz)
{
	if (is_dir) {
		snprintf(buf, bufsz, "%10s", "<DIR>");
		return;
	}
	char num[32];
	byte_estimate_to_str(sz, num, sizeof(num), 0, 1);
	/* byte_estimate_to_str emits "<digits>" for raw bytes or
	 * "<float><K|M|G|T|P>" for binary multiples.  Append "B" or "iB"
	 * so the unit is unambiguous, then right-align the combined
	 * string so the trailing letter lines up across rows. */
	size_t n = strlen(num);
	const char *suffix = "B";
	if (n > 0) {
		char last = num[n - 1];
		if (last == 'K' || last == 'M' || last == 'G' || last == 'T' || last == 'P')
			suffix = "iB";
	}
	char combined[48];
	snprintf(combined, sizeof(combined), "%s%s", num, suffix);
	snprintf(buf, bufsz, "%10s", combined);
}

static void
format_mtime(uint32_t mtime, char *buf, size_t bufsz)
{
	if (mtime == 0) {
		snprintf(buf, bufsz, "%10s", "");
		return;
	}
	time_t t = (time_t)mtime;
	struct tm tm;
	localtime_r(&t, &tm);
	strftime(buf, bufsz, "%Y-%m-%d", &tm);
}

/*
 * 4-character status tag shown to the left of each filename.
 *
 *   Queue-aware (overrides local compare when the file has a job):
 *     [\x18\x18]  upload active      [Q\x18]  upload queued
 *     [\x19\x19]  download active    [Q\x19]  download queued
 *     [er]        last attempt failed
 *     [cx]        last attempt cancelled
 *
 *   Local compare (no job in queue):
 *     [==]        local dldir copy matches size + mtime
 *     [<>]        local dldir copy exists but differs
 *     (blank)     no local counterpart
 */
struct entry;   /* forward */
static void rebuild_status_chip(const struct entry *e,
                                const char *dldir, char *out4);

/* ---- Entry list builder ----------------------------------------------- */

/*
 * load_dir populates two parallel arrays: `names[]` (the raw filenames
 * for use on Enter) and `lines[]` (the formatted-for-uifc-list display
 * strings).  Both are NULL-terminated; free with free_dir below.
 *
 * The first entry is always ".." (except when we're at "/").  The rest
 * are the directory's contents, sorted dirs-first then alphabetical.
 */

struct entry {
	char    *name;     /* filename or ".." */
	char    *line;     /* formatted display line */
	char    *lname;    /* long name from lname@syncterm.net ext attr, or NULL */
	char    *rpath;    /* full remote path for queue lookups */
	uint64_t size;     /* remote file size (for local-compare) */
	uint32_t mtime;    /* remote mtime (for local-compare) */
	uint8_t  hash[SHA1_DIGEST_SIZE];  /* sha1s/md5s extension hash, if any */
	uint8_t  hash_len; /* 20=sha1, 16=md5, 0=none */
	bool     is_dir;
	bool     is_parent; /* the ".." entry */
};

/* Byte offset in entry->line where the 4-char status chip starts.  Kept
 * in sync with load_dir's snprintf format: szbuf(10) + ' ' + datebuf(10)
 * + ' ' = 22. */
#define STATUS_COL_OFF 22

/*
 * Stream-hash the local file and write `hash_len` bytes into `out`.
 * Returns true on success.  Called only when the remote file exposed a
 * sha1s / md5s extension AND the file's size already matches, so the
 * caller has a reasonable prior that the hashes will match too.
 */
static bool
hash_local_file(const char *path, uint8_t *out, uint8_t hash_len)
{
	FILE *f = fopen(path, "rb");
	if (f == NULL)
		return false;
	uint8_t buf[16384];
	bool ok = false;
	if (hash_len == SHA1_DIGEST_SIZE) {
		SHA1_CTX ctx;
		SHA1Init(&ctx);
		size_t n;
		while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
			SHA1Update(&ctx, buf, n);
		ok = !ferror(f);
		SHA1Final(&ctx, out);
	}
	else if (hash_len == MD5_DIGEST_SIZE) {
		MD5 ctx;
		MD5_open(&ctx);
		size_t n;
		while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
			MD5_digest(&ctx, buf, n);
		ok = !ferror(f);
		MD5_close(&ctx, out);
	}
	fclose(f);
	return ok;
}

static void
rebuild_status_chip(const struct entry *e, const char *dldir, char *out4)
{
	memcpy(out4, "    ", 4);
	if (e->is_dir || e->is_parent || dldir == NULL || dldir[0] == '\0')
		return;
	int sdn = (e->rpath != NULL) ? sftp_queue_status(SFTP_JOB_DN, e->rpath) : -1;
	int sup = (e->rpath != NULL) ? sftp_queue_status(SFTP_JOB_UP, e->rpath) : -1;
	if (sdn == SFTP_JOB_ACTIVE) {
		memcpy(out4, "[\x19\x19]", 4);
		return;
	}
	if (sup == SFTP_JOB_ACTIVE) {
		memcpy(out4, "[\x18\x18]", 4);
		return;
	}
	if (sdn == SFTP_JOB_QUEUED) {
		memcpy(out4, "[Q\x19]", 4);
		return;
	}
	if (sup == SFTP_JOB_QUEUED) {
		memcpy(out4, "[Q\x18]", 4);
		return;
	}
	if (sdn == SFTP_JOB_FAILED || sup == SFTP_JOB_FAILED) {
		memcpy(out4, "[er]", 4);
		return;
	}
	if (sdn == SFTP_JOB_CANCELLED || sup == SFTP_JOB_CANCELLED) {
		memcpy(out4, "[cx]", 4);
		return;
	}
	char *lpath = path_join(dldir, e->name);
	if (lpath == NULL)
		return;
	struct stat lst;
	int rc = stat(lpath, &lst);
	if (rc != 0) {
		free(lpath);
		return;
	}
	/* Size is the cheap first filter: if bytes differ, nothing else
	 * can make them equal. */
	if ((uint64_t)lst.st_size != e->size) {
		free(lpath);
		memcpy(out4, "[<>]", 4);
		return;
	}
	/* Size matches.  If the server gave us a cryptographic hash,
	 * that's authoritative — compute the local hash and compare.
	 * Otherwise fall back to mtime. */
	if (e->hash_len != 0) {
		uint8_t local_hash[SHA1_DIGEST_SIZE];
		bool ok = hash_local_file(lpath, local_hash, e->hash_len);
		free(lpath);
		if (!ok)
			return;
		if (memcmp(local_hash, e->hash, e->hash_len) == 0)
			memcpy(out4, "[==]", 4);
		else
			memcpy(out4, "[<>]", 4);
		return;
	}
	free(lpath);
	if ((uint32_t)lst.st_mtime == e->mtime)
		memcpy(out4, "[==]", 4);
	else
		memcpy(out4, "[<>]", 4);
}

static int
cmp_entry(const void *a, const void *b)
{
	const struct entry *ea = (const struct entry *)a;
	const struct entry *eb = (const struct entry *)b;
	if (ea->is_parent != eb->is_parent)
		return ea->is_parent ? -1 : 1;
	if (ea->is_dir != eb->is_dir)
		return ea->is_dir ? -1 : 1;
	return strcmp(ea->name, eb->name);
}

static void
free_entries(struct entry *entries, uint32_t count)
{
	if (entries == NULL)
		return;
	for (uint32_t i = 0; i < count; i++) {
		free(entries[i].name);
		free(entries[i].line);
		free(entries[i].lname);
		free(entries[i].rpath);
	}
	free(entries);
}

/* Populated on failure with a caller-visible description. */
static char load_dir_err[256];

static void
set_load_err(const char *op, struct sftpc_outcome *out)
{
	snprintf(load_dir_err, sizeof(load_dir_err),
	    "%s failed: %s", op, out->estr);
}

static void
set_load_err_result(const char *op, uint32_t code)
{
	snprintf(load_dir_err, sizeof(load_dir_err),
	    "%s failed: %s (%" PRIu32 ")",
	    op, sftp_get_errcode_name(code), code);
}

static struct entry *
load_dir(const char *path, const char *dldir, uint32_t *out_count)
{
	*out_count = 0;
	load_dir_err[0] = '\0';

	SFTPC_OUTCOME_DECL(out, 256);
	sftp_dirhandle_t handle = NULL;
	if (!sftpc_opendir(sftp_state, path, &handle, out)) {
		set_load_err("opendir", out);
		return NULL;
	}
	if (out->result != SSH_FX_OK) {
		set_load_err_result("opendir", out->result);
		return NULL;
	}

	struct entry *entries = NULL;
	uint32_t nentries = 0;
	uint32_t cap = 0;

	/* Insert ".." unless we're at root. */
	if (strcmp(path, "/") != 0) {
		entries = (struct entry *)calloc(1, sizeof(*entries));
		if (entries == NULL) {
			sftpc_close(sftp_state, &handle, out);
			return NULL;
		}
		entries[0].name = strdup("..");
		entries[0].is_dir = true;
		entries[0].is_parent = true;
		entries[0].line = strdup("<DIR>             ..");
		if (entries[0].name == NULL || entries[0].line == NULL) {
			free_entries(entries, 1);
			sftpc_close(sftp_state, &handle, out);
			return NULL;
		}
		nentries = 1;
		cap = 1;
	}

	bool eof = false;
	while (!eof) {
		struct sftpc_dir_entry *chunk = NULL;
		uint32_t chunk_n = 0;
		if (!sftpc_readdir(sftp_state, handle, &chunk, &chunk_n, &eof, out)) {
			set_load_err("readdir", out);
			free_entries(entries, nentries);
			sftpc_close(sftp_state, &handle, out);
			return NULL;
		}
		if (!eof && out->result != SSH_FX_OK && chunk_n == 0) {
			set_load_err_result("readdir", out->result);
			free_entries(entries, nentries);
			sftpc_close(sftp_state, &handle, out);
			return NULL;
		}
		if (chunk_n == 0) {
			sftpc_free_dir_entries(chunk, chunk_n);
			continue;
		}
		if (nentries + chunk_n > cap) {
			uint32_t new_cap = cap ? cap * 2 : 16;
			while (new_cap < nentries + chunk_n)
				new_cap *= 2;
			struct entry *ne = (struct entry *)realloc(entries, new_cap * sizeof(*entries));
			if (ne == NULL) {
				sftpc_free_dir_entries(chunk, chunk_n);
				free_entries(entries, nentries);
				sftpc_close(sftp_state, &handle, out);
				return NULL;
			}
			entries = ne;
			cap = new_cap;
		}
		for (uint32_t i = 0; i < chunk_n; i++) {
			const char *nm = (const char *)chunk[i].filename->c_str;
			/* Skip "." and ".." from the server — we synthesize ".." ourselves. */
			if (strcmp(nm, ".") == 0 || strcmp(nm, "..") == 0)
				continue;
			uint32_t perm = 0;
			bool have_perm = sftp_fattr_get_permissions(chunk[i].attrs, &perm);
			bool is_dir;
			if (have_perm) {
				is_dir = (perm & S_IFMT) == S_IFDIR;
			}
			else if (chunk[i].longname != NULL && chunk[i].longname->len > 0) {
				/* Fallback: SFTP v3 longname is ls -l style; first char
				 * indicates type. */
				is_dir = chunk[i].longname->c_str[0] == 'd';
			}
			else {
				is_dir = false;
			}
			uint64_t sz = 0;
			sftp_fattr_get_size(chunk[i].attrs, &sz);
			uint32_t mtime = 0;
			sftp_fattr_get_mtime(chunk[i].attrs, &mtime);
			char *lname = NULL;
			if (sftpc_get_extensions(sftp_state) & SFTP_EXT_LNAME) {
				sftp_str_t ln = sftp_fattr_get_ext_by_type(chunk[i].attrs,
				                                          SFTP_EXT_NAME_LNAME);
				if (ln != NULL && ln->len > 0) {
					lname = (char *)malloc(ln->len + 1);
					if (lname != NULL) {
						memcpy(lname, ln->c_str, ln->len);
						lname[ln->len] = '\0';
					}
				}
			}
			/* Prefer SHA-1 over MD5 when both are on the file —
			 * stronger hash, no real cost difference. */
			uint8_t hash_bytes[SHA1_DIGEST_SIZE];
			uint8_t hash_len = 0;
			if (!is_dir) {
				uint32_t exts = sftpc_get_extensions(sftp_state);
				sftp_str_t h = NULL;
				if (exts & SFTP_EXT_SHA1S) {
					h = sftp_fattr_get_ext_by_type(chunk[i].attrs,
					    SFTP_EXT_NAME_SHA1S);
					if (h != NULL && h->len == SHA1_DIGEST_SIZE) {
						memcpy(hash_bytes, h->c_str, SHA1_DIGEST_SIZE);
						hash_len = SHA1_DIGEST_SIZE;
					}
				}
				if (hash_len == 0 && (exts & SFTP_EXT_MD5S)) {
					h = sftp_fattr_get_ext_by_type(chunk[i].attrs,
					    SFTP_EXT_NAME_MD5S);
					if (h != NULL && h->len == MD5_DIGEST_SIZE) {
						memcpy(hash_bytes, h->c_str, MD5_DIGEST_SIZE);
						hash_len = MD5_DIGEST_SIZE;
					}
				}
			}
			/* Directories with an lname render the lname in the name
			 * column; files keep their filename and the lname goes to
			 * the preview line below. */
			const uint8_t *display_name = chunk[i].filename->c_str;
			uint32_t display_len = chunk[i].filename->len;
			if (is_dir && lname != NULL) {
				display_name = (const uint8_t *)lname;
				display_len = (uint32_t)strlen(lname);
			}
			char szbuf[32];
			char datebuf[16];
			char statbuf[STATUS_COL_W + 1];
			format_size(sz, is_dir, szbuf, sizeof(szbuf));
			format_mtime(mtime, datebuf, sizeof(datebuf));
			struct entry tmp = {0};
			tmp.name = (char *)nm;      /* borrowed for rebuild_status_chip */
			tmp.rpath = path_join(path, nm);
			tmp.size = sz;
			tmp.mtime = mtime;
			tmp.is_dir = is_dir;
			if (hash_len != 0) {
				memcpy(tmp.hash, hash_bytes, hash_len);
				tmp.hash_len = hash_len;
			}
			rebuild_status_chip(&tmp, dldir, statbuf);
			statbuf[STATUS_COL_W] = '\0';
			size_t linesz = strlen(szbuf) + 1 + strlen(datebuf) + 1 +
			                STATUS_COL_W + 1 + display_len + 1;
			char *line = (char *)malloc(linesz);
			if (line == NULL) {
				free(tmp.rpath);
				free(lname);
				sftpc_free_dir_entries(chunk, chunk_n);
				free_entries(entries, nentries);
				sftpc_close(sftp_state, &handle, out);
				return NULL;
			}
			snprintf(line, linesz, "%s %s %s %.*s",
			    szbuf, datebuf, statbuf,
			    (int)display_len, display_name);
			entries[nentries].name = strdup(nm);
			entries[nentries].line = line;
			entries[nentries].lname = lname;
			entries[nentries].rpath = tmp.rpath;
			entries[nentries].size = sz;
			entries[nentries].mtime = mtime;
			entries[nentries].is_dir = is_dir;
			entries[nentries].is_parent = false;
			if (hash_len != 0) {
				memcpy(entries[nentries].hash, hash_bytes, hash_len);
				entries[nentries].hash_len = hash_len;
			}
			else {
				entries[nentries].hash_len = 0;
			}
			if (entries[nentries].name == NULL) {
				free(line);
				free(tmp.rpath);
				free(lname);
				sftpc_free_dir_entries(chunk, chunk_n);
				free_entries(entries, nentries);
				sftpc_close(sftp_state, &handle, out);
				return NULL;
			}
			nentries++;
		}
		sftpc_free_dir_entries(chunk, chunk_n);
	}

	sftpc_close(sftp_state, &handle, out);
	qsort(entries, nentries, sizeof(*entries), cmp_entry);
	*out_count = nentries;
	return entries;
}

/* ---- Preview line ------------------------------------------------------ */

/*
 * Paint the second-last line of the screen with uifc.bclr | (cclr << 4)
 * and centre `lname` in it.  If lname is NULL, just clear the line.
 * Called between uifc.list returns so the preview reflects the current
 * cursor's entry.
 */
static void
draw_preview_line(const char *lname)
{
	int row = uifc.scrn_len;   /* scrn_len+1 is the bottom status line */
	int width = (int)uifc.scrn_width;
	if (row < 1 || width < 1)
		return;
	uint8_t attr = uifc.bclr | (uifc.cclr << 4);
	uint32_t fg32 = 0, bg32 = 0;
	attr2palette(attr, &fg32, &bg32);
	struct vmem_cell *cells = (struct vmem_cell *)malloc(sizeof(*cells) * width);
	if (cells == NULL)
		return;
	for (int i = 0; i < width; i++) {
		cells[i].ch = ' ';
		cells[i].legacy_attr = attr;
		cells[i].fg = fg32;
		cells[i].bg = bg32;
		cells[i].font = 0;
		cells[i].hyperlink_id = 0;
	}
	if (lname != NULL) {
		int len = (int)strlen(lname);
		if (len > width)
			len = width;
		int start = (width - len) / 2;
		for (int i = 0; i < len; i++)
			cells[start + i].ch = (unsigned char)lname[i];
	}
	vmem_puttext(1, row, width, row, cells);
	free(cells);
}

/* ---- Public entry point ------------------------------------------------ */

void
sftp_browser_run(struct bbslist *bbs)
{
	if (!sftp_available || sftp_state == NULL) {
		uifcmsg("SFTP not available on this connection", NULL);
		return;
	}

	struct ciolib_screen *savscrn = cp437_savescrn();
	init_uifc(true, true);

	/* Resolve a starting path.  Prefer the server-advertised pubdir
	 * (pubdir@syncterm.net extension) when offered — that's the
	 * server's chosen "front door" for browsing.  Fall back to
	 * realpath(".") (most servers' user home), then "/". */
	char *cwd = NULL;
	const char *pd = sftpc_get_pubdir(sftp_state);
	if (pd != NULL && pd[0] != '\0')
		cwd = strdup(pd);
	if (cwd == NULL) {
		sftp_str_t rp = NULL;
		SFTPC_OUTCOME_DECL(rp_out, 0);
		if (sftpc_realpath(sftp_state, ".", &rp, rp_out) && rp_out->result == SSH_FX_OK
		    && rp != NULL && rp->len > 0) {
			cwd = (char *)malloc(rp->len + 1);
			if (cwd != NULL) {
				memcpy(cwd, rp->c_str, rp->len);
				cwd[rp->len] = '\0';
			}
		}
		free_sftp_str(rp);
	}
	if (cwd == NULL)
		cwd = strdup("/");

	int cur = 0;
	int bar = 0;
	bool done = false;
	while (!done) {
		uint32_t nentries = 0;
		struct entry *entries = load_dir(cwd, bbs->dldir, &nentries);
		if (entries == NULL) {
			char msg[MAX_PATH + 32];
			char help[MAX_PATH + 320];
			snprintf(msg, sizeof(msg), "Can't list %s", cwd);
			snprintf(help, sizeof(help),
			    "Path: %s\n%s",
			    cwd,
			    load_dir_err[0] ? load_dir_err : "(no error detail)");
			uifcmsg(msg, help);
			break;
		}
		/* Build NULL-terminated char* array for uifc.list */
		char **opts = (char **)calloc(nentries + 1, sizeof(char *));
		if (opts == NULL) {
			free_entries(entries, nentries);
			break;
		}
		for (uint32_t i = 0; i < nentries; i++)
			opts[i] = entries[i].line;
		opts[nentries] = NULL;

		char title[MAX_PATH + 16];
		snprintf(title, sizeof(title), "SFTP: %s", cwd);
		if (cur >= (int)nentries)
			cur = nentries ? (int)nentries - 1 : 0;
		/* Cap list height so the window's shadow doesn't extend past
		 * scrn_len - 1, leaving scrn_len free for our preview line.
		 * Natural height is opts + vbrdrsize (vbrdrsize == 4). */
		uifc.list_height = (int)nentries + 4;
		if (uifc.list_height > (int)uifc.scrn_len - 4)
			uifc.list_height = (int)uifc.scrn_len - 4;
		uifc.helpbuf =
		    "`SFTP Browser`\n\n"
		    "Browse the remote filesystem.  Each row shows the file's\n"
		    "size, last-modified date, a queue/compare status, and the\n"
		    "name.\n\n"
		    "~ Status column ~\n"
		    "  `[==]`  Local copy in download dir matches the remote.\n"
		    "        When the server publishes a SHA-1 or MD5 hash, the\n"
		    "        match is verified against the local bytes; otherwise\n"
		    "        size + mtime are used.\n"
		    "  `[<>]`  Local copy exists but differs.\n"
		    "  `[Q\x19]`  Download is queued.\n"
		    "  `[Q\x18]`  Upload is queued.\n"
		    "  `[\x19\x19]`  Download is in progress.\n"
		    "  `[\x18\x18]`  Upload is in progress.\n"
		    "  `[er]`  Last transfer failed.\n"
		    "  `[cx]`  Last transfer cancelled.\n"
		    "  blank   No local file, no pending transfer.\n\n"
		    "~ Keys ~\n"
		    "  `Enter`  Directory: descend into it.  `..`: go up.\n"
		    "         File: download to the session's download directory.\n"
		    "  `Ins`    Upload a file of the same name from the session's\n"
		    "         upload directory, replacing the remote copy.\n"
		    "  `Del`    Cancel a queued/active transfer for the file.\n"
		    "  `F2`     Show the file's extended description (when the\n"
		    "         server advertises `descs@syncterm.net`).\n"
		    "  `Esc`    Leave the browser and return to the terminal.\n";

		/* Inner loop: stay on this directory listing, polling with
		 * WIN_DYN so we can redraw the file-lname preview line as
		 * the cursor moves and refresh the status column when queue
		 * state changes.  WIN_DYN returns -2 - gotkey on each tick
		 * (gotkey = 0 for a pure tick, otherwise the key code).  We
		 * sleep a bit between polls so we don't pin the CPU.  Normal
		 * selection returns *cur as a non-negative value; Esc returns
		 * -1. */
		int rc = 0;
		int last_cur = -1;
		bool first = true;
		bool force_redraw = false;
		int selected_idx = -1;
		uint64_t last_gen = sftp_queue_gen();
		while (true) {
			if (cur != last_cur) {
				const char *prev = NULL;
				if (cur >= 0 && cur < (int)nentries) {
					struct entry *e = &entries[cur];
					if (!e->is_dir && !e->is_parent && e->lname != NULL)
						prev = e->lname;
				}
				draw_preview_line(prev);
				last_cur = cur;
			}
			/* Refresh status chips in-place when the queue state
			 * changes.  Line layout is fixed so we can overwrite
			 * the 4-char chip at STATUS_COL_OFF without reallocating.
			 * Force a redraw on the next uifc.list call so the
			 * updated text actually paints. */
			uint64_t cur_gen = sftp_queue_gen();
			if (cur_gen != last_gen) {
				last_gen = cur_gen;
				for (uint32_t i = 0; i < nentries; i++) {
					if (entries[i].is_parent || entries[i].line == NULL)
						continue;
					rebuild_status_chip(&entries[i], bbs->dldir,
					    &entries[i].line[STATUS_COL_OFF]);
				}
				force_redraw = true;
			}
			uifc_winmode_t mode = WIN_MID | WIN_SAV | WIN_ACT | WIN_DYN
			    | WIN_FIXEDHEIGHT | WIN_EXTKEYS;
			if (!first && !force_redraw)
				mode |= WIN_NODRAW;
			force_redraw = false;
			first = false;
			rc = uifc.list(mode, 0, 0, 0, &cur, &bar, title, opts);
			if (rc == -1) {               /* Esc */
				done = true;
				break;
			}
			if (rc < -1) {                /* WIN_DYN tick or extended key */
				int key = -2 - rc;
				struct entry *e = NULL;
				if (cur >= 0 && cur < (int)nentries)
					e = &entries[cur];
				if (e != NULL && !e->is_dir && !e->is_parent
				    && e->rpath != NULL) {
					if (key == CIO_KEY_IC) {
						char *lp = (bbs->uldir[0])
						    ? path_join(bbs->uldir, e->name) : NULL;
						if (lp == NULL) {
							uifcmsg("No upload directory configured", NULL);
						}
						else {
							struct stat lst;
							if (stat(lp, &lst) != 0) {
								char m[MAX_PATH + 64];
								snprintf(m, sizeof(m),
								    "No local file: %s", lp);
								uifcmsg(m, NULL);
							}
							else {
								sftp_queue_enqueue(SFTP_JOB_UP, lp,
								    e->rpath, (uint64_t)lst.st_size);
							}
							free(lp);
						}
					}
					else if (key == CIO_KEY_DC) {
						sftp_queue_cancel(SFTP_JOB_UP, e->rpath);
						sftp_queue_cancel(SFTP_JOB_DN, e->rpath);
					}
					else if (key == CIO_KEY_F(2)) {
						if (!(sftpc_get_extensions(sftp_state) & SFTP_EXT_DESCS)) {
							uifcmsg("No description available",
							    "`No description`\n\n"
							    "The server did not advertise the\n"
							    "`descs@syncterm.net` extension.");
						}
						else {
							sftp_str_t desc = NULL;
							SFTPC_OUTCOME_DECL(desc_out, 0);
							if (sftpc_descs(sftp_state, e->rpath, &desc, desc_out)
							    && desc_out->result == SSH_FX_OK
							    && desc != NULL && desc->len > 0) {
								char *body = (char *)malloc(desc->len + 1);
								if (body != NULL) {
									memcpy(body, desc->c_str, desc->len);
									body[desc->len] = '\0';
									uifc.showbuf(WIN_SAV | WIN_MID, 0, 0,
									    76, uifc.scrn_len - 2,
									    e->name, body, NULL, NULL);
									free(body);
								}
							}
							else {
								char m[MAX_PATH + 32];
								snprintf(m, sizeof(m), "No description: %s", e->name);
								uifcmsg(m, NULL);
							}
							free_sftp_str(desc);
						}
						force_redraw = true;
					}
				}
				SLEEP(50);            /* throttle poll rate */
				continue;
			}
			/* rc >= 0 — Enter.  Files: enqueue download and keep
			 * the list open.  Directories (including "..") break out
			 * so the outer loop descends/ascends and reloads. */
			int idx = (int)(rc & MSK_OFF);
			if (idx >= 0 && idx < (int)nentries) {
				struct entry *e = &entries[idx];
				if (!e->is_dir && !e->is_parent && e->rpath != NULL) {
					char *lp = (bbs->dldir[0])
					    ? path_join(bbs->dldir, e->name) : NULL;
					if (lp == NULL) {
						uifcmsg("No download directory configured", NULL);
					}
					else {
						sftp_queue_enqueue(SFTP_JOB_DN, lp,
						    e->rpath, e->size);
						free(lp);
					}
					continue;
				}
			}
			selected_idx = idx;
			break;
		}
		free(opts);
		/* Clear preview before leaving this directory (whether we
		 * exit the browser entirely or descend/ascend). */
		draw_preview_line(NULL);

		if (done || selected_idx < 0 || selected_idx >= (int)nentries) {
			free_entries(entries, nentries);
			continue;
		}
		struct entry *e = &entries[selected_idx];
		char *next = NULL;
		if (e->is_parent) {
			next = path_parent(cwd);
		}
		else if (e->is_dir) {
			next = path_join(cwd, e->name);
		}
		/* Files: no action in phase 3. */
		free_entries(entries, nentries);
		if (next != NULL) {
			free(cwd);
			cwd = next;
			cur = 0;
			bar = 0;
		}
	}

	free(cwd);
	uifcbail();
	restorescreen(savscrn);
	freescreen(savscrn);
}
