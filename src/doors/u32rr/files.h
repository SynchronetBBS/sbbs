#ifndef _FILES_H_
#define _FILES_H_

#include <stdbool.h>
#include <time.h>
#include <unistd.h>

#include "Races.h"
#include "Classes.h"

#define MAX_POISON	21

struct poison {
	int			cost;
	int			strength;
	char		name[71];
};

enum sex {
	Male,
	Female,
};

enum onloc {
	ONLOC_Nowhere,
	ONLOC_MainStreet,
	ONLOC_TheInn,
	ONLOC_DarkAlley,	// outside the shady shops
	ONLOC_Church,
	ONLOC_WeaponShop,
	ONLOC_Master,
	ONLOC_MagicShop,
	ONLOC_Dungeons,
	ONLOC_DeathMaze,
	ONLOC_MadMage		= 17,	// groggos shop, reached from shady shops
	ONLOC_ArmorShop,
	ONLOC_Bank,
	ONLOC_ReportRoom,
	ONLOC_Healer,
	ONLOC_MarketPlace,
	ONLOC_FoodStore,
	ONLOC_PlyMarket,
	ONLOC_Recruit,	// hall of recruitment, recruite.pas
	ONLOC_Dormitory,
	ONLOC_AnchorRoad,
	ONLOC_Orbs,
	ONLOC_OrbsMixing,	// mixing own drink at orbs bar
	ONLOC_OrbsBrowse,	// browsing drink file at orbs bar
	ONLOC_BobsBeer,	// Bobs Beer Hut
	ONLOC_Alchemist,
	ONLOC_Steroids,
	ONLOC_Drugs,
	ONLOC_Darkness,
	ONLOC_Whores,
	ONLOC_DarkerAlley,
	ONLOC_Gigalos,
	ONLOC_OutsideInn,
	ONLOC_OnARaid,
	ONLOC_TeamCorner,
	ONLOC_Mystic,
	ONLOC_RobbingBank,
	ONLOC_BountyRoom,
	ONLOC_ReadingNews,
	ONLOC_CheckPlys,
	ONLOC_Temple,		// altar of the gods
	ONLOC_BobThieves,
	ONLOC_BobDrink,	// beer drinking competiton
	ONLOC_UmanRest,
	ONLOC_UmanTame,
	ONLOC_UmanWRest,
	ONLOC_Innfight,
	ONLOC_DormFight,	// fight in the dormitory
	ONLOC_Entering,	// entering game
	ONLOC_GangPrep,	// attacking/viewing other teams
	ONLOC_ReadingMail,// reading mail, scanning news
	ONLOC_PostingMail,// posting mail/writing letter
	ONLOC_CombMaster,	// visiting Liu Zei, close combat master, at the market place
	ONLOC_DormFists,	// fistfight at the Dormitory
	ONLOC_GymFists,	// fistfight at the Gym
	ONLOC_Temples,	// temple of the gods, temple.pas. 't' from the challenge menu
	ONLOC_TheGym,		// the gym
	ONLOC_MultiChat,	// Multi Node Chat at the Inn - IMPORTANT! when this location is entered then usurper will scan for IPC files at every "wait for key" state, see ddplus.pas
	ONLOC_OutsideGym,	// outside the gym
	ONLOC_GossipMonger,//{gossip monger [Lydia], at lovers.pas
	ONLOC_LoveHistory,// love history room, at lovers.pas
	ONLOC_BeggarsWall,// beggars wall, reached from the marketplace
	ONLOC_Castle		= 70,		// royal castle
	ONLOC_RoyalMail,	// reading royal mail (in the castle), scanning news
	ONLOC_CourtMage,	// visiting court magician
	ONLOC_WarChamber,	// visiting war chamber
	ONLOC_QuestMaster,// royal quest master
	ONLOC_QuestHall,	// player visiting quest hall
	ONLOC_QuestAttemp,// player attempts a [monster] quest
	ONLOC_RoyOrphanag,// royal orphanage
	ONLOC_GuardOffice	= 80,// players applying for guard jobs or quitting
	ONLOC_OutCastle,	// players outside the Castle, deciding what to do
	ONLOC_Prison		= 90,		// king visiting prison
	ONLOC_Prisoner,	// prisoners in their cells
	ONLOC_PrisonerOp,	// prisoner, but the cell door is open
	ONLOC_PrisonerEx,	// prisoner, execution
	ONLOC_PrisonWalk,	// outside the prison
	ONLOC_PrisonBreak,// outside the prison, attempting to liberate a prisoner
	ONLOC_LoveStreet	= 200,	// love street
	ONLOC_Home,		// managing family affairs
	ONLOC_Nursery,	// in the childrens room
	ONLOC_Kidnapper,	// home and maintaining kidnapped children
	ONLOC_GiftShop,	// visiting gift-shop
	IONLOC_ceCaves	= 300,
	ONLOC_Heaven		= 400,
	ONLOC_HeavenBoss,	// visiting boss god
	ONLOC_Closed		= 30000,	// used by fakeplayers when deciding where to go. see 'online.pas' and procedure 'give_me_exits_from'
};

enum onlinetype {
	Player,
	God,
	Child,
	Fake
};

#define MAXNOD		5
#define MAX_ONLINERS	1000

enum ear {
	ear_all,
	ear_personal,
	ear_quiet
};

struct onliner {
	bool			online;
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
	enum ear		ear;			// how much info does the player want to get when online, see cms.pas and ear constants
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
	ABody,
	TOTAL_OBJTYPES
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

struct armor {
	char			name[31];
	long			val;
	long			pow;
};

struct weapon {
	char			name[31];
	long			value;
	long			pow;
};

enum aitype {
	AI_Computer,
	AI_Human
};

enum alignment {
	AlignAll,
	AlignGood,
	AlignEvil,
	AlignNone
};

#define KINGGUARDS	5
struct king {
	char			name[31];
	enum aitype		ai;
	enum sex		sexy;
	long			daysinpower;
	char			tax;			// Percentage
	enum alignment	taxalignment;
	long			treasury;
	int				prisonsleft;	// # of people king can imprison today. new every day
	int				executeleft;	// # of death sentences left today. new every day
	int				questsleft;		// # of new quests the king can issue today
	int				marryactions;	// # of marriages the king can interfer in today
	int				wolffeed;		// # kids have can be tossed to the wolves/day, set to config.allowfeedingthewolves at maint
	int				royaladoptions;	// # kids can be placed in the Royal Orphanage/day, set to config.allowRoyalAdoption at maint
	char			moatid[16];		// unique moat creature ID
	int				moatnr;			// how many crocodiles (or whatever) in the moat?
	char			guard[KINGGUARDS][31];	// king body guards, name
	long			guardpay[KINGGUARDS];	// king body guards, salary
	enum aitype		guardai[KINGGUARDS];	// king body guards, ai
	enum sex		guardsex[KINGGUARDS];	// king body guards, sex
	/* Shops open... */
	bool			shop_weapon;
	bool			shop_armor;
	bool			shop_magic;
	bool			shop_alabat;
	bool			shop_plmarket;
	bool			shop_healing;
	bool			shop_drugs;
	bool			shop_steroids;
	bool			shop_orbs;
	bool			shop_evilmagic;
	bool			shop_bobs;
	bool			shop_gigolos;
};

#define MAXMSPELLS	6
struct monster {
	char	name[31];	// Name of creature
	long	weapnr;		// Weapon (from weapon data file) used
	long	armnr;		// Armour (from data file) used
	bool	grabweap;	// Can weapon be taken
	bool	grabarm;	// Car armour be taken
	char	phrase[71];	// Intro phrase from monster
	int		magicres;	// Magic resistance
	int		strength;
	int		defence;
	bool	wuser;		// Weapon User
	bool	auser;		// Armour User
	long	hps;
	char	weapon[41];	// Name of weapon
	char	armor[41];	// Name of armour
	bool	disease;	// Infected by a disease?
	long	weappow;	// Weapon Power
	long	armpow;		// Armour power
	int	iq;
	int		evil;		// Evilne4ss (0-100%)
	int		magiclevel;	// The higher this is, the better the magic is
	int		mana;		// Manna remaining
	int		maxmana;
	bool	spell[MAXMSPELLS];	// Monster Spells

	long	punch;		// Temporary battle var(!)
	bool	poisoned;	// Temporary battle var(!)
	int	target;		// Temp. battle var
};

struct level {
	int		xpneed;
};

struct map_file {
	int		fd;
	off_t	len;
	void	*data;
};

extern struct map_file poison_map;
extern struct poison	*poison;
extern struct map_file player_map;
extern struct player	*player;
extern struct player	*players;
extern struct map_file npc_map;
extern struct player	*npcs;
extern struct map_file onliner_map;
extern struct onliner	*onliner;
extern struct onliner	*onliners;
extern struct map_file king_map;
extern struct king		*king;

extern struct map_file abody_map;
extern struct object	*abody;

extern struct map_file armor_map;
extern struct armor	*armor;

extern struct map_file arms_map;
extern struct object	*arms;

extern struct map_file body_map;
extern struct object	*body;

extern struct map_file drink_map;
extern struct object	*drink;

extern struct map_file face_map;
extern struct object	*face;

extern struct map_file feets_map;
extern struct object	*feets;

extern struct map_file food_map;
extern struct object	*food;

extern struct map_file hands_map;
extern struct object	*hands;

extern struct map_file head_map;
extern struct object	*head;

extern struct map_file legs_map;
extern struct object	*legs;

extern struct map_file neck_map;
extern struct object	*neck;

extern struct map_file rings_map;
extern struct object	*rings;

extern struct map_file shields_map;
extern struct object	*shields;

extern struct map_file waist_map;
extern struct object	*waist;

extern struct map_file weapons_map;
extern struct object	*weapons;

extern struct map_file weapon_map;
extern struct weapon	*weapon;

extern struct map_file monster_map;
extern struct monster	*monster;

extern struct map_file level_map;
extern struct level	*levels;


void open_files(void);
void map_file(char *name, struct map_file *map, size_t len);
void unmap_file(struct map_file *map);

#endif
