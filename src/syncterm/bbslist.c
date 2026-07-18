/* Copyright (C), 2007 by Stephen Hurd */

#include <limits.h>
#include <dirwrap.h>
#include <filewrap.h>  /* chsize */
#include <ini_file.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "bbslist.h"
#include "host_ui.h"
#include "ini_crypt.h"
#include "comio.h"
#include "conn.h"
#include "named_str_list.h"
#include "syncterm.h"
#include "vidmodes.h"
#include "wren_token.h"

#ifdef _MSC_VER
	#pragma warning(disable : 4244 4267 4018)
#endif

struct sort_order_info {
	char *name;
	int flags;
	size_t offset;
	int length;
};

#define SORT_ORDER_REVERSED (1 << 0)
#define SORT_ORDER_STRING (1 << 1)
#define DEFAULT_SORT_ORDER_VALUE (0)

static struct sort_order_info sort_order[] = {
	{
		NULL,
		0,
		0,
		0
	},
	{ // 1
		"Entry Name",
		SORT_ORDER_STRING,
		offsetof(struct bbslist, name),
		sizeof(((struct bbslist *)NULL)->name)
	},
	{
		"Date Added",
		SORT_ORDER_REVERSED,
		offsetof(struct bbslist, added),
		sizeof(((struct bbslist *)NULL)->added)
	},
	{
		"Date Last Connected",
		SORT_ORDER_REVERSED,
		offsetof(struct bbslist, connected),
		sizeof(((struct bbslist *)NULL)->connected)
	},
	{
		"Total Calls",
		SORT_ORDER_REVERSED,
		offsetof(struct bbslist, calls),
		sizeof(((struct bbslist *)NULL)->calls)
	},
	{ // 5
		"Dialing List",
		0,
		offsetof(struct bbslist, type),
		sizeof(((struct bbslist *)NULL)->type)
	},
	{
		"Address",
		SORT_ORDER_STRING,
		offsetof(struct bbslist, addr),
		sizeof(((struct bbslist *)NULL)->addr)
	},
	{
		"Port",
		0,
		offsetof(struct bbslist, port),
		sizeof(((struct bbslist *)NULL)->port)
	},
	{
		"Username",
		SORT_ORDER_STRING,
		offsetof(struct bbslist, user),
		sizeof(((struct bbslist *)NULL)->user)
	},
	{
		"Password",
		SORT_ORDER_STRING,
		offsetof(struct bbslist, password),
		sizeof(((struct bbslist *)NULL)->password)
	},
	{ // 10
		"System Password",
		SORT_ORDER_STRING,
		offsetof(struct bbslist, syspass),
		sizeof(((struct bbslist *)NULL)->syspass)
	},
	{
		"Connection Type",
		0,
		offsetof(struct bbslist, conn_type),
		sizeof(((struct bbslist *)NULL)->conn_type)
	},
	{
		"Screen Mode",
		0,
		offsetof(struct bbslist, screen_mode),
		sizeof(((struct bbslist *)NULL)->screen_mode)
	},
	{
		"Status Line Visibility",
		0,
		offsetof(struct bbslist, nostatus),
		sizeof(((struct bbslist *)NULL)->nostatus)
	},
	{
		"Download Directory",
		SORT_ORDER_STRING,
		offsetof(struct bbslist, dldir),
		sizeof(((struct bbslist *)NULL)->dldir)
	},
	{
		"Upload Directory",
		SORT_ORDER_STRING,
		offsetof(struct bbslist, uldir),
		sizeof(((struct bbslist *)NULL)->uldir)
	},
	{
		"Log File",
		SORT_ORDER_STRING,
		offsetof(struct bbslist, logfile),
		sizeof(((struct bbslist *)NULL)->logfile)
	},
	{
		"Transfer Log Level",
		0,
		offsetof(struct bbslist, xfer_loglevel),
		sizeof(((struct bbslist *)NULL)->xfer_loglevel)
	},
	{
		"BPS Rate",
		0,
		offsetof(struct bbslist, bpsrate),
		sizeof(((struct bbslist *)NULL)->bpsrate)
	},
	{
		"ANSI Music",
		0,
		offsetof(struct bbslist, music),
		sizeof(((struct bbslist *)NULL)->music)
	},
	{ // 20
		"Address Family",
		0,
		offsetof(struct bbslist, address_family),
		sizeof(((struct bbslist *)NULL)->address_family)
	},
	{
		"Font",
		SORT_ORDER_STRING,
		offsetof(struct bbslist, font),
		sizeof(((struct bbslist *)NULL)->font)
	},
	{
		"Hide Popups",
		0,
		offsetof(struct bbslist, hidepopups),
		sizeof(((struct bbslist *)NULL)->hidepopups)
	},
	{
		"RIP",
		0,
		offsetof(struct bbslist, rip),
		sizeof(((struct bbslist *)NULL)->rip)
	},
	{
		"Flow Control",
		0,
		offsetof(struct bbslist, flow_control),
		sizeof(((struct bbslist *)NULL)->flow_control)
	},
	{
		"Comment",
		SORT_ORDER_STRING,
		offsetof(struct bbslist, comment),
		sizeof(((struct bbslist *)NULL)->comment)
	},
	{
		"Force LCF",
		0,
		offsetof(struct bbslist, force_lcf),
		sizeof(((struct bbslist *)NULL)->force_lcf)
	},
	{
		"Yellow Is Yellow",
		0,
		offsetof(struct bbslist, yellow_is_yellow),
		sizeof(((struct bbslist *)NULL)->yellow_is_yellow)
	},
	{
		"Terminal Type",
		0,
		offsetof(struct bbslist, term_name),
		sizeof(((struct bbslist *)NULL)->term_name)
	},
	{
		"LF Expand",
		0,
		offsetof(struct bbslist, lf_expand),
		sizeof(((struct bbslist *)NULL)->lf_expand)
	},
	{ // 30
		"Explicit Sort Order",
		0,
		offsetof(struct bbslist, sort_order),
		sizeof(((struct bbslist *)NULL)->sort_order)
	},
	{
		NULL,
		0,
		0,
		0
	}
};

int sortorder[sizeof(sort_order) / sizeof(struct sort_order_info)];

#define SORT_PROFILE_NAME_MAX 19

static named_string_t **sort_profiles = NULL;
static int              active_profile = 0;
static bool             sort_profiles_dirty = false;

static const struct {
	const char *name;
	const char *value;
} default_sort_profiles[] = {
	{ "Name",           "29,1" },
	{ "Last Connected", "29,3,1" },
	{ "Most Called",    "29,4,1" },
	{ "Date Added",     "29,2,1" },
};
#define NUM_DEFAULT_SORT_PROFILES (sizeof(default_sort_profiles) / sizeof(default_sort_profiles[0]))

char *screen_modes[] = {
	"Current", "80x25", "LCD 80x25", "80x28", "80x30", "80x43", "80x50", "80x60", "132x37 (16:9)", "132x52 (5:4)",
	"132x25", "132x28", "132x30", "132x34", "132x43", "132x50", "132x60", "C64", "C128 (40col)", "C128 (80col)",
	"Atari", "Atari XEP80", "Custom", "EGA 80x25", "VGA 80x25", "Prestel", "BBC Micro Mode 7", "Atari ST 40x25", "Atari ST 80x25", "Atari ST 80x25 Mono", NULL
};
char *screen_modes_enum[] = {
	"Current", "80x25", "LCD80x25", "80x28", "80x30", "80x43", "80x50", "80x60", "132x37", "132x52", "132x25",
	"132x28", "132x30", "132x34", "132x43", "132x50", "132x60", "C64", "C128-40col", "C128-80col", "Atari",
	"Atari-XEP80", "Custom", "EGA80x25", "VGA80x25", "Prestel", "BBCMicro7", "AtariST40x25", "AtariST80x25", "AtariST80x25Mono", NULL
};

char *log_levels[] = {
	"Emergency", "Alert", "Critical", "Error", "Warning", "Notice", "Info", "Debug", NULL
};
char *rate_names[] = {
	"300", "600", "1200", "2400", "4800", "9600", "19200", "38400", "57600", "76800", "115200", "Current", NULL
};
int rates[] = {300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 76800, 115200, 0};

static char *rip_versions[] = {"Off", "RIPv1", "RIPv3", NULL};

static char *fc_enum[] = {"RTSCTS", "XONXOFF", "RTSCTS_XONXOFF", "None", NULL};

char *music_names[] = {"ESC [ | only", "BANSI Style", "All ANSI Music enabled", NULL};
char music_helpbuf[] = "`ANSI Music Setup`\n\n"
                       "~ ESC [ | only ~            Only CSI | (SyncTERM) ANSI music is supported.\n"
                       "                          Enables Delete Line\n"
                       "~ BANSI Style ~             Also enables BANSI-Style (CSI N)\n"
                       "                          Enables Delete Line\n"
                       "~ All ANSI Music Enabled ~  Enables both CSI M and CSI N ANSI music.\n"
                       "                          Delete Line is disabled.\n"
                       "\n"
                       "So-Called ANSI Music has a long and troubled history.  Although the\n"
                       "original ANSI standard has well defined ways to provide private\n"
                       "extensions to the spec, none of these methods were used.  Instead,\n"
                       "so-called ANSI music replaced the Delete Line ANSI sequence.  Many\n"
                       "full-screen editors use DL, and to this day, some programs (Such as\n"
                       "BitchX) require it to run.\n\n"
                       "To deal with this, BananaCom decided to use what *they* thought was an\n"
                       "unspecified escape code ESC[N for ANSI music.  Unfortunately, this is\n"
                       "broken also.  Although rarely implemented in BBS clients, ESC[N is\n"
                       "the erase field sequence.\n\n"
                       "SyncTERM has now defined a third ANSI music sequence which *IS* legal\n"
                       "according to the ANSI spec.  Specifically ESC[|.";

static char *address_families[] = {"PerDNS", "IPv4", "IPv6", NULL};

static char *parity_enum[] = {"None", "Even", "Odd", NULL};

char *scaling_names[] = {"Blocky", "Pointy", "External", NULL};

ini_style_t ini_style = {
	/* key_len */
	15,

	/* key_prefix */ "\t",

	/* section_separator */ "\n",

	/* value_separarator */ NULL,

	/* bit_separator */ NULL
};

char list_password[1024] = "";
enum iniCryptAlgo list_algo = INI_CRYPT_ALGO_NONE;
int list_keysize = 0;

static int
fc_to_enum(int fc)
{
	switch (fc & (COM_FLOW_CONTROL_RTS_CTS | COM_FLOW_CONTROL_XON_OFF)) {
		case COM_FLOW_CONTROL_RTS_CTS:
			return 0;
		case COM_FLOW_CONTROL_XON_OFF:
			return 1;
		case (COM_FLOW_CONTROL_RTS_CTS | COM_FLOW_CONTROL_XON_OFF):
			return 2;
		default:
			return 3;
	}
}

static int
fc_from_enum(int fc_enum)
{
	switch (fc_enum) {
		case 0:
			return COM_FLOW_CONTROL_RTS_CTS;
		case 1:
			return COM_FLOW_CONTROL_XON_OFF;
		case 2:
			return COM_FLOW_CONTROL_RTS_CTS | COM_FLOW_CONTROL_XON_OFF;
		default:
			return 0;
	}
}

int
get_rate_num(int rate)
{
	int i;

	for (i = 0; rates[i] && (!rate || rate > rates[i]); i++)
		;
	return i;
}

static int
intbufcmp(const void *a, const void *b, size_t size)
{
	int i;
	const unsigned char *ac = (const unsigned char *)a;
	const unsigned char *bc = (const unsigned char *)b;

#ifdef __BIG_ENDIAN__
	for (i = 0; i < size; i++) {
		bool last = i == 0;
#else
	for (i = size - 1; i >= 0; i--) {
		bool last = i == size - 1;
#endif
		if (ac[i] != bc[i]) {
			if (last) {
				return ((signed char)ac[i] - (signed char)bc[i]);
			}
			return ac[i] - bc[i];
		}
	}
	return 0;
}

static int
listcmp(const void *aptr, const void *bptr)
{
	const char *a = *(void **)(aptr);
	const char *b = *(void **)(bptr);
	int i;
	int item;
	int reverse;
	int ret = 0;

	for (i = 0; i < sizeof(sort_order) / sizeof(struct sort_order_info); i++) {
		item = abs(sortorder[i]);
		reverse = (sortorder[i] < 0 ? 1 : 0) ^ ((sort_order[item].flags & SORT_ORDER_REVERSED) ? 1 : 0);
		if (sort_order[item].name) {
			if (sort_order[item].flags & SORT_ORDER_STRING)
				ret = stricmp(a + sort_order[item].offset, b + sort_order[item].offset);
			else {
				ret =
				        intbufcmp(a + sort_order[item].offset,
				                  b + sort_order[item].offset,
				                  sort_order[item].length);
			}
			if (ret) {
				if (reverse)
					ret = 0 - ret;
				return ret;
			}
		}
		else
			break;
	}
	/* Unconditional name tiebreaker for stable sort (qsort is not stable) */
	if (ret == 0)
		ret = stricmp(a + sort_order[1].offset, b + sort_order[1].offset);
	return ret;
}

static void
sort_list(struct bbslist **list, int *listcount)
{
	qsort(list, *listcount, sizeof(struct bbslist *), listcmp);
}


/*
 * Parse a comma-separated string of signed integers into an order array.
 */
static void
parse_sort_value(const char *value, int *order, int maxfields)
{
	str_list_t parts;
	int i = 0;

	memset(order, 0, sizeof(int) * maxfields);
	parts = strListSplitCopy(NULL, value, ",");
	if (parts) {
		char *s;
		while ((s = strListRemove(&parts, 0)) != NULL && i < maxfields - 1) {
			order[i++] = atoi(s);
			free(s);
		}
		strListFree(&parts);
	}
}

/*
 * Serialize an order array back to a comma-separated string.
 * Returns a malloc'd string; caller must free().
 */
static char *
serialize_sort_order(const int *order)
{
	str_list_t parts;
	char num[16];
	char buf[256];
	int i;

	parts = strListInit();
	for (i = 0; sort_order[abs(order[i])].name != NULL; i++) {
		sprintf(num, "%d", order[i]);
		strListPush(&parts, num);
	}
	strListJoin(parts, buf, sizeof(buf), ",");
	strListFree(&parts);
	return strdup(buf);
}

/*
 * Load default sort profiles into sort_profiles.
 */
static void
load_default_sort_profiles(void)
{
	size_t i;

	iniFreeNamedStringList(sort_profiles);
	sort_profiles = NULL;
	for (i = 0; i < NUM_DEFAULT_SORT_PROFILES; i++)
		namedStrListInsert(&sort_profiles, default_sort_profiles[i].name,
		                   default_sort_profiles[i].value, i);
	active_profile = 0;
}

/*
 * Write sort profiles, active profile name, and current SortOrder to INI.
 * Does a single read-modify-write cycle to avoid inconsistent state.
 */
static bool
write_sort_profiles(void)
{
	char inipath[MAX_PATH + 1];
	FILE *fp;
	str_list_t inicontents;
	str_list_t sortorders;
	char str[64];
	int i;

	if (safe_mode)
		return true;
	get_syncterm_filename(inipath, sizeof(inipath), SYNCTERM_PATH_INI, false);
	if ((fp = fopen(inipath, "r")) != NULL) {
		inicontents = iniReadFile(fp);
		fclose(fp);
	}
	else
		inicontents = strListInit();

	/* Write [SortProfiles] section */
	iniRemoveSection(&inicontents, "SortProfiles");
	iniAppendSectionWithNamedStrings(&inicontents, "SortProfiles",
	                                 (const named_string_t **)sort_profiles, &ini_style);

	/* Write ActiveSortProfile and SortOrder in [SyncTERM] */
	if (sort_profiles && sort_profiles[active_profile])
		iniSetString(&inicontents, "SyncTERM", "ActiveSortProfile",
		             sort_profiles[active_profile]->name, &ini_style);

	sortorders = strListInit();
	for (i = 0; sort_order[abs(sortorder[i])].name != NULL; i++) {
		sprintf(str, "%d", sortorder[i]);
		strListPush(&sortorders, str);
	}
	iniSetStringList(&inicontents, "SyncTERM", "SortOrder", ",", sortorders, &ini_style);
	strListFree(&sortorders);

	bool success = false;
	if ((fp = fopen(inipath, "w")) != NULL) {
		success = iniWriteFile(fp, inicontents);
		if (fclose(fp) != 0)
			success = false;
	}
	strListFree(&inicontents);
	return success;
}

/*
 * Find a profile by name, returning its index or -1.
 */
static int
find_profile_by_name(const char *name)
{
	size_t i;

	if (sort_profiles == NULL || name == NULL)
		return -1;
	for (i = 0; sort_profiles[i] != NULL; i++) {
		if (stricmp(sort_profiles[i]->name, name) == 0)
			return (int)i;
	}
	return -1;
}

size_t
bbslist_sort_field_count(void)
{
	return (sizeof(sort_order) / sizeof(sort_order[0])) - 2;
}

const char *
bbslist_sort_field_name(int field)
{
	if (field == INT_MIN)
		return NULL;
	int index = abs(field);
	if (index <= 0 || (size_t)index >=
	    sizeof(sort_order) / sizeof(sort_order[0]))
		return NULL;
	return sort_order[index].name;
}

bool
bbslist_sort_field_reversed(int field)
{
	if (bbslist_sort_field_name(field) == NULL)
		return false;
	int index = abs(field);
	return (field < 0) !=
	    ((sort_order[index].flags & SORT_ORDER_REVERSED) != 0);
}

size_t
bbslist_sort_profile_count(void)
{
	size_t count = 0;
	if (sort_profiles != NULL) {
		while (sort_profiles[count] != NULL)
			count++;
	}
	return count;
}

const char *
bbslist_sort_profile_name(size_t index)
{
	return index < bbslist_sort_profile_count() ?
	    sort_profiles[index]->name : NULL;
}

size_t
bbslist_sort_profile_order(size_t index, int *order, size_t capacity)
{
	if (index >= bbslist_sort_profile_count() || order == NULL ||
	    capacity == 0)
		return 0;
	int parsed[sizeof(sort_order) / sizeof(sort_order[0])] = {0};
	parse_sort_value(sort_profiles[index]->value, parsed,
	    sizeof(parsed) / sizeof(parsed[0]));
	size_t count = 0;
	while (count < capacity && count <
	    sizeof(parsed) / sizeof(parsed[0]) &&
	    bbslist_sort_field_name(parsed[count]) != NULL) {
		order[count] = parsed[count];
		count++;
	}
	return count;
}

int
bbslist_active_sort_profile(void)
{
	return active_profile;
}

static bool
valid_profile_order(const int *order, size_t count)
{
	if (order == NULL || count == 0 ||
	    count > bbslist_sort_field_count())
		return false;
	for (size_t i = 0; i < count; i++) {
		if (bbslist_sort_field_name(order[i]) == NULL)
			return false;
		for (size_t j = 0; j < i; j++) {
			if (abs(order[i]) == abs(order[j]))
				return false;
		}
	}
	return true;
}

static bool
profile_name_available(const char *name, size_t except)
{
	if (name == NULL || name[0] == 0 || strlen(name) >
	    SORT_PROFILE_NAME_MAX)
		return false;
	for (size_t i = 0; i < bbslist_sort_profile_count(); i++) {
		if (i != except && stricmp(sort_profiles[i]->name, name) == 0)
			return false;
	}
	return true;
}

static char *
profile_order_string(const int *order, size_t count)
{
	int terminated[sizeof(sort_order) / sizeof(sort_order[0])] = {0};
	memcpy(terminated, order, count * sizeof(*order));
	return serialize_sort_order(terminated);
}

bool
bbslist_set_active_sort_profile(size_t index)
{
	if (index >= bbslist_sort_profile_count())
		return false;
	active_profile = (int)index;
	parse_sort_value(sort_profiles[index]->value, sortorder,
	    sizeof(sortorder) / sizeof(sortorder[0]));
	sort_profiles_dirty = true;
	return true;
}

bool
bbslist_add_sort_profile(size_t index, const char *name,
    const int *order, size_t count)
{
	size_t profile_count = bbslist_sort_profile_count();
	if (index > profile_count ||
	    !profile_name_available(name, SIZE_MAX) ||
	    !valid_profile_order(order, count))
		return false;
	char *value = profile_order_string(order, count);
	if (value == NULL)
		return false;
	named_string_t *inserted = namedStrListInsert(&sort_profiles, name,
	    value, index);
	free(value);
	if (inserted == NULL)
		return false;
	if ((size_t)active_profile >= index)
		active_profile++;
	sort_profiles_dirty = true;
	return true;
}

bool
bbslist_update_sort_profile(size_t index, const char *name,
    const int *order, size_t count)
{
	if (index >= bbslist_sort_profile_count() ||
	    !profile_name_available(name, index) ||
	    !valid_profile_order(order, count))
		return false;
	char *new_name = strdup(name);
	char *new_value = profile_order_string(order, count);
	if (new_name == NULL || new_value == NULL) {
		free(new_name);
		free(new_value);
		return false;
	}
	free(sort_profiles[index]->name);
	free(sort_profiles[index]->value);
	sort_profiles[index]->name = new_name;
	sort_profiles[index]->value = new_value;
	if ((int)index == active_profile) {
		parse_sort_value(new_value, sortorder,
		    sizeof(sortorder) / sizeof(sortorder[0]));
	}
	sort_profiles_dirty = true;
	return true;
}

bool
bbslist_delete_sort_profile(size_t index)
{
	size_t count = bbslist_sort_profile_count();
	if (count <= 1 || index >= count)
		return false;
	if (!namedStrListDelete(&sort_profiles, index))
		return false;
	if (active_profile > (int)index)
		active_profile--;
	else if (active_profile == (int)index &&
	    active_profile >= (int)(count - 1))
		active_profile = (int)count - 2;
	parse_sort_value(sort_profiles[active_profile]->value, sortorder,
	    sizeof(sortorder) / sizeof(sortorder[0]));
	sort_profiles_dirty = true;
	return true;
}

bool
bbslist_save_sort_profiles(void)
{
	if (!sort_profiles_dirty)
		return true;
	if (!write_sort_profiles())
		return false;
	sort_profiles_dirty = false;
	return true;
}

/*
 * Initialize sort profiles from INI, with migration from old SortOrder.
 * Called from syncterm.c after loading settings.
 */
void
init_sort_profiles(FILE *inifile)
{
	sort_profiles_dirty = false;
	if (sort_profiles)
		iniFreeNamedStringList(sort_profiles);

	sort_profiles = iniReadNamedStringList(inifile, "SortProfiles");

	if (sort_profiles == NULL || sort_profiles[0] == NULL) {
		/* No [SortProfiles] section — first run or upgrade */
		load_default_sort_profiles();

		/* Check if existing SortOrder matches a default */
		char *old_order = serialize_sort_order(sortorder);
		if (old_order) {
			int match = -1;
			size_t count;
			COUNT_LIST_ITEMS(sort_profiles, count);
			for (size_t i = 0; i < count; i++) {
				if (strcmp(sort_profiles[i]->value, old_order) == 0) {
					match = (int)i;
					break;
				}
			}
			if (match >= 0)
				active_profile = match;
			else if (old_order[0] && strcmp(old_order, "0") != 0)
				namedStrListInsert(&sort_profiles, "Custom", old_order, count);
			free(old_order);
		}
		return;
	}

	/* Load active profile by name */
	{
		char active_buf[INI_MAX_VALUE_LEN + 1];
		if (iniReadString(inifile, "SyncTERM", "ActiveSortProfile", "", active_buf)) {
			int idx = find_profile_by_name(active_buf);
			if (idx >= 0)
				active_profile = idx;
			else
				active_profile = 0;
		}
		else
			active_profile = 0;
	}
}

void
free_list(struct bbslist **list, int listcount)
{
	int i;

	for (i = 0; i < listcount; i++)
		FREE_AND_NULL(list[i]);
}

void
read_item(ini_fp_list_t *listfile, struct bbslist *entry, ini_lv_string_t *bbsname, int id, int type)
{
	char home[MAX_PATH + 1];
	str_list_t section;
	bool sys = (type == SYSTEM_BBSLIST);

	get_syncterm_filename(home, sizeof(home), SYNCTERM_DEFAULT_TRANSFER_PATH, false);
	if (bbsname != NULL) {
		size_t cpylen = bbsname->len;
		if (cpylen > LIST_NAME_MAX)
			cpylen = LIST_NAME_MAX;
		memcpy(entry->name, bbsname->str, cpylen);
		entry->name[cpylen] = 0;
	}
	section = iniGetFastParsedSectionLV(listfile, bbsname, true);
	iniGetSString(section, NULL, "Address", "", entry->addr, sizeof(entry->addr));
	entry->conn_type = iniGetEnum(section, NULL, "ConnectionType", conn_types_enum, CONN_TYPE_SSH);
	entry->flow_control = fc_from_enum(iniGetEnum(section, NULL, "FlowControl", fc_enum, 0));
	entry->port = iniGetUShortInt(section, NULL, "Port", conn_ports[entry->conn_type]);
	entry->added = iniGetDateTime(sys ? NULL : section, NULL, "Added", 0);
	entry->connected = iniGetDateTime(sys ? NULL : section, NULL, "LastConnected", 0);
	entry->calls = iniGetUInteger(sys ? NULL : section, NULL, "TotalCalls", 0);
	iniGetSString(sys ? NULL : section, NULL, "UserName", "", entry->user, sizeof(entry->user));
	iniGetSString(sys ? NULL : section, NULL, "Password", "", entry->password, sizeof(entry->password));
	iniGetSString(sys ? NULL : section, NULL, "SystemPassword", "", entry->syspass, sizeof(entry->syspass));
	if (iniGetBool(section, NULL, "BeDumb", false)) /* Legacy */
		entry->conn_type = CONN_TYPE_RAW;
	entry->screen_mode = iniGetEnum(section, NULL, "ScreenMode", screen_modes_enum, SCREEN_MODE_CURRENT);
	entry->nostatus = iniGetBool(section, NULL, "NoStatus", false);
	entry->hidepopups = iniGetBool(section, NULL, "HidePopups", false);
	entry->rip = iniGetEnum(section, NULL, "RIP", rip_versions, RIP_VERSION_NONE);
	entry->force_lcf = iniGetBool(section, NULL, "ForceLCF", false);
	entry->yellow_is_yellow = iniGetBool(section, NULL, "YellowIsYellow", false);
	iniGetSString(section, NULL, "TerminalType", "", entry->term_name, sizeof(entry->term_name));
	entry->ssh_fingerprint_len = 0;
	entry->lf_expand = iniGetBool(section, NULL, "LFExpand", false);
	if (iniKeyExists(section, NULL, "SSHFingerprint")) {
		char fp[65];	/* 64 hex chars for SHA-256 + NUL */
		iniGetSString(section, NULL, "SSHFingerprint", "", fp, sizeof(fp));
		size_t flen = strlen(fp);
		/* Accept either SHA-1 (40 hex chars, legacy) or SHA-256
		   (64 hex chars).  On connect, ssh.c upgrades SHA-1 to
		   SHA-256 and rewrites this entry. */
		size_t want = 0;
		if (flen == 40)
			want = 20;
		else if (flen == 64)
			want = 32;
		if (want > 0) {
			size_t i;
			for (i = 0; i < want; i++) {
				if (!(isxdigit(fp[i * 2]) && isxdigit(fp[i * 2 + 1])))
					break;
				entry->ssh_fingerprint[i] = (HEX_CHAR_TO_INT(fp[i * 2]) * 16) + HEX_CHAR_TO_INT(fp[i * 2 + 1]);
			}
			if (i == want)
				entry->ssh_fingerprint_len = (uint8_t)want;
		}
	}
	entry->sftp_public_key = iniGetBool(section, NULL, "SFTPPublicKey", false);
	entry->ssh_allow_aes128_cbc = iniGetBool(section, NULL, "SSHAllowAES128CBC", false);
	entry->ssh_accept_early_data = iniGetBool(section, NULL, "SSHAcceptEarlyData", false);
	iniGetSString(sys ? NULL : section, NULL, "DownloadPath", home, entry->dldir, sizeof(entry->dldir));
	iniGetSString(sys ? NULL : section, NULL, "UploadPath", home, entry->uldir, sizeof(entry->uldir));
	entry->data_bits = iniGetUShortInt(section, NULL, "DataBits", 8);
	if (entry->data_bits < 7 || entry->data_bits > 8)
		entry->data_bits = 8;
	entry->stop_bits = iniGetUShortInt(section, NULL, "StopBits", 1);
	if (entry->stop_bits < 1 || entry->stop_bits > 2)
		entry->stop_bits = 1;
	entry->parity = iniGetEnum(section, NULL, "Parity", parity_enum, SYNCTERM_PARITY_NONE);
	entry->telnet_no_binary = iniGetBool(section, NULL, "TelnetBrokenTextmode", false);
	entry->defer_telnet_negotiation = iniGetBool(section, NULL, "TelnetDeferNegotiate", false);
	// TODO: Make this not suck.
	int *pal = iniGetIntList(section, NULL, "Palette", &entry->palette_size, ",", NULL);
	if (pal == NULL)
		entry->palette_size = 0;
	if (entry->palette_size > 0) {
		unsigned lent = 0;
		for (unsigned pent = 0; pent < 16; pent++) {
			entry->palette[pent] = pal[lent++];
			if (lent == entry->palette_size)
				lent = 0;
		}
	}
	free(pal);

	/* Log Stuff */
	iniGetSString(sys ? NULL : section, NULL, "LogFile", "", entry->logfile, sizeof(entry->logfile));
	entry->append_logfile = iniGetBool(sys ? NULL : section, NULL, "AppendLogFile", true);
	entry->xfer_loglevel = iniGetEnum(sys ? NULL : section, NULL, "TransferLogLevel", log_levels, LOG_INFO);
	entry->telnet_loglevel = iniGetEnum(sys ? NULL : section, NULL, "TelnetLogLevel", log_levels, LOG_INFO);

	entry->bpsrate = iniGetInteger(section, NULL, "BPSRate", 0);
	entry->music = iniGetInteger(section, NULL, "ANSIMusic", CTERM_MUSIC_BANSI);
	if (entry->music < CTERM_MUSIC_SYNCTERM || entry->music > CTERM_MUSIC_ENABLED)
		entry->music = CTERM_MUSIC_BANSI;
	entry->address_family = iniGetEnum(section, NULL, "AddressFamily", address_families, ADDRESS_FAMILY_UNSPEC);
	iniGetSString(section, NULL, "Font", "Codepage 437 English", entry->font, sizeof(entry->font));
	iniGetSString(section, NULL, "Comment", "", entry->comment, sizeof(entry->comment));
	entry->type = type;
	entry->id = id;

	/* Random junk */
	entry->sort_order = iniGetInt32(section, NULL, "SortOrder", DEFAULT_SORT_ORDER_VALUE);
}

static bool
is_reserved_bbs_name(const char *name)
{
	if (stricmp(name, "syncterm-system-cache") == 0)
		return true;
	return false;
}

static bool
prompt_password(void)
{
	if (list_password[0] == 0) {
		int olen = host_ui_prompt("Encrypted List",
		    "Enter the password for the personal directory.",
		    list_password, sizeof(list_password),
		    sizeof(list_password) - 1, true);
		if (olen < 1)
			return false;
	}
	return list_password[0] != 0;
}

static const char *supplied_list_password;

static bool
use_supplied_password(char *keybuf, size_t *sz)
{
	if (supplied_list_password == NULL || supplied_list_password[0] == 0)
		return false;
	if (keybuf != NULL && sz != NULL)
		*sz = strlcpy(keybuf, supplied_list_password, *sz);
	return true;
}

const char *
bbslist_read_status_string(enum bbslist_read_status status)
{
	switch (status) {
		case BBSLIST_READ_OK:
			return "OK";
		case BBSLIST_READ_PASSWORD_REQUIRED:
			return "The BBS list is encrypted and requires a password.";
		case BBSLIST_READ_DECRYPT_FAILED:
			return "Failed to decrypt the BBS list.";
		case BBSLIST_READ_FAILED:
			return "Failed to read the BBS list.";
		case BBSLIST_READ_MIGRATION_LIST_OPEN_FAILED:
			return "Migration aborted: failed to open the BBS list for rewrite.";
		case BBSLIST_READ_MIGRATION_INI_OPEN_FAILED:
			return "Migration aborted: syncterm.ini could not be opened for rewrite.";
		case BBSLIST_READ_MIGRATION_INI_READ_FAILED:
			return "Migration aborted: syncterm.ini could not be read.";
		case BBSLIST_READ_MIGRATION_LIST_WRITE_FAILED:
			return "Migration aborted: failed to re-encrypt the BBS list.";
		case BBSLIST_READ_MIGRATION_INI_WRITE_FAILED:
			return "Migration aborted: syncterm.ini write failed after the BBS list was re-encrypted.";
	}
	return "Unknown BBS list read error.";
}

static enum bbslist_read_status
migrate_bbslist(str_list_t inifile, enum iniCryptAlgo algo, int keysize,
    enum xp_crypt_kdf kdf, const char *password)
{
	bool kdf_legacy = algo != INI_CRYPT_ALGO_NONE &&
	    kdf != XP_CRYPT_KDF_SCRYPT;
	bool cipher_legacy = algo != INI_CRYPT_ALGO_NONE &&
	    algo != INI_CRYPT_ALGO_AES && algo != INI_CRYPT_ALGO_CHACHA20;

	list_algo = algo;
	list_keysize = keysize;
	if (!kdf_legacy && !cipher_legacy)
		return BBSLIST_READ_OK;

	enum iniCryptAlgo new_algo = cipher_legacy ? INI_CRYPT_ALGO_AES : algo;
	int new_keysize = cipher_legacy ? 256 : keysize;
	const char *new_kdf_spec = kdf_legacy ? iniCryptDefaultKDFSpec() :
	    settings.keyDerivationIterations;
	FILE *listfp = fopen(settings.list_path, "r+b");
	if (listfp == NULL)
		return BBSLIST_READ_MIGRATION_LIST_OPEN_FAILED;

	FILE *inifp = NULL;
	str_list_t inicontents = NULL;
	if (kdf_legacy) {
		char inipath[MAX_PATH + 1];
		get_syncterm_filename(inipath, sizeof(inipath), SYNCTERM_PATH_INI,
		    false);
		inifp = fopen(inipath, "r+");
		if (inifp == NULL) {
			fclose(listfp);
			return BBSLIST_READ_MIGRATION_INI_OPEN_FAILED;
		}
		inicontents = iniReadFile(inifp);
		if (inicontents == NULL) {
			fclose(inifp);
			fclose(listfp);
			return BBSLIST_READ_MIGRATION_INI_READ_FAILED;
		}
	}

	if (!iniWriteEncryptedFile(listfp, inifile, new_algo, new_keysize,
	    new_kdf_spec, password)) {
		if (inicontents != NULL)
			strListFree(&inicontents);
		if (inifp != NULL)
			fclose(inifp);
		fclose(listfp);
		return BBSLIST_READ_MIGRATION_LIST_WRITE_FAILED;
	}
	fclose(listfp);
	list_algo = new_algo;
	list_keysize = new_keysize;

	if (kdf_legacy) {
		SAFECOPY(settings.keyDerivationIterations,
		    iniCryptDefaultKDFSpec());
		iniSetString(&inicontents, "SyncTERM", "KeyDerivationIterations",
		    settings.keyDerivationIterations, &ini_style);
		rewind(inifp);
		if (chsize(fileno(inifp), 0) != 0 ||
		    !iniWriteFile(inifp, inicontents)) {
			fclose(inifp);
			strListFree(&inicontents);
			return BBSLIST_READ_MIGRATION_INI_WRITE_FAILED;
		}
		fclose(inifp);
		strListFree(&inicontents);
	}
	return BBSLIST_READ_OK;
}

str_list_t
iniReadBBSListPassword(FILE *fp, bool userList, const char *password,
    enum bbslist_read_status *status)
{
	enum bbslist_read_status result = BBSLIST_READ_OK;
	enum iniCryptAlgo algo = INI_CRYPT_ALGO_NONE;
	int keysize = 0;
	enum xp_crypt_kdf kdf = 0;

	supplied_list_password = password;
	str_list_t inifile = iniReadEncryptedFile(fp, use_supplied_password,
	    settings.keyDerivationIterations, &algo, &keysize, &kdf);
	supplied_list_password = NULL;
	if (inifile == NULL) {
		result = algo != INI_CRYPT_ALGO_NONE &&
		    (password == NULL || password[0] == 0)
		    ? BBSLIST_READ_PASSWORD_REQUIRED
		    : (algo != INI_CRYPT_ALGO_NONE
		    ? BBSLIST_READ_DECRYPT_FAILED : BBSLIST_READ_FAILED);
		goto done;
	}
	if (algo != INI_CRYPT_ALGO_NONE &&
	    !iniGetBool(inifile, NULL, "DecryptionCheck", false)) {
		result = BBSLIST_READ_DECRYPT_FAILED;
		goto fail;
	}
	if (!userList)
		goto done;

	result = migrate_bbslist(inifile, algo, keysize, kdf,
	    password != NULL ? password : "");
	if (result != BBSLIST_READ_OK)
		goto fail;
	strlcpy(list_password, password != NULL ? password : "",
	    sizeof(list_password));

#ifndef WITHOUT_DEUCESSH
	/* The signing key is trusted only from the personal list. */
	{
		char hex[INI_MAX_VALUE_LEN] = "";
		iniGetString(inifile, NULL, "WrenPickerHmacKey", "", hex);
		uint8_t tkey[32];
		bool have_key = wren_token_key_from_hex(hex, tkey);
		if (!have_key && wren_token_generate_key(tkey)) {
			char encoded[65];
			wren_token_key_to_hex(tkey, encoded);
			iniSetString(&inifile, NULL, "WrenPickerHmacKey", encoded,
			    &ini_style);
			FILE *listfp = fopen(settings.list_path, "r+b");
			if (listfp != NULL) {
				iniWriteEncryptedFile(listfp, inifile, list_algo,
				    list_keysize, settings.keyDerivationIterations,
				    list_password);
				fclose(listfp);
			}
			have_key = true;
		}
		if (have_key)
			wren_token_set_key(tkey);
	}
#endif
	goto done;

fail:
	strListFree(&inifile);
done:
	if (status != NULL)
		*status = result;
	return inifile;
}

str_list_t
iniReadBBSList(FILE *fp, bool userList)
{
	enum bbslist_read_status status;
	str_list_t inifile = iniReadBBSListPassword(fp, userList,
	    list_password[0] != 0 ? list_password : NULL, &status);
	if (status == BBSLIST_READ_PASSWORD_REQUIRED &&
	    prompt_password()) {
		inifile = iniReadBBSListPassword(fp, userList, list_password,
		    &status);
	}
	if (inifile == NULL) {
		host_ui_alert("Directory Error",
		    bbslist_read_status_string(status));
		exit(EXIT_FAILURE);
	}
	return inifile;
}

static const struct bbslist **fip_last_visited;
static int fip_last_ret;

static int
fip_bsearch_cmp(const void *key_ptr, const void *elem_ptr)
{
	const struct bbslist **elem = (const struct bbslist**)elem_ptr;
	const ini_lv_string_t *key = key_ptr;

	fip_last_visited = elem;
	fip_last_ret = key->len == 0 ? 0 : strnicmp(key->str, elem[0]->name, key->len);
	if (fip_last_ret == 0) {
		if (elem[0]->name[key->len])
			fip_last_ret = 1;
	}
	return fip_last_ret;
}

static size_t
find_insert_point(struct bbslist **list, ini_lv_string_t *bbsname, int sz)
{
	fip_last_visited = NULL;
	fip_last_ret = 0;
	const struct bbslist **clist = (const struct bbslist**)list;

	void *item = bsearch(bbsname, list, sz, sizeof(*list), fip_bsearch_cmp);
	size_t ret = 0;
	if (item == NULL) {
		if (fip_last_visited) {
			ret = fip_last_visited - clist;
			if (fip_last_ret > 0)
				ret++;
		}
	}
	else
		ret = SIZE_MAX;
	return ret;
}

static bool
read_list_contents(str_list_t inilines, struct bbslist **list,
    struct bbslist *defaults, int *count, int type)
{
	ini_fp_list_t *parsed = iniFastParseSections(inilines, false);
	if (parsed == NULL)
		return false;
	if (defaults != NULL && type == USER_BBSLIST)
		read_item(parsed, defaults, NULL, -1, type);

	size_t bbs_count = 0;
	ini_lv_string_t **bbses = iniGetFastParsedSectionList(parsed, NULL,
	    &bbs_count);
	for (size_t j = 0; j < bbs_count; j++) {
		if ((size_t)*count >= BBSLIST_MAX_ENTRIES)
			break;
		size_t ip = find_insert_point(list, bbses[j], *count);
		if (ip < SIZE_MAX) {
			struct bbslist *entry = malloc(sizeof(*entry));
			if (entry == NULL) {
				iniFastParsedSectionListFree(bbses);
				iniFreeFastParse(parsed);
				return false;
			}
			if (ip < (size_t)*count) {
				memmove(&list[ip + 1], &list[ip],
				    ((size_t)*count - ip) * sizeof(*list));
			}
			list[ip] = entry;
			read_item(parsed, entry, bbses[j], *count, type);
			(*count)++;
		}
	}
	iniFastParsedSectionListFree(bbses);
	iniFreeFastParse(parsed);
	return true;
}

bool
read_list_password(const char *listpath, struct bbslist **list,
    struct bbslist *defaults, int *count, int type, const char *password,
    enum bbslist_read_status *status)
{
	FILE *listfile = fopen(listpath, "rb");
	if (listfile == NULL) {
		if (defaults != NULL && type == USER_BBSLIST)
			read_item(NULL, defaults, NULL, -1, type);
		if (status != NULL)
			*status = BBSLIST_READ_OK;
		return true;
	}
	str_list_t inilines = iniReadBBSListPassword(listfile,
	    type == USER_BBSLIST, password, status);
	fclose(listfile);
	if (inilines == NULL)
		return false;
	bool success = read_list_contents(inilines, list, defaults, count,
	    type);
	strListFree(&inilines);
	if (!success && status != NULL)
		*status = BBSLIST_READ_FAILED;
	return success;
}

/*
 * Reads in a BBS list from listpath using *i as the counter into bbslist
 * first BBS read goes into list[i]
 */
void
read_list(char *listpath, struct bbslist **list, struct bbslist *defaults, int *i, int type)
{
	FILE *listfile = fopen(listpath, "rb");
	if (listfile == NULL) {
		if (defaults != NULL && type == USER_BBSLIST)
			read_item(NULL, defaults, NULL, -1, type);
		return;
	}
	str_list_t inilines = iniReadBBSList(listfile,
	    type == USER_BBSLIST);
	fclose(listfile);
	if (!read_list_contents(inilines, list, defaults, i, type))
		fputs("Out of memory while reading BBS list\r\n", stderr);
	strListFree(&inilines);
}

void
sort_bbs_list(struct bbslist **list, int *listcount)
{
	sort_list(list, listcount);
}

static void
update_bbs_ini(str_list_t *inifile, struct bbslist *bbs, bool new_entry)
{
	const char *section = bbs->name[0] != 0 ? bbs->name : NULL;

	if (new_entry) {
		bbs->connected = 0;
		bbs->calls = 0;
	}
	if (section != NULL)
		iniSetString(inifile, section, "Address", bbs->addr, &ini_style);
	iniSetShortInt(inifile, section, "Port", bbs->port, &ini_style);
	if (section != NULL) {
		iniSetDateTime(inifile, section, "Added", true, time(NULL),
		    &ini_style);
		iniSetDateTime(inifile, section, "LastConnected", true,
		    bbs->connected, &ini_style);
		iniSetInteger(inifile, section, "TotalCalls", bbs->calls,
		    &ini_style);
	}
	iniSetString(inifile, section, "UserName", bbs->user, &ini_style);
	iniSetString(inifile, section, "Password", bbs->password,
	    &ini_style);
	iniSetString(inifile, section, "SystemPassword", bbs->syspass,
	    &ini_style);
	iniSetEnum(inifile, section, "ConnectionType", conn_types_enum,
	    bbs->conn_type, &ini_style);
	iniSetEnum(inifile, section, "FlowControl", fc_enum,
	    fc_to_enum(bbs->flow_control), &ini_style);
	iniSetEnum(inifile, section, "ScreenMode", screen_modes_enum,
	    bbs->screen_mode, &ini_style);
	iniSetBool(inifile, section, "NoStatus", bbs->nostatus, &ini_style);
	iniSetString(inifile, section, "DownloadPath", bbs->dldir,
	    &ini_style);
	iniSetString(inifile, section, "UploadPath", bbs->uldir,
	    &ini_style);
	iniSetString(inifile, section, "LogFile", bbs->logfile, &ini_style);
	iniSetEnum(inifile, section, "TransferLogLevel", log_levels,
	    bbs->xfer_loglevel, &ini_style);
	iniSetEnum(inifile, section, "TelnetLogLevel", log_levels,
	    bbs->telnet_loglevel, &ini_style);
	iniSetBool(inifile, section, "AppendLogFile", bbs->append_logfile,
	    &ini_style);
	iniSetInteger(inifile, section, "BPSRate", bbs->bpsrate,
	    &ini_style);
	iniSetInteger(inifile, section, "ANSIMusic", bbs->music,
	    &ini_style);
	iniSetEnum(inifile, section, "AddressFamily", address_families,
	    bbs->address_family, &ini_style);
	iniSetString(inifile, section, "Font", bbs->font, &ini_style);
	iniSetBool(inifile, section, "HidePopups", bbs->hidepopups,
	    &ini_style);
	iniSetEnum(inifile, section, "RIP", rip_versions, bbs->rip,
	    &ini_style);
	if (section != NULL)
		iniSetString(inifile, section, "Comment", bbs->comment,
		    &ini_style);
	iniSetBool(inifile, section, "ForceLCF", bbs->force_lcf,
	    &ini_style);
	iniSetBool(inifile, section, "YellowIsYellow",
	    bbs->yellow_is_yellow, &ini_style);
	if (bbs->term_name[0]) {
		iniSetString(inifile, section, "TerminalType", bbs->term_name,
		    &ini_style);
	}
	iniSetBool(inifile, section, "LFExpand", bbs->lf_expand,
	    &ini_style);
	iniSetBool(inifile, section, "TelnetBrokenTextmode",
	    bbs->telnet_no_binary, &ini_style);
	iniSetBool(inifile, section, "TelnetDeferNegotiate",
	    bbs->defer_telnet_negotiation, &ini_style);
	if (section != NULL && bbs->ssh_fingerprint_len > 0) {
		char fp[65];	/* up to 64 hex chars (SHA-256) + NUL */
		for (int i = 0; i < bbs->ssh_fingerprint_len; i++)
			sprintf(&fp[i * 2], "%02x", bbs->ssh_fingerprint[i]);
		fp[bbs->ssh_fingerprint_len * 2] = 0;
		iniSetString(inifile, section, "SSHFingerprint", fp, &ini_style);
	}
	iniSetBool(inifile, section, "SFTPPublicKey",
	    bbs->sftp_public_key, &ini_style);
	iniSetBool(inifile, section, "SSHAllowAES128CBC",
	    bbs->ssh_allow_aes128_cbc, &ini_style);
	iniSetBool(inifile, section, "SSHAcceptEarlyData",
	    bbs->ssh_accept_early_data, &ini_style);
	iniSetUShortInt(inifile, section, "StopBits", bbs->stop_bits,
	    &ini_style);
	iniSetUShortInt(inifile, section, "DataBits", bbs->data_bits,
	    &ini_style);
	iniSetEnum(inifile, section, "Parity", parity_enum, bbs->parity,
	    &ini_style);
	static_assert(sizeof(int) == sizeof(uint32_t), "int must be four bytes");
	if (bbs->palette_size > 0)
		iniSetIntList(inifile, section, "Palette", ",",
		    (int *)bbs->palette, bbs->palette_size, &ini_style);
	else
		iniRemoveKey(inifile, section, "Palette");
	if (section != NULL) {
		if (bbs->sort_order != DEFAULT_SORT_ORDER_VALUE)
			iniSetInt32(inifile, section, "SortOrder",
			    bbs->sort_order, &ini_style);
		else
			iniRemoveKey(inifile, section, "SortOrder");
	}
}

static str_list_t
read_personal_list(const char *listpath)
{
	FILE *listfile = fopen(listpath, "rb");
	if (listfile == NULL)
		return strListInit();
	enum bbslist_read_status status;
	str_list_t inifile = iniReadBBSListPassword(listfile, true,
	    list_password[0] != 0 ? list_password : NULL, &status);
	fclose(listfile);
	return inifile;
}

static bool
write_personal_list_as(const char *listpath, str_list_t inifile,
    enum iniCryptAlgo algo, int keysize, const char *kdf,
    const char *password)
{
	FILE *listfile = fopen(listpath, "wb");
	if (listfile == NULL)
		return false;
	bool success = iniWriteEncryptedFile(listfile, inifile, algo, keysize,
	    kdf, password);
	if (fclose(listfile) != 0)
		success = false;
	return success;
}

static bool
write_personal_list(const char *listpath, str_list_t inifile)
{
	return write_personal_list_as(listpath, inifile, list_algo,
	    list_keysize, settings.keyDerivationIterations, list_password);
}

bool
rewrite_bbslist_kdf(const char *listpath, const char *kdf_spec)
{
	if (safe_mode || kdf_spec == NULL || kdf_spec[0] == 0)
		return false;
	if (list_algo == INI_CRYPT_ALGO_NONE)
		return true;
	str_list_t inifile = read_personal_list(listpath);
	if (inifile == NULL)
		return false;
	bool success = write_personal_list_as(listpath, inifile, list_algo,
	    list_keysize, kdf_spec, list_password);
	strListFree(&inifile);
	return success;
}

bool
rewrite_bbslist_encryption(const char *listpath, enum iniCryptAlgo algo,
    int keysize, const char *new_password)
{
	if (safe_mode || (algo != INI_CRYPT_ALGO_NONE &&
	    algo != INI_CRYPT_ALGO_AES && algo != INI_CRYPT_ALGO_CHACHA20))
		return false;
	if ((algo == INI_CRYPT_ALGO_AES && keysize != 128 && keysize != 256) ||
	    (algo != INI_CRYPT_ALGO_AES && keysize != 0))
		return false;
	const char *password = new_password != NULL ? new_password : list_password;
	if (algo != INI_CRYPT_ALGO_NONE && password[0] == 0)
		return false;
	str_list_t inifile = read_personal_list(listpath);
	if (inifile == NULL)
		return false;
	if (algo == INI_CRYPT_ALGO_NONE)
		iniRemoveKey(&inifile, NULL, "DecryptionCheck");
	else
		iniSetBool(&inifile, NULL, "DecryptionCheck", true, &ini_style);
	bool success = write_personal_list_as(listpath, inifile, algo, keysize,
	    settings.keyDerivationIterations, password);
	strListFree(&inifile);
	if (!success)
		return false;
	list_algo = algo;
	list_keysize = keysize;
	strlcpy(list_password,
	    algo == INI_CRYPT_ALGO_NONE ? "" : password,
	    sizeof(list_password));
	return true;
}

bool
add_bbs(const char *listpath, struct bbslist *bbs, bool new_entry)
{
	if (safe_mode)
		return false;
	str_list_t inifile = read_personal_list(listpath);
	if (inifile == NULL)
		return false;
	update_bbs_ini(&inifile, bbs, new_entry);
	bool success = write_personal_list(listpath, inifile);
	strListFree(&inifile);
	return success;
}

bool
save_bbs_defaults(const char *listpath, struct bbslist *defaults)
{
	if (safe_mode)
		return false;
	str_list_t inifile = read_personal_list(listpath);
	if (inifile == NULL)
		return false;
	char name[sizeof(defaults->name)];
	strlcpy(name, defaults->name, sizeof(name));
	defaults->name[0] = 0;
	update_bbs_ini(&inifile, defaults, false);
	strlcpy(defaults->name, name, sizeof(defaults->name));
	bool success = write_personal_list(listpath, inifile);
	strListFree(&inifile);
	return success;
}

bool
rename_bbs(const char *listpath, const char *old_name, struct bbslist *bbs)
{
	if (safe_mode)
		return false;
	str_list_t inifile = read_personal_list(listpath);
	if (inifile == NULL)
		return false;
	bool success = iniRenameSection(&inifile, old_name, bbs->name);
	if (success) {
		update_bbs_ini(&inifile, bbs, false);
		success = write_personal_list(listpath, inifile);
	}
	strListFree(&inifile);
	return success;
}

bool
delete_bbs(const char *listpath, struct bbslist *bbs)
{
	char cache_path[MAX_PATH + 1];

	if (safe_mode)
		return false;
	str_list_t inifile = read_personal_list(listpath);
	if (inifile == NULL)
		return false;
	bool success = iniRemoveSection(&inifile, bbs->name) &&
	    write_personal_list(listpath, inifile);
	strListFree(&inifile);
	if (!success)
		return false;
	get_syncterm_filename(cache_path, sizeof(cache_path), SYNCTERM_PATH_CACHE, false);
	backslash(cache_path);
	if (strlen(cache_path) + strlen(bbs->name) < sizeof(cache_path)) {
		strcat(cache_path, bbs->name);
		if (isdir(cache_path)) {
			delfiles(cache_path, NULL, 0);
			rmdir(cache_path);
		}
	}
	return true;
}

bool
save_webgets(void)
{
	FILE      *inifile;
	char       inipath[MAX_PATH + 1];
	str_list_t ini_file;

	if (safe_mode)
		return false;
	get_syncterm_filename(inipath, sizeof(inipath), SYNCTERM_PATH_INI, false);
	if ((inifile = fopen(inipath, "r")) != NULL) {
		ini_file = iniReadFile(inifile);
		fclose(inifile);
	}
	else {
		ini_file = strListInit();
	}
	if (ini_file == NULL)
		return false;

	iniRemoveSection(&ini_file, "WebLists");
	iniAppendSectionWithNamedStrings(&ini_file, "WebLists", (const named_string_t**)settings.webgets, &ini_style);

	bool success = false;
	if ((inifile = fopen(inipath, "w")) != NULL) {
		success = iniWriteFile(inifile, ini_file);
		if (fclose(inifile) != 0)
			success = false;
	}

	strListFree(&ini_file);
	return success;
}

void
bbslist_sweep_orphan_caches(struct bbslist **list, size_t listcount)
{
	char          cache_root[MAX_PATH + 1];
	char          entry_path[MAX_PATH + 1];
	DIR          *dir;
	struct dirent *ent;
	bool          found;

	if (!get_syncterm_filename(cache_root, sizeof(cache_root), SYNCTERM_PATH_CACHE, false))
		return;
	backslash(cache_root);
	dir = opendir(cache_root);
	if (dir == NULL)
		return;
	while ((ent = readdir(dir)) != NULL) {
		if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
			continue;
		if (is_reserved_bbs_name(ent->d_name))
			continue;
		if (strlen(cache_root) + strlen(ent->d_name) >= sizeof(entry_path))
			continue;
		strcpy(entry_path, cache_root);
		strcat(entry_path, ent->d_name);
		if (!isdir(entry_path))
			continue;
		found = false;
		for (size_t i = 0; i < listcount; i++) {
			if (list[i] != NULL && stricmp(list[i]->name, ent->d_name) == 0) {
				found = true;
				break;
			}
		}
		if (!found) {
			delfiles(entry_path, NULL, 0);
			rmdir(entry_path);
		}
	}
	closedir(dir);
}

cterm_emulation_t
get_emulation(struct bbslist *bbs)
{
	if (bbs == NULL)
		return CTERM_EMULATION_ANSI_BBS;

	switch (bbs->screen_mode) {
		case SCREEN_MODE_C64:
		case SCREEN_MODE_C128_40:
		case SCREEN_MODE_C128_80:
			return CTERM_EMULATION_PETASCII;
		case SCREEN_MODE_ATARI:
		case SCREEN_MODE_ATARI_XEP80:
			return CTERM_EMULATION_ATASCII;
		case SCREEN_MODE_PRESTEL:
			return CTERM_EMULATION_PRESTEL;
		case SCREEN_MODE_BEEB:
			return CTERM_EMULATION_BEEB;
		case SCREEN_MODE_ATARIST_40X25:
		case SCREEN_MODE_ATARIST_80X25:
		case SCREEN_MODE_ATARIST_80X25_MONO:
			return CTERM_EMULATION_ATARIST_VT52;
		default:
			return CTERM_EMULATION_ANSI_BBS;
	}
}

const char *
get_emulation_str(struct bbslist *bbs)
{
	if (bbs->term_name[0])
		return bbs->term_name;
	
	switch (get_emulation(bbs)) {
		case CTERM_EMULATION_ANSI_BBS:
			return "syncterm";
		case CTERM_EMULATION_PETASCII:
			return "PETSCII";
		case CTERM_EMULATION_ATASCII:
			return "ATASCII";
		case CTERM_EMULATION_PRESTEL:
			return "Prestel";
		case CTERM_EMULATION_BEEB:
			return "Beeb7";
		case CTERM_EMULATION_ATARIST_VT52:
			return "AtariST+VT52";
	}
	return "none";
}

void
get_term_size(struct bbslist *bbs, int *cols, int *rows)
{
	int cmode = find_vmode(screen_to_ciolib(bbs->screen_mode));

	if (cmode < 0) {
		// This shouldn't happen, but if it does, make something up.
		*cols = 80;
		*rows = 24;
		return;
	}
	if (vparams[cmode].cols < 80) {
		if (cols)
			*cols = 40;
	}
	else {
		if (vparams[cmode].cols < 132) {
			if (cols)
				*cols = 80;
		}
		else {
			if (cols)
				*cols = 132;
		}
	}
	if (rows) {
		*rows = vparams[cmode].rows;
		if (!bbs->nostatus)
			(*rows)--;
	}
}
