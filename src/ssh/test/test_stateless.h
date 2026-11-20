#ifndef DSSH_TEST_STATELESS_H
#define DSSH_TEST_STATELESS_H

/* Deterministic test adapters, not cryptographic implementations. */
#define DSSH_TEST_STATELESS_NAME "test-stateless"
struct dssh_test_stateless_stats {
	unsigned enc_init;
	unsigned enc_cleanup;
	unsigned encrypt;
	unsigned decrypt;
	unsigned mac_init;
	unsigned mac_cleanup;
	unsigned generate;
	unsigned errors;
};
int dssh_test_stateless_reset(void);
int dssh_test_register_stateless(void);
void dssh_test_stateless_stats(struct dssh_test_stateless_stats *out);

#endif
