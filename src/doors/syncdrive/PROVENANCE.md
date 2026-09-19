# Provenance: tdport (Test Drive 1987, faithful C port)

- Upstream: https://github.com/kylofon/test-drive-sdl3
- Commit: a4d23077770fcc2cae81680fe4e3fd73163c0915 (2026-09-18)
- License: MIT (tdport/LICENSE), Copyright (c) 2026 Krzysztof Kania
- Copied: tdport/src/ (as tdport/src/), tdport/PORTING.md, FORMATS.md,
  LICENSE. Not copied: the reverse-engineering material (port/, tools/),
  which stays upstream.
- Omitted from tdport/src: host.c and main.c (SDL3). Replaced by
  door/host_term.c and door/syncdrive.c.
- Build shim: compat/SDL3/SDL.h maps the SDL calls in tdport/src/mem.c to
  libc, so mem.c compiles unedited.
- The game data (TDEGA.EXE, *.PES, ...) is not part of upstream or of this
  door; sysops supply it (xtrn/testdrive/getdata.js).

## Local patches

None yet.
