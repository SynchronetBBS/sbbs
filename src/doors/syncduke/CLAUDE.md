# Coding style in this directory

This directory mixes our own code with a vendored 3rd-party snapshot. Which
style rules apply depends on which side of that line a file is on.

## Our code — house style (uncrustify)

Our files are everything at the top level of this directory: `syncduke_*.c`,
`syncduke.h`, `compat/`, `tests/`, the build systems (`CMakeLists.txt`,
`build*`, `build-msvc/`, `deploy.*`), and the `*.md` docs.

For any C changes to these files, follow the house style defined in
[../../uncrustify.cfg](../../uncrustify.cfg) — write to it from the start
(tabs, brace placement, spacing), and run uncrustify over the modified files
as the closing step of the change:

```
uncrustify -c ../../uncrustify.cfg --replace --no-backup <file>...
```

## Vendored code — keep upstream intact

`Engine/src/` and `Game/src/` are a **frozen, unmodified-where-possible
snapshot of Chocolate Duke3D** (provenance and pinned revision in
[SEAM.md](SEAM.md)). When a change has to touch these files:

- **Do NOT run uncrustify on them.** Reformatting a vendored file destroys
  its diffability against upstream.
- Keep the upstream code as intact as possible — make the smallest surgical
  edit that does the job, and match the style of the surrounding code in the
  block you're modifying.
- **Avoid "stacked commands"** (multiple statements crammed onto one line,
  e.g. `if (x) y = 1, z = 2;` or `a++; b++; c++;` on a single line) in code
  you add or modify — *unless* the specific block being edited already uses
  that style upstream (the Build engine code is often written that way).
  Stacked lines have caused `-Wmisleading-indentation` warnings here before;
  new code should be unstacked by default.
