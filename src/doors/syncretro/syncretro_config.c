/* syncretro_config.c -- per-user sandbox + core/system/save/ROM dir resolution.
 *
 * Creates the -home sandbox and resolves the BIOS (system) + save dirs the core
 * asks for via retro_env.c's GET_SYSTEM_DIRECTORY / GET_SAVE_DIRECTORY. Mirrors
 * ../syncmoo1/syncmoo1_config.c (which pins 1oom's data + user paths the same
 * way). See DESIGN.md sec 10. Stub -- M1 fills these in.
 */
#include "syncretro.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static char g_system_dir[PATH_MAX];   /* BIOS: shared, read-only, per-install */
static char g_save_dir[PATH_MAX];     /* per-user SRAM + save states */

int sr_config_apply(void)
{
	/* TODO(M1):
	 *   - resolve the BIOS/system dir (per-install; door dir or a configured
	 *     path) into g_system_dir (absolutized -- the core keeps the pointer);
	 *   - mkpath() the -home sandbox (sr_door_home()), resolve it into
	 *     g_save_dir, and chdir into it (matching syncmoo1_config.c's
	 *     absolutize-before-anything-moves discipline);
	 *   - non-fatal on failure (log + continue), so a save-dir hiccup doesn't
	 *     kill the session. */
	return 0;
}

const char *sr_config_system_dir(void)
{
	return g_system_dir[0] ? g_system_dir : NULL;
}

const char *sr_config_save_dir(void)
{
	return g_save_dir[0] ? g_save_dir : NULL;
}
