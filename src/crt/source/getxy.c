#include <ciolib.h>
extern int crt_page;

int crt_getxy (int *x, int *y) //gets current cursor position in text page
//defined by crt_page.
 {
	int	rx,ry;

	rx=wherex()-1;
	ry=wherey()-1;
	if(x)
		*x=rx;
	if(y)
		*y=ry;
	return(ry*256+rx);
 }
