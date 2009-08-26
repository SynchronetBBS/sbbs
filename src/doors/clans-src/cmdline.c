/********************************************************************
  Copyright (c) 2002 Michael Dillon

  Permission is hereby granted, free of charge, to any person
  obtaining a copy of this software and associated documentation
  files (the "Software"), to deal in the Software without
  restriction, including without limitation the rights to use,
  copy, modify, merge, publish, distribute, sublicense, and/or
  sell copies of the Software, and to permit persons to
  whom the Software is furnished to do so, subject to the
  following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
  OTHER DEALINGS IN THE SOFTWARE.
*********************************************************************/
/* cmdline.c

   The purpose of this file is to provide a few functions to convert
   Windows single string commandline into the C style commandline
   arguments.
*/

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <tchar.h>
#include <ctype.h>
#include "cmdline.h"

void display_win32_error()
{
	LPVOID message;
	TCHAR buffer[1000];

	FormatMessage(
		FORMAT_MESSAGE_IGNORE_INSERTS|
		FORMAT_MESSAGE_FROM_SYSTEM|
		FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&message,
		0,
		NULL);

	_stprintf(buffer, _T("Win32 System Error:\n%s\n"), message);
	MessageBox(NULL, buffer, _T("System Error"), MB_OK | MB_ICONERROR);

	LocalFree(message);
}

int commandline_create(char *cmd_line, char ***argv, int *argc)
{
	char *option;
	int i = 0, len;
	TCHAR *program_path;
	*argv = NULL;
	*argc = 0;

	option = (char *) calloc(1, strlen(cmd_line) + 5);
	if (!option) {
		display_win32_error();
		return 0;
	}

	/* Initialize *argv[] with the program's name as first entry */
	program_path = (char *) calloc(1, _MAX_PATH);
	GetModuleFileName(NULL, program_path, _MAX_PATH);
	program_path = (char *) realloc(program_path, strlen(program_path) + 1);
	(*argv) = (char **) malloc(strlen(program_path) + 1);
	(*argv)[(*argc)++] = program_path;

	while (i < (signed)strlen(cmd_line)) {
		/* Skip over excess whitespace */
		while (isspace(cmd_line[i]))
			++i;
		/* Check for quoted strings */
		if (cmd_line[i] == '\"') {
			char *p_cur_quote = &cmd_line[i+1];
			char *p_next_quote = strchr((p_cur_quote + 1), '\"');
			if (p_next_quote) {
				strncpy(option, p_cur_quote, (p_next_quote - p_cur_quote));
				++i;
			}
			else
				strncpy(option, p_cur_quote, strlen(p_cur_quote));
		}
		else {
			/* Get non-quoted value */
			char *p_cur = &cmd_line[i];
			char *p_next = strchr(p_cur, ' ');
			if (p_next)
				strncpy(option, p_cur, (p_next - p_cur));
			else
				strncpy(option, p_cur, strlen(p_cur));
		}
		/* Assign new value */
		len = strlen(option);
		i += (len + 1);
		(*argv) = (char **) realloc((*argv), i + strlen(program_path));
		(*argv)[(*argc)] = (char *) calloc(1, (len + 1));
		strncpy((*argv)[(*argc)++], option, len);
		/* Memory cleanup */
		memset((void *) option, 0, len);
	}

	free(option);
	return 1;
}

int commandline_destroy(char ***argv, int argc)
{
	int i;

	for (i = 0; i < argc; i++)
		free((*argv)[i]);

	free((*argv));

	return 1;
}

#else /* !_WIN32 */
/* Dummy Functions for non-Win32 platforms */
void display_win32_error(void)
{
}

int commandline_create(char *cmd_line, char ***argv, int *argc)
{
	return 0;
}

int commandline_destroy(char ***argv, int argc)
{
	return 0;
}
#endif /* _WIN32 */