/*
 * TODO functions and constants used in current code
 */
#include <stdbool.h>
#include <inttypes.h>
#include "structs.h"

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
void d(int color, const char *text, ...);

/*
 * Displays a partial line (no trailing linefeed)
 * Each string has a colour
 */
void dc(int color, const char *text, ...);

/*
 * Displays a line (with trailing linefeed)
 */
void dl(int color, const char *text, ...);

/*
 * Displays a line (with trailing linefeed)
 * Each string has a colour
 */
void dlc(int color, const char *text, ...);

/*
 * Returns the next char pressed
 */
char gchar(void);

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
 * Displays a meny option "(B)uy Poison" for example with trailing newline
 */
void menu(const char *);

/*
 * Displays a meny option "(B)uy Poison" for example without trailing newline
 */
void menu2(const char *);

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

enum places {
	NoWhere,
	MainStreet,
	Slottet,
	Inn,
	Dormy,
	Prison,
	UmanCave
};

struct player {
	char		name1[71];	// BBS Name
	char		name2[71];	// Game name (used in news messages)
	bool		allowed;	// Can player today
	int			poison;		// Amount of poison
	bool		expert;		// Expert menus
	bool		auto_meny;	// Always display menus?
	int			level;
	bool		amember;	// Member of Alchemist brotherhood
	int			hps;		// Hit points (<1 is dead)
	int			gold;
	bool		deleted;
	enum places	auto_probe;	// When AutoProbe is set to a direction the player auto travels there
	uint16_t	head;
	uint16_t	body;
	uint16_t	arms;
	uint16_t	hands;
	uint16_t	legs;
	uint16_t	feet;
	uint16_t	face;
	uint16_t	abody;
	uint16_t	shield;
	bool		king;
	enum class	class;
};

#define MAX_PLAYERS	1024
#define MAX_NPCS	1024

struct config {
	// Integer colours
	int	textcolor;		// Normal text colour for display (same as textcol1)
	int	highlightcolor;	// Colour of highlights in textcolor
	int	textcolor2;		// Text colour "2" (normal for prompt chars)
	int	plycolor;		// Colour for player names
	int	talkcolor;		// Colour for phrases
	int	badcolor;		// Colour for bad stuff (You died, etc)
	int	goodcolor;		// Colour for good stuff (You won!, etc)
	int	headercolor;	// Colour for menu headers
	int	noticecolor;	// Colour for notices (slightly good/bad)
	int	monstercolor;	// Colour for monster names
	int	objectcolor;	// Colour for object names
	int	titlecolor;		// Colour for titles (ie: "The Secret Alchemist Order")
	int	menucolor;		// Colour for menus
	int placecolor;		// Colour for places
	int hotkeycolor;	// textcolor2
	int eventcolor;		// Events

	// String colours
	char	textcol1[3];
	char	textcol2[3];

	// Strings
	char	moneytype[23];	// Name of money (ie: Gold)
	char	moneytype2[23];
	char	moneytype3[23];	// Unknown... "can get it for 3,000 moneytype moneytype3" ("coins" maybe?)
	char	reese_name[23];	// Name of "Reese" from Armor shoppe
	char	groggo_name[23];// Name of "Groggo" from shady shoppe
	char	bobsplace[23];	// Name of Bobs Place

	// Allow/disallow
	bool	allow_drugs;
	bool	allow_steroids;
};

#ifdef getchar
#undef getchar
#endif
#define getchar()	(1 = 2)

extern struct config config;
#define global_plycol BRIGHT_GREEN
#define global_talkcol BRIGHT_MAGENTA
extern bool global_begged;
extern bool global_nobeg;
