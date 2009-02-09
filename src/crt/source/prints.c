extern void printc(int c, int x, int y, int color);

void prints(char *s, int x, int y, int color)
 //prints a string at position (x,y) with color "color".
 {
	while (*s)
	 {
		printc((int)*s,x,y,color);
		x++;
		s++;
	 }
 }
