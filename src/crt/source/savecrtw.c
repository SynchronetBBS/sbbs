extern int getcrtchar (int x, int y);
extern int getcrtcolor (int x,int y);

#include <stddef.h>

char*savecrtwin (char*s, int xi, int yi, int xf, int yf, int mode)
//saves a text window into "s" buffer with mode.
 {
	int c0,c1;

	if ((unsigned)mode>=4)
		return NULL;

	yf++; xf++;
	for (c0=yi;c0<yf;c0++)
		for (c1=xi;c1<xf;c1++)
		 {
			if (mode==1)
			 {
				*s=getcrtcolor(c1,c0); s++;
				*s=getcrtchar (c1,c0); s++;
			 }
			else
			 {
				if (mode!=3)
				 {
					*s=getcrtchar(c1,c0);
					s++;
				 }
				if (mode!=2)
				 {
					*s=getcrtcolor(c1,c0);
					s++;
				 }
			 }
		 }
	return s;
 }
