void getdacreg (int dacreg, char *red, char *green, char *blue)
//gets the color from individual DAC register
 {
	asm push ax;
	asm push bx;
	asm push cx;
	asm push dx;
	_BL=dacreg;
	_AX=0x1015;
	asm int 0x10;
	*red=_DH;
	*green=_CH;
	*blue=_CL;
	asm pop dx;
	asm pop cx;
	asm pop bx;
	asm pop ax;
 }

void setdacreg (int dacreg, char red, char green, char blue)
//changes the color of a individual DAC register
 {
	asm push ax;
	asm push bx;
	asm push cx;
	asm push dx;
	_BX=dacreg;
	_CH=green;
	_CL=blue;
	_DH=red;
	_AX=0x1010;
	asm int 0x10;
	asm pop dx;
	asm pop cx;
	asm pop bx;
	asm pop ax;
 }

void setdacpgmode (int pgmode) //selects video DAC paging mode
//pgmode== 0 => 4 Blocks of 64
//pgmode== 1 => 16 Blocks of 16
//this function is not valid in mode 13h (19)d
 {
	asm {
		push ax;
		push bx;
	}
	_BH=pgmode;
	asm {
		xor bl,bl; //_BL=0
		mov ax,0x1013;
		int 0x10;
		pop bx;
		pop ax;
	}
 }

void setdacpage(int page) //selects video DAC color page
//page == page number (0-3) or (0-15)
//this function is not valid in mode 13h (19)d
 {
	asm {
		push ax;
		push bx;
	}
	_BH=page;
	asm {
		mov bl,1;
		mov ax,0x1013;
		int 0x10;
		pop bx;
		pop ax;
	}
 }

int getdacpgstate (void) //returns a word containing video DAC information
//bits 15-8 => current DAC page
//bits 7-0  => paging mode (0 => 4 pages of 64)  (1 => 16 pages of 16)
 {
	asm {
		push bx;
		mov ax,0x101A;
		int 0x10;
	 }
	asm mov ax,bx;
	asm pop bx;
	return _AX;
 }