extern int vmode_x;
extern int vmode_y;

extern void printct(int c, int x, int y, int color, unsigned int type);

void fillbox (int c, int xi, int yi, int xf, int yf, int color, int func)
	//fills a text-window from (xi,yi) to (xf,yf) with character "c" and color
	//"color", where (xi,yi) is the upper left corner and (xf,yf) is the lower
	//right corner of the window.
 {
	int x,y;
	//Avoids waste of time in unuseless calls to chagecolor
	if (yi<0)
		yi=0;
	if (xi<0)
		xi=0;
	if (yf>=vmode_y)
		yf=vmode_y-1;
	if (xf>=vmode_x)
		xf=vmode_x-1;

	//Replaces the window color.
	for (y=yi;y<(yf+1);y++) //y<(yf+1) => protection against infinite loop
		for (x=xi;x<(xf+1);x++) //when yf=7FFFh. The same for x and xf
			printct (c,x,y,color,func);
 }
