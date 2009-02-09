/* UNHIGH - Utility to unhighlight higlighted lines in VIDEOHLP.TXT      */
/* By M rcio Afonso Arimura Fialho                                       */
/* Freeware/Public Domain                                                */

#include <STDIO.H>
#include <CONIO.H>

void main ()
 {
	FILE *source,*target;
	char *sourcename="VIDEOHLP.TXT"; //source filename
	char *targetname="VIDEOHLP.TMP"; //target filename
	int a0;
	int c;
	int linepos;

	source=fopen(sourcename,"rb"); //open source file
	if (source==NULL)
	 {
		printf ("\nERROR:Couldn't open %s to read",sourcename);
		fcloseall();
		return;
	 }

	target=fopen(targetname,"rb"); //verifies if target file already exists
	if (target!=NULL)
	 {
		printf ("\nWARNING: Target file already exists. Overwrite ? (Y/N)");
		fclose (target);
		a0=getch();
		if (a0!='y' && a0!='Y')
		 {
			printf ("\nReturning to OS");
			fcloseall();
			return;
		 }
		printf ("\nOverwriting target file");
	 }

	target=fopen(targetname,"wb"); //open target file for writing
	if (target==NULL)
	 {
		printf ("\nERROR:Couldn't open %s to write",targetname);
		fcloseall ();
		return;
	 }

	linepos=0; //linepos indicates character column in row.

 //replaces every occurence of % character at the beginning of each row
 //in source file by a blank space in target file.
	for(;;)
	 {
		c=fgetc(source);
		if (c==EOF)
			break;
		if (c==0x0a || c==0x0d)
			linepos=0;
		 else
			linepos++;
		if (c=='%' && linepos==1) //if '%' is the first character of the row
			c=0x20; //c=' '
		fputc (c,target);
	 }
	fclose(target);
	fclose(source);
 }

// http://pessoal.iconet.com.br/jlfialho
// E-mail: jlfialho@iconet.com.br
