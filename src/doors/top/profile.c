/******************************************************************************
PROFILE.C    User profile editor.

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

This module implements the user profile editor.  Most of its size is due to the
various menus, and the displays and key handling associated with them.
******************************************************************************/

#include "strwrap.h"
#include "top.h"

/* Macro to quickly get the language item name of the different action
   types. */
#define typstring(xxxx) xxxx == 0 ? (unsigned char *)"Normal" : (unsigned char *)"TalkType"

/* profile_editor() - User profile editor.
   Parameters:  None.
   Returns:  Nothing.
*/
void profile_editor(void)
{
/* Keypress holder, counter, personal action change flag, e/xmessage change
   flag, preferences change flag, new password entered okay flag. */
XINT key, d, pac = 0, msgchg = 0, prefchg = 0, pwok;
/* Temporary input buffer. */
unsigned char tmp[256]; // Dynam with MAXSTRLEN!
/* Password entry buffer, password confirmation buffer, menu option buffer,
   number of menu options. */
unsigned char ptmp1[20], ptmp2[20], optstr[50];
char opcount;

top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEditPrefix"));

/* The keys used in the editor are taken from language items.  The sysop
   may change the keys.  Each key has a specific character number inside
   its particular language item. */

/* Main menu loop. */
do
	{
    opcount = 0;
    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfMainPrefix"));
    if (cfg.allowactions)
        top_output(OUT_SCREEN, getlang((unsigned char *)"ProfMainPersAct"));
/*    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfMainDesc"));*/
    if (cfg.allowsexchange &&
        user.security >= cfg.sexchangesec)
        top_output(OUT_SCREEN, getlang((unsigned char *)"ProfMainGender"));
    if (cfg.allowhandlechange &&
        user.security >= cfg.handlechgsec)
        top_output(OUT_SCREEN, getlang((unsigned char *)"ProfMainHandle"));
    if (cfg.allowexmessages &&
        user.security >= cfg.exmsgchangesec)
        top_output(OUT_SCREEN, getlang((unsigned char *)"ProfMainEXMsg"));
    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfMainPref"));
    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfMainPW"));
    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfMainQuit"));

    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfMainPrompt"));

    strcpy((char*)outbuf, (char*)top_output(OUT_STRING, getlang((unsigned char *)"ProfMainKeys")));
    memset(optstr, 0, 50);

    /* Enable the option keys that are always available. */
    optstr[opcount++] = outbuf[1];
    optstr[opcount++] = outbuf[5];
    optstr[opcount++] = outbuf[6];
    optstr[opcount++] = outbuf[7];

    /* Add other options based on how the sysop has configured TOP. */
    if (cfg.allowactions)
        optstr[opcount++] = outbuf[0];
    if (cfg.allowsexchange &&
        user.security >= cfg.sexchangesec)
        optstr[opcount++] = outbuf[2];
    if (cfg.allowhandlechange &&
        user.security >= cfg.handlechgsec)
        optstr[opcount++] = outbuf[3];
    if (cfg.allowexmessages &&
        user.security >= cfg.exmsgchangesec)
        optstr[opcount++] = outbuf[4];

    key = od_get_answer((char*)optstr);
    sprintf((char*)outbuf, "%c", key);
    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfMainShowKey"), outbuf);

    pac = 0; msgchg = 0; prefchg = 0;
    /* Personal action editor. */
    if (key == getlangchar((unsigned char *)"ProfMainKeys", 0))
        {
        /* Personal action editor loop. */
        do
            {
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfPActHeader"));
            /* Display the four personal actions. */
            for (d = 0; d < 4; d++)
                {
                itoa(d + 1, (char*)outnum[0], 10);
                top_output(OUT_SCREEN, getlang((unsigned char *)"ProfPActNumber"),
                           outnum[0]);
                top_output(OUT_SCREEN, getlang((unsigned char *)"ProfPActVerbType"),
                           user.persact[d].verb,
                           getlang(typstring(user.persact[d].type)));
                top_output(OUT_SCREEN, getlang((unsigned char *)"ProfPActResponse"),
                          user.persact[d].response);
                top_output(OUT_SCREEN, getlang((unsigned char *)"ProfPActSingular"),
                          user.persact[d].singular);
                top_output(OUT_SCREEN, getlang((unsigned char *)"ProfPActPlural"),
                          user.persact[d].plural);
                }
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfPActPrompt"));
            key = od_get_answer((char*)getlang((unsigned char *)"ProfPActKeys"));
            /* Clear action. */
            if (key == getlangchar((unsigned char *)"ProfPActKeys", 0))
                {
                top_output(OUT_SCREEN, getlang((unsigned char *)"ProfClearAction"));
                top_output(OUT_SCREEN, getlang((unsigned char *)"ProfClActNumPrompt"));
                key = od_get_answer("1234");
                d = key - '1';
                itoa(d + 1, (char*)outnum[0], 10);
                top_output(OUT_SCREEN, getlang((unsigned char *)"ProfClActShowNum"),
                           outnum[0]);
                top_output(OUT_SCREEN, getlang((unsigned char *)"ProfClActOKPrompt"),
                           outnum[0]);
                /* Confirm, defaulting to no. */
                key = od_get_answer((char*)top_output(OUT_STRING,
                                    getlang((unsigned char *)"YesNoKeys")));
                if (key == getlangchar((unsigned char *)"YesNoKeys", 0))
                    {
                    top_output(OUT_SCREEN, getlang((unsigned char *)"Yes"));
                    memset(&user.persact[d], 0,
                           sizeof(struct pers_act_typ));
                    save_user_data(user_rec_num, &user);
                    actptrs[0][d]->data.responselen = 0;
                    actptrs[0][d]->data.singularlen = 0;
                    actptrs[0][d]->data.plurallen = 0;
                    // Errchecking bigtime!
                    actptrs[0][d]->ptrs.responsetext =
                        realloc(actptrs[0][d]->ptrs.responsetext,
                                1);
                    actptrs[0][d]->ptrs.singulartext =
                        realloc(actptrs[0][d]->ptrs.singulartext,
                                1);
                    actptrs[0][d]->ptrs.pluraltext =
                        realloc(actptrs[0][d]->ptrs.pluraltext,
                                1);
                    actptrs[0][d]->ptrs.responsetext[0] = '\0';
                    actptrs[0][d]->ptrs.singulartext[0] = '\0';
                    actptrs[0][d]->ptrs.pluraltext[0] = '\0';
                    actptrs[0][d]->data.verb[0] = '\0';
                    actptrs[0][d]->data.type = 0;
                    pac = 1;
                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfClActCleared"),
                               outnum[0]);
                    }
                else
                    {
                    top_output(OUT_SCREEN, getlang((unsigned char *)"No"));
                    }
                key = '\0';
                }
            /* Edit action. */
            if (key == getlangchar((unsigned char *)"ProfPActKeys", 1))
                {
                top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEditAction"));
                top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdActNumPrompt"));
                key = od_get_answer("1234");
                d = key - '1';
                sprintf((char*)outbuf, "%c", key);
                top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdActShowNum"),
                           outbuf);
                /* Main action editor loop. */
                do
                    {
                    itoa(d + 1, (char*)outnum[0], 10);
                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdActHeader"),
                               outnum[0]);

                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdActCurSet"));

                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfPActVerbType"),
                               user.persact[d].verb,
                               getlang(typstring(user.persact[d].type)));
                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfPActResponse"),
                              user.persact[d].response);
                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfPActSingular"),
                              user.persact[d].singular);
                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfPActPlural"),
                              user.persact[d].plural);

                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdActPrefix"));

                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdActType"));
                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdActVerb"));
                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdActResponse"));
                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdActSingular"));
                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdActPlural"));

                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdActHelp"));
                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdActQuit"));

                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdActPrompt"));

                    key = od_get_answer((char*)getlang((unsigned char *)"ProfEdActKeys"));
                    sprintf((char*)outbuf, "%c", key);
                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdActShowKey"),
                               outbuf);

                    /* Action type. */
                    if (key == getlangchar((unsigned char *)"ProfEdActKeys", 0))
                        {
                        top_output(OUT_SCREEN,
                                   getlang((unsigned char *)"ProfEdActEdCurType"),
                                   getlang(typstring(user.persact[d].type)));
                        top_output(OUT_SCREEN,
                                   getlang((unsigned char *)"ProfEdActEdTPrompt"));
                        key = od_get_answer((char*)getlang((unsigned char *)"ProfEdActEdTypKeys"));
                        if (key == getlangchar((unsigned char *)"ProfEdActEdTypKeys", 0))
                            user.persact[d].type = 0;
                        if (key == getlangchar((unsigned char *)"ProfEdActEdTypKeys", 1))
                            user.persact[d].type = 1;
                        top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdActEdNewType"),
                                   getlang(typstring(user.persact[d].type)));
                        save_user_data(user_rec_num, &user);
                        pac = 1;
                        key = '\0';
                        }
                    /* Action verb. */
                    if (key == getlangchar((unsigned char *)"ProfEdActKeys", 1))
                        {
                        do
                            {
                            top_output(OUT_SCREEN,
                                       getlang((unsigned char *)"ProfEdActEdCurVerb"),
                                       user.persact[d].verb);
                            top_output(OUT_SCREEN,
                                       getlang((unsigned char *)"ProfEdActEdVPrompt"));
                            od_input_str((char*)tmp, 10, '!',
                                         MAXASCII);
                            }
                        while(censorinput(tmp));

                        if (tmp[0])
                            {
                            trim_string(user.persact[d].verb,
                                        tmp, 0);
                            save_user_data(user_rec_num, &user);
                            pac = 1;
                            }
                        }
                    /* Action response text. */
                    if (key == getlangchar((unsigned char *)"ProfEdActKeys", 2))
                        {
                        do
                            {
                            top_output(OUT_SCREEN,
                                       getlang((unsigned char *)"ProfEdActEdCurResp"),
                                       user.persact[d].response);
                            top_output(OUT_SCREEN,
                                       getlang((unsigned char *)"ProfEdActEdRPrompt"));
                            od_input_str((char*)tmp, 60, ' ',
                                         MAXASCII);
                            }
                        while(censorinput(tmp));
                        if (tmp[0])
                            {
                            trim_string(user.persact[d].response,
                                        tmp, 0);
                            save_user_data(user_rec_num, &user);
                            pac = 1;
                            }
                        }
                    /* Singular action text. */
                    if (key == getlangchar((unsigned char *)"ProfEdActKeys", 3))
                        {
                        do
                            {
                            top_output(OUT_SCREEN,
                                       getlang((unsigned char *)"ProfEdActEdCurSing"),
                                       user.persact[d].singular);
                            top_output(OUT_SCREEN,
                                       getlang((unsigned char *)"ProfEdActEdSPrompt"));
                            od_input_str((char*)tmp, 60, ' ',
                                         MAXASCII);
                            }
                        while(censorinput(tmp));

                        if (tmp[0])
                            {
                            /* %m is required to prevent user malice. */
                            if (strstr((char*)tmp, "%m") ||
                                strstr((char*)tmp, "%M"))
                                {
                                trim_string(user.persact[d].singular,
                                            tmp, 0);
                                save_user_data(user_rec_num, &user);
                                pac = 1;
                                }
                            else
                                {
                                top_output(OUT_SCREEN,
                                           getlang((unsigned char *)"ProfEdActEdBadSing"));
                                }
                            }
                        }
                    /* Plural action text. */
                    if (key == getlangchar((unsigned char *)"ProfEdActKeys", 4))
                        {
                        do
                            {
                            top_output(OUT_SCREEN,
                                       getlang((unsigned char *)"ProfEdActEdCurPlur"),
                                       user.persact[d].plural);
                            top_output(OUT_SCREEN,
                                       getlang((unsigned char *)"ProfEdActEdPPrompt"));
                            od_input_str((char*)tmp, 60, ' ',
                                         MAXASCII);
                            }
                        while(censorinput(tmp));

                        if (tmp[0])
                            {
                            /* %m is required to prevent user malice. */
                            if (strstr((char*)tmp, "%m") ||
                                strstr((char*)tmp, "%M"))
                                {
                                trim_string(user.persact[d].plural,
                                            tmp, 0);
                                save_user_data(user_rec_num, &user);
                                pac = 1;
                                }
                            else
                                {
                                top_output(OUT_SCREEN,
                                           getlang((unsigned char *)"ProfEdActEdBadPlur"));
                                }
                            }
                        }
                    /* Action editor help. */
                    if (key == getlangchar((unsigned char *)"ProfEdActKeys", 5))
                        {
                        top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdActEdHlpPref"));
                        show_file("pershelp", SCRN_WAITKEY);
                        }
                    }
                /* Loop until the main action editor quit key is pressed. */
                while(key != getlangchar((unsigned char *)"ProfEdActKeys", 6));
                key = '\0';
                actptrs[0][d]->data.responselen =
                    strlen((char*)user.persact[d].response);
                actptrs[0][d]->data.singularlen =
                    strlen((char*)user.persact[d].singular);
                actptrs[0][d]->data.plurallen =
                    strlen((char*)user.persact[d].plural);
                // Errchecking bigtime!
                actptrs[0][d]->ptrs.responsetext =
                    realloc(actptrs[0][d]->ptrs.responsetext,
                            actptrs[0][d]->data.responselen + 1);
                actptrs[0][d]->ptrs.singulartext =
                    realloc(actptrs[0][d]->ptrs.singulartext,
                            actptrs[0][d]->data.singularlen + 1);
                actptrs[0][d]->ptrs.pluraltext =
                    realloc(actptrs[0][d]->ptrs.pluraltext,
                            actptrs[0][d]->data.plurallen + 1);
                strcpy((char*)actptrs[0][d]->ptrs.responsetext,
                       (char*)user.persact[d].response);
                strcpy((char*)actptrs[0][d]->ptrs.singulartext,
                       (char*)user.persact[d].singular);
                strcpy((char*)actptrs[0][d]->ptrs.pluraltext,
                       (char*)user.persact[d].plural);
                strcpy((char*)actptrs[0][d]->data.verb,
                       (char*)user.persact[d].verb);
                actptrs[0][d]->data.type =
                    user.persact[d].type;
                }
            }
        /* Loop until the action editor quit key is pressed. */
        while(key != getlangchar((unsigned char *)"ProfPActKeys", 2));
        top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdActQuitMsg"));
        key = '\0';
        }
/* Change description, no longer used since the implementation of the
   biography feture. */
/*    if (key == getlangchar((unsigned char *)"ProfMainKeys", 1))
        {
        top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdDescPrompt"));

        od_input_str(tmp, 60, ' ', MAXASCII);

        if (tmp[0] != '\0')
            {
            trim_string(user.description, tmp, 0);
            save_user_data(user_rec_num, &user);
            }
        }*/
    /* Change gender. */
    if (key == getlangchar((unsigned char *)"ProfMainKeys", 2))
        {
        top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdSexPrompt"));
        od_control.user_sex = od_get_answer((char*)getlang((unsigned char *)"ProfEdSexKeys"));
        if (od_control.user_sex == getlangchar((unsigned char *)"ProfEdSexKeys", 1))
            {
            strcpy((char*)tmp, (char*)top_output(OUT_STRINGNF, getlang((unsigned char *)"Female")));
            top_output(OUT_SCREEN, getlang((unsigned char *)"Female"));
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdSexSuffix"));
            user.gender = 1;
            }
        if (od_control.user_sex == getlangchar((unsigned char *)"ProfEdSexKeys", 0))
            {
            strcpy((char*)tmp, (char*)top_output(OUT_STRINGNF, getlang((unsigned char *)"Male")));
            top_output(OUT_SCREEN, getlang((unsigned char *)"Male"));
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdSexSuffix"));
            user.gender = 0;
            }
        curchannel = cmiprevchan;
        dispatch_message(MSG_SEXCHG, tmp, -1, 0, 0);
        curchannel = BUSYCHANNEL;

        node->gender = user.gender;

        save_node_data(od_control.od_node, node);
        save_user_data(user_rec_num, &user);
        }
    /* Change handle. */
    if (key == getlangchar((unsigned char *)"ProfMainKeys", 3))
        {
        do
            {
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdHandlePrompt"));
            od_input_str((char*)tmp, 30, ' ', MAXASCII);
            }
        while(censorinput(tmp));

        trim_string(tmp, tmp, 0);
        fixname(tmp, tmp);

        if (tmp[0] != '\0')
            {
            if (check_dupe_handles(tmp))
                {
                top_output(OUT_SCREEN, getlang((unsigned char *)"HandleInUse"));
                }
            else
                {
                fixname(user.handle, tmp);
                fixname(handles[od_control.od_node].string, tmp);
		        curchannel = cmiprevchan;
                dispatch_message(MSG_HANDLECHG, tmp, -1, 0, 0);
                curchannel = BUSYCHANNEL;
                save_user_data(user_rec_num, &user);
                od_log_write((char*)top_output(OUT_STRING,
                                        getlang((unsigned char *)"LogChangedHandle"),
                                        user.handle));

                fixname(node->handle, tmp);

                save_node_data(od_control.od_node, node);
                }
            }
        }
    /* Change entry/exit message(s). */
    if (key == getlangchar((unsigned char *)"ProfMainKeys", 4))
        {
        /* E/xmessage editor loop. */
        do
            {
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdEXMsgPrefix"));

            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdEXMsgCurEMsg"),
                       user.emessage);
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdEXMsgCurXMsg"),
                       user.xmessage);

            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdEXMsgEMsg"));
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdEXMsgXMsg"));
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdEXMsgQuit"));

            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdEXMsgPrompt"));

            key = od_get_answer((char*)getlang((unsigned char *)"ProfEdEXMsgKeys"));
            sprintf((char*)outbuf, "%c", key);
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdEXMsgShowKey"), outbuf);

            /* Change entry message. */
            if (key == getlangchar((unsigned char *)"ProfEdEXMsgKeys", 0))
                {
                do
                    {
                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdEXMEdCurEnt"),
                               user.emessage);
                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdEXMEdEPrompt"));
                    od_input_str((char*)tmp, 80, ' ',
                                 MAXASCII);
                    }
                while(censorinput(tmp));

                trim_string(user.emessage, tmp, 0);
                save_user_data(user_rec_num, &user);
                msgchg = 1;
                }
            /* Change exit message. */
            if (key == getlangchar((unsigned char *)"ProfEdEXMsgKeys", 1))
                {
                do
                    {
                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdEXMEdCurExit"),
                               user.xmessage);
                    top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdEXMEdXPrompt"));
                    od_input_str((char*)tmp, 80, ' ',
                                 MAXASCII);
                    }
                while(censorinput(tmp));
                trim_string(user.xmessage, tmp, 0);
                save_user_data(user_rec_num, &user);
                msgchg = 1;
                }
            }
        /* Loop until the e/xmessage editor quit key is pressed. */
        while(key != getlangchar((unsigned char *)"ProfEdEXMsgKeys", 2));
        key = '\0';
        top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdEXMsgSuffix"));
        }
    /* Edit user preferences. */
    if (key == getlangchar((unsigned char *)"ProfMainKeys", 5))
        {
        /* Preference editor loop. */
        do
            {
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPrefPrefix"));

            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPrefActEcho"),
                       getlang(user.pref1 & PREF1_ECHOACTIONS ?
                               (unsigned char *)"On" : (unsigned char *)"Off"));
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPrefTActEcho"),
                       getlang(user.pref1 & PREF1_ECHOTALKTYP ?
                               (unsigned char *)"On" : (unsigned char *)"Off"));
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPrefTMsgEcho"),
                       getlang(user.pref1 & PREF1_ECHOTALKMSG ?
                               (unsigned char *)"On" : (unsigned char *)"Off"));
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPrefMsgSent"),
                       getlang(user.pref1 & PREF1_MSGSENT ?
                               (unsigned char *)"On" : (unsigned char *)"Off"));
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPrefGAEcho"),
                       getlang(user.pref1 & PREF1_ECHOGA ?
                               (unsigned char *)"On" : (unsigned char *)"Off"));
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPrefTTSent"),
                       getlang(user.pref1 & PREF1_TALKMSGSENT ?
                               (unsigned char *)"On" : (unsigned char *)"Off"));
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPrefDualWin"),
                       getlang(user.pref1 & PREF1_DUALWINDOW ?
                               (unsigned char *)"On" : (unsigned char *)"Off"));
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPrefBlockTxt"),
                       getlang(user.pref1 & PREF1_BLKWHILETYP ?
                               (unsigned char *)"On" : (unsigned char *)"Off"));
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPrefListed"),
                       getlang(user.pref2 & PREF2_CHANLISTED ?
                               (unsigned char *)"On" : (unsigned char *)"Off"));
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPrefEchoOwn"),
                       getlang(user.pref2 & PREF2_ECHOOWNTEXT ?
                               (unsigned char *)"On" : (unsigned char *)"Off"));
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPrefActions"),
                       getlang(user.pref2 & PREF2_ACTIONSOFF ?
                               (unsigned char *)"On" : (unsigned char *)"Off"));
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPrefQuit"));

            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPrefPrompt"));

            key = od_get_answer((char*)getlang((unsigned char *)"ProfEdPrefKeys"));
            sprintf((char*)outbuf, "%c", key);
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPrefShowKey"), outbuf);

            /* The various preferences are covered in TOP.H and TOP.CFG. */

            if (key == getlangchar((unsigned char *)"ProfEdPrefKeys", 0))
                {
                user.pref1 ^= PREF1_ECHOACTIONS;
                prefchg = 1;
                }
            if (key == getlangchar((unsigned char *)"ProfEdPrefKeys", 1))
                {
                user.pref1 ^= PREF1_ECHOTALKTYP;
                prefchg = 1;
                }
            if (key == getlangchar((unsigned char *)"ProfEdPrefKeys", 2))
                {
                user.pref1 ^= PREF1_ECHOTALKMSG;
                prefchg = 1;
                }
            if (key == getlangchar((unsigned char *)"ProfEdPrefKeys", 3))
                {
                user.pref1 ^= PREF1_MSGSENT;
                prefchg = 1;
                }
            if (key == getlangchar((unsigned char *)"ProfEdPrefKeys", 4))
                {
                user.pref1 ^= PREF1_ECHOGA;
                prefchg = 1;
                }
            if (key == getlangchar((unsigned char *)"ProfEdPrefKeys", 5))
                {
                user.pref1 ^= PREF1_TALKMSGSENT;
                prefchg = 1;
                }
            if (key == getlangchar((unsigned char *)"ProfEdPrefKeys", 6))
                {
                if (!od_control.user_ansi/* &&
                    !od_control.user_avatar*/)
                    {
                    top_output(OUT_SCREEN,
                               getlang((unsigned char *)"DualWindowNoGfx"));
                    }
                else
                    {
                    user.pref1 ^= PREF1_DUALWINDOW;
                    prefchg = 1;
                    }
                }
            if (key == getlangchar((unsigned char *)"ProfEdPrefKeys", 7))
                {
                user.pref1 ^= PREF1_BLKWHILETYP;
                prefchg = 1;
                }
            if (key == getlangchar((unsigned char *)"ProfEdPrefKeys", 8))
                {
                user.pref2 ^= PREF2_CHANLISTED;
                prefchg = 1;
                node->channellisted =
                    (user.pref2 & PREF2_CHANLISTED);
                save_node_data(od_control.od_node, node);
                }
            if (key == getlangchar((unsigned char *)"ProfEdPrefKeys", 9))
                {
                user.pref2 ^= PREF2_ECHOOWNTEXT;
                prefchg = 1;
                }
            if (key == getlangchar((unsigned char *)"ProfEdPrefKeys", 10))
                {
                user.pref2 ^= PREF2_ACTIONSOFF;
                prefchg = 1;
                }
            }
        /* Loop until the preference editor quit key is pressed. */
        while(key != getlangchar((unsigned char *)"ProfEdPrefKeys", 11));
        key = '\0';
        /* Save the user data if preferences were changed. */
        if (prefchg)
            {
            save_user_data(user_rec_num, &user);
            }
        top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPrefSuffix"));
        }
    /* Change TOP password. */
    if (key == getlangchar((unsigned char *)"ProfMainKeys", 6))
        {
        char entryok = 1;

        /* Verify the current password if one exists. */
        if (user.password[0])
            {
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPWCurPrompt"));
            get_password(ptmp1, 15);
            if (stricmp((char*)user.password, (char*)ptmp1))
                {
                top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPWBadPW"));
                entryok = 0;
                }
            }
        if (entryok)
            {
            pwok = 0;
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPWNewPrompt"));
            get_password(ptmp1, 15);
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPWConfPrompt"));
            get_password(ptmp2, 15);
            if (stricmp((char*)ptmp1, (char*)ptmp2))
                {
                top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEdPWNoMatch"));
                pwok = 0;
                }
            else
                {
                pwok = 1;
                }
            top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEDPWSuffix"));
            if (pwok)
                {
                strcpy((char*)user.password, (char*)strupr((char*)ptmp1));
                save_user_data(user_rec_num, &user);
                }
            }
        }
    if (pac)
    	{
        curchannel = cmiprevchan;
        dispatch_message(MSG_PERACTCHG, (unsigned char *)"", -1, 0, 0);
        curchannel = BUSYCHANNEL;
        }
    if (msgchg)
    	{
        curchannel = cmiprevchan;
        dispatch_message(MSG_EXMSGCHG, (unsigned char *)"", -1, 0, 0);
        curchannel = BUSYCHANNEL;
        }
    }
/* Loop until the profile editor quit key is pressed. */
while(key != getlangchar((unsigned char *)"ProfMainKeys", 7));

top_output(OUT_SCREEN, getlang((unsigned char *)"ProfEditSuffix"));

return;
}

