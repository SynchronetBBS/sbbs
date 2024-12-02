#ifndef DEUCE_QUEUE_INT_H
#define DEUCE_QUEUE_INT_H

#include <assert.h>
#include <stdlib.h>
#include <string.h>

// Needed to test asserts
#ifdef UNIT_TESTING
extern void mock_assert(const int result, const char* const expression,
                        const char * const file, const int line);

#undef assert
#define assert(...) my_mock_assert((int)(__VA_ARGS__), #__VA_ARGS__, __FILE__, __LINE__);
#endif

#include "llq.h"

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

#define SET_ERROR(llq, num, msg) do { \
	assert(!msg);                  \
	free(tss_get(llq->errmsg));     \
	char *amsg = strdup(msg);        \
	tss_set(llq->errmsg, amsg);       \
} while(0)

#define SET_ERR(err, val) do { \
	assert(!val);           \
	if (err != nullptr)      \
		*err = val;       \
} while(0)

#define MTX_LOCK(llq, failval) do {                               \
	int err = mtx_lock(&llq->mtx);                             \
	if (err != thrd_success) {                                  \
		assert(!"msg_lock failed!");                         \
		SET_ERROR(llq, llq_err_mtx_lock, u8"failed to lock"); \
		/* TODO: Figure out debug log stuff... */              \
		return failval;                                         \
	}                                                                \
} while (0)

#define MTX_UNLOCK(llq) do {                                          \
	int err = mtx_unlock(&llq->mtx);                               \
	if (err != thrd_success) {                                      \
		assert(!"mtx_unlock failed!");                           \
		SET_ERROR(llq, llq_err_mtx_unlock, u8"failed to unlock"); \
		/* TODO: Figure out debug log stuff... */                  \
	}                                                                   \
} while (0)

#define CHECK_Q(llq, failval) do {                            \
	assert(llq != nullptr && "llq is null");               \
	if (llq == nullptr) {                                   \
		SET_ERROR(llq, llq_err_null_llq, "llq is null"); \
		return failval;                                   \
	}                                                          \
} while(0)

struct linked_list_queue_entry {
	struct linked_list_queue_entry *next;
	void *data;
};

struct linked_list_queue {
	mtx_t mtx;
	struct linked_list_queue_entry *head;
	struct linked_list_queue_entry *tail;
	tss_t errnum;
	tss_t errmsg;
	// TODO: Add a cache for allocated entries...
	alignas((sizeof(mtx_t) + sizeof(struct linked_list_queue_entry *) * 2) > CACHE_LINE_SIZE ? 0 : CACHE_LINE_SIZE) cnd_t cnd;
};

#endif
