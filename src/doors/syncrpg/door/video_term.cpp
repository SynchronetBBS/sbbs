/* SyncRPG -- terminal video present (Task 4 / M1). The engine draws each frame
 * into main_surface (32-bit, memory byte order R,G,B,X -- pinned in
 * ui_term.cpp's ctor); BaseUi_termgfx::UpdateDisplay() hands that surface here,
 * and this forwards its pixels to termgfx_termio_present_rgbx(), termgfx's
 * truecolor path (sixel-quantized or JXL-encoded, tier chosen at run time).
 * GPLv3+, like the EasyRPG tree.
 */
#include "video_term.h"

#include <cstdint>
#include <cstring>
#include <vector>

#include "bitmap.h"

extern "C" {
#include "termgfx_termio.h"
}

void video_term_present(const Bitmap& surf)
{
	const int w = surf.width();
	const int h = surf.height();
	if (w <= 0 || h <= 0)
		return;

	const uint8_t *pixels = static_cast<const uint8_t *>(surf.pixels());
	if (pixels == NULL)
		return;

	const int pitch     = surf.pitch();
	const int row_bytes = w * 4;   /* present_rgbx contract: 32bpp, stride w*4 */

	if (pitch == row_bytes) {
		/* Zero-copy: the surface is already tightly packed R,G,B,X, exactly
		 * present_rgbx's stride -- hand its pixels straight through. This is
		 * the norm (a 320x240x4 pixman surface pitches to w*4). */
		termgfx_termio_present_rgbx(pixels, w, h);
		return;
	}

	/* Padded rows (pitch > w*4): repack into a contiguous w*h*4 buffer so the
	 * stride matches present_rgbx's fixed w*4. A safety net, not the norm;
	 * the scratch persists across calls so a steady-state stream re-uses it. */
	static std::vector<uint8_t> packed;
	packed.resize((size_t)row_bytes * h);
	for (int y = 0; y < h; y++)
		memcpy(&packed[(size_t)y * row_bytes],
		       pixels + (size_t)y * pitch, (size_t)row_bytes);
	termgfx_termio_present_rgbx(packed.data(), w, h);
}
