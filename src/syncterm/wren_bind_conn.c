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
	if (wrenGetSlotType(vm, 1) == WREN_TYPE_BOOL)
		sftp_active_flag = wrenGetSlotBool(vm, 1);
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

void
fn_CTerm_logMode(WrenVM *vm)
{
	int v = (cterm != NULL) ? (cterm->log & CTERM_LOG_MASK) : 0;
	wrenSetSlotDouble(vm, 0, (double)v);
}

void
fn_CTerm_logPaused(WrenVM *vm)
{
	bool v = (cterm != NULL) && ((cterm->log & CTERM_LOG_PAUSED) != 0);
	wrenSetSlotBool(vm, 0, v);
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

/* Host.musicHelp — the multi-line help blurb shown by the bbslist
 * editor's music-mode picker.  Same source as the Setup dialog so a
 * Wren-side picker doesn't drift. */
void
fn_Host_musicHelp(WrenVM *vm)
{
	wrenSetSlotString(vm, 0, music_helpbuf);
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

