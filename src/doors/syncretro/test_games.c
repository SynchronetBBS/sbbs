/* test_games.c -- games.ini is the table MAME 2003-Plus will not send us
 * (no SET_INPUT_DESCRIPTORS), so every claim it makes about a cabinet is
 * hand-entered and has to survive a reader that is asked about games the file
 * has never heard of. See GAMES_INI.md.
 *
 * Copyright(C) 2026 Rob Swindell / SyncRetro.  GPL-2.0.
 */
#include "syncretro_games.h"
#include "libretro.h"
#include "dirwrap.h"

#include <stdio.h>
#include <string.h>

/* Own fixture directory (not the cwd) so a parallel ctest run cannot race
 * test_binds.c over a shared games.ini. */
#define FIXTURE_DIR "gamesfx"

static int failures;

#define CHECK(cond) \
		do { \
			if (!(cond)) { \
				printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
				failures++; \
			} \
		} while (0)

#define CHECK_STR(got, want) \
		do { \
			const char *g_ = (got); \
			if (g_ == NULL || strcmp(g_, (want)) != 0) { \
				printf("FAIL %s:%d: got \"%s\", want \"%s\"\n", \
					   __FILE__, __LINE__, g_ ? g_ : "(null)", (want)); \
				failures++; \
			} \
		} while (0)

static void write_fixture(void)
{
	FILE *f;

	mkpath(FIXTURE_DIR);
	f = fopen(FIXTURE_DIR "/games.ini", "w");

	fputs("; a comment, which the JSON this replaced could not have\n"
	      "\n"
	      "[bzone]\n"
	      "name     = Battlezone\n"
	      "button.Y = Fire\n"
	      "stick2   = Right tread\n"
	      "\n"
	      "[centiped]\n"
	      "name     = Centipede\n"
	      "button.B = Fire\n"
	      "\n"
	      "[pacman]\n"
	      "name = Pac-Man\n"
	      "\n"
	      "[weird]\n"
	      "name      = Odd One\n"
	      "button.ZZ = Nonsense\n"
	      "\n"
	      "[lc]\n"
	      "name     = Lowercase\n"
	      "button.y = Fire\n", f);
	fclose(f);
}

int main(void)
{
	write_fixture();

	/* A twin-stick cabinet: one labelled button, and a second stick. */
	sr_games_load(FIXTURE_DIR, "/some/where/bzone.zip");
	CHECK(sr_games_labelled());
	CHECK_STR(sr_games_button_label(RETRO_DEVICE_ID_JOYPAD_Y), "Fire");
	CHECK_STR(sr_games_stick2(), "Right tread");
	/* Unlabelled ids stay unlabelled -- the help screen omits them, and a reader
	 * that invented a label would put a dead button on a player's help screen. */
	CHECK(sr_games_button_label(RETRO_DEVICE_ID_JOYPAD_B) == NULL);
	CHECK(sr_games_button_label(RETRO_DEVICE_ID_JOYPAD_L) == NULL);

	/* Fire is NOT the same id on every cabinet -- the whole reason this file
	 * exists. Centipede's is B where Battlezone's is Y. */
	sr_games_load(FIXTURE_DIR, "centiped.zip");
	CHECK(sr_games_labelled());
	CHECK_STR(sr_games_button_label(RETRO_DEVICE_ID_JOYPAD_B), "Fire");
	CHECK(sr_games_button_label(RETRO_DEVICE_ID_JOYPAD_Y) == NULL);
	CHECK(sr_games_stick2() == NULL);          /* one stick */

	/* Title-only: the common case. Nothing labelled, so the help screen keeps
	 * its grouped numbering. */
	sr_games_load(FIXTURE_DIR, "pacman.zip");
	CHECK(!sr_games_labelled());
	CHECK(sr_games_button_label(RETRO_DEVICE_ID_JOYPAD_B) == NULL);
	CHECK(sr_games_stick2() == NULL);

	/* An unknown id name is ignored, and must not make the section "labelled" --
	 * that flag hides every unlabelled button. */
	sr_games_load(FIXTURE_DIR, "weird.zip");
	CHECK(!sr_games_labelled());

	/* A romset the file has never heard of. */
	sr_games_load(FIXTURE_DIR, "nosuchgame.zip");
	CHECK(!sr_games_labelled());
	CHECK(sr_games_button_label(RETRO_DEVICE_ID_JOYPAD_Y) == NULL);
	CHECK(sr_games_stick2() == NULL);

	/* No file at all is a WORKING install, not an error. Loading from a
	 * directory with no games.ini must leave every getter answering "nothing
	 * known" -- including after a successful load, or a console with no file
	 * would inherit the last cabinet's labels. */
	sr_games_load(FIXTURE_DIR, "bzone.zip");
	CHECK(sr_games_labelled());
	sr_games_load("/nonexistent-directory", "bzone.zip");
	CHECK(!sr_games_labelled());
	CHECK(sr_games_button_label(RETRO_DEVICE_ID_JOYPAD_Y) == NULL);

	/* A NULL rom path (the door run with no content) must not crash. */
	sr_games_load(FIXTURE_DIR, NULL);
	CHECK(!sr_games_labelled());

	/* xpdev's ini reader matches keys CASE-INSENSITIVELY (ini_file.c uses
	 * stricmp()), so a lower-case "button.y" is honoured exactly like
	 * "button.Y" is. The validator has to agree: a case-sensitive compare here
	 * would flag a key the reader just finished honouring, which is the
	 * validator lying about the file's own most natural sysop typo. Capture
	 * stderr for this one load and prove the bogus warning is gone. */
	{
		FILE * cap;
		char   buf[4096];
		size_t got = 0;

		fflush(stderr);
		freopen(FIXTURE_DIR "/stderr.txt", "w", stderr);
		sr_games_load(FIXTURE_DIR, "lc.zip");
		fflush(stderr);

		CHECK(sr_games_labelled());
		CHECK_STR(sr_games_button_label(RETRO_DEVICE_ID_JOYPAD_Y), "Fire");

		buf[0] = '\0';
		cap = fopen(FIXTURE_DIR "/stderr.txt", "r");
		if (cap != NULL) {
			got = fread(buf, 1, sizeof buf - 1, cap);
			fclose(cap);
		}
		buf[got] = '\0';
		CHECK(strstr(buf, "is not a RetroPad button") == NULL);
	}

	remove(FIXTURE_DIR "/stderr.txt");
	remove(FIXTURE_DIR "/games.ini");
	printf("%s: %d failure(s)\n", failures ? "FAIL" : "ok", failures);
	return failures != 0;
}
