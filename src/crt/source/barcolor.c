extern int vmode_x;
extern int vmode_y;

extern void changecolor(int x, int y, int color);

void barcolor (int xi, int yi, int xf, int yf, int color)
//replaces the window color. The window corners are (xi,yi) and (xf,yf)
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
			changecolor (x,y,color);

 }
