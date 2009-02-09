unsigned char  __inportb__( int __portid );
unsigned int   __inportw__( int __portid );
void           __outportb__( int __portid, unsigned char __value );
void           __outportw__( int __portid, unsigned int __value );

#define inportb         __inportb__
#define outportb        __outportb__

extern int crt_EVGA_addr; //EGA/VGA base port address.

void setchrboxwidth (int cmd)
//cmd == 0 => set's 9 pixels character box width
//cmd == 1 => set's 8 pixels character box width
//cmd == 2 => if 9 pixel box width is selected, selects 8 pixels, and vice versa.
 {
	register unsigned char a0,a1;
	outportb(crt_EVGA_addr+4u,1u); //selects clocking mode register
	a0=inportb(crt_EVGA_addr+5u); //a0 <= VGA sequencer clocking mode register
	if (cmd==2)
		a0^=0x01; //XORS bit 0 of a0
	else if (cmd==1)
		a0|=0x01; //SETS bit 0 of a0
	else if (cmd==0)
		a0&=0xfe; //CLEARS bit 0 of a0
	outportb(crt_EVGA_addr+5u,a0);//VGA sequencer clocking mode register is loaded with a0

	a1=inportb(crt_EVGA_addr+0x0Cu);//a1 <= VGA miscellaneous output register
	a1&=0xf3; //clears bit 3 and 2.
	if (!(a0&0x01)) //if bit 0 of a1 is cleared (cmd==0)
		a1|=0x04;	//sets bit 2 of a1, selecting the pixelclock frequency
						//for a character box 9 pixels wide.
						//otherwise (bit 2 reseted), selected frequency is for a
						//character box 8 pixels wide.
	outportb(crt_EVGA_addr+2u,a1); //VGA miscellaneous output register <= a1
 }
