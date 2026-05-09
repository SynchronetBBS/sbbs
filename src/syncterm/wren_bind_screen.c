/* Screen / Input / Cell / Cells / Font / Hyperlinks / Color / Palette
 * / CustomCursor / VideoFlags / KeyEvent / MouseEvent foreign surface
 * for the Wren scripting host.  Public API in wren_bind_screen.h;
 * BINDINGS table + lookup_class dispatch live in wren_bind.c. */

#include "wren_bind_fs.h"
#include "wren_bind_internal.h"
#include "wren_bind_screen.h"
#include "wren_host_internal.h"
#include "wren_host.h"
#include "wren/vm/wren_vm.h"
#include "wren/vm/wren_value.h"

#include "ciolib.h"
#include "cterm.h"
#include "syncterm.h"
#include "term.h"
#include "utf8_codepages.h"

extern struct cterminal *cterm;

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----- Screen / Input -------------------------------------------- */

/* Screen.supports.* - backend capability flags.  Each getter is one
 * bit-test against cio_api.options.  Macro to keep the boilerplate
 * out of the way. */
#define SCREEN_SUPPORTS(name, flag)                                  \
	void fnScreenSupports_##name(WrenVM *vm)                     \
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

/* Window bounds [sx, sy, ex, ey] - 1-based, inclusive.  Setter
 * delegates to ciolib_window which also resets the cursor to (1,1)
 * within the new window. */
void
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

void
fnScreenWindow_bounds_set(WrenVM *vm)
{
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_LIST) {
		wren_throw(vm, "Screen.window.bounds=: must be a List");
		return;
	}
	if (wrenGetListCount(vm, 1) < 4) {
		wren_throw(vm, "Screen.window.bounds=: List must have 4 elements");
		return;
	}
	wrenEnsureSlots(vm, 3);
	int box[4];
	for (int i = 0; i < 4; i++) {
		wrenGetListElement(vm, 1, i, 2);
		box[i] = (int)wrenGetSlotDouble(vm, 2);
	}
	window(box[0], box[1], box[2], box[3]);
}

void
fnScreenWindow_putChar(WrenVM *vm)
{
	int c = (int)wrenGetSlotDouble(vm, 1);
	putch(c);
}

/* Definition lives lower in the file alongside the other UTF-8
 * helpers; declared in wren_bind_internal.h so other binding modules
 * (wren_bind.c) can reuse it. */

/* ScreenWindow.print(s) writes `s` one cell at a time via putch.
 * Wren strings are UTF-8 byte sequences but local conio renders one
 * cell per byte, so we decode codepoint-by-codepoint and map each
 * codepoint to a CP437 byte (matching Cell.ch=) before putch.  This
 * keeps the cursor advance == rendered cell count even when the input
 * contains multi-byte UTF-8 sequences (em-dashes, box-drawing, etc.).
 * Invalid UTF-8 falls through as a single raw byte so binary output
 * still works. */
void
fnScreenWindow_print(WrenVM *vm)
{
	int len = 0;
	const char *s = wrenGetSlotBytes(vm, 1, &len);
	int i = 0;
	while (i < len) {
		uint32_t cp = 0;
		int n = decode_utf8_first(s + i, len - i, &cp);
		if (n <= 0) {
			putch((unsigned char)s[i]);
			i++;
			continue;
		}
		uint8_t b = cpchar_from_unicode_cpoint(CIOLIB_CP437, cp, '?');
		putch(b);
		i += n;
	}
}

/* Cursor position as a single [x, y] list - getter and setter
 * symmetric, no separate moveTo / cursorX / cursorY calls. */
void
fnScreenWindow_position(WrenVM *vm)
{
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
	wrenSetSlotDouble(vm, 1, (double)wherex());
	wrenInsertInList(vm, 0, -1, 1);
	wrenSetSlotDouble(vm, 1, (double)wherey());
	wrenInsertInList(vm, 0, -1, 1);
}

void
fnScreenWindow_position_set(WrenVM *vm)
{
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_LIST) {
		wren_throw(vm, "Screen.window.position=: must be a List");
		return;
	}
	if (wrenGetListCount(vm, 1) < 2) {
		wren_throw(vm, "Screen.window.position=: List must have 2 elements");
		return;
	}
	wrenEnsureSlots(vm, 3);
	wrenGetListElement(vm, 1, 0, 2);
	int x = (int)wrenGetSlotDouble(vm, 2);
	wrenGetListElement(vm, 1, 1, 2);
	int y = (int)wrenGetSlotDouble(vm, 2);
	gotoxy(x, y);
}

void
fn_Screen_attr_set(WrenVM *vm)
{
	int a = (int)wrenGetSlotDouble(vm, 1);
	textattr(a);
}

void
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

/* Forward decl - defined later beside the Cell helpers. */
static int encode_utf8(uint32_t cp, char out[4]);

/* Foreign class state for KeyEvent and MouseEvent - both are simple
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

void
wren_key_event_allocate(WrenVM *vm)
{
	uint16_t code = (uint16_t)wrenGetSlotDouble(vm, 1);
	/* Allocate first to satisfy Wren's "<allocate> must call
	 * wrenSetSlotNewForeign exactly once" rule.  The struct is
	 * abandoned to GC if we abort below - the finalizer is a
	 * no-op so that's free. */
	struct wren_key_event *ke =
	    wrenSetSlotNewForeign(vm, 0, 0, sizeof(*ke));
	/* ciolib's key transport packs a 16-bit code as low-byte-then-
	 * high-byte through getch/ungetch, with the convention that low
	 * byte == 0x00 or 0xE0 marks "extended scancode follows."  Codes
	 * with high byte non-zero AND low byte not 0/0xE0 are not
	 * representable in that stream - Input.unget would split them
	 * into two unrelated reads.  Reject at construction so the
	 * footgun fires here, not silently in dispatch later. */
	uint8_t lo = (uint8_t)(code & 0xFFu);
	uint8_t hi = (uint8_t)(code >> 8);
	if (hi != 0 && lo != 0x00 && lo != 0xE0) {
		char msg[140];
		snprintf(msg, sizeof(msg),
		    "KeyEvent.new: 0x%04X is not a representable key code "
		    "(extended keys require low byte 0x00 or 0xE0; "
		    "single-byte keys require high byte 0x00)",
		    (unsigned)code);
		wren_throw(vm, msg);
		return;
	}
	key_event_init(ke, code);
}

void
wren_key_event_finalize(void *data)
{
	(void)data;
}

/* Derive a plausible bstate from a ciolib mouse event code: bit for
 * the button the event is about, or 0 for MOUSE_MOVE.  Used by the
 * shorter MouseEvent.new(...) overloads that don't take an explicit
 * bstate.  Real bstate (which buttons were held at the instant the
 * event fired) is only meaningful for events delivered by ciolib;
 * synthesised events get the single-button approximation. */
static int
derive_bstate_from_event(int event)
{
	if (event <= CIOLIB_MOUSE_MOVE || event > CIOLIB_BUTTON_5_DRAG_END)
		return 0;
	int button = (event - 1) / 9 + 1;       /* 1..5 */
	return CIOLIB_BUTTON(button);
}

/* MouseEvent.new - five overloads, dispatched by argument count.
 * Wren's foreign-class allocator is keyed by class only (a single
 * "<allocate>" per class), so all `construct new` arities funnel
 * through here; we read the call arity from wrenGetSlotCount.
 * Defaults when an arg is omitted:
 *   endX, endY → copies of startX, startY (point event)
 *   modifiers  → 0
 *   bstate     → derived from event (bit for the button it's about)
 */
void
wren_mouse_event_allocate(WrenVM *vm)
{
	int  arity = wrenGetSlotCount(vm) - 1;  /* slot 0 = receiver */
	int  event, startX, startY;
	int  endX = 0, endY = 0, modifiers = 0, bstate = 0;
	bool bstate_explicit = false;

	if (arity < 3 || arity > 7) {
		wren_throw(vm,
		    "MouseEvent.new: expected 3..7 arguments");
		return;
	}
	event  = (int)wrenGetSlotDouble(vm, 1);
	startX = (int)wrenGetSlotDouble(vm, 2);
	startY = (int)wrenGetSlotDouble(vm, 3);
	endX   = startX;
	endY   = startY;

	switch (arity) {
	case 3:
		/* (event, startX, startY) */
		break;
	case 4:
		/* (event, startX, startY, modifiers) */
		modifiers = (int)wrenGetSlotDouble(vm, 4);
		break;
	case 5:
		/* (event, startX, startY, endX, endY) */
		endX = (int)wrenGetSlotDouble(vm, 4);
		endY = (int)wrenGetSlotDouble(vm, 5);
		break;
	case 6:
		/* (event, startX, startY, endX, endY, modifiers) */
		endX      = (int)wrenGetSlotDouble(vm, 4);
		endY      = (int)wrenGetSlotDouble(vm, 5);
		modifiers = (int)wrenGetSlotDouble(vm, 6);
		break;
	case 7:
		/* (event, startX, startY, endX, endY, modifiers, bstate) */
		endX            = (int)wrenGetSlotDouble(vm, 4);
		endY            = (int)wrenGetSlotDouble(vm, 5);
		modifiers       = (int)wrenGetSlotDouble(vm, 6);
		bstate          = (int)wrenGetSlotDouble(vm, 7);
		bstate_explicit = true;
		break;
	}
	if (!bstate_explicit)
		bstate = derive_bstate_from_event(event);

	struct wren_mouse_event *me =
	    wrenSetSlotNewForeign(vm, 0, 0, sizeof(*me));
	memset(me, 0, sizeof(*me));
	me->type           = SWF_MOUSE_EVENT;
	me->ev.event       = event;
	me->ev.bstate      = bstate;
	me->ev.kbmodifiers = modifiers;
	me->ev.startx      = startX;
	me->ev.starty      = startY;
	me->ev.endx        = endX;
	me->ev.endy        = endY;
}

void
wren_mouse_event_finalize(void *data)
{
	(void)data;
}

/* KeyEvent field accessors. */
void
fn_KeyEvent_code(WrenVM *vm)
{
	struct wren_key_event *ke = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)ke->code);
}

void
fn_KeyEvent_codepoint(WrenVM *vm)
{
	struct wren_key_event *ke = wrenGetSlotForeign(vm, 0);
	if (ke->codepoint < 0) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotDouble(vm, 0, (double)ke->codepoint);
}

void
fn_KeyEvent_text(WrenVM *vm)
{
	struct wren_key_event *ke = wrenGetSlotForeign(vm, 0);
	wrenSetSlotBytes(vm, 0, (const char *)ke->text, ke->text_len);
}

/* MouseEvent field accessors. */
#define MOUSE_FIELD(NAME, FIELD)                                        \
	void fn_MouseEvent_##NAME(WrenVM *vm)                    \
	{                                                               \
		struct wren_mouse_event *me = wrenGetSlotForeign(vm, 0);\
		wrenSetSlotDouble(vm, 0, (double)me->ev.FIELD);         \
	}

MOUSE_FIELD(event,     event)
MOUSE_FIELD(bstate,    bstate)
MOUSE_FIELD(modifiers, kbmodifiers)
MOUSE_FIELD(startX,    startx)
MOUSE_FIELD(startY,    starty)
MOUSE_FIELD(endX,      endx)
MOUSE_FIELD(endY,      endy)

#undef MOUSE_FIELD

void
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

void
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

/* Build a Wren-side KeyEvent in `dst` from a raw 16-bit code.  Uses
 * the captured KeyEvent class handle; callers must have at least
 * `dst + 2` slots ensured (slot dst = result, slot dst+1 = scratch
 * for the class lookup). */
void
push_key_event(WrenVM *vm, uint16_t code, int dst)
{
	struct wren_host_state *st = wren_host_state();
	load_class_into_slot(vm, &st->key_event_class, "KeyEvent", dst + 1);
	struct wren_key_event *ke =
	    wrenSetSlotNewForeign(vm, dst, dst + 1, sizeof(*ke));
	key_event_init(ke, code);
}

/* Build a Wren-side MouseEvent in `dst` from a struct mouse_event.
 * Same slot conventions as push_key_event - caller must have ensured
 * at least `dst + 2` slots. */
void
push_mouse_event(WrenVM *vm, const struct mouse_event *mev, int dst)
{
	struct wren_host_state *st = wren_host_state();
	load_class_into_slot(vm, &st->mouse_event_class, "MouseEvent", dst + 1);
	struct wren_mouse_event *me =
	    wrenSetSlotNewForeign(vm, dst, dst + 1, sizeof(*me));
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
			/* No mouse pending despite the marker - surface as a
			 * raw KeyEvent so the script at least sees the byte. */
			push_key_event(vm, (uint16_t)code, 0);
			return;
		}
		push_mouse_event(vm, &mev, 0);
		return;
	}
	push_key_event(vm, (uint16_t)code, 0);
}

/* Wake.post(fiber, value) - queue `value` to be delivered to `fiber`
 * via the same result queue as the claim dispatcher / Timer.trigger.
 * Safe to call from a hook body (queues only; the resume happens on
 * the next main-loop drain).  Hooks must NOT call from their own
 * foreign frame; the queue dispatcher's wrenCall would re-enter the
 * VM.
 *
 * Both `fiber` and `value` are pinned via wrenGetSlotHandle.  The
 * fiber handle is released by the result-queue drain after the call.
 * The value handle is released by the payload's free fn. */
struct wake_payload {
	WrenHandle *value;
};

static void
deliver_wake_value(WrenVM *vm, int slot, void *data)
{
	struct wake_payload *wp = data;
	if (wp != NULL && wp->value != NULL) {
		wrenSetSlotHandle(vm, slot, wp->value);
	} else {
		wrenSetSlotNull(vm, slot);
	}
}

static void
free_wake_payload(void *data)
{
	struct wake_payload    *wp = data;
	struct wren_host_state *st;

	if (wp == NULL)
		return;
	st = wren_host_state();
	if (wp->value != NULL && st != NULL && st->vm != NULL)
		wrenReleaseHandle(st->vm, wp->value);
	free(wp);
}

void
fn_Wake_post(WrenVM *vm)
{
	WrenHandle             *fiber;
	WrenHandle             *value;
	struct wake_payload    *wp;

	fiber = wrenGetSlotHandle(vm, 1);
	value = wrenGetSlotHandle(vm, 2);
	wp    = calloc(1, sizeof(*wp));
	if (wp == NULL) {
		if (value != NULL)
			wrenReleaseHandle(vm, value);
		if (fiber != NULL)
			wrenReleaseHandle(vm, fiber);
		wrenSetSlotNull(vm, 0);
		return;
	}
	wp->value = value;
	wren_result_push(fiber, wp, deliver_wake_value, free_wake_payload);
	wrenSetSlotNull(vm, 0);
}

/* ----- Input claim stack -------------------------------------------
 *
 * Each running App (or one-shot input consumer) pushes a claim that
 * receives KeyEvent / MouseEvent foreigns and returns a Bool
 * (consumed).  The C dispatcher walks claims top-down, short-circuits
 * on first consumed=true, and falls through to the hook chain when
 * everything passed through.  Per-fiber slot: a same-fiber re-push
 * replaces the existing entry in place; different-fiber pushes add
 * new entries on top.
 *
 * ClaimHandle is a foreign whose payload pairs an entry pointer with
 * a monotonic id.  pop() walks the list; if the entry is still
 * present and its id matches, unlink/release.  Idempotent - stale
 * handles (after a same-fiber re-push bumped the id) are no-ops.
 * The foreign finalizer also calls pop() so a forgotten / GC'd
 * handle still cleans up. */

struct wren_claim_handle {
	enum syncterm_wren_foreign  type;   /* SWF_CLAIM_HANDLE  */
	struct wren_input_claim    *entry;
	uint64_t                    id;
};

static void
free_claim_entry(struct wren_host_state *st, struct wren_input_claim *c)
{
	if (c->fn != NULL)
		wrenReleaseHandle(st->vm, c->fn);
	if (c->fiber != NULL)
		wrenReleaseHandle(st->vm, c->fiber);
	free(c);
}

void
wren_claim_handle_allocate(WrenVM *vm)
{
	struct wren_claim_handle *ch = wrenSetSlotNewForeign(vm, 0, 0,
	    sizeof(*ch));
	memset(ch, 0, sizeof(*ch));
	ch->type = SWF_CLAIM_HANDLE;
}

/* ClaimHandle finalizer: if the entry is still present (script
 * forgot to pop, or the handle was GC'd before App.run could clean
 * up), unlink it from the stack.  Compare-by-id so a same-fiber
 * re-push that bumped the id doesn't get yanked out from under
 * its current owner. */
void
wren_claim_handle_finalize(void *data)
{
	struct wren_claim_handle *ch = data;
	struct wren_host_state   *st;
	struct wren_input_claim **cur;

	if (ch == NULL || ch->entry == NULL)
		return;
	st = wren_host_state();
	if (st == NULL)
		return;
	for (cur = &st->claim_top; *cur != NULL; cur = &(*cur)->next) {
		if (*cur != ch->entry)
			continue;
		if ((*cur)->id != ch->id)
			break;
		struct wren_input_claim *gone = *cur;
		*cur = gone->next;
		free_claim_entry(st, gone);
		break;
	}
	ch->entry = NULL;
}

/* Input.pushClaim_(fiber, fn) - register a claim; returns ClaimHandle.
 * Same-fiber dedup: if the calling fiber already has a claim on the
 * stack, replace its fn in place (preserving stack position) and
 * bump the id.  Otherwise allocate a new entry at the top. */
void
fn_Input_pushClaim_(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	WrenHandle *fiber = wrenGetSlotHandle(vm, 1);
	WrenHandle *fn    = wrenGetSlotHandle(vm, 2);

	if (fiber == NULL || fn == NULL || st == NULL) {
		if (fiber != NULL) wrenReleaseHandle(vm, fiber);
		if (fn    != NULL) wrenReleaseHandle(vm, fn);
		wrenSetSlotNull(vm, 0);
		return;
	}

	struct wren_input_claim *entry = NULL;
	for (struct wren_input_claim *c = st->claim_top; c != NULL;
	    c = c->next) {
		if (c->fiber != NULL &&
		    wrenValuesSame(c->fiber->value, fiber->value)) {
			entry = c;
			break;
		}
	}

	if (entry != NULL) {
		/* Replace fn in place; bump id so any prior ClaimHandle
		 * for this entry no longer matches and pop()s no-op. */
		wrenReleaseHandle(vm, entry->fn);
		entry->fn = fn;
		entry->id = ++st->claim_next_id;
		/* fiber didn't change, but we hold an extra handle to it
		 * from this push - release the duplicate. */
		wrenReleaseHandle(vm, fiber);
	} else {
		entry = calloc(1, sizeof(*entry));
		if (entry == NULL) {
			wrenReleaseHandle(vm, fiber);
			wrenReleaseHandle(vm, fn);
			wrenSetSlotNull(vm, 0);
			return;
		}
		entry->fiber = fiber;
		entry->fn    = fn;
		entry->id    = ++st->claim_next_id;
		entry->next  = st->claim_top;
		st->claim_top = entry;
	}

	/* Build a ClaimHandle pointing at the entry. */
	wrenEnsureSlots(vm, 2);
	load_class_into_slot(vm, &st->claim_handle_class, "ClaimHandle", 1);
	struct wren_claim_handle *ch =
	    wrenSetSlotNewForeign(vm, 0, 1, sizeof(*ch));
	ch->type  = SWF_CLAIM_HANDLE;
	ch->entry = entry;
	ch->id    = entry->id;
}

/* ClaimHandle.pop() - unlink the claim entry by id match.  Idempotent
 * by design: stale handles (after a same-fiber re-push, or after
 * another path popped the entry, or after the finalizer ran) are no-
 * ops.  Returns true if this call actually removed the entry. */
void
fn_ClaimHandle_pop(WrenVM *vm)
{
	struct wren_claim_handle *ch = wrenGetSlotForeign(vm, 0);
	struct wren_host_state   *st = wren_host_state();
	bool removed = false;

	if (ch != NULL && ch->entry != NULL && st != NULL) {
		struct wren_input_claim **cur;
		for (cur = &st->claim_top; *cur != NULL;
		    cur = &(*cur)->next) {
			if (*cur != ch->entry)
				continue;
			if ((*cur)->id != ch->id)
				break;
			struct wren_input_claim *gone = *cur;
			*cur = gone->next;
			free_claim_entry(st, gone);
			removed = true;
			break;
		}
		ch->entry = NULL;
	}
	wrenSetSlotBool(vm, 0, removed);
}

/* Walk the claim stack with `event_handle` (a pinned WrenHandle to a
 * KeyEvent or MouseEvent foreign) as the dispatch arg.  Auto-prunes
 * dead-fiber claims as it goes.  Returns true on first consumed (Bool
 * true) return; false if all claims passed through or were pruned.
 *
 * The event is passed as a WrenHandle (not a slot index) so we don't
 * have to negotiate a slot layout with the caller - wrenCall and
 * fiber_is_done both truncate the api stack to whatever the called
 * stub's maxSlots is, which would invalidate any "scratch slot" the
 * caller had stashed the event in.  Pin once in the caller, push the
 * handle back into slot 2 each iteration, release in the caller. */
static bool
dispatch_claim_event(WrenHandle *event_handle)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->vm == NULL || st->dispatch1_handle == NULL ||
	    st->hook_class == NULL || event_handle == NULL)
		return false;

	bool consumed = false;
	struct wren_input_claim **cur = &st->claim_top;
	while (*cur != NULL) {
		struct wren_input_claim *c = *cur;
		struct wren_input_claim *next = c->next;

		if (c->fiber == NULL || fiber_is_done(c->fiber)) {
			*cur = next;
			free_claim_entry(st, c);
			continue;
		}

		wrenEnsureSlots(st->vm, 3);
		wrenSetSlotHandle(st->vm, 0, st->hook_class);
		wrenSetSlotHandle(st->vm, 1, c->fn);
		wrenSetSlotHandle(st->vm, 2, event_handle);
		WrenInterpretResult r = wrenCall(st->vm,
		    st->dispatch1_handle);
		if (r == WREN_RESULT_SUCCESS &&
		    wrenGetSlotType(st->vm, 0) == WREN_TYPE_BOOL &&
		    wrenGetSlotBool(st->vm, 0)) {
			consumed = true;
			break;
		}
		/* Re-fetch cur each iteration: the handler may have
		 * popped its own entry, pushed a new one (lands at top
		 * - handled by walking from the new head next call,
		 * not this dispatch), or had the entry mutated by a
		 * same-fiber re-push (id bumped, fn replaced - we'd
		 * fire the new fn next event).  Just walk forward
		 * from where we are. */
		if (*cur == c)
			cur = &c->next;
	}
	return consumed;
}

bool
wren_bind_dispatch_claim_key(int code)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->claim_top == NULL || code == CIO_KEY_MOUSE)
		return false;
	/* Build the KeyEvent in slot 0, pin it, hand the handle off to
	 * the dispatcher.  The slot itself is throwaway - every wrenCall
	 * inside the dispatch loop truncates the api stack.  The pin is
	 * what keeps the event alive across those truncations. */
	wrenEnsureSlots(st->vm, 2);
	push_key_event(st->vm, (uint16_t)code, 0);
	WrenHandle *eh = wrenGetSlotHandle(st->vm, 0);
	if (eh == NULL)
		return false;
	bool consumed = dispatch_claim_event(eh);
	wrenReleaseHandle(st->vm, eh);
	return consumed;
}

bool
wren_bind_dispatch_claim_mouse(struct mouse_event *ev)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->claim_top == NULL || ev == NULL)
		return false;
	wrenEnsureSlots(st->vm, 2);
	push_mouse_event(st->vm, ev, 0);
	WrenHandle *eh = wrenGetSlotHandle(st->vm, 0);
	if (eh == NULL)
		return false;
	bool consumed = dispatch_claim_event(eh);
	wrenReleaseHandle(st->vm, eh);
	return consumed;
}

/* Free every claim and clear the stack - called from
 * wren_host_shutdown before wrenFreeVM. */
void
wren_bind_claim_stack_clear(void)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL)
		return;
	while (st->claim_top != NULL) {
		struct wren_input_claim *c = st->claim_top;
		st->claim_top = c->next;
		free_claim_entry(st, c);
	}
	st->claim_next_id = 0;
}

/* Input.next() - block until something is ready, return the event. */
void
fn_Input_next(WrenVM *vm)
{
	push_next_event(vm);
}

/* Input.next(ms) - wait up to ms milliseconds; null on timeout. */
void
fn_Input_next_ms(WrenVM *vm)
{
	int ms = (int)wrenGetSlotDouble(vm, 1);
	if (kbwait(ms) == 0) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	push_next_event(vm);
}

/* Input.poll() - non-blocking; null when nothing is ready. */
void
fn_Input_poll(WrenVM *vm)
{
	if (!kbhit()) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	push_next_event(vm);
}

/* Input.ungetKey_(ev) / Input.ungetMouse_(ev) - typed unget primitives.
 * The public Input.unget(ev) is a Wren-side wrapper that picks one of
 * these via `ev is KeyEvent` / `ev is MouseEvent`, since Wren doesn't
 * expose a "class of this foreign value" API to C and we need to know
 * which queue to push back to.  ungetch handles the LITERAL_E0
 * reversal internally; ungetmouse appends to the mouse output queue
 * and the next getch will surface CIO_KEY_MOUSE on its own. */
void
fn_Input_ungetKey_(WrenVM *vm)
{
	struct wren_key_event *ke = wrenGetSlotForeign(vm, 1);
	ungetch(ke->code);
}

void
fn_Input_ungetMouse_(WrenVM *vm)
{
	struct wren_mouse_event *me = wrenGetSlotForeign(vm, 1);
	ungetmouse(&me->ev);
}

void
fnScreenWindow_clear(WrenVM *vm)
{
	(void)vm;
	clrscr();
}

void
fnScreenWindow_clearToLineEnd(WrenVM *vm)
{
	(void)vm;
	clreol();
}

/* ciolib_savescreen returns a malloc'd struct ciolib_screen *.
 * Round-trip the pointer through a Wren Num - pointers are typically
 * aligned and fit in the 53-bit double mantissa on every supported
 * host.  restore() frees the screen on success. */
void
fn_Screen_save(WrenVM *vm)
{
	struct ciolib_screen *scrn = savescreen();
	wrenSetSlotDouble(vm, 0, (double)(uintptr_t)scrn);
}

void
fn_Screen_restore(WrenVM *vm)
{
	uintptr_t v = (uintptr_t)wrenGetSlotDouble(vm, 1);
	struct ciolib_screen *scrn = (struct ciolib_screen *)v;
	if (scrn == NULL)
		return;
	restorescreen(scrn);
	freescreen(scrn);
}

/* ----- Cell / Surface / vmem text bindings ------------------------
 *
 * `Cell` wraps a single struct vmem_cell.  Two flavours: standalone
 * (Cell.new()) owns its own bytes; view (created by Surface.[i] or
 * Surface.cellAt) reads and writes through to its parent Surface's
 * buffer, with a Wren-level pin on the parent so the buffer outlives
 * the view.
 *
 * `Surface` owns a width × height malloc'd buffer of vmem_cells.  It
 * is iterable and indexable (linear, like Cells used to be) but also
 * supports 2D operations: cellAt, putRect, fill.
 * -------------------------------------------------------------------- */

/* struct wren_surface definition lives in wren_bind_screen.h so that
 * wren_host.c can pre-fill the recycled status-bar surface in place. */

/* Forward - definition lives next to the rest of the rect helpers. */
static bool slot_to_surface_(WrenVM *vm, int slot,
    struct vmem_cell **buf_out, int *w_out, int *h_out);

/* Forward decl: Scrollback ring linearizer (rotates cterm->scrollback in
 * place when backstart != 0).  Defined further down with the rest of
 * the Scrollback foreigns. */
static void scrollback_linearize_(void);

struct wren_cell {
	enum syncterm_wren_foreign type;
	struct vmem_cell   c;             /* used when standalone */
	int                index;         /* index into parent_buf in view mode */
	struct vmem_cell  *parent_buf;    /* parent Surface's malloc'd buf (stable
	                                     while parent_handle is held - the
	                                     handle pins the Surface, the buf is
	                                     malloc'd, never reallocated, freed
	                                     only at the Surface's finalize) */
	WrenHandle        *parent_handle; /* pin on parent Surface; NULL = standalone */
};

/* Standalone cells return their own embedded `c`; view-mode cells use
 * their captured malloc'd buffer.  Both pointers are stable across
 * arbitrary VM calls - `parent_buf` is a malloc'd region the parent
 * Surface doesn't reallocate, kept alive via `parent_handle`. */
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
int
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
 * Cell.new() in Wren land - fill a standalone cell with the current
 * mode's default attribute, fg, bg, a space character, font 0, and
 * no hyperlink.  `parent` stays NULL so accessors operate on `c`. */
void
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

void
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

/* --- Surface allocate / finalize ---
 *
 * `Surface.new(width, height)`: allocates a width*height cell buffer
 * inited to space + the current default attribute, mirroring how a
 * fresh Cell would.  Zero or negative dimensions yield an empty
 * (count=0) buffer so the degenerate case is a silent no-op for
 * every primitive.  Finalize frees the malloc'd buffer. */
void
wren_surface_allocate(WrenVM *vm)
{
	int w = (int)wrenGetSlotDouble(vm, 1);
	int h = (int)wrenGetSlotDouble(vm, 2);
	if (w < 0) w = 0;
	if (h < 0) h = 0;

	struct wren_surface *sf =
	    wrenSetSlotNewForeign(vm, 0, 0, sizeof(*sf));
	sf->type     = SWF_SURFACE;
	sf->width    = w;
	sf->height   = h;
	sf->count    = w * h;
	sf->buf      = NULL;
	sf->borrowed = false;
	if (sf->count == 0)
		return;

	sf->buf = calloc((size_t)sf->count, sizeof *sf->buf);
	if (sf->buf == NULL) {
		sf->count  = 0;
		sf->width  = 0;
		sf->height = 0;
		return;
	}
	struct text_info ti;
	gettextinfo(&ti);
	uint32_t fg = 0, bg = 0;
	ciolib_attr2palette(ti.normattr, &fg, &bg);
	for (int i = 0; i < sf->count; i++) {
		sf->buf[i].ch           = 0x20;
		sf->buf[i].legacy_attr  = ti.normattr;
		sf->buf[i].fg           = fg;
		sf->buf[i].bg           = bg;
		sf->buf[i].font         = 0;
		sf->buf[i].hyperlink_id = 0;
	}
}

void
wren_surface_finalize(void *data)
{
	struct wren_surface *sf = data;
	if (!sf->borrowed)
		free(sf->buf);
	sf->buf    = NULL;
	sf->count  = 0;
	sf->width  = 0;
	sf->height = 0;
}

void
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

void
fnScreenWindow_deleteLine(WrenVM *vm)
{
	(void)vm;
	delline();
}

void
fnScreenWindow_insertLine(WrenVM *vm)
{
	(void)vm;
	insline();
}

void
fnScreenWindow_scroll(WrenVM *vm)
{
	(void)vm;
	wscroll();
}

/* ----- Screen.hyperlinkId - singleton current-hyperlink ID for new
 * cells produced by window output (putChar, print). */
void
fn_Screen_hyperlinkId(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)ciolib_get_current_hyperlink());
}

void
fn_Screen_hyperlinkId_set(WrenVM *vm)
{
	int id = (int)wrenGetSlotDouble(vm, 1);
	ciolib_set_current_hyperlink((uint16_t)id);
}

/* ----- Input.mouseVisible - setter only.  ciolib has no get
 * counterpart, and tracking it from script-side would drift if other
 * code shows or hides the cursor. */
void
fn_Input_mouseVisible_set(WrenVM *vm)
{
	if (wrenGetSlotBool(vm, 1))
		ciolib_showmouse();
	else
		ciolib_hidemouse();
}

/* ----- Input.mouseEvents getter / setter - opaque uint64 bitmask of
 * the ciolib_mouse_event types that get delivered.  Use with the
 * Mouse.* constants and the Input.{enable,disable}MouseEvent helpers
 * below.  Returning the raw mask lets the App save it on entry and
 * restore on exit so the surrounding terminal's mouse-mode (set by
 * term.c:setup_mouse_events) survives a UI run. */
void
fn_Input_mouseEvents(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)ciomouse_getevents());
}

void
fn_Input_mouseEvents_set(WrenVM *vm)
{
	uint64_t mask = (uint64_t)wrenGetSlotDouble(vm, 1);
	ciomouse_setevents(mask);
}

void
fn_Input_enableMouseEvent(WrenVM *vm)
{
	int ev = (int)wrenGetSlotDouble(vm, 1);
	ciomouse_addevent(ev);
}

void
fn_Input_disableMouseEvent(WrenVM *vm)
{
	int ev = (int)wrenGetSlotDouble(vm, 1);
	ciomouse_delevent(ev);
}

/* ----- Font.codepage / Font.codepageOf(i) - codepage of a font slot. */
void
fn_Font_codepage(WrenVM *vm)
{
	int f = getfont(0);
	if (f < 0 || f > 256) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotDouble(vm, 0, (double)conio_fontdata[f].cp);
}

void
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

/* Screen.font[i] - slot in 0..4 (5 active font slots in conio). */
void
fnScreenFonts_subscript(WrenVM *vm)
{
	int i = (int)wrenGetSlotDouble(vm, 1);
	if (i < 0 || i > 4) {
		wren_throw(vm,
		    "Screen.font[i]: slot index must be 0..4");
		return;
	}
	wrenSetSlotDouble(vm, 0, (double)getfont(i));
}

void
fnScreenFonts_subscript_set(WrenVM *vm)
{
	int i = (int)wrenGetSlotDouble(vm, 1);
	int n = (int)wrenGetSlotDouble(vm, 2);
	if (i < 0 || i > 4) {
		wren_throw(vm,
		    "Screen.font[i]=: slot index must be 0..4");
		return;
	}
	/* force=0: refuse the change if the font isn't available in the
	 * current video mode rather than silently switching modes. */
	setfont(n, 0, i);
}

/* ----- Screen.palette ----------------------------------------------
 *
 * Per-entry [i]/[i]= read/write a single palette color as 0xRRGGBB.
 * `.mode` and `.mode=` get/set the 16-color mode palette as a Wren
 * List of 16 colors.  Setpalette wants 16-bit r/g/b, getpalette
 * returns 8-bit - we keep the script side at 8-bit-per-channel
 * (0xRRGGBB) and scale on the way in (×257 = ×0x101). */

void
fnPalette_subscript(WrenVM *vm)
{
	int i = (int)wrenGetSlotDouble(vm, 1);
	if (i < 0) {
		wren_throw(vm,
		    "Screen.palette[i]: index must be non-negative");
		return;
	}
	uint8_t r, g, b;
	/* getpalette failure is a runtime condition (palette not loaded
	 * for this index in the current mode) - null is the documented
	 * "no entry" return. */
	if (ciolib_getpalette((uint32_t)i, &r, &g, &b) != 1) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	uint32_t v = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
	wrenSetSlotDouble(vm, 0, (double)v);
}

void
fnPalette_subscript_set(WrenVM *vm)
{
	int      i = (int)wrenGetSlotDouble(vm, 1);
	uint32_t c = (uint32_t)(int64_t)wrenGetSlotDouble(vm, 2);
	if (i < 0) {
		wren_throw(vm,
		    "Screen.palette[i]=: index must be non-negative");
		return;
	}
	uint16_t r = (uint16_t)(((c >> 16) & 0xFFu) * 0x101u);
	uint16_t g = (uint16_t)(((c >>  8) & 0xFFu) * 0x101u);
	uint16_t b = (uint16_t)(( c        & 0xFFu) * 0x101u);
	ciolib_setpalette((uint32_t)i, r, g, b);
}

void
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

void
fnPalette_mode_set(WrenVM *vm)
{
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_LIST) {
		wren_throw(vm,
		    "Screen.palette.mode=: must be a List");
		return;
	}
	if (wrenGetListCount(vm, 1) < 16) {
		wren_throw(vm,
		    "Screen.palette.mode=: List must have 16 elements");
		return;
	}
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

void
wren_custom_cursor_allocate(WrenVM *vm)
{
	struct wren_custom_cursor *c = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*c));
	c->type = SWF_CUSTOM_CURSOR;
	getcustomcursor(&c->startline, &c->endline, &c->range,
	    &c->blink, &c->visible);
}

void
wren_custom_cursor_finalize(void *data)
{
	(void)data;
}

#define CC_INT_FIELD(NAME, FIELD)                                                \
	void fnCustomCursor_##NAME##_static(WrenVM *vm)                  \
	{                                                                        \
		int startline, endline, range, blink, visible;                   \
		getcustomcursor(&startline, &endline, &range, &blink, &visible); \
		wrenSetSlotDouble(vm, 0, (double)(FIELD));                       \
	}                                                                        \
	void fnCustomCursor_##NAME##_set_static(WrenVM *vm)              \
	{                                                                        \
		int startline, endline, range, blink, visible;                   \
		getcustomcursor(&startline, &endline, &range, &blink, &visible); \
		FIELD = (int)wrenGetSlotDouble(vm, 1);                           \
		setcustomcursor(startline, endline, range, blink, visible);      \
	}                                                                        \
	void fnCustomCursor_##NAME(WrenVM *vm)                           \
	{                                                                        \
		struct wren_custom_cursor *c = wrenGetSlotForeign(vm, 0);        \
		wrenSetSlotDouble(vm, 0, (double)c->FIELD);                      \
	}                                                                        \
	void fnCustomCursor_##NAME##_set(WrenVM *vm)                     \
	{                                                                        \
		struct wren_custom_cursor *c = wrenGetSlotForeign(vm, 0);        \
		c->FIELD = (int)wrenGetSlotDouble(vm, 1);                        \
	}

#define CC_BOOL_FIELD(NAME, FIELD)                                               \
	void fnCustomCursor_##NAME##_static(WrenVM *vm)                  \
	{                                                                        \
		int startline, endline, range, blink, visible;                   \
		getcustomcursor(&startline, &endline, &range, &blink, &visible); \
		wrenSetSlotBool(vm, 0, (FIELD) != 0);                            \
	}                                                                        \
	void fnCustomCursor_##NAME##_set_static(WrenVM *vm)              \
	{                                                                        \
		int startline, endline, range, blink, visible;                   \
		getcustomcursor(&startline, &endline, &range, &blink, &visible); \
		FIELD = wrenGetSlotBool(vm, 1) ? 1 : 0;                          \
		setcustomcursor(startline, endline, range, blink, visible);      \
	}                                                                        \
	void fnCustomCursor_##NAME(WrenVM *vm)                           \
	{                                                                        \
		struct wren_custom_cursor *c = wrenGetSlotForeign(vm, 0);        \
		wrenSetSlotBool(vm, 0, c->FIELD != 0);                           \
	}                                                                        \
	void fnCustomCursor_##NAME##_set(WrenVM *vm)                     \
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

void
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
void
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

void
wren_video_flags_allocate(WrenVM *vm)
{
	struct wren_video_flags *vf = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*vf));
	vf->type  = SWF_VIDEO_FLAGS;
	vf->flags = getvideoflags();
}

void
wren_video_flags_finalize(void *data)
{
	(void)data;
}

#define VF_FLAG(NAME, BIT)                                                       \
	void fnVideoFlags_##NAME##_static(WrenVM *vm)                    \
	{                                                                        \
		wrenSetSlotBool(vm, 0, (getvideoflags() & (BIT)) != 0);          \
	}                                                                        \
	void fnVideoFlags_##NAME##_set_static(WrenVM *vm)                \
	{                                                                        \
		int flags = getvideoflags();                                     \
		if (wrenGetSlotBool(vm, 1)) flags |= (BIT);                      \
		else                        flags &= ~(BIT);                     \
		setvideoflags(flags);                                            \
	}                                                                        \
	void fnVideoFlags_##NAME(WrenVM *vm)                             \
	{                                                                        \
		struct wren_video_flags *vf = wrenGetSlotForeign(vm, 0);         \
		wrenSetSlotBool(vm, 0, (vf->flags & (BIT)) != 0);                \
	}                                                                        \
	void fnVideoFlags_##NAME##_set(WrenVM *vm)                       \
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
 * fixed at video-mode init.  Getter only - no setter. */
void
fnVideoFlags_expand_static(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, (getvideoflags() & CIOLIB_VIDEO_EXPAND) != 0);
}

void
fnVideoFlags_expand(WrenVM *vm)
{
	struct wren_video_flags *vf = wrenGetSlotForeign(vm, 0);
	wrenSetSlotBool(vm, 0, (vf->flags & CIOLIB_VIDEO_EXPAND) != 0);
}

void
fnVideoFlags_apply(WrenVM *vm)
{
	struct wren_video_flags *vf = wrenGetSlotForeign(vm, 0);
	setvideoflags(vf->flags);
}

/* ----- Color ------------------------------------------------------- */

void
fnColor_fromRgb(WrenVM *vm)
{
	int r = (int)wrenGetSlotDouble(vm, 1);
	int g = (int)wrenGetSlotDouble(vm, 2);
	int b = (int)wrenGetSlotDouble(vm, 3);
	/* ciolib_map_rgb returns the value with the high RGB bit set; we
	 * mask it off - Cell.fgRgb=/bgRgb= add the bit when writing.
	 * Returning the raw 24-bit value keeps the script side simpler. */
	uint32_t v = ciolib_map_rgb((uint16_t)r, (uint16_t)g, (uint16_t)b)
	    & 0x00FFFFFFu;
	wrenSetSlotDouble(vm, 0, (double)v);
}

void
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

void
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

/* Screen.readRect returns a Surface - the rect's width/height come
 * along with the cell buffer.  Subscript, count, iteration, and
 * writeRect-as-source all operate on the same linear buffer the C
 * side allocated here. */
void
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

	int    w = ex - sx + 1;
	int    h = ey - sy + 1;
	size_t n = (size_t)w * (size_t)h;
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
	load_class_into_slot(vm, &st->surface_class, "Surface", 1);
	struct wren_surface *sf =
	    wrenSetSlotNewForeign(vm, 0, 1, sizeof(*sf));
	sf->type     = SWF_SURFACE;
	sf->width    = w;
	sf->height   = h;
	sf->count    = (int)n;
	sf->buf      = buf;
	sf->borrowed = false;
}

/* Screen.putRect(src, dstX, dstY): blit a Surface to the screen at
 * (dstX, dstY).  dstX/dstY are 1-based screen coords (matching the
 * rest of the screen API).  Out-of-screen regions are clipped
 * silently. */
void
fn_Screen_putRect_3(WrenVM *vm)
{
	struct vmem_cell *src_buf;
	int src_w, src_h;
	if (!slot_to_surface_(vm, 1, &src_buf, &src_w, &src_h)) {
		wren_throw(vm, "putRect: source must be a Surface");
		return;
	}
	int dstX = (int)wrenGetSlotDouble(vm, 2);
	int dstY = (int)wrenGetSlotDouble(vm, 3);

	struct text_info ti;
	gettextinfo(&ti);
	int scrW = ti.screenwidth;
	int scrH = ti.screenheight;

	int srcX = 0, srcY = 0, w = src_w, h = src_h;
	/* Clip on the screen side: dst (1-based) + extent must fit in
	 * [1..scrW] × [1..scrH]. */
	if (dstX < 1) {
		w   += dstX - 1;
		srcX -= dstX - 1;
		dstX = 1;
	}
	if (dstY < 1) {
		h   += dstY - 1;
		srcY -= dstY - 1;
		dstY = 1;
	}
	if (dstX + w - 1 > scrW) w = scrW - dstX + 1;
	if (dstY + h - 1 > scrH) h = scrH - dstY + 1;
	if (w <= 0 || h <= 0)    return;

	/* If we don't need to copy a sub-rect, hand the source buffer
	 * straight to puttext; otherwise gather a temp. */
	if (srcX == 0 && srcY == 0 && w == src_w && h == src_h) {
		ciolib_vmem_puttext(dstX, dstY, dstX + w - 1, dstY + h - 1,
		    src_buf);
		return;
	}
	struct vmem_cell *tmp = malloc((size_t)w * (size_t)h * sizeof(*tmp));
	if (tmp == NULL) return;
	for (int row = 0; row < h; row++) {
		memcpy(tmp + row * w,
		    src_buf + (srcY + row) * src_w + srcX,
		    (size_t)w * sizeof(*tmp));
	}
	ciolib_vmem_puttext(dstX, dstY, dstX + w - 1, dstY + h - 1, tmp);
	free(tmp);
}

/* Screen.putRect(src, srcX, srcY, srcW, srcH, dstX, dstY): blit a
 * sub-rect of `src` to the screen.  C-callable arity; the Wren-side
 * Screen declares a putRect(src, srcRect, dstX, dstY) wrapper. */
void
fn_Screen_putRect_7(WrenVM *vm)
{
	struct vmem_cell *src_buf;
	int src_w, src_h;
	if (!slot_to_surface_(vm, 1, &src_buf, &src_w, &src_h)) {
		wren_throw(vm, "putRect: source must be a Surface");
		return;
	}
	int srcX = (int)wrenGetSlotDouble(vm, 2);
	int srcY = (int)wrenGetSlotDouble(vm, 3);
	int srcW = (int)wrenGetSlotDouble(vm, 4);
	int srcH = (int)wrenGetSlotDouble(vm, 5);
	int dstX = (int)wrenGetSlotDouble(vm, 6);
	int dstY = (int)wrenGetSlotDouble(vm, 7);

	/* Clip srcRect against source bounds first. */
	if (srcX < 0) { srcW += srcX; dstX -= srcX; srcX = 0; }
	if (srcY < 0) { srcH += srcY; dstY -= srcY; srcY = 0; }
	if (srcX + srcW > src_w) srcW = src_w - srcX;
	if (srcY + srcH > src_h) srcH = src_h - srcY;

	struct text_info ti;
	gettextinfo(&ti);
	int scrW = ti.screenwidth;
	int scrH = ti.screenheight;
	/* Then clip on the screen side. */
	if (dstX < 1) { srcW += dstX - 1; srcX -= dstX - 1; dstX = 1; }
	if (dstY < 1) { srcH += dstY - 1; srcY -= dstY - 1; dstY = 1; }
	if (dstX + srcW - 1 > scrW) srcW = scrW - dstX + 1;
	if (dstY + srcH - 1 > scrH) srcH = scrH - dstY + 1;
	if (srcW <= 0 || srcH <= 0) return;

	struct vmem_cell *tmp = malloc((size_t)srcW * (size_t)srcH * sizeof(*tmp));
	if (tmp == NULL) return;
	for (int row = 0; row < srcH; row++) {
		memcpy(tmp + row * srcW,
		    src_buf + (srcY + row) * src_w + srcX,
		    (size_t)srcW * sizeof(*tmp));
	}
	ciolib_vmem_puttext(dstX, dstY, dstX + srcW - 1, dstY + srcH - 1, tmp);
	free(tmp);
}

void
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

	/* Surface: contiguous vmem_cell buffer - hand straight to puttext
	 * with no allocation or copy. */
	if (arg_type == WREN_TYPE_FOREIGN &&
	    slot_foreign_type(vm, 5) == SWF_SURFACE) {
		struct wren_surface *sf = wrenGetSlotForeign(vm, 5);
		if (sf->count != n_expected) {
			wrenEnsureSlots(vm, 1);
			wrenSetSlotString(vm, 0,
			    "putText: Surface size does not match region size");
			wrenAbortFiber(vm, 0);
			return;
		}
		int rv = ciolib_vmem_puttext(sx, sy, ex, ey, sf->buf);
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

void
fn_Input_mousedrag(WrenVM *vm)
{
	bool force_rect = false;
	if (wrenGetSlotCount(vm) > 1 &&
	    wrenGetSlotType(vm, 1) == WREN_TYPE_BOOL)
		force_rect = wrenGetSlotBool(vm, 1);
	mousedrag(NULL, force_rect);
}

/* ----- Cell field accessors ----- */

void
fn_Cell_ch(WrenVM *vm)
{
	struct vmem_cell *d  = slot_cell_data(vm, 0);
	uint32_t          cp = cpoint_from_cpchar(CIOLIB_CP437, d->ch);
	char              buf[4];
	int               len = encode_utf8(cp, buf);
	wrenSetSlotBytes(vm, 0, buf, (size_t)len);
}

void
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

void
fn_Cell_chByte(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)d->ch);
}

void
fn_Cell_chByte_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	d->ch = (uint8_t)(int)wrenGetSlotDouble(vm, 1);
}

void
fn_Cell_font(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)d->font);
}

void
fn_Cell_font_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	d->font = (uint8_t)(int)wrenGetSlotDouble(vm, 1);
}

void
fn_Cell_legacyAttr(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)d->legacy_attr);
}

void
fn_Cell_legacyAttr_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	d->legacy_attr = (uint8_t)(int)wrenGetSlotDouble(vm, 1);
}

void
fn_Cell_bright(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	wrenSetSlotBool(vm, 0, (d->legacy_attr & 0x08) != 0);
}

void
fn_Cell_bright_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	if (wrenGetSlotBool(vm, 1))
		d->legacy_attr |= 0x08u;
	else
		d->legacy_attr &= (uint8_t)~0x08u;
}

void
fn_Cell_blink(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	wrenSetSlotBool(vm, 0, (d->legacy_attr & 0x80) != 0);
}

void
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

void
fn_Cell_fgPalette(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	if (d->fg & CIOLIB_COLOR_RGB)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotDouble(vm, 0, (double)(d->fg & 0x00FFFFFFu));
}

void
fn_Cell_fgPalette_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	uint32_t          n = (uint32_t)(int64_t)wrenGetSlotDouble(vm, 1);
	d->fg = (d->fg & 0x7F000000u) | (n & 0x00FFFFFFu);
}

void
fn_Cell_fgRgb(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	if (!(d->fg & CIOLIB_COLOR_RGB))
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotDouble(vm, 0, (double)(d->fg & 0x00FFFFFFu));
}

void
fn_Cell_fgRgb_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	uint32_t          n = (uint32_t)(int64_t)wrenGetSlotDouble(vm, 1);
	d->fg = (d->fg & 0x7F000000u) | 0x80000000u | (n & 0x00FFFFFFu);
}

void
fn_Cell_bgPalette(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	if (d->bg & CIOLIB_COLOR_RGB)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotDouble(vm, 0, (double)(d->bg & 0x00FFFFFFu));
}

void
fn_Cell_bgPalette_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	uint32_t          n = (uint32_t)(int64_t)wrenGetSlotDouble(vm, 1);
	d->bg = (d->bg & 0x7F000000u) | (n & 0x00FFFFFFu);
}

void
fn_Cell_bgRgb(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	if (!(d->bg & CIOLIB_COLOR_RGB))
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotDouble(vm, 0, (double)(d->bg & 0x00FFFFFFu));
}

void
fn_Cell_bgRgb_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	uint32_t          n = (uint32_t)(int64_t)wrenGetSlotDouble(vm, 1);
	d->bg = (d->bg & 0x7F000000u) | 0x80000000u | (n & 0x00FFFFFFu);
}

void
fn_Cell_hyperlinkId(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)d->hyperlink_id);
}

void
fn_Cell_toString(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	char buf[48];
	snprintf(buf, sizeof(buf), "Cell(0x%02X attr=0x%02X)",
	    (unsigned)d->ch, (unsigned)d->legacy_attr);
	wrenSetSlotString(vm, 0, buf);
}

void
fn_Cell_hyperlinkId_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	d->hyperlink_id = (uint16_t)(int)wrenGetSlotDouble(vm, 1);
}

/* Cell.eqContent(other) - structural equality of every content field
 * of two cells.  NOT a == override (foreign `==` stays identity-based,
 * matching every other foreign in the API).  Field-by-field; memcmp
 * would compare struct padding and report false negatives.
 *
 * Equality rules:
 *  - non-Cell `other` is always false (no fiber abort).
 *  - the BG dirty bit (CIOLIB_BG_DIRTY) is render bookkeeping, not
 *    content - masked out before comparison.
 *  - if EITHER cell flies CIOLIB_BG_PIXEL_GRAPHICS, the cells reference
 *    pixel data outside the vmem_cell and we can't prove equality from
 *    these fields alone - return false.  Two pixel-graphics cells are
 *    therefore never eqContent-equal even if their other fields match.
 *  - everything else is compared bit-exact, including the Prestel
 *    control char in fg, the double-height/Prestel-mode/etc. flags in
 *    bg, and the palette-vs-RGB high bit. */
void
fn_Cell_eqContent(WrenVM *vm)
{
	if (slot_foreign_type(vm, 1) != SWF_CELL) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	struct vmem_cell *a = slot_cell_data(vm, 0);
	struct vmem_cell *b = slot_cell_data(vm, 1);
	if ((a->bg | b->bg) & CIOLIB_BG_PIXEL_GRAPHICS) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	uint32_t mask = ~(uint32_t)CIOLIB_BG_DIRTY;
	bool eq = a->ch           == b->ch &&
	          a->font         == b->font &&
	          a->legacy_attr  == b->legacy_attr &&
	          a->fg           == b->fg &&
	          (a->bg & mask)  == (b->bg & mask) &&
	          a->hyperlink_id == b->hyperlink_id;
	wrenSetSlotBool(vm, 0, eq);
}

/* ----- Surface accessors ----- */

void
fn_Surface_count(WrenVM *vm)
{
	struct wren_surface *sf = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)sf->count);
}

/* Materialize a Cell view into slot 0.  Pins the parent Surface
 * (slot 0 receiver) via a Wren handle so the buffer outlives the
 * view. */
static void
surface_make_view(WrenVM *vm, int idx)
{
	struct wren_surface *sf = wrenGetSlotForeign(vm, 0);
	if (idx < 0 || idx >= sf->count) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	struct wren_host_state *st = wren_host_state();
	if (st == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	/* Capture sf->buf before the slot-0 overwrite - it's a malloc'd
	 * pointer that stays valid as long as the parent Surface lives,
	 * which we ensure by pinning it via parent_handle. */
	struct vmem_cell *parent_buf    = sf->buf;
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

void
fn_Surface_subscript(WrenVM *vm)
{
	struct wren_surface *sf = wrenGetSlotForeign(vm, 0);
	int i = (int)wrenGetSlotDouble(vm, 1);
	if (i < 0 || i >= sf->count) {
		wren_throw(vm,
		    "Surface[i]: index out of bounds");
		return;
	}
	surface_make_view(vm, i);
}

/* Wren iteration: null -> 0; n -> n+1; past end -> false. */
void
fn_Surface_iterate(WrenVM *vm)
{
	struct wren_surface *sf = wrenGetSlotForeign(vm, 0);
	if (sf->count == 0) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	if (wrenGetSlotType(vm, 1) == WREN_TYPE_NULL) {
		wrenSetSlotDouble(vm, 0, 0.0);
		return;
	}
	int next = (int)wrenGetSlotDouble(vm, 1) + 1;
	if (next >= sf->count)
		wrenSetSlotBool(vm, 0, false);
	else
		wrenSetSlotDouble(vm, 0, (double)next);
}

void
fn_Surface_iteratorValue(WrenVM *vm)
{
	int i = (int)wrenGetSlotDouble(vm, 1);
	surface_make_view(vm, i);
}

void
fn_Surface_width(WrenVM *vm)
{
	struct wren_surface *sf = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)sf->width);
}

void
fn_Surface_height(WrenVM *vm)
{
	struct wren_surface *sf = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)sf->height);
}

/* `surface.cellAt(x, y)`: 2D-indexed Cell view.  Returns null when
 * (x, y) is outside the surface's bounds. */
void
fn_Surface_cellAt(WrenVM *vm)
{
	struct wren_surface *sf = wrenGetSlotForeign(vm, 0);
	int x = (int)wrenGetSlotDouble(vm, 1);
	int y = (int)wrenGetSlotDouble(vm, 2);
	if (x < 0 || x >= sf->width || y < 0 || y >= sf->height) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	surface_make_view(vm, y * sf->width + x);
}

/* Surface.urlAt(col, row): wraps detect_url_at() against the
 * surface's cell buffer.  Returns the detected URL as a String, or
 * null if the click falls on a non-URL character (or outside bounds).
 * The C side malloc's the result and we copy it into a Wren slot
 * before freeing. */
void
fn_Surface_urlAt(WrenVM *vm)
{
	struct wren_surface *sf = wrenGetSlotForeign(vm, 0);
	int col = (int)wrenGetSlotDouble(vm, 1);
	int row = (int)wrenGetSlotDouble(vm, 2);
	if (sf->buf == NULL || sf->width <= 0 || sf->height <= 0) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	char *url = detect_url_at(sf->buf, sf->width, sf->height, col, row);
	if (url == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotString(vm, 0, url);
	free(url);
}

/* Compute the clipped overlap of the source's [srcX..srcX+srcW) x
 * [srcY..srcY+srcH) region with both the source bounds and the
 * destination's [0..dst->width) x [0..dst->height) at offset
 * (dstX, dstY).  Writes adjusted offsets into *src_out / *dst_out and
 * the clipped width/height into *w_out / *h_out.  Returns false if
 * the clipped region is empty (caller should bail). */
static bool
clip_rect_(struct wren_surface *dst,
    int src_w, int src_h,
    int srcX, int srcY, int srcW, int srcH,
    int dstX, int dstY,
    int *out_srcX, int *out_srcY,
    int *out_dstX, int *out_dstY,
    int *out_w,    int *out_h)
{
	/* Clip src rect against the source's bounds. */
	if (srcX < 0) {
		srcW += srcX;
		dstX -= srcX;
		srcX = 0;
	}
	if (srcY < 0) {
		srcH += srcY;
		dstY -= srcY;
		srcY = 0;
	}
	if (srcX + srcW > src_w) srcW = src_w - srcX;
	if (srcY + srcH > src_h) srcH = src_h - srcY;
	/* Clip the dst placement against dst bounds. */
	if (dstX < 0) {
		srcW += dstX;
		srcX -= dstX;
		dstX = 0;
	}
	if (dstY < 0) {
		srcH += dstY;
		srcY -= dstY;
		dstY = 0;
	}
	if (dstX + srcW > dst->width)  srcW = dst->width  - dstX;
	if (dstY + srcH > dst->height) srcH = dst->height - dstY;

	if (srcW <= 0 || srcH <= 0)
		return false;

	*out_srcX = srcX;
	*out_srcY = srcY;
	*out_dstX = dstX;
	*out_dstY = dstY;
	*out_w    = srcW;
	*out_h    = srcH;
	return true;
}

/* Resolve slot to a Surface and surface its (buf, width, height) for
 * the rect-blit primitives.  Accepts:
 *   - a regular `Surface` foreign, OR
 *   - the `Scrollback` class object, in which case the live ring is
 *     linearized and surfaced as if it were a borrowed Surface.
 * Returns false on type mismatch. */
static bool
slot_to_surface_(WrenVM *vm, int slot,
    struct vmem_cell **buf_out, int *w_out, int *h_out)
{
	if (slot_foreign_type(vm, slot) == SWF_SURFACE) {
		struct wren_surface *sf = wrenGetSlotForeign(vm, slot);
		*buf_out = sf->buf;
		*w_out   = sf->width;
		*h_out   = sf->height;
		return true;
	}

	/* Scrollback class object → linearized ring view. */
	struct wren_host_state *st = wren_host_state();
	if (st == NULL)
		return false;
	if (st->scrollback_class == NULL) {
		/* Lazy load: use a slot beyond any expected arg. */
		wrenEnsureSlots(vm, 16);
		wrenGetVariable(vm, "syncterm", "Scrollback", 15);
		st->scrollback_class = wrenGetSlotHandle(vm, 15);
		if (st->scrollback_class == NULL)
			return false;
	}
	Value v = vm->apiStack[slot];
	if (!IS_CLASS(v))
		return false;
	if (AS_CLASS(v) != AS_CLASS(st->scrollback_class->value))
		return false;
	if (cterm == NULL || cterm->scrollback == NULL)
		return false;
	scrollback_linearize_();
	*buf_out = cterm->scrollback;
	*w_out   = cterm->backwidth;
	*h_out   = cterm->backfilled;
	return true;
}

/* `surface.putRect(src, dstX, dstY)` - paste the entirety of `src`
 * at (dstX, dstY) in self.  Out-of-bounds portions are clipped
 * silently. */
void
fn_Surface_putRect_3(WrenVM *vm)
{
	struct wren_surface *dst = wrenGetSlotForeign(vm, 0);
	struct vmem_cell    *src_buf;
	int src_w, src_h;
	if (!slot_to_surface_(vm, 1, &src_buf, &src_w, &src_h)) {
		wren_throw(vm, "putRect: source must be a Surface");
		return;
	}
	int dstX = (int)wrenGetSlotDouble(vm, 2);
	int dstY = (int)wrenGetSlotDouble(vm, 3);

	int sx, sy, dx, dy, w, h;
	if (!clip_rect_(dst, src_w, src_h, 0, 0, src_w, src_h,
	    dstX, dstY, &sx, &sy, &dx, &dy, &w, &h))
		return;

	for (int row = 0; row < h; row++) {
		struct vmem_cell *s = src_buf + (sy + row) * src_w + sx;
		struct vmem_cell *d = dst->buf  + (dy + row) * dst->width + dx;
		memcpy(d, s, (size_t)w * sizeof(*d));
	}
}

/* `surface.putRect(src, srcX, srcY, srcW, srcH, dstX, dstY)` - paste a
 * sub-rect of `src`.  This is the C-callable arity; the Wren-side
 * Surface declares a `putRect(src, srcRect, dstX, dstY)` wrapper that
 * unpacks Rect.x/y/w/h before calling here. */
void
fn_Surface_putRect_7(WrenVM *vm)
{
	struct wren_surface *dst = wrenGetSlotForeign(vm, 0);
	struct vmem_cell    *src_buf;
	int src_w, src_h;
	if (!slot_to_surface_(vm, 1, &src_buf, &src_w, &src_h)) {
		wren_throw(vm, "putRect: source must be a Surface");
		return;
	}
	int srcX = (int)wrenGetSlotDouble(vm, 2);
	int srcY = (int)wrenGetSlotDouble(vm, 3);
	int srcW = (int)wrenGetSlotDouble(vm, 4);
	int srcH = (int)wrenGetSlotDouble(vm, 5);
	int dstX = (int)wrenGetSlotDouble(vm, 6);
	int dstY = (int)wrenGetSlotDouble(vm, 7);

	int sx, sy, dx, dy, w, h;
	if (!clip_rect_(dst, src_w, src_h, srcX, srcY, srcW, srcH,
	    dstX, dstY, &sx, &sy, &dx, &dy, &w, &h))
		return;

	for (int row = 0; row < h; row++) {
		struct vmem_cell *s = src_buf + (sy + row) * src_w + sx;
		struct vmem_cell *d = dst->buf  + (dy + row) * dst->width + dx;
		memcpy(d, s, (size_t)w * sizeof(*d));
	}
}

/* `surface.fill_(x, y, w, h, cell)` - bulk-fill a rect with a single
 * Cell template.  C-callable primitive; Wren declares a
 * `fill(rect, cell)` convenience that unpacks Rect.x/y/w/h. */
void
fn_Surface_fill_(WrenVM *vm)
{
	struct wren_surface *sf = wrenGetSlotForeign(vm, 0);
	int x = (int)wrenGetSlotDouble(vm, 1);
	int y = (int)wrenGetSlotDouble(vm, 2);
	int w = (int)wrenGetSlotDouble(vm, 3);
	int h = (int)wrenGetSlotDouble(vm, 4);
	if (slot_foreign_type(vm, 5) != SWF_CELL) {
		wren_throw(vm, "fill: 5th arg must be a Cell template");
		return;
	}
	struct wren_cell *tmpl = wrenGetSlotForeign(vm, 5);
	struct vmem_cell  src  = *cell_data(tmpl);

	/* Clip rect to surface bounds. */
	if (x < 0)         { w += x; x = 0; }
	if (y < 0)         { h += y; y = 0; }
	if (x + w > sf->width)  w = sf->width  - x;
	if (y + h > sf->height) h = sf->height - y;
	if (w <= 0 || h <= 0)   return;

	for (int row = 0; row < h; row++) {
		struct vmem_cell *p = sf->buf + (y + row) * sf->width + x;
		for (int col = 0; col < w; col++)
			*p++ = src;
	}
}

/* `surface.toString` - diagnostic. */
void
fn_Surface_toString(WrenVM *vm)
{
	struct wren_surface *sf = wrenGetSlotForeign(vm, 0);
	char buf[48];
	snprintf(buf, sizeof(buf), "Surface(%dx%d)", sf->width, sf->height);
	wrenSetSlotString(vm, 0, buf);
}

/* ----- Font ----- */

void
fn_Font_name(WrenVM *vm)
{
	int i = (int)wrenGetSlotDouble(vm, 1);
	if (i < 0 || i > 256 || conio_fontdata[i].desc == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotString(vm, 0, conio_fontdata[i].desc);
}

void
fn_Font_count(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, 257.0);
}

void
fn_Font_available(WrenVM *vm)
{
	int i = (int)wrenGetSlotDouble(vm, 1);
	wrenSetSlotBool(vm, 0, ciolib_checkfont(i) != 0);
}

/* Font.load(file) — load a font from a File foreign (typically the
 * return value of Host.pickFile from a font-load picker dialog).
 * Returns true on success.  Takes a File rather than a path string
 * so the consent token model holds — Wren never sees the bare path,
 * and only paths the user explicitly approved through a picker can
 * be opened.  Gated by ScreenSupports.loadableFonts (text-mode
 * backends don't support runtime font swap). */
void
fn_Font_load(WrenVM *vm)
{
	const char *path = wren_file_path(vm, 1);
	if (path == NULL) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	wrenSetSlotBool(vm, 0, ciolib_loadfont(path) != 0);
}

/* ----- Hyperlinks (Map-like read interface) ----- */

void
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

void
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

void
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

void
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


/* ----- Scrollback ------------------------------------------------- */

/* Scrollback presents the process-wide scrollback ring as a
 * Surface-shaped, static-only foreign class.  The ring lives in
 * `cterm->scrollback` (a `vmem_cell *` of `backwidth × backlines`).
 * `backstart` marks the oldest valid row; `backfilled` is the count
 * of valid rows.  When the ring hasn't wrapped (`backstart == 0`)
 * the cell layout is already linear and Wren-side reads / writes
 * map to the buffer directly.  When it HAS wrapped, an in-place
 * row-wise rotation runs at the moment Wren first asks for a
 * range-spanning view, normalizing `backstart` back to 0.
 *
 * `Scrollback.pushScreen()` walks the live screen rows into the
 * ring (just like cterm's own line-to-scrollback path); the oldest
 * rows are evicted on overflow.  `popScreen(_)` restores the ring
 * counters from a saved snapshot taken at push time, so the modal
 * scrollback viewer can hand the ring back unchanged on exit
 * (modulo the few rows that got clobbered when overflow evicted
 * them - same outcome as the equivalent count of natural
 * scrollback writes).
 *
 * Wren can't inherit from a foreign class, so Scrollback is its own
 * foreign class with linearize-and-dispatch wrappers for the
 * Surface contract; an `is(_)` override on the Wren side reports
 * `Scrollback is Surface == true` for ad-hoc type checks. */

/* Rotate the ring in-place so `backstart` becomes 0.  No-op when
 * already linear (the typical case once data has been written).
 * Three-reverses: reverse [0..k) rows, reverse [k..N) rows, reverse
 * [0..N) rows - each "reverse" swaps rows pairwise via a single-row
 * temp buffer. */
static void
scrollback_linearize_(void)
{
	if (cterm == NULL || cterm->scrollback == NULL)
		return;
	int k = cterm->backstart;
	if (k == 0)
		return;

	int rows = cterm->backlines;
	int cols = cterm->backwidth;
	struct vmem_cell *buf = cterm->scrollback;
	struct vmem_cell *tmp = malloc((size_t)cols * sizeof(*tmp));
	if (tmp == NULL)
		return;

	#define SWAP_ROW(a, b) do {                                          \
		memcpy(tmp,                buf + (size_t)(a) * cols,             \
		    (size_t)cols * sizeof(*buf));                                \
		memcpy(buf + (size_t)(a) * cols, buf + (size_t)(b) * cols,       \
		    (size_t)cols * sizeof(*buf));                                \
		memcpy(buf + (size_t)(b) * cols, tmp,                            \
		    (size_t)cols * sizeof(*buf));                                \
	} while (0)

	for (int i = 0, j = k - 1; i < j; i++, j--)
		SWAP_ROW(i, j);
	for (int i = k, j = rows - 1; i < j; i++, j--)
		SWAP_ROW(i, j);
	for (int i = 0, j = rows - 1; i < j; i++, j--)
		SWAP_ROW(i, j);
	#undef SWAP_ROW

	free(tmp);

	/* After rotation, backstart=0 and the next write head sits past
	 * the valid data (or wraps to 0 when the ring was already full). */
	cterm->backstart = 0;
	cterm->backpos   = (cterm->backfilled == cterm->backlines)
	    ? 0 : cterm->backfilled;
}

/* Linearize the ring, then install a borrowed wren_surface foreign
 * in slot 0 (overwriting the Scrollback class object that was the
 * receiver).  Every subsequent slot-0 Surface foreign method body
 * sees the ring as a normal Surface - same buf / width / height
 * shape - and the borrowed flag prevents the eventual finalizer
 * from freeing cterm->scrollback.  Returns false if there's no
 * ring; the caller should set slot 0 to a sentinel and return.
 *
 * This is the linearize-and-dispatch shim every Scrollback static
 * method shares: the wrappers are uniform two-liners, so the entire
 * read-side Surface contract passes through without per-method
 * duplication. */
static bool
scrollback_install_(WrenVM *vm)
{
	if (cterm == NULL || cterm->scrollback == NULL)
		return false;
	struct wren_host_state *st = wren_host_state();
	if (st == NULL)
		return false;

	scrollback_linearize_();

	int w = cterm->backwidth;
	int h = cterm->backfilled;

	/* Use a scratch slot ABOVE every method arg the dispatched
	 * Surface foreign might read.  The widest Surface method is
	 * putRect_(7 args), so slots 1..7 are live; pick slot 8.
	 * Clobbering slot 1 here would silently replace `x` in
	 * cellAt(x, y) etc. with the Surface class object, which
	 * wrenGetSlotDouble then reads as garbage and the bounds check
	 * trips. */
	wrenEnsureSlots(vm, 9);
	load_class_into_slot(vm, &st->surface_class, "Surface", 8);
	struct wren_surface *sf =
	    wrenSetSlotNewForeign(vm, 0, 8, sizeof(*sf));
	sf->type     = SWF_SURFACE;
	sf->buf      = cterm->scrollback;
	sf->width    = w;
	sf->height   = h;
	sf->count    = w * h;
	sf->borrowed = true;
	return true;
}

/* Each Scrollback read method is two lines: install the synthetic
 * Surface in slot 0, dispatch to the regular Surface foreign body.
 * Failure path (no cterm / no ring) sets a method-appropriate
 * sentinel - null for cell / view / string returns, 0 for numeric
 * accessors. */
#define SCROLLBACK_DISPATCH_NULL(name, fn_surface)                   \
	void fn_Scrollback_##name(WrenVM *vm) {                          \
		if (!scrollback_install_(vm)) { wrenSetSlotNull(vm, 0); return; } \
		fn_surface(vm);                                              \
	}

#define SCROLLBACK_DISPATCH_NUM(name, fn_surface)                    \
	void fn_Scrollback_##name(WrenVM *vm) {                          \
		if (!scrollback_install_(vm)) { wrenSetSlotDouble(vm, 0, 0.0); return; } \
		fn_surface(vm);                                              \
	}

SCROLLBACK_DISPATCH_NUM (width,         fn_Surface_width)
SCROLLBACK_DISPATCH_NUM (height,        fn_Surface_height)
SCROLLBACK_DISPATCH_NUM (count,         fn_Surface_count)
SCROLLBACK_DISPATCH_NULL(subscript,     fn_Surface_subscript)
SCROLLBACK_DISPATCH_NULL(iterate,       fn_Surface_iterate)
SCROLLBACK_DISPATCH_NULL(iteratorValue, fn_Surface_iteratorValue)
SCROLLBACK_DISPATCH_NULL(cellAt,        fn_Surface_cellAt)
SCROLLBACK_DISPATCH_NULL(urlAt,         fn_Surface_urlAt)
SCROLLBACK_DISPATCH_NULL(putRect_3,     fn_Surface_putRect_3)
SCROLLBACK_DISPATCH_NULL(putRect_7,     fn_Surface_putRect_7)
SCROLLBACK_DISPATCH_NULL(fill_,         fn_Surface_fill_)
SCROLLBACK_DISPATCH_NULL(toString,      fn_Surface_toString)

#undef SCROLLBACK_DISPATCH_NULL
#undef SCROLLBACK_DISPATCH_NUM

/* Push the cterm region's visible rows into the ring, row by row.
 * Only the cterm area itself - the status bar (which sits below
 * cterm at term.y + term.height) is excluded, otherwise the viewer
 * sees a duplicated copy of it as the most recently pushed row.
 * Per-row logic mirrors cterm_line_to_scrollback (which is static
 * inside cterm.c, so we replicate rather than expose it).  The
 * ring's pre-push counters are stashed on wren_host_state so
 * popScreen can restore them. */
void
fn_Scrollback_pushScreen(WrenVM *vm)
{
	(void)vm;
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || cterm == NULL || cterm->scrollback == NULL)
		return;

	st->scrollback_save_valid  = true;
	st->scrollback_save_start  = cterm->backstart;
	st->scrollback_save_pos    = cterm->backpos;
	st->scrollback_save_filled = cterm->backfilled;

	int copy_w = cterm->backwidth;
	if (copy_w > cterm->width)
		copy_w = cterm->width;

	for (int i = 0; i < cterm->height; i++) {
		int row = cterm->y + i;
		cterm->backfilled++;
		if (cterm->backfilled > cterm->backlines) {
			cterm->backfilled--;
			cterm->backstart++;
			if (cterm->backstart == cterm->backlines)
				cterm->backstart = 0;
		}
		struct vmem_cell *dst =
		    cterm->scrollback +
		    (size_t)cterm->backpos * cterm->backwidth;
		ciolib_vmem_gettext(cterm->x, row,
		    cterm->x + copy_w - 1, row, dst);
		if (copy_w < cterm->backwidth) {
			memset(dst + copy_w, 0,
			    (size_t)(cterm->backwidth - copy_w) *
			    sizeof(*dst));
		}
		cterm->backpos++;
		if (cterm->backpos == cterm->backlines)
			cterm->backpos = 0;
	}
}

/* popScreen(_) restores the ring counters to the snapshot taken by
 * the matching pushScreen().  The argument exists for symmetry /
 * documentation - the caller passes the row count it pushed - but
 * the actual restore is unconditional, since we have the saved
 * values.  No-op when no save is live. */
void
fn_Scrollback_popScreen(WrenVM *vm)
{
	(void)vm;
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || !st->scrollback_save_valid ||
	    cterm == NULL || cterm->scrollback == NULL)
		return;

	cterm->backstart  = st->scrollback_save_start;
	cterm->backpos    = st->scrollback_save_pos;
	cterm->backfilled = st->scrollback_save_filled;
	st->scrollback_save_valid = false;
}

