/* Fresh-process consumer of the production library and public headers only.
 * Exercise release entry restrictions before the first session can freeze the
 * registry, and again afterwards. No private reset or test-library guard. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "deucessh-crypto.h"
#include "deucessh-result.h"

struct entry_probe {
	bool valid;
	unsigned checks;
	unsigned releases;
	unsigned unexpected_count;
	dssh_session unexpected[4];
	struct dssh_result_lease outer;
	struct dssh_result_lease inner;
};

static int
unused_io(uint8_t *buf, size_t len, dssh_session sess, void *opaque)
{
	(void)buf;
	(void)len;
	(void)sess;
	(void)opaque;
	return DSSH_ERROR_INIT;
}

static void
check_status(struct entry_probe *probe, int status)
{
	probe->checks++;
	if (status != DSSH_ERROR_INVALID)
		probe->valid = false;
}

static void
check_entries(struct entry_probe *probe)
{
	/* All inputs are otherwise valid; ordinary argument rejection must not
	 * mask whether the callback context itself was checked. */
	check_status(probe, dssh_key_algo_set_ctx("rsa-sha2-256", NULL));
	check_status(probe, dssh_kex_set_ctx("diffie-hellman-group-exchange-sha256", NULL));
	check_status(probe, dssh_transport_set_version("ReleaseProbe", NULL));
	check_status(probe, dssh_transport_set_callbacks(unused_io, unused_io, NULL, NULL));
	check_status(probe, dssh_transport_set_tx_gather(NULL));
	check_status(probe, dssh_register_curve25519_sha256());
	check_status(probe, dssh_register_ssh_ed25519());
	check_status(probe, dssh_register_none_enc());
	check_status(probe, dssh_register_none_mac());
	check_status(probe, dssh_register_none_comp());
	check_status(probe, dssh_register_aes128_cbc());
	dssh_session sess = dssh_session_init(true, 0);
	probe->checks++;
	if (sess != NULL) {
		probe->valid = false;
		if (probe->unexpected_count == 4)
			abort();
		probe->unexpected[probe->unexpected_count++] = sess;
	}
	/* The permitted primitive utilities still operate inside release. */
	static const uint8_t expected[32] = {
		0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
		0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
		0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
		0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55,
	};
	uint8_t digest[32];
	int ret = dssh_hash_oneshot("SHA-256", NULL, 0, digest, sizeof(digest));
	if (ret != 0 || memcmp(digest, expected, sizeof(digest)) != 0)
		probe->valid = false;
	dssh_cleanse(digest, sizeof(digest));
}

static void
release_inner(void *opaque)
{
	struct entry_probe *probe = opaque;
	probe->releases++;
	if (probe->inner.release != NULL || probe->inner.owner != NULL)
		probe->valid = false;
	check_entries(probe);
}

static void
release_outer(void *opaque)
{
	struct entry_probe *probe = opaque;
	probe->releases++;
	if (probe->outer.release != NULL || probe->outer.owner != NULL)
		probe->valid = false;
	probe->inner.release = release_inner;
	probe->inner.owner = probe;
	dssh_result_release(&probe->inner);
	dssh_result_release(&probe->inner);
	/* Nested release must restore the outer callback's restriction. */
	check_entries(probe);
}

static void
run_release(struct entry_probe *probe)
{
	probe->outer.release = release_outer;
	probe->outer.owner = probe;
	dssh_result_release(&probe->outer);
	dssh_result_release(&probe->outer);
}

int
main(void)
{
	struct entry_probe probe = {
		.valid = true,
		.unexpected = {NULL, NULL, NULL, NULL},
		.outer = {.release = NULL, .owner = NULL},
		.inner = {.release = NULL, .owner = NULL},
	};
	if (dssh_register_rsa_sha2_256() != 0 || dssh_register_dh_gex_sha256() != 0) {
		return 1;
	}
	run_release(&probe);
	bool before = probe.valid && probe.checks == 24 && probe.releases == 2;
	/* The callback must neither freeze nor alter any registration. The same
	 * inputs and registrations are still accepted in ordinary caller context. */
	int accepted[11] = {
		dssh_key_algo_set_ctx("rsa-sha2-256", NULL),
		dssh_kex_set_ctx("diffie-hellman-group-exchange-sha256", NULL),
		dssh_transport_set_version("ReleaseEntry", NULL),
		dssh_transport_set_callbacks(unused_io, unused_io, NULL, NULL),
		dssh_transport_set_tx_gather(NULL),
		dssh_register_curve25519_sha256(),
		dssh_register_ssh_ed25519(),
		dssh_register_none_enc(),
		dssh_register_none_mac(),
		dssh_register_none_comp(),
		dssh_register_aes128_cbc(),
	};
	for (size_t i = 0; i < sizeof(accepted) / sizeof(accepted[0]); i++) {
		if (accepted[i] != 0)
			probe.valid = false;
	}
	dssh_session sess = dssh_session_init(true, 0);
	if (sess == NULL)
		probe.valid = false;
	run_release(&probe);
	bool after = probe.valid && probe.checks == 48 && probe.releases == 4;
	bool restored = sess != NULL && dssh_session_set_timeout(sess, 100) == 0
	    && dssh_transport_set_version("Frozen", NULL) == DSSH_ERROR_TOOLATE;
	dssh_session_cleanup(sess);
	for (unsigned i = 0; i < probe.unexpected_count; i++)
		dssh_session_cleanup(probe.unexpected[i]);
	printf("release_entries=%u releases=%u before=%d after=%d restored=%d\n",
	    probe.checks, probe.releases, before, after, restored);
	return before && after && restored ? 0 : 1;
}
