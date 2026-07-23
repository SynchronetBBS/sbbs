#define FORBIDDEN_SYMBOL_EXCEPTION_FILE
#define FORBIDDEN_SYMBOL_EXCEPTION_getenv
#define FORBIDDEN_SYMBOL_EXCEPTION_fopen
#define FORBIDDEN_SYMBOL_EXCEPTION_fclose
#define FORBIDDEN_SYMBOL_EXCEPTION_fprintf
#define FORBIDDEN_SYMBOL_EXCEPTION_fwrite

#include "common/scummsys.h"

#if defined(USE_TERMGFX_DRIVER)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "video_dump.h"

#define SYNCSCUMM_DUMP_CAP 500

SyncscummDumpGraphicsManager::SyncscummDumpGraphicsManager()
	: _width(0), _height(0), _dirty(false), _frameNo(0) {
	memset(_palette, 0, sizeof(_palette));
	_dumpDir = getenv("SYNCSCUMM_DUMP");
}

SyncscummDumpGraphicsManager::~SyncscummDumpGraphicsManager() {
	_screen.free();
}

void SyncscummDumpGraphicsManager::initSize(uint width, uint height, const Graphics::PixelFormat *format) {
	// Also runs the base NullGraphicsManager::initSize(), which is what
	// getOverlayWidth()/getOverlayHeight() (not overridden here) read --
	// without this the overlay stays 0x0 and the GUI theme engine (which
	// sizes itself off the overlay) fails to init.
	NullGraphicsManager::initSize(width, height, format);
	_width = width;
	_height = height;
	_screen.create(width, height, Graphics::PixelFormat::createFormatCLUT8());
	_dirty = true;
}

void SyncscummDumpGraphicsManager::setPalette(const byte *colors, uint start, uint num) {
	memcpy(_palette + start * 3, colors, num * 3);
	_dirty = true;
}

void SyncscummDumpGraphicsManager::grabPalette(byte *colors, uint start, uint num) const {
	memcpy(colors, _palette + start * 3, num * 3);
}

void SyncscummDumpGraphicsManager::copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) {
	if (!_screen.getPixels())
		return;
	assert(x >= 0 && x < (int)_width);
	assert(y >= 0 && y < (int)_height);
	assert(h > 0 && y + h <= (int)_height);
	assert(w > 0 && x + w <= (int)_width);
	const byte *src = (const byte *)buf;
	for (int row = 0; row < h; row++)
		memcpy(_screen.getBasePtr(x, y + row), src + row * pitch, w);
	_dirty = true;
}

Graphics::Surface *SyncscummDumpGraphicsManager::lockScreen() {
	return &_screen;
}

void SyncscummDumpGraphicsManager::unlockScreen() {
	_dirty = true;
}

void SyncscummDumpGraphicsManager::updateScreen() {
	if (!_dirty)
		return;
	_dirty = false;
	dumpFrame();
}

void SyncscummDumpGraphicsManager::dumpFrame() {
	if (!_dumpDir || _frameNo >= SYNCSCUMM_DUMP_CAP || !_screen.getPixels())
		return;
	char path[1024];
	snprintf(path, sizeof(path), "%s/frame%04d.ppm", _dumpDir, _frameNo);
	FILE *fp = fopen(path, "wb");
	if (!fp)
		return;
	fprintf(fp, "P6\n%u %u\n255\n", _width, _height);
	const byte *px = (const byte *)_screen.getPixels();
	for (uint i = 0; i < _width * _height; i++)
		fwrite(_palette + px[i] * 3, 1, 3, fp);
	fclose(fp);
	_frameNo++;
}

#endif /* USE_TERMGFX_DRIVER */
