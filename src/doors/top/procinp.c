/******************************************************************************
PROCINP.C    Process all TOP commands and input.

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

This module processes all TOP commands.  Many are handled right in the
process_input() function, which accounts for its size.  Others (usually
multi-word commands) are handled by separate processors in other modules which
are called from this module.
******************************************************************************/

#include "top.h"

/* This macro is used to determine where the name starts in a whisper or
   directed message.  If there is a space at the start of the string, then
   shorthand mode was used (a single character).  Otherwise, longhand mode
   was used (two full words). */
#define spacecheck(xxxx) xxxx[0] == ' ' ? &xxxx[1] : &xxxx[0]

/* process_input() - Process all TOP commands and input.
   Parameters:  string - Input string to process.
   Returns:  Nothing.
*/
void process_input(unsigned char *string)
{
/* Shorthand flags for message sent and echo GA preferences. */
char msgsent = 1, echoga = 1;
/* Private node to receive the message (not used), number of words in input
   string, counter, command length holder, result code. */
XINT priv = -1, numwds, z, cmdlen = 0, res;
/* Action used flag, no longer used.  Was used with old single-list format. */
// char auflg = 0;
/* Node the message is destined for ("done to"). */
XINT sendto;
/* Command text holder, command handle holder. */
unsigned char tmpstr[256], tmphand[256]; // Dynam with MAXSTRLEN!

/* Check incoming messages before processing.  This results in a more
   realistic order for items to be displayed on screen. */
process_messages(FALSE);
/* Refresh active nodes. */
check_nodes_used(FALSE);

/* Prepare the screen for a command response. */
if (user.pref1 & PREF1_DUALWINDOW)
	{
	// Different for AVT when I find out what the store/recv codes are.
	od_disp_emu("\x1B" "[u", TRUE);
	top_output(OUT_SCREEN, getlang("DWOutputPrefix"));
	}
else
	{
	top_output(OUT_SCREEN, getlang("ResponsePrefix"));
	}

/* Set preference aliases. */
msgsent = user.pref1 & PREF1_MSGSENT;
echoga = user.pref1 & PREF1_ECHOGA;

/* Censor the string. */
if (numcensorwords > 0)
	{
	if (censorinput(string))
		{
        /* The censor blocked the input, so don't process it. */
		return;
		}
    }

/* Split the string into words. */
numwds = split_string(string);

/* Most commands are self-explanatory and simply call other functions to
   respond to the command in detail. */

/* QUIT command (exit TOP). */
if (checkcmdmatch(get_word(0), getlang("CmdsQuit")) > 0 &&
	numwds == 1)
	{
	dispatch_message(MSG_EXIT, user.xmessage, -1, 0, 0);
	unregister_node(1);
	quit_top();
	return;
	}
/* COMMANDS command (short list of all commands). */
if (checkcmdmatch(get_word(0), getlang("CmdsCommands")) > 0 &&
	numwds == 1)
	{
	minihelp();
	return;
	}
/* HELP command. */
if (checkcmdmatch(get_word(0), getlang("CmdsHelp")) > 0)
	{
	help_proc(numwds);
	return;
	}
/* COLOURHELP command (PubColour code list). */
if (checkcmdmatch(get_word(0), getlang("CmdsColourHelp")) > 0 &&
	numwds == 1)
	{
	top_output(OUT_SCREEN, getlang("ColourHelpPrefix"));
	show_file("colhelp", SCRN_NONE);
	return;
	}
/* ACTION multi-word command. */
if (checkcmdmatch(get_word(0), getlang("CmdsAction")) > 0)
	{
	if (!cfg.allowactions)
		{
		top_output(OUT_SCREEN, getlang("ActionsNotAllowed"));
		return;
		}
	if (user.security < cfg.actionusesec)
		{
		top_output(OUT_SCREEN, getlang("NoAccActUse"));
		return;
		}
	action_proc(numwds);
	return;
	}
/* TIME command. */
if (checkcmdmatch(get_word(0), getlang("CmdsTime")) > 0 &&
	numwds == 1)
	{
	show_time();
	return;
	}
/* SCAN or WHO command (show who's in each channel). */
if (checkcmdmatch(get_word(0), getlang("CmdsWho")) > 0 &&
	numwds == 1)
	{
	scan_pub();
	return;
	}
/* CDTOTAL command.  Not used, it was used with the games and was left in
   for future upgradability.  It could also be used with BJ4TOP if BJ4TOP
   stored the cybercash in the TOP user file, as there is already space
   reserved in the file for a cybercash total. */
if (checkcmdmatch(get_word(0), getlang("CmdsCDTotal")) > 0 &&
	numwds == 1)
	{
	ultoa(user.cybercash, outnum[0], 10);
	top_output(OUT_SCREEN, getlang("CDTotal"), outnum[0],
			   user.cybercash == 1 ? getlang("CDSingular") :
									 getlang("CDPlural"));
	return;
	}
/* PROFILE command (enter the profile editor). */
if (checkcmdmatch(get_word(0), getlang("CmdsProfile")) > 0 &&
	numwds == 1)
	{
	node->task |= TASK_EDITING;
	save_node_data(od_control.od_node, node);
	dispatch_message(MSG_INEDITOR, user.handle, -1, 0, 0);
	cmi_busy();
	profile_editor();
	cmi_unbusy();
	node->task = node->task & (0xFF - TASK_EDITING);
	save_node_data(od_control.od_node, node);
	dispatch_message(MSG_OUTEDITOR, user.handle, -1, 0, 0);
	return;
	}
/* BIO command (enter the biography editor). */
if (checkcmdmatch(get_word(0), getlang("CmdsBio")) > 0 &&
    numwds == 1)
	{
	node->task |= TASK_EDITING;
	save_node_data(od_control.od_node, node);
	dispatch_message(MSG_INBIOEDITOR, user.handle, -1, 0, 0);
    cmi_busy();
    bioeditor();
    cmi_unbusy();
	node->task = node->task & (0xFF - TASK_EDITING);
	save_node_data(od_control.od_node, node);
	dispatch_message(MSG_OUTBIOEDITOR, user.handle, -1, 0, 0);
	return;
	}
/* LOOKUP command (view a user's biography. */
if (checkcmdmatch(get_word(0), getlang("CmdsLookUp")) > 0)
	{
/* Old-style (pre-bio) lookup that showed all users. */
/*    if (numwds == 1)
		{
		lookup("\0");
		return;
		}
	lookup(&word_str[word_pos[1]]);*/
    /* LOOKUP needs a user name. */
    if (numwds == 1)
		{
		show_helpfile("helplkup");
		return;
		}
	if (displaybio(&word_str[word_pos[1]]))
		{
		top_output(OUT_SCREEN, getlang("LookUpSuffix"));
		}
	return;
	}
/* USERLIST command (show list of all TOP users). */
if (checkcmdmatch(get_word(0), getlang("CmdsUserList")) > 0)
	{
	userlist();
	return;
	}
/* /LOGOFF command (quit TOP and hang up immediately). */
if (checkcmdmatch(get_word(0), getlang("CmdsLogOff")) > 0 &&
	numwds == 1)
	{
	top_output(OUT_SCREEN, getlang("DirectLogOffMsg"));
	dispatch_message(MSG_EXIT, user.xmessage, -1, 0, 0);
	unregister_node(FALSE);
	od_exit(8, TRUE);
	return;
	}
/* CHAT command (enter private chat mode). */
if (checkcmdmatch(get_word(0), getlang("CmdsPrivChat")) > 0)
	{
    /* CHAT needs a name. */
    if (numwds > 1)
		{
        /* Determine the node of the user to chat with. */
        sendto = find_node_from_name(tmpstr, tmphand,
									 &word_str[word_pos[1]]);

		if (sendto == -1)
			{
			top_output(OUT_SCREEN, getlang("NotLoggedIn"), tmphand);
			}
		if (sendto == -2)
			{
			top_output(OUT_SCREEN, getlang("NotSpecific"), tmphand);
			}
		if (sendto == -3)
			{
			top_output(OUT_SCREEN, getlang("HasForgotYou"),
					   handles[lastsendtonode].string);
			}
        /* CHAT can be done across channels. */
        if (sendto == -4)
			{
			sendto = lastsendtonode;
			}
        /* Can't chat with yourself (it would be a nightmare technically). */
        if (sendto == od_control.od_node)
			{
			top_output(OUT_SCREEN, getlang("CantDoCmdToSelf"));
			return;
			}
		if (sendto >= 0)
			{
            if (privchatin >= 0 && privchatin != sendto)
				{
                /* If somebody else is waiting to chat with this node,
                   the user can't request a chat. */
				top_output(OUT_SCREEN, getlang("PrivChatCantReq1"),
						   handles[privchatin].string);
				return;
				}
			if (privchatout >= 0)
				{
                /* If we've already requested to chat with somebody, we
                   can't request another chat. */
				top_output(OUT_SCREEN, getlang("PrivChatCantReq2"),
						   handles[privchatout].string);
				return;
				}

			if (privchatin == sendto)
				{
                /* Answer a chat request from another node, which engages
                   private chat mode. */

                /* Notify everybody else in the channel that we are going
                   into private chat mode. */
                dispatch_message(MSG_INPRIVCHAT, user.handle, -1, 0, 0);
                /* Acknowledge the private chat. */
                msgsendglobal = 1;
				dispatch_message(MSG_WANTSPRIVCHAT, user.handle,
								 privchatin, 1, 0);
				node->task |= TASK_PRIVATECHAT;
				save_node_data(od_control.od_node, node);
				cmi_busy();
				privatechat(privchatin);
				cmi_unbusy();
				node->task = node->task & (0xFF - TASK_PRIVATECHAT);
				save_node_data(od_control.od_node, node);
				dispatch_message(MSG_OUTPRIVCHAT, user.handle, -1, 0, 0);
				}
			else
				{
                /* We are sending a new request to the other node. */
				msgsendglobal = 1;
				dispatch_message(MSG_WANTSPRIVCHAT, user.handle,
								 sendto, 1, 0);
				top_output(OUT_SCREEN, getlang("PrivChatReqOK"),
						   handles[sendto].string);
				privchatout = sendto;
				}
			}
		return;
		}
	}
/* DENYCHAT command (denies a user's private chat request. */
if (checkcmdmatch(get_word(0), getlang("CmdsPrivDenyChat")) > 0 &&
	numwds == 1)
	{
	if (privchatin >= 0)
		{
        /* Deny a request if we have one. */
        msgsendglobal = 1;
		dispatch_message(MSG_DENYPRIVCHAT, user.handle, privchatin, 1, 0);
		top_output(OUT_SCREEN, getlang("PrivChatDenyOK"),
				   handles[privchatin].string);
		privchatin = -1;
		}
	else
		{
        /* No request to deny. */
		top_output(OUT_SCREEN, getlang("PrivChatCantDeny"));
		}
	return;
	}
/* NODES command (show who's on the entire BBS). */
if (checkcmdmatch(get_word(0), getlang("CmdsNodes")) > 0 &&
	numwds == 1)
	{
    /* This command only works if BBS interfacing is on. */
    if (cfg.bbstype == BBS_UNKNOWN)
		{
		top_output(OUT_SCREEN, getlang("CmdNotAvail"));
		return;
		}
	bbs_useron(TRUE);
	return;
	}
/* PAGE command (send a message to a user elsewhere on the BBS). */
if (checkcmdmatch(get_word(0), getlang("CmdsPage")) > 0)
	{
    XINT pnode = -1; /* Node to page. */
    char *pmsg = NULL; /* Text to page the node with. */

    /* This command only works if BBS interfacing is on. */
	if (cfg.bbstype == BBS_UNKNOWN)
		{
		top_output(OUT_SCREEN, getlang("CmdNotAvail"));
		return;
		}
    /* The user specified the node number with the command. */
    if (numwds > 1)
        {
        pnode = atoi(get_word(1));
        if (pnode < 1)
            {
            itoa(cfg.maxnodes - 1, outnum[0], 10);
            top_output(OUT_SCREEN, getlang("InvalidNode"), outnum[0]);
            return;
            }
        }
    /* The user specified text with the command. */
    if (numwds > 2)
        {
        pmsg = &word_str[word_pos[2]];
        }
    bbs_page(pnode, pmsg);
	return;
	}
/* FORGET command (ignores a user). */
if (cfg.allowforgetting && user.security >= cfg.forgetsec &&
	checkcmdmatch(get_word(0), getlang("CmdsForget")) > 0)
	{
    /* FORGET needs a name. */
    if (numwds > 1)
		{
        node_idx_typ fgtchk; /* Data for node to forget. */

        /* Find node of user to forget. */
        sendto = find_node_from_name(tmpstr, tmphand,
									 &word_str[word_pos[1]]);

        /* FORGET is channel-independent and does not care if the user
           has already forgotten us. */
        if (sendto == -3 || sendto == -4)
			{
			sendto = lastsendtonode;
			}

		if (sendto == -1)
			{
			top_output(OUT_SCREEN, getlang("NotLoggedIn"), tmphand);
			}
		if (sendto == -2)
			{
			top_output(OUT_SCREEN, getlang("NotSpecific"), tmphand);
			}
        /* Forgetting yourself was causing too many technical problems
           so it is restricted to other users. */
        if (sendto == od_control.od_node)
			{
			top_output(OUT_SCREEN, getlang("CantDoCmdToSelf"));
			return;
			}
		get_node_data(sendto, &fgtchk);
        /* Can't forget a user with sysop security. */
        if (fgtchk.security >= cfg.sysopsec)
			{
			top_output(OUT_SCREEN, getlang("CantDoCmdToSysop"));
			return;
			}
		if (sendto >= 0)
			{
			forgetstatus[sendto] |= FGT_FORGOTTEN;
			msgsendglobal = 1;
			dispatch_message(MSG_FORGET, "\0", sendto, 1, 0);
			top_output(OUT_SCREEN, getlang("Forgotten"),
					   handles[sendto].string);
			}
		return;
		}
	}
/* REMEMBER command (undoes a FORGET command). */
if (cfg.allowforgetting && user.security >= cfg.forgetsec &&
	checkcmdmatch(get_word(0), getlang("CmdsRemember")) > 0)
	{
    /* Find node of user to forget. */
	if (numwds > 1)
		{
		sendto = find_node_from_name(tmpstr, tmphand,
									 &word_str[word_pos[1]]);

        /* REMEMBER is channel-independent and does not care if the user
           has forgotten us. */
		if (sendto == -3 || sendto == -4)
			{
			sendto = lastsendtonode;
			}

		if (sendto == -1)
			{
			top_output(OUT_SCREEN, getlang("NotLoggedIn"), tmphand);
			}
		if (sendto == -2)
			{
			top_output(OUT_SCREEN, getlang("NotSpecific"), tmphand);
			}
        /* Technically remembering yourself does not cause any problems
           (even though forgetting yourself does) but for consistency it is
           not allowed. */
        if (sendto == od_control.od_node)
			{
			top_output(OUT_SCREEN, getlang("CantDoCmdToSelf"));
			return;
			}
		if (sendto >= 0)
			{
			forgetstatus[sendto] &= (255 - FGT_FORGOTTEN);
			msgsendglobal = 1;
			dispatch_message(MSG_REMEMBER, "\0", sendto, 1, 0);
			top_output(OUT_SCREEN, getlang("Remembered"),
					   handles[sendto].string);
			}
		return;
		}
	}
/* User hit ENTER without typing any text. */
if (string[0] == 0 || numwds == 0)
	{
    /* Show the quick list of who's online and the short help. */
	channelsummary();
	return;
	}
/* JOIN command (changes the user's current channel. */
if (checkcmdmatch(get_word(0), getlang("CmdsJoin")) > 0)
	{
    unsigned long tchan; /* Channel to join. */
    long tch; /* Channel record number. */

	if (numwds > 1)
		{
        /* Check the channel name (if one was used) is valid. */
        if ((tchan = checkchannelname(&word_str[word_pos[1]])) > 0)
			{
            /* The name is valid. */

            /* Get the data from CHANIDX.TCH. */
            tch = findchannelrec(tchan);
            if (tch > -1L)
				{
                /* Data was found. */

                if (user.security < chan[tch].minsec ||
					user.security > chan[tch].maxsec)
					{
					top_output(OUT_SCREEN, getlang("BadChannelSec"));
					}
				else
					{
                    /* Leave the current channel. */
                    res = cmi_unjoin(curchannel);
					if (res != CMI_OKAY)
						{
						top_output(OUT_SCREEN, getlang("UnJoinError"));
						}
                    /* Join the new channel. */
                    res = cmi_join(tchan);
					if (res == CMIERR_BANNED)
						{
						top_output(OUT_SCREEN, getlang("Banned"));
						}
					if (res == CMIERR_NOINV)
						{
						top_output(OUT_SCREEN, getlang("NotInvited"));
						}
					if (res != CMI_OKAY && res != CMIERR_BANNED &&
						res != CMIERR_NOINV)
						{
						top_output(OUT_SCREEN, getlang("JoinError"));
						}
					if (res == CMI_OKAY)
						{
                        /* The join was successful. */
                        if (tchan > 0UL && tchan < 4000000000UL)
							{
							top_output(OUT_SCREEN, getlang("NowInChannel"),
									   channelname(tchan));
							}
						if (tchan >= 4001000000UL)
							{
							top_output(OUT_SCREEN, getlang("NowInConference"),
									   channelname(tchan));
							}
						dispatch_message(MSG_OUTCHANNEL, "\0", -1, 0, 0);
						curchannel = tchan;
						node->channel = curchannel;
						save_node_data(od_control.od_node, node);
						dispatch_message(MSG_INCHANNEL, "\0", -1, 0, 0);
						channelsummary();
						}
					else
						{
                        /* Rejoin the previous channel since we're not
                           allowed in the new one. */
						res = cmi_join(curchannel);
						if (res != CMI_OKAY)
							{
							top_output(OUT_SCREEN, getlang("CantReJoin"));
                            od_exit(230, FALSE);
							}
						}
					}
				}
			else
				{
                /* No data was found.  Currently the only difference if this
                   occurs is that the security is not checked, but this may
                   change so the entire procedure is duplicated. */

                /* Leave the current channel. */
                res = cmi_unjoin(curchannel);
				if (res != CMI_OKAY)
					{
					top_output(OUT_SCREEN, getlang("UnJoinError"));
					}
                /* Join the new channel. */
                res = cmi_join(tchan);
				if (res == CMIERR_BANNED)
					{
					top_output(OUT_SCREEN, getlang("Banned"));
					}
				if (res == CMIERR_NOINV)
					{
					top_output(OUT_SCREEN, getlang("NotInvited"));
					}
				if (res != CMI_OKAY && res != CMIERR_BANNED &&
					res != CMIERR_NOINV)
					{
					top_output(OUT_SCREEN, getlang("JoinError"));
					}
				if (res == CMI_OKAY)
					{
                    /* The join was sucessful. */
					if (tchan > 0UL && tchan < 4000000000UL)
						{
						top_output(OUT_SCREEN, getlang("NowInChannel"),
								   channelname(tchan));
						}
					if (tchan >= 4001000000UL)
						{
						top_output(OUT_SCREEN, getlang("NowInConference"),
								   channelname(tchan));
						}
					dispatch_message(MSG_OUTCHANNEL, "\0", -1, 0, 0);
					curchannel = tchan;
					node->channel = curchannel;
					save_node_data(od_control.od_node, node);
					dispatch_message(MSG_INCHANNEL, "\0", -1, 0, 0);
					channelsummary();
					}
				else
					{
                    /* Rejoin the old channel since we're not allowed in the
                       new one. */
                    res = cmi_join(curchannel);
					if (res != CMI_OKAY)
						{
						top_output(OUT_SCREEN, getlang("CantReJoin"));
                        od_exit(230, FALSE);
						}
					}
				}
			}
		else
			{
            /* The name was invalid. */

            /* If the second word (the channel to join) is all digits, it
               is not considered a name. */
            for (z = 0; z < strlen(&word_str[word_pos[1]]); z++)
				{
				if (!isdigit(word_str[word_pos[1] + z]))
					{
					z = -1;
					break;
					}
				}

            /* Convert the string to a number. */
            tchan = strtoul(&word_str[word_pos[1]], NULL, 10);

            /* Join the channel by number. */
			if (((tchan > 0 && tchan < 4000000000UL) ||
                /* Sysops may join channel 0 for global announcements. */
                (tchan == 0 && user.security >= cfg.sysopsec)) &&
				z != -1)
				{
                /* Leave the current channel. */
                res = cmi_unjoin(curchannel);
				if (res != CMI_OKAY)
					{
					top_output(OUT_SCREEN, getlang("UnJoinError"));
					}
                /* Join the new channel. */
                res = cmi_join(tchan);
				if (res == CMIERR_BANNED)
					{
					top_output(OUT_SCREEN, getlang("Banned"));
					}
				if (res == CMIERR_NOINV)
					{
					top_output(OUT_SCREEN, getlang("NotInvited"));
					}
				if (res != CMI_OKAY && res != CMIERR_BANNED &&
					res != CMIERR_NOINV)
					{
					top_output(OUT_SCREEN, getlang("JoinError"));
					}
				if (res == CMI_OKAY)
					{
                    /* The join was successful. */
					top_output(OUT_SCREEN, getlang("NowInChannel"),
							   channelname(tchan));
					dispatch_message(MSG_OUTCHANNEL, "\0", -1, 0, 0);
					curchannel = tchan;
					node->channel = curchannel;
					save_node_data(od_control.od_node, node);
					dispatch_message(MSG_INCHANNEL, "\0", -1, 0, 0);
					channelsummary();
					}
				else
					{
                    /* Rejoin the old channel as we are not allowed in
                       the new one. */
                    res = cmi_join(curchannel);
					if (res != CMI_OKAY)
						{
						top_output(OUT_SCREEN, getlang("CantReJoin"));
                        od_exit(230, FALSE);
						}
					}
				}
			else
				{
                /* The name is invalid and it was not determined to be a
                   number, so see if it is the name of a user to join their
                   personal channel. */
                sendto = find_node_from_name(tmpstr, tmphand,
											 &word_str[word_pos[1]]);
                /* Obviously, the JOIN command is channel-independent. */
                if (sendto == -4)
					{
					sendto = lastsendtonode;
					}
				if (sendto >= 0)
					{
                    /* A user's name was found so join their personal
                       channel. */
					tchan = (long) sendto + 4000000000UL;

                    /* Leave the current channel. */
					res = cmi_unjoin(curchannel);
					if (res != CMI_OKAY)
						{
						top_output(OUT_SCREEN, getlang("UnJoinError"));
						}
                    /* Join the new one. */
                    res = cmi_join(tchan);
					if (res == CMIERR_BANNED)
						{
						top_output(OUT_SCREEN, getlang("Banned"));
						}
					if (res == CMIERR_NOINV)
						{
						top_output(OUT_SCREEN, getlang("NotInvited"));
						}
					if (res != CMI_OKAY && res != CMIERR_BANNED &&
						res != CMIERR_NOINV)
						{
						top_output(OUT_SCREEN, getlang("JoinError"));
						}
					if (res == CMI_OKAY)
						{
                        /* The join was sucessful. */
                        top_output(OUT_SCREEN, getlang("NowInUserChannel"),
								   channelname(tchan));
						dispatch_message(MSG_OUTCHANNEL, "\0", -1, 0, 0);
						curchannel = tchan;
						node->channel = curchannel;
						save_node_data(od_control.od_node, node);
						dispatch_message(MSG_INCHANNEL, "\0", -1, 0, 0);
						channelsummary();
						}
					else
						{
                        /* Rejoin the old channel as we're not allowed in
                           the new one. */
						res = cmi_join(curchannel);
						if (res != CMI_OKAY)
							{
							top_output(OUT_SCREEN, getlang("CantReJoin"));
                            od_exit(230, FALSE);
							}
						}
					}
				else
					{
                    /* Invalid channel number/name. */
					top_output(OUT_SCREEN, getlang("BadChannelName"));
					}
				}
			}
		return;
		}
	}
/* CONFLIST command (list conferences). */
if (checkcmdmatch(get_word(0), getlang("CmdsConfList")) > 0 &&
	numwds == 1)
	{
	list_channels();
	return;
	}
/* MODERATOR multi-word command. */
if (checkcmdmatch(get_word(0), getlang("CmdsModerator")) > 0)
	{
	mod_proc();
	return;
	}
/* SYSOP multi-word command. */
if (checkcmdmatch(get_word(0), getlang("CmdsSysop")) > 0 &&
	user.security >= cfg.sysopsec)
	{
	sysop_proc();
	return;
	}
/* CHANGE multi-word command. */
if (checkcmdmatch(get_word(0), getlang("CmdsChange")) > 0)
	{
	change_proc();
	return;
	}
/* General action. */
if (!(user.pref2 & PREF2_ACTIONSOFF) && cfg.allowgas &&
	checkcmdmatch(get_word(0), getlang("CmdsGA")) > 0)
	{
	if (user.security < cfg.actionusesec)
		{
		top_output(OUT_SCREEN, getlang("NoAccActUse"));
		return;
		}
    /* GAs need text after the command. */
    if (numwds > 1)
		{
		top_output(OUT_SCREEN, getlang("GASentPrefix"));
		dispatch_message(MSG_GA, &word_str[word_pos[1]], -1, 0, echoga);
		if (msgsent)
			{
			top_output(OUT_SCREEN, getlang("GASent"));
			}
		return;
		}
	top_output(OUT_SCREEN, getlang("GANotSentPrefix"));
	if (msgsent)
		{
		top_output(OUT_SCREEN, getlang("GANotSent"));
		}
	return;
	}
/* Posessive general action. */
if (!(user.pref2 & PREF2_ACTIONSOFF) && cfg.allowgas &&
	checkcmdmatch(get_word(0), getlang("CmdsGA2")) > 0)
	{
    /* This command is treated identically to a normal GA except for the
       message number sent to other nodes. */

	if (user.security < cfg.actionusesec)
		{
		top_output(OUT_SCREEN, getlang("NoAccActUse"));
		return;
		}
	if (numwds > 1)
		{
		top_output(OUT_SCREEN, getlang("GASentPrefix"));
		dispatch_message(MSG_GA2, &word_str[word_pos[1]], -1, 0, echoga);
		if (msgsent)
			{
			top_output(OUT_SCREEN, getlang("GASent"));
			}
		return;
		}
	top_output(OUT_SCREEN, getlang("GANotSentPrefix"));
	if (msgsent)
		{
		top_output(OUT_SCREEN, getlang("GANotSent"));
		}
	return;
	}

/* Game multi-word commands, not used at this time. */
if (checkcmdmatch(get_word(0), getlang("CmdsSlots")))
	{
	slots_game();
	return;
	}
/*//|if (checkcmdmatch(get_word(0), getlang("CmdsPoker")))
	{
	poker_game(numwds);
	return;
	}*///|
if (checkcmdmatch(get_word(0), getlang("CmdsMatch")))
	{
	matchgame();
	return;
	}

/* Spawn commands, not used at this time. */
/*if (check_spawn_cmds(numwds))
	{
	return;
	}*/

/* Whispers (WHISPER TO or /). */
if (cfg.allowprivmsgs &&
    /* Check both words of the long form and the single character of the
       short form. */
    ((checkcmdmatch(get_word(0), getlang("CmdsWhisperLong1")) &&
	 checkcmdmatch(get_word(1), getlang("CmdsWhisperLong2"))) ||
	 get_word_char(0, 0) == getlangchar("WhisperShortChar", 0)))
	{
	if (user.security < cfg.privmessagesec)
		{
		top_output(OUT_SCREEN, getlang("NoAccPrivMsg"));
		return;
		}

	if (get_word_char(0, 0) == getlangchar("WhisperShortChar", 0))
		{
        /* Short form was used, so the length of the command is one (a
           single character). */
		cmdlen = 1;
        /* Test if only the command letter was specified. */
        if (strlen(get_word(0)) == 1)
			{
			cmdlen = -1;
			}
		}
	else
		{
        /* Long form was used, so the command length is the length of the
           two command words (usually WHISPER TO). */
        cmdlen = word_pos[2];
        /* Test if only the two command words were specified. */
        if (numwds < 3)
			{
			cmdlen = -1;
			}
		}

	if (cmdlen >= 0)
		{
        /* Determine which node to send the whisper to. */
		sendto = find_node_from_name(tmpstr, tmphand, &string[cmdlen]);

        /* Don't send if no text was specified (i.e. just a name). */
        if ((tmpstr[0] == '\0' || tmpstr[1] == '\0') && sendto >= 0)
			{
			cmdlen = -1;
			}

		if (cmdlen >= 0)
			{
			if (sendto == -1)
				{
				top_output(OUT_SCREEN, getlang("NotLoggedIn"), tmphand);
				}
			if (sendto == -2)
				{
				top_output(OUT_SCREEN, getlang("NotSpecific"), tmphand);
				}
			if (sendto == -3)
				{
				top_output(OUT_SCREEN, getlang("HasForgotYou"),
						   handles[lastsendtonode].string);
				}
			if (sendto == -4)
				{
				top_output(OUT_SCREEN, getlang("NotInYourChannel"),
						   handles[lastsendtonode].string);
				}
			if (sendto >= 0)
				{
                /* Send the whisper. */
				top_output(OUT_SCREEN, getlang("WhisperSentPrefix"));
				dispatch_message(MSG_WHISPER, spacecheck(tmpstr), sendto,
								 1, 0);
				if (msgsent)
					{
					top_output(OUT_SCREEN, getlang("WhisperSent"),
							   handles[sendto].string);
					}
				}
			return;
			}
		}

    /* Not enough information was specified. */
    top_output(OUT_SCREEN, getlang("WhisperNotSentPref"));
	if (msgsent)
		{
		top_output(OUT_SCREEN, getlang("WhisperNotSent"));
		}
	return;
	}
/* Directed messages. */
if ( ((checkcmdmatch(get_word(0), getlang("CmdsDirectedLong1")) &&
	 checkcmdmatch(get_word(1), getlang("CmdsDirectedLong2"))) ||
	 get_word_char(0, 0) == getlangchar("DirectedShortChar", 0)))
	{
    /* Directed messages are handled identically to whispers except for the
       commands, the message sent to other nodes, and the language items
       used. */

    if (get_word_char(0, 0) == getlangchar("DirectedShortChar", 0))
		{
		cmdlen = 1;
		if (strlen(get_word(0)) == 1)
			{
			cmdlen = -1;
			}
		}
	else
		{
		cmdlen = word_pos[2];
		if (numwds < 3)
			{
			cmdlen = -1;
			}
		}

	if (cmdlen >= 0)
		{
		sendto = find_node_from_name(tmpstr, tmphand, &string[cmdlen]);
		if ((tmpstr[0] == '\0' || tmpstr[1] == '\0') && sendto >= 0)
			{
			cmdlen = -1;
			}

		if (cmdlen >= 0)
			{
			if (sendto == -1)
				{
				top_output(OUT_SCREEN, getlang("NotLoggedIn"), tmphand);
				}
			if (sendto == -2)
				{
				top_output(OUT_SCREEN, getlang("NotSpecific"), tmphand);
				}
			if (sendto == -3)
				{
				top_output(OUT_SCREEN, getlang("HasForgotYou"),
						   handles[lastsendtonode].string);
				}
			if (sendto == -4)
				{
				top_output(OUT_SCREEN, getlang("NotInYourChannel"),
						   handles[lastsendtonode].string);
				}
			if (sendto >= 0)
				{
				top_output(OUT_SCREEN, getlang("DirectedSentPrefix"));
				dispatch_message(MSG_DIRECTED, spacecheck(tmpstr), sendto,
                                 0, user.pref2 & PREF2_ECHOOWNTEXT);
				if (msgsent)
					{
					top_output(OUT_SCREEN, getlang("DirectedSent"),
							   handles[sendto].string);
					}
				}
			return;
			}
		}
	top_output(OUT_SCREEN, getlang("DirectNotSentPref"));
	if (msgsent)
		{
		top_output(OUT_SCREEN, getlang("DirectedNotSent"));
		}
	return;
	}

/* Old-style (single list) action processor. */
/*load_action_verbs();
for (z = 0; z < numactions && !auflg && !cfg.allowactions; z++)
	{
	if (!stricmp(verbs[z].string, get_word(0)) &&
		verbs[z].string[0])
		{
		process_action(z, numwds, string);
		z = numactions;
		top_output(OUT_SCREEN, getlang("ActionUseSuffix"));
		auflg = 1;
		}
	}
for (z = 0; z < 4 && !auflg && cfg.allowactions; z++)
	{
	if (!stricmp(user.persact[z].verb, get_word(0)) &&
		user.persact[z].verb[0])
		{
		process_action(-(z + 1), numwds, string);
		z = 1000;
		top_output(OUT_SCREEN, getlang("ActionUseSuffix"));
		auflg = 1;
		}
	}*/

/* Check if the user used an action. */
if (cfg.allowactions && !(user.pref2 & PREF2_ACTIONSOFF) &&
	user.security >= cfg.actionusesec)
	{
	if (action_check(numwds))
		{
		return;
		}
	}

/* Normal text. */
if (user.security < cfg.talksec)
	{
	top_output(OUT_SCREEN, getlang("NoAccTalk"));
	return;
	}
dispatch_message(MSG_TEXT, string, priv, 0,
                 user.pref2 & PREF2_ECHOOWNTEXT);
top_output(OUT_SCREEN, getlang("MsgSentPrefix"));
if (msgsent)
	{
	top_output(OUT_SCREEN, getlang("MsgSent"));
	}

}
