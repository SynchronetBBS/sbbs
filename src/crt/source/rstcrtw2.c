extern char*restorecrtwin (char*s, int xi, int yi, int xf, int yf, int mode);

#include "crtwin.h"

char*restorecrtw(char*s, int mode)
 {
	return (char*) restorecrtwin(s,xi,yi,xf,yf,mode);
 }
