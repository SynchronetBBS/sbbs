/* Optional application module example; not an installed library interface. */
#ifndef DSSH_EXAMPLE_ZLIB_COMP_H
#define DSSH_EXAMPLE_ZLIB_COMP_H

#ifdef __cplusplus
extern "C" {
#endif

int dssh_example_register_zlib_comp(void);

#ifdef DSSH_EXAMPLE_ZLIB_TESTING
void dssh_example_zlib_stats(unsigned *allocated, unsigned *remaining, unsigned *errors);
unsigned dssh_example_zlib_resets(void);
void dssh_example_zlib_fail_after(int count);
#endif

#ifdef __cplusplus
}
#endif

#endif
