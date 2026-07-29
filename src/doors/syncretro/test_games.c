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
	      "boot_frames = 900\n"
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
	      "button.y = Fire\n"
	      "\n"
	      "[slowboot]\n"
	      "name        = Long Self-Test\n"
	      "boot_frames = 1800\n"
	      "\n"
	      "[noboot]\n"
	      "name        = Warm-Up Off\n"
	      "boot_frames = 0\n"
	      "\n"
	      "[silly]\n"
	      "name        = Slipped Digit\n"
	      "boot_frames = 900000\n"
	      "\n"
	      "[negative]\n"
	      "name        = Backwards\n"
	      "boot_frames = -1\n"
	      "\n"
	      "[paperboy]\n"
	      "name        = Paperboy\n"
	      "analog_rest = 45,57\n"
	      "\n"
	      "[restwide]\n"
	      "name        = Slipped Digit Again\n"
	      "analog_rest = 300,-999\n"
	      "\n"
	      "[restjunk]\n"
	      "name        = Not A Pair\n"
	      "analog_rest = sideways\n"
	      "\n"
	      "[resthalf]\n"
	      "name        = One Number\n"
	      "analog_rest = 43\n", f);
	fclose(f);
}

/* A third fixture for the sysop's overlay. games.ini is shipped, so anything
 * written into it is lost to the next pull; games.local.ini holds only what
 * differs and is read over the top. Its own directory, so the fixtures above
 * keep reading the files they expect. */
#define LOCAL_DIR "gamesfx_local"

static void write_local_fixture(void)
{
	FILE *f;

	mkpath(LOCAL_DIR);
	f = fopen(LOCAL_DIR "/games.ini", "w");
	fputs("boot_frames = 900\n"
	      "\n"
	      "[bzone]\n"
	      "name     = Battlezone\n"
	      "button.Y = Fire\n"
	      "stick2   = Right tread\n"
	      "\n"
	      "[centiped]\n"
	      "name     = Centipede\n"
	      "button.B = Fire\n", f);
	fclose(f);

	f = fopen(LOCAL_DIR "/games.local.ini", "w");
	fputs("boot_frames = 300\n"
	      "\n"
	      "[bzone]\n"
	      "button.Y = Zap\n"
	      "\n"
	      "[myrom]\n"
	      "name        = House Cabinet\n"
	      "button.A    = Launch\n"
	      "boot_frames = 60\n"
	      "analog_rest = 20,30\n", f);
	fclose(f);
}

/* A second fixture with no root-level boot_frames: the install that never asked
 * for a warm-up must not get one. Its own directory, so the root-default tests
 * above keep reading the file they expect. */
#define NOROOT_DIR "gamesfx_noroot"

static void write_noroot_fixture(void)
{
	FILE *f;

	mkpath(NOROOT_DIR);
	f = fopen(NOROOT_DIR "/games.ini", "w");
	fputs("[bzone]\n"
	      "name     = Battlezone\n"
	      "button.Y = Fire\n", f);
	fclose(f);
}

int main(void)
{
	write_fixture();
	write_noroot_fixture();
	write_local_fixture();

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

	/* boot_frames: the warm-up count, which unlike the button labels has an
	 * install-wide default. A section with no key of its own inherits the
	 * root-level value -- the common case, since most cabinets have no section
	 * at all and every one of them should still skip its self-test. */
	sr_games_load(FIXTURE_DIR, "bzone.zip");
	CHECK(sr_games_boot_frames() == 900);
	sr_games_load(FIXTURE_DIR, "nosuchgame.zip");   /* no section: still the default */
	CHECK(sr_games_boot_frames() == 900);

	/* A section's own key overrides the default, in both directions: a cabinet
	 * with a longer self-test asks for more, and one that should not be warmed
	 * up at all says 0 -- which must not be mistaken for "unset" and silently
	 * replaced by the root value. */
	sr_games_load(FIXTURE_DIR, "slowboot.zip");
	CHECK(sr_games_boot_frames() == 1800);
	sr_games_load(FIXTURE_DIR, "noboot.zip");
	CHECK(sr_games_boot_frames() == 0);

	/* A slipped digit is clamped rather than obeyed: the warm-up is real CPU
	 * spent with the player looking at nothing. */
	sr_games_load(FIXTURE_DIR, "silly.zip");
	CHECK(sr_games_boot_frames() == 18000);
	sr_games_load(FIXTURE_DIR, "negative.zip");
	CHECK(sr_games_boot_frames() == 0);

	/* No root key, and no file at all: no warm-up. An install that never asked
	 * for one runs the boot in real time, as the door always has. */
	sr_games_load(NOROOT_DIR, "bzone.zip");
	CHECK(sr_games_boot_frames() == 0);
	sr_games_load("/nonexistent-directory", "bzone.zip");
	CHECK(sr_games_boot_frames() == 0);
	sr_games_load(FIXTURE_DIR, NULL);
	CHECK(sr_games_boot_frames() == 0);

	/* games.local.ini -- the sysop's overlay. It wins key by key, and everything
	 * it does not mention keeps coming from the shipped file: that is what makes
	 * it safe to keep across an upgrade, where a whole-file copy would freeze
	 * the sysop's install at the version they copied. */
	sr_games_load(LOCAL_DIR, "bzone.zip");
	CHECK_STR(sr_games_button_label(RETRO_DEVICE_ID_JOYPAD_Y), "Zap");
	CHECK_STR(sr_games_stick2(), "Right tread");   /* untouched: still shipped */

	/* A section only the local file has. */
	sr_games_load(LOCAL_DIR, "myrom.zip");
	CHECK(sr_games_labelled());
	CHECK_STR(sr_games_button_label(RETRO_DEVICE_ID_JOYPAD_A), "Launch");
	CHECK(sr_games_boot_frames() == 60);

	/* A section only the SHIPPED file has still resolves. */
	sr_games_load(LOCAL_DIR, "centiped.zip");
	CHECK_STR(sr_games_button_label(RETRO_DEVICE_ID_JOYPAD_B), "Fire");

	/* Root keys overlay at the root: the local default wins for a romset with
	 * no boot_frames of its own... */
	sr_games_load(LOCAL_DIR, "bzone.zip");
	CHECK(sr_games_boot_frames() == 300);
	sr_games_load(LOCAL_DIR, "nosuchgame.zip");
	CHECK(sr_games_boot_frames() == 300);

	/* ...and the shipped file alone still answers when there is no overlay
	 * beside it, which is every install that has not been customized. */
	sr_games_load(FIXTURE_DIR, "bzone.zip");
	CHECK(sr_games_boot_frames() == 900);

	/* analog_rest: where a cabinet's stick SITS when nobody is touching it.
	 * Unlike boot_frames this has no install-wide default -- it is a measured
	 * fact about one control panel, and a wrong one steers the machine on its
	 * own -- so a cabinet that never declares it reads "no analog control". */
	{
		int x = -1, y = -1;

		sr_games_load(FIXTURE_DIR, "paperboy.zip");
		CHECK(sr_games_analog_rest(&x, &y) == 1);
		CHECK(x == 45);
		CHECK(y == 57);

		/* A digital control panel -- almost all of them. Both out-params are
		 * cleared, so a caller that ignores the return value still centres the
		 * stick rather than reusing the last cabinet's handlebar. */
		x = y = -1;
		sr_games_load(FIXTURE_DIR, "bzone.zip");
		CHECK(sr_games_analog_rest(&x, &y) == 0);
		CHECK(x == 0);
		CHECK(y == 0);

		/* Out of range is clamped, not obeyed: the value is a percentage of the
		 * stick's own travel, and there is nothing past the stops. */
		sr_games_load(FIXTURE_DIR, "restwide.zip");
		CHECK(sr_games_analog_rest(&x, &y) == 1);
		CHECK(x == 100);
		CHECK(y == -100);

		/* Unparseable is ABSENT, not zero: "sideways" and "43" are typos, and a
		 * typo that silently means "centred" is the very bug this key exists to
		 * fix -- it would look exactly like the cabinet had never been measured. */
		x = y = -1;
		sr_games_load(FIXTURE_DIR, "restjunk.zip");
		CHECK(sr_games_analog_rest(&x, &y) == 0);
		CHECK(x == 0 && y == 0);
		sr_games_load(FIXTURE_DIR, "resthalf.zip");
		CHECK(sr_games_analog_rest(&x, &y) == 0);

		/* A section that declares one must not leak into the next cabinet
		 * loaded, which is how a Pac-Man cabinet would inherit a handlebar. */
		sr_games_load(FIXTURE_DIR, "paperboy.zip");
		CHECK(sr_games_analog_rest(&x, &y) == 1);
		sr_games_load(FIXTURE_DIR, "pacman.zip");
		CHECK(sr_games_analog_rest(&x, &y) == 0);

		/* The sysop's overlay reaches it like every other key: a handlebar that
		 * measures differently on his ROM set is one line in games.local.ini. */
		sr_games_load(LOCAL_DIR, "myrom.zip");
		CHECK(sr_games_analog_rest(&x, &y) == 1);
		CHECK(x == 20);
		CHECK(y == 30);
	}

	remove(LOCAL_DIR "/games.local.ini");
	remove(LOCAL_DIR "/games.ini");
	remove(NOROOT_DIR "/games.ini");
	remove(FIXTURE_DIR "/stderr.txt");
	remove(FIXTURE_DIR "/games.ini");
	printf("%s: %d failure(s)\n", failures ? "FAIL" : "ok", failures);
	return failures != 0;
}
