#include <ciolib.h>
extern int crt_page;

void crt_gotoxy (int x, int y) //changes cursor position in text page defined
//by crt_page.
 {
	gotoxy(x+1,y+1);
 }
