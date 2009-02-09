extern int getcrtchar (int x, int y);
extern int getcrtcolor (int x,int y);

char*savevideowin(char*s, int xi, int yi, int xf, int yf)
//saves a text window into "s" buffer.
 {
	int c0,c1;
	yf++; xf++;
	for (c0=yi;c0<yf;c0++)
		for (c1=xi;c1<xf;c1++)
		 {
			*s=getcrtchar(c1,c0); s++;
			*s=getcrtcolor(c1,c0); s++;
		 }
	return s;
 }
