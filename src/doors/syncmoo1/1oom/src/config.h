/* src/config.h.  Generated from config.h.in by configure.  */
/* src/config.h.in.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* Add debugging options. */
/* #undef FEATURE_MODEBUG */

/* Define to 1 if you have the <allegro.h> header file. */
/* #undef HAVE_ALLEGRO_H */

/* Define to 1 if you have the 'atexit' function. */
#define HAVE_ATEXIT 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the 'alleg' library (-lalleg). */
/* #undef HAVE_LIBALLEG */

/* Define to 1 if you have the 'SDL' library (-lSDL). */
/* #undef HAVE_LIBSDL */

/* Define to 1 if you have the 'SDL2' library (-lSDL2). */
/* #undef HAVE_LIBSDL2 */

/* readline available */
/* #undef HAVE_READLINE */

/* Define to 1 if you have the <readline/readline.h> header file. */
/* #undef HAVE_READLINE_READLINE_H */

/* Does the `readline' library support `rl_readline_name'? */
/* #undef HAVE_RLNAME */

/* Enable libsamplerate */
/* #undef HAVE_SAMPLERATE */

/* Enable SDL1 OpenGL */
/* #undef HAVE_SDL1GL */

/* Enable SDL_mixer */
/* #undef HAVE_SDL1MIXER */

/* Enable SDL2main replacement */
/* #undef HAVE_SDL2MAIN */

/* Enable SDL2_mixer */
/* #undef HAVE_SDL2MIXER */

/* Enable SDLmain replacement */
/* #undef HAVE_SDLMAIN */

/* Define to 1 if you have the <SDL.h> header file. */
/* #undef HAVE_SDL_H */

/* Define to 1 if you have the <stdbool.h> header file. */
#define HAVE_STDBOOL_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the 'strcasecmp' function. */
#define HAVE_STRCASECMP 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <winnt.h> header file. */
/* #undef HAVE_WINNT_H */

/* Define to 1 if you have the <wtypes.h> header file. */
/* #undef HAVE_WTYPES_H */

/* Compiling for MSDOS */
/* #undef IS_MSDOS */

/* Compiling for Windows */
/* #undef IS_WINDOWS */

/* Name of package */
#define PACKAGE "1oom"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "1oom"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "1oom 1.11.8"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "1oom"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.11.8"

/* Define to 1 if all of the C89 standard headers exist (not just the ones
   required in a freestanding environment). This macro is provided for
   backward compatibility; new code need not use it. */
#define STDC_HEADERS 1

/* Enable Allegro 4 HW support. */
/* #undef USE_HWALLEG4 */

/* Enable SDL1 HW support. */
/* #undef USE_HWSDL1 */

/* Enable SDL2 HW support. */
/* #undef USE_HWSDL2 */

/* Version number of package */
#define VERSION "1.11.8"

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif
