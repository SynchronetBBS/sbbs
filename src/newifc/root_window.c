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
rw_copy(NewIfcObj old, NewIfcObj *newobj)
{
	struct root_window **newrw = (struct root_window **)newobj;
	struct root_window *oldrw = (struct root_window *)old;

	*newrw = malloc(sizeof(struct root_window));
	if (*newrw == NULL) {
		return NewIfc_error_allocation_failure;
	}
	memcpy(*newrw, old, sizeof(struct root_window));
	(*newrw)->mtx = pthread_mutex_initializer_np(true);
	(*newrw)->locks = 0;
	size_t cells = (*newrw)->api.width;
	cells *= (*newrw)->api.height;
	(*newrw)->display = malloc(sizeof(struct vmem_cell) * cells);
	if ((*newrw)->display == NULL) {
		free(*newrw);
		*newrw = NULL;
		return NewIfc_error_allocation_failure;
	}
	memcpy((*newrw)->display, oldrw->display, sizeof(struct vmem_cell) * cells);

	return NewIfc_error_none;
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
	NI_err ret = call_render_handlers(obj);
	if (ret != NewIfc_error_none)
		return ret;

	// Fill background
	if (obj->fill_colour != NI_TRANSPARENT || obj->fill_character_colour != NI_TRANSPARENT) {
		size_t c;
		for (size_t y = 0; y < obj->child_height; y++) {
			c = (y + ctx->ypos + obj->child_ypos) * ctx->dwidth;
			for (size_t x = 0; x < obj->child_width; x++) {
				if (obj->fill_colour != NI_TRANSPARENT)
					ctx->vmem[c].bg = obj->fill_colour;
				if (obj->fill_character_colour != NI_TRANSPARENT) {
					ctx->vmem[c].fg = obj->fill_character_colour;
					ctx->vmem[c].ch = obj->fill_character;
					if (ciolib_checkfont(obj->fill_font))
						ctx->vmem[c].font = obj->fill_font;
				}
				c++;
			}
		}
	}
	// Fill padding
	if (obj->top_pad || obj->bottom_pad || obj->left_pad || obj->right_pad) {
		size_t c;
		for (size_t y = 0; y < obj->height; y++) {
			if ((y < obj->top_pad) || (y >= (obj->height - obj->bottom_pad))) {
				c = (y + ctx->ypos + obj->ypos) * ctx->dwidth;
				for (size_t x = 0; x < obj->width; x++) {
					if (obj->bg_colour != NI_TRANSPARENT)
						ctx->vmem[c].bg = obj->bg_colour;
					if (obj->fg_colour != NI_TRANSPARENT) {
						ctx->vmem[c].fg = obj->fg_colour;
						ctx->vmem[c].ch = ' ';
						if (ciolib_checkfont(obj->font))
							ctx->vmem[c].font = obj->font;
					}
					c++;
				}
			}
			else {
				if (obj->left_pad) {
					c = (y + ctx->ypos + obj->ypos) * ctx->dwidth;
					for (size_t x = 0; x < obj->left_pad; x++) {
						if (obj->bg_colour != NI_TRANSPARENT)
							ctx->vmem[c].bg = obj->bg_colour;
						if (obj->fg_colour != NI_TRANSPARENT) {
							ctx->vmem[c].fg = obj->fg_colour;
							ctx->vmem[c].ch = ' ';
							if (ciolib_checkfont(obj->font))
								ctx->vmem[c].font = obj->font;
						}
						c++;
					}
				}
				if (obj->right_pad) {
					c = (y + ctx->ypos + obj->ypos) * ctx->dwidth;
					c += obj->width;
					c -= obj->right_pad;
					for (size_t x = 0; x < obj->right_pad; x++) {
						if (obj->bg_colour != NI_TRANSPARENT)
							ctx->vmem[c].bg = obj->bg_colour;
						if (obj->fg_colour != NI_TRANSPARENT) {
							ctx->vmem[c].fg = obj->fg_colour;
							ctx->vmem[c].ch = ' ';
							if (ciolib_checkfont(obj->font))
								ctx->vmem[c].font = obj->font;
						}
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
	ctx->width = obj->width - (obj->left_pad + obj->right_pad);
	ctx->height = obj->height - (obj->top_pad + obj->bottom_pad);
	ctx->xpos += obj->left_pad;
	ctx->ypos += obj->top_pad;
	if (obj->root != obj)
		obj->do_render(obj, ctx);

	ctx->width = obj->child_width;
	ctx->height = obj->child_height;
	assert(ctx->xpos >= obj->xpos);
	ctx->xpos -= obj->xpos;
	assert(ctx->ypos >= obj->ypos);
	ctx->ypos -= obj->ypos;

	// Recurse children
	if (obj->bottom_child) {
		ret = rw_do_render_recurse(obj->bottom_child, ctx);
		if (ret != NewIfc_error_none)
			return ret;
	}
	ctx->width = owidth;
	ctx->height = oheight;
	ctx->xpos = oxpos;
	ctx->ypos = oypos;
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
	struct root_window *rw = (struct root_window *) obj;
	struct NewIfc_render_context ctx = {
		.vmem = rw->display,
		.dwidth = obj->width,
		.dheight = obj->height,
		.width = obj->width,
		.height = obj->height,
		.xpos = 0,
		.ypos = 0,
	};
	NI_err ret = rw_do_render_recurse(obj, &ctx);
	if (ret == NewIfc_error_none) {
		struct root_window *rw = (struct root_window *)obj;
		ciolib_vmem_puttext(obj->xpos + 1, obj->ypos + 1, obj->width + obj->xpos, obj->height + obj->ypos, rw->display);
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

	if (parent != NULL || newobj == NULL)
		return NewIfc_error_invalid_arg;
	*newrw = calloc(1, sizeof(struct root_window));

	if (*newrw == NULL)
		return NewIfc_error_allocation_failure;
	(*newrw)->api.get = rw_get;
	(*newrw)->api.set = rw_set;
	(*newrw)->api.copy = rw_copy;
	(*newrw)->api.destroy = rw_destroy;
	// TODO: This is only needed by the unit tests...
	(*newrw)->api.root = *newobj;
	(*newrw)->api.focus = true;
	(*newrw)->api.do_render = &rw_do_render;
	(*newrw)->api.fg_colour = NI_LIGHTGRAY;
	(*newrw)->api.bg_colour = NI_BLACK;
	(*newrw)->api.font = 0;
	(*newrw)->mtx = pthread_mutex_initializer_np(true);

	struct text_info ti;
	ciolib_gettextinfo(&ti);
	(*newrw)->api.width = ti.screenwidth;
	(*newrw)->api.height = ti.screenheight;
	(*newrw)->api.child_width = ti.screenwidth;
	(*newrw)->api.child_height = ti.screenheight;
	(*newrw)->api.fill_character = ' ';
	(*newrw)->api.fill_colour = ciolib_bg;
	(*newrw)->api.fill_character_colour = ciolib_fg;
	int cf = ciolib_getfont(0);
	if (cf < 0)
		cf = 0;
	(*newrw)->api.fill_font = cf;
	size_t cells = ti.screenwidth;
	cells *= ti.screenheight;
	(*newrw)->display = malloc(sizeof(struct vmem_cell) * cells);
	if ((*newrw)->display == NULL) {
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
	CuAssertPtrNotNull(ct, obj->copy);
	CuAssertPtrNotNull(ct, obj->do_render);
	CuAssertTrue(ct, obj->width == 80);
	CuAssertTrue(ct, obj->height == 25);
	CuAssertTrue(ct, obj->min_width == 0);
	CuAssertTrue(ct, obj->min_height == 0);
	CuAssertTrue(ct, obj->focus == true);
	CuAssertTrue(ct, obj->get(obj, NewIfc_locked, &b) == NewIfc_error_none && !b);
	CuAssertTrue(ct, obj->get(obj, NewIfc_locked_by_me, &b) == NewIfc_error_none && !b);
	CuAssertTrue(ct, obj->fg_colour == NI_LIGHTGRAY);
	CuAssertTrue(ct, obj->bg_colour == NI_BLACK);
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
