extern void printc (int c, int x, int y, int color);
extern void printcn (int c,int x, int y);
extern void changecolor (int x, int y, int color);

#include <stddef.h>

char*restorecrtwin (char*s, int xi, int yi, int xf, int yf, int mode)
//sends "s" buffer contents to a text window with mode.
 {
	int c0,c1,chr,color;

	if ((unsigned)mode>=4)
		return NULL;

	xf++; yf++;
	for (c0=yi;c0<yf;c0++)
		for (c1=xi;c1<xf;c1++)
		 {
			if (mode<2)
			 {
				if (mode==0)
				 {
					chr=*s; s++;
					color=*s; s++;
				 }
				 else
				 {
					color=*s; s++;
					chr=*s; s++;
				 }
				printc(chr,c1,c0,color);
			 }
			else
			 {
				if (mode==2)
				 {
					printcn(*s,c1,c0);
					s++;
				 }
				if (mode==3)
				 {
					changecolor(c1,c0,*s);
					s++;
				 }
			 }
		 }
	return s;
 }
