/* retro_bridge.c -- the four hot libretro callbacks, wired to termgfx + the
 * door I/O and input modules.
 *
 * The core calls these synchronously from inside retro_run() (DESIGN.md sec 3):
 *   video_refresh  -> convert framebuffer -> sr_io_present() (termgfx, paced)
 *   audio_batch    -> sr_audio_feed()      (M1: discarded, DESIGN.md sec 8)
 *   input_poll     -> sr_input_pump()      (non-blocking socket drain)
 *   input_state    -> sr_pad_get()         (cached RetroPad)
 */
#include "retro_core.h"
#include "syncretro.h"

#include <stdlib.h>
#include <string.h>

/* retro_env.c owns the environment callback. */
bool sr_environment(unsigned cmd, void *data);

static int      g_pixfmt = RETRO_PIXEL_FORMAT_0RGB1555;   /* libretro default */
static uint8_t *g_rgb;                                    /* RGB24 scratch */
static size_t   g_rgb_cap;

void sr_bridge_set_pixfmt(int fmt) { g_pixfmt = fmt; }

/* Convert one libretro framebuffer to top-down RGB24 in g_rgb.
 * TODO(M1): implement the three source formats:
 *   RETRO_PIXEL_FORMAT_XRGB8888 (0x00RRGGBB LE), _RGB565, _0RGB1555.
 * `pitch` is BYTES per source row (may exceed w*bpp -- honor it). */
static void sr_fb_to_rgb(const void *src, int fmt, unsigned w, unsigned h,
                         size_t pitch, uint8_t **out)
{
	size_t need = (size_t)w * h * 3;
	if (g_rgb_cap < need) {
		free(g_rgb);
		g_rgb = malloc(need);
		g_rgb_cap = g_rgb ? need : 0;
	}
	*out = g_rgb;
	(void)src; (void)fmt; (void)pitch;
	/* TODO(M1): the actual per-pixel unpack loop. */
}

static void video_refresh(const void *data, unsigned w, unsigned h, size_t pitch)
{
	uint8_t *rgb;
	if (data == NULL)                       /* "dup previous frame" -> nothing new */
		return;
	sr_fb_to_rgb(data, g_pixfmt, w, h, pitch, &rgb);
	if (rgb)
		sr_io_present(rgb, (int)w, (int)h);  /* termgfx compose + emit, AIMD-paced */
}

static void audio_sample(int16_t l, int16_t r)
{
	int16_t f[2] = { l, r };
	sr_audio_feed(f, 1);
}

static size_t audio_sample_batch(const int16_t *data, size_t frames)
{
	return sr_audio_feed(data, frames);
}

static void input_poll(void) { sr_input_pump(); }

static int16_t input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
	(void)index;
	return sr_pad_get(port, device, id);
}

void sr_bridge_install(rc_core_t *c)
{
	c->set_environment(sr_environment);
	c->set_video_refresh(video_refresh);
	c->set_audio_sample(audio_sample);
	c->set_audio_sample_batch(audio_sample_batch);
	c->set_input_poll(input_poll);
	c->set_input_state(input_state);
}

/* M1: accept and discard the core's PCM stream (keeps the core's timing model
 * happy). Streaming console audio to a terminal is DESIGN.md sec 8 / M4. */
size_t sr_audio_feed(const int16_t *pcm, size_t frames)
{
	(void)pcm;
	return frames;
}
