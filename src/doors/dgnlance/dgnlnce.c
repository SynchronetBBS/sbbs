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
 #e
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

struct playertype {
    char            name[31];	       /* Name from BBS drop file */
    char            pseudo[31];	       /* In-game pseudonym */
    char            killer[31];	       /* Person killed by (if status==DEAD) */
    char            gaspd[61];	       /* Dying curse */
    WORD            status;	       /* Status alive or dead */
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
};

struct opponent {
    char            name[31];
    QWORD           gold;
    WORD            strength;
    WORD            dexterity;
    WORD            luck;
    WORD            hps;
    WORD            damage;
    WORD            attack;
    WORD            power;
    WORD            armour;
};

const char      dots[] = "...............................................";

QWORD           required[29];	       /* In these arrays, 0 isn't actually
				        * used */
char            wname[26][31];
char            sname[26][31];
char            temp[81];
DWORD           cost[26];
DWORD           w2[26];
DWORD           w3[26];
double          vary;
double          vary2;
double          roll;
QWORD           gain_exp;
char            blt[5][61];
FILE           *infile;
FILE           *outfile;
FILE           *messfile;
struct opponent opp;
DWORD           player_num;	       /* Current Player Number */
DWORD           curr_opp;	       /* Player number who is currently
				        * being fought */
int             dog;
DWORD           number_of_players;
int             okea;
int             trips;
int             tint;
BOOL            play;
BOOL            doga;
BOOL            uni;
BOOL            live;
BOOL            partone;
BOOL            bothover;
struct playertype player[31];
WORD            temp1a, temp1b, temp1c, temp1d, temp1e, temp1f;
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
    return ((player[player_num].attack * player[player_num].strength * player[player_num].dexterity * (xp_random(5) + 1) * vary) /
	    (opp.armour * opp.dexterity * opp.luck * vary2));
}

double
playerattack2(void)
{
    return ((player[player_num].power * player[player_num].strength * (xp_random(5) + 1) * (xp_random(5) + 1) * vary) /
	    ((opp.armour) * opp.luck * vary2));
}

double
opponentattack(void)
{
    return ((opp.attack * opp.strength * opp.dexterity * (xp_random(5) + 1) * vary2) /
	    (player[player_num].armour * player[player_num].dexterity * player[player_num].luck * vary));
}

double
opponentattack2(void)
{
    return ((opp.power * opp.strength * (xp_random(5) + 1) * (xp_random(5) + 1) * vary2) /
	    ((player[player_num].armour) * player[player_num].luck * vary));
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
    tempbank = player[player_num].gold;
    player[player_num].gold = player[player_num].gold - tempbank;
    player[player_num].bank = player[player_num].bank + tempbank;
    nl();
    od_set_color(L_CYAN, D_BLACK);
    od_printf("%" QWORDFORMAT " Steel pieces are in the bank.\r\n", player[player_num].bank);
}

void
withdrawbank(void)
{
    QWORD           tempbank;
    tempbank = player[player_num].bank;
    player[player_num].gold = player[player_num].gold + tempbank;
    player[player_num].bank = player[player_num].bank - tempbank;
    nl();
    od_set_color(D_MAGENTA, D_BLACK);
    od_printf("You are now carrying %" QWORDFORMAT " Steel pieces.\r\n", player[player_num].gold);
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
	    player[i].flights = FLIGHTS_PER_DAY;
	    player[i].status = ALIVE;
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
	fprintf(outfile, "%u %u %u %u %u %u %" QWORDFORMAT " %u %u %u %u %" QWORDFORMAT " %u %" QWORDFORMAT " %" QWORDFORMAT " %" QWORDFORMAT " %u\n",
		player[a].strength, player[a].intelligence,
		player[a].luck, player[a].dexterity, player[a].constitution,
		player[a].charisma, player[a].experience,
		player[a].r, player[a].hps, player[a].weapon,
		player[a].armour, player[a].gold,
	 player[a].flights, player[a].bank, player[a].wins, player[a].loses,
		player[a].plus);
    }
    fclose(outfile);
}

void
heal(void)
{
    WORD            opt;
    od_clr_scr();
    od_set_color(D_MAGENTA, D_BLACK);
    od_printf("Clerics want %u Steel pieces per wound.\r\n", 8 * player[player_num].r);
    od_set_color(L_YELLOW, D_BLACK);
    od_printf("You have %u points of damage to heal.\r\n", player[player_num].damage);
    od_set_color(L_CYAN, D_BLACK);
    od_disp_str("How many do points do you want healed? ");
    od_input_str(temp, 3, '0', '9');
    opt = atoi(temp);
    if (!temp[0])
	opt = player[player_num].damage;
    if (((opt) * (player[player_num].r) * 10) > player[player_num].gold) {
	od_set_color(L_RED, B_BLACK);
	od_disp_str("Sorry, you do not have enough Steel.\r\n");
	opt = 0;
    } else if (opt > player[player_num].damage)
	opt = player[player_num].damage;
    player[player_num].damage = player[player_num].damage - opt;
    player[player_num].gold = player[player_num].gold - 8 * opt * player[player_num].r;
    od_printf("%u hit points healed.\r\n", opt);
}

void
findo(void)
{
    nl();
    od_set_color(L_YELLOW, D_BLACK);
    od_disp_str("The Vile Enemy Dropped Something!\r\n");
    od_disp_str("Do you want to get it? ");
    if (od_get_answer("YN") == 'Y') {
	od_disp_str("Yes\r\n");
	okea = xp_random(99) + 1;
	if ((okea < 10) && (player[player_num].weapon >= 25)) {
	    player[player_num].weapon++;
	    od_printf("You have found a %s.\r\n", wname[player[player_num].weapon]);
	}
	if ((okea > 11) && (okea < 40)) {
	    player[player_num].gold = player[player_num].gold + 40;
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
    if (!doga)
	opp.gold = opp.gold * supplant();
    nl();
    od_printf("You take his %" QWORDFORMAT " Steel pieces.\r\n", opp.gold);
    player[player_num].gold = player[player_num].gold + opp.gold;
    if (doga) {
	int             i;
	i = curr_opp;
	nl();
	od_set_color(D_GREEN, D_BLACK);
	od_disp_str("The Last Words He Utters Are...\r\n");
	nl();
	od_printf("\"%s\"\r\n", player[i].gaspd);
	nl();
	player[player_num].wins++;
	player[i].loses++;
	SAFECOPY(player[i].killer, player[player_num].name);
	player[i].status = DEAD;
	player[i].gold = 0;
	if (player[i].weapon > player[player_num].weapon) {
	    d = player[player_num].weapon;
	    player[player_num].weapon = player[i].weapon;
	    player[i].weapon = d;
	    bt = player[player_num].plus;
	    player[player_num].plus = player[i].plus;
	    player[i].plus = bt;
	    od_set_color(D_GREEN, D_BLACK);
	    od_disp_str("You Hath Taken His Weapon.\r\n");
	}
	if (player[i].armour > player[player_num].armour) {
	    d = player[player_num].armour;
	    player[player_num].armour = player[i].armour;
	    player[i].armour = d;
	    od_set_color(L_YELLOW, D_BLACK);
	    od_disp_str("You Hath Taken His Armour.\r\n");
	}
	player[player_num].attack = w2[player[player_num].weapon];
	player[player_num].power = w3[player[player_num].weapon];
	outfile = fopen("data/record.lan", "ab");
	fprintf(outfile, "%s conquered %s\r\n", player[player_num].pseudo, player[i].pseudo);
	fclose(outfile);
    }
    gain_exp *= supplant();
    player[player_num].experience += gain_exp;
    od_printf("You obtain %" QWORDFORMAT " exp points.\r\n", gain_exp);
    doga = FALSE;
}

void
levelupdate(void)
{
    int             x;
    if (player[player_num].experience > required[player[player_num].r + 1]) {
	player[player_num].r++;
	od_set_color(L_YELLOW, D_BLACK);
	od_printf("Welcome to level %u!\r\n", player[player_num].r);
	x = xp_random(6) + 1;
	switch (x) {
	    case 1:
		player[player_num].strength++;
		break;
	    case 2:
		player[player_num].intelligence++;
		break;
	    case 3:
		player[player_num].luck++;
		break;
	    case 4:
		player[player_num].dexterity++;
		break;
	    case 5:
		player[player_num].constitution++;
		break;
	    case 6:
		player[player_num].charisma++;
		break;
	}
	player[player_num].hps = player[player_num].hps + (xp_random(5) + 1) + (player[player_num].constitution / 4);
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
	if (roll > 5 * player[player_num].power)
	    roll = 5 * player[player_num].power;
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
	    live = FALSE;
	}
    }
}

void
bmode(void)
{
    if ((opp.hps > opp.damage) && live) {
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
	    player[player_num].damage = player[player_num].damage + roll;
	    if (player[player_num].damage >= player[player_num].hps) {
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
		if (doga) {
		    player[curr_opp].wins++;
		    player[player_num].loses++;
		    player[curr_opp].gold += player[player_num].gold;
		    player[player_num].gold = 0;
		    outfile = fopen("data/record.lan", "ab");
		    fprintf(outfile, "%s killed %s\r\n", player[player_num].name, player[curr_opp].name);
		    fclose(outfile);
		}
		live = FALSE;
		leave();
		play = FALSE;
	    }
	}
    }
}

void
statshow(void)
{
    od_clr_scr();

    od_set_color(D_MAGENTA, D_BLACK);
    od_printf("Name: %s   Level: %d\r\n", player[player_num].pseudo, player[player_num].r);
    od_set_color(L_CYAN, D_BLACK);
    od_printf("W/L: %" QWORDFORMAT "/%" QWORDFORMAT "   Exp: %" QWORDFORMAT "\r\n", player[player_num].wins, player[player_num].loses, player[player_num].experience);
    nl();
    od_set_color(L_YELLOW, D_BLACK);
    od_printf("Steel  (in hand): %" QWORDFORMAT "\r\n", player[player_num].gold);
    od_printf("Steel  (in bank): %" QWORDFORMAT "\r\n", player[player_num].bank);
    nl();
    od_set_color(L_BLUE, D_BLACK);
    od_printf("Battles: %u   Retreats: %u    Fights: %u   Hps: %u(%u)\r\n", BATTLES_PER_DAY - dog, player[player_num].flights, FIGHTS_PER_DAY - trips, player[player_num].hps - player[player_num].damage, player[player_num].hps);
    nl();
    od_set_color(L_CYAN, D_BLACK);
    od_printf("Weapon: %s     Armor: %s\r\n", wname[player[player_num].weapon], sname[player[player_num].armour]);
}

void
incre(void)
{
    od_disp_str("Increase which stat? ");
    switch (od_get_answer("123456Q")) {
	case '1':
	    od_disp_str("Strength\r\n");
	    player[player_num].strength++;
	    break;
	case '2':
	    od_disp_str("Intelligence\r\n");
	    player[player_num].intelligence++;
	    break;
	case '3':
	    od_disp_str("Dexterity\r\n");
	    player[player_num].dexterity++;
	    break;
	case '4':
	    od_disp_str("Luck\r\n");
	    player[player_num].luck++;
	    break;
	case '5':
	    od_disp_str("Constitution\r\n");
	    player[player_num].constitution++;
	    break;
	case '6':
	    od_disp_str("Charisma\r\n");
	    player[player_num].charisma++;
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
	    player[player_num].strength -= 2;
	    break;
	case '2':
	    od_disp_str("Intelligence\r\n");
	    player[player_num].intelligence -= 2;
	    break;
	case '3':
	    od_disp_str("Dexterity\r\n");
	    player[player_num].dexterity -= 2;
	    break;
	case '4':
	    od_disp_str("Luck\r\n");
	    player[player_num].luck -= 2;
	    break;
	case '5':
	    od_disp_str("Constitution\r\n");
	    player[player_num].constitution -= 2;
	    break;
	case '6':
	    od_disp_str("Charisma\r\n");
	    player[player_num].charisma -= 2;
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
    od_printf("1> Str: %u\r\n", player[player_num].strength);
    od_printf("2> Int: %u\r\n", player[player_num].intelligence);
    od_printf("3> Dex: %u\r\n", player[player_num].dexterity);
    od_printf("4> Luk: %u\r\n", player[player_num].luck);
    od_printf("5> Con: %u\r\n", player[player_num].constitution);
    od_printf("6> Chr: %u\r\n", player[player_num].charisma);
    nl();
    if (player[player_num].strength < 6) {
	yaya = FALSE;
	nl();
	od_disp_str("Strength cannot go below 6\r\n");
    }
    if (player[player_num].intelligence < 6) {
	yaya = FALSE;
	nl();
	od_disp_str("Intelligence cannot go below 6\r\n");
    }
    if (player[player_num].dexterity < 6) {
	yaya = FALSE;
	nl();
	od_disp_str("Dexterity cannot go below 6\r\n");
    }
    if (player[player_num].luck < 6) {
	yaya = FALSE;
	nl();
	od_disp_str("Luck cannot go below 6\r\n");
    }
    if (player[player_num].constitution < 6) {
	yaya = FALSE;
	nl();
	od_disp_str("Constitution cannot go below 6\r\n");
    }
    if (player[player_num].charisma < 6) {
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
	    player[player_num].strength = temp1a;
	    player[player_num].intelligence = temp1b;
	    player[player_num].dexterity = temp1d;
	    player[player_num].luck = temp1c;
	    player[player_num].constitution = temp1e;
	    player[player_num].charisma = temp1f;
	    bothover = FALSE;
	    bothover = FALSE;
	}
    } else {
	player[player_num].strength = temp1a;
	player[player_num].intelligence = temp1b;
	player[player_num].dexterity = temp1d;
	player[player_num].luck = temp1c;
	player[player_num].constitution = temp1e;
	player[player_num].charisma = temp1f;
	bothover = FALSE;
    }
}

void
chstats(void)
{
    od_clr_scr();
    bothover = FALSE;
    temp1a = player[player_num].strength;
    temp1b = player[player_num].intelligence;
    temp1c = player[player_num].luck;
    temp1d = player[player_num].dexterity;
    temp1e = player[player_num].constitution;
    temp1f = player[player_num].charisma;

    for (;;) {
	partone = TRUE;
	od_disp_str("Status Change:\r\n");
	od_disp_str("@@@@@@@@@@@@@\r\n");
	od_disp_str("You may increase any stat by one,\r\n");
	od_disp_str("yet you must decrease another by two.\r\n");
	nl();
	od_printf("1> Str: %u\r\n", player[player_num].strength);
	od_printf("2> Int: %u\r\n", player[player_num].intelligence);
	od_printf("3> Dex: %u\r\n", player[player_num].dexterity);
	od_printf("4> Luk: %u\r\n", player[player_num].luck);
	od_printf("5> Con: %u\r\n", player[player_num].constitution);
	od_printf("6> Chr: %u\r\n", player[player_num].charisma);
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
    if (opp.dexterity > player[player_num].dexterity) {
	if (play)
	    bmode();
	if (play)
	    amode();
    } else if (opp.dexterity < player[player_num].dexterity) {
	if (play)
	    amode();
	if (play)
	    bmode();
    } else {
	if (play)
	    amode();
	if (play)
	    bmode();
    }
}

QWORD
readnumb(void)
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
    buf[pos++] = 0;
    ret = strtoll(buf, NULL, 10);
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
    trips++;
    rd = xp_random(readnumb() - 1) + 1;
    endofline();
    for (a = 1; a <= rd; a++) {
	readline(opp.name, sizeof(opp.name));
	opp.hps = readnumb();
	opp.damage = 0;
	opp.attack = readnumb();
	opp.power = readnumb();
	opp.armour = readnumb();
	opp.luck = readnumb();
	opp.strength = readnumb();
	opp.dexterity = readnumb();
	opp.gold = readnumb();
	gain_exp = readnumb();
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
    if (dog <= BATTLES_PER_DAY) {
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
			if (player[a].r > player[player_num].r - 4) {
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
		    enemy = atoi(tmphh);
		    if ((enemy == 0) || (!strcmp(player[enemy].pseudo, player[player_num].pseudo)) || (player[enemy].status == DEAD))
			return;
		}
	    }
	    for (a = 1; a <= number_of_players; a++) {
		if (enemy == a) {
		    if (player[a].r > (player[player_num].r - 4)) {
			finder = TRUE;
			curr_opp = a;
		    }
		}
	    }
	}
	SAFECOPY(opp.name, player[curr_opp].pseudo);
	opp.hps = player[curr_opp].hps;
	opp.damage = player[curr_opp].hps;
	vary2 = supplant();
	doga = TRUE;
	dog++;
	opp.attack = w2[player[curr_opp].weapon];
	opp.power = w3[player[curr_opp].weapon];
	opp.armour = player[curr_opp].armour;
	opp.luck = player[curr_opp].luck;
	opp.strength = player[curr_opp].strength;
	opp.dexterity = player[curr_opp].dexterity;
	opp.gold = player[curr_opp].gold;
	gain_exp = player[curr_opp].experience / 10;
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
    live = TRUE;
    while (live == TRUE) {
	playerrem = player[player_num].hps - player[player_num].damage;
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
		if ((xp_random(4) + 1) + player[player_num].dexterity > opp.dexterity) {
		    nl();
		    od_set_color(D_GREEN, D_BLACK);
		    od_disp_str("You Ride away on a Silver Dragon.\r\n");
		    doga = FALSE;
		    live = FALSE;
		    uni = FALSE;
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
readlist(void)
{
    int             i;
    infile = fopen("data/characte.lan", "rb");
    readline(temp, sizeof(temp));
    number_of_players = atoi(temp);
    for (i = 1; i <= number_of_players; i++) {
	readline(player[i].name, sizeof(player[i].name));
	readline(player[i].pseudo, sizeof(player[i].pseudo));
	readline(player[i].gaspd, sizeof(player[i].gaspd));
	fgets(temp, sizeof(temp), infile);
	player[i].status = atoi(temp);
	if (player[i].status == DEAD)
	    readline(player[i].killer, sizeof(player[i].killer));
	player[i].strength = readnumb();
	player[i].intelligence = readnumb();
	player[i].luck = readnumb();
	player[i].dexterity = readnumb();
	player[i].constitution = readnumb();
	player[i].charisma = readnumb();
	player[i].experience = readnumb();
	player[i].r = readnumb();
	player[i].hps = readnumb();
	player[i].weapon = readnumb();
	player[i].armour = readnumb();
	player[i].gold = readnumb();
	player[i].flights = readnumb();
	player[i].bank = readnumb();
	player[i].wins = readnumb();
	player[i].loses = readnumb();
	player[i].plus = readnumb();
	endofline();
    }
    fclose(infile);
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
		buy = atoi(temp);
		if (buy == 0)
		    return;
		if (cost[buy] > player[player_num].gold)
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
				player[player_num].gold -= cost[buy];
				player[player_num].weapon = buy;
				nl();
				od_set_color(D_MAGENTA, D_BLACK);
				od_printf("You've bought a %s\r\n", wname[buy]);
				player[player_num].attack = w2[player[player_num].weapon];
				player[player_num].power = w3[player[player_num].weapon];
			    } else
				od_disp_str("No\r\n");
			    break;
			case 'A':
			    od_disp_str("Armour\r\n");
			    od_disp_str("Are you sure you want buy it? ");
			    if (od_get_answer("YN") == 'Y') {
				od_disp_str("Yes\r\n");
				player[player_num].gold -= cost[buy];
				player[player_num].armour = buy;
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
			buyprice = player[player_num].charisma;
			buyprice = buyprice * cost[player[player_num].weapon];
			buyprice = (buyprice / 20);
			nl();
			od_printf("I will purchase it for %u, okay? ", buyprice);
			if (od_get_answer("YN") == 'Y') {
			    od_disp_str("Yes\r\n");
			    od_set_color(D_GREEN, D_BLACK);
			    od_disp_str("Is it Dwarven Made?\r\n");
			    player[player_num].weapon = 1;
			    player[player_num].gold = player[player_num].gold + buyprice;
			} else
			    od_disp_str("No\r\n");
			break;
		    case 'A':
			od_disp_str("Armour\r\n");
			buyprice = player[player_num].charisma * cost[player[player_num].armour] / 20;
			nl();
			od_printf("I will purchase it for %u, okay? ", buyprice);
			if (od_get_answer("YN") == 'Y') {
			    od_disp_str("Yes\r\n");
			    od_disp_str("Fine Craftsmanship!\r\n");
			    player[player_num].armour = 1;
			    player[player_num].gold = player[player_num].gold + buyprice;
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
	    if (player[player_num].gold < 20) {
		od_set_color(L_RED, B_BLACK);
		od_disp_str("You do not have enough Steel!\r\n");
	    } else {
		player[player_num].gold -= 20;
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
    nl();
    if (trips >= FIGHTS_PER_DAY)
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
	realgold = strtoll(tempgd, NULL, 10);
	if (realgold > player[player_num].gold) {
	    nl();
	    od_disp_str("You do not have enough Steel!\r\n");
	}
	if ((realgold != 0) && ((player[player_num].gold >= realgold) && (realgold <= 25000) && (realgold >= 1))) {
	    okea = xp_random(99) + 1;
	    if (okea <= 3) {
		realgold *= 100;
		player[player_num].gold += realgold;
		od_printf("You shot all the targets and win %u Steel pieces!\r\n", realgold);
	    } else if ((okea > 3) && (okea <= 15)) {
		realgold *= 10;
		player[player_num].gold += realgold;
		od_printf("You shot 50%% if the targets and win %u Steel pieces!\r\n", realgold);
	    } else if ((okea > 15) && (okea <= 30)) {
		realgold *= 3;
		player[player_num].gold += realgold;
		od_printf("You shot 25%% if the targets and win %u Steel pieces!\r\n", realgold);
	    } else {
		player[player_num].gold -= realgold;
		od_disp_str("Sorry You Hath Lost!\r\n");
	    }
	}
    }
}

void
afight(int lev)
{
    char            fname[32];
    uni = TRUE;
    while (uni) {
	if (trips >= FIGHTS_PER_DAY) {
	    nl();
	    od_set_color(D_MAGENTA, D_BLACK);
	    od_disp_str("It's Getting Dark Out!\r\n");
	    od_disp_str("Return to the Nearest Inn!\r\n");
	    uni = FALSE;
	} else {
	    od_clr_scr();
	    sprintf(fname, "data/junkm%d.lan", lev);
	    fight(fname);
	    uni = FALSE;
	}
    }
}

void
bulletin(void)
{
    BOOL            endfil;
    int             countr;
    char            tempcoun[3];
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
    if (trips >= FIGHTS_PER_DAY)
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
	    if (player[player_num].gold < (tttgld * 100))
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
		    player[player_num].gold -= tttgld * 100;
		    switch (realtrain) {
			case 1:
			    player[player_num].strength++;
			    break;
			case 2:
			    player[player_num].intelligence++;
			    break;
			case 3:
			    player[player_num].dexterity++;
			    break;
			case 4:
			    player[player_num].luck++;
			    break;
			case 5:
			    player[player_num].constitution++;
			    break;
			case 6:
			    player[player_num].charisma++;
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
    vary = supplant();
    dog = 0;
    doga = FALSE;
    for (i = 1; i <= 30; i++)
	player[i].damage = 0;
    play = TRUE;
    trips = 0;
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
    number_of_players = atoi(temp);
    for (i = 1; i <= number_of_players; i++) {
	readline(player[i].name, sizeof(player[i].name));
	readline(player[i].pseudo, sizeof(player[i].pseudo));
	readline(player[i].gaspd, sizeof(player[i].gaspd));
	fgets(temp, sizeof(temp), infile);
	player[i].status = atoi(temp);
	if (player[i].status == DEAD)
	    readline(player[i].killer, sizeof(player[i].killer));
	player[i].strength = readnumb();
	player[i].intelligence = readnumb();
	player[i].luck = readnumb();
	player[i].dexterity = readnumb();
	player[i].constitution = readnumb();
	player[i].charisma = readnumb();
	player[i].experience = readnumb();
	player[i].r = readnumb();
	player[i].hps = readnumb();
	player[i].weapon = readnumb();
	player[i].armour = readnumb();
	player[i].gold = readnumb();
	player[i].flights = readnumb();
	player[i].bank = readnumb();
	player[i].wins = readnumb();
	player[i].loses = readnumb();
	player[i].plus = readnumb();
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
    if (player[player_num].status == DEAD) {
	nl();
	od_set_color(L_CYAN, D_BLACK);
	od_printf("A defeat was lead over you by %s.", player[player_num].killer);
    }
    checkday();
    if (player[player_num].flights < 1) {
	trips = FIGHTS_PER_DAY;
	dog = BATTLES_PER_DAY;
    } else
	player[player_num].flights--;
    nl();
    pausescr();
    od_clr_scr();
    infile = fopen("data/weapons.lan", "rb");
    od_set_color(L_BLUE, D_BLACK);
    for (i = 1; i <= 25; i++) {
	readline(wname[i], sizeof(wname[i]));
	w2[i] = readnumb();
	w3[i] = readnumb();
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
	cost[i] = readnumb();
	endofline();
    }
    fclose(infile);
    player[player_num].attack = w2[player[player_num].weapon];
    player[player_num].power = w3[player[player_num].weapon];
    infile = fopen("data/experience.lan", "rb");
    for (i = 1; i <= 28; i++) {
	required[i] = readnumb();
	endofline();
    }
    fclose(infile);
    od_set_color(L_YELLOW, D_BLACK);
    player[player_num].status = ALIVE;
    vary2 = 1;
    statshow();
    for (play = TRUE; play;) {
	levelupdate();
	vary2 = 1;
	if (((player[player_num].wins + 1) * 4) < (player[player_num].loses)) {
	    nl();
	    od_disp_str("As you were Travelling along a Wilderness Path an   \r\n");
	    od_disp_str("Evil Wizard Confronted You.  When you tried to fight\r\n");
	    od_disp_str("Him off, he cast a Spell of Instant Death Upon you.  \r\n");
	    od_disp_str("Instantly you were slain by the Archmage, Leaving you\r\n");
	    od_disp_str("as nothing more than a pile of ashes.  Re-Rolled!   \r\n");
	    nl();
	    pausescr();
	    create(FALSE);
	    if (player[player_num].flights)
		player[player_num].flights--;
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
		    play = FALSE;
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
		    if (player[player_num].flights)
			player[player_num].flights--;
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
			SAFECOPY(player[player_num].pseudo, temp);
		} else
		    od_disp_str("No\r\n");
		break;
	    case '#':
		od_disp_str("Change Battle Cry\r\n");
		vic();
		SAFECOPY(player[player_num].gaspd, temp);
		break;
	}
    }
    return (0);
}
