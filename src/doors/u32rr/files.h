#ifndef _FILES_H_
#define _FILES_H_

#include <stdbool.h>
#include <inttypes.h>
#include <time.h>

#define MAX_POISON	22

struct poison {
	uint32_t	cost;
	uint16_t	strength;
	char		name[71];
};

enum race {
	Human,
	Hobbit,
	Elf,
	HalfElf,
	Dwarf,
	Troll,
	Orc,
	Gnome,
	Gnoll,
	Mutant
};

enum class {
	Alchemist,
	Assassin,
	Barbarian,
	Bard,
	Cleric,
	Jester,
	Magician,
	Paladin,
	Ranger,
	Sage,
	Warrior,
	MAXCLASSES
};

enum sex {
	Male,
	Female,
};

enum onloc {
	Nowhere,
	MainStreet,
	TheInn,
	DarkAlley,	// outside the shady shops
	Church,
	WeaponShop,
	Master,
	MagicShop,
	Dungeons,
	DeathMaze,
	MadMage		= 17,	// groggos shop, reached from shady shops
	ArmorShop,
	Bank,
	ReportRoom,
	Healer,
	MarketPlace,
	FoodStore,
	PlyMarket,
	Recruit,	// hall of recruitment, recruite.pas
	Dormitory,
	AnchorRoad,
	Orbs,
	OrbsMixing,	// mixing own drink at orbs bar
	OrbsBrowse,	// browsing drink file at orbs bar
	BobsBeer,	// Bobs Beer Hut
	onloc_Alchemist,
	Steroids,
	Drugs,
	Darkness,
	Whores,
	DarkerAlley,
	Gigalos,
	OutsideInn,
	OnARaid,
	TeamCorner,
	Mystic,
	RobbingBank,
	BountyRoom,
	ReadingNews,
	CheckPlys,
	Temple,		// altar of the gods
	BobThieves,
	BobDrink,	// beer drinking competiton
	UmanRest,
	UmanTame,
	UmanWRest,
	Innfight,
	DormFight,	// fight in the dormitory
	Entering,	// entering game
	GangPrep,	// attacking/viewing other teams
	ReadingMail,// reading mail, scanning news
	PostingMail,// posting mail/writing letter
	CombMaster,	// visiting Liu Zei, close combat master, at the market place
	DormFists,	// fistfight at the Dormitory
	Gymfists,	// fistfight at the Gym
	Temples,	// temple of the gods, temple.pas. 't' from the challenge menu
	TheGym,		// the gym
	MultiChat,	// Multi Node Chat at the Inn - IMPORTANT! when this location is entered then usurper will scan for IPC files at every "wait for key" state, see ddplus.pas
	OutsideGym,	// outside the gym
	GossipMonger,//{gossip monger [Lydia], at lovers.pas
	LoveHistory,// love history room, at lovers.pas
	BeggarsWall,// beggars wall, reached from the marketplace
	Castle		= 70,		// royal castle
	RoyalMail,	// reading royal mail (in the castle), scanning news
	CourtMage,	// visiting court magician
	WarChamber,	// visiting war chamber
	QuestMaster,// royal quest master
	QuestHall,	// player visiting quest hall
	QuestAttemp,// player attempts a [monster] quest
	RoyOrphanag,// royal orphanage
	GuardOffice	= 80,// players applying for guard jobs or quitting
	OutCastle,	// players outside the Castle, deciding what to do
	Prison		= 90,		// king visiting prison
	Prisoner,	// prisoners in their cells
	PrisonerOp,	// prisoner, but the cell door is open
	PrisonerEx,	// prisoner, execution
	PrisonWalk,	// outside the prison
	PrisonBreak,// outside the prison, attempting to liberate a prisoner
	LoveStreet	= 200,	// love street
	Home,		// managing family affairs
	Nursery,	// in the childrens room
	Kidnapper,	// home and maintaining kidnapped children
	GiftShop,	// visiting gift-shop
	IceCaves	= 300,
	Heaven		= 400,
	HeavenBoss,	// visiting boss god
	Closed		= 30000,	// used by fakeplayers when deciding where to go. see 'online.pas' and procedure 'give_me_exits_from'
};

enum onlinetype {
	Player,
	God,
	Child,
	Fake
};

#define MAXNOD		5

struct onliner {
	char			name[31];		// Player Alias
	char			realname[31];	// BBS Name
	char			node[4];		// Node as a three character string (zero padded)
	time_t			arrived;		// Date of arrival
	enum onlinetype	usertype;		// Player/God/Child
	bool			fake;			// a fake player is acting like a real player and will stay as long as the player who created him
	//uint16_t		recnr;			// Record number... shouldn't be neede.
	bool			shadow;			// if true then this character is computer controlled
	//bool			dead;			// is this being used?? check this one out jakob! i don't think its being used
	enum race		race;
	enum class		class;
	enum sex		sex;
	char			initiator[32];	// who created this record, could be a "GHOST"
	char			doing[41];		// status of player, what is he doing
	char			chatline[MAXNOD][91];
	char			chatsend[MAXNOD][91];
	char			info[MAXNOD][91];
	char			infosend[MAXNOD][31];
	uint8_t			ear;			// how much info does the player want to get when online, see cms.pas and ear constants
	char			bname[31];		// online opponents name
	char			comfile[91];	// name of file in which online comm will take place
	char			com;			// used in player<=>player online routines
	enum onloc		location;		// location in game, see onloc_?? constants
};

#define MAX_OBJECTS	1024

enum objtype {
	Head,
	Body,
	Arms,
	Hands,
	Fingers,
	Legs,
	Feet,
	Waist,
	Neck,
	Face,
	Shield,
	Food,
	Drink,
	Weapon,
	Abody
};

enum cures {
	Nothing,
	All,
	Blindness,
	Plague,
	Smallpox,
	Measles,
	Leprosy
};

struct object {
	char			name[71];
	enum objtype	ttype;
	long			value;
	int				hps;
	int				stamina;
	int				agility;
	int				charisma;
	int				dex;
	int				wisdom;
	int				mana;
	int				armor;
	int				attack;
	char			owned[71];
	bool			onlyone;
	enum cures		cure;
	bool			shop;
	bool			dng;					// can you find the item in the dungeons
	bool			cursed;
	int				minlev;
	int				maxlev;
	char			desc1[5][71];			// normal description
	char			desc2[5][71];			// detailed description
	int				strength;
	int				defence;
	int				str_need;
	bool			good;					// does character need to be good to use item?
	bool			evil;					// does character need to be evil to use item?
	bool			restr[MAXCLASSES];
};

extern struct poison	*poison;
extern struct onliner	*onliners;
extern struct onliner	*onliner;
extern struct player	*players;
extern struct player	*player;
extern struct player	*npcs;
extern struct object	*objects;

#endif
