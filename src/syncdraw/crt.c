#include <stdlib.h>

#include <ciolib.h>

void 
DrawBox(int x1, int y1, int x2, int y2)
{				/* Draws a nice looking box ... */
	int		width,height;
	int             x, y;
	int		i;
	char		*buf;

	width=x2-x1+3;
	height=y2-y1+1;
	if(width>height)
		buf=(char *)malloc(width*2);
	else
		buf=(char *)malloc(height*2);

	i=0;
	buf[i++]=220;
	buf[i++]=2;
	buf[i++]=254;
	buf[i++]=10;
	for(x=3;x<=width-4;x++) {
		buf[i++]=223;
		buf[i++]=10;
	}
	buf[i++]=254;
	buf[i++]=10;
	buf[i++]=220;
	buf[i]=2;
	puttext(x1,y1,x2,y1,buf);
	i=4;
	for(x=3;x<=width-4;x++) {
		buf[i++]=220;
		buf[i++]=2;
	}
	i+=2;
	buf[i]=223;
	buf[0]=223;
	puttext(x1,y2,x2,y2,buf);

	i=0;
	for(y=1; y<=height-2;y++) {
		buf[i++]=221;
		if(y==1 || y==height-2)
			buf[i++]=10;
		else if (y==2 || y==height-3)
			buf[i++]=7;
		else
			buf[i++]=8;
	}
	puttext(x1,y1+1,x1,y2-1,buf);
	i=0;
	for(y=1; y<=height-2;y++) {
		buf[i++]=222;
		i++;
	}
	puttext(x2,y1+1,x2,y2-1,buf);
	free(buf);
}
