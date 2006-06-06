#include <stdio.h>
#include <stdlib.h>

#include "xpendian.h"

#include "homedir.h"
#include "fonts.h"

#define TheDrawFontSize    254

MysticDrawFontHeader Header;
FontRecord      FontRec;
TheDrawFont     TDFont;
unsigned short  SFont;
unsigned char   Outline, MaxX, MaxY;
unsigned char   Chars[60][24];
char           *OutlineCharSet[20] = {
	"ÄÄ³³Ú¿Ú¿ÀÙÀÙ´Ã   ",
	"ÍÄ³³Õ¸Ú¿Ô¾ÀÙµÃ   ",
	"ÄÍ³³Ú¿Õ¸ÀÙÔ¾´Æ   ",
	"ÍÍ³³Õ¸Õ¸Ô¾Ô¾µÆ   ",
	"ÄÄº³Ö¿Ú·À½ÓÙ¶Ã   ",
	"ÍÄº³É¸Ú·Ô¼ÓÙ¹Ã   ",
	"ÄÍº³Ö¿Õ»À½È¾¶Æ   ",
	"ÍÍº³É¸Õ»Ô¼È¾¹Æ   ",
	"ÄÄ³ºÚ·Ö¿ÓÙÀ½´Ç   ",
	"ÍÄ³ºÕ»Ö¿È¾À½µÇ   ",
	"ÄÍ³ºÚ·É¸ÓÙÔ¼´Ì   ",
	"ÍÍ³ºÕ»É¸È¾Ô¼µÌ   ",
	"ÄÄººÖ·Ö·Ó½Ó½¶Ç   ",
	"ÍÄººÉ»Ö·È¼Ó½¹Ç   ",
	"ÄÍººÖ·É»Ó½È¼¶Ì   ",
	"ÍÍººÉ»É»È¼È¼¹Ì   ",
	"ÜÜÛÛÜÜÜÜÛÛÛÛÛÛ   ",
	"ßßÛÛÛÛÛÛßßßßÛÛ   ",
"ßÜŞİŞİÜÜßßŞİÛÛ   "};
void 
CreateFontFile(void)
{
	FILE           *fp;
	char            FontFile[255];
	MysticDrawFontHeader Head;
	sprintf(FontFile, "%s%s", homedir(), "allfont.fnt");
	fp = fopen(FontFile, "wb");
	if (fp != NULL) {
		Head.NumberofFonts = 0;
		Head.sign[0] = 9;
		Head.sign[1] = 'F';
		Head.sign[2] = 'o';
		Head.sign[3] = 'n';
		Head.sign[4] = 't';
		Head.sign[5] = ' ';
		Head.sign[6] = 'F';
		Head.sign[7] = 'i';
		Head.sign[8] = 'l';
		Head.sign[9] = 'e';
		fwrite(&Head.sign, 1, 10, fp);
		Head.NumberofFonts=LE_SHORT(Head.NumberofFonts);
		fwrite(&Head.NumberofFonts, 2, 1, fp);
		Head.NumberofFonts=LE_SHORT(Head.NumberofFonts);
		fclose(fp);
	}
}

void 
Openfont(int num)
{
	FILE           *fp;
	int             x;
	char            FontFile[255];
	sprintf(FontFile, "%s%s", homedir(), "allfont.fnt");
	fp = fopen(FontFile, "rb");
	if (fp != NULL) {
		fseek(fp, HeaderSize + (FontRecordSize * num), SEEK_SET);
		for (x = 0; x <= 16; x++)
			FontRec.FontName[x] = fgetc(fp);
		fread(&FontRec.FilePos, 4, 1, fp);
		FontRec.FilePos=LE_LONG(FontRec.FilePos);
		fread(&FontRec.Length, 4, 1, fp);
		FontRec.Length=LE_LONG(FontRec.Length);
		fseek(fp, FontRec.FilePos, SEEK_SET);
		for (x = 0; x <= 19; x++)
			TDFont.Sign[x] = fgetc(fp);
		for (x = 0; x <= 3; x++)
			TDFont.a[x] = fgetc(fp);
		for (x = 0; x <= 16; x++)
			TDFont.Name[x] = fgetc(fp);
		fread(&TDFont.FontType, 1, 1, fp);
		fread(&TDFont.Spaces, 1, 1, fp);
		fread(&TDFont.Nul, 2, 1, fp);
		TDFont.Nul=LE_SHORT(TDFont.Nul);
		for (x = 1; x <= 95; x++) {
			fread(&TDFont.Chartable[x], 2, 1, fp);
			TDFont.Chartable[x]=LE_SHORT(TDFont.Chartable[x]);
		}
		for (x = 1; x <= 20; x++)
			TDFont.b[x] = fgetc(fp);
		fclose(fp);
	}
}

unsigned char 
Transform(unsigned char a)
{
	if ((a - 64 > 0) & (a - 64 <= 17))
		return (OutlineCharSet[Outline][a - 65]);
	return 0;
}
void 
ReadFonts(unsigned char ch)
{
	int             x, y;
	unsigned char   a, b, c;
	FILE           *fp;
	char            FontFile[255];

	sprintf(FontFile, "%s%s", homedir(), "allfont.fnt");
	for (a = 1; a <= 60; a++)
		for (b = 1; b <= 24; b++)
			Chars[a][b] = 0;
	if (TDFont.Chartable[ch - 32] == 0xFFFF) {
		MaxX = 0;
		MaxY = 0;
		return;
	}
	fp = fopen(FontFile, "rb");
	if (fp != NULL) {

		fseek(fp, FontRec.FilePos + 233 + (TDFont.Chartable[ch - 32]), SEEK_SET);
		fread(&a, 1, 1, fp);
		fread(&b, 1, 1, fp);
		MaxX = a;
		MaxY = b;
		x = 1;
		y = 1;
		switch (TDFont.FontType) {
		case 0:	/* Outline-Font */
			fread(&c, 1, 1, fp);
			do {
				if (c == 13) {
					x = 0;
					y++;
				} else
					Chars[x][y] = Transform(c);
				x++;
				fread(&c, 1, 1, fp);
			} while (c != 0);
			break;
		case 1:	/* Block-Font */
			fread(&c, 1, 1, fp);
			do {
				if (c == 13) {
					x = 0;
					y++;
				} else
					Chars[x][y] = c;
				x++;
				fread(&c, 1, 1, fp);
			} while (c != 0);
			break;
		case 2:	/* Color-Font */
			fread(&c, 1, 1, fp);
			do {
				if (c == 13) {
					x = 0;
					y++;
				} else {
					fread(&a, 1, 1, fp);
					Chars[x * 2][y] = c;
					Chars[x * 2 - 1][y] = a;
				}
				x++;
				fread(&c, 1, 1, fp);
			} while (c != 0);
			break;
		}
		fclose(fp);
	}
}
