#include "deucegate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dirwrap.h"

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

static int failures;

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expr); failures++; \
} } while (0)

static void
test_cp437(void)
{
	dg_decoder_t decoder;
	uint8_t utf8[32], cp[32];
	const uint8_t input[] = {0x41, 0xb3, 0xdb};
	dg_decoder_init(&decoder, DG_CP437);
	size_t n = dg_decode(&decoder, input, sizeof(input), utf8, sizeof(utf8), true);
	CHECK(n == 7);
	CHECK(dg_encode(DG_CP437, utf8, n, cp, sizeof(cp)) == sizeof(input));
	CHECK(memcmp(cp, input, sizeof(input)) == 0);
}

static void
test_encoding_matrix(void)
{
	static const uint8_t cp437[] = {'A', 0xb3, 0xdb, '\r', '\n'};
	dg_decoder_t client_decoder, door_decoder;
	uint8_t canonical[64], edge[64], returned[64], client_out[64];
	size_t canonical_len, edge_len, returned_len, client_len;
	for (int client_enc = DG_CP437; client_enc <= DG_UTF8; client_enc++) {
		for (int door_enc = DG_CP437; door_enc <= DG_UTF8; door_enc++) {
			dg_decoder_init(&client_decoder, DG_CP437);
			canonical_len = dg_decode(&client_decoder, cp437, sizeof(cp437), canonical, sizeof(canonical), true);
			/* Model the caller edge in its selected encoding, then the door edge. */
			client_len = dg_encode((dg_encoding_t)client_enc, canonical, canonical_len, client_out, sizeof(client_out));
			dg_decoder_init(&client_decoder, (dg_encoding_t)client_enc);
			canonical_len = dg_decode(&client_decoder, client_out, client_len, canonical, sizeof(canonical), true);
			edge_len = dg_encode((dg_encoding_t)door_enc, canonical, canonical_len, edge, sizeof(edge));
			dg_decoder_init(&door_decoder, (dg_encoding_t)door_enc);
			returned_len = dg_decode(&door_decoder, edge, edge_len, returned, sizeof(returned), true);
			client_len = dg_encode((dg_encoding_t)client_enc, returned, returned_len, client_out, sizeof(client_out));
			if (client_enc == DG_CP437) {
				CHECK(client_len == sizeof(cp437));
				CHECK(memcmp(client_out, cp437, sizeof(cp437)) == 0);
			}
			else {
				CHECK(client_len == returned_len);
				CHECK(memcmp(client_out, returned, returned_len) == 0);
			}
		}
	}
}

static void
test_utf8_stream(void)
{
	dg_decoder_t decoder;
	uint8_t out[32];
	const uint8_t first[] = {0xe2, 0x94};
	const uint8_t second[] = {0x82};
	dg_decoder_init(&decoder, DG_UTF8);
	CHECK(dg_decode(&decoder, first, sizeof(first), out, sizeof(out), false) == 0);
	CHECK(dg_decode(&decoder, second, sizeof(second), out, sizeof(out), false) == 3);
	CHECK(memcmp(out, "\xe2\x94\x82", 3) == 0);
}

static void
test_mci(void)
{
	dg_config_t cfg = {0};
	dg_user_t user = {.access_level = 10};
	dg_mci_context_t ctx = {.config = &cfg, .user = &user, .node = 2, .seconds_left = 310};
	char out[256];
	strcpy(cfg.bbs_name, "Test BBS");
	strcpy(user.alias, "rumpelstiltskin");
	dg_mci_expand("{ALIAS10}|{10ALIAS}|{NODE}|{TIMELEFT}|{UNKNOWN}", out, sizeof(out), &ctx);
	CHECK(strcmp(out, "rumpelstil|lstiltskin|2|00:05:10|{UNKNOWN}") == 0);
}

static void
test_legacy_password(void)
{
	dg_config_t cfg = {0};
	dg_user_t user = {0};
	static const char expected[] =
	    "V9NISNAVEi2u9QewAgiQuFiim1HxNhHOy9H5Xxi3bucgzOw8DXyzp0cjb3D/1j77uuhjGe0fSOGWpBHakw2y9A==";
	strcpy(cfg.password_pepper, "pepper");
	strcpy(user.password_salt, "salt");
	strcpy(user.password_hash, expected);
	CHECK(dg_password_matches(&cfg, &user, (const uint8_t *)"secret", 6));
	CHECK(!dg_password_matches(&cfg, &user, (const uint8_t *)"wrong", 5));
	strcpy(cfg.password_pepper, "DISABLE");
	strcpy(user.password_hash, "plain");
	CHECK(dg_password_matches(&cfg, &user, (const uint8_t *)"plain", 5));
}

static bool
nth_line(const uint8_t *data, size_t len, unsigned wanted, char *out, size_t outsz)
{
	size_t start = 0;
	unsigned current = 1;
	for (size_t pos = 0; pos <= len; pos++) {
		if (pos != len && data[pos] != '\r' && data[pos] != '\n') continue;
		if (current == wanted) {
			size_t count = pos - start;
			if (count >= outsz) count = outsz - 1;
			memcpy(out, data + start, count);
			out[count] = 0;
			return true;
		}
		if (pos < len && data[pos] == '\r' && pos + 1 < len && data[pos + 1] == '\n') pos++;
		start = pos + 1;
		current++;
	}
	return false;
}

static void
test_chain_txt(void)
{
	dg_config_t cfg = {0};
	dg_client_t client = {0};
	char root[DG_PATH_MAX], node_dir[DG_PATH_MAX], path[DG_PATH_MAX], line[64];
	uint8_t *data = NULL;
	size_t len = 0;
	static const char *files[] = {
		"door.sys", "door32.sys", "doorfile.sr", "dorinfo.def", "dorinfo1.def", "dorinfo42.def", "chain.txt", NULL
	};
#ifdef _WIN32
	{
		char temp[DG_PATH_MAX];
		DWORD count = GetTempPathA(sizeof(temp), temp);
		CHECK(count > 0 && count < sizeof(temp));
		snprintf(root, sizeof(root), "%sdeucegate-chain-%ld", temp, (long)getpid());
	}
#else
	snprintf(root, sizeof(root), "/tmp/deucegate-chain-%ld", (long)getpid());
#endif
	CHECK(mkpath(root) == 0 || dg_dir_exists(root));
	snprintf(cfg.root, sizeof(cfg.root), "%s", root);
	strcpy(cfg.bbs_name, "Drop Test");
	strcpy(cfg.sysop_first_name, "Test");
	strcpy(cfg.sysop_last_name, "Sysop");
	strcpy(cfg.sysop_name, "Test Sysop");
	cfg.time_per_call = 60;
	client.config = &cfg;
	client.node = 42;
	client.cols = 132;
	client.rows = 50;
	client.terminal = DG_TERM_ANSI;
	client.started = time(NULL) - 60;
	client.user.id = 7;
	client.user.access_level = 20;
	strcpy(client.user.alias, "Chain User");
	CHECK(dg_create_drop_files(&client, 0, node_dir, sizeof(node_dir)));
	CHECK(dg_path_join(path, sizeof(path), node_dir, "chain.txt"));
	CHECK(dg_read_file(path, &data, &len));
	if (data != NULL) {
		CHECK(nth_line(data, len, 4, line, sizeof(line)) && strcmp(line, "") == 0);
		CHECK(nth_line(data, len, 9, line, sizeof(line)) && strcmp(line, "132") == 0);
		CHECK(nth_line(data, len, 10, line, sizeof(line)) && strcmp(line, "50") == 0);
		CHECK(nth_line(data, len, 30, line, sizeof(line)) && strcmp(line, "8N1") == 0);
		free(data);
	}
	for (size_t i = 0; files[i] != NULL; i++) {
		if (dg_path_join(path, sizeof(path), node_dir, files[i])) remove(path);
	}
	rmdir(node_dir);
	rmdir(root);
}

int
main(void)
{
	test_cp437();
	test_encoding_matrix();
	test_utf8_stream();
	test_mci();
	test_legacy_password();
	test_chain_txt();
	if (failures != 0) return 1;
	puts("All DeuceGate tests passed.");
	return 0;
}
