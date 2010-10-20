var Help_Topics=[];

Help_Topics["GOTO"]=[
	"move to a different room:",
	"goto <zone id number> <room id number>"
];
Help_Topics["OPEN"]=[
	"open a container or door:",
	"open <container keyword>",
	"open <door keyword>",
	"see topics: 'close', 'lock', 'unlock'"
];
Help_Topics["CLOSE"]=[
	"close a container or door:",
	"close <container keyword>",
	"close <door keyword>",
	"see topics: 'open', 'lock', 'unlock'"
];
Help_Topics["REMOVE"]=[
	"remove a piece of equipment:",
	"remove <item keyword>"
];
Help_Topics["WEAR"]=[
	"wear a piece of equipment:",
	"wear <item keyword>"
];
Help_Topics["WIELD"]=[
	"wield a weapon:",
	"wield <item keyword>"
];
Help_Topics["LOCK"]=[
	"lock a container or door:",
	"lock <container keyword>",
	"lock <door keyword>",
	"you must have the key in your inventory",
	"see topics: 'open', 'close', 'unlock'"
];
Help_Topics["UNLOCK"]=[
	"unlock a container or door:",
	"unlock <container keyword>",
	"unlock <door keyword>",
	"you must have the key in your inventory",
	"see topics: 'open', 'close', 'lock'"
];
Help_Topics["PUT"]=[
	"place an item in a container:",
	"put <item keyword> <container>",
	"put all <container>"
];
Help_Topics["GET"]=[
	"retrieve an item:",
	"get <item keyword>",
	"get <item keyword> <container>",
	"get all",
	"get all <container>",
	"note: a corpse is a container"
];
Help_Topics["DROP"]=[
	"drop an inventory item in the room:",
	"drop <item keyword>"
];
Help_Topics["UNLINK"]=[
	"clear an exit link setting:",
	"unlink [north,south,east,west,up,down]",
	"see topic: 'set','link'"
];
Help_Topics["LINK"]=[
	"link an exit to another room:",
	"link [north,south,east,west,up,down] <room id number>",
	"see topic: 'set','unlink'"
];
Help_Topics["SET"]=[
	"change a setting:",
	"set [new/current] [title/description] <text>",
	"set autolink on/off",
	"note: autolink will automatically add adjacent",
	" room exit links for you, if any. they can still",
	" be set/unset manually"
];
Help_Topics["NEW"]=[
	"create a new zone or object:",
	"new zone <level> <title>",
	"new item <not yet implemented>",
	"new mob <not yet implemented>",
	"new door <not yet implemented>"
];
Help_Topics["CREATE"]=[
	"To start playing, you must first create a character.",
	"To see a list of character races and classes, type 'rpg classes' and 'rpg races'.",
	"When you have settled on a combination, type 'rpg create <race> <class>.",
	"Once your character is created type 'rpg login' to start playing!"
]
Help_Topics["EDIT"]=[
	"The default world is very small. To get comfortable you may want to start by creating a new zone, and seeing how everything works.",
	"You must first create a character and login before you can edit. Once you have logged in, type 'rpg edit' to toggle the editor on/off.",
	"To create a new zone, turn the editor on and type 'new zone <level> <my new zone name>'.",
	"By default, all exits will link automatically if the editor finds a room that is adjacent to you upon creating a new room. You can either turn this feature off, or modify those links later if you choose.",
	"To create new rooms, simply walk in any direction while in edit mode.",
	"see topics: 'set','unset','new','goto'"
];
Help_Topics["KILL"]=[
	"attack a mob/person:",
	"kill <target keyword>"
];
Help_Topics["FLEE"]=[
	"run away from battle:",
	"see topic: 'kill'" 
];
