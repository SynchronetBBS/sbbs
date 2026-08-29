#include "deucegate.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "ini_file.h"
#include "str_list.h"
#include "utf8_codepages.h"

typedef enum {
	FLOW_CONTINUE,
	FLOW_MENU,
	FLOW_LOGOFF,
	FLOW_DISCONNECT
} flow_t;

static void
normalize_path(char *path)
{
	for (; *path != 0; path++)
		if (*path == '\\') *path = '/';
}

static unsigned
seconds_left(dg_client_t *client)
{
	time_t elapsed = time(NULL) - client->started;
	unsigned total = client->config->time_per_call * 60;
	return elapsed >= (time_t)total ? 0 : total - (unsigned)elapsed;
}

static void
make_context(dg_client_t *client, const char *menu, const char *filename, dg_mci_context_t *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->config = client->config;
	ctx->user = &client->user;
	ctx->menu_name = menu;
	ctx->filename = filename;
	ctx->remote_ip = client->remote_ip;
	ctx->node = client->node;
	ctx->seconds_left = seconds_left(client);
	ctx->now = time(NULL);
}

static bool
wait_key(dg_client_t *client, const char *prompt)
{
	int ch;
	if (!dg_client_puts(client, prompt)) return false;
	do ch = dg_client_getch(client, -1); while (ch == '\n' || ch == '\r');
	return ch >= 0;
}

static bool
select_display_path(dg_client_t *client, const char *requested, char *selected, size_t selected_sz)
{
	char base[DG_PATH_MAX], candidate[DG_PATH_MAX], req[DG_PATH_MAX];
	const char *ext;
	if (requested[0] == '@') {
		char index_path[DG_PATH_MAX];
		uint8_t *data;
		size_t len, count = 0, pick;
		char *line, *save = NULL;
		if (!dg_path_join(index_path, sizeof(index_path), client->config->root, requested + 1) ||
		    !dg_read_file(index_path, &data, &len))
			return false;
		for (line = strtok_r((char *)data, "\r\n", &save); line != NULL; line = strtok_r(NULL, "\r\n", &save))
			if (*dg_trim(line) != 0 && *dg_trim(line) != ';') count++;
		free(data);
		if (count == 0 || !dg_read_file(index_path, &data, &len)) return false;
		pick = (size_t)rand() % count; save = NULL;
		for (line = strtok_r((char *)data, "\r\n", &save); line != NULL; line = strtok_r(NULL, "\r\n", &save)) {
			line = dg_trim(line);
			if (*line == 0 || *line == ';') continue;
			if (pick-- == 0) {
				bool ok = select_display_path(client, line, selected, selected_sz);
				free(data); return ok;
			}
		}
		free(data); return false;
	}
	strncpy(req, requested, sizeof(req) - 1);
	req[sizeof(req) - 1] = 0;
	normalize_path(req);
	if (req[0] == '/' || req[0] == '\\' || (strlen(req) > 2 && req[1] == ':'))
		strncpy(base, req, sizeof(base) - 1);
	else if (!dg_path_join(base, sizeof(base), client->config->root, req))
		return false;
	base[sizeof(base) - 1] = 0;
	ext = strrchr(base, '.');
	if (ext != NULL && (strchr(ext, '/') == NULL && strchr(ext, '\\') == NULL)) {
		strncpy(selected, base, selected_sz - 1);
		selected[selected_sz - 1] = 0;
		return dg_file_exists(selected);
	}
	if (client->terminal == DG_TERM_RIP) ext = ".rip";
	else if (client->terminal == DG_TERM_ANSI) ext = ".ans";
	else ext = ".asc";
	if (snprintf(candidate, sizeof(candidate), "%s%s", base, ext) < (int)sizeof(candidate) && dg_file_exists(candidate)) {
		strncpy(selected, candidate, selected_sz - 1);
		selected[selected_sz - 1] = 0;
		return true;
	}
	return false;
}

bool
dg_display_file(dg_client_t *client, const char *path, bool more, bool pause,
    const dg_mci_context_t *ctx)
{
	char selected[DG_PATH_MAX];
	uint8_t *raw, *utf8;
	size_t rawlen, utf8len;
	char *expanded;
	unsigned lines = 0;
	if (!select_display_path(client, path, selected, sizeof(selected))) {
		dg_log(DG_LOG_WARN, "display file not found: %s", path);
		return false;
	}
	if (!dg_read_file(selected, &raw, &rawlen)) return false;
	for (size_t i = 0; i < rawlen; i++) {
		if (raw[i] == 0x1a) { rawlen = i; break; }
	}
	utf8 = cp_to_utf8(CIOLIB_CP437, (const char *)raw, rawlen, &utf8len);
	free(raw);
	if (utf8 == NULL) return false;
	expanded = malloc(utf8len * 4 + DG_MCI_MAX);
	if (expanded == NULL) { free(utf8); return false; }
	{
		dg_mci_context_t local = *ctx;
		local.filename = selected;
		dg_mci_expand((char *)utf8, expanded, utf8len * 4 + DG_MCI_MAX, &local);
	}
	free(utf8);
	for (char *p = expanded; *p != 0;) {
		char *marker = strstr(p, "{PAUSE}");
		char *nl = strstr(p, "\r\n");
		char *stop = marker;
		if (nl != NULL && (stop == NULL || nl < stop)) stop = nl + 2;
		if (stop == NULL) {
			if (!dg_client_puts(client, p)) break;
			p += strlen(p);
		}
		else {
			if (!dg_client_write(client, (uint8_t *)p, (size_t)(stop - p))) break;
			if (marker != NULL && stop == marker) {
				if (!wait_key(client, "\r\nPress any key to continue...")) break;
				p = marker + 7;
			}
			else {
				p = stop;
				if (more && ++lines >= 24) {
					int ch;
					dg_client_puts(client, "More [Q to quit]? ");
					ch = dg_client_getch(client, -1);
					dg_client_puts(client, "\r                    \r");
					if (ch == 'q' || ch == 'Q') break;
					lines = 0;
				}
			}
		}
	}
	free(expanded);
	return !pause || wait_key(client, "\r\nPress any key to continue...");
}

static flow_t
execute_action(dg_client_t *client, const char *action, const char *parameters,
    char *menu, size_t menusz)
{
	dg_mci_context_t ctx;
	make_context(client, menu, parameters, &ctx);
	if (dg_stricmp(action, "ChangeMenu") == 0 || dg_stricmp(action, "MainMenu") == 0) {
		strncpy(menu, parameters, menusz - 1);
		menu[menusz - 1] = 0;
		return FLOW_MENU;
	}
	if (dg_stricmp(action, "Disconnect") == 0) return FLOW_DISCONNECT;
	if (dg_stricmp(action, "LogOff") == 0) return FLOW_LOGOFF;
	if (dg_stricmp(action, "DisplayFile") == 0)
		dg_display_file(client, parameters, false, false, &ctx);
	else if (dg_stricmp(action, "DisplayFileMore") == 0)
		dg_display_file(client, parameters, true, false, &ctx);
	else if (dg_stricmp(action, "DisplayFilePause") == 0)
		dg_display_file(client, parameters, false, true, &ctx);
	else if (dg_stricmp(action, "Pause") == 0) {
		unsigned ms = (unsigned)strtoul(parameters, NULL, 10);
		SLEEP(ms);
	}
	else if (dg_stricmp(action, "RunDoor") == 0) {
		char err[512];
		if (!dg_run_door(client, parameters, err, sizeof(err))) {
			dg_log(DG_LOG_ERROR, "door %s: %s", parameters, err);
			dg_client_puts(client, "\r\nThat door is not available.\r\n");
		}
	}
	else if (dg_stricmp(action, "Telnet") == 0) {
		dg_log(DG_LOG_WARN, "disabled Telnet action requested by %s", client->user.alias);
		dg_client_puts(client, "\r\nTelnet actions are disabled on DeuceGate.\r\n");
	}
	return FLOW_CONTINUE;
}

static flow_t
run_process(dg_client_t *client, const char *relative, char *menu, size_t menusz)
{
	char path[DG_PATH_MAX];
	FILE *fp;
	str_list_t ini, sections;
	flow_t flow = FLOW_CONTINUE;
	if (!dg_path_join(path, sizeof(path), client->config->root, relative) ||
	    (fp = iniOpenFile(path, false)) == NULL)
		return flow;
	ini = iniReadFile(fp);
	iniCloseFile(fp);
	if (ini == NULL) return flow;
	sections = iniGetSectionList(ini, NULL);
	if (sections != NULL) {
		for (size_t i = 0; sections[i] != NULL; i++) {
			char action[64], params[DG_PATH_MAX];
			unsigned access = iniGetUInteger(ini, sections[i], "RequiredAccess", 0);
			if (client->user.access_level < access) continue;
			iniGetSString(ini, sections[i], "Action", "", action, sizeof(action));
			iniGetSString(ini, sections[i], "Parameters", "", params, sizeof(params));
			flow = execute_action(client, action, params, menu, menusz);
			if (flow != FLOW_CONTINUE) break;
		}
		strListFree(&sections);
	}
	strListFree(&ini);
	return flow;
}

static bool
display_menu(dg_client_t *client, const char *menu, str_list_t ini, str_list_t sections)
{
	char rel[DG_PATH_MAX], selected[DG_PATH_MAX], menu_file[64];
	dg_mci_context_t ctx;
	strncpy(menu_file, menu, sizeof(menu_file) - 1); menu_file[sizeof(menu_file) - 1] = 0;
	for (char *p = menu_file; *p != 0; p++) *p = (char)tolower((unsigned char)*p);
	snprintf(rel, sizeof(rel), "menus/%s%u", menu_file, client->user.access_level);
	if (!select_display_path(client, rel, selected, sizeof(selected)))
		snprintf(rel, sizeof(rel), "menus/%s", menu_file);
	make_context(client, menu, rel, &ctx);
	if (select_display_path(client, rel, selected, sizeof(selected)))
		return dg_display_file(client, rel, false, false, &ctx);
	dg_client_puts(client, "\r\n");
	for (size_t i = 0; sections != NULL && sections[i] != NULL; i++) {
		char name[256];
		if (client->user.access_level < iniGetUInteger(ini, sections[i], "RequiredAccess", 0)) continue;
		iniGetSString(ini, sections[i], "Name", sections[i], name, sizeof(name));
		dg_client_puts(client, "["); dg_client_puts(client, sections[i]);
		dg_client_puts(client, "] "); dg_client_puts(client, name); dg_client_puts(client, "\r\n");
	}
	return true;
}

int
dg_run_session(dg_client_t *client)
{
	char menu[64] = "MAIN";
	flow_t flow;
	client->started = time(NULL);
	flow = run_process(client, "config/logonprocess.ini", menu, sizeof(menu));
	if (flow == FLOW_DISCONNECT) return 0;
	if (flow == FLOW_LOGOFF) goto logoff;
	for (;;) {
		char rel[DG_PATH_MAX], path[DG_PATH_MAX], menu_file[64];
		FILE *fp;
		str_list_t ini, sections;
		int ch;
		if (seconds_left(client) == 0) {
			dg_client_puts(client, "\r\nYour time for this call has expired.\r\n");
			break;
		}
		strncpy(menu_file, menu, sizeof(menu_file) - 1); menu_file[sizeof(menu_file) - 1] = 0;
		for (char *p = menu_file; *p != 0; p++) *p = (char)tolower((unsigned char)*p);
		snprintf(rel, sizeof(rel), "menus/%s.ini", menu_file);
		if (!dg_path_join(path, sizeof(path), client->config->root, rel) || (fp = iniOpenFile(path, false)) == NULL) {
			dg_client_puts(client, "\r\nMenu configuration is missing.\r\n");
			break;
		}
		ini = iniReadFile(fp); iniCloseFile(fp);
		if (ini == NULL) break;
		sections = iniGetSectionList(ini, NULL);
		display_menu(client, menu, ini, sections);
		dg_client_puts(client, "\r\nSelection: ");
		ch = dg_client_getch(client, -1);
		if (ch < 0) { strListFree(&sections); strListFree(&ini); return 0; }
		if (isprint((unsigned char)ch)) {
			char hotkey[2] = {(char)ch, 0}, action[64], params[DG_PATH_MAX];
			if (iniGetExistingString(ini, hotkey, "Action", "", action) != NULL &&
			    client->user.access_level >= iniGetUInteger(ini, hotkey, "RequiredAccess", 0)) {
				iniGetSString(ini, hotkey, "Parameters", "", params, sizeof(params));
				flow = execute_action(client, action, params, menu, sizeof(menu));
				strListFree(&sections); strListFree(&ini);
				if (flow == FLOW_DISCONNECT) return 0;
				if (flow == FLOW_LOGOFF) break;
				continue;
			}
		}
		strListFree(&sections); strListFree(&ini);
	}
logoff:
	run_process(client, "config/logoffprocess.ini", menu, sizeof(menu));
	return 0;
}
