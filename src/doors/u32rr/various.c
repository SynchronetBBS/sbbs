/*
 * Various functions
 */

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

