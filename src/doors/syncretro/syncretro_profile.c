/* syncretro_profile.c -- see syncretro_profile.h. */
#include "syncretro_profile.h"
#include "syncretro.h"

#include <stdio.h>
#include <string.h>

typedef struct {
	sr_profile_id_t id;
	const char *key;              /* what -profile spells */
	const char *name;             /* what the help screen shows */
	int analog;                   /* the console reads the analog stick */
	int arrow_port;               /* which controller the arrow keys drive */
} sr_profile_row_t;

static const sr_profile_row_t g_profiles[] = {
	{ SR_PROFILE_PAD,    "pad",    "gamepad",        0, 0 },
	{ SR_PROFILE_INTV,   "intv",   "Intellivision",  1, 1 },
	/* Same two facts as `pad` -- no analog, arrows on port 0 -- because an
	 * arcade panel IS a gamepad electrically. What differs is the bind table's
	 * labels (syncretro_binds.c) and the absent Tab swap. */
	{ SR_PROFILE_ARCADE, "arcade", "arcade",         0, 0 }
};

#define SR_NPROFILES ((int)(sizeof g_profiles / sizeof g_profiles[0]))

/* The default is PAD, and it is a real default, not a fallback of last resort:
 * an unknown core is a gamepad console until someone proves otherwise, and a
 * gamepad console plays correctly with no C change at all. */
static const sr_profile_row_t *g_active = &g_profiles[0];

/* Case-insensitive equality. FreeIntv reports its library_name as "freeintv",
 * lower-case -- NOT the "FreeIntv" spelling used in its own repo, in RetroArch's
 * core list, and everywhere in our docs (PROVENANCE.md records the same casing
 * trap for its .so filename). A case-sensitive compare here would fail SILENTLY:
 * the Intellivision would fall through to `pad` and lose its keypad, with no
 * error anywhere. */
static int sr_ieq(const char *a, const char *b)
{
	if (a == NULL || b == NULL)
		return 0;
	for (; *a != '\0' && *b != '\0'; a++, b++) {
		int ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
		int cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;

		if (ca != cb)
			return 0;
	}
	return *a == '\0' && *b == '\0';
}

static const sr_profile_row_t *sr_profile_by_key(const char *key)
{
	int i;

	if (key == NULL || key[0] == '\0')
		return NULL;
	for (i = 0; i < SR_NPROFILES; i++)
		if (sr_ieq(g_profiles[i].key, key))
			return &g_profiles[i];
	return NULL;
}

/* The core's own name -> a profile, for a door run bare from a command line (a
 * probe, a test) with no -profile. The lobby always passes one. */
static const sr_profile_row_t *sr_profile_by_core(const char *library_name)
{
	if (sr_ieq(library_name, "freeintv"))
		return &g_profiles[SR_PROFILE_INTV];
	return NULL;
}

void sr_profile_select(const char *want, const char *library_name)
{
	const char *            by_core;
	const sr_profile_row_t *p = sr_profile_by_key(want);

	if (p == NULL && want != NULL && want[0] != '\0') {
		fprintf(stderr, "syncretro: unknown -profile \"%s\"; inferring from the core\n",
		        want);
	}
	if (p == NULL)
		p = sr_profile_by_core(library_name);
	if (p == NULL)
		p = &g_profiles[SR_PROFILE_PAD];

	g_active = p;
	by_core = (library_name != NULL) ? library_name : "?";
	fprintf(stderr, "syncretro: controller profile \"%s\" (%s), core \"%s\"\n",
	        g_active->key, g_active->name, by_core);
}

sr_profile_id_t sr_profile(void)      { return g_active->id; }
const char *sr_profile_name(void)     { return g_active->name; }
int sr_profile_analog(void)           { return g_active->analog; }
int sr_profile_arrow_port(void)       { return g_active->arrow_port; }
