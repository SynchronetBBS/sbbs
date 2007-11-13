/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _SYNCTERM_FONTS_H
#define _SYNCTERM_FONTS_H

struct font_files {
	char	*name;
	char	*path8x8;
	char	*path8x14;
	char	*path8x16;
};

void free_font_files(struct font_files *ff);
void save_font_files(struct font_files *fonts);
struct font_files *read_font_files(int *count);
void load_font_files(void);
int	find_font_id(char *name);
void font_management(void);

#endif
