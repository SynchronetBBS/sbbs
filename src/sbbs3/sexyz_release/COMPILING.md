# Building SEXYZ

SEXYZ is the Synchronet External X/Y/ZMODEM file transfer protocol driver. It
speaks XMODEM, XMODEM-CRC, XMODEM-1K, YMODEM, YMODEM-G and ZMODEM (including
ZedZap 8K blocks) over a TCP socket, a Telnet session, or stdin/stdout, which
makes it a drop-in replacement for the 16-bit DOS protocol drivers on a modern
BBS. `sexyz.txt` documents how to use it, and `sexyz_changes.md` lists what has
changed since the last release.

This archive is a stand-alone subset of the Synchronet source tree: the sexyz
sources plus the parts of Synchronet's portability library (`xpdev`) and hash
library that they actually need, flattened into one directory. Nothing else
from Synchronet is required to build it, and the resulting binary does not
require Synchronet to run.

## Requirements

**Unix-like systems** (Linux, FreeBSD, OpenBSD, NetBSD, macOS, Solaris):
GNU make and a C compiler (GCC or Clang). No configure step, no dependencies
beyond libc and pthreads.

**Windows:** Visual Studio 2015 or later, or the matching Build Tools. The
build uses only the C run-time and Winsock.

## Unix-like systems

```sh
make
```

That produces `sexyz` in the same directory. Other targets and options:

```sh
make DEBUG=1        # unoptimized, with symbols
make clean
make CC=clang       # override the compiler
make WARN=          # drop the GCC/Clang warning flags
```

If your make is not GNU make (notably on the BSDs, where `make` is BSD make),
invoke GNU make by name:

```sh
gmake
```

## Windows (MSVC)

From a Visual Studio Developer Command Prompt, in this directory:

```bat
nmake -f Makefile.vc
```

That produces `sexyz.exe`. The architecture follows whichever command prompt
you opened, so use the x64 Native Tools prompt for a 64-bit build and the x86
one for 32-bit. `nmake -f Makefile.vc DEBUG=1` builds with symbols, and
`nmake -f Makefile.vc clean` removes the output.

The makefile links the C run-time statically (`/MT`) so the resulting .exe
needs no redistributable. Change `/MT` to `/MD` if you would rather match
Synchronet's own project files.

Every `.c` file in this directory belongs to sexyz, and both makefiles build the
same 22 of them, so if you would rather not use a makefile at all, one command
does the job:

```bat
cl /nologo /O2 /MT /I. /DWIN32 /D_CONSOLE /DSBBS_EXPORTS ^
   /DRINGBUF_EVENT /DRINGBUF_MUTEX /DHAS_INTTYPES_H /DHAS_STDINT_H ^
   /DWINVER=0x600 /D_WIN32_WINNT=0x600 ^
   /D_CRT_SECURE_NO_DEPRECATE /D_CRT_NONSTDC_NO_DEPRECATE ^
   *.c /Fesexyz.exe ws2_32.lib iphlpapi.lib
```

Windows needs `iphlpapi.lib` alongside `ws2_32.lib`, because `netwrap.c`'s
`getNameServerList()` calls `GetAdaptersAddresses()`.

## Two sources carry their own platform guard

Both are in the list on both platforms, and each compiles to nothing where it is
not wanted, so no per-platform source list is needed:

- **`xpevent.c`** is the \*nix emulation of the Win32 `*Event` API. Its whole
  body is inside `#if defined(__unix__)`, so on Windows it yields an empty
  object and `eventwrap.h` maps `xpevent_t` straight to a `HANDLE` instead.
- **`xpprintf.c`** supplies the `vasprintf()` that `str_list.c` calls and the
  MSVC C run-time lacks. That definition is inside
  `#if defined(_MSC_VER) || defined(__MSVCRT__)`, so on Unix, where libc already
  has it, nothing conflicts.

## Preprocessor definitions that matter

Most of the definitions in the makefiles are ordinary platform plumbing, but
three are load-bearing and worth knowing if you build sexyz some other way:

- `RINGBUF_EVENT` and `RINGBUF_MUTEX` are **required**. sexyz runs the protocol
  on one thread and drains the output ring buffer on another, and it waits on
  the ring's events; without these the ring has neither and sexyz will not
  build.
- `PREFER_POLL` selects `poll()` over `select()` and is defined on Unix-like
  systems only. Do not define it on Windows: WSAPoll's semantics differ from
  poll()'s in ways that break the transfer (Synchronet GitLab issue #1212).

## Installing

There is no install target. Copy the binary wherever your BBS expects it. On
Synchronet that is the `exec` directory, and the protocol definitions in SCFG
invoke it from there. `sexyz.txt` covers the command line, the `sexyz.ini`
settings file, and the DSZLOG output.

Run `sexyz` with no arguments for usage, or `sexyz v` for the version of each
module in the build.

## Where this code comes from

These files are copied verbatim from the Synchronet source tree, so a fix made
here applies cleanly upstream and vice versa. The full tree, including the
Synchronet BBS itself and the MSBuild project files used for the official
Windows builds, lives at:

    https://gitlab.synchro.net/main/sbbs

The sexyz sources are in `src/sbbs3` (`sexyz.c`, `xmodem.c`, `zmodem.c`,
`ringbuf.c`, `nopen.c`, `date_str.c`, `telnet.c`), the portability library in
`src/xpdev`, and `crc16.c`/`crc32.c` in `src/hash`. `git_hash.h` and
`git_branch.h` record the commit this archive was cut from; in the full tree
they are generated at build time.

## License

Three licenses are represented here. The combination is distributable under
the GPL, which is the effective license of the resulting binary:

- **2-clause BSD**: `zmodem.c` and `zmodem.h`, copyright Rob Swindell and
  Stephen Hurd, with the original zmtx/zmrx code by Jacques Mattheij.
- **GNU General Public License, version 2 or later**: `sexyz.c`, `xmodem.c`,
  `ringbuf.c`, `nopen.c`, `date_str.c` and `telnet.c`.
- **GNU Lesser General Public License, version 2 or later**: the rest, which
  come from Synchronet's `xpdev` and `hash` libraries.

See <http://www.synchro.net/copyright.html> and the header at the top of each
source file.
