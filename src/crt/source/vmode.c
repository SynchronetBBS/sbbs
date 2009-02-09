extern int vmode_x;	//number of columns in text mode (default=80)
extern int vmode_y;  //number of rows in text mode (default=25)
int vmode_lm=3;		//last mode used or selected by videomode (default=25x80)
int vmode_am=3;	//current mode used by videomode (default=25x80)

void videomode (int newmode) //tells the functions declared in CRT.H which is
//the "selected" mode number of rows and columns.
 {
	if (newmode==-1)
		newmode=vmode_lm;
	vmode_lm=vmode_am;
	vmode_am=newmode;
	switch (vmode_am)
	 {
		case 0:	//executes the immediately following line
		case 1: vmode_y=25; vmode_x=40; break; //25 x 40
		case 2:
		case 3:
		case 7: vmode_y=25; vmode_x=80; break; //25 x 80
		case 24:vmode_y=43; vmode_x=40; break; //43 x 40 (ega)
		case 32:vmode_y=43; vmode_x=80; break; //43 x 80 (ega)
		case 48:vmode_y=50; vmode_x=40; break; //50 x 40 (vga)
		case 64:vmode_y=50; vmode_x=80; break; //50 x 80 (vga)
	 }
 }
