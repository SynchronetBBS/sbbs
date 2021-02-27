#include <stdlib.h>

#include <ciolib.h>

#include "attrs.h"

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
	if (!buf)
		exit(1);

	i=0;
	buf[i++]='\xda';
	buf[i++]=ATTR_NORM_HIGH;
	for(x=2;x<=width-4;x++) {
		buf[i++]='\xc4';
		buf[i++]=ATTR_NORM_HIGH;
	}
	buf[i++]='\xc4';
	buf[i++]=ATTR_NORM_HIGH;
	buf[i++]='\xbf';
	buf[i++]=ATTR_NORM_HIGH;
	puttext(x1,y1,x2,y1,buf);
	buf[i-2]='\xd9';
	buf[0]='\xc0';
	puttext(x1,y2,x2,y2,buf);

	i=0;
	for(y=1; y<=height-2;y++) {
		buf[i++]='\xb3';
		if(y==1 || y==height-2)
			buf[i++]=ATTR_NORM_HIGH;
		else if (y==2 || y==height-3)
			buf[i++]=ATTR_NORM_FADE1;
		else
			buf[i++]=ATTR_NORM_FADE2;
	}
	puttext(x1,y1+1,x1,y2-1,buf);
	puttext(x2,y1+1,x2,y2-1,buf);
	free(buf);
}
