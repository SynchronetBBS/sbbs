#include <stdlib.h>
#include <string.h>

#include "xpdev.h"
#include "dgnlnce.h"
#include "fileutils.h"
#include "userfile.h"
#include "utils.h"

double
tohit(struct playertype * attacker, struct playertype * defender)
{
    return ((attacker->attack * attacker->strength * attacker->dexterity * (xp_random(5) + 1) * attacker->vary) /
	    (defender->armour * defender->dexterity * defender->luck * defender->vary));
}

double
damage(struct playertype * attacker, struct playertype * defender)
{
    return ((attacker->power * attacker->strength * (xp_random(5) + 1) * (xp_random(5) + 1) * attacker->vary) /
	    ((defender->armour) * defender->luck * defender->vary));
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

void
amode(void)
{
    double          roll;	       /* Used for To Hit and damage rolls */
    int             okea;
    int             tint;
    roll = tohit(&user, &opp);
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
	roll = damage(&user, &opp);
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
	roll = tohit(&opp, &user);
	if (roll < 1.5) {
	    od_set_color(D_GREEN, D_BLACK);
	    od_printf("%s attack tears your armour.\r\n", sexstrings(&opp, HIS, TRUE));
	} else {
	    roll = tohit(&opp, &user);
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
searcher(char *filename)
{
    FILE           *infile;
    int             a;
    int             rd;
    infile = fopen(filename, "rb");
    user.fights--;
    rd = xp_random(readnumb(0, infile) - 1) + 1;
    endofline(infile);
    for (a = 1; a <= rd; a++) {
	readline(opp.pseudo, sizeof(opp.pseudo), infile);
	opp.hps = readnumb(10, infile);
	opp.damage = 0;
	opp.attack = readnumb(1, infile);
	opp.power = readnumb(1, infile);
	opp.armour = readnumb(1, infile);
	opp.luck = readnumb(1, infile);
	opp.strength = readnumb(6, infile);
	opp.dexterity = readnumb(6, infile);
	opp.gold = readnumb(0, infile);
	opp.experience = readnumb(0, infile);
	opp.status = MONSTER;
	opp.sex = 'I';
	endofline(infile);
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
fight(char *filename)
{
    searcher(filename);
    battle();
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
doggie(void)
{
    char            tmphh[3];
    int             a, i;
    DWORD           enemy;
    BOOL            finder;
    FILE           *infile;
    char            temp[81];
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
	    while (loadnextuser(&opp, infile)) {
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
		if (!loadnextuser(&opp, infile))
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
