static int bcolor=0;

int getbordercolor (void) //returns the DAC register index for overscan color
//(screen border color)
 {
	return(bcolor);
 }

void setbordercolor (int val) //set's screen border color (overscan color)
 {
	bcolor=val;
 }
