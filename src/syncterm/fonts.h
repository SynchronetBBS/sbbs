/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _SYNCTERM_FONTS_H
#define _SYNCTERM_FONTS_H

#include <stdbool.h>

struct font_files {
	char *name;
	char *path8x8;
	char *path8x14;
	char *path8x16;
	char *path12x20;
};

void free_font_files(struct font_files *ff);
bool save_font_files(struct font_files *fonts);
struct font_files *read_font_files(int *count, bool *success);
void load_font_files(void);
int find_font_id(char *name);

#endif // ifndef _SYNCTERM_FONTS_H
