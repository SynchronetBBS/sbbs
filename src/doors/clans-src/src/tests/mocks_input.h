/*
 * mocks_input.h
 *
 * Shared mocks for input.h functionality.
 */

#ifndef MOCKS_INPUT_H
#define MOCKS_INPUT_H

char GetChoice(char *DisplayFile, char *Prompt, char *Options[],
	char *Keys, char DefChar, bool ShowTime)
{
	(void)DisplayFile; (void)Prompt; (void)Options; (void)Keys;
	(void)DefChar; (void)ShowTime;
	return 0;
}

void GetStr(char *InputStr, int16_t MaxChars, bool HiBit)
{
	(void)HiBit;
	if (InputStr && MaxChars > 0) InputStr[0] = '\0';
}

long GetLong(const char *Prompt, long DefaultVal, long Maximum)
{
	(void)Prompt; (void)DefaultVal; (void)Maximum;
	return 0;
}

void GetStringChoice(const char **apszChoices, int16_t NumChoices,
	char *szPrompt, int16_t *UserChoice, bool ShowChoicesInitially,
	int16_t DisplayType, bool AllowBlank)
{
	(void)apszChoices; (void)NumChoices; (void)szPrompt;
	(void)UserChoice; (void)ShowChoicesInitially; (void)DisplayType;
	(void)AllowBlank;
}

#endif /* MOCKS_INPUT_H */
