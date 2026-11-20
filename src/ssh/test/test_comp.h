#ifndef DSSH_TEST_COMP_H
#define DSSH_TEST_COMP_H

#include <stdbool.h>

struct dssh_test_comp_stats {
	unsigned allocations;
	unsigned resets;
	unsigned cleanups;
	unsigned live;
	unsigned compressions;
	unsigned decompressions;
	unsigned bounds;
	unsigned owner_errors;
};

enum dssh_test_comp_stage {
	DSSH_TEST_COMP_NONE,
	DSSH_TEST_COMP_INIT,
	DSSH_TEST_COMP_BOUND,
	DSSH_TEST_COMP_COMPRESS,
	DSSH_TEST_COMP_UNCOMPRESS,
	DSSH_TEST_COMP_CLEANUP,
};

typedef void (*dssh_test_comp_observer)(enum dssh_test_comp_stage stage);
void dssh_test_comp_observe(dssh_test_comp_observer observer);
void dssh_test_comp_replace_on_reset(bool replace);
int dssh_test_comp_reset(void);
void dssh_test_comp_stats(struct dssh_test_comp_stats *out);
void dssh_test_comp_fail(enum dssh_test_comp_stage stage, int result);
int dssh_test_register_growth_comp(void);
int dssh_test_register_growth_alt(void);
/* Optional per-process fixture mode; absent/none preserves ordinary tests. */
int dssh_test_register_optional_comp(void);
bool dssh_test_zlib_available(void);
int dssh_test_register_zlib_comp(void);
void dssh_test_zlib_stats(unsigned *allocations, unsigned *live, unsigned *cleanup_errors);
void dssh_test_zlib_fail_after(int count);
unsigned dssh_test_zlib_resets(void);

#endif
