#include "deucegate.h"

#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh-algorithms.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "deucessh.h"
#include "dirwrap.h"
#include "genwrap.h"
#include "ini_file.h"
#include "str_list.h"
#include "threadwrap.h"

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

typedef struct {
	SOCKET sock;
} dg_ssh_io_t;

typedef struct {
	dg_config_t *cfg;
	SOCKET sock;
	char remote_ip[64];
} dg_connection_t;

typedef struct {
	dg_client_t *client;
	bool runbbs;
} dg_auth_t;

typedef struct {
	dg_config_t *cfg;
	dg_client_t **nodes;
	size_t node_count;
	pthread_mutex_t mutex;
	SOCKET listener;
} dg_server_state_t;

static dg_server_state_t server_state = {.listener = INVALID_SOCKET};
static atomic_bool stopping = false;
static atomic_bool offline = false;
static atomic_bool scheduler_running = false;
static atomic_uint active_threads = 0;
static volatile sig_atomic_t signal_received = 0;

static bool
list_contains(const dg_config_t *cfg, const char *relative, const char *value, bool ip_pattern)
{
	char path[DG_PATH_MAX], line[512];
	FILE *fp;
	if (!dg_path_join(path, sizeof(path), cfg->root, relative) || (fp = fopen(path, "r")) == NULL)
		return false;
	while (fgets(line, sizeof(line), fp) != NULL) {
		char *item = dg_trim(line);
		if (*item == 0 || *item == ';') continue;
		if ((ip_pattern && wildmatchi(value, item, false)) || (!ip_pattern && dg_stricmp(value, item) == 0)) {
			fclose(fp); return true;
		}
	}
	fclose(fp);
	return false;
}

static int
ssh_tx(uint8_t *buf, size_t bufsz, dssh_session session, void *cbdata)
{
	dg_ssh_io_t *io = cbdata;
	size_t sent = 0;
	while (sent < bufsz && !dssh_session_is_terminated(session)) {
		int n = send(io->sock, (socket_send_buffer_t)(buf + sent), (int)(bufsz - sent), 0);
		if (n > 0) sent += (size_t)n;
		else if (n == SOCKET_ERROR && (SOCKET_ERRNO == EINTR || SOCKET_ERRNO == EWOULDBLOCK)) SLEEP(1);
		else return DSSH_ERROR_INIT;
	}
	return sent == bufsz ? 0 : DSSH_ERROR_TERMINATED;
}

static int
ssh_rx(uint8_t *buf, size_t bufsz, dssh_session session, void *cbdata)
{
	dg_ssh_io_t *io = cbdata;
	size_t got = 0;
	while (got < bufsz && !dssh_session_is_terminated(session)) {
		fd_set reads;
		struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
		int ready, n;
		FD_ZERO(&reads); FD_SET(io->sock, &reads);
		ready = select((int)io->sock + 1, &reads, NULL, NULL, &tv);
		if (ready == 0) continue;
		if (ready < 0) {
			if (SOCKET_ERRNO == EINTR) continue;
			return DSSH_ERROR_INIT;
		}
		n = recv(io->sock, (socket_recv_buffer_t)(buf + got), (int)(bufsz - got), 0);
		if (n > 0) got += (size_t)n;
		else if (n == 0) return DSSH_ERROR_TERMINATED;
		else if (SOCKET_ERRNO != EINTR && SOCKET_ERRNO != EWOULDBLOCK) return DSSH_ERROR_INIT;
	}
	return got == bufsz ? 0 : DSSH_ERROR_TERMINATED;
}

static int
ssh_rxline(uint8_t *buf, size_t bufsz, size_t *received, dssh_session session, void *cbdata)
{
	bool cr = false;
	for (size_t pos = 0; pos < bufsz; pos++) {
		int result = ssh_rx(buf + pos, 1, session, cbdata);
		if (result < 0) return result;
		if (buf[pos] == '\r') cr = true;
		else if (buf[pos] == '\n' && cr) { *received = pos + 1; return 0; }
		else cr = false;
	}
	return DSSH_ERROR_TOOLONG;
}

static void
ssh_terminate(dssh_session session, void *cbdata)
{
	dg_ssh_io_t *io = cbdata;
	(void)session;
	if (io != NULL && io->sock != INVALID_SOCKET)
		shutdown(io->sock, SHUT_RDWR);
}

static void
copy_username(char *out, size_t outsz, const uint8_t *username, size_t username_len)
{
	if (username_len >= outsz) username_len = outsz - 1;
	memcpy(out, username, username_len);
	out[username_len] = 0;
}

static int
auth_publickey(const uint8_t *username, size_t username_len, const char *algorithm,
    const uint8_t *blob, size_t blob_len, bool has_signature, void *cbdata)
{
	dg_auth_t *auth = cbdata;
	char alias[DG_ALIAS_MAX], err[256];
	if (strcmp(algorithm, "ssh-ed25519") != 0 && strcmp(algorithm, "rsa-sha2-256") != 0 &&
	    strcmp(algorithm, "rsa-sha2-512") != 0 && strcmp(algorithm, "ssh-rsa") != 0)
		return DSSH_AUTH_FAILURE;
	copy_username(alias, sizeof(alias), username, username_len);
	if (list_contains(auth->client->config, "config/banned-users.txt", alias, false))
		return DSSH_AUTH_FAILURE;
	if (!dg_user_bind(auth->client->config, alias, algorithm, blob, blob_len, has_signature,
	    &auth->client->user, err, sizeof(err))) {
		dg_log(DG_LOG_WARN, "SSH key rejected for %s from %s: %s", alias, auth->client->remote_ip, err);
		return DSSH_AUTH_FAILURE;
	}
	if (has_signature)
		dg_log(DG_LOG_INFO, "SSH key accepted for %s from %s", alias, auth->client->remote_ip);
	return DSSH_AUTH_SUCCESS;
}

static int
auth_password(const uint8_t *username, size_t username_len, const uint8_t *password,
    size_t password_len, uint8_t **prompt, size_t *prompt_len, void *cbdata)
{
	dg_auth_t *auth = cbdata;
	char alias[DG_ALIAS_MAX];
	(void)prompt; (void)prompt_len;
	copy_username(alias, sizeof(alias), username, username_len);
	if (list_contains(auth->client->config, "config/banned-users.txt", alias, false))
		return DSSH_AUTH_FAILURE;
	if (!dg_user_validate_password(auth->client->config, alias, password, password_len, &auth->client->user)) {
		dg_log(DG_LOG_WARN, "legacy password rejected for %s from %s", alias, auth->client->remote_ip);
		return DSSH_AUTH_FAILURE;
	}
	dg_log(DG_LOG_INFO, "legacy password accepted for %s from %s", alias, auth->client->remote_ip);
	return DSSH_AUTH_SUCCESS;
}

static int
auth_none(const uint8_t *username, size_t username_len, void *cbdata)
{
	dg_auth_t *auth = cbdata;
	(void)username; (void)username_len;
	if (!auth->runbbs) return DSSH_AUTH_FAILURE;
	memset(&auth->client->user, 0, sizeof(auth->client->user));
	strcpy(auth->client->user.alias, "RUNBBS");
	auth->client->user.access_level = 10;
	auth->client->anonymous = true;
	return DSSH_AUTH_SUCCESS;
}

static int
accept_pty(dssh_channel channel, const struct dssh_chan_params *params, void *cbdata)
{
	dg_client_t *client = cbdata;
	(void)channel;
	client->cols = params->cols ? params->cols : 80;
	client->rows = params->rows ? params->rows : 24;
	return 0;
}

static int
accept_env(dssh_channel channel, const struct dssh_chan_params *params, void *cbdata)
{
	dg_client_t *client = cbdata;
	const struct dssh_chan_env *env;
	char tag[DG_LANGUAGE_TAG_MAX + 1];
	unsigned priority;
	bool converted = false;

	(void)channel;
	if (params == NULL || params->env_count == 0)
		return -1;
	env = &params->env[params->env_count - 1];
	if (strcmp(env->name, "DEUCEGATE_LANGUAGE_TAG") == 0) {
		if (!dg_language_tag_valid(env->value))
			return -1;
		snprintf(tag, sizeof(tag), "%s", env->value);
		priority = 3;
	}
	else if (strcmp(env->name, "LC_ALL") == 0)
		priority = 2;
	else if (strcmp(env->name, "LC_MESSAGES") == 0)
		priority = 1;
	else if (strcmp(env->name, "LANG") == 0)
		priority = 0;
	else
		return -1;
	if (strcmp(env->name, "DEUCEGATE_LANGUAGE_TAG") != 0) {
		converted = dg_language_from_locale(env->value, tag, sizeof(tag));
		if (!converted)
			return 0;
	}
	if (priority >= client->language_priority) {
		snprintf(client->language_tag, sizeof(client->language_tag), "%s", tag);
		client->language_priority = priority;
	}
	return 0;
}

static int
accept_shell(dssh_channel channel, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)channel; (void)params; (void)result; (void)cbdata;
	return 0;
}

static int
reject_channel(dssh_channel channel, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)channel; (void)params; (void)result; (void)cbdata;
	return -1;
}

static void
write_who_locked(void)
{
	char path[DG_PATH_MAX];
	char *data;
	size_t cap = server_state.node_count * 512 + 1, used = 0;
	data = calloc(1, cap);
	if (data == NULL || !dg_path_join(path, sizeof(path), server_state.cfg->root, "whoisonline.txt")) {
		free(data); return;
	}
	for (size_t i = 0; i < server_state.node_count; i++) {
		dg_client_t *c = server_state.nodes[i];
		if (c == NULL) continue;
		used += (size_t)snprintf(data + used, cap - used, "%u\t%s\t%s\tOnline\t%lld\r\n",
		    c->node, c->user.alias, c->remote_ip, (long long)c->started);
	}
	dg_write_atomic(path, data, used, 0644);
	free(data);
}

static bool
reserve_node(dg_client_t *client)
{
	bool found = false;
	if (atomic_load(&offline)) return false;
	pthread_mutex_lock(&server_state.mutex);
	if (!client->user.allow_multiple) {
		for (size_t i = 0; i < server_state.node_count; i++) {
			dg_client_t *old = server_state.nodes[i];
			if (old != NULL && dg_stricmp(old->user.alias, client->user.alias) == 0)
				dssh_session_terminate((dssh_session)old->session);
		}
	}
	for (size_t i = 0; i < server_state.node_count; i++) {
		if (server_state.nodes[i] == NULL) {
			client->node = server_state.cfg->first_node + (unsigned)i;
			server_state.nodes[i] = client;
			found = true;
			break;
		}
	}
	write_who_locked();
	pthread_mutex_unlock(&server_state.mutex);
	return found;
}

static void
release_node(dg_client_t *client)
{
	pthread_mutex_lock(&server_state.mutex);
	if (client->node >= server_state.cfg->first_node && client->node <= server_state.cfg->last_node) {
		size_t at = client->node - server_state.cfg->first_node;
		if (server_state.nodes[at] == client) server_state.nodes[at] = NULL;
	}
	write_who_locked();
	pthread_mutex_unlock(&server_state.mutex);
}

static void
connection_thread(void *arg)
{
	dg_connection_t *connection = arg;
	dg_client_t *client = calloc(1, sizeof(*client));
	dg_ssh_io_t io = {.sock = connection->sock};
	dg_auth_t auth;
	dssh_session session = NULL;
	dssh_channel channel = NULL;
	char runbbs_path[DG_PATH_MAX], err[512];
	uint8_t auth_user[256];
	size_t auth_user_len = sizeof(auth_user);
	bool node_reserved = false;
	if (client == NULL) goto done;
	client->config = connection->cfg;
	client->sock = connection->sock;
	client->cols = 80; client->rows = 24;
	snprintf(client->remote_ip, sizeof(client->remote_ip), "%s", connection->remote_ip);
	client->client_encoding = DG_CP437;
	strcpy(client->language_tag, "en");
	session = dssh_session_init(false, 0);
	if (session == NULL) goto done;
	client->session = session;
	dssh_session_set_cbdata(session, &io, &io, &io, NULL);
	dssh_session_set_terminate_cb(session, ssh_terminate, &io);
	if (dssh_transport_handshake(session) < 0) goto done;
	auth.client = client;
	auth.runbbs = dg_path_join(runbbs_path, sizeof(runbbs_path), client->config->root,
	    "doors/_runbbs.ini") && dg_file_exists(runbbs_path);
	{
		struct dssh_auth_server_cbs callbacks = {
		    .methods_str = "publickey,password",
		    .none_cb = auth_none,
		    .password_cb = auth_password,
		    .publickey_cb = auth_publickey,
		    .cbdata = &auth,
		};
		if (dssh_auth_server(session, &callbacks, auth_user, &auth_user_len) < 0) goto done;
	}
	if (dssh_session_start(session) < 0) goto done;
	{
		struct dssh_chan_accept_cbs callbacks = {
		    .pty_req = accept_pty, .env = accept_env, .shell = accept_shell,
		    .exec = reject_channel, .subsystem = reject_channel, .cbdata = client,
		};
		channel = dssh_chan_accept(session, &callbacks, 75000);
	}
	if (channel == NULL || dssh_chan_get_type(channel) != DSSH_CHAN_SHELL) goto done;
	client->channel = channel;
	client->started = time(NULL);
	if (!reserve_node(client)) {
		dg_client_puts(client, "\r\nAll DeuceGate nodes are currently busy.\r\n");
		goto done;
	}
	node_reserved = true;
	if (!dg_detect_terminal(client)) goto done;
	if (client->anonymous) {
		if (!dg_run_door(client, "_runbbs", err, sizeof(err)))
			dg_client_puts(client, "\r\nRUNBBS is unavailable.\r\n");
	}
	else
		dg_run_session(client);
done:
	if (node_reserved) release_node(client);
	if (channel != NULL) dssh_chan_close(channel, 0);
	if (session != NULL) dssh_session_cleanup(session);
	if (connection->sock != INVALID_SOCKET) closesocket(connection->sock);
	free(client);
	free(connection);
	atomic_fetch_sub(&active_threads, 1);
}

static bool
event_day_matches(const char *days, const struct tm *tmv)
{
	static const char *names[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
	char copy[256], *item, *save = NULL;
	if (strlen(days) >= sizeof(copy)) return false;
	strcpy(copy, days);
	for (item = strtok_r(copy, ",", &save); item != NULL; item = strtok_r(NULL, ",", &save))
		if (dg_stricmp(dg_trim(item), names[tmv->tm_wday]) == 0) return true;
	return false;
}

static void
run_event_command(const dg_config_t *cfg, const char *command)
{
	char path[DG_PATH_MAX], relative[DG_PATH_MAX], shell[DG_PATH_MAX * 2];
	strncpy(relative, command, sizeof(relative) - 1);
	relative[sizeof(relative) - 1] = 0;
#ifndef _WIN32
	for (char *p = relative; *p != 0; p++)
		if (*p == '\\') *p = '/';
#endif
	if (command[0] == '/' || command[0] == '\\' || (strlen(command) > 2 && command[1] == ':'))
		strncpy(path, relative, sizeof(path) - 1);
	else
		dg_path_join(path, sizeof(path), cfg->root, relative);
	path[sizeof(path) - 1] = 0;
#ifdef _WIN32
	snprintf(shell, sizeof(shell), "cmd.exe /c \"\"%s\"\"", path);
#else
	snprintf(shell, sizeof(shell), "cd \"%s\" && \"%s\"", cfg->root, path);
#endif
	system(shell);
}

static void
scheduler_thread(void *arg)
{
	dg_config_t *cfg = arg;
	time_t last_minute = 0;
	while (!atomic_load(&stopping)) {
		time_t now = time(NULL), minute = now / 60;
		if (minute != last_minute) {
			char path[DG_PATH_MAX];
			FILE *fp;
			last_minute = minute;
			if (dg_path_join(path, sizeof(path), cfg->root, "config/timed-events.ini") &&
			    (fp = iniOpenFile(path, false)) != NULL) {
				str_list_t ini = iniReadFile(fp), sections;
				struct tm tmv;
				iniCloseFile(fp);
#ifdef _WIN32
				localtime_s(&tmv, &now);
#else
				localtime_r(&now, &tmv);
#endif
				sections = ini != NULL ? iniGetSectionList(ini, NULL) : NULL;
				for (size_t i = 0; sections != NULL && sections[i] != NULL; i++) {
					char name[256], command[DG_PATH_MAX], days[256], when[32], current[16];
					bool go_offline;
					iniGetSString(ini, sections[i], "Name", sections[i], name, sizeof(name));
					iniGetSString(ini, sections[i], "Command", "", command, sizeof(command));
					iniGetSString(ini, sections[i], "Days", "", days, sizeof(days));
					iniGetSString(ini, sections[i], "Time", "", when, sizeof(when));
					strftime(current, sizeof(current), "%H:%M", &tmv);
					if (!*command || strcmp(current, when) != 0 || !event_day_matches(days, &tmv)) continue;
					go_offline = iniGetBool(ini, sections[i], "GoOffline", false);
					dg_log(DG_LOG_INFO, "running timed event %s%s", name, go_offline ? " (offline)" : "");
					if (go_offline) {
						atomic_store(&offline, true);
						pthread_mutex_lock(&server_state.mutex);
						for (size_t n = 0; n < server_state.node_count; n++)
							if (server_state.nodes[n] != NULL)
								dssh_session_terminate((dssh_session)server_state.nodes[n]->session);
						pthread_mutex_unlock(&server_state.mutex);
						for (unsigned wait = 0; wait < 300 && !atomic_load(&stopping); wait++) {
							bool any = false;
							pthread_mutex_lock(&server_state.mutex);
							for (size_t n = 0; n < server_state.node_count; n++) if (server_state.nodes[n] != NULL) any = true;
							pthread_mutex_unlock(&server_state.mutex);
							if (!any) break;
							SLEEP(100);
						}
					}
					run_event_command(cfg, command);
					if (go_offline) atomic_store(&offline, false);
				}
				if (sections != NULL) strListFree(&sections);
				if (ini != NULL) strListFree(&ini);
			}
		}
		SLEEP(500);
	}
	atomic_store(&scheduler_running, false);
}

static bool
init_ssh(const dg_config_t *cfg)
{
	if (dssh_transport_set_callbacks(ssh_tx, ssh_rx, ssh_rxline, NULL) != 0 ||
	    dssh_transport_set_version("DeuceGate_0.1", NULL) != 0 ||
	    dssh_register_mlkem768x25519_sha256() != 0 ||
	    dssh_register_sntrup761x25519_sha512() != 0 ||
	    dssh_register_curve25519_sha256() != 0 ||
	    dssh_register_ssh_ed25519() != 0 ||
	    dssh_register_rsa_sha2_256() != 0 || dssh_register_rsa_sha2_512() != 0 ||
	    dssh_register_aes256_ctr() != 0 || dssh_register_hmac_sha2_256() != 0 ||
	    dssh_register_hmac_sha2_512() != 0 || dssh_register_none_comp() != 0) {
		dg_log(DG_LOG_ERROR, "failed to initialize DeuceSSH algorithms");
		return false;
	}
	if (dg_file_exists(cfg->ssh_host_key)) {
#ifndef _WIN32
		struct stat st;
		if (stat(cfg->ssh_host_key, &st) != 0 || !S_ISREG(st.st_mode) || (st.st_mode & 077) != 0) {
			dg_log(DG_LOG_ERROR, "SSH host key %s must be a regular file accessible only by its owner",
			    cfg->ssh_host_key);
			return false;
		}
#endif
		if (dssh_ed25519_load_key_file(cfg->ssh_host_key, NULL, NULL) < 0) {
			dg_log(DG_LOG_ERROR, "cannot load SSH host key %s", cfg->ssh_host_key);
			return false;
		}
	}
	else {
		bool saved;
#ifndef _WIN32
		mode_t old_mask = umask(077);
#endif
		saved = dg_mkdir_parent(cfg->ssh_host_key) && dssh_ed25519_generate_key() >= 0 &&
		    dssh_ed25519_save_key_file(cfg->ssh_host_key, NULL, NULL) >= 0;
#ifndef _WIN32
		umask(old_mask);
#endif
		if (!saved) {
			dg_log(DG_LOG_ERROR, "cannot generate SSH host key %s", cfg->ssh_host_key);
			return false;
		}
#ifndef _WIN32
		chmod(cfg->ssh_host_key, 0600);
#endif
		dg_log(DG_LOG_INFO, "generated SSH host key %s", cfg->ssh_host_key);
	}
	return true;
}

void
dg_server_request_stop(void)
{
	atomic_store(&stopping, true);
	if (server_state.listener != INVALID_SOCKET) {
		shutdown(server_state.listener, SHUT_RDWR);
		closesocket(server_state.listener);
		server_state.listener = INVALID_SOCKET;
	}
	if (server_state.nodes != NULL) {
		pthread_mutex_lock(&server_state.mutex);
		for (size_t i = 0; i < server_state.node_count; i++)
			if (server_state.nodes[i] != NULL)
				dssh_session_terminate((dssh_session)server_state.nodes[i]->session);
		pthread_mutex_unlock(&server_state.mutex);
	}
}

static void
signal_stop(int sig)
{
	(void)sig;
	/* Keep the POSIX handler async-signal-safe.  The accept loop polls once a second. */
	signal_received = 1;
}

#ifdef _WIN32
static BOOL WINAPI
console_stop(DWORD event)
{
	if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT && event != CTRL_CLOSE_EVENT &&
	    event != CTRL_LOGOFF_EVENT && event != CTRL_SHUTDOWN_EVENT)
		return FALSE;
	atomic_store(&stopping, true);
	return TRUE;
}
#endif

int
dg_server_run(dg_config_t *cfg)
{
	struct addrinfo hints = {0}, *addresses = NULL, *address;
	char port[16];
	SOCKET listener = INVALID_SOCKET;
	int one = 1;
#ifdef _WIN32
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;
#endif
	if (!init_ssh(cfg)) return 1;
	server_state.cfg = cfg;
	server_state.node_count = cfg->last_node - cfg->first_node + 1;
	server_state.nodes = calloc(server_state.node_count, sizeof(*server_state.nodes));
	server_state.mutex = pthread_mutex_initializer_np(false);
	if (server_state.nodes == NULL) return 1;
	snprintf(port, sizeof(port), "%u", cfg->ssh_port);
	hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM; hints.ai_flags = AI_PASSIVE;
	if (getaddrinfo(cfg->ssh_ip, port, &hints, &addresses) != 0) return 1;
	for (address = addresses; address != NULL; address = address->ai_next) {
		listener = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
		if (listener == INVALID_SOCKET) continue;
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (socket_send_buffer_t)&one, sizeof(one));
		if (bind(listener, address->ai_addr, (socklen_t)address->ai_addrlen) == 0 && listen(listener, 32) == 0)
			break;
		closesocket(listener); listener = INVALID_SOCKET;
	}
	freeaddrinfo(addresses);
	if (listener == INVALID_SOCKET) {
		dg_log(DG_LOG_ERROR, "cannot listen on %s:%u", cfg->ssh_ip, cfg->ssh_port);
		return 1;
	}
	server_state.listener = listener;
	signal(SIGINT, signal_stop); signal(SIGTERM, signal_stop);
#ifdef _WIN32
	SetConsoleCtrlHandler(console_stop, TRUE);
#endif
	atomic_store(&scheduler_running, true);
	if (_beginthread(scheduler_thread, 0, cfg) == (ulong)-1L) {
		atomic_store(&scheduler_running, false);
		dg_log(DG_LOG_WARN, "could not start timed-event scheduler");
	}
	dg_log(DG_LOG_INFO, "DeuceGate listening on %s:%u", cfg->ssh_ip, cfg->ssh_port);
	while (!atomic_load(&stopping) && !signal_received) {
		struct sockaddr_storage peer;
		socklen_t peerlen = sizeof(peer);
		SOCKET sock;
		dg_connection_t *connection;
		fd_set reads;
		struct timeval tv = {1, 0};
		if (atomic_load(&offline)) { SLEEP(100); continue; }
		FD_ZERO(&reads); FD_SET(listener, &reads);
		if (select((int)listener + 1, &reads, NULL, NULL, &tv) <= 0) continue;
		sock = accept(listener, (struct sockaddr *)&peer, &peerlen);
		if (sock == INVALID_SOCKET) {
			if (atomic_load(&stopping)) break;
			if (SOCKET_ERRNO == EINTR) continue;
			dg_log(DG_LOG_WARN, "accept failed: %d", SOCKET_ERRNO);
			continue;
		}
		connection = calloc(1, sizeof(*connection));
		if (connection == NULL) { closesocket(sock); continue; }
		connection->cfg = cfg; connection->sock = sock;
		if (getnameinfo((struct sockaddr *)&peer, peerlen, connection->remote_ip,
		    sizeof(connection->remote_ip), NULL, 0, NI_NUMERICHOST) != 0)
			strcpy(connection->remote_ip, "unknown");
		if (list_contains(cfg, "config/ignored-ips.txt", connection->remote_ip, true) ||
		    list_contains(cfg, "config/ignored-ips-combined.txt", connection->remote_ip, true)) {
			closesocket(sock); free(connection); continue;
		}
		if (list_contains(cfg, "config/banned-ips.txt", connection->remote_ip, true)) {
			dg_log(DG_LOG_WARN, "rejected banned IP %s", connection->remote_ip);
			closesocket(sock); free(connection); continue;
		}
		atomic_fetch_add(&active_threads, 1);
		if (_beginthread(connection_thread, 0, connection) == (ulong)-1L) {
			atomic_fetch_sub(&active_threads, 1); closesocket(sock); free(connection);
		}
	}
	dg_server_request_stop();
	while (atomic_load(&active_threads) != 0 || atomic_load(&scheduler_running)) SLEEP(100);
	free(server_state.nodes); server_state.nodes = NULL;
	pthread_mutex_destroy(&server_state.mutex);
#ifdef _WIN32
	SetConsoleCtrlHandler(console_stop, FALSE);
	WSACleanup();
#endif
	return 0;
}
