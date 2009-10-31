#include <string.h>

#include "Config.h"
#include "IO.h"

struct config config;

void DefaultConfig()
{
	config.textcolor=green;	// Normal text colour for display (same as textcol1)
	config.highlightcolor=magenta;	// Text colour "2" (normal for prompt chars)
	config.plycolor=lgreen;	// Colour for player names
	config.talkcolor=lmagenta;	// Colour for phrases
	config.badcolor=lred;		// Colour for bad stuff (You died, etc)
	config.goodcolor=white;	// Colour for good stuff (You won!, etc)
	config.headercolor=magenta;	// Colour for menu headers
	config.noticecolor=cyan;	// Colour for notices (slightly good/bad)
	config.monstercolor=magenta;	// Colour for monster names
	config.objectcolor=magenta;	// Colour for object names
	config.titlecolor=lgreen;	// Colour for titles (ie: "The Secret Alchemist Order")
	config.menucolor=lblue;	// Colour for menus
	config.placecolor=lcyan;	// Colour for places
	config.hotkeycolor=magenta;	// textcolor2
	config.eventcolor=lgreen;	// Events
	config.moneycolor=yellow;
	config.bracketcolor=green;	// Use by menu()
	config.teamcolor=cyan;
	config.bashcolor=cyan;
	config.itemcolor=lcyan;
	config.umailheadcolor=cyan;

	// Strings
	strcpy(config.moneytype, "gold");	// Name of money (ie: Gold)
	strcpy(config.moneytype2, "coin");
	strcpy(config.moneytype3, "coins");
	strcpy(config.reese_name, "Reese");
	strcpy(config.groggo_name, "Groggo");
	strcpy(config.bobsplace, "Bob's Place");
	strcpy(config.returnenter, "Return");

	// Allow/disallow
	config.allow_drugs=true;
	config.allow_steroids=true;
	memset(config.allowitem, 0xff, sizeof(config.allowitem));

	// Gameplay
	config.classic=false;
}
