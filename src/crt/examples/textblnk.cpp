/* TEXTBLNK - TOGGLES BACKGROUND INTENSITY / BLINK ENABLE BIT     */
/*   ALSO AN EXAMPLE OF VIDEO HANDLING FUNCTIONS                  */

/* Warning: To work this program requires at least an EGA video adapter */
/* If you video adapter is worse, use the alternate program             */

#include <crt.h>

void main ()
 {
	unsigned char a0;

	//a0 = current attribute/color bit 7 status
	a0=((*(char far*)0x400065)&0x20) >>5;
	//a0 == 0 if attribute/color bit 7 is background intensity bit
	//a0 == 1 if attribute/color bit 7 is foreground blink enable bit
	settextblink (a0^0x01);
 }

//Alternate program (if you are using a CGA monitor)
/*
void main ()
 {
	unsigned char a0;

	a0=((*(char far*)0x400065)^0x20);

	(*(char far*)0x400065)=a0;
 }
	*/

// By Marcio Afonso Arimura Fialho
// http://pessoal.iconet.com.br/jlfialho
// e-mail: jlfialho@iconet.com.br or (alternate) jlfialho@yahoo.com