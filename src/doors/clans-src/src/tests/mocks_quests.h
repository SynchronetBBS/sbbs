/*
 * mocks_quests.h
 *
 * Shared mocks for quests.h functionality.  Provides Quest and Quests_TFlags
 * array definitions used by 14+ test files.
 */

#ifndef MOCKS_QUESTS_H
#define MOCKS_QUESTS_H

struct Quest Quests[MAX_QUESTS];
uint8_t Quests_TFlags[8];

#endif /* MOCKS_QUESTS_H */
