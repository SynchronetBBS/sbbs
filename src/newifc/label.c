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

struct label {
	struct newifc_api api;
	char *text;
};

static NI_err
label_set(NewIfcObj obj, int attr, ...)
{
	struct label *l = (struct label *)obj;
	NI_err ret = NewIfc_error_none;
	va_list ap;
	char *buf;

	va_start(ap, attr);
	switch (attr) {
		case NewIfc_text:
			buf = strdup(va_arg(ap, const char *));
			if (buf == NULL) {
				ret = NewIfc_error_allocation_failure;
				break;
			}
			free(l->text);
			l->text = buf;
			break;
		default:
			ret = NewIfc_error_not_implemented;
			break;
	}
	va_end(ap);

	return ret;
}

static NI_err
label_get(NewIfcObj obj, int attr, ...)
{
	struct label *l = (struct label *)obj;
	NI_err ret = NewIfc_error_none;
	va_list ap;

	va_start(ap, attr);
	switch (attr) {
		case NewIfc_text:
			*(va_arg(ap, char **)) = l->text;
			break;
		default:
			ret = NewIfc_error_not_implemented;
			break;
	}

	return ret;
}

static NI_err
label_copy(NewIfcObj old, NewIfcObj *newobj)
{
	struct label **newl = (struct label **)newobj;
	struct label *oldl = (struct label *)old;

	*newl = malloc(sizeof(struct label));
	if (*newl == NULL)
		return NewIfc_error_allocation_failure;
	memcpy(*newl, oldl, sizeof(struct label));
	assert(oldl->text);
	(*newl)->text = strdup(oldl->text);
	if ((*newl)->text == NULL) {
		free(*newl);
		return NewIfc_error_allocation_failure;
	}

	return NewIfc_error_none;
}

static NI_err
label_do_render(NewIfcObj obj, struct NewIfc_render_context *ctx)
{
	size_t tsz = 0;
	uint16_t xpos, ypos;
	struct label *l = (struct label *) obj;
	if (l->text)
		tsz = strlen(l->text);
	NI_err ret = NI_get_top_left(obj, tsz, 1, &xpos, &ypos);
	ypos -= ctx->ypos;
	xpos -= ctx->xpos;
	if (ret != NewIfc_error_none)
		return ret;
	// TODO: Ensure text fits...
	for (size_t y = 0; y < ctx->height; y++) {
		size_t c = (ctx->ypos + y) * ctx->dwidth + ctx->xpos;
		for (size_t x = 0; x < ctx->width; x++, c++) {
			if (l->api.bg_colour != NI_TRANSPARENT)
				ctx->vmem[c].bg = l->api.bg_colour;
			if (l->api.fg_colour != NI_TRANSPARENT) {
				ctx->vmem[c].fg = l->api.fg_colour;
				ctx->vmem[c].font = l->api.font;
				if (y == ypos && x >= xpos && x < xpos + tsz)
					ctx->vmem[c].ch = l->text[x - xpos];
				else
					ctx->vmem[c].ch = ' ';
			}
		}
	}
	return NewIfc_error_none;
}

static NI_err
label_destroy(NewIfcObj newobj)
{
	struct label *l = (struct label *)newobj;

	if (l == NULL)
		return NewIfc_error_invalid_arg;
	free(l->text);
	free(l);
	return NewIfc_error_none;
}

static NI_err
NewIFC_label(NewIfcObj parent, NewIfcObj *newobj)
{
	struct label **newl = (struct label **)newobj;

	if (parent == NULL || newobj == NULL)
		return NewIfc_error_invalid_arg;
	if (parent->child_height < 1)
		return NewIfc_error_wont_fit;
	(*newl) = calloc(1, sizeof(struct label));

	if (*newl == NULL)
		return NewIfc_error_allocation_failure;
	(*newl)->text = strdup("");
	if ((*newl)->text == NULL) {
		free(*newl);
		return NewIfc_error_allocation_failure;
	}
	NI_copy_globals(&((*newl)->api), parent);
	(*newl)->api.get = &label_get;
	(*newl)->api.set = &label_set;
	(*newl)->api.copy = &label_copy;
	(*newl)->api.do_render = &label_do_render;
	(*newl)->api.destroy = &label_destroy;
	(*newl)->api.child_ypos = 0;
	(*newl)->api.child_xpos = 0;
	(*newl)->api.child_width = 0;
	(*newl)->api.child_height = 0;
	(*newl)->api.parent = parent;
	(*newl)->api.width = parent->child_width;
	(*newl)->api.height = parent->child_height;
	(*newl)->api.min_width = 0;
	(*newl)->api.min_height = 1;
	(*newl)->api.left_pad = 0;
	(*newl)->api.right_pad = 0;
	(*newl)->api.top_pad = 0;
	(*newl)->api.bottom_pad = 0;
	return NewIfc_error_none;
}

#ifdef BUILD_TESTS

void test_label(CuTest *ct)
{
	char *s;
	NewIfcObj obj = NULL;
	NewIfcObj robj = NULL;
	static const char *new_title = "New Title";
	struct vmem_cell cells;

	CuAssertTrue(ct, NewIFC_root_window(NULL, &robj) == NewIfc_error_none);
	CuAssertPtrNotNull(ct, robj);
	CuAssertTrue(ct, NewIFC_label(robj, &obj) == NewIfc_error_none);
	obj->root = robj;
	obj->parent = robj;
	robj->top_child = obj;
	robj->bottom_child = obj;
	CuAssertPtrNotNull(ct, obj);
	CuAssertPtrNotNull(ct, obj->get);
	CuAssertPtrNotNull(ct, obj->set);
	CuAssertPtrNotNull(ct, obj->copy);
	CuAssertPtrNotNull(ct, obj->do_render);
	CuAssertTrue(ct, obj->width == 80);
	CuAssertTrue(ct, obj->height == 25);
	CuAssertTrue(ct, obj->min_width == 0);
	CuAssertTrue(ct, obj->min_height == 1);
	CuAssertTrue(ct, obj->focus == false);
	CuAssertTrue(ct, obj->get(obj, NewIfc_text, &s) == NewIfc_error_none && strcmp(s, "") == 0);

	CuAssertTrue(ct, obj->set(obj, NewIfc_text, new_title) == NewIfc_error_none);

	CuAssertTrue(ct, obj->get(obj, NewIfc_text, &s) == NewIfc_error_none && strcmp(s, new_title) == 0);

	CuAssertTrue(ct, robj->do_render(robj, NULL) == NewIfc_error_none);
	size_t sz = strlen(new_title);
	for (size_t i = 0; i < sz; i++) {
		ciolib_vmem_gettext(i+1,1,i+1,1,&cells);
		CuAssertTrue(ct, cells.ch == new_title[i]);
		CuAssertTrue(ct, cells.fg == obj->fg_colour);
		CuAssertTrue(ct, cells.bg == obj->bg_colour);
		CuAssertTrue(ct, cells.font == obj->font);
	}
	CuAssertTrue(ct, obj->destroy(obj) == NewIfc_error_none);
	CuAssertTrue(ct, robj->destroy(robj) == NewIfc_error_none);
}

#endif
