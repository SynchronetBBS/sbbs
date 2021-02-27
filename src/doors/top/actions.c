/******************************************************************************
ACTIONS.C    Contains all functions relating to Actions.

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

This module contains all action-related functions, including the loading,
processing, and listing of actions.
******************************************************************************/

#include "top.h"

/* loadactions() - Loads all actions into memory.
   Parameters:  None.
   Returns:  TRUE if the actions were successfully loaded, FALSE if falure
             occurred.
*/
char loadactions(void)
    {
    unsigned XINT lad, lae; /* Counters. */
    XINT actfil; /* File handle for loading actions. */
    unsigned long ldat = 0, ltxt; /* File position trackers. */
    unsigned char *memchk = NULL; /* Errorchecking pointer. */

    /* Allocate space for the pointer index. */
    actfiles = calloc(numactfiles + 1, sizeof(action_file_typ XFAR *));
    if (actfiles == NULL)
        {
        return 0;
        }

    /* Allocate space for each file's header. */
    for (lad = 0; lad <= numactfiles; lad++)
        {
        actfiles[lad] = malloc(sizeof(action_file_typ));
        if (actfiles[lad] == NULL)
            {
            return 0;
            }
        }

    /* Prepare list #0 as the personal action list. */
    strcpy(actfiles[0]->name, top_output(OUT_STRING,
           getlang("PersActListDesc")));
    // These next four should be configurable later.
    actfiles[0]->minsec = 0;
    actfiles[0]->maxsec = 0xFFFF;
    actfiles[0]->minchannel = 0;
    actfiles[0]->maxchannel = MAXCHANNEL;
    actfiles[0]->numactions = 4;

    /* Read the header data for each action file.  This "first pass" allows
       TOP to count the actions in each file. */
    for (lad = 1; lad <= numactfiles; lad++)
        {
        strcpy(outbuf, cfg.topactpath);
        strcat(outbuf, cfg.actfilenames[lad]);
        strcat(outbuf, ".tac");
        actfil = sh_open(outbuf, O_RDWR | O_CREAT | O_BINARY,
                         SH_DENYNO, S_IREAD | S_IWRITE);
        if (actfil == -1)
            {
            return 0;
            }
        lseek(actfil, 0, SEEK_SET);
        read(actfil, actfiles[lad], sizeof(action_file_typ));
        close(actfil);
        }

    /* Allocate space for the actual action data.  First the master index
       is allocated, followed by each list's index and actual space in turn.
       This is where a lot of memory gets sucked up. */
    actptrs = calloc(numactfiles + 1, sizeof(action_file_typ XFAR *));
    if (actptrs == NULL)
        {
        return 0;
        }
    for (lad = 0; lad <= numactfiles; lad++)
        {
        actptrs[lad] = calloc(actfiles[lad]->numactions,
                              sizeof(action_rec_typ XFAR *));
        if (actptrs[lad] == NULL)
            {
            return 0;
            }
        for (lae = 0; lae < actfiles[lad]->numactions; lae++)
            {
            actptrs[lad][lae] = malloc(sizeof(action_rec_typ));
            if (actptrs[lad][lae] == NULL)
                {
                return 0;
                }
            }
        }

    /* Load and process each action file. */
    for (lad = 1; lad <= numactfiles; lad++)
        {
        /* Open the file. */
        strcpy(outbuf, cfg.topactpath);
        strcat(outbuf, cfg.actfilenames[lad]);
        strcat(outbuf, ".tac");
        actfil = sh_open(outbuf, O_RDWR | O_CREAT | O_BINARY,
                         SH_DENYNO, S_IREAD | S_IWRITE);
        if (!actfil)
            {
            return 0;
            }

        /* ldat tracks the position in the file where the next data is to
           be loaded from. */
        ldat = actfiles[lad]->datstart;

        /* Load and process each action. */
        for (lae = 0; lae < actfiles[lad]->numactions; lae++)
            {
            /* Load the data for the action. */
            lseek(actfil, ldat, SEEK_SET);
            read(actfil, &actptrs[lad][lae]->data, sizeof(action_data_typ));
            ldat += (long) sizeof(action_data_typ);

            /* Allocate space for the strings in the action.  This is done
               dynamically to save memory.  The redundant memchk pointer
               simply saves typing for errorchecking. */
            memchk = actptrs[lad][lae]->ptrs.responsetext =
                malloc(actptrs[lad][lae]->data.responselen + 1);
            if (memchk == NULL)
                {
                close(actfil);
                return 0;
                }
            memset(actptrs[lad][lae]->ptrs.responsetext, 0,
                   actptrs[lad][lae]->data.responselen + 1);
            memchk = actptrs[lad][lae]->ptrs.singulartext =
                malloc(actptrs[lad][lae]->data.singularlen + 1);
            if (memchk == NULL)
                {
                close(actfil);
                return 0;
                }
            memset(actptrs[lad][lae]->ptrs.singulartext, 0,
                   actptrs[lad][lae]->data.singularlen + 1);
            memchk = actptrs[lad][lae]->ptrs.pluraltext =
                malloc(actptrs[lad][lae]->data.plurallen + 1);
            if (memchk == NULL)
                {
                close(actfil);
                return 0;
                }
            memset(actptrs[lad][lae]->ptrs.pluraltext, 0,
                   actptrs[lad][lae]->data.plurallen + 1);

            /* ltxt tracks the position in the file where the string text
               is to be loaded from. */
            ltxt = actfiles[lad]->txtstart + actptrs[lad][lae]->data.textofs;

            /* Read the text for the action. */
            // Still need errs down here.
            lseek(actfil, ltxt, SEEK_SET);
            read(actfil, actptrs[lad][lae]->ptrs.responsetext,
                 actptrs[lad][lae]->data.responselen);
            ltxt += actptrs[lad][lae]->data.responselen;
            lseek(actfil, ltxt, SEEK_SET);
            read(actfil, actptrs[lad][lae]->ptrs.singulartext,
                 actptrs[lad][lae]->data.singularlen);
            ltxt += actptrs[lad][lae]->data.singularlen;
            lseek(actfil, ltxt, SEEK_SET);
            read(actfil, actptrs[lad][lae]->ptrs.pluraltext,
                 actptrs[lad][lae]->data.plurallen);
            ltxt += actptrs[lad][lae]->data.plurallen;
            }
        close(actfil);
        }

    /* If the loop is completed in full then the action loading was done
       successfully. */
    return 1;
    }

/* action_check() - Checks the input string to see if it is an action.
   Parameters:  awords - The number of words in the input string.
   Returns:  TRUE if an action was found and processed, FALSE if not.
   Notes:  The string that is to be processed must first be parsed by the
           split_string() function.
*/
char action_check(XINT awords)
    {
    unsigned XINT acd, ace; /* Counters. */

    /* Grab the first word and check to make sure the action prefix is
       present, if one is defined.  outbuf is used as a temporary storage
       buffer. */
    strcpy(outbuf, get_word(0));
    if (cfg.actionprefix[0])
        {
        if (strnicmp(outbuf, cfg.actionprefix, strlen(cfg.actionprefix)))
            {
            return 0;
            }
        }

    /* Check each action in each list against the string. */
    for (acd = 0; acd <= numactfiles; acd++)
        {
        for (ace = 0; ace < actfiles[acd]->numactions; ace++)
            {
            if (!stricmp(&outbuf[strlen(cfg.actionprefix)],
                         actptrs[acd][ace]->data.verb))
                {
                if (user.security >= actfiles[acd]->minsec &&
                    user.security <= actfiles[acd]->maxsec &&
                    curchannel >= actfiles[acd]->minchannel &&
                    curchannel <= actfiles[acd]->maxchannel)
                    {
                    action_do(acd, ace, awords, &word_str[word_pos[1]]);
                    return 1;
                    }
                }
            }
        }

    return 0;
    }

/* action_do() - Parses an action.
   Parameters:  lnum - List number the action is found in.
                anum - Number of the action within the list.
                dowords - Number of words in the input string.
                dostr - Remainder of the string, not including the verb.
   Returns:  Nothing.
*/
void action_do(unsigned XINT lnum, unsigned XINT anum,
               XINT dowords, unsigned char *dostr)
    {
    char secret = 0, doecho = 1; /* Secret and Echo flags. */
    XINT sendto = -1, msgtype = -1; /* Node to send to and Type of message. */
    /* Output pointer, and temporary handle buffer. */
    unsigned char *useptr = NULL, tmphand[256];
    XINT res; /* Result code. */

    /* Verify that the type of action is one this version of TOP can handle. */
    if (actptrs[lnum][anum]->data.type < 0 ||
        actptrs[lnum][anum]->data.type > 1)
        {
        top_output(OUT_SCREEN, getlang("CantDoAction"));
        od_log_write(top_output(OUT_STRING, getlang("LogCantDoAction"),
                                get_word(0)));
        return;
        }

    /* The msgextradata is only used for some types of actions so it is
       cleared for safety. */
    msgextradata = -1L;

    /* Process the action based on its type. */
    switch(actptrs[lnum][anum]->data.type)
        {
        case ACT_NORMAL:
            if (dowords == 1)
                {
                /* Handle the singular case. */
                useptr = actptrs[lnum][anum]->ptrs.singulartext;
                msgtype = MSG_ACTIONSIN;
                doecho = user.pref1 & PREF1_ECHOACTIONS;
                strcpy(outbuf, useptr);
                }
            else
                {
                /* Handle the plural case. */
                useptr = actptrs[lnum][anum]->ptrs.pluraltext;
                msgtype = MSG_ACTIONPLU;
                doecho = user.pref1 & PREF1_ECHOACTIONS;
                if (checkcmdmatch(get_word(1), getlang("CmdsActionAll")))
                    {
                    /* Action was done to "all". */
                    sendto = -2;
                    }
                else
                    {
                    /* Determine who "receives" the action. */
                    sendto = find_node_from_name(outbuf, tmphand,
                                dostr);
                    if (sendto == -1)
                        {
                        top_output(OUT_SCREEN, getlang("NotLoggedIn"),
                                   tmphand);
                        return;
                        }
                    if (sendto == -2)
                        {
                        top_output(OUT_SCREEN, getlang("NotSpecific"),
                                   tmphand);
                        return;
                        }
                    if (sendto == -3)
                        {
                        top_output(OUT_SCREEN, getlang("HasForgotYou"),
                                   handles[lastsendtonode].string);
                        return;
                        }
                    if (sendto == -4)
                        {
                        top_output(OUT_SCREEN, getlang("NotInYourChannel"),
                                   handles[lastsendtonode].string);
                        return;
                        }
                    /* Check if action was done "secretly" (privately). */
                    if (outbuf[1] != '\0' && cfg.allowprivmsgs &&
                        user.security >= cfg.privmessagesec)
                        {
                        if (checkcmdmatch(&outbuf[1],
                                          getlang("CmdsActionSecretly")) > 0)
                            {
                            msgtype = MSG_ACTPLUSEC;
                            }
                        }
                    }
                strcpy(outbuf, useptr);
                }
            break;
        case ACT_TALKTYPE:
            if (dowords == 1)
                {
                /* Handle the singular case. */
                useptr = actptrs[lnum][anum]->ptrs.singulartext;
                msgtype = MSG_TLKTYPSIN;
                doecho = user.pref1 & PREF1_ECHOTALKTYP;
                strcpy(outbuf, useptr);
                }
            else
                {
                /* Handle any typed text. */
                useptr = actptrs[lnum][anum]->ptrs.pluraltext;
                msgtype = MSG_TLKTYPPLU;
                doecho = user.pref1 & PREF1_ECHOTALKTYP;
                outbuf = top_output(OUT_STRINGNF, getlang("TalkTypePrefix"),
                                    useptr, "\0");
                msgextradata = strlen(outbuf);
                outbuf = top_output(OUT_STRINGNF, getlang("TalkTypePrefix"),
                                    useptr, &word_str[word_pos[1]]);
                }
            break;
    	}

    if (!useptr[0])
        {
        /* If there is no text in the output string, assume the sysop
           specified "N/A" and issue an error. */
        top_output(OUT_SCREEN, getlang("BadContext"),
                   get_word(0),
                   !actptrs[lnum][anum]->data.type ?
                   getlang("Action") : getlang("TalkType"));
        return;
        }

    /* Display the response text, if any. */
    if (actptrs[lnum][anum]->ptrs.responsetext[0] != '\0')
        {
        top_output(OUT_SCREEN, getlang("ActionResponse"),
                   actptrs[lnum][anum]->ptrs.responsetext);
        }

    /* Append "secretly" text if applicable. */
    if (msgtype == MSG_ACTPLUSEC)
        {
        top_output(OUT_SCREEN, getlang("JustTo"), handles[sendto].string);
        secret = 1;
        }
    top_output(OUT_SCREEN, getlang("ActionSuffix"));

    /* Display "message sent" notifications, if requested. */
    if (msgtype == MSG_TLKTYPSIN && (user.pref1 & PREF1_TALKMSGSENT))
        {
        top_output(OUT_SCREEN, getlang("TalkTypeActionSent"));
        }
    if (msgtype == MSG_TLKTYPPLU && (user.pref1 & PREF1_TALKMSGSENT))
        {
        top_output(OUT_SCREEN, getlang("TalkTypeMsgSent"));
        }

    if (lnum == 0)
        {
        char *ppp; /* strstr() result pointer. */

        /* Changes %P to nothing in personal actions, preventing users from
           causing all sorts of problems.  This is a kludge fix; this should
           really be detected when the actions are edited. */
		Tempfix:
        ppp = strstr(outbuf, "%p");
        if (ppp == NULL)
            {
            ppp = strstr(outbuf, "%P");
            }
        if (ppp != NULL)
            {
            ppp[0] = '!';
            goto Tempfix;
            }
        }

    dispatch_message(msgtype, outbuf, sendto, secret, doecho);

    }

/* action_list() - Display list(s) of actions to the user.
   Parameters:  alwords - Number of words in the input string.
   Returns:  Nothing.
   Notes:  The string that is to be processed must first be parsed by the
           split_string() function.
*/
void action_list(XINT alwords)
    {
    /* Counter, list number to display, "all lists" flag/counter. */
    XINT ald, alist = -1, alll;
    char alines = 0, ans = 0; /* Line counter, nonstop flag. */

    if (alwords < 3)
        {
        /* If less than three words, assume user issued the list command by
           itself and show the "list of lists" to the user. */
        top_output(OUT_SCREEN, getlang("ActFileListPrefix"));
        top_output(OUT_SCREEN, getlang("ActFileListHdr"));
        top_output(OUT_SCREEN, getlang("ActFileListSep"));
        for (ald = 0; ald <= numactfiles; ald++)
            {
            if (user.security >= actfiles[ald]->minsec &&
                user.security <= actfiles[ald]->maxsec &&
                curchannel >= actfiles[ald]->minchannel &&
                curchannel <= actfiles[ald]->maxchannel)
                {
                top_output(OUT_SCREEN, getlang("ActFileListItem"),
                           cfg.actfilenames[ald], actfiles[ald]->name);
                }
            }
        top_output(OUT_SCREEN, getlang("ActFileListSuffix"));
        }
    else
        {
        /* Compare the third word of the input to each list name. */
        for (ald = 0; ald <= numactfiles; ald++)
            {
            if (!stricmp(get_word(2), cfg.actfilenames[ald]) &&
                user.security >= actfiles[ald]->minsec &&
                user.security <= actfiles[ald]->maxsec &&
                curchannel >= actfiles[ald]->minchannel &&
                curchannel <= actfiles[ald]->maxchannel)
                {
                alll = alist = ald;
                break;
                }
            }

        /* Test if all lists were requested. */
        if (checkcmdmatch(get_word(2), getlang("CmdsActionListAll")))
            {
            alll = 0;
            alist = numactfiles;
            }

        /* Fail if no list number has been determined. */
        if (alist < 0)
            {
            top_output(OUT_SCREEN, getlang("BadListName"));
            return;
            }

        /* Display the list to the user. */
        top_output(OUT_SCREEN, getlang("ActListPrefix"));
        /* This loop only executes once unless we're in "show all lists"
           mode. */
        for (; alll <= alist; alll++)
            {
            if (user.security >= actfiles[alll]->minsec &&
                user.security <= actfiles[alll]->maxsec &&
                curchannel >= actfiles[alll]->minchannel &&
                curchannel <= actfiles[alll]->maxchannel)
                {
                top_output(OUT_SCREEN, getlang("ActListHeader"),
                           cfg.actfilenames[alll], actfiles[alll]->name);
                top_output(OUT_SCREEN, getlang("ActListLegend"));
                top_output(OUT_SCREEN, getlang("ActListSep"));
                for (ald = 0; ald < actfiles[alll]->numactions; ald++)
                    {
                    if (actptrs[alll][ald]->data.type == 0)
                        {
                        top_output(OUT_SCREEN,
                                   getlang("ActListCharNormal"));
                        }
                    if (actptrs[alll][ald]->data.type == 1)
                        {
                        top_output(OUT_SCREEN,
                                   getlang("ActListCharTType"));
                        }
                    top_output(OUT_SCREEN, getlang("ActListItem"),
                               strlwr(actptrs[alll][ald]->data.verb));
                    top_output(OUT_SCREEN, getlang("ActListItemSep"));
                    /* Start a new line every six actions. */
                    if ((ald + 1) % 6 == 0 ||
                        ald == actfiles[alll]->numactions - 1)
                        {
                        top_output(OUT_SCREEN, getlang("ActListLineEnd"));
                        alines++;
                        /* Show a more prompt every 20 lines. */
                        if (alines == 20 && !ans)
                            {
                            alines = 0;
                            ans = moreprompt();
                            if (ans == -1)
                                {
                                break;
                                }
                            }
                        }
                    }
                /* If we're showing all the lists, reset for the next list. */
                if (alll < alist && !ans)
                    {
                    alines = 0;
                    ans = moreprompt();
                    if (ans == -1)
                        {
                        break;
                        }
                    }
                top_output(OUT_SCREEN, getlang("ActListSuffix"));
                }
            }
        }

    }

/* action_proc() - Process action-related commands.
   Parameters:  awords - Number of words in input string.
   Returns:  Nothing.
   Notes:  The string that is to be processed must first be parsed by the
           split_string() function.
*/
void action_proc(XINT awords)
    {

    /* List actions. */
    if (checkcmdmatch(get_word(1), getlang("CmdsActionList")))
        {
        action_list(awords);
        return;
        }
    /* Turn actions off. */
    if (checkcmdmatch(get_word(1), getlang("CmdsActionOff")))
        {
        user.pref2 |= PREF2_ACTIONSOFF;
        save_user_data(user_rec_num, &user);
        top_output(OUT_SCREEN, getlang("ActionsNowOff"));
        return;
        }
    /* Turn actions on. */
    if (checkcmdmatch(get_word(1), getlang("CmdsActionOn")))
        {
        user.pref2 &= (0xFF - PREF2_ACTIONSOFF);
        save_user_data(user_rec_num, &user);
        top_output(OUT_SCREEN, getlang("ActionsNowOn"));
        return;
        }

    /* Command not recognized, show help file instead. */
    show_helpfile("helpact0");

    }

/* loadpersactions() - Prepares the personal action list.
   Parameters:  None.
   Returns:  TRUE on success, FALSE on failure (usually out of memory).
*/
char loadpersactions(void)
    {
    unsigned XINT lad; /* Counter. */
    unsigned char *memchk = NULL; /* Errorcheck pointer. */

    /* This process is identical to the one used in the loadactions()
       function.  The only difference is that the actions are copied from
       the user's data instead of being loaded from a file. */
    for (lad = 0; lad < 4; lad++)
        {
        actptrs[0][lad]->data.type = user.persact[lad].type;
        strcpy(actptrs[0][lad]->data.verb, user.persact[lad].verb);
        actptrs[0][lad]->data.responselen =
            strlen(user.persact[lad].response);
        actptrs[0][lad]->data.singularlen =
            strlen(user.persact[lad].singular);
        actptrs[0][lad]->data.plurallen =
            strlen(user.persact[lad].plural);

        memchk = actptrs[0][lad]->ptrs.responsetext =
            malloc(actptrs[0][lad]->data.responselen + 1);
        if (memchk == NULL)
            {
            return 0;
            }
        memchk = actptrs[0][lad]->ptrs.singulartext =
            malloc(actptrs[0][lad]->data.singularlen + 1);
        if (memchk == NULL)
            {
            return 0;
            }
        memchk = actptrs[0][lad]->ptrs.pluraltext =
            malloc(actptrs[0][lad]->data.plurallen + 1);
        if (memchk == NULL)
            {
            return 0;
            }
        strcpy(actptrs[0][lad]->ptrs.responsetext,
               user.persact[lad].response);
        strcpy(actptrs[0][lad]->ptrs.singulartext,
               user.persact[lad].singular);
        strcpy(actptrs[0][lad]->ptrs.pluraltext,
               user.persact[lad].plural);
        }

    return 1;
    }

