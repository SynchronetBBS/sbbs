# Working in syncconquer (SyncConquer / syncalert door)

This directory mixes our own door code with a vendored Vanilla Conquer
snapshot. Which rules apply depends on which side of that line a file is on.

## Our code — house style (uncrustify)

Our files live under `door/` plus the top-level scaffolding (`CMakeLists.txt`,
`build.sh`, `deploy.sh`, the `*.md` docs). For any C/C++ change to these, follow
the house style in [../../uncrustify.cfg](../../uncrustify.cfg) — write to it
from the start (tabs, brace placement, spacing) and run it as the closing step:

```
uncrustify -c ../../uncrustify.cfg --replace --no-backup <file>...
```

## Vendored code — keep upstream intact

`vanilla/` is a frozen Vanilla Conquer subset (pinned revision + every local
patch tracked in [PROVENANCE.md](PROVENANCE.md)). When a change must touch it:

- **Do NOT run uncrustify on it** — reformatting destroys diffability against
  upstream.
- Make the smallest surgical edit and match the surrounding block's style.
- Record the edit in PROVENANCE.md (it is the authoritative patch list).

## Prefer xpdev — and follow SyncDuke / SyncDOOM

**Use xpdev wherever it's possible and practical**, rather than hand-rolling
platform portability. The door already links xpdev, so its wrappers are right
there. In particular:

- **Strings**: `stricmp` / `strnicmp` (genwrap.h maps them to
  `strcasecmp`/`strncasecmp` on POSIX) instead of `#ifdef _WIN32 _stricmp :
  strcasecmp` branches or hand-rolled case folding.
- **Sockets / networking** (incl. future multiplayer): `sockwrap.h`
  (`SOCKET`, `closesocket`, `ioctlsocket`, `socket_readable`, `send`/`recv`,
  `WSAStartup`) — one path for Windows and POSIX, no per-call `#ifdef`.
- **Threads / semaphores**: `threadwrap.h` / `semwrap.h` (already used by
  termgfx's async-music worker).
- **Files / dirs / time / ini**: `dirwrap.h`, `datewrap.h`, `ini_file.h`,
  `genwrap.h`'s `safe_snprintf`, etc.

**Follow the sibling doors — [../syncduke/](../syncduke/) and
[../syncdoom/](../syncdoom/) — as the reference for good practice** (their
sockwrap-based netgame, xpdev string/thread use, terminal handling, config
loading). This door was the odd one out (raw `#ifdef _WIN32` sockets and string
compares despite linking xpdev); prefer to converge on the siblings' approach.

**But don't cargo-cult a bad example**: if a sibling's practice turns out to be
wrong or worse than xpdev's own facility, fix the *sibling* (SyncDuke/SyncDOOM)
to match the better approach rather than copying the mistake here. The goal is
one consistent, xpdev-first set of doors — not three that diverge.
