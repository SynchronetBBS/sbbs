#include "defines.h"

struct __attribute__((packed)) IniFile {
	__BOOL Initialized;

	char *pszLanguage;
	char *pszNPCFileName[MAX_NPCFILES];
	char *pszSpells[MAX_SPELLFILES];
	char *pszItems[MAX_ITEMFILES];
	char *pszRaces[MAX_RACEFILES];
	char *pszClasses[MAX_CLASSFILES];
	char *pszVillages[MAX_VILLFILES];
} PACKED;


struct __attribute__((packed)) config {

  char szSysopName[40];
  char szBBSName[40];

  char szScoreFile[2][40];        // 0 == ascii, 1 == ansi
  char szRegcode[25];

  /*
   * IBBS Specific data
   */
  _INT16 BBSID;
  __BOOL InterBBS;
  char szNetmailDir[30];
  char szInboundDir[30];
  _INT16 MailerType;
} PACKED;

struct __attribute__((packed)) system {
  __BOOL Initialized;

  char szTodaysDate[11];
  char szMainDir[40];

  _INT16 Node;
  __BOOL InterBBS;
  __BOOL Local;
  __BOOL LocalIBBS;
} PACKED;

struct __attribute__((packed)) PClass {
	char szName[15];
	char Attributes[NUM_ATTRIBUTES];
	_INT16 MaxHP;
	_INT16 Gold;
  _INT16 MaxSP;
  char SpellsKnown[MAX_SPELLS];

	_INT16 VillageType;		// which villages is this allowed in?  0 == ALL
} PACKED;

struct __attribute__((packed)) Strategy {
	char AttackLength, AttackIntensity, LootLevel;
	char DefendLength, DefendIntensity;

	/*

-		*Length = 1..10 <- how long battle is

		ex:
		if AttackLength = 10 and defender's defendlength = 2,

		NumRounds = (10 + 2) / 2 = 6;

-		*Type	   =	Light		-- small attacks, less vulnerable
						Moderate	-- moderate attacks on all grounds, medium
						Heavy		-- heavy attacks, more vulnerable

-		LootLevel = How much you will loot or attempt to loot in percentage

			allowable levels:	10% - low
								20% - moderate
								30% - high

	*/
} PACKED;


struct __attribute__((packed)) Army {
	long Footmen, Axemen, Knights, Followers;
	char Rating;						// 0-100%, 100% = strongest
	char Level; 						// useable later?

	struct __attribute__((packed)) Strategy Strategy;

	long CRC;
} PACKED;

struct __attribute__((packed)) empire {
	char szName[25];				// who's empire
	_INT16 OwnerType;					// what type of empire? clan, alliance, vill
	long VaultGold;
	_INT16 Land;						// amount of land available for use
	_INT16 Buildings[MAX_BUILDINGS];	// # of each building type
	_INT16 AllianceID;
	char WorkerEnergy;				// energy left for builders in %'age
	_INT16 LandDevelopedToday; 		// how much development we did
	_INT16 SpiesToday; 				// how many spying attempts today?
	_INT16 AttacksToday;
	long Points;					// used in future?
	_INT16 Junk[4];

	// other stuff goes here
	struct __attribute__((packed)) Army Army;

	long CRC;
} PACKED;



struct __attribute__((packed)) village {
  __BOOL Initialized;

  struct __attribute__((packed)) village_data {
    char ColorScheme[50];
    char szName[30];

    _INT16 TownType;
    _INT16 TaxRate, InterestRate, GST;
    _INT16 ConscriptionRate;

    _INT16 RulingClanId[2];
    char szRulingClan[25];
    _INT16 GovtSystem;
    _INT16 RulingDays;

    unsigned _INT16 PublicMsgIndex;

    _INT16 MarketLevel;
    _INT16 TrainingHallLevel;  /* how good is the training hall? */
    _INT16 ChurchLevel;    /* how good is the church */
    _INT16 PawnLevel;      // level of pawn shop
    _INT16 WizardLevel;    // wizard's shop level

    unsigned _INT16
       SetTaxToday     : 1,
       SetInterestToday: 1,
       SetGSTToday     : 1,
       UpMarketToday   : 1,
       UpTHallToday    : 1,
       UpChurchToday   : 1,
       UpPawnToday     : 1,
       UpWizToday      : 1,
       SetConToday     : 1,
       ShowEmpireStats : 1,
       Junk            : 6;

    char HFlags[8];         /* daily flags -- reset nightly */
    char GFlags[8];         /* global flags -- reset with reset */

    _INT16 VillageType;    // what type of village do we have here?

    _INT16 CostFluctuation;  // from -10% to +10%
    _INT16 MarketQuality;

    struct __attribute__((packed)) empire Empire;

    long CRC;                 // used to prevent cheating
  } *Data;
} PACKED;

struct __attribute__((packed)) game {
  __BOOL Initialized;

  struct __attribute__((packed)) game_data {
    _INT16 GameState;            // 0 == Game is in progress
                              // 1 == Game is waiting for day to come to play
                              // 2 == Game is inactive, waiting for reset msg.
                              //      from LC

    __BOOL InterBBS;            // set to TRUE if IBBS *originally*
                              // check when entering game against
                              // Config->InterBBS to see if cheating occurs

    char szTodaysDate[11];    // Set to today's date when maintenance run
    char szDateGameStart[11]; // First day new game begins
    char szLastJoinDate[11];  // Last day to join game

    _INT16 NextClanID;           // contains next available clanid[1] value
    _INT16 NextAllianceID;       // contains next available alliance ID


    // ---- league specific data, not used in local games
    char szWorldName[30];     // league's world name
    char LeagueID[3];         // 2 char league ID
    char GameID[16];          // used to differentiate from old league games
    __BOOL ClanTravel;          // TRUE if clans are allowed to travel
    _INT16 LostDays;             // # of days before lost packets are returned
    // ----

    // ---- Individual game settings go here
    _INT16 MaxPermanentMembers;  // up to 6
    __BOOL ClanEmpires;         // toggles whether clans can create empires
    _INT16 MineFights,           // Max # of mine fights per day
        ClanFights,           // max # of clan fights per day
        DaysOfProtection;     // # of days of protection for newbies

    long CRC;                 // used to prevent cheating

  } *Data;
} PACKED;

struct __attribute__((packed)) SpellsInEffect {
	_INT16 SpellNum;	/* set to -1 so that it's inactive */
	_INT16 Energy; 	/* how much energy of spell remains, sees if it runs out */
} PACKED;

struct __attribute__((packed)) item_data {
	__BOOL Available;
	_INT16 UsedBy; 	/* 0 means nobody, 1.. etc. gives num of who uses it +1 */
	char szName[25];
	char cType;
	__BOOL Special;	/* if special item, can't be bought or sold */
	_INT16 SpellNum;	// which spell do you cast when reading this scroll?

	char Attributes[NUM_ATTRIBUTES];
	char ReqAttributes[NUM_ATTRIBUTES];

	long lCost;
	__BOOL DiffMaterials; 	/* means it can be made with different matterials */
	_INT16 Energy; 			/* how long before it "breaks"? */
	_INT16 MarketLevel;		// what level of market before this is seen?

	_INT16 VillageType;		// which villages is this allowed in?  0 == ALL
	long ItemDate;			// when was item taken into the pawn shop?
	char RandLevel; 		// from 0 - 10, level of randomness, 10 is highest
	char HPAdd, SPAdd;
} PACKED;


struct __attribute__((packed)) pc {
	char szName[20];
	_INT16 HP, MaxHP;
  _INT16 SP, MaxSP;

	char Attributes[NUM_ATTRIBUTES];
	char Status;

	_INT16 Weapon, Shield, Armor;

	_INT16 WhichRace, WhichClass;
	long Experience;
	_INT16 Level;
	_INT16 TrainingPoints;

  struct __attribute__((packed)) clan *MyClan;  /* pointer to his clan */
  char SpellsKnown[MAX_SPELLS];

  struct __attribute__((packed)) SpellsInEffect SpellsInEffect[10]; /* 10 spells is sufficient */


	_INT16 Difficulty; 		/* used only by monsters */
  char Undead : 1,
		 DefaultAction : 7; 	  // default action:
							//
							// 0 == attack
							// 1 == skip it, do nothing
							// 10 == spell #0
							// 11 == spell #1
							// 1x == spell #x

	long CRC;
} PACKED;

struct __attribute__((packed)) clan {
  _INT16 ClanID[2];
  char szUserName[30];
  char szName[25];
  char Symbol[21];

  char QuestsDone[8], QuestsKnown[8];

  char PFlags[8], DFlags[8];
  char ChatsToday, TradesToday;

	_INT16 ClanRulerVote[2];		// who are you voting for as the ruler?

	_INT16 Alliances[MAX_ALLIES];		// alliances change from BBS to BBS, -1
                                // means no alliance, 0 = first one, ID
                                // that is...

  long Points;
  char szDateOfLastGame[11];

  _INT16 FightsLeft, ClanFights,
      MineLevel;

  _INT16 WorldStatus, DestinationBBS;

	char VaultWithdrawals;

  _INT16 PublicMsgIndex;

	_INT16 ClanCombatToday[MAX_CLANCOMBAT][2];
  _INT16 ClanWars;

  struct __attribute__((packed)) pc *Member[MAX_MEMBERS];
  struct __attribute__((packed)) item_data Items[MAX_ITEMS_HELD];

	char ResUncToday, ResDeadToday ;

  struct __attribute__((packed)) empire Empire;

  // Help
  _INT16 DefActionHelp : 1,
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
  
  long CRC;
} PACKED;

struct __attribute__((packed)) Spell {
	char szName[20];
	_INT16 TypeFlag;
	__BOOL Friendly;
	__BOOL Target;
	char Attributes[NUM_ATTRIBUTES];
	char Value;
	_INT16 Energy;
	char Level; 	/* used to see if affects target */
	char *pszDamageStr;
	char *pszHealStr;
	char *pszModifyStr;
	char *pszWearoffStr;
	char *pszStatusStr;
	char *pszOtherStr;
	char *pszUndeadName;
  _INT16 SP;
	__BOOL StrengthCanReduce;
	__BOOL WisdomCanReduce;
#if defined(_WIN32) || defined(__unix__)
	char MultiAffect : 1;
	char Garbage : 7;
#else
	_INT16 MultiAffect : 1;	// set to true if it affects more than one user
    _INT16  Garbage : 7;
#endif
} PACKED;


struct __attribute__((packed)) Language {
	char Signature[30]; 		// "The Clans Language File v1.0"

	unsigned _INT16 StrOffsets[2000];		// offsets for up to 1100 strings
	unsigned _INT16 NumBytes;				// how big is the bigstring!?

	char *BigString;		// All 500 strings jumbled together into
								// one
} PACKED;

struct __attribute__((packed)) BuildingType {
	char szName[30];
	char HitZones,			// how vulnerable is it?  Higher = more vulnerable
		 LandUsed,			// how many units of land used?
		 EnergyUsed;		// how much energy is used to build it? 100=highest
	long Cost;				// how much it'll cost ya to build this

	/* in future:

	__BOOL MultipleBuild; 	// FALSE == can only build one at a time
	__BOOL BuildingPrerequisites[MAX_BUILDINGS];	// ???
	*/

} PACKED;


struct __attribute__((packed)) Alliance {
	_INT16 ID;
	char szName[30];
	_INT16 CreatorID[2];
	_INT16 OriginalCreatorID[2];
	_INT16 Member[MAX_ALLIANCEMEMBERS][2];

  struct __attribute__((packed)) empire Empire;
  struct __attribute__((packed)) item_data Items[MAX_ALLIANCEITEMS];
} PACKED;




// This struct __attribute__((packed)) should have just about EVERYTHING on the battle
// who the attacker is, what his name is, what type of attacker, what his ID is
// who the victim is, what type he is and his ID
struct __attribute__((packed)) AttackResult {
	__BOOL Success;					// was the attacker successful?
	__BOOL NoTarget;					// set this if there was no ruler to oust
									// or no clan to battle
	__BOOL InterBBS;					// was this an InterBBS attack?

	// starting info
	struct __attribute__((packed)) Army OrigAttackArmy; 	// what troops he had originally
	_INT16 AttackerType;				// what type of attacker he is
	_INT16 AttackerID[2];				// ID of person doing the attacking
	_INT16 AllianceID; 				// ID of alliance
	char szAttackerName[25];		// name of attacker

	// set beforehand
	_INT16 DefenderType;				// what type of defender
	_INT16 DefenderID[2];				// who is being attacked?

	// set in InterBBS only
	char szDefenderName[25];

	_INT16 BBSIDFrom;					// which BBS was the attacker from?
	_INT16 BBSIDTo;					// which BBS was the attack for?

	// actual result of battle:
	_INT16 PercentDamage;
	_INT16 Goal;						// original goal of attack
	_INT16 ExtentOfAttack;
	struct __attribute__((packed)) Army AttackCasualties, DefendCasualties;
	_INT16 BuildingsDestroyed[MAX_BUILDINGS];
	long GoldStolen;
	_INT16 LandStolen;

	struct __attribute__((packed)) Army ReturningArmy;		// how many survived and are coming back?
									// only used for IBBS
	_INT16 ResultIndex;				// used to prevent cheating
	_INT16 AttackIndex;				// used to delete attackpacket from backup.dat

	long CRC;						// IBBS only
} PACKED;


struct __attribute__((packed)) Topic {
	__BOOL Known; 				/* topic known yet? */
	__BOOL Active;				/* does topic even exist? */
	__BOOL ClanInfo;				/* if set to TRUE, means gives info on clan
								   he is in */
	char szName[70];			/* name of topic as seen by user */
	char szFileName[25];		/* name of topic in file */
} PACKED;


struct __attribute__((packed)) NPCInfo {
	char szName[20];
	struct __attribute__((packed)) Topic Topics[MAX_TOPICS];
	struct __attribute__((packed)) Topic IntroTopic;
  __BOOL Garbage;
  _INT16 Garbage2[2];
	char Loyalty;		/* how loyal is he? */
	_INT16 WhereWander;	// where does he wander most often?
  _INT16 Garbage3;
	__BOOL Roamer;		// is he a roamer type?
	_INT16 NPCPCIndex; 	// which NPC is he in the NPC.PC file?
	_INT16 KnownTopics;	// how many topics does he know?
	_INT16 MaxTopics;		// how many topics we can discuss in one sitting
	_INT16 OddsOfSeeing;	// good chance of seeing him or not
	char szHereNews[70]; // news shown when guy shows up
	// new additions
	char szQuoteFile[13];	// what file to use for quotes?
	char szMonFile[13]; 	// which .MON file is he in?
	char szIndex[20];		// index of this NPC

	_INT16 VillageType;		// which villages is this allowed in?  0 == ALL
} PACKED;


struct __attribute__((packed)) NPCNdx {
	char szIndex[20];
	__BOOL InClan;		/* in a clan yet? */
	_INT16 ClanID[2];		/* if so, which clan? */
  _INT16 WhereWander;    // where/what is he now?
  _INT16 Status;         // where/what is he now?
} PACKED;


struct __attribute__((packed)) TradeList {
	long Gold;
	long Followers;
	long Footmen, Axemen, Knights, Catapults;
} PACKED;

struct __attribute__((packed)) TradeData {
	struct __attribute__((packed)) TradeList Giving;
	struct __attribute__((packed)) TradeList Asking;
	__BOOL Active;

  _INT16 FromClanID[2], ToClanID[2];
	char szFromClan[25];
	long Code;
} PACKED;


struct __attribute__((packed)) Message {
  _INT16 ToClanID[2], FromClanID[2];
  char szFromName[25], szDate[11],
       szAllyName[30], szFromVillageName[40];

  _INT16 MessageType;                  // 0 == Public
                                    // 1 == Private
                                    // 2 == Alliance only
  _INT16 AllianceID;                   // If MessageType == 2, this alliance
                                    //    receives the message
  _INT16 Flags;
  _INT16 BBSIDFrom, BBSIDTo;

  _INT16 PublicMsgIndex;               // Msg# for public posts

  struct __attribute__((packed)) Msg_Txt {
    _INT16 Offsets[40];
    _INT16 Length, NumLines;
    char *MsgTxt;
  } Data;
} PACKED;


struct __attribute__((packed)) Packet {
  __BOOL Active;

  char GameID[16];
  char szDate[11];
  _INT16 BBSIDFrom, BBSIDTo;

	_INT16 PacketType;
  long PacketLength;
} PACKED;


struct __attribute__((packed)) AttackPacket {
	_INT16 BBSFromID;			// from whence it came
	_INT16 BBSToID;			// where it's going
  struct __attribute__((packed)) empire AttackingEmpire;
	struct __attribute__((packed)) Army AttackingArmy;	// army doing the attacking
	_INT16 Goal;				// what is the goal of this attack?
	_INT16 ExtentOfAttack; 	// level of attack
	_INT16 TargetType; 		// needed to find out who he's attacking
	_INT16 ClanID[2];			// if a clan he's attacking, this is its ID
	_INT16 AttackOriginatorID[2];	  // clanid of who started the attack

	_INT16 AttackIndex;
	long CRC;
} PACKED;

struct __attribute__((packed)) SpyAttemptPacket {
	char szSpierName[40];
	_INT16 IntelligenceLevel;
	_INT16 TargetType; 		// either a village or clan
	_INT16 ClanID[2];			// who is the target
	_INT16 MasterID[2];		// who sent it?

	_INT16 BBSFromID;
	_INT16 BBSToID;
} PACKED;

struct __attribute__((packed)) SpyResultPacket {
	_INT16 BBSFromID;			// these are the same as spyattemptpacket
	_INT16 BBSToID;
	_INT16 MasterID[2];		// who sent it originally?
	char szTargetName[35];	// who we're spying on

	__BOOL Success;
  struct __attribute__((packed)) empire Empire;   // unused if unsuccessful
	char szTheDate[11];
} PACKED;

struct __attribute__((packed)) ibbs {
  __BOOL Initialized;

  struct __attribute__((packed)) ibbs_data {
    _INT16 BBSID;

    _INT16 NumNodes;

    // Note, use IBBS.Nodes[x] where x corresponds to the BBSID
    struct __attribute__((packed)) ibbs_node {
      __BOOL Active;

      struct __attribute__((packed)) ibbs_node_info {
        char *pszBBSName;
        char *pszVillageName;
        char *pszAddress;
        _INT16 RouteThrough;
        _INT16 MailType;
      } Info;

      struct __attribute__((packed)) ibbs_node_reset {
        _INT16 Received;           /* used only by main BBS */
        long LastSent;          /* last attempt at sending a reset command */
      } Reset;

      struct __attribute__((packed)) ibbs_node_recon {
        long LastReceived;      /* when last recon gotten was */
        long LastSent;          /* last attempted recon */
        char PacketIndex;
      } Recon;

      struct __attribute__((packed)) ibbs_node_attack {
        _INT16 ReceiveIndex;
        _INT16 SendIndex;
      } Attack;

    } Nodes[MAX_IBBSNODES];

  } *Data;
} PACKED;

struct __attribute__((packed)) ResetData {
  char GameID[16];
  char szDateGameStart[11];
  char szLastJoinDate[11];
	char szVillageName[40];
	__BOOL InterBBSGame;
	__BOOL LeagueWide;
	__BOOL InProgress;
	__BOOL EliminationMode;

	__BOOL ClanTravel;
	_INT16 LostDays;
	__BOOL ClanEmpires;
	_INT16 MineFights, ClanFights;
	_INT16 MaxPermanentMembers;		// up to 6
	_INT16 DaysOfProtection;
} PACKED;

struct __attribute__((packed)) UserInfo {
	_INT16 ClanID[2];
	__BOOL Deleted;					// set for deletion during maintenance
	char szMasterName[30];			// name of user who plays
	char szName[30];				// name of clan itself
} PACKED;

struct __attribute__((packed)) UserScore {
	_INT16 ClanID[2];
	char Symbol[21];
	char szName[30];
	long Points;
	_INT16 BBSID;
} PACKED;

struct __attribute__((packed)) LeavingData {
	__BOOL Active;
  _INT16 DestID;
	_INT16 ClanID[2];

	// how many troops to bring along with you
	long Followers, Footmen, Axemen, Knights, Catapults;
} PACKED;

