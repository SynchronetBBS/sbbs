/* same license as Mystic Draw */
#include <gen_defs.h>
#include <ciolib.h>
#include <genwrap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "xpendian.h"

#include "homedir.h"
#include "fonts.h"

char            nul[200];

char           *
inputfield(char *Str, int length, int x1, int y)
{
	int             ch, x, pos = 0;
	gotoxy(x1 + 1 + 1, y + 1 + 1);
	textattr(8);
	for (x = 1; x <= length; x++)
		putch(250);
	gotoxy(x1 + 1, y + 1);
	textattr(7);
	sprintf(nul, "%s", Str);
	gotoxy(x1 + 1, y + 1);
	cprintf("%s", nul);
	do {
		gotoxy(x1 + pos + 1, y + 1);
		ch = getch();
		if (ch == 0 || ch == 0xe0)
			ch |= getch() << 8;
		switch (ch) {
		case 13:
			break;
		case CIO_KEY_LEFT:
			if (pos > 0)
				pos--;
			break;
		case CIO_KEY_RIGHT:
			if (pos < (int)strlen(nul))
				pos++;
			break;
		case CIO_KEY_HOME:
			pos = 0;
			break;
		case CIO_KEY_END:
			pos = strlen(nul);
			break;
		case 127:
			if (pos != (int)strlen(nul)) {
				memcpy(&nul[pos], &nul[pos + 1], 200 - pos);
				gotoxy(x1 + 1, y + 1);
				cprintf("%s", nul);
				textattr(8);
				putch(250);
				textattr(7);
				gotoxy(x1 + pos + 1, y + 1);
			}
			break;
		case 8:
			if (pos > 0) {
				memcpy(&nul[pos - 1], &nul[pos], 200 - pos);
				pos--;
				gotoxy(x1 + 1, y + 1);
				cprintf("%s", nul);
				textattr(8);
				putch(250);
				textattr(7);
				gotoxy(x1 + pos + 1, y + 1);
			}
			break;
		default:
			if ((pos < length) & (ch >= 32) & (ch <= 127)) {
				if (pos == (int)strlen(nul))
					sprintf(strchr(nul,0), "%c", (ch & 255));
				else
					nul[pos] = ch;
				gotoxy(x1 + 1, y + 1);
				cprintf("%s", nul);
				textattr(7);
				pos++;
			}
			break;
		}
	} while ((ch != 13) & (ch != 10) & (ch != 27));
	if (ch == 27)
		return Str;
	return nul;
}

void
clrscrWindow(void)
{
	int             x, y;
	for (y = 5; y <= 17; y++) {
		gotoxy(30 + 1, y + 1);
		for (x = 30; x <= 60; x++)
			putch(32);
	}
};

void
ShowCharacter(int num)
{
	int             a, b;
	ReadFonts(num);
	textattr(7);
	clrscrWindow();
	if (TDFont.Chartable[num - 32] == 0xFFFF)
		return;
	if (MaxX > 60)
		MaxX = 60;
	if (MaxY > 24)
		MaxY = 24;
	for (a = 1; a <= MaxY; a++)
		for (b = 1; b <= MaxX; b++) {
			switch (TDFont.FontType) {
			case 0:
			case 1:
				gotoxy(b + 29 + 1, a + 4 + 1);
				if (Chars[b][a] >= 32)
					putch(Chars[b][a]);
				else
					putch(32);
				break;
			case 2:
				gotoxy(b + 29 + 1, a + 4 + 1);
				textattr(Chars[b * 2 - 1][a]);
				if (Chars[b * 2][a] >= 32)
					putch(Chars[b * 2][a]);
				else
					putch(32);
				break;
			}
		}
};

void
ShowFont(int number)
{
	int             a = 0, x, ch;
	clrscr();
	Openfont(number);
	do {
		for (x = 33; x < 127; x++) {
			if (x == (a + 33))
				textattr(15 + 16);
			else
				textattr(8);

			if (TDFont.Chartable[x - 32] != 0xFFFF) {
				if (x == (a + 33))
					textattr(14 + 16);
				else
					textattr(14);
			}
			gotoxy(20 + (x - 33) % 47 + 1, (x - 33) / 47 + 1);
			putch(x);
		}
		ShowCharacter(a + 33);
		ch = getch();
		if (ch == 0 || ch == 0xe0)
			ch |= getch() << 8;
		switch (ch) {
		case CIO_KEY_UP:
			if ((a - 47) >= 0)
				a -= 47;
			break;
		case CIO_KEY_DOWN:
			if (a < 47)
				a += 47;
			break;
		case CIO_KEY_LEFT:
			a--;
			if (a < 0)
				a = 126 - 33;
			break;
		case CIO_KEY_RIGHT:
			a++;
			if ((a + 33) > 126)
				a = 0;
			break;
		}
	} while (toupper(ch) != 'Q');
	clrscr();
}

int
main(int argc, char **argv)
{
	int             x, y, ch, b, c;
	FILE           *fp, *fp2, *fp3;
	char            a[255], fnts[2000][16];
	char           *Name;
	int             size[2000];
	char            FontFile[255];
	sprintf(FontFile, "%s%s", homedir(), "allfont.fnt");

	fp = fopen(FontFile, "rb");
	if (fp == NULL) {
		CreateFontFile();
		fp = fopen(FontFile, "rb");
	}
	if(fp==NULL) {
		printf("Cannot open %s\n",FontFile);
		return(1);
	}
	fread(&Header.sign, 1, 10, fp);
	fread(&Header.NumberofFonts, 2, 1, fp);
	Header.NumberofFonts=LE_SHORT(Header.NumberofFonts);
	fclose(fp);
	for (y = 0; y < Header.NumberofFonts; y++) {
		Openfont(y);
		sprintf(fnts[y], "%s", FontRec.FontName);
		size[y] = FontRec.Length;
	}
	b = 1;
	c = 0;
	do {
		textattr(15);
		gotoxy(40 + 1, 4 + 1);
		cprintf("[E] Export font");
		gotoxy(40 + 1, 5 + 1);
		cprintf("[I] Import font");
		gotoxy(40 + 1, 6 + 1);
		cprintf("[Q] Quit");
		for (y = 0; y <= 22; y++)
			if (y + c < Header.NumberofFonts) {
				gotoxy(0 + 1, y + 1);
				if (y == b - 1)
					textattr(27);
				else
					textattr(11);
				for (x = 1; x <= fnts[y + c][0]; x++)
					putch(fnts[y + c][x]);
				if (y == b - 1)
					textattr(24);
				else
					textattr(8);
				for (x = x; x <= 16; x++)
					putch(250);
				textattr(10);
				cprintf("%02d", (size[y + c] / 1024));
				textattr(2);
				cprintf("kb");
			}
		ch = getch();
		if (ch == 0 || ch == 0xe0)
			ch |= getch() << 8;
		switch (ch) {
		case 'e':
		case 'E':
			Openfont(c + b - 1);
			fp = fopen(FontFile, "rb");
			if(fp!=NULL) {
				for (x = 1; x <= fnts[b + c - 1][0]; x++)
					a[x - 1] = fnts[b + c - 1][x];
				a[x - 1] = 0;
				strcat(a,".tdf");
				fp2 = fopen(a, "wb");
				if(fp2!=NULL) {
					fseek(fp, FontRec.FilePos, SEEK_SET);
					for (x = 0; (unsigned long)x < FontRec.Length; x++)
						fputc(fgetc(fp), fp2);
					fclose(fp2);
				}
				else
					fprintf(stderr,"cannot write to %s\r\n",a);
				fclose(fp);
			}
			else
				fprintf(stderr,"cannot open %s for read\r\n",FontFile);
			break;
		case 'i':
		case 'I':
			gotoxy(25 + 1, 9 + 1);
			cprintf("Enter Name:");
			Name = strdup(inputfield("", 30, 25, 10));
			fp = fopen(Name, "rb");
			if(fp==NULL) {
				fprintf(stderr, "Cannot open %s\r\n",Name);
				free(Name);
				break;
			}
			fread(&TDFont.Sign, 1, 19, fp);
			fclose(fp);
			TDFont.Sign[19] = 0;
			if (strcmp(TDFont.Sign, "TheDraw FONTS file") != 0) {
				fprintf(stderr, "Not a TheDraw FONTS file.\r\n");
				free(Name);
				break;
			}
			fp = fopen(FontFile, "rb");
			if(fp==NULL) {
				fprintf(stderr, "Cannot open %s\r\n",FontFile);
				free(Name);
				break;
			}
			b = 0;
			while (!feof(fp)) {
				fgetc(fp);
				b++;
			}
			fseek(fp, 0, SEEK_SET);
			fp2 = fopen("tmp.fnt", "wb");
			if(fp2==NULL) {
				fprintf(stderr, "Cannot open tmp.fnt\r\n");
				fclose(fp);
				free(Name);
				break;
			}
			fp3 = fopen(Name, "rb");
			if(fp3==NULL) {
				fprintf(stderr, "Cannot open %s\r\n",Name);
				fclose(fp);
				fclose(fp2);
				free(Name);
				break;
			}
			free(Name);
			if (errno != 0) {
				fclose(fp);
				fclose(fp2);
				fclose(fp3);
				clrscr();
				break;
			}
			fread(&Header.sign, 1, 10, fp);
			fwrite(&Header.sign, 1, 10, fp2);
			fread(&Header.NumberofFonts, 2, 1, fp);
			Header.NumberofFonts=LE_SHORT(Header.NumberofFonts);
			Header.NumberofFonts++;
			Header.NumberofFonts=LE_SHORT(Header.NumberofFonts);
			fwrite(&Header.NumberofFonts, 2, 1, fp2);
			Header.NumberofFonts=LE_SHORT(Header.NumberofFonts);
			for (y = 1; y < Header.NumberofFonts; y++) {
				for (x = 0; x <= 16; x++)
					FontRec.FontName[x] = fgetc(fp);
				for (x = 0; x <= 16; x++)
					fputc(FontRec.FontName[x], fp2);
				fread(&FontRec.FilePos, 4, 1, fp);
				FontRec.FilePos=LE_LONG(FontRec.FilePos);
				fread(&FontRec.Length, 4, 1, fp);
				FontRec.Length=LE_LONG(FontRec.Length);
				FontRec.FilePos += FontRecordSize;
				FontRec.FilePos=LE_LONG(FontRec.FilePos);
				fwrite(&FontRec.FilePos, 4, 1, fp2);
				FontRec.FilePos=LE_LONG(FontRec.FilePos);
				FontRec.Length=LE_LONG(FontRec.Length);
				fwrite(&FontRec.Length, 4, 1, fp2);
				FontRec.Length=LE_LONG(FontRec.Length);
			}
			for (x = 0; x <= 19; x++)
				TDFont.Sign[x] = fgetc(fp3);
			for (x = 0; x <= 3; x++)
				TDFont.a[x] = fgetc(fp3);
			for (x = 0; x <= 16; x++)
				TDFont.Name[x] = fgetc(fp3);
			fread(&TDFont.FontType, 1, 1, fp3);
			fread(&TDFont.Spaces, 1, 1, fp3);
			fread(&TDFont.Nul, 2, 1, fp3);
			TDFont.Nul=LE_SHORT(TDFont.Nul);
			for (x = 1; x <= 94; x++) {
				fread(&TDFont.Chartable[x], 2, 1, fp3);
				TDFont.Chartable[x]=LE_SHORT(TDFont.Chartable[x]);
			}
			for (x = 1; x <= 22; x++)
				TDFont.b[x] = fgetc(fp3);
			for (x = 0; x <= 16; x++)
				FontRec.FontName[x] = TDFont.Name[x];
			FontRec.FilePos = b + FontRecordSize;
			for (x = 0; x <= 16; x++)
				fputc(FontRec.FontName[x], fp2);
			FontRec.FilePos=LE_LONG(FontRec.FilePos);
			fwrite(&FontRec.FilePos, 4, 1, fp2);
			FontRec.FilePos=LE_LONG(FontRec.FilePos);
			fseek(fp3, 0, SEEK_END);
			FontRec.Length = ftell(fp3);
			FontRec.Length=LE_LONG(FontRec.Length);
			fwrite(&FontRec.Length, 4, 1, fp2);
			FontRec.Length=LE_LONG(FontRec.Length);
			while (!feof(fp))
				fputc(fgetc(fp), fp2);
			fseek(fp3, 0, SEEK_SET);
			while (!feof(fp3))
				fputc(fgetc(fp3), fp2);
			fclose(fp);
			fclose(fp2);
			fclose(fp3);
			remove(FontFile);
			rename("tmp.fnt", FontFile);
			clrscr();
			break;
		case 10:
		case 13:
			ShowFont(c + b - 1);
			break;
		case CIO_KEY_UP:
			b--;
			if (b < 1) {
				b = 1;
				c--;
				if (c < 0)
					c = 0;
			}
			break;
		case CIO_KEY_DOWN:
			b++;
			if (b > Header.NumberofFonts)
				b = Header.NumberofFonts;
			if (b > 23) {
				b = 23;
				c++;
				if (c + b >= Header.NumberofFonts)
					c = Header.NumberofFonts - b - 1;
			}
			break;
		}

	} while (toupper(ch) != 'Q');
	return(0);
}
