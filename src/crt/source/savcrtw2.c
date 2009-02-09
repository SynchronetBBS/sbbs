extern char*savecrtwin (char*s, int xi, int yi, int xf, int yf, int mode);

#include "crtwin.h"

char*savecrtw(char*s, int mode)
 {
	return (char *) savecrtwin(s,xi,yi,xf,yf,mode);
 }
