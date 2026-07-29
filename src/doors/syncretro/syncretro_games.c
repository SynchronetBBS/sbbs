/* syncretro_games.c -- see syncretro_games.h. */
#include "syncretro_games.h"
#include "libretro.h"    /* RETRO_DEVICE_ID_JOYPAD_* */

#include <stdio.h>
#include <string.h>

#include "dirwrap.h"     /* xpdev: getfname() */
#include "genwrap.h"     /* xpdev: strnicmp()/stricmp() -- ini keys fold case */
#include "ini_file.h"    /* xpdev: the reader both this and syncretro.ini use */

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

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
static int  g_boot_frames;

/* A ceiling on boot_frames, in frames. Warming up costs the host real CPU --
 * it is emulation with the pacing taken off, not a shortcut -- and it happens
 * with the player watching a blank screen, so a slipped digit must not turn a
 * launch into a stall on every node at once. Five emulated minutes is far past
 * any cabinet's self-test. */
#define SR_BOOT_FRAMES_MAX 18000

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

/* Clamp a boot_frames value and say so when it was out of range: a warm-up the
 * sysop asked for and did not get is otherwise indistinguishable from one the
 * reader never saw. `where` names the section for the message. */
static int sr_games_clamp_boot(int frames, const char *where)
{
	if (frames < 0) {
		fprintf(stderr, "syncretro: games.ini [%s]: boot_frames %d is negative"
		        " -- treated as 0 (no warm-up)\n", where, frames);
		return 0;
	}
	if (frames > SR_BOOT_FRAMES_MAX) {
		fprintf(stderr, "syncretro: games.ini [%s]: boot_frames %d exceeds the"
		        " %d-frame maximum -- clamped\n", where, frames,
		        SR_BOOT_FRAMES_MAX);
		return SR_BOOT_FRAMES_MAX;
	}
	return frames;
}

/* Read one ini beside the door, or NULL if it is not there. Absence is normal
 * for both files: games.ini for a cartridge console, games.local.ini for an
 * install whose sysop has changed nothing. */
static str_list_t sr_games_read(const char *dir, const char *name)
{
	char       path[PATH_MAX];
	str_list_t ini;
	FILE *     f;

	snprintf(path, sizeof path, "%s/%s", dir, name);
	f = fopen(path, "r");
	if (f == NULL)
		return NULL;
	ini = iniReadFile(f);
	fclose(f);
	return ini;
}

/* The two files resolve as if games.local.ini's lines were APPENDED to
 * games.ini: a key present in the local file wins at the SAME SCOPE. So a local
 * [sf2] overrides the shipped [sf2], and a local root key overrides the shipped
 * root key -- but a local ROOT key does not reach past a shipped SECTION key,
 * because specificity is decided before locality. A sysop who means to change
 * one cabinet writes that cabinet's section, exactly as they would in a single
 * file. See GAMES_INI.md sec 14.
 *
 * Either list may be NULL. Returns 1 and fills `val` when the key was found. */
static int sr_games_str(str_list_t base, str_list_t local, const char *section,
                        const char *key, char *val)
{
	if (local != NULL && iniKeyExists(local, section, key)) {
		iniGetString(local, section, key, "", val);
		return 1;
	}
	if (base != NULL && iniKeyExists(base, section, key)) {
		iniGetString(base, section, key, "", val);
		return 1;
	}
	val[0] = '\0';
	return 0;
}

static int sr_games_int(str_list_t base, str_list_t local, const char *section,
                        const char *key, int def)
{
	if (local != NULL && iniKeyExists(local, section, key))
		return iniGetInteger(local, section, key, def);
	if (base != NULL && iniKeyExists(base, section, key))
		return iniGetInteger(base, section, key, def);
	return def;
}

/* Warn about a key no reader honours, in whichever file carries it. Split out
 * so the local file gets the same scrutiny as the shipped one -- a typo is far
 * likelier in the file a sysop just hand-edited. */
static void sr_games_check_keys(str_list_t ini, const char *romset,
                                const char *fname)
{
	str_list_t keys;
	int        k, j;

	if (ini == NULL)
		return;
	keys = iniGetKeyList(ini, romset);
	if (keys == NULL)
		return;
	for (k = 0; keys[k] != NULL; k++) {
		/* iniGetString() (and every other xpdev ini lookup) matches keys
		 * case-insensitively (ini_file.c uses stricmp()), so a case-sensitive
		 * check here would flag a key the reader honours -- "button.y" reads
		 * fine and this validator called it invalid. */
		if (strnicmp(keys[k], "button.", 7) != 0)
			continue;
		for (j = 0; j < SR_GAMES_IDS; j++)
			if (stricmp(keys[k] + 7, g_ids[j].name) == 0)
				break;
		if (j == SR_GAMES_IDS)
			fprintf(stderr, "syncretro: %s [%s]: \"%s\" is not a RetroPad button"
			        " (B A Y X L R) -- ignored\n", fname, romset, keys[k]);
	}
	iniFreeStringList(keys);
}

void sr_games_load(const char *dir, const char *rom_path)
{
	char        romset[128];
	char        key[64];
	char        val[INI_MAX_VALUE_LEN];
	str_list_t  ini;
	str_list_t  local;
	int         i;
	int         buttons;
	int         rootboot;
	const char *reportdir;

	/* Clear FIRST and unconditionally: every early return below leaves a console
	 * with no file answering "nothing known" rather than inheriting the labels
	 * of whatever was loaded last. */
	for (i = 0; i < SR_GAMES_IDS; i++)
		g_label[i][0] = '\0';
	g_stick2[0]   = '\0';
	g_labelled    = 0;
	g_boot_frames = 0;
	reportdir     = dir != NULL ? dir : ".";

	sr_games_romset(rom_path, romset, sizeof romset);
	if (romset[0] == '\0')
		return;

	/* The sysop's file is read SECOND and wins. It is the one to edit: games.ini
	 * is shipped, so a pull or a merge overwrites whatever was put there. */
	ini   = sr_games_read(reportdir, "games.ini");
	local = sr_games_read(reportdir, "games.local.ini");

	if (ini == NULL && local == NULL) {
		/* Missing, mistyped, and unmeasured all used to look identical from
		 * outside this module -- silence in the name of "non-fatal" made the
		 * feature undiagnosable (GAMES_INI.md sec 7). Non-fatal stays; silent
		 * does not. */
		fprintf(stderr, "syncretro: games.ini: no file in %s\n", reportdir);
		return;
	}

	/* The install-wide warm-up, read BEFORE the section check: a romset with no
	 * section of its own is the common case (most cabinets need no button
	 * labels), and it is exactly the one that should still get the default. */
	rootboot      = sr_games_clamp_boot(sr_games_int(ini, local, ROOT_SECTION,
	                                                 "boot_frames", 0), "root");
	g_boot_frames = rootboot;

	if (!(ini != NULL && iniSectionExists(ini, romset))
	    && !(local != NULL && iniSectionExists(local, romset))) {
		fprintf(stderr, "syncretro: games.ini: no section for \"%s\"%s\n", romset,
		        rootboot > 0 ? " (install-wide boot_frames still applies)" : "");
		iniFreeStringList(ini);
		iniFreeStringList(local);
		return;
	}

	g_boot_frames = sr_games_clamp_boot(sr_games_int(ini, local, romset,
	                                                 "boot_frames", rootboot),
	                                    romset);

	buttons = 0;
	for (i = 0; i < SR_GAMES_IDS; i++) {
		snprintf(key, sizeof key, "button.%s", g_ids[i].name);
		sr_games_str(ini, local, romset, key, val);
		if (val[0] != '\0') {
			snprintf(g_label[i], sizeof g_label[i], "%s", val);
			g_labelled = 1;
			buttons++;
		}
	}
	sr_games_str(ini, local, romset, "stick2", val);
	snprintf(g_stick2, sizeof g_stick2, "%s", val);

	/* A misspelt id is silent data loss otherwise: the sysop believes a button
	 * is labelled, the help screen omits it as unlabelled, and the player is
	 * told the cabinet has fewer controls than it has. Say so. */
	sr_games_check_keys(ini, romset, "games.ini");
	sr_games_check_keys(local, romset, "games.local.ini");

	{
		char stick[INI_MAX_VALUE_LEN + 16];
		char boot[64];

		stick[0] = '\0';
		boot[0]  = '\0';
		if (g_stick2[0] != '\0')
			snprintf(stick, sizeof stick, ", stick2 \"%s\"", g_stick2);
		if (g_boot_frames > 0)
			snprintf(boot, sizeof boot, ", boot_frames %d", g_boot_frames);
		fprintf(stderr, "syncretro: games.ini: [%s] %d button%s%s%s%s\n",
		        romset, buttons, buttons == 1 ? "" : "s", stick, boot,
		        local != NULL ? " (+ games.local.ini)" : "");
	}

	iniFreeStringList(ini);
	iniFreeStringList(local);
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

int sr_games_boot_frames(void)
{
	return g_boot_frames;
}
