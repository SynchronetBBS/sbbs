#include "llq_int.h"

bool
llq_get_err(llq restrict llq, llq_err_t * restrict num, char8_t ** restrict msg)
{
	CHECK_Q(llq, false);
	if (num != nullptr)
		*num = (llq_err_t)tss_get(llq->errnum);
	if (msg != nullptr)
		*msg = (char8_t *)tss_get(llq->errnum);
	return true;
}
