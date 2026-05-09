/* Conn / CTerm / ExtAttr / LastColumnFlag foreigns + accessors —
 * connection state, emulator pointer, and the bitfield-snapshot
 * foreigns CTerm hands out.
 *
 * cterm is term.c's global; conn_send / conn_buf_* are conn.c's
 * I/O API.  st->cterm_suspended is a pointer into a doterm() local
 * — when true, doterm() stops draining the conn buffer.  Reads
 * `false` outside doterm()'s window. */

#include "wren_bind_conn.h"
#include "wren_bind_internal.h"
#include "wren_host_internal.h"
#include "wren_host.h"

#include "bbslist.h"   /* rates[], get_rate_num */
#include "ciolib.h"
#include "comio.h"     /* COM_FLOW_CONTROL_* — also used by FlowControl in wren_bind.c */
#include "conn.h"
#include "cterm.h"
#include "genwrap.h"   /* xp_fast_timer64 */
#include "syncterm.h"  /* safe_mode */
#include "term.h"      /* force_status_update, term, struct mouse_state, setup_mouse_events */
#include "uifcinit.h"  /* uifc (the singleton), UIFC_XF_QUIT */
#include "wren_bind_fs.h" /* wren_file_consume_write, FILE_ERR_* */
#ifndef WITHOUT_OOII
#include "ooii.h"      /* MAX_OOII_MODE */
#endif
#include "xpbeep.h"    /* xptone_open / xptone_close — OOII toggling */
#ifndef WITHOUT_DEUCESSH
#include "ssh.h"       /* ssh_set_sftp_buffer_mode */
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct cterminal *cterm;

/* ----- ConnError -------------------------------------------------- */

/* Recoverable wire-side failures from Conn.send / Conn.sendRaw.
 * Returned IN PLACE of `null` on failure; null still means success.
 * Mirrors the SFTPError / FileError pattern. */
enum conn_err_code {
	CONN_ERR_OK = 0,
	CONN_ERR_NOT_CONNECTED,  /* connection is down at send time */
	CONN_ERR_SHORT_SEND,     /* outbuf full / partial queue */
};

struct wren_conn_error {
	enum syncterm_wren_foreign type;
	uint32_t code;       /* enum conn_err_code */
	int      bytes_sent; /* for SHORT_SEND: how much was queued before
	                      * the buffer ran out — caller can retry the
	                      * remainder */
	char    *message;    /* malloc'd; may be NULL */
};

void
wren_conn_error_allocate(WrenVM *vm)
{
	struct wren_conn_error *e =
	    wrenSetSlotNewForeign(vm, 0, 0, sizeof(*e));
	memset(e, 0, sizeof(*e));
	e->type = SWF_CONN_ERROR;
}

void
wren_conn_error_finalize(void *data)
{
	struct wren_conn_error *e = data;
	free(e->message);
}

void
fn_ConnError_code(WrenVM *vm)
{
	struct wren_conn_error *e = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)e->code);
}

void
fn_ConnError_bytesSent(WrenVM *vm)
{
	struct wren_conn_error *e = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)e->bytes_sent);
}

void
fn_ConnError_message(WrenVM *vm)
{
	struct wren_conn_error *e = wrenGetSlotForeign(vm, 0);
	if (e->message != NULL)
		wrenSetSlotString(vm, 0, e->message);
	else
		wrenSetSlotNull(vm, 0);
}

void
fn_ConnError_toString(WrenVM *vm)
{
	struct wren_conn_error *e = wrenGetSlotForeign(vm, 0);
	const char *name;
	switch ((enum conn_err_code)e->code) {
	case CONN_ERR_OK:            name = "OK";             break;
	case CONN_ERR_NOT_CONNECTED: name = "NOT_CONNECTED";  break;
	case CONN_ERR_SHORT_SEND:    name = "SHORT_SEND";     break;
	default:                     name = "UNKNOWN";        break;
	}
	char buf[200];
	if (e->code == CONN_ERR_SHORT_SEND)
		snprintf(buf, sizeof(buf),
		    "ConnError: %s: %d bytes queued%s%s",
		    name, e->bytes_sent,
		    e->message != NULL ? ": " : "",
		    e->message != NULL ? e->message : "");
	else if (e->message != NULL)
		snprintf(buf, sizeof(buf),
		    "ConnError: %s: %s", name, e->message);
	else
		snprintf(buf, sizeof(buf), "ConnError: %s", name);
	wrenSetSlotString(vm, 0, buf);
}

/* Build a ConnError into slot 0.  Caller returns immediately; the
 * error IS the function's return value. */
static void
conn_build_error(WrenVM *vm, enum conn_err_code code, int bytes_sent,
                 const char *msg)
{
	struct wren_host_state *st = wren_host_state();
	wrenEnsureSlots(vm, 2);
	load_class_into_slot(vm, &st->conn_error_class, "ConnError", 1);
	struct wren_conn_error *e =
	    wrenSetSlotNewForeign(vm, 0, 1, sizeof(*e));
	memset(e, 0, sizeof(*e));
	e->type       = SWF_CONN_ERROR;
	e->code       = (uint32_t)code;
	e->bytes_sent = bytes_sent;
	if (msg != NULL)
		e->message = strdup(msg);
}

/* ----- Conn -------------------------------------------------------- */

/* Wire-side send.  Returns null on success, ConnError on failure
 * (connection down, outbuf full / partial queue).  Empty input is
 * a no-op success (returns null). */
void
fn_Conn_send(WrenVM *vm)
{
	int len = 0;
	const char *s = wrenGetSlotBytes(vm, 1, &len);
	if (len <= 0) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	if (!conn_connected()) {
		conn_build_error(vm, CONN_ERR_NOT_CONNECTED, 0,
		    "Conn.send: connection is down");
		return;
	}
	int sent = conn_send(s, (size_t)len, 0);
	if (sent < len) {
		conn_build_error(vm, CONN_ERR_SHORT_SEND, sent,
		    "Conn.send: outbuf full");
		return;
	}
	wrenSetSlotNull(vm, 0);
}

/* Same shape as fn_Conn_send but bypasses the IAC-escape pipeline. */
void
fn_Conn_sendRaw(WrenVM *vm)
{
	int len = 0;
	const char *s = wrenGetSlotBytes(vm, 1, &len);
	if (len <= 0) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	if (!conn_connected()) {
		conn_build_error(vm, CONN_ERR_NOT_CONNECTED, 0,
		    "Conn.sendRaw: connection is down");
		return;
	}
	int sent = conn_send_raw(s, (size_t)len, 0);
	if (sent < len) {
		conn_build_error(vm, CONN_ERR_SHORT_SEND, sent,
		    "Conn.sendRaw: outbuf full");
		return;
	}
	wrenSetSlotNull(vm, 0);
}

void
fn_Conn_close(WrenVM *vm)
{
	(void)vm;
	conn_close();
}

/* Conn.endSession(exitApp) — request a session end.  doterm() picks
 * the request up at the top of its next key-dispatch iteration via
 * `wren_host_take_pending_disconnect`, runs the existing
 * "Disconnect... Are you sure?" confirm, and either tears the
 * cterm down + returns to the bbslist (`exitApp == false`, like
 * Alt-H / Ctrl-Q) or also flags syncterm for full exit
 * (`exitApp == true`, like Alt-X / window-close).  Designed for
 * default key handlers that used to live as C cases in doterm();
 * synchronous from the script's perspective — the binding sets
 * the flag and returns immediately, the modal confirm runs on
 * doterm's next iteration. */
void
fn_Conn_endSession(WrenVM *vm)
{
	bool exit_app = false;
	if (wrenGetSlotType(vm, 1) == WREN_TYPE_BOOL)
		exit_app = wrenGetSlotBool(vm, 1);
	struct wren_host_state *st = wren_host_state();
	if (st == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	st->pending_disconnect      = true;
	st->pending_disconnect_exit = exit_app;
	if (exit_app)
		uifc.exit_flags |= UIFC_XF_QUIT;
	wrenSetSlotNull(vm, 0);
}

/* Conn.paste() — clipboard → wire.  Wraps the historical Shift-Insert
 * handler's `do_paste()` call: codepage conversion, alt-font
 * awareness, bracketed-paste mode.  No-op when clipboard is empty. */
void
fn_Conn_paste(WrenVM *vm)
{
	(void)vm;
	do_paste();
}

/* Conn.scrollback() — delegates to the Wren-side ScrollbackView.run()
 * in scripts/auto/connected/scrollback_view.wren.  No more uifc viewer;
 * the Wren modal owns the entire scrollback browse experience now. */
void
fn_Conn_scrollback(WrenVM *vm)
{
	wrenEnsureSlots(vm, 1);
	wrenGetVariable(vm, "scrollback_view", "ScrollbackView", 0);
	WrenHandle *cls = wrenGetSlotHandle(vm, 0);
	WrenHandle *run = wrenMakeCallHandle(vm, "run()");
	wrenSetSlotHandle(vm, 0, cls);
	wrenCall(vm, run);
	wrenReleaseHandle(vm, cls);
	wrenReleaseHandle(vm, run);
}

void
fn_Conn_connected(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, conn_connected());
}

void
fn_Conn_type(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	int t = (st != NULL && st->bbs != NULL) ? st->bbs->conn_type : 0;
	wrenSetSlotDouble(vm, 0, (double)t);
}

void
fn_Conn_pending(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)conn_buf_bytes(&conn_inbuf));
}

void
fn_Conn_queued(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)conn_buf_bytes(&conn_outbuf));
}

void
fn_Conn_peek(WrenVM *vm)
{
	int n = (int)wrenGetSlotDouble(vm, 1);
	if (n <= 0) {
		wrenSetSlotBytes(vm, 0, "", 0);
		return;
	}
	if ((size_t)n > conn_inbuf.bufsize)
		n = (int)conn_inbuf.bufsize;
	char *tmp = malloc((size_t)n);
	if (tmp == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	size_t got = conn_buf_peek(&conn_inbuf, tmp, (size_t)n);
	wrenSetSlotBytes(vm, 0, tmp, got);
	free(tmp);
}

void
fn_Conn_recv(WrenVM *vm)
{
	int n = (int)wrenGetSlotDouble(vm, 1);
	if (n <= 0) {
		wrenSetSlotBytes(vm, 0, "", 0);
		return;
	}
	if ((size_t)n > conn_inbuf.bufsize)
		n = (int)conn_inbuf.bufsize;
	char *tmp = malloc((size_t)n);
	if (tmp == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	size_t got = conn_buf_get(&conn_inbuf, tmp, (size_t)n);
	wrenSetSlotBytes(vm, 0, tmp, got);
	free(tmp);
}

/* ----- CTerm ------------------------------------------------------- */

void
fn_CTerm_emulation(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, cterm != NULL ? (double)cterm->emulation : 0.0);
}

void
fn_CTerm_x(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, cterm != NULL ? (double)cterm->xpos : 0.0);
}

void
fn_CTerm_y(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, cterm != NULL ? (double)cterm->ypos : 0.0);
}

void
fn_CTerm_attr(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, cterm != NULL ? (double)cterm->attr : 0.0);
}

void
fn_CTerm_doorwayMode(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0,
	    cterm != NULL && cterm->doorway_mode != 0);
}

/* CTerm.doorwayMode = b — toggle CTerm's doorway-keyboard mode. */
void
fn_CTerm_doorwayMode_set(WrenVM *vm)
{
	if (cterm == NULL)
		return;
	cterm->doorway_mode = wrenGetSlotBool(vm, 1) ? 1 : 0;
	force_status_update = true;
}

void
fn_CTerm_music(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0,
	    cterm != NULL ? (double)cterm->music_enable : 0.0);
}

/* CTerm.music = i — set the ANSI music enable level (see MusicMode in
 * the Wren-side syncterm module).  Out-of-range values are clamped to
 * the legal 0..N-1 range so a script can't push cterm into a state the
 * emulator won't honour. */
void
fn_CTerm_music_set(WrenVM *vm)
{
	if (cterm == NULL)
		return;
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_NUM)
		return;
	int i = (int)wrenGetSlotDouble(vm, 1);
	int n = 0;
	while (music_names[n] != NULL)
		n++;
	if (i < 0)
		i = 0;
	if (n > 0 && i >= n)
		i = n - 1;
	cterm->music_enable = i;
}

void
fn_CTerm_width(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, cterm != NULL ? (double)cterm->width : 0.0);
}

void
fn_CTerm_height(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, cterm != NULL ? (double)cterm->height : 0.0);
}

void
fn_CTerm_write(WrenVM *vm)
{
	int len = 0;
	const char *s = wrenGetSlotBytes(vm, 1, &len);
	int speed = 0;
	if (cterm != NULL && len > 0)
		cterm_write(cterm, s, len, &speed);
}

/* CTerm.suspended — backed by a doterm() local that gates the
 * wire-byte pump.  When true, doterm() stops draining the conn buffer
 * so the script can do something the screen can't be allowed to be
 * scribbled on (modal dialog, transfer overlay, …) without the
 * remote's output painting underneath.  Bytes pile up in the conn
 * buffer; eventually the TCP window fills and the remote sees
 * backpressure.  Reads false when no flag is bound (pre-init or
 * post-shutdown). */
void
fn_CTerm_suspended_get(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	bool v = (st != NULL && st->cterm_suspended != NULL)
	    ? *st->cterm_suspended : false;
	wrenSetSlotBool(vm, 0, v);
}

void
fn_CTerm_suspended_set(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->cterm_suspended == NULL)
		return;
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_BOOL)
		return;
	*st->cterm_suspended = wrenGetSlotBool(vm, 1);
}

/* CTerm.sftpActive — Wren-set flag the SSH driver reads to decide
 * whether to keep the session alive when the shell channel closes
 * (or the wire goes idle).  SftpQueue raises it whenever a job is
 * QUEUED or ACTIVE and clears it when the queue drains; ssh.c OR's
 * its own per-direction worker counts with wren_sftp_active() so the
 * connection stays up while either side has work.  Cleared on VM
 * teardown by wren_sftp_active_reset(). */
static bool sftp_active_flag = false;

void
fn_CTerm_sftpActive_get(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, sftp_active_flag);
}

void
fn_CTerm_sftpActive_set(WrenVM *vm)
{
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_BOOL)
		return;
	bool new_val = wrenGetSlotBool(vm, 1);
	if (new_val == sftp_active_flag)
		return;
	sftp_active_flag = new_val;
	/* Adjust the SSH socket's SO_SNDBUF cap to fit the workload
	 * shift — see ssh_set_sftp_buffer_mode for the rationale.
	 * Same-value sets short-circuited above so a tight queue run
	 * (job1 done → job2 starts immediately, sftpActive stays true
	 * throughout) doesn't flap the kernel buffer. */
#ifndef WITHOUT_DEUCESSH
	ssh_set_sftp_buffer_mode(new_val);
#endif
}

bool
wren_sftp_active(void)
{
	return sftp_active_flag;
}

void
wren_sftp_active_reset(void)
{
	sftp_active_flag = false;
}

#define CTERM_FIELD_NUM(field)                                          \
	do {                                                            \
		double v = (cterm != NULL) ? (double)cterm->field : 0.0;\
		wrenSetSlotDouble(vm, 0, v);                            \
	} while (0)

#define CTERM_FIELD_BOOL(field)                                         \
	do {                                                            \
		bool v = (cterm != NULL) ? (bool)cterm->field : false;  \
		wrenSetSlotBool(vm, 0, v);                              \
	} while (0)

void fn_CTerm_originX(WrenVM *vm)        { CTERM_FIELD_NUM(x); }
void fn_CTerm_originY(WrenVM *vm)        { CTERM_FIELD_NUM(y); }
void fn_CTerm_topMargin(WrenVM *vm)      { CTERM_FIELD_NUM(top_margin); }
void fn_CTerm_bottomMargin(WrenVM *vm)   { CTERM_FIELD_NUM(bottom_margin); }
void fn_CTerm_leftMargin(WrenVM *vm)     { CTERM_FIELD_NUM(left_margin); }
void fn_CTerm_rightMargin(WrenVM *vm)    { CTERM_FIELD_NUM(right_margin); }
void fn_CTerm_fontSlot(WrenVM *vm)       { CTERM_FIELD_NUM(font_slot); }
void fn_CTerm_scrollbackLines(WrenVM *vm){ CTERM_FIELD_NUM(backlines); }
void fn_CTerm_scrollbackWidth(WrenVM *vm){ CTERM_FIELD_NUM(backwidth); }
void fn_CTerm_scrollbackPos(WrenVM *vm)  { CTERM_FIELD_NUM(backpos); }
void fn_CTerm_scrollbackStart(WrenVM *vm){ CTERM_FIELD_NUM(backstart); }
void fn_CTerm_started(WrenVM *vm)        { CTERM_FIELD_BOOL(started); }
void fn_CTerm_skypix(WrenVM *vm)         { CTERM_FIELD_BOOL(skypix); }
void fn_CTerm_hasPaletteOverride(WrenVM *vm){ CTERM_FIELD_BOOL(has_palette_override); }
void fn_CTerm_statusDisplay(WrenVM *vm)  { CTERM_FIELD_NUM(status_display_type); }

/* Request a SyncTERM status-bar repaint on the next update_status
 * pass.  No-op when the bar is disabled (statusDisplay == 0) — the
 * flag is checked anyway, but update_status returns early for
 * term.nostatus.  Useful after Screen.restore() puts a stale snapshot
 * on the status row: this causes the bar's live state (speed,
 * transfer arrows, log indicators) to be re-emitted. */
void
fn_CTerm_refreshStatus(WrenVM *vm)
{
	(void)vm;
	force_status_update = true;
}

void
fn_CTerm_fgColor(WrenVM *vm)
{
	uint32_t v = (cterm != NULL) ? (cterm->fg_color & 0xFFFFFFu) : 0;
	wrenSetSlotDouble(vm, 0, (double)v);
}

void
fn_CTerm_bgColor(WrenVM *vm)
{
	uint32_t v = (cterm != NULL) ? (cterm->bg_color & 0xFFFFFFu) : 0;
	wrenSetSlotDouble(vm, 0, (double)v);
}

void
fn_CTerm_paletteOverride(WrenVM *vm)
{
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
	if (cterm == NULL || !cterm->has_palette_override)
		return;
	const unsigned cap = sizeof(cterm->palette_override) /
	    sizeof(cterm->palette_override[0]);
	for (unsigned i = 0; i < cap; i++) {
		uint32_t rgb = cterm->palette_override[i] & 0xFFFFFFu;
		wrenSetSlotDouble(vm, 1, (double)rgb);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

void
fn_CTerm_altFonts(WrenVM *vm)
{
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
	if (cterm == NULL)
		return;
	const unsigned cap = sizeof(cterm->altfont) /
	    sizeof(cterm->altfont[0]);
	for (unsigned i = 0; i < cap; i++) {
		wrenSetSlotDouble(vm, 1, (double)cterm->altfont[i]);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

/* CTerm.altFont — primary alt-font slot (altfont[0]), used for
 * non-styled cells.  The font picker writes through this; the other
 * three altfont entries (bold / blink / bold+blink) keep whatever
 * conio_init assigned.  Setter calls setfont(slot, false, 1) to
 * reload, then stashes the result so it survives mode changes. */
void
fn_CTerm_altFont(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0,
	    cterm != NULL ? (double)cterm->altfont[0] : 0.0);
}

void
fn_CTerm_altFont_set(WrenVM *vm)
{
	if (cterm == NULL || wrenGetSlotType(vm, 1) != WREN_TYPE_NUM) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	int slot = (int)wrenGetSlotDouble(vm, 1);
	setfont(slot, false, 1);
	cterm->altfont[0] = slot;
	wrenSetSlotNull(vm, 0);
}

/* CTerm.saveScreenshot(file, withSauce) — write the cterm area as
 * IBM-CGA / BinaryText to `file` (a write-consent File from
 * Host.pickSavePath).  When `withSauce` is true, appends a SAUCE
 * block populated from the active BBS (name → title, user → author).
 *
 * Status bar is NOT included — gettext is called with cterm
 * dimensions, not the full term area, matching the historical Alt-C
 * binary save behaviour.
 *
 * Consumes the File's write consent on success.  Returns null on
 * success, or a FileError on fopen / fwrite failure (errno captured). */
void
fn_CTerm_saveScreenshot(WrenVM *vm)
{
	if (cterm == NULL) {
		wren_throw(vm, "CTerm.saveScreenshot: no terminal");
		return;
	}
	if (wrenGetSlotType(vm, 2) != WREN_TYPE_BOOL) {
		wren_throw(vm,
		    "CTerm.saveScreenshot: withSauce must be a Bool");
		return;
	}
	bool with_sauce = wrenGetSlotBool(vm, 2);
	FILE *fp = wren_file_consume_write(vm, 1, NULL);
	if (fp == NULL)
		return;
	struct wren_host_state *st  = wren_host_state();
	struct bbslist         *bbs = (st != NULL) ? st->bbs : NULL;
	int rc = save_screen_binary(fp, with_sauce, bbs);
	int close_err = (fclose(fp) == EOF) ? errno : 0;
	if (rc != 0 || close_err != 0) {
		file_build_error(vm, 0, FILE_ERR_WRITE_FAILED,
		    rc != 0 ? rc : close_err,
		    "screen save failed");
		return;
	}
	wrenSetSlotNull(vm, 0);
}

/* ---- Capture: streaming-log control ----------------------------- */

/* Capture.active — true iff a streaming log is open (regardless of
 * pause state).  Replaces CTerm.logMode for the Wren-side surface. */
void
fn_Capture_active(WrenVM *vm)
{
	bool v = (cterm != NULL) && ((cterm->log & CTERM_LOG_MASK) != 0);
	wrenSetSlotBool(vm, 0, v);
}

/* Capture.paused — true iff the active log is paused.  Replaces
 * CTerm.logPaused. */
void
fn_Capture_paused(WrenVM *vm)
{
	bool v = (cterm != NULL) && ((cterm->log & CTERM_LOG_PAUSED) != 0);
	wrenSetSlotBool(vm, 0, v);
}

/* Capture.start(file, raw) — open a streaming log to the path
 * authorized by `file` (a write-consent File from Host.pickSavePath).
 * `raw` selects ANSI capture mode: true → CTERM_LOG_RAW (preserves
 * escape sequences), false → CTERM_LOG_ASCII (stripped).
 *
 * Consumes the File's write consent on success.  Returns null on
 * success, or a FileError when the open path fails (fopen errno
 * captured).  Aborts the fiber on programmer error (already
 * capturing, no consent on the File, etc.). */
void
fn_Capture_start(WrenVM *vm)
{
	if (cterm == NULL) {
		wren_throw(vm, "Capture.start: no terminal");
		return;
	}
	if (cterm->log & CTERM_LOG_MASK) {
		wren_throw(vm, "Capture.start: already capturing");
		return;
	}
	if (wrenGetSlotType(vm, 2) != WREN_TYPE_BOOL) {
		wren_throw(vm, "Capture.start: raw must be a Bool");
		return;
	}
	bool raw = wrenGetSlotBool(vm, 2);
	FILE *fp = wren_file_consume_write(vm, 1, NULL);
	if (fp == NULL)
		return;     /* error already in slot 0 or fiber aborted */
	if (!cterm->started)
		cterm_start(cterm);
	cterm->logfile = fp;
	cterm->log     = raw ? CTERM_LOG_RAW : CTERM_LOG_ASCII;
	wrenSetSlotNull(vm, 0);
}

/* Capture.stop() — close the active log.  No-op when not capturing. */
void
fn_Capture_stop(WrenVM *vm)
{
	(void)vm;
	if (cterm == NULL)
		return;
	cterm_closelog(cterm);
}

/* Capture.pause() — set the PAUSED bit on the active log.  No-op
 * when not capturing or already paused. */
void
fn_Capture_pause(WrenVM *vm)
{
	(void)vm;
	if (cterm == NULL)
		return;
	if (cterm->log & CTERM_LOG_MASK)
		cterm->log |= CTERM_LOG_PAUSED;
}

/* Capture.resume() — clear the PAUSED bit.  No-op when not
 * capturing. */
void
fn_Capture_resume(WrenVM *vm)
{
	(void)vm;
	if (cterm == NULL)
		return;
	cterm->log = cterm->log & CTERM_LOG_MASK;
}

/* CTerm.atasciiInverse — true while ATASCII inverse-video mode is on.
 * Read by the default status bar to surface "(INV)". */
void
fn_CTerm_atasciiInverse(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0,
	    cterm != NULL && cterm_atascii_inverse(cterm));
}

/* CTerm.ooiiMode — current OOII mode (0 = off, 1..3 = OOTerm/OOTerm1/
 * OOTerm2).  Backed by doterm()'s local; reads 0 when no session is
 * active. */
void
fn_CTerm_ooiiMode(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	int v = (st != NULL && st->ooii_mode != NULL) ? *st->ooii_mode : 0;
	wrenSetSlotDouble(vm, 0, (double)v);
}

/* CTerm.ooiiMode = n — set the OOII mode.  Out-of-range values are
 * clamped to 0..MAX_OOII_MODE.  Toggles the xptone audio context to
 * match: open while a non-zero mode is active, close on return to 0.
 * No-op when the session is inactive (`st->ooii_mode == NULL`) or
 * the build was compiled WITHOUT_OOII. */
void
fn_CTerm_ooiiMode_set(WrenVM *vm)
{
#ifdef WITHOUT_OOII
	(void)vm;
#else
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->ooii_mode == NULL)
		return;
	int n = (int)wrenGetSlotDouble(vm, 1);
	if (n < 0)
		n = 0;
	if (n > MAX_OOII_MODE)
		n = MAX_OOII_MODE;
	int prev = *st->ooii_mode;
	*st->ooii_mode = n;
	if (n == 0 && prev != 0)
		xptone_close();
	else if (n != 0 && prev == 0)
		xptone_open();
	force_status_update = true;
#endif
}

/* CTerm.mouseMode — raw enum value from cterm->mouse_state_change_cbdata
 * (MM_OFF == 0, MM_RIP == 1, MM_X10 == 9, MM_*_TRACKING == 1000-1003).
 * Used by the default status bar to decide whether to light "M". */
void
fn_CTerm_mouseMode(WrenVM *vm)
{
	int v = 0;
	if (cterm != NULL && cterm->mouse_state_change_cbdata != NULL) {
		struct mouse_state *ms = cterm->mouse_state_change_cbdata;
		v = (int)ms->mode;
	}
	wrenSetSlotDouble(vm, 0, (double)v);
}

/* CTerm.mouseDisabled — companion to mouseMode: true when the mouse is
 * in a tracking mode but the conio backend has signalled it can't
 * actually report events (no SDL pointer, headless, etc.). */
void
fn_CTerm_mouseDisabled(WrenVM *vm)
{
	bool v = false;
	if (cterm != NULL && cterm->mouse_state_change_cbdata != NULL) {
		struct mouse_state *ms = cterm->mouse_state_change_cbdata;
		v = (ms->flags & MS_FLAGS_DISABLED) != 0;
	}
	wrenSetSlotBool(vm, 0, v);
}

/* CTerm.mouseDisabled = b — toggle the MS_FLAGS_DISABLED bit on the
 * live mouse_state.  Mirrors what the old C Alt-O handler did before
 * delegating to setup_mouse_events.  Caller is expected to follow up
 * with Input.setupMouseEvents() so the conio backend reconfigures
 * which events it reports.  No-op when no mouse_state is bound. */
void
fn_CTerm_mouseDisabled_set(WrenVM *vm)
{
	if (cterm == NULL || cterm->mouse_state_change_cbdata == NULL)
		return;
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_BOOL)
		return;
	struct mouse_state *ms = cterm->mouse_state_change_cbdata;
	if (wrenGetSlotBool(vm, 1))
		ms->flags |= MS_FLAGS_DISABLED;
	else
		ms->flags &= ~MS_FLAGS_DISABLED;
}

/* CTerm.throttleSpeed — current network character-pacing rate in BPS,
 * or 0 when throttling is off (the "no cap" floor of the rates
 * ladder).  For serial connections this is always 0; the configured
 * port rate lives in BBS.bpsRate.  Read by the default status bar
 * to render the live throttled rate; written via the up/down
 * actions below. */
void
fn_CTerm_throttleSpeed(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	int v = (st != NULL && st->speed != NULL) ? *st->speed : 0;
	wrenSetSlotDouble(vm, 0, (double)v);
}

/* Helper: walk the doterm()-bound speed up or down the rates[] ladder
 * (bbslist.c).  rates[] is a sorted ascending list ending in a
 * sentinel 0; in this API the sentinel doubles as "unthrottled" so
 * both directions cycle through it: stepping up from the top entry
 * (115200) lands on 0; stepping down from 0 wraps to the top.
 * No-op for serial connections to mirror the historical Alt-Up /
 * Alt-Down C handler, which silently ignored those keys when the
 * port speed was the only meaningful rate. */
static void
throttle_step_(int step)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->speed == NULL || st->bbs == NULL)
		return;
	int ct = st->bbs->conn_type;
	if (ct == CONN_TYPE_SERIAL || ct == CONN_TYPE_SERIAL_NORTS)
		return;
	int cur = *st->speed;
	if (step > 0) {
		/* Match the C original (term.c): from 0 jump to rates[0],
		 * from any other rate index forward by one (the sentinel
		 * 0 entry past 115200 is what disables throttling again). */
		*st->speed = (cur == 0) ? rates[0]
		    : rates[get_rate_num(cur) + 1];
	}
	else {
		/* get_rate_num(0) returns the sentinel index, so stepping
		 * down from 0 gives rates[sentinel - 1] = 115200 — the
		 * wrap the C handler also produced. */
		int idx = get_rate_num(cur);
		*st->speed = (idx == 0) ? 0 : rates[idx - 1];
	}
}

void
fn_CTerm_throttleSpeedUp(WrenVM *vm)
{
	(void)vm;
	throttle_step_(+1);
}

void
fn_CTerm_throttleSpeedDown(WrenVM *vm)
{
	(void)vm;
	throttle_step_(-1);
}

/* CTerm.throttleSpeed = bps — set the throttled output rate to a
 * specific value.  Pass 0 to disable throttling.  Values are taken
 * verbatim — the syncmenu supplies one of the entries from
 * Host.outputRates so no clamping is needed; scripts that hand-pick
 * are trusted to know what they want. */
void
fn_CTerm_throttleSpeed_set(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->speed == NULL || st->bbs == NULL)
		return;
	int ct = st->bbs->conn_type;
	if (ct == CONN_TYPE_SERIAL || ct == CONN_TYPE_SERIAL_NORTS)
		return;
	*st->speed = (int)wrenGetSlotDouble(vm, 1);
}

/* Input.setupMouseEvents() — reconfigure ciolib's reported mouse
 * events to match the active mouse_state (MM_OFF disables; tracking
 * modes register the appropriate button/move events).  Plus an
 * unconditional showmouse() so the cursor surfaces.  Wraps what the
 * old C Alt-O handler did after toggling the disabled flag. */
void
fn_Input_setupMouseEvents(WrenVM *vm)
{
	(void)vm;
	if (cterm == NULL || cterm->mouse_state_change_cbdata == NULL)
		return;
	setup_mouse_events(cterm->mouse_state_change_cbdata);
	showmouse();
}

/* BBS.elapsedSeconds — wall-clock seconds since the connect handshake
 * completed (clamped to >= 0).  Avoids exposing fast_connected and
 * forcing every script to subtract a "now" timestamp themselves. */
void
fn_BBS_elapsedSeconds(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	int64_t e = 0;
	if (st != NULL && st->bbs != NULL) {
		int64_t now = xp_fast_timer64();
		e = now - st->bbs->fast_connected;
		if (e < 0)
			e = 0;
	}
	wrenSetSlotDouble(vm, 0, (double)e);
}

/* BBS.connTypeName — display string for the active connection type
 * ("Telnet", "SSH", "Serial", "RLogin Reversed", …).  Convenience
 * over the BBS.connType integer enum. */
void
fn_BBS_connTypeName(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	const char *s = "";
	if (st != NULL && st->bbs != NULL) {
		int t = st->bbs->conn_type;
		if (t >= 0 && conn_types[t] != NULL)
			s = conn_types[t];
	}
	wrenSetSlotString(vm, 0, s);
}

/* Host.safeMode — true when SyncTERM was started with -s (safe mode):
 * no auto-connect, no SFTP key writes, no script-driven external
 * commands.  Default status bar surfaces "(SAFE)". */
void
fn_Host_safeMode(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, safe_mode != 0);
}

/* Host.textTerminal — true when the active ciolib backend is a text-
 * mode terminal (curses / curses-IBM / curses-ASCII / ANSI) rather
 * than a graphical / windowed mode (X / SDL / Wayland / GDI / ...).
 * Used by the default Ctrl-Q hook to decide whether to interpret the
 * keystroke as "hangup" — graphical modes pass it through to cterm
 * as a normal control character. */
void
fn_Host_textTerminal(WrenVM *vm)
{
	bool is_text = (cio_api.mode == CIOLIB_MODE_CURSES)
	    || (cio_api.mode == CIOLIB_MODE_CURSES_IBM)
	    || (cio_api.mode == CIOLIB_MODE_CURSES_ASCII)
	    || (cio_api.mode == CIOLIB_MODE_ANSI);
	wrenSetSlotBool(vm, 0, is_text);
}

/* Host.altKeyName — display name of the modifier that produces Alt
 * keycodes on this platform.  "Command" on macOS (Cmd-as-Alt mapping
 * in the Quartz backend), "Alt" everywhere else.  Backed by the
 * ALT_KEY_NAMEP macro in syncterm.h.  Used by Wren scripts for inline
 * menu / help labels so they read e.g. "Disconnect (Command-H)" on
 * Macs and "Disconnect (Alt-H)" elsewhere. */
void
fn_Host_altKeyName(WrenVM *vm)
{
	wrenSetSlotString(vm, 0, ALT_KEY_NAMEP);
}

/* Host.altKeyShort — three-letter abbreviation of the same modifier
 * for places where horizontal space is tight (status bar, etc.).
 * "CMD" on macOS, "ALT" elsewhere.  Backed by ALT_KEY_NAME3CH. */
void
fn_Host_altKeyShort(WrenVM *vm)
{
	wrenSetSlotString(vm, 0, ALT_KEY_NAME3CH);
}

/* Host.print(s) — write a string and a newline to standard output and
 * flush.  Distinct from System.print(s), which is captured by the Wren
 * console log buffer.  Intended for scripts run under the -W flag that
 * need to emit results / progress lines to the launching shell. */
void
fn_Host_print(WrenVM *vm)
{
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_STRING) {
		wren_throw(vm, "Host.print: argument must be a string");
		return;
	}
	int len = 0;
	const char *s = wrenGetSlotBytes(vm, 1, &len);
	fwrite(s, 1, (size_t)len, stdout);
	fputc('\n', stdout);
	fflush(stdout);
}

/* Host.launchScript — full path of the script supplied via the -W
 * command-line flag, or null when -W wasn't used.  Lets scripts that
 * are also embedded for normal Alt+key dispatch detect command-line
 * invocation and run themselves immediately, without relying on
 * coincidental signals like the BBS URL. */
void
fn_Host_launchScript(WrenVM *vm)
{
	const char *p = wren_host_launch_script();
	if (p == NULL)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotString(vm, 0, p);
}

/* Host.haveOOII — true when the build has Operation Overkill ][ tone
 * support compiled in (i.e. WITHOUT_OOII is NOT defined).  The online
 * menu uses this to decide whether to include the "Toggle OOII" entry. */
void
fn_Host_haveOOII(WrenVM *vm)
{
#ifdef WITHOUT_OOII
	wrenSetSlotBool(vm, 0, false);
#else
	wrenSetSlotBool(vm, 0, true);
#endif
}

/* Host.maxOOIIMode — highest valid Operation Overkill ][ mode value.
 * Returns 0 in builds without OOII support.  Used by the online menu's
 * "Toggle OOII" entry to wrap the increment back to 0 once it exceeds
 * the cap, mirroring the historical SM_OOII cycle. */
void
fn_Host_maxOOIIMode(WrenVM *vm)
{
#ifdef WITHOUT_OOII
	wrenSetSlotDouble(vm, 0, 0.0);
#else
	wrenSetSlotDouble(vm, 0, (double)MAX_OOII_MODE);
#endif
}

/* Host.outputRates / Host.outputRateNames — display labels and BPS
 * values for the throttled-output ladder, mirroring bbslist.c's
 * `rate_names[]` / `rates[]`.  Indexes line up: entry N in `Names`
 * is the human label for the BPS value at the same index in `Rates`.
 * The trailing 0 entry means "no cap" (display label "Current").
 * Used by the online-menu's Output Rate picker — pass the chosen
 * rate to `CTerm.throttleSpeed=`. */
void
fn_Host_outputRates(WrenVM *vm)
{
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
	for (int i = 0; rate_names[i] != NULL; i++) {
		wrenSetSlotDouble(vm, 1, (double)rates[i]);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

void
fn_Host_outputRateNames(WrenVM *vm)
{
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
	for (int i = 0; rate_names[i] != NULL; i++) {
		wrenSetSlotString(vm, 1, rate_names[i]);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

/* Host.logLevel — current xfer log threshold (0 = Emergency, …,
 * 7 = Debug).  Backs the online-menu's Log Level picker. */
void
fn_Host_logLevel(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)log_level);
}

void
fn_Host_logLevel_set(WrenVM *vm)
{
	int n = (int)wrenGetSlotDouble(vm, 1);
	if (n < 0)
		n = 0;
	if (n > 7)
		n = 7;
	log_level = n;
}

void
fn_Host_logLevelNames(WrenVM *vm)
{
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
	for (int i = 0; log_levels[i] != NULL; i++) {
		wrenSetSlotString(vm, 1, log_levels[i]);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

/* Host.editBBSList() — open the bbslist editor over the active
 * connection (UIFC-driven).  Thin shim around show_bbslist + the
 * surrounding screen-save / settitle / uifcbail dance the C-side
 * Alt-E case used to do.  Routes the online-menu's
 * "Edit Dialing Directory" entry through here. */
void
fn_Host_editBBSList(WrenVM *vm)
{
	(void)vm;
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->bbs == NULL)
		return;
	struct ciolib_screen *savscrn = cp437_savescrn();
	show_bbslist(st->bbs->name, true);
	char title[LIST_NAME_MAX + 13];
	sprintf(title, "SyncTERM - %s\n", st->bbs->name);
	settitle(title);
	uifcbail();
	restorescreen(savscrn);
	freescreen(savscrn);
}

/* Host.logUnread / Host.logUnreadError — true while the Wren console
 * has unread output since the user last visited it.  `_error` is the
 * subset that includes only compile errors, runtime errors, and stack
 * frames (the status bar tints the bug indicator red when set,
 * yellow otherwise).  Cleared by Console.markSeen() / opening the
 * REPL pane. */
void
fn_Host_logUnread(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, wren_host_log_unread());
}

void
fn_Host_logUnreadError(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, wren_host_log_unread_error());
}

/* Host.musicNames — display strings for the ANSI music modes, in the
 * order the MusicMode enum uses (off, ESC[| only, BANSI style, all
 * music).  Returned as a Wren List<String> built fresh on each
 * call.  Mirrors `music_names[]` from bbslist.c so the bbslist
 * editor and a Wren-side music picker stay aligned without hard-
 * coding labels in the script. */
void
fn_Host_musicNames(WrenVM *vm)
{
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
	for (int i = 0; music_names[i] != NULL; i++) {
		wrenSetSlotString(vm, 1, music_names[i]);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

/* Host.musicHelp — the multi-line help blurb shown by the Wren-side
 * music picker.  Markdown variant of bbslist.c's `music_helpbuf`;
 * the uifc Setup dialog uses the backtick-marked-up version
 * directly, so we don't run a converter at runtime. */
void
fn_Host_musicHelp(WrenVM *vm)
{
	wrenSetSlotString(vm, 0,
	    "# ANSI Music Setup\n"
	    "\n"
	    "ESC [ | only\n"
	    ":  Only `CSI |` (SyncTERM) ANSI music is supported.  "
	    "Enables Delete Line.\n"
	    "BANSI Style\n"
	    ":  Also enables BANSI-Style (`CSI N`).  Enables Delete Line.\n"
	    "All ANSI Music Enabled\n"
	    ":  Enables both `CSI M` and `CSI N` ANSI music.  "
	    "Delete Line is disabled.\n"
	    "\n"
	    "So-Called ANSI Music has a long and troubled history.  Although the "
	    "original ANSI standard has well defined ways to provide private "
	    "extensions to the spec, none of these methods were used.  Instead, "
	    "so-called ANSI music replaced the Delete Line ANSI sequence.  Many "
	    "full-screen editors use DL, and to this day, some programs (such as "
	    "BitchX) require it to run.\n"
	    "\n"
	    "To deal with this, BananaCom decided to use what *they* thought was an "
	    "unspecified escape code `ESC[N` for ANSI music.  Unfortunately, this "
	    "is broken also.  Although rarely implemented in BBS clients, `ESC[N` "
	    "is the erase field sequence.\n"
	    "\n"
	    "SyncTERM has now defined a third ANSI music sequence which **is** legal "
	    "according to the ANSI spec.  Specifically `ESC[|`.");
}

/* ----- Status (Wren-driven status bar) ----------------------------- */

/* Status.callable — getter returns the currently installed Fn (or null
 * when no script has set one).  C calls it from wren_status_render(). */
void
fn_Status_callable_get(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->status_callable == NULL)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotHandle(vm, 0, st->status_callable);
}

/* Status.callable=(fn) — install (or clear) the status-bar render
 * function.  Pass null to detach.  Releases any previously installed
 * handle so VMs can swap implementations cleanly without leaking. */
void
fn_Status_callable_set(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL)
		return;
	if (st->status_callable != NULL) {
		wrenReleaseHandle(vm, st->status_callable);
		st->status_callable = NULL;
	}
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_NULL)
		st->status_callable = wrenGetSlotHandle(vm, 1);
}

/* Status.enabled — true while the SyncTERM-managed status row is
 * active.  False when DECSSDT (or nostatus startup config) has hidden
 * it; the script's render Fn should early-return then. */
void
fn_Status_enabled(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, !term.nostatus);
}

/* ----- ExtAttr / LastColumnFlag (CTerm bitfield snapshots) --------- */

struct wren_extattr {
	enum syncterm_wren_foreign type;
	uint32_t value;
};

struct wren_last_column_flag {
	enum syncterm_wren_foreign type;
	uint32_t value;
};

void
wren_extattr_allocate(WrenVM *vm)
{
	struct wren_extattr *e = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*e));
	e->type  = SWF_EXTATTR;
	e->value = 0;
}

void
wren_extattr_finalize(void *data)
{
	(void)data;
}

void
wren_last_column_flag_allocate(WrenVM *vm)
{
	struct wren_last_column_flag *l = wrenSetSlotNewForeign(vm, 0, 0,
	    sizeof(*l));
	l->type  = SWF_LAST_COLUMN_FLAG;
	l->value = 0;
}

void
wren_last_column_flag_finalize(void *data)
{
	(void)data;
}

#define EXTATTR_BOOL(name, flag)                                        \
	void fn_ExtAttr_##name(WrenVM *vm)                       \
	{                                                               \
		struct wren_extattr *e = wrenGetSlotForeign(vm, 0);     \
		wrenSetSlotBool(vm, 0, (e->value & (flag)) != 0);       \
	}

EXTATTR_BOOL(autoWrap,            CTERM_EXTATTR_AUTOWRAP)
EXTATTR_BOOL(originMode,          CTERM_EXTATTR_ORIGINMODE)
EXTATTR_BOOL(sxScroll,            CTERM_EXTATTR_SXSCROLL)
EXTATTR_BOOL(decLrmm,             CTERM_EXTATTR_DECLRMM)
EXTATTR_BOOL(bracketPaste,        CTERM_EXTATTR_BRACKETPASTE)
EXTATTR_BOOL(decBkm,              CTERM_EXTATTR_DECBKM)
EXTATTR_BOOL(prestelMosaic,       CTERM_EXTATTR_PRESTEL_MOSAIC)
EXTATTR_BOOL(prestelDoubleHeight, CTERM_EXTATTR_PRESTEL_DOUBLE_HEIGHT)
EXTATTR_BOOL(prestelConceal,      CTERM_EXTATTR_PRESTEL_CONCEAL)
EXTATTR_BOOL(prestelSeparated,    CTERM_EXTATTR_PRESTEL_SEPARATED)
EXTATTR_BOOL(prestelHold,         CTERM_EXTATTR_PRESTEL_HOLD)
EXTATTR_BOOL(alternateKeypad,     CTERM_EXTATTR_ALTERNATE_KEYPAD)

#define LCF_BOOL(name, flag)                                            \
	void fn_LCF_##name(WrenVM *vm)                           \
	{                                                               \
		struct wren_last_column_flag *l = wrenGetSlotForeign(vm, 0); \
		wrenSetSlotBool(vm, 0, (l->value & (flag)) != 0);       \
	}

LCF_BOOL(set,     CTERM_LCF_SET)
LCF_BOOL(enabled, CTERM_LCF_ENABLED)
LCF_BOOL(forced,  CTERM_LCF_FORCED)

void
fn_CTerm_extAttr(WrenVM *vm)
{
	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "syncterm", "ExtAttr", 1);
	struct wren_extattr *e = wrenSetSlotNewForeign(vm, 0, 1, sizeof(*e));
	e->type  = SWF_EXTATTR;
	e->value = (cterm != NULL) ? cterm->extattr : 0;
}

void
fn_CTerm_lastColumnFlag(WrenVM *vm)
{
	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "syncterm", "LastColumnFlag", 1);
	struct wren_last_column_flag *l = wrenSetSlotNewForeign(vm, 0, 1,
	    sizeof(*l));
	l->type  = SWF_LAST_COLUMN_FLAG;
	l->value = (cterm != NULL) ? cterm->last_column_flag : 0;
}

