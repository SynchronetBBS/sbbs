#include <stdbool.h>

#include "bitmap_con.h"

struct graphics_buffer {
	uint32_t* data;
	size_t    sz;
	size_t    w;
	size_t    h;
	struct graphics_buffer *next;
};

struct graphics_buffer * get_buffer(void);
void release_buffer(struct graphics_buffer *);
void init_r2y(void);

struct graphics_buffer * do_scale(struct rectlist* rect, int width, int height);
void aspect_correct(int *x, int *y, int aspect_width, int aspect_height);
void aspect_reverse(int *x, int *y, int scrnwidth, int scrnheight, int aspect_width, int aspect_height);
void aspect_fix(int *x, int *y, int aspect_width, int aspect_height);
void aspect_fix_wc(int *x, int *y, bool wc, int aspect_width, int aspect_height);
void aspect_fix_low(int *x, int *y, int aspect_width, int aspect_height);
void calc_scaling_factors(int *x, int *y, int winwidth, int winheight, int aspect_width, int aspect_height, int scrnwidth, int scrnheight);
void aspect_fix_inside(int *x, int *y, int aspect_width, int aspect_height);
