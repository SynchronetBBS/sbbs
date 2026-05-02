/* Screen / Input / Cell / Cells / Font / Hyperlinks / Color / Palette
 * / CustomCursor / VideoFlags / KeyEvent / MouseEvent foreign surface
 * for the Wren scripting host.  Public API in wren_bind_screen.h;
 * BINDINGS table + lookup_class dispatch live in wren_bind.c. */

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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----- Screen / Input -------------------------------------------- */

/* Screen.supports.* — backend capability flags.  Each getter is one
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

/* Window bounds [sx, sy, ex, ey] — 1-based, inclusive.  Setter
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

/* Cursor position as a single [x, y] list — getter and setter
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

/* Forward decl — defined later beside the Cell helpers. */
static int encode_utf8(uint32_t cp, char out[4]);

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

void
wren_key_event_allocate(WrenVM *vm)
{
	struct wren_key_event *ke =
	    wrenSetSlotNewForeign(vm, 0, 0, sizeof(*ke));
	uint16_t code = (uint16_t)wrenGetSlotDouble(vm, 1);
	key_event_init(ke, code);
}

void
wren_key_event_finalize(void *data)
{
	(void)data;
}

void
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

/* Wake.post(fiber, value) — queue `value` to be delivered to `fiber`
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
 * present and its id matches, unlink/release.  Idempotent — stale
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

/* Input.pushClaim_(fiber, fn) — register a claim; returns ClaimHandle.
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
		 * from this push — release the duplicate. */
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

/* ClaimHandle.pop() — unlink the claim entry by id match.  Idempotent
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

/* Walk the claim stack with the given event in the prepared slot.
 * Auto-prunes dead-fiber claims as it goes.  Returns true on first
 * consumed (Bool true) return; false if all claims passed through
 * or were pruned. */
static bool
dispatch_claim_event(int event_slot)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->vm == NULL || st->dispatch1_handle == NULL ||
	    st->hook_class == NULL)
		return false;

	WrenHandle *event_handle = wrenGetSlotHandle(st->vm, event_slot);
	if (event_handle == NULL)
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
		 * — handled by walking from the new head next call,
		 * not this dispatch), or had the entry mutated by a
		 * same-fiber re-push (id bumped, fn replaced — we'd
		 * fire the new fn next event).  Just walk forward
		 * from where we are. */
		if (*cur == c)
			cur = &c->next;
	}
	wrenReleaseHandle(st->vm, event_handle);
	return consumed;
}

bool
wren_bind_dispatch_claim_key(int code)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->claim_top == NULL || code == CIO_KEY_MOUSE)
		return false;
	wrenEnsureSlots(st->vm, 4);
	push_key_event(st->vm, (uint16_t)code);
	/* push_key_event puts the event in slot 0; move to slot 3 to
	 * keep the dispatch-table slots free. */
	wrenSetSlotHandle(st->vm, 3, wrenGetSlotHandle(st->vm, 0));
	return dispatch_claim_event(3);
}

bool
wren_bind_dispatch_claim_mouse(struct mouse_event *ev)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->claim_top == NULL || ev == NULL)
		return false;
	wrenEnsureSlots(st->vm, 4);
	push_mouse_event(st->vm, ev);
	wrenSetSlotHandle(st->vm, 3, wrenGetSlotHandle(st->vm, 0));
	return dispatch_claim_event(3);
}

/* Free every claim and clear the stack — called from
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

/* Input.next() — block until something is ready, return the event. */
void
fn_Input_next(WrenVM *vm)
{
	push_next_event(vm);
}

/* Input.next(ms) — wait up to ms milliseconds; null on timeout. */
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

/* Input.poll() — non-blocking; null when nothing is ready. */
void
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
 * Round-trip the pointer through a Wren Num — pointers are typically
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

struct wren_surface {
	enum syncterm_wren_foreign type;     /* SWF_SURFACE */
	int               count;             /* = width * height */
	struct vmem_cell *buf;               /* malloc'd; freed at finalize */
	int               width;
	int               height;
};

/* Forward — definition lives next to the rest of the rect helpers. */
static bool slot_to_surface_(WrenVM *vm, int slot,
    struct vmem_cell **buf_out, int *w_out, int *h_out);

struct wren_cell {
	enum syncterm_wren_foreign type;
	struct vmem_cell   c;             /* used when standalone */
	int                index;         /* index into parent_buf in view mode */
	struct vmem_cell  *parent_buf;    /* parent Surface's malloc'd buf (stable
	                                     while parent_handle is held — the
	                                     handle pins the Surface, the buf is
	                                     malloc'd, never reallocated, freed
	                                     only at the Surface's finalize) */
	WrenHandle        *parent_handle; /* pin on parent Surface; NULL = standalone */
};

/* Standalone cells return their own embedded `c`; view-mode cells use
 * their captured malloc'd buffer.  Both pointers are stable across
 * arbitrary VM calls — `parent_buf` is a malloc'd region the parent
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
 * Cell.new() in Wren land — fill a standalone cell with the current
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
	sf->type   = SWF_SURFACE;
	sf->width  = w;
	sf->height = h;
	sf->count  = w * h;
	sf->buf    = NULL;
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

/* ----- Screen.hyperlinkId — singleton current-hyperlink ID for new
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

/* ----- Input.mouseVisible — setter only.  ciolib has no get
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

/* ----- Input.mouseEvents getter / setter — opaque uint64 bitmask of
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

/* ----- Font.codepage / Font.codepageOf(i) — codepage of a font slot. */
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

/* Screen.font[i] — slot in 0..4 (5 active font slots in conio). */
void
fnScreenFonts_subscript(WrenVM *vm)
{
	int i = (int)wrenGetSlotDouble(vm, 1);
	if (i < 0 || i > 4) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotDouble(vm, 0, (double)getfont(i));
}

void
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

void
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

void
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
 * fixed at video-mode init.  Getter only — no setter. */
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
	 * mask it off — Cell.fgRgb=/bgRgb= add the bit when writing.
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

/* Screen.readRect returns a Surface — the rect's width/height come
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
	sf->type   = SWF_SURFACE;
	sf->width  = w;
	sf->height = h;
	sf->count  = (int)n;
	sf->buf    = buf;
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

	/* Surface: contiguous vmem_cell buffer — hand straight to puttext
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
	/* Capture sf->buf before the slot-0 overwrite — it's a malloc'd
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
	int i = (int)wrenGetSlotDouble(vm, 1);
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
 * the rect-blit primitives.  Returns false on type mismatch. */
static bool
slot_to_surface_(WrenVM *vm, int slot,
    struct vmem_cell **buf_out, int *w_out, int *h_out)
{
	if (slot_foreign_type(vm, slot) != SWF_SURFACE)
		return false;
	struct wren_surface *sf = wrenGetSlotForeign(vm, slot);
	*buf_out = sf->buf;
	*w_out   = sf->width;
	*h_out   = sf->height;
	return true;
}

/* `surface.putRect(src, dstX, dstY)` — paste the entirety of `src`
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

/* `surface.putRect(src, srcX, srcY, srcW, srcH, dstX, dstY)` — paste a
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

/* `surface.fill_(x, y, w, h, cell)` — bulk-fill a rect with a single
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

/* `surface.toString` — diagnostic. */
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

