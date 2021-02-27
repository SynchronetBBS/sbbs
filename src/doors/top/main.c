/******************************************************************************
MAIN.C       Main loop and program entry point.

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

This module contains the program entry point (main() in DOS & OS/2, WinMain()
in Win32) and the main input loop.
******************************************************************************/

#include "top.h"

/* main() - Program entry point under DOS and OS/2.
   Parameters:  ac - Number of command line arguments.
                av - String array of command line arguments.
   Returns:  Nothing.
*/
/* WinMain() - Program entry point under Win32.
   Parameters:  hInstance - Handle of this instance of TOP.
                hPrevInstance - Handle of previous instance of TOP.
                lpszCmdParam - String of command line parameters.
                nCmdShow - Numeric code telling how TOP's window should
                           open (normal, minimized, etc.)
   Returns:  Program result code.
   Notes:  This function never returns.  exit() is always used to quit TOP.
*/
#ifndef __WIN32__
extern int main(XINT ac, char *av[])
#else
extern int pascal WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                          LPSTR lpszCmdParam, int nCmdShow)
#endif
{

/* This function is a little cluttered but it is manageable. */

#ifdef __WIN32__
/* TOP doesn't use any of the WinMain parameters. */
(void) hInstance;
(void) hPrevInstance;
(void) lpszCmdParam;
/* OpenDoors needs to know how to show the TOP window. */
od_control.od_cmd_show = nCmdShow;
/* Load TOP's custom icon. */
od_control.od_app_icon = LoadIcon(hInstance, "Icon");
/* Because init() needs a separated command line, it is passed the global
   command line variables provided by the Borland compiler. */
init(_argc, _argv);
#else
/* Initialize TOP. */
init(ac, av);
#endif
/* Show a title page if the option is turned on. */
if (cfg.showtitle)
	{
    show_file("toptitle", SCRN_ALL);
    }
/* Find and load the user, creating a new user if needed. */
search_for_user();
/* Show a news file if the option is on. */
if (cfg.shownews)
	{
    show_file("topnews", SCRN_ALL);
    }
/* Prepare the screen for dual window mode if the user has it turned on. */
if (user.pref1 & PREF1_DUALWINDOW)
	{
    top_output(OUT_SCREEN, getlang("DWOutputPrepare"));
    top_output(OUT_SCREEN, getlang("DWOutputSetCurPos"));
    }
/* Show the welcome. */
top_output(OUT_SCREEN, getlang("SitDown"));
/* Show who's in the channel and a basic help. */
channelsummary();
/* Let other users know we're here. */
dispatch_message(MSG_ENTRY, user.emessage, -1, 0, 0);
/* Store the screen location under dual window mode. */
if (user.pref1 & PREF1_DUALWINDOW)
	{
    // Different for AVT when I find out what the store/recv codes are.
    od_disp_emu("\x1B" "[s", TRUE);
    }
/* Run the main input loop forever. */
for (;;)
	{
	main_loop();
    }

}

/* In the main loop, DOS gets the current clock tick from the system to
   help it determine when to timeslice. */
#if !defined(__OS2__) && !defined(__WIN32__) && !defined(__unix__)
#define _clock_tick() (*(long XFAR *)0x46cL)
#endif

/* main_loop() - Main input loop.
   Parameters:  None.
   Returns:  Nothing.
*/
void main_loop(void)
{
XINT key, pos, wwc; /* Input key, string position, word wrap counter. */
unsigned char input[256], ww[81]; /* Input buffer, word wrap buffer. */
XINT tmp; /* Result code. */
#ifndef __OS2__
long clktick; /* Holds the clock tick under DOS. */
#endif
int	xpos,ypos;

/* Initialize variables. */
onflag = 0;
memset(input, 0, MAXSTRLEN + 1); memset(ww, 0, 81);
pos = -1; wwc = 0;
for(;;)
	{
#ifdef __OS2__
    /* Timeslice under OS/2. */
//  od_sleep(0);
    DosSleep(10);
#endif
#if !defined(__OS2__) && !defined(__WIN32__) && !defined(__unix__)
    /* Get the clock tick under DOS. */
    clktick = _clock_tick();
#endif
    /* Clear the key input holder. */
    key = '\0';
    /* Set/clear the onflag if the user has started typing. */
    if (strlen(input) > 0)
        {
        onflag = 1;
        }
    else
        {
        onflag = 0;
        }
    /* If the position is -1 then the screen needs to be initialized. */
    if (pos == -1)
    	{
        /* Clear/initalize the screen in dual window mode. */
        if (onflag && !(user.pref1 & PREF1_DUALWINDOW))
            {
            top_output(OUT_SCREEN, getlang("InputPrefix"));
            } // Also time, date, minleft to bottom
        /* More dual window mode preparation. */
        if (user.pref1 & PREF1_DUALWINDOW)
            {
            if (onflag)
                {
                /* Prepare the screen when the user is typing. */
                // Next four lines are here temporarily!
                // These will get replaced when proper scrolling is done
                top_output(OUT_SCREEN, getlang("DWPrepareInput"));
                top_output(OUT_SCREEN, getlang("DWTopDisplay"));
                top_output(OUT_SCREEN, getlang("DWMiddleDisplay"));
                top_output(OUT_SCREEN, getlang("DWBottomDisplay"));
                top_output(OUT_SCREEN, getlang("DWInputCurPos"));
                /* Prepares the scren to "retype" what the user has already
                   input. */
                top_output(OUT_SCREEN, getlang("DWRestoreInput"));
                }
            else
                {
                /* Prepare the scren when the user is idle. */
                // Next four lines are here temporarily!
                top_output(OUT_SCREEN, getlang("DWPrepareInput"));
                top_output(OUT_SCREEN, getlang("DWTopDisplay"));
                top_output(OUT_SCREEN, getlang("DWMiddleDisplay"));
                top_output(OUT_SCREEN, getlang("DWBottomDisplay"));
                top_output(OUT_SCREEN, getlang("DWInputCurPos"));
                }
            }
        else
            {
            /* Show the prompt in normal mode. */
            top_output(OUT_SCREEN, getlang("DefaultPrompt"), user.handle);
            }
        /* Clear the position counter. */
        pos = 0;
        if (onflag) // Temp for DW mode.
            {
            /* If the user has typing, "retype" what the user has already
               input. */
            pos = strlen(input);
            /* We don't want codes to be processed when retyping. */
            outproclang = FALSE; outproccol = FALSE;
            top_output(OUT_SCREEN, input);
            outproclang = TRUE; outproccol = TRUE;
            /* In dual window mode, clear the onflag.  This is done because
               during message processing, the onflag is used to display the
               interrupt string, but the interrupt string is unnecessary
               in dual window mode. */
            if (user.pref1 & PREF1_DUALWINDOW)
                {
                onflag = 0;
                }
            }
        }

    /* Show messages only if the user isn't typing, or if the user has
       block while typing turned off. */
    if (pos == 0 || (pos > 0 && !(user.pref1 & PREF1_BLKWHILETYP)))
        {
        /* Only poll for messages once enough time has elapsed. */
        if ((((float) myclock() - (float) lastpoll) / (float) CLK_TCK) >=
            POLLTIMEDELAY)
            {
            /* Poker operations, unused. */
/*//|            if ((((float) myclock() - (float) lastpokerpoll) /
                   (float) CLK_TCK) >=
                   ((float) cfg.pokerpolltime / 10.0))
                {
                XINT pokd;

                for (pokd = 0; pokd < cfg.pokermaxgames; pokd++)
                    {
                    if (pokeringame[pokd])
                        {
                        poker_eventkernel(pokd);
                        }
                    }
                lastpokerpoll = myclock();
                }*///|
            /* Store the screen location.  This is no longer used but was
               left in so it could be used later. */
            if (pos > 0)
                {
				od_get_cursor(&ypos,&xpos);
				ystore=ypos;
				xstore=xpos;
                }
            /* Process incoming messages.  Only delete the name if the user
               isn't typing, which is what the parameter indicates. */
            tmp = process_messages(pos == 0 ? TRUE : FALSE);
            /* If the result is greater than one, at least one message was
               processed. */
            if (tmp > 0)
                {
                /* pos is set to -1 to tell the next run of the loop that
                   the screen needs to be restored. */
                pos = -1;
                /* onflag used to be set down here but it works better
                   when set at the start of the loop. */
/*                if (strlen(input) > 0)
                    {
                    onflag = 1;
                    }*/
                }
            if (tmp < 1)
                {
                /* If no messages were processed, check for any BBS
                   pages. */
                if (cfg.bbstype != BBS_UNKNOWN && bbs_call_processmsgs)
                    {
                    tmp = (*bbs_call_processmsgs)();
                    }
                if (tmp > 0)
                    {
                    /* Tell the next loop to restore the screen. */
                    pos = -1;
                    }
                }
            /* Set the last polling time. */
            lastpoll = myclock();
            }
        }
    /* Only get a keypress if the screen is ready. */
    if (pos != -1)
   		{
        /* Check if any keys have been pressed, without waiting for one. */
        key = od_get_key(FALSE);
        }
    /* Process the key if one was pressed. */
    if (key)
   		{
        /* Backspace (also delete). */
        if (key == 8 || key == 127)
   	        {
            /* Only backspace if the user has typed something. */
            if (pos > 0)
                {
                /* Clear the last character and move back the position
                   counter. */
                input[--pos] = '\0';
                /* Display a destructive backspace. */
                od_disp_str("\b \b");
                /* Clear the last character in the word wrap buffer if there
                   is one. */
                if (wwc > 0)
                    {
                    ww[--wwc] = '\0';
                    }
                }
            continue;
            }
        /* ENTER (or RETURN). */
        if (key == 13)
            {
            /* Trim trailing spaces from the input, which users can use to
               annoy people. */
            trim_string(input, input, NO_LTRIM);
            /* Process the input. */
            process_input(input);
            /* Save the screen position in dual window mode. */
            if (user.pref1 & PREF1_DUALWINDOW)
                {
                // Different for AVT when I find out what the store/recv codes are.
                od_disp_emu("\x1B" "[s", TRUE);
                }
            /* Tell the next loop the screen needs restoring. */
            pos = -1;
            /* Reset the buffers and word wrap counter. */
            memset(input, 0, MAXSTRLEN + 1);
            memset(ww, 0, 81);
            wwc = 0;
            continue;
            }
        /* Displayable characters.  Only process if the string isn't too
           long. */
        if (key >= 32 && key <= MAXASCII &&
            pos < MAXSTRLEN)
            {
            /* Store this keypress in the input buffer. */
            input[pos++] = key;
            input[pos] = '\0';
            /* Display the key directly. */
            od_putch(key);
            /* Store the keypress in the word wrap buffer. */
            ww[wwc++] = key;
            ww[wwc] = '\0';
            /* Space, hyphen, and slash are considered the word break
               characters. */
            if (key == ' ' || key == '-' || key == '/')
                {
                /* Clear the word wrap buffers. */
                wwc = 0;
                memset(ww, 0, 81);
                }
			od_get_cursor(&ypos,&xpos);
            if (xpos != 80)
                {
                /* If the cursor isn't at the end of the screen, nothing
                   more needs to be done. */
                continue;
                }
            else
                {
                /* End of line reached, perform word wrap. */
                XINT xwc; /* Backspace counter. */

                /* Only wrap if the current word is less than 50 characters
                   long.  Otherwise it can be slow and annoying to the user. */
                if (wwc < 50)
                    {
                    /* Backspace out the last word. */
                    for (xwc = 0; xwc < wwc; xwc++)
                        {
                        od_disp_str("\b \b");
                        }
                    }
                /* Jump to the next line. */
                od_disp_str("\r\n");
                /* Redisplay the last word if it's not too long. */
                if (wwc < 50)
                    {
                    od_disp_str(ww);
                    }
                /* Clear the word wrap buffer and reset the counter. */
                memset(ww, 0, 81);
                wwc = 0;
                continue;
                }
            }
        }
/* Timeslicing is only done under DOS. */
#if !defined(__OS2__) && !defined(__WIN32__) && !defined(__unix__)
	else
		{
        /* Timeslice if a key wasn't pressed and if the clock tick has
           changed.  Waiting for the next clock tick prevents TOP from
           completely surrendering control of the CPU, which would make
           input hideously slow. */
        if (!key || _clock_tick() != clktick)
	        {
            od_sleep(0);
        	}
        }
#endif
    }

}
