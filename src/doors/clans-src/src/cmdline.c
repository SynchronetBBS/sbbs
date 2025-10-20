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
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>

#include "cmdline.h"

void display_win32_error(void)
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

	_stprintf(buffer, _T("Win32 System Error:\n%s\n"), (char*)message);
	MessageBox(NULL, buffer, _T("System Error"), MB_OK | MB_ICONERROR);

	LocalFree(message);
}

#else /* !_WIN32 */

/* Dummy Functions for non-Win32 platforms */
void display_win32_error(void)
{
}

#endif /* _WIN32 */
