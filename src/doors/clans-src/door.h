/*
 * Door-specific ADT
 */

void Door_Init(__BOOL Local);
/*
 * Called to initialize OpenDoors specific info (od_init namely) and
 * load the dropfile or prompt the user for local login.
 */

void Door_Close(void);
/*
 * Called to destroy anything created by Door_Init.
 */

__BOOL Door_Initialized(void);
/*
 * Returns TRUE if od_init was already called.
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

_INT16 YesNo(char *Query);
_INT16 NoYes(char *Query);

void Door_ToggleScreenPause(void);
void Door_SetScreenPause(__BOOL State);
__BOOL Door_AllowScreenPause(void);

void Door_ShowTitle(void);
