#include "deucegate.h"

#include <stdlib.h>
#include <string.h>

#include "ini_file.h"
#include "str_list.h"
#include "dirwrap.h"

static void
get_string(str_list_t ini, const char *key, const char *def, char *out, size_t outsz)
{
	iniGetSString(ini, "CONFIGURATION", key, def, out, outsz);
}

bool
dg_config_load(const char *root, dg_config_t *cfg, char *err, size_t errsz)
{
	char path[DG_PATH_MAX];
	char host_key[DG_PATH_MAX];
	FILE *fp;
	str_list_t ini;
	memset(cfg, 0, sizeof(*cfg));
	if (root == NULL || strlen(root) >= sizeof(cfg->root)) {
		snprintf(err, errsz, "invalid GameSrv root directory");
		return false;
	}
	strcpy(cfg->root, root);
	if (!dg_path_join(path, sizeof(path), root, "config/gamesrv.ini") ||
	    (fp = iniOpenFile(path, false)) == NULL) {
		snprintf(err, errsz, "cannot open %s/config/gamesrv.ini", root);
		return false;
	}
	ini = iniReadFile(fp);
	iniCloseFile(fp);
	if (ini == NULL) {
		snprintf(err, errsz, "cannot parse %s", path);
		return false;
	}
	get_string(ini, "BBSName", "GameSrv", cfg->bbs_name, sizeof(cfg->bbs_name));
	get_string(ini, "SysopFirstName", "", cfg->sysop_first_name, sizeof(cfg->sysop_first_name));
	get_string(ini, "SysopLastName", "", cfg->sysop_last_name, sizeof(cfg->sysop_last_name));
	snprintf(cfg->sysop_name, sizeof(cfg->sysop_name), "%s%s%s", cfg->sysop_first_name,
	    *cfg->sysop_first_name && *cfg->sysop_last_name ? " " : "", cfg->sysop_last_name);
	get_string(ini, "SysopEmail", "", cfg->sysop_email, sizeof(cfg->sysop_email));
	get_string(ini, "SSHServerIP", "0.0.0.0", cfg->ssh_ip, sizeof(cfg->ssh_ip));
	cfg->ssh_port = iniGetIntInRange(ini, "CONFIGURATION", "SSHServerPort", 1, 22, 65535);
	get_string(ini, "SSHHostKey", "config/ssh_host_ed25519.pem", host_key, sizeof(host_key));
	if (host_key[0] == '/' || host_key[0] == '\\' ||
	    (strlen(host_key) > 2 && host_key[1] == ':'))
		snprintf(cfg->ssh_host_key, sizeof(cfg->ssh_host_key), "%s", host_key);
	else
		dg_path_join(cfg->ssh_host_key, sizeof(cfg->ssh_host_key), root, host_key);
	get_string(ini, "DOSBoxPath", "", cfg->dosbox_path, sizeof(cfg->dosbox_path));
	get_string(ini, "DOSBoxXPath", "", cfg->dosbox_x_path, sizeof(cfg->dosbox_x_path));
	get_string(ini, "DOSEmuPath", "", cfg->dosemu_path, sizeof(cfg->dosemu_path));
	get_string(ini, "PasswordPepper", "", cfg->password_pepper, sizeof(cfg->password_pepper));
	cfg->first_node = iniGetIntInRange(ini, "CONFIGURATION", "FirstNode", 1, 1, 9999);
	cfg->last_node = iniGetIntInRange(ini, "CONFIGURATION", "LastNode", 1, 10, 9999);
	if (cfg->last_node < cfg->first_node) {
		strListFree(&ini);
		snprintf(err, errsz, "LastNode must be greater than or equal to FirstNode");
		return false;
	}
	cfg->ssh_max_connections = iniGetIntInRange(ini, "CONFIGURATION", "SSHMaxConnections", 1,
	    (int)(cfg->last_node - cfg->first_node + 2), 65535);
	cfg->ssh_max_connections_per_ip = iniGetIntInRange(ini, "CONFIGURATION",
	    "SSHMaxConnectionsPerIP", 1, cfg->ssh_max_connections < 4 ? cfg->ssh_max_connections : 4, 65535);
	cfg->ssh_input_byte_limit = iniGetIntInRange(ini, "CONFIGURATION",
	    "SSHInputByteLimit", 0, 16 * 1024 * 1024, 0x7fffffff);
	cfg->ssh_output_byte_limit = iniGetIntInRange(ini, "CONFIGURATION",
	    "SSHOutputByteLimit", 0, 64 * 1024 * 1024, 0x7fffffff);
	cfg->ssh_idle_timeout_seconds = iniGetIntInRange(ini, "CONFIGURATION",
	    "SSHIdleTimeoutSeconds", 0, 15 * 60, 24 * 60 * 60);
	cfg->auth_tarpit_base_milliseconds = iniGetIntInRange(ini, "CONFIGURATION",
	    "AuthTarpitBaseMilliseconds", 0, 250, 60000);
	cfg->auth_tarpit_max_milliseconds = iniGetIntInRange(ini, "CONFIGURATION",
	    "AuthTarpitMaxMilliseconds", 0, 8000, 60000);
	cfg->auth_tarpit_decay_seconds = iniGetIntInRange(ini, "CONFIGURATION",
	    "AuthTarpitDecaySeconds", 0, 15 * 60, 24 * 60 * 60);
	cfg->time_per_call = iniGetIntInRange(ini, "CONFIGURATION", "TimePerCall", 1, 60, 1440);
	cfg->next_user_id = iniGetIntInRange(ini, "CONFIGURATION", "NextUserId", 1, 1, 0x7fffffff);
	strListFree(&ini);
	if (cfg->auth_tarpit_base_milliseconds > cfg->auth_tarpit_max_milliseconds) {
		snprintf(err, errsz, "AuthTarpitMaxMilliseconds must be greater than or equal to AuthTarpitBaseMilliseconds");
		return false;
	}
	return true;
}

bool
dg_config_check(const dg_config_t *cfg, FILE *out)
{
	char path[DG_PATH_MAX];
	bool ok = true;
	fprintf(out, "GameSrv root: %s\n", cfg->root);
	fprintf(out, "SSH listener: %s:%u\n", cfg->ssh_ip, cfg->ssh_port);
	fprintf(out, "Nodes: %u-%u\n", cfg->first_node, cfg->last_node);
	fprintf(out, "SSH connections: %u total, %u per IP\n", cfg->ssh_max_connections,
	    cfg->ssh_max_connections_per_ip);
	fprintf(out, "SSH quotas: %u bytes input, %u bytes output\n",
	    cfg->ssh_input_byte_limit, cfg->ssh_output_byte_limit);
	fprintf(out, "SSH idle timeout: %u seconds\n", cfg->ssh_idle_timeout_seconds);
	fprintf(out, "Authentication tarpit: %u-%u ms, %u-second decay\n",
	    cfg->auth_tarpit_base_milliseconds, cfg->auth_tarpit_max_milliseconds,
	    cfg->auth_tarpit_decay_seconds);
	if (!dg_path_join(path, sizeof(path), cfg->root, "menus") || !dg_dir_exists(path)) {
		fprintf(out, "ERROR: menus directory is missing\n");
		ok = false;
	}
	if (!dg_path_join(path, sizeof(path), cfg->root, "doors") || !dg_dir_exists(path)) {
		fprintf(out, "ERROR: doors directory is missing\n");
		ok = false;
	}
	if (!dg_path_join(path, sizeof(path), cfg->root, "users"))
		ok = false;
	else if (!dg_dir_exists(path)) {
		if (mkpath(path) != 0) {
			fprintf(out, "ERROR: cannot create users directory\n");
			ok = false;
		}
	}
	fprintf(out, "%s\n", ok ? "Configuration is valid." : "Configuration has errors.");
	return ok;
}
