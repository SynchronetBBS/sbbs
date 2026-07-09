/*
 * test_map_name.c -- unit test for syncduke_map_name() (level -> display name).
 *
 * Links the real syncduke_game.c against stub definitions of the handful of engine
 * globals it reads (duke3d.h declares them extern; global.c defines them, and pulling
 * that in would drag the whole engine). The cases that matter are the ones the CON
 * feeds us: an uppercase name with the trailing '\r' of a CRLF CON (every stock GRP),
 * an undefined level slot, and a -map user map.  -fcommon matches the engine build:
 * duke3d.h's globals are tentative definitions, so without it they collide at link.
 * Build+run (xpdev supplies getfname()/getfext()):
 *   cc -fcommon -DPLATFORM_UNIX -I../Game/src -I../Engine/src -I../../../xpdev \
 *      -I../../termgfx -o /tmp/test_map_name test_map_name.c ../syncduke_game.c \
 *      ../../../xpdev/gcc.linux.x64.lib.release/libxpdev.a && /tmp/test_map_name
 */

#include <stdio.h>
#include <string.h>
#include "duke3d.h"
#include "../syncduke.h"

/* Engine globals syncduke_game.c reads (defined in Game/src/global.c). */
char                 level_names[44][33];
char                 boardfilename[128];
struct user_defs     ud;
struct player_struct ps[MAXPLAYERS];
spritetype           sprite[MAXSPRITES];
short                myconnectindex;
short                frags[MAXPLAYERS][MAXPLAYERS];

static int           failures;

static void expect(const char *what, const char *got, const char *want)
{
	if (strcmp(got, want) == 0) {
		printf("ok   %-28s -> \"%s\"\n", what, got);
		return;
	}
	printf("FAIL %-28s -> \"%s\" (expected \"%s\")\n", what, got, want);
	failures++;
}

int test_main(void)
{
	char buf[64];

	/* Stock level: the CON's uppercase name, CRLF's '\r' trimmed off. */
	strcpy(level_names[0 * 11 + 0], "HOLLYWOOD HOLOCAUST\r");
	syncduke_map_name(0, 0, buf, sizeof(buf));
	expect("E1L1 (CRLF CON name)", buf, "HOLLYWOOD HOLOCAUST");

	/* A name with no trailing whitespace survives intact. */
	strcpy(level_names[2 * 11 + 0], "RAW MEAT");
	syncduke_map_name(2, 0, buf, sizeof(buf));
	expect("E3L1 (bare CON name)", buf, "RAW MEAT");

	/* A slot the CON never defined falls back to the E#L# code. */
	syncduke_map_name(0, 9, buf, sizeof(buf));
	expect("E1L10 (undefined slot)", buf, "E1L10");

	/* Out-of-range slot: fall back, don't read past level_names[]. */
	syncduke_map_name(3, 10, buf, sizeof(buf));
	expect("E4L11 (past table end)", buf, "E4L11");

	/* A -map user map (always slot E1L8): basename, ".map" stripped. */
	strcpy(boardfilename, "/sbbs/xtrn/syncduke/addons/Roch.MAP");
	syncduke_map_name(0, 7, buf, sizeof(buf));
	expect("user map (E1L8)", buf, "Roch");

	/* ...but a user map loaded while some other slot plays doesn't hijack the name. */
	strcpy(level_names[0 * 11 + 1], "RED LIGHT DISTRICT\r");
	syncduke_map_name(0, 1, buf, sizeof(buf));
	expect("E1L2 with a board loaded", buf, "RED LIGHT DISTRICT");

	printf(failures ? "\nFAILED (%d)\n" : "\nPASSED\n", failures);
	return failures != 0;
}

/* duke3d.h's funct.h declares the engine's own main(); ours must not collide. */
int main(int argc, char **argv) { (void)argc; (void)argv; return test_main(); }
