/* test_stream_volume.c -- the channel level: clamping, and the mute floor that
 * stops the uplink. Driven with the stream forced OFF (a non-libsndfile caps
 * reply), which is the one state where _set_volume runs its whole body and then
 * returns without emitting an APC -- so the level logic is under test without an
 * encoder, a terminal or libsndfile. */
#include "audio_stream.h"
#include "audio.h"

#include <stddef.h>
#include <stdio.h>

static int failures;

#define CHECK(cond) \
		do { \
			if (!(cond)) { \
				printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
				failures++; \
			} \
		} while (0)

static void put(void *ctx, const void *buf, size_t len)
{
	(void)ctx; (void)buf; (void)len;
}

static int flush(void *ctx)
{
	(void)ctx;
	return 1;
}

static size_t backlog(void *ctx)
{
	(void)ctx;
	return 0;
}

static const termgfx_stream_io_t io = { put, flush, backlog, NULL };

/* A stream parked in the OFF state at `volume_db`. */
static termgfx_stream_t *stream(float volume_db)
{
	termgfx_stream_cfg_t cfg;
	termgfx_stream_t *   s;

	cfg.enabled      = 1;
	cfg.quality      = 0.5;
	cfg.volume_db    = volume_db;
	cfg.chunk_ms     = 100;
	cfg.prebuffer    = 3;
	cfg.channels     = 1;
	cfg.rate         = 44100;
	cfg.ch           = 2;
	cfg.slot         = 0;
	cfg.name         = "test";
	cfg.cache_prefix = "t";
	s                = termgfx_stream_create(&cfg, &io);
	termgfx_stream_caps(s, 0);      /* tier 0: OFF, and no bytes ever sent */
	return s;
}

int main(void)
{
	termgfx_stream_t *s;

	s = stream(TERMGFX_DB_UNITY);
	CHECK(termgfx_stream_volume(s) == TERMGFX_DB_UNITY);
	CHECK(!termgfx_stream_muted(s));

	/* Down to the floor and back up: the level is absolute, so mute is a
	 * position on the scale like any other and not a state to escape. */
	CHECK(termgfx_stream_set_volume(s, TERMGFX_DB_MUTE) == TERMGFX_DB_MUTE);
	CHECK(termgfx_stream_muted(s));
	CHECK(termgfx_stream_set_volume(s, -20.0f) == -20.0f);
	CHECK(!termgfx_stream_muted(s));
	CHECK(termgfx_stream_volume(s) == -20.0f);

	/* Setting the level it already holds is a no-op, not a re-mute or a
	 * spurious re-prime. */
	CHECK(termgfx_stream_set_volume(s, -20.0f) == -20.0f);
	CHECK(!termgfx_stream_muted(s));

	/* Clamped at both ends: never above unity (these streams are pre-mixed,
	 * so a boost only clips), never below the silence floor. */
	CHECK(termgfx_stream_set_volume(s, +12.0f) == TERMGFX_DB_UNITY);
	CHECK(termgfx_stream_set_volume(s, -400.0f) == TERMGFX_DB_MUTE);
	CHECK(termgfx_stream_muted(s));
	termgfx_stream_destroy(s);

	/* The percent conversion a door drives this from: 100 is unity, 0 is the
	 * floor -- so a door's own 0..100 ladder reaches both rails exactly, with
	 * no dead zone at the bottom where audio is inaudible but still sent. */
	s = stream(termgfx_db_from_pct(100));
	CHECK(termgfx_stream_volume(s) == TERMGFX_DB_UNITY);
	CHECK(!termgfx_stream_muted(s));
	CHECK(termgfx_stream_set_volume(s, termgfx_db_from_pct(10)) < 0.0f);
	CHECK(!termgfx_stream_muted(s));            /* 10% is quiet, not off */
	termgfx_stream_set_volume(s, termgfx_db_from_pct(0));
	CHECK(termgfx_stream_muted(s));             /* 0% is a real mute */
	termgfx_stream_set_volume(s, termgfx_db_from_pct(10));
	CHECK(!termgfx_stream_muted(s));            /* and it comes back */
	termgfx_stream_destroy(s);

	/* A stream created already at the floor is muted from the start, not
	 * merely quiet: `volume = 0` in the sysop's INI sends nothing. */
	s = stream(termgfx_db_from_pct(0));
	CHECK(termgfx_stream_muted(s));
	termgfx_stream_destroy(s);

	/* Every entry point is NULL-safe: a door with audio disabled forwards
	 * keystrokes blindly into a NULL stream. */
	CHECK(termgfx_stream_set_volume(NULL, TERMGFX_DB_UNITY) == TERMGFX_DB_MUTE);
	CHECK(termgfx_stream_volume(NULL) == TERMGFX_DB_MUTE);
	CHECK(termgfx_stream_muted(NULL));

	printf("%s: %d failure(s)\n", __FILE__, failures);
	return failures == 0 ? 0 : 1;
}
