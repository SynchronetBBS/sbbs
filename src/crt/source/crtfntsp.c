void crtfontspec (int blkspec) //enables the display of two character fonts
 {
	asm push ax;
	asm push bx;
	_BL=blkspec;
	_AX=0x1103;
	asm int 0x10;
	asm pop bx;
	asm pop ax;
 }