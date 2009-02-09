#include <ciolib.h>

void biosprintc (int chr, int x, int y, int color, int func)
//prints a character at position (x,y) with color "color" if func=09h.
//if func=0Ah prints character with no color.
 {
	unsigned char buf[2];

	if(func==0x0a)
		gettext(x+1, y+1, x+1, y+1, buf);
	buf[0]=chr;
	if(func==0x09)
		buf[1]=color;
	puttext(x+1, y+1, x+1,y+1,buf);
 }
