/******************************************************************************
MESSAGES.C   MODERATOR command processor.

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

This module processes the MODERATOR command which lets users control aspects
about their current channel, including who is inside it.
******************************************************************************/

#include "top.h"

/* mod_proc() - MODERATOR command processor.
   Parameters:  None.
   Returns:  Nothing.
*/
void mod_proc(void)
    {
    XINT sendto; /* Node a command is being done to. */
    /* String buffer, handle buffer. */
    unsigned char tmpstr[256], tmphand[256];
    XINT res; /* Result code. */
    XINT baninvflg = 0; /* Action to perform ((un)ban or (un)invite). */
    long tmpcdef; /* Channel definition number holder. */
    node_idx_typ nmod; /* Node index data buffer for "receiving" node. */

    /* Find a channel record for the current channel. */
    tmpcdef = findchannelrec(curchannel);
    /* The user can't perform a moderatorship command if certain conditions are
       true.  The complex test below tests:  if the user is not listed
       as a moderator, if it is not the user's own personal channel, if the
       channel is not defined (in CHANNELS.CFG) and the user is not a sysop, or
       the user's security is below the defined moderatorship security.  All
       conditions must be true (and more often than not they are) to prohibit
       the user from moderating. */
    if (cmibuf.modnode != od_control.od_node &&
        curchannel != ((unsigned long) od_control.od_node + 4000000000UL) &&
        (tmpcdef == -1L || user.security < chan[tmpcdef].modsec) &&
        user.security < cfg.sysopsec)
        {
        top_output(OUT_SCREEN, getlang("NotModerator"));
        return;
        }

    /* Nobody can moderate the main channel because all users must have
       unconditional access to it in the event a user is tossed out of a
       channel or if a technical problem occurs. */
    if (curchannel == cfg.defaultchannel)
        {
        top_output(OUT_SCREEN, getlang("CantModInMain"));
        return;
        }

    /* All commands first refresh (reload) the channel data, which also
       locks it.  After copying new information, the command saves the
       new data, which unlocks it. */

    /* Change channel topic. */
    if (checkcmdmatch(get_word(1), getlang("CmdsModTopicChg")))
        {
        res = cmi_load(curchannel);
        if (res != CMI_OKAY)
            {
            top_output(OUT_SCREEN, getlang("CantDoModCmd"));
            return;
            }
        /* Copy the topic, which begins at the 3rd word of the input. */
        memset(cmibuf.topic, 0, 71);
        strncpy(cmibuf.topic, &word_str[word_pos[2]], 70);
        res = cmi_save();
        if (res != CMI_OKAY)
            {
            top_output(OUT_SCREEN, getlang("CantDoModCmd"));
            return;
            }
        dispatch_message(MSG_CHANTOPICCHG, cmibuf.topic, -1, 0, 0);
        top_output(OUT_SCREEN, getlang("ChangedTopic"));
        return;
        }
    /* Change the moderator, or add a new one. */
    if (checkcmdmatch(get_word(1), getlang("CmdsModModChg")))
        {
        /* Find the specified user. */
        sendto = find_node_from_name(tmpstr, tmphand,
                                     &word_str[word_pos[2]]);
        if (sendto == -1)
    		{
            top_output(OUT_SCREEN, getlang("NotLoggedIn"), tmphand);
            return;
    	    }
	    if (sendto == -2)
		   	{
            top_output(OUT_SCREEN, getlang("NotSpecific"), tmphand);
            return;
    	    }
        if (sendto == -3)
            {
            top_output(OUT_SCREEN, getlang("HasForgotYou"),
                       handles[sendto].string);
            return;
            }

        res = cmi_load(curchannel);
        if (res != CMI_OKAY)
            {
            top_output(OUT_SCREEN, getlang("CantDoModCmd"));
            return;
            }
        /* Assign the new moderator.  This can remove this user's moderator
           status depending on the channel.  See CHANNELS.TXT for more
           information. */
        cmibuf.modnode = sendto;
        res = cmi_save();
        if (res != CMI_OKAY)
            {
            top_output(OUT_SCREEN, getlang("CantDoModCmd"));
            return;
            }
        /* Notify everybody about the change. */
        dispatch_message(MSG_CHANMODCHG, "\0", sendto, 0, 0);
        top_output(OUT_SCREEN, getlang("ChangedMod"),
                   handles[sendto].string);
        return;
        }

    /* Ban, unban, invite, or uninvite a user.  The commands are handled by
       the same code as they are so similar.  baninvflg is set to control
       which action to do.  The codes are only used in this function. */
    if (checkcmdmatch(get_word(1), getlang("CmdsModBan")))
        {
        baninvflg = 1;
        }
    if (checkcmdmatch(get_word(1), getlang("CmdsModUnBan")))
        {
        baninvflg = 2;
        }
    if (checkcmdmatch(get_word(1), getlang("CmdsModInvite")))
        {
        baninvflg = 3;
        }
    if (checkcmdmatch(get_word(1), getlang("CmdsModUnInvite")))
        {
        baninvflg = 4;
        }

    /* The user can't ban in a personal channel.  The test is done using
       hardcoded internal channel numbers but probably should use the CHAN_
       channel type constants instead. */
    if (curchannel >= 4000000000UL && curchannel <= 4000999999UL &&
        baninvflg >= 1 && baninvflg <= 2)
        {
        top_output(OUT_SCREEN, getlang("CantBanFromHere"));
        return;
        }
    /* The user can't invite in normal channels or conferences.  Again
       the test uses hardcoded channel numbers. */
    if ((curchannel <= 3999999999UL || curchannel >= 4001000000UL) &&
        baninvflg >= 3 && baninvflg <= 4)
        {
        top_output(OUT_SCREEN, getlang("CantInvFromHere"));
        return;
        }

    /* Proces a ban or invite command. */
    if (baninvflg > 0)
        {
        XINT cmsgtyp = MSG_NULL; /* Message type holder. */

        /* Determine which user is affected by the command. */
        sendto = find_node_from_name(tmpstr, tmphand,
                                     &word_str[word_pos[2]]);
        if (sendto == -1)
    		{
            top_output(OUT_SCREEN, getlang("NotLoggedIn"), tmphand);
            return;
    	    }
	    if (sendto == -2)
		   	{
            top_output(OUT_SCREEN, getlang("NotSpecific"), tmphand);
            return;
    	    }
        /* Users cannot be unbanned or invited if they have forgotten
           this user. */
        if (sendto == -3 && (baninvflg == 2 || baninvflg == 3))
            {
            top_output(OUT_SCREEN, getlang("HasForgotYou"),
                       handles[sendto].string);
            return;
            }
        /* Ban and invite messages are channel and forget ignorant, except
           as described above. */
        if (sendto == -3 || sendto == -4)
            {
            sendto = lastsendtonode;
            }

        /* You can't moderate yourself, for technical reasons only.  I
           never place arbitrary restrictions on user actions unless I
           absolutely have to. */
        if (sendto == od_control.od_node)
            {
            top_output(OUT_SCREEN, getlang("CantDoCmdToSelf"));
            return;
            }

        /* Load the receiving user's data to see if we're trying to block
           a sysop, which is also a no-no.  This restriction is to give
           sysops complete control over TOP. */
        get_node_data(sendto, &nmod);
        if (nmod.security >= cfg.sysopsec && (baninvflg == 1 ||
            baninvflg == 4))
            {
            top_output(OUT_SCREEN, getlang("CantModToSysop"));
            return;
            }

        res = cmi_load(curchannel);
        if (res != CMI_OKAY)
            {
            top_output(OUT_SCREEN, getlang("CantDoModCmd"));
            return;
            }
        /* Ban and invite commands add the user to the special nodes
           list.  See CMI.C and CHANNELS.TXT for more information on
           special nodes. */
        if (baninvflg == 1 || baninvflg == 3)
            {
            /* Add the user to the special nodes list. */
            res = cmi_setspec(sendto, 1);
            if (res == CMIERR_FULL)
                {
                top_output(OUT_SCREEN,
                           getlang(baninvflg == 1 ?
                                   "CantBan" : "CantInvite"),
                           handles[sendto].string);
                cmi_save();
                return;
                }
            }
        /* Unban and Uninvite remove the user from the special nodes list. */
        if (baninvflg == 2 || baninvflg == 4)
            {
            /* Clear the user from the special nodes list. */
            res = cmi_setspec(sendto, 0);
            if (res == CMIERR_NOTFOUND)
                {
                top_output(OUT_SCREEN,
                           getlang(baninvflg == 2 ?
                                   "WasntBanned" : "WasntInvited"),
                           handles[sendto].string);
                cmi_save();
                return;
                }
            }
        if (res != CMI_OKAY)
            {
            top_output(OUT_SCREEN, getlang("CantDoModCmd"));
            cmi_save();
            return;
            }
        res = cmi_save();
        if (res != CMI_OKAY)
            {
            top_output(OUT_SCREEN, getlang("CantDoModCmd"));
            return;
            }

        /* Set the message type to send depending on the command code. */
        switch (baninvflg)
            {
            case 1: cmsgtyp = MSG_CHANBANNED; break;
            case 2: cmsgtyp = MSG_CHANUNBANNED; break;
            case 3: cmsgtyp = MSG_CHANINVITED; break;
            case 4: cmsgtyp = MSG_CHANUNINVITED; break;
            }
        /* Tell everybody in this channel what we just did. */
        dispatch_message(cmsgtyp, "\0", sendto, 0, 0);
        /* Global messages store the current channel in the data1 field. */
        msgextradata = (long) curchannel;
        /* Send a global message to the user the action was done to, who
           could be anywhere in TOP. */
        msgsendglobal = 1;
        dispatch_message(cmsgtyp, "\0", sendto, 1, 0);
        /* Set the appropriate language item name to display. */
        switch (baninvflg)
            {
            case 1: strcpy(outbuf, "BannedUser"); break;
            case 2: strcpy(outbuf, "UnBannedUser"); break;
            case 3: strcpy(outbuf, "InvitedUser"); break;
            case 4: strcpy(outbuf, "UnInvitedUser"); break;
            }
        /* Confirm the action by displaying a message. */
        top_output(OUT_SCREEN, getlang(outbuf), handles[sendto].string);
        /* If we just banned or uninvited a user, they are no longer in
           our channel. */
        if (baninvflg == 1 || baninvflg == 4)
            {
            activenodes[sendto] = 2;
            }
        return;
        }

    /* Command not recognized, show help file instead. */
    show_helpfile("helpmod0");

    return;
    }

