extern void fillbox (int c, int xi, int yi, int xf, int yf, int color, int func);

#include "crtwin.h"

void fillboxw (int c, int color, int func)
 {
	fillbox (c, xi+1, yi+1, xf-1, yf-1, color, func);
 }
