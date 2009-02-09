char crtframe_mat[]="ÚÄ¿³ÙÄÀ³ÉÍ»º¼ÍÈºÞßÝÝÝÜÞÞÛÛÛÛÛÛÛÛ";
	//these characters are used to build frames with crtframe
extern void printc(int c, int x, int y, int color);

void crtframe (int xi, int yi, int xf, int yf, int color, unsigned int type)
 //creates a text frame from (xi,yi) to (xf,yf) with color and type, where
 //(xi,yi) is the upper left corner and (xf,yf) is the lower right corner.
 {
	int x,y,a0,a1;
	if (type>=0x100) //selects character obtained by (type%256) as
	 {					  //frame character
		a0=type%0x100;
		for (a1=24;a1<32;a1++)
			crtframe_mat[a1]=a0;
		type=3;
	 }
	if (type>3)	//if (type>3) and (type<100h) return => these values are
		return;  //reserved for future versions
					//if type==3 crtframe draws a frame with the last frame
					//character selected when (type>=100h) (default='Û')
	//draws horizontal borders
	type*=8;
	a0=crtframe_mat[type+1];
	a1=crtframe_mat[type+5];
	for (x=xi+1;x<xf;x++)
	 {
		printc (a0,x,yi,color);
		printc (a1,x,yf,color);
	 }
	//draws vertical borders
	a0=crtframe_mat[type+3];
	a1=crtframe_mat[type+7];
	for (y=yi+1;y<yf;y++)
	 {
		printc (a1,xi,y,color);
		printc (a0,xf,y,color);
	 }
	 //draws the corners
	printc (crtframe_mat[type  ],xi,yi,color);
	printc (crtframe_mat[type+2],xf,yi,color);
	printc (crtframe_mat[type+6],xi,yf,color);
	printc (crtframe_mat[type+4],xf,yf,color);
 }
