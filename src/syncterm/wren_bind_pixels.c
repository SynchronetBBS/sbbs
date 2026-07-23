/* Wren bindings for ciolib pixel buffers, masks, and integer blits. */

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <ciolib.h>
#include <vidmodes.h>

#include "pixel_image.h"
#include "wren_bind_fs.h"
#include "wren_bind_internal.h"
#include "wren_bind_pixels.h"

#define WREN_MAX_EXACT_INTEGER 9007199254740991.0

struct wren_pixel_buffer {
	enum syncterm_wren_foreign type;
	struct ciolib_pixels *pixels;
};

struct wren_pixel_mask {
	enum syncterm_wren_foreign type;
	struct ciolib_mask *mask;
};

struct wren_pixel_blit {
	enum syncterm_wren_foreign type;
	struct ciolib_blit blit;
};

static bool
slot_integer(WrenVM *vm, int slot, double minimum, double maximum,
    int64_t *result, const char *message)
{
	double value;

	if (wrenGetSlotType(vm, slot) != WREN_TYPE_NUM) {
		wren_throw(vm, message);
		return false;
	}
	value = wrenGetSlotDouble(vm, slot);
	if (!isfinite(value) || value < minimum || value > maximum
	    || value != trunc(value)) {
		wren_throw(vm, message);
		return false;
	}
	*result = (int64_t)value;
	return true;
}

static bool
slot_uint32(WrenVM *vm, int slot, uint32_t *result, const char *message)
{
	int64_t value;

	if (!slot_integer(vm, slot, 0, UINT32_MAX, &value, message))
		return false;
	*result = (uint32_t)value;
	return true;
}

static bool
slot_int32(WrenVM *vm, int slot, int32_t *result, const char *message)
{
	int64_t value;

	if (!slot_integer(vm, slot, INT32_MIN, INT32_MAX, &value, message))
		return false;
	*result = (int32_t)value;
	return true;
}

static bool
dimensions(WrenVM *vm, int width_slot, int height_slot, uint32_t *width,
    uint32_t *height, size_t *count, const char *message)
{
	if (!slot_uint32(vm, width_slot, width, message)
	    || !slot_uint32(vm, height_slot, height, message))
		return false;
	*count = (size_t)*width * *height;
	if (*width != 0 && *count / *width != *height) {
		wren_throw(vm, message);
		return false;
	}
	return true;
}

static bool
pixel_dimensions(WrenVM *vm, int width_slot, int height_slot,
    uint32_t *width, uint32_t *height, size_t *count, const char *message)
{
	if (!dimensions(vm, width_slot, height_slot, width, height, count,
	    message))
		return false;
	if (*count > SIZE_MAX / sizeof(uint32_t)) {
		wren_throw(vm, message);
		return false;
	}
	return true;
}

static bool
slot_colour(WrenVM *vm, int slot, uint32_t *colour)
{
	uint32_t value;

	if (!slot_uint32(vm, slot, &value,
	    "pixel colour must be a native PixelColor value"))
		return false;
	if ((value & CIOLIB_COLOR_RGB) != 0) {
		if ((value & 0x7F000000u) != 0) {
			wren_throw(vm, "RGB pixel colour contains unsupported flag bits");
			return false;
		}
	}
	else if (value > UINT16_MAX) {
		wren_throw(vm, "palette pixel colour must be in 0..65535");
		return false;
	}
	*colour = value;
	return true;
}

static bool
colour_to_rgb(uint32_t colour, uint32_t *rgb)
{
	uint8_t red;
	uint8_t green;
	uint8_t blue;

	if ((colour & CIOLIB_COLOR_RGB) != 0) {
		*rgb = colour & 0x00FFFFFFu;
		return true;
	}
	if (ciolib_getpalette(colour, &red, &green, &blue) != 1)
		return false;
	*rgb = ((uint32_t)red << 16) | ((uint32_t)green << 8) | blue;
	return true;
}

static void
fn_PixelColor_rgb(WrenVM *vm)
{
	uint32_t rgb;

	if (!slot_uint32(vm, 1, &rgb, "PixelColor.rgb: expected 0xRRGGBB"))
		return;
	if (rgb > 0x00FFFFFFu) {
		wren_throw(vm, "PixelColor.rgb: value must be in 0..0xFFFFFF");
		return;
	}
	wrenSetSlotDouble(vm, 0, (double)(rgb | CIOLIB_COLOR_RGB));
}

static void
fn_PixelColor_palette(WrenVM *vm)
{
	int64_t index;

	if (!slot_integer(vm, 1, 0, UINT16_MAX, &index,
	    "PixelColor.palette: index must be in 0..65535"))
		return;
	wrenSetSlotDouble(vm, 0, (double)index);
}

static void
fn_PixelColor_isRgb(WrenVM *vm)
{
	uint32_t colour;

	if (!slot_colour(vm, 1, &colour))
		return;
	wrenSetSlotBool(vm, 0, (colour & CIOLIB_COLOR_RGB) != 0);
}

static void
fn_PixelColor_rgbValue(WrenVM *vm)
{
	uint32_t colour;

	if (!slot_colour(vm, 1, &colour))
		return;
	if ((colour & CIOLIB_COLOR_RGB) == 0) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotDouble(vm, 0, (double)(colour & 0x00FFFFFFu));
}

static void
fn_PixelColor_paletteIndex(WrenVM *vm)
{
	uint32_t colour;

	if (!slot_colour(vm, 1, &colour))
		return;
	if ((colour & CIOLIB_COLOR_RGB) != 0) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotDouble(vm, 0, (double)colour);
}

static void
fn_PixelColor_toRgb(WrenVM *vm)
{
	uint32_t colour;
	uint32_t rgb;

	if (!slot_colour(vm, 1, &colour))
		return;
	if (!colour_to_rgb(colour, &rgb)) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotDouble(vm, 0, (double)rgb);
}

static void
pixel_buffer_allocate(WrenVM *vm)
{
	struct wren_pixel_buffer *buffer =
	    wrenSetSlotNewForeign(vm, 0, 0, sizeof(*buffer));
	uint32_t width;
	uint32_t height;
	uint32_t fill = 0;
	size_t count;

	buffer->type = SWF_PIXEL_BUFFER;
	buffer->pixels = NULL;
	if (!pixel_dimensions(vm, 1, 2, &width, &height, &count,
	    "PixelBuffer.new: invalid dimensions"))
		return;
	if (wrenGetSlotCount(vm) > 3 && !slot_colour(vm, 3, &fill))
		return;
	buffer->pixels = pixel_image_alloc_pixels(width, height);
	if (buffer->pixels == NULL) {
		wren_throw(vm, "PixelBuffer.new: out of memory");
		return;
	}
	for (size_t i = 0; i < count; i++)
		buffer->pixels->pixels[i] = fill;
}

static void
pixel_buffer_finalize(void *data)
{
	struct wren_pixel_buffer *buffer = data;

	freepixels(buffer->pixels);
	buffer->pixels = NULL;
}

static struct wren_pixel_buffer *
pixel_buffer(WrenVM *vm)
{
	return wrenGetSlotForeign(vm, 0);
}

static size_t
buffer_count(const struct ciolib_pixels *pixels)
{
	return (size_t)pixels->width * pixels->height;
}

static bool
buffer_index(WrenVM *vm, struct ciolib_pixels *pixels, int slot,
    size_t *index)
{
	int64_t value;
	size_t count = buffer_count(pixels);

	if (!slot_integer(vm, slot, 0, WREN_MAX_EXACT_INTEGER, &value,
	    "PixelBuffer: index must be a non-negative integer"))
		return false;
	if ((uint64_t)value >= count) {
		wren_throw(vm, "PixelBuffer: index out of bounds");
		return false;
	}
	*index = (size_t)value;
	return true;
}

static bool
buffer_position(WrenVM *vm, struct ciolib_pixels *pixels, int xslot,
    int yslot, size_t *index)
{
	uint32_t x;
	uint32_t y;

	if (!slot_uint32(vm, xslot, &x,
	    "PixelBuffer: x must be a non-negative integer")
	    || !slot_uint32(vm, yslot, &y,
	    "PixelBuffer: y must be a non-negative integer"))
		return false;
	if (x >= pixels->width || y >= pixels->height) {
		wren_throw(vm, "PixelBuffer: coordinates out of bounds");
		return false;
	}
	*index = (size_t)y * pixels->width + x;
	return true;
}

static void
fn_PixelBuffer_count(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)buffer_count(pixel_buffer(vm)->pixels));
}

static void
fn_PixelBuffer_width(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)pixel_buffer(vm)->pixels->width);
}

static void
fn_PixelBuffer_height(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)pixel_buffer(vm)->pixels->height);
}

static void
fn_PixelBuffer_subscript(WrenVM *vm)
{
	struct ciolib_pixels *pixels = pixel_buffer(vm)->pixels;
	size_t index;

	if (!buffer_index(vm, pixels, 1, &index))
		return;
	wrenSetSlotDouble(vm, 0, (double)pixels->pixels[index]);
}

static void
fn_PixelBuffer_subscript_set(WrenVM *vm)
{
	struct ciolib_pixels *pixels = pixel_buffer(vm)->pixels;
	uint32_t colour;
	size_t index;

	if (!buffer_index(vm, pixels, 1, &index)
	    || !slot_colour(vm, 2, &colour))
		return;
	pixels->pixels[index] = colour;
}

static void
fn_PixelBuffer_pixelAt(WrenVM *vm)
{
	struct ciolib_pixels *pixels = pixel_buffer(vm)->pixels;
	size_t index;

	if (!buffer_position(vm, pixels, 1, 2, &index))
		return;
	wrenSetSlotDouble(vm, 0, (double)pixels->pixels[index]);
}

static void
fn_PixelBuffer_setPixel(WrenVM *vm)
{
	struct ciolib_pixels *pixels = pixel_buffer(vm)->pixels;
	uint32_t colour;
	size_t index;

	if (!buffer_position(vm, pixels, 1, 2, &index)
	    || !slot_colour(vm, 3, &colour))
		return;
	pixels->pixels[index] = colour;
}

static void
fn_PixelBuffer_fill(WrenVM *vm)
{
	struct ciolib_pixels *pixels = pixel_buffer(vm)->pixels;
	uint32_t colour;
	size_t count = buffer_count(pixels);

	if (!slot_colour(vm, 1, &colour))
		return;
	for (size_t i = 0; i < count; i++)
		pixels->pixels[i] = colour;
}

static void
fn_PixelBuffer_iterate(WrenVM *vm)
{
	size_t count = buffer_count(pixel_buffer(vm)->pixels);

	if (count == 0) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	if (wrenGetSlotType(vm, 1) == WREN_TYPE_NULL) {
		wrenSetSlotDouble(vm, 0, 0);
		return;
	}
	int64_t current;
	if (!slot_integer(vm, 1, 0, WREN_MAX_EXACT_INTEGER, &current,
	    "PixelBuffer.iterate: invalid iterator"))
		return;
	if ((uint64_t)current + 1 >= count)
		wrenSetSlotBool(vm, 0, false);
	else
		wrenSetSlotDouble(vm, 0, (double)(current + 1));
}

static void
fn_PixelBuffer_iteratorValue(WrenVM *vm)
{
	fn_PixelBuffer_subscript(vm);
}

static struct wren_pixel_buffer *
return_pixel_buffer(WrenVM *vm, struct ciolib_pixels *pixels)
{
	struct wren_pixel_buffer *buffer;

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "syncterm", "PixelBuffer", 1);
	buffer = wrenSetSlotNewForeign(vm, 0, 1, sizeof(*buffer));
	buffer->type = SWF_PIXEL_BUFFER;
	buffer->pixels = pixels;
	return buffer;
}

static bool
ensure_alternate(struct ciolib_pixels *pixels)
{
	size_t count;

	if (pixels->pixelsb != NULL)
		return true;
	count = buffer_count(pixels);
	if (count == 0)
		return true;
	pixels->pixelsb = malloc(count * sizeof(*pixels->pixelsb));
	if (pixels->pixelsb == NULL)
		return false;
	memcpy(pixels->pixelsb, pixels->pixels,
	    count * sizeof(*pixels->pixelsb));
	return true;
}

static void
fn_PixelBuffer_clone(WrenVM *vm)
{
	struct ciolib_pixels *copy = ciolib_duppixels(pixel_buffer(vm)->pixels);

	if (copy == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	return_pixel_buffer(vm, copy);
}

static void
fn_PixelBuffer_hasAlternate(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, pixel_buffer(vm)->pixels->pixelsb != NULL);
}

static void
fn_PixelBuffer_alternateAt(WrenVM *vm)
{
	struct ciolib_pixels *pixels = pixel_buffer(vm)->pixels;
	size_t index;

	if (!buffer_index(vm, pixels, 1, &index))
		return;
	if (pixels->pixelsb == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotDouble(vm, 0, (double)pixels->pixelsb[index]);
}

static void
fn_PixelBuffer_setAlternate(WrenVM *vm)
{
	struct ciolib_pixels *pixels = pixel_buffer(vm)->pixels;
	uint32_t colour;
	size_t index;

	if (!buffer_position(vm, pixels, 1, 2, &index)
	    || !slot_colour(vm, 3, &colour))
		return;
	if (!ensure_alternate(pixels)) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	if (pixels->pixelsb != NULL)
		pixels->pixelsb[index] = colour;
}

static void
fn_PixelBuffer_fillAlternate(WrenVM *vm)
{
	struct ciolib_pixels *pixels = pixel_buffer(vm)->pixels;
	uint32_t colour;
	size_t count = buffer_count(pixels);

	if (!slot_colour(vm, 1, &colour))
		return;
	if (!ensure_alternate(pixels)) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	for (size_t i = 0; i < count; i++)
		pixels->pixelsb[i] = colour;
}

static void
fn_PixelBuffer_clearAlternate(WrenVM *vm)
{
	struct ciolib_pixels *pixels = pixel_buffer(vm)->pixels;

	free(pixels->pixelsb);
	pixels->pixelsb = NULL;
}

static bool
import_rgb24(WrenVM *vm, int slot, uint32_t *destination, size_t count,
    const char *message)
{
	int length;
	const char *bytes;

	if (wrenGetSlotType(vm, slot) != WREN_TYPE_STRING) {
		wren_throw(vm, message);
		return false;
	}
	bytes = wrenGetSlotBytes(vm, slot, &length);
	if (count > INT_MAX / 3 || length != (int)(count * 3)) {
		wren_throw(vm, message);
		return false;
	}
	for (size_t i = 0; i < count; i++) {
		const uint8_t *pixel = (const uint8_t *)bytes + i * 3;
		destination[i] = CIOLIB_COLOR_RGB | ((uint32_t)pixel[0] << 16)
		    | ((uint32_t)pixel[1] << 8) | pixel[2];
	}
	return true;
}

static bool
export_rgb24(WrenVM *vm, const uint32_t *source, size_t count)
{
	uint8_t *bytes;

	if (count > SIZE_MAX / 3) {
		wrenSetSlotNull(vm, 0);
		return false;
	}
	if (count == 0) {
		wrenSetSlotBytes(vm, 0, "", 0);
		return true;
	}
	bytes = malloc(count * 3);
	if (bytes == NULL) {
		wrenSetSlotNull(vm, 0);
		return false;
	}
	for (size_t i = 0; i < count; i++) {
		uint32_t rgb;

		if (!colour_to_rgb(source[i], &rgb)) {
			free(bytes);
			wrenSetSlotNull(vm, 0);
			return false;
		}
		bytes[i * 3] = (uint8_t)(rgb >> 16);
		bytes[i * 3 + 1] = (uint8_t)(rgb >> 8);
		bytes[i * 3 + 2] = (uint8_t)rgb;
	}
	wrenSetSlotBytes(vm, 0, (const char *)bytes, count * 3);
	free(bytes);
	return true;
}

static void
fn_PixelBuffer_fromRgb24(WrenVM *vm)
{
	uint32_t width;
	uint32_t height;
	size_t count;
	struct ciolib_pixels *pixels;

	if (!pixel_dimensions(vm, 1, 2, &width, &height, &count,
	    "PixelBuffer.fromRgb24: invalid dimensions"))
		return;
	pixels = pixel_image_alloc_pixels(width, height);
	if (pixels == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	if (!import_rgb24(vm, 3, pixels->pixels, count,
	    "PixelBuffer.fromRgb24: byte length must be width * height * 3")) {
		freepixels(pixels);
		return;
	}
	if (wrenGetSlotCount(vm) > 4) {
		if (!ensure_alternate(pixels)
		    || !import_rgb24(vm, 4, pixels->pixelsb, count,
		    "PixelBuffer.fromRgb24: alternate byte length must be width * height * 3")) {
			freepixels(pixels);
			if (wrenGetSlotType(vm, 0) != WREN_TYPE_STRING)
				wrenSetSlotNull(vm, 0);
			return;
		}
	}
	return_pixel_buffer(vm, pixels);
}

static void
fn_PixelBuffer_rgb24(WrenVM *vm)
{
	struct ciolib_pixels *pixels = pixel_buffer(vm)->pixels;

	export_rgb24(vm, pixels->pixels, buffer_count(pixels));
}

static void
fn_PixelBuffer_alternateRgb24(WrenVM *vm)
{
	struct ciolib_pixels *pixels = pixel_buffer(vm)->pixels;

	if (pixels->pixelsb == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	export_rgb24(vm, pixels->pixelsb, buffer_count(pixels));
}

static void
fn_PixelBuffer_setAlternateRgb24(WrenVM *vm)
{
	struct ciolib_pixels *pixels = pixel_buffer(vm)->pixels;
	size_t count = buffer_count(pixels);
	int length;

	if (wrenGetSlotType(vm, 1) != WREN_TYPE_STRING) {
		wren_throw(vm,
		    "setAlternateRgb24: byte length must be width * height * 3");
		return;
	}
	(void)wrenGetSlotBytes(vm, 1, &length);
	if (count > INT_MAX / 3 || length != (int)(count * 3)) {
		wren_throw(vm,
		    "setAlternateRgb24: byte length must be width * height * 3");
		return;
	}
	if (!ensure_alternate(pixels)) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	if (!import_rgb24(vm, 1, pixels->pixelsb, count,
	    "setAlternateRgb24: byte length must be width * height * 3"))
		return;
}

static struct ciolib_pixels *
decode_pixel_source(WrenVM *vm, int slot, bool jxl)
{
	if (wrenGetSlotType(vm, slot) == WREN_TYPE_STRING) {
		int length;
		const char *bytes = wrenGetSlotBytes(vm, slot, &length);
		return jxl ? pixel_image_decode_jxl(bytes, (size_t)length)
		    : pixel_image_decode_ppm(bytes, (size_t)length);
	}
	if (slot_foreign_type(vm, slot) == SWF_FILE) {
		const char *path = wren_file_read_path(vm, slot);

		if (path == NULL) {
			wren_throw(vm, "image source File is not readable");
			return NULL;
		}
		return jxl ? pixel_image_decode_jxl_file(path)
		    : pixel_image_decode_ppm_file(path);
	}
	wren_throw(vm, "image source must be binary String data or a readable File");
	return NULL;
}

static void
fn_PixelBuffer_fromPpm(WrenVM *vm)
{
	struct ciolib_pixels *pixels = decode_pixel_source(vm, 1, false);

	if (pixels == NULL) {
		if (wrenGetSlotType(vm, 0) != WREN_TYPE_STRING)
			wrenSetSlotNull(vm, 0);
		return;
	}
	return_pixel_buffer(vm, pixels);
}

static void
fn_PixelBuffer_fromJxl(WrenVM *vm)
{
	struct ciolib_pixels *pixels;

	if (!pixel_image_jxl_supported()) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	pixels = decode_pixel_source(vm, 1, true);
	if (pixels == NULL) {
		if (wrenGetSlotType(vm, 0) != WREN_TYPE_STRING)
			wrenSetSlotNull(vm, 0);
		return;
	}
	return_pixel_buffer(vm, pixels);
}

static void
fn_PixelBuffer_jxlSupported(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, pixel_image_jxl_supported());
}

static void
pixel_mask_allocate(WrenVM *vm)
{
	struct wren_pixel_mask *wrapper =
	    wrenSetSlotNewForeign(vm, 0, 0, sizeof(*wrapper));
	uint32_t width;
	uint32_t height;
	size_t count;
	bool fill = false;

	wrapper->type = SWF_PIXEL_MASK;
	wrapper->mask = NULL;
	if (!dimensions(vm, 1, 2, &width, &height, &count,
	    "PixelMask.new: invalid dimensions"))
		return;
	if (wrenGetSlotCount(vm) > 3) {
		if (wrenGetSlotType(vm, 3) != WREN_TYPE_BOOL) {
			wren_throw(vm, "PixelMask.new: fill must be Bool");
			return;
		}
		fill = wrenGetSlotBool(vm, 3);
	}
	wrapper->mask = pixel_image_alloc_mask(width, height);
	if (wrapper->mask == NULL) {
		wren_throw(vm, "PixelMask.new: out of memory");
		return;
	}
	if (fill && count != 0) {
		memset(wrapper->mask->bits, 0xFF, (count + 7) / 8);
		if ((count % 8) != 0)
			wrapper->mask->bits[count / 8] &= 0xFF << (8 - count % 8);
	}
}

static void
pixel_mask_finalize(void *data)
{
	struct wren_pixel_mask *wrapper = data;

	freemask(wrapper->mask);
	wrapper->mask = NULL;
}

static struct ciolib_mask *
pixel_mask(WrenVM *vm)
{
	struct wren_pixel_mask *wrapper = wrenGetSlotForeign(vm, 0);
	return wrapper->mask;
}

static size_t
mask_count(const struct ciolib_mask *mask)
{
	return (size_t)mask->width * mask->height;
}

static bool
mask_index(WrenVM *vm, struct ciolib_mask *mask, int slot, size_t *index)
{
	int64_t value;
	size_t count = mask_count(mask);

	if (!slot_integer(vm, slot, 0, WREN_MAX_EXACT_INTEGER, &value,
	    "PixelMask: index must be a non-negative integer"))
		return false;
	if ((uint64_t)value >= count) {
		wren_throw(vm, "PixelMask: index out of bounds");
		return false;
	}
	*index = (size_t)value;
	return true;
}

static bool
mask_position(WrenVM *vm, struct ciolib_mask *mask, int xslot, int yslot,
    size_t *index)
{
	uint32_t x;
	uint32_t y;

	if (!slot_uint32(vm, xslot, &x,
	    "PixelMask: x must be a non-negative integer")
	    || !slot_uint32(vm, yslot, &y,
	    "PixelMask: y must be a non-negative integer"))
		return false;
	if (x >= mask->width || y >= mask->height) {
		wren_throw(vm, "PixelMask: coordinates out of bounds");
		return false;
	}
	*index = (size_t)y * mask->width + x;
	return true;
}

static bool
slot_bool(WrenVM *vm, int slot, bool *value, const char *message)
{
	if (wrenGetSlotType(vm, slot) != WREN_TYPE_BOOL) {
		wren_throw(vm, message);
		return false;
	}
	*value = wrenGetSlotBool(vm, slot);
	return true;
}

static bool
mask_bit(const struct ciolib_mask *mask, size_t index)
{
	return (mask->bits[index / 8] & (0x80 >> (index % 8))) != 0;
}

static void
set_mask_bit(struct ciolib_mask *mask, size_t index, bool value)
{
	uint8_t bit = 0x80 >> (index % 8);

	if (value)
		mask->bits[index / 8] |= bit;
	else
		mask->bits[index / 8] &= ~bit;
}

static void
fn_PixelMask_count(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)mask_count(pixel_mask(vm)));
}

static void
fn_PixelMask_width(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)pixel_mask(vm)->width);
}

static void
fn_PixelMask_height(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)pixel_mask(vm)->height);
}

static void
fn_PixelMask_subscript(WrenVM *vm)
{
	struct ciolib_mask *mask = pixel_mask(vm);
	size_t index;

	if (!mask_index(vm, mask, 1, &index))
		return;
	wrenSetSlotBool(vm, 0, mask_bit(mask, index));
}

static void
fn_PixelMask_subscript_set(WrenVM *vm)
{
	struct ciolib_mask *mask = pixel_mask(vm);
	bool value;
	size_t index;

	if (!mask_index(vm, mask, 1, &index)
	    || !slot_bool(vm, 2, &value, "PixelMask value must be Bool"))
		return;
	set_mask_bit(mask, index, value);
}

static void
fn_PixelMask_bitAt(WrenVM *vm)
{
	struct ciolib_mask *mask = pixel_mask(vm);
	size_t index;

	if (!mask_position(vm, mask, 1, 2, &index))
		return;
	wrenSetSlotBool(vm, 0, mask_bit(mask, index));
}

static void
fn_PixelMask_setBit(WrenVM *vm)
{
	struct ciolib_mask *mask = pixel_mask(vm);
	bool value;
	size_t index;

	if (!mask_position(vm, mask, 1, 2, &index)
	    || !slot_bool(vm, 3, &value, "PixelMask value must be Bool"))
		return;
	set_mask_bit(mask, index, value);
}

static void
fn_PixelMask_fill(WrenVM *vm)
{
	struct ciolib_mask *mask = pixel_mask(vm);
	bool value;
	size_t count = mask_count(mask);
	size_t bytes = (count + 7) / 8;

	if (!slot_bool(vm, 1, &value, "PixelMask.fill: value must be Bool"))
		return;
	if (bytes != 0)
		memset(mask->bits, value ? 0xFF : 0, bytes);
	if (value && (count % 8) != 0)
		mask->bits[count / 8] &= 0xFF << (8 - count % 8);
}

static void
fn_PixelMask_iterate(WrenVM *vm)
{
	size_t count = mask_count(pixel_mask(vm));

	if (count == 0) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	if (wrenGetSlotType(vm, 1) == WREN_TYPE_NULL) {
		wrenSetSlotDouble(vm, 0, 0);
		return;
	}
	int64_t current;
	if (!slot_integer(vm, 1, 0, WREN_MAX_EXACT_INTEGER, &current,
	    "PixelMask.iterate: invalid iterator"))
		return;
	if ((uint64_t)current + 1 >= count)
		wrenSetSlotBool(vm, 0, false);
	else
		wrenSetSlotDouble(vm, 0, (double)(current + 1));
}

static void
fn_PixelMask_iteratorValue(WrenVM *vm)
{
	fn_PixelMask_subscript(vm);
}

static struct wren_pixel_mask *
return_pixel_mask(WrenVM *vm, struct ciolib_mask *mask)
{
	struct wren_pixel_mask *wrapper;

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "syncterm", "PixelMask", 1);
	wrapper = wrenSetSlotNewForeign(vm, 0, 1, sizeof(*wrapper));
	wrapper->type = SWF_PIXEL_MASK;
	wrapper->mask = mask;
	return wrapper;
}

static void
fn_PixelMask_fromBits(WrenVM *vm)
{
	uint32_t width;
	uint32_t height;
	size_t count;
	size_t byte_count;
	int length;
	const char *bits;
	struct ciolib_mask *mask;

	if (!dimensions(vm, 1, 2, &width, &height, &count,
	    "PixelMask.fromBits: invalid dimensions"))
		return;
	if (wrenGetSlotType(vm, 3) != WREN_TYPE_STRING) {
		wren_throw(vm, "PixelMask.fromBits: bits must be a String");
		return;
	}
	bits = wrenGetSlotBytes(vm, 3, &length);
	byte_count = (count + 7) / 8;
	if (byte_count > INT_MAX || length != (int)byte_count) {
		wren_throw(vm,
		    "PixelMask.fromBits: byte length must be ceil(width * height / 8)");
		return;
	}
	mask = pixel_image_alloc_mask(width, height);
	if (mask == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	if (byte_count != 0)
		memcpy(mask->bits, bits, byte_count);
	if ((count % 8) != 0)
		mask->bits[count / 8] &= 0xFF << (8 - count % 8);
	return_pixel_mask(vm, mask);
}

static void
fn_PixelMask_bits(WrenVM *vm)
{
	struct ciolib_mask *mask = pixel_mask(vm);
	size_t bytes = (mask_count(mask) + 7) / 8;

	wrenSetSlotBytes(vm, 0,
	    bytes != 0 ? (const char *)mask->bits : "", bytes);
}

static void
fn_PixelMask_fromPbm(WrenVM *vm)
{
	struct ciolib_mask *mask = NULL;

	if (wrenGetSlotType(vm, 1) == WREN_TYPE_STRING) {
		int length;
		const char *bytes = wrenGetSlotBytes(vm, 1, &length);
		mask = pixel_image_decode_pbm(bytes, (size_t)length);
	}
	else if (slot_foreign_type(vm, 1) == SWF_FILE) {
		const char *path = wren_file_read_path(vm, 1);

		if (path == NULL) {
			wren_throw(vm, "PBM source File is not readable");
			return;
		}
		mask = pixel_image_decode_pbm_file(path);
	}
	else {
		wren_throw(vm,
		    "PBM source must be binary String data or a readable File");
		return;
	}
	if (mask == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	return_pixel_mask(vm, mask);
}

static void
pixel_blit_allocate(WrenVM *vm)
{
	struct wren_pixel_blit *descriptor =
	    wrenSetSlotNewForeign(vm, 0, 0, sizeof(*descriptor));

	descriptor->type = SWF_PIXEL_BLIT;
	memset(&descriptor->blit, 0, sizeof(descriptor->blit));
	descriptor->blit.scale_x = 1;
	descriptor->blit.scale_y = 1;
}

static void
pixel_blit_finalize(void *data)
{
	(void)data;
}

static struct ciolib_blit *
pixel_blit(WrenVM *vm)
{
	struct wren_pixel_blit *descriptor = wrenGetSlotForeign(vm, 0);
	return &descriptor->blit;
}

#define BLIT_UINT_FIELD(name, field, allow_zero)                              \
	static void fn_PixelBlit_##name(WrenVM *vm)                            \
	{                                                                       \
		wrenSetSlotDouble(vm, 0, (double)pixel_blit(vm)->field);          \
	}                                                                       \
	static void fn_PixelBlit_##name##_set(WrenVM *vm)                      \
	{                                                                       \
		uint32_t value;                                                   \
		if (!slot_uint32(vm, 1, &value,                                \
		    "PixelBlit." #name ": expected a non-negative integer"))    \
			return;                                                     \
		if (!(allow_zero) && value == 0) {                              \
			wren_throw(vm, "PixelBlit." #name ": value must be positive"); \
			return;                                                     \
		}                                                               \
		pixel_blit(vm)->field = value;                                  \
	}

#define BLIT_INT_FIELD(name, field)                                         \
	static void fn_PixelBlit_##name(WrenVM *vm)                            \
	{                                                                       \
		wrenSetSlotDouble(vm, 0, (double)pixel_blit(vm)->field);          \
	}                                                                       \
	static void fn_PixelBlit_##name##_set(WrenVM *vm)                      \
	{                                                                       \
		int32_t value;                                                    \
		if (!slot_int32(vm, 1, &value,                                \
		    "PixelBlit." #name ": expected a signed 32-bit integer"))    \
			return;                                                     \
		pixel_blit(vm)->field = value;                                  \
	}

BLIT_UINT_FIELD(sourceX, sx, true)
BLIT_UINT_FIELD(sourceY, sy, true)
BLIT_UINT_FIELD(sourceWidth, sw, true)
BLIT_UINT_FIELD(sourceHeight, sh, true)
BLIT_INT_FIELD(destinationX, dx)
BLIT_INT_FIELD(destinationY, dy)
BLIT_UINT_FIELD(scaleX, scale_x, false)
BLIT_UINT_FIELD(scaleY, scale_y, false)
BLIT_UINT_FIELD(maskX, mx, true)
BLIT_UINT_FIELD(maskY, my, true)

#undef BLIT_UINT_FIELD
#undef BLIT_INT_FIELD

static void
fn_PixelBlit_flipX(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0,
	    (pixel_blit(vm)->flags & CIOLIB_BLIT_FLIP_X) != 0);
}

static void
fn_PixelBlit_flipX_set(WrenVM *vm)
{
	bool value;

	if (!slot_bool(vm, 1, &value, "PixelBlit.flipX: expected Bool"))
		return;
	if (value)
		pixel_blit(vm)->flags |= CIOLIB_BLIT_FLIP_X;
	else
		pixel_blit(vm)->flags &= ~CIOLIB_BLIT_FLIP_X;
}

static void
fn_PixelBlit_flipY(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0,
	    (pixel_blit(vm)->flags & CIOLIB_BLIT_FLIP_Y) != 0);
}

static void
fn_PixelBlit_flipY_set(WrenVM *vm)
{
	bool value;

	if (!slot_bool(vm, 1, &value, "PixelBlit.flipY: expected Bool"))
		return;
	if (value)
		pixel_blit(vm)->flags |= CIOLIB_BLIT_FLIP_Y;
	else
		pixel_blit(vm)->flags &= ~CIOLIB_BLIT_FLIP_Y;
}

static bool
screen_pixel_geometry(uint32_t *width, uint32_t *height, uint32_t *cell_width,
    uint32_t *cell_height)
{
	struct text_info info;
	int mode;

	if ((cio_api.options & CONIO_OPT_SET_PIXEL) == 0)
		return false;
	gettextinfo(&info);
	mode = find_vmode(info.currmode);
	if (mode < 0 || vparams[mode].xres <= 0 || vparams[mode].yres <= 0
	    || vparams[mode].charwidth <= 0 || vparams[mode].charheight <= 0)
		return false;
	if (width != NULL)
		*width = (uint32_t)vparams[mode].xres;
	if (height != NULL)
		*height = (uint32_t)vparams[mode].yres;
	if (cell_width != NULL)
		*cell_width = (uint32_t)vparams[mode].charwidth;
	if (cell_height != NULL)
		*cell_height = (uint32_t)vparams[mode].charheight;
	return true;
}

static void
set_size_result(WrenVM *vm, uint32_t width, uint32_t height)
{
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
	wrenSetSlotDouble(vm, 1, (double)width);
	wrenInsertInList(vm, 0, -1, 1);
	wrenSetSlotDouble(vm, 1, (double)height);
	wrenInsertInList(vm, 0, -1, 1);
}

static void
fn_Screen_pixelSize(WrenVM *vm)
{
	uint32_t width;
	uint32_t height;

	if (!screen_pixel_geometry(&width, &height, NULL, NULL)) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	set_size_result(vm, width, height);
}

static void
fn_Screen_cellPixelSize(WrenVM *vm)
{
	uint32_t width;
	uint32_t height;

	if (!screen_pixel_geometry(NULL, NULL, &width, &height)) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	set_size_result(vm, width, height);
}

static void
fn_Screen_readPixels(WrenVM *vm)
{
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
	size_t count;
	uint64_t end_x;
	uint64_t end_y;
	bool force = false;
	struct ciolib_pixels *pixels;

	if (!slot_uint32(vm, 1, &x,
	    "Screen.readPixels: x must be a non-negative integer")
	    || !slot_uint32(vm, 2, &y,
	    "Screen.readPixels: y must be a non-negative integer")
	    || !dimensions(vm, 3, 4, &width, &height, &count,
	    "Screen.readPixels: invalid dimensions"))
		return;
	(void)count;
	if (width == 0 || height == 0) {
		wren_throw(vm, "Screen.readPixels: width and height must be positive");
		return;
	}
	if (wrenGetSlotCount(vm) > 5
	    && !slot_bool(vm, 5, &force,
	    "Screen.readPixels: force must be Bool"))
		return;
	end_x = (uint64_t)x + width - 1;
	end_y = (uint64_t)y + height - 1;
	if (end_x > UINT32_MAX || end_y > UINT32_MAX) {
		wren_throw(vm, "Screen.readPixels: rectangle exceeds coordinate range");
		return;
	}
	pixels = getpixels(x, y, (uint32_t)end_x, (uint32_t)end_y, force);
	if (pixels == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	return_pixel_buffer(vm, pixels);
}

static void
fn_Screen_setPixel(WrenVM *vm)
{
	uint32_t x;
	uint32_t y;
	uint32_t colour;

	if (!slot_uint32(vm, 1, &x,
	    "Screen.setPixel: x must be a non-negative integer")
	    || !slot_uint32(vm, 2, &y,
	    "Screen.setPixel: y must be a non-negative integer")
	    || !slot_colour(vm, 3, &colour))
		return;
	wrenSetSlotBool(vm, 0, setpixel(x, y, colour) != 0);
}

static void
fn_Screen_blitPixels(WrenVM *vm)
{
	struct wren_pixel_buffer *buffer;
	struct wren_pixel_mask *mask = NULL;
	struct wren_pixel_blit *descriptor;
	struct ciolib_blit blit;
	int descriptor_slot;

	if (slot_foreign_type(vm, 1) != SWF_PIXEL_BUFFER) {
		wren_throw(vm, "Screen.blitPixels: source must be a PixelBuffer");
		return;
	}
	buffer = wrenGetSlotForeign(vm, 1);
	if (wrenGetSlotCount(vm) == 4) {
		if (slot_foreign_type(vm, 2) != SWF_PIXEL_MASK) {
			wren_throw(vm, "Screen.blitPixels: mask must be a PixelMask");
			return;
		}
		mask = wrenGetSlotForeign(vm, 2);
		descriptor_slot = 3;
	}
	else
		descriptor_slot = 2;
	if (slot_foreign_type(vm, descriptor_slot) != SWF_PIXEL_BLIT) {
		wren_throw(vm, "Screen.blitPixels: descriptor must be a PixelBlit");
		return;
	}
	descriptor = wrenGetSlotForeign(vm, descriptor_slot);
	blit = descriptor->blit;
	if (buffer->pixels == NULL || buffer->pixels->pixels == NULL
	    || blit.sx > buffer->pixels->width
	    || blit.sy > buffer->pixels->height) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	if (blit.sw == 0)
		blit.sw = buffer->pixels->width - blit.sx;
	if (blit.sh == 0)
		blit.sh = buffer->pixels->height - blit.sy;
	if (blit.sw == 0 || blit.sh == 0
	    || blit.sw > buffer->pixels->width - blit.sx
	    || blit.sh > buffer->pixels->height - blit.sy) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	if (mask != NULL && (mask->mask == NULL || mask->mask->bits == NULL
	    || blit.mx >= mask->mask->width || blit.my >= mask->mask->height
	    || blit.sw > mask->mask->width - blit.mx
	    || blit.sh > mask->mask->height - blit.my)) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	wrenSetSlotBool(vm, 0,
	    blitpixels(buffer->pixels, mask != NULL ? mask->mask : NULL,
	    &blit) != 0);
}

struct pixel_binding {
	const char *class_name;
	bool is_static;
	const char *signature;
	WrenForeignMethodFn function;
};

#define BIND(cls, statik, sig, fn) { cls, statik, sig, fn }

static const struct pixel_binding BINDINGS[] = {
	BIND("PixelColor", true, "rgb(_)", fn_PixelColor_rgb),
	BIND("PixelColor", true, "palette(_)", fn_PixelColor_palette),
	BIND("PixelColor", true, "isRgb(_)", fn_PixelColor_isRgb),
	BIND("PixelColor", true, "rgbValue(_)", fn_PixelColor_rgbValue),
	BIND("PixelColor", true, "paletteIndex(_)", fn_PixelColor_paletteIndex),
	BIND("PixelColor", true, "toRgb(_)", fn_PixelColor_toRgb),

	BIND("PixelBuffer", true, "fromRgb24(_,_,_)", fn_PixelBuffer_fromRgb24),
	BIND("PixelBuffer", true, "fromRgb24(_,_,_,_)", fn_PixelBuffer_fromRgb24),
	BIND("PixelBuffer", true, "fromPpm(_)", fn_PixelBuffer_fromPpm),
	BIND("PixelBuffer", true, "fromJxl(_)", fn_PixelBuffer_fromJxl),
	BIND("PixelBuffer", true, "jxlSupported", fn_PixelBuffer_jxlSupported),
	BIND("PixelBuffer", false, "count", fn_PixelBuffer_count),
	BIND("PixelBuffer", false, "[_]", fn_PixelBuffer_subscript),
	BIND("PixelBuffer", false, "[_]=(_)", fn_PixelBuffer_subscript_set),
	BIND("PixelBuffer", false, "iterate(_)", fn_PixelBuffer_iterate),
	BIND("PixelBuffer", false, "iteratorValue(_)", fn_PixelBuffer_iteratorValue),
	BIND("PixelBuffer", false, "width", fn_PixelBuffer_width),
	BIND("PixelBuffer", false, "height", fn_PixelBuffer_height),
	BIND("PixelBuffer", false, "pixelAt(_,_)", fn_PixelBuffer_pixelAt),
	BIND("PixelBuffer", false, "setPixel(_,_,_)", fn_PixelBuffer_setPixel),
	BIND("PixelBuffer", false, "fill(_)", fn_PixelBuffer_fill),
	BIND("PixelBuffer", false, "clone()", fn_PixelBuffer_clone),
	BIND("PixelBuffer", false, "rgb24", fn_PixelBuffer_rgb24),
	BIND("PixelBuffer", false, "hasAlternate", fn_PixelBuffer_hasAlternate),
	BIND("PixelBuffer", false, "alternateAt(_)", fn_PixelBuffer_alternateAt),
	BIND("PixelBuffer", false, "setAlternate(_,_,_)", fn_PixelBuffer_setAlternate),
	BIND("PixelBuffer", false, "fillAlternate(_)", fn_PixelBuffer_fillAlternate),
	BIND("PixelBuffer", false, "alternateRgb24", fn_PixelBuffer_alternateRgb24),
	BIND("PixelBuffer", false, "setAlternateRgb24(_)", fn_PixelBuffer_setAlternateRgb24),
	BIND("PixelBuffer", false, "clearAlternate()", fn_PixelBuffer_clearAlternate),

	BIND("PixelMask", true, "fromPbm(_)", fn_PixelMask_fromPbm),
	BIND("PixelMask", true, "fromBits(_,_,_)", fn_PixelMask_fromBits),
	BIND("PixelMask", false, "bits", fn_PixelMask_bits),
	BIND("PixelMask", false, "count", fn_PixelMask_count),
	BIND("PixelMask", false, "[_]", fn_PixelMask_subscript),
	BIND("PixelMask", false, "[_]=(_)", fn_PixelMask_subscript_set),
	BIND("PixelMask", false, "iterate(_)", fn_PixelMask_iterate),
	BIND("PixelMask", false, "iteratorValue(_)", fn_PixelMask_iteratorValue),
	BIND("PixelMask", false, "width", fn_PixelMask_width),
	BIND("PixelMask", false, "height", fn_PixelMask_height),
	BIND("PixelMask", false, "bitAt(_,_)", fn_PixelMask_bitAt),
	BIND("PixelMask", false, "setBit(_,_,_)", fn_PixelMask_setBit),
	BIND("PixelMask", false, "fill(_)", fn_PixelMask_fill),

#define BLIT_BIND(name)                                                      \
	BIND("PixelBlit", false, #name, fn_PixelBlit_##name),                 \
	BIND("PixelBlit", false, #name "=(_)", fn_PixelBlit_##name##_set)
	BLIT_BIND(sourceX),
	BLIT_BIND(sourceY),
	BLIT_BIND(sourceWidth),
	BLIT_BIND(sourceHeight),
	BLIT_BIND(destinationX),
	BLIT_BIND(destinationY),
	BLIT_BIND(scaleX),
	BLIT_BIND(scaleY),
	BLIT_BIND(maskX),
	BLIT_BIND(maskY),
	BLIT_BIND(flipX),
	BLIT_BIND(flipY),
#undef BLIT_BIND

	BIND("Screen", true, "pixelSize", fn_Screen_pixelSize),
	BIND("Screen", true, "cellPixelSize", fn_Screen_cellPixelSize),
	BIND("Screen", true, "readPixels(_,_,_,_)", fn_Screen_readPixels),
	BIND("Screen", true, "readPixels(_,_,_,_,_)", fn_Screen_readPixels),
	BIND("Screen", true, "setPixel(_,_,_)", fn_Screen_setPixel),
	BIND("Screen", true, "blitPixels(_,_)", fn_Screen_blitPixels),
	BIND("Screen", true, "blitPixels(_,_,_)", fn_Screen_blitPixels),
};

#undef BIND

WrenForeignMethodFn
wren_bind_pixels_lookup(const char *class_name, bool is_static,
    const char *signature)
{
	for (size_t i = 0; i < sizeof(BINDINGS) / sizeof(BINDINGS[0]); i++) {
		if (BINDINGS[i].is_static == is_static
		    && strcmp(BINDINGS[i].class_name, class_name) == 0
		    && strcmp(BINDINGS[i].signature, signature) == 0)
			return BINDINGS[i].function;
	}
	return NULL;
}

WrenForeignClassMethods
wren_bind_pixels_lookup_class(const char *class_name)
{
	WrenForeignClassMethods methods = {NULL, NULL};

	if (strcmp(class_name, "PixelBuffer") == 0) {
		methods.allocate = pixel_buffer_allocate;
		methods.finalize = pixel_buffer_finalize;
	}
	else if (strcmp(class_name, "PixelMask") == 0) {
		methods.allocate = pixel_mask_allocate;
		methods.finalize = pixel_mask_finalize;
	}
	else if (strcmp(class_name, "PixelBlit") == 0) {
		methods.allocate = pixel_blit_allocate;
		methods.finalize = pixel_blit_finalize;
	}
	return methods;
}
