/* syncretro_input.c -- BBS socket decode -> a cached RetroPad state.
 *
 * sr_input_pump() is the retro_input_poll body: a NON-BLOCKING drain of the
 * client socket, decoding ESC/CSI/APC/kitty-keyboard sequences (via the termgfx
 * input decode) into a RetroPad bitfield + axes, and routing terminal probe
 * replies to sr_io_feed_reply(). sr_pad_get() is the retro_input_state body --
 * it just reads the cache. See DESIGN.md sec 7. Stub -- M1 fills these in.
 */
#include "syncretro.h"
#include "libretro.h"    /* RETRO_DEVICE_* / RETRO_DEVICE_ID_JOYPAD_* */

#include <string.h>

/* Cached RetroPad: one port for M1. Index by RETRO_DEVICE_ID_JOYPAD_*. */
static int16_t g_joypad[RETRO_DEVICE_ID_JOYPAD_R3 + 1];

void sr_input_pump(void)
{
	/* TODO(M1):
	 *   - non-blocking read from the door socket (sr_door_socket());
	 *   - run bytes through the ESC/CSI/APC/kitty state machine (shared with the
	 *     termgfx input decode, as ../syncmoo1/syncmoo1_input.c does);
	 *   - terminal replies (cursor-position/DA/caps) -> sr_io_feed_reply();
	 *   - key press/release -> update g_joypad[] via the per-core key map
	 *     (RETRO_DEVICE_ID_JOYPAD_UP/DOWN/A/B/START/... + the Intellivision
	 *     keypad overlay, DESIGN.md sec 7);
	 *   - enforce idle-disconnect ourselves (this bypasses getkey()'s timer). */
}

int16_t sr_pad_get(unsigned port, unsigned device, unsigned id)
{
	if (port != 0 || device != RETRO_DEVICE_JOYPAD)
		return 0;
	if (id >= (sizeof(g_joypad) / sizeof(g_joypad[0])))
		return 0;
	return g_joypad[id];
}
