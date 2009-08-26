
void Game_Init(void);
/*
 * Initializes Game by reading in the GAME.DAT file.  If file not found,
 * outputs error message and returns to DOS.
 */

void Game_Close(void);
/*
 * Deinitializes Game.
 */

void Game_Settings(void);

void Game_Start(void);

void Game_Write(void);
/*
 * Writes out the game data.
 */
