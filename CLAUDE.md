## Coding guidelines

This project's coding guidelines live in [CONTRIBUTING.md](CONTRIBUTING.md). Read it before making non-trivial code changes and follow the conventions it describes (style, formatting, commit messages, patch submission, etc.).

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
