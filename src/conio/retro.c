#include <string.h>

#define BITMAP_CIOLIB_DRIVER
#include "bitmap_con.h"
#include "ciolib.h"

#include "genwrap.h"

#include "libretro.h"

#define KEYBUFSIZE 128
static uint8_t keybuf[KEYBUFSIZE];
static size_t keybufremove = 0;
static size_t keybufadd = 0;
static size_t keybuffill = 0;

struct keyvals {
	unsigned keycode;
	uint16_t key;
	uint16_t shift;
	uint16_t ctrl;
	uint16_t alt;
};

static const struct keyvals keyval[] =
{
	{RETROK_BACKSPACE, 0x08, 0x08, 0x7f, 0x0e00},
	{RETROK_TAB, 0x09, 0x0f00, 0x9400, 0xa500},
	{RETROK_RETURN, 0x0d, 0x0d, 0x0a, 0xa600},
	{RETROK_ESCAPE, 0x1b, 0x1b, 0x1b, 0x0100},
	{RETROK_SPACE, 0x20, 0x20, 0x0300, 0x20,},
	{RETROK_QUOTE, '\'', '"', 0, 0x2800},
	{RETROK_COMMA, ',', '<', 0, 0x3300},
	{RETROK_MINUS, '-', '_', 0x1f, 0x8200},
	{RETROK_PERIOD, '.', '>', 0, 0x3400},
	{RETROK_SLASH, '/', '?', 0, 0x3500},
	{RETROK_0, '0', ')', 0, 0x8100},
	{RETROK_1, '1', '!', 0, 0x7800},
	{RETROK_2, '2', '@', 0x0300, 0x7900},
	{RETROK_3, '3', '#', 0, 0x7a00},
	{RETROK_4, '4', '$', 0, 0x7b00},
	{RETROK_5, '5', '%', 0, 0x7c00},
	{RETROK_6, '6', '^', 0x1e, 0x7d00},
	{RETROK_7, '7', '&', 0, 0x7e00},
	{RETROK_8, '8', '*', 0, 0x7f00},
	{RETROK_9, '9', '(', 0, 0x8000},

	{RETROK_SEMICOLON, ';', ':', 0, 0x2700},
	{RETROK_EQUALS, '=', '+', 0, 0x8300},
	{RETROK_LEFTBRACKET, '[', '{', 0x1b, 0x1a00},
	{RETROK_BACKSLASH, '\\', '|', 0x1c, 0x2b00},
	{RETROK_RIGHTBRACKET, ']', '}', 0x1d, 0x1b00},
	{RETROK_BACKQUOTE, '`', '~', 0, 0x2900},

	{RETROK_a, 'a', 'A', 0x01, 0x1e00},
	{RETROK_b, 'b', 'B', 0x02, 0x3000},
	{RETROK_c, 'c', 'C', 0x03, 0x2e00},
	{RETROK_d, 'd', 'D', 0x04, 0x2000},
	{RETROK_e, 'e', 'E', 0x05, 0x1200},
	{RETROK_f, 'f', 'F', 0x06, 0x2100},
	{RETROK_g, 'g', 'G', 0x07, 0x2200},
	{RETROK_h, 'h', 'H', 0x08, 0x2300},
	{RETROK_i, 'i', 'I', 0x09, 0x1700},
	{RETROK_j, 'j', 'J', 0x0a, 0x2400},
	{RETROK_k, 'k', 'K', 0x0b, 0x2500},
	{RETROK_l, 'l', 'L', 0x0c, 0x2600},
	{RETROK_m, 'm', 'M', 0x0d, 0x3200},
	{RETROK_n, 'n', 'N', 0x0e, 0x3100},
	{RETROK_o, 'o', 'O', 0x0f, 0x1800},
	{RETROK_p, 'p', 'P', 0x10, 0x1900},
	{RETROK_q, 'q', 'Q', 0x11, 0x1000},
	{RETROK_r, 'r', 'R', 0x12, 0x1300},
	{RETROK_s, 's', 'S', 0x13, 0x1f00},
	{RETROK_t, 't', 'T', 0x14, 0x1400},
	{RETROK_u, 'u', 'U', 0x15, 0x1600},
	{RETROK_v, 'v', 'V', 0x16, 0x2f00},
	{RETROK_w, 'w', 'W', 0x17, 0x1100},
	{RETROK_x, 'x', 'X', 0x18, 0x2d00},
	{RETROK_y, 'y', 'Y', 0x19, 0x1500},
	{RETROK_z, 'z', 'Z', 0x1a, 0x2c00},

	{RETROK_DELETE, CIO_KEY_DC, CIO_KEY_SHIFT_DC, CIO_KEY_CTRL_DC, CIO_KEY_CTRL_IC},

	{RETROK_KP0, '0', 0x5200, 0x9200, 0},
	{RETROK_KP1, '1', 0x4f00, 0x7500, 0},
	{RETROK_KP2, '2', 0x5000, 0x9100, 0},
	{RETROK_KP3, '3', 0x5100, 0x7600, 0},
	{RETROK_KP4, '4', 0x4b00, 0x7300, 0},
	{RETROK_KP5, '5', 0x4c00, 0x8f00, 0},
	{RETROK_KP6, '6', 0x4d00, 0x7400, 0},
	{RETROK_KP7, '7', 0x4700, 0x7700, 0},
	{RETROK_KP8, '8', 0x4800, 0x8d00, 0},
	{RETROK_KP9, '9', 0x4900, 0x8400, 0},
	{RETROK_KP_PERIOD, '.', '.', 0x5300, 0x9300},
	{RETROK_KP_DIVIDE, '/', '/', 0x9500, 0xa400},
	{RETROK_KP_MULTIPLY, '*', '*', 0x9600, 0x3700},
	{RETROK_KP_MINUS, '-', '-', 0x8e00, 0x4a00},
	{RETROK_KP_PLUS, '+', '+', 0x9000, 0x4e00},

	{RETROK_UP, 0x4800, 0x4800, 0x8d00, 0x9800},
	{RETROK_DOWN, 0x5000, 0x5000, 0x9100, 0xa000},
	{RETROK_RIGHT, 0x4d00, 0x4d00, 0x7400, 0x9d00},
	{RETROK_LEFT, 0x4b00, 0x4b00, 0x7300, 0x9b00},
	{RETROK_INSERT, CIO_KEY_IC, CIO_KEY_SHIFT_IC, CIO_KEY_CTRL_IC, CIO_KEY_ALT_IC},
	{RETROK_HOME, 0x4700, 0x4700, 0x7700, 0x9700},
	{RETROK_END, 0x4f00, 0x4f00, 0x7500, 0x9f00},
	{RETROK_PAGEUP, 0x4900, 0x4900, 0x8400, 0x9900},
	{RETROK_PAGEDOWN, 0x5100, 0x5100, 0x7600, 0xa100},

	{RETROK_F1, 0x3b00, 0x5400, 0x5e00, 0x6800},
	{RETROK_F2, 0x3c00, 0x5500, 0x5f00, 0x6900},
	{RETROK_F3, 0x3d00, 0x5600, 0x6000, 0x6a00},
	{RETROK_F4, 0x3e00, 0x5700, 0x6100, 0x6b00},
	{RETROK_F5, 0x3f00, 0x5800, 0x6200, 0x6c00},
	{RETROK_F6, 0x4000, 0x5900, 0x6300, 0x6d00},
	{RETROK_F7, 0x4100, 0x5a00, 0x6400, 0x6e00},
	{RETROK_F8, 0x4200, 0x5b00, 0x6500, 0x6f00},
	{RETROK_F9, 0x4300, 0x5c00, 0x6600, 0x7000},
	{RETROK_F10, 0x4400, 0x5d00, 0x6700, 0x7100},
	{RETROK_F11, 0x8500, 0x8700, 0x8900, 0x8b00},
	{RETROK_F12, 0x8600, 0x8800, 0x8a00, 0x8c00},

};

int
retro_keyval_cmp(const void *key, const void *memb)
{
	const unsigned *k = key;
	const struct keyvals *m = memb;

	return (int64_t)*k - m->keycode;
}

RETRO_CALLCONV void
retro_keyboard(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers)
{
	uint16_t add = 0;

	if (down) {
		// TODO: Map properly...
		if (keybuffill < KEYBUFSIZE) {
			struct keyvals *k = bsearch(&keycode, keyval, sizeof(keyval) / sizeof(keyval[0]), sizeof(keyval[0]), retro_keyval_cmp);

			if (k) {
				if (key_modifiers & RETROKMOD_ALT)
					add = k->alt;
				else if (key_modifiers & RETROKMOD_CTRL)
					add = k->ctrl;
				else if (key_modifiers & RETROKMOD_SHIFT)
					add = k->shift;
				else
					add = k->key;
			}
		}
	}
	if (add && keybuffill < KEYBUFSIZE - 2) {
		keybuf[keybufadd++] = add & 0xff;
		keybuffill++;
		if (add > 255) {
			keybuf[keybufadd++] = add >> 8;
			keybuffill++;
		}
	}
}

static retro_environment_t env_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static enum retro_pixel_format rpf = RETRO_PIXEL_FORMAT_XRGB8888;
static struct retro_keyboard_callback rkc = {
	.callback = retro_keyboard
};
static bool no_game = true;
bool retro_set = false;

int main(int argc, char **argv);
static void
runmain(void *arg)
{
	static char *args[] = {
		"syncterm"
	};
	main(0, args);
}

struct rectlist *last_rect = NULL;

void
retro_drawrect(struct rectlist *data)
{
	if (last_rect != NULL)
		bitmap_drv_free_rect(last_rect);
	last_rect = data;
}

void
retro_flush(void)
{
}

RETRO_API void
retro_init(void)
{
	_beginthread(runmain, 0, NULL);
	bitmap_drv_init(retro_drawrect, retro_flush);
}

RETRO_API void
retro_deinit(void)
{
	/*
	 * "The core must release all of its allocated resources before this function returns."
	 * Heh heh heh... sure thing bud.
	 */
	for (;;)
		SLEEP(1000);
}

RETRO_API unsigned
retro_api_version(void)
{
	return RETRO_API_VERSION;
}


RETRO_API void
retro_set_controller_port_device(unsigned port, unsigned device)
{
}

RETRO_API void
retro_get_system_info(struct retro_system_info *info)
{
	memset(info, 0, sizeof(struct retro_system_info));
	info->library_name = ciolib_initial_program_class;
	info->library_version = "0.1";
}

RETRO_API void
retro_get_system_av_info(struct retro_system_av_info* info)
{
	memset(info, 0, sizeof(struct retro_system_av_info));
	info->timing.fps = 60.0f;
	info->timing.sample_rate = 44100; // standard audio sample rate
	info->geometry.base_width = 640;
	info->geometry.base_height = 400;
	info->geometry.max_width = 640;
	info->geometry.max_height = 400;
	info->geometry.aspect_ratio = (float)4 / (float)3;
}

static struct retro_input_descriptor rid[] = {
};

RETRO_API void
retro_set_environment(retro_environment_t cb)
{
	env_cb = cb;
	retro_set = true;
	cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rpf);
	cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &rkc);
	cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
	// RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK
	// RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK
}

RETRO_API void
retro_set_audio_sample(retro_audio_sample_t cb)
{
	audio_cb = cb;
}

RETRO_API void
retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
	audio_batch_cb = cb;
}

RETRO_API void
retro_set_input_poll(retro_input_poll_t cb)
{
	input_poll_cb = cb;
}

RETRO_API void
retro_set_input_state(retro_input_state_t cb)
{
	input_state_cb = cb;
}

RETRO_API void
retro_set_video_refresh(retro_video_refresh_t cb)
{
	video_cb = cb;
}

RETRO_API void
retro_reset(void)
{
}

RETRO_API void
audio_callback(void)
{
}

RETRO_API void
retro_run(void)
{
	video_cb(last_rect->data, last_rect->rect.width, last_rect->rect.height, last_rect->rect.width * sizeof(uint32_t));
}

RETRO_API size_t
retro_serialize_size(void)
{
	return 0;
}

RETRO_API bool
retro_serialize(void *data_, size_t size)
{
	return false;
}

RETRO_API bool
retro_unserialize(const void *data_, size_t size)
{
	return false;
}

RETRO_API void
retro_cheat_reset(void)
{
}

RETRO_API void
retro_cheat_set(unsigned index, bool enabled, const char *code)
{
}

RETRO_API bool
retro_load_game(const struct retro_game_info *info)
{
	return true;
}

RETRO_API bool
retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
	return false;
}

RETRO_API void
retro_unload_game(void)
{
}

RETRO_API unsigned
retro_get_region(void)
{
	return RETRO_REGION_NTSC;
}

RETRO_API void *
retro_get_memory_data(unsigned id)
{
   return NULL;
}

RETRO_API size_t
retro_get_memory_size(unsigned id)
{
   return 0;
}

////////

void
retro_beep(void)
{
}

int
retro_kbhit(void)
{
	return keybuffill;
}

int
retro_getch(void)
{
	if (keybuffill == 0)
		return 0;
	int ret = keybuf[keybufremove];
	keybufremove++;
	keybuffill--;
	return ret;
}

void
retro_textmode(int mode)
{
	assert_rwlock_wrlock(&vstatlock);
	bitmap_drv_init_mode(mode, NULL, NULL, 0, 0);
	assert_rwlock_unlock(&vstatlock);
	bitmap_drv_request_pixels();

	return;
}

////////

