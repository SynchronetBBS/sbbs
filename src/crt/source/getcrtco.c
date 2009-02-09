#include <ciolib.h>

int getcrtcolor (int x,int y)
	//gets the color from position (x,y)
 {
	unsigned char buf[2];

	gettext(x+1,y+1,x+1,y+1,buf);
	return(buf[1]);
 }
