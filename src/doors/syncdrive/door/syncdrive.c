/* syncdrive.c -- door entry point: Test Drive (1987) over termgfx.
 *
 * usage: syncdrive [DOOR32.SYS] [-s<fd>] [--game-dir=DIR] [--data-dir=DIR]
 *                  [--check]
 * termgfx_termio_init() consumes the DOOR32.SYS path and -s<fd>. --check runs
 * the TDEGA.EXE loader alone and exits (for getdata.js). */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"
#include "mem.h"

#include "door32.h"
#include "termgfx_termio.h"
#include "ini_file.h"

#include "host_term_ext.h"

int game_main(void);      /* tdport/src/game/flow.c */

static const char *opt_value(const char *arg, const char *name)
{
	size_t n = strlen(name);

	return strncmp(arg, name, n) == 0 ? arg + n : NULL;
}

static void read_ini(int *frame_rate)
{
	FILE *     f = fopen("syncdrive.ini", "r");
	str_list_t ini;
	char       val[INI_MAX_VALUE_LEN];

	if (f == NULL)
		return;
	ini = iniReadFile(f);
	fclose(f);
	iniGetString(ini, "input", "held_keys", "auto", val);
	host_set_held_keys_allowed(strcmp(val, "off") != 0);
	*frame_rate = iniGetInteger(ini, "game", "frame_rate", *frame_rate);
	strListFree(&ini);
}

int main(int argc, char **argv)
{
	const char *game = ".";
	const char *data = NULL;
	const char *v;
	const char *keys;
	bool        check      = false;
	int         frame_rate = 8;
	char        exe_path[1100];
	char        err[256];
	int         i;

	for (i = 1; i < argc; i++) {
		if ((v = opt_value(argv[i], "--game-dir=")) != NULL)
			game = v;
		else if ((v = opt_value(argv[i], "--data-dir=")) != NULL)
			data = v;
		else if (strcmp(argv[i], "--check") == 0)
			check = true;
	}
	snprintf(exe_path, sizeof exe_path, "%s/TDEGA.EXE", game);
	if (check) {
		if (!mem_load_exe(exe_path, err, sizeof err)) {
			fprintf(stderr, "%s\n", err);
			return 1;
		}
		printf("TDEGA.EXE ok: image %u bytes, DGROUP %04X\n", mem_image_size, DGROUP);
		return 0;
	}

	termgfx_termio_set_app_name("syncdrive");
	termgfx_termio_set_mouse(0);
	termgfx_termio_init(argc, argv);
	atexit(termgfx_termio_shutdown);

	for (i = 1; i < argc; i++) {
		termgfx_door32_t d;

		if (termgfx_door32_is_path(argv[i]) && termgfx_door32_read(argv[i], &d) == 0)
			host_set_player_name(d.alias);
	}
	read_ini(&frame_rate);
	host_set_data_dir(data != NULL ? data : game);
	if (!host_init(game, 1))
		return 1;
	host_set_frame_rate(frame_rate);
	if ((keys = getenv("SYNCDRIVE_KEYS")) != NULL)
		host_set_keyscript(keys);
	if (!mem_load_exe(exe_path, err, sizeof err))
		host_fatal("%s", err);
	return game_main();
}
