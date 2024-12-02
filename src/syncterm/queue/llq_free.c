#include <stdlib.h>

#include "llq_int.h"

void
llq_free(llq restrict llq, llq_err_t *err)
{
	if (llq == nullptr) {
		SET_ERR(err, llq_err_null_llq);
		return;
	}
	if (mtx_lock(&llq->mtx) != thrd_success)
		SET_ERR(err, llq_err_mtx_lock);
	for (struct linked_list_queue_entry * restrict qe = llq->head; qe; qe = llq->head) {
		SET_ERR(err, llq_err_not_empty);
		llq->head = qe->next;
		free(qe);
	}
	// Try to make anyone else waiting crash...
	// We could memset these things to make it more likely. 😈
	tss_delete(llq->errnum);
	tss_delete(llq->errmsg);
	if (cnd_broadcast(&llq->cnd) != thrd_success)
		SET_ERR(err, llq_err_cnd_broadcast);
	cnd_destroy(&llq->cnd);
	if (mtx_unlock(&llq->mtx) != thrd_success)
		SET_ERR(err, llq_err_mtx_unlock);
	mtx_destroy(&llq->mtx);
	if (err)
		*err = llq_err_none;
	free(llq);
}
