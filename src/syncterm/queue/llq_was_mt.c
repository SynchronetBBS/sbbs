#include <stdlib.h>

#include "llq_int.h"

bool
llq_was_mt(llq restrict llq)
{
	bool ret;

	CHECK_Q(llq, false);
	MTX_LOCK(llq, nullptr);
	ret = (llq->head == nullptr);
	MTX_UNLOCK(llq);

	return ret;
}
