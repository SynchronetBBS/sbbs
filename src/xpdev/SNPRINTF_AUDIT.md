# snprintf() in Synchronet: two meanings, and what to do about it

*Scoping note for a future work item. Nothing here has been changed; this
records what was found, why the obvious fix is wrong, and what a real fix
would have to do. Written 2026-07-21 while adding a command-line truncation
check to `sbbs_t::external()`.*

## The problem in one paragraph

`genwrap.h` redefines `snprintf` to xpdev's `safe_snprintf()` unless
`USE_SNPRINTF` is defined, and `safe_snprintf()` deliberately **clamps its
return value** to `size - 1` on truncation. C99 says `snprintf()` returns the
length the string *would* have been — which is the only thing that makes a
truncation check possible. So in most of Synchronet, the return value cannot
tell you that truncation happened, and never tells you how much was needed.
`USE_SNPRINTF` *is* defined for **darwin and freebsd** (`Common.gmake`), and
sbbs3 inherits it (`sbbs3/GNUmakefile:163` picks up `$(XPDEV-MT_CFLAGS)`).
The result: **the same line of code has different semantics per platform.**

| platform | `USE_SNPRINTF` | `snprintf` resolves to | return on truncation |
|---|---|---|---|
| macOS | yes | C library | would-be length (C99) |
| FreeBSD | yes | C library | would-be length (C99) |
| Linux | no | `safe_snprintf` | clamped to `size - 1` |
| Windows / MSVC | no | `safe_snprintf` | clamped to `size - 1` |

(On MSVC the second `genwrap.h` block would map `snprintf` to `_snprintf` —
the pre-C99 one that returns -1 and does not NUL-terminate — but it never
fires, because the first block has already defined `snprintf`.)

Confirmed by preprocessing a TU with sbbs3's include path: `snprintf(dst,
261, ...)` emits a call to `safe_snprintf`.

## Why the obvious fix is wrong

Making `safe_snprintf()` C99-conformant looks like a two-line change. It is
not, because **the clamp is load-bearing**. About 32 call sites accumulate on
the return value:

```c
/* genwrap.c:309, representative */
d += safe_snprintf(d, maxlen - (d - dst), "\\x%02X", (uchar)*s);
```

With a C99 return, `d` advances by the *would-be* length on truncation and
the next write is past the end of the buffer. Removing the clamp converts one
silent truncation into ~32 potential overruns. Any fix must handle these
first.

## Known damage

**One confirmed live bug**, `sbbs3/main.cpp:5626` (`#ifdef __unix__`):

```c
if ((unsigned int)snprintf(str, sizeof(uspy_addr.un.sun_path),
                           "%slocalspy%d.sock", startup->temp_dir, i)
    >= sizeof(uspy_addr.un.sun_path)) {
        lprintf(LOG_ERR, "Node %d !ERROR local spy socket path ... too long.");
        continue;
}
```

Correct on macOS/FreeBSD. **On Linux the condition is unreachable**: the
clamped return is at most `size - 1`, so an over-long `temp_dir` skips the
error and binds a silently truncated unix-domain socket path instead.

That is the worst property of this bug class — it is correct on a developer's
Mac and dead in production on Linux, with no warning at either compile or run
time.

**Not yet triaged:** roughly 125 sites consume an `snprintf`/`safe_snprintf`
return value tree-wide. Most are the accumulating pattern above. The
inventory below is the actual work.

Related but separate (does not involve `snprintf`): `sbbs_t::cmdstr()` sizes
its output by `sizeof(cmdstr_output)` even when writing into a caller-supplied
buffer, so `@TYPE:`/`@INCLUDE:` in `atcodes.cpp` can write up to 511 bytes into
a `char str[128]`. Fix separately; it is a stack overflow, not a truncation.

## Options

**A. Leave it, and never use the return value.** Measure inputs with
`strlen()` where a truncation check is needed. This is what `external()`
does, with a comment at both sites explaining why. Zero risk, but the trap
stays armed for the next person, and `main.cpp:5626` stays broken on Linux.

**B. Add a conformant helper, migrate deliberately.** Introduce e.g.

```c
/* C99 semantics: returns the length the result WOULD have been. */
int xp_snprintf(char *dst, size_t size, const char *fmt, ...);
```

leaving `safe_snprintf()` and its clamp exactly as they are. Truncation
checks move to the new function one at a time; accumulating callers are
untouched. Each migration is individually reviewable and individually
revertible.

**C. Make `safe_snprintf()` conformant and fix the ~32 accumulators.** The
"right" end state — `snprintf` would mean one thing everywhere — but it is a
single flag-day change across every accumulating call site, where a missed
one is a buffer overrun rather than a compile error. High risk for the
benefit.

**D. Define `USE_SNPRINTF` everywhere.** Tempting and wrong: on MSVC it
routes `snprintf` to `_snprintf`, which returns -1 on truncation and does not
NUL-terminate. That trades a clamped return for an unterminated buffer.

## Recommendation

**B**, with a first pass that is pure inventory and no behaviour change:

1. Classify all ~125 return-value uses into *accumulating* (needs the clamp),
   *truncation-checking* (needs C99, broken today on Linux/Windows), and
   *ignored*. Machine-greppable start:
   - accumulate: `+= *(safe_)?snprintf\(`
   - check: `snprintf\([^;]*\) *(>=|>|==|!=|<)`
2. Fix `main.cpp:5626` and any siblings the classification turns up — these
   are live bugs on Linux, independent of which option is chosen.
3. Add `xp_snprintf()` (or an explicit `xp_snprintf_needed()` that only
   returns the length) and migrate the checking sites.
4. Document the platform table above in `genwrap.h` beside the `#define`, so
   the divergence is discoverable from the place that causes it.

Steps 1 and 2 are worth doing even if the rest is never scheduled.

## Verification notes

- The divergence is not reproducible on one host. Any change here needs a
  compile *and* a run on both a `USE_SNPRINTF` platform (macOS/FreeBSD) and a
  non-`USE_SNPRINTF` one (Linux, Windows).
- `safe_snprintf()` has no unit test. A small one — exact fit, one over, size
  0, and the accumulating idiom — is cheap and would pin the clamp's
  behaviour before anyone changes it.
