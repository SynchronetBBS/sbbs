# Clans Source Code Function Inventory

This document lists all functions defined in the Clans source files, their prototypes, descriptions, and internal function calls.

## tools.c

### ato8
**Prototype:** `int8_t ato8(const char *str, const char *desc)`
**Description:** Parses a signed 8-bit integer from a string with range validation and error reporting.
**Calls:** None

---

### atoc
**Prototype:** `char atoc(const char *str, const char *desc)`
**Description:** Parses a signed char from a string with range validation and error reporting.
**Calls:** None

---

### ato16
**Prototype:** `int16_t ato16(const char *str, const char *desc)`
**Description:** Parses a signed 16-bit integer from a string with range validation and error reporting.
**Calls:** None

---

### ato32
**Prototype:** `int32_t ato32(const char *str, const char *desc)`
**Description:** Parses a signed 32-bit integer from a string with range validation and error reporting.
**Calls:** None

---

## random.c

### my_random
**Prototype:** `int my_random(int limit)`
**Description:** Generates a pseudo-random integer in the range [0, limit) using rejection sampling to avoid modulo bias.
**Calls:** `System_Error`

---

## semfile.c

### CreateSemaphor
**Prototype:** `bool CreateSemaphor(uint16_t Node)`
**Description:** Attempts to create or acquire an exclusive semaphore file for single-instance game locking; uses NFS-safe link-based atomic operations on Unix.
**Calls:** None

---

### WaitSemaphor
**Prototype:** `void WaitSemaphor(uint16_t Node)`
**Description:** Blocks until an exclusive semaphore can be acquired, polling once per second with status logging.
**Calls:** `CreateSemaphor`, `LogDisplayStr`

---

### RemoveSemaphor
**Prototype:** `void RemoveSemaphor(void)`
**Description:** Deletes the online.flg semaphore file to release the exclusive lock.
**Calls:** None

---

## misc.c

### atoc
**Prototype:** `char atoc(const char *str, const char *desc, const char *func)`
**Description:** Parses a signed char from a string with detailed error reporting including function name and range bounds.
**Calls:** `System_Error`

---

### ato8
**Prototype:** `int8_t ato8(const char *str, const char *desc, const char *func)`
**Description:** Parses a signed 8-bit integer from a string with detailed error reporting including function name and range bounds.
**Calls:** `System_Error`

---

### atou8
**Prototype:** `uint8_t atou8(const char *str, const char *desc, const char *func)`
**Description:** Parses an unsigned 8-bit integer from a string with detailed error reporting including function name and range bounds.
**Calls:** `System_Error`

---

### ato16
**Prototype:** `int16_t ato16(const char *str, const char *desc, const char *func)`
**Description:** Parses a signed 16-bit integer from a string with detailed error reporting including function name and range bounds.
**Calls:** `System_Error`

---

### atou16
**Prototype:** `uint16_t atou16(const char *str, const char *desc, const char *func)`
**Description:** Parses an unsigned 16-bit integer from a string with detailed error reporting including function name and range bounds.
**Calls:** `System_Error`

---

### ato32
**Prototype:** `int32_t ato32(const char *str, const char *desc, const char *func)`
**Description:** Parses a signed 32-bit integer from a string with detailed error reporting including function name and range bounds.
**Calls:** `System_Error`

---

### atoul
**Prototype:** `unsigned long atoul(const char *str, const char *desc, const char *func)`
**Description:** Parses an unsigned long integer from a string with detailed error reporting including function name.
**Calls:** `System_Error`

---

### atoull
**Prototype:** `unsigned long long atoull(const char *str, const char *desc, const char *func)`
**Description:** Parses an unsigned long long integer from a string with detailed error reporting including function name.
**Calls:** `System_Error`

---

## parsing.c

### ParseLine
**Prototype:** `void ParseLine(char *szString)`
**Description:** Removes comments (# and ;) and leading/trailing whitespace from a configuration line, preserving content within quotes.
**Calls:** `Strip`

---

### GetToken
**Prototype:** `void GetToken(char *szString, char *szToken)`
**Description:** Extracts the first whitespace-delimited token from a string and removes it from the source, modifying both in place.
**Calls:** None

---

## platform.c

### strlcpy
**Prototype:** `size_t strlcpy(char *dst, const char *src, size_t dsize)`
**Description:** Safely copies a string to a fixed-size buffer with NUL termination; returns total source length.
**Calls:** None

---

### strlcat
**Prototype:** `size_t strlcat(char *dst, const char *src, size_t dsize)`
**Description:** Safely concatenates a string to a fixed-size buffer with NUL termination; returns total concatenated length.
**Calls:** None

---

### CRCValue
**Prototype:** `int32_t CRCValue(const void *Data, ptrdiff_t DataSize)`
**Description:** Computes a simple checksum by summing all bytes in a buffer.
**Calls:** None

---

### isRelative
**Prototype:** `bool isRelative(const char *fname)`
**Description:** Determines if a file path is relative (not absolute) accounting for platform-specific path separators.
**Calls:** None

---

### SameFile
**Prototype:** `bool SameFile(const char *f1, const char *f2)`
**Description:** Compares two file paths for equivalence using case-insensitive matching and fallback filename-only comparison for relative paths.
**Calls:** `isRelative`, `FileName`

---

### DirExists
**Prototype:** `bool DirExists(const char *pszDirName)`
**Description:** Checks whether a directory exists on the filesystem using platform-specific stat or findfirst APIs.
**Calls:** None

---

### Strip
**Prototype:** `void Strip(char *szString)`
**Description:** Removes leading and trailing whitespace from a string in place.
**Calls:** None

---

### iscodechar
**Prototype:** `bool iscodechar(char c)`
**Description:** Determines if a character is a valid hexadecimal digit (0-9, A-F) for color code parsing.
**Calls:** None

---

### RemovePipes
**Prototype:** `void RemovePipes(char *pszSrc, char *pszDest)`
**Description:** Strips color codes (| and ` format sequences) from a string and writes the plain text result to a destination buffer.
**Calls:** `iscodechar`

---

### IsLeapYear
**Prototype:** `static bool IsLeapYear(int year)`
**Description:** Determines if a Gregorian calendar year is a leap year.
**Calls:** None

---

### DaysSinceJan1
**Prototype:** `static int32_t DaysSinceJan1(char szTheDate[])`
**Description:** Calculates the number of days elapsed since January 1st for a date in MM/DD/YYYY format.
**Calls:** `IsLeapYear`

---

### DaysSince1970
**Prototype:** `int32_t DaysSince1970(char szTheDate[])`
**Description:** Calculates the number of days elapsed since January 1, 1970 for a date in MM/DD/YYYY format.
**Calls:** `IsLeapYear`, `DaysSinceJan1`

---

### DaysBetween
**Prototype:** `int32_t DaysBetween(char szFirstDate[], char szLastDate[])`
**Description:** Calculates the number of days between two dates in MM/DD/YYYY format (LastDate - FirstDate).
**Calls:** `DaysSince1970`

---

## language.c

### LoadStrings
**Prototype:** `void LoadStrings(int16_t StartIndex, int16_t NumStrings, char *szStrings[])`
**Description:** Populates an array of pointers with addresses of strings from the loaded language file at specified indices.
**Calls:** None

---

### Language_Init
**Prototype:** `void Language_Init(char *szLangFile)`
**Description:** Loads the binary language file into memory, decompressing string offsets and allocating storage for the string buffer.
**Calls:** `LogDisplayStr`, `delay`, `MyOpen`, `System_Error`, `CheckMem`, `s_Language_d`

---

### Language_Close
**Prototype:** `void Language_Close(void)`
**Description:** Frees allocated language data from memory.
**Calls:** None

---

## serialize.c

### s_ibbs_node_attack_s
**Prototype:** `size_t s_ibbs_node_attack_s(const struct ibbs_node_attack *s, void *bufptr, size_t bufsz)`
**Description:** Serializes an IBBS node attack structure (ReceiveIndex, SendIndex) into a binary buffer.
**Calls:** None

### s_MessageHeader_s
**Prototype:** `size_t s_MessageHeader_s(const struct MessageHeader *s, void *bufptr, size_t bufsz)`
**Description:** Serializes a message header structure (from/to usernames, subject, metadata, routing info) into binary format.
**Calls:** None

### s_ibbs_node_reset_s
**Prototype:** `size_t s_ibbs_node_reset_s(const struct ibbs_node_reset *s, void *bufptr, size_t bufsz)`
**Description:** Serializes IBBS node reset data (Received count, LastSent timestamp).
**Calls:** None

### s_ibbs_node_recon_s
**Prototype:** `size_t s_ibbs_node_recon_s(const struct ibbs_node_recon *s, void *bufptr, size_t bufsz)`
**Description:** Serializes IBBS node reconnection data (LastReceived, LastSent, PacketIndex).
**Calls:** None

### s_Msg_Txt_s
**Prototype:** `size_t s_Msg_Txt_s(const struct Msg_Txt *s, void *bufptr, size_t bufsz)`
**Description:** Serializes message text structure (offsets array, length, number of lines, text pointer).
**Calls:** None

### s_Message_s
**Prototype:** `size_t s_Message_s(const struct Message *s, void *bufptr, size_t bufsz)`
**Description:** Serializes a complete message (clan IDs, sender/date info, alliance name, message type, indices).
**Calls:** s_Msg_Txt_s

### s_Topic_s
**Prototype:** `size_t s_Topic_s(const struct Topic *s, void *bufptr, size_t bufsz)`
**Description:** Serializes a topic structure (Known, Active, ClanInfo bools, name and label).
**Calls:** None

### s_LeavingData_s
**Prototype:** `size_t s_LeavingData_s(const struct LeavingData *s, void *bufptr, size_t bufsz)`
**Description:** Serializes clan leaving data (active flag, destination ID, clan IDs, troop counts).
**Calls:** None

### s_PClass_s
**Prototype:** `size_t s_PClass_s(const struct PClass *s, void *bufptr, size_t bufsz)`
**Description:** Serializes player class data (name, attributes array, HP/SP limits, gold, spells known, village type).
**Calls:** None

### s_SpellsInEffect_s
**Prototype:** `size_t s_SpellsInEffect_s(const struct SpellsInEffect *s, void *bufptr, size_t bufsz)`
**Description:** Serializes an active spell effect (spell number, energy remaining).
**Calls:** None

### s_SpyAttemptPacket_s
**Prototype:** `size_t s_SpyAttemptPacket_s(const struct SpyAttemptPacket *s, void *bufptr, size_t bufsz)`
**Description:** Serializes a spy attempt packet (spier name, intelligence level, target type, clan/master IDs, BBS IDs).
**Calls:** None

### s_SpyResultPacket_s
**Prototype:** `size_t s_SpyResultPacket_s(const struct SpyResultPacket *s, void *bufptr, size_t bufsz)`
**Description:** Serializes spy attempt results (BBS IDs, master ID, target name, success flag, empire state, date).
**Calls:** s_empire_s

### s_TradeData_s
**Prototype:** `size_t s_TradeData_s(const struct TradeData *s, void *bufptr, size_t bufsz)`
**Description:** Serializes trade offer data (giving/asking lists, active flag, clan IDs, clan names, trade code).
**Calls:** s_TradeList_s

### s_TradeList_s
**Prototype:** `size_t s_TradeList_s(const struct TradeList *s, void *bufptr, size_t bufsz)`
**Description:** Serializes a trade list (gold, follower and troop counts).
**Calls:** None

### s_UserInfo_s
**Prototype:** `size_t s_UserInfo_s(const struct UserInfo *s, void *bufptr, size_t bufsz)`
**Description:** Serializes user info (clan ID, deleted flag, master/character names).
**Calls:** None

### s_UserScore_s
**Prototype:** `size_t s_UserScore_s(const struct UserScore *s, void *bufptr, size_t bufsz)`
**Description:** Serializes user score entry (clan ID, symbol, name, points, BBS ID).
**Calls:** None

### s_Spell_s
**Prototype:** `size_t s_Spell_s(const struct Spell *s, void *bufptr, size_t bufsz)`
**Description:** Serializes spell data (name, type, attributes, effect strings, SP cost, multiaffect bitfield).
**Calls:** None

### s_pc_s
**Prototype:** `size_t s_pc_s(const struct pc *s, void *bufptr, size_t bufsz)`
**Description:** Serializes player character data (name, stats, attributes, equipment, spells, status, CRC).
**Calls:** CRCValue

### s_Packet_s
**Prototype:** `size_t s_Packet_s(const struct Packet *s, void *bufptr, size_t bufsz)`
**Description:** Serializes IBBS packet header (active, game ID, date, BBS IDs, packet type and length).
**Calls:** None

### s_NPCInfo_s
**Prototype:** `size_t s_NPCInfo_s(const struct NPCInfo *s, void *bufptr, size_t bufsz)`
**Description:** Serializes NPC information (name, topics, loyalty, wandering info, stats, file references).
**Calls:** s_Topic_s

### s_NPCNdx_s
**Prototype:** `size_t s_NPCNdx_s(const struct NPCNdx *s, void *bufptr, size_t bufsz)`
**Description:** Serializes NPC index entry (file index, clan membership, wandering location, status).
**Calls:** None

### s_Language_s
**Prototype:** `size_t s_Language_s(const struct Language *s, void *bufptr, size_t bufsz)`
**Description:** Serializes language data (signature, string offset table, byte count, string blob pointer).
**Calls:** None

### s_Alliance_s
**Prototype:** `size_t s_Alliance_s(const struct Alliance *s, void *bufptr, size_t bufsz)`
**Description:** Serializes alliance data (ID, name, creator IDs, members, empire, items).
**Calls:** s_empire_s, s_item_data_s

### s_game_data_s
**Prototype:** `size_t s_game_data_s(const struct game_data *s, void *bufptr, size_t bufsz)`
**Description:** Serializes global game state (game state, inter-BBS flag, dates, clan/alliance IDs, settings, CRC).
**Calls:** CRCValue

### s_village_data_s
**Prototype:** `size_t s_village_data_s(const struct village_data *s, void *bufptr, size_t bufsz)`
**Description:** Serializes village data (color scheme, name, governance, tax/interest rates, building levels, flags, empire, CRC).
**Calls:** CRCValue, s_empire_s

### s_EventHeader_s
**Prototype:** `size_t s_EventHeader_s(const struct EventHeader *s, void *bufptr, size_t bufsz)`
**Description:** Serializes event header (name, event size, event flag).
**Calls:** None

### s_FileHeader_s
**Prototype:** `size_t s_FileHeader_s(const struct FileHeader *s, void *bufptr, size_t bufsz)`
**Description:** Serializes file header (file pointer, name, start/end positions, file size).
**Calls:** None

### s_Army_s
**Prototype:** `size_t s_Army_s(const struct Army *s, void *bufptr, size_t bufsz)`
**Description:** Serializes army data (troop counts, rating, level, strategy, CRC).
**Calls:** CRCValue, s_Strategy_s

### s_Strategy_s
**Prototype:** `size_t s_Strategy_s(const struct Strategy *s, void *bufptr, size_t bufsz)`
**Description:** Serializes combat strategy (attack length/intensity, loot level, defend length/intensity).
**Calls:** None

### s_AttackPacket_s
**Prototype:** `size_t s_AttackPacket_s(const struct AttackPacket *s, void *bufptr, size_t bufsz)`
**Description:** Serializes attack packet (BBS IDs, attacking empire/army, goal, extent, target, clan/originator IDs, CRC).
**Calls:** CRCValue, s_empire_s, s_Army_s

### s_AttackResult_s
**Prototype:** `size_t s_AttackResult_s(const struct AttackResult *s, void *bufptr, size_t bufsz)`
**Description:** Serializes attack result (success/target flags, armies, attacker/defender info, damage, casualties, loot, CRC).
**Calls:** CRCValue, s_Army_s

### s_item_data_s
**Prototype:** `size_t s_item_data_s(const struct item_data *s, void *bufptr, size_t bufsz)`
**Description:** Serializes item data (available, used-by, name, type, attributes, cost, energy, market level, village type, HP/SP bonuses).
**Calls:** None

### s_empire_s
**Prototype:** `size_t s_empire_s(const struct empire *s, void *bufptr, size_t bufsz)`
**Description:** Serializes empire data (name, owner type, vault gold, land, buildings, alliance ID, worker energy, development/spy/attack counters, points, army, CRC).
**Calls:** CRCValue, s_Army_s

### s_clan_s
**Prototype:** `size_t s_clan_s(const struct clan *s, void *bufptr, size_t bufsz)`
**Description:** Serializes clan data (clan ID, names, symbol, quest/flag arrays, daily counters, votes, points, dates, combat stats, members, items, empire, help flags, CRC).
**Calls:** CRCValue, s_empire_s, s_item_data_s

---

## deserialize.c

### s_ibbs_node_attack_d
**Prototype:** `size_t s_ibbs_node_attack_d(const void *bufptr, size_t bufsz, struct ibbs_node_attack *s)`
**Description:** Deserializes IBBS node attack structure from binary buffer.
**Calls:** None

### s_MessageHeader_d
**Prototype:** `size_t s_MessageHeader_d(const void *bufptr, size_t bufsz, struct MessageHeader *s)`
**Description:** Deserializes message header from binary format.
**Calls:** None

### s_ibbs_node_reset_d
**Prototype:** `size_t s_ibbs_node_reset_d(const void *bufptr, size_t bufsz, struct ibbs_node_reset *s)`
**Description:** Deserializes IBBS node reset data from buffer.
**Calls:** None

### s_ibbs_node_recon_d
**Prototype:** `size_t s_ibbs_node_recon_d(const void *bufptr, size_t bufsz, struct ibbs_node_recon *s)`
**Description:** Deserializes IBBS node reconnection data.
**Calls:** None

### s_Msg_Txt_d
**Prototype:** `size_t s_Msg_Txt_d(const void *bufptr, size_t bufsz, struct Msg_Txt *s)`
**Description:** Deserializes message text structure.
**Calls:** None

### s_Message_d
**Prototype:** `size_t s_Message_d(const void *bufptr, size_t bufsz, struct Message *s)`
**Description:** Deserializes a complete message structure.
**Calls:** s_Msg_Txt_d

### s_Topic_d
**Prototype:** `size_t s_Topic_d(const void *bufptr, size_t bufsz, struct Topic *s)`
**Description:** Deserializes a topic structure.
**Calls:** None

### s_LeavingData_d
**Prototype:** `size_t s_LeavingData_d(const void *bufptr, size_t bufsz, struct LeavingData *s)`
**Description:** Deserializes clan leaving data.
**Calls:** None

### s_PClass_d
**Prototype:** `size_t s_PClass_d(const void *bufptr, size_t bufsz, struct PClass *s)`
**Description:** Deserializes player class data.
**Calls:** None

### s_SpellsInEffect_d
**Prototype:** `size_t s_SpellsInEffect_d(const void *bufptr, size_t bufsz, struct SpellsInEffect *s)`
**Description:** Deserializes an active spell effect.
**Calls:** None

### s_SpyAttemptPacket_d
**Prototype:** `size_t s_SpyAttemptPacket_d(const void *bufptr, size_t bufsz, struct SpyAttemptPacket *s)`
**Description:** Deserializes a spy attempt packet.
**Calls:** None

### s_SpyResultPacket_d
**Prototype:** `size_t s_SpyResultPacket_d(const void *bufptr, size_t bufsz, struct SpyResultPacket *s)`
**Description:** Deserializes spy attempt results.
**Calls:** s_empire_d

### s_TradeData_d
**Prototype:** `size_t s_TradeData_d(const void *bufptr, size_t bufsz, struct TradeData *s)`
**Description:** Deserializes trade offer data.
**Calls:** s_TradeList_d

### s_TradeList_d
**Prototype:** `size_t s_TradeList_d(const void *bufptr, size_t bufsz, struct TradeList *s)`
**Description:** Deserializes a trade list.
**Calls:** None

### s_UserInfo_d
**Prototype:** `size_t s_UserInfo_d(const void *bufptr, size_t bufsz, struct UserInfo *s)`
**Description:** Deserializes user info.
**Calls:** None

### s_UserScore_d
**Prototype:** `size_t s_UserScore_d(const void *bufptr, size_t bufsz, struct UserScore *s)`
**Description:** Deserializes user score entry.
**Calls:** None

### s_Spell_d
**Prototype:** `size_t s_Spell_d(const void *bufptr, size_t bufsz, struct Spell *s)`
**Description:** Deserializes spell data and reconstructs MultiAffect bitfield.
**Calls:** None

### s_pc_d
**Prototype:** `size_t s_pc_d(const void *bufptr, size_t bufsz, struct pc *s)`
**Description:** Deserializes player character and verifies CRC, calls System_Error if corrupt.
**Calls:** CRCValue, System_Error

### s_Packet_d
**Prototype:** `size_t s_Packet_d(const void *bufptr, size_t bufsz, struct Packet *s)`
**Description:** Deserializes IBBS packet header.
**Calls:** None

### s_NPCInfo_d
**Prototype:** `size_t s_NPCInfo_d(const void *bufptr, size_t bufsz, struct NPCInfo *s)`
**Description:** Deserializes NPC information.
**Calls:** s_Topic_d

### s_NPCNdx_d
**Prototype:** `size_t s_NPCNdx_d(const void *bufptr, size_t bufsz, struct NPCNdx *s)`
**Description:** Deserializes NPC index entry.
**Calls:** None

### s_Language_d
**Prototype:** `size_t s_Language_d(const void *bufptr, size_t bufsz, struct Language *s)`
**Description:** Deserializes language data.
**Calls:** None

### s_Alliance_d
**Prototype:** `size_t s_Alliance_d(const void *bufptr, size_t bufsz, struct Alliance *s)`
**Description:** Deserializes alliance data.
**Calls:** s_empire_d, s_item_data_d

### s_game_data_d
**Prototype:** `size_t s_game_data_d(const void *bufptr, size_t bufsz, struct game_data *s)`
**Description:** Deserializes global game state and verifies CRC, calls System_Error if corrupt.
**Calls:** CRCValue, System_Error

### s_village_data_d
**Prototype:** `size_t s_village_data_d(const void *bufptr, size_t bufsz, struct village_data *s)`
**Description:** Deserializes village data and verifies CRC, reconstructs bitfields, calls System_Error if corrupt.
**Calls:** CRCValue, System_Error, s_empire_d

### s_EventHeader_d
**Prototype:** `size_t s_EventHeader_d(const void *bufptr, size_t bufsz, struct EventHeader *s)`
**Description:** Deserializes event header.
**Calls:** None

### s_FileHeader_d
**Prototype:** `size_t s_FileHeader_d(const void *bufptr, size_t bufsz, struct FileHeader *s)`
**Description:** Deserializes file header.
**Calls:** None

### s_Army_d
**Prototype:** `size_t s_Army_d(const void *bufptr, size_t bufsz, struct Army *s)`
**Description:** Deserializes army data and verifies CRC, calls System_Error if corrupt.
**Calls:** CRCValue, System_Error, s_Strategy_d

### s_Strategy_d
**Prototype:** `size_t s_Strategy_d(const void *bufptr, size_t bufsz, struct Strategy *s)`
**Description:** Deserializes combat strategy.
**Calls:** None

### s_AttackPacket_d
**Prototype:** `size_t s_AttackPacket_d(const void *bufptr, size_t bufsz, struct AttackPacket *s)`
**Description:** Deserializes attack packet and verifies CRC, calls System_Error if corrupt.
**Calls:** CRCValue, System_Error, s_empire_d, s_Army_d

### s_AttackResult_d
**Prototype:** `size_t s_AttackResult_d(const void *bufptr, size_t bufsz, struct AttackResult *s)`
**Description:** Deserializes attack result and verifies CRC, calls System_Error if corrupt.
**Calls:** CRCValue, System_Error, s_Army_d

### s_item_data_d
**Prototype:** `size_t s_item_data_d(const void *bufptr, size_t bufsz, struct item_data *s)`
**Description:** Deserializes item data.
**Calls:** None

### s_empire_d
**Prototype:** `size_t s_empire_d(const void *bufptr, size_t bufsz, struct empire *s)`
**Description:** Deserializes empire data and verifies CRC, calls System_Error if corrupt.
**Calls:** CRCValue, System_Error, s_Army_d

### s_clan_d
**Prototype:** `size_t s_clan_d(const void *bufptr, size_t bufsz, struct clan *s)`
**Description:** Deserializes clan data, reconstructs bitfields from packed bytes, verifies CRC, calls System_Error if corrupt.
**Calls:** CRCValue, System_Error, s_empire_d, s_item_data_d

---

## items.c

### GetOpenItemSlot
**Prototype:** `int16_t GetOpenItemSlot(struct clan *Clan)`
**Description:** Finds first empty inventory slot in clan's items array.
**Calls:** None

### GetUnusedItem
**Prototype:** `static int16_t GetUnusedItem(struct clan *Clan, int16_t type)`
**Description:** Finds first unequipped item of specified type in clan inventory.
**Calls:** None

### GetUsedItem
**Prototype:** `static int16_t GetUsedItem(struct clan *Clan, int16_t type)`
**Description:** Finds first equipped item of specified type, or returns status flag if none equipped or none exist.
**Calls:** None

### GetEquippableItem
**Prototype:** `static int16_t GetEquippableItem(struct clan *Clan, int16_t type)`
**Description:** Finds first unequipped item of specified type that can be equipped, or returns status flag.
**Calls:** None

### GetFirstItem
**Prototype:** `static int16_t GetFirstItem(struct clan *Clan, int16_t type)`
**Description:** Finds first available item of specified type in clan inventory.
**Calls:** None

### ChooseItem
**Prototype:** `int16_t ChooseItem(const char *prompt, struct clan *Clan, int16_t type, int filter)`
**Description:** Prompts user to select an item from clan inventory with filtering (any, used, unused, equippable).
**Calls:** GetFirstItem, GetUsedItem, GetUnusedItem, GetEquippableItem, rputs, GetLong

### UnequipItemsFromPC
**Prototype:** `void UnequipItemsFromPC(int16_t PC)`
**Description:** Unequips all items from a specific player character in PClan.
**Calls:** None

### Items_FindTreasureChest
**Prototype:** `void Items_FindTreasureChest(void)`
**Description:** Randomly selects and offers a treasure item from the items database to player.
**Calls:** Items_Init, my_random, YesNo, Items_GiveItemData, Items_Close, LogDisplayStr

### ReadBook
**Prototype:** `void ReadBook(void)`
**Description:** Allows player to select a book and choose a clan member to read it, granting spells/HP/SP bonuses.
**Calls:** ChooseItem, ShowItemStats, GetKey, ItemPenalty, rputs, toupper, snprintf, strlcpy

### ItemPenalty
**Prototype:** `bool ItemPenalty(struct pc *PC, struct item_data *Item)`
**Description:** Checks if player character has required attributes to use an item.
**Calls:** None

### ItemUseableBy
**Prototype:** `static void ItemUseableBy(struct item_data *Item)`
**Description:** Displays list of clan members who can use an item without penalty.
**Calls:** ItemPenalty, rputs, snprintf, strlcpy

### ShowItemStats
**Prototype:** `void ShowItemStats(struct item_data *Item, struct clan *Clan)`
**Description:** Displays detailed item statistics, modifiers, requirements, and who is using it.
**Calls:** LoadStrings, rputs, snprintf, strlcpy, strlcat, ItemUseableBy

### Items_Read
**Prototype:** `static void Items_Read(void)`
**Description:** Loads all items from data files into memory.
**Calls:** MyOpen, CheckMem, s_item_data_d, malloc, fread, fclose, SWAP16S

### Items_Destroy
**Prototype:** `static void Items_Destroy(void)`
**Description:** Frees all loaded item data from memory.
**Calls:** free

### Items_GiveItemData
**Prototype:** `static void Items_GiveItemData(struct item_data *Item)`
**Description:** Adds an item to player clan's inventory if there's room.
**Calls:** GetOpenItemSlot, rputs

### Items_GiveItem
**Prototype:** `void Items_GiveItem(char *szItemName)`
**Description:** Locates and gives a named item to player clan from loaded items database.
**Calls:** Items_Init, strcasecmp, Items_Close, rputs

### Items_Init
**Prototype:** `static void Items_Init(void)`
**Description:** Loads item data into memory if not already loaded.
**Calls:** Items_Read

### Items_Close
**Prototype:** `void Items_Close(void)`
**Description:** Unloads items from memory.
**Calls:** Items_Destroy

### Items_ReadScroll
**Prototype:** `void Items_ReadScroll(struct pc *PC, struct clan *TargetClan, int16_t Target, int16_t ScrollNum)`
**Description:** Uses a scroll, casts its spell on target, reduces charges, and removes if depleted.
**Calls:** snprintf, rputs, Spells_CastSpell

### Item_BuyItem
**Prototype:** `void Item_BuyItem(signed char ItemType)`
**Description:** Market interface allowing player to purchase items, choose materials, and apply magical/quality enhancements.
**Calls:** Items_Init, MyOpen, Items_Close, rputs, snprintf, GetAnswer, Help, YesNo, NoYes, GetOpenItemSlot, my_random, strlcpy, strlcat, LoadStrings, ShowItemStats

---

## myopen.c

### cipher
**Prototype:** `static void cipher(void *dest, void *src, size_t len, unsigned char xor_val)`
**Description:** XOR cipher implementation that applies XOR key to bytes from source to destination.
**Calls:** System_Error

### MyOpen
**Prototype:** `void MyOpen(char *szFileName, char *szMode, struct FileHeader *FileHeader)`
**Description:** Opens a file from either clans.pak archive or filesystem; supports alternate PAK files; returns FileHeader with file pointer and metadata.
**Calls:** strlcpy, fopen, fread, s_FileHeader_d, strcasecmp, fseek, fclose, ftell, System_Error

### EncryptWrite
**Prototype:** `bool EncryptWrite(void *Data, size_t DataSize, FILE *fp, char XorValue)`
**Description:** Encrypts data using XOR cipher and writes to file, then decrypts buffer back to original.
**Calls:** System_Error, cipher, fwrite

### EncryptRead
**Prototype:** `bool EncryptRead(void *Data, size_t DataSize, FILE *fp, char XorValue)`
**Description:** Reads encrypted data from file and decrypts using XOR cipher.
**Calls:** System_Error, fread, cipher

---

## u8cp437.c

### u8_to_cp437
**Prototype:** `int u8_to_cp437(unsigned char *buf, size_t len, const char *filename, int lineno)`
**Description:** Converts a UTF-8 encoded buffer to CP437 in place, replacing multi-byte sequences with single-byte CP437 codes; returns 0 on success, -1 on invalid UTF-8 or unmappable character.
**Calls:** None

---

### u8_fgets
**Prototype:** `char *u8_fgets(char *buf, int size, FILE *fp, int is_utf8, const char *filename, int *lineno)`
**Description:** Reads a line from a file, optionally converting UTF-8 to CP437 if the file is detected as UTF-8 encoded; returns NULL on EOF or conversion error.
**Calls:** `u8_to_cp437`

---

### u8_fopen
**Prototype:** `FILE *u8_fopen(const char *filename, const char *mode, int *is_utf8)`
**Description:** Opens a file and detects whether it contains UTF-8 by checking for a .u8. infix in the filename before the extension; prints notification if UTF-8 detected.
**Calls:** `FileName`

---

## unix_wrappers.c

### _fsopen
**Prototype:** `FILE *_fsopen(const char *pathname, const char *mode, int shflag)`
**Description:** Opens a file and applies platform-specific Unix file locking with fcntl(F_SETLKW) to enforce requested share flags (_SH_DENYRW, _SH_DENYWR).
**Calls:** None

---

### delay
**Prototype:** `void delay(unsigned msec)`
**Description:** Sleeps for the specified number of milliseconds using nanosleep, handling EINTR interruptions.
**Calls:** None

---

### FileName
**Prototype:** `const char *FileName(const char *path)`
**Description:** Returns a pointer to the filename component of a path, extracting text after the last forward slash.
**Calls:** None

---

### FreeFileList
**Prototype:** `void FreeFileList(char **fl)`
**Description:** Frees a dynamically allocated array of file path strings and the array itself, assuming NULL-terminated list.
**Calls:** None

---

### FilesOrderedByDate
**Prototype:** `char **FilesOrderedByDate(const char *path, const char *match, bool *error)`
**Description:** Returns a dynamically allocated NULL-terminated array of file paths matching a glob pattern, sorted by modification time (oldest first), with case-insensitive matching on Unix; sets error flag on allocation failure.
**Calls:** `FreeFileList`

---

### fullpath
**Prototype:** `char *fullpath(char *target, const char *path, size_t size)`
**Description:** Converts a relative or absolute path to a canonical absolute path, resolving . and .. components and normalizing double slashes; allocates buffer if target is NULL.
**Calls:** None

---

## win_wrappers.c

### display_win32_error
**Prototype:** `void display_win32_error(void)`
**Description:** Retrieves the last Windows system error message and displays it in a modal message box dialog.
**Calls:** None

---

### FileName
**Prototype:** `const char *FileName(const char *path)`
**Description:** Returns a pointer to the filename component of a path on Windows, handling both forward and back slashes and drive letters (C:filename).
**Calls:** None

---

### FreeFileList
**Prototype:** `void FreeFileList(char **fl)`
**Description:** Frees a dynamically allocated array of file path strings and the array itself, assuming NULL-terminated list.
**Calls:** None

---

### FilesOrderedByDate
**Prototype:** `char **FilesOrderedByDate(const char *path, const char *match, bool *error)`
**Description:** Returns a dynamically allocated NULL-terminated array of file paths matching a pattern on Windows using _findfirst/_findnext, sorted by modification time; sets error flag on failure.
**Calls:** None

---

## readcfg.c

### AddInboundDir
**Prototype:** `void AddInboundDir(const char *dir)`
**Description:** Dynamically adds a new inbound directory to the Config.szInboundDirs array, appending a trailing slash if missing.
**Calls:** `System_Error`, `CheckMem`

---

### Config_Init
**Prototype:** `bool Config_Init(uint16_t Node, struct NodeData *(*getNodeData)(int))`
**Description:** Loads configuration from clans.cfg file line-by-line, parsing keywords and storing values in the global Config struct; returns false if file not found, true on successful initialization.
**Calls:** `strlcpy`, `ParseLine`, `GetToken`, `ato16`, `ato8`, `atou8`, `atoull`, `AddInboundDir`, `System_Error`

---

### Config_Close
**Prototype:** `void Config_Close(void)`
**Description:** Frees dynamically allocated memory in the Config struct, particularly the inbound directory list.
**Calls:** None

---

## input.c

### Input_SetGetStringChoiceHook
**Prototype:** `void Input_SetGetStringChoiceHook(int16_t (*hook)(const char **apszChoices, int16_t NumChoices, bool AllowBlank))`
**Description:** Registers a hook function to intercept and handle string choice selection, allowing test or custom implementations to override the default choice logic.
**Calls:** None

---

### Similar
**Prototype:** `static int16_t Similar(const char *string, const char *word)`
**Description:** Returns true if the first N characters of string match the first N characters of word (case-insensitive), where N is the length of word.
**Calls:** None

---

### InsideStr
**Prototype:** `static int16_t InsideStr(const char *SubString, const char *FullString, int16_t AtStart)`
**Description:** Returns true if SubString appears within FullString; if AtStart is true, only checks the start of FullString.
**Calls:** `Similar`

---

### ListChoices
**Prototype:** `static void ListChoices(const char **apszChoices, int16_t NumChoices, int16_t DisplayType)`
**Description:** Displays a list of choices in either wide (3-column) or long (1-column) format with headers and footers.
**Calls:** `rputs`

---

### GetStringChoice
**Prototype:** `void GetStringChoice(const char **apszChoices, int16_t NumChoices, char *szPrompt, int16_t *UserChoice, bool ShowChoicesInitially, int16_t DisplayType, bool AllowBlank)`
**Description:** Allows the user to select one string from an array of choices using interactive keyboard input with auto-completion and help support; returns selected index in UserChoice or -1 if blank and allowed.
**Calls:** `get_string_choice_hook`, `ListChoices`, `InputCallback`, `GetKey`, `PutCh`

---

### GetStr
**Prototype:** `void GetStr(char *InputStr, int16_t MaxChars, bool HiBit)`
**Description:** Reads a string from the user up to MaxChars length with backspace and ctrl-Y clear support; optionally restricts high-bit ASCII characters.
**Calls:** `System_Error`, `rputs`, `rawputs`, `InputCallback`, `GetKey`, `PutCh`

---

### GetChoice
**Prototype:** `char GetChoice(char *DisplayFile, char *Prompt, char *Options[], char *Keys, char DefChar, bool ShowTime)`
**Description:** Displays a file/help topic, prompt, and options array, then accepts a single character choice from a predefined key list; returns the key corresponding to the user's selection.
**Calls:** `GetHoursLeft`, `GetMinutesLeft`, `Display`, `Help`, `rputs`, `GetAnswer`, `strlcpy`, `strlcat`

---

### GetLong
**Prototype:** `long GetLong(const char *Prompt, long DefaultVal, long Maximum)`
**Description:** Prompts the user to input a long integer with validation against a maximum value; supports < and > for minimum and maximum shortcuts.
**Calls:** `rputs`, `GetAnswer`, `PutCh`, `strlcpy`, `atol`

---

## help.c

### Help
**Prototype:** `void Help(const char *Topic, char *File)`
**Description:** Displays help text for a specific topic from an encrypted help file, with line-based display and optional screen pause support.
**Calls:** `MyOpen`, `rputs`, `ftell`, `fgets`, `fclose`, `strcasecmp`, `GetKeyNoWait`, `door_pause`, `Door_AllowScreenPause`, `GetKey`, `InputCallback`, `toupper`

---

### GeneralHelp
**Prototype:** `void GeneralHelp(char *pszFileName)`
**Description:** Displays a menu of all help topics from a file, allowing the user to select and read topics interactively; supports dynamic topic loading from file headers.
**Calls:** `MyOpen`, `rputs`, `ftell`, `fgets`, `fclose`, `strcasecmp`, `DupeStr`, `GetStringChoice`, `Help`

---

### MainHelp
**Prototype:** `void MainHelp(void)`
**Description:** Displays the main help system from the pre-game menu with 17 predefined help topics covering gameplay aspects like attributes, combat, empire, items, and spells.
**Calls:** `GetChoice`, `GeneralHelp`

---

## news.c

### News_AddNews
**Prototype:** `void News_AddNews(char *szString)`
**Description:** Appends a news item to the today.asc news file.
**Calls:** `_fsopen`, `fputs`, `fclose`

---

### News_ReadNews
**Prototype:** `void News_ReadNews(bool Today)`
**Description:** Displays news from today.asc or yest.asc file with screen-by-screen paging and user pause prompts.
**Calls:** `_fsopen`, `fgets`, `feof`, `fclose`, `rputs`, `Door_AllowScreenPause`, `InputCallback`, `GetKey`, `GetKeyNoWait`, `door_pause`

---

### News_CreateTodayNews
**Prototype:** `void News_CreateTodayNews(void)`
**Description:** Initializes the today.asc news file with a header containing today's date.
**Calls:** `snprintf`, `News_AddNews`

---

## maint.c

### Maintenance
**Prototype:** `void Maintenance(void)`
**Description:** Performs daily maintenance tasks including date updates, game reset checks, and calls to subsystem maintenance routines; prevents duplicate runs on IBBS games.
**Calls:** `LogDisplayStr`, `DaysBetween`, `System_Error`, `strlcpy`, `System_Maint`, `Village_Maint`, `IBBS_Maint`, `User_Maint`, `Mail_Maint`, `NPC_Maint`, `Trades_Maint`, `PS_Maint`, `unlink`, `Alliance_Maint`, `IBBS_SendQueuedResults`, `IBBS_HandleQueuedPackets`, `Game_Write`

---

## event.c

### GoQuest
**Prototype:** `static void GoQuest(void)`
**Description:** Displays available quests, prompts player to select and begin one, and processes quest completion by setting quest completion bits.
**Calls:** `snprintf`, `rputs`, `toupper`, `Help`, `YesNo`, `RunEvent`, `door_pause`

---

## class.c

### Load_PClasses
**Prototype:** `static void Load_PClasses(struct PClass *PClass[MAX_PCLASSES], bool GetPClasses)`
**Description:** Loads player classes or races from binary files into a dynamically allocated array, swapping byte order and deserializing each record.
**Calls:** `MyOpen`, `fread`, `SWAP16S`, `malloc`, `CheckMem`, `s_PClass_d`, `fclose`

---

### Free_PClasses
**Prototype:** `static void Free_PClasses(struct PClass *PClass[MAX_PCLASSES])`
**Description:** Frees all dynamically allocated class/race structures from an array.
**Calls:** None

---

### PClass_Init
**Prototype:** `void PClass_Init(void)`
**Description:** Initializes the class and race systems by loading all classes and races from configured files.
**Calls:** `LogDisplayStr`, `delay`, `Load_PClasses`

---

### PClass_Close
**Prototype:** `void PClass_Close(void)`
**Description:** Deinitializes the class and race systems by freeing all loaded structures.
**Calls:** `Free_PClasses`

---

## game.c

### Game_Read
**Prototype:** `static bool Game_Read(void)`
**Description:** Loads the current game state from game.dat file with XOR decryption; returns false if file not found.
**Calls:** `fopen`, `EncryptRead_s`, `fclose`

---

### Game_Write
**Prototype:** `void Game_Write(void)`
**Description:** Serializes and writes the game state to game.dat file with XOR encryption.
**Calls:** `fopen`, `s_game_data_s`, `EncryptWrite`, `fclose`

---

### Game_Destroy
**Prototype:** `static void Game_Destroy(void)`
**Description:** Deinitializes the Game struct by setting the Initialized flag to false.
**Calls:** None

---

### Game_Settings
**Prototype:** `void Game_Settings(void)`
**Description:** Displays current game configuration settings including start date, member limits, protection period, and fight counts.
**Calls:** `snprintf`, `rputs`, `door_pause`

---

### Game_Start
**Prototype:** `void Game_Start(void)`
**Description:** Initializes the game state when the configured start date is reached, recording the start in news and writing game data.
**Calls:** `News_AddNews`, `Game_Write`

---

### Game_Init
**Prototype:** `void Game_Init(void)`
**Description:** Loads game.dat on startup with validation; handles pre-game countdown or starts game if conditions are met.
**Calls:** `LogDisplayStr`, `delay`, `Game_Read`, `Game_Destroy`, `System_Error`, `DaysBetween`, `Game_Start`

---

### Game_Close
**Prototype:** `void Game_Close(void)`
**Description:** Performs cleanup for the game system, writing final state and setting initialized flag to false.
**Calls:** `Game_Write`, `Game_Destroy`

---

## clans.c

### MainGame
**Prototype:** `static void MainGame(void)`
**Description:** Main game menu loop; displays game state, handles player menu selection (play, stats, news, scores, etc.), and manages user entry/exit.
**Calls:** `LoadStrings`, `rputs`, `DaysBetween`, `GetChoice`, `Disbanded`, `Help`, `User_Init`, `VillageName`, `BBSName`, `IBBS_SendComeBack`, `strcasecmp`, `User_FirstTimeToday`, `News_ReadNews`, `ClanStats`, `Mail_Read`, `Trades_CheckTrades`, `GameLoop`, `User_Close`, `GeneralHelp`, `ShowVillageStats`, `DisplayScores`, `Game_Settings`, `IBBS_LeagueInfo`, `LeagueScores`

---

### main/WinMain
**Prototype:** `int main(int argc, char *argv[])` or `int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nShowCmd)`
**Description:** Program entry point; initializes system, runs maintenance if needed, displays intro screens, and enters main game menu.
**Calls:** `System_Init`, `DaysBetween`, `Maintenance`, `UnregMessage`, `Door_ShowTitle`, `Display`, `IBBS_ShowLeagueAscii`, `MainGame`, `System_Close`

---

### Disbanded
**Prototype:** `static bool Disbanded(void)`
**Description:** Checks if the current user is in disband.dat, indicating they have been disbanded from their clan today.
**Calls:** `fopen`, `EncryptRead`, `memchr`, `System_Error`, `strcasecmp`, `fclose`

---

## clansini.c

### ClansIni_Init
**Prototype:** `void ClansIni_Init(void)`
**Description:** Reads clans.ini file and parses NPC, item, class, race, spell, and language file references into the global IniFile struct with dynamic memory allocation.
**Calls:** `LogDisplayStr`, `delay`, `fopen`, `System_Error`, `fgets`, `ParseLine`, `GetToken`, `strcasecmp`, `DupeStr`, `fclose`

---

### ClansIni_Close
**Prototype:** `void ClansIni_Close(void)`
**Description:** Deinitializes the clans.ini system by freeing all dynamically allocated strings and setting initialized flag to false.
**Calls:** `free`

---

## gum.c

### output_bit
**Prototype:** `static void output_bit(FILE *output, int bit)`
**Description:** Writes a single bit to an output file by buffering 8 bits then writing as a byte.
**Calls:** None

---

### input_bit
**Prototype:** `static int input_bit(FILE *input)`
**Description:** Reads a single bit from an input file by buffering a byte and extracting bits one at a time.
**Calls:** None

---

### output_code
**Prototype:** `static void output_code(FILE *output, int code, int bits)`
**Description:** Writes a multi-bit code to output by calling output_bit for each bit.
**Calls:** `output_bit`

---

### input_code
**Prototype:** `static int input_code(FILE *input, int bits)`
**Description:** Reads a multi-bit code from input by calling input_bit multiple times.
**Calls:** `input_bit`

---

### flush_bits
**Prototype:** `static void flush_bits(FILE *output)`
**Description:** Flushes any remaining buffered bits to the output file before closing.
**Calls:** None

---

### initialize
**Prototype:** `static void initialize(void)`
**Description:** Initializes Huffman tree and copy distance ranges for compression/decompression.
**Calls:** None

---

### update_freq
**Prototype:** `static void update_freq(int a, int b)`
**Description:** Updates frequency counts in the Huffman tree from leaf to root.
**Calls:** None

---

### update_model
**Prototype:** `static void update_model(int code)`
**Description:** Updates the Huffman model for an individual character code.
**Calls:** `update_freq`

---

### compress
**Prototype:** `static void compress(FILE *output, int code)`
**Description:** Compresses a character code to the output stream using Huffman coding.
**Calls:** `output_bit`, `update_model`

---

### uncompress
**Prototype:** `static int uncompress(FILE *input)`
**Description:** Uncompresses a character code from the input stream using Huffman decoding.
**Calls:** `input_bit`, `update_model`

---

### add_node
**Prototype:** `static void add_node(int n)`
**Description:** Adds a position to the head of a hash table linked list for string matching.
**Calls:** None

---

### delete_node
**Prototype:** `static void delete_node(int n)`
**Description:** Deletes a position from the tail of a hash table linked list.
**Calls:** None

---

### match
**Prototype:** `static int match(int n, int depth)`
**Description:** Finds the longest string match in the lookahead buffer at a given position with depth limit.
**Calls:** None

---

### dictionary
**Prototype:** `static void dictionary(void)`
**Description:** Detects if the input buffer represents an ordered dictionary file with repeated line prefixes.
**Calls:** None

---

### encode
**Prototype:** `int encode(FILE *input, FILE *output)`
**Description:** Encodes (compresses) a file from input to output using adaptive Huffman and string matching compression.
**Calls:** `initialize`, `calloc`, `getc`, `compress`, `flush_bits`, `free`

---

### decode
**Prototype:** `void decode(FILE *input, FILE *output, void(*kputs)(const char *))`
**Description:** Decodes (decompresses) a file from input to output using adaptive Huffman decompression.
**Calls:** `initialize`, `malloc`, `uncompress`, `putc`, `free`

---

### ClearAll
**Prototype:** `void ClearAll(void)`
**Description:** Clears all static state and counters used in the compression/decompression system.
**Calls:** None

---

## pawn.c

### PS_Init
**Prototype:** `static void PS_Init(struct item_data **PS_Items)`
**Description:** Loads pawn shop inventory from pawn.dat file into memory.
**Calls:** `fopen`, `malloc`, `CheckMem`, `notEncryptRead_s`, `free`, `fclose`

---

### PS_Close
**Prototype:** `static void PS_Close(struct item_data **PS_Items)`
**Description:** Writes the pawn shop inventory back to pawn.dat and frees allocated memory.
**Calls:** `fopen`, `EncryptWrite_s`, `fclose`, `free`

---

### PS_List
**Prototype:** `static void PS_List(struct item_data *PS_Items[MAX_PSITEMS])`
**Description:** Displays the list of items available for sale at the pawn shop.
**Calls:** `rputs`

---

### PS_Buy
**Prototype:** `static void PS_Buy(struct item_data *PS_Items[MAX_PSITEMS])`
**Description:** Handles purchasing items from the pawn shop; deducts gold and adds item to player inventory.
**Calls:** `LoadStrings`, `GetChoice`, `GetLong`, `PS_List`, `ShowItemStats`, `YesNo`, `my_random`

---

### PS_Sell
**Prototype:** `static void PS_Sell(struct item_data *PS_Items[MAX_PSITEMS])`
**Description:** Handles selling items to the pawn shop; removes item from inventory and credits gold to vault.
**Calls:** `rputs`, `GetChoice`, `ListItems`, `ShowItemStats`, `GetLong`, `YesNo`, `malloc`, `CheckMem`, `free`, `my_random`, `DaysSince1970`

---

### PS_Maint
**Prototype:** `void PS_Maint(void)`
**Description:** Performs pawn shop maintenance, removing items older than MAX_PSITEMAGE days.
**Calls:** `LogDisplayStr`, `PS_Init`, `DaysSince1970`, `free`, `PS_Close`

---

### PawnShop
**Prototype:** `void PawnShop(void)`
**Description:** Main pawn shop menu loop; displays options to buy, sell, view stats, or quit.
**Calls:** `LoadStrings`, `Help`, `rputs`, `PS_Init`, `GetChoice`, `PS_Buy`, `PS_Sell`, `ClanStats`, `PS_Close`

---

## trades.c

### GetTradeList
**Prototype:** `static void GetTradeList(struct TradeList *TradeList, bool GivingList, char *szTitle)`
**Description:** Interactively prompts the user to specify amounts of gold, followers, and troops to offer or request in a trade.
**Calls:** `rputs`, `snprintf`, `GetChoice`, `GetLong`, `ClanStats`

---

### RejectTrade
**Prototype:** `void RejectTrade(struct TradeData *TradeData)`
**Description:** Rejects a trade offer, returning all offered resources to the originating clan and notifying them.
**Calls:** `GetClan`, `GenericMessage`, `Clan_Update`, `FreeClanMembers`

---

### Trades_CheckTrades
**Prototype:** `void Trades_CheckTrades(void)`
**Description:** Processes all active trade offers directed at the current player, allowing acceptance, rejection, or deferral.
**Calls:** `fopen`, `fseek`, `ftell`, `notEncryptRead_s`, `snprintf`, `rputs`, `YesNo`, `RejectTrade`, `EncryptWrite_s`, `NoYes`, `GetClan`, `GenericMessage`, `Clan_Update`, `FreeClanMembers`, `fclose`

---

### Trades_MakeTrade
**Prototype:** `void Trades_MakeTrade(void)`
**Description:** Initiates a new player-to-player trade by prompting for recipient, items/troops offered and requested.
**Calls:** `GetClanID`, `GetTradeList`, `snprintf`, `rputs`, `YesNo`, `fopen`, `EncryptWrite_s`, `fclose`

---

### Trades_Maint
**Prototype:** `void Trades_Maint(void)`
**Description:** Performs trades maintenance by removing inactive trade records from the trades.dat file.
**Calls:** `fopen`, `notEncryptRead_s`, `EncryptWrite_s`, `fclose`, `unlink`, `rename`

---

## reg.c

### Jumble
**Prototype:** `static void Jumble(char *szString, size_t n)`
**Description:** Obfuscates a string by rearranging characters in a deterministic pattern; used for trivial encryption of sensitive values.
**Calls:** None

---

### IsRegged
**Prototype:** `int16_t IsRegged(char *szSysopName, char *szBBSName, char *szRegCode)`
**Description:** Validates a registration code against a sysop/BBS name pair using checksums and XOR encryption; returns NTRUE if valid, NFALSE otherwise.
**Calls:** `Jumble`

---

### Register
**Prototype:** `void Register(void)`
**Description:** Platform-specific registration dialog for non-Unix/non-Windows systems; displays encrypted registration prompts and accepts user input.
**Calls:** `Config_Init`, `Input`

---

### dputs
**Prototype:** `static void dputs(char *string)`
**Description:** Platform-specific display function for encrypted strings on non-Unix/non-Windows systems; decrypts and renders text directly to video memory with color support.
**Calls:** `ScrollUp`

---

### AddRegToCfg
**Prototype:** `static void AddRegToCfg(char *szString)`
**Description:** Platform-specific function that appends a registration code to the clans.cfg file on non-Unix/non-Windows systems.
**Calls:** `fopen`, `fprintf`, `fclose`

---

### UnregMessage
**Prototype:** `void UnregMessage(void)`
**Description:** Displays a registration status message; shows freeware notice if unregistered, or registered-to message if valid registration code provided.
**Calls:** `IsRegged`, `rputs`, `snprintf`

---

## config.c

### MailerTypeName
**Prototype:** `static const char *MailerTypeName(int16_t t)`
**Description:** Returns a human-readable string name for a mailer type constant (MAIL_BINKLEY, MAIL_OTHER, MAIL_NONE).
**Calls:** None

---

### WriteCfg
**Prototype:** `static void WriteCfg(void)`
**Description:** Serializes the global Config struct to clans.cfg file with commented sections for game data, inter-BBS configuration, and node data; terminates on write failure.
**Calls:** `fopen`, `fputs`, `fprintf`, `fclose`, `System_Error`

---

### ConfigMenu
**Prototype:** `static void ConfigMenu(void)`
**Description:** Interactive menu loop for editing main configuration options (sysop name, BBS name, logging, score files, InterBBS settings, node configuration); supports navigation, editing, and save/abort.
**Calls:** `clrscr`, `ShowTextCursor`, `qputs`, `xputs`, `ColorArea`, `gotoxy`, `textattr`, `UpdateOption`, `cio_getch`, `strchr`, `EditOption`, `WriteCfg`, `save_screen`, `restore_screen`

---

### NodeMenu
**Prototype:** `static void NodeMenu(void)`
**Description:** Interactive menu loop for editing node-specific configuration options (dropfile directory, FOSSIL flag, serial port address, IRQ); displays node number in header.
**Calls:** `clrscr`, `snprintf`, `qputs`, `xputs`, `ColorArea`, `gotoxy`, `textattr`, `UpdateNodeOption`, `cio_getch`, `strchr`, `EditNodeOption`

---

### EditOption
**Prototype:** `static void EditOption(int16_t WhichOption)`
**Description:** Edits a single main configuration option by prompting user input or toggling values; modifies the global Config struct directly.
**Calls:** `save_screen`, `textattr`, `gotoxy`, `DosGetStr`, `DosGetLong`, `CheckMem`, `setCurrentNode`, `NodeMenu`, `restore_screen`, `free`

---

### EditNodeOption
**Prototype:** `static void EditNodeOption(int16_t WhichOption)`
**Description:** Edits a single node configuration option by prompting user input; modifies the current node struct directly.
**Calls:** `textattr`, `gotoxy`, `DosGetStr`, `DosGetLong`, `snprintf`, `strlcpy`, `strtoull`

---

### setCurrentNode
**Prototype:** `static struct NodeData *setCurrentNode(int node)`
**Description:** Finds an existing node by number or creates a new one with default values; allocates memory as needed and returns a pointer to the node struct, or NULL for invalid input.
**Calls:** `realloc`, `System_Error`, `snprintf`

---

### UpdateOption
**Prototype:** `static void UpdateOption(int8_t Option)`
**Description:** Refreshes the display of a single main configuration option value on screen at its designated position.
**Calls:** `textattr`, `xputs`, `snprintf`, `MailerTypeName`, `ColorArea`

---

### UpdateNodeOption
**Prototype:** `static void UpdateNodeOption(int8_t Option)`
**Description:** Refreshes the display of a single node configuration option value on screen at its designated position.
**Calls:** `textattr`, `xputs`, `snprintf`, `strlcpy`

---

## fight.c (lines 1–950)

### Fight_Heal
**Prototype:** `void Fight_Heal(struct clan *Clan)`
**Description:** Restores all present clan members to full HP.
**Calls:** None

---

### Fight_GetBattleOrder
**Prototype:** `static void Fight_GetBattleOrder(struct order *BattleOrder, struct clan *Team[2])`
**Description:** Calculates and sorts combat turn order based on agility, health percentage, and randomness; populates BattleOrder array for combat rounds.
**Calls:** `GetStat`, `my_random`, `NumMembers`, `LogDisplayStr`

---

### Fight_ManaRegenerate
**Prototype:** `static void Fight_ManaRegenerate(struct pc *PC)`
**Description:** Increases spell points for a character during combat based on Wisdom attribute and randomness.
**Calls:** `GetStat`, `my_random`

---

### Fight_IsIncapacitated
**Prototype:** `static bool Fight_IsIncapacitated(struct pc *PC)`
**Description:** Checks if a character is affected by an incapacitating spell and displays the status message if so.
**Calls:** `rputs`

---

### Fight_ChooseVictim
**Prototype:** `static int16_t Fight_ChooseVictim(struct clan *EnemyClan)`
**Description:** Selects a target from an enemy clan: either the lowest-HP member (60% chance) or random member (40% chance).
**Calls:** `my_random`, `NumMembers`, `rputs`

---

### NumUndeadMembers
**Prototype:** `static int16_t NumUndeadMembers(struct clan *Clan)`
**Description:** Counts the number of undead members present in a clan.
**Calls:** None

---

### Fight_GetNPCAction
**Prototype:** `static void Fight_GetNPCAction(struct pc *NPC, struct clan *EnemyClan, struct move *Move)`
**Description:** Determines next action for an NPC during combat: heal allies, cast damage/utility spells, or attack based on availability and randomness.
**Calls:** `my_random`, `rputs`, `Fight_ChooseVictim`, `NumUndeadMembers`

---

### Fight_Stats
**Prototype:** `static void Fight_Stats(struct clan *PlayerClan, struct clan *MobClan, struct pc *WhichPC)`
**Description:** Displays current HP and status for all combatants from both teams in color-coded format.
**Calls:** `rputs`, `snprintf`

---

### FirstAvailable
**Prototype:** `static int16_t FirstAvailable(struct clan *Clan)`
**Description:** Returns the index of the first present clan member alive, or -1 if none available.
**Calls:** None

---

### Fight_GetTarget
**Prototype:** `static int16_t Fight_GetTarget(struct clan *MobClan, int16_t Default)`
**Description:** Prompts user to select a target from alive clan members; allows list view with '?', supports letter/number selection, or Enter for default.
**Calls:** `GetKey`, `rputs`, `snprintf`, `FirstAvailable`, `System_Error`, `toupper`, `isdigit`

---

### CanRun
**Prototype:** `static bool CanRun(struct clan *RunningClan, struct clan *StayingClan)`
**Description:** Determines if a clan can successfully flee combat by comparing combined agility, dexterity, and wisdom of both sides with randomness.
**Calls:** `NumMembers`, `GetStat`, `my_random`

---

### Fight_Dead
**Prototype:** `static bool Fight_Dead(struct clan *Clan)`
**Description:** Returns true if a clan has no living non-undead members or total membership is zero.
**Calls:** `NumMembers`, `rputs`

---

### Fight_BattleAttack
**Prototype:** `static void Fight_BattleAttack(struct pc *Attacker, struct clan *VictimClan, int16_t Who, bool SkipOutput)`
**Description:** Performs a melee attack, calculating hit/miss, damage, experience, and gold rewards; updates target HP and status (unconscious/dead); frees undead members on death.
**Calls:** `GetStat`, `my_random`, `rputs`, `snprintf`, `FirstAvailable`

---

### Fight_DoMove
**Prototype:** `static bool Fight_DoMove(struct pc *AttackerPC, struct move Move, struct clan *Defender, bool FightToDeath, int16_t CurRound)`
**Description:** Executes a combat action (cast spell, read scroll, attack, run, skip); returns true if attacker chose to run away and can escape.
**Calls:** `Spells_CastSpell`, `Items_ReadScroll`, `CanRun`, `rputs`

---

### GetTarget2
**Prototype:** `static int16_t GetTarget2(struct clan *Clan)`
**Description:** Displays alive clan members with health info and prompts user to select a target; supports list view with '?', letter/number selection, or Q to cancel.
**Calls:** `snprintf`, `rputs`, `toupper`, `GetKey`, `isdigit`

---

### Fight_ReadyScroll
**Prototype:** `static bool Fight_ReadyScroll(struct pc *PC, struct clan *TargetClan, struct move *Move)`
**Description:** Allows player to select and validate a scroll for use in combat; returns false if selection invalid or character cannot use item.
**Calls:** `ListItems`, `ChooseItem`, `ItemPenalty`, `rputs`

---

## mail.c

### GenericMessage
**Prototype:** `void GenericMessage(char *szString, int16_t ToClanID[2], int16_t FromClanID[2], char *szFrom, bool AllowReply)`
**Description:** Sends an automated message to a clan from a system/NPC source with optional reply permission.
**Calls:** `ClanExists`, `GenericReply`

---

### MyWriteMessage2
**Prototype:** `void MyWriteMessage2(int16_t ClanID[2], bool ToAll, bool AllyReq, int16_t AllianceID, char *szAllyName, bool GlobalPost, int16_t WhichVillage)`
**Description:** Allows user to compose and send a message (private, public, alliance, or global inter-BBS) with editor, message history, and multi-line text input.
**Calls:** `GetClanID`, `rputs`, `InputStr`, `malloc`, `free`, `fopen`, `EncryptWrite_s`, `fclose`, `CheckedEncryptWrite`, `SendMsj`

---

### Mail_Read
**Prototype:** `bool Mail_Read(void)`
**Description:** Retrieves and displays all unread messages for the current clan from the message file, allowing reading, replying, quoting, and deleting; returns true if any messages were found.
**Calls:** `fopen`, `fseek`, `ftell`, `fclose`, `notEncryptRead_s`, `EncryptRead`, `malloc`, `free`, `snprintf`, `rputs`, `InputCallback`, `GetKey`, `toupper`, `YesNo`, `NoYes`, `FormAlliance`, `GenericReply`, `GetAnswer`, `EncryptWrite_s`

---

### Mail_Write
**Prototype:** `void Mail_Write(int16_t MessageType)`
**Description:** Initiates message composition for the specified message type (private, public, alliance).
**Calls:** `Msg_Create`

---

### Mail_Maint
**Prototype:** `void Mail_Maint(void)`
**Description:** Performs mail maintenance by removing deleted messages from the message file, compacting it into a temporary file and replacing the original.
**Calls:** `fopen`, `fclose`, `notEncryptRead_s`, `fseek`, `malloc`, `free`, `EncryptRead`, `EncryptWrite_s`, `unlink`, `rename`, `LogDisplayStr`

---

### Mail_RequestAlliance
**Prototype:** `void Mail_RequestAlliance(struct Alliance *Alliance)`
**Description:** Initiates an alliance membership request by prompting for target clan and sending them an invitation message.
**Calls:** `GetClanID`, `NoYes`, `GetClan`, `FreeClanMembers`, `MyWriteMessage2`, `rputs`

---

### Mail_WriteToAllies
**Prototype:** `void Mail_WriteToAllies(struct Alliance *Alliance)`
**Description:** Allows clan leader to compose and send a message to all alliance members.
**Calls:** `MyWriteMessage2`

---

### PostMsj
**Prototype:** `void PostMsj(struct Message *Message)`
**Description:** Directly appends a message to the message file, optionally updating the public message index for public posts.
**Calls:** `fopen`, `fclose`, `EncryptWrite_s`, `CheckedEncryptWrite`

---

### GlobalMsgPost
**Prototype:** `void GlobalMsgPost(void)`
**Description:** Allows inter-BBS message posting: prompts for post type (public/private), target village, and recipient clan, then composes the message for global network distribution.
**Calls:** `GetStringChoice`, `YesNo`, `GetUserNames`, `malloc`, `free`, `MyWriteMessage2`, `VillageName`, `rputs`

---

## alliance.c

### Alliances_Init
**Prototype:** `void Alliances_Init(void)`
**Description:** Initializes the alliances array from the ally.dat file, allocating memory and reading encrypted alliance structures into the global Alliances array.
**Calls:** `fopen`, `malloc`, `CheckMem`, `notEncryptRead_s`, `free`, `fclose`

---

### Alliances_Close
**Prototype:** `void Alliances_Close(void)`
**Description:** Saves all alliances to ally.dat file and frees allocated memory, clearing the alliances array and resetting initialization flag.
**Calls:** `UpdateAlliances`, `FreeAlliances`

---

### DeleteAlliance
**Prototype:** `void DeleteAlliance(int Index)`
**Description:** Removes an alliance from the array by index, packing remaining elements and decrementing the alliance count; calls System_Error if alliances not initialized.
**Calls:** `System_Error`, `free`, `memmove`

---

### KillAlliances
**Prototype:** `void KillAlliances(void)`
**Description:** Destroys all alliances by deleting their associated hall files and the ally.dat file, used during game reset.
**Calls:** `Alliances_Init`, `snprintf`, `unlink`, `FreeAlliances`

---

## alliancem.c

### RemoveFromAlliance
**Prototype:** `static void RemoveFromAlliance(struct Alliance *Alliance)`
**Description:** Removes the current player clan from an alliance, updating both the alliance member list and the clan's alliance array, and sends notification to alliance members.
**Calls:** `GetClanID`, `GetClanNameID`, `rputs`, `NoYes`, `GetClan`, `Clan_Update`, `FreeClanMembers`, `snprintf`, `GenericMessage`

---

### SeeMemberStats
**Prototype:** `static void SeeMemberStats(struct Alliance *Alliance)`
**Description:** Displays statistics for a selected alliance member clan by retrieving and displaying clan data.
**Calls:** `GetClanID`, `GetClan`, `ClanStats`, `FreeClanMembers`, `rputs`

---

### ShowAllianceItems
**Prototype:** `static void ShowAllianceItems(struct Alliance *Alliance)`
**Description:** Displays all available items in the alliance donation room in a two-column formatted list.
**Calls:** `rputs`, `snprintf`, `strlen`

---

### DonationRoom
**Prototype:** `static void DonationRoom(struct Alliance *Alliance)`
**Description:** Implements an interactive menu for alliance members to list, examine, donate, take, and destroy items in the alliance donation room.
**Calls:** `rputs`, `GetAnswer`, `GetLong`, `GetClanID`, `Help`, `ShowItemStats`, `ListItems`, `ChooseItem`, `snprintf`, `NoYes`, `GetOpenItemSlot`, `door_pause`

---

### CreateAlliance
**Prototype:** `void CreateAlliance(struct Alliance *Alliance)`
**Description:** Creates a new alliance with the current player as creator, initializing the alliance structure, empire, members, items, and adding it to the player's alliance list.
**Calls:** `snprintf`, `rputs`, `GetStr`, `RemovePipes`, `strlcpy`, `strcasecmp`, `Empire_Create`, `strlcpy`

---

### ShowAlliances
**Prototype:** `void ShowAlliances(struct clan *Clan)`
**Description:** Displays a formatted list of all alliances the given clan belongs to.
**Calls:** `rputs`, `snprintf`, `door_pause`

---

### EnterAlliance
**Prototype:** `bool EnterAlliance(struct Alliance *Alliance)`
**Description:** Implements the alliance management menu allowing members to view stats, invite clans, remove members, chat, manage empire, and destroy alliances; returns true if alliance destroyed, false if exiting normally.
**Calls:** `LoadStrings`, `snprintf`, `rputs`, `GetChoice`, `SeeMemberStats`, `NoYes`, `GetClanNameID`, `Mail_RequestAlliance`, `RemoveFromAlliance`, `ClanStats`, `DonationRoom`, `Menus_ChatRoom`, `Empire_Manage`, `ClanExists`, `door_pause`, `GenericMessage`

---

### FormAlliance
**Prototype:** `void FormAlliance(int16_t AllyID)`
**Description:** Adds the current player's clan to an existing alliance if there is available space in both the clan's alliance list and the alliance's member list.
**Calls:** `rputs`

---

### KillAlliance
**Prototype:** `void KillAlliance(int16_t WhichAlliance)`
**Description:** Destroys an alliance by removing all clan references, deleting the alliance chat file, and updating all player files to remove alliance linkage.
**Calls:** `snprintf`, `unlink`, `fopen`, `rputs`, `fseek`, `notEncryptRead_s`, `EncryptWrite_s`, `fclose`, `DeleteAlliance`

---

### Alliance_Maint
**Prototype:** `void Alliance_Maint(void)`
**Description:** Performs regular maintenance on all active alliances, updating empires and correcting corrupted alliance names.
**Calls:** `Empire_Maint`

---

## voting.c

### GetVotes
**Prototype:** `static int16_t GetVotes(int16_t TopCandidates[50][2], int16_t TopVotes[50], bool UserOnline)`
**Description:** Tallies and sorts all votes from player clans for ruler candidates, returning the count of undecided voters; skips the online user if requested.
**Calls:** `strlcpy`, `fopen`, `fseek`, `notEncryptRead_s`, `fclose`

---

### VotingBooth
**Prototype:** `void VotingBooth(void)`
**Description:** Displays an interactive voting booth interface allowing the player to view voting results and change their ruler vote.
**Calls:** `GetVotes`, `rputs`, `Help`, `LoadStrings`, `GetClanNameID`, `snprintf`, `GetChoice`, `ClanStats`, `GetClanID`

---

### ChooseNewLeader
**Prototype:** `void ChooseNewLeader(void)`
**Description:** Selects the new village ruler based on voting results, using charisma comparison to break ties and random selection as final tiebreaker; updates village data and news.
**Calls:** `GetVotes`, `GetClan`, `GetStat`, `free`, `my_random`, `GetClanNameID`, `snprintf`, `News_AddNews`

---

## menus.c

### WorldMenu
**Prototype:** `static int16_t WorldMenu(void)`
**Description:** Displays world travel menu and handles player navigation between villages, with newbie help and travel restrictions.
**Calls:** `LoadStrings`, `Help`, `rputs`, `GetChoice`, `IBBS_SeeVillages`, `ClanStats`, `GeneralHelp`, `IBBS_CurrentTravelInfo`

---

### AddChatFile
**Prototype:** `static void AddChatFile(char *szString, char *pszFileName)`
**Description:** Appends a string to a chat file, rolling off oldest lines if limit exceeded, maintaining a 42-line circular buffer.
**Calls:** `fopen`, `fputs`, `fclose`, `fgets`, `strlcpy`

---

### Menus_ChatRoom
**Prototype:** `void Menus_ChatRoom(char *pszFileName)`
**Description:** Displays a chat room file and allows user to add up to three lines of comments with clan name prefixed.
**Calls:** `od_clr_scr`, `Display`, `NoYes`, `rputs`, `GetStr`, `strlcpy`, `strlcat`, `AddChatFile`

---

### MainMenu
**Prototype:** `static int16_t MainMenu(void)`
**Description:** Displays the main menu with 20 options and handles high-level game navigation; shows banner variations and handles clan deletion.
**Calls:** `my_random`, `LoadStrings`, `snprintf`, `rputs`, `Help`, `GetChoice`, `NoYes`, `DeleteClan`, `strlcpy`, `LeagueKillUser`, `User_Destroy`, `System_Close`, `ClanStats`, `ChatVillagers`, `GeneralHelp`, `RunEvent`

---

### MineMenu
**Prototype:** `static int16_t MineMenu(void)`
**Description:** Displays mine menu with options to fight monsters, clans, quests, and manage mine level; handles fight availability checks.
**Calls:** `LoadStrings`, `Help`, `rputs`, `GetChoice`, `DisplayScores`, `Quests_GoQuest`, `NumMembers`, `my_random`, `Items_FindTreasureChest`, `RunEvent`, `door_pause`, `Fight_Monster`, `User_Write`, `Fight_Clan`, `GetLong`, `ClanStats`, `ChatVillagers`

---

### CommunicationsMenu
**Prototype:** `static int16_t CommunicationsMenu(void)`
**Description:** Displays mail and communications menu allowing private/public mail, IBBS global messages, and lastread pointer management.
**Calls:** `LoadStrings`, `Help`, `rputs`, `GetChoice`, `Mail_Read`, `Mail_Write`, `snprintf`, `GetLong`, `GlobalMsgPost`, `ClanStats`

---

### WizardShop
**Prototype:** `static void WizardShop(void)`
**Description:** Displays wizard shop menu for purchasing scrolls/books, examining items with cost display and help file lookup.
**Calls:** `LoadStrings`, `Help`, `rputs`, `GetChoice`, `ListItems`, `GetLong`, `ShowItemStats`, `snprintf`, `YesNo`, `door_pause`, `ClanStats`

---

### MarketMenu
**Prototype:** `static int16_t MarketMenu(void)`
**Description:** Displays market menu with weapon, armor, shield, and wizard shop options; handles trade timing restrictions.
**Calls:** `LoadStrings`, `rputs`, `GetChoice`, `Item_BuyItem`, `Trades_MakeTrade`, `PawnShop`, `ClanStats`, `ChatVillagers`

---

### AlliancesMenu
**Prototype:** `static int16_t AlliancesMenu(void)`
**Description:** Displays alliance list and allows joining existing or creating new alliances; handles alliance creation limits and message sending.
**Calls:** `Help`, `rputs`, `LoadStrings`, `snprintf`, `System_Error`, `GetAnswer`, `NoYes`, `malloc`, `CheckMem`, `CreateAlliance`, `EnterAlliance`, `KillAlliance`, `MyWriteMessage2`

---

### ChurchMenu
**Prototype:** `static int16_t ChurchMenu(void)`
**Description:** Displays church menu with mass, prayer, blessing, and resurrection/revival options; tracks daily limits and restores member stats.
**Calls:** `LoadStrings`, `Help`, `rputs`, `GetChoice`, `snprintf`, `RunEvent`, `door_pause`, `ResurrectDead`, `ClanStats`, `ChatVillagers`

---

### THallMenu
**Prototype:** `static int16_t THallMenu(void)`
**Description:** Displays training hall menu for managing member training and roster; requires hall to exist in village.
**Calls:** `LoadStrings`, `Help`, `rputs`, `snprintf`, `GetChoice`, `TrainMember`, `AddMember`, `ReleaseMember`, `ClanStats`, `ChatVillagers`

---

### GameLoop
**Prototype:** `void GameLoop(void)`
**Description:** Main game loop dispatching between different menu screens based on return values; handles inter-BBS world travel availability.
**Calls:** `rputs`, `MainMenu`, `MineMenu`, `MarketMenu`, `CommunicationsMenu`, `WorldMenu`, `OutsiderTownHallMenu`, `TownHallMenu`, `Empire_Manage`, `AlliancesMenu`, `ChurchMenu`, `THallMenu`, `Help`, `Village_NewRuler`, `YesNo`, `snprintf`

---

## menus2.c

### ResurrectDead
**Prototype:** `void ResurrectDead(bool Unconscious)`
**Description:** Lists dead or unconscious party members and handles resurrection/revival with cost calculation and confirmation prompt.
**Calls:** `rputs`, `snprintf`, `GetKey`, `toupper`, `NoYes`

---

### ReleaseMember
**Prototype:** `void ReleaseMember(void)`
**Description:** Displays party roster and prompts selection to permanently release a clan member; prevents release of temporary members.
**Calls:** `rputs`, `snprintf`, `strlcat`, `GetKey`, `toupper`, `NoYes`, `UnequipItemsFromPC`, `free`

---

### AddMember
**Prototype:** `void AddMember(void)`
**Description:** Creates new clan member in first empty slot with newbie or standard generation; checks permanent member limit.
**Calls:** `PC_Create`, `malloc`, `CheckMem`

---

### TrainMember
**Prototype:** `void TrainMember(void)`
**Description:** Interactive menu for selecting and training party members; upgrades attributes, HP, SP with training point costs and village discount scaling.
**Calls:** `rputs`, `snprintf`, `GetKey`, `toupper`, `GetChoice`, `my_random`, `door_pause`, `ShowPlayerStats`, `GeneralHelp`

---

## pcedit.c

### main
**Prototype:** `int main(void)`
**Description:** PC editor entry point; displays player clans in sequence with navigation and deletion options; maintains game data consistency across clan lists.
**Calls:** `InitVillage`, `InitGame`, `notEncryptRead_s`, `DeleteClan`, `UpdateVillage`, `fopen`, `fseek`, `ftell`, `fclose`

---

### DeleteClan
**Prototype:** `static void DeleteClan(int16_t ClanID[2])`
**Description:** Removes a clan from all game files (players, messages, trades, alliances) and updates associated data structures to maintain referential integrity.
**Calls:** `UpdateVillage`, `fopen`, `fclose`, `EncryptRead_s`, `notEncryptRead_s`, `EncryptWrite_s`, `unlink`, `rename`, `malloc`, `free`, `CheckMem`, `Alliances_Init`, `DeleteAlliance`, `Alliances_Close`, `RemoveFromUList`, `RemoveFromIPScores`

---

### InitVillage
**Prototype:** `static void InitVillage(void)`
**Description:** Loads village data from village.dat into the global Village struct.
**Calls:** `fopen`, `fclose`, `EncryptRead_s`, `System_Error`

---

### UpdateVillage
**Prototype:** `static void UpdateVillage(void)`
**Description:** Writes village data from the global Village struct to village.dat file.
**Calls:** `fopen`, `fclose`, `EncryptWrite_s`, `System_Error`

---

### RejectTrade
**Prototype:** `static void RejectTrade(struct TradeData *TradeData)`
**Description:** Marks a trade as inactive and returns offered resources (gold, followers, troops) to the originating clan.
**Calls:** `GetClan`, `UpdateClan`, `FreeClan`

---

### RemoveFromUList
**Prototype:** `static void RemoveFromUList(const int16_t ClanID[2])`
**Description:** Removes a clan from the userlist.dat file by reading all records and writing back all except the matching clan.
**Calls:** `ClanIDInList`, `fopen`, `fclose`, `notEncryptRead_s`, `EncryptWrite_s`, `unlink`, `rename`

---

### ClanIDInList
**Prototype:** `static bool ClanIDInList(const int16_t ClanID[2])`
**Description:** Checks whether a clan ID exists in the userlist.dat file.
**Calls:** `fopen`, `fclose`, `notEncryptRead_s`

---

### RemoveFromIPScores
**Prototype:** `static void RemoveFromIPScores(const int16_t ClanID[2])`
**Description:** Removes a clan from the ipscores.dat file by reading all scores, excluding the matching clan, and rewriting the file.
**Calls:** `fopen`, `fclose`, `malloc`, `calloc`, `free`, `CheckMem`, `EncryptRead`, `notEncryptRead_s`, `EncryptWrite_s`, `System_Error`, `CheckedEncryptWrite`

---

### FreeClan
**Prototype:** `static void FreeClan(struct clan *Clan)`
**Description:** Deallocates memory for all clan member pointers in a clan struct.
**Calls:** None

---

### GetClan
**Prototype:** `static bool GetClan(int16_t ClanID[2], struct clan *TmpClan)`
**Description:** Retrieves a clan's data from clans.pc file; loads clan header and all member records; returns false if clan not found.
**Calls:** `fopen`, `fclose`, `fseek`, `notEncryptRead_s`, `EncryptRead_s`, `malloc`, `CheckMem`

---

### UpdateClan
**Prototype:** `static void UpdateClan(struct clan *Clan)`
**Description:** Updates an existing clan's data in clans.pc file by finding the clan by ID and writing clan header and member records.
**Calls:** `fopen`, `fclose`, `fseek`, `ftell`, `notEncryptRead_s`, `EncryptWrite_s`, `malloc`, `free`, `CheckMem`

---

### InitGame
**Prototype:** `static void InitGame(void)`
**Description:** Loads game configuration data from game.dat into the global Game struct.
**Calls:** `fopen`, `fclose`, `notEncryptRead_s`, `System_Error`

---

### pce_getch
**Prototype:** `static int pce_getch(void)`
**Description:** (Unix only) Reads a single character from stdin using select() to wait for input availability.
**Calls:** None

---

## system.c

### CheckMem
**Prototype:** `void CheckMem(void *Test)`
**Description:** Terminates with a system error if the given pointer is NULL, preventing silent NULL pointer dereferences.
**Calls:** `System_Error`

---

### DupeStr
**Prototype:** `char *DupeStr(const char *str)`
**Description:** Allocates and returns a dynamically allocated copy of a string via strdup, terminating on allocation failure.
**Calls:** `CheckMem`

---

### System_LockedOut
**Prototype:** `static bool System_LockedOut(void)`
**Description:** Checks if the current user is locked out by reading a lockout.txt file and comparing usernames; returns true if found.
**Calls:** `ParseLine`, `strcasecmp`

---

### System_Error
**Prototype:** `noreturn void System_Error(char *szErrorMsg)`
**Description:** Displays an error message to the user and terminates the application cleanly; platform-specific output (Windows MessageBox or BBS terminal).
**Calls:** `LogStr`, `RemovePipes`, `DisplayStr`, `LogDisplayStr`, `delay`, `System_Close`

---

### ShowHelp
**Prototype:** `static void ShowHelp(void)`
**Description:** Displays the help screen for command-line /? and /Help parameters.
**Calls:** `zputs`

---

### ODCmdLineHandler
**Prototype:** `static void ODCmdLineHandler(char *flag, char *val)`
**Description:** Processes command-line options that require an argument (e.g., /Recon, /SendReset); validates and executes the corresponding action.
**Calls:** `ato16`, `snprintf`, `LogDisplayStr`, `IBBS_SendRecon`, `System_Close`, `System_Error`, `IBBS_SendReset`, `zputs`

---

### ODCmdLineFlagHandler
**Prototype:** `static BOOL ODCmdLineFlagHandler(const char *flag)`
**Description:** Processes command-line flags that do not require an argument (e.g., /L, /M, /?, /Users); validates and invokes the corresponding subsystem function or sets configuration flags.
**Calls:** `Maintenance`, `System_Close`, `ShowHelp`, `delay`, `User_List`, `System_Error`, `IBBS_DistributeNDX`, `IBBS_PacketIn`, `LogDisplayStr`, `RemoveSemaphor`, `Register`, `strlcpy`, `atoul`, `atou16`, `snprintf`, `zputs`

---

### System_Maint
**Prototype:** `void System_Maint(void)`
**Description:** Performs daily system maintenance including news rotation and creation of today's news file.
**Calls:** `LogDisplayStr`, `unlink`, `rename`, `News_CreateTodayNews`

---

### System_Close_AtExit
**Prototype:** `void System_Close_AtExit(void)`
**Description:** Closes all subsystems in reverse initialization order; safe to call multiple times (uses recursion detection) and must not call exit().
**Calls:** `DisplayScores`, `Items_Close`, `User_Close`, `Quests_Close`, `PClass_Close`, `Spells_Close`, `Door_Close`, `Alliances_Close`, `Village_Close`, `Game_Close`, `IBBS_Close`, `Config_Close`, `ClansIni_Close`, `Language_Close`, `Video_Close`

---

### System_Close
**Prototype:** `noreturn void System_Close(void)`
**Description:** Closes the system and terminates the application; calls System_Close_AtExit and exits via od_exit or exit() depending on Door initialization state.
**Calls:** `System_Close_AtExit`, `Door_Initialized`, `od_exit`, `exit`, `puts`, `abort`

---

### System_Init
**Prototype:** `void System_Init(void)`
**Description:** Initializes the entire system including subsystems (Game, Village, IBBS, Spells, Quests), configuration loading, command-line parsing, and semaphore locking; the main entry point for system startup.
**Calls:** `atexit`, `GetModuleFileName`, `strlcpy`, `fullpath`, `free`, `Video_Init`, `DisplayStr`, `ClansIni_Init`, `Language_Init`, `od_control` setup, `od_parse_cmd_line`, `delay`, `Config_Init`, `CreateSemaphor`, `Door_Init`, `WaitSemaphor`, `Game_Init`, `Village_Init`, `Alliances_Init`, `IBBS_Init`, `IBBS_UpdateRecon`, `IBBS_UpdateReset`, `System_LockedOut`, `rputs`, `System_Close`, `Spells_Init`, `PClass_Init`, `Quests_Init`, `IBBS_PacketIn`

---

## spells.c

### get_spell
**Prototype:** `static char * get_spell(char **dest, FILE *fp)`
**Description:** Reads a length-prefixed string from a binary file with validation; allocates memory and NUL-terminates the result.
**Calls:** `malloc`, `CheckMem`

---

### Spells_Init
**Prototype:** `void Spells_Init(void)`
**Description:** Initializes spell data by loading all configured spell files from the pak archive with byte-order swapping and deserialization.
**Calls:** `LogDisplayStr`, `delay`, `MyOpen`, `EncryptRead_s`, `get_spell`, `s_Spell_d`, `malloc`, `CheckMem`

---

### Spells_Close
**Prototype:** `void Spells_Close(void)`
**Description:** Frees all dynamically allocated spell structures and string data; resets initialized flag.
**Calls:** None

---

### Spells_UpdatePCSpells
**Prototype:** `void Spells_UpdatePCSpells(struct pc *PC)`
**Description:** Updates active spell effects on a player by reducing energy based on strength/wisdom resistances and removing expired spells.
**Calls:** `strlcpy`, `GetStat`, `rputs`

---

### Spells_ClearSpells
**Prototype:** `void Spells_ClearSpells(struct clan *Clan)`
**Description:** Resets all active spell slots for all clan members.
**Calls:** None

---

### Spells_CastSpell
**Prototype:** `void Spells_CastSpell(struct pc *PC, struct clan *EnemyClan, int16_t Target, int16_t SpellNum)`
**Description:** Executes spell casting logic including damage calculation, status effects, healing, banishing/raising undead, and experience/gold rewards with multi-target support.
**Calls:** `strlcpy`, `GetStat`, `my_random`, `snprintf`, `rputs`, `malloc`, `CheckMem`, `free`

---

## chew.c

### main
**Prototype:** `int main(int argc, char **argv)`
**Description:** Entry point for the chew utility; creates compressed GUM archive from a file list with optional Unix attribute preservation.
**Calls:** `strlcpy`, `fopen`, `fgets`, `GetToken`, `AddDir`, `AddGUM`, `unlink`, `fclose`

---

### AddGUM
**Prototype:** `static void AddGUM(FILE *fpGUM, char *pszFileName)`
**Description:** Compresses and adds a file to a GUM archive with encrypted filename, filesize, datestamp, and adaptive Huffman/LZ77 compression.
**Calls:** `ClearAll`, `strlen`, `strlcpy`, `fwrite`, `fopen`, `fseek`, `ftell`, `malloc`, `_dos_getftime`, `encode`, `fclose`

---

### AddDir
**Prototype:** `static void AddDir(FILE *fpGUM, char *pszDirName)`
**Description:** Adds a directory entry to a GUM archive with XOR-encrypted name.
**Calls:** `fwrite`

---

### _dos_getftime
**Prototype:** `static unsigned _dos_getftime(int handle, uint16_t *datep, uint16_t *timep)`
**Description:** Unix replacement for DOS _dos_getftime; converts file modification time to DOS date/time format using fstat and localtime; returns 0 on success.
**Calls:** `fstat`, `localtime`, `memcpy`

---

## ibbs.c (lines 1–1778)

### IBBS_SendFileInPacket
**Prototype:** `static void IBBS_SendFileInPacket(int16_t DestID, int16_t PacketType, char *szFileName)`
**Description:** Opens a file, reads its entire contents into memory, and sends them in an IBBS packet to a destination BBS.
**Calls:** `fopen`, `fseek`, `ftell`, `fread`, `fclose`, `malloc`, `CheckMem`, `IBBS_SendPacket`, `free`, `System_Error`

---

### IBBS_SendUserList
**Prototype:** `static void IBBS_SendUserList(int16_t DestID)`
**Description:** Verifies destination BBS is active and sends the userlist.dat file to that node as a packet.
**Calls:** `IBBS_SendFileInPacket`

---

### IBBS_DistributeNDX
**Prototype:** `void IBBS_DistributeNDX(void)`
**Description:** Loads world.ndx file and broadcasts it to all active BBSes except the local one as PT_NEWNDX packets.
**Calls:** `LogDisplayStr`, `fopen`, `fseek`, `ftell`, `fread`, `fclose`, `malloc`, `CheckMem`, `free`, `IBBS_SendPacket`, `System_Error`

---

### LeagueKillUser
**Prototype:** `void LeagueKillUser(struct UserInfo *User)`
**Description:** Serializes a UserInfo struct and broadcasts PT_SUBUSER packets to all active BBSes to remove the user from their lists.
**Calls:** `s_UserInfo_s`, `IBBS_SendPacket`

---

### IBBS_ProcessSpy
**Prototype:** `static void IBBS_ProcessSpy(struct SpyAttemptPacket *Spy)`
**Description:** Processes incoming spy attempt; determines success by comparing spy intelligence vs. target security with random roll; sends result packet to originating BBS.
**Calls:** `GetClan`, `strlcpy`, `my_random`, `snprintf`, `GenericMessage`, `FreeClanMembers`, `s_SpyResultPacket_s`, `IBBS_EnqueueOutPacket`

---

### IBBS_ProcessSpyResult
**Prototype:** `static void IBBS_ProcessSpyResult(struct SpyResultPacket *SpyResult)`
**Description:** Processes spy result packet; if successful, formats empire data into message; if failed, sends failure notification.
**Calls:** `snprintf`, `ArmySpeed`, `ArmyOffense`, `ArmyDefense`, `ArmyVitality`, `strlcat`, `GenericMessage`

---

### IBBS_SendQueuedResults
**Prototype:** `void IBBS_SendQueuedResults(void)`
**Description:** Reads pktqout.dat file, deserializes queued packets, and resends them to their destinations; deletes file on completion.
**Calls:** `fopen`, `notEncryptRead_s`, `EncryptRead`, `malloc`, `CheckMem`, `IBBS_SendPacket`, `free`, `fclose`, `unlink`, `System_Error`, `LogStr`

---

### EnqueueInPacket
**Prototype:** `static void EnqueueInPacket(struct Packet *Packet, FILE *fp)`
**Description:** Appends a packet and its data to pktqin.dat queue file for later processing.
**Calls:** `fopen`, `malloc`, `CheckMem`, `EncryptWrite_s`, `fread`, `fwrite`, `free`, `fclose`

---

### IBBS_SendDelUser
**Prototype:** `static void IBBS_SendDelUser(int16_t BBSID, struct UserInfo *User)`
**Description:** Serializes a UserInfo struct and sends PT_DELUSER packet to specified active BBS.
**Calls:** `s_UserInfo_s`, `IBBS_SendPacket`

---

### ClanIDInList
**Prototype:** `static bool ClanIDInList(const int16_t ClanID[2])`
**Description:** Searches userlist.dat for a clan matching the given ClanID array; returns true if found.
**Calls:** `fopen`, `notEncryptRead_s`, `fclose`

---

### RemoveFromUList
**Prototype:** `void RemoveFromUList(const int16_t ClanID[2])`
**Description:** Removes a clan from userlist.dat by reading, filtering, and rewriting the file; called by inter-BBS packet handlers.
**Calls:** `ClanIDInList`, `fopen`, `notEncryptRead_s`, `EncryptWrite_s`, `fclose`, `LogStr`, `unlink`, `rename`

---

### AddToUList
**Prototype:** `static void AddToUList(struct UserInfo *User)`
**Description:** Appends a UserInfo struct to userlist.dat if the clan is not already present.
**Calls:** `ClanIDInList`, `fopen`, `EncryptWrite_s`, `fclose`

---

### UpdateNodesOnNewUser
**Prototype:** `static void UpdateNodesOnNewUser(struct UserInfo *User)`
**Description:** Serializes a UserInfo and broadcasts PT_ADDUSER packet to all active BBSes except the local main BBS.
**Calls:** `s_UserInfo_s`, `IBBS_SendPacket`

---

### IBBS_LeagueNewUser
**Prototype:** `void IBBS_LeagueNewUser(struct UserInfo *User)`
**Description:** Adds new user to local userlist; if on League Controller (BBSID=1), broadcasts to all other BBSes; otherwise sends PT_NEWUSER to LC.
**Calls:** `AddToUList`, `s_UserInfo_s`, `IBBS_SendPacket`, `UpdateNodesOnNewUser`

---

### IBBS_InList
**Prototype:** `bool IBBS_InList(char *szName, bool ClanName)`
**Description:** Searches userlist.dat for a matching clan or player name; returns true if found.
**Calls:** `fopen`, `notEncryptRead_s`, `fclose`

---

### IBBS_SendComeBack
**Prototype:** `void IBBS_SendComeBack(int16_t BBSIdTo, struct clan *Clan)`
**Description:** Creates a simple comeback packet containing ClanID and source BBSID and sends it with PT_COMEBACK type.
**Calls:** `IBBS_SendPacket`

---

### VillageName
**Prototype:** `const char *VillageName(int16_t BBSID)`
**Description:** Returns the village name for a given BBSID from IBBS.Data; validates bounds and handles NULL/defunct cases.
**Calls:** None

---

### BBSName
**Prototype:** `const char *BBSName(int16_t BBSID)`
**Description:** Returns the BBS name for a given BBSID from IBBS.Data; validates bounds and handles NULL/defunct cases.
**Calls:** None

---

### ReturnLostAttack
**Prototype:** `static void ReturnLostAttack(struct AttackPacket *AttackPacket)`
**Description:** Returns troops from a failed inter-BBS attack; updates village or clan empire with lost army and sends message to attacker.
**Calls:** `GetClan`, `strlcpy`, `snprintf`, `VillageName`, `Clan_Update`, `FreeClanMembers`, `GenericMessage`

---

### IBBS_BackupMaint
**Prototype:** `static void IBBS_BackupMaint(void)`
**Description:** Reads backup.dat; restores stale packets (>LostDays old): PT_CLANMOVE re-adds clan, PT_ATTACK returns troops; rewrites active packets only.
**Calls:** `fopen`, `notEncryptRead_s`, `DaysBetween`, `EncryptRead_s`, `EncryptRead`, `IBBS_AddToGame`, `ReturnLostAttack`, `fseek`, `malloc`, `CheckMem`, `CheckedEncryptWrite`, `free`, `fclose`, `LogDisplayStr`, `unlink`, `rename`

---

### AbortTrip
**Prototype:** `static void AbortTrip(void)`
**Description:** Marks active trip in leaving.dat as inactive for the current clan; resets WorldStatus to WS_STAYING and DestinationBBS to -1.
**Calls:** `fopen`, `notEncryptRead_s`, `EncryptWrite_s`, `fclose`, `rputs`

---

### IBBS_CurrentTravelInfo
**Prototype:** `void IBBS_CurrentTravelInfo(void)`
**Description:** Displays current travel status if clan is leaving; offers option to abort trip or displays no-trip message.
**Calls:** `snprintf`, `rputs`, `VillageName`, `YesNo`, `AbortTrip`

---

### IBBS_UpdateLeavingClan
**Prototype:** `static void IBBS_UpdateLeavingClan(struct clan *Clan, struct LeavingData LeavingData)`
**Description:** Updates clan status to WS_GONE, modifies armies per LeavingData, updates game news, and persists changes.
**Calls:** `snprintf`, `News_AddNews`, `Clan_Update`

---

### IBBS_TravelMaint
**Prototype:** `static void IBBS_TravelMaint(void)`
**Description:** Reads leaving.dat and processes active inter-BBS travel; sends PT_CLANMOVE packets for travelers arriving at destination BBSes.
**Calls:** `fopen`, `notEncryptRead_s`, `GetClan`, `FreeClanMembers`, `InitClan`, `IBBS_UpdateLeavingClan`, `s_clan_s`, `s_pc_s`, `IBBS_SendPacket`, `unlink`

---

### GetTroopsTraveling
**Prototype:** `static void GetTroopsTraveling(struct LeavingData *LeavingData)`
**Description:** Interactive menu allowing user to select how many troops accompany the traveling clan; updates LeavingData structure.
**Calls:** `rputs`, `snprintf`, `GetAnswer`, `GetLong`

---

### IBBS_TravelToBBS
**Prototype:** `static bool IBBS_TravelToBBS(int16_t DestID)`
**Description:** Initiates inter-BBS travel; checks destination validity, recon data age, existing trips; appends to leaving.dat and sets clan travel status.
**Calls:** `rputs`, `DaysSince1970`, `snprintf`, `YesNo`, `Help`, `GetTroopsTraveling`, `fopen`, `EncryptWrite_s`, `fclose`, `System_Close`, `VillageName`

---

### IBBS_SeeVillages
**Prototype:** `void IBBS_SeeVillages(bool Travel)`
**Description:** Displays list of active villages; allows selection to view help files; optionally initiates travel to chosen village.
**Calls:** `VillageName`, `GetStringChoice`, `rputs`, `Help`, `snprintf`, `NoYes`, `IBBS_TravelToBBS`

---

### IBBS_AddLCLog
**Prototype:** `static void IBBS_AddLCLog(char *szString)`
**Description:** Opens lc.log file in append mode and writes a string to it; used for League Controller logging.
**Calls:** `_fsopen`, `fputs`, `fclose`

---

### IBBS_Destroy
**Prototype:** `static void IBBS_Destroy(void)`
**Description:** Deallocates all dynamically allocated IBBS node name/address strings and marks system as uninitialized.
**Calls:** `free`

---

### IBBS_LoginStats
**Prototype:** `void IBBS_LoginStats(void)`
**Description:** Outputs inter-BBS league information to console including league ID, world name, and game ID.
**Calls:** `snprintf`, `zputs`

---

### IBBS_Create
**Prototype:** `static void IBBS_Create(void)`
**Description:** Initializes all IBBS node tracking structures with zero reset/recon/attack indices for first-time use.
**Calls:** None

---

### IBBS_Read
**Prototype:** `static void IBBS_Read(void)`
**Description:** Reads ibbs.dat file deserializing per-node recon, attack, and reset tracking data; creates defaults if file missing.
**Calls:** `fopen`, `EncryptRead_s`, `fseek`, `notEncryptRead_s`, `fclose`, `IBBS_Create`

---

### IBBS_Write
**Prototype:** `static void IBBS_Write(void)`
**Description:** Writes ibbs.dat file serializing per-node recon, attack, and reset tracking data for all active nodes.
**Calls:** `fopen`, `s_ibbs_node_recon_s`, `EncryptWrite`, `s_ibbs_node_attack_s`, `s_ibbs_node_reset_s`, `fclose`

---

### FindPoint
**Prototype:** `static void FindPoint(int16_t Destination, int16_t Source, int16_t BasePoint)`
**Description:** Recursive path-finding algorithm used to locate routing path in host-based IBBS network topology; sets FoundPoint and StartingPoint flags.
**Calls:** `FindPoint` (recursive)

---

### IBBS_LoadNDX
**Prototype:** `static void IBBS_LoadNDX(void)`
**Description:** Parses world.ndx configuration file loading BBS nodes, village names, addresses, host routing; initializes IBBS network topology; calls FindPoint for host routing.
**Calls:** `fopen`, `fgets`, `ParseLine`, `GetToken`, `atoi`, `malloc`, `CheckMem`, `DupeStr`, `strlcpy`, `strcasecmp`, `fclose`, `IBBS_Destroy`, `System_Error`, `FindPoint`, `free`

---

### IBBS_ProcessRouteConfig
**Prototype:** `static void IBBS_ProcessRouteConfig(void)`
**Description:** Parses route.cfg file to configure per-BBS routing, mail type (normal/crash/hold), and applies to nodes; handles bulk "ALL" directives.
**Calls:** `fopen`, `fgets`, `ParseLine`, `GetToken`, `strcasecmp`, `atoi`, `fclose`, `System_Error`

---

## village.c

### OutsiderTownHallMenu
**Prototype:** `int16_t OutsiderTownHallMenu(void)`
**Description:** Displays town hall menu for non-ruling clans; allows viewing village stats, donating to empire vault, voting, messaging the ruler, accessing public chat, and viewing clan stats.
**Calls:** `LoadStrings`, `VillageName`, `rputs`, `GetChoice`, `Empire_Stats`, `DonateToEmpire`, `VotingBooth`, `GetLong`, `Menus_ChatRoom`, `MyWriteMessage2`, `GeneralHelp`, `ClanStats`, `ChatVillagers`, `snprintf`

---

### ChangeFlagScheme
**Prototype:** `static void ChangeFlagScheme(void)`
**Description:** Allows village ruler to customize the village flag by editing three color values; displays preview and prompts for confirmation before applying changes.
**Calls:** `Help`, `rputs`, `GetAnswer`, `GetLong`, `Door_SetColorScheme`, `NoYes`, `News_AddNews`, `snprintf`

---

### GetNums
**Prototype:** `static void GetNums(int8_t *Array, int16_t NumVars, char *string)`
**Description:** Parses comma/space-separated integer values from a string into an int8_t array.
**Calls:** `ato8`, `isdigit`

---

### LoadSchemes
**Prototype:** `static void LoadSchemes(struct Scheme *Scheme[128])`
**Description:** Loads color schemes from the pak file and optionally a custom schemes.txt file; initializes scheme array pointers to dynamically allocated scheme structures.
**Calls:** `MyOpen`, `ftell`, `fgets`, `LogStr`, `malloc`, `CheckMem`, `GetToken`, `strlcpy`, `GetNums`, `fclose`, `fopen`

---

### AddScheme
**Prototype:** `static void AddScheme(void)`
**Description:** Saves the current village color scheme to schemes.txt for later restoration.
**Calls:** `fopen`, `fprintf`, `fclose`, `rputs`

---

### ChangeColourScheme
**Prototype:** `static void ChangeColourScheme(void)`
**Description:** Provides interactive menu for ruler to change village color scheme; allows loading preset schemes, creating custom schemes, and selecting individual color values.
**Calls:** `Help`, `rputs`, `LoadSchemes`, `GetAnswer`, `strlcpy`, `snprintf`, `malloc`, `CheckMem`, `free`, `strcasecmp`, `isdigit`, `Door_SetColorScheme`, `door_pause`

---

### BuildMenu
**Prototype:** `static void BuildMenu(void)`
**Description:** Menu for village ruler to construct and upgrade buildings (Church, Training Hall, Pawn Shop, Wizard Shop, Smithy); deducts from village vault, adds news entries, and enforces level caps.
**Calls:** `LoadStrings`, `rputs`, `snprintf`, `GetChoice`, `Help`, `YesNo`, `News_AddNews`

---

### EconomicsMenu
**Prototype:** `static void EconomicsMenu(void)`
**Description:** Menu for village ruler to manage taxes (TaxRate, GST, Conscription Rate), withdraw from/deposit to vault; enforces once-per-day limits and generates news announcements.
**Calls:** `LoadStrings`, `rputs`, `snprintf`, `GetChoice`, `Help`, `GetLong`, `News_AddNews`

---

### ToggleEmpireStats
**Prototype:** `static void ToggleEmpireStats(void)`
**Description:** Toggles the ShowEmpireStats flag to control whether non-rulers can view village empire statistics.
**Calls:** `rputs`

---

### TownHallMenu
**Prototype:** `int16_t TownHallMenu(void)`
**Description:** Main menu for village ruler; allows managing economics, structures, color schemes, empire, voting, making announcements, abdicating throne, and accessing help and chat.
**Calls:** `LoadStrings`, `rputs`, `snprintf`, `GetChoice`, `ToggleEmpireStats`, `Help`, `GetLong`, `News_AddNews`, `Empire_Manage`, `VotingBooth`, `EconomicsMenu`, `ClanStats`, `ChangeFlagScheme`, `ChangeColourScheme`, `GetStr`, `GeneralHelp`, `BuildMenu`, `ChatVillagers`

---

### Village_NewRuler
**Prototype:** `void Village_NewRuler(void)`
**Description:** Establishes a new ruler for the village; sets ruling clan ID, resets ruling days, and adds news entry; grants the clan 20 bonus points; prevents re-election until next day.
**Calls:** `rputs`, `strlcpy`, `News_AddNews`, `door_pause`

---

### Village_Destroy
**Prototype:** `static void Village_Destroy(void)`
**Description:** Marks the village as uninitialized (cleanup stub).
**Calls:** None

---

### Village_Read
**Prototype:** `static bool Village_Read(void)`
**Description:** Reads village data from village.dat file with XOR decryption; returns false if file not found or deserialization fails.
**Calls:** `fopen`, `notEncryptRead_s`, `fclose`

---

### Village_Write
**Prototype:** `static void Village_Write(void)`
**Description:** Writes village data to village.dat file with XOR encryption; does nothing if village is not initialized.
**Calls:** `s_village_data_s`, `EncryptWrite`, `fopen`, `fclose`

---

### Village_Reset
**Prototype:** `void Village_Reset(void)`
**Description:** Initializes village data to default state: sets tax rates, conscription rate, initial vault gold, creates default empire with buildings and army, and resets flags.
**Calls:** `ClearFlags`, `Empire_Create`, `strlcpy`, `my_random`

---

### Village_Maint
**Prototype:** `void Village_Maint(void)`
**Description:** Daily maintenance: clears temporary flags, increments ruling days, selects new leader via voting, randomizes market quality/fluctuation, and writes village data to disk.
**Calls:** `LogDisplayStr`, `ClearFlags`, `ChooseNewLeader`, `my_random`, `News_AddNews`, `Empire_Maint`, `Village_Write`, `snprintf`

---

### Village_Init
**Prototype:** `void Village_Init(void)`
**Description:** Initializes village system by reading village.dat, validating CRC, and loading color scheme; fails fatally if data file missing or corrupt.
**Calls:** `LogDisplayStr`, `delay`, `Village_Read`, `Village_Destroy`, `System_Error`, `Door_SetColorScheme`

---

### Village_Close
**Prototype:** `void Village_Close(void)`
**Description:** Closes the village system; writes pending data to disk and marks as uninitialized if currently active.
**Calls:** `Village_Write`, `Village_Destroy`

---

## quests.c

### Quests_Init
**Prototype:** `void Quests_Init(void)`
**Description:** Reads quests.ini file and initializes the global Quests array with quest metadata (name, index, file, Known flag).
**Calls:** `fopen`, `fgets`, `fclose`, `ParseLine`, `GetToken`, `DupeStr`, `LogDisplayStr`, `delay`

---

### Quests_Close
**Prototype:** `void Quests_Close(void)`
**Description:** Frees all allocated memory for quest names, indices, and filenames in the Quests array.
**Calls:** `free`

---

### ClearFlags
**Prototype:** `void ClearFlags(uint8_t *Flags)`
**Description:** Zeros out an 8-byte flags array (64 boolean flags).
**Calls:** None

---

### SetFlag
**Prototype:** `static void SetFlag(uint8_t *Flags, int16_t WhichFlag)`
**Description:** Sets a specific bit in a flags array using bitwise OR.
**Calls:** None

---

### ClearFlag
**Prototype:** `static void ClearFlag(uint8_t *Flags, int16_t WhichFlag)`
**Description:** Clears a specific bit in a flags array using bitwise XOR if the bit is set.
**Calls:** None

---

### FlagSet
**Prototype:** `static bool FlagSet(uint8_t *Flags, int16_t WhichFlag)`
**Description:** Checks if a specific bit is set in a flags array.
**Calls:** None

---

### legal
**Prototype:** `static bool legal(char *pszAcs, int16_t *iCharsRead)`
**Description:** Recursively parses and evaluates ACS (Access Control String) conditions including parentheses, AND/OR operators, quest completion, gold amount, level checks, random chance, and various flag types (T/P/G/D/H/C).
**Calls:** `my_random`, `ato16`, `ato32`, `strlcpy`, `System_Error`

---

### JumpToEvent
**Prototype:** `static void JumpToEvent(const char *szLabel, struct FileHeader *FileHeader)`
**Description:** Seeks to an event with a matching label within an encrypted event file by iterating through event headers.
**Calls:** `fseek`, `ftell`, `fread`, `s_EventHeader_d`, `System_Error`

---

### TellQuest
**Prototype:** `static void TellQuest(char *pszQuestIndex)`
**Description:** Makes a quest known to the player clan by setting the corresponding bit in PClan.QuestsKnown and displaying a message.
**Calls:** `snprintf`, `rputs`

---

### RunEvent
**Prototype:** `bool RunEvent(bool QuoteToggle, bool reset_hp, char *szEventFile, char *szEventName, struct NPCInfo *NPCInfo, char *szNPCIndex)`
**Description:** Executes a compiled quest/event script by reading and processing commands (Text, Option, Fight, Jump, Flag operations, Heal, Gold/XP/Item rewards, NPC join, etc.) with ACS condition evaluation and user input handling.
**Calls:** `MyOpen`, `rputs`, `snprintf`, `ClearFlags`, `fread`, `ftell`, `fseek`, `legal`, `s_EventHeader_d`, `my_random`, `JumpToEvent`, `News_AddNews`, `NPC_ChatNPC`, `SetFlag`, `ClearFlag`, `ato16`, `ato32`, `Items_GiveItem`, `Fight_CheckLevelUp`, `Fight_Fight`, `FreeClanMembers`, `GetAnswer`, `DupeStr`, `GetStringChoice`, `NPC_ResetNPCClan`, `NPC_GetNPCNdx`, `NPC_AddNPCMember`, `NPC_UpdateNPCNdx`, `NPC_GetNPC`, `door_pause`, `fclose`, `Display`, `Help`, `YesNo`, `GetKey`, `toupper`, `malloc`, `CheckMem`, `free`, `strlcpy`

---

### Quests_GoQuest
**Prototype:** `void Quests_GoQuest(void)`
**Description:** Main quest selection and execution menu that displays available quests, gets user choice, shows help text, and runs the selected quest event.
**Calls:** `snprintf`, `rputs`, `Help`, `YesNo`, `GetKey`, `toupper`, `RunEvent`, `door_pause`

---

## myibbs.c

### ConvertStringToAddress
**Prototype:** `void ConvertStringToAddress(tFidoNode *pNode, const char *pszSource)`
**Description:** Converts a Fidonet address string (zone:net/node.point format) into a tFidoNode structure using sscanf parsing.
**Calls:** None

---

### IBSendFileAttach
**Prototype:** `tIBResult IBSendFileAttach(tIBInfo *pInfo, char *pszDestNode, const char *pszFileName)`
**Description:** Creates and sends a Fidonet netmail message with file attachment by constructing kludge lines (INTL, FMPT, TOPT, MSGID, PID) and writing message header/text to the netmail directory.
**Calls:** `ValidateInfoStruct`, `ConvertStringToAddress`, `CreateMessage`, `malloc`, `free`

---

## qtest.c

### Display
**Prototype:** `void Display(char *FileName)`
**Description:** Mock display function that prints a debug message showing the file name (used in interactive mode).
**Calls:** `zputs`

---

### LogStr
**Prototype:** `void LogStr(const char *szString)`
**Description:** Mock logging function that outputs a string to stdout (used by quest event system).
**Calls:** `fputs`

---

### DupeStr
**Prototype:** `char *DupeStr(const char *str)`
**Description:** Allocates memory and duplicates a string with memory check.
**Calls:** `strdup`, `CheckMem`

---

### script_read_line
**Prototype:** `static bool script_read_line(char *buf, size_t sz)`
**Description:** Reads a non-empty line from the script file, stripping trailing newlines and carriage returns.
**Calls:** `fgets`, `strlen`

---

### script_consume
**Prototype:** `static const char *script_consume(const char *expected_type)`
**Description:** Reads and validates the next script line has the expected type prefix and returns the value part after '='; exits with code 2 on mismatch.
**Calls:** `script_read_line`, `strcmp`, `strchr`, `strncasecmp`, `fprintf`, `memcpy`

---

### script_expect_end
**Prototype:** `static void script_expect_end(void)`
**Description:** Verifies the next script line is "End"; exits with code 3 if not or on EOF.
**Calls:** `script_read_line`, `strcmp`, `fprintf`

---

### hook_get_answer
**Prototype:** `static char hook_get_answer(const char *szAllowableChars)`
**Description:** Script mode callback for GetAnswer that reads Choice= line and validates the character against allowed set.
**Calls:** `script_consume`, `toupper`, `fprintf`

---

### hook_dos_get_str
**Prototype:** `static void hook_dos_get_str(char *InputStr, int16_t MaxChars, bool HiBit)`
**Description:** Script mode callback for DosGetStr that reads String= line and copies value to input buffer.
**Calls:** `script_consume`, `strlcpy`

---

### hook_dos_get_long
**Prototype:** `static long hook_dos_get_long(const char *Prompt, long DefaultVal, long Maximum)`
**Description:** Script mode callback for DosGetLong that reads Number= line and parses as long with clamping to maximum.
**Calls:** `script_consume`, `atol`

---

### hook_get_string_choice
**Prototype:** `static int16_t hook_get_string_choice(const char **apszChoices, int16_t NumChoices, bool AllowBlank)`
**Description:** Script mode callback for GetStringChoice (NPC topic selection) that reads Topic= line and performs exact or prefix matching with validation.
**Calls:** `script_consume`, `strcasecmp`, `strlen`, `strncasecmp`, `fprintf`

---

### Fight_Fight
**Prototype:** `int16_t Fight_Fight(struct clan *Attacker, struct clan *Defender, bool HumanEnemy, bool CanRun, bool AutoFight)`
**Description:** Mock combat function that in script mode reduces attacker members' HP and SP by 1 and returns outcome; in interactive mode prompts user for Win/Lose/Run result.
**Calls:** `script_consume`, `snprintf`, `zputs`, `GetAnswer`, `toupper`

---

### Fight_CheckLevelUp
**Prototype:** `void Fight_CheckLevelUp(void)`
**Description:** Mock level-up checker (no-op for quest testing).
**Calls:** None

---

### FreeClan
**Prototype:** `void FreeClan(struct clan *Clan)`
**Description:** Mock clan cleanup function (no-op for quest testing).
**Calls:** None

---

### FreeClanMembers
**Prototype:** `void FreeClanMembers(struct clan *Clan)`
**Description:** Frees all allocated member structures in a clan.
**Calls:** `free`

---

### News_AddNews
**Prototype:** `void News_AddNews(char *szString)`
**Description:** Mock news posting function that prints a debug message with the news string.
**Calls:** `snprintf`, `zputs`

---

### Items_GiveItem
**Prototype:** `void Items_GiveItem(char *szItemName)`
**Description:** Mock item giving function that prints a debug message with the item name.
**Calls:** `snprintf`, `zputs`

---

### NumMembers
**Prototype:** `int16_t NumMembers(struct clan *Clan, bool OnlyOnesHere)`
**Description:** Counts live members in a clan, optionally filtering by "Here" status.
**Calls:** None

---

### GetClanNameID
**Prototype:** `bool GetClanNameID(char *szName, size_t sz, int16_t ID[2])`
**Description:** Mock clan lookup that returns a fixed mock clan name "[MockClan]".
**Calls:** `strlcpy`

---

### GetClan
**Prototype:** `bool GetClan(int16_t ClanID[2], struct clan *TmpClan)`
**Description:** Mock clan retrieval that initializes structure and sets mock clan name.
**Calls:** `memset`, `strlcpy`

---

### ClanStats
**Prototype:** `void ClanStats(struct clan *Clan, bool AllowModify)`
**Description:** Mock clan statistics display that prints debug message with clan name and modify flag.
**Calls:** `snprintf`, `zputs`

---

### GetStat
**Prototype:** `int8_t GetStat(struct pc *PC, char Stat)`
**Description:** Retrieves an attribute from a player character by type (strength, dexterity, agility, wisdom, armor strength, charisma).
**Calls:** None

---

### my_random
**Prototype:** `int my_random(int limit)`
**Description:** Mock random function that in script mode reads Random= line and parses value; in interactive mode prompts user for random number 0 to limit-1.
**Calls:** `script_consume`, `atoi`, `snprintf`, `zputs`, `DosGetStr`

---

### setup_dummy_pclan
**Prototype:** `static void setup_dummy_pclan(void)`
**Description:** Initializes a test clan with four archetype members (Fighter, Mage, Cleric, Thief) with predefined attributes, HP/SP, and class, and marks all active known quests as known to the test clan.
**Calls:** `memset`, `strlcpy`

---

### setup_dummy_village
**Prototype:** `static void setup_dummy_village(void)`
**Description:** Initializes a test village with basic data structure marked as initialized.
**Calls:** `memset`, `strlcpy`

---

### print_flags
**Prototype:** `static void print_flags(const char *label, uint8_t flags[8])`
**Description:** Displays all set flag numbers for a flag group in interactive mode.
**Calls:** `snprintf`, `zputs`

---

### print_quest_flags
**Prototype:** `static void print_quest_flags(const char *label, uint8_t flags[8], int numquests)`
**Description:** Displays all set quest numbers (1-based) for a quest flag group in interactive mode.
**Calls:** `snprintf`, `zputs`

---

### count_active_quests
**Prototype:** `static int count_active_quests(void)`
**Description:** Counts how many quest slots are active in the Quests array.
**Calls:** None

---

### print_flag_summary
**Prototype:** `static void print_flag_summary(uint8_t flags[8])`
**Description:** Outputs comma-separated set flag numbers to stderr for script mode state summary.
**Calls:** `fputc`, `fprintf`

---

### print_quest_summary
**Prototype:** `static void print_quest_summary(const char *key, uint8_t flags[8])`
**Description:** Outputs quest key=comma-separated quest numbers to stderr for script mode state summary.
**Calls:** `fprintf`, `fputc`

---

### print_state_summary
**Prototype:** `static void print_state_summary(void)`
**Description:** Outputs complete state summary to stderr in script mode including flags, gold, mine level, points, fights, followers, quest status, and member stats.
**Calls:** `fputs`, `print_flag_summary`, `fprintf`, `print_quest_summary`

---

### parse_flag_list
**Prototype:** `static void parse_flag_list(const char *list, uint8_t flags[8])`
**Description:** Parses comma-separated flag numbers and sets corresponding bits in the flag array.
**Calls:** `strlcpy`, `strtok`, `atoi`

---

### parse_quest_list
**Prototype:** `static void parse_quest_list(const char *list, uint8_t qflags[8])`
**Description:** Parses comma-separated 1-based quest numbers and sets corresponding bits in the quest flag array.
**Calls:** `strlcpy`, `strtok`, `atoi`

---

### show_main_screen
**Prototype:** `static void show_main_screen(void)`
**Description:** Displays the main interactive menu with current flag values, quest status, and options for flag editing, gold/mine level editing, NPC chat, quest running, and exit.
**Calls:** `clrscr`, `zputs`, `print_flags`, `print_quest_flags`, `count_active_quests`, `snprintf`

---

### set_or_clear_flag
**Prototype:** `static void set_or_clear_flag(uint8_t flags[8], const char *label)`
**Description:** Interactive prompt to set or clear a single flag number (0-63) with input validation.
**Calls:** `snprintf`, `zputs`, `GetAnswer`, `toupper`, `DosGetStr`, `atoi`

---

### edit_flags_menu
**Prototype:** `static void edit_flags_menu(void)`
**Description:** Interactive menu to select a flag group (G/H/P/D/T) and edit individual flags within that group.
**Calls:** `clrscr`, `zputs`, `GetAnswer`, `snprintf`, `toupper`, `set_or_clear_flag`, `door_pause`

---

### edit_gold_mine
**Prototype:** `static void edit_gold_mine(void)`
**Description:** Interactive menu to modify test clan's vault gold and mine level using DosGetLong prompts.
**Calls:** `clrscr`, `zputs`, `snprintf`, `DosGetLong`

---

### collect_npcs
**Prototype:** `static int collect_npcs(struct DBGNPCEntry *entries, int maxentries)`
**Description:** Loads NPC definitions from all configured NPC files and populates a local entry array with name, index, wander location, and odds of seeing.
**Calls:** `malloc`, `CheckMem`, `MyOpen`, `ftell`, `fread`, `s_NPCInfo_d`, `strlcpy`, `fclose`, `free`

---

### wander_name
**Prototype:** `static const char *wander_name(int16_t w)`
**Description:** Returns a human-readable name for an NPC wander location constant.
**Calls:** None

---

### npc_submenu
**Prototype:** `static void npc_submenu(void)`
**Description:** Interactive NPC browser that displays all available NPCs and allows the user to select one to chat with using the quest system's NPC_ChatNPC function.
**Calls:** `collect_npcs`, `clrscr`, `zputs`, `snprintf`, `door_pause`, `DosGetStr`, `atoi`, `NPC_ChatNPC`

---

### quest_submenu
**Prototype:** `static void quest_submenu(void)`
**Description:** Interactive quest runner that displays all active quests with known/done status, allows selection, shows quest description from help file, optionally forces quest as known, and runs the selected quest event.
**Calls:** `clrscr`, `zputs`, `snprintf`, `DosGetStr`, `atoi`, `Help`, `door_pause`, `GetAnswer`, `toupper`, `ClearFlags`, `RunEvent`

---

### dbg_init
**Prototype:** `static void dbg_init(void)`
**Description:** Initializes qtest environment by setting up video, loading clans.ini configuration, initializing language system, setting up quest data, and creating dummy test clan and village.
**Calls:** `Video_Init`, `ClansIni_Init`, `Language_Init`, `Quests_Init`, `setup_dummy_pclan`, `setup_dummy_village`

---

### dbg_close
**Prototype:** `static void dbg_close(void)`
**Description:** Cleans up qtest environment by closing quests, language, configuration, showing text cursor, and closing video system.
**Calls:** `Quests_Close`, `Language_Close`, `ClansIni_Close`, `ShowTextCursor`, `Video_Close`

---

### main
**Prototype:** `int main(int argc, char *argv[])`
**Description:** Quest Pack Debugger main entry point that parses command-line options for state setup (-G/-H/-P/-D/-T flags, -g/-m gold/mine, -c charisma, -q/-Q quest status, -n NPC index, -e event file/label, -s script), installs script mode hooks, initializes environment, applies state, and either runs a single quest/NPC in script mode or enters interactive menu mode.
**Calls:** `fprintf`, `fopen`, `strlcpy`, `atol`, `parse_flag_list`, `parse_quest_list`, `Console_SetScriptMode`, `Console_SetGetAnswerHook`, `Video_SetScriptMode`, `Video_SetDosGetStrHook`, `Video_SetDosGetLongHook`, `Input_SetGetStringChoiceHook`, `dbg_init`, `NPC_ChatNPC`, `ClearFlags`, `RunEvent`, `script_expect_end`, `print_state_summary`, `dbg_close`, `fclose`, `show_main_screen`, `GetAnswer`, `toupper`, `snprintf`, `zputs`, `edit_flags_menu`, `edit_gold_mine`, `npc_submenu`, `quest_submenu`

---

## empire.c (lines 1576–3149)

### DestroyBuildings
**Prototype:** `static void DestroyBuildings(int16_t NumBuildings[MAX_BUILDINGS], int16_t NumDestroyed[MAX_BUILDINGS], int16_t Percent, int16_t *LandGained)`
**Description:** Simulates destruction of buildings during an empire attack by randomly selecting buildings to damage proportional to a percentage; calculates remaining buildings and land gained.
**Calls:** `my_random`

---

### ShowResults
**Prototype:** `static void ShowResults(struct AttackResult *Result)`
**Description:** Displays attack battle results to the user, including casualty counts and outcomes based on attack goal (oust ruler, steal gold/land, or destroy buildings).
**Calls:** `rputs`, `snprintf`

---

### GetNumTroops
**Prototype:** `static void GetNumTroops(struct Army *OriginalArmy, struct Army *AttackingArmy)`
**Description:** Presents an interactive menu for the player to select how many footmen, axemen, and knights to send into battle; defaults strategy from original army.
**Calls:** `rputs`, `snprintf`, `GetAnswer`, `GetLong`

---

### StartEmpireWar
**Prototype:** `static void StartEmpireWar(struct empire *Empire)`
**Description:** Orchestrates a full empire attack including target selection (village/alliance/clan), troop assignment, attack goal selection, and result processing; handles both local and inter-BBS attacks.
**Calls:** `Help`, `rputs`, `GetStringChoice`, `GetNumTroops`, `YesNo`, `VillageName`, `EmpireAttack`, `ProcessAttackResult`, `ShowResults`, `GenericMessage`, `News_AddNews`, `IBBS_SendAttackPacket`, `GetClan`, `FreeClanMembers`, `Clan_Update`, `User_Write`, `door_pause`

---

### SpyMenu
**Prototype:** `static void SpyMenu(struct empire *Empire)`
**Description:** Manages spy operations on villages, alliances, or clans; deducts spy cost, compares agency vs security buildings to determine success, and sends inter-BBS spy packets or displays local results.
**Calls:** `Help`, `rputs`, `snprintf`, `GetStringChoice`, `YesNo`, `my_random`, `Empire_Stats`, `GenericMessage`, `GetClan`, `FreeClanMembers`, `IBBS_SendSpy`

---

### Empire_Manage
**Prototype:** `void Empire_Manage(struct empire *Empire)`
**Description:** Main empire management menu presenting options for developing land, donating gold, building structures, managing army, attacking rivals, or spying on targets; loops until user exits.
**Calls:** `LoadStrings`, `rputs`, `snprintf`, `GetChoice`, `Help`, `GeneralHelp`, `DevelopLand`, `DonateToEmpire`, `StructureMenu`, `ManageArmy`, `StartEmpireWar`, `SpyMenu`

---

## user.c (lines 1321–2642)

### PC_Create
**Prototype:** `void PC_Create(struct pc *PC, bool ClanLeader)`
**Description:** Creates a new player character with race/class selection, random stats initialization, and prompts user for character name.
**Calls:** `GetClass`, `my_random`, `rputs`, `ShowPlayerStats`, `YesNo`, `GetStr`, `RemovePipes`, `Strip`

---

### NameInUse
**Prototype:** `static bool NameInUse(char *szName)`
**Description:** Checks if a given clan name is already in use locally or in inter-BBS league.
**Calls:** `fopen`, `fclose`, `fseek`, `notEncryptRead_s`, `IBBS_InList`

---

### ChooseClanName
**Prototype:** `static bool ChooseClanName(char *szName)`
**Description:** Prompts user to enter and confirm a unique clan name, checking for validity and conflicts.
**Calls:** `rputs`, `GetStr`, `RemovePipes`, `Strip`, `NameInUse`, `YesNo`

---

### User_ResetHelp
**Prototype:** `static void User_ResetHelp(void)`
**Description:** Resets all clan help flags to false.
**Calls:** None

---

### User_Create
**Prototype:** `static bool User_Create(void)`
**Description:** Creates a new clan with initial stats, member roster, and writes to player file; posts new clan news and adds to league if inter-BBS enabled.
**Calls:** `strlcpy`, `Help`, `YesNo`, `ChooseClanName`, `User_ResetHelp`, `ClearFlags`, `Empire_Create`, `PC_Create`, `malloc`, `CheckMem`, `fopen`, `fclose`, `fseek`, `EncryptWrite_s`, `News_AddNews`, `IBBS_LeagueNewUser`

---

### User_Destroy
**Prototype:** `void User_Destroy(void)`
**Description:** Destroys the clan in memory by freeing all member allocations and zeroing PClan struct.
**Calls:** `free`, `memset`

---

### User_Read
**Prototype:** `static bool User_Read(void)`
**Description:** Reads the current player's clan from the player data file; allocates and initializes member array.
**Calls:** `fopen`, `fclose`, `fseek`, `notEncryptRead_s`, `EncryptRead_s`, `malloc`, `CheckMem`

---

### Clan_Update
**Prototype:** `void Clan_Update(struct clan *Clan)`
**Description:** Locates clan in player file and overwrites its data; serializes clan and all members to encrypted format.
**Calls:** `fopen`, `fclose`, `fseek`, `ftell`, `notEncryptRead_s`, `s_clan_s`, `s_pc_s`, `EncryptWrite`

---

### User_Write
**Prototype:** `void User_Write(void)`
**Description:** Writes the current user's clan data to file if initialized.
**Calls:** `Clan_Update`

---

### User_FirstTimeToday
**Prototype:** `void User_FirstTimeToday(void)`
**Description:** Awards daily points and restores all clan members to full HP/SP and "Here" status on first login of the day.
**Calls:** None

---

### NumMembers
**Prototype:** `int16_t NumMembers(struct clan *Clan, bool OnlyOnesHere)`
**Description:** Returns count of existing clan members; optionally counts only members with "Here" status.
**Calls:** None

---

### GetClanID
**Prototype:** `bool GetClanID(int16_t ID[2], bool OnlyLiving, bool IncludeSelf, int16_t WhichAlliance, bool InAllianceOnly)`
**Description:** Displays a list of clans with filtering by living members and alliance membership; returns selected clan IDs via output parameter.
**Calls:** `fopen`, `fclose`, `fseek`, `notEncryptRead_s`, `EncryptRead_s`, `DupeStr`, `GetStringChoice`, `rputs`, `free`

---

### GetClanNameID
**Prototype:** `bool GetClanNameID(char *szName, size_t sz, int16_t ID[2])`
**Description:** Looks up clan name by ID pair and copies it to output buffer.
**Calls:** `fopen`, `fclose`, `fseek`, `notEncryptRead_s`, `strlcpy`

---

### GetClan
**Prototype:** `bool GetClan(int16_t ClanID[2], struct clan *TmpClan)`
**Description:** Reads a complete clan record (struct + all members) from player file by ID pair; allocates member memory and sets parent pointers.
**Calls:** `fopen`, `fclose`, `fseek`, `notEncryptRead_s`, `malloc`, `CheckMem`, `EncryptRead_s`, `free`

---

### User_List
**Prototype:** `void User_List(void)`
**Description:** Displays all clans with their IDs and names; uses inter-BBS userlist if enabled, otherwise reads local player file.
**Calls:** `fputs`, `fopen`, `fclose`, `fseek`, `notEncryptRead_s`, `printf`

---

### User_Maint
**Prototype:** `void User_Maint(void)`
**Description:** Performs daily maintenance on all clans: resets daily flags, revives unconscious members, validates experience, removes NPC members, fixes item ownership, and writes updated data.
**Calls:** `LogDisplayStr`, `fopen`, `fclose`, `System_Error`, `notEncryptRead_s`, `EncryptRead_s`, `malloc`, `CheckMem`, `ClearFlags`, `Empire_Maint`, `UnequipItemsFromPC`, `EncryptWrite_s`, `free`, `unlink`, `rename`

---

### User_Init
**Prototype:** `bool User_Init(void)`
**Description:** Initializes current user: attempts to load existing clan from file; if not found, creates new clan after checking inter-BBS league for duplicates.
**Calls:** `User_Read`, `IBBS_InList`, `rputs`, `User_Create`

---

### User_Close
**Prototype:** `void User_Close(void)`
**Description:** Shuts down user data by writing changes to file and destroying in-memory clan struct.
**Calls:** `User_Write`, `User_Destroy`

---

## mcomp.c

### main
**Prototype:** `int main(int argc, char *argv[])`
**Description:** Monster compiler that reads a .txt file, parses monster definitions, and writes compiled binary .mon file with indexed monster data.
**Calls:** `u8_fopen`, `u8_fgets`, `printf`, `fopen`, `fwrite`, `fseek`, `fclose`, `strlcpy`, `ato16`, `ato8`, `s_pc_s`

---

## mclass.c

### main
**Prototype:** `int main(int argc, char **argv)`
**Description:** Class/race compiler that reads class.txt, parses player class definitions, and writes compiled class.cls file with serialized class data.
**Calls:** `Init_PClasses`, `fopen`, `printf`, `fwrite`, `fclose`, `Deinit_PClasses`, `s_PClass_s`

---

### Deinit_PClasses
**Prototype:** `static void Deinit_PClasses(void)`
**Description:** Deallocates all dynamically allocated player class structures.
**Calls:** None

---

### Init_PClasses
**Prototype:** `static int16_t Init_PClasses(char *szFileName)`
**Description:** Parses a class definition file, populates a global array of player classes with attributes, spells, and stats.
**Calls:** `u8_fopen`, `u8_fgets`, `printf`, `exit`, `calloc`, `strlcpy`, `atoi`, `fclose`

---

## mitems.c

### main
**Prototype:** `int main(int argc, char *argv[])`
**Description:** Items compiler that reads items.txt, parses item definitions, and writes compiled items.dat file with serialized item data.
**Calls:** `Init_Items`, `fopen`, `printf`, `fwrite`, `fclose`, `Deinit_Items`, `s_item_data_s`

---

### Deinit_Items
**Prototype:** `static void Deinit_Items(void)`
**Description:** Deallocates all dynamically allocated item structures.
**Calls:** None

---

### Init_Items
**Prototype:** `static void Init_Items(char *szFileName)`
**Description:** Parses an item definition file, populates a global array of items with attributes, requirements, costs, and spell references.
**Calls:** `u8_fopen`, `u8_fgets`, `printf`, `exit`, `calloc`, `strlcpy`, `strcasecmp`, `ato8`, `ato16`, `ato32`

---

## mspells.c

### main
**Prototype:** `int main(int argc, char *argv[])`
**Description:** Spell compiler that reads spells.txt, parses spell definitions with damage/heal/modify strings, and writes compiled spells.dat file.
**Calls:** `printf`, `Init_Spells`, `fopen`, `fwrite`, `fclose`, `strlen`, `Deinit_Spells`, `s_Spell_s`

---

### Deinit_Spells
**Prototype:** `static void Deinit_Spells(void)`
**Description:** Deallocates all dynamically allocated spell structures and their string fields.
**Calls:** None

---

### Init_Spells
**Prototype:** `static void Init_Spells(char *szFileName)`
**Description:** Parses a spell definition file, populates a global array of spells with attributes, flags, damage strings, and effects.
**Calls:** `u8_fopen`, `u8_fgets`, `printf`, `exit`, `calloc`, `strlcpy`, `strcasecmp`, `ato8`, `ato16`, `strdup`, `fclose`

---

## makenpc.c

### main
**Prototype:** `int main(int argc, char **argv)`
**Description:** NPC compiler that reads NPC definition file and writes compiled .npc file with NPC topic data.
**Calls:** `printf`, `Init_NPCs`, `exit`

---

### Init_NPCs
**Prototype:** `static void Init_NPCs(char *szInfile, char *szOutfile)`
**Description:** Parses an NPC definition file, initializes NPC structures with topics, loyalty, locations, and writes compiled binary output.
**Calls:** `u8_fopen`, `u8_fgets`, `printf`, `exit`, `fopen`, `calloc`, `memset`, `strlcpy`, `GetToken`, `atoi`, `s_NPCInfo_s`, `fwrite`, `fclose`, `free`

---

## makepak.c

### main
**Prototype:** `int main(int argc, char *argv[])`
**Description:** PAK packager that reads a file listing, aggregates files with headers into a single .pak archive file.
**Calls:** `printf`, `exit`, `strlcpy`, `fopen`, `fclose`, `fgets`, `GetToken`, `AddToPak`

---

### AddToPak
**Prototype:** `static void AddToPak(char *pszFileName, char *pszFileAlias, FILE *fpPakFile)`
**Description:** Reads a single file, converts UTF-8 text to CP437 if needed, creates a file header, and writes the file data to the PAK archive.
**Calls:** `u8_fopen`, `printf`, `fclose`, `u8_fgets`, `strlen`, `malloc`, `fputs`, `fflush`, `exit`, `memcpy`, `realloc`, `free`, `fseek`, `ftell`, `strlcpy`, `s_FileHeader_s`, `fwrite`, `fread`, `calloc`

---

## install.c

### Display
**Prototype:** `static void Display(char *szFileName)`
**Description:** Displays file contents from install.ini or filesystem, with paging support.
**Calls:** `u8_fgets`, `zputs`, `cio_getch`

---

### detect_utf8_marker
**Prototype:** `static void detect_utf8_marker(void)`
**Description:** Detects UTF-8 encoding marker in install.ini file.
**Calls:** None

---

### SystemInit
**Prototype:** `static void SystemInit(void)`
**Description:** Initializes video and retrieves GUM archive name.
**Calls:** `Video_Init`, `GetGumName`

---

### main
**Prototype:** `int main(int argc, char **argv)`
**Description:** Main installer interface with interactive menu for installation, upgrade, file extraction, and display.
**Calls:** `SystemInit`, `detect_utf8_marker`, `clrscr`, `gotoxy`, `Display`, `SetScrollRegion`, `zputs`, `DosGetStr`, `GetToken`, `install`, `upgrade`, `ListFiles`, `Extract`, `ClearScrollRegion`, `Video_Close`

---

### ReadFilename
**Prototype:** `static bool ReadFilename(FILE *fpGUM, char *szEncryptedName, size_t sz)`
**Description:** Reads and decodes encrypted filename from GUM archive.
**Calls:** None

---

### fseekuc
**Prototype:** `static int fseekuc(FILE *fp, uint32_t offset)`
**Description:** Seeks forward by offset, handling large offsets that exceed LONG_MAX.
**Calls:** None

---

### GetGUM
**Prototype:** `static int GetGUM(FILE *fpGUM)`
**Description:** Extracts a single file from GUM archive, handling overwrite prompts and file permissions.
**Calls:** `ClearAll`, `ReadFilename`, `zputs`, `MKDIR`, `FileExists`, `WriteType`, `GetAnswer`, `DosGetStr`, `fopen`, `fclose`, `decode`, `_dos_setftime`, `fileno`, `fseekuc`

---

### install
**Prototype:** `static void install(void)`
**Description:** Performs full installation by extracting files from GUM archive.
**Calls:** `Display`, `zputs`, `GetAnswer`, `InitFiles`, `fopen`, `GetGUM`, `fclose`, `chmod`

---

### upgrade
**Prototype:** `static void upgrade(void)`
**Description:** Performs upgrade installation, similar to install but using different file list.
**Calls:** `Display`, `zputs`, `GetAnswer`, `InitFiles`, `fopen`, `GetGUM`, `fclose`, `chmod`

---

### FileExists
**Prototype:** `static bool FileExists(char *szFileName)`
**Description:** Checks if file exists by attempting to open it.
**Calls:** None

---

### FreeFiles
**Prototype:** `static void FreeFiles(void)`
**Description:** Frees all allocated file information entries.
**Calls:** None

---

### InitFiles
**Prototype:** `static void InitFiles(char *szFileName)`
**Description:** Initializes file list from install.ini, parsing file patterns and write types.
**Calls:** `FreeFiles`, `fopen`, `u8_fgets`, `fclose`, `zputs`, `reset_attribute`, `Video_Close`, `exit`, `snprintf`

---

### Match
**Prototype:** `static bool Match(const char *re, const char *Candidate)`
**Description:** Recursive wildcard pattern matching supporting * and ? wildcards.
**Calls:** None

---

### WriteType
**Prototype:** `static int WriteType(char *szFileName)`
**Description:** Determines write behavior (SKIP, OVERWRITE, or QUERY) for a file based on pattern matching.
**Calls:** `Match`

---

### Extract
**Prototype:** `static void Extract(char *szExtractFile, char *szNewName)`
**Description:** Extracts a specific file from GUM archive with filename search and optional rename.
**Calls:** `ClearAll`, `fopen`, `zputs`, `fclose`, `ReadFilename`, `fseekuc`, `FileExists`, `GetAnswer`, `DosGetStr`, `decode`, `_dos_setftime`, `fileno`, `snprintf`

---

### CheckRow
**Prototype:** `static int CheckRow(int row, bool *Done)`
**Description:** Checks if display has reached bottom and prompts for more, handling quit request.
**Calls:** `zputs`, `cio_getch`, `toupper`

---

### AddLFN
**Prototype:** `static void AddLFN(const char *name, uint32_t size)`
**Description:** Adds a long filename entry to internal linked list.
**Calls:** `malloc`, `strlen`, `System_Error`, `strdup`

---

### FreeLFNs
**Prototype:** `static void FreeLFNs(void)`
**Description:** Frees all long filename entries from linked list.
**Calls:** None

---

### DisplayLFNs
**Prototype:** `static void DisplayLFNs(int row, int column, size_t TotalBytes)`
**Description:** Displays long filename list in columns with size information.
**Calls:** `zputs`, `snprintf`, `CheckRow`, `FreeLFNs`

---

### ListFiles
**Prototype:** `static void ListFiles(void)`
**Description:** Lists all files in GUM archive with size information, organized in columns.
**Calls:** `fopen`, `zputs`, `ReadFilename`, `DisplayLFNs`, `fread`, `SWAP32`, `SWAP16`, `fseekuc`, `fclose`, `AddLFN`, `snprintf`, `System_Error`

---

### GetGumName
**Prototype:** `static void GetGumName(void)`
**Description:** Reads GUM archive filename from install.ini.
**Calls:** `FreeFiles`, `fopen`, `u8_fgets`, `fclose`, `zputs`, `reset_attribute`, `Video_Close`, `exit`

---

### _dos_setftime
**Prototype:** `static int _dos_setftime(int handle, unsigned short date, unsigned short time)`
**Description:** Sets file modification timestamp from DOS date/time values (Unix/Windows compatibility).
**Calls:** `memset`, `mktime`

---

## langcomp.c

### main
**Prototype:** `int main(int argc, char *argv[])`
**Description:** Compiles language string file (UTF-8) into binary format with optional macro header generation.
**Calls:** `printf`, `strlcpy`, `strlcat`, `strlen`, `u8_fopen`, `u8_fgets`, `malloc`, `free`, `fclose`, `atoi`, `Convert`, `s_Language_s`, `fwrite`, `fopen`, `fprintf`, `snprintf`

---

### Convert
**Prototype:** `void Convert(char *Dest, char *From)`
**Description:** Converts special language code sequences (^M, ^[, etc.) to control characters in strings.
**Calls:** None

---

## ecomp.c

### main
**Prototype:** `int main(int argc, char *argv[])`
**Description:** Event compiler that reads event definition file and generates binary event/result/topic blocks.
**Calls:** `printf`, `u8_fopen`, `fopen`, `fclose`, `calloc`, `u8_fgets`, `fgetc`, `ParseLine`, `GetToken`, `memmove`, `strlen`, `strcasecmp`, `strlcpy`, `snprintf`, `atoi`, `s_EventHeader_s`, `fwrite`, `free`, `exit`

---

## ibbs.c (lines 1779–3557)

### getPacketFP
**Prototype:** `static FILE *getPacketFP(int16_t DestID)`
**Description:** Returns a cached file pointer for a destination BBS's outbound packet file, creating it on first call with XOR encryption; maintains linked list of open files.
**Calls:** `malloc`, `CheckMem`, `_fsopen`, `System_Error`, `strlcpy`, `strlcat`, `snprintf`

---

### IBBS_SendPacketBuffer
**Prototype:** `static void IBBS_SendPacketBuffer(int16_t DestID, char *PacketHeader, char *PacketData, size_t PacketDataSize)`
**Description:** Writes an encrypted packet header and optional data to an outbound packet file via cached file pointer.
**Calls:** `getPacketFP`, `CheckedEncryptWrite`, `LogDisplayStr`, `DaysSince1970`

---

### IBBS_SendSpy
**Prototype:** `void IBBS_SendSpy(struct empire *Empire, int16_t DestID)`
**Description:** Constructs and sends a spy attempt packet from the current empire to a destination BBS.
**Calls:** `snprintf`, `s_SpyAttemptPacket_s`, `IBBS_SendPacket`, `rputs`

---

### IBBS_SendAttackPacket
**Prototype:** `void IBBS_SendAttackPacket(struct empire *AttackingEmpire, struct Army *AttackingArmy, int16_t Goal, int16_t ExtentOfAttack, int16_t TargetType, int16_t ClanID[2], int16_t DestID)`
**Description:** Constructs and sends an attack packet with full details of the attacking empire, army, and target.
**Calls:** `s_AttackPacket_s`, `IBBS_SendPacket`

---

### IBBS_ShowLeagueAscii
**Prototype:** `void IBBS_ShowLeagueAscii(void)`
**Description:** Reads and displays ASCII art from world.ndx file; outputs any lines starting with "Ascii" to the user.
**Calls:** `fopen`, `fclose`, `fgets`, `strlcpy`, `strcasecmp`, `rputs`, `snprintf`

---

### IBBS_SendPacket
**Prototype:** `void IBBS_SendPacket(int16_t PacketType, size_t PacketLength, void *PacketData, int16_t DestID)`
**Description:** Constructs a packet header with metadata and sends it via IBBS_SendPacketBuffer; optionally backs up attack and clanmove packets to backup.dat.
**Calls:** `s_Packet_s`, `IBBS_SendPacketBuffer`, `fopen`, `fclose`, `CheckedEncryptWrite`, `LogDisplayStr`, `System_Error`

---

### IBBS_EnqueueOutPacket
**Prototype:** `void IBBS_EnqueueOutPacket(int16_t PacketType, size_t PacketLength, void *PacketData, int16_t DestID)`
**Description:** Enqueues an outbound packet to pktqout.dat for later processing, used to defer packet sends until close time.
**Calls:** `s_Packet_s`, `fopen`, `fclose`, `CheckedEncryptWrite`, `LogDisplayStr`, `System_Error`

---

### IBBS_SendRecon
**Prototype:** `void IBBS_SendRecon(int16_t DestID)`
**Description:** Sends a PT_RECON packet to a destination BBS if it is active.
**Calls:** `IBBS_SendPacket`

---

### IBBS_SendReset
**Prototype:** `void IBBS_SendReset(int16_t DestID)`
**Description:** Serializes current game data and sends it via PT_RESET packet to a destination BBS if active.
**Calls:** `s_game_data_s`, `IBBS_SendPacket`

---

### IBBS_UpdateRecon
**Prototype:** `void IBBS_UpdateRecon(void)`
**Description:** Iterates all active IBBS nodes and sends initial PT_RECON packets to nodes never contacted; also periodically resends if stale based on RECONDAYS threshold.
**Calls:** `IBBS_SendRecon`, `DaysSince1970`, `IBBS_SendUserList`

---

### IBBS_UpdateReset
**Prototype:** `void IBBS_UpdateReset(void)`
**Description:** (League Controller only) Sends PT_RESET packets to all active nodes that have not yet received a reset today.
**Calls:** `IBBS_SendReset`, `DaysSince1970`

---

### IBBS_LeagueInfo
**Prototype:** `void IBBS_LeagueInfo(void)`
**Description:** Displays league status and member BBS information including recon history, game state, and world name; handles pagination via door_pause.
**Calls:** `rputs`, `snprintf`, `DaysBetween`, `DaysSince1970`, `BBSName`, `VillageName`, `door_pause`

---

### Reset
**Prototype:** `static void Reset(void)`
**Description:** Clears all game session files (clans, trades, NPC data), reinitializes IBBS, resets village state, and updates news with reset notification.
**Calls:** `unlink`, `KillAlliances`, `IBBS_Create`, `IBBS_UpdateRecon`, `Village_Reset`, `rename`, `News_CreateTodayNews`, `News_AddNews`

---

### IBBS_Reset
**Prototype:** `static void IBBS_Reset(struct game_data *GameData)`
**Description:** Processes a received reset packet; updates game data, schedules game start if today is start date, and calls Reset to reinitialize the node.
**Calls:** `strcasecmp`, `LogDisplayStr`, `DaysBetween`, `Game_Start`, `Reset`

---

### IBBS_RemoveFromBackup
**Prototype:** `static void IBBS_RemoveFromBackup(int16_t ID[2])`
**Description:** Searches backup.dat for a PT_CLANMOVE packet matching the given clan ID and marks it inactive to prevent duplicate clan arrival.
**Calls:** `fopen`, `fclose`, `ftell`, `fseek`, `notEncryptRead_s`, `EncryptWrite_s`, `LogDisplayStr`

---

### IBBS_AddToGame
**Prototype:** `static void IBBS_AddToGame(struct clan *Clan, bool WasLost)`
**Description:** Adds a clan to the game by merging with existing clan data (alliances, vault, army) or appending as new; updates clans.pc file.
**Calls:** `snprintf`, `News_AddNews`, `fopen`, `fclose`, `fseek`, `notEncryptRead_s`, `EncryptWrite_s`, `malloc`, `free`, `CheckMem`, `System_Error`

---

### ComeBack
**Prototype:** `static void ComeBack(int16_t ClanID[2], int16_t BBSID)`
**Description:** Marks a clan as leaving for a destination BBS and creates a leaving.dat entry so troops return home on arrival.
**Calls:** `GetClan`, `FreeClanMembers`, `fopen`, `fclose`, `EncryptWrite_s`, `Clan_Update`

---

### CheckID
**Prototype:** `static int16_t CheckID(int ID, const char *name)`
**Description:** Validates that a BBS ID is within range [1, MAX_IBBSNODES] and active; logs error and returns false if invalid.
**Calls:** `LogDisplayStr`, `snprintf`

---

### CheckSourceID
**Prototype:** `static int16_t CheckSourceID(int ID)`
**Description:** Wrapper to validate a source BBS ID using CheckID.
**Calls:** `CheckID`

---

### CheckDestinationID
**Prototype:** `static int16_t CheckDestinationID(int ID)`
**Description:** Wrapper to validate a destination BBS ID using CheckID.
**Calls:** `CheckID`

---

### HandleResetPacket
**Prototype:** `static void HandleResetPacket(struct Packet* Packet, FILE* fp)`
**Description:** Reads game data from a PT_RESET packet and processes it via IBBS_Reset; sends PT_GOTRESET acknowledgment.
**Calls:** `EncryptRead_s`, `IBBS_Reset`, `IBBS_SendPacket`

---

### HandleMessagePacket
**Prototype:** `static void HandleMessagePacket(FILE* fp, const char *szFileName)`
**Description:** Reads a message from a PT_MSJ packet, allocates memory, loads message text, and posts it via PostMsj.
**Calls:** `EncryptRead_s`, `EncryptRead`, `PostMsj`, `malloc`, `free`, `CheckMem`, `fclose`, `LogDisplayStr`, `MoveToBad`

---

### HandleScoreDataPacket
**Prototype:** `static void HandleScoreDataPacket(FILE* fp)`
**Description:** Reads PT_SCOREDATA packet containing an array of user score records and processes them via ProcessScoreData.
**Calls:** `EncryptRead16`, `EncryptRead_s`, `ProcessScoreData`, `malloc`, `free`, `CheckMem`

---

### HandleScoreListPacket
**Prototype:** `static void HandleScoreListPacket(FILE* fp)`
**Description:** Reads PT_SCORELIST packet containing dated score records and writes them to ipscores.dat for inter-BBS scoring.
**Calls:** `EncryptRead16`, `EncryptRead`, `EncryptRead_s`, `fopen`, `fclose`, `CheckedEncryptWrite`, `malloc`, `free`, `calloc`, `CheckMem`

---

### HandleClanMovePacket
**Prototype:** `static void HandleClanMovePacket(struct Packet *Packet, FILE* fp)`
**Description:** Reads a clan and its members from a PT_CLANMOVE packet; adds them to game via IBBS_AddToGame if game is active and sends PT_DATAOK acknowledgment.
**Calls:** `EncryptRead_s`, `LogDisplayStr`, `snprintf`, `malloc`, `CheckMem`, `IBBS_AddToGame`, `IBBS_SendPacket`, `FreeClanMembers`

---

### HandleNewUserPacket
**Prototype:** `static void HandleNewUserPacket(struct Packet *Packet, FILE* fp)`
**Description:** Processes PT_NEWUSER packet; if user is new, adds to league list and broadcasts PT_ADDUSER; if duplicate, sends PT_DELUSER.
**Calls:** `EncryptRead_s`, `IBBS_InList`, `IBBS_SendDelUser`, `UpdateNodesOnNewUser`, `AddToUList`

---

### HandleAddUserPacket
**Prototype:** `static void HandleAddUserPacket(FILE* fp)`
**Description:** Reads a PT_ADDUSER packet and adds the user to the local user list.
**Calls:** `EncryptRead_s`, `AddToUList`

---

### HandleDelUserPacket
**Prototype:** `static void HandleDelUserPacket(FILE* fp)`
**Description:** Reads a PT_DELUSER packet indicating a duplicate user and deletes the clan from local data.
**Calls:** `EncryptRead_s`, `snprintf`, `LogDisplayStr`, `DeleteClan`

---

### HandleSubUserPacket
**Prototype:** `static void HandleSubUserPacket(FILE* fp)`
**Description:** Processes PT_SUBUSER packet; removes a user being deleted from all boards via DeleteClan.
**Calls:** `EncryptRead_s`, `snprintf`, `LogDisplayStr`, `DeleteClan`

---

### HandleAttackPacket
**Prototype:** `static void HandleAttackPacket(FILE* fp)`
**Description:** Reads a PT_ATTACK packet and queues it for processing via ProcessAttackPacket.
**Calls:** `EncryptRead_s`, `ProcessAttackPacket`

---

### HandleAttackResultPacket
**Prototype:** `static void HandleAttackResultPacket(FILE* fp)`
**Description:** Reads a PT_ATTACKRESULT packet and processes it via ProcessResultPacket.
**Calls:** `EncryptRead_s`, `ProcessResultPacket`

---

### HandleSpyPacket
**Prototype:** `static void HandleSpyPacket(FILE* fp)`
**Description:** Reads a PT_SPY packet and processes it via IBBS_ProcessSpy.
**Calls:** `EncryptRead_s`, `IBBS_ProcessSpy`

---

### IBBS_HandleQueuedPackets
**Prototype:** `void IBBS_HandleQueuedPackets(void)`
**Description:** Processes pktqin.dat containing queued inbound packets; validates age and game ID, routes based on packet type, and deletes file on completion.
**Calls:** `fopen`, `fclose`, `unlink`, `notEncryptRead_s`, `fseek`, `DaysBetween`, `strcasecmp`, `CheckSourceID`, `LogStr`, `LogDisplayStr`, `HandleSpyPacket`, `HandleAttackPacket`

---

### HandleSpyResultPacket
**Prototype:** `static void HandleSpyResultPacket(FILE* fp)`
**Description:** Reads a PT_SPYRESULT packet and processes spy result data via IBBS_ProcessSpyResult.
**Calls:** `EncryptRead_s`, `IBBS_ProcessSpyResult`

---

### HandleNewNDXPacket
**Prototype:** `static void HandleNewNDXPacket(struct Packet* Packet, FILE* fp)`
**Description:** Reads a PT_NEWNDX packet containing a world.ndx file image and writes it to disk.
**Calls:** `malloc`, `CheckMem`, `EncryptRead`, `fopen`, `fclose`, `fwrite`, `free`

---

### HandleUserListPacket
**Prototype:** `static void HandleUserListPacket(struct Packet* Packet, FILE* fp)`
**Description:** Reads a PT_ULIST packet containing serialized userlist and writes it to userlist.dat.
**Calls:** `malloc`, `CheckMem`, `EncryptRead`, `fopen`, `fclose`, `fwrite`, `free`

---

### IBBS_ProcessPacket
**Prototype:** `static bool IBBS_ProcessPacket(char *szFileName, int16_t SrcID)`
**Description:** Opens an IBBS packet file, validates packets by age and game ID, routes non-local packets, and dispatches to appropriate handler; returns false on fatal errors.
**Calls:** `_fsopen`, `snprintf`, `LogDisplayStr`, `notEncryptRead_s`, `fseek`, `DaysBetween`, `strcasecmp`, `EncryptRead`, `free`, `malloc`, `CheckMem`, `IBBS_SendPacket`, `fclose`, `LogStr`, `EncryptRead16`, `HandleResetPacket`, `HandleMessagePacket`, `HandleScoreDataPacket`, `HandleScoreListPacket`, `HandleClanMovePacket`, `HandleNewUserPacket`, `HandleAddUserPacket`, `HandleDelUserPacket`, `HandleSubUserPacket`, `EnqueueInPacket`, `HandleAttackResultPacket`, `HandleSpyResultPacket`, `HandleNewNDXPacket`, `HandleUserListPacket`, `MoveToBad`

---

### MessageFileIterate
**Prototype:** `static void MessageFileIterate(const char *pszFileName, tIBInfo *InterBBSInfo, int16_t SourceID, bool(*skip)(void *), void(*process)(const char *, void *), void *cbdata)`
**Description:** Iterates over Fidonet message files in netmail directory, validates message headers and attachment filenames, and calls callback for matching files; supports strict source and message file validation.
**Calls:** `FilesOrderedByDate`, `System_Error`, `FreeFileList`, `fopen`, `fclose`, `fread`, `s_MessageHeader_d`, `ConvertStringToAddress`, `strcmp`, `SameFile`

---

### SkipAfterFound
**Prototype:** `static bool SkipAfterFound(void *cbdata)`
**Description:** Callback that returns whether a file was already found; used by MessageFileIterate to stop searching after first match.
**Calls:** None

---

### SetSkip
**Prototype:** `static void SetSkip(const char *fname, void *cbdata)`
**Description:** Callback that marks a file as found; used by CheckMessageFile to signal match.
**Calls:** None

---

### CheckMessageFile
**Prototype:** `static bool CheckMessageFile(const char *pszFileName, tIBInfo *InterBBSInfo, int16_t SourceID)`
**Description:** Validates that a packet file has a corresponding Fidonet message file in netmail directory with correct addressing; returns false if missing or invalid.
**Calls:** `MessageFileIterate`

---

### DeleteFound
**Prototype:** `static void DeleteFound(const char *fname, void *cbdata)`
**Description:** Callback that deletes a found message file from netmail directory.
**Calls:** `unlink`, `LogDisplayStr`, `snprintf`, `FileName`

---

### DeleteMessageWithFile
**Prototype:** `static void DeleteMessageWithFile(const char *pszFileName, tIBInfo *InterBBSInfo)`
**Description:** Locates and deletes the Fidonet message file attached to a packet file.
**Calls:** `MessageFileIterate`

---

### MoveToBad
**Prototype:** `static void MoveToBad(const char *szOldFilename)`
**Description:** Renames a file by appending .bad extension for quarantine; deletes any existing .bad file first.
**Calls:** `snprintf`, `unlink`, `rename`

---

### IBBS_PacketIn
**Prototype:** `void IBBS_PacketIn(void)`
**Description:** Main inbound packet processor: scans inbound directories for CLxxx*.IDy packet files, validates routing and message files, processes or quarantines them, and handles world.ndx updates.
**Calls:** `LogDisplayStr`, `strlcpy`, `snprintf`, `FilesOrderedByDate`, `System_Error`, `FreeFileList`, `atoi`, `CheckSourceID`, `IBBS_ProcessPacket`, `unlink`, `DeleteMessageWithFile`, `MoveToBad`, `_fsopen`, `fclose`, `rename`, `DisplayStr`, `FileName`

---

### IBBS_Init
**Prototype:** `void IBBS_Init(void)`
**Description:** Initializes IBBS subsystem by setting BBS ID from config, loading node list, processing routing rules, and reading persistent IBBS data.
**Calls:** `LogDisplayStr`, `delay`, `IBBS_LoadNDX`, `IBBS_ProcessRouteConfig`, `IBBS_Read`

---

### IBBS_Close
**Prototype:** `void IBBS_Close(void)`
**Description:** Closes IBBS by flushing all cached outbound packet files, sending them via Fidonet netmail with appropriate mail types (crash/hold), and updating output semaphore.
**Calls:** `strlcpy`, `snprintf`, `fclose`, `IBSendFileAttach`, `IBBS_Write`, `IBBS_Destroy`, `utime`, `fopen`

---

### IBBS_Maint
**Prototype:** `void IBBS_Maint(void)`
**Description:** Performs daily IBBS maintenance: processes inbound packets, increments packet counters, sends score data, broadcasts user list (LC only), and maintains clan travel state.
**Calls:** `LogDisplayStr`, `IBBS_PacketIn`, `CreateScoreData`, `SendScoreList`, `DaysSince1970`, `IBBS_SendUserList`, `IBBS_TravelMaint`, `IBBS_BackupMaint`

---
