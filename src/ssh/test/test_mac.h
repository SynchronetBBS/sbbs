/*
 * test_mac.h -- Test MAC module with failure injection.
 *
 * Registers as "hmac-sha2-256" with a simple XOR-fold MAC.
 * Use the fail_*_at functions to make the Nth call return an error.
 * Use test_mac_set_corrupt(true) to flip a bit in the MAC output,
 * causing verification mismatches on the receiving side.
 */

#ifndef DSSH_TEST_MAC_H
#define DSSH_TEST_MAC_H

#include <stdbool.h>

/* Register the test mac module.  Call INSTEAD of dssh_register_hmac_sha2_256(). */
int register_test_mac(void);

/*
 * Register a test MAC with a 128-byte tag. Verification storage supports
 * this size; packet capacity/refusal controls choose their own bounds.
 * Uses the name "hmac-sha2-256" so it participates in test negotiation.
 */
int register_test_mac_oversized(void);

/* Reset all failure injection state */
void test_mac_reset(void);

/* Fail the Nth init/generate call (0-based) */
void test_mac_fail_init_at(int n);
bool test_mac_init_failure_seen(void);
void test_mac_fail_generate_at(int n);

/* Corrupt output: flip a bit so receiver's verify fails */
void test_mac_set_corrupt(bool on);

#endif
