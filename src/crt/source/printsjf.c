extern void printsj (char *s, int y, int color);
#include "printsf.h"

int printsjf(int y, int color, char *fmt,... )
//sends formatted output using printsj function.
 {
	void *argptr;
	char s[256];
	int rtn;
	va_start(argptr, fmt);
	rtn = vsprintf(s, fmt, argptr);
	printsj(s,y,color);
	va_end(argptr);
	return rtn;
 }
