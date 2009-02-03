const	NAMESIZE=			80;
const	DISPLAYSIZE=		160;

const	ORDERGAP=			4;
const	BLOCKSIZE=			7;
const	BLOCKBORDER=		1;
const	MAINTENANCE=		10;
const	STARTMONEY=			5000;
const	RECRUITCOST=		50;
const	RECRUITFRACTION=	4;
const	ENTERTAININCOME=	20;
const	ENTERTAINFRACTION=	20;
const	TAXINCOME=			200;
const	COMBATEXP=			10;
const	PRODUCEEXP=			10;
const	TEACHNUMBER=		10;
const	STUDYCOST=			200;
const	POPGROWTH=			5;
const	PEASANTMOVE=		5;

const	K_ACCEPT=		0;
const	K_ADDRESS=		1;
const	K_ADMIT=		2;
const	K_ALLY=			3;
const	K_ATTACK=		4;
const	K_BEHIND=		5;
const	K_BOARD=		6;
const	K_BUILD=		7;
const	K_BUILDING=		8;
const	K_CAST=			9;
const	K_CLIPPER=		10;
const	K_COMBAT=		11;
const	K_DEMOLISH=		12;
const	K_DISPLAY=		13;
const	K_EAST=			14;
const	K_END=			15;
const	K_ENTER=		16;
const	K_ENTERTAIN=	17;
const	K_FACTION=		18;
const	K_FIND=			19;
const	K_FORM=			20;
const	K_GALLEON=		21;
const	K_GIVE=			22;
const	K_GUARD=		23;
const	K_LEAVE=		24;
const	K_LONGBOAT=		25;
const	K_MOVE=			26;
const	K_NAME=			27;
const	K_NORTH=		28;
const	K_PAY=			29;
const	K_PRODUCE=		30;
const	K_PROMOTE=		31;
const	K_QUIT=			32;
const	K_RECRUIT=		33;
const	K_RESEARCH=		34;
const	K_RESHOW=		35;
const	K_SAIL=			36;
const	K_SHIP=			37;
const	K_SINK=			38;
const	K_SOUTH=		39;
const	K_STUDY=		40;
const	K_TAX=			41;
const	K_TEACH=		42;
const	K_TRANSFER=		43;
const	K_UNIT=			44;
const	K_WEST=			45;
const	K_WORK=			46;
const	MAXKEYWORDS=	47;

const keywords =
[
	"accept",
	"address",
	"admit",
	"ally",
	"attack",
	"behind",
	"board",
	"build",
	"building",
	"cast",
	"clipper",
	"combat",
	"demolish",
	"display",
	"east",
	"end",
	"enter",
	"entertain",
	"faction",
	"find",
	"form",
	"galleon",
	"give",
	"guard",
	"leave",
	"longboat",
	"move",
	"name",
	"north",
	"pay",
	"produce",
	"promote",
	"quit",
	"recruit",
	"research",
	"reshow",
	"sail",
	"ship",
	"sink",
	"south",
	"study",
	"tax",
	"teach",
	"transfer",
	"unit",
	"west",
	"work",
];

const regionnames =
[
	"Aberaeron",
	"Aberdaron",
	"Aberdovey",
	"Abernethy",
	"Abersoch",
	"Abrantes",
	"Adrano",
	"AeBrey",
	"Aghleam",
	"Akbou",
	"Aldan",
	"Alfaro",
	"Alghero",
	"Almeria",
	"Altnaharra",
	"Ancroft",
	"Anshun",
	"Anstruther",
	"Antor",
	"Arbroath",
	"Arcila",
	"Ardfert",
	"Ardvale",
	"Arezzo",
	"Ariano",
	"Arlon",
	"Avanos",
	"Aveiro",
	"Badalona",
	"Baechahoela",
	"Ballindine",
	"Balta",
	"Banlar",
	"Barika",
	"Bastak",
	"Bayonne",
	"Bejaia",
	"Benlech",
	"Beragh",
	"Bergland",
	"Berneray",
	"Berriedale",
	"Binhai",
	"Birde",
	"Bocholt",
	"Bogmadie",
	"Braga",
	"Brechlin",
	"Brodick",
	"Burscough",
	"Calpio",
	"Canna",
	"Capperwe",
	"Caprera",
	"Carahue",
	"Carbost",
	"Carnforth",
	"Carrigaline",
	"Caserta",
	"Catrianchi",
	"Clatter",
	"Coilaco",
	"Corinth",
	"Corofin",
	"Corran",
	"Corwen",
	"Crail",
	"Cremona",
	"Crieff",
	"Cromarty",
	"Cumbraes",
	"Daingean",
	"Darm",
	"Decca",
	"Derron",
	"Derwent",
	"Deveron",
	"Dezhou",
	"Doedbygd",
	"Doramed",
	"Dornoch",
	"Drammes",
	"Dremmer",
	"Drense",
	"Drimnin",
	"Drumcollogher",
	"Drummore",
	"Dryck",
	"Drymen",
	"Dunbeath",
	"Duncansby",
	"Dunfanaghy",
	"Dunkeld",
	"Dunmanus",
	"Dunster",
	"Durness",
	"Duucshire",
	"Elgomaar",
	"Ellesmere",
	"Ellon",
	"Enfar",
	"Erisort",
	"Eskerfan",
	"Ettrick",
	"Fanders",
	"Farafra",
	"Ferbane",
	"Fetlar",
	"Flock",
	"Florina",
	"Formby",
	"Frainberg",
	"Galloway",
	"Ganzhou",
	"Geal Charn",
	"Gerr",
	"Gifford",
	"Girvan",
	"Glenagallagh",
	"Glenanane",
	"Glin",
	"Glomera",
	"Glormandia",
	"Gluggby",
	"Gnackstein",
	"Gnoelhaala",
	"Golconda",
	"Gourock",
	"Graevbygd",
	"Grandola",
	"Gresberg",
	"Gresir",
	"Greverre",
	"Griminish",
	"Grisbygd",
	"Groddland",
	"Grue",
	"Gurkacre",
	"Haikou",
	"Halkirk",
	"Handan",
	"Hasmerr",
	"Helmsdale",
	"Helmsley",
	"Helsicke",
	"Helvete",
	"Hoersalsveg",
	"Hullevala",
	"Ickellund",
	"Inber",
	"Inverie",
	"Jaca",
	"Jahrom",
	"Jeormel",
	"Jervbygd",
	"Jining",
	"Jotel",
	"Kaddervar",
	"Karand",
	"Karothea",
	"Kashmar",
	"Keswick",
	"Kielder",
	"Killorglin",
	"Kinbrace",
	"Kintore",
	"Kirriemuir",
	"Klen",
	"Knesekt",
	"Kobbe",
	"Komarken",
	"Kovel",
	"Krod",
	"Kursk",
	"Lagos",
	"Lamlash",
	"Langholm",
	"Larache",
	"Larkanth",
	"Larmet",
	"Lautaro",
	"Leighlin",
	"Lervir",
	"Leven",
	"Licata",
	"Limavady",
	"Lingen",
	"Lintan",
	"Liscannor",
	"Locarno",
	"Lochalsh",
	"Lochcarron",
	"Lochinver",
	"Lochmaben",
	"Lom",
	"Lorthalm",
	"Louer",
	"Lurkabo",
	"Luthiir",
	"Lybster",
	"Lynton",
	"Mallaig",
	"Mataro",
	"Melfi",
	"Melvaig",
	"Menter",
	"Methven",
	"Moffat",
	"Monamolin",
	"Monzon",
	"Morella",
	"Morgel",
	"Mortenford",
	"Mullaghcarn",
	"Mulle",
	"Murom",
	"Nairn",
	"Navenby",
	"Nephin Beg",
	"Niskby",
	"Nolle",
	"Nork",
	"Olenek",
	"Oloron",
	"Oranmore",
	"Ormgryte",
	"Orrebygd",
	"Palmi",
	"Panyu",
	"Partry",
	"Pauer",
	"Penhalolen",
	"Perkel",
	"Perski",
	"Planken",
	"Plattland",
	"Pleagne",
	"Pogelveir",
	"Porthcawl",
	"Portimao",
	"Potenza",
	"Praestbygd",
	"Preetsome",
	"Presu",
	"Prettstern",
	"Rantlu",
	"Rappbygd",
	"Rath Luire",
	"Rethel",
	"Riggenthorpe",
	"Rochfort",
	"Roddendor",
	"Roin",
	"Roptille",
	"Roter",
	"Rueve",
	"Sagunto",
	"Saklebille",
	"Salen",
	"Sandwick",
	"Sarab",
	"Sarkanvale",
	"Scandamia",
	"Scarinish",
	"Scourie",
	"Serov",
	"Shanyin",
	"Siegen",
	"Sinan",
	"Sines",
	"Skim",
	"Skokholm",
	"Skomer",
	"Skottskog",
	"Sledmere",
	"Sorisdale",
	"Spakker",
	"Stackforth",
	"Staklesse",
	"Stinchar",
	"Stoer",
	"Strichen",
	"Stroma",
	"Stugslett",
	"Suide",
	"Tabuk",
	"Tarraspan",
	"Tetuan",
	"Thurso",
	"Tiemcen",
	"Tiksi",
	"Tolsta",
	"Toppola",
	"Torridon",
	"Trapani",
	"Tromeforth",
	"Tudela",
	"Turia",
	"Uxelberg",
	"Vaila",
	"Valga",
	"Verguin",
	"Vernlund",
	"Victoria",
	"Waimer",
	"Wett",
	"Xontormia",
	"Yakleks",
	"Yuci",
	"Zaalsehuur",
	"Zamora",
	"Zapulla",
];

const	T_OCEAN=	0;
const	T_PLAIN=	1;
const	T_MOUNTAIN=	2;
const	T_FOREST=	3;
const	T_SWAMP=	4;

const terrains =
[
	{
		name:"ocean",
		foodproductivity:0,
		maxfoodoutput:0,
		productivity:{
			iron:0,
			wood:0,
			stone:0,
			horse:0,
		},
		maxoutput:{
			iron:0,
			wood:0,
			stone:0,
			horse:0,
		},
	},
	{
		name:"plain",
		foodproductivity:15,
		maxfoodoutput:100000,
		productivity:{
			iron:0,
			wood:0,
			stone:0,
			horse:1,
		},
		maxoutput:{
			iron:0,
			wood:0,
			stone:0,
			horse:200,
		},
	},
	{
		name:"mountain",
		foodproductivity:12,
		maxfoodoutput:20000,
		productivity:{
			iron:1,
			wood:0,
			stone:1,
			horse:0,
		},
		maxoutput:{
			iron:200,
			wood:0,
			stone:200,
			horse:0,
		},
	},
	{
		name:"forest",
		foodproductivity:12,
		maxfoodoutput:20000,
		productivity:{
			iron:0,
			wood:1,
			stone:0,
			horse:0,
		},
		maxoutput:{
			iron:0,
			wood:200,
			stone:0,
			horse:0,
		},
	},
	{
		name:"swamp",
		foodproductivity:12,
		maxfoodoutput:10000,
		productivity:{
			iron:0,
			wood:1,
			stone:0,
			horse:0,
		},
		maxoutput:{
			iron:0,
			wood:100,
			stone:0,
			horse:0,
		},
	}
];

const	SH_LONGBOAT=	0;
const	SH_CLIPPER=		1;
const	SH_GALLEON=		2;

const shiptypes=
[
	{
		name:'longboat',
		capacity:200,
		cost:100,
	},
	{
		name:'clipper',
		capacity:800,
		cost:200,
	},
	{
		name:'galleon',
		capacity:1800,
		cost:300,
	},
];

const	SK_MINING=			0;
const	SK_LUMBERJACK=		1;
const	SK_QUARRYING=		2;
const	SK_HORSE_TRAINING=	3;
const	SK_WEAPONSMITH=		4;
const	SK_ARMORER=			5;
const	SK_BUILDING=		6;
const	SK_SHIPBUILDING=	7;
const	SK_ENTERTAINMENT=	8;
const	SK_STEALTH=			9;
const	SK_OBSERVATION=		10;
const	SK_TACTICS=			11;
const	SK_RIDING=			12;
const	SK_SWORD=			13;
const	SK_CROSSBOW=		14;
const	SK_LONGBOW=			15;
const	SK_MAGIC=			16;
const	MAXSKILLS=			17;

const skillnames =
[
	"mining",
	"lumberjack",
	"quarrying",
	"horse training",
	"weaponsmith",
	"armorer",
	"building",
	"shipbuilding",
	"entertainment",
	"stealth",
	"observation",
	"tactics",
	"riding",
	"sword",
	"crossbow",
	"longbow",
	"magic",
];

const	I_IRON=						0;
const	I_WOOD=						1;
const	I_STONE=					2;
const	I_HORSE=					3;
const	I_SWORD=					4;
const	I_CROSSBOW=					5;
const	I_LONGBOW=					6;
const	I_CHAIN_MAIL=				7;
const	I_PLATE_ARMOR=				8;
const	I_AMULET_OF_DARKNESS=		9;
const	I_AMULET_OF_DEATH=			10;
const	I_AMULET_OF_HEALING=		11;
const	I_AMULET_OF_TRUE_SEEING=	12;
const	I_CLOAK_OF_INVULNERABILITY=	13;
const	I_RING_OF_INVISIBILITY=		14;
const	I_RING_OF_POWER=			15;
const	I_RUNESWORD=				16;
const	I_SHIELDSTONE=				17;
const	I_STAFF_OF_FIRE=			18;
const	I_STAFF_OF_LIGHTNING=		19;
const	I_WAND_OF_TELEPORTATION=	20;
const	MAXITEMS=					21;

const items = [
	{
		singular:"iron",
		plural:"iron",
		skill:SK_MINING,
	},
	{
		singular:"wood",
		plural:"wood",
		skill:SK_LUMBERJACK,
	},
	{
		singular:"stone",
		plural:"stone",
		skill:SK_QUARRYING,
	},
	{
		singular:"horse",
		plural:"horses",
		skill:SK_HORSE_TRAINING,
	},
	{
		singular:"sword",
		plural:"swords",
		skill:SK_WEAPONSMITH,
		rawmaterial:I_IRON,
	},
	{
		singular:"crossbow",
		plural:"crossbows",
		skill:SK_WEAPONSMITH,
		rawmaterial:I_WOOD,
	},
	{
		singular:"longbow",
		plural:"longbows",
		skill:SK_WEAPONSMITH,
		rawmaterial:I_WOOD,
	},
	{
		singular:"chain mail",
		plural:"chain mail",
		skill:SK_ARMORER,
		rawmaterial:I_IRON,
	},
	{
		singular:"plate armor",
		plural:"plate armor",
		skill:SK_ARMORER,
		rawmaterial:I_IRON,
	},
	{
		singular:"Amulet of Darkness",
		plural:"Amulets of Darkness",
	},
	{
		singular:"Amulet of Death",
		plural:"Amulets of Death",
	},
	{
		singular:"Amulet of Healing",
		plural:"Amulets of Healing",
	},
	{
		singular:"Amulet of True Seeing",
		plural:"Amulets of True Seeing",
	},
	{
		singular:"Cloak of Invulnerability",
		plural:"Cloaks of Invulnerability",
	},
	{
		singular:"Ring of Invisibility",
		plural:"Rings of Invisibility",
	},
	{
		singular:"Ring of Power",
		plural:"Rings of Power",
	},
	{
		singular:"Runesword",
		plural:"Runeswords",
	},
	{
		singular:"Shieldstone",
		plural:"Shieldstones",
	},
	{
		singular:"Staff of Fire",
		plural:"Staffs of Fire",
	},
	{
		singular:"Staff of Lightning",
		plural:"Staffs of Lightning",
	},
	{
		singular:"Wand of Teleportation",
		plural:"Wands of Teleportation",
	},
];
	
const	SP_BLACK_WIND=				0;
const	SP_CAUSE_FEAR=				1;
const	SP_CONTAMINATE_WATER=			2;
const	SP_DAZZLING_LIGHT=			3;
const	SP_FIREBALL=				4;
const	SP_HAND_OF_DEATH=			5;
const	SP_HEAL=				6;
const	SP_INSPIRE_COURAGE=			7;
const	SP_LIGHTNING_BOLT=			8;
const	SP_MAKE_AMULET_OF_DARKNESS=		9;
const	SP_MAKE_AMULET_OF_DEATH=		10;
const	SP_MAKE_AMULET_OF_HEALING=		11;
const	SP_MAKE_AMULET_OF_TRUE_SEEING=		12;
const	SP_MAKE_CLOAK_OF_INVULNERABILITY=	13;
const	SP_MAKE_RING_OF_INVISIBILITY=		14;
const	SP_MAKE_RING_OF_POWER=			15;
const	SP_MAKE_RUNESWORD=			16;
const	SP_MAKE_SHIELDSTONE=			17;
const	SP_MAKE_STAFF_OF_FIRE=			18;
const	SP_MAKE_STAFF_OF_LIGHTNING=		19;
const	SP_MAKE_WAND_OF_TELEPORTATION=		20;
const	SP_SHIELD=				21;
const	SP_SUNFIRE=				22;
const	SP_TELEPORT=				23;
const	MAXSPELLS=				24;

const spelldata =
[
	{
		name:"Black Wind",
		level:4,
		iscombatspell:true,
		data:"This spell creates a black whirlwind of energy which destroys all life, "
		+"leaving frozen corpses with faces twisted into expressions of horror. Cast "
		+"in battle, it kills from 2 to 1250 enemies.",
	},
	{
		name:"Cause Fear",
		level:2,
		iscombatspell:true,
		data:"This spell creates an aura of fear which causes enemy troops in battle to "
		+"panic. Each time it is cast, it demoralizes between 2 and 100 troops for "
		+"the duration of the battle. Demoralized troops are at a -1 to their "
		+"effective skill.",
	},
	{
		name:"Contaminate Water",
		level:1,
		iscombatspell:false,
		data:"This ritual spell causes pestilence to contaminate the water supply of the "
		+"region in which it is cast. It causes from 2 to 50 peasants to die from "
		+"drinking the contaminated water. Any units which end the month in the "
		+"affected region will know about the deaths, however only units which have "
		+"Observation skill higher than the caster's Stealth skill will know who "
		+"was responsible. The spell costs $50 to cast.",
	},
	{
		name:"Dazzling Light",
		level:1,
		iscombatspell:true,
		data:"This spell, cast in battle, creates a flash of light which dazzles from 2 "
		+"to 50 enemy troops. Dazzled troops are at a -1 to their effective skill "
		+"for their next attack.",
	},
	{
		name:"Fireball",
		level:2,
		iscombatspell:true,
		data:"This spell enables the caster to hurl balls of fire. Each time it is cast "
		+"in battle, it will incinerate between 2 and 50 enemy troops.",
	},
	{
		name:"Hand of Death",
		level:3,
		iscombatspell:true,
		data:"This spell disrupts the metabolism of living organisms, sending an "
		+"invisible wave of death across a battlefield. Each time it is cast, it "
		+"will kill between 2 and 250 enemy troops.",
	},
	{
		name:"Heal",
		level:2,
		iscombatspell:false,
		data:"This spell enables the caster to attempt to heal the injured after a "
		+"battle. It is used automatically, and does not require use of either the "
		+"COMBAT or CAST command. If one's side wins a battle, a number of  "
		+"casualties on one's side, between 2 and 50, will be healed. (If this "
		+"results in all the casualties on the winning side being healed, the winner "
		+"is still eligible for combat experience.)",
	},
	{
		name:"Inspire Courage",
		level:2,
		iscombatspell:true,
		data:"This spell boosts the morale of one's troops in battle. Each time it is "
		+"cast, it cancels the effect of the Cause Fear spell on a number of one's "
		+"own troops ranging from 2 to 100.",
	},
	{
		name:"Lightning Bolt",
		level:1,
		iscombatspell:true,
		data:"This spell enables the caster to throw bolts of lightning to strike down "
		+"enemies in battle. It kills from 2 to 10 enemies.",
	},
	{
		name:"Make Amulet of Darkness",
		level:5,
		iscombatspell:false,
		spellitem:I_AMULET_OF_DARKNESS,
		data:"This spell allows one to create an Amulet of Darkness. This amulet allows "
		+"its possessor to cast the Black Wind spell in combat, without having to "
		+"know the spell; the only requirement is that the user must have the Magic "
		+"skill at 1 or higher. The Black Wind spell creates a black whirlwind of "
		+"energy which destroys all life. Cast in battle, it kills from 2 to 1250 "
		+"people. The amulet costs $1000 to make.",
	},
	{
		name:"Make Amulet of Death",
		level:4,
		iscombatspell:false,
		spellitem:I_AMULET_OF_DEATH,
		data:"This spell allows one to create an Amulet of Death. This amulet allows its "
		+"possessor to cast the Hand of Death spell in combat, without having to "
		+"know the spell; the only requirement is that the user must have the Magic "
		+"skill at 1 or higher. The Hand of Death spell disrupts the metabolism of "
		+"living organisms, sending an invisible wave of death across a battlefield. "
		+"Each time it is cast, it will kill between 2 and 250 enemy troops. The "
		+"amulet costs $800 to make.",
	},
	{
		name:"Make Amulet of Healing",
		level:3,
		iscombatspell:false,
		spellitem:I_AMULET_OF_HEALING,
		data:"This spell allows one to create an Amulet of Healing. This amulet allows "
		+"its possessor to attempt to heal the injured after a battle. It is used "
		+"automatically, and does not require the use of either the COMBAT or CAST "
		+"command; the only requirement is that the user must have the Magic skill "
		+"at 1 or higher. If the user's side wins a battle, a number of casualties "
		+"on that side, between 2 and 50, will be healed. (If this results in all "
		+"the casualties on the winning side being healed, the winner is still "
		+"eligible for combat experience.) The amulet costs $600 to make.",
	},
	{
		name:"Make Amulet of True Seeing",
		level:3,
		iscombatspell:false,
		spellitem:I_AMULET_OF_TRUE_SEEING,
		data:"This spell allows one to create an Amulet of True Seeing. This allows its "
		+"possessor to see units which are hidden by Rings of Invisibility. (It has "
		+"no effect against units which are hidden by the Stealth skill.) The amulet "
		+"costs $600 to make.",
	},
	{
		name:"Make Cloak of Invulnerability",
		level:3,
		iscombatspell:false,
		spellitem:I_CLOAK_OF_INVULNERABILITY,
		data:"This spell allows one to create a Cloak of Invulnerability. This cloak "
		+"protects its wearer from injury in battle; any attack with a normal weapon "
		+"which hits the wearer has a 99.99% chance of being deflected. This benefit "
		+"is gained instead of, rather than as well as, the protection of any armor "
		+"worn; and the cloak confers no protection against magical attacks. The "
		+"cloak costs $600 to make.",
	},
	{
		name:"Make Ring of Invisibility",
		level:3,
		iscombatspell:false,
		spellitem:I_RING_OF_INVISIBILITY,
		data:"This spell allows one to create a Ring of Invisibility. This ring renders "
		+"its wearer invisible to all units not in the same faction, regardless of "
		+"Observation skill. For a unit of many people to remain invisible, it must "
		+"possess a Ring of Invisibility for each person. The ring costs $600 to "
		+"make.",
	},
	{
		name:"Make Ring of Power",
		level:4,
		iscombatspell:false,
		spellitem:I_RING_OF_POWER,
		data:"This spell allows one to create a Ring of Power. This ring doubles the "
		+"effectiveness of any spell the wearer casts in combat, or any magic item "
		+"the wearer uses in combat. The ring costs $800 to make.",
	},
	{
		name:"Make Runesword",
		level:3,
		iscombatspell:false,
		spellitem:I_RUNESWORD,
		data:"This spell allows one to create a Runesword. This is a black sword with "
		+"magical runes etched along the blade. To use it, one must have both the "
		+"Sword and Magic skills at 1 or higher. It confers a bonus of 2 to the "
		+"user's Sword skill in battle, and also projects an aura of power that has "
		+"a 50% chance of cancelling any Fear spells cast by an enemy magician. The "
		+"sword costs $600 to make.",
	},
	{
		name:"Make Shieldstone",
		level:4,
		iscombatspell:false,
		spellitem:I_SHIELDSTONE,
		data:"This spell allows one to create a Shieldstone. This is a small black "
		+"stone, engraved with magical runes, that creates an invisible shield of "
		+"energy that deflects hostile magic in battle. The stone is used "
		+"automatically, and does not require the use of either the COMBAT or CAST "
		+"commands; the only requirement is that the user must have the Magic skill "
		+"at 1 or higher. Each round of combat, it adds one layer to the shielding "
		+"around one's own side. When a hostile magician casts a spell, provided "
		+"there is at least one layer of shielding present, there is a 50% chance of "
		+"the spell being deflected. If the spell is deflected, nothing happens. If "
		+"it is not, then it has full effect, and one layer of shielding is removed. "
		+"The stone costs $800 to make.",
	},
	{
		name:"Make Staff of Fire",
		level:3,
		iscombatspell:false,
		spellitem:I_STAFF_OF_FIRE,
		data:"This spell allows one to create a Staff of Fire. This staff allows its "
		+"possessor to cast the Fireball spell in combat, without having to know the "
		+"spell; the only requirement is that the user must have the Magic skill at "
		+"1 or higher. The Fireball spell enables the caster to hurl balls of fire. "
		+"Each time it is cast in battle, it will incinerate between 2 and 50 enemy "
		+"troops. The staff costs $600 to make.",
	},
	{
		name:"Make Staff of Lightning",
		level:2,
		iscombatspell:false,
		spellitem:I_STAFF_OF_LIGHTNING,
		data:"This spell allows one to create a Staff of Lightning. This staff allows "
		+"its possessor to cast the Lightning Bolt spell in combat, without having "
		+"to know the spell; the only requirement is that the user must have the "
		+"Magic skill at 1 or higher. The Lightning Bolt spell enables the caster to "
		+"throw bolts of lightning to strike down enemies. It kills from 2 to 10 "
		+"enemies. The staff costs $400 to make.",
	},
	{
		name:"Make Wand of Teleportation",
		level:4,
		iscombatspell:false,
		spellitem:I_WAND_OF_TELEPORTATION,
		data:"This spell allows one to create a Wand of Teleportation. This wand allows "
		+"its possessor to cast the Teleport spell, without having to know the "
		+"spell; the only requirement is that the user must have the Magic skill at "
		+"1 or higher. The Teleport spell allows the caster to move himself and "
		+"others across vast distances without traversing the intervening space. The "
		+"command to use it is CAST TELEPORT target-unit unit-no ... The target unit "
		+"is a unit in the region to which the teleport is to occur. If the target "
		+"unit is not in your faction, it must be in a faction which has issued an "
		+"ADMIT command for you that month. After the target unit comes a list of "
		+"one or more units to be teleported into the target unit's region (this may "
		+"optionally include the caster). Any units to be teleported, not in your "
		+"faction, must be in a faction which has issued an ACCEPT command for you "
		+"that month. The total weight of all units to be teleported (including "
		+"people, equipment and horses) must not exceed 10000. If the target unit is "
		+"in a building or on a ship, the teleported units will emerge there, "
		+"regardless of who owns the building or ship. The caster spends the month "
		+"preparing the spell and the teleport occurs at the end of the month, so "
		+"any other units to be transported can spend the month doing something "
		+"else. The wand costs $800 to make, and $50 to use.",
	},
	{
		name:"Shield",
		level:3,
		iscombatspell:true,
		data:"This spell creates an invisible shield of energy that deflects hostile "
		+"magic. Each round that it is cast in battle, it adds one layer to the "
		+"shielding around one's own side. When a hostile magician casts a spell, "
		+"provided there is at least one layer of shielding present, there is a 50% "
		+"chance of the spell being deflected. If the spell is deflected, nothing "
		+"happens. If it is not, then it has full effect, and one layer of shielding "
		+"is removed.",
	},
	{
		name:"Sunfire",
		level:5,
		iscombatspell:true,
		data:"This spell allows the caster to incinerate whole armies with fire brighter "
		+"than the sun. Each round it is cast, it kills from 2 to 6250 enemies.",
	},
	{
		name:"Teleport",
		level:3,
		iscombatspell:false,
		data:"This spell allows the caster to move himself and others across vast "
		+"distances without traversing the intervening space. The command to use it "
		+"is CAST TELEPORT target-unit unit-no ... The target unit is a unit in the "
		+"region to which the teleport is to occur. If the target unit is not in "
		+"your faction, it must be in a faction which has issued an ADMIT command "
		+"for you that month. After the target unit comes a list of one or more "
		+"units to be teleported into the target unit's region (this may optionally "
		+"include the caster). Any units to be teleported, not in your faction, must "
		+"be in a faction which has issued an ACCEPT command for you that month. The "
		+"total weight of all units to be teleported (including people, equipment "
		+"and horses) must not exceed 10000. If the target unit is in a building or "
		+"on a ship, the teleported units will emerge there, regardless of who owns "
		+"the building or ship. The caster spends the month preparing the spell and "
		+"the teleport occurs at the end of the month, so any other units to be "
		+"transported can spend the month doing something else. The spell costs $50 "
		+"to cast.",
	},
];

function findkeyword (word)
{
	var w;

	switch(word) {
		case 'describe':
			return(K_DISPLAY);
		case 'n':
			return(K_NORTH);
		case 's':
			return(K_SOUTH);
		case 'e':
			return(K_EAST);
		case 'w':
			return(K_WEST);
	}

	for(w in keywords)
		if(keywords[w]==word)
			return(w);

	return(-1);
}

function findskill(s)
{
	var w;

	switch(s) {
		case "horse":
			return(SK_HORSE_TRAINING);
		case "entertain":
			return(SK_ENTERTAINMENT);
	}

	for(w in skillnames)
		if(skillnames[w]==s)
			return(w);

	return(-1);
}

function finditem (s)
{
	var w;

	switch(s) {
		case "chain":
			return(I_CHAIN_MAIL);
		case "plate":
			return(I_PLATE_ARMOR);
	}

	for(w in items)
		if(items[w].singular==s || items[w].plural==s)
			return(w);

	return(-1);
}

function findspell (s)
{
	var w;

	for(w in spelldata)
		if(spelldata[w].name==s)
			return(w);

	return(-1);
}

