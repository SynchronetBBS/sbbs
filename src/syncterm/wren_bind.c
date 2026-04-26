/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Foreign-method bindings for the Wren scripting host.  These wrap
 * SyncTERM's conio / connection / cterm / bbslist / cache APIs and
 * expose hook-registration entry points; lifecycle and dispatch live
 * in wren_host.c. */

#include "wren_host_internal.h"
#include "wren_host.h"
#include "wren.h"
/* REPL.compile_ uses wrenCompileSource to produce an ObjClosure that
 * the Wren-side REPL.eval invokes via .call().  That path is internal
 * to Wren — wrenCompileSource isn't exposed in wren.h — so pull in
 * the VM headers directly.  Mirrors what wren_opt_meta.c does. */
#include "wren/vm/wren_vm.h"
#include "wren/vm/wren_value.h"

#include "ciolib.h"
#include "cterm.h"
#include "syncterm.h"
#include "bbslist.h"
#include "conn.h"
#include "term.h"
#include "utf8_codepages.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* term.c globals — emulation state and current bbs context. */
extern struct cterminal *cterm;

/* ----- Screen / Input -------------------------------------------- */

/* Screen.supports.* — backend capability flags.  Each getter is one
 * bit-test against cio_api.options.  Macro to keep the boilerplate
 * out of the way. */
#define SCREEN_SUPPORTS(name, flag)                                  \
	static void fnScreenSupports_##name(WrenVM *vm)             \
	{                                                            \
		wrenSetSlotBool(vm, 0, (cio_api.options & (flag)) != 0); \
	}

SCREEN_SUPPORTS(loadableFonts,    CONIO_OPT_LOADABLE_FONTS)
SCREEN_SUPPORTS(altBlinkFont,     CONIO_OPT_BLINK_ALT_FONT)
SCREEN_SUPPORTS(altBoldFont,      CONIO_OPT_BOLD_ALT_FONT)
SCREEN_SUPPORTS(brightBackground, CONIO_OPT_BRIGHT_BACKGROUND)
SCREEN_SUPPORTS(paletteChange,    CONIO_OPT_PALETTE_SETTING)
SCREEN_SUPPORTS(pixels,           CONIO_OPT_SET_PIXEL)
SCREEN_SUPPORTS(customCursor,     CONIO_OPT_CUSTOM_CURSOR)
SCREEN_SUPPORTS(fontSelection,    CONIO_OPT_FONT_SELECT)
SCREEN_SUPPORTS(windowTitle,      CONIO_OPT_SET_TITLE)
SCREEN_SUPPORTS(windowName,       CONIO_OPT_SET_NAME)
SCREEN_SUPPORTS(windowIcon,       CONIO_OPT_SET_ICON)
SCREEN_SUPPORTS(extendedPalette,  CONIO_OPT_EXTENDED_PALETTE)
SCREEN_SUPPORTS(blockyScaling,    CONIO_OPT_BLOCKY_SCALING)
SCREEN_SUPPORTS(externalScaling,  CONIO_OPT_EXTERNAL_SCALING)
SCREEN_SUPPORTS(closeLock,        CONIO_OPT_DISABLE_CLOSE)

#undef SCREEN_SUPPORTS

/* Window bounds [sx, sy, ex, ey] — 1-based, inclusive.  Setter
 * delegates to ciolib_window which also resets the cursor to (1,1)
 * within the new window. */
static void
fnScreenWindow_bounds(WrenVM *vm)
{
	struct text_info ti;
	gettextinfo(&ti);
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
	const double v[4] = {
		(double)ti.winleft, (double)ti.wintop,
		(double)ti.winright, (double)ti.winbottom
	};
	for (int i = 0; i < 4; i++) {
		wrenSetSlotDouble(vm, 1, v[i]);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

static void
fnScreenWindow_bounds_set(WrenVM *vm)
{
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_LIST ||
	    wrenGetListCount(vm, 1) < 4)
		return;
	wrenEnsureSlots(vm, 3);
	int box[4];
	for (int i = 0; i < 4; i++) {
		wrenGetListElement(vm, 1, i, 2);
		box[i] = (int)wrenGetSlotDouble(vm, 2);
	}
	window(box[0], box[1], box[2], box[3]);
}

static void
fnScreenWindow_putChar(WrenVM *vm)
{
	int c = (int)wrenGetSlotDouble(vm, 1);
	putch(c);
}

static void
fnScreenWindow_print(WrenVM *vm)
{
	int len = 0;
	const char *s = wrenGetSlotBytes(vm, 1, &len);
	for (int i = 0; i < len; i++)
		putch((unsigned char)s[i]);
}

/* Cursor position as a single [x, y] list — getter and setter
 * symmetric, no separate moveTo / cursorX / cursorY calls. */
static void
fnScreenWindow_position(WrenVM *vm)
{
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
	wrenSetSlotDouble(vm, 1, (double)wherex());
	wrenInsertInList(vm, 0, -1, 1);
	wrenSetSlotDouble(vm, 1, (double)wherey());
	wrenInsertInList(vm, 0, -1, 1);
}

static void
fnScreenWindow_position_set(WrenVM *vm)
{
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_LIST ||
	    wrenGetListCount(vm, 1) < 2)
		return;
	wrenEnsureSlots(vm, 3);
	wrenGetListElement(vm, 1, 0, 2);
	int x = (int)wrenGetSlotDouble(vm, 2);
	wrenGetListElement(vm, 1, 1, 2);
	int y = (int)wrenGetSlotDouble(vm, 2);
	gotoxy(x, y);
}

static void
fn_Screen_attr_set(WrenVM *vm)
{
	int a = (int)wrenGetSlotDouble(vm, 1);
	textattr(a);
}

static void
fn_Screen_size(WrenVM *vm)
{
	struct text_info ti;
	gettextinfo(&ti);
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
	wrenSetSlotDouble(vm, 1, (double)ti.screenwidth);
	wrenInsertInList(vm, 0, -1, 1);
	wrenSetSlotDouble(vm, 1, (double)ti.screenheight);
	wrenInsertInList(vm, 0, -1, 1);
}

/* Forward decl — defined later beside the Cell helpers. */
static int encode_utf8(uint32_t cp, char out[4]);

/* Foreign class state for KeyEvent and MouseEvent — both are simple
 * value types (no parent, no resources) so finalize is a no-op. */
struct wren_key_event {
	uint16_t code;
	int32_t  codepoint;   /* -1 = none (extended key) */
	uint8_t  text[5];     /* UTF-8 of codepoint, NUL-terminated; "" if extended */
	uint8_t  text_len;    /* not counting NUL */
};

struct wren_mouse_event {
	struct mouse_event ev;
};

/* Read one fully-assembled 16-bit ciolib key (handles 0x00 / 0xe0 high-
 * byte sequences, collapses LITERAL_E0 to plain 0xe0).  Returns the
 * 16-bit value as int.  Mirrors what fn_Input_getch did before. */
static int
read_assembled_keycode(void)
{
	int ch = getch();
	if (ch == 0 || ch == 0xe0) {
		int ch2 = getch();
		ch |= ch2 << 8;
		if (ch == CIO_KEY_LITERAL_E0)
			ch = 0xe0;
	}
	return ch;
}

/* Populate a wren_key_event from a raw 16-bit code: normalize the
 * scancode-flavored Esc to plain Esc, then derive codepoint+text from
 * the low byte if the high byte is zero (i.e. a non-extended key).
 * Codepoint comes from the current Font[0] codepage so the byte→Unicode
 * translation matches what the screen would render for that byte. */
static void
key_event_init(struct wren_key_event *ke, uint16_t code)
{
	if (code == CIO_KEY_ABORTED)
		code = 0x001B;
	ke->code      = code;
	ke->codepoint = -1;
	ke->text[0]   = '\0';
	ke->text_len  = 0;
	if ((code & 0xFF00u) == 0) {
		uint8_t  byte = (uint8_t)(code & 0xFFu);
		int      f    = getfont(0);
		uint32_t cp;
		if (f >= 0 && f <= 256)
			cp = cpoint_from_cpchar(
			    (enum ciolib_codepage)conio_fontdata[f].cp, byte);
		else
			cp = byte;
		ke->codepoint = (int32_t)cp;
		char     buf[4];
		int      n = encode_utf8(cp, buf);
		for (int i = 0; i < n; i++)
			ke->text[i] = (uint8_t)buf[i];
		ke->text[n]  = '\0';
		ke->text_len = (uint8_t)n;
	}
}

static void
wren_key_event_allocate(WrenVM *vm)
{
	struct wren_key_event *ke =
	    wrenSetSlotNewForeign(vm, 0, 0, sizeof(*ke));
	uint16_t code = (uint16_t)wrenGetSlotDouble(vm, 1);
	key_event_init(ke, code);
}

static void
wren_key_event_finalize(void *data)
{
	(void)data;
}

static void
wren_mouse_event_allocate(WrenVM *vm)
{
	struct wren_mouse_event *me =
	    wrenSetSlotNewForeign(vm, 0, 0, sizeof(*me));
	memset(me, 0, sizeof(*me));
	me->ev.event       = (int)wrenGetSlotDouble(vm, 1);
	me->ev.kbmodifiers = (int)wrenGetSlotDouble(vm, 2);
	me->ev.startx      = (int)wrenGetSlotDouble(vm, 3);
	me->ev.starty      = (int)wrenGetSlotDouble(vm, 4);
	me->ev.endx        = (int)wrenGetSlotDouble(vm, 5);
	me->ev.endy        = (int)wrenGetSlotDouble(vm, 6);
}

static void
wren_mouse_event_finalize(void *data)
{
	(void)data;
}

/* KeyEvent field accessors. */
static void
fn_KeyEvent_code(WrenVM *vm)
{
	struct wren_key_event *ke = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)ke->code);
}

static void
fn_KeyEvent_codepoint(WrenVM *vm)
{
	struct wren_key_event *ke = wrenGetSlotForeign(vm, 0);
	if (ke->codepoint < 0) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotDouble(vm, 0, (double)ke->codepoint);
}

static void
fn_KeyEvent_text(WrenVM *vm)
{
	struct wren_key_event *ke = wrenGetSlotForeign(vm, 0);
	wrenSetSlotBytes(vm, 0, (const char *)ke->text, ke->text_len);
}

/* MouseEvent field accessors. */
#define MOUSE_FIELD(NAME, FIELD)                                        \
	static void fn_MouseEvent_##NAME(WrenVM *vm)                    \
	{                                                               \
		struct wren_mouse_event *me = wrenGetSlotForeign(vm, 0);\
		wrenSetSlotDouble(vm, 0, (double)me->ev.FIELD);         \
	}

MOUSE_FIELD(event,     event)
MOUSE_FIELD(modifiers, kbmodifiers)
MOUSE_FIELD(startX,    startx)
MOUSE_FIELD(startY,    starty)
MOUSE_FIELD(endX,      endx)
MOUSE_FIELD(endY,      endy)

#undef MOUSE_FIELD

/* Build a Wren-side KeyEvent in slot 0 from a raw 16-bit code.  Uses
 * the captured KeyEvent class handle; callers must have at least 2
 * slots ensured (slot 0 = result, slot 1 = scratch for the class). */
static void
push_key_event(WrenVM *vm, uint16_t code)
{
	struct wren_host_state *st = wren_host_state();
	wrenSetSlotHandle(vm, 1, st->key_event_class);
	struct wren_key_event *ke =
	    wrenSetSlotNewForeign(vm, 0, 1, sizeof(*ke));
	key_event_init(ke, code);
}

/* Build a Wren-side MouseEvent in slot 0 from a struct mouse_event. */
static void
push_mouse_event(WrenVM *vm, const struct mouse_event *mev)
{
	struct wren_host_state *st = wren_host_state();
	wrenSetSlotHandle(vm, 1, st->mouse_event_class);
	struct wren_mouse_event *me =
	    wrenSetSlotNewForeign(vm, 0, 1, sizeof(*me));
	me->ev = *mev;
}

/* Drain one event from ciolib into slot 0.  When getch returns
 * CIO_KEY_MOUSE the actual mouse details come from a paired getmouse
 * call, so we wrap it as MouseEvent rather than KeyEvent.  Anything
 * else becomes a KeyEvent. */
static void
push_next_event(WrenVM *vm)
{
	int code = read_assembled_keycode();
	wrenEnsureSlots(vm, 2);
	if (code == CIO_KEY_MOUSE) {
		struct mouse_event mev;
		memset(&mev, 0, sizeof(mev));
		if (getmouse(&mev) != 0) {
			/* No mouse pending despite the marker — surface as a
			 * raw KeyEvent so the script at least sees the byte. */
			push_key_event(vm, (uint16_t)code);
			return;
		}
		push_mouse_event(vm, &mev);
		return;
	}
	push_key_event(vm, (uint16_t)code);
}

/* Input.next — block until something is ready, return the event. */
static void
fn_Input_next(WrenVM *vm)
{
	push_next_event(vm);
}

/* Input.next(ms) — wait up to ms milliseconds; null on timeout. */
static void
fn_Input_next_ms(WrenVM *vm)
{
	int ms = (int)wrenGetSlotDouble(vm, 1);
	if (kbwait(ms) == 0) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	push_next_event(vm);
}

/* Input.poll — non-blocking; null when nothing is ready. */
static void
fn_Input_poll(WrenVM *vm)
{
	if (!kbhit()) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	push_next_event(vm);
}

/* Input.ungetKey_(ev) / Input.ungetMouse_(ev) — typed unget primitives.
 * The public Input.unget(ev) is a Wren-side wrapper that picks one of
 * these via `ev is KeyEvent` / `ev is MouseEvent`, since Wren doesn't
 * expose a "class of this foreign value" API to C and we need to know
 * which queue to push back to.  ungetch handles the LITERAL_E0
 * reversal internally; ungetmouse appends to the mouse output queue
 * and the next getch will surface CIO_KEY_MOUSE on its own. */
static void
fn_Input_ungetKey_(WrenVM *vm)
{
	struct wren_key_event *ke = wrenGetSlotForeign(vm, 1);
	ungetch(ke->code);
}

static void
fn_Input_ungetMouse_(WrenVM *vm)
{
	struct wren_mouse_event *me = wrenGetSlotForeign(vm, 1);
	ungetmouse(&me->ev);
}

static void
fnScreenWindow_clear(WrenVM *vm)
{
	(void)vm;
	clrscr();
}

static void
fnScreenWindow_clearToLineEnd(WrenVM *vm)
{
	(void)vm;
	clreol();
}

/* ciolib_savescreen returns a malloc'd struct ciolib_screen *.
 * Round-trip the pointer through a Wren Num — pointers are typically
 * aligned and fit in the 53-bit double mantissa on every supported
 * host.  restore() frees the screen on success. */
static void
fn_Screen_save(WrenVM *vm)
{
	struct ciolib_screen *scrn = savescreen();
	wrenSetSlotDouble(vm, 0, (double)(uintptr_t)scrn);
}

static void
fn_Screen_restore(WrenVM *vm)
{
	uintptr_t v = (uintptr_t)wrenGetSlotDouble(vm, 1);
	struct ciolib_screen *scrn = (struct ciolib_screen *)v;
	if (scrn == NULL)
		return;
	restorescreen(scrn);
	freescreen(scrn);
}

/* ----- Conn -------------------------------------------------------- */

static void
fn_Conn_send(WrenVM *vm)
{
	int len = 0;
	const char *s = wrenGetSlotBytes(vm, 1, &len);
	/* Use raw variant so onOutput hooks don't recurse when a script
	 * sends from inside its own hook. */
	if (len > 0)
		conn_send_raw(s, (size_t)len, 0);
}

static void
fn_Conn_sendRaw(WrenVM *vm)
{
	int len = 0;
	const char *s = wrenGetSlotBytes(vm, 1, &len);
	if (len > 0)
		conn_send_raw(s, (size_t)len, 0);
}

static void
fn_Conn_close(WrenVM *vm)
{
	(void)vm;
	conn_close();
}

static void
fn_Conn_connected(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, conn_connected());
}

static void
fn_Conn_type(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	int t = (st != NULL && st->bbs != NULL) ? st->bbs->conn_type : 0;
	wrenSetSlotDouble(vm, 0, (double)t);
}

static void
fn_Conn_inbufBytes(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)conn_buf_bytes(&conn_inbuf));
}

static void
fn_Conn_outbufBytes(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)conn_buf_bytes(&conn_outbuf));
}

static void
fn_Conn_inbufPeek(WrenVM *vm)
{
	int n = (int)wrenGetSlotDouble(vm, 1);
	if (n <= 0) {
		wrenSetSlotBytes(vm, 0, "", 0);
		return;
	}
	if (n > 65536)
		n = 65536;
	char *tmp = malloc((size_t)n);
	if (tmp == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	size_t got = conn_buf_peek(&conn_inbuf, tmp, (size_t)n);
	wrenSetSlotBytes(vm, 0, tmp, got);
	free(tmp);
}

static void
fn_Conn_inbufGet(WrenVM *vm)
{
	int n = (int)wrenGetSlotDouble(vm, 1);
	if (n <= 0) {
		wrenSetSlotBytes(vm, 0, "", 0);
		return;
	}
	if (n > 65536)
		n = 65536;
	char *tmp = malloc((size_t)n);
	if (tmp == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	size_t got = conn_buf_get(&conn_inbuf, tmp, (size_t)n);
	wrenSetSlotBytes(vm, 0, tmp, got);
	free(tmp);
}

static void
fn_Conn_outbufPut(WrenVM *vm)
{
	int len = 0;
	const char *s = wrenGetSlotBytes(vm, 1, &len);
	size_t put = 0;
	if (len > 0)
		put = conn_buf_put(&conn_outbuf, s, (size_t)len);
	wrenSetSlotDouble(vm, 0, (double)put);
}

/* ----- CTerm ------------------------------------------------------- */

static void
fn_CTerm_emulation(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, cterm != NULL ? (double)cterm->emulation : 0.0);
}

static void
fn_CTerm_x(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, cterm != NULL ? (double)cterm->xpos : 0.0);
}

static void
fn_CTerm_y(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, cterm != NULL ? (double)cterm->ypos : 0.0);
}

static void
fn_CTerm_attr(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, cterm != NULL ? (double)cterm->attr : 0.0);
}

static void
fn_CTerm_doorwayMode(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0,
	    cterm != NULL ? (double)cterm->doorway_mode : 0.0);
}

static void
fn_CTerm_music(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0,
	    cterm != NULL ? (double)cterm->music_enable : 0.0);
}

static void
fn_CTerm_width(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, cterm != NULL ? (double)cterm->width : 0.0);
}

static void
fn_CTerm_height(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, cterm != NULL ? (double)cterm->height : 0.0);
}

static void
fn_CTerm_write(WrenVM *vm)
{
	int len = 0;
	const char *s = wrenGetSlotBytes(vm, 1, &len);
	int speed = 0;
	if (cterm != NULL && len > 0)
		cterm_write(cterm, s, len, &speed);
}

/* ----- BBS --------------------------------------------------------- */

#define BBS_FIELD_STR(setter)                                           \
	do {                                                            \
		struct wren_host_state *st = wren_host_state();         \
		if (st != NULL && st->bbs != NULL)                      \
			wrenSetSlotString(vm, 0, st->bbs->setter);      \
		else                                                    \
			wrenSetSlotString(vm, 0, "");                   \
	} while (0)

#define BBS_FIELD_NUM(field)                                            \
	do {                                                            \
		struct wren_host_state *st = wren_host_state();         \
		double v = (st != NULL && st->bbs != NULL)              \
		    ? (double)st->bbs->field : 0.0;                     \
		wrenSetSlotDouble(vm, 0, v);                            \
	} while (0)

static void fn_BBS_name(WrenVM *vm)     { BBS_FIELD_STR(name); }
static void fn_BBS_addr(WrenVM *vm)     { BBS_FIELD_STR(addr); }
static void fn_BBS_port(WrenVM *vm)     { BBS_FIELD_NUM(port); }
static void fn_BBS_connType(WrenVM *vm) { BBS_FIELD_NUM(conn_type); }
static void fn_BBS_user(WrenVM *vm)     { BBS_FIELD_STR(user); }
static void fn_BBS_password(WrenVM *vm) { BBS_FIELD_STR(password); }
static void fn_BBS_syspass(WrenVM *vm)  { BBS_FIELD_STR(syspass); }
static void fn_BBS_music(WrenVM *vm)    { BBS_FIELD_NUM(music); }
static void fn_BBS_rip(WrenVM *vm)      { BBS_FIELD_NUM(rip); }
static void fn_BBS_comment(WrenVM *vm)  { BBS_FIELD_STR(comment); }

/* ----- Cache ------------------------------------------------------- */

static void
fn_Cache_basePath(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	char fn[MAX_PATH + 1] = "";
	if (st != NULL && st->bbs != NULL)
		get_cache_fn_base(st->bbs, fn, sizeof(fn));
	wrenSetSlotString(vm, 0, fn);
}

static void
fn_Cache_subdirPath(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	const char *sub = wrenGetSlotString(vm, 1);
	char fn[MAX_PATH + 1] = "";
	if (st != NULL && st->bbs != NULL && sub != NULL)
		get_cache_fn_subdir(st->bbs, fn, sizeof(fn), sub);
	wrenSetSlotString(vm, 0, fn);
}

static void
fn_Cache_readFile(WrenVM *vm)
{
	const char *name = wrenGetSlotString(vm, 1);
	struct wren_host_state *st = wren_host_state();
	if (name == NULL || st == NULL || st->bbs == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	char base[MAX_PATH + 1];
	get_cache_fn_base(st->bbs, base, sizeof(base));
	char path[MAX_PATH + 1];
	snprintf(path, sizeof(path), "%s%s", base, name);
	FILE *f = fopen(path, "rb");
	if (f == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz < 0 || sz > 16 * 1024 * 1024) {
		fclose(f);
		wrenSetSlotNull(vm, 0);
		return;
	}
	char *buf = malloc((size_t)sz);
	if (buf == NULL) {
		fclose(f);
		wrenSetSlotNull(vm, 0);
		return;
	}
	size_t got = fread(buf, 1, (size_t)sz, f);
	fclose(f);
	wrenSetSlotBytes(vm, 0, buf, got);
	free(buf);
}

static void
fn_Cache_writeFile(WrenVM *vm)
{
	const char *name = wrenGetSlotString(vm, 1);
	int len = 0;
	const char *contents = wrenGetSlotBytes(vm, 2, &len);
	struct wren_host_state *st = wren_host_state();
	if (name == NULL || contents == NULL || st == NULL || st->bbs == NULL) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	char base[MAX_PATH + 1];
	get_cache_fn_base(st->bbs, base, sizeof(base));
	char path[MAX_PATH + 1];
	snprintf(path, sizeof(path), "%s%s", base, name);
	FILE *f = fopen(path, "wb");
	if (f == NULL) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	size_t put = fwrite(contents, 1, (size_t)len, f);
	fclose(f);
	wrenSetSlotBool(vm, 0, put == (size_t)len);
}

/* ----- Console (log buffer accessor) ------------------------------ */

static void
fn_Console_count(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)wren_log_count());
}

static void
fn_Console_total(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)wren_log_total());
}

/* `Console[seq]` — entry by sequence number.  Returns null when seq
 * is outside the currently-buffered range (already evicted, or not
 * yet written). */
static void
fn_Console_subscript(WrenVM *vm)
{
	double d = wrenGetSlotDouble(vm, 1);
	if (d < 0) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wren_log_emit(vm, (uint64_t)d, 0);
}

static void
fn_Console_clear(WrenVM *vm)
{
	(void)vm;
	wren_log_clear();
}

/* Wren iteration protocol over seq numbers: start at the oldest seq
 * still in the buffer, end at total-1 (inclusive).  Skipped if the
 * buffer is empty.  iteratorValue returns the entry at that seq. */
static void
fn_Console_iterate(WrenVM *vm)
{
	int      count = wren_log_count();
	uint64_t total = wren_log_total();
	if (count == 0) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	if (wrenGetSlotType(vm, 1) == WREN_TYPE_NULL) {
		wrenSetSlotDouble(vm, 0, (double)(total - (uint64_t)count));
		return;
	}
	uint64_t next = (uint64_t)wrenGetSlotDouble(vm, 1) + 1;
	if (next >= total)
		wrenSetSlotBool(vm, 0, false);
	else
		wrenSetSlotDouble(vm, 0, (double)next);
}

static void
fn_Console_iteratorValue(WrenVM *vm)
{
	uint64_t seq = (uint64_t)wrenGetSlotDouble(vm, 1);
	wren_log_emit(vm, seq, 0);
}

/* ----- Hook -------------------------------------------------------- */

static void
fn_Hook_onKey(WrenVM *vm)    { wren_host_register_hook(vm, WREN_HOOK_KEY,    1); }
static void
fn_Hook_onInput(WrenVM *vm)  { wren_host_register_hook(vm, WREN_HOOK_INPUT,  1); }
static void
fn_Hook_onOutput(WrenVM *vm) { wren_host_register_hook(vm, WREN_HOOK_OUTPUT, 1); }
static void
fn_Hook_onMouse(WrenVM *vm)  { wren_host_register_hook(vm, WREN_HOOK_MOUSE,  1); }
static void
fn_Hook_onStatus(WrenVM *vm) { wren_host_register_hook(vm, WREN_HOOK_STATUS, 1); }

static void
fn_Hook_every(WrenVM *vm)
{
	int ms = (int)wrenGetSlotDouble(vm, 1);
	wren_host_register_timer(vm, ms, 2);
}

/* ----- Cell / Cells / vmem text bindings --------------------------
 *
 * `Cell` wraps a single struct vmem_cell.  Two flavours: standalone
 * (Cell.new()) owns its own bytes; view (created by Cells.[i]) reads
 * and writes through to its parent Cells's buffer, with a Wren-level
 * pin on the parent so the buffer outlives the view.
 *
 * `Cells` owns a malloc'd buffer of vmem_cells returned by getText.
 * It is iterable and indexable but has no add/insert/clear bindings,
 * so scripts can mutate individual cells but not change the count.
 * -------------------------------------------------------------------- */

struct wren_cells {
	int               count;
	struct vmem_cell *buf;          /* malloc'd; freed at finalize */
};

struct wren_cell {
	struct vmem_cell   c;            /* used when standalone */
	struct wren_cells *parent;       /* NULL = standalone */
	int                index;        /* index into parent->buf in view mode */
	WrenHandle        *parent_handle;/* pin on parent for view mode */
};

static struct vmem_cell *
cell_data(struct wren_cell *wc)
{
	return (wc->parent != NULL) ? &wc->parent->buf[wc->index] : &wc->c;
}

static struct vmem_cell *
slot_cell_data(WrenVM *vm, int slot)
{
	return cell_data(wrenGetSlotForeign(vm, slot));
}

/* Decode the first UTF-8 codepoint from `s`; return bytes consumed,
 * or 0 on truncated/invalid input. */
static int
decode_utf8_first(const char *s, int len, uint32_t *cp_out)
{
	if (s == NULL || len <= 0)
		return 0;
	unsigned char b0 = (unsigned char)s[0];
	if (b0 < 0x80) {
		*cp_out = b0;
		return 1;
	}
	int      n;
	uint32_t cp;
	if ((b0 & 0xE0) == 0xC0) {
		n  = 2;
		cp = b0 & 0x1Fu;
	} else if ((b0 & 0xF0) == 0xE0) {
		n  = 3;
		cp = b0 & 0x0Fu;
	} else if ((b0 & 0xF8) == 0xF0) {
		n  = 4;
		cp = b0 & 0x07u;
	} else {
		return 0;
	}
	if (len < n)
		return 0;
	for (int i = 1; i < n; i++) {
		unsigned char bi = (unsigned char)s[i];
		if ((bi & 0xC0) != 0x80)
			return 0;
		cp = (cp << 6) | (bi & 0x3Fu);
	}
	*cp_out = cp;
	return n;
}

/* Encode codepoint `cp` as UTF-8 into `out`; return bytes written. */
static int
encode_utf8(uint32_t cp, char out[4])
{
	if (cp < 0x80) {
		out[0] = (char)cp;
		return 1;
	}
	if (cp < 0x800) {
		out[0] = (char)(0xC0u | (cp >> 6));
		out[1] = (char)(0x80u | (cp & 0x3Fu));
		return 2;
	}
	if (cp < 0x10000) {
		out[0] = (char)(0xE0u | (cp >> 12));
		out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
		out[2] = (char)(0x80u | (cp & 0x3Fu));
		return 3;
	}
	out[0] = (char)(0xF0u | (cp >> 18));
	out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
	out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
	out[3] = (char)(0x80u | (cp & 0x3Fu));
	return 4;
}

/* --- Cell allocate / finalize ---
 *
 * Cell.new() in Wren land — fill a standalone cell with the current
 * mode's default attribute, fg, bg, a space character, font 0, and
 * no hyperlink.  `parent` stays NULL so accessors operate on `c`. */
static void
wren_cell_allocate(WrenVM *vm)
{
	struct wren_cell *wc = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*wc));
	memset(wc, 0, sizeof(*wc));
	struct text_info ti;
	gettextinfo(&ti);
	wc->c.legacy_attr  = ti.normattr;
	ciolib_attr2palette(ti.normattr, &wc->c.fg, &wc->c.bg);
	wc->c.ch           = 0x20;
	wc->c.font         = 0;
	wc->c.hyperlink_id = 0;
}

static void
wren_cell_finalize(void *data)
{
	struct wren_cell *wc = data;
	if (wc->parent_handle != NULL) {
		struct wren_host_state *st = wren_host_state();
		if (st != NULL && st->vm != NULL)
			wrenReleaseHandle(st->vm, wc->parent_handle);
		wc->parent_handle = NULL;
	}
}

/* --- Cells allocate / finalize ---
 *
 * Allocator leaves the struct empty; Screen.getText fills count and
 * buf after construction.  Finalize releases the malloc'd buffer. */
static void
wren_cells_allocate(WrenVM *vm)
{
	struct wren_cells *cs = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*cs));
	cs->count = 0;
	cs->buf   = NULL;
}

static void
wren_cells_finalize(void *data)
{
	struct wren_cells *cs = data;
	free(cs->buf);
	cs->buf   = NULL;
	cs->count = 0;
}

static void
fn_Screen_moveRect(WrenVM *vm)
{
	int sx = (int)wrenGetSlotDouble(vm, 1);
	int sy = (int)wrenGetSlotDouble(vm, 2);
	int ex = (int)wrenGetSlotDouble(vm, 3);
	int ey = (int)wrenGetSlotDouble(vm, 4);
	int dx = (int)wrenGetSlotDouble(vm, 5);
	int dy = (int)wrenGetSlotDouble(vm, 6);
	movetext(sx, sy, ex, ey, dx, dy);
}

static void
fnScreenWindow_deleteLine(WrenVM *vm)
{
	(void)vm;
	delline();
}

static void
fnScreenWindow_insertLine(WrenVM *vm)
{
	(void)vm;
	insline();
}

static void
fnScreenWindow_scroll(WrenVM *vm)
{
	(void)vm;
	wscroll();
}

/* ----- Screen.hyperlinkId — singleton current-hyperlink ID for new
 * cells produced by window output (putChar, print). */
static void
fn_Screen_hyperlinkId(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)ciolib_get_current_hyperlink());
}

static void
fn_Screen_hyperlinkId_set(WrenVM *vm)
{
	int id = (int)wrenGetSlotDouble(vm, 1);
	ciolib_set_current_hyperlink((uint16_t)id);
}

/* ----- Input.mouseVisible — setter only.  ciolib has no get
 * counterpart, and tracking it from script-side would drift if other
 * code shows or hides the cursor. */
static void
fn_Input_mouseVisible_set(WrenVM *vm)
{
	if (wrenGetSlotBool(vm, 1))
		ciolib_showmouse();
	else
		ciolib_hidemouse();
}

/* ----- Font.codepage / Font.codepageOf(i) — codepage of a font slot. */
static void
fn_Font_codepage(WrenVM *vm)
{
	int f = getfont(0);
	if (f < 0 || f > 256) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotDouble(vm, 0, (double)conio_fontdata[f].cp);
}

static void
fn_Font_codepageOf(WrenVM *vm)
{
	int i = (int)wrenGetSlotDouble(vm, 1);
	if (i < 0 || i > 4) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	int f = getfont(i);
	if (f < 0 || f > 256) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotDouble(vm, 0, (double)conio_fontdata[f].cp);
}

/* Screen.font[i] — slot in 0..4 (5 active font slots in conio). */
static void
fnScreenFonts_subscript(WrenVM *vm)
{
	int i = (int)wrenGetSlotDouble(vm, 1);
	if (i < 0 || i > 4) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotDouble(vm, 0, (double)getfont(i));
}

static void
fnScreenFonts_subscript_set(WrenVM *vm)
{
	int i = (int)wrenGetSlotDouble(vm, 1);
	int n = (int)wrenGetSlotDouble(vm, 2);
	if (i < 0 || i > 4)
		return;
	/* force=0: refuse the change if the font isn't available in the
	 * current video mode rather than silently switching modes. */
	setfont(n, 0, i);
}

/* ----- Screen.palette ----------------------------------------------
 *
 * Per-entry [i]/[i]= read/write a single palette color as 0xRRGGBB.
 * `.mode` and `.mode=` get/set the 16-color mode palette as a Wren
 * List of 16 colors.  Setpalette wants 16-bit r/g/b, getpalette
 * returns 8-bit — we keep the script side at 8-bit-per-channel
 * (0xRRGGBB) and scale on the way in (×257 = ×0x101). */

static void
fnPalette_subscript(WrenVM *vm)
{
	int i = (int)wrenGetSlotDouble(vm, 1);
	uint8_t r, g, b;
	if (i < 0 || ciolib_getpalette((uint32_t)i, &r, &g, &b) != 1) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	uint32_t v = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
	wrenSetSlotDouble(vm, 0, (double)v);
}

static void
fnPalette_subscript_set(WrenVM *vm)
{
	int      i = (int)wrenGetSlotDouble(vm, 1);
	uint32_t c = (uint32_t)(int64_t)wrenGetSlotDouble(vm, 2);
	if (i < 0)
		return;
	uint16_t r = (uint16_t)(((c >> 16) & 0xFFu) * 0x101u);
	uint16_t g = (uint16_t)(((c >>  8) & 0xFFu) * 0x101u);
	uint16_t b = (uint16_t)(( c        & 0xFFu) * 0x101u);
	ciolib_setpalette((uint32_t)i, r, g, b);
}

static void
fnPalette_mode(WrenVM *vm)
{
	uint32_t pal[16];
	if (ciolib_get_modepalette(pal) != 1) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
	for (int i = 0; i < 16; i++) {
		wrenSetSlotDouble(vm, 1, (double)(pal[i] & 0x00FFFFFFu));
		wrenInsertInList(vm, 0, -1, 1);
	}
}

static void
fnPalette_mode_set(WrenVM *vm)
{
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_LIST ||
	    wrenGetListCount(vm, 1) < 16)
		return;
	wrenEnsureSlots(vm, 3);
	uint32_t pal[16];
	for (int i = 0; i < 16; i++) {
		wrenGetListElement(vm, 1, i, 2);
		uint32_t c = (uint32_t)(int64_t)wrenGetSlotDouble(vm, 2);
		/* Mode-palette entries carry the high RGB-mode bit so the
		 * renderer treats them as direct colors rather than palette
		 * indices.  Mask the user value down and OR the bit on. */
		pal[i] = (c & 0x00FFFFFFu) | 0x80000000u;
	}
	ciolib_set_modepalette(pal);
}

/* ----- CustomCursor ------------------------------------------------
 *
 * Two access paths share the same field names:
 *   - foreign static X / X=(_) reads/writes the live cursor
 *     immediately via ciolib_get/setcustomcursor.
 *   - instance X / X=(_) reads/writes a snapshot taken at construct
 *     time; .apply() pushes the snapshot back to ciolib atomically.
 * Boilerplate is generated via macros so the 5×4=20 accessors fit
 * in a few dozen lines. */

struct wren_custom_cursor {
	int startline;
	int endline;
	int range;
	int blink;
	int visible;
};

static void
wren_custom_cursor_allocate(WrenVM *vm)
{
	struct wren_custom_cursor *c = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*c));
	getcustomcursor(&c->startline, &c->endline, &c->range,
	    &c->blink, &c->visible);
}

static void
wren_custom_cursor_finalize(void *data)
{
	(void)data;
}

#define CC_INT_FIELD(NAME, FIELD)                                                \
	static void fnCustomCursor_##NAME##_static(WrenVM *vm)                  \
	{                                                                        \
		int startline, endline, range, blink, visible;                   \
		getcustomcursor(&startline, &endline, &range, &blink, &visible); \
		wrenSetSlotDouble(vm, 0, (double)(FIELD));                       \
	}                                                                        \
	static void fnCustomCursor_##NAME##_set_static(WrenVM *vm)              \
	{                                                                        \
		int startline, endline, range, blink, visible;                   \
		getcustomcursor(&startline, &endline, &range, &blink, &visible); \
		FIELD = (int)wrenGetSlotDouble(vm, 1);                           \
		setcustomcursor(startline, endline, range, blink, visible);      \
	}                                                                        \
	static void fnCustomCursor_##NAME(WrenVM *vm)                           \
	{                                                                        \
		struct wren_custom_cursor *c = wrenGetSlotForeign(vm, 0);        \
		wrenSetSlotDouble(vm, 0, (double)c->FIELD);                      \
	}                                                                        \
	static void fnCustomCursor_##NAME##_set(WrenVM *vm)                     \
	{                                                                        \
		struct wren_custom_cursor *c = wrenGetSlotForeign(vm, 0);        \
		c->FIELD = (int)wrenGetSlotDouble(vm, 1);                        \
	}

#define CC_BOOL_FIELD(NAME, FIELD)                                               \
	static void fnCustomCursor_##NAME##_static(WrenVM *vm)                  \
	{                                                                        \
		int startline, endline, range, blink, visible;                   \
		getcustomcursor(&startline, &endline, &range, &blink, &visible); \
		wrenSetSlotBool(vm, 0, (FIELD) != 0);                            \
	}                                                                        \
	static void fnCustomCursor_##NAME##_set_static(WrenVM *vm)              \
	{                                                                        \
		int startline, endline, range, blink, visible;                   \
		getcustomcursor(&startline, &endline, &range, &blink, &visible); \
		FIELD = wrenGetSlotBool(vm, 1) ? 1 : 0;                          \
		setcustomcursor(startline, endline, range, blink, visible);      \
	}                                                                        \
	static void fnCustomCursor_##NAME(WrenVM *vm)                           \
	{                                                                        \
		struct wren_custom_cursor *c = wrenGetSlotForeign(vm, 0);        \
		wrenSetSlotBool(vm, 0, c->FIELD != 0);                           \
	}                                                                        \
	static void fnCustomCursor_##NAME##_set(WrenVM *vm)                     \
	{                                                                        \
		struct wren_custom_cursor *c = wrenGetSlotForeign(vm, 0);        \
		c->FIELD = wrenGetSlotBool(vm, 1) ? 1 : 0;                       \
	}

CC_INT_FIELD (startLine, startline)
CC_INT_FIELD (endLine,   endline)
CC_INT_FIELD (range,     range)
CC_BOOL_FIELD(blink,     blink)
CC_BOOL_FIELD(visible,   visible)

#undef CC_INT_FIELD
#undef CC_BOOL_FIELD

static void
fnCustomCursor_apply(WrenVM *vm)
{
	struct wren_custom_cursor *c = wrenGetSlotForeign(vm, 0);
	setcustomcursor(c->startline, c->endline, c->range, c->blink, c->visible);
}

/* presetLegacy_(legacyType): build a snapshot from the legacy
 * setcursortype() semantics.  Backends interpret the type code by
 * mutating the same start/end/blink/visible state custom-cursor uses,
 * so we capture the current values, apply the preset, read it back,
 * then revert.  The reverted state isn't drawn between syscalls (no
 * frame renders inside a foreign method), so no flicker. */
static void
fnCustomCursorpresetLegacy_(WrenVM *vm)
{
	int legacy = (int)wrenGetSlotDouble(vm, 1);

	int saved_sl, saved_el, saved_r, saved_b, saved_v;
	getcustomcursor(&saved_sl, &saved_el, &saved_r, &saved_b, &saved_v);

	_setcursortype(legacy);

	int sl, el, r, b, v;
	getcustomcursor(&sl, &el, &r, &b, &v);

	setcustomcursor(saved_sl, saved_el, saved_r, saved_b, saved_v);

	/* Slot 0 holds the CustomCursor class (static-method receiver);
	 * use it as the class for the new instance and overwrite slot 0
	 * with the result.  wrenSetSlotNewForeign reads the class before
	 * writing, so same-slot is safe. */
	struct wren_custom_cursor *c =
	    wrenSetSlotNewForeign(vm, 0, 0, sizeof(*c));
	c->startline = sl;
	c->endline   = el;
	c->range     = r;
	c->blink     = b;
	c->visible   = v;
}

/* ----- VideoFlags -------------------------------------------------- */

struct wren_video_flags {
	int flags;
};

static void
wren_video_flags_allocate(WrenVM *vm)
{
	struct wren_video_flags *vf = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*vf));
	vf->flags = getvideoflags();
}

static void
wren_video_flags_finalize(void *data)
{
	(void)data;
}

#define VF_FLAG(NAME, BIT)                                                       \
	static void fnVideoFlags_##NAME##_static(WrenVM *vm)                    \
	{                                                                        \
		wrenSetSlotBool(vm, 0, (getvideoflags() & (BIT)) != 0);          \
	}                                                                        \
	static void fnVideoFlags_##NAME##_set_static(WrenVM *vm)                \
	{                                                                        \
		int flags = getvideoflags();                                     \
		if (wrenGetSlotBool(vm, 1)) flags |= (BIT);                      \
		else                        flags &= ~(BIT);                     \
		setvideoflags(flags);                                            \
	}                                                                        \
	static void fnVideoFlags_##NAME(WrenVM *vm)                             \
	{                                                                        \
		struct wren_video_flags *vf = wrenGetSlotForeign(vm, 0);         \
		wrenSetSlotBool(vm, 0, (vf->flags & (BIT)) != 0);                \
	}                                                                        \
	static void fnVideoFlags_##NAME##_set(WrenVM *vm)                       \
	{                                                                        \
		struct wren_video_flags *vf = wrenGetSlotForeign(vm, 0);         \
		if (wrenGetSlotBool(vm, 1)) vf->flags |= (BIT);                  \
		else                        vf->flags &= ~(BIT);                 \
	}

VF_FLAG(altChars,           CIOLIB_VIDEO_ALTCHARS)
VF_FLAG(noBright,           CIOLIB_VIDEO_NOBRIGHT)
VF_FLAG(bgBright,           CIOLIB_VIDEO_BGBRIGHT)
VF_FLAG(blinkAltChars,      CIOLIB_VIDEO_BLINKALTCHARS)
VF_FLAG(noBlink,            CIOLIB_VIDEO_NOBLINK)
VF_FLAG(lineGraphicsExpand, CIOLIB_VIDEO_LINE_GRAPHICS_EXPAND)

#undef VF_FLAG

/* expand is read-only: the bitmap buffer's per-cell pixel width is
 * fixed at video-mode init.  Getter only — no setter. */
static void
fnVideoFlags_expand_static(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, (getvideoflags() & CIOLIB_VIDEO_EXPAND) != 0);
}

static void
fnVideoFlags_expand(WrenVM *vm)
{
	struct wren_video_flags *vf = wrenGetSlotForeign(vm, 0);
	wrenSetSlotBool(vm, 0, (vf->flags & CIOLIB_VIDEO_EXPAND) != 0);
}

static void
fnVideoFlags_apply(WrenVM *vm)
{
	struct wren_video_flags *vf = wrenGetSlotForeign(vm, 0);
	setvideoflags(vf->flags);
}

/* ----- Color ------------------------------------------------------- */

static void
fnColor_fromRgb(WrenVM *vm)
{
	int r = (int)wrenGetSlotDouble(vm, 1);
	int g = (int)wrenGetSlotDouble(vm, 2);
	int b = (int)wrenGetSlotDouble(vm, 3);
	/* ciolib_map_rgb returns the value with the high RGB bit set; we
	 * mask it off — Cell.fgRgb=/bgRgb= add the bit when writing.
	 * Returning the raw 24-bit value keeps the script side simpler. */
	uint32_t v = ciolib_map_rgb((uint16_t)r, (uint16_t)g, (uint16_t)b)
	    & 0x00FFFFFFu;
	wrenSetSlotDouble(vm, 0, (double)v);
}

static void
fnColor_fromAttr(WrenVM *vm)
{
	int      attr = (int)wrenGetSlotDouble(vm, 1);
	uint32_t fg   = 0;
	uint32_t bg   = 0;
	ciolib_attr2palette((uint8_t)attr, &fg, &bg);
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
	wrenSetSlotDouble(vm, 1, (double)(fg & 0x00FFFFFFu));
	wrenInsertInList(vm, 0, -1, 1);
	wrenSetSlotDouble(vm, 1, (double)(bg & 0x00FFFFFFu));
	wrenInsertInList(vm, 0, -1, 1);
}

static void
fnColor_toLegacyAttr(WrenVM *vm)
{
	uint32_t fg = (uint32_t)(int64_t)wrenGetSlotDouble(vm, 1);
	uint32_t bg = (uint32_t)(int64_t)wrenGetSlotDouble(vm, 2);
	/* ciolib_rgb_to_legacyattr looks at bit 31 to decide RGB vs
	 * palette; our callers pass raw 24-bit RGB so set the bit. */
	wrenSetSlotDouble(vm, 0,
	    (double)ciolib_rgb_to_legacyattr(
	        fg | 0x80000000u, bg | 0x80000000u));
}

/* ----- Screen.getText / Screen.putText ----- */

static void
fn_Screen_readRect(WrenVM *vm)
{
	int sx = (int)wrenGetSlotDouble(vm, 1);
	int sy = (int)wrenGetSlotDouble(vm, 2);
	int ex = (int)wrenGetSlotDouble(vm, 3);
	int ey = (int)wrenGetSlotDouble(vm, 4);

	if (sx < 1 || sy < 1 || ex < sx || ey < sy) {
		wrenSetSlotNull(vm, 0);
		return;
	}

	size_t            n   = (size_t)(ex - sx + 1) * (size_t)(ey - sy + 1);
	struct vmem_cell *buf = calloc(n, sizeof *buf);
	if (buf == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	if (!ciolib_vmem_gettext(sx, sy, ex, ey, buf)) {
		free(buf);
		wrenSetSlotNull(vm, 0);
		return;
	}

	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->cells_class == NULL) {
		free(buf);
		wrenSetSlotNull(vm, 0);
		return;
	}

	wrenEnsureSlots(vm, 2);
	wrenSetSlotHandle(vm, 1, st->cells_class);
	struct wren_cells *cs =
	    wrenSetSlotNewForeign(vm, 0, 1, sizeof(*cs));
	cs->count = (int)n;
	cs->buf   = buf;
}

static void
fn_Input_mousedrag(WrenVM *vm)
{
	(void)vm;
	mousedrag(NULL);
}

/* REPL.compile_(module, src, isExpression, printErrors) — compile
 * `src` at the top level of the named module and return the resulting
 * closure as a Wren value.  Calling .call() on the closure runs the
 * body in module scope, so top-level `var x = ...` becomes a module
 * variable that persists across submissions.
 *
 * isExpression=true compiles `src` as a single expression; .call()
 * returns the expression's value.  Wren-side REPL.eval uses this to
 * implement the "P" of REPL — try expression form, print the result,
 * fall back to statement form when the input isn't a single
 * expression (e.g. `var x = 5`).
 *
 * Mirrors metaCompile's pattern of dropping into a bare ObjClosure
 * because the public API has no wrapper.  wrenCompileSource is
 * foreign-method-safe (no fiber switching), and the Wren-side .call()
 * dispatch is foreign-method-safe too. */
/* REPL.captureStart_/Contains_/Clear_/Commit_ — gate around the
 * compile-error log capture in wren_host.c.  REPL.eval brackets each
 * compile attempt with start/commit (or clear) so it can peek at the
 * errors and decide whether to surface them to the real log. */
static void
fn_REPL_captureStart_(WrenVM *vm)
{
	(void)vm;
	wren_log_capture_start();
}

static void
fn_REPL_captureContains_(WrenVM *vm)
{
	const char *needle = wrenGetSlotString(vm, 1);
	wrenSetSlotBool(vm, 0, wren_log_capture_contains(needle));
}

static void
fn_REPL_captureClear_(WrenVM *vm)
{
	(void)vm;
	wren_log_capture_clear();
}

static void
fn_REPL_captureCommit_(WrenVM *vm)
{
	(void)vm;
	wren_log_capture_commit();
}

/* REPL.printTrace_(fiber) — replicates wrenDebugPrintStackTrace for a
 * caught fiber.  Wren only emits stack frames via the error callback
 * for *uncaught* aborts; once you Fiber.try() a failed fiber, the
 * frames are still on it but the runtime never walks them.  This
 * helper does the walk and pushes each frame into the host error
 * callback (and from there into the log buffer) so the console can
 * surface a real trace even when it caught the abort to keep itself
 * alive.  The runtime error MESSAGE is the .try() return value — the
 * caller prints that separately. */
static void
fn_REPL_printTrace_(WrenVM *vm)
{
	if (vm->config.errorFn == NULL)
		return;
	Value v = vm->apiStack[1];
	if (!IS_FIBER(v))
		return;
	ObjFiber *fiber = AS_FIBER(v);

	for (int i = fiber->numFrames - 1; i >= 0; i--) {
		CallFrame *frame = &fiber->frames[i];
		ObjFn     *fn    = frame->closure->fn;
		/* Skip C-API stubs and the unnamed core module to match
		 * wrenDebugPrintStackTrace's filtering. */
		if (fn->module == NULL || fn->module->name == NULL)
			continue;
		/* IP has advanced past the instruction that just executed,
		 * so step back one to find the source line for the call. */
		int line = fn->debug->sourceLines.data[
		    frame->ip - fn->code.data - 1];
		vm->config.errorFn(vm, WREN_ERROR_STACK_TRACE,
		    fn->module->name->value, line, fn->debug->name);
	}
}

static void
fn_REPL_compile_(WrenVM *vm)
{
	const char *m = wrenGetSlotString(vm, 1);
	const char *s = wrenGetSlotString(vm, 2);
	bool isExpr      = wrenGetSlotBool(vm, 3);
	bool printErrors = wrenGetSlotBool(vm, 4);
	if (m == NULL || s == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	ObjClosure *closure = wrenCompileSource(vm, m, s, isExpr, printErrors);
	if (closure == NULL)
		vm->apiStack[0] = NULL_VAL;
	else
		vm->apiStack[0] = OBJ_VAL(closure);
}

static void
fn_REPL_hasModule(WrenVM *vm)
{
	int         len = 0;
	const char *m   = wrenGetSlotBytes(vm, 1, &len);
	if (m == NULL || len <= 0) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	char *zm = malloc((size_t)len + 1);
	if (zm == NULL) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	memcpy(zm, m, (size_t)len);
	zm[len] = '\0';
	bool has = wrenHasModule(vm, zm);
	free(zm);
	wrenSetSlotBool(vm, 0, has);
}

/* Read system clipboard.  ciolib_getcliptext returns a malloc'd
 * UTF-8 buffer; we hand it to Wren and free our copy. */
static void
fn_Clipboard_text(WrenVM *vm)
{
	char *s = getcliptext();
	if (s == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotString(vm, 0, s);
	free(s);
}

/* Write the system clipboard. */
static void
fn_Clipboard_text_set(WrenVM *vm)
{
	int         len = 0;
	const char *s   = wrenGetSlotBytes(vm, 1, &len);
	if (s != NULL && len > 0)
		ciolib_copytext(s, (size_t)len);
}

static void
fn_Screen_writeRect(WrenVM *vm)
{
	int sx = (int)wrenGetSlotDouble(vm, 1);
	int sy = (int)wrenGetSlotDouble(vm, 2);
	int ex = (int)wrenGetSlotDouble(vm, 3);
	int ey = (int)wrenGetSlotDouble(vm, 4);

	if (sx < 1 || sy < 1 || ex < sx || ey < sy) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	if (wrenGetSlotType(vm, 5) != WREN_TYPE_LIST) {
		wrenEnsureSlots(vm, 1);
		wrenSetSlotString(vm, 0, "putText: 5th argument must be a List of Cell");
		wrenAbortFiber(vm, 0);
		return;
	}
	int n_expected = (ex - sx + 1) * (ey - sy + 1);
	int n_got      = wrenGetListCount(vm, 5);
	if (n_got != n_expected) {
		wrenEnsureSlots(vm, 1);
		wrenSetSlotString(vm, 0,
		    "putText: list length does not match region size");
		wrenAbortFiber(vm, 0);
		return;
	}

	struct vmem_cell *buf = calloc((size_t)n_expected, sizeof *buf);
	if (buf == NULL) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}

	wrenEnsureSlots(vm, 7);
	for (int i = 0; i < n_expected; i++) {
		wrenGetListElement(vm, 5, i, 6);
		if (wrenGetSlotType(vm, 6) != WREN_TYPE_FOREIGN) {
			free(buf);
			wrenSetSlotString(vm, 0,
			    "putText: list element is not a Cell");
			wrenAbortFiber(vm, 0);
			return;
		}
		struct wren_cell *wc = wrenGetSlotForeign(vm, 6);
		buf[i] = *cell_data(wc);
	}

	int rv = ciolib_vmem_puttext(sx, sy, ex, ey, buf);
	free(buf);
	wrenSetSlotBool(vm, 0, rv != 0);
}

/* ----- Cell field accessors ----- */

static void
fn_Cell_ch(WrenVM *vm)
{
	struct vmem_cell *d  = slot_cell_data(vm, 0);
	uint32_t          cp = cpoint_from_cpchar(CIOLIB_CP437, d->ch);
	char              buf[4];
	int               len = encode_utf8(cp, buf);
	wrenSetSlotBytes(vm, 0, buf, (size_t)len);
}

static void
fn_Cell_ch_set(WrenVM *vm)
{
	struct vmem_cell *d   = slot_cell_data(vm, 0);
	int               len = 0;
	const char       *s   = wrenGetSlotBytes(vm, 1, &len);
	uint32_t          cp  = 0;
	if (decode_utf8_first(s, len, &cp) == 0) {
		d->ch = 0;
		return;
	}
	d->ch = cpchar_from_unicode_cpoint(CIOLIB_CP437, cp, '?');
}

static void
fn_Cell_chByte(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)d->ch);
}

static void
fn_Cell_chByte_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	d->ch = (uint8_t)(int)wrenGetSlotDouble(vm, 1);
}

static void
fn_Cell_font(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)d->font);
}

static void
fn_Cell_font_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	d->font = (uint8_t)(int)wrenGetSlotDouble(vm, 1);
}

static void
fn_Cell_legacyAttr(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)d->legacy_attr);
}

static void
fn_Cell_legacyAttr_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	d->legacy_attr = (uint8_t)(int)wrenGetSlotDouble(vm, 1);
}

static void
fn_Cell_bright(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	wrenSetSlotBool(vm, 0, (d->legacy_attr & 0x08) != 0);
}

static void
fn_Cell_bright_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	if (wrenGetSlotBool(vm, 1))
		d->legacy_attr |= 0x08u;
	else
		d->legacy_attr &= (uint8_t)~0x08u;
}

static void
fn_Cell_blink(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	wrenSetSlotBool(vm, 0, (d->legacy_attr & 0x80) != 0);
}

static void
fn_Cell_blink_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	if (wrenGetSlotBool(vm, 1))
		d->legacy_attr |= 0x80u;
	else
		d->legacy_attr &= (uint8_t)~0x80u;
}

/* fg/bg follow the same pattern: bit 31 selects RGB vs palette; bits
 * 24..30 are out-of-scope flags preserved verbatim across writes; the
 * low 24 bits hold the value being read or written. */

static void
fn_Cell_fgPalette(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	if (d->fg & CIOLIB_COLOR_RGB)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotDouble(vm, 0, (double)(d->fg & 0x00FFFFFFu));
}

static void
fn_Cell_fgPalette_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	uint32_t          n = (uint32_t)(int64_t)wrenGetSlotDouble(vm, 1);
	d->fg = (d->fg & 0x7F000000u) | (n & 0x00FFFFFFu);
}

static void
fn_Cell_fgRgb(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	if (!(d->fg & CIOLIB_COLOR_RGB))
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotDouble(vm, 0, (double)(d->fg & 0x00FFFFFFu));
}

static void
fn_Cell_fgRgb_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	uint32_t          n = (uint32_t)(int64_t)wrenGetSlotDouble(vm, 1);
	d->fg = (d->fg & 0x7F000000u) | 0x80000000u | (n & 0x00FFFFFFu);
}

static void
fn_Cell_bgPalette(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	if (d->bg & CIOLIB_COLOR_RGB)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotDouble(vm, 0, (double)(d->bg & 0x00FFFFFFu));
}

static void
fn_Cell_bgPalette_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	uint32_t          n = (uint32_t)(int64_t)wrenGetSlotDouble(vm, 1);
	d->bg = (d->bg & 0x7F000000u) | (n & 0x00FFFFFFu);
}

static void
fn_Cell_bgRgb(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	if (!(d->bg & CIOLIB_COLOR_RGB))
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotDouble(vm, 0, (double)(d->bg & 0x00FFFFFFu));
}

static void
fn_Cell_bgRgb_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	uint32_t          n = (uint32_t)(int64_t)wrenGetSlotDouble(vm, 1);
	d->bg = (d->bg & 0x7F000000u) | 0x80000000u | (n & 0x00FFFFFFu);
}

static void
fn_Cell_hyperlinkId(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)d->hyperlink_id);
}

static void
fn_Cell_hyperlinkId_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	d->hyperlink_id = (uint16_t)(int)wrenGetSlotDouble(vm, 1);
}

/* ----- Cells accessors ----- */

static void
fn_Cells_count(WrenVM *vm)
{
	struct wren_cells *cs = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)cs->count);
}

/* Materialize a Cell view into slot 0.  Pins the parent Cells (slot 0
 * receiver) via a Wren handle so the buffer outlives the view. */
static void
cells_make_view(WrenVM *vm, int idx)
{
	struct wren_cells *cs = wrenGetSlotForeign(vm, 0);
	if (idx < 0 || idx >= cs->count) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->cell_class == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	WrenHandle *parent_handle = wrenGetSlotHandle(vm, 0);

	wrenEnsureSlots(vm, 3);
	wrenSetSlotHandle(vm, 2, st->cell_class);
	struct wren_cell *wc = wrenSetSlotNewForeign(vm, 0, 2, sizeof(*wc));
	memset(wc, 0, sizeof(*wc));
	wc->parent        = cs;
	wc->index         = idx;
	wc->parent_handle = parent_handle;
}

static void
fn_Cells_subscript(WrenVM *vm)
{
	int i = (int)wrenGetSlotDouble(vm, 1);
	cells_make_view(vm, i);
}

/* Wren iteration: null -> 0; n -> n+1; past end -> false. */
static void
fn_Cells_iterate(WrenVM *vm)
{
	struct wren_cells *cs = wrenGetSlotForeign(vm, 0);
	if (cs->count == 0) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	if (wrenGetSlotType(vm, 1) == WREN_TYPE_NULL) {
		wrenSetSlotDouble(vm, 0, 0.0);
		return;
	}
	int next = (int)wrenGetSlotDouble(vm, 1) + 1;
	if (next >= cs->count)
		wrenSetSlotBool(vm, 0, false);
	else
		wrenSetSlotDouble(vm, 0, (double)next);
}

static void
fn_Cells_iteratorValue(WrenVM *vm)
{
	int i = (int)wrenGetSlotDouble(vm, 1);
	cells_make_view(vm, i);
}

/* ----- Font ----- */

static void
fn_Font_name(WrenVM *vm)
{
	int i = (int)wrenGetSlotDouble(vm, 1);
	if (i < 0 || i > 256 || conio_fontdata[i].desc == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotString(vm, 0, conio_fontdata[i].desc);
}

static void
fn_Font_count(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, 257.0);
}

static void
fn_Font_available(WrenVM *vm)
{
	int i = (int)wrenGetSlotDouble(vm, 1);
	wrenSetSlotBool(vm, 0, ciolib_checkfont(i) != 0);
}

/* ----- Hyperlinks (Map-like read interface) ----- */

static void
fn_Hyperlinks_subscript(WrenVM *vm)
{
	int id = (int)wrenGetSlotDouble(vm, 1);
	if (id <= 0 || id > 65535) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	char *url = ciolib_get_hyperlink_url((uint16_t)id);
	if (url == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotString(vm, 0, url);
	free(url);
}

static void
fn_Hyperlinks_containsKey(WrenVM *vm)
{
	int id = (int)wrenGetSlotDouble(vm, 1);
	if (id <= 0 || id > 65535) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	char *url = ciolib_get_hyperlink_url((uint16_t)id);
	wrenSetSlotBool(vm, 0, url != NULL);
	free(url);
}

static void
fn_Hyperlinks_add(WrenVM *vm)
{
	int         len   = 0;
	const char *uri_b = wrenGetSlotBytes(vm, 1, &len);
	if (uri_b == NULL || len <= 0) {
		wrenSetSlotDouble(vm, 0, 0.0);
		return;
	}
	char *uri_z = malloc((size_t)len + 1);
	if (uri_z == NULL) {
		wrenSetSlotDouble(vm, 0, 0.0);
		return;
	}
	memcpy(uri_z, uri_b, (size_t)len);
	uri_z[len] = '\0';

	char *id_param_z = NULL;
	if (wrenGetSlotType(vm, 2) == WREN_TYPE_STRING) {
		int         plen = 0;
		const char *pb   = wrenGetSlotBytes(vm, 2, &plen);
		id_param_z = malloc((size_t)plen + 1);
		if (id_param_z != NULL) {
			memcpy(id_param_z, pb, (size_t)plen);
			id_param_z[plen] = '\0';
		}
	}

	uint16_t id = ciolib_add_hyperlink(uri_z, id_param_z);
	free(uri_z);
	free(id_param_z);
	wrenSetSlotDouble(vm, 0, (double)id);
}

static void
fn_Hyperlinks_params(WrenVM *vm)
{
	int id = (int)wrenGetSlotDouble(vm, 1);
	if (id <= 0 || id > 65535) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	char *params = ciolib_get_hyperlink_params((uint16_t)id);
	if (params == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotString(vm, 0, params);
	free(params);
}

/* ----- Lookup table -----------------------------------------------
 *
 * Wren signatures: foreign static methods are reported with the
 * leading "static " prefix in `signature`.  Getters look like "name"
 * (no parens); methods look like "name(_)" / "name(_,_)" with one
 * underscore per argument.
 * -------------------------------------------------------------------- */

struct binding {
	const char        *className;
	bool               isStatic;
	const char        *signature;
	WrenForeignMethodFn fn;
};

static const struct binding BINDINGS[] = {
	/* Screen — absolute / global operations */
	{ "Screen", true,  "size",                  fn_Screen_size            },
	{ "Screen", true,  "save()",                fn_Screen_save            },
	{ "Screen", true,  "restore(_)",            fn_Screen_restore         },
	{ "Screen", true,  "readRect(_,_,_,_)",     fn_Screen_readRect        },
	{ "Screen", true,  "writeRect(_,_,_,_,_)",  fn_Screen_writeRect       },
	{ "Screen", true,  "moveRect(_,_,_,_,_,_)", fn_Screen_moveRect        },
	{ "Screen", true,  "attr=(_)",              fn_Screen_attr_set        },
	{ "Screen", true,  "hyperlinkId",           fn_Screen_hyperlinkId     },
	{ "Screen", true,  "hyperlinkId=(_)",       fn_Screen_hyperlinkId_set },

	/* ScreenWindow — operations scoped to the active text window */
	{ "ScreenWindow", true, "bounds",             fnScreenWindow_bounds          },
	{ "ScreenWindow", true, "bounds=(_)",         fnScreenWindow_bounds_set      },
	{ "ScreenWindow", true, "position",           fnScreenWindow_position        },
	{ "ScreenWindow", true, "position=(_)",       fnScreenWindow_position_set    },
	{ "ScreenWindow", true, "putChar(_)",         fnScreenWindow_putChar         },
	{ "ScreenWindow", true, "print(_)",           fnScreenWindow_print           },
	{ "ScreenWindow", true, "clear()",            fnScreenWindow_clear           },
	{ "ScreenWindow", true, "clearToLineEnd()",   fnScreenWindow_clearToLineEnd  },
	{ "ScreenWindow", true, "deleteLine()",       fnScreenWindow_deleteLine      },
	{ "ScreenWindow", true, "insertLine()",       fnScreenWindow_insertLine      },
	{ "ScreenWindow", true, "scroll()",           fnScreenWindow_scroll          },

	/* ScreenFonts — Screen.font[i] / Screen.font[i] = n */
	{ "ScreenFonts", true, "[_]",     fnScreenFonts_subscript     },
	{ "ScreenFonts", true, "[_]=(_)", fnScreenFonts_subscript_set },

	/* Palette — Screen.palette[i] / .mode */
	{ "Palette", true, "[_]",       fnPalette_subscript     },
	{ "Palette", true, "[_]=(_)",   fnPalette_subscript_set },
	{ "Palette", true, "mode",      fnPalette_mode          },
	{ "Palette", true, "mode=(_)",  fnPalette_mode_set      },

	/* CustomCursor — static (live) and instance (snapshot) accessors */
	{ "CustomCursor", true,  "startLine",       fnCustomCursor_startLine_static     },
	{ "CustomCursor", true,  "startLine=(_)",   fnCustomCursor_startLine_set_static },
	{ "CustomCursor", true,  "endLine",         fnCustomCursor_endLine_static       },
	{ "CustomCursor", true,  "endLine=(_)",     fnCustomCursor_endLine_set_static   },
	{ "CustomCursor", true,  "range",           fnCustomCursor_range_static         },
	{ "CustomCursor", true,  "range=(_)",       fnCustomCursor_range_set_static     },
	{ "CustomCursor", true,  "blink",           fnCustomCursor_blink_static         },
	{ "CustomCursor", true,  "blink=(_)",       fnCustomCursor_blink_set_static     },
	{ "CustomCursor", true,  "visible",         fnCustomCursor_visible_static       },
	{ "CustomCursor", true,  "visible=(_)",     fnCustomCursor_visible_set_static   },
	{ "CustomCursor", false, "startLine",       fnCustomCursor_startLine            },
	{ "CustomCursor", false, "startLine=(_)",   fnCustomCursor_startLine_set        },
	{ "CustomCursor", false, "endLine",         fnCustomCursor_endLine              },
	{ "CustomCursor", false, "endLine=(_)",     fnCustomCursor_endLine_set          },
	{ "CustomCursor", false, "range",           fnCustomCursor_range                },
	{ "CustomCursor", false, "range=(_)",       fnCustomCursor_range_set            },
	{ "CustomCursor", false, "blink",           fnCustomCursor_blink                },
	{ "CustomCursor", false, "blink=(_)",       fnCustomCursor_blink_set            },
	{ "CustomCursor", false, "visible",         fnCustomCursor_visible              },
	{ "CustomCursor", false, "visible=(_)",     fnCustomCursor_visible_set          },
	{ "CustomCursor", false, "apply()",         fnCustomCursor_apply                },
	{ "CustomCursor", true,  "presetLegacy_(_)",     fnCustomCursorpresetLegacy_              },

	/* VideoFlags — static (live) and instance (snapshot) */
	{ "VideoFlags", true,  "altChars",                fnVideoFlags_altChars_static                },
	{ "VideoFlags", true,  "altChars=(_)",            fnVideoFlags_altChars_set_static            },
	{ "VideoFlags", true,  "noBright",                fnVideoFlags_noBright_static                },
	{ "VideoFlags", true,  "noBright=(_)",            fnVideoFlags_noBright_set_static            },
	{ "VideoFlags", true,  "bgBright",                fnVideoFlags_bgBright_static                },
	{ "VideoFlags", true,  "bgBright=(_)",            fnVideoFlags_bgBright_set_static            },
	{ "VideoFlags", true,  "blinkAltChars",           fnVideoFlags_blinkAltChars_static           },
	{ "VideoFlags", true,  "blinkAltChars=(_)",       fnVideoFlags_blinkAltChars_set_static       },
	{ "VideoFlags", true,  "noBlink",                 fnVideoFlags_noBlink_static                 },
	{ "VideoFlags", true,  "noBlink=(_)",             fnVideoFlags_noBlink_set_static             },
	{ "VideoFlags", true,  "expand",                  fnVideoFlags_expand_static                  },
	{ "VideoFlags", true,  "lineGraphicsExpand",      fnVideoFlags_lineGraphicsExpand_static      },
	{ "VideoFlags", true,  "lineGraphicsExpand=(_)",  fnVideoFlags_lineGraphicsExpand_set_static  },
	{ "VideoFlags", false, "altChars",                fnVideoFlags_altChars                       },
	{ "VideoFlags", false, "altChars=(_)",            fnVideoFlags_altChars_set                   },
	{ "VideoFlags", false, "noBright",                fnVideoFlags_noBright                       },
	{ "VideoFlags", false, "noBright=(_)",            fnVideoFlags_noBright_set                   },
	{ "VideoFlags", false, "bgBright",                fnVideoFlags_bgBright                       },
	{ "VideoFlags", false, "bgBright=(_)",            fnVideoFlags_bgBright_set                   },
	{ "VideoFlags", false, "blinkAltChars",           fnVideoFlags_blinkAltChars                  },
	{ "VideoFlags", false, "blinkAltChars=(_)",       fnVideoFlags_blinkAltChars_set              },
	{ "VideoFlags", false, "noBlink",                 fnVideoFlags_noBlink                        },
	{ "VideoFlags", false, "noBlink=(_)",             fnVideoFlags_noBlink_set                    },
	{ "VideoFlags", false, "expand",                  fnVideoFlags_expand                         },
	{ "VideoFlags", false, "lineGraphicsExpand",      fnVideoFlags_lineGraphicsExpand             },
	{ "VideoFlags", false, "lineGraphicsExpand=(_)",  fnVideoFlags_lineGraphicsExpand_set         },
	{ "VideoFlags", false, "apply()",                 fnVideoFlags_apply                          },

	/* Color — static utilities */
	{ "Color", true, "fromRgb(_,_,_)",     fnColor_fromRgb       },
	{ "Color", true, "fromAttr(_)",        fnColor_fromAttr      },
	{ "Color", true, "toLegacyAttr(_,_)",  fnColor_toLegacyAttr  },

	/* ScreenSupports — Screen.supports.<flag> capability getters */
	{ "ScreenSupports", true, "loadableFonts",    fnScreenSupports_loadableFonts    },
	{ "ScreenSupports", true, "altBlinkFont",     fnScreenSupports_altBlinkFont     },
	{ "ScreenSupports", true, "altBoldFont",      fnScreenSupports_altBoldFont      },
	{ "ScreenSupports", true, "brightBackground", fnScreenSupports_brightBackground },
	{ "ScreenSupports", true, "paletteChange",    fnScreenSupports_paletteChange    },
	{ "ScreenSupports", true, "pixels",           fnScreenSupports_pixels           },
	{ "ScreenSupports", true, "customCursor",     fnScreenSupports_customCursor     },
	{ "ScreenSupports", true, "fontSelection",    fnScreenSupports_fontSelection    },
	{ "ScreenSupports", true, "windowTitle",      fnScreenSupports_windowTitle      },
	{ "ScreenSupports", true, "windowName",       fnScreenSupports_windowName       },
	{ "ScreenSupports", true, "windowIcon",       fnScreenSupports_windowIcon       },
	{ "ScreenSupports", true, "extendedPalette",  fnScreenSupports_extendedPalette  },
	{ "ScreenSupports", true, "blockyScaling",    fnScreenSupports_blockyScaling    },
	{ "ScreenSupports", true, "externalScaling",  fnScreenSupports_externalScaling  },
	{ "ScreenSupports", true, "closeLock",        fnScreenSupports_closeLock        },

	/* Input (all static) */
	{ "Input",  true,  "next",                fn_Input_next               },
	{ "Input",  true,  "next(_)",             fn_Input_next_ms            },
	{ "Input",  true,  "poll",                fn_Input_poll               },
	{ "Input",  true,  "ungetKey_(_)",        fn_Input_ungetKey_          },
	{ "Input",  true,  "ungetMouse_(_)",      fn_Input_ungetMouse_        },
	{ "Input",  true,  "mousedrag()",         fn_Input_mousedrag          },
	{ "Input",  true,  "mouseVisible=(_)",    fn_Input_mouseVisible_set   },

	/* KeyEvent (instance) */
	{ "KeyEvent",   false, "code",       fn_KeyEvent_code      },
	{ "KeyEvent",   false, "codepoint",  fn_KeyEvent_codepoint },
	{ "KeyEvent",   false, "text",       fn_KeyEvent_text      },

	/* MouseEvent (instance) */
	{ "MouseEvent", false, "event",      fn_MouseEvent_event      },
	{ "MouseEvent", false, "modifiers",  fn_MouseEvent_modifiers  },
	{ "MouseEvent", false, "startX",     fn_MouseEvent_startX     },
	{ "MouseEvent", false, "startY",     fn_MouseEvent_startY     },
	{ "MouseEvent", false, "endX",       fn_MouseEvent_endX       },
	{ "MouseEvent", false, "endY",       fn_MouseEvent_endY       },

	/* Clipboard (all static) */
	{ "Clipboard", true, "text",               fn_Clipboard_text     },
	{ "Clipboard", true, "text=(_)",           fn_Clipboard_text_set },

	/* REPL */
	{ "REPL",  true,  "compile_(_,_,_,_)",   fn_REPL_compile_         },
	{ "REPL",  true,  "printTrace_(_)",      fn_REPL_printTrace_      },
	{ "REPL",  true,  "hasModule(_)",        fn_REPL_hasModule        },
	{ "REPL",  true,  "captureStart_()",     fn_REPL_captureStart_    },
	{ "REPL",  true,  "captureContains_(_)", fn_REPL_captureContains_ },
	{ "REPL",  true,  "captureClear_()",     fn_REPL_captureClear_    },
	{ "REPL",  true,  "captureCommit_()",    fn_REPL_captureCommit_   },

	/* Cell (all instance) */
	{ "Cell",  false, "ch",              fn_Cell_ch              },
	{ "Cell",  false, "ch=(_)",          fn_Cell_ch_set          },
	{ "Cell",  false, "chByte",          fn_Cell_chByte          },
	{ "Cell",  false, "chByte=(_)",      fn_Cell_chByte_set      },
	{ "Cell",  false, "font",            fn_Cell_font            },
	{ "Cell",  false, "font=(_)",        fn_Cell_font_set        },
	{ "Cell",  false, "legacyAttr",      fn_Cell_legacyAttr      },
	{ "Cell",  false, "legacyAttr=(_)",  fn_Cell_legacyAttr_set  },
	{ "Cell",  false, "bright",          fn_Cell_bright          },
	{ "Cell",  false, "bright=(_)",      fn_Cell_bright_set      },
	{ "Cell",  false, "blink",           fn_Cell_blink           },
	{ "Cell",  false, "blink=(_)",       fn_Cell_blink_set       },
	{ "Cell",  false, "fgPalette",       fn_Cell_fgPalette       },
	{ "Cell",  false, "fgPalette=(_)",   fn_Cell_fgPalette_set   },
	{ "Cell",  false, "fgRgb",           fn_Cell_fgRgb           },
	{ "Cell",  false, "fgRgb=(_)",       fn_Cell_fgRgb_set       },
	{ "Cell",  false, "bgPalette",       fn_Cell_bgPalette       },
	{ "Cell",  false, "bgPalette=(_)",   fn_Cell_bgPalette_set   },
	{ "Cell",  false, "bgRgb",           fn_Cell_bgRgb           },
	{ "Cell",  false, "bgRgb=(_)",       fn_Cell_bgRgb_set       },
	{ "Cell",  false, "hyperlinkId",     fn_Cell_hyperlinkId     },
	{ "Cell",  false, "hyperlinkId=(_)", fn_Cell_hyperlinkId_set },

	/* Cells (all instance) */
	{ "Cells", false, "count",            fn_Cells_count         },
	{ "Cells", false, "[_]",              fn_Cells_subscript     },
	{ "Cells", false, "iterate(_)",       fn_Cells_iterate       },
	{ "Cells", false, "iteratorValue(_)", fn_Cells_iteratorValue },

	/* Font (all static) */
	{ "Font",  true,  "name(_)",       fn_Font_name       },
	{ "Font",  true,  "count",         fn_Font_count      },
	{ "Font",  true,  "available(_)",  fn_Font_available  },
	{ "Font",  true,  "codepage",      fn_Font_codepage   },
	{ "Font",  true,  "codepageOf(_)", fn_Font_codepageOf },

	/* Hyperlinks (all static) */
	{ "Hyperlinks", true, "[_]",            fn_Hyperlinks_subscript   },
	{ "Hyperlinks", true, "containsKey(_)", fn_Hyperlinks_containsKey },
	{ "Hyperlinks", true, "add(_,_)",       fn_Hyperlinks_add         },
	{ "Hyperlinks", true, "params(_)",      fn_Hyperlinks_params      },

	/* Console */
	{ "Console", true, "count",            fn_Console_count         },
	{ "Console", true, "total",            fn_Console_total         },
	{ "Console", true, "[_]",              fn_Console_subscript     },
	{ "Console", true, "clear()",          fn_Console_clear         },
	{ "Console", true, "iterate(_)",       fn_Console_iterate       },
	{ "Console", true, "iteratorValue(_)", fn_Console_iteratorValue },

	/* Conn */
	{ "Conn",  true, "send(_)",        fn_Conn_send         },
	{ "Conn",  true, "sendRaw(_)",     fn_Conn_sendRaw      },
	{ "Conn",  true, "close()",        fn_Conn_close        },
	{ "Conn",  true, "connected",      fn_Conn_connected    },
	{ "Conn",  true, "type",           fn_Conn_type         },
	{ "Conn",  true, "inbufBytes",     fn_Conn_inbufBytes   },
	{ "Conn",  true, "outbufBytes",    fn_Conn_outbufBytes  },
	{ "Conn",  true, "inbufPeek(_)",   fn_Conn_inbufPeek    },
	{ "Conn",  true, "inbufGet(_)",    fn_Conn_inbufGet     },
	{ "Conn",  true, "outbufPut(_)",   fn_Conn_outbufPut    },

	/* CTerm */
	{ "CTerm", true, "emulation",      fn_CTerm_emulation   },
	{ "CTerm", true, "x",              fn_CTerm_x           },
	{ "CTerm", true, "y",              fn_CTerm_y           },
	{ "CTerm", true, "attr",           fn_CTerm_attr        },
	{ "CTerm", true, "doorwayMode",    fn_CTerm_doorwayMode },
	{ "CTerm", true, "music",          fn_CTerm_music       },
	{ "CTerm", true, "width",          fn_CTerm_width       },
	{ "CTerm", true, "height",         fn_CTerm_height      },
	{ "CTerm", true, "write(_)",       fn_CTerm_write       },

	/* BBS */
	{ "BBS",   true, "name",           fn_BBS_name          },
	{ "BBS",   true, "addr",           fn_BBS_addr          },
	{ "BBS",   true, "port",           fn_BBS_port          },
	{ "BBS",   true, "connType",       fn_BBS_connType      },
	{ "BBS",   true, "user",           fn_BBS_user          },
	{ "BBS",   true, "password",       fn_BBS_password      },
	{ "BBS",   true, "syspass",        fn_BBS_syspass       },
	{ "BBS",   true, "music",          fn_BBS_music         },
	{ "BBS",   true, "rip",            fn_BBS_rip           },
	{ "BBS",   true, "comment",        fn_BBS_comment       },

	/* Cache */
	{ "Cache", true, "basePath",       fn_Cache_basePath    },
	{ "Cache", true, "subdirPath(_)",  fn_Cache_subdirPath  },
	{ "Cache", true, "readFile(_)",    fn_Cache_readFile    },
	{ "Cache", true, "writeFile(_,_)", fn_Cache_writeFile   },

	/* Hook */
	{ "Hook",  true, "onKey(_)",       fn_Hook_onKey        },
	{ "Hook",  true, "onInput(_)",     fn_Hook_onInput      },
	{ "Hook",  true, "onOutput(_)",    fn_Hook_onOutput     },
	{ "Hook",  true, "onMouse(_)",     fn_Hook_onMouse      },
	{ "Hook",  true, "onStatus(_)",    fn_Hook_onStatus     },
	{ "Hook",  true, "every(_,_)",     fn_Hook_every        },
};

WrenForeignMethodFn
wren_bind_lookup(const char *module, const char *className, bool isStatic,
    const char *signature)
{
	if (module == NULL || strcmp(module, "syncterm") != 0)
		return NULL;
	for (size_t i = 0; i < sizeof(BINDINGS) / sizeof(BINDINGS[0]); i++) {
		if (BINDINGS[i].isStatic == isStatic &&
		    strcmp(BINDINGS[i].className, className) == 0 &&
		    strcmp(BINDINGS[i].signature, signature) == 0)
			return BINDINGS[i].fn;
	}
	return NULL;
}

WrenForeignClassMethods
wren_bind_lookup_class(const char *module, const char *className)
{
	WrenForeignClassMethods none = { NULL, NULL };
	if (module == NULL || strcmp(module, "syncterm") != 0)
		return none;
	if (strcmp(className, "Cell") == 0) {
		WrenForeignClassMethods m = {
			wren_cell_allocate, wren_cell_finalize
		};
		return m;
	}
	if (strcmp(className, "Cells") == 0) {
		WrenForeignClassMethods m = {
			wren_cells_allocate, wren_cells_finalize
		};
		return m;
	}
	if (strcmp(className, "CustomCursor") == 0) {
		WrenForeignClassMethods m = {
			wren_custom_cursor_allocate, wren_custom_cursor_finalize
		};
		return m;
	}
	if (strcmp(className, "VideoFlags") == 0) {
		WrenForeignClassMethods m = {
			wren_video_flags_allocate, wren_video_flags_finalize
		};
		return m;
	}
	if (strcmp(className, "KeyEvent") == 0) {
		WrenForeignClassMethods m = {
			wren_key_event_allocate, wren_key_event_finalize
		};
		return m;
	}
	if (strcmp(className, "MouseEvent") == 0) {
		WrenForeignClassMethods m = {
			wren_mouse_event_allocate, wren_mouse_event_finalize
		};
		return m;
	}
	return none;
}
