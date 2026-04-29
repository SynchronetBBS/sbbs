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

#include "ciolib.h"
#include "comio.h"     /* COM_FLOW_CONTROL_* — also used by FlowControl in wren_bind.c */
#include "conn.h"
#include "cterm.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

extern struct cterminal *cterm;

/* ----- Conn -------------------------------------------------------- */

void
fn_Conn_send(WrenVM *vm)
{
	int len = 0;
	const char *s = wrenGetSlotBytes(vm, 1, &len);
	if (len <= 0)
		return;
	conn_send(s, (size_t)len, 0);
}

void
fn_Conn_sendRaw(WrenVM *vm)
{
	int len = 0;
	const char *s = wrenGetSlotBytes(vm, 1, &len);
	if (len <= 0)
		return;
	conn_send_raw(s, (size_t)len, 0);
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

