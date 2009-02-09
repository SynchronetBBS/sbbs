extern int vmode_x;
extern int vmode_y;
extern int crt_direct;
extern char*video_addr;

extern int getcrtchar (int x, int y);
extern int getcrtcolor (int x,int y);

char*savevideo (char*s) //saves video contents into "s" buffer
 {
	int c0,c1,a0;
	char*crtvar_p; //used to access the video RAM

	if (!crt_direct)
	 {
		a0=vmode_x*vmode_y;
		crtvar_p=video_addr; //points to video RAM
		for (c0=0;c0<a0;c0++)
		 {
			*s=*crtvar_p;
			*(s+1)=*(crtvar_p+1);
			crtvar_p+=2; s+=2;
		 }
	 }
	 else
		for (c0=0;c0<vmode_y;c0++)
			for (c1=0;c1<vmode_x;c1++)
			 {
				*s=getcrtchar(c1,c0); s++;
				*s=getcrtcolor(c1,c0); s++;
			 }
	return s;
 }
