_INT16 GetChoice(char *DisplayFile, char *Prompt, char *Options[], char *Keys, char DefChar, __BOOL ShowTime);
/*
 * This function allows the user to choose an option from the options
 * listed.  Works much like FE's input system.
 *
 * PRE: DisplayFile contains the name of file to display.
 *      Prompt contains the prompt user will see.
 *      Options contains the array of options.
 *      Keys contains the corresponding keys to input to choose options.
 *      DefChar MUST contain the default character the user will choose if he
 *        presses Enter.
 *      ShowTime is set to TRUE if the time is to be displayed in prompt.
 */

void GetStr(char *InputStr, _INT16 MaxChars, __BOOL HiBit);
/*
 * This function allows the user to input a string of MaxChars length and
 * place it in InputStr.
 *
 * PRE: InputStr MAY contain a string which contains a value, HiBit
 *      can be used to toggle whether the user can enter hibit ascii.
 */

long GetLong(char *Prompt, long DefaultVal, long Maximum);
/*
 * This allows the user to input a long integer.
 *
 */

void GetStringChoice(char **apszChoices, _INT16 NumChoices, char *szPrompt,
					 _INT16 *UserChoice, __BOOL ShowChoicesInitially, _INT16 DisplayType, __BOOL AllowBlank);
/*
 * This will choose a string from a listing and return which was chosen in
 * UserChoice.
 *
 * PRE:   apszChoices is an array of the choices (strings)
 *        NumChoices contains how many total choices (strings) there are
 *        szPrompt contains the string to prompt user with.
 *        ShowChoicesInitially = TRUE if you wish to list the choices
 *          when this is called.
 *        DisplayType determines how the choices are listed.
 *        AllowBlank = TRUE if you wish to allow the user to choose nothing
 *          (i.e. press enter) or = FALSE if you want the user to choose
 *          one of the options.
 */
