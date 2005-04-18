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
/* SYSOP Maintenance Code: !-S-Y-S-O-P-!		        ETCETERA   */
/* REMOTE Maintenance Code: !-SIX-NINE-|                          MODULE   */
/* */
/***************************************************************************/

#include "smurfext.h"
#include "smurfsav.h"




void 
automate(void)
{
    od_clr_scr();
    od_set_colour(L_RED, D_BLACK);
    od_printf("Computer Automate Player Character\n\n\r");
    od_set_colour(L_BLUE, D_BLACK);
    od_printf("This function will change the real user number to 0 which will\n\r");
    od_printf("tell the Smurf  Combat daily maintenance program  to play this\n\r");
    od_printf("character,  like grandpa smurf, papa smurf, shadow smurf,  and\n\r");
    od_printf("rebel smurf.\n\n\r");
    od_set_colour(L_MAGENTA, D_BLACK);
    od_printf("Automate %s [yN]: ", smurfname[thisuserno]);
    bbsinkey = od_get_key(TRUE);
    if (bbsinkey == 'Y' || bbsinkey == 'y')
	od_printf("Yea\n\rAre You Sure [yN]: ");
    else {
	od_printf("Nope\n\n\r");
	return;
    } bbsinkey = od_get_key(TRUE);
    if (bbsinkey == 'Y' || bbsinkey == 'y') {
	realnumb[thisuserno] = 0;
	od_printf("Yea\n\n\rAutomated...\n\n\r");
    } else
	od_printf("Nope\n\n\rNot Automated...\n\n\r");
}









void 
givemoney(void)
{
    int             ref[256], conflicted, listno;
    char            bbsin4[5], bbsin5[10];
    od_clr_scr();
    rankuserlist(3);
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

    for (cyc = 0; cyc < noplayers; cyc++) {
	if (smurfexper[cyc] == 0) {
	    listno++;
	    ref[listno] = cyc;
	}
    }

    od_set_colour(L_CYAN, D_BLACK);
    od_printf("You have %.0f gold on hand and %.0f in your hiding place.\n\n\r", smurfmoney[thisuserno], smurfbankd[thisuserno]);
    od_set_colour(L_YELLOW, D_BLACK);
    od_printf("Give gold to who: ");
    od_input_str(bbsin4, 4, '0', '9');
    od_set_colour(L_RED, D_BLACK);
    if ((atoi(bbsin4)) == 0) {
	nl();
	nl();
	od_printf("Aborted\n\n\r");
	return;
    }
    if ((atoi(bbsin4)) > noplayers) {
	nl();
	nl();
	od_printf("No Such Player!\n\n\r");
	return;
    }
    od_set_colour(L_YELLOW, D_BLACK);
    od_printf("Amount: ");
    od_input_str(bbsin5, 9, '0', '9');
    od_set_colour(L_RED, D_BLACK);
    if ((atof(bbsin5)) == 0) {
	nl();
	nl();
	od_printf("Aborted\n\n\r");
	return;
    }
    if ((atof(bbsin5)) > smurfmoney[thisuserno]) {
	nl();
	nl();
	od_printf("You don't have that much on hand!\n\n\r");
	return;
    }
    smurfmoney[thisuserno] -= (atof(bbsin5));
    smurfmoney[ref[(atoi(bbsin4)) - 1]] += (atof(bbsin5));
    savegame();
    nl();
    nl();
}





void            buyitem(void){
    char            bbsin[10];
    int             numba;
    float           pp = 0;
    pp = (smurfint[thisuserno] * .05);
    if (pp > 1)
	pp = 1;
    /* stream = fopen("SMURF.LOG", "a+"); */
    /* fprintf(stream, "BUYIT(%i):",od_control.user_num);fprintf(stream, "%s
     * ... ",smurfname[thisuserno]); */
    /* fclose(stream); */
    itemlist();
    od_set_attrib(SD_YELLOW);
    od_printf("(B)uy (S)ell (C)onfines: ");
    od_clear_keybuffer();
    bbsinkey = od_get_key(TRUE);
    od_set_colour(D_CYAN, D_BLACK);
    switch (bbsinkey) {
	case 'S':
	case 's':
	    od_clr_scr();
	    od_set_colour(L_YELLOW, D_BLACK);
	    od_printf("Which One? (W)eapon (A)rmor :");
	    bbsin[0] = od_get_key(TRUE);
	    od_set_colour(L_RED, D_BLACK);
	    if (bbsin[0] == 'W' || bbsin[0] == 'w') {
		pp = (pp * defprices[smurfweapno[thisuserno]]);
		od_printf("\n\rI'll give you %.0f for your %s, deal? ", pp, smurfweap[thisuserno]);
		bbsin[0] = od_get_key(TRUE);
		od_set_colour(L_CYAN, D_BLACK);
		if (bbsin[0] == 'Y' || bbsin[0] == 'y') {
		    smurfweapno[thisuserno] = 0;
		    od_printf("Yea\n\n\r");
#ifdef TODO_WRAPPERS
		    strset(smurfweap[thisuserno], 0);
#else
		    memset(smurfweap[thisuserno], 0, sizeof(smurfweap[thisuserno]));
#endif
		    sprintf(smurfweap[thisuserno], "%s", defweapon[0]);
		    smurfmoney[thisuserno] = smurfmoney[thisuserno] + pp;
		    return;
		} else {
		    od_printf("Nope\n\n\r");
		    return;
		}
	    }
	    if (bbsin[0] == 'A' || bbsin[0] == 'a') {
		pp = (pp * defprices[smurfarmrno[thisuserno]]);
		od_printf("\n\rI'll give you %.0f for your %s, deal? ", pp, smurfarmr[thisuserno]);
		bbsin[0] = od_get_key(TRUE);
		od_set_colour(L_CYAN, D_BLACK);
		if (bbsin[0] == 'Y' || bbsin[0] == 'y') {
		    smurfarmrno[thisuserno] = 0;
		    od_printf("Yea\n\n\r");
#ifdef TODO_WRAPPERS
		    strset(smurfarmr[thisuserno], 0);
#else
		    memset(smurfarmr[thisuserno], 0, sizeof(smurfarmr[thisuserno]));
#endif
		    sprintf(smurfarmr[thisuserno], "%s", defarmor[0]);
		    smurfmoney[thisuserno] = smurfmoney[thisuserno] + pp;
		    return;
		} else {
		    od_printf("Nope\n\n\r");
		    return;
		}
	    } od_printf("Neither.\n\n\r");
	    return;
	case 'B':
	case 'b':
	    od_printf("Buy\n\rNumber: ");
	    od_set_colour(L_CYAN, D_BLACK);
	    od_input_str(bbsin, 2, '0', '9');
	    numba = atoi(bbsin);
	    numba--;
	    if (numba < 0 || numba > 20)
		return;
	    if (smurfmoney[thisuserno] < defprices[numba]) {
		od_set_colour(L_RED, D_BLACK);
		od_printf("\n\rNot enuff gold!\n\n\r");
		return;
	    }
	    od_clr_scr();
	    od_set_colour(L_YELLOW, D_BLACK);
	    od_printf("Which One? (W)eapon (A)rmor :");
	    bbsin[0] = od_get_key(TRUE);
	    od_set_colour(L_RED, D_BLACK);
	    if (bbsin[0] == 'W' || bbsin[0] == 'w') {
		od_printf("Weapon\n\n\r");
		od_set_colour(L_MAGENTA, D_BLACK);
		od_printf("Number: %i\n\rWeapon: %s\n\rPrice:  %.0f gold\n\rDamage: %i Points\n\n\r", numba + 1, defweapon[numba], defprices[numba], (numba + 1) * 5);
		od_set_colour(D_CYAN, D_BLACK);
		od_printf("Buy it? ");
		bbsin[0] = od_get_key(TRUE);
		od_set_colour(L_CYAN, D_BLACK);
		if (bbsin[0] == 'Y' || bbsin[0] == 'y') {
		    od_printf("Yea\n\n\r");
		    smurfmoney[thisuserno] = smurfmoney[thisuserno] - defprices[numba];
		    smurfweapno[thisuserno] = numba;
#ifdef TODO_WRAPPERS
		    strset(smurfweap[thisuserno], 0);
#else
		    memset(smurfweap[thisuserno], 0, sizeof(smurfweap[thisuserno]));
#endif
		    sprintf(smurfweap[thisuserno], "%s\0", defweapon[numba]);
		    return;
		} od_printf("Nope\n\n\r");
		return;
	    }
	    if (bbsin[0] == 'A' || bbsin[0] == 'a') {
		od_printf("Armor\n\n\r");
		od_set_colour(L_MAGENTA, D_BLACK);
		od_printf("Number: %i\n\rArmor:  %s\n\rPrice:  %.0f gold\n\rRating: %i Points\n\n\r", numba + 1, defarmor[numba], defprices[numba], (numba + 1) * 4);
		od_set_colour(D_CYAN, D_BLACK);
		od_printf("Buy it? ");
		bbsin[0] = od_get_key(TRUE);
		od_set_colour(L_CYAN, D_BLACK);
		if (bbsin[0] == 'Y' || bbsin[0] == 'y') {
		    od_printf("Yea\n\n\r");
		    smurfmoney[thisuserno] = smurfmoney[thisuserno] - defprices[numba];
		    smurfarmrno[thisuserno] = numba;
#ifdef TODO_WRAPPERS
		    strset(smurfarmr[thisuserno], 0);
#else
		    memset(smurfarmr[thisuserno], 0, sizeof(smurfarmr[thisuserno]));
#endif
		    sprintf(smurfarmr[thisuserno], "%s\0", defarmor[numba]);
		    return;
		} od_printf("Nope\n\n\r");
		return;
	    }
	    od_printf("Neither.");
	    return;
	case 'P':
	case 'p':
	    od_printf("Power Up!!\n\rNumber: ");
	    od_set_colour(L_CYAN, D_BLACK);
	    od_input_str(bbsin, 2, '0', '9');
	    numba = atoi(bbsin);
	    numba--;
	    if (numba < 0 || numba > 20)
		return;
	    if (smurfmoney[thisuserno] < defprices[numba]) {
		od_set_colour(L_RED, D_BLACK);
		od_printf("\n\rNot enuff gold!\n\n\r");
		return;
	    }
	    od_clr_scr();
	    od_set_colour(L_YELLOW, D_BLACK);
	    od_printf("Which One? (W)eapon (A)rmor :");
	    bbsin[0] = od_get_key(TRUE);
	    od_set_colour(L_RED, D_BLACK);
	    if (bbsin[0] == 'W' || bbsin[0] == 'w') {
		od_printf("Weapon\n\n\r");
		od_set_colour(L_MAGENTA, D_BLACK);
		od_printf("Number: %i\n\rWeapon: %s\n\rPrice:  %.0f gold\n\rDamage: %i Points\n\n\r", numba + 1, defweapon[smurfweapno[thisuserno]], defprices[numba], (numba + 1) * 5);
		od_set_colour(D_CYAN, D_BLACK);
		od_printf("Power it up? ");
		bbsin[0] = od_get_key(TRUE);
		od_set_colour(L_CYAN, D_BLACK);
		if (bbsin[0] == 'Y' || bbsin[0] == 'y') {
		    od_printf("Yea\n\n\r");
		    smurfmoney[thisuserno] = smurfmoney[thisuserno] - defprices[numba];
		    smurfweapno[thisuserno] = numba;
		    return;
		} od_printf("Nope\n\n\r");
		return;
	    }
	    if (bbsin[0] == 'A' || bbsin[0] == 'a') {
		od_printf("Armor\n\n\r");
		od_set_colour(L_MAGENTA, D_BLACK);
		od_printf("Number: %i\n\rArmor:  %s\n\rPrice:  %.0f gold\n\rRating: %i Points\n\n\r", numba + 1, defarmor[smurfweapno[thisuserno]], defprices[numba], (numba + 1) * 4);
		od_set_colour(D_CYAN, D_BLACK);
		od_printf("Power it up? ");
		bbsin[0] = od_get_key(TRUE);
		od_set_colour(L_CYAN, D_BLACK);
		if (bbsin[0] == 'Y' || bbsin[0] == 'y') {
		    od_printf("Yea\n\n\r");
		    smurfmoney[thisuserno] = smurfmoney[thisuserno] - defprices[numba];
		    smurfarmrno[thisuserno] = numba;
		    return;
		} od_printf("Nope\n\n\r");
		return;
	    }
	    od_printf("Neither.");
	    return;
	case 'C':
	case 'c':
	    buyconf();
	    break;
    } if (showexit == 0) {
	showexit = 1;
	od_printf("Exit\n\n\r");
    }
}

void 
tolocal(void)
{
    char            inputline[80];
    od_clr_scr();
    od_set_colour(L_BLUE, D_BLACK);
    od_printf("This function lets you say something to anyone who is watching for\n\rerror testing, or if you need help.\n\n\r");
    od_set_colour(L_YELLOW, D_BLACK);
    od_printf("(: ");
    od_set_attrib(SD_GREEN);
    od_input_str(inputline, 76, 32, 167);
}



void 
buyconf(void)
{
    char            bbsin[10];
    int             numba;
    /* stream = fopen("SMURF.LOG", "a+"); */
    /* fprintf(stream, "BUYCF(%i):",od_control.user_num);fprintf(stream, "%s
     * ... ",smurfname[thisuserno]); */
    /* fclose(stream); */
    conflist();
    od_set_attrib(SD_YELLOW);
    od_printf("(U)pgrade (A)rmaments: ");
    od_clear_keybuffer();
    bbsinkey = od_get_key(TRUE);
    od_set_colour(D_CYAN, D_BLACK);
    switch (bbsinkey) {
	case 'A':
	case 'a':
	    buyitem();
	    break;
	case 'U':
	case 'u':
	    od_printf("Upgrade\n\rNumber: ");
	    od_set_colour(L_CYAN, D_BLACK);
	    od_input_str(bbsin, 2, '0', '9');
	    numba = atoi(bbsin);
	    numba--;
	    if (numba < 0 || numba > 20)
		return;
	    if (smurfmoney[thisuserno] < defcprices[numba]) {
		od_set_colour(L_RED, D_BLACK);
		od_printf("\n\rNot enuff gold!\n\n\r");
		return;
	    }
	    od_clr_scr();
	    od_set_colour(L_MAGENTA, D_BLACK);
	    od_printf("Type:    #%i\n\rConfine: %s\n\rPrice:   %.0f gold\n\rEscape:  %i%\n\n\r", numba + 1, defconfine[numba], defcprices[numba], 100 - (numba + 1) * 8);
	    od_set_colour(D_CYAN, D_BLACK);
	    od_printf("Buy it? ");
	    bbsin[0] = od_get_key(TRUE);
	    od_set_colour(L_CYAN, D_BLACK);
	    if (bbsin[0] == 'Y' || bbsin[0] == 'y') {
		od_printf("Yea\n\n\r");
		smurfmoney[thisuserno] = smurfmoney[thisuserno] - defcprices[numba];
		smurfconfno[thisuserno] = numba;
#ifdef TODO_WRAPPERS
		strset(smurfconf[thisuserno], 0);
#else
		memset(smurfconf[thisuserno], 0, sizeof(smurfconf[thisuserno]));
#endif
		sprintf(smurfconf[thisuserno], "%s", defconfine[numba]);
		return;
	    } break;
    } if (showexit == 0) {
	showexit = 1;
	od_printf("Exit\n\n\r");
    }
}












void 
itemlist(void)
{
    int             max;	       /* stream = fopen("SMURF.LOG", "a+"); */
    /* fprintf(stream, "ILIST(%i):",od_control.user_num);fprintf(stream, "%s
     * ... ",smurfname[thisuserno]); */
    /* fclose(stream); */
    od_clr_scr();
    if (displayreal == 1)
	max = 20;
    else
	max = 18;
    if (smurflevel[thisuserno] > 20)
	max = 20;
    od_set_attrib(SD_RED);
    od_disp_str(" No.   Weapon               Armor                     Price  \n\r");
    od_set_attrib(SD_YELLOW);
    od_disp_str("------------------------------------------------------------\n\r");
    for (cyc = 0; cyc < max; cyc++) {
	if (defprices[cyc] < smurfmoney[thisuserno] + smurfbankd[thisuserno])
	    od_set_attrib(SD_GREEN);
	else
	    od_set_colour(D_GREEN, D_BLACK);
	od_printf(" %2i :  ", cyc + 1);
	od_set_colour(D_GREY, D_BLACK);
	if (smurfweapno[thisuserno] == cyc)
	    od_set_colour(L_CYAN, D_BLACK);
	od_printf("%-20s ", defweapon[cyc]);
	od_set_colour(D_GREY, D_BLACK);
	if (smurfarmrno[thisuserno] == cyc)
	    od_set_colour(L_CYAN, D_BLACK);
	od_printf("%-20s ", defarmor[cyc]);
	od_set_colour(D_GREY, D_BLACK);
	od_printf("%10.0f\n\r", defprices[cyc]);
    } nl();
}

void 
conflist(void)
{
    int             max;	       /* stream = fopen("SMURF.LOG", "a+"); */
    /* fprintf(stream, "CLIST(%i):",od_control.user_num);fprintf(stream, "%s
     * ... ",smurfname[thisuserno]); */
    /* fclose(stream); */
    od_clr_scr();
    if (displayreal == 1)
	max = 11;
    else
	max = 10;
    if (smurflevel[thisuserno] > 30)
	max = 11;
    od_set_attrib(SD_RED);
    od_disp_str(" No.   Confine Name                        Price  \n\r");
    od_set_attrib(SD_YELLOW);
    od_disp_str("-------------------------------------------------\n\r");
    for (cyc = 0; cyc < max; cyc++) {
	if (defcprices[cyc] < smurfmoney[thisuserno] + smurfbankd[thisuserno])
	    od_set_attrib(SD_GREEN);
	else
	    od_set_colour(D_GREEN, D_BLACK);	/* od_set_attrib(SD_GREEN); */
	od_printf(" %2i :  ", cyc + 1);
	od_set_colour(D_GREY, D_BLACK);
	if (smurfconfno[thisuserno] == cyc)
	    od_set_colour(L_CYAN, D_BLACK);
	od_printf("%-30s %10.0f\n\r", defconfine[cyc], defcprices[cyc]);
    }
    nl();
}






void 
smurf_pause(void)
{
    od_clear_keybuffer();
    od_set_attrib(SD_CYAN);
    od_disp_str("\r(-*-)");
    od_get_key(TRUE);
    od_clr_line();
    od_disp_str("\r      ");
    nl();
}

void 
nl(void)
{
    od_printf("\n\r");
}

void 
lastquote(void)
{
    if (!statis) {
	od_set_colour(L_CYAN, D_BLACK);
	od_printf("Laurence: Hey, before you go, I was just wondering how come your\n\r");
	od_printf("          SysOp didn't register this program, I mean it's FREE!!!\n\n\r");
	 /* sleep(2); */ od_set_colour(L_WHITE, D_BLUE);
	od_printf("REGISTER ME!");
	od_set_colour(7, 0);
	od_printf(" \n\n\r");	       /* sleep(3); */
    }
}

void 
asksex(void)
{
    od_set_colour(L_BLUE, D_BLACK);
    od_printf("\n\n\rMale or Female? [Mf]: ");
    bbsinkey = od_get_key(TRUE);
    od_set_colour(L_RED, D_BLACK);
    if (bbsinkey == 'F' || bbsinkey == 'f') {
	od_printf("Femme\n\r");
	smurfsex[thisuserno] = 002;
	smurf_pause();
	return;
    }
    od_set_colour(D_CYAN, D_BLACK);
    od_printf("Male\n\r");
    smurfsex[thisuserno] = 001;
    smurf_pause();
}










void 
rankuserlist(int ink)
{
    int             ref[100], conflicted, listno;
    listno = noplayers;
    if (ink == 1)
	listno = 10;
    if (ink == 2)
	listno = 20;
    if (listno > noplayers)
	listno = noplayers;
    od_disp_str("\n\n\rProcessing.....");
    od_sleep(100);
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
    od_disp_str(" No.   Smurf Name                  Level Wins:Loss Conf Smurfette Morale\n\r");
    od_set_attrib(SD_YELLOW);
    od_disp_str("-------------------------------------------------------------------------\n\r");
    for (cyc = 0; cyc < listno; cyc++) {
	od_set_attrib(SD_GREEN);
	if (thisuserno == ref[cyc])
	    od_set_colour(L_CYAN, D_BLACK);
	for (cyc3 = 0; cyc3 < hcount; cyc3++)
	    if (hostage[cyc3] == ref[cyc])
		od_set_colour(D_GREEN, D_BLACK);
	od_printf(" %2i :  ", cyc + 1);
	od_set_colour(D_GREY, D_BLACK);
	if (thisuserno == ref[cyc])
	    od_set_colour(L_CYAN, D_BLACK);
	od_printf("%-30s %2i %4i:%4i %4i %4i %3i%% %5i%%\n\r", smurfname[ref[cyc]], smurflevel[ref[cyc]], smurfwin[ref[cyc]], smurflose[ref[cyc]], smurfconfno[ref[cyc]], smurfettelevel[ref[cyc]], __ettemorale[ref[cyc]], 130 - __morale[ref[cyc]] * 10);
    }
    for (cyc = 0; cyc < noplayers; cyc++) {
	if (smurfexper[cyc] == 0)
	    if ((ink == 1 && listno < 10) || (ink == 2 && listno < 20) || ink == 3) {
		listno++;
		od_set_attrib(SD_GREEN);
		if (thisuserno == cyc)
		    od_set_colour(L_CYAN, D_BLACK);
		for (cyc3 = 0; cyc3 < hcount; cyc3++)
		    if (hostage[cyc3] == cyc)
			od_set_colour(D_GREEN, D_BLACK);
		od_printf(" %2i :  ", listno);
		od_set_colour(D_GREY, D_BLACK);
		if (thisuserno == cyc)
		    od_set_colour(L_CYAN, D_BLACK);
		od_printf("%-30s %2i %4i:%4i %4i %4i %3i%% %5i%%\n\r", smurfname[cyc], smurflevel[cyc], smurfwin[cyc], smurflose[cyc], smurfconfno[cyc], smurfettelevel[cyc], __ettemorale[cyc], 130 - __morale[cyc] * 10);
	    }
    } nl();
}
