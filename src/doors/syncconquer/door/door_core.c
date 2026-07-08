// SyncConquer door core: the public seam video_termgfx.cpp /
// wwkeyboard_termgfx.cpp call (door_present/door_pump/door_mouse_xy, door.h)
// -- kept as a thin, stable forwarder into door_io.c, where the socket
// setup, present/pacing/tier pipeline and input parsing actually live. The
// *names* here are final (Task-1 contract); the bodies won't grow again.
#include "door.h"
#include "door_io.h"
#include "door_input.h"

void door_present(const uint8_t *fb, const uint8_t *pal768)
{
	door_io_present(fb, pal768);
}

void door_pump(void)
{
	door_io_pump();
}

void door_mouse_xy(int *x, int *y)
{
	// The OS pointer is the cursor (DESIGN.md): the engine's own cursor
	// sprite stays suppressed (Set_Video_Cursor is a no-op; confirmed by
	// source trace -- WWMouseClass's software-cursor blit path is compiled
	// out entirely under NEW_VIDEO_BUILD, see the Task-3 report). door_input.c
	// tracks the pointer from SGR mouse reports mapped through the
	// displayed-image rect (door_io.c's geometry).
	door_input_mouse_xy(x, y);
}
