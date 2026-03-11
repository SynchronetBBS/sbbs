/*
 * mocks_language.h
 *
 * Shared mocks for language.h functionality.  Provides Language struct and
 * related stubs used by 10+ test files.
 */

#ifndef MOCKS_LANGUAGE_H
#define MOCKS_LANGUAGE_H

static struct Language g_lang;
struct Language *Language = &g_lang;

void Language_Init(char *f)
{
	(void)f;
}

void Language_Close(void)
{
}

void LoadStrings(int16_t s, int16_t n, char *arr[])
{
	for (int16_t i = 0; i < n; i++) arr[i] = "";
	(void)s;
}

#endif /* MOCKS_LANGUAGE_H */
