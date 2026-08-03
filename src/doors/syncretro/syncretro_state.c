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

/* <home>/<rom-basename-sans-extension>.<key8>.state
 *
 * The key is in the NAME, not inside the file: that is what lets the lobby tell
 * a live snapshot from a stale one with a single directory read rather than a
 * probe per cartridge, and it means a snapshot for a core or option set that no
 * longer applies simply never matches. */
int sr_state_path(char *out, size_t max, const char *home,
                  const char *rom_path, const char *key8)
{
	const char *base;
	const char *dot;
	char        stem[256];
	size_t      n;

	if (out == NULL || max == 0 || home == NULL || rom_path == NULL
	    || key8 == NULL || key8[0] == '\0')
		return -1;

	base = strrchr(rom_path, '/');
#ifdef _WIN32
	{
		const char *bs = strrchr(rom_path, '\\');

		if (bs != NULL && (base == NULL || bs > base))
			base = bs;
	}
#endif
	base = base != NULL ? base + 1 : rom_path;

	dot = strrchr(base, '.');
	n   = dot != NULL ? (size_t)(dot - base) : strlen(base);
	if (n >= sizeof stem)
		n = sizeof stem - 1;
	memcpy(stem, base, n);
	stem[n] = '\0';

	snprintf(out, max, "%s/%s.%s.state", home, stem, key8);
	return 0;
}

int sr_state_save(rc_core_t *core, const char *path)
{
	size_t size;
	void * buf;
	FILE * f;
	int    rc = 0;

	if (core == NULL || core->serialize_size == NULL || core->serialize == NULL)
		return -1;
	size = core->serialize_size();
	if (size == 0)          /* the core does not support save states at all */
		return -1;
	buf = malloc(size);
	if (buf == NULL)
		return -1;
	if (!core->serialize(buf, size)) {
		free(buf);
		return -1;
	}
	f = fopen(path, "wb");
	if (f == NULL) {
		free(buf);
		return -1;
	}
	if (fwrite(buf, 1, size, f) != size)
		rc = -1;
	if (fclose(f) != 0)
		rc = -1;
	free(buf);
	if (rc != 0)
		remove(path);   /* a truncated snapshot is worse than none */
	return rc;
}

int sr_state_load(rc_core_t *core, const char *path)
{
	size_t size;
	size_t got;
	void * buf;
	FILE * f;
	int    ok;

	if (core == NULL || core->unserialize == NULL)
		return -1;
	f = fopen(path, "rb");
	if (f == NULL)
		return -1;      /* no snapshot is the normal case, not an error */
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return -1;
	}
	size = (size_t)ftell(f);
	rewind(f);
	buf = size > 0 ? malloc(size) : NULL;
	if (buf == NULL) {
		fclose(f);
		return -1;
	}
	got = fread(buf, 1, size, f);
	fclose(f);
	if (got != size) {
		free(buf);
		return -1;
	}
	ok = core->unserialize(buf, size);
	free(buf);
	return ok ? 0 : -1;
}
