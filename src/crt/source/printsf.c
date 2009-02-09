extern void prints(char *s, int x, int y, int color);
#include "printsf.h"

int printsf(int x, int y, int color, char *fmt,... )
//sends formatted output using prints function.
 {
	void *argptr;
	char s[256];
	int rtn;
	va_start(argptr, fmt );
	rtn = vsprintf(s, fmt, argptr);
	prints(s,x,y,color);
	va_end(argptr);
	return rtn;
 }
