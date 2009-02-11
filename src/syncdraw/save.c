#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ciolib.h>
#include <genwrap.h>
#include <gen_defs.h>

#include "syncdraw.h"
#include "miscfunctions.h"
#include "options.h"
#include "sauce.h"

unsigned char   oldColor = 7;

char           *
AvatarColor(int col)
{
	static char           tmp[4];
	if (col == oldColor)
		return NULL;
	sprintf(tmp, "%c", col);
	oldColor = col;
	return tmp;
}

char           *
PCBoardColor(int col)
{
	static char           a[4];
	if (col == oldColor)
		return NULL;
	sprintf(a, "@X%02x", col);
	strupr(a);
	oldColor = col;
	return a;
}

char           *
SyncColor(int col)
{
	static char           a[10];
	if (col == oldColor)
		return NULL;
	a[0]=0;
	if ((oldColor >= 128) && (col < 128)) {
		strcat(a, "N");
		oldColor = 7;
		if (col == 7)
			return a;
	}
	if (((col & 8) != 8) & ((oldColor & 8) == 8)) {
		strcat(a, "N");
		oldColor = oldColor & 15;
	}
	if (((col & 8) == 8) & ((oldColor & 8) != 8))
		strcat(a, "H");

	if ((col & 128) == 128) {
		strcat(a, "I");
	}
	if ((col & 15) != (oldColor & 15))
		switch (col & 7) {
		case 0:
			strcat(a, "K");
			break;
		case 1:
			strcat(a, "B");
			break;
		case 2:
			strcat(a, "G");
			break;
		case 3:
			strcat(a, "C");
			break;
		case 4:
			strcat(a, "R");
			break;
		case 5:
			strcat(a, "M");
			break;
		case 6:
			strcat(a, "Y");
			break;
		case 7:
			strcat(a, "W");
			break;
		}
	if ((col & 112) != (oldColor & 112))
		switch ((col & 112) >> 4) {
		case 0:
			strcat(a, "0");
			break;
		case 1:
			strcat(a, "4");
			break;
		case 2:
			strcat(a, "2");
			break;
		case 3:
			strcat(a, "6");
			break;
		case 4:
			strcat(a, "1");
			break;
		case 5:
			strcat(a, "5");
			break;
		case 6:
			strcat(a, "3");
			break;
		case 7:
			strcat(a, "7");
			break;
		}
	oldColor = col;
	return a;
}

char           *
AnsiColor(unsigned char col)
{
	static char           a[30];
	if (col == oldColor)
		return NULL;
	a[0]=0;
	if ((oldColor >= 128) && (col < 128)) {
		strcpy(a, "[0m");
		oldColor = 7;
		if (col == 7)
			return a;
	}
	strcat(a, "[");
	if (((col & 8) != 8) & ((oldColor & 8) == 8)) {
		strcat(a, "0;");
		oldColor = oldColor & 15;
	}
	if (((col & 8) == 8) & ((oldColor & 8) != 8))
		strcat(a, "1;");

	if ((col & 128) == 128) {
		strcat(a, "5");
		if ((col & 15) != (oldColor & 15) || (col & 112) != (oldColor & 112))
			strcat(a, ";");
	}
	if ((col & 15) != (oldColor & 15))
		switch (col & 7) {
		case 0:
			strcat(a, "30");
			break;
		case 1:
			strcat(a, "34");
			break;
		case 2:
			strcat(a, "32");
			break;
		case 3:
			strcat(a, "36");
			break;
		case 4:
			strcat(a, "31");
			break;
		case 5:
			strcat(a, "35");
			break;
		case 6:
			strcat(a, "33");
			break;
		case 7:
			strcat(a, "37");
			break;
		}
	if (((col & 15)) != (oldColor & 15) && (col & 112) != (oldColor & 112))
		strcat(a, ";");
	if ((col & 112) != (oldColor & 112))
		switch ((col & 112) >> 4) {
		case 0:
			strcat(a, "40");
			break;
		case 1:
			strcat(a, "44");
			break;
		case 2:
			strcat(a, "42");
			break;
		case 3:
			strcat(a, "46");
			break;
		case 4:
			strcat(a, "41");
			break;
		case 5:
			strcat(a, "45");
			break;
		case 6:
			strcat(a, "43");
			break;
		case 7:
			strcat(a, "47");
			break;
		}
	strcat(a, "m");
	oldColor = col;
	return a;
}

int 
SelectSaveMode(void)
{
	clrline();
	stri[1] = "Clearscreen";
	stri[2] = "Home";
	stri[3] = "None";
	return auswahl(3, 16, 1);
}

char           *
EnterName(char *b)
{
	char           *a;
	char           *ext;
	struct			text_info	ti;

	gettextinfo(&ti);
	clrline();
	CoolWrite(1, ti.screenheight, "Enter Filename :");
	a = "";
	a = inputfield(a, 60, 16, ti.screenheight - 1);
	if (strlen(a) == 0)
		a = strdup("NONAME");
	ext = strchr(a, '.');
	if (ext == NULL)
		a = strcat(a, b);
	return a;
}

int 
CharCount(int d, int e, int a, int chr)
{
	int             b, c = 0;
	for (b = d; b <= e; b++)
		if ((Screen[ActivePage][a][b << 1] == chr) & (Screen[ActivePage][a][(b << 1) + 1] == Screen[ActivePage][a][(d << 1) + 1]))
			c++;
		else
			break;
	return c;
}

int 
Numberofchars(int a)
{
	int             b, c = 0;
	for (b = 0; b <= 79; b++)
		if (((Screen[ActivePage][a][b << 1] != 32) & (Screen[ActivePage][a][b << 1] != 0)
		     ) |
		    ((Screen[ActivePage][a][(b << 1) + 1] & 112) != 0))
			c = b;
	return c;
}

void 
save(void)
{
	char           *Name, *s;
	FILE           *fp;
	int             x, y, z, chnum;
	struct			text_info	ti;

	gettextinfo(&ti);
	oldColor = 0;
	stri[1] = "aNsi";
	stri[2] = "aVatar";
	stri[3] = "Pcboard";
	stri[4] = "aScii";
	stri[5] = "Binary";
	stri[6] = "C";
	stri[7] = "Sync";
	stri[8] = "Abort";
	x = auswahl(8, 0, 1);
	if (LastLine >= MaxLines)
		LastLine = MaxLines - 1;
	switch (x) {
	case 1:		/* ANSI Save */
		Name = EnterName(".ans");
		fp = fopen(Name, "wb");
		switch (SelectSaveMode()) {
		case 1:
			fprintf(fp, "[2J");
			break;
		case 2:
			fprintf(fp, "[1;1H");
			break;
		}
		for (y = 0; y <= LastLine; y++) {
			/*
			 * if (y>0) if (Numberofchars(y-1)>=79)
			 * fprintf(fp,"[A");
			 */
			chnum = Numberofchars(y);
			for (x = 0; x <= chnum; x++) {
				z = CharCount(x, chnum, y, ' ');
				if ((z > 2) & ((Screen[ActivePage][y][(x << 1) + 1] & 112) == 0)) {
					fprintf(fp, "[%dC", z);
					x += z - 1;
				} else {
					if (Screen[ActivePage][y][x << 1] == 0)
						Screen[ActivePage][y][x << 1] = 32;
					s = AnsiColor(Screen[ActivePage][y][(x << 1) + 1]);
					if (s != NULL)
						fprintf(fp, "%s", s);
					fputc(Screen[ActivePage][y][x << 1], fp);
				}
			}
			fputc(10, fp);
		}
		fprintf(fp, "[0m");

		SauceDescr.DataType = 1;
		SauceDescr.FileType = 1;
		SauceDescr.TInfo1 = 80;
		SauceDescr.TInfo2 = LastLine;
		if (SaveSauce)
			AppendSauce(fp);

		fclose(fp);
		break;
	case 2:		/* AVATAR Save */
		Name = EnterName(".avt");
		fp = fopen(Name, "wb");
		switch (SelectSaveMode()) {
		case 1:
			fputc('', fp);
			break;
		case 2:
			fprintf(fp, "");
			break;
		}
		for (y = 0; y <= LastLine; y++) {
			chnum = Numberofchars(y);
			for (x = 0; x <= chnum; x++) {
				z = CharCount(x, chnum, y, Screen[ActivePage][y][x << 1]);
				s = AvatarColor(Screen[ActivePage][y][(x << 1) + 1]);
				if (s != NULL)
					fprintf(fp, "%s", s);
				if ((z > 2) & ((Screen[ActivePage][y][(x << 1) + 1] & 112) == 0)) {
					fprintf(fp, "%c%c", Screen[ActivePage][y][x << 1], z);
					x += z - 1;
				} else {
					if (Screen[ActivePage][y][(x << 1) + 1] == 0)
						fputc(0, fp);
					if (Screen[ActivePage][y][x << 1] == 0)
						Screen[ActivePage][y][x << 1] = 32;
					fputc(Screen[ActivePage][y][x << 1], fp);
				}
			}
			fputc(10, fp);
		}
		fprintf(fp, "");
		SauceDescr.DataType = 1;
		SauceDescr.FileType = 5;
		if (SaveSauce)
			AppendSauce(fp);
		fclose(fp);
		break;
	case 3:
		Name = EnterName(".pcb");
		fp = fopen(Name, "wb");
		switch (SelectSaveMode()) {
		case 1:
			fprintf(fp, "@CLS@");
			break;
		case 2:
			fprintf(fp, "@HOME@");
			break;
		}
		for (y = 0; y <= LastLine; y++) {
			for (x = 0; x <= Numberofchars(y); x++) {
				if (Screen[ActivePage][y][x << 1] == 0)
					Screen[ActivePage][y][x << 1] = 32;
				s = PCBoardColor(Screen[ActivePage][y][(x << 1) + 1]);
				if (s != NULL)
					fprintf(fp, "%s", s);
				fputc(Screen[ActivePage][y][x << 1], fp);
			}
			fputc(10, fp);
		}
		SauceDescr.DataType = 1;
		SauceDescr.FileType = 4;
		SauceDescr.TInfo1 = 80;
		SauceDescr.TInfo2 = LastLine;
		if (SaveSauce)
			AppendSauce(fp);

		fclose(fp);
		break;
	case 4:
		Name = EnterName(".asc");
		fp = fopen(Name, "wb");
		for (y = 0; y <= LastLine; y++) {
			for (x = 0; x <= Numberofchars(y); x++) {
				if (Screen[ActivePage][y][x << 1] == 0)
					Screen[ActivePage][y][x << 1] = 32;
				fputc(Screen[ActivePage][y][x << 1], fp);
			}
			fputc(10, fp);
		}
		SauceDescr.DataType = 1;
		SauceDescr.FileType = 0;
		SauceDescr.TInfo1 = 80;
		SauceDescr.TInfo2 = LastLine;
		if (SaveSauce)
			AppendSauce(fp);
		fclose(fp);
		break;
	case 5:
		Name = EnterName(".bin");
		fp = fopen(Name, "wb");
		for (y = 0; y <= LastLine; y++) {
			for (x = 0; x <= 159; x++)
				fputc(Screen[ActivePage][y][x], fp);
		}
		fclose(fp);
		break;
	case 6:
		Name = EnterName(" ");
		fp = fopen(Name, "wb");
		fprintf(fp, "unsigned char %s[%d]={\n", Name, (LastLine + 1) * 160);
		for (y = 0; y <= LastLine; y++) {
			for (x = 0; x <= 159; x++) {
				fprintf(fp, "%d", Screen[ActivePage][y][x]);
				if (x < 159)
					fputc(',', fp);
			}
			if (y < LastLine)
				fputc(',', fp);
			else
				fprintf(fp, "};");
			fputc(10, fp);
		}
		SauceDescr.DataType = 5;
		SauceDescr.FileType = 40;
		/* if (SaveSauce) AppendSauce(fp); */
		fclose(fp);
		break;
	case 7:
		Name = EnterName(".msg");
		fp = fopen(Name, "wb");
		switch (SelectSaveMode()) {
		case 1:
			fprintf(fp, "L");
			break;
		case 2:
			/*Clear Screen on Home since sync doesn 't support homeing. */
				fprintf(fp, "L");
			break;
		}
		for (y = 0; y <= LastLine; y++) {
			/*
			 * if (y>0) if (Numberofchars(y-1)>=79)
			 * fprintf(fp,"[A");
			 */
			chnum = Numberofchars(y);
			for (x = 0; x <= chnum; x++) {
				z = CharCount(x, chnum, y, ' ');
				if ((z > 2) & ((Screen[ActivePage][y][(x << 1) + 1] & 112) == 0)) {
					fprintf(fp, "%c", (z + 127));
					x += z - 1;
				} else {
					if (Screen[ActivePage][y][x << 1] == 0)
						Screen[ActivePage][y][x << 1] = 32;
					s = SyncColor(Screen[ActivePage][y][(x << 1) + 1]);
					if (s != NULL)
						fprintf(fp, "%s", s);
					fputc(Screen[ActivePage][y][x << 1], fp);
				}
			}
			fputc(10, fp);
		}
		fprintf(fp, "N");

		SauceDescr.DataType = 1;
		/*I really don 't know the "Sauce" specs. */
			SauceDescr.FileType = 8;
		SauceDescr.TInfo1 = 80;
		SauceDescr.TInfo2 = LastLine;
		if (SaveSauce)
			AppendSauce(fp);

		fclose(fp);
		break;
	}
}
