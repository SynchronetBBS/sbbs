/*
 * gtest -- Game Test Tool
 *
 * Script-driven integration testing for game combat and menus.
 * Follows the same pattern as qtest (Quest Pack Debugger).
 */

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"

#include "class.h"
#include "clansini.h"
#include "console.h"
#include "fight.h"
#include "input.h"
#include "items.h"
#include "language.h"
#include "myopen.h"
#include "scripteng.h"
#include "spells.h"
#include "video.h"

/* =========================================================================
 * Globals required by fight.c / items.c / spells.c / class.c
 * ========================================================================= */

struct clan    PClan;
struct village Village;
struct game    Game;
struct system  System = { false, "20260315", ".", 0, true, false };

bool   Verbose = false;

/* =========================================================================
 * Mocks — functions referenced by linked .o files but not needed
 * ========================================================================= */

/* door.h / system.h */
void Display(char *FileName)
{
	char buf[256];
	snprintf(buf, sizeof(buf), "[MOCK Display] file=\"%s\"\n", FileName);
	zputs(buf);
}

void LogStr(const char *szString)
{
	fputs(szString, stdout);
}

char *DupeStr(const char *str)
{
	char *pc = strdup(str);
	CheckMem(pc);
	return pc;
}

/* OpenDoors */
void od_clr_scr(void) { }

/* news.h */
void News_AddNews(char *szString)
{
	char buf[512];
	snprintf(buf, sizeof(buf), "[MOCK News] %s\n", szString);
	zputs(buf);
}

/* mail.h */
void GenericMessage(char *szString, int16_t ToClanID[2],
                    int16_t FromClanID[2], char *szFrom, bool AllowReply)
{
	(void)szString; (void)ToClanID; (void)FromClanID;
	(void)szFrom; (void)AllowReply;
}

/* user.h — re-implementations (same as the real user.c logic) */

int16_t NumMembers(struct clan *Clan, bool OnlyOnesHere)
{
	int16_t Members = 0;
	for (int16_t i = 0; i < MAX_MEMBERS; i++) {
		if (Clan->Member[i]) {
			if (OnlyOnesHere && Clan->Member[i]->Status != Here)
				continue;
			Members++;
		}
	}
	return Members;
}

int8_t GetStat(struct pc *PC, char Stat)
{
	int8_t val;
	if (!PC) return 0;
	val = PC->Attributes[(int)Stat];
	if (PC->Weapon)
		val += PC->MyClan->Items[PC->Weapon - 1].Attributes[(int)Stat];
	if (PC->Shield)
		val += PC->MyClan->Items[PC->Shield - 1].Attributes[(int)Stat];
	if (PC->Armor)
		val += PC->MyClan->Items[PC->Armor - 1].Attributes[(int)Stat];
	for (int i = 0; i < MAX_SPELLS_IN_EFFECT; i++) {
		if (PC->SpellsInEffect[i].SpellNum == -1)
			continue;
		val += Spells[PC->SpellsInEffect[i].SpellNum]->Attributes[(int)Stat];
	}
	return val;
}

void ShowPlayerStats(struct pc *PC, bool AllowModify)
{
	(void)AllowModify;
	char buf[128];
	snprintf(buf, sizeof(buf), "[Stats] %s  HP=%d/%d  SP=%d/%d  Lv=%d\n",
	         PC->szName, PC->HP, PC->MaxHP, PC->SP, PC->MaxSP, PC->Level);
	zputs(buf);
}

void ListItems(struct clan *Clan)
{
	(void)Clan;
	zputs("[Items list]\n");
}

void ClanStats(struct clan *Clan, bool AllowModify)
{
	(void)AllowModify;
	char buf[128];
	snprintf(buf, sizeof(buf), "[ClanStats] %s\n", Clan->szName);
	zputs(buf);
}

bool GetClanNameID(char *szName, size_t sz, int16_t ID[2])
{
	(void)ID;
	strlcpy(szName, "[MockClan]", sz);
	return true;
}

bool GetClan(int16_t ClanID[2], struct clan *TmpClan)
{
	(void)ClanID;
	memset(TmpClan, 0, sizeof(*TmpClan));
	strlcpy(TmpClan->szName, "[MockClan]", sizeof(TmpClan->szName));
	return false;
}

bool GetClanID(int16_t ID[2], bool OnlyLiving, bool IncludeSelf,
               int16_t WhichAlliance, bool InAllianceOnly)
{
	(void)ID; (void)OnlyLiving; (void)IncludeSelf;
	(void)WhichAlliance; (void)InAllianceOnly;
	return false;
}

bool ClanExists(int16_t ClanID[2])
{
	(void)ClanID;
	return false;
}

void Clan_Update(struct clan *Clan) { (void)Clan; }
void User_Write(void) { }
void ShowVillageStats(void) { }

/* ibbs.h */
const char *VillageName(int16_t BBSID) { (void)BBSID; return "TestVillage"; }
const char *BBSName(int16_t BBSID) { (void)BBSID; return "TestBBS"; }

/* random.h — script-controlled, fixed, seeded, or default PRNG */
static bool g_fixed_rand_set = false;
static int  g_fixed_rand     = 0;

int my_random(int limit)
{
	if (limit <= 0) return 0;
	if (limit == 1) return 0;
	if (script_is_active()) {
		const char *val = script_consume("Random");
		int n = atoi(val);
		if (n < 0) n = 0;
		if (n >= limit) n = limit - 1;
		return n;
	}
	if (g_fixed_rand_set)
		return g_fixed_rand % limit;
	return rand() % limit;
}

/* =========================================================================
 * Dummy setup
 * ========================================================================= */

static struct pc g_members[4];

static void setup_dummy_pclan(void)
{
	memset(&PClan, 0, sizeof(PClan));
	memset(g_members, 0, sizeof(g_members));

	strlcpy(PClan.szName, "TestClan", sizeof(PClan.szName));
	strlcpy(PClan.szUserName, "TestUser", sizeof(PClan.szUserName));
	PClan.ClanID[0] = 1;
	PClan.ClanID[1] = 0;
	PClan.Empire.VaultGold = 5000;
	PClan.MineLevel = 1;
	PClan.FightsLeft = 10;
	PClan.Points = 0;

	struct {
		const char *name;
		int8_t str, dex, agi, wis, cha;
		int16_t hp, sp;
		int16_t class_id;
	} archetypes[] = {
		{ "Fighter", 10, 7, 6, 4, 5,  80, 10, 0 },
		{ "Mage",     4, 5, 5, 10, 5, 40, 60, 1 },
		{ "Cleric",   5, 5, 5, 9, 7,  50, 50, 2 },
		{ "Thief",    6, 10, 9, 5, 5, 50, 20, 3 },
	};

	for (int i = 0; i < 4; i++) {
		PClan.Member[i] = &g_members[i];
		struct pc *m = PClan.Member[i];
		strlcpy(m->szName, archetypes[i].name, sizeof(m->szName));
		m->Attributes[ATTR_STRENGTH]  = archetypes[i].str;
		m->Attributes[ATTR_DEXTERITY] = archetypes[i].dex;
		m->Attributes[ATTR_AGILITY]   = archetypes[i].agi;
		m->Attributes[ATTR_WISDOM]    = archetypes[i].wis;
		m->Attributes[ATTR_CHARISMA]  = archetypes[i].cha;
		m->HP = m->MaxHP = archetypes[i].hp;
		m->SP = m->MaxSP = archetypes[i].sp;
		m->Level = 1;
		m->Experience = 0;
		m->Status = Here;
		m->WhichClass = archetypes[i].class_id;
		m->Weapon = 0;
		m->Shield = 0;
		m->Armor = 0;
		m->MyClan = &PClan;
		for (int j = 0; j < MAX_SPELLS_IN_EFFECT; j++)
			m->SpellsInEffect[j].SpellNum = -1;
	}
}

static void setup_dummy_village(void)
{
	memset(&Village, 0, sizeof(Village));
	strlcpy(Village.Data.szName, "TestVillage", sizeof(Village.Data.szName));
	Village.Data.TaxRate = 10;
	Village.Data.ConscriptionRate = 10;
	Village.Data.MarketLevel = 3;
	Village.Data.WizardLevel = 1;
	Village.Initialized = true;
}

static void setup_dummy_game(void)
{
	memset(&Game, 0, sizeof(Game));
	Game.Data.MaxPermanentMembers = 6;
	Game.Data.MineFights = 10;
	Game.Data.GameState = 0;  /* 0 == game in progress */
	Game.Initialized = true;
}

/* =========================================================================
 * State summary (printed to stderr for test assertions)
 * ========================================================================= */

static void print_state_summary(int fight_result)
{
	const char *result_str = "NONE";
	if (fight_result == FT_WON) result_str = "WON";
	else if (fight_result == FT_LOST) result_str = "LOST";
	else if (fight_result == FT_RAN) result_str = "RAN";

	fprintf(stderr, "FightResult=%s\n", result_str);
	fprintf(stderr, "Gold=%" PRId32 "\n", PClan.Empire.VaultGold);
	fprintf(stderr, "MineLevel=%d\n", PClan.MineLevel);
	fprintf(stderr, "Points=%" PRId32 "\n", PClan.Points);
	fprintf(stderr, "FightsLeft=%d\n", PClan.FightsLeft);
	fprintf(stderr, "Followers=%" PRId32 "\n", PClan.Empire.Army.Followers);

	for (int i = 0; i < 4; i++) {
		struct pc *m = PClan.Member[i];
		if (!m) continue;
		fprintf(stderr, "Member%d.Name=%s\n", i, m->szName);
		fprintf(stderr, "Member%d.HP=%d\n", i, m->HP);
		fprintf(stderr, "Member%d.MaxHP=%d\n", i, m->MaxHP);
		fprintf(stderr, "Member%d.SP=%d\n", i, m->SP);
		fprintf(stderr, "Member%d.MaxSP=%d\n", i, m->MaxSP);
		fprintf(stderr, "Member%d.XP=%" PRId32 "\n", i, m->Experience);
		fprintf(stderr, "Member%d.Level=%d\n", i, m->Level);
	}
}

/* =========================================================================
 * Init / Close
 * ========================================================================= */

static void gtest_init(void)
{
	Video_Init();
	ClansIni_Init();
	Language_Init("strings.xl");
	Spells_Init();
	PClass_Init();
	setup_dummy_pclan();
	setup_dummy_village();
	setup_dummy_game();
}

static void gtest_close(void)
{
	Spells_Close();
	PClass_Close();
	Language_Close();
	ClansIni_Close();
	Video_Close();
}

/* =========================================================================
 * Usage
 * ========================================================================= */

static void usage(void)
{
	fprintf(stderr,
	        "Usage: gtest -c COMMAND [options]\n"
	        "\n"
	        "Commands:\n"
	        "  -c autofight   Run Fight_Monster with AI control (no input)\n"
	        "  -c levelup     Run Fight_CheckLevelUp\n"
	        "\n"
	        "Options:\n"
	        "  -l LEVEL       Mine level for autofight (default: 1)\n"
	        "  -R SEED        Seed the PRNG for deterministic results\n"
	        "  -r VALUE       Fixed return value for my_random (mod limit)\n"
	        "  -g GOLD        Set starting gold\n"
	        "  -m LEVEL       Set starting mine level\n"
	        "  -s SCRIPT      Script file for input\n"
	        );
}

/* =========================================================================
 * main
 * ========================================================================= */

int main(int argc, char *argv[])
{
	char command[32]     = {0};
	char script_path[256] = {0};
	int  mine_level      = 1;
	long arg_gold        = -1;
	long arg_mine        = -1;

	/* ---- argument parsing ---- */
	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (a[0] != '-' || a[1] == '\0') {
			fprintf(stderr, "gtest: unexpected argument: %s\n", a);
			return 1;
		}
		switch (a[1]) {
		case 'c':
			if (i + 1 >= argc) {
				fprintf(stderr, "gtest: -c requires a command\n");
				return 1;
			}
			strlcpy(command, argv[++i], sizeof(command));
			break;
		case 'l':
			if (i + 1 >= argc) {
				fprintf(stderr, "gtest: -l requires a level\n");
				return 1;
			}
			mine_level = atoi(argv[++i]);
			break;
		case 'R':
			if (i + 1 >= argc) {
				fprintf(stderr, "gtest: -R requires a seed\n");
				return 1;
			}
			srand((unsigned)atoi(argv[++i]));
			break;
		case 'r':
			if (i + 1 >= argc) {
				fprintf(stderr, "gtest: -r requires a value\n");
				return 1;
			}
			g_fixed_rand_set = true;
			g_fixed_rand = atoi(argv[++i]);
			break;
		case 'g':
			arg_gold = atol(a + 2);
			break;
		case 'm':
			arg_mine = atol(a + 2);
			break;
		case 's':
			if (i + 1 >= argc) {
				fprintf(stderr, "gtest: -s requires a file\n");
				return 1;
			}
			strlcpy(script_path, argv[++i], sizeof(script_path));
			break;
		default:
			fprintf(stderr, "gtest: unknown option: %s\n", a);
			return 1;
		}
	}

	if (command[0] == '\0') {
		usage();
		return 1;
	}

	/* ---- suppress interactive prompts before init ---- */
	Console_SetScriptMode(true);
	Video_SetScriptMode(true);

	/* ---- script mode ---- */
	if (script_path[0]) {
		if (!script_open(script_path)) {
			fprintf(stderr, "gtest: cannot open script file \"%s\"\n",
			        script_path);
			return 1;
		}
		script_set_tool_name("gtest");
		script_install_hooks();
	}

	/* ---- initialise ---- */
	gtest_init();

	/* ---- apply state arguments ---- */
	if (arg_gold >= 0) PClan.Empire.VaultGold = (int32_t)arg_gold;
	if (arg_mine >= 0) PClan.MineLevel = (int16_t)arg_mine;

	/* ---- dispatch command ---- */
	int fight_result = -1;

	if (plat_stricmp(command, "autofight") == 0) {
		/* Build a simple enemy clan for testing.
		   Member must be malloc'd because FreeClanMembers calls free(). */
		struct clan EnemyClan = {0};
		struct pc *enemy = calloc(1, sizeof(struct pc));
		strlcpy(EnemyClan.szName, "Monsters", sizeof(EnemyClan.szName));
		EnemyClan.Member[0] = enemy;
		strlcpy(enemy->szName, "Goblin", sizeof(enemy->szName));
		enemy->HP = enemy->MaxHP = (int16_t)(20 + mine_level * 5);
		enemy->SP = enemy->MaxSP = 0;
		enemy->Status = Here;
		enemy->Attributes[ATTR_STRENGTH]  = (int8_t)(3 + mine_level);
		enemy->Attributes[ATTR_DEXTERITY] = (int8_t)(3 + mine_level);
		enemy->Attributes[ATTR_AGILITY]   = (int8_t)(3 + mine_level);
		enemy->Attributes[ATTR_WISDOM]    = 1;
		enemy->MyClan = &EnemyClan;
		for (int j = 0; j < MAX_SPELLS_IN_EFFECT; j++)
			enemy->SpellsInEffect[j].SpellNum = -1;

		PClan.FightsLeft = Game.Data.MineFights;
		fight_result = Fight_Fight(&PClan, &EnemyClan, false, true, true);
		Fight_Heal(&PClan);
		Spells_ClearSpells(&PClan);
		FreeClanMembers(&EnemyClan);
		if (fight_result == FT_WON)
			PClan.Points += 5;
		else
			PClan.Points -= 3;
		PClan.FightsLeft--;
		Fight_CheckLevelUp();
	}
	else if (plat_stricmp(command, "levelup") == 0) {
		/* Set XP high enough to trigger level-up */
		for (int i = 0; i < 4; i++) {
			if (PClan.Member[i])
				PClan.Member[i]->Experience = 1000;
		}
		Fight_CheckLevelUp();
		fight_result = -1;  /* not a fight */
	}
	else {
		fprintf(stderr, "gtest: unknown command: %s\n", command);
		gtest_close();
		return 1;
	}

	/* ---- verify script consumed ---- */
	if (script_is_active()) {
		script_expect_end();
	}

	/* ---- state summary ---- */
	print_state_summary(fight_result);

	/* ---- cleanup ---- */
	gtest_close();
	if (script_is_active())
		script_close();

	return 0;
}
