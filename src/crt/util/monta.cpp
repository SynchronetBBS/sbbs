//Copyright (c) By M rcio Afonso Arimura Fialho
//Freeware version, may be distributed freely

#include <conio.h>
#include <stdio.h>
#include <string.h>

#include "readname.cpp"
	//readfname defined in readname.cpp appends a extension to a filename if
	//none is given

void main (int n, char *ent[16])
 {
	FILE *source, *target;
	char targetpath[132];
	char *bin_open="\x0d\x0a//\1"; //This is the file open tag in binary format
	char *bin_close="//\2\x0d\x0a"; //This is the file close tag in binary format
	char *text_open="\x0d\x0a//<FILE=\""; //This is the file open tag in ASCII format
	char *text_close="\x0d\x0a//</FILE>\x0d\x0a"; //This is the file close tag in ASCII format
	int format=1; //if format==0 uses binary format.
	int c0; //counter
	int a0; //auxiliar var
	int key; //keyboard input

	if (n<2)
	 {
		printf ("\
MONTA 2.0b - Simple text file grouper for C/C++ sources - Freeware version\n\n\
Usage:\tMONTA [/B] (archive name [.SRC]) file1 [file2 [...]]\twhere:\n\
\t/B\t\tcreate archive using binary file tags\n\
\tarchive name\tname of the archive to be created\n\t\t\t(.SRC extension supplied if none is supplied)\n\
\tfile1, file2\tfiles to be stored\
\n\n\
By M rcio A. A. Fialho\nhttp:pessoal.iconet.com.br/jlfialho\n\
e-mail:jlfialho@iconet.com.br OR jlfialho@yahoo.com");
		return;
	 }
	if (n==2)
	 {
		puts ("ERROR: NO file to include in archive \nType MONTA with no parameters to obtain help.\a");
		return;
	 }
	c0=0;
	a0=1;
	if (*ent[1]=='/') //verifies if /B swich is on
	 {
		ent[1]++;
		if (*ent[1]=='B' || *ent[1]=='b')
			format=0;
		a0++;
	 }

	readfname(targetpath,ent[a0],".SRC");
		//reads filename and appends .SRC extension if none is found

	key='O'; //if key==O, the archive will be written from the begin
	target=fopen(targetpath,"rb");
	if (target!=NULL)
	//if archive (target file) already exists, prompts for action
	 {
		fclose (target);
		for(;;)
		 {
			printf ("\nFile already exist. Overwrite/Append/Quit?(O/A/Q)");
			key=getche ();
			if (key>0x60 && key<0x7b) //upcases the key returned by the user
				key=key-0x20;
			if (key=='O')
			 {
				target=fopen (targetpath,"wb");
				break;
			 }
			else if (key=='A')
			 {
				target=fopen (targetpath,"rb+");
				fseek(target,0,2);
				break;
			 }
			else if (key=='Q')
			 {
				fclose(target);
				return;
			 }
		 }
	 }
	 else
		target=fopen(targetpath,"wb");

	if (target==NULL)
	 {
		printf ("ERROR: Couldn't open target file for writing.");
		fcloseall();
		return;
	 }

	if (key=='O') //writes message
		fputs("//* * * Archive created by MONTA utility * * * \x0d\x0a",target);

	a0++;
	for(c0=a0;c0<n;c0++) //stores files to be stored
	 {
		source=fopen(ent[c0],"rb");
		if (source==NULL)
		 {
			printf ("\nWarning: file \"%s\" couldn't be opened. Skipping file \"%s\"\n",ent[c0],ent[c0]);
			goto jmp1;
		 }
		 else
			printf ("\n%s",ent[c0]);
		if (format==1)
		 {
			fputs(text_open,target);
			fputs(ent[c0],target);
			fputs("\">\x0d\x0a",target);
		 }
		 else
		 {
			fputs(bin_open,target);
			fputs(ent[c0],target);
			fputs("\x0d\x0a",target);
		 }
		while (!(feof(source))) //writes the files to be stored in the archive
		 {
			fputc ((char)fgetc(source),target);
		 }
		fseek (target,-1,2);
		if (format==1)
			fputs(text_close,target);
		 else
			fputs(bin_close,target);
		fclose (source);
	  jmp1:
	 }
  pula1:
	fclose(source);
	fclose(target);
 }

//by Marcio A. A. Fialho
//http: pessoal.iconet.com.br/jlfialho/english.htm
//E-mail : jlfialho@iconet.com.br (main e-mail) OR
//	(alternate e-mail) jlfialho@yahoo.com