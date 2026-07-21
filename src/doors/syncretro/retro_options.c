/* retro_options.c -- the core-option store. See retro_options.h for WHY.
 *
 * Two inputs, resolved before the core runs:
 *   - what the core advertises (SET_VARIABLES / SET_CORE_OPTIONS[_V2]), which
 *     arrives DURING rc_core_load_game();
 *   - what the console pinned (`-option key=value`), which arrives from argv
 *     long before that.
 * So pins are collected first and applied per key as the core declares it --
 * not the other way round, which would need the table to exist before the core
 * has said what is in it.
 */
#include "retro_options.h"
#include "syncretro.h"

#include <stdio.h>
#include <string.h>

/* Sizes: mame2003_plus advertises 23 options, fbneo a few dozen, the biggest
 * cores around 150. 256 is headroom, not a guess at the maximum. Values are
 * short words ("enabled", "48000", "auto_aggressive"); 96 is generous. */
#define RO_MAX_OPTIONS  256
#define RO_MAX_PINS      32
#define RO_KEY_MAX       64
#define RO_VAL_MAX       96

typedef struct {
	char key[RO_KEY_MAX];
	char val[RO_VAL_MAX];
	int  pinned;
	int  queried;
} ro_opt_t;

static ro_opt_t g_opt[RO_MAX_OPTIONS];
static int      g_nopt;

static struct {
	char key[RO_KEY_MAX];
	char val[RO_VAL_MAX];
	int  matched;
} g_pin[RO_MAX_PINS];
static int g_npin;

static int g_unknown;      /* GET_VARIABLE for a key the core never advertised */

/* Bounded copy that REPORTS truncation instead of silently handing the core a
 * value that is not the one anybody wrote. A truncated option value is not a
 * cosmetic loss: "auto_aggressive" cut to "auto_agg" is simply not a value the
 * core offers, and it would fall back to something else without a word. */
static int ro_copy(char *dst, size_t sz, const char *src, const char *what)
{
	size_t n = strlen(src);

	if (sz == 0)
		return -1;
	if (n >= sz) {
		fprintf(stderr, "syncretro: core option %s \"%s\" is too long (max %u)\n",
		        what, src, (unsigned)(sz - 1));
		return -1;
	}
	memcpy(dst, src, n + 1);
	return 0;
}

int sr_option_pin(const char *kv)
{
	const char *eq;
	size_t      n;

	if (kv == NULL || (eq = strchr(kv, '=')) == NULL || eq == kv) {
		fprintf(stderr, "syncretro: -option \"%s\" is not key=value\n",
		        kv != NULL ? kv : "");
		return -1;
	}
	if (g_npin >= RO_MAX_PINS) {
		fprintf(stderr, "syncretro: too many -option pins (max %d)\n", RO_MAX_PINS);
		return -1;
	}
	n = (size_t)(eq - kv);
	if (n >= RO_KEY_MAX) {
		fprintf(stderr, "syncretro: -option key \"%s\" is too long\n", kv);
		return -1;
	}
	memcpy(g_pin[g_npin].key, kv, n);
	g_pin[g_npin].key[n] = '\0';
	if (ro_copy(g_pin[g_npin].val, RO_VAL_MAX, eq + 1, "value") != 0)
		return -1;
	g_npin++;
	return 0;
}

/* One advertised option: store it, then let a pin overwrite its value. `dflt`
 * may be NULL -- a core is allowed to advertise a key with no usable default,
 * and an empty value is still better than no answer (the core gets a definite
 * "" rather than an unanswered query). */
static void ro_declare(const char *key, const char *dflt)
{
	ro_opt_t *o;
	int       i;

	if (key == NULL || key[0] == '\0')
		return;
	for (i = 0; i < g_nopt; i++)
		if (strcmp(g_opt[i].key, key) == 0)
			return;                        /* re-declared: keep the first */
	if (g_nopt >= RO_MAX_OPTIONS) {
		fprintf(stderr, "syncretro: core advertises more than %d options;"
		        " \"%s\" and any after it will be left at the core's own default\n",
		        RO_MAX_OPTIONS, key);
		return;
	}
	o = &g_opt[g_nopt];
	if (ro_copy(o->key, RO_KEY_MAX, key, "key") != 0)
		return;
	if (dflt == NULL || ro_copy(o->val, RO_VAL_MAX, dflt, "default") != 0)
		o->val[0] = '\0';
	g_nopt++;

	for (i = 0; i < g_npin; i++) {
		if (strcmp(g_pin[i].key, key) != 0)
			continue;
		memcpy(o->val, g_pin[i].val, sizeof o->val);
		o->pinned        = 1;
		g_pin[i].matched = 1;
	}
}

/* SET_VARIABLES (options version 0). `value` is "description; a|b|c" and the
 * FIRST listed value is the default -- the separator is a semicolon, and the
 * description itself may contain them, so split on the FIRST one only. */
void ro_declare_v0(const struct retro_variable *v)
{
	for (; v != NULL && v->key != NULL; v++) {
		const char *p;
		const char *bar;
		char        dflt[RO_VAL_MAX];
		size_t      n;

		if (v->value == NULL || (p = strchr(v->value, ';')) == NULL) {
			ro_declare(v->key, NULL);
			continue;
		}
		for (p++; *p == ' '; p++)
			;
		bar = strchr(p, '|');
		n   = (bar != NULL) ? (size_t)(bar - p) : strlen(p);
		if (n >= sizeof dflt)
			n = sizeof dflt - 1;
		memcpy(dflt, p, n);
		dflt[n] = '\0';
		ro_declare(v->key, dflt);
	}
}

void ro_declare_v1(const struct retro_core_option_definition *d)
{
	for (; d != NULL && d->key != NULL; d++)
		ro_declare(d->key, d->default_value != NULL ? d->default_value
		           : d->values[0].value);
}

void ro_declare_v2(const struct retro_core_options_v2 *o)
{
	const struct retro_core_option_v2_definition *d;

	if (o == NULL)
		return;
	for (d = o->definitions; d != NULL && d->key != NULL; d++)
		ro_declare(d->key, d->default_value != NULL ? d->default_value
		           : d->values[0].value);
}

const char *ro_option_get(const char *key)
{
	int i;

	if (key == NULL)
		return NULL;
	for (i = 0; i < g_nopt; i++) {
		if (strcmp(g_opt[i].key, key) != 0)
			continue;
		g_opt[i].queried = 1;
		return g_opt[i].val;
	}
	g_unknown++;
	return NULL;
}

int ro_options_changed(void)
{
	return 0;
}

/* One line for the door log, plus a WARNING per pin that matched nothing.
 *
 * That warning is the whole reason this function exists. A pinned key with a
 * typo -- or one renamed by a core update -- is otherwise perfectly silent: the
 * core runs on a default nobody chose, and the symptom is a door that boots to
 * the wrong screen or the wrong sound with no error anywhere. Cheap to detect
 * here, expensive to find later. */
void sr_options_report(void)
{
	int i, pinned = 0, queried = 0;

	for (i = 0; i < g_nopt; i++) {
		pinned  += g_opt[i].pinned;
		queried += g_opt[i].queried;
	}
	fprintf(stderr, "syncretro: core options: %d advertised, %d pinned, %d read\n",
	        g_nopt, pinned, queried);
	for (i = 0; i < g_npin; i++) {
		if (!g_pin[i].matched)
			fprintf(stderr, "syncretro: WARNING -option \"%s\": this core"
			        " advertises no such option -- it is being IGNORED\n",
			        g_pin[i].key);
	}
	if (g_unknown > 0)
		fprintf(stderr, "syncretro: %d option read(s) for a key the core never"
		        " advertised (answered false)\n", g_unknown);
}
