#include "deucegate.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh-conn.h"
#include "dirwrap.h"
#include "ini_file.h"
#include "str_list.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__FreeBSD__)
#include <libutil.h>
#elif defined(__APPLE__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <util.h>
#else
#include <pty.h>
#endif
#endif

typedef struct {
	char name[128];
	char command[DG_PATH_MAX];
	char parameters[DG_PATH_MAX * 2];
	char platform[32];
	char emulator[32];
	char io[32];
	dg_encoding_t encoding;
	bool auto_encoding;
	unsigned force_quit_delay;
	bool watch_dtr;
} dg_door_t;

typedef struct {
#ifdef _WIN32
	PROCESS_INFORMATION process;
	HANDLE job;
	HANDLE pipe_in;
	HANDLE pipe_out;
#else
	pid_t pid;
#endif
	SOCKET io;
	bool io_is_socket;
} dg_child_t;

#define DG_DOOR_ENCODING_ENV "DEUCEGATE_ENCODING="
#define DG_DOOR_LANGUAGE_ENV "DEUCEGATE_LANGUAGE_TAG="

static const char *
door_encoding_name(dg_encoding_t encoding)
{
	return encoding == DG_UTF8 ? "UTF-8" : "CP437";
}

static dg_encoding_t
effective_door_encoding(const dg_client_t *client, const dg_door_t *door)
{
	return door->auto_encoding ? client->client_encoding : door->encoding;
}

#ifdef _WIN32
static bool
is_door_environment_entry(const char *entry)
{
	return strnicmp(entry, DG_DOOR_ENCODING_ENV, sizeof(DG_DOOR_ENCODING_ENV) - 1) == 0
	    || strnicmp(entry, DG_DOOR_LANGUAGE_ENV, sizeof(DG_DOOR_LANGUAGE_ENV) - 1) == 0;
}

static char *
door_environment_block(dg_encoding_t encoding, const char *language)
{
	LPCH inherited = GetEnvironmentStringsA();
	char encoding_assignment[sizeof(DG_DOOR_ENCODING_ENV) + 8];
	char language_assignment[sizeof(DG_DOOR_LANGUAGE_ENV) + DG_LANGUAGE_TAG_MAX + 1];
	const char *assignments[2] = {encoding_assignment, language_assignment};
	size_t total = 1, next_assignment = 0;
	char *block, *out;

	snprintf(encoding_assignment, sizeof(encoding_assignment), "%s%s", DG_DOOR_ENCODING_ENV,
	    door_encoding_name(encoding));
	snprintf(language_assignment, sizeof(language_assignment), "%s%s", DG_DOOR_LANGUAGE_ENV, language);
	for (size_t i = 0; i < 2; i++)
		total += strlen(assignments[i]) + 1;
	for (const char *entry = inherited; entry != NULL && *entry != 0; entry += strlen(entry) + 1) {
		if (!is_door_environment_entry(entry))
			total += strlen(entry) + 1;
	}
	block = malloc(total);
	if (block == NULL) {
		if (inherited != NULL) FreeEnvironmentStringsA(inherited);
		return NULL;
	}
	out = block;
	for (const char *entry = inherited; entry != NULL && *entry != 0; entry += strlen(entry) + 1) {
		size_t len;
		if (is_door_environment_entry(entry))
			continue;
		while (next_assignment < 2 && dg_stricmp(entry, assignments[next_assignment]) > 0) {
			len = strlen(assignments[next_assignment]);
			memcpy(out, assignments[next_assignment], len + 1);
			out += len + 1;
			next_assignment++;
		}
		len = strlen(entry);
		memcpy(out, entry, len + 1);
		out += len + 1;
	}
	while (next_assignment < 2) {
		size_t len = strlen(assignments[next_assignment]);

		memcpy(out, assignments[next_assignment], len + 1);
		out += len + 1;
		next_assignment++;
	}
	*out = 0;
	if (inherited != NULL) FreeEnvironmentStringsA(inherited);
	return block;
}
#else
extern char **environ;

static void
free_door_environment(char **environment)
{
	if (environment == NULL)
		return;
	for (size_t i = 0; environment[i] != NULL; i++)
		free(environment[i]);
	free(environment);
}

static char **
door_environment(dg_encoding_t encoding, const char *language)
{
	size_t count = 0, used = 0;
	char **environment;

	while (environ != NULL && environ[count] != NULL)
		count++;
	environment = calloc(count + 3, sizeof(*environment));
	if (environment == NULL)
		return NULL;
	for (size_t i = 0; i < count; i++) {
		size_t len;
		if (strncmp(environ[i], DG_DOOR_ENCODING_ENV, sizeof(DG_DOOR_ENCODING_ENV) - 1) == 0
		    || strncmp(environ[i], DG_DOOR_LANGUAGE_ENV, sizeof(DG_DOOR_LANGUAGE_ENV) - 1) == 0)
			continue;
		len = strlen(environ[i]);
		environment[used] = malloc(len + 1);
		if (environment[used] == NULL) {
			free_door_environment(environment);
			return NULL;
		}
		memcpy(environment[used++], environ[i], len + 1);
	}
	{
		const char *value = door_encoding_name(encoding);
		size_t len = sizeof(DG_DOOR_ENCODING_ENV) - 1 + strlen(value);
		environment[used] = malloc(len + 1);
		if (environment[used] == NULL) {
			free_door_environment(environment);
			return NULL;
		}
		snprintf(environment[used++], len + 1, "%s%s", DG_DOOR_ENCODING_ENV, value);
	}
	{
		size_t len = sizeof(DG_DOOR_LANGUAGE_ENV) - 1 + strlen(language);

		environment[used] = malloc(len + 1);
		if (environment[used] == NULL) {
			free_door_environment(environment);
			return NULL;
		}
		snprintf(environment[used], len + 1, "%s%s", DG_DOOR_LANGUAGE_ENV, language);
	}
	return environment;
}
#endif

static unsigned
time_left(const dg_client_t *client)
{
	unsigned total = client->config->time_per_call * 60;
	time_t elapsed = time(NULL) - client->started;
	return elapsed >= (time_t)total ? 0 : total - (unsigned)elapsed;
}

static bool
write_text(const char *path, const char *text)
{
	return dg_write_atomic(path, text, strlen(text), 0600);
}

static bool
node_path(const dg_client_t *client, const char *name, char *out, size_t outsz)
{
	char rel[128];
	snprintf(rel, sizeof(rel), "node%u/%s", client->node, name);
	return dg_path_join(out, outsz, client->config->root, rel);
}

bool
dg_create_drop_files(const dg_client_t *client, SOCKET handle, char *node_dir, size_t node_dir_sz)
{
	char path[DG_PATH_MAX], text[16384], dropname[64], alias_upper[DG_ALIAS_MAX];
	unsigned seconds = time_left(client), minutes = seconds / 60;
	int terminal = client->terminal == DG_TERM_RIP ? 3 : client->terminal == DG_TERM_ANSI ? 1 : 0;
	time_t now = time(NULL);
	struct tm login_tm;
	snprintf(path, sizeof(path), "node%u", client->node);
	if (!dg_path_join(node_dir, node_dir_sz, client->config->root, path) ||
	    (!dg_dir_exists(node_dir) && mkpath(node_dir) != 0))
		return false;
	snprintf(text, sizeof(text),
	    "COM1:\r\n57600\r\n8\r\n%u\r\n57600\r\nY\r\nY\r\nY\r\nY\r\n%s\r\n"
	    "City, State\r\n555-555-5555\r\n555-555-5555\r\nPASSWORD\r\n%u\r\n1\r\n00/00/00\r\n"
	    "%u\r\n%u\r\nGR\r\n24\r\nN\r\n\r\n\r\n00/00/00\r\n%u\r\nZ\r\n0\r\n0\r\n0\r\n0\r\n"
	    "00/00/00\r\n%s\r\n%s\r\n%s\r\n%s\r\n00:00\r\nY\r\n%s\r\nY\r\n7\r\n0\r\n"
	    "00/00/00\r\n00:00\r\n00:00\r\n0\r\n0\r\n0\r\n0\r\nNo Comment\r\n0\r\n0",
	    client->node, client->user.alias, client->user.access_level, seconds, minutes,
	    client->user.id ? client->user.id - 1 : 0, client->config->root, client->config->root,
	    client->config->sysop_name, client->user.alias,
	    client->terminal == DG_TERM_ASCII ? "N" : "Y");
	if (!node_path(client, "door.sys", path, sizeof(path)) || !write_text(path, text)) return false;
	snprintf(text, sizeof(text), "2\r\n%llu\r\n57600\r\nDeuceGate v0.1\r\n%u\r\n%s\r\n%s\r\n%u\r\n%u\r\n%d\r\n%u",
	    (unsigned long long)(uintptr_t)handle, client->user.id, client->user.alias, client->user.alias,
	    client->user.access_level, minutes, terminal, client->node);
	if (!node_path(client, "door32.sys", path, sizeof(path)) || !write_text(path, text)) return false;
	snprintf(text, sizeof(text), "%s\r\n%d\r\n1\r\n24\r\n57600\r\n1\r\n%u\r\n%s",
	    client->user.alias, terminal == 0 ? 0 : 1, minutes, client->user.alias);
	if (!node_path(client, "doorfile.sr", path, sizeof(path)) || !write_text(path, text)) return false;
	snprintf(text, sizeof(text), "%s\r\n%s\r\n%s\r\nCOM1\r\n57600 BAUD,N,8,1\r\n0\r\n%s\r\n\r\n"
	    "City, State\r\n%d\r\n%u\r\n%u\r\n1", client->config->bbs_name,
	    client->config->sysop_first_name, client->config->sysop_last_name,
	    client->user.alias, terminal == 0 ? 0 : 1, client->user.access_level, minutes);
	if (!node_path(client, "dorinfo.def", path, sizeof(path)) || !write_text(path, text)) return false;
	if (!node_path(client, "dorinfo1.def", path, sizeof(path)) || !write_text(path, text)) return false;
	snprintf(dropname, sizeof(dropname), "dorinfo%u.def", client->node);
	if (!node_path(client, dropname, path, sizeof(path)) || !write_text(path, text)) return false;
	strncpy(alias_upper, client->user.alias, sizeof(alias_upper) - 1);
	alias_upper[sizeof(alias_upper) - 1] = 0;
	for (char *p = alias_upper; *p != 0; p++) *p = (char)toupper((unsigned char)*p);
#ifdef _WIN32
	localtime_s(&login_tm, &client->started);
#else
	localtime_r(&client->started, &login_tm);
#endif
	snprintf(text, sizeof(text),
	    "%u\r\n%s\r\n%s\r\n\r\n0\r\n?\r\n0\r\n00/00/00\r\n%u\r\n%u\r\n%u\r\n0\r\n%d\r\n%d\r\n1\r\n"
	    "%u\r\n%s\r\n%s\r\nnode.log\r\n57600\r\n1\r\n%s\r\n%s\r\n%d\r\n%u\r\n0\r\n0\r\n0\r\n0\r\n8N1",
	    client->user.id, alias_upper, alias_upper, client->cols, client->rows,
	    client->user.access_level, client->user.access_level >= 100 ? 1 : 0,
	    terminal == 0 ? 0 : 1, seconds, node_dir, client->config->root,
	    client->config->bbs_name, client->config->sysop_name,
	    login_tm.tm_hour * 3600 + login_tm.tm_min * 60 + login_tm.tm_sec,
	    (unsigned)(now - client->started));
	return node_path(client, "chain.txt", path, sizeof(path)) && write_text(path, text);
}

static void
cleanup_node(const dg_client_t *client)
{
	static const char *files[] = {"door.sys", "door32.sys", "doorfile.sr", "dorinfo.def", "dorinfo1.def", "chain.txt",
	    "deucegate-dosbox.conf", "external.bat", "dosemu.log", NULL};
	char path[DG_PATH_MAX], node[DG_PATH_MAX], name[64];
	for (size_t i = 0; files[i] != NULL; i++)
		if (node_path(client, files[i], path, sizeof(path))) remove(path);
	snprintf(name, sizeof(name), "dorinfo%u.def", client->node);
	if (node_path(client, name, path, sizeof(path))) remove(path);
	snprintf(name, sizeof(name), "node%u", client->node);
	if (dg_path_join(node, sizeof(node), client->config->root, name)) rmdir(node);
}

static bool
load_door(const dg_config_t *cfg, const char *name, dg_door_t *door, char *err, size_t errsz)
{
	char rel[DG_PATH_MAX], path[DG_PATH_MAX], encoding[32], door_file[DG_ALIAS_MAX];
	FILE *fp;
	str_list_t ini;
	strncpy(door_file, name, sizeof(door_file) - 1); door_file[sizeof(door_file) - 1] = 0;
	for (char *p = door_file; *p != 0; p++) *p = (char)tolower((unsigned char)*p);
	snprintf(rel, sizeof(rel), "doors/%s.ini", door_file);
	if (!dg_path_join(path, sizeof(path), cfg->root, rel) || (fp = iniOpenFile(path, false)) == NULL) {
		snprintf(err, errsz, "door configuration %s was not found", rel);
		return false;
	}
	ini = iniReadFile(fp); iniCloseFile(fp);
	if (ini == NULL) { snprintf(err, errsz, "cannot parse %s", path); return false; }
	memset(door, 0, sizeof(*door));
	iniGetSString(ini, "DOOR", "Name", name, door->name, sizeof(door->name));
	iniGetSString(ini, "DOOR", "Command", "", door->command, sizeof(door->command));
	iniGetSString(ini, "DOOR", "Parameters", "", door->parameters, sizeof(door->parameters));
	iniGetSString(ini, "DOOR", "Platform", "", door->platform, sizeof(door->platform));
	if (!*door->platform) strcpy(door->platform, iniGetBool(ini, "DOOR", "Native", false) ? "Windows" : "DOS");
	iniGetSString(ini, "DOOR", "Encoding", "CP437", encoding, sizeof(encoding));
	door->auto_encoding = dg_stricmp(encoding, "Auto") == 0;
	door->encoding = dg_stricmp(encoding, "UTF-8") == 0 || dg_stricmp(encoding, "UTF8") == 0 ? DG_UTF8 : DG_CP437;
	iniGetSString(ini, "DOOR", "Emulator", "Auto", door->emulator, sizeof(door->emulator));
	iniGetSString(ini, "DOOR", "IO", "Socket", door->io, sizeof(door->io));
	door->force_quit_delay = iniGetIntInRange(ini, "DOOR", "ForceQuitDelay", 0, 5, 300);
	door->watch_dtr = iniGetBool(ini, "DOOR", "WatchDTR", true);
	strListFree(&ini);
	if (!*door->command) { snprintf(err, errsz, "door Command is empty"); return false; }
	return true;
}

static void
replace_all(char *buf, size_t bufsz, const char *needle, const char *replacement)
{
	char temp[DG_PATH_MAX * 4];
	char *at;
	while ((at = strstr(buf, needle)) != NULL) {
		size_t prefix = (size_t)(at - buf), nlen = strlen(needle), rlen = strlen(replacement);
		if (prefix + rlen + strlen(at + nlen) + 1 > sizeof(temp) ||
		    prefix + rlen + strlen(at + nlen) + 1 > bufsz) return;
		memcpy(temp, buf, prefix); memcpy(temp + prefix, replacement, rlen);
		strcpy(temp + prefix + rlen, at + nlen); strcpy(buf, temp);
	}
}

static void
expand_command(const dg_client_t *client, SOCKET handle, char *command, size_t commandsz,
    char *parameters, size_t paramsz)
{
	struct { const char *key; char value[DG_PATH_MAX]; } macros[16];
	unsigned seconds = time_left(client);
	memset(macros, 0, sizeof(macros));
	macros[0].key = "*DORINFOx"; snprintf(macros[0].value, sizeof(macros[0].value), "%s/node%u/dorinfo%u.def", client->config->root, client->node, client->node);
	macros[1].key = "*DORINFO1"; snprintf(macros[1].value, sizeof(macros[1].value), "%s/node%u/dorinfo1.def", client->config->root, client->node);
	macros[2].key = "*DORINFO"; snprintf(macros[2].value, sizeof(macros[2].value), "%s/node%u/dorinfo.def", client->config->root, client->node);
	macros[3].key = "*DOOR32"; snprintf(macros[3].value, sizeof(macros[3].value), "%s/node%u/door32.sys", client->config->root, client->node);
	macros[4].key = "*DOORSYS"; snprintf(macros[4].value, sizeof(macros[4].value), "%s/node%u/door.sys", client->config->root, client->node);
	macros[5].key = "*DOORFILE"; snprintf(macros[5].value, sizeof(macros[5].value), "%s/node%u/doorfile.sr", client->config->root, client->node);
	macros[6].key = "*CHAIN"; snprintf(macros[6].value, sizeof(macros[6].value), "%s/node%u/chain.txt", client->config->root, client->node);
	macros[7].key = "*SOCKETHANDLE"; snprintf(macros[7].value, sizeof(macros[7].value), "%llu", (unsigned long long)(uintptr_t)handle);
	macros[8].key = "*HANDLE"; strcpy(macros[8].value, macros[7].value);
	macros[9].key = "*IPADDRESS"; strncpy(macros[9].value, client->remote_ip, sizeof(macros[9].value) - 1);
	macros[10].key = "*MINUTESLEFT"; snprintf(macros[10].value, sizeof(macros[10].value), "%u", seconds / 60);
	macros[11].key = "*SECONDSLEFT"; snprintf(macros[11].value, sizeof(macros[11].value), "%u", seconds);
	macros[12].key = "*NODE"; snprintf(macros[12].value, sizeof(macros[12].value), "%u", client->node);
	macros[13].key = "***ALIAS"; strncpy(macros[13].value, client->user.alias, sizeof(macros[13].value) - 1);
	macros[14].key = "***USERNAME"; strcpy(macros[14].value, macros[13].value);
	macros[15].key = "***PASSWORD"; strncpy(macros[15].value, client->user.password_hash, sizeof(macros[15].value) - 1);
	for (size_t i = 0; i < 16; i++) {
		replace_all(command, commandsz, macros[i].key, macros[i].value);
		replace_all(parameters, paramsz, macros[i].key, macros[i].value);
	}
	if (*client->user.ini_path) {
		FILE *fp = iniOpenFile(client->user.ini_path, false);
		if (fp != NULL) {
			str_list_t ini = iniReadFile(fp), keys;
			iniCloseFile(fp);
			keys = ini != NULL ? iniGetKeyList(ini, "USER") : NULL;
			for (size_t i = 0; keys != NULL && keys[i] != NULL; i++) {
				if (strnicmp(keys[i], "AdditionalInfo_", 15) == 0) {
					char macro[192], value[DG_PATH_MAX];
					snprintf(macro, sizeof(macro), "***%s", keys[i] + 15);
					iniGetSString(ini, "USER", keys[i], "", value, sizeof(value));
					replace_all(command, commandsz, macro, value);
					replace_all(parameters, paramsz, macro, value);
				}
			}
			if (keys != NULL) strListFree(&keys);
			if (ini != NULL) strListFree(&ini);
		}
	}
}

static int
child_write(dg_child_t *child, const uint8_t *buf, size_t len)
{
#ifdef _WIN32
	if (child->io_is_socket)
		return send(child->io, (socket_send_buffer_t)buf, (int)len, 0);
	else {
		DWORD wrote = 0;
		return WriteFile(child->pipe_in, buf, (DWORD)len, &wrote, NULL) ? (int)wrote : -1;
	}
#else
	return child->io_is_socket ? send(child->io, buf, len, 0) : (int)write(child->io, buf, len);
#endif
}

static int
child_read(dg_child_t *child, uint8_t *buf, size_t len)
{
#ifdef _WIN32
	if (child->io_is_socket)
		return recv(child->io, (socket_recv_buffer_t)buf, (int)len, 0);
	else {
		DWORD got = 0;
		return ReadFile(child->pipe_out, buf, (DWORD)len, &got, NULL) ? (int)got : -1;
	}
#else
	return child->io_is_socket ? recv(child->io, buf, len, 0) : (int)read(child->io, buf, len);
#endif
}

static bool
child_read_ready(dg_child_t *child)
{
#ifdef _WIN32
	if (!child->io_is_socket) {
		DWORD available = 0;
		return PeekNamedPipe(child->pipe_out, NULL, 0, NULL, &available, NULL) && available > 0;
	}
#endif
	{
		fd_set reads; struct timeval tv = {0, 0};
		FD_ZERO(&reads); FD_SET(child->io, &reads);
		return select((int)child->io + 1, &reads, NULL, NULL, &tv) > 0;
	}
}

static bool
relay_io(dg_client_t *client, const dg_door_t *door, dg_child_t *child)
{
	dg_decoder_t from_client, from_door;
	uint8_t raw[DG_IO_BUFSZ], utf8[DG_IO_BUFSZ * 4], encoded[DG_IO_BUFSZ * 4];
	dssh_channel channel = client->channel;
	dg_encoding_t encoding = effective_door_encoding(client, door);
	bool connected = true;
	dg_decoder_init(&from_client, client->client_encoding);
	dg_decoder_init(&from_door, encoding);
	while (connected) {
		int ev = dssh_chan_poll(channel, DSSH_POLL_READ | DSSH_POLL_EVENT, 10);
		if (ev < 0) break;
		if (ev & DSSH_POLL_EVENT) {
			struct dssh_chan_event event;
			while (dssh_chan_read_event(channel, &event) == 0)
				if (event.type == DSSH_EVENT_EOF || event.type == DSSH_EVENT_CLOSE) connected = false;
		}
		if (ev & DSSH_POLL_READ) {
			int64_t n = dssh_chan_read(channel, 0, raw, sizeof(raw));
			const uint8_t *output;
			size_t output_len;

			if (n <= 0) break;
			output = raw;
			output_len = (size_t)n;
			if (!door->auto_encoding) {
				size_t u = dg_decode(&from_client, raw, (size_t)n, utf8, sizeof(utf8), false);

				output_len = dg_encode(encoding, utf8, u, encoded, sizeof(encoded));
				output = encoded;
			}
			for (size_t p = 0; p < output_len;) {
				int wrote = child_write(child, output + p, output_len - p);
				if (wrote <= 0) { connected = false; break; }
				p += (size_t)wrote;
			}
		}
		{
			if (child_read_ready(child)) {
				int n = child_read(child, raw, sizeof(raw));
				if (n <= 0) break;
				if (door->auto_encoding) {
					if (!dg_client_write_raw(client, raw, (size_t)n)) break;
				}
				else {
					size_t u = dg_decode(&from_door, raw, (size_t)n, utf8, sizeof(utf8), false);

					if (!dg_client_write(client, utf8, u)) break;
				}
			}
		}
#ifdef _WIN32
		if (WaitForSingleObject(child->process.hProcess, 0) == WAIT_OBJECT_0) break;
#else
		{
			int status;
			if (waitpid(child->pid, &status, WNOHANG) == child->pid) { child->pid = -1; break; }
		}
#endif
	}
	return connected;
}

#ifndef _WIN32
static bool
spawn_native_posix(const dg_client_t *client, const dg_door_t *door, SOCKET pair[2],
    dg_child_t *child, char *err, size_t errsz)
{
	char command[DG_PATH_MAX * 4], cmdpath[DG_PATH_MAX], params[DG_PATH_MAX * 2], node_tmp[DG_PATH_MAX];
	char **environment;
	bool stdio = dg_stricmp(door->io, "Stdio") == 0;
	int ptyfd = -1;
	pid_t pid;
	snprintf(cmdpath, sizeof(cmdpath), "%s", door->command);
	if (cmdpath[0] != '/' && !dg_path_join(command, sizeof(command), client->config->root, cmdpath)) return false;
	else if (cmdpath[0] == '/') snprintf(command, sizeof(command), "%s", cmdpath);
	snprintf(params, sizeof(params), "%s", door->parameters);
	environment = door_environment(effective_door_encoding(client, door), client->language_tag);
	if (environment == NULL) {
		snprintf(err, errsz, "cannot create door environment");
		return false;
	}
	if (stdio) {
		pid = forkpty(&ptyfd, NULL, NULL, NULL);
		pair[0] = ptyfd; pair[1] = 0;
	}
	else {
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) != 0) {
			free_door_environment(environment);
			snprintf(err, errsz, "socketpair failed"); return false;
		}
		if (!dg_create_drop_files(client, pair[1], node_tmp, sizeof(node_tmp))) {
			closesocket(pair[0]); closesocket(pair[1]);
			free_door_environment(environment);
			snprintf(err, errsz, "cannot update drop files"); return false;
		}
		pid = fork();
	}
	if (pid < 0) {
		if (!stdio) { closesocket(pair[0]); closesocket(pair[1]); }
		free_door_environment(environment);
		snprintf(err, errsz, "fork failed: %s", strerror(errno)); return false;
	}
	if (pid == 0) {
		if (!stdio) closesocket(pair[0]);
		setsid();
		chdir(client->config->root);
		expand_command(client, stdio ? 0 : pair[1], command, sizeof(command), params, sizeof(params));
		{
			char shell[sizeof(command) + sizeof(params) + 8];
			snprintf(shell, sizeof(shell), "\"%s\" %s", command, params);
			execle("/bin/sh", "sh", "-c", shell, (char *)NULL, environment);
		}
		_exit(127);
	}
	free_door_environment(environment);
	if (!stdio) closesocket(pair[1]);
	child->pid = pid; child->io = pair[0]; child->io_is_socket = !stdio;
	return true;
}
#else
static bool
spawn_native_windows(const dg_client_t *client, const dg_door_t *door, SOCKET pair[2],
    dg_child_t *child, char *err, size_t errsz)
{
	STARTUPINFOA si = {0};
	char command[DG_PATH_MAX * 4], params[DG_PATH_MAX * 2], cmdpath[DG_PATH_MAX];
	char command_line[DG_PATH_MAX * 6], node_tmp[DG_PATH_MAX];
	SECURITY_ATTRIBUTES sa = {.nLength = sizeof(sa), .bInheritHandle = TRUE};
	HANDLE child_stdin = NULL, child_stdout = NULL;
	char *environment;
	bool stdio = dg_stricmp(door->io, "Stdio") == 0;
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};
	if (stdio) {
		if (!CreatePipe(&child_stdin, &child->pipe_in, &sa, 0) ||
		    !CreatePipe(&child->pipe_out, &child_stdout, &sa, 0)) {
			snprintf(err, errsz, "CreatePipe failed: %lu", GetLastError()); return false;
		}
		SetHandleInformation(child->pipe_in, HANDLE_FLAG_INHERIT, 0);
		SetHandleInformation(child->pipe_out, HANDLE_FLAG_INHERIT, 0);
	}
	else {
		if (socketpair(AF_INET, SOCK_STREAM, 0, pair) != 0) { snprintf(err, errsz, "socketpair failed"); return false; }
		if (!dg_create_drop_files(client, pair[1], node_tmp, sizeof(node_tmp))) {
			closesocket(pair[0]); closesocket(pair[1]);
			snprintf(err, errsz, "cannot update drop files"); return false;
		}
	}
	snprintf(cmdpath, sizeof(cmdpath), "%s", door->command);
	if ((strlen(cmdpath) < 2 || cmdpath[1] != ':') && !dg_path_join(command, sizeof(command), client->config->root, cmdpath)) return false;
	else if (strlen(cmdpath) > 1 && cmdpath[1] == ':') snprintf(command, sizeof(command), "%s", cmdpath);
	snprintf(params, sizeof(params), "%s", door->parameters);
	expand_command(client, stdio ? 0 : pair[1], command, sizeof(command), params, sizeof(params));
	if (snprintf(command_line, sizeof(command_line), "\"%s\" %s", command, params) >= (int)sizeof(command_line)) {
		snprintf(err, errsz, "expanded door command line is too long"); return false;
	}
	si.cb = sizeof(si);
	if (stdio) {
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdInput = child_stdin; si.hStdOutput = child_stdout; si.hStdError = child_stdout;
	}
	environment = door_environment_block(effective_door_encoding(client, door), client->language_tag);
	if (environment == NULL) {
		if (stdio) {
			CloseHandle(child_stdin); CloseHandle(child->pipe_in);
			CloseHandle(child->pipe_out); CloseHandle(child_stdout);
		}
		else { closesocket(pair[0]); closesocket(pair[1]); }
		snprintf(err, errsz, "cannot create door environment"); return false;
	}
	memset(&child->process, 0, sizeof(child->process));
	if (!CreateProcessA(NULL, command_line, NULL, NULL, TRUE, CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED, environment,
	    client->config->root, &si, &child->process)) {
		DWORD create_error = GetLastError();
		free(environment);
		snprintf(err, errsz, "CreateProcess failed: %lu", create_error); return false;
	}
	free(environment);
	child->job = CreateJobObjectA(NULL, NULL);
	if (child->job != NULL) {
		limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		SetInformationJobObject(child->job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
		AssignProcessToJobObject(child->job, child->process.hProcess);
	}
	ResumeThread(child->process.hThread);
	if (stdio) {
		CloseHandle(child_stdin); CloseHandle(child_stdout);
		child->io = INVALID_SOCKET; child->io_is_socket = false;
	}
	else {
		closesocket(pair[1]); child->io = pair[0]; child->io_is_socket = true;
	}
	return true;
}
#endif

#ifdef _WIN32
static bool
command_available_windows(const char *command)
{
	char found[DG_PATH_MAX];
	if (strchr(command, '\\') != NULL || strchr(command, '/') != NULL || strchr(command, ':') != NULL)
		return GetFileAttributesA(command) != INVALID_FILE_ATTRIBUTES;
	return SearchPathA(NULL, command, ".exe", sizeof(found), found, NULL) > 0;
}
#else
static bool
command_available_posix(const char *command)
{
	const char *path;
	char copy[8192], *part, *save = NULL;
	if (strchr(command, '/') != NULL) return access(command, X_OK) == 0;
	path = getenv("PATH");
	if (path == NULL || strlen(path) >= sizeof(copy)) return false;
	strcpy(copy, path);
	for (part = strtok_r(copy, ":", &save); part != NULL; part = strtok_r(NULL, ":", &save)) {
		char candidate[DG_PATH_MAX];
		if (dg_path_join(candidate, sizeof(candidate), part, command) && access(candidate, X_OK) == 0)
			return true;
	}
	return false;
}
#endif

static const char *
dos_backend(const dg_client_t *client, const dg_door_t *door, char *kind, size_t kindsz)
{
	const char *candidate;
	bool auto_mode = dg_stricmp(door->emulator, "Auto") == 0 || !*door->emulator;
	(void)kindsz;
#ifdef _WIN32
#define COMMAND_AVAILABLE(cmd) command_available_windows(cmd)
#else
#define COMMAND_AVAILABLE(cmd) command_available_posix(cmd)
#endif
	if (auto_mode || dg_stricmp(door->emulator, "DOSBox-X") == 0) {
		candidate = *client->config->dosbox_x_path ? client->config->dosbox_x_path : "dosbox-x";
		if (COMMAND_AVAILABLE(candidate)) { strcpy(kind, "DOSBox-X"); return candidate; }
		if (!auto_mode) return NULL;
	}
	if (auto_mode || dg_stricmp(door->emulator, "DOSBox") == 0) {
		candidate = *client->config->dosbox_path ? client->config->dosbox_path : "dosbox";
		if (COMMAND_AVAILABLE(candidate)) { strcpy(kind, "DOSBox"); return candidate; }
		if (!auto_mode) return NULL;
	}
#ifndef _WIN32
	if (auto_mode || dg_stricmp(door->emulator, "DOSEMU") == 0) {
		candidate = *client->config->dosemu_path ? client->config->dosemu_path : "dosemu";
		if (COMMAND_AVAILABLE(candidate)) { strcpy(kind, "DOSEMU"); return candidate; }
		if (!*client->config->dosemu_path && COMMAND_AVAILABLE("dosemu.bin")) {
			strcpy(kind, "DOSEMU"); return "dosemu.bin";
		}
	}
#endif
	return NULL;
#undef COMMAND_AVAILABLE
}

#ifndef _WIN32
static bool
spawn_dos_posix(const dg_client_t *client, const dg_door_t *door, dg_child_t *child,
    char *err, size_t errsz)
{
	char kind[32], conf[DG_PATH_MAX], base_conf[DG_PATH_MAX], command[DG_PATH_MAX];
	char params[DG_PATH_MAX * 2], text[8192];
	const char *emulator = dos_backend(client, door, kind, sizeof(kind));
	SOCKET listener;
	struct sockaddr_in addr = {.sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
	socklen_t alen = sizeof(addr);
	pid_t pid;
	if (emulator == NULL) {
		snprintf(err, errsz, "requested DOS emulator is not installed or not executable");
		return false;
	}
	if (dg_stricmp(kind, "DOSEMU") == 0) {
		char batch[DG_PATH_MAX], batch_text[8192];
		int ptyfd;
		snprintf(command, sizeof(command), "%s", door->command);
		snprintf(params, sizeof(params), "%s", door->parameters);
		expand_command(client, 0, command, sizeof(command), params, sizeof(params));
		replace_all(command, sizeof(command), client->config->root, "G:");
		replace_all(params, sizeof(params), client->config->root, "G:");
		if (!node_path(client, "external.bat", batch, sizeof(batch))) return false;
		snprintf(batch_text, sizeof(batch_text),
		    "@echo off\r\nlredir g: linux\\fs%s\r\nset path=%%path%%;g:\\dosutils\r\n"
		    "set DEUCEGATE_ENCODING=%s\r\nset DEUCEGATE_LANGUAGE_TAG=%s\r\n"
		    "fossil.com\r\nshare.com\r\nansi.com\r\ng:\r\n%s %s\r\nexitemu\r\n",
		    client->config->root, door_encoding_name(effective_door_encoding(client, door)),
		    client->language_tag, command, params);
		if (!write_text(batch, batch_text)) return false;
		pid = forkpty(&ptyfd, NULL, NULL, NULL);
		if (pid < 0) { snprintf(err, errsz, "forkpty failed: %s", strerror(errno)); return false; }
		if (pid == 0) {
			char home[DG_PATH_MAX + 8], execute[DG_PATH_MAX + 4], log[DG_PATH_MAX + 4];
			snprintf(home, sizeof(home), "HOME=%s", client->config->root);
			snprintf(execute, sizeof(execute), "-Ed:%s", batch);
			node_path(client, "dosemu.log", log + 2, sizeof(log) - 2); log[0] = '-'; log[1] = 'o';
			putenv(home);
			execlp(emulator, emulator, "-Ivideo { none }", "-Ikeystroke \\r",
			    "-Iserial { virtual com 1 }", "-t", execute, log, (char *)NULL);
			_exit(127);
		}
		child->pid = pid; child->io = ptyfd; child->io_is_socket = false;
		return true;
	}
	listener = socket(AF_INET, SOCK_STREAM, 0);
	if (listener == INVALID_SOCKET || bind(listener, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
	    getsockname(listener, (struct sockaddr *)&addr, &alen) != 0 || listen(listener, 1) != 0) {
		snprintf(err, errsz, "cannot create loopback DOS serial listener"); return false;
	}
	snprintf(command, sizeof(command), "%s", door->command);
	snprintf(params, sizeof(params), "%s", door->parameters);
	expand_command(client, 0, command, sizeof(command), params, sizeof(params));
	replace_all(command, sizeof(command), client->config->root, "C:");
	replace_all(params, sizeof(params), client->config->root, "C:");
	for (char *p = command; *p != 0; p++) if (*p == '/') *p = '\\';
	for (char *p = params; *p != 0; p++) if (*p == '/') *p = '\\';
	if (!node_path(client, "deucegate-dosbox.conf", conf, sizeof(conf))) return false;
	dg_path_join(base_conf, sizeof(base_conf), client->config->root, "dosbox.conf");
	snprintf(text, sizeof(text),
	    "[sdl]\nfullscreen=false\n[serial]\nserial1=nullmodem server:127.0.0.1 port:%u transparent:1 telnet:0\n"
	    "[autoexec]\nmount c \"%s\"\nc:\nset PATH=%%PATH%%;C:\\dosutils\n"
	    "set DEUCEGATE_ENCODING=%s\n"
	    "set DEUCEGATE_LANGUAGE_TAG=%s\n"
	    "if exist C:\\dosutils\\fossil.com C:\\dosutils\\fossil.com\n"
	    "if exist C:\\dosutils\\share.com C:\\dosutils\\share.com\n"
	    "if exist C:\\dosutils\\ansi.com C:\\dosutils\\ansi.com\n%s %s\nexit\n",
	    ntohs(addr.sin_port), client->config->root, door_encoding_name(effective_door_encoding(client, door)),
	    client->language_tag, command, params);
	if (!write_text(conf, text)) { closesocket(listener); return false; }
	pid = fork();
	if (pid == 0) {
		setsid(); chdir(client->config->root);
		if (dg_file_exists(base_conf))
			execlp(emulator, emulator, "-conf", base_conf, "-conf", conf, "-noconsole", (char *)NULL);
		else
			execlp(emulator, emulator, "-conf", conf, "-noconsole", (char *)NULL);
		_exit(127);
	}
	if (pid < 0) { closesocket(listener); return false; }
	{
		fd_set reads; struct timeval tv = {15, 0};
		FD_ZERO(&reads); FD_SET(listener, &reads);
		if (select(listener + 1, &reads, NULL, NULL, &tv) <= 0) {
			kill(-pid, SIGTERM); closesocket(listener);
			snprintf(err, errsz, "%s did not connect to its loopback serial port", emulator); return false;
		}
		child->io = accept(listener, NULL, NULL);
	}
	closesocket(listener); child->pid = pid; child->io_is_socket = true;
	if (child->io == INVALID_SOCKET) { kill(-pid, SIGTERM); return false; }
	return true;
}
#endif

#ifdef _WIN32
static bool
spawn_dos_windows(const dg_client_t *client, const dg_door_t *door, dg_child_t *child,
    char *err, size_t errsz)
{
	char kind[32], conf[DG_PATH_MAX], base_conf[DG_PATH_MAX], command[DG_PATH_MAX], params[DG_PATH_MAX * 2];
	char text[8192], command_line[DG_PATH_MAX * 2];
	const char *emulator = dos_backend(client, door, kind, sizeof(kind));
	SOCKET listener;
	struct sockaddr_in addr = {.sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
	int alen = sizeof(addr);
	STARTUPINFOA si = {0};
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};
	if (emulator == NULL) {
		snprintf(err, errsz, "requested DOSBox/DOSBox-X executable was not found"); return false;
	}
	listener = socket(AF_INET, SOCK_STREAM, 0);
	if (listener == INVALID_SOCKET || bind(listener, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
	    getsockname(listener, (struct sockaddr *)&addr, &alen) != 0 || listen(listener, 1) != 0) {
		snprintf(err, errsz, "cannot create loopback DOS serial listener"); return false;
	}
	snprintf(command, sizeof(command), "%s", door->command);
	snprintf(params, sizeof(params), "%s", door->parameters);
	expand_command(client, 0, command, sizeof(command), params, sizeof(params));
	replace_all(command, sizeof(command), client->config->root, "C:");
	replace_all(params, sizeof(params), client->config->root, "C:");
	for (char *p = command; *p != 0; p++) if (*p == '/') *p = '\\';
	for (char *p = params; *p != 0; p++) if (*p == '/') *p = '\\';
	if (!node_path(client, "deucegate-dosbox.conf", conf, sizeof(conf))) return false;
	dg_path_join(base_conf, sizeof(base_conf), client->config->root, "dosbox.conf");
	snprintf(text, sizeof(text),
	    "[sdl]\r\nfullscreen=false\r\n[serial]\r\nserial1=nullmodem server:127.0.0.1 port:%u transparent:1 telnet:0\r\n"
	    "[autoexec]\r\nmount c \"%s\"\r\nc:\r\nset PATH=%%PATH%%;C:\\dosutils\r\n"
	    "set DEUCEGATE_ENCODING=%s\r\n"
	    "set DEUCEGATE_LANGUAGE_TAG=%s\r\n"
	    "if exist C:\\dosutils\\fossil.com C:\\dosutils\\fossil.com\r\n"
	    "if exist C:\\dosutils\\share.com C:\\dosutils\\share.com\r\n"
	    "if exist C:\\dosutils\\ansi.com C:\\dosutils\\ansi.com\r\n%s %s\r\nexit\r\n",
	    ntohs(addr.sin_port), client->config->root, door_encoding_name(effective_door_encoding(client, door)),
	    client->language_tag, command, params);
	if (!write_text(conf, text)) { closesocket(listener); return false; }
	if (dg_file_exists(base_conf))
		snprintf(command_line, sizeof(command_line), "\"%s\" -conf \"%s\" -conf \"%s\" -noconsole",
		    emulator, base_conf, conf);
	else
		snprintf(command_line, sizeof(command_line), "\"%s\" -conf \"%s\" -noconsole", emulator, conf);
	si.cb = sizeof(si);
	memset(&child->process, 0, sizeof(child->process));
	if (!CreateProcessA(NULL, command_line, NULL, NULL, FALSE,
	    CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED, NULL, client->config->root, &si, &child->process)) {
		closesocket(listener); snprintf(err, errsz, "cannot start %s: %lu", emulator, GetLastError()); return false;
	}
	child->job = CreateJobObjectA(NULL, NULL);
	if (child->job != NULL) {
		limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		SetInformationJobObject(child->job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
		AssignProcessToJobObject(child->job, child->process.hProcess);
	}
	ResumeThread(child->process.hThread);
	{
		fd_set reads; struct timeval tv = {15, 0};
		FD_ZERO(&reads); FD_SET(listener, &reads);
		if (select(0, &reads, NULL, NULL, &tv) <= 0) {
			TerminateProcess(child->process.hProcess, 1); closesocket(listener);
			snprintf(err, errsz, "%s did not connect to its loopback serial port", emulator); return false;
		}
		child->io = accept(listener, NULL, NULL);
	}
	closesocket(listener);
	child->io_is_socket = true;
	return child->io != INVALID_SOCKET;
}
#endif

static void
stop_child(dg_child_t *child, unsigned delay, bool graceful)
{
#ifdef _WIN32
	if (child->io_is_socket) closesocket(child->io);
	else {
		if (child->pipe_in != NULL) CloseHandle(child->pipe_in);
		if (child->pipe_out != NULL) CloseHandle(child->pipe_out);
	}
	if (graceful && WaitForSingleObject(child->process.hProcess, delay * 1000) == WAIT_TIMEOUT)
		TerminateProcess(child->process.hProcess, 1);
	WaitForSingleObject(child->process.hProcess, 5000);
	CloseHandle(child->process.hThread); CloseHandle(child->process.hProcess);
	if (child->job != NULL) CloseHandle(child->job);
#else
	closesocket(child->io);
	if (child->pid <= 0) return;
	if (graceful) {
		for (unsigned i = 0; i < delay * 10; i++) {
			int status;
			if (waitpid(child->pid, &status, WNOHANG) == child->pid) return;
			SLEEP(100);
		}
	}
	kill(-child->pid, SIGTERM); SLEEP(250); kill(-child->pid, SIGKILL);
	waitpid(child->pid, NULL, 0);
#endif
}

bool
dg_run_door(dg_client_t *client, const char *door_name, char *err, size_t errsz)
{
	dg_door_t door;
	dg_child_t child;
	SOCKET pair[2] = {INVALID_SOCKET, INVALID_SOCKET};
	char node[DG_PATH_MAX];
	bool spawned = false;
	memset(&child, 0, sizeof(child)); child.io = INVALID_SOCKET;
	if (!load_door(client->config, door_name, &door, err, errsz)) return false;
	if (dg_stricmp(door.platform, "DOS") != 0) {
#ifdef _WIN32
		if (dg_stricmp(door.platform, "Windows") != 0) {
			snprintf(err, errsz, "door platform %s is not available on Windows", door.platform); return false;
		}
#else
		if (dg_stricmp(door.platform, "Linux") != 0) {
			snprintf(err, errsz, "door platform %s is not available on this POSIX host", door.platform); return false;
		}
#endif
	}
	/* Create once with a placeholder; native socket mode recreates it with the inherited handle. */
	if (!dg_create_drop_files(client, 0, node, sizeof(node))) { snprintf(err, errsz, "cannot create node drop files"); return false; }
	if (dg_stricmp(door.platform, "DOS") == 0) {
#ifdef _WIN32
		spawned = spawn_dos_windows(client, &door, &child, err, errsz);
#else
		spawned = spawn_dos_posix(client, &door, &child, err, errsz);
#endif
	}
	else {
#ifdef _WIN32
		spawned = spawn_native_windows(client, &door, pair, &child, err, errsz);
#else
		spawned = spawn_native_posix(client, &door, pair, &child, err, errsz);
#endif
	}
	if (!spawned) { cleanup_node(client); return false; }
	dg_log(DG_LOG_INFO, "node %u running door %s (%s, %s%s)", client->node, door.name,
	    door.platform, door_encoding_name(effective_door_encoding(client, &door)),
	    door.auto_encoding ? ", pass-through" : "");
	relay_io(client, &door, &child);
	stop_child(&child, door.force_quit_delay, true);
	cleanup_node(client);
	return true;
}
