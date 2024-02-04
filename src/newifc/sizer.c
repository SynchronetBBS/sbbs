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

struct sizer {
	struct newifc_api api;
	uint8_t *weights;
	uint8_t group_size;
};

static size_t
count_children(NewIfcObj obj)
{
	size_t ret = 0;
	for (obj = obj->top_child; obj; obj = obj->lower_peer)
		ret++;
	return ret;
}

static NI_err
sizer_set(NewIfcObj obj, int attr, ...)
{
	struct sizer *s = (struct sizer *)obj;
	NI_err ret = NewIfc_error_none;
	va_list ap;
	size_t c;
	size_t idx;
	uint8_t olds;
	uint8_t *neww;

	va_start(ap, attr);
	switch (attr) {
		case NewIfc_group_size:
			olds = s->group_size;
			s->group_size = va_arg(ap, int);
			if (s->group_size == 0) {
				s->group_size = olds;
				return NewIfc_error_out_of_range;
			}
			neww = realloc(s->weights, s->group_size * sizeof(s->weights[0]));
			if (neww == NULL) {
				s->group_size = olds;
				return NewIfc_error_allocation_failure;
			}
			s->weights = neww;
			if (s->group_size > olds) {
				for (uint8_t i = olds; i < s->group_size; i++)
					s->weights[i] = 0;
			}
			// TODO: Trigger new layout
			break;
		case NewIfc_weight:
			c = count_children(obj);
			idx = c % s->group_size;
			s->weights[idx] = va_arg(ap, int);
			break;
		default:
			ret = NewIfc_error_not_implemented;
			break;
	}
	va_end(ap);

	return ret;
}

static NI_err
sizer_get(NewIfcObj obj, int attr, ...)
{
	struct sizer *s = (struct sizer *)obj;
	NI_err ret = NewIfc_error_none;
	va_list ap;
	size_t c;
	size_t idx;

	va_start(ap, attr);
	switch (attr) {
		case NewIfc_group_size:
			*(va_arg(ap, bool *)) = s->group_size;
			break;
		case NewIfc_weight:
			c = count_children(obj);
			idx = c % s->group_size;
			*(va_arg(ap, uint8_t *)) = s->weights[idx];
			break;
		default:
			ret = NewIfc_error_not_implemented;
			break;
	}

	return ret;
}

static NI_err
sizer_do_render(NewIfcObj obj, struct NewIfc_render_context *ctx)
{
	(void)obj;
	(void)ctx;
	return NewIfc_error_none;
}

static NI_err
sizer_destroy(NewIfcObj newobj)
{
	struct sizer *s = (struct sizer *)newobj;

	if (s == NULL)
		return NewIfc_error_invalid_arg;
	free(s);
	return NewIfc_error_none;
}

static NI_err
sizer_resize_width_cb(NewIfcObj obj, const uint16_t newval, void *width)
{
	struct sizer *s = (struct sizer *)obj;
	size_t idx;
	size_t weight = 0;
	size_t max_weight = 0;
	uint16_t w[s->group_size];
	NewIfcObj c;

	memset(w, 0, sizeof(w));
	// Collect min sizes of children
	for (c = s->api.top_child; c; c = c->lower_peer) {
		idx = (idx + 1) % s->group_size;
		if (c->min_width > w[idx])
			w[idx] = c->min_width;
	}

	// Distribute extra space by weight
	size_t remain = newval;
	for (idx = 0; idx < s->group_size; idx++) {
		if (w[idx] > remain)
			return NewIfc_error_wont_fit;
		remain -= w[idx];
		if (s->weights[idx] > max_weight)
			max_weight = s->weights[idx];
	}
	if (max_weight) {
		while (remain) {
			if (weight == 0)
				weight = max_weight;
			for (idx = 0; idx < s->group_size; idx++) {
				if (weight <= s->weights[idx]) {
					w[idx]++;
					remain--;
					if (remain == 0)
						break;
				}
			}
			weight--;
		}
	}
	// TODO: Store in magic place which is named child_width, but knows what column it's in :(
}

static NI_err
NewIFC_sizer(NewIfcObj parent, NewIfcObj *newobj)
{
	struct sizer *news;

	if (newobj == NULL)
		return NewIfc_error_invalid_arg;
	*newobj = NULL;
	news = calloc(1, sizeof(struct sizer));
	if (news == NULL)
		return NewIfc_error_allocation_failure;
	NI_err ret = NI_setup_globals(&news->api, parent);
	if (ret != NewIfc_error_none) {
		free(news);
		return ret;
	}
	news->api.get = &sizer_get;
	news->api.set = &sizer_set;
	news->api.do_render = &sizer_do_render;
	news->api.destroy = &sizer_destroy;
	news->group_size = 1;
	news->weights = calloc(1, sizeof(news->weights[0]));
	if (news->weights == NULL) {
		free(news);
		return NewIfc_error_allocation_failure;
	}
	*newobj = (NewIfcObj)news;
	return NewIfc_error_none;
}

#ifdef BUILD_TESTS

void test_sizer(CuTest *ct)
{
	// TODO: Write tests...
}

#endif
