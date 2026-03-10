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
#include "help.h"
#include "input.h"
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

/* =========================================================================
 * Script engine
 * ========================================================================= */

static bool   script_mode = false;
static FILE  *script      = NULL;
static int    script_line = 0;

static bool script_read_line(char *buf, size_t sz)
{
	while (fgets(buf, (int)sz, script)) {
		script_line++;
		size_t len = strlen(buf);
		while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
			buf[--len] = '\0';
		if (len > 0)
			return true;
	}
	return false;
}

/*
 * Reads the next script line and verifies it has the expected Type prefix.
 * Returns a pointer to the value part (after '=').
 * On mismatch or EOF, prints a diagnostic to stderr and exits with code 2.
 */
static const char *script_consume(const char *expected_type)
{
	static char line[512];

	if (!script_read_line(line, sizeof(line))) {
		fprintf(stderr, "qtest: line %d: script ended unexpectedly, "
		        "expected %s\n", script_line + 1, expected_type);
		exit(2);
	}

	if (strcmp(line, "End") == 0) {
		fprintf(stderr, "qtest: line %d: script ended (End marker) while "
		        "expecting %s\n", script_line, expected_type);
		exit(2);
	}

	char *eq = strchr(line, '=');
	if (!eq) {
		fprintf(stderr, "qtest: line %d: malformed line (no '='): %s\n",
		        script_line, line);
		exit(2);
	}

	size_t tlen = strlen(expected_type);
	size_t alen = (size_t)(eq - line);
	if (alen != tlen || strncasecmp(line, expected_type, tlen) != 0) {
		char actual[64] = "(unknown)";
		if (alen < sizeof(actual)) {
			memcpy(actual, line, alen);
			actual[alen] = '\0';
		}
		fprintf(stderr, "qtest: line %d: type mismatch: "
		        "expected %s, got %s\n",
		        script_line, expected_type, actual);
		exit(2);
	}

	return eq + 1;
}

/*
 * Called after the event/NPC chat returns.  Reads the next script line,
 * expects it to be "End".  If not, prints a diagnostic and exits with
 * code 3.
 */
static void script_expect_end(void)
{
	char line[512];

	if (!script_read_line(line, sizeof(line))) {
		fprintf(stderr, "qtest: line %d: expected End but reached "
		        "end of file\n", script_line + 1);
		exit(3);
	}
	if (strcmp(line, "End") != 0) {
		fprintf(stderr, "qtest: line %d: expected End, got: %s\n",
		        script_line, line);
		exit(3);
	}
}

/* =========================================================================
 * Hook callbacks (called by subsystems in script mode)
 * ========================================================================= */

static char hook_get_answer(const char *szAllowableChars)
{
	const char *val = script_consume("Choice");
	if (val[0] == '\0') {
		fprintf(stderr, "qtest: line %d: Choice= has no value\n",
		        script_line);
		exit(2);
	}
	char c = (char)toupper((unsigned char)val[0]);
	/* validate — check both cases since allowable chars may be any case */
	const char *p = szAllowableChars;
	while (*p && toupper((unsigned char)*p) != c)
		p++;
	if (!*p) {
		fprintf(stderr, "qtest: line %d: Choice='%c' not in "
		        "allowable set \"%s\"\n",
		        script_line, c, szAllowableChars);
		exit(2);
	}
	return c;
}

static void hook_dos_get_str(char *InputStr, int16_t MaxChars, bool HiBit)
{
	(void)HiBit;
	const char *val = script_consume("String");
	strlcpy(InputStr, val, (size_t)MaxChars + 1);
}

static long hook_dos_get_long(const char *Prompt, long DefaultVal, long Maximum)
{
	(void)Prompt;
	(void)DefaultVal;
	const char *val = script_consume("Number");
	long n = atol(val);
	if (n > Maximum)
		n = Maximum;
	return n;
}

static int16_t hook_get_string_choice(const char **apszChoices,
                                      int16_t NumChoices,
                                      bool AllowBlank)
{
	const char *val = script_consume("Topic");

	if (val[0] == '\0') {
		if (!AllowBlank) {
			fprintf(stderr, "qtest: line %d: empty Topic= but "
			        "AllowBlank is false\n", script_line);
			exit(2);
		}
		return -1;
	}

	/* exact match first, then unambiguous prefix */
	for (int i = 0; i < NumChoices; i++) {
		if (strcasecmp(val, apszChoices[i]) == 0)
			return (int16_t)i;
	}

	size_t vlen = strlen(val);
	int found  = -1;
	int matches = 0;
	for (int i = 0; i < NumChoices; i++) {
		if (strncasecmp(val, apszChoices[i], vlen) == 0) {
			found = i;
			matches++;
		}
	}
	if (matches == 0) {
		fprintf(stderr, "qtest: line %d: Topic=\"%s\" not found\n",
		        script_line, val);
		exit(2);
	}
	if (matches > 1) {
		fprintf(stderr, "qtest: line %d: Topic=\"%s\" is ambiguous\n",
		        script_line, val);
		exit(2);
	}
	return (int16_t)found;
}

/* =========================================================================
 * fight.h mocks
 * ========================================================================= */

int16_t Fight_Fight(struct clan *Attacker, struct clan *Defender,
                    bool HumanEnemy, bool CanRun, bool AutoFight)
{
	(void)HumanEnemy;
	(void)AutoFight;

	char buf[128];

	if (script_mode) {
		const char *val = script_consume("Fight");
		char c = (char)toupper((unsigned char)val[0]);

		/* each living attacker member loses 1 HP and 1 SP */
		for (int i = 0; i < MAX_MEMBERS; i++) {
			struct pc *m = Attacker->Member[i];
			if (!m || m->HP <= 0)
				continue;
			if (m->HP > 0) m->HP--;
			if (m->SP > 0) m->SP--;
		}

		snprintf(buf, sizeof(buf), "[MOCK Fight] outcome=%c\n", c);
		zputs(buf);

		if (c == 'W') return FT_WON;
		if (c == 'L') return FT_LOST;
		return FT_RAN;
	}

	if (Defender->szName[0]) {
		snprintf(buf, sizeof(buf), "\n|0E[MOCK Fight] |07vs |0B%s|07:\n",
		         Defender->szName);
	} else {
		snprintf(buf, sizeof(buf), "\n|0E[MOCK Fight] |07vs enemies:\n");
	}
	zputs(buf);
	for (int i = 0; i < MAX_MEMBERS; i++) {
		if (!Defender->Member[i])
			continue;
		snprintf(buf, sizeof(buf), "  |0B%-20s|07 Difficulty: |0A%d|07\n",
		         Defender->Member[i]->szName,
		         (int)Defender->Member[i]->Difficulty);
		zputs(buf);
	}

	zputs("Choose fight result:\n");
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
	snprintf(buf, sizeof(buf), "[MOCK News_AddNews] \"%s\"\n", szString);
	zputs(buf);
}

/* =========================================================================
 * items.h mocks
 * ========================================================================= */

void Items_GiveItem(char *szItemName)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "[MOCK Items_GiveItem] item=\"%s\"\n",
	         szItemName);
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
	/* Don't mess around with garbage */
	if (limit <= 0)
		System_Error("Bad value passed to my_random()");
	if (limit == 1)
		return 0;
	if (limit >= RAND_MAX)
		System_Error("Broken Random (range too small)!");

	if (script_mode) {
		const char *val = script_consume("Random");
		int n = atoi(val);
		if (n < 0)
			return 0;
		if (n >= limit)
			return limit - 1;
		return n;
	}

	char prompt[128];
	sprintf(prompt, "|0E[MOCK my_random] |07Random number 0 to %d: ",
	        limit - 1);
	zputs(prompt);
	char numstr[8] = {0};
	DosGetStr(numstr, 7, false);
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
		m->HP         = m->MaxHP / 2;
		m->MaxSP      = g_archetypes[i].sp;
		m->SP         = m->MaxSP / 2;
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
 * Flag display helpers (interactive mode)
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
 * State summary output (script mode)
 * ========================================================================= */

static void print_flag_summary(uint8_t flags[8])
{
	bool any = false;
	for (int i = 0; i < 64; i++) {
		if (flags[i / 8] & (uint8_t)(1u << (i % 8))) {
			if (any)
				fputc(',', stderr);
			fprintf(stderr, "%d", i);
			any = true;
		}
	}
	fputc('\n', stderr);
}

static void print_quest_summary(const char *key, uint8_t flags[8])
{
	fprintf(stderr, "%s=", key);
	bool any = false;
	for (int i = 0; i < MAX_QUESTS; i++) {
		if (!Quests[i].Active)
			continue;
		if (flags[i / 8] & (uint8_t)(1u << (i % 8))) {
			if (any)
				fputc(',', stderr);
			fprintf(stderr, "%d", i + 1);
			any = true;
		}
	}
	fputc('\n', stderr);
}

static void print_state_summary(void)
{
	fputs("GFlags=", stderr); print_flag_summary(Village.Data.GFlags);
	fputs("HFlags=", stderr); print_flag_summary(Village.Data.HFlags);
	fputs("PFlags=", stderr); print_flag_summary(PClan.PFlags);
	fputs("DFlags=", stderr); print_flag_summary(PClan.DFlags);
	fputs("TFlags=", stderr); print_flag_summary(Quests_TFlags);

	fprintf(stderr, "Gold=%"PRId32"\n",      PClan.Empire.VaultGold);
	fprintf(stderr, "MineLevel=%d\n",        (int)PClan.MineLevel);
	fprintf(stderr, "Points=%"PRId32"\n",    PClan.Points);
	fprintf(stderr, "FightsLeft=%d\n",       (int)PClan.FightsLeft);
	fprintf(stderr, "Followers=%"PRId32"\n", PClan.Empire.Army.Followers);

	print_quest_summary("QuestsKnown", PClan.QuestsKnown);
	print_quest_summary("QuestsDone",  PClan.QuestsDone);

	for (int i = 0; i < 4; i++) {
		struct pc *m = PClan.Member[i];
		if (!m)
			continue;
		fprintf(stderr, "Member%d.Name=%s\n",  i, m->szName);
		fprintf(stderr, "Member%d.HP=%d\n",    i, (int)m->HP);
		fprintf(stderr, "Member%d.MaxHP=%d\n", i, (int)m->MaxHP);
		fprintf(stderr, "Member%d.SP=%d\n",    i, (int)m->SP);
		fprintf(stderr, "Member%d.MaxSP=%d\n", i, (int)m->MaxSP);
		fprintf(stderr, "Member%d.XP=%"PRId32"\n", i, m->Experience);
		fprintf(stderr, "Member%d.Level=%d\n", i, (int)m->Level);
		fprintf(stderr, "Member%d.Cha=%d\n",   i, (int)m->Attributes[ATTR_CHARISMA]);
	}
}

/* =========================================================================
 * State argument helpers
 * ========================================================================= */

static void parse_flag_list(const char *list, uint8_t flags[8])
{
	char buf[256];
	strlcpy(buf, list, sizeof(buf));
	for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
		int n = atoi(tok);
		if (n >= 0 && n < 64)
			flags[n / 8] |= (uint8_t)(1u << (n % 8));
	}
}

static void parse_quest_list(const char *list, uint8_t qflags[8])
{
	char buf[256];
	strlcpy(buf, list, sizeof(buf));
	for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
		int n = atoi(tok) - 1;  /* convert 1-based to 0-based */
		if (n >= 0 && n < MAX_QUESTS)
			qflags[n / 8] |= (uint8_t)(1u << (n % 8));
	}
}

/* =========================================================================
 * Main status screen (interactive mode)
 * ========================================================================= */

static void show_main_screen(void)
{
	clrscr();
	zputs("|0BThe Clans Quest Pack Debugger\n");
	zputs("|07================================\n\n");

	zputs("|0EFlag Values:\n");
	print_flags("G (Global)",      Village.Data.GFlags);
	print_flags("H (Global Daily)", Village.Data.HFlags);
	print_flags("P (Player)",      PClan.PFlags);
	print_flags("D (Player Daily)", PClan.DFlags);
	print_flags("T (Temp)",        Quests_TFlags);

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
 * Flag editor (interactive mode)
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
			case 'G': flags = Village.Data.GFlags; label = "Global (G)";      break;
			case 'H': flags = Village.Data.HFlags; label = "Global Daily (H)"; break;
			case 'P': flags = PClan.PFlags;        label = "Player (P)";      break;
			case 'D': flags = PClan.DFlags;        label = "Player Daily (D)"; break;
			case 'T': flags = Quests_TFlags;       label = "Temp (T)";        break;
			default:  return;
		}
		set_or_clear_flag(flags, label);
		door_pause();
	}
}

/* =========================================================================
 * Gold / Mine Level editor (interactive mode)
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
 * NPC browser / chat sub-menu (interactive mode)
 * ========================================================================= */

struct DBGNPCEntry {
	char    szName[20];
	char    szIndex[20];
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

			strlcpy(entries[n].szName,  info->szName,
			        sizeof(entries[n].szName));
			strlcpy(entries[n].szIndex, info->szIndex,
			        sizeof(entries[n].szIndex));
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
 * Quest runner sub-menu (interactive mode)
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
			         "   |07[%s%s] |0A%2d|07) |0B%-30s\n",
			         known ? "K" : "-",
			         done  ? "D" : "-",
			         j + 1,
			         Quests[i].pszQuestName
			             ? Quests[i].pszQuestName : "(unnamed)");
			zputs(buf);
		}

		zputs("\n  |07Legend: |0AK|07=Known  |0AD|07=Done\n");
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
		const char *qname = Quests[qi].pszQuestName
		                    ? Quests[qi].pszQuestName : "(unnamed)";

		for (;;) {
			char buf[256];
			snprintf(buf, sizeof(buf),
			         "\n|0B%s\n|07  |0AR|07) Run  |0AD|07) Description"
			         "  |0AB|07) Back\n|0E> |07",
			         qname);
			zputs(buf);

			char act = GetAnswer("RDB");
			act = (char)toupper((unsigned char)act);
			snprintf(buf, sizeof(buf), "%c\n", act);
			zputs(buf);

			if (act == 'B')
				break;

			if (act == 'D') {
				Help(qname, (char[]){ "quests.hlp" });
				door_pause();
				continue;
			}

			/* act == 'R': run the quest */
			if (!(PClan.QuestsKnown[qi / 8] & (uint8_t)(1u << (qi % 8)))) {
				zputs("Force quest |0AK|07)nown before running? (Y/N): ");
				char c = GetAnswer("YN");
				snprintf(buf, sizeof(buf), "%c\n", c);
				zputs(buf);
				if (toupper((unsigned char)c) == 'Y')
					PClan.QuestsKnown[qi / 8] |= (uint8_t)(1u << (qi % 8));
			}

			ClearFlags(Quests_TFlags);
			zputs("\n|0BDescription (from quests.hlp)\n|07=============================\n\n");
			Help(qname, (char[]){ "quests.hlp" });
			zputs("\n");
			zputs("\n|0BQuest Begins Here\n|07=================\n\n");
			bool result = RunEvent(false, true,
			                       Quests[qi].pszQuestFile,
			                       Quests[qi].pszQuestIndex,
			                       NULL, NULL);

			if (result) {
				PClan.QuestsDone[qi / 8]  |= (uint8_t)(1u << (qi % 8));
				PClan.QuestsKnown[qi / 8] |= (uint8_t)(1u << (qi % 8));
				zputs("\n|14Quest marked as completed.\n");
			}

			door_pause();
			break;
		}
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
	/* ---- state arg accumulators ---- */
	uint8_t gflags[8] = {0}, hflags[8] = {0};
	uint8_t pflags[8] = {0}, dflags[8] = {0}, tflags[8] = {0};
	uint8_t qknown[8] = {0}, qdone[8]  = {0};
	long    arg_gold  = -1;
	long    arg_mine  = -1;
	long    arg_cha   = -1;

	/* ---- target / script ---- */
	char npc_index[64]   = {0};
	char event_file[256] = {0};
	char event_label[64] = {0};
	char script_path[256] = {0};
	bool have_npc   = false;
	bool have_event = false;

	/* ---- argument parsing ---- */
	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (a[0] != '-' || a[1] == '\0') {
			fprintf(stderr, "qtest: unexpected argument: %s\n", a);
			return 1;
		}
		switch (a[1]) {
			case 'G': parse_flag_list(a + 2, gflags); break;
			case 'H': parse_flag_list(a + 2, hflags); break;
			case 'P': parse_flag_list(a + 2, pflags); break;
			case 'D': parse_flag_list(a + 2, dflags); break;
			case 'T': parse_flag_list(a + 2, tflags); break;
			case 'g': arg_gold = atol(a + 2); break;
			case 'm': arg_mine = atol(a + 2); break;
			case 'c': arg_cha  = atol(a + 2); break;
			case 'q': parse_quest_list(a + 2, qknown); break;
			case 'Q': parse_quest_list(a + 2, qdone);  break;
			case 'n':
				if (i + 1 >= argc) {
					fprintf(stderr, "qtest: -n requires an index\n");
					return 1;
				}
				strlcpy(npc_index, argv[++i], sizeof(npc_index));
				have_npc = true;
				break;
			case 'e':
				if (i + 2 >= argc) {
					fprintf(stderr,
					        "qtest: -e requires a file and a label\n");
					return 1;
				}
				strlcpy(event_file,  argv[++i], sizeof(event_file));
				strlcpy(event_label, argv[++i], sizeof(event_label));
				have_event = true;
				break;
			case 's':
				if (i + 1 >= argc) {
					fprintf(stderr, "qtest: -s requires a file\n");
					return 1;
				}
				strlcpy(script_path, argv[++i], sizeof(script_path));
				script_mode = true;
				break;
			default:
				fprintf(stderr, "qtest: unknown option: %s\n", a);
				return 1;
		}
	}

	/* ---- validate script mode requirements ---- */
	if (script_mode) {
		if (have_npc && have_event) {
			fprintf(stderr, "qtest: -n and -e are mutually exclusive\n");
			return 1;
		}
		if (!have_npc && !have_event) {
			fprintf(stderr,
			        "qtest: -s requires exactly one of -n or -e\n");
			return 1;
		}

		script = fopen(script_path, "r");
		if (!script) {
			fprintf(stderr, "qtest: cannot open script file \"%s\"\n",
			        script_path);
			return 1;
		}

		/* install hooks */
		Console_SetScriptMode(true);
		Console_SetGetAnswerHook(hook_get_answer);
		Video_SetScriptMode(true);
		Video_SetDosGetStrHook(hook_dos_get_str);
		Video_SetDosGetLongHook(hook_dos_get_long);
		Input_SetGetStringChoiceHook(hook_get_string_choice);
	}

	/* ---- initialise ---- */
	dbg_init();

	/* ---- apply state arguments ---- */
	for (int i = 0; i < 8; i++) Village.Data.GFlags[i] |= gflags[i];
	for (int i = 0; i < 8; i++) Village.Data.HFlags[i] |= hflags[i];
	for (int i = 0; i < 8; i++) PClan.PFlags[i]        |= pflags[i];
	for (int i = 0; i < 8; i++) PClan.DFlags[i]        |= dflags[i];
	for (int i = 0; i < 8; i++) Quests_TFlags[i]       |= tflags[i];
	if (arg_gold >= 0) PClan.Empire.VaultGold = (int32_t)arg_gold;
	if (arg_mine >= 0) PClan.MineLevel        = (int16_t)arg_mine;
	if (arg_cha  >= 0) PClan.Member[0]->Attributes[ATTR_CHARISMA] = (int8_t)arg_cha;
	for (int i = 0; i < 8; i++) PClan.QuestsKnown[i] |= qknown[i];
	for (int i = 0; i < 8; i++) {
		PClan.QuestsDone[i]  |= qdone[i];
		PClan.QuestsKnown[i] |= qdone[i];  /* done implies known */
	}

	/* ---- script mode: run target, check End, print summary, exit ---- */
	if (script_mode) {
		if (have_npc) {
			NPC_ChatNPC(npc_index);
		} else {
			ClearFlags(Quests_TFlags);
			if (RunEvent(false, false, event_file, event_label, NULL, NULL)) {
				/* Script mode runs a single event block directly, not via
				 * quests.ini, so there is no quest index to record.  Mirror
				 * what Quests_GoQuest does and set bit 0 (1-based index 1)
				 * so DoneQuest is visible in the state summary. */
				PClan.QuestsDone[0] |= 1;
			}
		}

		script_expect_end();
		print_state_summary();
		dbg_close();
		fclose(script);
		return 0;
	}

	/* ---- interactive mode ---- */
	for (;;) {
		show_main_screen();

		char c = GetAnswer("FGNQX");
		c = (char)toupper((unsigned char)c);
		char buf[4]; snprintf(buf, sizeof(buf), "%c\n", c); zputs(buf);

		switch (c) {
			case 'F': edit_flags_menu(); break;
			case 'G': edit_gold_mine();  break;
			case 'N': npc_submenu();     break;
			case 'Q': quest_submenu();   break;
			case 'X':
				dbg_close();
				return 0;
		}
	}
}
