/* syncretro_door.c -- DOOR32.SYS / -s<fd> / -name / -home / -t / -core setup.
 *
 * Resolves the client socket + session limits + paths from the drop file and
 * the door's own flags, BEFORE the core is loaded. Same contract as the other
 * doors (../syncmoo1/syncmoo1_door.c). Stub -- M1 fills these in.
 */
#include "syncretro.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int   g_sock = -1;
static char *g_name;
static char *g_home;
static char *g_core;
static char *g_rom;
static long  g_seconds;   /* session limit, 0 = none */

int sr_door_setup(int argc, char **argv)
{
	/* TODO(M1): parse argv:
	 *   - a bare path whose filename is door32.sys (case-insensitive) -> parse
	 *     DOOR32.SYS (line 2 = socket handle, line 9 = minutes left);
	 *   - -s<fd>  (digit-suffix only, so it never collides with a core's -s...);
	 *   - -t<seconds>, -name <handle>, -home <dir>, -core <path>, <rom>;
	 *   - -help/-?  -> print usage and exit(0).
	 * Dev fallback: SYNCRETRO_SOCK env, else socket -1 (stdout). Mirror
	 * ../syncmoo1/syncmoo1_door.c exactly. */
	(void)argc; (void)argv;
	return 0;
}

void sr_door_sanitize_argv(int *argc, char **argv)
{
	/* TODO(M1): remove the door's own args (drop file, -s/-t/-name/-home/-core,
	 * the ROM) so a core with its own arg parser never sees them. */
	(void)argc; (void)argv;
}

int         sr_door_socket(void)    { return g_sock; }
const char *sr_door_name(void)      { return g_name; }
const char *sr_door_home(void)      { return g_home; }
const char *sr_door_core_path(void) { return g_core; }
const char *sr_door_rom_path(void)  { return g_rom; }

int sr_door_should_exit(void)
{
	/* TODO(M1): true on carrier drop (socket EOF/error), session time expiry
	 * (g_seconds), or the quit hotkey. */
	(void)g_seconds;
	return 0;
}
