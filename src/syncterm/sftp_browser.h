/*
 * Interactive SFTP directory browser.
 *
 * Modal screen triggered by Alt-S from the terminal when
 * sftp_available && sftp_shell_alive.  Shows the current remote
 * directory, lets the user navigate into subdirectories, and
 * compares each remote file against the local download directory
 * (size + mtime) so the caller can see what's already there.
 *
 * Phase 3: read-only — no upload/download/delete actions yet.
 */

#ifndef SYNCTERM_SFTP_BROWSER_H
#define SYNCTERM_SFTP_BROWSER_H

#include "bbslist.h"

void sftp_browser_run(struct bbslist *bbs);

#endif /* SYNCTERM_SFTP_BROWSER_H */
