/*
 * Quest Pack Debugger
 *
 * A standalone tool for testing quest packs and NPC interactions
 * without modifying any game state on disk.
 */

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"

#include "clansini.h"
#include "console.h"
#include "language.h"
#include "myopen.h"
#include "npc.h"

/* =========================================================================
 * Globals required by quests.c / npc.c / video.c
 * ========================================================================= */

struct clan    PClan;
struct village Village;
struct game    Game;
struct system  System = { false, "20260306", ".", 0, true, false };

bool   Verbose = false;

void Display(char *FileName)
{
	char buf[256];
	snprintf(buf, sizeof(buf), "|0E[MOCK Display] |07file=\"%s\"\n", FileName);
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

/* =========================================================================
 * fight.h mocks
 * ========================================================================= */

int16_t Fight_Fight(struct clan *Attacker, struct clan *Defender,
                    bool HumanEnemy, bool CanRun, bool AutoFight)
{
	(void)Attacker;
	(void)Defender;
	(void)HumanEnemy;
	(void)AutoFight;

	zputs("\n|0E[MOCK Fight] |07Choose fight result:\n");
	zputs("  |0AW|07) Win\n");
	zputs("  |0AL|07) Lose\n");
	if (CanRun)
		zputs("  |0AR|07) Run Away\n");

	char keys[4];
	int  k = 0;
	keys[k++] = 'W';
	keys[k++] = 'L';
	if (CanRun)
		keys[k++] = 'R';
	keys[k] = '\0';

	char c = GetAnswer(keys);
	c = (char)toupper((unsigned char)c);
	char buf[4];
	snprintf(buf, sizeof(buf), "%c\n", c);
	zputs(buf);

	if (c == 'W')
		return FT_WON;
	if (c == 'L')
		return FT_LOST;
	return FT_RAN;
}

void Fight_CheckLevelUp(void)
{
}

void FreeClan(struct clan *Clan)
{
	(void)Clan;
}

void FreeClanMembers(struct clan *Clan)
{
	for (int i = 0; i < MAX_MEMBERS; i++) {
		if (Clan->Member[i]) {
			free(Clan->Member[i]);
			Clan->Member[i] = NULL;
		}
	}
}

/* =========================================================================
 * news.h mocks
 * ========================================================================= */

void News_AddNews(char *szString)
{
	char buf[350];
	snprintf(buf, sizeof(buf), "|0E[MOCK News_AddNews] |07\"%s\"\n", szString);
	zputs(buf);
}

/* =========================================================================
 * items.h mocks
 * ========================================================================= */

void Items_GiveItem(char *szItemName)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "|0E[MOCK Items_GiveItem] |07item=\"%s\"\n", szItemName);
	zputs(buf);
}

/* =========================================================================
 * user.h mocks
 * ========================================================================= */

int16_t NumMembers(struct clan *Clan, bool OnlyOnesHere)
{
	int16_t n = 0;
	for (int i = 0; i < MAX_MEMBERS; i++) {
		if (!Clan->Member[i])
			continue;
		if (OnlyOnesHere && Clan->Member[i]->Status != Here)
			continue;
		n++;
	}
	return n;
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
	return true;
}

void ClanStats(struct clan *Clan, bool AllowModify)
{
	char buf[128];
	snprintf(buf, sizeof(buf),
	         "|0E[MOCK ClanStats] |07clan=\"%s\" AllowModify=%d\n",
	         Clan->szName, (int)AllowModify);
	zputs(buf);
}

/* =========================================================================
 * random.h mocks
 * ========================================================================= */
int my_random(int limit)
{
	// Don't mess around with garbage
	if (limit <= 0)
		System_Error("Bad value passed to my_random()");
	if (limit == 1)
		return 0;
	if (limit >= RAND_MAX)
		System_Error("Broken Random (range too small)!");
	char prompt[128];
	sprintf(prompt, "|0E[MOCK my_random] |07Random number 0 to %d: ", limit-1);
	zputs(prompt);
	char numstr[8] = {0};
	DosGetStr(numstr, 7, false);
	zputs("\n");
	int n = atoi(numstr);
	if (n < 0)
		return 0;
	if (n >= limit)
		return limit - 1;
	return n;
}

/* =========================================================================
 * Dummy PClan member setup
 * ========================================================================= */

static struct pc g_Members[4];

static const struct {
	const char *name;
	int8_t str, dex, agi, wis, arm, cha;
	int16_t hp, sp;
	int16_t cls;
} g_archetypes[4] = {
	{ "Fighter",  14,  8,  8,  4,  6,  6,  80, 10, 0 },
	{ "Mage",      4,  8,  8, 16,  2,  8,  30, 80, 1 },
	{ "Cleric",    8,  8,  8, 14,  4,  8,  50, 50, 2 },
	{ "Thief",     8, 14, 14,  8,  2, 10,  50, 20, 3 },
};

static void setup_dummy_pclan(void)
{
	memset(&PClan, 0, sizeof(PClan));
	strlcpy(PClan.szName,     "DebugClan", sizeof(PClan.szName));
	strlcpy(PClan.szUserName, "Debugger",  sizeof(PClan.szUserName));
	PClan.ClanID[0]          = 1;
	PClan.ClanID[1]          = 1;
	PClan.MineLevel          = 1;
	PClan.Empire.VaultGold   = 5000;
	PClan.FightsLeft         = 10;

	for (int i = 0; i < 4; i++) {
		struct pc *m = &g_Members[i];
		memset(m, 0, sizeof(*m));
		strlcpy(m->szName, g_archetypes[i].name, sizeof(m->szName));
		m->Attributes[ATTR_STRENGTH]  = g_archetypes[i].str;
		m->Attributes[ATTR_DEXTERITY] = g_archetypes[i].dex;
		m->Attributes[ATTR_AGILITY]   = g_archetypes[i].agi;
		m->Attributes[ATTR_WISDOM]    = g_archetypes[i].wis;
		m->Attributes[ATTR_ARMORSTR]  = g_archetypes[i].arm;
		m->Attributes[ATTR_CHARISMA]  = g_archetypes[i].cha;
		m->MaxHP      = g_archetypes[i].hp;
		m->HP         = m->MaxHP;
		m->MaxSP      = g_archetypes[i].sp;
		m->SP         = m->MaxSP;
		m->WhichClass = g_archetypes[i].cls;
		m->Level      = 1;
		m->Status     = Here;
		m->MyClan     = &PClan;
		for (int j = 0; j < 10; j++)
			m->SpellsInEffect[j].SpellNum = -1;
		PClan.Member[i] = m;
	}
	for (int i = 0; i < MAX_QUESTS; i++) {
		if (Quests[i].Active && Quests[i].Known)
			PClan.QuestsKnown[ i / 8 ] |= (char)(1 << (i % 8));
	}
}

static void setup_dummy_village(void)
{
	memset(&Village, 0, sizeof(Village));
	Village.Initialized = true;
	strlcpy(Village.Data.szName, "DebugVillage",
	        sizeof(Village.Data.szName));
}

/* =========================================================================
 * Flag display helpers
 * ========================================================================= */

static void print_flags(const char *label, uint8_t flags[8])
{
	char buf[80];
	snprintf(buf, sizeof(buf), "  |0A%-16s|07: ", label);
	zputs(buf);
	bool any = false;
	for (int i = 0; i < 64; i++) {
		if (flags[i / 8] & (uint8_t)(1u << (i % 8))) {
			snprintf(buf, sizeof(buf), "%d ", i);
			zputs(buf);
			any = true;
		}
	}
	if (!any) zputs("(none)");
	zputs("\n");
}

static void print_quest_flags(const char *label, uint8_t flags[8], int numquests)
{
	char buf[80];
	snprintf(buf, sizeof(buf), "  |0A%-10s|07: ", label);
	zputs(buf);
	bool any = false;
	for (int i = 0; i < numquests; i++) {
		if (flags[i / 8] & (uint8_t)(1u << (i % 8))) {
			snprintf(buf, sizeof(buf), "%d ", i + 1);
			zputs(buf);
			any = true;
		}
	}
	if (!any) zputs("(none)");
	zputs("\n");
}

static int count_active_quests(void)
{
	int n = 0;
	for (int i = 0; i < MAX_QUESTS; i++)
		if (Quests[i].Active) n++;
	return n;
}

/* =========================================================================
 * Main status screen
 * ========================================================================= */

static void show_main_screen(void)
{
	clrscr();
	zputs("|0BThe Clans Quest Pack Debugger\n");
	zputs("|07================================\n\n");

	zputs("|0EFlag Values:\n");
	print_flags("G (Global)",  Village.Data.GFlags);
	print_flags("H (Global Daily)",   Village.Data.HFlags);
	print_flags("P (Player)",  PClan.PFlags);
	print_flags("D (Player Daily)", PClan.DFlags);
	print_flags("T (Temp)",    Quests_TFlags);

	int nq = count_active_quests();
	zputs("\n|0EQuest Flags:\n");
	print_quest_flags("Known", PClan.QuestsKnown, nq);
	print_quest_flags("Done",  PClan.QuestsDone,  nq);

	char buf[160];
	zputs("\n|0EDebug Clan:\n");
	snprintf(buf, sizeof(buf),
	         "  Name: |0B%s|07  Gold: |0A%"PRId32
	         "|07  MineLevel: |0A%d|07\n",
	         PClan.szName, PClan.Empire.VaultGold, (int)PClan.MineLevel);
	zputs(buf);

	zputs("\n|07Options:\n");
	zputs("  |0AF|07) Edit Flags\n");
	zputs("  |0AG|07) Edit Gold / Mine Level\n");
	zputs("  |0AN|07) Chat with NPCs\n");
	zputs("  |0AQ|07) Run Quests\n");
	zputs("  |0AX|07) Exit\n");
	zputs("|0E> |07");
}

/* =========================================================================
 * Flag editor
 * ========================================================================= */

static void set_or_clear_flag(uint8_t flags[8], const char *label)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "\n|0EModify %s flag.\n", label);
	zputs(buf);
	zputs("  |0AS|07) Set   |0AC|07) Clear   |0AX|07) Cancel\n|0E> |07");

	char op = GetAnswer("SCX");
	op = (char)toupper((unsigned char)op);
	snprintf(buf, sizeof(buf), "%c\n", op);
	zputs(buf);
	if (op == 'X')
		return;

	zputs("Flag number (0-63): ");
	char numstr[8] = {0};
	DosGetStr(numstr, 7, false);
	zputs("\n");
	int n = atoi(numstr);
	if (n < 0 || n > 63) {
		zputs("|0CInvalid flag number.\n");
		return;
	}

	if (op == 'S')
		flags[n / 8] |= (uint8_t)(1u << (n % 8));
	else
		flags[n / 8] &= (uint8_t)~(1u << (n % 8));
}

static void edit_flags_menu(void)
{
	for (;;) {
		clrscr();
		zputs("|0BFlag Editor\n|07============\n\n");
		zputs("Which flag group?\n");
		zputs("  |0AG|07) Global (G)  |0AH|07) Global Daily (H)\n");
		zputs("  |0AP|07) Player (P)  |0AD|07) Player Daily (D)\n");
		zputs("  |0AT|07) Temp (T)\n");
		zputs("  |0AX|07) Back\n|0E> |07");

		char c = GetAnswer("GHPDTX");
		c = (char)toupper((unsigned char)c);
		char buf[4];
		snprintf(buf, sizeof(buf), "%c\n", c);
		zputs(buf);
		if (c == 'X') return;

		uint8_t   *flags = NULL;
		const char *label = "";
		switch (c) {
			case 'G':
				flags = Village.Data.GFlags;
				label = "Global (G)";
				break;
			case 'H':
				flags = Village.Data.HFlags;
				label = "Global Daily (H)";
				break;
			case 'P':
				flags = PClan.PFlags;
				label = "Player (P)";
				break;
			case 'D':
				flags = PClan.DFlags;
				label = "Player Daily (D)";
				break;
			case 'T':
				flags = Quests_TFlags;
				label = "Temp (T)";
				break;
			default:
				return;
		}
		set_or_clear_flag(flags, label);
		door_pause();
	}
}

/* =========================================================================
 * Gold / Mine Level editor
 * ========================================================================= */

static void edit_gold_mine(void)
{
	clrscr();
	zputs("|0BEdit Gold / Mine Level\n|07========================\n\n");

	char buf[128];
	snprintf(buf, sizeof(buf), "Current Gold: |0A%"PRId32"|07\n",
	         PClan.Empire.VaultGold);
	zputs(buf);
	long newgold = DosGetLong("New Gold (Enter to keep current): ",
	                          (long)PClan.Empire.VaultGold, 1000000000L);
	PClan.Empire.VaultGold = (int32_t)newgold;

	snprintf(buf, sizeof(buf), "Current Mine Level: |0A%d|07\n",
	         (int)PClan.MineLevel);
	zputs(buf);
	long newmine = DosGetLong("New Mine Level (Enter to keep current): ",
	                          (long)PClan.MineLevel, 20L);
	PClan.MineLevel = (int16_t)newmine;
}

/* =========================================================================
 * NPC browser / chat sub-menu
 * ========================================================================= */

struct DBGNPCEntry {
	char   szName[20];
	char   szIndex[20];
	int16_t WhereWander;
	int16_t OddsOfSeeing;
};

static int collect_npcs(struct DBGNPCEntry *entries, int maxentries)
{
	int n = 0;
	struct NPCInfo *info = malloc(sizeof(struct NPCInfo));
	CheckMem(info);

	for (int fi = 0; fi < MAX_NPCFILES && n < maxentries; fi++) {
		if (!IniFile.pszNPCFileName[fi])
			break;

		struct FileHeader fh;
		MyOpen(IniFile.pszNPCFileName[fi], "rb", &fh);
		if (!fh.fp)
			continue;

		for (;;) {
			if (ftell(fh.fp) >= fh.lEnd)
				break;
			uint8_t nbuf[BUF_SIZE_NPCInfo];
			if (fread(nbuf, sizeof(nbuf), 1, fh.fp) == 0)
				break;
			s_NPCInfo_d(nbuf, sizeof(nbuf), info);

			strlcpy(entries[n].szName,  info->szName,  sizeof(entries[n].szName));
			strlcpy(entries[n].szIndex, info->szIndex, sizeof(entries[n].szIndex));
			entries[n].WhereWander  = info->WhereWander;
			entries[n].OddsOfSeeing = info->OddsOfSeeing;
			n++;
			if (n >= maxentries)
				break;
		}
		fclose(fh.fp);
	}

	free(info);
	return n;
}

static const char *wander_name(int16_t w)
{
	switch (w) {
		case WN_NONE:     return "Never";
		case WN_CHURCH:   return "Church";
		case WN_STREET:   return "Street";
		case WN_MARKET:   return "Market";
		case WN_TOWNHALL: return "TownHall";
		case WN_THALL:    return "Training";
		case WN_MINE:     return "Mine";
		default:          return "Other";
	}
}

static void npc_submenu(void)
{
	struct DBGNPCEntry entries[MAX_NPCS * MAX_NPCFILES];
	int n = collect_npcs(entries, MAX_NPCS * MAX_NPCFILES);

	for (;;) {
		clrscr();
		zputs("|0BNPC Chat Debugger\n|07===================\n\n");

		if (n == 0) {
			zputs("|0CNo NPCs found (check clans.ini NpcFile entries).\n");
			door_pause();
			return;
		}

		for (int i = 0; i < n; i++) {
			char buf[128];
			snprintf(buf, sizeof(buf),
			         "  |0A%2d|07) |0B%-20s|07 "
			         "Index:%-20s Wander:%-9s Odds:%d%%\n",
			         i + 1,
			         entries[i].szName,
			         entries[i].szIndex,
			         wander_name(entries[i].WhereWander),
			         (int)entries[i].OddsOfSeeing);
			zputs(buf);
		}
		zputs("\n  |0A 0|07) Back\n|0E> |07");

		char numstr[8] = {0};
		DosGetStr(numstr, 7, false);
		zputs("\n");
		int choice = atoi(numstr);
		if (choice == 0)
			return;
		if (choice < 1 || choice > n) {
			zputs("|0CInvalid choice.\n");
			door_pause();
			continue;
		}

		NPC_ChatNPC(entries[choice - 1].szIndex);
		door_pause();
	}
}

/* =========================================================================
 * Quest runner sub-menu
 * ========================================================================= */

static void quest_submenu(void)
{
	/* Build the active-quest index map once; quests don't change at runtime. */
	int nq = 0;
	int qmap[MAX_QUESTS];
	for (int i = 0; i < MAX_QUESTS; i++) {
		if (Quests[i].Active)
			qmap[nq++] = i;
	}

	if (nq == 0) {
		zputs("|0CNo quests found (check quests.ini).\n");
		door_pause();
		return;
	}

	for (;;) {
		clrscr();
		zputs("|0BQuest Runner Debugger\n|07=======================\n\n");

		for (int j = 0; j < nq; j++) {
			int i = qmap[j];
			bool known = (PClan.QuestsKnown[i / 8] &
			              (uint8_t)(1u << (i % 8))) != 0;
			bool done  = (PClan.QuestsDone[i / 8] &
			              (uint8_t)(1u << (i % 8))) != 0;

			char buf[256];
			snprintf(buf, sizeof(buf),
			         "  |0A%2d|07) |0B%-30s|07 [%s%s]\n",
			         j + 1,
			         Quests[i].pszQuestName
			             ? Quests[i].pszQuestName : "(unnamed)",
			         known ? "K" : "-",
			         done  ? "D" : "-");
			zputs(buf);
		}

		zputs("\n  Legend: |0AK|07=Known  |0AD|07=Done\n");
		zputs("  |0A 0|07) Back\n|0E> |07");

		char numstr[8] = {0};
		DosGetStr(numstr, 7, false);
		zputs("\n");
		int choice = atoi(numstr);
		if (choice == 0) return;
		if (choice < 1 || choice > nq) {
			zputs("|0CInvalid choice.\n");
			door_pause();
			continue;
		}

		int qi = qmap[choice - 1];

		zputs("Force quest |0AK|07)nown before running? (Y/N): ");
		char c = GetAnswer("YN");
		char buf[4];
		snprintf(buf, sizeof(buf), "%c\n", c);
		zputs(buf);
		if (toupper((unsigned char)c) == 'Y')
			PClan.QuestsKnown[qi / 8] |= (uint8_t)(1u << (qi % 8));

		ClearFlags(Quests_TFlags);
		bool result = RunEvent(false,
		                       Quests[qi].pszQuestFile,
		                       Quests[qi].pszQuestIndex,
		                       NULL, NULL);

		if (result) {
			PClan.QuestsDone[qi / 8]  |= (uint8_t)(1u << (qi % 8));
			PClan.QuestsKnown[qi / 8] |= (uint8_t)(1u << (qi % 8));
			zputs("\n|14Quest marked as completed.\n");
		}

		door_pause();
	}
}

/* =========================================================================
 * Initialisation / teardown
 * ========================================================================= */

static void dbg_init(void)
{
	Video_Init();
	ClansIni_Init();
	if (IniFile.pszLanguage)
		Language_Init(IniFile.pszLanguage);
	else
		Language_Init("strings.xl");
	Quests_Init();
	setup_dummy_pclan();
	setup_dummy_village();
}

static void dbg_close(void)
{
	Quests_Close();
	Language_Close();
	ClansIni_Close();
	ShowTextCursor(true);
	Video_Close();
}

/* =========================================================================
 * main()
 * ========================================================================= */

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	dbg_init();

	for (;;) {
		show_main_screen();

		char c = GetAnswer("FGNQX");
		c = (char)toupper((unsigned char)c);
		char buf[4]; snprintf(buf, sizeof(buf), "%c\n", c); zputs(buf);

		switch (c) {
			case 'F': edit_flags_menu();          break;
			case 'G': edit_gold_mine();               break;
			case 'N': npc_submenu();              break;
			case 'Q': quest_submenu();            break;
			case 'X':
				dbg_close();
				return 0;
		}
	}
}
