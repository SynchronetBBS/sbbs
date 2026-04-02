/*
 * sntrup761_optblocker.c — provide the three optblocker symbols needed
 * by sntrup761.c's generic-C constant-time helpers on architectures
 * without inline-asm paths (i.e. anything other than x86_64 / aarch64).
 *
 * On x86_64 and aarch64 the asm branches are taken and these symbols
 * are dead code, but the linker still needs them on other targets.
 *
 * Matches SUPERCOP's cryptoint/optblocker.c.  Public Domain.
 */

#include <stdint.h>

typedef int16_t crypto_int16;
typedef int32_t crypto_int32;
typedef int64_t crypto_int64;

volatile crypto_int16 crypto_int16_optblocker;
volatile crypto_int32 crypto_int32_optblocker;
volatile crypto_int64 crypto_int64_optblocker;
