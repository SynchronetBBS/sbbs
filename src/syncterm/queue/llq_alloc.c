#include <stdlib.h>

#include "llq_int.h"

llq
llq_alloc(llq_err_t *err, uint32_t flags, ...)
{
	if (flags != 0) {
		SET_ERR(err, llq_err_badflags);
		return nullptr;
	}

	llq ret = calloc(1, sizeof(struct linked_list_queue));
	if (ret == nullptr) {
		SET_ERR(err, llq_err_calloc);
		return nullptr;
	}
	ret->head = nullptr;
	ret->tail = nullptr;
	if (mtx_init(&ret->mtx, mtx_plain) != thrd_success) {
		SET_ERR(err, llq_err_mtx_init);
		free(ret);
		return nullptr;
	}
	if (cnd_init(&ret->cnd) != thrd_success) {
		SET_ERR(err, llq_err_cnd_init);
		mtx_destroy(&ret->mtx);
		free(ret);
		return nullptr;
	}
	if (tss_create(&ret->errnum, nullptr) != thrd_success) {
		SET_ERR(err, llq_err_tss_create);
		cnd_destroy(&ret->cnd);
		mtx_destroy(&ret->mtx);
		free(ret);
		return nullptr;
	}
	if (tss_create(&ret->errmsg, free) != thrd_success) {
		SET_ERR(err, llq_err_tss_create);
		tss_delete(ret->errnum);
		cnd_destroy(&ret->cnd);
		mtx_destroy(&ret->mtx);
		free(ret);
		return nullptr;
	}
	if (err)
		*err = llq_err_none;

	return ret;
}
