/*
 * mocks_door.h
 *
 * Shared mocks for door.h functionality.  Provides all 21 door output and
 * input function stubs used by 10+ test files.
 *
 * Optionally defines struct Door global if MOCKS_DOOR_SKIP_GLOBAL is not
 * defined.  Set MOCKS_DOOR_SKIP_GLOBAL before #including this header if
 * your test file #includes door.c directly (which already defines Door).
 */

#ifndef MOCKS_DOOR_H
#define MOCKS_DOOR_H

void rputs(const char *s)
{
	(void)s;
}

#ifndef MOCKS_DOOR_SKIP_LOG_DISPLAY_STR
void LogDisplayStr(const char *s)
{
	(void)s;
}
#endif

void LogStr(const char *s)
{
	(void)s;
}

char GetKey(void)
{
	return 0;
}

char GetKeyNoWait(void)
{
	return 0;
}

char GetAnswer(const char *s)
{
	(void)s;
	return 0;
}

void door_pause(void)
{
}

void Display(char *f)
{
	(void)f;
}

bool YesNo(char *s)
{
	(void)s;
	return false;
}

bool NoYes(char *s)
{
	(void)s;
	return false;
}

bool Door_AllowScreenPause(void)
{
	return false;
}

void Door_ToggleScreenPause(void)
{
}

void Door_ShowTitle(void)
{
}

void InputCallback(void)
{
}

void PutCh(char c)
{
	(void)c;
}

void rawputs(const char *s)
{
	(void)s;
}

int16_t GetHoursLeft(void)
{
	return 0;
}

int16_t GetMinutesLeft(void)
{
	return 0;
}

void Door_Init(bool local)
{
	(void)local;
}

void Door_Close(void)
{
}

bool Door_Initialized(void)
{
	return false;
}

void Door_SetColorScheme(int8_t *s)
{
	(void)s;
}

#ifndef MOCKS_DOOR_SKIP_GLOBAL
struct Door Door;
#endif

#endif /* MOCKS_DOOR_H */
