#define MAX_KEY_WORDS   15
#define MAX_TOKEN_CHARS 32

char *papszKeyWords[MAX_KEY_WORDS] =
{
    "SysopName",
    "BBSName",
    "UseLog",
    "ScoreANSI",
    "ScoreASCII",
    "Node",
    "DropDirectory",
    "UseFOSSIL",
    "SerialPortAddr",
    "SerialPortIRQ",
    "BBSId",
    "NetmailDir",
    "InboundDir",
    "MailerType",
    "InterBBS"
};

#define MAX_IM_WORDS    22
#define I_ITEM          0
#define I_WEAPON        1
#define I_ARMOR         2
#define I_SHIELD        3

char *papszItemKeyWords[MAX_IM_WORDS] =
{
    "Name",
    "Type",
    "Special",
    "Agility",
    "Dexterity",
    "Strength",
    "Wisdom",
    "ArmorStr",
    "Charisma",
    "Cost",
    "DiffMaterials",
    "Stats",
    "Requirements",
    "Penalties",
    "Energy",
    "Uses",
    "Spell",
    "Level",
    "Village",
    "RandLevel",
    "HPAdd",
    "SPAdd"
};

#define MAX_PC_WORDS    12

char *papszPCKeyWords[MAX_PC_WORDS] =
{
    "Name",
    "HP",
    "Agility",
    "Dexterity",
    "Strength",
    "Wisdom",
    "ArmorStr",
    "Weapon",
    "Shield",
    "Armor",
    "Item",
    "ClanName"
};

#define MAX_PCLASS_WORDS    11

char *papszPClassKeyWords[MAX_PCLASS_WORDS] =
{
    "Name",
    "Agility",
    "Dexterity",
    "Strength",
    "Wisdom",
    "ArmorStr",
    "Charisma",
    "MaxHP",
    "Gold",
    "MaxMP",
    "Spell"
};



#define MAX_SPELL_WORDS     24

char *papszSpellKeyWords[MAX_SPELL_WORDS] =
{
    "Name",
    "Agility",
    "Dexterity",
    "Strength",
    "Wisdom",
    "ArmorStr",
    "Charisma",
    "Value",
    "Flag",
    "HealStr",
    "DamageStr",
    "ModifyStr",
    "SP",
    "Friendly",
    "NoTarget",
    "Level",
    "WearoffStr",
    "Energy",
    "StatusStr",
    "OtherStr",
    "UndeadName",
    "StrengthCanReduce",
    "WisdomCanReduce",
    "MultiAffect"
};

#define MAX_NDX_WORDS   10

char *papszNdxKeyWords[MAX_NDX_WORDS] =
{
    "BBSName",
    "VillageName",
    "Address",
    "ConnectType",
    "MailType",
    "BBSId",
    "Status",
    "WorldName",
    "LeagueId",
    "Host"
};

#define MAX_RT_WORDS    4

char *papszRouteKeyWords[MAX_RT_WORDS] =
{
    "ROUTE",
    "CRASH",
    "HOLD",
    "NORMAL"
};

#define MAX_COMLINE_WORDS    20

char *papszComLineKeyWords[MAX_COMLINE_WORDS] =
{
    "L",
    "Local",
    "M",
    "Maint",
    "FM",
    "FMaint",
    "ForceMaint",
    "O",
    "Outbound",
    "I",
    "Inbound",
    "F",
    "Fullmode",
    "T",
    "TimeSlicer",
    "N",
    "?",
    "H",
    "Help",
    "Reset"
};

#define MAX_NPC_WORDS   13

char *papszNPCKeyWords[MAX_NPC_WORDS] =
{
    "Name",
    "Loyalty",
    "KnownTopic",
    "Topic",
    "Wander",
    "NPCDAT",
    "MaxTopics",
    "OddsOfSeeing",
    "IntroTopic",
    "HereNews",
    "QuoteFile",
    "MonFile",
    "Index"
};
