/*
 * test_dhgex_provider.h — Minimal DH-GEX provider for tests.
 *
 * Returns the RFC 3526 group 14 (2048-bit MODP) prime and generator.
 * Include this header and call test_dhgex_setup(server_session) after
 * dssh_session_init(false, 0) when DSSH_TEST_KEX=dh-gex is set.
 */

#ifndef DSSH_TEST_DHGEX_PROVIDER_H
#define DSSH_TEST_DHGEX_PROVIDER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "deucessh.h"
#include "deucessh-algorithms.h"

/*
 * RFC 3526 s3 — 2048-bit MODP Group (group 14).
 * Generator g = 2.
 */
static const uint8_t rfc3526_grp14_p[] = {
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xC9,0x0F,0xDA,0xA2,0x21,0x68,0xC2,0x34,
	0xC4,0xC6,0x62,0x8B,0x80,0xDC,0x1C,0xD1,
	0x29,0x02,0x4E,0x08,0x8A,0x67,0xCC,0x74,
	0x02,0x0B,0xBE,0xA6,0x3B,0x13,0x9B,0x22,
	0x51,0x4A,0x08,0x79,0x8E,0x34,0x04,0xDD,
	0xEF,0x95,0x19,0xB3,0xCD,0x3A,0x43,0x1B,
	0x30,0x2B,0x0A,0x6D,0xF2,0x5F,0x14,0x37,
	0x4F,0xE1,0x35,0x6D,0x6D,0x51,0xC2,0x45,
	0xE4,0x85,0xB5,0x76,0x62,0x5E,0x7E,0xC6,
	0xF4,0x4C,0x42,0xE9,0xA6,0x37,0xED,0x6B,
	0x0B,0xFF,0x5C,0xB6,0xF4,0x06,0xB7,0xED,
	0xEE,0x38,0x6B,0xFB,0x5A,0x89,0x9F,0xA5,
	0xAE,0x9F,0x24,0x11,0x7C,0x4B,0x1F,0xE6,
	0x49,0x28,0x66,0x51,0xEC,0xE4,0x5B,0x3D,
	0xC2,0x00,0x7C,0xB8,0xA1,0x63,0xBF,0x05,
	0x98,0xDA,0x48,0x36,0x1C,0x55,0xD3,0x9A,
	0x69,0x16,0x3F,0xA8,0xFD,0x24,0xCF,0x5F,
	0x83,0x65,0x5D,0x23,0xDC,0xA3,0xAD,0x96,
	0x1C,0x62,0xF3,0x56,0x20,0x85,0x52,0xBB,
	0x9E,0xD5,0x29,0x07,0x70,0x96,0x96,0x6D,
	0x67,0x0C,0x35,0x4E,0x4A,0xBC,0x98,0x04,
	0xF1,0x74,0x6C,0x08,0xCA,0x18,0x21,0x7C,
	0x32,0x90,0x5E,0x46,0x2E,0x36,0xCE,0x3B,
	0xE3,0x9E,0x77,0x2C,0x18,0x0E,0x86,0x03,
	0x9B,0x27,0x83,0xA2,0xEC,0x07,0xA2,0x8F,
	0xB5,0xC5,0x5D,0xF0,0x6F,0x4C,0x52,0xC9,
	0xDE,0x2B,0xCB,0xF6,0x95,0x58,0x17,0x18,
	0x39,0x95,0x49,0x7C,0xEA,0x95,0x6A,0xE5,
	0x15,0xD2,0x26,0x18,0x98,0xFA,0x05,0x10,
	0x15,0x72,0x8E,0x5A,0x8A,0xAC,0xAA,0x68,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
};

static int
test_dhgex_select_group(uint32_t min, uint32_t preferred, uint32_t max,
    uint8_t **p_out, size_t *p_len,
    uint8_t **g_out, size_t *g_len, void *cbdata)
{
	(void)min; (void)preferred; (void)max; (void)cbdata;

	*p_len = sizeof(rfc3526_grp14_p);
	*p_out = malloc(*p_len);
	if (*p_out == NULL)
		return -1;
	memcpy(*p_out, rfc3526_grp14_p, *p_len);

	*g_len = 1;
	*g_out = malloc(1);
	if (*g_out == NULL) {
		free(*p_out);
		return -1;
	}
	(*g_out)[0] = 2;

	return 0;
}

static struct dssh_dh_gex_provider test_dhgex_provider = {
	.select_group = test_dhgex_select_group,
	.cbdata = NULL,
};

/*
 * Returns true if DSSH_TEST_KEX=dh-gex is set.
 */
static inline bool
test_using_dhgex(void)
{
	const char *kex = getenv("DSSH_TEST_KEX");
	return (kex != NULL && strcmp(kex, "dh-gex") == 0);
}

/*
 * Returns true if DSSH_TEST_KEX=sntrup is set.
 */
static inline bool
test_using_sntrup(void)
{
	const char *kex = getenv("DSSH_TEST_KEX");
	return (kex != NULL && strcmp(kex, "sntrup") == 0);
}

/*
 * Returns true if DSSH_TEST_KEX=mlkem is set.
 */
static inline bool
test_using_mlkem(void)
{
	const char *kex = getenv("DSSH_TEST_KEX");
	return (kex != NULL && strcmp(kex, "mlkem") == 0);
}

/*
 * Call after dssh_session_init(false, 0) on a server session
 * when using DH-GEX.
 */
static inline void
test_dhgex_setup(dssh_session server)
{
	if (test_using_dhgex())
		dssh_dh_gex_set_provider(server, &test_dhgex_provider);
}

/*
 * Returns true if DSSH_TEST_KEY=rsa is set.
 * When set, tests register rsa-sha2-256 before ssh-ed25519 so RSA
 * wins host key negotiation, and generate an RSA key instead of ed25519.
 */
static inline bool
test_using_rsa(void)
{
	const char *key = getenv("DSSH_TEST_KEY");
	return (key != NULL && strcmp(key, "rsa") == 0);
}

/*
 * Register key algorithms in the order appropriate for the current
 * test configuration.  When DSSH_TEST_KEY=rsa, registers RSA first
 * so it wins negotiation; otherwise ed25519 first.  Both are always
 * registered so cross-algorithm scenarios still work.
 */
static inline int
test_register_key_algos(void)
{
	int res;
	if (test_using_rsa()) {
		res = dssh_register_rsa_sha2_256();
		if (res < 0) return res;
		res = dssh_register_ssh_ed25519();
	}
	else {
		res = dssh_register_ssh_ed25519();
		if (res < 0) return res;
		res = dssh_register_rsa_sha2_256();
	}
	return res;
}

/*
 * Generate or load the host key appropriate for the current test
 * configuration.
 *
 * If DSSH_TEST_RSA_KEY / DSSH_TEST_ED25519_KEY is set to a file path,
 * the key is loaded from that file if it exists, or generated and saved
 * there if it doesn't.  This avoids repeated RSA keygen (~200ms each)
 * across test runs.
 *
 * If the env var is not set, a fresh key is generated every time
 * (original behavior).
 */
static inline int
test_generate_host_key(void)
{
	const char *keyfile;
	if (test_using_rsa()) {
		keyfile = getenv("DSSH_TEST_RSA_KEY");
		if (keyfile != NULL) {
			FILE *fp = fopen(keyfile, "r");
			if (fp != NULL) {
				fclose(fp);
				return dssh_rsa_sha2_256_load_key_file(keyfile,
				    NULL, NULL);
			}
			int res = dssh_rsa_sha2_256_generate_key(2048);
			if (res < 0) return res;
			return dssh_rsa_sha2_256_save_key_file(keyfile,
			    NULL, NULL);
		}
		return dssh_rsa_sha2_256_generate_key(2048);
	}
	keyfile = getenv("DSSH_TEST_ED25519_KEY");
	if (keyfile != NULL) {
		FILE *fp = fopen(keyfile, "r");
		if (fp != NULL) {
			fclose(fp);
			return dssh_ed25519_load_key_file(keyfile,
			    NULL, NULL);
		}
		int res = dssh_ed25519_generate_key();
		if (res < 0) return res;
		return dssh_ed25519_save_key_file(keyfile, NULL, NULL);
	}
	return dssh_ed25519_generate_key();
}

/*
 * Return the key algorithm name for the current test configuration.
 */
static inline const char *
test_key_algo_name(void)
{
	return test_using_rsa() ? "rsa-sha2-256" : "ssh-ed25519";
}

#endif
