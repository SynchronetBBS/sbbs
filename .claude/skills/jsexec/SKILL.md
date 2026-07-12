---
name: jsexec
description: Use whenever you need to RUN Synchronet JavaScript — executing inline JS expressions, running scripts from the install's exec/ directory, testing JS modules, or validating changes to .js/.ssjs/.xjs files against a live install. This skill covers the jsexec runner itself: invocation modes, flags, output capture, crash tracing, and (on Windows) running a freshly-built debug binary. Trigger on "run a Synchronet script", "test this JS against Synchronet", "check what jsexec does", or any one-off JavaScript probe of a live install. For the JavaScript LANGUAGE and host API (how MsgBase/User/FileBase behave, SpiderMonkey dialect, the object model, writing tests, stock exec/*.js) see the javascript skill.
---

# Synchronet jsexec (the JavaScript runner)

`jsexec` is the standalone runner for Synchronet's JavaScript engine
(SpiderMonkey 1.8.5). It loads the full Synchronet runtime — config, message
bases, file bases, sockets, the `system`/`server`/`js` globals — without
requiring a BBS session. It's the fastest way to run a script, probe an object,
or validate a code change against a real install.

This skill is about **driving jsexec**. For the JavaScript dialect, the host
object model, and how the APIs behave (MsgBase, User, FileBase, …), see the
**`javascript`** skill. For low-level SMB file repair use
**`smbutils`**; to build a debug binary use **`synchronet-build`**.

**References:**
- Official jsexec docs: https://wiki.synchro.net/util:jsexec
- JS objects available in jsexec: https://nix.synchro.net/jsexecobjs.html

## Locating the install

Path conventions here are relative to the Synchronet install root. Use the
**`$SBBSCTRL`** environment variable to find a live install — it points to the
`ctrl/` subdirectory, and the install root is its parent:

```bash
SBBS="$(dirname "$SBBSCTRL")"        # install root
"<sbbs>/exec/jsexec"  ...             # binary
"<sbbs>/exec/foo.js"                  # a stock module
"<sbbs>/data/logs/"                   # log files land here
```

On a host where `<sbbs>/exec` isn't on `PATH`, prepend `"<sbbs>/"` or `cd "<sbbs>"`
first. To target a *different* install than `$SBBSCTRL` points at, use
`-c <other-ctrl-dir>`. (On Windows with a freshly-built debug binary there's an
extra wrinkle — see "Windows / debug-build invocation" below.)

## Two invocation modes

### Inline expression (`-r`)

Use `-r` for one-shot expressions. The flag is `-r`, **not** `-e`.

```bash
jsexec -r "print(system.version_notice);"
jsexec -r "var u = new User(1); print(u.alias);"
```

The expression runs as a single statement-list in the JS global scope.
Multi-statement code works — separate with semicolons. For anything beyond a few
lines, write a temp script instead; quoting hell is not worth it.

### Script file

```bash
jsexec path/to/script.js [args...]
jsexec -c "$SBBSCTRL" exec/foo.js
```

A bare `module[.js]` resolves to the Synchronet exec directory (`<sbbs>/exec`);
the `.js` extension is appended if omitted. To run a script elsewhere, pass an
absolute path or one containing `/`. Extra positional args are exposed to the
script as `argv`. The `-i` flag affects only `load()` lookups inside the script,
not how the script argument itself is resolved.

## Flags worth remembering

| Flag | Use when |
|------|----------|
| `-r <expr>` | inline JS — primary mode for probes |
| `-c <dir>` | point at a non-default CTRL dir (e.g. a test install's `ctrl/`) |
| `-U` | tolerate config load failures (probing a half-installed system) |
| `-C` | don't `chdir` into CTRL dir — keep relative paths working |
| `-q` | send all stdout (including `print()`) to /dev/null — rarely what you want |
| `-n` | suppress status messages |
| `-A` | merge stderr (status, `log()` output) into stdout — single capture stream |
| `-L <n>` | log level (default 6 = info; 4 = warning; 7 = debug) |
| `-D` | load into the interactive JS debugger |
| `-v` | print version + build info (and build **date** — handy for confirming a debug build) |

For clean, parseable output from an inline probe, use `-n` and `print()` what
you want. **Do not add `-q`** — it redirects all stdout (including `print()`) to
/dev/null, so you'll see nothing.

```bash
jsexec -n -r "print(JSON.stringify({v: system.version, nodes: system.node_list.length}));"
```

## What the jsexec environment provides (vs a BBS session)

jsexec runs *outside* a user session, so the global surface differs from what
scripts see under `bbs.exec()` / a logon flow. The classes themselves behave
identically; what changes is which globals exist. (Full class model: the
`javascript` skill and https://nix.synchro.net/jsexecobjs.html.)

**Available under jsexec:** `system`, `js`, `server`, `conio`; `msg_area`,
`file_area`, `xtrn_area`; `User`, `MsgBase`, `FileBase`, `File`, `Archive`,
`Queue`; `Socket` & friends, `MQTT`, `COM`; the `Crypt*` classes; `uifc`
(jsexec-only full-screen UI); and `env` (jsexec-only, below).

### `env` — the process environment (jsexec-only)

There is **no `getenv()`** anywhere in Synchronet's JS. jsexec instead defines a
global **`env` OBJECT**, keyed by variable name (`jsexec.cpp`):

```javascript
env.SBBSCTRL      // "/sbbs/ctrl"
env.PATH          // ...
for (var k in env) print(k + "=" + env[k]);
```

Two things to get right:

- **It is an object, not an array.** `env[0]` is `undefined` and `env.length`
  is `undefined`; iterate with `for (var k in env)`.
- **It does not exist in a BBS session** — a script run by `bbs.exec()` or a
  logon flow gets a `ReferenceError`. Anything meant to run in both contexts
  cannot use it.

**Prefer `system.*_dir` over `env` for install paths.** `system.ctrl_dir`,
`system.data_dir`, `system.exec_dir` &c. exist in *both* contexts and name the
install the script is actually running against — whereas `env.SBBSCTRL` only
reports whatever the launching shell happened to export, and is absent under the
BBS. Reach for `env` only for things that genuinely are environment (a caller's
own variable, `PATH`, a CI flag).

**A script may be run BOTH ways — detect the context, don't guess it.** This is
not hypothetical: `install-xtrn.js` runs an installer's `[exec:<file>.js]` steps
with **`js.exec()`**, i.e. *in the same process*, and `install-xtrn` itself is
invoked either from the command line (`jsexec install-xtrn ...`) **or from inside
the BBS** (`xtrn-setup.js`, the sysop's Auto-install menu). So a door's
`getcore.js` / `getwads.js` / `getdata.js` runs under whichever the sysop chose.

**The probe is `js.global.<name>`, not a bare identifier.** A bare `console` in a
jsexec run throws `ReferenceError`; `js.global.console` is an ordinary property
lookup and is simply `undefined`. That is why the stock code spells it that way:

```javascript
if (!js.global.bbs) {                       // exec/logonlist.js
    alert("This module must be run from the BBS");
    exit(1);
}
if (js.global.console !== undefined         // exec/load/gettext.js
    && typeof js.global.console.charset === 'string') { ... }
if (js.terminated || (js.global.console && console.aborted))   // install-xtrn.js itself
```

**Most I/O works in BOTH contexts** — `print()` is not the only option, and the
full table is at <https://wiki.synchro.net/custom:javascript#output>:

| | works in BBS **and** jsexec | BBS only | jsexec only |
|---|---|---|---|
| output | `print` `write` `writeln` `printf` `alert` `log` | `console.print`, `console.putmsg`, `console.*` | — |
| input | `read` `readln` | `console.getkey`, `console.getnum`, `console.*` | — |
| prompts | `prompt` `confirm` `deny` | — | `uifc` (full-screen UI) |
| other | `system` `js` `file_*` `argv`/`argc` `load` | `bbs`, `user` | `env`, `conio` |

So an installer child script has a real vocabulary — `confirm()` to ask, `alert()`
to warn, `prompt()` to collect, `print()` to narrate — all of which do the right
thing on a terminal *and* on a command line. Reach for `console.*` only after a
`js.global.console` check, for the things only a terminal can do (colour, hot
keys, `putmsg`).

**NOT available under jsexec** (session/terminal-bound — referencing them throws
`ReferenceError`):
- `bbs` — the per-user session object (menus, prompts, user-state mutators)
- `console` — the user's terminal I/O (`console.print`, `console.getkey`, …);
  use plain `print()` / `writeln()` / `log()` instead
- `client` — the connecting client's socket/identity
- `user` — the **current session's logged-on user** (lowercase, distinct from
  the `User` constructor class which IS available).  In the C++ codebase the
  same object is called `useron`; the JS binding renamed it to `user`.  Inside
  a BBS session this is the caller; under jsexec there's no session so the
  global is undefined.

Porting a BBS script to a jsexec probe: replace `console.print(x)` with
`print(x)` and strip any `bbs.*` interactivity. (Which output/input function maps
to which context is tabulated in the `javascript` skill.)

### Checking access against a specific user under jsexec

A common need in indexing / batch scripts is: *would user X be able to read
sub Y / download dir Z?*  The natural-looking accessors `sub.can_read`,
`dir.can_access`, `dir.can_download` evaluate against the **current session
user** — and under jsexec there isn't one, so they return `true` for
everything (no restrictions to compare against).  Likewise `user = new
User(N)` doesn't override the implicit session user — the global isn't
writable that way.

The reliable pattern is to instantiate a `User` and call its `compare_ars`
method directly against the ARS strings you find on the target object.
Walk the ownership chain because access is conjunctive (group ARS AND sub
ARS AND read-specific ARS, etc.):

```js
var u = new User(4);   // pick the user whose perms you want to check
function user_can_read_sub(u, sub) {
    var grp = msg_area.grp_list[sub.grp_index];
    return u.compare_ars(grp.ars     || '')
        && u.compare_ars(sub.ars     || '')
        && u.compare_ars(sub.read_ars || '');
}
function user_can_download_dir(u, dir) {
    var lib = file_area.lib_list[dir.lib_index];
    return u.compare_ars(lib.ars          || '')
        && u.compare_ars(dir.ars          || '')
        && u.compare_ars(dir.download_ars || '');
}
```

Note: it's `u.compare_ars(...)` on the User object itself, NOT
`u.security.compare_ars(...)` — `security` holds the level / flags / etc.
fields but the evaluator hangs off the User.

## A minimal probe

```bash
jsexec -n -r '
  var mb = new MsgBase("mail");
  if (!mb.open()) { print("open failed: " + mb.last_error); exit(1); }
  var hdrs = mb.get_all_msg_headers();
  for (var i in hdrs) { var h = hdrs[i]; print(h.number + " " + h.from + " -> " + h.to); }
  mb.close();
'
```

For the API details these probes exercise — and the **MsgBase
`get_all_msg_headers()` lazy-field gotcha** (`to_ext`/`from_ext`/etc. read
`undefined` unless you touch a non-NULL field first) — see the
`javascript` skill before writing anything that filters headers.

## Debugging crashes / step-tagged tracing

When a script crashes (SIGSEGV, abort, hang) and you need to find which call did
it, interleave `log("step N: <what>")` between candidate calls and run with `-A`:

```bash
jsexec -A -r '
  log("step1: ctor");  var mb = new MsgBase(path, true);
  log("step2: open");  mb.open();
  log("step3: save");  mb.save_msg({to:"x", to_ext:"1", from:"y", subject:"z"}, "body");
  log("step4: close"); mb.close();
  log("step5: done");
'
```

The last `step N` you see is the call before the crash; the next call is the
culprit. Use `log()` not `print()`: `print()` is stdout (line-buffered; a crash
can swallow the last line), while `log()` goes to stderr as a logged event and
flushes more aggressively, so it survives crashes that swallow `print()`. `-A`
merges stderr into stdout so you see `log()` inline.

Once narrowed to a call, switch to a C-side debugger or `printf`-trace in the
`.cpp` after a single-file rebuild (see `synchronet-build`). Per the project's
rules, a crash in any Synchronet binary is a real defect to root-cause, not work
around.

## When to use jsexec vs. other tools

- **Run JS against the live config / message bases / users** → jsexec. It's the
  only tool that gives you the real runtime.
- **Manipulate message-base files at the storage layer** → `smbutil` /
  `chksmb` / `fixsmb` (`smbutils`). Lower level, no JS.
- **Just syntax-check a script** → `jsexec -r 'load("path/to/file.js");'`
  compile-checks via load, but a real run is more informative.

## jsexec runtime constraints

- Default working directory after launch is the CTRL dir (`$SBBSCTRL`) unless
  `-C` is passed; relative paths resolve from there.
- Default CTRL dir is compile-time (typically what `$SBBSCTRL` points at); use
  `-c <ctrl-dir>` to target a different install.
- Heap defaults to 16 MB (`-m`) and time limit to 10 days (`-t`); adjust if a
  probe needs it.
- jsexec writes log messages to **stderr**, not to `<sbbs>/data/logs/`. Capture
  with `2>file`, merge into stdout with `-A`, or send to a file with `-e <file>`.
- The engine is SpiderMonkey 1.8.5 (ES3-ish). Language do's and don'ts are in
  the `javascript` skill.

## stdout / stderr / stdin are global File instances (with a gotcha)

jsexec exposes the three standard streams as global `File` objects: `stdin`,
`stdout`, `stderr`. They're created at startup via `js_CreateFileObject()` in
`src/sbbs3/jsexec.cpp:940-952` and have the full `File` method surface —
including `.flush()` (wraps `fflush()` on the underlying `FILE*`).

```js
stdout.writeln("hi");      // write via the File's FILE* buffer
stderr.writeln("warn");    // bypasses Synchronet's log() formatter
stdout.flush();            // fflush() the File's FILE*
```

**Why this matters: `print()` is block-buffered when stdout is redirected.**
When stdout goes to a TTY, C stdio uses line buffering and you see output
as it's emitted. When stdout is redirected to a file or pipe (`jsexec foo.js
> out.log`), C stdio switches to *block* buffering (~4KB chunks), and
`print()` calls accumulate in the buffer until either the buffer fills or
the process exits. A long-running script can look completely stalled in
`tail -f` even though it's making progress.

### Gotcha: `stdout.flush()` does NOT flush `print()`

The JS `stdout` File object wraps a **separately dup()'d FILE\***, not libc's
stdout FILE\*. From `js_CreateFileObject()` in `js_file.cpp`:

```c
int newfd = dup(fd);           // duplicate the fd
fp = fdopen(newfd, mode);      // open a NEW FILE* on the duped fd
p->fp = fp;                    // File object's private FILE*
```

So `print()` writes through libc's `stdout` FILE\* (one buffer), and
`stdout.writeln()` / `stdout.flush()` operate on the JS File's FILE\*
(a different buffer that just happens to point at the same fd). Calling
`stdout.flush()` after `print()` flushes nothing useful — `print()`'s
data is still sitting in libc's buffer.

**To make output watchable via `tail -f`, route ALL output through
`stdout.writeln()` and flush the same File:**

```js
/* Make `print` route to the JS File so flushes are effective. */
print = function (s) { stdout.writeln(String(s == null ? '' : s)); };

for (var i = 0; i < cases.length; i++) {
    run_one_case(cases[i]);
    print('---- case ' + i + ' done ----');
    stdout.flush();          // NOW tail -f sees it
}
```

Don't mix `print()` and `stdout.writeln()` in the same script if you care
about real-time visibility — output from each goes into a different buffer
with no ordering guarantee in the receiving file. Replace `print` globally
(as above) and you're consistent.

`log(LOG_INFO, msg)` is an alternative for crash-survivable trace output —
it goes to stderr via Synchronet's log facility which flushes more
aggressively per message. Side effects: `log()` output is formatted
(timestamp prefix, log-level tag) and goes to stderr (capture with `-A` or
`-e <file>`).

### Interactive input: use `prompt()`, not a raw `stdin` read

For a script that reads lines typed at an **interactive terminal**, use the
global **`prompt(label)`** — NOT `stdin.readln()` / `new File('/dev/stdin')`.
jsexec puts the controlling terminal into raw / no-echo mode for its own
console handling, so a raw stdin read consumes keystrokes **without echoing
them and without line-editing** — the user ends up typing blind. `prompt()`
manages the terminal itself (echo + line edit), prints `label` followed by
`": "`, and returns:

- the entered string,
- `""` on a blank line,
- `null` on EOF (Ctrl-D) — use that to terminate a read loop.

```js
var line;
while ((line = prompt('query')) !== null) {   // echoes; null on Ctrl-D
    line = line.replace(/^\s+|\s+$/g, '');
    if (!line) continue;                       // blank line
    if (line === '/quit') break;
    handle(line);
}
```

This only matters for a real TTY. Reading **piped / redirected** input
(`echo … | jsexec foo.js`, `jsexec foo.js < input`) has no terminal and no
echo concern, so reading lines off the `stdin` File is fine there.

## Pitfalls

- `-e` is **not** the inline-expression flag — that's `-r`. `-e<filename>` means
  "send error messages to file."
- Quoting: bash single-quotes beat escaping double-quotes inside `-r`. If the
  expression itself contains single quotes, write a temp `.js`.
- An inline `exit(N)` returns N as the process exit status — handy for pipelines.
- Running as a non-`sbbs` user can create log/data files with wrong ownership;
  prefer `sudo -u sbbs jsexec ...` or stick to read-only probes.

## Windows / debug-build invocation

On Windows, the **installed** `jsexec.exe` (under `<install>\exec\`, typically
a release build that shipped with the install) is frequently **stale** relative
to your source tree. **For development/testing, prefer the freshly-built binary
under `<sbbs-src>\src\sbbs3\msvc.win32.exe.<config>\`** over the installed one
when you're validating recent source changes.

MSBuild puts its outputs in per-config sibling directories under `src/sbbs3/`:

- `<sbbs-src>\src\sbbs3\msvc.win32.exe.debug\jsexec.exe` (and the other utility
  exes)
- `<sbbs-src>\src\sbbs3\msvc.win32.dll.debug\sbbs.dll` (the runtime — where the
  `MsgBase`/`User`/etc. bindings live)
- Same layout for the `.release` configurations.

These are **not** copied into `<install>\exec\`. The key gotcha: Windows' DLL
search order checks the **current directory before PATH**, so to load the
freshly-built `sbbs.dll` the **dll output dir must be the cwd (or on PATH)** —
otherwise `jsexec.exe` falls through to whichever `sbbs.dll` is on PATH or
under the install, and your C++ changes have no effect. Run the exe **by
absolute path from the dll dir, in a single command** (the shell may reset
cwd between calls):

```bash
# adjust drive letters and paths to your actual layout
cd <sbbs-src>/src/sbbs3/msvc.win32.dll.debug && \
  <sbbs-src>/src/sbbs3/msvc.win32.exe.debug/jsexec.exe \
    -c <install>/ctrl/ -n -r 'print(system.version + system.revision);'
```

Confirm which DLL you loaded — print the build banner/date, or `md5sum sbbs.dll`
in the cwd and compare against `msvc.win32.dll.debug\sbbs.dll`. If results look
wrong (e.g. a recent C++ fix seems absent), suspect a stale/PATH `sbbs.dll`
before doubting the code. (A debug `sbbs.dll` is locked while loaded — see the
next section.)

### When the live BBS is running the build you want to rebuild

A common situation: the live BBS on the same machine is running the **debug**
build (or whichever build you're trying to rebuild), and you want to
instrument that same binary or test a candidate C++ fix. The Windows file
system locks `sbbs.dll` and the running exes while the BBS holds them open —
`link` fails with `LNK1104` / a sharing violation — so you can't overwrite
them in place. Options, in increasing disruptiveness:

1. **Switch to the other configuration.** If the live BBS is on debug, build
   and run the **release** variant instead, and vice versa. Both layouts live
   side-by-side under `src/sbbs3/msvc.win32.{exe,dll}.{debug,release}\` — pick
   the one that's *not* currently loaded by the live server:
   ```bash
   cd <sbbs-src>/src/sbbs3/msvc.win32.dll.release && \
     <sbbs-src>/src/sbbs3/msvc.win32.exe.release/jsexec.exe \
       -c <install>/ctrl/ -r '...'
   ```
   Build release with `build.bat /p:Configuration=Release` (or `release.bat`)
   — see `synchronet-build`. This is the right default when your probe is
   read-only and you don't need debug symbols on the running BBS's binary.

2. **Build in an isolated git worktree.** Same source revision (or a variant),
   but its own `msvc.win32.*` output dirs that the running BBS doesn't hold
   open. See `synchronet-build` for the worktree recipe.

3. **Stop the BBS briefly, rebuild, restart.** Last resort — affects users.
   For coordinated downtime see `control` (`ctrl/recycle` /
   `ctrl/shutdown` semfiles, MQTT `host/+/pause` for graceful drains).

**Don't retry-loop a debug rebuild against the lock**, and don't reach for
stopping the live BBS just to compile.

### `total_text (X) != TOTAL_TEXT (Y)` at startup

This means the running `sbbs.dll` and the calling exe (or the install's
`ctrl/text.dat`) were built from different `text.h` / `text_defaults.c`.
Either rebuild both halves from the same tree, or point `-c` at a `ctrl/`
whose `text.dat` matches the binary's compile-time `TOTAL_TEXT`. `-U` does
**not** reliably mask this check. See `synchronet-build`.

### Other Windows notes

- **Binary name from Git Bash / POSIX shells**: invoke `jsexec.exe`, not bare
  `jsexec` (same for `smbutil.exe`, `chksmb.exe`, …).
- **Path separators**: forward slashes work in arguments — `-c <install>/ctrl/`
  is fine on Windows.
