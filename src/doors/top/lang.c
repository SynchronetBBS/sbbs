/******************************************************************************
LANG.C       Language file functions.

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

This module contains all functions related to the language file.
******************************************************************************/

#include "top.h"

/* Memory consumption while the language file is loading is logged. */
extern FILE *debuglog;

/* loadlang() - Load the language file.
   Parameters:  None.
   Returns:  TRUE if successful, FALSE on error.
   Notes:  The name of the language file is obtained from TOP.CFG.
*/
char loadlang(void)
{
FILE *langfil = NULL; /* File stream for loading the language file. */
XINT lres; /* Result code. */
long linecount = 0, lld; /* Number of lines, counter. */

/* Open the language file. */
sprintf(outbuf, "%s%s", cfg.toppath, cfg.langfile);
langfil = fopen(outbuf, "rt");
if (!langfil)
	{
    errorabort(ERR_CANTOPEN, cfg.langfile);
    }

/* Count the number of usable lines (items) in the file. */
while(!feof(langfil))
	{
    if (fgets(outbuf, 256, langfil))
    	{
        /* Only count non-comment lines. */
        if (outbuf[0] != ';')
            {
            linecount++;
            }
        }
    }

/* Allocate the pointer buffer for the language items. */
numlang = linecount;
langptrs = calloc(linecount, sizeof(lang_text_typ XFAR *));
if (!langptrs)
	{
    fclose(langfil);
    return 0;
    }

if (debuglog != NULL)
    {
    printf("\n\nLanguage File Free Memory Check:\n\n");
    }

/* Allocate space for each language item. */
for (lld = 0, lres = 0; lld < linecount; lld++)
	{
    lres |= ((langptrs[lld] = malloc(sizeof(lang_text_typ))) == NULL);
#if !defined(__OS2__) && !defined(__WIN32__) && !defined(__unix__)
    /* Log the free memory if DEBUG mode is on. */
    if (debuglog != NULL)
        {
        printf("/%lu", farcoreleft());
        }
#endif
    }

if (lres)
    {
    /* On error, free all pointers. */
    for (lld = 0; lld < linecount; lld++)
    	{
        dofree(langptrs[lld]);
        }
    dofree(langptrs);
    fclose(langfil);
    return 0;
    }

/* Load each language item. */
rewind(langfil);
for (lld = 0; lld < linecount; lld++)
	{
    // Some kind of extra errcheck here.
    fgets(outbuf, 256, langfil);
    if (outbuf[0] == ';')
    	{
        /* Skip comments. */
        lld--;
        continue;
        }
    /* Remove trailing newline.  Not sure why the stripcr() macro wasn't
       used. */
    if (outbuf[strlen(outbuf) - 1] == '\n')
    	{
        outbuf[strlen(outbuf) - 1] = 0;
        }
    /* Break the string to get the name and the rest of the text. */
    lres = split_string(outbuf);
    if (lres > 0)
    	{
        /* Copy the item name. */
        memset(langptrs[lld], 0, sizeof(lang_text_typ));
        strncpy(langptrs[lld]->idtag, get_word(0), 30);
        /* Get the length of the text, which starts at the second word. */
        langptrs[lld]->length = strlen(&word_str[word_pos[1]]);
        /* Allocate space for the text. */
        langptrs[lld]->string =
            malloc(langptrs[lld]->length + 1);
        if (!langptrs[lld]->string)
        	{
            /* Free all memory on error. */
		    for (lld = 0; lld < linecount; lld++)
		    	{
		        dofree(langptrs[lld]);
		        }
		    dofree(langptrs);
		    fclose(langfil);
            return 0;
            }
        /* Copy the string text. */
        strcpy(langptrs[lld]->string, &word_str[word_pos[1]]);
        }
    }

fclose(langfil);

return 1;
}

/* getlang() - Gets a language text item from memory.
   Parameters:  langname - ID tag (name) of the item to get.
   Returns:  String containing the text.
*/
unsigned char XFAR *getlang(unsigned char *langname)
{
long gld; /* Counter. */

/* Iterate through each item. */
for (gld = 0; gld < numlang; gld++)
	{
    /* The ID tags have to match exactly, except for case. */
    if (!stricmp(langname, langptrs[gld]->idtag))
    	{
        /* Return pointer to the language text. */
        return langptrs[gld]->string;
        }
    }

/* Return a blank string because the item wasn't found. */
return "\0";
}

/* getlangchar() - Gets a single character from a language item.
   Parameters:  langname - ID tag (name) of item to use.
                charnum - Character number (0-based) to get.
   Returns:  Character in the specified position, or 0 if the item was not
             found.
*/
unsigned char getlangchar(unsigned char *langname, XINT charnum)
{
long gld; /* Counter. */

/* Iterate through each item. */
for (gld = 0; gld < numlang; gld++)
	{
    /* The ID tags have to match exactly, except for case. */
    if (!stricmp(langname, langptrs[gld]->idtag))
    	{
        /* Return character in the specified position. */
        return langptrs[gld]->string[charnum];
        }
    }

/* Return ASCII #0, no item was found. */
return 0;
}

