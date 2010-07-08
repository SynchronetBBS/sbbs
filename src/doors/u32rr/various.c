/*
 * Various functions
 */

#include <sys/limits.h>

#include "IO.h"
#include "Config.h"
#include "files.h"
#include "macros.h"

#include "todo.h"

const char *color[11] = {
	"white",
	"red",
	"blue",
	"green",
	"brown",
	"black",
	"purple",
	"grey",
	"blue",
	"yellow",
	"white"
};

void Display_Menu(bool force, bool terse, bool *refresh, const char *name, const char *expert_prompt, void (*Meny)(void *), void *cbdata)
{
	if(terse) {
		if(!player->expert) {
			if(*refresh && player->auto_meny) {
				*refresh=false;
				Meny(cbdata);
			}
			nl();
			D(config.textcolor, name, " (", config.hotkeycolor, "?", config.textcolor, " for menu) :");
		}
		else {
			nl();
			TEXT(name, " ", expert_prompt, " :");
		}
	}
	else {
		if((!player->expert) || force) {
			Meny(cbdata);
		}
	}
}

void Reduce_Player_Resurrections(struct player *pl, bool typeinfo)
{
	if(pl->resurrections > 0)
		pl->resurrections--;
	if(pl->resurrections < 0)
		pl->allowed=false;
	if(typeinfo)
		DL(config.textcolor, "You have ", white, commastr(pl->resurrections), config.textcolor, " resurrection", pl->resurrections>1?"s":"", " left today.");
}

long level_raise(int level, long exp)
{
	if(levels[level].xpneed <= exp)
		return(0);
	return(levels[level].xpneed-exp);
}

struct object *items(enum objtype type)
{
	switch(type) {
		case Head:
			return head;
		case Body:
			return body;
		case Arms:
			return arms;
		case Hands:
			return hands;
		case Fingers:
			return rings;
		case Legs:
			return legs;
		case Feet:
			return feets;
		case Waist:
			return waist;
		case Neck:
			return neck;
		case Face:
			return face;
		case Shield:
			return shields;
		case Food:
			return food;
		case Drink:
			return drink;
		case Weapon:
			return weapons;
		case ABody:
			return abody;
		default:
			CRASH;
	};
}

const char *immunity(int val)
{
	if(val > 250)
		return "*IMMUNE*";
	if(val > 150)
		return "EXTREME";
	if(val > 90)
		return "very strong";
	if(val > 70)
		return "strong";
	if(val > 40)
		return "above average";
	if(val > 28)
		return "average";
	if(val > 10)
		return "medium";
	if(val > 4)
		return "poor";
	return "very poor";
}

void decplayermoney(struct player *pl, long coins)
{
	pl->gold -= coins;
	if(pl->gold < 0)
		pl->gold=0;
}

void incplayermoney(struct player *pl, long coins)
{
	pl->gold += coins;
	if(pl->gold < 0)
		pl->gold=INT_MAX;
}

void Quick_Healing(struct player *pl)
{
	int	quaff;
	int	regain;

	nl();

	if(pl->hps >= pl->maxhps)
		DL(yellow, "You don't need healing.");

	if(pl->hps < pl->maxhps && pl->healing==0)
		DL(lred, "You need healing, but don't have any potions!");

	if(pl->hps < pl->maxhps && pl->healing > 0) {
		quaff=(pl->maxhps-pl->hps)/5;
		regain=quaff*5;
		if(pl->hps + regain < pl->maxhps)
			quaff++;

		DL(config.textcolor, "You need ", yellow, commastr(quaff), config.textcolor, quaff==1?" potion.":" potions.");

		if(quaff>pl->healing)
			quaff=pl->healing;
		pl->healing -= quaff;
		regain=quaff*5;
		if(pl->hps+regain > pl->maxhps)
			regain=pl->maxhps-pl->hps;
		pl->hps += regain;
		DL(config.textcolor, "You quaffed ", white, commastr(quaff), config.textcolor, quaff==1?" potion":" potions", " and regained ",white,commastr(regain), config.textcolor, " hitpoints.");
		DL(config.textcolor, "You have ", white, commastr(pl->healing), pl->healing==1?" potion":" potions", " left.");
	}
}

void Healing(struct player *pl)
{
	int	quaff, regain;

	nl();

	if(pl->hps == pl->maxhps)
		DL(config.textcolor, "You don't need healing.");

	if(pl->hps < pl->maxhps) {
		DL(config.textcolor, "You have ",white,commastr(pl->healing),config.textcolor, " healing potions.");
		DL(config.textcolor, "Quaff how many potions");
		D(config.textcolor, ":");

		quaff=get_number(0,pl->healing);

		if(quaff <= pl->healing && quaff > 0) {
			regain=quaff*5;
			if(regain+pl->hps > pl->maxhps) {
				if(regain+pl->hps > pl->maxhps+4) {
					quaff=(pl->maxhps - pl->hps)/5;
					regain=quaff*5;
					if(pl->hps+regain < pl->maxhps) {
						quaff++;
					}
					regain=quaff*5;
					DL(lred, "You only need ", white, commastr(quaff), lred, quaff==1?" potion.":" potions.");
				}
				if(regain+pl->hps > pl->maxhps)
					regain=pl->maxhps - pl->hps;
			}
			pl->healing -= quaff;
			pl->hps += regain;
			DL(config.textcolor, "You quaffed ",white,commastr(quaff),config.textcolor, quaff==1?" potion":" potions");
			DL(config.textcolor, "You regained ",white,commastr(regain),config.textcolor, regain==1?" hitpoint  (":" hitpoints  (",commastr(pl->hps),"/",commastr(pl->maxhps),")");
			DL(config.textcolor, "You have ",white,commastr(pl->healing),config.textcolor, pl->healing==1?" potion left":"potions left");
		}
	}
}

int HitChance(int skill)
{
	switch(skill) {
		case 0:
			return 7;
		case 1:
		case 2:
		case 3:
			return 6;
		case 4:
		case 5:
		case 6:
			return 5;
		case 7:
		case 8:
		case 9:
		case 10:
			return 4;
		case 11:
		case 12:
		case 13:
		case 14:
			return 3;
		case 15:
		case 16:
		case 17:
			return 2;
	}
	return(8);
}
