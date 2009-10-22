
/*

Copyright 2007 Jakob Dangarden
C translation Copyright 2009 Stephen Hurd

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


// Usurper - 'Alchemists only' place
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

#include "macros.h"
#include "files.h"
#include "structs.h"
#include "various.h"

#include "todo.h"

static const char *name="Alchemist Store";
static const char *expert_prompt="(B,S,T,R,?)";
static void Failed_Quest(uint8_t level)
{
	char *str;

	if(asprintf(&str, " %s%s%s failed the %s test for the Secret Order.",uplc, player->name2, config.textcol1, commastr(level))<0)
		CRASH;

	newsy(true, "Failed Challenge!", str, NULL);
	free(str);

	pbreak();
	BAD("Oh No! You have failed the test! You may try again tomorrow...");
	pbreak();
	player->allowed=false;
	reduce_player_resurrections(player, true);
	upause();
	halt();
}

/*
 * Returns a string with info on how strong the current poison is
 */
static const char *Alchemist_Poison(const struct player *rec)
{
	if(rec->poison  > 80)
		return "Deadly";
	if(rec->poison > 50)
		return "Strong";
	if(rec->poison > 25)
		return "Medium";
	if(rec->poison > 0)
		return "Light";
	return "None";
}

static void Meny()
{
	newscreen();
	pbreak();
	HEADER("-*- Alchemists Heaven -*-");
	pbreak();
	TEXT("Scarred by the tooth of time, the old cottage doesn't look like "	// Added "like" at end
		"much. Usilama the once so proud Druid is now the shopkeeper "
		"of the Alchemists Heaven. Here you can buy the stuff "
		"you need for expanding your knowledge of your profession. "
		"Recipes can be learned and practiced. Always remember that "
		"your skills as an Alchemist can be expanded beyond belief...");
	pbreak();

	dlc(config.textcolor, "Poison currently affecting your weapon : ", config.highlightcolor, Alchemist_Poison(player), D_DONE);

	pbreak();
	menu("(B)uy Poison");
	menu("(T)he Secret Order");
	menu("(S)tatus");
	menu("(R)eturn to street");
}

static void Join_Order(void)
{
	char	*str;
	struct monster	mon;
	struct monster	*monsters[2]={&mon, NULL};

	pbreak();
	dlc(config.plycolor, "Usilama", config.textcolor, " looks at you very intensly.", D_DONE);
	SAY("I know that you are not a member of the Secret Order. "
			"Since you have made some progress in your career, "
			"an application for membership could be accepted...");
	upause();

	pbreak();
	SAY("So you would like to join the secret order? "
			"You have to pass some tests before you can be approved! "
			"These tests are very dangerous and can easily have you killed!");
	upause();

	pbreak();
	SAY("But if you want to become a member of this powerful order, you should not hesitate. "
			"The benefits of the brotherhood cannot be measured...!");
	upause();

	pbreak();
	if(confirm("Apply for membership",'N')) {
		// Quest 1
		pbreak();
		SAY("Prepare for the first challenge!");
		pbreak();
		memset(&mon, 0, sizeof(mon));

		switch(rand_num(3)) {
			case 0:
				strcpy(mon.name, "Great Boa");
				strcpy(mon.phrase, "Ssssssss.....!!");
				strcpy(mon.weapon, "Moon Claw");
				mon.hps=150;
				mon.strength=20;
				mon.defence=12;
				mon.weappow=15;
				mon.punch=100;
				mon.poisoned=false;
				mon.disease=false;
				mon.grabweap=false;
				mon.grabarm=false;
				break;
			case 1:
				strcpy(mon.name, "Great Boar");
				strcpy(mon.phrase, "Mmmmrrrrr..!");
				strcpy(mon.weapon, "Diamond Claw");
				mon.hps=125;
				mon.strength=25;
				mon.defence=10;
				mon.weappow=17;
				mon.punch=80;
				mon.poisoned=false;
				mon.disease=false;
				mon.grabweap=false;
				mon.grabarm=false;
				break;
			case 2:
				strcpy(mon.name, "Huge Tiger");
				strcpy(mon.phrase, "rrrrrrr...");
				strcpy(mon.weapon, "Steel Jaws");
				mon.hps=110;
				mon.strength=30;
				mon.defence=14;
				mon.weappow=27;
				mon.punch=90;
				mon.poisoned=false;
				mon.disease=false;
				mon.grabweap=false;
				mon.grabarm=false;
				break;
		}
		global_begged=false;
		global_nobeg=true;

		player_vs_monsters(Pl_Vs_Alchemist, monsters, NULL);

		if(player->hps < 1)
			Failed_Quest(2);

		// Quest 2
		pbreak();
		pbreak();
		GOOD("*****************");
		GOOD("*   WELL DONE!  *");
		GOOD("*  1st mission  *");
		GOOD("*    cleared    *");
		GOOD("*****************");
		pbreak();
		upause();

		pbreak();
		dlc(config.textcolor, "Prepare to face the wicked ", config.monstercolor, "Iceman", config.textcolor, "!", D_DONE);
		pbreak();
		NOTICE("** You are doing well so far **");
		upause();

		memset(&mon, 0, sizeof(mon));

		strcpy(mon.name, "Iceman");
		strcpy(mon.phrase, "Thou shall be frosty!");
		strcpy(mon.weapon, "Ice Spear");
		mon.hps=75;
		mon.strength=30;
		mon.defence=17;
		mon.weappow=25;
		mon.punch=75;
		mon.poisoned=false;
		mon.disease=false;
		mon.grabweap=true;
		mon.grabarm=false;
		player_vs_monsters(Pl_Vs_Alchemist, monsters, NULL);

		if(player->hps < 1)
			Failed_Quest(1);

		// Quest 3
		pbreak();
		pbreak();
		GOOD("*****************");
		GOOD("*   WELL DONE!  *");
		GOOD("*  2nd mission  *");
		GOOD("*    cleared    *");
		GOOD("*****************");
		pbreak();

		if(asprintf(&str, " %s%s%s has been accepted as a member of the secret order.", uplc, player->name2, config.textcol1)<0)
			CRASH;
		newsy(true, "** SECRET ORDER OF ALCHEMY EXPANDS **", str, NULL);
		free(str);

		pbreak();
		player->amember=true;
		user_save(player);

		GOOD("\"WELL DONE! You have been accepted as a member of our secret society. As a member of this order you must follow certain rules. Neglecting to do so will lead to immediate excommunication from the order\".");
		pbreak();
		upause();
	}
	else {
		pbreak();
		TEXT("Usilama looks a bit disappointed.");
		SAY("Well then, You are welcome to try again later.");
		pbreak();
	}
}

static void Create_Poison(void)
{
	const char *name=NULL;
	int			price;
	int			strength;
	char		ch;

	pbreak();
	pbreak();
	HEADER("Create Poison");
	TEXT("You can create 5 different poisons :");
	menu("(1) Stone Breaker      ( 50,000)");
	menu("(2) Warrior Terminator (150,000)");
	menu("(3) Magic Terrorizer   (250,000)");
	menu("(4) Evil Disposer      (300,000)");
	menu("(5) Heart Tracker      (325,000)");
	menu("(A)bort");
	PART(":");
	do {
		ch=toupper(gchar());
	} while(strchr("A12345",ch)==NULL);

	pbreak();

	switch(ch) {
		case '1':
			name="Stone Breaker";
			price=50000;
			strength=75;
			break;
		case '2':
			name="Warrior Terminator";
			price=150000;
			strength=90;
			break;
		case '3':
			name="Magic Terrorizer";
			price=250000;
			strength=120;
			break;
		case '4':
			name="Evil Disposer";
			price=300000;
			strength=150;
			break;
		case '5':
			name="Heart Tracker";
			price=325000;
			strength=200;
			break;
		default:
			name=NULL;
	}
	if(name != NULL) {
		if(player->gold >= price) {
			player->poison=strength;
			decplayermoney(player,price);
			
			GOOD("Ok.");
			dlc(config.goodcolor, "You are now in possession of the powerful ", config.objectcolor, "poison.");
		}
		else
			BAD("You can't afford it!");
	}
}

static bool Chamber_Menu(void)
{
	char ch;
	int		x,i;

	if(onliner->location != ONLOC_Mystic) {
		ch='?';
		onliner->location = ONLOC_Mystic;
		strcpy(onliner->doing, location_desc(onliner->location));
	}
	else {
		PART("The Order (E,C,S,U,?) :");
		ch=toupper(gchar());
	}
	switch(ch) {
		case '?':
			pbreak();
			menu("(E)xamine book");
			menu("(C)reate poison");
			menu("(S)tatus");
			menu("(U)p");
			break;
		case 'S':
			newscreen();
			status(player);
			break;
		case 'C':
			Create_Poison();
			break;
		case 'E':
			pbreak();
			pbreak();
			TEXT("It's the list of all members");
			pbreak();
			dl(WHITE, "**-- Members of the Order --**", NULL);
			x = 0;
			for(i=0; i<MAX_PLAYERS; i++) {
				if(players[i].amember && !players[i].deleted) {
					x++;
					PLAYER(players[i].name2);
					TEXT("");
				}
			}

			for(i=0; i<MAX_NPCS; i++) {
				if(npcs[i].amember && !npcs[i].deleted) {
					x++;
					PLAYER(npcs[i].name2);
					TEXT("");
				}
			}

			dlc(config.textcolor, "The Order has a total of ", config.highlightcolor, commastr(x), config.textcolor, " members.");
			break;
		case 'U':
			return false;
	}
	
	return true;
}

static void Enter_Chamber(void)
{
	char	ch;
	char	str[256];

	TEXT("You enter the secret chamber behind the counter. "
			"You remove the old rug and open the trap-door. "
			"You set fire to a torch and descend down the staircase...");
	pbreak();
	upause();
	pbreak();
	TITLE("The Secret Alchemist Order");
	PART("You are standing in a chamber under the store. "
			"The stones in the walls are black as night. Some of them have inscriptions on them."
			"A large round table is placed in the middle of the room."
			"A book is laying on the table."
			"A fire is burning in the grate. In front of the fire an ");
	PLAYER("old man");
	TEXT(" is sitting in a rocking-chair. It looks if he's sleeping."
			"The man is holding a poker in his right hand."
			"There are two exits from here; up or down.");
	pbreak();

	ch='?';
	onliner->location = ONLOC_Mystic;
	strcpy(onliner->doing, location_desc(onliner->location));

	while(Chamber_Menu())
		;
	pbreak();
	pbreak();
	TEXT("You leave up...");
	pbreak();
	ch=' ';
}

static void Buy_Poison(void)
{
	char	*str;
	int		x,xx,zz,i;

	newscreen();
	pbreak();
	HEADER("Poisons currently available :");
	HEADER("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");

	x=xx=0;
	for(i=0; i<MAX_POISON; i++) {
		if(player->level > x) {
			x+=5;
			xx++;
			if(asprintf(&str, "%s. %.32s", commastr(i), commastr(poison[i].cost))<0)
				CRASH;
			dl(BRIGHT_BLUE, str, NULL);
			free(str);
		}
	}

	pbreak();
	dl(GREY, "Enter number to buy (1-", commastr(xx), ")", NULL);
	d(GREY, ":", NULL);

	zz=get_number(0, xx);
	if(zz > 0 && zz <= xx) {
		zz--;
		if(asprintf(&str, "Buy the recipe for %s", poison[zz].name)<0)
			CRASH;
		if(confirm(str, 'N')) {
			free(str);
			if(player->gold < poison[zz].cost) {
				PART("You don't have enough ");
				PART(config.moneytype);
				TEXT("!");
				pbreak();
				upause();
			}
			else {
				decplayermoney(player, poison[zz].cost);
				player->poison=poison[zz].strength;
				pbreak();
				TEXT("You receive the ingredients for the poison.");
				TEXT("After a few hours in the laboratory your poison is ready to be tested in the real world!");
				pbreak();
				if(asprintf(&str, " %s%s%s, the alchemist, bought poison!", uplc, player->name2, config.textcol1)<0)
					CRASH;
				newsy(true, "Beware!", str, NULL);
				free(str);
				upause();
			}
		}
		else {
			free(str);
		}
	}
	else
		pbreak();
}

static bool Menu(bool * refresh)
{
	char ch;

	// update online location, if necessary
	if(onliner->location != ONLOC_Alchemist) {
		*refresh=true;
		onliner->location = ONLOC_Alchemist;
		strcpy(onliner->doing, location_desc(onliner->location));
	}
	Display_Menu(true, true, refresh, name, expert_prompt, Meny);

	switch((ch=toupper(gchar()))) {
		case '?':	// Display Menu
			if(player->expert)
				Display_Menu(true, false, refresh, name, expert_prompt, Meny);
			else
				Display_Menu(false, false, refresh, name, expert_prompt, Meny);
			break;
		case 'S':	// Status
			newscreen();
			status(player);
			break;
		case 'T':	// The Secret Order
			pbreak();
			pbreak();
			if(player->level < 10) {
				PLAYER("Usilama");
				TEXT(" looks at you.");
				SAY("You are not yet worthy to join the Order!");
				SAY("You must at least be a level 10 Alchemist before we can deal with your application!");
			}
			else {
				if(!player->amember) {
					Join_Order();
				}
				else {
					Enter_Chamber();
				}
			}
			break;
		case 'B':
			Buy_Poison();
			break;
		case 'R':
			return false;
	}
	return true;
}

void Alchemisty(void)
{
	char	ch;
	bool	refresh=true;

	while(Menu(&refresh))
		;
}
