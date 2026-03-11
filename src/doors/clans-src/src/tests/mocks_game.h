/*
 * mocks_game.h
 *
 * Shared mocks for game.h functionality.  Provides Game struct and
 * game management stubs used by 8+ test files.
 */

#ifndef MOCKS_GAME_H
#define MOCKS_GAME_H

struct game Game;

void Game_Init(void)
{
}

void Game_Close(void)
{
}

void Game_Settings(void)
{
}

void Game_Start(void)
{
}

void Game_Write(void)
{
}

#endif /* MOCKS_GAME_H */
