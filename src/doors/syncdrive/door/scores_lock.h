/* scores_lock.h -- serializes SCORES read-merge-write across door processes
 * (and hosts sharing the data dir) with a lock on <data-dir>/SCORES.lck. */
#ifndef SYNCDRIVE_SCORES_LOCK_H_
#define SYNCDRIVE_SCORES_LOCK_H_

void scores_lock_init(const char *data_dir);
int  scores_lock_acquire(void);
void scores_lock_release(void);

#endif
