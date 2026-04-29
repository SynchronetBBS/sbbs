/* Console (log buffer accessor) + Hook registration entry points +
 * HookHandle foreign — together because Hook.on* methods produce a
 * HookHandle, and HookHandle's GC finalizer talks to the same
 * wren_host_register / wren_host_remove machinery in wren_host.c.
 *
 * Hook.onMatch goes through RE1 (regexp.h); RE1's parser calls
 * fatal() on errors, so the registration site brackets parsing with
 * a setjmp/longjmp catcher to convert fatal()s into Fiber.abort. */

#include "wren_bind_hook.h"
#include "wren_bind_internal.h"
#include "wren_host_internal.h"
#include "wren_host.h"

#include "regexp.h"

#include <inttypes.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----- Console (log buffer accessor) ------------------------------ */

void
fn_Console_count(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)wren_log_count());
}

void
fn_Console_total(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)wren_log_total());
}

/* `Console[seq]` — entry by sequence number.  Returns null when seq
 * is outside the currently-buffered range (already evicted, or not
 * yet written). */
void
fn_Console_subscript(WrenVM *vm)
{
	double d = wrenGetSlotDouble(vm, 1);
	if (d < 0) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wren_log_emit(vm, (uint64_t)d, 0);
}

void
fn_Console_clear(WrenVM *vm)
{
	(void)vm;
	wren_log_clear();
}

void
fn_Console_markSeen(WrenVM *vm)
{
	(void)vm;
	wren_host_mark_log_seen();
}

/* Wren iteration protocol over seq numbers: start at the oldest seq
 * still in the buffer, end at total-1 (inclusive).  Skipped if the
 * buffer is empty.  iteratorValue returns the entry at that seq. */
void
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

void
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

void
fn_Hook_onKey(WrenVM *vm)
{
	push_hook_handle(vm,
	    wren_host_register_hook(vm, WREN_HOOK_KEY, 1));
}
void
fn_Hook_onInput(WrenVM *vm)
{
	push_hook_handle(vm,
	    wren_host_register_hook(vm, WREN_HOOK_INPUT, 1));
}
void
fn_Hook_onMouse(WrenVM *vm)
{
	push_hook_handle(vm,
	    wren_host_register_hook(vm, WREN_HOOK_MOUSE, 1));
}
void
fn_Hook_onStatus(WrenVM *vm)
{
	push_hook_handle(vm,
	    wren_host_register_hook(vm, WREN_HOOK_STATUS, 1));
}

/* Filtered variants: signature is (filter, fn) so slot 1 is the
 * match value (key code / byte / mouse-event type) and slot 2 is the
 * callable.  Same per-event array as the unfiltered forms; dispatch
 * order = registration order. */
void
fn_Hook_onKey_filtered(WrenVM *vm)
{
	int filter = (int)wrenGetSlotDouble(vm, 1);
	push_hook_handle(vm,
	    wren_host_register_hook_filtered(vm, WREN_HOOK_KEY, 2, filter));
}
void
fn_Hook_onInput_filtered(WrenVM *vm)
{
	int filter = (int)wrenGetSlotDouble(vm, 1) & 0xff;
	push_hook_handle(vm,
	    wren_host_register_hook_filtered(vm, WREN_HOOK_INPUT, 2, filter));
}
void
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

void
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

void
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

void
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
void
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
void
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

void
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

void
fn_HookHandle_callCount(WrenVM *vm)
{
	const struct hook_metrics *m =
	    hook_handle_metrics(wrenGetSlotForeign(vm, 0));
	wrenSetSlotDouble(vm, 0, m != NULL ? (double)m->call_count : 0.0);
}

void
fn_HookHandle_totalRuntime(WrenVM *vm)
{
	const struct hook_metrics *m =
	    hook_handle_metrics(wrenGetSlotForeign(vm, 0));
	wrenSetSlotDouble(vm, 0, m != NULL ? (double)m->total_runtime_s : 0.0);
}

void
fn_HookHandle_minRuntime(WrenVM *vm)
{
	const struct hook_metrics *m =
	    hook_handle_metrics(wrenGetSlotForeign(vm, 0));
	wrenSetSlotDouble(vm, 0, m != NULL ? (double)m->min_runtime_s : 0.0);
}

void
fn_HookHandle_maxRuntime(WrenVM *vm)
{
	const struct hook_metrics *m =
	    hook_handle_metrics(wrenGetSlotForeign(vm, 0));
	wrenSetSlotDouble(vm, 0, m != NULL ? (double)m->max_runtime_s : 0.0);
}

void
fn_HookHandle_toString(WrenVM *vm)
{
	const struct hook_metrics *m =
	    hook_handle_metrics(wrenGetSlotForeign(vm, 0));
	uint64_t calls = m != NULL ? m->call_count : 0;
	char buf[48];
	snprintf(buf, sizeof(buf), "HookHandle(calls=%" PRIu64 ")", calls);
	wrenSetSlotString(vm, 0, buf);
}
