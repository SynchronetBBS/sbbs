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

#define DEAD	1<<0
#define ONLINE	1<<1
/* Status for opponent != a player */

enum {
    MONSTER
    ,PLAYER
};

enum {
    HE
    ,HIS
    ,HIM
};

struct playertype {
    char            name[31];	       /* Name from BBS drop file */
    char            pseudo[31];	       /* In-game pseudonym */
    char            killer[31];	       /* Person killed by (if status==DEAD) */
    char            bcry[61];	       /* Battle Cry */
    char            winmsg[61];	       /* What you say when you win a battle */
    char            gaspd[61];	       /* Dying curse */
    char            laston[11];	       /* Last date one */
    char            sex;	       /* Characters sex */
    DWORD           status;	       /* Status alive or dead for player
				        * records, PLAYER or MONSTER for
				        * opponent record (record 0) */
    /* ToDo make configurable */
    WORD            flights;	       /* Weird thing... may be number of
				        * times game can be ran per day */
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


struct weapon {
    char            name[31];
    WORD            attack;
    WORD            power;
    DWORD           cost;
};


struct armour {
    char            name[31];
    DWORD           cost;
};


struct playertype player[3];
/* player[1] is the current user */
#define user	player[1]
/* player[0] is the current enemy/temp player */
#define opp	player[0]
/* player[2] is the temporary user */
#define tmpuser player[2]

const char      dots[] = "...............................................";
/* In these arrays, 0 isn't actually used */
QWORD           required[29];	       /* Experience required for each level */
struct weapon   weapon[26];
struct armour   armour[26];
char            temp[81];
FILE           *infile;		       /* Current input file */
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

void            quickexit(void);
BOOL            loadnextuser(struct playertype * plr);
BOOL            loaduser(char *name, BOOL bbsname, struct playertype * plr);
void            writenextuser(struct playertype * plr, FILE * outfile);

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
    DWORD           h, i, j;
    FILE           *outfile;
    BOOL            newday = FALSE;
    struct playertype **playerlist = NULL;
    struct playertype **ra;
    infile = fopen("data/date.lan", "r+b");
    readline(oldy, sizeof(oldy));

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
    while (loadnextuser(&tmpuser)) {
	ra = (struct playertype **) realloc(playerlist, sizeof(struct playertype *) * (i + 1));
	if (ra == NULL)
	    quickexit();
	playerlist = ra;
	playerlist[i] = (struct playertype *) malloc(sizeof(struct playertype));
	if (playerlist[i] == NULL)
	    quickexit();
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
    fprintf(outfile, "%u\n", i);
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
    while (loadnextuser(&tmpuser)) {
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
writenextuser(struct playertype * plr, FILE * outfile)
{
    fprintf(outfile, "%s\n", plr->name);
    fprintf(outfile, "%s\n", plr->pseudo);
    fprintf(outfile, "%c\n", plr->sex);
    fprintf(outfile, "%s\n", plr->gaspd);
    fprintf(outfile, "%s\n", plr->laston);
    fprintf(outfile, "%u\n", plr->status);
    if (plr->status == DEAD)
	fprintf(outfile, "%s\n", plr->killer);
    fprintf(outfile, "%u %u %u %u %u %u %" QWORDFORMAT " %u %u %u %u %" QWORDFORMAT " %u %" QWORDFORMAT " %" QWORDFORMAT " %" QWORDFORMAT " %u %u %u %1.4f\n",
	    plr->strength, plr->intelligence,
	    plr->luck, plr->dexterity, plr->constitution,
	    plr->charisma, plr->experience,
	    plr->r, plr->hps, plr->weapon,
	    plr->armour, plr->gold,
	    plr->flights, plr->bank, plr->wins, plr->loses,
	    plr->plus, plr->fights, plr->battles, plr->vary);
}

void
saveuser(struct playertype * plr)
{
    DWORD           h, i, j;
    FILE           *outfile;
    struct playertype **playerlist = NULL;
    struct playertype **ra;
    BOOL            saved = FALSE;
    infile = fopen("data/characte.lan", "rb");
    fgets(temp, sizeof(temp), infile);
    i = 0;
    while (loadnextuser(&tmpuser)) {
	ra = (struct playertype **) realloc(playerlist, sizeof(plr) * (i + 1));
	if (ra == NULL)
	    quickexit();
	playerlist = ra;
	playerlist[i] = (struct playertype *) malloc(sizeof(struct playertype));
	if (playerlist[i] == NULL)
	    quickexit();
	if (!stricmp(tmpuser.name, plr->name)) {
	    *playerlist[i] = *plr;
	    saved = TRUE;
	} else
	    *playerlist[i] = tmpuser;
	i++;
    }
    fclose(infile);

    if (!saved) {
	ra = (struct playertype **) realloc(playerlist, sizeof(plr) * (i + 1));
	if (ra == NULL)
	    quickexit();
	playerlist = ra;
	playerlist[i] = (struct playertype *) malloc(sizeof(struct playertype));
	if (playerlist[i] == NULL)
	    quickexit();
	*playerlist[i] = *plr;
	i++;
    }
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
    fprintf(outfile, "%u\n", i);
    for (j = 0; j < i; j++) {
	writenextuser(playerlist[j], outfile);
	free(playerlist[j]);
    }
    free(playerlist);
    fclose(outfile);
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
	if ((okea < 10) && (user.weapon <= 25)) {
	    user.weapon++;
	    od_printf("You have found a %s.\r\n", weapon[user.weapon].name);
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

char           *
sexstrings(struct playertype * plr, int type, BOOL cap)
{
    static char     retbuf[16];
    char            sex;
    sex = plr->sex;
    if (plr->charisma < 8)
	sex = 'I';
    switch (sex) {
	case 'M':
	    switch (type) {
		case HIS:
		    SAFECOPY(retbuf, "his");
		    break;
		case HE:
		    SAFECOPY(retbuf, "he");
		    break;
		case HIM:
		    SAFECOPY(retbuf, "him");
		    break;
	    }
	case 'F':
	    switch (type) {
		case HIS:
		    SAFECOPY(retbuf, "her");
		    break;
		case HE:
		    SAFECOPY(retbuf, "she");
		    break;
		case HIM:
		    SAFECOPY(retbuf, "her");
		    break;
	    }
	case 'I':
	    switch (type) {
		case HIS:
		    SAFECOPY(retbuf, "its");
		    break;
		case HE:
		    SAFECOPY(retbuf, "it");
		    break;
		case HIM:
		    SAFECOPY(retbuf, "it");
		    break;
	    }
    }
    if (cap)
	retbuf[0] = toupper(retbuf[0]);
    return (retbuf);
}

void
endofbattle(BOOL won)
{
    int             bt;
    int             d;
    FILE           *outfile;
    char            lstr[5];
    char            wstr[5];
    struct playertype *winner;
    struct playertype *loser;
    if (opp.status != MONSTER)
	loaduser(opp.name, TRUE, &tmpuser);
    if (won) {
	winner = &user;
	loser = &tmpuser;
	SAFECOPY(lstr, sexstrings(loser, HIS, FALSE));
	SAFECOPY(wstr, "You");
    } else {
	winner = &tmpuser;
	loser = &user;
	SAFECOPY(lstr, sexstrings(winner, HIS, FALSE));
	SAFECOPY(lstr, "your");
	opp.experience = user.experience / 10;
    }

    if (opp.status == MONSTER)
	opp.gold = opp.gold * supplant();
    nl();
    if (won)
	od_printf("You take %s %" QWORDFORMAT " Steel pieces.\r\n", sexstrings(&opp, HIS, FALSE), opp.gold);
    else {
	if (opp.status != MONSTER)
	    od_printf("%s takes your %" QWORDFORMAT " Steel pieces.\r\n", sexstrings(&opp, HE, TRUE), opp.gold);
    }
    winner->gold += opp.gold;
    loser->gold = 0;

    if (opp.status != MONSTER) {
	nl();
	od_set_color(D_GREEN, D_BLACK);
	if (won) {
	    od_printf("The Last Words %s Utters Are...\r\n", sexstrings(&tmpuser, HE, FALSE));
	    nl();
	    od_printf("\"%s\"\r\n", tmpuser.gaspd);
	    nl();
	} else {
	    od_disp_str("The Last Words You Hear Are...\r\n");
	    nl();
	    od_printf("\"%s\"\r\n", tmpuser.winmsg);
	    nl();
	}
	winner->wins++;
	loser->loses++;
	SAFECOPY(loser->killer, winner->pseudo);
	loser->status = DEAD;
	if (loser->weapon > winner->weapon) {
	    d = winner->weapon;
	    winner->weapon = loser->weapon;
	    loser->weapon = d;
	    bt = winner->plus;
	    winner->plus = loser->plus;
	    loser->plus = bt;
	    od_set_color(D_GREEN, D_BLACK);
	    od_printf("%s Hath Taken %s Weapon.\r\n", wstr, lstr);
	}
	if (tmpuser.armour > user.armour) {
	    d = winner->armour;
	    winner->armour = loser->armour;
	    loser->armour = d;
	    od_set_color(L_YELLOW, D_BLACK);
	    od_printf("%s Hath Taken %s Armour.\r\n", wstr, lstr);
	}
	user.attack = weapon[user.weapon].attack;
	user.power = weapon[user.weapon].power;
	tmpuser.attack = weapon[tmpuser.weapon].attack;
	tmpuser.power = weapon[tmpuser.weapon].power;
	outfile = fopen("data/record.lan", "ab");
	fprintf(outfile, "%s attacked %s and %s was victorious!\r\n", user.pseudo, tmpuser.pseudo, loser->pseudo);
	fclose(outfile);
    }
    opp.experience *= supplant();
    winner->experience += opp.experience;
    if (won || opp.status != MONSTER)
	od_printf("%s obtain %" QWORDFORMAT " exp points.\r\n", wstr, opp.experience);

    if (opp.status != MONSTER)
	saveuser(&tmpuser);
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
		od_printf("HA HA! %s dodges your swing!\r\n", sexstrings(&opp, HE, FALSE));
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
		od_printf("You sliced %s for %1.0f.\r\n", sexstrings(&opp, HIM, FALSE), roll);
		break;
	    case 2:
		od_printf("You made contact to %s body for %1.0f.\r\n", sexstrings(&opp, HIS, FALSE), roll);
		break;
	    case 3:
		od_printf("You hacked %s for %1.0f.", sexstrings(&opp, HIM, FALSE), roll);
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
		    od_printf("%s Hath died!\r\n", sexstrings(&opp, HE, TRUE));
		    break;
		case 3:
		    od_disp_str("A Smooth killing!\r\n");
		    break;
		case 4:
		    od_printf("%s has gone to the Abyss!\r\n", sexstrings(&opp, HE, TRUE));
		    break;
	    }
	    okea = xp_random(99) + 1;
	    if (okea < 30)
		findo();
	    endofbattle(TRUE);
	}
    }
}

void
bmode(void)
{
    double          roll;	       /* Used for To Hit and damage rolls */
    int             tint;
    if ((opp.hps > opp.damage) && user.damage < user.hps) {
	roll = opponentattack();
	if (roll < 1.5) {
	    od_set_color(D_GREEN, D_BLACK);
	    od_printf("%s attack tears your armour.\r\n", sexstrings(&opp, HIS, TRUE));
	} else {
	    roll = opponentattack2();
	    if (roll > 5 * opp.power)
		roll = 5 * opp.power;
	    if (roll < 1)
		roll = 1;
	    tint = xp_random(2) + 1;
	    switch (tint) {
		case 1:
		    od_printf("%s hammered you for %1.0f.\r\n", sexstrings(&opp, HE, TRUE), roll);
		    break;
		case 2:
		    od_printf("%s swung and hit for %1.0f.\r\n", sexstrings(&opp, HE, TRUE), roll);
		    break;
		case 3:
		    od_printf("You are surprised when %s hits you for %1.0f.\r\n", sexstrings(&opp, HE, FALSE), roll);
		    break;
	    }
	    user.damage += roll;
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
		endofbattle(FALSE);
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
    od_printf("Weapon: %s     Armor: %s\r\n", weapon[user.weapon].name, armour[user.armour].name);
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
	readline(opp.pseudo, sizeof(opp.pseudo));
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
	opp.sex = 'I';
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
    int             a, i;
    DWORD           enemy;
    BOOL            finder;
    if (user.battles > 0) {
	nl();
	infile = fopen("data/characte.lan", "rb");
	for (finder = FALSE; finder == FALSE;) {
	    od_clr_scr();
	    od_set_color(L_YELLOW, D_BLACK);
	    od_disp_str("Battle Another Hero:\r\n");
	    od_disp_str("********************\r\n");
	    i = 3;
	    a = 1;
	    fseek(infile, 0, SEEK_SET);
	    fgets(temp, sizeof(temp), infile);
	    while (loadnextuser(&opp)) {
		if (opp.r > user.r - 4) {
		    nl();
		    od_set_color(D_MAGENTA, D_BLACK);
		    od_printf("%2u.  `bright cyan`%.30s%.*s`bright blue`Lev=%-2u  W=%-2" QWORDFORMAT "  L=%-2" QWORDFORMAT "  S=%s `blinking red`%s"
			      ,a++, opp.pseudo, 30 - strlen(opp.pseudo), dots
			      ,opp.r, opp.wins, opp.loses
			      ,((opp.status & DEAD) ? "SLAIN" : "ALIVE"), ((opp.status & ONLINE) ? "ONLINE" : ""));
		    if (++i >= od_control.user_screen_length)
			pausescr();
		    a += 1;
		}
	    }
	    nl();
	    od_set_color(L_CYAN, D_BLACK);
	    od_disp_str("Enter the # of your opponent: ");
	    od_input_str(tmphh, 2, '0', '9');
	    enemy = strtoul(tmphh, NULL, 10);
	    if (enemy < 1 || enemy >= a)
		continue;
	    fseek(infile, 0, SEEK_SET);
	    fgets(temp, sizeof(temp), infile);
	    for (a = 1; a <= enemy;) {
		if (!loadnextuser(&opp))
		    opp.status = DEAD;
		else if (opp.r > user.r - 4)
		    a++;
	    }
	    if ((!stricmp(opp.pseudo, user.pseudo)) || (opp.status == DEAD)) {
		fclose(infile);
		return;
	    }
	    finder = TRUE;
	}
	fclose(infile);
	opp.status = PLAYER;
	user.battles--;
	opp.experience /= 10;
	nl();
	od_disp_str("Your opponent screams out:\r\n");
	od_printf("\"%s\" as battle is joined.\r\n", opp.bcry);
	nl();
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
	od_printf("You are attacked by a %s.\r\n", opp.pseudo);
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
BOOL
loadnextuser(struct playertype * plr)
{
    if (feof(infile))
	return (FALSE);
    readline(plr->name, sizeof(plr->name));
    if (feof(infile))
	return (FALSE);
    readline(plr->pseudo, sizeof(plr->pseudo));
    if (feof(infile))
	return (FALSE);
    fread(&plr->sex, 1, 1, infile);
    endofline();
    if (feof(infile))
	return (FALSE);
    readline(plr->gaspd, sizeof(plr->gaspd));
    if (feof(infile))
	return (FALSE);
    readline(plr->laston, sizeof(plr->laston));
    if (feof(infile))
	return (FALSE);
    fgets(temp, sizeof(temp), infile);
    plr->status = strtoul(temp, NULL, 10);
    if (feof(infile))
	return (FALSE);
    if (plr->status == DEAD) {
	readline(plr->killer, sizeof(plr->killer));
	if (feof(infile))
	    return (FALSE);
    }
    plr->strength = readnumb(6);
    plr->intelligence = readnumb(6);
    plr->luck = readnumb(6);
    plr->dexterity = readnumb(6);
    plr->constitution = readnumb(6);
    plr->charisma = readnumb(6);
    plr->experience = readnumb(0);
    plr->r = readnumb(1);
    plr->hps = readnumb(10);
    plr->weapon = readnumb(1);
    plr->armour = readnumb(1);
    plr->gold = readnumb(0);
    plr->flights = readnumb(3);
    plr->bank = readnumb(0);
    plr->wins = readnumb(0);
    plr->loses = readnumb(0);
    plr->plus = readnumb(0);
    plr->fights = readnumb(FIGHTS_PER_DAY);
    plr->battles = readnumb(BATTLES_PER_DAY);
    plr->vary = readfloat(supplant());
    endofline();
    return (TRUE);
}
BOOL
loaduser(char *name, BOOL bbsname, struct playertype * plr)
{
    infile = fopen("data/characte.lan", "rb");
    fgets(temp, sizeof(temp), infile);
    while (loadnextuser(plr)) {
	if (bbsname) {
	    if (!stricmp(plr->name, name)) {
		fclose(infile);
		return (TRUE);
	    }
	} else {
	    if (!stricmp(plr->pseudo, name)) {
		fclose(infile);
		return (TRUE);
	    }
	}
    }
    fclose(infile);
    return (FALSE);
}

int
main(int argc, char **argv)
{
    DWORD           i;
    BOOL            found;
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
	readline(weapon[i].name, sizeof(weapon[i].name));
	weapon[i].attack = readnumb(1);
	weapon[i].power = readnumb(1);
	endofline();
    }
    fclose(infile);
    infile = fopen("data/armor.lan", "rb");
    od_set_color(L_BLUE, D_BLACK);
    for (i = 1; i <= 25; i++)
	readline(armour[i].name, sizeof(armour[i].name));
    fclose(infile);
    infile = fopen("data/prices.lan", "rb");
    for (i = 1; i <= 25; i++) {
	weapon[i].cost = readnumb(100000000);
	armour[i].cost = weapon[i].cost;
	endofline();
    }
    fclose(infile);
    user.attack = weapon[user.weapon].attack;
    user.power = weapon[user.weapon].power;
    infile = fopen("data/experience.lan", "rb");
    for (i = 1; i <= 28; i++) {
	required[i] = readnumb(100000000);
	endofline();
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
