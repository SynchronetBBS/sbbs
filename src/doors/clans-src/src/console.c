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

void CheckMem(const void *Test)
/*
 * Gives system error if the pointer is NULL.
 */
{
	if (Test == NULL) {
		printf("Checkmem Failed.\n");
		ShowTextCursor(true);
		Video_Close();
		exit(EXIT_FAILURE);
	}
}

void System_Error(char *szErrorMsg)
/*
 * purpose  To output an error message and close down the system.
 *          This SHOULD be run from anywhere and NOT fail.  It should
 *          be FOOLPROOF.
 */
{
	zputs("\n|12Error: |07");
	zputs(szErrorMsg);
	delay(1000);
	ShowTextCursor(true);
	Video_Close();
	exit(EXIT_FAILURE);
}
