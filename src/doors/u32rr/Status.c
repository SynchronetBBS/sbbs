/*
Copyright 2007 Jakob Dangarden
 This file is part of Usurper.

    Usurper is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Usurper is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Usurper; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <ctype.h>

#include "IO.h"
#include "doorIO.h"
#include "Alchemisty.h"
#include "Config.h"
#include "Races.h"
#include "Bash.h"
#include "files.h"
#include "macros.h"
#include "various.h"
#include "todo.h"

// Usurper - Status of Player
char *nada="{nothing}";

static void Meny(void *cbdata)
{
	int				x,i;
	struct player	*pl=(struct player *)cbdata;
	char			*itemstr[ALLOW_TOTAL+1][2];
	enum allow_items	aiorder[ALLOW_TOTAL] = {
					ALLOW_SecondaryWeapon,
					ALLOW_Head,
					ALLOW_Face,
					ALLOW_Neck1,
					ALLOW_Neck2,
					ALLOW_Arm,
					ALLOW_Body,
					ALLOW_RightFinger,
					ALLOW_LeftFinger,
					ALLOW_AroundBody,
					ALLOW_Leg,
					ALLOW_Feet,
					ALLOW_Hand,
					ALLOW_Waist,
					ALLOW_Shield,
	};
	char *aitext[ALLOW_TOTAL] = {
					"Left Hand   : ",
					"Head        : ",
					"Face        : ",
					"Neck        : ",
					"Neck        : ",
					"Arms        : ",
					"Body        : ",
					"Right Finger: ",
					"Left Finger : ",
					"Around Body : ",
					"Legs        : ",
					"Feet        : ",
					"Hands       : ",
					"Waist       : ",
					"Shield      : ",
	};
	int current[ALLOW_TOTAL] = {
					pl->lhand,
					pl->head,
					pl->face,
					pl->neck,
					pl->neck2,
					pl->arms,
					pl->body,
					pl->rfinger,
					pl->lfinger,
					pl->abody,
					pl->legs,
					pl->feet,
					pl->hands,
					pl->waist,
					pl->shield,
	};
	struct object *arrays[ALLOW_TOTAL] = {
					weapons,
					head,
					face,
					neck,
					neck,
					arms,
					body,
					rings,
					rings,
					abody,
					legs,
					feets,
					hands,
					waist,
					shields,
	};

	clr();

	if(pl==player) {
		D(magenta, "Your status, ", pl->name2);
		if(pl->king) {
			D(yellow, " (THE ", upcasestr(kingstring(pl->sex)), ")");
		}

		if(level_raise(pl->level, pl->exp)==0) {
			DL(lgreen, " (you are eligible for a level raise!)");
		}
		else if(pl->trains > 0) {
			DL(lgreen, " (you should train some moves!)");
		}
	}
	else
		DL(magenta, pl->name2, " Status");
	nl();
	if(config.classic) {
		DL(config.textcolor, Asprintf("Race       : %-17s Weapon     : %s", UCRaceName[pl->race], pl->weapon==-1?"bare hands":weapon[pl->weapon].name));
		DL(config.textcolor, Asprintf("Class      : %-17s Armor      : %s", UCClassNames[pl->class], pl->armor==-1?"skin":armor[pl->armor].name));
		DL(config.textcolor, Asprintf("Level      : %-17s Age        : %s years", commastr(pl->level), commastr(pl->age)));
		DL(config.textcolor, Asprintf(     "%-10s : %-17s Bank Funds : %s", config.moneytype, commastr(pl->gold), commastr(pl->bankgold)));
		DL(config.textcolor, Asprintf("Healings   : %-17s Experience : %s", commastr(pl->healing), commastr(pl->exp)));
		DL(lgreen,           Asprintf("Hitpoints  : %-17s Strength   : %s", Asprintf("%s/%s", commastr(pl->hps), commastr(pl->maxhps)),commastr(pl->strength)));
		DL(config.textcolor, Asprintf("Defence    : %-17s Team       : %s", commastr(pl->defence), pl->team[0]?Asprintf("%s%s%s",config.teamcolor, pl->team, config.textcolor):"<not in a team>"));
		DL(config.textcolor, Asprintf("WeaponPower: %-17s ArmorPower : %s", commastr(pl->wpow*11), commastr(pl->apow*11)));
		DL(config.textcolor, Asprintf("Trainings  : %-17s %s", commastr(pl->trains), (pl->class==Cleric || pl->class==Sage || pl->class==Magician)?Asprintf("Mana       : %s/%s", commastr(pl->mana), commastr(pl->maxmana)):""));
		DL(config.textcolor, Asprintf("Pick-Pocket: %s",commastr(pl->pickpocketattempts)));
		nl();
		D(config.textcolor, "(", config.hotkeycolor, "M", config.textcolor, ")ore  (",
				config.hotkeycolor, "C", config.textcolor, ")onfig  (",
				config.hotkeycolor, "S", config.textcolor, ")kills  (",
				config.hotkeycolor, "H", config.textcolor, ")ealing  (",
				config.hotkeycolor, "T", config.textcolor, ")eam  (",
				config.hotkeycolor, "L", config.textcolor, ")evel ");
		if(pl->class == Cleric || pl->class == Magician || pl->class == Sage)
			menu2("(E) Spells");
	}
	else {
		// NEW mode displayen starts here
		x=0;
		memset(itemstr, 0, sizeof(itemstr));
		itemstr[x][0]="Weapon      : ";
		itemstr[x][1]=pl->rhand==-1?nada:weapons[pl->rhand].name;
		x++;
		for(i=0; i<ALLOW_TOTAL; i++) {
			if(config.allowitem[aiorder[i]]) {
				itemstr[x][0]=aitext[i];
				if(current[i]==-1)
					itemstr[x][1]=nada;
				else
					itemstr[x][1]=arrays[i][current[i]].name;
				x++;
			}
		}

		// Race
		D(config.textcolor, Asprintf("Race       : %-17s ", UCRaceName[pl->race]));
		if(itemstr[0][0])
			DL(config.textcolor, itemstr[0][0], itemstr[0][1]);
		else
			nl();
		D(config.textcolor, Asprintf("Class      : %-17s ", UCClassNames[pl->class]));
		if(itemstr[1][0])
			DL(config.textcolor, itemstr[1][0], itemstr[1][1]);
		else
			nl();
		D(config.textcolor, Asprintf("Level      : %-17s ", commastr(pl->level)));
		if(itemstr[2][0])
			DL(config.textcolor, itemstr[2][0], itemstr[2][1]);
		else
			nl();
		D(config.textcolor, Asprintf("Age        : %-17s ", commastr(pl->age)));
		if(itemstr[3][0])
			DL(config.textcolor, itemstr[3][0], itemstr[3][1]);
		else
			nl();
		D(config.textcolor, Asprintf(     "%-10s : %-17s ", config.moneytype, commastr(pl->gold)));
		if(itemstr[4][0])
			DL(config.textcolor, itemstr[4][0], itemstr[4][1]);
		else
			nl();
		D(config.textcolor, Asprintf("Bank Funds : %-17s ", commastr(pl->bankgold)));
		if(itemstr[5][0])
			DL(config.textcolor, itemstr[5][0], itemstr[5][1]);
		else
			nl();
		D(config.textcolor, Asprintf("Healings   : %-17s ", commastr(pl->healing)));
		if(itemstr[6][0])
			DL(config.textcolor, itemstr[6][0], itemstr[6][1]);
		else
			nl();
		D(config.textcolor, Asprintf("Experience : %-17s ", commastr(pl->exp)));
		if(itemstr[7][0])
			DL(config.textcolor, itemstr[7][0], itemstr[7][1]);
		else
			nl();
		D(lgreen,           Asprintf("Hitpoints  : %-17s ", Asprintf("%s/%s", commastr(pl->hps), commastr(pl->maxhps))));
		if(itemstr[8][0])
			DL(config.textcolor, itemstr[8][0], itemstr[8][1]);
		else
			nl();
		D(config.textcolor, Asprintf("Strength   : %-17s ", commastr(pl->strength)));
		if(itemstr[9][0])
			DL(config.textcolor, itemstr[9][0], itemstr[9][1]);
		else
			nl();
		D(config.textcolor, Asprintf("Defence    : %-17s ", commastr(pl->defence)));
		if(itemstr[10][0])
			DL(config.textcolor, itemstr[10][0], itemstr[10][1]);
		else
			nl();
		D(config.textcolor, Asprintf("Team       : %-17s ", pl->team[0]?Asprintf("%s%s%s",config.teamcolor, pl->team, config.textcolor):"<not in a team>"));
		if(itemstr[11][0])
			DL(config.textcolor, itemstr[11][0], itemstr[11][1]);
		else
			nl();
		D(config.textcolor, Asprintf("WeaponPower: %-17s ", commastr(pl->wpow*11)));
		if(itemstr[12][0])
			DL(config.textcolor, itemstr[12][0], itemstr[12][1]);
		else
			nl();
		D(config.textcolor, Asprintf("ArmorPower : %-17s ", commastr(pl->apow*11)));
		if(itemstr[13][0])
			DL(config.textcolor, itemstr[13][0], itemstr[13][1]);
		else
			nl();
		D(config.textcolor, Asprintf("Trainings  : %-17s ", commastr(pl->trains)));
		if(itemstr[14][0])
			DL(config.textcolor, itemstr[14][0], itemstr[14][1]);
		else
			nl();
		D(config.textcolor, (pl->class==Cleric || pl->class==Sage || pl->class==Magician)?Asprintf("Mana       : %s", Asprintf("%s/%s", commastr(pl->mana), commastr(pl->maxmana))):"                               ");
		if(itemstr[15][0])
			DL(config.textcolor, itemstr[15][0], itemstr[15][1]);
		else
			nl();
		nl();

		if(pl != player)
			DL(config.textcolor, "**");
		else {
			D(config.textcolor, "(", config.hotkeycolor, "M", config.textcolor, ")ore  (",
					config.hotkeycolor, "I", config.textcolor, ")nventory  (",
					config.hotkeycolor, "C", config.textcolor, ")onfig  (",
					config.hotkeycolor, "S", config.textcolor, ")kills  (",
					config.hotkeycolor, "H", config.textcolor, ")ealing  (",
					config.hotkeycolor, "T", config.textcolor, ")eam  (",
					config.hotkeycolor, "L", config.textcolor, ")evel ");
			if(pl->class == Cleric || pl->class == Magician || pl->class == Sage)
				menu2("(E) Spells");
		}
	}
}

static void LevelInfo(struct player *pl)
{
	int		x;

	clr();
	nl();
	DL(lcyan, "Level Info");
	DL(white, commastr(pl->level), config.textcolor, " character");
	DL(lgreen, "\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4");
	x=level_raise(pl->level, pl->exp);
	DL(config.textcolor, "You are a level ", yellow, commastr(pl->level), config.textcolor, " ", LCRaceName[pl->race], ".");
	if(x>0)
		DL(config.textcolor, "You need ", commastr(x), config.textcolor, " experience points to advance a level.");
	else
		DL(white, "You have enough points to make a level!");
	nl();
	upause();
}

static bool HealingMenu(struct player *pl)
{
	bool	infect;
	char	ch;

	clr();
	nl();
	nl();
	DL(cyan, "Hitpoints : ", magenta, commastr(pl->hps), "/", commastr(pl->maxhps));
	if(pl->hps != pl->maxhps)
		DL(lred, "You NEED healing.");
	else
		DL(lred, "You don't need healing.");

	infect=false;
	if(pl->blind || pl->plague || pl->smallpox || pl->measles || pl->leprosy) {
		DL(lred, "You have a disease!");
		infect=true;
	}
	menu2("(Q)uick head ");
	menu2("(H)eal");
	if(infect)
		menu2(" (D)iseases");
	DL(config.textcolor, " or [", config.returnenter, "]");
	D(config.textcolor, ":");
	do {
		ch=toupper(gchar());
	} while(strchr("QHD\r", ch)==NULL);

	switch(ch) {
		case 'Q':
			Quick_Healing(pl);
			upause();
			break;
		case 'H':
			Healing(pl);
			upause();
			break;
		case 'D':
			nl();
			DL(magenta, "Affecting Diseases");
			if(pl->blind)
				DL(red, "*Blindness*");
			if(pl->plague)
				DL(red, "*Plague*");
			if(pl->smallpox)
				DL(red, "*Smallpox*");
			if(pl->measles)
				DL(red, "*Measles*");
			if(pl->leprosy)
				DL(red, "*Leprosy*");
			if(!infect) {
				nl();
				DL(yellow, "You are not infected!");
				DL(yellow, "Stay healthy!");
			}
			break;
		case '\r':
			return false;
	}
	return(true);
}

static void SkillsMenu(struct player *pl)
{
	const char	*col;
	int			i;

	clr();
	nl();
	DL(white, "Your Close-Combat Skills :");
	DL(magenta, "+-+-+-+-+-+-+-+-+-+-+-+-+-");
	nl();
	for(i=0; i<BASH_COUNT; i++) {
		D(config.bashcolor, ljust(UCBashName[i], 15));
		switch(pl->skill[i]) {
			case 0: case 1: case 2:
				col=lcyan;
				break;
			case 3: case 4: case 5: case 6: case 7: case 8:
				col=cyan;
				break;
			case 9: case 10: case 11: case 12: case 13:
				col=lred;
				break;
			case 14: case 15: case 16:
				col=yellow;
				break;
			case 17:
				col=white;
				break;
			default:
				col=lcyan;
				break;
		}
		DL(col, "- ", BashRank[pl->skill[i]]);
	}
	if(pl->trains > 0){
		DL(white, "You should seek your training master right now!");
		DL(white, "You are able to improve your skills!");
	}
	upause();
}

static bool ConfigMenu(struct player *pl)
{
	char	ch;
	bool	force=false;
	char	str[256];
	int		i,x;

	do {
		clr();
		DL(white, "Your Setup & Off-line Behaviour :");
		DL(magenta, "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+");
		if((!player->expert) || force) {
			force=false;
			clr();
			DL(white, "Your Setup & Off-line Behaviour :");
			DL(magenta, "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+");
			menu2("(0) Ansi Graphics is currently : ");
			DL(white, dropfile.ansi?"On":"Off");
			menu2("(1) Buy new healing potions when needed : ");
			DL(white, pl->autoheal?"Yes":"No");
			menu("(2) Phrase when you are being attacked :");
			DL(config.talkcolor, "     ",pl->attacked);
			menu("(3) Phrase when you have defeated an opponent :");
			DL(config.talkcolor, "     ",pl->defeatedopp);
			menu("(4) Phrase when you have been defeated :");
			DL(config.talkcolor,"     ",pl->beendefeated);
			menu("(5) Phrase when you are begging for mercy :");
			DL(config.talkcolor,"     ",pl->begging);
			menu("(6) Character Description");
			menu("(7) Battlecry :");
			DL(config.talkcolor,"     ",pl->battlecry);
			menu("(8) Phrase when you spare your opponent :");
			DL(config.talkcolor,"     ",pl->spareopp);
			menu("(9) Phrase when you don't spare your opponent :");
			DL(config.talkcolor,"     ",pl->dontspareopp);
			menu2("(A) Expert Menus : ");
			DL(white, pl->expert?"On":"Off");
			menu2("(B) Online Ear   : ");
			switch(pl->ear) {
				case ear_all:
					DL(lcyan, "*All*");
					break;
				case ear_personal:
					DL(lcyan, "*Personal*");
					break;
				case ear_quiet:
					DL(lcyan, "*Quiet*");
					break;
			}
			menu2("(C) Auto-Display menus   : ");
			DL(white, pl->auto_meny?"Yes":"No");
			menu2("(D) Auto Hate : ");
			DL(white, pl->autohate?"Yes":"No");
		}
		if(player->expert) {
			nl();
			D(config.textcolor, "(0..9,A..D,?, [",config.returnenter,"] to continue) :");
		}
		else {
			nl();
			D(config.textcolor, "(", config.hotkeycolor, config.returnenter, config.textcolor, "), ? :");
		}
		ch=toupper(gchar());
	} while(strchr("1234567890ABCD?\r", ch)==NULL);

	switch(ch) {
		case '\r':
			return false;
		case '?':
			if(player->expert)
				force=true;
			break;
		case '0':	// ANSI
			dropfile.ansi=!dropfile.ansi;
			break;
		case '1':	// AutoHeal
			pl->autoheal=!pl->autoheal;
			break;
		case '2':
			nl();
			DL(config.textcolor, "What shall you say when you are being attacked ?");
			DL(config.textcolor, "Enter phrase (1 line)");
			D(config.textcolor, ":");
			get_string(str, sizeof(pl->attacked));
			D(config.talkcolor, str);
			if(confirm("Is this alright", 'Y'))
				strcpy(pl->attacked, str);
			break;
		case '3':
			nl();
			DL(config.textcolor, "What shall you say when you have defeated somebody ?");
			DL(config.textcolor, "Enter phrase (1 line)");
			D(config.textcolor, ":");
			get_string(str, sizeof(pl->defeatedopp));
			D(config.talkcolor, str);
			if(confirm("Is this alright", 'Y'))
				strcpy(pl->defeatedopp, str);
			break;
		case '4':
			nl();
			DL(config.textcolor, "What shall you say when you have been defeated ?");
			DL(config.textcolor, "Enter phrase (1 line)");
			D(config.textcolor, ":");
			get_string(str, sizeof(pl->beendefeated));
			D(config.talkcolor, str);
			if(confirm("Is this alright", 'Y'))
				strcpy(pl->beendefeated, str);
			break;
		case '5':
			nl();
			DL(config.textcolor, "What shall you say when you are begging for mercy ?");
			DL(config.textcolor, "Enter phrase (1 line)");
			D(config.textcolor, ":");
			get_string(str, sizeof(pl->begging));
			D(config.talkcolor, str);
			if(confirm("Is this alright", 'Y'))
				strcpy(pl->begging, str);
			break;
		case '6':
			clr();
			DL(config.textcolor, "Description :");
			for(i=0; i<4; i++)
				DL(lgreen, "[", commastr(i+1), "] ", pl->desc[i]);
			nl();
			if(confirm("Would You like to change this",'Y')) {
				DL(config.textcolor, "Enter your description below, Max 4 lines");
				x=0;
				do {
					x++;
					D(config.textcolor, "[", commastr(x), "]:");
					get_string(pl->desc[x-1], sizeof(pl->desc[x-1]));
				} while (pl->desc[x-1] && x<=4);
			}
			break;
		case '7':
			nl();
			DL(config.textcolor, "What shall your general Battlecry be ?");
			DL(config.textcolor, "This is displayed when you are fighting with your team, "
					"in the Dungeons and in Team fights.");
			DL(config.textcolor, "Enter phrase (1 line)");
			D(config.textcolor, ":");
			get_string(str, sizeof(pl->battlecry));
			DL(config.talkcolor, str);
			if(confirm("Is this alright",'Y'))
				strcpy(pl->battlecry, str);
			break;
		case '8':
			nl();
			DL(config.textcolor, "What shall you say when you spare your opponents life ?");
			DL(config.textcolor, "Enter phrase (1 line)");
			D(config.textcolor, ":");
			get_string(str, sizeof(pl->spareopp));
			if(confirm("Is this alright",'Y'))
				strcpy(pl->spareopp, str);
			break;
		case '9':
			nl();
			DL(config.textcolor, "What shall you say when you don't spare your opponents life ?");
			DL(config.textcolor, "Enter phrase (1 line)");
			D(config.textcolor, ":");
			get_string(str, sizeof(pl->dontspareopp));
			if(confirm("Is this alright",'Y'))
				strcpy(pl->dontspareopp, str);
			break;
		case 'A':
			pl->expert=!pl->expert;
			break;
		case 'B':
			nl();
			DL(white, "Set \"Outside\" MODE variable :");
			DL(white, "(A)ll information available.");
			DL(white, "   All vital information about other players");
			DL(white, "   will be displayed.");
			nl();
			DL(white, "(P)ersonal information only.");
			DL(white, "   You will only get information concerning");
			DL(white, "   yourself.");
			nl();
			DL(white, "(Q)uiet mode. No information about other");
			DL(white, "   players will be displayed. You will refuse");
			DL(white, "   to accept any messages or challenges.");
			D(white, ":");
			do {
				ch=toupper(gchar());
			} while (strchr("APQ", ch)==NULL);
			switch(ch) {
				case 'A':
					pl->ear=ear_all;
					break;
				case 'P':
					pl->ear=ear_personal;
					break;
				case 'Q':
					pl->ear=ear_quiet;
					break;
			}
			onliner->ear=pl->ear;
			break;
		case 'C':
			pl->auto_meny=!pl->auto_meny;
			if(pl->automeny)
				DL(white, "Auto-Display menus is ON.");
			else
				DL(white, "Auto-Display menus is OFF.");
			break;
		case 'D':	// auto hate, deteriorate relations when attacked offline
			pl->autohate=!pl->autohate;
			if(pl->autohate) {
				DL(white, "Auto-Hate is ON.");
				DL(config.textcolor, "When you are attacked off-line, your relation with");
				DL(config.textcolor, "the attacking player will ", lred, "deteriorate", config.textcolor, ".");
			}
			else {
				DL(white, "Auto-Hate is OFF.");
				DL(config.textcolor, "When you are attacked off-line, your relation with");
				DL(config.textcolor, "the attacking player will not be affected.");
			}
			upause();
			break;
	}
	if(player->expert)
		upause();

	return true;
}

static bool Inventory_Menu(struct player *pl)
{
	char	ch;
	int		i;

	inventory_display(pl);
	if((!pl->expert) || ch=='?') {
		menu2("(E)xamine item ");
		menu2("(D)rop ");
		menu2("(U)se ");
		menu2("(S)top using ");
		menu2("(*) Drop All ");
		D(config.textcolor, "[", config.hotkeycolor, config.returnenter, config.textcolor, "] to Continue :");
	}
	else {
		D(config.textcolor, "Inventory (E,D,U,S,*,?) :");
	}

	do {
		ch=toupper(gchar());
	} while(strchr("DUES*?\r", ch)==NULL);

	switch(ch) {
		case '\r':
			return false;
		case '*':	// Drop everything
			nl();
			nl();
			if(confirm("Drop everything in Your inventory", 'N')) {
				for(i=0; i<MAXITEM; i++) {
					if(pl->item[i]>0) {
						if(!items(pl->itemtype[i])[pl->item[i]].cursed) {
							DL(config.textcolor, "You drop ", config.itemcolor, items(pl->itemtype[i])[pl->item[i]].name);
							pl->item[i]=-1;
						}
						else {
							DL(config.textcolor, "You can't drop the ", config.itemcolor, items(pl->itemtype[i])[pl->item[i]].name, config.textcolor, "! It seems to be cursed!");
						}
					}											
				}
				nl();
				upause();
				inventory_sort(pl);
			}
			break;
		case 'S':	// Stop using item
			Remove_Item();
			break;
		case 'E':	// Examine item
			do {
				nl();
				i=item_select(pl);
				if(i>-1) {
					nl();
					DL(config.textcolor, "You examine the ", config.itemcolor, items(pl->itemtype[i])[pl->item[i]].name, config.textcolor, "...");
					nl();
					for(i=0; i<5; i++) {
						if(items(pl->itemtype[i])[pl->item[i]].desc1[i][0])
							DL(config.textcolor, items(pl->itemtype[i])[pl->item[i]].desc1[i]);
					}
					nl();
					upause();
				}
			} while(i>=0);
			break;
		case 'U':	// Use item
			use_item(0);
			nl();
			break;
		case 'D':	// Drop specific item
			drop_item(pl);
			break;
	}
	return(true);
}

void More_Info(struct player *pl)
{
	clr();
	nl();
	nl();
	DL(config.textcolor, Asprintf("Wisdom     : %-17s Darkness   : %s", commastr(pl->wisdom), commastr(pl->dark)));
	DL(config.textcolor, Asprintf("Charisma   : %-17s Fights     : %s", commastr(pl->charisma), commastr(pl->fights)));
	DL(config.textcolor, Asprintf("Agility    : %-17s Teamfights : %s", commastr(pl->agility), commastr(pl->tfights)));
	DL(config.textcolor, Asprintf("Dexterity  : %-17s Chivalry   : %s", commastr(pl->dex), commastr(pl->chiv)));
	DL(config.textcolor, Asprintf("Stamina    : %-17s", commastr(pl->stamina)));
	nl();
	DL(config.textcolor, "Disease resistance  : ", immunity(pl->disres));
	DL(config.textcolor, "Drug addiction      : ", commastr(pl->addict), " %");
	DL(config.textcolor, "Mental stability    : ", commastr(pl->mental), " %");
	nl();
	DL(config.textcolor, "Height : ",commastr(pl->height), " cm, Hair is ", color[pl->hair]);
	DL(config.textcolor, "Weight : ",commastr(pl->weight), " kilos, Eyes are ", color[pl->eyes]);
	D(config.textcolor, "Skin : ",color[pl->skin]);

	// Alchemist Poison
	if(pl->class == Alchemist) {
		nl();
		nl();
		D(config.textcolor, "Active Poison : ", white, pl->poison>0?Alchemist_Poison(pl):"None");
	}
	nl();
	nl();
	DL(config.textcolor, "Monster Kills : ", commastr(pl->m_kills));
	DL(config.textcolor, "      Defeats : ", commastr(pl->m_defeats));
	DL(config.textcolor, "Player  Kills : ", commastr(pl->p_kills));
	DL(config.textcolor, "      Defeats : ", commastr(pl->p_defeats));
	DL(config.textcolor, "                 ..", lgreen, commastr(door.time_left), config.textcolor, " min left..  Resurrections left..", lgreen, commastr(pl->resurrections));
	nl();
	upause();
}

void Status(struct player *pl)
{
	bool		refresh=true;
	const char	*name="Status";
	const char	*keys;
	char		ch;

	if(config.classic) {
		if(pl->class==Cleric || pl->class==Sage || pl->class==Magician)
			keys="(M,H,T,S,C,L,E,?)";
		else
			keys="(M,H,T,S,C,L,?)";
	}
	else {
		if(pl->class==Cleric || pl->class==Sage || pl->class==Magician)
			keys="(M,H,T,S,I,C,L,E,?)";
		else
			keys="(M,H,T,S,I,C,L,?)";
	}
	if(pl != player) {
		Display_Menu(true, false, &refresh, name, keys, Meny, pl);
		nl();
		upause();
		return;
	}

	Display_Menu(false, false, &refresh, name, keys, Meny, pl);

	for(;;) {
		Display_Menu(true, true, &refresh, name, keys, Meny, pl);
		do {
			ch=toupper(gchar());
			if(ch=='I' && config.classic)
				ch='-';
			if(ch=='E') {
				if(pl->class!=Cleric && pl->class!=Sage && pl->class!=Magician)
					ch='-';
			}
		} while(strchr("QMITHLCS?E\r", ch)==NULL);

		switch(ch) {
			case '\r':
			case 'Q':
				return;
			case 'E':
				nl();
				spell_list(pl);
				break;
			case '?':
				Display_Menu(player->expert, false, &refresh, name, keys, Meny, pl);
				break;
			case 'T':	// Team
				if(player->team[0]) {
					Display_Member(player, true);
					Display_Members(player->team, false);
					nl();
				}
				else {
					nl();
					nl();
					DL(magenta, "You don't belong to a team.");
				}
				break;
			case 'L':	// XP left to raise info
				LevelInfo(pl);
				break;
			case 'H':	// Healing
				while(HealingMenu(pl))
					;
				break;
			case 'S':
				SkillsMenu(pl);
				break;
			case 'C':
				while(ConfigMenu(pl))
					;
				break;
			case 'I':	// Inventory display
				while(Inventory_Menu(pl))
					;
				break;
			case 'M':
				More_Info(pl);
				break;
		}
	}
}
