#define intm(intnum) asm int intnum

void settextblink (int cmd) //if cmd==1 => set attribute bit 7 as enable blink
//bit (default), otherwise it becomes background intensity bit
 {
	cmd%=(unsigned)256u;
	if ((unsigned)cmd>1)
		return;
	asm push ax;
	asm push bx;
	_BX=cmd;
	_AX=0x1003;
	intm (0x10);
	asm pop bx;
	asm pop ax;
 }