extern void printcn(int c, int x, int y);

void printsn(char *s, int x, int y)
  //prints a string starting from postion (x,y), keeping the old colors of
  //positions where the string is going to be printed.
 {
	while (*s)
	 {
		printcn((int)*s,x,y);
		x++;
		s++;
	 }
 }
