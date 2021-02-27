/******************************************************************************
CENSOR.C     Profanity censor functions.

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

This module contains the profanity censor.
******************************************************************************/

#include "top.h"

/* load_censor_words() - Loads words to censor from CENSOR.CFG.
   Parameters:  None.
   Returns:  TRUE on success, FALSE if an error occurred.
*/
char load_censor_words(void)
    {
    FILE *fil; /* CENSOR.CFG file stream. */
    XINT d, count = 0; /* Loop counter, number of words loaded. */
    char loadstr[256]; /* Loading buffer. */

    numcensorwords = 0;

    /* Open the file. */
    sprintf(outbuf, "%scensor.cfg", cfg.toppath);
    fil = fopen(outbuf, "rt");
    if (fil == NULL)
        {
        return 0;
        }

    /* Count the number of non-comment lines in the file to get an
       approximate estimation of the number of words. */
    rewind(fil);
    while(!feof(fil))
        {
        fgets(loadstr, 256, fil);
        if (loadstr[0] != ';')
            {
            count++;
            }
        }

    /* Round up to the nearest multiple of three, then divide the counted
       number of lines by three, because there are three lines per word
       definition. */
    count += (count % 3);
    count /= 3;

    /* Allocate memory for the counted number of definition. */
    cwords = calloc(count, sizeof(censor_word_typ));
    if (cwords == NULL)
        {
        fclose(fil);
        return 0;
        }

    /* Load and process each line. */
    rewind(fil);
    for (d = 0; d < count; d++)
        {
        /* Keep reading until a non-comment line is found. */
        do
            {
            fgets(loadstr, 256, fil);
            }
        while(loadstr[0] == ';');

        /* Set the whole-word flag and level.  See CENSOR.CFG comments for
           more information. */
        if (toupper(loadstr[0]) == 'W')
            {
            cwords[d].wholeword = 1;
            cwords[d].level = atoi(&loadstr[1]);
            }
        else
            {
            cwords[d].wholeword = 0;
            cwords[d].level = atoi(loadstr);
            }

        /* Load the word to be censored. */
        fgets(loadstr, 256, fil);
        stripcr(loadstr);
        /* All censor text is dynamically allocated to save space. */
        cwords[d].word = malloc(strlen(loadstr) + 1);
        if (cwords[d].word == NULL)
            {
            fclose(fil);
            return 0;
            }
        strcpy(cwords[d].word, loadstr);

        /* Load the replacement text, if any. */
        fgets(loadstr, 256, fil);
        stripcr(loadstr);
        cwords[d].changeto = malloc(strlen(loadstr) + 1);
        if (cwords[d].changeto == NULL)
            {
            fclose(fil);
            return 0;
            }
        strcpy(cwords[d].changeto, loadstr);
        }

    fclose(fil);

    /* Set the global censor word counter. */
    numcensorwords = count;

    return 1;
    }

/* censorinput() - Censors a string of text.
   Parameters:  str - String containing text to be censored, and buffer to
                      hold the new, possibly modified string.
   Returns:  TRUE if the string should be blocked, FALSE if the string is
             permitted.
   Notes:  Changed text is not considered in the return code.  So long as
           the final string is displayable, even if the censor has changed
           the contents, it will return TRUE.  str must be at least
           MAXSTRLEN+1 characters long.
*/
char censorinput(char *str)
    {
    /* Counter, current character, current newstring character, current
       unfiltered string character, word replacement flag. */
    XINT d, p = 0, np = 0, fp = 0, flg;
    /* New string, filtered string. */
    char newstr[512], filtstr[512];

    /* The censor uses three strings.  filtstr contains a colour-code
       filtered copy of the string and is used for word detection.  newstr
       contains the new string including colour codes and modified text.
       str is used as the actual source.  The reason the filtered string
       cannot be used is because the colour codes still need to be
       displayed later. */

    /* Initialize the strings. */
    filter_string(filtstr, str);
    newstr[0] = '\0';
    
    /* Loop for each character in the filtered string.  The censor tests
       for offensive text beginning at every position in the string. */
    for (p = 0; p < strlen(filtstr); p++)
        {
        /* Loop through known offensive text. */
        for (d = 0, flg = 0; d < numcensorwords; d++)
            {
            /* Only test if this text is not longer than the remainder of
               the string to be tested. */
            if (strlen(&filtstr[p]) >= strlen(cwords[d].word))
                {
                /* Look for the text starting at the current position. */
                if (!strnicmp(&filtstr[p], cwords[d].word,
                              strlen(cwords[d].word)))
                    {
                    if (cwords[d].wholeword)
                        {
                        /* The criteria for a "whole word" is if there is
                           a whitespace character before and after the word.
                           The start and end of the string is considered
                           whitespace. */
                        if (p > 0)
                            {
                            if (isalpha(str[p - 1]))
                                {
                                /* An alpha character was found preceding
                                   the text, don't censor. */
                                continue;
                                }
                            }
                        if (isalpha(filtstr[p + strlen(cwords[d].word)]))
                            {
                            /* An alpha character was found after the
                               text, don't censor. */
                            continue;
                            }
                        }

                    /* If we've gotten to this point, the censor found a
                       match and needs to respond. */
                    switch(cwords[d].level)
                        {
                        /* Level 1:  Immediate disconnection. */
                        case 1:
                            top_output(OUT_SCREEN, getlang("CensorToss1"),
                                       cwords[d].word);
/* od_sleep() uses seconds in OS/2, milliseconds in DOS/Win32. */
#ifdef __OS2__
                            od_sleep(1);
#else
                            od_sleep(1000);
#endif
                            od_exit(10, TRUE);
                            /* This line is never reached but included for
                               completeness. */
                            return 1;
                        /* Level 2 - Warn, toss if applicable, don't allow. */
                        case 2:
                            /* Warn the user. */
                            top_output(OUT_SCREEN, getlang("CensorWarn2"),
                                       cwords[d].word);
                            /* Reset the high warning counter and timer. */
                            censorwarningshigh++;
                            lastcensorwarnhigh = time(NULL);
                            /* Do a warning check, toss if too many. */
                            if (censorwarningshigh >= cfg.maxcensorwarnhigh)
                                {
                                top_output(OUT_SCREEN, getlang("CensorToss2"),
                                           cwords[d].word);
#ifdef __OS2__
                                od_sleep(1);
#else
                                od_sleep(1000);
#endif
                                od_exit(10, TRUE);
                                }
                            /* Block the text. */
                            return 1;
                        /* Level 3 - Warn without tossing, don't allow. */
                        case 3:
                            /* Warn the user. */
                            top_output(OUT_SCREEN, getlang("CensorWarn3"),
                                       cwords[d].word);
                            /* Reset the high warning counter and timer. */
                            censorwarningshigh++;
                            lastcensorwarnhigh = time(NULL);
                            /* Block the text. */
                            return 1;
                        /* Level 4 - Warn, toss if applicable, allow. */
                        case 4:
                            /* Warn the user. */
                            top_output(OUT_SCREEN, getlang("CensorWarn4"),
                                       cwords[d].word);
                            /* Reset the low warning counter and timer. */
                            censorwarningslow++;
                            lastcensorwarnlow = time(NULL);
                            /* Do a warning check, toss if too many. */
                            if (censorwarningslow >= cfg.maxcensorwarnlow)
                                {
                                top_output(OUT_SCREEN, getlang("CensorToss4"),
                                           cwords[d].word);
#ifdef __OS2__
                                od_sleep(1);
#else
                                od_sleep(1000);
#endif
                                od_exit(10, TRUE);
                                }
                            /* The text will still be allowed. */
                            break;
                        /* Warn, don't toss, allow. */
                        case 5:
                            /* Warn the user. */
                            top_output(OUT_SCREEN, getlang("CensorWarn5"),
                                       cwords[d].word);
                            /* Reset the low warning counter and timer. */
                            censorwarningslow++;
                            lastcensorwarnlow = time(NULL);
                            /* The text will still be allowed. */
                            break;
                        }

                    /* Change the text if there is replacement text
                       available.  Note that this can occur even if one
                       of the actions above was not taken.  This is
                       known as level 6 of the censor (replace only). */
                    if (cwords[d].changeto[0])
                        {
                        XINT e; /* Word position counter. */

                        /* Flag that the text is being replaced. */
                        flg = 1;
                        /* Skip any colour codes that preceed the text to
                           be replaced.  The colour codes are still copied
                           into the new string. */
                        while(str[fp] == '^')
                            {
                            newstr[np++] = str[fp++];
                            newstr[np++] = str[fp++];
                            newstr[np] = '\0';
                            }
                        /* Append the replacement text and reset the
                           newstring pointer. */
                        strcat(newstr, cwords[d].changeto);
                        np = strlen(newstr);
                        /* Advance the copy pointer past the old text.  It
                           is assumed that the new text won't need to be
                           censored. */
                        p += (strlen(cwords[d].word) - 1);
                        /* Advance the unfiltered pointer past the old
                           text. */
                        for (e = 0; e < strlen(cwords[d].word); e++)
                            {
                            fp++;
                            }
                        /* Notify the user of the replacement. */
                        top_output(OUT_SCREEN, getlang("CensorReplace"));
                        /* Break out of the word-check loop to begin
                           testing the next position. */
                        break;
                        }
                    }
                }
            }

        /* Only copy the text if the text replacer didn't do it above. */
        if (!flg)
            {
            /* Skip any colour codes that preceed the text to
               be replaced.  The colour codes are still copied
               into the new string. */
            while(str[fp] == '^')
                {
                newstr[np++] = str[fp++];
                newstr[np++] = str[fp++];
                newstr[np] = '\0';
                }
            /* Copy this character into the new string. */
            newstr[np++] = str[fp++];
            newstr[np] = '\0';
            }
        }

    /* Copy the new string to the original buffer.  The string is truncated
       if it is longer than MAXSTRLEN because TOP's message routines can't
       handle larger strings. */
    memset(str, 0, MAXSTRLEN + 1);
    strncpy(str, newstr, MAXSTRLEN);

    /* The string was permitted. */
    return 0;
    }

