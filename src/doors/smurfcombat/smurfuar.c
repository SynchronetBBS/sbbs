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
/* SYSOP Maintenance Code: !-S-Y-S-O-P-!                            ARENA  */
/* REMOTE Maintenance Code: !-SIX-NINE-|                           MODULE  */
/* */
/***************************************************************************/

#include "smurfext.h"




void 
userarena(void)
{
    int             ht = 0, tw = 0, ta = 0, erand, numba, ew, aw, round,
                    run = 0, hit = 0, enemyhp, old, mod;
    char            enemie[41], eweapon[41], earmor[41], bbsin[10], *enform;
    float           gf, ef;
    if (smurffights[thisuserno] < 1) {
	smurffights[thisuserno] = 0;
	od_printf("\n\rOutta Turns!\n\n\r");
	return;
    }
    /* stream = fopen("SMURF.LOG", "a+"); */
    /* fprintf(stream, "USERF(%i):%s ...
     * ",od_control.user_num,smurfname[thisuserno]); */
    /* fclose(stream); */

    userlist(1);


    od_set_colour(L_CYAN, D_BLACK);
    od_printf("Number Of Smurf To Fight: (0-%d)? ", noplayers);
    od_input_str(bbsin, 2, '0', '9');
    numba = atoi(bbsin);
    erand = xp_random(10);

    if (numba < 1)
	return;

    if (numba < 1 || numba > noplayers) {
	od_printf("\n\rNot Listed!\n\n\r");
	return;
    }
    if (numba == thisuserno + 1) {
	od_printf("\n\rWe do not encourage suicide...\n\n\r");
	return;
    }
    for (cyc = 0; cyc < hcount; cyc++) {
	if (hostage[cyc] == numba - 1 && holder[cyc] == thisuserno) {
	    od_set_colour(L_RED, D_BLACK);
	    od_printf("\n\rThat's YOUR Hostage!\n\n\r", smurfname[holder[cyc]]);
	    return;
	} else if (hostage[cyc] == numba - 1) {
	    od_set_colour(L_RED, D_BLACK);
	    od_printf("\n\rThat Smurf is %s's Hostage!\n\n\r", smurfname[holder[cyc]]);
	    return;
	}
    }
    /* if(smurfhpm[numba-1]<1 || smurfhp[numba-1]<1){
     * od_set_colour(L_RED,D_BLACK); od_printf("\n\rThat Smurf is
     * *UNDEAD*!\n\n\r");smurf_pause(); smurfhpm[numba-1]==xp_random(500)+100; } */

    numba--;
    enemyhp = smurfhpm[numba];

#ifdef TODO_WRAPPERS
    strset(eweapon, 0);
    strset(earmor, 0);
#else
    memset(eweapon, 0, sizeof(eweapon));
    memset(earmor, 0, sizeof(earmor));
#endif
    sprintf(eweapon, "%s", smurfweap[numba]);
    sprintf(earmor, "%s", smurfarmr[numba]);

    ew = (smurfweapno[numba] + 1) * 5;
    aw = (smurfarmrno[numba] + 1) * 4;
    sprintf(enemie, "%s", smurfname[numba]);
    smurffights[thisuserno]--;

    if (xp_random(25) == 1)
	registerme();
    od_clr_scr();
    od_set_colour(L_CYAN, D_BLACK);
    od_printf("%s %s\n\r", enemie, defstartline[erand]);
    od_set_colour(L_GREEN, D_BLACK);
    od_printf("Y: Weapon: %s, %i Pts\n\rY: Defense: %s, %i Pts\n\n\r", smurfweap[thisuserno], (smurfweapno[thisuserno] + 1) * 5, smurfarmr[thisuserno], (smurfarmrno[thisuserno] + 1) * 4);
    od_printf("E: Weapon: %s, %i Pts\n\rE: Defense: %s, %i Pts\n\n\r", eweapon, ew, earmor, aw);

    do {
	od_set_colour(D_CYAN, D_BLACK);
	od_printf("%s has %i points left.\n\r", dataref3[smurfsex[numba]], enemyhp);
	od_set_colour(D_CYAN, D_BLACK);
	od_printf("You have %i points left.\n\r", smurfhp[thisuserno]);
	od_set_colour(L_YELLOW, D_BLACK);
	od_printf("(A)ttack (R)un (S)tats: ", enemie);

	bbsinkey = od_get_key(TRUE);
	mod = smurfstr[thisuserno] * 5;
	if (mod > 150)
	    mod = 150;
	if (mod < 50)
	    mod = 50;
	switch (bbsinkey) {
	    case 'A':
	    case 'a':
		od_set_colour(L_CYAN, D_BLACK);
		od_printf("Attack!\n\r");
		if ((xp_random(smurfspd[thisuserno])) > (xp_random(smurfspd[numba]))) {

		    hit = ((xp_random((smurfweapno[thisuserno] + 1) * 5) * mod) / 100) + 1;
		    hit -= xp_random(aw);
		    if (hit < 0)
			hit = 0;
		    enemyhp -= hit;
		    __hit(hit);

		    if (enemyhp > 0) {
			hit = xp_random(ew) + 1;
			hit = hit - xp_random(aw);
			if (hit < 0)
			    hit = 0;
			smurfhp[thisuserno] = (smurfhp[thisuserno] - hit);
			__ehit(hit, numba);
		    }
		} else {
		    hit = xp_random(ew) + 1;
		    hit = hit - xp_random(aw);
		    if (hit < 0)
			hit = 0;
		    smurfhp[thisuserno] = (smurfhp[thisuserno] - hit);
		    __ehit(hit, numba);

		    if (smurfhp[thisuserno] > 0) {
			hit = ((xp_random((smurfweapno[thisuserno] + 1) * 5) * mod) / 100) + 1;
			hit -= xp_random(aw);
			if (hit < 0)
			    hit = 0;
			enemyhp -= hit;
			__hit(hit);
		    }
		} nl();
		nl();
		/* if(enemyhp>0 && smurfhp[thisuserno]>0)od_clr_scr(); */
		/* else */
		nl();
		nl();
		break;

	    case 'R':
	    case 'r':
		run = 1;
		od_printf("Run\n\n\rWimp!\n\n\r");
		smurflose[thisuserno]++;
		if (xp_random(10) == 1)
		    deductmorale();
		if (xp_random(3) == 1) {
		    deductettemorale();
		    od_printf("Your smurfette is ashamed to know you...\n\r");
		}
		nl();
		break;

	    case 'S':
	    case 's':
		od_set_colour(L_CYAN, D_BLACK);
		od_clr_scr();
		od_printf("Enemie: %s [Level %i]\n\r", enemie, smurflevel[numba]);
		od_set_colour(L_GREEN, D_BLACK);
		od_printf("Y: Weapon: %s, %i Pts\n\rY: Defense: %s, %i Pts\n\n\r", smurfweap[thisuserno], (smurfweapno[thisuserno] + 1) * 5, smurfarmr[thisuserno], (smurfarmrno[thisuserno] + 1) * 4);
		od_printf("E: Weapon: %s, %i Pts\n\rE: Defense: %s, %i Pts\n\n\r", eweapon, ew, earmor, aw);
		break;

	    default:
		nl();
		nl();
		break;
	}
    }
    while (enemyhp > 0 && smurfhp[thisuserno] > 0 && run != 1);

    od_set_colour(L_BLUE, D_BLACK);















    /* // // // // // // // // // // // // // // // // // // // // // // //
     * // // */
    /* // // // // // // // // // // // // // // // // // // // // // // //
     * // // */
    if (smurfhp[thisuserno] < 1) {
	smurflose[thisuserno]++;
	smurfwin[numba]++;
	gf = smurflevel[thisuserno];
	gf *= smurflevel[numba] + 1;
	gf *= (xp_random(100));
	gf += 83 * smurflevel[numba];
	gf *= .5;
	ef = smurflevel[thisuserno];
	ef *= smurflevel[numba] + 1;
	ef *= (xp_random(100));
	ef += 83 * smurflevel[numba];
	ef *= .5;
	if (gf < smurfmoney[thisuserno])
	    gf = smurfmoney[thisuserno];
	smurfmoney[thisuserno] = 0;
	smurfmoney[numba] += gf;
	smurfexper[numba] += ef;
	od_printf("\n\n\rLooks like we have a problem here...\n\n\r");
	od_set_colour(L_MAGENTA, D_BLACK);
	counthostages();

	if (numhost[numba] <= smurfconfno[numba]) {
	    ht = 1;
	    hostage[hcount] = thisuserno;
	    holder[hcount] = numba;
	    hcount++;
	    writehostage();
	    od_printf("*** You've been taken hostage! ***\n\n\r");
	    stream = fopen("smurf.new", "a+");
	    fprintf(stream, "%s took %s HOSTAGE!\n\r", smurfname[numba], smurfname[thisuserno]);
	    fclose(stream);
	}
	od_set_colour(L_BLUE, D_BLACK);

	if (smurfweapno[thisuserno] > smurfweapno[numba]) {
	    for (cyc = 0; cyc < hcount; cyc++)
		if (smurfhost[numba][0] == hostage[cyc])
		    hostage[cyc] = 255;
	    tw = 1;
	    sprintf(eweapon, "%s", smurfweap[numba]);
	    ew = smurfweapno[numba];
#ifdef TODO_WRAPPERS
	    strset(smurfweap[numba], 0);
#else
	    memset(smurfweap[numba], 0, sizeof(smurfweap[numba]));
#endif
	    sprintf(smurfweap[numba], "%s", smurfweap[thisuserno]);
#ifdef TODO_WRAPPERS
	    strset(smurfweap[thisuserno], 0);
#else
	    memset(smurfweap[thisuserno], 0, sizeof(smurfweap[thisuserno]));
#endif
	    sprintf(smurfweap[thisuserno], "%s", defweapon[smurfweapno[numba]]);
	    smurfweapno[numba] = smurfweapno[thisuserno];
	    smurfweapno[thisuserno] = ew;
	    od_printf("He took your weapon!\n\r");
	}
	if (smurfarmrno[thisuserno] > smurfarmrno[numba]) {
	    ta = 1;
	    sprintf(earmor, "%s", smurfarmr[numba]);
	    ew = smurfarmrno[numba];
#ifdef TODO_WRAPPERS
	    strset(smurfarmr[numba], 0);
#else
	    memset(smurfarmr[numba], 0, sizeof(smurfarmr[numba]));
#endif
	    sprintf(smurfarmr[numba], "%s", smurfarmr[thisuserno]);
#ifdef TODO_WRAPPERS
	    strset(smurfarmr[thisuserno], 0);
#else
	    memset(smurfarmr[thisuserno], 0, sizeof(smurfarmr[thisuserno]));
#endif
	    sprintf(smurfarmr[thisuserno], "%s", defarmor[smurfarmrno[numba]]);
	    smurfarmrno[numba] = smurfarmrno[thisuserno];
	    smurfarmrno[thisuserno] = ew;
	    od_printf("He took your armor!\n\r");
	}
	smurfhp[thisuserno] = smurfhpm[thisuserno];
	stream = fopen("smurf.new", "a+");
	fprintf(stream, "%s thrased %s!\n\r", enemie, smurfname[thisuserno]);
	if (ht == 1)
	    fprintf(stream, "%s took %s HOSTAGE!\n\r", smurfname[numba], smurfname[thisuserno]);
	fclose(stream);
	savegame();
	lastquote();
	od_exit(10, FALSE);
    }
    /* // // // // // // // // // // // // // // // // // // // // // // //
     * // // */
    /* // // // // // // // // // // // // // // // // // // // // // // //
     * // // */
    if (enemyhp < 1) {
	smurfwin[thisuserno]++;
	smurflose[numba]++;

	nl();
	sprintf(tempenform, "%s thrased %s!\n\r", smurfname[thisuserno], smurfname[numba]);
	newshit(tempenform);
	counthostages();

	od_set_colour(L_CYAN, D_BLACK);

	if (smurfexper[thisuserno] < smurfexper[numba]) {
	    od_printf("You made a MASSIVE experience jump!\n\r%s swaps experience with you!\n\n\r", smurfname[numba]);
	    old = smurfexper[thisuserno];
	    smurfexper[thisuserno] = smurfexper[numba];
	    smurfexper[numba] = old;
	}
	od_set_colour(L_MAGENTA, D_BLACK);

	if (numhost[thisuserno] <= smurfconfno[thisuserno]) {
	    ht = 1;
	    hostage[hcount] = numba;
	    holder[hcount] = thisuserno;
	    hcount++;
	    writehostage();
	    od_printf("You took %s as a hostage!\n\r", smurfname[numba]);
	    fclose(stream);
	    stream = fopen("smurf.new", "a+");
	    fprintf(stream, "%s took %s HOSTAGE!\n\r", smurfname[thisuserno], smurfname[numba]);
	    fclose(stream);
	    writehostage();
	}
	od_set_colour(L_BLUE, D_BLACK);

	od_printf("As they carry %s away, he yells:\n\r'%s'\n\n\r", smurfname[numba], smurftext[numba]);
	gf = smurflevel[thisuserno];
	gf *= smurflevel[numba] + 1;
	gf *= (xp_random(100));
	gf += 83 * smurflevel[numba];
	gf *= .5;
	ef = smurflevel[thisuserno];
	ef *= smurflevel[numba] + 1;
	ef *= (xp_random(100));
	ef += 83 * smurflevel[numba];
	ef *= .5;
	if (gf < smurfmoney[numba])
	    gf = smurfmoney[numba];
	smurfmoney[numba] = 0;

	od_printf("The arena roars as you defeat %s...\n\n\r", enemie);
	od_set_colour(L_YELLOW, D_BLACK);
	od_printf("You found %.0f gold and get %.0f experience!\n\n\r", gf, ef);
	smurfmoney[thisuserno] = (smurfmoney[thisuserno] + gf);
	smurfexper[thisuserno] = (smurfexper[thisuserno] + ef);
	if (smurfweapno[thisuserno] < smurfweapno[numba]) {
	    tw = 1;
#ifdef TODO_WRAPPERS
	    strset(smurfweap[thisuserno], 0);
#else
	    memset(smurfweap[thisuserno], 0, sizeof(smurfweap[thisuserno]));
#endif
	    sprintf(smurfweap[thisuserno], "%s", eweapon);
#ifdef TODO_WRAPPERS
	    strset(smurfweap[numba], 0);
#else
	    memset(smurfweap[numba], 0, sizeof(smurfweap[numba]));
#endif
	    sprintf(smurfweap[numba], "%s", defweapon[smurfweapno[thisuserno]]);
	    smurfweapno[numba] = smurfweapno[thisuserno];
	    smurfweapno[thisuserno] = ew / 5;
	    od_printf("You took his weapon!\n\r");
	}
	if (smurfarmrno[thisuserno] < smurfarmrno[numba]) {
	    ta = 1;
#ifdef TODO_WRAPPERS
	    strset(smurfarmr[thisuserno], 0);
#else
	    memset(smurfarmr[thisuserno], 0, sizeof(smurfarmr[thisuserno]));
#endif
	    sprintf(smurfarmr[thisuserno], "%s", earmor);
#ifdef TODO_WRAPPERS
	    strset(smurfarmr[numba], 0);
#else
	    memset(smurfarmr[numba], 0, sizeof(smurfarmr[numba]));
#endif
	    sprintf(smurfarmr[numba], "%s", defarmor[smurfarmrno[thisuserno]]);
	    smurfarmrno[numba] = smurfarmrno[thisuserno];
	    smurfarmrno[thisuserno] = aw / 4;
	    od_printf("You took his armor!\n\r");
	}
	checklevel();
	savegame();

	sprintf(logname, "smurf.%03i", numba);
	stream = fopen(logname, "a+");
	fprintf(stream, "%s thrashed you!!!\n\r", smurfname[thisuserno]);
	if (tw == 1)
	    fprintf(stream, "%s stole your %s!\n\r", smurfname[thisuserno], eweapon);
	if (ta == 1)
	    fprintf(stream, "%s stole your %s!\n\r", smurfname[thisuserno], earmor);
	if (ht == 1)
	    fprintf(stream, "***** YOU ARE A HOSTAGE OF %s *****\n\r", smurfname[thisuserno]);
	fprintf(stream, "\n\r");
	fclose(stream);
    }
    nl();

    if (numba == 0 && enemyhp < 1 && smurfexper[thisuserno] > 100000000)
	wingame();

}
