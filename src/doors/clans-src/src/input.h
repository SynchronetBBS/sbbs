int16_t GetChoice(char *DisplayFile, char *Prompt, char *Options[], char *Keys, char DefChar, bool ShowTime);
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
 *      ShowTime is set to true if the time is to be displayed in prompt.
 */

void GetStr(char *InputStr, int16_t MaxChars, bool HiBit);
/*
 * This function allows the user to input a string of MaxChars length and
 * place it in InputStr.
 *
 * PRE: InputStr MAY contain a string which contains a value, HiBit
 *      can be used to toggle whether the user can enter hibit ascii.
 */

int32_t GetLong(char *Prompt, int32_t DefaultVal, int32_t Maximum);
/*
 * This allows the user to input a long integer.
 *
 */

void GetStringChoice(char **apszChoices, int16_t NumChoices, char *szPrompt,
					 int16_t *UserChoice, bool ShowChoicesInitially, int16_t DisplayType, bool AllowBlank);
/*
 * This will choose a string from a listing and return which was chosen in
 * UserChoice.
 *
 * PRE:   apszChoices is an array of the choices (strings)
 *        NumChoices contains how many total choices (strings) there are
 *        szPrompt contains the string to prompt user with.
 *        ShowChoicesInitially = true if you wish to list the choices
 *          when this is called.
 *        DisplayType determines how the choices are listed.
 *        AllowBlank = true if you wish to allow the user to choose nothing
 *          (i.e. press enter) or = false if you want the user to choose
 *          one of the options.
 */
