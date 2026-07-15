/* Terminal-rendering graphics manager: extends the M1 dump manager (so
 * SYNCSCUMM_DUMP keeps working) and forwards each changed frame to the
 * sst_io present path when a terminal session is active. M2 Task 5 adds
 * server-side cursor compositing: the game surface (_screen, inherited)
 * stays untouched -- dumps and future readers of it see game pixels only
 * -- while updateScreen() presents a separate _composed surface that is
 * _screen plus a key-colored cursor blit. M2 Task 6 adds the GUI overlay:
 * when visible, compose() builds an RGB888 frame (game + overlay + cursor)
 * and quantizes it to 256 colors for presentation; the CLUT8 path from
 * Task 5 is untouched when the overlay is hidden. */
#ifndef SYNCSCUMM_VIDEO_TERM_H_
#define SYNCSCUMM_VIDEO_TERM_H_

#include "video_dump.h"

extern "C" {
#include "sst_io.h"
}

class SyncscummTermGraphicsManager : public SyncscummDumpGraphicsManager {
public:
	SyncscummTermGraphicsManager();
	~SyncscummTermGraphicsManager() override;
	void updateScreen() override;
	void setMouseCursor(const void *buf, uint w, uint h, int hotspotX, int hotspotY,
	                    uint32 keycolor, bool dontScale = false,
	                    const Graphics::PixelFormat *format = NULL,
	                    const byte *mask = NULL) override;
	void setCursorPalette(const byte *colors, uint start, uint num) override;
	bool showMouse(bool visible) override;
	void warpMouse(int x, int y) override;

	Graphics::PixelFormat getOverlayFormat() const override { return Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0); }
	void showOverlay(bool inGUI) override;
	void hideOverlay() override;
	void clearOverlay() override;
	void grabOverlay(Graphics::Surface &surface) const override;
	void copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h) override;
	int16 getOverlayHeight() const override { return SST_FB_H; }
	int16 getOverlayWidth() const override { return SST_FB_W; }

private:
	void compose();
	/* Lazily create the RGB565 overlay surface on first use; shared by
	 * showOverlay()/clearOverlay()/copyRectToOverlay() so the create geometry
	 * and format live in exactly one place. */
	void ensureOverlay();

	Graphics::Surface _composed;         /* game + cursor (+ overlay, Task 6) */
	/* RGB888 scratch for the overlay composite (game + overlay + cursor before
	 * quantization). A member, not a compose()-local: at SST_FB_W*SST_FB_H*3 =
	 * 192000 bytes it would be a large automatic on whatever thread drives
	 * updateScreen(), so it lives with the (heap-allocated) manager instead. */
	byte _composeRgb[SST_FB_W * SST_FB_H * 3];
	byte _cursorPal[256 * 3];
	byte *_cursorBuf;
	uint _cursorW, _cursorH;
	int _cursorHotX, _cursorHotY;
	uint32 _cursorKey;
	bool _cursorVisible, _cursorUsePal;
	int _cursorX, _cursorY;

	/* Last-presented cursor state, so a pure cursor move/show/hide change
	 * is detected as dirty even when _dirty (game-surface changes) is
	 * false -- see updateScreen()'s dump-vs-present split. */
	int _lastCursorX, _lastCursorY;
	bool _lastCursorShown;

	/* Set by setMouseCursor() (a new sprite, independent of position),
	 * cleared once that sprite has been presented -- covers e.g. cursor
	 * animation where the hotspot doesn't move but the image does. */
	bool _cursorSpriteDirty;

	/* GUI overlay (Task 6): RGB565, always SST_FB_W x SST_FB_H regardless
	 * of the game's own resolution -- matches getOverlayWidth/Height()
	 * above, which the GUI theme engine sizes itself against. Created
	 * lazily on first use (showOverlay() or copyRectToOverlay()). */
	Graphics::Surface _overlay;
	byte _quantIdx[SST_FB_W * SST_FB_H];
	byte _quantPal[768];

	/* Mirrors the cursor's _lastCursor.../_cursorSpriteDirty pattern: a
	 * pure overlay show/hide or content redraw must trigger a present even
	 * when the game surface (_dirty) hasn't changed -- but, unlike setting
	 * the base's _dirty would, must NOT trigger a PPM dump frame
	 * (dumpFrame() is game-surface-only; see SyncscummDumpGraphicsManager's
	 * header comment). */
	bool _lastOverlayShown;
	bool _overlayContentDirty;
};

#endif /* SYNCSCUMM_VIDEO_TERM_H_ */
