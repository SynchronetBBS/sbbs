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
/* SYSOP Maintenance Code: !-S-Y-S-O-P-!                          HOSTAGE  */
/* REMOTE Maintenance Code: !-SIX-NINE-|                           MODULE  */
/* */
/***************************************************************************/

#include <genwrap.h>
#include "smurfext.h"





void 
hostagemenu(void)
{
    int             quit = 0, tnumba, thishostcount, numba;
    char            bbsin500[10], enform[20];
    __mess(4);
    do {
	od_clr_scr();
	od_set_colour(L_RED, D_BLACK);
	od_printf("      [ Hostage Menu ]\n\r");
	nl();
	thishostcount = 0;
	od_set_colour(D_RED, D_BLACK);
	od_printf(" Hostages: ");
	od_set_colour(L_BLUE, D_BLACK);
	for (cyc = 0; cyc < hcount; cyc++)
	    if (holder[cyc] == thisuserno) {
		thishostcount++;
		od_printf("%s\n\r           ", smurfname[hostage[cyc]]);
	    }
	if (thishostcount == 0)
	    od_printf("NONE\n\r");
	od_set_colour(D_RED, D_BLACK);
	od_printf("\r    Total: ");
	od_set_colour(L_BLUE, D_BLACK);
	od_printf("%d\n\n\r", thishostcount);
	od_set_colour(L_CYAN, D_BLACK);
	od_printf(" No  Treatment Name             Pts\n\r");
	od_set_colour(D_CYAN, D_BLACK);
	od_printf(" [1] Torture Hostage              1\n\r");
	od_printf(" [2] Abuse Hostage                3\n\r");
	od_printf(" [3] Batter Hostage               6\n\r");
	od_printf(" [4] Transfer Hostage             0\n\r");
	od_printf(" [5] Release Hostage              0\n\r");
	od_printf(" [?] Hostage Help\n\r");
	od_printf(" [Q] Quit Menu\n\n\r");

	od_set_colour(L_GREEN, D_BLACK);
	od_printf(" Points Remaining: %i\n\r", torturepoints);
	od_set_colour(D_GREEN, D_BLACK);
	if (statis)
	    od_printf(" Your Choice [1-5,Q]: ? ");
	if (!statis)
	    od_printf(" Your Choice [1-4,V,Q]: ? ");
	bbsinkey = od_get_key(TRUE);

	switch (bbsinkey) {
	    case '1':
		treathostage(1);
		break;
	    case '2':
		treathostage(2);
		break;
	    case '3':
		treathostage(3);
		break;
	    case '4':
		od_printf("\n\n\rTransfer Who (1-%i): ", thishostcount);
		od_input_str(bbsin500, 2, '0', '9');
		numba = atoi(bbsin500);
		if (numba > 0 && numba <= noplayers) {
		    userlist(1);
		    od_printf("\n\rTo Who (1-%i): ", noplayers);
		    od_input_str(bbsin500, 2, '0', '9');
		    tnumba = atoi(bbsin500);
		    od_set_colour(L_BLUE, D_BLACK);
		    od_printf("Are You Sure [yN]: ");
		    od_set_colour(L_CYAN, D_BLACK);
		    bbsinkey = od_get_key(TRUE);
		    if (bbsinkey == 'Y' || bbsinkey == 'y') {
			od_printf("Yea\n\n\r");
			cyc3 = 0;
			for (cyc = 0; cyc3 < numba; cyc++) {
			    if (holder[cyc] == thisuserno)
				cyc3++;
			} cyc--;
			od_set_colour(L_CYAN, D_BLACK);
			od_printf("%s transfered.\n\n\r", smurfname[hostage[cyc]]);
			sprintf(tempenform, "%s *TRADED* you to %s!!!\n\r", smurfname[thisuserno], smurfname[tnumba - 1]);
			logshit(hostage[cyc], tempenform);
			holder[cyc] = tnumba - 1;
			writehostage();
		    } else
			od_printf("Nope\n\n\r");
		    smurf_pause();
		} break;
	    case '5':
		od_printf("\n\n\rRelease Who (1-%i): ", thishostcount);
		od_input_str(bbsin500, 2, '0', '9');
		numba = atoi(bbsin500);
		if (numba > 0 && numba <= thishostcount) {
		    od_set_colour(L_BLUE, D_BLACK);
		    od_printf("Are You Sure [yN]: ");
		    od_set_colour(L_CYAN, D_BLACK);
		    bbsinkey = od_get_key(TRUE);
		    if (bbsinkey == 'Y' || bbsinkey == 'y') {
			od_printf("Yea\n\n\r");
			cyc = 0;
			for (cyc3 = 0; cyc < numba; cyc3++) {
			    if (holder[cyc3] == thisuserno)
				cyc++;
			} cyc = cyc3 - 1;
			od_set_colour(L_CYAN, D_BLACK);
			od_printf("%s released.\n\n\r", smurfname[hostage[cyc]]);
			sprintf(tempenform, "%s *FREED* You!!!\n\r", smurfname[thisuserno]);
			logshit(hostage[cyc], tempenform);
			if (xp_random(20) == 1) {
			    increasemorale();
			    nl();
			}
			holder[cyc] = 255;
			hostage[cyc] = 255;
			writehostage();
		    } else
			od_printf("Nope\n\n\r");
		    smurf_pause();
		} break;
	    case '?':
		od_clr_scr();
		od_send_file("smurfdat.d03");
		smurf_pause();
		break;
	    case 'Q':
	    case 'q':
		nl();
		nl();
		quit = 1;
		break;
	}
    } while (quit != 1);
    writehostage();
    savegame();
    counthostages();
}


void 
treathostage(int mere)
{
    int             fatality;
    int             total = 0;
    int             thishostcount, numba;
    char            bbsin500[10];
    int             tcost = 0;
    fatality = mere * 20 + 20 * smurfsex[hostage[cyc]] - 20;
    thishostcount = 0;
    for (cyc = 0; cyc < hcount; cyc++)
	if (holder[cyc] == thisuserno)
	    thishostcount++;
    total = 0;
    od_printf("\n\n\rHostage (1-%i): ", thishostcount);
    od_input_str(bbsin500, 2, '0', '9');
    numba = atoi(bbsin500);
    if (numba > 0 && numba <= thishostcount) {
	od_set_colour(L_GREEN, D_BLACK);
	od_printf("Fatality will result in negative effect! (%d percent chance)\n\r", fatality);
	od_set_colour(L_BLUE, D_BLACK);
	od_printf("Proceed [yN]: ");
	od_set_colour(L_CYAN, D_BLACK);
	bbsinkey = od_get_key(TRUE);
	if (bbsinkey == 'Y' || bbsinkey == 'y') {
	    od_printf("Yea\n\n\r");
	    cyc = 0;
	    for (cyc3 = 0; cyc < numba; cyc3++) {
		if (holder[cyc3] == thisuserno)
		    cyc++;
	    }
	    od_clr_scr();
	    cyc = cyc3 - 1;
	    if (mere == 1)
		tcost = 1;
	    if (mere == 2)
		tcost = 3;
	    if (mere == 3)
		tcost = 6;
	    if (torturepoints < tcost) {
		od_printf("Urr, I think you've had enuff...\n\n\r");
		smurf_pause();
		return;
	    } torturepoints -= tcost;
	    od_set_color(D_GREEN, D_BLACK);
	    if (mere == 1)
		od_printf("Torture Hostage: Attempting to inflict bodily harm with risk of overkill.\n\n\r");
	    if (mere == 2)
		od_printf("Abuse Hostage: Physical or Sexual abuse with high risk of overkill.\n\n\r");
	    if (mere == 3)
		od_printf("Batter Hostage: Severe torture and/or abuse with intent of fatality.\n\n\r");
	    od_set_colour(L_CYAN, D_BLACK);
	    od_printf("Subject:  %s%d\n\r", smurfname[hostage[cyc]], cyc);
	    od_printf("Gender:   %s\n\r", datasex[smurfsex[hostage[cyc]]]);
	    od_printf("Fatality: %i\%\n\n\r", fatality);
	    od_set_colour(L_BLUE, D_BLACK);
	    total = 0;

	    for (cyc3 = 0; cyc3 < 10; cyc3++) {
		cyc2 = xp_random(100);
		if (cyc2 < 20)
		    od_printf("ú");
		if (cyc2 < 40)
		    od_printf("-");
		if (cyc2 < 60)
		    od_printf("=");
		if (cyc2 < 81)
		    od_printf("ð");
		if (cyc2 > 80)
		    od_printf("þ");
		total += cyc2;
		od_sleep(200);
	    }
	    if (mere == 1) {
		sprintf(tempenform, "%s Tortured %s!\n\r", smurfname[thisuserno], smurfname[hostage[cyc]]);
		newshit(tempenform);
	    }
	    if (mere == 2) {
		sprintf(tempenform, "%s ABUSED %s!!!\n\r", smurfname[thisuserno], smurfname[hostage[cyc]]);
		newshit(tempenform);
	    }
	    if (mere == 3) {
		sprintf(tempenform, "%s *BATTERED* %s!!!!!!!!!!\n\r", smurfname[thisuserno], smurfname[hostage[cyc]]);
		newshit(tempenform);
	    }
	    if (total < fatality * 10) {
		od_printf("°");
		od_sleep(200);
		od_printf("÷");
		od_sleep(200);
		od_printf("±");
		od_sleep(200);
		od_printf("²");
		od_sleep(200);
		od_printf("÷");
		od_sleep(200);
		od_printf("Û\n\n\r");
		od_sleep(200);
		killsmurf(hostage[cyc]);
		hostage[cyc] = 255;
		holder[cyc] = 255;
		writehostage();
		od_set_colour(D_CYAN, D_BLACK);
		od_printf("You lose control, brutally beating %s.\n\r", dataref[smurfsex[hostage[cyc]]]);
		sleep(1);
		od_printf("A grin of satisfaction crosses your face as blue blood slowly covers\n\r");
		od_printf("the cubicle...\n\n\r");
		sleep(1);
		od_set_colour(L_RED, D_BLACK);
		sprintf(tempenform, "Hostage: %s *KILLED* %s!!!\n\r", smurfname[thisuserno], smurfname[hostage[cyc]]);
		newshit(tempenform);
		od_printf("Fatality...\n\n\r");
		sleep(2);
		od_set_colour(D_RED, D_BLACK);
		deductmorale();
		od_printf("Experience Points Lost!\n\r");
		od_sleep(100);
		if (smurflevel[thisuserno] > 0)
		    smurfexper[thisuserno] = defexper[smurflevel[thisuserno] - 1];
		else
		    smurfexper[thisuserno] = 0;
		od_printf("Hit Points Lost!\n\r");
		od_sleep(100);
		smurfhpm[thisuserno] *= .8;
		od_printf("Loss Tally Increased!\n\n\r");
		od_sleep(100);
		smurflose[thisuserno] += (xp_random(5) + 5);
	    } else
		od_printf("\n\n\rAs you walk away you say 'Damn That Felt GOOD!'\n\n\r");
	} else
	    od_printf("Nope\n\n\r");
	smurf_pause();
    }
}














void 
phostage(void)
{
    char            logname[13];
    unsigned int    escape = 0, thishostno = 255, freecost = 10000;
    for (cyc = 0; cyc < hcount; cyc++)
	if (hostage[cyc] == thisuserno)
	    thishostno = cyc;
    smurfmoney[thisuserno] = smurfmoney[thisuserno] + smurfbankd[thisuserno];
    smurfbankd[thisuserno] = 0;
    if (smurflevel[thisuserno] > 0)
	freecost = 1000;
    if (smurflevel[thisuserno] > 5)
	freecost = 20000;
    if (smurflevel[thisuserno] > 10)
	freecost = 50000;
    if (smurflevel[thisuserno] > 20)
	freecost = 100000;
    do {
	od_clr_scr();
	od_set_colour(L_RED, D_BLACK);
	od_printf("You are the hostage of %s!\n\n\r", smurfname[holder[thishostno]]);
	od_set_colour(L_BLUE, D_BLACK);
	od_printf("The chance of escape is %i percent.\n\r", 100 - (smurfconfno[holder[thishostno]] + 1) * 8);
	od_printf("Turns Left: %i\n\r", smurfturns[thisuserno]);
	od_printf("It'll cost you %i gold to buy your freedom.\n\r", freecost);
	od_printf("Gold on you is %.0f.\n\r", smurfmoney[thisuserno]);
	od_printf("He'll give you %i gold for working.\n\r", smurflevel[thisuserno] * 100);
	od_printf("You can trade your weapons and armor for gold.\n\r", smurfmoney[thisuserno]);
	od_set_colour(D_CYAN, D_BLACK);
	od_printf("\n\r(E)scape (T)rade (P)ayup (W)ork (S)UICIDE!!!!!\n\r");
	if (smurffights[thisuserno] < 1) {
	    od_set_colour(L_RED, D_BLACK);
	    od_printf("You rot in your cubicle while waiting your next turn...\n\n\r");
	    lastquote();
	    od_exit(10, FALSE);
	}
	od_set_colour(L_CYAN, D_BLACK);
	od_printf("\n\rHostage [etpwqs?]: ");
	od_clear_keybuffer();
	bbsinkey = od_get_key(TRUE);
	switch (bbsinkey) {
	    case 'T':
	    case 't':
		buyitem();
		break;
	    case 'E':
	    case 'e':
	    case 'Q':
	    case 'q':
		escape = 1;
		break;
	    case 'W':
	    case 'w':
		smurfmoney[thisuserno] = smurfmoney[thisuserno] + smurflevel[thisuserno] * 100;
		od_printf("\n\n\r");
		bbsexit = 1;
		break;
	    case 'P':
	    case 'p':
		if (smurfmoney[thisuserno] > freecost) {
		    sprintf(tempenform, "%s payed %i for freedom!\n\r", smurfname[thisuserno], freecost);
		    logshit(holder[thishostno], tempenform);
		    smurfmoney[thisuserno] = smurfmoney[thisuserno] - freecost;
		    smurfmoney[holder[thishostno]] = smurfmoney[holder[thishostno]] + freecost;
		    freecost = 0;
		    od_printf("\n\n\rHe excepts the money...\n\n\r");
		} else {
		    od_printf("\n\n\rNot enuff gold!\n\r");
		    smurf_pause();
		} break;
	    case 'S':
	    case 's':
		killsmurf(thisuserno);
		od_clr_scr();
		od_printf("Death becomes you.\n\r");
		smurf_pause();
		savegame();
		od_exit(0, FALSE);
		break;
	    case '?':
	    case '*':
		od_set_colour(L_RED, D_BLACK);
		od_printf("\n\n\r(E)scape (T)rade (P)ayup (W)ork (S)UICIDE!!!!!\n\n\r");
		smurf_pause();
		break;
	}
    } while (freecost > 0 && bbsexit != 1 && escape != 1);
    smurfbankd[thisuserno] = smurfbankd[thisuserno] + smurfmoney[thisuserno];
    smurfmoney[thisuserno] = 0;
    if (escape == 1) {
	od_clr_scr();
	od_set_colour(L_BLUE, D_BLACK);
	od_printf("You await nightfall . . .");
	sleep(2);
	od_printf("\n\n\rYou take advantage of your dimly lit surroundings to try to escape!");
	sleep(2);
	od_set_colour(L_RED, D_BLACK);
	if (xp_random(101) > (smurfconfno[holder[thishostno]] + 1) * 8) {
	    od_printf("\n\n\rYou escape successfully!\n\n\r");
	    escape = 3;
	    od_set_colour(L_YELLOW, D_BLACK);
	    od_printf("You can play if you have more turns next game ...\n\n\r");
	} else {
	    sprintf(tempenform, "%s ATTEMPTED ESCAPE!\n\r", smurfname[thisuserno]);
	    logshit(holder[thishostno], tempenform);
	    od_printf("\n\n\rYou hear a hollow crash and feel a shocking pain on your head!");
	    sleep(1);
	    od_set_colour(L_YELLOW, D_BLACK);
	    od_printf("\n\n\rYou were unsuccessful!\n\n\r");
	    sleep(1);
	    od_set_colour(D_CYAN, D_BLACK);
	    od_printf("You must try again or pay up ...\n\n\r");
	}
    }
    if (escape == 3) {
	sprintf(tempenform, "%s *** ESCAPED ***\n\r", smurfname[thisuserno]);
	logshit(holder[thishostno], tempenform);
	sprintf(tempenform, "%s *ESCAPED* from %s\n\r", smurfname[thisuserno], smurfname[holder[thishostno]]);
	newshit(tempenform);
	hostage[thishostno] = 255;
	holder[thishostno] = 255;
	od_set_colour(D_CYAN, D_BLACK);
	smurfhost[holder[thishostno]][0] = 255;
    }
    if (freecost == 0) {
	hostage[thishostno] = 255;
	holder[thishostno] = 255;
	od_set_colour(D_CYAN, D_BLACK);
	smurfhost[holder[thishostno]][0] = 255;
    }
    writehostage();
    savegame();
    lastquote();
    od_exit(10, FALSE);
}

void 
newshit(char shitlog[80])
{
    stream = fopen("smurf.new", "a+");
    fprintf(stream, "%s", shitlog);
    fclose(stream);
}



void 
logshit(int numba, char shitlog[80])
{
    char            senform[15];
    sprintf(senform, "smurf.%03i", numba);
    stream = fopen(senform, "a+");
    fprintf(stream, "%s", shitlog);
    fclose(stream);
}

void 
what(void)
{
    od_printf("\n\rWhat did he say!?!?\n\n\r");
}

void 
counthostages(void)
{
    for (cyc2 = 0; cyc2 < noplayers; cyc2++) {
	numhost[cyc2] = 0;
	for (cyc = 0; cyc < hcount; cyc++)
	    if (holder[cyc] == thisuserno) {
		numhost[cyc2]++;
	    }
    }
}

void 
writehostage(void)
{
    stream = fopen("smurf.hst", "w+");
    for (cyc = 0; cyc < hcount; cyc++) {
	fprintf(stream, "%03i%03i", hostage[cyc], holder[cyc]);
    } fclose(stream);
}
