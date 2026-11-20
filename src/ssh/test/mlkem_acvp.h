#ifndef DSSH_TEST_MLKEM_ACVP_H
#define DSSH_TEST_MLKEM_ACVP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum dssh_acvp_kind {
	DSSH_ACVP_KEYGEN = 1,
	DSSH_ACVP_ENCAP,
	DSSH_ACVP_DECAP,
	DSSH_ACVP_DECAP_SEED,
	DSSH_ACVP_PUBLIC_CHECK,
	DSSH_ACVP_PRIVATE_CHECK,
	DSSH_ACVP_KIND_COUNT
};

/* Each string object stays below C17's minimum supported literal length.
 * Separate case objects keep the table representable with 16-bit size_t. */
struct dssh_acvp_hex {
	const char *chunks[3];
	size_t length;
};

struct dssh_acvp_case {
	const char *profile;
	unsigned group;
	unsigned id;
	enum dssh_acvp_kind kind;
	bool valid;
	bool implicit_rejection;
	struct dssh_acvp_hex d, z, m, ek, dk, c, k;
};

struct dssh_acvp_decoded {
	uint8_t d[32], z[32], m[32];
	uint8_t ek[1184], dk[2400], c[1088], k[32];
};

typedef bool (*dssh_acvp_evaluate)(const struct dssh_acvp_case *, const struct dssh_acvp_decoded *);
int dssh_acvp_run(int argc, char **argv, const char *provider, dssh_acvp_evaluate evaluate);

#ifdef __cplusplus
}
#endif
#endif
