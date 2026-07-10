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

/* Convert one libretro framebuffer row-by-row to top-down RGB24 in g_rgb.
 *
 * `pitch` is BYTES per source row and routinely exceeds w*bpp -- cores hand out
 * a window into a larger buffer (FreeIntv's 352-pixel frame sits in a 352-wide
 * XRGB8888 buffer, pitch 1408) -- so every row is addressed through it rather
 * than by multiplying out the width.
 *
 * The two 16-bit formats are widened by REPLICATING the high bits into the low
 * ones (`v << 3 | v >> 2` for a 5-bit channel), not by shifting and leaving
 * zeros. Plain shifting maps a channel's maximum 0x1f to 0xf8, so a core's pure
 * white never reaches 0xff and its blacks and whites both drift -- visible as a
 * dingy picture. Replication maps 0 -> 0x00 and 0x1f -> 0xff exactly.
 *
 * Returns NULL on allocation failure (the caller then skips the frame). */
static uint8_t *sr_fb_to_rgb(const void *src, int fmt, unsigned w, unsigned h,
                             size_t pitch)
{
	size_t   need = (size_t)w * h * 3;
	unsigned x, y;

	if (g_rgb_cap < need) {
		uint8_t *p = (uint8_t *)realloc(g_rgb, need);

		if (p == NULL)
			return NULL;
		g_rgb     = p;
		g_rgb_cap = need;
	}

	for (y = 0; y < h; y++) {
		uint8_t *d = g_rgb + (size_t)y * w * 3;

		switch (fmt) {
			case RETRO_PIXEL_FORMAT_XRGB8888: {
				const uint32_t *s = (const uint32_t *)((const uint8_t *)src + (size_t)y * pitch);

				for (x = 0; x < w; x++) {
					uint32_t px = s[x];        /* 0x00RRGGBB, host byte order */

					*d++ = (uint8_t)(px >> 16);
					*d++ = (uint8_t)(px >> 8);
					*d++ = (uint8_t)px;
				}
				break;
			}
			case RETRO_PIXEL_FORMAT_RGB565: {
				const uint16_t *s = (const uint16_t *)((const uint8_t *)src + (size_t)y * pitch);

				for (x = 0; x < w; x++) {
					unsigned px = s[x];
					unsigned r = (px >> 11) & 0x1f, g = (px >> 5) & 0x3f, b = px & 0x1f;

					*d++ = (uint8_t)((r << 3) | (r >> 2));
					*d++ = (uint8_t)((g << 2) | (g >> 4));
					*d++ = (uint8_t)((b << 3) | (b >> 2));
				}
				break;
			}
			case RETRO_PIXEL_FORMAT_0RGB1555:
			default: {
				const uint16_t *s = (const uint16_t *)((const uint8_t *)src + (size_t)y * pitch);

				for (x = 0; x < w; x++) {
					unsigned px = s[x];
					unsigned r = (px >> 10) & 0x1f, g = (px >> 5) & 0x1f, b = px & 0x1f;

					*d++ = (uint8_t)((r << 3) | (r >> 2));
					*d++ = (uint8_t)((g << 3) | (g >> 2));
					*d++ = (uint8_t)((b << 3) | (b >> 2));
				}
				break;
			}
		}
	}
	return g_rgb;
}

static void video_refresh(const void *data, unsigned w, unsigned h, size_t pitch)
{
	uint8_t *rgb;

	if (data == NULL)                       /* "dup previous frame" -> nothing new */
		return;
	rgb = sr_fb_to_rgb(data, g_pixfmt, w, h, pitch);
	if (rgb != NULL)
		sr_io_present(rgb, (int)w, (int)h);  /* termgfx compose + emit, DSR-ACK paced */
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
	return sr_pad_get(port, device, index, id);
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
