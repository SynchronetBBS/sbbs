/* FONTRESIZE - Resizes the character height of .FNT files          */
/* Freeware version                                                 */
/* By Marcio Afonso Arimura Fialho                                  */
/* http://pessoal.iconet.com.br/jlfialho                            */
/* e-mail: jlfialho@iconet.com.br or (alternate) jlfialho@yahoo.com */

#include <stdio.h>
#include <conio.h>
#include <string.h>

#include "readname.cpp"

void main (int n, char *ent[5])
 {
	FILE *source,*target;
	int c0,c1;
	int a0,a1;
	int k;
	char path[132];

	char c;
	char info[]="\nType FONTRESZ with no parameters for help\n";
	if (n==1)
	 {
		printf ("\tFONTRESZ ver 1.0 - - - DOS FONT RESIZER - - - FREEWARE VERSION\n\n\
\tUsage:\tFONTRESZ source[.fnt] s_heigh target[.fnt] s_target\n\n");
		printf ("Where: s_heigh is the heigh of caracters patterns in source file and\n\
\40\40\40\40\40\40 s_target is the heigh of characters patterns in target file\n\n");
		printf ("By Marcio Afonso Arimura Fialho\nhtpp://pessoal.iconet.com.br/jlfialho\n");
		return;
	 }
	if (n<4)
	 {
		printf ("ERROR: FEW PARAMETERS%s",info);
		return;
	 }

	readfname (path, ent[1], ".FNT");
	source=fopen (path,"rb");
	if(source==NULL)
	 {
		printf ("ERROR: FILE %s COULDN'T BE OPEN TO READ%s",ent[1],info);
		fcloseall();
		return;
	 }
	strcpy(path, ent[3]);
	if (strchr(path,'.')==NULL)
		strcat(path,".FNT");
	target=fopen (path,"rb");
	if (target!=NULL)
		do
		 {
			printf ("\nFILE %s already exists. Overwrite (Y/N)?",path);
			k=getche();
			if (k>0x60)
				k-=0x20;
			if (k=='N')
			 {
				fcloseall();
				return;
			 }
		 }
		 while (k!='N' && k!='Y');
	target=fopen (path,"wb");
	if(target==NULL)
	 {
		printf ("ERROR: FILE %s COULDN'T BE OPEN TO WRITE%s",ent[3],info);
		fcloseall();
		return;
	 }
	sscanf(ent[2],"%d",&a0);
	sscanf(ent[4],"%d",&a1);

	if (a0<0 || a0>256 || a1<0 || a1>256)
	 {
		printf ("ERROR: INPUT VALUES OUT OF RANGE%s",info);
		fcloseall();
		return;
	 }
	for (c0=0;c0<256;c0++)
	 {
		for (c1=0;c1<a0;c1++)
			path[c1]=fgetc(source);
		for (c1=0;c1<a1;c1++)
			fputc(path[c1*a0/a1],target);
	 }
	fseek(target,-1,2);
	fcloseall ();
 }
