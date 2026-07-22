# Graphical-door idle-user detection — design

Date: 2026-07-21
Status: design approved; not yet implemented

## Problem

Synchronet's per-program inactivity timeout — SCFG's "Maximum Inactivity",
`xtrn.ini` `max_inactivity`, `xtrn_t.max_inactivity` — cannot detect an idle
user in any of the termgfx graphical doors. Not "is misconfigured": cannot.

`xtrn_sec.cpp:1427` does apply the setting for the duration of the door run,
saving and restoring `max_socket_inactivity` around it. Enforcement lives in
`input_thread()` (`main.cpp:2188`), which tests a counter that is reset on any
successful socket read:

```c
// main.cpp:2241 -- after a successful read of ANY bytes
sbbs->socket_inactive = 0;
```

Every termgfx door drives its frame loop with DSR-ack pacing (`termgfx/pace.h`):
it emits a frame followed by `ESC[6n` and waits for the terminal's
cursor-position report. The terminal answers roughly ten times a second on its
own, with no human involved. `socket_inactive` is therefore reset ~10x/sec for
the entire session and never approaches the threshold.

The failure is silent and points the safe-looking way: a sysop who sets
"Maximum Inactivity" on a graphical door gets a setting that reads as
configured and enforced, and does nothing.

### Observed case

Node 11, 2026-07-21: user 604 in `syncnes` (SyncRetro, fceumm NES core), 236
minutes into a session. The TCP connection was healthy — `ESTAB`, 291 MB sent,
peer ACKing at ~116 ms RTT, 24 retransmits across four hours — and the door was
rendering normally at ~10 fps and ~20 KB/s.

Sampling the inbound byte and segment counters showed every inbound TCP segment
was **exactly 6 bytes** (`ESC[1;1R`, the pace-ack) at ~10/sec, matching the
frame rate 1:1. Across 1,759 consecutive segments there was zero excess
payload: no keystrokes at all. The user had walked away, and nothing at any
layer was positioned to notice.

The door's own `-t%T` deadline was the only backstop, and because
`gettimeleft()` (`userdat.c:4226`) clamps to `0x7fff`, it was ~9h06m out.

## Goals

- A graphical door detects that its *user* has stopped providing input, on a
  signal that DSR pacing cannot forge.
- The threshold is sysop-configurable, and users exempt from inactivity
  timeouts by the BBS's own convention are exempt here too.
- A silent user who is still engaged — watching a cutscene, an FMV, a
  deliberately-running attract loop — gets a warning and a chance to stay,
  rather than being dropped mid-scene.
- The mechanism is shared, so the remaining five doors adopt it rather than
  each growing its own.

## Non-goals

- **Sibling-door adoption.** `termgfx/idle.{h,c}` is built to serve all six
  doors, but only SyncRetro wires it up in this change. syncduke, syncdoom,
  syncscumm, syncconquer and syncmoo1 adopt it as follow-on work, tracked
  below, so the behavior difference is deliberate and visible rather than
  drift.
- **Hard disconnect from the door.** The door exits to the lobby; hanging up
  is the BBS's job (see "Exit path").
- **ARS evaluation in C.** The door has no `scfg_t` and no user record.
  Evaluating an ARS there would mean linking `userdat` and re-reading the user
  base for one boolean. The lobby does it instead.
- **Fixing `max_inactivity` itself.** Making the Terminal Server distinguish
  user input from terminal auto-replies would mean teaching `input_thread()`
  to parse terminal responses — a much larger change to a much more
  load-bearing component, and one that would still be blind to a door that
  reads the socket directly via `-s<fd>`. Out of scope.

## Design

### Where the code lands

| Piece | Home | Shared by |
|---|---|---|
| Idle clock + state machine | new `termgfx/idle.{h,c}` | all six doors |
| `parse_duration()` default-unit arg; ini + ARS → seconds | `exec/load/game_lobby.js` | all door lobbies |
| `-i` parse, activity hook, warning paint, exit check | SyncRetro (this change) | — |

`termgfx/idle.c` is deliberately dumb: no I/O, no config parsing, no rendering,
no clock of its own. It is fed a monotonic millisecond stamp by its caller. That
keeps it usable both by SyncRetro, which owns its I/O loop, and by syncscumm,
which goes through `termgfx_termio`, without either restructuring — and it
makes the module a pure function, unit-testable in the existing
`test_binds.c` / `test_dirty.c` / `test_quant.c` pattern.

Proposed surface:

```c
typedef enum {
    TERMGFX_IDLE_ACTIVE,   /* within threshold                       */
    TERMGFX_IDLE_WARN,     /* inside the grace window; see secs_left */
    TERMGFX_IDLE_EXPIRED   /* grace elapsed; caller should exit      */
} termgfx_idle_state_t;

/* threshold_s == 0 disables: poll() then always returns ACTIVE. */
void termgfx_idle_init(unsigned threshold_s, unsigned warn_s, uint32_t now_ms);
void termgfx_idle_activity(uint32_t now_ms);
termgfx_idle_state_t termgfx_idle_poll(uint32_t now_ms, unsigned *secs_left);
```

`warn_s` is clamped to `threshold_s` at init, so a misconfigured
`warn > timeout` degrades to "warn immediately, exit at the threshold" rather
than underflowing into a never-firing or instantly-firing timer.

### What counts as activity

The entire point is that "the socket had bytes" is the wrong signal. Activity
is anything that reaches the door's key/action dispatch — for SyncRetro,
`sr_key_apply()` (covering both the byte path via `sr_key_byte()` and edges via
`sr_key_edge()`), the SyncTERM evdev path, and mouse button/motion events.

Explicitly *not* activity: every terminal auto-reply that `sr_csi_final()`
already handles as a distinct case —

| Reply | CSI final | Source |
|---|---|---|
| Cursor-position report (pace-ack) | `R` | `termgfx/pace.h` frame pacing |
| Device attributes / CTDA | `c` | capability probe |
| XTSMGRAPHICS (sixel geometry) | `S` | capability probe |
| Window-op text-area size | `t` | geometry probe |
| kitty keyboard-flags query reply | `u` | key-mode negotiation |
| Audio DSR ack | — | `termgfx/audio_mgr.c` |

That the input parser already separates these two populations is what makes the
hook small: `termgfx_idle_activity()` goes on the dispatch side, and no
auto-reply path touches it.

The clock keeps running while a door screen or pause menu is up — a user who
pauses and walks away is still idle — but keys that arrive while suspended
still count, because `g_anykey` is set even for unbound keys.

### Configuration

In the door's own ini (`xtrn/syncnes/syncretro.ini` and siblings), which the
lobby already reads for `[roms]` and `[lobby]` (`syncretro_lobby.js:74-77`),
following the established split: the console's half is the spec in `lobby.js`,
the sysop's half is the ini.

```ini
[idle]
timeout    = 15m         ; 0 or absent = disabled
warn       = 60s         ; countdown before exit
exempt_ars = EXEMPT H    ; evaluated by the lobby via bbs.compare_ars()
```

`exempt_ars` defaults to `EXEMPT H`, matching `getnode.cpp:146`
(`useron.exempt & FLAG('H')` sets `CON_NO_INACT`), so an H-exempt user is
exempt here for the same reason they are exempt everywhere else. A sysop
wanting different criteria writes any valid ARS. `EXEMPT` is an ARS keyword
taking a letter argument (`ars.c:466`); a bare `H` is not valid ARS.

The lobby evaluates the ARS and appends `-i<seconds>` to the door command line,
alongside the existing `-s%H -t%T`. Exempt users get an explicit `-i0` rather
than an omitted flag, so "exempt" positively overrides any ini default instead
of silently falling through to it.

**Precedence**, mirroring `-t%T`: `-i` wins when present. Absent it, the door
reads `[idle] timeout` from its own ini — the path taken by a door registered
directly as an `xtrn` and by dev/standalone runs, where no lobby computes a
value. That path cannot evaluate the ARS, so the raw timeout applies.

### The duration-parsing hazard

`game_lobby.js:531` already has `parse_duration()`, but its bare unit is
**days**, while xpdev's C `parse_duration()` — which the door will use for the
same key on the no-lobby path — treats a bare number as **seconds**. The same
ini string would mean 15 minutes to the door and 900 days to the lobby.

That failure is silent and points the safe-looking way: a sysop writing
`timeout = 900` and expecting 15 minutes would get an effectively disabled
timeout — reproducing precisely the trap this change exists to remove.

Fix: give the JS function an optional default-unit argument.

```js
function parse_duration(str, dflt_unit)   // dflt_unit defaults to "d"
```

The default stays `"d"`, so the existing `activity_max_age` caller is untouched.
The `[idle]` keys pass `"s"`, so both parsers agree on every string, suffixed or
bare.

### Behavior and exit path

At `timeout - warn`, the door paints a warning with a live countdown through
its existing overlay path, and keeps painting it as the countdown runs. Any
real key clears the warning and resets the clock — no menu, no confirmation, no
penalty.

If the grace elapses, `sr_door_should_exit()` returns 1 and the door tears down
exactly as the existing `-t` deadline does. Control returns to the lobby, then
to the BBS menu.

This composes with the layer below rather than duplicating it. Once the door
exits there is no more DSR pacing, so `socket_inactive` starts accumulating for
real and Synchronet's ordinary `max_session_inactivity` reaps the user from the
menu on its own. The door does not need to hang up the socket, and a user who
returns during the BBS's own grace can still do something else with the
session.

The exit is logged to the node log the way the existing deadline exit is, so a
sysop reading logs can tell an idle-reap from a quit or a time-limit exit.

## Testing

- `test_idle.c`, in the existing door unit-test pattern: threshold and grace
  boundaries, activity resetting mid-warn, `threshold_s == 0` disabling,
  `warn > timeout` clamping, and monotonic-stamp wraparound (the `int32_t`
  difference idiom `sr_door_should_exit()` already uses for `g_deadline_ms`).
- A JS test for `parse_duration()`'s new argument covering bare numbers under
  both default units, each suffix, and the existing `activity_max_age`
  behavior remaining unchanged.
- Live verification on a node: confirm a paced-but-silent session warns and
  exits at the configured threshold, that a keypress during the warning clears
  it and resets the clock, and that an `EXEMPT H` user is never warned. Per
  house rule, this change is not committed on static reasoning alone.

## Follow-on work

- Adopt `termgfx/idle.{h,c}` in syncduke, syncdoom, syncscumm, syncconquer and
  syncmoo1 — one commit per door, each with its own `[idle]` ini section and
  its own warning render. Until then those five doors retain the current
  behavior, in which an idle user is bounded only by the `-t%T` deadline.
- Consider a SCFG helpbuf note on "Maximum Inactivity" recording that it is
  socket-based and therefore inert for doors that pace the terminal, so the
  next sysop does not set it and assume it works.
