# Coding style in this directory

This directory mixes our own code with vendored 3rd-party sources — and unlike
syncduke, the two are **intermixed in one flat directory**. Which style rules
apply depends on which side of that line a file is on.

## Which files are ours vs. vendored

**Ours:** `syncdoom.c`, `i_termsound.c`, `i_termmusic.c`, `mp_server.c`/`.h`,
`sd_splash.h`, `tests/`, `tools/`, the build systems (`CMakeLists.txt`,
`build*`, `build-msvc/`, `deploy.*`), and the `*.md` docs.

**Vendored:** everything else — the doomgeneric / Chocolate Doom sources
(`d_*.c`, `p_*.c`, `r_*.c`, `i_video.c`, `net_*.c`, etc.). When unsure, check
the file header: upstream files carry id Software / Simon Howard
(`Copyright(C) 2005-2014 Simon Howard`) copyrights; ours don't.

## Our code — house style (uncrustify)

For any C changes to our files, follow the house style defined in
[../../uncrustify.cfg](../../uncrustify.cfg) — write to it from the start
(tabs, brace placement, spacing), and run uncrustify over the modified files
as the closing step of the change:

```
uncrustify -c ../../uncrustify.cfg --replace --no-backup <file>...
```

## Vendored code — keep upstream intact

The doomgeneric / Chocolate Doom files are a frozen snapshot (see
[CREDITS](CREDITS)); we never track upstream. When a change has to touch them:

- **Do NOT run uncrustify on them.** Reformatting a vendored file destroys
  its diffability against upstream.
- Keep the upstream code as intact as possible — make the smallest surgical
  edit that does the job, and match the style of the surrounding code in the
  block you're modifying (Chocolate Doom's own style is spaces-indented,
  brace-on-own-line; don't impose house style on it).
- **Avoid "stacked commands"** (multiple statements crammed onto one line,
  e.g. `if (x) y = 1, z = 2;` or `a++; b++; c++;` on a single line) in code
  you add or modify — *unless* the specific block being edited already uses
  that style upstream. Stacked lines have caused `-Wmisleading-indentation`
  warnings here before; new code should be unstacked by default.
