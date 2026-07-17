/* Copyright (C), 2007 by Stephen Hurd */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "ciolib.h"
#include "fonts.h"
#include "gen_defs.h"
#include "ini_file.h"
#include "syncterm.h"

void
free_font_files(struct font_files *ff)
{
	int i;

	if (ff == NULL)
		return;
	for (i = 0; ff[i].name != NULL; i++) {
		FREE_AND_NULL(ff[i].name);
		FREE_AND_NULL(ff[i].path8x8);
		FREE_AND_NULL(ff[i].path8x14);
		FREE_AND_NULL(ff[i].path8x16);
		FREE_AND_NULL(ff[i].path12x20);
	}
	free(ff);
}

bool
save_font_files(struct font_files *fonts)
{
	FILE      *inifile;
	char       inipath[MAX_PATH + 1];
	char       newfont[MAX_PATH + 1];
	char      *fontid;
	str_list_t ini_file;
	str_list_t fontnames;
	int        i;

	if (safe_mode)
		return false;
	get_syncterm_filename(inipath, sizeof(inipath), SYNCTERM_PATH_INI, false);
	if ((inifile = fopen(inipath, "r")) != NULL) {
		ini_file = iniReadFile(inifile);
		fclose(inifile);
	}
	else if (errno == ENOENT) {
		ini_file = strListInit();
	}
	else
		return false;
	if (ini_file == NULL)
		return false;

	fontnames = iniGetSectionList(ini_file, "Font:");

        /* TODO: Remove all sections... we don't *NEED* to do this */
	while ((fontid = strListPop(&fontnames)) != NULL) {
		iniRemoveSection(&ini_file, fontid);
		free(fontid);
	}

	if (fonts != NULL) {
		for (i = 0; fonts[i].name && fonts[i].name[0]; i++) {
			if (snprintf(newfont, sizeof(newfont), "Font:%s",
			    fonts[i].name) >= (int)sizeof(newfont)) {
				strListFree(&fontnames);
				strListFree(&ini_file);
				return false;
			}
			if (fonts[i].path8x8)
				iniSetString(&ini_file, newfont, "Path8x8", fonts[i].path8x8, &ini_style);
			if (fonts[i].path8x14)
				iniSetString(&ini_file, newfont, "Path8x14", fonts[i].path8x14, &ini_style);
			if (fonts[i].path8x16)
				iniSetString(&ini_file, newfont, "Path8x16", fonts[i].path8x16, &ini_style);
			if (fonts[i].path12x20)
				iniSetString(&ini_file, newfont, "Path12x20", fonts[i].path12x20, &ini_style);
		}
	}
	bool success = false;
	if ((inifile = fopen(inipath, "w")) != NULL) {
		success = iniWriteFile(inifile, ini_file);
		if (fclose(inifile) != 0)
			success = false;
	}

	strListFree(&fontnames);
	strListFree(&ini_file);
	return success;
}

static bool
read_font_path(FILE *inifile, const char *section, const char *key,
    char **result)
{
	char fontpath[MAX_PATH + 1];
	char *value = iniReadSString(inifile, section, key, NULL, fontpath,
	    sizeof(fontpath));
	if (value == NULL) {
		*result = NULL;
		return true;
	}
	*result = strdup(fontpath);
	return *result != NULL;
}

struct font_files *
read_font_files(int *count, bool *success)
{
	FILE              *inifile;
	char               inipath[MAX_PATH + 1];
	char              *fontid;
	str_list_t         fonts;
	struct font_files *ret = NULL;
	struct font_files *tmp;

	*count = 0;
	*success = false;
	get_syncterm_filename(inipath, sizeof(inipath), SYNCTERM_PATH_INI, false);
	if ((inifile = fopen(inipath, "r")) == NULL) {
		*success = errno == ENOENT;
		return ret;
	}
	fonts = iniReadSectionList(inifile, "Font:");
	while ((fontid = strListRemove(&fonts, 0)) != NULL) {
		if (!fontid[5]) {
			free(fontid);
			continue;
		}
		if (*count >= 256 - CONIO_FIRST_FREE_FONT) {
			free(fontid);
			goto fail;
		}
		(*count)++;
		tmp = (struct font_files *)realloc(ret, sizeof(struct font_files) * (*count + 1));
		if (tmp == NULL) {
			free(fontid);
			goto fail;
		}
		ret = tmp;
		memset(&ret[*count - 1], 0, sizeof(ret[*count - 1]));
		memset(&ret[*count], 0, sizeof(ret[*count]));
		ret[*count - 1].name = strdup(fontid + 5);
		if (ret[*count - 1].name == NULL) {
			free(fontid);
			goto fail;
		}
		if (!read_font_path(inifile, fontid, "Path8x8",
		    &ret[*count - 1].path8x8) ||
		    !read_font_path(inifile, fontid, "Path8x14",
		    &ret[*count - 1].path8x14) ||
		    !read_font_path(inifile, fontid, "Path8x16",
		    &ret[*count - 1].path8x16) ||
		    !read_font_path(inifile, fontid, "Path12x20",
		    &ret[*count - 1].path12x20)) {
			free(fontid);
			goto fail;
		}
		free(fontid);
	}
	fclose(inifile);
	strListFree(&fonts);
	*success = true;
	return ret;

fail:
	fclose(inifile);
	strListFree(&fonts);
	free_font_files(ret);
	*count = 0;
	return NULL;
}

void
load_font_files(void)
{
	int                count = 0;
	int                i;
	int                nextfont = CONIO_FIRST_FREE_FONT;
	struct font_files *ff;
	FILE              *fontfile;
	char              *fontdata;
	bool               success;

	ff = read_font_files(&count, &success);
	if (!success)
		return;
	for (i = CONIO_FIRST_FREE_FONT; i < 256; i++) {
		FREE_AND_NULL(conio_fontdata[i].eight_by_sixteen);
		FREE_AND_NULL(conio_fontdata[i].eight_by_fourteen);
		FREE_AND_NULL(conio_fontdata[i].eight_by_eight);
		FREE_AND_NULL(conio_fontdata[i].twelve_by_twenty);
		FREE_AND_NULL(conio_fontdata[i].desc);
	}
	for (i = 0; i < count && nextfont < 256; i++) {
		if (ff[i].name == NULL ||
		    (conio_fontdata[nextfont].desc = strdup(ff[i].name)) == NULL)
			continue;
		if (ff[i].path8x8 && ff[i].path8x8[0]) {
			if ((fontfile = fopen(ff[i].path8x8, "rb")) != NULL) {
				if ((fontdata = (char *)malloc(2048)) != NULL) {
					if (fread(fontdata, 1, 2048, fontfile) == 2048)
						conio_fontdata[nextfont].eight_by_eight = fontdata;
					else
						free(fontdata);
				}
				fclose(fontfile);
			}
		}
		if (ff[i].path8x14 && ff[i].path8x14[0]) {
			if ((fontfile = fopen(ff[i].path8x14, "rb")) != NULL) {
				if ((fontdata = (char *)malloc(3584)) != NULL) {
					if (fread(fontdata, 1, 3584, fontfile) == 3584)
						conio_fontdata[nextfont].eight_by_fourteen = fontdata;
					else
						free(fontdata);
				}
				fclose(fontfile);
			}
		}
		if (ff[i].path8x16 && ff[i].path8x16[0]) {
			if ((fontfile = fopen(ff[i].path8x16, "rb")) != NULL) {
				if ((fontdata = (char *)malloc(4096)) != NULL) {
					if (fread(fontdata, 1, 4096, fontfile) == 4096)
						conio_fontdata[nextfont].eight_by_sixteen = fontdata;
					else
						free(fontdata);
				}
				fclose(fontfile);
			}
		}
		if (ff[i].path12x20 && ff[i].path12x20[0]) {
			if ((fontfile = fopen(ff[i].path12x20, "rb")) != NULL) {
				if ((fontdata = (char *)malloc(10240)) != NULL) {
					if (fread(fontdata, 1, 10240, fontfile) == 10240)
						conio_fontdata[nextfont].twelve_by_twenty = fontdata;
					else
						free(fontdata);
				}
				fclose(fontfile);
			}
		}
		nextfont++;
	}
	free_font_files(ff);

	for (i = 0; i < 257; i++)
		font_names[i] = NULL;
	for (i = 0; i < 256 && conio_fontdata[i].desc != NULL; i++) {
		font_names[i] = conio_fontdata[i].desc;
		if (!strcmp(conio_fontdata[i].desc, "Codepage 437 English"))
			default_font = i;
	}

        /* Set default font */
	setfont(default_font, false, 0);
	font_names[i] = "";
}

int
find_font_id(char *name)
{
	int ret = 0;
	int i;

	for (i = 0; i < 256; i++) {
		if (!conio_fontdata[i].desc)
			continue;
		if (!strcmp(conio_fontdata[i].desc, name)) {
			ret = i;
			break;
		}
	}
	return ret;
}
