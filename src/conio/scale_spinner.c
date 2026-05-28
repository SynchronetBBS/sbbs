#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ciolib.h"
#include "genwrap.h"

static void
usage(const char *prog)
{
	fprintf(stderr,
	    "usage: %s [gGsSxX] [-i|-e] [-d delay_ms] [-t seconds] [-m mode] [-p x,y]\n"
	    "  g/G: GDI window/fullscreen\n"
	    "  s/S: SDL window/fullscreen\n"
	    "  x/X: X11 window/fullscreen\n"
	    "  -i: internal scaling (default)\n"
	    "  -e: external scaling\n"
	    "  -d: delay between frames in ms (default 16, 0 for max rate)\n"
	    "  -t: run time in seconds (default 0, until keypress)\n"
	    "  -m: conio text mode (default current initial mode)\n"
	    "  -p: 1-based spinner position (default screen center)\n",
	    prog);
}

static int
parse_pos(const char *arg, int *x, int *y)
{
	char *endp;
	long lx;
	long ly;

	lx = strtol(arg, &endp, 10);
	if (*endp != ',')
		return 0;
	ly = strtol(endp + 1, &endp, 10);
	if (*endp != '\0' || lx < 1 || ly < 1)
		return 0;
	*x = (int)lx;
	*y = (int)ly;
	return 1;
}

int
main(int argc, char **argv)
{
	static const char spinner[] = "/-\\|";
	struct text_info ti;
	struct vmem_cell cells[sizeof(spinner) - 1];
	struct vmem_cell *fill;
	int mode = CIOLIB_MODE_AUTO;
	int delay_ms = 16;
	int duration_s = 0;
	int text_mode = 0;
	int x = 0;
	int y = 0;
	int use_external = 0;
	uint64_t start;
	uint64_t frames = 0;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			usage(argv[0]);
			return 0;
		}
		if (strcmp(argv[i], "-i") == 0) {
			use_external = 0;
			continue;
		}
		if (strcmp(argv[i], "-e") == 0) {
			use_external = 1;
			continue;
		}
		if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
			delay_ms = atoi(argv[++i]);
			if (delay_ms < 0)
				delay_ms = 0;
			continue;
		}
		if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
			duration_s = atoi(argv[++i]);
			if (duration_s < 0)
				duration_s = 0;
			continue;
		}
		if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
			text_mode = atoi(argv[++i]);
			continue;
		}
		if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
			if (!parse_pos(argv[++i], &x, &y)) {
				usage(argv[0]);
				return 1;
			}
			continue;
		}

		switch(argv[i][0]) {
			case 'G':
				mode = CIOLIB_MODE_GDI_FULLSCREEN;
				break;
			case 'g':
				mode = CIOLIB_MODE_GDI;
				break;
			case 'S':
				mode = CIOLIB_MODE_SDL_FULLSCREEN;
				break;
			case 's':
				mode = CIOLIB_MODE_SDL;
				break;
			case 'X':
				mode = CIOLIB_MODE_X_FULLSCREEN;
				break;
			case 'x':
				mode = CIOLIB_MODE_X;
				break;
			default:
				usage(argv[0]);
				return 1;
		}
	}

	ciolib_initial_scaling_type = use_external
	    ? CIOLIB_SCALING_EXTERNAL
	    : CIOLIB_SCALING_INTERNAL;
	if (text_mode)
		ciolib_initial_mode = text_mode;
	if (initciolib(mode) != 0)
		return 1;
	if (text_mode)
		textmode(text_mode);

	gettextinfo(&ti);
	if (x == 0)
		x = ti.screenwidth / 2;
	if (y == 0)
		y = ti.screenheight / 2;
	if (x < 1 || x > ti.screenwidth || y < 1 || y > ti.screenheight) {
		fprintf(stderr, "position %d,%d is outside %ux%u screen\n",
		    x, y, ti.screenwidth, ti.screenheight);
		return 1;
	}

	_setcursortype(_NOCURSOR);
	fill = malloc(ti.screenwidth * ti.screenheight * sizeof(*fill));
	if (fill == NULL)
		return 1;
	for (size_t i = 0; i < ti.screenwidth * ti.screenheight; i++)
		ciolib_set_vmem(&fill[i], 0xb2, LIGHTGRAY, ciolib_attrfont(LIGHTGRAY));
	vmem_puttext(1, 1, ti.screenwidth, ti.screenheight, fill);
	free(fill);
	for (size_t i = 0; i < sizeof(cells) / sizeof(cells[0]); i++)
		ciolib_set_vmem(&cells[i], spinner[i], LIGHTGRAY, ciolib_attrfont(LIGHTGRAY));

	start = xp_timer64();
	for (;;) {
		uint64_t now;

		vmem_puttext(x, y, x, y, &cells[frames % (sizeof(cells) / sizeof(cells[0]))]);
		frames++;

		if (kbhit())
			break;
		now = xp_timer64();
		if (duration_s > 0 && now - start >= (uint64_t)duration_s * 1000)
			break;
		if (delay_ms > 0)
			SLEEP(delay_ms);
	}

	_setcursortype(_NORMALCURSOR);
	fprintf(stderr, "frames=%llu elapsed_ms=%llu\n",
	    (unsigned long long)frames,
	    (unsigned long long)(xp_timer64() - start));
	return 0;
}
