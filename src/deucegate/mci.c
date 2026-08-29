#include "deucegate.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "ini_file.h"
#include "str_list.h"

static bool
additional_value(const dg_user_t *user, const char *token, char *out, size_t outsz)
{
	FILE *fp;
	str_list_t ini;
	char key[160];
	bool found = false;
	if (user == NULL || !*user->ini_path || snprintf(key, sizeof(key), "AdditionalInfo_%s", token) >= (int)sizeof(key))
		return false;
	if ((fp = iniOpenFile(user->ini_path, false)) == NULL)
		return false;
	ini = iniReadFile(fp);
	iniCloseFile(fp);
	if (ini != NULL) {
		found = iniGetSString(ini, "USER", key, "", out, outsz) != NULL;
		strListFree(&ini);
	}
	return found;
}

static bool
who_value(const dg_mci_context_t *ctx, const char *token, char *out, size_t outsz)
{
	unsigned node;
	char field[32], path[DG_PATH_MAX], line[1024];
	FILE *fp;
	if (sscanf(token, "WHOSONLINE_%u_%31s", &node, field) != 2 ||
	    !dg_path_join(path, sizeof(path), ctx->config->root, "whoisonline.txt") ||
	    (fp = fopen(path, "r")) == NULL)
		return false;
	while (fgets(line, sizeof(line), fp) != NULL) {
		char *cols[5] = {0};
		char *p = line;
		unsigned n;
		for (size_t i = 0; i < 5 && p != NULL; i++) {
			cols[i] = p;
			p = strchr(p, '\t');
			if (p != NULL) *p++ = 0;
		}
		if (cols[0] == NULL || sscanf(cols[0], "%u", &n) != 1 || n != node)
			continue;
		if (dg_stricmp(field, "ALIAS") == 0 && cols[1] != NULL) p = cols[1];
		else if (dg_stricmp(field, "IPADDRESS") == 0 && cols[2] != NULL) p = cols[2];
		else if (dg_stricmp(field, "STATUS") == 0 && cols[3] != NULL) p = cols[3];
		else break;
		p[strcspn(p, "\r\n")] = 0;
		strncpy(out, p, outsz - 1);
		out[outsz - 1] = 0;
		fclose(fp);
		return true;
	}
	fclose(fp);
	return false;
}

static bool
value_for(const char *token, char *out, size_t outsz, const dg_mci_context_t *ctx)
{
	struct tm tmv;
	time_t now = ctx->now ? ctx->now : time(NULL);
#ifdef _WIN32
	localtime_s(&tmv, &now);
#else
	localtime_r(&now, &tmv);
#endif
	out[0] = 0;
	if (dg_stricmp(token, "ACCESSLEVEL") == 0) snprintf(out, outsz, "%u", ctx->user->access_level);
	else if (dg_stricmp(token, "ALIAS") == 0) strncpy(out, ctx->user->alias, outsz - 1);
	else if (dg_stricmp(token, "BBSNAME") == 0) strncpy(out, ctx->config->bbs_name, outsz - 1);
	else if (dg_stricmp(token, "DATE") == 0) strftime(out, outsz, "%m/%d/%Y", &tmv);
	else if (dg_stricmp(token, "FILENAME") == 0) strncpy(out, ctx->filename ? ctx->filename : "", outsz - 1);
	else if (dg_stricmp(token, "GSDIR") == 0) strncpy(out, ctx->config->root, outsz - 1);
	else if (dg_stricmp(token, "MENUNAME") == 0) strncpy(out, ctx->menu_name ? ctx->menu_name : "", outsz - 1);
	else if (dg_stricmp(token, "NODE") == 0) snprintf(out, outsz, "%u", ctx->node);
#ifdef _WIN32
	else if (dg_stricmp(token, "OPERATINGSYSTEM") == 0) strncpy(out, "Windows", outsz - 1);
#else
	else if (dg_stricmp(token, "OPERATINGSYSTEM") == 0) strncpy(out, "Unix", outsz - 1);
#endif
	else if (dg_stricmp(token, "SYSOPEMAIL") == 0) strncpy(out, ctx->config->sysop_email, outsz - 1);
	else if (dg_stricmp(token, "SYSOPNAME") == 0) strncpy(out, ctx->config->sysop_name, outsz - 1);
	else if (dg_stricmp(token, "TIME") == 0) strftime(out, outsz, "%I:%M %p", &tmv);
	else if (dg_stricmp(token, "TIMELEFT") == 0) snprintf(out, outsz, "%02u:%02u:%02u",
	    ctx->seconds_left / 3600, (ctx->seconds_left / 60) % 60, ctx->seconds_left % 60);
	else if (strncmp(token, "WHOSONLINE_", 12) == 0) return who_value(ctx, token, out, outsz);
	else return additional_value(ctx->user, token, out, outsz);
	out[outsz - 1] = 0;
	return true;
}

static size_t
append_bytes(char *out, size_t outsz, size_t used, const char *s, size_t len)
{
	if (used >= outsz) return used;
	if (len > outsz - used - 1) len = outsz - used - 1;
	memcpy(out + used, s, len);
	out[used + len] = 0;
	return used + len;
}

size_t
dg_mci_expand(const char *input, char *output, size_t outsz, const dg_mci_context_t *ctx)
{
	const char *p = input;
	size_t used = 0;
	if (outsz == 0) return 0;
	output[0] = 0;
	while (*p != 0) {
		const char *open = strchr(p, '{');
		const char *close;
		char raw[160], token[160], value[DG_MCI_MAX];
		unsigned width = 0;
		bool right = false, known;
		if (open == NULL) {
			used = append_bytes(output, outsz, used, p, strlen(p));
			break;
		}
		used = append_bytes(output, outsz, used, p, (size_t)(open - p));
		close = strchr(open + 1, '}');
		if (close == NULL || (size_t)(close - open - 1) >= sizeof(raw)) {
			used = append_bytes(output, outsz, used, open, 1);
			p = open + 1;
			continue;
		}
		memcpy(raw, open + 1, (size_t)(close - open - 1));
		raw[close - open - 1] = 0;
		strcpy(token, raw);
		if (isdigit((unsigned char)token[0])) {
			char *end;
			width = (unsigned)strtoul(token, &end, 10);
			if (*end != 0) memmove(token, end, strlen(end) + 1);
			else width = 0;
			right = width != 0;
		}
		else {
			size_t n = strlen(token), at = n;
			while (at > 0 && isdigit((unsigned char)token[at - 1])) at--;
			if (at < n) {
				width = (unsigned)strtoul(token + at, NULL, 10);
				token[at] = 0;
			}
		}
		if (width > 80) width = 80;
		known = value_for(token, value, sizeof(value), ctx);
		if (!known) {
			used = append_bytes(output, outsz, used, open, (size_t)(close - open + 1));
		}
		else if (width == 0) {
			used = append_bytes(output, outsz, used, value, strlen(value));
		}
		else {
			size_t n = strlen(value), copy = n > width ? width : n;
			const char *src = value + (right && n > width ? n - width : 0);
			if (right && copy < width)
				for (size_t i = 0; i < width - copy; i++) used = append_bytes(output, outsz, used, " ", 1);
			used = append_bytes(output, outsz, used, src, copy);
			if (!right && copy < width)
				for (size_t i = 0; i < width - copy; i++) used = append_bytes(output, outsz, used, " ", 1);
		}
		p = close + 1;
	}
	return used;
}
