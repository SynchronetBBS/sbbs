/* Copyright (C), 2026 by Stephen Hurd */

#ifndef SYNCTERM_PIXEL_IMAGE_H
#define SYNCTERM_PIXEL_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <ciolib.h>

struct ciolib_pixels *pixel_image_alloc_pixels(uint32_t width, uint32_t height);
struct ciolib_mask *pixel_image_alloc_mask(uint32_t width, uint32_t height);

struct ciolib_pixels *pixel_image_decode_ppm(const void *data, size_t length);
struct ciolib_pixels *pixel_image_decode_ppm_file(const char *path);
struct ciolib_mask *pixel_image_decode_pbm(const void *data, size_t length);
struct ciolib_mask *pixel_image_decode_pbm_file(const char *path);

bool pixel_image_jxl_supported(void);
struct ciolib_pixels *pixel_image_decode_jxl(const void *data, size_t length);
struct ciolib_pixels *pixel_image_decode_jxl_file(const char *path);

#endif
