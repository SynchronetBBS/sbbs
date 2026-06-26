// jxl.c -- JPEG XL frame encoder (libjxl), carved out of syncdoom.c's emit_frame_jxl().
// Game-agnostic: RGB888 in -> JXL codestream out. Compiled only when WITH_JXL is defined
// (the door's CMake sets it + the JXL include dir when libjxl is found); otherwise the
// stub at the bottom keeps the symbol present so callers compile and just fall back.

#include "jxl.h"
#include <stdlib.h>

#ifdef WITH_JXL
#include <jxl/encode.h>

static void jxl_ensure(uint8_t **buf, size_t *cap, size_t need)
{
	if (need > *cap) {
		*buf = realloc(*buf, need);
		*cap = need;
	}
}

size_t termgfx_jxl_encode(uint8_t **buf, size_t *cap, const uint8_t *rgb,
                          int w, int h, float distance, int effort)
{
	JxlEncoder *             enc;
	JxlEncoderFrameSettings *fs;
	JxlBasicInfo             info;
	JxlColorEncoding         color;
	JxlPixelFormat           pf = { 3, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0 };
	JxlEncoderStatus         st;
	size_t                   written = 0;

	enc = JxlEncoderCreate(NULL);
	if (enc == NULL)
		return 0;
	fs = JxlEncoderFrameSettingsCreate(enc, NULL);
	JxlEncoderFrameSettingsSetOption(fs, JXL_ENC_FRAME_SETTING_EFFORT, effort);
	JxlEncoderSetFrameLossless(fs, JXL_FALSE);
	JxlEncoderSetFrameDistance(fs, distance);

	JxlEncoderInitBasicInfo(&info);
	info.xsize = w;  info.ysize = h;
	info.bits_per_sample = 8;
	info.num_color_channels = 3;
	info.alpha_bits = 0;
	info.uses_original_profile = JXL_FALSE;
	if (JxlEncoderSetBasicInfo(enc, &info) != JXL_ENC_SUCCESS) { JxlEncoderDestroy(enc); return 0; }

	JxlColorEncodingSetToSRGB(&color, JXL_FALSE);
	JxlEncoderSetColorEncoding(enc, &color);

	if (JxlEncoderAddImageFrame(fs, &pf, rgb, (size_t)w * h * 3) != JXL_ENC_SUCCESS) {
		JxlEncoderDestroy(enc); return 0;
	}
	JxlEncoderCloseInput(enc);

	jxl_ensure(buf, cap, *cap < 4096 ? 4096 : *cap);
	do {
		uint8_t *next  = *buf + written;
		size_t   avail = *cap - written;
		st = JxlEncoderProcessOutput(enc, &next, &avail);
		written = (size_t)(next - *buf);
		if (st == JXL_ENC_NEED_MORE_OUTPUT)
			jxl_ensure(buf, cap, *cap * 2);
	} while (st == JXL_ENC_NEED_MORE_OUTPUT);
	JxlEncoderDestroy(enc);
	if (st != JXL_ENC_SUCCESS)
		return 0;
	return written;
}

#else  // !WITH_JXL -- stub so callers link and fall back to another tier

size_t termgfx_jxl_encode(uint8_t **buf, size_t *cap, const uint8_t *rgb,
                          int w, int h, float distance, int effort)
{
	(void)buf; (void)cap; (void)rgb; (void)w; (void)h; (void)distance; (void)effort;
	return 0;
}

#endif // WITH_JXL
