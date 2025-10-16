#ifndef THE_CLANS__DOOR___H
#define THE_CLANS__DOOR___H

#include "structs.h"

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


void Door_SetColorScheme(char *ColorScheme);
/*
 * This function will set the color scheme.
 */

void rputs(char *string);
/*
 * Outputs the pipe or ` encoded string.
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
