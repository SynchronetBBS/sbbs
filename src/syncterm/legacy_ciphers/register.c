/* Copyright (C), 2026 by Stephen Hurd */
/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "legacy_ciphers.h"

void
legacy_ciphers_init(void)
{
	legacy_idea_register();
	legacy_rc2_register();
}
