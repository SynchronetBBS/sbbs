#!/usr/bin/env python3
"""Compare RIPscrip rendering between DOSBox/RIPterm and SyncTERM.

Normalizes the different pixel geometries (DOSBox 640x350 native EGA vs
SyncTERM 640x480 aspect-corrected) by upscaling the 350-row image using
the same YCoCg-space interpolation SyncTERM uses, then compares at 640x480.

Usage:
    python3 ripdiff.py <image_a> <image_b> [output_dir]
    python3 ripdiff.py --no-snap <image_a> <image_b> [output_dir]
"""

import argparse
import os
import struct
import subprocess
import sys
from PIL import Image

EGA_PALETTE = [
	(0, 0, 0),       (0, 0, 170),     (0, 170, 0),     (0, 170, 170),
	(170, 0, 0),     (170, 0, 170),   (170, 85, 0),    (170, 170, 170),
	(85, 85, 85),    (85, 85, 255),   (85, 255, 85),   (85, 255, 255),
	(255, 85, 85),   (255, 85, 255),  (255, 255, 85),  (255, 255, 255),
]

def snap_to_ega(r, g, b):
	best = 0
	best_dist = (r - EGA_PALETTE[0][0])**2 + (g - EGA_PALETTE[0][1])**2 + (b - EGA_PALETTE[0][2])**2
	for i in range(1, 16):
		er, eg, eb = EGA_PALETTE[i]
		d = (r - er)**2 + (g - eg)**2 + (b - eb)**2
		if d < best_dist:
			best_dist = d
			best = i
	return EGA_PALETTE[best]


def rgb_to_ycocg(r, g, b):
	"""Convert RGB to YCoCg, matching scale.c RGB_to_YCoCg()."""
	co = r - b                    # -255 to 255
	tmp = b + (co // 2)
	cg = g - tmp
	y = tmp + (cg // 2)
	return (y, co, cg)


def ycocg_to_rgb(y, co, cg):
	"""Convert YCoCg to RGB, matching scale.c YCoCg_to_RGB()."""
	tmp = y - (cg // 2)
	gi = cg + tmp
	bi = tmp - (co // 2)
	ri = bi + co
	r = max(0, min(255, ri))
	g = max(0, min(255, gi))
	b = max(0, min(255, bi))
	return (r, g, b)


def blend_ycocg(c1, c2, weight):
	"""Blend two RGB tuples in YCoCg space, matching scale.c blend_YCoCg().

	weight is 0..65535 (16-bit fractional part).
	"""
	iw = 65535 - weight
	y1, co1, cg1 = rgb_to_ycocg(*c1)
	y2, co2, cg2 = rgb_to_ycocg(*c2)
	y3 = (y1 * iw + y2 * weight) // 65535
	co3 = (co1 * iw + co2 * weight) // 65535
	cg3 = (cg1 * iw + cg2 * weight) // 65535
	return ycocg_to_rgb(y3, co3, cg3)


def interpolate_height_up(img, newheight):
	"""Upscale image height using YCoCg interpolation, matching scale.c."""
	width, height = img.size
	src = img.load()
	dst = Image.new('RGB', (width, newheight))
	dpix = dst.load()

	mult = (height << 16) // newheight
	ypos = 0
	for y in range(newheight):
		yposi = ypos >> 16
		weight = ypos & 0xffff
		if weight == 0 or yposi >= height - 1:
			for x in range(width):
				dpix[x, y] = src[x, yposi]
		else:
			for x in range(width):
				pix1 = src[x, yposi]
				pix2 = src[x, yposi + 1]
				if pix1 == pix2:
					dpix[x, y] = pix1
				else:
					dpix[x, y] = blend_ycocg(pix1, pix2, weight)
		ypos += mult
	return dst


def load_xwd(path):
	"""Load an XWD file natively (format version 7, 32bpp)."""
	with open(path, 'rb') as f:
		data = f.read()

	header_size, file_version = struct.unpack_from('>II', data, 0)
	if file_version != 7:
		return None

	fields = struct.unpack_from('>13I', data, 0)
	pixmap_depth = fields[3]
	width = fields[4]
	height = fields[5]
	byte_order = fields[7]  # 0=LSBFirst, 1=MSBFirst
	bits_per_pixel = fields[11]
	bytes_per_line = fields[12]

	more = struct.unpack_from('>7I', data, 52)
	visual_class = more[0]
	red_mask = more[1]
	green_mask = more[2]
	blue_mask = more[3]
	ncolors = more[6]

	# Colormap follows header: ncolors entries, each 12 bytes
	cmap_offset = header_size
	colormap = []
	for i in range(ncolors):
		off = cmap_offset + i * 12
		# XWD colormap entry: pixel(4), red(2), green(2), blue(2), flags(1), pad(1)
		pixel, r, g, b = struct.unpack_from('>IHHH', data, off)
		colormap.append((r >> 8, g >> 8, b >> 8))

	pixel_offset = cmap_offset + ncolors * 12
	img = Image.new('RGB', (width, height))
	pix = img.load()

	for y in range(height):
		row_off = pixel_offset + y * bytes_per_line
		for x in range(width):
			poff = row_off + x * (bits_per_pixel // 8)
			if byte_order == 0:  # LSBFirst
				val = struct.unpack_from('<I', data, poff)[0]
			else:
				val = struct.unpack_from('>I', data, poff)[0]

			if visual_class in (4, 3) and pixmap_depth <= 8:  # PseudoColor / StaticColor
				idx = val & ((1 << pixmap_depth) - 1)
				if idx < len(colormap):
					pix[x, y] = colormap[idx]
				else:
					pix[x, y] = (0, 0, 0)
			else:  # DirectColor / TrueColor / deep PseudoColor (packed RGB)
				r = (val & red_mask) >> 16
				g = (val & green_mask) >> 8
				b = val & blue_mask
				pix[x, y] = (r, g, b)

	return img


def load_image(path):
	"""Load an image: try PIL first, then native XWD, then ImageMagick."""
	if not path.lower().endswith('.xwd'):
		try:
			img = Image.open(path)
			img.load()
			return img.convert('RGB')
		except Exception:
			pass

	# Try native XWD reader
	try:
		img = load_xwd(path)
		if img is not None:
			return img
	except Exception as e:
		print(f"Warning: native XWD reader failed on {path}: {e}", file=sys.stderr)

	# Fallback: PIL then ImageMagick
	try:
		img = Image.open(path)
		img.load()
		return img.convert('RGB')
	except Exception:
		pass

	for cmd in ['magick', 'convert']:
		try:
			result = subprocess.run(
				[cmd, 'xwd:' + path, 'png:-'],
				capture_output=True, check=True
			)
			from io import BytesIO
			img = Image.open(BytesIO(result.stdout))
			img.load()
			return img.convert('RGB')
		except FileNotFoundError:
			continue
		except subprocess.CalledProcessError:
			continue
	print(f"Error: Cannot load {path}", file=sys.stderr)
	sys.exit(1)


def snap_image(img):
	"""Snap every pixel in an image to the nearest EGA palette color."""
	pixels = img.load()
	w, h = img.size
	for y in range(h):
		for x in range(w):
			r, g, b = pixels[x, y]
			pixels[x, y] = snap_to_ega(r, g, b)
	return img


def count_diffs(img_a, img_b):
	"""Count mismatching pixels without creating a diff image."""
	w, h = img_a.size
	pix_a = img_a.load()
	pix_b = img_b.load()
	mismatches = 0

	for y in range(h):
		for x in range(w):
			if pix_a[x, y] != pix_b[x, y]:
				mismatches += 1

	return mismatches


def make_diff(img_a, img_b):
	"""Create a diff image: matching pixels dimmed, differing pixels bright red."""
	w, h = img_a.size
	diff = Image.new('RGB', (w, h))
	pix_a = img_a.load()
	pix_b = img_b.load()
	pix_d = diff.load()
	mismatches = 0

	for y in range(h):
		for x in range(w):
			a = pix_a[x, y]
			b = pix_b[x, y]
			if a == b:
				pix_d[x, y] = (a[0] // 2, a[1] // 2, a[2] // 2)
			else:
				pix_d[x, y] = (255, 0, 0)
				mismatches += 1

	return diff, mismatches


def main():
	parser = argparse.ArgumentParser(
		description='Compare RIPscrip rendering between DOSBox and SyncTERM')
	parser.add_argument('image_a', help='First capture (XWD or PNG)')
	parser.add_argument('image_b', help='Second capture (XWD or PNG)')
	parser.add_argument('output_dir', nargs='?', default='.',
		help='Output directory (default: current dir)')
	parser.add_argument('--no-snap', action='store_true',
		help='Skip EGA palette snapping')
	parser.add_argument('--threshold', type=int, default=0,
		help='Per-channel color distance tolerance (default: 0)')
	args = parser.parse_args()

	os.makedirs(args.output_dir, exist_ok=True)

	# Load images
	img_a = load_image(args.image_a)
	img_b = load_image(args.image_b)

	print(f"Image A: {args.image_a} ({img_a.size[0]}x{img_a.size[1]})")
	print(f"Image B: {args.image_b} ({img_b.size[0]}x{img_b.size[1]})")

	# Normalize dimensions by upscaling 350 -> 480 using SyncTERM's algorithm
	label_a = os.path.basename(args.image_a)
	label_b = os.path.basename(args.image_b)

	if img_a.size == (640, 480) and img_b.size == (640, 350):
		print(f"Upscaling {label_b} from 640x350 to 640x480 (YCoCg interpolation)")
		img_b = interpolate_height_up(img_b, 480)
	elif img_a.size == (640, 350) and img_b.size == (640, 480):
		print(f"Upscaling {label_a} from 640x350 to 640x480 (YCoCg interpolation)")
		img_a = interpolate_height_up(img_a, 480)
	elif img_a.size == img_b.size:
		print(f"Both images are {img_a.size[0]}x{img_a.size[1]}, no scaling needed")
	else:
		print(f"Error: Unexpected dimensions {img_a.size} vs {img_b.size}", file=sys.stderr)
		print("Expected 640x350 (DOSBox) and/or 640x480 (SyncTERM)", file=sys.stderr)
		sys.exit(1)

	# Snap to EGA palette
	if not args.no_snap:
		print("Snapping to EGA palette...")
		img_a = snap_image(img_a.copy())
		img_b = snap_image(img_b.copy())

	# Save normalized versions
	norm_a = os.path.join(args.output_dir, 'normalized_a.png')
	norm_b = os.path.join(args.output_dir, 'normalized_b.png')
	img_a.save(norm_a)
	img_b.save(norm_b)
	print(f"Saved: {norm_a}")
	print(f"Saved: {norm_b}")

	# Compare
	if args.threshold > 0:
		w, h = img_a.size
		diff = Image.new('RGB', (w, h))
		pix_a = img_a.load()
		pix_b = img_b.load()
		pix_d = diff.load()
		mismatches = 0
		for y in range(h):
			for x in range(w):
				a = pix_a[x, y]
				b = pix_b[x, y]
				if (abs(a[0] - b[0]) <= args.threshold and
				    abs(a[1] - b[1]) <= args.threshold and
				    abs(a[2] - b[2]) <= args.threshold):
					pix_d[x, y] = (a[0] // 2, a[1] // 2, a[2] // 2)
				else:
					pix_d[x, y] = (255, 0, 0)
					mismatches += 1
	else:
		diff, mismatches = make_diff(img_a, img_b)

	diff_path = os.path.join(args.output_dir, 'diff.png')
	diff.save(diff_path)

	total = img_a.size[0] * img_a.size[1]
	matching = total - mismatches
	pct = matching / total * 100

	print(f"\nResults:")
	print(f"  Total pixels:    {total}")
	print(f"  Matching:        {matching}")
	print(f"  Different:       {mismatches}")
	print(f"  Match:           {pct:.2f}%")
	print(f"  Diff image:      {diff_path}")

	sys.exit(0 if mismatches == 0 else 1)


if __name__ == '__main__':
	main()
