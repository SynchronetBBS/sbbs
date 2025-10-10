/******************************************************************************
SCREENS.C    Large displays and screen showing functions.

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

This module contains functions to display external screens.  It also contains
many of TOP's large displays, such as the opening and closing credits.
******************************************************************************/

#include "strwrap.h"
#include "top.h"

/* This macro should be defined during testing, when an internal revision
   number is needed in addition to the program version. */
#define USE_REVISION

/* show_file() - Displays an external screen file.
   Parameters:  name - Base file name to display, with optional pass.
                tasks - Functions to perform (SCRN_ task bits).
   Returns:  Nothing.
   Notes:  The name parameter can contain an extension, but for normal
           external files (eg. those stored in the TOP ANSI Path) it should
           not contain one, which tells the door kit to determine the
           proper extension based on terminal type.
*/
void show_file(char *name, unsigned char tasks)
{

/* Reset attribute before displaying. */
if (tasks & SCRN_RESETCOL)
    {
    od_set_colour(D_GREY, D_BLACK);
    }
/* Clear screen before displaying. */
if (tasks & SCRN_CLS)
    {
    od_clr_scr();
    }

/* Display the file. */
top_output(OUT_SCREEN, getlang((unsigned char *)"ShowFilePrefix"));
sprintf((char*)outbuf, "%s%s", cfg.topansipath, name);
od_send_file((char*)outbuf);
top_output(OUT_SCREEN, getlang((unsigned char *)"ShowFileSuffix"));

/* Wait for keypress after displaying. */
if (tasks & SCRN_WAITKEY)
    {
    top_output(OUT_SCREEN, getlang((unsigned char *)"PressAnyKey"));
    od_get_key(TRUE);
    top_output(OUT_SCREEN, getlang((unsigned char *)"PressAnyKeySuffix"));
    }

return;
}

/* minihelp() - Shows command listing.
   Parameters:  None.
   Returns:  Nothing.
*/
void minihelp(void)
{

/* The command listing used to be hard coded, which is why this display has
   its own function.  Now, however, an external file is used. */

top_output(OUT_SCREEN, getlang((unsigned char *)"CommandListPrefix"));
show_file("minihelp", SCRN_NONE);

}

/* welcomescreen() - Show TOP opening credits.
   Parameters:  None.
   Returns:  Nothing.
*/
void welcomescreen(void)
{

if (cfg.showopeningcred)
    {
    /* Long form. */

    top_output(OUT_SCREEN, (unsigned char *)"^o@cWelcome to The Online Pub for ");
#if defined(PLATFORM_DESC)
    top_output(OUT_SCREEN, (unsigned char *)(PLATFORM_DESC));
#elif defined(__OS2__)
    top_output(OUT_SCREEN, (unsigned char *)"OS/2");
#elif defined(__WIN32__)
    top_output(OUT_SCREEN, (unsigned char *)"Windows 95");
#elif defined(__unix__)
    top_output(OUT_SCREEN, (unsigned char *)"*nix");
#else
    top_output(OUT_SCREEN, (unsigned char *)"DOS");
#endif
    top_output(OUT_SCREEN, (unsigned char *)"^l v@1^o, by ^lPaul Sidorsky^o.@c", ver);
#ifdef USE_REVISION
    top_output(OUT_SCREEN, (unsigned char *)"^oInternal revision #^l@1^o.@c", rev);
#endif
    top_output(OUT_SCREEN, (unsigned char *)"Copyright 1993 - 1998 ");
    top_output(OUT_SCREEN, (unsigned char *)"^lISMWare(TM)^o, All Rights Reserved.@c");
    top_output(OUT_SCREEN, (unsigned char *)"^oWritten using ^l");
#ifdef __OS2__
    top_output(OUT_SCREEN, (unsigned char *)"Doors/2 v5.00b3");
#else
    top_output(OUT_SCREEN, (unsigned char *)"OpenDoors v6.00");
#endif
    top_output(OUT_SCREEN, (unsigned char *)"^o by ^l");
#ifdef __OS2__
    top_output(OUT_SCREEN, (unsigned char *)"Joel Downer & Brian Pirie");
#else
    top_output(OUT_SCREEN, (unsigned char *)"Brian Pirie");
#endif
    top_output(OUT_SCREEN, (unsigned char *)"^o.@c");
    top_output(OUT_SCREEN, getlang((unsigned char *)"BroughtToYouBy"), cfg.sysopname,
               cfg.bbsname);
    itoa(od_control.od_node, (char*)outnum[0], 10);
    top_output(OUT_SCREEN, getlang((unsigned char *)"LoggedOnNode"), outnum[0]);
    }
else
    {
    /* Short form. */

    top_output(OUT_SCREEN, (unsigned char *)"@c^oThe Online Pub for ");
#if defined(PLATFORM_DESC)
    top_output(OUT_SCREEN, (unsigned char *)(PLATFORM_DESC));
#elif defined(__OS2__)
    top_output(OUT_SCREEN, (unsigned char *)"OS/2");
#elif defined(__WIN32__)
    top_output(OUT_SCREEN, (unsigned char *)"Windows 95");
#elif defined(__unix__)
    top_output(OUT_SCREEN, (unsigned char *)"*nix");
#else
    top_output(OUT_SCREEN, (unsigned char *)"DOS");
#endif
    top_output(OUT_SCREEN, (unsigned char *)" v@1@cCopyright 1993 - 1998 Paul Sidorsky, "
               "All Rights Reserved.@c", ver);
#ifdef USE_REVISION
    top_output(OUT_SCREEN, (unsigned char *)"^oInternal revision #^l@1^o.@c", rev);
#endif
    }

return;
}

/* newuserscreen() - Shows information screen to new TOP users.
   Parameters:  None.
   Return:  Nothing.
*/
void newuserscreen(void)
{

/* The new user screen used to be hard coded, which is why this display has
   its own function.  Now, however, an external file is used. */

show_file("newuser", SCRN_NONE);

return;
}

/* quit_top_screen() - Show TOP closing credits.
   Parameters:  None
   Returns:  Nothing.
*/
void quit_top_screen(void)
{

/* Nothing is shown if the sysop has closing credits turned off, to provide
   a more seamless appearance. */
if (!cfg.showclosingcred)
    {
    return;
    }

top_output(OUT_SCREEN, (unsigned char *)"@c@c^kThe Online Pub for ");
#if defined(PLATFORM_DESC)
    top_output(OUT_SCREEN, (unsigned char *)(PLATFORM_DESC));
#elif defined(__OS2__)
    top_output(OUT_SCREEN, (unsigned char *)"OS/2");
#elif defined(__WIN32__)
    top_output(OUT_SCREEN, (unsigned char *)"Windows 95");
#elif defined(__unix__)
    top_output(OUT_SCREEN, (unsigned char *)"*nix");
#else
    top_output(OUT_SCREEN, (unsigned char *)"DOS");
#endif
sprintf((char*)outbuf, " v%s@c", ver);
top_output(OUT_SCREEN, outbuf);
top_output(OUT_SCREEN, (unsigned char *)"Copyright 1993 - 1998 ISMWare(TM)@c@c");
top_output(OUT_SCREEN, getlang((unsigned char *)"Goodbye"));
top_output(OUT_SCREEN, getlang((unsigned char *)"ReturnToBBS"), cfg.bbsname);

return;
}

/* channelsummary() - Show who's in a channel and a quick help note.
   Parameters:  None.
   Returns:  Nothing.
   Notes:  This is the display used whenever a user first enters a new
           channel or when a user presses ENTER without typing any text.
*/
void channelsummary(void)
{
long xchan; /* No longer used. */
char chtyp, yestop = 0; /* Channel type reference, flag if there is a topic. */

top_output(OUT_SCREEN, getlang((unsigned char *)"ChanSummaryPrefix"));

/* Determine the channel type.  The type numbers are internal to this
   function only. */
/* Normal channel. */
if (curchannel < 4000000000UL)
    {
    chtyp = 0;
    }
/* Personal channel. */
if (curchannel >= 4000000000UL && curchannel < 4001000000UL)
    {
    chtyp = 1;
    }
/* Conference. */
if (curchannel > 4000999999UL)
    {
    chtyp = 2;
    }

/* Set a flag if there is a topic defined for this channel. */
if (cmibuf.topic[0] != 0)
    {
    yestop = 1;
    }

/* Tell the user where he/she is. */
switch (chtyp)
    {
    case 0: strcpy((char*)outbuf, "YoureInChannel"); break;
    case 1: strcpy((char*)outbuf, "YoureInUserChannel"); break;
    case 2: strcpy((char*)outbuf, "YoureInConference"); break;
    }
top_output(OUT_SCREEN, getlang(outbuf), channelname(curchannel));

/* Show topic, if one exists. */
if (yestop)
    {
    switch (chtyp)
        {
        case 0: strcpy((char*)outbuf, "ChannelTopic"); break;
        case 1: strcpy((char*)outbuf, "UserChannelTopic"); break;
        case 2: strcpy((char*)outbuf, "ConferenceTopic"); break;
        }
    top_output(OUT_SCREEN, getlang(outbuf), cmibuf.topic);
    }

/* Show who's in the channel. */
whos_in_pub();

/* Show a quick message on how to get help. */
top_output(OUT_SCREEN, getlang((unsigned char *)"ShortHelp"));

top_output(OUT_SCREEN, getlang((unsigned char *)"OpeningSuffix"));

}
