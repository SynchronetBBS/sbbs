/* Copyright (c) 2026 Stephen Hurd
 * SPDX-License-Identifier: BSD-2-Clause */
#include "deucessh.h"

#if EXPECTED_STANDARD == 11
_Static_assert(__STDC_VERSION__ == 201112L, "C11 mode must actually be selected");
#else
_Static_assert(__STDC_VERSION__ >= 201710L, "C17 mode must actually be selected");
#endif

int
main(void)
{
	return 0;
}
