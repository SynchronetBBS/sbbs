#ifndef THE_CLANS__CONSOLE___H
#define THE_CLANS__CONSOLE___H

#include <stdbool.h>
#include <stdnoreturn.h>
#include "defines.h"
#include "video.h"

bool Door_Initialized(void);
void rputs(const char *str);
void CheckMem(void *Test);
noreturn void System_Error(char *szErrorMsg);
void LogDisplayStr(const char *szString);
char GetAnswer(const char *szAllowableChars);
void Console_SetScriptMode(bool mode);
void Console_SetGetAnswerHook(char (*hook)(const char *szAllowableChars));
bool YesNo(char *Query);
bool NoYes(char *Query);
void InputCallback(void);
void PutCh(char ch);
void rawputs(const char *str);
int16_t GetHoursLeft(void);
int16_t GetMinutesLeft(void);
void door_pause(void);
bool Door_AllowScreenPause(void);
void Door_ToggleScreenPause(void);
char GetKeyNoWait(void);

#endif
