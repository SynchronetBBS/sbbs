#include <stdio.h>
#include <stdlib.h>
#include "unix_wrappers.h"

#include "console.h"
#include "defines.h"
#include "video.h"

bool Door_Initialized(void)
{
	return false;
}

void rputs(const char *str)
{
	zputs(str);
}

void CheckMem(void *Test)
/*
 * Gives system error if the pointer is NULL.
 */
{
	if (Test == NULL) {
		printf("Checkmem Failed.\n");
		ShowTextCursor(true);
		Video_Close();
		exit(0);
	}
}

void System_Error(char *szErrorMsg)
/*
 * purpose  To output an error message and close down the system.
 *          This SHOULD be run from anywhere and NOT fail.  It should
 *          be FOOLPROOF.
 */
{
	qputs("|12Error: |07", 0,0);
	qputs(szErrorMsg, 0, 7);
	delay(1000);
	ShowTextCursor(true);
	Video_Close();
	exit(0);
}
