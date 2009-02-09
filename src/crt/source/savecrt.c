extern int vmode_x;
extern int vmode_y;
extern int crt_direct;
extern char*video_addr;

extern int getcrtchar (int x, int y);
extern int getcrtcolor (int x,int y);

#include <stddef.h>

char*savecrt (char*s, int mode)
//saves screen contents into "s" buffer with mode
 {
	int c0,c1,a0;
	char*crtvar_p; //used to access the video RAM

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
				*s=*crtvar_p;
				*(s+1)=*(crtvar_p+1);
				crtvar_p+=2; s+=2;
			 }
		 }
		 else if (mode==1)
		 {
			for (c0=0;c0<a0;c0++)
			 {
				*s=*(crtvar_p+1); s++;
				*s=*crtvar_p; s++;
				crtvar_p+=2;
			 }
		 }
		 else
		 {
			if (mode==3)
				crtvar_p++;
			for (c0=0;c0<a0;c0++)
			 {
				*s=*crtvar_p;
				crtvar_p+=2; s++;
			 }
		 }
	 }
	 else
	  {
		for (c0=0;c0<vmode_y;c0++)
			for (c1=0;c1<vmode_x;c1++)
			 {
				if (mode==1)
				 {
					*s=getcrtcolor(c1,c0); s++;
					*s=getcrtchar (c1,c0); s++;
				 }
				else
				 {
					if (mode!=3)
					 {
						*s=getcrtchar(c1,c0);
						s++;
					 }
					if (mode!=2)
					 {
						*s=getcrtcolor(c1,c0);
						s++;
					 }
				 }
			 }
	  }
	return s;
 }
