#include <stdio.h>

#if !defined _MSC_VER && !defined __BORLANDC__
char* itoa(int val, char* str, int radix)
{
	switch(radix) {
		case 8:
			sprintf(str,"%o",val);
			break;
		case 10:
			sprintf(str,"%u",val);
			break;
		case 16:
			sprintf(str,"%x",val);
			break;
		default:
			sprintf(str,"bad radix: %d",radix);
			break;
	}
	return(str);
}

char* ltoa(long val, char* str, int radix)
{
	switch(radix) {
		case 8:
			sprintf(str,"%lo",val);
			break;
		case 10:
			sprintf(str,"%lu",val);
			break;
		case 16:
			sprintf(str,"%lx",val);
			break;
		default:
			sprintf(str,"bad radix: %d",radix);
			break;
	}
	return(str);
}

/* Set characters of a string to a character. 8/
char           *
strset (char *str, int c)
{
	char *p;
	for (p=str; *p; p++) {
		*p=c;
	}
	return (str);
}
 
#endif
