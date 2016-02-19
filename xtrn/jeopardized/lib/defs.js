// Main module states
const	STATE_MENU = 0,
		STATE_GAME = 1,
		STATE_SCORE = 2,
		STATE_NEWS = 3,
		STATE_HELP = 4,
		STATE_CREDIT = 5,
		STATE_QUIT = 6,
		STATE_MAINTENANCE = 7,
		STATE_POPUP = 8;

// Menu module states
const	STATE_MENU_TREE = 0,
		STATE_MENU_MESSAGES = 1;

// Game module states
const	STATE_GAME_PLAY = 0,
		STATE_GAME_MESSAGES = 1,
		STATE_GAME_POPUP = 2;

// Round module states
const 	STATE_ROUND_BOARD = 0,
		STATE_ROUND_CLUE = 1,
		STATE_ROUND_ANSWER = 2,
		STATE_ROUND_POPUP = 3,
		STATE_ROUND_WAGER = 4,
		STATE_ROUND_COMPLETE = 5;

// Clue module states
const	STATE_CLUE_INPUT = 0;

// PopUp prompt modes
const	PROMPT_ANY = 0,
		PROMPT_YN = 1,
		PROMPT_TEXT = 2,
		PROMPT_NUMBER = 3;

// Round module return values
const	RET_ROUND_QUIT = 0,		// User opted to quit
		RET_ROUND_CONTINUE = 1, // Clues available & can't advance yet
		RET_ROUND_NEXT = 2,		// Can advance to next round
		RET_ROUND_STOP = 3,		// No more clues & can't advance
		RET_ROUND_LAST = 4;		// Round 3 complete