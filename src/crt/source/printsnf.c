extern void printsn(char *s, int x, int y);
#include "printsf.h"

int printsnf(int x, int y, char *fmt,... )
//sends formatted output using printsn function.
 {
	void *argptr;
	char s[256];
	int rtn;
	va_start(argptr, fmt);
	rtn = vsprintf(s, fmt, argptr);
	printsn(s,x,y);
	va_end(argptr);
	return rtn;
 }
