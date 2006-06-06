#include <stdio.h>
#include <stdlib.h>

#include "xpendian.h"

#include "homedir.h"
#include "config.h"
#include "effekt.h"
#include "fonts.h"
#include "syncdraw.h"

/*
 * Version      4 Byte Color        1 Byte TABPos      80 Byte Outline      1
 * Byte EffekStruct  1 teffekt
 */
void 
loadconfig(void)
{
	FILE           *fp;
	int             ver, i;
	char            fname[255];
	sprintf(fname, "%s%s", homedir(), CONFIGFILE);
	fp = fopen(fname, "rb");
	if (fp != NULL) {
		fread(&ver, 4, 1, fp);
		ver=LE_LONG(ver);
		if (ver >= 1) {
			fread(&Attribute, 1, 1, fp);
			for (i = 1; i <= 80; i++)
				fread(&tabs[i], 1, 1, fp);
			fread(&Outline, 1, 1, fp);
			fread(&effect, sizeof(effect), 1, fp);
		}
		if (ver == 2) {
			fread(&InsertMode, 1, 1, fp);
			fread(&EliteMode, 1, 1, fp);
			fread(&FullScreen, 1, 1, fp);
		}
		fclose(fp);
	}
}

void 
saveconfig(void)
{
	FILE           *fp;
	int             ver = 2, i;
	char            fname[255];
	sprintf(fname, "%s%s", homedir(), CONFIGFILE);
	fp = fopen(fname, "wb");
	if (fp != NULL) {
		ver=LE_LONG(ver);
		fwrite(&ver, 4, 1, fp);
		ver=LE_LONG(ver);
		fwrite(&Attribute, 1, 1, fp);
		for (i = 1; i <= 80; i++)
			fwrite(&tabs[i], 1, 1, fp);
		fwrite(&Outline, 1, 1, fp);
		fwrite(&effect, sizeof(effect), 1, fp);
		fwrite(&InsertMode, 1, 1, fp);
		fwrite(&EliteMode, 1, 1, fp);
		fwrite(&FullScreen, 1, 1, fp);
		fclose(fp);
	}
}
