extern int vmode_mode;
extern char*video_addr;

extern int getcrtmode (void);

#define MK_FP( seg,ofs )( (void _seg * )( seg ) +( void near * )( ofs ))

#define word unsigned int

void setcrtpage (int page) //selects active display page
//the actual selected page number is stored in crt_page after a call to this
//function
 {
	register word a0,a1;
  //changes active display page
	asm push ax;
	_AL=page;
	_AH=0x05;
	asm int 0x10;
	asm pop ax;
 //updates vmode_mode and crt_page
	getcrtmode ();
 //updates video_addr;
	if (vmode_mode!=7)
		a0=0xb800;
	 else
		a0=0xb000;
	a1=*(word far*)0x40004EUL; //a1=current page start address in regen buffer
	video_addr=(char far*)MK_FP(a0+a1/16,a1%16);
 }