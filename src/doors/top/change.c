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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

Processes the CHANGE command to change user settings.
******************************************************************************/

#include "top.h"

/* change_proc() - CHANGE command processor.
   Parameters:  None.
   Returns:  Nothing.
*/
void change_proc(void)
{
unsigned char tmpstr[256]; /* Temporary output buffer. */

/* Change user handle. */
if (checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsChangeHandle")) > 0)
	{
    unsigned char thand[31]; /* Temporary handle buffer. */

    if (!cfg.allowhandlechange)
        {
        top_output(OUT_SCREEN, getlang((unsigned char *)"ChgHandNotAllowed"));
        return;
        }

    if (user.security < cfg.handlechgsec)
        {
        top_output(OUT_SCREEN, getlang((unsigned char *)"NoAccChgHandle"));
        return;
        }

    memset(thand, 0, 31);

    /* Only continue processing if there's at least three words. */
    if (get_word_char(2, 0))
    	{
	    strncpy((char*)thand, (char*)&word_str[word_pos[2]], 30);

        if (check_dupe_handles(thand))
           	{
            top_output(OUT_SCREEN, getlang((unsigned char *)"HandleInUse"));
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

        top_output(OUT_SCREEN, getlang((unsigned char *)"ChangedHandle"), user.handle);
        od_log_write((char*)top_output(OUT_STRING, getlang((unsigned char *)"LogChangedHandle"),
                     user.handle));

        return;
	    }
    top_output(OUT_SCREEN, getlang((unsigned char *)"HandleNotSpecified"));
    return;
	}
/* Change the user's gender. */
if (checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsChangeSex")) > 0)
    {
    if (!cfg.allowsexchange)
        {
        top_output(OUT_SCREEN, getlang((unsigned char *)"ChgSexNotAllowed"));
        return;
        }

    if (user.security < cfg.sexchangesec)
        {
        top_output(OUT_SCREEN, getlang((unsigned char *)"NoAccChgSex"));
        return;
        }

    /* Save new information to user file. */
    user.gender = !user.gender;
    save_user_data(user_rec_num, &user);
    top_output(OUT_SCREEN, getlang((unsigned char *)"ChangedSex"),
               getlang(user.gender ? (unsigned char *)"Female" : (unsigned char *)"Male"));

    /* Notify other users. */
    if (user.gender)
    {
        strcpy((char*)tmpstr, (char*)top_output(OUT_STRINGNF, getlang((unsigned char *)"Female")));
    }
    else
    {
        strcpy((char*)tmpstr, (char*)top_output(OUT_STRINGNF, getlang((unsigned char *)"Male")));
    }
    dispatch_message(MSG_SEXCHG, tmpstr, -1, 0, 0);

    return;
    }
/* Change user's entry message. */
if (checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsChangeEMessage")) > 0)
    {
    if (!cfg.allowexmessages)
        {
        top_output(OUT_SCREEN, getlang((unsigned char *)"ChgEXMsgNotAllowed"));
        return;
        }

    if (user.security < cfg.exmsgchangesec)
        {
        top_output(OUT_SCREEN, getlang((unsigned char *)"NoAccChgEXMsg"));
        return;
        }

    /* Clear the message if none is specified. */
    if (!get_word_char(2, 0))
        {
        memset(user.emessage, 0, 81);
        save_user_data(user_rec_num, &user);
        top_output(OUT_SCREEN, getlang((unsigned char *)"EMessageCleared"));
        return;
        }

    /* Save new information to user file. */
    memset(user.emessage, 0, 81);
    strncpy((char*)user.emessage, (char*)&word_str[word_pos[2]], 80);
    save_user_data(user_rec_num, &user);
    top_output(OUT_SCREEN, getlang((unsigned char *)"ChangedEMessage"));

    /* Notify other users. */
    dispatch_message(MSG_EXMSGCHG, (unsigned char *)"", -1, 0, 0);

    return;
    }
/* Change the user's exit message. */
if (checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsChangeXMessage")) > 0)
    {
    if (!cfg.allowexmessages)
        {
        top_output(OUT_SCREEN, getlang((unsigned char *)"ChgEXMsgNotAllowed"));
        return;
        }

    if (user.security < cfg.exmsgchangesec)
        {
        top_output(OUT_SCREEN, getlang((unsigned char *)"NoAccChgEXMsg"));
        return;
        }

    /* Clear the message if none is specified. */
    if (!get_word_char(2, 0))
        {
        memset(user.xmessage, 0, 81);
        save_user_data(user_rec_num, &user);
        top_output(OUT_SCREEN, getlang((unsigned char *)"XMessageCleared"));
        return;
        }

    /* Save new information to user file. */
    memset(user.xmessage, 0, 81);
    strncpy((char*)user.xmessage, (char*)&word_str[word_pos[2]], 80);
    save_user_data(user_rec_num, &user);
    top_output(OUT_SCREEN, getlang((unsigned char *)"ChangedXMessage"));

    /* Notify other users. */
    dispatch_message(MSG_EXMSGCHG, (unsigned char *)"", -1, 0, 0);
    return;
    }
/* Toggle dual-window chat mode. */
if (checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsChangeChatMode")) > 0)
    {
    /* Dual Window chat mode currently only works with ANSI.  AVATAR
       support not implemented. */
    if (!od_control.user_ansi/* && !od_control.user_avatar*/)
        {
        top_output(OUT_SCREEN, (unsigned char *)"DualWindowNoGfx");
        return;
        }
    else
        {
        /* Save new information. */
        user.pref1 ^= PREF1_DUALWINDOW;
        save_user_data(user_rec_num, &user);
        top_output(OUT_SCREEN, getlang((unsigned char *)"ChangedChatMode"),
                   getlang(user.pref1 & PREF1_DUALWINDOW ?
                           (unsigned char *)"DualWindowChatMode" : (unsigned char *)"NormalChatMode"));
        return;
        }
    }
/* Toggle channel listed status. */
if (checkcmdmatch(get_word(1), getlang((unsigned char *)"CmdsChangeListed")) > 0)
    {
    /* Save new information. */
    user.pref2 ^= PREF2_CHANLISTED;
    save_user_data(user_rec_num, &user);
    node->channellisted = (user.pref2 & PREF2_CHANLISTED);
    save_node_data(od_control.od_node, node);
    top_output(OUT_SCREEN, getlang((unsigned char *)"ChangedListed"),
               getlang(user.pref2 & PREF2_CHANLISTED ? (unsigned char *)"ChannelListed" :
               (unsigned char *)"ChannelUnlisted"));
    return;
    }

/* Command not recognized, show help file instead. */
show_helpfile((unsigned char *)"helpchg0");

return;
}
