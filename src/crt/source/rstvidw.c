extern void printc (int c, int x, int y, int color);

char*restorevideowin(char*s, int xi, int yi, int xf, int yf)
//sends "s" buffer contents to a text window.
 {
	int c0,c1,a0;
	yf++; xf++;
	for (c0=yi;c0<yf;c0++)
		for (c1=xi;c1<xf;c1++)
		 {
			a0=*s;
			s++;
			printc(a0,c1,c0,*s);
			s++;
		 }
	return s;
 }
