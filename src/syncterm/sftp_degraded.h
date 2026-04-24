/*
 * Shell-dead degraded mode: overlay that runs after the interactive
 * shell channel closes when SFTP transfers are still in progress.
 *
 * Called from term.c's disconnect branch.  Returns only when the queue
 * has drained or the user chose to abandon transfers; the caller then
 * proceeds to normal disconnect teardown.
 */

#ifndef SYNCTERM_SFTP_DEGRADED_H
#define SYNCTERM_SFTP_DEGRADED_H

struct bbslist;

void sftp_degraded_run(struct bbslist *bbs);

#endif
