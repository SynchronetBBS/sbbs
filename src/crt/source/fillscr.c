extern int vmode_x;
extern int vmode_y;
extern int crt_direct;
extern char*video_addr;

extern void biosprintc (int chr, int x, int y, int color, int func);

void fillscr (int c, int color)
	//fills the screen with character "c" and color "color"
 {
	int c0,c1,a0;
	char*crtvar_p;
	a0=vmode_x*vmode_y; //number of character positions in screen
	crtvar_p=video_addr;
	if (!crt_direct)
	 {
		for (c0=0;c0<a0;c0++)
		 {
			*crtvar_p=(char)c;
			crtvar_p++;
			*crtvar_p=(char)color;
			crtvar_p++;
		 }
		return;
	 }
	for (c0=0;c0<vmode_y;c0++)
		for (c1=0;c1<vmode_x;c1++)
			biosprintc (c,c1,c0,color,0x09);
 }
