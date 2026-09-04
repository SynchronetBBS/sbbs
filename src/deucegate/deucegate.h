#ifndef DEUCEGATE_H
#define DEUCEGATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "gen_defs.h"
#include "sockwrap.h"
#include "str_list.h"

#define DG_PATH_MAX 1024
#define DG_ALIAS_MAX 64
#define DG_KEY_MAX 8192
#define DG_MCI_MAX 16384
#define DG_IO_BUFSZ 8192
#define DG_LANGUAGE_TAG_MAX 64

typedef enum {
	DG_CP437,
	DG_UTF8
} dg_encoding_t;

typedef enum {
	DG_TERM_ASCII,
	DG_TERM_ANSI,
	DG_TERM_RIP
} dg_term_t;

typedef enum {
	DG_LOG_DEBUG,
	DG_LOG_INFO,
	DG_LOG_WARN,
	DG_LOG_ERROR
} dg_log_level_t;

typedef struct {
	char root[DG_PATH_MAX];
	char bbs_name[128];
	char sysop_first_name[64];
	char sysop_last_name[64];
	char sysop_name[128];
	char sysop_email[256];
	char ssh_ip[64];
	unsigned ssh_port;
	char ssh_host_key[DG_PATH_MAX];
	unsigned ssh_max_connections;
	unsigned ssh_max_connections_per_ip;
	unsigned ssh_input_byte_limit;
	unsigned ssh_output_byte_limit;
	unsigned ssh_idle_timeout_seconds;
	unsigned auth_tarpit_base_milliseconds;
	unsigned auth_tarpit_max_milliseconds;
	unsigned auth_tarpit_decay_seconds;
	char dosbox_path[DG_PATH_MAX];
	char dosbox_x_path[DG_PATH_MAX];
	char dosemu_path[DG_PATH_MAX];
	char password_pepper[256];
	unsigned first_node;
	unsigned last_node;
	unsigned time_per_call;
	unsigned next_user_id;
} dg_config_t;

typedef struct {
	unsigned id;
	unsigned access_level;
	char alias[DG_ALIAS_MAX];
	bool allow_multiple;
	char key_algorithm[64];
	char key_blob[DG_KEY_MAX];
	char password_hash[256];
	char password_salt[256];
	char ini_path[DG_PATH_MAX];
} dg_user_t;

typedef struct {
	dg_encoding_t encoding;
	uint8_t pending[4];
	size_t pending_len;
	uint64_t invalid_count;
} dg_decoder_t;

typedef struct {
	SOCKET sock;
	void *session;
	void *channel;
	dg_config_t *config;
	dg_user_t user;
	char remote_ip[64];
	unsigned node;
	unsigned cols;
	unsigned rows;
	dg_term_t terminal;
	dg_encoding_t client_encoding;
	char language_tag[DG_LANGUAGE_TAG_MAX + 1];
	unsigned language_priority;
	bool anonymous;
	time_t started;
} dg_client_t;

typedef struct {
	const dg_config_t *config;
	const dg_user_t *user;
	const char *menu_name;
	const char *filename;
	const char *remote_ip;
	unsigned node;
	unsigned seconds_left;
	time_t now;
} dg_mci_context_t;

/* util.c */
void dg_log(dg_log_level_t level, const char *fmt, ...);
bool dg_path_join(char *out, size_t outsz, const char *a, const char *b);
bool dg_file_exists(const char *path);
bool dg_dir_exists(const char *path);
bool dg_read_file(const char *path, uint8_t **data, size_t *len);
bool dg_write_atomic(const char *path, const void *data, size_t len, unsigned mode);
bool dg_mkdir_parent(const char *path);
bool dg_alias_valid(const char *alias);
bool dg_expand_command(const dg_client_t *client, SOCKET handle, bool shell, char *command,
    size_t commandsz, char *parameters, size_t paramsz, str_list_t *exports);
int dg_stricmp(const char *a, const char *b);
char *dg_trim(char *s);
bool dg_language_tag_valid(const char *tag);
bool dg_language_from_locale(const char *locale, char *tag, size_t tagsz);

/* config.c */
bool dg_config_load(const char *root, dg_config_t *cfg, char *err, size_t errsz);
bool dg_config_check(const dg_config_t *cfg, FILE *out);

/* charset.c */
void dg_decoder_init(dg_decoder_t *decoder, dg_encoding_t encoding);
size_t dg_decode(dg_decoder_t *decoder, const uint8_t *in, size_t inlen,
    uint8_t *out, size_t outsz, bool flush);
size_t dg_encode(dg_encoding_t encoding, const uint8_t *utf8, size_t len,
    uint8_t *out, size_t outsz);

/* user.c */
bool dg_user_find(const dg_config_t *cfg, const char *alias, dg_user_t *user);
bool dg_user_bind(const dg_config_t *cfg, const char *alias, const char *algorithm,
    const uint8_t *blob, size_t blob_len, bool signed_request, dg_user_t *user,
    char *err, size_t errsz);
bool dg_user_authorize(const dg_config_t *cfg, const char *alias,
    const char *public_key_path, bool replace, char *err, size_t errsz);
bool dg_user_validate_password(const dg_config_t *cfg, const char *alias,
    const uint8_t *password, size_t password_len, dg_user_t *user);
bool dg_password_matches(const dg_config_t *cfg, const dg_user_t *user,
    const uint8_t *password, size_t password_len);

/* terminal.c */
bool dg_detect_terminal(dg_client_t *client);
bool dg_client_write_raw(dg_client_t *client, const uint8_t *data, size_t len);
bool dg_client_write(dg_client_t *client, const uint8_t *utf8, size_t len);
bool dg_client_puts(dg_client_t *client, const char *utf8);
int dg_client_getch(dg_client_t *client, int timeout_ms);

/* mci.c */
size_t dg_mci_expand(const char *input, char *output, size_t outsz,
    const dg_mci_context_t *ctx);

/* menu.c */
int dg_run_session(dg_client_t *client);
bool dg_display_file(dg_client_t *client, const char *path, bool more, bool pause,
    const dg_mci_context_t *ctx);

/* door.c */
bool dg_create_drop_files(const dg_client_t *client, const char *communications,
    SOCKET handle, dg_encoding_t encoding, char *node_dir, size_t node_dir_sz);
bool dg_run_door(dg_client_t *client, const char *door_name, char *err, size_t errsz);

/* server.c */
int dg_server_run(dg_config_t *cfg);
void dg_server_request_stop(void);

#endif
