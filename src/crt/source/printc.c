
extern int vmode_x;
extern int vmode_y;
extern int crt_direct;

extern char*video_addr;
extern char*crtvar_p;

extern void biosprintc (int chr, int x, int y, int color, int func);

void printc(int c, int x, int y, int color)
 //prints the character "c" with color "color" at position (x,y).
 {
	if ((unsigned)x>=(unsigned)vmode_x || (unsigned)y>=(unsigned)vmode_y)
		return;
	if (!crt_direct)
	 {
		crtvar_p=video_addr + 2 * (x + y*vmode_x);
		*crtvar_p=(char)c;
		crtvar_p++;
		*crtvar_p=(char)color;
		return;
	 }
	biosprintc (c,x,y,color,0x09);
 }
