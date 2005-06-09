/******************************************************************************
WORDS.C      Word splitter.

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

This module contains the word splitter, which splits a string into words
based on whitespace separation.
******************************************************************************/

#include "top.h"

/* split_string() - Splits a string into words.
   Parameters:  lin - String (or line) of text to separate.
   Returns:  Number of words in the separated string.
   Notes:  Although this function is little more than a simple string-to-
           word splitter, it forms the core of the TOP command processor so
           it should be handled with care.  The split string (that is, lin)
           is copied to the global variable word_str for storage.  The
           word_pos and word_len global variables are also loaded with
           data related to the split string.
*/
XINT split_string(unsigned char *lin)
{
/* Word counter, string position counter, word length counter. */
XINT wc = 0, pc = 0, lc = 0;

do
	{
    /* Skip extra whitespace that might precede the word. */
    if (iswhite(lin[pc]))
        {
    	pc++;
        }
    else
    	{
        lc = 0;
        /* Note the start of the word. */
        word_pos[wc] = pc;
        /* Count off letters until whitespace is encountered. */
        do
        	{
            lc++; pc++;
            }
        while(!iswhite(lin[pc]));
        /* Note the length of the word. */
        word_len[wc] = lc;
		wc++;
        }
    }
while(pc < strlen(lin));

/* The full text of the string is stored in the global variable
   word_str, which is later used by the get_word() function to actually
   get the word.  This storage method has the additional benefit of
   allowing direct access to all the text starting at a particular word,
   which is ideal for whispers, talktypes, and similar commands. */
memset(word_str, 0, MAXSTRLEN + 1);
strncpy(word_str, lin, MAXSTRLEN);

/* Zero out the lengths for unused words. */
for (pc = wc; pc < MAXWORDS; pc++)
	{
    word_pos[pc] = strlen(lin);
    word_len[pc] = 0;
    }

return wc;
}

/* get_word() - Gets a word from a previously split string.
   Parameters:  wordnum - Number of word to get (0-based).
   Returns:  Pointer to the global variable wordret, which contains the
             retreived word.
*/
char XFAR *get_word(XINT wordnum)
{

memset(wordret, 0, word_len[wordnum] + 1L);
strncpy(wordret, &word_str[word_pos[wordnum]], word_len[wordnum]);

return wordret;
}
