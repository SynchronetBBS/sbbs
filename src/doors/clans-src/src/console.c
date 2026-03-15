#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "platform.h"

#include "console.h"
#include "defines.h"
#include "video.h"

static bool script_mode = false;
static char (*get_answer_hook)(const char *szAllowableChars) = NULL;
static char (*get_key_hook)(void) = NULL;

void Console_SetScriptMode(bool mode)
{
	script_mode = mode;
}

void Console_SetGetAnswerHook(char (*hook)(const char *szAllowableChars))
{
	get_answer_hook = hook;
}

void Console_SetGetKeyHook(char (*hook)(void))
{
	get_key_hook = hook;
}

bool Door_Initialized(void)
{
	return false;
}

void rputs(const char *str)
{
	zputs(str);
}

char GetKey(void)
{
	if (get_key_hook)
		return get_key_hook();
	int ret = cio_getch();
	if (ret < 0)
		return 0;
	return (char)ret;
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
		exit(EXIT_FAILURE);
	}
}

noreturn void System_Error(char *szErrorMsg)
/*
 * purpose  To output an error message and close down the system.
 *          This SHOULD be run from anywhere and NOT fail.  It should
 *          be FOOLPROOF.
 */
{
	zputs("\n|12Error: |07");
	zputs(szErrorMsg);
	plat_Delay(1000);
	ShowTextCursor(true);
	Video_Close();
	exit(EXIT_FAILURE);
}

void LogDisplayStr(const char *szString)
{
	DisplayStr(szString);
}

char GetAnswer(const char *szAllowableChars)
{
	int cKey;
	uint16_t iTemp;

	if (get_answer_hook)
		return get_answer_hook(szAllowableChars);

	for (;;) {
		cKey = cio_getch();
		if (cKey == 0 || cKey == 0xE0) {
			cio_getch();
			continue;
		}

		/* see if allowable */
		for (iTemp = 0; iTemp < strlen(szAllowableChars); iTemp++) {
			if (toupper(cKey) == toupper(szAllowableChars[iTemp]))
				break;
		}

		if (iTemp < strlen(szAllowableChars))
			break;  /* found allowable key */
	}

	return (char)(toupper(cKey));
}

static bool yesNoQ(char *Query, bool Current)
{
	char Answer;
	/* show query */
	zputs(Query);

	/* show Yes/no */
	if (Current)
		zputs(" |01(|15Yes|07/no|01) |11");
	else
		zputs(" |01(|07yes|15/No|01) |11");

	/* get user input */
	Answer = GetAnswer("YN\r\n");
	if (Answer == 'N') {
		/* user says NO */
		zputs("No\n");
		return false;
	}
	else if (Answer == 'Y') {  /* user says YES */
		zputs("Yes\n");
		return true;
	}
	zputs(Current ? "Yes\n" : "No\n");
	return Current;
}

bool
YesNo(char *Query)
{
	return yesNoQ(Query, true);
}

bool
NoYes(char *Query)
{
	return yesNoQ(Query, false);
}

void InputCallback(void)
{
}

void PutCh(char ch)
{
	char str[2] = {ch, 0};
	zputs(str);
}

void rawputs(const char *str)
{
	for (const char *ch = str; *ch; ch++)
		clans_putch((unsigned char)(*ch));
}

int16_t GetHoursLeft(void)
{
	return 23;
}

int16_t GetMinutesLeft(void)
{
	return 59;
}

void door_pause(void)
/*
 * Displays <pause> prompt.
 * TODO: Does not have "JustPaused" handling as in door.c
 */
{
	if (script_mode)
		return;

	const char *pc = "|S|0V<|0Wpaused|0V>|R";

	rputs(pc);

	/* wait for user to hit key */
	while (GetKey() == false)
		InputCallback();

	PutCh('\r');
	pc = "|S|0V<|0Wpaused|0V>|R";
	while (*pc) {
		PutCh(' ');
		pc++;
	}
	PutCh('\r');
}

static bool pauseEnabled = true;
bool
Door_AllowScreenPause(void)
{
	return pauseEnabled;
}

void
Door_ToggleScreenPause(void)
{
	pauseEnabled = !pauseEnabled;
}

char
GetKeyNoWait(void)
{
	return 0;
}
