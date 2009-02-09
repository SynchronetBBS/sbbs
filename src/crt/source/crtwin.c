#include <stddef.h>

#include "crtwin.h"

struct crtwin_inp
 {
	char *title; //Title (if title==NULL doesn't print title)
	int tcolor; //Title color
	int fchr;   //character used to fill window internal area
	int fcolor; //internal window area color
	int bcolor; //border color
	int btype;  //border type, same as crtframe
 };

extern unsigned int strlen(const char *__s);
extern void 	crtframew(int color, unsigned int type);
extern void 	fillbarw (int c, int color);
extern void		printc	(int c, int x, int y, int color);
extern void		prints	(char *s, int x, int y, int color);
extern void		mkline_aux (int cnt, int var, unsigned int mode, int pos, int color);

void crtwindow (struct crtwin_inp p0)
 {
	unsigned int a0,a2,a3;

	crtframew(p0.bcolor,p0.btype);
	fillbarw(p0.fchr,p0.fcolor);
	if (p0.title!=NULL)
	 {
		a0=strlen(p0.title);
		a2=(xi+xf-a0)/2;
		a3=(xi+xf+a0)/2+1;
		printc(' ',a2,yi,p0.tcolor);
		printc(' ',a3,yi,p0.tcolor);
		if ((unsigned)p0.btype <2)
		 {
			mkline_aux(a2,yi,1,3,p0.bcolor);
			mkline_aux(a3,yi,1,3,p0.bcolor);
			mkline_aux(yi,a2,2*p0.btype,2,p0.bcolor);
			mkline_aux(yi,a3,2*p0.btype,1,p0.bcolor);
		 }
		prints(p0.title,a2+1,yi,p0.tcolor);
	 }
 }
