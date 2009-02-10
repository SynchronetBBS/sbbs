#include <ciolib.h>

int getcrtchar (int x, int y)
	//gets the character from position (x,y)
 {
	unsigned char	buf[2];

	gettext(x+1,y+1,x+1,y+1,buf);
	return(buf[0]);
 }
