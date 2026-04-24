/*
 * SFTP queue screen — Alt-Q inside the terminal.  Shows the current
 * queue with live progress and lets the user cancel transfers.
 *
 * Rendering follows the same pattern as sftp_browser.c: uifc.list with
 * WIN_DYN + WIN_EXTKEYS, polling the queue's generation counter every
 * tick and rebuilding the option strings when anything changes.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirwrap.h>
#include <genwrap.h>
#include <uifc.h>

#include "bbslist.h"
#include "ciolib.h"
#include "sftp_queue.h"
#include "sftp_queue_screen.h"
#include "uifcinit.h"

struct ciolib_screen *cp437_savescrn(void);

/*
 * Sort key: ACTIVE first (they're the most "live"), then QUEUED, then
 * terminal states (DONE / FAILED / CANCELLED).  Within a bucket the
 * array's existing insertion order is preserved since qsort is stable
 * for equal keys — except it isn't guaranteed.  We accept that for now;
 * the user sees a consistent grouping which is the important part.
 */
static int
status_order(int status)
{
	switch (status) {
		case SFTP_JOB_ACTIVE: return 0;
		case SFTP_JOB_QUEUED: return 1;
		case SFTP_JOB_FAILED: return 2;
		case SFTP_JOB_DONE:   return 3;
		case SFTP_JOB_CANCELLED: return 4;
	}
	return 5;
}

static int
snap_cmp(const void *a, const void *b)
{
	const struct sftp_job_snap *sa = (const struct sftp_job_snap *)a;
	const struct sftp_job_snap *sb = (const struct sftp_job_snap *)b;
	int oa = status_order(sa->status);
	int ob = status_order(sb->status);
	if (oa != ob)
		return oa - ob;
	return strcmp(sa->remote_path, sb->remote_path);
}

static const char *
status_label(int s)
{
	switch (s) {
		case SFTP_JOB_QUEUED:    return "QUEUED   ";
		case SFTP_JOB_ACTIVE:    return "ACTIVE   ";
		case SFTP_JOB_DONE:      return "DONE     ";
		case SFTP_JOB_FAILED:    return "FAILED   ";
		case SFTP_JOB_CANCELLED: return "CANCELLED";
	}
	return "?        ";
}

static char *
format_row(const struct sftp_job_snap *s)
{
	const char *dir_arrow = (s->dir == SFTP_JOB_UP) ? "\x18" : "\x19";
	const char *st = status_label(s->status);
	const char *local_fn = getfname(s->local_path);
	if (local_fn == NULL || local_fn[0] == '\0')
		local_fn = s->local_path;

	size_t linesz = 192 + strlen(s->err) +
	                strlen(local_fn) + strlen(s->remote_path);
	char *out = (char *)malloc(linesz);
	if (out == NULL)
		return NULL;
	if (s->status == SFTP_JOB_FAILED && s->err[0]) {
		snprintf(out, linesz, " %s %s  %s \x1a %s  (%s)",
		    dir_arrow, st, local_fn, s->remote_path, s->err);
	}
	else {
		char sdone[16], stotal[16];
		byte_estimate_to_str(s->done, sdone, sizeof(sdone), 0, 1);
		byte_estimate_to_str(s->total, stotal, sizeof(stotal), 0, 1);
		unsigned pct = 0;
		if (s->total > 0 && s->done <= s->total)
			pct = (unsigned)((s->done * 100) / s->total);
		else if (s->status == SFTP_JOB_DONE)
			pct = 100;
		snprintf(out, linesz, " %s %s  %6s/%-6s (%3u%%)  %s \x1a %s",
		    dir_arrow, st, sdone, stotal, pct, local_fn, s->remote_path);
	}
	return out;
}

/*
 * Build the uifc.list option array plus a parallel snapshot kept for
 * key-handler lookups (so Del can find the job under the cursor even
 * after a sort).  Caller frees via free_view.
 */
struct view {
	char                   **opts;     /* NULL-terminated char* array */
	struct sftp_job_snap    *snaps;    /* owned by queue snapshot API */
	uint32_t                 n;
};

static void
free_view(struct view *v)
{
	if (v == NULL)
		return;
	if (v->opts != NULL) {
		for (uint32_t i = 0; i < v->n; i++)
			free(v->opts[i]);
		free(v->opts);
	}
	sftp_queue_free_snapshot(v->snaps, v->n);
	memset(v, 0, sizeof(*v));
}

static bool
build_view(struct view *v)
{
	memset(v, 0, sizeof(*v));
	v->n = sftp_queue_snapshot(&v->snaps);
	if (v->snaps == NULL && v->n > 0)
		return false;
	if (v->n > 0)
		qsort(v->snaps, v->n, sizeof(v->snaps[0]), snap_cmp);
	v->opts = (char **)calloc(v->n + 2, sizeof(char *));
	if (v->opts == NULL) {
		sftp_queue_free_snapshot(v->snaps, v->n);
		v->snaps = NULL;
		v->n = 0;
		return false;
	}
	if (v->n == 0) {
		v->opts[0] = strdup(" (queue is empty)");
		v->opts[1] = NULL;
	}
	else {
		for (uint32_t i = 0; i < v->n; i++) {
			v->opts[i] = format_row(&v->snaps[i]);
			if (v->opts[i] == NULL)
				v->opts[i] = strdup("(oom)");
		}
		v->opts[v->n] = NULL;
	}
	return true;
}

void
sftp_queue_screen_run(struct bbslist *bbs)
{
	(void)bbs;
	struct ciolib_screen *savscrn = cp437_savescrn();
	init_uifc(true, true);

	int cur = 0;
	int bar = 0;
	bool done = false;
	uint64_t last_gen = 0;

	struct view view;
	memset(&view, 0, sizeof(view));

	bool first = true;
	while (!done) {
		uint64_t cur_gen = sftp_queue_gen();
		bool rebuilt = false;
		if (view.opts == NULL || cur_gen != last_gen) {
			free_view(&view);
			if (!build_view(&view)) {
				uifcmsg("Queue snapshot failed", NULL);
				break;
			}
			last_gen = cur_gen;
			rebuilt = true;
			/* Clamp cur to the new list. */
			if (cur >= (int)view.n)
				cur = (view.n > 0) ? (int)view.n - 1 : 0;
		}
		int rows = (int)view.n;
		if (rows < 1)
			rows = 1;
		uifc.list_height = rows + 4;
		if (uifc.list_height > (int)uifc.scrn_len - 4)
			uifc.list_height = (int)uifc.scrn_len - 4;

		uifc.helpbuf =
		    "`SFTP Queue`\n\n"
		    "All pending and finished transfers for this session.  The\n"
		    "first column shows direction (`\x18` upload, `\x19` download); the\n"
		    "second shows state.  Active transfers advance their progress\n"
		    "counter in place.\n\n"
		    "~ Keys ~\n"
		    "  `Del`  Cancel the highlighted transfer (queued or active).\n"
		    "  `Esc`  Close the queue screen.\n\n"
		    "Queued jobs persist across sessions per BBS: the next time you\n"
		    "connect, any unfinished transfers resume automatically.\n";

		uifc_winmode_t mode = WIN_MID | WIN_SAV | WIN_ACT | WIN_DYN
		    | WIN_FIXEDHEIGHT | WIN_EXTKEYS;
		if (!first && !rebuilt)
			mode |= WIN_NODRAW;
		first = false;
		int rc = uifc.list(mode, 0, 0, 0, &cur, &bar, "SFTP Queue", view.opts);
		if (rc == -1) {
			done = true;
			break;
		}
		if (rc < -1) {
			int key = -2 - rc;
			if (key == CIO_KEY_DC && cur >= 0 && cur < (int)view.n) {
				struct sftp_job_snap *s = &view.snaps[cur];
				sftp_queue_cancel(s->dir, s->remote_path);
			}
			SLEEP(50);
			continue;
		}
		/* rc >= 0 — Enter.  No per-row action yet; keep polling. */
		SLEEP(50);
	}

	free_view(&view);
	uifcbail();
	restorescreen(savscrn);
	freescreen(savscrn);
}
