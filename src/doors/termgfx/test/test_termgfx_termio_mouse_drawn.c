/* The SGR mouse mapper must invert where the frame was DRAWN, not where the
 * fit asked for it to go.
 *
 * On the sixel tier those differ. A sixel is placed by CUP at a text cell, so
 * its true top-left is that cell's pixel -- the centering offset rounded DOWN
 * to the cell grid -- and the vstep trim may since have shrunk and re-centred
 * the image as well. Mapping against the requested rect biases every click by
 * the difference: on a 1330x1480 canvas with 20px cells the image is centred at
 * y=314 but drawn at y=300, about 7 game pixels of upward bias, which is enough
 * that the top of a button stops responding (syncconquer, live).
 *
 * Clicks at the image's own top-left and bottom-right corners pin it. The
 * bottom-right is the discriminator: the top-left clamps to (0,0) either way,
 * while a biased mapper misses the far corner by the whole offset.
 *
 * Geometry comes off the WIRE (the CUP that positions the sixel, and the
 * raster attributes that state its size) rather than being recomputed here, so
 * the test asserts against what the terminal was actually told.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "termgfx_termio.h"

int termgfx_termio_test_set_geom(int canvas_w, int canvas_h, int cell_w, int cell_h,
                                 int cols, int rows, int pixels);
int termgfx_termio_test_mouse_report(int b, int col, int row, int release);

static int drain(int fd, char *buf, int cap)
{
	int     n = 0;
	ssize_t r;

	while (n < cap - 1 && (r = recv(fd, buf + n, (size_t)(cap - 1 - n), MSG_DONTWAIT)) > 0)
		n += (int)r;
	buf[n] = '\0';
	return n;
}

int main(void)
{
	static uint8_t        idx[TERMGFX_TERMIO_FB_W * TERMGFX_TERMIO_FB_H];
	static uint8_t        pal[768];
	static char           out[1 << 22];
	termgfx_input_event_t ev;
	char                  fdarg[32];
	char *                argv[3];
	const char *          p;
	int                   sv[2], n, irow = 0, icol = 0, ew = 0, eh = 0;
	int                   x0, y0;
	double                cw, ch;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	snprintf(fdarg, sizeof fdarg, "-s%d", sv[1]);
	argv[0] = (char *)"test_termgfx_termio_mouse_drawn";
	argv[1] = fdarg;
	argv[2] = NULL;
	assert(termgfx_termio_init(2, argv) == 1);
	termgfx_termio_flush();
	drain(sv[0], out, sizeof out);

	/* A Windows-Terminal-alike: sixel, no JXL, exact 1330x1480 canvas on a
	 * 66x74 grid of 20x20 cells -- the geometry a live session reported, and
	 * the one whose vertical centering does NOT land on a cell boundary. */
	{
		const char *r = "\x1b[?62;4c" "\x1b[74;66R" "\x1b[4;1480;1330t" "\x1b[6;20;20t";
		assert(send(sv[0], r, strlen(r), 0) > 0);
	}
	termgfx_termio_pump();
	assert(termgfx_termio_have_sixel() == 1);

	memset(idx, 3, sizeof idx);
	memset(pal, 0x30, sizeof pal);
	termgfx_termio_present(idx, pal);
	termgfx_termio_flush();
	n = drain(sv[0], out, sizeof out);
	assert(n > 0);

	/* The LAST CUP before the sixel introducer is what positions the image. */
	p = strstr(out, "\x1bP");
	assert(p != NULL);
	{
		const char *q = out, *last = NULL;

		while ((q = strstr(q, "\x1b[")) != NULL && q < p) {
			int r_, c_;

			if (sscanf(q, "\x1b[%d;%dH", &r_, &c_) == 2)
				last = q;
			q += 2;
		}
		assert(last != NULL);
		assert(sscanf(last, "\x1b[%d;%dH", &irow, &icol) == 2);
	}
	/* Raster attributes carry the emitted size: DCS ... q " 1;1;W;H */
	{
		const char *r = strstr(p, "\"1;1;");

		assert(r != NULL);
		assert(sscanf(r, "\"1;1;%d;%d", &ew, &eh) == 2);
	}
	assert(ew > 0 && eh > 0);

	/* The addressed cell's pixel -- the image's true top-left. Same double
	 * math the placement used, not an assumed 8x16 cell. */
	cw = 1330.0 / 66.0;
	ch = 1480.0 / 74.0;
	x0 = (int)((icol - 1) * cw);
	y0 = (int)((irow - 1) * ch);
	/* The bug only exists where the centering does not already land on a cell
	 * boundary; assert this geometry still provides that, so the test cannot
	 * quietly stop testing anything. */
	assert(y0 % (int)ch == 0);
	assert(irow > 1);

	/* Pixel-granular mouse on the same geometry, so a report IS a canvas
	 * pixel and the mapping is read directly. Does not disturb what was drawn. */
	termgfx_termio_test_set_geom(1330, 1480, 20, 20, 66, 74, /*pixels=*/ 1);

	/* Top-left of the drawn image -> game (0,0). Reports are 1-based. */
	termgfx_termio_test_mouse_report(/*b=*/ 32, x0 + 1, y0 + 1, 0);
	assert(termgfx_termio_next_event(&ev) && ev.type == TERMGFX_EV_MOUSE_MOVE);
	assert(ev.x == 0 && ev.y == 0);

	/* Bottom-right -> the far corner of the game frame. A mapper biased by the
	 * cell remainder lands short here, with no clamp to hide it. */
	termgfx_termio_test_mouse_report(/*b=*/ 32, x0 + ew, y0 + eh, 0);
	assert(termgfx_termio_next_event(&ev) && ev.type == TERMGFX_EV_MOUSE_MOVE);
	assert(ev.x == TERMGFX_TERMIO_FB_W - 1);
	assert(ev.y == TERMGFX_TERMIO_FB_H - 1);

	/* Centre -> centre, within a pixel of rounding. */
	termgfx_termio_test_mouse_report(/*b=*/ 32, x0 + ew / 2, y0 + eh / 2, 0);
	assert(termgfx_termio_next_event(&ev) && ev.type == TERMGFX_EV_MOUSE_MOVE);
	assert(ev.x >= TERMGFX_TERMIO_FB_W / 2 - 1 && ev.x <= TERMGFX_TERMIO_FB_W / 2 + 1);
	assert(ev.y >= TERMGFX_TERMIO_FB_H / 2 - 1 && ev.y <= TERMGFX_TERMIO_FB_H / 2 + 1);

	printf("mouse maps against the drawn rect: image %dx%d at cell(%d,%d) = px(%d,%d)\n",
	       ew, eh, icol, irow, x0, y0);
	return 0;
}
