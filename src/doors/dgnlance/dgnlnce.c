#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "OpenDoor.h"

#include "xp64.h"
#include "xpdev.h"
#include "dgnlnce.h"
#include "combat.h"
#include "fileutils.h"
#include "userfile.h"
#include "utils.h"

struct playertype player[3];
/* In these arrays, 0 isn't actually used */
QWORD           required[29];	       /* Experience required for each level */
struct weapon   weapon[26];
struct armour   armour[26];

const char      dots[] = "...............................................";
char            temp[81];
/*****************************/
/* Functions from common.pas */
/*****************************/

char           *
date(void)
{
    static char     retdate[9];
    time_t          now;
    now = time(NULL);
    strftime(retdate, sizeof(retdate), "%m/%d/%y", localtime(&now));
    return (retdate);
}
/********************************/
/* Functions from original .pas */
/********************************/

void
depobank(void)
{
    QWORD           tempbank;
    tempbank = user.gold;
    user.gold = user.gold - tempbank;
    user.bank = user.bank + tempbank;
    nl();
    od_set_color(L_CYAN, D_BLACK);
    od_printf("%" QWORDFORMAT " Steel pieces are in the bank.\r\n", user.bank);
}

void
withdrawbank(void)
{
    QWORD           tempbank;
    tempbank = user.bank;
    user.gold = user.gold + tempbank;
    user.bank = user.bank - tempbank;
    nl();
    od_set_color(D_MAGENTA, D_BLACK);
    od_printf("You are now carrying %" QWORDFORMAT " Steel pieces.\r\n", user.gold);
}

void
checkday(void)
{
    char            oldy[256];
    DWORD           h, i, j;
    FILE           *outfile;
    BOOL            newday = FALSE;
    FILE           *infile;
    struct playertype **playerlist = NULL;
    struct playertype **ra;
    infile = fopen("data/date.lan", "r+b");
    readline(oldy, sizeof(oldy), infile);

    if (strcmp(oldy, date())) {
	fseek(infile, 0, SEEK_SET);
	fputs(date(), infile);
	fputs("\n", infile);
	newday = TRUE;
	outfile = fopen("data/record.lan", "wb");
	fclose(outfile);
    }
    fclose(infile);

    infile = fopen("data/characte.lan", "rb");
    fgets(temp, sizeof(temp), infile);
    if (feof(infile))
	return;
    i = 0;
    while (loadnextuser(&tmpuser, infile)) {
	ra = (struct playertype **) realloc(playerlist, sizeof(struct playertype *) * (i + 1));
	if (ra == NULL)
	    exit(1);
	playerlist = ra;
	playerlist[i] = (struct playertype *) malloc(sizeof(struct playertype));
	if (playerlist[i] == NULL)
	    exit(1);
	*playerlist[i] = tmpuser;
	if (newday) {
	    playerlist[i]->battles = BATTLES_PER_DAY;
	    playerlist[i]->fights = FIGHTS_PER_DAY;
	    playerlist[i]->flights = FLIGHTS_PER_DAY;
	    playerlist[i]->status &= ~DEAD;
	    playerlist[i]->damage = 0;
	    playerlist[i]->vary = supplant();
	}
	i++;
    }
    fclose(infile);
    for (j = 0; j < (i - 1); j++) {
	for (h = j + 1; h < i; h++) {
	    if (playerlist[h]->experience > playerlist[j]->experience) {
		tmpuser = *playerlist[h];
		*playerlist[h] = *playerlist[j];
		*playerlist[j] = tmpuser;
	    }
	}
    }
    outfile = fopen("data/characte.lan", "wb");
    fprintf(outfile, "%lu\n", i);
    for (j = 0; j < i; j++) {
	writenextuser(playerlist[j], outfile);
	free(playerlist[j]);
    }
    free(playerlist);
    fclose(outfile);
}

void
playerlist(void)
{
    int             i, a;
    FILE           *infile;
    checkday();
    od_clr_scr();
    nl();
    od_set_color(D_MAGENTA, D_BLACK);
    od_disp_str("Hero Rankings:\r\n");
    od_disp_str("!!!!!!!!!!!!!!\r\n");
    infile = fopen("data/characte.lan", "rb");
    fgets(temp, sizeof(temp), infile);
    i = 3;
    a = 1;
    while (loadnextuser(&tmpuser, infile)) {
	nl();
	od_set_color(D_MAGENTA, D_BLACK);
	od_printf("%2u.  `bright cyan`%.30s%.*s`green`Lev=%-2u  W=%-6" QWORDFORMAT " L=%-6" QWORDFORMAT " S=%s `blinking red`%s"
	    ,a, tmpuser.pseudo, 30 - strlen(tmpuser.pseudo), dots, tmpuser.r
		  ,tmpuser.wins, tmpuser.loses
		  ,((tmpuser.status & DEAD) ? "SLAIN" : "ALIVE"), ((tmpuser.status & ONLINE) ? "ONLINE" : ""));
	if (++i >= od_control.user_screen_length)
	    pausescr();
	a++;
    }
    fclose(infile);
}

void
leave(void)
{
    user.status &= ~ONLINE;
    saveuser(&user);
}

void
heal(void)
{
    WORD            opt;
    od_clr_scr();
    od_set_color(D_MAGENTA, D_BLACK);
    od_printf("Clerics want %u Steel pieces per wound.\r\n", 8 * user.r);
    od_set_color(L_YELLOW, D_BLACK);
    od_printf("You have %u points of damage to heal.\r\n", user.damage);
    od_set_color(L_CYAN, D_BLACK);
    od_disp_str("How many do points do you want healed? ");
    od_input_str(temp, 3, '0', '9');
    opt = strtoul(temp, NULL, 10);
    if (!temp[0])
	opt = user.damage;
    if (((opt) * (user.r) * 10) > user.gold) {
	od_set_color(L_RED, B_BLACK);
	od_disp_str("Sorry, you do not have enough Steel.\r\n");
	opt = 0;
    } else if (opt > user.damage)
	opt = user.damage;
    user.damage -= opt;
    user.gold = user.gold - 8 * opt * user.r;
    od_printf("%u hit points healed.\r\n", opt);
}

void
levelupdate(void)
{
    int             x;
    if (user.experience > required[user.r + 1]) {
	user.r++;
	od_set_color(L_YELLOW, D_BLACK);
	od_printf("Welcome to level %u!\r\n", user.r);
	x = xp_random(6) + 1;
	switch (x) {
	    case 1:
		user.strength++;
		break;
	    case 2:
		user.intelligence++;
		break;
	    case 3:
		user.luck++;
		break;
	    case 4:
		user.dexterity++;
		break;
	    case 5:
		user.constitution++;
		break;
	    case 6:
		user.charisma++;
		break;
	}
	user.hps = user.hps + (xp_random(5) + 1) + (user.constitution / 4);
    }
}
BOOL
incre(void)
{
    od_disp_str("Increase which stat? ");
    switch (od_get_answer("123456Q")) {
	case '1':
	    od_disp_str("Strength\r\n");
	    tmpuser.strength++;
	    break;
	case '2':
	    od_disp_str("Intelligence\r\n");
	    tmpuser.intelligence++;
	    break;
	case '3':
	    od_disp_str("Dexterity\r\n");
	    tmpuser.dexterity++;
	    break;
	case '4':
	    od_disp_str("Luck\r\n");
	    tmpuser.luck++;
	    break;
	case '5':
	    od_disp_str("Constitution\r\n");
	    tmpuser.constitution++;
	    break;
	case '6':
	    od_disp_str("Charisma\r\n");
	    tmpuser.charisma++;
	    break;
	case 'Q':
	    od_disp_str("Quit\r\n");
	    return (FALSE);
    }
    return (TRUE);
}
BOOL
decre(void)
{
    od_disp_str("Decrease which stat? ");
    switch (od_get_answer("123456Q")) {
	case '1':
	    od_disp_str("Strength\r\n");
	    tmpuser.strength -= 2;
	    break;
	case '2':
	    od_disp_str("Intelligence\r\n");
	    tmpuser.intelligence -= 2;
	    break;
	case '3':
	    od_disp_str("Dexterity\r\n");
	    tmpuser.dexterity -= 2;
	    break;
	case '4':
	    od_disp_str("Luck\r\n");
	    tmpuser.luck -= 2;
	    break;
	case '5':
	    od_disp_str("Constitution\r\n");
	    tmpuser.constitution -= 2;
	    break;
	case '6':
	    od_disp_str("Charisma\r\n");
	    tmpuser.charisma -= 2;
	    break;
	case 'Q':
	    od_disp_str("Quit\r\n");
	    return (FALSE);
    }
    return (TRUE);
}
BOOL
ministat(void)
{
    BOOL            yaya = TRUE;
    nl();
    od_disp_str("Status Change:\r\n");
    od_disp_str("^^^^^^^^^^^^^^\r\n");
    od_printf("1> Str: %u\r\n", tmpuser.strength);
    od_printf("2> Int: %u\r\n", tmpuser.intelligence);
    od_printf("3> Dex: %u\r\n", tmpuser.dexterity);
    od_printf("4> Luk: %u\r\n", tmpuser.luck);
    od_printf("5> Con: %u\r\n", tmpuser.constitution);
    od_printf("6> Chr: %u\r\n", tmpuser.charisma);
    nl();
    if (tmpuser.strength < 6) {
	yaya = FALSE;
	nl();
	od_disp_str("Strength cannot go below 6\r\n");
    }
    if (tmpuser.intelligence < 6) {
	yaya = FALSE;
	nl();
	od_disp_str("Intelligence cannot go below 6\r\n");
    }
    if (tmpuser.dexterity < 6) {
	yaya = FALSE;
	nl();
	od_disp_str("Dexterity cannot go below 6\r\n");
    }
    if (tmpuser.luck < 6) {
	yaya = FALSE;
	nl();
	od_disp_str("Luck cannot go below 6\r\n");
    }
    if (tmpuser.constitution < 6) {
	yaya = FALSE;
	nl();
	od_disp_str("Constitution cannot go below 6\r\n");
    }
    if (tmpuser.charisma < 6) {
	yaya = FALSE;
	nl();
	od_disp_str("Charisma cannot go below 6\r\n");
    }
    if (yaya) {
	od_disp_str("Is this correct? ");
	if (od_get_answer("YN") == 'Y') {
	    od_disp_str("Yes\r\n");
	    user = tmpuser;
	    return (TRUE);
	} else {
	    od_disp_str("No\r\n");
	    return (FALSE);
	}
    } else
	return (FALSE);
}

void
chstats(void)
{
    od_clr_scr();

    for (;;) {
	tmpuser = user;
	od_disp_str("Status Change:\r\n");
	od_disp_str("@@@@@@@@@@@@@\r\n");
	od_disp_str("You may increase any stat by one,\r\n");
	od_disp_str("yet you must decrease another by two.\r\n");
	nl();
	od_printf("1> Str: %u\r\n", tmpuser.strength);
	od_printf("2> Int: %u\r\n", tmpuser.intelligence);
	od_printf("3> Dex: %u\r\n", tmpuser.dexterity);
	od_printf("4> Luk: %u\r\n", tmpuser.luck);
	od_printf("5> Con: %u\r\n", tmpuser.constitution);
	od_printf("6> Chr: %u\r\n", tmpuser.charisma);
	nl();
	if (incre()) {
	    if (decre()) {
		if (ministat())
		    break;
	    }
	} else
	    break;
    }
}

void
credits(void)
{
    od_clr_scr();
    od_set_color(D_GREEN, D_BLACK);
    od_disp_str("Dragonlance 3.0 Credits\r\n");
    od_disp_str("@@@@@@@@@@@@@@@@@@@@@@@\r\n");
    od_disp_str("Original Dragonlance   :  Raistlin Majere & TML   \r\n");
    od_disp_str("Special Thanks To      : The Authors Of Dragonlance\r\n");
    od_disp_str("Dragonlance's Home     : The Tower Of High Sorcery\r\n");
    od_disp_str("Originally modified from the Brazil Source Code.\r\n");
    od_disp_str("C Port                 : Deuce\r\n");
    od_disp_str("Home Page              : http://doors.synchro.net/\r\n");
    od_disp_str("Support                : deuce@nix.synchro.net\r\n");
    nl();
    pausescr();
}

void
docs()
{
    od_clr_scr();
    od_send_file("text/docs");
    pausescr();
}

void
vic(void)
{
    BOOL            ahuh;
    for (ahuh = FALSE; !ahuh;) {
	nl();
	od_set_color(L_CYAN, D_BLACK);
	od_disp_str("Enter your new Battle Cry.\r\n");
	od_disp_str("> ");
	od_input_str(user.bcry, 60, ' ', '~');
	od_disp_str("Is this correct? ");
	if (od_get_answer("YN") == 'Y') {
	    ahuh = TRUE;
	    od_disp_str("Yes\r\n");
	} else {
	    ahuh = FALSE;
	    od_disp_str("No\r\n");
	}
    }

    for (ahuh = FALSE; !ahuh;) {
	nl();
	od_set_color(L_CYAN, D_BLACK);
	od_disp_str("What will you say when you're mortally wounded?.\r\n");
	od_disp_str("> ");
	od_input_str(user.gaspd, 60, ' ', '~');
	od_disp_str("Is this correct? ");
	if (od_get_answer("YN") == 'Y') {
	    ahuh = TRUE;
	    od_disp_str("Yes\r\n");
	} else {
	    ahuh = FALSE;
	    od_disp_str("No\r\n");
	}
    }

    for (ahuh = FALSE; !ahuh;) {
	nl();
	od_set_color(L_CYAN, D_BLACK);
	od_disp_str("What will you say when you win?\r\n");
	od_disp_str("> ");
	od_input_str(user.winmsg, 60, ' ', '~');
	od_disp_str("Is this correct? ");
	if (od_get_answer("YN") == 'Y') {
	    ahuh = TRUE;
	    od_disp_str("Yes\r\n");
	} else {
	    ahuh = FALSE;
	    od_disp_str("No\r\n");
	}
    }
}

void
create(BOOL isnew)
{
    user.vary = supplant();
    user.strength = 12;
    user.status = ONLINE;
    user.intelligence = 12;
    user.luck = 12;
    user.damage = 0;
    user.dexterity = 12;
    user.constitution = 12;
    user.charisma = 12;
    user.gold = xp_random(100) + 175;
    user.weapon = 1;
    user.armour = 1;
    user.experience = 0;
    user.plus = 0;
    user.bank = (xp_random(199) + 1);
    user.r = 1;
    user.hps = (xp_random(4) + 1) + user.constitution;
    user.wins = 0;
    user.loses = 0;
    user.sex = 'M';
    if (isnew) {
	user.bcry[0] = 0;
	user.gaspd[0] = 0;
	user.winmsg[0] = 0;
	nl();
	SAFECOPY(user.name, od_control.user_name);
	SAFECOPY(user.pseudo, od_control.user_name);
	vic();
	user.battles = BATTLES_PER_DAY;
	user.fights = FIGHTS_PER_DAY;
	user.flights = FLIGHTS_PER_DAY;
    }
    od_printf("What sex would you like your character to be? (M/F) ");
    user.sex = od_get_answer("MF");
    nl();
}

void
weaponlist(void)
{
    int             i = 1;
    nl();
    nl();
    od_disp_str("  Num. Weapon                      Armour                          Price   \r\n");
    od_disp_str("(-------------------------------------------------------------------------)\r\n");
    od_set_color(L_YELLOW, D_BLACK);
    for (i = 1; i <= 25; i++)
	od_printf("  %2d>  %-25.25s   %-25.25s   %9u\r\n", i, weapon[i].name, armour[i].name, weapon[i].cost);
}

void
weaponshop(void)
{
    WORD            buy;
    DWORD           buyprice;
    od_clr_scr();
    od_set_color(L_YELLOW, D_BLACK);
    od_disp_str("Weapon & Armour Shop\r\n");
    od_set_color(D_GREEN, D_BLACK);
    od_disp_str("$$$$$$$$$$$$$$$$$$$$\r\n");
    while (1) {
	nl();
	od_disp_str("(B)rowse, (S)ell, (P)urchase, or (Q)uit? ");
	switch (od_get_answer("BSQP")) {
	    case 'Q':
		od_disp_str("Quit\r\n");
		return;
	    case 'B':
		od_disp_str("Browse\r\n");
		weaponlist();
		break;
	    case 'P':
		od_disp_str("Purchase\r\n");
		nl();
		nl();
		od_disp_str("Enter weapon/armour # you wish buy: ");
		od_input_str(temp, 2, '0', '9');
		buy = strtoul(temp, NULL, 10);
		if (buy == 0)
		    return;
		if (weapon[buy].cost > user.gold)
		    od_disp_str("You do not have enough Steel.\r\n");
		else {
		    nl();
		    od_disp_str("(W)eapon or (A)rmour: ");
		    switch (od_get_answer("WA")) {
			case 'W':
			    od_disp_str("Weapon\r\n");
			    od_disp_str("Are you sure you want buy it? ");
			    if (od_get_answer("YN") == 'Y') {
				od_disp_str("Yes\r\n");
				user.gold -= weapon[buy].cost;
				user.weapon = buy;
				nl();
				od_set_color(D_MAGENTA, D_BLACK);
				od_printf("You've bought a %s\r\n", weapon[buy].name);
				user.attack = weapon[user.weapon].attack;
				user.power = weapon[user.weapon].power;
			    } else
				od_disp_str("No\r\n");
			    break;
			case 'A':
			    od_disp_str("Armour\r\n");
			    od_disp_str("Are you sure you want buy it? ");
			    if (od_get_answer("YN") == 'Y') {
				od_disp_str("Yes\r\n");
				user.gold -= armour[buy].cost;
				user.armour = buy;
				nl();
				od_set_color(D_MAGENTA, D_BLACK);
				od_printf("You've bought a %s\r\n", armour[buy].name);
			    } else
				od_disp_str("No\r\n");
			    break;
		    }
		}
		break;
	    case 'S':
		od_disp_str("Sell\r\n");
		nl();
		od_disp_str("(W)eapon,(A)rmour,(Q)uit : ");
		switch (od_get_answer("AWQ")) {
		    case 'Q':
			od_disp_str("Quit\r\n");
			return;
		    case 'W':
			od_disp_str("Weapon\r\n");
			buyprice = user.charisma;
			buyprice = buyprice * weapon[user.weapon].cost;
			buyprice = (buyprice / 20);
			nl();
			od_printf("I will purchase it for %u, okay? ", buyprice);
			if (od_get_answer("YN") == 'Y') {
			    od_disp_str("Yes\r\n");
			    od_set_color(D_GREEN, D_BLACK);
			    od_disp_str("Is it Dwarven Made?\r\n");
			    user.weapon = 1;
			    user.gold = user.gold + buyprice;
			} else
			    od_disp_str("No\r\n");
			break;
		    case 'A':
			od_disp_str("Armour\r\n");
			buyprice = user.charisma * armour[user.armour].cost / 20;
			nl();
			od_printf("I will purchase it for %u, okay? ", buyprice);
			if (od_get_answer("YN") == 'Y') {
			    od_disp_str("Yes\r\n");
			    od_disp_str("Fine Craftsmanship!\r\n");
			    user.armour = 1;
			    user.gold = user.gold + buyprice;
			} else
			    od_disp_str("No\r\n");
			break;
		}
	}
    }
}

void
listplayers(void)
{
    od_clr_scr();
    od_set_color(L_YELLOW, D_BLACK);
    od_disp_str("Heroes That Have Been Defeated\r\n");
    od_set_color(D_MAGENTA, D_BLACK);
    od_disp_str("+++++++++++++++++++++++++++++++\r\n");
    od_set_color(L_CYAN, D_BLACK);
    od_send_file("data/record.lan");
}

void
spy(void)
{
    char            aa[31];
    od_clr_scr();
    od_disp_str("Spying on another user eh.. well you may spy, but to keep\r\n");
    od_disp_str("you from copying this person's stats, they will not be   \r\n");
    od_disp_str("available to you.  Note that this is gonna cost you some \r\n");
    od_disp_str("cash too.  Cost: 20 Steel pieces                         \r\n");
    nl();
    od_disp_str("Who do you wish to spy on? ");
    od_input_str(aa, sizeof(aa) - 1, ' ', '~');
    if (loaduser(aa, FALSE, &tmpuser)) {
	if (user.gold < 20) {
	    od_set_color(L_RED, B_BLACK);
	    od_disp_str("You do not have enough Steel!\r\n");
	} else {
	    user.gold -= 20;
	    nl();
	    od_set_color(L_RED, B_BLACK);
	    od_printf("%s\r\n", tmpuser.pseudo);
	    nl();
	    od_printf("Level  : %u\r\n", tmpuser.r);
	    od_printf("Exp    : %" QWORDFORMAT "\r\n", tmpuser.experience);
	    od_printf("Flights: %u\r\n", tmpuser.flights);
	    od_printf("Hps    : %u(%u)\r\n", tmpuser.hps - tmpuser.damage, tmpuser.hps);
	    nl();
	    od_set_color(D_MAGENTA, D_BLACK);
	    od_printf("Weapon : %s\r\n", weapon[tmpuser.weapon].name);
	    od_printf("Armour : %s\r\n", armour[tmpuser.armour].name);
	    nl();
	    od_set_color(L_YELLOW, D_BLACK);
	    od_printf("Steel  (in hand): %" QWORDFORMAT "\r\n", tmpuser.gold);
	    od_printf("Steel  (in bank): %" QWORDFORMAT "\r\n", tmpuser.bank);
	    nl();
	    pausescr();
	}
    }
}

void
gamble(void)
{
    char            tempgd[6];
    INT32           realgold;
    int             okea;
    nl();
    if (user.fights == 0)
	od_disp_str("The Shooting Gallery is closed until tomorrow!\r\n");
    else {
	od_clr_scr();
	od_disp_str("  Welcome to the Shooting Gallery\r\n");
	od_disp_str(" Maximum wager is 25,000 Steel pieces\r\n");
	nl();
	od_set_color(L_YELLOW, D_BLACK);
	od_disp_str("How many Steel pieces do you wish to wager? ");
	od_set_color(D_GREY, D_BLACK);
	od_input_str(tempgd, sizeof(tempgd) - 1, '0', '9');
	realgold = strtoull(tempgd, NULL, 10);
	if (realgold > user.gold) {
	    nl();
	    od_disp_str("You do not have enough Steel!\r\n");
	}
	if ((realgold != 0) && ((user.gold >= realgold) && (realgold <= 25000) && (realgold >= 1))) {
	    okea = xp_random(99) + 1;
	    if (okea <= 3) {
		realgold *= 100;
		user.gold += realgold;
		od_printf("You shot all the targets and win %u Steel pieces!\r\n", realgold);
	    } else if ((okea > 3) && (okea <= 15)) {
		realgold *= 10;
		user.gold += realgold;
		od_printf("You shot 50%% if the targets and win %u Steel pieces!\r\n", realgold);
	    } else if ((okea > 15) && (okea <= 30)) {
		realgold *= 3;
		user.gold += realgold;
		od_printf("You shot 25%% if the targets and win %u Steel pieces!\r\n", realgold);
	    } else {
		user.gold -= realgold;
		od_disp_str("Sorry You Hath Lost!\r\n");
	    }
	}
    }
}

void
bulletin(void)
{
    BOOL            endfil;
    int             countr;
    char            tempcoun[3];
    char            blt[5][61];
    FILE           *messfile;
    od_clr_scr();
    countr = 0;
    endfil = FALSE;
    od_send_file("text/bullet.lan");
    nl();
    od_disp_str("Do you wish to enter a News Bulletin? ");
    if (od_get_answer("YN") == 'Y') {
	od_disp_str("Yes\r\n");
	nl();
	while (!endfil) {
	    countr++;
	    sprintf(tempcoun, "%d", countr);
	    od_set_color(L_YELLOW, D_BLACK);
	    od_disp_str(tempcoun);
	    od_disp_str("> ");
	    od_set_color(D_GREY, D_BLACK);
	    od_input_str(blt[countr], 60, ' ', '~');
	    if (countr == 4)
		endfil = TRUE;
	}
	nl();
	od_disp_str("Is the bulletin correct? ");
	if (od_get_answer("YN") == 'Y') {
	    od_disp_str("Yes\r\n");
	    od_disp_str("Saving Bulletin...\r\n");
	    messfile = fopen("text/bullet.lan", "ab");
	    fputs(blt[1], messfile);
	    fputs("\n", messfile);
	    fputs(blt[2], messfile);
	    fputs("\n", messfile);
	    fputs(blt[3], messfile);
	    fputs("\n", messfile);
	    fputs(blt[4], messfile);
	    fputs("\n", messfile);
	    fclose(messfile);
	} else
	    od_disp_str("No\r\n");
    } else
	od_disp_str("No\r\n");
}

void
training(void)
{
    char            temptrain[2];
    int             realtrain;
    int             tttgld;
    nl();
    if (user.fights == 0)
	od_disp_str("The Training Grounds are closed until tomorrow!\r\n");
    else {
	od_clr_scr();
	od_disp_str("Training Grounds\r\n");
	od_disp_str("%%%%%%%%%%%%%%%%\r\n");
	od_disp_str("Each characteristic you wish to upgrade\r\n");
	od_disp_str("will cost 1,000,000 Steel pieces per point.\r\n");
	nl();
	tttgld = 10000;
	od_disp_str("Do you wish to upgrade a stat? ");
	if (od_get_answer("YN") == 'Y') {
	    od_disp_str("Yes\r\n");
	    if (user.gold < (tttgld * 100))
		od_disp_str("Sorry, but you do not have enough Steel!\r\n");
	    else {
		nl();
		od_disp_str("1> Strength       2> Intelligence\r\n");
		od_disp_str("3> Dexterity      4> Luck        \r\n");
		od_disp_str("5> Constitution   6> Charisma    \r\n");
		nl();
		od_set_color(L_YELLOW, D_BLACK);
		od_disp_str("Which stat do you wish to increase? ");
		od_set_color(D_GREY, D_BLACK);
		temptrain[0] = od_get_answer("123456");
		switch (temptrain[0]) {
		    case 1:
			od_disp_str("Strength\r\n");
			break;
		    case 2:
			od_disp_str("Intelligence\r\n");
			break;
		    case 3:
			od_disp_str("Dexterity\r\n");
			break;
		    case 4:
			od_disp_str("Luck\r\n");
			break;
		    case 5:
			od_disp_str("Constitution\r\n");
			break;
		    case 6:
			od_disp_str("Charisma\r\n");
			break;
		}
		realtrain = temptrain[0] - '0';
		od_disp_str("Are you sure? ");
		if (od_get_answer("YN") == 'Y') {
		    od_disp_str("Yes\r\n");
		    user.gold -= tttgld * 100;
		    switch (realtrain) {
			case 1:
			    user.strength++;
			    break;
			case 2:
			    user.intelligence++;
			    break;
			case 3:
			    user.dexterity++;
			    break;
			case 4:
			    user.luck++;
			    break;
			case 5:
			    user.constitution++;
			    break;
			case 6:
			    user.charisma++;
			    break;
		    }
		} else
		    od_disp_str("No\r\n");
	    }
	} else
	    od_disp_str("No\r\n");
    }
}

void
menuit(void)
{
    od_clr_scr();
    od_send_file("text/menu");
}

int
main(int argc, char **argv)
{
    DWORD           i;
    BOOL            found;
    FILE           *infile;
    od_init();
    checkday();
    atexit(leave);

    found = FALSE;
    nl();
    nl();
    credits();
    od_clr_scr();
    od_printf("`bright yellow`----------`blinking red`   -=-=DRAGONLANCE=-=-      `bright yellow`    /\\     \r\n");
    od_printf("\\        /`white`       Version 3.0          `bright yellow`    ||     \r\n");
    od_printf(" \\      /                                 ||     \r\n");
    od_printf("  \\    /  `bright blue`  Welcome To The World of   `bright yellow`    ||     \r\n");
    od_printf("   |  |   `bright cyan`  Krynn, Where the gods     `bright yellow`    ||     \r\n");
    od_printf("  /    \\  `bright blue`  of good and evil battle.  `bright yellow`  \\ || /   \r\n");
    od_printf(" /      \\ `bright cyan`  Help the People Of Krynn. `bright yellow`   \\==/    \r\n");
    od_printf("/        \\                                ##     \r\n");
    od_printf("----------`blinking red`        ON WARD!!!          `bright yellow`    ##     \r\n");
    nl();
    od_disp_str("News Bulletin:\r\n");
    nl();
    od_send_file("text/bullet.lan");
    if (!loaduser(od_control.user_name, TRUE, &user))
	create(TRUE);
    if (user.status == DEAD) {
	if (!strcmp(date(), user.laston)) {
	    od_printf("You have already died today...\r\n");
	    od_printf("Return tomorrow for your revenge!\r\n");
	    exit(0);
	} else {
	    nl();
	    od_set_color(L_CYAN, D_BLACK);
	    od_printf("A defeat was lead over you by %s.", user.killer);
	}
    }
    SAFECOPY(user.laston, date());
    if (user.flights < 1) {
	user.fights = 0;
	user.battles = 0;
    } else
	user.flights--;
    nl();
    pausescr();
    od_clr_scr();
    infile = fopen("data/weapons.lan", "rb");
    od_set_color(L_BLUE, D_BLACK);
    for (i = 1; i <= 25; i++) {
	readline(weapon[i].name, sizeof(weapon[i].name), infile);
	weapon[i].attack = readnumb(1, infile);
	weapon[i].power = readnumb(1, infile);
	endofline(infile);
    }
    fclose(infile);
    infile = fopen("data/armor.lan", "rb");
    od_set_color(L_BLUE, D_BLACK);
    for (i = 1; i <= 25; i++)
	readline(armour[i].name, sizeof(armour[i].name), infile);
    fclose(infile);
    infile = fopen("data/prices.lan", "rb");
    for (i = 1; i <= 25; i++) {
	weapon[i].cost = readnumb(100000000, infile);
	armour[i].cost = weapon[i].cost;
	endofline(infile);
    }
    fclose(infile);
    user.attack = weapon[user.weapon].attack;
    user.power = weapon[user.weapon].power;
    infile = fopen("data/experience.lan", "rb");
    for (i = 1; i <= 28; i++) {
	required[i] = readnumb(100000000, infile);
	endofline(infile);
    }
    fclose(infile);
    od_set_color(L_YELLOW, D_BLACK);
    user.status = ONLINE;
    statshow();
    saveuser(&user);
    while (user.damage < user.hps) {
	levelupdate();
	if (((user.wins + 1) * 4) < (user.loses)) {
	    nl();
	    od_disp_str("As you were Travelling along a Wilderness Path an   \r\n");
	    od_disp_str("Evil Wizard Confronted You.  When you tried to fight\r\n");
	    od_disp_str("Him off, he cast a Spell of Instant Death Upon you.  \r\n");
	    od_disp_str("Instantly you were slain by the Archmage, Leaving you\r\n");
	    od_disp_str("as nothing more than a pile of ashes.  Re-Rolled!   \r\n");
	    nl();
	    pausescr();
	    create(FALSE);
	    if (user.flights)
		user.flights--;
	}
	nl();
	nl();
	od_set_color(L_YELLOW, D_BLACK);
	od_disp_str("Command (?): ");
	switch (od_get_answer("QVP12345CHWLADGFRSTX+-?EZ*#")) {
	    case 'Q':
		od_disp_str("Quit\r\n");
		od_disp_str("LEAVE KRYNN? Are you sure? ");
		if (od_get_answer("YN") == 'Y') {
		    od_disp_str("Yes\r\n");
		    return (0);
		} else
		    od_disp_str("No\r\n");
		break;
	    case '1':
		od_disp_str("1\r\n");
		afight(1);
		break;
	    case '2':
		od_disp_str("2\r\n");
		afight(2);
		break;
	    case '3':
		od_disp_str("3\r\n");
		afight(3);
		break;
	    case '4':
		od_disp_str("4\r\n");
		afight(4);
		break;
	    case '5':
		od_disp_str("5\r\n");
		afight(5);
		break;
	    case 'C':
		od_disp_str("Change Stats\r\n");
		chstats();
		break;
	    case 'H':
		od_disp_str("Heal\r\n");
		heal();
		break;
	    case 'W':
		od_disp_str("Weapon Shop\r\n");
		weaponshop();
		break;
	    case 'L':
		od_disp_str("Level Update\r\n");
		levelupdate();
		break;
	    case 'A':
		od_disp_str("Battle Another User\r\n");
		doggie();
		break;
	    case 'D':
		od_disp_str("Docs\r\n");
		docs();
		break;
	    case 'G':
		od_disp_str("Gamble\r\n");
		gamble();
		break;
	    case 'F':
		od_disp_str("Battles Today\r\n");
		listplayers();
		break;
	    case 'R':
		od_disp_str("Rank Players\r\n");
		playerlist();
		break;
	    case 'S':
		od_disp_str("Status\r\n");
		statshow();
		break;
	    case 'T':
		od_disp_str("Training Grounds\r\n");
		training();
		break;
	    case 'X':
		od_disp_str("Re-Roll\r\n");
		od_disp_str("Please note that this will completely purge\r\n");
		od_disp_str("your current hero of all atributes!\r\n");
		nl();
		od_disp_str("Are you sure you want to REROLL your character? ");
		if (od_get_answer("YN") == 'Y') {
		    od_disp_str("Yes\r\n");
		    create(FALSE);
		    if (user.flights)
			user.flights--;
		} else
		    od_disp_str("No\r\n");
		break;
	    case '+':
		od_disp_str("Deposit\r\n");
		depobank();
		break;
	    case 'P':
		od_disp_str("Plug\r\n");
		od_send_file("text/plug");
		break;
	    case '-':
		od_disp_str("Withdraw\r\n");
		withdrawbank();
		break;
	    case '?':
		od_disp_str("Help\r\n");
		menuit();
		break;
	    case 'E':
		od_disp_str("Edit Announcement\r\n");
		bulletin();
		break;
	    case 'Z':
		od_disp_str("Spy\r\n");
		spy();
		break;
	    case 'V':
		od_disp_str("Version\r\n");
		od_set_color(L_BLUE, D_BLACK);
		od_disp_str("This Is Dragonlance version 3.0\r\n");
		pausescr();
		break;
	    case '*':
		od_disp_str("Change Name\r\n");
		nl();
		od_set_color(L_CYAN, D_BLACK);
		od_disp_str("Your family crest has been stolen, they\r\n");
		od_disp_str("inscribe a new one with the name...\r\n");
		od_disp_str("> ");
		od_input_str(temp, 30, ' ', '~');
		nl();
		od_disp_str("Are you sure? ");
		if (od_get_answer("YN") == 'Y') {
		    od_disp_str("Yes\r\n");
		    if (strlen(temp))
			SAFECOPY(user.pseudo, temp);
		} else
		    od_disp_str("No\r\n");
		break;
	    case '#':
		od_disp_str("Change Battle Cry\r\n");
		vic();
		break;
	}
    }
    return (0);
}
