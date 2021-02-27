/******************************************************************************
CHANGE.C     CHANGE command processor.

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

Processes the CHANGE command to change user settings.
******************************************************************************/

#include "top.h"

/* change_proc() - CHANGE command processor.
   Parameters:  None.
   Returns:  Nothing.
*/
void change_proc(void)
{
char tmpstr[256]; /* Temporary output buffer. */

/* Change user handle. */
if (checkcmdmatch(get_word(1), getlang("CmdsChangeHandle")) > 0)
	{
    char thand[31]; /* Temporary handle buffer. */

    if (!cfg.allowhandlechange)
        {
        top_output(OUT_SCREEN, getlang("ChgHandNotAllowed"));
        return;
        }

    if (user.security < cfg.handlechgsec)
        {
        top_output(OUT_SCREEN, getlang("NoAccChgHandle"));
        return;
        }

    memset(thand, 0, 31);

    /* Only continue processing if there's at least three words. */
    if (get_word_char(2, 0))
    	{
	    strncpy(thand, &word_str[word_pos[2]], 30);

        if (check_dupe_handles(thand))
           	{
            top_output(OUT_SCREEN, getlang("HandleInUse"));
            return;
			}

        fixname(user.handle, thand);

        /* Update list of online handles. */
        fixname(handles[od_control.od_node].string, thand);

        /* Notify other users. */
        dispatch_message(MSG_HANDLECHG, thand, -1, 0, 0);

        /* Save new handle to user and NODEIDX files. */
        save_user_data(user_rec_num, &user);
        fixname(node->handle, thand);
        save_node_data(od_control.od_node, node);

        top_output(OUT_SCREEN, getlang("ChangedHandle"), user.handle);
        od_log_write(top_output(OUT_STRING, getlang("LogChangedHandle"),
                     user.handle));

        return;
	    }
    top_output(OUT_SCREEN, getlang("HandleNotSpecified"));
    return;
	}
/* Change the user's gender. */
if (checkcmdmatch(get_word(1), getlang("CmdsChangeSex")) > 0)
    {
    if (!cfg.allowsexchange)
        {
        top_output(OUT_SCREEN, getlang("ChgSexNotAllowed"));
        return;
        }

    if (user.security < cfg.sexchangesec)
        {
        top_output(OUT_SCREEN, getlang("NoAccChgSex"));
        return;
        }

    /* Save new information to user file. */
    user.gender = !user.gender;
    save_user_data(user_rec_num, &user);
    top_output(OUT_SCREEN, getlang("ChangedSex"),
               getlang(user.gender ? "Female" : "Male"));

    /* Notify other users. */
    if (user.gender)
    {
        strcpy(tmpstr, top_output(OUT_STRINGNF, getlang("Female")));
    }
    else
    {
        strcpy(tmpstr, top_output(OUT_STRINGNF, getlang("Male")));
    }
    dispatch_message(MSG_SEXCHG, tmpstr, -1, 0, 0);

    return;
    }
/* Change user's entry message. */
if (checkcmdmatch(get_word(1), getlang("CmdsChangeEMessage")) > 0)
    {
    if (!cfg.allowexmessages)
        {
        top_output(OUT_SCREEN, getlang("ChgEXMsgNotAllowed"));
        return;
        }

    if (user.security < cfg.exmsgchangesec)
        {
        top_output(OUT_SCREEN, getlang("NoAccChgEXMsg"));
        return;
        }

    /* Clear the message if none is specified. */
    if (!get_word_char(2, 0))
        {
        memset(user.emessage, 0, 81);
        save_user_data(user_rec_num, &user);
        top_output(OUT_SCREEN, getlang("EMessageCleared"));
        return;
        }

    /* Save new information to user file. */
    memset(user.emessage, 0, 81);
    strncpy(user.emessage, &word_str[word_pos[2]], 80);
    save_user_data(user_rec_num, &user);
    top_output(OUT_SCREEN, getlang("ChangedEMessage"));

    /* Notify other users. */
    dispatch_message(MSG_EXMSGCHG, "\0", -1, 0, 0);

    return;
    }
/* Change the user's exit message. */
if (checkcmdmatch(get_word(1), getlang("CmdsChangeXMessage")) > 0)
    {
    if (!cfg.allowexmessages)
        {
        top_output(OUT_SCREEN, getlang("ChgEXMsgNotAllowed"));
        return;
        }

    if (user.security < cfg.exmsgchangesec)
        {
        top_output(OUT_SCREEN, getlang("NoAccChgEXMsg"));
        return;
        }

    /* Clear the message if none is specified. */
    if (!get_word_char(2, 0))
        {
        memset(user.xmessage, 0, 81);
        save_user_data(user_rec_num, &user);
        top_output(OUT_SCREEN, getlang("XMessageCleared"));
        return;
        }

    /* Save new information to user file. */
    memset(user.xmessage, 0, 81);
    strncpy(user.xmessage, &word_str[word_pos[2]], 80);
    save_user_data(user_rec_num, &user);
    top_output(OUT_SCREEN, getlang("ChangedXMessage"));

    /* Notify other users. */
    dispatch_message(MSG_EXMSGCHG, "\0", -1, 0, 0);
    return;
    }
/* Toggle dual-window chat mode. */
if (checkcmdmatch(get_word(1), getlang("CmdsChangeChatMode")) > 0)
    {
    /* Dual Window chat mode currently only works with ANSI.  AVATAR
       support not implemented. */
    if (!od_control.user_ansi/* && !od_control.user_avatar*/)
        {
        top_output(OUT_SCREEN, "DualWindowNoGfx");
        return;
        }
    else
        {
        /* Save new information. */
        user.pref1 ^= PREF1_DUALWINDOW;
        save_user_data(user_rec_num, &user);
        top_output(OUT_SCREEN, getlang("ChangedChatMode"),
                   getlang(user.pref1 & PREF1_DUALWINDOW ?
                           "DualWindowChatMode" : "NormalChatMode"));
        return;
        }
    }
/* Toggle channel listed status. */
if (checkcmdmatch(get_word(1), getlang("CmdsChangeListed")) > 0)
    {
    /* Save new information. */
    user.pref2 ^= PREF2_CHANLISTED;
    save_user_data(user_rec_num, &user);
    node->channellisted = (user.pref2 & PREF2_CHANLISTED);
    save_node_data(od_control.od_node, node);
    top_output(OUT_SCREEN, getlang("ChangedListed"),
               getlang(user.pref2 & PREF2_CHANLISTED ? "ChannelListed" :
               "ChannelUnlisted"));
    return;
    }

/* Command not recognized, show help file instead. */
show_helpfile("helpchg0");

return;
}
