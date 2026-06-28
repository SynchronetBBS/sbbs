# Wren API Design Review

Critical review of the Wren scripting API surface (Wren.adoc + scripts/syncterm.wren
+ wren_bind*.c). Captured during alpha so the structural decisions can still
shift without backward-compat hacks.

## Deferred: Wren main menu VM

The eventual Wren main-menu implementation should keep the connected and
unconnected contexts as separate VMs/capability sets.  The menu VM has no wire
control by default; connected-only terminal/session APIs stay gated away from
it.

Menu code should deal in real `BBS` instances backed by `struct bbslist`, not a
parallel dialing-entry type.  Existing connected `BBS.*` static getters can
remain as a compatibility facade over the active/current BBS record while newer
code moves toward the same instance model the menu will use.

Script autoload can use `scripts/auto/menu/*.wren`; the embed/build list still
needs to include that path when the menu VM is added.

## A. Documentation drift (fix before any structural changes) — DONE

These are factual mismatches between `Wren.adoc` and what's actually exported.
Fix these first so the rest of the review is grounded in reality.

1. **`Cells` class doesn't exist.** Wren.adoc:1483-1487, 2088-2091 describe
   `Screen.readRect` as returning a `Cells` immutable foreign list. The actual
   class is `Surface is Sequence` (`syncterm.wren:537`), with a much richer
   surface (`width`, `height`, `cellAt`, `putRect`, `fill`). This is a *major*
   doc gap — the entire 2D / blit story is hidden.
   **Fixed:** `Cells` references replaced with `Surface`; new Surface section
   documents the full API (width/height/count/[i]/cellAt/putRect/fill,
   row-major linear order, Sequence inheritance).
2. **`Screen.writeRect(sx,sy,ex,ey, cells)`** — doc says the 5th arg is "a list
   of `Cell` objects"; actual binding takes a `Surface` (`syncterm.wren:46`).
   **Fixed:** documented as accepting either a Surface (zero-copy) or a List
   of Cell, with size/type mismatch behavior; `Screen.putRect` also added to
   the table (was undocumented).
3. **`MouseEvent` constructor.** Doc names args `sx, sy, ex, ey`; code names
   them `startX, startY, endX, endY` (`syncterm.wren:235`).
   **Fixed:** doc updated to match the foreign declaration (`startX, startY,
   endX, endY`).
4. **`VideoFlags.expand`** ~~documented as "Read-only"; C bindings register
   both getter AND setter.~~ **False alarm.** Verified: only the getter
   handlers (`fnVideoFlags_expand_static` at `wren_bind.c:733` and
   `fnVideoFlags_expand` at `:746`) are in the dispatch table; the
   `*_set*` declarations in `wren_bind_screen.h` lines 178/180 are dead
   forward-decls with no callers.  Doc is correct as written.  Side
   cleanup TODO: drop the dead header declarations.
5. **`Console.markSeen()`** is wired in C and declared in `syncterm.wren:591`
   but missing from Wren.adoc entirely.
   **Fixed:** documented in the Console table — it snapshots the current
   `total` / error counters as the "seen" baseline so the status-bar log
   indicator clears until the next entry.
6. **`Hook.onMouse` callback shape.** Doc says `ev` is a 7-element list
   `[event, bstate, modifiers, sx, sy, ex, ey]` (Wren.adoc:967). Verified
   against `wren_host.c:1326-1364` — the doc is accurate. **No fix in
   Section A**; the inconsistency between this list shape and the
   `MouseEvent` foreign returned everywhere else is a structural concern
   tracked in B.1.

## B. Inconsistent return / argument shapes for similar operations

These are the genuinely structural concerns; most of them get harder to change
once people start writing scripts against them.

1. **`MouseEvent` vs `[event, bstate, modifiers, sx, sy, ex, ey]` list.** Two
   encodings of the same data, used in different paths:
   - `Input.next()` / `Input.poll()` / `Input.pushClaim` callback → `MouseEvent`
     foreign object.
   - `Hook.onMouse { |ev| ... }` → 7-element List.

   This forces script authors to remember which surface uses which. The list
   form has a `bstate` field the foreign doesn't expose at all.
   **Done.** `bstate` added as a `MouseEvent` field; `wren_host_dispatch_mouse`
   now pushes a `MouseEvent` foreign in slot 2 instead of building a
   7-element list, so `Hook.onMouse` callbacks receive the same shape as
   every other mouse surface.  Drive-by: refactored `push_key_event` /
   `push_mouse_event` to take a destination slot, eliminating the
   `wrenSetSlotHandle(..., wrenGetSlotHandle(...))` re-slot dance in the
   claim dispatchers (which was leaking a `WrenHandle` per claim-routed
   key/mouse event).  `Hook.onMouse(event, fn)` filtered docs updated to
   compare `ev.event == event` instead of `ev[0] == event`.

   **Follow-up 1 — `MouseEvent.new` overloads.** Five constructor
   overloads added, dispatched in C by `wrenGetSlotCount`:
   - `new(event, startX, startY)` — point event, derived bstate, modifiers=0
   - `new(event, startX, startY, modifiers)`
   - `new(event, startX, startY, endX, endY)`
   - `new(event, startX, startY, endX, endY, modifiers)`
   - `new(event, startX, startY, endX, endY, modifiers, bstate)` — canonical
     fully-explicit form

   Defaults when an arg is omitted: `endX`/`endY` → copies of `startX`/`startY`;
   `modifiers` → 0; `bstate` → derived from `event` (bit for the button
   the event is about per `CIOLIB_BUTTON(n)`, or 0 for `Mouse.move`).
   `wrentest.wren` gained `testMouseEventCtors_` covering all five
   arities and the bstate-derivation paths.  Twenty-six call sites in
   the UI test files were rewritten from the old 6-arg
   `(event, modifiers, sx, sy, ex, ey)` shape to the new 5-arg
   `(event, sx, sy, ex, ey)` shape (modifiers=0 in every case);
   `ui_widget_test.wren`'s `mouse_(x, y)` helper became
   `MouseEvent.new(0, x, y)` (3-arg form).

   **Follow-up 2 — proper claim-dispatch slot-lifetime fix.** The
   drive-by "push event into slot 3 instead of slot 0" change made the
   `wren_bind_dispatch_claim_*` paths *look* clean but still violated
   wren.md §6: it left the event in a slot across `fiber_is_done` (a
   `wrenCall("isDone")` with maxSlots=1) and the inner
   `wrenCall("dispatch_(_,_)")` (maxSlots=3), both of which rebase the
   api stack.  Refactored `dispatch_claim_event` to take a pinned
   `WrenHandle *event_handle` C argument instead of a slot index; the
   caller pushes into slot 0, pins via `wrenGetSlotHandle`, hands the
   handle off, and releases after.  The dispatcher loop pushes the
   handle back into slot 2 fresh each iteration via
   `wrenSetSlotHandle`.  No more slot-as-scratch dependency between
   helper and caller.  This was the actual fix for the original
   wren.md §6 violation, not just a cosmetic refactor.

   **Follow-up 3 — `KeyEvent.new` encoding-constraint validation.**
   T09 (`wrentest`'s "older-fiber claim wins after newer pops") was
   actually failing because of an unrelated test bug:
   `KeyEvent.new(0xFB01)` doesn't survive a roundtrip through ciolib's
   ungetch. ciolib byte-stuffs 16-bit codes as low-byte-then-high-byte
   with the convention that low byte == 0x00 or 0xE0 marks "extended
   scancode follows."  Codes with high byte non-zero AND low byte not
   0x00 / 0xE0 split into two unrelated `getch` reads (the test author
   slipped from the `0xFx00` sentinel pattern into `0xFB01`).  Fixed
   the immediate test (`0xFB01` → `0xF900`); also added validation in
   `wren_key_event_allocate` that aborts the calling fiber for
   non-representable codes.  C-side `key_event_init` (called from real
   getch via `push_key_event`) is unchanged — the constraint applies
   only at user-facing construction.  New `testKeyEventCtorValidation_`
   in wrentest covers four valid forms and two rejection cases.
   `Wren.adoc` gained a `==== Encoding constraint [[keyevent-encoding]]`
   subsection laying out the three valid encoding shapes.

   Final state: 435/435 wrentest passing, 316/316 cterm_test passing.

2. **Hook return-value contracts vary across five hook types.** This is the
   messiest table in the API:

   | Hook | Callback returns | Meaning of each value |
   |---|---|---|
   | `onKey` | Bool | true=consume |
   | `onMouse` | Bool | true=consume |
   | `onInput` | Bool / String / other | true=drop, String=replace, other=passthrough |
   | `onStatus` | String / other | String=replace, other=passthrough |
   | `onMatch`, `onMatchClean` | **IGNORED** | passthrough only |
   | `every`, `onShellClose`, `onDisconnect` | ignored | no-arg |

   **Done.** Reduced to two contracts (no behaviour change — the
   dispatchers already implemented these; the API just wasn't
   *documented* coherently):

   - **C1 (replaceable):** `true` = consume, `String` = replace as
     bytes where meaningful (onInput byte replacement, onStatus
     text replacement), other = passthrough.  onKey / onMouse use
     C1 but have no meaningful interpretation for `String` (a
     16-bit key code or a `MouseEvent` isn't a byte stream), so a
     String return from those silently falls through to passthrough.
     Use `onInput` if you need byte-level replacement.
   - **C2 (observe-only):** return value ignored.  Used by `onMatch`,
     `onMatchClean`, `every`, `onShellClose`, `onDisconnect`.

   onStatus stays C1 for now (single `String`-replace branch); slated
   for replacement when the status bar moves to a Wren-script-driven
   model so its contract dies with the old surface.

   `onMatch`-as-C2 is a deliberate constraint: honouring `true =
   consume` would require holding every input byte until every
   potential pattern resolves, which a streaming terminal can't do.

   To catch the common "I thought I could consume from C2" mistake:
   `wren_host.c:warn_if_c2_misused()` runs after every C2 dispatch
   and logs a runtime error any time the callback returned Bool /
   String.  Fires every time, not once — the noise is the diagnostic.
   A misused `every(50)` hook will spam ~20 lines/sec until the
   script returns `null` (or just doesn't `return` at all).

   `Wren.adoc`'s Hook section now leads with a `[[hook-contracts]]`
   subsection presenting C1/C2 in one table; each individual hook
   row in the per-method table cites its contract.  Built clean;
   cterm_test 316/316.

3. **Two error-reporting mechanisms.** `SFTP.<op>` returns `SFTPError` *as a
   value*; `File.<op>` on a dead handle calls `Fiber.abort`. Both subsystems do
   the same logical thing (report failure) but force different demuxing
   patterns. The `is SFTPError` checks litter `sftp_queue.wren` (lines 401-470,
   544-580). Pick one model:
   - **Either** make SFTP throw via `Fiber.abort` and let scripts use
     `Fiber.try()` for catching (cleaner reads).
   - **Or** make File errors return a value type too.

   Mixing them is the worst of both worlds.

   **Done.** Walked all 33 abort sites across `wren_bind_fs.c` and
   `wren_bind_sftp.c`.  Findings:

   - **SFTP was already principled** — all 4 abort sites are
     programmer errors (wrong type, closed handle, SFTP not built).
     Every recoverable failure goes through `SFTPError`.  No fix
     needed.
   - **File had ~13 abort sites firing on recoverable I/O failures**
     (write failed, mmap failed, OOM mid-op, file vanished out from
     under an open handle, etc.).  These are now `FileError` returns
     instead.  The original audit's "two error mechanisms"
     framing was misdiagnosed: the inconsistency wasn't between File
     and SFTP, it was *internal to File* — and the user's call was
     to push File's recoverable failures through a value type
     mirroring SFTPError.  Reasoning: nobody will check the result
     anyway, and silent ignore-and-continue is less annoying than
     fiber abort.

   Implementation:
   - New `SWF_FILE_ERROR` foreign type, `wren_file_error` struct
     (`code`, `errno`, `message`), allocator/finalizer, accessors,
     `toString`, and `file_build_error()` helper in `wren_bind_fs.c`.
   - 13 Bucket B + C `wren_throw` sites converted to
     `file_build_error(vm, 0, FILE_ERR_*, errno, msg)`:
     `Cache: failed to resolve path` (RESOLVE_FAILED), `File:
     backing file no longer exists` (VANISHED ×2 — file + dir),
     `File: open failed` / `write failed (reopen)` (OPEN_FAILED),
     `write failed` (WRITE_FAILED ×4), `out of memory` (OOM ×2),
     `flength failed` (STAT_FAILED ×2), `mmap failed`
     (MMAP_FAILED).
   - 16 Bucket A sites still abort: dead-handle use, not-open,
     already-open, negative-arg, offset-past-end, wrong-type
     setter, "Cache: no BBS context" (impossible-state).
   - Wren-side: `foreign class FileError` declaration + `class
     FileErr` enum (8 named codes) in `syncterm.wren`.
   - `Wren.adoc`: new `==== FileError [[fileerror]]` subsection
     after the File table; FileErr constants enumerated; explicit
     "what is NOT a FileError" call-out for programmer errors and
     EOF.  Updated the dead-handle paragraph to distinguish
     programmer-deletion (abort) from external-`rm` (vanished
     FileError).
   - `wrentest.wren`: `testFileErrConstants_` verifies the FileErr
     enum + that `FileError` is importable.  The actual Bucket B/C
     paths (write failed, mmap failed, vanished, OOM, …) all
     require OS-level conditions (disk full, read-only mount,
     external `rm`, OOM) that pure Wren can't deterministically
     trigger — so they're verified by code review only.  Note that
     `Cache.delete(name); f.size` does NOT exercise the vanished
     path because `Cache.delete` actively invalidates the live
     handle (Bucket A: programmer-error abort).  External rm of
     an open file would, but there's no in-Wren mechanism for it.

   Built clean; cterm_test 316/316.  Wrentest case runs via Alt+T.

   **Follow-up audit (after B.3 itself was done, the user pointed
   out the rule should hold across the WHOLE API):** dispatched
   three parallel agents to classify every error site in the
   remaining bindings (`wren_bind_screen.c`, `wren_bind.c`,
   `wren_bind_conn.c`, `wren_bind_hook.c`, `wren_bind_won.c`).
   Triaged the reports skeptically (agents over-flag in this
   space too — many "should be a typed error" claims were really
   "documented null contract" or "OOM, which we keep as abort").

   Tier 1 (real, high impact) — DONE:
   - **`WONError`** — every `WON.deserialize` parse failure now
     returns a `WONError` value instead of aborting.  `WONError`
     foreign + 7-value `WONErr` enum (`ok`, `syntax`, `truncated`,
     `invalidEscape`, `invalidKey`, `tooDeep`, `trailingData`).
     `won_set_error()` helper writes the error to slot 0.  Batch
     converted 27 `won_throw` sites via Python.  Top-level
     `fn_WON_deserialize` now passes through whatever slot 0 holds
     (parsed value or `WONError`).  Programmer error
     ("argument must be a String") still aborts.  Wrentest's
     `testWONDeserializeErrors_` rewritten to verify the
     value-return contract + that the type-error programmer-error
     case still aborts.
   - **`ConnError`** — `Conn.send` / `Conn.sendRaw` now check
     `conn_connected()` and `conn_send()` return value, return
     `ConnError(notConnected)` or `ConnError(shortSend)` (with
     `bytesSent` for retry) on failure, `null` on success.  Was
     silently dropping send failures.  3-value `ConnErr` enum.

   Tier 2 (silent programmer errors → abort) — DONE:
   - `fnScreenWindow_bounds_set` — bad type / wrong-length List
   - `fnScreenWindow_position_set` — bad type / wrong-length List
   - `fnScreenFonts_subscript` / `_set` — slot index out of 0..4
   - `fnPalette_subscript` / `_set` — negative index (kept null
     return for "palette entry not loaded" — runtime condition,
     not bad arg)
   - `fnPalette_mode_set` — bad type / wrong-length List
   - `fn_Surface_subscript` — `[i]` out of bounds (cellAt and
     iteratorValue still go through the helper that returns null
     for documented contracts)
   - `fn_REPL_compile_` — non-String args
   - `fn_REPL_hasModule` — OOM (now aborts loudly instead of
     silently returning false)

   OOM strategy decision (per user): **OOM stays as abort.**
   Allocating a typed-error value on the OOM path needs more
   memory; not viable.  Documented in `WONError` section.

   Agent items NOT taken (defensible as-is):
   - Hook registration `null` on limit — already documented
     contract.
   - Documented null returns: `Screen.readRect`/`Console[seq]`/
     `Hyperlinks[id]`/`KeyEvent.codepoint`/`Host.sshPublicKey`/
     `Conn.peek(n<=0)`.
   - `HookHandle.*Runtime` returns 0 on dead handle — documented.

   Built clean; cterm_test 316/316.  WONError doc subsection
   added under WON; ConnError doc subsection added under Conn.

   **Polymorphic `Error` base — DONE.**  Added `class Error {}`
   (empty marker) at top of `syncterm.wren`; every foreign Error
   subclass declares `is Error` (FileError, SFTPError, WONError,
   ConnError).  Scripts can now demux any recoverable failure with
   one check: `if (v is Error) ...`.

   The base has to be field-free because Wren rejects foreign-class
   inheritance from a parent with any `_foo` references (verified
   at `wren_vm.c:559-563`: `numFields == -1 && superclass->numFields > 0`
   → "Foreign class 'X' may not inherit from a class with fields").

   Added `class ScriptError is Error` as a concrete pure-Wren
   constructible base for script-defined error types
   (`construct new(code, message)` with `code`/`message` getters and
   a `toString` that uses `this.type.name` so subclasses don't need
   to override).  Foreigns inherit the empty `Error`; user code can
   `class MyConfigError is ScriptError {}` and get the constructor.

   `Wren.adoc` gained a top-level `=== Error / ScriptError [[error]]`
   subsection at the head of the Object Model section, documenting
   the polymorphic contract and the foreign-class field-free
   constraint.  Wrentest's `testWONDeserializeErrors_` now also
   checks `bad is Error` (polymorphic) and a fresh ScriptError
   verifies the constructor + `toString` shape.

4. **`Conn.recv` vs `File.readBytes` vs `SFTP.read`.** Three "read N bytes"
   methods, three different shapes:

   | API | Signature | Returns |
   |---|---|---|
   | `Conn.recv(n)` | sync | bytes |
   | `File.readBytes(count)` / `(count, offset)` | sync | bytes |
   | `SFTP.read(fiber, handle, offset, count)` | async | bytes / null / SFTPError |

   The argument order alone is inconsistent (`offset, count` vs `count, offset`
   between File and SFTP). At minimum, swap one to match.

   **RESOLVED (2026-05-03):** Swapped `SFTP.read` argument order to
   `(fiber, handle, count, offset)` to match `File.readBytes(count, offset)`.
   Renamed `Conn.recv(n)` / `Conn.peek(n)` parameter to `count` for
   vocabulary consistency.  The async vs sync split (Conn/File sync,
   SFTP async with fiber + null/typed-error contract) stays as-is — it
   reflects real differences in transport, not API churn.  Updated
   callers in `sftp_pubkey.wren`, `sftp_queue.wren`, and Wren.adoc.

5. **`Cell` accessors vs `CTerm` accessors.** ~~Both expose a 1-byte attribute
   and RGB foreground/background, but with different vocabularies~~ —
   the original framing was wrong on both counts.

   The two classes describe *different* things at *different* scopes:
   - `Cell.*` is persistent storage of one on-screen vmem cell — needs
     the full ciolib bit layout because it has to round-trip Prestel
     control chars, dirty-tracking, double-height, etc.
   - `CTerm.attr/fgColor/bgColor` is the *current pen state* the next
     written character will use — `attr` is the legacy 8-bit SAUCE
     attr that every ANSI BBS script reaches for; `fgColor`/`bgColor`
     are the modern uint32 representation.

   And the "magic side-effect bits 24..30" are not magic: per
   `ciolib.h:295-324` those bits encode the Prestel control character
   (fg) and the double-height/Prestel/pixel-graphics/reveal/dirty/
   separated/terminal flags (bg). The setters preserving them across
   writes is correct — stripping would corrupt cells and break the
   dirty-tracking that the renderer depends on.

   ~~The real (smaller) gap is the other direction~~ — also wrong.
   `cterm->fg_color`/`bg_color` always hold *resolved* RGB triples:
   `attr2palette` (`bitmap_con.c:413-429`) looks up `vstat.palette[N]`,
   which stores entries as `0xFFRRGGBB` (`bitmap_con.c:2547`). There's
   no palette-index state for the cterm pen to expose — the
   palette-vs-RGB split exists only at the per-cell level (where
   non-Prestel/non-pixel cells *can* still be palette-indexed). The
   binding's `& 0xFFFFFFu` mask just strips the implicit RGB flag so
   the script sees a clean `0xRRGGBB`. Correct.

   **RESOLVED (no changes — original framing was wrong on every count).**

## C. Naming inconsistencies

These are easy to fix now and impossible to fix later. Walk through the
surface and pick one rule per axis.

1. **camelCase vs snake_case in enum members.** `Codepage.cp1251_b` and
   `iso8859_4` use snake_case; `Codepage.petsciiUpper` and `atariSt` use
   camelCase; `ScreenMode.c80x25` uses neither (digits glued to letters).
   Standardize. Suggestions:
   - Keep `cp1251`, `cp866m`, `cp866m2` as-is (the suffix is part of the
     historical name).
   - But `iso8859_4` could be `iso88594` or `latin4`; `_b` could be
     `BrokenVert` or a separate flag.
   - `atariSt` ↔ `atari` ↔ `atariXep80`: pick one casing;
     `Atari8Bit`/`AtariSt`/`AtariXep80` reads better.

2. **Helper-class naming for nested namespaces.** `Screen` exposes:
   - `Screen.window` → `ScreenWindow`
   - `Screen.supports` → `ScreenSupports`
   - `Screen.font` → `ScreenFonts` (note plural!)
   - `Screen.palette` → `Palette` (no `Screen` prefix)
   - `Screen.color` → `Color`
   - `Screen.cursor` → `CustomCursor`
   - `Screen.videoFlags` → `VideoFlags`

   No coherent rule. Either:
   - Always prefix with `Screen` (`ScreenPalette`, `ScreenColor`, etc.), or
   - Never prefix and rely on the navigation path to disambiguate (`Window`,
     `Fonts`, `Palette`).

   Singular vs plural also: `ScreenFonts` (plural) but `Palette` and `Color`
   (singular). Pick a rule.

3. **`Cache` / `Download` / `Upload` module-level vars vs `Host.cacheDirectory`
   / `Host.downloadDir` / `Host.uploadDir` foreign methods.** Same data
   exposed twice, with renames (`cacheDirectory` ↔ `Cache`, `downloadDir` ↔
   `Download`). The Wren-side `var Cache = Host.cacheDirectory`
   (`syncterm.wren:1163`) is the published surface; `Host.cacheDirectory` is
   documented but redundant. Either hide the `Host.*` foreigns (only the
   module-level vars are public) or drop the re-export.

4. **Trailing `_` "private" methods that aren't private.** `Codepage.encodes_(text)`
   is a class-private convention (per `wren.md` §3) but it's used by the UI
   library's `Glyphs` (a different module). Either drop the `_` (it's part
   of the API), make it a public name (`canEncode`), or move it to a place
   that isn't pretending to be private.

5. **`Hyperlinks.add(uri, idParam)` arg name.** The class uses `id` as the
   index key everywhere else (`Hyperlinks[id]`, `Hyperlinks.containsKey(id)`);
   the `add()` arg `idParam` reads as if it's the ID being added rather than
   what it is (the OSC 8 `id=` parameter). Rename to `params` or `idParam` →
   `paramString`.

6. **Function key constants vs `Mouse` constants.** `Key.f1`-`f12` named,
   `shiftF1`-`shiftF12`, `ctrlF1`-`ctrlF12`, `altF1`-`altF12` all defined.
   But `Mouse` only defines `button1*` (10 entries) and `wheelUp/Down*` (4
   entries) — buttons 2 and 3 are completely absent from the enum, leaving
   users to do bare-integer math against the documented "offset by 9 per
   button" rule. Either expose `button2*`/`button3*`/`wheelUp*`/`wheelDown*`
   constants explicitly, or reduce both enums to a documentation table.

7. **`Font` named constants for only 14 of 46 slots.** `Font.cp437English` is
   the only named CP slot; slots 1-31 (every other codepage font) require
   numeric lookup. If naming is the convention, name them all.

## D. Repeated patterns that suggest missing abstractions

1. **Modal screen ceremony — 25+ occurrences.** Every script that takes the
   screen wraps the same dance:
   ```wren
   var saved = Screen.save()
   CTerm.suspended = true
   ...                             // do the modal work
   CTerm.suspended = false
   Screen.restore(saved)
   ```
   This is documented in `ui_demo.wren` ~18 times, in `console.wren`,
   `sftp_app.wren`, and the test harness. **Recommendation: add
   `Screen.modalRun(fn)` (or `App.runModal()`) that captures
   save/suspend/restore atomically.** Forgetting the suspend toggle leaks
   remote bytes onto the modal screen — that's a foot-shooter.

   **RESOLVED (2026-05-03):** Added `Screen.modalRun(fn)` to
   `syncterm.wren` — saves screen + saves+sets `CTerm.suspended`,
   calls `fn` directly on the caller's fiber, restores both.
   Returns `fn`'s value.  No `Fiber.try` wrapper because async
   `App.run`'s `drainOnce_` does `Fiber.yield()`, which would bubble
   up to the wrapper and kill the event loop on the first event;
   matching the manual pattern's "abort skips cleanup" semantics is
   the right trade.  Converted callers: `sftp_app.wren:run` (the
   canonical case) and 10 demo methods in `ui_demo.wren`.  Also
   `wrentest.wren:testScreenRectRoundtrip_`.  `console.wren` left
   alone — uses the screen-save dance for a different reason (REPL
   claim, not BBS-byte gating) plus its own `Screen.window.bounds`
   save/restore around the body, so converting would lose more than
   it gains.

2. **Static-vs-instance "stage and commit" pattern.** `CustomCursor` and
   `VideoFlags` both implement: every property as `foreign static X` AND
   `foreign X`, plus `apply()`. Two classes use it; the binding code
   duplicates *every getter and setter twice* (one binding per static, one
   per instance). If a third "staged settings" class lands, this triples.
   Either:
   - Generalize: a `Stage(target)` class that shadows reads/writes and commits
     via `apply()`, or
   - Drop the static accessors and use `CustomCursor.current.X` everywhere.

   The "live state" use case for the static getters is rare in scripts; most
   uses snapshot via `.current`.

   **RESOLVED (2026-05-03) — both restructuring suggestions rejected after
   closer look; small documentation + helper additions made instead.**

   The two access paths serve distinct legitimate use cases:
   - *Static* — cheap, ergonomic for live reads (`if (CustomCursor.visible) ...`)
     and single-field writes (`CustomCursor.blink = true`).  Each write is a
     full read-modify-write under the hood, atomic for that one field.
   - *Instance* — required for atomic multi-field changes (chained statics
     race the renderer between writes), for snapshot/restore (the App's
     `setupCursor_` / `teardownCursor_` pair at `ui_app.wren:547-555` does
     exactly this), and as the carrier for preset values
     (`CustomCursor.normal/.solid/.none`).

   "Drop the statics" loses the ergonomic single-write path.  "Stage(target)
   wrapper" doesn't actually reduce the C-side binding count — Wren is
   statically dispatched, every property still needs a table entry.  The
   `CC_INT_FIELD` / `CC_BOOL_FIELD` macros in `wren_bind_screen.c:1410-1458`
   already dedupe the function bodies; the only real duplication is ~20
   extra binding-table entries across both classes, which isn't worth a
   structural rewrite.

   What was actually added:
   - **Docstrings** on both classes warning about chained-static
     non-atomicity, with the "use the instance path" recommendation.
   - **`CustomCursor.preserve(fn)`** and **`VideoFlags.preserve(fn)`** —
     snapshot, run `fn`, re-apply the snapshot.  Same fiber semantics as
     `Screen.modalRun` (no Fiber.try wrapper; aborts skip cleanup).  No
     existing call sites convert (the only `.current` use in scripts is the
     App lifecycle pair, which is split across run() boundaries and can't be
     wrapped in a closure), but the helper is there for future scripts that
     want the temporary-cursor-change pattern.
   - **Wren.adoc** — Screen.cursor / Screen.videoFlags sections updated
     with the non-atomicity caveat and the preserve helper.

3. **`App.runChild` exists because the SFTP `|| Fiber.yield()` shortcut is
   unsafe in fibers with multiple resume sources.** This is the right tooling,
   but it's also a sign that the `||`-yield trick is too clever. If
   `SFTP.<op>(fiber, ...)` always queued and never returned the result
   synchronously, the `||` would be unnecessary and `runChild` would be the
   only correct shape, not the "use this if your fiber has other resume
   sources" footnote (Wren.adoc:3399-3403). Consider making the
   queue-vs-immediate behavior consistent.

   **RESOLVED (2026-05-03) — doc reframe only; API is correct as-is.**

   Surveyed every `||`-yield call site (sftp_pubkey × 3, sftp_queue × 6,
   sftp_app loadDir × 5, sftp_app.descs × 1).  All sit in fibers that are
   single-source by construction:
   - hook handler fibers (host-spawned, fresh per invocation)
   - SftpQueue worker fibers (queue-spawned, long-lived, single-source by
     design)
   - `App.runChild` children (App-spawned for exactly this purpose)

   No call site uses `||`-yield in a multi-source fiber.  The "footnote"
   was actually the universal pattern.

   Both originally proposed restructurings rejected:
   - **"Always queue, never return sync"** would force a fiber-park-then-
     resume round-trip for the most common error path (no session, OOM)
     for zero behavioral benefit — `||` already handles both paths
     uniformly from the caller's view.
   - **"Make runChild the only correct shape"** would force every caller
     to have an `App` (which `sftp_pubkey` doesn't, and `sftp_queue`
     workers explicitly are not).

   What was actually done: reframed `Wren.adoc`'s async-pattern section
   (`[[async-pattern]]`) to lead with the universal rule (`||`-yield
   needs a single-source fiber) and list the three providers up front,
   instead of presenting `||`-yield as a "shortcut" with `runChild` as
   the safer alternative.  Also updated the `runChild` section
   (`[[ui-app-runchild]]`) to identify itself as "the App-context
   provider for the single-source fiber the SFTP idiom requires."

   Future work, when/if a multi-source non-App context appears: a
   standalone `Fiber.runIsolated(fn)` primitive that pumps any
   ambient host events while parking on the child.  Today, runChild
   on App is enough — every multi-source fiber in this codebase IS
   an App's run-fiber.

4. **SFTP type-tag demux at every call site.** `if (r is SFTPError) ... if (r
   is SFTPHandle) ...` repeated for `realpath`, `stat`, `opendir`, `readdir`,
   `open`, `read`, `write`. This is a direct consequence of B.3 above; fixing
   the error-reporting mechanism removes most of these.

   **RESOLVED (2026-05-03) — no further action.**

   B.3 already minimized the variance to `Result | <Op>Error` and converted
   internal call sites.  The remaining `is` checks are intrinsic to each
   op (handle vs error, bytes vs EOF marker, list vs error) — they're real
   information the caller needs, not noise that can be sugared away.

   Considered but rejected: a **map-of-callables** call shape
   (`SFTP.read(fiber, h, n, off, {"ok": ..., "eof": ..., "error": ...})`).
   Cost in this codebase:
   - Loses control-flow linearity — closures can't `return` from the
     enclosing function (the readdir loop in `sftp_app.wren:210-222`
     does exactly this: `if (batch is SFTPError) return batch`).
   - Map allocation in tight chunk loops (sftp_queue.wren:448 runs once
     per 16 KiB chunk) is GC pressure for no payoff.
   - Stringly-typed keys with no validation; a misspelled handler silently
     never fires.
   - Each op has a different result shape, so the keys vary per op — no
     uniform contract, every call site still has to know which keys apply.
   - Doesn't reduce the demux, just relocates it.

   A `Result.kind` enum wrapper would be even worse — you'd have to unbox
   `r.value` to get at the payload, costing one indirection per access in
   exchange for nothing.  Map-of-callables is clean for *stateful event
   subscribers* (where the map is allocated once and the handlers stay
   live — `App.bind` is essentially this for keymap), but per-call result
   demux is the wrong shape for it.

   Wren has no tagged unions or `match`; `is`-chains are the idiom.

## E. Wren stdlib adherence — places where the API breaks the protocols

Wren's "small batteries" make protocol consistency more important than in
larger languages. The principles to copy: `Sequence` mixin for anything
iterable, `==`/`hash` for anything map-key-able, `[k]`/`containsKey`/`keys`/`values`
for anything map-shaped.

1. **`Console` does iteration but doesn't declare `is Sequence`.** It defines
   `iterate` and `iteratorValue` (`syncterm.wren:586`), so `for (e in Console)`
   works — but `Console.map { ... }`, `.where`, `.toList`, `.count` (it has
   `count` separately) won't compose. Either declare `is Sequence` or document
   why not.

   **RESOLVED (2026-05-03) — added `Console.entries` Sequence view.**

   The "just declare `is Sequence`" intuition doesn't actually work: Sequence's
   helpers (`map`, `where`, `all`, ...) are defined at `wren_core.wren:7-...`
   as INSTANCE methods that call `this.iterate(...)`.  Console is an all-static
   foreign class, so `Console.map { ... }` would look up `map` on Console's
   metaclass — `is Sequence` only adds methods to instances, and Console has
   no instances.

   Considered a singleton-instance shape (`var Console = Console_.new()` over
   a foreign class with `is Sequence`), but rejected: foreign classes require
   a `construct` declaration with no way to suppress it, so `Console_.new()`
   would always be callable from script.  The result would be functionally-
   identical objects that compare unequal under default identity-based `==` —
   weird and surprising.

   Solution: add `Console.entries` returning a fresh `ConsoleEntries`
   instance that delegates `count`/`[seq]`/`iterate`/`iteratorValue` back
   to Console, and inherits Sequence's helpers.  Call sites that want
   Sequence semantics write `Console.entries.map { ... }`; everything
   else stays as `Console.X`.  Consistent with the existing pattern in
   this codebase (`Screen` is all-static, exposes iteration via the
   instance-shaped `Surface` it returns from `readRect`).

   Wrentest covers it: `testConsoleEntriesSequence_` checks
   `Console.entries is Sequence`, `.toList`, and `.where`.

2. **`Hyperlinks` is map-shaped but not Map-protocol.** It has `[id]`,
   `containsKey(id)`, but no `keys`, `values`, `count`, `iterate`. A script
   can't enumerate hyperlinks at all. Either implement the full Map shape or
   document it as a sparse lookup.

   **RESOLVED (2026-05-03) — documented as sparse lookup.**

   Three barriers stack:
   - Same all-static problem as Console (E.1) — class-object lookups can't
     dispatch through `is Anything` to surface helper methods.
   - Foreigns can't truly `is Map` — Map is a Wren primitive type with
     hash-table-backed `MapObj` storage; its methods are bound to that
     specific layout and won't dispatch on a foreign object's opaque buffer.
     (`is Sequence` works because Sequence's helpers go through generic
     `iterate`/`iteratorValue`; Map has no equivalent.)
   - ciolib doesn't expose enumeration either — `hyperlink_used_count` and
     `hyperlink_used_head` are private to `ciolib.c:123-126`.  Building
     `Hyperlinks.entries` requires new C API (`ciolib_hyperlink_count()` +
     iterator walks of the used-list).

   No current script enumerates hyperlinks.  IDs flow in from `add()`
   returns or `Cell.hyperlinkId`; the consumption pattern is push-from-
   renderer, not pull-from-script.  Building speculative C ciolib API for
   no caller is bad investment.

   Documented as a typed-ID lookup table (not Map) in both `Wren.adoc`
   and the class docstring.  If a caller turns up later that wants
   enumeration, `Hyperlinks.count` / `keys` / `entries` can be added
   without breaking the existing surface (same backward-compatible
   extension shape `Console.entries` used in E.1).

3. **`Cells`/`Surface`** declares `is Sequence` ✓ but the linear iteration
   order through a 2D grid is undocumented (row-major? column-major?). Add
   the contract.

   **RESOLVED (2026-05-03) — order documented + `rows` / `cols` views added.**

   Verified at `wren_bind_screen.c:2177` that the C side maps `cellAt(x, y)`
   to linear index `y * width + x` — row-major.  Documented in both the
   class docstring (`syncterm.wren`) and `Wren.adoc`'s Surface section.

   Also took the opportunity to add Sequence-of-Sequence views since the
   2D-iteration ergonomics were the underlying motivation:
   - `surf.rows` — `SurfaceRows` (Sequence of `SurfaceRow`, length =
     height).  `surf.rows[y]` is a Sequence of width Cells, yielded
     left-to-right.
   - `surf.cols` — `SurfaceCols` (Sequence of `SurfaceCol`, length =
     width).  `surf.cols[x]` is a Sequence of height Cells, yielded
     top-to-bottom.
   - Single-cell access: `surf.rows[y][x]` or `surf.cols[x][y]` (both
     equivalent to `surf.cellAt(x, y)`).

   Wren-side only — all four wrapper classes delegate to `cellAt(x, y)`
   already implemented in C.  Lazy views hold a Surface reference and
   fetch on demand, so mutations propagate.

   Considered and rejected: making `surf[x]` itself return a column
   (the user's first instinct).  Would have changed `surf.count` from
   `width*height` to `width` and broken `for (cell in surf)` — too
   much existing semantics to silently re-mean for a syntactic win.
   The `.rows` / `.cols` namespace gives the same expressiveness with
   zero breakage and intent-visible-at-call-site (`surf.cols[x][y]`
   vs. `surf[x][y]` makes "column-major access" explicit).

   Wrentest covers it: `testSurfaceRowsCols_` builds a 3×2 surface,
   stamps a known pattern, verifies linear / `.rows` / `.cols` order,
   single-cell indexing, and Sequence-helper composition (`.map`).

4. **`Cell` has no `==` / `hash` / structural comparison.** Two cells with
   identical char/attr/colors compare unequal. This is fine for foreign
   objects in general, but means Cells can't go in `Map` keys, can't be
   deduped, can't compare regions efficiently. If Cells are intended as data
   values, define `==`.

   **RESOLVED (2026-05-03) — added `Cell.eqContent(other)`, NOT a `==` override.**

   The Map-key motivation is dead: per `wren_value.h:880-888`,
   `wrenMapIsValidKey` restricts keys to Bool / Class / Null / Num / Range /
   String.  Foreign objects, instances of regular classes, Lists, Maps, Fn,
   Fiber — none are valid Map keys.  Overriding `==` does NOT lift this
   restriction; `validateKey` short-circuits before `==` is ever called.

   Considered overriding `==` to do structural comparison.  Rejected:
   - Would make Cell the ONLY foreign in this codebase where `==` doesn't
     mean identity.  Surprising, violates the established contract for
     foreign objects.
   - The motivation that survives (Sequence-helper composition like
     `surf.contains(c)` and `surf.where {|c| c == ref}`) is ergonomic but
     small.  Doesn't justify a contract violation.

   Added a named method `Cell.eqContent(other)` instead.  Callers explicitly
   opt into structural comparison; the `==` operator stays foreign-identity-
   based.  C-side, field-by-field comparison (NOT `memcmp` — vmem_cell has
   padding bytes that would produce false negatives).

   Equality semantics:
   - non-Cell `other` returns false (no fiber abort).
   - if EITHER cell has `CIOLIB_BG_PIXEL_GRAPHICS`, returns false — pixel
     data lives outside the vmem_cell and isn't compared, so we can't prove
     equality from these fields alone.
   - BG dirty bit (`CIOLIB_BG_DIRTY`) ignored — render bookkeeping, not
     content.
   - everything else compared bit-exact: ch, font, legacy_attr, fg
     (including Prestel control char in bits 24..30 and palette/RGB high
     bit), bg (including double-height/Prestel/reveal/separated/Prestel-
     terminal flags and palette/RGB high bit), hyperlink_id.

   Wrentest: `testCellEqContent_` checks identical match, `==`-stays-
   identity, differing-content unequal, non-Cell returns false, and a
   Surface roundtrip via `cellAt`.

   No `hash` method — Wren has no public `hash` protocol that user-defined
   classes plug into; hashing is internal to Map, which already excludes
   foreigns.  Adding it would be dead code.

5. **`HookHandle.toString`, `KeyEvent.toString`, `MouseEvent.toString`,
   `Cell.toString`, `Surface.toString`** are all declared `foreign toString`
   but their format is completely unspecified. Either pin a format (so
   logging is reproducible) or document them as "for debugging only, format
   may change."

   **RESOLVED (2026-05-03) — documented as debug-only, format not part of
   the contract.**

   Surveyed all five formats — they're all minimal debug strings:
   - `KeyEvent('A' 0x0041)` or `KeyEvent(0x0041)` (printable vs not)
   - `MouseEvent(event=N at X,Y)` or `MouseEvent(event=N at X,Y-X,Y)` (drag)
   - `Cell(0xCH attr=0xAA)` (skips fg/bg)
   - `Surface(WxH)` (skips contents)
   - `HookHandle(calls=N)` (skips everything except call count)

   Pinning rejected — they were obviously written for `System.print` /
   `"%(value)"` debug output, not data exchange.  Locking the formats
   blocks future improvements (Cell.toString could grow fg/bg, Surface
   could include a content hash, etc.) for no real-world benefit; scripts
   that need fields should use property accessors, not parse strings.

   Added a single convention note up front in `Wren.adoc` (right under
   `== Object Model` at line 1498) covering all `toString` methods at
   once: format is not part of the contract, may change between
   versions, scripts should use property accessors instead of parsing.
   The motivation for having `toString` at all is also covered — a
   minimal `Cell(0x41 attr=0x07)` beats the default `[instance of
   Cell]`.

   No code changes; no per-class doc additions needed.

## F. Module organization

`syncterm.wren` is 1329 lines and contains five independent concept islands:
I/O (Screen, Input, Conn, CTerm, Clipboard, Hyperlinks), filesystem (Cache,
Directory, File, Host), SFTP (8 classes), serialization (WON, ~140 lines of
pure Wren), and event scaffolding (Hook, Wake, Timer).

1. **WON could be its own module.** It has zero dependency on other syncterm
   classes and ~half its body is pure Wren. Move to `won.wren` (library
   module). Same applies to `Codepage` (just an enum + one foreign).
2. **SFTP could split out** — it's 8 classes (`SFTP`, `FileFlag`, `SFTPEntry`,
   `SFTPStat`, `SFTPHandle`, `SFTPError`) and conceptually independent. Same
   shape as the UI library: pure Wren split with a foreign-bindings module.
3. **Enum-only classes don't need to be in the foreign module at all.**
   `LogSource`, `LogMode`, `StatusDisplay`, `BBSListType`, `ScreenMode`,
   `AddressFamily`, `MusicMode`, `RipVersion`, `Parity`, `LogLevel`,
   `Emulation`, `ConnType`, `Mouse`, `Key`, `Codepage`, `Font`, `FileFlag`
   — at least 17 classes, ~250 lines. They could live in
   `syncterm_enums.wren` and be re-exported.

The goal isn't churn — it's that future per-module evolution gets easier when
the splits are reasonable.

## G. Future-proofing concerns

These are decisions that will be hard to walk back once user scripts depend on
them.

1. **Codepage / Font / ScreenMode by integer index.** The enums map directly
   to internal C indices (`Codepage.cp437 == 0`). If we ever reorder,
   renumber, or remove a font/codepage in C, every script breaks. Either:
   - Switch the enum values to opaque interned strings (`Codepage.cp437`
     returns `"cp437"`, foreign methods accept both) — slower but rename-safe.
   - Or commit publicly to never renumbering. Document that the integer values
     are part of the API.

2. **`File.token` opacity is bound to the picker.** The capability-token
   pattern (HMAC over path+SHA256) is a great design, but binding it
   specifically to "files came from the UIFC picker" forecloses other paths
   to user-consented file access:
   - User editing a path string in a config dialog
   - SSH known_hosts entries
   - Drag-and-drop (future)

   Consider exposing a more general `Host.consent(path)` that mints a token
   from any context where the host has a valid reason to grant access, with a
   typed reason field. Otherwise every new consent path will look like the
   picker did.

3. **`ScreenMode` enum mixes width×height (`c80x25`), display family
   (`atariSt40x25`), and "magic" entries (`current`, `custom`).** As new modes
   get added, this taxonomy decays. Consider:
   - A `ScreenMode.dimensions(mode)` method returning `[w, h]` and an explicit
     `ScreenMode.familyOf(mode)`, so scripts query rather than parse the name.

4. **`Hook` takes Wren callables but doesn't carry context.** Every hook must
   close over its state. There's no `userData` argument or "owner" tag. If we
   ever want to support "stop all hooks owned by X" (e.g. when an App tears
   down), we'll have to add it on top. Consider an opt-in
   `Hook.onKey(key, fn, owner)` from the start.

5. **`Hook.dispatch_` is in pure Wren** (`syncterm.wren:917-941`). That's
   elegant for catching yields, but it means user overrides of the syncterm
   module would replace dispatching too. Document that `dispatch_` is the
   dispatcher contract or harden against override.

## H. Specific footguns worth flagging

1. **`Hook.onMatch`/`onMatchClean` callback returns silently ignored** — see B.2.
2. **`Cell.fgRgb=` magic bit-preservation** — see B.5.
3. **`||` short-circuit pattern** assumes the user knows that `null` from
   SFTP means "queued, must yield" and an `SFTPError` means "don't yield."
   Forgetting either case is silent corruption (yield with no resume → fiber
   stuck; or treating queued-null as a result → wrong logic).
4. **`Input.pushClaim` GC fallback.** If a script forgets to `pop()`, the
   foreign finalizer eventually does — but Wren's GC isn't deterministic, so
   a same-fiber re-push between forgotten-pop and GC-pop would silently take
   its slot. Add a debug-only warning when a claim is GC-popped.
5. **`Codepage.cp437` is hardcoded as the cell-storage codepage** in numerous
   Wren-side comments (`syncterm.wren:430-433`, Glyphs auto-fallback logic).
   If we ever support non-CP437 cell storage (Unicode terminals?), every
   Wren-side glyph library breaks. At minimum, expose `Codepage.cellStorage`
   so scripts query it instead of assuming.
6. **`Conn.send` and `CTerm.write` look interchangeable** but route differently
   (wire vs local). Easy to mix up. Doc-only fix: add a one-paragraph "which
   one to use when" guide.

## J. Slot-lifetime audit (done during B.1)

Triggered by the `dispatch_claim_event` slot-lifetime bug surfaced
during B.1.  Three parallel agents audited every `wrenEnsureSlots` /
slot-write site across the eight binding files; a follow-up pass
audited every slot READ outside foreign-method entrypoints.  After
re-walking each finding line-by-line:

**Real bugs found:** 2.
- `dispatch_claim_event` held the event in a slot across `wrenCall`
  truncations (fixed in B.1 follow-up 2 — now takes a pinned
  `WrenHandle*` C arg).
- `fn_SFTP_setMtime` (`wren_bind_sftp.c:986-1007`) held a
  `wrenGetSlotString` path pointer across `sftp_call_prelude` /
  `wrenGetSlotDouble` / `sftp_build_synth_error`.  Works in current
  Wren (non-moving GC + slot 2 stays rooted) but violates wren.md §6.
  **Fixed:** path is now copied into a stack `char path[MAX_PATH+1]`
  via `strlcpy` before any other VM activity.

**Agent findings that didn't survive close reading (false alarms):**
- `wren_host_dispatch_input` / `wren_host_compose_status` — agents
  flagged "bytes/string pointer held across next loop iteration's
  wrenCall." Both functions `return` immediately after using the
  pointer; the next iteration never happens.
- `dispatch_match_drain` — slot 2 list "across wrenCall." The list
  is consumed AS the wrenCall arg; nothing reads slot 2 after the
  call.
- `parse_map` (WON) — recursion uses `slot+1` / `slot+2`; each level
  uses its own slot range; outer key in `slot+1` is preserved
  because no recursion writes to slot 1 of the outer frame.
- `slot_cell_data`, `slot_to_surface_`, `load_class_into_slot`,
  `surface_make_view`, `build_sftp_entry`, `dir_list_impl` — agents
  flagged various "could be" cases; close inspection showed each
  caller uses the returned pointer / slot value with no intervening
  VM call before use.

**Net lesson:** agent reports needed careful re-walking; both
"BROKEN" and "PARANOID" tags were over-applied in different
directions.  Future audits should require the agent to cite the
EXACT VM call between write-and-use, not just "may GC eventually."

## I. What's good and shouldn't change

So you don't second-guess the parts that work:

- **The fiber-arg async pattern** (`SFTP.<op>(fiber, args...)`,
  `Timer.trigger(fiber, ms)`, `Wake.post(fiber, value)`) is consistent and
  idiomatic Wren. Don't break it.
- **Wren-side `Hook.dispatch_`** wrapping is the right place to enforce
  hook-yield discipline.
- **`HookHandle.remove()` + GC fallback** is a clean lifecycle.
- **`Input.unget(ev)` polymorphism** is a textbook good wrapper.
- **`Directory` two-layer staleness check** (active invalidation + per-call
  existence) is pragmatic and well-documented.
- **`WON` as a non-eval safe parser** is exactly right for round-tripping
  persisted state.
- **The `is_cache`/`relaxed-name` filename policy split + capability tokens**
  is a security-conscious model worth keeping.

---

## Recommended order of attack

1. ~~**Fix doc drift (A)**~~ — DONE.
2. **Settle return-shape inconsistencies (B.1, B.2, B.3)** — these are the
   costliest to defer; user scripts will lock them in fast.
   - B.1 (MouseEvent unification + constructor overloads + bstate +
     KeyEvent encoding validation + slot-lifetime fix) — **DONE**.
   - B.2 (hook return-value contract unification — collapsed to C1/C2,
     C2-misuse runtime warning) — **DONE**.
   - B.3 (FileError for File's recoverable I/O failures; SFTP was
     already principled) — **DONE**.
   - B.3 follow-up (rule audited across remaining bindings, Tier 1
     + Tier 2 fixes applied) — **DONE**.
3. **Naming pass (C)** — bulk rename now while scripts are few.
4. **Decide on enum-stability commitment (G.1)** — either lock the integers or
   move to strings.
5. **Add the modal helper (D.1)** — eliminates 25+ identical sequences.
6. **Module-split (F)** — defer until the above settle; resplitting after a
   rename pass is cheaper.
