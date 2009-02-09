/* FNTTOCPP - Converts a DOS font file (.FNT) to a C/C++ source file */
/* Freeware version                                                  */
/* By Marcio Afonso Arimura Fialho                                   */
/* http://pessoal.iconet.com.br/jlfialho                             */
/* e-mail: jlfialho@iconet.com.br or (alternate) jlfialho@yahoo.com  */

//fnttocpp converts a .FNT file to a .CPP file holding a array with
//.FNT file information

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "readname.cpp"

void numtohex (unsigned num, char *s)
	//converts a byte to string (in borland hex notation, plus comma and space)
 {
	*s='0';s++;
	*s='x';s++;
	*s=(num/16u)%16u;
	if (*s<10u)
		*s+='0';
	 else
		*s+='7';
	s++;
	*s=num%16u;
	if (*s<10u)
		*s+='0';
	 else
		*s+='7';
	s++;
	*s=','; s++;
	*s=0;
 }

void main (int n, char *ent[5])
 {
	FILE *source,*target; //source file is the archive containing target files
	char sourcepath[132];
	char targetpath[132];
	char temp[132];
	char *path;
	int  c0,c1; //counters
	int  cheight; //char height
	int  a0; //input
	if (n<5) //about the program
	 {
		printf ("FNTTOCPP - ver 1.0\n");
		printf ("usage: FNTTOCPP <source file[.FNT]> <character size> <name> <target file>\n");
		printf ("By M rcio A. A. Fialho\nhttp://pessoal.iconet.com.br/jlfialho\n");
		printf ("E-mail: jlfialho@iconet.com.br OR jlfialho@yahoo.com");
		return;
	 }
	readfname(sourcepath,ent[1],".FNT");
	readfname(targetpath,ent[4],".CPP");

	cheight=atoi(ent[2]);
	if (cheight<=0 || cheight>64)
	 {
		printf ("ERROR: Wrong character size\n");
		goto jmp1;
	 }

	source=fopen (sourcepath,"rb");
	if(source==NULL) //source file could not be opened
	 {
		printf ("ERROR: FILE \"%s\" DOESN'T EXIST OR COULDN'T BE READ.\n",sourcepath);
		goto jmp1;
	 }

	target=fopen (targetpath,"rb");
	if(target!=NULL) //source file could not be opened
	 {
		printf ("ERROR: FILE \"%s\" ALREADY EXISTS.\n",targetpath);
		goto jmp1;
	 }
	fclose (target);
	target=fopen (targetpath,"wb");
	if(target==NULL) //source file could not be opened
	 {
		printf ("ERROR: FILE \"%s\" COULDN'T BE OPENED FOR WRITING.\n",targetpath);
	  jmp1:
		puts ("Type FNTTOCPP with no parameters to obtain help.\a");
		fcloseall();
		return;
	 }

	sprintf(temp,"\tchar %s[]={",ent[3]);
	fputs (temp,target);

	for (c0=0;c0<256;c0++)
	 {
		if (!(c0%16u)) //writes comments
		 {
			sprintf(temp,"\x0d\x0a\t//Shape of characters: %.3d (%.2X)h thru \
%.3d (%.2X)h\x0d\x0a",c0,(unsigned char)c0,c0+15,(unsigned char)c0+15);
			fputs(temp,target);
		 }

		//writes a row
		for (c1=0;c1<cheight;c1++)
		 {
			if (!(c1%4u) && c1) //spacing after 4 and 16 characters
			 {
				fputc(' ',target);
				if (!(c1%16u))
					fputs("  ",target);
			 }
		  //converts a source file byte into C hexdecimal format and writes it.
			a0=fgetc(source);
			numtohex(a0, temp);
			fputs (temp,target);
		 }

		//insert comments at the end of each row
		sprintf(temp,"//%3d(%.2X)\x0d\x0a",c0,(unsigned char)c0);
		fputs (temp,target);
	 }
	fputs ("};",target);
	fcloseall ();
 }