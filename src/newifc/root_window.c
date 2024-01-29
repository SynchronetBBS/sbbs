#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>  // malloc()/free()

#include "genwrap.h"
#include "strwrap.h"
#include "threadwrap.h"

#include "newifc.h"
#include "newifc_internal.h"

#include "internal_macros.h"

#ifdef BUILD_TESTS
#include "CuTest.h"
#endif

static const char default_title[] = "No Title Set";

struct root_window {
	struct newifc_api api;
	char *title;
	size_t title_sz;
	pthread_mutex_t mtx;
	unsigned locks;
	unsigned show_title:1;
	unsigned help:1;
	unsigned dirty:1;
};

struct rw_recalc_child_cb_params {
	uint16_t height;
	uint16_t width;
};

static NI_err
rw_set(NewIfcObj obj, int attr, ...)
{
	struct root_window *rw = (struct root_window *)obj;
	SET_VARS;

	rw->api.last_error = NewIfc_error_none;
	va_start(ap, attr);
	switch (attr) {
		case NewIfc_locked:
			if (va_arg(ap, int)) {
				if (pthread_mutex_lock(&rw->mtx) != 0) {
					rw->api.last_error = NewIfc_error_lock_failed;
					break;
				}
				rw->locks++;
			}
			else {
				if (pthread_mutex_trylock(&rw->mtx) != 0) {
					rw->api.last_error = NewIfc_error_lock_failed;
					break;
				}
				if (rw->locks == 0) {
					rw->api.last_error = NewIfc_error_lock_failed;
					// TODO: Unlock failure can't be recovered and is silent... :(
#ifndef NDEBUG
					int lkret =
#endif
					pthread_mutex_unlock(&rw->mtx);
#ifndef NDEBUG
					assert(lkret == 0);
#endif
					break;
				}
				if (pthread_mutex_unlock(&rw->mtx) != 0) {
					rw->api.last_error = NewIfc_error_lock_failed;
				}
				rw->locks--;
				if (pthread_mutex_unlock(&rw->mtx) != 0) {
					rw->api.last_error = NewIfc_error_lock_failed;
				}
			}
			break;
		default:
			rw->api.last_error = NewIfc_error_not_implemented;
			break;
	}
	va_end(ap);

	return rw->api.last_error;
}

static NI_err
rw_get(NewIfcObj obj, int attr, ...)
{
	struct root_window *rw = (struct root_window *)obj;
	GET_VARS;
	int lkret;

	rw->api.last_error = NewIfc_error_none;
	va_start(ap, attr);
	switch (attr) {
		case NewIfc_locked:
			if (pthread_mutex_trylock(&rw->mtx) != 0)
				*(va_arg(ap, bool *)) = true;
			else {
				*(va_arg(ap, bool *)) = rw->locks > 0;
				if (pthread_mutex_unlock(&rw->mtx) != 0) {
					rw->api.last_error = NewIfc_error_lock_failed;
					assert(rw->api.last_error != NewIfc_error_lock_failed);
					break;
				}
			}
			break;
		case NewIfc_locked_by_me:
			if ((lkret = pthread_mutex_trylock(&rw->mtx)) != 0) {
				if (lkret != EBUSY) {
					rw->api.last_error = NewIfc_error_lock_failed;
					break;
				}
				*(va_arg(ap, bool *)) = false;
			}
			else {
				*(va_arg(ap, bool *)) = rw->locks > 0;
				if (pthread_mutex_unlock(&rw->mtx) != 0) {
					rw->api.last_error = NewIfc_error_lock_failed;
					assert(rw->api.last_error != NewIfc_error_lock_failed);
				}
			}
			break;
		default:
			rw->api.last_error = NewIfc_error_not_implemented;
			break;
	}

	return rw->api.last_error;
}

static NI_err
rw_copy(NewIfcObj old, NewIfcObj *newobj)
{
	struct root_window **newrw = (struct root_window **)newobj;
	struct root_window *oldrw = (struct root_window *)old;

	*newrw = malloc(sizeof(struct root_window));
	if (*newrw == NULL) {
		return NewIfc_error_allocation_failure;
	}
	memcpy(*newrw, old, sizeof(struct root_window));

	return NewIfc_error_none;
}

static NI_err
NewIFC_root_window(NewIfcObj *newobj)
{
	struct root_window **newrw = (struct root_window **)newobj;

	*newrw = malloc(sizeof(struct root_window));

	if (*newrw == NULL) {
		return NewIfc_error_allocation_failure;
	}
	(*newrw)->api.get = rw_get;
	(*newrw)->api.set = rw_set;
	(*newrw)->api.copy = rw_copy;
	(*newrw)->api.last_error = NewIfc_error_none;
	(*newrw)->api.width = 80;
	(*newrw)->api.height = 25;
	(*newrw)->api.child_width = 80;
	(*newrw)->api.child_height = 25;
	(*newrw)->api.xpos = 0;
	(*newrw)->api.ypos = 0;
	(*newrw)->api.child_xpos = 0;
	(*newrw)->api.child_ypos = 1;
	(*newrw)->api.min_height = 2;
	(*newrw)->api.min_width = 40;
	// TODO: This is only needed by the unit tests...
	(*newrw)->api.root = *newobj;
	(*newrw)->api.focus = true;
	(*newrw)->locks = 0;
	(*newrw)->mtx = pthread_mutex_initializer_np(true);

	return NewIfc_error_none;
}

#ifdef BUILD_TESTS

void test_root_window(CuTest *ct)
{
	bool b;
	char *s;
	NewIfcObj obj;
	static const char *new_title = "New Title";

	CuAssertTrue(ct, !NewIFC_root_window(&obj));
	CuAssertPtrNotNull(ct, obj);
	CuAssertPtrNotNull(ct, obj->get);
	CuAssertPtrNotNull(ct, obj->set);
	CuAssertPtrNotNull(ct, obj->copy);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->width == 80);
	CuAssertTrue(ct, obj->height == 25);
	CuAssertTrue(ct, obj->min_width == 40);
	CuAssertTrue(ct, obj->min_height == 2);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->focus == true);
	CuAssertTrue(ct, obj->get(obj, NewIfc_locked, &b) == NewIfc_error_none && !b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->get(obj, NewIfc_locked_by_me, &b) == NewIfc_error_none && !b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);

	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->set(obj, NewIfc_locked, false) == NewIfc_error_lock_failed);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_lock_failed);
	CuAssertTrue(ct, obj->set(obj, NewIfc_locked, true) == NewIfc_error_none);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->set(obj, NewIfc_locked_by_me, &b) == NewIfc_error_not_implemented);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_not_implemented);

	CuAssertTrue(ct, obj->get(obj, NewIfc_locked, &b) == NewIfc_error_none && b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->get(obj, NewIfc_locked_by_me, &b) == NewIfc_error_none && b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->set(obj, NewIfc_locked, false) == NewIfc_error_none);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->set(obj, NewIfc_locked, false) == NewIfc_error_lock_failed);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_lock_failed);
	CuAssertTrue(ct, obj->get(obj, NewIfc_locked_by_me, &b) == NewIfc_error_none && !b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
}

CuSuite* root_window_get_test_suite() {
	CuSuite* suite = CuSuiteNew();
	SUITE_ADD_TEST(suite, test_root_window);
	return suite;
}

#endif
