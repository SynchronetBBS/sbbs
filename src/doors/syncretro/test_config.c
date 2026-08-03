/* test_config.c -- syncretro.ini is shipped and syncretro.local.ini is the
 * sysop's overlay, read over the top of it. Every key the door has must resolve
 * through both, at the same scope, with the local file winning. See
 * docs/superpowers/specs/2026-08-02-syncretro-config-consolidation-design.md.
 *
 * Copyright(C) 2026 Rob Swindell / SyncRetro.  GPL-2.0.
 */
#include "syncretro.h"
#include "retro_core.h"   /* rc_core_t, for the rc_core_load_game() stub */
#include "dirwrap.h"

#include <stdio.h>
#include <string.h>

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

/* Every door module syncretro_config.c calls into, stubbed so this test needs
 * neither a libretro core nor termgfx: the claim under test is the ini
 * resolution, not what the pinned options later do. sr_config_apply() is never
 * called here, but the linker still has to resolve what it references. */
#define STUB_PINS 32
static char stub_pin[STUB_PINS][160];
static int  stub_npin;

int sr_option_pin(const char *kv)
{
	if (stub_npin >= STUB_PINS)
		return -1;
	snprintf(stub_pin[stub_npin], sizeof stub_pin[0], "%s", kv);
	stub_npin++;
	return 0;
}

const char *rc_core_ext(void)
{
	return ".so";
}

int rc_core_load_game(rc_core_t *c, const char *rom_path)
{
	(void)c;
	(void)rom_path;
	return 0;
}

const char *sr_door_home(void)
{
	return NULL;
}

const char *sr_door_core_path(void)
{
	return NULL;
}

const char *sr_door_rom_path(void)
{
	return NULL;
}

void sr_io_set_aspect(double aspect)
{
	(void)aspect;
}

/* The LAST pin recorded for `key`, or NULL. Mirrors sr_option_apply()'s
 * last-wins loop (retro_options.c:118-123), which is what makes appending the
 * local file's options after the shipped file's the correct merge. */
static const char *last_pin(const char *key)
{
	size_t      n = strlen(key);
	const char *hit = NULL;
	int         i;

	for (i = 0; i < stub_npin; i++) {
		if (strncmp(stub_pin[i], key, n) == 0 && stub_pin[i][n] == '=')
			hit = stub_pin[i] + n + 1;
	}
	return hit;
}

#define BASE_DIR  "cfgfx_base"    /* shipped file only */
#define BOTH_DIR  "cfgfx_both"    /* shipped + sysop overlay */
#define EMPTY_DIR "cfgfx_empty"   /* neither file */

static void write_fixtures(void)
{
	FILE *f;

	mkpath(BASE_DIR);
	f = fopen(BASE_DIR "/syncretro.ini", "w");
	fputs("[console]\n"
	      "name    = Intellivision\n"
	      "short   = Intv\n"
	      "core    = freeintv_libretro\n"
	      "profile = intv\n"
	      "\n"
	      "[video]\n"
	      "aspect     = 4:3\n"
	      "dirty_rect = true\n"
	      "pace_depth = 0\n"
	      "\n"
	      "[audio]\n"
	      "enabled = true\n"
	      "volume  = 80\n"
	      "\n"
	      "[idle]\n"
	      "timeout = 15m\n"
	      "warn    = 60\n"
	      "\n"
	      "[disc]\n"
	      "rotate = brickout\n"
	      "\n"
	      "[options]\n"
	      "freeintv_pixel_perfect = enabled\n"
	      "freeintv_control       = keypad\n", f);
	fclose(f);

	mkpath(BOTH_DIR);
	f = fopen(BOTH_DIR "/syncretro.ini", "w");
	fputs("[console]\n"
	      "name    = Intellivision\n"
	      "short   = Intv\n"
	      "profile = intv\n"
	      "\n"
	      "[video]\n"
	      "aspect     = 4:3\n"
	      "dirty_rect = true\n"
	      "\n"
	      "[audio]\n"
	      "volume = 80\n"
	      "\n"
	      "[idle]\n"
	      "timeout = 15m\n"
	      "\n"
	      "[options]\n"
	      "freeintv_pixel_perfect = enabled\n"
	      "freeintv_control       = keypad\n", f);
	fclose(f);

	/* The sysop's overlay: only what differs. */
	f = fopen(BOTH_DIR "/syncretro.local.ini", "w");
	fputs("[video]\n"
	      "aspect = square\n"
	      "\n"
	      "[audio]\n"
	      "volume = 40\n"
	      "\n"
	      "[idle]\n"
	      "timeout = 0\n"
	      "\n"
	      "[debug]\n"
	      "dirty_log = true\n"
	      "\n"
	      "[options]\n"
	      "freeintv_control = disc\n", f);
	fclose(f);

	mkpath(EMPTY_DIR);
}

/* The shipped file alone: every key resolves from it. */
static void test_base_only(void)
{
	stub_npin = 0;
	sr_config_read(BASE_DIR);

	CHECK_STR(sr_config_console_name(), "Intellivision");
	CHECK_STR(sr_config_profile(), "intv");
	CHECK_STR(sr_config_aspect_mode(), "4:3");
	CHECK(sr_config_audio_volume() == 80);
	CHECK(sr_config_idle_timeout() == 900);
	CHECK(sr_config_idle_warn() == 60);
	CHECK(sr_config_dirty_log() == 0);
	CHECK_STR(last_pin("freeintv_control"), "keypad");
}

/* The overlay: a local key wins, a shipped-only key survives, a local-only
 * section arrives, and [options] merges by name rather than replacing. */
static void test_local_overlay(void)
{
	stub_npin = 0;
	sr_config_read(BOTH_DIR);

	CHECK_STR(sr_config_aspect_mode(), "square");          /* local wins */
	CHECK(sr_config_audio_volume() == 40);                 /* local wins */
	CHECK(sr_config_idle_timeout() == 0);                  /* local wins, 0 is a value */
	CHECK(sr_config_dirty_rect() == 1);                    /* shipped survives */
	CHECK_STR(sr_config_console_name(), "Intellivision");  /* shipped survives */
	CHECK(sr_config_dirty_log() == 1);                     /* local-only section */
	CHECK_STR(last_pin("freeintv_control"), "disc");       /* local wins */
	CHECK_STR(last_pin("freeintv_pixel_perfect"), "enabled");  /* shipped survives */
}

/* Neither file: the compiled floor, and the door still starts. */
static void test_no_files(void)
{
	stub_npin = 0;
	sr_config_read(EMPTY_DIR);

	CHECK(sr_config_audio_volume() == 100);
	CHECK(sr_config_dirty_rect() == 1);
	CHECK(sr_config_idle_timeout() == 600);
	CHECK(sr_config_idle_warn() == 60);
	CHECK(sr_config_console_name() == NULL);
	CHECK(stub_npin == 0);
}

/* Byte-for-byte copy a fixture file, for test_full_copy_equivalence() below. */
static void copy_file(const char *src_path, const char *dst_path)
{
	FILE *src = fopen(src_path, "r");
	FILE *dst = fopen(dst_path, "w");
	int   c;

	CHECK(src != NULL && dst != NULL);
	if (src != NULL && dst != NULL) {
		while ((c = fgetc(src)) != EOF)
			fputc(c, dst);
	}
	if (src != NULL)
		fclose(src);
	if (dst != NULL)
		fclose(dst);
}

/* An install whose syncretro.local.ini is a byte-for-byte copy of its
 * syncretro.ini resolves identically to an install with no overlay at all.
 * This is the property that makes the migration in the spec safe: a sysop
 * renaming their whole config to syncretro.local.ini changes nothing, because
 * the shipped file underneath it still reads the same either way. */
static void test_full_copy_equivalence(void)
{
	char aspect[64];
	int  volume;

	stub_npin = 0;
	sr_config_read(BASE_DIR);
	snprintf(aspect, sizeof aspect, "%s", sr_config_aspect_mode());
	volume = sr_config_audio_volume();

	mkpath("cfgfx_copy");
	copy_file(BASE_DIR "/syncretro.ini", "cfgfx_copy/syncretro.ini");
	copy_file(BASE_DIR "/syncretro.ini", "cfgfx_copy/syncretro.local.ini");

	stub_npin = 0;
	sr_config_read("cfgfx_copy");
	CHECK_STR(sr_config_aspect_mode(), aspect);
	CHECK(sr_config_audio_volume() == volume);
}

int main(void)
{
	write_fixtures();
	test_base_only();
	test_local_overlay();
	test_no_files();
	test_full_copy_equivalence();

	printf("%s: %d failure(s)\n", failures ? "FAILED" : "ok", failures);
	return failures != 0;
}
