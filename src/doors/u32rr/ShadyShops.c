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


// Usurper - Shady Shops
#include <ctype.h>
#include <string.h>

#include "macros.h"
#include "files.h"
#include "various.h"

#include "Alchemisty.h"

#include "todo.h"

static const char *name="Shady Shops";
static const char *expert_prompt="(B,A,G,O,S,D,R,?)";

static void Meny(void)
{
	const int offset = 25;

	newscreen();
	pbreak();
	HEADER("-*- Shady Shops -*-");
	pbreak();
	TEXT("You stumble in to the dark areas of the town. "
			"It is here where you can get what you want, "
			"without any questions being asked. Trouble "
			"is never far away in these neighbourhood.");
	pbreak();

	menu2(ljust("(D)rug Palace", offset));
	menu("(S)teroid Shop");

	menu2(ljust("(O)rbs Health Club", offset));
	menu(Asprintf("(G) %s%s%s Magic Services", uplc, config.groggo_name, config.textcol1));

	menu2(ljust("(B)eer Hut", offset));
	menu("(A)lchemists Heaven");

	menu("(R)eturn to street");
}

static bool Menu(bool * refresh)
{
	char cho=0;

	if(onliner->location != ONLOC_DarkAlley) {
		*refresh=true;
		onliner->location=ONLOC_DarkAlley;
		strcpy(onliner->doing, location_desc(onliner->location));
	}
	// auto-travel
	switch(player->auto_probe) {
		case NoWhere:
			Display_Menu(true, true, refresh, name, expert_prompt, Meny);
			cho=toupper(gchar());
			break;
		case UmanCave:
		case MainStreet:
		case Slottet:
		case Inn:
		case Dormy:
		case Prison:
			cho='R';
			break;
	}

	// Filter out disabled options
	if(cho=='D' && (!config.allow_drugs)) {
		pbreak();
		BAD("Drugs are banned in this game.");
		upause();
		cho=' ';
	}
	else if(cho=='S' && (!config.allow_steroids)) {
		pbreak();
		BAD("Steroids are banned in this game.");
		upause();
		cho=' ';
	}

	switch(cho) {
		case '?':
			if(player->expert)
				Display_Menu(true, false, refresh, name, expert_prompt, Meny);
			else
				Display_Menu(false, false, refresh, name, expert_prompt, Meny);
			break;
		case 'R':	// Return
			return false;
			break;
		case 'O':	// Orbs drink cener
			if((!king->shop_orbs) && (!player->king)) {
				pbreak();
				dl(config.badcolor, "Orbs Health Club is closed! (The ", upcasestr(kingstring(king->sexy)), "s order!", NULL);
			}
			else {
				pbreak();
				pbreak();
				TEXT("You decide to enter this somewhat dubious place.");
				Orb_Center();
			}
			break;
		case 'A':	// alchemist secret order
			if(player->class != Alchemist) {
				pbreak();
				dl(MAGENTA, "The guards outside the building humiliate you and block the entrance.", NULL);
				dl(MAGENTA, "It seems as only Alchemists are allowed.", NULL);
			}
			else {
				Alchemisty();
			}
			break;
		case 'B':	// Bobs Beer Hut
			if((!king->shop_bobs) && (!player->king)) {
				pbreak();
				BAD(config.bobsplace, " is closed! (The ", upcasestr(kingstring(king->sexy)), "s order!)");
			}
			else {
				pbreak();
				pbreak();
				dlc(config.textcolor, "You enter ", config.placecolor, config.bobsplace, D_DONE);
				Bobs_Inn();
			}
			break;
		case 'G':
			if((!king->shop_evilmagic) && (!player->king)) {
				pbreak();
				BAD(config.groggo_name, "s place is closed! (The ", upcasestr(kingstring(king->sexy)), "s order!");
			}
			else {
				Groggos_Magic();
			}
			break;
		case 'D':	// Drugs
			if((!king->shop_drugs) && (!player->king)) {
				pbreak();
				BAD("The Drug Palace is closed! (The ", upcasestr(kingstring(king->sexy)), "s order!)");
			}
			else {
				Drug_Store();
			}
			break;
		case 'S':	// Steroids
			if((!king->shop_steroids) && (!player->king)) {
				pbreak();
				BAD("The Steroid Shop is closed! (The ", upcasestr(kingstring(king->sexy)), "s order!)");
			}
			else {
				Steroid_Store();
			}
			break;
	}
	return true;
}

void Shady_Shops(void)
{
	bool refresh=false;

	while(Menu(&refresh))
		;

	pbreak();
}
