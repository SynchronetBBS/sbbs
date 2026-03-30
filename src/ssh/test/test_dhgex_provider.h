/*
 * test_dhgex_provider.h -- Test helpers for KEX and key algorithm selection.
 *
 * DH-GEX now has a built-in RFC 3526 group provider, so no per-test
 * provider setup is needed.  This header provides environment-variable
 * helpers for test variant selection.
 */

#ifndef DSSH_TEST_DHGEX_PROVIDER_H
#define DSSH_TEST_DHGEX_PROVIDER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "deucessh.h"
#include "deucessh-algorithms.h"

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
 * No-op: DH-GEX now uses the built-in RFC 3526 default provider.
 * Kept for call-site compatibility.
 */
static inline void
test_dhgex_setup(dssh_session server)
{
	(void)server;
}

/*
 * Returns true if DSSH_TEST_KEY=rsa or DSSH_TEST_KEY=rsa512 is set.
 * When set, tests register an RSA algorithm before ssh-ed25519 so RSA
 * wins host key negotiation, and generate an RSA key instead of ed25519.
 */
static inline bool
test_using_rsa(void)
{
	const char *key = getenv("DSSH_TEST_KEY");
	return (key != NULL && (strcmp(key, "rsa") == 0 ||
	    strcmp(key, "rsa512") == 0));
}

/*
 * Returns true if DSSH_TEST_KEY=rsa512 is set specifically.
 */
static inline bool
test_using_rsa512(void)
{
	const char *key = getenv("DSSH_TEST_KEY");
	return (key != NULL && strcmp(key, "rsa512") == 0);
}

/*
 * Register key algorithms in the order appropriate for the current
 * test configuration.  When DSSH_TEST_KEY=rsa or rsa512, registers
 * the corresponding RSA variant first so it wins negotiation;
 * otherwise ed25519 first.  All are always registered so
 * cross-algorithm scenarios still work.
 */
static inline int
test_register_key_algos(void)
{
	int res;
	if (test_using_rsa512()) {
		res = dssh_register_rsa_sha2_512();
		if (res < 0) return res;
		res = dssh_register_rsa_sha2_256();
		if (res < 0) return res;
		res = dssh_register_ssh_ed25519();
	}
	else if (test_using_rsa()) {
		res = dssh_register_rsa_sha2_256();
		if (res < 0) return res;
		res = dssh_register_rsa_sha2_512();
		if (res < 0) return res;
		res = dssh_register_ssh_ed25519();
	}
	else {
		res = dssh_register_ssh_ed25519();
		if (res < 0) return res;
		res = dssh_register_rsa_sha2_256();
		if (res < 0) return res;
		res = dssh_register_rsa_sha2_512();
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
				int res;
				if (test_using_rsa512()) {
					res = dssh_rsa_sha2_512_load_key_file(
					    keyfile, NULL, NULL);
				}
				else {
					res = dssh_rsa_sha2_256_load_key_file(
					    keyfile, NULL, NULL);
				}
				return res;
			}
			int res;
			if (test_using_rsa512()) {
				res = dssh_rsa_sha2_512_generate_key(2048);
				if (res < 0) return res;
				return dssh_rsa_sha2_512_save_key_file(keyfile,
				    NULL, NULL);
			}
			res = dssh_rsa_sha2_256_generate_key(2048);
			if (res < 0) return res;
			return dssh_rsa_sha2_256_save_key_file(keyfile,
			    NULL, NULL);
		}
		if (test_using_rsa512())
			return dssh_rsa_sha2_512_generate_key(2048);
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
	if (test_using_rsa512())
		return "rsa-sha2-512";
	return test_using_rsa() ? "rsa-sha2-256" : "ssh-ed25519";
}

#endif
