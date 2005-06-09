/******************************************************************************
OUTPUT.C     Screen output function.

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

This module contains TOP's custom screen output function, which is capable
of processing PubColour codes and language tokens.  It is also capable of
outputting to strings for log output or message sending.
******************************************************************************/

#include "top.h"

/* top_output() - Screen output function.
   Parameters:  outmode - Output mode (OUT_ constant).
                string - String to output.
                ... - Language item parameter(s) (see notes).
   Returns:  Pointer to global outputstr variable.
   Notes:  This is a variable-parameter function.  After string (the main
           output string, up to 9 additional strings may be specified.
           These will be inserted into the output wherever language
           parameter tokens (@1, @2, etc.) appear.  They may not be used
           at all depending on the contents of string.  Integers or other
           number CANNOT BE SPECIFIED!  To include an int as a parameter,
           convert it with itoa() or a similar function.  The global array
           of outnum strings can be used to hold these conversions without
           having to allocate extra space.  Only two parameters are
           required.  Although this function always returns a pointer to
           outputstr, outputstr will only be updated with the new output if
           OUT_STRING or OUT_STRINGNF modes were specified.  This function
           uses the outproc... global flags to control code output.
*/
unsigned char *top_output(char outmode, unsigned char *string, ...)
{
va_list oap; /* System pointer to variable arguments. */
unsigned char *argptrs[9] = /* Variable argument holders. */
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
/* Counter, number of variable arguments, argument number holder. */
char odd, nargs, argnum;
/* Counter, counter, word wrap counter, memory/length counter, temporary
   attribute holder. */
XINT d, e, owwc, uu, tatr;
unsigned long lentmp; /* Temporary length holder. */
unsigned char oww[55]; /* Output word wrap buffer. */
unsigned char *lenbuf = NULL; /* Parameter length restriction buffer. */
XINT tmulti; /* Not used. */
#ifndef __unix__
struct text_info ti; /* Screen attribute information. */
#endif
/* OUT_STRINGNF mode flag, OUT_EMULATE mode flag. */
char stringnf = 0, emu = 0;

/* This is a complicated and extemely lengthy function that should not be
   messed with unless you have a thorough understanding of all of TOP's
   codes, the door kit output functions, and string and memory handling. */

/* OUT_STRINGNF functions identically to OUT_STRING in most places so
   a flag is set and OUT_STRING mode is forced. */
if (outmode == OUT_STRINGNF)
	{
    stringnf = 1;
    outmode = OUT_STRING;
    }
/* OUT_EMULATE functions identically to OUT_SCREEN in most places so
   a flag is set and OUT_SCREEN mode is forced. */
if (outmode == OUT_EMULATE)
	{
    emu = 1;
    outmode = OUT_SCREEN;
    }

/* Initialize the argument holders. */
for (d = 0; d < 9; d++)
    {
    argptrs[d] = NULL;
    }

/* This loop ascertains the highest number argument the output string
   requires. */
for (d = 0, nargs = 0; d < strlen(string); d++)
	{
    /* Look for @# codes that are higher than the current highest known
       code to exist. */
    if (string[d] == '@' && isdigit(string[d + 1]) &&
    	(string[d + 1] - '0' > nargs))
    	{
        /* Set the new high arg number. */
        nargs = string[d + 1] - '0';
        }
    }

/* Grab all of the arguments. */
va_start(oap, string);
for (odd = 0; odd < nargs; odd++)
	{
    argptrs[odd] = va_arg(oap, unsigned char *);
    }
va_end(oap);

#ifndef __unix__
/* Clear the word wrap buffer if a screen mode or local mode is being used. */
if (outmode == OUT_SCREEN || outmode == OUT_LOCAL)
	{
    memset(oww, 0, 55);
    }
#endif
/* Clear the output string if a string mode is being used. */
if (outmode == OUT_STRING)
	{
    memset(outputstr, 0, 513);
    }

/* Loop through and process the entire string. */
for (d = 0, owwc = 0; d < strlen(string); d++)
	{
    /* PubColour codes.  Only process if the global flag is on. */
    if (string[d] == '^' && outproccol)
    	{
        /* Copy the ^ character in non-filtered mode. */
        if (outmode == OUT_STRING && stringnf)
        	{
            strncat(outputstr, &string[d], 1);
            }
        /* We're now working with the character after the ^. */
        d++;
        /* Foreground colour change (lower case). */
        if (string[d] >= 'a' && string[d] <= 'p')
        	{
            /* Change the screen colour in screen mode, preserving the
               current background. */
            if (outmode == OUT_SCREEN)
            	{
                od_set_attrib((od_control.od_cur_attrib & 0xF0) +
							  (string[d] - 'a'));
                }
#ifndef __unix__
            /* Force the attribute colour in local mode, preserving the
               current background. */
            if (outmode == OUT_LOCAL)
            	{
                gettextinfo(&ti);
                textattr((ti.attribute & 0xF0) +
						 (string[d] - 'a'));
                }
#endif
            /* Copy the code in non-filtered mode. */
            if (outmode == OUT_STRING && stringnf)
        		{
    	        strncat(outputstr, &string[d], 1);
	            }
            continue;
            }
        /* Background colour change (upper case). */
        if (string[d] >= 'A' && string[d] <= 'P')
            {
            /* Change the screen colour in screen mode, preserving the
               current foreground. */
            if (outmode == OUT_SCREEN)
            	{
            	od_set_attrib((od_control.od_cur_attrib & 0x0F) +
							  ((string[d] - 'A') << 4));
                }
#ifndef __unix__
            /* Force the attribute colour in local mode, preserving the
               current foreground. */
            if (outmode == OUT_LOCAL)
            	{
                gettextinfo(&ti);
            	textattr((ti.attribute & 0x0F) +
						 ((string[d] - 'A') << 4));
                }
#endif
            /* Copy the code in non-filtered mode. */
            if (outmode == OUT_STRING && stringnf)
        		{
    	        strncat(outputstr, &string[d], 1);
	            }
            continue;
            }
        if (string[d] != '^')
        	{
            /* Any character but another ^ forces the default attribute.
               The default attribute is not configurable at this time. */
            if (outmode == OUT_SCREEN)
            	{
                od_set_attrib(outdefattrib);
                }
            continue;
            }
        /* A second ^ passes through to be displayed to the screen. */
        }
    /* Language codes.  Only process if the global flag is on. */
    if (string[d] == '@' && outproclang)
    	{
        /* We're now working with the character after the @. */
        d++;
        /* Language parameter. */
        if (string[d] >= '1' && string[d] <= '9')
        	{
            /* Grab the requested parameter number. */
            argnum = string[d] - '1';
            /* By definition argnum will be less than nargs because we
               tested all @# codes above, but it's checked in case. */
            if (argnum < nargs)
                {
                /* Process all following & codes which control code
                   processing during parameter display. */
                while(string[d + 1] == '&' && d < strlen(string))
                    {
                    /* Jump to the character after the &. */
                    d += 2;
                    /* String modes copy the & code directly. */
                    if (outmode == OUT_STRING)
                        {
                        char ttss[5];
                        sprintf(ttss, "%c%c", string[d - 1], string[d]);
                        strcat(outputstr, ttss);
                        }
                    else
                        {
                        /* Action code disable (not currently used in this
                           function). */
                        if (toupper(string[d]) == 'A') outprocact = FALSE;
                        /* Language code disable. */
                        if (toupper(string[d]) == 'L') outproclang = FALSE;
                        /* PubColour code disable. */
                        if (toupper(string[d]) == 'C') outproccol = FALSE;
                        }
                    }

                /* Find out if the language item calls for a fixed or
                   limited length. */
                lentmp = outchecksuffix(&string[d + 1]);
                if (lentmp > 0L)
                	{
                    /* Length wanted. */
                    unsigned XINT lenwant = lentmp & 0x000000FFL;
                    XINT xmem = 0; /* Extra memory counter. */

                    /* Loop through the parameter string and count all of
                       the colour codes, adding 2 extra memory bytes for
                       each. */
                    for (uu = 0; uu < strlen(argptrs[argnum]);
                         uu++)
                        {
                        if (argptrs[argnum][uu] == '^')
                            {
                            /* Skip to the character after the ^. */
                            uu++;
                            xmem += 2;
                            }
                        }

                    /* Increase the size of the wanted length. */
                    lenwant += xmem;
                    /* Grab the requested length of memory. */
                    lenbuf = malloc(lenwant + 1);
                    if (lenbuf)
                    	{
                        /* Clear the temporary buffer.  It's filled with
                           zeros if that was requested, otherwise spaces
                           are used. */
                        if (lentmp & 0x00020000L)
                        	{
                            memset(lenbuf, '0', lenwant);
                            }
                        else
                        	{
                            memset(lenbuf, ' ', lenwant);
                            }
                        lenbuf[lenwant] = '\0';
                        if (lentmp & 0x00010000L)
                        	{
                            /* Left justification. */
                            if (strlen(argptrs[argnum]) >
                                lenwant)
                                {
                                /* Truncate the parameter string if it is
                                   too long. */
                                memcpy(lenbuf, argptrs[argnum],
                                       lenwant);
                                }
                            else
                                {
                                /* Copy the string in its entirety.
                                   memcpy() is used because we don't want a
                                   \0 after the string is copied. */
                                memcpy(lenbuf, argptrs[argnum],
                                       strlen(argptrs[argnum]));
                                }
                            }
                        else
                        	{
                            /* Right justification. */
                            if (strlen(argptrs[argnum]) >
/* 7 */                         lenwant)
                                {
                                /* Truncate the parameter string if it is
                                   too long.  No compensation for right
                                   justification is needed since the
                                   parameter will fill the entire buffer. */
                                memcpy(lenbuf, argptrs[argnum],
                                       lenwant);
                                }
                            else
                                {
                                /* Copy the entire string right justified. */
                                memcpy(&lenbuf[lenwant -
                                       strlen(argptrs[argnum])],
                                       argptrs[argnum],
                                       strlen(argptrs[argnum]));
                                }
                            }
                        /* Screen and local modes make a recursive call to
                           top_output() to output the parameter. */
                        if (outmode == OUT_SCREEN)
							{
							top_output(OUT_SCREEN, lenbuf);
                            }
#ifndef __unix__
                        if (outmode == OUT_LOCAL)
							{
							top_output(OUT_LOCAL, lenbuf);
                            }
#endif
                        /* String modes copy the parameter directly. */
                        if (outmode == OUT_STRING)
                        	{
                            strcat(outputstr, lenbuf);
                            }
                        dofree(lenbuf);
                        }
                    /* Increment the counter the length of the actual
                       parameter code. */
                    d += ((lentmp & 0xFF00) >> 8);
                    /* Reactivate all code processing flags. */
                    outproccol = outproclang = outprocact = TRUE;
                    continue;
                	}
                else
                	{
                    /* User doesn't care about length. */

                    /* Screen and local modes make a recursive call to
                       top_output() to output the parameter. */
                    if (outmode == OUT_SCREEN)
                    	{
                        top_output(OUT_SCREEN, argptrs[argnum]);
                        }
#ifndef __unix__
                    if (outmode == OUT_LOCAL)
                    	{
                        top_output(OUT_LOCAL, argptrs[argnum]);
                        }
#endif
                    /* String modes copy the parameter directly. */
                    if (outmode == OUT_STRING)
                    	{
                        strcat(outputstr, argptrs[argnum]);
                        }
                    /* Reactivate all code processing flags. */
                    outproccol = outproclang = outprocact = TRUE;
                    continue;
                    }
                }
            else
            	{
                /* Skip invalid parameter numbers. */
                continue;
                }
            }
        /* Destructive backspace. */
        if (toupper(string[d]) == '<')
            {
            /* Screen and local modes display a destructive backspace. */
            if (outmode == OUT_SCREEN)
                {
                od_disp_str("\b \b");
                }
#ifndef __unix__
            if (outmode == OUT_LOCAL)
                {
                cputs("\b \b");
                }
#endif
            /* String modes just copy the backspace, assuming it will
               be properly handled later by whatever uses it. */
            if (outmode == OUT_STRING)
                {
                strcat(outputstr, "\b");
                }
            continue;
            }
        /* Beep. */
        if (toupper(string[d]) == 'B')
        	{
            char locecho = 0; /* Whether to echo beep locally. */

            /* Screen modes use the configured setting to determine if the
               beep should be sounded on the local console. */
            if (outmode == OUT_SCREEN)
            	{
                /* Remote node type. */
                if (!localmode && !lanmode && od_control.baud != 0)
                    {
                    locecho = (cfg.localbeeping & BEEP_REMOTE);
                    }
                /* Local node type, or a local user on any type of
                   node. */
                if (localmode || od_control.baud == 0)
                    {
                    locecho = (cfg.localbeeping & BEEP_LOCAL);
                    }
                /* LAN node type. */
                if (lanmode)
                    {
                    locecho = (cfg.localbeeping & BEEP_LAN);
                    }
                /* Send the beep to the user, echoing locally if
                   requested. */
                od_disp("\a", 1, locecho);
                }
            /* Local mode always beeps on the local console, as there is
               nowhere else to beep to! */
#ifndef __unix__
            if (outmode == OUT_LOCAL)
            	{
                cputs("\a");
                }
#endif
            /* String modes copy the beep directly. */
            if (outmode == OUT_STRING)
            	{
                strcat(outputstr, "\a");
                }
            continue;
            }
        /* CRLF (carriage return and line feed. */
        if (toupper(string[d]) == 'C')
        	{
            /* All modes output or copy an ASCII 13 and 10 pair. */
            if (outmode == OUT_SCREEN)
            	{
                top_output(OUT_SCREEN, "\r\n");
                }
#ifndef __unix__
            if (outmode == OUT_LOCAL)
            	{
                top_output(OUT_LOCAL, "\r\n");
                }
#endif
            if (outmode == OUT_STRING)
            	{
                strcat(outputstr, "\r\n");
                }
            continue;
            }
        /* Delete to end of line.  Mostly used in dual-window chat mode. */
        if (toupper(string[d]) == 'D')
            {
            /* Screen and local modes clear to end of line. */
            if (outmode == OUT_SCREEN)
                {
                od_clr_line();
                }
#ifndef __unix__
            if (outmode == OUT_LOCAL)
                {
                clreol();
                }
#endif
            /* String modes ignore this code at this time. */
            if (outmode == OUT_STRING) // Support this later
                {
                }
            continue;
            }
        /* Erase screen. */
        if (toupper(string[d]) == 'E')
            {
            /* Screen and local modes clear the screen. */
            if (outmode == OUT_SCREEN)
                {
                od_clr_scr();
                }
#ifndef __unix__
            if (outmode == OUT_LOCAL)
                {
                clrscr();
                }
#endif
            /* String modes ignore this code at this time. */
            if (outmode == OUT_STRING) // Support this later
                {
                }
            continue;
            }
        /* Include language item. */
        if (toupper(string[d]) == 'L')
        	{
            /* Name end marker, name buffer. */
            unsigned char *lnmend = NULL, *lnm = NULL;

            /* Must have a quote mark right after the @L. */
            if (string[d + 1] != '\"')
            	{
                continue;
                }
            /* Must have a closing quote mark as well.  If it's found
               it is marked as the end for later use. */
            if ((lnmend = strchr(&string[d + 2], '\"')) == NULL)
            	{
                continue;
                }
            /* Allocate enough space for the name. */
            lnm = malloc(lnmend - &string[d + 1]);
        	if (!lnm)
            	{
                /* Ignore the code if the memory can't be gotten. */
                continue;
                }
            /* Clear the name buffer then copy the name, using pointer
               arithmetic. */
            memset(lnm, 0, (lnmend - &string[d + 1]));
            strncpy(lnm, &string[d + 2], (lnmend - &string[d + 2]));
            /* Screen and local modes use a recursive call to top_output()
               to display the language item.  Note that there is no way
               to provide the display with any parameters that might be
               needed, so only non-parameter codes should be used. */
            if (outmode == OUT_SCREEN)
            	{
	            top_output(OUT_SCREEN, getlang(lnm));
                }
#ifndef __unix__
            if (outmode == OUT_LOCAL)
            	{
                top_output(OUT_LOCAL, getlang(lnm));
                }
#endif
            /* String modes copy the item directly. */
            if (outmode == OUT_STRING)
            	{
	            strcat(outputstr, getlang(lnm));
                }
            /* Skip past the code and name. */
            d += (lnmend - &string[d]);
            dofree(lnm);
            continue;
            }
        /* Position cursor on screen.  Usually used with dual-window chat
           mode, and should only be used when ANSI or AVATAR mode is
           known. */
        if (toupper(string[d]) == 'P')
            {
            char txxx[5] = "\0\0\0\0"; /* Temporary position holder. */
            XINT txx, txy; /* X position, Y position. */

            /* Copy the X position then convert it to an int. */
            strncpy(txxx, &string[d + 1], 2);
            txy = atoi(txxx);
            /* Copy the Y position then convert it to an int. */
            strncpy(txxx, &string[d + 3], 2);
            txx = atoi(txxx); // Below also screen size checks
            if (txx < 1 || txy < 1 || txx > 80 || txy > 23)
                {
                /* Ignore invalid values. */
                continue;
                }
            /* Screen and local modes position the cursor. */
            if (outmode == OUT_SCREEN)
                {
                od_set_cursor(txy, txx);
                }
#ifndef __unix__
            if (outmode == OUT_LOCAL)
                {
                gotoxy(txx, txy);
                }
#endif
            /* String modes ignore this code at this time. */
            if (outmode == OUT_STRING) // Support this later
                {
                }
            d += 4;
            continue;
            }
        /* Repeat character. */
        if (toupper(string[d]) == 'R')
            {
            char txxx[5] = "\0\0\0\0"; /* Temporary repeat value holder. */

            /* Copy the repeat value, which is always 3 digits. */
            strncpy(txxx, &string[d + 1], 3);
            /* Skip ahead to the character to repeat. */
            d += 4;
            /* Screen modes use the door kit to repeat. */
            if (outmode == OUT_SCREEN)
                {
                od_repeat(string[d], atoi(txxx));
                }
            /* Local mode uses outputstr as a temporary holder for the
               repeated characters, then shows them directly. */
#ifndef __unix__
            if (outmode == OUT_LOCAL)
                {
                memset(outputstr, string[d], atoi(txxx));
                outputstr[atoi(txxx)] = '\0';
                cputs(outputstr);
                }
#endif
            if (outmode == OUT_STRING)
                {
                if (stringnf)
                    {
                    /* Non-filtered mode copies the entire code for later
                       use. */
                    strncat(outputstr, &string[d - 5], 5);
                    }
                else
                    {
                    /* Filtered mode appends the repeated characters to
                       outputstr. */
                    uu = strlen(outputstr);
                    memset(&outputstr[uu], string[d], atoi(txxx));
                    outputstr[uu + atoi(txxx)] = '\0';
                    }
                }
            continue;
            }
        /* Space character.  Generally used at the beginning or end of an
           item. */
        if (toupper(string[d]) == 'S')
        	{
            /* All modes simply show or copy a single space. */
            if (outmode == OUT_SCREEN)
            	{
	            top_output(OUT_SCREEN, " ");
                }
#ifndef __unix__
            if (outmode == OUT_LOCAL)
            	{
	            top_output(OUT_LOCAL, " ");
                }
#endif
            if (outmode == OUT_STRING)
            	{
                strcat(outputstr, " ");
                }
            continue;
            }
        /* Display any ASCII value. */
        if (toupper(string[d]) == 'X')
        	{
            char xxxn[4] = "\0\0\0\0"; /* Temporary code holder. */
            char xxxc[2] = "\0\0"; /* ASCII code number to display. */

            /* Copy the string representation of the code. */
            strncpy(xxxn, &string[d + 1], 3);
            /* Store the actual character in a small string. */
            xxxc[0] = atoi(xxxn);
            /* All modes display or copy the character directly. */
            if (outmode == OUT_SCREEN)
            	{
	            top_output(OUT_SCREEN, xxxc);
                }
#ifndef __unix__
            if (outmode == OUT_LOCAL)
            	{
                top_output(OUT_LOCAL, xxxc);
				}
#endif
            if (outmode == OUT_STRING)
            	{
                strcat(outputstr, xxxc);
                }
            /* Skip ahead to the code. */
            d += 3;
            continue;
            }
        }

    /* Actual backspace character, not handled in string modes. */
    if (string[d] == 8 && outmode != OUT_STRING)
    	{
        /* Screen and local modes display a desctructive backspace. */
        if (outmode == OUT_SCREEN)
        	{
            od_disp_str("\b \b");
            }
#ifndef __unix__
        if (outmode == OUT_LOCAL)
        	{
            cputs("\b \b");
            }
#endif
        /* Remove the last character from the word wrap buffer, if anything
           is in it. */
        if (owwc > 0)
            {
            oww[--owwc] = '\0';
            }
        continue;
        }
    /* Actual newline character, not handled in string modes. */
    if (string[d] == '\n' && outmode != OUT_STRING)
		{
        /* This was an attempt to bypass the well-known newline colour bar
           bug that occurs with ANSI.SYS, where the next line gets filled
           with the background colour whether we want it to be or not.  It
           doesn't work. */
        if (outmode == OUT_SCREEN)
    		{
        	tatr = od_control.od_cur_attrib;
	        od_set_attrib(0x07);
            }
#ifndef __unix__
	    if (outmode == OUT_LOCAL)
            {
            textcolor(0x07);
            }
#endif
        }
    /* End of special code processing. */

    /* Display the character in screen modes. */
    if (outmode == OUT_SCREEN)
    	{
        if (emu)
            {
            /* In emulate mode, use the door kit to handle raw ANSI, AVATAR,
               RIP, or RA codes. */
            od_emulate(string[d]);
            }
        else
            {
            /* In screen mode, dump the character to the screen directly,
               which is way faster. */
            od_putch(string[d]);
            }
        }
#ifndef __unix__
    /* Display the character in local mode. */
    if (outmode == OUT_LOCAL)
    	{
        putch(string[d]);
        }
#endif
    /* Copy the character in string modes. */
    if (outmode == OUT_STRING)
    	{
        strncat(outputstr, &string[d], 1);
        }

    /* End of the ANSI newline fix described above. */
    if (string[d] == '\n' && outmode == OUT_SCREEN)
    	{
        /* Restore the previous attribute. */
        if (outmode == OUT_SCREEN)
        	{
            od_set_attrib(tatr);
            }
#ifndef __unix__
        if (outmode == OUT_LOCAL)
        	{
            textattr(tatr);
            }
#endif
        }

    /* Word wrap all displayable characters. */
    if (string[d] >= 32 && string[d] <= 254 && outmode != OUT_STRING)
    	{
        /* Add the character to the word wrap buffer. */
        oww[owwc++] = string[d];
        /* Reset the buffer if a space, hyphen, or slash is encountered. */
        if (string[d] == ' ' || string[d] == '-' || string[d] == '/')
        	{
            owwc = 0;
            memset(oww, 0, 55);
            }
        /* Perform word wrapping if needed. */
        outwordwrap(outmode, &owwc, oww);
        }
	}

/* As mentioned, outputstr is always returned though it may not be updated
   with the new output. */
return outputstr;
}

/* outwordwrap() - Performs word wrap during output.
   Parameters:  omode - Output mode (OUT_ constant).
                wwwc - Pointer to word wrap counter.
                www - Word wrap string.
   Returns:  Nothing.
*/
void outwordwrap(XINT omode, XINT *wwwc, char *www)
{
XINT e; /* Counter. */
int xpos,ypos;

/* Only word wrap if the cursor is at the end of a line. */
od_get_cursor(&ypos,&xpos);
if (xpos == 80)
	{
    /* Only word wrap if the buffer is less than 50 characters.  Longer
       buffers are too long to be words so wrapping is pointless. */
    if (*wwwc < 50)
        {
        /* Backspace out the last word. */
        for (e = 0; e < *wwwc; e++)
            {
            if (omode == OUT_SCREEN)
            	{
                od_disp_str("\b \b");
                }
#ifndef __unix__
            if (omode == OUT_LOCAL)
            	{
                cputs("\b \b");
                }
#endif
            }
        }
    /* Go to the next line. */
    top_output(omode, "\r\n");
    /* Redisplay the word, only if it is short enough. */
    if (*wwwc < 50)
        {
        top_output(omode, www);
        }
    /* Reset the word wrap buffer and counter. */
    *wwwc = 0;
    memset(www, 0, 55);
    }

return;
}

/* outchecksuffix() - Process period modifier for output parameters.
   Parameters:  strstart - Start of string where the period is expected.
   Returns:  A dword that contains several bits of information.  See notes.
   Notes:  The return code takes the following format, in hex:
           abcdCCll
           The first four hex digits, abcd, are bitwise codes (4 in each
           digit) that indicate requested modes.  Only d is used right now.
           Bit one is set to indicate left justification.  Bit two is set
           if the string is to be filled with zeros.
           The next two hex digits, CC, contain the length of the processed
           code.
           The last two hex ditgits, ll, contain the length requested by
           the user.
*/
unsigned long outchecksuffix(unsigned char *strstart)
{
/* Desired length, code (string) length, start of length. */
XINT dlen, slen, numstart;
unsigned long outres = 0; /* Return code holder. */

/* As a sidebar, the reason all of the other language tokens use fixed-
   length numbers is because otherwise the large amount of processing done
   in this function would be needed for each code. */

/* Abort if no dot is found at the start of the string. */
if (strstart[0] != '.')
	{
    return 0x00000000L;
    }
/* Abort if the character after the dot is invalid (i.e. not a digit or
   dash. */
if (!isdigit(strstart[1]) && strstart[1] != '-')
	{
    return 0x00000000L;
    }

/* Initialize variables. */
slen = 1;
/* The length can only start at the second character or later.  Remember,
   numstart is holding a character index for a string so it is 0-based. */
numstart = 1;

/* Left justification. */
if (strstart[1] == '-')
	{
    /* Set the left justification flag. */
	outres += 0x00010000L;
    /* Increase code length. */
    slen++;
    /* The requested length now begins at the third character. */
    numstart = 2;
    }
/* Fill with zeros. */
if (strstart[1] == '0')
	{
    /* Set the zeros flag. */
    outres += 0x00020000L;
    /* The requested length now begins at the third character. */
    numstart = 2;
    }

/* Count the number of following digits, which make up the rest of the
   code. */
while(isdigit(strstart[slen]))
    {
    slen++;
    }

/* Grab the requested length from the string, starting at the appropriate
   character. */
dlen = atoi(&strstart[numstart]);

if (slen == 0 || dlen == 0)
	{
    /* Abort the whole procedure if a 0 length was given. */
    outres = 0x00000000L;
    }
else
	{
    /* Format the return code (see function notes). */
    outres += (slen << 8) + dlen;
    }

return outres;
}

