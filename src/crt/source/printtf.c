extern int printtext (char *s, int x, int y, int color);
#include "printsf.h"

int printtextf(int x, int y, int color, char *fmt,... )
//sends formatted output using printtext function.
 {
	void *argptr;
	char s[4096];
	int rtn;
	va_start(argptr, fmt );
	rtn = vsprintf(s, fmt, argptr);
	printtext(s,x,y,color);
	va_end(argptr);
	return rtn;
 }
