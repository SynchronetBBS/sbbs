// NEW_VIDEO_BUILD backend glue: the three free functions the null video
// branch needs beyond video_null.cpp (see plans/m2-proto/FACTS.md #2) --
// wired to the door instead of a window. Video_Render_Frame is reached via
// Frame_Limiter() (common/framelimit.cpp) every rendered frame.
#include "gbuffer.h"
#include "palette.h"
#include "door.h"

#include <cstring>

extern GraphicBufferClass VisiblePage;

// The engine's GraphicViewPortClass::Fill_Rect has a "hardware blit" fast path
// for fills of >= 32x32 pixels: when AllowHardwareBlitFills is set and the
// surface IsHardware (which the video surface is flagged as), it calls
// Get_DD_Surface()->FillRect() instead of the software Buffer_Fill_Rect(). Our
// null/termgfx video backend's surface is video_null.cpp's VideoSurfaceDummy,
// whose FillRect() is a no-op -- so every large fill is SILENTLY DROPPED, while
// smaller fills and Draw_Rect/Draw_Line borders (always software) still render.
// The visible symptom is dialog BACKGROUNDS vanishing (a full-dialog Fill_Rect
// is large) while their borders + gadgets remain, so the screen behind shows
// through. There is no real hardware surface here, so force every fill through
// the software path. Set at runtime (not a static initializer) to avoid any
// init-order dependency on the vendored `AllowHardwareBlitFills = true` global.
extern bool AllowHardwareBlitFills;

void Get_Video_Mouse(int& x, int& y)
{
	door_mouse_xy(&x, &y);
}

// The door tracks the player's pointer via the terminal's own reporting
// (SGR mouse), not an engine-drawn sprite -- deliberately a no-op, same as
// the m2-proto prototype.
void Set_Video_Cursor(void* cursor, int w, int h, int hotx, int hoty)
{
}

void Video_Render_Frame()
{
	static uint8_t framebuf[DOOR_FB_WIDTH * DOOR_FB_HEIGHT];

	AllowHardwareBlitFills = false;   // software fills only (see note above)

	int            w = VisiblePage.Get_Width();
	int            h = VisiblePage.Get_Height();

	if (!VisiblePage.Lock()) {
		return;
	}

	const uint8_t* px = reinterpret_cast<const uint8_t*>(VisiblePage.Get_Offset());
	bool           have_frame = (px != nullptr && w == DOOR_FB_WIDTH && h == DOOR_FB_HEIGHT);
	if (have_frame) {
		memcpy(framebuf, px, sizeof(framebuf));
	}
	VisiblePage.Unlock();

	if (have_frame) {
		door_present(framebuf, CurrentPalette);
	}
}
