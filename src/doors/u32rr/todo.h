#ifndef _TODO_H_
#define _TODO_H_

/*
 * TODO functions and constants used in current code
 */
#include <stdbool.h>
#include <inttypes.h>
#include "Bash.h"
#include "structs.h"
#include "files.h"

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

void objekt_affect(int, uint16_t, enum objtype, struct player *player, bool loud);

/*
 * Most likely not needed...
 * Force write, unlock area(?)
 */
void user_save(struct player *player);

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

#define MAXITEM			15	// Most items you can carry at a time...
#define MAXSPELLS		12	// Max spells available
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
	int			head;
	int			body;
	int			arms;
	int			hands;
	int			legs;
	int			feet;
	int			face;
	int			abody;
	int			shield;
	int			lhand;
	int			neck;
	int			neck2;
	int			rfinger;
	int			lfinger;
	int			waist;
	bool		king;
	enum class	class;
	enum sex	sex;
	unsigned long	exp;
	enum race	race;
	int			weapon;
	int			armor;
	int			trains;
	int			age;
	int			bankgold;
	int			healing;
	int			maxhps;
	int			strength;
	int			defence;
	char		team[26];
	int			wpow;
	int			apow;
	int			mana;
	int			maxmana;
	int			pickpocketattempts;
	int			rhand;
	bool		blind;
	bool		plague;
	bool		smallpox;
	bool		measles;
	bool		leprosy;
	int			skill[BASH_COUNT];
	char		beendefeated[71];
	char		begging[71];
	char		battlecry[71];
	char		spareopp[71];
	char		dontspareopp[71];
	char		attacked[71];
	char		defeatedopp[71];
	bool		autohate;
	bool		autoheal;
	enum ear	ear;
	char		desc[4][71];
	bool		automeny;
	int			item[MAXITEM];
	enum objtype	itemtype[MAXITEM];
	int			wisdom;
	int			dark;
	int			charisma;
	int			fights;
	int			agility;
	int			tfights;
	int			dex;
	int			chiv;
	int			stamina;
	int			disres;
	int			addict;
	int			mental;
	int			height;
	int			hair;
	int			weight;
	int			eyes;
	int			skin;
	int			m_kills;
	int			m_defeats;
	int			p_kills;
	int			p_defeats;
	int			resurrections;
	bool		spell[MAXSPELLS][2];
	enum aitype	ai;
	int			darknr;		// Dark Deeds remaining
	int			thiefs;
	int			brawls;
	int			chivnr;		// Good deeds left
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

enum mailaction {
	MailRead,
	MailSend
};

enum mailspecial {
	MAILREQUEST_Nothing,
	MAILREQUEST_BeMyGuard,
	MAILREQUEST_IWantGuard,
	MAILREQUEST_DrinkOffer,

	// Immortal
	MAILREQUEST_ImmortalOffer = 40,
	MAILREQUEST_RoyalAngel = 50,
	MAILREQUEST_RoyalAvenger,
	MAILREQUEST_QuestOffer = 60,
	MAILREQUEST_Birthday,

	// Relationship
	MAILREQUEST_HoldHands = 70,
	MAILREQUEST_Roses,
	MAILREQUEST_Poison,
	MAILREQUEST_Dinner,
	MAILREQUEST_Scorpions,
	MAILREQUEST_Chocolate,
	MAILREQUEST_Kiss,
	MAILREQUEST_HaveSex,
	MAILREQUEST_HaveDiscreteSex,
	MAILREQUEST_ScanForBabies,
	MAILREQUEST_ChildRaisingExp,
	MAILREQUEST_ChildPoisonedExp,
	MAILREQUEST_ChildFightExp,
	MAILREQUEST_SilentExp,
	MAILREQUEST_ChildCursedExp,
	MAILREQUEST_ChildDepressedExp,
	MAILREQUEST_GymMembership = 89,
	MAILREQUEST_JoinTeam
};
void Post(enum mailaction, const char *to, enum aitype toai, bool togod, enum mailspecial special, const char *from, ...);
void Brawl(void);
void Drinking_Competition(void);


#endif
