# SyncRetro configuration consolidation — design

Date: 2026-08-02
Status: design approved; not yet implemented

## Problem

A SyncRetro console package — `xtrn/syncarcade`, `xtrn/syncnes`,
`xtrn/syncivision` — configures itself through **three different conventions**
and **four layers of defaults**. Every one of the three is a different answer to
the same question: which file is ours and which is the sysop's.

### Three conventions for one job

| file | shipped | sysop's | how the sysop's file comes to exist |
|---|---|---|---|
| `console.ini` | tracked | — | no override exists at all |
| `syncretro.ini` | `syncretro.example.ini` | `syncretro.ini` | `[copy:]` step in `install-xtrn.ini` |
| `games.ini` | tracked | `games.local.ini` | overlay, read second |

Only the third is right, and `GAMES_INI.md` §14 already argues why.

### The copied template goes stale

`[copy:syncretro.example.ini]` — `syncarcade/install-xtrn.ini:64`,
`syncnes:53`, `syncivision:56` — seeds `syncretro.ini` once. The result is a
**snapshot**: a key added upstream never reaches an existing install, and the
sysop never learns the key exists. The installer's prompt-before-overwrite is a symptom of that design, not
a fix for it — there is no way to deliver a new default without destroying the
sysop's edits.

`games.local.ini` has none of this problem precisely because it is an overlay:
it holds only what differs, so upstream's new titles keep arriving underneath.

### Four sources of defaults

| key | declared in | overridden by |
|---|---|---|
| `name`, `short`, `core`, `profile` | `lobby.js` spec | `console.ini` |
| `shared_saves` | `lobby.js` spec | nothing |
| `ext` | `lobby.js` spec | `syncretro.ini` `[roms] ext` |
| `min_size`, `max_size`, `bios` | `lobby.js` spec | nothing |
| audio / video / pace / idle / input / disc | C `iniGetX()` defaults | `syncretro.ini` |
| `profile: "pad"`, `shared_saves: false` | `syncretro_lib.js:346` | the `lobby.js` spec |

Four keys are written twice already. `syncretro_lobby.js:28-36` documents that
duplication as a deliberate fallback for an install with no `console.ini` — but
`console.ini` is tracked and therefore always present, so the fallback can only
fire in an install that is already broken. Its remaining effect is to be a second
place the truth can drift from.

`syncretro_lib.js:342` makes the argument against itself, about a different
duplication: *"a separate key is a thing to get out of step with the name beside
it."* A separate **file** is the same hazard.

### Consequence

Answering "what is this console's `profile`, and where do I change it?" means
reading a `.js` file, a `console.ini`, and a `syncretro.ini`, and knowing which
of the three wins. Sysop-facing reasoning — why the `arcade` profile exists when
it binds what `pad` binds, why `.fds` is excluded pending `disksys.rom`, the
measured romset size band — currently lives in a `.js` file sysops are told not
to touch.

## Goals

- One rule for shipped-vs-sysop config, and it is the rule the rest of the
  Synchronet tree already uses.
- A sysop's file holds only what differs and survives every upgrade; upstream
  defaults and newly added keys always arrive underneath it.
- One declaration of what a console **is**, readable by both halves of the door
  (the JS lobby and the native binary).
- No new vocabulary — no file-naming convention invented for this door.
- Unblock the per-user save/restore feature (separate spec), which needs a
  `[console] save_state` key and `shared_saves` readable by both halves and
  overridable by a sysop.

## Non-goals

- Changing `games.ini` / `games.local.ini`. Already the target shape.
- Per-host or per-platform config variants (`iniFileName()`'s other tiers).
- Migrating the other 27 xtrn doors off `.example.ini` / `.example.cfg`.
- Any save-state behavior. That is the follow-on spec.

## Design

### 1. One rule: shipped `X.ini`, sysop `X.local.ini`, overlaid key by key

- **`X.ini`** — ours. Shipped, tracked, fully documented, read **first**.
- **`X.local.ini`** — the sysop's. Untracked, read **second**, wins key by key.

This is the convention the repo root `.gitignore` already documents:

> A sysop's own copy of a configuration file Synchronet ships. `file_cfgname()`
> (C: `iniFileName()`) prefers `<name>.local.<ext>` over the plain
> `<name>.<ext>`, so customizations go in the `.local` file and survive the
> upgrade that replaces ours. Never tracked, anywhere in the tree.

One deliberate deviation. `iniFileName()` (`src/xpdev/ini_file.c:2298`)
**selects** a single file by precedence — `<name>.<host>.ini` >
`<name>.<platform>.ini` > `<name>.local.ini` > `<name>.ini`. SyncRetro
**layers** instead, as `games.local.ini` already does. Selecting reintroduces
exactly the staleness being removed here, and would regress `games.local.ini`
from overlay to replacement.

Layering is a strict superset of selecting: a sysop who copies the whole shipped
file into their `.local` gets replacement semantics, and one who writes only
their diffs gets something better. So the deviation cannot surprise a sysop who
arrived expecting the tree-wide behavior.

The mechanism already exists in this door and is reused verbatim —
`syncretro_games.c:126-145`: two `str_list_t`, `iniKeyExists(local, …)` tested
first, falling through to the shipped list. Its documented resolution rule
carries over unchanged: *a key present in the local file wins at the same scope;
specificity is decided before locality.*

### 2. Why not `.example.ini`, and why not a new `.default.ini`

`.example.ini` describes an **inert** file: never read at runtime, and the
sysop's copy is standalone. Once the shipped file is live — read first, on every
launch, with the sysop's overlaid on top — "example" is false, and a sysop who
believes it may delete the file.

A `.default.ini` naming would describe the new semantics accurately, and would
put the don't-edit-me signal in the filename where instinct meets it. It was
rejected because it is a **syncretro-only invention**: `*.example.ini` appears
32 times across 24 xtrn doors — 39 files across 28 doors counting the
`*.example.cfg` spelling — including `syncdoom`, `syncduke`, `syncmoo1`,
`syncalert`, `syncdawn`, `syncrpg`, `zzt` and the six `syncscumm` packages.
`.default.ini` appears nowhere in the tree, and would contradict the root
`.gitignore`'s `*.local.ini` rule.

The cost of that choice is real and should be stated: **a sysop's instinct is to
edit `syncretro.ini`**, which under this design is ours. Two git behaviors decide
whether that instinct is dangerous, and both were measured rather than assumed:

| the sysop's file is… | on `git pull` |
|---|---|
| **ignored**, and upstream starts tracking that name | silently overwritten, exit 0, no message |
| **tracked**, with local modifications | `Please commit your changes or stash them before you merge. Aborting`, exit 1, file preserved |

So the instinct misfires **loudly and non-destructively**, once, with a message —
and identically to how every other shipped Synchronet config behaves. That is a
better trade than a per-door vocabulary.

Two rules follow from the same table, and they are the load-bearing constraints
of this design:

1. The shipped file **must be tracked**.
2. A name that already exists in the field as a sysop's ignored file **must
   never become the shipped name** without the migration below.

Mitigation is documentation doing its job: the first three lines of the shipped
`syncretro.ini` say *edit `syncretro.local.ini`, not this file*, and each
package's `README.md` says it too.

### 3. The shipped `syncretro.ini` is the console's whole declaration

Everything console-specific moves into it, including what is currently only in
JS. The `[console]` section grows from four keys to nine:

| key | today | after |
|---|---|---|
| `name`, `short`, `core`, `profile` | `lobby.js` spec **and** `console.ini` | `syncretro.ini` `[console]`, once |
| `shared_saves` | `lobby.js` spec only | `syncretro.ini` `[console]` |
| `ext` | `lobby.js` spec, overridable via `[roms] ext` | `syncretro.ini` `[roms] ext` |
| `min_size`, `max_size`, `bios` | `lobby.js` spec only | `syncretro.ini` `[roms]` |
| `[video]`, `[audio]`, `[disc]`, `[idle]`, `[debug]`, `[input]`, `[options]` | `syncretro.example.ini`, copied | `syncretro.ini`, shipped |

Two things fall out for free. Keys that were previously unreachable from any
config file — `min_size`, `max_size`, `bios` — become sysop-overridable through
`syncretro.local.ini`, so a sysop with an unusual collection can widen a band
rather than file a bug. And the **comments** move with the values: the reasoning
now buried in `lobby.js` lands beside the key it explains, in the file a sysop is
already reading.

### 4. Readers

**Door** (`syncretro_config.c`). `sr_config_read_console_ini()` and
`sr_config_read_ini()` collapse into one two-layer read of
`syncretro.ini` + `syncretro.local.ini`, using the `sr_games_*` pattern. Sections
consumed by the door: `[console]`, `[video]`, `[audio]`, `[disc]`, `[idle]`,
`[debug]`, `[input]`, `[options]`.

`[options]` needs its own resolution rule, because it is read with
`iniGetNamedStringList()` rather than key by key. It **merges by key**, like
everything else: a name present in the local file wins, names present only in the
shipped file survive. A sysop therefore cannot *remove* a shipped core option,
only change its value — the escape being to set it explicitly to the core's own
default. This is a deliberate limit; expressing removal would need a sentinel
value and is not worth it.

**Lobby** (`syncretro_lobby.js`). The `console.ini` read at `:162` becomes the
same two-layer read, and the `[roms]` / `[lobby]` / `[text]` / `[idle]` reads at
`:213-223` join it instead of coming from a separate file. On the JS side the
overlay is `File.iniGetObject()` on both files with the local object's keys
copied over the shipped one. SpiderMonkey 1.8.5 applies: no `let`/`const`, no
arrow functions, no template literals.

### 5. `lobby.js` collapses

Every package's `lobby.js` becomes the same two lines, once
`syncretro_lobby()` defaults `dir` to `js.exec_dir`:

```js
load("syncretro_lobby.js");
syncretro_lobby();
```

It remains a per-package file because the xtrn registration
(`cmd = ?lobby`) needs an entry point per door.

### 6. The compiled floor stays, and is not a config layer

The `iniGetX()` default arguments in C and the `syncretro_console()` defaults at
`syncretro_lib.js:346` remain. They apply only to an install whose shipped file
is missing or unreadable — which, the file being tracked, means a damaged
install. The rule is stated once in the shipped file's header: **the shipped ini
is authoritative; the compiled values exist so a damaged install still starts.**

That leaves two sources of defaults instead of four, and only one of them is
ever consulted in a healthy install.

### 7. What this gives up

`console.ini` today makes it **structurally impossible** for an install to
redefine which console it is — there is no override file, by design, and its
header says so. After the merge, `[console] core` sits in a file the overlay can
reach.

Accepted rather than special-cased. The follow-on save-state work puts a
`save_state` key in that same section *specifically* to be sysop-overridable, so
a rule that exempts `[console]` from the overlay would have to carve an
exception immediately. A sysop who sets `core` gets what they asked for. The
shipped file's `[console]` header says which keys are facts about what we shipped
and which are claims a sysop may correct.

### 8. Installer and repo hygiene

- Delete `[copy:syncretro.example.ini]` from all three `install-xtrn.ini`.
- Delete `xtrn/*/syncretro.example.ini` and `xtrn/*/console.ini`.
- Delete the `/syncretro.ini` ignore line from all three `.gitignore` files — the
  name is tracked now. `*.local.ini` is already ignored tree-wide, so
  `syncretro.local.ini` needs no new rule.
- `xtrn/syncnes/.gitignore` and `xtrn/syncivision/.gitignore` contradict
  themselves today: a comment block states the live `syncretro.ini` "is
  intentionally NOT ignored here — leave it untracked so a sysop's edited config
  is visible in `git status`", and the last line of the same file ignores it.
  Both the comment and the line go.

## Migration

There are only a handful of live SyncRetro installs, and the design is not bent
to accommodate them — but the upgrade path has one destructive edge that must be
called out rather than discovered.

An existing install's `syncretro.ini` is untracked **and ignored**. The commit
that lands this both removes the ignore line and adds a tracked file of that
name, which is the first row of the §2 table: **silently overwritten, exit 0**.

So the instruction, in each package's `README.md`:

> Before pulling, rename your `xtrn/<console>/syncretro.ini` to
> `syncretro.local.ini`. It keeps working unchanged — a full config as an
> overlay simply wins on every key — and you can trim it to just your
> differences whenever you like.

A sysop who misses it loses their settings. With a handful of installs that is
acceptable; it would not be at a hundred.

## Testing

- **Door**: a two-layer read test mirroring `test_games.c`'s existing
  `games.local.ini` coverage (it already writes a local overlay into `LOCAL_DIR`
  and asserts key-by-key precedence at both root and section scope). Cases:
  local-only key, shipped-only key, local overriding shipped, `[options]`
  merge-by-name, and both files absent.
- **Lobby**: `jsexec` coverage of the JS overlay, alongside the existing
  `xtrn/syncivision/test_lobby_headless.js`.
- **Equivalence**: an install with no `.local` file behaves identically to one
  whose `.local` is a byte-for-byte copy of the shipped file. This is the
  property that makes the migration safe.
- **Damaged install**: with the shipped file absent, the door still starts on the
  compiled floor.

## Files touched

- `src/doors/syncretro/syncretro_config.c` — merge the two readers into one
  two-layer read
- `src/doors/syncretro/syncretro.h` — reader contract if it changes
- `src/doors/syncretro/test_config.c` (new) — overlay coverage
- `exec/load/syncretro_lobby.js` — two-layer read; `dir` defaults to
  `js.exec_dir`
- `exec/load/syncretro_lib.js` — `syncretro_console()` floor only
- `xtrn/{syncarcade,syncnes,syncivision}/lobby.js` — collapse to two lines
- `xtrn/{syncarcade,syncnes,syncivision}/syncretro.ini` — new, tracked, absorbing
  `console.ini` + `syncretro.example.ini` + the `lobby.js` spec and its comments
- `xtrn/{syncarcade,syncnes,syncivision}/console.ini` — deleted
- `xtrn/{syncarcade,syncnes,syncivision}/syncretro.example.ini` — deleted
- `xtrn/{syncarcade,syncnes,syncivision}/install-xtrn.ini` — drop `[copy:]`
- `xtrn/{syncarcade,syncnes,syncivision}/.gitignore` — drop `/syncretro.ini`, fix
  the self-contradiction
- `xtrn/{syncarcade,syncnes,syncivision}/README.md` — the one rule, and the
  migration line
- `src/doors/syncretro/GAMES_INI.md` — §14 restated in terms of the now-uniform
  rule

No `docs/v322_new.md` entry: SyncRetro did not ship in v3.21, so the release
notes cover its debut, not a change to a scheme no released version had.

## Follow-on

The per-user save/restore feature depends on this landing first. It adds a
`[console] save_state` key (shipped default per console, per-romset override in
`games.local.ini`) and reads `shared_saves` from the same section in both halves
of the door. Because that spec assumes the rule established here,
`console.local.ini` is never invented.
