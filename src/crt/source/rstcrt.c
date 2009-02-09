extern int vmode_x;
extern int vmode_y;
extern int crt_direct;
extern char*video_addr;

extern void printc (int chr, int x, int y, int color);
extern void printcn (int c,int x, int y);
extern void changecolor (int x, int y, int color);

#include <stddef.h>

char*restorecrt (char*s, int mode)
//sends "s" buffer contents to screen with mode
 {
	int c0,c1,a0,chr,color;
	char*crtvar_p;//used to access the video RAM

	if ((unsigned)mode>=4)
		return NULL;

	if (!crt_direct)
	 {
		a0=vmode_x*vmode_y;
		crtvar_p=video_addr; //points to video RAM
		if (mode==0)
		 {
			for (c0=0;c0<a0;c0++)
			 {
				*crtvar_p=*s;
				*(crtvar_p+1)=*(s+1);
				crtvar_p+=2; s+=2;
			 }
		 }
		 else if (mode==1)
		 {
			for (c0=0;c0<a0;c0++)
			 {
				*(crtvar_p+1)=*s; s++;
				*crtvar_p=*s; s++;
				crtvar_p+=2;
			 }
		 }
		 else
		 {
			if (mode==3)
				crtvar_p++;
			for (c0=0;c0<a0;c0++)
			 {
				*crtvar_p=*s;
				crtvar_p+=2; s++;
			 }
		 }
	 }
	 else
	  {
		for (c0=0;c0<vmode_y;c0++)
			for (c1=0;c1<vmode_x;c1++)
			 {
				if (mode<2)
				 {
					if (mode==0)
					 {
						chr=*s; s++;
						color=*s; s++;
					 }
					 else
					 {
						color=*s; s++;
						chr=*s; s++;
					 }
					printc(chr,c1,c0,color);
				 }
				else
				 {
					if (mode==2)
					 {
						printcn(*s,c1,c0);
						s++;
					 }
					if (mode==3)
					 {
						changecolor(c1,c0,*s);
						s++;
					 }
				 }
			 }
	  }
	return s;
 }
