//Copyright (c) By M rcio Afonso Arimura Fialho
//Freeware version, may be distributed freely

#include <stdio.h>
#include <string.h>
#include <conio.h>

typedef unsigned int word;

void putmstr (int n, char *str, FILE *stream)
//saves n characters from a string into a stream
 {
	int c0;
	for(c0=0;c0<n;c0++)
	 {
		fputc(*str,stream);
		str++;
	 }
 }

#include "readname.cpp"
	//readfname defined in readname.cpp appends a extension to a filename if
	//none is given

void main (int n, char *ent[2])
 {
	FILE *source,*target; //source file is the archive containing target files
	char sourcepath[132];
	char targetpath[132];
	char *path;
	char *opentag="\x0d\x0a//<FILE=\""; //file opening tag
	char *closetag="\x0d\x0a//</FILE>"; //file closing tag
	int  tagpos=5; //last position where both tags are equal
	int  key;
	int  c0; //counters
	word nlevel; //nesting level
	word sf; //subcontrol
	char c;
	if (n<2) //about the program
	 {
		printf ("EXTRAIA 2.0b - Text file extractor for C/C++ sources - Freeware version\n\n");
		printf ("Usage:\tEXTRAIA filename[.SRC] [/B] \twhere:\n\tfilename\tsource archive pathname (.SRC extension optional)\n");
		printf ("\t/B\t\textracts files from archives with binary file tags\n\n");
		printf ("For further help read EXTRAIA.TXT\n\n");
		printf ("By M rcio A. A. Fialho\nhttp://pessoal.iconet.com.br/jlfialho\n");
		printf ("E-mail: jlfialho@iconet.com.br OR jlfialho@yahoo.com");
		return;
	 }
	c0=0;

	if (*ent[2]=='/') //verifies if it is the /B switch
	 {
		ent[2]++;
		if (*ent[2]=='B' || *ent[2]=='b') //instructs the program to read file tags in binary mode
		 {
			opentag="\x0d\x0a//\x01"; //replaces file tags by binary ones
			closetag="\x0d\x0a//\x02";
			printf ("Reading archive in binary format\n");
			//if tagpos==4 then archive file is istructed to read binary tags
			tagpos=4;
		 }
		else
		 {
			ent[2]--;
			printf ("Unknow option %s. ",ent[2]);
			goto jmp1; //display error message
		 }
	 }

	readfname(sourcepath,ent[1],".SRC");
		//reads filename and appends .SRC extension if none is found

	source=fopen (sourcepath,"rb");
	if(source==NULL) //source file could not be opened
	 {
		printf ("ERROR: FILE \"%s\" DOESN'T EXIST OR COULDN'T BE READ.\n",sourcepath);
	  jmp1:
		puts ("Type EXTRAIA with no parameters to obtain help.\a");
		fcloseall();
		return;
	 }
//resets program control vars
	nlevel=0;
	c0=0;
	sf=0;
	while (!feof(source)) //program main loop, reads source file
	 {
	  volta:
		c=(char)fgetc (source);
		if (nlevel==0) //reading the archive control data and comments
		 {
			//sf==0; searches for the file open tag
			//sf==1; reads file name and open it if possible
			if (sf==0)
			 {
			  jmp0_1:
				if(opentag[c0]==c) //searches for the file open tag //<FILE="????????.SRC"> or //
				 {
					c0++;
					if (opentag[c0]==0) //if found then reads file name and open it if possible
					 {
						sf=1;
						path=targetpath;
					 }
				 }
				 else if (c0) //if not, try again
				 {
					c0=0;
					goto jmp0_1;
				 }
			 }
			else if (sf==1)
			 {
				c0=0;
				do //reads stored file name
				 {
					*path=c;
					path++;
					c0++;
					if (c0>128)
						printf ("WARNING: filename %s too big. It has been truncated\n",targetpath);
					c=(char)fgetc (source);
				 }
				 while (c!='"' && c!=0x0d && c!=0x20 && c!=0x09 && c0<=128);
					// " or (NL) or (SPACE BAR) or <TAB> is required only if filename doesn't exceed 128 characters
				*path=0; //puts a null terminator in filename end

			 //opens target file and verifies if it can be written
				puts (targetpath); //writes the name of the file that is being extracted
				target=fopen (targetpath,"rb");
				key='Y';
				if (target!=NULL)
				 {
					printf ("File %s already exist. Overwrite (y/n)?",targetpath);
					do
					 {
						key=getche();
						if (key>0x60 && key<0x7b) //upcase keyboard input
							key-=0x20;
						if (key!='N' && key!='Y')
							printf ("\b\a");
					 }
					 while (key!='Y' && key !='N');
					puts("");
				 }
				if (key=='N')
					goto jmp0_2;
				if (key=='Y')
				 {
					fclose(target);
					target=fopen (targetpath,"wb");
					if (target==NULL)
					 {
						printf ("File %s couldn't be written. ");
					  jmp0_2:
						printf ("Skipping file %s\n",targetpath);
						fclose (target);
						nlevel=0; c0=0; sf=0;
					 }
					 else
					 {
						if (tagpos==5) //waits for '>' to end comments only if not instructed to read binary tags (reading ascii tags only)
							while (c!='>')//searches for the end of file opentag
								c=(char)fgetc(source);
						while (c!=0x0A)//searches for a newline
							c=(char)fgetc(source);
						nlevel=1; c0=0; sf=0;//starts extracting stored file
					 }
				 }
			 }
		 }

		else if (nlevel) //writes into target file
		 {
			//if sf==0 searching for file open or close tag
			//if sf==1 verifies if tag is opentag or closetag
			//if sf==2 searching for file open tag
			//if sf==3 searching for file close tag

		  jmp1_0:
			if (sf==0)
			 {
				if (c0 && c!=opentag[c0]) //if not found opentag completely
				 {
					if (nlevel==1)
						putmstr(c0, opentag, target);
					c0=0; //resets c0
				 }

				if (opentag[c0]==c) //searches for file open tag or close tag
				 {
					c0++;
					if (c0==tagpos) //tagpos==5 in ascii mode or 4 in binary mode
						sf=1;
				 }
			 }
			else if (sf==1)
			 {
				if (tagpos==4) //if EXTRAIA is reading archive in binary mode (with binary tags)
				 {
					if (c==0x01) //file open tag
					 {
						nlevel++; sf=0; c0=0;
						if (nlevel==2)
						 {
							putmstr(5,opentag, target);
							goto volta;
						 }
					 }
					else if (c==0x02) //file close tag
					 {
						nlevel--; sf=0; c0=0;
						if (!nlevel)
						 {
							fclose (target);
							goto volta;
						 }
					 }
					else
					 {
						if (nlevel==1)
							putmstr(c0, opentag,target);
						c0=0; sf=0;
					 }
				 }
				 else //if EXTRAIA is reading archive in ascii mode (with normal ASCII tags)
				 {
					if (c=='F')
					 {
						sf=2;
						c0++;
						if (nlevel==1)
							putmstr (c0, opentag, target);
					 }
					else if (c=='/')
					 {
						sf=3;
						c0++;
					 }
					else
					 {
						if (nlevel==1)
							putmstr(c0, opentag, target);
						c0=0;	sf=0;
					 }
				 }
			 }
			else if (sf==2)
			 {
				if (opentag[c0]==c)
				 {
					c0++;
					if (opentag[c0]==0)
					 {
						nlevel++; c0=0; sf=0;
					 }
					if (nlevel==1)
						fputc(c,target);
				 }
				 else
				 {
					c0=0; sf=0;
					goto jmp1_0;
				 }
			 }
			else if (sf==3)
			 {
				if (closetag[c0]==c)
				 {
					c0++;
					if (closetag[c0]==0)
					 {
						nlevel--; c0=0; sf=0;
						if (!nlevel)
						 {
							fclose (target);
							goto volta;
						 }
					 }
				 }
				 else
				 {
					if (nlevel==1)
						putmstr(c0, closetag, target);
					sf=0; c0=0;
					goto jmp1_0;
				 }
			 }
			if (!c0 || nlevel>1)
				fputc(c,target);
		 }
	 }
	fcloseall ();
 }

//by Marcio A. A. Fialho
//http: pessoal.iconet.com.br/jlfialho
//E-mail : jlfialho@iconet.com.br (main e-mail) OR
//	(alternate e-mail) jlfialho@yahoo.com