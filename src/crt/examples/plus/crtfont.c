#include <crt.h>
#include <stdio.h>
#include <stdlib.h>

int readfntfile (unsigned char *target, char *path, int offset, int cnt)
//reads a font file from disk
//cnt = count of patterns to read.    h = character height
//offset = offset in source file.     path= source file path and name
//target = buffer to store character patterns (font).
 {
	FILE *source;
	int c0,a0;
	source = fopen (path,"rb");
	if (source==NULL)
		return 1; //error code (couldn't open file)
	if (fseek(source,offset,0))
	 {
		fclose(source);
		return 2; //error code (couldn't reposition file pointer)
	 }
	a0=cnt*changechar_height;
	for (c0=0;c0<a0;c0++) //reads character patterns from file.
	 {
		*target=fgetc(source);
		target++;
	 }
	fclose(source);
	return 0; //OK
 }

int changefnt (char *path, int offset, int ind, int cnt)
//path = source file path and name
//offset = offset in source file
//ind = index of first character to have its pattern changed
//cnt = count of patterns to read and change.
 {
	unsigned char *fntbuf;
	int a0;
	fntbuf=(unsigned char *)malloc(cnt*changechar_height);
	if (fntbuf==NULL)
		return 3; //error code (not enough memory for allocating buffer)
	a0=readfntfile (fntbuf, path, offset, cnt);
	if (a0)
		return a0;
	changechar (fntbuf, ind, cnt);
	return 0;
 }

int changefntg (char *path, int offset, int rows)
//path = source file path and name
//offset = offset in source file
//rows = number of screen rows
 {
	unsigned char *fntbuf;
	int a0;
	fntbuf=(unsigned char *)malloc(0x100*changechar_height);
	if (fntbuf==NULL)
		return 3; //error code (not enough memory for allocating buffer)
	a0=readfntfile (fntbuf, path, offset, 0x100);
	if (a0)
		return a0;
	changecharg (fntbuf, rows);
	return 0;
 }