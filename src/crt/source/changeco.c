#include <ciolib.h>

void changecolor(int x, int y, int color)
  //changes the color at position (x,y) replacing the old color by "color"
 {
	unsigned char	buf[2];

	gettext(x+1,y+1,x+1,y+1,buf);
	buf[1]=color;
 }
