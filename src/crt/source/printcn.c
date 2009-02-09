extern int vmode_x;
extern int vmode_y;
extern int crt_direct;
extern char*video_addr;
extern char*crtvar_p;

extern void biosprintc (int chr, int x, int y, int color, int func);

void printcn(int c,int x, int y)
  //prints a character at position (x,y) keeping the old position color.
 {
	if ((unsigned)x>=(unsigned)vmode_x || (unsigned)y>=(unsigned)vmode_y)
		return;
	if (!crt_direct)
	 {
		crtvar_p=video_addr + 2 * (x + y*vmode_x);
		*crtvar_p=(char)c;
		return;
	 }
	biosprintc (c,x,y,0x07,0x0a);
 }
