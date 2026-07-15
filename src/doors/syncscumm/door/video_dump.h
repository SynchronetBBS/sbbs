/* SyncSCUMM M1: a graphics manager that KEEPS the pixels.
 * Maintains the 8-bit game surface + palette; when the SYNCSCUMM_DUMP
 * environment variable names a directory, each changed updateScreen()
 * writes frame%04d.ppm there (capped). M2 replaces the PPM writer with
 * the termgfx dirty-rect encoder -- the surface/palette bookkeeping here
 * is the part that carries forward.
 */
#ifndef SYNCSCUMM_VIDEO_DUMP_H_
#define SYNCSCUMM_VIDEO_DUMP_H_

#include "backends/graphics/null/null-graphics.h"
#include "graphics/surface.h"

class SyncscummDumpGraphicsManager : public NullGraphicsManager {
public:
	SyncscummDumpGraphicsManager();
	~SyncscummDumpGraphicsManager() override;

	void initSize(uint width, uint height, const Graphics::PixelFormat *format = NULL) override;
	int16 getHeight() const override { return _height; }
	int16 getWidth() const override { return _width; }
	void setPalette(const byte *colors, uint start, uint num) override;
	void grabPalette(byte *colors, uint start, uint num) const override;
	void copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) override;
	Graphics::Surface *lockScreen() override;
	void unlockScreen() override;
	void updateScreen() override;

protected:
	void dumpFrame();

	Graphics::Surface _screen;
	byte _palette[256 * 3];
	uint _width, _height;
	bool _dirty;
	int _frameNo;
	const char *_dumpDir;	// getenv("SYNCSCUMM_DUMP"); NULL = no dumps
};

#endif /* SYNCSCUMM_VIDEO_DUMP_H_ */
