/* test_title.c -- the who's-online title, derived from a ROM filename.
 *
 * This parser exists so the lobby can leave -title OFF the door's command line,
 * which Synchronet assembles into a 260-byte buffer on Windows and truncates
 * there in silence. The cases below are real filenames from the shipped ROM
 * sets, including the one whose command line overflowed.
 *
 * The lobby's syncretro_parse_title() stays authoritative (see
 * syncretro_title.h); the "left alone" group is what this deliberately does not
 * try to strip, asserted so a later "improvement" has to be a deliberate one.
 *
 * Copyright(C) 2026 Rob Swindell / SyncRetro.  GPL-2.0.
 */
#include "syncretro_title.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void check(const char *path, const char *want)
{
	char got[64];

	sr_title_from_rom(got, sizeof got, path);
	if (strcmp(got, want) != 0) {
		printf("FAIL \"%s\"\n  got  \"%s\"\n  want \"%s\"\n",
		       path ? path : "(null)", got, want);
		failures++;
	}
}

int main(void)
{
	/* The shape this exists for: "(year) (publisher)", with a path in front. */
	check("roms/Astrosmash (1981) (Mattel).int", "Astrosmash");
	check("roms\\Astrosmash (1981) (Mattel).int", "Astrosmash");
	check("/sbbs/xtrn/syncivision/roms/Astrosmash (1981) (Mattel).int", "Astrosmash");

	/* The cartridge whose command line was truncated on VERT: the title runs to
	 * the end of the line, so losing it is what the door reported as "(no ROM)". */
	check("roms\\Advanced D&D - Treasure of Tarmin (1982) (Mattel).zip",
	      "Advanced D&D - Treasure of Tarmin");
	check("roms/SDK-1600 Intellicart Bankswitched Spinning World (2002) (Joseph Zbiciak).bin",
	      "SDK-1600 Intellicart Bankswitched Spinning World");

	/* A year with no publisher, and a year range. */
	check("Bowling (1979).bin", "Bowling");
	check("4-TRIS (2000-01) (Homebrew).bin", "4-TRIS");

	/* Stacked dump markers, before and after the year/publisher strip. */
	check("Astrosmash (1981) (Mattel)[!].int", "Astrosmash");
	check("Astrosmash (1981) (Mattel) [a1][!].int", "Astrosmash");
	check("Bowling [!].bin", "Bowling");

	/* A publisher with NO year in front of it is left alone: on its own, a
	 * trailing parenthetical is far more likely to be part of the title. */
	check("Pac-Man (Atarisoft).int", "Pac-Man (Atarisoft)");
	check("AD&D - Cloudy Mountain.int", "AD&D - Cloudy Mountain");
	check("Advanced Dungeons & Dragons (Cloudy Mountain).int",
	      "Advanced Dungeons & Dragons (Cloudy Mountain)");

	/* Cataloging tags are NOT stripped -- that is the lobby's job, against a word
	 * list this deliberately does not copy. */
	check("roms/Downtown Special - Kunio-kun (Japan) (Translated).zip",
	      "Downtown Special - Kunio-kun (Japan) (Translated)");

	/* No extension, a dot in the name, and a dotfile-ish leading dot. */
	check("puckman", "puckman");
	check("Mr. Do! (1983) (CBS).bin", "Mr. Do!");
	check(".hack.zip", ".hack");

	/* Degenerate input: never an empty status line. */
	check(NULL, "a cartridge");
	check("", "a cartridge");
	check("roms/(1981) (Mattel).int", "(1981) (Mattel)");
	check("roms/", "a cartridge");

	/* Truncation: cap is honored and the result stays NUL-terminated. */
	{
		char small[8];

		sr_title_from_rom(small, sizeof small, "roms/Astrosmash (1981) (Mattel).int");
		if (strlen(small) >= sizeof small) {
			printf("FAIL: overran an 8-byte buffer\n");
			failures++;
		}
	}

	printf("%s: test_title\n", failures ? "FAILED" : "ok");
	return failures ? 1 : 0;
}
