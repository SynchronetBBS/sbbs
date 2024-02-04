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

static const char default_title[] = "No Title Set";

struct root_window {
	struct newifc_api api;
	struct vmem_cell *display;
	pthread_mutex_t mtx;
	unsigned locks;
};

struct rw_recalc_child_cb_params {
	uint16_t height;
	uint16_t width;
};

static NI_err
rw_set(NewIfcObj obj, int attr, ...)
{
	struct root_window *rw = (struct root_window *)obj;
	NI_err ret = NewIfc_error_none;
	va_list ap;

	va_start(ap, attr);
	switch (attr) {
		case NewIfc_locked:
			if (va_arg(ap, int)) {
				if (pthread_mutex_lock(&rw->mtx) != 0) {
					ret = NewIfc_error_lock_failed;
					break;
				}
				rw->locks++;
			}
			else {
				if (pthread_mutex_trylock(&rw->mtx) != 0) {
					ret = NewIfc_error_lock_failed;
					break;
				}
				if (rw->locks == 0) {
					ret = NewIfc_error_lock_failed;
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
					ret = NewIfc_error_lock_failed;
				}
				rw->locks--;
				if (pthread_mutex_unlock(&rw->mtx) != 0) {
					ret = NewIfc_error_lock_failed;
				}
			}
			break;
		default:
			ret = NewIfc_error_not_implemented;
			break;
	}
	va_end(ap);

	return ret;
}

static NI_err
rw_get(NewIfcObj obj, int attr, ...)
{
	struct root_window *rw = (struct root_window *)obj;
	NI_err ret = NewIfc_error_none;
	va_list ap;
	int lkret;

	va_start(ap, attr);
	switch (attr) {
		case NewIfc_locked:
			if (pthread_mutex_trylock(&rw->mtx) != 0)
				*(va_arg(ap, bool *)) = true;
			else {
				*(va_arg(ap, bool *)) = rw->locks > 0;
				if (pthread_mutex_unlock(&rw->mtx) != 0) {
					ret = NewIfc_error_lock_failed;
					assert(ret != NewIfc_error_lock_failed);
				}
			}
			break;
		case NewIfc_locked_by_me:
			if ((lkret = pthread_mutex_trylock(&rw->mtx)) != 0) {
				if (lkret != EBUSY) {
					ret = NewIfc_error_lock_failed;
					break;
				}
				*(va_arg(ap, bool *)) = false;
			}
			else {
				*(va_arg(ap, bool *)) = rw->locks > 0;
				if (pthread_mutex_unlock(&rw->mtx) != 0) {
					ret = NewIfc_error_lock_failed;
					assert(ret != NewIfc_error_lock_failed);
				}
			}
			break;
		default:
			ret = NewIfc_error_not_implemented;
			break;
	}

	return ret;
}

static NI_err
call_render_handlers(NewIfcObj obj)
{
	enum NewIfc_event_handlers hval = NewIfc_on_render;
	if (obj->handlers == NULL)
		return NewIfc_error_none;
	struct NewIfc_handler **head = bsearch(&hval, obj->handlers, obj->handlers_sz, sizeof(obj->handlers[0]), handler_bsearch_compar);
	if (head == NULL)
		return NewIfc_error_none;
	struct NewIfc_handler *h = *head;
	while (h != NULL) {
		NI_err ret = h->on_render(obj, h->cbdata);
		if (ret != NewIfc_error_none)
			return ret;
		h = h->next;
	}
	return NewIfc_error_none;
}

static NI_err
rw_do_render_recurse(NewIfcObj obj, struct NewIfc_render_context *ctx)
{
	int16_t oxpos, oypos, owidth, oheight;
	if (obj->hidden)
		return NewIfc_error_none;
	NI_err ret = call_render_handlers(obj);
	if (ret != NewIfc_error_none)
		return ret;

	// On entry, ctx contains the render rectangle for this object,
	// so xpos and ypos are "included" in the values... which means
	// this should render the entire rectangle (at least until we
	// have clipping boxes)

	// Fill background of child area
	if (obj->fill_colour != NI_TRANSPARENT || obj->fill_character_colour != NI_TRANSPARENT) {
		size_t c;
		for (size_t y = 0; y < obj->child_height; y++) {
			c = (y + ctx->ypos + obj->child_ypos) * ctx->dwidth;
			for (size_t x = 0; x < obj->child_width; x++) {
				set_vmem_cell(&ctx->vmem[c], obj->fill_character_colour, obj->fill_colour, obj->fill_character, obj->fill_font);
				c++;
			}
		}
	}
	// Fill padding
	if (obj->top_pad || obj->bottom_pad || obj->left_pad || obj->right_pad) {
		size_t c;
		for (size_t y = 0; y < obj->layout_size.height; y++) {
			if ((y < obj->top_pad) || (y >= (obj->layout_size.height - obj->bottom_pad))) {
				c = (y + ctx->ypos + obj->ypos) * ctx->dwidth;
				for (size_t x = 0; x < obj->layout_size.width; x++) {
					set_vmem_cell(&ctx->vmem[c], obj->fg_colour, obj->bg_colour, ' ', obj->font);
					c++;
				}
			}
			else {
				if (obj->left_pad) {
					c = (y + ctx->ypos + obj->ypos) * ctx->dwidth;
					for (size_t x = 0; x < obj->left_pad; x++) {
						set_vmem_cell(&ctx->vmem[c], obj->fg_colour, obj->bg_colour, ' ', obj->font);
						c++;
					}
				}
				if (obj->right_pad) {
					c = (y + ctx->ypos + obj->ypos) * ctx->dwidth;
					c += obj->layout_size.width;
					c -= obj->right_pad;
					for (size_t x = 0; x < obj->right_pad; x++) {
						set_vmem_cell(&ctx->vmem[c], obj->fg_colour, obj->bg_colour, ' ', obj->font);
						c++;
					}
				}
			}
		}
	}

	owidth = ctx->width;
	oheight = ctx->height;
	oxpos = ctx->xpos;
	oypos = ctx->ypos;

	// Calculate this objects internal render rectangle (inside of padding)
	ctx->width = obj->layout_size.width - (obj->left_pad + obj->right_pad);
	ctx->height = obj->layout_size.height - (obj->top_pad + obj->bottom_pad);
	ctx->xpos += obj->left_pad;
	ctx->ypos += obj->top_pad;
	// And render it
	if (obj->root != obj)
		obj->do_render(obj, ctx);

	// Next, we recurse to the children of this object, so update
	// ctx to the child area
	if (obj->bottom_child) {
		ctx->width = obj->child_width;
		ctx->height = obj->child_height;
		ctx->xpos += obj->child_xpos;
		ctx->xpos += obj->bottom_child->xpos;
		ctx->ypos += obj->child_ypos;
		ctx->ypos += obj->bottom_child->ypos;

		// Recurse children
		ret = rw_do_render_recurse(obj->bottom_child, ctx);
		if (ret != NewIfc_error_none)
			return ret;
	}

	// Now, restore 
	ctx->width = owidth;
	ctx->height = oheight;
	ctx->xpos = oxpos;
	ctx->ypos = oypos;
	// And "uncorrect" for our x/ypos
	assert(obj->xpos <= ctx->xpos);
	ctx->xpos -= obj->xpos;
	assert(obj->ypos <= ctx->ypos);
	ctx->ypos -= obj->ypos;
	// Recurse peers
	if (obj->higher_peer) {
		ret = rw_do_render_recurse(obj->higher_peer, ctx);
		if (ret != NewIfc_error_none)
			return ret;
	}
	return NewIfc_error_none;
}

static NI_err
rw_do_render(NewIfcObj obj, struct NewIfc_render_context *nullctx)
{
	(void)nullctx;
	struct root_window *rw = (struct root_window *) obj;
	struct NewIfc_render_context ctx = {
		.vmem = rw->display,
		.dwidth = obj->layout_size.width,
		.dheight = obj->layout_size.height,
		.width = obj->layout_size.width,
		.height = obj->layout_size.height,
		.xpos = 0,
		.ypos = 0,
	};
	NI_err ret = rw_do_render_recurse(obj, &ctx);
	if (ret == NewIfc_error_none) {
		struct root_window *rw = (struct root_window *)obj;
		ciolib_vmem_puttext(obj->xpos + 1, obj->ypos + 1, obj->layout_size.width + obj->xpos, obj->layout_size.height + obj->ypos, rw->display);
	}
	return ret;
}

static NI_err
rw_destroy(NewIfcObj obj)
{
	struct root_window *newrw = (struct root_window *)obj;

	if (obj == NULL)
		return NewIfc_error_invalid_arg;
	free(newrw->display);
	pthread_mutex_destroy(&newrw->mtx);
	free(newrw);
	return NewIfc_error_none;
}

static NI_err
NewIFC_root_window(NewIfcObj parent, NewIfcObj *newobj)
{
	struct root_window **newrw = (struct root_window **)newobj;
	NI_err ret;

	if (parent != NULL || newobj == NULL)
		return NewIfc_error_invalid_arg;
	*newrw = calloc(1, sizeof(struct root_window));
	if (*newrw == NULL)
		return NewIfc_error_allocation_failure;
	(*newrw)->mtx = pthread_mutex_initializer_np(true);
	(*newrw)->api.get = rw_get;
	(*newrw)->api.set = rw_set;
	(*newrw)->api.do_render = &rw_do_render;
	(*newrw)->api.destroy = rw_destroy;
	ret = NI_setup_globals(*newobj, parent);
	if (ret != NewIfc_error_none) {
		pthread_mutex_destroy(&(*newrw)->mtx);
		free(*newrw);
		return ret;
	}
	struct text_info ti;
	ciolib_gettextinfo(&ti);
	int cf = ciolib_getfont(0);
	if (cf < 0)
		cf = 0;
	NI_set_focus(&(*newrw)->api, true);
	NI_set_fg_color(&(*newrw)->api, ciolib_fg);
	NI_set_bg_color(&(*newrw)->api, ciolib_bg);
	NI_set_font(&(*newrw)->api, cf);
	NI_set_width(&(*newrw)->api, ti.screenwidth);
	NI_set_height(&(*newrw)->api, ti.screenheight);
	NI_set_child_width(&(*newrw)->api, ti.screenwidth);
	NI_set_child_height(&(*newrw)->api, ti.screenheight);
	NI_set_fill_character(&(*newrw)->api, ' ');
	NI_set_fill_colour(&(*newrw)->api, ciolib_bg);
	NI_set_fill_character_colour(&(*newrw)->api, ciolib_fg);
	NI_set_fill_font(&(*newrw)->api, cf);

	size_t cells = ti.screenwidth;
	cells *= ti.screenheight;
	(*newrw)->display = malloc(sizeof(struct vmem_cell) * cells);
	if ((*newrw)->display == NULL) {
		pthread_mutex_destroy(&(*newrw)->mtx);
		free(*newrw);
		*newrw = NULL;
		return NewIfc_error_allocation_failure;
	}
	ciolib_vmem_gettext(1, 1, (*newrw)->api.width, (*newrw)->api.height, (*newrw)->display);
	return NewIfc_error_none;
}

#ifdef BUILD_TESTS

void test_root_window(CuTest *ct)
{
	bool b;
	NewIfcObj obj = NULL;

	CuAssertTrue(ct, NewIFC_root_window(NULL, &obj) == NewIfc_error_none);
	CuAssertPtrNotNull(ct, obj);
	CuAssertPtrNotNull(ct, obj->get);
	CuAssertPtrNotNull(ct, obj->set);
	CuAssertPtrNotNull(ct, obj->do_render);
	CuAssertTrue(ct, obj->width == 80);
	CuAssertTrue(ct, obj->height == 25);
	CuAssertTrue(ct, obj->min_width == 80);
	CuAssertTrue(ct, obj->min_height == 25);
	CuAssertTrue(ct, obj->focus == true);
	CuAssertTrue(ct, obj->get(obj, NewIfc_locked, &b) == NewIfc_error_none && !b);
	CuAssertTrue(ct, obj->get(obj, NewIfc_locked_by_me, &b) == NewIfc_error_none && !b);
	CuAssertTrue(ct, obj->fg_colour == 7);
	CuAssertTrue(ct, obj->bg_colour == 0);
	CuAssertTrue(ct, obj->font == 0);

	CuAssertTrue(ct, obj->set(obj, NewIfc_locked, false) == NewIfc_error_lock_failed);
	CuAssertTrue(ct, obj->set(obj, NewIfc_locked, true) == NewIfc_error_none);
	CuAssertTrue(ct, obj->set(obj, NewIfc_locked_by_me, &b) == NewIfc_error_not_implemented);

	CuAssertTrue(ct, obj->get(obj, NewIfc_locked, &b) == NewIfc_error_none && b);
	CuAssertTrue(ct, obj->get(obj, NewIfc_locked_by_me, &b) == NewIfc_error_none && b);
	CuAssertTrue(ct, obj->set(obj, NewIfc_locked, false) == NewIfc_error_none);
	CuAssertTrue(ct, obj->set(obj, NewIfc_locked, false) == NewIfc_error_lock_failed);
	CuAssertTrue(ct, obj->get(obj, NewIfc_locked_by_me, &b) == NewIfc_error_none && !b);

	CuAssertTrue(ct, obj->do_render(obj, NULL) == NewIfc_error_none);
	CuAssertTrue(ct, obj->destroy(obj) == NewIfc_error_none);
}

#endif
