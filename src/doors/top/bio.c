/******************************************************************************
BIO.C        Biography functions.

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

This module contains all functions that pertain to the Biography feature of
TOP.
******************************************************************************/

#include "top.h"

/* loadbioquestions() - Load biography questions from BIOQUES.CFG.
   Parameters:  None.
   Returns:  TRUE on success, FALSE on error.
*/
char loadbioquestions(void)
    {
    FILE *bfil = NULL; /* File stream for cfg file. */
    XINT qcount = 0; /* Question count. */

    /* Open configuration file. */
    sprintf(outbuf, "%sbioques.cfg", cfg.toppath);
    bfil = fopen(outbuf, "rt");
    if (bfil == NULL)
        {
        return 0;
        }

    /* Read and process each line. */
    while (!feof(bfil))
        {
        fgets(outbuf, 256, bfil);
        /* Ignore comments. */
        if (outbuf[0] == ';')
            {
            continue;
            }
        stripcr(outbuf);
        memset(&bioques[qcount], 0, sizeof(bio_ques_typ));
        /* Parse the line.  See BIOQUES.CFG notes for format description. */
        bioques[qcount].number = atoi(&outbuf[0]);
        strncpy(bioques[qcount].field, &outbuf[3], 30);
        trim_string(bioques[qcount].field, bioques[qcount].field,
                    NO_LTRIM);
        if (!strnicmp(&outbuf[34], "NUM", 3))
            bioques[qcount].resptype = BIORESP_NUMERIC;
        if (!strnicmp(&outbuf[34], "STR", 3))
            bioques[qcount].resptype = BIORESP_STRING;
        bioques[qcount].minval = atol(&outbuf[38]);
        bioques[qcount].maxval = atol(&outbuf[50]);
        qcount++;
        }

    fclose(bfil);

    numbioques = qcount;

    return 1;
    }

/* displaybio - Displays a user's biography.
   Parameters:  uname - String naming the user who's biography is to be
                        displayed.
   Returns:  TRUE if the bio was found and shown, FALSE on error.
   Notes:  Currently, the user's FULL name is required.  Partial matching
           is not supported.
*/
char displaybio(unsigned char *uname)
    {
    long resploc[MAXBIOQUES]; /* Response locations. */
    XINT unum = -1, d; /* User record number, counter. */
    bio_resp_typ rbuf; /* Bio response buffer. */
    user_data_typ ubuf; /* User data buffer. */
    char ns = 0; /* Nonstop mode flag. */

    /* Locate the user and determine if the user's bio has been completed. */
    unum = find_user_from_name(uname, &ubuf, UFIND_USECFG);
    if (unum != user_rec_num && !ubuf.donebio)
        {
        top_output(OUT_SCREEN, getlang("BioUserHasNone"));
        return 0;
        }
    if (unum < 0)
        {
        fixname(uname, uname);
        top_output(OUT_SCREEN, getlang("BioUserNotFound"), uname);
        return 0;
        }

    /* Load the biography repsonse index for this user from BIOIDX.TOP. */
    rec_locking(REC_LOCK, bioidxfil,
                (long) unum * (MAXBIOQUES * sizeof(long)),
                MAXBIOQUES * sizeof(long));
    lseek(bioidxfil, (long) unum * (MAXBIOQUES * sizeof(long)), SEEK_SET);
    read(bioidxfil, resploc, MAXBIOQUES * sizeof(long));
    rec_locking(REC_UNLOCK, bioidxfil,
                (long) unum * (MAXBIOQUES * sizeof(long)),
                MAXBIOQUES * sizeof(long));

    /* Response numbers are stored as 1-based, with 0 indicating that no
       response is present.  This loop converts the numbers to 0-based. */
    for (d = 0; d < MAXBIOQUES; d++, resploc[d]--);

    top_output(OUT_SCREEN, getlang("BioDispPrefix"), ubuf.handle);

    for (d = 0; d < numbioques; d++)
        {
        itoa(bioques[d].number, outnum[0], 10);
        if (unum == user_rec_num)
            {
            /* If we're displaying our own biography, show the question number
               as well because the function may have been called from the
               editor. */
            top_output(OUT_SCREEN, getlang("BioQuesNumber"), outnum[0]);
            }
        top_output(OUT_SCREEN, getlang("BioRespPrefix"), bioques[d].field);

        if (resploc[bioques[d].number] >= 0)
            {
            /* Load the response for the question. */
            rec_locking(REC_LOCK, biorespfil,
                        resploc[bioques[d].number] * sizeof(bio_resp_typ),
                        sizeof(bio_resp_typ));
            lseek(biorespfil,
                  resploc[bioques[d].number] * sizeof(bio_resp_typ),
                  SEEK_SET);
            read(biorespfil, &rbuf, sizeof(bio_resp_typ));
            rec_locking(REC_UNLOCK, biorespfil,
                        resploc[bioques[d].number] * sizeof(bio_resp_typ),
                        sizeof(bio_resp_typ));

            top_output(OUT_SCREEN, getlang("BioResp"), rbuf.response);
            }
        else
            {
            top_output(OUT_SCREEN, getlang("BioNoResp"));
            }
        top_output(OUT_SCREEN, getlang("BioRespSuffix"));
        /* Display more prompt every 23 lines. */
        if (d % 23 == 0 && d > 0 && !ns)
            {
            ns = moreprompt();
            if (ns == -1)
                {
                break;
                }
            }
        }

    top_output(OUT_SCREEN, getlang("BioDispSuffix"));

    return 1;
    }

/* writebioresponse() - Writes a biography response to disk.
   Parameters:  qnum - Question number of the response.
                br - Pointer to response data.
   Returns:  TRUE on success, FALSE on failure.
*/
char writebioresponse(XINT qnum, bio_resp_typ *br)
    {
    long rnum = 0; /* Response number. */
    char newresp = 0; /* Whether or not this is a new response. */

    /* Load the current value for this question from BIOIDX.TOP to see if a
       response to this question already exists. */
    rec_locking(REC_LOCK, bioidxfil,
                (user_rec_num * (MAXBIOQUES * sizeof(long))) +
                ((long) qnum * sizeof(long)), sizeof(long));
    lseek(bioidxfil,
         (user_rec_num * (MAXBIOQUES * sizeof(long))) +
         ((long) qnum * sizeof(long)), SEEK_SET);
    read(bioidxfil, &rnum, sizeof(long));
    if (rnum <= 0)
        {
        /* Obtain the next question number. */
        rnum = (filelength(biorespfil) / sizeof(bio_resp_typ)) + 1L;
        newresp = 1;
        }

    /* Write the response. */
    rec_locking(REC_LOCK, biorespfil, (rnum - 1L) * sizeof(bio_resp_typ),
                sizeof(bio_resp_typ));
    lseek(biorespfil, (rnum - 1L) * sizeof(bio_resp_typ), SEEK_SET);
    write(biorespfil, br, sizeof(bio_resp_typ));
    rec_locking(REC_UNLOCK, biorespfil, (rnum - 1L) * sizeof(bio_resp_typ),
                sizeof(bio_resp_typ));

    /* If this is a new response, write the location to the index. */
    if (newresp)
        {
        lseek(bioidxfil,
             (user_rec_num * (MAXBIOQUES * sizeof(long))) +
             ((long) qnum * sizeof(long)), SEEK_SET);
        write(bioidxfil, &rnum, sizeof(long));
        }
    rec_locking(REC_UNLOCK, bioidxfil,
                (user_rec_num * (MAXBIOQUES * sizeof(long))) +
                ((long) qnum * sizeof(long)), sizeof(long));

    return 1;
    }

/* readbioresponse() - Reads a biography response from disk.
   Parameters:  qnum - Question number of the response.
                br - Pointer to response buffer to fill.
   Returns:  TRUE on success, FALSE on failure.
*/
char readbioresponse(XINT qnum, bio_resp_typ *br)
    {
    long rnum; /* Response number. */

    /* Obtain the response number for this question from BIOIDX.TOP. */
    rec_locking(REC_LOCK, bioidxfil,
                (user_rec_num * (MAXBIOQUES * sizeof(long))) +
                ((long) qnum * sizeof(long)), sizeof(long));
    lseek(bioidxfil,
         (user_rec_num * (MAXBIOQUES * sizeof(long))) +
         ((long) qnum * sizeof(long)), SEEK_SET);
    read(bioidxfil, &rnum, sizeof(long));
    /* Abort if the index indicates no response is present. */
    if (rnum <= 0)
        {
        rec_locking(REC_UNLOCK, bioidxfil,
                    (user_rec_num * (MAXBIOQUES * sizeof(long))) +
                    ((long) qnum * sizeof(long)), sizeof(long));
        return 0;
        }

    /* Convert response number to 0-based. */
    rnum--;

    /* Load the response itself. */
    rec_locking(REC_LOCK, biorespfil, rnum * sizeof(bio_resp_typ),
                sizeof(bio_resp_typ));
    lseek(biorespfil, rnum * sizeof(bio_resp_typ), SEEK_SET);
    read(biorespfil, br, sizeof(bio_resp_typ));
    rec_locking(REC_UNLOCK, biorespfil, rnum * sizeof(bio_resp_typ),
                sizeof(bio_resp_typ));

    rec_locking(REC_UNLOCK, bioidxfil,
                (user_rec_num * (MAXBIOQUES * sizeof(long))) +
                ((long) qnum * sizeof(long)), sizeof(long));

    return 1;
    }

/* getbioresponse() - Prompts the user for a biography response.
   Parameters:  rec - Record number of question the user is responding to.
                br - Pointer to buffer to hold the response.
   Returns:  TRUE if the response changed, FALSE if the user aborted.
*/
char getbioresponse(XINT rec, bio_resp_typ *br)
    {
    char tresp[256]; /* Input buffer. */
    long tnr; /* Response length holder. */
    char doneresp = 0, changed = 0; /* Loop abort flag, changed flag. */

    /* Loop until the user enters a valid response. */
    do
        {
        if (bioques[rec].resptype == BIORESP_NUMERIC)
            {
            /* Prompt the user. */
            itoa(bioques[rec].number, outnum[0], 10);
            top_output(OUT_SCREEN, getlang("BioNumRespPrompt"), outnum[0],
                       bioques[rec].field);

//            od_input_str(tresp,
//                         numresplen(bioques[rec].minval, bioques[rec].maxval),
//                         '-', '9');
            /* Get a string, restricting the user to numeric values only. */
            od_input_str(tresp, 11, '-', '9');
            tnr = strtol(tresp, NULL, 10);
            if (tnr < bioques[rec].minval ||
                tnr > bioques[rec].maxval &&
                tresp[0])
                {
                /* Abort if the value is too low or high, but only if
                   something was entered. */
                ltoa(bioques[rec].minval, outnum[0], 10);
                ltoa(bioques[rec].maxval, outnum[1], 10);
                top_output(OUT_SCREEN, getlang("BioBadNumResp"), outnum[0],
                           outnum[1]);
                }
            else
                {
                /* If the user entered something, set the changed flag. */
                if (tresp[0])
                    {
                    /* All responses are stored as strings, even for
                       numeric questions. */
                    ltoa(tnr, br->response, 10);
                    changed = 1;
                    }
                doneresp = 1;
                }
            }
        if (bioques[rec].resptype == BIORESP_STRING)
            {
            /* Loop until the censor accepts the input. */
            do
                {
                itoa(bioques[rec].number, outnum[0], 10);
                top_output(OUT_SCREEN, getlang("BioStrRespPrompt"), outnum[0],
                           bioques[rec].field);

                od_input_str(tresp, bioques[rec].maxval, ' ',
                             MAXASCII);
                }
            while(censorinput(tresp));

            if (strlen(tresp) < bioques[rec].minval)
                {
                /* Reject the response if it was too short. */
                itoa(bioques[rec].minval, outnum[0], 10);
                top_output(OUT_SCREEN, getlang("BioBadStrResp"), outnum[0]);
                }
            else
                {
                /* If the user entered something, set the changed flag. */
                if (tresp[0])
                    {
                    strcpy(br->response, tresp);
                    changed = 1;
                    }
                doneresp = 1;
                }
        	}
        }
    while (!doneresp);

    top_output(OUT_SCREEN, getlang("BioRespSuffix"));

    return changed;
    }

/* biogetall() - Asks the user all of the biography questions.
   Parameters:  None.
   Returns:  The number of questions the user has filled out, including any
             new responses.
*/
char biogetall(void)
    {
    XINT d; /* Counter. */
    bio_resp_typ tresp; /* Response buffer. */
    char chg = 0; /* Flag indicating at least one new response. */
    char qc = 0; /* Question count. */

    top_output(OUT_SCREEN, getlang("BioGetAllPrefix"));

    for (d = 0; d < numbioques; d++)
        {
        if (!readbioresponse(bioques[d].number, &tresp))
            {
            /* Only ask for a response if one is not found. */
            if (getbioresponse(d, &tresp))
                {
                /* Write the new response if one was entered. */
                chg = 1;
                qc++;
                writebioresponse(bioques[d].number, &tresp);
                if (!user.donebio)
                    {
                    /* Flag that the user has filled out at least one
                       Bio question. */
                    user.donebio = 1;
                    save_user_data(user_rec_num, &user);
                    }
                }
            }
        else
            {
            /* Responses found on disk are counted. */
            qc++;
            }
        }

    /* Notify other users if the biography was changed. */
    if (chg)
        {
        curchannel = cmiprevchan;
        dispatch_message(MSG_BIOCHG, "\0", -1, 0, 0);
        curchannel = BUSYCHANNEL;
        }

    top_output(OUT_SCREEN, getlang("BioGetAllSuffix"));

    return qc;
    }

/* bioeditor() - Interactive biography editor.
   Parameters:  None.
   Returns:  Nothing.
*/
void bioeditor(void)
    {
    char tbuf[3]; /* Input buffer. */
    XINT d, nn; /* Counter, number of question to edit. */
    bio_resp_typ tresp; /* Response buffer. */
    char chg = 0; /* Flag indicating bio was changed. */

    /* If the user has not filled out at least one question, invite the
       user to fill out all questions. */
    if (!user.donebio)
        {
        top_output(OUT_SCREEN, getlang("BioEditDoAllPrompt"));
        d = toupper(od_get_key(TRUE));
        top_output(OUT_SCREEN, getlang("BioEditDoAllSuffix"));
        // This should use YesNoKeys...
        if (d != 'N')
            {
            biogetall();
            }
        }

    displaybio(user.handle);
    for(;;)
        {
        /* Get the question number. */
        top_output(OUT_SCREEN, getlang("BioEditPrompt"));
        od_input_str(tbuf, 2, '0', 'z');
        strupr(tbuf);
        if (!tbuf[0])
            {
            continue;
            }
        /* Quit editor. */
        if (strchr(getlang("BioEditQuitKeys"), tbuf[0]))
            {
            top_output(OUT_SCREEN, getlang("BioEditSuffix"));

            /* Notify other users if the biography was changed. */
            if (chg)
                {
                curchannel = cmiprevchan;
                dispatch_message(MSG_BIOCHG, "\0", -1, 0, 0);
                curchannel = BUSYCHANNEL;
                }
            /* Break out of the infinite loop. */
            return;
            }
        /* Redisplay biography. */
        if (strchr(getlang("BioEditShowKeys"), tbuf[0]))
            {
            displaybio(user.handle);
            continue;
            }
        /* Complete all unanswered questions. */
        if (strchr(getlang("BioEditCompKeys"), tbuf[0]))
            {
            biogetall();
            displaybio(user.handle);
            }
        nn = atoi(tbuf);
        for (d = 0; d < numbioques; d++)
            {
            /* Find the question number in memory. */
            if (nn == bioques[d].number)
                {
                /* Get a response. */
                if (getbioresponse(d, &tresp))
                    {
                    chg = 1;
                    writebioresponse(bioques[d].number, &tresp);
                    if (!user.donebio)
                        {
                        /* Flag that the user has filled out at least one
                           Bio question. */
                        user.donebio = 1;
                        save_user_data(user_rec_num, &user);
                        }
                    }
                }
            }
        if (nn < 0 || nn >= MAXBIOQUES)
            {
            top_output(OUT_SCREEN, getlang("BioEditBadQNum"));
            }
        }

    }

/* userlist() - Displays a list of user names and summaries.
   Parameters:  None.
   Returns:  Nothing.
   Notes:  This function may seem out of place here, but because it
           accesses the biographies and wasn't added until the biography
           feature was implemented, I've placed it in this module.
*/
void userlist(void)
    {
    bio_resp_typ sum; /* Buffer to hold the summary response. */
    XINT d, numus, x = 0; /* Counter, number of users, line counter. */
    user_data_typ ubuf; /* User data buffer. */
    long sloc; /* Location of summary. */
    char ns = 0; /* Nonstop mode flag. */
    XINT qrn = -1; /* Record number (in memory) of Summary question. */

    /* Question #99 is considered the "summary" question, and is shown
       beside each user name if a response exists.  This loop determines
       if a question #99 exists and finds it in memory. */
    for (d = 0; d < numbioques && qrn == -1; d++)
        {
        if (bioques[d].number == 99)
            {
            qrn = d;
            }
        }

    /* Prepare screen. */
    top_output(OUT_SCREEN, getlang("UserListHeader"),
               qrn > -1 ? bioques[qrn].field : (uchar *)"");
    top_output(OUT_SCREEN, getlang("UserListSep"));

    numus = filelength(userfil) / sizeof(user_data_typ);
    for (d = 0; d < numus; d++)
        {
        /* Load just the handle and name of each user, for speed. */
        rec_locking(REC_LOCK, userfil, (long) d * sizeof(user_data_typ),
                    72L);
        lseek(userfil, (long) d * sizeof(user_data_typ), SEEK_SET);
        read(userfil, &ubuf, 72L);
        rec_locking(REC_UNLOCK, userfil, (long) d * sizeof(user_data_typ),
                    72L);

        /* Load the location of the user's response to question #99 from
           the index. */
        rec_locking(REC_LOCK, bioidxfil,
                    ((long) d * MAXBIOQUES * sizeof(long)) +
                    (99L * sizeof(long)),
                    sizeof(long));
        lseek(bioidxfil, ((long) d * MAXBIOQUES * sizeof(long)) +
                         (99L * sizeof(long)), SEEK_SET);
        read(bioidxfil, &sloc, sizeof(long));
        rec_locking(REC_UNLOCK, bioidxfil,
                    ((long) d * MAXBIOQUES * sizeof(long)) +
                    (99L * sizeof(long)),
                    sizeof(long));

        sum.response[0] = '\0';
        /* Only try to load the response if a question #99 exists. */
        if (qrn > -1)
            {
            /* Load the response if one was found. */
            if (sloc > 0)
                {
                rec_locking(REC_LOCK, biorespfil,
                            (sloc - 1) * sizeof(bio_resp_typ),
                            sizeof(bio_resp_typ));
                lseek(biorespfil, (sloc - 1) * sizeof(bio_resp_typ),
                      SEEK_SET);
                read(biorespfil, &sum, sizeof(bio_resp_typ));
                rec_locking(REC_UNLOCK, biorespfil,
                            (sloc - 1) * sizeof(bio_resp_typ),
                            sizeof(bio_resp_typ));
                }
            }

        /* Show the entry. */
        top_output(OUT_SCREEN, getlang("UserListEntry"),
                   ubuf.handle, sum.response);
        /* Show a more prompt every 20 users. */
        if (++x % 20 == 0 && x > 0 && !ns)
            {
            x = 0;
            ns = moreprompt();
            if (ns == -1)
                {
                break;
                }
            }
        }

    top_output(OUT_SCREEN, getlang("UserListSuffix"));

    }

/* biocheckalldone() - Checks if a user has completed all bio questions.
   Parameters:  None.
   Returns:  TRUE if all bio questions have been completed, FALSE otherwise.
   Notes:  Used when the ForceBio configuration option is turned on.
*/
char biocheckalldone(void)
    {
    XINT d; /* Counter. */
    bio_resp_typ tresp; /* Response buffer. */

    /* Cycle through each question. */
    for (d = 0; d < numbioques; d++)
        {
        /* Abort with failure if no response can be loaded. */
        if (!readbioresponse(bioques[d].number, &tresp))
            {
            return 0;
            }
        }

    /* Successful completion of the loop indicates all questions have been
       completed by the user. */
    return 1;
    }
