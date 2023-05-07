#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

uint32_t r2y[16777216];
uint32_t y2r[16777216];

#define CLAMP(x) do { \
	if (x < 0) \
		x = 0; \
	else if (x > 255) \
		x = 255; \
} while(0)

void
init_r2y(void)
{
	int r, g, b;
	int y, u, v;
	const double luma = 255.0 / 219;
	const double col  = 255.0 / 224;

	for (r = 0; r < 256; r++) {
		for (g = 0; g < 256; g++) {
			for (b = 0; b < 256; b++) {
				y =  16 + ( 65.738 * r + 129.057 * g +  25.064 * b + 128) / 256;
				CLAMP(y);
				u = 128 + (-37.945 * r -  74.494 * g + 112.439 * b + 128) / 256;
				CLAMP(u);
				v = 128 + (112.439 * r -  94.154 * g -  18.285 * b + 128) / 256;
				CLAMP(v);

				r2y[(r<<16) | (g<<8) | b] = (y<<16)|(u<<8)|v;
			}
		}
	}
	for (y = 0; y < 256; y++) {
		for (u = 0; u < 256; u++) {
			for (v = 0; v < 256; v++) {
				const int c = y - 16;
				const int d = u - 128;
				const int e = v - 128;
				r = luma * c                                     + col * 1.402 * e;
				CLAMP(r);
				g = luma * c - col * 1.772 * (0.114 / 0.587) * d - col * 1.402 * (0.299 / 0.587) * e;
				CLAMP(g);
				b = luma * c + col * 1.772 * d;
				CLAMP(b);

				y2r[(y<<16) | (u<<8) | v] = (r<<16)|(g<<8)|b;
			}
		}
	}
}

int
main(int argc, char **argv)
{
	FILE *s, *h, *r, *y;
	char path[1024];
	char *mangle = "";

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <os> <path>\n", argv[0]);
		return EXIT_FAILURE;
	}
	sprintf(path, "%s/rgbmap.s", argv[2]);
	s = fopen(path, "w");
	sprintf(path, "%s/rgbmap.h", argv[2]);
	h = fopen(path, "w");
	sprintf(path, "%s/r2y.bin", argv[2]);
	r = fopen(path, "wb");
	sprintf(path, "%s/y2r.bin", argv[2]);
	y = fopen(path, "wb");
	init_r2y();
	if (argc > 1 && strcmp(argv[1], "win32") == 0)
		mangle = "_";

	fprintf(s,
	    ".section .rodata\n"
	    ".global %sr2y\n"
	    ".global %sy2r\n"
	    ".align  4\n"
	    "%sr2y:\n"
	    "	.incbin \"%s/r2y.bin\"\n"
	    "%sy2r:\n"
	    "	.incbin \"%s/y2r.bin\"\n", mangle, mangle, argv[2], mangle, argv[2], mangle);
	fprintf(h,
	    "#ifndef RGBMAP_H\n"
	    "#define RGBMAP_H\n"
	    "\n"
	    "#include <inttypes.h>\n"
	    "\n"
	    "extern const uint32_t r2y[16777216];\n"
	    "extern const uint32_t y2r[16777216];\n"
	    "\n"
	    "#endif\n");
	fwrite(r2y, 4, 1 << 24, r);
	fwrite(y2r, 4, 1 << 24, y);
	fclose(s);
	fclose(h);
	fclose(r);
	fclose(y);
	return EXIT_SUCCESS;
}
