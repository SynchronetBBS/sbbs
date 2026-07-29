/* test_warmup.c -- the boot warm-up runs the core with its output going
 * NOWHERE. Both halves matter and fail differently: a frame that reached
 * termgfx would be encoded and pushed at the warm-up's tens-of-frames-per-real
 * -millisecond rate, and PCM that reached the stream would queue the whole
 * self-test's audio in front of the game. Neither is visible from a headless
 * run of the door -- there is no terminal to see it on -- so the gate is pinned
 * here instead, by capturing the callbacks retro_bridge.c installs and counting
 * what they pass on. See GAMES_INI.md sec 13.
 *
 * Copyright(C) 2026 Rob Swindell / SyncRetro.  GPL-2.0.
 */
#include "retro_core.h"
#include "syncretro.h"

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

/* --- the door modules retro_bridge.c calls into, counted ------------------ */

static int    n_present;
static size_t n_pcm_frames;
static int    n_pump;

void sr_io_present(const uint8_t *rgb, int w, int h)
{
	(void)rgb; (void)w; (void)h;
	n_present++;
}

size_t sr_audio_feed(const int16_t *pcm, size_t frames)
{
	(void)pcm;
	n_pcm_frames += frames;
	return frames;
}

void sr_input_pump(void) { n_pump++; }

int16_t sr_pad_get(unsigned port, unsigned device, unsigned index, unsigned id)
{
	(void)port; (void)device; (void)index; (void)id;
	return 0;
}

bool sr_environment(unsigned cmd, void *data)
{
	(void)cmd; (void)data;
	return false;
}

/* --- capture the callbacks the bridge installs ---------------------------- */

static retro_video_refresh_t      cb_video;
static retro_audio_sample_t       cb_audio;
static retro_audio_sample_batch_t cb_audio_batch;
static retro_input_poll_t         cb_poll;

static void set_env(retro_environment_t f)             { (void)f; }
static void set_video(retro_video_refresh_t f)         { cb_video = f; }
static void set_audio(retro_audio_sample_t f)          { cb_audio = f; }
static void set_audio_batch(retro_audio_sample_batch_t f) { cb_audio_batch = f; }
static void set_poll(retro_input_poll_t f)             { cb_poll = f; }
static void set_input(retro_input_state_t f)           { (void)f; }

/* One frame of everything a core emits from inside retro_run(). */
static void emit_frame(void)
{
	static uint16_t fb[8 * 8];              /* 0RGB1555, the libretro default */
	static int16_t  pcm[64 * 2];

	cb_poll();
	cb_video(fb, 8, 8, 8 * sizeof fb[0]);
	cb_audio_batch(pcm, 64);
	cb_audio(0, 0);
}

int main(void)
{
	rc_core_t core;

	memset(&core, 0, sizeof core);
	core.set_environment        = set_env;
	core.set_video_refresh      = set_video;
	core.set_audio_sample       = set_audio;
	core.set_audio_sample_batch = set_audio_batch;
	core.set_input_poll         = set_poll;
	core.set_input_state        = set_input;
	sr_bridge_install(&core);
	CHECK(cb_video != NULL && cb_audio != NULL && cb_audio_batch != NULL);

	/* Not warming up: every frame and every sample reaches the door. This is
	 * the case that proves the counters work, so the silence below means the
	 * gate and not a broken harness. */
	emit_frame();
	CHECK(n_present == 1);
	CHECK(n_pcm_frames == 65);              /* the batch, plus the single */

	/* Warming up: nothing reaches either sink. */
	sr_bridge_set_warmup(1);
	n_present = 0; n_pcm_frames = 0;
	emit_frame();
	emit_frame();
	CHECK(n_present == 0);
	CHECK(n_pcm_frames == 0);

	/* ...but the input pump keeps running underneath. The terminal's probe
	 * replies (DA1/CTDA, the geometry report) arrive during startup, which is
	 * exactly when the warm-up is running: swallowing them would leave the door
	 * drawing text tiles to a client that had already said it does sixel. */
	CHECK(n_pump == 3);

	/* A batch must still be REPORTED as consumed. A core that is told fewer
	 * frames were taken than it offered may re-submit the remainder, so a gate
	 * that returned 0 would spin rather than skip. */
	{
		int16_t pcm[8 * 2];

		CHECK(cb_audio_batch(pcm, 8) == 8);
	}

	/* Released: the first frame of the game the player came for gets through. */
	sr_bridge_set_warmup(0);
	emit_frame();
	CHECK(n_present == 1);
	CHECK(n_pcm_frames == 65);

	printf("%s: %d failure(s)\n", failures ? "FAIL" : "ok", failures);
	return failures != 0;
}
