/******************************************************************************
HELP.C       HELP command processor.

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

Processes the HELP command to display help files.
******************************************************************************/
#include "top.h"

/* help_proc() - Process the HELP command.
   Parameters:  helpwds - Number of words in the command string.
   Returns:  Nothing.
*/
void help_proc(XINT helpwds)
    { // Will need to change the order to the order proc'd later.
    char hused = 0; /* Flag if a help file has been shown or not. */
    unsigned char *helpfile = NULL; /* Name of help file to show. */

    /* In this function, basically every command is checked to see if it
       follows the HELP command.  If a match is found, the name of the
       file to show is set, as is a flag. */

    /* Due to the simplicity of this function, comments have been kept
       to a minimum. */

    /* Regular commands. */
    if (!hused && (helpwds < 2 ||
                   checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsHelp"))))
        {
        hused = 1;
        helpfile = (unsigned char *)"help_hlp";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsQuit")))
        {
        hused = 1;
        helpfile = (unsigned char *)"helpquit";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsCommands")))
        {
        hused = 1;
        helpfile = (unsigned char *)"helpcmds";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsColourHelp")))
        {
        hused = 1;
        helpfile = (unsigned char *)"helpchlp";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsTime")))
        {
        hused = 1;
        helpfile = (unsigned char *)"helptime";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsWho")))
        {
        hused = 1;
        helpfile = (unsigned char *)"help_who";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsProfile")))
        {
        hused = 1;
        helpfile = (unsigned char *)"helpprof";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsLookUp")))
        {
        hused = 1;
        helpfile = (unsigned char *)"helplkup";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsNodes")))
        {
        hused = 1;
        helpfile = (unsigned char *)"helpnods";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsPage")))
        {
        hused = 1;
        helpfile = (unsigned char *)"helppage";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsForget")))
        {
        hused = 1;
        helpfile = (unsigned char *)"help_fgt";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsRemember")))
        {
        hused = 1;
        helpfile = (unsigned char *)"helprmbr";
        } // May need slight mods below
    /* Whispers have two command forms; each is checked. */
    if (!hused && (checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsWhisperLong1")) ||
                   checkcmdmatch(get_word(1), getlang((unsigned char *)"WhisperShortChar"))))
        {
        hused = 1;
        helpfile = (unsigned char *)"helpwhis";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsGA")))
        {
        hused = 1;
        helpfile = (unsigned char *)"help_ga1";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsGA2")))
        {
        hused = 1;
        helpfile = (unsigned char *)"help_ga2";
        } // May need slight mods below
    /* Directed messages have two command forms; each is checked. */
    if (!hused && (checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsDirectedLong1")) ||
                   checkcmdmatch(get_word(1), getlang((unsigned char *)"DirectedShortChar"))))
        {
        hused = 1;
        helpfile = (unsigned char *)"help_dir";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsJoin")))
        {
        hused = 1;
        helpfile = (unsigned char *)"helpjoin";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsPrivChat")))
        {
        hused = 1;
        helpfile = (unsigned char *)"helpchat";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsPrivDenyChat")))
        {
        hused = 1;
        helpfile = (unsigned char *)"helpdeny";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsLogOff")))
        {
        hused = 1;
        helpfile = (unsigned char *)"helplgof";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsConfList")))
        {
        hused = 1;
        helpfile = (unsigned char *)"helpclst";
        }

    /* Multi-part commands.  If a valid secondary command is recognized
       then its specific help file will be shown, otherwise a general help
       file for the multi-part command will be shown. */
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsAction")))
        {
        hused = 1;
        helpfile = (unsigned char *)"helpact0";
        if (helpwds >= 2)
            {
            if (checkcmdmatch(get_word(2), getlang((unsigned char *)"CmdsActionList")))
                helpfile = (unsigned char *)"helpact1";
            if (checkcmdmatch(get_word(2), getlang((unsigned char *)"CmdsActionOn")))
                helpfile = (unsigned char *)"helpact2";
            if (checkcmdmatch(get_word(2), getlang((unsigned char *)"CmdsActionOff")))
                helpfile = (unsigned char *)"helpact3";
            }
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsChange")))
        {
        hused = 1;
        helpfile = (unsigned char *)"helpchg0";
        if (helpwds >= 2)
            {
            if (checkcmdmatch(get_word(2), getlang((unsigned char *)"CmdsChangeHandle")))
                helpfile = (unsigned char *)"helpchg1";
            if (checkcmdmatch(get_word(2), getlang((unsigned char *)"CmdsChangeEMessage")))
                helpfile = (unsigned char *)"helpchg2";
            if (checkcmdmatch(get_word(2), getlang((unsigned char *)"CmdsChangeXMessage")))
                helpfile = (unsigned char *)"helpchg3";
            if (checkcmdmatch(get_word(2), getlang((unsigned char *)"CmdsChangeSex")))
                helpfile = (unsigned char *)"helpchg4";
            if (checkcmdmatch(get_word(2), getlang((unsigned char *)"CmdsChangeChatMode")))
                helpfile = (unsigned char *)"helpchg5";
            if (checkcmdmatch(get_word(2), getlang((unsigned char *)"CmdsChangeListed")))
                helpfile = (unsigned char *)"helpchg6";
            }
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsSysop")))
        {
        hused = 1;
        helpfile = (unsigned char *)"helpsys0";
        if (helpwds >= 2)
            {
            if (checkcmdmatch(get_word(2), getlang((unsigned char *)"CmdsSysopClear")))
                helpfile = (unsigned char *)"helpsys1";
            if (checkcmdmatch(get_word(2), getlang((unsigned char *)"CmdsSysopToss")))
                helpfile = (unsigned char *)"helpsys2";
            if (checkcmdmatch(get_word(2), getlang((unsigned char *)"CmdsSysopZap")))
                helpfile = (unsigned char *)"helpsys3";
            if (checkcmdmatch(get_word(2), getlang((unsigned char *)"CmdsSysopSetSec")))
                helpfile = (unsigned char *)"helpsys4";
            }
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsModerator")))
        {
        hused = 1;
        helpfile = (unsigned char *)"helpmod0";
        if (helpwds >= 2)
            {
            if (checkcmdmatch(get_word(2), getlang((unsigned char *)"CmdsModTopicChg")))
                helpfile = (unsigned char *)"helpmod1";
            if (checkcmdmatch(get_word(2), getlang((unsigned char *)"CmdsModModChg")))
                helpfile = (unsigned char *)"helpmod2";
            if (checkcmdmatch(get_word(2), getlang((unsigned char *)"CmdsModBan")))
                helpfile = (unsigned char *)"helpmod3";
            if (checkcmdmatch(get_word(2), getlang((unsigned char *)"CmdsModUnBan")))
                helpfile = (unsigned char *)"helpmod4";
            if (checkcmdmatch(get_word(2), getlang((unsigned char *)"CmdsModInvite")))
                helpfile = (unsigned char *)"helpmod5";
            if (checkcmdmatch(get_word(2), getlang((unsigned char *)"CmdsModUnInvite")))
                helpfile = (unsigned char *)"helpmod6";
            }
        }

    /* Non-command help topics. */
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsHelpTopics")))
        {
        hused = 1;
        helpfile = (unsigned char *)"help_top";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsHelpGeneral")))
        {
        hused = 1;
        helpfile = (unsigned char *)"help_gen";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsHelpActions")))
        {
        hused = 1;
        helpfile = (unsigned char *)"helpacts";
        }
    if (!hused && checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsHelpChannels")))
        {
        hused = 1;
        helpfile = (unsigned char *)"helpchan";
        }

    if (hused)
        {
        /* Show the help file if one was set. */
        show_helpfile(helpfile);
        }
    else
        {
        /* Unknown command. */
        top_output(OUT_SCREEN, getlang((unsigned char *)"NoHelpAvailable"));
        }

    }

/* show_helpfile() - Displays a help file to the screen.
   Parameters:  hlpname - Base name of the help file to show.
   Returns:  Nothing.
   Notes:  This function is really just a wrapper for show_file() that
           adds in the help prefix and suffix as well.
*/
void show_helpfile(unsigned char *hlpname)
    {

    top_output(OUT_SCREEN, getlang((unsigned char *)"HelpPrefix"));
    show_file((char *)hlpname, SCRN_RESETCOL);
    top_output(OUT_SCREEN, getlang((unsigned char *)"HelpSuffix"));

    }
