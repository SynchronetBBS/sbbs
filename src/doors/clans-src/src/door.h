#ifndef THE_CLANS__DOOR___H
#define THE_CLANS__DOOR___H

#include "structs.h"

extern struct Door Door;

/*
 * Door-specific ADT
 */

void Door_Init(bool Local);
/*
 * Called to initialize OpenDoors specific info (od_init namely) and
 * load the dropfile or prompt the user for local login.
 */

void Door_Close(void);
/*
 * Called to destroy anything created by Door_Init.
 */

bool Door_Initialized(void);
/*
 * Returns true if od_init was already called.
 */


void Door_SetColorScheme(int8_t *ColorScheme);
/*
 * This function will set the color scheme.
 */

void rputs(const char *string);
/*
 * Outputs the pipe or ` encoded string.
 */

void LogDisplayStr(const char *szString);
/*
 * Logs a string then passes to DisplayStr()
 */

void LogStr(const char *szString);
/*
 * Logs a string
 */

void door_pause(void);
/*
 * Displays <pause> prompt.
 */

void Display(char *FileName);
/*
 * Displays the file given.
 */

int16_t YesNo(char *Query);
int16_t NoYes(char *Query);

bool Door_AllowScreenPause(void);

void Door_ShowTitle(void);

#endif
