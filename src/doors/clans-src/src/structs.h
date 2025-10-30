#ifndef THE_CLANS__STRUCTS___H
#define THE_CLANS__STRUCTS___H

#include "defines.h"

struct IniFile {
	bool Initialized;

	char *pszLanguage;
	char *pszNPCFileName[MAX_NPCFILES];
	char *pszSpells[MAX_SPELLFILES];
	char *pszItems[MAX_ITEMFILES];
	char *pszRaces[MAX_RACEFILES];
	char *pszClasses[MAX_CLASSFILES];
	char *pszVillages[MAX_VILLFILES];
};


struct NodeData {
	int number;
	char dropDir[1024];
	bool fossil;
	uintptr_t addr;
	int irq;
};


struct config {
	char *pszInfoPath;
	char szSysopName[40];
	char szBBSName[40];
	bool UseLog;
	bool NoFossil;
	bool Initialized;
	uint8_t ComIRQ;

	char szScoreFile[2][PATH_SIZE];        // 0 == ascii, 1 == ansi
	char szRegcode[25];

	/*
	 * IBBS Specific data
	 */
	int16_t BBSID;
	bool InterBBS;
	char szNetmailDir[PATH_SIZE];
	char szOutputSem[PATH_SIZE];
	char **szInboundDirs;
	int16_t NumInboundDirs;
	int16_t MailerType;
};

struct system {
	bool Initialized;

	char szTodaysDate[11];
	char szMainDir[PATH_SIZE];

	int16_t Node;
	bool Local;
	bool LocalIBBS;
};

struct PClass {
	char szName[15];
	char Attributes[NUM_ATTRIBUTES];
	int16_t MaxHP;
	int16_t Gold;
	int16_t MaxSP;
	char SpellsKnown[MAX_SPELLS];

	int16_t VillageType;     // which villages is this allowed in?  0 == ALL
};

struct Strategy {
	char AttackLength, AttackIntensity, LootLevel;
	char DefendLength, DefendIntensity;

	/*

	-       *Length = 1..10 <- how long battle is

	    ex:
	    if AttackLength = 10 and defender's defendlength = 2,

	    NumRounds = (10 + 2) / 2 = 6;

	-       *Type      =    Light       -- small attacks, less vulnerable
	                    Moderate    -- moderate attacks on all grounds, medium
	                    Heavy       -- heavy attacks, more vulnerable

	-       LootLevel = How much you will loot or attempt to loot in percentage

	        allowable levels:   10% - low
	                            20% - moderate
	                            30% - high

	*/
};


struct Army {
	int32_t Footmen, Axemen, Knights, Followers;
	char Rating;                        // 0-100%, 100% = strongest
	char Level;                         // useable later?

	struct Strategy Strategy;

	int32_t CRC;
};

struct empire {
	char szName[25];                // who's empire
	int16_t OwnerType;                   // what type of empire? clan, alliance, vill
	int32_t VaultGold;
	int16_t Land;                        // amount of land available for use
	int16_t Buildings[MAX_BUILDINGS];    // # of each building type
	int16_t AllianceID;
	char WorkerEnergy;              // energy left for builders in %'age
	int16_t LandDevelopedToday;      // how much development we did
	int16_t SpiesToday;              // how many spying attempts today?
	int16_t AttacksToday;
	int32_t Points;                    // used in future?

	// other stuff goes here
	struct Army Army;

	int32_t CRC;
};



struct village {
	bool Initialized;

	struct village_data {
		char ColorScheme[50];
		char szName[30];

		int16_t TownType;
		int16_t TaxRate, InterestRate, GST;
		int16_t ConscriptionRate;

		int16_t RulingClanId[2];
		char szRulingClan[25];
		int16_t GovtSystem;
		int16_t RulingDays;

		uint16_t PublicMsgIndex;

		int16_t MarketLevel;
		int16_t TrainingHallLevel;  /* how good is the training hall? */
		int16_t ChurchLevel;    /* how good is the church */
		int16_t PawnLevel;      // level of pawn shop
		int16_t WizardLevel;    // wizard's shop level

		unsigned
		SetTaxToday     : 1,
		SetInterestToday: 1,
		SetGSTToday     : 1,
		UpMarketToday   : 1,
		UpTHallToday    : 1,
		UpChurchToday   : 1,
		UpPawnToday     : 1,
		UpWizToday      : 1,
		SetConToday     : 1,
		ShowEmpireStats : 1;

		char HFlags[8];         /* daily flags -- reset nightly */
		char GFlags[8];         /* global flags -- reset with reset */

		int16_t VillageType;    // what type of village do we have here?

		int16_t CostFluctuation;  // from -10% to +10%
		int16_t MarketQuality;

		struct empire Empire;

		int32_t CRC;                 // used to prevent cheating
	} Data;
};

struct game {
	bool Initialized;

	struct game_data {
		int16_t GameState;            // 0 == Game is in progress
		// 1 == Game is waiting for day to come to play
		// 2 == Game is inactive, waiting for reset msg.
		//      from LC

		bool InterBBS;            // set to true if IBBS *originally*
		// check when entering game against
		// Config->InterBBS to see if cheating occurs

		char szTodaysDate[11];    // Set to today's date when maintenance run
		char szDateGameStart[11]; // First day new game begins
		char szLastJoinDate[11];  // Last day to join game

		int16_t NextClanID;           // contains next available clanid[1] value
		int16_t NextAllianceID;       // contains next available alliance ID


		// ---- league specific data, not used in local games
		char szWorldName[30];     // league's world name
		char LeagueID[3];         // 2 char league ID
		char GameID[16];          // used to differentiate from old league games
		bool ClanTravel;          // true if clans are allowed to travel
		int16_t LostDays;             // # of days before lost packets are returned
		// ----

		// ---- Individual game settings go here
		int16_t MaxPermanentMembers;  // up to 6
		bool ClanEmpires;         // toggles whether clans can create empires
		int16_t MineFights,           // Max # of mine fights per day
		ClanFights,           // max # of clan fights per day
		DaysOfProtection;     // # of days of protection for newbies

		int32_t CRC;                 // used to prevent cheating

	} Data;
};

struct SpellsInEffect {
	int16_t SpellNum;    /* set to -1 so that it's inactive */
	int16_t Energy;  /* how much energy of spell remains, sees if it runs out */
};

struct item_data {
	bool Available;
	int16_t UsedBy;  /* 0 means nobody, 1.. etc. gives num of who uses it +1 */
	char szName[25];
	char cType;
	bool Special; /* if special item, can't be bought or sold */
	int16_t SpellNum;    // which spell do you cast when reading this scroll?

	char Attributes[NUM_ATTRIBUTES];
	char ReqAttributes[NUM_ATTRIBUTES];

	int32_t lCost;
	bool DiffMaterials;   /* means it can be made with different matterials */
	int16_t Energy;          /* how long before it "breaks"? */
	int16_t MarketLevel;     // what level of market before this is seen?

	int16_t VillageType;     // which villages is this allowed in?  0 == ALL
	int32_t ItemDate;          // when was item taken into the pawn shop?
	char RandLevel;         // from 0 - 10, level of randomness, 10 is highest
	char HPAdd, SPAdd;
};


struct pc {
	char szName[20];
	int16_t HP, MaxHP;
	int16_t SP, MaxSP;

	char Attributes[NUM_ATTRIBUTES];
	char Status;

	int16_t Weapon, Shield, Armor;

	int16_t WhichRace, WhichClass;
	int32_t Experience;
	int16_t Level;
	int16_t TrainingPoints;

	struct clan *MyClan;  /* pointer to his clan */
	char SpellsKnown[MAX_SPELLS];

	struct SpellsInEffect SpellsInEffect[10]; /* 10 spells is sufficient */


	int16_t Difficulty;      /* used only by monsters */
	unsigned Undead : 1,
	DefaultAction : 7;       // default action:
	//
	// 0 == attack
	// 1 == skip it, do nothing
	// 10 == spell #0
	// 11 == spell #1
	// 1x == spell #x

	int32_t CRC;
};

struct clan {
	int16_t ClanID[2];
	char szUserName[30];
	char szName[25];
	char Symbol[21];

	char QuestsDone[8], QuestsKnown[8];

	char PFlags[8], DFlags[8];
	char ChatsToday, TradesToday;

	int16_t ClanRulerVote[2];        // who are you voting for as the ruler?

	int16_t Alliances[MAX_ALLIES];       // alliances change from BBS to BBS, -1
	// means no alliance, 0 = first one, ID
	// that is...

	int32_t Points;
	char szDateOfLastGame[11];

	int16_t FightsLeft, ClanFights,
	MineLevel;

	int16_t WorldStatus, DestinationBBS;

	char VaultWithdrawals;

	int16_t PublicMsgIndex;

	int16_t ClanCombatToday[MAX_CLANCOMBAT][2];
	int16_t ClanWars;

	struct pc *Member[MAX_MEMBERS];
	struct item_data Items[MAX_ITEMS_HELD];

	char ResUncToday, ResDeadToday ;

	struct empire Empire;

	// Help
	unsigned DefActionHelp : 1,
	CommHelp      : 1,
	MineHelp      : 1,
	MineLevelHelp : 1,
	CombatHelp    : 1,
	TrainHelp     : 1,
	MarketHelp    : 1,
	PawnHelp      : 1,
	WizardHelp    : 1,
	EmpireHelp    : 1,
	DevelopHelp   : 1,
	TownHallHelp  : 1,
	DestroyHelp   : 1,
	ChurchHelp    : 1,
	THallHelp     : 1,
	SpyHelp       : 1,
	AllyHelp      : 1,
	WarHelp       : 1,
	VoteHelp      : 1,
	TravelHelp    : 1,

	WasRulerToday : 1,
	MadeAlliance  : 1,
	Protection    : 4,
	FirstDay      : 1,
	Eliminated    : 1,
	QuestToday    : 1,
	AttendedMass  : 1,
	GotBlessing   : 1,
	Prayed        : 1;

	int32_t CRC;
};

struct Spell {
	char szName[20];
	int16_t TypeFlag;
	bool Friendly;
	bool Target;
	char Attributes[NUM_ATTRIBUTES];
	char Value;
	int16_t Energy;
	char Level;     /* used to see if affects target */
	char *pszDamageStr;
	char *pszHealStr;
	char *pszModifyStr;
	char *pszWearoffStr;
	char *pszStatusStr;
	char *pszOtherStr;
	char *pszUndeadName;
	int16_t SP;
	bool StrengthCanReduce;
	bool WisdomCanReduce;
	unsigned MultiAffect : 1; // set to true if it affects more than one user
};


struct Language {
	char Signature[30];         // "The Clans Language File v1.0"

	uint16_t StrOffsets[2000];       // offsets for up to 1100 strings
	uint16_t NumBytes;               // how big is the bigstring!?

	char *BigString;        // All 500 strings jumbled together into
	// one
};

struct BuildingType {
	char szName[30];
	char HitZones,          // how vulnerable is it?  Higher = more vulnerable
	LandUsed,          // how many units of land used?
	EnergyUsed;        // how much energy is used to build it? 100=highest
	int32_t Cost;              // how much it'll cost ya to build this

	/* in future:

	bool MultipleBuild;   // false == can only build one at a time
	bool BuildingPrerequisites[MAX_BUILDINGS];    // ???
	*/

};


struct Alliance {
	int16_t ID;
	char szName[30];
	int16_t CreatorID[2];
	int16_t OriginalCreatorID[2];
	int16_t Member[MAX_ALLIANCEMEMBERS][2];

	struct empire Empire;
	struct item_data Items[MAX_ALLIANCEITEMS];
};




// This struct should have just about EVERYTHING on the battle
// who the attacker is, what his name is, what type of attacker, what his ID is
// who the victim is, what type he is and his ID
struct AttackResult {
	bool Success;                 // was the attacker successful?
	bool NoTarget;                    // set this if there was no ruler to oust
	// or no clan to battle
	bool InterBBS;                    // was this an InterBBS attack?

	// starting info
	struct Army OrigAttackArmy;  // what troops he had originally
	int16_t AttackerType;                // what type of attacker he is
	int16_t AttackerID[2];               // ID of person doing the attacking
	int16_t AllianceID;              // ID of alliance
	char szAttackerName[25];        // name of attacker

	// set beforehand
	int16_t DefenderType;                // what type of defender
	int16_t DefenderID[2];               // who is being attacked?

	// set in InterBBS only
	char szDefenderName[25];

	int16_t BBSIDFrom;                   // which BBS was the attacker from?
	int16_t BBSIDTo;                 // which BBS was the attack for?

	// actual result of battle:
	int16_t PercentDamage;
	int16_t Goal;                        // original goal of attack
	int16_t ExtentOfAttack;
	struct Army AttackCasualties, DefendCasualties;
	int16_t BuildingsDestroyed[MAX_BUILDINGS];
	int32_t GoldStolen;
	int16_t LandStolen;

	struct Army ReturningArmy;       // how many survived and are coming back?
	// only used for IBBS
	int16_t ResultIndex;             // used to prevent cheating
	int16_t AttackIndex;             // used to delete attackpacket from backup.dat

	int32_t CRC;                       // IBBS only
};


struct Topic {
	bool Known;               /* topic known yet? */
	bool Active;              /* does topic even exist? */
	bool ClanInfo;                /* if set to true, means gives info on clan
                                   he is in */
	char szName[70];            /* name of topic as seen by user */
	char szFileName[25];        /* name of topic in file */
};


struct NPCInfo {
	char szName[20];
	struct Topic Topics[MAX_TOPICS];
	struct Topic IntroTopic;
	char Loyalty;       /* how loyal is he? */
	int16_t WhereWander; // where does he wander most often?
	bool Roamer;      // is he a roamer type?
	int16_t NPCPCIndex;  // which NPC is he in the NPC.PC file?
	int16_t KnownTopics; // how many topics does he know?
	int16_t MaxTopics;       // how many topics we can discuss in one sitting
	int16_t OddsOfSeeing;    // good chance of seeing him or not
	char szHereNews[70]; // news shown when guy shows up
	// new additions
	char szQuoteFile[13];   // what file to use for quotes?
	char szMonFile[13];     // which .MON file is he in?
	char szIndex[20];       // index of this NPC

	int16_t VillageType;     // which villages is this allowed in?  0 == ALL
};


struct NPCNdx {
	char szIndex[20];
	bool InClan;      /* in a clan yet? */
	int16_t ClanID[2];       /* if so, which clan? */
	int16_t WhereWander;    // where/what is he now?
	int16_t Status;         // where/what is he now?
};


struct TradeList {
	int32_t Gold;
	int32_t Followers;
	int32_t Footmen, Axemen, Knights, Catapults;
};

struct TradeData {
	struct TradeList Giving;
	struct TradeList Asking;
	bool Active;

	int16_t FromClanID[2], ToClanID[2];
	char szFromClan[25];
	int32_t Code;
};


struct Message {
	int16_t ToClanID[2], FromClanID[2];
	char szFromName[25], szDate[11],
	szAllyName[30], szFromVillageName[40];

	int16_t MessageType;                  // 0 == Public
	// 1 == Private
	// 2 == Alliance only
	int16_t AllianceID;                   // If MessageType == 2, this alliance
	//    receives the message
	int16_t Flags;
	int16_t BBSIDFrom, BBSIDTo;

	int16_t PublicMsgIndex;               // Msg# for public posts

	struct Msg_Txt {
		int16_t Offsets[40];
		int16_t Length, NumLines;
		char *MsgTxt;
	} Data;
};


struct Packet {
	bool Active;

	char GameID[16];
	char szDate[11];
	int16_t BBSIDFrom, BBSIDTo;

	int16_t PacketType;
	int32_t PacketLength;
};


struct AttackPacket {
	int16_t BBSFromID;           // from whence it came
	int16_t BBSToID;         // where it's going
	struct empire AttackingEmpire;
	struct Army AttackingArmy;   // army doing the attacking
	int16_t Goal;                // what is the goal of this attack?
	int16_t ExtentOfAttack;  // level of attack
	int16_t TargetType;      // needed to find out who he's attacking
	int16_t ClanID[2];           // if a clan he's attacking, this is its ID
	int16_t AttackOriginatorID[2];     // clanid of who started the attack

	int16_t AttackIndex;
	int32_t CRC;
};

struct SpyAttemptPacket {
	char szSpierName[72];
	int16_t IntelligenceLevel;
	int16_t TargetType;      // either a village or clan
	int16_t ClanID[2];           // who is the target
	int16_t MasterID[2];     // who sent it?

	int16_t BBSFromID;
	int16_t BBSToID;
};

struct SpyResultPacket {
	int16_t BBSFromID;           // these are the same as spyattemptpacket
	int16_t BBSToID;
	int16_t MasterID[2];     // who sent it originally?
	char szTargetName[25];  // who we're spying on

	bool Success;
	struct empire Empire;   // unused if unsuccessful
	char szTheDate[11];
};

struct ibbs {
	bool Initialized;

	struct ibbs_data {
		int16_t BBSID;
		int16_t NumNodes;
		bool StrictMsgFile;
		bool StrictRouting;
		bool PacketSent;

		// Note, use IBBS.Nodes[x] where x corresponds to the BBSID
		struct ibbs_node {
			bool Active;

			struct ibbs_node_info {
				char *pszBBSName;
				char *pszVillageName;
				char *pszAddress;
				int16_t RouteThrough;
				int16_t MailType;
			} Info;

			struct ibbs_node_reset {
				int16_t Received;           /* used only by main BBS */
				int32_t LastSent;          /* last attempt at sending a reset command */
			} Reset;

			struct ibbs_node_recon {
				int32_t LastReceived;      /* when last recon gotten was */
				int32_t LastSent;          /* last attempted recon */
				char PacketIndex;
			} Recon;

			struct ibbs_node_attack {
				int16_t ReceiveIndex;
				int16_t SendIndex;
			} Attack;

		} Nodes[MAX_IBBSNODES];

	} Data;
};

struct ResetData {
	char GameID[16];
	char szDateGameStart[11];
	char szLastJoinDate[11];
	char szVillageName[40];
	bool InterBBSGame;
	bool LeagueWide;
	bool InProgress;
	bool EliminationMode;

	bool ClanTravel;
	int16_t LostDays;
	bool ClanEmpires;
	int16_t MineFights, ClanFights;
	int16_t MaxPermanentMembers;     // up to 6
	int16_t DaysOfProtection;
};

struct UserInfo {
	int16_t ClanID[2];
	bool Deleted;                 // set for deletion during maintenance
	char szMasterName[30];          // name of user who plays
	char szName[30];                // name of clan itself
};

struct UserScore {
	int16_t ClanID[2];
	char Symbol[21];
	char szName[30];
	int32_t Points;
	int16_t BBSID;
};

struct LeavingData {
	bool Active;
	int16_t DestID;
	int16_t ClanID[2];

	// how many troops to bring along with you
	int32_t Followers, Footmen, Axemen, Knights, Catapults;
};

struct Door {
	bool Initialized;
	bool AllowScreenPause;

	char ColorScheme[50];

	bool UserBooted;            // True if a user was online already
};

#endif
