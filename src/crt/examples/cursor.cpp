/* CURSOR - REPLACES ACTIVE PAGE CURSOR SHAPE     */
/*   ALSO AN EXAMPLE OF VIDEO HANDLING FUNCTIONS  */

#include <stdio.h>
#include <crt.h>
#include <ctype.h>

#include "hextoi.c"
  //hextoi defined in HEXTOI.CPP converts a string (in hexadecimal notation) to a integer

unsigned int main (unsigned int a, unsigned char * ent[3])
 {
	int a0;

	getcrtmode ();//updates crt_page

	if (a>2) //in case of too input parameters
	 {
		 printf (" Too much parameters - %s",ent[2]);
		 goto fim;
	 }
	if (a<2) //if required parameter is missing
	 {
		printf (" Required parameter missing");
		goto fim;
	 }

	if (*ent[1]=='/' & *(ent[1]+1)=='?') //displays help
	 {
		printf ("\tCURSOR\t\t\t\t\tver 1.0\t\n\n* * * Freeware version\
 * * * CAN BE DISTRIBUTED FREELY * * *\n\n\
 Replaces cursor shape.\n\n\t\tCURSOR <shape>\n  where :\n\
 \t shape => new cursor shape (0h-FFFFh)\n\n\
	By Marcio Afonso Arimura Fialho\n\
 http://pessoal.iconet.com.br/jlfialho\n\
 e-mail:jlfialho@iconet.com.br");
		return 0;
	 }

	if (hextoi(&(int)a0,ent[1])) //converts input to a0 and verify if input is OK
	 {
	  jmp_error:
		printf ("\nERROR: Incorrect input parameter"); //displays error message
		goto fim;
	 }

	setcursorsh (a0); //changes cursor shape
	return 0;

  fim : //displays a message and then returns
	printf ("\nType \"/?\" to get help\n");
	return 1;
 }

// By Marcio Afonso Arimura Fialho
// http://pessoal.iconet.com.br/jlfialho
// e-mail: jlfialho@iconet.com.br or (alternate) jlfialho@yahoo.com