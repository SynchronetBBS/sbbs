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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

This module implements the user profile editor.  Most of its size is due to the
various menus, and the displays and key handling associated with them.
******************************************************************************/

#include "top.h"

/* Macro to quickly get the language item name of the different action
   types. */
#define typstring(xxxx) xxxx == 0 ? "Normal" : "TalkType"

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
char tmp[256]; // Dynam with MAXSTRLEN!
/* Password entry buffer, password confirmation buffer, menu option buffer,
   number of menu options. */
char ptmp1[20], ptmp2[20], optstr[50], opcount;

top_output(OUT_SCREEN, getlang("ProfEditPrefix"));

/* The keys used in the editor are taken from language items.  The sysop
   may change the keys.  Each key has a specific character number inside
   its particular language item. */

/* Main menu loop. */
do
	{
    opcount = 0;
    top_output(OUT_SCREEN, getlang("ProfMainPrefix"));
    if (cfg.allowactions)
        top_output(OUT_SCREEN, getlang("ProfMainPersAct"));
/*    top_output(OUT_SCREEN, getlang("ProfMainDesc"));*/
    if (cfg.allowsexchange &&
        user.security >= cfg.sexchangesec)
        top_output(OUT_SCREEN, getlang("ProfMainGender"));
    if (cfg.allowhandlechange &&
        user.security >= cfg.handlechgsec)
        top_output(OUT_SCREEN, getlang("ProfMainHandle"));
    if (cfg.allowexmessages &&
        user.security >= cfg.exmsgchangesec)
        top_output(OUT_SCREEN, getlang("ProfMainEXMsg"));
    top_output(OUT_SCREEN, getlang("ProfMainPref"));
    top_output(OUT_SCREEN, getlang("ProfMainPW"));
    top_output(OUT_SCREEN, getlang("ProfMainQuit"));

    top_output(OUT_SCREEN, getlang("ProfMainPrompt"));

    strcpy(outbuf, top_output(OUT_STRING, getlang("ProfMainKeys")));
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

    key = od_get_answer(optstr);
    sprintf(outbuf, "%c", key);
    top_output(OUT_SCREEN, getlang("ProfMainShowKey"), outbuf);

    pac = 0; msgchg = 0; prefchg = 0;
    /* Personal action editor. */
    if (key == getlangchar("ProfMainKeys", 0))
        {
        /* Personal action editor loop. */
        do
            {
            top_output(OUT_SCREEN, getlang("ProfPActHeader"));
            /* Display the four personal actions. */
            for (d = 0; d < 4; d++)
                {
                itoa(d + 1, outnum[0], 10);
                top_output(OUT_SCREEN, getlang("ProfPActNumber"),
                           outnum[0]);
                top_output(OUT_SCREEN, getlang("ProfPActVerbType"),
                           user.persact[d].verb,
                           getlang(typstring(user.persact[d].type)));
                top_output(OUT_SCREEN, getlang("ProfPActResponse"),
                          user.persact[d].response);
                top_output(OUT_SCREEN, getlang("ProfPActSingular"),
                          user.persact[d].singular);
                top_output(OUT_SCREEN, getlang("ProfPActPlural"),
                          user.persact[d].plural);
                }
            top_output(OUT_SCREEN, getlang("ProfPActPrompt"));
            key = od_get_answer(getlang("ProfPActKeys"));
            /* Clear action. */
            if (key == getlangchar("ProfPActKeys", 0))
                {
                top_output(OUT_SCREEN, getlang("ProfClearAction"));
                top_output(OUT_SCREEN, getlang("ProfClActNumPrompt"));
                key = od_get_answer("1234");
                d = key - '1';
                itoa(d + 1, outnum[0], 10);
                top_output(OUT_SCREEN, getlang("ProfClActShowNum"),
                           outnum[0]);
                top_output(OUT_SCREEN, getlang("ProfClActOKPrompt"),
                           outnum[0]);
                /* Confirm, defaulting to no. */
                key = od_get_answer(top_output(OUT_STRING,
                                    getlang("YesNoKeys")));
                if (key == getlangchar("YesNoKeys", 0))
                    {
                    top_output(OUT_SCREEN, getlang("Yes"));
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
                    top_output(OUT_SCREEN, getlang("ProfClActCleared"),
                               outnum[0]);
                    }
                else
                    {
                    top_output(OUT_SCREEN, getlang("No"));
                    }
                key = '\0';
                }
            /* Edit action. */
            if (key == getlangchar("ProfPActKeys", 1))
                {
                top_output(OUT_SCREEN, getlang("ProfEditAction"));
                top_output(OUT_SCREEN, getlang("ProfEdActNumPrompt"));
                key = od_get_answer("1234");
                d = key - '1';
                sprintf(outbuf, "%c", key);
                top_output(OUT_SCREEN, getlang("ProfEdActShowNum"),
                           outbuf);
                /* Main action editor loop. */
                do
                    {
                    itoa(d + 1, outnum[0], 10);
                    top_output(OUT_SCREEN, getlang("ProfEdActHeader"),
                               outnum[0]);

                    top_output(OUT_SCREEN, getlang("ProfEdActCurSet"));

                    top_output(OUT_SCREEN, getlang("ProfPActVerbType"),
                               user.persact[d].verb,
                               getlang(typstring(user.persact[d].type)));
                    top_output(OUT_SCREEN, getlang("ProfPActResponse"),
                              user.persact[d].response);
                    top_output(OUT_SCREEN, getlang("ProfPActSingular"),
                              user.persact[d].singular);
                    top_output(OUT_SCREEN, getlang("ProfPActPlural"),
                              user.persact[d].plural);

                    top_output(OUT_SCREEN, getlang("ProfEdActPrefix"));

                    top_output(OUT_SCREEN, getlang("ProfEdActType"));
                    top_output(OUT_SCREEN, getlang("ProfEdActVerb"));
                    top_output(OUT_SCREEN, getlang("ProfEdActResponse"));
                    top_output(OUT_SCREEN, getlang("ProfEdActSingular"));
                    top_output(OUT_SCREEN, getlang("ProfEdActPlural"));

                    top_output(OUT_SCREEN, getlang("ProfEdActHelp"));
                    top_output(OUT_SCREEN, getlang("ProfEdActQuit"));

                    top_output(OUT_SCREEN, getlang("ProfEdActPrompt"));

                    key = od_get_answer(getlang("ProfEdActKeys"));
                    sprintf(outbuf, "%c", key);
                    top_output(OUT_SCREEN, getlang("ProfEdActShowKey"),
                               outbuf);

                    /* Action type. */
                    if (key == getlangchar("ProfEdActKeys", 0))
                        {
                        top_output(OUT_SCREEN,
                                   getlang("ProfEdActEdCurType"),
                                   getlang(typstring(user.persact[d].type)));
                        top_output(OUT_SCREEN,
                                   getlang("ProfEdActEdTPrompt"));
                        key = od_get_answer(getlang("ProfEdActEdTypKeys"));
                        if (key == getlangchar("ProfEdActEdTypKeys", 0))
                            user.persact[d].type = 0;
                        if (key == getlangchar("ProfEdActEdTypKeys", 1))
                            user.persact[d].type = 1;
                        top_output(OUT_SCREEN, getlang("ProfEdActEdNewType"),
                                   getlang(typstring(user.persact[d].type)));
                        save_user_data(user_rec_num, &user);
                        pac = 1;
                        key = '\0';
                        }
                    /* Action verb. */
                    if (key == getlangchar("ProfEdActKeys", 1))
                        {
                        do
                            {
                            top_output(OUT_SCREEN,
                                       getlang("ProfEdActEdCurVerb"),
                                       user.persact[d].verb);
                            top_output(OUT_SCREEN,
                                       getlang("ProfEdActEdVPrompt"));
                            od_input_str(tmp, 10, '!',
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
                    if (key == getlangchar("ProfEdActKeys", 2))
                        {
                        do
                            {
                            top_output(OUT_SCREEN,
                                       getlang("ProfEdActEdCurResp"),
                                       user.persact[d].response);
                            top_output(OUT_SCREEN,
                                       getlang("ProfEdActEdRPrompt"));
                            od_input_str(tmp, 60, ' ',
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
                    if (key == getlangchar("ProfEdActKeys", 3))
                        {
                        do
                            {
                            top_output(OUT_SCREEN,
                                       getlang("ProfEdActEdCurSing"),
                                       user.persact[d].singular);
                            top_output(OUT_SCREEN,
                                       getlang("ProfEdActEdSPrompt"));
                            od_input_str(tmp, 60, ' ',
                                         MAXASCII);
                            }
                        while(censorinput(tmp));

                        if (tmp[0])
                            {
                            /* %m is required to prevent user malice. */
                            if (strstr(tmp, "%m") ||
                                strstr(tmp, "%M"))
                                {
                                trim_string(user.persact[d].singular,
                                            tmp, 0);
                                save_user_data(user_rec_num, &user);
                                pac = 1;
                                }
                            else
                                {
                                top_output(OUT_SCREEN,
                                           getlang("ProfEdActEdBadSing"));
                                }
                            }
                        }
                    /* Plural action text. */
                    if (key == getlangchar("ProfEdActKeys", 4))
                        {
                        do
                            {
                            top_output(OUT_SCREEN,
                                       getlang("ProfEdActEdCurPlur"),
                                       user.persact[d].plural);
                            top_output(OUT_SCREEN,
                                       getlang("ProfEdActEdPPrompt"));
                            od_input_str(tmp, 60, ' ',
                                         MAXASCII);
                            }
                        while(censorinput(tmp));

                        if (tmp[0])
                            {
                            /* %m is required to prevent user malice. */
                            if (strstr(tmp, "%m") ||
                                strstr(tmp, "%M"))
                                {
                                trim_string(user.persact[d].plural,
                                            tmp, 0);
                                save_user_data(user_rec_num, &user);
                                pac = 1;
                                }
                            else
                                {
                                top_output(OUT_SCREEN,
                                           getlang("ProfEdActEdBadPlur"));
                                }
                            }
                        }
                    /* Action editor help. */
                    if (key == getlangchar("ProfEdActKeys", 5))
                        {
                        top_output(OUT_SCREEN, getlang("ProfEdActEdHlpPref"));
                        show_file("pershelp", SCRN_WAITKEY);
                        }
                    }
                /* Loop until the main action editor quit key is pressed. */
                while(key != getlangchar("ProfEdActKeys", 6));
                key = '\0';
                actptrs[0][d]->data.responselen =
                    strlen(user.persact[d].response);
                actptrs[0][d]->data.singularlen =
                    strlen(user.persact[d].singular);
                actptrs[0][d]->data.plurallen =
                    strlen(user.persact[d].plural);
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
                strcpy(actptrs[0][d]->ptrs.responsetext,
                       user.persact[d].response);
                strcpy(actptrs[0][d]->ptrs.singulartext,
                       user.persact[d].singular);
                strcpy(actptrs[0][d]->ptrs.pluraltext,
                       user.persact[d].plural);
                strcpy(actptrs[0][d]->data.verb,
                       user.persact[d].verb);
                actptrs[0][d]->data.type =
                    user.persact[d].type;
                }
            }
        /* Loop until the action editor quit key is pressed. */
        while(key != getlangchar("ProfPActKeys", 2));
        top_output(OUT_SCREEN, getlang("ProfEdActQuitMsg"));
        key = '\0';
        }
/* Change description, no longer used since the implementation of the
   biography feture. */
/*    if (key == getlangchar("ProfMainKeys", 1))
        {
        top_output(OUT_SCREEN, getlang("ProfEdDescPrompt"));

        od_input_str(tmp, 60, ' ', MAXASCII);

        if (tmp[0] != '\0')
            {
            trim_string(user.description, tmp, 0);
            save_user_data(user_rec_num, &user);
            }
        }*/
    /* Change gender. */
    if (key == getlangchar("ProfMainKeys", 2))
        {
        top_output(OUT_SCREEN, getlang("ProfEdSexPrompt"));
        od_control.user_sex = od_get_answer(getlang("ProfEdSexKeys"));
        if (od_control.user_sex == getlangchar("ProfEdSexKeys", 1))
            {
            strcpy(tmp, top_output(OUT_STRINGNF, getlang("Female")));
            top_output(OUT_SCREEN, getlang("Female"));
            top_output(OUT_SCREEN, getlang("ProfEdSexSuffix"));
            user.gender = 1;
            }
        if (od_control.user_sex == getlangchar("ProfEdSexKeys", 0))
            {
            strcpy(tmp, top_output(OUT_STRINGNF, getlang("Male")));
            top_output(OUT_SCREEN, getlang("Male"));
            top_output(OUT_SCREEN, getlang("ProfEdSexSuffix"));
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
    if (key == getlangchar("ProfMainKeys", 3))
        {
        do
            {
            top_output(OUT_SCREEN, getlang("ProfEdHandlePrompt"));
            od_input_str(tmp, 30, ' ', MAXASCII);
            }
        while(censorinput(tmp));

        trim_string(tmp, tmp, 0);
        fixname(tmp, tmp);

        if (tmp[0] != '\0')
            {
            if (check_dupe_handles(tmp))
                {
                top_output(OUT_SCREEN, getlang("HandleInUse"));
                }
            else
                {
                fixname(user.handle, tmp);
                fixname(handles[od_control.od_node].string, tmp);
		        curchannel = cmiprevchan;
                dispatch_message(MSG_HANDLECHG, tmp, -1, 0, 0);
                curchannel = BUSYCHANNEL;
                save_user_data(user_rec_num, &user);
                od_log_write(top_output(OUT_STRING,
                                        getlang("LogChangedHandle"),
                                        user.handle));

                fixname(node->handle, tmp);

                save_node_data(od_control.od_node, node);
                }
            }
        }
    /* Change entry/exit message(s). */
    if (key == getlangchar("ProfMainKeys", 4))
        {
        /* E/xmessage editor loop. */
        do
            {
            top_output(OUT_SCREEN, getlang("ProfEdEXMsgPrefix"));

            top_output(OUT_SCREEN, getlang("ProfEdEXMsgCurEMsg"),
                       user.emessage);
            top_output(OUT_SCREEN, getlang("ProfEdEXMsgCurXMsg"),
                       user.xmessage);

            top_output(OUT_SCREEN, getlang("ProfEdEXMsgEMsg"));
            top_output(OUT_SCREEN, getlang("ProfEdEXMsgXMsg"));
            top_output(OUT_SCREEN, getlang("ProfEdEXMsgQuit"));

            top_output(OUT_SCREEN, getlang("ProfEdEXMsgPrompt"));

            key = od_get_answer(getlang("ProfEdEXMsgKeys"));
            sprintf(outbuf, "%c", key);
            top_output(OUT_SCREEN, getlang("ProfEdEXMsgShowKey"), outbuf);

            /* Change entry message. */
            if (key == getlangchar("ProfEdEXMsgKeys", 0))
                {
                do
                    {
                    top_output(OUT_SCREEN, getlang("ProfEdEXMEdCurEnt"),
                               user.emessage);
                    top_output(OUT_SCREEN, getlang("ProfEdEXMEdEPrompt"));
                    od_input_str(tmp, 80, ' ',
                                 MAXASCII);
                    }
                while(censorinput(tmp));

                trim_string(user.emessage, tmp, 0);
                save_user_data(user_rec_num, &user);
                msgchg = 1;
                }
            /* Change exit message. */
            if (key == getlangchar("ProfEdEXMsgKeys", 1))
                {
                do
                    {
                    top_output(OUT_SCREEN, getlang("ProfEdEXMEdCurExit"),
                               user.xmessage);
                    top_output(OUT_SCREEN, getlang("ProfEdEXMEdXPrompt"));
                    od_input_str(tmp, 80, ' ',
                                 MAXASCII);
                    }
                while(censorinput(tmp));
                trim_string(user.xmessage, tmp, 0);
                save_user_data(user_rec_num, &user);
                msgchg = 1;
                }
            }
        /* Loop until the e/xmessage editor quit key is pressed. */
        while(key != getlangchar("ProfEdEXMsgKeys", 2));
        key = '\0';
        top_output(OUT_SCREEN, getlang("ProfEdEXMsgSuffix"));
        }
    /* Edit user preferences. */
    if (key == getlangchar("ProfMainKeys", 5))
        {
        /* Preference editor loop. */
        do
            {
            top_output(OUT_SCREEN, getlang("ProfEdPrefPrefix"));

            top_output(OUT_SCREEN, getlang("ProfEdPrefActEcho"),
                       getlang(user.pref1 & PREF1_ECHOACTIONS ?
                               "On" : "Off"));
            top_output(OUT_SCREEN, getlang("ProfEdPrefTActEcho"),
                       getlang(user.pref1 & PREF1_ECHOTALKTYP ?
                               "On" : "Off"));
            top_output(OUT_SCREEN, getlang("ProfEdPrefTMsgEcho"),
                       getlang(user.pref1 & PREF1_ECHOTALKMSG ?
                               "On" : "Off"));
            top_output(OUT_SCREEN, getlang("ProfEdPrefMsgSent"),
                       getlang(user.pref1 & PREF1_MSGSENT ?
                               "On" : "Off"));
            top_output(OUT_SCREEN, getlang("ProfEdPrefGAEcho"),
                       getlang(user.pref1 & PREF1_ECHOGA ?
                               "On" : "Off"));
            top_output(OUT_SCREEN, getlang("ProfEdPrefTTSent"),
                       getlang(user.pref1 & PREF1_TALKMSGSENT ?
                               "On" : "Off"));
            top_output(OUT_SCREEN, getlang("ProfEdPrefDualWin"),
                       getlang(user.pref1 & PREF1_DUALWINDOW ?
                               "On" : "Off"));
            top_output(OUT_SCREEN, getlang("ProfEdPrefBlockTxt"),
                       getlang(user.pref1 & PREF1_BLKWHILETYP ?
                               "On" : "Off"));
            top_output(OUT_SCREEN, getlang("ProfEdPrefListed"),
                       getlang(user.pref2 & PREF2_CHANLISTED ?
                               "On" : "Off"));
            top_output(OUT_SCREEN, getlang("ProfEdPrefEchoOwn"),
                       getlang(user.pref2 & PREF2_ECHOOWNTEXT ?
                               "On" : "Off"));
            top_output(OUT_SCREEN, getlang("ProfEdPrefActions"),
                       getlang(user.pref2 & PREF2_ACTIONSOFF ?
                               "On" : "Off"));
            top_output(OUT_SCREEN, getlang("ProfEdPrefQuit"));

            top_output(OUT_SCREEN, getlang("ProfEdPrefPrompt"));

            key = od_get_answer(getlang("ProfEdPrefKeys"));
            sprintf(outbuf, "%c", key);
            top_output(OUT_SCREEN, getlang("ProfEdPrefShowKey"), outbuf);

            /* The various preferences are covered in TOP.H and TOP.CFG. */

            if (key == getlangchar("ProfEdPrefKeys", 0))
                {
                user.pref1 ^= PREF1_ECHOACTIONS;
                prefchg = 1;
                }
            if (key == getlangchar("ProfEdPrefKeys", 1))
                {
                user.pref1 ^= PREF1_ECHOTALKTYP;
                prefchg = 1;
                }
            if (key == getlangchar("ProfEdPrefKeys", 2))
                {
                user.pref1 ^= PREF1_ECHOTALKMSG;
                prefchg = 1;
                }
            if (key == getlangchar("ProfEdPrefKeys", 3))
                {
                user.pref1 ^= PREF1_MSGSENT;
                prefchg = 1;
                }
            if (key == getlangchar("ProfEdPrefKeys", 4))
                {
                user.pref1 ^= PREF1_ECHOGA;
                prefchg = 1;
                }
            if (key == getlangchar("ProfEdPrefKeys", 5))
                {
                user.pref1 ^= PREF1_TALKMSGSENT;
                prefchg = 1;
                }
            if (key == getlangchar("ProfEdPrefKeys", 6))
                {
                if (!od_control.user_ansi/* &&
                    !od_control.user_avatar*/)
                    {
                    top_output(OUT_SCREEN,
                               getlang("DualWindowNoGfx"));
                    }
                else
                    {
                    user.pref1 ^= PREF1_DUALWINDOW;
                    prefchg = 1;
                    }
                }
            if (key == getlangchar("ProfEdPrefKeys", 7))
                {
                user.pref1 ^= PREF1_BLKWHILETYP;
                prefchg = 1;
                }
            if (key == getlangchar("ProfEdPrefKeys", 8))
                {
                user.pref2 ^= PREF2_CHANLISTED;
                prefchg = 1;
                node->channellisted =
                    (user.pref2 & PREF2_CHANLISTED);
                save_node_data(od_control.od_node, node);
                }
            if (key == getlangchar("ProfEdPrefKeys", 9))
                {
                user.pref2 ^= PREF2_ECHOOWNTEXT;
                prefchg = 1;
                }
            if (key == getlangchar("ProfEdPrefKeys", 10))
                {
                user.pref2 ^= PREF2_ACTIONSOFF;
                prefchg = 1;
                }
            }
        /* Loop until the preference editor quit key is pressed. */
        while(key != getlangchar("ProfEdPrefKeys", 11));
        key = '\0';
        /* Save the user data if preferences were changed. */
        if (prefchg)
            {
            save_user_data(user_rec_num, &user);
            }
        top_output(OUT_SCREEN, getlang("ProfEdPrefSuffix"));
        }
    /* Change TOP password. */
    if (key == getlangchar("ProfMainKeys", 6))
        {
        char entryok = 1;

        /* Verify the current password if one exists. */
        if (user.password[0])
            {
            top_output(OUT_SCREEN, getlang("ProfEdPWCurPrompt"));
            get_password(ptmp1, 15);
            if (stricmp(user.password, ptmp1))
                {
                top_output(OUT_SCREEN, getlang("ProfEdPWBadPW"));
                entryok = 0;
                }
            }
        if (entryok)
            {
            pwok = 0;
            top_output(OUT_SCREEN, getlang("ProfEdPWNewPrompt"));
            get_password(ptmp1, 15);
            top_output(OUT_SCREEN, getlang("ProfEdPWConfPrompt"));
            get_password(ptmp2, 15);
            if (stricmp(ptmp1, ptmp2))
                {
                top_output(OUT_SCREEN, getlang("ProfEdPWNoMatch"));
                pwok = 0;
                }
            else
                {
                pwok = 1;
                }
            top_output(OUT_SCREEN, getlang("ProfEDPWSuffix"));
            if (pwok)
                {
                strcpy(user.password, strupr(ptmp1));
                save_user_data(user_rec_num, &user);
                }
            }
        }
    if (pac)
    	{
        curchannel = cmiprevchan;
        dispatch_message(MSG_PERACTCHG, "\0", -1, 0, 0);
        curchannel = BUSYCHANNEL;
        }
    if (msgchg)
    	{
        curchannel = cmiprevchan;
        dispatch_message(MSG_EXMSGCHG, "\0", -1, 0, 0);
        curchannel = BUSYCHANNEL;
        }
    }
/* Loop until the profile editor quit key is pressed. */
while(key != getlangchar("ProfMainKeys", 7));

top_output(OUT_SCREEN, getlang("ProfEditSuffix"));

return;
}

