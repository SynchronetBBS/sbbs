#include <stdlib.h>

#include "llq_int.h"

bool
llq_nq(llq restrict llq, void * restrict data)
{
	CHECK_Q(llq, false);

	struct linked_list_queue_entry * restrict qe = calloc(1, sizeof(struct linked_list_queue_entry));
	if (qe == nullptr) {
		MTX_LOCK(llq, false);
		SET_ERROR(llq, llq_err_calloc, "unable to calloc new entry");
		MTX_UNLOCK(llq);
		return false;
	}
	qe->next = nullptr;
	qe->data = data;
	MTX_LOCK(llq, false);
	if (llq->tail != nullptr)
		llq->tail->next = qe;
	if (llq->head == nullptr) {
		llq->head = qe;
		if (cnd_signal(&llq->cnd) != thrd_success)
			SET_ERROR(llq, llq_err_cnd_signal, "cnd_signal() failed");
	}
	llq->tail = qe;
	MTX_UNLOCK(llq);
	return true;
}
