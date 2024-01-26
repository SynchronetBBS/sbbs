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
	unsigned transparent:1;
	unsigned show_title:1;
	unsigned help:1;
};

static bool
rw_set(NewIfcObj obj, int attr, ...)
{
	struct root_window *rw = (struct root_window *)obj;
	SET_VARS;

	rw->api.last_error = NewIfc_error_none;
	va_start(ap, attr);
	switch (attr) {
		case NewIfc_transparent:
			SET_BOOL(rw, transparent);
			break;
		case NewIfc_show_title:
			SET_BOOL(rw, show_title);
			break;
		case NewIfc_show_help:
			SET_BOOL(rw, help);
			break;
		case NewIfc_title:
			SET_STRING(rw, title, title_sz);
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

	return rw->api.last_error == NewIfc_error_none;
}

static bool
rw_get(NewIfcObj obj, int attr, ...)
{
	struct root_window *rw = (struct root_window *)obj;
	GET_VARS;
	int lkret;

	rw->api.last_error = NewIfc_error_none;
	va_start(ap, attr);
	switch (attr) {
		case NewIfc_transparent:
			GET_BOOL(rw, transparent);
			break;
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

	return rw->api.last_error == NewIfc_error_none;
}

static NewIfcObj
rw_copy(NewIfcObj old)
{
	struct root_window *ret;

	ret = malloc(sizeof(struct root_window));
	memcpy(ret, old, sizeof(struct root_window));
	ret->title = strdup(ret->title);

	return (struct newifc_api *)ret;
}

NewIfcObj
NewIFC_root_window(void)
{
	struct root_window *ret;

	ret = malloc(sizeof(struct root_window));

	if (ret) {
		ret->api.get = rw_get;
		ret->api.set = rw_set;
		ret->api.copy = rw_copy;
		ret->api.last_error = NewIfc_error_none;
		ret->api.width = 80;
		ret->api.height = 25;
		ret->transparent = false;
		ret->show_title = true;
		ret->help = true;
		ret->title_sz = 0;
		ret->title = strdup(default_title);
		ret->locks = 0;
		if (ret->title == NULL) {
			free(ret);
			return NULL;
		}
		ret->mtx = pthread_mutex_initializer_np(true);
	}
	return (struct newifc_api *)ret;
}

#ifdef BUILD_TESTS

void test_root_window(CuTest *ct)
{
	bool b;
	uint32_t u32;
	char *s;
	NewIfcObj obj;
	static const char *new_title = "New Title";

	obj = NewIFC_root_window();
	CuAssertPtrNotNull(ct, obj);
	CuAssertPtrNotNull(ct, obj->get);
	CuAssertPtrNotNull(ct, obj->set);
	CuAssertPtrNotNull(ct, obj->copy);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->width == 80);
	CuAssertTrue(ct, obj->height == 25);
	CuAssertTrue(ct, obj->get(obj, NewIfc_transparent, &b) && !b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->get(obj, NewIfc_show_title, &b) && b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->get(obj, NewIfc_show_help, &b) && b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertStrEquals(ct, (obj->get(obj, NewIfc_title, &s), s), default_title);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->get(obj, NewIfc_locked, &b) && !b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->get(obj, NewIfc_locked_by_me, &b) && !b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);

	CuAssertTrue(ct, obj->set(obj, NewIfc_transparent, true));
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->set(obj, NewIfc_show_title, false));
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->set(obj, NewIfc_show_help, false));
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->set(obj, NewIfc_title, new_title));
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, !obj->set(obj, NewIfc_locked, false));
	CuAssertTrue(ct, obj->last_error == NewIfc_error_lock_failed);
	CuAssertTrue(ct, obj->set(obj, NewIfc_locked, true));
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, !obj->set(obj, NewIfc_locked_by_me, &b));
	CuAssertTrue(ct, obj->last_error == NewIfc_error_not_implemented);

	CuAssertTrue(ct, obj->get(obj, NewIfc_transparent, &b) && b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->get(obj, NewIfc_show_title, &b) && !b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->get(obj, NewIfc_show_help, &b) && !b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->get(obj, NewIfc_title, &s));
	CuAssertStrEquals(ct, s, new_title);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->get(obj, NewIfc_locked, &b) && b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->get(obj, NewIfc_locked_by_me, &b) && b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, obj->set(obj, NewIfc_locked, false));
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
	CuAssertTrue(ct, !obj->set(obj, NewIfc_locked, false));
	CuAssertTrue(ct, obj->last_error == NewIfc_error_lock_failed);
	CuAssertTrue(ct, obj->get(obj, NewIfc_locked_by_me, &b) && !b);
	CuAssertTrue(ct, obj->last_error == NewIfc_error_none);
}

CuSuite* root_window_get_test_suite() {
	CuSuite* suite = CuSuiteNew();
	SUITE_ADD_TEST(suite, test_root_window);
	return suite;
}

#endif
