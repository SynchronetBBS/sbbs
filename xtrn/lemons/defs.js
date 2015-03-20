// Magic 'state' numbers used by most Lemons modules and the main loop.
const	STATE_MENU = 0,
		STATE_PLAY = 1,
		STATE_PAUSE = 2,
		STATE_HELP = 3,
		STATE_SCORES = 4,
		STATE_EXIT = 5;

/*	Magic numbers returned by Level.cycle() and Level.getcmd(), to be
	evaluated by the Game object. */
const	LEVEL_DEAD = 0,
		LEVEL_TIME = 1,
		LEVEL_NEXT = 2,
		LEVEL_CONTINUE = 3;

//	Magic numbers for state-tracking within the Game object
const	GAME_STATE_CHOOSELEVEL = 0,
		GAME_STATE_NORMAL = 1,
		GAME_STATE_POPUP = 2;

/*	These are used by the Level object during colorization and lemonization of
	skilled lemons, and also to colorize the skill list in the status bar. You
	can change these values if you want. */
const	COLOUR_LEMON = YELLOW,
		COLOUR_BASHER = LIGHTMAGENTA,
		COLOUR_BLOCKER = GREEN,
		COLOUR_BOMBER = LIGHTRED,
		COLOUR_BUILDER = LIGHTGREEN,
		COLOUR_CLIMBER = LIGHTCYAN,
		COLOUR_DIGGER = CYAN,
		COLOUR_NUKED = LIGHTRED,
		COLOUR_DYING = LIGHTGRAY,
		// The default status bar colours
		COLOUR_STATUSBAR_BG = BG_BLUE,
		COLOUR_STATUSBAR_FG = WHITE;

// The name of the JSON database.  You probably don't need to change this.
const	DBNAME = "LEMONS";

const	KEY_BASH = "A",
		KEY_BLOCK = "S",
		KEY_BOMB = "F",
		KEY_BUILD = "V",
		KEY_CLIMB = "C",
		KEY_DIG = "D";