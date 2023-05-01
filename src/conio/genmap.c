#include <stdio.h>
#include "scale.h"
#include "ciolib.h"
cioapi_t cio_api;

int main(int argc, char **argv)
{
	uint32_t i;
	FILE *c = fopen("rgbmap.c", "wb");
	FILE *h = fopen("rgbmap.h", "wb");
	init_r2y();

	fprintf(c, "#include <inttypes.h>\n\n"
	    "const uint32_t r2y[%u] = {\n", 1 << 24);
	fprintf(h, "#ifndef RGBMAP_H\n"
	    "#define RGBMAP_H\n\n"
	    "#include <inttypes.h>\n\n"
	    "extern const uint32_t r2y[%u];\n"
	    "extern const uint32_t y2r[%u];\n\n"
	    "#endif\n", 1 << 24, 1 << 24);
	for (i = 0; i < (1<<24); i++) {
		if (i % 8 == 0)
			fputs("\t", c);
		fprintf(c, "0x%08x,", r2y[i]);
		if (i % 8 == 7)
			fputs("\n", c);
		else
			fputs(" ", c);
	}
	fprintf(c, "};\n\n"
	    "const uint32_t y2r[%u] = {\n", 1 << 24);
	for (i = 0; i < (1<<24); i++) {
		if (i % 8 == 0)
			fputs("\t", c);
		fprintf(c, "0x%08x,", y2r[i]);
		if (i % 8 == 7)
			fputs("\n", c);
		else
			fputs(" ", c);
	}
	fputs("};\n", c);
}
