/* Foreign-method bindings for the Wren scripting host.  These wrap
 * SyncTERM's conio / connection / cterm / bbslist / cache APIs and
 * expose hook-registration entry points; lifecycle and dispatch live
 * in wren_host.c. */

#include "wren_host_internal.h"
#include "wren_host.h"
#include "wren.h"
#include "wren_bind_internal.h"
#include "regexp.h"
#include <setjmp.h>
/* REPL.compile_ uses wrenCompileSource to produce an ObjClosure that
 * the Wren-side REPL.eval invokes via .call().  That path is internal
 * to Wren - wrenCompileSource isn't exposed in wren.h - so pull in
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
#include "wren_bind_conn.h"
#include "wren_bind_hook.h"
#include "wren_bind_sftp.h"
#include "wren_bind_fs.h"
#include "wren_bind_screen.h"
#include "wren_bind_won.h"

#ifndef WITHOUT_DEUCESSH
#include "deucessh-algorithms.h"   /* dssh_ed25519_get_pub_str */
#endif

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

/* dirwrap.h provides a portable opendir/readdir/closedir + struct dirent -
 * pulls in <dirent.h> on POSIX, supplies its own implementation for MSVC. */
#include <dirwrap.h>

/* MSVC's <sys/stat.h> ships only the _S_IF* constants; older Windows SDKs
 * don't define the POSIX S_ISREG macro at all. */
#ifndef S_ISREG
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

/* term.c globals - emulation state and current bbs context. */
extern struct cterminal *cterm;

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



/* REPL.compile_(module, src, isExpression, printErrors) - compile
 * `src` at the top level of the named module and return the resulting
 * closure as a Wren value.  Calling .call() on the closure runs the
 * body in module scope, so top-level `var x = ...` becomes a module
 * variable that persists across submissions.
 *
 * isExpression=true compiles `src` as a single expression; .call()
 * returns the expression's value.  Wren-side REPL.eval uses this to
 * implement the "P" of REPL - try expression form, print the result,
 * fall back to statement form when the input isn't a single
 * expression (e.g. `var x = 5`).
 *
 * Mirrors metaCompile's pattern of dropping into a bare ObjClosure
 * because the public API has no wrapper.  wrenCompileSource is
 * foreign-method-safe (no fiber switching), and the Wren-side .call()
 * dispatch is foreign-method-safe too. */
/* REPL.captureStart_/Contains_/Clear_/Commit_ - gate around the
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

/* REPL.printTrace_(fiber) - replicates wrenDebugPrintStackTrace for a
 * caught fiber.  Wren only emits the runtime error + stack frames via
 * the error callback for *uncaught* aborts; once you Fiber.try() a
 * failed fiber, the runtime never walks them.  This helper fires both
 * a WREN_ERROR_RUNTIME with the fiber's error string and a
 * WREN_ERROR_STACK_TRACE per frame, so the caught-error path reaches
 * the same log/stderr sink as an uncaught one - the in-app console
 * can still print its own "error: …" line for the user, but
 * developers staring at stderr also see what blew up. */
static void
fn_REPL_printTrace_(WrenVM *vm)
{
	if (vm->config.errorFn == NULL)
		return;
	Value v = vm->apiStack[1];
	if (!IS_FIBER(v))
		return;
	ObjFiber *fiber = AS_FIBER(v);

	if (IS_STRING(fiber->error)) {
		vm->config.errorFn(vm, WREN_ERROR_RUNTIME, NULL, -1,
		    AS_CSTRING(fiber->error));
	} else if (!IS_NULL(fiber->error)) {
		vm->config.errorFn(vm, WREN_ERROR_RUNTIME, NULL, -1,
		    "[error object]");
	}

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
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_STRING ||
	    wrenGetSlotType(vm, 2) != WREN_TYPE_STRING) {
		wren_throw(vm,
		    "REPL.compile_: module and src must both be String");
		return;
	}
	const char *m = wrenGetSlotString(vm, 1);
	const char *s = wrenGetSlotString(vm, 2);
	bool isExpr      = wrenGetSlotBool(vm, 3);
	bool printErrors = wrenGetSlotBool(vm, 4);
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
		wren_throw(vm, "REPL.hasModule: out of memory");
		return;
	}
	memcpy(zm, m, (size_t)len);
	zm[len] = '\0';
	bool has = wrenHasModule(vm, zm);
	free(zm);
	wrenSetSlotBool(vm, 0, has);
}

/* REPL.modules - list of every module currently loaded into the VM
 * (including "core", which Wren creates for its built-in classes).
 * Walks vm->modules directly because the public C API doesn't expose
 * an iterator.  Skips empty slots and tombstones (key is UNDEFINED_VAL
 * for both); non-string keys are skipped as a defensive measure but
 * shouldn't occur - the modules map is keyed by ObjString. */
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

/* Codepage.encodes_(text) - does the *first* codepoint of `text` map
 * to a CP437 byte?  Returns false for empty input or unmappable
 * codepoints.  Used by ui_style's Glyphs class to fall back from a
 * rich Unicode primary to its ASCII fallback whenever the primary
 * isn't in the active codepage's table.  Two probes (unmapped=0 and
 * unmapped=1) sidestep the U+0000-versus-unmapped ambiguity that a
 * single probe with unmapped=0 leaves: if both probes agree, the
 * codepoint mapped successfully; if they disagree, it didn't. */
static void
fn_Codepage_encodes_(WrenVM *vm)
{
	int         len = 0;
	const char *s   = wrenGetSlotBytes(vm, 1, &len);
	uint32_t    cp  = 0;

	if (s == NULL || len <= 0 || decode_utf8_first(s, len, &cp) == 0) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	uint8_t b0 = cpchar_from_unicode_cpoint(CIOLIB_CP437, cp, 0);
	uint8_t b1 = cpchar_from_unicode_cpoint(CIOLIB_CP437, cp, 1);
	wrenSetSlotBool(vm, 0, b0 == b1);
}

/* ----- Timer -------------------------------------------------------
 *
 * One-shot fiber resumption after a delay.  The foreign captures the
 * fiber and an absolute due-time; doterm()'s sweep pushes a
 * TimerElapsed onto the result queue once xp_timer() reaches that
 * time.  Multiple pending entries per fiber are fine - each yields
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

/* ----- Format ----------------------------------------------------- */

/* Format.bytes(n) - wraps byte_estimate_to_str(n, …, 0, 1): auto-picks
 * a K/M/G/T/P suffix based on magnitude, with 1 decimal of precision.
 * Returns a String like "1.4M" or "37".  Negative inputs become 0. */
static void
fn_Format_bytes(WrenVM *vm)
{
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_NUM) {
		wren_throw(vm, "Format.bytes: argument must be a number");
		return;
	}
	double v = wrenGetSlotDouble(vm, 1);
	if (!(v >= 0.0))
		v = 0.0;
	if (v > (double)UINT64_MAX)
		v = (double)UINT64_MAX;
	char buf[32];
	byte_estimate_to_str((uint64_t)v, buf, sizeof(buf), 0, 1);
	wrenSetSlotString(vm, 0, buf);
}

/* Format.duration(seconds) - wraps duration_estimate_to_str(s, …, 0, 1):
 * auto-picks a unit (s / m / h / d / y) with 1 decimal of precision.
 * Returns a String like "8.2s" or "3.5m".  Negative inputs become 0. */
static void
fn_Format_duration(WrenVM *vm)
{
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_NUM) {
		wren_throw(vm, "Format.duration: argument must be a number");
		return;
	}
	double v = wrenGetSlotDouble(vm, 1);
	if (!(v >= 0.0))
		v = 0.0;
	char buf[32];
	duration_estimate_to_str(v, buf, sizeof(buf), 0, 1);
	wrenSetSlotString(vm, 0, buf);
}

/* Timer.now - monotonic seconds since some unspecified epoch.  Wraps
 * xp_timer(); the absolute value is meaningless, but differences between
 * two readings are reliable.  Used for throughput/ETA calculations in
 * the SFTP queue and anywhere else a script wants to time something. */
static void
fn_Timer_now(WrenVM *vm)
{
	wrenEnsureSlots(vm, 1);
	wrenSetSlotDouble(vm, 0, (double)xp_timer());
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

/* ----- Status-bar transfer arrows -------------------------------- */

/* ----- Host.sshPublicKey ----------------------------------------- */

/* Host.sshPublicKey - first locally-configured SSH public key, as a
 * Map { "algo": String, "blob": String }, or null when no key is
 * available (no SSH backend, key not loaded, OOM).  Used by Wren
 * scripts (sftp_pubkey.wren) to upload the user's authorized_keys
 * entry on connect.  The structured shape (Map rather than raw
 * OpenSSH line) future-proofs for RSA / sntrup keys: when those
 * become reachable here, this getter can pick whichever algo's
 * key is configured without the consumer needing to parse a
 * line. */
static void
fn_Host_sshPublicKey(WrenVM *vm)
{
#ifdef WITHOUT_DEUCESSH
	wrenSetSlotNull(vm, 0);
#else
	int64_t sz = dssh_ed25519_get_pub_str(NULL, 0);
	if (sz <= 0) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	char *line = malloc((size_t)sz + 1);
	if (line == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	if (dssh_ed25519_get_pub_str(line, (size_t)sz + 1) <= 0) {
		free(line);
		wrenSetSlotNull(vm, 0);
		return;
	}
	/* DeuceSSH emits "<algo> <base64-blob>" with no trailing comment.
	 * Split on the first space to separate algo and blob; if a
	 * future variant adds a comment, drop everything past the second
	 * space (consumers want the bare blob for matching). */
	char *space = strchr(line, ' ');
	if (space == NULL) {
		free(line);
		wrenSetSlotNull(vm, 0);
		return;
	}
	*space      = '\0';
	char *blob  = space + 1;
	char *trail = strchr(blob, ' ');
	if (trail != NULL)
		*trail = '\0';
	wrenEnsureSlots(vm, 3);
	wrenSetSlotNewMap(vm, 0);
	wrenSetSlotString(vm, 1, "algo");
	wrenSetSlotString(vm, 2, line);
	wrenSetMapValue(vm, 0, 1, 2);
	wrenSetSlotString(vm, 1, "blob");
	wrenSetSlotString(vm, 2, blob);
	wrenSetMapValue(vm, 0, 1, 2);
	free(line);
#endif
}

/* ----- Platform --------------------------------------------------- */

/* Platform.name - OS identifier for platform-specific script logic.
 * Returns "Windows" on Windows, the uname(2) sysname on POSIX, and
 * "Unknown" on anything else (DOS, OS/2, …).  Win32 is checked
 * before _POSIX_VERSION because POSIX layers like Cygwin / MSYS
 * define both - we want the native-OS answer there.  Called
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
	/* Screen - absolute / global operations */
	{ "Screen", true,  "size",                  fn_Screen_size            },
	{ "Screen", true,  "save()",                fn_Screen_save            },
	{ "Screen", true,  "restore(_)",            fn_Screen_restore         },
	{ "Screen", true,  "readRect(_,_,_,_)",     fn_Screen_readRect        },
	{ "Screen", true,  "writeRect(_,_,_,_,_)",  fn_Screen_writeRect       },
	{ "Screen", true,  "putRect(_,_,_)",        fn_Screen_putRect_3       },
	{ "Screen", true,  "putRect_(_,_,_,_,_,_,_)", fn_Screen_putRect_7     },
	{ "Screen", true,  "moveRect(_,_,_,_,_,_)", fn_Screen_moveRect        },
	{ "Screen", true,  "attr=(_)",              fn_Screen_attr_set        },
	{ "Screen", true,  "hyperlinkId",           fn_Screen_hyperlinkId     },
	{ "Screen", true,  "hyperlinkId=(_)",       fn_Screen_hyperlinkId_set },

	/* ScreenWindow - operations scoped to the active text window */
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

	/* ScreenFonts - Screen.font[i] / Screen.font[i] = n */
	{ "ScreenFonts", true, "[_]",     fnScreenFonts_subscript     },
	{ "ScreenFonts", true, "[_]=(_)", fnScreenFonts_subscript_set },

	/* Palette - Screen.palette[i] / .mode */
	{ "Palette", true, "[_]",       fnPalette_subscript     },
	{ "Palette", true, "[_]=(_)",   fnPalette_subscript_set },
	{ "Palette", true, "mode",      fnPalette_mode          },
	{ "Palette", true, "mode=(_)",  fnPalette_mode_set      },

	/* CustomCursor - static (live) and instance (snapshot) accessors */
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

	/* VideoFlags - static (live) and instance (snapshot) */
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

	/* Color - static utilities */
	{ "Color", true, "fromRgb(_,_,_)",     fnColor_fromRgb       },
	{ "Color", true, "fromAttr(_)",        fnColor_fromAttr      },
	{ "Color", true, "toLegacyAttr(_,_)",  fnColor_toLegacyAttr  },

	/* ScreenSupports - Screen.supports.<flag> capability getters */
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
	{ "Input",  true,  "pushClaim_(_,_)",     fn_Input_pushClaim_         },
	{ "Wake",   true,  "post(_,_)",           fn_Wake_post                },
	{ "ClaimHandle", false, "pop()",          fn_ClaimHandle_pop          },
	{ "Input",  true,  "ungetKey_(_)",        fn_Input_ungetKey_          },
	{ "Input",  true,  "ungetMouse_(_)",      fn_Input_ungetMouse_        },
	{ "Input",  true,  "mousedrag()",         fn_Input_mousedrag          },
	{ "Input",  true,  "mousedrag(_)",        fn_Input_mousedrag          },
	{ "Input",  true,  "mouseVisible=(_)",    fn_Input_mouseVisible_set   },
	{ "Input",  true,  "mouseEvents",         fn_Input_mouseEvents        },
	{ "Input",  true,  "mouseEvents=(_)",     fn_Input_mouseEvents_set    },
	{ "Input",  true,  "enableMouseEvent(_)", fn_Input_enableMouseEvent   },
	{ "Input",  true,  "disableMouseEvent(_)",fn_Input_disableMouseEvent  },
	{ "Input",  true,  "setupMouseEvents()", fn_Input_setupMouseEvents   },

	/* KeyEvent (instance) */
	{ "KeyEvent",   false, "code",       fn_KeyEvent_code      },
	{ "KeyEvent",   false, "codepoint",  fn_KeyEvent_codepoint },
	{ "KeyEvent",   false, "text",       fn_KeyEvent_text      },
	{ "KeyEvent",   false, "toString",   fn_KeyEvent_toString  },

	/* MouseEvent (instance) */
	{ "MouseEvent", false, "event",      fn_MouseEvent_event      },
	{ "MouseEvent", false, "bstate",     fn_MouseEvent_bstate     },
	{ "MouseEvent", false, "modifiers",  fn_MouseEvent_modifiers  },
	{ "MouseEvent", false, "startX",     fn_MouseEvent_startX     },
	{ "MouseEvent", false, "startY",     fn_MouseEvent_startY     },
	{ "MouseEvent", false, "endX",       fn_MouseEvent_endX       },
	{ "MouseEvent", false, "endY",       fn_MouseEvent_endY       },
	{ "MouseEvent", false, "toString",   fn_MouseEvent_toString   },

	/* Clipboard (all static) */
	{ "Clipboard", true, "text",               fn_Clipboard_text     },
	{ "Clipboard", true, "text=(_)",           fn_Clipboard_text_set },

	/* Codepage (all static) */
	{ "Codepage",  true, "encodes_(_)",        fn_Codepage_encodes_  },

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
	{ "Cell",  false, "eqContent(_)",    fn_Cell_eqContent       },
	{ "Cell",  false, "toString",        fn_Cell_toString        },

	/* Surface (all instance) - w×h grid of vmem_cells.  Inherits
	 * Sequence on the Wren side; the count / [_] / iterate /
	 * iteratorValue methods walk the linear cell buffer.  cellAt /
	 * putRect / fill add 2D operations.  The Wren-side Surface class
	 * declares `putRect(_,_,_,_)` and `fill(_,_)` wrappers that unpack
	 * a Rect before calling the underscore-suffixed primitives. */
	{ "Surface", false, "count",                       fn_Surface_count         },
	{ "Surface", false, "[_]",                         fn_Surface_subscript     },
	{ "Surface", false, "iterate(_)",                  fn_Surface_iterate       },
	{ "Surface", false, "iteratorValue(_)",            fn_Surface_iteratorValue },
	{ "Surface", false, "width",                       fn_Surface_width         },
	{ "Surface", false, "height",                      fn_Surface_height        },
	{ "Surface", false, "cellAt(_,_)",                 fn_Surface_cellAt        },
	{ "Surface", false, "urlAt(_,_)",                  fn_Surface_urlAt         },
	{ "Surface", false, "putRect(_,_,_)",              fn_Surface_putRect_3     },
	{ "Surface", false, "putRect_(_,_,_,_,_,_,_)",     fn_Surface_putRect_7     },
	{ "Surface", false, "fill_(_,_,_,_,_)",            fn_Surface_fill_         },
	{ "Surface", false, "toString",                    fn_Surface_toString      },

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

	/* Scrollback (all static) - uniform linearize-and-dispatch
	 * wrappers over the Surface read-side contract, plus the two
	 * genuinely new verbs (pushScreen / popScreen). */
	{ "Scrollback", true, "width",                       fn_Scrollback_width         },
	{ "Scrollback", true, "height",                      fn_Scrollback_height        },
	{ "Scrollback", true, "count",                       fn_Scrollback_count         },
	{ "Scrollback", true, "[_]",                         fn_Scrollback_subscript     },
	{ "Scrollback", true, "iterate(_)",                  fn_Scrollback_iterate       },
	{ "Scrollback", true, "iteratorValue(_)",            fn_Scrollback_iteratorValue },
	{ "Scrollback", true, "cellAt(_,_)",                 fn_Scrollback_cellAt        },
	{ "Scrollback", true, "urlAt(_,_)",                  fn_Scrollback_urlAt         },
	{ "Scrollback", true, "putRect(_,_,_)",              fn_Scrollback_putRect_3     },
	{ "Scrollback", true, "putRect_(_,_,_,_,_,_,_)",     fn_Scrollback_putRect_7     },
	{ "Scrollback", true, "fill_(_,_,_,_,_)",            fn_Scrollback_fill_         },
	{ "Scrollback", true, "toString",                    fn_Scrollback_toString      },
	{ "Scrollback", true, "pushScreen()",                fn_Scrollback_pushScreen    },
	{ "Scrollback", true, "popScreen(_)",                fn_Scrollback_popScreen     },

	{ "Host", true, "cacheDirectory",       fn_Host_cacheDirectory    },
	{ "Host", true, "downloadDir",          fn_Host_downloadDir       },
	{ "Host", true, "uploadPath",           fn_Host_uploadPath        },
	{ "Host", true, "pickFile(_,_,_)",      fn_Host_pickFile          },
	{ "Host", true, "pickFiles(_,_,_)",     fn_Host_pickFiles         },
	{ "Host", true, "pickSavePath(_,_)",    fn_Host_pickSavePath      },
	{ "Host", true, "openLocalFile(_)",     fn_Host_openLocalFile     },

	/* Platform - OS identification. */
	{ "Platform", true, "name",             fn_Platform_name          },

	/* Timer - one-shot fiber resumption after a delay. */
	{ "Timer", true, "trigger(_,_)",        fn_Timer_trigger          },
	{ "Timer", true, "now",                 fn_Timer_now              },

	/* Format - number-to-string helpers. */
	{ "Format", true, "bytes(_)",           fn_Format_bytes           },
	{ "Format", true, "duration(_)",        fn_Format_duration        },

	/* SFTP (all static) */
	{ "SFTP",       true,  "available",       fn_SFTP_available       },
	{ "SFTP",       true,  "pubdir",          fn_SFTP_pubdir          },
	{ "SFTP",       true,  "lname",           fn_SFTP_lname           },
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
	{ "SFTP",       true,  "setMtime(_,_,_)", fn_SFTP_setMtime        },
	{ "SFTP",       true,  "descs(_,_)",      fn_SFTP_descs           },

	/* SFTPEntry (instance) */
	{ "SFTPEntry",  false, "name",            fn_SFTPEntry_name       },
	{ "SFTPEntry",  false, "longname",        fn_SFTPEntry_longname   },
	{ "SFTPEntry",  false, "size",            fn_SFTPEntry_size       },
	{ "SFTPEntry",  false, "mtime",           fn_SFTPEntry_mtime      },
	{ "SFTPEntry",  false, "isDir",           fn_SFTPEntry_isDir      },
	{ "SFTPEntry",  false, "hasLongDesc",     fn_SFTPEntry_hasLongDesc },
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
	{ "Conn",  true, "endSession(_)",  fn_Conn_endSession   },
	{ "Conn",  true, "paste()",        fn_Conn_paste        },
	{ "Conn",  true, "scrollback()",   fn_Conn_scrollback   },
	{ "Conn",  true, "upload()",       fn_Conn_upload       },
	{ "Conn",  true, "download()",     fn_Conn_download     },
	{ "Conn",  true, "connected",      fn_Conn_connected    },
	{ "Conn",  true, "type",           fn_Conn_type         },
	{ "Conn",  true, "pending",        fn_Conn_pending      },
	{ "Conn",  true, "queued",         fn_Conn_queued       },
	{ "Conn",  true, "peek(_)",        fn_Conn_peek         },
	{ "Conn",  true, "recv(_)",        fn_Conn_recv         },

	/* ConnError (instance) */
	{ "ConnError", false, "code",       fn_ConnError_code      },
	{ "ConnError", false, "bytesSent",  fn_ConnError_bytesSent },
	{ "ConnError", false, "message",    fn_ConnError_message   },
	{ "ConnError", false, "toString",   fn_ConnError_toString  },

	/* CTerm */
	{ "CTerm", true, "emulation",          fn_CTerm_emulation       },
	{ "CTerm", true, "x",                  fn_CTerm_x               },
	{ "CTerm", true, "y",                  fn_CTerm_y               },
	{ "CTerm", true, "originX",            fn_CTerm_originX         },
	{ "CTerm", true, "originY",            fn_CTerm_originY         },
	{ "CTerm", true, "attr",               fn_CTerm_attr            },
	{ "CTerm", true, "doorwayMode",        fn_CTerm_doorwayMode     },
	{ "CTerm", true, "doorwayMode=(_)",    fn_CTerm_doorwayMode_set },
	{ "CTerm", true, "music",              fn_CTerm_music           },
	{ "CTerm", true, "music=(_)",          fn_CTerm_music_set       },
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
	{ "CTerm", true, "saveScreenshot(_,_)", fn_CTerm_saveScreenshot },

	/* Capture (streaming-log control) - replaces the old
	 * CTerm.logMode / CTerm.logPaused getters. */
	{ "Capture", true, "active",           fn_Capture_active       },
	{ "Capture", true, "paused",           fn_Capture_paused       },
	{ "Capture", true, "start(_,_)",       fn_Capture_start        },
	{ "Capture", true, "stop()",           fn_Capture_stop         },
	{ "Capture", true, "pause()",          fn_Capture_pause        },
	{ "Capture", true, "resume()",         fn_Capture_resume       },
	{ "CTerm", true, "atasciiInverse",     fn_CTerm_atasciiInverse  },
	{ "CTerm", true, "ooiiMode",           fn_CTerm_ooiiMode        },
	{ "CTerm", true, "ooiiMode=(_)",       fn_CTerm_ooiiMode_set    },
	{ "CTerm", true, "mouseMode",          fn_CTerm_mouseMode       },
	{ "CTerm", true, "mouseDisabled",      fn_CTerm_mouseDisabled   },
	{ "CTerm", true, "mouseDisabled=(_)",  fn_CTerm_mouseDisabled_set },
	{ "CTerm", true, "throttleSpeed",      fn_CTerm_throttleSpeed     },
	{ "CTerm", true, "throttleSpeed=(_)",  fn_CTerm_throttleSpeed_set },
	{ "CTerm", true, "throttleSpeedUp()",  fn_CTerm_throttleSpeedUp   },
	{ "CTerm", true, "throttleSpeedDown()",fn_CTerm_throttleSpeedDown },
	{ "CTerm", true, "statusDisplay",      fn_CTerm_statusDisplay   },
	{ "CTerm", true, "refreshStatus()",    fn_CTerm_refreshStatus   },
	{ "CTerm", true, "sftpActive",         fn_CTerm_sftpActive_get  },
	{ "CTerm", true, "sftpActive=(_)",     fn_CTerm_sftpActive_set  },
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
	{ "BBS",   true, "elapsedSeconds",          fn_BBS_elapsedSeconds        },
	{ "BBS",   true, "connTypeName",            fn_BBS_connTypeName          },
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
	{ "File",      false, "mtime",           fn_File_mtime          },
	{ "File",      false, "mtime=(_)",       fn_File_mtime_set      },
	{ "File",      false, "isOpen",          fn_File_isOpen         },
	{ "File",      false, "sha1",            fn_File_sha1           },
	{ "File",      false, "md5",             fn_File_md5            },
	{ "File",      false, "token",           fn_File_token          },
	{ "File",      false, "toString",        fn_File_toString       },

	/* FileError (instance) */
	{ "FileError", false, "code",            fn_FileError_code      },
	{ "FileError", false, "errno",           fn_FileError_errno     },
	{ "FileError", false, "message",         fn_FileError_message   },
	{ "FileError", false, "toString",        fn_FileError_toString  },

	/* Hook */
	{ "Hook",  true, "onKey(_)",       fn_Hook_onKey            },
	{ "Hook",  true, "onKey(_,_)",     fn_Hook_onKey_filtered   },
	{ "Hook",  true, "onInput(_)",     fn_Hook_onInput          },
	{ "Hook",  true, "onInput(_,_)",   fn_Hook_onInput_filtered },
	{ "Hook",  true, "onMatch(_,_)",      fn_Hook_onMatch       },
	{ "Hook",  true, "onMatchClean(_,_)", fn_Hook_onMatchClean  },
	{ "Hook",  true, "onMouse(_)",     fn_Hook_onMouse          },
	{ "Hook",  true, "onMouse(_,_)",   fn_Hook_onMouse_filtered },
	{ "Hook",  true, "onShellClose(_)", fn_Hook_onShellClose    },
	{ "Hook",  true, "onDisconnect(_)", fn_Hook_onDisconnect    },
	{ "Hook",  true, "every(_,_)",     fn_Hook_every        },

	{ "Host", true, "sshPublicKey",      fn_Host_sshPublicKey      },
	{ "Host", true, "safeMode",          fn_Host_safeMode          },
	{ "Host", true, "textTerminal",      fn_Host_textTerminal      },
	{ "Host", true, "altKeyName",        fn_Host_altKeyName        },
	{ "Host", true, "altKeyShort",       fn_Host_altKeyShort       },
	{ "Host", true, "print(_)",          fn_Host_print             },
	{ "Host", true, "launchScript",      fn_Host_launchScript      },
	{ "Host", true, "haveOOII",          fn_Host_haveOOII          },
	{ "Host", true, "maxOOIIMode",       fn_Host_maxOOIIMode       },
	{ "Host", true, "outputRates",       fn_Host_outputRates       },
	{ "Host", true, "outputRateNames",   fn_Host_outputRateNames   },
	{ "Host", true, "logLevel",          fn_Host_logLevel          },
	{ "Host", true, "logLevel=(_)",      fn_Host_logLevel_set      },
	{ "Host", true, "logLevelNames",     fn_Host_logLevelNames     },
	{ "Host", true, "fontControl()",     fn_Host_fontControl       },
	{ "Host", true, "editBBSList()",     fn_Host_editBBSList       },
	{ "Host", true, "logUnread",         fn_Host_logUnread         },
	{ "Host", true, "logUnreadError",    fn_Host_logUnreadError    },
	{ "Host", true, "musicNames",        fn_Host_musicNames        },
	{ "Host", true, "musicHelp",         fn_Host_musicHelp         },

	{ "Status", true, "callable",        fn_Status_callable_get    },
	{ "Status", true, "callable=(_)",    fn_Status_callable_set    },
	{ "Status", true, "enabled",         fn_Status_enabled         },

	{ "WON",   true, "deserialize(_)", fn_WON_deserialize   },

	/* WONError (instance) */
	{ "WONError", false, "code",     fn_WONError_code     },
	{ "WONError", false, "offset",   fn_WONError_offset   },
	{ "WONError", false, "message",  fn_WONError_message  },
	{ "WONError", false, "toString", fn_WONError_toString },
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
	if (strcmp(className, "Surface") == 0) {
		WrenForeignClassMethods m = {
			wren_surface_allocate, wren_surface_finalize
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
	if (strcmp(className, "ClaimHandle") == 0) {
		WrenForeignClassMethods m = {
			wren_claim_handle_allocate, wren_claim_handle_finalize
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
	if (strcmp(className, "FileError") == 0) {
		WrenForeignClassMethods m = {
			wren_file_error_allocate, wren_file_error_finalize
		};
		return m;
	}
	if (strcmp(className, "WONError") == 0) {
		WrenForeignClassMethods m = {
			wren_won_error_allocate, wren_won_error_finalize
		};
		return m;
	}
	if (strcmp(className, "ConnError") == 0) {
		WrenForeignClassMethods m = {
			wren_conn_error_allocate, wren_conn_error_finalize
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
