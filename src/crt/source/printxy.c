extern void	printc (int c, int x, int y, int color);

void printxy (char *s, int x, int y, int dx, int dy, int color)
 //prints a string at position (x,y) with character spacing (dx,dy) with color
 {
	while (*s)
	 {
		printc ((int)*s,x,y,color);
		x+=dx;
		y+=dy;
		s++;
	 }
 }
