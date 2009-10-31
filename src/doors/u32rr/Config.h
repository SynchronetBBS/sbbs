#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdbool.h>

enum allow_items {
	ALLOW_Hand,
	ALLOW_Head,
	ALLOW_Body,
	ALLOW_Arm,
	ALLOW_LeftFinger,
	ALLOW_RightFinger,
	ALLOW_Leg,
	ALLOW_Feet,
	ALLOW_Waist,
	ALLOW_Neck1,
	ALLOW_Neck2,
	ALLOW_Face,
	ALLOW_Shield,
	ALLOW_AroundBody,
	ALLOW_SecondaryWeapon,
	ALLOW_TOTAL
};

struct config {
	// Integer colours
	const char *textcolor;		// Normal text colour for display (same as textcol1)
	const char *highlightcolor;	// Colour of highlights in textcolor
	const char *textcolor2;		// Text colour "2" (normal for prompt chars)
	const char *plycolor;		// Colour for player names
	const char *talkcolor;		// Colour for phrases
	const char *badcolor;		// Colour for bad stuff (You died, etc)
	const char *goodcolor;		// Colour for good stuff (You won!, etc)
	const char *headercolor;	// Colour for menu headers
	const char *noticecolor;	// Colour for notices (slightly good/bad)
	const char *monstercolor;	// Colour for monster names
	const char *objectcolor;	// Colour for object names
	const char *titlecolor;		// Colour for titles (ie: "The Secret Alchemist Order")
	const char *menucolor;		// Colour for menus
	const char *placecolor;		// Colour for places
	const char *hotkeycolor;	// textcolor2
	const char *eventcolor;		// Events
	const char *moneycolor;
	const char *bracketcolor;
	const char *teamcolor;
	const char *bashcolor;
	const char *itemcolor;
	const char *umailheadcolor;

	// Strings
	char	moneytype[23];	// Name of money (ie: Gold)
	char	moneytype2[23];
	char	moneytype3[23];	// Unknown... "can get it for 3,000 moneytype moneytype3" ("coins" maybe?)
	char	reese_name[23];	// Name of "Reese" from Armor shoppe
	char	groggo_name[23];// Name of "Groggo" from shady shoppe
	char	bobsplace[23];	// Name of Bobs Place
	char	returnenter[23];// Name of return/enter key

	// Allow/disallow
	bool	allow_drugs;
	bool	allow_steroids;
	bool	allowitem[ALLOW_TOTAL];

	// Gameplay
	bool	classic;
};

extern struct config config;

#endif
