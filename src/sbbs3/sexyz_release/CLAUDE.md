## What this directory is

The packaging inputs for the stand-alone **sexyz source release**: a flat
archive that builds sexyz on its own, with no configure step and nothing else
from Synchronet. Nothing here is part of any Synchronet build. `mksexyzsrc.sh`
cuts the archives; `COMPILING.md` is what the recipient reads.

The **change log is not here**: it is `docs/sexyz_changes.md`, beside
`docs/sexyz.txt`, because it is user documentation rather than a packaging
input. The script copies both into the archive. Record a user-visible sexyz
change there as well as in the Synchronet release notes -- sexyz ships on its
own, to sysops who may not run Synchronet at all.

## The file set is derived, never hand-maintained

Cut a release with `./mksexyzsrc.sh <sbbs-checkout> <output-dir>`. It reads the
source list out of `GNUmakefile`, computes the header closure with `gcc -MM`,
pins `git_branch.h`/`git_hash.h` to HEAD, test-builds, and writes both
containers, named after `sexyz.c`'s own `revision` string.

**Never assemble an archive by hand or by patching a previous one.** Both have
been tried and both shipped a broken archive: one missing `xpprintf.c`
entirely, one with three stale sources. Adding a source means adding it
to `GNUmakefile`'s `SRCS` and `Makefile.vc`'s `OBJS`; the script does the rest.

Sources travel **verbatim**, which is what lets a fix made in the archive apply
upstream and lets anyone diff the two. Never edit a copy inside an archive: fix
it in the tree and re-cut.

## Both makefiles build the same list

The two platform-specific sources carry their own guards -- `xpevent.c` outside
`__unix__`, `xpprintf.c`'s `vasprintf` outside MSVC -- so each compiles to
nothing where it is not wanted and neither toolchain needs a list of its own.
Keep it that way; a per-platform source list is what the guards exist to avoid.

## A GCC link cannot tell you what MSVC needs

The first archive's file set came from a GCC link map, which by construction
only sees what the Unix build needs. glibc supplies `vasprintf`, so
`xpprintf.o` was never pulled and the file was omitted; the Windows link then
failed on it. Two more defects surfaced the same way, and only on Windows:
`xpevent.c` must not be compiled there, and `netwrap.c`'s
`GetAdaptersAddresses()` needs `iphlpapi.lib`.

**Verify a cut on both toolchains before releasing it.** Predicting MSVC from a
Linux host has produced three defects in this archive and zero correct calls.

## Settled decisions

- **Line endings stay LF**, except `FILE_ID.DIZ`, which is CRLF like the older
  sexyz archives. Synchronet's own `sbbs_src.zip` ships LF sources too, and
  converting would cost the byte-identical-to-tree property.
- **One file set, two containers** (`.zip` and `.tgz`), same contents, matching
  `sbbs_src.zip`/`sbbs_src.tgz`. Separate \*nix and Windows *file sets* were
  considered and rejected: the difference is two files, and two subsets would
  each be verified on one platform only.
- Three definitions are load-bearing and documented in `COMPILING.md`:
  `RINGBUF_EVENT` and `RINGBUF_MUTEX` are required, `PREFER_POLL` must not be
  defined on Windows (GitLab #1212), and `-I.` is needed because `md5.h` uses
  an angle-bracket include.

## Keep COMPILING.md's license section honest

The archive spans three licenses: `zmodem.c`/`zmodem.h` are 2-clause BSD, six
sources are GPL, and the rest are LGPL from `xpdev` and `hash`. It has been
wrong twice already. Re-check the headers of the files actually shipped when
any of them changes.
