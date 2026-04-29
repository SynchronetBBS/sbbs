/* Foreign-method bindings for the Wren scripting host.  These wrap
 * SyncTERM's conio / connection / cterm / bbslist / cache APIs and
 * expose hook-registration entry points; lifecycle and dispatch live
 * in wren_host.c. */

#include "wren_host_internal.h"
#include "wren_host.h"
#include "wren.h"
#include "regexp.h"
#include <setjmp.h>
/* REPL.compile_ uses wrenCompileSource to produce an ObjClosure that
 * the Wren-side REPL.eval invokes via .call().  That path is internal
 * to Wren — wrenCompileSource isn't exposed in wren.h — so pull in
 * the VM headers directly.  Mirrors what wren_opt_meta.c does. */
#include "wren/vm/wren_vm.h"
#include "wren/vm/wren_value.h"

#include "ciolib.h"
#include "cterm.h"
#include "genwrap.h"      /* xp_timer */
#include "syncterm.h"
#include "bbslist.h"
#include "conn.h"
#include "term.h"
#include "utf8_codepages.h"
#include "comio.h"
#include "sftp.h"
#include "sftp_session.h"

#if !defined(_WIN32)
#include <unistd.h>     /* for _POSIX_VERSION */
#if defined(_POSIX_VERSION)
#include <sys/utsname.h>
#endif
#endif

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

/* dirwrap.h provides a portable opendir/readdir/closedir + struct dirent —
 * pulls in <dirent.h> on POSIX, supplies its own implementation for MSVC. */
#include <dirwrap.h>

/* MSVC's <sys/stat.h> ships only the _S_IF* constants; older Windows SDKs
 * don't define the POSIX S_ISREG macro at all. */
#ifndef S_ISREG
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

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

/* Discriminator for foreign objects in this binding.  Every wren_*
 * struct stamped at the front with one of these tags so foreign-method
 * bodies can identify *which* foreign they have — wrenGetSlotType
 * only tells us "WREN_TYPE_FOREIGN", not the class.  SWF_NONE = 0 so
 * an allocator that forgets to set the tag fails any specific check
 * loudly instead of pretending to be a different type. */
enum syncterm_wren_foreign {
	SWF_NONE = 0,
	SWF_KEY_EVENT,
	SWF_MOUSE_EVENT,
	SWF_EXTATTR,
	SWF_LAST_COLUMN_FLAG,
	SWF_FLOWCONTROL,
	SWF_DIRECTORY,
	SWF_FILE,
	SWF_CELL,
	SWF_CELLS,
	SWF_CUSTOM_CURSOR,
	SWF_VIDEO_FLAGS,
	SWF_HOOK_HANDLE,
	SWF_SFTP_ENTRY,
	SWF_SFTP_STAT,
	SWF_SFTP_HANDLE,
	SWF_SFTP_ERROR,
	SWF_TIMER_ELAPSED,
};

/* Every foreign struct's first field has this exact name and type, so
 * a (struct wren_foreign_header *) cast yields the tag regardless of
 * which struct we hold. */
struct wren_foreign_header {
	enum syncterm_wren_foreign type;
};

/* Returns the tag for the foreign object in `slot`, or SWF_NONE if
 * the slot doesn't hold a foreign value. */
static enum syncterm_wren_foreign
slot_foreign_type(WrenVM *vm, int slot)
{
	if (wrenGetSlotType(vm, slot) != WREN_TYPE_FOREIGN)
		return SWF_NONE;
	struct wren_foreign_header *h = wrenGetSlotForeign(vm, slot);
	return h->type;
}

/* Drop a foreign class from the syncterm module into `slot`, suitable
 * to hand to wrenSetSlotNewForeign as the class of a new instance.
 * The class handle is captured into `*cache` on first use and reused
 * thereafter — saves the symbol-table lookup on every allocation
 * without forcing the caller to populate the cache up front. Caller
 * must wrenEnsureSlots beforehand. */
static void
load_class_into_slot(WrenVM *vm, WrenHandle **cache, const char *name,
    int slot)
{
	if (*cache != NULL) {
		wrenSetSlotHandle(vm, slot, *cache);
		return;
	}
	wrenGetVariable(vm, "syncterm", name, slot);
	*cache = wrenGetSlotHandle(vm, slot);
}

/* Foreign class state for KeyEvent and MouseEvent — both are simple
 * value types (no parent, no resources) so finalize is a no-op. */
struct wren_key_event {
	enum syncterm_wren_foreign type;
	uint16_t code;
	int32_t  codepoint;   /* -1 = none (extended key) */
	uint8_t  text[5];     /* UTF-8 of codepoint, NUL-terminated; "" if extended */
	uint8_t  text_len;    /* not counting NUL */
};

struct wren_mouse_event {
	enum syncterm_wren_foreign type;
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
	ke->type      = SWF_KEY_EVENT;
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
	me->type           = SWF_MOUSE_EVENT;
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

static void
fn_KeyEvent_toString(WrenVM *vm)
{
	struct wren_key_event *ke = wrenGetSlotForeign(vm, 0);
	char buf[64];
	if (ke->text_len > 0 && ke->text[0] >= 0x20 && ke->text[0] < 0x7F)
		snprintf(buf, sizeof(buf), "KeyEvent('%s' 0x%04X)",
		    (const char *)ke->text, (unsigned)ke->code);
	else
		snprintf(buf, sizeof(buf), "KeyEvent(0x%04X)",
		    (unsigned)ke->code);
	wrenSetSlotString(vm, 0, buf);
}

static void
fn_MouseEvent_toString(WrenVM *vm)
{
	struct wren_mouse_event *me = wrenGetSlotForeign(vm, 0);
	char buf[96];
	if (me->ev.startx == me->ev.endx && me->ev.starty == me->ev.endy)
		snprintf(buf, sizeof(buf),
		    "MouseEvent(event=%d at %d,%d)",
		    me->ev.event, me->ev.startx, me->ev.starty);
	else
		snprintf(buf, sizeof(buf),
		    "MouseEvent(event=%d at %d,%d-%d,%d)",
		    me->ev.event, me->ev.startx, me->ev.starty,
		    me->ev.endx, me->ev.endy);
	wrenSetSlotString(vm, 0, buf);
}

/* Build a Wren-side KeyEvent in slot 0 from a raw 16-bit code.  Uses
 * the captured KeyEvent class handle; callers must have at least 2
 * slots ensured (slot 0 = result, slot 1 = scratch for the class). */
static void
push_key_event(WrenVM *vm, uint16_t code)
{
	struct wren_host_state *st = wren_host_state();
	load_class_into_slot(vm, &st->key_event_class, "KeyEvent", 1);
	struct wren_key_event *ke =
	    wrenSetSlotNewForeign(vm, 0, 1, sizeof(*ke));
	key_event_init(ke, code);
}

/* Build a Wren-side MouseEvent in slot 0 from a struct mouse_event. */
static void
push_mouse_event(WrenVM *vm, const struct mouse_event *mev)
{
	struct wren_host_state *st = wren_host_state();
	load_class_into_slot(vm, &st->mouse_event_class, "MouseEvent", 1);
	struct wren_mouse_event *me =
	    wrenSetSlotNewForeign(vm, 0, 1, sizeof(*me));
	me->type = SWF_MOUSE_EVENT;
	me->ev   = *mev;
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

/* Input.nextEvent(fiber) — register the fiber to receive the next
 * key or mouse event.  The fiber should yield right after (typically
 * the next statement, but it may fire other async ops first and then
 * yield in a loop demuxing results by type).  When an event arrives,
 * wren_bind_resume_parked_* picks the fiber up and calls
 * fiber.call(event), making the yield return that event.
 *
 * Throws if another fiber is already registered — single-subscriber
 * is a structural property of the framework today, so two parks is
 * a script bug. */
static void
fn_Input_nextEvent(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	if (st->parked_fiber != NULL) {
		wrenEnsureSlots(vm, 1);
		wrenSetSlotString(vm, 0, "Input.nextEvent: another fiber "
		    "is already registered for the next event");
		wrenAbortFiber(vm, 0);
		return;
	}
	st->parked_fiber = wrenGetSlotHandle(vm, 1);
	wrenSetSlotNull(vm, 0);
}

/* Result-queue payload for Input.nextEvent: enough state to build the
 * KeyEvent or MouseEvent foreign instance at delivery time.  Workers
 * never construct it (Input is owner-thread only), but it goes through
 * the same queue path as everything else for uniformity. */
struct input_result {
	bool                is_mouse;
	uint16_t            code;     /* is_mouse == false */
	struct mouse_event  ev;       /* is_mouse == true  */
};

static void
deliver_input_result(WrenVM *vm, int slot, void *data)
{
	struct wren_host_state *st = wren_host_state();
	struct input_result    *ir = data;
	if (ir->is_mouse) {
		load_class_into_slot(vm, &st->mouse_event_class,
		    "MouseEvent", slot + 1);
		struct wren_mouse_event *me =
		    wrenSetSlotNewForeign(vm, slot, slot + 1, sizeof(*me));
		me->type = SWF_MOUSE_EVENT;
		me->ev   = ir->ev;
	} else {
		load_class_into_slot(vm, &st->key_event_class,
		    "KeyEvent", slot + 1);
		struct wren_key_event *ke =
		    wrenSetSlotNewForeign(vm, slot, slot + 1, sizeof(*ke));
		key_event_init(ke, ir->code);
	}
}

bool
wren_bind_resume_parked_key(int code)
{
	struct wren_host_state *st = wren_host_state();
	if (st->parked_fiber == NULL || code == CIO_KEY_MOUSE)
		return false;
	WrenHandle *fiber = st->parked_fiber;
	st->parked_fiber  = NULL;

	struct input_result *ir = calloc(1, sizeof(*ir));
	if (ir == NULL) {
		wrenReleaseHandle(st->vm, fiber);
		return true;
	}
	ir->is_mouse = false;
	ir->code     = (uint16_t)code;
	wren_result_push(fiber, ir, deliver_input_result, free);
	return true;
}

bool
wren_bind_resume_parked_mouse(struct mouse_event *ev)
{
	struct wren_host_state *st = wren_host_state();
	if (st->parked_fiber == NULL || ev == NULL)
		return false;
	WrenHandle *fiber = st->parked_fiber;
	st->parked_fiber  = NULL;

	struct input_result *ir = calloc(1, sizeof(*ir));
	if (ir == NULL) {
		wrenReleaseHandle(st->vm, fiber);
		return true;
	}
	ir->is_mouse = true;
	ir->ev       = *ev;
	wren_result_push(fiber, ir, deliver_input_result, free);
	return true;
}

/* Input.next() — block until something is ready, return the event. */
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

/* Input.poll() — non-blocking; null when nothing is ready. */
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
	if (len <= 0)
		return;
	conn_send(s, (size_t)len, 0);
}

static void
fn_Conn_sendRaw(WrenVM *vm)
{
	int len = 0;
	const char *s = wrenGetSlotBytes(vm, 1, &len);
	if (len <= 0)
		return;
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
fn_Conn_pending(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)conn_buf_bytes(&conn_inbuf));
}

static void
fn_Conn_queued(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)conn_buf_bytes(&conn_outbuf));
}

static void
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

static void
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
	wrenSetSlotBool(vm, 0,
	    cterm != NULL && cterm->doorway_mode != 0);
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

/* CTerm.suspended — backed by a doterm() local that gates the
 * wire-byte pump.  When true, doterm() stops draining the conn buffer
 * so the script can do something the screen can't be allowed to be
 * scribbled on (modal dialog, transfer overlay, …) without the
 * remote's output painting underneath.  Bytes pile up in the conn
 * buffer; eventually the TCP window fills and the remote sees
 * backpressure.  Reads false when no flag is bound (pre-init or
 * post-shutdown). */
static void
fn_CTerm_suspended_get(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	bool v = (st != NULL && st->cterm_suspended != NULL)
	    ? *st->cterm_suspended : false;
	wrenSetSlotBool(vm, 0, v);
}

static void
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

static void fn_CTerm_originX(WrenVM *vm)        { CTERM_FIELD_NUM(x); }
static void fn_CTerm_originY(WrenVM *vm)        { CTERM_FIELD_NUM(y); }
static void fn_CTerm_topMargin(WrenVM *vm)      { CTERM_FIELD_NUM(top_margin); }
static void fn_CTerm_bottomMargin(WrenVM *vm)   { CTERM_FIELD_NUM(bottom_margin); }
static void fn_CTerm_leftMargin(WrenVM *vm)     { CTERM_FIELD_NUM(left_margin); }
static void fn_CTerm_rightMargin(WrenVM *vm)    { CTERM_FIELD_NUM(right_margin); }
static void fn_CTerm_fontSlot(WrenVM *vm)       { CTERM_FIELD_NUM(font_slot); }
static void fn_CTerm_scrollbackLines(WrenVM *vm){ CTERM_FIELD_NUM(backlines); }
static void fn_CTerm_scrollbackWidth(WrenVM *vm){ CTERM_FIELD_NUM(backwidth); }
static void fn_CTerm_scrollbackPos(WrenVM *vm)  { CTERM_FIELD_NUM(backpos); }
static void fn_CTerm_scrollbackStart(WrenVM *vm){ CTERM_FIELD_NUM(backstart); }
static void fn_CTerm_started(WrenVM *vm)        { CTERM_FIELD_BOOL(started); }
static void fn_CTerm_skypix(WrenVM *vm)         { CTERM_FIELD_BOOL(skypix); }
static void fn_CTerm_hasPaletteOverride(WrenVM *vm){ CTERM_FIELD_BOOL(has_palette_override); }
static void fn_CTerm_statusDisplay(WrenVM *vm)  { CTERM_FIELD_NUM(status_display_type); }

static void
fn_CTerm_fgColor(WrenVM *vm)
{
	uint32_t v = (cterm != NULL) ? (cterm->fg_color & 0xFFFFFFu) : 0;
	wrenSetSlotDouble(vm, 0, (double)v);
}

static void
fn_CTerm_bgColor(WrenVM *vm)
{
	uint32_t v = (cterm != NULL) ? (cterm->bg_color & 0xFFFFFFu) : 0;
	wrenSetSlotDouble(vm, 0, (double)v);
}

static void
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

static void
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

static void
fn_CTerm_logMode(WrenVM *vm)
{
	int v = (cterm != NULL) ? (cterm->log & CTERM_LOG_MASK) : 0;
	wrenSetSlotDouble(vm, 0, (double)v);
}

static void
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

static void
wren_extattr_allocate(WrenVM *vm)
{
	struct wren_extattr *e = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*e));
	e->type  = SWF_EXTATTR;
	e->value = 0;
}

static void
wren_extattr_finalize(void *data)
{
	(void)data;
}

static void
wren_last_column_flag_allocate(WrenVM *vm)
{
	struct wren_last_column_flag *l = wrenSetSlotNewForeign(vm, 0, 0,
	    sizeof(*l));
	l->type  = SWF_LAST_COLUMN_FLAG;
	l->value = 0;
}

static void
wren_last_column_flag_finalize(void *data)
{
	(void)data;
}

#define EXTATTR_BOOL(name, flag)                                        \
	static void fn_ExtAttr_##name(WrenVM *vm)                       \
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
	static void fn_LCF_##name(WrenVM *vm)                           \
	{                                                               \
		struct wren_last_column_flag *l = wrenGetSlotForeign(vm, 0); \
		wrenSetSlotBool(vm, 0, (l->value & (flag)) != 0);       \
	}

LCF_BOOL(set,     CTERM_LCF_SET)
LCF_BOOL(enabled, CTERM_LCF_ENABLED)
LCF_BOOL(forced,  CTERM_LCF_FORCED)

static void
fn_CTerm_extAttr(WrenVM *vm)
{
	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "syncterm", "ExtAttr", 1);
	struct wren_extattr *e = wrenSetSlotNewForeign(vm, 0, 1, sizeof(*e));
	e->type  = SWF_EXTATTR;
	e->value = (cterm != NULL) ? cterm->extattr : 0;
}

static void
fn_CTerm_lastColumnFlag(WrenVM *vm)
{
	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "syncterm", "LastColumnFlag", 1);
	struct wren_last_column_flag *l = wrenSetSlotNewForeign(vm, 0, 1,
	    sizeof(*l));
	l->type  = SWF_LAST_COLUMN_FLAG;
	l->value = (cterm != NULL) ? cterm->last_column_flag : 0;
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

#define BBS_FIELD_BOOL(field)                                           \
	do {                                                            \
		struct wren_host_state *st = wren_host_state();         \
		bool v = (st != NULL && st->bbs != NULL)                \
		    ? (bool)st->bbs->field : false;                     \
		wrenSetSlotBool(vm, 0, v);                              \
	} while (0)

static void fn_BBS_name(WrenVM *vm)         { BBS_FIELD_STR(name); }
static void fn_BBS_addr(WrenVM *vm)         { BBS_FIELD_STR(addr); }
static void fn_BBS_port(WrenVM *vm)         { BBS_FIELD_NUM(port); }
static void fn_BBS_connType(WrenVM *vm)     { BBS_FIELD_NUM(conn_type); }
static void fn_BBS_user(WrenVM *vm)         { BBS_FIELD_STR(user); }
static void fn_BBS_password(WrenVM *vm)     { BBS_FIELD_STR(password); }
static void fn_BBS_syspass(WrenVM *vm)      { BBS_FIELD_STR(syspass); }
static void fn_BBS_music(WrenVM *vm)        { BBS_FIELD_NUM(music); }
static void fn_BBS_rip(WrenVM *vm)          { BBS_FIELD_NUM(rip); }
static void fn_BBS_comment(WrenVM *vm)      { BBS_FIELD_STR(comment); }

static void fn_BBS_type(WrenVM *vm)         { BBS_FIELD_NUM(type); }
static void fn_BBS_id(WrenVM *vm)           { BBS_FIELD_NUM(id); }
static void fn_BBS_addressFamily(WrenVM *vm){ BBS_FIELD_NUM(address_family); }
static void fn_BBS_termName(WrenVM *vm)     { BBS_FIELD_STR(term_name); }
static void fn_BBS_screenMode(WrenVM *vm)   { BBS_FIELD_NUM(screen_mode); }
static void fn_BBS_bpsRate(WrenVM *vm)      { BBS_FIELD_NUM(bpsrate); }
static void fn_BBS_font(WrenVM *vm)         { BBS_FIELD_STR(font); }

static void fn_BBS_noStatus(WrenVM *vm)     { BBS_FIELD_BOOL(nostatus); }
static void fn_BBS_hidePopups(WrenVM *vm)   { BBS_FIELD_BOOL(hidepopups); }
static void fn_BBS_yellowIsYellow(WrenVM *vm){ BBS_FIELD_BOOL(yellow_is_yellow); }
static void fn_BBS_forceLcf(WrenVM *vm)     { BBS_FIELD_BOOL(force_lcf); }

static void fn_BBS_added(WrenVM *vm)        { BBS_FIELD_NUM(added); }
static void fn_BBS_connected(WrenVM *vm)    { BBS_FIELD_NUM(connected); }
static void fn_BBS_fastConnected(WrenVM *vm){ BBS_FIELD_NUM(fast_connected); }
static void fn_BBS_calls(WrenVM *vm)        { BBS_FIELD_NUM(calls); }

static void fn_BBS_dlDir(WrenVM *vm)        { BBS_FIELD_STR(dldir); }
static void fn_BBS_ulDir(WrenVM *vm)        { BBS_FIELD_STR(uldir); }
static void fn_BBS_logFile(WrenVM *vm)      { BBS_FIELD_STR(logfile); }
static void fn_BBS_appendLogFile(WrenVM *vm){ BBS_FIELD_BOOL(append_logfile); }
static void fn_BBS_xferLogLevel(WrenVM *vm) { BBS_FIELD_NUM(xfer_loglevel); }
static void fn_BBS_telnetLogLevel(WrenVM *vm){ BBS_FIELD_NUM(telnet_loglevel); }

static void fn_BBS_stopBits(WrenVM *vm)     { BBS_FIELD_NUM(stop_bits); }
static void fn_BBS_dataBits(WrenVM *vm)     { BBS_FIELD_NUM(data_bits); }
static void fn_BBS_parity(WrenVM *vm)       { BBS_FIELD_NUM(parity); }
struct wren_flowcontrol {
	enum syncterm_wren_foreign type;
	uint32_t value;
};

static void
wren_flowcontrol_allocate(WrenVM *vm)
{
	struct wren_flowcontrol *f = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*f));
	f->type  = SWF_FLOWCONTROL;
	f->value = 0;
}

static void
wren_flowcontrol_finalize(void *data)
{
	(void)data;
}

static void
fn_FlowControl_rtsCts(WrenVM *vm)
{
	struct wren_flowcontrol *f = wrenGetSlotForeign(vm, 0);
	wrenSetSlotBool(vm, 0, (f->value & COM_FLOW_CONTROL_RTS_CTS) != 0);
}

static void
fn_FlowControl_xonOff(WrenVM *vm)
{
	struct wren_flowcontrol *f = wrenGetSlotForeign(vm, 0);
	wrenSetSlotBool(vm, 0, (f->value & COM_FLOW_CONTROL_XON_OFF) != 0);
}

static void
fn_BBS_flowControl(WrenVM *vm)
{
	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "syncterm", "FlowControl", 1);
	struct wren_flowcontrol *f = wrenSetSlotNewForeign(vm, 0, 1,
	    sizeof(*f));
	struct wren_host_state *st = wren_host_state();
	f->type  = SWF_FLOWCONTROL;
	f->value = (st != NULL && st->bbs != NULL)
	    ? (uint32_t)st->bbs->flow_control : 0;
}

static void fn_BBS_telnetNoBinary(WrenVM *vm){ BBS_FIELD_BOOL(telnet_no_binary); }
static void fn_BBS_deferTelnetNegotiation(WrenVM *vm){ BBS_FIELD_BOOL(defer_telnet_negotiation); }

static void fn_BBS_ghostProgram(WrenVM *vm) { BBS_FIELD_STR(ghost_program); }
static void fn_BBS_sftpPublicKey(WrenVM *vm){ BBS_FIELD_BOOL(sftp_public_key); }
static void fn_BBS_sshFingerprintLen(WrenVM *vm){ BBS_FIELD_NUM(ssh_fingerprint_len); }
static void fn_BBS_sortOrder(WrenVM *vm)    { BBS_FIELD_NUM(sort_order); }

static void
fn_BBS_sshFingerprint(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	if (st != NULL && st->bbs != NULL && st->bbs->ssh_fingerprint_len > 0) {
		wrenSetSlotBytes(vm, 0,
		    (const char *)st->bbs->ssh_fingerprint,
		    st->bbs->ssh_fingerprint_len);
	}
	else {
		wrenSetSlotBytes(vm, 0, "", 0);
	}
}

static void
fn_BBS_palette(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
	if (st == NULL || st->bbs == NULL)
		return;
	unsigned n = st->bbs->palette_size;
	const unsigned cap = sizeof(st->bbs->palette) / sizeof(st->bbs->palette[0]);
	if (n > cap)
		n = cap;
	for (unsigned i = 0; i < n; i++) {
		uint32_t rgb = st->bbs->palette[i] & 0xFFFFFFu;
		wrenSetSlotDouble(vm, 1, (double)rgb);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

static void fn_BBS_paletteSize(WrenVM *vm)  { BBS_FIELD_NUM(palette_size); }

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

static void
wren_throw(WrenVM *vm, const char *msg)
{
	wrenEnsureSlots(vm, 1);
	wrenSetSlotString(vm, 0, msg);
	wrenAbortFiber(vm, 0);
}

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

static void
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

static void
wren_directory_finalize(void *data)
{
	struct wren_directory *wd = data;
	fs_unregister_dir(wd);
}

static void
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

static void
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
	struct stat sb;
	bool yes = (stat(p, &sb) == 0) && S_ISREG(sb.st_mode);
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

static void
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

static void
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

static void
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

static void
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

static void
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
/* Platform.name — OS identifier for platform-specific script logic.
 * Returns "Windows" on Windows, the uname(2) sysname on POSIX, and
 * "Unknown" on anything else (DOS, OS/2, …).  Win32 is checked
 * before _POSIX_VERSION because POSIX layers like Cygwin / MSYS
 * define both — we want the native-OS answer there.  Called
 * rarely; not worth memoizing on the Wren side. */
static void
fn_Platform_name(WrenVM *vm)
{
	wrenEnsureSlots(vm, 1);
#if defined(_WIN32)
	wrenSetSlotString(vm, 0, "Windows");
#elif defined(_POSIX_VERSION)
	struct utsname uts;
	if (uname(&uts) == 0)
		wrenSetSlotString(vm, 0, uts.sysname);
	else
		wrenSetSlotString(vm, 0, "Unknown");
#else
	wrenSetSlotString(vm, 0, "Unknown");
#endif
}

static void
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

static void
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

static void
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

static void
fn_File_readBytes_1(WrenVM *vm)
{
	struct wren_file *wf = file_check_open(vm);
	if (wf == NULL)
		return;
	long count = (long)wrenGetSlotDouble(vm, 1);
	long off   = ftell(wf->fp);
	do_read_at(vm, wf, off, count, true);
}

static void
fn_File_readBytes_2(WrenVM *vm)
{
	struct wren_file *wf = file_check_open(vm);
	if (wf == NULL)
		return;
	long count = (long)wrenGetSlotDouble(vm, 1);
	long off   = (long)wrenGetSlotDouble(vm, 2);
	do_read_at(vm, wf, off, count, false);
}

static void
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

static void
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

static void
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
static void
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
static void
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
static void
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

static void
fn_File_offset_get(WrenVM *vm)
{
	struct wren_file *wf = file_check_open(vm);
	if (wf == NULL)
		return;
	wrenSetSlotDouble(vm, 0, (double)ftell(wf->fp));
}

static void
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

static void
fn_File_size(WrenVM *vm)
{
	struct wren_file *wf = file_check(vm);
	if (wf == NULL)
		return;
	if (wf->fp != NULL) {
		wrenSetSlotDouble(vm, 0, (double)file_size_now(wf));
		return;
	}
	struct stat sb;
	if (stat(wf->path, &sb) != 0) {
		wren_throw(vm, "File: stat failed");
		return;
	}
	wrenSetSlotDouble(vm, 0, (double)sb.st_size);
}

static void
fn_File_isOpen(WrenVM *vm)
{
	struct wren_file *wf = file_check(vm);
	if (wf == NULL)
		return;
	wrenSetSlotBool(vm, 0, wf->fp != NULL);
}

/* toString skips the usual file_check / dir_check guards so a dead
 * handle prints "(dead)" instead of aborting the fiber. */
static void
fn_File_toString(WrenVM *vm)
{
	struct wren_file *wf = wrenGetSlotForeign(vm, 0);
	const char *suffix = wf->dead ? " (dead)" :
	                     (wf->fp != NULL ? " (open)" : "");
	char buf[MAX_PATH + 16];
	snprintf(buf, sizeof(buf), "%s%s", wf->path, suffix);
	wrenSetSlotString(vm, 0, buf);
}

static void
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

static void
fn_Console_markSeen(WrenVM *vm)
{
	(void)vm;
	wren_host_mark_log_seen();
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

/* Forward-declare; full definitions live with the rest of the
 * HookHandle bindings below.  Each foreign Hook.on* / Hook.every
 * method returns a HookHandle in slot 0 (or null on failure). */
static void push_hook_handle(WrenVM *vm, struct wren_hook_entry *h);

static void
fn_Hook_onKey(WrenVM *vm)
{
	push_hook_handle(vm,
	    wren_host_register_hook(vm, WREN_HOOK_KEY, 1));
}
static void
fn_Hook_onInput(WrenVM *vm)
{
	push_hook_handle(vm,
	    wren_host_register_hook(vm, WREN_HOOK_INPUT, 1));
}
static void
fn_Hook_onMouse(WrenVM *vm)
{
	push_hook_handle(vm,
	    wren_host_register_hook(vm, WREN_HOOK_MOUSE, 1));
}
static void
fn_Hook_onStatus(WrenVM *vm)
{
	push_hook_handle(vm,
	    wren_host_register_hook(vm, WREN_HOOK_STATUS, 1));
}

/* Filtered variants: signature is (filter, fn) so slot 1 is the
 * match value (key code / byte / mouse-event type) and slot 2 is the
 * callable.  Same per-event array as the unfiltered forms; dispatch
 * order = registration order. */
static void
fn_Hook_onKey_filtered(WrenVM *vm)
{
	int filter = (int)wrenGetSlotDouble(vm, 1);
	push_hook_handle(vm,
	    wren_host_register_hook_filtered(vm, WREN_HOOK_KEY, 2, filter));
}
static void
fn_Hook_onInput_filtered(WrenVM *vm)
{
	int filter = (int)wrenGetSlotDouble(vm, 1) & 0xff;
	push_hook_handle(vm,
	    wren_host_register_hook_filtered(vm, WREN_HOOK_INPUT, 2, filter));
}
static void
fn_Hook_onMouse_filtered(WrenVM *vm)
{
	int filter = (int)wrenGetSlotDouble(vm, 1);
	push_hook_handle(vm,
	    wren_host_register_hook_filtered(vm, WREN_HOOK_MOUSE, 2, filter));
}

/* RE1 calls fatal() — and thus exit(2) — on syntax errors and
 * internal asserts.  Catch via the host-trap hook in regexp.h:
 * stash the message, longjmp back to fn_Hook_onMatch, convert into
 * Fiber.abort so the script's stack trace points at the bad
 * registration site. */
#define WREN_REGEX_BUF_CAP 4096

static jmp_buf re1_jmp;
static char    re1_errmsg[256];

static void
on_re1_fatal(const char *msg)
{
	strncpy(re1_errmsg, msg, sizeof re1_errmsg - 1);
	re1_errmsg[sizeof re1_errmsg - 1] = '\0';
	longjmp(re1_jmp, 1);
}

/* The dispatcher's IMPOSSIBLE-shift trick depends on every byte
 * either advancing the NFA or being trimmed.  A pattern whose
 * leading construct can match without consuming a byte (any of *, +,
 * ?) keeps threads alive forever on `.`-style fillers, the buffer
 * grows past its cap, and matches start being silently dropped.
 * Reject such patterns at registration time.  A leading literal byte
 * or `.` is fine; an alt is fine if every branch is fine; quantified
 * leading constructs are not. */
static const char *
regex_anchor_check(Regexp *r)
{
	const char *err;

	if (r == NULL)
		return "empty pattern";
	switch (r->type) {
	case Lit:
	case Dot:
		return NULL;
	case Cat:
		return regex_anchor_check(r->left);
	case Paren:
		return regex_anchor_check(r->left);
	case Alt:
		err = regex_anchor_check(r->left);
		if (err != NULL)
			return err;
		return regex_anchor_check(r->right);
	case Star:
	case Plus:
	case Quest:
		return "pattern must start with a fixed-character anchor; "
		       "leading * + ? quantifiers prevent buffer trimming";
	}
	return "unknown regex node type";
}

static void
fn_Hook_onMatch(WrenVM *vm)
{
	const char *pat = wrenGetSlotString(vm, 1);
	if (pat == NULL) {
		wrenSetSlotString(vm, 0, "Hook.onMatch: pattern must be a String");
		wrenAbortFiber(vm, 0);
		return;
	}

	/* RE1's parser is destructive on its argument? No — parse() reads
	 * the string but doesn't write to it.  Still, copy into a mutable
	 * buffer so we don't depend on Wren's internal string lifetime. */
	size_t plen = strlen(pat);
	char *patcopy = malloc(plen + 1);
	if (patcopy == NULL) {
		wrenSetSlotString(vm, 0, "Hook.onMatch: out of memory");
		wrenAbortFiber(vm, 0);
		return;
	}
	memcpy(patcopy, pat, plen + 1);

	Regexp *r = NULL;
	Prog   *p = NULL;
	re1_errmsg[0] = '\0';
	re1_fatal_handler = on_re1_fatal;
	if (setjmp(re1_jmp) != 0) {
		/* RE1 fatal() landed here. */
		re1_fatal_handler = NULL;
		free(patcopy);
		char buf[320];
		snprintf(buf, sizeof buf, "Hook.onMatch: %s", re1_errmsg);
		wrenSetSlotString(vm, 0, buf);
		wrenAbortFiber(vm, 0);
		return;
	}
	r = parse_unanchored(patcopy);
	const char *aerr = regex_anchor_check(r);
	if (aerr != NULL) {
		re1_fatal_handler = NULL;
		free(patcopy);
		char buf[320];
		snprintf(buf, sizeof buf, "Hook.onMatch: %s", aerr);
		wrenSetSlotString(vm, 0, buf);
		wrenAbortFiber(vm, 0);
		return;
	}
	p = compile(r);
	re1_fatal_handler = NULL;
	free(patcopy);

	struct PikeVM *pvm = pikevm_new(p, MAXSUB);
	char *buf = malloc(WREN_REGEX_BUF_CAP);
	if (buf == NULL) {
		pikevm_free(pvm);
		free(p);
		wrenSetSlotString(vm, 0, "Hook.onMatch: out of memory");
		wrenAbortFiber(vm, 0);
		return;
	}
	buf[0] = '\0';

	struct wren_hook_entry *h = wren_host_register_hook_match(vm, 2,
	    p, pvm, buf, WREN_REGEX_BUF_CAP, MAXSUB);
	if (h == NULL) {
		pikevm_free(pvm);
		free(p);
		free(buf);
		wrenSetSlotString(vm, 0, "Hook.onMatch: hook limit reached");
		wrenAbortFiber(vm, 0);
		return;
	}
	push_hook_handle(vm, h);
}

static void
fn_Hook_every(WrenVM *vm)
{
	int ms = (int)wrenGetSlotDouble(vm, 1);
	struct wren_hook_entry *t = wren_host_register_timer(vm, ms, 2);
	push_hook_handle(vm, t);
}

/* ----- HookHandle -------------------------------------------------- */

struct wren_hook_handle {
	enum syncterm_wren_foreign type;
	struct wren_hook_entry    *entry;
};

/* No Wren-side `construct new()` — HookHandle is only ever produced
 * by a successful Hook registration and handed back from C.  The
 * allocator below should never run; if Wren bytecode somehow does
 * invoke it, hand back a zeroed handle whose remove() is a no-op
 * rather than something pointing at a real entry. */
static void
wren_hook_handle_allocate(WrenVM *vm)
{
	struct wren_hook_handle *hh = wrenSetSlotNewForeign(vm, 0, 0,
	    sizeof(*hh));
	memset(hh, 0, sizeof(*hh));
	hh->type = SWF_HOOK_HANDLE;
}

/* Wren has just GC'd the HookHandle.  Set the entry's `gced` bit so
 * compact (or shutdown) knows nobody references the struct anymore;
 * if the entry is also already `unregistered` (compact shifted it out
 * of the dispatch array), free it now. */
static void
wren_hook_handle_finalize(void *data)
{
	struct wren_hook_handle *hh = data;
	if (hh == NULL)
		return;
	struct wren_hook_entry *e = hh->entry;
	hh->entry = NULL;
	if (e == NULL)
		return;
	e->gced = true;
	if (e->unregistered)
		free(e);
}

/* Allocate a HookHandle in slot 0 wrapping the freshly-registered
 * entry (or null if registration failed).  The pointer stored in the
 * handle stays valid until the entry's compaction completes AND the
 * handle is GC'd — heap-allocated entries plus the two-bit lifetime
 * flags keep the struct alive across either-order teardown. */
static void
push_hook_handle(WrenVM *vm, struct wren_hook_entry *h)
{
	if (h == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	struct wren_host_state *st = wren_host_state();
	wrenEnsureSlots(vm, 2);
	load_class_into_slot(vm, &st->hook_handle_class, "HookHandle", 1);
	struct wren_hook_handle *hh = wrenSetSlotNewForeign(vm, 0, 1,
	    sizeof(*hh));
	hh->type  = SWF_HOOK_HANDLE;
	hh->entry = h;
}

static void
fn_HookHandle_remove(WrenVM *vm)
{
	struct wren_hook_handle *hh = wrenGetSlotForeign(vm, 0);
	bool removed = wren_host_remove_entry(hh->entry);
	wrenSetSlotBool(vm, 0, removed);
}

static const struct hook_metrics *
hook_handle_metrics(struct wren_hook_handle *hh)
{
	if (hh == NULL || hh->entry == NULL)
		return NULL;
	return &hh->entry->metrics;
}

static void
fn_HookHandle_callCount(WrenVM *vm)
{
	const struct hook_metrics *m =
	    hook_handle_metrics(wrenGetSlotForeign(vm, 0));
	wrenSetSlotDouble(vm, 0, m != NULL ? (double)m->call_count : 0.0);
}

static void
fn_HookHandle_totalRuntime(WrenVM *vm)
{
	const struct hook_metrics *m =
	    hook_handle_metrics(wrenGetSlotForeign(vm, 0));
	wrenSetSlotDouble(vm, 0, m != NULL ? (double)m->total_runtime_s : 0.0);
}

static void
fn_HookHandle_minRuntime(WrenVM *vm)
{
	const struct hook_metrics *m =
	    hook_handle_metrics(wrenGetSlotForeign(vm, 0));
	wrenSetSlotDouble(vm, 0, m != NULL ? (double)m->min_runtime_s : 0.0);
}

static void
fn_HookHandle_maxRuntime(WrenVM *vm)
{
	const struct hook_metrics *m =
	    hook_handle_metrics(wrenGetSlotForeign(vm, 0));
	wrenSetSlotDouble(vm, 0, m != NULL ? (double)m->max_runtime_s : 0.0);
}

static void
fn_HookHandle_toString(WrenVM *vm)
{
	const struct hook_metrics *m =
	    hook_handle_metrics(wrenGetSlotForeign(vm, 0));
	uint64_t calls = m != NULL ? m->call_count : 0;
	char buf[48];
	snprintf(buf, sizeof(buf), "HookHandle(calls=%" PRIu64 ")", calls);
	wrenSetSlotString(vm, 0, buf);
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
	enum syncterm_wren_foreign type;
	int               count;
	struct vmem_cell *buf;          /* malloc'd; freed at finalize */
};

struct wren_cell {
	enum syncterm_wren_foreign type;
	struct vmem_cell   c;             /* used when standalone */
	int                index;         /* index into parent_buf in view mode */
	struct vmem_cell  *parent_buf;    /* parent Cells' malloc'd buf (stable
	                                     while parent_handle is held — the
	                                     handle pins the Cells, the buf is
	                                     malloc'd, never reallocated, freed
	                                     only at the Cells' finalize) */
	WrenHandle        *parent_handle; /* pin on parent Cells; NULL = standalone */
};

/* Standalone cells return their own embedded `c`; view-mode cells use
 * their captured malloc'd buffer.  Both pointers are stable across
 * arbitrary VM calls — `parent_buf` is a malloc'd region the parent
 * Cells doesn't reallocate, kept alive via `parent_handle`. */
static struct vmem_cell *
cell_data(struct wren_cell *wc)
{
	return (wc->parent_handle != NULL)
	    ? &wc->parent_buf[wc->index]
	    : &wc->c;
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
	wc->type = SWF_CELL;
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
	cs->type  = SWF_CELLS;
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
	enum syncterm_wren_foreign type;
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
	c->type = SWF_CUSTOM_CURSOR;
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
	c->type      = SWF_CUSTOM_CURSOR;
	c->startline = sl;
	c->endline   = el;
	c->range     = r;
	c->blink     = b;
	c->visible   = v;
}

/* ----- VideoFlags -------------------------------------------------- */

struct wren_video_flags {
	enum syncterm_wren_foreign type;
	int flags;
};

static void
wren_video_flags_allocate(WrenVM *vm)
{
	struct wren_video_flags *vf = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*vf));
	vf->type  = SWF_VIDEO_FLAGS;
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
	if (st == NULL) {
		free(buf);
		wrenSetSlotNull(vm, 0);
		return;
	}

	wrenEnsureSlots(vm, 2);
	load_class_into_slot(vm, &st->cells_class, "Cells", 1);
	struct wren_cells *cs =
	    wrenSetSlotNewForeign(vm, 0, 1, sizeof(*cs));
	cs->type  = SWF_CELLS;
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

/* REPL.modules — list of every module currently loaded into the VM
 * (including "core", which Wren creates for its built-in classes).
 * Walks vm->modules directly because the public C API doesn't expose
 * an iterator.  Skips empty slots and tombstones (key is UNDEFINED_VAL
 * for both); non-string keys are skipped as a defensive measure but
 * shouldn't occur — the modules map is keyed by ObjString. */
static void
fn_REPL_modules(WrenVM *vm)
{
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
	if (vm->modules == NULL)
		return;
	for (uint32_t i = 0; i < vm->modules->capacity; i++) {
		MapEntry *e = &vm->modules->entries[i];
		if (IS_UNDEFINED(e->key) || !IS_STRING(e->key))
			continue;
		ObjString *s = AS_STRING(e->key);
		wrenSetSlotBytes(vm, 1, s->value, s->length);
		wrenInsertInList(vm, 0, -1, 1);
	}
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
	int      n_expected = (ex - sx + 1) * (ey - sy + 1);
	WrenType arg_type   = wrenGetSlotType(vm, 5);

	/* Cells: contiguous vmem_cell buffer — hand it straight to puttext
	 * with no allocation or copy. */
	if (arg_type == WREN_TYPE_FOREIGN
	    && slot_foreign_type(vm, 5) == SWF_CELLS) {
		struct wren_cells *cs = wrenGetSlotForeign(vm, 5);
		if (cs->count != n_expected) {
			wrenEnsureSlots(vm, 1);
			wrenSetSlotString(vm, 0,
			    "putText: Cells length does not match region size");
			wrenAbortFiber(vm, 0);
			return;
		}
		int rv = ciolib_vmem_puttext(sx, sy, ex, ey, cs->buf);
		wrenSetSlotBool(vm, 0, rv != 0);
		return;
	}

	/* List of Cell: gather into a temporary contiguous buffer. */
	if (arg_type != WREN_TYPE_LIST) {
		wrenEnsureSlots(vm, 1);
		wrenSetSlotString(vm, 0,
		    "putText: 5th argument must be a Cells or List of Cell");
		wrenAbortFiber(vm, 0);
		return;
	}
	int n_got = wrenGetListCount(vm, 5);
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
		if (slot_foreign_type(vm, 6) != SWF_CELL) {
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
fn_Cell_toString(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	char buf[48];
	snprintf(buf, sizeof(buf), "Cell(0x%02X attr=0x%02X)",
	    (unsigned)d->ch, (unsigned)d->legacy_attr);
	wrenSetSlotString(vm, 0, buf);
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
	if (st == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	/* Capture cs->buf before the slot-0 overwrite — it's a malloc'd
	 * pointer that stays valid as long as the parent Cells lives,
	 * which we ensure by pinning it via parent_handle. */
	struct vmem_cell *parent_buf    = cs->buf;
	WrenHandle       *parent_handle = wrenGetSlotHandle(vm, 0);

	wrenEnsureSlots(vm, 3);
	load_class_into_slot(vm, &st->cell_class, "Cell", 2);
	struct wren_cell *wc = wrenSetSlotNewForeign(vm, 0, 2, sizeof(*wc));
	memset(wc, 0, sizeof(*wc));
	wc->type          = SWF_CELL;
	wc->index         = idx;
	wc->parent_buf    = parent_buf;
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

static void
fn_Cells_toString(WrenVM *vm)
{
	struct wren_cells *cs = wrenGetSlotForeign(vm, 0);
	char buf[32];
	snprintf(buf, sizeof(buf), "Cells(count=%d)", cs->count);
	wrenSetSlotString(vm, 0, buf);
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

/* ----- SFTP --------------------------------------------------------
 *
 * The SFTP class (static-only) and four result foreign classes —
 * SFTPEntry, SFTPStat, SFTPHandle, SFTPError.  Phase 2 wires up the
 * skeleton plus the two synchronous getters (`available`, `pubdir`);
 * Phases 3-5 add the parking ops that produce and consume these
 * foreigns.  Allocators just stamp the type tag and zero-init —
 * fields are populated by the deliver functions reading off the
 * pending pointer handed to them via the result-queue framework.
 * -------------------------------------------------------------------- */

struct wren_sftp_entry {
	enum syncterm_wren_foreign type;
	char     *name;       /* malloc'd, NUL-terminated */
	char     *longname;   /* malloc'd, NUL-terminated; NULL if no lname */
	uint64_t  size;
	uint32_t  mtime;
	bool      is_dir;
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

static void
wren_sftp_entry_allocate(WrenVM *vm)
{
	struct wren_sftp_entry *e = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*e));
	memset(e, 0, sizeof(*e));
	e->type = SWF_SFTP_ENTRY;
}

static void
wren_sftp_entry_finalize(void *data)
{
	struct wren_sftp_entry *e = data;
	free(e->name);
	free(e->longname);
	free(e->hash);
}

static void
wren_sftp_stat_allocate(WrenVM *vm)
{
	struct wren_sftp_stat *s = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*s));
	memset(s, 0, sizeof(*s));
	s->type = SWF_SFTP_STAT;
}

static void
wren_sftp_stat_finalize(void *data)
{
	(void)data;
}

static void
wren_sftp_handle_allocate(WrenVM *vm)
{
	struct wren_sftp_handle *h = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*h));
	memset(h, 0, sizeof(*h));
	h->type = SWF_SFTP_HANDLE;
}

/* Best-effort close on GC.  Scripts should call SFTP.close explicitly;
 * this is a safety net for handles that go unreferenced.  Status reply
 * is discarded. */
static void
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

static void
wren_sftp_error_allocate(WrenVM *vm)
{
	struct wren_sftp_error *e = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*e));
	memset(e, 0, sizeof(*e));
	e->type = SWF_SFTP_ERROR;
}

static void
wren_sftp_error_finalize(void *data)
{
	struct wren_sftp_error *e = data;
	free(e->message);
}

/* ----- SFTP static getters ---------------------------------------- */

static void
fn_SFTP_available(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, sftp_available && sftp_state != NULL);
}

static void
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

/* ----- SFTPEntry field accessors ---------------------------------- */

static void
fn_SFTPEntry_name(WrenVM *vm)
{
	struct wren_sftp_entry *e = wrenGetSlotForeign(vm, 0);
	if (e->name == NULL)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotString(vm, 0, e->name);
}

static void
fn_SFTPEntry_longname(WrenVM *vm)
{
	struct wren_sftp_entry *e = wrenGetSlotForeign(vm, 0);
	if (e->longname == NULL)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotString(vm, 0, e->longname);
}

static void
fn_SFTPEntry_size(WrenVM *vm)
{
	struct wren_sftp_entry *e = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)e->size);
}

static void
fn_SFTPEntry_mtime(WrenVM *vm)
{
	struct wren_sftp_entry *e = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)e->mtime);
}

static void
fn_SFTPEntry_isDir(WrenVM *vm)
{
	struct wren_sftp_entry *e = wrenGetSlotForeign(vm, 0);
	wrenSetSlotBool(vm, 0, e->is_dir);
}

static void
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

static void
fn_SFTPStat_size(WrenVM *vm)
{
	struct wren_sftp_stat *s = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)s->size);
}

static void
fn_SFTPStat_mtime(WrenVM *vm)
{
	struct wren_sftp_stat *s = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)s->mtime);
}

static void
fn_SFTPStat_atime(WrenVM *vm)
{
	struct wren_sftp_stat *s = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)s->atime);
}

static void
fn_SFTPStat_mode(WrenVM *vm)
{
	struct wren_sftp_stat *s = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)s->mode);
}

static void
fn_SFTPStat_uid(WrenVM *vm)
{
	struct wren_sftp_stat *s = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)s->uid);
}

static void
fn_SFTPStat_gid(WrenVM *vm)
{
	struct wren_sftp_stat *s = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)s->gid);
}

/* ----- SFTPError field accessors ---------------------------------- */

static void
fn_SFTPError_code(WrenVM *vm)
{
	struct wren_sftp_error *e = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)e->code);
}

static void
fn_SFTPError_message(WrenVM *vm)
{
	struct wren_sftp_error *e = wrenGetSlotForeign(vm, 0);
	if (e->message == NULL)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotString(vm, 0, e->message);
}

static void
fn_SFTPError_isTransient(WrenVM *vm)
{
	struct wren_sftp_error *e = wrenGetSlotForeign(vm, 0);
	wrenSetSlotBool(vm, 0, e->transient);
}

static void
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

static void
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

static void
fn_SFTPStat_toString(WrenVM *vm)
{
	struct wren_sftp_stat *s = wrenGetSlotForeign(vm, 0);
	char buf[128];
	snprintf(buf, sizeof(buf),
	    "SFTPStat(size=%" PRIu64 ", mode=0%o, mtime=%" PRIu32 ")",
	    s->size, s->mode, s->mtime);
	wrenSetSlotString(vm, 0, buf);
}

static void
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

static void
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

static void
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
 * scratch for the class lookup. */
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
	e->longname = sftp_str_to_cstr(de->longname);
	if (de->attrs != NULL) {
		sftp_fattr_get_size(de->attrs,  &e->size);
		sftp_fattr_get_mtime(de->attrs, &e->mtime);
		uint32_t perm;
		if (sftp_fattr_get_permissions(de->attrs, &perm))
			e->is_dir = (perm & S_IFMT) == S_IFDIR;
		e->hash = sftp_attr_extract_hash(de->attrs, &e->hash_len);
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

static void
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

static void
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

static void
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

static void
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

static void
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
	uint64_t offset = (uint64_t)wrenGetSlotDouble(vm, 3);
	uint32_t count  = (uint32_t)wrenGetSlotDouble(vm, 4);
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

static void
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

static void
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

static void
fn_SFTP_rmdir(WrenVM *vm)
{
	struct sftp_call_ctx *ctx = sftp_call_prelude(vm, sftp_status_deliver);
	if (ctx == NULL)
		return;
	const char *path = wrenGetSlotString(vm, 2);
	sftpc_rmdir(sftp_state, path, sftp_call_cb, ctx);
	wrenSetSlotNull(vm, 0);
}

static void
fn_SFTP_remove(WrenVM *vm)
{
	struct sftp_call_ctx *ctx = sftp_call_prelude(vm, sftp_status_deliver);
	if (ctx == NULL)
		return;
	const char *path = wrenGetSlotString(vm, 2);
	sftpc_remove(sftp_state, path, sftp_call_cb, ctx);
	wrenSetSlotNull(vm, 0);
}

static void
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

/* ----- Timer -------------------------------------------------------
 *
 * One-shot fiber resumption after a delay.  The foreign captures the
 * fiber and an absolute due-time; doterm()'s sweep pushes a
 * TimerElapsed onto the result queue once xp_timer() reaches that
 * time.  Multiple pending entries per fiber are fine — each yields
 * one event independently.
 *
 * Recurring timers belong on `Hook.every(ms, fn)`; that's a
 * fire-and-forget callback, no fiber resume.
 * -------------------------------------------------------------------- */

struct wren_timer_elapsed {
	enum syncterm_wren_foreign type;
};

static void
wren_timer_elapsed_allocate(WrenVM *vm)
{
	struct wren_timer_elapsed *te =
	    wrenSetSlotNewForeign(vm, 0, 0, sizeof(*te));
	te->type = SWF_TIMER_ELAPSED;
}

static void
wren_timer_elapsed_finalize(void *data)
{
	(void)data;
}

static void
deliver_timer_elapsed(WrenVM *vm, int slot, void *data)
{
	(void)data;
	struct wren_host_state *st = wren_host_state();
	wrenEnsureSlots(vm, slot + 2);
	load_class_into_slot(vm, &st->timer_elapsed_class, "TimerElapsed",
	    slot + 1);
	struct wren_timer_elapsed *te =
	    wrenSetSlotNewForeign(vm, slot, slot + 1, sizeof(*te));
	te->type = SWF_TIMER_ELAPSED;
}

static void
fn_Timer_trigger(WrenVM *vm)
{
	if (wrenGetSlotType(vm, 2) != WREN_TYPE_NUM) {
		wren_throw(vm, "Timer.trigger: ms must be a number");
		return;
	}
	double ms = wrenGetSlotDouble(vm, 2);
	if (ms < 0) {
		wren_throw(vm, "Timer.trigger: ms must be non-negative");
		return;
	}
	struct wren_host_state *st = wren_host_state();
	if (st->pending_timer_count >= WREN_HOST_MAX_PENDING_TIMERS) {
		wren_throw(vm, "Timer.trigger: too many pending timers");
		return;
	}
	struct wren_pending_timer *t =
	    &st->pending_timers[st->pending_timer_count++];
	t->fiber = wrenGetSlotHandle(vm, 1);
	t->due_s = xp_timer() + ms / 1000.0L;
	wrenSetSlotNull(vm, 0);
}

void
wren_bind_sweep_pending_timers(void)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->pending_timer_count == 0)
		return;
	long double now = xp_timer();
	int w = 0;
	for (int r = 0; r < st->pending_timer_count; r++) {
		struct wren_pending_timer t = st->pending_timers[r];
		if (now < t.due_s) {
			if (w != r)
				st->pending_timers[w] = t;
			w++;
			continue;
		}
		/* wren_result_push takes ownership of `fiber` and frees
		 * `data` (NULL here) via free_fn (also NULL).  Any past-due
		 * entry is removed from the pending list by virtue of not
		 * being copied to the write index. */
		wren_result_push(t.fiber, NULL, deliver_timer_elapsed, NULL);
	}
	st->pending_timer_count = w;
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
	{ "Input",  true,  "next()",              fn_Input_next               },
	{ "Input",  true,  "next(_)",             fn_Input_next_ms            },
	{ "Input",  true,  "poll()",              fn_Input_poll               },
	{ "Input",  true,  "nextEvent(_)",        fn_Input_nextEvent          },
	{ "Input",  true,  "ungetKey_(_)",        fn_Input_ungetKey_          },
	{ "Input",  true,  "ungetMouse_(_)",      fn_Input_ungetMouse_        },
	{ "Input",  true,  "mousedrag()",         fn_Input_mousedrag          },
	{ "Input",  true,  "mouseVisible=(_)",    fn_Input_mouseVisible_set   },

	/* KeyEvent (instance) */
	{ "KeyEvent",   false, "code",       fn_KeyEvent_code      },
	{ "KeyEvent",   false, "codepoint",  fn_KeyEvent_codepoint },
	{ "KeyEvent",   false, "text",       fn_KeyEvent_text      },
	{ "KeyEvent",   false, "toString",   fn_KeyEvent_toString  },

	/* MouseEvent (instance) */
	{ "MouseEvent", false, "event",      fn_MouseEvent_event      },
	{ "MouseEvent", false, "modifiers",  fn_MouseEvent_modifiers  },
	{ "MouseEvent", false, "startX",     fn_MouseEvent_startX     },
	{ "MouseEvent", false, "startY",     fn_MouseEvent_startY     },
	{ "MouseEvent", false, "endX",       fn_MouseEvent_endX       },
	{ "MouseEvent", false, "endY",       fn_MouseEvent_endY       },
	{ "MouseEvent", false, "toString",   fn_MouseEvent_toString   },

	/* Clipboard (all static) */
	{ "Clipboard", true, "text",               fn_Clipboard_text     },
	{ "Clipboard", true, "text=(_)",           fn_Clipboard_text_set },

	/* REPL */
	{ "REPL",  true,  "compile_(_,_,_,_)",   fn_REPL_compile_         },
	{ "REPL",  true,  "printTrace_(_)",      fn_REPL_printTrace_      },
	{ "REPL",  true,  "hasModule(_)",        fn_REPL_hasModule        },
	{ "REPL",  true,  "modules",             fn_REPL_modules          },
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
	{ "Cell",  false, "toString",        fn_Cell_toString        },

	/* Cells (all instance) */
	{ "Cells", false, "count",            fn_Cells_count         },
	{ "Cells", false, "[_]",              fn_Cells_subscript     },
	{ "Cells", false, "iterate(_)",       fn_Cells_iterate       },
	{ "Cells", false, "iteratorValue(_)", fn_Cells_iteratorValue },
	{ "Cells", false, "toString",         fn_Cells_toString      },

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

	{ "Host", true, "cacheDirectory",       fn_Host_cacheDirectory    },

	/* Platform — OS identification. */
	{ "Platform", true, "name",             fn_Platform_name          },

	/* Timer — one-shot fiber resumption after a delay. */
	{ "Timer", true, "trigger(_,_)",        fn_Timer_trigger          },

	/* SFTP (all static) */
	{ "SFTP",       true,  "available",       fn_SFTP_available       },
	{ "SFTP",       true,  "pubdir",          fn_SFTP_pubdir          },
	{ "SFTP",       true,  "realpath(_,_)",   fn_SFTP_realpath        },
	{ "SFTP",       true,  "stat(_,_)",       fn_SFTP_stat            },
	{ "SFTP",       true,  "opendir(_,_)",    fn_SFTP_opendir         },
	{ "SFTP",       true,  "readdir(_,_)",    fn_SFTP_readdir         },
	{ "SFTP",       true,  "close(_,_)",      fn_SFTP_close           },
	{ "SFTP",       true,  "open(_,_,_)",     fn_SFTP_open            },
	{ "SFTP",       true,  "read(_,_,_,_)",   fn_SFTP_read            },
	{ "SFTP",       true,  "write(_,_,_,_)",  fn_SFTP_write           },
	{ "SFTP",       true,  "mkdir(_,_)",      fn_SFTP_mkdir           },
	{ "SFTP",       true,  "rmdir(_,_)",      fn_SFTP_rmdir           },
	{ "SFTP",       true,  "remove(_,_)",     fn_SFTP_remove          },
	{ "SFTP",       true,  "rename(_,_,_)",   fn_SFTP_rename          },

	/* SFTPEntry (instance) */
	{ "SFTPEntry",  false, "name",            fn_SFTPEntry_name       },
	{ "SFTPEntry",  false, "longname",        fn_SFTPEntry_longname   },
	{ "SFTPEntry",  false, "size",            fn_SFTPEntry_size       },
	{ "SFTPEntry",  false, "mtime",           fn_SFTPEntry_mtime      },
	{ "SFTPEntry",  false, "isDir",           fn_SFTPEntry_isDir      },
	{ "SFTPEntry",  false, "hash",            fn_SFTPEntry_hash       },
	{ "SFTPEntry",  false, "toString",        fn_SFTPEntry_toString   },

	/* SFTPStat (instance) */
	{ "SFTPStat",   false, "size",            fn_SFTPStat_size        },
	{ "SFTPStat",   false, "mtime",           fn_SFTPStat_mtime       },
	{ "SFTPStat",   false, "atime",           fn_SFTPStat_atime       },
	{ "SFTPStat",   false, "mode",            fn_SFTPStat_mode        },
	{ "SFTPStat",   false, "uid",             fn_SFTPStat_uid         },
	{ "SFTPStat",   false, "gid",             fn_SFTPStat_gid         },
	{ "SFTPStat",   false, "toString",        fn_SFTPStat_toString    },

	/* SFTPError (instance) */
	{ "SFTPError",  false, "code",            fn_SFTPError_code         },
	{ "SFTPError",  false, "message",         fn_SFTPError_message      },
	{ "SFTPError",  false, "isTransient",     fn_SFTPError_isTransient  },
	{ "SFTPError",  false, "serverStatus",    fn_SFTPError_serverStatus },
	{ "SFTPError",  false, "toString",        fn_SFTPError_toString     },

	/* HookHandle (instance methods/getters) */
	{ "HookHandle", false, "remove()",       fn_HookHandle_remove       },
	{ "HookHandle", false, "callCount",      fn_HookHandle_callCount    },
	{ "HookHandle", false, "totalRuntime",   fn_HookHandle_totalRuntime },
	{ "HookHandle", false, "minRuntime",     fn_HookHandle_minRuntime   },
	{ "HookHandle", false, "maxRuntime",     fn_HookHandle_maxRuntime   },
	{ "HookHandle", false, "toString",       fn_HookHandle_toString     },

	/* Console */
	{ "Console", true, "count",            fn_Console_count         },
	{ "Console", true, "total",            fn_Console_total         },
	{ "Console", true, "[_]",              fn_Console_subscript     },
	{ "Console", true, "clear()",          fn_Console_clear         },
	{ "Console", true, "markSeen()",       fn_Console_markSeen      },
	{ "Console", true, "iterate(_)",       fn_Console_iterate       },
	{ "Console", true, "iteratorValue(_)", fn_Console_iteratorValue },

	/* Conn */
	{ "Conn",  true, "send(_)",        fn_Conn_send         },
	{ "Conn",  true, "sendRaw(_)",     fn_Conn_sendRaw      },
	{ "Conn",  true, "close()",        fn_Conn_close        },
	{ "Conn",  true, "connected",      fn_Conn_connected    },
	{ "Conn",  true, "type",           fn_Conn_type         },
	{ "Conn",  true, "pending",        fn_Conn_pending      },
	{ "Conn",  true, "queued",         fn_Conn_queued       },
	{ "Conn",  true, "peek(_)",        fn_Conn_peek         },
	{ "Conn",  true, "recv(_)",        fn_Conn_recv         },

	/* CTerm */
	{ "CTerm", true, "emulation",          fn_CTerm_emulation       },
	{ "CTerm", true, "x",                  fn_CTerm_x               },
	{ "CTerm", true, "y",                  fn_CTerm_y               },
	{ "CTerm", true, "originX",            fn_CTerm_originX         },
	{ "CTerm", true, "originY",            fn_CTerm_originY         },
	{ "CTerm", true, "attr",               fn_CTerm_attr            },
	{ "CTerm", true, "doorwayMode",        fn_CTerm_doorwayMode     },
	{ "CTerm", true, "music",              fn_CTerm_music           },
	{ "CTerm", true, "width",              fn_CTerm_width           },
	{ "CTerm", true, "height",             fn_CTerm_height          },
	{ "CTerm", true, "topMargin",          fn_CTerm_topMargin       },
	{ "CTerm", true, "bottomMargin",       fn_CTerm_bottomMargin    },
	{ "CTerm", true, "leftMargin",         fn_CTerm_leftMargin      },
	{ "CTerm", true, "rightMargin",        fn_CTerm_rightMargin     },
	{ "CTerm", true, "fgColor",            fn_CTerm_fgColor         },
	{ "CTerm", true, "bgColor",            fn_CTerm_bgColor         },
	{ "CTerm", true, "hasPaletteOverride", fn_CTerm_hasPaletteOverride },
	{ "CTerm", true, "paletteOverride",    fn_CTerm_paletteOverride },
	{ "CTerm", true, "fontSlot",           fn_CTerm_fontSlot        },
	{ "CTerm", true, "altFonts",           fn_CTerm_altFonts        },
	{ "CTerm", true, "scrollbackLines",    fn_CTerm_scrollbackLines },
	{ "CTerm", true, "scrollbackWidth",    fn_CTerm_scrollbackWidth },
	{ "CTerm", true, "scrollbackPos",      fn_CTerm_scrollbackPos   },
	{ "CTerm", true, "scrollbackStart",    fn_CTerm_scrollbackStart },
	{ "CTerm", true, "started",            fn_CTerm_started         },
	{ "CTerm", true, "skypix",             fn_CTerm_skypix          },
	{ "CTerm", true, "logMode",            fn_CTerm_logMode         },
	{ "CTerm", true, "logPaused",          fn_CTerm_logPaused       },
	{ "CTerm", true, "statusDisplay",      fn_CTerm_statusDisplay   },
	{ "CTerm", true, "extAttr",            fn_CTerm_extAttr         },
	{ "CTerm", true, "lastColumnFlag",     fn_CTerm_lastColumnFlag  },
	{ "CTerm", true, "write(_)",           fn_CTerm_write           },
	{ "CTerm", true, "suspended",          fn_CTerm_suspended_get   },
	{ "CTerm", true, "suspended=(_)",      fn_CTerm_suspended_set   },
	{ "ExtAttr", false, "autoWrap",            fn_ExtAttr_autoWrap            },
	{ "ExtAttr", false, "originMode",          fn_ExtAttr_originMode          },
	{ "ExtAttr", false, "sxScroll",            fn_ExtAttr_sxScroll            },
	{ "ExtAttr", false, "decLrmm",             fn_ExtAttr_decLrmm             },
	{ "ExtAttr", false, "bracketPaste",        fn_ExtAttr_bracketPaste        },
	{ "ExtAttr", false, "decBkm",              fn_ExtAttr_decBkm              },
	{ "ExtAttr", false, "prestelMosaic",       fn_ExtAttr_prestelMosaic       },
	{ "ExtAttr", false, "prestelDoubleHeight", fn_ExtAttr_prestelDoubleHeight },
	{ "ExtAttr", false, "prestelConceal",      fn_ExtAttr_prestelConceal      },
	{ "ExtAttr", false, "prestelSeparated",    fn_ExtAttr_prestelSeparated    },
	{ "ExtAttr", false, "prestelHold",         fn_ExtAttr_prestelHold         },
	{ "ExtAttr", false, "alternateKeypad",     fn_ExtAttr_alternateKeypad     },
	{ "LastColumnFlag", false, "set",     fn_LCF_set     },
	{ "LastColumnFlag", false, "enabled", fn_LCF_enabled },
	{ "LastColumnFlag", false, "forced",  fn_LCF_forced  },

	/* BBS */
	{ "BBS",   true, "name",                    fn_BBS_name                  },
	{ "BBS",   true, "addr",                    fn_BBS_addr                  },
	{ "BBS",   true, "port",                    fn_BBS_port                  },
	{ "BBS",   true, "connType",                fn_BBS_connType              },
	{ "BBS",   true, "user",                    fn_BBS_user                  },
	{ "BBS",   true, "password",                fn_BBS_password              },
	{ "BBS",   true, "syspass",                 fn_BBS_syspass               },
	{ "BBS",   true, "music",                   fn_BBS_music                 },
	{ "BBS",   true, "rip",                     fn_BBS_rip                   },
	{ "BBS",   true, "comment",                 fn_BBS_comment               },
	{ "BBS",   true, "type",                    fn_BBS_type                  },
	{ "BBS",   true, "id",                      fn_BBS_id                    },
	{ "BBS",   true, "addressFamily",           fn_BBS_addressFamily         },
	{ "BBS",   true, "termName",                fn_BBS_termName              },
	{ "BBS",   true, "screenMode",              fn_BBS_screenMode            },
	{ "BBS",   true, "bpsRate",                 fn_BBS_bpsRate               },
	{ "BBS",   true, "font",                    fn_BBS_font                  },
	{ "BBS",   true, "noStatus",                fn_BBS_noStatus              },
	{ "BBS",   true, "hidePopups",              fn_BBS_hidePopups            },
	{ "BBS",   true, "yellowIsYellow",          fn_BBS_yellowIsYellow        },
	{ "BBS",   true, "forceLcf",                fn_BBS_forceLcf              },
	{ "BBS",   true, "added",                   fn_BBS_added                 },
	{ "BBS",   true, "connected",               fn_BBS_connected             },
	{ "BBS",   true, "fastConnected",           fn_BBS_fastConnected         },
	{ "BBS",   true, "calls",                   fn_BBS_calls                 },
	{ "BBS",   true, "dlDir",                   fn_BBS_dlDir                 },
	{ "BBS",   true, "ulDir",                   fn_BBS_ulDir                 },
	{ "BBS",   true, "logFile",                 fn_BBS_logFile               },
	{ "BBS",   true, "appendLogFile",           fn_BBS_appendLogFile         },
	{ "BBS",   true, "xferLogLevel",            fn_BBS_xferLogLevel          },
	{ "BBS",   true, "telnetLogLevel",          fn_BBS_telnetLogLevel        },
	{ "BBS",   true, "stopBits",                fn_BBS_stopBits              },
	{ "BBS",   true, "dataBits",                fn_BBS_dataBits              },
	{ "BBS",   true, "parity",                  fn_BBS_parity                },
	{ "BBS",   true, "flowControl",             fn_BBS_flowControl           },
	{ "FlowControl", false, "rtsCts",            fn_FlowControl_rtsCts       },
	{ "FlowControl", false, "xonOff",            fn_FlowControl_xonOff       },
	{ "BBS",   true, "telnetNoBinary",          fn_BBS_telnetNoBinary        },
	{ "BBS",   true, "deferTelnetNegotiation",  fn_BBS_deferTelnetNegotiation},
	{ "BBS",   true, "ghostProgram",            fn_BBS_ghostProgram          },
	{ "BBS",   true, "sftpPublicKey",           fn_BBS_sftpPublicKey         },
	{ "BBS",   true, "sshFingerprint",          fn_BBS_sshFingerprint        },
	{ "BBS",   true, "sshFingerprintLen",       fn_BBS_sshFingerprintLen     },
	{ "BBS",   true, "palette",                 fn_BBS_palette               },
	{ "BBS",   true, "paletteSize",             fn_BBS_paletteSize           },
	{ "BBS",   true, "sortOrder",               fn_BBS_sortOrder             },

	/* Cache */
	{ "Directory", false, "contains(_)",     fn_Directory_contains  },
	{ "Directory", false, "list",            fn_Directory_list      },
	{ "Directory", false, "create(_)",       fn_Directory_create    },
	{ "Directory", false, "createDir(_)",    fn_Directory_createDir },
	{ "Directory", false, "delete(_)",       fn_Directory_delete    },
	{ "Directory", false, "toString",        fn_Directory_toString  },
	{ "File",      false, "open()",          fn_File_open           },
	{ "File",      false, "close()",         fn_File_close          },
	{ "File",      false, "readBytes(_)",    fn_File_readBytes_1    },
	{ "File",      false, "readBytes(_,_)",  fn_File_readBytes_2    },
	{ "File",      false, "read()",          fn_File_read           },
	{ "File",      false, "writeBytes(_)",   fn_File_writeBytes_1   },
	{ "File",      false, "writeBytes(_,_)", fn_File_writeBytes_2   },
	{ "File",      false, "write(_)",        fn_File_write          },
	{ "File",      false, "readLine()",      fn_File_readLine       },
	{ "File",      false, "writeLine(_)",    fn_File_writeLine      },
	{ "File",      false, "offset",          fn_File_offset_get     },
	{ "File",      false, "offset=(_)",      fn_File_offset_set     },
	{ "File",      false, "size",            fn_File_size           },
	{ "File",      false, "isOpen",          fn_File_isOpen         },
	{ "File",      false, "toString",        fn_File_toString       },

	/* Hook */
	{ "Hook",  true, "onKey(_)",       fn_Hook_onKey            },
	{ "Hook",  true, "onKey(_,_)",     fn_Hook_onKey_filtered   },
	{ "Hook",  true, "onInput(_)",     fn_Hook_onInput          },
	{ "Hook",  true, "onInput(_,_)",   fn_Hook_onInput_filtered },
	{ "Hook",  true, "onMatch(_,_)",   fn_Hook_onMatch          },
	{ "Hook",  true, "onMouse(_)",     fn_Hook_onMouse          },
	{ "Hook",  true, "onMouse(_,_)",   fn_Hook_onMouse_filtered },
	{ "Hook",  true, "onStatus(_)",    fn_Hook_onStatus         },
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
	if (strcmp(className, "HookHandle") == 0) {
		WrenForeignClassMethods m = {
			wren_hook_handle_allocate, wren_hook_handle_finalize
		};
		return m;
	}
	if (strcmp(className, "Directory") == 0) {
		WrenForeignClassMethods m = {
			wren_directory_allocate, wren_directory_finalize
		};
		return m;
	}
	if (strcmp(className, "File") == 0) {
		WrenForeignClassMethods m = {
			wren_file_allocate, wren_file_finalize
		};
		return m;
	}
	if (strcmp(className, "FlowControl") == 0) {
		WrenForeignClassMethods m = {
			wren_flowcontrol_allocate, wren_flowcontrol_finalize
		};
		return m;
	}
	if (strcmp(className, "ExtAttr") == 0) {
		WrenForeignClassMethods m = {
			wren_extattr_allocate, wren_extattr_finalize
		};
		return m;
	}
	if (strcmp(className, "LastColumnFlag") == 0) {
		WrenForeignClassMethods m = {
			wren_last_column_flag_allocate,
			wren_last_column_flag_finalize
		};
		return m;
	}
	if (strcmp(className, "SFTPEntry") == 0) {
		WrenForeignClassMethods m = {
			wren_sftp_entry_allocate, wren_sftp_entry_finalize
		};
		return m;
	}
	if (strcmp(className, "SFTPStat") == 0) {
		WrenForeignClassMethods m = {
			wren_sftp_stat_allocate, wren_sftp_stat_finalize
		};
		return m;
	}
	if (strcmp(className, "SFTPHandle") == 0) {
		WrenForeignClassMethods m = {
			wren_sftp_handle_allocate, wren_sftp_handle_finalize
		};
		return m;
	}
	if (strcmp(className, "SFTPError") == 0) {
		WrenForeignClassMethods m = {
			wren_sftp_error_allocate, wren_sftp_error_finalize
		};
		return m;
	}
	if (strcmp(className, "TimerElapsed") == 0) {
		WrenForeignClassMethods m = {
			wren_timer_elapsed_allocate, wren_timer_elapsed_finalize
		};
		return m;
	}
	return none;
}
