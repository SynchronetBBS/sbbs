#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "OpenDoor.h"

#define FIGHTS_PER_DAY	10
#define BATTLES_PER_DAY	3
#define FLIGHTS_PER_DAY	3

#ifndef INT64
#ifdef _MSC_VER
#define INT64	__int64
#else
#define INT64	long long int
#endif
#endif

#ifndef INT64FORMAT
#ifdef __BORLANDC__
#define	INT64FORMAT	"Ld"
#else
#ifdef _MSC_VER
#define INT64FORMAT	"I64d"
#else
#define INT64FORMAT		"lld"
#endif
#endif
#endif

#ifndef QWORD
#ifdef _MSC_VER
#define QWORD	unsigned _int64
#else
#define	QWORD	unsigned long long int
#endif
#endif

#ifndef QWORDFORMAT
#ifdef __BORLANDC__
#define	QWORDFORMAT	"Lu"
#else
#ifdef _MSC_VER
#define 	QWORDFORMAT	"I64u"
#else
#define 	QWORDFORMAT	"llu"
#endif
#endif
#endif

enum {
    ALIVE
    ,DEAD
};
/* Status for opponent != a player */
#define		MONSTER		0

struct playertype {
    char            name[31];	       /* Name from BBS drop file */
    char            pseudo[31];	       /* In-game pseudonym */
    char            killer[31];	       /* Person killed by (if status==DEAD) */
    char            gaspd[61];	       /* Dying curse */
    DWORD           status;	       /* Status alive or dead for player
				        * records, player number or MONSTER
				        * for opponent record (record 0) */
    /* ToDo make configurable */
    WORD            flights;	       /* Weird thing... may be number of
				        * times can run */
    WORD            plus;	       /* Weapon Bonus - Not currently Used */
    WORD            r;		       /* Level */
    WORD            strength;	       /* Str */
    WORD            intelligence;      /* Int */
    WORD            dexterity;	       /* Dex */
    WORD            constitution;      /* Con */
    WORD            charisma;	       /* Cha */
    QWORD           experience;	       /* Exp */
    WORD            luck;	       /* Luk */
    WORD            weapon;	       /* Weapon Number */
    WORD            armour;	       /* Armour Number */
    WORD            hps;	       /* Hit Points */
    WORD            damage;	       /* Damage taken so far */
    QWORD           gold;	       /* Gold on hand */
    QWORD           bank;	       /* Gold in bank */
    WORD            attack;	       /* Weapon Attack */
    WORD            power;	       /* Weapon Power */
    QWORD           wins;	       /* Wins so far */
    QWORD           loses;	       /* Losses so far */
    WORD            battles;	       /* Battles left today */
    WORD            fights;	       /* Fights left today */
    /* Players variance... applied to various rolls, Generally, how good the
     * player is feeling. Is set at start and not modified */
    double          vary;
};

struct playertype player[31];
#define user	player[player_num]
/* player[0] is the current enemy */
#define opp	player[0]

const char      dots[] = "...............................................";
/* In these arrays, 0 isn't actually used */
QWORD           required[29];	       /* Experience required for each level */
char            wname[26][31];	       /* Array of weapon names */
DWORD           w2[26];		       /* Weapon "attack" values */
DWORD           w3[26];		       /* Weapon "power" values */
char            sname[26][31];	       /* Array of shield names */
char            temp[81];
DWORD           cost[26];	       /* Array of weapon/shield costs */
FILE           *infile;		       /* Current input file */
DWORD           player_num;	       /* Current Player Number */
DWORD           number_of_players;     /* Number of players in player[] */
/**********************************************************/
/* These variables are used for the stat adjustment stuff */
/**********************************************************/
BOOL            partone;
BOOL            bothover;
/************************/
/* Functions from xpdev */
/************************/
#define SAFECOPY(dst,src)                               sprintf(dst,"%.*s",(int)sizeof(dst)-1,src)
#if defined(__unix__)
#if !defined(stricmp)
#define stricmp(x,y)            strcasecmp(x,y)
#define strnicmp(x,y,z)         strncasecmp(x,y,z)
#endif
#endif
/****************************************************************************/
/* Truncates all white-space chars off end of 'str'     (needed by STRERRROR)   */
/****************************************************************************/
char           *
truncsp(char *str)
{
    size_t          i, len;
    i = len = strlen(str);
    while (i && (str[i - 1] == ' ' || str[i - 1] == '\t' || str[i - 1] == '\r' || str[i - 1] == '\n'))
	i--;
    if (i != len)
	str[i] = 0;		       /* truncate */

    return (str);
}

int
xp_random(int n)
{
    float           f;
    static BOOL     initialized;
    if (!initialized) {
	srand(time(NULL));	       /* seed random number generator */
	rand();			       /* throw away first result */
	initialized = TRUE;
    }
    if (n < 2)
	return (0);
    f = (float)rand() / (float)RAND_MAX;

    return ((int)(n * f));
}
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

void
pausescr(void)
{
    od_set_color(L_CYAN, D_BLACK);
    od_disp_str("[Pause]");
    od_set_color(D_GREY, D_BLACK);
    od_get_key(TRUE);
    od_disp_str("\010 \010\010 \010\010 \010\010 \010\010 \010\010 \010\010 \010");
}
#define nl()	od_disp_str("\r\n");

/********************************/
/* Functions from original .pas */
/********************************/

double
playerattack(void)
{
    return ((user.attack * user.strength * user.dexterity * (xp_random(5) + 1) * user.vary) /
	    (opp.armour * opp.dexterity * opp.luck * opp.vary));
}

double
playerattack2(void)
{
    return ((user.power * user.strength * (xp_random(5) + 1) * (xp_random(5) + 1) * user.vary) /
	    ((opp.armour) * opp.luck * opp.vary));
}

double
opponentattack(void)
{
    return ((opp.attack * opp.strength * opp.dexterity * (xp_random(5) + 1) * opp.vary) /
	    (user.armour * user.dexterity * user.luck * user.vary));
}

double
opponentattack2(void)
{
    return ((opp.power * opp.strength * (xp_random(5) + 1) * (xp_random(5) + 1) * opp.vary) /
	    ((user.armour) * user.luck * user.vary));
}

double
supplant(void)
{
    return ((double)(xp_random(40) + 80)) / 100;
}

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

char           *
readline(char *buf, size_t size)
{
    fgets(buf, size, infile);
    truncsp(buf);
    return (buf);
}

void
checkday(void)
{
    char            oldy[256];
    DWORD           h, i;
    FILE           *outfile;
    for (i = 1; i < number_of_players; i++) {
	for (h = i + 1; h <= number_of_players; h++) {
	    if (player[h].experience > player[i].experience) {
		if (h == player_num)
		    player_num = i;
		else if (i == player_num)
		    player_num = h;
		player[number_of_players + 1] = player[h];
		player[h] = player[i];
		player[i] = player[number_of_players + 1];
	    }
	}
    }

    infile = fopen("data/date.lan", "r+b");
    readline(oldy, sizeof(oldy));

    if (strcmp(oldy, date())) {
	fseek(infile, 0, SEEK_SET);
	fputs(date(), infile);
	fputs("\n", infile);
	for (i = 1; i <= number_of_players; i++) {
	    player[i].battles = BATTLES_PER_DAY;
	    player[i].fights = FIGHTS_PER_DAY;
	    player[i].flights = FLIGHTS_PER_DAY;
	    player[i].status = ALIVE;
	    player[i].vary = supplant();
	}
	outfile = fopen("data/record.lan", "wb");
	fclose(outfile);
    }
    fclose(infile);
}

void
playerlist(void)
{
    int             i;
    checkday();
    od_clr_scr();
    nl();
    od_set_color(D_MAGENTA, D_BLACK);
    od_disp_str("Hero Rankings:\r\n");
    od_disp_str("!!!!!!!!!!!!!!\r\n");
    for (i = 1; i <= number_of_players; i++) {
	nl();
	od_set_color(D_MAGENTA, D_BLACK);
	od_printf("%2u.  `bright cyan`%.30s%.*s`green`Lev=%-2u  W=%-2" QWORDFORMAT "  L=%-2" QWORDFORMAT "  S=%s"
		  ,i, player[i].pseudo, 30 - strlen(player[i].pseudo), dots, player[i].r
		  ,player[i].wins, player[i].loses
		  ,(player[i].status == DEAD ? "SLAIN" : "ALIVE"));
    }
}

void
leave(void)
{
    int             a;
    FILE           *outfile;
    nl();
    outfile = fopen("data/characte.lan", "wb");
    fprintf(outfile, "%lu\n", number_of_players);
    for (a = 1; a <= number_of_players; a++) {
	fprintf(outfile, "%s\n", player[a].name);
	fprintf(outfile, "%s\n", player[a].pseudo);
	fprintf(outfile, "%s\n", player[a].gaspd);
	fprintf(outfile, "%u\n", player[a].status);
	if (player[a].status == DEAD)
	    fprintf(outfile, "%s\n", player[a].killer);
	fprintf(outfile, "%u %u %u %u %u %u %" QWORDFORMAT " %u %u %u %u %" QWORDFORMAT " %u %" QWORDFORMAT " %" QWORDFORMAT " %" QWORDFORMAT " %u %u %u %1.4f\n",
		player[a].strength, player[a].intelligence,
		player[a].luck, player[a].dexterity, player[a].constitution,
		player[a].charisma, player[a].experience,
		player[a].r, player[a].hps, player[a].weapon,
		player[a].armour, player[a].gold,
	 player[a].flights, player[a].bank, player[a].wins, player[a].loses,
	player[a].plus, player[a].fights, player[a].battles, player[a].vary);
    }
    fclose(outfile);
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
    user.damage = user.damage - opt;
    user.gold = user.gold - 8 * opt * user.r;
    od_printf("%u hit points healed.\r\n", opt);
}

void
findo(void)
{
    int             okea;
    nl();
    od_set_color(L_YELLOW, D_BLACK);
    od_disp_str("The Vile Enemy Dropped Something!\r\n");
    od_disp_str("Do you want to get it? ");
    if (od_get_answer("YN") == 'Y') {
	od_disp_str("Yes\r\n");
	okea = xp_random(99) + 1;
	if ((okea < 10) && (user.weapon >= 25)) {
	    user.weapon++;
	    od_printf("You have found a %s.\r\n", wname[user.weapon]);
	}
	if ((okea > 11) && (okea < 40)) {
	    user.gold = user.gold + 40;
	    od_disp_str("You have come across 40 Steel pieces\r\n");
	} else
	    od_disp_str("It's gone!!!!\r\n");
    } else {
	od_disp_str("No\r\n");
	od_disp_str("I wonder what it was?!?\r\n");
    }
}

void            battle(void);

void
mutantvictory(void)
{
    int             bt;
    int             d;
    FILE           *outfile;
    if (opp.status == MONSTER)
	opp.gold = opp.gold * supplant();
    nl();
    od_printf("You take his %" QWORDFORMAT " Steel pieces.\r\n", opp.gold);
    user.gold = user.gold + opp.gold;
    if (opp.status != MONSTER) {
	nl();
	od_set_color(D_GREEN, D_BLACK);
	od_disp_str("The Last Words He Utters Are...\r\n");
	nl();
	od_printf("\"%s\"\r\n", player[opp.status].gaspd);
	nl();
	user.wins++;
	player[opp.status].loses++;
	SAFECOPY(player[opp.status].killer, user.name);
	player[opp.status].status = DEAD;
	player[opp.status].gold = 0;
	if (player[opp.status].weapon > user.weapon) {
	    d = user.weapon;
	    user.weapon = player[opp.status].weapon;
	    player[opp.status].weapon = d;
	    bt = user.plus;
	    user.plus = player[opp.status].plus;
	    player[opp.status].plus = bt;
	    od_set_color(D_GREEN, D_BLACK);
	    od_disp_str("You Hath Taken His Weapon.\r\n");
	}
	if (player[opp.status].armour > user.armour) {
	    d = user.armour;
	    user.armour = player[opp.status].armour;
	    player[opp.status].armour = d;
	    od_set_color(L_YELLOW, D_BLACK);
	    od_disp_str("You Hath Taken His Armour.\r\n");
	}
	user.attack = w2[user.weapon];
	user.power = w3[user.weapon];
	outfile = fopen("data/record.lan", "ab");
	fprintf(outfile, "%s conquered %s\r\n", user.pseudo, player[opp.status].pseudo);
	fclose(outfile);
    }
    opp.experience *= supplant();
    user.experience += opp.experience;
    od_printf("You obtain %" QWORDFORMAT " exp points.\r\n", opp.experience);
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

void
quickexit(void)
{
    od_disp_str("The Gods of Krynn Disapprove of your actions!\r\n");
    exit(1);
}

void
amode(void)
{
    double          roll;	       /* Used for To Hit and damage rolls */
    int             okea;
    int             tint;
    roll = playerattack();
    if (roll < 1.5) {
	tint = xp_random(3) + 1;
	switch (tint) {
	    case 1:
		od_disp_str("Swish you missed!\r\n");
		break;
	    case 2:
		od_disp_str("HA HA! He dodges your swing!\r\n");
		break;
	    case 3:
		od_disp_str("CLANG, The attack is blocked!\r\n");
		break;
	    case 4:
		od_disp_str("You Fight vigorously and miss!\r\n");
		break;
	}
    } else {
	roll = playerattack2();
	if (roll > 5 * user.power)
	    roll = 5 * user.power;
	if (roll < 1)
	    roll = 1;
	opp.damage += roll;
	tint = xp_random(2) + 1;
	switch (tint) {
	    case 1:
		od_printf("You sliced him for %1.0f.\r\n", roll);
		break;
	    case 2:
		od_printf("You made contact to his body for %1.0f.\r\n", roll);
		break;
	    case 3:
		od_printf("You hacked him for %1.0f.", roll);
		break;
	}
	if (opp.hps <= opp.damage) {
	    nl();
	    tint = xp_random(3) + 1;
	    switch (tint) {
		case 1:
		    od_disp_str("A Painless Death!\r\n");
		    break;
		case 2:
		    od_disp_str("It Hath died!\r\n");
		    break;
		case 3:
		    od_disp_str("A Smooth killing!\r\n");
		    break;
		case 4:
		    od_disp_str("It has gone to the Abyss!\r\n");
		    break;
	    }
	    okea = xp_random(99) + 1;
	    if (okea < 30)
		findo();
	    mutantvictory();
	}
    }
}

void
bmode(void)
{
    double          roll;	       /* Used for To Hit and damage rolls */
    FILE           *outfile;
    int             okea;
    int             tint;
    if ((opp.hps > opp.damage) && user.damage < user.hps) {
	roll = opponentattack();
	if (roll < 1.5) {
	    od_set_color(D_GREEN, D_BLACK);
	    od_disp_str("His attack tears your armour.\r\n");
	} else {
	    roll = opponentattack2();
	    if (roll > 5 * opp.power)
		roll = 5 * opp.power;
	    if (roll < 1)
		roll = 1;
	    tint = xp_random(2) + 1;
	    switch (tint) {
		case 1:
		    od_printf("He hammered you for %1.0f.\r\n", roll);
		    break;
		case 2:
		    od_printf("He swung and hit for %1.0f.\r\n", roll);
		    break;
		case 3:
		    od_printf("You are surprised when he hits you for %1.0f.\r\n", roll);
		    break;
	    }
	    user.damage = user.damage + roll;
	    if (user.damage >= user.hps) {
		nl();
		tint = xp_random(3) + 1;
		switch (tint) {
		    case 1:
			od_disp_str("Return This Knight To Huma's Breast....\r\n");
			break;
		    case 2:
			od_disp_str("You Hath Been Slain!!\r\n");
			break;
		    case 3:
			od_disp_str("Return Soon Brave Warrior!\r\n");
			break;
		    case 4:
			od_disp_str("May Palidine Be With You!!\r\n");
			break;
		}
		if (opp.status != MONSTER) {
		    player[opp.status].wins++;
		    user.loses++;
		    player[opp.status].gold += user.gold;
		    user.gold = 0;
		    outfile = fopen("data/record.lan", "ab");
		    fprintf(outfile, "%s killed %s\r\n", user.name, player[opp.status].name);
		    fclose(outfile);
		}
	    }
	}
    }
}

void
statshow(void)
{
    od_clr_scr();

    od_set_color(D_MAGENTA, D_BLACK);
    od_printf("Name: %s   Level: %d\r\n", user.pseudo, user.r);
    od_set_color(L_CYAN, D_BLACK);
    od_printf("W/L: %" QWORDFORMAT "/%" QWORDFORMAT "   Exp: %" QWORDFORMAT "\r\n", user.wins, user.loses, user.experience);
    nl();
    od_set_color(L_YELLOW, D_BLACK);
    od_printf("Steel  (in hand): %" QWORDFORMAT "\r\n", user.gold);
    od_printf("Steel  (in bank): %" QWORDFORMAT "\r\n", user.bank);
    nl();
    od_set_color(L_BLUE, D_BLACK);
    od_printf("Battles: %u   Retreats: %u    Fights: %u   Hps: %u(%u)\r\n", user.battles, user.flights, user.fights, user.hps - user.damage, user.hps);
    nl();
    od_set_color(L_CYAN, D_BLACK);
    od_printf("Weapon: %s     Armor: %s\r\n", wname[user.weapon], sname[user.armour]);
}

void
incre(void)
{
    od_disp_str("Increase which stat? ");
    switch (od_get_answer("123456Q")) {
	case '1':
	    od_disp_str("Strength\r\n");
	    user.strength++;
	    break;
	case '2':
	    od_disp_str("Intelligence\r\n");
	    user.intelligence++;
	    break;
	case '3':
	    od_disp_str("Dexterity\r\n");
	    user.dexterity++;
	    break;
	case '4':
	    od_disp_str("Luck\r\n");
	    user.luck++;
	    break;
	case '5':
	    od_disp_str("Constitution\r\n");
	    user.constitution++;
	    break;
	case '6':
	    od_disp_str("Charisma\r\n");
	    user.charisma++;
	    break;
	case 'Q':
	    od_disp_str("Quit\r\n");
	    partone = FALSE;
	    break;
    }
}

void
decre(void)
{
    od_disp_str("Decrease which stat? ");
    switch (od_get_answer("123456")) {
	case '1':
	    od_disp_str("Strength\r\n");
	    user.strength -= 2;
	    break;
	case '2':
	    od_disp_str("Intelligence\r\n");
	    user.intelligence -= 2;
	    break;
	case '3':
	    od_disp_str("Dexterity\r\n");
	    user.dexterity -= 2;
	    break;
	case '4':
	    od_disp_str("Luck\r\n");
	    user.luck -= 2;
	    break;
	case '5':
	    od_disp_str("Constitution\r\n");
	    user.constitution -= 2;
	    break;
	case '6':
	    od_disp_str("Charisma\r\n");
	    user.charisma -= 2;
	    break;
    }
}

void
ministat(void)
{
    BOOL            yaya = TRUE;
    partone = FALSE;
    nl();
    od_disp_str("Status Change:\r\n");
    od_disp_str("^^^^^^^^^^^^^^\r\n");
    od_printf("1> Str: %u\r\n", user.strength);
    od_printf("2> Int: %u\r\n", user.intelligence);
    od_printf("3> Dex: %u\r\n", user.dexterity);
    od_printf("4> Luk: %u\r\n", user.luck);
    od_printf("5> Con: %u\r\n", user.constitution);
    od_printf("6> Chr: %u\r\n", user.charisma);
    nl();
    if (user.strength < 6) {
	yaya = FALSE;
	nl();
	od_disp_str("Strength cannot go below 6\r\n");
    }
    if (user.intelligence < 6) {
	yaya = FALSE;
	nl();
	od_disp_str("Intelligence cannot go below 6\r\n");
    }
    if (user.dexterity < 6) {
	yaya = FALSE;
	nl();
	od_disp_str("Dexterity cannot go below 6\r\n");
    }
    if (user.luck < 6) {
	yaya = FALSE;
	nl();
	od_disp_str("Luck cannot go below 6\r\n");
    }
    if (user.constitution < 6) {
	yaya = FALSE;
	nl();
	od_disp_str("Constitution cannot go below 6\r\n");
    }
    if (user.charisma < 6) {
	yaya = FALSE;
	nl();
	od_disp_str("Charisma cannot go below 6\r\n");
    }
    if (yaya) {
	od_disp_str("Is this correct? ");
	if (od_get_answer("YN") == 'Y') {
	    od_disp_str("Yes\r\n");
	} else {
	    od_disp_str("No\r\n");
	    user = opp;
	    bothover = FALSE;
	    bothover = FALSE;
	}
    } else {
	user = opp;
	bothover = FALSE;
    }
}

void
chstats(void)
{
    od_clr_scr();
    bothover = FALSE;
    opp = user;

    for (;;) {
	partone = TRUE;
	od_disp_str("Status Change:\r\n");
	od_disp_str("@@@@@@@@@@@@@\r\n");
	od_disp_str("You may increase any stat by one,\r\n");
	od_disp_str("yet you must decrease another by two.\r\n");
	nl();
	od_printf("1> Str: %u\r\n", user.strength);
	od_printf("2> Int: %u\r\n", user.intelligence);
	od_printf("3> Dex: %u\r\n", user.dexterity);
	od_printf("4> Luk: %u\r\n", user.luck);
	od_printf("5> Con: %u\r\n", user.constitution);
	od_printf("6> Chr: %u\r\n", user.charisma);
	nl();
	incre();
	if (partone)
	    decre();
	else
	    partone = FALSE;
	if (partone)
	    bothover = TRUE;
	if (!partone)
	    bothover = FALSE;
	else
	    ministat();
	if (!bothover)
	    break;
    }
}

void
attackmodes(void)
{
    if (opp.dexterity > user.dexterity) {
	if (user.damage < user.hps && opp.damage < opp.hps)
	    bmode();
	if (user.damage < user.hps && opp.damage < opp.hps)
	    amode();
    } else if (opp.dexterity < user.dexterity) {
	if (user.damage < user.hps && opp.damage < opp.hps)
	    amode();
	if (user.damage < user.hps && opp.damage < opp.hps)
	    bmode();
    } else {
	if (user.damage < user.hps && opp.damage < opp.hps)
	    amode();
	if (user.damage < user.hps && opp.damage < opp.hps)
	    bmode();
    }
}

double
readfloat(float deflt)
{
    char            buf[101];
    int             pos = 0;
    int             rd;
    double          ret;
    buf[0] = 0;
    while ((rd = fgetc(infile)) != EOF) {
	if (rd == '\r')
	    continue;
	if (rd == '\n') {
	    ungetc('\n', infile);
	    break;
	}
	/* Skip leading spaces */
	if (!pos && isspace(rd))
	    continue;
	if (isspace(rd))
	    break;
	buf[pos++] = rd;
    }
    if (!pos)
	return (deflt);
    buf[pos++] = 0;
    ret = strtod(buf, NULL);
    return (ret);
}

QWORD
readnumb(QWORD deflt)
{
    char            buf[101];
    int             pos = 0;
    int             rd;
    QWORD           ret;
    buf[0] = 0;
    while ((rd = fgetc(infile)) != EOF) {
	if (rd == '\r')
	    continue;
	if (rd == '\n') {
	    ungetc('\n', infile);
	    break;
	}
	/* Skip leading spaces */
	if (!pos && isspace(rd))
	    continue;
	if (isspace(rd))
	    break;
	buf[pos++] = rd;
    }
    if (!pos)
	return (deflt);
    buf[pos++] = 0;
    ret = strtoull(buf, NULL, 10);
    return (ret);
}

void
endofline(void)
{
    int             rd;
    while ((rd = fgetc(infile)) != EOF) {
	if (rd == '\n');
	break;
    }
}

void
searcher(void)
{
    int             a;
    int             rd;
    user.fights--;
    rd = xp_random(readnumb(0) - 1) + 1;
    endofline();
    for (a = 1; a <= rd; a++) {
	readline(opp.name, sizeof(opp.name));
	opp.hps = readnumb(10);
	opp.damage = 0;
	opp.attack = readnumb(1);
	opp.power = readnumb(1);
	opp.armour = readnumb(1);
	opp.luck = readnumb(1);
	opp.strength = readnumb(6);
	opp.dexterity = readnumb(6);
	opp.gold = readnumb(0);
	opp.experience = readnumb(0);
	opp.status = MONSTER;
	endofline();
    }
    fclose(infile);
    opp.attack *= supplant();
    if (opp.attack < 1)
	opp.attack = 1;
    opp.power *= supplant();
    if (opp.power < 1)
	opp.power = 1;
    opp.armour *= supplant();
    if (opp.armour < 1)
	opp.armour = 1;
    opp.luck *= supplant();
    if (opp.luck < 1)
	opp.luck = 1;
    opp.strength *= supplant();
    if (opp.strength < 1)
	opp.strength = 1;
    opp.dexterity *= supplant();
    if (opp.dexterity < 1)
	opp.dexterity = 1;
    opp.hps *= supplant();
    if (opp.hps < 1)
	opp.hps = 1;
    opp.vary = supplant();
}

void
fight(char *filename)
{
    infile = fopen(filename, "rb");
    searcher();
    battle();
}

void
doggie(void)
{
    char            tmphh[3];
    int             a;
    DWORD           enemy;
    BOOL            finder;
    if (user.battles == 0) {
	nl();
	checkday();
	for (finder = FALSE; finder == FALSE;) {
	    for (enemy = 0; enemy == 0;) {
		od_clr_scr();
		od_set_color(L_YELLOW, D_BLACK);
		od_disp_str("Battle Another Hero:\r\n");
		od_disp_str("********************\r\n");
		if (number_of_players == 1) {
		    od_disp_str("You are the only hero on Krynn!\r\n");
		    return;
		} else {
		    a = 1;
		    for (a = 1; a <= number_of_players;) {
			if (player[a].r > user.r - 4) {
			    nl();
			    od_set_color(D_MAGENTA, D_BLACK);
			    od_printf("%2u.  `bright cyan`%.30s%.*s`bright blue`Lev=%-2u  W=%-2" QWORDFORMAT "  L=%-2" QWORDFORMAT "  S=%s"
				      ,a, player[a].pseudo, 30 - strlen(player[a].pseudo), dots
			       ,player[a].r, player[a].wins, player[a].loses
			    ,(player[a].status == DEAD ? "SLAIN" : "ALIVE"));
			}
			a += 1;
		    }
		    nl();
		    od_set_color(L_CYAN, D_BLACK);
		    od_disp_str("Enter the rank # of your opponent: ");
		    od_input_str(tmphh, 2, '0', '9');
		    enemy = strtoul(tmphh, NULL, 10);
		    if ((enemy == 0) || (!strcmp(player[enemy].pseudo, user.pseudo)) || (player[enemy].status == DEAD))
			return;
		}
	    }
	    for (a = 1; a <= number_of_players; a++) {
		if (enemy == a) {
		    if (player[a].r > (user.r - 4)) {
			finder = TRUE;
			opp.status = a;
		    }
		}
	    }
	}
	SAFECOPY(opp.name, player[opp.status].pseudo);
	opp.hps = player[opp.status].hps;
	opp.damage = player[opp.status].hps;
	opp.vary = player[opp.status].vary;
	user.battles--;
	opp.attack = w2[player[opp.status].weapon];
	opp.power = w3[player[opp.status].weapon];
	opp.armour = player[opp.status].armour;
	opp.luck = player[opp.status].luck;
	opp.strength = player[opp.status].strength;
	opp.dexterity = player[opp.status].dexterity;
	opp.gold = player[opp.status].gold;
	opp.experience = player[opp.status].experience / 10;
	finder = FALSE;
	battle();
    }
}

void
battle(void)
{
    WORD            playerrem;
    char            option;
    nl();
    while (user.damage < user.hps && opp.damage < opp.hps) {
	playerrem = user.hps - user.damage;
	nl();
	od_set_color(L_YELLOW, D_BLACK);
	od_printf("You are attacked by a %s.\r\n", opp.name);
	for (option = '?'; option == '?';) {
	    od_set_color(L_BLUE, D_BLACK);
	    od_printf("Combat (%u hps): (B,F,S): ", playerrem);
	    option = od_get_answer("BFS?");
	    if (option == '?') {
		od_disp_str("Help\r\n");
		nl();
		nl();
		od_disp_str("(B)attle your opponent.\r\n");
		od_disp_str("(F)lee from your opponent.\r\n");
		od_disp_str("(S)tatus check.\r\n");
		nl();
	    }
	}
	switch (option) {
	    case 'B':
		od_disp_str("Battle\r\n");
		attackmodes();
		break;
	    case 'S':
		od_disp_str("Stats\r\n");
		statshow();
		break;
	    case 'F':
		od_disp_str("Flee\r\n");
		if ((xp_random(4) + 1) + user.dexterity > opp.dexterity) {
		    nl();
		    od_set_color(D_GREEN, D_BLACK);
		    od_disp_str("You Ride away on a Silver Dragon.\r\n");
		    return;
		}
		break;
	}
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
	od_input_str(temp, 60, ' ', '~');
	od_disp_str("Is this correct? ");
	if (od_get_answer("YN") == 'Y') {
	    ahuh = TRUE;
	    od_disp_str("Yes\r\n");
	} else {
	    ahuh = FALSE;
	    od_disp_str("No\r\n");
	}
    }
    ahuh = TRUE;
}

void
create(BOOL isnew)
{
    DWORD           newnum;
    if (isnew) {
	nl();
	newnum = ++number_of_players;
	SAFECOPY(player[newnum].name, od_control.user_name);
	SAFECOPY(player[newnum].pseudo, od_control.user_name);
	vic();
    } else {
	newnum = player_num;
	SAFECOPY(temp, player[newnum].gaspd);
    }
    player[newnum].strength = 12;
    player[newnum].status = ALIVE;
    player[newnum].intelligence = 12;
    player[newnum].luck = 12;
    player[newnum].damage = 0;
    SAFECOPY(player[newnum].gaspd, temp);
    player[newnum].dexterity = 12;
    player[newnum].constitution = 12;
    player[newnum].charisma = 12;
    player[newnum].gold = xp_random(100) + 175;
    player[newnum].weapon = 1;
    player[newnum].armour = 1;
    player[newnum].experience = 0;
    player[newnum].plus = 0;
    player[newnum].bank = (xp_random(199) + 1);
    player[newnum].r = 1;
    player[newnum].hps = (xp_random(4) + 1) + player[newnum].constitution;
    player[newnum].flights = FLIGHTS_PER_DAY;
    player[newnum].wins = 0;
    player[newnum].loses = 0;
    nl();
    player_num = newnum;
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
	od_printf("  %2d>  %-25.25s   %-25.25s   %9u\r\n", i, wname[i], sname[i], cost[i]);
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
		if (cost[buy] > user.gold)
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
				user.gold -= cost[buy];
				user.weapon = buy;
				nl();
				od_set_color(D_MAGENTA, D_BLACK);
				od_printf("You've bought a %s\r\n", wname[buy]);
				user.attack = w2[user.weapon];
				user.power = w3[user.weapon];
			    } else
				od_disp_str("No\r\n");
			    break;
			case 'A':
			    od_disp_str("Armour\r\n");
			    od_disp_str("Are you sure you want buy it? ");
			    if (od_get_answer("YN") == 'Y') {
				od_disp_str("Yes\r\n");
				user.gold -= cost[buy];
				user.armour = buy;
				nl();
				od_set_color(D_MAGENTA, D_BLACK);
				od_printf("You've bought a %s\r\n", sname[buy]);
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
			buyprice = buyprice * cost[user.weapon];
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
			buyprice = user.charisma * cost[user.armour] / 20;
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
    int             a;
    od_clr_scr();
    od_disp_str("Spying on another user eh.. well you may spy, but to keep\r\n");
    od_disp_str("you from copying this person's stats, they will not be   \r\n");
    od_disp_str("available to you.  Note that this is gonna cost you some \r\n");
    od_disp_str("cash too.  Cost: 20 Steel pieces                         \r\n");
    nl();
    od_disp_str("Who do you wish to spy on? ");
    od_input_str(aa, sizeof(aa) - 1, ' ', '~');
    for (a = 1; a <= number_of_players; a++) {
	if (!stricmp(player[a].pseudo, aa)) {
	    if (user.gold < 20) {
		od_set_color(L_RED, B_BLACK);
		od_disp_str("You do not have enough Steel!\r\n");
	    } else {
		user.gold -= 20;
		nl();
		od_set_color(L_RED, B_BLACK);
		od_printf("%s\r\n", player[a].pseudo);
		nl();
		od_printf("Level  : %u\r\n", player[a].r);
		od_printf("Exp    : %" QWORDFORMAT "\r\n", player[a].experience);
		od_printf("Flights: %u\r\n", player[a].flights);
		od_printf("Hps    : %u(%u)\r\n", player[a].hps - player[a].damage, player[a].hps);
		nl();
		od_set_color(D_MAGENTA, D_BLACK);
		od_printf("Weapon : %s\r\n", wname[player[a].weapon]);
		od_printf("Armour : %s\r\n", sname[player[a].armour]);
		nl();
		od_set_color(L_YELLOW, D_BLACK);
		od_printf("Steel  (in hand): %" QWORDFORMAT "\r\n", player[a].gold);
		od_printf("Steel  (in bank): %" QWORDFORMAT "\r\n", player[a].bank);
		nl();
		pausescr();
	    }
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
afight(int lev)
{
    char            fname[32];
    if (user.fights == 0) {
	nl();
	od_set_color(D_MAGENTA, D_BLACK);
	od_disp_str("It's Getting Dark Out!\r\n");
	od_disp_str("Return to the Nearest Inn!\r\n");
    } else {
	od_clr_scr();
	sprintf(fname, "data/junkm%d.lan", lev);
	fight(fname);
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
    atexit(leave);

    od_init();
    for (i = 1; i <= 30; i++)
	player[i].damage = 0;
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
    infile = fopen("data/characte.lan", "rb");
    fgets(temp, sizeof(temp), infile);
    number_of_players = strtoul(temp, NULL, 10);
    for (i = 1; i <= number_of_players; i++) {
	readline(player[i].name, sizeof(player[i].name));
	readline(player[i].pseudo, sizeof(player[i].pseudo));
	readline(player[i].gaspd, sizeof(player[i].gaspd));
	fgets(temp, sizeof(temp), infile);
	player[i].status = strtoul(temp, NULL, 10);
	if (player[i].status == DEAD)
	    readline(player[i].killer, sizeof(player[i].killer));
	player[i].strength = readnumb(6);
	player[i].intelligence = readnumb(6);
	player[i].luck = readnumb(6);
	player[i].dexterity = readnumb(6);
	player[i].constitution = readnumb(6);
	player[i].charisma = readnumb(6);
	player[i].experience = readnumb(0);
	player[i].r = readnumb(1);
	player[i].hps = readnumb(10);
	player[i].weapon = readnumb(1);
	player[i].armour = readnumb(1);
	player[i].gold = readnumb(0);
	player[i].flights = readnumb(3);
	player[i].bank = readnumb(0);
	player[i].wins = readnumb(0);
	player[i].loses = readnumb(0);
	player[i].plus = readnumb(0);
	player[i].fights = readnumb(FIGHTS_PER_DAY);
	player[i].battles = readnumb(BATTLES_PER_DAY);
	player[i].vary = readfloat(supplant());
	endofline();
    }
    fclose(infile);
    for (i = 1; i <= number_of_players; i++) {
	if (!strcmp(player[i].name, od_control.user_name)) {
	    found = TRUE;
	    player_num = i;
	    break;
	}
    }
    if (!found) {
	if (number_of_players >= 30)
	    quickexit();
	else
	    create(TRUE);
    }
    if (user.status == DEAD) {
	nl();
	od_set_color(L_CYAN, D_BLACK);
	od_printf("A defeat was lead over you by %s.", user.killer);
    }
    checkday();
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
	readline(wname[i], sizeof(wname[i]));
	w2[i] = readnumb(1);
	w3[i] = readnumb(1);
	endofline();
    }
    fclose(infile);
    infile = fopen("data/armor.lan", "rb");
    od_set_color(L_BLUE, D_BLACK);
    for (i = 1; i <= 25; i++)
	readline(sname[i], sizeof(sname[i]));
    fclose(infile);
    infile = fopen("data/prices.lan", "rb");
    for (i = 1; i <= 25; i++) {
	cost[i] = readnumb(100000000);
	endofline();
    }
    fclose(infile);
    user.attack = w2[user.weapon];
    user.power = w3[user.weapon];
    infile = fopen("data/experience.lan", "rb");
    for (i = 1; i <= 28; i++) {
	required[i] = readnumb(100000000);
	endofline();
    }
    fclose(infile);
    od_set_color(L_YELLOW, D_BLACK);
    user.status = ALIVE;
    statshow();
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
		SAFECOPY(user.gaspd, temp);
		break;
	}
    }
    return (0);
}
