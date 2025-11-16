#ifndef TITH_FILE_HEADER
#define TITH_FILE_HEADER

#include <stdio.h>

/*
 * Reads a text line from a file into allocated memory.
 * 
 * A line must end with a a linefeed (0x0A) and must have no control
 * characters other than TAB (0x09) contained in it.
 * 
 * The caller must free() the returned value
 */
char *tith_readLine(FILE *fp);

/*
 * Returns the length of the file specified by fname or -1L on error.
 */
long tith_flen(const char *fname);

#endif
