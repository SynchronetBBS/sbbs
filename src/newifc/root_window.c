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
rw_recalc_child_cb(NewIfcObj obj, void *cbdata)
{
	struct rw_recalc_child_cb_params *nsz = cbdata;

	if (obj->height > nsz->height)
		return NewIfc_error_wont_fit;
	if (obj->width > nsz->width)
		return NewIfc_error_wont_fit;
	return NewIfc_error_none;
}

static void
rw_recalc_child(struct root_window *rw, uint16_t height, uint16_t width)
{
	uint16_t losty = 0;
	struct rw_recalc_child_cb_params nsz = {height, width};

	if (rw->show_title) {
		nsz.height--;
		losty++;
	}
	if (rw->help) {
		nsz.height--;
		losty++;
	}
	if (losty > height) {
		rw->api.last_error = NewIfc_error_wont_fit;
		return;
	}
	if (NI_walk_children((NewIfcObj)rw, false, rw_recalc_child_cb, &nsz) != NewIfc_error_none) {
		rw->api.last_error = NewIfc_error_wont_fit;
		return;
	}
	rw->api.child_height = nsz.height;
	rw->api.child_ypos = rw->show_title ? 1 : 0;
	return;
}

static NI_err
rw_set(NewIfcObj obj, int attr, ...)
{
	struct root_window *rw = (struct root_window *)obj;
	SET_VARS;

	rw->api.last_error = NewIfc_error_none;
	va_start(ap, attr);
	switch (attr) {
		case NewIfc_show_title:
			SET_BOOL(rw, show_title);
			break;
		case NewIfc_show_help:
			SET_BOOL(rw, help);
			rw_recalc_child(rw, rw->api.height, rw->api.width);
			break;
		case NewIfc_title:
			SET_STRING(rw, title, title_sz);
			rw_recalc_child(rw, rw->api.height, rw->api.width);
			break;
		case NewIfc_height:
			rw_recalc_child(rw, va_arg(ap, int), rw->api.width);
			break;
		case NewIfc_width:
			rw_recalc_child(rw, rw->api.height, va_arg(ap, int));
			break;
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
		case NewIfc_show_title:
			GET_BOOL(rw, show_title);
			break;
		case NewIfc_show_help:
			GET_BOOL(rw, help);
			break;
		case NewIfc_title:
			GET_STRING(rw, title);
			break;
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
	(*newrw)->title = strdup(oldrw->title);
	if ((*newrw)->title == NULL) {
		free(*newrw);
		return NewIfc_error_allocation_failure;
	}

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
	(*newrw)->api.xpos = 0;
	(*newrw)->api.ypos = 0;
	(*newrw)->api.child_xpos = 0;
	(*newrw)->api.child_ypos = 1;
	(*newrw)->api.child_width = 80;
	(*newrw)->api.child_height = 23;
	(*newrw)->api.min_height = 2;
	(*newrw)->api.min_width = 40;
	// TODO: This is only needed by the unit tests...
	(*newrw)->api.root = *newobj;
	(*newrw)->api.focus = true;
	(*newrw)->show_title = true;
	(*newrw)->help = true;
	(*newrw)->title_sz = 0;
	(*newrw)->title = strdup(default_title);
	(*newrw)->locks = 0;
	if ((*newrw)->title == NULL) {
		free(*newrw);
		return NewIfc_error_allocation_failure;
	}
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
	CuAssertTrue(ct, obj->get(obj, NewIfc_show_title, &b) == NewIfc_error_none && b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->get(obj, NewIfc_show_help, &b) == NewIfc_error_none && b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->get(obj, NewIfc_title, &s) == NewIfc_error_none);
	CuAssertStrEquals(ct, s, default_title);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->get(obj, NewIfc_locked, &b) == NewIfc_error_none && !b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->get(obj, NewIfc_locked_by_me, &b) == NewIfc_error_none && !b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);

	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->set(obj, NewIfc_show_title, false) == NewIfc_error_none);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->set(obj, NewIfc_show_help, false) == NewIfc_error_none);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->set(obj, NewIfc_title, new_title) == NewIfc_error_none);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->set(obj, NewIfc_locked, false) == NewIfc_error_lock_failed);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_lock_failed);
	CuAssertTrue(ct, obj->set(obj, NewIfc_locked, true) == NewIfc_error_none);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->set(obj, NewIfc_locked_by_me, &b) == NewIfc_error_not_implemented);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_not_implemented);

	CuAssertTrue(ct, obj->get(obj, NewIfc_show_title, &b) == NewIfc_error_none && !b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->get(obj, NewIfc_show_help, &b) == NewIfc_error_none && !b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->get(obj, NewIfc_title, &s) == NewIfc_error_none);
	CuAssertStrEquals(ct, s, new_title);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
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
