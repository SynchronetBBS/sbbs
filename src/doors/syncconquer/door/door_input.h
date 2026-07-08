// door_input.h -- terminal input parser: CSI/SS3 keys, kitty keyboard protocol,
// SyncTERM evdev physical-key reports, and SGR mouse -- decoded from the bytes
// door_io.c's door_input_byte() ring buffer collects (door.h's Task-2 hook)
// into the small event queue wwkeyboard_termgfx.cpp drains and injects into
// the engine (Put_Key_Message/Put_Mouse_Message, per wwkeyboard_sdl2.cpp's
// contract). Kept a plain-C module (unlike the wwkeyboard_*.cpp shims) so it
// stays engine/C++-independent and unit-testable on its own -- it never
// includes wwkeyboard.h; the VK_* bit values it emits are plain integers that
// match wwkeyboard.h's WWKey_Type/VK_* numbering by convention (asserted by
// wwkeyboard_termgfx.cpp's use of them, not by a shared header).
#ifndef DOOR_INPUT_H_
#define DOOR_INPUT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Drain door_io.c's Task-2 ring buffer (door_io_input_pop()) and turn whatever
// bytes are waiting into zero or more queued door_input_event_t values (below).
// Call once per Fill_Buffer_From_System() poll, right after door_pump() --
// door_pump() is what actually reads the socket and feeds the ring.
void door_input_drain(void);

enum door_input_kind {
	DOOR_IN_KEY,     // vk (+ release) -- inject via Put_Key_Message
	DOOR_IN_MOUSE,   // vk is VK_LBUTTON/RBUTTON/MBUTTON, x/y are game coords -- Put_Mouse_Message
};

typedef struct {
	enum door_input_kind kind;
	unsigned short vk;              // Win32-style VK_* code, WWKEY_*_BIT already OR'd in as needed
	int release;                    // 1 = key/button up, 0 = down
	int x, y;                       // DOOR_IN_MOUSE only: 0..DOOR_FB_WIDTH-1 / DOOR_FB_HEIGHT-1
} door_input_event_t;

// Pop one decoded event in arrival order. Returns 0 (and leaves *ev untouched)
// once the queue is empty.
int door_input_next_event(door_input_event_t *ev);

// The tracked pointer, in engine screen coordinates -- door_core.c's
// door_mouse_xy() (door.h, called by Get_Video_Mouse) forwards here. Starts
// at the framebuffer center (matching the Task-2 stub) until the first SGR
// mouse report arrives.
void door_input_mouse_xy(int *x, int *y);

// True once the kitty keyboard protocol has been negotiated (a CSI?u reply
// landed and we pushed our flags). door_io.c's exit-time terminal restore
// calls this to decide whether to pop them (CSI<u).
int door_input_kitty_active(void);

// Shared VK_*<->ASCII table (the sdl_keymap.h equivalent for our plain
// Win32-style VK_* space, wwkeyboard.h's non-SDL branch): a single source of
// truth used both to encode legacy terminal bytes into VK codes here and to
// decode them back in wwkeyboard_termgfx.cpp's To_ASCII. Returns 0 for a key
// with no ASCII meaning (arrows, F-keys, ...), matching KA_NONE.
int door_input_ascii_from_vk(unsigned short vk, int shifted);

// Reverse of the above: an ASCII byte -> its VK_* code (0 if none) and
// whether the shift bit is needed to produce it.
unsigned short door_input_vk_from_ascii(int ch, int *shift_out);

#ifdef __cplusplus
}
#endif

#endif // DOOR_INPUT_H_
