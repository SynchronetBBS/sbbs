/* DETECT - Displays current video status                           */
/* Freeware version                                                 */
/* By Marcio Afonso Arimura Fialho                                  */
/* http://pessoal.iconet.com.br/jlfialho                            */
/* e-mail: jlfialho@iconet.com.br or (alternate) jlfialho@yahoo.com */

#include <crt.h>
#include <stdio.h>

#define CGA  1
#define MCGA 2
#define EGA  3
#define VGA  4

void main ()
 {
	char *msg[2]={"Text","Graphics"};
	crt_detect(VGA); //if your video adapter isn't VGA or SVGA+, replace
						  //crt_detect input argument
	printf ("\n\tCurrent video mode = %d (%.2X)h (%s mode)\n",
	  (unsigned char)vmode_mode,(unsigned char)vmode_mode,msg[crt_direct]);
	if (!crt_direct)
	 {
		printf ("\tCurrent video page = %d\n",crt_page);
		printf ("\tCurrent video page base address = %Fp\n",video_addr);
	 }
	printf ("\tNumber of screen columns = %d\n",vmode_x);
	printf ("\tNumber of screen rows = %d\n",vmode_y);
 }