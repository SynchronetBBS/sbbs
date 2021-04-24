/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef SAUCE_H_
#define SAUCE_H_

#include <stdio.h>
#include <stdbool.h>
#include "saucedefs.h"

struct sauce_charinfo {
	char title[SAUCE_LEN_TITLE + 1];
	char author[SAUCE_LEN_AUTHOR + 1];
	char group[SAUCE_LEN_GROUP + 1];
	char date[SAUCE_LEN_DATE + 1];
	int height;
	int width;
	bool ice_color;
};

#ifdef __cplusplus
extern "C" {
#endif

// Note: Does not support comments
bool sauce_fread_record(FILE*, sauce_record_t*);
bool sauce_fread_charinfo(FILE*, enum sauce_char_filetype*, struct sauce_charinfo*);

#ifdef __cplusplus
}
#endif
#endif /* Don't add anything after this line */
