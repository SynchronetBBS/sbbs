/*
 * SFTP transfer queue.  One worker thread per direction runs atop the
 * session-scoped sftp_state owned by ssh.c, popping QUEUED jobs off a
 * shared list and transferring them in small chunks so the terminal
 * stays responsive.
 */

#ifndef SYNCTERM_SFTP_QUEUE_H
#define SYNCTERM_SFTP_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

enum sftp_job_dir {
	SFTP_JOB_UP,    /* local → remote */
	SFTP_JOB_DN,    /* remote → local */
};

enum sftp_job_status {
	SFTP_JOB_QUEUED,
	SFTP_JOB_ACTIVE,
	SFTP_JOB_DONE,
	SFTP_JOB_FAILED,
	SFTP_JOB_CANCELLED,
};

struct bbslist;

/*
 * Launches the uploader + downloader threads.  Idempotent.  Must be
 * called only after sftp_available is true.
 */
void sftp_queue_start(void);

/*
 * Binds the queue to a specific BBS for persistence: records the per-BBS
 * cache path, loads any previously-saved jobs, and saves on every state
 * transition thereafter.  Must be called AFTER sftp_queue_start().  Safe
 * to call with NULL to operate the queue purely in-memory.
 */
void sftp_queue_attach_bbs(struct bbslist *bbs);

/*
 * Signals the workers to finish the current chunk and exit; drops all
 * remaining jobs.  Safe to call even if start was never called.  Called
 * from ssh_close() before the SFTP channel is torn down.
 */
void sftp_queue_stop(void);

/*
 * Append a transfer to the queue.  Both paths are copied; the caller
 * retains ownership of the originals.  `expected_bytes` is used for
 * progress display only.
 *
 * Returns false if the queue hasn't been started, or on allocation
 * failure.
 */
bool sftp_queue_enqueue(enum sftp_job_dir dir,
                        const char *local_path,
                        const char *remote_path,
                        uint64_t expected_bytes);

/*
 * Populates *up and *down with the number of jobs currently in
 * QUEUED-or-ACTIVE state for each direction.  Used by the status bar
 * indicator; lock-free.
 */
void sftp_queue_activity(uint32_t *up, uint32_t *dn);

/*
 * True if any job is QUEUED or ACTIVE.  Used by the shell-dead
 * degraded mode to decide whether to keep the session alive after
 * the interactive shell closes.
 */
bool sftp_queue_has_work(void);

/*
 * Bumped on every enqueue and every job state transition.  Consumers
 * (the browser, the queue screen) cache the last seen value and repaint
 * when it changes.
 */
uint64_t sftp_queue_gen(void);

/*
 * Report the current status of (dir, remote_path) as one of the enum
 * values above.  Returns -1 if no such job is in the queue.  Used by
 * the browser to paint the per-entry status column.
 */
int sftp_queue_status(enum sftp_job_dir dir, const char *remote_path);

/*
 * Flag the job identified by (dir, remote_path) for cancellation.  The
 * worker picks it up between chunks and marks the job CANCELLED.
 * Returns true if a matching QUEUED or ACTIVE job was found.
 */
bool sftp_queue_cancel(enum sftp_job_dir dir, const char *remote_path);

/* --- Snapshot API (for the Alt-Q queue screen, Phase 5) --------------- */

struct sftp_job_snap {
	int                 dir;
	int                 status;
	char               *local_path;
	char               *remote_path;
	uint64_t            total;
	uint64_t            done;
	char                err[64];
};

uint32_t sftp_queue_snapshot(struct sftp_job_snap **out);
void     sftp_queue_free_snapshot(struct sftp_job_snap *snap, uint32_t n);

#endif /* SYNCTERM_SFTP_QUEUE_H */
