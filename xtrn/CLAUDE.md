# CLAUDE.md

Guidance for Claude Code when working in the Synchronet `xtrn/` tree (external
programs: doors, online games, editors). Each program lives in its own
`xtrn/<dir>/`. For JS engine constraints (SM 1.8.5 ↔ SM128), see
`exec/CLAUDE.md`.

## External Program Installers (`install-xtrn.ini`)

A program ships an `install-xtrn.ini` in its own `xtrn/<dir>/` so a sysop can
install it with `jsexec install-xtrn ../xtrn/<dir>` (or, from the terminal,
`;exec ?install-xtrn ../xtrn/<dir>`). `exec/install-xtrn.js` parses it, prompts,
writes the program into `ctrl/xtrn.ini`, and touches `recycle`. (Modern config
is `ctrl/xtrn.ini`; legacy `ctrl/xtrn.cnf`. There is no `xtrn.dab`.)

- **Root keys** (top of file, `Key: value`): `Name`, `Desc`, `By`, `Cats`,
  `Subs`, `Inst`.
- **Sections:** `[prog:<CODE>]`, `[event:<CODE>]`, `[editor:<CODE>]`,
  `[service:<proto>]`, plus action sections `[copy:<file>]`,
  `[ini:<file>[:section]]`, `[exec:<file>.js]`, `[eval:<expr>]` (and `pre-exec:`
  / `pre-eval:` variants that run before install). Per-section flow keys:
  `required`, `prompt` (set `false` to skip confirmation), `note`, `fail`,
  `last`, `done`.
- **Internal codes are 16 chars max** — `LEN_CODE` in `src/sbbs3/sbbsdefs.h`
  (32 for a library-qualified `lib:code`). The `install-xtrn.js` header comment
  saying "8 chars max" is **stale**; don't trust it.
- **`cmd = ?<module>`** for a JS door: a leading `?` runs a JS module, resolved
  against the program's **`startup_dir` first**, then `mods/`, then `exec/`
  (`js_execfile`, `src/sbbs3/exec.cpp:578`). `startup_dir` defaults to the
  `.ini`'s own directory, so a door at `xtrn/<dir>/<module>.js` just uses
  `?<module>` — no path needed. (A door's own "bare names resolve to exec/, not
  xtrn/" caveat only applies to `;exec` from the terminal, where there is no
  xtrn `startup_dir`.)
- `settings` / `type` / `event` values are `eval()`d against `sbbsdefs.js`
  (e.g. `settings = XTRN_MULTIUSER`). `execution_ars` / `ars` take an ARS string
  (e.g. `ANSI AND COLS 80`); omit them to allow all terminals.
- `[copy:]` / `[ini:]` source paths are relative to the `.ini`'s directory;
  `[ini:]` `values` are `eval()`d, so quote string literals.

### ⚠️ `[exec:]` runs **JavaScript only** — and a bad name silently kills the install

`[exec:<file>]` / `[pre-exec:<file>]` are handed to `js.exec()`, so the file
**must be `.js`**. Naming anything else (a `.sh` asset-fetcher, say) makes
`install_exec_cmd()` return the error string `Only '.js' files may be executed:
<file>` — and that check runs **before** the `prompt`/`required` handling, so
`required = false` does **not** make it skippable. It is an unconditional abort.

The damage is worse than it looks, because of *when* `[exec:]` runs. `install()`
builds the program list in memory (printing `<Desc> (<name>) installed
successfully` per program — a lie of timing), then runs the `[exec:]` sections,
and **only then** writes `ctrl/xtrn.ini`. The error string short-circuits the
return before that write, so **every program is discarded**. The console shows a
cheerful "installed successfully" followed by `Installed 0 external programs.`
and the door is simply absent from `ctrl/xtrn.ini`. If you see that pair, look at
your `[exec:]` sections, not at `[prog:]`.

So an install-time helper (fetch game data, download a binary) is a **`.js`
module**, never a shell script — which is also what makes it work on Windows.
See `xtrn/syncdoom/getwads.js`, `xtrn/syncduke/download.js`, and
`xtrn/syncalert/fetch-assets.js` for the house pattern: idempotent (skip what's
already present), streams the download to disk via `HTTPRequest.Download`,
unpacks with `Archive` (libarchive — reads ZIP, ISO9660, …), and is non-fatal so
a sysop with no internet can still install the door and supply the data by hand.
Pair it with `required = false`. The `javascript` skill covers `HTTPRequest` /
`Archive` / hashing, including cryptlib's rejection of legacy TLS certificate
chains (which is why some download hosts are unreachable from `HTTPRequest`
though `curl` fetches them fine).

### Shipping a default config: `<name>.example.ini` → `[copy:]`

Version-control the **template**, never the live config a sysop edits — a
git-tracked `.ini` a sysop is *meant* to edit (a JSONdb `host`/`port`, colour
scheme, game-balance knobs) leaves their edits showing forever as a dirty
working tree. Get this right when the door is **new**; see the warning below
before retrofitting it onto a door that already ships a tracked `.ini`. Three
parts, all required:

1. Track `<name>.example.ini`; the live `<name>.ini` is never added.
2. Seed the live file from the installer:

```ini
[copy:<name>.example.ini]
dest = <name>.ini
```

3. `.gitignore` the live file in the door's own `xtrn/<dir>/.gitignore`
   (create it if absent) so it doesn't show up as untracked noise:

```gitignore
# Live sysop-edited config, seeded from <name>.example.ini by install-xtrn.
/<name>.ini
```

If `dest` already exists, `[copy:]` asks before replacing it (`deny()`, so the
default answer is "no" and declining aborts the install); an unattended
(`-auto`) run isn't asked and keeps the existing file. Add `overwrite = true`
only if the template really should win every time — for a sysop-edited config it
shouldn't.

`[copy:]` sections are processed **before** `[ini:]` sections, regardless of
their order in the file — so a `[copy:]` that seeds `<name>.ini` and an
`[ini:<name>.ini]` that then customizes a key work together. Don't rely on the
reverse: an `[ini:<file>]` whose target doesn't exist in the door's
`startup_dir` silently falls back to writing `ctrl/<file>`.

Keep the config in **one place**: a second copy elsewhere in the tree (e.g. a
sample beside the C source under `src/`) silently drifts out of sync — a single
combined `xtrn/<dir>/<name>.example.ini` read by both the door binary and its JS
lobby is better than two half-configs. Point any docs at the one template.

`xtrn/synchess` predates the convention and names its template
`synchess-dist.ini`; it serves the same purpose, so leave it as-is rather than
renaming. A door with **no** `install-xtrn.ini` at all is unsupported — don't
invent one just to introduce this pattern.

#### ⚠️ Don't retrofit this onto a door whose `.ini` is already tracked

Renaming a tracked `<name>.ini` to `<name>.example.ini` is a delete + add, and
git **cannot untrack a file without deleting it from every working tree**. For a
sysop who has edited that file — i.e. exactly the sysop this convention is meant
to help — `git pull` refuses:

```
error: Your local changes to the following files would be overwritten by merge:
        xtrn/gooble/server.ini
Please commit your changes or stash them before you merge.
Aborting
```

That aborts the **entire pull**, so they can't take *any* Synchronet update until
they resolve it by hand. A sysop with an unedited copy gets no error at all —
git just deletes their config, and the door then fails (`starstocks/stars.js`,
`synchronetris/service.js` `return_error` on a missing `server.ini`) or silently
changes behaviour (`chess`, `seabattle` read its absence as "local-only"). And
`jsexec install-xtrn` with no argument won't repair it: it skips any door already
registered in `xtrn.ini` (`install-xtrn.js:550`).

This was attempted in `0aa214f366` across 24 doors and reverted the same day. It
is **not** fixable by anything shipped in the repo, because every in-repo remedy
is downstream of a pull the affected sysop cannot complete. Timing doesn't help
either — the same sysops break whenever it lands. If it's ever worth doing, it
needs an out-of-band announcement first (SYNC sub-board + IRC) telling sysops to
back up their door configs, a release cycle of lead time, and a `docs/v3*_new`
entry — not a quiet re-land.

Minimal door example:

```ini
Name: Synchronet Z-Machine
Desc: Play classic Infocom-style interactive fiction
By:   Author
Cats: Games
Subs: Adventure, Interactive Fiction

[prog:ZMACHINE]
cmd  = ?zmachine
settings = XTRN_MULTIUSER
required = true
```

## Environment variables SBBS sets for external programs

When the Terminal Server spawns an external program it exports these (see
`src/sbbs3/xtrn.cpp` — `setenv` on \*nix, the env list on Windows, and the DOS env
file for DOS doors). A **native door** uses them to locate Synchronet's directories
and identify its node **without hardcoding paths or adding command-line arguments** —
the C-door equivalent of the JS `system.*_dir` rule (repo-root `CLAUDE.md`, "put
files where they belong").

| Env var    | Value                                   | Use |
|------------|-----------------------------------------|-----|
| `SBBSCTRL` | ctrl directory                          | configuration (`text.dat`, `*.ini`) |
| `SBBSDATA` | data directory                          | generated/shared runtime state (in a per-program subdir, e.g. `data/syncduke/`) |
| `SBBSEXEC` | exec directory                          | bundled scripts/binaries |
| `SBBSNODE` | this node's **directory** (e.g. `.../node11/`) | per-node files; the node's `terminal.ini` lives here |
| `SBBSNNUM` | this node's **number**                  | per-node uniqueness (e.g. a `<name>.<node>.log` so two co-op nodes don't clobber one file) |

- `SBBSNODE` is a **path**; `SBBSNNUM` is a **number**. `termgfx/sbbs_node.c`'s
  `sbbs_my_node()` returns the number (reads `SBBSNNUM`, falls back to the trailing
  digits of `SBBSNODE`) — prefer it over a raw `getenv`.
- They're **absent for a dev/standalone run** (no BBS) — always provide a fallback
  (e.g. CWD) rather than assuming they're set.
- **Don't derive one dir from another** (e.g. `SBBSNODE/../data`): the dirs are
  SCFG-configurable and need not be siblings — use the specific variable.
