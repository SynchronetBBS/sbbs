/******************************************************************************
STRINGS.C    TOP string functions.

    Copyright 1993 - 2000 Paul J. Sidorsky

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2, as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

This module contains all of TOP's functions for manipulating strings, including
the command checker and various input routines, except for the word splitter,
which is contained in WORDS.C.
******************************************************************************/

#include "top.h"
#include "strwrap.h"

/* IMPORTANT:  All TOP string functions are designed to work with strings
               of 256 bytes or less (including the terminating \0).  Many
               have larger buffers to prevent overflow, but they should
               not be passed long strings under normal circumstances.
*/

/* fixname() - Adjust the capitalization of a string to a proper name.
   Parameters:  newname - String buffer that will hold the fixed name.
                oldname - String containing the name to fix.
   Returns:  Nothing.
   Notes:  newname and oldname may point to the same place.
*/
void fixname(unsigned char *newname, unsigned char *oldname)
{
XINT d, next; /* Counter, flag if next character should be a capital. */
char fntmp[512]; /* Temporary buffer to hold the fixed name. */

/* There was a configuration option to control name fixing until I found
   out that some fixing is built into the door kit, so overriding it
   wouldn't help and would be confusing. */
/*if (!cfg.fixnames)
    {
    strcpy(fntmp, oldname);
    strcpy(newname, fntmp);
    return;
    }*/

/* Don't fix 0-length strings. */
if (strlen(oldname) < 1)
    {
    newname[0] = '\0';
    return;
    }

/* Prepare the name for fixing. */
strcpy(fntmp, oldname);
strlwr(fntmp);

/* Loop through each character. */
for (d = 0, next = 1; d < strlen(fntmp); d++)
    {
    /* Skip colour codes. */
    if (fntmp[d] == '^')
        {
        d++;
        continue;
        }
    /* Capitalize this character if a space was encountered last time. */
    if (next)
        {
        fntmp[d] = toupper(fntmp[d]);
        next = 0;
        }
    /* Test for whitespace. */
    if (iswhite(fntmp[d])) // Also test namebreakchars!
        {
        next = 1;
        }
    }

strcpy(newname, fntmp);

return;
}

/* filter_string() - Removes PubColour codes from a string.
   Parameters:  newstring - String buffer to hold filtered string.
                oldstring - String to filter colour codes from.
   Returns:  Nothing.
   Notes:  newstring and oldstring may point to the same place.
*/
void filter_string(char *newstring, char *oldstring)
{
XINT d, e; /* oldstring counter, newstring counter. */
char sstmp[256]; /* Temporary filtered string holding buffer. */

/* Don't filter 0-length strings. */
if (strlen(oldstring) < 1)
    {
    newstring[0] = '\0';
    return;
    }

// Optimize with a strchr() test for ^

memset(sstmp, 0, MAXSTRLEN + 1);

/* Loop for each character in oldstring. */
for (d = 0, e = 0; d < strlen(oldstring); d++)
    {
    /* Look for the colour code character (^). */
    if (oldstring[d] == '^')
        {
        /* Two ^ characters in a row mean the user intended to show a
           single ^ instead of a colour code. */
        if (oldstring[d + 1] == '^')
            {
            sstmp[e++] = '^';
            }
        /* Skip the colour code. */
        d++;
        continue;
        }
    /* Copy the character. */
    sstmp[e++] = oldstring[d];
    }

strcpy(newstring, sstmp);

return;
}

/* get_password() - Gets a password from the user.
   Parameters:  buffer - String buffer to hold the password.
                len - Maximum length of the password.
   Returns:  Nothing.
*/
void get_password(char *buffer, XINT len)
{
XINT ps = 0, ky; /* Buffer position, keypress holder. */

memset(buffer, 0, len + 1);

/* Password input loop.  Aborted manually by an ENTER keypress. */
for(;;)
	{
    ky = od_get_key(TRUE);
    /* Backspace. */
    if (ky == 8 && ps > 0)
    	{
        top_output(OUT_SCREEN, getlang("PWBackSpace"));
        buffer[--ps] = '\0';
        }
    /* ENTER ends the input. */
    if (ky == 13)
    	{
        return;
        }
    /* Normal character. */
    if (ky >= ' ' && ky <= MAXASCII && ps < len)
    	{
        /* An echo character (usually a *) is displayed instead of the
           character typed. */
        top_output(OUT_SCREEN, getlang("PWEcho"));
        buffer[ps++] = ky;
        buffer[ps] = '\0';
        }
    }
}

/* trim_string() - Trims spaces from either side of a string.
   Parameters:  newstring - String buffer to hold trimmed string.
                oldstring - String to trim.
                nobits - Action restrictions (NO_ bit constants).
   Returns:  Nothing.
*/
void trim_string(char *newstring, char *oldstring, unsigned char nobits)
{
XINT f = 0; /* Counter. */
char tstmp[512]; /* Temporary trimmed string holding buffer. */

/* Don't trim 0-length strings. */
if (strlen(oldstring) < 1)
    {
    newstring[0] = '\0';
    return;
    }

/* trim_string()'s normal action is to trim spaces from both sides of a
   string (front and back), but this can be controlled using one or more
   NO_ constants in the nobits parameter. */

/* Trim space from left (front) of string. */
if (!(nobits & NO_LTRIM))
	{
    /* Simply skip all characters until a nonwhitespace character is
       encountered. */
    while(iswhite(oldstring[f]) && f < strlen(oldstring))
		{
        f++;
        }
    }
strcpy(tstmp, &oldstring[f]);

/* Trim space from right (back) of string. */
if (!(nobits & NO_RTRIM))
	{
    f = strlen(tstmp) - 1;
    /* Change whitespace characters to \0 from back to front until a
       nonwhitespace character is encountered. */
    while(iswhite(tstmp[f]) && f >= 0)
    	{
        tstmp[f--] = '\0';
        }
	}

/* Compensate for any unfinished PubColour codes. */
if (!stricmp(tstmp, "^"))
    {
    strset(tstmp, '\0');
    }

strcpy(newstring, tstmp);

return;
}

/* iswhite() - Determine if a character is whitespace.
   Parameters:  ch - Character to test.
   Returns:  TRUE if the character is whitespace, FALSE if it is not.
*/
char iswhite(unsigned char ch)
{

/* Although the Borland compilers contain their own whitespace finding
   function (isspace()), I found it to be inadequate for TOP's needs. */

if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\0')
    {
	return 1;
    }

return 0;
}

/* unbuild_pascal_string() - Changes a Pascal string to a C string.
   Parameters:  maxclen - Maximum size of the C string (not including \0).
                buffer - Pascal string to be changed.
   Returns:  Length of the C string, not including terminating \0.
   Notes:  In theory, the length of the C string (including \0) and the
           length of the Pascal string (including length byte) should
           be identical, but maxclen is provided as an extra safety
           measure.  This function is generally used for interfacing with
           BBS types that use Pascal strings in their data files.
*/
XINT unbuild_pascal_string(XINT maxclen, unsigned char *buffer)
{
XINT ut; /* Temporary length holder. */

/* Pascal strings contain their length in the first byte. */
ut = buffer[0];

/* Adjust the length if it is too long. */
if (ut > maxclen)
    {
    ut = maxclen;
    }

/* Don't do anything if the length is 0. */
if (ut == 0)
    {
    return 0;
    }

/* Move everything left one byte, starting at the second byte.  Borland's
   memmove() can do this safely. */
memmove(&buffer[0], &buffer[1], ut);

/* Make the last byte the terminator. */
buffer[ut] = '\0';

return strlen(buffer);
}

/* build_pascal_string() - Changes a C string to a Pascal string.
   Parameters:  buffer - C string to be changed.
   Returns:  Length of the converted string, not including length byte.
   Notes:  This function is generally used for interfacing with BBS types that
           use Pascal strings in their data files.
*/
XINT build_pascal_string(unsigned char *buffer)
{
XINT tl; /* Temporary length holder. */

/* Get the length. */
tl = strlen(buffer);

/* Do nothing if the length is 0. */
if (tl == 0)
    {
    return 0;
    }

/* Move the entire string (except the terminating \0) over to the right one
   byte.  Borland's memmove() can do this safely. */
memmove(&buffer[1], &buffer[0], tl);

/* Make the first byte the length byte. */
buffer[0] = tl;

/* Pascal strings cannot be more than 255 characters long. */
if (tl > 255)
    {
    buffer[0] = 255;
    }

return buffer[0];
}

/* This function is intended to get a string using full-screen editing
   capabilities, but there can be cosmetic complications if the user is not
   using ANSI/AVATAR and full-screen editing facilities cannot be provided,
   so it was never used. */
/*void get_string(char *is, char *fs, XINT ro, XINT co, unsigned char nc,
                unsigned char hc, char ch, unsigned XINT fl, XINT ml, char lch,
				char hch)
{

if (od_control.user_ansi || od_control.user_avatar)
	{
    od_edit_str(is, fs, ro, co, nc, hc, ch, fl);
    }
else
	{
    od_input_str(is, ml, lch, hch);
    }

return;
}*/

/* checkcmdmatch() - Compare a string to a list of command words.
   Parameters:  instring - String being checked.
                cmdstring - List of command words to check (see notes).
   Returns:  If the string is a command (i.e. the string matches a
             word in cmdstring), the length of the command is returned.
             Otherwise, 0 is returned.
   Notes:  cmdstring should be a list of command words separated by spaces.
           This function is usually used in conjunction with a single
           word and not an entire string.  The return value can be tested
           as a simple boolean case (TRUE or FALSE).  This function stores
           the actual command matched inside the global variable lastcmd.
*/
char checkcmdmatch(unsigned char *instring, unsigned char *cmdstring)
{
XINT nw; /* Number of words. */
XINT ccd; /* Counter. */
char retlen; /* Length of command to return. */
/* word_str preservation buffer, input string buffer. */
unsigned char *tmpstore = NULL, *inpstring = NULL;
XINT ccres; /* Result code. */

/* Don't process if a 0-length string is encountered. */
if (!instring[0] || !cmdstring[0])
    {
    return 0;
    }

tmpstore = malloc(strlen(word_str) + 1);
if (!tmpstore)
	{
    return 0;
    }
inpstring = malloc(MAXSTRLEN + 1);
if (!inpstring)
	{
    dofree(tmpstore);
    return 0;
    }

/* The state of the word splitter must be preserved because this function
   is usually called when processing individual words.  This is done by
   storing the current contents of the global variable word_str. */
strcpy(tmpstore, word_str);
/* The string being tested is copied in case it points to somewhere
   inside word_str. */
strcpy(inpstring, instring);

/* Split the command list into words. */
nw = split_string(cmdstring);

/* Loop to test each command word. */
for (ccd = 0, retlen = 0; ccd < nw; ccd++)
	{
    /* Commands are always case-insensitive. */
    if (!stricmp(get_word(ccd), inpstring))
        {
        /* Only consider a match if this command is longer than any
           previous matches. */
        if (strlen(get_word(ccd)) > retlen)
            {
            retlen = strlen(get_word(ccd));
            strcpy(lastcmd, get_word(ccd));
            }
        /* Processing continues even after a successful find because
           there may be longer commands that start with the same letters.
           It is best to find the longest command that matches. */
        }
    }

/* Restore the original state of the word splitter by resplitting the
   old word_str being stored. */
split_string(tmpstore);

dofree(tmpstore); dofree(inpstring);

return retlen;
}

