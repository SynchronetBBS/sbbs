#include "deucegate.h"

#include <ctype.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "base64.h"
#include "deucessh-crypto.h"
#include "deucessh.h"
#include "dirwrap.h"
#include "ini_file.h"
#include "str_list.h"

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

static atomic_flag user_lock = ATOMIC_FLAG_INIT;

static void
lock_users(void)
{
	while (atomic_flag_test_and_set_explicit(&user_lock, memory_order_acquire))
		YIELD();
}

static void
unlock_users(void)
{
	atomic_flag_clear_explicit(&user_lock, memory_order_release);
}

static bool
load_user_file(const char *path, dg_user_t *user)
{
	FILE *fp = iniOpenFile(path, false);
	str_list_t ini;
	if (fp == NULL)
		return false;
	ini = iniReadFile(fp);
	iniCloseFile(fp);
	if (ini == NULL)
		return false;
	memset(user, 0, sizeof(*user));
	iniGetSString(ini, "USER", "Alias", "", user->alias, sizeof(user->alias));
	user->id = iniGetUInteger(ini, "USER", "UserId", 0);
	user->access_level = iniGetUInteger(ini, "USER", "AccessLevel", 10);
	user->allow_multiple = iniGetBool(ini, "USER", "AllowMultipleConnections", false);
	iniGetSString(ini, "USER", "SSHKeyAlgorithm", "", user->key_algorithm, sizeof(user->key_algorithm));
	iniGetSString(ini, "USER", "SSHKey", "", user->key_blob, sizeof(user->key_blob));
	iniGetSString(ini, "USER", "PasswordHash", "", user->password_hash, sizeof(user->password_hash));
	iniGetSString(ini, "USER", "PasswordSalt", "", user->password_salt, sizeof(user->password_salt));
	snprintf(user->ini_path, sizeof(user->ini_path), "%s", path);
	strListFree(&ini);
	return *user->alias != 0;
}

bool
dg_user_find(const dg_config_t *cfg, const char *alias, dg_user_t *user)
{
	char dirpath[DG_PATH_MAX];
	DIR *dir;
	struct dirent *ent;
	if (!dg_path_join(dirpath, sizeof(dirpath), cfg->root, "users") || (dir = opendir(dirpath)) == NULL)
		return false;
	while ((ent = readdir(dir)) != NULL) {
		char path[DG_PATH_MAX];
		size_t n = strlen(ent->d_name);
		if (n < 5 || dg_stricmp(ent->d_name + n - 4, ".ini") != 0 ||
		    !dg_path_join(path, sizeof(path), dirpath, ent->d_name))
			continue;
		if (load_user_file(path, user) && dg_stricmp(user->alias, alias) == 0) {
			closedir(dir);
			return true;
		}
	}
	closedir(dir);
	return false;
}

static bool
save_ini(const char *path, str_list_t ini)
{
	char tmp[DG_PATH_MAX];
	FILE *fp;
	bool ok;
	if (!dg_mkdir_parent(path) || snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp))
		return false;
	fp = fopen(tmp, "w+b");
	if (fp == NULL)
		return false;
	ok = iniWriteFile(fp, ini) && fflush(fp) == 0;
#ifdef _WIN32
	if (ok) ok = _commit(_fileno(fp)) == 0;
#else
	if (ok) ok = fsync(fileno(fp)) == 0;
#endif
	if (!iniCloseFile(fp)) ok = false;
	if (ok) {
#ifdef _WIN32
		ok = MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
		ok = rename(tmp, path) == 0;
#endif
	}
	if (!ok) remove(tmp);
	return ok;
}

static bool
set_existing_key(dg_user_t *user, const char *algorithm, const char *b64, char *err, size_t errsz)
{
	FILE *fp = iniOpenFile(user->ini_path, false);
	str_list_t ini;
	bool ok;
	if (fp == NULL) {
		snprintf(err, errsz, "cannot open user file %s", user->ini_path);
		return false;
	}
	ini = iniReadFile(fp);
	iniCloseFile(fp);
	if (ini == NULL) {
		snprintf(err, errsz, "cannot parse user file %s", user->ini_path);
		return false;
	}
	iniSetString(&ini, "USER", "SSHKeyAlgorithm", algorithm, NULL);
	iniSetString(&ini, "USER", "SSHKey", b64, NULL);
	ok = save_ini(user->ini_path, ini);
	strListFree(&ini);
	if (!ok) {
		snprintf(err, errsz, "cannot atomically update %s", user->ini_path);
		return false;
	}
	snprintf(user->key_algorithm, sizeof(user->key_algorithm), "%s", algorithm);
	snprintf(user->key_blob, sizeof(user->key_blob), "%s", b64);
	return true;
}

static bool
create_user(const dg_config_t *cfg, const char *alias, const char *algorithm, const char *b64,
    dg_user_t *user, char *err, size_t errsz)
{
	char cfgpath[DG_PATH_MAX], userpath[DG_PATH_MAX], username[DG_ALIAS_MAX * 5];
	FILE *fp;
	str_list_t ini, user_ini;
	unsigned id;
	bool ok;
	if (!dg_path_join(cfgpath, sizeof(cfgpath), cfg->root, "config/gamesrv.ini") ||
	    (fp = iniOpenFile(cfgpath, false)) == NULL) {
		snprintf(err, errsz, "cannot open gamesrv.ini while creating user");
		return false;
	}
	ini = iniReadFile(fp);
	iniCloseFile(fp);
	if (ini == NULL) return false;
	id = iniGetUInteger(ini, "CONFIGURATION", "NextUserId", cfg->next_user_id);
	if (id == 0) id = 1;
	{
		size_t used = 0;
		static const char *invalid = "<>:\"/\\|?*";
		for (size_t i = 0; alias[i] != 0 && used + 6 < sizeof(username); i++) {
			unsigned char ch = (unsigned char)alias[i];
			if (ch < 0x20 || strchr(invalid, ch) != NULL)
				used += (size_t)snprintf(username + used, sizeof(username) - used, "_%u_", ch);
			else
				username[used++] = (char)tolower(ch);
		}
		username[used] = 0;
	}
	{
		char rel[sizeof(username) + 16];
		snprintf(rel, sizeof(rel), "users/%s.ini", username);
		dg_path_join(userpath, sizeof(userpath), cfg->root, rel);
	}
	user_ini = strListInit();
	iniSetUInteger(&user_ini, "USER", "AccessLevel", 10, NULL);
	iniSetString(&user_ini, "USER", "Alias", alias, NULL);
	iniSetBool(&user_ini, "USER", "AllowMultipleConnections", false, NULL);
	iniSetString(&user_ini, "USER", "PasswordHash", "", NULL);
	iniSetString(&user_ini, "USER", "PasswordSalt", "", NULL);
	iniSetUInteger(&user_ini, "USER", "UserId", id, NULL);
	iniSetString(&user_ini, "USER", "SSHKeyAlgorithm", algorithm, NULL);
	iniSetString(&user_ini, "USER", "SSHKey", b64, NULL);
	ok = save_ini(userpath, user_ini);
	strListFree(&user_ini);
	if (!ok) {
		strListFree(&ini);
		snprintf(err, errsz, "cannot create user file %s", userpath);
		return false;
	}
	iniSetUInteger(&ini, "CONFIGURATION", "NextUserId", id + 1, NULL);
	if (!save_ini(cfgpath, ini))
		dg_log(DG_LOG_WARN, "created user %s but could not persist NextUserId", alias);
	strListFree(&ini);
	memset(user, 0, sizeof(*user));
	user->id = id;
	user->access_level = 10;
	snprintf(user->alias, sizeof(user->alias), "%s", alias);
	snprintf(user->key_algorithm, sizeof(user->key_algorithm), "%s", algorithm);
	snprintf(user->key_blob, sizeof(user->key_blob), "%s", b64);
	snprintf(user->ini_path, sizeof(user->ini_path), "%s", userpath);
	return true;
}

bool
dg_password_matches(const dg_config_t *cfg, const dg_user_t *user,
    const uint8_t *password, size_t password_len)
{
	uint8_t digest[64];
	char encoded[128];
	uint8_t *input;
	size_t salt_len, pepper_len, input_len;
	bool ok = false;
	if (!*user->password_hash)
		return false;
	if (dg_stricmp(cfg->password_pepper, "DISABLE") == 0) {
		return strlen(user->password_hash) == password_len &&
		    dssh_crypto_memcmp(user->password_hash, password, password_len) == 0;
	}
	if (!*user->password_salt || !*cfg->password_pepper)
		return false;
	salt_len = strlen(user->password_salt);
	pepper_len = strlen(cfg->password_pepper);
	if (salt_len > SIZE_MAX - password_len || salt_len + password_len > SIZE_MAX - pepper_len)
		return false;
	input_len = salt_len + password_len + pepper_len;
	input = malloc(input_len);
	if (input == NULL) return false;
	memcpy(input, user->password_salt, salt_len);
	{
		size_t src = 0, dst = salt_len;
		while (src < password_len) {
			uint8_t ch = password[src];
			if (ch < 0x80) { input[dst++] = ch; src++; continue; }
			unsigned need = (ch & 0xe0) == 0xc0 ? 2 : (ch & 0xf0) == 0xe0 ? 3 : (ch & 0xf8) == 0xf0 ? 4 : 1;
			bool valid = src + need <= password_len && need > 1;
			for (unsigned i = 1; valid && i < need; i++) valid = (password[src + i] & 0xc0) == 0x80;
			input[dst++] = '?'; src += valid ? need : 1;
		}
		memcpy(input + dst, cfg->password_pepper, pepper_len);
		input_len = dst + pepper_len;
	}
	if (dssh_hash_oneshot("SHA-512", input, input_len, digest, sizeof(digest)) < 0)
		goto done;
	for (unsigned i = 0; i < 1024; i++) {
		uint8_t next[64];
		if (dssh_hash_oneshot("SHA-512", digest, sizeof(digest), next, sizeof(next)) < 0)
			goto done;
		memcpy(digest, next, sizeof(digest));
	}
	if (dssh_base64_encode(digest, sizeof(digest), encoded, sizeof(encoded)) < 0)
		goto done;
	ok = strlen(user->password_hash) == strlen(encoded) &&
	    dssh_crypto_memcmp(user->password_hash, encoded, strlen(encoded)) == 0;
done:
	dssh_cleanse(input, input_len);
	free(input);
	dssh_cleanse(digest, sizeof(digest));
	return ok;
}

bool
dg_user_validate_password(const dg_config_t *cfg, const char *alias,
    const uint8_t *password, size_t password_len, dg_user_t *user)
{
	return dg_alias_valid(alias) && dg_user_find(cfg, alias, user) &&
	    dg_password_matches(cfg, user, password, password_len);
}

bool
dg_user_bind(const dg_config_t *cfg, const char *alias, const char *algorithm,
    const uint8_t *blob, size_t blob_len, bool signed_request, dg_user_t *user,
    char *err, size_t errsz)
{
	char b64[DG_KEY_MAX];
	char key_type[64];
	uint32_t type_len;
	dg_user_t found;
	bool ok = false;
	if (!dg_alias_valid(alias)) {
		snprintf(err, errsz, "invalid alias");
		return false;
	}
	if (dssh_parse_uint32(blob, blob_len, &type_len) < 0 || type_len == 0 ||
	    type_len >= sizeof(key_type) || type_len + 4 > blob_len) {
		snprintf(err, errsz, "invalid SSH public key blob");
		return false;
	}
	memcpy(key_type, blob + 4, type_len); key_type[type_len] = 0;
	if (strcmp(key_type, "ssh-ed25519") != 0 && strcmp(key_type, "ssh-rsa") != 0) {
		snprintf(err, errsz, "unsupported SSH public key type");
		return false;
	}
	if (dssh_base64_encode(blob, blob_len, b64, sizeof(b64)) < 0) {
		snprintf(err, errsz, "public key is too large");
		return false;
	}
	lock_users();
	if (dg_user_find(cfg, alias, &found)) {
		if (!*found.key_blob) {
			snprintf(err, errsz, "legacy account requires sysop authorization");
			goto done;
		}
		if (strlen(found.key_blob) != strlen(b64) ||
		    dssh_crypto_memcmp(found.key_blob, b64, strlen(b64)) != 0) {
			snprintf(err, errsz, "public key does not match this account");
			goto done;
		}
		*user = found;
		ok = true;
	}
	else if (!signed_request) {
		memset(user, 0, sizeof(*user));
		snprintf(user->alias, sizeof(user->alias), "%s", alias);
		ok = true;
	}
	else
		ok = create_user(cfg, alias, key_type, b64, user, err, errsz);
done:
	(void)algorithm;
	unlock_users();
	return ok;
}

static bool
parse_public_key(const char *path, char *algorithm, size_t algsz, char *b64, size_t b64sz,
    uint8_t *blob, size_t *bloblen, char *err, size_t errsz)
{
	uint8_t *data;
	size_t len;
	char *type, *encoded, *end;
	ssize_t decoded;
	uint32_t wire_len;
	if (!dg_read_file(path, &data, &len)) {
		snprintf(err, errsz, "cannot read public key file %s", path);
		return false;
	}
	type = dg_trim((char *)data);
	encoded = strpbrk(type, " \t");
	if (encoded == NULL) goto invalid;
	*encoded++ = 0;
	encoded = dg_trim(encoded);
	end = strpbrk(encoded, " \t\r\n");
	if (end != NULL) *end = 0;
	if (strcmp(type, "ssh-ed25519") != 0 && strcmp(type, "ssh-rsa") != 0) goto invalid;
	if (strlen(type) >= algsz || strlen(encoded) >= b64sz) goto invalid;
	decoded = b64_decode((char *)blob, *bloblen, encoded, 0);
	if (decoded < 8 || (size_t)decoded > *bloblen ||
	    dssh_parse_uint32(blob, (size_t)decoded, &wire_len) < 0 || wire_len != strlen(type) ||
	    wire_len + 4 > (uint32_t)decoded || memcmp(blob + 4, type, wire_len) != 0)
		goto invalid;
	strcpy(algorithm, type);
	strcpy(b64, encoded);
	*bloblen = (size_t)decoded;
	free(data);
	return true;
invalid:
	free(data);
	snprintf(err, errsz, "public key must be an OpenSSH Ed25519 or RSA public key");
	return false;
}

bool
dg_user_authorize(const dg_config_t *cfg, const char *alias, const char *public_key_path,
    bool replace, char *err, size_t errsz)
{
	dg_user_t user;
	char algorithm[64], b64[DG_KEY_MAX];
	uint8_t blob[6144];
	size_t bloblen = sizeof(blob);
	bool ok;
	if (!dg_alias_valid(alias) || !parse_public_key(public_key_path, algorithm, sizeof(algorithm),
	    b64, sizeof(b64), blob, &bloblen, err, errsz))
		return false;
	lock_users();
	if (!dg_user_find(cfg, alias, &user)) {
		snprintf(err, errsz, "no existing GameSrv user named %s", alias);
		ok = false;
	}
	else if (*user.key_blob && !replace) {
		snprintf(err, errsz, "%s already has an SSH key; use --replace to rotate it", alias);
		ok = false;
	}
	else
		ok = set_existing_key(&user, algorithm, b64, err, errsz);
	unlock_users();
	return ok;
}
