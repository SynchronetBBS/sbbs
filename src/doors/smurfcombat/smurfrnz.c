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
/* SYSOP Maintenance Code: !-S-Y-S-O-P-!                       RENDEZVOUS  */
/* REMOTE Maintenance Code: !-SIX-NINE-|                           MODULE  */
/* */
/***************************************************************************/

#include<ctype.h>
#include<genwrap.h>
#include"smurfext.h"

/* use od_disp(buffer,size,optional local echo); --- userlist(1); */


void 
__etteit(int a, int b, int c, unsigned long d)
{
    int             cost, outco_, gain, typo;
    cost = smurfettelevel[thisuserno];
    cost *= d;
    gain = xp_random(c) + b;
    od_set_colour(D_GREEN, D_BLACK);
    nl();
    nl();
    if (a > etpoints) {
	od_printf("Insufficient Points!\n\r");
	smurf_pause();
	return;
    }
    if (cost > smurfmoney[thisuserno]) {
	od_printf("Insufficient Money On Hand!\n\r");
	smurf_pause();
	return;
    }
    etpoints -= a;
    smurfmoney[thisuserno] -= cost;
    typo = xp_random(__ettemorale[thisuserno] / 2) + __ettemorale[thisuserno] / 2;
    if (a > typo + 1) {
	od_printf("Rejection!\n\r");
	deductettemorale();
	od_printf("You lost your standing with her while trying to go to far!\n\n\r");
	smurf_pause();
	return;
    }
    if (a == 10) {
	od_printf("She accepts!!!");
	if (__ettemorale[thisuserno] < 100)
	    __ettemorale[thisuserno] = 100;
    } else
	od_printf("You Score!\n\r");
    od_printf("Hey stud, look like you raised your standing with her.\n\r");
    od_printf("Morale points: +%d\n\n\r", gain);
    __ettemorale[thisuserno] += gain;
    savegame();
    smurf_pause();
}

void 
ettemenu(void)
{
    int             xms;
    ettelist(1);
    smurf_pause();
    do {
	od_clr_scr();
	od_set_colour(L_RED, D_BLACK);
	od_printf("     [ Smurfette Menu ]\n\r");
	nl();
	od_set_colour(L_CYAN, D_BLACK);
	od_printf(" No  Search Area                           Pts Price/Lvl\n\r");
	od_set_colour(D_CYAN, D_BLACK);
	od_printf(" [1] Grab Some Fast Food                     1        10\n\r");
	od_set_colour(D_CYAN, D_BLACK);
	od_printf(" [2] Have a Night on The Town                2       100\n\r");
	od_set_colour(D_CYAN, D_BLACK);
	od_printf(" [3] Wieney Roast and Sing Along Concert     3       200\n\r");
	od_set_colour(D_CYAN, D_BLACK);
	od_printf(" [4] Visit Very,Very Expensive Resturant     4       500\n\r");
	od_set_colour(D_CYAN, D_BLACK);
	od_printf(" [5] Watch The Sunset on a Cruise Boat       6      1000\n\r");
	od_set_colour(D_CYAN, D_BLACK);
	od_printf(" [6] Go on a Wilderness Camping Trip         8      2000\n\r");
	od_set_colour(D_CYAN, D_BLACK);
	od_printf(" [7] Will You Marry Me?                     10     10000\n\r");
	nl();
	od_set_colour(D_CYAN, D_BLACK);
	od_printf(" [?] Smurfette Menu General Help\n\r");
	od_set_colour(D_CYAN, D_BLACK);
	od_printf(" [Q] Return To Main Menu\n\r");
	nl();
	xms = 0;
	xms = __ettemorale[thisuserno] / 10;
	if (xms > 10)
	    xms = 11;

	od_set_colour(L_GREEN, D_BLACK);
	od_printf(" Smurfette Morale: %s (%d%%)\n\r", _ettemorale[xms], xms * 10);
	od_printf(" Points Remaining: %i\n\r", etpoints);
	od_set_colour(D_GREEN, D_BLACK);
	if (statis)
	    od_printf(" Your Choice [1-7,S,V,Q]: ? ");
	if (!statis)
	    od_printf(" Your Choice [1-4,V,Q]: ? ");
	bbsinkey = (toupper(od_get_key(TRUE)));
	if (bbsinkey == '?') {
	    od_clr_scr();
	    od_send_file("smurfdat.d08");
	    smurf_pause();
	}
	if (bbsinkey == 'V' || bbsinkey == 'v') {
	    ettelist(1);
	    smurf_pause();
	}
	if (bbsinkey == '1')
	    __etteit(1, 1, 1, 10);
	if (bbsinkey == '2')
	    __etteit(2, 2, 1, 100);
	if (bbsinkey == '3')
	    __etteit(3, 3, 1, 200);
	if (bbsinkey == '4')
	    __etteit(4, 4, 3, 500);
	if (bbsinkey == '5')
	    __etteit(6, 5, 5, 1000);
	if (bbsinkey == '6')
	    __etteit(8, 10, 10, 2000);
	if (bbsinkey == '7')
	    __etteit(10, 25, 25, 10000);

    } while (bbsinkey != 'Q' && bbsinkey != 'q');
    nl();
    nl();
}

void 
__incite(int x, int c, int y)
{
    int             outco_, xms;
    unsigned long   cost;
    char            bbsin4[10];
    userlist(1);
    od_printf("Subject? (1-%d): ", noplayers);
    od_input_str(bbsin4, 9, '0', '9');
    inuser = (atoi(bbsin4)) - 1;
    if (inuser < 0)
	return;
    if (inuser >= noplayers)
	return;
    if (c == 2)
	cost = smurfettelevel[inuser] * y;
    if (c == 1)
	cost = smurflevel[inuser] * y;
    if (c == 0)
	cost = smurfconfno[inuser] * y;
    cost *= 1000;
    od_clr_scr();
    od_set_colour(D_CYAN, D_BLACK);
    od_printf("Smurf Name: ");
    od_set_colour(L_CYAN, D_BLACK);
    od_printf("%s\n\r", smurfname[inuser]);
    if (c != 2) {
	od_set_colour(D_CYAN, D_BLACK);
	od_printf("Level:      ");
	od_set_colour(L_CYAN, D_BLACK);
	od_printf("%i\n\r", smurflevel[inuser]);
    }
    if (c == 2) {
	od_set_colour(D_CYAN, D_BLACK);
	od_printf("Smurfette:  ");
	od_set_colour(L_CYAN, D_BLACK);
	od_printf("%s\n\r", smurfettename[inuser]);
	od_set_colour(D_CYAN, D_BLACK);
	od_printf("Ettelevel:  ");
	od_set_colour(L_CYAN, D_BLACK);
	od_printf("%i\n\r", smurfettelevel[inuser]);
    }
    if (x != 1 && x < 4) {
	od_set_colour(D_CYAN, D_BLACK);
	od_printf("Confine:    ");
	od_set_colour(L_CYAN, D_BLACK);
	od_printf("%s\n\r", smurfconf[inuser]);
	od_set_colour(D_CYAN, D_BLACK);
	od_printf("Conf No:    ");
	od_set_colour(L_CYAN, D_BLACK);
	od_printf("%i\n\r", smurfconfno[inuser]);
    }
    od_set_colour(D_CYAN, D_BLACK);
    od_printf("Cost:       ");
    od_set_colour(L_CYAN, D_BLACK);
    od_printf("%lu\n\r", cost);
    od_set_colour(D_GREEN, D_BLACK);
    nl();
    if (cost > smurfmoney[thisuserno]) {
	od_printf("Insufficient Money!\n\n\r");
	smurf_pause();
	return;
    }
    od_printf("Proceed [yN]: ? ");
    bbsinkey = od_get_key(TRUE);

    od_set_colour(L_GREEN, D_BLACK);
    if (bbsinkey == 'Y' || bbsinkey == 'y') {
	od_printf("Yea\n\n\r");
	od_set_colour(L_RED, D_BLACK);
	od_printf("[Results]\n\r");
	sleep(2);
	smurfmoney[thisuserno] -= cost;

	if (x == 3) {
	    sprintf(tempenform, "%s attacked %s's confine!!!\n\r", smurfname[thisuserno], smurfname[inuser]);
	    newshit(tempenform);
	    outco_ = xp_random(y / 10);
	    if (outco_) {
		od_set_colour(L_GREEN, D_BLACK);
		od_printf("Success\n\n\r");
	    }
	    if (!outco_) {
		od_set_colour(D_RED, D_BLACK);
		od_printf("Failure\n\n\r");
		smurf_pause();
		return;
	    } od_set_colour(D_GREEN, D_BLACK);
	    sprintf(tempenform, "%s attacked your confine!!!\n\r", smurfname[thisuserno]);
	    logshit(inuser, tempenform);
	    outco_ = xp_random(y / 10) + y / 20;
	    od_printf("Confine Lowered %d Level(s)\n\r", outco_);
	    smurfconfno[inuser] -= outco_;
	    if (smurfconfno[inuser] < 1) {
		smurfconfno[inuser] = 0;
		od_printf("Confine Destroyed\n\r");
		sprintf(tempenform, "%s destroyed your confine!!!\n\r", smurfname[thisuserno]);
		logshit(inuser, tempenform);
	    }
	    nl();
	    smurf_pause();
	    return;
	}
	if (x == 2) {
	    sprintf(tempenform, "%s sabotaged %s's confine!!!\n\r", smurfname[thisuserno], smurfname[inuser]);
	    newshit(tempenform);
	    outco_ = xp_random(y / 5);
	    if (outco_) {
		od_set_colour(L_GREEN, D_BLACK);
		od_printf("Success\n\n\r");
	    }
	    if (!outco_) {
		od_set_colour(D_RED, D_BLACK);
		od_printf("Failure\n\n\r");
		smurf_pause();
		return;
	    }
	    od_set_colour(D_GREEN, D_BLACK);
	    smurfconfno[inuser] -= outco_;
	    if (smurfconfno[inuser] < 1) {
		smurfconfno[inuser] = 0;
		od_printf("Confine Destroyed\n\r");
		sprintf(tempenform, "%s destroyed your confine!!!\n\r", smurfname[thisuserno]);
		logshit(inuser, tempenform);
	    } else
		od_printf("Confine Damaged\n\r", outco_);
	    nl();
	    smurf_pause();
	    counthostages();
	    for (cyc2 = 0; cyc2 < noplayers; cyc2++) {
		for (cyc = 0; cyc < hcount; cyc++)
		    if (holder[cyc] == inuser) {
			hostage[cyc] = 255;
			holder[cyc] = 255;
		    }
	    }
	    od_printf("%d Hostage(s) have been rescued thanks to your efforts.\n\r0 Hostages remain!\n\r", numhost[inuser]);
	    if (__morale[inuser] < 2)
		od_printf("You have offended the gods severely!!!\n\r");
	    else if (__morale[inuser] < 3)
		od_printf("Your crime will not go unpunished!\n\r");
	    else if (__morale[inuser] < 5)
		od_printf("Many people will look down on you for this.\n\r");
	    else if (__morale[inuser] < 7)
		od_printf("People will look up to you for some time...\n\r");
	    else
		od_printf("Soon after, you are congradulated by the head of justice!\n\r");
	    if (__morale[inuser] < 5)
		deductmorale();
	    savegame();
	    nl();
	    smurf_pause();
	    return;
	}
	if (x == 1) {
	    sprintf(tempenform, "%s *DEMORALIZED* %s!\n\r", smurfname[thisuserno], smurfname[inuser]);
	    newshit(tempenform);
	    od_set_colour(L_GREEN, D_BLACK);
	    od_printf("Success\n\n\r");
	    od_set_colour(D_GREEN, D_BLACK);
	    od_printf("Before: %s\n\r", _morale[__morale[inuser]]);
	    __morale[inuser]++;
	    od_printf("After:  %s\n\n\r", _morale[__morale[inuser]]);
	    logshit(inuser, "Someone caused you to lost morale!\n\r");
	    nl();
	    smurf_pause();
	    return;
	}
	if (x == 5) {
	    outco_ = xp_random(4);
	    if (outco_ == 1) {
		od_set_colour(L_GREEN, D_BLACK);
		od_printf("Failure\n\rYou have been CAUGHT!!\n\r");
		od_set_colour(D_GREEN, D_BLACK);
		sprintf(tempenform, "\n\rSEXUAL HARASSMENT!!\n\r", smurfname[thisuserno], smurfname[inuser]);
		newshit(tempenform);
		sprintf(tempenform, "   In broad daylight, %s made relentless\n\r", smurfname[thisuserno]);
		newshit(tempenform);
		sprintf(tempenform, "   attempts to make %s, %s's ette\n\r", smurfettename[inuser], smurfname[inuser]);
		newshit(tempenform);
		sprintf(tempenform, "   to go to bed with him for 'a price' --Sgt. Broadwick Smurf\n\r\n\r");
		newshit(tempenform);
		sprintf(tempenform, "%s tried to pimp your smurfette!!!\n\r", smurfname[thisuserno]);
		logshit(inuser, tempenform);
		deductmorale();
		smurf_pause();
		return;
	    }
	    sprintf(tempenform, "%s's Ette Arrested ***SELLING HER BODY***\n\r", smurfname[inuser]);
	    newshit(tempenform);
	    od_set_colour(L_GREEN, D_BLACK);
	    od_printf("Success\n\n\r");
	    od_set_colour(D_GREEN, D_BLACK);
	    xms = 0;
	    xms = __ettemorale[inuser] / 10;
	    if (xms > 10)
		xms = 11;
	    od_printf("Before: %-21s (%d)\n\r", _ettemorale[xms], __ettemorale[inuser]);
	    __ettemorale[inuser] -= xp_random(9) + 1;
	    savegame();
	    xms = 0;
	    xms = __ettemorale[inuser] / 10;
	    if (xms > 10)
		xms = 11;
	    od_printf("After:  %-21s (%d)\n\n\r", _ettemorale[xms], __ettemorale[inuser]);
	    logshit(inuser, "Your smurfette has become a teenage callgirl!!!\n\r");
	    nl();
	    smurf_pause();
	    return;
	}
	if (x == 4) {
	    outco_ = xp_random(4);
	    if (outco_ == 1) {
		sprintf(tempenform, "%s tried to talk crap about you in the Times!\n\r");
		logshit(inuser, tempenform);
		od_set_colour(L_GREEN, D_BLACK);
		od_printf("Success\n\n\r");
		sprintf(tempenform, "--- %s is caught trying to enter an article\n\r", smurfname[thisuserno]);
		newshit(tempenform);
		sprintf(tempenform, "    demeaning %s in the Smurf Times!!!\n\r", smurfname[inuser]);
		newshit(tempenform);
		deductmorale();
	    } else {
		logshit(inuser, "Someone has been spreading rumors of you in the Smurf Times!\n\r");
		od_set_colour(L_GREEN, D_BLACK);
		od_printf("Success\n\n\r");
		sprintf(tempenform, "\n\rSPECIAL REPORT:\n\r");
		newshit(tempenform);
		sprintf(tempenform, "   Secret Service agent Dwight Emancipation has hard\n\r");
		newshit(tempenform);
		sprintf(tempenform, "   evidence of %s MOLESTING\n\r", smurfname[inuser]);
		newshit(tempenform);
		sprintf(tempenform, "   hostages! The mayor expressed much concern over\n\r");
		newshit(tempenform);
		sprintf(tempenform, "   the matter and urges that someone attack his confine,\n\r");
		newshit(tempenform);
		sprintf(tempenform, "   and NOW!\n\r\n\r");
		newshit(tempenform);
	    }
	    nl();
	    smurf_pause();
	    return;
	}
	if (x == 6) {
	    outco_ = xp_random(4);
	    if (outco_ == 1) {
		od_set_colour(L_GREEN, D_BLACK);
		od_printf("Failure\n\rYou have been CAUGHT!!\n\r");
		od_set_colour(D_GREEN, D_BLACK);
		sprintf(tempenform, "\n\rRAPE ATTEMPT*******\n\r", smurfname[thisuserno], smurfname[inuser]);
		newshit(tempenform);
		sprintf(tempenform, "   %s was arrested yesterday\n\r", smurfname[thisuserno]);
		newshit(tempenform);
		sprintf(tempenform, "   evening on charges of raping %s\n\r", smurfettename[inuser]);
		newshit(tempenform);
		sprintf(tempenform, "   %s, her fiance expressed desire\n\r", smurfname[inuser]);
		newshit(tempenform);
		sprintf(tempenform, "   to 'KILL THAT MOTHER F***!'\n\r\n\r");
		newshit(tempenform);
		sprintf(tempenform, "%s tried to ***RAPE*** %s!!!\n\r", smurfname[thisuserno], smurfettename[inuser]);
		logshit(inuser, tempenform);
		deductmorale();
		deductmorale();
		smurf_pause();
		return;
	    }
	    sprintf(tempenform, "\n\r o %s's ette found BRUTALLY BEATEN AND NUDE!\n\r", smurfname[inuser]);
	    newshit(tempenform);
	    sprintf(tempenform, " o %s had appearently had high amount of\n\r", smurfettename[inuser]);
	    newshit(tempenform);
	    sprintf(tempenform, " o alcohol in her blood and had no recollection of what happened.\n\r\n\r");
	    newshit(tempenform);
	    od_set_colour(L_GREEN, D_BLACK);
	    od_printf("Success\n\n\r");
	    od_set_colour(D_GREEN, D_BLACK);
	    xms = 0;
	    xms = __ettemorale[inuser] / 10;
	    if (xms > 10)
		xms = 11;
	    od_printf("Before: %-21s (%d)\n\r", _ettemorale[xms], __ettemorale[inuser]);
	    __ettemorale[inuser] -= xp_random(29) + 21;
	    savegame();
	    xms = 0;
	    xms = __ettemorale[inuser] / 10;
	    if (xms > 10)
		xms = 11;
	    od_printf("After:  %-21s (%d)\n\n\r", _ettemorale[xms], __ettemorale[inuser]);
	    logshit(inuser, "Your smurfette was raped!!!\n\r");
	    nl();
	    smurf_pause();
	    return;
	}
    } else {
	od_printf("Nope\n\n\r");
    }
}

void 
inciterevolt(void)
{
    char            bbsin4[11];
    do {
	od_clr_scr();
	od_set_colour(L_RED, D_BLACK);
	od_printf("     [ Civil Operations ]\n\r");
	nl();
	od_set_colour(L_CYAN, D_BLACK);
	od_printf(" No  Operation Type              Price Per\n\r");
	od_set_colour(D_CYAN, D_BLACK);
	od_printf(" [1] Demoralize Player           5,000 Level\n\r");
	od_printf(" [2] Publicize Hostage Abuse     5,000 Level\n\r");
	od_printf(" [3] Demoralize Smurfette        5,000 Level\n\r");
	od_printf(" [4] Rape Smurfette             10,000 Level\n\r");
	od_printf(" [5] Support Rescue Operation   10,000 Confn\n\r");
	od_printf(" [6] Attempt Rescue Operation   20,000 Confn\n\r");
	od_printf(" [7] Sabotage Confine           20,000 Confn\n\r");
	od_printf(" [8] Muster Confine Attack     100,000 Confn\n\r");
	nl();
	od_printf(" [?] Civil Operations Help\n\r");
	od_printf(" [V] View Smurf Ette Listing\n\r");
	od_printf(" [Q] Return To Main Menu\n\r");
	nl();
	od_set_colour(D_GREEN, D_BLACK);
	if (statis)
	    od_printf(" Your Choice [1-5,S,V,Q]: ? ");
	if (!statis)
	    od_printf(" Your Choice [1-3,5,V,Q]: ? ");
	bbsinkey = od_get_key(TRUE);
	if (bbsinkey == '?') {
	    od_clr_scr();
	    od_send_file("smurfdat.d06");
	    smurf_pause();
	}
	if (bbsinkey == 'v' || bbsinkey == 'V') {
	    userlist(1);
	    smurf_pause();
	}
	if (bbsinkey == '1') {
	    __incite(1, 1, 5);
	}
	if (bbsinkey == '2') {
	    __incite(4, 1, 5);
	}
	if (bbsinkey == '3') {
	    __incite(5, 2, 5);
	}
	if (bbsinkey == '5') {
	    __incite(2, 0, 10);
	}
	if (bbsinkey == '7') {
	    __incite(3, 0, 20);
	} if (statis) {
	    if (bbsinkey == '4') {
		__incite(6, 2, 5);
	    }
	    if (bbsinkey == '6') {
		__incite(2, 0, 20);
	    }
	    if (bbsinkey == '8') {
		__incite(3, 0, 100);
	    }
	}
	if (!statis)
	    if (bbsinkey == '4' || bbsinkey == '6' || bbsinkey == '8')
		mustregister();
    } while (bbsinkey != 'Q' && bbsinkey != 'q');
    nl();
    nl();
}

void 
rendezvous()
{
    char            bbsin4[4];
    ettelist(2);
    smurf_pause();
    do {
	od_clr_scr();
	od_set_colour(L_RED, D_BLACK);
	od_printf("     [ ReNdEz-VouS Menu ]\n\r");
	nl();
	od_set_colour(L_CYAN, D_BLACK);
	od_printf(" No  Search Area              Pts  %\n\r");
	od_set_colour(D_CYAN, D_BLACK);
	od_printf(" [1] Probe City Streets         1 99\n\r");
	od_printf(" [2] Village Communities        2 40\n\r");
	od_printf(" [3] Industrial Areas           4 50\n\r");
	od_printf(" [4] Local Education Facils     6 20\n\r");
	od_printf(" [5] Night Life                10 80\n\r");
	od_printf(" [6] Journey To Other Lands    25 20\n\r");
	od_printf(" [7] Engage Full Blown Seance  25 ??\n\r");
	nl();
	od_printf(" [S] Attempt To Steal Smurfette\n\r");
	od_printf(" [V] View Smurf Ette Listing\n\r");
	od_set_colour(D_CYAN, D_BLACK);
	od_printf(" [?] Rendez-Vous General Help\n\r");
	od_printf(" [Q] Return To Main Menu\n\r");
	nl();
	od_set_colour(L_GREEN, D_BLACK);
	od_printf(" Points Remaining: %i\n\r", vpoints);
	od_set_colour(D_GREEN, D_BLACK);
	if (statis)
	    od_printf(" Your Choice [1-7,S,V,Q]: ? ");
	if (!statis)
	    od_printf(" Your Choice [1-4,V,Q]: ? ");
	bbsinkey = od_get_key(TRUE);

	if (bbsinkey == '?') {
	    od_clr_scr();
	    od_send_file("smurfdat.d07");
	    smurf_pause();
	}
	if (bbsinkey == '1') {
	    rva(1);
	}
	if (bbsinkey == '2') {
	    rva(2);
	}
	if (bbsinkey == '3') {
	    rva(3);
	}
	if (bbsinkey == '4') {
	    rva(4);
	}
	if (statis) {
	    if (bbsinkey == '5') {
		rva(5);
	    }
	    if (bbsinkey == '6') {
		rva(6);
	    }
	    if (bbsinkey == '7') {
		rva(7);
	    }
	    if (bbsinkey == 'S' || bbsinkey == 's') {
		ettelist(2);
		od_printf("Steal Which Ette? ");
		od_input_str(bbsin4, 3, '0', '9');
		savegame();
	    }
	}
	if (!statis)
	    if (bbsinkey == '5' || bbsinkey == '6' || bbsinkey == '7' || bbsinkey == 'S' || bbsinkey == 's')
		mustregister();
	if (bbsinkey == 'V' || bbsinkey == 'v') {
	    ettelist(2);
	    smurf_pause();
	}
    } while (bbsinkey != 'Q' && bbsinkey != 'q');
    nl();
    nl();
}








void 
rva(int sel__)
{
    int             charg, chance, bevl, dmx, cst, outco_;
    if (sel__ == 1) {
	charg = 1;
	chance = 99;
	bevl = 1;
	dmx = 2;
	cst = 1000;
    }
    if (sel__ == 2) {
	charg = 2;
	chance = 40;
	bevl = 3;
	dmx = 4;
	cst = 2000;
    }
    if (sel__ == 3) {
	charg = 4;
	chance = 50;
	bevl = 2;
	dmx = 3;
	cst = 1000;
    }
    if (sel__ == 4) {
	charg = 6;
	chance = 20;
	bevl = 5;
	dmx = 5;
	cst = 5000;
    }
    if (sel__ == 5) {
	charg = 10;
	chance = 80;
	bevl = 6;
	dmx = 3;
	cst = 2000;
    }
    if (sel__ == 6) {
	charg = 25;
	chance = 20;
	bevl = 7;
	dmx = 3;
	cst = 10000;
    }
    if (sel__ == 7) {
	charg = 25;
	chance = 80;
	bevl = 1;
	dmx = 9;
	cst = 100;
    }
    od_clr_scr();
    od_set_colour(D_CYAN, D_BLACK);
    od_printf("Rendezvous Points: ");
    od_set_colour(L_CYAN, D_BLACK);
    od_printf("%i Points\n\r", vpoints);
    od_set_colour(D_CYAN, D_BLACK);
    od_printf("           Charge: ");
    od_set_colour(L_CYAN, D_BLACK);
    od_printf("%i Points\n\r", charg);
    od_set_colour(D_CYAN, D_BLACK);
    od_printf("          Expense: ");
    od_set_colour(L_CYAN, D_BLACK);
    od_printf("%i Gold\n\r", cst);
    od_set_colour(D_GREEN, D_BLACK);
    nl();
    if (charg > vpoints) {
	od_printf("Insufficient Points!\n\n\r");
	smurf_pause();
	return;
    }
    if (cst > smurfmoney[thisuserno]) {
	od_printf("Insufficient Gold On Hand!\n\n\r");
	smurf_pause();
	return;
    }
    od_printf("Attempt ReNdEz-VouS [yN]: ? ");
    bbsinkey = od_get_key(TRUE);
    od_set_colour(L_GREEN, D_BLACK);
    if (bbsinkey == 'Y' || bbsinkey == 'y') {
	vpoints -= charg;
	od_printf("Yea\n\n\r");
	od_set_colour(L_RED, D_BLACK);
	od_printf("Success : %i%%\n\rOutcome : ", chance);
	sleep(2);
	outco_ = xp_random(101);
	if (outco_ <= chance) {
	    od_set_colour(L_GREEN, D_BLACK);
	    od_printf("ReNdEz-VouS\n\n\r");
	}
	if (outco_ > chance) {
	    od_set_colour(D_RED, D_BLACK);
	    od_printf("Failure\n\n\r");
	    smurf_pause();
	    return;
	}
	charg = xp_random(dmx) + bevl;
	chance = xp_random(20);
	nl();
	od_set_colour(L_MAGENTA, D_BLACK);
	od_printf("Smurfette Name : %s\n\r", ettenamez[chance]);
	od_printf("Level: %i\n\r", charg);
	nl();
	etpoints += charg * 10;
	if (etpoints < 25)
	    etpoints = 25;
	od_set_colour(D_RED, D_BLACK);
	od_printf("Acceptable [yN]: ? ");
	od_set_colour(L_RED, D_BLACK);
	bbsinkey = od_get_key(TRUE);
	if (bbsinkey == 'Y' || bbsinkey == 'y') {
	    vpoints -= charg;
	    sprintf(tempenform, "%s ReNDeZ-VouS avec %s\n\r", ettenamez[chance], smurfname[thisuserno]);
	    newshit(tempenform);
	    od_printf("Yea\n\n\r");
	    __ettemorale[thisuserno] = 0;
#ifdef TODO_WRAPPERS
	    strset(smurfettename[thisuserno], 0);
#else
	    memset(smurfettename[thisuserno], 0, sizeof(smurfettename[thisuserno]));
#endif
	    sprintf(smurfettename[thisuserno], "%s", ettenamez[chance]);
	    smurfettelevel[thisuserno] = charg;
	    etpoints = smurfettelevel[thisuserno] * 10;
	    if (etpoints > 25)
		etpoints = 25;
	} else
	    od_printf("Nope\n\n\r");
    } else {
	od_printf("Nope\n\n\r");
    }
}












void 
ettelist(int high)
{
    int             ref[256], conflicted, listno = noplayers;
    for (cyc = 0; cyc < noplayers; cyc++) {
	conflicted = 0;
	if (smurfexper[cyc] == 0)
	    listno--;
	else {
	    for (cyc2 = 0; cyc2 < noplayers; cyc2++) {
		if (smurfexper[cyc2] > smurfexper[cyc])
		    conflicted++;
	    }
	    ref[conflicted] = cyc;
	}
    }
    od_clr_scr();
    od_set_attrib(SD_RED);
    od_disp_str(" No.   Smurf Name                     Smurfette            Morale Level\n\r");
    od_set_attrib(SD_YELLOW);
    od_disp_str("------------------------------------------------------------------------\n\r");
    for (cyc = 0; cyc < listno; cyc++) {
	od_set_attrib(SD_GREEN);
	if (thisuserno == ref[cyc])
	    od_set_colour(L_CYAN, D_BLACK);
	od_printf(" %2i :  ", cyc + 1);
	od_set_colour(D_GREY, D_BLACK);
	if (thisuserno == ref[cyc])
	    od_set_colour(L_CYAN, D_BLACK);
	od_printf("%-30s %-20s", smurfname[ref[cyc]], smurfettename[ref[cyc]]);
	if (high == 1)
	    od_set_colour(L_CYAN, D_BLACK);
	od_printf("%5i%%", __ettemorale[ref[cyc]]);
	od_set_colour(D_GREY, D_BLACK);
	if (thisuserno == ref[cyc])
	    od_set_colour(L_CYAN, D_BLACK);
	if (high == 2)
	    od_set_colour(L_CYAN, D_BLACK);
	od_printf("%5i\n\r", smurfettelevel[ref[cyc]]);
    }
    for (cyc = 0; cyc < noplayers; cyc++) {
	if (smurfexper[cyc] == 0) {
	    listno++;
	    od_set_attrib(SD_GREEN);
	    if (thisuserno == cyc)
		od_set_colour(L_CYAN, D_BLACK);
	    od_printf(" %2i :  ", listno);
	    od_set_colour(D_GREY, D_BLACK);
	    if (thisuserno == cyc)
		od_set_colour(L_CYAN, D_BLACK);
	    od_printf("%-30s %-20s", smurfname[cyc], smurfettename[cyc]);
	    if (high == 1)
		od_set_colour(L_CYAN, D_BLACK);
	    od_printf("%5i%%", __ettemorale[cyc]);
	    od_set_colour(D_GREY, D_BLACK);
	    if (thisuserno == cyc)
		od_set_colour(L_CYAN, D_BLACK);
	    if (high == 2)
		od_set_colour(L_CYAN, D_BLACK);
	    od_printf("%5i\n\r", smurfettelevel[cyc]);
	    /* od_printf("%-30s %-20s %5i%%
	     * %5i\n\r",smurfname[cyc],smurfettename[cyc],__ettemorale[cyc],sm
	     * urfettelevel[cyc]); */
	}
    }
    nl();
}
