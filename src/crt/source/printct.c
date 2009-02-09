extern int vmode_x;
extern int vmode_y;
extern int crt_direct;

extern char*video_addr;
extern char*crtvar_p;

extern int getcrtchar (int x, int y);
extern void biosprintc (int chr, int x, int y, int color, int func);

void printct(int c, int x, int y, int color, unsigned int type)
	//replaces the character and/or color of position (x,y) according
	//to type parameter.
 {
	if ((unsigned)x>=(unsigned)vmode_x || (unsigned)y>=(unsigned)vmode_y)
		return;
	if (!crt_direct)
	 {
		crtvar_p=video_addr + 2 * (x + y*vmode_x);
		if (type & 0x01)
			*crtvar_p=(char)c;
		crtvar_p++;
		if (type & 0x02)
			*crtvar_p=(char)color;
		return;
	 }
	if (!(type & 0x01))
		c=getcrtchar(x,y);
	if (!(type & 0x02))
		color=0x07;
	biosprintc (c,x,y,color,0x0A-(type & 0x02)/2U);
 }
