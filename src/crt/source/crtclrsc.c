extern void fillscr (int c, int color);
extern void crt_gotoxy (int x, int y);

void crt_clrscr (void)
 {
	fillscr (' ',7);
	crt_gotoxy(0,0);
 }
