// caps.c -- terminal capability cap-probe parsing, carved out of syncdoom.c's
// probe_jxl(). See caps.h: I/O-free, the door owns the query emit + read loop.

#include "caps.h"

// APC: ESC _ SyncTERM:Q;JXL ST  (ST = ESC backslash)
const char *const termgfx_query_jxl = "\x1b_SyncTERM:Q;JXL\x1b\\";

int termgfx_caps_parse_jxl(const uint8_t *acc, int len)
{
	int j;

	for (j = 0; j + 6 < len; j++) {
		if (acc[j] == 0x1b && acc[j + 1] == '[' && acc[j + 2] == '=' &&
		    acc[j + 3] == '1' && acc[j + 4] == ';' &&
		    (acc[j + 5] == '0' || acc[j + 5] == '1') && acc[j + 6] == '-')
			return acc[j + 5] - '0';
	}
	return -1;
}

int termgfx_caps_cterm_version(const int *params, int nparams, char intro)
{
	// SyncTERM answers DA1 ("ESC [ c") with a CTerm report of the form
	// "ESC [ = 67;84;101;114;109; MAJ ; MIN ; ... c" -- 67,84,101,114,109 spell
	// "Cterm". The door hands us the numeric params already split out (the '='
	// intro sits in `intro`). Version is MAJ*1000+MIN (so 1.329 -> 1329).
	if (intro != '=' || params == NULL || nparams < 7)
		return -1;
	if (params[0] != 67 || params[1] != 84 || params[2] != 101
	    || params[3] != 114 || params[4] != 109)
		return -1;
	return params[5] * 1000 + params[6];
}
