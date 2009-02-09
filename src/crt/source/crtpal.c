#define intm(intnum) asm int intnum

int getpalreg (int regpal) //returns the palette register (regpal) value
//(DAC register number)
//Compatibility: EGA(with UltraVision v2+), VGA, UltraVision v2+, and above

 {
	int a0;
	asm push ax;
	asm push bx;
	_BL=regpal;
	_BH=0xFF;
	_AX=0x1007;
	intm (0x10);
	a0=_BH;
	asm pop bx;
	asm pop ax;
	return a0; //returns selected DAC register index
 }

void setpalreg (int regpal, int val) //selects DAC register for specified
//color (palette register)
//Compatibility: PCjr,Tandy,EGA,MCGA,VGA and above
 {
	asm push ax;
	asm push bx;
	_BH=val;
	_BL=regpal;
	_AX=0x1000;
	intm (0x10);
	asm pop bx;
	asm pop ax;
 }
