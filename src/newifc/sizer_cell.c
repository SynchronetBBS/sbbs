#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>  // malloc()/free()

#define CIOLIB_NO_MACROS
#include "ciolib.h"
#include "genwrap.h"
#include "strwrap.h"
#include "threadwrap.h"

#include "newifc.h"
#include "newifc_internal.h"

#ifdef BUILD_TESTS
#include "CuTest.h"
#endif

struct sizer_cell {
	struct newifc_api api;
};

static NI_err
sizer_cell_set(NewIfcObj obj, int attr, ...)
{
	struct sizer_cell *s = (struct sizer_cell *)obj;
	NI_err ret = NewIfc_error_none;
	va_list ap;
	size_t c;
	size_t idx;
	uint8_t olds;
	uint8_t *neww;

	va_start(ap, attr);
	switch (attr) {
		default:
			ret = NewIfc_error_not_implemented;
			break;
	}
	va_end(ap);

	return ret;
}

static NI_err
sizer_cell_get(NewIfcObj obj, int attr, ...)
{
	struct sizer_cell *s = (struct sizer_cell *)obj;
	NI_err ret = NewIfc_error_none;
	va_list ap;
	size_t c;
	size_t idx;

	va_start(ap, attr);
	switch (attr) {
		default:
			ret = NewIfc_error_not_implemented;
			break;
	}

	return ret;
}

static NI_err
sizer_cell_do_render(NewIfcObj obj, struct NewIfc_render_context *ctx)
{
	(void)obj;
	(void)ctx;
	return NewIfc_error_none;
}

static NI_err
sizer_cell_destroy(NewIfcObj newobj)
{
	struct sizer_cell *s = (struct sizer_cell *)newobj;

	if (s == NULL)
		return NewIfc_error_invalid_arg;
	free(s);
	return NewIfc_error_none;
}

static NI_err
NewIFC_sizer_cell(NewIfcObj parent, NewIfcObj *newobj)
{
	struct sizer_cell *news;

	if (newobj == NULL)
		return NewIfc_error_invalid_arg;
	if (parent->type != NewIfc_sizer)
		return NewIfc_error_invalid_arg;
	*newobj = NULL;
	news = calloc(1, sizeof(struct sizer_cell));
	if (news == NULL)
		return NewIfc_error_allocation_failure;
	NI_err ret = NI_setup_globals(&news->api, parent);
	if (ret != NewIfc_error_none) {
		free(news);
		return ret;
	}
	news->api.get = &sizer_cell_get;
	news->api.set = &sizer_cell_set;
	news->api.do_render = &sizer_cell_do_render;
	news->api.destroy = &sizer_cell_destroy;
	*newobj = (NewIfcObj)news;
	return NewIfc_error_none;
}

#ifdef BUILD_TESTS

void test_sizer_cell(CuTest *ct)
{
	// TODO: Write tests...
}

#endif
