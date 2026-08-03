/* syncretro_state.c -- see syncretro_state.h.
 *
 * Copyright(C) 2026 Rob Swindell / SyncRetro.  GPL-2.0.
 */
#include "syncretro_state.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "md5.h"   /* src/hash, already on termgfx's PUBLIC include path */

void sr_state_key(char out[9], const char *core_md5, const char *rom_md5,
                  const char *opts)
{
	BYTE   digest[MD5_DIGEST_SIZE];
	char * buf;
	size_t n;
	int    i;

	if (core_md5 == NULL)
		core_md5 = "";
	if (rom_md5 == NULL)
		rom_md5 = "";
	if (opts == NULL)
		opts = "";

	n   = strlen(core_md5) + 1 + strlen(rom_md5) + 1 + strlen(opts) + 1;
	buf = malloc(n);
	if (buf == NULL) {          /* no key, no snapshot -- never a wrong key */
		out[0] = '\0';
		return;
	}
	snprintf(buf, n, "%s\n%s\n%s", core_md5, rom_md5, opts);
	MD5_calc(digest, buf, strlen(buf));
	free(buf);

	for (i = 0; i < 4; i++)
		snprintf(out + i * 2, 3, "%02x", digest[i]);
	out[8] = '\0';
}
