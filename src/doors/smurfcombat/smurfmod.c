/***************************************************************************/
/* */
/* sss                       fff     ccc                 b               */
/* s   s                     f   f   c   c                b               */
/* s                         f       c                    b               */
/* sss  mmm mmm  u   u r rr  fff    c      ooo  mmm mmm   bbb  aaa  ttt  */
/* s m  m   m u   u rr   f       c     o   o m  m   m b   b a  a  t   */
/* s   s m  m   m u  uu r    f       c   c o   o m  m   m b   b aaaa  t   */
/* sss  m  m   m  uu u r    f        ccc   ooo  m  m   m  bbb  a  a  t   */
/* */
/* SYSOP Maintenance Code: !-S-Y-S-O-P-!                         ALTERNATE */
/* REMOTE Maintenance Code: !-SIX-NINE-|                         MODULE    */
/* */
/***************************************************************************/

#include <genwrap.h>
#include "smurfext.h"





void 
spy(void)
{
    char            bbsin4[10];
    unsigned long   price = 0, r;
    userlist(1);
    r = xp_random(3);
    od_set_colour(L_BLUE, D_BLACK);
    od_printf("Spy On Who? (1-%d): ", noplayers);
    od_input_str(bbsin4, 9, '0', '9');
    inuser = (atoi(bbsin4)) - 1;
    od_clr_scr();
    if (r == 0)
	od_printf("The legendary Murf Smurf HIMSELF reviews your request...\n\n\r");
    if (r == 1)
	od_printf("The Smurfland cheif of defense reviews your request...\n\n\r");
    if (r == 2)
	od_printf("The secretary of state reviews your request...\n\n\r");
    r = xp_random(50) + 50;
    price = smurflevel[inuser] * smurflevel[thisuserno];
    price *= r;
    od_printf("Subject: %s\n\r", smurfname[inuser]);
    od_printf("Level:   %d / %d Exp\n\n\r", smurflevel[inuser], smurfexper[inuser]);
    od_printf("Price:   %lu\n\n\r", price);
    if (smurflevel[inuser] > 19)
	od_printf("In my personal opinion, the Secret Service would better suite your\n\rneeds, however we'll do our best...\n\n\r");
    else if (smurflevel[inuser] > 9)
	od_printf("Well, this shouldn't be that tough a job, I'll get on it\n\rpersonally...\n\n\r");
    else
	od_printf("Okey Dokey, you got the PERFECT person for the job, deal? ...\n\n\r");
    if (smurfmoney[thisuserno] < price) {
	od_printf("Oops, looks like you don't have enough money!\n\n\r");
	return;
    }
    od_printf("Accept [Yn]: ?");
    bbsinkey = od_get_key(TRUE);
    nl();
    nl();
    if (bbsinkey == 'N' || bbsinkey == 'n')
	return;
    displaystatus();
    smurfmoney[thisuserno] -= price;
    savegame();
}

void 
toomany(void)
{
    od_clr_scr();
    od_set_colour(L_CYAN, D_BLACK);
    od_printf("Sorry, but this game is SOOOOOOoooooooooooooooooooooooo popular\n\r");
    od_printf("that there isn't any more room for new players!\n\n\r");
    od_printf("Tell your SysOp to make a new directory, copy all the files and\n\r");
    od_printf("run SMURFNEW in that 2nd directory to start up a SECOND GAME!!\n\n\n\r");
    sleep(5);
    od_exit(10, FALSE);
}










void 
killeduser(void)
{
    char            _rame[40], _rame2[40];
    sprintf(_rame, "%s", realname[thisuserno]);
    sprintf(_rame2, "%s", od_control.user_name);
    if (thisuserno == 255)
	return;
    if (_rame2[0] != _rame[0] && _rame2[2] != _rame[2]) {
	od_clr_scr();
	od_set_colour(L_WHITE, D_BLACK);
	od_printf("When old players are erased on a bulletin board, their characters\n\r");
	od_printf("are not.   Your name does not match the smurf record of your user\n\r");
	od_printf("account number.  By pressing No on the following,  this character\n\r");
	od_printf("will be computerized, and you will get your own slot.\n\n\r");
	od_set_colour(L_CYAN, D_BLACK);
	od_printf("Is your name '%s'? [Yn]: ", realname[thisuserno]);
	bbsinkey = od_get_key(TRUE);
	nl();
	nl();
	if (bbsinkey != 'N' && bbsinkey != 'n')
	    return;
	realnumb[thisuserno] = 0;
	thisuserno = 255;
    }
}


void 
heal(void)
{
    unsigned int    cost, numba, injur, intelmod, maxheal;
    char            bbsin[5];
    intelmod = smurfint[thisuserno];
    if (intelmod > 20)
	intelmod = 20;
    injur = smurfhpm[thisuserno] - smurfhp[thisuserno];
    cost = (((smurflevel[thisuserno] * 10)) * (((20 - intelmod) * .05) + 1));
    od_set_colour(L_BLUE, D_BLACK);
    if (smurfhp[thisuserno] + 1 > smurfhpm[thisuserno]) {
	od_printf("\n\n\rYou're not hurt!\n\n\r");
	return;
    }
    if (smurfmoney[thisuserno] < cost) {
	od_printf("\n\n\rCome back when you have money!\n\n\r");
	return;
    }
    od_clr_scr();
    od_set_colour(L_MAGENTA, D_BLACK);
    od_printf("Doctor Smurf: It'll cost you %i gold per point for me to heal.\n\r", cost);
    od_printf("You have %.0f gold and %i injuries.\n\n\r", smurfmoney[thisuserno], injur);
    od_set_colour(L_RED, D_BLACK);
    maxheal = injur;
    if (maxheal * cost > smurfmoney[thisuserno])
	maxheal = smurfmoney[thisuserno] / cost;
    od_printf("Heal How Much? [%i] :", maxheal);
    od_set_colour(L_YELLOW, D_BLACK);
    od_input_str(bbsin, 3, '0', '9');
    numba = atoi(bbsin);
    if (numba > injur || numba == 0)
	numba = injur;
    if (numba > maxheal)
	numba = maxheal;
    if (smurfmoney[thisuserno] < cost * numba) {
	od_printf("\n\n\rCome back when you have enuff money!\n\n\r");
	return;
    }
    od_printf("%i points healed.\n\n\r", numba);
    smurfhp[thisuserno] = smurfhp[thisuserno] + numba;
    smurfmoney[thisuserno] = smurfmoney[thisuserno] - numba * cost;
}


void 
userlist(int ink)
{
    if (ink == 0) {
	od_set_colour(L_CYAN, D_BLACK);
	od_disp_str("\n\n\rList --> All [DEFAULT]\n\rRank --> Top (1)0\n\r         Top (2)0\n\r         (A)ll\n\r         (S)murfettes\n\r");
	bbsinkey = od_get_key(TRUE);
	if (bbsinkey == '1') {
	    rankuserlist(1);
	    return;
	}
	if (bbsinkey == '2') {
	    rankuserlist(2);
	    return;
	}
	if (bbsinkey == 'A') {
	    rankuserlist(3);
	    return;
	}
	if (bbsinkey == 'a') {
	    rankuserlist(3);
	    return;
	}
	if (bbsinkey == 'S') {
	    ettelist(0);
	    return;
	}
	if (bbsinkey == 's') {
	    ettelist(0);
	    return;
	}
    }
    od_clr_scr();
    od_set_attrib(SD_RED);
    od_disp_str(" No.   Smurf Name                  Level Wins:Loss Conf Smurfette Morale\n\r");
    od_set_attrib(SD_YELLOW);
    od_disp_str("-------------------------------------------------------------------------\n\r");
    for (cyc = 0; cyc < noplayers; cyc++) {
	od_set_attrib(SD_GREEN);
	if (thisuserno == cyc)
	    od_set_colour(L_CYAN, D_BLACK);
	for (cyc3 = 0; cyc3 < hcount; cyc3++)
	    if (hostage[cyc3] == cyc)
		od_set_colour(D_GREEN, D_BLACK);
	od_printf(" %2i :  ", cyc + 1);
	od_set_colour(D_GREY, D_BLACK);
	if (thisuserno == cyc)
	    od_set_colour(L_CYAN, D_BLACK);
	od_printf("%-30s %2i %4i:%4i %4i %4i %3i%% %5i%%\n\r", smurfname[cyc], smurflevel[cyc], smurfwin[cyc], smurflose[cyc], smurfconfno[cyc], smurfettelevel[cyc], __ettemorale[cyc], 130 - __morale[cyc] * 10);
    }
    nl();
}

void 
changename(void)
{
    char            inputline[80];
    od_clr_scr();
    od_set_colour(D_CYAN, D_BLACK);
    od_printf("\n\rCurrent Name: %s\n\n\r", smurfname[thisuserno]);
    od_set_attrib(SD_RED);
getname:;
    od_printf("Enter in your smurf name (i.e. WARRIOR SMURF) of 28 character or less.\n\r");
    od_set_colour(L_YELLOW, D_BLACK);
    od_printf(": ");
    od_set_attrib(SD_GREEN);
    od_input_str(inputline, 25, 32, 167);
    if (strlen(inputline) < 2) {
	od_printf("Aborted . . .\n\r");
	smurf_pause();
	return;
    }
    for (cyc = 0; cyc + 1 < noplayers; cyc++) {
	od_set_attrib(SD_GREEN);
	if (strcmp(smurfname[cyc], strupr(inputline)) == 0) {
	    od_set_colour(L_CYAN, D_BLACK);
	    od_printf("Same name found in rebel roster!!!\n\n\n\r");
	    goto getname;
	}
    }
    od_set_attrib(SD_CYAN);
    od_printf("Is this cool? ");
    do {
	bbsinkey = od_get_key(TRUE);
	od_set_colour(L_CYAN, D_BLACK);
	if (bbsinkey == 'N' || bbsinkey == 'n') {
	    od_printf("Nope");
	    nl();
	    nl();
	    nl();
	    return;
	}
    } while (bbsinkey != 'Y' && bbsinkey != 'y');
    od_set_colour(D_CYAN, D_BLACK);
    od_printf("Yea\n\n\r");
#ifdef TODO_WRAPPERS
    strset(smurfname[thisuserno], 0);
#else
    memset(smurfname[thisuserno], 0, sizeof(smurfname[thisuserno]));
#endif
    sprintf(smurfname[thisuserno], "%s", strupr(inputline));
}

void 
changelast(void)
{
    char            inputline[80];
    od_clr_scr();
    od_set_colour(D_CYAN, D_BLACK);
    od_printf("\n\rNow: %s\n\n\r", smurftext[thisuserno]);
    od_set_attrib(SD_RED);
    od_printf("When someone wins you in a fight, what do you want to say? (i.e. I'LL BE BACK!)\n\r");
    od_set_colour(L_YELLOW, D_BLACK);
    od_printf(": ");
    od_set_attrib(SD_GREEN);
    od_input_str(inputline, 70, 32, 167);
    if (strlen(inputline) < 2) {
	od_printf("Aborted . . .\n\r");
	smurf_pause();
	return;
    }
    od_set_attrib(SD_CYAN);
    od_printf("Is this cool? ");
    do {
	bbsinkey = od_get_key(TRUE);
	od_set_colour(L_CYAN, D_BLACK);
	if (bbsinkey == 'N' || bbsinkey == 'n') {
	    od_printf("Nope");
	    nl();
	    nl();
	    nl();
	    return;
	}
    } while (bbsinkey != 'Y' && bbsinkey != 'y');
    od_set_colour(D_CYAN, D_BLACK);
    od_printf("Yea\n\n\r");
#ifdef TODO_WRAPPERS
    strset(smurftext[thisuserno], 0);
#else
    memset(smurftext[thisuserno], 0, sizeof(smurftext[thisuserno]));
#endif
    sprintf(smurftext[thisuserno], "%s", inputline);
}
