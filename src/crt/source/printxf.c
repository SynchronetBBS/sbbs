extern void printx (char *s, int x, int y, int color);
#include "printsf.h"

int printxf(int x, int y, int color, char *fmt,... )
//sends formatted output using printx function.
 {
	void *argptr;
	char s[4096];
	int rtn;
	va_start(argptr, fmt );
	rtn = vsprintf(s, fmt, argptr);
	printx(s,x,y,color);
	va_end(argptr);
	return rtn;
 }
