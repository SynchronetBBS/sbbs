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


#ifdef getchar
#undef getchar
#endif
#define getchar()	(1 = 2)

extern struct config config;
#define global_plycol BRIGHT_GREEN
#define global_talkcol BRIGHT_MAGENTA
extern bool global_begged;
extern bool global_nobeg;
