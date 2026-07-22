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

- Every termgfx graphical door detects that its *user* has stopped providing
  input, on a signal that DSR pacing cannot forge.
- The threshold is sysop-configurable in all six doors, by command-line
  argument or by the door's own ini, with no dependency on Synchronet-side
  JS — these doors also run under other BBS software via DOOR32.SYS, and the
  feature must be configurable there too.
- Users exempt from inactivity timeouts by the BBS's own convention are exempt
  here too, in the doors whose launch path can determine that.
- A silent user who is still engaged — watching a cutscene, an FMV, a
  deliberately-running attract loop — gets a warning and a chance to stay,
  rather than being dropped mid-scene.
- The clock is written once and shared, not reimplemented per door.

## Non-goals

- **Hard disconnect from the door.** The door exits to the lobby or the BBS
  menu; hanging up is the BBS's job (see "Behavior and exit path").
- **ARS evaluation in C.** The door has no `scfg_t` and no user record.
  `termgfx/sbbs_node.c` deliberately avoids `load_cfg` (see its header note),
  and DOOR32.SYS carries no exempt flags — `termgfx_door32_t`
  (`termgfx/door32.h:43`) parses commtype, socket, stdio, time limit, node and
  alias, and not even the security level on line 8. Evaluating an ARS in the
  door would mean linking `userdat` and re-reading the user base. The lobby
  does it where a lobby exists; where none exists there is no exemption (see
  "Exemption asymmetry", which is an accepted consequence).
- **Fixing `max_inactivity` itself.** Teaching `input_thread()` to distinguish
  user input from terminal auto-replies means parsing terminal responses in a
  much more load-bearing component, and it would still be blind to a door that
  reads the socket directly via `-s<fd>`. Out of scope.
- **Giving the lobby-less doors a lobby.** syncscumm, syncconquer and syncmoo1
  are static `xtrn.ini` entries. Converting them is a much larger change with
  its own design; this spec configures them by argument and ini instead.

## Design

### The shared clock

New `termgfx/idle.{h,c}`, used by all six doors. Deliberately dumb: no I/O, no
config parsing, no rendering, and no clock of its own — it is fed a monotonic
millisecond stamp by its caller. That keeps it usable both by the doors that
own their I/O loop and by syncscumm, which goes through `termgfx_termio`,
without either restructuring, and makes it a pure function, unit-testable in
the existing `test_binds.c` / `test_dirty.c` / `test_quant.c` pattern.

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
is anything reaching a door's key/action dispatch. Explicitly *not* activity:
the terminal auto-replies every door's CSI dispatcher already handles as
distinct cases —

| Reply | CSI final | Source |
|---|---|---|
| Cursor-position report (pace-ack) | `R` | `termgfx/pace.h` frame pacing |
| Device attributes / CTDA | `c` | capability probe |
| XTSMGRAPHICS (sixel geometry) | `S` | capability probe |
| Window-op text-area size | `t` | geometry probe |
| kitty keyboard-flags query reply | `u` | key-mode negotiation |
| DECRPM | `y` | mode query |
| Audio DSR ack | `n` | `termgfx/audio_mgr.c` |

All six doors separate these two populations cleanly — verified per door. In
syncconquer the separation is structural: `door_csi_final()`
(`door/door_io.c:3067`) consumes every auto-reply and re-emits only
unrecognized bytes into the key parser (`:3199-3206`), so an auto-reply cannot
reach the key path even in principle.

**Mouse.** Mouse button, wheel and motion all count. Placement differs, and the
difference matters: only syncscumm/termio decodes motion into a queued event
(`TERMGFX_EV_MOUSE_MOVE`). The other doors treat motion as position-only state
and queue nothing, so their hook must go in the mouse **report handler**, not
the event queue. syncretro has no mouse support at all — it never enables
DECSET 1000/1006/1016 and never parses mouse reports — so it has no mouse hook.

**Door hotkeys.** Ctrl-S and friends are real keystrokes but bypass the game
key path in several doors. They count as activity; the per-door hook list below
includes them where they diverge.

The clock keeps running while a door screen, pause menu or help card is up — a
user who pauses and walks away is still idle — but keys arriving while
suspended still count.

### Per-door hook points

| Door | Activity hook(s) | Mouse hook | Exit check | Warning render |
|---|---|---|---|---|
| syncretro | `sr_key_apply()` `syncretro_input.c:341` | none (no mouse support) | `sr_door_should_exit()` `syncretro_door.c:112` | `sr_io_toast()` `syncretro_io.c:686` |
| syncduke | `press()` `:144`, `hold_press()` `:182`, `hold_release()` `:194`, `handle_key()` `:838` (all `syncduke_input.c`) | `syncduke_mouse_event()` `syncduke_input.c:599` | scattered: `_nextpage()` `syncduke_plat.c:285`, `syncduke_hangup()` `syncduke_door.c:60` | `banner_set()` `syncduke_node.c:28` |
| syncdoom | `key_seen()` `:1365`, `key_dispatch()` `:1743` (both `syncdoom.c`) | `mouse_seen()` `syncdoom.c:1438` | `check_hangup()` `:2172`, `check_timelimit()` `:2181` | `draw_page_overlay()` `syncdoom.c:1043` |
| syncscumm | `sst_push_event()` `termgfx_termio.c:3069`, plus hotkeys at `:1235`, `:3207`, `:996` | same (`sst_mouse_report()` `:3342`) | `pollEvent()` `syncscumm.cpp:310` — **no time limit exists** | **none — must be built** |
| syncconquer | `emit_key()` `door_input.c:111`, optionally `door_io_hotkey()` `door_io.c:1837` | `mouse_event()` `door_input.c:305` (motion queues nothing) | `door_check_time_limit()` `door_io.c:1288` (warn-only stub) | `banner_set()` `door_node.c:49` |
| syncmoo1 | four sites in `syncmoo1_input.c`: `:323`, `:346`, `:384`, `:402` | `sm_mouse_event()` `syncmoo1_input.c:111` | `sm_door_check_time()` `syncmoo1_door.c:458` | **none — must be built** |

Two doors grew parallel key dispatchers (legacy-byte vs native kitty/evdev) and
therefore need multi-site hooks: syncduke's `csi_final()` routes arrows and
F-keys straight past `handle_key()`, and syncdoom's `key_dispatch()` calls
`key_seen()` only for hotkeys. Hooking the deepest common point instead
(`rawq_push()`, `keyq_push()`) is rejected: both are also called by synthetic
key-up expiry timers, which would forge activity for a user who has left.

### Configuration

**Every door honors `-i<seconds>` and its own `[idle]` ini section.** That is
the base contract, and it is deliberately not conditional on a lobby: these
doors run under other BBS software too — Mystic and anything else that writes a
DOOR32.SYS — where no Synchronet JS exists at all. A non-Synchronet install
configures the timeout exactly the way a lobby-less Synchronet install does,
by ini or by argument.

The JS lobby is an **additional layer on top**, available only on Synchronet
and only for the three doors that have one, and its sole added value is
evaluating the exempt ARS.

#### The base contract (all six doors, every BBS)

```ini
[idle]
timeout    = 15m    ; 0 or absent = disabled
warn       = 60s    ; countdown before exit
```

`-i<seconds>` on the command line overrides the ini. `-i0` disables. This is
the whole configuration story for syncscumm, syncconquer and syncmoo1 on any
BBS, and for all six doors on a non-Synchronet BBS.

For syncscumm the ini is the practical route: its `cmd` is already near the
100-char `LEN_CMD` ceiling that `xtrn/syncscumm/install-xtrn.ini:22` warns
about, and that ini is **per-title** (one per game package, `startup_dir` per
entry — `/sbbs/ctrl/xtrn.ini:854`, `:868`, …), not per-door. A sysop
configuring idle timeouts for syncscumm sets them once per installed title.

#### The lobby layer (Synchronet only; syncretro, syncduke, syncdoom)

syncretro (`exec/load/syncretro_lobby.js:295`), syncduke
(`xtrn/syncduke/syncduke_lib.js:211`) and syncdoom (`xtrn/syncdoom/lobby.js:47`)
read the door's ini, evaluate an exempt ARS with `bbs.compare_ars()`, and append
`-i<seconds>` to the command line alongside the existing `-s%H -t%T`:

```ini
[idle]
timeout    = 15m         ; 0 or absent = disabled
warn       = 60s         ; countdown before exit
exempt_ars = EXEMPT H    ; evaluated by the lobby
```

`exempt_ars` defaults to `EXEMPT H`, matching `getnode.cpp:146`
(`useron.exempt & FLAG('H')` sets `CON_NO_INACT`), so an H-exempt user is
exempt here for the same reason they are exempt everywhere else. `EXEMPT` is an
ARS keyword taking a letter argument (`ars.c:466`); a bare `H` is not valid ARS.
Exempt users get an explicit `-i0` rather than an omitted flag, so "exempt"
positively overrides any ini default instead of silently falling through to it.

`exempt_ars` is read and acted on **only** by a lobby. The doors themselves
ignore the key entirely — on a lobby-less door, or any door on a non-Synchronet
BBS, there is nothing that can evaluate an ARS.

**Precedence, uniform everywhere:** `-i` wins when present; absent it, the door
reads `[idle] timeout` from its own ini. This mirrors `-t%T`. Because a lobby
always emits `-i` (including `-i0` for an exempt user), the lobby's answer wins
on a Synchronet install without the door needing to know a lobby exists.

An implementation consequence worth stating: each door must read its ini
**before** it arms the clock, so the ini fallback is populated when no `-i`
was given.

Per-door ini files and readers: `syncretro.ini` (`syncretro_config.c:156`),
`syncduke.ini` (`syncduke_config.c:218`), `syncdoom.ini` (`syncdoom.c:3238`),
`syncscumm.ini` (`termgfx_termio.c:1327` and `syncscumm.cpp:178`/`:256`),
`<argv0>.ini` i.e. `syncalert.ini`/`syncdawn.ini` (`door_io.c:791`),
`syncmoo1.ini` (`syncmoo1_config.c:127`). All use xpdev `iniGet*`.

#### Exemption asymmetry

An `EXEMPT H` user is exempt from the idle timeout in syncretro, syncduke and
syncdoom **on Synchronet**, and is **not** exempt in syncscumm, syncconquer and
syncmoo1 — nor in any of the six on a non-Synchronet BBS, where no ARS exists to
evaluate. This is a deliberate, accepted consequence of the exemption being a
lobby-layer feature, recorded here so it is a known asymmetry rather than a
surprise. Sysops without a lobby who need a door to leave a particular user
alone set `[idle] timeout = 0` for that door, which disables the feature
door-wide.

If this becomes a real complaint, the fix is to give those doors a launcher, or
to add a narrow exempt-flag read to termgfx — both larger changes than this one,
and neither is in scope here.

### The duration-parsing hazard

`game_lobby.js:531` already has `parse_duration()`, but its bare unit is
**days**, while xpdev's C `parse_duration()` — which the doors use for the same
key — treats a bare number as **seconds**. The same ini string would mean 15
minutes to the door and 900 days to the lobby.

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

At `timeout - warn`, the door paints a warning with a live countdown, and keeps
it refreshed as the countdown runs. Any real key clears the warning and resets
the clock — no menu, no confirmation, no penalty.

If the grace elapses, the door exits the way its existing time-limit path
already does. Control returns to the lobby where one exists, otherwise to the
BBS menu.

This composes with the layer below rather than duplicating it. Once the door
exits there is no more DSR pacing, so `socket_inactive` starts accumulating for
real and Synchronet's ordinary `max_session_inactivity` reaps the user from the
menu on its own. The door does not hang up the socket, and a user who returns
during the BBS's own grace can still do something else with the session.

The exit is logged to the per-node log the way existing deadline exits are, so
a sysop reading logs can distinguish an idle-reap from a quit or a time-limit
exit.

### Doors needing new machinery

Three doors need more than a hook, and these are the bulk of the work:

**syncscumm** has no session time limit at all: `termgfx_termio.c:618` reads
`d.time_limit_ms` from the drop file and discards it, and there is no `-t`
handling anywhere in termio or the glue. The deadline concept must be
introduced. It also has no transient overlay — only the Ctrl-S stats strip
(`sst_stats_draw()` `termgfx_termio.c:3025`, which early-returns when the
toggle is off). Both are built in `termgfx_termio.c` rather than the glue, so
syncrpg inherits them; `sst_bottom_row()` / `out_put()` /
`termgfx_termio_flush()` are the working precedent to copy.

**syncmoo1** has no overlay of any kind — `syncmoo1_io.c:372` says so
explicitly. The only on-screen text path is the pre-session splash
(`sm_door_splash()` `syncmoo1_door.c:260`), which the first present wipes. A
transient bottom-row primitive must be written, modeled on syncretro's
`sr_io_toast()`.

**syncduke** has no unified exit check; its time-limit exit lives in
`_nextpage()` (`syncduke_plat.c:285`) and carrier loss in `syncduke_hangup()`.
The idle check needs a per-present home consistent with the others.

By contrast **syncconquer** is the cheapest and the natural first
implementation: a documented single keyboard chokepoint, a timed banner, and a
`door_check_time_limit()` (`door_io.c:1288`) that is already a warn-only stub
whose own comment (`:1279-1283`) invites exactly this escalation.

syncretro's `sr_io_toast()` carries a fixed `SR_TOAST_MS` 1500 ms dwell, so its
countdown is driven by re-arming the toast each second rather than by a
long-lived banner.

### Suggested order

syncconquer (cheapest, proves the shared clock) → syncretro → syncmoo1 (needs
an overlay) → syncdoom → syncduke (multi-site hook, needs an exit-check home) →
syncscumm (needs both a deadline concept and an overlay, in shared code).

One commit per door, each independently testable, so a door that turns out
harder than surveyed does not block the rest.

## Testing

- `test_idle.c`, in the existing door unit-test pattern: threshold and grace
  boundaries, activity resetting mid-warn, `threshold_s == 0` disabling,
  `warn > timeout` clamping, and monotonic-stamp wraparound (the `int32_t`
  difference idiom `sr_door_should_exit()` already uses for `g_deadline_ms`).
- A JS test for `parse_duration()`'s new argument: bare numbers under both
  default units, each suffix, and `activity_max_age` behavior unchanged.
- Per door, a live session verifying that a paced-but-silent session warns and
  exits at the configured threshold, that a keypress during the warning clears
  it and resets the clock, and — in the three lobby doors — that an `EXEMPT H`
  user is never warned. Per house rule, no door's change is committed on static
  reasoning alone.
- Regression check that the synthetic key-up expiry timers in syncduke
  (`syncduke_input.c:173`) and syncdoom (`:1427`) do **not** register as
  activity; this is the specific failure mode that would make the feature
  silently inert.

## Follow-on work

- Consider a SCFG helpbuf note on "Maximum Inactivity" recording that it is
  socket-based and therefore inert for doors that pace the terminal, so the
  next sysop does not set it and assume it works.
- syncrpg inherits the termio work automatically; confirm when it lands.
