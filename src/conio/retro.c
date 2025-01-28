#include <string.h>

#define BITMAP_CIOLIB_DRIVER
#include "bitmap_con.h"
#include "ciolib.h"

#include "genwrap.h"

#include "libretro.h"

#define KEYBUFSIZE 32
static uint16_t keybuf[KEYBUFSIZE];
static size_t keybufremove = 0;
static size_t keybufadd = 0;
static size_t keybuffill = 0;

RETRO_CALLCONV void
retro_keyboard(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers)
{
	fprintf(stderr, "%d, %u, %" PRIu32 ", %" PRIu16 "\n", down, keycode, character, key_modifiers);
	if (down) {
		// TODO: Map properly...
		if (keybuffill < KEYBUFSIZE) {
			keybuf[keybufadd++] = keycode;
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

