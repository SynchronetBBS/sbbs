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
	      "button.ZZ = Nonsense\n", f);
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

	remove(FIXTURE_DIR "/games.ini");
	printf("%s: %d failure(s)\n", failures ? "FAIL" : "ok", failures);
	return failures != 0;
}
