/* Same license as Mystic Draw */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dirwrap.h"
#include "xpendian.h"

#include "homedir.h"
#include "fonts.h"

FontRecord      fr[10000];
char           *fontdata[10000];
int             FilePos;

void
insertfont(char *Name)
{
	FILE           *fp;
	int             size, x;
	fp = fopen(Name, "rb");
	if (fp == NULL) {
		printf("Cannot open %s for read\n",Name);
		return;
	}
	/* get filesize */
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	/* Read Font Header in TDFont */
	TDFont.Sign[0] = fgetc(fp);
	for (x = 0; x < 19; x++)
		TDFont.Sign[x] = fgetc(fp);
	TDFont.Sign[18] = 0;
	if (strcmp(TDFont.Sign, "TheDraw FONTS file") != 0)
		return;
	for (x = 0; x <= 3; x++)
		TDFont.a[x] = fgetc(fp);
	for (x = 0; x <= 16; x++)
		TDFont.Name[x] = fgetc(fp);
	fread(&TDFont.FontType, 1, 1, fp);
	fread(&TDFont.Spaces, 1, 1, fp);
	fread(&TDFont.Nul, 2, 1, fp);
	TDFont.Nul=LE_SHORT(TDFont.Nul);
	for (x = 1; x <= 94; x++) {
		fread(&TDFont.Chartable[x], 2, 1, fp);
		TDFont.Chartable[x]=LE_SHORT(TDFont.Chartable[x]);
	}
	/* Increase Font Index */
	Header.NumberofFonts++;

	/* Make FontRecord */
	memcpy(&fr[Header.NumberofFonts].FontName, &TDFont.Name, 16);
	fr[Header.NumberofFonts].Length = size;
	fr[Header.NumberofFonts].FilePos = FilePos;
	FilePos += size;
	/* Read FontData */
	fontdata[Header.NumberofFonts] = (char *)malloc(size);
	fseek(fp, 0, SEEK_SET);
	fread(fontdata[Header.NumberofFonts], size, 1, fp);
}

int main(int argnum, char *args[])
{
	FILE           *fp, *font;
	char            filename[254];
	int             x, y;
	char            FontFile[255];

	sprintf(FontFile, "%s%s", homedir(), "allfont.fnt");

	if (argnum < 2) {
		printf("usage :\n");
		printf("%s <FILENAME>\n",args[0]);
		printf("FILENAME is a text file which contains a list of tdf fonts\n");
		return(0);
	}

	FilePos = HeaderSize;
	fp = fopen(args[1], "rb");
	if (fp == NULL) {
		printf("can't open %s for read\n",args[1]);
		return(1);
	}
	CreateFontFile();
	/*Huh ? It 's not open! */
		/* fseek(font, 0, SEEK_SET); */
	while (1) {
		fgets(filename, 255, fp);
		if (!feof(fp)) {
			filename[strlen(filename) - 1] = 0;
			printf("- open font file : %s\n", filename);
			insertfont(filename);
		} else
			break;
	}
	printf("building %s \n", FontFile);
	/* Adding the size of the Font Records to the filetables */
	for (x = 1; x <= Header.NumberofFonts; x++) {
		fr[x].FilePos += FontRecordSize * Header.NumberofFonts;
	}
	fclose(fp);
	/* writing to FontFile */
	font = fopen(FontFile, "wb");
	if(font==NULL) {
		printf("cannot open %s for write\n",FontFile);
		return(1);
	}
	/* Header */
	fwrite(&Header.sign, 1, 10, font);
	Header.NumberofFonts=LE_SHORT(Header.NumberofFonts);
	fwrite(&Header.NumberofFonts, 2, 1, font);
	Header.NumberofFonts=LE_SHORT(Header.NumberofFonts);
	/* Font Headers */
	for (x = 1; x <= Header.NumberofFonts; x++) {
		for (y = 0; y <= 16; y++)
			fputc(fr[x].FontName[y], font);
		fr[x].FilePos=LE_LONG(fr[x].FilePos);
		fwrite(&fr[x].FilePos, 4, 1, font);
		fr[x].FilePos=LE_LONG(fr[x].FilePos);
		fr[x].Length=LE_LONG(fr[x].Length);
		fwrite(&fr[x].Length, 4, 1, font);
		fr[x].Length=LE_LONG(fr[x].Length);
	}
	/* write Font Data */
	for (x = 1; x <= Header.NumberofFonts; x++) {
		fwrite(fontdata[x], 1, fr[x].Length, font);
		free(fontdata[x]);
	}
	fclose(font);
	return(0);
}
