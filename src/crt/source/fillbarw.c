extern void fillbar (int c, int xi, int yi, int xf, int yf, int color);

#include "crtwin.h"

void fillbarw (int c, int color)
 {
	fillbar (c,xi+1,yi+1,xf-1,yf-1,color);
 }
