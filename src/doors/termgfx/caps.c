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
