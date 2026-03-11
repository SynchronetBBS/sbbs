/*

The Clans BBS Door Game
Copyright (C) 1997-2002 Allen Ussher

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef THE_CLANS__U8CP437__H
#define THE_CLANS__U8CP437__H

#include <stdio.h>

// Probe for .u8. variant of filename; returns opened FILE*.
// Sets *is_utf8 = 1 if .u8. variant found, 0 otherwise.
// On failure returns NULL (neither variant exists).
FILE *u8_fopen(const char *filename, const char *mode, int *is_utf8);

// fgets() wrapper.  If is_utf8, converts the line from UTF-8 to CP437
// in-place (output is always <= input length).  Returns same as fgets().
// Prints error to stderr with filename:lineno on unmapped codepoints.
char *u8_fgets(char *buf, int size, FILE *fp, int is_utf8,
               const char *filename, int *lineno);

// Low-level: convert a UTF-8 buffer to CP437 in-place.
// Returns 0 on success, -1 on error (message printed to stderr).
int u8_to_cp437(unsigned char *buf, size_t len,
                const char *filename, int lineno);

#endif /* THE_CLANS__U8CP437__H */
