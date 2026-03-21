/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _MENU_H_
#define _MENU_H_

#include "bbslist.h"

enum syncmenu_result {
	SM_SCROLLBACK,
	SM_DISCONNECT,
	SM_LOGIN,
	SM_UPLOAD,
	SM_DOWNLOAD,
	SM_OUTPUT_RATE,
	SM_LOG_LEVEL,
	SM_CAPTURE,
	SM_MUSIC,
	SM_FONT,
	SM_DOORWAY,
	SM_MOUSE,
#ifndef WITHOUT_OOII
	SM_OOII,
#endif
	SM_EXIT,
	SM_DIRECTORY,
};

int syncmenu(struct bbslist *, int *speed);
void viewscroll(void);

#endif
