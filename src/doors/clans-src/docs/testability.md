# Clans Source Code Testability Analysis

This document tracks which functions directly (D) or transitively (T) perform I/O or access global variables, for the purpose of understanding testability and identifying which functions need mocking for unit tests.

**Legend:**
- **D:** Direct call (the function itself directly invokes an operation listed below)
- **T:** Transitive call (the function reaches the operation only through a callee)

**Marking rules:** A function gets **D** only if its own body contains a call to
one of the listed primitives.  Calling a wrapper (e.g. `rputs` → `zputs` →
`fputs(stdout)`) is **T**, not D, because the function itself never invokes the
primitive.  Macros that expand to a function call (e.g. `EncryptRead_s` →
`EncryptRead()`) count as calling that function, not as inlining the primitive.
- **Disk Write:** fwrite, fputs(fp), fprintf(fp), fputc(fp), fclose, unlink, rename, link, _fsopen with write modes, od_log_write
- **Disk Read:** fread, fgets, feof, ftell, fseek, fopen with read modes, _fsopen with read modes
- **User Input:** od_get_key, od_get_answer, od_sleep, cio_getch, read(STDIN_FILENO,...)
- **User Output:** od_disp, od_disp_str, od_disp_emu, od_putch, od_set_colour, od_emulate, od_clr_scr, fputc(stdout), fputs(stdout), fprintf(stdout), printf, WriteConsole
- **my_random:** direct calls to my_random()
- **Read Global:** any read access to System, Config, Game, PClan, Village, IBBS, Language, Alliances[], NumAlliances, Alliances_Initialized, Quests[], Quests_TFlags[], Spells[], Spells_szCast*, Spells_CastValue, IniFile, PClasses[], Races[], BuildingType[], Door, serBuf[], erRet, ScreenWidth, ScreenLines, IsTTY, _argc, _argv
- **Write Global:** any write access to the same global variables
- **Non-C17:** Directly calls functions outside the C17 standard (POSIX, Win32 APIs,
  MSVC extensions like `_fsopen`).  Calling a linked C library (e.g. OpenDoors `od_*`)
  does not count — those are normal C function calls.

## tools.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| ato8 |  |  |  | D |  |  |  |  |
| atoc |  |  |  | D |  |  |  |  |
| ato16 |  |  |  | D |  |  |  |  |
| ato32 |  |  |  | D |  |  |  |  |

---

## random.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| my_random |  | T |  | T |  | T | T |  |

---

## semfile.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| CreateSemaphor | D | D |  |  |  |  |  |  |
| WaitSemaphor | T | T |  | T |  |  |  |  |
| RemoveSemaphor | D |  |  |  |  |  |  |  |

---

## misc.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| atoc | T | T |  | T |  | T | T |  |
| ato8 | T | T |  | T |  | T | T |  |
| atou8 | T | T |  | T |  | T | T |  |
| ato16 | T | T |  | T |  | T | T |  |
| atou16 | T | T |  | T |  | T | T |  |
| ato32 | T | T |  | T |  | T | T |  |
| atoul | T | T |  | T |  | T | T |  |
| atoull | T | T |  | T |  | T | T |  |

---

## parsing.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| ParseLine |  |  |  |  |  |  |  |  |
| GetToken |  |  |  |  |  |  |  |  |

---

## platform.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| strlcpy |  |  |  |  |  |  |  |  |
| strlcat |  |  |  |  |  |  |  |  |
| CRCValue |  |  |  |  |  |  |  |  |
| isRelative |  |  |  |  |  |  |  |  |
| SameFile |  |  |  |  |  |  |  |  |
| DirExists |  | D |  |  |  |  |  |  |
| Strip |  |  |  |  |  |  |  |  |
| iscodechar |  |  |  |  |  |  |  |  |
| RemovePipes |  |  |  |  |  |  |  |  |
| IsLeapYear |  |  |  |  |  |  |  |  |
| DaysSinceJan1 |  |  |  |  |  |  |  |  |
| DaysSince1970 |  |  |  |  |  |  |  |  |
| DaysBetween |  |  |  |  |  |  |  |  |

---

## language.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| LoadStrings |  |  |  |  |  | D |  |  |
| Language_Init | D | D |  | T |  | D | T |  |
| Language_Close |  |  |  |  |  | D | D |  |

---

## deserialize.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| s_ibbs_node_attack_d |  |  |  |  |  |  |  |  |
| s_MessageHeader_d |  |  |  |  |  |  |  |  |
| s_ibbs_node_reset_d |  |  |  |  |  |  |  |  |
| s_ibbs_node_recon_d |  |  |  |  |  |  |  |  |
| s_Msg_Txt_d |  |  |  |  |  |  |  |  |
| s_Message_d |  |  |  |  |  |  |  |  |
| s_Topic_d |  |  |  |  |  |  |  |  |
| s_LeavingData_d |  |  |  |  |  |  |  |  |
| s_PClass_d |  |  |  |  |  |  |  |  |
| s_SpellsInEffect_d |  |  |  |  |  |  |  |  |
| s_SpyAttemptPacket_d |  |  |  |  |  |  |  |  |
| s_SpyResultPacket_d |  |  |  |  |  |  |  |  |
| s_TradeData_d |  |  |  |  |  |  |  |  |
| s_TradeList_d |  |  |  |  |  |  |  |  |
| s_UserInfo_d |  |  |  |  |  |  |  |  |
| s_UserScore_d |  |  |  |  |  |  |  |  |
| s_Spell_d |  |  |  |  |  |  |  |  |
| s_pc_d |  |  |  |  |  |  |  |  |
| s_Packet_d |  |  |  |  |  |  |  |  |
| s_NPCInfo_d |  |  |  |  |  |  |  |  |
| s_NPCNdx_d |  |  |  |  |  |  |  |  |
| s_Language_d |  |  |  |  |  |  |  |  |
| s_Alliance_d |  |  |  |  |  |  |  |  |
| s_game_data_d |  |  |  |  |  |  |  |  |
| s_village_data_d |  |  |  |  |  |  |  |  |
| s_EventHeader_d |  |  |  |  |  |  |  |  |
| s_FileHeader_d |  |  |  |  |  |  |  |  |
| s_Army_d |  |  |  |  |  |  |  |  |
| s_Strategy_d |  |  |  |  |  |  |  |  |
| s_AttackPacket_d |  |  |  |  |  |  |  |  |
| s_AttackResult_d |  |  |  |  |  |  |  |  |
| s_item_data_d |  |  |  |  |  |  |  |  |
| s_empire_d |  |  |  |  |  |  |  |  |
| s_clan_d |  |  |  |  |  |  |  |  |

---

## items.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| GetOpenItemSlot |  |  |  |  |  | D |  |  |
| GetUnusedItem |  |  |  |  |  |  |  |  |
| GetUsedItem |  |  |  |  |  |  |  |  |
| GetEquippableItem |  |  |  |  |  |  |  |  |
| GetFirstItem |  |  |  |  |  |  |  |  |
| ChooseItem |  |  | T | T |  | D |  |  |
| UnequipItemsFromPC |  |  |  |  |  |  |  |  |
| Items_FindTreasureChest |  |  |  | T | D | D |  |  |
| ReadBook | T | T | T | T | T | T | T |  |
| ItemPenalty |  |  |  |  |  | D |  |  |
| ItemUseableBy |  |  |  | T |  | D |  |  |
| ShowItemStats |  |  |  | T |  | D |  |  |
| Items_Read | T | D |  | T |  | T | T |  |
| Items_Destroy |  |  |  |  |  |  | T |  |
| Items_GiveItemData |  |  |  |  |  | D |  |  |
| Items_GiveItem | T | T |  | T |  | T | T |  |
| Items_Init | T | T |  | T |  | T | T |  |
| Items_Close |  |  |  |  |  |  |  |  |
| Items_ReadScroll |  |  |  | T |  | D | D |  |
| Item_BuyItem | T | T | T | T | T | T | T |  |

---

## menus2.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| ResurrectDead | T | T | T | T |  | T | T |  |
| ReleaseMember | T |  | T | T |  | T | T |  |
| AddMember | T | T | T | T |  | T | T |  |
| TrainMember | T |  | T | T |  | T | T |  |

---

## myopen.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| cipher |  |  |  |  |  |  |  |  |
| MyOpen |  | D |  |  |  | D |  |  |
| EncryptWrite | D |  |  |  |  |  |  |  |
| EncryptRead |  | D |  |  |  |  |  |  |

---

## serialize.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| s_ibbs_node_attack_s |  |  |  |  |  |  |  |  |
| s_MessageHeader_s |  |  |  |  |  |  |  |  |
| s_ibbs_node_reset_s |  |  |  |  |  |  |  |  |
| s_ibbs_node_recon_s |  |  |  |  |  |  |  |  |
| s_Msg_Txt_s |  |  |  |  |  |  |  |  |
| s_Message_s |  |  |  |  |  |  |  |  |
| s_Topic_s |  |  |  |  |  |  |  |  |
| s_LeavingData_s |  |  |  |  |  |  |  |  |
| s_PClass_s |  |  |  |  |  |  |  |  |
| s_SpellsInEffect_s |  |  |  |  |  |  |  |  |
| s_SpyAttemptPacket_s |  |  |  |  |  |  |  |  |
| s_SpyResultPacket_s |  |  |  |  |  |  |  |  |
| s_TradeData_s |  |  |  |  |  |  |  |  |
| s_TradeList_s |  |  |  |  |  |  |  |  |
| s_UserInfo_s |  |  |  |  |  |  |  |  |
| s_UserScore_s |  |  |  |  |  |  |  |  |
| s_Spell_s |  |  |  |  |  |  |  |  |
| s_pc_s |  |  |  |  |  |  |  |  |
| s_Packet_s |  |  |  |  |  |  |  |  |
| s_NPCInfo_s |  |  |  |  |  |  |  |  |
| s_NPCNdx_s |  |  |  |  |  |  |  |  |
| s_Language_s |  |  |  |  |  |  |  |  |
| s_Alliance_s |  |  |  |  |  |  |  |  |
| s_game_data_s |  |  |  |  |  |  |  |  |
| s_village_data_s |  |  |  |  |  |  |  |  |
| s_EventHeader_s |  |  |  |  |  |  |  |  |
| s_FileHeader_s |  |  |  |  |  |  |  |  |
| s_Army_s |  |  |  |  |  |  |  |  |
| s_Strategy_s |  |  |  |  |  |  |  |  |
| s_AttackPacket_s |  |  |  |  |  |  |  |  |
| s_AttackResult_s |  |  |  |  |  |  |  |  |
| s_item_data_s |  |  |  |  |  |  |  |  |
| s_empire_s |  |  |  |  |  |  |  |  |
| s_clan_s |  |  |  |  |  |  |  |  |

---

## u8cp437.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| u8_to_cp437 |  |  |  |  |  |  |  |  |
| u8_fgets |  | D |  |  |  |  |  |  |
| u8_fopen |  | D |  | D |  |  |  |  |

---

## unix_wrappers.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| plat_DeleteFile | D |  |  |  |  |  |  | Y |
| plat_fsopen | D | D |  |  |  |  |  | Y |
| plat_Delay |  |  |  |  |  |  |  | Y |
| FileName |  |  |  |  |  |  |  |  |
| FreeFileList |  |  |  |  |  |  |  |  |
| FilesOrderedByDate |  | D |  |  |  |  |  | Y |
| fullpath |  | D |  |  |  |  |  | Y |
| plat_getftime |  | D |  |  |  |  |  | Y |
| plat_setftime | D |  |  |  |  |  |  | Y |

---

## win_wrappers.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| display_win32_error |  |  |  | D |  |  |  | Y |
| FileName |  |  |  |  |  |  |  |  |
| FreeFileList |  |  |  |  |  |  |  |  |
| FilesOrderedByDate |  | D |  |  |  |  |  | Y |
| plat_DeleteFile | D |  |  |  |  |  |  | Y |
| plat_Delay |  |  |  |  |  |  |  | Y |
| plat_getftime |  | D |  |  |  |  |  | Y |
| plat_setftime | D |  |  |  |  |  |  | Y |

---

## readcfg.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| AddInboundDir | T | T |  | T |  | D | D |  |
| Config_Init | T | D |  | T |  | D | D |  |
| Config_Close |  |  |  |  |  |  | D |  |

---

## input.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| Input_SetGetStringChoiceHook |  |  |  |  |  |  | D |  |
| Similar |  |  |  |  |  |  |  |  |
| InsideStr |  |  |  |  |  |  |  |  |
| ListChoices |  |  |  | T | T | D | D |  |
| GetStringChoice |  |  | T | T | T | D | D |  |
| GetStr | T | T | T | T | T | T | T |  |
| GetChoice | T | T | T | T | T | T | D |  |
| GetLong |  |  | T | T | T | D | D |  |

---

## help.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| Help | T | T | T | T | T | T | T |  |
| GeneralHelp | T | T | T | T | T | T | T |  |
| MainHelp | T | T | T | T | T | T | T |  |

---

## Summary

### Functions with no I/O or global access (pure functions)
These are safest to test and least likely to have side effects:
- tools.c: None (all call System_Error via printf)
- random.c: my_random (but calls System_Error)
- semfile.c: None (all perform disk I/O)
- misc.c: None (all call System_Error via printf)
- parsing.c: GetToken
- platform.c: strlcpy, strlcat, CRCValue, isRelative, SameFile, Strip, iscodechar, RemovePipes, IsLeapYear, DaysSinceJan1, DaysSince1970, DaysBetween
- language.c: None (all have side effects)

### Functions requiring mocking for testing
- **System_Error:** All ato* functions and my_random
- **Disk I/O (fopen, fread, fwrite, unlink, link, stat, fprintf):** CreateSemaphor, WaitSemaphor, RemoveSemaphor, Language_Init
- **User Output (printf, LogDisplayStr):** WaitSemaphor, Language_Init, all ato* functions
- **Global Language access:** LoadStrings, Language_Init, Language_Close

### Best candidates for unit tests
1. parsing.c::GetToken — pure string manipulation
2. platform.c functions (except DirExists) — pure math/string functions
3. random.c::my_random — isolated but needs System_Error mock

## news.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| News_AddNews |  |  |  | T |  | T | T |  |
| News_ReadNews | T | T | T | T | T | T | T |  |
| News_CreateTodayNews | T |  |  | T |  | T | T |  |

---

## maint.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| Maintenance | T | T | T | T | T | T | T |  |

---

## event.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| GoQuest | T | T | T | T | T | T | T |  |

---

## class.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| Load_PClasses | T | T |  | T |  | T | T |  |
| Free_PClasses |  |  |  |  |  |  | D |  |
| PClass_Init | T | T |  | T |  | T | T |  |
| PClass_Close |  |  |  |  |  | D | D |  |

---

## game.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| Game_Read |  | T |  |  |  |  | D |  |
| Game_Write | T |  |  |  |  | D |  |  |
| Game_Destroy |  |  |  |  |  |  | D |  |
| Game_Settings |  |  | T | T | T | T | T |  |
| Game_Start | T |  |  | T |  | T | T |  |
| Game_Init | T | T |  | T |  | T | T |  |
| Game_Close | T |  |  |  |  | T | T |  |

---

## clans.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| MainGame | T | T | T | T | T | T | T |  |
| main/WinMain | T | T | T | T | T | T | T |  |
| Disbanded | T | T |  | T |  | T | T |  |

---

## clansini.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| ClansIni_Init | T | T |  | T |  | T | T |  |
| ClansIni_Close |  |  |  |  |  | D | D |  |

---

## gum.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| output_bit | D |  |  |  |  |  |  |  |
| input_bit |  | D |  | D |  |  |  |  |
| output_code | D |  |  |  |  |  |  |  |
| input_code |  | D |  | D |  |  |  |  |
| flush_bits | D |  |  |  |  |  |  |  |
| initialize |  |  |  |  |  |  | D |  |
| update_freq |  |  |  |  |  |  | D |  |
| update_model |  |  |  |  |  |  | D |  |
| compress | D |  |  |  |  |  | D |  |
| uncompress |  | D |  | D |  |  | D |  |
| add_node |  |  |  |  |  |  | D |  |
| delete_node |  |  |  |  |  |  | D |  |
| match |  |  |  |  |  | D |  |  |
| dictionary |  |  |  |  |  | D | D |  |
| encode | D | D |  | D |  |  | D |  |
| decode | D | D |  | D |  |  | D |  |
| ClearAll |  |  |  |  |  |  | D |  |

---

## pawn.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| PS_Init | T | T |  | T |  | T | T |  |
| PS_Close | T |  |  |  |  |  | D |  |
| PS_List |  |  |  | D | T | T | T |  |
| PS_Buy | T | T | T | T | T | T | T |  |
| PS_Sell | T | T | T | T | T | T | T |  |
| PS_Maint | T | T |  | T |  | T | T |  |
| PawnShop | T | T | T | T | T | T | T |  |

---

## trades.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| GetTradeList | T | T | T | T | T | T | T |  |
| RejectTrade | T | T |  | T |  | T | T |  |
| Trades_CheckTrades | T | T | T | T | T | T | T |  |
| Trades_MakeTrade | T | T | T | T | T | T | T |  |
| Trades_Maint | T | T |  |  |  |  |  |  |

---

## reg.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| Jumble |  |  |  |  |  |  |  |  |
| IsRegged |  |  |  |  |  |  |  |  |
| Register | T | T | T | T |  | T | T |  |
| dputs |  |  |  | D |  | D |  |  |
| AddRegToCfg | D |  |  |  |  |  |  |  |
| UnregMessage |  |  |  | D | T | T | T |  |

---

## config.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| MailerTypeName |  |  |  |  |  |  |  |  |
| WriteCfg | T | T |  | T |  | T | T |  |
| ConfigMenu | T | T | T | T |  | T | T |  |
| NodeMenu |  |  | D | T |  | D | D |  |
| EditOption | T | T | T | T |  | T | T |  |
| EditNodeOption |  |  | D | T |  | D | D |  |
| setCurrentNode | T | T |  | T |  | T | T |  |
| UpdateOption |  |  |  | T |  | D | D |  |
| UpdateNodeOption |  |  |  | T |  | D | D |  |

---

## video.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| Video_VideoType |  |  |  |  |  | D |  |  |
| Video_SetScriptMode |  |  |  |  |  |  | D |  |
| Video_SetDosGetStrHook |  |  |  |  |  |  | D |  |
| Video_SetDosGetLongHook |  |  |  |  |  |  | D |  |
| ScrollUp |  |  |  | D |  | D |  |  |
| SetScrollRegion |  |  |  | D |  | D | D |  |
| ClearScrollRegion |  |  |  | D |  | D |  |  |
| zputs |  |  |  | D |  | D | D |  |
| qputs |  |  |  | D |  | D |  |  |
| Input |  |  | T | T |  | D | D |  |
| DosGetLong |  |  | T | T |  | D | D |  |
| DosGetStr |  |  | D | D |  | D | D |  |
| Video_Init |  |  |  | D |  | D | D |  |
| Video_Close |  |  |  | D |  | D | D |  |
| ClearArea |  |  |  | D |  | D |  |  |
| xputs |  |  |  | D |  | D |  |  |
| DisplayStr |  |  |  | T |  | D |  |  |
| ColorArea |  |  |  | D |  | D |  |  |
| ShowTextCursor |  |  |  | D |  | D |  |  |
| gotoxy |  |  |  | D |  | D | D |  |
| clrscr |  |  |  | D |  | D | D |  |
| textattr |  |  |  | D |  | D | D |  |
| clans_putch |  |  |  | D |  | D | D |  |
| cio_getch |  |  | D |  |  | D |  |  |
| getxy |  |  |  | D |  | D | D |  |
| save_screen |  |  |  |  |  | D |  |  |
| restore_screen |  |  |  | D |  | D | D |  |

---

## fight.c (lines 1–950)

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| Fight_Heal |  |  |  |  |  | D | D |  |
| Fight_GetBattleOrder |  | D | T | T | D | D | D |  |
| Fight_ManaRegenerate |  | D |  | T | D | D | D |  |
| Fight_IsIncapacitated |  |  |  | T | D | D | D |  |
| Fight_ChooseVictim |  | D | T | T | D | D | D |  |
| NumUndeadMembers |  |  |  |  |  | D |  |  |
| Fight_GetNPCAction |  | D |  | T | D | D | D |  |
| Fight_Stats |  |  |  | T | D | D | D |  |
| FirstAvailable |  |  |  |  |  | D |  |  |
| Fight_GetTarget | T | T | T | T | T | T | T |  |
| CanRun |  | D | T | T | D | D | D |  |
| Fight_Dead |  |  |  | T | D | D | D |  |
| Fight_BattleAttack |  | D | T | T | D | D | D |  |
| Fight_DoMove | T | T | T | T | T | T | T |  |
| GetTarget2 |  |  | T | T | D | D | D |  |
| Fight_ReadyScroll |  |  |  | T | D | D | D |  |

---

## fight.c (lines 951+)

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| Fight_Fight | T | T | T | T | T | T | T |  |
| Fight_CheckLevelUp |  |  | T | T | D | D | D |  |
| GetDifficulty |  |  |  |  | D |  |  |  |
| Fight_GetMonster |  | T |  | T |  | D | D |  |
| Fight_LoadMonsters |  | T |  | T | D | D | D |  |
| RemoveUndead |  |  |  |  |  |  | D |  |
| FreeClanMembers |  |  |  |  |  |  |  |  |
| FreeClan |  |  |  |  |  |  |  |  |
| MineFollowersGained |  |  |  |  | D |  |  |  |
| Fight_GiveFollowers |  |  |  | T | D | D | D |  |
| TakeItemsFromClan |  |  | T | T |  | D | D |  |
| Fight_Monster | T | T | T | T | T | T | T |  |
| InitClan |  |  |  |  |  |  |  |  |

---

## alliance.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| Alliances_Init | T | T |  | T |  | T | T |  |
| Alliances_Close | T |  |  |  |  |  | D |  |
| DeleteAlliance | T | T |  | T |  | T | T |  |
| KillAlliances | T | T |  | T |  | T | T |  |

---

## alliancem.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| RemoveFromAlliance | T | T | T | T | T | T | T |  |
| SeeMemberStats | T | T | T | T | T | T | T |  |
| ShowAllianceItems |  |  |  | T | T | T | T |  |
| DonationRoom | T | T | T | T | T | T | T |  |
| CreateAlliance | T | T | T | T | T | T | T |  |
| ShowAlliances |  |  | T | T | T | T | T |  |
| EnterAlliance | T | T | T | T | T | T | T |  |
| FormAlliance |  |  |  | T | T | T | T |  |
| KillAlliance | T | T |  | T | T | T | T |  |
| Alliance_Maint |  |  |  |  |  | D |  |  |

---

## voting.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| GetVotes |  | D |  |  |  | D |  |  |
| VotingBooth | T | T | T | T | T | T | T |  |
| ChooseNewLeader | T | T | T | T | T | T | T |  |

---

## pcedit.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| main | D | D | T | T |  | D |  |  |
| DeleteClan | D | D |  | T |  | D | D |  |
| InitVillage | D | D |  | T |  |  | D |  |
| UpdateVillage | D |  |  | T |  | D |  |  |
| RejectTrade | T | T |  | T |  |  |  |  |
| RemoveFromUList | D | D |  | T |  |  |  |  |
| ClanIDInList | D | D |  |  |  |  |  |  |
| RemoveFromIPScores | D | D |  | T |  |  |  |  |
| FreeClan |  |  |  |  |  |  |  |  |
| GetClan | D | D |  |  |  | D |  |  |
| UpdateClan | D | D |  | T |  | D |  |  |
| InitGame | D | D |  | T |  |  | D |  |

---

## menus.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| WorldMenu | T | T | T | T | T | T | T |  |
| AddChatFile | T | T |  |  |  |  |  |  |
| Menus_ChatRoom | T | T | T | T | T | T | T |  |
| MainMenu | T | T | T | T | T | T | T |  |
| MineMenu | T | T | T | T | T | T | T |  |
| CommunicationsMenu | T | T | T | T | T | T | T |  |
| WizardShop | T | T | T | T | T | T | T |  |
| MarketMenu | T | T | T | T | T | T | T |  |
| AlliancesMenu | T | T | T | T | T | T | T |  |
| ChurchMenu | T | T | T | T | T | T | T |  |
| THallMenu | T | T | T | T | T | T | T |  |
| GameLoop | T | T | T | T | T | T | T |  |

---

## menus2.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| ResurrectDead |  |  | T | T | T | T | T |  |
| ReleaseMember |  |  | T | T | T | T | T |  |
| AddMember | T | T | T | T | T | T | T |  |
| TrainMember | T | T | T | T | T | T | T |  |

---

## scores.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| GetColourString |  |  |  |  |  |  |  |  |
| PipeToAnsi |  |  |  |  |  |  |  |  |
| DisplayScores | T | T |  | T |  | T |  |  |
| SendScoreData |  |  |  |  |  |  | D |  |
| ProcessScoreData | T | T |  |  |  |  | D |  |
| LeagueScores |  | D |  | D |  | D |  |  |
| RemoveFromIPScores | T | T |  | T |  | T | T |  |
| SendScoreList |  | D |  |  |  | D |  |  |
| CreateScoreData |  | D |  |  |  | D | D |  |

---

## reset.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| main | T | T |  | T |  | T | T |  |
| ResetMenu | T |  | T | T |  | T | T |  |
| EditOption | T | T | T | T |  | T | T |  |
| DosHelp |  | D |  | D |  |  |  |  |
| GenerateGameID |  |  |  |  |  |  |  |  |
| GoReset | D |  |  | D |  | D | D |  |
| News_CreateTodayNews | D |  |  | D |  | D | D |  |
| News_AddNews |  |  |  | D |  | D | D |  |
| ClearFlags |  |  |  |  |  |  |  |  |
| InitEmpire |  |  |  |  |  |  |  |  |
| CreateVillageDat | D |  |  |  | D |  |  |  |
| UpdateOption |  |  |  | D |  | D | D |  |

---

## system.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| CheckMem | T | T |  | T |  | T | T |  |
| DupeStr | T | T |  | T |  | T | T |  |
| System_LockedOut |  | D |  |  |  |  |  |  |
| System_Error | T | T |  | T |  | T | T | Y |
| ShowHelp |  |  |  | D |  | D | D |  |
| ODCmdLineHandler | T | T |  | T |  | T | T |  |
| ODCmdLineFlagHandler | T | T | T | T | T | T | T |  |
| System_Maint | T |  |  | T |  | T | T |  |
| System_Close_AtExit | T | T |  | T |  | T | T |  |
| System_Close | T | T |  | T |  | T | T |  |
| System_Init | T | T | T | T | T | T | T |  |

---

## console.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| Console_SetScriptMode |  |  |  |  |  |  |  |  |
| Console_SetGetAnswerHook |  |  |  |  |  |  |  |  |
| Door_Initialized |  |  |  |  |  |  |  |  |
| rputs |  |  |  | T |  | T | T |  |
| GetKey |  |  | T |  |  |  |  |  |
| CheckMem |  |  |  | D |  |  |  |  |
| System_Error |  |  |  | T |  | T | T |  |
| LogDisplayStr |  |  |  | T |  | T | T |  |
| GetAnswer |  |  | T |  |  |  |  |  |
| yesNoQ |  |  | T | T |  | T | T |  |
| YesNo |  |  | T | T |  | T | T |  |
| NoYes |  |  | T | T |  | T | T |  |
| InputCallback |  |  |  |  |  |  |  |  |
| PutCh |  |  |  | T |  | T | T |  |
| rawputs |  |  |  | T |  | T | T |  |
| GetHoursLeft |  |  |  |  |  |  |  |  |
| GetMinutesLeft |  |  |  |  |  |  |  |  |
| door_pause |  |  | T | T |  | T | T |  |
| Door_AllowScreenPause |  |  |  |  |  |  |  |  |
| Door_ToggleScreenPause |  |  |  |  |  |  |  |  |
| GetKeyNoWait |  |  |  |  |  |  |  |  |

---

## door.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| Door_Initialized |  |  |  |  |  | D |  |  |
| Door_SetColorScheme |  |  |  |  |  |  | D |  |
| LocalLoad |  | D | D | T |  | D | D |  |
| BeforeChat |  |  |  | T |  |  |  |  |
| AfterChat |  |  |  | T |  |  |  |  |
| NoDoorFileHandler |  |  |  | T |  | D |  |  |
| NoStatus |  |  |  | T |  | D | D |  |
| TwoLiner |  |  |  | T |  | D | D |  |
| Door_ToggleScreenPause |  |  |  |  |  |  | D |  |
| Door_SetScreenPause |  |  |  |  |  |  | D |  |
| Door_AllowScreenPause |  |  |  |  |  | D |  |  |
| door_pause |  |  | D | D |  | D | D |  |
| rputs |  |  |  | D | T | D | D |  |
| LogToWhatever | D |  |  | D |  | D |  |  |
| GetKey |  |  | D |  |  |  |  |  |
| GetKeyNoWait |  |  | D |  |  |  |  |  |
| GetAnswer |  |  | D |  |  |  |  |  |
| LogStr | D |  |  |  |  |  |  |  |
| LogDisplayStr |  |  |  | T |  | D |  |  |
| Display |  | D | D | T |  | D | D |  |
| YesNo |  |  | D |  |  | D |  |  |
| NoYes |  |  | D |  |  | D |  |  |
| Door_ShowTitle |  |  | D | T | T | D | D |  |
| Door_Init | T | T | T | T |  | T | T |  |
| Door_Close | T |  |  |  |  | T | T |  |
| InputCallback |  |  |  |  |  |  |  |  |
| PutCh |  |  |  | D |  |  |  |  |
| rawputs |  |  |  | D |  |  |  |  |
| GetHoursLeft |  |  |  |  |  | D |  |  |
| GetMinutesLeft |  |  |  |  |  | D |  |  |

---

## mail.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| GenericMessage | D |  |  | D |  | D |  |  |
| MyWriteMessage2 | T | T | T | T | T | T | T |  |
| Mail_Read | T | T | T | T | T | T | T |  |
| Mail_Write |  |  | T | T |  | T | T |  |
| Mail_Maint | T | T |  | T |  | T |  |  |
| Mail_RequestAlliance | T | T | T | T | T | T | T |  |
| Mail_WriteToAllies | T | T | T | T | T | T | T |  |
| PostMsj | D |  |  |  |  | D | D |  |
| GlobalMsgPost | T | T | T | T | T | T | T |  |

---

## npc.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| NPC_GetNPCNdx |  | D |  |  |  |  |  |  |
| NPC_UpdateNPCNdx | D | D |  | T |  |  |  |  |
| NPC_Maint | D | D |  | T |  | T | T |  |
| NPC_ChatNPC | T | T | T | T |  | T | T |  |
| ChatVillagers | T | T | T | T |  | T | T |  |
| NPC_ResetNPCClan |  |  |  |  |  |  |  |  |
| NPC_GetNPC |  | D |  | T |  | T |  |  |
| NPC_AddNPCMember | T | T |  | T |  | T | T |  |

---

## spells.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| get_spell | T | T |  | T |  | T | T |  |
| Spells_Init | T | T |  | T |  | T | T |  |
| Spells_Close |  |  |  |  |  |  | D |  |
| Spells_UpdatePCSpells |  |  |  | D | T | D | D |  |
| Spells_ClearSpells |  |  |  |  |  | D | D |  |
| Spells_CastSpell | T | T | T | T | T | T | T |  |

---

## chew.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| main | T | T |  | T |  | T | T |  |
| AddGUM | T | T |  | T |  |  | D |  |
| AddDir | D |  |  | D |  |  |  |  |
| _dos_getftime |  |  |  |  |  |  |  |  |

---

## ibbs.c (lines 1–1778)

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| IBBS_SendFileInPacket | D | D |  | T |  | D | D |  |
| IBBS_SendUserList | T | T |  | T |  | T | T |  |
| IBBS_DistributeNDX | D | D |  | T |  | T | T |  |
| LeagueKillUser | T | D |  | T |  | T | T |  |
| IBBS_ProcessSpy | T | T |  | T | D | T | T |  |
| IBBS_ProcessSpyResult | T |  |  | T |  | T |  |  |
| IBBS_SendQueuedResults | D | D |  | T |  | T | T |  |
| EnqueueInPacket | D | D |  |  |  | T |  |  |
| IBBS_SendDelUser | T | D |  | T |  | T | T |  |
| ClanIDInList |  | D |  |  |  | D |  |  |
| RemoveFromUList | D | D |  | T |  | T |  |  |
| AddToUList | D | T |  |  |  | T |  |  |
| UpdateNodesOnNewUser | T | D |  | T |  | T | T |  |
| IBBS_LeagueNewUser | T | T |  | T |  | T | T |  |
| IBBS_InList |  | D |  |  |  | D |  |  |
| IBBS_SendComeBack | T | D |  | T |  | T | T |  |
| VillageName |  |  |  |  |  | D |  |  |
| BBSName |  |  |  |  |  | D |  |  |
| ReturnLostAttack | T | T |  | T |  | T | T |  |
| IBBS_BackupMaint | D | D |  | T |  | T | T |  |
| AbortTrip | D | D |  | T | D | T | T |  |
| IBBS_CurrentTravelInfo | T | T | T | T | D | T | T |  |
| IBBS_UpdateLeavingClan | T | D |  | T |  | T | T |  |
| IBBS_TravelMaint | T | D |  | T |  | T | T |  |
| GetTroopsTraveling |  |  | T | T | T | T | T |  |
| IBBS_TravelToBBS | D | D | T | T | T | T | T |  |
| IBBS_SeeVillages | T | T | T | T | T | T | T |  |
| IBBS_AddLCLog | D | D |  |  |  | D |  |  |
| IBBS_Destroy |  |  |  |  |  | D | D |  |
| IBBS_LoginStats |  |  |  | T |  | T | T |  |
| IBBS_Create |  |  |  |  |  |  | D |  |
| IBBS_Read |  | D |  |  |  | D | T |  |
| IBBS_Write | D |  |  |  |  | T |  |  |
| FindPoint |  |  |  |  |  |  | D |  |
| IBBS_LoadNDX |  | D |  | T |  | T | T |  |
| IBBS_ProcessRouteConfig |  | D |  | T |  | T | T |  |

---

## village.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| OutsiderTownHallMenu |  |  | T | T |  | T | T |  |
| ChangeFlagScheme |  |  | T | T |  | T | T |  |
| GetNums |  |  |  |  |  | D | D |  |
| LoadSchemes |  | D |  | T |  | T | T |  |
| AddScheme | D |  |  | T |  | D | D |  |
| ChangeColourScheme | T | T | T | T |  | T | T |  |
| BuildMenu |  |  | T | T |  | T | T |  |
| EconomicsMenu |  |  | T | T |  | T | T |  |
| ToggleEmpireStats |  |  |  | T | D | T | T |  |
| TownHallMenu |  |  | T | T |  | T | T |  |
| Village_NewRuler |  |  |  | T | D | T | T |  |
| Village_Destroy |  |  |  |  |  | D | D |  |
| Village_Read |  | D |  |  |  | D | T |  |
| Village_Write | D |  |  |  |  | T |  |  |
| Village_Reset |  |  |  |  | D | D | T |  |
| Village_Maint |  |  |  | T | D | T | T |  |
| Village_Init |  | T |  | T |  | T | T |  |
| Village_Close |  | T |  |  |  | D | D |  |

---

## myibbs.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| ConvertStringToAddress |  |  |  |  |  |  |  |  |
| WriteMessage | D | D |  |  |  |  |  |  |
| IBSendFileAttach | T |  |  |  |  | D |  |  |

---

## quests.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| Quests_Init |  | D |  | T |  | T | T |  |
| Quests_Close |  |  |  |  |  | D | D |  |
| ClearFlags |  |  |  |  |  |  |  |  |
| SetFlag |  |  |  |  |  |  |  |  |
| ClearFlag |  |  |  |  |  |  |  |  |
| FlagSet |  |  |  |  |  |  |  |  |
| legal |  |  |  |  | D | D | D |  |
| JumpToEvent |  | D |  | T |  | D | D |  |
| TellQuest |  |  |  | T |  | D | D |  |
| RunEvent | T | D | T | T | D | T | T |  |
| Quests_GoQuest |  |  | T | T | T | T | T |  |

---

## user.c (lines 1–1320)

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| PadString |  |  |  |  |  |  |  |  |
| AddToDisband | D |  |  |  |  | D |  |  |
| User_ResetAllVotes | D | D |  | T |  | D | D |  |
| DeleteClan | D | D |  | T |  | T | T |  |
| ClanExists |  | D |  |  |  | D |  |  |
| GetStat |  |  |  |  |  | D |  |  |
| ShowBaseStats |  |  |  | T |  | D |  |  |
| GetClass |  |  | T | T |  | D |  |  |
| ChooseDefaultAction |  |  | T | T |  | D |  |  |
| ShowPlayerStats |  |  | T | T |  | D | D |  |
| ListItems |  |  |  | T |  | D |  |  |
| ItemEquipResults |  |  |  | T |  | D |  |  |
| ItemStats |  |  | T | T |  | D | D |  |
| NumClansInVillage |  | D |  |  |  | D |  |  |
| ShowVillageStats |  | T | T | T |  | D | D |  |
| ClanStats | T | T | T | T |  | T | T |  |

---

## user.c (lines 1321–2642)

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| PC_Create |  |  | T | T | D | T | T |  |
| NameInUse |  | D |  |  |  | D |  |  |
| ChooseClanName |  | T | T | T |  | T | T |  |
| User_ResetHelp |  |  |  |  |  | D | D |  |
| User_Create | D |  | T | T |  | T | T |  |
| User_Destroy |  |  |  |  |  | D | D |  |
| User_Read |  | D |  |  |  | D | T |  |
| Clan_Update | D | D |  | T |  | T |  |  |
| User_Write | T | T |  | T |  | T |  |  |
| User_FirstTimeToday |  |  |  |  |  | D | D |  |
| NumMembers |  |  |  |  |  | D |  |  |
| GetClanID |  | D | T | T |  | T | T |  |
| GetClanNameID |  | D |  |  |  |  |  |  |
| GetClan |  | D |  | T |  | T | T |  |
| User_List |  | D |  | T |  | D |  |  |
| User_Maint | D | D |  | T |  | T | T |  |
| User_Init | T | T | T | T | T | T | T |  |
| User_Close | T |  |  |  |  | D | D |  |

---

## empire.c (lines 1–1575)

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| SendResultPacket | T |  |  |  |  | T | T |  |
| ValidateIndex |  |  |  | T |  | D | D |  |
| ProcessAttackPacket | T | T |  | T |  | T | T |  |
| ProcessResultPacket | T | T |  | T |  | T | T |  |
| Empire_Create |  |  |  |  |  |  |  |  |
| Empire_Maint |  |  |  | T | D | D | D |  |
| ArmySpeed |  |  |  |  |  |  |  |  |
| ArmyOffense |  |  |  |  |  |  |  |  |
| ArmyDefense |  |  |  |  |  |  |  |  |
| ArmyVitality |  |  |  |  |  |  |  |  |
| Empire_Stats |  |  | T | T |  | D | D |  |
| DevelopLand |  |  | T | T |  | T | T |  |
| DonateToEmpire |  |  | T | T |  | T | T |  |
| Destroy_Menu |  |  | T | T |  | T | T |  |
| StructureMenu |  |  | T | T |  | T | T |  |
| ManageArmy |  |  | T | T |  | T | T |  |
| ArmyAttack |  |  |  |  |  |  |  |  |
| EmpireAttack |  |  |  |  | T |  |  |  |
| ProcessAttackResult |  |  |  |  |  |  |  |  |

---

## empire.c (lines 1576–3149)

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| DestroyBuildings |  |  |  |  | D | T | T |  |
| ShowResults |  |  |  | D |  | T | T |  |
| GetNumTroops |  |  | T | D |  | T | T |  |
| StartEmpireWar |  |  | T | T |  | T | T |  |
| SpyMenu |  |  | T | T | D | T | T |  |
| Empire_Manage |  |  | T | T |  | T | T |  |

---

## mcomp.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| main | D | D |  | D |  | T | T |  |

---

## mclass.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| main | D | T |  | D |  | T | T |  |
| Deinit_PClasses |  |  |  |  |  |  |  |  |
| Init_PClasses |  | D |  | D |  |  |  |  |

---

## mitems.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| main | D | T |  | D |  | T | T |  |
| Deinit_Items |  |  |  |  |  |  |  |  |
| Init_Items |  | D |  | D |  | T | T |  |

---

## mspells.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| main | D | T |  | D |  | T | T |  |
| Deinit_Spells |  |  |  |  |  |  |  |  |
| Init_Spells |  | D |  | D |  | T | T |  |

---

## makenpc.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| main | T | T |  | D |  | T | T |  |
| Init_NPCs | D | D |  | D |  |  |  |  |

---

## makepak.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| main | D | D |  | D |  | T | T |  |
| AddToPak | D | D |  | D |  |  |  |  |

---

## qtest.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| Display |  |  |  | D |  | D | D |  |
| LogStr |  |  |  | D |  |  |  |  |
| DupeStr |  |  |  |  |  |  |  |  |
| script_read_line |  | D |  |  |  |  |  |  |
| script_consume |  | T |  | D |  |  |  |  |
| script_expect_end |  | T |  | D |  |  |  |  |
| hook_get_answer |  | T |  | D |  |  |  |  |
| hook_dos_get_str |  | T |  |  |  |  |  |  |
| hook_dos_get_long |  | T |  |  |  |  |  |  |
| hook_get_string_choice |  | T |  | D |  |  |  |  |
| Fight_Fight |  | T | T | D |  | T | T |  |
| Fight_CheckLevelUp |  |  |  |  |  |  |  |  |
| FreeClan |  |  |  |  |  |  |  |  |
| FreeClanMembers |  |  |  |  |  |  |  |  |
| News_AddNews |  |  |  | D |  | D | D |  |
| Items_GiveItem |  |  |  | D |  | D | D |  |
| NumMembers |  |  |  |  |  | D |  |  |
| GetClanNameID |  |  |  |  |  |  |  |  |
| GetClan |  |  |  |  |  | D | D |  |
| ClanStats |  |  |  | D |  | D | D |  |
| GetStat |  |  |  |  |  | D |  |  |
| my_random |  |  |  | D | D | D | D |  |
| setup_dummy_pclan |  | T |  |  | T | D | D |  |
| setup_dummy_village |  |  |  |  |  |  | D |  |
| print_flags |  |  |  | D |  | D | D |  |
| print_quest_flags |  |  |  | D |  | D | D |  |
| count_active_quests |  |  |  |  |  | D |  |  |
| print_flag_summary |  |  |  | D |  | D |  |  |
| print_quest_summary |  |  |  | D |  | D |  |  |
| print_state_summary |  |  |  | D |  | D |  |  |
| parse_flag_list |  |  |  |  |  |  |  |  |
| parse_quest_list |  |  |  |  |  |  |  |  |
| show_main_screen |  |  | T | D |  | D | D |  |
| set_or_clear_flag |  |  | T | D |  | D | D |  |
| edit_flags_menu |  |  | T | D |  | D | D |  |
| edit_gold_mine |  |  | T | D |  | D | D |  |
| collect_npcs |  |  |  | D |  | D | D |  |
| wander_name |  |  |  |  |  |  |  |  |
| npc_submenu |  |  | T | D |  | D | D |  |
| quest_submenu |  | T | T | D | T | D | D |  |
| dbg_init | T | T |  | D |  | T | T |  |
| dbg_close |  |  |  |  |  |  |  |  |
| main | D | D |  | D |  | D | D |  |

---

## langcomp.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| main | D | D |  | D |  | T | T |  |
| Convert |  |  |  |  |  |  |  |  |

---

## ecomp.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| main | D | D |  | D |  | T | T |  |

---

## install.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| Display |  | D | T | T |  | T | T |  |
| detect_utf8_marker |  | D |  |  |  |  |  |  |
| SystemInit | T | T |  | T |  | T | T |  |
| main | D | D | T | T |  | T | T |  |
| ReadFilename |  | D |  |  |  |  |  |  |
| fseekuc |  | D |  |  |  |  |  |  |
| GetGUM | D | D |  | T |  | T | T |  |
| install | D | D | T | T |  | T | T |  |
| upgrade | D | D | T | T |  | T | T |  |
| FileExists |  | D |  |  |  |  |  |  |
| FreeFiles |  |  |  |  |  |  |  |  |
| InitFiles |  | D |  | T |  | T | T |  |
| Match |  |  |  |  |  |  |  |  |
| WriteType |  | D | T | T |  | D | D |  |
| Extract | D | D | T | T |  | T | T |  |
| CheckRow |  |  | T | T |  | D | D |  |
| AddLFN |  |  |  |  |  |  | D |  |
| FreeLFNs |  |  |  |  |  |  |  |  |
| DisplayLFNs |  |  |  | T |  | D | D |  |
| ListFiles |  | D | T | T |  | T | T |  |
| GetGumName |  | D | T | T |  | T | T |  |
| _dos_setftime | D |  |  |  |  |  |  |  |

---

## langcomp.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| main | D | D |  | D |  | D | D |  |
| Convert |  |  |  |  |  |  |  |  |

---

## ecomp.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| main | D | D |  | D |  |  |  |  |

---

## ibbs.c (lines 1779–3557)

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global | Non-C17 |
|---|---|---|---|---|---|---|---|---|
| getPacketFP | D |  |  |  |  | D | T |  |
| IBBS_SendPacketBuffer | T |  |  | T |  | T | T |  |
| IBBS_SendSpy | T |  |  | T |  | T | T |  |
| IBBS_SendAttackPacket | T |  |  |  |  | T | T |  |
| IBBS_ShowLeagueAscii |  | D |  | T |  | T | T |  |
| IBBS_SendPacket | D |  |  | T |  | T | T |  |
| IBBS_EnqueueOutPacket | T |  |  |  |  | T | T |  |
| IBBS_SendRecon | T |  |  |  |  | T | T |  |
| IBBS_SendReset | T |  |  |  |  | T | T |  |
| IBBS_UpdateRecon | T |  |  |  |  | T | T |  |
| IBBS_UpdateReset | T |  |  |  |  | T | T |  |
| IBBS_LeagueInfo |  |  |  | T | T | T | T |  |
| Reset | T | T | T | T | T | T | T |  |
| IBBS_Reset | T | T | T | T | T | T | T |  |
| IBBS_RemoveFromBackup | T | T |  | T |  | T | T |  |
| IBBS_AddToGame | T | T |  | T |  | T | T |  |
| ComeBack | T | T |  | T |  | T | T |  |
| CheckID |  |  |  | D |  | D |  |  |
| CheckSourceID |  |  |  | D |  | D |  |  |
| CheckDestinationID |  |  |  | D |  | D |  |  |
| HandleResetPacket | T | T | T | T | T | T | T |  |
| HandleMessagePacket | T | T |  | T |  | T | T |  |
| HandleScoreDataPacket | T | T |  | T |  | T | T |  |
| HandleScoreListPacket | T | T |  | T |  | T | T |  |
| HandleClanMovePacket | T | T |  | T |  | T | T |  |
| HandleNewUserPacket | T | T |  | T |  | T | T |  |
| HandleAddUserPacket | T | T |  |  |  | T |  |  |
| HandleDelUserPacket | T | T |  | T |  | T | T |  |
| HandleSubUserPacket | T | T |  | T |  | T | T |  |
| HandleAttackPacket |  | D |  |  |  |  |  |  |
| HandleAttackResultPacket |  | D |  |  |  |  |  |  |
| HandleSpyPacket | T | T | T | T | T | T | T |  |
| IBBS_HandleQueuedPackets | T | T | T | T | T | T | T |  |
| HandleSpyResultPacket | T | T |  | T |  | T |  |  |
| HandleNewNDXPacket | T | T |  | T |  | T | T |  |
| HandleUserListPacket | T | T |  | T |  | T | T |  |
| IBBS_ProcessPacket | T | T | T | T | T | T | T |  |
| MessageFileIterate | T | T |  | T |  | T | T |  |
| SkipAfterFound |  |  |  |  |  | D |  |  |
| SetSkip |  |  |  |  |  |  | D |  |
| CheckMessageFile | T | T |  | T |  | T | T |  |
| DeleteFound | D |  |  | T |  | D |  |  |
| DeleteMessageWithFile | T | T |  | T |  | T | T |  |
| MoveToBad | D |  |  |  |  |  |  |  |
| IBBS_PacketIn | D | D |  | T |  | T | T |  |
| IBBS_Init | T | T |  | T |  | T | T |  |
| IBBS_Close | T |  |  |  |  | T | T |  |
| IBBS_Maint | T | T |  | T |  | T | T |  |

---
