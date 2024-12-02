#ifndef DEUCE_QUEUE_H
#define DEUCE_QUEUE_H

#include <stdalign.h>
#include <stdint.h>
#include <threads.h>

typedef struct linked_list_queue *llq;
typedef intptr_t llq_err_t;

enum llq_errval : llq_err_t {
	llq_err_none,
	llq_err_badflags,
	llq_err_calloc,
	llq_err_cnd_broadcast,
	llq_err_cnd_init,
	llq_err_cnd_signal,
	llq_err_cnd_timedwait,
	llq_err_cnd_wait,
	llq_err_null_llq,
	llq_err_mtx_destroy,
	llq_err_mtx_init,
	llq_err_mtx_lock,
	llq_err_mtx_unlock,
	llq_err_not_empty,
	llq_err_timedout,
	llq_err_tss_create,
};

/*
 * Allocates a queue.  flags must be zero.
 * If flags is zero, the queue is FIFO
 * If err is not nullptr, will be filled if llq == nullptr
 */
llq llq_alloc(llq_err_t *err, uint32_t flags, ...);

/*
 * Frees a queue allocated with llq_alloc()
 * if err isn't nullptr, will be filled if an error occurs
 * in the case of errors, llq_free() pretends there were no errors.
 * Hopefully this will crash your program.  You're welcome.
 */
void llq_free(llq restrict llq, llq_err_t *err);

// Enqueues data in llq
bool llq_nq(llq restrict llq, void * restrict data);

// Dequeues an enqueued value from llq
void * llq_dq(llq restrict llq);

/*
 * Blocks for ts time or until it can dequeue a value from llq.
 * if ts is nullptr, blocks forever.
 */
void * llq_dq_wait(llq restrict llq, const struct timespec * restrict ts);

/*
 * Checks llq for an item.  If there was an item in llq when
 * checked, returns true.  Returns false in all other cases
 */
bool llq_was_empty(llq restrict llq);

/*
 * Gets the error status of the queue.
 * Must be called from the same thread where the error occured.
 * msg is only valid until the next call to any llq_*() function.
 * Returns true if no errors occured while getting the error. hehhehheh.
 */
bool llq_get_err(llq restrict llq, llq_err_t * restrict num, char8_t ** restrict msg);

#endif
