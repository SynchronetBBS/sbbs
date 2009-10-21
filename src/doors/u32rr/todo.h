/*
 * TODO functions and constants used in current code
 */
#include <stdbool.h>
#include <inttypes.h>
#include "structs.h"

/*
 * Formats a number with commas into a malloc()ed buffer
 */
char *acommastr(int64_t num);

/*
 * Sets input hotkeys
 */
#define NoKill	0	// Type for sethotkeys_on (Delete?)
void sethotkeys_on(int type, char *chars);

/*
 * Describes a location
 */
const char *location_desc(enum onloc);

/*
 * Player VS Mosters
 */
enum pl_vs_type {
	Pl_Vs_Monster,
	Pl_Vs_DoorGuards,
	Pl_Vs_Supreme,
	Pl_Vs_Demon,
	Pl_Vs_Alchemist,
	Pl_Vs_PrisonGuards,
};
void player_vs_monsters(enum pl_vs_type type, struct monster **mosnters, struct player **players);

/*
 * News message... NULL terminated
 */
void newsy(bool, ...);

/*
 * Paragraph break (newline)
 */
#define pbreak()	TEXT("")

/*
 * Displays a partial line (no trailing linefeed)
 */
void d_part(int color, const char *text);

/*
 * Displays a comma formatted number (no trailing lnefeed)
 */
void d_comma(int color, long number);

/*
 * Displays a line (with trailing linefeed)
 */
void d_line(int color, const char *text);

/*
 * Does the pause thing
 */
void upause(void);

/*
 * Exits game normally
 */
void halt(void);

/*
 * Starts a new screen (clear screen)
 */
void newscreen(void);

/*
 * Displays a meny option "(B)uy Poison" for example
 */
void menu(const char *);

/*
 * Y/N prompt
 */
char confirm(const char *player, char dflt);

/*
 * Random integer less than parameter
 */
int rand_num(int limit);

/*
 * Display status for specified player
 */
void status(struct player *);

void reduce_player_resurrections(struct player *, bool);

void objekt_affect(int, uint16_t, enum objtype, struct player *player, bool loud);

/*
 * Most likely not needed...
 * Force write, unlock area(?)
 */
void user_save(struct player *player);

void decplayermoney(struct player *player, int amount);

int get_number(int min, int max);

extern char *uplc;	// Colour string for player name in messages
extern char *uitemc;	// Colour string for items in messages

struct player {
	char	name2[70];	// Alias? (used in news messages)
	bool	allowed;	// Can player today
	int	poison;		// Amount of poison
	bool	expert;		// Expert menus
	bool	auto_meny;	// Always display menus?
	int	level;
	bool	amember;	// Member of Alchemist brotherhood
	int	hps;		// Hit points (<1 is dead)
	int	gold;
	bool	deleted;
	uint16_t	head;
	uint16_t	body;
	uint16_t	arms;
	uint16_t	hands;
	uint16_t	legs;
	uint16_t	feet;
	uint16_t	face;
	uint16_t	abody;
	uint16_t	shield;
};

#define MAX_PLAYERS	1024
#define MAX_NPCS	1024

struct config {
	int	textcolor;	// Normal text colour for display (same as textcol1)
	char	textcol1[6];	// Text colour "1" (normal for news)
	char	textcol2[6];	// Text colour "2" (normal for prompt chars)
	char	moneytype[23];	// Name of money (ie: Gold)
	char	moneytype3[23];	// Unknown... "can get it for 3,000 moneytype moneytype3" ("coins" maybe?)
	char	reese_name[23];	// Name of "Reese" from Armor shoppe
};

struct config config;
#define global_plycol BRIGHT_GREEN
#define global_talkcol BRIGHT_MAGENTA
extern bool global_begged;
extern bool global_nobeg;
