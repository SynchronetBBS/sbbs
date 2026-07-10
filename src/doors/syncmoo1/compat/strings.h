#ifndef SYNCMOO1_STRINGS_SHIM_H_
#define SYNCMOO1_STRINGS_SHIM_H_

/*
 * Minimal <strings.h> shim for syncmoo1's MSVC build.
 *
 * The vendored engine's 1oom/src/util.h includes <strings.h> unconditionally,
 * for the POSIX strcasecmp/strncasecmp -- and util.h is pulled in by nearly
 * every 1oom translation unit. MSVC has no <strings.h>; it spells those two
 * _stricmp/_strnicmp in <string.h>. Found via the compat/ include dir, which
 * CMakeLists.txt puts on the search path for MSVC only, so we satisfy the
 * vendored include without editing 1oom/ (see CLAUDE.md's ours-vs-vendored
 * contract). Same pattern as src/doors/syncduke/compat/strings.h.
 *
 * Note the frozen 1oom/src/config.h defines HAVE_STRCASECMP, so util.h/util.c
 * do NOT compile their own fallback strcasecmp() -- the macros below are what
 * the engine actually calls on Windows.
 *
 * MSVC-only by construction (the include dir is added only under MSVC), so
 * unlike syncduke's copy this needs no #include_next passthrough.
 */

#ifndef _MSC_VER
#error "compat/strings.h is the MSVC-only shim; on other toolchains the real <strings.h> must be used"
#endif

#include <string.h>

#ifndef strcasecmp
#define strcasecmp  _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif

#endif /* SYNCMOO1_STRINGS_SHIM_H_ */
