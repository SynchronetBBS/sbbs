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
/* cmdline.h

   Header file for cmdline.c, provides function prototypes which will
   only be included once when compiled.
*/
#ifndef THE_CLANS__W32_COMMANDLINE_CREATION__H
#define THE_CLANS__W32_COMMANDLINE_CREATION__H 1

#ifdef _WIN32

void display_win32_error();
int commandline_create(char *, char ***, int *);
int commandline_destroy(char ***, int);

#endif /* _WIN32 */

#endif /* THE_CLANS__W32_COMMANDLINE_CREATION__H */
