extern void printxy (char *s, int x, int y, int dx, int dy, int color);
#include "printsf.h"

int printxyf (int x, int y, int dx, int dy, int color, char *fmt,... )
//sends formatted output using printxy function.
 {
	void *argptr;
	char s[256];
	int rtn;
	va_start(argptr, fmt );
	rtn = vsprintf(s, fmt, argptr);
	printxy(s,x,y,dx,dy,color);
	va_end(argptr);
	return rtn;
 }
