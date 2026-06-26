#ifndef SYNCDUKE_STRINGS_SHIM_H_
#define SYNCDUKE_STRINGS_SHIM_H_

/*
 * Minimal <strings.h> shim for SyncDuke's MSVC build.
 *
 * The vendored engine (Engine/src/filesystem.c) includes <strings.h> for the
 * POSIX strcasecmp/strncasecmp. MSVC has no <strings.h>; it spells them
 * _stricmp/_strnicmp in <string.h>. Found via the compat/ include dir (same
 * pattern as compat/SDL.h) so we satisfy the vendored include without editing
 * the engine.
 *
 * On every other toolchain this header is also on the include path, so it must
 * be transparent: chain to the real <strings.h> with #include_next (a GCC/Clang
 * extension -- not needed, and not supported, under MSVC).
 */

#ifdef _MSC_VER
  #include <string.h>
  #ifndef strcasecmp
    #define strcasecmp  _stricmp
  #endif
  #ifndef strncasecmp
    #define strncasecmp _strnicmp
  #endif
#else
  #include_next <strings.h>
#endif

#endif /* SYNCDUKE_STRINGS_SHIM_H_ */
