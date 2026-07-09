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

Version-control the **template**, never the live config a sysop edits. Track
`<name>.example.ini` and have the installer seed the live file from it:

```ini
[copy:<name>.example.ini]
dest = <name>.ini
```

`[copy:]` does not overwrite an existing `dest` without confirmation (it prompts,
and aborts under `--auto`), so a sysop's edited `<name>.ini` survives a
re-install. Leave the live `<name>.ini` **untracked** — don't `git add` it and
don't `.gitignore` it (the other `*.example.ini` doors all follow this). Keep the
config in **one place**: a second copy elsewhere in the tree (e.g. a sample
beside the C source under `src/`) silently drifts out of sync — a single combined
`xtrn/<dir>/<name>.example.ini` read by both the door binary and its JS lobby is
better than two half-configs. Point any docs at the one template.

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
