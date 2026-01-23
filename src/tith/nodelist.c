#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tith-file.h"
#include "tith-nodelist.h"

/*
 * Generates a TITH nodelist from a standard nodelist
 * 
 * Encoding is UTF-8
 * Line ending is a single LF
 * No EOF character
 * Tabs separate fields
 * Spaces are allowed in strings
 * No limits on field length
 * For unpublished phone numbers, a blank string is used
 * DCE speed field is removed
 * 
 * Comment lines are unchanged from FTS-5000 with the exception of the
 * special header. While it is retained, it is not modified, so the
 * checksum is invalid.
 * 
 * Flags are split into five fields, and are separated with commas
 *
 * The fields in data lines are:
 * Keyword - Mostly unchanged from FTS-5000, but does not allow Pvt to
 *           be used when an internet address is present, regardless of
 *           local policy.
 * Node number - Unchanged from FTS-5000
 * Node name - May not contain control characters, no other formatting limitations
 *             The primary purpose is to serve as an identifier that is displayed
 *             to humans. Nothing should try to parse this as a hostname ever.
 * Location - May not contain control characters, no formatting restrictions.
 *            It is suggested that a comma and space are used where FTS-5000
 *            uses an underscore (ie: Ada, MI instead of Ada_MI), but this
 *            is not required or enforced. The primary purpose if this field
 *            is to allow a local person browsing the nodelist to know they
 *            are geographically close to the system.
 * Sysop Name - May not contain control characters, no formatting restrictions.
 *              The primary purpose is to provide a user name to send netmail to
 *              at the node, and a user name that is allowed to make changes to
 *              this nodelist.
 * Phone Number - As FTS-5000 with the following exceptions:
 *                Static IP addresses will not go here.
 *                Instead of "-Unpublished-", a zero-length string is used
 * System Flags - Flags which indicate when a node is available, and the
 *                file/update requests supported.
 *                (ie: CM,ICM,MN,XA,XB,XC,XP,XR,XW,XX,#xx,!xx,TZZ)
 *                Note that this list does not correspond precisely to
 *                FTS-5001 categories as it includes ICM and does not
 *                include MO.
 * Dial Flags - Flags which correlate to usage of the phone number field
 *              This includes the Modem Connection Protocol flags, the
 *              Session Level Error-correction and Compression Flags, and
 *              the ISDN capability flags.
 * Internet Flags - Contains information for internet users.
 *                  If the INA: flag is present, it is always first.
 *                  Flags from sections 5.9.1, 5.9.2, and 5.9.3 (except
 *                  the ICM flag) from FTS-5001 are here.
 * Email Flags - Contains information for email users
 *               Flags from sections 5.9.4 and 5.9.5 are here. If IEM is
 *               present with an email address, it is always first.
 * Other Flags - Any flags that are not included in another field.
 *               The U flag is not present.
 * 
 * This processor allows overriding fields 3, 4, and 5 (Node Name, Location,
 * and Sysop Name) via a netmail from Sysop Name at the node. Note that once
 * you update the Sysop Name field, you must use the new Sysop Name for
 * further updates.
 * 
 * You can also provide a list of flags to add to any of the flags fields
 * (but can't delete flags that are present in the official nodelist).
 */

static size_t line = 0;
static struct TITH_NodelistAddr curAddr;

static void
stripend(char *line)
{
	size_t len = strlen(line);
	while (len && isspace(line[len - 1])) {
		line[len - 1] = 0;
		len--;
	}
}

static char *
getfield(char *line, char **end)
{
	if (line == NULL)
		return "";
	char *p = strchr(line, ',');
	if (p) {
		if (*p) {
			*end = p + 1;
			*p = 0;
		}
		else
			*end = p;
	}
	else
		*end = NULL;
	stripend(line);
	return line;
}

void
underscoresToSpaces(char *str)
{
	for (char *p = str; *p; p++) {
		if (*p == '_')
			*p = ' ';
	}
}

bool
isTimeChar(char ch)
{
	if (ch >= 'A' && ch <= 'X')
		return true;
	if (ch >= 'a' && ch <= 'x')
		return true;
	return false;
}

bool isInternet(char *field)
{
	if (strcmp(field, "INO4") == 0)
		return true;
	if (strncmp(field, "IBN", 3) == 0 && (field[3] == ':' || field[3] == 0)) {
		return true;
	}
	if (strncmp(field, "IFC", 3) == 0 && (field[3] == ':' || field[3] == 0)) {
		return true;
	}
	if (strncmp(field, "IFT", 3) == 0 && (field[3] == ':' || field[3] == 0)) {
		return true;
	}
	if (strncmp(field, "ITN", 3) == 0 && (field[3] == ':' || field[3] == 0)) {
		return true;
	}
	if (strncmp(field, "IVM", 3) == 0 && (field[3] == ':' || field[3] == 0)) {
		return true;
	}
	if (strncmp(field, "INA:", 4) == 0) {
		return true;
	}
	if (strncmp(field, "IIH:", 4) == 0) {
		return true;
	}
	if (strncmp(field, "IP", 2) == 0 && (field[2] == ':' || field[2] == 0)) {
		return true;
	}
	return false;
}

bool isEmail(char *field)
{
	if (strncmp(field, "ITX", 3) == 0 && (field[3] == ':' || field[3] == 0)) {
		return true;
	}
	if (strncmp(field, "IUC", 3) == 0 && (field[3] == ':' || field[3] == 0)) {
		return true;
	}
	if (strncmp(field, "IMI", 3) == 0 && (field[3] == ':' || field[3] == 0)) {
		return true;
	}
	if (strncmp(field, "ISE", 3) == 0 && (field[3] == ':' || field[3] == 0)) {
		return true;
	}
	if (strncmp(field, "EVY", 3) == 0 && (field[3] == ':' || field[3] == 0)) {
		return true;
	}
	if (strncmp(field, "EMA", 3) == 0 && (field[3] == ':' || field[3] == 0)) {
		return true;
	}
	if (strncmp(field, "IEM:", 4) == 0) {
		return true;
	}
	if (strcmp(field, "IEM") == 0) {
		return true;
	}
	return false;
}

void
appendFlag(char **list, const char *flag)
{
	size_t llen = *list ? strlen(*list) : 0;
	size_t flen = strlen(flag);
	size_t newsz = llen + flen + (llen ? 1 : 0) + 1;
	char *nlist = realloc(*list, newsz);
	if (nlist == NULL) {
		fprintf(stderr, "realloc() to %zu failed for flag \"%s\"\n", newsz, flag);
		exit(EXIT_FAILURE);
	}
	if (*list == NULL)
		nlist[0] = 0;
	*list = nlist;
	if ((*list)[0] == 0)
		strcat(*list, flag);
	else {
		// TODO: Check for duplicates?
		if (strncmp(flag, "INA:", 4) == 0 || strncmp(flag, "IEM:", 4) == 0) {
			size_t m = strlen(flag);
			size_t l = strlen(*list);
			memmove(&(*list)[m + 1], *list, l + 1);
			memcpy(*list, flag, m);
			(*list)[m] = ',';
		}
		else {
			strcat(*list, ",");
			strcat(*list, flag);
		}
	}
}

struct Overrides {
	struct TITH_NodelistAddr addr;
	char *flags;		// FL
	char *location;		// LO
	char *nodeName;		// NN
	char *sysopName;	// SN
};

struct Overrides *override;
size_t overrides;

struct Overrides *
findOverride(struct TITH_NodelistAddr *addr)
{
	return bsearch(addr, override, overrides, sizeof(*override), tith_cmpNodelistAddr);
}

bool
checkType(char *line, const char *prefix, char **member, char *addrStr)
{
	if (strncmp(line, prefix, 2) == 0) {
		if (*member) {
			fprintf(stderr, "Duplicate %s line for %s\n", prefix, addrStr);
			exit(EXIT_FAILURE);
		}
		memmove(line, &line[2], strlen(&line[2]) + 1);
		*member = line;
		return true;
	}
	return false;
}

/*
 * Overrides format
 * One line with the address in Z:R/N format followed by zero or more lines
 * prefixed with one of:
 * LO, NN, FL, or SN
 */
void
loadOverrides(const char *path)
{
	struct TITH_NodelistAddr addr = {0};
	char *addrStr = NULL;
	struct Overrides *over = NULL;
	FILE *fp = fopen(path, "rb");
	if (fp == NULL) {
		fprintf(stderr, "Unable to open \"%s\", aborting.\n", path);
		exit(EXIT_FAILURE);
	}
	char *line;
	while ((line = tith_readLine(fp))) {
		if (line[0] == 0) {
			free(line);
			continue;
		}
		if (isdigit(line[0])) {
			if (!tith_parseNodelistAddr(line, &addr)) {
				fprintf(stderr, "Failed to parse address \"%s\"\n", line);
				exit(EXIT_FAILURE);
			}
			free(addrStr);
			addrStr = line;
			over = findOverride(&addr);
			if (over == NULL) {
				struct Overrides *newover = realloc(override, sizeof(struct Overrides) * (overrides + 1));
				if (newover == NULL) {
					fputs("Failed to realloc() override\n", stderr);
					exit(EXIT_FAILURE);
				}
				override = newover;
				// TODO: Fast insert should be fairly easy using bsearch()
				over = &override[overrides++];
				memset(over, 0, sizeof(*over));
				over->addr = addr;
				qsort(override, overrides, sizeof(*override), tith_cmpNodelistAddr);
				over = findOverride(&addr);
				if (over == NULL) {
					fputs("Failed to find override after inserting\n", stderr);
					exit(EXIT_FAILURE);
				}
			}
			continue;
		}
		if (over == NULL) {
			fprintf(stderr, "No override set\n");
			exit(EXIT_FAILURE);
		}
		if (!(checkType(line, "FL", &over->flags, addrStr)
		    || checkType(line, "LO", &over->location, addrStr)
		    || checkType(line, "NN", &over->nodeName, addrStr)
		    || checkType(line, "SN", &over->sysopName, addrStr))) {
			fprintf(stderr, "Invalid line prefix %.2s\n", line);
			exit(EXIT_FAILURE);
		}
	}
	if (!feof(fp)) {
		fprintf(stderr, "Failed to read to end of file\n");
		exit(EXIT_FAILURE);
	}
	fclose(fp);
	free(addrStr);
}

int
main(int argc, char **argv)
{
	char linebuf[1024];	// I think it's actually restricted to 158 or some shit

	setlocale(LC_ALL, "");
	for (int arg = 1; arg < argc; arg++) {
		loadOverrides(argv[arg]);
	}

	while (fgets(linebuf, sizeof(linebuf), stdin) != NULL) {
		stripend(linebuf);
		line++;
		// Comment
		if (linebuf[0] == ';') {
			fputs(linebuf, stdout);
			fputs("\n", stdout);
			continue;
		}
		// EOF
		if (linebuf[0] == 0x1a)
			break;
		char *next;

		// Keyword
		// TODO: Strip Pvt when an internet address is present
		char *field = getfield(linebuf, &next);
		const enum TITH_NodelistLineType type = tith_parseNodelistKeyword(field, &curAddr);
		if (type == TYPE_Unknown) {
			fprintf(stderr, "Unknown Keyword \"%s\", skipping\n", field);
			continue;
		}
		fprintf(stdout, "%s\t", field);

		// Node number
		field = getfield(next, &next);
		if (!tith_parseNodelistNodeNumber(field, type, &curAddr)) {
			fprintf(stderr, "Failed to parse node number\n");
			continue;
		}
		fprintf(stdout, "%s\t", field);

		struct Overrides *over = findOverride(&curAddr);
		// Node name (MAY be an internet addres)
		field = getfield(next, &next);
		underscoresToSpaces(field);
		if (over && over->nodeName)
			field = over->nodeName;
		fprintf(stdout, "%s\t", field);
		// Location
		field = getfield(next, &next);
		underscoresToSpaces(field);
		if (over && over->location)
			field = over->location;
		fprintf(stdout, "%s\t", field);
		// Sysop name
		field = getfield(next, &next);
		underscoresToSpaces(field);
		if (over && over->sysopName)
			field = over->sysopName;
		fprintf(stdout, "%s\t", field);
		// Phone number
		field = getfield(next, &next);
		if (strcmp(field, "-Unpublished-") == 0) {
			field[0] = 0;
		}
		fprintf(stdout, "%s\t", field);
		// DCE speed, dropped
		field = getfield(next, &next);
		// Flags
		char *sysFlags = NULL;
		char *internetFlags = NULL;
		char *dialFlags = NULL;
		char *otherFlags = NULL;
		char *emailFlags = NULL;
		appendFlag(&sysFlags, "");
		appendFlag(&internetFlags, "");
		appendFlag(&dialFlags, "");
		appendFlag(&otherFlags, "");
		appendFlag(&emailFlags, "");
		bool didOverrides = false;
		for (;;) {
			field = getfield(next, &next);
			if (field[0] == 0) {
				if (!didOverrides && over && over->flags) {
					next = over->flags;
					didOverrides = true;
					continue;
				}
				break;
			}
			if (strcmp(field, "CM") == 0)
				appendFlag(&sysFlags, field);
			else if (strcmp(field, "ICM") == 0)
				appendFlag(&sysFlags, field);
			else if (strcmp(field, "MO") == 0)
				appendFlag(&otherFlags, field);
			else if (strcmp(field, "LO") == 0)
				appendFlag(&sysFlags, field);
			else if (strcmp(field, "MN") == 0)
				appendFlag(&sysFlags, field);
			else if (strcmp(field, "V22") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "V22B") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "V29") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "V32") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "V32b") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "V32B") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "V34") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "V42B") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "V90C") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "V90S") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "V32T") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "VFC") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "HST") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "H14") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "H16") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "X2C") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "X2S") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "ZYX") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "Z19") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "H96") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "PEP") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "CSP") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "MNP") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "V42") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "V42b") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "XA") == 0)
				appendFlag(&sysFlags, field);
			else if (strcmp(field, "XB") == 0)
				appendFlag(&sysFlags, field);
			else if (strcmp(field, "XC") == 0)
				appendFlag(&sysFlags, field);
			else if (strcmp(field, "XP") == 0)
				appendFlag(&sysFlags, field);
			else if (strcmp(field, "XR") == 0)
				appendFlag(&sysFlags, field);
			else if (strcmp(field, "XW") == 0)
				appendFlag(&sysFlags, field);
			else if (strcmp(field, "XX") == 0)
				appendFlag(&sysFlags, field);
			else if (strcmp(field, "GUUCP") == 0)
				appendFlag(&otherFlags, field);
			else if ((field[0] == '#' || field[0] == '!') && isdigit(field[1]) && isdigit(field[2]))
				appendFlag(&sysFlags, field);
			else if (field[0] == 'T' && isTimeChar(field[1]) && isTimeChar(field[2]) && field[3] == 0)
				appendFlag(&sysFlags, field);
			else if (strcmp(field, "V110L") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "V110H") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "V120L") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "V120H") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "X75") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "ISDN") == 0)
				appendFlag(&dialFlags, field);
			else if (strcmp(field, "PING") == 0)
				appendFlag(&otherFlags, field);
			else if (strcmp(field, "TRACE") == 0)
				appendFlag(&otherFlags, field);
			else if (field[0] == 'U' && field[1] == 0)
				/* Do nothing */;
			else if (strcmp(field, "ZEC") == 0)
				appendFlag(&otherFlags, field);
			else if (strcmp(field, "REC") == 0)
				appendFlag(&otherFlags, field);
			else if (strcmp(field, "NEC") == 0)
				appendFlag(&otherFlags, field);
			else if (strcmp(field, "NC") == 0)
				appendFlag(&otherFlags, field);
			else if (strcmp(field, "SDS") == 0)
				appendFlag(&otherFlags, field);
			else if (strcmp(field, "SMH") == 0)
				appendFlag(&otherFlags, field);
			else if (strcmp(field, "RPK") == 0)
				appendFlag(&otherFlags, field);
			else if (strcmp(field, "NPK") == 0)
				appendFlag(&otherFlags, field);
			else if (strcmp(field, "ENC") == 0)
				appendFlag(&otherFlags, field);
			else if (strcmp(field, "CDP") == 0)
				appendFlag(&otherFlags, field);
			else if (strcmp(field, "BEER") == 0)
				appendFlag(&otherFlags, field);
			else if (strncmp(field, "BEER:", 5) == 0)
				appendFlag(&otherFlags, field);
			else if (isInternet(field))
				appendFlag(&internetFlags, field); // Re-parse later!
			else if (isEmail(field))
				appendFlag(&emailFlags, field); // Re-parse later!
			else {
				fprintf(stderr, "Unhandled flag \"%s\"\n", field);
				appendFlag(&otherFlags, field);
			}
		}
		fprintf(stdout, "%s\t", sysFlags);
		fprintf(stdout, "%s\t", dialFlags);
		fprintf(stdout, "%s\t", internetFlags);
		fprintf(stdout, "%s\t", emailFlags);
		fprintf(stdout, "%s", otherFlags);
		fputs("\n", stdout);
	}
}
