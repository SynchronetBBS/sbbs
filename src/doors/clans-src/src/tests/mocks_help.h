/*
 * mocks_help.h
 *
 * Shared mocks for help.h functionality.
 */

#ifndef MOCKS_HELP_H
#define MOCKS_HELP_H

void MainHelp(void)
{
}

void Help(const char *Topic, char *File)
{
	(void)Topic; (void)File;
}

void GeneralHelp(char *pszFileName)
{
	(void)pszFileName;
}

#endif /* MOCKS_HELP_H */
