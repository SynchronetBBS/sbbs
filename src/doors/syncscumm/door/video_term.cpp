#include "common/scummsys.h"

#if defined(USE_SYNCHRONET_DRIVER)

#include <stdlib.h>
#include <string.h>

#include "video_term.h"

#include "audio_term.h"
#include "termgfx_quant.h"

SyncscummTermGraphicsManager::SyncscummTermGraphicsManager()
	: _cursorBuf(NULL), _cursorW(0), _cursorH(0),
	  _cursorHotX(0), _cursorHotY(0), _cursorKey(0),
	  _cursorVisible(false), _cursorUsePal(false),
	  _cursorX(0), _cursorY(0),
	  _lastCursorX(0), _lastCursorY(0), _lastCursorShown(false),
	  _cursorSpriteDirty(false),
	  _lastOverlayShown(false), _overlayContentDirty(false) {
	memset(_cursorPal, 0, sizeof(_cursorPal));
	memset(_quantIdx, 0, sizeof(_quantIdx));
	memset(_quantPal, 0, sizeof(_quantPal));
}

SyncscummTermGraphicsManager::~SyncscummTermGraphicsManager() {
	free(_cursorBuf);
	_composed.free();
	_overlay.free();
}

void SyncscummTermGraphicsManager::setMouseCursor(const void *buf, uint w, uint h,
                                                   int hotspotX, int hotspotY, uint32 keycolor,
                                                   bool dontScale, const Graphics::PixelFormat *format,
                                                   const byte *mask) {
	/* All cursor features (kFeatureCursorPalette et al.) report false, so
	 * no engine should ever hand us a non-CLUT8 cursor or a mask -- both
	 * are asserted/ignored rather than handled. May be called before
	 * initSize()/any showMouse() -- just buffers the sprite. */
	assert(!format || format->isCLUT8());
	(void)dontScale;
	(void)mask;

	if (w != _cursorW || h != _cursorH || !_cursorBuf) {
		byte *newBuf = (byte *)realloc(_cursorBuf, (size_t)w * h);
		if (!newBuf && w && h)
			return;   /* OOM: keep the previous cursor rather than crash */
		_cursorBuf = newBuf;
	}
	_cursorW = w;
	_cursorH = h;
	if (buf && w && h)
		memcpy(_cursorBuf, buf, (size_t)w * h);
	_cursorHotX = hotspotX;
	_cursorHotY = hotspotY;
	_cursorKey = keycolor;
	_cursorSpriteDirty = true;
}

void SyncscummTermGraphicsManager::setCursorPalette(const byte *colors, uint start, uint num) {
	/* kFeatureCursorPalette is false, so no shipped engine calls this --
	 * store it anyway (future-proofing) without acting on it; the cursor
	 * is composited through the game palette (_palette) in compose(). */
	memcpy(_cursorPal + start * 3, colors, num * 3);
	_cursorUsePal = true;
}

bool SyncscummTermGraphicsManager::showMouse(bool visible) {
	bool old = _cursorVisible;
	_cursorVisible = visible;
	return old;
}

void SyncscummTermGraphicsManager::warpMouse(int x, int y) {
	if (x < 0)
		x = 0;
	else if (x >= TERMGFX_TERMIO_FB_W)
		x = TERMGFX_TERMIO_FB_W - 1;
	if (y < 0)
		y = 0;
	else if (y >= TERMGFX_TERMIO_FB_H)
		y = TERMGFX_TERMIO_FB_H - 1;
	_cursorX = x;
	_cursorY = y;
}

void SyncscummTermGraphicsManager::ensureOverlay() {
	if (!_overlay.getPixels())
		_overlay.create(TERMGFX_TERMIO_FB_W, TERMGFX_TERMIO_FB_H, getOverlayFormat());
}

void SyncscummTermGraphicsManager::showOverlay(bool inGUI) {
	NullGraphicsManager::showOverlay(inGUI);   /* sets the base's _overlayVisible */
	ensureOverlay();
}

void SyncscummTermGraphicsManager::hideOverlay() {
	NullGraphicsManager::hideOverlay();
}

void SyncscummTermGraphicsManager::clearOverlay() {
	ensureOverlay();
	if (_overlay.getPixels())
		memset(_overlay.getPixels(), 0, (size_t)_overlay.pitch * _overlay.h);
	_overlayContentDirty = true;
}

void SyncscummTermGraphicsManager::grabOverlay(Graphics::Surface &surface) const {
	if (!_overlay.getPixels())
		return;
	assert(surface.w >= _overlay.w);
	assert(surface.h >= _overlay.h);
	assert(surface.format.bytesPerPixel == _overlay.format.bytesPerPixel);
	const byte *src = (const byte *)_overlay.getPixels();
	byte *dst = (byte *)surface.getPixels();
	for (int row = 0; row < _overlay.h; row++)
		memcpy(dst + row * surface.pitch, src + row * _overlay.pitch,
		       (size_t)_overlay.w * _overlay.format.bytesPerPixel);
}

void SyncscummTermGraphicsManager::copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h) {
	ensureOverlay();
	if (!_overlay.getPixels())
		return;
	assert(x >= 0 && x < TERMGFX_TERMIO_FB_W);
	assert(y >= 0 && y < TERMGFX_TERMIO_FB_H);
	assert(h > 0 && y + h <= TERMGFX_TERMIO_FB_H);
	assert(w > 0 && x + w <= TERMGFX_TERMIO_FB_W);
	const byte *src = (const byte *)buf;
	for (int row = 0; row < h; row++)
		memcpy(_overlay.getBasePtr(x, y + row), src + row * pitch, (size_t)w * _overlay.format.bytesPerPixel);
	_overlayContentDirty = true;
}

void SyncscummTermGraphicsManager::compose() {
	if (!_composed.getPixels())
		_composed.create(TERMGFX_TERMIO_FB_W, TERMGFX_TERMIO_FB_H, Graphics::PixelFormat::createFormatCLUT8());

	if (_screen.getPixels())
		memcpy(_composed.getPixels(), _screen.getPixels(), (size_t)TERMGFX_TERMIO_FB_W * TERMGFX_TERMIO_FB_H);

	bool cursorOn = _cursorVisible && _cursorBuf;
	int ox = _cursorX - _cursorHotX, oy = _cursorY - _cursorHotY;

	if (!isOverlayVisible()) {
		/* Task 4-5 CLUT8 path, unchanged: composite the cursor straight
		 * into _composed (indices, through the game palette at present
		 * time) and stop -- _composed + _palette is what gets presented. */
		if (!cursorOn)
			return;
		for (uint cy = 0; cy < _cursorH; cy++) {
			int ty = oy + (int)cy;
			if (ty < 0 || ty >= TERMGFX_TERMIO_FB_H)
				continue;
			const byte *src = _cursorBuf + cy * _cursorW;
			byte *dst = (byte *)_composed.getBasePtr(0, ty);
			for (uint cx = 0; cx < _cursorW; cx++) {
				int tx = ox + (int)cx;
				if (tx < 0 || tx >= TERMGFX_TERMIO_FB_W)
					continue;
				if (src[cx] != (byte)_cursorKey)
					dst[tx] = src[cx];
			}
		}
		return;
	}

	/* Overlay visible: build an RGB888 composite -- game under (expanded
	 * through _palette, from the CLUT8 _composed built above), overlay
	 * over (RGB565 expanded; the GUI draws opaque, no per-pixel alpha in
	 * v1) -- blit the cursor in RGB too, then quantize to 256 colors for
	 * presentation. _composed/_palette themselves are not presented while
	 * the overlay is up; _quantIdx/_quantPal are. */
	byte *rgb = _composeRgb;   /* member scratch, not a 192KB automatic */
	const byte *game = (const byte *)_composed.getPixels();
	for (int i = 0; i < TERMGFX_TERMIO_FB_W * TERMGFX_TERMIO_FB_H; i++) {
		const byte *p = _palette + game[i] * 3;
		rgb[i * 3 + 0] = p[0];
		rgb[i * 3 + 1] = p[1];
		rgb[i * 3 + 2] = p[2];
	}

	if (_overlay.getPixels()) {
		for (int y = 0; y < TERMGFX_TERMIO_FB_H; y++) {
			const byte *srow = (const byte *)_overlay.getBasePtr(0, y);
			byte *drow = rgb + (size_t)y * TERMGFX_TERMIO_FB_W * 3;
			for (int x = 0; x < TERMGFX_TERMIO_FB_W; x++) {
				uint16 px = *(const uint16 *)(srow + x * 2);
				byte r, g, b;
				_overlay.format.colorToRGB(px, r, g, b);
				drow[x * 3 + 0] = r;
				drow[x * 3 + 1] = g;
				drow[x * 3 + 2] = b;
			}
		}
	}

	if (cursorOn) {
		for (uint cy = 0; cy < _cursorH; cy++) {
			int ty = oy + (int)cy;
			if (ty < 0 || ty >= TERMGFX_TERMIO_FB_H)
				continue;
			const byte *src = _cursorBuf + cy * _cursorW;
			byte *drow = rgb + (size_t)ty * TERMGFX_TERMIO_FB_W * 3;
			for (uint cx = 0; cx < _cursorW; cx++) {
				int tx = ox + (int)cx;
				if (tx < 0 || tx >= TERMGFX_TERMIO_FB_W)
					continue;
				if (src[cx] != (byte)_cursorKey) {
					const byte *p = _palette + src[cx] * 3;
					drow[tx * 3 + 0] = p[0];
					drow[tx * 3 + 1] = p[1];
					drow[tx * 3 + 2] = p[2];
				}
			}
		}
	}

	termgfx_quant_rgb(rgb, TERMGFX_TERMIO_FB_W, TERMGFX_TERMIO_FB_H, _quantIdx, _quantPal);
}

void SyncscummTermGraphicsManager::updateScreen() {
	/* BASS's intro drives updateScreen() in a tight loop without ever
	 * pumping events, so a pollEvent-only tick would starve audio during
	 * the exact cutscene M4 exists for. tick() is cheap and internally
	 * rate-limited by the wall clock, so ticking from both sites is safe.
	 * Reached via the module's own pointer, not g_system: getMixerManager()
	 * is ModularBackend's, not OSystem's. Ahead of the no-change early
	 * returns below -- a still frame is exactly when audio must keep
	 * flowing. */
	if (g_syncscumm_mixer != NULL)
		g_syncscumm_mixer->tick();

	/* Dump-vs-present split: the inherited PPM dump (M1's boot-test
	 * frames) is game-surface-only and must fire strictly on real
	 * _screen changes (_dirty) -- a cursor-only move/show/sprite change
	 * must NOT produce a new dump frame, since dumpFrame() reads _screen
	 * (never _composed). The terminal *present*, on the other hand,
	 * needs to redraw whenever the game surface changed OR the cursor's
	 * on-screen appearance changed (position, visibility, or sprite),
	 * since the composited frame differs in either case. */
	bool screenDirty = _dirty;
	/* Gate sprite/position terms on visibility; _last* still tracks even
	 * when hidden so a later showMouse(true) presents at the right spot. */
	bool cursorChanged = (_cursorVisible != _lastCursorShown)
		|| (_cursorVisible && (_cursorSpriteDirty || _cursorX != _lastCursorX || _cursorY != _lastCursorY));
	/* Same idea for the overlay: a show/hide toggle or a content redraw
	 * (copyRectToOverlay/clearOverlay) must trigger a present even when
	 * the game surface hasn't changed -- see the _lastOverlayShown /
	 * _overlayContentDirty comment in video_term.h for why this is its
	 * own tracked pair rather than reusing _dirty. */
	bool overlayVisible = isOverlayVisible();
	bool overlayChanged = (overlayVisible != _lastOverlayShown)
		|| (overlayVisible && _overlayContentDirty);

	if (screenDirty)
		SyncscummDumpGraphicsManager::updateScreen();   /* clears _dirty, dumps PPM if enabled */

	if (!screenDirty && !cursorChanged && !overlayChanged)
		return;   /* nothing changed at all -- keep the no-change path fast */

	if (termgfx_termio_active()) {
		compose();
		if (overlayVisible) {
			/* Quantized palettes naturally differ frame to frame, so
			 * relying on termgfx_termio_present()'s own memcmp-based palette
			 * dirty-check (rather than forcing anything here) is
			 * correct: two frames that happen to quantize identically
			 * really do look identical to the client, so skipping the
			 * palette re-emit for them is the right call, not a bug to
			 * work around. */
			termgfx_termio_present(_quantIdx, _quantPal);
		} else if (_composed.getPixels()) {
			termgfx_termio_present((const byte *)_composed.getPixels(), _palette);
		}
	}

	_lastCursorX = _cursorX;
	_lastCursorY = _cursorY;
	_lastCursorShown = _cursorVisible;
	_cursorSpriteDirty = false;
	_lastOverlayShown = overlayVisible;
	_overlayContentDirty = false;
}

#endif
