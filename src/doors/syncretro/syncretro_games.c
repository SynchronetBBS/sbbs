/* syncretro_games.c -- see syncretro_games.h. */
#include "syncretro_games.h"
#include "libretro.h"    /* RETRO_DEVICE_ID_JOYPAD_* */

#include <stdio.h>
#include <string.h>

#include "dirwrap.h"     /* xpdev: getfname() */
#include "ini_file.h"    /* xpdev: the reader both this and syncretro.ini use */

/* The RetroPad buttons a cabinet can label, and the spelling games.ini uses for
 * each. Only these six: the d-pad is the stick, and SELECT / START are the coin
 * slot and the start button, which the binds table already names correctly. */
static const struct {
	int id;
	const char *name;
} g_ids[] = {
	{ RETRO_DEVICE_ID_JOYPAD_B, "B" },
	{ RETRO_DEVICE_ID_JOYPAD_A, "A" },
	{ RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
	{ RETRO_DEVICE_ID_JOYPAD_X, "X" },
	{ RETRO_DEVICE_ID_JOYPAD_L, "L" },
	{ RETRO_DEVICE_ID_JOYPAD_R, "R" }
};

#define SR_GAMES_IDS ((int)(sizeof g_ids / sizeof g_ids[0]))

static char g_label[SR_GAMES_IDS][INI_MAX_VALUE_LEN];
static char g_stick2[INI_MAX_VALUE_LEN];
static int  g_labelled;

/* The romset name: the ROM's filename without directory or extension, which is
 * the section key. MAME identifies a driver by exactly this string, which is
 * why a romset cannot be renamed and why this file exists at all. */
static void sr_games_romset(const char *rom_path, char *out, size_t len)
{
	char *dot;

	out[0] = '\0';
	if (rom_path == NULL || *rom_path == '\0')
		return;
	snprintf(out, len, "%s", getfname(rom_path));
	dot = strrchr(out, '.');
	if (dot != NULL)
		*dot = '\0';
}

void sr_games_load(const char *dir, const char *rom_path)
{
	char       path[512];
	char       romset[128];
	char       key[64];
	char       val[INI_MAX_VALUE_LEN];
	str_list_t ini;
	str_list_t keys;
	FILE *     f;
	int        i;

	/* Clear FIRST and unconditionally: every early return below leaves a console
	 * with no file answering "nothing known" rather than inheriting the labels
	 * of whatever was loaded last. */
	for (i = 0; i < SR_GAMES_IDS; i++)
		g_label[i][0] = '\0';
	g_stick2[0] = '\0';
	g_labelled  = 0;

	sr_games_romset(rom_path, romset, sizeof romset);
	if (romset[0] == '\0')
		return;

	snprintf(path, sizeof path, "%s/games.ini", dir != NULL ? dir : ".");
	f = fopen(path, "r");
	if (f == NULL)
		return;                 /* no file is the normal case, not an error */
	ini = iniReadFile(f);
	fclose(f);
	if (ini == NULL)
		return;

	for (i = 0; i < SR_GAMES_IDS; i++) {
		snprintf(key, sizeof key, "button.%s", g_ids[i].name);
		iniGetString(ini, romset, key, "", val);
		if (val[0] != '\0') {
			snprintf(g_label[i], sizeof g_label[i], "%s", val);
			g_labelled = 1;
		}
	}
	iniGetString(ini, romset, "stick2", "", val);
	snprintf(g_stick2, sizeof g_stick2, "%s", val);

	/* A misspelt id is silent data loss otherwise: the sysop believes a button
	 * is labelled, the help screen omits it as unlabelled, and the player is
	 * told the cabinet has fewer controls than it has. Say so. */
	keys = iniGetKeyList(ini, romset);
	if (keys != NULL) {
		int k, j;

		for (k = 0; keys[k] != NULL; k++) {
			if (strncmp(keys[k], "button.", 7) != 0)
				continue;
			for (j = 0; j < SR_GAMES_IDS; j++)
				if (strcmp(keys[k] + 7, g_ids[j].name) == 0)
					break;
			if (j == SR_GAMES_IDS)
				fprintf(stderr, "syncretro: games.ini [%s]: \"%s\" is not a"
				        " RetroPad button (B A Y X L R) -- ignored\n",
				        romset, keys[k]);
		}
		iniFreeStringList(keys);
	}
	iniFreeStringList(ini);
}

const char *sr_games_button_label(int id)
{
	int i;

	for (i = 0; i < SR_GAMES_IDS; i++)
		if (g_ids[i].id == id)
			return g_label[i][0] != '\0' ? g_label[i] : NULL;
	return NULL;
}

const char *sr_games_stick2(void)
{
	return g_stick2[0] != '\0' ? g_stick2 : NULL;
}

int sr_games_labelled(void)
{
	return g_labelled;
}
