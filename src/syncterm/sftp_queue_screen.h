/*
 * Alt-Q queue screen — lists all SFTP transfer jobs with live progress,
 * allows cancelling the selected job.  The queue machinery lives in
 * sftp_queue.[ch]; this screen only drives the UI.
 */

#ifndef SYNCTERM_SFTP_QUEUE_SCREEN_H
#define SYNCTERM_SFTP_QUEUE_SCREEN_H

struct bbslist;

void sftp_queue_screen_run(struct bbslist *bbs);

#endif
