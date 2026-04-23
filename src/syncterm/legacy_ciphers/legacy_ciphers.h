/* Copyright (C), 2026 by Stephen Hurd */
/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SYNCTERM_LEGACY_CIPHERS_H
#define SYNCTERM_LEGACY_CIPHERS_H

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Register bundled decrypt-only reference implementations of legacy
 * ciphers (IDEA, RC2, ...) with xp_crypt.  Call once at SyncTERM
 * startup before any iniReadEncryptedFile() that might need them.
 */
void legacy_ciphers_init(void);

void legacy_idea_register(void);
void legacy_rc2_register(void);

#if defined(__cplusplus)
}
#endif

#endif
