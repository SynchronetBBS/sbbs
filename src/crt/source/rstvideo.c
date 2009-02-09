extern int vmode_x;
extern int vmode_y;
extern int crt_direct;
extern char*video_addr;

extern void biosprintc (int chr, int x, int y, int color, int func);

char*restorevideo (char*s) //sends "s" buffer contents to screen
 {
	int c0,c1,a0,chr;
	char*crtvar_p; //used to access the video RAM

	if (!crt_direct)
	 {
		a0=vmode_x*vmode_y;
		crtvar_p=video_addr;        //points to video RAM
		for (c0=0;c0<a0;c0++)
		 {
			*crtvar_p=*s;
			*(crtvar_p+1)=*(s+1);
			crtvar_p+=2; s+=2;
		 }
	 }
	 else
		for (c0=0;c0<vmode_y;c0++)
			for (c1=0;c1<vmode_x;c1++)
			 {
				chr=*s; s++;
				biosprintc(chr,c1,c0,*s,0x09);
				s++;
			 }
	return s;
 }
