# Clans Source Code Testability Analysis

This document tracks which functions directly perform I/O or access global variables, for the purpose of understanding testability and identifying which functions need mocking for unit tests.

**Legend:**
- **Disk Write:** fwrite, fputs(fp), fprintf(fp), fputc(fp), fclose, unlink, rename, link, _fsopen with write modes, od_log_write
- **Disk Read:** fread, fgets, feof, ftell, fseek, fopen with read modes, _fsopen with read modes
- **User Input:** od_get_key, od_get_answer, od_sleep, cio_getch, read(STDIN_FILENO,...)
- **User Output:** od_disp, od_disp_str, od_disp_emu, od_putch, od_set_colour, od_emulate, od_clr_scr, fputc(stdout), fputs(stdout), fprintf(stdout), printf, WriteConsole
- **my_random:** direct calls to my_random()
- **Read Global:** any read access to System, Config, Game, PClan, Village, IBBS, Language, Alliances[], NumAlliances, Alliances_Initialized, Quests[], Quests_TFlags[], Spells[], Spells_szCast*, Spells_CastValue, IniFile, PClasses[], Races[], BuildingType[], Door, serBuf[], erRet, ScreenWidth, ScreenLines, IsTTY, _argc, _argv
- **Write Global:** any write access to the same global variables

## tools.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| ato8 | Y | Y |  | Y |  | Y | Y |
| atoc | Y | Y |  | Y |  | Y | Y |
| ato16 | Y | Y |  | Y |  | Y | Y |
| ato32 | Y | Y |  | Y |  | Y | Y |

---

## random.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| my_random |  | Y | Y | Y | Y | Y | Y |

---

## semfile.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| CreateSemaphor | Y | Y |  |  |  |  |  |
| WaitSemaphor | Y | Y |  | Y |  | Y |  |
| RemoveSemaphor | Y |  |  |  |  |  |  |

---

## misc.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| atoc | Y | Y |  | Y |  | Y | Y |
| ato8 | Y | Y |  | Y |  | Y | Y |
| atou8 | Y | Y |  | Y |  | Y | Y |
| ato16 | Y | Y |  | Y |  | Y | Y |
| atou16 | Y | Y |  | Y |  | Y | Y |
| ato32 | Y | Y |  | Y |  | Y | Y |
| atoul | Y | Y |  | Y |  | Y | Y |
| atoull | Y | Y |  | Y |  | Y | Y |

---

## parsing.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| ParseLine |  |  |  |  |  |  |  |
| GetToken |  |  |  |  |  |  |  |

---

## platform.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| strlcpy |  |  |  |  |  |  |  |
| strlcat |  |  |  |  |  |  |  |
| CRCValue |  |  |  |  |  |  |  |
| isRelative |  |  |  |  |  |  |  |
| SameFile |  |  |  |  |  |  |  |
| DirExists | Y |  |  |  |  |  |  |
| Strip |  |  |  |  |  |  |  |
| iscodechar |  |  |  |  |  |  |  |
| RemovePipes |  |  |  |  |  |  |  |
| IsLeapYear |  |  |  |  |  |  |  |
| DaysSinceJan1 |  |  |  |  |  |  |  |
| DaysSince1970 |  |  |  |  |  |  |  |
| DaysBetween |  |  |  |  |  |  |  |

---

## language.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| LoadStrings |  |  |  |  |  | Y |  |
| Language_Init | Y | Y |  | Y |  | Y | Y |
| Language_Close |  |  |  |  |  | Y | Y |

---

## u8cp437.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| u8_to_cp437 |  |  |  | Y |  |  |  |
| u8_fgets |  | Y |  | Y |  |  |  |
| u8_fopen |  | Y |  | Y |  |  |  |

---

## unix_wrappers.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| _fsopen | Y | Y |  |  |  |  |  |
| delay |  |  |  |  |  |  |  |
| FileName |  |  |  |  |  |  |  |
| FreeFileList |  |  |  |  |  |  |  |
| FilesOrderedByDate |  | Y |  |  |  |  |  |
| fullpath |  |  |  |  |  |  |  |

---

## win_wrappers.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| display_win32_error |  |  |  | Y |  |  |  |
| FileName |  |  |  |  |  |  |  |
| FreeFileList |  |  |  |  |  |  |  |
| FilesOrderedByDate |  | Y |  |  |  |  |  |

---

## readcfg.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| AddInboundDir | Y | Y |  | Y |  | Y | Y |
| Config_Init | Y | Y |  | Y |  | Y | Y |
| Config_Close |  |  |  |  |  |  | Y |

---

## input.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| Input_SetGetStringChoiceHook |  |  |  |  |  |  | Y |
| Similar |  |  |  |  |  |  |  |
| InsideStr |  |  |  |  |  |  |  |
| ListChoices |  |  |  | Y | Y | Y | Y |
| GetStringChoice |  |  | Y | Y | Y | Y | Y |
| GetStr | Y | Y | Y | Y | Y | Y | Y |
| GetChoice | Y | Y | Y | Y | Y | Y | Y |
| GetLong |  |  | Y | Y | Y | Y | Y |

---

## help.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| Help | Y | Y | Y | Y | Y | Y | Y |
| GeneralHelp | Y | Y | Y | Y | Y | Y | Y |
| MainHelp | Y | Y | Y | Y | Y | Y | Y |

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

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| News_AddNews |  |  |  | Y |  | Y | Y |
| News_ReadNews | Y | Y | Y | Y | Y | Y | Y |
| News_CreateTodayNews | Y |  |  | Y |  | Y | Y |

---

## maint.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| Maintenance | Y | Y | Y | Y | Y | Y | Y |

---

## event.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| GoQuest | Y | Y | Y | Y | Y | Y | Y |

---

## class.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| Load_PClasses | Y | Y |  | Y |  | Y | Y |
| Free_PClasses |  |  |  |  |  |  | Y |
| PClass_Init | Y | Y |  | Y |  | Y | Y |
| PClass_Close |  |  |  |  |  | Y | Y |

---

## game.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| Game_Read |  | Y |  |  |  |  | Y |
| Game_Write | Y |  |  |  |  | Y |  |
| Game_Destroy |  |  |  |  |  |  | Y |
| Game_Settings |  |  | Y | Y | Y | Y | Y |
| Game_Start | Y |  |  | Y |  | Y | Y |
| Game_Init | Y | Y |  | Y |  | Y | Y |
| Game_Close | Y |  |  |  |  | Y | Y |

---

## clans.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| MainGame | Y | Y | Y | Y | Y | Y | Y |
| main/WinMain | Y | Y | Y | Y | Y | Y | Y |
| Disbanded | Y | Y |  | Y |  | Y | Y |

---

## clansini.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| ClansIni_Init | Y | Y |  | Y |  | Y | Y |
| ClansIni_Close |  |  |  |  |  | Y | Y |

---

## gum.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| output_bit | Y |  |  |  |  |  |  |
| input_bit |  | Y |  | Y |  |  |  |
| output_code | Y |  |  |  |  |  |  |
| input_code |  | Y |  | Y |  |  |  |
| flush_bits | Y |  |  |  |  |  |  |
| initialize |  |  |  |  |  |  | Y |
| update_freq |  |  |  |  |  |  | Y |
| update_model |  |  |  |  |  |  | Y |
| compress | Y |  |  |  |  |  | Y |
| uncompress |  | Y |  | Y |  |  | Y |
| add_node |  |  |  |  |  |  | Y |
| delete_node |  |  |  |  |  |  | Y |
| match |  |  |  |  |  | Y |  |
| dictionary |  |  |  |  |  | Y | Y |
| encode | Y | Y |  | Y |  |  | Y |
| decode | Y | Y |  | Y |  |  | Y |
| ClearAll |  |  |  |  |  |  | Y |

---

## pawn.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| PS_Init | Y | Y |  | Y |  | Y | Y |
| PS_Close | Y |  |  |  |  |  | Y |
| PS_List |  |  |  | Y | Y | Y | Y |
| PS_Buy | Y | Y | Y | Y | Y | Y | Y |
| PS_Sell | Y | Y | Y | Y | Y | Y | Y |
| PS_Maint | Y | Y |  | Y |  | Y | Y |
| PawnShop | Y | Y | Y | Y | Y | Y | Y |

---

## trades.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| GetTradeList | Y | Y | Y | Y | Y | Y | Y |
| RejectTrade | Y | Y |  | Y |  | Y | Y |
| Trades_CheckTrades | Y | Y | Y | Y | Y | Y | Y |
| Trades_MakeTrade | Y | Y | Y | Y | Y | Y | Y |
| Trades_Maint | Y | Y |  |  |  |  |  |

---

## reg.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| Jumble |  |  |  |  |  |  |  |
| IsRegged |  |  |  |  |  |  |  |
| Register | Y | Y | Y | Y |  | Y | Y |
| dputs |  |  |  | Y |  | Y |  |
| AddRegToCfg | Y |  |  |  |  |  |  |
| UnregMessage |  |  |  | Y | Y | Y | Y |

---

## config.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| MailerTypeName |  |  |  |  |  |  |  |
| WriteCfg | Y | Y |  | Y |  | Y | Y |
| ConfigMenu | Y | Y | Y | Y |  | Y | Y |
| NodeMenu |  |  | Y | Y |  | Y | Y |
| EditOption | Y | Y | Y | Y |  | Y | Y |
| EditNodeOption |  |  | Y | Y |  | Y | Y |
| setCurrentNode | Y | Y |  | Y |  | Y | Y |
| UpdateOption |  |  |  | Y |  | Y | Y |
| UpdateNodeOption |  |  |  | Y |  | Y | Y |

---

## video.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| Video_VideoType |  |  |  |  |  | Y |  |
| Video_SetScriptMode |  |  |  |  |  |  | Y |
| Video_SetDosGetStrHook |  |  |  |  |  |  | Y |
| Video_SetDosGetLongHook |  |  |  |  |  |  | Y |
| ScrollUp |  |  |  | Y |  | Y |  |
| SetScrollRegion |  |  |  | Y |  | Y | Y |
| ClearScrollRegion |  |  |  | Y |  | Y |  |
| zputs |  |  |  | Y |  | Y | Y |
| qputs |  |  |  | Y |  | Y |  |
| Input |  |  | Y | Y |  | Y | Y |
| DosGetLong |  |  | Y | Y |  | Y | Y |
| DosGetStr |  |  | Y | Y |  | Y | Y |
| Video_Init |  |  |  | Y |  | Y | Y |
| Video_Close |  |  |  | Y |  | Y | Y |
| ClearArea |  |  |  | Y |  | Y |  |
| xputs |  |  |  | Y |  | Y |  |
| DisplayStr |  |  |  | Y |  | Y |  |
| ColorArea |  |  |  | Y |  | Y |  |
| ShowTextCursor |  |  |  | Y |  | Y |  |
| gotoxy |  |  |  | Y |  | Y | Y |
| clrscr |  |  |  | Y |  | Y | Y |
| textattr |  |  |  | Y |  | Y | Y |
| clans_putch |  |  |  | Y |  | Y | Y |
| cio_getch |  |  | Y |  |  | Y |  |
| getxy |  |  |  | Y |  | Y | Y |
| save_screen |  |  |  |  |  | Y |  |
| restore_screen |  |  |  | Y |  | Y | Y |

---

## fight.c (lines 1–950)

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| Fight_Heal |  |  |  |  |  | Y | Y |
| Fight_GetBattleOrder |  | Y | Y | Y | Y | Y | Y |
| Fight_ManaRegenerate |  | Y | Y | Y | Y | Y | Y |
| Fight_IsIncapacitated |  |  |  | Y | Y | Y | Y |
| Fight_ChooseVictim |  | Y | Y | Y | Y | Y | Y |
| NumUndeadMembers |  |  |  |  |  | Y |  |
| Fight_GetNPCAction |  | Y | Y | Y | Y | Y | Y |
| Fight_Stats |  |  |  | Y | Y | Y | Y |
| FirstAvailable |  |  |  |  |  | Y |  |
| Fight_GetTarget | Y | Y | Y | Y | Y | Y | Y |
| CanRun |  | Y | Y | Y | Y | Y | Y |
| Fight_Dead |  |  |  | Y | Y | Y | Y |
| Fight_BattleAttack |  | Y | Y | Y | Y | Y | Y |
| Fight_DoMove | Y | Y | Y | Y | Y | Y | Y |
| GetTarget2 |  |  | Y | Y | Y | Y | Y |
| Fight_ReadyScroll |  |  |  | Y | Y | Y | Y |

---

## alliance.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| Alliances_Init | Y | Y |  | Y |  | Y | Y |
| Alliances_Close | Y |  |  |  |  |  | Y |
| DeleteAlliance | Y | Y |  | Y |  | Y | Y |
| KillAlliances | Y | Y |  | Y |  | Y | Y |

---

## alliancem.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| RemoveFromAlliance | Y | Y | Y | Y | Y | Y | Y |
| SeeMemberStats | Y | Y | Y | Y | Y | Y | Y |
| ShowAllianceItems |  |  |  | Y | Y | Y | Y |
| DonationRoom | Y | Y | Y | Y | Y | Y | Y |
| CreateAlliance | Y | Y | Y | Y | Y | Y | Y |
| ShowAlliances |  |  | Y | Y | Y | Y | Y |
| EnterAlliance | Y | Y | Y | Y | Y | Y | Y |
| FormAlliance |  |  |  | Y | Y | Y | Y |
| KillAlliance | Y | Y |  | Y | Y | Y | Y |
| Alliance_Maint |  |  |  |  |  | Y |  |

---

## voting.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| GetVotes |  | Y |  |  |  | Y |  |
| VotingBooth | Y | Y | Y | Y | Y | Y | Y |
| ChooseNewLeader | Y | Y | Y | Y | Y | Y | Y |

---

## pcedit.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| main | Y | Y |  | Y |  | Y | Y |
| DeleteClan | Y | Y |  | Y |  | Y | Y |
| InitVillage | Y | Y |  | Y |  | Y | Y |
| UpdateVillage | Y | Y |  | Y |  | Y | Y |
| RejectTrade | Y | Y |  | Y |  | Y | Y |
| RemoveFromUList | Y | Y |  | Y |  | Y |  |
| ClanIDInList |  | Y |  |  |  | Y |  |
| RemoveFromIPScores | Y | Y |  | Y |  | Y | Y |
| FreeClan |  |  |  |  |  |  |  |
| GetClan | Y | Y |  | Y |  | Y | Y |
| UpdateClan | Y | Y |  | Y |  | Y | Y |
| InitGame | Y | Y |  | Y |  | Y | Y |
| pce_getch |  |  | Y |  |  |  |  |

---

## menus.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| WorldMenu | Y | Y | Y | Y | Y | Y | Y |
| AddChatFile | Y | Y |  |  |  |  |  |
| Menus_ChatRoom | Y | Y | Y | Y | Y | Y | Y |
| MainMenu | Y | Y | Y | Y | Y | Y | Y |
| MineMenu | Y | Y | Y | Y | Y | Y | Y |
| CommunicationsMenu | Y | Y | Y | Y | Y | Y | Y |
| WizardShop | Y | Y | Y | Y | Y | Y | Y |
| MarketMenu | Y | Y | Y | Y | Y | Y | Y |
| AlliancesMenu | Y | Y | Y | Y | Y | Y | Y |
| ChurchMenu | Y | Y | Y | Y | Y | Y | Y |
| THallMenu | Y | Y | Y | Y | Y | Y | Y |
| GameLoop | Y | Y | Y | Y | Y | Y | Y |

---

## menus2.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| ResurrectDead |  |  | Y | Y | Y | Y | Y |
| ReleaseMember |  |  | Y | Y | Y | Y | Y |
| AddMember | Y | Y | Y | Y | Y | Y | Y |
| TrainMember | Y | Y | Y | Y | Y | Y | Y |

---

## scores.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| GetColourString |  |  |  |  |  |  |  |
| PipeToAnsi |  |  |  |  |  |  |  |
| DisplayScores | Y | Y |  | Y |  | Y |  |
| SendScoreData |  |  |  |  |  |  | Y |
| ProcessScoreData | Y | Y |  |  |  |  | Y |
| LeagueScores |  | Y |  | Y |  | Y |  |
| RemoveFromIPScores | Y | Y |  | Y |  | Y | Y |
| SendScoreList |  | Y |  |  |  | Y |  |
| CreateScoreData |  | Y |  |  |  | Y | Y |

---

## reset.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| main | Y | Y |  | Y |  | Y | Y |
| ResetMenu | Y |  | Y | Y |  | Y | Y |
| EditOption | Y | Y | Y | Y |  | Y | Y |
| DosHelp |  | Y |  | Y |  |  |  |
| GenerateGameID |  |  |  |  |  |  |  |
| GoReset | Y |  |  | Y |  | Y | Y |
| News_CreateTodayNews | Y |  |  | Y |  | Y | Y |
| News_AddNews |  |  |  | Y |  | Y | Y |
| ClearFlags |  |  |  |  |  |  |  |
| InitEmpire |  |  |  |  |  |  |  |
| CreateVillageDat | Y |  |  |  | Y |  |  |
| UpdateOption |  |  |  | Y |  | Y | Y |

---

## system.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| CheckMem | Y | Y |  | Y |  | Y | Y |
| DupeStr | Y | Y |  | Y |  | Y | Y |
| System_LockedOut |  | Y |  |  |  |  |  |
| System_Error | Y | Y |  | Y |  | Y | Y |
| ShowHelp |  |  |  | Y |  | Y | Y |
| ODCmdLineHandler | Y | Y |  | Y |  | Y | Y |
| ODCmdLineFlagHandler | Y | Y | Y | Y | Y | Y | Y |
| System_Maint | Y |  |  | Y |  | Y | Y |
| System_Close_AtExit | Y | Y |  | Y |  | Y | Y |
| System_Close | Y | Y |  | Y |  | Y | Y |
| System_Init | Y | Y | Y | Y | Y | Y | Y |

---

## console.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| Console_SetScriptMode |  |  |  |  |  |  | Y |
| Console_SetGetAnswerHook |  |  |  |  |  |  | Y |
| Door_Initialized |  |  |  |  |  | Y |  |
| rputs |  |  |  | Y | Y | Y | Y |
| GetKey |  |  | Y |  |  |  |  |
| CheckMem | Y | Y |  | Y |  | Y | Y |
| System_Error | Y | Y |  | Y |  | Y | Y |
| LogDisplayStr |  |  |  | Y |  | Y |  |
| GetAnswer |  |  | Y |  |  |  |  |
| yesNoQ |  |  | Y |  |  |  |  |
| YesNo |  |  | Y |  |  | Y |  |
| NoYes |  |  | Y |  |  | Y |  |
| InputCallback |  |  |  |  |  |  |  |
| PutCh |  |  |  | Y |  |  |  |
| rawputs |  |  |  | Y |  |  |  |
| GetHoursLeft |  |  |  |  |  | Y |  |
| GetMinutesLeft |  |  |  |  |  | Y |  |
| door_pause |  |  | Y | Y |  | Y | Y |
| Door_AllowScreenPause |  |  |  |  |  | Y |  |
| Door_ToggleScreenPause |  |  |  |  |  |  | Y |
| GetKeyNoWait |  |  | Y |  |  |  |  |

---

## door.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| Door_Initialized |  |  |  |  |  | Y |  |
| Door_SetColorScheme |  |  |  |  |  |  | Y |
| LocalLoad |  | Y | Y | Y |  | Y | Y |
| BeforeChat |  |  |  | Y |  |  |  |
| AfterChat |  |  |  | Y |  |  |  |
| NoDoorFileHandler |  |  |  | Y |  | Y |  |
| NoStatus |  |  |  | Y |  | Y | Y |
| TwoLiner |  |  |  | Y |  | Y | Y |
| Door_ToggleScreenPause |  |  |  |  |  |  | Y |
| Door_SetScreenPause |  |  |  |  |  |  | Y |
| Door_AllowScreenPause |  |  |  |  |  | Y |  |
| door_pause |  |  | Y | Y |  | Y | Y |
| rputs |  |  |  | Y | Y | Y | Y |
| LogToWhatever | Y |  |  | Y |  | Y |  |
| GetKey |  |  | Y |  |  |  |  |
| GetKeyNoWait |  |  | Y |  |  |  |  |
| GetAnswer |  |  | Y |  |  |  |  |
| LogStr | Y |  |  |  |  |  |  |
| LogDisplayStr |  |  |  | Y |  | Y |  |
| Display |  | Y | Y | Y |  | Y | Y |
| YesNo |  |  | Y |  |  | Y |  |
| NoYes |  |  | Y |  |  | Y |  |
| Door_ShowTitle |  |  | Y | Y | Y | Y | Y |
| Door_Init | Y | Y | Y | Y |  | Y | Y |
| Door_Close | Y |  |  |  |  | Y | Y |
| InputCallback |  |  |  |  |  |  |  |
| PutCh |  |  |  | Y |  |  |  |
| rawputs |  |  |  | Y |  |  |  |
| GetHoursLeft |  |  |  |  |  | Y |  |
| GetMinutesLeft |  |  |  |  |  | Y |  |

---

## mail.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| GenericMessage | Y |  |  | Y |  | Y |  |
| MyWriteMessage2 | Y | Y | Y | Y | Y | Y | Y |
| Mail_Read | Y | Y | Y | Y | Y | Y | Y |
| Mail_Write |  |  | Y | Y |  | Y | Y |
| Mail_Maint | Y | Y |  | Y |  | Y |  |
| Mail_RequestAlliance | Y | Y | Y | Y | Y | Y | Y |
| Mail_WriteToAllies | Y | Y | Y | Y | Y | Y | Y |
| PostMsj | Y |  |  |  |  | Y | Y |
| GlobalMsgPost | Y | Y | Y | Y | Y | Y | Y |

---

## spells.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| get_spell | Y | Y |  | Y |  | Y | Y |
| Spells_Init | Y | Y |  | Y |  | Y | Y |
| Spells_Close |  |  |  |  |  |  | Y |
| Spells_UpdatePCSpells |  |  |  | Y | Y | Y | Y |
| Spells_ClearSpells |  |  |  |  |  | Y | Y |
| Spells_CastSpell | Y | Y | Y | Y | Y | Y | Y |

---

## chew.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| main | Y | Y |  | Y |  | Y | Y |
| AddGUM | Y | Y |  | Y |  |  | Y |
| AddDir | Y |  |  | Y |  |  |  |
| _dos_getftime |  |  |  |  |  |  |  |

---

## ibbs.c (lines 1–1778)

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| IBBS_SendFileInPacket | Y | Y |  | Y |  | Y | Y |
| IBBS_SendUserList | Y | Y |  | Y |  | Y | Y |
| IBBS_DistributeNDX | Y | Y |  | Y |  | Y | Y |
| LeagueKillUser | Y | Y |  | Y |  | Y | Y |
| IBBS_ProcessSpy | Y | Y | Y | Y | Y | Y | Y |
| IBBS_ProcessSpyResult | Y |  |  | Y |  | Y |  |
| IBBS_SendQueuedResults | Y | Y |  | Y |  | Y | Y |
| EnqueueInPacket | Y | Y |  | Y |  | Y | Y |
| IBBS_SendDelUser | Y | Y |  | Y |  | Y | Y |
| ClanIDInList |  | Y |  |  |  | Y |  |
| RemoveFromUList | Y | Y |  | Y |  | Y |  |
| AddToUList | Y | Y |  |  |  | Y |  |
| UpdateNodesOnNewUser | Y | Y |  | Y |  | Y | Y |
| IBBS_LeagueNewUser | Y | Y |  | Y |  | Y | Y |
| IBBS_InList |  | Y |  |  |  | Y |  |
| IBBS_SendComeBack | Y | Y |  | Y |  | Y | Y |
| VillageName |  |  |  |  |  | Y |  |
| BBSName |  |  |  |  |  | Y |  |
| ReturnLostAttack | Y | Y |  | Y |  | Y | Y |
| IBBS_BackupMaint | Y | Y |  | Y |  | Y | Y |
| AbortTrip | Y | Y |  | Y | Y | Y | Y |
| IBBS_CurrentTravelInfo | Y | Y | Y | Y | Y | Y | Y |
| IBBS_UpdateLeavingClan | Y | Y |  | Y |  | Y | Y |
| IBBS_TravelMaint | Y | Y |  | Y |  | Y | Y |
| GetTroopsTraveling |  |  | Y | Y | Y | Y | Y |
| IBBS_TravelToBBS | Y | Y | Y | Y | Y | Y | Y |
| IBBS_SeeVillages | Y | Y | Y | Y | Y | Y | Y |
| IBBS_AddLCLog | Y | Y |  |  |  | Y |  |
| IBBS_Destroy |  |  |  |  |  | Y | Y |
| IBBS_LoginStats |  |  |  | Y |  | Y | Y |
| IBBS_Create |  |  |  |  |  |  | Y |
| IBBS_Read | Y | Y |  |  |  | Y | Y |
| IBBS_Write | Y |  |  |  |  | Y |  |
| FindPoint |  |  |  |  |  |  | Y |
| IBBS_LoadNDX | Y | Y |  | Y |  | Y | Y |
| IBBS_ProcessRouteConfig | Y | Y |  | Y |  | Y | Y |

---

## village.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| OutsiderTownHallMenu | Y | Y | Y | Y | Y | Y | Y |
| ChangeFlagScheme | Y | Y | Y | Y | Y | Y | Y |
| GetNums | Y | Y |  | Y |  | Y | Y |
| LoadSchemes | Y | Y |  | Y |  | Y | Y |
| AddScheme | Y |  |  | Y | Y | Y | Y |
| ChangeColourScheme | Y | Y | Y | Y | Y | Y | Y |
| BuildMenu | Y | Y | Y | Y | Y | Y | Y |
| EconomicsMenu | Y | Y | Y | Y | Y | Y | Y |
| ToggleEmpireStats |  |  |  | Y | Y | Y | Y |
| TownHallMenu | Y | Y | Y | Y | Y | Y | Y |
| Village_NewRuler |  |  | Y | Y | Y | Y | Y |
| Village_Destroy |  |  |  |  |  |  | Y |
| Village_Read |  | Y |  |  |  |  | Y |
| Village_Write | Y |  |  |  |  | Y |  |
| Village_Reset |  | Y | Y | Y | Y | Y | Y |
| Village_Maint | Y | Y | Y | Y | Y | Y | Y |
| Village_Init | Y | Y |  | Y |  | Y | Y |
| Village_Close | Y |  |  |  |  | Y | Y |

---

## myibbs.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| ConvertStringToAddress |  |  |  |  |  |  |  |
| IBSendFileAttach | Y |  |  |  |  | Y |  |

---

## quests.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| Quests_Init | Y | Y |  | Y |  | Y | Y |
| Quests_Close |  |  |  |  |  | Y | Y |
| ClearFlags |  |  |  |  |  |  |  |
| SetFlag |  |  |  |  |  |  |  |
| ClearFlag |  |  |  |  |  |  |  |
| FlagSet |  |  |  |  |  |  |  |
| legal | Y | Y | Y | Y | Y | Y | Y |
| JumpToEvent | Y | Y |  | Y |  | Y | Y |
| TellQuest |  |  |  | Y | Y | Y | Y |
| RunEvent | Y | Y | Y | Y | Y | Y | Y |
| Quests_GoQuest | Y | Y | Y | Y | Y | Y | Y |

---

## user.c (lines 1321–2642)

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| PC_Create | Y | Y | Y | Y | Y | Y | Y |
| NameInUse |  | Y |  |  |  | Y |  |
| ChooseClanName | Y | Y | Y | Y | Y | Y | Y |
| User_ResetHelp |  |  |  |  |  | Y | Y |
| User_Create | Y | Y | Y | Y | Y | Y | Y |
| User_Destroy |  |  |  |  |  | Y | Y |
| User_Read | Y | Y |  | Y |  | Y | Y |
| Clan_Update | Y | Y |  |  |  | Y |  |
| User_Write | Y | Y |  |  |  | Y |  |
| User_FirstTimeToday |  |  |  |  |  | Y | Y |
| NumMembers |  |  |  |  |  | Y |  |
| GetClanID | Y | Y | Y | Y | Y | Y | Y |
| GetClanNameID |  |  |  |  |  |  |  |
| GetClan | Y | Y |  | Y |  | Y | Y |
| User_List |  | Y |  | Y |  | Y |  |
| User_Maint | Y | Y |  | Y |  | Y | Y |
| User_Init | Y | Y | Y | Y | Y | Y | Y |
| User_Close | Y | Y |  |  |  | Y | Y |

---

## empire.c (lines 1576–3149)

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| DestroyBuildings |  | Y | Y | Y | Y | Y | Y |
| ShowResults |  |  |  | Y | Y | Y | Y |
| GetNumTroops |  |  | Y | Y | Y | Y | Y |
| StartEmpireWar | Y | Y | Y | Y | Y | Y | Y |
| SpyMenu | Y | Y | Y | Y | Y | Y | Y |
| Empire_Manage | Y | Y | Y | Y | Y | Y | Y |

---

## mcomp.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| main | Y | Y |  | Y |  | Y | Y |

---

## mclass.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| main | Y | Y |  | Y |  | Y | Y |
| Deinit_PClasses |  |  |  |  |  |  |  |
| Init_PClasses |  | Y |  | Y |  |  |  |

---

## mitems.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| main | Y | Y |  | Y |  | Y | Y |
| Deinit_Items |  |  |  |  |  |  |  |
| Init_Items | Y | Y |  | Y |  | Y | Y |

---

## mspells.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| main | Y | Y |  | Y |  | Y | Y |
| Deinit_Spells |  |  |  |  |  |  |  |
| Init_Spells | Y | Y |  | Y |  | Y | Y |

---

## makenpc.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| main | Y | Y |  | Y |  | Y | Y |
| Init_NPCs | Y | Y |  | Y |  |  |  |

---

## makepak.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| main | Y | Y |  | Y |  | Y | Y |
| AddToPak | Y | Y |  | Y |  |  |  |

---

## qtest.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| Display |  | Y | Y | Y |  | Y | Y |
| LogStr | Y |  |  |  |  |  |  |
| DupeStr | Y | Y |  | Y |  | Y | Y |
| script_read_line |  | Y |  |  |  |  |  |
| script_consume |  | Y |  | Y |  |  |  |
| script_expect_end |  | Y |  | Y |  |  |  |
| hook_get_answer |  | Y |  | Y |  |  |  |
| hook_dos_get_str |  | Y |  | Y |  |  |  |
| hook_dos_get_long |  | Y |  | Y |  |  |  |
| hook_get_string_choice |  | Y |  | Y |  |  |  |
| Fight_Fight |  | Y | Y | Y |  | Y | Y |
| Fight_CheckLevelUp |  |  |  |  |  |  |  |
| FreeClan |  |  |  |  |  |  |  |
| FreeClanMembers |  |  |  |  |  |  |  |
| News_AddNews |  |  |  | Y |  | Y | Y |
| Items_GiveItem |  |  |  | Y |  | Y | Y |
| NumMembers |  |  |  |  |  | Y |  |
| GetClanNameID |  |  |  |  |  |  |  |
| GetClan | Y | Y |  | Y |  | Y | Y |
| ClanStats |  |  |  | Y |  | Y | Y |
| GetStat |  |  |  |  |  | Y |  |
| my_random |  | Y | Y | Y | Y | Y | Y |
| setup_dummy_pclan |  |  |  |  |  |  | Y |
| setup_dummy_village |  |  |  |  |  |  | Y |
| print_flags |  |  |  | Y |  | Y | Y |
| print_quest_flags |  |  |  | Y |  | Y | Y |
| count_active_quests |  |  |  |  |  | Y |  |
| print_flag_summary |  |  |  | Y |  | Y |  |
| print_quest_summary |  |  |  | Y |  | Y |  |
| print_state_summary |  |  |  | Y |  | Y |  |
| parse_flag_list |  |  |  |  |  |  |  |
| parse_quest_list |  |  |  |  |  |  |  |
| show_main_screen |  |  |  | Y |  | Y | Y |
| set_or_clear_flag |  |  | Y | Y |  | Y | Y |
| edit_flags_menu |  |  | Y | Y |  | Y | Y |
| edit_gold_mine |  |  | Y | Y |  | Y | Y |
| collect_npcs | Y | Y |  | Y |  | Y | Y |
| wander_name |  |  |  |  |  |  |  |
| npc_submenu | Y | Y | Y | Y |  | Y | Y |
| quest_submenu | Y | Y | Y | Y | Y | Y | Y |
| dbg_init | Y | Y |  | Y |  | Y | Y |
| dbg_close |  |  |  | Y |  | Y | Y |
| main | Y | Y |  | Y |  | Y | Y |

---

## install.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| Display |  | Y | Y | Y |  | Y | Y |
| detect_utf8_marker |  | Y |  |  |  |  |  |
| SystemInit |  | Y |  | Y |  | Y | Y |
| main | Y | Y |  | Y |  | Y | Y |
| ReadFilename |  | Y |  |  |  |  |  |
| fseekuc |  | Y |  |  |  |  |  |
| GetGUM | Y | Y | Y | Y |  | Y | Y |
| install | Y | Y | Y | Y |  | Y | Y |
| upgrade | Y | Y | Y | Y |  | Y | Y |
| FileExists |  | Y |  |  |  |  |  |
| FreeFiles |  |  |  |  |  |  |  |
| InitFiles |  | Y |  | Y |  | Y | Y |
| Match |  |  |  |  |  |  |  |
| WriteType |  |  |  |  |  |  |  |
| Extract | Y | Y | Y | Y |  | Y | Y |
| CheckRow |  |  | Y | Y |  | Y | Y |
| AddLFN | Y | Y |  | Y |  | Y | Y |
| FreeLFNs |  |  |  |  |  |  |  |
| DisplayLFNs |  |  | Y | Y |  | Y | Y |
| ListFiles | Y | Y | Y | Y |  | Y | Y |
| GetGumName |  | Y |  | Y |  | Y | Y |
| _dos_setftime |  |  |  |  |  |  |  |

---

## langcomp.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| main | Y | Y |  | Y |  | Y | Y |
| Convert |  |  |  |  |  |  |  |

---

## ecomp.c

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| main | Y | Y |  | Y |  | Y | Y |

---

## ibbs.c (lines 1779–3557)

| Function | Disk Write | Disk Read | User Input | User Output | my_random | Read Global | Write Global |
|---|---|---|---|---|---|---|---|
| getPacketFP | Y | Y |  | Y |  | Y | Y |
| IBBS_SendPacketBuffer | Y | Y |  | Y |  | Y | Y |
| IBBS_SendSpy | Y | Y |  | Y | Y | Y | Y |
| IBBS_SendAttackPacket | Y | Y |  | Y |  | Y | Y |
| IBBS_ShowLeagueAscii |  | Y |  | Y | Y | Y | Y |
| IBBS_SendPacket | Y | Y |  | Y |  | Y | Y |
| IBBS_EnqueueOutPacket | Y | Y |  | Y |  | Y | Y |
| IBBS_SendRecon | Y | Y |  | Y |  | Y | Y |
| IBBS_SendReset | Y | Y |  | Y |  | Y | Y |
| IBBS_UpdateRecon | Y | Y |  | Y |  | Y | Y |
| IBBS_UpdateReset | Y | Y |  | Y |  | Y | Y |
| IBBS_LeagueInfo |  |  | Y | Y | Y | Y | Y |
| Reset | Y | Y | Y | Y | Y | Y | Y |
| IBBS_Reset | Y | Y | Y | Y | Y | Y | Y |
| IBBS_RemoveFromBackup | Y | Y |  | Y |  | Y | Y |
| IBBS_AddToGame | Y | Y |  | Y |  | Y | Y |
| ComeBack | Y | Y |  | Y |  | Y | Y |
| CheckID |  |  |  | Y |  | Y |  |
| CheckSourceID |  |  |  | Y |  | Y |  |
| CheckDestinationID |  |  |  | Y |  | Y |  |
| HandleResetPacket | Y | Y | Y | Y | Y | Y | Y |
| HandleMessagePacket | Y | Y |  | Y |  | Y | Y |
| HandleScoreDataPacket | Y | Y |  | Y |  | Y | Y |
| HandleScoreListPacket | Y | Y |  | Y |  | Y | Y |
| HandleClanMovePacket | Y | Y |  | Y |  | Y | Y |
| HandleNewUserPacket | Y | Y |  | Y |  | Y | Y |
| HandleAddUserPacket | Y | Y |  |  |  | Y |  |
| HandleDelUserPacket | Y | Y |  | Y |  | Y | Y |
| HandleSubUserPacket | Y | Y |  | Y |  | Y | Y |
| HandleAttackPacket |  | Y |  |  |  |  |  |
| HandleAttackResultPacket |  | Y |  |  |  |  |  |
| HandleSpyPacket | Y | Y | Y | Y | Y | Y | Y |
| IBBS_HandleQueuedPackets | Y | Y | Y | Y | Y | Y | Y |
| HandleSpyResultPacket | Y | Y |  | Y |  | Y |  |
| HandleNewNDXPacket | Y | Y |  | Y |  | Y | Y |
| HandleUserListPacket | Y | Y |  | Y |  | Y | Y |
| IBBS_ProcessPacket | Y | Y | Y | Y | Y | Y | Y |
| MessageFileIterate | Y | Y |  | Y |  | Y | Y |
| SkipAfterFound |  |  |  |  |  | Y |  |
| SetSkip |  |  |  |  |  |  | Y |
| CheckMessageFile | Y | Y |  | Y |  | Y | Y |
| DeleteFound | Y |  |  | Y |  | Y |  |
| DeleteMessageWithFile | Y | Y |  | Y |  | Y | Y |
| MoveToBad | Y |  |  |  |  |  |  |
| IBBS_PacketIn | Y | Y | Y | Y | Y | Y | Y |
| IBBS_Init | Y | Y |  | Y |  | Y | Y |
| IBBS_Close | Y |  |  |  |  | Y | Y |
| IBBS_Maint | Y | Y | Y | Y | Y | Y | Y |

---
