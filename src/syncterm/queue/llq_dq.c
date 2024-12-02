#include <stddef.h>
#include <stdlib.h>

#include "llq_int.h"

static void *
dq_locked(llq restrict llq)
{
	assert(llq != nullptr && "llq is nullptr");
	if (llq->head == nullptr)
		return nullptr;
	struct linked_list_queue_entry * restrict qe = llq->head;
	llq->head = qe->next;
	if (llq->tail == qe)
		llq->tail = nullptr;
	return qe;
}

void *
llq_dq(llq restrict llq)
{
	void * restrict ret;
	struct linked_list_queue_entry * restrict qe;

	CHECK_Q(llq, nullptr);
	MTX_LOCK(llq, nullptr);
	qe = dq_locked(llq);
	MTX_UNLOCK(llq);

	if (qe == nullptr)
		return nullptr;
	ret = qe->data;
	free(qe);
	return ret;
}

#define TRY_DQ(llq) do {                                              \
	struct linked_list_queue_entry * restrict qe = dq_locked(llq); \
	if (qe != nullptr) {                                            \
		MTX_UNLOCK(llq);                                         \
		void * restrict ret = qe->data;                           \
		free(qe);                                                  \
		return ret;                                                 \
	}                                                                    \
} while(0)

static void *
dq_wait_forever(llq restrict llq)
{
	for (;;) {
		if (cnd_wait(&llq->cnd, &llq->mtx) != thrd_success) {
			SET_ERROR(llq, llq_err_cnd_wait, "cnd_wait() failed");
			MTX_UNLOCK(llq);
			return nullptr;
		}
		TRY_DQ(llq);
	}
	unreachable();
}

static void *
dq_wait_timed(llq restrict llq, const struct timespec * restrict ts)
{
	if (cnd_timedwait(&llq->cnd, &llq->mtx, ts) != thrd_success) {
		SET_ERROR(llq, llq_err_cnd_timedwait, "cnd_timedwait() failed");
		MTX_UNLOCK(llq);
		return nullptr;
	}

	TRY_DQ(llq);
	SET_ERROR(llq, llq_err_timedout, "timeout expired");
	MTX_UNLOCK(llq);
	return nullptr;
}

void *
llq_dq_wait(llq restrict llq, const struct timespec * restrict ts)
{
	CHECK_Q(llq, nullptr);
	MTX_LOCK(llq, nullptr);
	TRY_DQ(llq);

	if (ts == nullptr)
		return dq_wait_forever(llq);
	return dq_wait_timed(llq, ts);
}
