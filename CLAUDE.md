## Coding guidelines

This project's coding guidelines live in [CONTRIBUTING.md](CONTRIBUTING.md). Read it before making non-trivial code changes and follow the conventions it describes (style, formatting, commit messages, patch submission, etc.).

## Directory hierarchy — put files where they belong

Synchronet's install directory has a defined structure (documented at <https://wiki.synchro.net/dir:>). Each sub-directory has a specific purpose, and **what a file is determines where it goes** — getting this wrong (e.g. dropping a data file in `exec/`) is a real defect, not a stylistic nit. The defaults below are sub-directories of the install dir (`/sbbs`, `~/sbbs`, `c:\sbbs`) but are SCFG-configurable, so **code must locate them via `system.*_dir` (`system.ctrl_dir`, `system.data_dir`, `system.exec_dir`, `system.text_dir`, `system.mods_dir`) — never a hardcoded path.**

| Dir | `system.*` | Holds | Don't put here |
|-----|-----------|-------|----------------|
| `ctrl/` | `ctrl_dir` | **Configuration** for components that ship with Synchronet — `.ini`/`.cfg`/`.dat` config and curated/hand-edited data files (e.g. `text.dat`, `chat_llm.ini`, the curated `llm_external_archives.json`). The one place for "how this BBS is set up." | generated runtime state; scripts |
| `data/` | `data_dir` | **Generated run-time data** created during normal operation — message bases, user data (`data/user/`), logs, `*.json`/`*.jsonl` state, semaphore files, and generated indexes (e.g. the chat BM25 index `data/chat/guru.idx`). | config; scripts |
| `exec/` | `exec_dir` | **Executable & interpreted code** — `.exe`, Baja `.bin`/`.src`/`.inc`, and JavaScript `.js`. Not normally modified by the sysop. | data/config files of any kind |
| `exec/load/` | — | JS **libraries** `load()`ed by other modules (`sbbsdefs.js`, etc.). | standalone modules |
| `mods/` | `mods_dir` | Sysop-modified-stock or 3rd-party `.js`/`.bin` (and, since v3.21, `text`/`menu` files). **Shadows** `exec/` and `text/`: a file here is used instead of the same-named file there, surviving upgrades. | brand-new stock features |
| `text/` | `text_dir` | User-displayable text & menu files (`text/menu/`, `.asc`/`.ans`/`.msg`, avatars). | code; config |

Practical rule that has bitten this codebase: a curated/config data file (JSON included) goes in **`ctrl/`**, generated state goes in **`data/`**, and **`exec/` is for code only**. When adding a file or a path that reads/writes one, pick the directory by the file's role and address it with the matching `system.*_dir`.

## Segfaults are bugs — always investigate

Any segfault (or other crash: access violation, abort, stack overflow) of a Synchronet executable — `sbbs.dll`, `jsexec`, `sbbscon`, `mailsrvr`, `ftpsrvr`, `websrvr`, `services`, `ntsvcs`, `smbutil`, `chksmb`, `fixsmb`, `scfg`, `sbbsctrl`, `useredit`, `syncterm`, the SMB or xpdev test binaries, etc. — is a real defect and **must be root-caused**, not worked around or ignored. This applies whether the crash happens during normal use, in a test, or in a one-off probe (jsexec, a quick MsgBase script, a build's own self-test). "Just don't do that" / "retry / skip the offending input" is not an acceptable resolution.

Investigate the root cause: get a stack trace (debugger, crash dump, core file), identify the offending function and reason (null deref, use-after-free, bad cast, uninitialized memory, struct-size mismatch, ABI break, recursion, etc.), and fix it at the source. If a fix is out of scope for the immediate task, file/track it explicitly so it isn't lost.

## Don't create identifiers in the reserved namespace

The C and C++ standards reserve certain identifier forms for the implementation. User code (including macros, header guards, typedefs, function/variable names) must not use them:

- Any identifier beginning with an underscore followed by an uppercase letter (e.g. `_FOO`, `_RATELIMIT_HPP`).
- Any identifier containing a double underscore anywhere (e.g. `FOO__BAR`, `__foo`).
- At file scope, any identifier beginning with a single underscore (e.g. `_foo`).

This includes **header include guards**. Use the trailing-underscore form instead — e.g. `RATELIMIT_HPP_`, `SBBS_FOO_H_` — or any other form that doesn't start with an underscore or contain `__`. Much of the existing tree predates this rule and still uses leading-underscore guards; don't mass-rewrite, but never introduce a new one, and flip a header's guard to a conforming form whenever you're already editing it.

## Write portable C/C++ — the project builds with MSVC, GCC, and Clang

Synchronet's C/C++ sources have to compile cleanly under three main toolchains (MSVC on Windows, GCC and Clang on Unix-likes), plus to a lesser extent Borland C++ for the legacy `sbbsctrl` / `useredit` GUIs. MSVC tends to be the most permissive of the bunch; constructs MSVC accepts (with at most a warning) are often hard errors under GCC and Clang. A successful MSBuild compile on Windows is therefore **not** a full verification — it only proves the change isn't broken under the most lenient compiler.

The portability gotcha I've actually been bitten by:

- **`goto` jumping past a local variable's initialization** is a hard error under GCC and Clang (`error: jump to label 'X' ... crosses initialization of 'Y'`) whenever `Y` is still in scope at label `X`. MSVC accepts it. Fix by either (a) hoisting `Y`'s declaration above any `goto` that targets a label past it, or (b) wrapping `Y` in its own brace block so it falls out of scope before the label. Real-world pattern: an early-exit `goto cleanup;` near the top of a function that skips later `T var = init();` declarations whose label is at the bottom — happened in `chat.cpp`'s `chat_llm_session()` / `chat_llm_multinode_turn()` (introduced in commit `683147f9c`, fixed in `2a0ee8dd1`).

Other areas where MSVC is more forgiving than GCC/Clang:

- **Reaching the end of a non-void function** without a return — GCC/Clang error, MSVC warning.
- **Designated initializers** in C++20 — GCC/Clang require declaration order; MSVC accepts arbitrary order. Use declaration order for portability.
- **Narrowing conversions in braced-init lists** (`int x{some_long}`) — GCC/Clang error, MSVC warning.
- **`-Werror` is in effect on some build targets**, so a warning under one compiler can become an error elsewhere. Don't ignore warnings just because the build you ran passed.

Borland is even more conservative than MSVC on modern C/C++ features and largely affects only the two GUI sub-projects; if you're touching those, see `src/sbbs3/CLAUDE.md` and the `synchronet-build` skill's Borland section. For everything else, the practical rule is: write code that compiles cleanly under GCC/Clang first; MSVC will follow.

When you make a `.c`/`.cpp` change and can only verify it under MSVC, say so explicitly when offering the commit so the next compile under GCC/Clang isn't a surprise.

## Release notes — `docs/v3*_new*`

Each release has a "what's new" file in `docs/` (`docs/v321_new.txt`, `docs/v320_new.txt`, …, and the in-progress `docs/v322_new.md`). When recording changes for an upcoming release, update the file for that version (one greater than the last shipped release).

- **Match the existing `v3xx_new` files in style, category structure, and depth.** Group changes under the same kinds of headings the prior notes use, written for sysops/users — *what* changed and *why it matters* — not as a raw commit log.
- **Keep entries brief.** Concise, one-line-style bullets; no paragraphs or commit-message dumps.
- To find what belongs in the notes, review the merged changes on master since the previous release tag (git / GitLab history); `data/gitpush.jsonl` can help enumerate them.
