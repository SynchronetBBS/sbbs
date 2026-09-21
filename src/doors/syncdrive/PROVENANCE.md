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

1. `src/game/flow_scores.c`: `scores_enter_name()` split into
   `scores_get_name()` and `scores_insert()`. With a BBS alias
   (`host_player_name()`), the name box shows the alias, which cannot be
   edited, flushes keys still queued from driving (`kbd_flush()`) so they
   cannot skip the screen instantly, and waits for a key or 3 seconds.
   `scores_get_name()` stops the qualifying jingle (`snd_stop_oneshot()`)
   before returning, so it always runs before the locked save. `high_scores()`
   takes the name first, then re-reads SCORES, inserts if the score still
   qualifies, and saves, all under `host_scores_lock()`, so concurrent
   sessions do not overwrite each other. Without an alias the original editor
   is used.
