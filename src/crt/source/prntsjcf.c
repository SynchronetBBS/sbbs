extern void printsjc (char *s, int y, int color);
#include "printsf.h"

int printsjcf(int y, int color, char *fmt,... )
//sends formatted output using printsjc function.
 {
	void *argptr;
	char s[256];
	int rtn;
	va_start(argptr, fmt);
	rtn = vsprintf(s, fmt, argptr);
	printsjc (s,y,color);
	va_end(argptr);
	return rtn;
 }
