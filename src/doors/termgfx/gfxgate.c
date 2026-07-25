#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfxgate.h"

/* Longest notice file we will load. A notice is a few lines on a terminal;
 * anything past this is a wrong path pointed at a game asset, and reading it
 * would dump megabytes at the player. */
#define GFXGATE_FILE_MAX  16384

static const char gfxgate_default[] =
	"\r\n\x1b[0m\r\n"
	"  This game requires a terminal with sixel or JXL graphics support\r\n"
	"  (such as SyncTERM). This terminal reports neither, so the game\r\n"
	"  cannot be displayed.\r\n\r\n";

/* Framing shared by the built-in and by a one-line [text] replacement. */
static const char gfxgate_lead[] = "\r\n\x1b[0m\r\n  ";
static const char gfxgate_tail[] = "\r\n\r\n";

const char        termgfx_gfxgate_prompt[] = "  Press any key to return to the BBS...";

int termgfx_gfxgate(int have_graphics, int probe_replied, int jxl_answered,
                    uint32_t probe_start_ms, uint32_t now_ms)
{
	if (!probe_replied)
		return TERMGFX_GFXGATE_PROCEED;
	if (have_graphics)
		return TERMGFX_GFXGATE_PROCEED;
	if (!jxl_answered
	    && (int32_t)(now_ms - probe_start_ms) <= TERMGFX_GFX_SETTLE_MS)
		return TERMGFX_GFXGATE_WAIT;
	return TERMGFX_GFXGATE_REJECT;
}

/* Read `path` into a NUL-terminated buffer, rewriting bare LF as CRLF and
 * leaving an existing CRLF alone. NULL on any failure (the caller falls back
 * to a less specific notice). */
static char *gfxgate_slurp_crlf(const char *path)
{
	FILE * f;
	char * raw, *out;
	size_t n, i, o, lf = 0;
	long   len;

	if ((f = fopen(path, "rb")) == NULL)
		return NULL;
	if (fseek(f, 0, SEEK_END) != 0 || (len = ftell(f)) < 0
	    || fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return NULL;
	}
	if (len > GFXGATE_FILE_MAX) {
		fclose(f);
		fprintf(stderr, "termgfx: no-graphics notice '%s' is %ld bytes"
		        " (max %d) -- ignored\n", path, len, GFXGATE_FILE_MAX);
		return NULL;
	}
	if ((raw = (char *)malloc((size_t)len + 1)) == NULL) {
		fclose(f);
		return NULL;
	}
	n = fread(raw, 1, (size_t)len, f);
	fclose(f);
	raw[n] = '\0';

	for (i = 0; i < n; i++)
		if (raw[i] == '\n' && (i == 0 || raw[i - 1] != '\r'))
			lf++;
	if ((out = (char *)malloc(n + lf + 1)) == NULL) {
		free(raw);
		return NULL;
	}
	for (i = 0, o = 0; i < n; i++) {
		if (raw[i] == '\n' && (i == 0 || raw[i - 1] != '\r'))
			out[o++] = '\r';
		out[o++] = raw[i];
	}
	out[o] = '\0';
	free(raw);
	return out;
}

const char *termgfx_gfxgate_notice(const char *file, const char *text)
{
	static const char *cached;
	char *             body;

	if (cached != NULL)
		return cached;

	if (file != NULL && file[0] != '\0'
	    && (body = gfxgate_slurp_crlf(file)) != NULL) {
		/* The file owns its own layout; we only bracket it with an SGR
		 * reset so a half-attributed .ans can't leak into the BBS prompt
		 * the player lands back on. */
		size_t bl   = strlen(body);
		int    endnl = (bl > 0 && body[bl - 1] == '\n');
		size_t need = sizeof("\r\n\x1b[0m\r\n") - 1 + bl
		              + (endnl ? 0 : 2) + sizeof("\x1b[0m\r\n") - 1 + 1;
		char * buf  = (char *)malloc(need);

		if (buf != NULL) {
			strcpy(buf, "\r\n\x1b[0m\r\n");
			strcat(buf, body);
			if (!endnl)
				strcat(buf, "\r\n");
			strcat(buf, "\x1b[0m\r\n");
			free(body);
			cached = buf;
			return cached;
		}
		free(body);
	}

	if (text != NULL && text[0] != '\0') {
		size_t need = sizeof(gfxgate_lead) - 1 + strlen(text)
		              + sizeof(gfxgate_tail) - 1 + 1;
		char * buf  = (char *)malloc(need);

		if (buf != NULL) {
			strcpy(buf, gfxgate_lead);
			strcat(buf, text);
			strcat(buf, gfxgate_tail);
			cached = buf;
			return cached;
		}
	}

	cached = gfxgate_default;
	return cached;
}
