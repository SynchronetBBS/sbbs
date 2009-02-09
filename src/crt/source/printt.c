#include "crtwin.h"

extern void printc(int c, int x, int y, int color);

int printtext (char *s, int x, int y, int color)
//prints a string inside a text window defined by crtwin_dta, continuing into
//the next line if necessary
 {
	unsigned cnt=0;
	unsigned c0, c1;
	if (x<0 || y<0)
		return 0;
	c1=xi+x+1;
	for (c0=yi+1+y;(unsigned)c0<(unsigned)yf;c0++)
	 {
		while (c1<(unsigned)xf)
		 {
			if (!(*s))
				return cnt;
			cnt++;
			printc(*s,c1,c0,color);
			s++;
			c1++;
		 }
		c1=xi+1;
	 }
	return cnt;
 }
