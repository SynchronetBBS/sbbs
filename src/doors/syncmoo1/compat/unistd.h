#ifndef SYNCMOO1_UNISTD_SHIM_H_
#define SYNCMOO1_UNISTD_SHIM_H_

/*
 * Minimal <unistd.h> shim for syncmoo1's MSVC build.
 *
 * Two callers need it, and MSVC ships no <unistd.h>:
 *
 *   - the vendored 1oom/src/game/game_save.c, which includes it for unlink();
 *   - our syncmoo1_config.c, which uses getcwd()/chdir() for the per-user
 *     -home sandbox.
 *
 * MSVC spells all of those with a leading underscore, but still declares the
 * POSIX names in <io.h>/<direct.h> under _CRT_INTERNAL_NONSTDC_NAMES (and
 * resolves them via oldnames.lib). So including those two headers is the whole
 * shim; CMakeLists.txt defines _CRT_NONSTDC_NO_DEPRECATE to quiet the
 * accompanying "use _unlink instead" warnings.
 *
 * Found via the compat/ include dir, which CMakeLists.txt puts on the search
 * path for MSVC only -- so the vendored 1oom/ tree stays unedited (CLAUDE.md's
 * ours-vs-vendored contract), and no other toolchain ever sees this file.
 *
 * Deliberately NOT a general POSIX emulation: no read()/write() (on Windows the
 * door's descriptor is a Winsock SOCKET, which the CRT's read/write cannot
 * touch -- see syncmoo1_plat.c), no fork/exec, no sysconf.
 */

#ifndef _MSC_VER
#error "compat/unistd.h is the MSVC-only shim; on other toolchains the real <unistd.h> must be used"
#endif

#include <direct.h>   /* getcwd, chdir, mkdir */
#include <io.h>       /* access, unlink */
#include <process.h>  /* _exit, getpid */

/* <io.h>'s _access() takes the same mode bits, but MSVC names no constants for
 * them. X_OK has no Windows meaning (every readable file is "executable" to
 * _access); mapping it to F_OK matches what MinGW's <io.h> does. */
#ifndef F_OK
#define F_OK 0
#endif
#ifndef X_OK
#define X_OK 0
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef R_OK
#define R_OK 4
#endif

#endif /* SYNCMOO1_UNISTD_SHIM_H_ */
