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
