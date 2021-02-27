/******************************************************************************
PROCMSGS.C   Process all incoming TOP messages.

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

This module processes all incoming TOP messages from other node.  Most
messages are handled directly in the process_messages() function, which
accounts for its enormous size.  For a detailed list of the actual information
needed by each message, see MESSAGES.TXT.
******************************************************************************/

#include "top.h"

/* process_messages() - Process all incoming TOP messages.
   Parameters:  delname - TRUE to delete the name prompt, otherwise FALSE.
   Returns:  Number of incoming messages processed.
*/
XINT process_messages(char delname)
{
/* Loop counter, number of messages processed, number of messages used
   (displayed), action flag, result code, delname override. */
XINT d, proc = 0, mused = 0, isaction = 0, res, dn;
msg_typ msgin; /* Incoming message buffer. */
/* File name buffer, Temporary short string buffer (usually used for gender-
   specific data in actions), test byte holder. */
char filnam[256], ttt[10], tstbit[2];
unsigned char allowbits = 0; /* Action token allow bits. */
/* Temporary long number holder.  It is named cdc because it was originally
   used for CyberDollar values with the games before they were removed. */
unsigned long cdc;
/* Incoming message file data.  No longer used. */
/* struct ffblk mchg; */
char privchatflag = 0; /* Flag if we are entering private chat. */
/* Unused game variables. */
//|poker_game_typ mgame;
//|char pokcount = 0;
//|XINT pokstackgame[100];
/* Flag if we need to return to the main channel, type of ban/invite
   action. */
char rettomain = 0, baninv = 0;
/* Flag if the last message was used.  It is an int because the number of
   the last message used (relative to mused) is needed, but it amounts to a
   flag.  The sole purpose of this variable is to prevent extra newlines
   in dual-window mode, which turned out to be a very involving process. */
XINT lastmsgused;
char nointerflg = 0; /* Flag if the Interrupt string has been sent. */

/*//|for (d = 0; d < 100; d++)
    {
    pokstackgame[d] = -1;
    }*///|

/* Normal chat mode always needs the message prefix (which is what
   lastmsgused essentially regulates), but dual window mode does not use it
   on the first message to prevent double-spacing. */
lastmsgused = !(user.pref1 & PREF1_DUALWINDOW);
/* If the door kit sends a time message, it can't be sent as a TOP message
   during message processing or it would throw TOP into an infinite loop. */
blocktimemsgs = 1;

/* Scan our byte in the CHGIDX to see if we have new messages. */
lseek(chgfil, (od_control.od_node), SEEK_SET);
rec_locking(REC_LOCK, chgfil, (od_control.od_node), 1L);
read(chgfil, tstbit, 1L);
rec_locking(REC_UNLOCK, chgfil, (od_control.od_node), 1L);

/* If the byte was FALSE, we have no new messages. */
if (!tstbit[0])
	{
    blocktimemsgs = 0;
    return 0;
    }

/* Clear the CHGIDX byte immediately. */
tstbit[0] = 0;
lseek(chgfil, (od_control.od_node), SEEK_SET);
rec_locking(REC_LOCK, chgfil, (od_control.od_node), 1L);
write(chgfil, tstbit, 1L);
rec_locking(REC_UNLOCK, chgfil, (od_control.od_node), 1L);

if (user.pref1 & PREF1_DUALWINDOW)
    {
    /* In dual window mode, restore the last known position in the
       lower (incoming messages) window. */
    // Different for AVT when I find out what the store/recv codes are.
    od_disp_emu("\x1B" "[u", TRUE);
    top_output(OUT_SCREEN, getlang("DWOutputPrefix"));
    }

/* Loop for each message indexed by the MIX file, processing if needed. */
for (d = 0, dn = delname; d < filelength(midxinfil); d++)
	{
    /* Read the byte in the MIX file. */
    res = lseek(midxinfil, (long) d, SEEK_SET);
    if (res == -1)
    	{
        continue;
        }
    rec_locking(REC_LOCK, midxinfil, d, 1L);
    res = read(midxinfil, tstbit, 1L);
    if (res == -1)
    	{
	    rec_locking(REC_UNLOCK, midxinfil, d, 1L);
        continue;
        }
	// This needs changing to a large, malloc()ed, buffered array to
    // improve speed
    if (!tstbit[0])
    	{
        /* If the byte is 0, ignore the message as it has been processed
           and is just leftover garbage. */
	    rec_locking(REC_UNLOCK, midxinfil, d, 1L);
        continue;
        }

    /* Read the actual message. */
    res = lseek(msginfil, (long) d * (long) sizeof(msg_typ), SEEK_SET);
    if (res == -1)
    	{
	    rec_locking(REC_UNLOCK, midxinfil, d, 1L);
        continue;
        }
    rec_locking(REC_LOCK, msginfil, (long) d * (long) sizeof(msg_typ),
    			(long) sizeof(msg_typ));
    res = read(msginfil, &msgin, sizeof(msg_typ));
    rec_locking(REC_UNLOCK, msginfil, (long) d * (long) sizeof(msg_typ),
    			(long) sizeof(msg_typ));
    if (res == -1)
    	{
	    rec_locking(REC_UNLOCK, midxinfil, d, 1L);
        continue;
        }
    /* The byte in the MIX file stays locked so other nodes don't write
       a message in this spot before it's cleared. */

    if (msgin.channel != curchannel && msgin.channel != 0)
    	{
        /* Ignore the message if it isn't global and isn't on our channel. */
        tstbit[0] = 0;
        lseek(midxinfil, (long) d, SEEK_SET);
        write(midxinfil, tstbit, 1L);
	    rec_locking(REC_UNLOCK, midxinfil, d, 1L);
        continue;
        }

    /* Do a little pre-processing preparation. */
    isaction = baninv = 0;
    fixname(msgin.handle, msgin.handle);

    /* At this time, some common steps in message processing will be
       explained so they do not have to be commented each time below.

       !(forgetstatus[msgin.from] & FGT_FORGOTTEN) tests to make sure the
           message isn't coming from someone that we've forgotten.  If it
           is, most messages won't be processed.  This has to be done for
           each message type because there are some messages that aren't
           to be ignored even if the user is forgotten.  These messages
           generally don't involve displaying anything on-screen.
       proc++ increases the number of messages processed, which includes
           ignored messages.
       mused++ increases the number of messages actually used, so it does
           not include ignored messages. */

    if (!dn && lastmsgused && (!forgetstatus[msgin.from] & FGT_FORGOTTEN))
    	{
        /* Essentially, if this is the first message to be shown and the
           user is using normal chat mode, a small string (usually ***) is
           sent to let the user know that their typing is being interrupted.
           This string is never sent if block while typing is on, but the
           test is included for completeness. */
        if (!(user.pref1 & PREF1_DUALWINDOW))
            {
            /* Only send the interrupt string if the user is in the process
               of typing something (onflag == TRUE). */
            if (!nointerflg && onflag && !(user.pref1 & PREF1_BLKWHILETYP))
                {
                top_output(OUT_SCREEN, getlang("Interrupt"));
                /* Don't send the interrupt string again. */
                nointerflg = 1;
                }
            }
        /* Send the message display prefix (usually just a newline). */
        top_output(OUT_SCREEN, getlang("MsgDisplayPrefix"));
        }

    /* The name prompt only has to be deleted once. */
    dn = 0;
    /* Many message types use a numeric value so it is obtained before
       processing begins. */
    cdc = strtoul(msgin.string, NULL, 10);

    /* The lastmsgused flag is figured from the mused total later on. */
    lastmsgused = mused;

    /* Process each type of message.  Most processing is straightforward
       (except for the common items covered above) so comments are kept
       to a minimum. */
	switch(msgin.type)
        {
        /* Normal text (talking). */
        case MSG_TEXT:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
            	{
                delprompt(delname);
                top_output(OUT_SCREEN, getlang("MsgPrefix"), msgin.handle,
                           msgin.string);
                mused++;
                }
            proc++;
            break;
        /* A user has entered TOP. */
        case MSG_ENTRY:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
                {
                delprompt(delname);
                if (cfg.allowexmessages && msgin.string[0])
                    {
                    /* Send the user's entry message if they have one. */
                    top_output(OUT_SCREEN, getlang("EMsgPrefix"),
                               msgin.string);
                    }
                else
                    {
                    /* Send the default entry message. */
                    top_output(OUT_SCREEN, getlang("EMessage"), msgin.handle);
                    }
                mused++;
                }
            proc++;
            /* Add the new user to the active nodes index and reset the
               forget data for the user's node. */
            fixname(handles[msgin.from].string, msgin.handle);
            activenodes[msgin.from] = 1;
            forgetstatus[msgin.from] = 0;
            break;
        /* A user has left TOP normally (with the QUIT command). */
        case MSG_EXIT:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
                {
                delprompt(delname);
                if (cfg.allowexmessages && msgin.string[0])
                    {
                    /* Send the user's exit message if they have one. */
                    top_output(OUT_SCREEN, getlang("XMsgPrefix"),
                               msgin.string);
                    }
                else
                    {
                    /* Send the default exit message. */
                    top_output(OUT_SCREEN, getlang("XMessage"), msgin.handle);
                    }
                mused++;
                }
            /* Reset private chat variables if they involve the node that is
               leaving. */
            if (privchatin == msgin.from) privchatin = -1;
            if (privchatout == msgin.from) privchatout = -1;
            activenodes[msgin.from] = 0;
            forgetstatus[msgin.from] = 0;
            /* If we are currently in the personal channel of the user that
               is leaving, we must return to the main channel. */
            if (curchannel ==
                ((unsigned long) msgin.from + 4000000000UL))
                {
                top_output(OUT_SCREEN, getlang("NoLongerOnSystem"),
                           handles[msgin.from].string);
                rettomain = 1;
                }
            proc++;
            break;
        /* A user entered the profile editor. */
        case MSG_INEDITOR:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
                {
                delprompt(delname);
                top_output(OUT_SCREEN, getlang("InProf"), msgin.handle);
                mused++;
                }
            /* Reset private chat variables if they involve the node that is
               going in the editor. */
            if (privchatin == msgin.from) privchatin = -1;
            if (privchatout == msgin.from) privchatout = -1;
            proc++;
            break;
        /* A user has returned from the profile editor. */
        case MSG_OUTEDITOR:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
                {
                delprompt(delname);
                top_output(OUT_SCREEN, getlang("OutProf"), msgin.handle);
                mused++;
                }
            proc++;
            break;
        /* Private text, just to us. */
        case MSG_WHISPER:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
                {
                delprompt(delname);
                top_output(OUT_SCREEN, getlang("WhisperPrefix"), msgin.handle,
                           msgin.string);
                mused++;
                }
            proc++;
            break;
        /* General action. */
        case MSG_GA:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN) &&
                !(user.pref2 & PREF2_ACTIONSOFF))
                {
                delprompt(delname);
                top_output(OUT_SCREEN, getlang("GAPrefix"), msgin.handle,
                           msgin.string);
                mused++;
                }
            proc++;
            break;
        /* Posessive general action (largely the same as a normal GA). */
        case MSG_GA2:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN) &&
                !(user.pref2 & PREF2_ACTIONSOFF))
                {
                delprompt(delname);
                top_output(OUT_SCREEN, getlang("GA2Prefix"), msgin.handle,
                           msgin.string);
                mused++;
                }
            proc++;
            break;

        /* Actions are handled commonly at the end of the processing loop.
           For now, flag the message as being an action and determine the
           % tokens to allow based on type. */

        /* Singular action verb. */
        case MSG_ACTIONSIN:
            isaction = 1;
            allowbits = ALLOW_ME | ALLOW_SEX | ALLOW_SLF | ALLOW_HESHE;
            proc++;
            break;
        /* Plural action ("done to" somebody). */
        case MSG_ACTIONPLU:
            isaction = 1;
            allowbits = ALLOW_ME | ALLOW_YOU | ALLOW_SEX | ALLOW_POS |
                        ALLOW_SLF | ALLOW_HESHE;
            proc++;
            break;
        /* Singular talktype verb. */
        case MSG_TLKTYPSIN:
            isaction = 1;
            allowbits = ALLOW_ME | ALLOW_SEX | ALLOW_SLF | ALLOW_HESHE;
            proc++;
            break;
        /* Plural talktype (contains text). */
        case MSG_TLKTYPPLU:
            isaction = 1;
            allowbits = ALLOW_ME | ALLOW_SEX | ALLOW_SLF | ALLOW_HESHE;
            proc++;
            break;
        /* Secret (private) plural action that was done to us. */
        case MSG_ACTPLUSEC:
            isaction = 1;
            allowbits = ALLOW_ME | ALLOW_YOU | ALLOW_SEX | ALLOW_POS |
                        ALLOW_SLF | ALLOW_HESHE;
            proc++;
            break;
        /* A user has changed their TOP handle. */
        case MSG_HANDLECHG:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
                {
                delprompt(delname);
                if (msgin.gender == 0)
                    {
                    strcpy(ttt, getlang("His"));
                    }
                if (msgin.gender == 1)
                    {
                    strcpy(ttt, getlang("Her"));
                    }
                fixname(msgin.string, msgin.string);
                top_output(OUT_SCREEN, getlang("HandleChangeMsg"),
                           handles[msgin.from].string, ttt, msgin.string);
                mused++;
                }
            /* Update the handle in the main handle index. */
            fixname(handles[msgin.from].string, msgin.string);
            proc++;
            break;
        /* A user has changed their biography. */
        case MSG_BIOCHG:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
                {
                delprompt(delname);
                if (msgin.gender == 0)
                    {
                    strcpy(ttt, getlang("His"));
                    }
                if (msgin.gender == 1)
                    {
                    strcpy(ttt, getlang("Her"));
                    }
                mused++;
                top_output(OUT_SCREEN, getlang("BioChangeMsg"), msgin.handle,
                           ttt);
                }
            proc++;
            break;
        /* A user has changed genders. */
        case MSG_SEXCHG:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
                {
                delprompt(delname);
                top_output(OUT_SCREEN, getlang("SexChangeMsg"), msgin.handle,
                           msgin.string);
                mused++;
                }
            proc++;
            break;
        /* A user has changed their personal actions. */
        case MSG_PERACTCHG:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
                {
                delprompt(delname);
                if (msgin.gender == 0)
                    {
                    strcpy(ttt, getlang("His"));
                    }
                if (msgin.gender == 1)
                    {
                    strcpy(ttt, getlang("Her"));
                    }
                top_output(OUT_SCREEN, getlang("PersActChangeMsg"), msgin.handle, ttt);
                mused++;
                }
            proc++;
            break;
        /* A user left TOP abruptly (carrier drop, time expired, etc.) */
        case MSG_TOSSOUT:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
                {
                delprompt(delname);
                top_output(OUT_SCREEN, getlang("TossOut"), msgin.handle);
                mused++;
                }
            /* The same post-processing is done as for a normal exit. */
            activenodes[msgin.from] = 0;
            forgetstatus[msgin.from] = 0;
            if (curchannel ==
                ((unsigned long) msgin.from + 4000000000UL))
                {
                top_output(OUT_SCREEN, getlang("NoLongerOnSystem"),
                           handles[msgin.from].string);
                rettomain = 1;
                }
            proc++;
            break;
        /* A user has just "forgotten" us. */
        case MSG_FORGET:
            /* Note that we have been forgotten, so we can tell the user
               later if they try to do something to this user. */
            forgetstatus[msgin.from] |= FGT_HASFORGOTME;
            /* Reset private chat variables. */
            if (privchatin == msgin.from) privchatin = -1;
            if (privchatout == msgin.from) privchatout = -1;
            proc++;
            break;
        /* A user has just "remembered" us. */
        case MSG_REMEMBER:
            /* Note that we are no longer forgotten by resetting the
               HasForgotMe bit. */
            forgetstatus[msgin.from] &= (255 - FGT_HASFORGOTME);
            proc++;
            break;
        // these 3 probably need forget mods later on.
        
        /* Although TOP itself currently does not have link support, it may
           in the future, which is why the three link messages were put in.
           However, the external TOPLink utility does in fact use the link
           messages so they should be supported. */

        /* A link has just been established to another BBS. */
        case MSG_LINKUP:
            delprompt(delname);
            top_output(OUT_SCREEN, getlang("LinkOn"), msgin.handle, msgin.string);
            strcpy(handles[msgin.from].string, msgin.string);
            mused++;
            proc++;
            break;
        /* Text received from a linked BBS. */
        case MSG_LINKTEXT:
            delprompt(delname);
            top_output(OUT_SCREEN, getlang("LinkPrefix"), msgin.handle, msgin.string);
            mused++;
            proc++;
            break;
        /* The link to another BBS has been disconnected. */
        case MSG_UNLINK:
            delprompt(delname);
            top_output(OUT_SCREEN, getlang("LinkOff"), msgin.handle);
            strcpy(handles[msgin.from].string, msgin.string);
            activenodes[msgin.from] = 0;
            forgetstatus[msgin.from] = 0;
            mused++;
            proc++;
            break;
        /* A user has changed their entry or exit message. */
        case MSG_EXMSGCHG:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
                {
                delprompt(delname);
                if (msgin.gender == 0)
                    {
                    strcpy(ttt, getlang("His"));
                    }
                if (msgin.gender == 1)
                    {
                    strcpy(ttt, getlang("Her"));
                    }
                top_output(OUT_SCREEN, getlang("EXMsgChange"), msgin.handle, ttt);
                mused++;
                }
            proc++;
            break;
        /* A sysop just tossed us back to the BBS. */
        case MSG_TOSSED:
            delprompt(delname);
            top_output(OUT_SCREEN, getlang("HasTossed"), msgin.handle);
            if (msgin.string[0])
            	{
                top_output(OUT_SCREEN, getlang("TossComment"), msgin.string);
                }
            tstbit[0] = 0;
		    lseek(midxinfil, (long) d, SEEK_SET);
		    write(midxinfil, tstbit, 1L);
		    rec_locking(REC_UNLOCK, midxinfil, d, 1L);
            od_exit(6, FALSE);
            /* Processing continues in case the exit fails. */
            proc++;
            mused++;
            break;
        /* A sysop disconnected us from TOP and the BBS. */
        case MSG_ZAPPED:
            delprompt(delname);
            top_output(OUT_SCREEN, getlang("HasZapped"), msgin.handle);
            if (msgin.string[0])
            	{
                top_output(OUT_SCREEN, getlang("TossComment"), msgin.string);
                }
            tstbit[0] = 0;
		    lseek(midxinfil, (long) d, SEEK_SET);
		    write(midxinfil, tstbit, 1L);
		    rec_locking(REC_UNLOCK, midxinfil, d, 1L);
            od_exit(7, TRUE);
            /* Processing continues in case the exit fails. */
            proc++;
            mused++;
            break;
        /* A sysop has reset a crashed node. */
        case MSG_CLEARNODE:
            delprompt(delname);
            itoa(cdc, outnum[0], 10);
            top_output(OUT_SCREEN, getlang("HasCleared"), outnum[0],
                       handles[(XINT) cdc].string, msgin.handle);
        	check_nodes_used(FALSE);
            mused++;
            proc++;
            break;
/* Sysop CyberCash transactions, not currently used. */
/*        case MSG_SYSGIVECD:
        	delprompt(delname);
            ultoa(cdc, outnum[0], 10);
            top_output(OUT_SCREEN, getlang("CDGive"), msgin.handle, outnum[0],
                       cdc == 1 ? getlang("CDSingular") :
                                  getlang("CDPlural"));
            user.cybercash += cdc;
            save_user_data(user_rec_num, &user);
            mused++;
            proc++;
            break;
        case MSG_SYSTAKECD:
        	delprompt(delname);
            ultoa(cdc, outnum[0], 10);
            top_output(OUT_SCREEN, getlang("CDTake"), msgin.handle, outnum[0],
                       cdc == 1 ? getlang("CDSingular") :
                                  getlang("CDPlural"));
            user.cybercash -= cdc;
            save_user_data(user_rec_num, &user);
            mused++;
            proc++;
            break;
        case MSG_SYSSETCD:
        	delprompt(delname);
            ultoa(cdc, outnum[0], 10);
            top_output(OUT_SCREEN, getlang("CDSet"), msgin.handle, outnum[0]);
            user.cybercash = cdc;
            save_user_data(user_rec_num, &user);
            mused++;
            proc++;
            break;*///!
/* A poker game message, currently unused. */
/*//|        case MSG_POKER:
            poker_core(msgin.data1, msgin.string);
            if (msgextradata == 1)
            	{
                pokstackgame[pokcount++] = msgin.data1;
                }
        	if (stricmp(msgin.string, "GameOver") &&
                stricmp(msgin.string, "DoRescan"))
                {
                mused++;
                }
            proc++;
            break;*///|
        /* Generic text (usually system messages) to be displayed verbatim. */
        case MSG_GENERIC:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
                {
	            delprompt(delname);
                top_output(OUT_SCREEN, msgin.string);
        	    mused++;
                }
            proc++;
            break;
        /* A user has joined our current channel. */
        case MSG_INCHANNEL:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
                {
	        	delprompt(delname);
                top_output(OUT_SCREEN, getlang("ChannelIn"),
                           msgin.handle);
        	    mused++;
                }
            proc++;
            break;
        /* A user has left our current channel. */
        case MSG_OUTCHANNEL:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
                {
	        	delprompt(delname);
                top_output(OUT_SCREEN, getlang("ChannelOut"),
                           msgin.handle);
    	        mused++;
                }
            proc++;
            break;
        /* Crash protection has detected a stuck node. */
        case MSG_KILLCRASH:
            res = atoi(msgin.string);
            delprompt(delname);
            top_output(OUT_SCREEN, getlang("CrashToss"),
                       msgin.string);
            /* Clear the node's variables so it does not remain visible. */
            activenodes[res] = 0;
            forgetstatus[res] = 0;
            mused++;
            proc++;
            break;
        /* Directed message (not necessarily to us). */
        case MSG_DIRECTED:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
                {
	        	delprompt(delname);
                top_output(OUT_SCREEN, getlang("DirectedPrefix"),
                		   handles[msgin.from].string,
                           msgin.doneto == od_control.od_node ?
						   getlang("You") : handles[msgin.doneto].string,
						   msgin.string);
                mused++;
                }
            proc++;
            break;
/* The sysop has edited the user on this node.  Not used. */
/*        case MSG_SYSUSEREDITED:
            delprompt(delname);
            top_output(OUT_SCREEN, getlang("SysopUserEdited"),
            		   msgin.string);
            load_user_data(user_rec_num, &user);
            mused++;
            proc++;
            break;*/
        /* The sysop has changed our security level. */
        case MSG_SYSSETSEC:
        	delprompt(delname);
            ultoa(cdc, outnum[0], 10);
            top_output(OUT_SCREEN, getlang("SecChangeMsg"),
                       msgin.handle, outnum[0]);
            /* Update that security level in the user and NODEIDX files. */
            user.security = cdc;
            save_user_data(user_rec_num, &user);
            node->security = cdc;
            save_node_data(od_control.od_node, node);
            mused++;
            proc++;
            break;
        /* A user has entered private chat mode. */
        case MSG_INPRIVCHAT:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
                {
	        	delprompt(delname);
                top_output(OUT_SCREEN, getlang("PrivChatIn"),
                           msgin.handle);
                mused++;
                }
            proc++;
            break;
        /* A user has returned from private chat mode. */
        case MSG_OUTPRIVCHAT:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
                {
	        	delprompt(delname);
                top_output(OUT_SCREEN, getlang("PrivChatOut"),
                           msgin.handle);
                mused++;
                }
            proc++;
            break;
        /* A user is requesting a private chat with us. */
        case MSG_WANTSPRIVCHAT:
            if (privchatout >= 0 && msgin.from == privchatout)
                {
                /* If we have previously requested to chat with the other
                   user, enter private chat mode after processing is done. */
                privchatflag = 1;
                }
            /* The other user initiated the chat request. */
            if (privchatin < 0 && privchatout < 0)
                {
                privchatin = msgin.from;
	        	delprompt(delname);
                top_output(OUT_SCREEN, getlang("PrivChatRequested"),
                           msgin.handle);
                }
            mused++;
            proc++;
            break;
        /* A user has denied this user's request for a private chat. */
        case MSG_DENYPRIVCHAT:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
                {
	        	delprompt(delname);
                top_output(OUT_SCREEN, getlang("PrivChatDenied"),
                           msgin.handle);
                privchatout = -1;
                mused++;
                }
            proc++;
            break;
        /* A moderator has changed our current channel's topic. */
        case MSG_CHANTOPICCHG:
            delprompt(delname);
            top_output(OUT_SCREEN, getlang("TopicChangeMsg"),
                       handles[msgin.from].string, msgin.string);
            strcpy(cmibuf.topic, msgin.string);
            mused++;
            proc++;
            break;
        /* BAN, UNBAN, INVITE, and UNINVITE commands from a channel
           moderator, not necessarily done to us.  They are processed
           collectively later. */
        case MSG_CHANBANNED:
        case MSG_CHANUNBANNED:
        case MSG_CHANINVITED:
        case MSG_CHANUNINVITED:
            baninv = (msgin.type - MSG_CHANBANNED) + 1;
            mused++;
            proc++;
            break;
        /* Another user (which could be us) has been made the moderator
           of our current channel. */
        case MSG_CHANMODCHG:
            delprompt(delname);
            top_output(OUT_SCREEN, getlang("ModChangeMsg"),
                       handles[msgin.from].string,
                       msgin.doneto == od_control.od_node ?
                           getlang("You") : handles[msgin.doneto].string);
            cmibuf.modnode = msgin.doneto;
            mused++;
            proc++;
            break;
        /* Force an update of the active nodes index.  Not currently used
           by TOP or any known utility. */
        case MSG_FORCERESCAN:
            check_nodes_used(FALSE);
            proc++;
            break;
        /* A user has entered the biography editor. */
        case MSG_INBIOEDITOR:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
                {
                delprompt(delname);
                top_output(OUT_SCREEN, getlang("InBioEditor"),
                           msgin.handle);
                mused++;
                }
            if (privchatin == msgin.from) privchatin = -1;
            if (privchatout == msgin.from) privchatout = -1;
            proc++;
            break;
        /* A user has returned from the biography editor. */
        case MSG_OUTBIOEDITOR:
            if (!(forgetstatus[msgin.from] & FGT_FORGOTTEN))
                {
                delprompt(delname);
                top_output(OUT_SCREEN, getlang("OutBioEditor"),
                           msgin.handle);
                mused++;
                }
            proc++;
            break;
        }

    /* Handle incoming actions. */
    if (isaction && !(forgetstatus[msgin.from] & FGT_FORGOTTEN) &&
        !(user.pref2 & PREF2_ACTIONSOFF))
        {
        /* Action text buffer, pictorial action buffer, pictorial action
           extension buffer, final action text pointer, temporary character
           exchange holder. */
        char *tmp = NULL, *buf = NULL, texbuf[5], *str = NULL, xch;
        /* Counter, flag when a % is incountered, flag to process % tokens,
           flag if we're using a dynamic buffer, end of string marker. */
        XINT c, nxt = 0, proctokon = 1, dynam, tmark = 0;

        dynam = 0;
        /* Immediately determine if this is a pictorial action.  Pictorial
           actions require a different setup because of their larger size. */
        if (!strnicmp(msgin.string, "%P", 2))
            {
            FILE *fil = NULL; /* Pictorial action file stream. */

            /* Although it was omitted from the TOP docs, it is possible to
               specify a path along with a pictorial action file base name. */
            if (
#ifndef __unix__
				strchr(&msgin.string[2], ':') == NULL &&
#endif
                strchr(&msgin.string[2], PATH_DELIM) == NULL)
                {
                /* No path characters found, so assume the TOP ANSI Path. */
                sprintf(outbuf, "%s%s", cfg.topansipath, &msgin.string[2]);
                }
            else
                {
                /* Use the path as is. */
                strcpy(outbuf, &msgin.string[2]);
                }
            get_extension(outbuf, texbuf);
            strcat(outbuf, texbuf);

            /* Open the pictorial action file. */
            fil = fopen(outbuf, "rb");
            if (!fil)
                {
                // Err msg
                }
            /* An extra byte is needed because the entire file is treated
               as one big string. */
            buf = malloc(filelength(fileno(fil)) + 1);
            if (buf != NULL)
                {
                /* Read in the entire file. */
                memset(buf, 0, filelength(fileno(fil)) + 1);
                dynam = 1;
                rewind(fil);
                fread(buf, 1, filelength(fileno(fil)), fil);
                }
            fclose(fil);
            }

        /* Allocate a larger temporary buffer to hold the translated output
           text. */
        if (!dynam || buf != NULL)
            {
            /* 1000 extra bytes are allocated to minimize chances of a
               buffer overrun due to excessive % tokens. */
            tmp = malloc(dynam ?
                    (strlen(buf) + 1000) : (strlen(msgin.string) + 1000));
            }
        if (tmp != NULL)
            {
            memset(tmp, 0,
                (dynam ? (strlen(buf) + 1000) : (strlen(msgin.string) + 1000)));
            delprompt(delname);
            mused++;
            /* Assign a common string pointer to whatever buffer we are
               using, for manageability. */
            if (dynam)
                {
                str = buf;
                }
            else
                {
                str = msgin.string;
                }

            /* Loop for each character in the buffer. */
            for (c = 0; c < (dynam ? strlen(buf) : strlen(msgin.string));
                 c++)
                {
                if (msgin.type == MSG_TLKTYPPLU && c == msgin.data1)
                    {
                    /* Disable token processing once we reach the user's
                       text in a talktype, so they can't wreak havoc with
                       extra % codes. */
                    proctokon = 0;
                    }
                if (nxt)
                    {
                    /* The previous character was a %, so we expect a
                       token character. */

                    nxt = 2;

                    /* For token explanations, see TOP.DOC. */

                    /* "Me" token. */
                    if (toupper(str[c]) == 'M' &&
                        (allowbits & ALLOW_ME))
                        {
                        tmark = strlen(tmp);
                        strcat(tmp, top_output(OUT_STRINGNF,
                                               getlang("ActionMe"),
                                               msgin.handle));
                        /* Adjust the start of user text (normally used
                           in talktypes). */
                        msgin.data1 -= 2L;
                        msgin.data1 += (long) strlen(tmp) - (long) tmark;
                        }
                    /* "You" token. */
                    if (toupper(str[c]) == 'Y' &&
                        (allowbits & ALLOW_YOU))
                        {
                        tmark = strlen(tmp);
                        strcat(tmp, top_output(OUT_STRINGNF,
                                               getlang("ActionYouPrefix")));
                        if (msgin.doneto == od_control.od_node)
                            {
                            strcat(tmp, getlang("You"));
                            }
                        if (msgin.doneto == -2)
                            {
                            strcat(tmp, getlang("All"));
                            }
                        if ((msgin.doneto >= 0) &&
                            (msgin.doneto != od_control.od_node))
                            {
                            strcat(tmp, handles[msgin.doneto].string);
                            }
                        strcat(tmp, top_output(OUT_STRINGNF,
                                               getlang("ActionYouSuffix")));
                        /* Adjust the start of user text (normally used
                           in talktypes). */
                        msgin.data1 -= 2L;
                        msgin.data1 += (long) strlen(tmp) - (long) tmark;
                        }
                    /* "His/Her" token. */
                    if (toupper(str[c]) == 'H' &&
                        (allowbits & ALLOW_SEX))
                        {
                        tmark = strlen(tmp);
                        strcat(tmp, top_output(OUT_STRINGNF,
                                               getlang("ActionHisHerPrefix")));
                        if (msgin.gender == 0)
                            {
                            strcat(tmp, getlang("His"));
                            }
                        if (msgin.gender == 1)
                            {
                            strcat(tmp, getlang("Her"));
                            }
                        strcat(tmp, top_output(OUT_STRINGNF,
                                               getlang("ActionHisHerSuffix")));
                        /* Adjust the start of user text (normally used
                           in talktypes). */
                        msgin.data1 -= 2L;
                        msgin.data1 += (long) strlen(tmp) - (long) tmark;
                        }
                    /* "He/She" token. */
                    if (toupper(str[c]) == 'E' &&
                        (allowbits & ALLOW_HESHE))
                        {
                        tmark = strlen(tmp);
                        strcat(tmp, top_output(OUT_STRINGNF,
                                               getlang("ActionHeShePrefix")));
                        if (msgin.gender == 0)
                            {
                            strcat(tmp, getlang("He"));
                            }
                        if (msgin.gender == 1)
                            {
                            strcat(tmp, getlang("She"));
                            }
                        strcat(tmp, top_output(OUT_STRINGNF,
                                               getlang("ActionHeSheSuffix")));
                        /* Adjust the start of user text (normally used
                           in talktypes). */
                        msgin.data1 -= 2L;
                        msgin.data1 += (long) strlen(tmp) - (long) tmark;
                        }
                    /* "Himself/Herself" token. */
                    if (toupper(str[c]) == 'F' &&
                        (allowbits & ALLOW_SLF))
                        {
                        tmark = strlen(tmp);
                        strcat(tmp, top_output(OUT_STRINGNF,
                                               getlang("ActionSelfPrefix")));
                        if (msgin.gender == 0)
                            {
                            strcat(tmp, getlang("Himself"));
                            }
                        if (msgin.gender == 1)
                            {
                            strcat(tmp, getlang("Herself"));
                            }
                        strcat(tmp, top_output(OUT_STRINGNF,
                                               getlang("ActionSelfSuffix")));
                        /* Adjust the start of user text (normally used
                           in talktypes). */
                        msgin.data1 -= 2L;
                        msgin.data1 += (long) strlen(tmp) - (long) tmark;
                        }
                    /* Possessive token. */
                    if (toupper(str[c]) == 'S' &&
                        (allowbits & ALLOW_POS))
                        {
                        tmark = strlen(tmp);
                        if (msgin.doneto == od_control.od_node)
                            {
                            strcat(tmp, top_output(OUT_STRINGNF,
                                            getlang("ActionYour"),
                                            getlang("YourSuffix")));
                            }
                        else
                            {
                            strcat(tmp, top_output(OUT_STRINGNF,
                                            getlang("ActionPossessive"),
                                            getlang("Pers3Suffix")));
                            }
                        /* Adjust the start of user text (normally used
                           in talktypes). */
                        msgin.data1 -= 2L;
                        msgin.data1 += (long) strlen(tmp) - (long) tmark;
                        }
                    }
                if (nxt == 0)
                    {
                    if (str[c] == '%' && proctokon)
                        {
                        /* Flag the next loop that a % was found. */
                        nxt = 1;
                        }
                    else
                        {
                        /* Copy the character as is. */
                        strncat(tmp, &str[c], 1);
                        }
                    }
                /* Reset the next flag if it was used on this loop. */
                if (nxt == 2)
                    {
                    nxt = 0;
                    }
                }

            /* Append the user text if this is a plural talktype. */
            if (msgin.type == MSG_TLKTYPPLU)
                {
                /* Store the first character of user text and replace it
                   with a \0 so the action text can be printed. */
                xch = tmp[msgin.data1];
                tmp[msgin.data1] = '\0';
                top_output(dynam ? OUT_EMULATE : OUT_SCREEN, tmp);
                /* Restore the user text and print it with language code
                   translation turned off. */
                tmp[msgin.data1] = xch;
                top_output(dynam ? OUT_EMULATE : OUT_SCREEN, "@1&L",
                           &tmp[msgin.data1]);
                }
            else
                {
                top_output(dynam ? OUT_EMULATE : OUT_SCREEN, tmp);
                }
            /* Secret action. */
            if (msgin.type == MSG_ACTPLUSEC)
                {
                top_output(OUT_SCREEN, getlang("Secretly"));
                }
            dofree(tmp);
            }
        if (dynam)
            {
            dofree(buf);
            }
        }

    /* BAN, UNBAN, INVITE, and UNINVITE commands. */
    if (baninv > 0)
        {
        delprompt(delname);

        /* These messages come in globally if the command affects us,
           otherwise they come in on the current channel as a notification. */
        if (msgin.channel != 0)
            {
            /* Notification. */

            /* Copy the right language item name based on the message. */
            switch (baninv)
                {
                case 1: strcpy(outbuf, "HasBeenBanned"); break;
                case 2: strcpy(outbuf, "HasBeenUnBanned"); break;
                case 3: strcpy(outbuf, "HasBeenInvited"); break;
                case 4: strcpy(outbuf, "HasBeenUnInvited"); break;
                }
            top_output(OUT_SCREEN, getlang(outbuf),
                       handles[msgin.from].string,
                       msgin.doneto == od_control.od_node ?
                           getlang("You") : handles[msgin.doneto].string);
            /* BAN and INVITE add the node to the special nodes list. */
            if (baninv == 1 || baninv == 3)
                {
                cmi_setspec(msgin.doneto, 1);
                }
            /* UNBAN and UNINVITE remove the node from the special nodes
               list. */
            if (baninv == 2 || baninv == 4)
                {
                cmi_setspec(msgin.doneto, 0);
                }
            if (baninv == 1 || baninv == 4)
                {
                /* If a BAN or UNINVITE command was done to us and we
                   received the notification, it means we are being thrown
                   out of our current channel, so we have to return the
                   user to the main channel. */
                if (msgin.doneto == od_control.od_node)
                    {
                    rettomain = 1;
                    }
                }
            }
        else
            {
            /* Global message that affects us. */

            /* Select the right language item. */
            /* A switch was not used here because compiler didn't like it
               for some reason, possibly due to the enormous switch used
               for message processing. */
            if (baninv == 1)
                {
                if ((unsigned long) msgin.data1 <= 3999999999UL)
                    strcpy(outbuf, "YouBannedChan");
                if ((unsigned long) msgin.data1 >= 4000000000UL &&
                    (unsigned long) msgin.data1 <= 4000999999UL)
                    strcpy(outbuf, "YouBannedUserChan");
                if ((unsigned long) msgin.data1 >= 4001000000UL)
                    strcpy(outbuf, "YouBannedConf");
                }
            if (baninv == 2)
                {
                if ((unsigned long) msgin.data1 <= 3999999999UL)
                    strcpy(outbuf, "YouUnBannedChan");
                if ((unsigned long) msgin.data1 >= 4000000000UL &&
                    (unsigned long) msgin.data1 <= 4000999999UL)
                    strcpy(outbuf, "YouUnBannedUsChan");
                if ((unsigned long) msgin.data1 >= 4001000000UL)
                    strcpy(outbuf, "YouUnBannedConf");
                }
            if (baninv == 3)
                {
                if ((unsigned long) msgin.data1 <= 3999999999UL)
                    strcpy(outbuf, "YouInvitedChan");
                if ((unsigned long) msgin.data1 >= 4000000000UL &&
                    (unsigned long) msgin.data1 <= 4000999999UL)
                    strcpy(outbuf, "YouInvitedUsChan");
                if ((unsigned long) msgin.data1 >= 4001000000UL)
                    strcpy(outbuf, "YouInvitedConf");
                }
            if (baninv == 4)
                {
                if ((unsigned long) msgin.data1 <= 3999999999UL)
                    strcpy(outbuf, "YouUnInvitedChan");
                if ((unsigned long) msgin.data1 >= 4000000000UL &&
                    (unsigned long) msgin.data1 <= 4000999999UL)
                    strcpy(outbuf, "YouUnInvitedUsCh");
                if ((unsigned long) msgin.data1 >= 4001000000UL)
                    strcpy(outbuf, "YouUnInvitedConf");
                }
            top_output(OUT_SCREEN, getlang(outbuf),
                       channelname((unsigned long) msgin.data1));
            }
        }

    /* Determine if the message just processed was actually used. */
    lastmsgused = mused - lastmsgused; /* TRUE if last used, FALSE if not. */

    /* Clear the bit for this message in the MIX file and finally unlock
       it. */
    tstbit[0] = 0;
    lseek(midxinfil, (long) d, SEEK_SET);
    write(midxinfil, tstbit, 1L);
    rec_locking(REC_UNLOCK, midxinfil, d, 1L);
    }

/* It is now safe for door kit time messages to be treated as TOP messages. */
blocktimemsgs = 0;

/* Some poker post-processing which is not used. */
/*//|for (res = 0; res < pokcount; res++)
	{
    poker_lockgame(pokstackgame[res]);
    poker_loadgame(pokstackgame[res], &mgame);
    poker_unlockgame(pokstackgame[res]);

    poker_sendmessage(MSG_POKER, 1, pokstackgame[res], 1, "Finished",
					  &mgame);
    }*///|

/* Enter private chat if needed. */
if (privchatflag)
    {
    dispatch_message(MSG_INPRIVCHAT, user.handle, -1, 0, 0);
    node->task |= TASK_PRIVATECHAT;
    save_node_data(od_control.od_node, node);
    cmi_busy();
    privatechat(privchatout);
    cmi_unbusy();
    node->task = node->task & (0xFF - TASK_PRIVATECHAT);
    save_node_data(od_control.od_node, node);
    dispatch_message(MSG_OUTPRIVCHAT, user.handle, -1, 0, 0);
    }

/* Return to main channel if needed. */
if (rettomain)
    {
    top_output(OUT_SCREEN, getlang("ReturnToMain"));
    /* Leave the current channel.  Even though the user is likely being
       forced out, it is actually voluntary as to whether TOP complies
       with the request to remove the user.  Obviously, this should always
       be done to prevent confusion for both other users and possibly TOP
       itself. */
    res = cmi_unjoin(curchannel);
    if (res != CMI_OKAY)
        {
        top_output(OUT_SCREEN, getlang("UnJoinError"));
        }
    /* Join the main channel. */
    res = cmi_join(cfg.defaultchannel);
    if (res != CMI_OKAY)
        {
        /* Join our personal channel if for some reason the main channel
           cannot be joined.  This likely means the CMI is corrupt. */
        res = cmi_join(4000000000UL + (unsigned long) od_control.od_node);
        if (res != CMI_OKAY)
            {
            top_output(OUT_SCREEN, getlang("SevereJoinError"));
            od_exit(230, FALSE);
            }
        else
            {
            curchannel = 4000000000UL + (unsigned long) od_control.od_node;
            }
        }
    node->channel = curchannel = cfg.defaultchannel;
    save_node_data(od_control.od_node, node);
    dispatch_message(MSG_INCHANNEL, "\0", -1, 0, 0);
    channelsummary();
    }

if (delname && mused > 0 && lastmsgused)
	{
    top_output(OUT_SCREEN, getlang("MessageTrailer"));
    }

if (user.pref1 & PREF1_DUALWINDOW)
    {
    /* "Reactivate" the upper window in dual window mode. */
    if (onflag)
        {
        top_output(OUT_SCREEN, getlang("DWRestorePrefix"));
        }
    // Different for AVT when I find out what the store/recv codes are.
    od_disp_emu("\x1B" "[s", TRUE);

    }

/* In dual window mode, TOP prepares the screen for a message before it is
   known if the message will actually be used.  Therefore, if no messages
   were used but at least one was processed, we have to fool the calling
   function into thinking at least one message was used so it will restore
   the screen. */
if (mused == 0 && (user.pref1 & PREF1_DUALWINDOW) && proc > 0)
    {
    mused = 1;
    }

return mused;
}

/* delprompt() - Deletes the name prompt.
   Parameters:  dname - Flag whether or not to delete the name (see notes).
   Return:  Nothing.
   Notes:  The dname parameter essentially controls whether or not this
           function actually does anything.  If it is TRUE, TOP will delete
           the name prompt as planned.  If it is FALSE, TOP will do nothing.
           This parameter is needed because the function can be called
           several times during message processing, but the name prompt
           only needs to be deleted once, and attempting to delete it
           multiple times would mess up the screen.
*/
void delprompt(char dname)
{
XINT dd; /* Counter. */

if (dname)
    {
    if (user.pref1 & PREF1_DUALWINDOW)
        {
        /* At the moment, no action is taken under dual window mode. */
 //       top_output(OUT_SCREEN, getlang("DWClearInput"));
        }
    else
        {
        top_output(OUT_SCREEN, getlang("DelPromptPrefix"));
        /* Backspace out the name prompt. */
        for (dd = 0;
             dd < strlen(top_output(OUT_STRING, getlang("DefaultPrompt"),
                                    user.handle)); dd++)
            {
            top_output(OUT_SCREEN, getlang("DelPrompt"));
            }
        top_output(OUT_SCREEN, getlang("DelPromptSuffix"));
        }
    }

}
